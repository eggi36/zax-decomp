// Checks of the non-interactive sequences and the small action classes (sequences.cpp) against the
// original, in the game thread.
//
//  - The classes (checks_inventory_common.h): Create on a pool block filled with a pattern, CreateArray
//    with 0-3 elements, the deleting destructors, the class getters (both copies; the class variable set
//    and 0), RegisterFields into a fake class info and the registered descriptors, the registered class
//    infos: CNonInteractiveSequenceScene, CBeginNonInteractiveSequenceAction,
//    CEndCurrentNonInteractiveSequenceSceneAction, CSetMusicAction, CSwitchMusicTrackAction,
//    CAnimationTrigger, CThresholdTrigger.
//  - The scene list (side 0 with the module's hooks out, both sides on their own heap objects, compared
//    by content): appending to lists of 0-5 scenes at and below their capacity (the growth by 1.2), the
//    scene and CObject assignments, the scene constructor, the list's deleting destructor.
//  - The state logic in the fake world (checks_seq_world.h): Start, BeginScene, SceneArrived, EndScene,
//    CameraArrived on sequences in every state with 0-4 scenes (targets found or not, the player as the
//    target, the trackers there or not (for CameraArrived always there: in state 1 the original reads the
//    target's tracker without a null check), speeds 0, negative, NaN), the slots 23 of the begin and end
//    actions, of CSetMusicAction (the world's mixer or none) and CSwitchMusicTrackAction; the conversion
//    of the old properties (slot 18) with the dynamic properties, the byte value and the parent's class
//    recorders, operator new giving an extra object and operator delete recording, the
//    CMultipleActionsActions made freed after each side.
#include "checks_seq_common.h"
#include "checks_inventory_common.h"
#include "checks_seq_world.h"

namespace zax {

int hooks_sequences(Hook *out);

namespace {

// ---- The classes -------------------------------------------------------------------------------------------

const invcheck::ClassDef CLASSES[] = {
    {"CNonInteractiveSequenceScene", 0x14, 0x6d0124, 0x521060, 0x5210a0, 0x521210, {0x521010, 0x5211b0}, true},
    {"CBeginNonInteractiveSequenceAction", 0x28, 0x6d0128, 0x521560, 0x5215b0, 0x521780, {0x521510, 0x521720}, true},
    {"CEndCurrentNonInteractiveSequenceSceneAction", 8, 0x6d012c, 0x522760, 0x522790, 0x5228d0, {0x522710, 0x522870}, true, true},
    {"CSetMusicAction", 0x20, 0x6d0050, 0x51d920, 0x51d990, 0x51db40, {0x51d8d0, 0x51dae0}, true},
    {"CSwitchMusicTrackAction", 0xc, 0x6d0054, 0x51dcd0, 0x51dd00, 0x51de40, {0x51dc80, 0x51dde0}, true},
    {"CAnimationTrigger", 0x10, 0x713138, 0x55e5c0, 0x55e600, 0x55e6f0, {0x55e570, 0}, true},
    {"CThresholdTrigger", 0x18, 0x713814, 0x583ca0, 0x583cd0, 0x583e10, {0x583c50, 0x583db0}, true},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];

// ---- The scene list ------------------------------------------------------------------------------------------

seqc::ModuleHooks g_hooks{"sequences", hooks_sequences, {}, {}, {}, 0};
unsigned g_rs = 777;
DWORD rnd()
{
    g_rs = g_rs * 1664525u + 1013904223u;
    DWORD x = g_rs ^ (g_rs >> 15);
    x *= 0x2c1b3c6du;
    return x ^ (x >> 12);
}
const char *const SCENE_NAMES[] = {"", "Camera Spot", "Zax", "A rather long name of a part that the camera goes to in the scene"};
void zs_assign_text(void *z, const char *s) { at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, s); }
const char *zs_text(const void *z) { return at<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }

// The content of a list: count, capacity (whether it grew), every scene's fields and name
unsigned list_hash(BYTE *l)
{
    unsigned h = fnv(fnv(2166136261u, l, 4), l + 8, 8);
    BYTE *d = at<BYTE *>(seqc::fl<DWORD>(l, 4));
    if (!d)
        return fnv(h, "0", 1);
    for (DWORD i = 0; i < seqc::fl<DWORD>(l, 8); i++) {
        BYTE *s = d + i * 0x14;
        h = fnv(h, s, 8);
        h = fnv(h, s + 0xc, 8);
        h = fnv_text(h, zs_text(s + 8));
    }
    return h;
}

void test_scene_list(Tally &t)
{
    using seqc::fl;
    for (int c = 0; c < scaled(300); c++) {
        const DWORD count = rnd() % 6, cap = count + (rnd() % 3 ? 0 : rnd() % 3);
        int names[8];
        DWORD speeds[8];
        for (int i = 0; i < 8; i++) {
            names[i] = (int)(rnd() % 4);
            speeds[i] = rnd();
        }
        unsigned h[2][3];
        DWORD r[2][4];
        for (int side = 0; side < 2; side++) {
            alignas(4) BYTE list[0x10];
            fl<DWORD>(list, 0) = 0x5fb658;
            BYTE *data = cap ? static_cast<BYTE *>(at<void *(__fastcall *)(int, int, int)>(0x5210a0)((int)cap, 0, 0)) : nullptr;
            for (DWORD i = 0; i < count; i++) {
                zs_assign_text(data + i * 0x14 + 8, SCENE_NAMES[names[i]]);
                fl<DWORD>(data + i * 0x14, 0x10) = speeds[i];
            }
            fl<DWORD>(list, 4) = seqc::ptrv(data);
            fl<DWORD>(list, 8) = count;
            fl<DWORD>(list, 0xc) = cap;
            // the scene to append, constructed by the side's 0x5226e0, assigned by 0x522b00
            alignas(4) BYTE scene[0x14], copy[0x14];
            if (side == 0)
                g_hooks.out();
            r[side][0] = at<DWORD(THISCALL *)(void *)>(0x5226e0)(scene) - seqc::ptrv(scene);
            zs_assign_text(scene + 8, SCENE_NAMES[names[7]]);
            fl<DWORD>(scene, 0x10) = speeds[7];
            at<DWORD(THISCALL *)(void *)>(0x5226e0)(copy);
            r[side][1] = at<DWORD(THISCALL *)(void *, void *)>(0x522b00)(copy, scene) - seqc::ptrv(copy);
            r[side][2] = at<DWORD(THISCALL *)(void *, void *)>(0x522b30)(copy, scene) - seqc::ptrv(copy);
            h[side][0] = fnv_text(fnv(fnv(2166136261u, copy, 8), copy + 0xc, 8), zs_text(copy + 8));
            r[side][3] = at<DWORD(THISCALL *)(void *, void *)>(0x522960)(list, scene);
            h[side][1] = list_hash(list);
            at<DWORD(THISCALL *)(void *, DWORD)>(0x522ab0)(list, 0);
            h[side][2] = fnv(2166136261u, list, 0x10);
            vcall<void *>(scene, 1, (DWORD)0);
            vcall<void *>(copy, 1, (DWORD)0);
            if (side == 0)
                g_hooks.in();
        }
        char d[120];
        wsprintfA(d, "[scene list %d: %u scenes, capacity %u: old count %u / %u, %x / %x]", c, count, cap, r[0][3], r[1][3], h[0][1], h[1][1]);
        t.check(memcmp(h[0], h[1], sizeof h[0]) == 0 && memcmp(r[0], r[1], sizeof r[0]) == 0, "SceneList", d);
    }
}

// ---- The state logic in the fake world -----------------------------------------------------------------------

using namespace sqw;

DWORD speed(Gen &g)
{
    const DWORD v = g.next() % 8;
    if (v == 0)
        return 0;
    if (v == 1)
        return bits(-30.0f);
    if (v == 2)
        return 0x7fc00000u;
    return g.fbits_(1.0f, 400.0f);
}
// The sequence under test: state 0-4, 0-4 scenes (in an extra object) with names (empty or not), actions
// and speeds, an index, the return action and speed
void make_begin(Gen &g)
{
    BYTE *s = obj(O_SELF);
    fl<DWORD>(s, 0) = 0x5fb5e8;
    fl<DWORD>(s, 4) = 0;
    fl<DWORD>(s, 8) = g.next() % 5;
    fl<DWORD>(s, 0xc) = 0x5fb658;
    fl<DWORD>(s, 0x10) = ptrv(extra(0));
    fl<DWORD>(s, 0x14) = g.next() % 5;
    fl<DWORD>(s, 0x18) = 4;
    fl<DWORD>(s, 0x1c) = fl<DWORD>(s, 0x14) ? g.next() % fl<DWORD>(s, 0x14) : 0;
    fl<DWORD>(s, 0x20) = g.next() % 3 ? ptrv(obj(O_ACTION + (int)(g.next() % N_ACTION))) : 0;
    fl<DWORD>(s, 0x24) = speed(g);
    for (int i = 0; i < 4; i++) {
        BYTE *sc = extra(0) + i * 0x14;
        fl<DWORD>(sc, 0) = 0x5fb58c;
        fl<DWORD>(sc, 4) = 0;
        fl<DWORD>(sc, 8) = ptrv(g_texts[g.next() % 4 == 0 ? 0 : 1 + g.next() % 3].buffer);
        fl<DWORD>(sc, 0xc) = g.next() % 3 ? ptrv(obj(O_ACTION + (int)(g.next() % N_ACTION))) : 0;
        fl<DWORD>(sc, 0x10) = speed(g);
    }
}
// For the legacy conversion: the scenes a new[] array (the count in front) with counted names, the
// capacity at or above the count, no array for capacity 0
void make_begin_legacy(Gen &g)
{
    make_begin(g);
    BYTE *s = obj(O_SELF);
    const DWORD n = fl<DWORD>(s, 0x14), cap = n + (g.next() % 3 ? 0 : g.next() % 3);
    fl<DWORD>(s, 0x18) = cap;
    fl<DWORD>(extra(0), 0) = cap;
    fl<DWORD>(s, 0x10) = cap ? ptrv(extra(0) + 4) : 0;   // no array without capacity (as in the game)
    for (DWORD i = 0; i < cap && i < 8; i++) {
        BYTE *sc = extra(0) + 4 + i * 0x14;
        fl<DWORD>(sc, 0) = 0x5fb58c;
        fl<DWORD>(sc, 4) = 0;
        at<void *(THISCALL *)(void *, const void *)>(0x4dedf0)(sc + 8, &g_texts[g.next() % NTEXTS]);   // ZString's copy constructor
        fl<DWORD>(sc, 0xc) = g.next() % 3 ? ptrv(obj(O_ACTION + (int)(g.next() % N_ACTION))) : 0;
        fl<DWORD>(sc, 0x10) = speed(g);
    }
}
void free_made()
{
    for (int i = 0; i < g_nmade; i++)
        vcall<void *>(at<void *>(g_made[i]), 1, (DWORD)1);
    g_nmade = 0;
}
void a_parent(Gen &g, DWORD *a) { a[0] = g.next() % 5 ? ptrv(obj(O_ACTION + (int)(g.next() % N_ACTION))) : 0; }
void make_set_music(Gen &g)
{
    BYTE *s = obj(O_SELF);
    fl<DWORD>(s, 0) = 0x5fb340;
    fl<DWORD>(s, 4) = 0;
    fl<DWORD>(s, 8) = 0x5fb224;
    fl<DWORD>(s, 0xc) = 0;
    fl<DWORD>(s, 0x10) = g.fbits_(0.0f, 10.0f);
    for (DWORD off = 0x14; off < 0x20; off += 4)
        fl<DWORD>(s, off) = g.next() % 4;
    BYTE *m = obj(O_MIXER);
    for (DWORD off = 0x18; off < 0x24; off += 4)
        fl<DWORD>(m, off) = g.next() % 4;
    fl<BYTE>(m, 0x2c) = (BYTE)(g.next() % 2);
}
void make_switch(Gen &g)
{
    BYTE *s = obj(O_SELF);
    fl<DWORD>(s, 0) = 0x5fb3b0;
    fl<DWORD>(s, 4) = 0;
    fl<DWORD>(s, 8) = g.next() % 4;
}

void a_layer(Gen &, DWORD *a) { a[0] = ptrv(obj(O_LAYER)); }
void a_arrived(Gen &g, DWORD *a)
{
    a[0] = g.next();
    a[1] = ptrv(obj(O_LAYER));
}
void a_execute(Gen &g, DWORD *a)
{
    a[0] = ptrv(obj(O_LAYER));
    a[1] = ptrv(any_entity(g, 4));
    a[2] = g.next() % 2 ? 0 : g.next();
    a[3] = ptrv(any_entity(g, 4));
}

struct WorldTest {
    SlotTest t;
    SelfMaker make;
    DWORD nonnull;   // Policy::nonnull
};
const WorldTest TESTS[] = {
    {{"CBeginNonInteractiveSequenceAction::Start", 0x521e90, 1, 0xff, 800, a_layer}, make_begin, 0},
    {{"CBeginNonInteractiveSequenceAction::BeginScene", 0x521ef0, 1, 0, 1200, a_layer}, make_begin, 0},
    {{"CBeginNonInteractiveSequenceAction::SceneArrived", 0x522060, 1, 0, 800, a_layer}, make_begin, 0},
    {{"CBeginNonInteractiveSequenceAction::EndScene", 0x522110, 1, 0, 1500, a_layer}, make_begin, 0},
    {{"CBeginNonInteractiveSequenceAction::CameraArrived", 0x5222a0, 2, 0, 1200, a_arrived}, make_begin, 1u << T_AI},
    {{"CBeginNonInteractiveSequenceAction::Execute", 0x521e70, 4, 0xffffffff, 200, a_execute}, make_begin, 0},
    {{"CEndCurrentNonInteractiveSequenceSceneAction::Execute", 0x522940, 4, 0xffffffff, 200, a_execute}, make_begin, 0},
    {{"CSetMusicAction::Execute", 0x51dbd0, 4, 0xffffffff, 400, a_execute}, make_set_music, 0},
    {{"CSwitchMusicTrackAction::Execute", 0x51ded0, 4, 0xffffffff, 200, a_execute}, make_switch, 0},
};

void test_world(Tally &t)
{
    if (!(g_modules = M_SEQUENCES, world_on())) {
        t.check(false, "sequences", "[the fake world could not be set up]");
        return;
    }
    for (const WorldTest &w : TESTS) {
        Policy p{};
        p.nonnull = w.nonnull;
        for (unsigned seed = 1; seed <= (unsigned)scaled(w.t.count); seed++)
            run_slot(t, w.t, seed * 2654435761u + w.t.address, w.make, p);
    }
    // the legacy conversion: operator new gives an extra object, operator delete records; the
    // CMultipleActionsActions made freed after each side
    const SlotTest legacy{"CBeginNonInteractiveSequenceAction::ConvertLegacy", 0x522380, 1, 0, 1500, a_parent};
    Policy q{};
    q.extra[0] = 0x5d033a;
    q.extra[1] = 0x5d0320;
    g_nmade = 0;
    g_after_side = free_made;
    for (unsigned seed = 1; seed <= (unsigned)scaled(legacy.count); seed++)
        run_slot(t, legacy, seed * 2654435761u + legacy.address, make_begin_legacy, q);
    g_after_side = nullptr;
    world_off();
}

}  // namespace

ZAX_CHECKS(sequences)
{
    (void)cls;
    (void)n;
    Tally getters{"sequences: class getters"}, objects{"sequences: Create, CreateArray, destructors"}, fields{"sequences: RegisterFields"},
        real_fields{"sequences: registered fields and classes (real data)"}, list{"sequences: the scene list"},
        world{"sequences: the state logic and the actions (fake world)"};
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    invcheck::g_hook_table = hooks_sequences;
    if (invcheck::arena_setup())
        invcheck::test_classes(objects, getters, fields, real_fields, CLASSES, NCLASSES);
    else
        objects.check(false, "sequences", "[no arena]");
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    test_scene_list(list);
    list.report();
    test_world(world);
    world.report();
    restore_bytes(saved_random);
}

}  // namespace zax
