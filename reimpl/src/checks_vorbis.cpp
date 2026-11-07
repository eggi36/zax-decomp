// Checks of libogg, libvorbis and vorbisfile 1.0 beta 4 built from source (lib_vorbis.cpp) against the exe's
// copies, in the game thread.
//
// Every case runs on both sides from the same inputs: the Vorbis hooks put out (the exe's copies; all 181
// functions are hooked) and in again, called through the exe's addresses. Meanwhile the game's sound
// streams are paused (checks_libs_common.h): the music decodes in the multimedia timer's thread.
//  - Real data, the game's .ogg files read through its file layer (voice-overs and quips of 22,050 Hz mono,
//    music of 22,050 and 44,100 Hz stereo), with the callbacks reading from memory and recording every call:
//    ov_open_callbacks, ov_info, ov_pcm_total and ov_time_total (also for link 0 and a missing link), the
//    whole file through ov_read in pieces of random length (16-bit signed little endian as the game reads,
//    and 8 bit, unsigned and big endian), ov_clear. The music: the first second, then ov_time_seek (also
//    before the start and behind the end), ov_pcm_seek, ov_pcm_seek_page and ov_raw_seek with a short read
//    and ov_time_tell after each. Also a damaged file (bits flipped, cut, garbage), a stream that cannot
//    seek (_open_nonseekable), the first bytes given as "initial", and one file through the game's own
//    callbacks (0x525380 Read, 0x5253a0 Seek, 0x525400 Close, 0x57d310 Tell on a CFileBuf, as OggStream_Open
//    0x525aa0 passes them).
//  - The parts the decoder does not reach (the encoder's, linked because the backends' export bundles point
//    to them; the game never encodes), each on the same inputs on both sides: smallft, the MDCT, the LPC,
//    vorbis_lpc_to_lsp and vorbis_lsp_to_curve, the bit packer, the codebooks' encoder side on a game file's
//    books, the psychoacoustics (with the release's _psy_set_A0), the envelope's and IIR filters' clear, the
//    backends' pack/copy_info/free_info, lsp.c's cdecl comparator and the functions the linker folded.
//  - Each hook alone: all out, one in at a time, a workload through the decoder, the seeks and the encoder's
//    parts compared with the exe's alone (the source-built functions call each other directly, so only this
//    tells whether an inner function is the one its address was named after).
// Compared exactly: every return, size, total, callback call, seek result, integer table and packed bit. The
// floats and the PCM bit for bit as well, but judged with tolerances: MSVC 6 and clang keep different float
// intermediates in 80-bit registers, so the last bit differs here and there and a sample now and then by one
// (docs/DECOMPILATION.md 10, "zlib and Ogg Vorbis built from their original sources"); the logs count the
// bit-identical cases. vorbisfile's host_is_big_endian is folded with the game's 0x46b760: both called.
#include "checks_libs_common.h"

extern "C" int __fastcall host_is_big_endian(void);   // vorbisfile.c, built from source (not hooked: 0x46b760 is game code)

namespace zax {

int hooks_vorbis(Hook *out);

namespace {

using namespace libchk;

// ---- The exe's functions (fastcall, /Gr) ------------------------------------------------------------------------

struct OvCallbacks {
    void *read, *seek, *close, *tell;
};
constexpr DWORD OV_CLEAR = 0x5c3bf0, OV_OPEN = 0x5c3ce0, OV_PCM_TOTAL = 0x5c4590, OV_TIME_TOTAL = 0x5c45f0, OV_RAW_SEEK = 0x5c4670,
                OV_PCM_SEEK_PAGE = 0x5c4a40, OV_PCM_SEEK = 0x5c4be0, OV_TIME_SEEK = 0x5c4ca0, OV_TIME_TELL = 0x5c4d90, OV_INFO = 0x5c4e40,
                OV_READ = 0x5c4e80;
constexpr DWORD VF_SIZE = 0x2c0;   // OggVorbis_File with the exe's 4-byte packing
int ov_open(void *ds, BYTE *vf, const char *initial, long n, OvCallbacks cb)
{
    return at<int(__fastcall *)(void *, BYTE *, const char *, long, OvCallbacks)>(OV_OPEN)(ds, vf, initial, n, cb);
}
int ov_clear(BYTE *vf) { return at<int(__fastcall *)(BYTE *)>(OV_CLEAR)(vf); }
BYTE *ov_info(BYTE *vf, int link) { return at<BYTE *(__fastcall *)(BYTE *, int)>(OV_INFO)(vf, link); }
long long ov_pcm_total(BYTE *vf, int link) { return at<long long(__fastcall *)(BYTE *, int)>(OV_PCM_TOTAL)(vf, link); }
double ov_time_total(BYTE *vf, int link) { return at<double(__fastcall *)(BYTE *, int)>(OV_TIME_TOTAL)(vf, link); }
int ov_raw_seek(BYTE *vf, long pos) { return at<int(__fastcall *)(BYTE *, long)>(OV_RAW_SEEK)(vf, pos); }
int ov_pcm_seek(BYTE *vf, long long pos) { return at<int(__fastcall *)(BYTE *, long long)>(OV_PCM_SEEK)(vf, pos); }
int ov_pcm_seek_page(BYTE *vf, long long pos) { return at<int(__fastcall *)(BYTE *, long long)>(OV_PCM_SEEK_PAGE)(vf, pos); }
int ov_time_seek(BYTE *vf, double s) { return at<int(__fastcall *)(BYTE *, double)>(OV_TIME_SEEK)(vf, s); }
double ov_time_tell(BYTE *vf) { return at<double(__fastcall *)(BYTE *)>(OV_TIME_TELL)(vf); }
long ov_read(BYTE *vf, BYTE *buf, int n, int big, int word, int sgned, int *bitstream)
{
    return at<long(__fastcall *)(BYTE *, BYTE *, int, int, int, int, int *)>(OV_READ)(vf, buf, n, big, word, sgned, bitstream);
}

HookSites g_hooks;   // module and table set by the checks (no constructors in the DLL)
PausedStreams g_streams;

// ---- The record of a side -------------------------------------------------------------------------------------

constexpr int LOG = 4096;   // every event of a case kept for the report of the first difference
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
void ev_double(DWORD tag, double v)
{
    DWORD d[2];
    memcpy(d, &v, 8);
    ev(tag, d[0], d[1]);
}
// Float results go into a buffer per side, compared after both sides: bit for bit, and if not, by their
// largest difference relative to the largest value of the case (x87 register use differs between MSVC 6 and
// clang: where one keeps a float intermediate in a register and the other stores it, the rounding differs).
constexpr DWORD FLOATS_MAX = 40000;
float g_fo[2][FLOATS_MAX];
DWORD g_fo_n[2];
constexpr int SEGMENTS_MAX = 64;
DWORD g_seg[2][SEGMENTS_MAX + 1];   // start of every ev_floats call's values (the scale is per call)
// How a call's values are compared beyond bit equality:
//  SCALE: the difference relative to the largest value of the call (transforms, filters, tables);
//  EXPONENT: relative to the value itself, per unit of its natural logarithm (curves made by exp(): an error
//    of the argument grows with it; vorbis_lsp_to_curve's fromdB(amp / sqrt(p + q) - ampoffset) after a
//    product of up to 32 rounded factors);
//  DECIBEL: absolute in dB (a mask in dB whose float positions are cut to array indices: a value a few ulp
//    from a rounding boundary moves a step of the curve over a range of lines, _vp_compute_mask); besides,
//    at most DECIBEL_LINES single lines of a call may differ by more (a seed put on the neighbouring line).
enum Compare { SCALE, EXPONENT, DECIBEL };
BYTE g_seg_mode[2][SEGMENTS_MAX];
int g_nseg[2];
void ev_floats(DWORD tag, const float *p, DWORD n, Compare mode = SCALE)
{
    ev(tag, n);
    if (g_nseg[g_s] < SEGMENTS_MAX) {
        g_seg_mode[g_s][g_nseg[g_s]] = (BYTE)mode;
        g_seg[g_s][g_nseg[g_s]++] = g_fo_n[g_s];
    }
    for (DWORD i = 0; i < n && g_fo_n[g_s] < FLOATS_MAX; i++)
        g_fo[g_s][g_fo_n[g_s]++] = p[i];
}
void ev_float(DWORD tag, float v) { ev_floats(tag, &v, 1); }
// ov_time_tell divides by vf->vi[link].rate with link -1 when the stream cannot seek or pcm_offset is below the
// start of the first link (-1 after a failed seek): it reads the dword 24 bytes before the vi array, outside
// the block, so its result depends on the heap (beta 4's vorbisfile.c; the original gave tiny values, the
// source-built one +-inf in the first runs). Recorded only where it is defined; the game's streams seek.
void ev_time_tell(DWORD tag, BYTE *vf)
{
    const long long pcm_offset = *reinterpret_cast<const long long *>(vf + 0x50);
    if (*reinterpret_cast<const int *>(vf + 4) && pcm_offset >= 0)
        ev_double(tag, ov_time_tell(vf));
    else
        ev(tag, 0xdead, (DWORD)pcm_offset);
}

// The PCM of a case per side, for the report of a difference
constexpr DWORD PCM_MAX = 4u << 20;
BYTE *g_pcm[2];
DWORD g_pcm_n[2];
int g_word, g_sgned, g_big;

bool g_each;   // the checks of each hook alone: the hooks stay as test_each_hook sets them
void begin(int side)
{
    g_s = side;
    g_side[side].hash = 2166136261u;
    g_side[side].n = 0;
    g_pcm_n[side] = 0;
    g_fo_n[side] = 0;
    g_nseg[side] = 0;
    if (side == 0 && !g_each)
        g_hooks.out();
}
void end(int side)
{
    if (side == 0 && !g_each)
        g_hooks.in();
}
// What a tally saw beyond the exact comparison
struct Near {
    int cases, exact;          // cases with floats or PCM; of them bit-identical
    DWORD pcm_samples, pcm_differ, pcm_max;
    DWORD lines_moved;         // DECIBEL lines beyond the tolerance (at most DECIBEL_LINES per call)
    double rel_max;            // largest float difference as a fraction of FLOAT_TOLERANCE (see compare_floats)
    void report(const char *what)
    {
        if (!cases)
            return;
        char rel[32];
        const double pct = rel_max / 1e-5 * 100;   // FLOAT_TOLERANCE
        if (pct < 1e6)
            wsprintfA(rel, "%d.%02d %%", (int)pct, (int)((pct - (int)pct) * 100));   // no printf for floats
        else
            lstrcpyA(rel, pct >= 1e6 ? "far above" : "NaN");
        log("  %s: %d of %d cases bit-identical; PCM samples differing %u of %u (largest %u); largest float difference %s "
            "of its tolerance; single mask lines moved %u", what, exact, cases, pcm_differ, pcm_samples, pcm_max, rel, lines_moved);
    }
};
constexpr double FLOAT_TOLERANCE = 1e-5;     // SCALE: relative to the largest value of the same call's results
constexpr double EXPONENT_TOLERANCE = 1e-4;  // EXPONENT: relative, per unit of the natural logarithm
constexpr double DECIBEL_TOLERANCE = 3.0;    // DECIBEL: dB
constexpr DWORD DECIBEL_LINES = 2;           // DECIBEL: lines beyond it
double x87_ln(double x)
{
    long double v = x;
    __asm__("fldln2\n\tfxch %%st(1)\n\tfyl2x" : "+t"(v));
    return (double)v;
}
constexpr DWORD PCM_TOLERANCE = 1;          // raw units (one LSB)

// PCM of both sides: counts, the first difference
bool compare_pcm(Near &nr, bool report)
{
    if (g_pcm_n[0] != g_pcm_n[1])
        return false;
    const DWORD n = g_pcm_n[0], w = g_word == 1 ? 1 : 2;
    DWORD first = n, differ = 0, maxdiff = 0;
    for (DWORD i = 0; i + w <= n; i += w) {
        if (!memcmp(g_pcm[0] + i, g_pcm[1] + i, w))
            continue;
        if (first == n)
            first = i;
        differ++;
        int a, b;
        if (w == 1) {
            a = g_pcm[0][i];
            b = g_pcm[1][i];
        } else {
            a = (short)(g_big ? g_pcm[0][i] * 256 + g_pcm[0][i + 1] : g_pcm[0][i + 1] * 256 + g_pcm[0][i]);
            b = (short)(g_big ? g_pcm[1][i] * 256 + g_pcm[1][i + 1] : g_pcm[1][i + 1] * 256 + g_pcm[1][i]);
        }
        const DWORD d = (DWORD)(a > b ? a - b : b - a);
        if (d > maxdiff)
            maxdiff = d;
    }
    nr.pcm_samples += n / w;
    nr.pcm_differ += differ;
    if (maxdiff > nr.pcm_max)
        nr.pcm_max = maxdiff;
    if (report && differ)
        log("    PCM: %u bytes, %u samples differ (of %u bytes), the first at byte %u, by at most %u", n, differ, w, first, maxdiff);
    return maxdiff <= PCM_TOLERANCE;
}
bool compare_floats(Near &nr, bool report)
{
    if (g_fo_n[0] != g_fo_n[1] || g_nseg[0] != g_nseg[1])
        return false;
    double worst_rel = 0;
    DWORD differ = 0, first = 0;
    int worst_seg = -1;
    DWORD worst_index = 0;
    g_seg[0][g_nseg[0]] = g_fo_n[0];
    for (int k = 0; k < g_nseg[0]; k++) {
        const int mode = g_seg_mode[0][k];
        double scale = 0, worst = 0;
        DWORD seg_differ = 0, worst_at = 0, lines = 0;
        for (DWORD i = g_seg[0][k]; i < g_seg[0][k + 1]; i++) {
            const double a = g_fo[0][i] < 0 ? -g_fo[0][i] : g_fo[0][i];
            if (a > scale && a < 1e30)
                scale = a;
        }
        for (DWORD i = g_seg[0][k]; i < g_seg[0][k + 1]; i++) {
            if (bits(g_fo[0][i]) == bits(g_fo[1][i]))
                continue;
            if (!differ++)
                first = i;
            seg_differ++;
            double d = (double)g_fo[0][i] - (double)g_fo[1][i];
            d = d < 0 ? -d : d;
            if (mode == EXPONENT) {
                const double a = g_fo[0][i] < 0 ? -g_fo[0][i] : g_fo[0][i], b = g_fo[1][i] < 0 ? -g_fo[1][i] : g_fo[1][i];
                if (a >= 1e30 && b >= 1e30)
                    continue;   // both at the end of the float range (one may have reached infinity)
                const double m = a > b ? a : b, e = m > 0 ? x87_ln(m) : 0;
                d /= m * (e < -1 ? -e : e > 1 ? e : 1);
            }
            if (mode == DECIBEL && d > DECIBEL_TOLERANCE && d < 200 && ++lines <= DECIBEL_LINES) {
                nr.lines_moved++;
                continue;
            }
            if (!(d <= worst)) {
                worst = d;   // NaN on one side counts as the worst
                worst_at = i;
            }
        }
        // all three as a fraction of their tolerance times FLOAT_TOLERANCE, so one maximum serves
        double rel = mode == EXPONENT ? worst / EXPONENT_TOLERANCE * FLOAT_TOLERANCE
                     : mode == DECIBEL ? worst / DECIBEL_TOLERANCE * FLOAT_TOLERANCE
                                       : scale > 0 ? worst / scale : worst;
        if (!(rel <= worst_rel)) {
            worst_rel = rel;
            worst_seg = k;
            worst_index = worst_at;
        }
        (void)seg_differ;
    }
    if (!(worst_rel <= nr.rel_max))
        nr.rel_max = worst_rel;
    if (report && differ) {
        log("    floats: %u of %u differ, the first at %u (%08x / %08x)", differ, g_fo_n[0], first, bits(g_fo[0][first]),
            bits(g_fo[1][first]));
        if (worst_seg >= 0 && worst_rel > FLOAT_TOLERANCE)
            log("    the largest difference: call %d (values %u-%u, compared by %d) at %u: %08x / %08x", worst_seg, g_seg[0][worst_seg],
                g_seg[0][worst_seg + 1], g_seg_mode[0][worst_seg], worst_index, bits(g_fo[0][worst_index]), bits(g_fo[1][worst_index]));
    }
    return worst_rel <= FLOAT_TOLERANCE;
}
bool same(Tally &t, Near &nr, const char *what, unsigned seed)
{
    const Side &a = g_side[0], &b = g_side[1];
    const bool exact_log = a.hash == b.hash && a.n == b.n;
    const bool any = g_pcm_n[0] || g_pcm_n[1] || g_fo_n[0] || g_fo_n[1];
    const bool bit_identical = exact_log && g_pcm_n[0] == g_pcm_n[1] && !memcmp(g_pcm[0], g_pcm[1], g_pcm_n[0]) &&
                               g_fo_n[0] == g_fo_n[1] && !memcmp(g_fo[0], g_fo[1], 4 * g_fo_n[0]);
    if (any) {
        nr.cases++;
        nr.exact += bit_identical;
    }
    const bool report = t.mismatches < 4;
    const bool pcm_ok = compare_pcm(nr, !exact_log && report), floats_ok = compare_floats(nr, !exact_log && report);
    const bool ok = exact_log && pcm_ok && floats_ok;
    char d[128];
    wsprintfA(d, "[%s seed %u]", what, seed);
    t.check(ok, "", d);
    if (!ok && report) {
        for (int i = 0; i < a.n && i < b.n && i < LOG; i++)
            if (memcmp(a.log[i], b.log[i], 16)) {
                log("    event %d: %x %x %x %x / %x %x %x %x", i, a.log[i][0], a.log[i][1], a.log[i][2], a.log[i][3], b.log[i][0],
                    b.log[i][1], b.log[i][2], b.log[i][3]);
                break;
            }
        if (a.n != b.n)
            log("    events: %d / %d", a.n, b.n);
        Near again = nr;   // the report only, not counted twice
        compare_pcm(again, true);
        compare_floats(again, true);
    }
    return ok;
}

// ---- The data source: a file in memory, every callback call recorded ------------------------------------------

struct MemSource {
    const BYTE *data;
    DWORD size, pos;
    bool no_seek;
};
size_t __fastcall mem_read(void *p, size_t size, size_t count, void *ds)
{
    MemSource *m = static_cast<MemSource *>(ds);
    DWORD n = size * count, left = m->size - m->pos;
    if (n > left)
        n = left;
    memcpy(p, m->data + m->pos, n);
    m->pos += n;
    ev(0x100, size, count, n);
    return size ? n / size : 0;
}
int __fastcall mem_seek(void *ds, long long offset, int whence)
{
    MemSource *m = static_cast<MemSource *>(ds);
    ev(0x101, (DWORD)offset, (DWORD)(offset >> 32), (DWORD)whence);
    if (m->no_seek)
        return -1;
    const long long base = whence == 0 ? 0 : whence == 1 ? (long long)m->pos : (long long)m->size;
    const long long to = base + offset;
    if (to < 0 || to > (long long)m->size)
        return -1;
    m->pos = (DWORD)to;
    return 0;
}
int __fastcall mem_close(void *) { ev(0x102); return 0; }
long __fastcall mem_tell(void *ds)
{
    ev(0x103, static_cast<MemSource *>(ds)->pos);
    return (long)static_cast<MemSource *>(ds)->pos;
}

// ---- Decoding ------------------------------------------------------------------------------------------------------

alignas(8) BYTE g_vf[2][VF_SIZE];
BYTE g_buf[9000];

// ov_read until the end, the limit or too many errors; the PCM into the side's buffer
void read_pcm(BYTE *vf, DWORD limit)
{
    DWORD got = 0;
    int errors = 0;
    for (int calls = 0; calls < 4000 && got < limit; calls++) {
        const unsigned r = random();
        int n = r % 3 == 0 ? 4096 : r % 3 == 1 ? (int)(1 + random() % 8192) : (int)(random() % 64);
        int bitstream = -7;
        memset(g_buf, 0xcc, sizeof g_buf);
        const long got1 = ov_read(vf, g_buf, n, g_big, g_word, g_sgned, &bitstream);
        ev(0x200, (DWORD)got1, (DWORD)bitstream, got1 > 0 ? fnv(2166136261u, g_buf + got1, 16) : 0);   // the bytes behind
        if (got1 > 0) {
            if (g_pcm_n[g_s] + (DWORD)got1 <= PCM_MAX) {
                memcpy(g_pcm[g_s] + g_pcm_n[g_s], g_buf, (DWORD)got1);
                g_pcm_n[g_s] += (DWORD)got1;
            }
            got += (DWORD)got1;
        } else if (got1 == 0) {
            break;
        } else if (++errors > 40) {
            break;
        }
    }
}

void ev_info(BYTE *vf)
{
    const int links = *reinterpret_cast<int *>(vf + 0x34);
    ev(0x10, (DWORD)links, *reinterpret_cast<DWORD *>(vf + 4));
    for (int link = -1; link <= links; link++) {
        const BYTE *vi = ov_info(vf, link);
        if (vi)
            ev(0x11, *reinterpret_cast<const DWORD *>(vi) * 256 + *reinterpret_cast<const DWORD *>(vi + 4),
               *reinterpret_cast<const DWORD *>(vi + 8), fnv(2166136261u, vi + 12, 16));
        else
            ev(0x11, 0xffffffff);
        const long long t = ov_pcm_total(vf, link);
        ev(0x12, (DWORD)t, (DWORD)(t >> 32));
        ev_double(0x13, ov_time_total(vf, link));
    }
}

enum Mode { FULL, MUSIC, DAMAGED, NO_SEEK, INITIAL, GAME_CALLBACKS };
struct OggCase {
    const char *path;
    Mode mode;
    int word, sgned, big;
};
const OggCase CASES[] = {
    {"Levels/ACT 2/M9/VOs/1 switch left.ogg", FULL, 2, 1, 0},
    {"Levels/ACT 2/M9/VOs/2switches left.ogg", FULL, 1, 1, 0},
    {"Levels/ACT 2/M9/VOs/Ask for moon key.ogg", FULL, 2, 0, 0},
    {"Levels/ACT 2/M9/VOs/Butu Demon Pits.ogg", FULL, 2, 1, 1},
    {"Resources/Quips/About to fight/Bring it on.ogg", FULL, 1, 0, 0},
    {"Resources/Quips/About to fight/Bring it on3.ogg", FULL, 2, 1, 0},
    {"Levels/ACT 2/M9/VOs/3water switches.ogg", DAMAGED, 2, 1, 0},
    {"Resources/Quips/About to fight/Bring it on2.ogg", NO_SEEK, 2, 1, 0},
    {"Levels/ACT 2/M9/VOs/Butu will stay.ogg", INITIAL, 2, 1, 0},
    {"Levels/ACT 2/M9/VOs/1 switch left.ogg", GAME_CALLBACKS, 2, 1, 0},
    {"Music/Jungle/01- Jungle 01.ogg", MUSIC, 2, 1, 0},
    {"Music/Main Menu/Main Menu.ogg", MUSIC, 2, 1, 0},
    {"Music/Caves/Caves 01.ogg", MUSIC, 2, 1, 0},
};

// The seeks of a music case: ov_time_seek, ov_pcm_seek, ov_pcm_seek_page, ov_raw_seek, each with a short read
void seeks(BYTE *vf, DWORD size, DWORD bytes_per_second)
{
    const long long total = ov_pcm_total(vf, -1);
    const double seconds = ov_time_total(vf, -1);
    for (int k = 0; k < 9; k++) {
        int r;
        const unsigned v = random();
        switch (k) {
        case 0: r = ov_time_seek(vf, seconds * (double)(v % 1000) / 1000.0); break;
        case 1: r = ov_time_seek(vf, k == 1 && v % 2 ? -1.0 : seconds + 3.0); break;   // before the start, behind the end
        case 2: r = ov_time_seek(vf, seconds - 0.25); break;
        case 3: r = ov_pcm_seek(vf, total ? (long long)(v % (DWORD)total) : 0); break;
        case 4: r = ov_pcm_seek(vf, total + 1000); break;
        case 5: r = ov_pcm_seek_page(vf, total ? (long long)(v % (DWORD)total) : 0); break;
        case 6: r = ov_raw_seek(vf, (long)(v % size)); break;
        case 7: r = ov_raw_seek(vf, (long)size + 100); break;
        default: r = ov_time_seek(vf, 0.0); break;
        }
        ev(0x300 + (DWORD)k, (DWORD)r);
        ev_time_tell(0x310, vf);
        read_pcm(vf, bytes_per_second / 5);
        ev_time_tell(0x311, vf);
    }
}

// The game's callbacks need a stream object whose +8 is the CFileBuf (OggCallback_Read 0x525380)
alignas(4) BYTE g_filebuf[2][0x20];
DWORD g_game_source[2][4];

void run_case(int side, const OggCase &c, const BYTE *data, DWORD size, unsigned seed)
{
    begin(side);
    g_seed = seed;
    BYTE *vf = g_vf[side];
    memset(vf, 0, VF_SIZE);
    MemSource src = {data, size, 0, c.mode == NO_SEEK};
    OvCallbacks cb = {(void *)&mem_read, (void *)&mem_seek, (void *)&mem_close, (void *)&mem_tell};
    void *ds = &src;
    const char *initial = nullptr;
    long ibytes = 0;
    bool game = false;
    if (c.mode == INITIAL) {
        initial = (const char *)data;
        ibytes = 4000;
        src.pos = 4000;
    } else if (c.mode == GAME_CALLBACKS) {
        BYTE *fb = g_filebuf[side];
        at<void *(THISCALL *)(void *)>(0x4a1780)(fb);   // CFileBuf::CFileBuf
        game = (BYTE)at<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x4a1840)(fb, c.path, 4) != 0;   // FileBuf_Open
        ev(0x20, game);
        g_game_source[side][2] = (DWORD)(uintptr_t)fb;
        ds = g_game_source[side];
        cb = {at<void *>(0x525380), at<void *>(0x5253a0), at<void *>(0x525400), at<void *>(0x57d310)};
    }
    const int r = c.mode == GAME_CALLBACKS && !game ? -999 : ov_open(ds, vf, initial, ibytes, cb);
    ev(1, (DWORD)r);
    if (r == 0) {
        ev_info(vf);
        const BYTE *vi = ov_info(vf, -1);
        const DWORD bps = vi ? *reinterpret_cast<const DWORD *>(vi + 4) * *reinterpret_cast<const DWORD *>(vi + 8) * 2 : 0;
        if (c.mode == MUSIC) {
            read_pcm(vf, bps);
            seeks(vf, size, bps);
        } else {
            read_pcm(vf, c.mode == INITIAL ? bps * 3 : PCM_MAX);
        }
        ev_time_tell(0x30, vf);
        ev(2, (DWORD)ov_clear(vf));
    }
    if (c.mode == GAME_CALLBACKS)
        at<void(THISCALL *)(void *)>(0x4a1800)(g_filebuf[side]);   // CFileBuf::~CFileBuf (ov_clear closed it already)
    end(side);
}

void damage(BYTE *p, DWORD &n)
{
    for (int k = 0; k < 4; k++)
        p[n / 8 + random() % (n - n / 8)] ^= (BYTE)(1 << (random() % 8));
    for (DWORD i = n * 3 / 4; i < n * 3 / 4 + 600 && i < n; i++)
        p[i] = (BYTE)random();
    n = n * 9 / 10;
}

void test_files(Tally &t, Near &nr, int &found)
{
    found = 0;
    for (unsigned i = 0; i < sizeof CASES / sizeof CASES[0]; i++) {
        const OggCase &c = CASES[i];
        DWORD size;
        BYTE *data = read_game_file(c.path, size);
        if (!data) {
            log("  vorbis real data: %s not found", c.path);
            continue;
        }
        found++;
        const unsigned seed = 0x60000u + i;
        if (c.mode == DAMAGED) {
            g_seed = seed * 2654435761u;
            damage(data, size);
        }
        g_word = c.word;
        g_sgned = c.sgned;
        g_big = c.big;
        for (int side = 0; side < 2; side++)
            run_case(side, c, data, size, seed);
        char what[128];
        wsprintfA(what, "%.80s (%u PCM bytes)", c.path, g_pcm_n[1]);
        same(t, nr, what, seed);
        crt_free(data);
    }
    g_pcm_n[0] = g_pcm_n[1] = 0;
}

// ---- The encoder's and the other parts the decoder does not reach -------------------------------------------------

float g_f[2][8200], g_g[2][8200], g_in[8200];
float rnd_float(float scale) { return ((float)(int)(random() >> 8) / 8388608.0f - 1.0f) * scale; }

constexpr DWORD DRFT_FORWARD = 0x5e6470, DRFT_BACKWARD = 0x5e7830, DRFT_INIT = 0x5e8e50, DRFT_CLEAR = 0x5e9040;
// Each function on both sides from the same inputs: the transforms on a lookup built through the source-built
// side before the case (the tables of drft_init are compared on their own), backward on forward's output of
// that side, so a difference in the last bits of one step does not grow through the next.
void test_smallft(Tally &t, Near &nr)
{
    static float fwd[8200];
    for (int i = 0, cases = scaled(200); i < cases; i++) {
        const unsigned seed = 0x70000u + (unsigned)i;
        g_seed = seed * 2654435761u;
        // the sizes of the codec (products of 4, 2, 3, 5: dradf4/2, dradb3) and some with other factors (dradfg)
        int n = 1;
        const unsigned v = random();
        for (int k = 0, f = (int)(1 + random() % 7); k < f; k++)
            n *= (int)(2 + random() % 4);
        if (v % 4 == 0)
            n *= (int)(7 + 2 * (random() % 4));   // 7, 9, 11, 13
        if (n > 4096)
            n = 4096;
        for (int k = 0; k < n; k++)
            g_in[k] = rnd_float(1.0f);
        DWORD shared[3] = {};
        at<void(__fastcall *)(DWORD *, int)>(DRFT_INIT)(shared, n);
        memcpy(fwd, g_in, 4 * (DWORD)n);
        at<void(__fastcall *)(DWORD *, float *)>(DRFT_FORWARD)(shared, fwd);
        for (int side = 0; side < 2; side++) {
            begin(side);
            DWORD l[3] = {};
            at<void(__fastcall *)(DWORD *, int)>(DRFT_INIT)(l, n);
            ev(0x400, l[0], l[2] ? fnv(2166136261u, (void *)(uintptr_t)l[2], 32 * 4) : 0);
            if (l[1])
                ev_floats(0x400, (const float *)(uintptr_t)l[1], 3 * (DWORD)n);
            at<void(__fastcall *)(DWORD *)>(DRFT_CLEAR)(l);
            ev(0x403, l[1], l[2]);
            memcpy(g_f[side], g_in, 4 * (DWORD)n);
            at<void(__fastcall *)(DWORD *, float *)>(DRFT_FORWARD)(shared, g_f[side]);
            ev_floats(0x401, g_f[side], (DWORD)n);
            memcpy(g_f[side], fwd, 4 * (DWORD)n);
            at<void(__fastcall *)(DWORD *, float *)>(DRFT_BACKWARD)(shared, g_f[side]);
            ev_floats(0x402, g_f[side], (DWORD)n);
            end(side);
        }
        at<void(__fastcall *)(DWORD *)>(DRFT_CLEAR)(shared);
        same(t, nr, "smallft", seed);
    }
}

constexpr DWORD MDCT_INIT = 0x5e30b0, MDCT_CLEAR = 0x5e3290, MDCT_BACKWARD = 0x5e32d0, MDCT_FORWARD = 0x5e3e00;
void test_mdct(Tally &t, Near &nr)
{
    for (int i = 0, cases = scaled(60); i < cases; i++) {
        const unsigned seed = 0x71000u + (unsigned)i;
        g_seed = seed * 2654435761u;
        const int n = 64 << (random() % 7);   // 64 to 4096
        const float range = random() % 2 ? 1.0f : 30000.0f;
        for (int k = 0; k < n + n / 2; k++)
            g_in[k] = rnd_float(range);
        for (int side = 0; side < 2; side++) {
            begin(side);
            DWORD l[5] = {};
            at<void(__fastcall *)(DWORD *, int)>(MDCT_INIT)(l, n);
            ev(0x410, l[0], l[1], l[3] ? fnv(2166136261u, (void *)(uintptr_t)l[3], (DWORD)n) : 0);   // bitrev: n/4 ints
            float scale;
            memcpy(&scale, &l[4], 4);
            ev_float(0x411, scale);
            if (l[2])
                ev_floats(0x411, (const float *)(uintptr_t)l[2], (DWORD)(n + n / 4));
            memcpy(g_f[side], g_in, 4 * (DWORD)n);
            at<void(__fastcall *)(DWORD *, float *, float *)>(MDCT_FORWARD)(l, g_f[side], g_g[side]);
            ev_floats(0x412, g_g[side], (DWORD)n / 2);
            memcpy(g_g[side], g_in + n, 4 * (DWORD)(n / 2));   // backward on the same coefficients on both sides
            at<void(__fastcall *)(DWORD *, float *, float *)>(MDCT_BACKWARD)(l, g_g[side], g_f[side]);
            ev_floats(0x413, g_f[side], (DWORD)n);
            at<void(__fastcall *)(DWORD *)>(MDCT_CLEAR)(l);
            end(side);
        }
        same(t, nr, "mdct", seed);
    }
}

constexpr DWORD LPC_FROM_DATA = 0x5e4280, LPC_FROM_CURVE = 0x5e43f0, LPC_INIT = 0x5e44a0, LPC_CLEAR = 0x5e44d0, LSP_TO_CURVE = 0x5e9080,
                LPC_TO_LSP = 0x5e91f0;
int g_map[2050];
float g_curve[1024];
double x87_sin(double x)
{
    long double v = x;
    __asm__("fsin" : "+t"(v));
    return (double)v;
}
float g_lsp[2][64];
void test_lpc(Tally &t, Near &nr)
{
    for (int i = 0, cases = scaled(150); i < cases; i++) {
        const unsigned seed = 0x72000u + (unsigned)i;
        g_seed = seed * 2654435761u;
        const int mapped = 32 << (random() % 5);   // 32 to 512
        const int m = (int)(1 + random() % 32);
        const int n = (int)(m + 1 + random() % 1000);
        // from_curve: a positive power curve; from_data and lpc_to_lsp: a few sinusoids and a little noise, as
        // audio (Laguerre_With_Deflation loops until it converges, so no arbitrary filters)
        for (int k = 0; k < 2 * mapped; k++)
            g_curve[k] = (float)(random() % 1000) / 10.0f + 0.01f;
        double freq[3], gain[3];
        for (int j = 0; j < 3; j++) {
            freq[j] = 0.02 + (double)(random() % 1000) / 1000.0 * 1.5;
            gain[j] = (double)(1 + random() % 100) / 100.0;
        }
        for (int k = 0; k < n; k++) {
            double v = 0;
            for (int j = 0; j < 3; j++)
                v += gain[j] * x87_sin(freq[j] * k);
            g_in[k] = (float)v + rnd_float(0.01f);
        }
        // lsp_to_curve's input: a map like floor0_look's (non-decreasing, ends with -1) and sorted frequencies
        const int curve_n = (int)(16 + random() % 1000), ln = (int)(8 + random() % 300);
        for (int k = 0; k < curve_n; k++)
            g_map[k] = k * ln / curve_n;
        g_map[curve_n] = -1;
        // increasing frequencies in (0, pi) half way between the map's (floor0's lsp_to_curve evaluates at
        // pi * map[i] / ln), the amplitude and offset of a floor0 curve
        float lsp_in[64];
        int at_ = 0;
        for (int k = 0; k < m; k++) {
            at_ += 1 + (int)(random() % (DWORD)(ln / m > 1 ? ln / m : 1));
            lsp_in[k] = 3.14159265f * ((float)(at_ % ln) + 0.5f) / (float)ln;
        }
        for (int k = 1; k < m; k++)   // keep them increasing
            if (lsp_in[k] <= lsp_in[k - 1])
                lsp_in[k] = lsp_in[k - 1] + 0.001f;
        const float amp = (float)(random() % 400) / 10.0f, ampoffset = (float)(random() % 400) / 10.0f;
        // a lookup and the filter for lpc_to_lsp through the source-built side, the same for both sides
        DWORD shared[5] = {};
        at<void(__fastcall *)(DWORD *, long, int)>(LPC_INIT)(shared, mapped, m);
        float lpc_shared[64];
        memcpy(g_f[0], g_in, 4 * (DWORD)n);
        at<float(__fastcall *)(float *, float *, int, int)>(LPC_FROM_DATA)(g_f[0], lpc_shared, n, m);
        for (int side = 0; side < 2; side++) {
            begin(side);
            DWORD l[5] = {};
            at<void(__fastcall *)(DWORD *, long, int)>(LPC_INIT)(l, mapped, m);
            ev(0x420, l[0], l[3], l[4]);
            if (l[1])
                ev_floats(0x420, (const float *)(uintptr_t)l[1], 3 * 2 * (DWORD)mapped);
            at<void(__fastcall *)(DWORD *)>(LPC_CLEAR)(l);
            float lpc[64];
            memset(lpc, 0, sizeof lpc);
            memcpy(g_f[side], g_curve, 4 * (DWORD)(2 * mapped));
            const float e1 = at<float(__fastcall *)(float *, float *, DWORD *)>(LPC_FROM_CURVE)(g_f[side], lpc, shared);
            ev_float(0x421, e1);
            ev_floats(0x421, lpc, (DWORD)m);
            memcpy(g_f[side], g_in, 4 * (DWORD)n);
            const float e2 = at<float(__fastcall *)(float *, float *, int, int)>(LPC_FROM_DATA)(g_f[side], lpc, n, m);
            ev_float(0x422, e2);
            ev_floats(0x422, lpc, (DWORD)m);
            float lsp[64];
            memset(lsp, 0, sizeof lsp);
            memcpy(lpc, lpc_shared, 4 * (DWORD)m);
            const int r = at<int(__fastcall *)(float *, float *, int)>(LPC_TO_LSP)(lpc, lsp, m);
            ev(0x423, (DWORD)r);
            ev_floats(0x423, lsp, (DWORD)m);
            memcpy(g_lsp[side], lsp_in, 4 * (DWORD)m);
            at<void(__fastcall *)(float *, int *, int, int, float *, int, float, float)>(LSP_TO_CURVE)(g_g[side], g_map, curve_n, ln, g_lsp[side],
                                                                                                         m, amp, ampoffset);
            ev_floats(0x424, g_g[side], (DWORD)curve_n, EXPONENT);
            ev_floats(0x425, g_lsp[side], (DWORD)m);
            end(side);
        }
        at<void(__fastcall *)(DWORD *)>(LPC_CLEAR)(shared);
        same(t, nr, "lpc, lsp", seed);
    }
}

constexpr DWORD WRITEINIT = 0x5e98b0, WRITECLEAR = 0x5e98f0, READINIT = 0x5e9920, WRITE = 0x5e9950, LOOK = 0x5e9a50, ADV = 0x5e9b10,
                READ = 0x5e9b40, READ1 = 0x5e9c20;
void test_bitpack(Tally &t, Near &nr)
{
    for (int i = 0, cases = scaled(200); i < cases; i++) {
        const unsigned seed = 0x73000u + (unsigned)i;
        for (int side = 0; side < 2; side++) {
            begin(side);
            g_seed = seed * 2654435761u;
            DWORD b[5] = {};
            at<void(__fastcall *)(DWORD *)>(WRITEINIT)(b);
            const int writes = (int)(random() % 600);
            for (int k = 0; k < writes; k++) {
                const int nbits = (int)(random() % 33);
                at<void(__fastcall *)(DWORD *, DWORD, int)>(WRITE)(b, random(), nbits);
            }
            ev(0x430, b[0], b[1], b[4]);
            const DWORD bytes = b[0] + (b[1] ? 1 : 0);
            ev(0x431, fnv(2166136261u, (void *)(uintptr_t)b[2], bytes));
            static BYTE copy[2][20000];
            memcpy(copy[side], (void *)(uintptr_t)b[2], bytes < sizeof copy[side] ? bytes : sizeof copy[side]);
            at<void(__fastcall *)(DWORD *)>(WRITECLEAR)(b);
            DWORD r[5] = {};
            at<void(__fastcall *)(DWORD *, BYTE *, int)>(READINIT)(r, copy[side], (int)(bytes < sizeof copy[side] ? bytes : sizeof copy[side]));
            for (int k = 0; k < writes + 20; k++) {
                const unsigned op = random() % 4;
                const int nbits = (int)(random() % 33);
                long v = 0;
                if (op == 0)
                    v = at<long(__fastcall *)(DWORD *, int)>(LOOK)(r, nbits);
                else if (op == 1)
                    at<void(__fastcall *)(DWORD *, int)>(ADV)(r, nbits);
                else if (op == 2)
                    v = at<long(__fastcall *)(DWORD *, int)>(READ)(r, nbits);
                else
                    v = at<long(__fastcall *)(DWORD *)>(READ1)(r);
                ev(0x432, op, (DWORD)v, r[0] * 64 + r[1]);
            }
            end(side);
        }
        same(t, nr, "bit packer", seed);
    }
}

constexpr DWORD BOOK_CLEAR = 0x5e2120, BOOK_INIT_ENCODE = 0x5e21a0, BOOK_BESTERROR = 0x5e2680, BOOK_ENCODE = 0x5e2970;
// The static books of a game file, set up through the source-built side (they stay in the codec setup)
void test_books(Tally &t, Near &nr, int &books)
{
    books = 0;
    DWORD size;
    BYTE *data = read_game_file(CASES[0].path, size);
    if (!data)
        return;
    BYTE *vf = g_vf[0];
    memset(vf, 0, VF_SIZE);
    MemSource src = {data, size, 0, false};
    const OvCallbacks cb = {(void *)&mem_read, (void *)&mem_seek, (void *)&mem_close, (void *)&mem_tell};
    g_s = 0;
    if (ov_open(&src, vf, nullptr, 0, cb) == 0) {
        const BYTE *vi = ov_info(vf, -1);
        const BYTE *ci = *reinterpret_cast<BYTE *const *>(vi + 28);
        const int nbooks = *reinterpret_cast<const int *>(ci + 28);
        for (int bk = 0; bk < nbooks && bk < 256; bk++) {
            const BYTE *sb = *reinterpret_cast<BYTE *const *>(ci + 0x924 + 4 * bk);
            if (!sb)
                continue;
            books++;
            const int dim = *reinterpret_cast<const int *>(sb), entries = *reinterpret_cast<const int *>(sb + 4),
                      maptype = *reinterpret_cast<const int *>(sb + 12);
            const unsigned seed = 0x74000u + (unsigned)bk;
            for (int side = 0; side < 2; side++) {
                begin(side);
                g_seed = seed * 2654435761u;
                DWORD book[7] = {};
                const int r = at<int(__fastcall *)(DWORD *, const BYTE *)>(BOOK_INIT_ENCODE)(book, sb);
                ev(0x440, (DWORD)r, book[0], book[1]);
                ev(0x441, book[4] ? fnv(2166136261u, (void *)(uintptr_t)book[4], 4 * (DWORD)entries) : 0, book[6]);
                if (book[3])
                    ev_floats(0x441, (const float *)(uintptr_t)book[3], (DWORD)(dim * entries));
                DWORD w[5] = {};
                at<void(__fastcall *)(DWORD *)>(WRITEINIT)(w);
                for (int k = 0; k < 40 && entries > 0; k++) {
                    const int a = (int)(random() % (DWORD)entries);
                    ev(0x442, (DWORD)at<int(__fastcall *)(DWORD *, int, DWORD *)>(BOOK_ENCODE)(book, a, w));
                }
                ev(0x443, w[0], w[1], fnv(2166136261u, (void *)(uintptr_t)w[2], w[0] + 1));
                at<void(__fastcall *)(DWORD *)>(WRITECLEAR)(w);
                if (maptype && book[3]) {
                    for (int k = 0; k < 30; k++) {
                        float vec[64];
                        const int step = (int)(1 + random() % 2);
                        for (int j = 0; j < 64; j++)
                            vec[j] = rnd_float(random() % 2 ? 2.0f : 40.0f);
                        const int addmul = (int)(random() % 2);
                        const int best = dim * step <= 64 ? at<int(__fastcall *)(DWORD *, float *, int, int)>(BOOK_BESTERROR)(book, vec, step, addmul) : -1;
                        ev(0x444, (DWORD)best);
                        ev_floats(0x444, vec, 64);
                    }
                }
                at<void(__fastcall *)(DWORD *)>(BOOK_CLEAR)(book);
                end(side);
            }
            char what[64];
            wsprintfA(what, "book %d (dim %d, %d entries, map %d)", bk, dim, entries, maptype);
            same(t, nr, what, seed);
        }
        ov_clear(vf);
    }
    crt_free(data);
}


// ---- Psychoacoustics (psy.c, encoder only) --------------------------------------------------------------------------

// vorbis_info_psy of 1.0 beta 4 (psy.h, 1700 bytes)
struct InfoPsy {
    int athp, decayp;
    float ath_adjatt, ath_maxatt;
    int eighth_octave_lines, tonemaskp;
    float toneatt[17][11];
    int peakattp;
    float peakatt[17][11];
    int noisemaskp;
    float noisemaxsupp, noisewindowlo, noisewindowhi;
    int noisewindowlomin, noisewindowhimin;
    float noisemedian[34];
    float max_curve_dB, bound_att_dB, attack_coeff, decay_coeff;
};
static_assert(sizeof(InfoPsy) == 1700);
// The release's first encoder setting, _psy_set_A0 of modes/mode_A.h 1.14 (the release has no other source of
// realistic values; the game never sets one up)
void psy_set_a0(InfoPsy &p)
{
    static const float tone[3][11] = {{-999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999},
                                      {-30, -35, -35, -40, -40, -50, -60, -70, -80, -90, -100},
                                      {-40, -40, -40, -40, -40, -50, -60, -70, -80, -90, -100}};
    static const int tone_row[17] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 1, 1, 1, 1, 1};
    static const float peak[7][11] = {{-14, -16, -18, -19, -20, -21, -22, -22, -22, -22, -22},
                                      {-10, -10, -10, -10, -16, -16, -18, -20, -22, -22, -22},
                                      {-10, -10, -10, -10, -14, -14, -14, -20, -22, -22, -22},
                                      {-10, -10, -10, -10, -14, -14, -16, -20, -22, -22, -22},
                                      {-10, -10, -10, -10, -10, -11, -12, -13, -22, -22, -22},
                                      {-10, -10, -10, -10, -10, -10, -10, -11, -22, -22, -22},
                                      {-10, -10, -10, -10, -10, -10, -10, -10, -20, -21, -22}};
    static const int peak_row[17] = {0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 3, 3, 4, 4, 5, 6};
    static const float median[17] = {0, 0, 0, 0, 0, 0, 0, .3f, .5f, .5f, .5f, .5f, .6f, .7f, .85f, .9f, .9f};
    memset(&p, 0, sizeof p);
    p.athp = p.decayp = 1;
    p.ath_adjatt = -100;
    p.ath_maxatt = -140;
    p.eighth_octave_lines = 8;
    p.tonemaskp = p.peakattp = p.noisemaskp = 1;
    for (int b = 0; b < 17; b++)
        for (int l = 0; l < 11; l++) {
            p.toneatt[b][l] = tone[tone_row[b]][l];
            p.peakatt[b][l] = peak[peak_row[b]][l];
        }
    p.noisemaxsupp = -10;
    p.noisewindowlo = p.noisewindowhi = .5f;
    p.noisewindowlomin = p.noisewindowhimin = 5;
    for (int b = 0; b < 17; b++)
        p.noisemedian[2 * b] = median[b];
    p.max_curve_dB = 95;
    p.bound_att_dB = -22;
    p.attack_coeff = -0.f;
    p.decay_coeff = -.004f;
}

constexpr DWORD VI_PSY_FREE = 0x5e01e0, PSY_INIT = 0x5e0200, PSY_CLEAR = 0x5e0e90, COMPUTE_MASK = 0x5e0f50, APPLY_FLOOR = 0x5e1aa0;
// vorbis_look_psy: n, vi, tonecurves, peakatt, noisemedian, noiseoffset, ath, octave, bark, firstoc, shiftoc,
// eighth_octave_lines, total_octave_lines (13 dwords)
void ev_psy_look(const DWORD *l, int n)
{
    ev(0x450, l[0], l[9], l[10]);
    ev(0x451, l[11], l[12]);
    if (l[6])
        ev_floats(0x452, (const float *)(uintptr_t)l[6], (DWORD)n);   // ath
    if (l[7])
        ev(0x453, fnv(2166136261u, (void *)(uintptr_t)l[7], 4 * (DWORD)n));   // octave (longs)
    if (l[8])
        ev_floats(0x454, (const float *)(uintptr_t)l[8], (DWORD)n);   // bark
    if (l[4])
        ev_floats(0x455, (const float *)(uintptr_t)l[4], (DWORD)n);   // noisemedian
    if (l[5])
        ev_floats(0x456, (const float *)(uintptr_t)l[5], (DWORD)n);   // noiseoffset
    if (l[3])
        for (int b = 0; b < 17; b++)
            ev_floats(0x457, ((const float *const *)(uintptr_t)l[3])[b], 11);
    if (l[2]) {
        static float curves[17 * 11 * 58];
        for (int b = 0; b < 17; b++)
            for (int k = 0; k < 11; k++)
                memcpy(curves + (b * 11 + k) * 58, ((const float *const *const *)(uintptr_t)l[2])[b][k], 58 * 4);
        ev_floats(0x458, curves, 17 * 11 * 58);
    }
}
void test_psy(Tally &t, Near &nr)
{
    static float fft[2][1024], mdct[2][1024], flr[2][1024], decay[2][1024], in_fft[1024], in_mdct[1024], in_floor[1024];
    for (int i = 0, cases = scaled(24); i < cases; i++) {
        const unsigned seed = 0x76000u + (unsigned)i;
        g_seed = seed * 2654435761u;
        const int n = 64 << (random() % 5);   // 64 to 1024 (half the block)
        const long rates[3] = {11025, 22050, 44100};
        const long rate = rates[random() % 3];
        InfoPsy vi;
        psy_set_a0(vi);
        vi.athp = (int)(random() % 4 != 0);
        vi.tonemaskp = (int)(random() % 4 != 0);
        vi.peakattp = (int)(random() % 4 != 0);
        vi.noisemaskp = (int)(random() % 4 != 0);
        for (int k = 0; k < n; k++) {
            const bool peak = random() % 40 == 0;
            in_fft[k] = (float)(1 + random() % 1000) / 1000.0f * (peak ? 1000.0f : 1.0f);
            in_mdct[k] = rnd_float(peak ? 800.0f : 1.0f);
            in_floor[k] = random() % 10 == 0 ? 0.0f : (float)(1 + random() % 1000) / 100.0f;
        }
        const float specmax = (float)((int)(random() % 60) - 30);
        DWORD shared[13] = {};
        at<void(__fastcall *)(DWORD *, InfoPsy *, int, long)>(PSY_INIT)(shared, &vi, n, rate);
        for (int side = 0; side < 2; side++) {
            begin(side);
            InfoPsy *copy = (InfoPsy *)crt_malloc(sizeof(InfoPsy));
            memcpy(copy, &vi, sizeof vi);
            DWORD l[13] = {};
            at<void(__fastcall *)(DWORD *, InfoPsy *, int, long)>(PSY_INIT)(l, copy, n, rate);
            ev_psy_look(l, n);
            at<void(__fastcall *)(DWORD *)>(PSY_CLEAR)(l);
            ev(0x459, fnv(2166136261u, l, sizeof l));
            memcpy(fft[side], in_fft, 4 * (DWORD)n);
            memcpy(mdct[side], in_mdct, 4 * (DWORD)n);
            memset(flr[side], 0, 4 * (DWORD)n);
            memset(decay[side], 0, 4 * (DWORD)n);
            const float r = at<float(__fastcall *)(DWORD *, float *, float *, float *, float *, float)>(COMPUTE_MASK)(
                shared, fft[side], mdct[side], flr[side], decay[side], specmax);
            ev_float(0x45a, r);
            ev_floats(0x45b, flr[side], (DWORD)n, DECIBEL);
            ev_floats(0x45c, fft[side], (DWORD)n);
            ev_floats(0x45d, mdct[side], (DWORD)n);
            memcpy(mdct[side], in_mdct, 4 * (DWORD)n);
            at<void(__fastcall *)(DWORD *, float *, float *)>(APPLY_FLOOR)(shared, mdct[side], in_floor);
            ev_floats(0x45e, mdct[side], (DWORD)n);
            at<void(__fastcall *)(InfoPsy *)>(VI_PSY_FREE)(copy);
            at<void(__fastcall *)(InfoPsy *)>(VI_PSY_FREE)(nullptr);
            end(side);
        }
        at<void(__fastcall *)(DWORD *)>(PSY_CLEAR)(shared);
        same(t, nr, "psy", seed);
    }
}

// ---- The envelope's and IIR filters' clear (encoder only; their init is not in the exe) -------------------------

constexpr DWORD ENVELOPE_CLEAR = 0x5e2f00, IIR_CLEAR = 0x5e2f60;
void test_envelope(Tally &t)
{
    for (int i = 0, cases = scaled(40); i < cases; i++) {
        const unsigned seed = 0x77000u + (unsigned)i;
        for (int side = 0; side < 2; side++) {
            begin(side);
            g_seed = seed * 2654435761u;
            // envelope_lookup: ch, winlength, searchstep, minenergy, iir, filtered, storage, current, lastmark;
            // IIR_state: stages, coeff_A, coeff_B, z_A, ring, gain
            const DWORD ch = 1 + random() % 2;
            DWORD e[9] = {ch, random() % 4096, random() % 64, random(), 0, 0, random(), random(), random()};
            DWORD *iir = (DWORD *)crt_malloc(ch * 4 * 24);
            float **filtered = (float **)crt_malloc(ch * 4 * 4);
            for (DWORD k = 0; k < ch * 4; k++) {
                DWORD *st = iir + k * 6;
                st[0] = 1 + random() % 8;
                for (int j = 1; j <= 3; j++)
                    st[j] = (DWORD)(uintptr_t)crt_malloc(4 * (st[0] + 1));
                st[4] = random() % 8;
                st[5] = random();
                filtered[k] = (float *)crt_malloc(64);
            }
            e[4] = (DWORD)(uintptr_t)iir;
            e[5] = (DWORD)(uintptr_t)filtered;
            at<void(__fastcall *)(DWORD *)>(ENVELOPE_CLEAR)(e);
            ev(0x460, fnv(2166136261u, e, sizeof e));
            DWORD st[6] = {3, (DWORD)(uintptr_t)crt_malloc(16), (DWORD)(uintptr_t)crt_malloc(16), (DWORD)(uintptr_t)crt_malloc(16), 2, random()};
            at<void(__fastcall *)(DWORD *)>(IIR_CLEAR)(st);
            at<void(__fastcall *)(DWORD *)>(IIR_CLEAR)(nullptr);
            ev(0x461, fnv(2166136261u, st, sizeof st));
            end(side);
        }
        Near unused = {};
        same(t, unused, "envelope, IIR", seed);
    }
}

// ---- The backends' pack, copy_info and free_info on a game file's setup (encoder side, never called) ------------

constexpr DWORD MAPPING0_COPY = 0x5e44f0, MAPPING0_FREE = 0x5e4510, MAPPING0_PACK = 0x5e4810, RES0_COPY = 0x5e4fe0, RES0_FREE = 0x5e5000,
                RES0_PACK = 0x5e50a0, FLOOR0_COPY = 0x5e5ac0, FLOOR0_FREE = 0x5e5ae0, FLOOR0_PACK = 0x5e5b50;
void test_backends(Tally &t, int &setups)
{
    setups = 0;
    DWORD size;
    BYTE *data = read_game_file(CASES[0].path, size);
    if (!data)
        return;
    BYTE *vf = g_vf[0];
    memset(vf, 0, VF_SIZE);
    MemSource src = {data, size, 0, false};
    const OvCallbacks cb = {(void *)&mem_read, (void *)&mem_seek, (void *)&mem_close, (void *)&mem_tell};
    g_s = 0;
    if (ov_open(&src, vf, nullptr, 0, cb) == 0) {
        BYTE *vi = ov_info(vf, -1);
        const BYTE *ci = *reinterpret_cast<BYTE *const *>(vi + 28);
        struct Kind {
            int count_at, param_at;
            DWORD size, copy, free, pack;
            const char *what;
        } kinds[3] = {{12, 548, 1284, MAPPING0_COPY, MAPPING0_FREE, MAPPING0_PACK, "mapping0"},
                      {20, 1572, 96, FLOOR0_COPY, FLOOR0_FREE, FLOOR0_PACK, "floor0"},
                      {24, 2084, 2324, RES0_COPY, RES0_FREE, RES0_PACK, "residue0"}};
        for (const Kind &k : kinds) {
            const int count = *reinterpret_cast<const int *>(ci + k.count_at);
            for (int j = 0; j < count && j < 64; j++) {
                BYTE *info = *reinterpret_cast<BYTE *const *>(ci + k.param_at + 4 * j);
                if (!info)
                    continue;
                setups++;
                for (int side = 0; side < 2; side++) {
                    begin(side);
                    DWORD w[5] = {};
                    at<void(__fastcall *)(DWORD *)>(WRITEINIT)(w);
                    if (k.pack == MAPPING0_PACK)
                        at<void(__fastcall *)(BYTE *, BYTE *, DWORD *)>(k.pack)(vi, info, w);
                    else
                        at<void(__fastcall *)(BYTE *, DWORD *)>(k.pack)(info, w);
                    ev(0x470, w[0], w[1], fnv(2166136261u, (void *)(uintptr_t)w[2], w[0] + 1));
                    at<void(__fastcall *)(DWORD *)>(WRITECLEAR)(w);
                    BYTE *copy = at<BYTE *(__fastcall *)(BYTE *)>(k.copy)(info);
                    ev(0x471, copy ? fnv(2166136261u, copy, k.size) : 0, copy && !memcmp(copy, info, k.size));
                    at<void(__fastcall *)(BYTE *)>(k.free)(copy);
                    end(side);
                }
                char what[64];
                wsprintfA(what, "%s %d", k.what, j);
                Near unused = {};
                same(t, unused, what, 0x78000u + (DWORD)j);
            }
        }
        ov_clear(vf);
    }
    crt_free(data);
}


// ---- Each hook alone ----------------------------------------------------------------------------------------------
// As in checks_zlib.cpp: the source-built functions call each other directly, so the checks above cannot tell
// whether an inner function is the one its exe address was named after (two of mdct.c's were not, found by
// their stack bytes). Here every hook is out (the exe's copies) and one in at a time, and a workload that
// reaches the decoder, the seeks and the encoder's parts is compared with the exe's alone.
BYTE *g_quip, *g_music;
DWORD g_quip_n, g_music_n;
void workload_setup_parts(BYTE *vi)
{
    // codebooks and backends of the open file's setup (encoder side)
    const BYTE *ci = *reinterpret_cast<BYTE *const *>(vi + 28);
    const int nbooks = *reinterpret_cast<const int *>(ci + 28);
    for (int bk = 0, done = 0; bk < nbooks && done < 2; bk++) {
        const BYTE *sb = *reinterpret_cast<BYTE *const *>(ci + 0x924 + 4 * bk);
        if (!sb || !*reinterpret_cast<const int *>(sb + 12))
            continue;
        done++;
        const int dim = *reinterpret_cast<const int *>(sb), entries = *reinterpret_cast<const int *>(sb + 4);
        DWORD book[7] = {};
        ev(0x500, (DWORD)at<int(__fastcall *)(DWORD *, const BYTE *)>(BOOK_INIT_ENCODE)(book, sb));
        if (book[3])
            ev_floats(0x501, (const float *)(uintptr_t)book[3], (DWORD)(dim * entries));
        DWORD w[5] = {};
        at<void(__fastcall *)(DWORD *)>(WRITEINIT)(w);
        for (int k = 0; k < 8 && entries > 0; k++)
            ev(0x502, (DWORD)at<int(__fastcall *)(DWORD *, int, DWORD *)>(BOOK_ENCODE)(book, (k * 7) % entries, w));
        ev(0x503, w[0], w[1], fnv(2166136261u, (void *)(uintptr_t)w[2], w[0] + 1));
        at<void(__fastcall *)(DWORD *)>(WRITECLEAR)(w);
        float vec[64];
        for (int j = 0; j < 64; j++)
            vec[j] = (float)((j * 37) % 23) - 11.0f;
        if (dim <= 64)
            ev(0x504, (DWORD)at<int(__fastcall *)(DWORD *, float *, int, int)>(BOOK_BESTERROR)(book, vec, 1, 1));
        ev_floats(0x505, vec, 64);
        at<void(__fastcall *)(DWORD *)>(BOOK_CLEAR)(book);
    }
    const DWORD kinds[3][6] = {{12, 548, 1284, MAPPING0_COPY, MAPPING0_FREE, MAPPING0_PACK},
                               {20, 1572, 96, FLOOR0_COPY, FLOOR0_FREE, FLOOR0_PACK},
                               {24, 2084, 2324, RES0_COPY, RES0_FREE, RES0_PACK}};
    for (const auto &k : kinds) {
        if (*reinterpret_cast<const int *>(ci + k[0]) < 1)
            continue;
        BYTE *info = *reinterpret_cast<BYTE *const *>(ci + k[1]);
        DWORD w[5] = {};
        at<void(__fastcall *)(DWORD *)>(WRITEINIT)(w);
        if (k[5] == MAPPING0_PACK)
            at<void(__fastcall *)(BYTE *, BYTE *, DWORD *)>(k[5])(vi, info, w);
        else
            at<void(__fastcall *)(BYTE *, DWORD *)>(k[5])(info, w);
        ev(0x506, w[0], w[1], fnv(2166136261u, (void *)(uintptr_t)w[2], w[0] + 1));
        at<void(__fastcall *)(DWORD *)>(WRITECLEAR)(w);
        BYTE *copy = at<BYTE *(__fastcall *)(BYTE *)>(k[3])(info);
        ev(0x507, copy && !memcmp(copy, info, k[2]));
        at<void(__fastcall *)(BYTE *)>(k[4])(copy);
    }
}
void workload(int side)
{
    begin(side);
    g_word = 2;
    g_sgned = 1;
    g_big = 0;
    g_seed = 0x5eed3u;
    BYTE *vf = g_vf[side];
    const OvCallbacks cb = {(void *)&mem_read, (void *)&mem_seek, (void *)&mem_close, (void *)&mem_tell};
    // a quip, whole, and its setup's encoder parts
    memset(vf, 0, VF_SIZE);
    MemSource quip = {g_quip, g_quip_n, 0, false};
    if (ov_open(&quip, vf, nullptr, 0, cb) == 0) {
        ev_info(vf);
        read_pcm(vf, PCM_MAX);
        workload_setup_parts(ov_info(vf, -1));
        ev(0x510, (DWORD)ov_clear(vf));
    }
    // the same as 8-bit unsigned, big endian (ov_read's other paths), not seekable (_open_nonseekable)
    memset(vf, 0, VF_SIZE);
    quip = {g_quip, g_quip_n, 0, true};
    g_word = 1;
    g_sgned = 0;
    g_big = 1;
    if (ov_open(&quip, vf, nullptr, 0, cb) == 0) {
        read_pcm(vf, 3000);
        ev(0x511, (DWORD)ov_clear(vf));
    }
    g_word = 2;
    g_sgned = 1;
    g_big = 0;
    // music: the start, and every seek
    memset(vf, 0, VF_SIZE);
    MemSource music = {g_music, g_music_n, 0, false};
    if (ov_open(&music, vf, (const char *)g_music, 0, cb) == 0) {
        const long long total = ov_pcm_total(vf, -1);
        const double seconds = ov_time_total(vf, -1);
        read_pcm(vf, 4000);
        ev(0x512, (DWORD)ov_time_seek(vf, seconds * 0.5));
        ev_time_tell(0x513, vf);
        read_pcm(vf, 2000);
        ev(0x514, (DWORD)ov_pcm_seek_page(vf, (long long)((DWORD)total / 3)));
        read_pcm(vf, 2000);
        ev(0x515, (DWORD)ov_raw_seek(vf, (long)(g_music_n / 4)));
        read_pcm(vf, 2000);
        ev(0x516, (DWORD)ov_pcm_seek(vf, (long long)((DWORD)total / 5 + 333)));
        read_pcm(vf, 2000);
        ev_time_tell(0x517, vf);
        ev(0x518, (DWORD)ov_clear(vf));
    }
    // the encoder's transforms, LPC/LSP, psychoacoustics, bit packer, envelope, comparator, folded functions
    for (int k = 0; k < 256; k++)
        g_in[k] = (float)((k * 73) % 101) / 50.0f - 1.0f;
    const int sizes[2] = {60, 98};   // 4, 3, 5 and 2, 7, 7 (dradfg)
    for (int n : sizes) {
        DWORD l[3] = {};
        at<void(__fastcall *)(DWORD *, int)>(DRFT_INIT)(l, n);
        ev_floats(0x520, (const float *)(uintptr_t)l[1], 3 * (DWORD)n);
        memcpy(g_f[side], g_in, 4 * (DWORD)n);
        at<void(__fastcall *)(DWORD *, float *)>(DRFT_FORWARD)(l, g_f[side]);
        ev_floats(0x521, g_f[side], (DWORD)n);
        memcpy(g_f[side], g_in, 4 * (DWORD)n);
        at<void(__fastcall *)(DWORD *, float *)>(DRFT_BACKWARD)(l, g_f[side]);
        ev_floats(0x522, g_f[side], (DWORD)n);
        at<void(__fastcall *)(DWORD *)>(DRFT_CLEAR)(l);
    }
    {
        DWORD l[5] = {};
        at<void(__fastcall *)(DWORD *, int)>(MDCT_INIT)(l, 256);
        memcpy(g_f[side], g_in, 4 * 256);
        at<void(__fastcall *)(DWORD *, float *, float *)>(MDCT_FORWARD)(l, g_f[side], g_g[side]);
        ev_floats(0x523, g_g[side], 128);
        memcpy(g_g[side], g_in, 4 * 128);
        at<void(__fastcall *)(DWORD *, float *, float *)>(MDCT_BACKWARD)(l, g_g[side], g_f[side]);
        ev_floats(0x524, g_f[side], 256);
        at<void(__fastcall *)(DWORD *)>(MDCT_CLEAR)(l);
    }
    {
        DWORD l[5] = {};
        at<void(__fastcall *)(DWORD *, long, int)>(LPC_INIT)(l, 64, 12);
        float lpc[16], lsp[16];
        for (int k = 0; k < 128; k++)
            g_f[side][k] = (float)(1 + (k * 31) % 97) / 10.0f;
        ev_float(0x525, at<float(__fastcall *)(float *, float *, DWORD *)>(LPC_FROM_CURVE)(g_f[side], lpc, l));
        ev_floats(0x525, lpc, 12);
        for (int k = 0; k < 256; k++)
            g_f[side][k] = (float)(x87_sin(0.3 * k) + 0.5 * x87_sin(1.1 * k) + 0.01 * g_in[k]);
        ev_float(0x526, at<float(__fastcall *)(float *, float *, int, int)>(LPC_FROM_DATA)(g_f[side], lpc, 256, 12));
        ev_floats(0x526, lpc, 12);
        ev(0x527, (DWORD)at<int(__fastcall *)(float *, float *, int)>(LPC_TO_LSP)(lpc, lsp, 12));
        ev_floats(0x527, lsp, 12);
        for (int k = 0; k < 200; k++)
            g_map[k] = k * 50 / 200;
        g_map[200] = -1;
        for (int k = 0; k < 12; k++)
            lsp[k] = 3.14159265f * ((float)(k * 4 + 1) + 0.5f) / 50.0f;
        at<void(__fastcall *)(float *, int *, int, int, float *, int, float, float)>(LSP_TO_CURVE)(g_g[side], g_map, 200, 50, lsp, 12, 20.0f, 30.0f);
        ev_floats(0x528, g_g[side], 200, EXPONENT);
        at<void(__fastcall *)(DWORD *)>(LPC_CLEAR)(l);
    }
    {
        InfoPsy *vi = (InfoPsy *)crt_malloc(sizeof(InfoPsy));
        psy_set_a0(*vi);
        DWORD l[13] = {};
        at<void(__fastcall *)(DWORD *, InfoPsy *, int, long)>(PSY_INIT)(l, vi, 64, 22050);
        ev_psy_look(l, 64);
        static float fft[64], mdct[64], flr[64], decay[64], fl[64];
        for (int k = 0; k < 64; k++) {
            fft[k] = (float)(1 + (k * 13) % 50) * (k % 9 == 0 ? 100.0f : 0.1f);
            mdct[k] = g_in[k] * (k % 9 == 0 ? 500.0f : 1.0f);
            fl[k] = k % 7 == 0 ? 0.0f : (float)(k + 1) / 8.0f;
            flr[k] = decay[k] = 0;
        }
        ev_float(0x529, at<float(__fastcall *)(DWORD *, float *, float *, float *, float *, float)>(COMPUTE_MASK)(l, fft, mdct, flr, decay, 0.0f));
        ev_floats(0x52a, flr, 64, DECIBEL);
        at<void(__fastcall *)(DWORD *, float *, float *)>(APPLY_FLOOR)(l, mdct, fl);
        ev_floats(0x52b, mdct, 64);
        at<void(__fastcall *)(DWORD *)>(PSY_CLEAR)(l);
        at<void(__fastcall *)(InfoPsy *)>(VI_PSY_FREE)(vi);
    }
    {
        DWORD b[5] = {};
        at<void(__fastcall *)(DWORD *)>(WRITEINIT)(b);
        for (int k = 0; k < 300; k++)
            at<void(__fastcall *)(DWORD *, DWORD, int)>(WRITE)(b, (DWORD)k * 2654435761u, k % 33);
        ev(0x530, b[0], b[1], fnv(2166136261u, (void *)(uintptr_t)b[2], b[0] + 1));
        DWORD r[5] = {};
        at<void(__fastcall *)(DWORD *, BYTE *, int)>(READINIT)(r, (BYTE *)(uintptr_t)b[2], (int)b[0]);
        for (int k = 0; k < 300; k++) {
            const int nb = k % 33;
            const long v = k % 4 == 0 ? at<long(__fastcall *)(DWORD *, int)>(LOOK)(r, nb) : k % 4 == 1 ? at<long(__fastcall *)(DWORD *)>(READ1)(r)
                                                                                                  : at<long(__fastcall *)(DWORD *, int)>(READ)(r, nb);
            if (k % 4 == 0)
                at<void(__fastcall *)(DWORD *, int)>(ADV)(r, nb);
            ev(0x531, (DWORD)v, r[0] * 64 + r[1]);
        }
        at<void(__fastcall *)(DWORD *)>(WRITECLEAR)(b);
    }
    {
        DWORD e[9] = {2, 100, 10, 0, 0, 0, 0, 0, 0};
        DWORD *iir = (DWORD *)crt_malloc(8 * 24);
        float **filtered = (float **)crt_malloc(8 * 4);
        for (int k = 0; k < 8; k++) {
            iir[k * 6] = 2;
            for (int j = 1; j <= 3; j++)
                iir[k * 6 + j] = (DWORD)(uintptr_t)crt_malloc(12);
            filtered[k] = (float *)crt_malloc(16);
        }
        e[4] = (DWORD)(uintptr_t)iir;
        e[5] = (DWORD)(uintptr_t)filtered;
        at<void(__fastcall *)(DWORD *)>(ENVELOPE_CLEAR)(e);
        ev(0x532, fnv(2166136261u, e, sizeof e));
    }
    {
        const float a = 1.5f, b = -2.0f;
        ev(0x533, (DWORD)at<int(__cdecl *)(const void *, const void *)>(0x5e9430)(&a, &b),
           (DWORD)at<int(__cdecl *)(const void *, const void *)>(0x5e9430)(&b, &a));
        const char *u = at<const char *(__fastcall *)(void *, void *)>(0x5e6430)(nullptr, nullptr);
        const char *lk = at<const char *(__fastcall *)(void *, void *, void *)>(0x5e6440)(nullptr, nullptr, nullptr);
        at<void(__fastcall *)(void *, void *)>(0x5e6450)(nullptr, nullptr);
        const int f = at<int(__fastcall *)(void *, void *, void *, void *)>(0x5e6460)(nullptr, nullptr, nullptr, nullptr);
        at<void(__fastcall *)(const char *, int, float *, int, int, int)>(0x5e44e0)("x", 0, nullptr, 0, 0, 0);
        ev(0x534, u && !*u, lk && !*lk, (DWORD)f);
    }
    end(side);
}
void test_each_hook(Tally &t, Near &nr, int &hooks)
{
    hooks = 0;
    g_quip = read_game_file(CASES[5].path, g_quip_n);
    g_music = read_game_file(CASES[10].path, g_music_n);
    if (!g_quip || !g_music) {
        log("  vorbis each hook alone: files missing");
        t.check(false, "", "[files]");
    } else {
        g_hooks.out();
        g_each = true;
        workload(0);
        // the reference's PCM and floats stay in side 0's buffers
        hooks = g_hooks.n;
        for (int k = 0; k < g_hooks.n; k++) {
            g_hooks.one_in(k);
            workload(1);
            g_hooks.one_out(k);
            same(t, nr, g_hooks.name(k), (unsigned)k);
        }
        g_each = false;
        g_hooks.in();
    }
    crt_free(g_quip);
    crt_free(g_music);
    g_quip = g_music = nullptr;
}

}  // namespace

ZAX_CHECKS(vorbis)
{
    (void)cls;
    (void)n;
    g_hooks.module = "vorbis";
    g_hooks.table = hooks_vorbis;
    for (int side = 0; side < 2; side++)
        if (!g_pcm[side])
            g_pcm[side] = (BYTE *)VirtualAlloc(nullptr, PCM_MAX, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_pcm[0] || !g_pcm[1]) {
        log("Check vorbis: MISMATCH (no memory for the PCM buffers)");
        return;
    }
    g_streams.pause();
    log("  vorbis: %d sound streams paused", g_streams.n);
    Tally files{"vorbis: the game's .ogg files through vorbisfile (open, info, totals, ov_read, seeks, callbacks; real data)"},
        smallft{"vorbis: smallft drft_init/forward/backward/clear"}, mdct{"vorbis: MDCT init/forward/backward/clear"},
        lpc{"vorbis: LPC and LSP (lpc_init, from_curve, from_data, lpc_to_lsp, lsp_to_curve, lpc_clear)"},
        bitpack{"vorbis: the bit packer writing and reading"}, books{"vorbis: codebooks of a game file, encoder side"},
        endian{"vorbis: host_is_big_endian (folded with 0x46b760)"};
    Near n_files = {}, n_smallft = {}, n_mdct = {}, n_lpc = {}, n_bitpack = {}, n_books = {};
    int found = 0, nbooks = 0;
    test_files(files, n_files, found);
    files.report();
    n_files.report("vorbis files");
    log("  vorbis real data: %d of %d files", found, (int)(sizeof CASES / sizeof CASES[0]));
    test_smallft(smallft, n_smallft);
    smallft.report();
    n_smallft.report("vorbis smallft");
    test_mdct(mdct, n_mdct);
    mdct.report();
    n_mdct.report("vorbis mdct");
    test_lpc(lpc, n_lpc);
    lpc.report();
    n_lpc.report("vorbis lpc");
    test_bitpack(bitpack, n_bitpack);
    bitpack.report();
    test_books(books, n_books, nbooks);
    books.report();
    n_books.report("vorbis books");
    log("  vorbis books: %d", nbooks);
    Tally psy{"vorbis: psychoacoustics (_vp_psy_init, _vp_compute_mask, _vp_apply_floor, _vp_psy_clear, _vi_psy_free)"},
        envelope{"vorbis: _ve_envelope_clear and IIR_clear"}, backends{"vorbis: the backends' pack, copy_info and free_info"};
    Near n_psy = {};
    test_psy(psy, n_psy);
    psy.report();
    n_psy.report("vorbis psy");
    test_envelope(envelope);
    envelope.report();
    int setups = 0;
    test_backends(backends, setups);
    backends.report();
    log("  vorbis backend setups: %d", setups);
    Tally each{"vorbis: each hook alone among the exe's functions (a workload through the decoder, the seeks and the encoder's parts)"};
    Near n_each = {};
    int hooks = 0;
    test_each_hook(each, n_each, hooks);
    each.report();
    n_each.report("vorbis each hook alone");
    log("  vorbis each hook alone: %d hooks", hooks);
    endian.check(at<int(__cdecl *)()>(0x46b760)() == host_is_big_endian(), "", "[0x46b760 / host_is_big_endian]");
    endian.report();
    // lsp.c's qsort comparator, cdecl in the exe (the hook goes to a cdecl entry into the source's comp)
    Tally comp{"vorbis: comp 0x5e9430 (lsp.c's qsort comparator, cdecl)"};
    for (int i = 0, cases = scaled(200); i < cases; i++) {
        g_seed = (0x75000u + (unsigned)i) * 2654435761u;
        float a = rnd_float(10.0f), b = i % 5 == 0 ? a : rnd_float(10.0f);
        int r[2];
        for (int side = 0; side < 2; side++) {
            begin(side);
            r[side] = at<int(__cdecl *)(const void *, const void *)>(0x5e9430)(&a, &b);
            end(side);
        }
        comp.check(r[0] == r[1], "", "[comp]");
    }
    comp.report();
    // The empty functions folded by the linker (time0's, _analysis_output with a game vtable slot): returns and
    // the stack (fastcall: two register arguments, the rest popped by the callee)
    Tally folded{"vorbis: time0_unpack/look/pack/forward 0x5e6430-0x5e6460, _analysis_output 0x5e44e0 (folded)"};
    for (int side = 0; side < 2; side++) {
        begin(side);
        const char *u = at<const char *(__fastcall *)(void *, void *)>(0x5e6430)(nullptr, nullptr);   // unpack, copy_info
        const char *l = at<const char *(__fastcall *)(void *, void *, void *)>(0x5e6440)(nullptr, nullptr, nullptr);   // look
        at<void(__fastcall *)(void *, void *)>(0x5e6450)(nullptr, nullptr);   // pack, free_info, free_look, tr_static_init
        const int f = at<int(__fastcall *)(void *, void *, void *, void *)>(0x5e6460)(nullptr, nullptr, nullptr, nullptr);
        at<void(__fastcall *)(const char *, int, float *, int, int, int)>(0x5e44e0)("x", 0, nullptr, 0, 0, 0);
        ev(0x480, u && !*u, l && !*l, (DWORD)f);
        end(side);
    }
    {
        Near unused = {};
        same(folded, unused, "folded", 0);
    }
    folded.report();
    g_streams.resume();
}

}  // namespace zax
