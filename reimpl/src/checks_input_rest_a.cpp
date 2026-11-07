// Checks of input_rest_a.cpp against the original, in the game thread, in the fake world of
// checks_resta_world.h: the key table, the joystick, the mouse, the time, the INI and the network block are
// stand-ins; the bit set functions, the ZString functions and sprintf are the exe's.
//
//  - The control configuration in the object under test with every block in the fake block (the three bit
//    sets' words, up to 6 bindings, a joystick); the statics' case values point into the fake block too,
//    their guard bits set or not (a static the original makes on the heap is logged by its words, freed
//    and tagged before the state is taken). The frame with every argument combination (no packet, -1, a
//    packet; a vector wanted; mouse buttons; paused; capturing; a pending joystick check), the analogue
//    vector (mouse, joystick in and out of the dead zone, normalised, clamped), the joystick check, the
//    aiming angle, the binding text (keys, joystick bits, mouse buttons and the wheel; the out string by its
//    text), the INI words, the joystick change (a heap joystick freed, a new one made; tagged and freed after
//    the side), the joystick id, the pointer array's index search.
#include "checks_resta_world.h"
#include "x87.h"

namespace zax {

namespace {

using namespace ra;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD g_block_id;
void log_hash(const void *p, DWORD n) { log_event(0xb10c0000u + g_block_id++, n, fnv(2166136261u, p, n)); }

// Areas of the fake block
BYTE *area(int k) { return obj(O_SPARE + 2 + k); }
enum { A_WORDS0, A_WORDS1, A_WORDS2, A_BINDINGS, A_JOYSTICK, A_PRESSED, A_SENT, A_HELD, A_TEXT, A_ARRAY };

DWORD key_value(Gen &g)
{
    const DWORD keys[] = {0, 0, 0x1e, 0x1e, 0x39, 0x3b, 1, 0x153};
    return keys[g.next() % 8];
}
DWORD bits_value(Gen &g)
{
    const DWORD v = g.next() % 4;
    return v == 0 ? 0 : v == 1 ? 1u << (g.next() % 8) : v == 2 ? 3 : g.next();
}
DWORD time_value(Gen &g)
{
    const DWORD v = g.next() % 5;
    return v == 0 ? 0 : v == 1 ? 0xffffffffu : 500 + g.next() % 5000;
}
void fill_words(Gen &g, BYTE *w, DWORD n)
{
    for (DWORD i = 0; i < n; i++)
        D(w, i * 4) = g.next() % 3 == 0 ? 0 : g.next();
}
void make_binding(Gen &g, BYTE *b)
{
    for (DWORD k = 0; k < 0x40; k += 4)
        D(b, k) = g.next();
    D(b, 0) = key_value(g);
    D(b, 4) = key_value(g);
    D(b, 8) = bits_value(g);
    D(b, 0xc) = bits_value(g);
    D(b, 0x10) = g.next() % 4;
    D(b, 0x14) = g.next() % 500;
    D(b, 0x18) = g.next() % 500;
    D(b, 0x1c) = time_value(g);
    b[0x20] = (BYTE)(g.next() % 2);
    D(b, 0x30) = time_value(g);
    D(b, 0x34) = g.next() % 3 ? 0 : bits_value(g);
    D(b, 0x38) = g.next() % 3 ? 0 : bits_value(g);
    D(b, 0x3c) = g.next() % 3 ? 0 : key_value(g);
}
DWORD finite_axis(Gen &g)
{
    const DWORD v = g.next() % 8;
    if (v == 0)
        return 0;
    if (v == 1)
        return 0x7fc00000u;
    return bits(-1.5f + 3.0f * (float)((g.next() >> 8) % 1000) * 0.001f);
}

void make_config(Gen &g)
{
    BYTE *s = obj(O_SELF);
    const DWORD bits = g.next() % 5 == 0 ? 0 : 1 + g.next() % 64;
    const DWORD n = bits ? g.next() % (bits < 6 ? bits + 1 : 7) : 0;
    const DWORD sets[3] = {0, 0xc, 0x64};
    for (int i = 0; i < 3; i++) {
        BYTE *w = area(A_WORDS0 + i);
        fill_words(g, w, 4);
        D(s, sets[i]) = ptrv(w);
        D(s, sets[i] + 4) = bits;
        s[sets[i] + 8] = (BYTE)(g.next() % 2);
        s[sets[i] + 9] = (BYTE)(g.next() % 2);
    }
    D(s, 0x3c) = n;
    BYTE *b = area(A_BINDINGS);
    for (DWORD i = 0; i < 6; i++)
        make_binding(g, b + i * 0x40);
    D(s, 0x40) = ptrv(b);
    for (DWORD k = 0x4c; k < 0x50; k++)
        s[k] = (BYTE)(g.next() % 2);
    for (DWORD k = 0x50; k < 0x64; k += 4)
        D(s, k) = finite_axis(g);
    D(s, 0x58) = 500 + g.next() % 5000;
    BYTE *j = area(A_JOYSTICK);
    D(j, 0) = g.next() % 4;
    D(j, 0x14) = finite_axis(g);
    D(j, 0x18) = finite_axis(g);
    D(s, 0x70) = g.next() % 3 ? ptrv(j) : 0;
    s[0x74] = (BYTE)(g.next() % 4 == 0);
    s[0x75] = (BYTE)(g.next() % 6 == 0);
    const char *const names[] = {"Gamepad Pro", "NO JOYSTICK", "", "Joystick"};
    const char *nm = names[g.next() % 4];
    DWORD k = 0;
    for (; nm[k]; k++)
        s[0xb8 + k] = nm[k];
    s[0xb8 + k] = 0;
    D(s, 0x1b8) = 500 + g.next() % 5000;
    D(s, 0x1c0) = g.next() % 3;
    s[0x1c4] = (BYTE)(g.next() % 6 != 0);
    s[0x1c5] = (BYTE)(g.next() % 2);
    // the statics: words in the fake block, the guard bits set (made) or not
    const DWORD statics[3] = {0x6bd548, 0x6bd558, 0x6bd570};
    for (int i = 0; i < 3; i++) {
        BYTE *w = area(A_PRESSED + i);
        fill_words(g, w, 4);
        DWORD *c = case_global(statics[i]);
        c[0] = ptrv(w);
        c[1] = bits;
        c[2] = g.next() & 0xffff;
        if (i == 2)
            c[3] = g.next();   // the paused byte 0x6bd57c and its neighbours
    }
    case_global(0x6bd564)[0] = g.next() % 4 == 0 ? g.next() % 16 : 15;
    DWORD *v = case_global(0x6bd538);
    v[0] = finite_axis(g);
    v[1] = finite_axis(g);
}

// Statics the original made on the heap (their case value was not a fake block): their words logged, the
// block freed and the pointer tagged
void tag_statics()
{
    g_block_id = 0;
    const DWORD statics[3] = {0x6bd548, 0x6bd558, 0x6bd570};
    for (int i = 0; i < 3; i++) {
        DWORD &p = *at<DWORD *>(statics[i]);
        if (p == ptrv(area(A_PRESSED + i)))
            continue;
        const DWORD bits = *at<DWORD *>(statics[i] + 4);
        log_hash(at<void *>(p), (((bits & 0x1f) ? 1 : 0) + (bits >> 5)) * 4);
        heap_free(at<void *>(p));
        p = 0xe1000000u + (DWORD)i;
    }
}

// The binding text's out string: by its text, then freed
DWORD g_kept_text;
void tag_text()
{
    tag_statics();
    BYTE *z = area(A_TEXT);
    g_kept_text = D(z, 0);
    const char *t = at<const char *(THISCALL *)(const void *)>(0x4e13a0)(z);
    log_event(0x7e700000u, fnv_text(2166136261u, t), 0);
    D(z, 0) = 0xe5000000u;
}
void free_text()
{
    BYTE *z = area(A_TEXT);
    D(z, 0) = g_kept_text;
    at<void(THISCALL *)(void *)>(0x4defd0)(z);
}

// The joystick change: the heap joystick the side made, tagged and freed
DWORD g_kept_joystick;
void tag_joystick()
{
    tag_statics();
    BYTE *s = obj(O_SELF);
    g_kept_joystick = D(s, 0x70);
    if (g_kept_joystick) {
        log_hash(at<void *>(g_kept_joystick), 4);
        D(s, 0x70) = 0xe4000000u;
    }
}
void free_joystick()
{
    if (g_kept_joystick)
        heap_free(at<void *>(g_kept_joystick));
    g_kept_joystick = 0;
}

void make_self(Gen &g) { make_config(g); }

void a_none(Gen &, DWORD *) {}
void a_frame(Gen &g, DWORD *a)
{
    a[0] = g.next() % 2 ? 1 : g.next() % 3 ? 0 : 0x100;
    a[1] = g.next() % 3 == 0 ? 1 : g.next() % 4 == 0 ? 0xffffff00u : 0;
    const DWORD v = g.next() % 4;
    a[2] = v == 0 ? 0 : v == 1 ? 0xffffffffu : ptrv(obj(O_PACKET));
}
void a_vector(Gen &g, DWORD *a)
{
    BYTE *m = obj(O_MSG);
    D(m, 0) = finite_axis(g);
    D(m, 4) = finite_axis(g);
    a[0] = ptrv(m);
}
void a_index(Gen &g, DWORD *a)
{
    const DWORD n = D(obj(O_SELF), 0x3c);
    a[0] = n ? g.next() % n : 0;
}
void a_text(Gen &g, DWORD *a)
{
    a_index(g, a);
    BYTE *z = area(A_TEXT);
    D(z, 0) = ZSTRING_EMPTY;
    if (g.next() % 2)
        at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, "old text");
    a[1] = ptrv(z);
}
void a_joystick(Gen &g, DWORD *a)
{
    BYTE *s = obj(O_SELF);
    if (D(s, 0x70)) {
        BYTE *j = static_cast<BYTE *>(heap_alloc(0x100));
        memset(j, 0, 0x100);
        D(j, 0) = g.next() % 4;
        D(j, 0x48) = ZSTRING_EMPTY;
        D(s, 0x70) = ptrv(j);
    }
    a[0] = g.next() % 3 == 0 ? 0xffffffffu : g.next() % 4;
}
void a_find(Gen &g, DWORD *a)
{
    BYTE *s = obj(O_SELF);
    BYTE *data = area(A_ARRAY);
    const DWORD n = g.next() % 9;
    for (DWORD i = 0; i < 8; i++)
        D(data, i * 4) = g.next() % 5;
    D(s, 0) = 0x5ee138;
    D(s, 4) = ptrv(data);
    D(s, 8) = n;
    D(s, 0xc) = 8;
    BYTE *m = obj(O_MSG);
    D(m, 0) = g.next() % 6;
    a[0] = ptrv(m);
    a[1] = g.next() % 4 == 0 ? g.next() : g.next() % 10;
}

struct Test {
    SlotTest t;
    void (*before)();
    void (*after)();
};
const Test TESTS[] = {
    {{"ControlConfig::SetVector", 0x470030, 1, 0, 40, a_vector}, tag_statics, nullptr},
    {{"ControlConfig::Frame", 0x470050, 3, 0, 2500, a_frame}, tag_statics, nullptr},
    {{"ControlConfig::AnalogueVector", 0x4708c0, 0, 0, 800, a_none}, tag_statics, nullptr},
    {{"ControlConfig::CheckJoystick2", 0x470ac0, 0, 0xff, 200, a_none}, tag_statics, nullptr},
    {{"ControlConfig::BindingText", 0x470b60, 2, 0, 600, a_text}, tag_text, free_text},
    {{"ControlConfig::IniWord1", 0x470ce0, 1, 0xffffffff, 30, a_index}, tag_statics, nullptr},
    {{"ControlConfig::IniWord2", 0x470d00, 1, 0xffffffff, 30, a_index}, tag_statics, nullptr},
    {{"ControlConfig::SetJoystick", 0x470d20, 1, 0, 200, a_joystick}, tag_joystick, free_joystick},
    {{"ControlConfig::JoystickId", 0x470e50, 0, 0xffffffff, 30, a_none}, tag_statics, nullptr},
    {{"PtrArray::IndexOfHint", 0x470e60, 2, 0xffffffff, 300, a_find}, tag_statics, nullptr},
};

void test_all(Tally &t)
{
    for (const Test &x : TESTS) {
        g_before_state = x.before;
        g_after_side = x.after;
        Policy p{};
        const unsigned n = (unsigned)scaled(x.t.count);
        for (unsigned seed = 1; seed <= n; seed++)
            run_slot(t, x.t, seed * 2654435761u + x.t.address, make_self, p);
    }
    g_before_state = nullptr;
    g_after_side = nullptr;
}

// The aiming angle: st(0) of both sides on random vectors (the arc tangent table; no fake world)
void test_angle(Tally &t)
{
    char d[120];
    BYTE *s = obj(O_SELF);
    for (unsigned k = 0; k < (unsigned)scaled(300); k++) {
        Gen g{k * 2654435761u + 0x470b50};
        D(s, 0x5c) = finite_axis(g);
        D(s, 0x60) = finite_axis(g);
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            r[side] = bits(fstp(at<long double(THISCALL *)(void *)>(0x470b50)(s)));
            if (side == 0)
                hooks_in();
        }
        wsprintfA(d, "[aim angle case %u: %x / %x]", k, r[0], r[1]);
        t.check(r[0] == r[1], "", d);
    }
}

}  // namespace

ZAX_CHECKS(input_rest_a)
{
    (void)cls;
    (void)n;
    Tally t{"input_rest_a: control configuration frame, vector, joystick, binding text in the fake world"};
    make_static_texts();
    test_angle(t);
    if (world_on()) {
        test_all(t);
        world_off();
    } else {
        t.check(false, "input_rest_a", "[the fake world could not be set up]");
    }
    t.report();
}

}  // namespace zax
