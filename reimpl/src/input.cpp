// The game's input state (phase 3, game flow): the key table 0x4e2720-0x4e2a8a with its character
// queue, the key names and the keyboard repeat settings 0x4e2a90-0x4e2b7a, and the joystick
// 0x4dd930-0x4de28c (winmm and the registry through the exe's import slots).
//
// Key table [confirmed: code; callers 0x409xxx (the window framework) and 0x599a0c-0x599fc6 (the window
// procedure)]: a dword per key code 1-0x1ff (+4 * code; bit 0 down, bit 1 pressed since it was last
// taken), +0x800 a queue of 10 typed characters, +0x80c their count, +0x810 the write and +0x814 the read
// position, +0x818 an optional filter (fastcall (context +0x81c, character), al 1 swallows it). While the
// byte 0x6c1870 is set no key counts as pressed. Some functions pump the window messages first
// (App_PumpMessages 0x5998f0) unless their argument is set.
//
// Joystick [confirmed: code, JOYCAPSA/JOYINFOEX of winmm] (the global one 0x6c0db0, built by the static
// initializer 0x4dd890): +0 the device id, +4/+8 the x dead zone (0.2) and its scale (1.25), +0xc/+0x10
// the same for y, +0x14/+0x18 the x and y axes (-1..1 outside the dead zone, scaled), +0x1c/+0x20 the z
// and rudder axes (0..1), +0x24..+0x30 the negated minimums of x, y, z and r, +0x34..+0x40 their
// factors (2 / range for x and y, 1 / range for z and r; z's range is wZmax - wRmin, as in the original),
// +0x44 the buttons and directions of the last poll (bits 0-23 the buttons, 0x1000000 hat left, 0x2000000
// hat right, 0x4000000 hat up, 0x8000000 hat down, 0x10000000 left, 0x20000000 right, 0x40000000 up,
// 0x80000000 down), +0x48 the device's name (ZString), +0x4c the bits that were released since (a bit
// counts once until it is released), +0x50/+0x54 the axes at the last movement and +0x58 its time
// (after 30 s without movement the axes read 0), +0xfc connected.
#include "exe_base.h"
#include "x87.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

static DWORD pump_messages() { return fn<DWORD(__cdecl *)()>(0x5998f0)(); }
static void zs_ctor(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
static void zs_set_text(void *z, const char *s) { fn<void(THISCALL *)(void *, const char *)>(0x4df010)(z, s); }
static const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
static void zs_trim_right(void *z) { fn<void *(THISCALL *)(void *)>(0x4e0f20)(z); }
static void zs_trim_left(void *z) { fn<void *(THISCALL *)(void *)>(0x4e10f0)(z); }
static char *zs_get_buffer(void *z, DWORD n) { return fn<char *(THISCALL *)(void *, DWORD)>(0x4e1590)(z, n); }
static void zs_release_buffer(void *z, int n) { fn<void(THISCALL *)(void *, int)>(0x4e16d0)(z, n); }
template <typename... A> static void zs_format(void *z, const char *fmt, A... a) { fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, fmt, a...); }
static void zs_append_format(void *z, const char *fmt) { fn<void(__cdecl *)(void *, const char *, ...)>(0x4e0a30)(z, fmt); }
template <typename... A> static void crt_sprintf(char *out, const char *fmt, A... a) { fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(out, fmt, a...); }
static bool text_is_blank(const char *s) { return fn<bool(__fastcall *)(const char *)>(0x587a70)(s); }
static DWORD ticks() { return fn<DWORD(__cdecl *)()>(0x4d2460)(); }

// Imports of the exe (their slots, so the checks can put stand-ins there)
template <typename F> static F import(DWORD slot) { return *at<F *>(slot); }
using JoyGetNumDevs = DWORD(STDCALL *)();
using JoyGetDevCaps = DWORD(STDCALL *)(DWORD, void *, DWORD);
using JoyGetPosEx = DWORD(STDCALL *)(DWORD, void *);
using RegOpenKeyEx = int(STDCALL *)(DWORD, const char *, DWORD, DWORD, DWORD *);
using RegQueryValueEx = int(STDCALL *)(DWORD, const char *, DWORD *, DWORD *, void *, DWORD *);
using RegCloseKey = int(STDCALL *)(DWORD);
constexpr DWORD IMP_JOY_NUM_DEVS = 0x5ea298, IMP_JOY_DEV_CAPS = 0x5ea28c, IMP_JOY_POS_EX = 0x5ea29c, IMP_REG_OPEN = 0x5ea004,
                IMP_REG_QUERY = 0x5ea008, IMP_REG_CLOSE = 0x5ea000, IMP_KEY_NAME_TEXT = 0x5ea248, IMP_SYSTEM_PARAMETERS = 0x5ea278;
constexpr DWORD HKLM = 0x80000002, KEY_ALL = 0xf003f, NO_JOY_NAME = 0xa6;

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
static BYTE &by(void *p, DWORD off) { return *(static_cast<BYTE *>(p) + off); }
static DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
static float cf(DWORD a) { return *at<float *>(a); }

// ---- The key table -------------------------------------------------------------------------------------

constexpr DWORD KEYS_OFF = 0x6c1870;   // set: no key counts as pressed
constexpr DWORD NUM_KEYS = 0x1ff;
struct KeyTable {
    DWORD keys[0x200];   // 0x000 (0 unused)
    char chars[12];      // 0x800 the queue (10)
    DWORD count;         // 0x80c
    DWORD write;         // 0x810
    DWORD read;          // 0x814
    DWORD filter;        // 0x818 fastcall (context, character), al 1: swallowed
    DWORD context;       // 0x81c
};
static_assert(offsetof(KeyTable, count) == 0x80c && offsetof(KeyTable, filter) == 0x818);
static bool keys_off() { return *glob<BYTE *>(KEYS_OFF) != 0; }

// 0x4e2720: the next typed character, 0 when none is queued. al only (the upper bytes the caller's).
static DWORD THISCALL Keys_ReadChar(KeyTable *t)
{
    if (!t->count)
        return 0;
    const BYTE c = (BYTE)t->chars[t->read];
    if (++t->read >= 10)
        t->read = 0;
    t->count--;
    return c;
}
// 0x4e2760: the character queue emptied (the messages pumped first unless no_pump). eax: no_pump, or
// what the pump returned.
static DWORD THISCALL Keys_ClearChars(KeyTable *t, DWORD no_pump)
{
    DWORD r = no_pump;
    if (!no_pump)
        r = pump_messages();
    t->count = 0;
    t->write = 0;
    t->read = 0;
    return r;
}
// 0x4e2980: every key's pressed bit cleared (after pumping unless no_pump). eax: the end of the table.
static DWORD THISCALL Keys_ClearPresses(KeyTable *t, DWORD no_pump)
{
    if (!no_pump)
        pump_messages();
    for (DWORD i = 1; i <= NUM_KEYS; i++)
        t->keys[i] &= ~2u;
    return ptrv(t) + 0x800;
}
// 0x4e29b0: every key's down bit cleared. eax: the end of the table.
static DWORD THISCALL Keys_ClearDowns(KeyTable *t, DWORD no_pump)
{
    if (!no_pump)
        pump_messages();
    for (DWORD i = 1; i <= NUM_KEYS; i++)
        t->keys[i] &= ~1u;
    return ptrv(t) + 0x800;
}
// 0x4e27a0: presses, downs and the queue cleared, the messages pumped before each step unless no_pump.
// eax: the end of the table, or what the last pump returned.
static DWORD THISCALL Keys_ClearAll(KeyTable *t, DWORD no_pump)
{
    if (!no_pump)
        pump_messages();
    for (DWORD i = 1; i <= NUM_KEYS; i++)
        t->keys[i] &= ~2u;
    if (!no_pump)
        pump_messages();
    for (DWORD i = 1; i <= NUM_KEYS; i++)
        t->keys[i] &= ~1u;
    DWORD r = ptrv(t) + 0x800;
    if (!no_pump)
        r = pump_messages();
    t->count = 0;
    t->write = 0;
    t->read = 0;
    return r;
}
// 0x4e2820: the key was pressed or is down: its pressed bit taken. al: 1 then, 0 otherwise (also while
// keys are off).
static DWORD THISCALL Keys_TakePress(KeyTable *t, DWORD k)
{
    if (keys_off())
        return 0;
    const DWORD e = t->keys[k];
    if (!(e & 3))
        return e & ~0xffu;
    t->keys[k] = e & ~2u;
    return ((e & ~2u) & ~0xffu) | 1;
}
// 0x4e2850: the key is down (key 0 always). al 1/0 with the key code's upper bytes.
static DWORD THISCALL Keys_IsDown(KeyTable *t, DWORD k)
{
    if (k && !(t->keys[k] & 1))
        return k & ~0xffu;
    return (k & ~0xffu) | 1;
}
// 0x4e2870: the first key pressed or down (its pressed bit taken), 0 when none (or keys are off).
static DWORD THISCALL Keys_TakeAny(KeyTable *t)
{
    const bool off = keys_off();
    for (DWORD i = 1; i <= NUM_KEYS; i++)
        if (!off && (t->keys[i] & 3)) {
            t->keys[i] &= ~2u;
            return i;
        }
    return 0;
}
// 0x4e2900: the first key pressed since it was last taken (its pressed bit taken), 0 when none.
static DWORD THISCALL Keys_TakeAnyPressed(KeyTable *t)
{
    const bool off = keys_off();
    for (DWORD i = 1; i <= NUM_KEYS; i++)
        if (!off && (t->keys[i] & 2)) {
            t->keys[i] &= ~2u;
            return i;
        }
    return 0;
}
// 0x4e2940: every pressed key up to the first one other than `except` gets its pressed bit taken; that
// key's code, 0 when none.
static DWORD THISCALL Keys_TakeOtherPressed(KeyTable *t, DWORD except)
{
    for (DWORD i = 1; i <= NUM_KEYS; i++) {
        if (keys_off())
            continue;
        const DWORD e = t->keys[i];
        if (!(e & 2))
            continue;
        t->keys[i] = e & ~2u;
        if (i != except)
            return i;
    }
    return 0;
}
// 0x4e28b0: the key was pressed while each given modifier (0: none) is down: its pressed bit taken.
// al 1, else 0.
static DWORD THISCALL Keys_TakeCombo(KeyTable *t, DWORD k, DWORD m1, DWORD m2, DWORD m3)
{
    if (keys_off())
        return 0;
    const DWORD e = t->keys[k];
    if (!(e & 2))
        return e & ~0xffu;
    if ((m1 && !(t->keys[m1] & 1)) || (m2 && !(t->keys[m2] & 1)) || (m3 && !(t->keys[m3] & 1)))
        return e & ~0xffu;
    t->keys[k] = e & ~2u;
    return ((e & ~2u) & ~0xffu) | 1;
}
// 0x4e29e0: the key goes down (down and pressed). eax: the code.
static DWORD THISCALL Keys_Press(KeyTable *t, DWORD k)
{
    t->keys[k] |= 3;
    return k;
}
// 0x4e29f0: the key goes up. eax: the code.
static DWORD THISCALL Keys_Release(KeyTable *t, DWORD k)
{
    t->keys[k] &= ~1u;
    return k;
}
// 0x4e2a00: a typed character into the queue unless it is 0 or the filter swallows it; a full queue
// (10) drops its oldest character.
static void THISCALL Keys_AddChar(KeyTable *t, DWORD c)
{
    if (!c)
        return;
    if (t->filter && (fn_dyn<DWORD(__fastcall *)(DWORD, DWORD)>(t->filter)(t->context, c) & 0xff))
        return;
    t->chars[t->write] = (char)c;
    if (++t->write >= 10)
        t->write = 0;
    if (++t->count >= 10) {
        t->count--;
        if (++t->read >= 10)
            t->read = 0;
    }
}

// ---- Key names, the keyboard repeat ---------------------------------------------------------------------

constexpr DWORD KEY_NAME_BUFFER = 0x6c1974;   // 0x100 bytes
// 0x4e2ab0: GetKeyNameTextA for the scan code into the buffer 0x6c1974; the buffer, 0 when it failed
static const char *__stdcall KeyNameText(DWORD scan)
{
    const int n = imp<int(STDCALL *)(int, char *, int)>(IMP_KEY_NAME_TEXT)((int)(scan << 16), glob<char *>(KEY_NAME_BUFFER), 0xff);
    return n ? glob<const char *>(KEY_NAME_BUFFER) : nullptr;
}
// 0x4e2a90: the key's name, or the text of the string 0x6c1048 when Windows has none
static const char *__stdcall KeyName(DWORD scan)
{
    if (const char *s = fn<const char *(__stdcall *)(DWORD)>(0x4e2ab0)(scan))
        return s;
    return zs_c_str(glob<void *>(0x6c1048));
}
// 0x4e2ae0: the keyboard delay in seconds (SPI_GETKEYBOARDDELAY: v · 1/3 · 0.75 + 0.25)
static long double __cdecl KeyboardDelay()
{
    DWORD v = 0;
    imp<BOOL(STDCALL *)(DWORD, DWORD, void *, DWORD)>(IMP_SYSTEM_PARAMETERS)(0x16, 0, &v, 0);
    return (long double)(unsigned long long)v * (long double)cf(glob_addr(0x5f88c8)) * (long double)cf(glob_addr(0x5f14e8)) + (long double)cf(glob_addr(0x5f05b8));
}
// 0x4e2b30: the keyboard repeat interval in seconds (SPI_GETKEYBOARDSPEED: (31 - v) · 1/31 · 0.3667 + 0.0333)
static long double __cdecl KeyboardRepeat()
{
    DWORD v = 0;
    imp<BOOL(STDCALL *)(DWORD, DWORD, void *, DWORD)>(IMP_SYSTEM_PARAMETERS)(10, 0, &v, 0);
    return (long double)(unsigned long long)(DWORD)(0x1f - v) * (long double)cf(glob_addr(0x5f4920)) * (long double)cf(glob_addr(0x5f88d0)) +
           (long double)cf(glob_addr(0x5f88cc));
}

// ---- The joystick ------------------------------------------------------------------------------------------

constexpr DWORD JOY_POS = 0x6c0eb0;   // the JOYINFOEX of the poll (0x34 bytes)
constexpr DWORD F_ONE = 0x5ec3f4, F_ZERO = 0x5ebed0, D_MOVE = 0x5f86c8;   // 1.0f, 0.0f, 0.35 (double)
struct JoyCaps {   // JOYCAPSA (0x194 bytes)
    WORD mid, pid;
    char name[32];
    DWORD xmin, xmax, ymin, ymax, zmin, zmax, buttons, period_min, period_max, rmin, rmax, umin, umax, vmin, vmax, caps, max_axes,
        num_axes, max_buttons;
    char reg_key[32];
    char oem_vxd[260];
};
static_assert(sizeof(JoyCaps) == 0x194 && offsetof(JoyCaps, rmin) == 0x48 && offsetof(JoyCaps, reg_key) == 0x70);
struct JoyInfo {   // JOYINFOEX (0x34 bytes)
    DWORD size, flags, x, y, z, r, u, v, buttons, button_number, pov, reserved1, reserved2;
};

// 0x4de0e0 (fastcall: device id, the name string): the OEM name of the device from the registry
// ("…\MediaResources\Joystick\<reg key>\CurrentJoystickSettings", value "Joystick<id + 1>OEMName", then
// "…\PrivateProperties\Joystick\OEM\<that>" value "OEMName") into the string (emptied when the first key
// has no such value). eax: 0, or 0xa6 when anything is missing.
static DWORD __fastcall Joy_OemName(DWORD id, void *name)
{
    if (id + 1 > imp<JoyGetNumDevs>(IMP_JOY_NUM_DEVS)())
        return NO_JOY_NAME;
    JoyCaps caps;
    imp<JoyGetDevCaps>(IMP_JOY_DEV_CAPS)(id, &caps, 0x194);
    char path[0x100], value[0x100], oem[0x100];
    crt_sprintf(path, glob<const char *>(0x61726c) /* "%s\\%s\\%s" */, glob<const char *>(0x617278), caps.reg_key, glob<const char *>(0x6172b4));
    DWORD key, size;
    if (imp<RegOpenKeyEx>(IMP_REG_OPEN)(HKLM, path, 0, KEY_ALL, &key))
        return NO_JOY_NAME;
    size = 0x100;
    crt_sprintf(value, glob<const char *>(0x617254) /* "Joystick%d%s" */, id + 1, glob<const char *>(0x617264) /* "OEMName" */);
    const int q = imp<RegQueryValueEx>(IMP_REG_QUERY)(key, value, nullptr, nullptr, oem, &size);
    imp<RegCloseKey>(IMP_REG_CLOSE)(key);
    if (q) {
        zs_set_text(name, glob<const char *>(0x62e558));
        return NO_JOY_NAME;
    }
    crt_sprintf(path, glob<const char *>(0x617314) /* "%s\\%s" */, glob<const char *>(0x617204), oem);
    if (imp<RegOpenKeyEx>(IMP_REG_OPEN)(HKLM, path, 0, KEY_ALL, &key))
        return NO_JOY_NAME;
    size = 0x100;
    char *buf = zs_get_buffer(name, 0x100);
    const int q2 = imp<RegQueryValueEx>(IMP_REG_QUERY)(key, glob<const char *>(0x617264), nullptr, nullptr, buf, &size);
    zs_release_buffer(name, -1);
    imp<RegCloseKey>(IMP_REG_CLOSE)(key);
    return q2 ? NO_JOY_NAME : 0;
}
// 0x4dd930 (fastcall like 0x4de0e0): the device has an OEM name. eax 1/0.
static DWORD __fastcall Joy_HasOemName(DWORD id, void *name) { return fn<DWORD(__fastcall *)(DWORD, void *)>(0x4de0e0)(id, name) == 0 ? 1 : 0; }
// 0x4dd940 (the device id in ecx): the device answers (caps and a position). al 1, else 0 with the error.
static DWORD __fastcall Joy_Answers(DWORD id)
{
    JoyCaps caps;
    JoyInfo info;
    info.size = 0x34;
    info.flags = 0xff;
    const DWORD e = imp<JoyGetDevCaps>(IMP_JOY_DEV_CAPS)(id, &caps, 0x194);
    if (e)
        return e & ~0xffu;
    const DWORD p = imp<JoyGetPosEx>(IMP_JOY_POS_EX)(id, &info);
    return p ? p & ~0xffu : 1;
}

// 0x4dda00 (device id): the name "No Joystick", then with a device id within joyGetNumDevs its caps and a
// first position: the calibration (+0x24..+0x40), the OEM name ("Unknown Joystick" when it is blank),
// connected. eax 1; 0 (and not connected, unless the id is out of range) otherwise.
static DWORD THISCALL Joy_Init(BYTE *j, DWORD id)
{
    JoyInfo info;
    info.size = 0x34;
    info.flags = 0xff;
    dw(j, 0x4c) = 0;
    zs_set_text(j + 0x48, glob<const char *>(0x6171d0) /* "No Joystick" */);
    if (imp<JoyGetNumDevs>(IMP_JOY_NUM_DEVS)() < id)
        return 0;
    dw(j, 0) = id;
    JoyCaps caps;
    if (imp<JoyGetDevCaps>(IMP_JOY_DEV_CAPS)(id, &caps, 0x194) || imp<JoyGetPosEx>(IMP_JOY_POS_EX)(dw(j, 0), &info)) {
        by(j, 0xfc) = 0;
        return 0;
    }
    const long double one = (long double)cf(glob_addr(F_ONE));
    const long double fx = one / (long double)(unsigned long long)(DWORD)(caps.xmax - caps.xmin);
    dw(j, 0x24) = (DWORD)-(int)caps.xmin;
    dw(j, 0x28) = (DWORD)-(int)caps.ymin;
    dw(j, 0x2c) = (DWORD)-(int)caps.zmin;
    fl(j, 0x34) = (float)(fx + fx);
    const long double fy = one / (long double)(unsigned long long)(DWORD)(caps.ymax - caps.ymin);
    dw(j, 0x30) = (DWORD)-(int)caps.rmin;
    fl(j, 0x38) = (float)(fy + fy);
    fl(j, 0x3c) = (float)(one / (long double)(unsigned long long)(DWORD)(caps.zmax - caps.rmin));
    fl(j, 0x40) = (float)(one / (long double)(unsigned long long)(DWORD)(caps.rmax - caps.rmin));
    fn<DWORD(__fastcall *)(DWORD, void *)>(0x4de0e0)(dw(j, 0), j + 0x48);
    zs_trim_right(j + 0x48);
    zs_trim_left(j + 0x48);
    if (text_is_blank(zs_c_str(j + 0x48)))
        zs_set_text(j + 0x48, glob<const char *>(0x6171bc) /* "Unknown Joystick" */);
    by(j, 0xfc) = 1;
    return 1;
}
// 0x4dd9a0 (device id): the name string, the dead zones 0.2 and scales 1.25, Init, +0x7c..+0xfb cleared.
// eax: this.
static BYTE *THISCALL Joy_Ctor(BYTE *j, DWORD id)
{
    zs_ctor(j + 0x48);
    dw(j, 4) = 0x3e4ccccd;
    dw(j, 0xc) = 0x3e4ccccd;
    by(j, 0xfc) = 0;
    by(j, 0x5c) = 0;
    by(j, 0x74) = 0;
    by(j, 0x68) = 0;
    dw(j, 0x58) = 0;
    dw(j, 8) = 0x3fa00000;
    dw(j, 0x10) = 0x3fa00000;
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4dda00)(j, id);
    for (DWORD k = 0; k < 0x80; k += 4)
        dw(j, 0x7c + k) = 0;
    return j;
}
// One axis: below -dead it becomes (v + dead) · scale, above dead (v - dead) · scale, else it stays 0
// (NaN counts as below)
static void axis(BYTE *j, long double v, DWORD dead, DWORD scale, DWORD out)
{
    const long double d = (long double)fl(j, dead);
    if (!(v >= -d))
        fl(j, out) = (float)((v + d) * (long double)fl(j, scale));
    else if (v > d)
        fl(j, out) = (float)((v - d) * (long double)fl(j, scale));
}
// 0x4ddb80: the poll (joyGetPosEx into 0x6c0eb0): the axes, the directions of x and y, the buttons and
// the hat (POV in hundredths of a degree; up below 75° or above 285°, right from 15° to 165°, down from
// 105° to 255°, left from 195° to 345°); the result keeps only the bits that were released since they
// last counted (+0x4c). eax: the bits.
static DWORD THISCALL Joy_Poll(BYTE *j)
{
    dw(j, 0x44) = 0;
    dw(j, 0x14) = 0;
    dw(j, 0x18) = 0;
    if (by(j, 0xfc)) {
        JoyInfo &info = *glob<JoyInfo *>(JOY_POS);
        info.size = 0x34;
        info.flags = 0xff;
        if (imp<JoyGetPosEx>(IMP_JOY_POS_EX)(dw(j, 0), &info) == 0) {
            const long double one = (long double)cf(glob_addr(F_ONE));
            const long double fx = (long double)(unsigned long long)(DWORD)(dw(j, 0x24) + info.x) * (long double)fl(j, 0x34) - one;
            const long double fy = (long double)(unsigned long long)(DWORD)(dw(j, 0x28) + info.y) * (long double)fl(j, 0x38) - one;
            fl(j, 0x1c) = (float)((long double)(unsigned long long)(DWORD)(dw(j, 0x2c) + info.z) * (long double)fl(j, 0x3c));
            fl(j, 0x20) = (float)((long double)(unsigned long long)(DWORD)(dw(j, 0x30) + info.r) * (long double)fl(j, 0x40));
            axis(j, fy, 0xc, 0x10, 0x18);
            axis(j, fx, 4, 8, 0x14);
            if (dw(j, 0x58) == 0) {
                dw(j, 0x50) = dw(j, 0x14);
                dw(j, 0x54) = dw(j, 0x18);
                dw(j, 0x58) = ticks() - 30000;
            }
            const long double move = (long double)*glob<double *>(D_MOVE);
            const long double dx = __builtin_fabsl((long double)fl(j, 0x14) - (long double)fl(j, 0x50));
            const long double dy = __builtin_fabsl((long double)fl(j, 0x18) - (long double)fl(j, 0x54));
            if (dx > move || dy > move) {
                dw(j, 0x50) = dw(j, 0x14);
                dw(j, 0x54) = dw(j, 0x18);
                dw(j, 0x58) = ticks();
            }
            if (ticks() - dw(j, 0x58) > 30000) {
                dw(j, 0x14) = 0;
                dw(j, 0x18) = 0;
            }
            const long double zero = (long double)cf(glob_addr(F_ZERO));
            const long double y = (long double)fl(j, 0x18), x = (long double)fl(j, 0x14);
            if (!(y >= zero))
                dw(j, 0x44) |= 0x40000000;
            else if (y > zero)
                dw(j, 0x44) |= 0x80000000;
            if (!(x >= zero))
                dw(j, 0x44) |= 0x10000000;
            else if (x > zero)
                dw(j, 0x44) |= 0x20000000;
            dw(j, 0x44) |= info.buttons & 0xffffff;
            const DWORD pov = info.pov;
            if (pov != 0xffff) {
                if ((pov > 0x6f54 && pov <= 0x8ca0) || pov < 0x1d4c)
                    dw(j, 0x44) |= 0x4000000;
                if (pov > 0x5dc && pov < 0x4074)
                    dw(j, 0x44) |= 0x2000000;
                if (pov > 0x2904 && pov < 0x639c)
                    dw(j, 0x44) |= 0x8000000;
                if (pov > 0x4c2c && pov < 0x86c4)
                    dw(j, 0x44) |= 0x1000000;
            }
        }
    }
    const DWORD released = dw(j, 0x4c) | ~dw(j, 0x44);
    dw(j, 0x4c) = released;
    dw(j, 0x44) &= released;
    return dw(j, 0x44);
}
// 0x4dde40 (the device's name: c_str of +0x48) is a 3-byte add and a jump, not hookable.

// 0x4dde50 (bit, string): the name of a bit of the poll: the eight directions by name, a button as
// "Button <n>" and, when the registry has one ("…\PrivateProperties\Joystick\OEM\<OEM name>\Buttons\<bit>",
// its default value), the device's own name for it.
static void THISCALL Joy_BitName(BYTE *j, DWORD bit, void *out)
{
    DWORD text = 0;
    switch (1u << (bit & 31)) {
    case 0x1000000: text = glob_addr(0x6172f4); break;   // "Hat Left"
    case 0x2000000: text = glob_addr(0x617300); break;   // "Hat Right"
    case 0x4000000: text = glob_addr(0x6172ec); break;   // "Hat Up"
    case 0x8000000: text = glob_addr(0x6172e0); break;   // "Hat Down"
    case 0x10000000: text = glob_addr(0x6172d8); break;  // "Left"
    case 0x20000000: text = glob_addr(0x6171dc); break;  // "Right"
    case 0x40000000: text = glob_addr(0x6171e4); break;  // "Up"
    case 0x80000000: text = glob_addr(0x6171e8); break;  // "Down"
    }
    if (text) {
        zs_set_text(out, at<const char *>(text));
        return;
    }
    const DWORD id = dw(j, 0);
    zs_format(out, glob<const char *>(0x6172cc) /* "Button %d" */, bit + 1);
    if (id + 1 > imp<JoyGetNumDevs>(IMP_JOY_NUM_DEVS)())
        return;
    JoyCaps caps;
    imp<JoyGetDevCaps>(IMP_JOY_DEV_CAPS)(id, &caps, 0x194);
    char path[0x100], value[0x100], oem[0x100];
    crt_sprintf(path, glob<const char *>(0x61726c), glob<const char *>(0x617278), caps.reg_key, glob<const char *>(0x6172b4));
    DWORD key, size;
    if (imp<RegOpenKeyEx>(IMP_REG_OPEN)(HKLM, path, 0, KEY_ALL, &key))
        return;
    size = 0x100;
    crt_sprintf(value, glob<const char *>(0x617254), id + 1, glob<const char *>(0x617264));
    const int q = imp<RegQueryValueEx>(IMP_REG_QUERY)(key, value, nullptr, nullptr, oem, &size);
    imp<RegCloseKey>(IMP_REG_CLOSE)(key);
    if (q)
        return;
    crt_sprintf(path, glob<const char *>(0x6171f0) /* "%s\\%s\\Buttons\\%d" */, glob<const char *>(0x617204), oem, bit);
    if (imp<RegOpenKeyEx>(IMP_REG_OPEN)(HKLM, path, 0, KEY_ALL, &key))
        return;
    size = 0x100;
    char *buf = zs_get_buffer(out, 0x100);
    imp<RegQueryValueEx>(IMP_REG_QUERY)(key, glob<const char *>(0x62e558), nullptr, nullptr, buf, &size);
    zs_release_buffer(out, -1);
    imp<RegCloseKey>(IMP_REG_CLOSE)(key);
}
// 0x4de080 (bits, string): the string emptied, then for every set bit "Joy " (the first) or "+" appended
// and the bit's name written over it (0x4dde50 sets the whole string), so the last bit's name remains.
static void THISCALL Joy_BitsText(BYTE *j, DWORD bits, void *out)
{
    zs_set_text(out, glob<const char *>(0x62e558));
    bool any = false;
    for (DWORD i = 0; i < 32; i++)
        if (bits & (1u << i)) {
            zs_append_format(out, any ? glob<const char *>(0x60fca0 /* "+" */) : glob<const char *>(0x61730c /* "Joy " */));
            fn<void(THISCALL *)(void *, DWORD, void *)>(0x4dde50)(j, i, out);
            any = true;
        }
}

ZAX_HOOKS(input)
{
    const Hook list[] = {
#include "input_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
