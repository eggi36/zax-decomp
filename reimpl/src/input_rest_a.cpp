// The rest of the control configuration 0x470030-0x470e60 (phase 3 remains, band A; docs/DECOMPILATION.md
// section 8, "Phase 3 remains, band A"; the object and the rest of its functions: control_config.cpp, whose
// header has the layout).
//
//   0x470030  (vector): +0x50/+0x54 set.
//   0x470050  the frame (mouse vector wanted, no mouse buttons, packet): with +0x1c4 set. The time +0x58
//             from timeGetTime when +0x1c5 is set. With a packet (or -1): the actions pressed this frame
//             are gathered into the static bit set 0x6bd548 (made on first use from the bit count +4):
//             a binding's key newly pressed while its second key is held (and its "shared" key +0x3c not
//             held), its joystick bits all down and none of +0x34, its mouse bits all down (the wheel as
//             0x08/0x10) and none of +0x38; while the game is paused (the reporter's slot 30) only the
//             bindings with +0x20 count. Nothing at all while a key is being captured (+0x75); a pending
//             joystick check (+0x74) runs first. With -1 the set becomes +0x0c directly; otherwise +0x0c
//             takes it word by word, and the static copy 0x6bd558 with the vector 0x6bd538 goes to the
//             packet through the client control block reader (0x4f7a70, with the paused byte 0x6bd57c and
//             whether the copy changed). Then +0x00 takes +0x0c, +0x5c/+0x60 take +0x50/+0x54, the static
//             set 0x6bd570 keeps last frame's +0x64 and +0x64 takes +0x00. Last the repeat: an action held
//             since last frame stays on for kind 0 only every +0x14 ms (+0x30 the next time), kind 1 is a
//             one-shot, kind 2 stays; a new press starts the timers unless the binding's +0x1c is still in
//             the future (kind 2 ignores that). +0x1c5 set.
//   0x4708c0  the analogue vector +0x5c/+0x60: the mouse's offset from (320, 240) times 0.005 (with
//             +0x4c), the joystick's axes when one leaves the dead zone (0.15, with +0x4d; +0x4e records it
//             and a return into the dead zone clears the vector once), normalised beyond length 1 (+0x4f)
//             and clamped to [-1, 1]; a changed vector moves the mouse back to (x · 200 + 320, y · 200 +
//             240) (fistp, the FPU's rounding).
//   0x470ac0  the joystick check (as 0x46ffa0): every 2 s the joystick re-initialised; a changed name
//             writes and reads the INI (al 1).
//   0x470b50  the aiming angle: Table_Atan2 0x509100 of (+0x60, +0x5c) (st0).
//   0x470b60  (binding, out ZString): the binding as text ("<second>+<key>, ", the joystick bits'
//             text, the mouse buttons ("Mouse Button %d", the wheel's two names) joined with the separator
//             0x60fca0), the last two characters cut.
//   0x470ce0, 0x470d00  (binding): its INI key's two words (+0x24, +0x28).
//   0x470d20  (joystick id): the joystick replaced (-1: none, the name from 0x6bd568), "[Player %d
//             ]Controler ID Number" written as id + 1 (Ini_WriteInt on 0x6bd330 / 0x6bd430).
//   0x470e50  the joystick's id or -1.
//   0x470e60  (thiscall on a pointer array: &value, hint): the index of the value (the hint first), -1.
//
// The function-local statics are the exe's (guard byte 0x6bd564, bits 1, 2, 4, 8; atexit as the
// original). Return values: as the original where it defines them; void otherwise [assumption].
#include "class_info.h"
#include "ai_common.h"
#include "x87_exact.h"


namespace zax {

using namespace ai;

namespace {

// ---- Functions and statics of the exe -----------------------------------------------------------------------

DWORD words(DWORD bits) { return ((bits & 0x1f) ? 1 : 0) + (bits >> 5); }
DWORD now_ms() { return fn<DWORD(__stdcall *)()>(0x4d2460)(); }
// The key table and the mouse as functions, not as `BYTE *const` globals: such a global's initializer folds to a
// constant only while glob<>() inlines to nothing; the coverage build's instrumented glob<>() left it as a dynamic
// initializer, which the DLL (no C runtime) never runs, and the mouse stayed null (2026-10-31)
BYTE *keys_obj() { return glob<BYTE *>(0x6c1050); }
BYTE *mouse_obj() { return glob<BYTE *>(0x6cff40); }
bool key_down(DWORD k) { return fn<BYTE(THISCALL *)(void *, DWORD)>(0x4e2850)(keys_obj(), k) & 0xff; }
bool key_take(DWORD k) { return fn<BYTE(THISCALL *)(void *, DWORD)>(0x4e2820)(keys_obj(), k) & 0xff; }
bool paused() { return vcall<BYTE>(*glob<void **>(0x6c02cc), 30) & 0xff; }
void write_ini(BYTE *self) { fn<void(THISCALL *)(void *, const char *)>(0x46fb60)(self, nullptr); }
void read_ini(BYTE *self) { fn<void(THISCALL *)(void *, const char *)>(0x46fcc0)(self, nullptr); }

// The static bit sets {words, bit count, changed, received} and their guard
BYTE &guard() { return *glob<BYTE *>(0x6bd564); }
DWORD *&set_words(DWORD set) { return *at<DWORD **>(set); }
DWORD &set_bits(DWORD set) { return *at<DWORD *>(set + 4); }
BYTE &set_changed(DWORD set) { return *at<BYTE *>(set + 8); }
constexpr DWORD PRESSED = 0x6bd548, SENT = 0x6bd558, HELD = 0x6bd570, VECTOR = 0x6bd538, SENT_PAUSED = 0x6bd57c;

void set_bit(DWORD set, DWORD i)
{
    DWORD *w = set_words(set);
    const DWORD bit = 1u << (i & 31);
    if (!(w[i >> 5] & bit)) {
        w[i >> 5] |= bit;
        set_changed(set) = 1;
    }
}
// A new static set of `bits` bits (operator new), its words cleared ("changed" when one was not 0)
void make_static_set(DWORD set, DWORD bits)
{
    set_bits(set) = bits;
    DWORD *w = static_cast<DWORD *>(heap_alloc(words(bits) * 4));
    set_words(set) = w;
    for (DWORD i = 0; i < words(set_bits(set)); i++)
        if (set_words(set)[i]) {
            set_words(set)[i] = 0;
            set_changed(set) = 1;
        }
}
// A ZString's text compared like strcmp's sign (0 when equal)
int compare(const char *a, const char *b)
{
    for (;; a++, b++) {
        if (*a != *b)
            return (BYTE)*a < (BYTE)*b ? -1 : 1;
        if (!*a)
            return 0;
    }
}

// ---- The functions -----------------------------------------------------------------------------------------

// 0x470030
void THISCALL Config_SetVector(BYTE *self, const DWORD *v)
{
    dw(self, 0x50) = v[0];
    dw(self, 0x54) = v[1];
}

// 0x4708c0
void THISCALL Config_AnalogueVector(BYTE *self)
{
    if (self[0x4c]) {
        int x, y;
        fn<void(THISCALL *)(void *, int *, int *)>(0x5184e0)(mouse_obj(), &x, &y);
        x -= 320;
        y -= 240;
        fl<float>(self, 0x5c) = fstp(m_mul(fild(x), *glob<const float *>(0x5effac)));
        fl<float>(self, 0x60) = fstp(m_mul(fild(y), *glob<const float *>(0x5effac)));
    }
    if (self[0x4d]) {
        BYTE *j = bp(dw(self, 0x70));
        if (j) {
            const float jx = fl<float>(j, 0x14);
            const DWORD jy = dw(j, 0x18);
            BYTE out;
            if (!c03(fld(jx), cst(glob_addr(0x5effa8))) || c0(fld(jx), cst(glob_addr(0x5effa4))) || !c03(fld(raw_float(jy)), cst(glob_addr(0x5effa8))) ||
                c0(fld(raw_float(jy)), cst(glob_addr(0x5effa4)))) {
                out = 1;
                fl<float>(self, 0x5c) = jx;
                dw(self, 0x60) = jy;
            } else {
                out = 0;
                if (self[0x4e]) {
                    dw(self, 0x5c) = 0;
                    dw(self, 0x60) = 0;
                }
            }
            self[0x4e] = out;
        }
    }
    bool changed = false;
    if (self[0x4f]) {
        const long double y = fld(fl<float>(self, 0x60)), x = fld(fl<float>(self, 0x5c));
        if (!c03(p_add(x * x, y * y), cst(glob_addr(C_ONE)))) {
            const long double y2 = fld(fl<float>(self, 0x60)), x2 = fld(fl<float>(self, 0x5c));
            const long double len = fsqrt(p_add(x2 * x2, y2 * y2));
            if (!c3(len, cst(glob_addr(C_ZERO)))) {
                const long double inv = m_divr(len, *glob<const float *>(C_ONE));
                fl<float>(self, 0x5c) = fstp(m_mul(inv, fl<float>(self, 0x5c)));
                fl<float>(self, 0x60) = fstp(m_mul(inv, fl<float>(self, 0x60)));
            }
            changed = true;
        }
    }
    if (!c03(fld(fl<float>(self, 0x5c)), cst(glob_addr(C_ONE)))) {
        dw(self, 0x5c) = 0x3f800000;
        changed = true;
    }
    if (c0(fld(fl<float>(self, 0x5c)), cst(glob_addr(0x5eeccc)))) {
        dw(self, 0x5c) = 0xbf800000;
        changed = true;
    }
    if (!c03(fld(fl<float>(self, 0x60)), cst(glob_addr(C_ONE)))) {
        dw(self, 0x60) = 0x3f800000;
        changed = true;
    }
    if (c0(fld(fl<float>(self, 0x60)), cst(glob_addr(0x5eeccc)))) {
        dw(self, 0x60) = 0xbf800000;
        changed = true;
    }
    if (!changed || !self[0x4c])
        return;
    const float *k = glob<const float *>(0x5eff98);   // 320, 240, 200
    const int my = x87_fistp(fld(fstp(m_add(m_mul(fld(fl<float>(self, 0x60)), k[2]), k[1]))));
    const int mx = x87_fistp(fld(fstp(m_add(m_mul(fld(fl<float>(self, 0x5c)), k[2]), k[0]))));
    fn<void(THISCALL *)(void *, int, int)>(0x518570)(mouse_obj(), mx, my);
}

// The joystick re-initialised; true when its name changed (the INI written and read again)
bool recheck_joystick(BYTE *self)
{
    BYTE *j = bp(dw(self, 0x70));
    if (!j)
        return false;
    fn<void(THISCALL *)(void *, DWORD)>(0x4dda00)(j, dw(j, 0));
    const char *name = fn<const char *(THISCALL *)(void *)>(0x4dde40)(bp(dw(self, 0x70)));
    if (!compare(reinterpret_cast<const char *>(self + 0xb8), name))
        return false;
    write_ini(self);
    read_ini(self);
    return true;
}

// 0x470050
void THISCALL Config_Frame(BYTE *self, DWORD want_vector_arg, DWORD no_mouse_arg, DWORD packet)
{
    const BYTE want_vector = (BYTE)want_vector_arg, no_mouse_buttons = (BYTE)no_mouse_arg;
    // the original keeps the bindings loop's offset in the argument slot of no_mouse_buttons, and later
    // passes that dword (its low byte replaced) to the network block
    DWORD slot = no_mouse_arg;
    if (!self[0x1c4])
        return;
    if (self[0x1c5])
        dw(self, 0x58) = now_ms();
    if (packet) {
        if (!(guard() & 1)) {
            guard() |= 1;
            set_bits(glob_addr(PRESSED)) = dw(self, 4);
            set_words(glob_addr(PRESSED)) = static_cast<DWORD *>(heap_alloc(words(dw(self, 4)) * 4));
            fn<DWORD(THISCALL *)(void *)>(0x46ee00)(glob<void *>(PRESSED));
            *at<BYTE *>(glob_addr(PRESSED) + 8) = 0;
            *at<BYTE *>(glob_addr(PRESSED) + 9) = 0;
            fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x4708a0));
        }
        DWORD vec[2] = {0, 0};
        if (self[0x75])
            return;
        if (self[0x74]) {
            self[0x74] = 0;
            dw(self, 0x1b8) = now_ms() + 2000;
            recheck_joystick(self);
        }
        for (DWORD i = 0; i < words(set_bits(glob_addr(PRESSED))); i++)
            if (set_words(glob_addr(PRESSED))[i]) {
                set_words(glob_addr(PRESSED))[i] = 0;
                set_changed(glob_addr(PRESSED)) = 1;
            }
        DWORD joy = 0;
        if (dw(self, 0x70))
            joy = fn<DWORD(THISCALL *)(void *)>(0x4ddb80)(bp(dw(self, 0x70)));
        DWORD mouse = 0;
        if (self[0x4c] && !no_mouse_buttons) {
            mouse = fn<DWORD(THISCALL *)(void *)>(0x51f450)(mouse_obj());
            const int wheel = fn<int(THISCALL *)(void *)>(0x518520)(mouse_obj());
            if (wheel < 0)
                mouse |= 8;
            if (wheel > 0)
                mouse |= 0x10;
        }
        if (want_vector && !paused()) {
            fn<void(THISCALL *)(void *)>(0x4708c0)(self);
            vec[0] = dw(self, 0x5c);
            vec[1] = dw(self, 0x60);
        }
        if (dw(self, 0x3c))
            slot = 0;
        for (DWORD i = 0; i < dw(self, 0x3c); i++, slot += 0x40) {
            BYTE *b = bp(dw(self, 0x40)) + i * 0x40;
            if (paused() && !b[0x20])
                continue;
            if (dw(b, 0) && (!dw(b, 0x3c) || !key_down(dw(b, 0x3c))) && key_take(dw(b, 0)) && key_down(dw(b, 4)))
                set_bit(glob_addr(PRESSED), i);
            const DWORD jb = dw(b, 8);
            if (jb && (jb & joy) == jb && !(dw(b, 0x34) & joy))
                set_bit(glob_addr(PRESSED), i);
            const DWORD mb = dw(b, 0xc);
            if (mb && (mb & mouse) == mb && !(dw(b, 0x38) & mouse))
                set_bit(glob_addr(PRESSED), i);
        }
        fn<void(THISCALL *)(void *, DWORD)>(0x4e2980)(keys_obj(), 1);
        if (packet == 0xffffffffu) {
            fn<void *(THISCALL *)(void *, void *)>(0x46eda0)(self + 0xc, glob<void *>(PRESSED));
            dw(self, 0x50) = vec[0];
            dw(self, 0x54) = vec[1];
        } else {
            dw(self, 0x50) = vec[0];
            dw(self, 0x54) = vec[1];
            for (DWORD i = 0; i < words(dw(self, 0x10)); i++) {
                DWORD *w = reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(self, 0xc))) + i;
                const DWORD v = set_words(glob_addr(PRESSED))[i];
                if (*w != v) {
                    *w = v;
                    self[0x14] = 1;
                }
            }
            if (!(guard() & 2)) {
                guard() |= 2;
                make_static_set(glob_addr(SENT), dw(self, 4));
                *at<BYTE *>(glob_addr(SENT) + 8) = 0;
                *at<BYTE *>(glob_addr(SENT) + 9) = 0;
                fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x470880));
            }
            if (!(guard() & 4)) {
                guard() |= 4;
                *glob<DWORD *>(VECTOR) = 0;
                *at<DWORD *>(glob_addr(VECTOR) + 4) = 0;
                fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x470870));
            }
            for (DWORD i = 0; i < words(set_bits(glob_addr(SENT))); i++) {
                const DWORD v = set_words(glob_addr(PRESSED))[i];
                if (set_words(glob_addr(SENT))[i] != v) {
                    set_words(glob_addr(SENT))[i] = v;
                    set_changed(glob_addr(SENT)) = 1;
                }
            }
            *at<DWORD *>(glob_addr(VECTOR) + 4) = vec[1];
            // the original passes the changed byte in the low byte of the argument slot of no_mouse_buttons
            const DWORD changed = (DWORD)set_changed(glob_addr(SENT)) | (slot & 0xffffff00u);
            set_changed(glob_addr(SENT)) = 0;
            *glob<DWORD *>(VECTOR) = vec[0];
            const DWORD p = vcall<DWORD>(*glob<void **>(0x6c02cc), 30);
            *glob<BYTE *>(SENT_PAUSED) = (BYTE)p;
            void *reader = fn<void *(__cdecl *)()>(0x4f79a0)();
            fn<void(THISCALL *)(void *, DWORD, void *, void *, DWORD, DWORD)>(0x4f7a70)(reader, packet, glob<void *>(SENT), glob<void *>(VECTOR), p,
                                                                                        changed);
        }
    }
    DWORD *w0 = reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(self, 0)));
    for (DWORD i = 0; i < words(dw(self, 4)); i++) {
        w0 = reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(self, 0)));
        const DWORD v = reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(self, 0xc)))[i];
        if (w0[i] != v) {
            w0[i] = v;
            self[8] = 1;
        }
    }
    self[0x15] = 0;
    dw(self, 0x5c) = dw(self, 0x50);
    dw(self, 0x60) = dw(self, 0x54);
    if (!(guard() & 8)) {
        guard() |= 8;
        make_static_set(glob_addr(HELD), dw(self, 0x68));
        *at<BYTE *>(glob_addr(HELD) + 8) = 0;
        *at<BYTE *>(glob_addr(HELD) + 9) = 0;
        fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x470850));
    }
    for (DWORD i = 0; i < words(set_bits(glob_addr(HELD))); i++) {
        const DWORD v = reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(self, 0x64)))[i];
        if (set_words(glob_addr(HELD))[i] != v) {
            set_words(glob_addr(HELD))[i] = v;
            set_changed(glob_addr(HELD)) = 1;
        }
    }
    for (DWORD i = 0; i < words(dw(self, 0x68)); i++) {
        DWORD *w = reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(self, 0x64))) + i;
        const DWORD v = reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(self, 0)))[i];
        if (*w != v) {
            *w = v;
            self[0x6c] = 1;
        }
    }
    for (DWORD i = 0; i < dw(self, 0x3c); i++) {
        const DWORD bit = 1u << (i & 31), word = i >> 5;
        DWORD *cur = reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(self, 0)));
        BYTE *b = bp(dw(self, 0x40)) + i * 0x40;
        if (!(cur[word] & bit)) {
            dw(b, 0x30) = 0;
            continue;
        }
        bool clear = false;
        if (set_words(glob_addr(HELD))[word] & bit) {
            const DWORD kind = dw(b, 0x10);
            if (kind == 0) {
                const DWORD next = dw(b, 0x30);
                if (!next || dw(self, 0x58) <= next) {
                    clear = true;
                } else {
                    if (!(cur[word] & bit)) {
                        cur[word] |= bit;
                        self[8] = 1;
                    }
                    BYTE *b2 = bp(dw(self, 0x40)) + i * 0x40;
                    dw(b2, 0x30) = dw(b2, 0x14) + dw(self, 0x58);
                }
            } else if (kind == 1) {
                clear = true;
            }
        } else {
            const DWORD now = dw(self, 0x58);
            if (dw(b, 0x1c) < now || dw(b, 0x10) == 2) {
                dw(b, 0x30) = dw(b, 0x14) + now;
                BYTE *b2 = bp(dw(self, 0x40)) + i * 0x40;
                dw(b2, 0x1c) = dw(b2, 0x18) + dw(self, 0x58);
            } else {
                clear = true;
            }
        }
        if (clear && (cur[word] & bit)) {
            cur[word] &= ~bit;
            self[8] = 1;
        }
    }
    self[0x1c5] = 1;
}

// 0x470ac0. al: the INI was read again.
BYTE THISCALL Config_CheckJoystick(BYTE *self)
{
    if (now_ms() < dw(self, 0x1b8))
        return 0;
    dw(self, 0x1b8) = now_ms() + 2000;
    return recheck_joystick(self) ? 1 : 0;
}

// 0x470b50
long double THISCALL Config_AimAngle(BYTE *self) { return fn<long double(__stdcall *)(DWORD, DWORD)>(0x509100)(dw(self, 0x60), dw(self, 0x5c)); }

// 0x470b60
void THISCALL Config_BindingText(BYTE *self, DWORD index, ZString *out)
{
    using Append = void(__cdecl *)(ZString *, const char *, ...);
    BYTE *b = bp(dw(self, 0x40)) + index * 0x40;
    fn<void(THISCALL *)(ZString *, const char *)>(0x4df010)(out, glob<const char *>(0x62e558));
    auto key_name = [](DWORD k) { return fn<const char *(THISCALL *)(void *, DWORD)>(0x4e2a90)(keys_obj(), k); };
    if (dw(b, 0)) {
        if (dw(b, 4))
            fn<Append>(0x4e0a30)(out, glob<const char *>(0x60fcac) /* "%s+" */, key_name(dw(b, 4)));
        fn<Append>(0x4e0a30)(out, glob<const char *>(0x60fca4) /* "%s, " */, key_name(dw(b, 0)));
    }
    if (dw(b, 8) && dw(self, 0x70)) {
        BYTE text[4];
        zs_ctor(text);
        fn<void(THISCALL *)(void *, DWORD, void *)>(0x4de080)(bp(dw(self, 0x70)), dw(b, 8), text);
        fn<Append>(0x4e0a30)(out, glob<const char *>(0x60fca4), zs_c_str(text));
        zs_dtor(text);
    }
    if (dw(b, 0xc) && self[0x4c]) {
        bool first = true;
        for (DWORD i = 0; i < 5; i++) {
            const DWORD bit = 1u << i;
            if (!(dw(b, 0xc) & bit))
                continue;
            if (!first)
                fn<Append>(0x4e0a30)(out, glob<const char *>(0x60fca0));
            if (bit == 8) {
                fn<void(THISCALL *)(ZString *, const void *)>(0x4e1ab0)(out, glob<void *>(0x6bd540));
            } else if (bit == 0x10) {
                fn<void(THISCALL *)(ZString *, const void *)>(0x4e1ab0)(out, glob<void *>(0x6bd544));
            } else {
                const DWORD n = i + 1;
                fn<Append>(0x4e0a30)(out, glob<const char *>(0x60fc98) /* "%s %d" */, zs_c_str(glob<void *>(0x6bd530)), n);
                first = false;
            }
        }
        fn<Append>(0x4e0a30)(out, glob<const char *>(0x60fc94) /* ", " */);
    }
    char *t = fn<char *(THISCALL *)(ZString *, DWORD)>(0x4e1590)(out, 0);
    if (*t)
        t[lstrlenA(t) - 2] = 0;
    fn<void(THISCALL *)(ZString *, DWORD)>(0x4e16d0)(out, 0xffffffffu);
}

// 0x470ce0, 0x470d00
DWORD THISCALL Config_IniWord1(BYTE *self, DWORD i) { return dw(bp(dw(self, 0x40)) + i * 0x40, 0x24); }
DWORD THISCALL Config_IniWord2(BYTE *self, DWORD i) { return dw(bp(dw(self, 0x40)) + i * 0x40, 0x28); }

// 0x470d20
void THISCALL Config_SetJoystick(BYTE *self, DWORD id)
{
    if (BYTE *j = bp(dw(self, 0x70))) {
        zs_dtor(j + 0x48);
        heap_free(j);
        dw(self, 0x70) = 0;
    }
    char *name = reinterpret_cast<char *>(self + 0xb8);
    const char *from;
    if (id != 0xffffffffu) {
        void *j = heap_alloc(0x100);
        j = j ? fn<void *(THISCALL *)(void *, DWORD)>(0x4dd9a0)(j, id) : nullptr;
        dw(self, 0x70) = (DWORD)(uintptr_t)j;
        from = fn<const char *(THISCALL *)(void *)>(0x4dde40)(j);
        memcpy(name, from, (DWORD)lstrlenA(from) + 1);
    } else {
        from = zs_c_str(glob<void *>(0x6bd568));
        memcpy(name, from, (DWORD)lstrlenA(from) + 1);
        dw(self, 0x70) = 0;
    }
    char key[0x100];
    key[0] = 0;
    if (const DWORD player = dw(self, 0x1c0))
        fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(key, glob<const char *>(0x60fc58) /* "Player %d " */, player + 1);
    const char *suffix = glob<const char *>(0x60fc44);   // "Controler ID Number"
    const DWORD at_ = (DWORD)lstrlenA(key);
    memcpy(key + at_, suffix, (DWORD)lstrlenA(suffix) + 1);
    fn<DWORD(__fastcall *)(void *, void *, const char *, DWORD)>(0x4a4690)(glob<void *>(0x6bd330), glob<void *>(0x6bd430), key, id + 1);
}

// 0x470e50
DWORD THISCALL Config_JoystickId(BYTE *self)
{
    BYTE *j = bp(dw(self, 0x70));
    return j ? dw(j, 0) : 0xffffffffu;
}

// 0x470e60 (on a pointer array {vtable, data, count, capacity})
DWORD THISCALL PtrArray_IndexOfHint(BYTE *self, const DWORD *value, DWORD hint)
{
    const DWORD n = dw(self, 8);
    const DWORD *data = reinterpret_cast<const DWORD *>(static_cast<uintptr_t>(dw(self, 4)));
    if (hint < n && data[hint] == *value)
        return hint;
    for (DWORD i = 0; i < n; i++)
        if (data[i] == *value)
            return i;
    return 0xffffffffu;
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(input_rest_a)
{
    const Hook list[] = {
#include "input_rest_a_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
