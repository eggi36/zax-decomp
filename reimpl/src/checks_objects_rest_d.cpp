// Checks of objects_rest_d.cpp against the original, in the game thread.
//
//  - The class (checks_inventory_common.h): CCharacterListHolder's getter, Create (pool block filled with a
//    pattern), CreateArray with 0-3 elements, the deleting destructor of both, RegisterFields into a fake class
//    info and the registered descriptors and class info (real data).
//  - The list functions and the layer block in the fake world (checks_restd_world.h): the holder's message
//    slot ("Delete" in all spellings and other names, from listed and other parts), the entry's address, set,
//    remove, append and index, the deleting destructor with every flag on single holders and arrays of one to
//    three holders (the listener calls, frees and destructors are recorders), the reader's registration and
//    the layer's block write (with and without the parts' rectangles).
//  - Real data: the application's player list (+0x284): every entry's index and address, a part not in it.
#include "checks_restd_world.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_objects_rest_d(Hook *out);

namespace {

using namespace rdw;

const invcheck::ClassDef CLASSES[] = {
    {"CCharacterListHolder", 0x18, 0x6c28ec, 0x4ef440, 0x4ef490, 0x4ef720, {0x4ef6c0, 0}, true},
};

// ---- The holder in the fake world -------------------------------------------------------------------------

const char *const MESSAGES[] = {"Delete", "delete", "DELETE", "Deleted", "Hit", ""};
bool g_fake_array;   // the list's array has the fake vtable (append records slot 2)
int g_holders;       // 0: a single holder at obj(O_SELF); 1-3: an array of that many behind its count

// A holder at h: its list in the 0x100 bytes at data (entries: fake entities, now and then null)
void fill_holder(Gen &g, BYTE *h, BYTE *data)
{
    fl<DWORD>(h, 0) = 0x5f8ef0;
    fl<DWORD>(h, 4) = 0;
    fl<DWORD>(h, 8) = g_fake_array ? ptrv(g_fvt[T_OTHER]) : 0x5f8f4c;
    const DWORD n = g.next() % 6 == 0 ? 0 : 1 + g.next() % 12;
    for (DWORD i = 0; i < 16; i++)
        fl<DWORD>(data, 4 * i) = g.next() % 8 == 0 ? 0 : ptrv(obj(O_ENTITY + (int)(g.next() % N_ENTITY)));
    fl<DWORD>(h, 0xc) = n || g.next() % 2 ? ptrv(data) : 0;
    fl<DWORD>(h, 0x10) = fl<DWORD>(h, 0xc) ? n : 0;
    fl<DWORD>(h, 0x14) = fl<DWORD>(h, 0xc) ? 16 : 0;
}
void make_holder(Gen &g)
{
    if (!g_holders) {
        fill_holder(g, obj(O_SELF), obj(O_SPARE));
        return;
    }
    BYTE *block = obj(O_SPARE + 1);
    fl<DWORD>(block, 0) = (DWORD)g_holders;
    g_self = block + 4;
    for (int i = 0; i < g_holders; i++)
        fill_holder(g, g_self + 0x18 * (DWORD)i, obj(O_SPARE + 2 + i));
}
// A part: one of the list (when there is one) or any entity, now and then null
DWORD a_part(Gen &g)
{
    const DWORD n = fl<DWORD>(g_self, 0x10), data = fl<DWORD>(g_self, 0xc);
    const DWORD v = g.next() % 8;
    if (v == 0)
        return 0;
    if (v < 5 && n && data)
        return *at<DWORD *>(data + 4 * (g.next() % n));
    return ptrv(obj(O_ENTITY + (int)(g.next() % N_ENTITY)));
}
DWORD a_index(Gen &g)
{
    const DWORD n = fl<DWORD>(g_self, 0x10);
    return g.next() % 6 == 0 ? n + g.next() % 3 : (n ? g.next() % n : 0);
}
void a_message(Gen &g, DWORD *a)
{
    a[0] = a_part(g);
    a[1] = ptrv(MESSAGES[g.next() % 6]);
}
void a_slot(Gen &g, DWORD *a) { a[0] = fl<DWORD>(g_self, 0x10) ? g.next() % fl<DWORD>(g_self, 0x10) : 0; }
void a_set(Gen &g, DWORD *a)
{
    // Set writes its entry without a bounds test (its callers pass a valid index): a list with data
    if (!fl<DWORD>(g_self, 0xc)) {
        fl<DWORD>(g_self, 0xc) = ptrv(obj(O_SPARE));
        fl<DWORD>(g_self, 0x14) = 16;
    }
    a[0] = fl<DWORD>(g_self, 0x10) ? g.next() % fl<DWORD>(g_self, 0x10) : 0;
    a[1] = a_part(g);
}
void a_remove(Gen &g, DWORD *a) { a[0] = a_index(g); }
void a_part1(Gen &g, DWORD *a) { a[0] = a_part(g); }
void a_flags(Gen &g, DWORD *a) { a[0] = g_holders ? 2 + g.next() % 2 : g.next() % 2; }
void a_block(Gen &g, DWORD *a)
{
    BYTE *r = obj(O_POINT);
    for (DWORD k = 0; k < 4; k++)
        fl<DWORD>(r, 4 * k) = g.next() % 4 ? g.next() % 5000 : g.next();
    a[0] = ptrv(obj(O_PACKET));
    a[1] = g.next();
    a[2] = ptrv(r);
}
void make_registry(Gen &) { g_self = obj(O_OTHER); }
void make_layer(Gen &g)
{
    fl<DWORD>(obj(O_SELF), 0) = ptrv(g_fvt[T_LAYER]);
    (void)g;
}

struct Test {
    SlotTest t;
    SelfMaker make;
    bool fake_array;
    int holders;
};
const Test TESTS[] = {
    {{"CCharacterListHolder::OnMessage", 0x4ef840, 2, 0xffffffff, 600, a_message}, make_holder, false, 0},
    {{"CCharacterListHolder::Slot", 0x4ef8e0, 1, 0xffffffff, 100, a_slot}, make_holder, false, 0},
    {{"CCharacterListHolder::Set", 0x4ef900, 2, 0xffffffff, 400, a_set}, make_holder, false, 0},
    {{"CCharacterListHolder::RemoveAt", 0x4ef950, 1, 0, 400, a_remove}, make_holder, false, 0},
    {{"CCharacterListHolder::Append", 0x4ef9b0, 1, 0xffffffff, 300, a_part1}, make_holder, true, 0},
    {{"CCharacterListHolder::IndexOf", 0x4ef9e0, 1, 0xffffffff, 300, a_part1}, make_holder, false, 0},
    {{"CCharacterListHolder::DeletingDtor (single)", 0x4ef510, 1, 0xffffffff, 300, a_flags}, make_holder, false, 0},
    {{"CCharacterListHolder::DeletingDtor (1 in an array)", 0x4ef510, 1, 0xffffffff, 100, a_flags}, make_holder, false, 1},
    {{"CCharacterListHolder::DeletingDtor (3 in an array)", 0x4ef510, 1, 0xffffffff, 100, a_flags}, make_holder, false, 3},
    {{"LayerBlock_Register", 0x4ee750, 0, 0xffffffff, 20, nullptr}, make_registry, false, 0},
    {{"LayerBlock_Write", 0x4ee760, 3, 0xffffffff, 400, a_block}, make_layer, false, 0},
};

void test_slots(Tally &t)
{
    for (const Test &x : TESTS) {
        g_fake_array = x.fake_array;
        g_holders = x.holders;
        Policy p{};
        const unsigned n = (unsigned)scaled(x.t.count);
        for (unsigned seed = 1; seed <= n; seed++)
            run_slot(t, x.t, seed * 2654435761u + x.t.address + (unsigned)x.holders, x.make, p);
    }
    g_holders = 0;
    g_fake_array = false;
}

// ---- Real data: the application's player list ----------------------------------------------------------

void test_real(Tally &t)
{
    BYTE *app = *at<BYTE **>(0x713f14);
    if (!app || !readable(app + 0x284, 0x18)) {
        log("  objects_rest_d: no application");
        return;
    }
    BYTE *h = app + 0x284;
    const DWORD n = fl<DWORD>(h, 0x10);
    log("  objects_rest_d: the application's player list has %u entries", n);
    for (DWORD i = 0; i <= n && i < 64; i++) {
        void *part = i < n ? fl<void **>(h, 0xc)[i] : static_cast<void *>(obj(O_SELF));
        DWORD r[2][2];
        for (int side = 0; side < 2; side++) {
            invcheck::side_begin(side);
            r[side][0] = at<DWORD(THISCALL *)(void *, void *)>(0x4ef9e0)(h, part);
            r[side][1] = i < n ? ptrv(at<void **(THISCALL *)(void *, DWORD)>(0x4ef8e0)(h, i)) : 0;
            invcheck::side_end(side);
        }
        char d[120];
        wsprintfA(d, "[entry %u: index %x / %x, slot %x / %x]", i, r[0][0], r[1][0], r[0][1], r[1][1]);
        t.check(r[0][0] == r[1][0] && r[0][1] == r[1][1], "player list", d);
    }
}

}  // namespace

ZAX_CHECKS(objects_rest_d)
{
    (void)cls;
    (void)n;
    Tally getters{"objects_rest_d: class getter"}, objects{"objects_rest_d: Create, CreateArray, destructors"},
        fields{"objects_rest_d: RegisterFields"}, real_fields{"objects_rest_d: registered descriptors and class (real data)"},
        slots{"objects_rest_d: the player list holder and the layer block in the fake world"},
        real{"objects_rest_d: the application's player list (real data)"};
    invcheck::g_hook_table = hooks_objects_rest_d;
    if (invcheck::arena_setup())
        invcheck::test_classes(objects, getters, fields, real_fields, CLASSES, 1);
    else
        objects.check(false, "objects_rest_d", "[no arena]");
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    make_static_texts();
    if (world_on()) {
        test_slots(slots);
        world_off();
    } else {
        slots.check(false, "objects_rest_d", "[the fake world could not be set up]");
    }
    slots.report();
    test_real(real);
    real.report();
}

}  // namespace zax
