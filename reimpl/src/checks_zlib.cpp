// Checks of zlib 1.1.3 built from source (lib_zlib.cpp) against the exe's copy, in the game thread.
//
// Every case runs on both sides from the same inputs: the zlib hooks put out (the exe's copy; all its 56
// functions are hooked, so nothing of the source-built one runs) and in again. Called through the exe's
// addresses. Compared per case: every call's return code and the stream's fields after it (avail_in/out,
// total_in/out, adler, data_type, the message text), the bytes written (the whole output buffer, also
// behind what the call reports) and the lengths.
//  - compress2 on generated data (zeros, runs, short periods, text, small alphabets, random bytes, copies
//    from up to 40,000 bytes back, mixed) of 0 to 70,000 bytes at levels -1 and 0-9, with an output
//    buffer of the full bound or too small; uncompress of the result into the exact size, a bigger and a
//    too small buffer.
//  - uncompress of damaged streams: bits flipped, truncated, garbage, a preset dictionary flag, a wrong
//    adler32, a bad window size or method.
//  - deflate as a stream: deflateInit2_ with levels, window bits 9-15 (negative: raw), memory levels 1-9,
//    the three strategies and invalid parameters and versions (not window bits 8, which 1.1.3 accepts but
//    then overruns its window: see test_deflate); input and output in random pieces with
//    the five flush modes (and an invalid one), deflateReset in between, deflateEnd.
//  - inflate as a stream on such streams (also with a smaller window, raw, damaged): pieces, Z_NO_FLUSH,
//    Z_SYNC_FLUSH and Z_FINISH, inflateReset in between, inflateEnd; deflateInit_ and inflateInit_.
//  - adler32 on random buffers and start values and on a null buffer.
//  - Real data: game files read through the game's file layer (a level, its .way, the rules, a dialogue
//    script, a texture, a model, a voice-over) through the game's Pack 0x5c3b40 and Unpack 0x5c3ba0 (the
//    temp file blocks: compress2 at the default level, uncompress), and compress2 at levels 1 and 9.
//  - Each hook alone: all out, one in at a time, a workload through all 56 functions compared with the
//    exe's alone (see test_each_hook).
#include "checks_libs_common.h"

namespace zax {

int hooks_zlib(Hook *out);

namespace {

using namespace libchk;

// The exe's zlib (fastcall, /Gr)
struct ZlibStream {
    BYTE *next_in;
    DWORD avail_in, total_in;
    BYTE *next_out;
    DWORD avail_out, total_out;
    const char *msg;
    void *state;
    void *zalloc, *zfree, *opaque;
    int data_type;
    DWORD adler, reserved;
};
static_assert(sizeof(ZlibStream) == 56);
constexpr DWORD COMPRESS2 = 0x5caa90, UNCOMPRESS = 0x5cab30, DEFLATE_INIT = 0x5cabc0, DEFLATE_INIT2 = 0x5cabe0,
                DEFLATE_RESET = 0x5cade0, DEFLATE = 0x5cae50, DEFLATE_END = 0x5cb1a0, INFLATE_RESET = 0x5cbe80,
                INFLATE_END = 0x5cbec0, INFLATE_INIT2 = 0x5cbf00, INFLATE_INIT = 0x5cc020, INFLATE = 0x5cc040, ADLER32 = 0x5cc4a0;
inline int compress2(BYTE *d, DWORD *dn, const BYTE *s, DWORD n, int level)
{
    return at<int(__fastcall *)(BYTE *, DWORD *, const BYTE *, DWORD, int)>(COMPRESS2)(d, dn, s, n, level);
}
inline int uncompress(BYTE *d, DWORD *dn, const BYTE *s, DWORD n)
{
    return at<int(__fastcall *)(BYTE *, DWORD *, const BYTE *, DWORD)>(UNCOMPRESS)(d, dn, s, n);
}
inline int deflate_init2(ZlibStream *z, int level, int method, int bits, int mem, int strategy, const char *version, int size)
{
    return at<int(__fastcall *)(ZlibStream *, int, int, int, int, int, const char *, int)>(DEFLATE_INIT2)(z, level, method, bits, mem,
                                                                                                        strategy, version, size);
}
inline int deflate_init(ZlibStream *z, int level, const char *version, int size)
{
    return at<int(__fastcall *)(ZlibStream *, int, const char *, int)>(DEFLATE_INIT)(z, level, version, size);
}
inline int inflate_init2(ZlibStream *z, int bits, const char *version, int size)
{
    return at<int(__fastcall *)(ZlibStream *, int, const char *, int)>(INFLATE_INIT2)(z, bits, version, size);
}
inline int inflate_init(ZlibStream *z, const char *version, int size)
{
    return at<int(__fastcall *)(ZlibStream *, const char *, int)>(INFLATE_INIT)(z, version, size);
}
inline int zcall(DWORD f, ZlibStream *z) { return at<int(__fastcall *)(ZlibStream *)>(f)(z); }
inline int zcall(DWORD f, ZlibStream *z, int flush) { return at<int(__fastcall *)(ZlibStream *, int)>(f)(z, flush); }
inline DWORD adler32(DWORD a, const BYTE *p, DWORD n) { return at<DWORD(__fastcall *)(DWORD, const BYTE *, DWORD)>(ADLER32)(a, p, n); }

HookSites g_hooks;   // module and table set by the checks (no constructors in the DLL)

// ---- The record of a side ------------------------------------------------------------------------------------

constexpr int LOG = 48;
struct Side {
    unsigned hash;
    int n;
    DWORD log[LOG][4];
};
Side g_side[2];
int g_s;
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    Side &s = g_side[g_s];
    const DWORD e[4] = {a, b, c, d};
    s.hash = fnv(s.hash, e, sizeof e);
    if (s.n < LOG)
        memcpy(s.log[s.n], e, sizeof e);
    s.n++;
}
DWORD text_hash(const char *s) { return s ? fnv_text(2166136261u, s) : 0; }
void ev_stream(DWORD tag, int r, const ZlibStream &z, const BYTE *out_base)
{
    ev(tag, (DWORD)r, z.avail_in, z.avail_out);
    ev(tag + 1, z.total_in, z.total_out, z.adler);
    ev(tag + 2, (DWORD)z.data_type, text_hash(z.msg), z.state ? 1 : 0);
    ev(tag + 3, (DWORD)(z.next_out - out_base), 0, 0);
}
bool g_each;   // the checks of each hook alone: the hooks stay as test_each_hook sets them
void begin(int side)
{
    g_s = side;
    g_side[side].hash = 2166136261u;
    g_side[side].n = 0;
    if (side == 0 && !g_each)
        g_hooks.out();
}
void end(int side)
{
    if (side == 0 && !g_each)
        g_hooks.in();
}
bool same(Tally &t, const char *what, unsigned seed)
{
    const Side &a = g_side[0], &b = g_side[1];
    const bool ok = a.hash == b.hash && a.n == b.n;
    char d[96];
    wsprintfA(d, "[%s seed %u]", what, seed);
    t.check(ok, "", d);
    if (!ok && t.mismatches <= 3) {
        for (int i = 0; i < a.n && i < b.n && i < LOG; i++)
            if (memcmp(a.log[i], b.log[i], 16)) {
                log("    event %d: %x %x %x %x / %x %x %x %x", i, a.log[i][0], a.log[i][1], a.log[i][2], a.log[i][3], b.log[i][0],
                    b.log[i][1], b.log[i][2], b.log[i][3]);
                break;
            }
        if (a.n != b.n)
            log("    events: %d / %d", a.n, b.n);
    }
    return ok;
}

// ---- Inputs ---------------------------------------------------------------------------------------------------

constexpr DWORD MAX_IN = 70000, MAX_OUT = MAX_IN + MAX_IN / 100 + 64;
BYTE g_in[MAX_IN + 16];
BYTE g_packed[MAX_OUT];
BYTE g_out[2][MAX_OUT];
BYTE g_round[2][MAX_IN + 256];

const char *const WORDS[] = {"the ", "Zax ", "alien ", "hunter ", "Korbo ", "level", "\r\n", "\t", "Position=", "Layer ",
                             "1234", ", ", "{", "}", "CEntityCharacter", " 0.5", "\"", "Name=", "Sequence ", "Kor"};

DWORD gen_size(unsigned r)
{
    switch (r % 8) {
    case 0: return r / 8 % 4;
    case 1:
    case 2: return r / 8 % 300;
    case 3:
    case 4: return r / 8 % 6000;
    case 5: return 16384 + r / 8 % 3;   // around the window and the pending buffer sizes
    default: return r / 8 % MAX_IN;
    }
}
void gen_data(BYTE *p, DWORD n, unsigned kind)
{
    switch (kind % 9) {
    case 0: memset(p, 0, n); break;
    case 1: memset(p, (int)(random() & 0xff), n); break;
    case 2: {
        const DWORD period = 1 + random() % 300;
        for (DWORD i = 0; i < n; i++)
            p[i] = i < period ? (BYTE)random() : p[i - period];
        break;
    }
    case 3:
        for (DWORD i = 0; i < n;) {
            const char *w = WORDS[random() % (sizeof WORDS / sizeof WORDS[0])];
            while (*w && i < n)
                p[i++] = (BYTE)*w++;
        }
        break;
    case 4: {
        const DWORD alphabet = 2 + random() % 15;
        for (DWORD i = 0; i < n; i++)
            p[i] = (BYTE)('a' + random() % alphabet);
        break;
    }
    case 5:
        for (DWORD i = 0; i < n; i++)
            p[i] = (BYTE)(random() >> 13);
        break;
    case 6:   // copies from up to 40,000 bytes back (beyond the 32 KB window too)
        for (DWORD i = 0; i < n;) {
            if (i > 64 && random() % 3) {
                DWORD dist = 1 + random() % (i < 40000 ? i : 40000), len = 3 + random() % 300;
                for (; len-- && i < n; i++)
                    p[i] = p[i - dist];
            } else {
                for (DWORD k = random() % 40; k-- && i < n; i++)
                    p[i] = (BYTE)(random() >> 11);
            }
        }
        break;
    case 7:   // runs
        for (DWORD i = 0; i < n;) {
            const BYTE b = (BYTE)(random() % 4);
            for (DWORD k = 1 + random() % 600; k-- && i < n; i++)
                p[i] = b;
        }
        break;
    default:   // mixed: text, then random, then zeros
        for (DWORD i = 0; i < n; i++)
            p[i] = i < n / 3 ? (BYTE)WORDS[0][i % 4] : i < 2 * n / 3 ? (BYTE)(random() >> 9) : 0;
        break;
    }
}

// ---- compress2 and uncompress -----------------------------------------------------------------------------------

void run_compress(int side, DWORD n, int level, DWORD out_size, unsigned seed)
{
    begin(side);
    memset(g_out[side], 0xa5, MAX_OUT);
    DWORD dn = out_size;
    const int r = compress2(g_out[side], &dn, g_in, n, level);
    ev(1, (DWORD)r, dn, fnv(2166136261u, g_out[side], MAX_OUT));
    if (r == 0) {
        const DWORD sizes[3] = {n, n + 100, n ? n - 1 - seed % n : 0};
        for (int k = 0; k < 3; k++) {
            memset(g_round[side], 0x5a, sizeof g_round[side]);
            DWORD rn = sizes[k];
            const int u = uncompress(g_round[side], &rn, g_out[side], dn);
            ev(2, (DWORD)u, rn, fnv(2166136261u, g_round[side], sizeof g_round[side]));
            if (k == 0 && side == 1 && (u != 0 || rn != n || memcmp(g_round[side], g_in, n)))
                ev(3, 0xbad);   // the source-built round trip itself (both sides log it the same only if equal)
        }
    }
    end(side);
}

void test_compress(Tally &t)
{
    for (int i = 0, cases = scaled(600); i < cases; i++) {
        const unsigned seed = 0x10000u + (unsigned)i;
        g_seed = seed * 2654435761u;
        const DWORD n = gen_size(random());
        gen_data(g_in, n, random());
        const int level = (int)(random() % 11) - 1;
        const DWORD bound = n + n / 1000 + 12 + 8;   // compress2 needs 0.1 % + 12
        DWORD out = bound;
        if (random() % 6 == 0)
            out = random() % (bound > 1 ? bound : 1);   // too small: Z_BUF_ERROR
        if (out > MAX_OUT)
            out = MAX_OUT;
        const unsigned s2 = random();
        for (int side = 0; side < 2; side++)
            run_compress(side, n, level, out, s2);
        same(t, "compress2/uncompress", seed);
    }
}

// A valid stream (zlib header unless raw) of generated data, made through the source-built side
DWORD make_stream(DWORD n, int level, int bits)
{
    ZlibStream z;
    memset(&z, 0, sizeof z);
    if (deflate_init2(&z, level, 8, bits, 8, 0, "1.1.3", sizeof z) != 0)
        return 0;
    z.next_in = g_in;
    z.avail_in = n;
    z.next_out = g_packed;
    z.avail_out = MAX_OUT;
    zcall(DEFLATE, &z, 4);
    zcall(DEFLATE_END, &z);
    return z.total_out;
}

void damage(BYTE *p, DWORD &n, unsigned kind)
{
    switch (kind % 8) {
    case 0:
    case 1:
        for (DWORD k = 1 + random() % 3; k-- && n;)
            p[random() % n] ^= (BYTE)(1 << (random() % 8));
        break;
    case 2: n = n ? random() % n : 0; break;
    case 3:
        for (DWORD i = 2; i < n; i++)
            p[i] = (BYTE)(random() >> 7);
        break;
    case 4:   // a preset dictionary (FDICT) with a valid header check
        if (n >= 2) {
            p[1] |= 0x20;
            p[1] = (BYTE)(p[1] - (p[0] * 256u + p[1]) % 31);
            if ((p[0] * 256u + p[1]) % 31)
                p[1] = (BYTE)(p[1] + 31 - (p[0] * 256u + p[1]) % 31);
        }
        break;
    case 5:
        if (n >= 4)
            p[n - 1 - random() % 4] ^= 0x40;   // adler32
        break;
    case 6:
        if (n >= 1)
            p[0] = (BYTE)((p[0] & 0x0f) | (BYTE)((8 + random() % 8) << 4));   // window size above 15
        break;
    default:
        if (n >= 1)
            p[0] = (BYTE)((p[0] & 0xf0) | (BYTE)(random() % 16));   // method
        break;
    }
}

void test_damaged(Tally &t)
{
    static BYTE src[MAX_OUT];
    for (int i = 0, cases = scaled(400); i < cases; i++) {
        const unsigned seed = 0x20000u + (unsigned)i;
        g_seed = seed * 2654435761u;
        const DWORD n = gen_size(random()) % 20000;
        gen_data(g_in, n, random());
        DWORD m = make_stream(n, (int)(random() % 10), 15);
        memcpy(src, g_packed, m);
        damage(src, m, random());
        const DWORD out = random() % 3 ? n : n + 1000;
        for (int side = 0; side < 2; side++) {
            begin(side);
            memset(g_round[side], 0x5a, sizeof g_round[side]);
            DWORD rn = out;
            const int u = uncompress(g_round[side], &rn, src, m);
            ev(4, (DWORD)u, rn, fnv(2166136261u, g_round[side], sizeof g_round[side]));
            end(side);
        }
        same(t, "uncompress (damaged)", seed);
    }
}

// ---- Streams ------------------------------------------------------------------------------------------------------

const char *version_of(unsigned r) { return r % 40 == 0 ? "2.0" : r % 40 == 1 ? "1.0.4" : "1.1.3"; }
int size_of(unsigned r) { return r % 50 == 0 ? 52 : (int)sizeof(ZlibStream); }

struct DeflateCase {
    DWORD n;
    int level, method, bits, mem, strategy;
    unsigned version, steps_seed;
};
void run_deflate(int side, const DeflateCase &c)
{
    begin(side);
    BYTE *out = g_out[side];
    memset(out, 0xa5, MAX_OUT);
    ZlibStream z;
    memset(&z, 0, sizeof z);
    const int r0 = c.version % 7 == 3 ? deflate_init(&z, c.level, version_of(c.version), size_of(c.version))
                                      : deflate_init2(&z, c.level, c.method, c.bits, c.mem, c.strategy, version_of(c.version),
                                                      size_of(c.version));
    ev(10, (DWORD)r0, z.state ? 1 : 0, text_hash(z.msg));
    if (r0 == 0) {
        g_seed = c.steps_seed;
        z.next_in = g_in;
        z.next_out = out;
        DWORD left_in = c.n;
        bool finishing = false;
        for (int step = 0; step < 300; step++) {
            const DWORD in = random() % 5 ? random() % 5000 : random() % 4;
            const DWORD room = (DWORD)(out + MAX_OUT - z.next_out);
            DWORD o = random() % 4 ? 1 + random() % 3000 : random() % 3;
            if (o > room)
                o = room;
            z.avail_in = in < left_in ? in : left_in;
            z.avail_out = o;
            int flush = 0;
            const unsigned f = random() % 16;
            if (finishing || (z.avail_in == left_in && f < 8))
                flush = 4, finishing = true;
            else if (f < 10)
                flush = 0;
            else if (f < 12)
                flush = 1;
            else if (f < 14)
                flush = 2;
            else if (f == 14)
                flush = 3;
            else
                flush = random() % 3 ? 4 : 5;   // early finish, or an invalid flush
            if (flush == 4)
                finishing = true;
            const DWORD before = z.avail_in;
            const int r = zcall(DEFLATE, &z, flush);
            left_in -= before - z.avail_in;
            ev_stream(20, r, z, out);
            if (r == 1 || (r < 0 && r != -5))
                break;
            if (random() % 60 == 0) {
                const int rr = zcall(DEFLATE_RESET, &z);   // before the fields are read (argument order)
                ev(30, (DWORD)rr, z.total_in, z.total_out);
                z.next_in = g_in;
                left_in = c.n;
                finishing = false;
            }
        }
    }
    const int re = zcall(DEFLATE_END, &z);
    ev(40, (DWORD)re, text_hash(z.msg), fnv(2166136261u, out, MAX_OUT));
    end(side);
}

void test_deflate(Tally &t)
{
    for (int i = 0, cases = scaled(300); i < cases; i++) {
        const unsigned seed = 0x30000u + (unsigned)i;
        g_seed = seed * 2654435761u;
        DeflateCase c;
        c.n = gen_size(random()) % 40000;
        gen_data(g_in, c.n, random());
        const unsigned v = random();
        c.level = v % 30 == 0 ? 10 : (int)(random() % 11) - 1;
        c.method = v % 31 == 1 ? 7 : 8;
        // window bits 9-15: 1.1.3 accepts 8 too but then writes outside its window (AddressSanitizer on a native
        // build of the same source; zlib 1.2.9 uses 9 for 8), so both sides would only compare heap damage
        c.bits = v % 29 == 2 ? (int)(random() % 2 ? 7 : 16) : (int)(9 + random() % 7) * (random() % 4 ? 1 : -1);
        c.mem = v % 27 == 3 ? (int)(random() % 2 ? 0 : 10) : (int)(1 + random() % 9);
        c.strategy = v % 26 == 4 ? 3 : (int)(random() % 3);
        c.version = random();
        c.steps_seed = random();
        for (int side = 0; side < 2; side++)
            run_deflate(side, c);
        same(t, "deflate stream", seed);
    }
}

struct InflateCase {
    DWORD m;
    int bits;
    unsigned version, steps_seed;
    bool plain_init;
};
void run_inflate(int side, const InflateCase &c, const BYTE *src)
{
    begin(side);
    BYTE *out = g_round[side];
    memset(out, 0x5a, sizeof g_round[side]);
    ZlibStream z;
    memset(&z, 0, sizeof z);
    const int r0 = c.plain_init ? inflate_init(&z, version_of(c.version), size_of(c.version))
                                : inflate_init2(&z, c.bits, version_of(c.version), size_of(c.version));
    ev(50, (DWORD)r0, z.state ? 1 : 0, text_hash(z.msg));
    if (r0 == 0) {
        g_seed = c.steps_seed;
        z.next_in = (BYTE *)src;
        z.next_out = out;
        DWORD left_in = c.m;
        int stuck = 0;
        for (int step = 0; step < 400; step++) {
            const DWORD in = random() % 4 ? 1 + random() % 3000 : random() % 3;
            const DWORD room = (DWORD)(out + sizeof g_round[side] - z.next_out);
            DWORD o = random() % 4 ? 1 + random() % 4000 : random() % 3;
            if (o > room)
                o = room;
            z.avail_in = in < left_in ? in : left_in;
            z.avail_out = o;
            const unsigned f = random() % 8;
            const int flush = f < 5 ? 0 : f < 7 ? 2 : 4;
            const DWORD before = z.avail_in;
            const int r = zcall(INFLATE, &z, flush);
            left_in -= before - z.avail_in;
            ev_stream(60, r, z, out);
            if (r == 1 || r == 2 || (r < 0 && r != -5))
                break;
            if (r == -5 && ++stuck > 20)
                break;
            if (random() % 80 == 0) {
                const int rr = zcall(INFLATE_RESET, &z);
                ev(70, (DWORD)rr, z.total_in, z.total_out);
                z.next_in = (BYTE *)src;
                left_in = c.m;
            }
        }
    }
    const int re = zcall(INFLATE_END, &z);
    ev(80, (DWORD)re, text_hash(z.msg), fnv(2166136261u, out, sizeof g_round[side]));
    end(side);
}

void test_inflate(Tally &t)
{
    static BYTE src[MAX_OUT];
    for (int i = 0, cases = scaled(300); i < cases; i++) {
        const unsigned seed = 0x40000u + (unsigned)i;
        g_seed = seed * 2654435761u;
        const DWORD n = gen_size(random()) % 60000;
        gen_data(g_in, n, random());
        const int bits = (int)(9 + random() % 7);   // deflate's window (not 8, see test_deflate); inflate gets 8 below
        const bool raw = random() % 5 == 0;
        DWORD m = make_stream(n, (int)(random() % 10), raw ? -bits : bits);
        memcpy(src, g_packed, m);
        if (random() % 4 == 0)
            damage(src, m, random());
        InflateCase c;
        c.m = m;
        const unsigned w = random() % 6;
        c.bits = (raw ? -1 : 1) * (w == 0 && bits > 8 ? bits - 1 : w == 1 ? 15 : bits);   // a smaller window: an error
        c.version = random();
        c.steps_seed = random();
        c.plain_init = !raw && random() % 5 == 0;
        for (int side = 0; side < 2; side++)
            run_inflate(side, c, src);
        same(t, "inflate stream", seed);
    }
}

void test_adler(Tally &t)
{
    for (int i = 0, cases = scaled(300); i < cases; i++) {
        const unsigned seed = 0x50000u + (unsigned)i;
        g_seed = seed * 2654435761u;
        const DWORD n = gen_size(random());
        gen_data(g_in, n, random());
        const DWORD start = random() % 3 ? 1 : random();
        const bool null = random() % 20 == 0;
        for (int side = 0; side < 2; side++) {
            begin(side);
            ev(90, adler32(start, null ? nullptr : g_in, n));
            end(side);
        }
        same(t, "adler32", seed);
    }
}

// ---- Real data: game files through Pack and Unpack ------------------------------------------------------------------

const char *const FILES[] = {
    "Rules.txt",
    "Levels/ACT 2/M9/Mission09.RDS",
    "Levels/ACT 2/M9/09 Assimilate Cave1.way",
    "Levels/ACT 2/M9/09 Assimilate Cave1.zax",
    "Cache/Textures/15 Brick 1.frm16",
    "Cache/Models/Special Effects/fire 2.mdl16",
    "Levels/ACT 2/M9/VOs/Ask for moon key.ogg",
};

void test_real(Tally &t, int &found)
{
    found = 0;
    for (unsigned f = 0; f < sizeof FILES / sizeof FILES[0]; f++) {
        DWORD n;
        BYTE *data = read_game_file(FILES[f], n);
        if (!data) {
            log("  zlib real data: %s not found", FILES[f]);
            continue;
        }
        found++;
        DWORD packed_n[2] = {}, sum[2] = {};
        for (int side = 0; side < 2; side++) {
            begin(side);
            BYTE *packed = nullptr, *unpacked = nullptr;
            // Pack (thiscall, object unused: out, source, n) -> the packed size; Unpack (out, size, source, n) -> al
            const DWORD pn = at<DWORD(THISCALL *)(void *, BYTE **, const BYTE *, DWORD)>(0x5c3b40)(nullptr, &packed, data, n);
            const BYTE ok = (BYTE)at<DWORD(THISCALL *)(void *, BYTE **, DWORD, const BYTE *, DWORD)>(0x5c3ba0)(nullptr, &unpacked, n, packed, pn);
            ev(100, pn, packed ? fnv(2166136261u, packed, pn) : 0, ok);
            ev(101, unpacked ? fnv(2166136261u, unpacked, n) : 0, unpacked && !memcmp(unpacked, data, n));
            packed_n[side] = pn;
            at<void(__cdecl *)(void *)>(0x5d0320)(packed);
            at<void(__cdecl *)(void *)>(0x5d0320)(unpacked);
            // compress2 at levels 1 and 9 (the first 200,000 bytes)
            const DWORD m = n < 200000 ? n : 200000;
            BYTE *out = (BYTE *)crt_malloc(m + m / 100 + 64);
            for (int level = 1; level <= 9; level += 8) {
                DWORD dn = m + m / 100 + 64;
                const int r = compress2(out, &dn, data, m, level);
                ev(102, (DWORD)r, dn, fnv(2166136261u, out, dn));
                sum[side] += dn;
            }
            crt_free(out);
            end(side);
        }
        char what[96];
        wsprintfA(what, "%.60s %u bytes, packed %u", FILES[f], n, packed_n[1]);
        same(t, what, f);
        crt_free(data);
    }
}


// ---- Each hook alone ----------------------------------------------------------------------------------------------
// The checks above call the library's entry points; inside the source-built side its functions call each other
// directly, so they cannot tell whether an inner function (pqdownheap, huft_build, ...) is the one its exe
// address was named after. Here the hooks are all out (the exe's copy) and one of them in at a time: the exe's
// callers of that address run the source-built function, and the result of a workload that reaches every
// function is compared with the exe's alone.
void workload(int side)
{
    begin(side);
    g_seed = 0x5eed1u;
    static BYTE src[24000], packed[26000], out[24000];
    const DWORD n = sizeof src;
    gen_data(src, n / 3, 3);
    gen_data(src + n / 3, n / 3, 6);
    gen_data(src + 2 * (n / 3), n - 2 * (n / 3), 4);
    for (int level = 0; level <= 9; level += 3) {   // stored, fast, slow
        DWORD dn = sizeof packed;
        const int r = compress2(packed, &dn, src, level == 0 ? 3000 : level == 3 ? 200 : n, level);   // 200 bytes: fixed trees
        ev(200, (DWORD)r, dn, fnv(2166136261u, packed, dn));
        DWORD un = sizeof out;
        const int u = uncompress(out, &un, packed, dn);   // before un is read (argument order)
        ev(201, (DWORD)u, un, fnv(2166136261u, out, un));
    }
    // streams with every flush, small windows and memory levels, a reset; inflate in pieces; errors
    const int params[4][3] = {{9, 2, 0}, {15, 9, 1}, {12, 8, 2}, {-10, 5, 0}};
    for (const auto &pa : params) {
        ZlibStream z;
        memset(&z, 0, sizeof z);
        ev(202, (DWORD)deflate_init2(&z, 6, 8, pa[0], pa[1], pa[2], "1.1.3", sizeof z));
        z.next_in = src;
        z.next_out = packed;
        const int flushes[6] = {0, 1, 2, 3, 0, 4};
        for (int k = 0; k < 6; k++) {
            z.avail_in = k == 5 ? n - z.total_in : 3000;
            z.avail_out = k == 5 ? sizeof packed - z.total_out : 2000;
            ev_stream(203, zcall(DEFLATE, &z, flushes[k]), z, packed);
        }
        const DWORD m = z.total_out;
        ev(204, (DWORD)zcall(DEFLATE_RESET, &z));
        ev(205, (DWORD)zcall(DEFLATE_END, &z));
        memset(&z, 0, sizeof z);
        ev(206, (DWORD)(pa[0] == 15 ? inflate_init(&z, "1.1.3", sizeof z) : inflate_init2(&z, pa[0], "1.1.3", sizeof z)));
        z.next_in = packed;
        z.next_out = out;
        for (int k = 0; k < 40 && z.total_in < m; k++) {
            z.avail_in = 700 < m - z.total_in ? 700 : m - z.total_in;
            z.avail_out = 900;
            const int r = zcall(INFLATE, &z, k % 3 == 2 ? 2 : 0);
            ev_stream(207, r, z, out);
            if (r == 1 || (r < 0 && r != -5))
                break;
        }
        ev(208, (DWORD)zcall(INFLATE_RESET, &z));
        ev(209, (DWORD)zcall(INFLATE_END, &z));
    }
    static BYTE bad[26000];
    for (unsigned kind = 0; kind < 8; kind++) {
        DWORD dn = sizeof packed;
        compress2(packed, &dn, src, 5000, 6);
        memcpy(bad, packed, dn);
        g_seed = 0x5eed2u + kind;
        damage(bad, dn, kind);
        DWORD un = sizeof out;
        const int u = uncompress(out, &un, bad, dn);
        ev(210, (DWORD)u, un, fnv(2166136261u, out, un));
    }
    ev(211, adler32(1, src, n), adler32(7, nullptr, 9));
    end(side);
}
void test_each_hook(Tally &t, int &hooks)
{
    g_hooks.out();
    g_each = true;
    workload(0);
    hooks = g_hooks.n;
    for (int k = 0; k < g_hooks.n; k++) {
        g_hooks.one_in(k);
        workload(1);
        g_hooks.one_out(k);
        same(t, g_hooks.name(k), (unsigned)k);
    }
    g_each = false;
    g_hooks.in();
}

}  // namespace

ZAX_CHECKS(zlib)
{
    (void)cls;
    (void)n;
    g_hooks.module = "zlib";
    g_hooks.table = hooks_zlib;
    Tally compress{"zlib: compress2 and uncompress on generated data, levels -1 to 9, small buffers"},
        damaged{"zlib: uncompress of damaged streams"}, deflate{"zlib: deflate as a stream (parameters, pieces, flushes, reset)"},
        inflate{"zlib: inflate as a stream (pieces, flushes, small windows, raw, damaged, reset)"}, adler{"zlib: adler32"},
        real{"zlib: game files through Pack and Unpack and compress2 at levels 1 and 9 (real data)"};
    test_compress(compress);
    compress.report();
    test_damaged(damaged);
    damaged.report();
    test_deflate(deflate);
    deflate.report();
    test_inflate(inflate);
    inflate.report();
    test_adler(adler);
    adler.report();
    int found = 0;
    test_real(real, found);
    real.report();
    log("  zlib real data: %d of %d files", found, (int)(sizeof FILES / sizeof FILES[0]));
    Tally each{"zlib: each hook alone among the exe's functions (a workload through all of them)"};
    int hooks = 0;
    test_each_hook(each, hooks);
    each.report();
    log("  zlib each hook alone: %d hooks", hooks);
}

}  // namespace zax
