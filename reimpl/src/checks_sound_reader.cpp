// Checks of sound_reader.cpp against the original, in the game thread.
//
// In the sound fake world (checks_sound_common.h): the file buffer is a recorder over a memory file of
// the case (RIFF/WAVE images made from the seed, other bytes, and the first 4 KB of real sound files),
// ACM and the Ogg Vorbis open are recorders with answers of the case (also failures), the readers under
// test lie in each side's arena (or have a recording vtable where a function calls their Read). The two
// pools (the stream buffers 0x716080 and the Ogg states 0x7160e0, with their guards and warm-up flags
// and the Ogg callbacks) are set per case: constructed or not, warm or not, 0-4 blocks (in the arena)
// with random free flags, always with room, so no pool array grows into the game's pool from arena data;
// a case that constructs a pool lets it grow from the game's pool and gives those blocks back after each
// side (the second side gets the same ones). Compared: eax (st(0) for the seconds), the calls, the
// arenas, the fakes and both pools' state.
//
// The stream's multimedia timer runs through ReadParts, ReadMix, the pool's Take and Give, the arrays'
// constructors and the readers' Read and Seek while a level's music plays; those hook sites are never
// put out (checks_sound_common.h), their cases call the original through its trampoline.
//
// Real data: the first 4 KB of up to 24 files of the sound list (0x713708; their names, sizes and chunks)
// through the factory, Open and the first Reads.
#include "checks_sound_common.h"

namespace zax {

int hooks_sound_reader(Hook *out);

namespace {

using namespace sndc;

constexpr DWORD VT_BASE = 0x600bfc, VT_RAW = 0x600bcc, VT_WAVE = 0x600be4;
enum { K_FILE, K_READER };
enum { F_FILE, F_FILE2, F_OGG_ARENA };
const DWORD NEVER_OUT[] = {0x57d330, 0x57d550, 0x57cd40, 0x57cdb0, 0x57ced0, 0x525760, 0x57d2f0, 0x57db50, 0x57d320};
const Module MODULE = {hooks_sound_reader, NEVER_OUT, sizeof NEVER_OUT / 4};

// ---- The memory file --------------------------------------------------------------------------------------

BYTE g_image[0x1400];
struct MemFile {
    DWORD size, avail, pos;
    bool open_ok;
} g_file;
DWORD c_fb_ctor(Call &c)
{
    dw(at<void *>(c.ecx), 0) = ptrv(g_vt[K_FILE]);
    return c.ecx;
}
DWORD c_fb_open(Call &)
{
    g_file.pos = 0;
    return g_file.open_ok ? 1 : (nv() & 0xffffff00);
}
DWORD g_reads;   // reads of the case: a chunk length that wraps around seeks back forever (in the exe too)
DWORD c_fb_read(Call &c)
{
    if (++g_reads > 20000)
        g_file.pos = 0xfffff000;   // past any RIFF size: the chunk loop ends
    // always the whole request (beyond the end a pattern): a short read would leave the exe's locals as
    // the stack had them, different on the two sides
    BYTE *out = at<BYTE *>(c.a[0]);
    const DWORD n = c.a[1];
    for (DWORD i = 0; i < n; i++) {
        const DWORD p = g_file.pos + i;
        out[i] = p < g_file.avail ? g_image[p] : (BYTE)(p * 7);
    }
    g_file.pos += n;
    return n;
}
DWORD c_fb_seek(Call &c)
{
    g_file.pos = c.a[0];
    return c.a[0];
}
DWORD c_fb_tell(Call &) { return g_file.pos; }
DWORD c_fb_size(Call &) { return g_file.size; }
DWORD c_fb_skip(Call &c)
{
    g_file.pos += c.a[0];
    return g_file.pos;
}
DWORD c_this(Call &c) { return c.ecx; }

// ---- ACM ----------------------------------------------------------------------------------------------------

DWORD c_acm_suggest(Call &c)
{
    if (nv() % 6 == 0)
        return 0x200001;
    const BYTE *src = at<const BYTE *>(c.a[1]);
    BYTE *dst = at<BYTE *>(c.a[2]);
    fl<WORD>(dst, 0) = 1;
    fl<WORD>(dst, 2) = fl<WORD>((void *)src, 2);
    dw(dst, 4) = dw((void *)src, 4);
    fl<WORD>(dst, 0xe) = 16;
    fl<WORD>(dst, 0xc) = (WORD)(2 * fl<WORD>(dst, 2));
    dw(dst, 8) = dw(dst, 4) * fl<WORD>(dst, 0xc);
    return 0;
}
DWORD c_acm_open(Call &c)
{
    if (nv() % 6 == 0)
        return 0x200002;
    dw(at<void *>(c.a[0]), 0) = 0x5a5a0001;
    return 0;
}
DWORD c_acm_size(Call &c)
{
    if (nv() % 5 == 0)
        return 0x200003;
    dw(at<void *>(c.a[2]), 0) = c.a[1] / 4 + nv() % 7;
    return 0;
}
DWORD c_acm_convert(Call &c)
{
    DWORD *h = at<DWORD *>(c.a[1]);
    if (nv() % 8 == 0)
        return 0x200004;
    h[5] = h[4] - (nv() % 3 ? 0 : (h[4] < 4 ? h[4] : nv() % 5));
    DWORD out = h[4] * 4 - nv() % 3;
    if (out > h[8])
        out = h[8];
    h[9] = out;
    BYTE *dst = at<BYTE *>(h[7]);
    for (DWORD i = 0; i < out; i++)
        dst[i] = (BYTE)(i * 13 + 5);
    return 0;
}

const Spec DIRECT[] = {
    {0x4a1780, "FileBuf_Ctor", 't', "", 'c', c_fb_ctor},
    {0x4a1840, "FileBuf_Open", 't', "sv", 'c', c_fb_open},
    {0x4a1b00, "FileBuf_Read", 't', "-v", 'c', c_fb_read},   // the buffers are locals of different frames
    {0x4a20e0, "FileBuf_Seek", 't', "v", 'c', c_fb_seek},
    {0x4a2180, "FileBuf_Tell", 't', "", 'c', c_fb_tell},
    {0x4a2190, "FileBuf_Size", 't', "", 'c', c_fb_size},
    {0x4a20c0, "FileBuf_Skip", 't', "v", 'c', c_fb_skip},
    {0x5caa58, "acmStreamClose", 's', "vv", 'z', nullptr},
    {0x5caa64, "acmFormatSuggest", 's', "pwpvv", 'c', c_acm_suggest},
    {0x5caa5e, "acmStreamOpen", 's', "ppwpvvvv", 'c', c_acm_open},
    {0x5caa7c, "acmStreamSize", 's', "vv-v", 'c', c_acm_size},   // the output and the headers are locals
    {0x5caa76, "acmStreamPrepareHeader", 's', "v-v", 'h', nullptr},
    {0x5caa70, "acmStreamConvert", 's', "v-v", 'c', c_acm_convert},
    {0x5caa6a, "acmStreamUnprepareHeader", 's', "v-v", 'h', nullptr},
    {0x525aa0, "OggStream_Open", 't', "p", 'b', nullptr},
    {0x525820, "COggSoundReader::DeletingDtor", 't', "v", 'c', c_this},
    {0x5d0863, "atexit", 'c', "v", 'z', nullptr},
};
const Spec FILE_SLOTS[] = {{0, "CFileBuf::DeletingDtor", 't', "v", 'c', c_this}};
// A reader whose Read a function calls: the bytes of the case (also fewer than asked)
DWORD c_reader_read(Call &c)
{
    BYTE *out = at<BYTE *>(c.a[0]);
    DWORD n = c.a[1];
    if (nv() % 5 == 0)
        n = n ? nv() % n : 0;
    for (DWORD i = 0; i < n; i++)
        out[i] = (BYTE)(nv() >> 3);
    return n;
}
const Spec READER_SLOTS[] = {
    {0, nullptr, 0, "", 0, nullptr},
    {0, nullptr, 0, "", 0, nullptr},
    {0, "Reader::Read", 't', "pv", 'c', c_reader_read},
};
const Standins STANDINS[] = {{DIRECT, sizeof DIRECT / sizeof DIRECT[0]}};

// ---- The pools of a case ------------------------------------------------------------------------------------

struct PoolDef {
    DWORD base, guard, warm, block, vt_ptrs;
};
const PoolDef POOLS[2] = {{0x716080, 0x716078, 0x713704, 100000, 0x600c20}, {0x7160e0, 0x7160c0, 0x6d0164, 0x2c0, 0x5fba54}};
SavedBytes g_saved_pool[2], g_saved_guard[2], g_saved_warm[2], g_saved_callbacks;
bool g_constructs[2];

void pools_save()
{
    for (int p = 0; p < 2; p++) {
        save_bytes(g_saved_pool[p], POOLS[p].base, 0x20);
        save_bytes(g_saved_guard[p], POOLS[p].guard, 1);
        save_bytes(g_saved_warm[p], POOLS[p].warm, 1);
    }
    save_bytes(g_saved_callbacks, 0x6d0150, 0x10);
}
void pools_restore()
{
    for (int p = 0; p < 2; p++) {
        restore_bytes(g_saved_warm[p]);
        restore_bytes(g_saved_guard[p]);
        restore_bytes(g_saved_pool[p]);
    }
    restore_bytes(g_saved_callbacks);
}
// The pools of the case in the running side's arena; `fresh` a pool that is not constructed yet
void pools_build(Gen &g, bool fresh_buffers, bool fresh_ogg)
{
    for (int p = 0; p < 2; p++) {
        const PoolDef &d = POOLS[p];
        BYTE *pool = at<BYTE *>(d.base);
        const bool fresh = p == 0 ? fresh_buffers : fresh_ogg;
        g_constructs[p] = fresh;
        *at<BYTE *>(d.guard) = fresh ? 0 : 1;
        *at<BYTE *>(d.warm) = (BYTE)(g.next() % 3 ? 1 : 0);
        const DWORD count = fresh ? 0 : g.below(4), cap = count + 8;
        DWORD *data = reinterpret_cast<DWORD *>(alloc(4 * cap));
        BYTE *flags = alloc(cap);
        for (DWORD i = 0; i < count; i++) {
            data[i] = ptrv(alloc(d.block < 0x1000 ? d.block : 0x600));   // the ring buffer reads use at most 0x400 bytes
            flags[i] = (BYTE)(g.next() % 2);
        }
        dw(pool, 0) = d.vt_ptrs;
        dw(pool, 4) = ptrv(data);
        dw(pool, 8) = count;
        dw(pool, 0xc) = cap;
        dw(pool, 0x10) = 0x5fba60;
        dw(pool, 0x14) = ptrv(flags);
        dw(pool, 0x18) = count;
        dw(pool, 0x1c) = cap;
    }
    if (g.next() % 3 == 0)
        memset(at<void *>(0x6d0150), 0, 0x10);
}
// After a side: both pools' state into the arena (compared with the rest); the arrays a construction
// grew in the game's pool given back, so the other side gets the same blocks
void pools_snapshot()
{
    for (int p = 0; p < 2; p++) {
        BYTE *pool = at<BYTE *>(POOLS[p].base);
        BYTE *copy = alloc(0x22);
        memcpy(copy, pool, 0x20);
        // the arrays' data compare by their contents (below): a construction grows them in the game's
        // pool, whose free lists differ between the sides
        dw(copy, 4) = 0;
        dw(copy, 0x14) = 0;
        copy[0x20] = *at<BYTE *>(POOLS[p].guard);
        copy[0x21] = *at<BYTE *>(POOLS[p].warm);
        const DWORD n = dw(pool, 8) < 64 ? dw(pool, 8) : 64;
        memcpy(alloc(4 * n + 4), at<void *>(dw(pool, 4)), 4 * n);
        memcpy(alloc(n + 4), at<void *>(dw(pool, 0x14)), n);
        if (g_constructs[p]) {
            if (dw(pool, 4) && !in_arena(dw(pool, 4), 0) && !in_arena(dw(pool, 4), 1))
                at<void(THISCALL *)(void *, void *, unsigned)>(0x5065e0)(at<void *(__cdecl *)()>(0x506470)(), at<void *>(dw(pool, 4)), 4 * dw(pool, 0xc));
            if (dw(pool, 0x14) && !in_arena(dw(pool, 0x14), 0) && !in_arena(dw(pool, 0x14), 1))
                at<void(THISCALL *)(void *, void *, unsigned)>(0x5065e0)(at<void *(__cdecl *)()>(0x506470)(), at<void *>(dw(pool, 0x14)), dw(pool, 0x1c));
        }
    }
    BYTE *cb = alloc(0x10);
    memcpy(cb, at<void *>(0x6d0150), 0x10);
}

// ---- The images ------------------------------------------------------------------------------------------------

struct Real {
    char name[264];
    DWORD size, avail;
    BYTE head[0x1000];
};
Real *g_reals;
int g_nreals;
const Real *g_real;

void put32(DWORD &at_, DWORD v)
{
    if (at_ + 4 <= sizeof g_image)
        memcpy(g_image + at_, &v, 4);
    at_ += 4;
}
void put16(DWORD &at_, WORD v)
{
    if (at_ + 2 <= sizeof g_image)
        memcpy(g_image + at_, &v, 2);
    at_ += 2;
}
// A RIFF/WAVE image: chunks fmt (PCM or a compressed tag), fact, data and a junk chunk in a random order,
// some missing or cut, the RIFF size right, short or long
void make_wave(Gen &g)
{
    memset(g_image, 0, sizeof g_image);
    DWORD p = 0;
    put32(p, 0x46464952);
    const DWORD size_at = p;
    put32(p, 0);
    put32(p, g.next() % 10 ? 0x45564157 : 0x4c4c4f4c);
    int order[4] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) {
        const int j = (int)g.below((DWORD)i + 1), t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (int k = 0; k < 4; k++) {
        switch (order[k]) {
        case 0:   // fmt
            if (g.next() % 8 == 0)
                break;
            put32(p, 0x20746d66);
            put32(p, g.next() % 4 ? 16 : 18 + g.below(6));
            put16(p, (WORD)(g.next() % 3 ? 1 : g.next() % 2 ? 2 : 0x11));
            put16(p, (WORD)(1 + g.below(2)));
            put32(p, g.next() % 2 ? 22050 : 11025);
            put32(p, 44100);
            put16(p, (WORD)(1 << g.below(3)));
            put16(p, (WORD)(g.next() % 2 ? 16 : 8));
            put16(p, (WORD)g.below(4));
            if (g.next() % 2)
                p += 2 * g.below(3);
            break;
        case 1:   // fact
            if (g.next() % 2)
                break;
            put32(p, 0x74636166);
            put32(p, 4);
            put32(p, 1000 + g.below(40000));
            break;
        case 2:   // data
            if (g.next() % 8 == 0)
                break;
            put32(p, 0x61746164);
            put32(p, g.below(3000) + (g.next() % 2));
            p += g.below(64);
            break;
        default:   // LIST
            put32(p, 0x5453494c);
            put32(p, g.below(40) + 1);
            p += g.below(40);
            break;
        }
    }
    const DWORD k = g.below(5);
    const DWORD riff = k == 0 ? p / 2 : k == 1 ? p + 400 : p - 8;
    memcpy(g_image + size_at, &riff, 4);
    g_file.avail = p < sizeof g_image ? p : sizeof g_image;
    g_file.size = g.next() % 3 ? p + g.below(5000) : p;
}
bool g_wave_only;   // WAVE's Open gets structured images (garbage with a huge RIFF size loops in the exe too)
void make_image(Gen &g)
{
    if (g_real) {
        memcpy(g_image, g_real->head, g_real->avail);
        g_file.avail = g_real->avail;
        g_file.size = g_real->size;
    } else if (g.next() % 4 || g_wave_only) {
        make_wave(g);
    } else {
        for (DWORD i = 0; i < 64; i++)
            g_image[i] = (BYTE)g.next();
        g_file.avail = 64;
        g_file.size = g.below(200000);
    }
    g_file.open_ok = g.next() % 8 != 0;
    g_file.pos = 0;
}

// ---- Readers of a case --------------------------------------------------------------------------------------------

DWORD ff(Gen &g, float lo, float hi) { return g.fbits_(lo, hi); }
BYTE *make_format(Gen &g)
{
    BYTE *f = alloc(0x14);
    fl<WORD>(f, 0) = (WORD)(g.next() % 3 ? 1 : 2);
    fl<WORD>(f, 2) = (WORD)(1 + g.below(2));
    dw(f, 4) = g.next() % 2 ? 22050 : 11025;
    dw(f, 8) = g.next() % 9 ? 44100 * (1 + g.below(2)) : g.next() % 2 ? 0 : g.next();
    fl<WORD>(f, 0xc) = (WORD)(1 << g.below(3));
    fl<WORD>(f, 0xe) = (WORD)(g.next() % 2 ? 16 : 8);
    fl<WORD>(f, 0x10) = (WORD)g.below(3);
    return f;
}
// A pool block of the case's stream buffer pool (or 0)
DWORD pool_block(Gen &g, int p)
{
    BYTE *pool = at<BYTE *>(POOLS[p].base);
    if (!dw(pool, 8) || g.next() % 3 == 0)
        return g.next() % 4 ? 0 : ptrv(alloc(0x40));   // also a block that is not in the pool
    return reinterpret_cast<DWORD *>(dw(pool, 4))[g.below(dw(pool, 8))];
}
BYTE *make_reader(Gen &g, DWORD vtable, DWORD size)
{
    BYTE *r = alloc(size);
    dw(r, 0) = vtable;
    dw(r, 4) = g.next() % 3 ? ptrv(make_format(g)) : 0;
    dw(r, 8) = g.next() % 4 ? ptrv(fake(F_FILE)) : 0;
    dw(r, 0xc) = g.below(50000);
    dw(r, 0x10) = g.below(30000);
    dw(r, 0x14) = pool_block(g, 0);
    if (size > 0x18) {
        dw(r, 0x18) = g.next() % 2 ? 0x5a5a0001 : 0;
        const DWORD n = g.below(0x200);
        dw(r, 0x1c) = g.next() % 3 ? ptrv(alloc(n + 4)) : 0;
        dw(r, 0x20) = dw(r, 0x1c) ? n : 0;
        dw(r, 0x24) = g.next() % 3 ? g.below(dw(r, 0xc) + 1) : g.next() % 2 ? 0 : dw(r, 0xc);
        dw(r, 0x28) = g.next() % 3 ? g.below(dw(r, 0x10) + 1) : g.next() % 2 ? 0 : dw(r, 0x10);
    }
    return r;
}
BYTE *make_samples(Gen &g, DWORD n)
{
    BYTE *b = alloc(n + 4);
    for (DWORD i = 0; i + 1 < n; i += 2) {
        const DWORD k = g.below(10);
        const short v = k == 0 ? (short)32767 : k == 1 ? (short)-32768 : (short)g.next();
        memcpy(b + i, &v, 2);
    }
    return b;
}

enum What {
    W_ARRAY_PTRS, W_ARRAY_OGG, W_ARRAY_FLAGS, W_TAKE, W_TAKE_OGG, W_GIVE, W_CTOR, W_DTOR_BASE, W_DTOR_PLAIN, W_CLOSE_BASE, W_READ,
    W_SEEK, W_FORMAT, W_SECONDS, W_READ_PARTS, W_READ_MIX, W_OPEN_RAW, W_OPEN_WAVE, W_READ_WAVE, W_CLOSE_WAVE, W_DTOR_WAVE,
    W_FOR_FILE, W_OGG_CTOR, NWHAT
};
const char *const WHAT[] = {"StreamBuffers::ArrayCtor", "OggStates::ArrayCtor",      "PoolFlags::ArrayCtor",     "StreamBuffers::Take",
                            "OggStates::Take",          "StreamBuffers::Give",       "CSoundReader::Ctor",       "CSoundReader::DeletingDtor",
                            "CSoundReader::Dtor",       "CSoundReader::Close",       "CSoundReader::Read",       "CSoundReader::Seek",
                            "CSoundReader::Format",     "CSoundReader::Seconds",     "CSoundReader::ReadParts",  "CSoundReader::ReadMix",
                            "CRawSoundReader::Open",    "CWaveSoundReader::Open",    "CWaveSoundReader::Read",   "CWaveSoundReader::Close",
                            "CWaveSoundReader::DeletingDtor", "SoundReader_ForFile", "COggSoundReader::Ctor"};
const DWORD ADDR[] = {0x57ced0, 0x525740, 0x525760, 0x57cd40, 0x5255e0, 0x57cdb0, 0x57d790, 0x57cf40, 0x57d110, 0x57d7b0, 0x57d2f0, 0x57d320,
                      0x57d2a0, 0x57d0e0, 0x57d330, 0x57d550, 0x57c980, 0x57d930, 0x57db50, 0x57dd70, 0x57cb40, 0x57ca10, 0x525410};
const char *const NAMES[] = {"Sounds/a.wav", "Sounds/b.ogg", "Sounds/c", "ogg", "Sounds/D.OGG"};

bool never_out(DWORD a)
{
    for (DWORD n : NEVER_OUT)
        if (n == a)
            return true;
    return false;
}

void run_case(Tally &t, What w, unsigned seed)
{
    Result r{};
    pools_save();
    Gen g0{seed};
    memset(&g_template, 0, sizeof g_template);
    make_fake(F_FILE, K_FILE);
    make_fake(F_FILE2, K_FILE);
    // the pools are constructed in most cases; a few construct them
    const bool fresh_buffers = g0.next() % 16 == 0, fresh_ogg = g0.next() % 16 == 0;
    for (int side = 0; side < 2; side++) {
        Gen g{seed * 31u + 7u};
        side_begin(side, seed, MODULE, (BYTE)(0x5a + seed % 3));
        pools_build(g, fresh_buffers, fresh_ogg);
        g_wave_only = w == W_OPEN_WAVE || w == W_READ_WAVE;
        make_image(g);
        g_reads = 0;
        DWORD a[6] = {0, 0, 0, 0, 0, 0}, ecx = 0, edx = 0;
        int nargs = 0;
        bool float_ret = false;
        switch (w) {
        case W_ARRAY_PTRS:
        case W_ARRAY_OGG:
        case W_ARRAY_FLAGS:
            ecx = ptrv(alloc(0x10));
            memset(at<void *>(ecx), 0x77, 0x10);
            break;
        case W_TAKE:
        case W_GIVE:
            ecx = POOLS[0].base;
            if (w == W_GIVE) {
                a[0] = pool_block(g, 0);
                nargs = 1;
            }
            break;
        case W_TAKE_OGG:
            ecx = POOLS[1].base;
            break;
        case W_CTOR:
            ecx = ptrv(alloc(0x18));
            memset(at<void *>(ecx), 0x44, 0x18);
            break;
        case W_DTOR_BASE:
        case W_DTOR_PLAIN:
        case W_CLOSE_BASE:
            ecx = ptrv(make_reader(g, g.next() % 2 ? VT_RAW : VT_BASE, 0x18));
            a[0] = g.below(2);
            nargs = w == W_DTOR_BASE ? 1 : 0;
            break;
        case W_READ:
        case W_SEEK:
            ecx = ptrv(make_reader(g, VT_RAW, 0x18));
            dw(at<void *>(ecx), 8) = ptrv(fake(F_FILE));
            if (w == W_READ) {
                a[0] = ptrv(alloc(0x200));
                a[1] = g.below(0x200);
                nargs = 2;
            } else {
                a[0] = g.below(g_file.size + 10);
                nargs = 1;
            }
            break;
        case W_FORMAT:
            ecx = ptrv(make_reader(g, VT_RAW, 0x18));
            if (!dw(at<void *>(ecx), 4))
                dw(at<void *>(ecx), 4) = ptrv(make_format(g));
            a[0] = ptrv(alloc(0x14));
            nargs = 1;
            break;
        case W_SECONDS:
            ecx = ptrv(make_reader(g, VT_RAW, 0x18));
            if (!dw(at<void *>(ecx), 4))
                dw(at<void *>(ecx), 4) = ptrv(make_format(g));
            dw(at<void *>(ecx), 0xc) = g.next() % 8 ? g.below(1000000) : g.next();
            float_ret = true;
            break;
        case W_READ_PARTS:
        case W_READ_MIX: {
            BYTE *rd = make_reader(g, ptrv(g_vt[K_READER]), 0x18);
            const DWORD n1 = 2 * g.below(0x100) + g.below(2), n2 = g.next() % 4 ? 2 * g.below(0x100) + g.below(2) : 0;
            ecx = ptrv(rd);
            a[0] = ptrv(make_samples(g, n1));
            a[1] = n1;
            a[2] = g.next() % 5 ? ptrv(make_samples(g, n2)) : 0;
            a[3] = n2;
            a[4] = ff(g, 0.0f, 1.0f);
            if (w == W_READ_MIX && !a[2])
                a[2] = ptrv(make_samples(g, n2));
            nargs = w == W_READ_MIX ? 5 : 4;
            break;
        }
        case W_OPEN_RAW:
        case W_OPEN_WAVE: {
            BYTE *rd = make_reader(g, w == W_OPEN_RAW ? VT_RAW : VT_WAVE, w == W_OPEN_RAW ? 0x18 : 0x2c);
            ecx = ptrv(rd);
            a[0] = ptrv(fake(F_FILE2));
            nargs = 1;
            break;
        }
        case W_READ_WAVE: {
            BYTE *rd = make_reader(g, VT_WAVE, 0x2c);
            dw(rd, 8) = ptrv(fake(F_FILE));
            ecx = ptrv(rd);
            a[0] = ptrv(alloc(0x800));
            a[1] = g.below(0x800);
            nargs = 2;
            break;
        }
        case W_CLOSE_WAVE:
        case W_DTOR_WAVE:
            ecx = ptrv(make_reader(g, VT_WAVE, 0x2c));
            a[0] = g.below(2);
            nargs = w == W_DTOR_WAVE ? 1 : 0;
            break;
        case W_FOR_FILE:
            ecx = ptrv(g_real ? g_real->name : NAMES[g.below(5)]);
            edx = g.below(8);
            break;
        case W_OGG_CTOR:
            ecx = ptrv(alloc(0x20));
            memset(at<void *>(ecx), 0x55, 0x20);
            break;
        default: break;
        }
        const DWORD fn = side == 0 && never_out(ADDR[w]) ? ptrv(original_address(ADDR[w])) : ADDR[w];
        r.float_ret = float_ret;
        r.eax[side] = call_fn(fn, ecx, edx, nargs, a, float_ret, &r.st0[side]);
        // a reader the factory made, through its first Reads (real data)
        if (w == W_FOR_FILE && r.eax[side] && g_real) {
            BYTE *rd = at<BYTE *>(r.eax[side]);
            BYTE *buf = alloc(0x400);
            for (int k = 0; k < 2; k++)
                ev(0x100 + (DWORD)k, vcall<DWORD>(rd, 2, buf, 0x400));
        }
        g_file.pos = 0;
        pools_snapshot();
        side_end();
        pools_restore();
    }
    DWORD mask = 0xffffffff;
    if (w == W_CLOSE_BASE || w == W_CLOSE_WAVE || w == W_DTOR_PLAIN || w == W_GIVE || w == W_FORMAT)
        mask = 0;   // void
    if (w == W_OPEN_RAW || w == W_OPEN_WAVE)
        mask = 0xff;
    compare(t, WHAT[w], seed, r, mask);
}

void test_readers(Tally &t)
{
    const int COUNTS[NWHAT] = {10, 10, 10, 200, 100, 200, 10, 200, 100, 200, 100, 50, 100, 300, 400, 600, 100, 1500, 800, 200, 200, 400, 100};
    for (int w = 0; w < NWHAT; w++) {
        for (int k = 0; k < scaled(COUNTS[w]); k++)
            run_case(t, (What)w, 0x3000u * (unsigned)(w + 1) + (unsigned)k);
    }
}

// ---- Real data ------------------------------------------------------------------------------------------------

// The first 4 KB of up to 24 files of the sound list (real file buffer functions, outside the cases)
int collect_real()
{
    if (!g_reals)
        g_reals = static_cast<Real *>(VirtualAlloc(nullptr, 24 * sizeof(Real), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!g_reals)
        return 0;
    BYTE *list = at<BYTE *>(0x713708);
    const DWORD n = dw(list, 0x14);
    const BYTE *records = at<const BYTE *>(dw(list, 0x34));
    const char *folder = at<const char *(THISCALL *)(const void *)>(0x4e13a0)(list + 0x30);
    if (!n || !records || !readable(records, 0x108 * n) || !folder)
        return 0;
    int found = 0;
    for (DWORD i = 0; i < n && found < 24; i++) {
        if (!spread_pick((int)i, (int)n, 24))
            continue;
        Real &r = g_reals[found];
        const char *name = reinterpret_cast<const char *>(records + 0x108 * i);
        wsprintfA(r.name, "%.120s%s%.120s", folder, folder[0] && folder[lstrlenA(folder) - 1] != '/' ? "/" : "", name);
        void *fb = at<void *(__fastcall *)(DWORD)>(0x417710)(0x1c);
        if (!fb)
            continue;
        at<void *(THISCALL *)(void *)>(0x4a1780)(fb);
        if (at<bool(THISCALL *)(void *, const char *, DWORD)>(0x4a1840)(fb, r.name, 4)) {
            r.size = at<DWORD(THISCALL *)(void *)>(0x4a2190)(fb);
            r.avail = at<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(fb, r.head, sizeof r.head);
            if (r.avail > sizeof r.head)
                r.avail = sizeof r.head;
            found++;
        }
        vcall<void *>(fb, 0, 1);
    }
    return found;
}

}  // namespace

ZAX_CHECKS(sound_reader)
{
    (void)cls;
    (void)n;
    Tally readers{"sound_reader: the readers, the pools and the ring buffer reads in the fake world"},
        real{"sound_reader: the sound list's files through the factory, Open and Read (real data)"};
    g_nreals = collect_real();
    set_kind(K_FILE, FILE_SLOTS, sizeof FILE_SLOTS / sizeof FILE_SLOTS[0]);
    set_kind(K_READER, READER_SLOTS, sizeof READER_SLOTS / sizeof READER_SLOTS[0]);
    if (world_on(STANDINS, 1)) {
        test_readers(readers);
        for (int i = 0; i < g_nreals; i++) {
            g_real = &g_reals[i];
            run_case(real, W_FOR_FILE, 0x80000u + (unsigned)i);
            run_case(real, W_OPEN_WAVE, 0x80100u + (unsigned)i);
        }
        g_real = nullptr;
        world_off();
    } else {
        readers.check(false, "sound_reader", "[the fake world could not be set up]");
    }
    log("  sound_reader real data: %d files of the sound list", g_nreals);
    readers.report();
    real.report();
}

}  // namespace zax
