// Checks of the game types, rules and save game data (game_types.cpp) against the original, in the game
// thread (machinery: checks_inventory_common.h; side 0 with all hooks of the module taken out, side 1
// through the reimplementation).
//  - Classes: Create (pool block filled with a pattern, the C heap from the arena), CreateArray with 0-3
//    elements, the deleting destructors, the class getters, RegisterFields into a fake class info and with
//    the class variable 0, the registered descriptors (real data) of the eight classes.
//  - Objects: CZaxSaveData's constructor on a pattern block, the 12-byte holder's deleting destructor
//    (flags 0 and 1), CUseCannedAction's Create, CreateArray and destructors (+0xc cleared first).
//  - Behaviour on a fake world: Capture the Flag's frag (slot 25), team hue (39) and slot 45, Salvage
//    King's minerals (40) and slot 45, the mineral scoring action (slot 23); stand-ins record the net
//    player lookup and its score setters (0x5bac80, 0x5ba360-0x5ba420, writing the player like the
//    originals), FindOrFail 0x591fc0, the item counts and inventory (0x426860, 0x4267e0, 0x4258b0), the
//    application's game type 0x59ff80 and the match end 0x51c650; the game types, entities, items, the
//    world and the net game settings are fakes. CZaxSaveData's difficulty (0x5bb780) on a fake difficulty
//    list (its records, name slot 1, loader slot 37 and the difficulties' slot 7 recorded).
//    CZaxSaveGameStats' name expansion, the pattern made from a name, the description and the order of
//    two saves on real objects (patterns, map names, dates and times from the seed, missions fakes whose
//    number 0x512280 is a stand-in).
//  - Real data: Rules.txt, Capture The Flag.zgt and Salvage King.zgt read on each side (Object_CreateFromFile,
//    side 0 with the hooks out) and compared; the two game types through their slots on the fake world
//    (their own vtable copied with slots 27 and 48 recorded).
#include "checks_inventory_common.h"
#include "x87.h"

namespace zax {

int hooks_game_types(Hook *out);

namespace {

using namespace invcheck;

// The "Screen Shot" descriptor's default colour is a FieldColor on the stack whose bytes 0xa/0xb (the
// palette index and padding) the colour constructor 0x53f010 leaves (stack contents in the original):
// its bytes +0x3e/+0x3f in the made descriptors set to 0, as they are in the registered one
void mask_screen_shot(CClassInfo *c)
{
    for (DWORD i = c->num_inherited; i < c->num_fields; i++)
        if (CFieldDesc *f = c->fields[i])
            if (f->vftable == 0x5f4ce8 && lstrcmpA(f->name.c_str(), "Screen Shot") == 0) {
                reinterpret_cast<BYTE *>(f)[0x3e] = 0;
                reinterpret_cast<BYTE *>(f)[0x3f] = 0;
            }
}

const ClassDef CLASSES[] = {
    {"CCaptureTheFlagGameType", 0xfc, 0x6bd12c, 0x468f30, 0x468f60, 0x4690b0, {0x469050, 0}, true},
    {"CSalvageKingGameType", 0xdc, 0x71316c, 0x5612f0, 0x561320, 0x561470, {0x5612a0, 0x561410}, true},
    {"CSalvageKingScoreMineralsAction", 8, 0x713170, 0x561940, 0x561970, 0, {0x5618f0, 0x561a50}, true},
    {"CCustomRules", 8, 0x713150, 0, 0, 0, {0x560ac0, 0}, false},
    {"CCustomRulesList", 0x18, 0x71314c, 0x560750, 0x5607a0, 0x5609f0, {0x560990, 0}, true},
    {"CRules", 0x3c, 0x713154, 0x560b70, 0x560bf0, 0x560e50, {0x560b20, 0}, true},
    {"CZaxSaveData", 0x34, 0x714474, 0x5bb160, 0x5bb1c0, 0x5bb3e0, {0x5bb110, 0}, true},
    {"CZaxSaveGameStats", 0x80, 0x714478, 0x5bba00, 0x5bba20, 0x5bbbc0, {0x5bb9b0, 0}, true, false, &mask_screen_shot},
    // its Create leaves +0xc as the pool gave it, which the destructor follows: tested on its own below
    {"CUseCannedAction", 0x10, 0x6bd110, 0x468560, 0x4685a0, 0x468840, {0x4687e0, 0}, false},
};

using Create0 = void *(__cdecl *)();
DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
short &W(void *p, DWORD off) { return *reinterpret_cast<short *>(static_cast<BYTE *>(p) + off); }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }
unsigned text_hash(const void *z) { return fnv_text(2166136261u, static_cast<const ZString *>(z)->c_str()); }
BYTE *pt_of(void *p, DWORD off) { return at<BYTE *>(D(p, off)); }
const char *ztext(void *o, DWORD off) { return static_cast<ZString *>(static_cast<void *>(static_cast<BYTE *>(o) + off))->c_str(); }

// ---- Objects ---------------------------------------------------------------------------------------------

// CZaxSaveData's constructor 0x5bb6c0 on a pool block filled with a pattern, then destroyed (slot 1)
void test_save_data_ctor(Tally &t, unsigned seed)
{
    BYTE obj[2][0x34];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        BYTE *p = static_cast<BYTE *>(pool_alloc(0x34));
        memset(p, (BYTE)random(), 0x34);
        arena_on(side, (BYTE)seed);
        r[side] = at<DWORD(THISCALL *)(void *)>(0x5bb6c0)(p) == ptrv(p);
        memcpy(obj[side], p, 0x34);
        D(obj[side], 0x10) = D(obj[side], 0x10) == ptrv(p);   // the holder's owner: the object itself
        vcall<void *>(p, 1, 1);
        arena_off();
        side_end(side);
    }
    char d[120];
    wsprintfA(d, "[seed %u: eax this %d / %d, first difference +%x: %x / %x]", seed, r[0], r[1], g_diff_at, g_diff_a, g_diff_b);
    t.check(r[0] == r[1] && r[0] && same_region(obj[0], obj[1], 0x34) && same_region(g_arena[0], g_arena[1], g_used[0]),
            "CZaxSaveData::Ctor", d);
}

// The holder's deleting destructor 0x5bb700 with flags 0 and 1 (the save data's pointer arrays' 0x5bb730,
// 0x5bb060 and 0x5bccb0 are the Core ones of ptr_array_dtors.inc)
void test_small_dtors(Tally &t, unsigned seed)
{
    const DWORD fn = 0x5bb700, flags = seed & 1;
    BYTE obj[2][0x10];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        arena_on(side, (BYTE)seed);
        BYTE *o;
        if (fn == 0x5bb700) {
            o = static_cast<BYTE *>(pool_alloc(0xc));
            for (DWORD k = 0; k < 0xc; k += 4)
                D(o, k) = random();
        } else {
            o = static_cast<BYTE *>(heap_alloc(0x10));   // the arena
            D(o, 0) = random();
            const DWORD cap = rnd(5);
            D(o, 0xc) = cap;
            D(o, 8) = cap ? rnd(cap + 1) : 0;
            D(o, 4) = 0;
            if (cap || rnd(2)) {
                if (fn == 0x5bccb0)
                    D(o, 4) = ptrv(heap_alloc(cap * 4 + 4));
                else if (cap)
                    D(o, 4) = ptrv(pool_get_alloc(cap * 4));
            }
        }
        r[side] = at<DWORD(THISCALL *)(void *, DWORD)>(fn)(o, flags) == ptrv(o);
        memcpy(obj[side], o, fn == 0x5bb700 ? 0xc : 0x10);
        if (fn == 0x5bb700 && !flags)
            pool_free(o, 0xc);
        arena_off();
        side_end(side);
    }
    char d[120];
    wsprintfA(d, "[0x%x seed %u flags %u: eax %x / %x, first difference +%x: %x / %x]", fn, seed, flags, r[0], r[1], g_diff_at, g_diff_a,
              g_diff_b);
    // a freed block's bytes are the pool's or the arena's (the stand-in leaves them)
    const bool freed = flags && fn == 0x5bb700;
    t.check(r[0] == r[1] && (freed || same_region(obj[0], obj[1], fn == 0x5bb700 ? 0xc : 0x10)) && same_region(g_arena[0], g_arena[1], g_used[0]),
            "pointer array / holder", d);
}

// ---- The fake world ----------------------------------------------------------------------------------------

constexpr int NOBJ = 12;
struct FakeObject {   // game types, entities, items, players' data; slot 2 the class
    DWORD vftable;
    BYTE body[0x11c];
    CClassInfo *cls;
    DWORD id;
};
struct World {
    FakeObject objs[NOBJ];
    DWORD world_vtable, settings[8];
    BYTE net_game[0x20];
    DWORD app;
    DWORD answers[16];
    BYTE guards[0x10];
};
World g_world;
World g_after[2];
void *g_vtable[80];
void *g_world_vt[4];
CClassInfo *g_classes[6];
enum { C_CHARACTER, C_SALVAGE_KING, C_CTF, C_MINERAL, C_OBJECT, C_ACTION };

int obj_id(const void *p)
{
    for (int i = 0; i < NOBJ; i++)
        if (p == &g_world.objs[i])
            return 0x10 + i;
    return p ? 0x99 : 0;
}
DWORD ans(int k) { return g_world.answers[k]; }

DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
CClassInfo *THISCALL f_class(FakeObject *o) { return o->cls; }
DWORD THISCALL f_lead(FakeObject *o) { ev(0x127, (DWORD)obj_id(o)); return result(0x27); }
DWORD THISCALL f_count(FakeObject *o) { ev(0x141, (DWORD)obj_id(o)); return D(o->body, 0x20) % 50; }
DWORD THISCALL f_kill_message(FakeObject *o, void *a, void *b) { ev(0x148, (DWORD)obj_id(o), (DWORD)obj_id(a), (DWORD)obj_id(b)); return result(0x48); }
DWORD THISCALL f_take(FakeObject *o, DWORD a, DWORD n) { ev(0x152, (DWORD)obj_id(o), a, n); return result(0x52); }
void *THISCALL f_world_net_game(void *) { ev(0x161); return g_world.net_game; }

// Stand-ins
BYTE *THISCALL s_net_player(void *entity)
{
    ev(0x701, (DWORD)obj_id(entity));
    const int k = (int)((ans(0) >> (obj_id(entity) & 7)) & 3);
    return k ? reinterpret_cast<BYTE *>(&g_world.objs[8 + k]) : nullptr;   // objs 9-11 are players' data
}
// the score setters as the originals: the short, a bit in +0x10c when it changes. eax: the argument.
template <DWORD OFF, DWORD BIT> DWORD THISCALL s_set(BYTE *player, int v)
{
    ev(0x702, OFF, (DWORD)v, (DWORD)obj_id(player));
    if (W(player, OFF) != (short)v)
        D(player, 0x10c) |= BIT;
    W(player, OFF) = (short)v;
    return (DWORD)v;
}
DWORD THISCALL s_find_or_fail(void *list, const char *name, int hint)
{
    ev(0x703, ptrv(list), fnv_text(2166136261u, name), (DWORD)hint);
    return fnv_text(2166136261u, name) % 7;
}
DWORD __fastcall s_item_count(void *part, DWORD def) { ev(0x704, (DWORD)obj_id(part), def); return (ans(1) >> (def & 15)) % 40; }
void *THISCALL s_inventory(void *part) { ev(0x705, (DWORD)obj_id(part)); return ans(2) & 3 ? &g_world.objs[5] : nullptr; }
void *THISCALL s_find(void *inv, DWORD def) { ev(0x706, (DWORD)obj_id(inv), def); return (ans(3) >> (def & 7)) & 3 ? &g_world.objs[6 + def % 2] : nullptr; }
void *THISCALL s_game_type(void *a) { ev(0x707, a == *at<void **>(0x713f14)); return ans(4) & 7 ? &g_world.objs[0] : nullptr; }
void THISCALL s_end_match(void *g) { ev(0x708, (DWORD)obj_id(g)); }

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x5bac80, (void *)&s_net_player},        {0x5ba360, (void *)&s_set<0xee, 0x80>}, {0x5ba390, (void *)&s_set<0xf0, 0x100>},
    {0x5ba3c0, (void *)&s_set<0xf2, 0x200>},  {0x5ba3f0, (void *)&s_set<0xf4, 0x400>}, {0x5ba420, (void *)&s_set<0xf6, 4>},
    {0x591fc0, (void *)&s_find_or_fail},      {0x426860, (void *)&s_item_count},      {0x4267e0, (void *)&s_inventory},
    {0x4258b0, (void *)&s_find},              {0x59ff80, (void *)&s_game_type},       {0x51c650, (void *)&s_end_match},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_stand_patches[NSTANDS];

void init_world_tables()
{
    for (int i = 0; i < 80; i++)
        g_vtable[i] = (void *)&f_bad;
    g_vtable[2] = (void *)&f_class;
    g_vtable[27] = (void *)&f_lead;
    g_vtable[41] = (void *)&f_count;
    g_vtable[48] = (void *)&f_kill_message;
    g_vtable[52] = (void *)&f_take;
    for (int i = 0; i < 4; i++)
        g_world_vt[i] = (void *)&f_bad;
    g_world_vt[1] = (void *)&f_world_net_game;
    const DWORD getters[6] = {0x49a930, 0x5612a0, 0x469050, 0x5af480, 0x4163b0, 0x41a650};
    for (int i = 0; i < 6; i++)
        g_classes[i] = at<CClassInfo *(__cdecl *)()>(getters[i])();
}

void setup(unsigned seed)
{
    g_seed = seed;
    World &w = g_world;
    memset(&w, 0, sizeof w);
    for (int i = 0; i < NOBJ; i++) {
        FakeObject &o = w.objs[i];
        o.vftable = ptrv(g_vtable);
        for (DWORD k = 0; k < sizeof o.body; k += 4)
            D(o.body, k) = random();
        o.id = (DWORD)i;
        o.cls = g_classes[C_OBJECT];
    }
    // 0: the game type (Salvage King or Capture the Flag), 1-4 entities, 5 the inventory, 6/7 items,
    // 9-11 the players' data (team +0x14, the scores as shorts)
    w.objs[0].cls = g_classes[rnd(4) ? C_SALVAGE_KING : C_CTF];
    for (int i = 1; i <= 4; i++)
        w.objs[i].cls = g_classes[rnd(5) ? C_CHARACTER : C_OBJECT];
    for (int i = 6; i <= 7; i++)
        w.objs[i].cls = g_classes[rnd(4) ? C_MINERAL : C_OBJECT];
    for (int i = 9; i <= 11; i++) {
        BYTE *p = w.objs[i].body - 4;   // the data starts at the object
        D(p, 0x14) = rnd(3);
        for (DWORD k = 0xec; k <= 0xf6; k += 2)
            W(p, k) = (short)(rnd(5) ? rnd(200) - 50 : random());
        D(p, 0x10c) = rnd(2) ? 0 : random();
    }
    // the game type's fields: the frag points, the hues, the force flag, the mineral values
    BYTE *g = w.objs[0].body - 4;
    D(g, 0xc8) = rnd(3) ? rnd(20) - 5 : random();
    D(g, 0xcc) = rnd(3) ? rnd(20) - 10 : random();
    D(g, 0xd0) = rnd(3) ? rnd(6) - 3 : random();
    const float values[] = {0.0f, 1.0f, 3.0f, 0.5f, -2.25f, 1e9f, 1e-7f, 7.75f};
    if (rnd(4)) {
        float a = values[rnd(8)], b = values[rnd(8)];
        memcpy(g + 0xc8, &a, 4);
        memcpy(g + 0xcc, &b, 4);
    }
    g[0xf0] = (BYTE)(rnd(3) ? rnd(2) : random());
    w.world_vtable = ptrv(g_world_vt);
    D(w.net_game, 8) = rnd(4) ? ptrv(w.settings) : 0;
    w.settings[5] = rnd(3) ? rnd(60) : random();   // +0x14 the frag limit
    w.app = rnd(6) ? 1 : 0;
    for (int k = 0; k < 16; k++)
        w.answers[k] = random();
    w.guards[0] = (BYTE)random();
}

enum Fn { F_FRAG, F_HUE, F_CTF45, F_MINERALS, F_SK45, F_EXECUTE, NFN };
const char *const FN_NAMES[NFN] = {"CCaptureTheFlagGameType::Frag", "CCaptureTheFlagGameType::TeamHue", "CCaptureTheFlagGameType::Slot45",
                                   "CSalvageKingGameType::Minerals", "CSalvageKingGameType::Slot45",
                                   "CSalvageKingScoreMineralsAction::Execute"};

constexpr DWORD GUARDS[3] = {0x713168, 0x71315c, 0x713160};
DWORD g_saved_guards[3], g_saved_world, g_saved_app;

// One case on a side; real (a game type read from a file, with its vtable copy) instead of the fake
BYTE *g_real_type[2];
DWORD run_fn(Fn f, BYTE *game_type)
{
    World &w = g_world;
    void *entity = &w.objs[1 + rnd(4)], *other = rnd(5) ? (void *)&w.objs[1 + rnd(4)] : nullptr;
    switch (f) {
    case F_FRAG: return at<DWORD(THISCALL *)(void *, void *, void *)>(0x469800)(game_type, rnd(6) ? entity : nullptr, other);
    case F_HUE: {
        DWORD hue = random();
        at<void(THISCALL *)(void *, void *, DWORD *)>(0x4698b0)(game_type, w.objs[9 + rnd(3)].body - 4, &hue);
        ev(0x800, hue);
        return 0;
    }
    case F_CTF45: return at<DWORD(__cdecl *)()>(0x4697f0)();
    case F_MINERALS: {
        // the original calls the setters on a null player too: a player always exists here
        const DWORD keep = w.answers[0];
        w.answers[0] |= 0x5555;
        const DWORD r = at<DWORD(THISCALL *)(void *, void *, DWORD)>(0x5616b0)(game_type, entity, random());
        w.answers[0] = keep;
        return r;
    }
    case F_SK45: return at<DWORD(__cdecl *)()>(0x5618e0)();
    case F_EXECUTE: {
        BYTE *a = static_cast<BYTE *>(at<Create0>(0x561940)());
        const DWORD r = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *)>(0x561ab0)(a, random(), random(), random(),
                                                                                              rnd(6) ? entity : nullptr);
        vcall<void *>(a, 1, 1);
        return r & 0xff;
    }
    default: return 0;
    }
}

void test_fn(Tally &t, Fn f, unsigned seed, bool real = false)
{
    DWORD r[2];
    BYTE type_after[2][0x100];
    for (int side = 0; side < 2; side++) {
        setup(seed);
        log_reset(side);
        side_begin(side);
        for (int i = 0; i < NSTANDS; i++)
            patch_jump(g_stand_patches[i], STANDS[i].address, STANDS[i].fn);
        for (int i = 0; i < 3; i++)
            g_saved_guards[i] = *at<DWORD *>(GUARDS[i]);
        *at<BYTE *>(GUARDS[0]) = g_world.guards[0];
        *at<DWORD *>(GUARDS[1]) = 0x5e7c0001;
        *at<DWORD *>(GUARDS[2]) = 0x5e7c0002;
        g_saved_world = *at<DWORD *>(0x6c2080);
        g_saved_app = *at<DWORD *>(0x713f14);
        *at<DWORD *>(0x6c2080) = ptrv(&g_world.world_vtable);
        *at<DWORD *>(0x713f14) = g_world.app ? ptrv(&g_world.objs[4]) : 0;
        BYTE *game_type = real ? g_real_type[side] : g_world.objs[0].body - 4;
        if (real && f == F_EXECUTE)
            g_world.objs[0].cls = g_classes[C_SALVAGE_KING];
        arena_on(side, 0x5c);
        g_seed = seed * 13u + 5u;
        r[side] = run_fn(f, game_type);
        arena_off();
        memcpy(type_after[side], game_type + 0xc8, real ? 0x14 : 0x38);
        ev(0x900, *at<BYTE *>(GUARDS[0]), *at<DWORD *>(GUARDS[1]), *at<DWORD *>(GUARDS[2]));
        *at<DWORD *>(0x713f14) = g_saved_app;
        *at<DWORD *>(0x6c2080) = g_saved_world;
        for (int i = 2; i >= 0; i--)
            *at<DWORD *>(GUARDS[i]) = g_saved_guards[i];
        for (int i = NSTANDS - 1; i >= 0; i--)
            unpatch(g_stand_patches[i]);
        side_end(side);
        memcpy(&g_after[side], &g_world, sizeof g_world);
    }
    const bool same = r[0] == r[1] && same_log() && same_region(type_after[0], type_after[1], real ? 0x14 : 0x38) &&
                      same_region(reinterpret_cast<BYTE *>(&g_after[0]), reinterpret_cast<BYTE *>(&g_after[1]), sizeof(World)) &&
                      g_used[0] == g_used[1] && same_region(g_arena[0], g_arena[1], g_used[0]);
    char d[200];
    wsprintfA(d, "[seed %u: eax %x / %x, log %d / %d, first difference +%x: %x / %x]", seed, r[0], r[1], g_nlog[0] / 4, g_nlog[1] / 4,
              g_diff_at, g_diff_a, g_diff_b);
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, FN_NAMES[f], d);
}

// ---- CZaxSaveData's difficulty on a fake difficulty list -----------------------------------------------------

constexpr DWORD DIFFICULTY_LIST = 0x6c2a00;
struct Record {
    char name[0x78];
    void *object;
    BYTE keep, in_use, applied, loading;
};
static_assert(sizeof(Record) == 0x80);
Record g_records[8];
FakeObject g_difficulties[4];
void *g_list_vtable[40];
BYTE g_saved_list[0x140];
const char *const RECORD_NAMES[4] = {"Easy.dif", "Normal.dif", "Hard/Hard.dif", "Insane"};

const char *THISCALL l_name(void *, DWORD i) { ev(0x401, i); return RECORD_NAMES[i % 4]; }
void *THISCALL l_make(void *, const char *folder, const char *name, void **out)
{
    ev(0x425, fnv_text(2166136261u, folder), fnv_text(2166136261u, name), (DWORD)(reinterpret_cast<Record *>(reinterpret_cast<BYTE *>(out) - 0x78) - g_records));
    return &g_difficulties[fnv_text(1, name) % 4];
}
DWORD THISCALL d_apply(FakeObject *o) { ev(0x407, o->id); return result(7); }
void *g_difficulty_vtable[10];

void test_difficulty(Tally &t, unsigned seed)
{
    DWORD r[2], obj[2];
    Record after[2][8];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        for (int i = 0; i < 40; i++)
            g_list_vtable[i] = (void *)&f_bad;
        g_list_vtable[1] = (void *)&l_name;
        g_list_vtable[37] = (void *)&l_make;
        for (int i = 0; i < 10; i++)
            g_difficulty_vtable[i] = (void *)&f_bad;
        g_difficulty_vtable[7] = (void *)&d_apply;
        for (int i = 0; i < 4; i++) {
            g_difficulties[i].vftable = ptrv(g_difficulty_vtable);
            g_difficulties[i].id = (DWORD)i;
        }
        const DWORD count = 1 + rnd(5);
        const DWORD index = rnd(8);
        for (DWORD i = 0; i < 8; i++) {
            Record &e = g_records[i];
            memset(&e, 0, sizeof e);
            lstrcpyA(e.name, RECORD_NAMES[i % 4]);
            e.object = rnd(2) ? &g_difficulties[i % 4] : nullptr;
            e.in_use = (BYTE)rnd(2);
            e.applied = (BYTE)rnd(3) == 0;
            e.loading = (BYTE)(rnd(5) == 0);
        }
        // the original calls slot 7 on null when the clamped record is being loaded: not such a case
        const DWORD clamped = index < count ? index : 0;
        if (!g_records[index].applied)
            g_records[clamped].loading = 0;
        const bool no_records = rnd(8) == 0;
        BYTE *o = static_cast<BYTE *>(at<Create0>(0x5bb160)());
        D(o, 0x28) = random();
        log_reset(side);
        side_begin(side);
        memcpy(g_saved_list, at<void *>(DIFFICULTY_LIST), sizeof g_saved_list);
        D(at<void *>(DIFFICULTY_LIST), 0) = ptrv(g_list_vtable);
        D(at<void *>(DIFFICULTY_LIST), 0x30) = no_records ? 0 : ptrv(g_records);
        D(at<void *>(DIFFICULTY_LIST), 0x34) = count;
        lstrcpyA(at<char *>(DIFFICULTY_LIST + 0x38), rnd(2) ? "Resources/Difficulty" : "");
        r[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x5bb780)(o, index);
        if (r[side] == ptrv(g_records))
            r[side] = 0x5e7d0000;
        obj[side] = D(o, 0x28);
        memcpy(at<void *>(DIFFICULTY_LIST), g_saved_list, sizeof g_saved_list);
        side_end(side);
        memcpy(after[side], g_records, sizeof g_records);
        vcall<void *>(o, 1, 1);
    }
    char d[120];
    wsprintfA(d, "[seed %u: eax %x / %x, log %d / %d]", seed, r[0], r[1], g_nlog[0] / 4, g_nlog[1] / 4);
    if (!same_log() && t.mismatches < 4)
        log_first_difference();
    t.check(r[0] == r[1] && obj[0] == obj[1] && same_log() && memcmp(after[0], after[1], sizeof after[0]) == 0, "CZaxSaveData::SetDifficulty", d);
}

// ---- CZaxSaveGameStats' names -------------------------------------------------------------------------------------

FakeObject g_missions[3];
DWORD THISCALL s_mission_number(FakeObject *m) { ev(0x501, m->id); return m->id * 7 + 1; }
const char *const PIECES[] = {"\\m", "\\l", " ", "\\\\", "\\x", "Save ", "\\", "M8", "Main", "\\m\\l", "-"};
const char *const MAPS[] = {"01 Main", "Main", "09 Assimilate Cave1", "\\", "M8", "A"};
const char *const NAMES[] = {"", "Main", "My Save", "01 Main M8 x", "M15 Main\\Main", "\\\\", "MM8M8", "Main Main"};

void fill_stats(BYTE *o)
{
    auto set = [](BYTE *z, const char *t) { at<void *(THISCALL *)(void *, const char *)>(0x4df010)(z, t); };
    char pattern[64] = "";
    for (DWORD k = rnd(5); k; k--)
        lstrcatA(pattern, PIECES[rnd(sizeof PIECES / sizeof PIECES[0])]);
    set(o + 0x7c, pattern);
    set(o + 0x5c, MAPS[rnd(sizeof MAPS / sizeof MAPS[0])]);
    set(o + 8, rnd(2) ? "Sat Oct 25 16:30:00 2026" : "");
    D(o, 0xc) = rnd(3) ? rnd(4) : random();
    D(o, 0x10) = rnd(3) ? ptrv(&g_missions[rnd(3)]) : 0;
}

enum StatsFn { S_NAME, S_PATTERN, S_DESCRIPTION, S_COMPARE, NSTATS };
const char *const STATS_NAMES[NSTATS] = {"CZaxSaveGameStats::Name", "CZaxSaveGameStats::SetPattern", "CZaxSaveGameStats::Description",
                                         "CZaxSaveGameStats::Compare"};

void test_stats(Tally &t, StatsFn f, unsigned seed)
{
    unsigned h[2];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        for (DWORD i = 0; i < 3; i++)
            g_missions[i].id = i;
        BYTE *a = static_cast<BYTE *>(at<Create0>(0x5bba00)()), *b = static_cast<BYTE *>(at<Create0>(0x5bba00)());
        fill_stats(a);
        fill_stats(b);
        ZString name, out;
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&name, NAMES[rnd(sizeof NAMES / sizeof NAMES[0])]);
        log_reset(side);
        side_begin(side);
        CodePatch p;
        patch_jump(p, 0x512280, (void *)&s_mission_number);
        r[side] = 0;
        h[side] = 0;
        switch (f) {
        case S_NAME:
            r[side] = at<DWORD(THISCALL *)(void *, void *)>(0x5bc810)(a, &out) == ptrv(&out);
            h[side] = text_hash(&out);
            break;
        case S_PATTERN:
            // an empty map name never ends (in the original too)
            if (!ztext(a, 0x5c)[0])
                at<void *(THISCALL *)(void *, const char *)>(0x4df010)(a + 0x5c, "Main");
            at<void(THISCALL *)(void *, void *)>(0x5bc910)(a, &name);
            h[side] = text_hash(a + 0x7c);
            break;
        case S_DESCRIPTION:
            r[side] = at<DWORD(THISCALL *)(void *, void *)>(0x5bcb60)(a, &out) == ptrv(&out);
            h[side] = text_hash(&out);
            break;
        case S_COMPARE:
            r[side] = at<DWORD(THISCALL *)(void *, void *, DWORD)>(0x5bcc10)(a, b, rnd(3));
            break;
        default: break;
        }
        unpatch(p);
        side_end(side);
        if (f == S_NAME || f == S_DESCRIPTION)
            at<void(THISCALL *)(void *)>(0x4defd0)(&out);
        at<void(THISCALL *)(void *)>(0x4defd0)(&name);
        h[side] = fnv(h[side], a + 0xc, 4);
        D(a, 0x10) = 0;
        D(b, 0x10) = 0;
        vcall<void *>(a, 1, 1);
        vcall<void *>(b, 1, 1);
    }
    char d[120];
    wsprintfA(d, "[seed %u: eax %x / %x, text %x / %x, log %d / %d]", seed, r[0], r[1], h[0], h[1], g_nlog[0] / 4, g_nlog[1] / 4);
    t.check(r[0] == r[1] && h[0] == h[1] && same_log(), STATS_NAMES[f], d);
}

// ---- Real data ---------------------------------------------------------------------------------------------------

BYTE *create_from_file(const char *path)
{
    ZString p;
    original<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&p, path);
    void *o = at<void *(__fastcall *)(ZString *, DWORD, DWORD, void **)>(0x4164c0)(&p, 0, 0, nullptr);
    original<void(THISCALL *)(ZString *)>(0x4defd0)(&p);
    return static_cast<BYTE *>(o);
}
const char *const FILES[3] = {"Rules.txt", "Resources/Game Types/MultiPlayer/Capture The Flag.zgt",
                              "Resources/Game Types/MultiPlayer/Salvage King.zgt"};
const DWORD FILE_SIZES[3] = {0x3c, 0xfc, 0xdc};
// Strings by text, owned objects by class, the game types' inherited part left to their own checks
bool same_read(BYTE *a, BYTE *b, int k)
{
    if (!a || !b || D(a, 0) != D(b, 0))
        return false;
    if (k == 0) {
        if (lstrcmpA(ztext(a, 8), ztext(b, 8)) || lstrcmpA(ztext(a, 0xc), ztext(b, 0xc)))
            return false;
        if (memcmp(a + 0x10, b + 0x10, 0x10) || D(a, 0x30) != D(b, 0x30) || (D(a, 0x38) == 0) != (D(b, 0x38) == 0))
            return false;
        return !D(a, 0x38) || D(pt_of(a, 0x38), 0) == D(pt_of(b, 0x38), 0);
    }
    if (k == 1)   // +0xf1-+0xf3 is the padding after the byte +0xf0 (pool contents)
        return memcmp(a + 0xc8, b + 0xc8, 0x29) == 0 && memcmp(a + 0xf4, b + 0xf4, 8) == 0;
    return memcmp(a + 0xc8, b + 0xc8, FILE_SIZES[k] - 0xc8) == 0;
}

void test_real(Tally &t, Tally &slots)
{
    BYTE *made[2][3];
    for (int k = 0; k < 3; k++) {
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            made[side][k] = create_from_file(FILES[k]);
            side_end(side);
        }
        char d[120];
        wsprintfA(d, "[%s: %x / %x]", FILES[k], ptrv(made[0][k]), ptrv(made[1][k]));
        t.check(same_read(made[0][k], made[1][k], k), "read", d);
    }
    // The two game types through their slots (their vtables copied, slots 27 and 48 recorded)
    static void *vt[2][64];
    for (int k = 1; k < 3; k++) {
        if (!made[0][k] || !made[1][k])
            continue;
        DWORD saved[2];
        for (int side = 0; side < 2; side++) {
            saved[side] = D(made[side][k], 0);
            memcpy(vt[side], at<void *>(saved[side]), sizeof vt[side]);
            vt[side][27] = (void *)&f_lead;
            vt[side][48] = (void *)&f_kill_message;
            D(made[side][k], 0) = ptrv(vt[side]);
            g_real_type[side] = made[side][k];
        }
        const Fn fns[3] = {k == 1 ? F_FRAG : F_MINERALS, k == 1 ? F_HUE : F_EXECUTE, k == 1 ? F_CTF45 : F_SK45};
        for (Fn f : fns)
            for (unsigned s = 0; s < 20; s++)
                test_fn(slots, f, 0x7700000u + 0x10000u * (unsigned)k + 0x100u * (unsigned)f + s, true);
        for (int side = 0; side < 2; side++)
            D(made[side][k], 0) = saved[side];
    }
    for (int k = 0; k < 3; k++)
        for (int side = 0; side < 2; side++)
            if (made[side][k]) {
                side_begin(side);
                vcall<void *>(made[side][k], 1, 1);
                side_end(side);
            }
}

// ---- CUseCannedAction ------------------------------------------------------------------------------------------

// Create on a pattern block and CreateArray (0-3) in the arena; +0xc (not set by the constructor) cleared
// before the destructors (slot 1)
void test_use_canned_objects(Tally &t, unsigned seed)
{
    BYTE obj[2][0x10];
    DWORD used[2];
    const int count = (int)(seed % 4);
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        BYTE *p = static_cast<BYTE *>(pool_alloc(0x10));
        memset(p, (BYTE)random(), 0x10);
        pool_free(p, 0x10);
        arena_on(side, (BYTE)seed);
        BYTE *o = static_cast<BYTE *>(at<Create0>(0x468560)());
        memcpy(obj[side], o, 0xc);
        D(o, 0xc) = 0;
        vcall<void *>(o, 1, 1);
        BYTE *a = static_cast<BYTE *>(at<void *(__fastcall *)(int, int, int)>(0x4685a0)(count, 0, 0));
        for (int i = 0; i < count; i++)
            D(a, (DWORD)i * 0x10 + 0xc) = 0;
        if (a && count)
            vcall<void *>(a, 1, 3);
        used[side] = g_used[side];
        arena_off();
        side_end(side);
    }
    char d[120];
    wsprintfA(d, "[seed %u: heap %u / %u, first difference +%x: %x / %x]", seed, used[0], used[1], g_diff_at, g_diff_a, g_diff_b);
    t.check(used[0] == used[1] && same_region(obj[0], obj[1], 0xc) && same_region(g_arena[0], g_arena[1], used[0]), "CUseCannedAction", d);
}

constexpr DWORD CANNED_LIST = 0x6bcfa8;
BYTE g_saved_canned[0x140];
Record g_canned_records[4];
FakeObject g_canned[4];   // canned objects: +8 the object, +0xc how it is used
void *g_canned_vtable[40];
const char *THISCALL c_name(void *, DWORD i) { ev(0x441, i); return RECORD_NAMES[i % 4]; }
void *THISCALL c_make(void *, const char *folder, const char *name, void **out)
{
    ev(0x442, fnv_text(1, folder), fnv_text(1, name), (DWORD)(reinterpret_cast<Record *>(reinterpret_cast<BYTE *>(out) - 0x78) - g_canned_records));
    return &g_canned[fnv_text(1, name) % 4];
}
// the list's loader 0x55e090: the record's object made (the destructor reads it without a test)
DWORD THISCALL c_load(void *list, DWORD i)
{
    ev(0x443, list == at<void *>(CANNED_LIST), i);
    g_canned_records[i & 3].object = &g_canned[i & 3];
    return result(0x43);
}
void *THISCALL c_clone(FakeObject *o, DWORD a, DWORD b) { ev(0x444, (DWORD)obj_id(o), a, b); return &g_world.objs[3]; }
DWORD THISCALL c_execute(void *a, DWORD a1, DWORD a2, DWORD a3, DWORD a4, void *caller, void *field)
{
    ev(0x445, (DWORD)obj_id(a), a1 ^ a2 * 3, a3 ^ a4 * 5);
    ev(0x446, ptrv(caller) & 0xfff, ptrv(field) - ptrv(caller));
    return result(0x45);
}
DWORD THISCALL c_delete(FakeObject *o, DWORD flags) { ev(0x447, (DWORD)obj_id(o), flags); return result(0x47); }

// Execute (slot 23) and the destructor with +0xc set, on a fake canned object list (the global's vtable,
// records and folder swapped for the case): canned objects with an action, another object or none, used
// as it is (0), cloned (1) or neither (2); records loaded, missing (the original's 0x55e090 or the inline
// loader through slots 1 and 37) or being loaded (not for the canned object the code reads unchecked)
void test_use_canned(Tally &t, unsigned seed, bool execute)
{
    DWORD r[2];
    BYTE after[2][0x10];
    for (int side = 0; side < 2; side++) {
        setup(seed);
        for (int i = 0; i < 40; i++)
            g_canned_vtable[i] = (void *)&f_bad;
        g_canned_vtable[1] = (void *)&c_name;
        g_canned_vtable[37] = (void *)&c_make;
        for (int i = 0; i < 4; i++) {
            FakeObject &c = g_canned[i];
            memset(&c, 0, sizeof c);
            c.vftable = ptrv(g_vtable);
            c.cls = g_classes[C_OBJECT];
            c.id = 0x40 + (DWORD)i;
            D(&c, 8) = rnd(5) ? ptrv(&g_world.objs[1 + rnd(4)]) : 0;
            D(&c, 0xc) = rnd(6) ? rnd(2) : 2;
        }
        for (int i = 1; i <= 4; i++)
            g_world.objs[i].cls = g_classes[rnd(3) ? C_ACTION : C_OBJECT];
        g_world.objs[3].vftable = ptrv(g_vtable);
        const DWORD count = 1 + rnd(4), index = rnd(6);
        const DWORD clamped = index < count ? index : 0;
        for (DWORD i = 0; i < 4; i++) {
            Record &e = g_canned_records[i];
            memset(&e, 0, sizeof e);
            lstrcpyA(e.name, RECORD_NAMES[i]);
            e.object = rnd(3) ? &g_canned[i] : nullptr;
            e.loading = (BYTE)(i != clamped && rnd(5) == 0);
        }
        BYTE *o = static_cast<BYTE *>(at<Create0>(0x468560)());
        D(o, 8) = index;
        D(o, 0xc) = execute ? random() : rnd(4) ? ptrv(&g_world.objs[5]) : 0;
        g_world.objs[5].vftable = ptrv(g_vtable);
        log_reset(side);
        side_begin(side);
        memcpy(g_saved_canned, at<void *>(CANNED_LIST), sizeof g_saved_canned);
        D(at<void *>(CANNED_LIST), 0) = ptrv(g_canned_vtable);
        D(at<void *>(CANNED_LIST), 0x30) = ptrv(g_canned_records);
        D(at<void *>(CANNED_LIST), 0x34) = count;
        lstrcpyA(at<char *>(CANNED_LIST + 0x38), rnd(2) ? "Resources/Canned Objects" : "");
        CodePatch patches[4];
        patch_jump(patches[0], 0x55e090, (void *)&c_load);
        patch_jump(patches[1], 0x416760, (void *)&c_clone);
        patch_jump(patches[2], 0x41a720, (void *)&c_execute);
        void *saved_delete = g_vtable[1];
        g_vtable[1] = (void *)&c_delete;
        if (execute) {
            r[side] = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x468950)(o, random(), random(), random(), random()) & 0xff;
            memcpy(after[side], o, 0x10);
            D(after[side], 0xc) = (DWORD)obj_id(at<void *>(D(o, 0xc)));
        } else {
            r[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x468600)(o, 0) == ptrv(o);
            memcpy(after[side], o, 0x10);
        }
        g_vtable[1] = saved_delete;
        for (int i = 2; i >= 0; i--)
            unpatch(patches[i]);
        memcpy(at<void *>(CANNED_LIST), g_saved_canned, sizeof g_saved_canned);
        side_end(side);
        if (execute) {
            D(o, 0xc) = 0;
            vcall<void *>(o, 1, 1);
        } else
            pool_free(o, 0x10);
    }
    char d[120];
    wsprintfA(d, "[seed %u: eax %x / %x, log %d / %d]", seed, r[0], r[1], g_nlog[0] / 4, g_nlog[1] / 4);
    if (!same_log() && t.mismatches < 4)
        log_first_difference();
    t.check(r[0] == r[1] && same_log() && memcmp(after[0], after[1], 0x10) == 0, execute ? "CUseCannedAction::Execute" : "CUseCannedAction::DeletingDtor", d);
}

}  // namespace

ZAX_CHECKS(game_types)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_game_types;
    if (!arena_setup()) {
        log("Check game types: no arena");
        return;
    }
    init_world_tables();
    Tally objects{"game types: Create, CreateArray, destructors, constructors"}, getters{"game types: class getters"},
        fields{"game types: RegisterFields"}, registered{"game types: registered descriptors and classes (real data)"};
    // RegisterFields keeps the element descriptor of the custom rules list in 0x713148; the static
    // initialiser 0x560650 set its class (+0x2c) afterwards, which the made ones lack: 0 while the
    // registered descriptors are compared
    const DWORD saved_element = *at<DWORD *>(0x713148);
    const DWORD saved_class = saved_element ? D(at<void *>(saved_element), 0x2c) : 0;
    if (saved_element)
        D(at<void *>(saved_element), 0x2c) = 0;
    // the registered "Screen Shot" descriptor's undefined colour bytes (see mask_screen_shot)
    WORD saved_color = 0;
    CFieldDesc *screen_shot = nullptr;
    if (CClassInfo *k = *at<CClassInfo **>(0x714478))
        for (DWORD i = k->num_inherited; i < k->num_fields; i++)
            if (k->fields[i] && lstrcmpA(k->fields[i]->name.c_str(), "Screen Shot") == 0)
                screen_shot = k->fields[i];
    if (screen_shot) {
        memcpy(&saved_color, reinterpret_cast<BYTE *>(screen_shot) + 0x3e, 2);
        memset(reinterpret_cast<BYTE *>(screen_shot) + 0x3e, 0, 2);
    }
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    *at<DWORD *>(0x713148) = saved_element;
    if (saved_element)
        D(at<void *>(saved_element), 0x2c) = saved_class;
    if (screen_shot)
        memcpy(reinterpret_cast<BYTE *>(screen_shot) + 0x3e, &saved_color, 2);
    if (saved_color)
        log("  the registered Screen Shot colour's undefined bytes: %x", saved_color);
    for (unsigned k = 0; k < (unsigned)scaled(40); k++)
        test_save_data_ctor(objects, k);
    for (unsigned k = 0; k < (unsigned)scaled(40); k++)
        test_small_dtors(objects, k);
    for (unsigned k = 0; k < (unsigned)scaled(40); k++)
        test_use_canned_objects(objects, k);
    objects.report();
    getters.report();
    fields.report();
    registered.report();

    Tally behaviour{"game types: the game types' slots and the scoring action (fakes)"};
    for (int f = 0; f < NFN; f++) {
        log("  %s", FN_NAMES[f]);   // the last line before a case that ends the game
        for (unsigned s = 0; s < (unsigned)scaled(300); s++)
            test_fn(behaviour, (Fn)f, 0x100000u * (unsigned)(f + 1) + s);
    }
    for (unsigned k = 0; k < (unsigned)scaled(300); k++) {
        test_use_canned(behaviour, 0x600000u + k, true);
        test_use_canned(behaviour, 0x700000u + k, false);
    }
    behaviour.report();
    Tally save{"game types: save data difficulty, the save game names, descriptions and order"};
    for (unsigned s = 0; s < (unsigned)scaled(300); s++)
        test_difficulty(save, s);
    for (int f = 0; f < NSTATS; f++)
        for (unsigned s = 0; s < (unsigned)scaled(300); s++)
            test_stats(save, (StatsFn)f, 0x200000u * (unsigned)(f + 1) + s);
    save.report();

    // The running game's canned objects (read only): what "Use" they hold (the value CUseCannedAction tests)
    if (const BYTE *records = at<const BYTE *>(D(at<void *>(CANNED_LIST), 0x30)))
        for (DWORD i = 0; i < D(at<void *>(CANNED_LIST), 0x34) && i < 16; i++)
            if (const BYTE *c = at<const BYTE *>(D(const_cast<BYTE *>(records + i * 0x80), 0x78)))
                log("  canned object %u '%s': object %x, use %u", i, reinterpret_cast<const char *>(records + i * 0x80),
                    D(const_cast<BYTE *>(c), 8), D(const_cast<BYTE *>(c), 0xc));
    // A canned object file of the game read once (the original, hooks out): its "Use" value
    {
        hooks_out();
        if (BYTE *c = create_from_file("Resources/Canned Objects/Drop Cystals and Ore.can")) {
            log("  Drop Cystals and Ore.can: object %x (vtable %x), use %u", D(c, 8), D(c, 8) ? *at<DWORD *>(D(c, 8)) : 0, D(c, 0xc));
            vcall<void *>(c, 1, 1);
        }
        hooks_in();
    }
    Tally real{"game types: Rules.txt and the two game types read from their files (real data)"};
    test_real(real, real);
    real.report();
}

}  // namespace zax
