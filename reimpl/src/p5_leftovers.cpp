// Phase 5 leftovers (docs/DECOMPILATION.md 10, "Phase 5 leftovers"): the four game functions the batches C3 and C4
// left: the mission over window's drawing (0x50dc10), the properties file name callback (0x4f8120) and the model
// preview's slots 2 and 3 (0x517f20, 0x518080).
//
// Every call goes through its exe address (the original's side of a check runs the original all the way down);
// floating point follows the original instruction by instruction (x87.h).
#include "ui_screens.h"
#include "x87.h"

namespace zax::ui {

namespace {

constexpr DWORD FONT_LIST = 0x6be1e0;
constexpr DWORD BLEND_TRANSPARENT_MASK = 0x6bcc68;

inline DWORD class_info(DWORD getter) { return fn_dyn<DWORD(__cdecl *)()>(getter)(); }
inline bool is_kind_of(DWORD obj, DWORD getter)
{
    const DWORD ci = class_info(getter);
    return (BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x416790)(obj, ci);
}
inline DWORD app_game_type() { return fn<DWORD(THISCALL *)(DWORD)>(0x59ff80)(gd(glob_addr(G_APP))); }
inline DWORD draw_wrapped(DWORD font, DWORD frame, int x, int y, const char *text, DWORD width, DWORD align, DWORD last)
{
    using F = DWORD(THISCALL *)(DWORD, DWORD, int, int, const char *, DWORD, DWORD, DWORD);
    return fn<F>(0x4a6790)(font, frame, x, y, text, width, align, last);
}
inline void draw_text(DWORD font, DWORD frame, int x, int y, const char *text, DWORD a, DWORD b)
{
    fn<void(THISCALL *)(DWORD, DWORD, int, int, const char *, DWORD, DWORD)>(0x4a6470)(font, frame, x, y, text, a, b);
}
inline void draw_int(DWORD font, DWORD frame, int x, int y, DWORD value, DWORD a)
{
    fn<void(THISCALL *)(DWORD, DWORD, int, int, DWORD, DWORD)>(0x4a6b90)(font, frame, x, y, value, a);
}
inline DWORD line_height(DWORD font) { return fn<DWORD(THISCALL *)(DWORD)>(0x4a6140)(font); }
inline void blit_at(DWORD frame, DWORD image, int x, int y)
{
    fn<void(THISCALL *)(DWORD, DWORD, int, int, DWORD)>(0x4b3280)(frame, image, x, y, glob_addr(BLEND_TRANSPARENT_MASK));
}
inline const char *field_name(const void *desc) { return fn<const char *(THISCALL *)(const void *)>(0x5007a0)(desc); }
// The quest game type's columns (0x5525e0: +0x8 the title's field descriptor, +0x18 the font, +0x1c the x offset,
// +0x20 the width, +0x24 the last argument of the wrapped text)
inline BYTE *quest_column(DWORD gt, DWORD k) { return fn<BYTE *(THISCALL *)(DWORD, DWORD)>(0x5525e0)(gt, k); }
inline DWORD stat_rows(DWORD gt, DWORD mission, DWORD frame, DWORD x, DWORD y)
{
    return fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(0x552730)(gt, mission, frame, x, y);
}
inline void set_clip(DWORD frame, void *rect) { fn<void(THISCALL *)(DWORD, void *)>(0x524f50)(frame, rect); }

// ZString functions of the exe
inline void zs_assign_text(DWORD z, const char *t) { fn<void(THISCALL *)(DWORD, const char *)>(0x4e1930)(z, t); }
inline void zs_append(DWORD z, const char *t) { fn<void(THISCALL *)(DWORD, const char *)>(0x4e1be0)(z, t); }
inline void zs_set_path3(DWORD z, const char *a, const char *b, const char *c)
{
    fn<void(THISCALL *)(DWORD, const char *, const char *, const char *)>(0x4e02c0)(z, a, b, c);
}

// A record of a definition list by index (the list's records at +0x30, 0x80 bytes each), null without records
inline const char *record_at(DWORD list, DWORD i)
{
    const DWORD recs = gd(list + 0x30);
    return recs ? at<const char *>(recs + (i << 7)) : nullptr;
}
// The index of the record whose object (+0x78) is `obj`, or -1
inline DWORD record_of(DWORD list, DWORD obj)
{
    const DWORD n = gd(list + 0x34);
    for (DWORD i = 0; i < n; i++)
        if (gd(gd(list + 0x30) + (i << 7) + 0x78) == obj)
            return i;
    return 0xffffffff;
}

}  // namespace

// ---- The mission over window's drawing ----------------------------------------------------------------------------

// CJWindow slot 4 of the mission over window (vtable 0x5fa064; layout in ui_mission.cpp): (frame) the window's frame
// sequence at the origin, the title, a double divider line (options +0x74, colour +0x84), the text wrapped in the
// text column (not in multiplayer), then by the game type: the quest statistics table (the mission +0x138: the
// column titles 0..2, the points column 3 with the total and the "points" label when any objective is scored, the
// rows below them, scrolled by the ticks times the game type's speed +0x40 through a clip when they do not fit) or
// the multiplayer scoreboard (the three column titles, one line per player: rank, name, frags); the buttons'
// images centred on the buttons; the children
void THISCALL MissionOver_Draw(BYTE *self, DWORD frame)   // 0x50dc10
{
    vc(bp(frame), 57, self + 0x17c, glob_addr(0x62e5a8));
    blit_at(frame, m(self, 0x168), -mi(self, 0x16c), -mi(self, 0x170));
    draw_text(m(self, 0x18c), frame, mi(self, 0x194), mi(self, 0x198), c_str(self + 0x190), 0, 0);
    BYTE *opt = bp(m(self, 0x164));
    int x = mi(opt, 0x58) - mi(opt, 0x70) + mi(opt, 0x50) - mi(self, 0x16c);
    const int top = mi(opt, 0x54) - mi(self, 0x170);
    int bottom = mi(opt, 0x5c) - mi(self, 0x170) + mi(opt, 0x54);
    if (mb(opt, 0x74)) {
        vc(bp(frame), 51, x, top, x, bottom, opt + 0x84);
        BYTE *o = bp(m(self, 0x164));
        vc(bp(frame), 51, x + 1, top, x + 1, bottom, o + 0x84);
    }
    DWORD text_height = 0;
    if (m(self, 0x1ac) != 2) {
        const DWORD font = record_loaded(glob_addr(FONT_LIST), m(opt, 0x78));
        BYTE *o = bp(m(self, 0x164));
        const DWORD width = m(o, 0x58) - 2 * m(o, 0x94) - m(o, 0x70);
        const char *text = c_str(self + 0x134);
        const int ty = mi(o, 0x98) + mi(o, 0x54) - mi(self, 0x170);
        const int tx = mi(o, 0x94) + mi(o, 0x50) - mi(self, 0x16c);
        text_height = draw_wrapped(font, frame, tx, ty, text, width, 2, 0);
    }
    opt = bp(m(self, 0x164));
    bottom -= mi(opt, 0x98);
    x -= mi(opt, 0x94);
    // the table's area: left, top (below the text), right, bottom
    int area[4];
    area[1] = mi(opt, 0x54) + 2 * mi(opt, 0x98) - mi(self, 0x170) + (int)text_height;
    area[0] = mi(opt, 0x50) - mi(self, 0x16c) + mi(opt, 0x94);
    area[2] = x;
    area[3] = bottom;
    const int left = area[0];
    int y = area[1];
    if (m(self, 0x138) && gd(glob_addr(G_APP)) && app_game_type()) {
        const DWORD quest_ci = class_info(fn_addr(0x551c20));
        if ((BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x416790)(app_game_type(), quest_ci) &&
            (BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x5525f0)(app_game_type(), m(self, 0x138))) {
            const DWORD gt = app_game_type();
            const BYTE scored = (BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x552690)(gt, m(self, 0x138));
            // the column titles; the tallest gives the rows' top
            DWORD titles = 0;
            for (DWORD k = 0; k < 3; k++) {
                BYTE *col = quest_column(gt, k);
                const DWORD font = record_loaded(glob_addr(FONT_LIST), m(col, 0x18));
                const DWORD h = draw_wrapped(font, frame, left + mi(col, 0x1c), y, field_name(col + 8), m(col, 0x20), 2, m(col, 0x24));
                if (k == 0 || titles <= h)
                    titles = h;
            }
            if (scored) {
                BYTE *col = quest_column(gt, 3);
                const DWORD font = record_loaded(glob_addr(FONT_LIST), m(col, 0x18));
                const DWORD h = draw_wrapped(font, frame, left + mi(col, 0x1c), y, field_name(col + 8), m(col, 0x20), 2, m(col, 0x24));
                if (titles <= h)
                    titles = h;
                // the total at the bottom with the label above it, both measured first
                BYTE total[4];
                zs_ctor(total);
                const int points = x87_ftol(fn<long double(THISCALL *)(DWORD, DWORD)>(0x5527f0)(gt, m(self, 0x138)));
                zs_format(total, glob<const char *>(0x60a5a0), points);   // "%d"
                int ty = area[3];
                DWORD f = record_loaded(glob_addr(FONT_LIST), m(col, 0x18));
                DWORD tall = draw_wrapped(f, 0, left + mi(col, 0x1c), ty, c_str(total), m(col, 0x20), 2, m(col, 0x24));
                f = record_loaded(glob_addr(FONT_LIST), m(col, 0x18));
                const DWORD lh = draw_wrapped(f, 0, mi(col, 0x1c) / 2 + left, ty,
                                              field_name(bp(m(self, 0x164)) + 0xa4), m(col, 0x1c), 2, 1);
                if (tall <= lh)
                    tall = lh;
                ty -= (int)tall;
                f = record_loaded(glob_addr(FONT_LIST), m(col, 0x18));
                draw_wrapped(f, frame, left + mi(col, 0x1c), ty, c_str(total), m(col, 0x20), 2, m(col, 0x24));
                f = record_loaded(glob_addr(FONT_LIST), m(col, 0x18));
                draw_wrapped(f, frame, mi(col, 0x1c) / 2 + left, ty, field_name(bp(m(self, 0x164)) + 0xa4), m(col, 0x1c), 2, 1);
                area[3] -= mi(bp(m(self, 0x164)), 0x98) + (int)tall;
                zs_dtor(total);
            }
            area[1] = y + mi(bp(m(self, 0x164)), 0x98) + (int)titles;
            const DWORD need = stat_rows(gt, m(self, 0x138), 0, 0, 0);
            if (need <= (DWORD)(area[3] - area[1])) {
                stat_rows(gt, m(self, 0x138), frame, (DWORD)area[0], (DWORD)area[1]);
            } else {
                // scrolled: the rows twice, one table height apart, through a clip on the area
                const float speed = fstp(-fld(m<float>(bp(gt), 0x40)));
                const DWORD ticks = fn<DWORD(__cdecl *)()>(0x4d2650)();
                const DWORD off = (DWORD)x87_ftol(fild64((long long)ticks) * fld(speed)) % need;
                int clip[4];
                vc(bp(frame), 36, clip);
                set_clip(frame, area);
                stat_rows(gt, m(self, 0x138), frame, (DWORD)area[0], off + (DWORD)area[1]);
                stat_rows(gt, m(self, 0x138), frame, (DWORD)area[0], off - need + (DWORD)area[1]);
                set_clip(frame, clip);
            }
            goto buttons;
        }
    }
    if (gd(glob_addr(G_APP)) && app_game_type()) {
        const DWORD mp_ci = class_info(fn_addr(0x51a270));
        if ((BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x416790)(app_game_type(), mp_ci)) {
            BYTE *gt = bp(app_game_type());
            int row_y = mi(gt, 0xc4) + y;
            DWORD font = record_loaded(glob_addr(FONT_LIST), m(gt, 0x98));
            draw_text(font, frame, mi(gt, 0xa4) + left, row_y, c_str(glob_addr(0x6cfd10)), m(gt, 0xa8), 0);
            draw_text(font, frame, mi(gt, 0xac) + left, row_y, c_str(glob_addr(0x6cfd28)), m(gt, 0xb0), 0);
            draw_text(font, frame, mi(gt, 0xb4) + left, row_y, c_str(glob_addr(0x6cfd0c)), m(gt, 0xb8), 0);
            row_y += (int)(line_height(font) + m(gt, 0xc0));
            const DWORD board = fn<DWORD(__cdecl *)()>(0x566a80)();
            const DWORD n = fn<DWORD(THISCALL *)(DWORD)>(0x482de0)(board);
            for (DWORD i = 0; i < n; i++) {
                const DWORD row = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x566e50)(board, i);
                if ((BYTE)vc(gt, 37)) {
                    const DWORD which = fn<DWORD(THISCALL *)(DWORD)>(0x482de0)(row) == 0 ? m(gt, 0x9c) : m(gt, 0xa0);
                    font = record_inline(glob_addr(FONT_LIST), which);
                }
                draw_int(font, frame, mi(gt, 0xa4) + left, row_y, i + 1, m(gt, 0xa8));
                const DWORD ident = fn<DWORD(THISCALL *)(DWORD)>(0x5664b0)(row);
                draw_text(font, frame, mi(gt, 0xac) + left, row_y, c_str(ident), m(gt, 0xb0), 0);
                const DWORD frags = fn<DWORD(THISCALL *)(DWORD)>(0x483750)(row);
                draw_int(font, frame, mi(gt, 0xb4) + left, row_y, frags, m(gt, 0xb8));
                row_y += (int)(line_height(font) + m(gt, 0xbc));
            }
        }
    }
buttons:
    for (DWORD *p = &m(self, 0x154); *p; p++) {
        BYTE *b = bp(*p);
        const DWORD image = m(self, 0x1a4);
        const int bh = mi(b, 0x10) - mi(b, 8);
        const int bw = mi(b, 0xc) - mi(b, 4);
        const int by = mi(b, 8) - (int)((vc(bp(image), 39) - (DWORD)bh) >> 1);
        const int bx = mi(b, 4) - (int)((vc(bp(image), 45) - (DWORD)bw) >> 1);
        blit_at(frame, m(self, 0x1a4), bx, by);
    }
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x40b620)(self, frame);
}

// ---- The properties file name callback ----------------------------------------------------------------------------

// The function pointer the static list 0x62e6a8 gets (0x4f8721; fastcall: the object, the ZString for the name):
// the "Properties.txt" or definition file of an object by its class: a model (its folder +0x6c), the layer's save
// data (the current level), a ground texture frame (the texture list 0x6c1d50's folder, slot 5, and record name +
// ".txt"), a dialogue line (its text's ZString), a rotation sequence (the model having a sequence of that name:
// "<model>/-<sequence>/Properties.txt"), the rules ("Rules.txt"), an inventory item or type definition (the
// catalogue's or the categories' folder, record name + ".txt"), a font or a sequence (the font list's, the
// sequence list 0x6c0948's or else the sequence list 0x6c06a0's folder "/-" record name "/Properties.txt"; the
// record loaded first, the last list's loader inlined). False for anything else or an object in no record.
DWORD __fastcall PropertiesFileName(DWORD obj, DWORD out)   // 0x4f8120
{
    if (is_kind_of(obj, fn_addr(0x515870))) {
    model:
        fn<void(THISCALL *)(DWORD, DWORD, const char *)>(0x4dfa50)(out, gd(obj + 0x6c), glob<const char *>(0x613a3c));
        return 1;
    }
    if (is_kind_of(obj, fn_addr(0x4e52f0))) {
        fn<void(THISCALL *)(DWORD, DWORD)>(0x4f3ac0)(gd(glob_addr(G_WORLD)), out);
        return 1;
    }
    if (is_kind_of(obj, fn_addr(0x4d1d50))) {
        const DWORD list = glob_addr(0x6c1d50);
        const DWORD i = record_of(list, obj);
        if (i == 0xffffffff)
            return 0;
        const char *rec = nullptr;
        if (gd(list + 0x30)) {
            fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x48d8f0)(list, i);
            rec = record_at(list, i);
        }
        const char *folder = vc<const char *>(at<void *>(list), 5);
        zs_set_path3(out, folder, rec, glob<const char *>(0x60f62c));
        return 1;
    }
    if (is_kind_of(obj, fn_addr(0x41deb0))) {
        zs_assign_text(out, c_str(gd(obj + 0x10) + 0x14));
        return 1;
    }
    if (is_kind_of(obj, fn_addr(0x515870)))
        goto model;
    if (is_kind_of(obj, fn_addr(0x55eae0))) {
        zs_assign_text(out, nullptr);
        DWORD index = 0xffffffff;
        const DWORD model = fn<DWORD(__fastcall *)(DWORD, DWORD *)>(0x4f7fc0)(obj, &index);
        if (!model)
            return 0;
        const DWORD folder = gd(model + 0x6c);
        const DWORD name = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x517200)(model, index);
        zs_format(bp(out), glob<const char *>(0x618908), folder, name);   // "%s/-%s/Properties.txt"
        return 1;
    }
    if (is_kind_of(obj, fn_addr(0x560b20))) {
        zs_assign_text(out, glob<const char *>(0x61826c));   // "Rules.txt"
        return 1;
    }
    // the item catalogue 0x6c0c08 and the categories 0x6c0800: folder (+0x38), record name, ".txt"
    const DWORD DEFS[2] = {fn_addr(0x4d55c0), fn_addr(0x4d70b0)};
    for (const DWORD def : DEFS) {
        if (!is_kind_of(obj, def))
            continue;
        const DWORD list = def == fn_addr(0x4d55c0) ? glob_addr(0x6c0c08) : glob_addr(0x6c0800);
        const DWORD i = record_of(list, obj);
        if (i == 0xffffffff)
            return 0;
        const char *rec = nullptr;
        if (gd(list + 0x30)) {
            fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x48d8f0)(list, i);
            rec = record_at(list, i);
        }
        zs_set_path3(out, at<const char *>(list + 0x38), rec, glob<const char *>(0x60f62c));
        return 1;
    }
    // a font or a sequence: folder, "/-", the record's name (the record loaded), "/Properties.txt"
    auto folder_record = [&](DWORD list, DWORD i, bool inline_loader) {
        zs_assign_text(out, at<const char *>(list + 0x38));
        zs_append(out, glob<const char *>(0x61804c));   // "/-"
        const char *rec = nullptr;
        if (gd(list + 0x30)) {
            if (inline_loader)
                record_inline(list, i);
            else
                record_loaded(list, i);
            rec = record_at(list, i);
        }
        zs_append(out, rec);
        zs_append(out, glob<const char *>(0x61803c));   // "/Properties.txt"
        return (DWORD)1;
    };
    if (is_kind_of(obj, fn_addr(0x4a5910))) {
        const DWORD i = record_of(glob_addr(FONT_LIST), obj);
        return i == 0xffffffff ? 0 : folder_record(glob_addr(FONT_LIST), i, false);
    }
    if (!is_kind_of(obj, fn_addr(0x56ab50)))
        return 0;
    DWORD i = record_of(glob_addr(0x6c0948), obj);
    if (i != 0xffffffff)
        return folder_record(glob_addr(0x6c0948), i, false);
    i = record_of(glob_addr(0x6c06a0), obj);
    return i == 0xffffffff ? 0 : folder_record(glob_addr(0x6c06a0), i, true);
}

// ---- The model preview's slots 2 and 3 ----------------------------------------------------------------------------

// Slot 2 (dt): for an animated entity (+0xc): the timer +0x14 counts down by dt (a new animation when it ran out,
// SetModel 0x517a80), the frame advances by the sequence's rate (slot 26) times dt and wraps at the frame count
// (+0x48); the entity turns by dt times 0x5faea0 (kept above 0 by adding 2 pi) unless it is a walking entity
// standing "Idle"; the children follow
void THISCALL ModelPreview_Update(BYTE *self, float dt)   // 0x517f20
{
    if (!is_kind_of(m(self, 0xc), fn_addr(0x48dcc0)))
        return;
    BYTE *e = bp(m(self, 0xc));
    float frame = fstp(vc<long double>(e, 84));
    const DWORD model = fn<DWORD(THISCALL *)(BYTE *)>(0x48b290)(e);
    BYTE *seq = fn<BYTE *(THISCALL *)(DWORD, DWORD)>(0x5173f0)(model, fn<DWORD(THISCALL *)(BYTE *)>(0x48aed0)(e));
    const long double timer = fld(m<float>(self, 0x14)) - fld(dt);
    m<float>(self, 0x14) = fstp(timer);
    if (c03(timer, cst(glob_addr(C_ZERO))))
        fn<void(THISCALL *)(BYTE *, DWORD)>(0x517a80)(self, 0);
    long double v = vc<long double>(seq, 26) * fld(dt) + fld(frame);
    frame = fstp(v);
    const float count = fstp(fild64((long long)m(seq, 0x48)));
    if (!c0(v, fld(count))) {
        do
            v = v - fld(count);
        while (!c0(v, fld(count)));
        frame = fstp(v);
    }
    fn<void(THISCALL *)(BYTE *, DWORD)>(0x48feb0)(e, float_bits(frame));
    float angle = fstp(vc<long double>(e, 46));
    bool turn = true;
    if (is_kind_of(m(self, 0xc), fn_addr(0x49b810))) {
        const char *name = fn<const char *(THISCALL *)(BYTE *)>(0x48aee0)(e);
        if (fn<int(__cdecl *)(const char *, const char *)>(0x5d3640)(name, glob<const char *>(0x60f8d0)) == 0)   // "Idle"
            turn = false;
    }
    if (turn) {
        long double a = fld(angle) - fld(dt) * cst(glob_addr(0x5faea0));
        angle = fstp(a);
        if (c0(a, cst(glob_addr(C_ZERO)))) {
            do
                a = a + cst(glob_addr(C_TWO_PI));
            while (c0(a, cst(glob_addr(C_ZERO))));
            angle = fstp(a);
        }
    }
    vc(e, 104, float_bits(angle), 1);
    fn<void(THISCALL *)(BYTE *)>(0x517e70)(e);
}

// Slot 3 (out): for an animated entity the union of its rectangles (slot 37) over every frame (+0x48) and rotation
// (+0x4c) of its sequence, each set through SetFrame and slot 104 (the rotation's angle, 0x55fe90); a rectangle
// counts as empty while all four values are 0. Otherwise the object's own slot 37 (out, 0, 0)
DWORD THISCALL ModelPreview_Bounds(BYTE *self, int *out)   // 0x518080
{
    if (!is_kind_of(m(self, 0xc), fn_addr(0x48dcc0))) {
        BYTE *o = bp(m(self, 0xc));
        return vc(o, 37, out, 0, 0);
    }
    BYTE *e = bp(m(self, 0xc));
    int left = 0, top = 0, right = 0, bottom = 0;
    const DWORD model = fn<DWORD(THISCALL *)(BYTE *)>(0x48b290)(e);
    const DWORD seq = fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x5173f0)(model, fn<DWORD(THISCALL *)(BYTE *)>(0x48aed0)(e));
    const DWORD frames = gd(seq + 0x48), turns = gd(seq + 0x4c);
    for (DWORD i = 0; i < frames; i++) {
        if (!turns)
            continue;
        const float f = fstp(fild64((long long)i));
        for (DWORD j = 0; j < turns; j++) {
            fn<void(THISCALL *)(BYTE *, DWORD)>(0x48feb0)(e, float_bits(f));
            const DWORD vt = m(e, 0);
            const float angle = fstp(fn<long double(THISCALL *)(DWORD, DWORD)>(0x55fe90)(seq, j));
            at<void(THISCALL *)(BYTE *, DWORD, DWORD)>(m(bp(vt), 0x1a0))(e, float_bits(angle), 1);
            fn<void(THISCALL *)(BYTE *)>(0x517e70)(e);
            int r[4];
            vc(bp(m(self, 0xc)), 37, r, 0, 0);
            if (!left && !right && !top && !bottom) {
                left = r[0], top = r[1], right = r[2], bottom = r[3];
            } else {
                if (left >= r[0])
                    left = r[0];
                top = top < r[1] ? top : r[1];
                right = right > r[2] ? right : r[2];
                bottom = bottom > r[3] ? bottom : r[3];
            }
        }
    }
    out[0] = left, out[1] = top, out[2] = right, out[3] = bottom;
    return pv(out);
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(p5_leftovers)
{
    const Hook list[] = {
#include "p5_leftovers_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
