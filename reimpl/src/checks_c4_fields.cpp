// Checks of batch C4's field kind slots, unit converters and resource callbacks (c4_fields.cpp) against
// the original, in the game thread (machinery: checks_inventory_common.h; side 0 with the module's hooks
// out, side 1 through the reimplementation, the C heap from each side's arena).
//  - Converters: objects of our own with the real vtables, values with NaN, infinities, zero, denormals
//    and random bits; the 80-bit result on the FPU stack (or eax) compared; the deleting destructors
//    with flag 0 and 1 on arena blocks.
//  - Binary and text slots on the registered descriptors (real data; copies, the setters cleared) of
//    every kind: values written into memory streams (bytes, eax relative to the file buffer) and read
//    back from what the original wrote, from random bytes and from short or empty streams (the object's
//    bytes, strings by their text, eax, the position).
//  - The dynamic properties' slots with the list functions, the extra list and the descriptors as
//    recording stand-ins (fake descriptors whose slots record their calls); the polygon's text and array
//    slots with the real Polygon_Insert; the resource lists' slot 47 on the real lists with the vtable
//    and the records swapped for recording ones; the embedded value's slots with Object_ReadFromStream,
//    Object_WriteBinary and Object_ReadBinary recorded; the array slots and small slots on fakes.
//  - 0x54e200 and 0x54e210 write to address 0 by design: not called; the replacement's first bytes are
//    compared with the original's store (c7 05 00 00 00 00 00 00 00 00).
#include "checks_inventory_common.h"
#include "file_buffer.h"
#include "init_list.h"
#include "memory_stream.h"
#include "x87.h"

namespace zax {

int hooks_c4_fields(Hook *out);

namespace {

using namespace invcheck;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }
DWORD text_of(const char *t) { return t ? fnv_text(2166136261u, t) : 0x0bad; }

// ---- File buffers on memory streams (as checks_resource_fields.cpp) -------------------------------------

// Each side's buffer at the same offset 0x20 of its own 256-byte slot, so both have the same low address byte:
// a read slot's `mov al, 1` over a pointer to the buffer (rel_file) must not depend on where the linker puts
// it. With the buffers side by side (0x1c apart) side 1's happened to start on a 256-byte boundary in one
// build (2026-10-31, run 20261031-220037-a912: "Ambient Sound Index" int read eax f2000001 / f1000001, 666 of
// 1374 cases), so its al = 1 form looked like "the buffer + 1" there only.
struct alignas(0x100) FileSlot {
    BYTE before[0x20];
    CFileBuf file;
};
FileSlot g_file_slot[2];
void fb_ctor(CFileBuf *f) { at<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(f); }
void fb_dtor(CFileBuf *f) { at<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(f); }
DWORD fb_tell(CFileBuf *f) { return at<DWORD(THISCALL *)(CFileBuf *)>(0x4a2180)(f); }
// A file buffer reading a copy of p (flags 4) or writing into an empty memory stream (flags 3); the
// streams come from the game's heap and pool, not the arena (made before arena_on)
bool *g_open_flag(CFileBuf *f);
void open_memory(CFileBuf *f, const void *p, DWORD n)
{
    *g_open_flag(f) = true;
    fb_ctor(f);
    BYTE *copy = (BYTE *)heap_alloc(n ? n : 1);
    memcpy(copy, p, n);
    void *m = pool_alloc(sizeof(MemoryStream));
    at<MemoryStream *(THISCALL *)(void *, BYTE *, DWORD)>(0x505fb0)(m, copy, n);
    at<DWORD(THISCALL *)(CFileBuf *, ZStream *, DWORD)>(0x4a1970)(f, (ZStream *)m, 4);
}
void open_output(CFileBuf *f)
{
    *g_open_flag(f) = true;
    fb_ctor(f);
    void *m = pool_alloc(sizeof(MemoryStream));
    at<MemoryStream *(THISCALL *)(void *)>(0x505f00)(m);
    at<DWORD(THISCALL *)(CFileBuf *, ZStream *, DWORD)>(0x4a1970)(f, (ZStream *)m, 3);
}
DWORD output_hash(CFileBuf *f)
{
    auto *m = (MemoryStream *)f->stream;
    if (!m)
        return 0;
    return fnv(fnv(2166136261u, &m->size, 4), m->data ? m->data : (const BYTE *)"", m->data ? m->size : 0);
}
// The bytes a stream wrote (copied out, at most n)
DWORD output_bytes(CFileBuf *f, BYTE *out, DWORD n)
{
    auto *m = (MemoryStream *)f->stream;
    if (!m || !m->data)
        return 0;
    const DWORD k = m->size < n ? m->size : n;
    memcpy(out, m->data, k);
    return k;
}
// eax relative to the side's file buffer (the write functions return it)
DWORD rel_file(DWORD v, int side)
{
    const DWORD f = ptrv(&g_file_slot[side].file);
    if (v - f < sizeof(CFileBuf))
        return 0xf1000000u + (v - f);
    // a read slot's `mov al, 1` over a pointer into the file buffer: the pointer's upper bytes, relative to the
    // buffer's (when the buffer's own low byte was above 1 this fell through, depending on the DLL's layout)
    if ((v & ~0xffu) - (f & ~0xffu) < sizeof(CFileBuf))
        return 0xf2000000u + ((v & ~0xffu) - (f & ~0xffu)) + (v & 0xff);
    return rel(v, side);
}

// ---- Hooks of the module, the two sides ------------------------------------------------------------------

// A file buffer opened for the case is closed while the arena is still on (a growing memory stream's
// buffer can be an arena block): its output hash, bytes and position are kept per side
bool g_open[2];
DWORD g_out_hash[2], g_pos[2], g_nout[2];
BYTE g_out[2][256];
void begin(int side, BYTE pattern)
{
    side_begin(side);
    log_reset(side);
    arena_on(side, pattern);
}
void end(int side)
{
    if (g_open[side]) {
        g_out_hash[side] = output_hash(&g_file_slot[side].file);
        g_nout[side] = output_bytes(&g_file_slot[side].file, g_out[side], 256);
        g_pos[side] = fb_tell(&g_file_slot[side].file);
        fb_dtor(&g_file_slot[side].file);
        g_open[side] = false;
    }
    arena_off();
    side_end(side);
}
bool *g_open_flag(CFileBuf *f) { return &g_open[f == &g_file_slot[1].file]; }

// ---- Converters --------------------------------------------------------------------------------------

struct Ld {
    BYTE b[10];
};
Ld ld_bits(long double v)
{
    Ld r;
    memcpy(r.b, &v, 10);
    return r;
}
float pick_float(unsigned k)
{
    static const DWORD SPECIAL[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x7f800000, 0xff800000, 0x7fc00000, 0x7f800001, 0x00000001,
                                    0x007fffff, 0x7f7fffff, 0x40490fdb, 0x3eaaaaab, 0x41200000, 0xc2c80000, 0x4b800001};
    const DWORD u = k < 16 ? SPECIAL[k] : (k & 1 ? random() : (random() & 0x3fffffff) + 0x30000000);
    return raw_float(u);
}

void test_converters(Tally &t)
{
    struct Conv {
        const char *name;
        DWORD vtable, slot, kind;   // kind 0 float in st0, 1 dword in eax, 2 double arg
    };
    const Conv CONV[] = {{"scale to", 0x5f46a8, 1, 0},     {"scale from", 0x5f46a8, 2, 0},  {"offset to", 0x5f46c0, 1, 1},
                         {"offset from", 0x5f46c0, 2, 1},  {"reciprocal", 0x5f46d8, 1, 0},  {"reciprocal 2", 0x5f46d8, 2, 0},
                         {"square to", 0x5f46e4, 1, 0},    {"square from", 0x5f46e4, 2, 0}, {"inverse to", 0x5f46f0, 1, 0},
                         {"inverse from", 0x5f46f0, 2, 0}, {"int identity", 0x5fd434, 1, 1}, {"float identity", 0x5fdd64, 1, 0},
                         {"double identity", 0x5fdd70, 1, 2}};
    for (const Conv &c : CONV) {
        const int cases = scaled(300);
        for (int k = 0; k < cases; k++) {
            BYTE obj[16];
            D(obj, 0) = c.vtable;
            const float kf = pick_float(random() % 24);
            memcpy(obj + 4, &kf, 4);
            if (c.vtable == 0x5f46f0) {   // the embedded converter: a scale one
                D(obj, 4) = 0x5f46a8;
                memcpy(obj + 8, &kf, 4);
            }
            if (c.vtable == 0x5f46c0)
                D(obj, 4) = random();
            const float v = pick_float((unsigned)k);
            const DWORD vi = random();
            const double vd = (double)pick_float((unsigned)k) * 3.25;
            Ld r[2];
            DWORD e[2];
            const DWORD fn = *at<DWORD *>(c.vtable + c.slot * 4);
            for (int side = 0; side < 2; side++) {
                begin(side, 0x5c);
                memset(&r[side], 0, sizeof r[side]);
                e[side] = 0;
                if (c.kind == 0)
                    r[side] = ld_bits(at<long double(THISCALL *)(void *, float)>(fn)(obj, v));
                else if (c.kind == 1)
                    e[side] = at<DWORD(THISCALL *)(void *, DWORD)>(fn)(obj, vi);
                else
                    r[side] = ld_bits(at<long double(THISCALL *)(void *, double)>(fn)(obj, vd));
                end(side);
            }
            char d[120];
            wsprintfA(d, "[%s case %d: %x / %x]", c.name, k, *(DWORD *)r[0].b ^ e[0], *(DWORD *)r[1].b ^ e[1]);
            t.check(memcmp(&r[0], &r[1], 10) == 0 && e[0] == e[1], "converter", d);
        }
    }
    // the deleting destructors on arena blocks, flags 0 to 3
    const DWORD DTORS[][2] = {{0x5f46f0, 0x4a7ec0}, {0x5fd434, 0x54d570}, {0x5f46c0, 0x54d7d0}, {0x5fd660, 0x54dd60},
                              {0x5f46a8, 0x54fb40}, {0x5fdd70, 0x54fb60}};
    for (const auto &d0 : DTORS)
        for (DWORD flags = 0; flags < 4; flags++) {
            BYTE copy[2][16];
            DWORD e[2];
            for (int side = 0; side < 2; side++) {
                begin(side, 0x3d);
                BYTE *o = static_cast<BYTE *>(heap_alloc(16));
                D(o, 0) = d0[0];
                D(o, 4) = 0x5f46a8;
                D(o, 8) = 0x40000000;
                e[side] = rel(at<DWORD(THISCALL *)(void *, DWORD)>(d0[1])(o, flags), side);
                memcpy(copy[side], o, 16);
                end(side);
            }
            char d[80];
            wsprintfA(d, "[dtor %x flags %u: %x / %x]", d0[1], flags, e[0], e[1]);
            t.check(e[0] == e[1] && memcmp(copy[0], copy[1], 16) == 0, "converter dtor", d);
        }
}

// ---- Registered descriptors of a slot ---------------------------------------------------------------------

constexpr int MAXD = 48;
CFieldDesc *g_found[MAXD];
// Up to cap registered descriptors whose vtable has fn at the slot (every descriptor once; a spread over
// the classes)
int collect(CClassInfo **cls, int n, int slot, DWORD fn, int cap)
{
    static CFieldDesc *all[2048];
    int na = 0;
    for (int i = 0; i < n; i++) {
        if (!cls[i] || !cls[i]->fields)
            continue;
        for (DWORD j = 0; j < cls[i]->num_fields && na < 2048; j++) {
            CFieldDesc *f = cls[i]->fields[j];
            if (!f || !readable(f, 0x30) || *at<DWORD *>(*reinterpret_cast<DWORD *>(f) + slot * 4) != fn)
                continue;
            bool seen = false;
            for (int k = 0; k < na && !seen; k++)
                seen = all[k] == f;
            if (!seen)
                all[na++] = f;
        }
    }
    int m = 0;
    for (int i = 0; i < na && m < MAXD; i++)
        if (spread_pick(i, na, cap))
            g_found[m++] = all[i];
    return m;
}

// The descriptor copied (0x60 bytes) with its setter cleared
BYTE g_desc[0x60];
void copy_desc(const CFieldDesc *f, DWORD setter_at)
{
    memcpy(g_desc, f, sizeof g_desc);
    if (setter_at)
        D(g_desc, setter_at) = 0;
}

// The test object of a side: 0x3000 bytes (the biggest value offset of a registered field is below)
constexpr DWORD OBJ = 0x3000;
bool g_trace = false;  // progress lines while the module is new
BYTE g_o[2][OBJ];
void fill_object(int side, unsigned seed)
{
    g_seed = seed;
    for (DWORD i = 0; i < OBJ; i += 4)
        D(g_o[side], i) = random();
}

// ---- Plain binary values: kinds whose value is bytes at the offset ------------------------------------------

struct PlainKind {
    const char *name;
    DWORD write, read;   // the two slots (26, 29)
    DWORD size;          // bytes of the value
    DWORD setter_at;
};
const PlainKind PLAIN[] = {
    {"colour", 0x46dfb0, 0x46dfe0, 0x10, 0x38}, {"point", 0x4a8180, 0x4a81a0, 8, 0x30},    {"int", 0x54dc50, 0x54ddd0, 4, 0},
    {"byte", 0x54df70, 0x54df90, 1, 0x28},      {"bit", 0x54e190, 0, 4, 0},                {"float", 0x54f000, 0x54f020, 4, 0},
    {"double", 0x54f320, 0x54f340, 8, 0},
};

void test_plain(Tally &t, CClassInfo **cls, int n)
{
    for (const PlainKind &k : PLAIN) {
        const int nd = collect(cls, n, 26, k.write, 12);
        for (int di = 0; di < nd; di++) {
            copy_desc(g_found[di], k.setter_at);
            const DWORD off = D(g_desc, 0x10);
            if (off + 0x10 > OBJ)
                continue;
            if (g_trace)
                log("  plain %s %s +%x vt %x", k.name, g_found[di]->name.c_str(), off, D(g_desc, 0));
            for (int c = 0; c < scaled(6); c++) {
                const unsigned seed = 7000u * (unsigned)di + (unsigned)c;
                // write
                DWORD h[2], e[2];
                BYTE written[16] = {};
                DWORD nw = 0;
                for (int side = 0; side < 2; side++) {
                    fill_object(side, seed);
                    open_output(&g_file_slot[side].file);
                    begin(side, 0x11);
                    e[side] = rel_file(at<DWORD(THISCALL *)(void *, void *, void *)>(k.write)(g_desc, g_o[side], &g_file_slot[side].file), side);
                    end(side);
                    h[side] = g_out_hash[side];
                    if (side == 0)
                        nw = g_nout[0] < 16 ? g_nout[0] : 16, memcpy(written, g_out[0], nw);
                }
                char d[140];
                wsprintfA(d, "[%s write, field %s +%x, seed %u: eax %x / %x]", k.name, g_found[di]->name.c_str(), off, seed, e[0], e[1]);
                t.check(h[0] == h[1] && e[0] == e[1] && same_region(g_o[0], g_o[1], OBJ), "plain write", d);
                if (!k.read)
                    continue;
                // read: what was written, random bytes, a short stream
                for (int v = 0; v < 3; v++) {
                    BYTE in[16];
                    DWORD nin = nw;
                    memcpy(in, written, 16);
                    if (v == 1)
                        for (DWORD i = 0; i < 16; i++)
                            in[i] = (BYTE)random();
                    if (v == 2)   // an empty stream (the colour: 0-2 bytes)
                        nin = k.read == 0x46dfe0 ? seed % 3 : 0;
                    DWORD pos[2];
                    for (int side = 0; side < 2; side++) {
                        fill_object(side, seed + 1);
                        open_memory(&g_file_slot[side].file, in, nin);
                        begin(side, 0x12);
                        // the colour read through zax_call_stack's fill (batch decomp-faithful2): the bytes a short
                        // stream leaves unread are the fill's at the exe's [esp + 0..2], the same on both sides
                        const DWORD args[2] = {(DWORD)(uintptr_t)g_o[side], (DWORD)(uintptr_t)&g_file_slot[side].file};
                        const DWORD r = k.read == 0x46dfe0 ? zax_call_filled(at<void *>(k.read), (DWORD)(uintptr_t)g_desc, args, 2, seed * 3u + 0x46dfe0u)
                                                           : at<DWORD(THISCALL *)(void *, void *, void *)>(k.read)(g_desc, g_o[side], &g_file_slot[side].file);
                        // the colour read returns FieldColor_SetRGBA's eax (the value's flags dword +0xc, a
                        // byte of the random fill, with al 1), never a pointer: compared as it is (made relative,
                        // a fill dword that happened to lie in one side's arena differed, runs
                        // 20261029-011734-ae65 and -111902-5d32, "MiniMap Color")
                        e[side] = k.read == 0x46dfe0 ? r : rel_file(r, side);
                        end(side);
                        pos[side] = g_pos[side];
                    }
                    // the region first, so the message names its difference (not the previous case's)
                    g_diff_at = g_diff_a = g_diff_b = 0;
                    const bool same = same_region(g_o[0], g_o[1], OBJ);
                    wsprintfA(d, "[%s read %d, field %s +%x, seed %u: eax %x / %x, +%x %x / %x]", k.name, v, g_found[di]->name.c_str(), off, seed, e[0],
                              e[1], g_diff_at, g_diff_a, g_diff_b);
                    t.check(e[0] == e[1] && pos[0] == pos[1] && same, "plain read", d);
                }
            }
        }
    }
}

// ---- ZString, flags and C string kinds ------------------------------------------------------------------------

const char *const TEXTS[] = {"", "a", "Zax", "Target Name", "$Trigger", "Hello World 1234567890 abcdefghijklmnopqrstuvwxyz", "x y"};
void zs_ctor_text(void *z, const char *s) { at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(z, s); }
void zs_dtor(void *z) { at<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }
const char *zs_text(void *z) { return (*reinterpret_cast<ZStringBuffer **>(z))->text; }

void test_strings(Tally &t, CClassInfo **cls, int n)
{
    // the ZString kind's write and read (+0x28 as registered: the read translates)
    const int nd = collect(cls, n, 26, 0x4160d0, 16);
    for (int di = 0; di < nd; di++) {
        copy_desc(g_found[di], 0x30);
        const DWORD off = D(g_desc, 0x10);
        if (off + 8 > OBJ)
            continue;
        for (DWORD ti = 0; ti < 7; ti++) {
            DWORD h[2], e[2];
            BYTE written[80] = {};
            DWORD nw = 0;
            for (int side = 0; side < 2; side++) {
                fill_object(side, ti);
                zs_ctor_text(g_o[side] + off, TEXTS[ti]);
                open_output(&g_file_slot[side].file);
                begin(side, 0x21);
                e[side] = rel_file(at<DWORD(THISCALL *)(void *, void *, void *)>(0x4160d0)(g_desc, g_o[side], &g_file_slot[side].file), side);
                end(side);
                h[side] = g_out_hash[side];
                if (side == 0)
                    nw = g_nout[0] < 80 ? g_nout[0] : 80, memcpy(written, g_out[0], nw);
                zs_dtor(g_o[side] + off);
            }
            char d[160];
            wsprintfA(d, "[string write %s +%x text %u: %x / %x]", g_found[di]->name.c_str(), off, ti, e[0], e[1]);
            t.check(h[0] == h[1] && e[0] == e[1], "string write", d);
            // read back; a length beyond the rest; zero length; an empty stream
            for (int v = 0; v < 4; v++) {
                BYTE in[80];
                DWORD nin = nw;
                memcpy(in, written, 80);
                if (v == 1 && nw >= 4)
                    D(in, 0) += 3;
                if (v == 2) {
                    D(in, 0) = 0;
                    nin = 4;
                }
                if (v == 3)
                    nin = 0;
                DWORD pos[2], err[2], text[2];
                for (int side = 0; side < 2; side++) {
                    fill_object(side, ti + 50);
                    zs_ctor_text(g_o[side] + off, "old text");
                    if (g_desc[0x28])   // the translation goes to the ZString behind the value
                        zs_ctor_text(g_o[side] + off + 4, "");
                    open_memory(&g_file_slot[side].file, in, nin);
                    *at<int *(__cdecl *)()>(0x5d1014)() = 0x16;
                    begin(side, 0x22);
                    const DWORD r = at<DWORD(THISCALL *)(void *, void *, void *)>(0x416110)(g_desc, g_o[side], &g_file_slot[side].file);
                    end(side);
                    e[side] = (BYTE)r;   // the upper bytes: the errno pointer or the translation's eax
                    err[side] = *at<int *(__cdecl *)()>(0x5d1014)();
                    pos[side] = g_pos[side];
                    text[side] = text_of(zs_text(g_o[side] + off)) ^ (g_desc[0x28] ? text_of(zs_text(g_o[side] + off + 4)) : 0);
                    zs_dtor(g_o[side] + off);
                    if (g_desc[0x28])
                        zs_dtor(g_o[side] + off + 4);
                }
                wsprintfA(d, "[string read %s text %u v %d: al %x / %x, errno %x / %x]", g_found[di]->name.c_str(), ti, v, e[0], e[1], err[0], err[1]);
                t.check(e[0] == e[1] && err[0] == err[1] && pos[0] == pos[1] && text[0] == text[1], "string read", d);
            }
        }
    }
    // slots 47/48 on a descriptor copy with its default (+0x2c) a ZString of ours
    for (DWORD ti = 0; ti < 7; ti++)
        for (DWORD fn : {0x415f30u, 0x415f40u}) {
            DWORD text[2], e[2];
            BYTE src[4];
            zs_ctor_text(src, TEXTS[ti]);
            for (int side = 0; side < 2; side++) {
                BYTE desc[0x34] = {};
                D(desc, 0) = 0x5ec91c;
                zs_ctor_text(desc + 0x2c, "default");
                begin(side, 0x23);
                const DWORD r = fn == 0x415f30 ? at<DWORD(THISCALL *)(void *, const char *)>(fn)(desc, TEXTS[ti])
                                               : at<DWORD(THISCALL *)(void *, void *)>(fn)(desc, src);
                end(side);
                e[side] = r == ptrv(desc + 0x2c);
                text[side] = text_of(zs_text(desc + 0x2c));
                zs_dtor(desc + 0x2c);
            }
            zs_dtor(src);
            char d[80];
            wsprintfA(d, "[default %x text %u: %x / %x]", fn, ti, e[0], e[1]);
            t.check(e[0] == e[1] && text[0] == text[1], "string default", d);
        }
    // the flags kind: write (slot 16's text) and read (slot 19) on registered descriptors, setter cleared
    const int nf = collect(cls, n, 26, 0x415030, 16);
    for (int di = 0; di < nf; di++) {
        copy_desc(g_found[di], 0x30);
        const DWORD off = D(g_desc, 0x10);
        if (off + 8 > OBJ)
            continue;
        for (int c = 0; c < scaled(8); c++) {
            const unsigned seed = 900u * (unsigned)di + (unsigned)c;
            DWORD h[2], e[2];
            BYTE written[200] = {};
            DWORD nw = 0;
            DWORD names = 0;
            if (void *list = at<void *>(D(g_desc, 0x2c)))
                names = vcall<DWORD>(list, 22);
            const DWORD mask = names >= 16 ? 0xffff : (1u << names) - 1;
            for (int side = 0; side < 2; side++) {
                fill_object(side, seed);
                D(g_o[side], off) = (D(g_o[side], off) & ~0xffffu) | (D(g_o[side], off) & mask & (c & 1 ? 0x5555u : 0xffffu));
                open_output(&g_file_slot[side].file);
                begin(side, 0x24);
                e[side] = at<DWORD(THISCALL *)(void *, void *, void *)>(0x415030)(g_desc, g_o[side], &g_file_slot[side].file);
                end(side);
                h[side] = g_out_hash[side];
                if (side == 0)
                    nw = g_nout[0] < 200 ? g_nout[0] : 200, memcpy(written, g_out[0], nw);
            }
            char d[160];
            wsprintfA(d, "[flags write %s seed %u]", g_found[di]->name.c_str(), seed);
            t.check(h[0] == h[1], "flags write", d);
            DWORD pos[2];
            for (int side = 0; side < 2; side++) {
                fill_object(side, seed + 3);
                open_memory(&g_file_slot[side].file, written, nw);
                begin(side, 0x25);
                e[side] = (BYTE)at<DWORD(THISCALL *)(void *, void *, void *)>(0x415070)(g_desc, g_o[side], &g_file_slot[side].file);
                end(side);
                pos[side] = g_pos[side];
            }
            wsprintfA(d, "[flags read %s seed %u: +%x %x / %x]", g_found[di]->name.c_str(), seed, g_diff_at, g_diff_a, g_diff_b);
            t.check(e[0] == e[1] && pos[0] == pos[1] && same_region(g_o[0], g_o[1], OBJ), "flags read", d);
        }
    }
    // the mask
    {
        DWORD e[2];
        for (int side = 0; side < 2; side++) {
            begin(side, 0);
            e[side] = at<DWORD(THISCALL *)(void *)>(0x414dd0)(g_desc);
            end(side);
        }
        t.check(e[0] == e[1], "flags mask");
    }
    // the C string kind: write and read (texts in the arena, old texts too), the default
    const int nt = collect(cls, n, 26, 0x54d3f0, 12);
    for (int di = 0; di < nt; di++) {
        copy_desc(g_found[di], 0x2c);
        const DWORD off = D(g_desc, 0x10);
        if (off + 8 > OBJ)
            continue;
        for (DWORD ti = 0; ti < 8; ti++) {
            DWORD h[2], e[2];
            BYTE written[80] = {};
            DWORD nw = 0;
            for (int side = 0; side < 2; side++) {
                fill_object(side, ti);
                open_output(&g_file_slot[side].file);
                begin(side, 0x26);
                char *s = nullptr;
                if (ti < 7) {
                    s = static_cast<char *>(heap_alloc(80));
                    lstrcpyA(s, TEXTS[ti]);
                }
                D(g_o[side], off) = ptrv(s);
                e[side] = rel_file(at<DWORD(THISCALL *)(void *, void *, void *)>(0x54d3f0)(g_desc, g_o[side], &g_file_slot[side].file), side);
                end(side);
                h[side] = g_out_hash[side];
                if (side == 0)
                    nw = g_nout[0] < 80 ? g_nout[0] : 80, memcpy(written, g_out[0], nw);
            }
            char d[160];
            wsprintfA(d, "[text write %s text %u: %x / %x]", g_found[di]->name.c_str(), ti, e[0], e[1]);
            t.check(h[0] == h[1] && e[0] == e[1], "text write", d);
            DWORD pos[2], text[2];
            for (int side = 0; side < 2; side++) {
                fill_object(side, ti + 9);
                open_memory(&g_file_slot[side].file, written, ti == 7 ? 0 : nw);
                begin(side, 0x27);
                char *old = static_cast<char *>(heap_alloc(16));
                lstrcpyA(old, "old");
                D(g_o[side], off) = ti & 1 ? 0 : ptrv(old);
                e[side] = rel_file(at<DWORD(THISCALL *)(void *, void *, void *)>(0x54d420)(g_desc, g_o[side], &g_file_slot[side].file), side);
                const char *now = at<const char *>(D(g_o[side], off));
                text[side] = now ? text_of(now) : 0;
                end(side);
                pos[side] = g_pos[side];
            }
            wsprintfA(d, "[text read %s text %u: %x / %x, text %x / %x]", g_found[di]->name.c_str(), ti, e[0], e[1], text[0], text[1]);
            t.check(e[0] == e[1] && pos[0] == pos[1] && text[0] == text[1], "text read", d);
        }
    }
    for (DWORD v : {0u, 0x12345678u, 0x62e558u}) {
        BYTE desc[2][0x30] = {};
        DWORD e[2];
        for (int side = 0; side < 2; side++) {
            begin(side, 0);
            e[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x54d1c0)(desc[side], v);
            end(side);
        }
        t.check(e[0] == e[1] && memcmp(desc[0], desc[1], 0x30) == 0, "text default");
    }
}

// ---- Arrays and small slots ------------------------------------------------------------------------------------

DWORD g_rec_vtable[64];
BYTE g_class[0x50];
DWORD THISCALL rec_slot1(void *self, DWORD flags)
{
    ev(0x101, rel(ptrv(self), g_side), flags);
    return result(1);
}
DWORD THISCALL rec_slot7(void *self)
{
    ev(0x107, rel(ptrv(self), g_side) & 0xfff);
    return result(7);
}
DWORD orel(const void *p)
{
    const DWORD v = ptrv(p), b = ptrv(g_o[g_side]);
    return v - b < OBJ ? 0x0b000000u + (v - b) : rel(v, g_side);
}
DWORD THISCALL rec_slot19(void *self, void *object, const char *text, DWORD stream)
{
    ev(0x119, ptrv(self) == ptrv(g_desc), orel(object), text_of(text) ^ stream);
    return result(0x19);
}
DWORD THISCALL rec_slot28(void *self, DWORD n, DWORD a, DWORD b)
{
    ev(0x128, ptrv(self) == ptrv(g_class), n, a ^ b);
    return result(0x28);
}
DWORD THISCALL rec_slot33(void *) { ev(0x133); return 12; }

void test_arrays(Tally &t)
{
    for (DWORD i = 0; i < 64; i++)
        g_rec_vtable[i] = ptrv((void *)&rec_slot1);
    g_rec_vtable[1] = ptrv((void *)&rec_slot1);
    g_rec_vtable[7] = ptrv((void *)&rec_slot7);
    g_rec_vtable[19] = ptrv((void *)&rec_slot19);
    g_rec_vtable[28] = ptrv((void *)&rec_slot28);
    g_rec_vtable[33] = ptrv((void *)&rec_slot33);
    // new arrays (operator new from the arena; the colour's constructor on every element)
    for (DWORD fn : {0x46e040u, 0x524070u, 0x54dfb0u, 0x54f040u, 0x54f360u})
        for (DWORD count : {0u, 1u, 2u, 5u, 17u}) {
            DWORD e[2];
            for (int side = 0; side < 2; side++) {
                begin(side, 0x31);
                e[side] = rel(at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(fn)(g_desc, count, 0, 0xffffffff), side);
                end(side);
            }
            char d[80];
            wsprintfA(d, "[new array %x count %u: %x / %x]", fn, count, e[0], e[1]);
            t.check(e[0] == e[1] && same_region(g_arena[0], g_arena[1], g_used[0] + 0x100), "new array", d);
        }
    // delete arrays: operator delete (the arena ignores it), the embedded kind's slot 1 (3)
    for (DWORD fn : {0x54e2b0u, 0x54f380u, 0x54e870u})
        for (int k = 0; k < 2; k++) {
            DWORD e[2];
            for (int side = 0; side < 2; side++) {
                begin(side, 0x32);
                BYTE *p = static_cast<BYTE *>(heap_alloc(16));
                D(p, 0) = ptrv(g_rec_vtable);
                e[side] = rel(at<DWORD(THISCALL *)(void *, void *)>(fn)(g_desc, k ? p : nullptr), side);
                end(side);
            }
            const bool cmp_eax = k || fn != 0x54e870;   // without an object: the caller's eax
            t.check((!cmp_eax || e[0] == e[1]) && same_log(), "delete array");
        }
    // move: slot 33 of the descriptor (a recorder giving 12) bytes
    {
        BYTE buf[2][32];
        DWORD e[2];
        for (int side = 0; side < 2; side++) {
            BYTE desc[0x40] = {};
            D(desc, 0) = ptrv(g_rec_vtable);
            for (int i = 0; i < 32; i++)
                buf[side][i] = (BYTE)i;
            begin(side, 0);
            e[side] = at<DWORD(THISCALL *)(void *, void *, void *)>(0x524090)(desc, buf[side] + 4, buf[side] + 8) - ptrv(buf[side]);
            end(side);
        }
        t.check(e[0] == e[1] && memcmp(buf[0], buf[1], 32) == 0 && same_log(), "move");
    }
    // slots 33 and 47 of the new[] array, the typed resource's list, the callbacks
    for (int k = 0; k < scaled(40); k++) {
        BYTE desc[2][0x60];
        DWORD e[2][6];
        const unsigned seed = (unsigned)k * 31u;
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            for (DWORD i = 0; i < 0x60; i += 4)
                D(desc[side], i) = random();
            D(desc[side], 0) = ptrv(g_rec_vtable);
            memcpy(g_desc, desc[side], 0x60);
            begin(side, 0);
            e[side][0] = at<DWORD(THISCALL *)(void *)>(0x417a10)(g_desc);
            e[side][1] = at<DWORD(THISCALL *)(void *, void *, const char *)>(0x4179f0)(g_desc, g_o[side], TEXTS[k % 7]);
            e[side][2] = at<DWORD(THISCALL *)(void *, void *)>(0x54e600)(g_desc, g_o[side]);
            e[side][3] = at<DWORD(THISCALL *)(void *, DWORD)>(0x41d7d0)(g_desc, seed);
            e[side][4] = D(g_desc, 8) ^ D(g_desc, 0xc);
            e[side][5] = 0;
            end(side);
            memcpy(desc[side], g_desc, 0x60);
        }
        t.check(memcmp(e[0], e[1], sizeof e[0]) == 0 && memcmp(desc[0], desc[1], 0x60) == 0 && same_log(), "small slots");
    }
    // 0x48da40: the tail jump to slot 93 (a recorder at slot 93 of a vtable of our own)
    {
        static DWORD vt[100];
        for (DWORD i = 0; i < 100; i++)
            vt[i] = ptrv((void *)&rec_slot1);
        DWORD obj[2] = {ptrv(vt), 0};
        DWORD e[2];
        for (int side = 0; side < 2; side++) {
            begin(side, 0);
            e[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x48da40)(obj, 0x55);
            end(side);
        }
        t.check(e[0] == e[1] && same_log(), "resource callback 0x48da40");
    }
    // the embedded value: slots 32 and 38 on recording objects; 19/26/29 with the object functions recorded
    {
        BYTE desc[0x34] = {};
        D(desc, 0x10) = 0x20;
        D(desc, 0x28) = ptrv(g_class);
        D(g_class, 0) = ptrv(g_rec_vtable);
        for (int side = 0; side < 2; side++)
            D(g_o[side], 0x20) = ptrv(g_rec_vtable);
        DWORD e[2][2];
        for (int side = 0; side < 2; side++) {
            begin(side, 0);
            e[side][0] = at<DWORD(THISCALL *)(void *, void *)>(0x54e830)(desc, g_o[side]);
            e[side][1] = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x54e850)(desc, 3, 0, 0xffffffff);
            end(side);
        }
        t.check(memcmp(e[0], e[1], sizeof e[0]) == 0 && same_log(), "embedded slots 32/38");
    }
    // the null array slots: the replacement stores to address 0 as the original does
    for (DWORD a : {0x54e200u, 0x54e210u}) {
        static const BYTE STORE[] = {0xc7, 0x05, 0, 0, 0, 0, 0, 0, 0, 0};
        const bool orig = memcmp(original_address(a), STORE, 10) == 0;
        // the replacement: the hook's jump target (e9 rel32 at the site)
        const BYTE *site = at<const BYTE *>(a);
        bool repl = false;
        if (site[0] == 0xe9) {
            const BYTE *target = site + 5 + *reinterpret_cast<const INT32 *>(site + 1);
            for (int i = 0; i < 16 && !repl; i++)
                repl = memcmp(target + i, STORE, 10) == 0;
        }
        t.check(orig && repl, "null array slot (bytes)");
    }
}

// ---- The embedded value's object functions, recorded --------------------------------------------------------

CodePatch g_p[16];
int g_np;
void stand_in(DWORD a, void *f) { patch_jump(g_p[g_np++], a, f); }
void stand_ins_off()
{
    while (g_np)
        unpatch(g_p[--g_np]);
}
DWORD THISCALL rec_obj_read_stream(void *self, void *file) { ev(0x201, rel(ptrv(self), g_side) & 0xfff, rel_file(ptrv(file), g_side)); return result(1); }
DWORD THISCALL rec_obj_write_bin(void *self, void *file) { ev(0x202, rel(ptrv(self), g_side) & 0xfff, rel_file(ptrv(file), g_side)); return result(2); }
DWORD THISCALL rec_obj_read_bin(void *self, void *file) { ev(0x203, rel(ptrv(self), g_side) & 0xfff, rel_file(ptrv(file), g_side)); return result(0); }

void test_embedded(Tally &t)
{
    BYTE desc[0x34] = {};
    for (DWORD off : {0u, 0x24u, 0x400u}) {
        D(desc, 0x10) = off;
        DWORD e[2][3];
        for (int side = 0; side < 2; side++) {
            begin(side, 0);
            stand_in(0x416af0, (void *)&rec_obj_read_stream);
            stand_in(0x416f50, (void *)&rec_obj_write_bin);
            stand_in(0x416fe0, (void *)&rec_obj_read_bin);
            e[side][0] = at<DWORD(THISCALL *)(void *, void *, const char *, void *)>(0x54e7b0)(desc, g_o[side], "x", &g_file_slot[side].file);
            e[side][1] = at<DWORD(THISCALL *)(void *, void *, void *)>(0x54e7f0)(desc, g_o[side], &g_file_slot[side].file);
            e[side][2] = at<DWORD(THISCALL *)(void *, void *, void *)>(0x54e810)(desc, g_o[side], &g_file_slot[side].file);
            stand_ins_off();
            end(side);
        }
        t.check(memcmp(e[0], e[1], sizeof e[0]) == 0 && same_log(), "embedded 19/26/29");
    }
}

// ---- Dynamic properties with the list functions recorded ------------------------------------------------------

// Fake list {vtable, count}; fake descriptors with a recording vtable; names known to the fake extra list
const char *const PROP_NAMES[] = {"Alpha", "Beta Prop", "Gamma"};
struct FakeDesc {
    DWORD vftable;
    DWORD id;
};
FakeDesc g_props[3];
DWORD g_prop_vt[64];
BYTE g_list_block[2][0x10];
DWORD g_list_count;
DWORD lrel(void *p)
{
    const DWORD v = ptrv(p), b = ptrv(g_list_block[g_side]);
    return v - b < 0x10 ? 0x1b000000u + (v - b) : v;
}
DWORD g_entries[2][8];
DWORD g_read_fail_at;   // slot 29 fails for this property id (0: never)
DWORD THISCALL p_count(void *l) { ev(0x301, lrel(l)); return g_list_count; }
void *THISCALL p_desc_at(void *, DWORD i) { ev(0x302, i); return &g_props[i % 3]; }
void *THISCALL p_value_at(void *, DWORD i) { ev(0x303, i); return at<void *>(0x7700 + i * 0x10); }
DWORD THISCALL p_dtor(void *l) { ev(0x304, lrel(l)); return result(4); }
void *THISCALL p_get_or_add(void *, void *o, FakeDesc *d) { ev(0x305, rel(ptrv(o), g_side) & 0xfff, d->id); return at<void *>(0x8800 + d->id); }
void *THISCALL p_add(void *, void *o, FakeDesc *d) { ev(0x306, rel(ptrv(o), g_side) & 0xfff, d->id); return at<void *>(0x9900 + d->id); }
void *THISCALL p_extra(void *o) { ev(0x308, rel(ptrv(o), g_side) & 0xfff); return at<void *>(0xe7e7); }
void *THISCALL p_find(void *, const char *name)
{
    ev(0x309, text_of(name));
    for (int i = 0; i < 3; i++)
        if (lstrcmpA(name, PROP_NAMES[i]) == 0)
            return &g_props[i];
    return nullptr;
}
const char *THISCALL p_name(FakeDesc *d) { ev(0x30a, d->id); return PROP_NAMES[d->id % 3]; }
void *__fastcall p_pool_alloc(DWORD n) { ev(0x30b, n); return g_list_block[g_side]; }
void __fastcall p_pool_free(void *p, DWORD n) { ev(0x30c, lrel(p), n); }
DWORD THISCALL pd_from_text(FakeDesc *d, void *v, const char *text, void *f) { ev(0x311, d->id, ptrv(v), text_of(text) ^ rel_file(ptrv(f), g_side)); return result(0x19); }
DWORD THISCALL pd_write(FakeDesc *d, void *v, void *f)
{
    ev(0x312, d->id, ptrv(v), rel_file(ptrv(f), g_side));
    at<DWORD(THISCALL *)(void *, DWORD)>(0x4a21c0)(f, 0xabc000 + d->id);
    return result(0x26);
}
DWORD THISCALL pd_read(FakeDesc *d, void *v, void *f)
{
    ev(0x313, d->id, ptrv(v), rel_file(ptrv(f), g_side));
    DWORD x = 0;
    at<DWORD(THISCALL *)(void *, DWORD *)>(0x4a2270)(f, &x);
    return result(d->id + 1 == g_read_fail_at ? 0 : 1);
}
DWORD THISCALL obj_slot11(void *o, const char *name, const char *value, void *f)
{
    ev(0x314, rel(ptrv(o), g_side) & 0xfff, text_of(name), text_of(value) ^ rel_file(ptrv(f), g_side));
    return result(0x11);
}

void dyn_stand_ins(bool pool)
{
    stand_in(0x482de0, (void *)&p_count);
    stand_in(0x482df0, (void *)&p_desc_at);
    stand_in(0x482e00, (void *)&p_value_at);
    stand_in(0x4828b0, (void *)&p_dtor);
    stand_in(0x482970, (void *)&p_get_or_add);
    stand_in(0x482a70, (void *)&p_add);
    stand_in(0x417220, (void *)&p_extra);
    stand_in(0x483760, (void *)&p_find);
    stand_in(0x5007a0, (void *)&p_name);
    if (pool) {   // only where no ZString is made (their buffers come from the pool too)
        stand_in(0x417710, (void *)&p_pool_alloc);
        stand_in(0x54d130, (void *)&p_pool_free);
    }
}

void test_dynamic(Tally &t)
{
    for (DWORD i = 0; i < 64; i++)
        g_prop_vt[i] = ptrv((void *)&rec_slot1);
    g_prop_vt[19] = ptrv((void *)&pd_from_text);
    g_prop_vt[26] = ptrv((void *)&pd_write);
    g_prop_vt[29] = ptrv((void *)&pd_read);
    g_prop_vt[32] = ptrv((void *)&rec_slot1);   // ForEach (real) passes the value
    for (DWORD i = 0; i < 3; i++)
        g_props[i] = {ptrv(g_prop_vt), i};
    static DWORD obj_vt[64];
    for (DWORD i = 0; i < 64; i++)
        obj_vt[i] = ptrv((void *)&rec_slot1);
    obj_vt[11] = ptrv((void *)&obj_slot11);
    BYTE desc[0x28] = {};
    D(desc, 0) = 0x5f17ec;
    D(desc, 0x10) = 0x40;
    // ToText, WriteBinary, slot 32 with lists of 0-4 entries and none
    for (DWORD count = 0; count < 5; count++)
        for (int has = 0; has < 2; has++) {
            g_list_count = count;
            DWORD e[2][3], h[2], text[2];
            for (int side = 0; side < 2; side++) {
                fill_object(side, count);
                D(g_o[side], 0) = ptrv(obj_vt);
                memset(g_list_block[side], 0, 0x10);
                for (DWORD i = 0; i < 4; i++) {
                    g_entries[side][i * 2] = ptrv(&g_props[i % 3]);
                    g_entries[side][i * 2 + 1] = 0x7700 + i;
                }
                D(g_list_block[side], 4) = ptrv(g_entries[side]);
                D(g_list_block[side], 8) = count;
                D(g_o[side], 0x40) = has ? ptrv(g_list_block[side]) : 0;
                open_output(&g_file_slot[side].file);
                BYTE z[4];
                at<void *(THISCALL *)(void *)>(0x4dec90)(z);
                begin(side, 0x41);
                dyn_stand_ins(false);
                e[side][0] = at<DWORD(THISCALL *)(void *, void *, void *)>(0x483020)(desc, g_o[side], z) == ptrv(z);
                e[side][1] = rel_file(at<DWORD(THISCALL *)(void *, void *, void *)>(0x483310)(desc, g_o[side], &g_file_slot[side].file), side);
                e[side][2] = at<DWORD(THISCALL *)(void *, void *)>(0x4834a0)(desc, g_o[side]);
                stand_ins_off();
                end(side);
                text[side] = text_of(zs_text(z));
                zs_dtor(z);
                h[side] = g_out_hash[side];
            }
            char d[120];
            wsprintfA(d, "[dynamic count %u list %d: %x %x %x / %x %x %x]", count, has, e[0][0], e[0][1], e[0][2], e[1][0], e[1][1], e[1][2]);
            // ToText's eax: Format's or SetText's (not compared); the text, the bytes written, the log
            t.check(e[0][1] == e[1][1] && e[0][2] == e[1][2] && text[0] == text[1] && h[0] == h[1] && same_log(), "dynamic write", d);
        }
    // FromText: count texts and blocks of lines
    static const char *const BLOCKS[] = {
        "{\r\nAlpha=1\r\nGamma=two words\r\n}\r\n",   "{\r\n  Beta Prop = 3\r\nUnknown=7\r\n\r\nno equals\r\n}\r\nAlpha=9\r\n",
        "{\r\nAlpha=1\r\n",                           "",
        "{\r\n}\r\n",                                 "{\r\nUnknown=\r\n=x\r\nGamma=\r\n}",
    };
    static const char *const COUNTS[] = {"0", "2", "-1", "x", "5"};
    for (const char *cnt : COUNTS)
        for (const char *block : BLOCKS)
            for (int has = 0; has < 2; has++) {
                DWORD e[2], pos[2];
                for (int side = 0; side < 2; side++) {
                    fill_object(side, 5);
                    D(g_o[side], 0) = ptrv(obj_vt);
                    D(g_o[side], 0x40) = has ? 0x4444 : 0;
                    memset(g_list_block[side], 0, 0x10);
                    open_memory(&g_file_slot[side].file, block, (DWORD)lstrlenA(block));
                    begin(side, 0x42);
                    dyn_stand_ins(true);
                    e[side] = rel(at<DWORD(THISCALL *)(void *, void *, const char *, void *)>(0x483070)(desc, g_o[side], cnt, &g_file_slot[side].file), side);
                    stand_ins_off();
                    end(side);
                    pos[side] = g_pos[side];
                }
                // eax: a pointer into the line buffer on the stack (by its low byte) or a recorder's
                const bool same_eax = e[0] == e[1] || ((e[0] & 0xff) == (e[1] & 0xff) && e[0] > 0x10000 && e[1] > 0x10000);
                char d[120];
                wsprintfA(d, "[dynamic from text %s / %u: %x / %x]", cnt, (DWORD)lstrlenA(block), e[0], e[1]);
                DWORD made[2];
                for (int side = 0; side < 2; side++)
                    made[side] = D(g_o[side], 0x40) == ptrv(g_list_block[side]) ? 0x1b000000u : D(g_o[side], 0x40);
                t.check(same_eax && pos[0] == pos[1] && made[0] == made[1] && memcmp(g_list_block[0], g_list_block[1], 0x10) == 0 && same_log(),
                        "dynamic from text", d);
            }
    // ReadBinary: count, names, values (the stand-in reads a dword), a failing value, an unknown name
    for (DWORD count = 0; count < 5; count++)
        for (DWORD fail = 0; fail < 4; fail++)
            for (int unknown = 0; unknown < 2; unknown++) {
                BYTE in[256];
                DWORD nin = 0;
                D(in, 0) = count;
                nin = 4;
                for (DWORD i = 0; i < count; i++) {
                    const char *name = unknown && i == 1 ? "Nope" : PROP_NAMES[i % 3];
                    D(in, nin) = (DWORD)lstrlenA(name);
                    memcpy(in + nin + 4, name, (DWORD)lstrlenA(name));
                    nin += 4 + (DWORD)lstrlenA(name);
                    D(in, nin) = i;
                    nin += 4;
                }
                g_read_fail_at = fail;
                DWORD e[2], pos[2];
                for (int side = 0; side < 2; side++) {
                    fill_object(side, 6);
                    D(g_o[side], 0) = ptrv(obj_vt);
                    D(g_o[side], 0x40) = count & 1 ? 0x4444 : 0;
                    open_memory(&g_file_slot[side].file, in, nin);
                    begin(side, 0x43);
                    dyn_stand_ins(true);
                    e[side] = at<DWORD(THISCALL *)(void *, void *, void *)>(0x4833a0)(desc, g_o[side], &g_file_slot[side].file);
                    stand_ins_off();
                    end(side);
                    pos[side] = g_pos[side];
                }
                g_read_fail_at = 0;
                char d[120];
                wsprintfA(d, "[dynamic read count %u fail %u unknown %d: %x / %x]", count, fail, unknown, e[0], e[1]);
                t.check(e[0] == e[1] && pos[0] == pos[1] && same_log(), "dynamic read", d);
            }
}

// ---- Polygons ------------------------------------------------------------------------------------------------

void poly_insert(BYTE *p, float x, float y) { at<DWORD(THISCALL *)(void *, float, float, DWORD)>(0x5466c0)(p, x, y, 0xffffffff); }
// A polygon's state: count, points, rectangle, flags, mean (the point and edge blocks by their contents)
DWORD poly_hash(BYTE *p)
{
    DWORD h = fnv(2166136261u, p + 8, 0x20);
    const DWORD n = D(p, 8);
    if (n && n < 1000) {
        if (D(p, 0))
            h = fnv(h, at<void *>(D(p, 0)), n * 8);
        if (D(p, 4) && n > 1)
            h = fnv(h, at<void *>(D(p, 4)), (n - 1) * 12);
    }
    return h ^ (D(p, 0) ? 1 : 0) ^ (D(p, 4) ? 2 : 0);
}

void test_polygons(Tally &t, CClassInfo **cls, int n)
{
    static const char *const TEXTS_P[] = {"", "1, 2", "1, 2, 3, 4, 5, 6", "1e1, 2", "1, 2, 3", "0.5, -7.25, 100, 200, 100, 0, 0.5, -7.25",
                                          "x, 1", "3,4,5,6", "1, 2 e", "   1  ,  2  ,  3  ,  4", "1, 2,, 3, 4"};
    const int nd = collect(cls, n, 19, 0x5480e0, 4);
    for (int di = 0; di < nd || di == 0; di++) {
        if (nd)
            copy_desc(g_found[di], 0);
        else
            D(g_desc, 0) = 0x5fd2a0;
        D(g_desc, 0x10) = 0x80;
        for (DWORD def = 0; def < 2; def++)
            for (const char *text : TEXTS_P)
                for (int pre = 0; pre < 3; pre++) {
                    D(g_desc, 0x28) = def ? 0x12345 : 0;
                    DWORD e[2], h[2];
                    for (int side = 0; side < 2; side++) {
                        fill_object(side, 77);
                        memset(g_o[side] + 0x80, 0, 0x28);
                        begin(side, 0x51);
                        for (int k = 0; k < pre; k++)
                            poly_insert(g_o[side] + 0x80, 10.0f * (float)k, 3.0f);
                        e[side] = rel(at<DWORD(THISCALL *)(void *, void *, const char *, void *)>(0x5480e0)(g_desc, g_o[side], text, nullptr), side);
                        if (e[side] - ptrv(text) <= (DWORD)lstrlenA(text))
                            e[side] = 0xc0000000u + (e[side] - ptrv(text));
                        h[side] = poly_hash(g_o[side] + 0x80);
                        end(side);
                    }
                    char d[140];
                    wsprintfA(d, "[polygon from text '%s' default %u pre %d: %x / %x]", text, def, pre, e[0], e[1]);
                    t.check(e[0] == e[1] && h[0] == h[1] && same_region(g_o[0] + 0x80, g_o[1] + 0x80, 0x28), "polygon from text", d);
                }
        if (!nd)
            break;
    }
    // WriteBinary and ReadBinary (Polygon_Read) round trip, a damaged header, an empty stream
    BYTE desc[0x30] = {};
    D(desc, 0) = 0x5fd2a0;
    D(desc, 0x10) = 0x100;
    for (int pts = 0; pts < 7; pts++)
        for (int v = 0; v < 3; v++) {
            DWORD h[2], e[2];
            BYTE written[128] = {};
            DWORD nw = 0;
            for (int side = 0; side < 2; side++) {
                fill_object(side, 99);
                memset(g_o[side] + 0x100, 0, 0x28);
                open_output(&g_file_slot[side].file);
                begin(side, 0x52);
                for (int k = 0; k < pts; k++)
                    poly_insert(g_o[side] + 0x100, (float)(k * k), (float)(7 - k));
                e[side] = rel_file(at<DWORD(THISCALL *)(void *, void *, void *)>(0x5482a0)(desc, g_o[side], &g_file_slot[side].file), side);
                end(side);
                h[side] = g_out_hash[side];
                if (side == 0)
                    nw = g_nout[0] < 128 ? g_nout[0] : 128, memcpy(written, g_out[0], nw);
            }
            t.check(e[0] == e[1] && h[0] == h[1], "polygon write");
            if (v == 1)
                written[3] ^= 0x20;
            DWORD pos[2], ph[2];
            for (int side = 0; side < 2; side++) {
                fill_object(side, 98);
                memset(g_o[side] + 0x100, 0, 0x28);
                open_memory(&g_file_slot[side].file, written, v == 2 ? 0 : nw);
                begin(side, 0x53);
                e[side] = (BYTE)at<DWORD(THISCALL *)(void *, void *, void *)>(0x548310)(desc, g_o[side], &g_file_slot[side].file);
                ph[side] = poly_hash(g_o[side] + 0x100);
                end(side);
                pos[side] = g_pos[side];
            }
            t.check(e[0] == e[1] && pos[0] == pos[1] && ph[0] == ph[1], "polygon read");
        }
    // DeleteArray: arrays of 0-4 polygons with and without points
    for (DWORD count = 0; count < 5; count++)
        for (int with_points = 0; with_points < 2; with_points++) {
            DWORD e[2];
            BYTE copy[2][4 + 4 * 0x28];
            for (int side = 0; side < 2; side++) {
                begin(side, 0x54);
                BYTE *block = static_cast<BYTE *>(heap_alloc(4 + count * 0x28));
                D(block, 0) = count;
                for (DWORD i = 0; i < count; i++) {
                    BYTE *p = block + 4 + i * 0x28;
                    memset(p, 0, 0x28);
                    if (with_points)
                        for (DWORD k = 0; k <= i; k++)
                            poly_insert(p, (float)k, 1.0f);
                }
                e[side] = rel(at<DWORD(THISCALL *)(void *, void *)>(0x548390)(desc, block + 4), side);
                memcpy(copy[side], block, 4 + count * 0x28);
                end(side);
            }
            t.check(e[0] == e[1] && memcmp(copy[0], copy[1], 4 + count * 0x28) == 0, "polygon delete array");
        }
}

// ---- The resource lists' slot 47 on the real definition lists with fake records ------------------------------

DWORD g_fake_list_vt[64];
BYTE g_records[2][8 * 0x80];
const char *THISCALL fl_name(void *, DWORD i)
{
    ev(0x401, i);
    static const char *const N[] = {"Models/Crater", "Plain", "A/B/C", "/root", "trail/"};
    return N[i % 5];
}
DWORD THISCALL fl_load(void *, const char *folder, const char *name, DWORD *slot)
{
    ev(0x437, text_of(folder), text_of(name), ptrv(slot) - ptrv(g_records[g_side]));
    return 0x10000u + g_nlog[g_side];
}

void test_def_lists(Tally &t)
{
    for (DWORD i = 0; i < 64; i++)
        g_fake_list_vt[i] = ptrv((void *)&rec_slot1);
    g_fake_list_vt[1] = ptrv((void *)&fl_name);
    g_fake_list_vt[37] = ptrv((void *)&fl_load);
    struct L {
        DWORD list, fn, index_at;
    };
    const L LISTS[] = {{0x6cfdd8, 0x518360, 0x34}, {0x62e7e0, 0x41dc20, 0}};
    for (const L &l : LISTS)
        for (int k = 0; k < scaled(60); k++) {
            const unsigned seed = (unsigned)k * 13u + l.list;
            DWORD e[2], h[2];
            for (int side = 0; side < 2; side++) {
                g_seed = seed;
                const DWORD count = rnd(6);
                for (DWORD i = 0; i < 8; i++) {
                    BYTE *r = g_records[side] + i * 0x80;
                    memset(r, 0, 0x80);
                    D(r, 0x78) = rnd(3) == 0 ? 0x5550 + i : 0;
                    r[0x7f] = rnd(5) == 0;
                }
                const DWORD index = rnd(9);
                SavedBytes saved;
                save_bytes(saved, l.list, 0x140);
                D(at<void *>(l.list), 0) = ptrv(g_fake_list_vt);
                D(at<void *>(l.list), 0x30) = rnd(8) == 0 ? 0 : ptrv(g_records[side]);
                D(at<void *>(l.list), 0x34) = count;
                lstrcpyA(at<char *>(l.list + 0x38), rnd(2) ? "Cache/Models" : "");
                BYTE desc[0x38] = {};
                D(desc, 0x34) = 0x10;
                fill_object(side, seed);
                if (l.index_at == 0)
                    D(g_o[side], 8) = index;
                else
                    D(g_o[side], 0x10) = index;
                begin(side, 0);
                e[side] = at<DWORD(THISCALL *)(void *, void *)>(l.fn)(desc, g_o[side]);
                end(side);
                restore_bytes(saved);
                h[side] = fnv(2166136261u, g_records[side], sizeof g_records[side]);
            }
            char d[100];
            wsprintfA(d, "[list %x seed %u: %x / %x]", l.list, seed, e[0], e[1]);
            t.check(e[0] == e[1] && h[0] == h[1] && same_log(), "resource list", d);
        }
    // 0x51d8b0: the error reporter 0x6c02cc swapped
    for (int k = 0; k < 3; k++) {
        BYTE reporter[0x280] = {};
        D(reporter, 0x274) = k == 2 ? 0x77 : 0;
        const DWORD saved = *at<DWORD *>(0x6c02cc);
        DWORD e[2];
        for (int side = 0; side < 2; side++) {
            *at<DWORD *>(0x6c02cc) = k == 0 ? 0 : ptrv(reporter);
            begin(side, 0);
            e[side] = at<DWORD(THISCALL *)(void *, void *)>(0x51d8b0)(g_desc, g_o[side]);
            end(side);
            *at<DWORD *>(0x6c02cc) = saved;
        }
        t.check(e[0] == e[1], "resource list 0x51d8b0");
    }
}

}  // namespace

ZAX_CHECKS(c4_fields)
{
    g_hook_table = &hooks_c4_fields;
    if (!arena_setup()) {
        log("Check c4 fields: no arena");
        return;
    }
    Tally conv{"c4 fields: unit converters"};
    test_converters(conv);
    conv.report();
    Tally plain{"c4 fields: binary slots of the plain kinds (real data)"};
    test_plain(plain, cls, n);
    plain.report();
    Tally strings{"c4 fields: string, flags and C string slots (real data)"};
    test_strings(strings, cls, n);
    strings.report();
    Tally arrays{"c4 fields: array and small slots, embedded values"};
    test_arrays(arrays);
    test_embedded(arrays);
    arrays.report();
    Tally dyn{"c4 fields: dynamic properties"};
    test_dynamic(dyn);
    dyn.report();
    Tally poly{"c4 fields: polygon slots"};
    test_polygons(poly, cls, n);
    poly.report();
    Tally lists{"c4 fields: resource lists' slot 47"};
    test_def_lists(lists);
    lists.report();
}

}  // namespace zax
