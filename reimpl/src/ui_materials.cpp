// The materials generator 0x502960-0x505d30 (docs/DECOMPILATION.md 9, "Phase 4 leftovers"): the crafting window the
// action CShowMaterialsGeneratorAction 0x5b53a0 opens (CMaterialsGeneratorWindow, 0x140 bytes, vtable 0x5f9ccc), its
// grid of buildable items (0x158, vtable 0x5f9a4c, parent GridControl), its description window (0x10c, vtable
// 0x5f9b9c), the preload of its images and fonts, and the ten LocalStrings of its labels. Every call goes through its
// exe address, the calls between the functions here too.
//
// The option object "Materials Generator Display" (CZaxCustomRules::MaterialsGeneratorDisplay 0x5a1cd0, 0xf4 bytes)
// [confirmed: the functions below]: +8 the window's top, +0xc/+0x10 the header's position, +0x14/+0x1c its width and
// height, +0x18 the grid's height (+ +0x1c), +0x20 the row height, +0x24 the header's and the grid's left offset,
// +0x28/+0x2c the text offsets of the "Usage" and the item column, +0x30-+0x40 the five column widths, +0x44/+0x48
// the description's position, +0x4c/+0x50 its size, +0x54/+0x58 and +0x5c/+0x60 its scroll bar's position and
// size, +0x64 the description's border, +0x68 its font, +0x6c/+0x70 the sounds of a new and of a known item, +0x74
// the sound when the generator is done, +0x78/+0x88 two colours, +0x98-+0xa4 the grid's scroll bar, +0xa8-+0xd4
// the four tab buttons and the "Generate" and "Done" buttons (x, y each), +0xd8-+0xec the column fonts, +0xf0 an
// action run after an item is made.
// The window: +0x100 the background ("Materials Generator Frame"), +0x104 the options, +0x108 the description,
// +0x10c its scroll bar, +0x110 an object deleted with the window, +0x114 "Generate", +0x118 "Done", +0x11c-+0x128
// the tabs (weapons, shields, power, miscellaneous), +0x12c the header, +0x130 the grid, +0x134 the grid's scroll
// bar, +0x138 the application's +0x340 (the HUD bar's slide: +0x128 its state, +0x130 a float), +0x13c an item is
// being made (the slide runs), +0x13d close requested. The window is also the global 0x6c2cfc.
// The grid: +0x100 the header, +0x124 the selected row, +0x12c include (1) or exclude (0) the categories
// +0x130-+0x140, +0x144 the rows {vtable 0x5f9b90, data, count, capacity} of 8 bytes {definition, affordable},
// +0x154 the options.
// The item definitions (CZaxInventoryItemDefinition): +0x10 the icon, +0x18 the category, +0x1c the default item,
// +0x2c the "Usage" item, +0x4c the "Usage" number, +0x9c must be discovered, +0xa4/+0xa8, +0xac/+0xb0,
// +0xb4/+0xb8 three {count, mineral} costs, +0xf8 a required item.
#include "ui_screens.h"

namespace zax::uimat {

using namespace ui;

constexpr DWORD CATALOGUE = 0x6c0c08, ICONS = 0x6c0aa8, CATEGORIES = 0x6c0800, TRACKS = 0x6c22e0;
constexpr DWORD G_WINDOW = 0x6c2cfc, G_NO_CATEGORY = 0x6c0834 /* the categories' count: no category */, MASK = 0x6bcc68, MODE_2 = 0x6bb4e0;
constexpr DWORD MAT_GRID_VT = 0x5f9a4c, ROWS_VT = 0x5f9b90, DETAIL_VT = 0x5f9b9c, WINDOW_VT = 0x5f9ccc, BASE_VT = 0x5ea72c;

inline DWORD cat_count() { return gd(glob_addr(CATALOGUE) + 0x34); }
inline DWORD cat_records() { return gd(glob_addr(CATALOGUE) + 0x30); }
inline void load_record(DWORD list, DWORD i) { zax::fn<void(THISCALL *)(DWORD, DWORD)>(0x55e090)(list, i); }
inline DWORD def_object(DWORD list, DWORD i) { return zax::fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x48d8f0)(list, i); }
inline bool kind_of(DWORD obj, DWORD ci) { return (BYTE)zax::fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x416790)(obj, ci); }
inline DWORD rules_class() { return zax::fn<DWORD(__cdecl *)()>(0x5a1350)(); }
inline DWORD display_of(DWORD rules) { return zax::fn<DWORD(THISCALL *)(DWORD)>(0x5a1cd0)(rules); }
inline void section_begin(DWORD text) { zax::fn<void(__fastcall *)(DWORD)>(0x4a2650)(text); }
inline DWORD section_end() { return zax::fn<DWORD(__cdecl *)()>(0x4a2670)(); }
inline DWORD first_character() { return zax::fn<DWORD(THISCALL *)(DWORD)>(0x4f5d80)(gd(glob_addr(G_APP))); }
inline DWORD inventory_of(DWORD part) { return zax::fn<DWORD(THISCALL *)(DWORD)>(0x4267e0)(part); }
inline DWORD net_name(DWORD def) { return zax::fn<DWORD(THISCALL *)(DWORD)>(0x5b7aa0)(def); }
inline DWORD find_category(DWORD name) { return zax::fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x523df0)(glob_addr(CATEGORIES), at<const char *>(name), -1); }
inline void play_index(DWORD i) { zax::fn<void(THISCALL *)(DWORD, DWORD)>(0x57e890)(glob_addr(G_SOUNDS), i); }
inline void set_opacity(DWORD mode, float v) { zax::fn<void(THISCALL *)(DWORD, float)>(0x433be0)(mode, v); }
inline void blit_at(DWORD frame, DWORD src, int x, int y, DWORD mode) { zax::fn<void(THISCALL *)(DWORD, DWORD, int, int, DWORD)>(0x4b3280)(frame, src, x, y, mode); }
inline int draw_wrapped(DWORD font, DWORD frame, int x, int y, const char *t, int w, int spacing, int just)
{
    return zax::fn<int(THISCALL *)(DWORD, DWORD, int, int, const char *, int, int, int)>(0x4a6790)(font, frame, x, y, t, w, spacing, just);
}
inline int font_height(DWORD font) { return zax::fn<int(__fastcall *)(DWORD)>(0x4a6be0)(font); }
inline void draw_int(DWORD font, DWORD frame, int x, int y, int v, int just) { zax::fn<void(THISCALL *)(DWORD, DWORD, int, int, int, int)>(0x4a6b90)(font, frame, x, y, v, just); }
inline void draw_text(DWORD font, DWORD frame, int x, int y, const char *t, int just, int fixed)
{
    zax::fn<void(THISCALL *)(DWORD, DWORD, int, int, const char *, int, int)>(0x4a6470)(font, frame, x, y, t, just, fixed);
}
inline void display_name(DWORD i, void *z) { zax::fn<void(__fastcall *)(DWORD, void *)>(0x4d66d0)(i, z); }
inline void description(DWORD i, void *z) { zax::fn<void(__fastcall *)(DWORD, void *)>(0x4d67c0)(i, z); }
inline int column_left(DWORD header, int c) { return zax::fn<int(THISCALL *)(DWORD, int)>(0x402950)(header, c); }
inline DWORD column_width(DWORD header, int c) { return zax::fn<DWORD(THISCALL *)(DWORD, int)>(0x4028f0)(header, c); }
inline void set_size(const void *w, DWORD a, DWORD b) { zax::fn<void(THISCALL *)(const void *, DWORD, DWORD)>(0x40d510)(w, a, b); }
inline void set_content_size(DWORD w, DWORD a, DWORD b) { zax::fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x40d550)(w, a, b); }
inline void set_range_from(DWORD bar, DWORD w) { zax::fn<void(THISCALL *)(DWORD, DWORD)>(0x413ce0)(bar, w); }
inline void set_image(DWORD b, DWORD img) { zax::fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD)>(0x40ffd0)(b, img, 1, 0); }
inline void clear_tip(DWORD b) { zax::fn<void(THISCALL *)(DWORD, DWORD)>(0x4101c0)(b, 1); }
inline DWORD list_index(DWORD list, DWORD name) { return vc(at<void *>(list), 3, at<const char *>(name), -1); }
// The catalogue index of a definition (-1 none)
inline DWORD catalogue_index(DWORD def)
{
    for (DWORD i = 0; i < cat_count(); i++)
        if (gd(cat_records() + 0x78 + (i << 7)) == def)
            return i;
    return 0xffffffff;
}
// The options of the rules object when it is a CZaxCustomRules (the guarded form: nothing without one)
inline DWORD display_guarded()
{
    BYTE *holder = bp(m(bp(gd(glob_addr(G_APP))), 0x2e0));
    if (!m(holder, 0x38))
        return 0;
    const DWORD ci = rules_class();
    if (!kind_of(m(holder, 0x38), ci) || !m(holder, 0x38))
        return 0;
    return display_of(m(holder, 0x38));
}

// ---- The grid of buildable items --------------------------------------------------------------------------------

BYTE *THISCALL MatGrid_Ctor(BYTE *self, DWORD parent, DWORD header)   // 0x502960
{
    zax::fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x401000)(self, parent, header);
    m(self, 0x144) = vt(ROWS_VT);
    m(self, 0x148) = 0;
    m(self, 0x14c) = 0;
    m(self, 0x150) = 0;
    m(self, 0) = vt(MAT_GRID_VT);
    if (const DWORD d = display_guarded()) {
        m(self, 0x154) = d;
        zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x401f70)(self, m(bp(d), 0x20));
        BYTE *o = bp(m(self, 0x154));
        set_size(self, m(o, 0x14), m(o, 0x18) - m(o, 0x1c));
        for (DWORD off = 0x130; off <= 0x140; off += 4)
            m(self, off) = gd(glob_addr(G_NO_CATEGORY));
        zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x401ca0)(self, m(self, 0x154) + 0x88);
        zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x40cfe0)(self, m(self, 0x154) + 0x78);
    }
    return self;
}

BYTE *THISCALL MatGrid_Dtor(BYTE *self, DWORD flags)   // 0x502a80, slot 0
{
    m(self, 0) = vt(MAT_GRID_VT);
    BYTE *rows = self + 0x144;
    vc(rows, 1);
    m(rows, 0) = vt(ROWS_VT);
    if (const DWORD d = m(rows, 4)) {
        op_delete(bp(d));
        m(rows, 4) = 0;
        m(rows, 0xc) = 0;
    }
    m(rows, 8) = 0;
    window_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x158);
    return self;
}

DWORD THISCALL MatGrid_Notify(BYTE *self, DWORD from, DWORD code)   // 0x502af0, slot 21
{
    return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x401f90)(self, from, code);
}

// Slot 77: a cell (row, column, frame, rect): 0 the icon, 1 the name, 2 the "Usage" (the item it makes or a
// number), 3 the minerals' icons and names, 4 their counts; a row the player cannot afford at opacity 0.4
void THISCALL MatGrid_DrawCell(BYTE *self, DWORD row, DWORD column, DWORD frame, const int *rect)   // 0x502b10
{
    DWORD label;
    zs_ctor(&label);
    const int left = rect[0], top = rect[1];
    if (!mb(bp(m(self, 0x148)), row * 8 + 4)) {
        set_opacity(glob_addr(MASK), 0.4f);
        set_opacity(glob_addr(MODE_2), 0.4f);
    }
    BYTE *disp = bp(m(self, 0x154));
    BYTE *item = bp(m(bp(m(self, 0x148)), row * 8));
    const DWORD header = m(self, 0x100);
    int tx = column_left(header, 3);
    tx += (int)column_width(header, 3);
    tx = tx - 0xc + (left - mi(disp, 0x24));
    const int tx2 = tx - 2;
    const DWORD font = m(self, 0x7c);
    const DWORD dy = (DWORD)(0xc - m(bp(font), 0x80)) >> 1;
    const DWORD costs = (m(item, 0xa8) && m(item, 0xa4)) + (m(item, 0xb0) && m(item, 0xac)) + (m(item, 0xb8) && m(item, 0xb4));
    DWORD name;
    switch (column) {
    case 0: {
        const DWORD icon = record_loaded(glob_addr(ICONS), m(item, 0x10));
        const int y = (mi(disp, 0x20) - 0x24) / 2 + top;
        const int x = ((int)column_width(header, 0) - 0x24) / 2 + left;
        blit_at(frame, icon, x, y, glob_addr(MASK));
        goto done;
    }
    case 1: {
        zs_ctor(&name);
        display_name(catalogue_index(pv(item)), &name);
        const char *t = c_str(&name);
        int w = (int)column_width(header, 1) - 4;
        const int h1 = draw_wrapped(font, 0, left, top, t, w, 2, 1);
        t = c_str(&name);
        w = (int)column_width(header, 1) - 4;
        const int h2 = font_height(font);
        const int y = (h2 - h1) / 2 + mi(disp, 0x2c) + top;
        const DWORD half = column_width(header, 1) >> 1;
        const int x = column_left(header, 1) + (left + (int)half);
        draw_wrapped(font, frame, x, y, t, w, 2, 1);
        break;
    }
    case 2: {
        if (const DWORD made = m(item, 0x2c)) {
            if (def_object(glob_addr(CATALOGUE), made) == pv(item))
                goto done;
            const DWORD f = record_loaded(glob_addr(FONTS), m(disp, 0xe0));
            zs_ctor(&name);
            display_name(made, &name);
            const char *t = c_str(&name);
            int w = (int)column_width(header, 2) - 4;
            const int h1 = draw_wrapped(f, 0, left, top, t, w, 2, 1);
            t = c_str(&name);
            w = (int)column_width(header, 2) - 4;
            const int h2 = font_height(font);
            const int y = (h2 - h1) / 2 + mi(disp, 0x28) + top;
            const DWORD half = column_width(header, 2) >> 1;
            const int x = column_left(header, 2) + (left + (int)half);
            draw_wrapped(f, frame, x, y, t, w, 2, 1);
            break;
        }
        const long double v = m<float>(item, 0x4c), c = *glob<float *>(0x5ebed0);
        if (!(v < c || v > c))
            goto done;
        const DWORD f = record_loaded(glob_addr(FONTS), m(disp, 0xe4));
        const int n = x87_ftol(v);
        const int y = mi(disp, 0x28) + top;
        const DWORD half = column_width(header, 2) >> 1;
        const int x = column_left(header, 2) + (left + (int)half);
        draw_int(f, frame, x, y, n, 1);
        goto done;
    }
    case 3: {
        DWORD y = (DWORD)(rect[3] - (int)costs * 14 - rect[1] + 2) >> 1;
        zs_ctor(&name);
        for (int k = 0; k < 3; k++) {
            const DWORD mineral = m(item, 0xa8 + 8 * k);
            if (!mineral || !m(item, 0xa4 + 8 * k))
                continue;
            const DWORD def = k == 0 ? def_object(glob_addr(CATALOGUE), mineral) : record_loaded(glob_addr(CATALOGUE), mineral);
            const DWORD icon = record_loaded(glob_addr(ICONS), m(bp(def), 0x14));
            blit_at(frame, icon, tx, (int)y + top, glob_addr(MASK));
            display_name(m(item, 0xa8 + 8 * k), &name);
            zs_format(&label, glob<const char *>(0x61950c), c_str(&name));
            const char *t = c_str(&label);
            draw_text(font, frame, tx2, (int)(y + dy) + top, t, 2, 0);
            if (k < 2)
                y += 14;
        }
        break;
    }
    case 4: {
        DWORD y = (DWORD)(rect[3] - (int)costs * 14 - rect[1] + 2) >> 1;
        zs_ctor(&name);
        for (int k = 0; k < 3; k++) {
            const DWORD count = m(item, 0xa4 + 8 * k);
            if (!m(item, 0xa8 + 8 * k) || !count)
                continue;
            const int yy = (int)(y + dy) + top;
            const int x = column_left(header, 4) + mi(disp, 0x24) + left;
            draw_int(font, frame, x, yy, (int)count, 0);
            if (k < 2)
                y += 14;
        }
        break;
    }
    default: goto done;
    }
    zs_dtor(&name);
done:
    set_opacity(glob_addr(MASK), 1.0f);
    set_opacity(glob_addr(MODE_2), 1.0f);
    zs_dtor(&label);
}

// Generate: the selected row's item is made (when affordable), the quest's counter 9 counts it, the rows again
void THISCALL MatGrid_Generate(BYTE *self)   // 0x503430
{
    const DWORD sel = m(self, 0x124);
    if (sel == 0xffffffff || !mb(bp(m(self, 0x148)), sel * 8 + 4))
        return;
    section_begin(glob_addr(0x619510));
    const DWORD gt = zax::fn<DWORD(THISCALL *)(DWORD)>(0x59ff80)(gd(glob_addr(G_APP)));
    if (gt) {
        const DWORD ci = zax::fn<DWORD(__cdecl *)()>(0x551c20)();
        if (kind_of(gt, ci)) {
            const DWORD stat = zax::fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x552940)(gt, 9);
            if (stat) {
                const DWORD ci2 = zax::fn<DWORD(__cdecl *)()>(0x554ae0)();
                if (kind_of(stat, ci2)) {
                    const DWORD data = zax::fn<DWORD(__cdecl *)()>(0x5525b0)();
                    zax::fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x555930)(stat, m(bp(m(bp(data), 0xc)), 0x24), 1);
                }
            }
        }
    }
    zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x503960)(self, m(bp(m(self, 0x148)), m(self, 0x124) * 8));
    zax::fn<void(THISCALL *)(BYTE *, int, int)>(0x401930)(self, -1, 0);
    zax::fn<void(THISCALL *)(BYTE *)>(0x5034f0)(self);
    section_end();
}

// The rows: every catalogue definition of the included (or not excluded) categories the character may build,
// with whether its minerals suffice; sorted by their costs
void THISCALL MatGrid_Fill(BYTE *self)   // 0x5034f0
{
    const DWORD character = first_character();
    const DWORD inv = inventory_of(first_character());
    BYTE *rows = self + 0x144;
    vc(rows, 1);
    for (DWORD i = 0; i < cat_count(); i++) {
        DWORD recs = cat_records();
        if (!recs)
            continue;
        const DWORD k = i < cat_count() ? i : 0;
        const DWORD rec = k << 7;
        if (gb(recs + rec + 0x7f))
            continue;
        if (!gd(recs + rec + 0x78)) {
            load_record(glob_addr(CATALOGUE), k);
            recs = cat_records();
        }
        BYTE *def = bp(gd(recs + rec + 0x78));
        if (!def)
            continue;
        const DWORD ci = zax::fn<DWORD(__cdecl *)()>(0x5b5f10)();
        if (!kind_of(pv(def), ci))
            continue;
        bool ok = true;
        BYTE afford = 1;
        const DWORD cat = m(def, 0x18);
        const bool listed = cat == m(self, 0x130) || cat == m(self, 0x134) || cat == m(self, 0x138) || cat == m(self, 0x13c) ||
                            cat == m(self, 0x140);
        if (mb(self, 0x12c) ? !listed : listed)
            ok = false;
        if (!i)
            ok = false;
        DWORD dflt = m(def, 0x1c);
        if (dflt) {
            const DWORD ci2 = zax::fn<DWORD(__cdecl *)()>(0x429e50)();
            if (!kind_of(dflt, ci2))
                dflt = 0;
        }
        if (!(BYTE)vc(def, 25, character, inv, dflt, 0))
            ok = false;
        if (m(def, 0x1c)) {
            const DWORD o = net_name(pv(def));
            if (!(BYTE)vc(bp(o), 32, character))
                ok = false;
        }
        if (!(m(def, 0xa4) && m(def, 0xa8)) && !(m(def, 0xac) && m(def, 0xb0)) && !(m(def, 0xb4) && m(def, 0xb8)))
            ok = false;
        if (mb(def, 0x9c) && !(BYTE)zax::fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x426650)(inv, i))
            ok = false;
        if (m(def, 0xf8) && zax::fn<int(THISCALL *)(DWORD, DWORD)>(0x4257a0)(inv, m(def, 0xf8)) == -1)
            ok = false;
        for (DWORD c = 0; c < 3; c++) {
            const DWORD mineral = m(def, 0xa8 + 8 * c);
            if (!mineral)
                continue;
            const DWORD need = m(def, 0xa4 + 8 * c);
            if (zax::fn<DWORD(__fastcall *)(DWORD, DWORD)>(0x5aef60)(inv, mineral) < need)
                afford = 0;
        }
        if (!ok)
            continue;
        // append {definition, affordable} (the template's heap flavour: grows by the factor 0x5ea5f4)
        DWORD want = m(rows, 8) + 1;
        if (m(rows, 0xc) < want) {
            const DWORD grown = (DWORD)x87_ftol((long double)(unsigned long long)m(rows, 0xc) * (long double)*glob<float *>(0x5ea5f4));
            if (grown > want)
                want = grown;
            if (m(rows, 0xc) < want) {
                m(rows, 0xc) = want;
                BYTE *d = op_new(want * 8);
                for (DWORD j = 0; j < m(rows, 8); j++) {
                    BYTE *from = bp(m(rows, 4)) + j * 8;
                    m(d, j * 8) = m(from, 0);
                    m(d, j * 8 + 4) = m(from, 4);
                }
                if (m(rows, 4))
                    op_delete(bp(m(rows, 4)));
                m(rows, 4) = pv(d);
            }
        }
        BYTE *e = bp(m(rows, 4)) + m(rows, 8) * 8;
        m(rows, 8) = m(rows, 8) + 1;
        m(e, 0) = pv(def);
        // the icon: marked as used, loaded, and once (+0x7e) told so (slot 7)
        const DWORD icon = m(def, 0x10);
        DWORD r = gd(glob_addr(ICONS) + 0x30);
        if (r) {
            const DWORD off = icon << 7;
            gb(r + off + 0x7d) = 1;
            r = gd(glob_addr(ICONS) + 0x30);
            if (r) {
                const DWORD c = icon < gd(glob_addr(ICONS) + 0x34) ? icon : 0;
                if (!gb(r + (c << 7) + 0x7f) && !gd(r + (c << 7) + 0x78)) {
                    load_record(glob_addr(ICONS), c);
                    r = gd(glob_addr(ICONS) + 0x30);
                }
            }
            if (!gb(r + off + 0x7e)) {
                gb(r + off + 0x7e) = 1;
                vc(bp(record_loaded(glob_addr(ICONS), icon)), 7);
            }
        }
        mb(e, 4) = afford;
    }
    if (m(self, 0x14c))
        zax::fn<void(__cdecl *)(DWORD, DWORD, DWORD, DWORD)>(0x5d097c)(m(self, 0x148), m(self, 0x14c), 8, fn_addr(0x503910));
    zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x401bb0)(self, m(self, 0x14c));
}

// The rows' order (qsort): by 5 * (second + 2 * third cost count) + first cost count
int __cdecl MatGrid_Compare(const DWORD *pa, const DWORD *pb)   // 0x503910
{
    BYTE *a = bp(*pa), *b = bp(*pb);
    return (int)((m(a, 0xac) + m(a, 0xb4) * 2) * 5 - m(b, 0xb4) * 10 - m(b, 0xac) * 5 - m(b, 0xa4) + m(a, 0xa4));
}

// Make: the minerals and the required item used up, the item (the default item's clone or a new CInventoryItem)
// given to the character's inventory; a new item's sound and the HUD bar's slide, else the known item's sound and
// the inventory's current item; then the options' action
void THISCALL MatGrid_Make(BYTE *, DWORD def_)   // 0x503960
{
    BYTE *def = bp(def_);
    const DWORD character = first_character();
    BYTE *inv = bp(inventory_of(character));
    using UseItems = void(__fastcall *)(DWORD, DWORD, DWORD);
    if (m(def, 0xa8))
        zax::fn<UseItems>(0x426930)(character, m(def, 0xa8), m(def, 0xa4));
    if (m(def, 0xb0))
        zax::fn<UseItems>(0x426930)(character, m(def, 0xb0), m(def, 0xac));
    if (m(def, 0xb8))
        zax::fn<UseItems>(0x426930)(character, m(def, 0xb8), m(def, 0xb4));
    if (m(def, 0xf8))
        zax::fn<UseItems>(0x426930)(character, m(def, 0xf8), 1);
    BYTE *item;
    if (m(def, 0x1c)) {
        const DWORD o = net_name(def_);
        item = bp(zax::fn<DWORD(THISCALL *)(DWORD, DWORD, int)>(0x416760)(o, 0, -1));
    } else {
        BYTE *b = alloc(0x18);
        item = b ? zax::fn<BYTE *(THISCALL *)(BYTE *)>(0x42a2b0)(b) : nullptr;
        vc(item, 19);
    }
    const DWORD index = catalogue_index(def_);
    zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x54fdb0)(item, index);
    vc(item, 7);
    const DWORD c2 = first_character();
    vc(item, 33, 0, c2);
    const DWORD disp = display_of(custom_rules());
    const DWORD focus = zax::fn<DWORD(THISCALL *)(BYTE *)>(0x426070)(inv);
    const DWORD r = vc(inv, 26, focus);
    if (zax::fn<DWORD(__fastcall *)(DWORD)>(0x482de0)(r) == index) {
        play_index(m(bp(disp), 0x6c));
    } else {
        const int k = zax::fn<int(THISCALL *)(BYTE *, DWORD)>(0x425900)(inv, def_);
        if (k != -1) {
            play_index(m(bp(disp), 0x6c));
            mi(inv, 0xc) = k;
        } else {
            play_index(m(bp(disp), 0x70));
            if (gd(glob_addr(G_WINDOW)))
                mb(bp(gd(glob_addr(G_WINDOW))), 0x13c) = 0;
            if (const DWORD slide = m(bp(gd(glob_addr(G_APP))), 0x340))
                m<float>(bp(m(bp(slide), 0x128)), 0x130) = 1.0f;
        }
    }
    if (item)
        vc(item, 1, 1);
    if (m(bp(disp), 0xf0)) {
        BYTE *ch = bp(character);
        zax::fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x41a720)(m(bp(disp), 0xf0), m(ch, 0x48), character, 0,
                                                                                       character, disp, disp + 0xf0);
    }
}

// ---- The description window ----------------------------------------------------------------------------------

BYTE *THISCALL MatDetail_Ctor(BYTE *self, DWORD parent)   // 0x503b80
{
    window_ctor(self, parent, glob<const char *>(EMPTY_TEXT));
    m(self, 0x100) = 0;
    m(self, 0x104) = 0;
    m(self, 0x108) = 0;
    m(self, 0) = vt(DETAIL_VT);
    mb(self, 0x7b) = 0;
    zax::fn<void(THISCALL *)(BYTE *, const char *)>(0x40cdc0)(self, nullptr);
    const DWORD d = display_of(custom_rules());
    m(self, 0x100) = d;
    vc(bp(d), 7);
    m(self, 0x7c) = record_inline(glob_addr(FONTS), m(bp(m(self, 0x100)), 0x68));
    m(self, 0x108) = m(bp(m(self, 0x100)), 0x64);
    return self;
}

BYTE *THISCALL MatDetail_Dtor(BYTE *self, DWORD flags)   // 0x503d10, slot 0
{
    m(self, 0) = vt(BASE_VT);
    window_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x10c);
    return self;
}

// Slot 43: the text; its height as the content (the scroll info's width kept), scrolled to the top
void THISCALL MatDetail_SetTitle(BYTE *self, const char *text)   // 0x503d40
{
    zax::fn<void(THISCALL *)(BYTE *, const char *)>(0x40cdc0)(self, text);
    if (!text || !*text)
        return;
    const DWORD font = vc(self, 45);
    const int b = mi(self, 0x108);
    const int w = mi(self, 0xc) - 2 * b - mi(self, 4);
    const char *t = vc<const char *>(self, 44);
    const int h = zax::fn<int(THISCALL *)(DWORD, const char *, int, int, int, int, int, int, int, int)>(0x4a7010)(font, t, w, 2, 0, b, b, 0, b, b);
    const int border = mi(self, 0x108);
    const DWORD info = m(self, 0xfc);
    set_content_size(pv(self), info ? m(bp(info), 8) : 0, (DWORD)(h + border));
    zax::fn<void(THISCALL *)(BYTE *, int, int)>(0x40d700)(self, 0, 0);
}

// Slot 4: the children, then the text as paragraphs inside the border
DWORD THISCALL MatDetail_Draw(BYTE *self, DWORD frame)   // 0x503dd0
{
    zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x40b620)(self, frame);
    const DWORD font = vc(self, 45);
    const int b = mi(self, 0x108);
    const int w = mi(self, 0xc) - 2 * b - mi(self, 4);
    const char *t = vc<const char *>(self, 44);
    return zax::fn<DWORD(THISCALL *)(DWORD, DWORD, int, int, const char *, int, int, int, int, int, int, int, int)>(0x4a6ae0)(
        font, frame, 0, mi(self, 0x108), t, w, 2, 0, b, b, 0, b, b);
}

// Slot 15: the scroll bar (+0x104) first
DWORD THISCALL MatDetail_Mouse15(BYTE *self, DWORD a, DWORD b)   // 0x503e30
{
    if (BYTE *bar = bp(m(self, 0x104))) {
        const DWORD r = vc(bar, 15, a, b);
        if ((BYTE)r)
            return r;
    }
    return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40c0b0)(self, a, b);
}

// ---- The window ------------------------------------------------------------------------------------------------

// The image scroll bars' colour and the game's arrow and thumb images
void scroll_bar_images(BYTE *bar)
{
    BYTE colour[0x10];
    const DWORD c = zax::fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(colour, 2, 0x8f, 0x65, 0xff);
    zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x414560)(bar, c);
    mb(bar, 0x164) = 1;
    DWORD hi = record_loaded(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x616ef8)));
    DWORD lo = record_loaded(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x616ee4)));
    zax::fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x4145e0)(bar, lo, hi);
    hi = record_loaded(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x616ecc)));
    lo = record_loaded(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x616eb8)));
    zax::fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x414600)(bar, lo, hi);
    const DWORD thumb = record_loaded(glob_addr(TRACKS), list_index(glob_addr(TRACKS), glob_addr(0x616eb0)));
    zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x414620)(bar, thumb);
}
// A radio button (tab) with its image, no tip, at the options' position
void place_button(BYTE *self, DWORD b, DWORD img, DWORD pos)
{
    set_image(b, img);
    clear_tip(b);
    BYTE *d = bp(m(self, 0x104));
    add_child(self, bp(b), 5, 0, m(d, pos), m(d, pos + 4), 2);
}
inline BYTE *radio_z(BYTE *self, DWORD label, DWORD group)
{
    BYTE *b = alloc(0x138);
    return b ? zax::fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x4109e0)(b, self, label, group) : nullptr;
}
inline DWORD add_column(DWORD header, DWORD name, DWORD width, DWORD font, DWORD just)
{
    return zax::fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x402770)(header, name, width, font, just, 0);
}

BYTE *THISCALL MatWindow_Ctor(BYTE *self)   // 0x504050
{
    DWORD empty;
    const DWORD parent = zax::fn<DWORD(THISCALL *)(DWORD)>(0x4cdf30)(gd(glob_addr(G_REPORTER)));
    window_ctor(self, parent, glob<const char *>(0x619670));
    m(self, 0) = vt(WINDOW_VT);
    m(self, 0x10c) = 0;
    m(self, 0x110) = 0;
    mb(self, 0x13d) = 0;
    gd(glob_addr(G_WINDOW)) = pv(self);
    mb(self, 0x13c) = 0;
    m(self, 0x138) = m(bp(gd(glob_addr(G_APP))), 0x340);
    section_begin(glob_addr(0x619650));
    if (const DWORD d = display_guarded()) {
        m(self, 0x104) = d;
        vc(bp(d), 7);
    }
    m(self, 0x7c) = record_loaded(glob_addr(FONTS), m(bp(m(self, 0x104)), 0xd8));
    // the description and its scroll bar
    BYTE *det = alloc(0x10c);
    det = det ? zax::fn<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x503b80)(det, self) : nullptr;
    m(self, 0x108) = pv(det);
    BYTE *d = bp(m(self, 0x104));
    vc(det, 59, m(d, 0x4c), m(d, 0x50));
    set_content_size(m(self, 0x108), m(d, 0x4c), m(d, 0x50));
    add_child(self, bp(m(self, 0x108)), 5, 0, m(d, 0x44), m(d, 0x48), 2);
    BYTE *bar = alloc(0x190);
    bar = bar ? zax::fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4144b0)(bar, self, 1) : nullptr;
    m(self, 0x10c) = pv(bar);
    vc(bar, 59, m(d, 0x5c), m(d, 0x60));
    scroll_bar_images(bp(m(self, 0x10c)));
    add_child(self, bp(m(self, 0x10c)), 5, 0, m(d, 0x54), m(d, 0x58), 2);
    m(bp(m(self, 0x10c)), 0x160) = m(self, 0x108);
    set_range_from(m(self, 0x10c), m(self, 0x108));
    m(bp(m(self, 0x108)), 0x104) = m(self, 0x10c);
    // the background gives the size
    BYTE *img = bp(record_loaded(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x619634))));
    m(self, 0x100) = pv(img);
    m(self, 0x114) = 0;
    const DWORD h = vc(img, 39);
    const DWORD w = vc(img, 45);
    set_size(self, w, h);
    // the header with its five columns
    BYTE *hd = alloc(0x140);
    hd = hd ? zax::fn<BYTE *(THISCALL *)(BYTE *, BYTE *)>(0x4020d0)(hd, self) : nullptr;
    m(self, 0x12c) = pv(hd);
    m(hd, 0x128) = m(bp(m(self, 0x104)), 0x24);
    d = bp(m(self, 0x104));
    vc(bp(m(self, 0x12c)), 59, m(d, 0x14), m(d, 0x1c));
    zax::fn<void(THISCALL *)(DWORD, DWORD)>(0x402b40)(m(self, 0x12c), m(self, 0x104) + 0x88);
    zs_ctor_text(&empty, glob<const char *>(EMPTY_TEXT));
    DWORD f = record_loaded(glob_addr(FONTS), m(bp(m(self, 0x104)), 0xd8));
    add_column(m(self, 0x12c), pv(&empty), m(bp(m(self, 0x104)), 0x30), f, 1);
    f = record_loaded(glob_addr(FONTS), m(bp(m(self, 0x104)), 0xdc));
    add_column(m(self, 0x12c), glob_addr(0x6c2cd0), m(bp(m(self, 0x104)), 0x34), f, 1);
    f = record_loaded(glob_addr(FONTS), m(bp(m(self, 0x104)), 0xe0));
    add_column(m(self, 0x12c), glob_addr(0x6c2cd4), m(bp(m(self, 0x104)), 0x38), f, 1);
    f = record_loaded(glob_addr(FONTS), m(bp(m(self, 0x104)), 0xe8));
    add_column(m(self, 0x12c), glob_addr(0x6c2ce8), m(bp(m(self, 0x104)), 0x3c), f, 2);
    f = record_loaded(glob_addr(FONTS), m(bp(m(self, 0x104)), 0xec));
    add_column(m(self, 0x12c), glob_addr(0x6c2cdc), m(bp(m(self, 0x104)), 0x40), f, 0);
    zax::fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x402b90)(m(self, 0x12c), 4, 0);
    mb(bp(m(self, 0x12c)), 0x133) = 1;
    d = bp(m(self, 0x104));
    add_child(self, bp(m(self, 0x12c)), 5, 0, m(d, 0xc), m(d, 0x10), 2);
    // the grid with the weapons' categories
    BYTE *grid = alloc(0x158);
    grid = grid ? zax::fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x502960)(grid, self, m(self, 0x12c)) : nullptr;
    m(self, 0x130) = pv(grid);
    const DWORD none = gd(glob_addr(G_NO_CATEGORY));
    const DWORD ammo = find_category(glob_addr(0x619628));
    const DWORD secondary = find_category(glob_addr(0x60b788));
    const DWORD primary = find_category(glob_addr(0x60b780));
    BYTE *g = bp(m(self, 0x130));
    mb(g, 0x12c) = 1;
    m(g, 0x130) = primary;
    m(g, 0x134) = secondary;
    m(g, 0x138) = ammo;
    m(g, 0x13c) = none;
    m(g, 0x140) = none;
    zax::fn<void(THISCALL *)(BYTE *)>(0x5034f0)(g);
    add_child(self, bp(m(self, 0x130)), 8, 0, 0, 0, 0);
    // the grid's scroll bar
    bar = alloc(0x190);
    bar = bar ? zax::fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x4144b0)(bar, self, 1) : nullptr;
    m(self, 0x134) = pv(bar);
    d = bp(m(self, 0x104));
    vc(bar, 59, m(d, 0xa0), m(d, 0xa4));
    scroll_bar_images(bp(m(self, 0x134)));
    d = bp(m(self, 0x104));
    add_child(self, bp(m(self, 0x134)), 5, 0, m(d, 0x98), m(d, 0x9c), 2);
    zax::fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x401f20)(m(self, 0x130), m(self, 0x134), 1);
    // the tabs
    m(self, 0x11c) = pv(radio_z(self, glob_addr(0x6c2cd8), 0));
    place_button(self, m(self, 0x11c), record_loaded(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x619610))), 0xa8);
    m(self, 0x120) = pv(radio_z(self, glob_addr(0x6c2cf0), m(self, 0x11c)));
    place_button(self, m(self, 0x120), record_loaded(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x6195f8))), 0xb0);
    m(self, 0x124) = pv(radio_z(self, glob_addr(0x6c2cf4), m(self, 0x120)));
    place_button(self, m(self, 0x124), record_loaded(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x6195e0))), 0xb8);
    BYTE *b = alloc(0x138);
    if (b) {
        const char *t = c_str(glob_addr(0x6c2ce0));
        b = zax::fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x4109a0)(b, self, t, m(self, 0x124));
    }
    m(self, 0x128) = pv(b);
    place_button(self, m(self, 0x128), record_inline(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x6195c8))), 0xc0);
    vc(bp(m(self, 0x11c)), 78, 1);
    // "Generate" and "Done"
    m(self, 0x114) = pv(new_button_z(self, glob_addr(0x6c2cec)));
    set_image(m(self, 0x114), record_inline(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x6195b0))));
    zax::fn<void(THISCALL *)(DWORD, DWORD)>(0x4100f0)(m(self, 0x114), record_inline(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x619598))));
    clear_tip(m(self, 0x114));
    d = bp(m(self, 0x104));
    add_child(self, bp(m(self, 0x114)), 5, 0, m(d, 0xc8), m(d, 0xcc), 2);
    m(self, 0x118) = pv(new_button_z(self, glob_addr(0x6c2cf8)));
    place_button(self, m(self, 0x118), record_inline(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x619584))), 0xd0);
    focus(self, bp(m(self, 0x130)));
    zax::fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x505630)(self, m(self, 0x130), 1);
    vc(bp(gd(glob_addr(G_REPORTER))), 23, self, 1);
    if (vc(bp(m(bp(gd(glob_addr(G_REPORTER))), 4)), 45) == 0x280) {
        set_left(self, -0x50);
        set_top(self, (int)m(bp(m(self, 0x104)), 8) - 0x78);
    } else {
        set_left(self, 0);
        set_top(self, (int)m(bp(m(self, 0x104)), 8));
    }
    section_end();
    zs_dtor(&empty);
    return self;
}

BYTE *THISCALL MatWindow_Dtor(BYTE *self, DWORD flags)   // 0x5052e0, slot 0
{
    m(self, 0) = vt(WINDOW_VT);
    if (BYTE *o = bp(m(self, 0x110)))
        vc(o, 1, 1);
    gd(glob_addr(G_WINDOW)) = 0;
    m(self, 0) = vt(BASE_VT);
    window_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x140);
    return self;
}

DWORD __cdecl MatWindow_IsOpen() { return gd(glob_addr(G_WINDOW)) != 0; }   // 0x505330

// Slot 5: closed, the inventory with it
DWORD THISCALL MatWindow_Close(BYTE *self)   // 0x505340
{
    gd(glob_addr(G_WINDOW)) = 0;
    zax::fn<void(THISCALL *)(DWORD)>(0x5a07e0)(gd(glob_addr(G_APP)));
    return zax::fn<DWORD(THISCALL *)(BYTE *)>(0x40bb90)(self);
}

// The images, the fonts and the button images loaded before the window opens
DWORD __cdecl MatWindow_Preload()   // 0x505390
{
    section_begin(glob_addr(0x619650));
    BYTE *disp = nullptr;
    if (const DWORD r = custom_rules()) {
        disp = bp(display_of(r));
        vc(disp, 7);
    }
    def_object(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x616ee4)));
    def_object(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x616ef8)));
    def_object(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x616eb8)));
    def_object(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x616ecc)));
    def_object(glob_addr(TRACKS), list_index(glob_addr(TRACKS), glob_addr(0x616eb0)));
    def_object(glob_addr(SEQUENCES), list_index(glob_addr(SEQUENCES), glob_addr(0x619634)));
    DWORD recs = gd(glob_addr(FONTS) + 0x30);
    const DWORD OFFSETS[5] = {0xd8, 0xdc, 0xe0, 0xe8, 0xec};
    for (DWORD off : OFFSETS) {
        DWORD i = m(disp, off);
        if (!recs)
            continue;
        if (i >= gd(glob_addr(FONTS) + 0x34))
            i = 0;
        const DWORD rec = recs + (i << 7);
        if (!gb(rec + 0x7f) && !gd(rec + 0x78)) {
            load_record(glob_addr(FONTS), i);
            recs = gd(glob_addr(FONTS) + 0x30);
        }
    }
    const DWORD NAMES[7] = {glob_addr(0x619610), glob_addr(0x6195f8), glob_addr(0x6195e0), glob_addr(0x6195c8), glob_addr(0x6195b0), glob_addr(0x619598), glob_addr(0x619584)};
    for (DWORD n : NAMES)
        zax::fn<DWORD(THISCALL *)(DWORD, const char *)>(0x505c20)(glob_addr(SEQUENCES), at<const char *>(n));
    return section_end();
}

// The categories of a tab: the header's "Usage" title, the grid's categories, the rows again
void tab(BYTE *self, DWORD title)
{
    zax::fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x402970)(m(self, 0x12c), 2, title);
    section_begin(glob_addr(0x619694));
}
void tab_done(BYTE *self)
{
    zax::fn<void(THISCALL *)(DWORD)>(0x5034f0)(m(self, 0x130));
    section_end();
    zax::fn<void(THISCALL *)(BYTE *)>(0x505ad0)(self);
}

// Slot 21: "Generate" or a double click makes the item (once per slide of the HUD bar), "Done" closes, a tab
// changes the categories, a new selection the description and "Generate"'s state; the grid's scroll bar goes to the
// grid; then CJWindow's
void THISCALL MatWindow_Notify(BYTE *self, DWORD from, DWORD code)   // 0x505630
{
    if (code == 0) {
        if (from == m(self, 0x114) || from == m(self, 0x130)) {
            if (mb(self, 0x13c))
                return;
            mb(self, 0x13c) = 1;
            m(bp(m(bp(m(self, 0x138)), 0x128)), 0x130) = 0;
            zax::fn<void(THISCALL *)(DWORD)>(0x503430)(m(self, 0x130));
            BYTE *inv = bp(inventory_of(first_character()));
            if (inv) {
                const DWORD c = first_character();
                vc(inv, 24, c, 0);
            }
            return;
        }
        if (from == m(self, 0x118)) {
            vc(self, 5);
            return;
        }
        if (from == m(self, 0x11c)) {
            tab(self, glob_addr(0x6c2cd4));
            const DWORD none = gd(glob_addr(G_NO_CATEGORY));
            const DWORD ammo = find_category(glob_addr(0x619628)), secondary = find_category(glob_addr(0x60b788)), primary = find_category(glob_addr(0x60b780));
            BYTE *g = bp(m(self, 0x130));
            mb(g, 0x12c) = 1;
            m(g, 0x130) = primary, m(g, 0x134) = secondary, m(g, 0x138) = ammo, m(g, 0x13c) = none, m(g, 0x140) = none;
            tab_done(self);
            return;
        }
        if (from == m(self, 0x120)) {
            tab(self, glob_addr(0x6c2cd4));
            const DWORD none = gd(glob_addr(G_NO_CATEGORY));
            const DWORD shield = find_category(glob_addr(0x61968c));
            BYTE *g = bp(m(self, 0x130));
            m(g, 0x130) = shield;
            mb(g, 0x12c) = 1;
            m(g, 0x134) = none, m(g, 0x138) = none, m(g, 0x13c) = none, m(g, 0x140) = none;
            tab_done(self);
            return;
        }
        if (from == m(self, 0x124)) {
            tab(self, glob_addr(0x6c2cf4));
            const DWORD none = gd(glob_addr(G_NO_CATEGORY));
            const DWORD power = find_category(glob_addr(0x619538));
            BYTE *g = bp(m(self, 0x130));
            mb(g, 0x12c) = 1;
            m(g, 0x130) = power;
            m(g, 0x134) = none, m(g, 0x138) = none, m(g, 0x13c) = none, m(g, 0x140) = none;
            tab_done(self);
            return;
        }
        if (from == m(self, 0x128)) {
            tab(self, glob_addr(0x6c2cd4));
            const DWORD power = find_category(glob_addr(0x619538)), shield = find_category(glob_addr(0x61968c)), ammo = find_category(glob_addr(0x619628));
            const DWORD secondary = find_category(glob_addr(0x60b788)), primary = find_category(glob_addr(0x60b780));
            BYTE *g = bp(m(self, 0x130));
            m(g, 0x130) = primary;
            mb(g, 0x12c) = 0;
            m(g, 0x134) = secondary, m(g, 0x138) = ammo, m(g, 0x13c) = shield, m(g, 0x140) = power;
            tab_done(self);
            return;
        }
    } else if (code == 1 && from == m(self, 0x130)) {
        BYTE *g = bp(from);
        const DWORD sel = m(g, 0x124);
        const BYTE on = sel != 0xffffffff && mb(bp(m(g, 0x148)), sel * 8 + 4) ? 1 : 0;
        if (BYTE *create = bp(m(self, 0x114)))
            mb(create, 0x61) = on;
        zax::fn<void(THISCALL *)(BYTE *)>(0x505ad0)(self);
    }
    if (from == m(self, 0x134))
        vc(bp(m(self, 0x130)), 21, from, code);
    zax::fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(0x40c400)(self, from, code);
}

// Slot 4: the background (at the origin either way), then the children; nothing once closing
void THISCALL MatWindow_Draw(BYTE *self, DWORD frame)   // 0x505990
{
    if (mb(self, 0x13d))
        return;
    vc(bp(frame), 45);
    blit_at(frame, m(self, 0x100), 0, 0, glob_addr(MASK));
    zax::fn<void(THISCALL *)(BYTE *, DWORD)>(0x40b620)(self, frame);
}

// Slot 16: a key; Escape closes
DWORD THISCALL MatWindow_Key(BYTE *self, DWORD key)   // 0x505a00
{
    if (!zax::fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40c170)(self, key)) {
        if (key != 0x1b)
            return 0;
        vc(self, 5);
    }
    return 1;
}

// Slot 2: closed when asked; when the HUD bar's slide is done (its float at least 0x5ec3f4) after an item was
// made, the sound and the inventory told; then CJWindow's
DWORD THISCALL MatWindow_Update(BYTE *self, DWORD arg)   // 0x505a30
{
    if (mb(self, 0x13d))
        return vc(self, 5);
    if (mb(self, 0x13c)) {
        const long double v = m<float>(bp(m(bp(m(self, 0x138)), 0x128)), 0x130);
        if (!x87_lt(v, (long double)*glob<float *>(0x5ec3f4))) {
            mb(self, 0x13c) = 0;
            play_index(m(bp(m(self, 0x104)), 0x74));
            BYTE *inv = bp(inventory_of(first_character()));
            if (inv) {
                const DWORD c = first_character();
                vc(inv, 24, c, 0);
            }
        }
    }
    return zax::fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x40b5d0)(self, arg);
}

// The description of the selected row ("name: description") or none
void THISCALL MatWindow_ShowDetails(BYTE *self)   // 0x505ad0
{
    BYTE *g = bp(m(self, 0x130));
    const DWORD sel = m(g, 0x124);
    DWORD def;
    if (sel == 0xffffffff || !(def = m(bp(m(g, 0x148)), sel * 8))) {
        vc(bp(m(self, 0x108)), 43, glob<const char *>(EMPTY_TEXT));
        zax::fn<void(THISCALL *)(DWORD, DWORD, DWORD)>(0x413bb0)(m(self, 0x10c), 0, 0);
        BYTE *bar = bp(m(self, 0x10c));
        m(bar, 0x150) = 0;
        m(bar, 0x154) = 0;
        return;
    }
    const DWORD index = catalogue_index(def);
    DWORD name, desc, text;
    zs_ctor(&name);
    display_name(index, &name);
    zs_ctor(&desc);
    description(index, &desc);
    zs_ctor(&text);
    const char *n = c_str(&name);
    zs_format(&text, glob<const char *>(0x61107c), n, c_str(&desc));
    vc(bp(m(self, 0x108)), 43, c_str(&text));
    set_range_from(m(self, 0x10c), m(self, 0x108));
    zs_dtor(&text);
    zs_dtor(&desc);
    zs_dtor(&name);
}

// The object of a list's record by name, loaded on first use (thiscall list; name)
DWORD THISCALL List_RecordByName(BYTE *list, const char *name)   // 0x505c20
{
    const DWORD i = vc(list, 3, name, -1);
    return record_inline(pv(list), i);
}

// ---- The labels -------------------------------------------------------------------------------------------------

inline int crt_atexit(CodeKey fn) { return zax::fn<int(__cdecl *)(DWORD)>(0x5d0863)(key_fn_addr(fn)); }
inline void local_string(GlobKey z, GlobKey text, const char *context)
{
    zax::fn<DWORD(THISCALL *)(DWORD, const char *, const char *)>(0x501030)(key_glob_addr(z), glob<const char *>(text), context);
}
inline DWORD zstring_exit(GlobKey z) { return zax::fn<DWORD(THISCALL *)(DWORD)>(0x4defd0)(key_glob_addr(z)); }
#define MAT_LABEL(name, z, text, context, exit_fn)       \
    int __cdecl StaticInit_##name()                       \
    {                                                     \
        local_string(z, text, context);                   \
        return crt_atexit(exit_fn);                       \
    }                                                     \
    DWORD __cdecl StaticExit_##name() { return zstring_exit(z); }
MAT_LABEL(MatItem, 0x6c2cd0, 0x60af18, nullptr, 0x503e90)
MAT_LABEL(MatUsage, 0x6c2cd4, 0x619530, nullptr, 0x503ec0)
MAT_LABEL(MatPower, 0x6c2cf4, 0x619538, nullptr, 0x503ef0)
MAT_LABEL(MatMineral, 0x6c2ce8, 0x619540, nullptr, 0x503f20)
MAT_LABEL(MatQty, 0x6c2cdc, 0x619548, nullptr, 0x503f50)
MAT_LABEL(MatGenerate, 0x6c2cec, 0x619550, nullptr, 0x503f80)
MAT_LABEL(MatDone, 0x6c2cf8, 0x60f8d8, nullptr, 0x503fb0)
MAT_LABEL(MatWeapons, 0x6c2cd8, 0x61955c, nullptr, 0x503fe0)
MAT_LABEL(MatShields, 0x6c2cf0, 0x619564, nullptr, 0x504010)
MAT_LABEL(MatMisc, 0x6c2ce0, 0x61956c, glob<const char *>(0x619574), 0x504040)

}  // namespace zax::uimat

namespace zax {

using namespace uimat;

ZAX_HOOKS(ui_materials)
{
    const Hook list[] = {
#include "ui_materials_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
