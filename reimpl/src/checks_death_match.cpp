// Checks of death_match.cpp against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): class getters (with the list's element class 0x478840),
//    Create, CreateArray, deleting destructors, RegisterFields and the registered descriptors of
//    CDeathBehaviorListDefinition and CDeathMatchGameType; CDeathBehaviorListDefinition's RegisterFields
//    registers a file lookup with the static list 0x62e6a8, whose Add (0x419ea0) is a stand-in meanwhile.
//  - The slots in the fake world (checks_weapon_world.h): the death behaviour definition's apply, its
//    file reading (the object streams are recorders: no file is read or written), its destructor on a
//    fake with behaviours, the file lookup 0x4780c0 on the global list given fake contents, the list's
//    slots (use, clear, unload, file of an object, create an entry, the deleting destructor) on a fake list,
//    the exit destructor on the global given fake contents; the death match's score name, update, the
//    frag scoring (the net players, the kill message and the leads are recorders) and 0x479900.
//  - Real data: Init of a fresh list on the game's "Death Behaviors" folder (the names equal the global
//    list's; the registry entry taken off again), the static initializer on the global (its bytes saved
//    and put back, atexit a stand-in), the registered class infos.
#include "checks_weapon_world.h"
#include "checks_inventory_common.h"
#include "definitions.h"

namespace zax {

int hooks_death_match(Hook *out);

namespace {

using namespace ww;
using Getter = CClassInfo *(__cdecl *)();

const invcheck::ClassDef CLASSES[] = {
    {"CDeathBehaviorListDefinition", 0x18, 0x6bd9f4, 0x477d30, 0x477d80, 0x477fe0, {0x477ce0, 0x477f80}, true},
    {"CDeathMatchGameType", 0x100, 0x6bda0c, 0x478d30, 0x478d60, 0x478eb0, {0x478ce0, 0x478e50}, true},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];
enum { C_DEATH_DEF, C_DEATH_MATCH, C_LIST, C_GLOBAL, C_NONE };
constexpr DWORD DEATH_LIST = 0x6bd8b0;

// The static list's Add during the class tests: the item and a value of its own
DWORD g_static_added;
DWORD __fastcall fake_static_add(DWORD item)
{
    g_static_added = item;
    return 0x5a5a0000u + item;
}

// ---- The slots ---------------------------------------------------------------------------------------------

int g_self_class;
ZString g_out;                 // the ZString the file lookups write (outside the fake block)
DWORD g_entries_block;         // the records a fake list got (freed after the side unless the slot did)
DWORD g_static_records[1 + 6 * 0x20];
DWORD g_behaviors_block;       // the behaviour array a death behaviour definition got
DWORD g_made_entry;            // the object CreateEntry made (freed after the side)
SavedBytes g_saved_list;       // the global list during the exit destructor's cases

DWORD entity(Gen &g, int null_one_in = 0) { return ptrv(any_entity(g, null_one_in)); }
DWORD other(Gen &g) { return ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))); }

// Records of 0x80 bytes: names, objects (fakes or 0), flags; `heap` puts them into a heap block of the side
// (the count in front, as the lists allocate them), otherwise into the static block
DWORD make_records(Gen &g, DWORD n, bool heap, bool loading)
{
    BYTE *data;
    if (heap) {
        DWORD *block = static_cast<DWORD *>(heap_alloc((n + 1) * 0x80 + 4));
        block[0] = n + 1;
        data = reinterpret_cast<BYTE *>(block + 1);
        g_entries_block = ptrv(block);
    } else {
        g_static_records[0] = n + 1;
        data = reinterpret_cast<BYTE *>(g_static_records + 1);
    }
    for (DWORD i = 0; i < (n + 1) * 0x80; i++)
        data[i] = (BYTE)g.next();
    const char *const names[] = {"Standard", "Sub/Explode", "-Burn", "x", "Melt", "Fall"};
    for (DWORD i = 0; i <= n; i++) {
        BYTE *rec = data + i * 0x80;
        const char *name = names[i % 6];
        DWORD k = 0;
        for (; name[k]; k++)
            rec[k] = (BYTE)name[k];
        rec[k] = 0;
        fl<DWORD>(rec, 0x78) = g.next() % 3 ? (g.next() % 2 ? other(g) : ptrv(obj(O_DEF + (int)(g.next() % N_DEF)))) : 0;
        rec[0x7c] = (BYTE)(g.next() % 4 == 0);
        rec[0x7d] = (BYTE)(g.next() % 3 == 0);
        rec[0x7e] = (BYTE)(g.next() % 3 == 0);
        rec[0x7f] = loading ? (BYTE)(g.next() % 6 == 0) : 0;
    }
    return ptrv(data);
}
// A fake list (object 0 or the global): the recorder vtable, the aliases an empty pointer array, the
// folder, the records, a monitor (a fake whose slot 7 takes one argument)
void make_list(Gen &g, BYTE *s, bool heap, bool loading)
{
    memset(s, 0, 0x140);
    fl<DWORD>(s, 0) = ptrv(g_fvt[T_CATALOGUE]);
    fl<DWORD>(s, 0x1c) = 0x5ed558;
    const char folder[] = "Death Behaviors";
    memcpy(s + 0x38, folder, sizeof folder);
    const DWORD n = g.next() % 5;
    g_entries_block = 0;
    if (n || g.next() % 2) {
        fl<DWORD>(s, 0x30) = make_records(g, n, heap, loading);
        fl<DWORD>(s, 0x34) = n;
    }
    fl<DWORD>(s, 0x13c) = g.next() % 3 ? ptrv(obj(O_VIEW)) : 0;   // the monitor
}

void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    const int c = g_self_class;
    switch (c) {
    case C_DEATH_DEF: {
        for (DWORD i = 0; i < 0x40; i++)
            fl<DWORD>(s, i * 4) = g.next();
        fl<DWORD>(s, 0) = 0x5f0ba4;
        fl<DWORD>(s, 4) = 0;
        fl<DWORD>(s, 8) = 0x5f0c00;
        const DWORD n = g.next() % 5;
        fl<DWORD>(s, 0xc) = ptrv(obj(O_POINTS));
        for (DWORD i = 0; i < 8; i++)
            fl<DWORD>(obj(O_POINTS), i * 4) = g.next() % 4 ? other(g) : 0;
        fl<DWORD>(s, 0x10) = n;
        fl<DWORD>(s, 0x14) = n + g.next() % 3;
        break;
    }
    case C_DEATH_MATCH:
        for (DWORD i = 0; i < 0x40; i++)
            fl<DWORD>(s, i * 4) = g.next();
        fl<DWORD>(s, 0) = ptrv(g_fvt[T_GAMETYPE]);
        break;
    case C_LIST:
        make_list(g, s, false, false);
        break;
    default:
        break;
    }
    at<void *(THISCALL *)(void *)>(0x4dec90)(&g_out);
}

void a_none(Gen &, DWORD *) {}
void a_part(Gen &g, DWORD *a) { a[0] = entity(g); }
void a_flags0(Gen &, DWORD *a) { a[0] = 0; }
void a_value(Gen &g, DWORD *a) { a[0] = g.next(); }
void a_kill(Gen &g, DWORD *a)
{
    a[0] = entity(g, 6);
    a[1] = g.next() % 4 ? entity(g, 5) : a[0];
}
void a_load(Gen &g, DWORD *a)
{
    const char *const dirs[] = {"Death Behaviors", "", "Death Behaviors/Sub"};
    fl<DWORD>(obj(O_SELF), 0) = ptrv(g_fvt[T_OTHER]);
    a[0] = ptrv(dirs[g.next() % 3]);
    a[1] = ptrv(g.next() % 2 ? "Standard" : "Burn");
}
// The death behaviour definition's destructor: its behaviours in a heap block of the side
void a_destroy_behaviors(Gen &g, DWORD *a)
{
    BYTE *s = obj(O_SELF);
    const DWORD n = g.next() % 5, cap = n + g.next() % 3;
    if (cap) {
        DWORD *data = static_cast<DWORD *>(heap_alloc(cap * 4));
        for (DWORD i = 0; i < cap; i++)
            data[i] = i < n && g.next() % 4 ? other(g) : 0;
        fl<DWORD>(s, 0xc) = ptrv(data);
        g_behaviors_block = ptrv(data);
    } else {
        fl<DWORD>(s, 0xc) = 0;
        g_behaviors_block = 0;
    }
    fl<DWORD>(s, 0x10) = n;
    fl<DWORD>(s, 0x14) = cap;
    a[0] = 0;
}
void after_destroy_behaviors() { g_behaviors_block = 0; }   // the destructor freed it
// The global list's file lookup 0x4780c0 (ecx: the object, edx: out): the global given fake contents
// (static records; set by global_records once the world's globals are in)
void a_global_file_of(Gen &g, DWORD *a)
{
    make_records(g, g.next() % 5, false, true);
    a[0] = ptrv(&g_out);
}
void global_records()
{
    // the list's records (+0x30) and count (+0x34); one record's object is the object asked for now and then
    BYTE *data = reinterpret_cast<BYTE *>(g_static_records + 1);
    const DWORD n = g_static_records[0] - 1;
    *at<DWORD *>(DEATH_LIST + 0x30) = ptrv(data);
    *at<DWORD *>(DEATH_LIST + 0x34) = n;
    if (n && (fl<DWORD>(data, 0x78) & 1))
        fl<DWORD>(data + 0x80 * (fl<DWORD>(data, 0x7c) % n), 0x78) = ptrv(obj(O_SELF));
}
void a_index(Gen &g, DWORD *a) { a[0] = g.next() % (fl<DWORD>(obj(O_SELF), 0x34) + 1); }
void a_file_of(Gen &g, DWORD *a)
{
    BYTE *s = obj(O_SELF);
    const DWORD n = fl<DWORD>(s, 0x34);
    const DWORD k = g.next() % 3;
    a[0] = k == 0 && n && fl<DWORD>(s, 0x30) ? fl<DWORD>(at<BYTE *>(fl<DWORD>(s, 0x30) + 0x80 * (g.next() % n)), 0x78) : other(g);
    a[1] = ptrv(&g_out);
}
void a_create(Gen &g, DWORD *a)
{
    const char *const dirs[] = {"Death Behaviors", "", "Death Behaviors/Sub"};
    g_made_entry = ptrv(pool_alloc(0x18));   // the block the slot will get (freed after the side)
    pool_free(at<void *>(g_made_entry), 0x18);
    a[0] = ptrv(dirs[g.next() % 3]);
    a[1] = ptrv(g.next() % 2 ? "Standard" : "Burn");
    a[2] = g.next() % 4 ? ptrv(obj(O_OUT)) : 0;
}
// The list's deleting destructor on a fake list whose records are a heap block of the side
void a_list_dtor(Gen &g, DWORD *a)
{
    make_list(g, obj(O_SELF), true, false);
    a[0] = 0;
}
// The exit destructor 0x478420 on the global list given fake contents (records a heap block of the side;
// put back over the world's globals by global_list once those are in)
DWORD g_global_n, g_global_data;
void a_at_exit(Gen &g, DWORD *)
{
    save_bytes(g_saved_list, DEATH_LIST, 0x140);
    make_list(g, at<BYTE *>(DEATH_LIST), true, false);
    g_global_data = *at<DWORD *>(DEATH_LIST + 0x30);
    g_global_n = *at<DWORD *>(DEATH_LIST + 0x34);
}
void global_list()
{
    *at<DWORD *>(DEATH_LIST) = ptrv(g_fvt[T_CATALOGUE]);
    *at<DWORD *>(DEATH_LIST + 0x30) = g_global_data;
    *at<DWORD *>(DEATH_LIST + 0x34) = g_global_n;
}
void after_at_exit()
{
    if (g_entries_block && *at<DWORD *>(DEATH_LIST + 0x30))
        heap_free(at<void *>(g_entries_block));
    g_entries_block = 0;
    restore_bytes(g_saved_list);
}

// Before the state: the records in the log (a heap block of the side by a tag), the text written and the
// object CreateEntry made (a pool block of the side) by a tag
void tag_state()
{
    BYTE *s = obj(O_SELF);
    if (g_self_class == C_LIST && fl<DWORD>(s, 0x30) == ptrv(g_static_records + 1))
        g_log[g_side].hash = fnv(g_log[g_side].hash, g_static_records + 1, g_static_records[0] * 0x80);
    if (g_entries_block && fl<DWORD>(s, 0x30) == g_entries_block + 4)
        fl<DWORD>(s, 0x30) = 0xe0000004u;
    if (g_self_class == C_GLOBAL)
        g_log[g_side].hash = fnv(g_log[g_side].hash, at<void *>(DEATH_LIST), 0x140);
    if (g_made_entry && fl<DWORD>(obj(O_OUT), 0) == g_made_entry)
        fl<DWORD>(obj(O_OUT), 0) = 0xe0000018u;
    if (g_made_entry)
        g_log[g_side].hash = fnv(g_log[g_side].hash, at<BYTE *>(g_made_entry) + 8, 0x10);
}
void after_side()
{
    BYTE *s = obj(O_SELF);
    if (g_self_class == C_LIST && g_entries_block && fl<DWORD>(s, 0x30) != 0)
        heap_free(at<void *>(g_entries_block));
    g_entries_block = 0;
    if (g_made_entry) {
        pool_free(at<void *>(g_made_entry), 0x18);
        g_made_entry = 0;
    }
    if (g_behaviors_block)
        heap_free(at<void *>(g_behaviors_block));
    g_behaviors_block = 0;
    for (int i = 0; i < g_num_made_ai; i++)
        pool_free(g_made_ai[i], 0x34);
    g_num_made_ai = 0;
    const DWORD b = *reinterpret_cast<DWORD *>(&g_out);
    DWORD h = 0;
    if (b)
        for (const char *p = at<const char *>(b + 8); *p; p++)
            h = h * 31u + (BYTE)*p;
    g_log[g_side].hash = fnv(g_log[g_side].hash, &h, 4);
    at<void(THISCALL *)(void *)>(0x4defd0)(&g_out);
}

struct ClassSlot {
    int cls;
    SlotTest t;
    void (*after)() = nullptr;
    void (*before_call)() = nullptr;
};
const ClassSlot SLOTS[] = {
    {C_DEATH_DEF, {"CDeathBehaviorListDefinition::Apply", 0x478260, 1, 0, 300, a_part}},
    {C_DEATH_DEF, {"CDeathBehaviorListDefinition::LoadOrCreate", 0x4782d0, 2, 0xff, 300, a_load}},
    {C_DEATH_DEF, {"CDeathBehaviorListDefinition::DeletingDtor", 0x477e00, 1, 0xffffffff, 200, a_destroy_behaviors}, after_destroy_behaviors},
    {C_NONE, {"DeathBehaviors_FileOfObject", 0x4780c0, -1, 0xffffffff, 400, a_global_file_of}, nullptr, global_records},
    {C_LIST, {"DeathBehaviorList::Use", 0x478940, 1, 0xffffffff, 400, a_index}},
    {C_LIST, {"DeathBehaviorList::ClearUse", 0x478b30, 0, 0xffffffff, 100, a_none}},
    {C_LIST, {"DeathBehaviorList::Unload", 0x478b60, 1, 0xffffffff, 200, a_index}},
    {C_LIST, {"DeathBehaviorList::FileOfObject", 0x478bb0, 2, 0xffffffff, 300, a_file_of}},
    {C_LIST, {"DeathBehaviorList::CreateEntry", 0x4787d0, 3, 0, 200, a_create}},
    {C_NONE, {"DeathBehaviorList::DeletingDtor", 0x478890, 1, 0xffffffff, 200, a_list_dtor}},
    {C_GLOBAL, {"DeathBehaviors_AtExit", 0x478420, 0, 0, 100, a_at_exit}, after_at_exit, global_list},
    {C_DEATH_MATCH, {"CDeathMatchGameType::ScoreName", 0x479560, 0, 0xffffffff, 10, a_none}},
    {C_DEATH_MATCH, {"CDeathMatchGameType::Update", 0x479570, 1, 0xffffffff, 30, a_value}},
    {C_DEATH_MATCH, {"CDeathMatchGameType::Kill", 0x479580, 2, 0xffffffff, 600, a_kill}},
    {C_DEATH_MATCH, {"CDeathMatchGameType::DeletingDtor", 0x478dc0, 1, 0xffffffff, 30, a_flags0}},
    {C_NONE, {"ClearFlag_6bda40", 0x479900, 0, 0, 30, a_none}},
};

Policy death_policy()
{
    Policy p{};
    p.find_minus_one = 4;
    // used without a test: the slot 37 objects, the world's data, the players (T_OTHER)
    p.nonnull = 1u << T_LAYER | 1u << T_OTHER | 1u << T_DEF;
    return p;
}

void (*g_slot_after)();
void side_done()
{
    if (g_slot_after)
        g_slot_after();
    after_side();
}

void test_slots(Tally &t)
{
    for (const ClassSlot &s : SLOTS) {
        g_self_class = s.cls;
        g_slot_after = s.after;
        g_after_side = side_done;
        g_before_state = tag_state;
        g_before_call = s.before_call;
        const unsigned count = (unsigned)scaled(s.t.count);
        for (unsigned seed = 1; seed <= count; seed++)
            run_slot(t, s.t, seed * 2654435761u + s.t.address, make_self, death_policy());
        g_after_side = nullptr;
        g_before_state = nullptr;
        g_before_call = nullptr;
        g_slot_after = nullptr;
    }
}

// ---- Real data ---------------------------------------------------------------------------------------------

// Init of a fresh list on the game's folder; the registry entry it adds is taken off again; the names must
// equal the global list's (made by the original at startup). Then the list's deleting destructor.
void test_init(Tally &t)
{
    unsigned h[2];
    DWORD r[2], count_delta[2];
    for (int side = 0; side < 2; side++) {
        auto *l = static_cast<ZDefinitionList *>(heap_alloc(0x140));
        memset(l, 0, 0x140);
        at<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, 0, 0x80, 0, nullptr, 0);
        l->aliases = {0x5ed558, nullptr, 0, 0};
        l->registry_index = 0xffffffff;
        l->records.vftable = 0x5f0c0c;
        DWORD &count = *at<DWORD *>(0x6be088 + 0xc);
        DWORD *const *entries = at<DWORD **>(0x6be088 + 8);
        const DWORD before = count;
        if (side == 0)
            ww::hooks_out();
        r[side] = at<DWORD(THISCALL *)(void *)>(0x4784e0)(l);
        if (side == 0)
            ww::hooks_in();
        count_delta[side] = count - before;
        if (count == before + 1 && (*entries)[before * 4 + 1] == ptrv(l))
            count = before;
        unsigned x = fnv(2166136261u, &l->num_entries, 4);
        x = fnv(x, &count_delta[side], 4);
        x = fnv_text(x, l->folder);
        x = fnv_text(x, l->list_name ? l->list_name : "");
        for (DWORD i = 0; i < l->num_entries; i++) {
            x = fnv_text(x, l->entries[i].name);
            x = fnv(x, &l->entries[i].keep, 4);
        }
        const auto *g = at<ZDefinitionList *>(DEATH_LIST);
        bool same = g->num_entries == l->num_entries;
        for (DWORD i = 0; same && i < l->num_entries; i++)
            same = lstrcmpA(g->entries[i].name, l->entries[i].name) == 0;
        x = fnv(x, &same, 1);
        if (side == 0)
            ww::hooks_out();
        at<void *(THISCALL *)(void *, DWORD)>(0x478890)(l, 0);
        if (side == 0)
            ww::hooks_in();
        x = fnv(x, &l->records.vftable, 4);
        x = fnv(x, &l->num_entries, 4);
        h[side] = x;
        heap_free(l);
        if (!same)
            log("  death_match: Init on side %d gave %u entries, the global list has %u", side, l->num_entries, g->num_entries);
    }
    char d[120];
    wsprintfA(d, "[0x4784e0: state %x / %x, registry +%u / +%u]", h[0], h[1], count_delta[0], count_delta[1]);
    t.check(h[0] == h[1] && count_delta[0] == 1 && count_delta[1] == 1, "DeathBehaviorList::Init", d);
    log("  death_match: the global list has %u death behaviours", at<ZDefinitionList *>(DEATH_LIST)->num_entries);
}

// The static initializer 0x4783b0 on the global (its bytes saved and put back), atexit a stand-in
DWORD g_atexit_arg[2];
int g_atexit_side;
int __cdecl fake_atexit(DWORD f)
{
    g_atexit_arg[g_atexit_side] = f;
    return 0x1234;
}
void test_static_init(Tally &t)
{
    SavedBytes list;
    save_bytes(list, DEATH_LIST, 0x140);
    BYTE made[2][0x140];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_atexit_side = side;
        g_atexit_arg[side] = 0;
        memset(at<void *>(DEATH_LIST), 0xa5, 0x140);
        CodePatch p;
        if (side == 0)
            ww::hooks_out();
        patch_jump(p, 0x5d0863, (void *)&fake_atexit);
        r[side] = at<DWORD(__cdecl *)()>(0x4783b0)();
        unpatch(p);
        if (side == 0)
            ww::hooks_in();
        memcpy(made[side], at<void *>(DEATH_LIST), 0x140);
        restore_bytes(list);
    }
    char d[120];
    wsprintfA(d, "[0x4783b0: eax %x / %x, atexit %x / %x]", r[0], r[1], g_atexit_arg[0], g_atexit_arg[1]);
    t.check(r[0] == r[1] && g_atexit_arg[0] == g_atexit_arg[1] && g_atexit_arg[0] == 0x478420 && !memcmp(made[0], made[1], 0x140),
            "DeathBehaviors_StaticInit", d);
}

void test_registered(Tally &registered)
{
    for (const invcheck::ClassDef &c : CLASSES) {
        const CClassInfo *ci = *at<CClassInfo **>(c.var);
        char d[160];
        wsprintfA(d, "[%s: class info %x, size %x, create %x]", c.name, ptrv(ci), ci ? ci->size : 0, ci ? ptrv((void *)ci->create) : 0);
        registered.check(ci && ci->size == c.size && ptrv((void *)ci->create) == c.create && ptrv((void *)ci->create_array) == c.create_array &&
                             at<Getter>(c.getters[0])() == ci,
                         c.name, d);
    }
    registered.check(at<Getter>(0x478840)() == *at<CClassInfo **>(0x6bd9f4), "DeathBehaviorList::ElementClass", "");
}

}  // namespace

ZAX_CHECKS(death_match)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally getters{"death_match: class getters"}, objects{"death_match: Create, CreateArray, destructors"}, fields{"death_match: RegisterFields"},
        real_fields{"death_match: RegisterFields against the registered fields (real data)"}, slots{"death_match: slots in the fake world"},
        real{"death_match: list Init, static initializer, registered class infos (real data)"};
    invcheck::g_hook_table = hooks_death_match;
    CodePatch add;
    patch_jump(add, 0x419ea0, (void *)&fake_static_add);
    if (invcheck::arena_setup()) {
        invcheck::test_classes(objects, getters, fields, real_fields, CLASSES, NCLASSES);
        invcheck::test_getter(getters, CLASSES[C_DEATH_DEF], 0x478840);
    } else {
        objects.check(false, "death_match", "[no arena]");
    }
    unpatch(add);
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    make_static_texts();
    if (world_on()) {
        test_slots(slots);
        world_off();
    } else {
        slots.check(false, "death_match", "[the fake world could not be set up]");
    }
    slots.report();
    test_init(real);
    test_static_init(real);
    test_registered(real);
    real.report();
    log("  death_match: %u ms", GetTickCount() - start);
}

}  // namespace zax
