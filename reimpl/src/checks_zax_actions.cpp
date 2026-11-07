// Checks of the game type, team and inventory actions (zax_actions.cpp) against the original, in the
// game thread (machinery: checks_inventory_common.h).
//  - Classes: Create (pool block filled with a pattern, the C heap from the arena), CreateArray with 0-3
//    elements, the deleting destructors, both class getters of the 18 classes, RegisterFields into a fake
//    class info and with the class variable 0, the registered descriptors (real data);
//    CCheckForInventoryItemBaseAction's deleting destructor 0x5b3270 called directly.
//  - Behaviour: real actions made by the original Create (their fields set from a seed, strings
//    assigned) on a fake world; the functions of other subsystems jump to recorders: the application's
//    game type, end, inventory and HUD functions (0x5a0850, 0x5a01b0, 0x5a01a0, 0x59ff80, 0x5a0760,
//    0x5a3480, 0x429e40), the action helpers 0x41a720 and 0x41ad40 (the names resolve to fake parts),
//    the parts' team, children, rectangle and player (0x4fa960, 0x4fc7c0, 0x4fc7d0, 0x4fa8c0, 0x5babc0,
//    0x5bac80, 0x5ba420), the extra fields 0x418fc0/0x418790/0x4187c0/0x418fb0, the colour holder's
//    constructor 0x4d4cc0 and the materials window's 0x504050 (their pool blocks freed), the first
//    character 0x4f5d80, the inventory (0x4267e0, 0x4257f0, 0x4258b0, 0x426930), Object_Clone 0x416760;
//    the item catalogue reads records of our own and the error reporter 0x6c02cc is a fake. The editor's
//    converters run on real actions too (the old one deleted by the code, the new one compared and
//    deleted). Compared: eax (al for the al results), the event log, the fake world, the action's bytes
//    after the call (strings by text) and the arena.
#include "checks_inventory_common.h"
#include "x87.h"

namespace zax {

int hooks_zax_actions(Hook *out);

namespace {

using namespace invcheck;

const ClassDef CLASSES[] = {
    {"CEndGameAction", 8, 0x713fec, 0x5a8220, 0x5a8250, 0, {0x5a81d0, 0x5a8330}, true},
    {"CIsGameTypeAction", 0xc, 0x713ff0, 0x5a83f0, 0x5a8430, 0x5a8570, {0x5a83a0, 0x5a8510}, true},
    {"CGameTypeSpecificAction", 0x18, 0x713ff4, 0x5a8680, 0x5a86d0, 0x5a88a0, {0x5a8630, 0x5a8840}, true},
    {"CIsOnTeamAction", 0x10, 0x713ff8, 0x5a8a70, 0x5a8ab0, 0x5a8c20, {0x5a8a20, 0x5a8bc0}, true},
    {"CIsOnSameTeamAction", 0x10, 0x713ffc, 0x5a8e20, 0x5a8e60, 0x5a8fe0, {0x5a8dd0, 0x5a8f80}, true},
    {"CCopyColorAction", 0x14, 0x714000, 0x5a91f0, 0x5a9230, 0x5a93b0, {0x5a91a0, 0x5a9350}, true},
    {"CGiveTeamAPointAction", 0xc, 0x714004, 0x5a9730, 0x5a9760, 0x5a98a0, {0x5a96e0, 0x5a9840}, true},
    {"CGiveMyTeamAPointAction", 0xc, 0x714008, 0x5a99d0, 0x5a9a10, 0x5a9b80, {0x5a9980, 0x5a9b20}, true},
    {"CGiveMeSomePointsAction", 0x10, 0x71400c, 0x5a9cd0, 0x5a9d10, 0x5a9e80, {0x5a9c80, 0x5a9e20}, true},
    {"CCheckForInventoryItemBaseAction", 0x1c, 0x7143b4, 0, 0, 0x5b2d20, {0x5b2c70, 0x5b2cc0}, false},
    {"CCheckForInventoryItemAction", 0x1c, 0x7143b8, 0x5b32a0, 0x5b32e0, 0, {0x5b31c0, 0x5b33f0}, true},
    {"CUseInventoryItemAction", 0x1c, 0x7143bc, 0x5b3560, 0x5b35a0, 0x5b3770, {0x5b34b0, 0x5b36b0}, true, true},
    {"CGiveInventoryItemActionBase", 0x10, 0x7143c0, 0, 0, 0x5b39a0, {0x5b38f0, 0x5b3940}, false},
    {"CGiveCustomInventoryItemAction", 0x14, 0x7143c4, 0x5b3ca0, 0x5b3cf0, 0x5b3ee0, {0x5b3bf0, 0x5b3e20}, true},
    {"CGiveDefaultInventoryItemAction", 0x14, 0x7143c8, 0x5b4170, 0x5b41c0, 0x5b43b0, {0x5b40c0, 0x5b42f0}, true},
    {"CShowInventoryAction", 8, 0x7143cc, 0x5b5060, 0x5b5090, 0, {0x5b5010, 0x5b5170}, true},
    {"CShowMaterialsGeneratorAction", 8, 0x7143d0, 0x5b5230, 0x5b5260, 0, {0x5b51e0, 0x5b5340}, true},
    {"CUseChargeFromInventoryItemAction", 0x10, 0x7143d4, 0x5b5430, 0x5b5460, 0x5b55a0, {0x5b53e0, 0x5b5540}, true},
};

using Create0 = void *(__cdecl *)();
using T1 = DWORD(THISCALL *)(void *, DWORD);
using T2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
using T4 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD);

// 0x5b3270 on a CCheckForInventoryItemAction made by the original Create (flags 0: compared, then freed)
void test_check_base_dtor(Tally &t, unsigned seed)
{
    BYTE obj[2][0x1c];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        arena_on(side, (BYTE)seed);
        hooks_in();
        BYTE *o = static_cast<BYTE *>(at<Create0>(0x5b32a0)());
        if (side == 0)
            hooks_out();
        for (DWORD k = 0xc; k < 0x1c; k += 4)   // "Inventory Item Definition" +8 stays the default
            *reinterpret_cast<DWORD *>(o + k) = random() & 0xff00ff;
        *reinterpret_cast<DWORD *>(o + 0x10) = 0;
        *reinterpret_cast<DWORD *>(o + 0x14) = 0;
        const DWORD flags = seed & 1;
        r[side] = at<T1>(0x5b3270)(o, flags) == ptrv(o);
        memcpy(obj[side], o, 0x1c);
        if (!flags)
            pool_free(o, 0x1c);
        arena_off();
        side_end(side);
    }
    char d[120];
    wsprintfA(d, "[seed %u: eax is this %d / %d, first difference +%x]", seed, r[0], r[1], g_diff_at);
    t.check(r[0] == r[1] && r[0] && (seed & 1 ? true : same_region(obj[0], obj[1], 0x1c)) && same_region(g_arena[0], g_arena[1], g_used[0]),
            "CCheckForInventoryItemBaseAction::DeletingDtor", d);
}

// ---- The fake world ----------------------------------------------------------------------------------

constexpr int NPART = 4, NOBJ = 8, NDEF = 6;

struct Record {
    BYTE name[0x78];
    void *object;
    BYTE keep, in_use, unknown7e, loading;
};
struct FakeObject {   // parts, items, holders, clones: slot 2 the class
    DWORD vftable;
    BYTE body[0x2c];
    CClassInfo *cls;
    DWORD id;
    DWORD answer[2];
};
struct World {
    FakeObject parts[NPART];
    FakeObject objs[NOBJ];
    Record records[NDEF];
    BYTE defs[NDEF][0x60];
    DWORD actions[6];          // the game type actions' array
    DWORD reporter_vftable;
    DWORD answers[24];         // what the stand-ins give
    BYTE conv[0x14];           // a converter object (+8 its text, +0xc/+0x10 field names)
    BYTE player_data[0x100];   // +0xf6 the score
    BYTE hue_holder[0x20];
};
World g_world;

void *g_vtable[128];
void *g_reporter_vtable[20];
CClassInfo *g_classes[8];

int obj_id(const void *p)
{
    for (int i = 0; i < NPART; i++)
        if (p == &g_world.parts[i])
            return 0x10 + i;
    for (int i = 0; i < NOBJ; i++)
        if (p == &g_world.objs[i])
            return 0x20 + i;
    if (p == g_world.hue_holder)
        return 0x30;
    if (p == g_world.player_data)
        return 0x31;
    return p ? 0x99 : 0;
}
DWORD ans(int k) { return g_world.answers[k]; }

DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
CClassInfo *THISCALL f_class(FakeObject *o) { ev(0x102, (DWORD)obj_id(o)); return o->cls; }
DWORD THISCALL f_delete(FakeObject *o, DWORD flags) { ev(0x101, (DWORD)obj_id(o), flags); return result(0x01); }
DWORD THISCALL f_reset(FakeObject *o) { ev(0x119, (DWORD)obj_id(o)); return result(0x19); }
DWORD THISCALL f_game_point(FakeObject *o, DWORD team, DWORD n) { ev(0x11a, (DWORD)obj_id(o), team, n); return result(0x1a); }
DWORD THISCALL f_take(FakeObject *o, DWORD a, void *part) { ev(0x121, (DWORD)obj_id(o), a, (DWORD)obj_id(part)); return result(0x21); }
DWORD THISCALL f_use(FakeObject *o, DWORD amount) { ev(0x132, (DWORD)obj_id(o), amount); return result(o->answer[0]); }
DWORD THISCALL f_report(void *, const char *text) { ev(0x610, fnv_text(2166136261u, text)); return result(0x10); }

// Stand-ins
void THISCALL s_end_game(void *a) { ev(0x701, a == *at<void **>(0x713f14)); }
DWORD THISCALL s_bits(void *) { ev(0x702); return ans(0); }
DWORD THISCALL s_index(void *) { ev(0x703); return ans(1); }
void *THISCALL s_game(void *) { ev(0x704); return ans(2) & 1 ? &g_world.objs[0] : nullptr; }
void THISCALL s_show(void *) { ev(0x705); }
DWORD THISCALL s_hud(void *, DWORD def, DWORD flag, DWORD player) { ev(0x706, def, flag, player); return result(0x06); }
DWORD THISCALL s_page(void *w) { ev(0x707, ptrv(w)); return result(0x07); }
DWORD THISCALL s_execute(void *a, void *layer, DWORD a2, DWORD a3, DWORD a4, void *caller, DWORD field)
{
    ev(0x708, ptrv(a), ptrv(layer) ^ a2, a3 ^ (a4 << 3));
    ev(0x709, caller != nullptr, field);
    return result(ans(3));
}
void *__fastcall s_resolve(void *layer, ZString *name, DWORD a2, DWORD a4, DWORD a3, void *caller, DWORD report, CClassInfo *cls)
{
    const DWORD h = name && name->buffer ? fnv_text(2166136261u, name->c_str()) : 0x0bad;
    ev(0x70a, ptrv(layer) ^ h, a2 ^ (a4 << 2) ^ (a3 << 4), (caller != nullptr) + report * 2 + ptrv(cls) * 4);
    const DWORD k = (h ^ ans(4)) % 5;
    return k < NPART ? &g_world.parts[k] : nullptr;
}
DWORD THISCALL s_team(FakeObject *p) { ev(0x70b, (DWORD)obj_id(p)); return p->answer[0]; }
DWORD THISCALL s_children(FakeObject *p) { ev(0x70c, (DWORD)obj_id(p)); return p->answer[1] & 3; }
void *THISCALL s_child(FakeObject *p, DWORD i) { ev(0x70d, (DWORD)obj_id(p), i); return &g_world.parts[(i + 1) % NPART]; }
void THISCALL s_tell(FakeObject *p) { ev(0x70e, (DWORD)obj_id(p)); }
DWORD __fastcall s_player(void *p, DWORD o) { ev(0x70f, (DWORD)obj_id(p), o); return 0x5e7aa000u + (DWORD)obj_id(p); }
void *THISCALL s_net(void *p) { ev(0x710, (DWORD)obj_id(p)); return ans(5) & 1 ? g_world.player_data : nullptr; }
void THISCALL s_score(void *d, int s) { ev(0x711, (DWORD)obj_id(d), (DWORD)s); }
DWORD THISCALL s_dyn_has(void *, void *o) { ev(0x712, (DWORD)obj_id(o)); return result((ans(6) >> (obj_id(o) & 15)) & 1); }
void *THISCALL s_dyn_get(void *, void *o) { ev(0x713, (DWORD)obj_id(o)); return &g_world.objs[(DWORD)obj_id(o) % NOBJ]; }
void THISCALL s_dyn_set(void *, void *o, void *v) { ev(0x714, (DWORD)obj_id(o), (DWORD)obj_id(v)); }
void THISCALL s_dyn_remove(void *, void *o) { ev(0x715, (DWORD)obj_id(o)); }
void *THISCALL s_hue_ctor(void *block)
{
    ev(0x716);
    pool_free(block, 0x20);
    return g_world.hue_holder;
}
void *THISCALL s_window(void *block)
{
    ev(0x717);
    pool_free(block, 0x140);
    return block;
}
void *THISCALL s_first(void *) { ev(0x718); return ans(7) & 3 ? &g_world.parts[0] : nullptr; }
void *THISCALL s_inventory(void *p) { ev(0x719, (DWORD)obj_id(p)); return &g_world.objs[7]; }
DWORD THISCALL s_total(void *inv, DWORD def) { ev(0x71a, (DWORD)obj_id(inv), def); return ans(8) % 6; }
void *THISCALL s_find(void *inv, DWORD def) { ev(0x71b, (DWORD)obj_id(inv), def); return ans(9) & 1 ? &g_world.objs[1 + def % 4] : nullptr; }
DWORD __fastcall s_use_items(void *p, DWORD def, DWORD n) { ev(0x71c, (DWORD)obj_id(p), def, n); return result(0x1c); }
void *THISCALL s_clone(void *o, DWORD a, DWORD b) { ev(0x71d, (DWORD)obj_id(o), a, b); return &g_world.objs[6]; }

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x5a0850, (void *)&s_end_game}, {0x5a01b0, (void *)&s_bits},       {0x5a01a0, (void *)&s_index},    {0x59ff80, (void *)&s_game},
    {0x5a0760, (void *)&s_show},     {0x5a3480, (void *)&s_hud},        {0x429e40, (void *)&s_page},     {0x41a720, (void *)&s_execute},
    {0x41ad40, (void *)&s_resolve},  {0x4fa960, (void *)&s_team},       {0x4fc7c0, (void *)&s_children}, {0x4fc7d0, (void *)&s_child},
    {0x4fa8c0, (void *)&s_tell},     {0x5babc0, (void *)&s_player},     {0x5bac80, (void *)&s_net},      {0x5ba420, (void *)&s_score},
    {0x418fc0, (void *)&s_dyn_has},  {0x418790, (void *)&s_dyn_get},    {0x4187c0, (void *)&s_dyn_set},  {0x418fb0, (void *)&s_dyn_remove},
    {0x4d4cc0, (void *)&s_hue_ctor}, {0x504050, (void *)&s_window},     {0x4f5d80, (void *)&s_first},    {0x4267e0, (void *)&s_inventory},
    {0x4257f0, (void *)&s_total},    {0x4258b0, (void *)&s_find},       {0x426930, (void *)&s_use_items}, {0x416760, (void *)&s_clone},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_stand_patches[NSTANDS];

void init_world_tables()
{
    for (int i = 0; i < 128; i++)
        g_vtable[i] = (void *)&f_bad;
    g_vtable[1] = (void *)&f_delete;
    g_vtable[2] = (void *)&f_class;
    g_vtable[19] = (void *)&f_reset;
    g_vtable[26] = (void *)&f_game_point;
    g_vtable[33] = (void *)&f_take;
    g_vtable[50] = (void *)&f_use;
    for (int i = 0; i < 20; i++)
        g_reporter_vtable[i] = (void *)&f_bad;
    g_reporter_vtable[16] = (void *)&f_report;
    const DWORD getters[8] = {0x4163b0, 0x49a930, 0x429e50, 0x5afbd0, 0x42a760, 0x5b40c0, 0x5b3bf0, 0x41a650};
    for (int i = 0; i < 8; i++)
        g_classes[i] = at<CClassInfo *(__cdecl *)()>(getters[i])();
}

DWORD rnd(DWORD n) { return (random() >> 8) % n; }

void setup(unsigned seed)
{
    g_seed = seed;
    World &w = g_world;
    memset(&w, 0, sizeof w);
    for (int i = 0; i < NPART + NOBJ; i++) {
        FakeObject &o = i < NPART ? w.parts[i] : w.objs[i - NPART];
        o.vftable = ptrv(g_vtable);
        for (DWORD k = 0; k < sizeof o.body; k++)
            o.body[k] = (BYTE)random();
        o.cls = g_classes[rnd(8)];
        o.id = (DWORD)i;
        o.answer[0] = rnd(4);
        o.answer[1] = random();
        *reinterpret_cast<DWORD *>(&o.body[4]) = rnd(NDEF);   // +8: a definition index
    }
    for (int d = 0; d < NDEF; d++) {
        for (DWORD k = 0; k < 0x60; k += 4)
            *reinterpret_cast<DWORD *>(&w.defs[d][k]) = random();
        *reinterpret_cast<DWORD *>(&w.defs[d][0x1c]) = rnd(3) ? ptrv(&w.objs[1 + rnd(5)]) : 0;
        w.records[d].object = w.defs[d];
        w.records[d].name[0] = (BYTE)('a' + d);
    }
    for (int i = 0; i < 6; i++)
        w.actions[i] = rnd(3) ? 0x5e7ac000u + (DWORD)i : 0;
    w.reporter_vftable = ptrv(g_reporter_vtable);
    for (int k = 0; k < 24; k++)
        w.answers[k] = random();
    w.answers[1] = rnd(8);   // the game type index (6 actions)
    for (DWORD k = 0; k < sizeof w.player_data; k++)
        w.player_data[k] = (BYTE)random();
    for (DWORD k = 0; k < sizeof w.hue_holder; k += 4)
        *reinterpret_cast<DWORD *>(&w.hue_holder[k]) = random();
    *reinterpret_cast<DWORD *>(w.hue_holder) = ptrv(g_vtable);
    for (DWORD k = 0; k < sizeof w.conv; k += 4)
        *reinterpret_cast<DWORD *>(&w.conv[k]) = random();
}

// The action of a case: made by the (original) Create, its fields set; deleted again afterwards
struct Case {
    const char *name;
    DWORD create, fn;
    int args;    // 4: slot 23 (layer, a2, a3, a4); 1/2: other slots
    bool al, no_eax;
};
const Case CASES[] = {
    {"CEndGameAction::Execute", 0x5a8220, 0x5a8390, 4, true, false},
    {"CIsGameTypeAction::Execute", 0x5a83f0, 0x5a8610, 4, true, false},
    {"CGameTypeSpecificAction::Execute", 0x5a8680, 0x5a89d0, 4, true, false},
    {"CIsOnTeamAction::Execute", 0x5a8a70, 0x5a8d70, 4, true, false},
    {"CIsOnSameTeamAction::Execute", 0x5a8e20, 0x5a9100, 4, true, false},
    {"CCopyColorAction::Execute", 0x5a91f0, 0x5a95b0, 4, true, false},
    {"CGiveTeamAPointAction::Execute", 0x5a9730, 0x5a9950, 4, true, false},
    {"CGiveMyTeamAPointAction::Execute", 0x5a99d0, 0x5a9c10, 4, true, false},
    {"CGiveMeSomePointsAction::Execute", 0x5a9cd0, 0x5a9fa0, 4, true, false},
    {"CCheckForInventoryItemAction::Execute", 0x5b32a0, 0x5b30f0, 4, true, false},
    {"CUseInventoryItemAction::Execute", 0x5b3560, 0x5b30f0, 4, true, false},
    {"CGiveCustomInventoryItemAction::Execute", 0x5b3ca0, 0x5b3b70, 4, true, false},
    {"CGiveDefaultInventoryItemAction::Execute", 0x5b4170, 0x5b3b70, 4, true, false},
    {"CGiveCustomInventoryItemAction::AfterLoad", 0x5b3ca0, 0x5b3fc0, 1, false, false},
    {"CGiveCustomInventoryItemAction::Give", 0x5b3ca0, 0x5b3fd0, 1, false, false},
    {"CGiveCustomInventoryItemAction::SetItem", 0x5b3ca0, 0x5b3ff0, 1, false, true},
    {"CGiveCustomInventoryItemAction::Valid", 0x5b3ca0, 0x5b4030, 0, false, false},
    {"CGiveDefaultInventoryItemAction::AfterLoad", 0x5b4170, 0x5b4640, 1, false, true},
    {"CGiveDefaultInventoryItemAction::Give", 0x5b4170, 0x5b4d90, 1, false, true},
    {"CGiveDefaultInventoryItemAction::Definition", 0x5b4170, 0x5b4ea0, 0, false, false},
    {"CGiveDefaultInventoryItemAction::Valid", 0x5b4170, 0x5b4ef0, 0, false, false},
    {"CShowInventoryAction::Execute", 0x5b5060, 0x5b51d0, 4, true, false},
    {"CShowMaterialsGeneratorAction::Execute", 0x5b5230, 0x5b53a0, 4, true, false},
    {"CUseChargeFromInventoryItemAction::Execute", 0x5b5430, 0x5b56c0, 4, true, false},
    {"CToCustomItemConverter::Describe", 0x5b4170, 0x5b4700, 2, false, false},
    {"CToCustomItemConverter::Convert", 0x5b4170, 0x5b48d0, -1, false, false},
    {"CToDefaultItemConverter::Describe", 0x5b3ca0, 0x5b4a90, 2, false, false},
    {"CToDefaultItemConverter::Convert", 0x5b3ca0, 0x5b4c60, -1, false, false},
    {"CToUseItemConverter::Describe", 0x5b32a0, 0x5b5720, 2, false, false},
    {"CToUseItemConverter::Convert", 0x5b32a0, 0x5b59f0, -1, false, false},
    {"CToCheckForItemConverter::Describe", 0x5b3560, 0x5b5aa0, 2, false, false},
    {"CToCheckForItemConverter::Convert", 0x5b3560, 0x5b5d70, -1, false, false},
};
constexpr int NCASES = sizeof CASES / sizeof CASES[0];

const char *const NAMES[] = {"", "$Instigator", "$Trigger", "Door 1", "Fruit", "Zax"};
const char *const FIELD_NAMES[] = {"Count", "Inventory Item Definition", "Show in HUD", "Legacy Succeed Action Do NOT use", "No such field"};

// The fields of a made action from the seed (strings assigned; owned objects are fakes whose slot 1 only records)
void fill_action(BYTE *o, DWORD create, DWORD fn)
{
    const bool converter = fn == 0x5b48d0 || fn == 0x5b4c60 || fn == 0x5b5720 || fn == 0x5b59f0 || fn == 0x5b5aa0 || fn == 0x5b5d70;
    auto D = [&](DWORD off) -> DWORD & { return *reinterpret_cast<DWORD *>(o + off); };
    auto assign = [&](DWORD off) { at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(o + off, NAMES[rnd(6)]); };
    switch (create) {
    case 0x5a83f0: D(8) = random(); break;
    case 0x5a8680: D(0xc) = ptrv(g_world.actions); D(0x10) = rnd(7); D(0x14) = 6; break;
    case 0x5a8a70: D(8) = rnd(5) - 1; assign(0xc); break;
    case 0x5a8e20: case 0x5a91f0: assign(8); assign(0xc); if (create == 0x5a91f0) o[0x10] = (BYTE)rnd(2); break;
    case 0x5a9730: D(8) = rnd(5); break;
    case 0x5a99d0: assign(8); break;
    case 0x5a9cd0: assign(8); D(0xc) = rnd(50) - 10; break;
    case 0x5b32a0: case 0x5b3560:
        D(8) = rnd(NDEF); D(0xc) = rnd(5);
        D(0x10) = rnd(2) && !converter ? 0x5e7ad000u : 0;   // the converters copy and delete them
        D(0x14) = rnd(2) && !converter ? 0x5e7ae000u : 0;
        o[0x18] = (BYTE)rnd(2);
        break;
    case 0x5b3ca0:   // Give and the converter to the default action read the item without a check
        assign(8);
        o[0xc] = (BYTE)rnd(2);
        D(0x10) = rnd(5) || fn == 0x5b3fd0 || fn == 0x5b4c60 ? ptrv(&g_world.objs[1 + rnd(5)]) : 0;
        break;
    case 0x5b4170: assign(8); o[0xc] = (BYTE)rnd(2); D(0x10) = rnd(NDEF); break;
    case 0x5b5430: D(8) = rnd(NDEF); D(0xc) = float_bits((float)rnd(20)); break;
    }
    // the padding after a byte field from the seed too: the converters copy a byte field into a dword
    // field with the bytes after it, which otherwise depend on the pool block's history
    const DWORD pad = create == 0x5b3ca0 || create == 0x5b4170 ? 0xd : create == 0x5b32a0 || create == 0x5b3560 ? 0x19 : create == 0x5a91f0 ? 0x11 : 0;
    for (DWORD k = 0; pad && k < 3; k++)
        o[pad + k] = (BYTE)random();
}
// Before the original destructor runs: what is not the action's own (fake owned objects, the world's
// array) is taken out
void unfill_action(BYTE *o, DWORD create)
{
    auto D = [&](DWORD off) -> DWORD & { return *reinterpret_cast<DWORD *>(o + off); };
    switch (create) {
    case 0x5a8680: D(0xc) = 0; D(0x10) = 0; D(0x14) = 0; break;
    case 0x5b32a0: case 0x5b3560: D(0x10) = 0; D(0x14) = 0; break;
    case 0x5b3ca0: D(0x10) = 0; break;
    }
}

constexpr DWORD SWAPS[] = {0x6c0c38, 0x6c0c3c, 0x6c02cc};
DWORD g_saved[3];
BYTE g_obj_after[2][0x20];
DWORD g_made_size[2];
World g_after[2];

DWORD run_case(const Case &c, int side)
{
    World &w = g_world;
    BYTE *o = static_cast<BYTE *>(at<Create0>(c.create)());
    fill_action(o, c.create, c.fn);
    const DWORD a = random(), b = random(), x = random(), y = random();
    DWORD r = 0;
    g_made_size[side] = 0;
    BYTE *made = nullptr;
    switch (c.args) {
    case 4: r = at<T4>(c.fn)(o, a, b, x, rnd(5) ? ptrv(&w.parts[rnd(NPART)]) : 0); break;
    case 1: r = at<T1>(c.fn)(o, c.fn == 0x5b3ff0 ? ptrv(&w.objs[rnd(NOBJ)]) : rnd(4) ? ptrv(&w.parts[rnd(NPART)]) : a); break;
    case 0: r = at<DWORD(THISCALL *)(void *)>(c.fn)(o); break;
    case 2: {
        CClassInfo *cls = nullptr;
        r = at<T2>(c.fn)(w.conv, ptrv(o), rnd(3) ? ptrv(&cls) : 0);
        ev(0x800, ptrv(cls));
        break;
    }
    case -1: {
        made = static_cast<BYTE *>(at<void *(THISCALL *)(void *, void *)>(c.fn)(w.conv, o));
        o = nullptr;   // deleted by the converter
        r = 0;
        break;
    }
    }
    (void)y;
    BYTE *keep = made ? made : o;
    if (keep) {
        DWORD size = made ? (c.fn == 0x5b48d0 || c.fn == 0x5b4c60 ? 0x14u : 0x1cu) : 0;
        for (const ClassDef &k : CLASSES)
            if (!made && k.create == c.create)
                size = k.size;
        memcpy(g_obj_after[side], keep, size);
        g_made_size[side] = size;
        // the strings by their text now: their buffers are freed with the action
        const DWORD cls = made ? (c.fn == 0x5b48d0 ? 0x5b3ca0u : c.fn == 0x5b4c60 ? 0x5b4170u : c.fn == 0x5b59f0 ? 0x5b3560u : 0x5b32a0u) : c.create;
        const DWORD strings = cls == 0x5a8a70 ? 0x4u : cls == 0x5a8e20 || cls == 0x5a91f0 ? 0x6u
                            : cls == 0x5a99d0 || cls == 0x5a9cd0 || cls == 0x5b3ca0 || cls == 0x5b4170 ? 0x2u : 0;   // bit k: +4k+4
        // the padding after a byte field, which neither side writes (a block of another history)
        const DWORD pad = cls == 0x5b3ca0 || cls == 0x5b4170 ? 0xd : cls == 0x5b32a0 || cls == 0x5b3560 ? 0x19 : cls == 0x5a91f0 ? 0x11 : 0;
        if (pad)
            memset(g_obj_after[side] + pad, 0, 3);
        for (DWORD k = 0; k < 4; k++)
            if (strings >> k & 1)
                if (const char *text = buffer_text(*reinterpret_cast<DWORD *>(g_obj_after[side] + 4 * k + 4)))
                    *reinterpret_cast<DWORD *>(g_obj_after[side] + 4 * k + 4) = 0x7e700000u ^ fnv_text(2166136261u, text);
        unfill_action(keep, made ? (c.fn == 0x5b48d0 ? 0x5b3ca0u : c.fn == 0x5b4c60 ? 0x5b4170u : c.create) : c.create);
        if (made && c.fn == 0x5b48d0)
            *reinterpret_cast<DWORD *>(keep + 0x10) = 0;   // the fake clone
        vcall<void *>(keep, 1, (DWORD)1);
    }
    return r;
}

void test_case(Tally &t, int k, unsigned seed)
{
    const Case &c = CASES[k];
    DWORD r[2], raw[2];
    for (int side = 0; side < 2; side++) {
        setup(seed);
        // the converter's field names: real names of the action classes or none
        World &w = g_world;
        *reinterpret_cast<DWORD *>(&w.conv[8]) = rnd(2) ? ptrv("Converted") : 0;
        *reinterpret_cast<DWORD *>(&w.conv[0xc]) = rnd(3) ? ptrv(FIELD_NAMES[rnd(5)]) : 0;
        *reinterpret_cast<DWORD *>(&w.conv[0x10]) = ptrv(FIELD_NAMES[rnd(5)]);
        log_reset(side);
        side_begin(side);
        for (int i = 0; i < NSTANDS; i++)
            patch_jump(g_stand_patches[i], STANDS[i].address, STANDS[i].fn);
        const DWORD values[3] = {ptrv(g_world.records), NDEF, ptrv(&g_world.reporter_vftable)};
        for (int i = 0; i < 3; i++) {
            g_saved[i] = *at<DWORD *>(SWAPS[i]);
            *at<DWORD *>(SWAPS[i]) = values[i];
        }
        arena_on(side, 0x5c);
        g_seed = seed * 13u + 5u;
        raw[side] = run_case(c, side);
        r[side] = rel(raw[side], side);
        arena_off();
        for (int i = 2; i >= 0; i--)
            *at<DWORD *>(SWAPS[i]) = g_saved[i];
        for (int i = NSTANDS - 1; i >= 0; i--)
            unpatch(g_stand_patches[i]);
        side_end(side);
        memcpy(&g_after[side], &g_world, sizeof g_world);
    }
    if (raw[0] == raw[1])   // equal raw values are equal before the arena mapping
        r[0] = r[1] = raw[0];
    if (c.al) {
        r[0] &= 0xff;
        r[1] &= 0xff;
    }
    if (c.no_eax)
        r[0] = r[1] = 0;
    const bool same_obj = g_made_size[0] == g_made_size[1] && same_region(g_obj_after[0], g_obj_after[1], g_made_size[0]);
    const bool same = r[0] == r[1] && same_obj && same_log() && g_used[0] == g_used[1] &&
                      same_region(reinterpret_cast<BYTE *>(&g_after[0]), reinterpret_cast<BYTE *>(&g_after[1]), sizeof(World)) &&
                      same_region(g_arena[0], g_arena[1], g_used[0]);
    char d[200];
    wsprintfA(d, "[seed %u: eax %x / %x, object %d, log %d / %d, heap %u / %u, first difference +%x: %x / %x]", seed, r[0], r[1], same_obj,
              g_nlog[0] / 4, g_nlog[1] / 4, g_used[0], g_used[1], g_diff_at, g_diff_a, g_diff_b);
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, c.name, d);
}

// ---- Real data ----------------------------------------------------------------------------------------

// Every record of the real item catalogue as a CGiveDefaultInventoryItemAction's definition: Valid,
// Definition and the converter 0x714370's description (the action made by the original Create, deleted again)
void test_real(Tally &t, int &cases)
{
    const DWORD n = *at<DWORD *>(0x6c0c3c);
    for (DWORD d = 0; d < n; d++) {
        const DWORD fns[3] = {0x5b4ef0, 0x5b4ea0, 0x5b4700};
        for (DWORD f : fns) {
            DWORD r[2];
            for (int side = 0; side < 2; side++) {
                hooks_out();
                BYTE *o = static_cast<BYTE *>(at<Create0>(0x5b4170)());
                hooks_in();
                *reinterpret_cast<DWORD *>(o + 0x10) = d;
                side_begin(side);
                if (f == 0x5b4700) {
                    CClassInfo *cls = nullptr;
                    r[side] = at<T2>(f)(at<void *>(0x714370), ptrv(o), ptrv(&cls)) ^ ptrv(cls);
                } else
                    r[side] = at<DWORD(THISCALL *)(void *)>(f)(o);
                side_end(side);
                hooks_out();
                vcall<void *>(o, 1, (DWORD)1);
                hooks_in();
            }
            char text[80];
            wsprintfA(text, "[definition %u, 0x%x: %x / %x]", d, f, r[0], r[1]);
            t.check(r[0] == r[1], "CGiveDefaultInventoryItemAction", text);
            cases++;
        }
    }
}

}  // namespace

ZAX_CHECKS(zax_actions)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_zax_actions;
    if (!arena_setup()) {
        log("Check zax actions: no arena");
        return;
    }
    init_world_tables();
    Tally objects{"zax actions: Create, CreateArray, destructors"}, getters{"zax actions: class getters"}, fields{"zax actions: RegisterFields"},
        registered{"zax actions: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    for (unsigned k = 0; k < (unsigned)scaled(40); k++)
        test_check_base_dtor(objects, k);
    objects.report();
    getters.report();
    fields.report();
    registered.report();

    Tally behaviour{"zax actions: execute and the other slots, the editor's converters"};
    for (int k = 0; k < NCASES; k++) {
        log("  %s", CASES[k].name);   // the last line before a case that ends the game
        for (unsigned s = 0; s < (unsigned)scaled(200); s++)
            test_case(behaviour, k, 9000u * (unsigned)k + s);
    }
    behaviour.report();

    Tally real{"zax actions: the real item catalogue's records in a CGiveDefaultInventoryItemAction (real data)"};
    int cases = 0;
    test_real(real, cases);
    log("  %d real-data cases", cases);
    real.report();
}

}  // namespace zax
