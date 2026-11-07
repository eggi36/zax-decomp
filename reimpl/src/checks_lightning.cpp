// Checks of lightning.cpp against the original, in the game thread.
//  - The class (checks_inventory_common.h): Create, CreateArray, the deleting destructor (both arrays' data
//    freed), the getters, RegisterFields and the registered descriptors; RegisterFields' first use (the
//    default colours' guard cleared, AddField and atexit stand-ins): the static colours and the guard.
//  - On new objects of the class (both sides their own, the same fields) with random positions, lengths
//    (also 0 and negative), heights, widths and minimum segment lengths (1-40) and on copies of the running
//    level's lightning parts (none on the standard maps), the game's random generator put back before each
//    side: the geometry 0x500080 and 0x4fff60 (the points, the angle's cosine and sine), the draw (slot 23)
//    with stand-ins for DrawQueue_Add and CLayerPart's slot 23 (the draw items with their pointers made
//    relative to the object, the queue calls, eax), the bounds 0x4feb30 alone, the rectangle (slot 37,
//    kinds -1 … 6), the hit kinds (slot 38), the setters (slots 83-86 and the field setters 0x500700-0x500730,
//    the relist a stand-in).
//  - The network blocks (slots 25/27) with recording stand-ins for the packet (words, bytes, the block type;
//    reads from the case's values): the calls, the fields read back (the colours too), eax; the reader's
//    registration; slot 61 (the short name) on the lightning class and other layer parts.
#include "checks_inventory_common.h"
#include "checks_particle_world.h"
#include "ai_common.h"

namespace zax {

int hooks_lightning(Hook *out);

namespace {

using namespace ai;
using invcheck::ev;
using invcheck::ptrv;
using Create0 = void *(__cdecl *)();
DWORD rnd(DWORD n) { return n ? random() % n : 0; }
float rnd_float(float lo, float hi) { return lo + (hi - lo) * (float)(random() % 100001) / 100000.0f; }

const invcheck::ClassDef CLASSES[] = {
    {"CLightningLayerPart", 0xb4, 0x6c2b8c, 0x4fecf0, 0x4fed60, 0x4fefc0, {0x4fec90, 0x4fef50}, true},
};

// ---- Recording stand-ins ---------------------------------------------------------------------------------------

BYTE *g_obj;   // the side's object (pointers into it logged relative to it)
DWORD rel(const void *p)
{
    const DWORD v = ptrv(p);
    return g_obj && v >= ptrv(g_obj) && v < ptrv(g_obj) + 0xb4 ? 0x0b000000 + (v - ptrv(g_obj)) : v;
}
DWORD THISCALL rec_relist(void *o) { ev('L', rel(o)); return invcheck::result(1); }
DWORD THISCALL rec_queue_add(void *q, BYTE *item)
{
    unsigned h = fnv(2166136261u, item + 0x18, 0xe);   // (+0x26/+0x27 padding)
    h = fnv(h, item + 0x2c, 0x18);
    h = fnv(h, item + 0x44, 0xc);
    ev('Q', ptrv(q), (DWORD)h, rel(bp(dw(item, 0x10))) ^ rel(bp(dw(item, 0x28))) << 1 ^ rel(bp(dw(item, 0x50))) << 2 ^ rel(bp(dw(item, 0x54))) << 3);
    ev('q', (DWORD)(item[0x24] | item[0x25] << 8), dw(item, 4) | dw(item, 8) | dw(item, 0x14));
    return invcheck::result(2);
}
DWORD THISCALL rec_part_slot23(void *o, DWORD a1, void *q, DWORD a3, DWORD a4, const int *off)
{
    ev(rel(o), a1, ptrv(q), a3);
    ev(a4, (DWORD)off[0], (DWORD)off[1], (DWORD)off[2]);
    return invcheck::result(3);
}
DWORD THISCALL rec_write_byte(void *p, DWORD v) { ev('8', ptrv(p), v & 0xff); return invcheck::result(4); }
DWORD THISCALL rec_write_word(void *p, DWORD v) { ev('W', ptrv(p), v & 0xffff); return invcheck::result(5); }
DWORD THISCALL rec_block_type(void *p, void *r, DWORD k) { ev('b', ptrv(p), ptrv(r), k); return invcheck::result(6); }
BYTE g_reads[24];
int g_nread;
DWORD THISCALL rec_read_byte(void *p, BYTE *out)
{
    *out = g_reads[g_nread++ % 24];
    ev('r', ptrv(p), *out);
    return invcheck::result(7);
}
DWORD THISCALL rec_read_word(void *p, BYTE *out)
{
    out[0] = g_reads[g_nread++ % 24];
    out[1] = g_reads[g_nread++ % 24];
    ev('w', ptrv(p), out[0] | out[1] << 8);
    return invcheck::result(8);
}
DWORD THISCALL rec_register(void *p, void *r, const char *name)
{
    ev('g', ptrv(p), ptrv(r), fnv(2166136261u, name, (DWORD)lstrlenA(name)));
    return invcheck::result(9);
}
DWORD THISCALL rec_add_field(CClassInfo *c, void *f) { ev('F', ptrv(c), f ? 1 : 0); return invcheck::result(10); }
int g_atexits;
int __cdecl rec_atexit(DWORD f)
{
    ev('X', f);
    g_atexits++;
    return 0;
}

struct Patches {
    CodePatch p[12];
    int n = 0;
    void add(DWORD a, void *f) { patch_jump(p[n++], a, f); }
    void clear()
    {
        while (n)
            unpatch(p[--n]);
    }
};

// ---- Objects for a case ---------------------------------------------------------------------------------------

BYTE g_fields[0xb4];   // the case's field values (+0x4c … +0x60, +0x8c, +0x90, the colours)
BYTE *g_reals[8];
int g_nreal;
BYTE *g_parts[4096];
void find_real()
{
    g_nreal = 0;
    const int n = pw::level_parts(g_parts, 4096);
    for (int i = 0; i < n && g_nreal < 8; i++)
        if (dw(g_parts[i], 0) == 0x5f984c && pw::readable(g_parts[i], 0xb4))
            g_reals[g_nreal++] = g_parts[i];
}
void random_fields(int k)
{
    memset(g_fields, 0, sizeof g_fields);
    if (k < g_nreal) {
        memcpy(g_fields, g_reals[k], 0xb4);
        return;
    }
    fl<float>(g_fields, 0x4c) = rnd_float(-500, 3000);
    fl<float>(g_fields, 0x50) = rnd_float(-500, 3000);
    fl<float>(g_fields, 0x54) = rnd(3) ? 0.0f : rnd_float(-50, 200);
    fl<float>(g_fields, 0x58) = rnd(8) ? rnd_float(-400, 400) : 0.0f;
    fl<float>(g_fields, 0x5c) = rnd(8) ? rnd_float(-400, 400) : 0.0f;
    fl<float>(g_fields, 0x60) = rnd(3) ? 0.0f : rnd_float(-100, 100);
    fl<float>(g_fields, 0x8c) = rnd(4) ? rnd_float(0, 0.6f) : 0.25f;
    fl<float>(g_fields, 0x90) = (float)(1 + rnd(40));
    dw(g_fields, 0x1c) = rnd(2) ? 0x200000 : 0;
    for (DWORD off = 0x94; off < 0xb4; off += 0x10) {
        dw(g_fields, off) = random();
        dw(g_fields, off + 4) = dw(g_fields, off);
        dw(g_fields, off + 8) = random() & 0xffff;
        dw(g_fields, off + 0xc) = rnd(4);
    }
}
// A new object with the case's fields (the arrays and the vtable its own)
BYTE *make_object()
{
    BYTE *o = static_cast<BYTE *>(original<Create0>(0x4fecf0)());
    if (dw(g_fields, 0))   // a copy of a real part: its position and look, not its links
        dw(g_fields, 0x1c) &= 0x200000;
    memcpy(o + 0x4c, g_fields + 0x4c, 0x18);
    memcpy(o + 0x8c, g_fields + 0x8c, 0x28);
    dw(o, 0x1c) = dw(g_fields, 0x1c);
    return o;
}
void delete_object(BYTE *o) { original<void *(THISCALL *)(void *, DWORD)>(0x4fee10)(o, 1); }

// The state of an object after a case: fields, cosine and sine, the points, the draw items (pointers
// relative to the object)
unsigned object_state(BYTE *o)
{
    unsigned h = fnv(2166136261u, o + 0x4c, 0x18);
    h = fnv(h, o + 0x84, 0x30);
    h = fnv(h, o + 0x1c, 4);
    const DWORD npts = dw(o, 0x7c), nitems = dw(o, 0x6c);
    h = fnv(h, &npts, 4);
    if (npts && npts < 0x100000)
        h = fnv(h, bp(dw(o, 0x78)), npts * 8);
    h = fnv(h, &nitems, 4);
    for (DWORD i = 0; i < nitems && i < 4096; i++) {
        BYTE *it = bp(dw(o, 0x68)) + i * 0x58;
        h = fnv(h, it + 0x18, 0xe);   // (+0x26/+0x27 padding, as the heap gave it)
        h = fnv(h, it + 0x2c, 0x18);
        h = fnv(h, it + 0x44, 0xc);
        const DWORD r = rel(bp(dw(it, 0x10))) ^ rel(bp(dw(it, 0x28))) << 1 ^ rel(bp(dw(it, 0x50))) << 2 ^ rel(bp(dw(it, 0x54))) << 3;
        h = fnv(h, &r, 4);
    }
    return h;
}

DWORD g_queue[4];
const char *const CASE_NAMES[] = {"geometry", "subdivide", "draw", "draw (no queue)", "rect", "hit kind", "set slot", "field setter", "bounds"};

void check_objects(Tally &t)
{
    g_seed = 0x500080;
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    for (int k = 0; k < scaled(400) + g_nreal * 3; k++) {
        random_fields(k / 3 < g_nreal ? k / 3 : g_nreal + k);
        const int what = k < g_nreal * 3 ? (k % 3 == 0 ? 0 : 2) : (int)rnd(9);
        const int kind = (int)rnd(8) - 1, setslot = 83 + (int)rnd(4);
        const DWORD value = random();
        const int off[3] = {(int)rnd(400) - 200, (int)rnd(400) - 200, (int)rnd(10)};
        const float from[2] = {rnd_float(-200, 200), rnd_float(-200, 200)}, to[2] = {rnd_float(-200, 200), rnd_float(-200, 200)};
        const float len = rnd_float(0, 300);
        const float cs[2] = {rnd_float(-1, 1), rnd_float(-1, 1)};
        const DWORD pick1 = random(), pick2 = random();
        unsigned h[2];
        DWORD eax[2];
        for (int side = 0; side < 2; side++) {
            restore_bytes(saved_random);
            BYTE *o = make_object();
            g_obj = o;
            invcheck::log_reset(side);
            Patches p;
            p.add(0x49ca30, (void *)&rec_queue_add);
            p.add(0x4fa9a0, (void *)&rec_part_slot23);
            p.add(0x5717e0, (void *)&rec_relist);
            if (what == 8) {   // the bounds case: the points made by the original on both sides first
                invcheck::hooks_out();
                at<void(THISCALL *)(void *)>(0x500080)(o);
                invcheck::hooks_in();
            }
            invcheck::side_begin(side);
            eax[side] = 0;
            int rect[4] = {0x11, 0x22, 0x33, 0x44};
            switch (what) {
            case 0:
                at<void(THISCALL *)(void *)>(0x500080)(o);
                break;
            case 1:
                fl<float>(o, 0x84) = cs[0];
                fl<float>(o, 0x88) = cs[1];
                at<void(THISCALL *)(void *, const float *, const float *, float)>(0x4fff60)(o, from, to, len);
                break;
            case 2:
            case 3:
                eax[side] = at<DWORD(THISCALL *)(void *, DWORD, void *, DWORD, DWORD, const int *)>(0x4ff500)(o, 7, what == 2 ? g_queue : nullptr, 8, 9, off);
                break;
            case 4:
                eax[side] = at<BYTE(THISCALL *)(void *, int *, int, DWORD)>(0x4ffcb0)(o, rect, kind, 0);
                ev('R', (DWORD)rect[0], (DWORD)rect[1], (DWORD)rect[2] ^ (DWORD)rect[3] << 16);
                break;
            case 5:
                eax[side] = at<BYTE(THISCALL *)(void *, DWORD, DWORD, int)>(0x4fff40)(o, 1, 2, kind);
                break;
            case 6: {
                const float r = (float)vcall<long double>(o, setslot, value);
                memcpy(&eax[side], &r, 4);
                break;
            }
            case 7: {
                const float r = (float)at<long double(THISCALL *)(void *, DWORD)>(0x500700 + 0x10 * (setslot - 83))(o, value);
                memcpy(&eax[side], &r, 4);
                break;
            }
            default: {
                // a fake item over the object's points
                BYTE item[0x58] = {};
                const DWORD n = dw(o, 0x7c);
                dw(item, 0x28) = ptrv(o);
                dw(item, 0x2c) = n ? pick1 % n : 0;
                dw(item, 0x30) = n > 1 ? dw(item, 0x2c) + pick2 % (n - dw(item, 0x2c)) - 1 : 0xffffffff;
                if (n && dw(item, 0x30) + 1 >= n)
                    dw(item, 0x30) = n - 2;   // the loop reads up to last + 1
                at<void(THISCALL *)(void *)>(0x4feb30)(item);
                ev('B', dw(item, 0x34), dw(item, 0x38), dw(item, 0x3c) ^ dw(item, 0x40) << 16);
            }
            }
            invcheck::side_end(side);
            p.clear();
            h[side] = object_state(o) ^ invcheck::g_loghash[side];
            g_obj = nullptr;
            delete_object(o);
        }
        restore_bytes(saved_random);
        char d[128];
        wsprintfA(d, "[%s, case %d: eax %x / %x, state %x / %x]", CASE_NAMES[what], k, eax[0], eax[1], h[0], h[1]);
        t.check(invcheck::same_log() && h[0] == h[1] && eax[0] == eax[1], "", d);
    }
    log("  lightning: %d real parts", g_nreal);
}

DWORD g_packet[4];
void check_blocks(Tally &t)
{
    g_seed = 0x4ff710;
    for (int k = 0; k < scaled(300); k++) {
        random_fields(g_nreal + k);
        for (BYTE &b : g_reads)
            b = (BYTE)random();
        if (rnd(3) == 0)
            g_reads[(random() % 8) + 10] |= 0x80;   // the opaque bit somewhere around the colours
        const int what = (int)rnd(4);
        const DWORD flag = random() & 0xffffff01;
        unsigned h[2];
        DWORD eax[2];
        for (int side = 0; side < 2; side++) {
            BYTE *o = make_object();
            g_obj = o;
            invcheck::log_reset(side);
            g_nread = 0;
            Patches p;
            p.add(0x520820, (void *)&rec_write_byte);
            p.add(0x5209c0, (void *)&rec_write_word);
            p.add(0x51ff50, (void *)&rec_block_type);
            p.add(0x520490, (void *)&rec_read_byte);
            p.add(0x520510, (void *)&rec_read_word);
            p.add(0x51ff30, (void *)&rec_register);
            invcheck::side_begin(side);
            switch (what) {
            case 0:
                eax[side] = at<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4ff710)(o, g_packet, flag);
                break;
            case 1:
                at<void(THISCALL *)(void *, void *)>(0x4ff9c0)(o, g_packet);
                eax[side] = 0;
                break;
            case 2:
                eax[side] = at<DWORD(THISCALL *)(void *)>(0x4ff4f0)(g_packet);
                break;
            default:
                eax[side] = ptrv(at<const char *(THISCALL *)(void *)>(0x5717d0)(o));
            }
            invcheck::side_end(side);
            p.clear();
            h[side] = fnv(fnv(2166136261u, o + 0x4c, 0x18), o + 0x8c, 0x28);
            g_obj = nullptr;
            delete_object(o);
        }
        char d[96];
        wsprintfA(d, "[block case %d (%d): eax %x / %x, fields %x / %x]", k, what, eax[0], eax[1], h[0], h[1]);
        t.check(invcheck::same_log() && h[0] == h[1] && eax[0] == eax[1], "", d);
    }
    // slot 61 on other layer parts of the level (shared by their vtables)
    const int n = pw::level_parts(g_parts, 4096);
    for (int i = 0; i < n && i < scaled(60); i++) {
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            invcheck::side_begin(side);
            r[side] = ptrv(at<const char *(THISCALL *)(void *)>(0x5717d0)(g_parts[i]));
            invcheck::side_end(side);
        }
        t.check(r[0] == r[1], "", "[0x5717d0 on a level part]");
    }
}

// RegisterFields' first use: the guard cleared, AddField and atexit stand-ins
void check_first_use(Tally &t)
{
    SavedBytes colours, guard;
    save_bytes(colours, 0x6c2b68, 0x20);
    save_bytes(guard, 0x6c2b88, 1);
    unsigned h[2];
    for (int side = 0; side < 2; side++) {
        restore_bytes(colours);
        *at<BYTE *>(0x6c2b88) &= (BYTE)~3;
        memset(at<void *>(0x6c2b68), 0x5a, 0x20);
        invcheck::log_reset(side);
        Patches p;
        p.add(0x415700, (void *)&rec_add_field);
        p.add(0x5d0863, (void *)&rec_atexit);
        invcheck::side_begin(side);
        at<DWORD(__cdecl *)()>(0x4fefc0)();
        invcheck::side_end(side);
        p.clear();
        h[side] = fnv(fnv(invcheck::g_loghash[side], at<void *>(0x6c2b68), 0x20), at<void *>(0x6c2b88), 1);
    }
    restore_bytes(colours);
    restore_bytes(guard);
    // the descriptors the stand-in did not add stay with the heap (eight small blocks per side)
    char d[64];
    wsprintfA(d, "[first use: %x / %x]", h[0], h[1]);
    t.check(h[0] == h[1] && invcheck::same_log(), "", d);
}

}  // namespace

ZAX_CHECKS(lightning)
{
    (void)cls;
    (void)n;
    invcheck::g_hook_table = &hooks_lightning;
    if (!invcheck::arena_setup()) {
        log("Check lightning: no arena");
        return;
    }
    Tally objects{"lightning: Create, CreateArray, destructors"}, getters{"lightning: class getters"}, fields{"lightning: RegisterFields"},
        registered{"lightning: registered descriptors and classes (real data)"};
    invcheck::test_classes(objects, getters, fields, registered, CLASSES, 1);
    check_first_use(fields);
    objects.report();
    getters.report();
    fields.report();
    registered.report();
    find_real();
    Tally slots{"lightning: geometry, draw, bounds, rectangle, setters"}, blocks{"lightning: network blocks, reader, short name"};
    check_objects(slots);
    check_blocks(blocks);
    slots.report();
    blocks.report();
}

}  // namespace zax
