// Checks of simple_warp.cpp against the original, in the game thread.
//  - The classes CSimpleWarpEffect and the abstract CWarpEffect (checks_inventory_common.h): Create,
//    CreateArray, the deleting destructors, the getters (both copies each), RegisterFields, the registered
//    classes; CWarpEffect's constructor, destructor and deleting destructor and CSimpleWarpEffect's
//    constructor and destructor out of line on pool blocks.
//  - The network blocks (slots 23/24 of both classes) and the reader's registration with recording stand-ins
//    for the packet's functions (bytes written, the block type, the sequence token; reads answering from
//    the case's values): the calls and the fields read.
//  - The seven loops (0x5726c0 … 0x573440), 0x571ff0 and the dispatcher 0x571c50 (the display depth 8 and
//    16 set for the call and put back, the sequence lookup 0x48d8f0 a stand-in) on blocks of the running
//    level's compressed sprite frames (copies; 8- and 16-bit) and on random RLE rows, with a fake sequence
//    of fake texture frames (8 or 16 bits, 1-128 pixels, also not powers of two), random origins, strengths
//    (every threshold and its neighbours) and rates (0, time-based): the copy's bytes, the fade table
//    pointer 0x6b67e8.
//  - Begin, draw and end (slots 28, 29, 30, and CWarpEffect's slots 25/26 through a fake entity) on fake
//    frames of the frame check harness: uncompressed 8-bit frames and copies of compressed level frames
//    whose blocks all walk at the depth (a broken row would never end on either side),
//    a render buffer of the check's own (big enough so neither side moves it; the game's put back after):
//    the frame object, the copy in the render buffer after the draw, the pixels, the frame calls.
//  - The rate (slot 31) on loaded records of the sequence list only (nothing loads), slot 27, CWarpEffect's
//    rate.
#include "checks_frame_r1.h"
#include "checks_hud_common.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_simple_warp(Hook *out);

namespace {

using namespace r1;

framec::ModuleHooks g_hooks{"simple_warp", &hooks_simple_warp, {}, {}, {}, 0, true};

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
float &ff(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
DWORD fn_address(const void *f) { return (DWORD)(uintptr_t)f; }
using Create0 = void *(__cdecl *)();
using Loop = void(__fastcall *)(const BYTE *, const int *, const DWORD *, BYTE *, DWORD, DWORD, void *, float, float);

const invcheck::ClassDef CLASSES[] = {
    {"CSimpleWarpEffect", 0x30, 0x713574, 0x571920, 0x571970, 0x571ad0, {0x5718d0, 0x571a70}, true},
    {"CWarpEffect", 0x10, 0x713c50, 0, 0, 0, {0x5963e0, 0x596430}, false},
};

// ---- A fake sequence of texture frames ----------------------------------------------------------------------

DWORD g_seq_count;
void *g_seq_frames[8];
DWORD THISCALL seq_count(void *) { return g_seq_count; }
void *THISCALL seq_frame(void *, DWORD k) { return k < 8 ? g_seq_frames[k] : nullptr; }
DWORD g_seq_vt[64];
DWORD g_seq[4];
void *seq_object()
{
    // only slots 25 (the count) and 23 (frame k) are called
    g_seq_vt[25] = fn_address((void *)&seq_count);
    g_seq_vt[23] = fn_address((void *)&seq_frame);
    g_seq[0] = fn_address(g_seq_vt);
    return g_seq;
}
void *THISCALL fake_list_object(void *, DWORD) { return g_seq; }

const float EDGES[] = {0.9f, 0.7f, 0.5f, 0.3f, 0.1f, 1.0f, 0.0f};
const DWORD LOOPS8[5] = {0x5726c0, 0x5728e0, 0x572b00, 0x572d20, 0x572f40};
float strength_value()
{
    const DWORD r = rnd(10);
    if (r < 7) {
        const float e = EDGES[r];
        const DWORD d = rnd(3);
        return d == 0 ? e : d == 1 ? e - 0.0001f : e + 0.0001f;
    }
    return (float)rnd(1101) / 1000.0f;
}
float rate_value()
{
    switch (rnd(4)) {
    case 0:
    case 1:
        return 0.0f;
    case 2:
        return (float)rnd(400) / 10.0f;
    default:
        return -(float)rnd(100) / 10.0f;
    }
}

// Random RLE rows (8 or 16 bits) whose runs add up to the width; the row table in `rows`
DWORD make_rle(BYTE *out, DWORD cap, DWORD *rows, DWORD width, DWORD height, DWORD bpp)
{
    DWORD at_ = 0;
    for (DWORD y = 0; y < height; y++) {
        rows[y] = at_;
        DWORD left = width;
        while (left && at_ + 2 + 63 * bpp < cap) {
            const DWORD kind = rnd(3);
            DWORD n = 1 + rnd(kind == 0 ? 127 : 63);
            if (n > left)
                n = left;
            if (kind == 0) {
                out[at_++] = (BYTE)(0x80 | n);
            } else if (kind == 1) {
                out[at_++] = (BYTE)(0x40 | n);
                for (DWORD j = 0; j < n * bpp; j++)
                    out[at_++] = (BYTE)random();
            } else {
                out[at_++] = (BYTE)n;
                for (DWORD j = 0; j < bpp; j++)
                    out[at_++] = (BYTE)random();
            }
            left -= n;
        }
        if (left)
            return 0;   // out of room
    }
    return at_;
}

// Whether RLE rows can be walked as the loops walk them (every run at least one pixel, the runs adding up to
// the width, inside the block): a broken row would make the loops run for ever
bool valid_rle(const BYTE *b, DWORD bytes, const DWORD *rows, DWORD width, DWORD height, DWORD bpp)
{
    for (DWORD r = 0; r < height; r++) {
        DWORD off = rows[r], left = width;
        while (left) {
            if (off >= bytes)
                return false;
            const BYTE c = b[off++];
            const DWORD n = c & 0x80 ? c & 0x7f : c & 0x3f;
            if (!n || n > left)
                return false;
            off += c & 0x80 ? 0 : c & 0x40 ? n * bpp : bpp;
            left -= n;
        }
        if (off > bytes)
            return false;
    }
    return true;
}
// Whether all of a compressed frame's blocks can be walked at the display's depth
bool valid_frame(const FrameData *f, DWORD bpp)
{
    for (int i = 0; i < 5; i++) {
        const BYTE *b = block_data(f, i);
        const DWORD *t = block_table(f, i);
        if (!b)
            continue;
        if (!t || !valid_rle(b, *reinterpret_cast<const DWORD *>(b), t, f->width, f->height, bpp))
            return false;
    }
    return true;
}

// The level's compressed sprite frames (8 and 16 bits)
Sources g_sources;
const FrameData *g_compressed[64];
int g_ncompressed;
void find_compressed()
{
    collect_sources(g_sources, 24);
    g_ncompressed = 0;
    for (int i = 0; i < g_sources.n && g_ncompressed < 64; i++) {
        const FrameData *f = g_sources.list[i].frame;
        if ((f->flags & F_COMPRESSED) && (f->flags & (F_8 | F_16)) && compressed_bytes(f))
            g_compressed[g_ncompressed++] = f;
    }
}

BYTE g_src[0x20000], g_dst[2][0x20000];
DWORD g_rows[0x400];

// The display depth (the reporter's +0x24) set for a call
struct Depth {
    DWORD saved;
    void set(DWORD d)
    {
        saved = dw(bp(*at<DWORD *>(0x6c02cc)), 0x24);
        dw(bp(*at<DWORD *>(0x6c02cc)), 0x24) = d;
    }
    void restore() { dw(bp(*at<DWORD *>(0x6c02cc)), 0x24) = saved; }
};

void check_loops(Tally &t)
{
    seq_object();
    g_seed = 0x5726c0;
    for (int k = 0; k < scaled(500); k++) {
        // 0-4 the 8-bit loops, 5/6 the 16-bit ones, 7 the dispatcher
        const int fn = (int)rnd(8);
        DWORD bpp = fn == 5 || fn == 6 ? 2 : fn == 7 ? 1 + rnd(2) : 1;
        // the source block: a real compressed frame's pixel block or random rows
        DWORD width, height, bytes = 0;
        const FrameData *real = nullptr;
        if (g_ncompressed && rnd(2)) {
            real = g_compressed[rnd((DWORD)g_ncompressed)];
            if ((bpp == 2) != !!(real->flags & F_16))
                real = nullptr;
        }
        if (real) {
            width = real->width;
            height = real->height;
            const DWORD *b = reinterpret_cast<const DWORD *>(real->pixels);
            bytes = b[0];
            if (bytes > sizeof g_src || height > 0x400 || !geo::readable(real->pixels, bytes) || !geo::readable(real->rows, height * 4) ||
                !valid_rle(real->pixels, bytes, real->rows, width, height, bpp))
                real = nullptr;
            else {
                memcpy(g_src, real->pixels, bytes);
                memcpy(g_rows, real->rows, height * 4);
            }
        }
        if (!real) {
            width = 1 + rnd(150);
            height = 1 + rnd(60);
            bytes = make_rle(g_src, sizeof g_src, g_rows, width, height, bpp);
            if (!bytes)
                continue;
        }
        // the textures
        begin_case(random());
        g_seq_count = rnd(8) ? 1 + rnd(4) : 0;
        for (DWORD i = 0; i < 4; i++) {
            FrameSpec s;
            s.format = bpp == 2 ? RGB565 : PAL8;
            s.width = rnd(4) ? 1 << rnd(8) : 1 + rnd(100);
            s.height = rnd(4) ? 1 << rnd(8) : 1 + rnd(100);
            s.recording = false;
            const int fi = make_frame(s);
            g_seq_frames[i] = fi >= 0 ? frame(0, fi) : nullptr;
        }
        const int origin[2] = {rnd_range(-300, 300), rnd_range(-300, 300)};
        const float strength = strength_value(), rate = rate_value();
        const DWORD depth = bpp == 2 ? 16 : 8;
        DWORD fade[2];
        const DWORD index = rnd(4);
        const bool no_src = rnd(20) == 0;
        for (int side = 0; side < 2; side++) {
            memset(g_dst[side], 0xa5, bytes + 64);
            *at<DWORD *>(0x6b67e8) = 0x6347b0;
            if (side == 0)
                g_hooks.out();
            if (fn < 5)
                at<Loop>(LOOPS8[fn])(g_src, origin, g_rows, g_dst[side], width, height, g_seq, strength, rate);
            else if (fn < 7)
                at<Loop>(fn == 5 ? 0x5730f0 : 0x573440)(g_src, origin, g_rows, g_dst[side], width, height, g_seq, strength, rate);
            else {
                BYTE effect[0x30] = {};
                dw(effect, 0x2c) = index;
                Depth d;
                d.set(depth);
                CodePatch p;
                patch_jump(p, 0x48d8f0, (void *)&fake_list_object);
                at<void(THISCALL *)(void *, const BYTE *, const int *, const DWORD *, BYTE *, DWORD, DWORD, float, float)>(0x571c50)(
                    effect, no_src ? nullptr : g_src, origin, g_rows, g_dst[side], width, height, strength, rate);
                unpatch(p);
                d.restore();
            }
            if (side == 0)
                g_hooks.in();
            fade[side] = *at<DWORD *>(0x6b67e8);
        }
        *at<DWORD *>(0x6b67e8) = 0x6347b0;
        char d[128];
        wsprintfA(d, "[loop %d (%s, %d bits), %u x %u, strength %d/1000, case %d: %x / %x]", fn, real ? "real" : "random", bpp * 8, width, height,
                  (int)(strength * 1000), k, fnv(2166136261u, g_dst[0], bytes + 64), fnv(2166136261u, g_dst[1], bytes + 64));
        t.check(g_hooks.fits && !memcmp(g_dst[0], g_dst[1], bytes + 64) && fade[0] == fade[1], "", d);
    }
    // 0x571ff0 alone
    for (int k = 0; k < scaled(60); k++) {
        BYTE row[0x100], out[2][0x120];
        for (int i = 0; i < 0x100; i++)
            row[i] = (BYTE)random();
        const DWORD n = 1 + rnd(0x100), x = random(), mask = (1u << rnd(9)) - 1;
        for (int side = 0; side < 2; side++) {
            memset(out[side], 0x5a, sizeof out[side]);
            if (side == 0)
                g_hooks.out();
            at<void(THISCALL *)(BYTE *, DWORD, const BYTE *, DWORD, DWORD)>(0x571ff0)(out[side], n, row, x, mask & 0xff);
            if (side == 0)
                g_hooks.in();
        }
        t.check(!memcmp(out[0], out[1], sizeof out[0]), "", "[0x571ff0]");
    }
}

// ---- Begin, draw, end on fake frames --------------------------------------------------------------------------

DWORD g_entity[0x20];
void check_frames(Tally &t)
{
    seq_object();
    g_seed = 0x572020;
    // a render buffer of the check's own, big enough for every case, so neither side moves it; the game's
    // (data, capacity, size, in use at 0x713108) put back afterwards
    DWORD saved_buffer[4];
    memcpy(saved_buffer, at<void *>(0x713108), 16);
    memset(at<void *>(0x713108), 0, 16);
    at<void *(__fastcall *)(DWORD)>(0x55e2f0)(0x40000);
    at<void(__cdecl *)()>(0x55e2e0)();
    for (int k = 0; k < scaled(240); k++) {
        begin_case(random());
        int fi = -1;
        const FrameData *real = nullptr;
        if (g_ncompressed && rnd(4)) {
            real = g_compressed[rnd((DWORD)g_ncompressed)];
            if (compressed_bytes(real) > 0x30000 || !(real->flags & (F_8 | F_16)) || !valid_frame(real, real->flags & F_16 ? 2 : 1))
                real = nullptr;
            else
                fi = copy_compressed(real);
        }
        if (fi < 0) {
            real = nullptr;
            FrameSpec s;
            s.format = PAL8;
            s.width = 1 + rnd(120);
            s.height = 1 + rnd(80);
            fi = make_frame(s);
        }
        if (fi < 0)
            continue;
        const bool sixteen = real && (real->flags & F_16);
        for (DWORD i = 0; i < 4; i++) {
            FrameSpec s;
            s.format = sixteen ? RGB565 : PAL8;
            s.width = 1 << rnd(8);
            s.height = 1 << rnd(8);
            s.recording = false;
            const int ti = make_frame(s);
            g_seq_frames[i] = ti >= 0 ? frame(0, ti) : nullptr;
        }
        g_seq_count = 1 + rnd(4);
        BYTE *e = static_cast<BYTE *>(original<Create0>(0x571920)());
        ff(e, 8) = strength_value();
        // the rate (slot 31) through the real list: a loaded record only (nothing loads), else the case is
        // skipped
        const BYTE *records = *at<BYTE **>(0x6c27b8);
        const DWORD count = records ? *at<DWORD *>(0x6c27bc) : 0;
        DWORD loaded = 0xffffffff;
        for (DWORD i = 0, start = count ? rnd(count) : 0; i < count; i++) {
            const DWORD j = (start + i) % count;
            if (!records[j * 0x80 + 0x7f] && dw(const_cast<BYTE *>(records) + j * 0x80, 0x78)) {
                loaded = j;
                break;
            }
        }
        if (loaded == 0xffffffff) {
            vcall<void *>(e, 1, 1);
            continue;
        }
        dw(e, 0x2c) = loaded;
        const bool via_entity = rnd(3) == 0;
        BYTE copy[2][0x30];
        char what[96];
        wsprintfA(what, "begin/draw/end, %s frame %d bits, %s, case %d", real ? "compressed" : "plain", sixteen ? 16 : 8, via_entity ? "entity" : "slots", k);
        hudcheck::run_sides(t, g_hooks, what, [&](int side) -> DWORD {
            memcpy(copy[side], e, 0x30);
            BYTE *fx = copy[side];
            FrameData *f = frame(side, fi);
            Depth d;
            d.set(sixteen ? 16 : 8);
            CodePatch p;
            patch_jump(p, 0x48d8f0, (void *)&fake_list_object);
            unsigned h = 2166136261u;
            if (via_entity) {
                memset(g_entity, 0, sizeof g_entity);
                dw(g_entity, 0x3c) = (DWORD)(uintptr_t)f;   // (a missing frame would crash both sides: slot 28 reads it)
                at<void(THISCALL *)(void *, void *)>(0x5965d0)(fx, g_entity);
            } else {
                vcall<void>(fx, 28, f);
                vcall<void>(fx, 29, f);
            }
            const DWORD n = dw(fx, 0x28);
            if (n && n <= 0x40000)
                h = fnv(h, *at<void **>(0x713108), n);
            h = fnv(h, fx + 0x24, 8);   // on, the copy's bytes (+0x10 … +0x20 are the side's frame blocks)
            if (via_entity)
                at<void(THISCALL *)(void *, void *)>(0x596610)(fx, g_entity);
            else
                vcall<void>(fx, 30, f);
            h = fnv(h, fx + 0x24, 1);
            unpatch(p);
            d.restore();
            return h;
        });
        vcall<void *>(e, 1, 1);
    }
    heap_free(*at<void **>(0x713108));
    memcpy(at<void *>(0x713108), saved_buffer, 16);
}

// ---- The network blocks, the reader's registration -------------------------------------------------------------

DWORD THISCALL rec_write_byte(void *p, DWORD v)
{
    invcheck::ev('8', invcheck::ptrv(p), v & 0xff);
    return invcheck::result(v);
}
DWORD THISCALL rec_block_type(void *p, void *reader, DWORD kind)
{
    invcheck::ev('b', invcheck::ptrv(p), invcheck::ptrv(reader), kind);
    return invcheck::result(1);
}
DWORD THISCALL rec_write_token(void *p, DWORD v, void *table)
{
    invcheck::ev('t', invcheck::ptrv(p), v, invcheck::ptrv(table));
    return invcheck::result(2);
}
BYTE g_reads[8];
int g_nread;
DWORD THISCALL rec_read_byte(void *p, BYTE *out)
{
    *out = g_reads[g_nread++ & 7];
    invcheck::ev('r', invcheck::ptrv(p), *out);
    return invcheck::result(3);
}
DWORD THISCALL rec_read_token(void *p, BYTE *out, void *table)
{
    *out = g_reads[g_nread++ & 7];
    invcheck::ev('k', invcheck::ptrv(p), *out, invcheck::ptrv(table));
    return invcheck::result(4);
}
DWORD THISCALL rec_register(void *p, void *reader, const char *name)
{
    invcheck::ev('g', invcheck::ptrv(p), invcheck::ptrv(reader), fnv(2166136261u, name, (DWORD)lstrlenA(name)));
    return invcheck::result(5);
}
DWORD g_packet[4];

void check_blocks(Tally &t)
{
    g_seed = 0x571bd0;
    for (int k = 0; k < scaled(200); k++) {
        BYTE *e = static_cast<BYTE *>(original<Create0>(0x571920)());
        ff(e, 8) = rnd(5) ? (float)rnd(1001) / 1000.0f : -(float)rnd(10);
        ff(e, 0xc) = (float)rnd(400) / 10.0f;
        dw(e, 0x2c) = rnd(300);
        for (BYTE &b : g_reads)
            b = (BYTE)random();
        const int what = (int)rnd(5);
        const BYTE flag = (BYTE)rnd(2);
        BYTE copy[2][0x30];
        DWORD eax[2];
        for (int side = 0; side < 2; side++) {
            memcpy(copy[side], e, 0x30);
            invcheck::log_reset(side);
            g_nread = 0;
            CodePatch p[6];
            patch_jump(p[0], 0x520820, (void *)&rec_write_byte);
            patch_jump(p[1], 0x51ff50, (void *)&rec_block_type);
            patch_jump(p[2], 0x520c40, (void *)&rec_write_token);
            patch_jump(p[3], 0x520490, (void *)&rec_read_byte);
            patch_jump(p[4], 0x520620, (void *)&rec_read_token);
            patch_jump(p[5], 0x51ff30, (void *)&rec_register);
            invcheck::side_begin(side);
            switch (what) {
            case 0:
                eax[side] = at<DWORD(THISCALL *)(void *, void *, BYTE)>(0x571bd0)(copy[side], g_packet, flag);
                break;
            case 1:
                at<void(THISCALL *)(void *, void *)>(0x571c10)(copy[side], g_packet);
                eax[side] = 0;
                break;
            case 2:
                eax[side] = at<DWORD(THISCALL *)(void *, void *, DWORD)>(0x596500)(copy[side], g_packet, 0);
                break;
            case 3:
                at<void(THISCALL *)(void *, void *)>(0x596550)(copy[side], g_packet);
                eax[side] = 0;
                break;
            default:
                eax[side] = at<DWORD(THISCALL *)(void *)>(0x571b60)(g_packet);
            }
            invcheck::side_end(side);
            for (int i = 5; i >= 0; i--)
                unpatch(p[i]);
        }
        vcall<void *>(e, 1, 1);
        char d[96];
        wsprintfA(d, "[block case %d (%d): eax %x / %x]", k, what, eax[0], eax[1]);
        t.check(invcheck::same_log() && eax[0] == eax[1] && !memcmp(copy[0], copy[1], 0x30), "", d);
    }
}

// ---- Small ones: constructors and destructors out of line, the rates, slot 27 ------------------------------------

void check_small(Tally &t)
{
    g_seed = 0x596490;
    for (int k = 0; k < scaled(60); k++) {
        const int what = (int)rnd(5);
        const float rate = (float)rnd(1000) / 7.0f;
        const DWORD pick = random();
        BYTE b[2][0x30];
        DWORD eax[2];
        for (int side = 0; side < 2; side++) {
            BYTE *o = static_cast<BYTE *>(pool_alloc(0x30));
            memset(o, 0x3c + k, 0x30);
            invcheck::side_begin(side);
            switch (what) {
            case 0:   // CWarpEffect's constructor, then its deleting destructor without freeing
                at<BYTE *(THISCALL *)(void *)>(0x596490)(o);
                eax[side] = invcheck::ptrv(at<BYTE *(THISCALL *)(void *, DWORD)>(0x5964b0)(o, 0)) - invcheck::ptrv(o);
                break;
            case 1:
                at<BYTE *(THISCALL *)(void *)>(0x596490)(o);
                at<void(THISCALL *)(void *)>(0x5964e0)(o);
                eax[side] = 0;
                break;
            case 2:
                eax[side] = invcheck::ptrv(at<BYTE *(THISCALL *)(void *)>(0x571b70)(o)) - invcheck::ptrv(o);
                at<void(THISCALL *)(void *)>(0x571bb0)(o);
                break;
            case 3: {
                at<BYTE *(THISCALL *)(void *)>(0x571b70)(o);
                ff(o, 0xc) = rate;
                const float r = (float)at<long double(THISCALL *)(void *)>(0x5965c0)(o);
                at<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x596650)(o, 1, 2, 3, 4, 5, 6);
                memcpy(&eax[side], &r, 4);
                at<void(THISCALL *)(void *)>(0x571bb0)(o);
                break;
            }
            default: {   // the rate on loaded records only
                at<BYTE *(THISCALL *)(void *)>(0x571b70)(o);
                const BYTE *records = *at<BYTE **>(0x6c27b8);
                const DWORD count = records ? *at<DWORD *>(0x6c27bc) : 0;
                DWORD i = count ? pick % count : 0;
                if (records && (records[i * 0x80 + 0x7f] || !dw(const_cast<BYTE *>(records) + i * 0x80, 0x78)))
                    i = 0;
                dw(o, 0x2c) = i;
                float r = -1;
                if (!records || (!records[i * 0x80 + 0x7f] && dw(const_cast<BYTE *>(records) + i * 0x80, 0x78)))
                    r = (float)at<long double(THISCALL *)(void *)>(0x572570)(o);
                memcpy(&eax[side], &r, 4);
                at<void(THISCALL *)(void *)>(0x571bb0)(o);
            }
            }
            invcheck::side_end(side);
            memcpy(b[side], o, 0x30);
            pool_free(o, 0x30);
        }
        char d[64];
        wsprintfA(d, "[small case %d (%d): %x / %x]", k, what, eax[0], eax[1]);
        t.check(eax[0] == eax[1] && !memcmp(b[0], b[1], 0x30), "", d);
    }
}

}  // namespace

ZAX_CHECKS(simple_warp)
{
    (void)cls;
    (void)n;
    read_frame_trace();
    invcheck::g_hook_table = &hooks_simple_warp;
    if (!invcheck::arena_setup()) {
        log("Check simple warp: no arena");
        return;
    }
    Tally objects{"simple warp: Create, CreateArray, destructors"}, getters{"simple warp: class getters"}, fields{"simple warp: RegisterFields"},
        registered{"simple warp: registered descriptors and classes (real data)"};
    invcheck::test_classes(objects, getters, fields, registered, CLASSES, 2);
    objects.report();
    getters.report();
    fields.report();
    registered.report();
    find_compressed();
    Tally loops{"simple warp: the bending loops and 0x571c50"}, frames{"simple warp: begin, draw, end"}, blocks{"simple warp: network blocks"},
        small{"simple warp: constructors, destructors, rates, slot 27"};
    check_loops(loops);
    check_frames(frames);
    check_blocks(blocks);
    check_small(small);
    log("  simple warp: %d compressed level frames", g_ncompressed);
    loops.report();
    frames.report();
    blocks.report();
    small.report();
}

}  // namespace zax
