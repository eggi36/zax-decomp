// Checks of the game's own inventory definitions (zax_item_defs.cpp) against the original, in the game
// thread (machinery: checks_inventory_common.h).
//  - Classes: Create, CreateArray with 0-3 elements, the deleting destructors (single and array), both
//    class getters, RegisterFields into a fake class info and with the class variable 0, the registered
//    descriptors (real data); the constructors and destructors called directly (the C heap from the
//    arena); Rect_Offset.
//  - Behaviour on a fake world set up again from the same seed before each side: the item catalogue,
//    the category list, the detail and HUD sequence lists and the projectile list read from records of
//    our own (their globals swapped during a case; every record loaded or, where the original allows
//    it, "being loaded"); definitions with a copy of the real vtable whose slots 31-34 and 36 record
//    (HUD image, HUD renderer, detail image, detail renderer, "Generate/Max Count"); items with copies
//    of the real item vtables (slots 1, 37, 41 and 46 recording); fake objects (renderers, sequences
//    with a rectangle at +0xc, frame counts and rates, the continuous fire data, old actions, the
//    default items, quest objects) whose slot 2 gives the class the case needs; an inventory with the
//    real vtable; a part. The functions of other subsystems jump to recorders: the first character
//    0x4f5d80, the extra byte field 0x54de90/0x54dec0, the quest game type and its stats (0x59ff80,
//    0x552940, 0x5525b0, 0x555930), the catalogue's name search 0x591fc0, Object_Clone 0x416760, the
//    part's sound 0x4fc8a0, the icons 0x4d68a0/0x4d6b70, the display name 0x4d66d0; the error reporter
//    0x6c02cc is a fake. Compared: eax (al where the upper bytes are left over), st(0), the event log,
//    all bytes of the fake world, the objects made during a case and the arena.
//  - Real data: every record of the real item catalogue (loaded as the game loads them): the continuous
//    fire values, the button labels, the shots, the damage at several distances, the category, the energy
//    source and "may use" for the player, the order of pairs of definitions, may the player add an item
//    of it, the icon bounds (the three fields it writes saved and restored).
#include "checks_inventory_common.h"
#include "x87.h"
#include "zax_item_defs.h"
#include "zax_items.h"

namespace zax {

int hooks_zax_item_defs(Hook *out);

namespace {

using namespace invcheck;

const ClassDef CLASSES[] = {
    {"CZaxInventoryItemDefinition", 0x100, ZAX_ITEM_DEFINITION_CLASS, 0x5b5f70, 0x5b5f90, 0x5b61b0, {0x5b5f10, 0x5b6140}, true},
    {"CZaxInventoryTypeDefinition", 0x28, ZAX_TYPE_DEFINITION_CLASS, 0x5b9450, 0x5b94a0, 0x5b95f0, {0x5b9400, 0x5b9590}, true},
};

using T0 = DWORD(THISCALL *)(void *);
using T1 = DWORD(THISCALL *)(void *, DWORD);
using T2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
using T3 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD);
using T4 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD);
using T5 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD);
using L0 = long double(THISCALL *)(void *);
using L1 = long double(THISCALL *)(void *, DWORD);

// ---- Constructors and destructors called directly ------------------------------------------------------

struct Life {
    const char *name;
    DWORD size, ctor, dtor, deleting;
};
const Life LIVES[] = {
    {"CZaxInventoryItemDefinition", 0x100, 0x5b7380, 0x5b73f0, 0x5b5fe0},
    {"CZaxInventoryTypeDefinition", 0x28, 0x5b9b50, 0x5b9b30, 0x5b9510},
};
void test_lifetime(Tally &t, const Life &l, unsigned seed)
{
    DWORD r[2], e[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        arena_on(side, (BYTE)(seed * 3));
        BYTE *o = static_cast<BYTE *>(pool_alloc(l.size));
        memset(o, (BYTE)random(), l.size);
        e[side] = at<T0>(l.ctor)(o) == ptrv(o);
        memcpy(g_obj[side], o, l.size);
        if (seed & 1) {
            r[side] = at<T0>(l.dtor)(o);
            pool_free(o, l.size);
        } else
            r[side] = at<T1>(l.deleting)(o, 1) == ptrv(o);
        memcpy(g_obj[side] + l.size, o, l.size < 0x40 ? l.size : 0x40);
        arena_off();
        side_end(side);
    }
    char d[160];
    wsprintfA(d, "[%s seed %u: eax %x / %x, +%x: %x / %x]", l.name, seed, r[0], r[1], g_diff_at, g_diff_a, g_diff_b);
    t.check(e[0] && e[1] && r[0] == r[1] && same_region(g_obj[0], g_obj[1], l.size) && same_region(g_arena[0], g_arena[1], g_used[0]),
            l.name, d);
}
void test_rect_offset(Tally &t, unsigned seed)
{
    int rc[2][4];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        for (int k = 0; k < 4; k++)
            rc[side][k] = (int)random();
        const DWORD dx = random(), dy = random();
        r[side] = at<T2>(0x5b9b80)(rc[side], dx, dy);
        side_end(side);
    }
    t.check(r[0] == r[1] && memcmp(rc[0], rc[1], sizeof rc[0]) == 0, "Rect_Offset");
}

// ---- The fake world ----------------------------------------------------------------------------------

constexpr int NDEF = 8, NCAT = 6, NIMG = 6, NPROJ = 4, NITEM = 8, NOBJ = 16, NRENDERER = 6;


struct Record {
    BYTE name[0x78];
    void *object;   // 0x78
    BYTE keep, in_use, unknown7e, loading;
};
static_assert(sizeof(Record) == 0x80);

struct FakeObject {   // renderers, sequences, the continuous fire data, old actions, default items, quest objects
    DWORD vftable;
    BYTE body[0x2c];   // +0x0c a rectangle for the sequences; +0x08 the renderers' sequence index
    CClassInfo *cls;   // slot 2
    DWORD id;
    DWORD answer[4];   // what the recording slots give
};

struct World {
    Record cat_rec[NDEF];
    BYTE defs[NDEF][0x100];
    Record type_rec[NCAT];
    BYTE types[NCAT][0x28];
    Record detail_rec[NIMG];
    Record hud_rec[NIMG];
    Record proj_rec[NPROJ];
    BYTE projs[NPROJ][0x60];
    FakeObject objs[NOBJ];
    BYTE items[NITEM][0x30];
    void *data[NITEM + 2];
    CInventory inv;
    DWORD part_vftable;
    BYTE part_body[0x1c];   // +0x1c of the part: the flags (part_flags)
    DWORD part_flags;
    CClassInfo *part_cls;
    CInventory *part_inv;
    DWORD reporter_vftable;
    DWORD def_slot[NDEF][5];   // the answers of slots 31, 32, 33, 34, 36
    DWORD item_answer[NITEM][3];   // slots 37 and 46 (low byte), 41
    int icon[4];
    DWORD fruit_found, first_is_part;
    BYTE clone[0x30];
    BYTE quest_data[0x10];
    BYTE quest_data_c[0x10];
};
World g_world;

void *g_def_vtable[40];
void *g_type_vtable[24];
void *g_obj_vtable[64];        // sequences, the continuous fire data, old actions, default items, quest objects
void *g_renderer_vtable[64];   // the HUD and detail renderers (objects 0-5)
void *g_item_vt[5][64];
void *g_part_vtable[40];
void *g_reporter_vtable[20];
const DWORD ITEM_KIND_VTABLES[5] = {INVENTORY_ITEM_VTABLE, ENERGY_ITEM_VTABLE, BATTERY_ITEM_VTABLE, HEALTH_ITEM_VTABLE, AMMUNITION_ITEM_VTABLE};
CClassInfo *g_classes[12];   // the classes the fake objects can have
DWORD g_health, g_primary, g_secondary, g_none_sound, g_sounds;
ZString g_label_text[3];     // labels: empty, a text, another

int obj_id(const void *p)
{
    for (int i = 0; i < NOBJ; i++)
        if (p == &g_world.objs[i])
            return i + 1;
    for (int i = 0; i < NITEM; i++)
        if (p == g_world.items[i])
            return 0x40 + i;
    for (int i = 0; i < NDEF; i++)
        if (p == g_world.defs[i])
            return 0x60 + i;
    if (p == &g_world.inv)
        return 0x70;
    if (p == &g_world.part_vftable)
        return 0x71;
    if (p == g_world.clone)
        return 0x72;
    return p ? 0x99 : 0;
}
int def_index(const void *p)
{
    for (int i = 0; i < NDEF; i++)
        if (p == g_world.defs[i])
            return i;
    return 0;
}
int item_index(const void *p)
{
    for (int i = 0; i < NITEM; i++)
        if (p == g_world.items[i])
            return i;
    return 0;
}

// The recorders
DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
CClassInfo *THISCALL f_obj_class(FakeObject *o) { ev(0x102, (DWORD)obj_id(o)); return o->cls; }
DWORD THISCALL f_obj_delete(FakeObject *o, DWORD flags) { ev(0x101, (DWORD)obj_id(o), flags); return result(0x01); }
DWORD THISCALL f_obj_23(FakeObject *o, DWORD a, DWORD b, DWORD c) { ev(0x123, (DWORD)obj_id(o), a, b ^ (c << 8)); return result(0x23); }
DWORD THISCALL f_obj_23_1(FakeObject *o, DWORD a) { ev(0x1231, (DWORD)obj_id(o), a); return result(0x31); }
DWORD THISCALL f_obj_24(FakeObject *o, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
    ev(0x124, (DWORD)obj_id(o), a ^ b, c ^ (d << 4) ^ (e << 8));
    return result(0x24);
}
void *THISCALL f_obj_25(FakeObject *o, void *item, DWORD a) { ev(0x125, (DWORD)obj_id(o), (DWORD)obj_id(item), a); return reinterpret_cast<void *>((uintptr_t)o->answer[0]); }
DWORD THISCALL f_obj_frames(FakeObject *o) { ev(0x125, (DWORD)obj_id(o)); return o->answer[1]; }
long double THISCALL f_obj_rate(FakeObject *o) { ev(0x126, (DWORD)obj_id(o)); return fld(raw_float(o->answer[2])); }
DWORD THISCALL f_obj_50(FakeObject *o) { ev(0x150, (DWORD)obj_id(o)); return o->answer[3]; }
DWORD THISCALL f_obj_51(FakeObject *o, DWORD a) { ev(0x151, (DWORD)obj_id(o), a); return result(0x51); }
// The definitions' slots 31-34, 36 and the delete
template <int K> DWORD THISCALL f_def_slot(BYTE *d)
{
    ev(0x231 + K, (DWORD)def_index(d));
    return g_world.def_slot[def_index(d)][K];
}
DWORD THISCALL f_def_delete(BYTE *d, DWORD flags) { ev(0x201, (DWORD)def_index(d), flags); return result(0x01); }
// The items' slots
DWORD THISCALL f_item_delete(BYTE *it, DWORD flags) { ev(0x301, (DWORD)obj_id(it), flags); return result(0x01); }
DWORD THISCALL f_item_37(BYTE *it, void *part) { ev(0x337, (DWORD)obj_id(it), (DWORD)obj_id(part)); return result(g_world.item_answer[item_index(it)][0]); }
DWORD THISCALL f_item_46(BYTE *it, void *other) { ev(0x346, (DWORD)obj_id(it), (DWORD)obj_id(other)); return result(g_world.item_answer[item_index(it)][1]); }
DWORD THISCALL f_item_41(BYTE *it) { ev(0x341, (DWORD)obj_id(it)); return g_world.item_answer[item_index(it)][2]; }
CClassInfo *THISCALL f_part_class(void *) { ev(0x402); return g_world.part_cls; }
CInventory *THISCALL f_part_inventory(void *) { ev(0x436); return g_world.part_inv; }
DWORD THISCALL f_report(void *, const char *text) { ev(0x611, fnv_text(2166136261u, text)); return result(0x11); }

// Stand-ins for the functions of other subsystems
void *THISCALL s_first_character(void *world) { ev(0x701, world == *at<void **>(0x6c2080)); return g_world.first_is_part ? (void *)&g_world.part_vftable : (void *)g_world.items[0]; }
DWORD THISCALL s_byte_get(void *desc, void *o) { ev(0x702, desc == *at<void **>(0x714404), (DWORD)obj_id(o)); return result(g_world.fruit_found); }
DWORD THISCALL s_byte_set(void *desc, void *o, DWORD v) { ev(0x703, desc == *at<void **>(0x714404), (DWORD)obj_id(o), v); return result(0x03); }
void *THISCALL s_game(void *app) { ev(0x704, app == *at<void **>(0x713f14)); return g_world.objs[12].answer[0] ? &g_world.objs[12] : nullptr; }
void *THISCALL s_stat(void *game, DWORD i) { ev(0x705, (DWORD)obj_id(game), i); return g_world.objs[13].answer[0] ? &g_world.objs[13] : nullptr; }
void *__cdecl s_quest_data() { ev(0x706); return g_world.quest_data; }
DWORD THISCALL s_counter_add(void *stat, DWORD key, DWORD n) { ev(0x707, (DWORD)obj_id(stat), key, n); return result(0x07); }
DWORD THISCALL s_named_find(void *list, const char *name, DWORD hint) { ev(0x708, list == at<void *>(0x6c0c08), fnv_text(2166136261u, name), hint); return g_world.objs[14].answer[0]; }
void *THISCALL s_clone(BYTE *o, DWORD a, DWORD b)
{
    ev(0x709, (DWORD)obj_id(o), a, b);
    memcpy(g_world.clone, o, sizeof g_world.clone);
    return g_world.clone;
}
DWORD THISCALL s_layer_add(void *part, DWORD sound) { ev(0x70a, (DWORD)obj_id(part), sound); return result(0x0a); }
DWORD THISCALL s_load_icons(void *def) { ev(0x70b, (DWORD)obj_id(def)); return result(0x0b); }
DWORD THISCALL s_icon_bounds(void *def, int *r)
{
    ev(0x70c, (DWORD)obj_id(def));
    memcpy(r, g_world.icon, sizeof g_world.icon);
    return result(0x0c);
}
DWORD __fastcall s_display_name(DWORD def, void *out) { ev(0x70d, def, out != nullptr); return result(0x0d); }

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x4f5d80, (void *)&s_first_character}, {0x54de90, (void *)&s_byte_get},   {0x54dec0, (void *)&s_byte_set},
    {0x59ff80, (void *)&s_game},            {0x552940, (void *)&s_stat},       {0x5525b0, (void *)&s_quest_data},
    {0x555930, (void *)&s_counter_add},     {0x591fc0, (void *)&s_named_find}, {0x416760, (void *)&s_clone},
    {0x4fc8a0, (void *)&s_layer_add},       {0x4d68a0, (void *)&s_load_icons}, {0x4d6b70, (void *)&s_icon_bounds},
    {0x4d66d0, (void *)&s_display_name},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_stand_patches[NSTANDS];

// The list globals swapped during a case: records, count
struct Swap {
    DWORD address;
    DWORD value;
};
constexpr DWORD SWAPS[] = {0x6c0c38, 0x6c0c3c, 0x6c0830, 0x6c0834, 0x6c0978, 0x6c097c, 0x6c06d0, 0x6c06d4, 0x6cfe08, 0x6cfe0c, 0x6c02cc};
constexpr int NSWAPS = sizeof SWAPS / sizeof SWAPS[0];
DWORD g_saved[NSWAPS];
void swap_in()
{
    World &w = g_world;
    const DWORD values[NSWAPS] = {ptrv(w.cat_rec),  NDEF, ptrv(w.type_rec), NCAT,  ptrv(w.detail_rec),           NIMG,
                                  ptrv(w.hud_rec), NIMG, ptrv(w.proj_rec), NPROJ, ptrv(&w.reporter_vftable)};
    for (int i = 0; i < NSWAPS; i++) {
        g_saved[i] = *at<DWORD *>(SWAPS[i]);
        *at<DWORD *>(SWAPS[i]) = values[i];
    }
}
void swap_out()
{
    for (int i = NSWAPS - 1; i >= 0; i--)
        *at<DWORD *>(SWAPS[i]) = g_saved[i];
}

void init_world_tables()
{
    memcpy(g_def_vtable, at<void *>(ZAX_ITEM_DEFINITION_VTABLE), sizeof(void *) * 37);
    for (int i = 37; i < 40; i++)
        g_def_vtable[i] = (void *)&f_bad;
    g_def_vtable[1] = (void *)&f_def_delete;
    g_def_vtable[31] = (void *)&f_def_slot<0>;
    g_def_vtable[32] = (void *)&f_def_slot<1>;
    g_def_vtable[33] = (void *)&f_def_slot<2>;
    g_def_vtable[34] = (void *)&f_def_slot<3>;
    g_def_vtable[36] = (void *)&f_def_slot<4>;
    memcpy(g_type_vtable, at<void *>(ZAX_TYPE_DEFINITION_VTABLE), sizeof(void *) * 23);
    g_type_vtable[1] = (void *)&f_bad;
    for (int i = 0; i < 64; i++)
        g_obj_vtable[i] = (void *)&f_bad;
    g_obj_vtable[1] = (void *)&f_obj_delete;
    g_obj_vtable[2] = (void *)&f_obj_class;
    g_obj_vtable[23] = (void *)&f_obj_23_1;   // the sequences' load (with 0)
    g_obj_vtable[24] = (void *)&f_obj_24;
    g_obj_vtable[25] = (void *)&f_obj_frames;
    g_obj_vtable[26] = (void *)&f_obj_rate;
    g_obj_vtable[50] = (void *)&f_obj_50;
    g_obj_vtable[51] = (void *)&f_obj_51;
    for (int i = 0; i < 64; i++)
        g_renderer_vtable[i] = (void *)&f_bad;
    g_renderer_vtable[1] = (void *)&f_obj_delete;
    g_renderer_vtable[2] = (void *)&f_obj_class;
    g_renderer_vtable[23] = (void *)&f_obj_23;
    g_renderer_vtable[24] = (void *)&f_obj_24;
    g_renderer_vtable[25] = (void *)&f_obj_25;
    for (int k = 0; k < 5; k++) {
        memcpy(g_item_vt[k], at<void *>(ITEM_KIND_VTABLES[k]), sizeof(void *) * 49);
        for (int i = 49; i < 64; i++)
            g_item_vt[k][i] = (void *)&f_bad;
        g_item_vt[k][1] = (void *)&f_item_delete;
        g_item_vt[k][37] = (void *)&f_item_37;
        g_item_vt[k][41] = (void *)&f_item_41;
        g_item_vt[k][46] = (void *)&f_item_46;
    }
    for (int i = 0; i < 40; i++)
        g_part_vtable[i] = (void *)&f_bad;
    g_part_vtable[2] = (void *)&f_part_class;
    g_part_vtable[36] = (void *)&f_part_inventory;
    for (int i = 0; i < 20; i++)
        g_reporter_vtable[i] = (void *)&f_bad;
    g_reporter_vtable[17] = (void *)&f_report;
    const DWORD getters[12] = {0x4163b0, 0x472e80, 0x4dc8c0, 0x42a760, 0x5afbd0, 0x5aefe0, 0x5adb50, 0x551c20,
                               0x554ae0, 0x49a930, 0x429e50, 0x5b0ae0};
    for (int i = 0; i < 12; i++)
        g_classes[i] = at<CClassInfo *(__cdecl *)()>(getters[i])();
    auto find = [](DWORD list, const char *name) { return at<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(at<void *>(list), name, -1); };
    g_health = find(0x6c0800, "Health");
    g_primary = find(0x6c0800, "Primary");
    g_secondary = find(0x6c0800, "Secondary");
    g_none_sound = find(0x713708, "!None.wav");
    g_sounds = vcall<DWORD>(at<void *>(0x713708), 22);   // the record count RecordList_Name checks against
    at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&g_label_text[0]);
    at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&g_label_text[1], "Equip");
    at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&g_label_text[2], "Throw away");
}

DWORD rnd(DWORD n) { return (random() >> 8) % n; }
int renderer() { return (int)rnd(NRENDERER); }
int other_object() { return NRENDERER + (int)rnd(NOBJ - NRENDERER); }
float some_float()
{
    const DWORD r = random() >> 8;
    switch (r % 10) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: { DWORD nan = 0x7fc00000u | (r >> 8 & 0xff); float f; memcpy(&f, &nan, 4); return f; }
    case 3: return -(float)(int)(r >> 12 & 0xff) / 8.0f;
    case 4: return (float)(int)(r >> 12 & 0xf);
    default: return (float)(int)(r >> 12 & 0x3ff) / 16.0f;
    }
}
DWORD some_float_bits() { return float_bits(some_float()); }
// A record of a list: loaded, or (where the code allows it) sometimes being loaded
void set_record(Record &r, void *object, bool may_load, int k)
{
    for (int c = 0; c < 8; c++)
        r.name[c] = (BYTE)('a' + k + c);
    r.object = object;
    r.loading = may_load && rnd(9) == 0;
}

// The world of a case; `nulls` allows records being loaded where the original can take it
void setup(unsigned seed, bool nulls, bool nulls_images)
{
    g_seed = seed;
    World &w = g_world;
    memset(&w, 0, sizeof w);
    for (int i = 0; i < NOBJ; i++) {
        FakeObject &o = w.objs[i];
        o.vftable = i < NRENDERER ? ptrv(g_renderer_vtable) : ptrv(g_obj_vtable);
        for (DWORD k = 0; k < sizeof o.body; k++)
            o.body[k] = (BYTE)random();
        o.cls = g_classes[rnd(12)];
        o.id = (DWORD)i + 1;
        for (int k = 0; k < 4; k++)
            o.answer[k] = random();
        *reinterpret_cast<DWORD *>(&o.body[4]) = rnd(NIMG + 1);   // +8 a renderer's sequence index
        int *rect = reinterpret_cast<int *>(&o.body[8]);           // +0xc a sequence's rectangle
        for (int k = 0; k < 4; k++)
            rect[k] = (int)rnd(200) - 60;
        o.answer[0] = rnd(3) ? 0 : ptrv(&w.objs[rnd(NOBJ)]);        // slot 25 of a renderer
        o.answer[1] = 1 + rnd(40);                                  // a sequence's frame count
        o.answer[2] = float_bits((float)rnd(61));                   // its frames per second
        o.answer[3] = rnd(4) == 0 ? 0xffffffff : rnd(3) ? rnd(30) : 0;
    }
    for (int d = 0; d < NDEF; d++) {
        BYTE *def = w.defs[d];
        for (DWORD k = 0; k < 0x100; k += 4)
            *reinterpret_cast<DWORD *>(def + k) = random();
        *reinterpret_cast<DWORD *>(def) = ptrv(g_def_vtable);
        auto D = [&](DWORD off) -> DWORD & { return *reinterpret_cast<DWORD *>(def + off); };
        const DWORD r = rnd(6);
        D(0x18) = r == 0 ? g_health : r == 1 ? g_primary : r == 2 ? g_secondary : rnd(NCAT + 1);
        D(0x1c) = rnd(4) ? ptrv(&w.objs[other_object()]) : 0;
        D(0x20) = rnd(3) ? rnd(NPROJ + 1) : 0;
        D(0x2c) = rnd(3) ? rnd(NDEF + 1) : 0;
        D(0x34) = some_float_bits();
        D(0x3c) = some_float_bits();
        D(0x4c) = some_float_bits();
        D(0x50) = rnd(6);
        for (DWORD off = 0x60; off < 0x80; off += 8) {   // the labels: a dummy name and one of the texts
            D(off) = *reinterpret_cast<DWORD *>(&g_label_text[0]);
            D(off + 4) = *reinterpret_cast<DWORD *>(&g_label_text[rnd(3)]);
        }
        D(0x84) = rnd(3) ? ptrv(&w.objs[other_object()]) : 0;
        D(0x8c) = rnd(3) ? ptrv(&w.objs[other_object()]) : 0;
        D(0x98) = rnd(3) == 0 ? g_none_sound : rnd(g_sounds ? g_sounds : 1);
        D(0xcc) = rnd(3) ? ptrv(&w.objs[other_object()]) : 0;
        D(0xe0) = float_bits((float)rnd(50) - 5.0f);
        D(0xf4) = random() & 0x4f;
        // the "Required Item" chain only goes to later definitions (no cycles)
        D(0xf8) = (DWORD)d + 1 < NDEF && rnd(2) ? (DWORD)d + 1 + rnd(NDEF - (DWORD)d - 1) : 0;
        D(0xfc) = rnd(3);
        for (int k = 0; k < 5; k++)
            w.def_slot[d][k] = random();
        w.def_slot[d][0] = rnd(3) ? rnd(NIMG + 1) : 0;                 // slot 31: the HUD image
        w.def_slot[d][1] = rnd(3) ? ptrv(&w.objs[renderer()]) : 0;     // slot 32: the HUD renderer
        w.def_slot[d][2] = rnd(3) ? rnd(NIMG + 1) : 0;                 // slot 33: the detail image
        w.def_slot[d][3] = rnd(3) ? ptrv(&w.objs[renderer()]) : 0;     // slot 34: the detail renderer
        w.def_slot[d][4] = rnd(3) ? rnd(8) : 0;                        // slot 36: "Generate/Max Count"
        set_record(w.cat_rec[d], def, nulls, d);
    }
    if (rnd(4) == 0)
        memcpy(w.cat_rec[rnd(NDEF)].name, "Fruit Single", 13);
    if (rnd(4) == 0)
        memcpy(w.cat_rec[rnd(NDEF)].name, "Fruit Pile", 11);
    for (int c = 0; c < NCAT; c++) {
        BYTE *t = w.types[c];
        for (DWORD k = 0; k < 0x28; k += 4)
            *reinterpret_cast<DWORD *>(t + k) = random();
        *reinterpret_cast<DWORD *>(t) = ptrv(g_type_vtable);
        *reinterpret_cast<DWORD *>(t + 0x20) = rnd(3) == 0 ? g_none_sound : rnd(g_sounds ? g_sounds : 1);
        // with `nulls` also another class or no object (the callers that can take it)
        const DWORD k = nulls ? rnd(8) : 2;
        set_record(w.type_rec[c], k == 0 ? nullptr : k == 1 ? (void *)&w.objs[other_object()] : (void *)t, nulls, c);
        if (k == 0)
            w.type_rec[c].loading = 1;   // a record without object is being loaded (else the real list would load it)
    }
    for (int i = 0; i < NIMG; i++) {
        set_record(w.detail_rec[i], &w.objs[NRENDERER + (int)rnd(6)], nulls_images, i);
        set_record(w.hud_rec[i], &w.objs[NRENDERER + (int)rnd(6)], nulls_images, i);
    }
    for (int p = 0; p < NPROJ; p++) {
        for (DWORD k = 0; k < 0x60; k += 4)
            *reinterpret_cast<DWORD *>(&w.projs[p][k]) = random();
        set_record(w.proj_rec[p], w.projs[p], false, p);
    }
    for (int i = 0; i < NITEM; i++) {
        BYTE *it = w.items[i];
        for (DWORD k = 0; k < 0x30; k += 4)
            *reinterpret_cast<DWORD *>(it + k) = random();
        *reinterpret_cast<void ***>(it) = g_item_vt[rnd(5)];
        *reinterpret_cast<DWORD *>(it + 8) = rnd(NDEF + 1);
        it[0x11] = (BYTE)(rnd(5) == 0);
        w.item_answer[i][0] = random() & 1;
        w.item_answer[i][1] = random() & 1;
        w.item_answer[i][2] = rnd(5) == 0 ? 0xffffffff : rnd(9);
        w.data[i] = it;
    }
    CInventory &inv = w.inv;
    for (DWORD k = 0; k < sizeof inv; k += 4)
        *reinterpret_cast<DWORD *>(reinterpret_cast<BYTE *>(&inv) + k) = random();
    inv.vftable = INVENTORY_VTABLE;
    inv.items = {0x5ee144, w.data, rnd(NITEM + 1), NITEM + 2};
    w.part_vftable = ptrv(g_part_vtable);
    for (DWORD k = 0; k < sizeof w.part_body; k++)
        w.part_body[k] = (BYTE)random();
    w.part_flags = rnd(3) == 0 ? 0x10000000u : random() & 0xefffffffu;
    w.part_cls = rnd(5) ? g_classes[9] : g_classes[0];
    w.part_inv = &w.inv;
    w.reporter_vftable = ptrv(g_reporter_vtable);
    for (int k = 0; k < 4; k++)
        w.icon[k] = (int)rnd(100) - 20;
    w.fruit_found = random() & 1;
    w.first_is_part = rnd(4) != 0;
    *reinterpret_cast<DWORD *>(&w.quest_data[0xc]) = ptrv(w.quest_data_c);
    *reinterpret_cast<DWORD *>(&w.quest_data_c[0xc]) = random();
    w.objs[12].cls = rnd(4) ? g_classes[7] : g_classes[0];   // the quest game type
    w.objs[13].cls = rnd(4) ? g_classes[8] : g_classes[0];   // its counter stat
    w.objs[12].answer[0] = rnd(4) != 0;
    w.objs[13].answer[0] = rnd(4) != 0;
    w.objs[14].answer[0] = rnd(NDEF + 1);                    // the catalogue's "Fruit Single"
}

struct Fn {
    const char *name;
    DWORD address;
    bool nulls, nulls_images;   // records being loaded allowed (catalogue, categories, projectiles / images)
    bool al, fpu, no_eax;
};
const Fn FNS[] = {
    {"AfterLoad", 0x5b7490, false, false, false, false, true},
    {"IsContinuousFire", 0x5b7640, false, false, false, false, false},
    {"ContinuousFire8", 0x5b7670, false, false, false, false, false},
    {"ContinuousFireC", 0x5b76a0, false, false, false, false, false},
    {"PowerUpModel", 0x5b76d0, false, false, false, false, false},
    {"PlaceLabel", 0x5b76e0, false, false, false, false, false},
    {"DisableLabel", 0x5b7710, false, false, false, false, false},
    {"UseLabel", 0x5b7740, false, false, false, false, false},
    {"UndefinedLabel", 0x5b7770, false, false, false, false, false},
    {"ShotsPerSecond", 0x5b77a0, false, false, false, false, false},
    {"ContinuousFireRate", 0x5b78f0, false, false, false, true, true},
    {"EnergySource", 0x5b7920, true, false, false, false, false},
    {"DamageAt", 0x5b79f0, false, false, false, true, true},
    {"DetailSlot24", 0x5b7a60, false, false, false, false, false},
    {"HudSlot23", 0x5b7ab0, false, false, false, false, false},
    {"TypeDefinition", 0x5b7af0, true, false, false, false, false},
    {"SwitchSound", 0x5b7c50, false, false, false, false, false},
    {"StatsItem", 0x5b7e20, false, false, false, false, false},
    {"MayUse", 0x5b8010, false, false, true, false, false},
    {"LoadImages", 0x5b80d0, false, false, false, false, false},
    {"Accept", 0x5b83c0, false, false, true, false, false},
    {"RemoveRequired", 0x5b88f0, true, false, false, false, true},
    {"ItemBefore", 0x5b8a80, false, false, true, false, false},
    {"ItemAfter", 0x5b8ac0, false, false, true, false, false},
    {"Before", 0x5b8b00, false, false, true, false, false},
    {"After", 0x5b8bd0, false, false, true, false, false},
    {"Animate", 0x5b8f60, false, true, false, false, false},
    {"Bounds", 0x5b9110, false, false, false, false, false},
};
constexpr int NFNS = sizeof FNS / sizeof FNS[0];

BYTE g_made[2][0x1c];   // an object a case made (the stats item, the use action)
DWORD g_made_ptr[2];

DWORD call_fn(int f, long double &st, int side)
{
    World &w = g_world;
    BYTE *def = w.defs[rnd(NDEF)];
    const DWORD a = random(), b = random();
    void *item = rnd(5) ? (void *)w.items[rnd(NITEM)] : nullptr;
    void *part = rnd(6) ? (void *)&w.part_vftable : nullptr;
    st = 0;
    g_made_ptr[side] = 0;
    switch (f) {
    case 0: {
        const DWORD before = *reinterpret_cast<DWORD *>(def + 0x8c);
        const DWORD r = at<T1>(0x5b7490)(def, a);
        const DWORD now = *reinterpret_cast<DWORD *>(def + 0x8c);
        if (now && now != before && obj_id(at<void *>(now)) == 0x99) {   // a new use action
            g_made_ptr[side] = now;
            memcpy(g_made[side], at<void *>(now), 8);
            vcall<void *>(at<void *>(now), 1, (DWORD)1);   // freed again: the other side gets the same block
        }
        return r;
    }
    case 1: return at<T0>(0x5b7640)(def);
    case 2: return at<T0>(0x5b7670)(def);
    case 3: return at<T0>(0x5b76a0)(def);
    case 4: return at<T0>(0x5b76d0)(def);
    case 5: return at<T0>(0x5b76e0)(def);
    case 6: return at<T0>(0x5b7710)(def);
    case 7: return at<T0>(0x5b7740)(def);
    case 8: return at<T0>(0x5b7770)(def);
    case 9: return at<T0>(0x5b77a0)(def);
    case 10: st = at<L0>(0x5b78f0)(def); return 0;
    case 11: return at<T1>(0x5b7920)(def, ptrv(&w.inv));
    case 12: st = at<L1>(0x5b79f0)(def, rnd(3) ? float_bits((float)rnd(400) - 20.0f) : some_float_bits()); return 0;
    case 13: return at<T5>(0x5b7a60)(def, a, b, random(), random(), random());
    case 14: return at<T3>(0x5b7ab0)(def, a, b, random());
    case 15: return at<T0>(0x5b7af0)(def);
    case 16: return at<T1>(0x5b7c50)(def, ptrv(part));
    case 17: {
        if (!item)
            item = w.items[0];
        const DWORD r = at<T2>(0x5b7e20)(def, ptrv(item), a);
        if (r && obj_id(at<void *>(r)) == 0x99) {   // a new stats item: compared, then deleted
            g_made_ptr[side] = 1;
            memcpy(g_made[side], at<void *>(r), 0x1c);
            for (DWORD k = 0xc; k <= 0x14; k += 8)   // the two strings by their text: their buffers are freed with it
                if (const char *text = buffer_text(*reinterpret_cast<DWORD *>(g_made[side] + k)))
                    *reinterpret_cast<DWORD *>(g_made[side] + k) = 0x7e700000u ^ fnv_text(2166136261u, text);
            vcall<void *>(at<void *>(r), 1, (DWORD)1);
            return 0x5e7a0000u;
        }
        return r;
    }
    case 18: {
        if (!item)
            item = w.items[0];
        return at<T2>(0x5b8010)(def, ptrv(item), ptrv(part));
    }
    case 19: return at<T0>(0x5b80d0)(def);
    case 20: return at<T4>(0x5b83c0)(def, rnd(4) ? ptrv(&w.part_vftable) : ptrv(w.items[1]), ptrv(&w.inv), ptrv(item),
                                     rnd(3) ? ptrv(&w.objs[rnd(NOBJ)]) : 0);
    case 21: return at<T3>(0x5b88f0)(rnd(9) ? def : nullptr, a, ptrv(&w.inv), b);
    case 22: return at<T3>(0x5b8a80)(def, ptrv(w.items[rnd(NITEM)]), a % 3, b % 3);
    case 23: return at<T3>(0x5b8ac0)(def, ptrv(w.items[rnd(NITEM)]), a % 3, b % 3);
    case 24: return at<T3>(0x5b8b00)(def, ptrv(w.defs[rnd(NDEF)]), a % 3, b % 3);
    case 25: return at<T3>(0x5b8bd0)(def, ptrv(w.defs[rnd(NDEF)]), a % 3, b % 3);
    case 26: return at<T1>(0x5b8f60)(def, float_bits((float)rnd(9) / 4.0f));
    case 27: {
        int rect[4] = {(int)a, (int)b, (int)random(), (int)random()};
        const DWORD r = at<T1>(0x5b9110)(def, ptrv(rect));
        ev(0x800, (DWORD)rect[0], (DWORD)rect[1], (DWORD)rect[2] ^ ((DWORD)rect[3] << 16));
        return r;
    }
    }
    return 0;
}

World g_after[2];
long double g_st[2];

void test_fn(Tally &t, int f, unsigned seed)
{
    DWORD r[2], raw[2];
    const Fn &fn = FNS[f];
    for (int side = 0; side < 2; side++) {
        setup(seed, fn.nulls, fn.nulls_images);
        log_reset(side);
        side_begin(side);
        for (int i = 0; i < NSTANDS; i++)
            patch_jump(g_stand_patches[i], STANDS[i].address, STANDS[i].fn);
        swap_in();
        arena_on(side, 0x5b);
        g_seed = seed * 7u + 3u;
        raw[side] = call_fn(f, g_st[side], side);
        r[side] = rel(raw[side], side);
        arena_off();
        swap_out();
        for (int i = NSTANDS - 1; i >= 0; i--)
            unpatch(g_stand_patches[i]);
        side_end(side);
        memcpy(&g_after[side], &g_world, sizeof g_world);
    }
    if (raw[0] == raw[1])   // equal raw values are equal before the arena mapping
        r[0] = r[1] = raw[0];
    if (fn.al) {
        r[0] &= 0xff;
        r[1] &= 0xff;
    }
    if (fn.no_eax)
        r[0] = r[1] = 0;
    const bool same_st = memcmp(&g_st[0], &g_st[1], 10) == 0;
    const bool same_made = g_made_ptr[0] == g_made_ptr[1] && (!g_made_ptr[0] || same_region(g_made[0], g_made[1], f == 0 ? 8 : 0x1c));
    const bool same = r[0] == r[1] && same_st && same_made && same_log() && g_used[0] == g_used[1] &&
                      same_region(reinterpret_cast<BYTE *>(&g_after[0]), reinterpret_cast<BYTE *>(&g_after[1]), sizeof(World)) &&
                      same_region(g_arena[0], g_arena[1], g_used[0]);
    char d[200];
    wsprintfA(d, "[seed %u: eax %x / %x, st %d, made %d, log %d / %d, heap %u / %u, first difference +%x: %x / %x]", seed, r[0], r[1],
              same_st, same_made, g_nlog[0] / 4, g_nlog[1] / 4, g_used[0], g_used[1], g_diff_at, g_diff_a, g_diff_b);
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, fn.name, d);
}

// ---- Real data ----------------------------------------------------------------------------------------

template <typename C> void real_case(Tally &t, const char *what, DWORD arg, bool al, C call_it)
{
    DWORD r[2];
    long double st[2] = {0, 0};
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        r[side] = call_it(st[side]);
        side_end(side);
    }
    if (al) {
        r[0] &= 0xff;
        r[1] &= 0xff;
    }
    char d[120];
    wsprintfA(d, "[%s %x: %x / %x]", what, arg, r[0], r[1]);
    t.check(r[0] == r[1] && memcmp(&st[0], &st[1], 10) == 0, "catalogue", d);
}

void test_real(Tally &t, int &cases)
{
    void *world = *at<void **>(0x6c2080);
    void *player = world ? at<void *(THISCALL *)(void *)>(0x4f5d80)(world) : nullptr;
    CInventory *inv = player ? at<CInventory *(THISCALL *)(void *)>(0x4267e0)(player) : nullptr;
    const DWORD n = *at<DWORD *>(0x6c0c3c);
    CClassInfo *zax_def = *at<CClassInfo **>(ZAX_ITEM_DEFINITION_CLASS);
    BYTE *prev = nullptr;
    for (DWORD d = 0; d < n; d++) {
        BYTE *def = static_cast<BYTE *>(at<void *(THISCALL *)(void *, DWORD)>(0x48d8f0)(at<void *>(0x6c0c08), d));   // loaded as the game does
        if (!def || !(at<DWORD(THISCALL *)(void *, void *)>(0x416790)(def, zax_def) & 0xff))
            continue;
        const DWORD simple[] = {0x5b7640, 0x5b7670, 0x5b76a0, 0x5b76d0, 0x5b76e0, 0x5b7710, 0x5b7740, 0x5b7770, 0x5b7af0};
        for (DWORD fa : simple)
            real_case(t, "slot", fa ^ (d << 24), false, [&](long double &) { return at<T0>(fa)(def); });
        if (*reinterpret_cast<DWORD *>(def + 0x20) || *reinterpret_cast<DWORD *>(def + 0xcc))
            real_case(t, "shots", d, false, [&](long double &) { return at<T0>(0x5b77a0)(def); });
        real_case(t, "rate", d, false, [&](long double &st) {
            st = at<L0>(0x5b78f0)(def);
            return 0u;
        });
        const float distances[] = {0.0f, 10.0f, 50.0f, 100.0f, 250.0f, 1000.0f, -1.0f};
        for (float x : distances)
            real_case(t, "damage", d, false, [&](long double &st) {
                st = at<L1>(0x5b79f0)(def, float_bits(x));
                return 0u;
            });
        cases += 16;
        if (inv) {
            real_case(t, "energy source", d, false, [&](long double &) { return at<T1>(0x5b7920)(def, ptrv(inv)); });
            real_case(t, "accept", d, true, [&](long double &) { return at<T4>(0x5b83c0)(def, ptrv(player), ptrv(inv), 0, 0); });
            for (DWORD i = 0; i < inv->items.count; i++)
                if (void *it = inv->items.data[i]) {
                    real_case(t, "may use", d, true, [&](long double &) { return at<T2>(0x5b8010)(def, ptrv(it), ptrv(player)); });
                    real_case(t, "item before", d, true, [&](long double &) { return at<T3>(0x5b8a80)(def, ptrv(it), d, i); });
                    real_case(t, "item after", d, true, [&](long double &) { return at<T3>(0x5b8ac0)(def, ptrv(it), i, d); });
                    cases += 3;
                }
            cases += 2;
        }
        if (prev) {
            real_case(t, "before", d, true, [&](long double &) { return at<T3>(0x5b8b00)(def, ptrv(prev), 1, 2); });
            real_case(t, "after", d, true, [&](long double &) { return at<T3>(0x5b8bd0)(def, ptrv(prev), 2, 1); });
            cases += 2;
        }
        // the icon bounds write +0xe4, +0xe8, +0xec: saved and restored around both sides
        DWORD saved[3], got[2][7];
        memcpy(saved, def + 0xe4, 12);
        for (int side = 0; side < 2; side++) {
            int rect[4] = {0, 0, 0, 0};
            side_begin(side);
            got[side][0] = at<T1>(0x5b9110)(def, ptrv(rect));
            side_end(side);
            memcpy(&got[side][1], rect, 16);
            memcpy(&got[side][5], def + 0xe4, 8);
            memcpy(def + 0xe4, saved, 12);
        }
        char text[80];
        wsprintfA(text, "[bounds %u: %d %d %d %d / %d %d %d %d]", d, got[0][1], got[0][2], got[0][3], got[0][4], got[1][1], got[1][2],
                  got[1][3], got[1][4]);
        t.check(memcmp(got[0], got[1], sizeof got[0]) == 0, "catalogue", text);
        cases++;
        prev = def;
    }
}

}  // namespace

ZAX_CHECKS(zax_item_defs)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_zax_item_defs;
    if (!arena_setup()) {
        log("Check zax item definitions: no arena");
        return;
    }
    init_world_tables();
    Tally objects{"zax item definitions: Create, CreateArray, destructors"}, getters{"zax item definitions: class getters"},
        fields{"zax item definitions: RegisterFields"}, registered{"zax item definitions: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    Tally life{"zax item definitions: constructors, destructors, Rect_Offset"};
    for (const Life &l : LIVES) {
        hooks_out();
        if (void *o = pool_alloc(l.size))
            at<T0>(l.ctor)(o);   // kept alive: what its defaults load stays loaded
        hooks_in();
        for (unsigned k = 0; k < (unsigned)scaled(40); k++)
            test_lifetime(life, l, k);
    }
    for (unsigned k = 0; k < (unsigned)scaled(100); k++)
        test_rect_offset(life, k);
    objects.report();
    getters.report();
    fields.report();
    registered.report();
    life.report();

    Tally behaviour{"zax item definitions: definition slots and helpers on fake worlds"};
    for (int f = 0; f < NFNS; f++) {
        log("  %s", FNS[f].name);   // the last line before a case that ends the game
        for (unsigned k = 0; k < (unsigned)scaled(250); k++)
            test_fn(behaviour, f, 7000u * (unsigned)f + k);
    }
    behaviour.report();

    Tally real{"zax item definitions: the real item catalogue (real data)"};
    int cases = 0;
    test_real(real, cases);
    log("  %d real-data cases", cases);
    real.report();
}

}  // namespace zax
