// The mission objectives' drawing (docs/DECOMPILATION.md 10, "Batch C5"): MissionObjectives_DrawPanel 0x509890 and
// MissionObjectives_Draw 0x509910. Every call goes through its exe address.
//
// [confirmed: code; VM]
//  - 0x509890 (fastcall: the mission manager, the frame; draw): reads the reporter's time (slot 13; with draw it
//    computes 0x5eecd8 - (time - +0x1c) and drops it); with draw: the current mission's objectives (0x5127e0) at
//    (140, frame height - 110), width frame width - 280, opacity 1, the title drawn, no options. eax: 0x509910's.
//  - 0x509910 (fastcall: the mission, the frame; x, y, width, opacity, draw, with title, options or null): a local
//    CMissionObjectiveDisplayOptions (its defaults when no options are given) and the options' four fonts (+0x10
//    description, +0x14 normal, +0x18 done, +0x1c failed) and "Small Gold". Without a mission: with draw the text
//    0x6cfd14 one line above y; the height (the font's line + 2, or 0 without draw). With a mission the frame's
//    Transparent Mask opacity (0x6bcc68) is multiplied by the opacity; with title the mission's name (+8, right of
//    the tab stop +8, with draw) one line above and its description (+0xc) below, then per objective (+0x18, the
//    linked objective +0x20 when set; CMission::OldCount 0x512330): its mark (+8) left of x (with draw), "<name><
//    0x6cfd04> " in the description font and the objective's text (+0x14, or 0x6cfd18... 0x6cfcf8 when empty) after
//    it in the state's font (slot 24 done: half opacity, slot 25 failed), the heights added up (CFont::
//    DrawParagraphs 0x4a6ae0, BoxLastRight 0x4a7210); without objectives and without draw the text 0x6cfd18. eax:
//    the height drawn less the options' margin +0xc (or the heights as above).
#include "ui_screens.h"
#include "x87.h"

namespace zax::missiondraw {

using namespace ui;
using LD = long double;

constexpr DWORD FONTS_LIST = 0x6be1e0, MASK_BLEND = 0x6bcc68, OPTIONS_VTABLE = 0x5f9fac;

inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
inline DWORD h(DWORD font) { return m(bp(font), 0x80); }
inline const char *desc_name(BYTE *field) { return fn<const char *(THISCALL *)(void *)>(0x5007a0)(field); }
inline void set_opacity(DWORD bits) { fn<void(THISCALL *)(DWORD, DWORD)>(0x433be0)(glob_addr(MASK_BLEND), bits); }
inline void draw_text(DWORD font, void *frame, int x, int y, const char *s, DWORD just)
{
    fn<void(THISCALL *)(DWORD, void *, int, int, const char *, DWORD, DWORD)>(0x4a6470)(font, frame, x, y, s, just, 0);
}
// CFont::DrawParagraphs (frame, x, y, text, width, 2, 0, indent, 0, 0, 2 · tab stop, 0): the lines drawn
inline int paragraphs(DWORD font, void *frame, int x, int y, const char *s, int width, DWORD indent, DWORD tab)
{
    return fn<int(THISCALL *)(DWORD, void *, int, int, const char *, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4a6ae0)(
        font, frame, x, y, s, width, 2, 0, indent, 0, 0, tab, 0);
}
inline const char *text_or_empty(BYTE *z) { return zs_not_empty(z) ? c_str(z) : glob<const char *>(0x62e558); }
inline void options_dtor(BYTE *o)
{
    m(o, 0) = vt(OPTIONS_VTABLE);
    fn<void(THISCALL *)(void *)>(0x4173f0)(o);
    fn<void(THISCALL *)(void *)>(0x4166d0)(o);
}

}  // namespace zax::missiondraw

namespace zax {

using namespace missiondraw;

namespace {

// 0x509910
int __fastcall MissionObjectives_Draw(BYTE *mission, void *frame, int x, int y, int width, DWORD opacity, BYTE draw, BYTE title, BYTE *opts)
{
    alignas(4) BYTE local[0x20];
    fn<void *(THISCALL *)(void *)>(0x416610)(local);
    m(local, 0) = vt(OPTIONS_VTABLE);
    fn<void(THISCALL *)(void *)>(0x4173c0)(local);
    if (!opts) {
        fn<void(THISCALL *)(void *)>(0x417230)(local);
        opts = local;
    }
    const DWORD f1 = record_loaded(glob_addr(FONTS_LIST), m(opts, 0x10));
    const DWORD f2 = record_loaded(glob_addr(FONTS_LIST), m(opts, 0x14));
    const DWORD f3 = record_loaded(glob_addr(FONTS_LIST), m(opts, 0x1c));
    const DWORD f4 = record_loaded(glob_addr(FONTS_LIST), m(opts, 0x18));
    const DWORD i_gold = vc(glob<void *>(FONTS_LIST), 3, glob<const char *>(0x612264) /* "Small Gold" */, -1);
    const DWORD gold = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x48d8f0)(glob_addr(FONTS_LIST), i_gold);
    int total = 0;
    if (!mission) {
        int height = 0;
        if (draw) {
            if (frame)
                draw_text(gold, frame, x, y - (int)h(gold) - 2, c_str(glob_addr(0x6cfd14)), 0);
            height = (int)h(gold) + 2;
        }
        options_dtor(local);
        return height;
    }
    const int margin = (int)m(opts, 0xc);
    DWORD saved = 0x3f800000;
    if (frame) {
        const LD o = fn<LD(THISCALL *)(DWORD)>(0x433c20)(glob_addr(MASK_BLEND));
        saved = float_bits(fstp(o));
        opacity = float_bits(fstp(o * raw_float(opacity)));
        set_opacity(opacity);
    }
    int left = x, w = width, top = y;
    if (title) {
        if (draw) {
            const char *name = text_or_empty(mission + 8);
            const int gh = (int)h(gold);
            const int lines = paragraphs(gold, frame, left, y - gh - 2, name, w, 0, m(opts, 8) << 1);
            total = lines * (gh + 2);
            top = y + (total - (int)h(gold)) - 2;
        }
        const char *desc = desc_name(mission + 0xc);
        const int fh = (int)h(f1);
        const int lines = paragraphs(f1, frame, left, top, desc, w, 0, m(opts, 8) << 1);
        const int added = lines * (fh + 2) + margin;
        top = added + top - 2;
        total = total + added - 2;
        const int tab = (int)m(opts, 8);
        left += tab;
        w -= tab;
    }
    if (!m(mission, 0x1c) && !draw) {
        if (frame) {
            draw_text(gold, frame, left, top, c_str(glob_addr(0x6cfd18)), 0);
            set_opacity(saved);
        }
        const int gh = (int)h(gold);
        options_dtor(local);
        return total + gh + 2;
    }
    const DWORD count = fn<DWORD(THISCALL *)(BYTE *)>(0x512330)(mission);
    for (DWORD i = 0; i < count; i++) {
        BYTE *obj = bp(m(bp(m(mission, 0x18)), i * 4));
        if (BYTE *linked = bp(m(obj, 0x20)))
            obj = linked;
        DWORD alpha = 0x3f800000, font = f2;
        const char *name = desc_name(obj + 0x14);
        if (!*name)
            name = c_str(glob_addr(0x6cfcf8));
        if ((BYTE)vc(obj, 24)) {
            alpha = 0x3f000000;
            font = f4;
        } else if ((BYTE)vc(obj, 25)) {
            font = f3;
        }
        if (frame)
            set_opacity(float_bits(fstp(fld(raw_float(alpha)) * raw_float(opacity))));
        if (draw)
            draw_text(gold, frame, left - 2, top, text_or_empty(obj + 8), 2);
        BYTE line[4];
        zs_ctor(line);
        const char *desc = desc_name(obj + 0xc);
        zs_format(line, glob<const char *>(0x619c3c) /* "%s%s " */, desc, c_str(glob_addr(0x6cfd04)));
        const int f1h = (int)h(f1);
        int lines = paragraphs(f1, frame, left, top, c_str(line), w, 0, m(opts, 8) << 1);
        const int part = lines * (f1h + 2) - (int)h(f1);
        top = part + top - 2;
        total = total + part - 2;
        const int right = fn<int(THISCALL *)(DWORD, const char *, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4a7210)(
                              f1, c_str(line), w, 2, 0, 0, 0, 0, m(opts, 8) << 1, 0) +
                          left;
        const int fh = (int)h(font);
        lines = paragraphs(font, frame, left, top, name, w, (DWORD)(right - left), m(opts, 8) << 1);
        const int added = (int)h(f1) + (margin - (int)h(font) + lines * (fh + 2));
        total = total + added - 2;
        top = added + top - 2;
        zs_dtor(line);
    }
    if (frame)
        set_opacity(saved);
    options_dtor(local);
    return total - margin;
}

// 0x509890 (fastcall: the mission manager, the frame; draw)
int __fastcall MissionObjectives_DrawPanel(BYTE *manager, BYTE *frame, BYTE draw)
{
    const LD now = vc<LD>(bp(gd(glob_addr(0x6c02cc))), 13);
    if (!draw)
        return 0;
    (void)fstp(cst(glob_addr(0x5eecd8)) - (now - fld(m<float>(manager, 0x1c))));
    const int hgt = (int)vc(frame, 39) - 0x6e;
    const int wid = (int)vc(frame, 45) - 0x118;
    BYTE *mission = fn<BYTE *(THISCALL *)(BYTE *)>(0x5127e0)(manager);
    return fn<int(__fastcall *)(BYTE *, void *, int, int, int, DWORD, DWORD, DWORD, DWORD)>(0x509910)(mission, frame, 0x8c, hgt, wid, 0x3f800000,
                                                                                                  draw, 1, 0);
}

}  // namespace

ZAX_HOOKS(mission_draw_c5)
{
    const Hook list[] = {
#include "mission_draw_c5_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
