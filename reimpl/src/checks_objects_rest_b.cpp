// Checks of objects_rest_b.cpp against the original, in the game thread.
//
//  - CDoorAI's class (checks_inventory_common.h): both getters, RegisterFields with the registered
//    descriptors, the deleting destructor (through Create and CreateArray of ai_classes.cpp).
//  - The block readers' create functions (side 0 with the module's hooks out): a real CEntityBase or
//    CEntityAnimated is made, its slot 27 (0x4896e0 / 0x48ef00) a stand-in that records the reader global
//    (0x6bdd40 / 0x6bdd94) and the packet; the new object's bytes (pointers to itself by their offset), the
//    globals and the light beam's last-made part compared, the object deleted through slot 1 (the globals
//    put back).
//  - The owning pointer array's deleting destructor: arrays of fake objects (slot 6 records, some entries
//    null) of each side, with and without bit 0 (the array object from operator new then).
//  - The slots in the fake world (checks_rest_b_world.h): CDoorAI's Begin, Update, message, open and close
//    (every state, locked, the actions there or not, the frame advance running forwards and backwards,
//    the closing damage with touching parts, the timeout; the entity animated or not), the overlay walking
//    reader's registration, CEntityBase's rectangle (every kind and an unknown one, the cache empty or not,
//    with and without parent, the model and its "Also Render" model, the model list's records).
//  - Real data: the level's CDoorAIs as copies through Begin, Update and the message in the fake world; the
//    rectangle of the level's entities (a spread of 60, every kind, the cache as it is and cleared; the
//    cache put back before each side).
#include "checks_rest_b_world.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_objects_rest_b(Hook *out);

namespace {

using namespace rbw;
using Getter = CClassInfo *(__cdecl *)();

const invcheck::ClassDef CLASSES[] = {
    {"CDoorAI", 0x48, 0x6bdc6c, 0x481760, 0x4817b0, 0x481920, {0x481710, 0x4818c0}, true},
};

template <typename F> void two_sides(F f)
{
    for (int side = 0; side < 2; side++) {
        invcheck::side_begin(side);
        f(side);
        invcheck::side_end(side);
    }
}

// ---- The block readers' create functions --------------------------------------------------------------------

DWORD g_events[2][32];
int g_nevents[2];
int g_ev_side;
void ev(DWORD v)
{
    if (g_nevents[g_ev_side] < 32)
        g_events[g_ev_side][g_nevents[g_ev_side]] = v;
    g_nevents[g_ev_side]++;
}
bool same_events() { return g_nevents[0] == g_nevents[1] && !memcmp(g_events[0], g_events[1], 4 * (g_nevents[0] < 32 ? g_nevents[0] : 32)); }

BYTE *g_made;
DWORD THISCALL s_read_block(BYTE *o, DWORD packet)
{
    g_made = o;
    ev(0x27);
    ev(*at<DWORD *>(0x6bdd40) * 0x100 + *at<DWORD *>(0x6bdd94));
    ev(packet);
    ev(*reinterpret_cast<DWORD *>(o));
    return 0x5eb10c00u + (DWORD)g_nevents[g_ev_side];
}

void test_readers(Tally &t)
{
    struct Reader {
        DWORD create, size;
    } const readers[] = {{0x487080, 0x7c}, {0x4872d0, 0x7c}, {0x48dac0, 0xe4}, {0x48db30, 0xe4}, {0x48dbb0, 0xe4}};
    SavedBytes g1, g2, beam;
    save_bytes(g1, 0x6bdd40, 4);
    save_bytes(g2, 0x6bdd94, 4);
    save_bytes(beam, 0x6c29fc, 4);
    for (const Reader &rd : readers)
        for (unsigned k = 0; k < (unsigned)scaled(6); k++) {
            DWORD r[2], beams[2], globals[2];
            alignas(16) BYTE bytes[2][0xe4];
            bool made_ok[2];
            for (int side = 0; side < 2; side++) {
                g_ev_side = side;
                g_nevents[side] = 0;
                g_seed = 0x4ead00 + k;
                *at<DWORD *>(0x6bdd40) = random() % 3;
                *at<DWORD *>(0x6bdd94) = random() % 3;
                const DWORD packet = random();
                g_made = nullptr;
                CodePatch a, b;
                invcheck::side_begin(side);
                patch_jump(a, 0x4896e0, (void *)&s_read_block);   // CEntityBase's slot 27
                patch_jump(b, 0x48ef00, (void *)&s_read_block);   // CEntityAnimated's slot 27
                r[side] = at<DWORD(THISCALL *)(void *, DWORD)>(rd.create)(nullptr, packet);
                unpatch(b);
                unpatch(a);
                invcheck::side_end(side);
                beams[side] = *at<DWORD *>(0x6c29fc) == ptrv(g_made) ? 1 : 0;
                globals[side] = *at<DWORD *>(0x6bdd40) * 0x100 + *at<DWORD *>(0x6bdd94);
                made_ok[side] = g_made != nullptr;
                if (g_made) {
                    // the object's bytes, pointers into itself by their offset
                    for (DWORD i = 0; i < rd.size; i += 4) {
                        DWORD v = *reinterpret_cast<DWORD *>(g_made + i);
                        if (v >= ptrv(g_made) && v < ptrv(g_made) + rd.size)
                            v = 0xa0000000u + (v - ptrv(g_made));
                        memcpy(bytes[side] + i, &v, 4);
                    }
                    vcall<void *>(g_made, 1, 1);
                }
            }
            char d[140];
            wsprintfA(d, "[reader 0x%x case %u: eax %x / %x, globals %x / %x, beam %u / %u]", rd.create, k, r[0], r[1], globals[0], globals[1], beams[0],
                      beams[1]);
            t.check(r[0] == r[1] && globals[0] == globals[1] && beams[0] && beams[1] && made_ok[0] && made_ok[1] && !memcmp(bytes[0], bytes[1], rd.size) &&
                        same_events(),
                    "block reader", d);
        }
    restore_bytes(beam);
    restore_bytes(g2);
    restore_bytes(g1);
}

// ---- The owning pointer array's deleting destructor ------------------------------------------------------------

struct Fake6 {
    DWORD vtable;
    DWORD id;
};
DWORD g_fake6_vtable[8];
Fake6 g_fake6[12];
DWORD THISCALL fake6_delete(Fake6 *o, DWORD flags)
{
    ev(0x600 + o->id * 0x10 + flags);
    return ptrv(o);
}

void test_owning_array(Tally &t)
{
    for (DWORD i = 0; i < 8; i++)
        g_fake6_vtable[i] = 0;
    g_fake6_vtable[6] = ptrv((void *)&fake6_delete);
    for (DWORD i = 0; i < 12; i++)
        g_fake6[i] = {ptrv(g_fake6_vtable), i + 1};
    for (unsigned k = 0; k < (unsigned)scaled(40); k++) {
        DWORD r[2], state[2][4];
        for (int side = 0; side < 2; side++) {
            g_ev_side = side;
            g_nevents[side] = 0;
            g_seed = 0x4874a0 + k;
            const DWORD flags = random() % 4;
            const DWORD n = random() % 9, room = n + random() % 4;
            static DWORD fixed[2][4];
            DWORD *a = flags & 1 ? static_cast<DWORD *>(heap_alloc(0x10)) : fixed[side];
            a[0] = 0x5f1de8;
            a[1] = room ? ptrv(pool_get_alloc(room * 4)) : 0;
            a[2] = n;
            a[3] = room;
            for (DWORD i = 0; i < n; i++)
                at<DWORD *>(a[1])[i] = random() % 5 ? ptrv(&g_fake6[random() % 12]) : 0;
            invcheck::side_begin(side);
            const DWORD res = ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x4874a0)(a, flags));
            invcheck::side_end(side);
            r[side] = res == ptrv(a) ? 1 : res;
            if (flags & 1)
                memset(state[side], 0, sizeof state[side]);
            else
                memcpy(state[side], a, 16);
        }
        t.check(r[0] == r[1] && !memcmp(state[0], state[1], sizeof state[0]) && same_events(), "OwningPtrArray::DeletingDtor", "");
    }
}

// ---- The slots in the fake world --------------------------------------------------------------------------------

enum Self { S_DOOR, S_ENTITY, S_PACKET };
int g_self;
BYTE *g_real_self;   // a real AI copied into object 0 (real data)
DWORD g_real_size;

void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    if (g_real_self) {
        memcpy(s, g_real_self, g_real_size);
        return;
    }
    switch (g_self) {
    case S_DOOR:
        s[0x10] = (BYTE)(g.next() % 4 == 0);
        fl<DWORD>(s, 0x14) = g.next() % 5;
        fl<DWORD>(s, 0x18) = g.next() % 4 == 0 ? 0 : g.fbits_(-30.0f, 30.0f);
        fl<DWORD>(s, 0x1c) = g.next() % 3 == 0 ? 0xbf800000 : g.fbits_(-0.1f, 0.3f);
        for (DWORD off = 0x20; off <= 0x38; off += 4)
            fl<DWORD>(s, off) = g.next() % 3 ? ptrv(obj(O_ACTION + (int)(g.next() % N_ACTION))) : 0;
        fl<DWORD>(s, 0x3c) = g.next() % 3 == 0 ? 0 : g.fbits_(-1.0f, 10.0f);
        fl<DWORD>(s, 0x40) = g.fbits_(-1.0f, 5.0f);
        break;
    case S_ENTITY:
        fl<DWORD>(s, 0) = ptrv(g_fvt[T_ENTITY]);
        fl<DWORD>(s, 0xc) = g.next() % 3 == 0 ? ptrv(any_entity(g)) : 0;
        fl<int>(s, 0x54) = (int)(g.next() % 4000);
        fl<int>(s, 0x58) = (int)(g.next() % 4000);
        if (g.next() % 2) {
            fl<int>(s, 0x5c) = fl<int>(s, 0x64) = (int)(g.next() % 100);
        } else {
            fl<int>(s, 0x5c) = (int)(g.next() % 1000);
            fl<int>(s, 0x60) = (int)(g.next() % 1000);
            fl<int>(s, 0x64) = fl<int>(s, 0x5c) + 1 + (int)(g.next() % 300);
            fl<int>(s, 0x68) = fl<int>(s, 0x60) + 1 + (int)(g.next() % 300);
        }
        fl<int>(s, 0x6c) = (int)(g.next() % 101) - 50;
        fl<DWORD>(s, 0x74) = g.next() % 6;
        fl<DWORD>(s, 0x78) = g.next() % 4;
        break;
    default:
        break;
    }
}
void a_entity(Gen &g, DWORD *a) { a[0] = ptrv(any_entity(g)); }
void a_update(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = g.next() % 8 ? g.fbits_(0.0f, 0.5f) : g.fbits_(-0.1f, 3.0f);
}
void a_message(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    BYTE *m = obj(O_MSG);
    const DWORD ids[] = {*at<DWORD *>(0x6bdc60), *at<DWORD *>(0x6bdc64), *at<DWORD *>(0x6bdc3c), *at<DWORD *>(0x6bdc38), *at<DWORD *>(0x6bdc40),
                         g.next()};
    fl<DWORD>(m, 4) = ids[g.next() % 6];
    m[8] = (BYTE)g.next();
    a[1] = ptrv(m);
}
void a_rect(Gen &g, DWORD *a)
{
    a[0] = ptrv(obj(O_SPARE));
    a[1] = g.next() % 7;
    BYTE *o = obj(O_SPARE + 1);
    fl<int>(o, 0) = (int)(g.next() % 200) - 100;
    fl<int>(o, 4) = (int)(g.next() % 200) - 100;
    fl<int>(o, 8) = (int)(g.next() % 100) - 50;
    a[2] = g.next() % 3 ? ptrv(o) : 0;
}

struct ClassSlot {
    int self;
    SlotTest t;
};
const ClassSlot SLOTS[] = {
    {S_DOOR, {"CDoorAI::Begin", 0x482110, 1, 0xffffffff, 200, a_entity}},
    {S_DOOR, {"CDoorAI::Update", 0x482170, 2, 0, 1200, a_update}},
    {S_DOOR, {"CDoorAI::Message", 0x4824a0, 2, 0xff, 800, a_message}},
    {S_DOOR, {"CDoorAI::Open", 0x4825b0, 1, 0xff, 500, a_entity}},
    {S_DOOR, {"CDoorAI::Close", 0x482710, 1, 0xff, 500, a_entity}},
    {S_PACKET, {"COverlayWalking_RegisterReader", 0x49d390, 0, 0xffffffff, 20, nullptr}},
    {S_ENTITY, {"CEntityBase::Rect", 0x48a100, 3, 0xffffffff, 1500, a_rect}},
};

Policy door_policy(unsigned seed)
{
    Policy p{};
    if (seed % 4)
        p.always[0] = at<Getter>(0x48dcc0)();   // CEntityAnimated
    return p;
}

void test_slots(Tally &t)
{
    for (const ClassSlot &s : SLOTS) {
        g_self = s.self;
        const unsigned n = (unsigned)scaled(s.t.count);
        for (unsigned seed = 1; seed <= n; seed++)
            run_slot(t, s.t, seed * 2654435761u + s.t.address, make_self, door_policy(seed));
    }
}

// The level's door AIs as copies (at most 12, a spread) through Begin, Update and the message
void test_real_doors(Tally &t, BYTE **parts, int nparts)
{
    BYTE **ais = static_cast<BYTE **>(heap_alloc(64 * 4));
    const CClassInfo *door = *at<CClassInfo **>(0x6bdc6c);
    int count = 0;
    for (int pass = 0; pass < 2; pass++) {
        int seen = 0;
        for (int i = 0; i < nparts; i++) {
            const int na = part_ais(parts[i], parts, nparts, ais, 64);
            for (int k = 0; k < na; k++) {
                if (class_of(ais[k]) != door)
                    continue;
                const int index = seen++;
                if (pass == 0 || !spread_pick(index, count, 12))
                    continue;
                g_real_self = ais[k];
                g_real_size = 0x48;
                g_self = S_DOOR;
                for (int s = 0; s < 3; s++)
                    for (unsigned seed = 1; seed <= 4; seed++)
                        run_slot(t, SLOTS[s].t, seed * 7919u + (unsigned)index * 131u + SLOTS[s].t.address, make_self, door_policy(seed));
            }
        }
        if (pass == 0)
            count = seen;
    }
    g_real_self = nullptr;
    log("  objects_rest_b: %d door AIs in the level", count);
    heap_free(ais);
}

// The rectangle of the level's entities (outside the fake world: the real models, sequences and frames;
// the tick count the animated sequences' frames depend on (0x4d2650, through the import 0x5ea0e8) fixed)
DWORD WINAPI fixed_ticks() { return 0x123456; }
void test_real_rects(Tally &t, BYTE **parts, int nparts)
{
    ImportPatch ticks;
    patch_import(ticks, 0x5ea0e8, (void *)&fixed_ticks);
    const CClassInfo *base = *at<CClassInfo **>(0x6bdd44);
    int count = 0;
    for (int i = 0; i < nparts; i++)
        if (derived_from(class_of(parts[i]), base))
            count++;
    int index = 0;
    for (int i = 0; i < nparts; i++) {
        BYTE *e = parts[i];
        if (!derived_from(class_of(e), base) || !spread_pick(index++, count, 60))
            continue;
        int cache[4];
        memcpy(cache, e + 0x5c, 16);
        for (DWORD kind = 0; kind < 6; kind++)
            for (int cleared = 0; cleared < 2; cleared++) {
                int rect[2][4], offset[3] = {3, -5, 7};
                DWORD r[2];
                int after[2][4];
                for (int side = 0; side < 2; side++) {
                    memcpy(e + 0x5c, cache, 16);
                    if (cleared)
                        memset(e + 0x5c, 0, 16);
                    memset(rect[side], 0x5a, 16);
                    invcheck::side_begin(side);
                    r[side] = at<DWORD(THISCALL *)(void *, int *, DWORD, int *)>(0x48a100)(e, rect[side], kind, kind % 2 ? offset : nullptr);
                    invcheck::side_end(side);
                    memcpy(after[side], e + 0x5c, 16);
                }
                memcpy(e + 0x5c, cache, 16);
                char d[160];
                wsprintfA(d, "[rect of %x kind %u%s: %x / %x, %d %d %d %d / %d %d %d %d]", ptrv(e), kind, cleared ? " cleared" : "", r[0], r[1], rect[0][0],
                          rect[0][1], rect[0][2], rect[0][3], rect[1][0], rect[1][1], rect[1][2], rect[1][3]);
                const bool same_r = (r[0] & 0xff) == (r[1] & 0xff) && ((r[0] & 0xff) == 0 || !memcmp(rect[0], rect[1], 16));
                t.check(same_r && !memcmp(after[0], after[1], 16), "CEntityBase::Rect", d);
            }
    }
    unpatch_import(ticks);
    log("  objects_rest_b: %d entities in the level", count);
}

}  // namespace

ZAX_CHECKS(objects_rest_b)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    invcheck::g_hook_table = hooks_objects_rest_b;
    if (!invcheck::arena_setup()) {
        log("Check objects_rest_b: MISMATCH (no arena)");
        return;
    }
    Tally getters{"objects_rest_b: class getters"}, objects{"objects_rest_b: Create, CreateArray, destructors"}, fields{"objects_rest_b: RegisterFields"},
        real_fields{"objects_rest_b: registered descriptors and classes (real data)"}, small{"objects_rest_b: block readers, owning array"},
        slots{"objects_rest_b: door, rectangle, reader registration in the fake world"},
        real{"objects_rest_b: the level's doors as copies and the entities' rectangles (real data)"};
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    log("    > classes");
    invcheck::test_classes(objects, getters, fields, real_fields, CLASSES, 1);
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    log("    > readers");
    test_readers(small);
    log("    > owning array");
    test_owning_array(small);
    small.report();
    BYTE **parts = static_cast<BYTE **>(heap_alloc(8192 * 4));
    const int nparts = level_parts(parts, 8192);
    log("    > real rectangles");
    test_real_rects(real, parts, nparts);
    if (world_on()) {
        test_slots(slots);
        log("    > real doors");
        test_real_doors(real, parts, nparts);
        world_off();
    } else {
        slots.check(false, "objects_rest_b", "[the fake world could not be set up]");
    }
    heap_free(parts);
    slots.report();
    real.report();
    restore_bytes(saved_random);
    log("  objects_rest_b: %u ms", GetTickCount() - start);
}

}  // namespace zax
