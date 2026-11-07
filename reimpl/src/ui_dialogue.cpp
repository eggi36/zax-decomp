// The dialogue window ("Dialog Display Window", docs/DECOMPILATION.md 9, "The rest of the user interface"): the
// window of the conversations (CZaxDialogAction), one at a time (0x6bdabc), with the lines scrolling up at a
// speed that follows the voice-over, the two speakers' portraits and the selection lights. Every call into the
// toolkit or the rest of the game goes through its exe address.
//
// Layout [confirmed: the constructor, the destructor and the slots below] (0x15c bytes, vtable 0x5f13a0):
//   +0x100 clicked, +0x104 the lines (a pool pointer array, vtable 0x5f14d0/0x5f14dc) of 0x10-byte lines {ZString
//   the wrapped text, the height, ZString the voice file, ZString the speaker}, +0x114 two speakers {ZString
//   the name, the portrait index, shown} of 0xc bytes, +0x12c the next speaker slot, +0x130 the speaker
//   talking (-1), +0x134/+0x138/+0x13c/+0x140 the sequences "Dialog Window Background", "Dialog Window Frame",
//   "Selection Light Top"/"Bottom", +0x144 the scroll position (float), +0x148 the scroll speed, +0x14c the
//   line being said (-1), +0x150 the button, +0x154 a line was finished, +0x155 the voice is playing, +0x158 an
//   object deleted with the window
//   the display options (CZaxCustomRules::DialogDisplay 0x5a1e80): +0x10/+0x14 the background, +0x18/+0x1c the
//   button, +0x20/+0x24 the portraits (+0x2c apart), +0x38 … +0x44 the lights, +0x48 … +0x54 the text area,
//   +0x58 … +0x64 the filled area, +0x68 the most speed, +0x6c the speed without voice, +0x70 the font
#include "ui_screens.h"

namespace zax::ui {

namespace {

constexpr DWORD G_DIALOGUE = 0x6bdabc, MASK = 0x6bcc68;
constexpr DWORD OPACITY_FULL = 0x3f800000, OPACITY_DIM = 0x3f19999a;

BYTE *dialog_display() { return fn<BYTE *(THISCALL *)(DWORD)>(0x5a1e80)(custom_rules()); }
DWORD find(DWORD name) { return fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x523df0)(glob_addr(SEQUENCES), at<const char *>(name), -1); }
void set_opacity(DWORD bits) { fn<void(THISCALL *)(DWORD, DWORD)>(0x433be0)(glob_addr(MASK), bits); }
void blit(DWORD frame, DWORD image, int x, int y) { fn<DWORD(THISCALL *)(DWORD, DWORD, int, int, DWORD)>(0x4b3280)(frame, image, x, y, glob_addr(MASK)); }
BYTE *voice() { return fn<BYTE *(__cdecl *)()>(0x595b50)(); }
BYTE *line(BYTE *self, DWORD i) { return bp(m(bp(m(self, 0x108)), i * 4)); }
long double f32(DWORD bits) { float f; memcpy(&f, &bits, 4); return f; }
long double fat(DWORD address) { return (long double)*at<float *>(address); }
// The portrait of a speaker ("Dialog Display/Dialog Window Frame/Portraits/%s", else "!Unknown")
DWORD portrait(const char *name)
{
    DWORD text;
    zs_ctor(&text);
    zs_format(&text, glob<const char *>(0x611040), name);
    DWORD index = fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x523df0)(glob_addr(SEQUENCES), c_str(&text), -1);
    if (index == 0xffffffff)
        index = fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x523df0)(glob_addr(SEQUENCES), glob_addr(0x611000), index);
    zs_dtor(&text);
    return index;
}

}  // namespace

// The constructor (the parent): the lines and speakers empty, the messages of the world cleared, the scroll
// position at minus the text area's height, the reporter's slot 28, the sequences found, sized to the frame
// sequence and the background's offsets and centred above the HUD bar (114 pixels), the button
BYTE *THISCALL DialogueWindow_Ctor(BYTE *self, DWORD parent)   // 0x47bbd0
{
    window_ctor(self, parent, glob<const char *>(0x610fe8));
    m(self, 0x104) = vt(0x5f14d0);
    m(self, 0x108) = 0, m(self, 0x10c) = 0, m(self, 0x110) = 0;
    for (DWORD o = 0x114; o < 0x12c; o += 0xc) {
        zs_ctor(self + o);
        m(self, o + 4) = 0xffffffff;
        mb(self, o + 8) = 0;
    }
    m(self, 0) = vt(0x5f13a0);
    fn<void(THISCALL *)(DWORD)>(0x506f70)(m(bp(gd(glob_addr(G_WORLD))), 0x280));
    BYTE *opt = dialog_display();
    m(self, 0x12c) = 0;
    m(self, 0x130) = 0xffffffff;
    mb(self, 0x154) = 0;
    m(self, 0x148) = 0;
    m(self, 0x14c) = 0xffffffff;
    volatile float scroll = (float)-(long double)(long long)m(opt, 0x54);
    m<float>(self, 0x144) = scroll;
    vc(bp(gd(glob_addr(G_REPORTER))), 28);
    mb(self, 0x100) = 0;
    fn<void(__fastcall *)(DWORD)>(0x4a2650)(glob_addr(0x610fd8));
    m(self, 0x134) = find(glob_addr(0x610fb0));
    m(self, 0x138) = find(glob_addr(0x610f8c));
    m(self, 0x13c) = find(glob_addr(0x610f68));
    m(self, 0x140) = find(glob_addr(0x610f40));
    int r[4];
    vc(bp(record_loaded(glob_addr(SEQUENCES), m(self, 0x138))), 36, r);
    BYTE *a = bp(record_loaded(glob_addr(SEQUENCES), m(self, 0x138)));
    BYTE *b = bp(record_loaded(glob_addr(SEQUENCES), m(self, 0x138)));
    const int h = (int)vc(a, 39) + (mi(opt, 0x14) - r[1]);
    const int w = (int)vc(b, 45) + (mi(opt, 0x10) - r[0]);
    fn<void(THISCALL *)(BYTE *, int, int)>(0x40d510)(self, w, h);
    const int height = mi(self, W_RECT + 0xc) - mi(self, W_RECT + 4);
    const int width = mi(self, W_RECT + 8) - mi(self, W_RECT);
    BYTE *screen = bp(m(bp(gd(glob_addr(G_REPORTER))), 4));
    const DWORD y = (vc(screen, 39) + (DWORD)(-0x72 - height)) >> 1;
    const DWORD x = (vc(screen, 45) - (DWORD)width) >> 1;
    move_to(self, (int)x, (int)y);
    BYTE *button = new_button(self, glob<const char *>(EMPTY_TEXT), 0);
    m(self, 0x150) = pv(button);
    const DWORD pressed = record_inline(glob_addr(SEQUENCES), find_index(glob_addr(SEQUENCES), glob_addr(0x610f20)));
    fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD)>(0x40ffd0)(m(self, 0x150), pressed, 1, 0);
    add_child(self, bp(m(self, 0x150)), 5, 0, m(opt, 0x18), m(opt, 0x1c), 2);
    fn<void(__cdecl *)()>(0x4a2670)();
    m(self, 0x158) = 0;
    return self;
}
// The deleting destructor: the voice stopped, the object +0x158 deleted, the reporter's slot 29, no dialogue
// window any more, the lines freed (0x47e540, pool blocks of 0x10), the speakers' names, the array
BYTE *THISCALL DialogueWindow_DeletingDtor(BYTE *self, DWORD flags)   // 0x47bfe0
{
    m(self, 0) = vt(0x5f13a0);
    if (voice())
        fn<void(THISCALL *)(BYTE *)>(0x595af0)(voice());
    if (BYTE *o = bp(m(self, 0x158))) {
        vc(o, 0, 1);
        m(self, 0x158) = 0;
    }
    vc(bp(gd(glob_addr(G_REPORTER))), 29);
    gd(glob_addr(G_DIALOGUE)) = 0;
    array_delete_all(self + 0x104, [](DWORD e) {
        fn<void(THISCALL *)(DWORD)>(0x47e540)(e);
        pool_delete(bp(e), 0x10);
    });
    zs_dtor(self + 0x120);
    zs_dtor(self + 0x114);
    array_free_pool(self + 0x104, vt(0x5f14dc));
    window_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x15c);
    return self;
}
// Slot 10 (the button, the point): the window's own handling first; a left click inside the window marks it
// clicked (1)
DWORD THISCALL DialogueWindow_MouseDown(BYTE *self, DWORD button, const int *pt)   // 0x47c250
{
    if (fn<DWORD(THISCALL *)(BYTE *, DWORD, const int *)>(0x40bcc0)(self, button, pt))
        return 1;
    if (button == 0 && pt[0] >= mi(self, W_SCREEN) && pt[1] >= mi(self, W_SCREEN + 4) && pt[0] < mi(self, W_SCREEN + 8) &&
        pt[1] < mi(self, W_SCREEN + 0xc)) {
        mb(self, 0x100) = 1;
        return 1;
    }
    return 0;
}
// Slot 4 (the frame): the filled area (colour 0x62e5b8), the lines in the text area (the line being said at full
// opacity, the others at 60 %; from the scroll position down), the background, the speakers' portraits (the one
// talking at full opacity), the frame, the light of the speaker talking, the children
void THISCALL DialogueWindow_Draw(BYTE *self, DWORD frame)   // 0x47c2e0
{
    BYTE *opt = dialog_display();
    BYTE *f = bp(frame);
    int saved[4];
    vc(f, 36, saved);
    int r[4] = {mi(opt, 0x58), mi(opt, 0x5c), mi(opt, 0x58) + mi(opt, 0x60), mi(opt, 0x5c) + mi(opt, 0x64)};
    vc(f, 57, r, glob_addr(0x62e5b8));
    r[0] = mi(opt, 0x48), r[1] = mi(opt, 0x4c), r[2] = mi(opt, 0x48) + mi(opt, 0x50), r[3] = mi(opt, 0x4c) + mi(opt, 0x54);
    fn<void(THISCALL *)(DWORD, int *)>(0x524f50)(frame, r);
    const int x = (signed char)mb(self, W_BORDER) + mi(opt, 0x48);
    int y = mi(opt, 0x4c) - x87_fistp(f32(m(self, 0x144)));
    for (DWORD i = 0; i < m(self, 0x10c) && y < r[3]; i++) {
        set_opacity(i == m(self, 0x14c) ? OPACITY_FULL : OPACITY_DIM);
        BYTE *l = line(self, i);
        const DWORD font = record_loaded(glob_addr(FONTS), m(opt, 0x70));
        fn<void(THISCALL *)(DWORD, DWORD, int, int, const char *, DWORD, DWORD)>(0x4a66c0)(font, frame, x, y, c_str(l), 2, 0);
        y += mi(line(self, i), 4);
    }
    set_opacity(OPACITY_FULL);
    fn<void(THISCALL *)(DWORD, int *)>(0x524f50)(frame, saved);
    blit(frame, record_loaded(glob_addr(SEQUENCES), m(self, 0x134)), mi(opt, 0x10), mi(opt, 0x14));
    for (DWORD k = 0; k < 2; k++) {
        BYTE *speaker = self + 0x118 + k * 0xc;
        set_opacity(k == m(self, 0x130) ? OPACITY_FULL : OPACITY_DIM);
        if (mb(speaker, 4))
            blit(frame, record_loaded(glob_addr(SEQUENCES), m(speaker, 0)), mi(opt, 0x20), mi(opt, 0x2c) * (int)k + mi(opt, 0x24));
    }
    set_opacity(OPACITY_FULL);
    blit(frame, record_loaded(glob_addr(SEQUENCES), m(self, 0x138)), 0, 0);
    const DWORD talking = m(self, 0x130);
    if (talking != 0xffffffff) {
        const int ly = talking ? mi(opt, 0x44) : mi(opt, 0x3c);
        const int lx = talking ? mi(opt, 0x40) : mi(opt, 0x38);
        const DWORD light = record_inline(glob_addr(SEQUENCES), talking ? m(self, 0x140) : m(self, 0x13c));
        blit(frame, light, lx, ly);
    }
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x40b620)(self, frame);
}
// Slot 2 (the seconds): the next line once the voice has finished (or, without a voice for the first line, once
// it scrolled to 80 pixels above its end); after the last line the window closes (slot 5); the scroll position
// moves by the seconds × the speed, and lines scrolled out above (not the first being said) are dropped
void THISCALL DialogueWindow_Update(BYTE *self, float dt)   // 0x47c810
{
    BYTE close = 0;
    const DWORD current = m(self, 0x14c);
    if (current != 0xffffffff) {
        bool next;
        if (mb(self, 0x155))
            next = !(BYTE)fn<DWORD(THISCALL *)(BYTE *)>(0x595bd0)(voice());
        else
            next = current == 0 && x87_fistp(f32(m(self, 0x144))) > mi(line(self, 0), 4) - 0x50;
        if (next) {
            mb(self, 0x154) = 1;
            const DWORD n = m(self, 0x14c) + 1;
            if (n < m(self, 0x10c)) {
                m(self, 0x14c) = n;
                if (BYTE *o = bp(m(self, 0x158)))
                    vc(o, 1);
                fn<void(THISCALL *)(BYTE *)>(0x47cfa0)(self);
            } else {
                m(self, 0x14c) = 0xffffffff;
                m(self, 0x130) = 0xffffffff;
                close = 1;
            }
        }
    }
    const long double v = (long double)dt * f32(m(self, 0x148)) + f32(m(self, 0x144));
    volatile float scroll = (float)v;
    m<float>(self, 0x144) = scroll;
    if (m(self, 0x14c) && m(self, 0x10c)) {
        const int h = mi(line(self, 0), 4);
        if (x87_fistp((long double)scroll) > h) {
            volatile float rest = (float)(f32(m(self, 0x144)) - (long double)h);
            m<float>(self, 0x144) = rest;
            if (BYTE *l = line(self, 0)) {
                zs_dtor(l + 0xc);
                zs_dtor(l + 8);
                zs_dtor(l);
                pool_delete(l, 0x10);
            }
            const DWORD count = m(self, 0x10c);
            if (count) {
                if (count > 1)
                    memmove_(bp(m(self, 0x108)), bp(m(self, 0x108)) + 4, count * 4 - 4);
                m(self, 0x10c) = m(self, 0x10c) - 1;
            }
            const DWORD i = m(self, 0x14c);
            if (i != 0xffffffff && i)
                m(self, 0x14c) = i - 1;
        }
    }
    if (close)
        vc(self, V_CLOSE);
}
// The object deleted with the window (the old one deleted first)
void THISCALL DialogueWindow_SetObject(BYTE *self, DWORD object)   // 0x47c9d0
{
    if (BYTE *o = bp(m(self, 0x158))) {
        vc(o, 0, 1);
        m(self, 0x158) = 0;
    }
    m(self, 0x158) = object;
}
// A line of a conversation (fastcall: the speaker, the text; the voice file, a new window): the dialogue window
// made when there is none (or again when asked), the reporter's slot 23 with it, the line added (0x47cd20)
void __fastcall Dialogue_Say(DWORD speaker, DWORD text, DWORD voice_file, BYTE renew)   // 0x47ca10
{
    BYTE *w = bp(gd(glob_addr(G_DIALOGUE)));
    if (!w || renew) {
        if (w) {
            vc(w, 0, 1);
            gd(glob_addr(G_DIALOGUE)) = 0;
        }
        BYTE *n = alloc(0x15c);
        if (n)
            n = fn<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x47bbd0)(n, fn<DWORD(THISCALL *)(DWORD)>(0x4cdf30)(gd(glob_addr(G_REPORTER))));
        gd(glob_addr(G_DIALOGUE)) = pv(n);
        vc(bp(gd(glob_addr(G_REPORTER))), 23, n, 0);
    }
    fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD)>(0x47cd20)(gd(glob_addr(G_DIALOGUE)), speaker, text, voice_file);
}
DWORD __cdecl Dialogue_Window() { return gd(glob_addr(G_DIALOGUE)); }   // 0x47ca90
// The window's sequences kept loaded (the records' flags +0x7d and +0x7e, the objects' slot 7 once); a window
// made for it only is deleted again
void __cdecl Dialogue_Preload()   // 0x47caa0
{
    const BYTE had = gd(glob_addr(G_DIALOGUE)) != 0;
    if (!had) {
        BYTE *n = alloc(0x15c);
        if (n)
            n = fn<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x47bbd0)(n, fn<DWORD(THISCALL *)(DWORD)>(0x4cdf30)(gd(glob_addr(G_REPORTER))));
        gd(glob_addr(G_DIALOGUE)) = pv(n);
    }
    using Object = DWORD(THISCALL *)(DWORD, DWORD);
    for (DWORD member = 0x134; member <= 0x13c; member += 4) {
        const DWORD index = m(bp(gd(glob_addr(G_DIALOGUE))), member);
        if (!gd(glob_addr(SEQUENCES) + 0x30))
            continue;
        const DWORD rec = index << 7;
        gb(gd(glob_addr(SEQUENCES) + 0x30) + rec + 0x7d) = 1;
        if (DWORD recs = gd(glob_addr(SEQUENCES) + 0x30)) {
            const DWORD i = index < gd(glob_addr(SEQUENCES) + 0x34) ? index : 0;
            if (!gb(recs + (i << 7) + 0x7f) && !m(bp(recs), (i << 7) + 0x78))
                fn<void(THISCALL *)(DWORD, DWORD)>(0x55e090)(glob_addr(SEQUENCES), i);
        }
        if (gb(gd(glob_addr(SEQUENCES) + 0x30) + rec + 0x7e))
            continue;
        gb(gd(glob_addr(SEQUENCES) + 0x30) + rec + 0x7e) = 1;
        vc(bp(fn<Object>(0x48d8f0)(glob_addr(SEQUENCES), index)), 7);
    }
    const DWORD index = m(bp(gd(glob_addr(G_DIALOGUE))), 0x140);
    if (gd(glob_addr(SEQUENCES) + 0x30)) {
        const DWORD rec = index << 7;
        gb(gd(glob_addr(SEQUENCES) + 0x30) + rec + 0x7d) = 1;
        fn<Object>(0x48d8f0)(glob_addr(SEQUENCES), index);
        if (!gb(gd(glob_addr(SEQUENCES) + 0x30) + rec + 0x7e)) {
            gb(gd(glob_addr(SEQUENCES) + 0x30) + rec + 0x7e) = 1;
            vc(bp(fn<Object>(0x48d8f0)(glob_addr(SEQUENCES), index)), 7);
        }
    }
    if (!had && gd(glob_addr(G_DIALOGUE))) {
        vc(bp(gd(glob_addr(G_DIALOGUE))), 0, 1);
        gd(glob_addr(G_DIALOGUE)) = 0;
    }
}
// The portrait index of a speaker (a C string)
DWORD __fastcall Dialogue_Portrait(const char *speaker) { return portrait(speaker); }   // 0x47ccc0
// A line (the speaker, the text, the voice file: ZStrings): the speaker trimmed ("???" when empty), "%s: %s"
// wrapped to the text area's width less twice the border in the options' font, its height plus the font's;
// when no line is being said this one starts (0x47cfa0)
void THISCALL DialogueWindow_AddLine(BYTE *self, DWORD speaker, DWORD text, DWORD voice_file)   // 0x47cd20
{
    BYTE *opt = dialog_display();
    BYTE *l = alloc(0x10);
    if (l) {
        zs_ctor(l);
        zs_ctor(l + 8);
        zs_ctor(l + 0xc);
    }
    vc(self + 0x104, 2, &l);
    zs_assign(l + 8, voice_file);
    zs_assign(l + 0xc, speaker);
    fn<void(THISCALL *)(BYTE *)>(0x4e0f20)(l + 0xc);
    fn<void(THISCALL *)(BYTE *)>(0x4e10f0)(l + 0xc);
    if ((BYTE)fn<DWORD(THISCALL *)(BYTE *)>(0x4e13d0)(l + 0xc))
        fn<void(THISCALL *)(BYTE *, const char *)>(0x4e1930)(l + 0xc, glob<const char *>(0x611084));
    if (!m(self, W_FONT))
        m(self, W_FONT) = gd(glob_addr(0x62e5d0));
    const int width = mi(opt, 0x50) - 2 * (signed char)mb(self, W_BORDER);
    DWORD both;
    zs_ctor(&both);
    const char *who = c_str(speaker);
    zs_format(&both, glob<const char *>(0x61107c), who, c_str(text));
    const DWORD font = record_loaded(glob_addr(FONTS), m(opt, 0x70));
    DWORD wrapped;
    const DWORD z = fn<DWORD(THISCALL *)(DWORD, void *, const char *, int)>(0x4a7450)(font, &wrapped, c_str(&both), width);
    zs_assign(l, z);
    zs_dtor(&wrapped);
    const DWORD lead = m(bp(record_loaded(glob_addr(FONTS), m(opt, 0x70))), 0x80);
    const DWORD f3 = record_loaded(glob_addr(FONTS), m(opt, 0x70));
    m(l, 4) = fn<DWORD(THISCALL *)(DWORD, const char *, DWORD)>(0x4a6c80)(f3, c_str(l), 2) + lead;
    if (m(self, 0x14c) == 0xffffffff) {
        m(self, 0x14c) = m(self, 0x10c) - 1;
        fn<void(THISCALL *)(BYTE *)>(0x47cfa0)(self);
    }
    zs_dtor(&both);
}
// The line being said starts: its voice played (the seconds it takes); with a voice the speed makes the line
// reach the top as the voice ends (at least 1, at most +0x68 pixels a second), else the options' +0x6c; the
// speaker is one of the two shown (else takes the next slot with its portrait) and talking
void THISCALL DialogueWindow_StartLine(BYTE *self)   // 0x47cfa0
{
    BYTE *opt = dialog_display();
    const DWORD i = m(self, 0x14c);
    mb(self, 0x155) = 0;
    float seconds = 0;
    const char *file = c_str(line(self, i) + 8);
    fn<void(THISCALL *)(BYTE *, const char *, DWORD, float *)>(0x595be0)(voice(), file, 2, &seconds);
    long double speed;
    if (x87_lt((long double)seconds, fat(glob_addr(0x5f14ec)))) {
        speed = (long double)(long long)m(opt, 0x6c);
    } else {
        const DWORD n = m(self, 0x14c);
        mb(self, 0x155) = 1;
        long double top = -f32(m(self, 0x144));
        volatile float above = (float)top;
        if (n) {
            for (DWORD k = 0; k < n; k++)
                top += (long double)mi(line(self, k), 4);
            above = (float)top;
        }
        BYTE *o2 = dialog_display();
        const long double area = (long double)(long long)m(o2, 0x54);
        volatile float area_f = (float)area;
        long double s = area * fat(glob_addr(0x5eee2c));
        if (!mb(self, 0x154))
            s = (long double)area_f * fat(glob_addr(0x5f14e8));
        volatile float most = (float)(long double)(long long)m(opt, 0x68);
        s -= (long double)mi(line(self, n), 4);
        s = (long double)above - s;
        s /= (long double)seconds;
        if (x87_lt(s, fat(glob_addr(0x5ec3f4))))
            s = fat(glob_addr(0x5ec3f4));
        else if (!x87_le(s, (long double)most))
            s = most;
        speed = s;
    }
    volatile float stored = (float)speed;
    m<float>(self, 0x148) = stored;
    BYTE *name = line(self, m(self, 0x14c)) + 0xc;
    for (DWORD k = 0; k < 2; k++) {
        BYTE *s = self + 0x114 + k * 0xc;
        if (mb(s, 8) && !fn<int(THISCALL *)(BYTE *, BYTE *)>(0x4e17e0)(s, name)) {
            m(self, 0x130) = k;
            return;
        }
    }
    const DWORD slot = m(self, 0x12c);
    const DWORD index = portrait(c_str(name));
    BYTE *s = self + 0x114 + slot * 0xc;
    mb(s, 8) = 1;
    zs_assign(s, pv(name));
    m(s, 4) = index;
    m(self, 0x12c) = slot + 1 >= 2 ? 0 : slot + 1;
    m(self, 0x130) = slot;
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(ui_dialogue)
{
    const Hook list[] = {
#include "ui_dialogue_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
