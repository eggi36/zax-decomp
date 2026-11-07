// Checks of the pickup AIs and the FieldColor helpers (ai_pickup.cpp) against the original, in the game
// thread.
//
//  - The classes: the class getters (both copies), Create/CreateArray with the deleting destructor,
//    RegisterFields and the registered descriptors (checks_inventory_common.h).
//  - The slots in the fake world (checks_ai_world.h): CPickupAI (a copy of a Create'd object with random
//    fields: "Special Item" a fake item or 0, the definitions, "Pickup Effect", the categories, the
//    action) through its item lookup, Begin, Update (the respawn modes on fake COverridePickupAIs), slot
//    23, the accept test, the pickup, the old text lines, its constructor and destructors;
//    COverridePickupAI's SetItem and destructor; CLayerPartAI's slot 31/33 default; the FieldColor
//    functions on a random block (the palette a fake).
//  - Real data: the registered class infos; the level's CPickupAIs and COverridePickupAIs (copies) through
//    their slots in the fake world, the entities copies of their parts.
#include "checks_ai_world.h"
#include "checks_inventory_common.h"
#include "checks_ai_pickup.h"

namespace zax {

int hooks_ai_pickup(Hook *out);

namespace {

using namespace aiw;
using Getter = CClassInfo *(__cdecl *)();

const invcheck::ClassDef CLASSES[] = {
    {"CPickupAI", 0x30, 0x6d0b9c, 0x53d1e0, 0x53d240, 0x53d410, {0x53d190, 0x53d3b0}, true},
    {"COverridePickupAI", 0x38, 0x6d0ba0, 0x53e8a0, 0x53e8e0, 0x53ea50, {0x53e850, 0x53e9f0}, true},
};
constexpr int NCLASSES = 2, C_PICKUP = 0, C_OVERRIDE = 1, C_NONE = 2;

void test_classes(Tally &getters, Tally &objects, Tally &fields, Tally &real_fields)
{
    invcheck::g_hook_table = hooks_ai_pickup;
    if (!invcheck::arena_setup()) {
        objects.check(false, "ai_pickup", "[no arena]");
        return;
    }
    for (int c = 0; c < NCLASSES; c++) {
        const invcheck::ClassDef &d = CLASSES[c];
        for (DWORD gtr : d.getters)
            invcheck::test_getter(getters, d, gtr);
        for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++)
            invcheck::test_create(objects, d, seed * 7 + (unsigned)c);
        for (int n = 0; n < 4; n++)
            invcheck::test_create_array(objects, d, n, (BYTE)(0x29 * n + c));
        for (unsigned seed = 1; seed <= 2; seed++)
            invcheck::test_register_fields(fields, real_fields, d, seed * 13 + (unsigned)c);
    }
}

// ---- The slots ---------------------------------------------------------------------------------------------

BYTE *g_template[NCLASSES];
int g_self_class;
const BYTE *g_real_self, *g_real_part;
BYTE *g_parts[8192];
bool g_alt_needed;   // the pickup's alternate definition is never 0 (the pickup itself reads it without a test)

void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    const int c = g_self_class;
    if (c == C_NONE)
        return;   // FieldColor: the random block
    memcpy(s, g_real_self ? g_real_self : g_template[c], CLASSES[c].size);
    fl<DWORD>(s, 4) = 0;     // no dynamic properties, no broadcaster (a copy must not reach the real ones)
    fl<DWORD>(s, 0xc) = 0;
    if (g_real_part)
        for (int i = 0; i < N_ENTITY; i++) {
            BYTE *e = obj(O_ENTITY + i);
            memcpy(e, g_real_part, OBJ);
            fl<DWORD>(e, 0) = ptrv(g_fvt[T_ENTITY]);
            fl<DWORD>(e, 0x48) = ptrv(obj(O_LAYER));
        }
    if (c == C_PICKUP) {
        if (!g_real_self) {
            fl<DWORD>(s, 0x10) = g.next() % 2 ? ptrv(obj(O_ITEM + (int)(g.next() % N_ITEM))) : 0;
            fl<DWORD>(s, 0x14) = g.next() % 5;
            fl<DWORD>(s, 0x18) = g.next() % 4;
            fl<DWORD>(s, 0x20) = g.next() % 3;
            fl<DWORD>(s, 0x24) = g.next();
            fl<DWORD>(s, 0x28) = g.next() % 2 ? ptrv(obj(O_BEHAVIOR)) : 0;
            fl<BYTE>(s, 0x2c) = (BYTE)(g.next() % 2);
        } else if (fl<DWORD>(s, 0x10)) {
            fl<DWORD>(s, 0x10) = ptrv(obj(O_ITEM));   // the special item: a fake
        }
        if (fl<DWORD>(s, 0x28))
            fl<DWORD>(s, 0x28) = ptrv(obj(O_BEHAVIOR));   // the action: a fake (its slot 1 recorded)
        if (g_alt_needed) {
            if (!fl<DWORD>(s, 0x18))
                fl<DWORD>(s, 0x18) = 1;
            for (int i = 0; i < N_AI; i++)
                if (!fl<DWORD>(obj(O_AI + i), 0x14))
                    fl<BYTE>(obj(O_AI + i), 0x18) = 0;
        }
    } else if (!g_real_self) {
        fl<DWORD>(s, 0x10) = g.next() % 2 ? ptrv(obj(O_ITEM + (int)(g.next() % N_ITEM))) : 0;
        for (DWORD off = 0x14; off < 0x38; off += 4)
            fl<DWORD>(s, off) = g.next();
    } else {
        fl<DWORD>(s, 0x10) = fl<DWORD>(s, 0x10) ? ptrv(obj(O_ITEM)) : 0;
    }
}

DWORD seconds(Gen &g) { return g.fbits_(0.0f, 2.0f); }
void a_entity(Gen &g, DWORD *a) { a[0] = ptrv(any_entity(g)); }
void a_entity_or_null(Gen &g, DWORD *a) { a[0] = ptrv(any_entity(g, 5)); }
void a_entity_seconds(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g, 6));
    a[1] = seconds(g);
}
void a_update_no_copy(Gen &g, DWORD *a)
{
    a_entity_seconds(g, a);
    for (int i = 0; i < N_AI; i++)
        fl<DWORD>(obj(O_AI + i), 0x1c) %= 3;   // no respawn mode 3: no copies (an override may be missing)
}
void a_accepts(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g, 5));
    a[1] = ptrv(any_entity(g, 6));
    a[2] = g.next();
}
void a_collide(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(any_entity(g));
    a[2] = g.next();
    a[3] = g.next();
    a[4] = g.next() % 4 ? (g.next() & 0xffffff00u) | 1 : g.next() & 0xffffff00u;
    a[5] = g.next() % 2 ? ptrv(obj(O_OUT)) : 0;
}
const char *const LINE_NAMES[] = {"Inventory Item", "inventory item", "Custom Item", "CUSTOM ITEM", "Other", "", "Inventory Items"};
const char *const LINE_VALUES[] = {"CPistolItem", "X", "", "CCountInventoryItem"};
void a_line(Gen &g, DWORD *a)
{
    a[0] = ptrv(LINE_NAMES[g.next() % 7]);
    const DWORD v = g.next() % 6;
    a[1] = v >= 4 ? (v == 4 ? 0 : ptrv(LINE_VALUES[2])) : ptrv(LINE_VALUES[v]);
    a[2] = ptrv(obj(O_OTHER + 1));
}
void a_three(Gen &g, DWORD *a)
{
    a[0] = g.next();
    a[1] = g.next();
    a[2] = g.next();
}
void a_flags0(Gen &, DWORD *a) { a[0] = 0; }
void a_item(Gen &g, DWORD *a) { a[0] = ptrv(obj(O_ITEM + (int)(g.next() % N_ITEM))); }
void a_rgba(Gen &g, DWORD *a)
{
    for (int i = 0; i < 4; i++)   // bytes (the pixel function looks the colour up in a table)
        a[i] = g.next() % 256;
}
void a_index(Gen &g, DWORD *a)
{
    a[0] = g.next() % 170;   // within the fake palette's 0x200 bytes
    fl<DWORD>(obj(O_PALETTE), 0) = ptrv(obj(O_PALETTE_DATA));   // read without a test
}
void a_palette(Gen &g, DWORD *)
{
    fl<DWORD>(obj(O_SELF), 0xc) = g.next() % 4;   // the flags
}

struct ClassSlot {
    int cls;
    SlotTest t;
    int policy;
    bool fresh_only = false;   // not on copies of the level's AIs (the destructors)
};
constexpr int P_PLAIN = 0, P_UPDATE = 1, P_ACCEPT = 2, P_COLLECT = 3, P_LINE = 4;
const ClassSlot SLOTS[] = {
    {C_PICKUP, {"CPickupAI::Item", 0x53e4c0, 1, 0xffffffff, 600, a_entity_or_null}, P_PLAIN},
    {C_PICKUP, {"CPickupAI::Begin", 0x53d990, 1, 0, 300, a_entity_or_null}, P_PLAIN},
    {C_PICKUP, {"CPickupAI::Update", 0x53da40, 2, 0, 1200, a_entity_seconds}, P_UPDATE},
    {C_PICKUP, {"CPickupAI::Update (no override needed)", 0x53da40, 2, 0, 400, a_update_no_copy}, P_PLAIN},
    {C_PICKUP, {"CPickupAI::Slot23", 0x53de80, 1, 0, 100, a_entity}, P_PLAIN},
    {C_PICKUP, {"CPickupAI::Accepts", 0x53dea0, 3, 0xff, 800, a_accepts}, P_ACCEPT},
    {C_PICKUP, {"CPickupAI::Collide", 0x53e0e0, 5, 0, 600, a_collide}, P_COLLECT},
    {C_PICKUP, {"CPickupAI::Collect", 0x53e110, 6, 0, 800, a_collide}, P_COLLECT},
    {C_PICKUP, {"CPickupAI::ReadOwnLine", 0x53e680, 3, 0xffffffff, 600, a_line}, P_LINE},
    {C_PICKUP, {"CPickupAI::Ctor", 0x53d8f0, 0, 0xffffffff, 100, nullptr}, P_PLAIN, true},
    {C_PICKUP, {"CPickupAI::Dtor", 0x53d940, 0, 0, 100, nullptr}, P_PLAIN, true},
    {C_PICKUP, {"CPickupAI::DeletingDtor", 0x53d2d0, 1, 0xffffffff, 100, a_flags0}, P_PLAIN, true},
    {C_PICKUP, {"CLayerPartAI::True", 0x53e0d0, 3, 0xff, 50, a_three}, P_PLAIN},
    {C_OVERRIDE, {"COverridePickupAI::SetItem", 0x53efe0, 1, 0xffffffff, 200, a_item}, P_PLAIN},
    {C_OVERRIDE, {"COverridePickupAI::DeletingDtor", 0x53e950, 1, 0xffffffff, 100, a_flags0}, P_PLAIN, true},
    {C_NONE, {"FieldColor_Ctor", 0x53f000, 0, 0xffffffff, 100, nullptr}, P_PLAIN},
    {C_NONE, {"FieldColor_CtorRGBA", 0x53f010, 4, 0xffffffff, 400, a_rgba}, P_PLAIN},
    {C_NONE, {"FieldColor_SetBlack", 0x53f060, 0, 0xffffffff, 100, nullptr}, P_PLAIN},
    {C_NONE, {"FieldColor_Set", 0x53f0c0, 4, 0xffffffff, 400, a_rgba}, P_PLAIN},
    {C_NONE, {"FieldColor_FromPalette", 0x53f100, 1, 0xffffffff, 400, a_index}, P_PLAIN},
    {C_NONE, {"FieldColor_PaletteIndex", 0x53f160, 0, 0xffffffff, 400, a_palette}, P_PLAIN},
};

Policy policy_of(int kind)
{
    Policy p{};
    p.find_minus_one = 4;
    switch (kind) {
    case P_UPDATE: p.nonnull = 1u << T_AI; break;
    case P_ACCEPT: p.nonnull = 1u << T_DEF; break;
    case P_COLLECT: p.nonnull = 1u << T_AI; break;
    case P_LINE:   // the object read is a CInventoryItem (its definition index is read without a test)
        p.nonnull = 1u << T_ITEM;
        p.always[0] = at<Getter>(0x429e50)();
        break;
    }
    return p;
}

void run(Tally &t, const ClassSlot &s, unsigned seed)
{
    g_self_class = s.cls;
    g_alt_needed = s.policy == P_COLLECT;
    run_slot(t, s.t, seed, make_self, policy_of(s.policy));
}

void test_slots(Tally &t)
{
    g_real_self = g_real_part = nullptr;
    for (const ClassSlot &s : SLOTS)
        for (unsigned seed = 1; seed <= (unsigned)scaled(s.t.count); seed++)
            run(t, s, seed * 2654435761u + s.t.address);
}

// ---- Real data ---------------------------------------------------------------------------------------------

void test_real(Tally &registered, Tally &real_slots)
{
    for (const invcheck::ClassDef &c : CLASSES) {
        const CClassInfo *ci = *at<CClassInfo **>(c.var);
        char d[160];
        wsprintfA(d, "[%s: class info %x, size %x, create %x]", c.name, ptrv(ci), ci ? ci->size : 0, ci ? ptrv((void *)ci->create) : 0);
        registered.check(ci && ci->size == c.size && ptrv((void *)ci->create) == c.create && ptrv((void *)ci->create_array) == c.create_array &&
                             at<Getter>(c.getters[0])() == ci && at<Getter>(c.getters[1])() == ci,
                         c.name, d);
    }
    const int n = level_parts(g_parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0) {
        log("  ai_pickup real data: no level grid found");
        return;
    }
    int found[NCLASSES] = {}, ran = 0;
    for (int i = 0; i < n; i++) {
        BYTE *part = g_parts[i];
        BYTE *ais[64];
        const int count = part_ais(part, g_parts, n, ais, 64);
        for (int k = 0; k < count; k++) {
            const CClassInfo *ci = class_of(ais[k]);
            for (int c = 0; c < NCLASSES; c++)
                if (ci && ci == *at<CClassInfo **>(CLASSES[c].var) && readable(ais[k], CLASSES[c].size)) {
                    found[c]++;
                    real_slots.check(fl<DWORD>(ais[k], 0) == PICKUP_CLASSES[c].vtable, CLASSES[c].name, "[vtable]");
                    if (found[c] > 60)
                        continue;
                    g_real_self = ais[k];
                    g_real_part = readable(part, OBJ) ? part : nullptr;
                    for (const ClassSlot &s : SLOTS)
                        if (s.cls == c && !s.fresh_only)
                            for (unsigned seed = 1; seed <= 2; seed++) {
                                run(real_slots, s, seed * 40503u + ptrv(ais[k]) + s.t.address);
                                ran++;
                            }
                    g_real_self = g_real_part = nullptr;
                }
        }
    }
    log("  ai_pickup real data: %d parts, %d CPickupAI, %d COverridePickupAI; %d cases", n, found[C_PICKUP], found[C_OVERRIDE], ran);
}

}  // namespace

ZAX_CHECKS(ai_pickup)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally getters{"ai_pickup: class getters"}, objects{"ai_pickup: Create, CreateArray, destructors"}, fields{"ai_pickup: RegisterFields"},
        real_fields{"ai_pickup: RegisterFields against the registered fields (real data)"}, slots{"ai_pickup: slots in the fake world"},
        registered{"ai_pickup: registered class infos (real data)"},
        real_slots{"ai_pickup: the level's pickup AIs through their slots (real data)"};
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    test_classes(getters, objects, fields, real_fields);
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    for (int c = 0; c < NCLASSES; c++)
        g_template[c] = static_cast<BYTE *>(at<void *(__cdecl *)()>(CLASSES[c].create)());
    if (world_on()) {
        test_slots(slots);
        test_real(registered, real_slots);
        world_off();
    } else {
        slots.check(false, "ai_pickup", "[the fake world could not be set up]");
    }
    for (int c = 0; c < NCLASSES; c++)
        if (g_template[c])
            vcall<void *>(g_template[c], 1, 1);
    slots.report();
    registered.report();
    real_slots.report();
    restore_bytes(saved_random);
    log("  ai_pickup: %u ms", GetTickCount() - start);
}

}  // namespace zax
