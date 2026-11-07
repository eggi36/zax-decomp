// Checks of ai_turrets.cpp (and the definition list slots of def_list_slots.h hooked by ai_tasks.cpp)
// against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): class getters, Create, CreateArray, deleting destructors,
//    RegisterFields and the registered descriptors (as checks_ai_tasks.cpp). CSurfaceType's RegisterFields
//    (it only registers 0x588310 with a static list) with that list's Add replaced by a stand-in.
//  - The slots in the fake world (checks_task_world.h): the map actions (their map names from a table;
//    the world, the application, the other map list and the directory list are fakes and stand-ins, so
//    no map of the running game is flushed or expired), the temporary task AI and its action, the
//    tenacious investigate AI and the turrets; the counters; the definition list slots on a fake list
//    (its vtable a recorder table, its records a heap block of each side, Object_CreateFromFile a
//    stand-in).
//  - Real data: the surface types' file lookup 0x588310 for every loaded entry of the real list (and
//    objects not in it); the item category list's static getter 0x58fe20 with its guard set and clear
//    (the list's bytes saved and put back, atexit a stand-in); the registered class infos; the AIs of
//    these classes on the running level's parts as copies through their slots, at most 12 per class.
#include "checks_task_world.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_ai_turrets(Hook *out);

namespace {

using namespace tw;
using Getter = CClassInfo *(__cdecl *)();

const invcheck::ClassDef CLASSES[] = {
    {"CSurfaceType", 8, 0x7139ac, 0x5881e0, 0x588210, 0, {0x588190, 0x588540}, true},
    {"CFlushMapAction", 0xc, 0x7139c0, 0x58a0f0, 0x58a120, 0x58a260, {0x58a0a0, 0x58a200}, true},
    {"CExpireMapAction", 0xc, 0x7139c4, 0x58a490, 0x58a4c0, 0x58a600, {0x58a440, 0x58a5a0}, true},
    {"CExpireMapsInFolderAction", 0xc, 0x7139c8, 0x58a8a0, 0x58a8d0, 0x58aa10, {0x58a850, 0x58a9b0}, true},
    {"CTemporaryTaskAI", 0x1c, 0x7139d0, 0x58af50, 0x58af90, 0x58b100, {0x58af00, 0x58b0a0}, true},
    {"CAssignTemporaryTaskAction", 0x10, 0x7139d4, 0x58b4a0, 0x58b4e0, 0x58b650, {0x58b450, 0x58b5f0}, true},
    {"CTenaciousInvestigateAI", 0xd4, 0x7139dc, 0x58c220, 0x58c270, 0x58c420, {0x58c1c0, 0x58c3b0}, true},
    {"CTurretAI", 0x30, 0x713a2c, 0x58d890, 0x58d900, 0x58daa0, {0x58d840, 0x58da40}, true},
    {"CSentinonAI", 0x30, 0x713a30, 0x58e790, 0x58e810, 0, {0x58e6e0, 0x58e980}, true},
    {"CAutoGunAI", 0x64, 0x713a34, 0x58eb60, 0x58ebf0, 0x58ee40, {0x58eab0, 0x58ed80}, true},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];
enum { C_SURFACE, C_FLUSH, C_EXPIRE, C_EXPIRE_FOLDER, C_TEMPORARY, C_ASSIGN, C_TENACIOUS, C_TURRET, C_SENTINON, C_AUTOGUN, C_LIST };

// ---- The classes ----------------------------------------------------------------------------------------

void test_create_array_in_turn(Tally &t, const invcheck::ClassDef &c, int count, BYTE pattern)
{
    using namespace invcheck;
    DWORD e[2], destroyed[2], used[2];
    bool arenas_same = false;
    for (int side = 0; side < 2; side++) {
        invcheck::side_begin(side);
        arena_on(side, pattern);
        void *a = at<CreateN>(c.create_array)(count, 0, 0);
        e[side] = rel(invcheck::ptrv(a), side);
        used[side] = g_used[side];
        if (side == 0)
            memcpy(g_copy, g_arena[0], used[0]);
        else
            arenas_same = used[0] == used[1] && same_region(g_copy, g_arena[1], used[1]);
        destroyed[side] = count > 0 && a ? rel(invcheck::ptrv(vcall<void *>(a, 1, 2)), side) : 0;
        arena_off();
        invcheck::side_end(side);
    }
    char d[200];
    wsprintfA(d, "[CreateArray %d: %x / %x, %u / %u bytes, destroyed %x / %x, first difference +%x: %x / %x]", count, e[0], e[1], used[0],
              used[1], destroyed[0], destroyed[1], g_diff_at, g_diff_a, g_diff_b);
    t.check(arenas_same && e[0] == e[1] && destroyed[0] == destroyed[1], c.name, d);
}

// CSurfaceType's RegisterFields 0x588300: the static list's Add (0x419ea0, fastcall) a stand-in
DWORD g_static_added[2];
int g_static_side;
DWORD __fastcall fake_static_add(DWORD item)
{
    g_static_added[g_static_side] = item;
    return 0x5a5a0000u + item;
}
void test_surface_register(Tally &t)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_static_side = side;
        g_static_added[side] = 0;
        invcheck::side_begin(side);
        CodePatch p;
        patch_jump(p, 0x419ea0, (void *)&fake_static_add);
        r[side] = at<DWORD(__cdecl *)()>(0x588300)();
        unpatch(p);
        invcheck::side_end(side);
    }
    char d[120];
    wsprintfA(d, "[0x588300: added %x / %x, eax %x / %x]", g_static_added[0], g_static_added[1], r[0], r[1]);
    t.check(r[0] == r[1] && g_static_added[0] == g_static_added[1] && g_static_added[0] == 0x588310, "CSurfaceType", d);
}

void test_classes(Tally &getters, Tally &objects, Tally &fields, Tally &real_fields)
{
    invcheck::g_hook_table = hooks_ai_turrets;
    if (!invcheck::arena_setup()) {
        objects.check(false, "ai_turrets", "[no arena]");
        return;
    }
    for (int c = 0; c < NCLASSES; c++)
        invcheck::warm_up(CLASSES[c]);
    for (int c = 0; c < NCLASSES; c++) {
        const invcheck::ClassDef &d = CLASSES[c];
        for (DWORD g : d.getters)
            if (g)
                invcheck::test_getter(getters, d, g);
        if (d.create && d.destroy) {
            for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++)
                invcheck::test_create(objects, d, seed * 7 + (unsigned)c);
            for (int n = 0; n < 4; n++)
                test_create_array_in_turn(objects, d, n, (BYTE)(0x31 * n + c));
        }
        if (d.register_fields)
            for (unsigned seed = 1; seed <= 2; seed++)
                invcheck::test_register_fields(fields, real_fields, d, seed * 13 + (unsigned)c);
    }
    test_surface_register(fields);
}

// ---- The slots ---------------------------------------------------------------------------------------------

BYTE *g_template[NCLASSES];
int g_self_class;
const BYTE *g_real_self;
const BYTE *g_real_part;
BYTE *g_parts[8192];
ZString g_out;                 // the ZString the list slots write (outside the fake block)
DWORD g_entries_block;         // the records a fake list got (freed after the side unless the slot did)
bool g_task_made;              // CAssignTemporaryTaskAction made a CTemporaryTaskAI from the pool
BYTE *g_task_block;

// The map names of the map actions (a C string field; the actions copy it)
const char *const MAP_NAMES[] = {"", "Levels/Act 1/M1", "ACT 2\\M9\\09 Assimilate Cave1", "Multiplayer", "Multiplayer/", "A\\B//",
                                 "Eigene/Level 1"};

DWORD fake_ai(Gen &g, int null_one_in = 4)
{
    if (null_one_in && g.next() % (DWORD)null_one_in == 0)
        return 0;
    return ptrv(obj(O_AI + (int)(g.next() % N_AI)));
}
DWORD time_value(Gen &g, float lo, float hi)
{
    const DWORD v = g.next() % 16;
    const DWORD sp[] = {0, 0x80000000u, 0x7fc00000u, 0x7f800000u, 0xbf800000u};
    return v < 5 ? sp[v] : g.fbits_(lo, hi);
}
DWORD seconds(Gen &g)
{
    const DWORD v = g.next() % 16;
    if (v == 0)
        return 0;
    if (v == 1)
        return 0x7fc00000u;
    return g.fbits_(0.0f, 0.5f);
}
void fake_vtable(BYTE *s, DWORD off, int type) { fl<DWORD>(s, off) = ptrv(g_fvt[type]); }

// A fake definition list in the object under test: a recorder vtable (T_CATALOGUE), 0-3 records of
// 0x80 bytes on the heap (with the count in front) holding fake objects or 0, a monitor object
void make_list(Gen &g, BYTE *s)
{
    memset(s, 0, 0x140);
    fl<DWORD>(s, 0) = ptrv(g_fvt[T_CATALOGUE]);
    fl<DWORD>(s, 0x1c) = 0x5ed558;   // the aliases: an empty pointer array
    const DWORD n = g.next() % 4;
    g_entries_block = 0;
    if (n || g.next() % 2) {
        DWORD *block = static_cast<DWORD *>(heap_alloc((n + 1) * 0x80 + 4));
        block[0] = n + 1;
        BYTE *data = reinterpret_cast<BYTE *>(block + 1);
        for (DWORD i = 0; i < (n + 1) * 0x80; i++)
            data[i] = (BYTE)g.next();
        for (DWORD i = 0; i <= n; i++) {
            BYTE *rec = data + i * 0x80;
            const char *name = i % 2 ? "Rock" : "Generic";
            for (DWORD k = 0; k < 8; k++)
                rec[k] = name[k < 7 ? k : 7];
            fl<DWORD>(rec, 0x78) = g.next() % 3 ? ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))) : 0;
        }
        fl<DWORD>(s, 0x30) = ptrv(data);
        g_entries_block = ptrv(block);
    }
    fl<DWORD>(s, 0x34) = n;
    fl<DWORD>(s, 0x13c) = g.next() % 3 ? ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))) : 0;
    at<void *(THISCALL *)(void *)>(0x4dec90)(&g_out);
}

void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    const int c = g_self_class;
    if (c == C_LIST) {
        make_list(g, s);
        return;
    }
    memcpy(s, g_real_self ? g_real_self : g_template[c], CLASSES[c].size);
    fl<DWORD>(s, 4) = 0;
    if (c != C_FLUSH && c != C_EXPIRE && c != C_EXPIRE_FOLDER && c != C_ASSIGN && c != C_SURFACE)
        fl<DWORD>(s, 0xc) = 0;
    if (g_real_part)
        for (int i = 0; i < N_ENTITY; i++) {
            BYTE *e = obj(O_ENTITY + i);
            memcpy(e, g_real_part, OBJ);
            fl<DWORD>(e, 0) = ptrv(g_fvt[T_ENTITY]);
            fl<DWORD>(e, 0x48) = ptrv(obj(O_LAYER));
            fl<DWORD>(e, 0x9c) = g.next() % 4;
            fl<DWORD>(e, 0x40) = g.next() % 3;
        }
    const bool r = g_real_self != nullptr;
    switch (c) {
    case C_FLUSH:
    case C_EXPIRE:
    case C_EXPIRE_FOLDER: {
        const DWORD k = g.next() % 9;
        fl<DWORD>(s, 8) = k < 7 ? ptrv(MAP_NAMES[k]) : k == 7 ? 0 : ptrv(MAP_NAMES[1]);
        break;
    }
    case C_TEMPORARY:
        if (!r) {
            fl<DWORD>(s, 0x10) = fake_ai(g, 0);
            fl<DWORD>(s, 0x14) = fake_ai(g, 3);
            fl<BYTE>(s, 0x18) = (BYTE)(g.next() % 2);
        } else {
            fl<DWORD>(s, 0x10) = fake_ai(g, 0);
            if (fl<DWORD>(s, 0x14))
                fl<DWORD>(s, 0x14) = fake_ai(g, 0);
        }
        break;
    case C_ASSIGN:
        if (!r || fl<DWORD>(s, 0xc))
            fl<DWORD>(s, 0xc) = fake_ai(g, r ? 0 : 4);
        break;
    case C_TENACIOUS:
        fake_vtable(s, 0x14, T_BEHAVIOR);
        if (!r) {
            fl<DWORD>(s, 0x10) = time_value(g, 0.0f, 3.0f);
            fl<DWORD>(s, 0xbc) = g.next();
            fl<DWORD>(s, 0xd0) = time_value(g, -1.0f, 2.0f);
        }
        break;
    case C_TURRET:
    case C_SENTINON:
    case C_AUTOGUN:
        if (c == C_AUTOGUN)
            fake_vtable(s, 0x30, T_AI);   // the embedded CPickupAI
        if (!r) {
            fl<DWORD>(s, 0x10) = g.next() % 8 ? g.next() % 700 : g.next();
            fl<DWORD>(s, 0x14) = g.next() % 100;
            fl<DWORD>(s, 0x18) = time_value(g, -1.0f, 1.0f);
            fl<DWORD>(s, 0x1c) = g.next() % 6;
            fl<DWORD>(s, 0x20) = g.next();
            fl<DWORD>(s, 0x24) = time_value(g, 0.0f, 3.0f);
            fl<DWORD>(s, 0x28) = time_value(g, 0.0f, 360.0f);
            fl<BYTE>(s, 0x2c) = (BYTE)(g.next() % 2);
            if (c == C_AUTOGUN)
                fl<DWORD>(s, 0x60) = time_value(g, -1.0f, 3.0f);
        } else {
            fl<DWORD>(s, 0x18) = g.next() % 2 ? 0xbf800000u : fl<DWORD>(s, 0x18);   // the retry time over now and then
        }
        break;
    default:
        break;
    }
}

DWORD entity(Gen &g) { return ptrv(any_entity(g)); }
void a_part(Gen &g, DWORD *a) { a[0] = entity(g); }
void a_part_or_null(Gen &g, DWORD *a) { a[0] = ptrv(any_entity(g, 5)); }
void a_part_seconds(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = seconds(g);
}
void a_two(Gen &g, DWORD *a)
{
    a[0] = g.next() % 2 ? entity(g) : g.next();
    a[1] = g.next();
}
void a_collide(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = ptrv(any_entity(g, 4));
    a[2] = g.next();
    a[3] = g.next();
    a[4] = g.next() % 4 ? g.next() & 0xff : 0;
}
void a_touch(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g, 6));
    a[1] = ptrv(any_entity(g, 4));
    a[2] = g.next();
    if (a[1] && g.next() % 2)
        fl<DWORD>(at<BYTE *>(a[1]), 0x1c) |= *at<DWORD *>(0x6c29a0) & (1u << (g.next() % 32));
}
DWORD message(Gen &g)
{
    BYTE *m = obj(O_OUT);
    const DWORD ids[] = {0x6c29e8, 0x6c29e0, 0x6c29d8, 0x6c29d4};
    fl<DWORD>(m, 0) = 0x5eeb64;
    const DWORD k = g.next() % 5;
    fl<DWORD>(m, 4) = k < 4 ? *at<DWORD *>(ids[k]) : g.next();
    fl<DWORD>(m, 8) = g.next() & 0xff;
    return ptrv(m);
}
void a_part_message(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = message(g);
}
void a_execute(Gen &g, DWORD *a)
{
    a[0] = ptrv(obj(O_LAYER));
    a[1] = entity(g);
    a[2] = ptrv(obj(O_AI));
    a[3] = entity(g);
}
void a_temporary_ctor(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = fake_ai(g, 0);
    a[2] = g.next() % 3 ? g.next() & 1 : g.next();
}
void a_at_target(Gen &g, DWORD *a)
{
    a[0] = entity(g);
    a[1] = ptrv(obj(O_AI));
    a[2] = entity(g);
    a[3] = ptrv(any_entity(g, 4));
    g_task_made = a[3] && fl<DWORD>(obj(O_SELF), 0xc);
    if (g_task_made) {
        g_task_block = static_cast<BYTE *>(pool_alloc(0x1c));   // the block the action will get
        pool_free(g_task_block, 0x1c);
    }
}
void after_at_target()
{
    if (g_task_made)
        pool_free(g_task_block, 0x1c);
    g_task_made = false;
}
void a_counter(Gen &g, DWORD *a)
{
    fl<DWORD>(obj(O_SELF), 0) = ptrv(g_fvt[T_ENTITY]);
    a[0] = g.next() % 4 ? g.next() % 10 : g.next();
}
// The list slots' arguments
// The accessors' eax holds the records' address: the records move to a static block (the same address on
// both sides; the slots never free them)
DWORD g_static_records[1 + 5 * 0x20];
void static_records(Gen &g, bool needed)
{
    BYTE *s = obj(O_SELF);
    BYTE *data = reinterpret_cast<BYTE *>(g_static_records + 1);
    if (fl<DWORD>(s, 0x30)) {
        const DWORD n = *at<DWORD *>(g_entries_block);
        g_static_records[0] = n;
        memcpy(data, at<void *>(g_entries_block + 4), n * 0x80);
        heap_free(at<void *>(g_entries_block));
        g_entries_block = 0;
        fl<DWORD>(s, 0x30) = ptrv(data);
    } else if (needed) {   // slots 8 and 9 read the records without a test
        g_static_records[0] = 1;
        memset(data, (BYTE)g.next(), 0x80);
        fl<DWORD>(s, 0x30) = ptrv(data);
    }
}
void a_index(Gen &g, DWORD *a)
{
    static_records(g, false);
    a[0] = g.next() % (fl<DWORD>(obj(O_SELF), 0x34) + 1);
}
void a_index_records(Gen &g, DWORD *a)
{
    static_records(g, true);
    a[0] = g.next() % (fl<DWORD>(obj(O_SELF), 0x34) + 1);
}
void a_index_keep(Gen &g, DWORD *a)
{
    static_records(g, false);
    a[0] = g.next() % (fl<DWORD>(obj(O_SELF), 0x34) + 1);
    a[1] = g.next();
}
void a_path(Gen &g, DWORD *a)
{
    a[0] = g.next() % 4;
    a[1] = ptrv(&g_out);
}
void a_file_of(Gen &g, DWORD *a)
{
    BYTE *s = obj(O_SELF);
    const DWORD n = fl<DWORD>(s, 0x34);
    const DWORD k = g.next() % 3;
    a[0] = k == 0 && n && fl<DWORD>(s, 0x30) ? fl<DWORD>(at<BYTE *>(fl<DWORD>(s, 0x30) + 0x80 * (g.next() % n)), 0x78)
                                             : ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER)));
    a[1] = ptrv(&g_out);
}
void a_create(Gen &g, DWORD *a)
{
    const char *const dirs[] = {"Surfaces", "", "Game Types/MP"};
    a[0] = ptrv(dirs[g.next() % 3]);
    a[1] = ptrv(g.next() % 2 ? "Rock" : "Generic");
    a[2] = g.next() % 4 ? ptrv(obj(O_OUT)) : 0;
}
void a_flags0(Gen &, DWORD *a) { a[0] = 0; }
// Before the state of a list slot is taken: the records (each side's own heap block) by a tag in the
// list and their bytes into the log
void tag_records()
{
    BYTE *s = obj(O_SELF);
    if (fl<DWORD>(s, 0x30) == ptrv(g_static_records + 1)) {
        g_log[g_side].hash = fnv(g_log[g_side].hash, g_static_records + 1, g_static_records[0] * 0x80);
    } else if (g_entries_block && fl<DWORD>(s, 0x30) == g_entries_block + 4) {
        const DWORD n = *at<DWORD *>(g_entries_block);
        g_log[g_side].hash = fnv(g_log[g_side].hash, at<void *>(g_entries_block + 4), n * 0x80);
        fl<DWORD>(s, 0x30) = 0xe0000004u;
    }
}
// After a list slot: the records the slot did not free, and the text it wrote (into the log)
void after_list()
{
    BYTE *s = obj(O_SELF);
    if (fl<DWORD>(s, 0x30) && fl<DWORD>(s, 0x30) != ptrv(g_static_records + 1) && g_entries_block)
        heap_free(at<void *>(g_entries_block));
    g_entries_block = 0;
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
    void (*after)();
    bool animated = false;   // IsKindOf affirms CEntityAnimated (the slot reads the entity without a test)
};
const ClassSlot SLOTS[] = {
    {C_FLUSH, {"CFlushMapAction::Execute", 0x58a2f0, 4, 0xff, 200, a_execute}, nullptr},
    {C_EXPIRE, {"CExpireMapAction::Execute", 0x58a690, 4, 0xff, 200, a_execute}, nullptr},
    {C_EXPIRE_FOLDER, {"CExpireMapsInFolderAction::Execute", 0x58aaa0, 4, 0xff, 200, a_execute}, nullptr},
    {C_TEMPORARY, {"CTemporaryTaskAI::Collide", 0x58b350, 5, 0, 60, a_collide}, nullptr},
    {C_TEMPORARY, {"CTemporaryTaskAI::Message", 0x58b380, 2, 0xff, 200, a_part_message}, nullptr},
    {C_TEMPORARY, {"CTemporaryTaskAI::Begin", 0x58b400, 1, 0, 40, a_part}, nullptr},
    {C_TEMPORARY, {"CTemporaryTaskAI::Update", 0x58b410, 2, 0, 40, a_part_seconds}, nullptr},
    {C_TEMPORARY, {"CTemporaryTaskAI::Draw", 0x58b430, 2, 0, 40, a_two}, nullptr},
    {C_TEMPORARY, {"CTemporaryTaskAI::CtorAssign", 0x58b2a0, 3, 0xffffffff, 200, a_temporary_ctor}, nullptr},
    {C_ASSIGN, {"CAssignTemporaryTaskAction::AtTarget", 0x58b770, 4, 0xff, 120, a_at_target}, after_at_target},
    {C_TENACIOUS, {"CTenaciousInvestigateAI::Begin", 0x58c5d0, 1, 0, 80, a_part_or_null}, nullptr},
    {C_TENACIOUS, {"CTenaciousInvestigateAI::Update", 0x58c620, 2, 0, 200, a_part_seconds}, nullptr, true},
    {C_TURRET, {"CTurretAI::Begin", 0x58de70, 1, 0, 80, a_part_or_null}, nullptr},
    {C_TURRET, {"CTurretAI::Update", 0x58deb0, 2, 0, 600, a_part_seconds}, nullptr},
    {C_TURRET, {"CTurretAI::MayTouch", 0x58e340, 3, 0xff, 80, a_touch}, nullptr},
    {C_SENTINON, {"CSentinonAI::MayTouch", 0x58ea50, 3, 0xff, 120, a_touch}, nullptr},
    {C_AUTOGUN, {"CAutoGunAI::Update", 0x58eff0, 2, 0, 300, a_part_seconds}, nullptr},
    {C_AUTOGUN, {"CAutoGunAI::MayTouch", 0x58f010, 3, 0xff, 120, a_touch}, nullptr},
    {C_AUTOGUN, {"CAutoGunAI::Slot33", 0x58f060, 3, 0xff, 120, a_touch}, nullptr},
    {C_AUTOGUN, {"CAutoGunAI::Collide", 0x58f0b0, 5, 0, 120, a_collide}, nullptr},
    {C_TURRET, {"UsedOreCount_Get", 0x58fd80, 0, 0xffffffff, 30, a_counter}, nullptr},
    {C_TURRET, {"UsedOreCount_Add", 0x58fda0, -1, 0xffffffff, 30, a_counter}, nullptr},
    {C_TURRET, {"UsedCrystalsCount_Get", 0x58fdd0, 0, 0xffffffff, 30, a_counter}, nullptr},
    {C_TURRET, {"UsedCrystalsCount_Add", 0x58fdf0, -1, 0xffffffff, 30, a_counter}, nullptr},
    {C_LIST, {"SurfaceTypeList::DeletingDtor", 0x588590, 1, 0xffffffff, 120, a_flags0}, after_list},
    {C_LIST, {"GameTypeList::DeletingDtor", 0x4ce7a0, 1, 0xffffffff, 60, a_flags0}, after_list},
    {C_LIST, {"ItemCategoryList::DeletingDtor", 0x58ff70, 1, 0xffffffff, 60, a_flags0}, after_list},
    {C_LIST, {"SurfaceTypeList::CreateFromFile", 0x5884a0, 3, 0xffffffff, 120, a_create}, after_list},
    {C_LIST, {"SurfaceTypeList::CreateFromFileBase", 0x588640, 3, 0xffffffff, 80, a_create}, after_list},
    {C_LIST, {"GameTypeList::CreateFromFile", 0x4cec90, 3, 0xffffffff, 80, a_create}, after_list},
    {C_LIST, {"SurfaceTypeList::FileOfObject", 0x588740, 2, 0xffffffff, 160, a_file_of}, after_list},
    {C_LIST, {"GameTypeList::FileOfObject", 0x4ced10, 2, 0xffffffff, 120, a_file_of}, after_list},
    // eax of the path slots: what the ZString destructor left (a pool address)
    {C_LIST, {"SurfaceTypeList::EntryPath", 0x5887e0, 2, 0, 80, a_path}, after_list},
    {C_LIST, {"GameTypeList::EntryPath", 0x4cedc0, 2, 0, 80, a_path}, after_list},
    {C_LIST, {"DefinitionList::InUse", 0x5886c0, 1, 0xffffffff, 80, a_index}, after_list},
    {C_LIST, {"DefinitionList::SetKeep", 0x5886e0, 2, 0xffffffff, 80, a_index_keep}, after_list},
    {C_LIST, {"DefinitionList::Loaded", 0x588700, 1, 0xffffffff, 80, a_index_records}, after_list},
    {C_LIST, {"DefinitionList::Loading", 0x588720, 1, 0xffffffff, 80, a_index_records}, after_list},
};

Policy turret_policy(bool animated)
{
    Policy p{};
    p.find_minus_one = 4;
    // the made behaviours and the AI taken from a part with AIs (CLayerPart_TakeAI) are used without a test
    p.nonnull = 1u << T_BEHAVIOR | 1u << T_AI;
    if (animated)
        p.always[0] = at<Getter>(0x48dcc0)();
    return p;
}

void run_slots(Tally &t, const ClassSlot &s, unsigned count, unsigned base)
{
    g_self_class = s.cls;
    g_after_side = s.after;
    g_before_state = s.cls == C_LIST ? tag_records : nullptr;
    for (unsigned seed = 1; seed <= count; seed++)
        run_slot(t, s.t, seed * 2654435761u + base, make_self, turret_policy(s.animated));
    g_after_side = nullptr;
    g_before_state = nullptr;
}

void test_slots(Tally &t)
{
    for (const ClassSlot &s : SLOTS) {
        g_real_self = nullptr;
        run_slots(t, s, (unsigned)scaled(s.t.count), s.t.address);
    }
}

// ---- Real data ---------------------------------------------------------------------------------------------

// The surface types' file lookup 0x588310 on the real list (its loaded entries' objects and others)
void test_surface_files(Tally &t)
{
    const BYTE *l = at<BYTE *>(0x713868);
    const DWORD n = fl<DWORD>(const_cast<BYTE *>(l), 0x34);
    const BYTE *data = at<BYTE *>(fl<DWORD>(const_cast<BYTE *>(l), 0x30));
    int cases = 0;
    for (DWORD i = 0; i <= n && i < 64; i++) {
        void *o = nullptr;
        if (i < n && data && readable(data + 0x80 * i, 0x80))
            o = at<void *>(fl<DWORD>(const_cast<BYTE *>(data + 0x80 * i), 0x78));
        if (i < n && !o)
            continue;   // not loaded: the lookup would load it on one side only
        if (i == n)
            o = g_template[C_SURFACE];   // a surface type that is not in the list
        DWORD r[2];
        unsigned h[2];
        for (int side = 0; side < 2; side++) {
            ZString out;
            at<void *(THISCALL *)(void *)>(0x4dec90)(&out);
            if (side == 0)
                tw::hooks_out();
            r[side] = at<DWORD(__fastcall *)(void *, ZString *)>(0x588310)(o, &out);
            if (side == 0)
                tw::hooks_in();
            h[side] = 0;
            for (const char *p = out.c_str(); *p; p++)
                h[side] = h[side] * 31u + (BYTE)*p;
            at<void(THISCALL *)(void *)>(0x4defd0)(&out);
        }
        char d[120];
        wsprintfA(d, "[0x588310 entry %u: eax %x / %x, path %x / %x]", i, r[0], r[1], h[0], h[1]);
        t.check(r[0] == r[1] && h[0] == h[1], "CSurfaceType", d);
        cases++;
    }
    log("  ai_turrets: %u surface types in the list, %d file lookups", n, cases);
}

// The item category list's getter 0x58fe20: the guard set (the list returned) and clear (the list made
// again); the list and the guard put back after each side, atexit a stand-in
DWORD g_atexit_arg[2];
int g_atexit_side;
int __cdecl fake_atexit(DWORD f)
{
    g_atexit_arg[g_atexit_side] = f;
    return 0;
}
void test_category_list(Tally &t)
{
    for (int guard = 0; guard < 2; guard++) {
        SavedBytes list, bits;
        save_bytes(list, 0x713a58, 0x140);
        save_bytes(bits, 0x713a50, 1);
        BYTE made[2][0x140];
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            g_atexit_side = side;
            g_atexit_arg[side] = 0;
            *at<BYTE *>(0x713a50) = guard ? (BYTE)(bits.bytes[0] | 1) : (BYTE)(bits.bytes[0] & ~1);
            CodePatch p;
            patch_jump(p, 0x5d0863, (void *)&fake_atexit);
            if (side == 0)
                tw::hooks_out();
            r[side] = at<DWORD(__cdecl *)()>(0x58fe20)();
            if (side == 0)
                tw::hooks_in();
            unpatch(p);
            memcpy(made[side], at<void *>(0x713a58), 0x140);
            made[side][0x13f] ^= *at<BYTE *>(0x713a50);   // the guard in the comparison
            restore_bytes(list);
            restore_bytes(bits);
        }
        char d[120];
        wsprintfA(d, "[0x58fe20 guard %d: eax %x / %x, atexit %x / %x]", guard, r[0], r[1], g_atexit_arg[0], g_atexit_arg[1]);
        t.check(r[0] == r[1] && g_atexit_arg[0] == g_atexit_arg[1] && !memcmp(made[0], made[1], 0x140), "ItemCategoryList", d);
    }
}

// 0x58ade0 on heap arrays of each side: the fields and eax compared (eax: operator delete's or 0)
void test_heap_array_clear(Tally &t)
{
    for (unsigned seed = 1; seed <= (unsigned)scaled(20); seed++) {
        DWORD a[2][4], r[2];
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            a[side][0] = random();
            a[side][1] = seed % 3 ? ptrv(heap_alloc(4 + seed % 13)) : 0;
            a[side][2] = random();
            a[side][3] = random();
            const DWORD had = a[side][1];
            if (side == 0)
                tw::hooks_out();
            r[side] = at<DWORD(THISCALL *)(void *)>(0x58ade0)(a[side]);
            if (side == 0)
                tw::hooks_in();
            (void)had;
        }
        char d[120];
        wsprintfA(d, "[0x58ade0 seed %u: eax %x / %x]", seed, r[0], r[1]);
        t.check(!memcmp(a[0], a[1], sizeof a[0]) && ((r[0] == 0) == (r[1] == 0)), "HeapArray_Clear", d);
    }
}

void test_real(Tally &registered, Tally &real_slots)
{
    for (const invcheck::ClassDef &c : CLASSES) {
        const CClassInfo *ci = *at<CClassInfo **>(c.var);
        char d[160];
        wsprintfA(d, "[%s: class info %x, size %x, create %x]", c.name, ptrv(ci), ci ? ci->size : 0, ci ? ptrv((void *)ci->create) : 0);
        registered.check(ci && ci->size == c.size && ptrv((void *)ci->create) == c.create && ptrv((void *)ci->create_array) == c.create_array &&
                             at<Getter>(c.getters[0])() == ci,
                         c.name, d);
    }
    BYTE **parts = g_parts;
    const int n = level_parts(parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0) {
        log("  ai_turrets real data: no level grid found");
        return;
    }
    int found[NCLASSES] = {}, ran = 0;
    for (int i = 0; i < n; i++) {
        const BYTE *part = parts[i];
        BYTE *ais[64];
        const int count = part_ais(part, parts, n, ais, 64);
        for (int k = 0; k < count; k++) {
            const CClassInfo *ci = class_of(ais[k]);
            for (int c = 0; c < NCLASSES; c++) {
                if (!ci || ci != *at<CClassInfo **>(CLASSES[c].var) || !readable(ais[k], CLASSES[c].size))
                    continue;
                if (found[c]++ >= 12)
                    continue;
                g_real_self = ais[k];
                g_real_part = readable(part, OBJ) ? part : nullptr;
                for (const ClassSlot &s : SLOTS)
                    if (s.cls == c && s.t.address != 0x58b2a0 && s.t.nargs >= 0) {
                        g_self_class = c;
                        g_after_side = s.after;
                        for (unsigned seed = 1; seed <= 2; seed++) {
                            run_slot(real_slots, s.t, seed * 40503u + ptrv(ais[k]), make_self, turret_policy(s.animated));
                            ran++;
                        }
                        g_after_side = nullptr;
                    }
                g_real_self = nullptr;
                g_real_part = nullptr;
            }
        }
    }
    log("  ai_turrets real data: %d parts, AIs %d CTemporaryTaskAI, %d CTenaciousInvestigateAI, %d CTurretAI, %d CSentinonAI, %d "
        "CAutoGunAI; %d cases",
        n, found[C_TEMPORARY], found[C_TENACIOUS], found[C_TURRET], found[C_SENTINON], found[C_AUTOGUN], ran);
}

}  // namespace

ZAX_CHECKS(ai_turrets)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally getters{"ai_turrets: class getters"}, objects{"ai_turrets: Create, CreateArray, destructors"},
        fields{"ai_turrets: RegisterFields"}, real_fields{"ai_turrets: RegisterFields against the registered fields (real data)"},
        slots{"ai_turrets: slots in the fake world"}, lists{"ai_turrets: surface type files, item category list, heap array (real data)"},
        registered{"ai_turrets: registered class infos (real data)"}, real_slots{"ai_turrets: the level's AIs through their slots (real data)"};
    SavedBytes saved_random, saved_random3;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random3, 0x712cc0, GAME_RANDOM_BYTES);
    test_classes(getters, objects, fields, real_fields);
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    for (int c = 0; c < NCLASSES; c++)
        g_template[c] = static_cast<BYTE *>(at<void *(__cdecl *)()>(CLASSES[c].create)());
    test_surface_files(lists);
    test_category_list(lists);
    test_heap_array_clear(lists);
    lists.report();
    make_static_texts();
    if (world_on()) {
        test_slots(slots);
        test_real(registered, real_slots);
        world_off();
    } else {
        slots.check(false, "ai_turrets", "[the fake world could not be set up]");
    }
    for (int c = 0; c < NCLASSES; c++)
        if (g_template[c])
            vcall<void *>(g_template[c], 1, 1);
    slots.report();
    registered.report();
    real_slots.report();
    restore_bytes(saved_random3);
    restore_bytes(saved_random);
    log("  ai_turrets: %u ms", GetTickCount() - start);
}

}  // namespace zax
