// Checks of the start point types' converting wrappers (particle_start.cpp, ZAX_HOOKS(particle_convert))
// against the original, in the game thread.
//  - Describe (slot 1) of the twenty real global wrappers (0x6d0188 …, vtables 0x5fc048-0x5fc12c) and of
//    fakes with a text, on objects of all seven particle description classes, a CParticleInfo and a
//    CLengthEnvelope, with and without the class output: eax (a text pointer of the exe or the class's
//    short name) and the class written.
//  - Convert (slot 2) of the real and fake wrappers (one field name for both sides: in both classes, in one
//    only, none) on fresh objects of each source type with random envelope values (both sides their own source,
//    which the call deletes): the new object's bytes (vtables, envelopes, the YX ratio; the byte fields'
//    dwords masked to their byte: the field copy writes a byte, the rest is the pool block's).
//  - 0x5317b0 (the length envelope's constructor out of line) on pool blocks, 0x52b5d0 (the disk's YX
//    ratio, slot 26) on random values.
#include "checks_inventory_common.h"
#include "ai_common.h"

namespace zax {

int hooks_particle_convert(Hook *out);

namespace {

using namespace ai;
using invcheck::ev;
using invcheck::ptrv;
using Create0 = void *(__cdecl *)();

// The wrappers (static initialisers 0x52a220-0x52a855) in the order of their vtables 0x5fc048 …
constexpr DWORD WRAPPERS[20] = {0x6d02f8, 0x6d0188, 0x6d0478, 0x6d0410, 0x6d0230, 0x6d04c0, 0x6d02e0, 0x6d0490, 0x6d0320, 0x6d04a8,
                                0x6d01a0, 0x6d0428, 0x6d0258, 0x6d0218, 0x6d02a0, 0x6d01d8, 0x6d0380, 0x6d0270, 0x6d03f8, 0x6d03e0};
struct Kind {
    const char *name;
    DWORD size, create;
};
// the five concrete types, then CParticleInfo, a CLengthEnvelope (0x49f4d0's Create)
const Kind KINDS[] = {{"Point", 8, 0x52b2d0},      {"Disk", 0x58, 0x52b600},     {"Line", 0x2c, 0x52c140}, {"Cylinder", 0x7c, 0x52c8f0},
                      {"Sphere", 0x58, 0x52d6c0}, {"ParticleInfo", 0x24, 0x52a8c0}};
constexpr int NSTART = 5, NKINDS = sizeof KINDS / sizeof KINDS[0];
// Field names for fake wrappers: in both disk and sphere/cylinder, in disk and line under other names, none
const DWORD FIELD_NAMES[] = {0x61da64 /* "Inside Radius" */, 0x61da54 /* "Outside Radius" */, 0x61daa4 /* "Center Length" */,
                             0x60b340 /* "YX Ratio" */, 0x61dad8 /* "Height of Cylinder" */, 0};

DWORD rnd(DWORD n) { return random() % n; }

BYTE g_fake[0x14];
BYTE *wrapper(DWORD &text_out)
{
    if (rnd(4)) {
        text_out = 0;
        return at<BYTE *>(WRAPPERS[rnd(20)]);
    }
    memcpy(g_fake, at<void *>(WRAPPERS[rnd(20)]), 0x14);
    dw(g_fake, 8) = rnd(2) ? ptrv("A fake converter") : 0;
    // the same name on both sides (a field copied into a field of another kind would be garbage)
    dw(g_fake, 0xc) = rnd(3) ? FIELD_NAMES[rnd(6)] : 0;
    dw(g_fake, 0x10) = dw(g_fake, 0xc);
    text_out = dw(g_fake, 8);
    return g_fake;
}
float rnd_float()
{
    switch (rnd(6)) {
    case 0:
        return 0.0f;
    case 1:
        return -(float)rnd(100);
    default:
        return (float)rnd(20000) / 100.0f;
    }
}
// Random values in an object's envelopes (the floats +8 … +0x18 and the byte +0x1c of each; +0x20, the
// list index "Ending Behavior", stays) and the disk's +0x50/+0x54
void randomize(BYTE *o, DWORD size)
{
    for (DWORD e = 8; e + 0x24 <= size && e <= 0x58; e += 0x24) {
        if (e == 0x50)
            break;
        for (DWORD off = 8; off < 0x1c; off += 4)
            fl<float>(o, e + off) = rnd(5) ? rnd_float() : 0.0f;
        o[e + 0x1c] = (BYTE)rnd(2);   // "Using Start" (u8)
    }
    if (size >= 0x58) {
        o[0x50] = (BYTE)rnd(2);
        fl<float>(o, 0x54) = rnd_float();
    }
}

// The byte fields (the envelopes' "Using Start" +0x1c, the disk's +0x50): the field copy writes their byte,
// the rest of the dword is the pool block's
bool byte_field(DWORD off) { return off == 0x24 || off == 0x48 || off == 0x74 || off == 0x50; }

BYTE g_bytes[2][0x80];

DWORD vtable_slot(BYTE *w, int slot) { return *reinterpret_cast<DWORD *>(dw(w, 0) + 4 * slot); }

void check_describe(Tally &t)
{
    g_seed = 0x530d70;
    for (int k = 0; k < scaled(600); k++) {
        DWORD text;
        BYTE *w = wrapper(text);
        const int kind = (int)rnd(NKINDS + 1);
        BYTE *o = kind < NKINDS ? static_cast<BYTE *>(at<Create0>(KINDS[kind].create)()) : static_cast<BYTE *>(at<Create0>(0x49f580)());
        const bool want = rnd(3) != 0;
        DWORD r[2], cls[2];
        for (int side = 0; side < 2; side++) {
            CClassInfo *out = reinterpret_cast<CClassInfo *>(0x1234);
            invcheck::side_begin(side);
            r[side] = ptrv(at<const char *(THISCALL *)(void *, void *, CClassInfo **)>(vtable_slot(w, 1))(w, o, want ? &out : nullptr));
            invcheck::side_end(side);
            cls[side] = ptrv(out);
        }
        vcall<void *>(o, 1, 1);
        char d[128];
        wsprintfA(d, "[Describe 0x%x on %s, case %d: %x %x / %x %x]", vtable_slot(w, 1), kind < NKINDS ? KINDS[kind].name : "Envelope", k, r[0], cls[0],
                  r[1], cls[1]);
        t.check(r[0] == r[1] && cls[0] == cls[1], "", d);
    }
}

void check_convert(Tally &t)
{
    g_seed = 0x5316d0;
    for (int k = 0; k < scaled(400); k++) {
        DWORD text;
        BYTE *w = wrapper(text);
        const int kind = (int)rnd(NSTART);
        const unsigned seed = random();
        DWORD h[2] = {}, vt[2] = {};
        for (int side = 0; side < 2; side++) {
            BYTE *src = static_cast<BYTE *>(at<Create0>(KINDS[kind].create)());
            g_seed = seed;
            randomize(src, KINDS[kind].size);
            invcheck::side_begin(side);
            BYTE *made = at<BYTE *(THISCALL *)(void *, void *)>(vtable_slot(w, 2))(w, src);
            invcheck::side_end(side);
            vt[side] = dw(made, 0);
            CClassInfo *c = vcall<CClassInfo *>(made, 2);
            const DWORD size = c->size;
            unsigned hh = fnv(2166136261u, &size, 4);
            for (DWORD off = 0; off + 4 <= size; off += 4) {
                const DWORD v = byte_field(off) ? dw(made, off) & 0xff : dw(made, off);
                hh = fnv(hh, &v, 4);
            }
            h[side] = hh;
            memcpy(g_bytes[side], made, size < 0x80 ? size : 0x80);
            vcall<void *>(made, 1, 1);
        }
        char d[128];
        wsprintfA(d, "[Convert 0x%x from %s, case %d: %x %x / %x %x]", vtable_slot(w, 2), KINDS[kind].name, k, vt[0], h[0], vt[1], h[1]);
        t.check(vt[0] == vt[1] && h[0] == h[1], "", d);
        if (h[0] != h[1])
            for (DWORD off = 0; off < 0x80; off += 4)
                if (dw(g_bytes[0], off) != dw(g_bytes[1], off) && !byte_field(off)) {
                    log("    first difference at +0x%x: %x / %x (wrapper %x, field %x)", off, dw(g_bytes[0], off), dw(g_bytes[1], off), ptrv(w),
                        dw(w, 0xc));
                    break;
                }
    }
}

void check_small(Tally &t)
{
    g_seed = 0x5317b0;
    for (int k = 0; k < scaled(40); k++) {
        BYTE b[2][0x24];
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            BYTE *e = static_cast<BYTE *>(pool_alloc(0x24));
            memset(e, 0xa5 + k, 0x24);
            invcheck::side_begin(side);
            r[side] = ptrv(at<BYTE *(THISCALL *)(void *)>(0x5317b0)(e)) - ptrv(e);
            invcheck::side_end(side);
            memcpy(b[side], e, 0x24);
            vcall<void *>(e, 1, 1);
        }
        t.check(r[0] == r[1] && !memcmp(b[0], b[1], 0x24), "", "[0x5317b0]");
    }
    for (int k = 0; k < scaled(40); k++) {
        BYTE o[0x58] = {};
        fl<float>(o, 0x54) = k == 0 ? __builtin_nanf("") : rnd_float();
        float r[2];
        for (int side = 0; side < 2; side++) {
            invcheck::side_begin(side);
            r[side] = (float)at<long double(THISCALL *)(void *)>(0x52b5d0)(o);
            invcheck::side_end(side);
        }
        t.check(!memcmp(&r[0], &r[1], 4), "", "[0x52b5d0]");
    }
}

}  // namespace

ZAX_CHECKS(particle_convert)
{
    (void)cls;
    (void)n;
    invcheck::g_hook_table = &hooks_particle_convert;
    Tally describe{"particle convert: the converters' Describe"}, convert{"particle convert: the converters' Convert"},
        small{"particle convert: 0x5317b0, 0x52b5d0"};
    check_describe(describe);
    check_convert(convert);
    check_small(small);
    describe.report();
    convert.report();
    small.report();
}

}  // namespace zax
