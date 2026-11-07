// Checks of the characters' inventory items (char_items.cpp) against the original, in the game thread
// (machinery: checks_inventory_common.h).
//  - Classes: Create (pool block filled with a pattern, the C heap from the arena), CreateArray with 0-3
//    elements, the deleting destructors, both class getters of every class, RegisterFields into a fake
//    class info and with the class variable 0, the registered descriptors (real data).
//  - The item slots on a fake world set up again from the same seed before each side: a special ability
//    item with a copy of its real vtable (slots 49 and 50 recording), its definition and type definition
//    with the real vtables (so IsKindOf answers as in the game) and levels of our own, the item category
//    list's records 0x6c0830/0x6c0834 pointing to ours during a case; a Sphere of Zin item with its real
//    vtable (the armed flags grow through the real byte array, compared by their contents), its Sphere of
//    Zin data with the real vtable and our thresholds; two parts for the evil broadcast and the application
//    0x713f14 swapped for a fake whose rules have the real CZaxCustomRules vtable. Stand-ins that record:
//    ItemRef_Definition 0x4dd480, DefinitionList_Object 0x48d8f0, the category list's load 0x55e090, the
//    item's MaxCharge 0x5afe50 and Add 0x5afe90, World::FirstCharacter 0x4f5d80, the actions 0x41a720 and
//    0x41a740, Inventory_ItemCount 0x4268d0, Part_UseItems 0x426930, CInventory_IndexOf 0x4262a0,
//    EnumTable_Name 0x591e10, the dialogue window 0x47ca10 (with the text), LayerPart_Position 0x4fb0a0 and
//    the Sphere of Zin options 0x5a21f0. Compared: eax, the event log, the fake world's bytes (arrays by
//    their contents).
#include "checks_inventory_common.h"
#include "x87.h"

namespace zax {

int hooks_char_items(Hook *out);

namespace {

using namespace invcheck;

const ClassDef CLASSES[] = {
    {"CCharacterInventoryItem", 0x24, 0x7137b0, 0x57fd50, 0x57fd90, 0x57fee0, {0x57fd00, 0x57fe80}, true},
    {"CSpecialAbilitylevelInfo", 0x20, 0x7137b4, 0x580320, 0x580360, 0x5804d0, {0x580470, 0}, true},
    {"CCharacterInventoryItemDefinition", 0x180, 0x7137b8, 0x580850, 0x5808a0, 0x580a50, {0x5807f0, 0x5809e0}, true},
    {"CCharacterInventoryTypeDefinition", 0x34, 0x7137c0, 0x5810b0, 0x5810e0, 0x581230, {0x581060, 0x5811d0}, true},
    {"CSphereOfZinEnergyInventoryItem", 0x34, 0x71381c, 0x584560, 0x5845c0, 0x584790, {0x584510, 0x584730}, true},
};

template <typename T> T &at_off(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
DWORD &d32(void *p, DWORD off) { return at_off<DWORD>(p, off); }

float some_float()
{
    const DWORD r = random() >> 8;
    switch (r % 12) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: { DWORD nan = 0x7fc00000u | (r >> 8 & 0xff); float f; memcpy(&f, &nan, 4); return f; }
    case 3: return -(float)(int)(r >> 12 & 0xff) / 8.0f;
    case 4: return 1e30f;
    default: return (float)(int)(r >> 12 & 0x3ff) / 16.0f;
    }
}
DWORD fbits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}

constexpr int NLEVEL = 4, NREC = 4, NTHRESH = 6;

struct World {
    BYTE item[0x40];          // the item under test (special ability item or Sphere of Zin item)
    BYTE def[0x180];          // its CCharacterInventoryItemDefinition
    BYTE levels[NLEVEL][0x20];
    BYTE level_text[NLEVEL][0x20];   // ZStringBuffers of the levels' "Description"
    BYTE records[NREC][0x80];        // the item category list's records
    BYTE type[0x40];                 // the CCharacterInventoryTypeDefinition
    DWORD needed[NLEVEL];
    BYTE inv[0x40];
    BYTE character[0x60];
    BYTE catalogue_object[0x100];    // +0xcc the Sphere of Zin data
    BYTE sphere[0x20];               // CSphereOfZinData: +0xc thresholds, +0x10 count
    BYTE thresholds[NTHRESH][0x18];
    BYTE parts[2][0x60];
    BYTE app[0x300], holder[0x40], rules[0x40], options[0x10];
    float positions[2][2];
    DWORD def_kind, slot50, item_count, max_charge;
};
World g_world;
DWORD g_item_vt[64];
DWORD g_other_vt[64];   // the vtable of an object that is none of the classes asked for

int fake(const void *p)
{
    const BYTE *b = static_cast<const BYTE *>(p), *w = reinterpret_cast<const BYTE *>(&g_world);
    if (!p)
        return 0;
    if (b >= w && b < w + sizeof g_world)
        return 0x10000 + (int)(b - w);
    return 0x99;
}

// The recorders
DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
DWORD THISCALL f_slot49(void *it, DWORD a) { ev(0x149, (DWORD)fake(it), a); return result(0x49); }
DWORD THISCALL f_slot50(void *it, DWORD bits) { ev(0x150, (DWORD)fake(it), bits); return result(g_world.slot50); }
CClassInfo *g_other_class;
CClassInfo *THISCALL f_other_class(void *) { return g_other_class; }

// Stand-ins
BYTE *THISCALL s_definition(void *it) { ev(0x701, (DWORD)fake(it)); return g_world.def_kind ? g_world.def : nullptr; }
BYTE *THISCALL s_list_object(void *list, DWORD i)
{
    ev(0x702, list == at<void *>(0x6c0c08), i);
    return g_world.catalogue_object;
}
DWORD THISCALL s_load_record(void *list, DWORD i)
{
    ev(0x703, list == at<void *>(0x6c0800), i);
    if (i < NREC)
        d32(g_world.records[i], 0x78) = invcheck::ptrv(g_world.type);
    return result(0x03);
}
long double THISCALL s_max_charge(void *it)
{
    ev(0x704, (DWORD)fake(it));
    float f;
    memcpy(&f, &g_world.max_charge, 4);
    return fld(f);
}
DWORD THISCALL s_add(void *it, DWORD bits) { ev(0x705, (DWORD)fake(it), bits); return result(0x05); }
BYTE *THISCALL s_first_character(void *w) { ev(0x706, w == *at<void **>(0x6c2080)); return g_world.character; }
DWORD THISCALL s_execute(void *action, void *layer, void *a2, DWORD a3, void *a4, void *self, void *field)
{
    ev(0x707, invcheck::ptrv(action), (DWORD)fake(layer), (DWORD)fake(a2));
    ev(0x708, a3, (DWORD)fake(a4), (DWORD)fake(self) ^ (DWORD)fake(field) << 8);
    return result(0x07);
}
DWORD THISCALL s_execute_from(void *action, void *layer, DWORD a2, DWORD a3, void *a4, void *self)
{
    ev(0x709, invcheck::ptrv(action), (DWORD)fake(layer), a2);
    ev(0x70a, a3, (DWORD)fake(a4), (DWORD)fake(self));
    return result(0x09);
}
DWORD __fastcall s_item_count(void *inv, DWORD item)
{
    ev(0x70b, (DWORD)fake(inv), item);
    return g_world.item_count;
}
DWORD __fastcall s_use_items(void *part, DWORD item, DWORD n)
{
    ev(0x70c, (DWORD)fake(part), item, n);
    return result(0x0c);
}
DWORD THISCALL s_index_of(void *inv, void *it) { ev(0x70d, (DWORD)fake(inv), (DWORD)fake(it)); return 7; }
const char *THISCALL s_enum_name(void *list, DWORD i)
{
    ev(0x70e, list == at<void *>(0x713790), i);
    return "Heal";
}
DWORD __fastcall s_window(ZString *speaker, ZString *text, DWORD a, DWORD b)
{
    const char *s = speaker->c_str(), *t = text->c_str();
    ev(0x70f, fnv_text(2166136261u, s), fnv_text(2166136261u, t), a * 16 + b);
    return result(0x0f);
}
float *THISCALL s_position(void *part, float *out)
{
    const int i = part == g_world.parts[1] ? 1 : 0;
    ev(0x710, (DWORD)fake(part));
    out[0] = g_world.positions[i][0];
    out[1] = g_world.positions[i][1];
    return out;
}
BYTE *THISCALL s_options(void *rules) { ev(0x711, (DWORD)fake(rules)); return g_world.options; }

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x4dd480, (void *)&s_definition},     {0x48d8f0, (void *)&s_list_object}, {0x55e090, (void *)&s_load_record},
    {0x5afe50, (void *)&s_max_charge},     {0x5afe90, (void *)&s_add},         {0x4f5d80, (void *)&s_first_character},
    {0x41a720, (void *)&s_execute},        {0x41a740, (void *)&s_execute_from}, {0x4268d0, (void *)&s_item_count},
    {0x426930, (void *)&s_use_items},      {0x4262a0, (void *)&s_index_of},    {0x591e10, (void *)&s_enum_name},
    {0x47ca10, (void *)&s_window},         {0x4fb0a0, (void *)&s_position},    {0x5a21f0, (void *)&s_options},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_patches[NSTANDS];

void init_vtables()
{
    memcpy(g_item_vt, at<void *>(0x600dfc), sizeof(DWORD) * 52);
    for (int i = 52; i < 64; i++)
        g_item_vt[i] = invcheck::ptrv((void *)&f_bad);
    g_item_vt[49] = invcheck::ptrv((void *)&f_slot49);
    g_item_vt[50] = invcheck::ptrv((void *)&f_slot50);
    for (DWORD &v : g_other_vt)
        v = invcheck::ptrv((void *)&f_bad);
    g_other_vt[2] = invcheck::ptrv((void *)&f_other_class);
    g_other_class = at<CClassInfo *(__cdecl *)()>(0x4163b0)();
}

// The world of a case; kind 0: a special ability item, 1: a Sphere of Zin item
void setup(unsigned seed, int kind)
{
    g_seed = seed;
    World &w = g_world;
    BYTE *all = reinterpret_cast<BYTE *>(&w);
    for (DWORD k = 0; k + 4 <= sizeof w; k += 4)
        *reinterpret_cast<DWORD *>(all + k) = random();
    // the special ability item, its definition, levels and type
    d32(w.item, 0) = kind == 0 ? invcheck::ptrv(g_item_vt) : 0x601380;
    d32(w.item, 4) = 0;
    d32(w.item, 0xc) = 0;
    d32(w.item, 8) = random() % 6;
    d32(w.item, 0x18) = fbits(some_float());
    d32(w.item, 0x1c) = kind == 0 ? random() % (NLEVEL + 2) : fbits(some_float());
    d32(w.item, 0x20) = kind == 0 ? random() % 2 : fbits(some_float());
    w.def_kind = 1;   // without a definition (or a type definition below) the exe ends (no null tests)
    d32(w.def, 0) = 0x600f28;
    d32(w.def, 0x18) = random() % (NREC + 1);
    d32(w.def, 0x174) = random() % 4;
    d32(w.def, 0x178) = random() % (NLEVEL + 1);
    d32(w.def, 0x17c) = invcheck::ptrv(w.levels);   // 0 would end the exe (an address of 0x20 times the level)
    for (int l = 0; l < NLEVEL; l++) {
        BYTE *lv = w.levels[l];
        d32(lv, 8) = invcheck::ptrv(w.level_text[l]);
        d32(w.level_text[l], 0) = 2;
        d32(w.level_text[l], 4) = 0x10;
        wsprintfA(reinterpret_cast<char *>(w.level_text[l] + 8), "level %d", l);
        d32(lv, 0xc) = fbits(some_float());
        d32(lv, 0x10) = fbits(some_float());
        d32(lv, 0x1c) = random() % 3 ? 0x5e7ac700u + (DWORD)l : 0;
    }
    for (int r = 0; r < NREC; r++) {
        d32(w.records[r], 0x78) = random() % 4 == 0 ? 0 : invcheck::ptrv(w.type);   // 0: loaded by the stand-in
        w.records[r][0x7f] = 0;
    }
    d32(w.inv, 0) = invcheck::ptrv(g_other_vt);   // a record that is no type definition
    d32(w.type, 0) = 0x601024;
    d32(w.type, 0x28) = random() % 5;
    d32(w.type, 0x2c) = invcheck::ptrv(w.needed);
    d32(w.type, 0x30) = random() % (NLEVEL + 1);
    for (DWORD &n : w.needed)
        n = random() % 4;
    d32(w.character, 0x48) = 0x5e7a1a00;
    w.slot50 = random() & 1;
    w.item_count = random() % 5;
    w.max_charge = fbits(some_float());
    // the Sphere of Zin item's data
    d32(w.catalogue_object, 0xcc) = random() % 10 ? invcheck::ptrv(w.sphere) : invcheck::ptrv(w.inv);
    d32(w.sphere, 0) = 0x601318;
    d32(w.sphere, 4) = 0;
    d32(w.sphere, 0xc) = invcheck::ptrv(w.thresholds);
    d32(w.sphere, 0x10) = random() % (NTHRESH + 1);
    for (int i = 0; i < NTHRESH; i++) {
        BYTE *t = w.thresholds[i];
        d32(t, 8) = fbits((float)(int)(random() % 40) / 4.0f);
        d32(t, 0xc) = fbits((float)(int)(random() % 8) / 4.0f);
        d32(t, 0x10) = 0x5e7ac100u + (DWORD)i;
        d32(t, 0x14) = random() % 3;
    }
    // the parts, the application and its rules
    for (int p = 0; p < 2; p++) {
        d32(w.parts[p], 0x48) = random() % 6 == 0 ? 0 : random() % 4 == 0 ? 0x5e7a1a04u : 0x5e7a1a00u;
        w.positions[p][0] = (float)(int)(random() % 2000) - 1000.0f;
        w.positions[p][1] = random() % 6 == 0 ? w.positions[1 - p][1] : (float)(int)(random() % 2000) - 1000.0f;
    }
    d32(w.app, 0x2e0) = invcheck::ptrv(w.holder);
    d32(w.holder, 0x38) = random() % 5 ? invcheck::ptrv(w.rules) : random() % 2 ? invcheck::ptrv(w.inv) : 0;
    d32(w.rules, 0) = 0x603028;
    d32(w.rules, 4) = 0;
    d32(w.options, 8) = fbits((float)(int)(random() % 400) + 1.0f);
    w.options[0xc] = (BYTE)(random() % 2);
}

World g_after[2];
unsigned g_flags_hash[2];

// One case of a function on the world (both sides from the same seed)
void test_fn(Tally &t, const char *name, int kind, unsigned seed, DWORD mask, DWORD (*call)())
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        setup(seed, kind);
        BYTE *it = g_world.item;
        if (kind == 1) {   // the armed flags through the real append (heap)
            d32(it, 0x24) = 0x60144c;
            d32(it, 0x28) = 0;
            d32(it, 0x2c) = 0;
            d32(it, 0x30) = 0;
            const DWORD n = random() % (NTHRESH + 2);
            for (DWORD i = 0; i < n; i++) {
                BYTE b = (BYTE)(random() % 2);
                vcall<DWORD>(it + 0x24, 2, &b);
            }
        }
        log_reset(side);
        side_begin(side);
        for (int i = 0; i < NSTANDS; i++)
            patch_jump(g_patches[i], STANDS[i].address, STANDS[i].fn);
        void *const records = *at<void **>(0x6c0830);
        const DWORD count = *at<DWORD *>(0x6c0834);
        void *const app = *at<void **>(0x713f14);
        *at<void **>(0x6c0830) = g_world.records;
        *at<DWORD *>(0x6c0834) = NREC;
        *at<void **>(0x713f14) = g_world.app;
        g_seed = seed * 7u + 3u;
        r[side] = call() & mask;
        *at<void **>(0x713f14) = app;
        *at<DWORD *>(0x6c0834) = count;
        *at<void **>(0x6c0830) = records;
        for (int i = NSTANDS - 1; i >= 0; i--)
            unpatch(g_patches[i]);
        side_end(side);
        g_flags_hash[side] = 0;
        if (kind == 1) {
            const DWORD n = d32(it, 0x2c);
            g_flags_hash[side] = fnv(fnv(2166136261u, &n, 4), reinterpret_cast<void *>(static_cast<uintptr_t>(d32(it, 0x28))), n < 64 ? n : 64);
            if (d32(it, 0x28))
                heap_free(reinterpret_cast<void *>(static_cast<uintptr_t>(d32(it, 0x28))));
            d32(it, 0x28) = 0;   // the heap block compared by its contents
        }
        memcpy(&g_after[side], &g_world, sizeof g_world);
    }
    const bool same = r[0] == r[1] && same_log() && g_flags_hash[0] == g_flags_hash[1] &&
                      same_region(reinterpret_cast<BYTE *>(&g_after[0]), reinterpret_cast<BYTE *>(&g_after[1]), sizeof(World));
    char d[200];
    wsprintfA(d, "[seed %u: eax %x / %x, log %d / %d, flags %x / %x, first difference +%x: %x / %x]", seed, r[0], r[1], g_nlog[0] / 4,
              g_nlog[1] / 4, g_flags_hash[0], g_flags_hash[1], g_diff_at, g_diff_a, g_diff_b);
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, name, d);
}

using F2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
using F3 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD);
DWORD call_update()
{
    return at<F2>(0x580070)(g_world.item, invcheck::ptrv(g_world.inv), fbits(some_float()));
}
DWORD call_level()
{
    return at<DWORD(THISCALL *)(void *, DWORD)>(0x580bd0)(g_world.def, random() % (NLEVEL + 2));
}
DWORD call_step() { return at<F3>(0x584960)(g_world.item, random(), random(), random()); }
DWORD call_thresholds()
{
    BYTE *e = g_world.character;
    return at<F3>(0x584980)(g_world.item, invcheck::ptrv(e), random(), random());
}
DWORD call_receive()
{
    const DWORD k = random() % 8;
    void *a = g_world.parts[0], *b = k == 0 ? g_world.parts[0] : g_world.parts[1];
    return at<F3>(0x584d40)(g_world.item, invcheck::ptrv(a), invcheck::ptrv(b), fbits(some_float()));
}

}  // namespace

ZAX_CHECKS(char_items)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_char_items;
    if (!arena_setup()) {
        log("Check char items: no arena");
        return;
    }
    init_vtables();
    Tally objects{"char items: Create, CreateArray, destructors"}, getters{"char items: class getters"},
        fields{"char items: RegisterFields"}, registered{"char items: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    objects.report();
    getters.report();
    fields.report();
    registered.report();

    Tally behaviour{"char items: item slots on fake worlds"};
    for (unsigned k = 0; k < (unsigned)scaled(1500); k++)
        test_fn(behaviour, "CCharacterInventoryItem::Update", 0, k, 0, call_update);
    for (unsigned k = 0; k < (unsigned)scaled(200); k++)
        test_fn(behaviour, "CCharacterInventoryItemDefinition::Level", 0, 20000 + k, 0xffffffff, call_level);
    for (unsigned k = 0; k < (unsigned)scaled(200); k++)
        test_fn(behaviour, "CSphereOfZinEnergyInventoryItem::Step", 1, 30000 + k, 0xffffffff, call_step);
    for (unsigned k = 0; k < (unsigned)scaled(1500); k++)
        test_fn(behaviour, "CSphereOfZinEnergyInventoryItem::Thresholds", 1, 40000 + k, 0, call_thresholds);
    for (unsigned k = 0; k < (unsigned)scaled(800); k++)
        test_fn(behaviour, "CSphereOfZinEnergyInventoryItem::ReceiveEvil", 1, 50000 + k, 0, call_receive);
    behaviour.report();
}

}  // namespace zax
