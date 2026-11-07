// Checks of CSayQuip and the quip list (say_quip.cpp) against the original, in the game thread.
//
//  - The class (checks_inventory_common.h): the class getters (both copies, and CQuipCategory's two
//    copies here), Create on a pool block filled with a pattern, CreateArray with 0-3 elements, the
//    deleting destructor, RegisterFields into a fake class info and against the registered descriptors.
//  - The list's constructor 0x55afe0 on a scratch block and its getter and exit function on the global
//    list (saved and restored; atexit and the record list's constructor and Set stand-ins of the fake
//    world).
//  - In the fake world (checks_seq_world.h): CSayQuip's slot 23 with the global list a fake (records of
//    loaded, unloaded and loading categories, conditions, the packet writer a recorder); the list's slots
//    on a fake list: play (the voice manager, its play and the sound list recorders), its copy in slot 16,
//    loading the entries in use, the timers, the file of an object, the file of an entry (the text of the
//    ZString it writes compared), making an entry from its file with and without its sounds
//    (Object_CreateFromFile and ReadSounds recorders), the verb, the deleting destructor (operator
//    delete a recorder for the records block).
//  - Real data: the game's quip list: the verb, the file of every loaded category, the file of every
//    entry (texts compared).
#include "checks_inventory_common.h"
#include "checks_seq_world.h"

namespace zax {

int hooks_say_quip(Hook *out);

namespace {

const invcheck::ClassDef CLASS{"CSayQuip", 0x10, 0x7124b4, 0x55aca0, 0x55acd0, 0x55ae10, {0x55ac50, 0x55adb0}, true};
const invcheck::ClassDef QUIP_CATEGORY{"CQuipCategory", 0x20, 0x7124b8, 0, 0, 0, {0x55b240, 0x55bf00}, false};

using namespace sqw;

constexpr DWORD QUIP_LIST = 0x712370;
const char *zs_text(const void *z) { return at<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }

// CSayQuip: the category index (sometimes past the end), the probability
void make_say_quip(Gen &g)
{
    BYTE *s = obj(O_SELF);
    fl<DWORD>(s, 0) = 0x5fe6dc;
    fl<DWORD>(s, 4) = 0;
    fl<DWORD>(s, 8) = g.next() % 6;
    fl<DWORD>(s, 0xc) = g.next() % 5 == 0 ? (g.next() % 2 ? 0 : bits(100.0f)) : g.fbits_(0.0f, 100.0f);
    // the records are there and the chosen one is not loading (the original reads the category without
    // a test)
    BYTE *ql = case_global(QUIP_LIST);
    fl<DWORD>(ql, 0x30) = ptrv(obj(O_RECORDS));
    DWORD i = fl<DWORD>(s, 8);
    if (i >= fl<DWORD>(ql, 0x34))
        i = 0;
    obj(O_RECORDS)[i * 0x80 + 0x7f] = 0;
}
// A fake quip list as the object under test: the fake list vtable, records (none loading), count, folder,
// a monitor
void make_list(Gen &g)
{
    BYTE *s = obj(O_SELF);
    fl<DWORD>(s, 0) = ptrv(g_fvt[T_LIST]);
    for (DWORD off = 4; off < 0x2c; off += 4)
        fl<DWORD>(s, off) = g.next() % 2 ? 0 : g.next();
    fl<DWORD>(s, 0x30) = ptrv(obj(O_RECORDS));
    fl<DWORD>(s, 0x34) = 1 + g.next() % 8;
    lstrcpyA(reinterpret_cast<char *>(s + 0x38), g.next() % 2 ? "Quips" : "Data/Quips");
    for (int r = 0; r < 8; r++)
        obj(O_RECORDS)[r * 0x80 + 0x7f] = 0;
}
// The list with its records block in an extra object (the count in front) for the destructor
void make_list_block(Gen &g)
{
    make_list(g);
    BYTE *s = obj(O_SELF);
    const DWORD n = 1 + g.next() % 3;
    fl<DWORD>(extra(0), 0) = n;
    for (DWORD r = 0; r < n; r++) {
        BYTE *rec = extra(0) + 4 + r * 0x80;
        fl<DWORD>(rec, 0x78) = g.next() % 3 ? ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))) : 0;
    }
    fl<DWORD>(s, 0x30) = g.next() % 5 ? ptrv(extra(0) + 4) : 0;
    fl<DWORD>(s, 0x34) = n;
    fl<DWORD>(s, 0x13c) = g.next() % 2 ? ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))) : 0;
}
// The ZString the entry path writes: a real one in an extra object, its text logged and freed before the
// state is taken
void make_list_out(Gen &g)
{
    make_list(g);
    at<void *(THISCALL *)(void *)>(0x4dec90)(extra(5));
}
void log_out_text()
{
    log_value(fnv_text(2166136261u, zs_text(extra(5))));
    at<void(THISCALL *)(void *)>(0x4defd0)(extra(5));
    fl<DWORD>(extra(5), 0) = 0;
}

void a_execute(Gen &g, DWORD *a)
{
    a[0] = ptrv(obj(O_LAYER));
    a[1] = ptrv(any_entity(g, 4));
    a[2] = g.next() % 2 ? 0 : g.next();
    a[3] = ptrv(any_entity(g, 4));
}
void a_index(Gen &g, DWORD *a) { a[0] = g.next() % 10; }
void a_seconds(Gen &g, DWORD *a) { a[0] = g.fbits_(0.0f, 2.0f); }
void a_none(Gen &, DWORD *) {}
void a_file_of(Gen &g, DWORD *a)
{
    const DWORD k = g.next() % 3;
    a[0] = k == 0 ? fl<DWORD>(obj(O_RECORDS) + (g.next() % 8) * 0x80, 0x78) : ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER)));
    a[1] = ptrv(extra(5));
}
void a_entry_path(Gen &g, DWORD *a)
{
    a[0] = g.next() % 8;
    a[1] = ptrv(extra(5));
}
void a_create(Gen &g, DWORD *a)
{
    static const char *const dirs[] = {"", "Quips", "Data/Quips/Sub"};
    static const char *const names[] = {"Hurt", "Taunt", ""};
    a[0] = ptrv(dirs[g.next() % 3]);
    a[1] = ptrv(names[g.next() % 3]);
    a[2] = ptrv(extra(1));
}
void a_flags0(Gen &, DWORD *a) { a[0] = 0; }

struct WorldTest {
    SlotTest t;
    SelfMaker make;
    void (*before_state)();
    bool deletes;   // operator delete recorded instead of freeing
};
const WorldTest TESTS[] = {
    {{"CSayQuip::Execute", 0x55b050, 4, 0xffffffff, 1500, a_execute}, make_say_quip, nullptr, false},
    {{"QuipList::Play", 0x55bb40, 1, 0, 600, a_index}, make_list, nullptr, false},
    {{"QuipList::Play2", 0x55b040, 1, 0xffffffff, 200, a_index}, make_list, nullptr, false},
    {{"QuipList::LoadUsed", 0x55bc60, 1, 0, 600, a_index}, make_list, nullptr, false},
    {{"QuipList::Update", 0x55bd50, 1, 0, 1200, a_seconds}, make_list, nullptr, false},
    {{"QuipList::FileOfObject", 0x55bfd0, 2, 0xffffffff, 600, a_file_of}, make_list_out, log_out_text, false},
    {{"QuipList::EntryPath", 0x55c080, 2, 0, 400, a_entry_path}, make_list_out, log_out_text, false},
    {{"QuipList::CreateEntry", 0x55ba90, 3, 0xffffffff, 400, a_create}, make_list, nullptr, false},
    {{"QuipList::CreateFromFileBase", 0x55bf50, 3, 0xffffffff, 300, a_create}, make_list, nullptr, false},
    {{"QuipList::Verb", 0x55b030, 0, 0xffffffff, 10, a_none}, make_list, nullptr, false},
    {{"QuipList::DeletingDtor", 0x55b890, 1, 0xffffffff, 400, a_flags0}, make_list_block, nullptr, true},
};

void test_world(Tally &t)
{
    for (const WorldTest &w : TESTS) {
        Policy p{};
        p.nonnull = 1u << T_OTHER;   // the categories the list makes (slot 37) are read without a test
        if (w.deletes)
            p.extra[0] = 0x5d0320;
        g_before_state = w.before_state;
        for (unsigned seed = 1; seed <= (unsigned)scaled(w.t.count); seed++)
            run_slot(t, w.t, seed * 2654435761u + w.t.address, w.make, p);
        g_before_state = nullptr;
    }
}

// The list's constructor on a scratch block; the getter and the exit function on the global list (saved
// and restored) with the fake world's stand-ins for atexit and the record list
void test_list_statics(Tally &t)
{
    alignas(16) static BYTE scratch[2][0x140];
    for (int c = 0; c < scaled(20); c++) {
        DWORD r[2];
        const BYTE pattern = (BYTE)(0x31 + c * 7);
        for (int side = 0; side < 2; side++) {
            memset(scratch[side], pattern, sizeof scratch[side]);
            if (side == 0)
                hooks_out();
            r[side] = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x55afe0)(scratch[side], c & 1, 0x80, c % 3, 0, c % 2) -
                      ptrv(scratch[side]);
            if (side == 0)
                hooks_in();
        }
        t.check(r[0] == r[1] && memcmp(scratch[0], scratch[1], sizeof scratch[0]) == 0, "QuipList_Ctor", "");
    }
    static SavedBytes list, guard;
    save_bytes(list, QUIP_LIST, 0x140);
    save_bytes(guard, 0x7124b1, 1);
    for (int c = 0; c < 4; c++) {
        BYTE after[2][0x140];
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            g_vseed = (unsigned)c;
            g_vindex = 0;
            restore_bytes(list);
            *at<BYTE *>(0x7124b1) = (BYTE)(c & 1 ? 0xfe : 0);   // the bit clear, the other bits kept
            if (side == 0)
                hooks_out();
            if (c < 2) {
                r[side] = at<DWORD(__cdecl *)()>(0x55b940)() == QUIP_LIST ? 1 : 2;
            } else {
                // the exit function on a list without records or monitor
                *at<DWORD *>(QUIP_LIST + 0x30) = 0;
                *at<DWORD *>(QUIP_LIST + 0x13c) = 0;
                at<void(__cdecl *)()>(0x55b9d0)();
                r[side] = 3;
            }
            if (side == 0)
                hooks_in();
            memcpy(after[side], at<void *>(QUIP_LIST), 0x140);
            r[side] = r[side] * 0x100 + *at<BYTE *>(0x7124b1);
        }
        restore_bytes(list);
        restore_bytes(guard);
        t.check(r[0] == r[1] && memcmp(after[0], after[1], 0x140) == 0, "QuipList statics", "");
    }
}

// The game's quip list: the verb, the file of every loaded category and of every entry
void test_real(Tally &t)
{
    BYTE *l = at<BYTE *>(QUIP_LIST);
    if (!(*at<BYTE *>(0x7124b1) & 1) || !fl<DWORD>(l, 0x30)) {
        log("  say_quip real data: the quip list is empty");
        return;
    }
    const DWORD n = fl<DWORD>(l, 0x34);
    int loaded = 0;
    for (DWORD i = 0; i < n && i < 200; i++) {
        BYTE *rec = at<BYTE *>(fl<DWORD>(l, 0x30)) + i * 0x80;
        unsigned h[2];
        DWORD r[2];
        void *o = at<void *>(fl<DWORD>(rec, 0x78));
        for (int side = 0; side < 2; side++) {
            ZString out;
            at<void *(THISCALL *)(void *)>(0x4dec90)(&out);
            if (side == 0)
                hooks_out();
            r[side] = at<DWORD(THISCALL *)(void *, DWORD, void *)>(0x55c080)(l, i, &out) & 0;
            if (o)
                r[side] += at<DWORD(THISCALL *)(void *, void *, void *)>(0x55bfd0)(l, o, &out) & 0xff;
            r[side] = r[side] * 0x100 + (at<DWORD(THISCALL *)(void *)>(0x55b030)(l) == 0x61fb70);
            if (side == 0)
                hooks_in();
            h[side] = fnv_text(2166136261u, zs_text(&out));
            at<void(THISCALL *)(void *)>(0x4defd0)(&out);
        }
        if (o)
            loaded++;
        t.check(r[0] == r[1] && h[0] == h[1], "real QuipList", reinterpret_cast<const char *>(rec));
    }
    log("  say_quip real data: %u entries, %d loaded", n, loaded);
}

}  // namespace

ZAX_CHECKS(say_quip)
{
    (void)cls;
    (void)n;
    Tally getters{"say_quip: class getters"}, objects{"say_quip: Create, CreateArray, destructor"}, fields{"say_quip: RegisterFields"},
        real_fields{"say_quip: registered fields and class (real data)"}, statics{"say_quip: the list's constructor, getter, exit"},
        slots{"say_quip: slot 23 and the list's slots (fake world)"}, real{"say_quip: the game's quip list (real data)"};
    SavedBytes saved_random, saved_random3;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random3, 0x712cc0, GAME_RANDOM_BYTES);
    invcheck::g_hook_table = hooks_say_quip;
    if (invcheck::arena_setup()) {
        invcheck::test_classes(objects, getters, fields, real_fields, &CLASS, 1);
        for (DWORD g : QUIP_CATEGORY.getters)
            invcheck::test_getter(getters, QUIP_CATEGORY, g);
    } else {
        objects.check(false, "say_quip", "[no arena]");
    }
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    if ((g_modules = M_QUIP, world_on())) {
        test_list_statics(statics);
        test_world(slots);
        world_off();
    } else {
        slots.check(false, "say_quip", "[the fake world could not be set up]");
    }
    statics.report();
    slots.report();
    test_real(real);
    real.report();
    restore_bytes(saved_random3);
    restore_bytes(saved_random);
}

}  // namespace zax
