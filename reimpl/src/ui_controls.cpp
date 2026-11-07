// The controls configuration dialog (docs/DECOMPILATION.md 9, "The rest of the user interface"): one tab page per
// group of actions with a key button and a label of its bindings per action, the "Weapon AutoSwitch" check box,
// the joystick combo box, the buttons; the key button; the "save the changes?" question; the credits text that
// scrolls in the credits and the end game credits. Every call into the toolkit or the rest of the game goes
// through its exe address.
//
// Layouts [confirmed: the constructors, destructors and slots below]:
//   controls dialog (0x16c, vtable 0x5f010c, a CDialog): +0x134 the control configuration (0x46ef80-), +0x138
//     the key buttons (operator new, one per action), +0x13c their count, +0x140 the tab control, +0x144/+0x148/
//     +0x14c/+0x150 the buttons, +0x154 "Weapon AutoSwitch", +0x158 the joysticks listed, +0x15c their ids
//     (-1 none first), +0x160 their names (a FieldNameList), +0x164 the combo box, +0x168 the joystick chosen
//   key button (0x148, vtable 0x5effb0, a button 0x40f9b0): +0x138 the label of its bindings, +0x13c the
//     configuration, +0x140 the action, +0x144 waiting for a key (blinks)
//   question (0x140, vtable 0x5f02b0, a CDialog): +0x134 Yes, +0x138 No, +0x13c the dialog asking
//   credits text (0x20): +0 the start time, +4 the font, +8, +0xc the pixels a millisecond (the line height ×
//     0x5f08ac), +0x10 the line height + 2, +0x14 2, +0x18 the lines, +0x1c the text (1000 lines of 256 bytes)
#include "ui_screens.h"

namespace zax::ui {

namespace {

constexpr int SET_TITLE = 43, SET_VALUE = 77, CHECK = 78;
constexpr DWORD MASK = 0x6bcc68;

DWORD time_ms() { return fn<DWORD(__cdecl *)()>(0x4d2460)(); }
// A key button's label: its bindings (ControlConfig::BindingText 0x470b60), empty without a configuration
void binding_text(BYTE *button)
{
    if (!m(button, 0x13c)) {
        vc(bp(m(button, 0x138)), SET_VALUE, glob_addr(EMPTY_TEXT));
        return;
    }
    DWORD text;
    zs_ctor(&text);
    fn<void(THISCALL *)(DWORD, DWORD, void *)>(0x470b60)(m(button, 0x13c), m(button, 0x140), &text);
    BYTE *label = bp(m(button, 0x138));
    vc(label, SET_VALUE, c_str(&text));
    zs_dtor(&text);
}
// A button centred on the dialog's border (2 × font height / 5)
void centre_on_border(BYTE *self, BYTE *b)
{
    const DWORD fifths = (m(bp(m(self, W_FONT)), 0x80) / 5) << 1;
    set_left(b, (int)fifths - (mi(b, W_RECT + 8) - mi(b, W_RECT)) / 2 + mi(b, W_RECT));
}
long double fat(DWORD address) { return (long double)*at<float *>(address); }
long double f32(DWORD bits) { float f; memcpy(&f, &bits, 4); return f; }

}  // namespace

// ---- The key button -------------------------------------------------------------------------------------------

BYTE *THISCALL KeyButton_DeletingDtor(BYTE *self, DWORD flags)   // 0x471080
{
    fn<void(THISCALL *)(BYTE *)>(0x40fb80)(self);
    if (flags & 1)
        pool_delete(self, 0x148);
    return self;
}
void THISCALL KeyButton_UpdateText(BYTE *self) { binding_text(self); }   // 0x4710b0
// Slot 4 (the frame): slot 86, the pressed look blinking (every 0x200 ms) while waiting for a key, the button's
void THISCALL KeyButton_Draw(BYTE *self, DWORD frame)   // 0x471120
{
    vc(self, 86);
    if (mb(self, 0x144))
        vc(self, CHECK, time_ms() & 0x200);
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x40fc20)(self, frame);
}
// The click: only with a configuration, it waits for a key (the focus moves by keyboard; the screen's cursor
// hidden) and the button's click follows
void THISCALL KeyButton_Click(BYTE *self)   // 0x471370
{
    if (!m(self, 0x13c))
        return;
    gb(glob_addr(G_FOCUS_BY_KEY)) = 1;
    mb(self, 0x144) = 1;
    if (BYTE *screen = fn<BYTE *(THISCALL *)(BYTE *)>(0x40ccd0)(self))
        fn<void(THISCALL *)(BYTE *)>(0x4098b0)(screen);
    fn<void(THISCALL *)(BYTE *)>(0x410160)(self);
}

// ---- The controls configuration dialog ------------------------------------------------------------------------

// The constructor (the parent, the configuration): a copy of the configuration kept (0x46fa40); per action a key
// button on the page of its group (made the first time) below the previous one or at the top, its name as title;
// the labels of the bindings beside the buttons, the pages' content sizes; at most 300 pixels high; the check
// box from the INI file, the joysticks, the buttons; the bindings shown
BYTE *THISCALL ControlsDialog_Ctor(BYTE *self, DWORD parent, DWORD config)   // 0x4713b0
{
    dialog_ctor(self, parent, c_str(glob_addr(0x6bd598)));
    m(self, 0) = vt(0x5f010c);
    m(self, 0x15c) = 0, m(self, 0x164) = 0, m(self, 0x160) = 0;
    m(self, 0x134) = config;
    fn<void(THISCALL *)(DWORD)>(0x46fa40)(config);
    fn<void(THISCALL *)(BYTE *)>(0x4038a0)(self);
    BYTE *tabs = alloc(0x11c);
    if (tabs)
        tabs = fn<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x409be0)(tabs, self);
    m(self, 0x140) = pv(tabs);
    m(self, 0x13c) = m(bp(m(self, 0x134)), 0x3c);
    m(self, 0x138) = pv(op_new(m(self, 0x13c) * 4));
    for (DWORD i = 0; i < m(self, 0x13c); i++) {
        const DWORD group = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x470d00)(m(self, 0x134), i);
        BYTE *page = fn<BYTE *(THISCALL *)(DWORD, DWORD)>(0x40a4a0)(m(self, 0x140), group);
        if (!page) {
            page = alloc(0x100);
            if (page) {
                window_ctor(page, m(self, 0x140), at<const char *>(group));
                m(page, 0) = vt(0x5ea72c);
            }
            fn<void(THISCALL *)(DWORD, BYTE *)>(0x40a2f0)(m(self, 0x140), page);
        }
        BYTE *kb = alloc(0x148);
        if (kb) {
            fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x40f9b0)(kb, self, 0, 0);
            m(kb, 0) = vt(0x5effb0);
            mb(kb, 0x144) = 0;
            m(kb, 0x138) = 0, m(kb, 0x13c) = 0;
            m(kb, 0x108) = 2;
        }
        m(bp(m(self, 0x138)), i * 4) = pv(kb);
        const DWORD how = (BYTE)vc(page, 65) ? 8 : 1;
        vc(page, V_ADD_CHILD, kb, how, 0, NOWHERE, NOWHERE, 0);
        BYTE *b = bp(m(bp(m(self, 0x138)), i * 4));
        m(b, 0x13c) = m(self, 0x134);
        m(b, 0x140) = i;
        if (m(self, 0x134))
            vc(b, SET_TITLE, fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x470ce0)(m(self, 0x134), i));
        else
            vc(b, SET_TITLE, glob_addr(EMPTY_TEXT));
        if (m(b, 0x138))
            fn<void(THISCALL *)(BYTE *)>(0x4710b0)(b);
    }
    for (DWORD i = 0; i < m(self, 0x13c); i++)
        vc(bp(m(bp(m(self, 0x138)), i * 4)), 76);
    for (DWORD p = 0; p < m(bp(m(self, 0x140)), 0x104); p++) {
        BYTE *page = bp(m(bp(m(bp(m(self, 0x140)), 0x108)), p * 4));
        same_width(bp(m(page, W_FIRST)), bp(m(page, W_LAST)));
    }
    for (DWORD i = 0; i < m(self, 0x13c); i++) {
        BYTE *kb = bp(m(bp(m(self, 0x138)), i * 4));
        BYTE *label = alloc(0x128);
        if (label)
            label = fn<BYTE *(THISCALL *)(BYTE *, DWORD, const char *)>(0x4127d0)(label, m(kb, W_PARENT), glob<const char *>(0x60fd8c));
        m(kb, 0x138) = pv(label);
        vc(bp(m(kb, W_PARENT)), V_ADD_CHILD, label, 0xd, kb, NOWHERE, NOWHERE, 0);
        mb(bp(m(kb, 0x138)), W_FRAMED) = 1;
        fn<void(THISCALL *)(BYTE *)>(0x4710b0)(kb);
    }
    for (DWORD p = 0; p < m(bp(m(self, 0x140)), 0x104); p++) {
        BYTE *page = bp(m(bp(m(bp(m(self, 0x140)), 0x108)), p * 4));
        const int border = (signed char)mb(page, W_BORDER);
        const int h = vc<const int *>(bp(m(page, W_LAST)), V_GET_RECT)[3] + border;
        const DWORD w = m(page, W_SCROLL) ? m(bp(m(page, W_SCROLL)), S_W) : 0;
        fn<void(THISCALL *)(BYTE *, DWORD, int)>(0x40d550)(page, w, h);
        fn<void(THISCALL *)(BYTE *, DWORD)>(0x40da30)(page, 1);
    }
    fn<void(THISCALL *)(DWORD)>(0x40a520)(m(self, 0x140));
    BYTE *t = bp(m(self, 0x140));
    if (mi(t, W_RECT + 0xc) - mi(t, W_RECT + 4) > 0x12c)
        set_height(t, 0x12c);
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x40a4b0)(m(self, 0x140), 0, 1);
    add_child(self, bp(m(self, 0x140)), 1, 0, NOWHERE, NOWHERE, 0);
    BYTE *w = alloc(0x144);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x412280)(w, self, c_str(glob_addr(0x6bd58c)), 0);
    m(self, 0x154) = pv(w);
    add_child(self, w, 0xc, 0, NOWHERE, NOWHERE, 0);
    BYTE *box = bp(m(self, 0x154));
    const int autoswitch = fn<int(__fastcall *)(DWORD, DWORD, const char *, int)>(0x4a4670)(glob_addr(0x6bd330), glob_addr(0x6bd430), glob<const char *>(0x60fa14), 1);
    vc(box, CHECK, (DWORD)(autoswitch != 0));
    fn<void(THISCALL *)(BYTE *)>(0x471fb0)(self);
    w = alloc(0x118);
    if (w)
        w = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x408cd0)(w, self, m(self, 0x160), 999999);
    m(self, 0x164) = pv(w);
    add_child(self, w, 0xc, 0, NOWHERE, NOWHERE, 0);
    fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x408bc0)(m(self, 0x164), m(self, 0x168), 0);
    w = new_button(self, c_str(glob_addr(0x6bd59c)), 0);
    m(self, 0x150) = pv(w);
    add_child(self, w, 0xc, 0, NOWHERE, NOWHERE, 0);
    w = new_button(self, c_str(glob_addr(0x6bd5a4)), 0);
    m(self, 0x144) = pv(w);
    add_child(self, w, 0xc, 0, 0, 6, 0);
    w = new_button(self, c_str(glob_addr(0x6bd594)), 0);
    m(self, 0x148) = pv(w);
    add_child(self, w, 0xc, 0, 0, 8, 0);
    centre_on_border(self, bp(m(self, 0x148)));
    w = new_button(self, c_str(glob_addr(0x6bd588)), 0);
    m(self, 0x14c) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    fn<void(THISCALL *)(DWORD)>(0x46ffa0)(m(self, 0x134));
    for (DWORD i = 0; i < m(self, 0x13c); i++) {
        BYTE *kb = bp(m(bp(m(self, 0x138)), i * 4));
        m(kb, 0x13c) = m(self, 0x134);
        m(kb, 0x140) = i;
        if (m(self, 0x134))
            vc(kb, SET_TITLE, fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x470ce0)(m(self, 0x134), i));
        else
            vc(kb, SET_TITLE, glob_addr(EMPTY_TEXT));
        if (m(kb, 0x138))
            binding_text(kb);
    }
    m(self, 0x11c) = m(self, 0x148);
    m(self, 0x120) = m(self, 0x14c);
    return self;
}
BYTE *THISCALL ControlsDialog_DeletingDtor(BYTE *self, DWORD flags)   // 0x471a40
{
    m(self, 0) = vt(0x5f010c);
    if (DWORD ids = m(self, 0x15c)) {
        op_delete(bp(ids));
        m(self, 0x15c) = 0;
    }
    if (BYTE *names = bp(m(self, 0x160))) {
        vc(names, 0, 1);
        m(self, 0x160) = 0;
    }
    if (DWORD buttons = m(self, 0x138)) {
        op_delete(bp(buttons));
        m(self, 0x138) = 0;
    }
    dialog_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x16c);
    return self;
}
// Closing: with changes the "save the changes?" question (modal, 6) and no (al 0); else slot 6 and yes (al 1)
BYTE THISCALL ControlsDialog_CanClose(BYTE *self)   // 0x471bd0
{
    if ((BYTE)fn<DWORD(THISCALL *)(DWORD)>(0x46fb00)(m(self, 0x134))) {
        BYTE *q = alloc(0x140);
        if (q)
            q = fn<BYTE *(THISCALL *)(BYTE *, BYTE *, BYTE *)>(0x4721b0)(q, self, self);
        fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x40c6e0)(self, q, 6);
        return 0;
    }
    vc(self, V_ON_CLOSE);
    return 1;
}
// The joysticks: "none" (0x6bd580, id -1) and every joystick that answers or is the configuration's ("%d: " and
// its name) into a new name list and id array; the chosen one; the combo box's list and choice
void THISCALL ControlsDialog_Joysticks(BYTE *self)   // 0x471fb0
{
    DWORD text;
    zs_ctor(&text);
    if (DWORD ids = m(self, 0x15c)) {
        op_delete(bp(ids));
        m(self, 0x15c) = 0;
    }
    if (BYTE *names = bp(m(self, 0x160))) {
        vc(names, 0, 1);
        m(self, 0x160) = 0;
    }
    BYTE *names = alloc(0x14);
    if (names) {
        fn<void(THISCALL *)(BYTE *)>(0x592200)(names);
        m(names, 0) = vt(0x5f0240);
    }
    m(self, 0x160) = pv(names);
    const DWORD n = fn<DWORD(__cdecl *)()>(0x4dd920)();
    m(self, 0x15c) = pv(op_new(n * 4 + 4));
    m(self, 0x168) = 0;
    m(self, 0x158) = 0;
    vc(bp(m(self, 0x160)), 18, c_str(glob_addr(0x6bd580)));
    m(bp(m(self, 0x15c)), m(self, 0x158) * 4) = 0xffffffff;
    m(self, 0x158) = m(self, 0x158) + 1;
    for (DWORD i = 0; i < n; i++) {
        if (fn<DWORD(THISCALL *)(DWORD)>(0x470e50)(m(self, 0x134)) == i)
            m(self, 0x168) = m(self, 0x158);
        if (fn<DWORD(THISCALL *)(DWORD)>(0x470e50)(m(self, 0x134)) != i && !(BYTE)fn<DWORD(__fastcall *)(DWORD)>(0x4dd940)(i))
            continue;
        zs_format(&text, glob<const char *>(0x60fdbc), i + 1);
        DWORD name;
        zs_ctor(&name);
        fn<void(__fastcall *)(DWORD, void *)>(0x4dd930)(i, &name);
        fn<void(THISCALL *)(void *, const char *)>(0x4df330)(&text, c_str(&name));
        BYTE *list = bp(m(self, 0x160));
        vc(list, 18, c_str(&text));
        m(bp(m(self, 0x15c)), m(self, 0x158) * 4) = i;
        m(self, 0x158) = m(self, 0x158) + 1;
        zs_dtor(&name);
    }
    if (m(self, 0x164)) {
        fn<void(THISCALL *)(DWORD, DWORD)>(0x408b50)(m(self, 0x164), m(self, 0x160));
        fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x408bc0)(m(self, 0x164), m(self, 0x168), 0);
    }
    zs_dtor(&text);
}
// The name list of the joysticks (a FieldNameList, 0x14 bytes)
BYTE *THISCALL JoystickNames_DeletingDtor(BYTE *self, DWORD flags)   // 0x472180
{
    fn<void(THISCALL *)(BYTE *)>(0x592220)(self);
    if (flags & 1)
        pool_delete(self, 0x14);
    return self;
}

// ---- The question "save the changes?" --------------------------------------------------------------------------

BYTE *THISCALL SaveChangesDialog_Ctor(BYTE *self, DWORD parent, DWORD owner)   // 0x4721b0
{
    dialog_ctor(self, parent, nullptr);
    m(self, 0) = vt(0x5f02b0);
    BYTE *w = new_label(self, c_str(glob_addr(0x6bd584)));
    add_child(self, w, 1, 0, NOWHERE, NOWHERE, 0);
    w = new_button(self, c_str(glob_addr(0x6bd590)), 0);
    m(self, 0x134) = pv(w);
    add_child(self, w, 0xc, 0, NOWHERE, NOWHERE, 0);
    centre_on_border(self, bp(m(self, 0x134)));
    w = new_button(self, c_str(glob_addr(0x6bd5a0)), 0);
    m(self, 0x138) = pv(w);
    add_child(self, w, 0xe, 0, NOWHERE, NOWHERE, 0);
    focus(self, bp(m(self, 0x138)));
    m(self, 0x11c) = m(self, 0x138);
    m(self, 0x120) = m(self, 0x138);
    m(self, 0x13c) = owner;
    return self;
}
BYTE *THISCALL SaveChangesDialog_DeletingDtor(BYTE *self, DWORD flags)   // 0x472300
{
    dialog_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x140);
    return self;
}

// ---- The credits text ------------------------------------------------------------------------------------------

// The constructor (the file, the font): up to 999 lines of the file (those starting with '/' skipped), the rest
// of the 1000 emptied; the speed from the font's line height
BYTE *THISCALL CreditsText_Ctor(BYTE *self, const char *file, DWORD font)   // 0x475eb0
{
    m(self, 0x1c) = pv(op_new(0x3e800));
    m(self, 4) = font;
    m(self, 0x14) = 2;
    const int lh = fn<int(__fastcall *)(DWORD)>(0x4a6140)(font) + (int)m(self, 0x14);
    m(self, 8) = 0;
    m(self, 0x10) = (DWORD)lh;
    volatile float speed = (float)((long double)(long long)(DWORD)lh * fat(glob_addr(0x5f08ac)));
    m<float>(self, 0xc) = speed;
    m(self, 0) = time_ms();
    m(self, 0x18) = 0;
    alignas(4) BYTE fb[0x1c];
    fn<void(THISCALL *)(BYTE *)>(0x4a1780)(fb);
    using ReadLine = BYTE(THISCALL *)(BYTE *, BYTE *, DWORD);
    if ((BYTE)fn<DWORD(THISCALL *)(BYTE *, const char *, DWORD)>(0x4a1840)(fb, file, 4)) {
        if (fn<ReadLine>(0x4a20a0)(fb, bp(m(self, 0x1c)) + (m(self, 0x18) << 8), 0x100)) {
            do {
                const DWORD n = m(self, 0x18);
                if (n >= 0x3e7)
                    break;
                if (mb(bp(m(self, 0x1c)), n << 8) != '/')
                    m(self, 0x18) = n + 1;
            } while (fn<ReadLine>(0x4a20a0)(fb, bp(m(self, 0x1c)) + (m(self, 0x18) << 8), 0x100));
        }
        if ((int)m(self, 0x18) < 0x3e8) {
            DWORD p = m(self, 0x18) << 8;
            do {
                p += 0x100;
                mb(bp(m(self, 0x1c)), p - 0x100) = 0;
            } while ((int)p < 0x3e800);
        }
        fn<void(THISCALL *)(BYTE *)>(0x4a2100)(fb);
    }
    fn<void(THISCALL *)(BYTE *)>(0x4a1800)(fb);
    return self;
}
void THISCALL CreditsText_Dtor(BYTE *self)   // 0x475fd0
{
    if (DWORD text = m(self, 0x1c)) {
        op_delete(bp(text));
        m(self, 0x1c) = 0;
    }
}
// Draws the lines in view (the frame, the rectangle of the view): centred; a line with ':' as two columns (the
// part before right of the centre, the rest left of it); '_' a red and '+' a green underlined heading (the colour
// at the blend's opacity on 8 bits); faded in over the top eighth and out over the bottom eighth (from 5 pixels
// above it); the scroll restarts after the last line has passed
void THISCALL CreditsText_Draw(BYTE *self, BYTE *frame, const int *view)   // 0x475ff0
{
    const DWORD elapsed = time_ms() - m(self, 0);
    const DWORD centre = vc(frame, 45) >> 1;
    int y = (int)vc(frame, 39);
    volatile float scrolled = (float)((long double)(long long)elapsed * f32(m(self, 0xc)));
    const int off = x87_fistp(scrolled);
    y -= off;
    const DWORD lh = m(self, 0x10);
    const DWORD shown = (DWORD)(view[3] - view[1]) / lh;
    int first = (int)((DWORD)off / lh - shown) - 1;
    if (first < 0)
        first = 0;
    const int end = first + (int)shown + 2;
    volatile float saved = (float)fn<long double(THISCALL *)(DWORD)>(0x433c20)(glob_addr(MASK));
    if (end < (int)(m(self, 0x18) + shown + 1)) {
        y += (int)(lh * (DWORD)first);
        if (first < end) {
            DWORD p = (DWORD)first << 8;
            int count = end - first;
            const DWORD font = m(self, 4);
            using Draw = void(THISCALL *)(DWORD, BYTE *, DWORD, int, const char *, DWORD, DWORD);
            do {
                volatile float opacity = 1.0f;
                const int h1 = (int)vc(frame, 39);
                if (y < h1 / 8) {
                    opacity = (float)y;
                    opacity = (float)((long double)opacity / ((long double)(long long)vc(frame, 39) * fat(glob_addr(0x5f08c0))));
                } else if (y > (int)(vc(frame, 39) * 7u) / 8 - 5) {   // (7 × the height in 32 bits, as the original)
                    opacity = (float)y;
                    opacity = (float)((long double)opacity - (long double)(long long)vc(frame, 39) * fat(glob_addr(0x5f08bc)) + fat(glob_addr(0x5f08b8)));
                    opacity = (float)(fat(glob_addr(0x5ec3f4)) - (long double)opacity / ((long double)(long long)vc(frame, 39) * fat(glob_addr(0x5f08c0))));
                } else {
                    goto set;
                }
                if (x87_lt((long double)opacity, fat(glob_addr(0x5ebed0))))
                    opacity = 0.0f;
                else if (!x87_le((long double)opacity, fat(glob_addr(0x5ec3f4))))
                    opacity = 1.0f;
            set:
                fn<void(THISCALL *)(DWORD, float)>(0x433be0)(glob_addr(MASK), opacity);
                char *line = reinterpret_cast<char *>(bp(m(self, 0x1c)) + p);
                DWORD colon = 0;
                for (DWORD c = 0; c < 0x100; c++) {
                    if (line[c] == ':')
                        colon = c;
                    if (!line[c])
                        break;
                }
                if (colon) {
                    line[colon] = 0;
                    fn<Draw>(0x4a6470)(font, frame, centre, y, line, 2, 0);
                    fn<Draw>(0x4a6470)(m(self, 4), frame, centre, y, line + colon + 1, 0, 0);
                    line[colon] = ':';
                } else if (line[0] == '_' || line[0] == '+') {
                    BYTE color[0x10];
                    fn<void(THISCALL *)(BYTE *)>(0x53f000)(color);
                    using RGBA = void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD);
                    const bool red = line[0] == '_';
                    if (vc(frame, 29) == 8) {
                        volatile float a = (float)(fn<long double(THISCALL *)(DWORD)>(0x433c20)(glob_addr(MASK)) * fat(glob_addr(0x5f08b4)));
                        const DWORD ai = (DWORD)x87_fistp(a);
                        if (red)
                            fn<RGBA>(0x53f080)(color, ai, 0, 0, 0xff);
                        else
                            fn<RGBA>(0x53f080)(color, 0, ai, 0, 0xff);
                    } else if (red) {
                        fn<RGBA>(0x53f080)(color, 0x9c, 0, 0, gd(glob_addr(0x6bcc90)));
                    } else {
                        fn<RGBA>(0x53f080)(color, 0, 0x9c, 0, gd(glob_addr(0x6bcc90)));
                    }
                    fn<Draw>(0x4a6470)(m(self, 4), frame, centre, y, line + 1, 1, 0);
                    const DWORD tw = fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x4a7310)(m(self, 4), line + 1, -1);
                    const int lh2 = fn<int(__fastcall *)(DWORD)>(0x4a6140)(m(self, 4));
                    const DWORD half = tw >> 1;
                    fn<void(THISCALL *)(BYTE *, int, int, int, int, BYTE *)>(0x4b3a70)(
                        frame, (int)(centre - half), lh2 + y + 1, (int)(half + centre + 1), lh2 + y + 2, color);
                } else {
                    fn<Draw>(0x4a6470)(m(self, 4), frame, centre, y, line, 1, 0);
                }
                y += (int)m(self, 0x10);
                p += 0x100;
            } while (--count);
        }
    }
    fn<void(THISCALL *)(DWORD, float)>(0x433be0)(glob_addr(MASK), saved);
    if (end > (int)(m(self, 0x18) + shown * 2))
        m(self, 0) = time_ms();
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_controls)
{
    const Hook list[] = {
#include "ui_controls_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
