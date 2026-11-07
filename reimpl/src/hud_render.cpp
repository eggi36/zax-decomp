// The HUD's item renderers (phase 4, rendering): the slots of CInventoryHUDRenderer's classes that keep a
// player's render data up to date (25: update from the player's item, 26: read from a packet) and draw it
// into the HUD (28), the item icon 0x5acab0, the placement helper 0x4298d0, the inventory display options
// 0x429de0 and the energy meter 0x486d10. Classes in hud_classes.cpp, the render data in hud_data.cpp.
//
// How the HUD draws an item [confirmed: CZaxWorld::DrawHud 0x59e730, the slots below]: the net player's
// records (+0x100, 12 bytes {catalogue index, render data, flags}) are walked; each item definition's
// "Display/HUD Renderer" (slot 32, else its type definition's +0x24) gets slot 28 (render data, frame, x
// offset; -0x50 after a shield renderer). Slot 28 places the item by its type definition (0x4298d0: +8/+0xc
// the position, +0x10/+0x14 the horizontal and vertical anchor (1 centre, 2 from the right or bottom edge of
// the frame), +0x18 bit 1: above the HUD bar (minus +0x108 of the application's object +0x340)) and draws:
//  - CCompassRenderer: the "HUD Overlay" sequence (HUD sequence list 0x6c06a0) at the frame the compass
//    points to, pulsing (Transparent Mask opacity · (64 + a triangle of timeGetTime() >> 2) / 128), then the
//    item icon. The frame (slot 25): the angle from the player to the target (0x62e9d8, set by slot 23 from
//    the layer's CZaxLayerSaveData "Compass Target" or the attract compass AI; 0x62e9d1 "target known")
//    through Table_Atan2, minus 0x5ee000, wrapped to [0, 2π), times (frames - 1) · 0x5edff8, rounded, +1.
//  - CFlagInventoryHUDRenderer: the "HUD Sequence" at "Animation Speed" (backwards when negative) through
//    Sequence_SyncFrame (the clock).
//  - CWeaponInventoryHUDRenderer: ammunition as "%d" (continuous fire: "+%d" for whole rounds and the
//    item's energy meter for the fraction), without ammunition the battery's meter; the item icon; "Show
//    Ammo Icon" adds the ammunition's icon after the text.
//  - CShieldInventoryHUDRenderer: per shield item a meter and "%d/%d %s" in "Small Gold", one row per shield.
//  - CEnergyInventoryHUDRenderer (and CCharacterInventoryHUDRenderer with the special ability's name,
//    CSphereOfZinEnergyInventoryHUDRenderer with a pulsing sequence): the energy meter.
// Frames, blend modes, blitters, fonts and sequences stay the exe's (phase 4 batches R1-R7). Floats follow
// the original (x87.h).
#include "ai_helpers.h"
#include "x87.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD ITEM_LIST = 0x6c0c08, ITEM_RECORDS = 0x6c0c38, ITEM_COUNT = 0x6c0c3c;
constexpr DWORD HUD_LIST = 0x6c06a0, HUD_RECORDS = 0x6c06d0, HUD_COUNT = 0x6c06d4, HUD_FOLDER = 0x6c06d8;
constexpr DWORD FONT_LIST = 0x6be1e0, FONT_RECORDS = 0x6be210, FONT_COUNT = 0x6be214, FONT_FOLDER = 0x6be218;
constexpr DWORD MASK_BLEND = 0x6bcc68;   // "Transparent Mask"
constexpr DWORD COMPASS_TARGET = 0x62e9d8, COMPASS_KNOWN = 0x62e9d1;
constexpr DWORD C_TWO_PI_HUD = 0x5edffc, ONE = 0x3f800000;

// A definition list's record object as slots 25 and 28 look it up (DefinitionList_LoadRecord 0x55e090 when
// missing); null without records or while it is loading
BYTE *record_object(DWORD list, DWORD records_at, DWORD count_at, DWORD i)
{
    BYTE *records = *at<BYTE **>(records_at);
    if (!records)
        return nullptr;
    if (i >= *at<DWORD *>(count_at))
        i = 0;
    const DWORD off = i << 7;
    if (records[off + 0x7f])
        return nullptr;
    if (!dw(records + off, 0x78)) {
        fn<void(THISCALL *)(void *, DWORD)>(0x55e090)(at<void *>(list), i);
        records = *at<BYTE **>(records_at);
    }
    return bp(dw(records + off, 0x78));
}
// The same loaded through the list's slot 37 (list_object), as the inline lookups with Path_Join do
BYTE *record_object_loading(DWORD list, DWORD records_at, DWORD count_at, DWORD folder, DWORD i)
{
    if (!*at<BYTE **>(records_at))
        return nullptr;
    return list_object(list, records_at, count_at, folder, i);
}
BYTE *item_definition_at(DWORD i) { return record_object(glob_addr(ITEM_LIST), glob_addr(ITEM_RECORDS), glob_addr(ITEM_COUNT), i); }
BYTE *of_class(BYTE *o, DWORD getter) { return o && is_kind_of(o, class_at(getter)) ? o : nullptr; }

CClassInfo *energy_def_class() { return class_at(fn_addr(0x486220)); }
BYTE *type_definition(BYTE *def) { return fn<BYTE *(THISCALL *)(void *)>(0x5b7af0)(def); }
BYTE *hud_screen() { return bp(dw(*glob<BYTE **>(0x713f14), 0x340)); }
void hud_place(BYTE *def, void *frame, int *x, int *y)
{
    fn<DWORD(THISCALL *)(void *, void *, void *, int *, int *)>(0x4298d0)(hud_screen(), type_definition(def), frame, x, y);
}

void set_opacity(DWORD mode, DWORD bits) { fn<void(THISCALL *)(void *, DWORD)>(0x433be0)(at<void *>(mode), bits); }
long double get_opacity(DWORD mode) { return fn<long double(THISCALL *)(void *)>(0x433c20)(at<void *>(mode)); }
void blit_at(void *frame, void *src, int x, int y, void *mode) { fn<DWORD(THISCALL *)(void *, void *, int, int, void *)>(0x4b3280)(frame, src, x, y, mode); }
void *sequence_frame(BYTE *seq, DWORD i) { return vcall<void *>(seq, 23, i); }
DWORD sequence_frames(BYTE *seq) { return vcall<DWORD>(seq, 25); }
void *sequence_blend(BYTE *seq) { return fn<void *(THISCALL *)(void *)>(0x56bf00)(seq); }
// Sequence_SyncFrame 0x55e4b0 (frames in ecx, offset in edx; frames per second): the frame of the clock
DWORD sync_frame(DWORD frames, DWORD offset, DWORD fps) { return fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x55e4b0)(frames, offset, fps); }
DWORD properties_count(void *ref) { return fn<DWORD(THISCALL *)(void *)>(0x482de0)(ref); }
void data_set_value(BYTE *d, DWORD bits) { fn<void(THISCALL *)(void *, DWORD)>(0x5aca10)(d, bits); }
void data_set_count(BYTE *d, DWORD v) { fn<void(THISCALL *)(void *, DWORD)>(0x5aca30)(d, v); }
// Font: draw text (0x4a6470: frame, x, y, text, a5, a6), its height (0x4a6be0), the width of a text (0x4a7310)
void font_text(BYTE *font, void *frame, int x, int y, const char *text, DWORD a5, DWORD a6)
{
    fn<DWORD(THISCALL *)(void *, void *, int, int, const char *, DWORD, DWORD)>(0x4a6470)(font, frame, x, y, text, a5, a6);
}
DWORD meter_draw(BYTE *meter, void *frame, void *rect, DWORD value, DWORD max, DWORD a5)
{
    return fn<DWORD(THISCALL *)(void *, void *, void *, DWORD, DWORD, DWORD)>(0x486d10)(meter, frame, rect, value, max, a5);
}
void draw_item_icon(BYTE *def, void *frame, int x, int y, DWORD opacity)
{
    fn<void(__stdcall *)(void *, void *, int, int, DWORD)>(0x5acab0)(def, frame, x, y, opacity);
}

// The render data a renderer keeps in *slot: kept when it is of the class (IsKindOf), else the old one
// deleted (slot 1 with 1) and a new one made (pool, the constructor levels inlined, slot 19)
struct Made {
    BYTE *data;
    bool made;
};
void construct_data(BYTE *o, DWORD vtable)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5ede94));
    if (vtable)
        vtable_and_defaults(o, vtable);
}
template <typename C> Made ensure_data(DWORD *slot, bool keep, DWORD size, C construct)
{
    if (keep)
        return {bp(*slot), false};
    if (*slot) {
        vcall<DWORD>(bp(*slot), 1, 1);
        *slot = 0;
    }
    BYTE *o = static_cast<BYTE *>(pool_alloc(size));
    if (o)
        construct(o);
    vcall<DWORD>(o, 19);
    *slot = ptr_value(o);
    return {o, true};
}
bool kind(DWORD slot_value, DWORD getter) { return slot_value && is_kind_of(bp(slot_value), class_at(getter)); }

int sprintf_exe(char *out, const char *fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    const int n = crt_vsprintf(out, fmt, ap);
    __builtin_va_end(ap);
    return n;
}

}  // namespace

// ---- Helpers ---------------------------------------------------------------------------------------------

// 0x429de0: the inventory display options of the custom rules (CZaxCustomRules::InventoryDisplay 0x5a1d40
// on the application's rules when they are CZaxCustomRules, else on null)
static BYTE *__cdecl InventoryDisplayOptions()
{
    BYTE *holder = bp(dw(*glob<BYTE **>(0x713f14), 0x2e0));
    BYTE *rules = bp(dw(holder, 0x38));
    if (rules && is_kind_of(rules, class_at(fn_addr(0x5a1350))))
        return fn<BYTE *(THISCALL *)(void *)>(0x5a1d40)(bp(dw(holder, 0x38)));
    return fn<BYTE *(THISCALL *)(void *)>(0x5a1d40)(nullptr);
}

// 0x4298d0 (HUD object; type definition, frame, x, y): where an item's HUD part goes. eax: y's address
// after the HUD bar correction, else the flags shifted.
static DWORD THISCALL Hud_Place(BYTE *self, BYTE *td, void *frame, int *x, int *y)
{
    int px = (int)dw(td, 8), py = (int)dw(td, 0xc);
    if (dw(td, 0x10) == 1)
        px += (int)(vcall<DWORD>(frame, 45) >> 1);
    else if (dw(td, 0x10) == 2)
        px = (int)vcall<DWORD>(frame, 45) - px;
    if (dw(td, 0x14) == 1)
        py += (int)(vcall<DWORD>(frame, 39) >> 1);
    else if (dw(td, 0x14) == 2)
        py = (int)vcall<DWORD>(frame, 39) - py;
    const DWORD flags = dw(td, 0x18) >> 1;
    *x = px;
    if (flags & 1) {
        *y = py - (int)dw(self, 0x108);
        return ptr_value(y);
    }
    *y = py;
    return flags;
}

// 0x5acab0 (definition, frame, x, y, opacity): the item's icon (definition slot 31, the HUD sequence list's
// frame 0) blitted with the Transparent Mask at the opacity, which is put back to 1 afterwards
static void __stdcall Hud_DrawItemIcon(BYTE *def, void *frame, int x, int y, DWORD opacity)
{
    BYTE *seq = record_object_loading(glob_addr(HUD_LIST), glob_addr(HUD_RECORDS), glob_addr(HUD_COUNT), glob_addr(HUD_FOLDER), vcall<DWORD>(def, 31));
    void *icon = sequence_frame(seq, 0);
    set_opacity(glob_addr(MASK_BLEND), opacity);
    blit_at(frame, icon, x, y, glob<void *>(MASK_BLEND));
    set_opacity(glob_addr(MASK_BLEND), ONE);
}

// 0x486d10 (CEnergyMeter: frame, rectangle, value, maximum, unused). Taller than wide: black, a frame in
// +0x18 one pixel around, the filled part from the bottom in +0x38. Wider: (only for a maximum above
// 0x5f1c3c) black, top and bottom lines in +8, the left and right ends as bars between +8 and +0x18 split
// at a third of the height, the filled part from the left as two bars between +0x28 and +0x38.
static void THISCALL EnergyMeter_Draw(BYTE *self, void *frame, const int *r, DWORD value, DWORD max, DWORD)
{
    using Rect = int[4];
    const DWORD w = (DWORD)(r[2] - r[0]), h = (DWORD)(r[3] - r[1]);
    auto fill = [&](Rect &q, void *c) { vcall<DWORD>(frame, 28, q, c); };
    auto bar = [&](Rect &q, DWORD a, DWORD b) { fn<DWORD(THISCALL *)(void *, void *, void *, void *)>(0x4b3ab0)(frame, q, self + a, self + b); };
    BYTE color[0x10];
    if (w < h) {
        field_color_rgba(ptr_value(color), 0, 0, 0, 0xff);
        const int fh = x87_ftol(fild64(h) / fld(raw_float(max)) * fld(raw_float(value)));
        Rect q = {r[0], r[1], r[2], r[3]};
        fill(q, color);
        q[0]--;
        q[1]--;
        q[2]++;
        q[3]++;
        fn<DWORD(THISCALL *)(void *, void *, void *)>(0x4b3430)(frame, q, self + 0x18);
        Rect f = {r[0], r[3] - fh, r[2], r[3]};
        fill(f, self + 0x38);
        return;
    }
    if (c03(fld(raw_float(max)), cst(glob_addr(0x5f1c3c))))
        return;
    const DWORD fw = (DWORD)x87_ftol(fild64(w) / fld(raw_float(max)) * fld(raw_float(value)));
    fn<void *(THISCALL *)(void *)>(0x53f000)(color);
    fn<void *(THISCALL *)(void *)>(0x53f060)(color);
    Rect q = {r[0], r[1], r[2], r[3]};
    fill(q, color);
    q[0]--;
    q[1]--;
    q[2]++;
    q[3]++;
    const int third = (int)((DWORD)(q[3] - q[1]) / 3);
    vcall<DWORD>(frame, 51, q[0], q[1], q[2] - 1, q[1], self + 8);
    vcall<DWORD>(frame, 51, q[0], q[3] - 1, q[2] - 1, q[3] - 1, self + 8);
    Rect b1 = {q[0], q[1], q[0] + 1, q[1] + third};
    bar(b1, 8, 0x18);
    Rect b2 = {q[0], q[1] + third, q[0] + 1, q[3]};
    bar(b2, 0x18, 8);
    Rect b3 = {q[2] - 1, q[1], q[2], q[1] + third};
    bar(b3, 8, 0x18);
    Rect b4 = {q[2] - 1, q[1] + third, q[2], q[3]};
    bar(b4, 0x18, 8);
    if (fw < 1)
        return;
    const int right = r[0] + (int)fw, mid = r[1] + (r[3] - r[1]) / 3;
    Rect a = {r[0], r[1], right, mid};
    bar(a, 0x28, 0x38);
    Rect b = {r[0], mid, right, r[3]};
    bar(b, 0x38, 0x28);
}

// ---- CCompassRenderer ------------------------------------------------------------------------------------

// 0x423e90 (slot 23: a1, a2, layer): the compass target from the layer's CZaxLayerSaveData "Compass Target"
// (the part's position into 0x62e9d8, 0x62e9d1 set; cleared when the name finds no part)
static DWORD THISCALL Compass_Begin(BYTE *, DWORD, DWORD, BYTE *layer)
{
    BYTE *save = bp(dw(layer, 0x198));
    if (!save || !is_kind_of(save, class_at(fn_addr(0x5b9c00))))
        return 0;
    const char *name = zs_c_str(save + 8);
    BYTE *part = fn<BYTE *(__fastcall *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x41ad80)(layer, name, 0, 0, 0, 0, 0, 0);
    if (!part) {
        *glob<BYTE *>(COMPASS_KNOWN) = 0;
        return 0;
    }
    vcall<DWORD>(part, 43, glob<float *>(COMPASS_TARGET));
    *glob<BYTE *>(COMPASS_KNOWN) = 1;
    return 1;
}

namespace {
void compass_hide(BYTE *d)
{
    if (d[0x14]) {
        d[0x15] = 1;
        d[0x16] = 0;
        d[0x14] = 0;
    }
}
}  // namespace

// 0x423990 (slot 25: part, item reference, opacity, render data slot): al: the render data was made new
static DWORD THISCALL Compass_Update(BYTE *self, BYTE *part, void *ref, DWORD opacity, DWORD *slot)
{
    const Made m = ensure_data(slot, kind(*slot, fn_addr(0x423110)), 0x1c, [](BYTE *o) { construct_data(o, vt(0x5ede28)); });
    BYTE *d = m.data;
    if (*glob<BYTE *>(COMPASS_KNOWN)) {
        BYTE *seq = record_object(glob_addr(HUD_LIST), glob_addr(HUD_RECORDS), glob_addr(HUD_COUNT), dw(self, 8));
        const int n = (int)sequence_frames(seq) - 1;
        if (n > 0) {
            if (d[0x14] != 1) {
                d[0x15] = 1;
                d[0x16] = 1;
                d[0x14] = 1;
            }
            float pos[2];
            vcall<DWORD>(part, 43, pos);
            const float dx = fstp(fld(pos[0]) - cst(glob_addr(COMPASS_TARGET)));
            const float dy = fstp(fld(pos[1]) - cst(glob_addr(COMPASS_TARGET) + 4));
            long double a = fn<long double(__stdcall *)(DWORD, DWORD)>(0x509100)(float_bits(dy), float_bits(dx)) - cst(glob_addr(0x5ee000));
            if (c0(a, cst(glob_addr(C_ZERO))))
                a = a + cst(glob_addr(C_TWO_PI_HUD));
            if (!c0(a, cst(glob_addr(C_TWO_PI_HUD))))
                a = a - cst(glob_addr(C_TWO_PI_HUD));
            const float t = fstp(a * (fild(n) * cst(glob_addr(0x5edff8))));
            int f = x87_fistp(fld(t));
            if (f >= n)
                f -= n;
            f++;
            if (f >= n)
                f = n;
            if (dw(d, 0x18) != (DWORD)f && d[0x14]) {
                d[0x15] = 1;
                d[0x16] = 1;
            }
            dw(d, 0x18) = (DWORD)f;
        } else {
            compass_hide(d);
        }
    } else {
        compass_hide(d);
    }
    data_set_value(d, opacity);
    data_set_count(d, properties_count(ref));
    return m.made;
}
// 0x423b70 (slot 26: packet, render data slot)
static DWORD THISCALL Compass_Read(BYTE *, void *packet, DWORD *slot)
{
    const Made m = ensure_data(slot, kind(*slot, fn_addr(0x423110)), 0x1c, [](BYTE *o) { construct_data(o, vt(0x5ede28)); });
    vcall<DWORD>(m.data, 25, packet);
    return m.made;
}
// 0x423c50 (slot 28: render data, frame, x offset)
static void THISCALL Compass_Draw(BYTE *self, BYTE *data, void *frame, int dx)
{
    BYTE *d = data && is_kind_of(data, class_at(fn_addr(0x423110))) ? data : nullptr;
    void *overlay = nullptr;
    if (d[0x14])
        overlay = sequence_frame(record_object(glob_addr(HUD_LIST), glob_addr(HUD_RECORDS), glob_addr(HUD_COUNT), dw(self, 8)), dw(d, 0x18));
    BYTE *def = of_class(item_definition_at(dw(d, 0xc)), fn_addr(0x5b5f10));
    int x = 0, y = 0;
    hud_place(def, frame, &x, &y);
    x += dx;
    if (overlay) {
        const float saved = fstp(get_opacity(glob_addr(MASK_BLEND)));
        BYTE pulse = (BYTE)(fn<DWORD(__stdcall *)()>(0x4d24b0)() >> 2);
        if (pulse > 0x80)
            pulse = (BYTE)-pulse;
        pulse = (DWORD)pulse + 0x40 >= 0x80 ? 0x80 : (BYTE)(pulse + 0x40);
        set_opacity(glob_addr(MASK_BLEND), float_bits(fstp(fild(pulse) * cst(glob_addr(0x5ee004)) * fld(saved))));
        blit_at(frame, overlay, x, y, glob<void *>(MASK_BLEND));
        set_opacity(glob_addr(MASK_BLEND), float_bits(saved));
    }
    draw_item_icon(def, frame, x, y, dw(d, 8));
}

// ---- CFlagInventoryHUDRenderer ---------------------------------------------------------------------------

namespace {
// The flag's render data is a plain CInventoryHUDRenderData (its class compared exactly)
bool plain_data(DWORD v) { return v && vcall<CClassInfo *>(bp(v), 2) == class_at(fn_addr(0x5ac6b0)); }
}  // namespace

// 0x469e90 (slot 25)
static DWORD THISCALL Flag_Update(BYTE *, void *, void *ref, DWORD opacity, DWORD *slot)
{
    const Made m = ensure_data(slot, plain_data(*slot), 0x14, [](BYTE *o) { construct_data(o, 0); });
    data_set_value(m.data, opacity);
    data_set_count(m.data, properties_count(ref));
    return m.made;
}
// 0x469f20 (slot 26)
static DWORD THISCALL Flag_Read(BYTE *, void *packet, DWORD *slot)
{
    const Made m = ensure_data(slot, plain_data(*slot), 0x14, [](BYTE *o) { construct_data(o, 0); });
    vcall<DWORD>(m.data, 25, packet);
    return m.made;
}
// 0x469fa0 (slot 28): the "HUD Sequence" played at "Animation Speed" (negative: backwards)
static void THISCALL Flag_Draw(BYTE *self, BYTE *data, void *frame, int dx)
{
    BYTE *d = data && vcall<CClassInfo *>(data, 2) == class_at(fn_addr(0x5ac6b0)) ? data : nullptr;
    BYTE *def = of_class(item_definition_at(dw(d, 0xc)), fn_addr(0x5b5f10));
    int x = 0, y = 0;
    hud_place(def, frame, &x, &y);
    x += dx;
    BYTE *seq = record_object_loading(glob_addr(HUD_LIST), glob_addr(HUD_RECORDS), glob_addr(HUD_COUNT), glob_addr(HUD_FOLDER), dw(self, 8));
    if (!seq)
        return;
    const long double speed = vcall<long double>(seq, 26) * fld(fl<float>(self, 0xc));
    float fps = fstp(speed);
    bool backwards = false;
    if (c0(speed, cst(glob_addr(C_ZERO)))) {
        fps = fstp(fld(fps) * cst(glob_addr(0x5eeccc)));
        backwards = true;
    }
    const DWORD n = sequence_frames(seq);
    DWORD f = sync_frame(n, 0, float_bits(fps));
    if (backwards)
        f = n - f - 1;
    void *img = sequence_frame(seq, f);
    if (img)
        blit_at(frame, img, x, y, sequence_blend(seq));
}

// ---- CWeaponInventoryHUDRenderer -------------------------------------------------------------------------

// 0x4859b0 (slot 25): the rounds (continuous fire: the energy source's charge), else the battery's capacity
// and charge
static DWORD THISCALL Weapon_Update(BYTE *, BYTE *part, void *ref, DWORD opacity, DWORD *slot)
{
    const Made m = ensure_data(slot, kind(*slot, fn_addr(0x484d40)), 0x28, [](BYTE *o) { construct_data(o, vt(0x5f1a64)); });
    BYTE *d = m.data;
    BYTE *def = item_definition(ref);
    BYTE *zax = of_class(def, fn_addr(0x5b5f10));
    if (fn<bool(THISCALL *)(void *)>(0x4d77c0)(def)) {
        if (zax && fn<bool(THISCALL *)(void *)>(0x5b7640)(zax)) {
            BYTE *src = fn<BYTE *(THISCALL *)(void *, void *)>(0x5b7920)(zax, part_inventory(part));
            if (src) {
                if (!c3(fld(fl<float>(src, 0x18)), fld(fl<float>(d, 0x18)))) {
                    fl<float>(d, 0x18) = fstp(fld(fl<float>(src, 0x18)));
                    dw(d, 0x24) |= 1;
                }
            } else if (!c3(fld(fl<float>(d, 0x18)), cst(glob_addr(C_ZERO)))) {
                dw(d, 0x18) = 0;
                dw(d, 0x24) |= 1;
            }
        } else {
            const DWORD rounds = fn<DWORD(__fastcall *)(void *, void *)>(0x5aeec0)(part, def);
            if (dw(d, 0x14) != rounds) {
                dw(d, 0x14) = rounds;
                dw(d, 0x24) |= 1;
            }
        }
    } else {
        DWORD charge = 0;
        BYTE *battery = fn<BYTE *(THISCALL *)(void *)>(0x5b0da0)(part_inventory(part));
        long double cap;
        if (battery) {
            charge = dw(battery, 0x18);
            cap = fn<long double(THISCALL *)(void *)>(0x5afe50)(battery);
        } else {
            cap = fld(raw_float(0));
        }
        if (!c3(cap, fld(fl<float>(d, 0x1c)))) {
            fl<float>(d, 0x1c) = fstp(cap);
            dw(d, 0x24) |= 2;
        }
        if (!c3(fld(fl<float>(d, 0x20)), fld(raw_float(charge)))) {
            dw(d, 0x20) = charge;
            dw(d, 0x24) |= 4;
        }
    }
    data_set_value(d, opacity);
    data_set_count(d, properties_count(ref));
    return m.made;
}
// 0x485be0 (slot 26)
static DWORD THISCALL Weapon_Read(BYTE *, void *packet, DWORD *slot)
{
    const Made m = ensure_data(slot, kind(*slot, fn_addr(0x484d40)), 0x28, [](BYTE *o) { construct_data(o, vt(0x5f1a64)); });
    vcall<DWORD>(m.data, 25, packet);
    return m.made;
}

namespace {
BYTE *font_at(DWORD i) { return record_object(glob_addr(FONT_LIST), glob_addr(FONT_RECORDS), glob_addr(FONT_COUNT), i); }
void report_missing_meter()
{
    void *r = reporter();
    vcall<DWORD>(r, 17, zs_c_str(glob<void *>(0x6bdc98)));
}
}  // namespace


// 0x485cc0 (slot 28): the meter rectangle from the inventory display options (+0x238..+0x244 relative to the
// placement); with ammunition the rounds as text at +0x1e0/+0x1e4 ("Primary" items) or +0x1e8/+0x1ec in the
// font +0x1f0; a continuous fire weapon shows "+%d" whole rounds right of the meter (+0x240), the ammunition's
// icon after the text with "Show Ammo Icon", and the fraction in the item's energy meter; without ammunition
// the battery in the item's energy meter; then the item icon
static void THISCALL Weapon_Draw(BYTE *self, BYTE *data, void *frame, int dx)
{
    BYTE *opts = InventoryDisplayOptions();
    BYTE *d = data && is_kind_of(data, class_at(fn_addr(0x484d40))) ? data : nullptr;
    BYTE *def = nullptr, *zax = nullptr;
    {
        BYTE *records = *glob<BYTE **>(ITEM_RECORDS);
        DWORD i = dw(d, 0xc);
        if (records) {
            if (i >= *glob<DWORD *>(ITEM_COUNT))
                i = 0;
            if (!records[(i << 7) + 0x7f]) {
                def = item_definition_at(i);
                zax = of_class(def, fn_addr(0x5b5f10));
            }
        }
    }
    int x = 0, y = 0;
    hud_place(zax, frame, &x, &y);
    x += dx;
    int rect[4];
    rect[0] = (int)dw(opts, 0x238) + x;
    rect[2] = (int)dw(opts, 0x240) + rect[0];
    rect[1] = (int)dw(opts, 0x23c) + y;
    rect[3] = (int)dw(opts, 0x244) + rect[1];
    const bool primary = dw(zax, 0x18) == category_index(glob_addr(0x60b780) /* "Primary" */);
    char text[0x80];
    if (fn<bool(THISCALL *)(void *)>(0x4d77c0)(def)) {
        if (zax && fn<bool(THISCALL *)(void *)>(0x5b7640)(zax)) {
            float frac = fl<float>(d, 0x18);
            DWORD whole = fn<DWORD(__stdcall *)(DWORD)>(0x509310)(dw(d, 0x18));
            const long double fr = fld(frac) - fild64(whole);
            frac = fstp(fr);
            if (c3(fr, cst(glob_addr(C_ZERO))) && whole) {
                frac = raw_float(ONE);
                whole--;
            }
            if (whole) {
                if (whole >= 1 || !self[8])
                    sprintf_exe(text, glob<const char *>(0x61230c) /* "+%d" */, whole);
                BYTE *font = font_at(dw(opts, 0x1f0));
                font_text(font, frame, (int)(dw(opts, 0x240) + dw(opts, 0x1e0)) + x, (int)dw(opts, 0x1e4) + y, text, 0, 0);
                if (self[8]) {
                    BYTE *ammo = of_class(fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(ITEM_LIST), dw(zax, 0x2c)), fn_addr(0x5b5f10));
                    BYTE *seq = record_object(glob_addr(HUD_LIST), glob_addr(HUD_RECORDS), glob_addr(HUD_COUNT), vcall<DWORD>(ammo, 31));
                    void *icon = sequence_frame(seq, 0);
                    const int iy = (int)((fn<DWORD(THISCALL *)(void *)>(0x4a6be0)(font) >> 1) + dw(opts, 0x1e4)) + y;
                    const DWORD half = vcall<DWORD>(icon, 45) >> 1;
                    const DWORD tw = fn<DWORD(THISCALL *)(void *, const char *, int)>(0x4a7310)(font, text, -1);
                    blit_at(frame, icon, (int)(half + tw + dw(opts, 0x240) + dw(opts, 0x1e0)) + x, iy, glob<void *>(MASK_BLEND));
                }
                def = def ? def : nullptr;
            }
            if (def && is_kind_of(def, energy_def_class()))
                meter_draw(def + 0x100, frame, rect, float_bits(frac), ONE, ONE);
            else
                report_missing_meter();
        } else {
            sprintf_exe(text, glob<const char *>(0x60a5a0) /* "%d" */, dw(d, 0x14));
            BYTE *font = font_at(dw(opts, 0x1f0));
            if (primary)
                font_text(font, frame, (int)dw(opts, 0x1e0) + x, (int)dw(opts, 0x1e4) + y, text, 1, 0);
            else
                font_text(font, frame, (int)dw(opts, 0x1e8) + x, (int)dw(opts, 0x1ec) + y, text, 1, 0);
        }
    } else {
        const DWORD charge = dw(d, 0x20), cap = dw(d, 0x1c), meter_max = dw(def, 0x4c);
        if (def && is_kind_of(def, energy_def_class())) {
            const long double v = zax && fn<bool(THISCALL *)(void *)>(0x5b7640)(zax) ? -fld(raw_float(cap)) : fld(raw_float(meter_max));
            meter_draw(def + 0x100, frame, rect, charge, cap, float_bits(fstp(-v)));
        } else {
            report_missing_meter();
        }
    }
    draw_item_icon(def, frame, x, y, float_bits(fstp(fld(fl<float>(d, 8)))));
}

// ---- CShieldInventoryHUDRenderer ---------------------------------------------------------------------

// 0x56ef20 (slot 25): one CEnergyHUDRenderData per shield item of the part's inventory (category "Shield",
// CEnergyItem with a CEnergyItemDefinition, in category order from the first): its charge (+0x18), capacity
// (MaxCharge) and catalogue index; the list cut to the shields found
static DWORD THISCALL Shield_Update(BYTE *, BYTE *part, void *, DWORD, DWORD *slot)
{
    const Made m = ensure_data(slot, kind(*slot, fn_addr(0x56e640)), 0x28, [](BYTE *o) {
        obj_ctor(o);
        vtable_and_defaults(o, vt(0x5ede94));
        fn<void *(THISCALL *)(void *)>(0x56f8a0)(o + 0x18);
        vtable_and_defaults(o, vt(0x5ffcb8));
    });
    BYTE *d = m.data;
    DWORD n = 0;
    BYTE *inv = part_inventory(part);
    const DWORD first = inventory_first_of(inv, category_index(glob_addr(TEXT_SHIELD)));
    BYTE *item = of_class(inventory_item(inv, first), fn_addr(0x5afbd0));
    BYTE *def = item_definition(item);
    if (def && is_kind_of(def, energy_def_class()) && item) {
        DWORD cur = first;
        while (true) {
            n++;
            if (dw(d, 0x20) < n)
                fn<void(THISCALL *)(void *, DWORD)>(0x56ec50)(d, n);
            fn<void(THISCALL *)(void *, DWORD)>(0x581880)(bp(dw(bp(dw(d, 0x1c)), (n - 1) * 4)), float_bits(fstp(fld(fl<float>(item, 0x18)))));
            BYTE *e = bp(dw(bp(dw(d, 0x1c)), (n - 1) * 4));
            fn<void(THISCALL *)(void *, DWORD)>(0x581860)(e, float_bits(fstp(fn<long double(THISCALL *)(void *)>(0x5afe50)(item))));
            e = bp(dw(bp(dw(d, 0x1c)), (n - 1) * 4));
            data_set_count(e, properties_count(item));
            BYTE *idef = item_definition(item);
            if (!idef || !is_kind_of(idef, energy_def_class()))
                break;
            cur = inventory_next_of(inv, cur, category_index(glob_addr(TEXT_SHIELD)));
            if (cur == first)
                break;
            item = inventory_item(inv, cur);
            if (!item || !is_kind_of(item, class_at(fn_addr(0x5afbd0))))
                break;
        }
    }
    if (dw(d, 0x20) > n)
        fn<void(THISCALL *)(void *, DWORD)>(0x56ec50)(d, n);
    return m.made;
}
// 0x56f160 (slot 26)
static DWORD THISCALL Shield_Read(BYTE *, void *packet, DWORD *slot)
{
    const Made m = ensure_data(slot, kind(*slot, fn_addr(0x56e640)), 0x28, [](BYTE *o) {
        obj_ctor(o);
        vtable_and_defaults(o, vt(0x5ede94));
        dw(o, 0x18) = vt(0x5ffd24);
        dw(o, 0x1c) = 0;
        dw(o, 0x20) = 0;
        dw(o, 0x24) = 0;
        vtable_and_defaults(o, vt(0x5ffcb8));
    });
    vcall<DWORD>(m.data, 25, packet);
    return m.made;
}

// ---- CShieldInventoryHUDRenderer: drawing ----------------------------------------------------------------

// 0x56f280 (slot 28): per shield its definition's meter (+0x16c/+0x170 its size, centred on the placement,
// which is moved up by 4) and "%d/%d %s" (charge, capacity, the units of 0x713518) in "Small Gold" above it
static void THISCALL Shield_Draw(BYTE *, BYTE *data, void *frame, int dx)
{
    BYTE *d = data && is_kind_of(data, class_at(fn_addr(0x56e640))) ? data : nullptr;
    for (DWORD i = 0; i < dw(d, 0x20); i++) {
        BYTE *e = bp(dw(bp(dw(d, 0x1c)), i * 4));
        BYTE *def = of_class(item_definition_at(dw(e, 0xc)), fn_addr(0x486220));
        int x = 0, y = 0;
        hud_place(def, frame, &x, &y);
        x += dx;
        y -= 4;
        BYTE *fonts = glob<BYTE *>(FONT_LIST);
        BYTE *font = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(fonts, vcall<DWORD>(fonts, 3, glob<const char *>(0x612264) /* "Small Gold" */, -1));
        e = bp(dw(bp(dw(d, 0x1c)), i * 4));
        const DWORD value = dw(e, 0x18), max = dw(e, 0x14);
        const int h = (int)dw(def, 0x170), hw = (int)dw(def, 0x16c) / 2, hh = h / 2;
        const int rect[4] = {x - hw, y - hh, hw + x, hh + y};
        meter_draw(def + 0x100, frame, const_cast<int *>(rect), value, max, 0);
        const char *units = zs_c_str(glob<void *>(0x713518));
        const int a = x87_ftol(fld(raw_float(max)));
        const int b = x87_ftol(fld(raw_float(value)));
        char text[0x54];
        sprintf_exe(text, glob<const char *>(0x6121b8) /* "%d/%d %s" */, b, a, units);
        font_text(font, frame, x, y - (int)dw(font, 0x80) - h, text, 1, 0);
        y += (int)dw(def, 0x168);   // the next row; the placement overwrites it (as in the original)
    }
}

// ---- CEnergyInventoryHUDRenderer -------------------------------------------------------------------------

// 0x581a90 (slot 25: part, item, opacity, render data slot): an energy item's charge (+0x18) and capacity
// (MaxCharge); for anything else the catalogue index of "!None"
static DWORD THISCALL Energy_Update(BYTE *, void *, BYTE *item, DWORD opacity, DWORD *slot)
{
    const Made m = ensure_data(slot, kind(*slot, fn_addr(0x581380)), 0x20, [](BYTE *o) { construct_data(o, vt(0x5ffd30)); });
    BYTE *d = m.data;
    BYTE *energy = of_class(item, fn_addr(0x5afbd0));
    BYTE *def = of_class(item_definition(item), fn_addr(0x4d55c0));
    if (energy && def) {
        data_set_value(d, opacity);
        data_set_count(d, properties_count(item));
        if (!c3(fld(fl<float>(energy, 0x18)), fld(fl<float>(d, 0x18)))) {
            fl<float>(d, 0x18) = fstp(fld(fl<float>(energy, 0x18)));
            dw(d, 0x1c) |= 2;
        }
        const long double cap = fn<long double(THISCALL *)(void *)>(0x5afe50)(energy);
        if (!c3(cap, fld(fl<float>(d, 0x14)))) {
            fl<float>(d, 0x14) = fstp(cap);
            dw(d, 0x1c) |= 1;
        }
        return m.made;
    }
    DWORD i = vcall<DWORD>(glob<void *>(ITEM_LIST), 2, glob<const char *>(0x60b0a0) /* "!None" */, -1);
    if (i == 0xffffffff)
        i = 0;
    data_set_count(d, i);
    return m.made;
}
// 0x581c40 (slot 26)
static DWORD THISCALL Energy_Read(BYTE *, void *packet, DWORD *slot)
{
    const Made m = ensure_data(slot, kind(*slot, fn_addr(0x581380)), 0x20, [](BYTE *o) { construct_data(o, vt(0x5ffd30)); });
    vcall<DWORD>(m.data, 25, packet);
    return m.made;
}
// 0x581d20 (slot 28): the definition's energy meter (+0x16c/+0x170 its size, centred on the placement)
static void THISCALL Energy_Draw(BYTE *, BYTE *data, void *frame, int dx)
{
    BYTE *d = data && is_kind_of(data, class_at(fn_addr(0x581380))) ? data : nullptr;
    BYTE *def = of_class(item_definition_at(dw(d, 0xc)), fn_addr(0x486220));
    int x = 0, y = 0;
    hud_place(def, frame, &x, &y);
    x += dx;
    if (!def)
        return;
    const int hw = (int)dw(def, 0x16c) / 2, hh = (int)dw(def, 0x170) / 2;
    int rect[4] = {x - hw, y - hh, hw + x, hh + y};
    meter_draw(def + 0x100, frame, rect, float_bits(fstp(fld(fl<float>(d, 0x18)))), float_bits(fstp(fld(fl<float>(d, 0x14)))), 0);
}

// 0x582270 (CCharacterInventoryHUDRenderer slot 28): the energy meter, then the special ability's name
// ("Special/Special Ability" of the CCharacterInventoryItemDefinition, enum list 0x713790) in the font of +8,
// vertically centred on the placement
static void THISCALL Character_Draw(BYTE *self, BYTE *data, void *frame, int dx)
{
    BYTE *d = data && is_kind_of(data, class_at(fn_addr(0x581380))) ? data : nullptr;
    fn<void(THISCALL *)(void *, void *, void *, int)>(0x581d20)(self, data, frame, dx);
    BYTE *def = of_class(item_definition_at(dw(d, 0xc)), fn_addr(0x5807f0));
    int x = 0, y = 0;
    hud_place(def, frame, &x, &y);
    x += dx;
    if (!def)
        return;
    const char *name = fn<const char *(THISCALL *)(void *, DWORD)>(0x591e10)(glob<void *>(0x713790), dw(def, 0x174));
    BYTE *font = record_object_loading(glob_addr(FONT_LIST), glob_addr(FONT_RECORDS), glob_addr(FONT_COUNT), glob_addr(FONT_FOLDER), dw(self, 8));
    font_text(font, frame, x, y - (int)(dw(font, 0x80) >> 1), name, 1, 0);
}

// 0x583860 (CSphereOfZinEnergyInventoryHUDRenderer slot 28): the "HUD Sequence" at a frame that swings with a
// sine: the phase (static 0x713810, kept below 0x5eeeac by whole turns) advances by the time since the last
// call (0x4d2460, static 0x7137f8 with guard 0x7137d9 bit 1) times a rate between "Min/Max Energy-Pulse Rate"
// by the charge ratio; the amplitude between "Min/Max Energy-Pulse Percentage"; "Flip HUD Sequence Order"
// counts frames backwards. Then the "Overlay HUD Sequence" at the clock's frame.
static void THISCALL SphereOfZin_Draw(BYTE *self, BYTE *data, void *frame, int dx)
{
    BYTE *d = data && is_kind_of(data, class_at(fn_addr(0x581380))) ? data : nullptr;
    BYTE *def = of_class(item_definition_at(dw(d, 0xc)), fn_addr(0x5b5f10));
    int x = 0, y = 0;
    hud_place(def, frame, &x, &y);
    x += dx;
    BYTE *seq = record_object(glob_addr(HUD_LIST), glob_addr(HUD_RECORDS), glob_addr(HUD_COUNT), dw(self, 8));
    if (!seq)
        return;
    const DWORD n = sequence_frames(seq);
    const long double c = cst(glob_addr(0x5eee2c));
    const float scale = fstp((fld(fl<float>(self, 0xc)) + fld(fl<float>(self, 0x10))) * c + cst(glob_addr(0x5ec3f4)));
    const long double ratio = fld(fl<float>(d, 0x18)) / fld(fl<float>(d, 0x14));
    const float amp = fstp(((fld(fl<float>(self, 0xc)) - fld(fl<float>(self, 0x10))) * ratio + fld(fl<float>(self, 0x10))) * c);
    const float base = fstp(fld(fl<float>(self, 0x10)) * c + ratio);
    const float rate = fstp((fld(fl<float>(self, 0x14)) - fld(fl<float>(self, 0x18))) * ratio + fld(fl<float>(self, 0x18)));
    float &phase = *glob<float *>(0x713810);
    if (!c03(fld(phase), cst(glob_addr(0x5eeeac)))) {
        const DWORD k = (DWORD)x87_ftol(fld(phase));
        phase = fstp(fld(phase) - fild64((k / 7) * 2) * cst(glob_addr(0x5eecc8)));
    }
    BYTE &guard = *glob<BYTE *>(0x7137d9);
    DWORD &last = *glob<DWORD *>(0x7137f8);
    if (!(guard & 1)) {
        guard |= 1;
        last = fn<DWORD(__stdcall *)()>(0x4d2460)();
    }
    const DWORD delta = fn<DWORD(__stdcall *)()>(0x4d2460)() - last;
    last += delta;
    phase = fstp(fild64(delta) * fld(rate) * cst(glob_addr(0x60123c)) + fld(phase));
    const long double s = fn<long double(__stdcall *)(DWORD)>(0x508ec0)(float_bits(phase));
    int f = x87_ftol(fild64(n) * ((s * fld(amp) + fld(base)) * (long double)*glob<double *>(0x5ee620)) / fld(scale));
    if (self[0x1c])
        f = (int)n - f - 1;
    if (void *img = sequence_frame(seq, (DWORD)f))
        blit_at(frame, img, x, y, sequence_blend(seq));
    const DWORD over = dw(self, 0x20);
    if (!over || !*glob<BYTE **>(HUD_RECORDS))
        return;
    BYTE *seq2 = list_object(glob_addr(HUD_LIST), glob_addr(HUD_RECORDS), glob_addr(HUD_COUNT), glob_addr(HUD_FOLDER), over);
    if (!seq2)
        return;
    const DWORD n2 = sequence_frames(seq2);
    const DWORD fps = float_bits(fstp(vcall<long double>(seq2, 26)));
    if (void *img = sequence_frame(seq2, sync_frame(n2, 0, fps)))
        blit_at(frame, img, x, y, sequence_blend(seq2));
}

// ---- The auto-map ------------------------------------------------------------------------------------------

namespace {
constexpr DWORD MAP_FRAME = 0x6c2d38, COPY_BLEND = 0x6bb4e0;
BYTE *hud_bar_options()
{
    BYTE *holder = bp(dw(*glob<BYTE **>(0x713f14), 0x2e0));
    BYTE *rules = bp(dw(holder, 0x38));
    if (rules && is_kind_of(rules, class_at(fn_addr(0x5a1350))))
        return fn<BYTE *(THISCALL *)(void *)>(0x5a1fa0)(bp(dw(holder, 0x38)));
    return fn<BYTE *(THISCALL *)(void *)>(0x5a1fa0)(nullptr);
}
BYTE *current_layer() { return fn<BYTE *(THISCALL *)(void *)>(0x4f1050)(world()); }
void to_minimap(BYTE *layer, int *out, const int *in) { fn<void(THISCALL *)(void *, int *, const int *)>(0x4ec3c0)(layer, out, in); }
void fill_rect(void *frame, const int *r, void *color) { fn<DWORD(THISCALL *)(void *, const int *, void *)>(0x4abe90)(frame, r, color); }
// A part's position as the mini map's rectangle, one pixel more on every side
void minimap_dot(BYTE *layer, BYTE *part, void *color)
{
    float pos[2];
    fn<float *(THISCALL *)(void *, float *)>(0x4fb0a0)(part, pos);
    const int px = x87_ftol(fld(pos[0])), py = x87_ftol(fld(pos[1]));
    const int in[4] = {px, py, px, py};
    int r[4];
    to_minimap(layer, r, in);
    r[0]--;
    r[1]--;
    r[2]++;
    r[3]++;
    fill_rect(glob<void *>(MAP_FRAME), r, color);
}
}  // namespace

// 0x508000 (the auto-map's state: seconds): +0 the mode (1 opening, 3 closing: +4 counts down, then 2 open or
// 0 closed); then, unless the reporter's slot 30 or the application's slot 84 says so, the fog of war of the
// current layer is cleared (palette colour 0) over the view's mini map rectangle when the view target moved
// more than "Fog Of War Update Distance" (its square; 0x5f9e38 "Always Update") or the layer changed
static void THISCALL HudMiniMap_UpdateFog(BYTE *self, DWORD seconds)
{
    const DWORD mode = dw(self, 0);
    if (mode == 1 || mode == 3) {
        const long double t = fld(fl<float>(self, 4)) - fld(raw_float(seconds));
        fl<float>(self, 4) = fstp(t);
        if (c0(t, cst(glob_addr(C_ZERO)))) {
            dw(self, 4) = 0;
            dw(self, 0) = mode == 1 ? 2 : 0;
        }
    }
    if ((BYTE)vcall<DWORD>(reporter(), 30))
        return;
    BYTE *opts = hud_bar_options();
    float pos[2];
    fn<void(THISCALL *)(void *, float *, DWORD)>(0x4f5da0)(world(), pos, 0);
    if ((BYTE)vcall<DWORD>(*glob<void **>(0x713f14), 84))
        return;
    if (bp(dw(self, 0x10)) == current_layer() && !c3(fld(fl<float>(opts, 0x58)), cst(glob_addr(0x5f9e38)))) {
        const long double dy = fld(pos[1]) - fld(fl<float>(self, 0xc));
        const long double dx = fld(pos[0]) - fld(fl<float>(self, 8));
        if (c03(dy * dy + dx * dx, fld(fl<float>(opts, 0x58))))
            return;
    }
    dw(self, 0x10) = ptr_value(current_layer());
    fl<float>(self, 8) = pos[0];
    fl<float>(self, 0xc) = pos[1];
    int view[4], r[4];
    vcall<DWORD>(world(), 57, 0, view);
    to_minimap(bp(dw(self, 0x10)), r, view);
    BYTE color[0x10];
    field_color_rgba(ptr_value(color), 0, 0, 0, 0);
    fn<void(THISCALL *)(void *, DWORD)>(0x53f100)(color, 0);
    BYTE *fog = fn<BYTE *(THISCALL *)(void *)>(0x4e7170)(bp(dw(self, 0x10)) + 0x3c);
    vcall<DWORD>(fog, 28, r, color);
}

// 0x508180 (the auto-map's state: frame, x, y): when open and with a window and a game type, the static frame
// 0x6c2d38 (guard 0x6c2db8 bit 1; sized to "MiniMap Size" at the colour depth) gets the game type's map
// (its slot 43) or the layer's mini map image, the fog of war (Transparent Mask), the first character's dot
// ("MiniMap Player Color") and the monitored parts' dots ("MiniMap Intrest Color"); then it is blitted at
// (x, y) with the Copy mode at "Mini-Map Opacity" times its opacity
static void THISCALL HudMiniMap_Draw(BYTE *self, void *frame, int x, int y)
{
    if (!dw(self, 0))
        return;
    BYTE *window = fn<BYTE *(THISCALL *)(void *)>(0x4cdf30)(reporter());
    if (!vcall<DWORD>(window, 27))
        return;
    if (!fn<BYTE *(THISCALL *)(void *)>(0x59ff80)(*glob<void **>(0x713f14)))
        return;
    BYTE *opts = hud_bar_options();
    BYTE *layer = current_layer();
    BYTE &guard = *glob<BYTE *>(0x6c2db8);
    if (!(guard & 1)) {
        guard |= 1;
        fn<void *(THISCALL *)(void *)>(0x4a8a90)(glob<void *>(MAP_FRAME));
        crt_atexit(code<void(__cdecl *)()>(0x508490));
    }
    if (*at<WORD *>(glob_addr(MAP_FRAME) + 0x12) != dw(opts, 0x10) || (*at<DWORD *>(glob_addr(MAP_FRAME) + 0x14) & 0xffff) != dw(opts, 0x14))
        fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(glob<void *>(MAP_FRAME), dw(opts, 0x10), dw(opts, 0x14),
                                                                     fn<DWORD(__cdecl *)()>(0x55d030)());
    BYTE *gt = fn<BYTE *(THISCALL *)(void *)>(0x59ff80)(*glob<void **>(0x713f14));
    if (gt && !(BYTE)vcall<DWORD>(fn<BYTE *(THISCALL *)(void *)>(0x59ff80)(*glob<void **>(0x713f14)), 30)) {
        vcall<DWORD>(fn<BYTE *(THISCALL *)(void *)>(0x59ff80)(*glob<void **>(0x713f14)), 43, glob<void *>(MAP_FRAME));
    } else {
        BYTE *save = layer + 0x3c;
        blit_at(glob<void *>(MAP_FRAME), fn<void *(THISCALL *)(void *)>(0x4e7100)(save), 0, 0, glob<void *>(COPY_BLEND));
        blit_at(glob<void *>(MAP_FRAME), fn<void *(THISCALL *)(void *)>(0x4e7170)(save), 0, 0, glob<void *>(MASK_BLEND));
        minimap_dot(layer, fn<BYTE *(THISCALL *)(void *)>(0x4f5d80)(world()), opts + 0x38);
        void *desc = *glob<void **>(0x6c2d78);
        if ((BYTE)fn<DWORD(THISCALL *)(void *, void *)>(0x418fc0)(desc, save)) {
            BYTE *list = fn<BYTE *(THISCALL *)(void *, void *)>(0x418790)(*glob<void **>(0x6c2d78), save);
            for (DWORD i = 0; i < dw(list, 0x10); i++)
                minimap_dot(layer, bp(dw(bp(dw(bp(dw(list, 0xc)), i * 4)), 8)), opts + 0x48);
        }
    }
    const float saved = fstp(get_opacity(glob_addr(COPY_BLEND)));
    set_opacity(glob_addr(COPY_BLEND), float_bits(fstp(fld(fl<float>(opts, 0x5c)) * fld(saved))));
    blit_at(frame, glob<void *>(MAP_FRAME), x, y, glob<void *>(COPY_BLEND));
    set_opacity(glob_addr(COPY_BLEND), float_bits(saved));
}

ZAX_HOOKS(hud_render)
{
    const Hook list[] = {
        {0x429de0, (void *)&InventoryDisplayOptions, "InventoryDisplayOptions", 5, {0xa1, 0x14, 0x3f, 0x71, 0x00, 0x56, 0x8b, 0xb0}},
        {0x4298d0, ptr(&Hud_Place), "InventoryHUD_Place", 8, {0x51, 0x53, 0x55, 0x56, 0x8b, 0x5c, 0x24, 0x18}},
        {0x5acab0, (void *)&Hud_DrawItemIcon, "Hud_DrawItemIcon", 10, {0x8b, 0x4c, 0x24, 0x04, 0x81, 0xec, 0x00, 0x01}},
        {0x486d10, ptr(&EnergyMeter_Draw), "EnergyMeter_Draw", 5, {0x83, 0xec, 0x58, 0x53, 0x55, 0x8b, 0x6c, 0x24}},
        {0x423e90, ptr(&Compass_Begin), "CCompassRenderer::Begin", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x14, 0x8b, 0xb7}},
        {0x423990, ptr(&Compass_Update), "CCompassRenderer::Update", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x14, 0x53, 0x56}},
        {0x423b70, ptr(&Compass_Read), "CCompassRenderer::Read", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x10, 0x56, 0x32}},
        {0x423c50, ptr(&Compass_Draw), "CCompassRenderer::Draw", 5, {0x83, 0xec, 0x10, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x469e90, ptr(&Flag_Update), "CFlagInventoryHUDRenderer::Update", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x1c, 0x32}},
        {0x469f20, ptr(&Flag_Read), "CFlagInventoryHUDRenderer::Read", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x14, 0x32}},
        {0x469fa0, ptr(&Flag_Draw), "CFlagInventoryHUDRenderer::Draw", 6, {0x81, 0xec, 0x0c, 0x01, 0x00, 0x00, 0x53, 0x56}},
        {0x4859b0, ptr(&Weapon_Update), "CWeaponInventoryHUDRenderer::Update", 7, {0x51, 0x53, 0x55, 0x8b, 0x6c, 0x24, 0x1c, 0x56}},
        {0x485be0, ptr(&Weapon_Read), "CWeaponInventoryHUDRenderer::Read", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x10, 0x56, 0x32}},
        {0x485cc0, ptr(&Weapon_Draw), "CWeaponInventoryHUDRenderer::Draw", 6, {0x81, 0xec, 0x98, 0x00, 0x00, 0x00, 0x53, 0x55}},
        {0x56ef20, ptr(&Shield_Update), "CShieldInventoryHUDRenderer::Update", 5, {0x83, 0xec, 0x08, 0x55, 0x56, 0x57, 0x8b, 0x7c}},
        {0x56f160, ptr(&Shield_Read), "CShieldInventoryHUDRenderer::Read", 7, {0x51, 0x53, 0x55, 0x8b, 0x6c, 0x24, 0x14, 0x56}},
        {0x56f280, ptr(&Shield_Draw), "CShieldInventoryHUDRenderer::Draw", 6, {0x81, 0xec, 0x88, 0x00, 0x00, 0x00, 0x53, 0x55}},
        {0x581a90, ptr(&Energy_Update), "CEnergyInventoryHUDRenderer::Update", 6, {0x51, 0x53, 0x8b, 0x5c, 0x24, 0x18, 0x55, 0x56}},
        {0x581c40, ptr(&Energy_Read), "CEnergyInventoryHUDRenderer::Read", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x10, 0x56, 0x32}},
        {0x581d20, ptr(&Energy_Draw), "CEnergyInventoryHUDRenderer::Draw", 5, {0x83, 0xec, 0x18, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x582270, ptr(&Character_Draw), "CCharacterInventoryHUDRenderer::Draw", 6, {0x81, 0xec, 0x0c, 0x01, 0x00, 0x00, 0x53, 0x55}},
        {0x583860, ptr(&SphereOfZin_Draw), "CSphereOfZinEnergyInventoryHUDRenderer::Draw", 6, {0x81, 0xec, 0x20, 0x01, 0x00, 0x00, 0x53, 0x55}},
        {0x508000, ptr(&HudMiniMap_UpdateFog), "HudMiniMap_UpdateFog", 6, {0x83, 0xec, 0x38, 0x56, 0x8b, 0xf1, 0x57, 0x8b}},
        {0x508180, ptr(&HudMiniMap_Draw), "HudMiniMap_Draw", 5, {0x8b, 0x01, 0x83, 0xec, 0x54, 0x85, 0xc0, 0x0f}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
