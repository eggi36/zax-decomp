// Checks of the inventory item classes and the rest of the inventory (inventory_items.cpp) against the
// original, in the game thread (machinery: checks_inventory_common.h).
//  - Classes: Create (pool block filled with a pattern, the C heap from the arena), CreateArray with
//    0-3 elements, the deleting destructors (single and array), the class getters, RegisterFields into
//    a fake class info and with the class variable 0; the constructors and destructors called directly
//    (CInventory, CInventoryItem with and without a definition index, the three page option classes).
//  - Behaviour on a fake world, set up again from the same seed before each side: an item catalogue of
//    our own (the globals 0x6c0c38/0x6c0c3c point to it during the case) whose definitions record their
//    slots 2, 24, 25, 35, 36; an inventory with the real vtable 0x5ee0c0 copied, slot 26 recording and
//    forwarding to GetItem; items (CCountInventoryItem layout) with copies of the real item vtables,
//    some slots recording; a part (category bits, slot 36 the inventory or null); the functions the
//    module calls in other subsystems jump to recorders during the case (the automatic weapon choice
//    0x425a70, the sound functions of ChangeTo, the quest game type of Discover, the definition helpers
//    0x5b7a60/0x5b8a80/0x5b8ac0/0x4d6540/0x5b7e20/0x5b7ab0, the part's name, the error reporter).
//    Compared: eax (not for the functions that leave it undefined), the event log, all bytes of the
//    fake world.
//  - Real data: the registered descriptors and class infos; the player's inventory (0x428b10), its items
//    and items made from loaded definitions of the real item catalogue (read-only slots; the made items
//    are deleted again, the running game stays as it was).
#include "checks_inventory_common.h"
#include "inventory_items.h"

namespace zax {

int hooks_inventory_items(Hook *out);

namespace {

using namespace invcheck;

// ---- The classes -----------------------------------------------------------------------------------------

const ClassDef CLASSES[] = {
    {"CItemTypeData", 0x18, 0x62e9fc, 0x424190, 0x4241d0, 0x424310, {0, 0}, true},
    {"CInventory", 0x48, 0x62ea00, 0x4245b0, 0x4245d0, 0x4246f0, {0x424560, 0x424690}, true},
    {"CInventoryDisplayOptions", 0x2a0, 0x62ea18, 0x426cc0, 0x426dd0, 0x427240, {0x4271e0, 0}, true},
    {"CInventoryItem", 0x18, 0x62ea20, 0x429ee0, 0x429f20, 0x42a090, {0x429e50, 0x42a030}, true},
    {"CCountInventoryItem", 0x24, 0x62ea24, 0x42a8e0, 0x42a930, 0x42ab20, {0x42a760, 0x42aa60}, true},
    {"CInventoryItemHolder", 0xc, 0x62e9f8, 0, 0, 0x4240b0, {0, 0}, false},
};

// ---- The fake world ----------------------------------------------------------------------------------

constexpr int NDEF = 12, MAXI = 12, NTYPES = 64, CAP = 32;

struct Record {
    BYTE name[0x78];
    void *object;   // 0x78
    BYTE keep, in_use, unknown7e, loading;
};
static_assert(sizeof(Record) == 0x80);

struct FakeDef {
    DWORD vftable;
    BYTE body[0x100];
    DWORD id;
    CClassInfo *cls;    // slot 2
    DWORD accept;       // slot 25: low byte
    DWORD limit;        // slot 36
};
DWORD &def_dw(FakeDef &d, DWORD off) { return *reinterpret_cast<DWORD *>(reinterpret_cast<BYTE *>(&d) + off); }

struct FakeItem {
    CCountInventoryItem base;
    DWORD id;
    DWORD accept;       // slot 32 (inventory tests): low byte
};

struct FakePart {
    DWORD vftable;
    BYTE body[0x18];
    DWORD categories;   // 0x1c
    DWORD id;
    DWORD inventory;    // slot 36
    CClassInfo *cls;    // slot 2
};
static_assert(offsetof(FakePart, categories) == 0x1c);

struct FakeObject {     // the quest game type and its stat: slot 2 the class
    DWORD vftable;
    CClassInfo *cls;
};

struct World {
    Record records[NDEF];
    FakeDef defs[NDEF];
    FakeItem items[MAXI + 1];
    void *data[CAP];
    void *discovered[CAP];
    void *disposed[CAP];
    CItemTypeData types[NTYPES];
    CInventory inv;
    FakePart part, part2;
    FakeObject game, stat;
    BYTE broadcaster[0x28];
    DWORD s_c[4];          // 0x5525b0's object: +0xc points to s_t
    DWORD s_t[12];         // +0x20 the value
    BYTE message[0x14];
    FakeItem clone;        // what Object_Clone gives
    ZPtrArray cand;        // candidates of the automatic choice
    void *cand_data[8];
};
World g_world;

void *g_inv_vtable[64];
void *g_item_vtable[2][64];    // [0] CInventoryItem, [1] CCountInventoryItem; some slots recording
void *g_list_vtable[4];        // the disposed list
void *g_def_vtable[40];
void *g_part_vtable[40];
void *g_object_vtable[4];
void *g_reporter_vtable[20];
DWORD g_reporter;
bool g_trace = false;          // a log line per case (to find a case that ends the game)

int item_id(const void *p)
{
    for (int i = 0; i <= MAXI; i++)
        if (p == &g_world.items[i])
            return i + 1;
    return p ? 0x99 : 0;
}
DWORD def_id(const void *p)
{
    for (int i = 0; i < NDEF; i++)
        if (p == &g_world.defs[i])
            return (DWORD)i + 1;
    return p ? 0x99 : 0;
}
DWORD part_id(const void *p) { return p == &g_world.part ? 1 : p == &g_world.part2 ? 2 : p ? 0x99 : 0; }

// The recorders
CInventoryItem *THISCALL f_get_item(CInventory *inv, DWORD i)
{
    CInventoryItem *r = at<CInventoryItem *(THISCALL *)(CInventory *, DWORD)>(0x425990)(inv, i);
    ev(0x26, i, (DWORD)item_id(r));
    return r;
}
DWORD THISCALL f_item_delete(FakeItem *it, DWORD flags) { ev(0x101, it->id, flags); return result(1); }
DWORD THISCALL f_item_29(FakeItem *it, DWORD a, CInventory *inv, DWORD b) { ev(0x129, it->id, a ^ ptrv(inv), b); return result(0x29); }
DWORD THISCALL f_item_30(FakeItem *it, DWORD a, CInventory *inv, DWORD b) { ev(0x130, it->id, a ^ ptrv(inv), b); return result(0x30); }
DWORD THISCALL f_item_32(FakeItem *it, void *part) { ev(0x132, it->id, part_id(part)); return result(it->accept); }
DWORD THISCALL f_item_select(FakeItem *it, CInventory *inv)
{
    ev(0x144, it->id, inv == &g_world.inv);
    it->base.selected = 1;
    return result(0x44);
}
DWORD THISCALL f_item_deselect(FakeItem *it, CInventory *inv)
{
    ev(0x145, it->id, inv == &g_world.inv);
    it->base.selected = 0;
    return result(0x45);
}
DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
DWORD THISCALL f_list_remove_all(ZPtrArray *a) { ev(0x201, a->count); return result(0x21); }
DWORD THISCALL f_list_add(ZPtrArray *, void *const *p) { ev(0x202, (DWORD)item_id(*p)); return result(0x22); }
CClassInfo *THISCALL f_def_class(FakeDef *d) { ev(0x302, d->id); return d->cls; }
DWORD THISCALL f_def_24(FakeDef *d, void *part) { ev(0x324, d->id, part_id(part)); return result(0x24); }
DWORD THISCALL f_def_25(FakeDef *d, void *part, CInventory *inv, void *item, DWORD x)
{
    ev(0x325, d->id, part_id(part) * 0x100 + (inv == &g_world.inv), (DWORD)item_id(item) * 0x100 + x);
    return result(d->accept);
}
DWORD THISCALL f_def_35(FakeDef *d, void *item, DWORD a) { ev(0x335, d->id, (DWORD)item_id(item), a); return result(0x35); }
DWORD THISCALL f_def_36(FakeDef *d) { ev(0x336, d->id); return d->limit; }
DWORD THISCALL f_def_26(FakeDef *d, void *part, void *item, DWORD third)
{
    ev(0x326, d->id, part_id(part) * 0x100 + (item == &g_world.inv), (DWORD)item_id(reinterpret_cast<void *>((uintptr_t)third)));
    return result(0x26);
}
DWORD THISCALL f_part_36(FakePart *p) { ev(0x436, p->id); return p->inventory; }
CClassInfo *THISCALL f_part_class(FakePart *p) { ev(0x402, p->id); return p->cls; }
CClassInfo *THISCALL f_object_class(FakeObject *o) { ev(0x502, o == &g_world.game); return o->cls; }
DWORD THISCALL f_report(void *, const char *text) { ev(0x616, fnv_text(2166136261u, text)); return result(0x16); }
DWORD THISCALL f_report17(void *, const char *text) { ev(0x617, fnv_text(2166136261u, text)); return result(0x17); }

// Stand-ins for the functions of other subsystems (patched in during a case)
DWORD THISCALL s_choose(CInventory *inv, void *item, void *part, DWORD x)
{
    ev(0x701, inv == &g_world.inv, (DWORD)item_id(item), part_id(part) * 0x100 + x);
    return result(0x01);
}
DWORD g_sound_index;
DWORD THISCALL s_sound_find(void *list, const char *name, int hint)
{
    ev(0x702, list == at<void *>(0x713708), fnv_text(2166136261u, name), (DWORD)hint);
    return g_sound_index;
}
DWORD g_sound_resource[4];
void *THISCALL s_sound_get(void *list, DWORD i) { ev(0x703, list == at<void *>(0x713708), i); return g_sound_resource; }
DWORD g_sound_valid;
DWORD THISCALL s_sound_valid(void *res) { ev(0x704, res == g_sound_resource); return result(g_sound_valid); }
DWORD THISCALL s_layer_add(void *part, DWORD i) { ev(0x705, part_id(part), i); return result(0x05); }
DWORD g_game_mode;
void *THISCALL s_game(void *app) { ev(0x706, app == *at<void **>(0x713f14)); return g_game_mode ? &g_world.game : nullptr; }
DWORD g_stat_mode;
void *THISCALL s_stat(void *game, DWORD i) { ev(0x707, game == &g_world.game, i); return g_stat_mode ? &g_world.stat : nullptr; }
void *__cdecl s_stat_source() { ev(0x708); return g_world.s_c; }
DWORD THISCALL s_stat_add(void *stat, DWORD value, DWORD n) { ev(0x709, stat == &g_world.stat, value, n); return result(0x09); }
DWORD THISCALL s_def_28(void *def, void *item, DWORD a, DWORD b, DWORD c, DWORD d)
{
    ev(0x710, def_id(def), (DWORD)item_id(item), a ^ (b << 8));
    ev(0x711, c, d);
    return result(0x10);
}
DWORD THISCALL s_before(void *def, DWORD a, DWORD b, DWORD c) { ev(0x712, def_id(def), a, b ^ (c << 16)); return result(0x12); }
DWORD THISCALL s_after(void *def, DWORD a, DWORD b, DWORD c) { ev(0x713, def_id(def), a, b ^ (c << 16)); return result(0x13); }
DWORD THISCALL s_sequence(void *def, DWORD a) { ev(0x714, def_id(def), a); return result(0x14); }
DWORD THISCALL s_def_31(void *def, void *item, DWORD a) { ev(0x715, def_id(def), (DWORD)item_id(item), a); return result(0x15); }
DWORD THISCALL s_def_34(void *def, DWORD a, void *item, DWORD b) { ev(0x716, def_id(def), (DWORD)item_id(item), a ^ (b << 4)); return result(0x16); }
const char *THISCALL s_part_name(void *part) { ev(0x717, part_id(part)); return "Fake Part"; }
DWORD g_local;
DWORD __fastcall s_counter(void *part, DWORD n) { ev(0x71b, part_id(part), n); return result(0x1b); }
void *__fastcall s_player(void *part, DWORD x) { ev(0x719, part_id(part), x); return &g_world.part2; }
DWORD THISCALL s_is_local(void *app, void *player) { ev(0x71a, app == *at<void **>(0x713f14), player == &g_world.part2); return result(g_local); }
// Object_Clone: the clone is a copy of the item in the world's clone slot
void *THISCALL s_clone(FakeItem *o, DWORD a, DWORD b)
{
    ev(0x718, (DWORD)item_id(o), a, b);
    if (o && o != &g_world.clone)
        memcpy(&g_world.clone, o, sizeof g_world.clone);
    g_world.clone.id = 0x70;
    return &g_world.clone;
}

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x425a70, (void *)&s_choose},      {0x57e690, (void *)&s_sound_find}, {0x57e650, (void *)&s_sound_get},
    {0x57b740, (void *)&s_sound_valid}, {0x4fc8a0, (void *)&s_layer_add},  {0x59ff80, (void *)&s_game},
    {0x552940, (void *)&s_stat},        {0x5525b0, (void *)&s_stat_source}, {0x555930, (void *)&s_stat_add},
    {0x5b7a60, (void *)&s_def_28},      {0x5b8a80, (void *)&s_before},     {0x5b8ac0, (void *)&s_after},
    {0x4d6540, (void *)&s_sequence},    {0x5b7e20, (void *)&s_def_31},     {0x5b7ab0, (void *)&s_def_34},
    {0x4fbf20, (void *)&s_part_name},   {0x416760, (void *)&s_clone},      {0x5babc0, (void *)&s_player},
    {0x59e6f0, (void *)&s_is_local},    {0x58fda0, (void *)&s_counter},     {0x58fdf0, (void *)&s_counter},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_stand_patches[NSTANDS];
DWORD g_tested;   // the function of the case: not replaced by its stand-in

void init_vtables()
{
    memcpy(g_inv_vtable, at<void *>(INVENTORY_VTABLE), sizeof(void *) * 29);
    for (int i = 29; i < 64; i++)
        g_inv_vtable[i] = (void *)&f_bad;
    g_inv_vtable[26] = (void *)&f_get_item;
    for (int k = 0; k < 2; k++) {
        const int n = k ? 54 : 50;
        memcpy(g_item_vtable[k], at<void *>(k ? COUNT_ITEM_VTABLE : INVENTORY_ITEM_VTABLE), sizeof(void *) * (DWORD)n);
        for (int i = n; i < 64; i++)
            g_item_vtable[k][i] = (void *)&f_bad;
        g_item_vtable[k][1] = (void *)&f_item_delete;
    }
    for (int i = 0; i < 4; i++)
        g_list_vtable[i] = (void *)&f_bad;
    g_list_vtable[1] = (void *)&f_list_remove_all;
    g_list_vtable[2] = (void *)&f_list_add;
    for (int i = 0; i < 40; i++)
        g_def_vtable[i] = g_part_vtable[i] = (void *)&f_bad;
    g_def_vtable[2] = (void *)&f_def_class;
    g_def_vtable[24] = (void *)&f_def_24;
    g_def_vtable[26] = (void *)&f_def_26;
    g_def_vtable[25] = (void *)&f_def_25;
    g_def_vtable[35] = (void *)&f_def_35;
    g_def_vtable[36] = (void *)&f_def_36;
    g_part_vtable[36] = (void *)&f_part_36;
    g_part_vtable[2] = (void *)&f_part_class;
    for (int i = 0; i < 4; i++)
        g_object_vtable[i] = (void *)&f_bad;
    g_object_vtable[2] = (void *)&f_object_class;
    for (int i = 0; i < 20; i++)
        g_reporter_vtable[i] = (void *)&f_bad;
    g_reporter_vtable[16] = (void *)&f_report;
    g_reporter_vtable[17] = (void *)&f_report17;   // Repair's report
    g_reporter = ptrv(g_reporter_vtable);
}
// The items' vtables for a case: inventory tests record the slots the inventory calls
void set_item_slots(bool inventory)
{
    for (int k = 0; k < 2; k++) {
        const DWORD *real = at<DWORD *>(k ? COUNT_ITEM_VTABLE : INVENTORY_ITEM_VTABLE);
        g_item_vtable[k][29] = inventory ? (void *)&f_item_29 : at<void *>(real[29]);
        g_item_vtable[k][30] = inventory ? (void *)&f_item_30 : at<void *>(real[30]);
        g_item_vtable[k][32] = inventory ? (void *)&f_item_32 : at<void *>(real[32]);
        g_item_vtable[k][44] = inventory ? (void *)&f_item_select : at<void *>(real[44]);
        g_item_vtable[k][45] = inventory ? (void *)&f_item_deselect : at<void *>(real[45]);
    }
}

DWORD g_ore, g_crystals;
CClassInfo *g_char_class;
CClassInfo *g_zax_def_class, *g_object_class, *g_quest_class, *g_stat_class, *g_count_class, *g_item_class;
bool obj_is_kind(const void *o, CClassInfo *c) { return c && (at<DWORD(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c) & 0xff); }

DWORD pick_index(DWORD count)
{
    const DWORD r = random() >> 8;
    switch (r % 6) {
    case 0: return 0xffffffff;
    case 1: return count + (r >> 4) % 3;
    default: return count ? (r >> 4) % count : 0xffffffff;
    }
}
float small_float()
{
    const DWORD r = random() >> 8;
    switch (r % 9) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: { DWORD nan = 0x7fc00000u | (r >> 8 & 0xff); float f; memcpy(&f, &nan, 4); return f; }
    case 3: return -(float)(int)(r >> 12 & 0xff) / 32.0f;
    default: return (float)(int)(r >> 12 & 0x3ff) / 64.0f;
    }
}

bool g_null_defs = true;
void setup(unsigned seed, bool need_item)
{
    g_seed = seed;
    World &w = g_world;
    memset(&w, 0, sizeof w);
    for (int d = 0; d < NDEF; d++) {
        Record &r = w.records[d];
        for (int k = 0; k < 8; k++)
            r.name[k] = (BYTE)('A' + d);
        r.object = &w.defs[d];
        r.loading = random() % 13 == 0 && g_null_defs;
        FakeDef &def = w.defs[d];
        def.vftable = ptrv(g_def_vtable);
        def.id = (DWORD)d + 1;
        const DWORD c = random() >> 10;
        def_dw(def, 0x18) = c % 6;                            // category
        const float delay = small_float();
        memcpy(&def_dw(def, 0x48), &delay, 4);               // "Reuse Delay"
        def_dw(def, 0xfc) = (random() >> 12) % 4;              // sort order
        def.cls = (random() >> 9) % 3 ? g_zax_def_class : g_object_class;
        def.accept = (random() >> 10) % 3 != 0;
        def.limit = (random() >> 9) % 3 == 0 ? 0 : (random() >> 9) % 4 == 0 ? 0xffffffff : (random() >> 9) % 60;
        def_dw(def, 0x2c) = (random() >> 9) % 3 == 0 ? (random() >> 9) % NDEF : 0;           // ammunition
        def_dw(def, 0x1c) = (random() >> 9) % 2 ? 0 : ptrv(&w.items[(random() >> 9) % MAXI]);   // default item
    }
    // a weapon's ammunition must be a CZaxInventoryItemDefinition (the original reads its default item)
    for (int d = 0; d < NDEF; d++)
        if (w.defs[def_dw(w.defs[d], 0x2c)].cls != g_zax_def_class)
            def_dw(w.defs[d], 0x2c) = 0;
    const DWORD n = need_item ? 1 + (random() >> 9) % MAXI : (random() >> 9) % (MAXI + 1);
    for (int i = 0; i <= MAXI; i++) {
        FakeItem &it = w.items[i];
        const int k = (int)((random() >> 9) % 2);
        it.base.vftable = ptrv(g_item_vtable[k]);
        it.base.definition = (random() >> 7) % (NDEF + 2);   // beyond the count means record 0
        const float delay = small_float();
        memcpy(&it.base.current_delay, &delay, 4);
        it.base.selected = (random() >> 13) % 5 == 0;
        it.base.marked_for_deletion = (random() >> 13) % 4 == 0;
        it.base.broadcaster = nullptr;
        it.base.rounds = (int)((random() >> 9) % 80) - 5;
        it.base.max_rounds = (random() >> 9) % 3 == 0 ? -1 : (int)((random() >> 9) % 60);
        it.base.pickup_if_full = (random() >> 13) % 4 == 0;
        it.id = (DWORD)i + 1;
        it.accept = (random() >> 13) % 4 != 0;
    }
    for (DWORD i = 0; i < n; i++)
        w.data[i] = (random() >> 8) % 9 == 0 ? &w.items[(random() >> 8) % MAXI] : &w.items[i];
    for (DWORD k = 0; k < NTYPES; k++) {
        CItemTypeData &t = w.types[k];
        t.vftable = 0x5ee064;
        t.cur_index = pick_index(n);
        t.changing_to = pick_index(n);
        t.seconds_into_change = (float)(int)(random() >> 20) / 64.0f;
        t.seconds_to_change = (float)(int)(random() >> 20) / 16.0f;
    }
    const DWORD nd = (random() >> 9) % 9;
    for (DWORD k = 0; k < nd; k++)
        w.discovered[k] = &w.defs[(random() >> 9) % NDEF];
    const DWORD ndis = (random() >> 9) % 4;
    for (DWORD k = 0; k < ndis; k++)
        w.disposed[k] = (random() >> 9) % 5 == 0 ? nullptr : &w.items[(random() >> 9) % (MAXI + 1)];
    CInventory &inv = w.inv;
    inv.vftable = ptrv(g_inv_vtable);
    inv.unknown4 = 0x1234;
    inv.inited = (random() >> 12) % 3 != 0;
    inv.focus = pick_index(n);
    inv.types = w.types;
    inv.num_types = 1 + (random() >> 11) % 7;
    inv.items = {0x5ee144, w.data, n, CAP};
    inv.discovered = {0x5ee138, w.discovered, nd, CAP};
    inv.disposed = {ptrv(g_list_vtable), w.disposed, ndis, CAP};
    FakePart *const parts[2] = {&w.part, &w.part2};
    for (FakePart *p : parts) {
        p->vftable = ptrv(g_part_vtable);
        p->categories = random() >> (random() >> 27);
        p->id = part_id(p);
        p->inventory = (random() >> 9) % 4 == 0 ? 0 : ptrv(&inv);
        p->cls = (random() >> 9) % 3 ? g_char_class : g_object_class;
    }
    w.game = {ptrv(g_object_vtable), (random() >> 9) % 4 == 0 ? g_object_class : g_quest_class};
    w.stat = {ptrv(g_object_vtable), (random() >> 9) % 4 == 0 ? g_object_class : g_stat_class};
    w.s_c[3] = ptrv(w.s_t);
    w.s_t[8] = random();
    g_sound_index = random() % 50;
    g_sound_valid = (random() >> 9) % 2;
    g_game_mode = (random() >> 9) % 5;
    g_stat_mode = (random() >> 9) % 4;
    DWORD *m = reinterpret_cast<DWORD *>(w.message);
    m[1] = (random() >> 9) % 3 ? *at<DWORD *>(0x6c2ccc) : random();
    m[3] = random();
    m[4] = random();
}

// ---- Globals swapped and stand-ins patched during a case ---------------------------------------------

struct Globals {
    DWORD data, count, reporter, mask, sound;
    BYTE guard;
};
Globals g_saved;
void swap_in(unsigned seed)
{
    g_saved = {*at<DWORD *>(ITEM_CATALOGUE_DATA), *at<DWORD *>(ITEM_CATALOGUE_COUNT), *at<DWORD *>(0x6c02cc), *at<DWORD *>(0x6c29a0),
               *at<DWORD *>(0x62e9f0), *at<BYTE *>(0x62e9ec)};
    *at<DWORD *>(ITEM_CATALOGUE_DATA) = seed % 17 == 3 && g_null_defs ? 0 : ptrv(g_world.records);
    *at<DWORD *>(ITEM_CATALOGUE_COUNT) = NDEF;
    *at<DWORD *>(0x6c02cc) = ptrv(&g_reporter);
    *at<DWORD *>(0x6c29a0) = seed % 5 == 0 ? 0 : 0xffffffffu >> (seed % 7);
    *at<BYTE *>(0x62e9ec) = (BYTE)(seed % 3 == 0 ? 0 : 1);
    *at<DWORD *>(0x62e9f0) = 0x40 + seed % 9;
    for (int i = 0; i < NSTANDS; i++)
        if (STANDS[i].address != g_tested)
            patch_jump(g_stand_patches[i], STANDS[i].address, STANDS[i].fn);
}
void swap_out()
{
    for (int i = NSTANDS - 1; i >= 0; i--)
        if (STANDS[i].address != g_tested)
            unpatch(g_stand_patches[i]);
    *at<DWORD *>(ITEM_CATALOGUE_DATA) = g_saved.data;
    *at<DWORD *>(ITEM_CATALOGUE_COUNT) = g_saved.count;
    *at<DWORD *>(0x6c02cc) = g_saved.reporter;
    *at<DWORD *>(0x6c29a0) = g_saved.mask;
    *at<DWORD *>(0x62e9f0) = g_saved.sound;
    *at<BYTE *>(0x62e9ec) = g_saved.guard;
}

// ---- The functions on the fake world ------------------------------------------------------------------

struct Fn {
    DWORD address;
    const char *name;
    bool inventory;     // this = the inventory (else an item)
    bool eax;           // eax is compared
    bool need_item;
    bool null_defs = true;   // definitions may resolve to null (records being loaded, no catalogue)
};
const Fn FNS[] = {
    {0x424ed0, "Clear", true, true, false, false},
    {0x424f60, "ItemAt", true, true, false},
    {0x424f80, "Slot18", true, true, false, false},   // Repair reads every definition
    {0x424f90, "Slot23", true, true, false},
    {0x424fd0, "Update", true, true, false, false},
    {0x425590, "ChangeTo", true, true, false, false},   // Repair (inited 0) reads every definition
    {0x4256c0, "CycleNext", true, true, false, false},
    {0x425700, "CyclePrev", true, true, false, false},
    {0x4259b0, "AddItem", true, true, false, false},
    {0x426690, "Discover", true, true, false},
    {0x429ea0, "Item Ready", false, true, false},
    {0x42a340, "Item Slot28", false, true, false},
    {0x42a370, "Item Select", false, false, false},
    {0x42a380, "Item Deselect", false, false, false},
    {0x42a390, "Item SameKind", false, true, false},
    {0x42a3b0, "Item Before", false, true, false},
    {0x42a3f0, "Item After", false, true, false},
    {0x42a430, "Item Slot35", false, true, false},
    {0x42a450, "Item CountDown", false, false, false},
    {0x42a480, "Item StartDelay", false, true, false},
    {0x42a4a0, "Item Slot38", false, true, false},
    {0x42a4d0, "Item Slot31", false, true, false},
    {0x42a500, "Item CanAdd", false, true, false, false},
    {0x42a720, "Item Message", false, true, false, false},
    {0x42a830, "Count AddRounds", false, true, false},
    {0x42a850, "Count SetRounds", false, true, false},
    {0x42a890, "Count TakeRounds", false, true, false},
    {0x42a8c0, "Count Slot53", false, true, false},
    {0x42afa0, "Count Limit", false, true, false},
    {0x42adb0, "Count CanAdd", false, true, true, false},
    {0x42a550, "Item Take", false, true, false, false},
    {0x42aff0, "Count Take", false, true, false, false},
    {0x425a70, "Choose", true, true, true, false},
    {0x426930, "Part UseItems", true, true, false, true},
};
constexpr int NFNS = sizeof FNS / sizeof FNS[0];

using T0 = DWORD(THISCALL *)(void *);
using T1 = DWORD(THISCALL *)(void *, DWORD);
using T2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
using T3 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD);
using T4 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD);

struct Args {
    DWORD a, b, c, d;
    void *self;
};
DWORD fbits(float f) { return bits(f); }
Args draw_args(int f)
{
    World &w = g_world;
    const DWORD n = w.inv.items.count;
    const DWORD r = random() >> 6;
    Args x = {random(), random(), random(), random(), &w.inv};
    void *part = (r & 7) == 0 ? &w.part2 : &w.part;
    switch (f) {
    case 1: x.a = pick_index(n); break;
    case 4: x.b = fbits(small_float()); break;
    case 5: x.a = n ? (r >> 1) % n : 0; x.b = (r >> 3) % 7; x.c = ptrv(part); x.d = (r >> 6) % 3; break;
    case 6: case 7: x.a = (r >> 3) % 7; x.b = ptrv(part); x.c = (r >> 6) % 3; break;
    case 8: x.a = ptrv(&w.items[(r >> 3) % (MAXI + 1)]); x.b = ptrv(part); break;
    case 9: x.a = (r >> 3) % 5 == 0 ? ptrv(&w.items[0]) : ptrv(&w.defs[(r >> 6) % NDEF]); break;
    case 33: {
        g_world.part.inventory = ptrv(&w.inv);
        x.self = &w.part;
        const DWORD k = (r >> 3) % 5;
        x.a = k == 0 ? g_ore : k == 1 ? g_crystals : (r >> 6) % (NDEF + 2);
        x.b = (r >> 9) % 4;
        break;
    }
    case 32: {
        // the item's category always has a current item (AddItem made it so; the original reads its
        // order), and the other categories' are items or -1
        for (DWORD k = 0; k < NTYPES; k++)
            w.types[k].cur_index = n && random() % 5 ? random() % n : 0xffffffff;
        x.a = n && (r >> 3) % 6 ? ptrv(w.data[(r >> 4) % n]) : ptrv(&w.items[(r >> 4) % (MAXI + 1)]);
        {
            const DWORD di = static_cast<FakeItem *>(reinterpret_cast<void *>((uintptr_t)x.a))->base.definition;
            const DWORD c = def_dw(w.defs[di < NDEF ? di : 0], 0x18);   // beyond the count: record 0
            if (n && w.types[c].cur_index == 0xffffffff)
                w.types[c].cur_index = 0;
        }
        x.b = ptrv(part);
        x.c = 0;
        if ((r >> 8) % 3 == 0) {
            const DWORD k = (r >> 10) % 8;
            for (DWORD i = 0; i < k; i++)
                w.cand_data[i] = reinterpret_cast<void *>((uintptr_t)(n ? random() % n : 0));
            w.cand = {0x5ee168, w.cand_data, k, 8};
            x.c = ptrv(&w.cand);
        }
        g_local = random() % 5 != 0;
        break;
    }
    default: break;
    }
    if (!FNS[f].inventory) {
        FakeItem &it = w.items[(r >> 9) % (MAXI + 1)];
        x.self = &it;
        switch (f) {
        case 14: x.a = ptrv(&w.items[(r >> 3) % (MAXI + 1)]); break;
        case 15: case 16: x.a = ptrv(&w.items[(r >> 3) % (MAXI + 1)]); x.b = pick_index(n); x.c = pick_index(n); break;
        case 18: x.c = fbits(small_float()); break;
        case 22: case 29: x.a = ptrv(part); break;
        case 30: case 31: x.b = ptrv(part); break;
        case 23: x.b = ptrv(w.message); break;
        case 24: case 25: case 26: case 27: x.a = (DWORD)((int)(r >> 3) % 90 - 10); x.b = (DWORD)((int)(r >> 9) % 90 - 10); break;
        default: break;
        }
        if (f >= 24 && f != 30) {   // the CCountInventoryItem functions: an item of that class
            it.base.vftable = ptrv(g_item_vtable[1]);
            if (f == 26)
                x.b = x.a, x.a = random();
        }
    }
    return x;
}

DWORD call(int f, const Args &x)
{
    const DWORD a = FNS[f].address;
    void *s = x.self;
    switch (f) {
    case 0: case 28: return at<T0>(a)(s);
    case 1: case 2: case 9: case 10: case 12: case 13: case 14: case 17: case 19: case 20: case 21: case 22: case 24: case 25:
    case 27: case 29:
        return at<T1>(a)(s, x.a);
    case 3: case 4: case 8: case 23: case 26: case 30: case 31: return at<T2>(a)(s, x.a, x.b);
    case 6: case 7: case 15: case 16: case 18: case 32: return at<T3>(a)(s, x.a, x.b, x.c);
    case 33: return at<DWORD(__fastcall *)(void *, DWORD, DWORD)>(a)(s, x.a, x.b);
    case 5: case 11: return at<T4>(a)(s, x.a, x.b, x.c, x.d);
    default: return 0;
    }
}

void run_case(Tally &t, int f, unsigned seed)
{
    DWORD r[2];
    unsigned h[2];
    for (int side = 0; side < 2; side++) {
        g_side = side;
        log_reset(side);
        set_item_slots(FNS[f].inventory);
        g_null_defs = FNS[f].null_defs;
        g_tested = FNS[f].address;
        setup(seed, FNS[f].need_item);
        const Args x = draw_args(f);
        // Clear frees the disposed list's data to the pool: a pool block then, tagged after the call
        void **block = nullptr;
        if (f == 0) {
            block = static_cast<void **>(pool_get_alloc(CAP * 4));
            memcpy(block, g_world.disposed, CAP * 4);
            g_world.inv.disposed.data = block;
        }
        side_begin(side);   // first: taking the hooks out puts the original bytes back where stand-ins go
        swap_in(seed);
        r[side] = call(f, x);
        swap_out();
        side_end(side);
        if (block) {
            ZPtrArray &d = g_world.inv.disposed;
            if (d.data == block)
                pool_get_free(block, CAP * 4);
            d.data = reinterpret_cast<void **>((uintptr_t)(d.data == block ? 1 : d.data ? 2 : 0));
        }
        h[side] = fnv(2166136261u, &g_world, sizeof g_world);
    }
    if (!FNS[f].eax)
        r[0] = r[1] = 0;
    char d[160];
    wsprintfA(d, "[%s seed %u: eax %x / %x, log %d / %d, world %x / %x]", FNS[f].name, seed, r[0], r[1], g_nlog[0] / 4, g_nlog[1] / 4,
              h[0], h[1]);
    const bool same = r[0] == r[1] && same_log() && h[0] == h[1];
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, "inventory items", d);
}

// ---- Lifetime: constructors and destructors called directly -------------------------------------------------

// CInventory's constructor on a filled buffer (the category data from the arena), then its destructor
// with fake items in the lists (their deletion recorded)
void test_inventory_lifetime(Tally &t, unsigned seed)
{
    DWORD r[2], e[2];
    for (int side = 0; side < 2; side++) {
        g_side = side;
        log_reset(side);
        set_item_slots(true);
        g_null_defs = true;
        setup(seed, false);
        side_begin(side);
        arena_on(side, (BYTE)(seed * 7));
        CInventory *o = static_cast<CInventory *>(pool_alloc(0x48));
        memset(o, (BYTE)(seed * 13), 0x48);
        e[side] = at<CInventory *(THISCALL *)(CInventory *)>(0x424b30)(o) == o;
        memcpy(g_obj[side], o, 0x48);
        const DWORD n = random() % 6;
        for (DWORD i = 0; i < n; i++) {
            void *it = &g_world.items[random() % (MAXI + 1)];
            vcall<DWORD>(&o->items, 2, &it);
            if (random() % 3 == 0)
                vcall<DWORD>(&o->disposed, 2, &it);
            if (random() % 4 == 0)
                vcall<DWORD>(&o->discovered, 2, &g_world.defs[random() % NDEF]);
        }
        swap_in(seed);
        if (seed & 1)
            r[side] = rel(at<DWORD(THISCALL *)(CInventory *)>(0x424c40)(o), side);
        else
            r[side] = rel(ptrv(vcall<void *>(o, 1, (DWORD)0)), side) == rel(ptrv(o), side);
        swap_out();
        arena_off();
        pool_free(o, 0x48);
        side_end(side);
    }
    char d[160];
    wsprintfA(d, "[CInventory constructor/destructor seed %u: eax %x / %x, log %d / %d, +%x: %x / %x]", seed, r[0], r[1], g_nlog[0] / 4,
              g_nlog[1] / 4, g_diff_at, g_diff_a, g_diff_b);
    const bool same = e[0] && e[1] && r[0] == r[1] && same_log() && same_region(g_obj[0], g_obj[1], 0x48) &&
                      same_region(g_arena[0], g_arena[1], g_used[0]);
    if (!same && t.mismatches < 3)
        log_first_difference();
    t.check(same, "CInventory", d);
}

// CInventoryItem's two constructors and destructor on a filled buffer (with and without a broadcaster
// without listeners)
void test_item_lifetime(Tally &t, unsigned seed)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        BYTE *o = static_cast<BYTE *>(pool_alloc(0x18));
        memset(o, (BYTE)random(), 0x18);
        const DWORD definition = random();
        BYTE *made = (seed & 1) ? at<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x42a2d0)(o, definition)
                                : at<BYTE *(THISCALL *)(BYTE *)>(0x42a2b0)(o);
        memcpy(g_obj[side], o, 0x18);
        // the broadcaster is an owned field: a real one without listeners, deleted by FreeFields
        void *b = nullptr;
        if (seed & 2) {
            CClassInfo *bc = at<CClassInfo *(__cdecl *)()>(0x467830)();
            b = bc->create(0, 0);
        }
        *reinterpret_cast<void **>(o + 0x14) = b;
        r[side] = made == o;
        g_eax[side] = at<DWORD(THISCALL *)(BYTE *)>(0x42a300)(o);
        memcpy(g_obj[side] + 0x18, o, 0x18);
        DWORD &held = *reinterpret_cast<DWORD *>(g_obj[side] + 0x18 + 0x14);
        held = held == ptrv(b) ? (b ? 1 : 0) : 2;
        pool_free(o, 0x18);
        side_end(side);
    }
    char d[140];
    wsprintfA(d, "[CInventoryItem constructor/destructor seed %u: this %x / %x, eax %x / %x, +%x: %x / %x]", seed, r[0], r[1], g_eax[0],
              g_eax[1], g_diff_at, g_diff_a, g_diff_b);
    t.check(r[0] == r[1] && r[0] == 1 && g_eax[0] == g_eax[1] && same_region(g_obj[0], g_obj[1], 0x30), "CInventoryItem", d);
}

// The three page option destructors on pages made like the constructor makes them
void test_page_dtors(Tally &t, unsigned seed)
{
    static const DWORD DTOR[3] = {0x427110, 0x427130, 0x427160};
    static const DWORD VT[3] = {0x5ee2e4, 0x5ee288, 0x5ee22c};
    static const DWORD SIZE[3] = {0xa8, 0x60, 0x9c};
    static const DWORD ENTRIES[5] = {0x18, 0x38, 0x44, 0x50, 0x90};
    const int k = (int)(seed % 3);
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        BYTE *p = static_cast<BYTE *>(pool_alloc(SIZE[k]));
        memset(p, (BYTE)seed, SIZE[k]);
        at<void *(THISCALL *)(void *)>(0x416610)(p);
        if (k == 0) {
            at<void *(THISCALL *)(void *)>(0x53f000)(p + 0x88);
            at<void *(THISCALL *)(void *)>(0x53f000)(p + 0x98);
        } else
            for (int i = 0; i < (k == 1 ? 1 : 5); i++)
                at<void *(THISCALL *)(void *)>(0x500740)(p + ENTRIES[i]);
        *reinterpret_cast<DWORD *>(p) = VT[k];
        at<void(THISCALL *)(void *)>(0x4173c0)(p);
        r[side] = at<DWORD(THISCALL *)(void *)>(DTOR[k])(p) & 0;
        memcpy(g_obj[side], p, SIZE[k]);
        pool_free(p, SIZE[k]);
        side_end(side);
    }
    char d[120];
    wsprintfA(d, "[page destructor %x seed %u: +%x: %x / %x]", DTOR[k], seed, g_diff_at, g_diff_a, g_diff_b);
    t.check(r[0] == r[1] && same_region(g_obj[0], g_obj[1], SIZE[k]), "page options", d);
}

// 0x426b90 on a heap array (data from the arena)
void test_reserve(Tally &t, unsigned seed)
{
    DWORD r[2];
    ZPtrArray a[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        arena_on(side, 0x5a);
        const DWORD count = random() % 6, capacity = count + random() % 4;
        a[side] = {0x5ee168, nullptr, count, capacity};
        if (capacity && random() % 4) {
            a[side].data = static_cast<void **>(heap_alloc(capacity * 4));
            for (DWORD i = 0; i < count; i++)
                a[side].data[i] = reinterpret_cast<void *>((uintptr_t)random());
        } else
            a[side].count = 0;   // entries without data: the original copies from null
        const DWORD n = random() % 12;
        r[side] = rel(at<DWORD(THISCALL *)(ZPtrArray *, DWORD)>(0x426b90)(&a[side], n), side);
        arena_off();
        side_end(side);
    }
    char d[120];
    wsprintfA(d, "[Reserve seed %u: eax %x / %x, %u / %u bytes]", seed, r[0], r[1], g_used[0], g_used[1]);
    t.check(r[0] == r[1] && same_region(reinterpret_cast<BYTE *>(&a[0]), reinterpret_cast<BYTE *>(&a[1]), sizeof a[0]) &&
                g_used[0] == g_used[1] && same_region(g_arena[0], g_arena[1], g_used[0]),
            "HeapArray_Reserve", d);
}

// ---- Real data ---------------------------------------------------------------------------------------

template <typename C> void two_sides(Tally &t, const char *what, DWORD arg, C call_it)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        r[side] = call_it();
        side_end(side);
    }
    char d[120];
    wsprintfA(d, "[real %s %x: %x / %x]", what, arg, r[0], r[1]);
    t.check(r[0] == r[1], "inventory items", d);
}

// Read-only slots of a real item (the definition's sequence choice and order through the real code)
void real_item(Tally &t, BYTE *it, BYTE *other, DWORD i)
{
    two_sides(t, "Ready", i, [&] { return at<T1>(0x429ea0)(it, 0); });
    two_sides(t, "SameKind", i, [&] { return at<T1>(0x42a390)(it, ptrv(other)); });
    two_sides(t, "Before", i, [&] { return at<T3>(0x42a3b0)(it, ptrv(other), i, i + 1) & 0xff; });
    two_sides(t, "After", i, [&] { return at<T3>(0x42a3f0)(it, ptrv(other), i + 1, i) & 0xff; });
    if (obj_is_kind(it, g_count_class))
        two_sides(t, "Limit", i, [&] { return at<T0>(0x42afa0)(it); });
}

void test_real(Tally &t, int &items, int &made)
{
    DWORD inv[2];
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        inv[side] = at<DWORD(__cdecl *)()>(0x428b10)();
        side_end(side);
    }
    t.check(inv[0] == inv[1], "inventory items", "[real player inventory]");
    CInventory *pi = reinterpret_cast<CInventory *>((uintptr_t)inv[0]);
    if (pi && readable(pi, sizeof *pi) && pi->vftable == INVENTORY_VTABLE) {
        const DWORD n = pi->items.count;
        log("  player inventory: %u items, %u categories, focus %d", n, pi->num_types, (int)pi->focus);
        for (DWORD i = 0; i < n + 2; i++)
            two_sides(t, "ItemAt", i, [&] { return at<T1>(0x424f60)(pi, i); });
        for (DWORD i = 0; i < n; i++) {
            BYTE *it = static_cast<BYTE *>(pi->items.data[i]);
            if (!it || !class_of(it))
                continue;
            items++;
            real_item(t, it, static_cast<BYTE *>(pi->items.data[(i + 1) % n]), i);
        }
    }
    // Items made from loaded definitions of the real catalogue (every tenth record loaded through the
    // original 0x4dd480, as when the player gets the item), deleted again
    const DWORD records = *at<DWORD *>(ITEM_CATALOGUE_DATA), count = *at<DWORD *>(ITEM_CATALOGUE_COUNT);
    const DWORD step = count / 12 + 1;
    BYTE *prev = nullptr;
    for (DWORD i = 0; records && i < count; i++) {
        BYTE *rec = at<BYTE *>(records + i * 0x80);
        if (i % step == 0 && !*reinterpret_cast<void **>(rec + 0x78) && !rec[0x7f]) {
            CInventoryItem probe = {};
            probe.definition = i;
            original<void *(THISCALL *)(CInventoryItem *)>(0x4dd480)(&probe);
        }
        if (!*reinterpret_cast<void **>(rec + 0x78) || rec[0x7f])
            continue;
        for (int k = 0; k < 2; k++) {
            BYTE *it = static_cast<BYTE *>(at<Create0>(k ? 0x42a8e0 : 0x429ee0)());
            if (!it)
                continue;
            *reinterpret_cast<DWORD *>(it + 8) = i;
            made++;
            two_sides(t, "StartDelay", i, [&] {
                *reinterpret_cast<DWORD *>(it + 0xc) = 0x12345;
                const DWORD e = at<T1>(0x42a480)(it, 0);
                return e ^ *reinterpret_cast<DWORD *>(it + 0xc);
            });
            for (int s = 0; s < 3; s++) {
                const float seconds = s == 0 ? 0.05f : s == 1 ? 1.0f : 100.0f;
                DWORD after[2];
                for (int side = 0; side < 2; side++) {
                    at<T1>(0x42a480)(it, 0);
                    side_begin(side);
                    at<T3>(0x42a450)(it, 0, 0, bits(seconds));
                    side_end(side);
                    after[side] = *reinterpret_cast<DWORD *>(it + 0xc);
                }
                t.check(after[0] == after[1], "inventory items", "[real CountDown]");
            }
            real_item(t, it, prev ? prev : it, i);
            if (prev)
                vcall<void *>(prev, 1, (DWORD)1);
            prev = it;
        }
    }
    if (prev)
        vcall<void *>(prev, 1, (DWORD)1);
}

}  // namespace

ZAX_CHECKS(inventory_items)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_inventory_items;
    if (!arena_setup()) {
        log("Check inventory items: no arena");
        return;
    }
    init_vtables();
    g_zax_def_class = at<CClassInfo *(__cdecl *)()>(0x5b5f10)();
    g_object_class = at<CClassInfo *(__cdecl *)()>(0x4163b0)();
    g_quest_class = at<CClassInfo *(__cdecl *)()>(0x551c20)();
    g_stat_class = at<CClassInfo *(__cdecl *)()>(0x554ae0)();
    g_count_class = *at<CClassInfo **>(COUNT_ITEM_CLASS);
    g_item_class = *at<CClassInfo **>(INVENTORY_ITEM_CLASS);
    g_char_class = at<CClassInfo *(__cdecl *)()>(0x49a930)();
    g_ore = at<DWORD(THISCALL *)(void *, const char *, int)>(0x591fc0)(at<void *>(ITEM_CATALOGUE), "Ore Deposits", -1);
    g_crystals = at<DWORD(THISCALL *)(void *, const char *, int)>(0x591fc0)(at<void *>(ITEM_CATALOGUE), "Crystals", -1);

    Tally objects{"inventory items: Create, CreateArray, destructors"}, getters{"inventory items: class getters"},
        fields{"inventory items: RegisterFields"}, registered{"inventory items: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    Tally life{"inventory items: constructors and destructors"};
    for (unsigned k = 0; k < (unsigned)scaled(60); k++) {
        if (g_trace)
            log("  trace lifetime %u", k);
        test_inventory_lifetime(life, k);
        if (g_trace)
            log("  trace item lifetime %u", k);
        test_item_lifetime(life, k);
        if (g_trace)
            log("  trace pages %u", k);
        test_page_dtors(life, k);
        if (g_trace)
            log("  trace reserve %u", k);
        test_reserve(life, k);
    }
    objects.report();
    getters.report();
    fields.report();
    life.report();
    Tally t{"inventory items: inventory and item functions on fake objects"};
    for (int f = 0; f < NFNS; f++)
        for (unsigned k = 0; k < (unsigned)scaled(120); k++) {
            if (g_trace)
                log("  trace %s %u", FNS[f].name, k);
            run_case(t, f, 5000u * (unsigned)f + k);
        }
    t.report();
    int items = 0, made = 0;
    test_real(registered, items, made);
    log("  real data: %d items of the player, %d items made from the catalogue", items, made);
    registered.report();
}

}  // namespace zax
