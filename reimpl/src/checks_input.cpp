// Checks of the input state (input.cpp) against the original, in the game thread (machinery:
// checks_inventory_common.h; side 0 with the module's hooks taken out, side 1 through the
// reimplementation).
//  - The key table: every function on a table of our own (keys from a seed, sparse and dense, codes up to
//    0x1ff), the "keys off" byte 0x6c1870 set and clear (saved and restored), App_PumpMessages 0x5998f0 a
//    recorder, the character filter a recorder. Compared: eax (al where only al is defined), the table and
//    the event log.
//  - Key names and the keyboard repeat: GetKeyNameTextA (import 0x5ea248) and SystemParametersInfoA
//    (0x5ea278) fakes, the buffer 0x6c1974 saved and restored; the long double results by their 10 bytes.
//    Real data: the real Windows functions for every scan code 0-0x7f and both settings.
//  - The joystick: joyGetNumDevs, joyGetDevCapsA, joyGetPosEx (imports 0x5ea298, 0x5ea28c, 0x5ea29c),
//    RegOpenKeyExA, RegQueryValueExA, RegCloseKey (0x5ea004, 0x5ea008, 0x5ea000) and the time thunk 0x4d2460
//    as fakes giving values from the seed (ranges, positions, buttons, hat angles, missing devices and
//    registry keys); the poll's JOYINFOEX 0x6c0eb0 saved and restored. Objects of our own (their name
//    string compared by text). Real data: the device query and OEM name of device 0 and a poll on a copy of
//    the game's joystick 0x6c0db0 with the real functions.
#include "checks_inventory_common.h"
#include "x87.h"

WINIMPORT char *WINAPI lstrcpynA(char *out, const char *s, int n);

namespace zax {

int hooks_input(Hook *out);

namespace {

using namespace invcheck;

DWORD g_answers[32];
DWORD ans(int k) { return g_answers[k]; }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }

// ---- The key table ---------------------------------------------------------------------------------------

struct KeyTable {
    DWORD keys[0x200];
    char chars[12];
    DWORD count, write, read, filter, context;
};
KeyTable g_table;
KeyTable g_table_after[2];

DWORD __cdecl s_pump() { ev(0x701); return result(0x01); }
DWORD __fastcall f_filter(DWORD context, DWORD c) { ev(0x702, context, c); return result(ans(0) % 3 == 0); }

void setup_table(unsigned seed)
{
    g_seed = seed;
    for (int k = 0; k < 32; k++)
        g_answers[k] = random();
    KeyTable &t = g_table;
    const DWORD density = rnd(4);   // 0: empty, 1: one key, 2: sparse, 3: dense
    for (DWORD i = 0; i < 0x200; i++)
        t.keys[i] = density == 3 || (density == 2 && rnd(40) == 0) ? random() : (random() & ~3u) * (rnd(2));
    if (density == 1)
        t.keys[1 + rnd(0x1ff)] |= 1 + rnd(3);
    for (DWORD k = 0; k < 12; k++)
        t.chars[k] = (char)random();
    t.count = rnd(11);
    t.write = rnd(10);
    t.read = rnd(10);
    t.filter = rnd(3) ? 0 : ptrv((void *)&f_filter);
    t.context = random();
}

struct KeyCase {
    const char *name;
    DWORD fn;
    int args;   // stack arguments
    bool al, no_eax;
};
const KeyCase KEY_CASES[] = {
    {"Keys::ReadChar", 0x4e2720, 0, true, false},        {"Keys::ClearChars", 0x4e2760, 1, false, false},
    {"Keys::ClearAll", 0x4e27a0, 1, false, false},       {"Keys::TakePress", 0x4e2820, 1, true, false},
    {"Keys::IsDown", 0x4e2850, 1, false, false},         {"Keys::TakeAny", 0x4e2870, 0, false, false},
    {"Keys::TakeCombo", 0x4e28b0, 4, true, false},       {"Keys::TakeAnyPressed", 0x4e2900, 0, false, false},
    {"Keys::TakeOtherPressed", 0x4e2940, 1, false, false}, {"Keys::ClearPresses", 0x4e2980, 1, false, false},
    {"Keys::ClearDowns", 0x4e29b0, 1, false, false},     {"Keys::Press", 0x4e29e0, 1, false, false},
    {"Keys::Release", 0x4e29f0, 1, false, false},        {"Keys::AddChar", 0x4e2a00, 1, false, true},
};
constexpr int NKEY_CASES = sizeof KEY_CASES / sizeof KEY_CASES[0];

DWORD key_arg(DWORD fn)
{
    if (fn == 0x4e2760 || fn == 0x4e27a0 || fn == 0x4e2980 || fn == 0x4e29b0)
        return rnd(2) ? 0 : random();
    if (fn == 0x4e2a00)
        return rnd(5) ? random() : 0;
    return rnd(8) ? 1 + rnd(0x1ff) : rnd(3) ? 0 : 0x1ff;
}

void test_key_case(Tally &t, int k, unsigned seed)
{
    const KeyCase &c = KEY_CASES[k];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        setup_table(seed);
        log_reset(side);
        side_begin(side);
        CodePatch pump;
        patch_jump(pump, 0x5998f0, (void *)&s_pump);
        const BYTE off = *at<BYTE *>(0x6c1870);
        *at<BYTE *>(0x6c1870) = (BYTE)(rnd(5) == 0);
        const DWORD a = key_arg(c.fn), b = rnd(3) ? 0 : key_arg(0), x = rnd(3) ? 0 : key_arg(0), y = rnd(3) ? 0 : key_arg(0);
        switch (c.args) {
        case 0: r[side] = at<DWORD(THISCALL *)(void *)>(c.fn)(&g_table); break;
        case 1: r[side] = at<DWORD(THISCALL *)(void *, DWORD)>(c.fn)(&g_table, a); break;
        case 4: r[side] = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(c.fn)(&g_table, a, b, x, y); break;
        }
        *at<BYTE *>(0x6c1870) = off;
        unpatch(pump);
        side_end(side);
        // the end of the table as an offset
        if (r[side] == ptrv(&g_table) + 0x800)
            r[side] = 0x7ab1e800;
        memcpy(&g_table_after[side], &g_table, sizeof g_table);
    }
    if (c.al) {
        r[0] &= 0xff;
        r[1] &= 0xff;
    }
    if (c.no_eax)
        r[0] = r[1] = 0;
    const bool same = r[0] == r[1] && same_log() && memcmp(&g_table_after[0], &g_table_after[1], sizeof(KeyTable)) == 0;
    char d[120];
    wsprintfA(d, "[seed %u: eax %x / %x, log %d / %d]", seed, r[0], r[1], g_nlog[0] / 4, g_nlog[1] / 4);
    t.check(same, c.name, d);
}

// ---- Key names, the keyboard repeat -------------------------------------------------------------------------

int __stdcall fake_key_name(int lparam, char *out, int n)
{
    ev(0x710, (DWORD)lparam, ptrv(out), (DWORD)n);
    if (ans(1) % 4 == 0)
        return 0;
    return wsprintfA(out, "Key %x", (DWORD)lparam >> 16);
}
BOOL __stdcall fake_spi(DWORD action, DWORD param, DWORD *out, DWORD ini)
{
    ev(0x711, action, param, ini);
    static const DWORD V[] = {0, 1, 2, 3, 15, 31, 32, 0xffffffff};
    *out = V[ans(2) % 8];
    return 1;
}

struct LongDouble {
    BYTE b[12];
};
void test_names(Tally &t, unsigned seed, bool real)
{
    const DWORD fns[4] = {0x4e2ab0, 0x4e2a90, 0x4e2ae0, 0x4e2b30};
    const int k = (int)(seed % 4);
    DWORD r[2];
    LongDouble v[2];
    char text[2][0x100];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        for (int i = 0; i < 32; i++)
            g_answers[i] = random();
        log_reset(side);
        side_begin(side);
        ImportPatch p1, p2;
        if (!real) {
            patch_import(p1, 0x5ea248, (void *)&fake_key_name);
            patch_import(p2, 0x5ea278, (void *)&fake_spi);
        }
        SavedBytes saved;
        save_bytes(saved, 0x6c1974, 0x100);
        const DWORD scan = real ? seed / 4 : rnd(4) ? rnd(0x80) : random();
        memset(&v[side], 0, sizeof v[side]);
        r[side] = 0;
        text[side][0] = 0;
        if (k < 2) {
            const char *s = at<const char *(__stdcall *)(DWORD)>(fns[k])(scan);
            r[side] = s == at<const char *>(0x6c1974) ? 1 : s ? 2 : 0;
            if (s)
                lstrcpynA(text[side], s, 0x100);
        } else {
            const long double x = at<long double(__cdecl *)()>(fns[k])();
            memcpy(v[side].b, &x, 10);
        }
        restore_bytes(saved);
        if (!real) {
            unpatch_import(p2);
            unpatch_import(p1);
        }
        side_end(side);
    }
    char d[160];
    wsprintfA(d, "[seed %u: %x / %x, '%.30s' / '%.30s']", seed, r[0], r[1], text[0], text[1]);
    t.check(r[0] == r[1] && lstrcmpA(text[0], text[1]) == 0 && memcmp(v[0].b, v[1].b, 10) == 0 && same_log(),
            k == 0 ? "KeyNameText" : k == 1 ? "KeyName" : k == 2 ? "KeyboardDelay" : "KeyboardRepeat", d);
}

// ---- The joystick ------------------------------------------------------------------------------------------

struct JoyCaps {
    WORD mid, pid;
    char name[32];
    DWORD xmin, xmax, ymin, ymax, zmin, zmax, buttons, period_min, period_max, rmin, rmax, umin, umax, vmin, vmax, caps, max_axes,
        num_axes, max_buttons;
    char reg_key[32];
    char oem_vxd[260];
};
struct JoyInfo {
    DWORD size, flags, x, y, z, r, u, v, buttons, button_number, pov, reserved1, reserved2;
};
DWORD g_tick;

DWORD __stdcall fake_num_devs() { ev(0x720); return ans(3) % 4; }
DWORD __stdcall fake_dev_caps(DWORD id, JoyCaps *c, DWORD size)
{
    ev(0x721, id, size);
    memset(c, 0, sizeof *c);   // written even when it fails: the original reads it anyway
    static const DWORD R[] = {0, 1, 100, 0x7fff, 0xffff, 0x10000, 50, 0xfffff};
    c->xmin = R[ans(4) % 4];
    c->xmax = R[2 + ans(5) % 6];
    c->ymin = R[ans(6) % 4];
    c->ymax = R[2 + ans(7) % 6];
    c->zmin = R[ans(8) % 8];
    c->zmax = R[ans(9) % 8];
    c->rmin = R[ans(10) % 8];
    c->rmax = R[ans(11) % 8];
    wsprintfA(c->reg_key, "DINPUT.DLL %u", ans(12) % 3);
    return ans(13) % 5 == 0 ? 0xa5 : 0;
}
DWORD __stdcall fake_pos_ex(DWORD id, JoyInfo *info)
{
    ev(0x722, id, info->size, info->flags);
    if (ans(14) % 6 == 0)
        return 0xa7;
    const DWORD k = random();
    info->x = ans(15) % 4 == 0 ? 0x7fff : k % 0x10001;
    info->y = ans(16) % 4 == 0 ? 0x7fff : (k >> 7) % 0x10001;
    info->z = (k >> 3) & 0xffff;
    info->r = (k >> 9) & 0xffff;
    info->buttons = ans(17) % 3 ? 0 : random();
    static const DWORD POV[] = {0xffff, 0, 4500, 9000, 13500, 18000, 22500, 27000, 31500, 0x1d4c, 0x6f54, 0x8ca0, 0x5dc, 0x4074, 0x2904, 0x639c};
    info->pov = POV[ans(18) % 16];
    g_answers[14] = random();
    g_answers[15] = random();
    g_answers[16] = random();
    return 0;
}
int __stdcall fake_reg_open(DWORD root, const char *path, DWORD options, DWORD sam, DWORD *key)
{
    ev(0x723, root, fnv_text(2166136261u, path), options ^ (sam << 1));
    *key = 0x5e7a0000u + ans(19) % 16;
    return ans(19) % 5 == 0 ? 2 : 0;
}
int __stdcall fake_reg_query(DWORD key, const char *name, DWORD *reserved, DWORD *type, char *data, DWORD *size)
{
    ev(0x724, key, fnv_text(2166136261u, name), ptrv(reserved) ^ ptrv(type) ^ *size);
    g_answers[19] = random();
    if (ans(20) % 4 == 0)
        return 2;
    static const char *const NAMES[] = {"Gamepad", "  ", "", " Stick Pro  ", "Button A", "Fire"};
    lstrcpynA(data, NAMES[ans(21) % 6], (int)*size);
    g_answers[21] = random();
    return 0;
}
int __stdcall fake_reg_close(DWORD key) { ev(0x725, key); return 0; }
DWORD __cdecl fake_ticks()
{
    ev(0x726);
    g_tick += ans(22) % 3 == 0 ? 31000 : ans(22) % 100;
    g_answers[22] = random();
    return g_tick;
}

constexpr DWORD JOY_IMPORTS[] = {0x5ea298, 0x5ea28c, 0x5ea29c, 0x5ea004, 0x5ea008, 0x5ea000};
void *const JOY_FAKES[] = {(void *)&fake_num_devs, (void *)&fake_dev_caps,  (void *)&fake_pos_ex,
                           (void *)&fake_reg_open, (void *)&fake_reg_query, (void *)&fake_reg_close};

BYTE g_joy[0x100];
BYTE g_joy_after[2][0x100];
char g_text[2][0x100];

DWORD float_from(DWORD k)
{
    static const float F[] = {0.0f, 0.2f, 1.25f, 0.5f, -0.3f, 1.0f, 2.0f, 1e-6f, 0.35f, 3.0f};
    if (k % 17 == 0)
        return 0x7fc00000;
    return float_bits(F[k % 10]);
}
void setup_joy(unsigned seed)
{
    g_seed = seed;
    for (int k = 0; k < 32; k++)
        g_answers[k] = random();
    for (DWORD k = 0; k < sizeof g_joy; k += 4)
        *reinterpret_cast<DWORD *>(g_joy + k) = random();
    auto D = [](DWORD off) -> DWORD & { return *reinterpret_cast<DWORD *>(g_joy + off); };
    D(0) = rnd(4);
    for (DWORD off = 4; off <= 0x20; off += 4)
        D(off) = float_from(rnd(170));
    for (DWORD off = 0x24; off <= 0x30; off += 4)
        D(off) = rnd(3) ? 0 : random() & 0xffff;
    for (DWORD off = 0x34; off <= 0x40; off += 4)
        D(off) = rnd(4) ? float_bits(2.0f / 65535.0f) : float_from(rnd(170));
    D(0x44) = random();
    D(0x4c) = rnd(2) ? 0xffffffff : random();
    D(0x50) = float_from(rnd(170));
    D(0x54) = float_from(rnd(170));
    D(0x58) = rnd(3) ? 0 : random();
    g_joy[0xfc] = (BYTE)rnd(2);
    g_tick = rnd(2) ? random() : D(0x58) + 1000;
}

struct JoyCase {
    const char *name;
    DWORD fn;
};
const JoyCase JOY_CASES[] = {
    {"Joystick_HasOemName", 0x4dd930}, {"Joystick_Answers", 0x4dd940}, {"CJoystick::Ctor", 0x4dd9a0}, {"CJoystick::Init", 0x4dda00},
    {"CJoystick::Poll", 0x4ddb80},     {"CJoystick::BitName", 0x4dde50}, {"CJoystick::BitsText", 0x4de080}, {"Joystick_OemName", 0x4de0e0},
};
constexpr int NJOY_CASES = sizeof JOY_CASES / sizeof JOY_CASES[0];

DWORD run_joy(int k)
{
    ZString *name = reinterpret_cast<ZString *>(g_joy + 0x48);
    ZString out;
    at<void *(THISCALL *)(ZString *)>(0x4dec90)(&out);
    if (rnd(2))
        at<void(THISCALL *)(ZString *, const char *)>(0x4df010)(&out, "previous");
    if (k != 2) {
        at<void *(THISCALL *)(ZString *)>(0x4dec90)(name);
        if (rnd(2))
            at<void(THISCALL *)(ZString *, const char *)>(0x4df010)(name, "Old Name");
    }
    DWORD r = 0;
    const DWORD id = rnd(5);
    switch (JOY_CASES[k].fn) {
    case 0x4dd930: r = at<DWORD(__fastcall *)(DWORD, ZString *)>(0x4dd930)(id, &out); break;
    case 0x4de0e0: r = at<DWORD(__fastcall *)(DWORD, ZString *)>(0x4de0e0)(id, &out); break;
    case 0x4dd940: r = at<DWORD(__fastcall *)(DWORD)>(0x4dd940)(id); break;
    case 0x4dd9a0: r = at<DWORD(THISCALL *)(void *, DWORD)>(0x4dd9a0)(g_joy, id) == ptrv(g_joy); break;
    case 0x4dda00: r = at<DWORD(THISCALL *)(void *, DWORD)>(0x4dda00)(g_joy, id); break;
    case 0x4ddb80: r = at<DWORD(THISCALL *)(void *)>(0x4ddb80)(g_joy); break;
    case 0x4dde50: at<void(THISCALL *)(void *, DWORD, ZString *)>(0x4dde50)(g_joy, rnd(3) ? rnd(24) : 24 + rnd(8), &out); break;
    case 0x4de080: at<void(THISCALL *)(void *, DWORD, ZString *)>(0x4de080)(g_joy, rnd(3) ? random() : 1u << rnd(32), &out); break;
    }
    r ^= fnv_text(0, out.c_str()) ^ (fnv_text(0, name->c_str()) << 1);
    at<void(THISCALL *)(ZString *)>(0x4defd0)(&out);
    return r;
}

void test_joy_case(Tally &t, int k, unsigned seed)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        setup_joy(seed);
        log_reset(side);
        side_begin(side);
        ImportPatch imports[6];
        for (int i = 0; i < 6; i++)
            patch_import(imports[i], JOY_IMPORTS[i], JOY_FAKES[i]);
        CodePatch tick;
        patch_jump(tick, 0x4d2460, (void *)&fake_ticks);
        SavedBytes pos;
        save_bytes(pos, 0x6c0eb0, 0x34);
        g_seed = seed * 7u + 3u;
        r[side] = run_joy(k);
        lstrcpynA(g_text[side], reinterpret_cast<ZString *>(g_joy + 0x48)->c_str(), 0x100);
        memcpy(g_joy_after[side], g_joy, sizeof g_joy);
        *reinterpret_cast<DWORD *>(g_joy_after[side] + 0x48) = 0;   // the name compared by its text
        restore_bytes(pos);
        unpatch(tick);
        for (int i = 5; i >= 0; i--)
            unpatch_import(imports[i]);
        side_end(side);
        at<void(THISCALL *)(void *)>(0x4defd0)(g_joy + 0x48);
    }
    // the strings' own destructors ran (out and the name): only their texts were compared
    const bool same = r[0] == r[1] && same_log() && lstrcmpA(g_text[0], g_text[1]) == 0 && memcmp(g_joy_after[0], g_joy_after[1], 0x100) == 0;
    int at_byte = -1;
    for (int i = 0; i < 0x100; i++)
        if (g_joy_after[0][i] != g_joy_after[1][i]) {
            at_byte = i;
            break;
        }
    char d[200];
    wsprintfA(d, "[seed %u: %x / %x, log %d / %d, name '%.20s' / '%.20s', object differs at %d]", seed, r[0], r[1], g_nlog[0] / 4,
              g_nlog[1] / 4, g_text[0], g_text[1], at_byte);
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, JOY_CASES[k].name, d);
}

// Real data: device 0's query and OEM name and a poll on a copy of the game's joystick 0x6c0db0 (its name
// string a fresh one), with the real winmm and registry functions
void test_joy_real(Tally &t, int &cases)
{
    for (int k = 0; k < 3; k++) {
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            SavedBytes pos;
            save_bytes(pos, 0x6c0eb0, 0x34);
            ZString out;
            at<void *(THISCALL *)(ZString *)>(0x4dec90)(&out);
            if (k == 0)
                r[side] = at<DWORD(__fastcall *)(DWORD)>(0x4dd940)(0);
            else if (k == 1)
                r[side] = at<DWORD(__fastcall *)(DWORD, ZString *)>(0x4de0e0)(0, &out) ^ fnv_text(0, out.c_str());
            else {
                memcpy(g_joy, at<void *>(0x6c0db0), 0x100);
                at<void *(THISCALL *)(ZString *)>(0x4dec90)(reinterpret_cast<ZString *>(g_joy + 0x48));
                r[side] = at<DWORD(THISCALL *)(void *)>(0x4ddb80)(g_joy);
                memcpy(g_joy_after[side], g_joy, 0x100);
                at<void(THISCALL *)(void *)>(0x4defd0)(g_joy + 0x48);
            }
            at<void(THISCALL *)(ZString *)>(0x4defd0)(&out);
            restore_bytes(pos);
            side_end(side);
        }
        char d[80];
        wsprintfA(d, "[real %d: %x / %x]", k, r[0], r[1]);
        t.check(r[0] == r[1] && (k < 2 || memcmp(g_joy_after[0], g_joy_after[1], 0x100) == 0), "joystick", d);
        cases++;
    }
}

}  // namespace

ZAX_CHECKS(input)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_input;
    Tally keys{"input: the key table"};
    for (int k = 0; k < NKEY_CASES; k++)
        for (unsigned s = 0; s < (unsigned)scaled(300); s++)
            test_key_case(keys, k, 3000u * (unsigned)k + s);
    keys.report();

    Tally names{"input: key names, the keyboard repeat"};
    for (unsigned s = 0; s < (unsigned)scaled(400); s++)
        test_names(names, s, false);
    names.report();
    Tally real_names{"input: key names and the keyboard repeat with the real Windows functions (real data)"};
    for (unsigned s = 0; s < 0x80 * 4; s++)
        if (s % 4 < 2 || s < 8)
            test_names(real_names, s, true);
    real_names.report();

    Tally joy{"input: the joystick"};
    for (int k = 0; k < NJOY_CASES; k++) {
        log("  %s", JOY_CASES[k].name);   // the last line before a case that ends the game
        for (unsigned s = 0; s < (unsigned)scaled(k == 4 ? 1200 : 300); s++)
            test_joy_case(joy, k, 9000u * (unsigned)k + s);
    }
    joy.report();
    Tally joy_real{"input: the joystick with the real winmm and registry (real data)"};
    int cases = 0;
    test_joy_real(joy_real, cases);
    joy_real.report();
}

}  // namespace zax
