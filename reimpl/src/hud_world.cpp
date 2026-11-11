// The world's HUD (phase 4, rendering): CZaxWorld::DrawHud 0x59e730 (slot 96 of the application), the
// status text 0x5a0a30/0x5a0c00 and the three game types' HUD slot 42 (Capture the Flag 0x4698f0, death
// match 0x4796a0, Salvage King 0x561760). The item renderers, the auto-map, fonts, frames and blitters
// stay the HUD batch's, the sprite batch's and R1-R7's (through their exe addresses).
//
// CZaxWorld::DrawHud (frame) [confirmed: code; VM] draws, while a game runs and its view is drawn (+0x310,
// +0x311):
//  1. the direction pointer (CPlayerWalkingControlAI::LoadDirectionPointer 0x5432a0, frame 0 of it) when the
//     direct drawing flag 0x6c1d08 is set and the local character 0x6bde9c exists and the application's
//     slot 84 is 0, or, without the flag, when the local character's inventory has a current "Primary"
//     item: at the character's position moved by the custom rules' pointer distance (+0x18 object: +0x2c
//     times the direction +0x5c/+0x60 of the world's slot 88 object, minus +0x3c in y), relative to the
//     current layer's view (+0x80/+0x84), divided by the zoom (0x4f6960), rounded, and pushed back inside the
//     frame's rectangle by the pointer's margins (+0xc..+0x18 of the pointer object; 0x38 more at the
//     bottom), with the Transparent Mask;
//  2. the HUD frames of "HUD Main", "HUD Spacer", "HUD Under MiniMap" and "NEWMINIMAP" (the HUD image list
//     0x6c0540): the auto-map's open fraction (0x507f40) pushes the mini map down from the HUD bar top
//     (0x429880): a black bar (0x6be368, Frame_FillRectXY) under the map while it is open, the map frame,
//     the auto-map itself (HudMiniMap_Draw 0x508180 at the HUD options' +8/+0xc), the frame under it, "HUD
//     Main" at the right edge and, on a frame 800 pixels wide, "HUD Spacer" after "HUD Under MiniMap" (the
//     texts then move 0x50 pixels right);
//  3. with a quest or Salvage King game type (+0x58c in the game type list 0x6c02e8): the ore and crystal
//     counts ("%s = %d", "/%d" for a maximum) with their small icons and the names of "Ore Deposits" and
//     "Crystals" (function-local statics, guard bits 2-16 of 0x713e88), at the inventory display options'
//     +0x214..+0x234;
//  4. the inventory window's frame (+0x110 of the HUD object +0x340, created on first use 0x4293c0) and the
//     player's +0xec as a number (+0x200..+0x210);
//  5. with +0x594 above 0 one of the help texts (+0x5a8: the binding of control 10 in two formats,
//     +0x5ac: the same with +0x290, +0x5b0: one format, centred; DrawLines at +0x274/+0x278) or the item
//     message (+0x5a4: an icon +0x598 and its text: +0x5a0 0 and 1 a Local.txt entry, 2 the item's name);
//     otherwise the game type's time (its slot 31) as h:m:s (+0x1f4..+0x1fc);
//  6. every HUD record of the player (+0x100, +0x104 records {catalogue index, render data}): the item
//     definition's "Display/HUD Renderer" (slot 32, else the type definition's +0x24) draws it (slot 28)
//     with the text offset, -0x50 for a shield renderer when the offset is not 0.
// The function-local statics: the text 0x713e4c (guard bit 1 of 0x713e88, atexit 0x59fc90), the class
// CZaxCustomRules 0x713f24 registered on first use (0x415780), the net layer 0x713f18 (0x4f7df0, vtable
// 0x602fa8), the custom rules' +0x18 (0x40 bytes, vtable 0x5fcebc) and +0x1c (0x60 bytes, three colours,
// vtable 0x5f9e98) objects made on first use.
//
// The status text 0x5a0a30 [confirmed: code; VM]: +0x364 in "Loading Font" centred on the frame, on a black
// bar (alpha (+0x368 / 1, or / 0.5 in 8 bits, at most 1) * 0x5eee14 * 0x5eb728) unless +0x36c or 8 bits,
// with the Transparent Mask's opacity at that fraction (put back after). 0x5a0c00 (text, fraction, no bar)
// sets them, draws and puts the frame to the display +0x44 + 1 times (Frame_BlitAt, then 0x5693a0: the
// screen frame's flip, phase 5).
//
// The game types' HUD (frame, x, y) [confirmed: code; VM]: Capture the Flag: the player's team flag
// (+0xe8 or +0xec by the player's +0x14, the image list 0x6c0540) at +0xe0/+0xe4, then per team "%s = "
// (right justified) and the score (+4 of the team) in the font +0xd4 at +0xd8/+0xdc, the team names from
// the two registered strings 0x6bd124, 0x6bd114 (a third team reads on past them, as in the original: below),
// then the player's own score (+0xf6) after 0x6bd11c; death match: the player's score centred in the font
// +0xc8 at +0xcc/+0xd0, then the first +0xe0 rows of the scoreboard (rank, name, frags; columns +0xe8..
// +0xfc with their justifications) in the font +0xd4 at +0xd8/+0xdc, rows the font's line height plus
// +0xe4 apart; Salvage King: the player's score centred in the font +0xd0 (loaded inline when missing) at
// +0xd4/+0xd8. The player is ZaxNet_PlayerAt 0x5ba810 of 0x5fe8f8.
//
// Return values: the drawing functions are void for their callers (eax not reproduced).
#include "ai_helpers.h"
#include "x87.h"

namespace zax {

using namespace ai;

namespace {

using LD = long double;
constexpr DWORD MASK = 0x6bcc68, COPY = 0x6bb4e0;
constexpr DWORD FONTS = 0x6be1e0, FONT_RECORDS = 0x6be210, FONT_COUNT = 0x6be214, FONT_FOLDER = 0x6be218;
constexpr DWORD IMAGES = 0x6c0540, IMAGE_RECORDS = 0x6c0570, IMAGE_COUNT = 0x6c0574;
constexpr DWORD ICONS = 0x6c0aa8, ICON_RECORDS = 0x6c0ad8, ICON_COUNT = 0x6c0adc;
constexpr DWORD GAME_TYPES = 0x6c02e8, GT_RECORDS = 0x6c0318, GT_COUNT = 0x6c031c;
constexpr DWORD ITEMS = 0x6c0c08, ITEM_RECORDS = 0x6c0c38, ITEM_COUNT = 0x6c0c3c, ITEM_FOLDER = 0x6c0c40;
constexpr DWORD TEXT = 0x713e4c, GUARD = 0x713e88, APP = 0x713f14;

// A definition list's record object as the inline lookups with DefinitionList_LoadRecord 0x55e090 do it
BYTE *record(DWORD list, DWORD records_at, DWORD count_at, DWORD i)
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
        zax::fn<void(THISCALL *)(void *, DWORD)>(0x55e090)(at<void *>(list), i);
        records = *at<BYTE **>(records_at);
    }
    return bp(dw(records + off, 0x78));
}
// The same with the list's own loading (Path_Join, slot 37) inline
BYTE *record_loading(DWORD list, DWORD records_at, DWORD count_at, DWORD folder, DWORD i)
{
    if (!*at<BYTE **>(records_at))
        return nullptr;
    return list_object(list, records_at, count_at, folder, i);
}
BYTE *font(DWORD i) { return record(glob_addr(FONTS), glob_addr(FONT_RECORDS), glob_addr(FONT_COUNT), i); }
BYTE *font_loading(DWORD i) { return record_loading(glob_addr(FONTS), glob_addr(FONT_RECORDS), glob_addr(FONT_COUNT), glob_addr(FONT_FOLDER), i); }
DWORD list_find(DWORD list, DWORD name) { return vcall<DWORD>(at<void *>(list), 3, at<const char *>(name), (DWORD)-1); }
BYTE *list_object_at(DWORD list, DWORD i) { return zax::fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(at<void *>(list), i); }
BYTE *image(DWORD name) { return list_object_at(glob_addr(IMAGES), list_find(glob_addr(IMAGES), name)); }
// The game type of the world (+0x58c in the game type list; -1: none)
BYTE *game_type(BYTE *w)
{
    const DWORD i = dw(w, 0x58c);
    if (i == 0xffffffffu)
        return nullptr;
    return record(glob_addr(GAME_TYPES), glob_addr(GT_RECORDS), glob_addr(GT_COUNT), i);
}

DWORD blit_at(void *frame, void *src, int x, int y, DWORD mode)
{
    return zax::fn<DWORD(THISCALL *)(void *, void *, int, int, void *)>(0x4b3280)(frame, src, x, y, at<void *>(mode));
}
DWORD draw_text(void *f, void *frame, int x, int y, const char *s, DWORD just, DWORD a6)
{
    return zax::fn<DWORD(THISCALL *)(void *, void *, int, int, const char *, DWORD, DWORD)>(0x4a6470)(f, frame, x, y, s, just, a6);
}
DWORD draw_lines(void *f, void *frame, int x, int y, const char *s, DWORD a5, DWORD a6)
{
    return zax::fn<DWORD(THISCALL *)(void *, void *, int, int, const char *, DWORD, DWORD)>(0x4a66c0)(f, frame, x, y, s, a5, a6);
}
DWORD seq_height(void *f) { return zax::fn<DWORD(__fastcall *)(void *)>(0x4a6be0)(f); }
DWORD fw(void *f) { return vcall<DWORD>(f, 45); }
DWORD fh(void *f) { return vcall<DWORD>(f, 39); }
template <typename... A> void zs_format(void *z, const char *fmt, A... a) { zax::fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, fmt, a...); }
template <typename... A> void zs_append(void *z, const char *fmt, A... a) { zax::fn<void(__cdecl *)(void *, const char *, ...)>(0x4e0a30)(z, fmt, a...); }
// ZString = ZString (0x4e18f0; the Local.txt entries' second member is the text's ZString)
void zs_assign(void *z, const void *s) { zax::fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(z, s); }
const char *field_name(void *f) { return zax::fn<const char *(THISCALL *)(void *)>(0x5007a0)(f); }
const void *second_member(void *o) { return zax::fn<const void *(THISCALL *)(void *)>(0x5007b0)(o); }
bool first_use(BYTE bit)
{
    BYTE &g = *glob<BYTE *>(GUARD);
    if (g & bit)
        return false;
    g |= bit;
    return true;
}
void atexit_exe(DWORD fn) { zax::fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(fn)); }

// The application's custom rules (+0x2e0 of the application, its +0x38 when a CZaxCustomRules; the class
// registered on first use as DrawHud does it)
BYTE *custom_rules()
{
    BYTE *holder = bp(dw(*glob<BYTE **>(APP), 0x2e0));
    if (!dw(holder, 0x38))
        return nullptr;
    if (!*glob<DWORD *>(0x713f24)) {
        if (void *c = zax::fn<void *(__fastcall *)(DWORD)>(0x417710)(0x48)) {
            void *parent = zax::fn<void *(__cdecl *)()>(0x560ac0)();
            zax::fn<void *(THISCALL *)(void *, DWORD, const char *, float, void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x415780)(
                c, glob_addr(0x713f24), glob<const char *>(0x6249b0), 1.0f, parent, 0x30, 0, fn_addr(0x5a13d0), fn_addr(0x5a1410), fn_addr(0x5a1580));
        }
    }
    if (!is_kind_of(bp(dw(holder, 0x38)), *glob<CClassInfo **>(0x713f24)))
        return nullptr;
    return bp(dw(holder, 0x38));
}
// A custom rules object made on first use (pool block, CObject levels, the vtable, slot 19)
template <typename C> BYTE *rules_object(BYTE *rules, DWORD at_off, DWORD size, C construct)
{
    if (!dw(rules, at_off)) {
        BYTE *o = static_cast<BYTE *>(zax::fn<void *(__fastcall *)(DWORD)>(0x417710)(size));
        if (o)
            construct(o);
        dw(rules, at_off) = ptr_value(o);
        vcall<DWORD>(o, 19);
    }
    return bp(dw(rules, at_off));
}

// The direction pointer (part 1 of DrawHud)
void draw_pointer(BYTE *w, void *frame)
{
    BYTE *rules = custom_rules();
    BYTE *opt = rules_object(rules, 0x18, 0x40, [](BYTE *o) {
        obj_ctor(o);
        dw(o, 0) = vt(0x5fcebc);
        obj_init_fields(o);
    });
    void *img = vcall<void *>(zax::fn<void *(__cdecl *)()>(0x5432a0)(), 23, (DWORD)0);
    float pos[2];
    part_position(*glob<void **>(0x6bde9c), pos);
    BYTE *dir = vcall<BYTE *>(w, 88);
    pos[0] = fstp(fild64(dw(opt, 0x2c)) * fl<float>(dir, 0x5c) + pos[0]);
    pos[1] = fstp(fild64(dw(opt, 0x2c)) * fl<float>(dir, 0x60) + pos[1]);
    pos[1] = fstp((LD)pos[1] - fild((int)dw(opt, 0x3c)));
    BYTE *layer = zax::fn<BYTE *(THISCALL *)(void *)>(0x4f1050)(w);
    float view[2] = {fl<float>(layer, 0x80), fl<float>(layer, 0x84)};
    int rect[4];
    zax::fn<int(THISCALL *)(void *, const float *, int *)>(0x4ea660)(zax::fn<BYTE *(THISCALL *)(void *)>(0x4f1050)(w), view, rect);
    fh(bp(dw(w, 4)));
    fw(bp(dw(w, 4)));
    pos[0] = fstp((LD)pos[0] - view[0]);
    pos[1] = fstp((LD)pos[1] - view[1]);
    pos[0] = fstp((LD)pos[0] / zax::fn<LD(THISCALL *)(void *, DWORD)>(0x4f6960)(w, 0));
    pos[1] = fstp((LD)pos[1] / zax::fn<LD(THISCALL *)(void *, DWORD)>(0x4f6960)(w, 0));
    const int x = x87_fistp(pos[0]), y = x87_fistp(pos[1]);
    const int *m = reinterpret_cast<const int *>(zax::fn<BYTE *(__cdecl *)()>(0x5432a0)() + 0xc);
    const int ml = m[0], mt = m[1], mr = m[2], mb = m[3];
    int c[4];
    vcall<DWORD>(frame, 36, c);
    int bx;
    if (x + ml < c[0])
        bx = c[0] - ml;
    else if (x + mr > c[2])
        bx = c[2] - mr;
    else
        bx = x;
    int by;
    if (y + mt < c[1])
        by = c[1] - mt;
    else if (y + mb > c[3] - 0x38)
        by = c[3] - mb - 0x38;
    else
        by = y;
    blit_at(frame, img, bx, by, glob_addr(MASK));
}

}  // namespace

// 0x59e730 (frame)
void THISCALL ZaxWorld_DrawHud(BYTE *w, void *frame)
{
    BYTE *opts = zax::fn<BYTE *(__cdecl *)()>(0x429de0)();
    if (!*glob<DWORD *>(0x713f18)) {
        BYTE *o = static_cast<BYTE *>(zax::fn<void *(__cdecl *)(DWORD)>(0x5d033a)(8));
        if (o) {
            zax::fn<void *(THISCALL *)(void *)>(0x4f7df0)(o);
            dw(o, 0) = vt(0x602fa8);
            *glob<BYTE **>(0x713f18) = o;
        } else {
            *glob<DWORD *>(0x713f18) = 0;
        }
    }
    BYTE *player = zax::fn<BYTE *(__fastcall *)(DWORD)>(0x5ba810)(*glob<DWORD *>(0x5fe8f8));
    if (!w[0x310] || !w[0x311])
        return;
    if (first_use(1)) {
        zs_ctor(glob<BYTE *>(TEXT));
        atexit_exe(fn_addr(0x59fc90));
    }
    bool local = !(BYTE)vcall<DWORD>(*glob<void **>(APP), 84) && *glob<DWORD *>(0x6bde9c);
    bool pointer;
    if ((BYTE)zax::fn<DWORD(__cdecl *)()>(0x4e9d80)()) {
        pointer = local;
    } else {
        pointer = false;
        if (local && part_inventory(*glob<void **>(0x6bde9c))) {
            const DWORD cat = category_index(glob_addr(0x60b780));
            BYTE *inv = part_inventory(*glob<void **>(0x6bde9c));
            pointer = zax::fn<int(THISCALL *)(void *, DWORD)>(0x425290)(inv, cat) != -1;
        }
    }
    if (pointer)
        draw_pointer(w, frame);

    // the HUD frames and the auto-map
    BYTE *rules = custom_rules();
    BYTE *hud = rules_object(rules, 0x1c, 0x60, [](BYTE *o) {
        obj_ctor(o);
        zax::fn<void *(THISCALL *)(void *)>(0x53f000)(o + 0x28);
        zax::fn<void *(THISCALL *)(void *)>(0x53f000)(o + 0x38);
        zax::fn<void *(THISCALL *)(void *)>(0x53f000)(o + 0x48);
        dw(o, 0) = vt(0x5f9e98);
        obj_init_fields(o);
    });
    BYTE *main = image(glob_addr(0x624cd8));
    BYTE *spacer = image(glob_addr(0x624ccc));
    BYTE *under = image(glob_addr(0x624d70));
    BYTE *map = image(glob_addr(0x624d64));
    const float open = fstp(zax::fn<LD(THISCALL *)(void *)>(0x507f40)(bp(dw(w, 0x344))));
    const int bar = zax::fn<int(THISCALL *)(void *, void *)>(0x429880)(bp(dw(w, 0x340)), frame);
    const DWORD mh = fh(map) - 0x11;
    const int shift = x87_fistp(fld(fstp(fild64(mh) * open)));
    const int main_h = (int)fh(main);
    int y = bar - shift - main_h;
    y -= (int)vcall<DWORD>(main, 44);
    if (!x87_le(open, cst(glob_addr(0x5ebed0)))) {
        const int a = (int)fh(main);
        const int b = (int)fh(frame);
        const int under_w = (int)fw(under);
        const int d = (int)fh(main);
        const int e = (int)fh(frame);
        zax::fn<DWORD(THISCALL *)(void *, int, int, int, int, void *)>(0x4b3a70)(frame, 0, e - d, under_w, 0x11 - a + b, glob<void *>(0x6be368));
    }
    blit_at(frame, map, 0, y + 1, glob_addr(MASK));
    zax::fn<DWORD(THISCALL *)(void *, void *, int, int)>(0x508180)(bp(dw(w, 0x344)), frame, (int)dw(hud, 8), (int)dw(hud, 0xc) + y);
    int top = bar - (int)fh(main);
    top -= (int)vcall<DWORD>(main, 44);
    blit_at(frame, under, 0, top, glob_addr(MASK));
    const int main_w = (int)fw(main);
    blit_at(frame, main, (int)fw(frame) - main_w, top, glob_addr(MASK));
    int dx = 0, item_dx = 0;
    if (fw(frame) == 0x320) {
        dx = (int)fw(spacer);
        item_dx = 0x50;
        blit_at(frame, spacer, (int)fw(under), top, glob_addr(MASK));
    }

    // the ore and crystal counts of the quest and Salvage King game types
    bool counts = false;
    if (dw(w, 0x58c) != 0xffffffffu && list_object_at(glob_addr(GAME_TYPES), dw(w, 0x58c)))
    {
        BYTE *gt = game_type(w);
        CClassInfo *quest = class_at(fn_addr(0x551c20));
        counts = is_kind_of(gt, quest);
        if (!counts) {
            BYTE *gt2 = game_type(w);
            CClassInfo *salvage = class_at(fn_addr(0x5612a0));
            counts = is_kind_of(gt2, salvage);
        }
    }
    if (counts) {
        if (first_use(2))
            *glob<DWORD *>(0x713f0c) = zax::fn<DWORD(THISCALL *)(void *, const char *, int)>(0x591fc0)(glob<void *>(ITEMS), glob<const char *>(0x60b7c8), -1);
        if (first_use(4))
            *glob<DWORD *>(0x713ed0) = zax::fn<DWORD(THISCALL *)(void *, const char *, int)>(0x591fc0)(glob<void *>(ITEMS), glob<const char *>(0x60b7d4), -1);
        if (first_use(8)) {
            zs_ctor(glob<BYTE *>(0x713ec8));
            atexit_exe(fn_addr(0x59fc80));
        }
        if (first_use(0x10)) {
            zs_ctor(glob<BYTE *>(0x713ea4));
            atexit_exe(fn_addr(0x59fc70));
        }
        if ((BYTE)zax::fn<DWORD(THISCALL *)(void *)>(0x4e13d0)(glob<void *>(0x713ec8))) {
            zax::fn<DWORD(__fastcall *)(DWORD, void *)>(0x4d66d0)(*glob<DWORD *>(0x713ed0), glob<void *>(0x713ec8));
            zax::fn<DWORD(__fastcall *)(DWORD, void *)>(0x4d66d0)(*glob<DWORD *>(0x713f0c), glob<void *>(0x713ea4));
        }
        BYTE *ore = list_object_at(glob_addr(ICONS), list_find(glob_addr(ICONS), glob_addr(0x624d54)));
        zs_format(glob<void *>(TEXT), glob<const char *>(0x624d4c), zs_c_str(glob<void *>(0x713ec8)), (int)fl<short>(player, 0xee));
        if (fl<short>(player, 0xf0))
            zs_append(glob<void *>(TEXT), glob<const char *>(0x624d48), (int)fl<short>(player, 0xf0));
        blit_at(frame, ore, (int)dw(opts, 0x214) + dx, bar - (int)dw(opts, 0x218), glob_addr(MASK));
        draw_text(font(dw(opts, 0x234)), frame, (int)dw(opts, 0x21c) + dx, bar - (int)dw(opts, 0x220), zs_c_str(glob<void *>(TEXT)), 0, 0);
        BYTE *crystal = record(glob_addr(ICONS), glob_addr(ICON_RECORDS), glob_addr(ICON_COUNT), list_find(glob_addr(ICONS), glob_addr(0x624d34)));
        zs_format(glob<void *>(TEXT), glob<const char *>(0x624d4c), zs_c_str(glob<void *>(0x713ea4)), (int)fl<short>(player, 0xf2));
        if (fl<short>(player, 0xf4))
            zs_append(glob<void *>(TEXT), glob<const char *>(0x624d48), (int)fl<short>(player, 0xf4));
        blit_at(frame, crystal, (int)dw(opts, 0x224) + dx, bar - (int)dw(opts, 0x228), glob_addr(MASK));
        draw_text(font(dw(opts, 0x234)), frame, (int)dw(opts, 0x22c) + dx, bar - (int)dw(opts, 0x230), zs_c_str(glob<void *>(TEXT)), 0, 0);
    }

    // the inventory window's frame and the player's number
    BYTE *screen = bp(dw(w, 0x340));
    if (!dw(screen, 0x110))
        zax::fn<void *(THISCALL *)(void *)>(0x4293c0)(screen);
    blit_at(frame, bp(dw(screen, 0x110)), (int)dw(opts, 0x204), bar - (int)dw(opts, 0x208), glob_addr(MASK));
    const short number = fl<short>(player, 0xec);
    zax::fn<DWORD(THISCALL *)(void *, void *, int, int, int, DWORD)>(0x4a6b90)(font(dw(opts, 0x200)), frame, (int)dw(opts, 0x20c), bar - (int)dw(opts, 0x210),
                                                                       (int)number, 0);

    // the help texts, the item message or the game type's time
    const LD zero = cst(glob_addr(0x5ebed0));
    if (!x87_le(fl<float>(w, 0x594), zero)) {
        const bool h1 = !x87_le(fl<float>(w, 0x5a8), zero);
        const bool h2 = !h1 && !x87_le(fl<float>(w, 0x5ac), zero);
        const bool h3 = !h1 && !h2 && !x87_le(fl<float>(w, 0x5b0), zero);
        if (h1 || h2 || h3) {
            BYTE keys[4], fmt[4];
            zs_ctor(keys);
            zax::fn<DWORD(THISCALL *)(void *, DWORD, void *)>(0x470b60)(vcall<void *>(w, 88), 0xa, keys);
            zs_ctor(fmt);
            zs_format(fmt, field_name(opts + 0x298), zs_c_str(keys));
            if (h3)
                zs_format(glob<void *>(TEXT), glob<const char *>(0x624d20), field_name(opts + 0x280), zs_c_str(fmt));
            else
                zs_format(glob<void *>(TEXT), glob<const char *>(0x624d28), field_name(opts + 0x280), field_name(opts + (h1 ? 0x288 : 0x290)), zs_c_str(fmt));
            BYTE *f1 = font(dw(opts, 0x27c));
            BYTE *f2 = font(dw(opts, 0x27c));
            const char *s = zs_c_str(glob<void *>(TEXT));
            DWORD h = seq_height(f1);
            if (h3)
                h >>= 1;
            draw_lines(f2, frame, (int)dw(opts, 0x274) + dx, bar - (int)dw(opts, 0x278) - (int)h, s, 2, 1);
            zs_dtor(fmt);
            zs_dtor(keys);
        } else if (!x87_le(fl<float>(w, 0x5a4), zero)) {
            BYTE *icon = record(glob_addr(ICONS), glob_addr(ICON_RECORDS), glob_addr(ICON_COUNT), dw(w, 0x598));
            BYTE text[4];
            zs_ctor(text);
            DWORD fi = 0;
            switch (dw(w, 0x5a0)) {
            case 0:
                fi = dw(opts, 0x25c);
                zs_assign(text, second_member(opts + 0x268));
                break;
            case 1:
                fi = dw(opts, 0x258);
                zs_assign(text, second_member(opts + 0x260));
                break;
            case 2:
                fi = dw(opts, 0x25c);
                zax::fn<DWORD(__fastcall *)(DWORD, void *)>(0x4d66d0)(dw(w, 0x59c), text);
                break;
            }
            BYTE *f = font_loading(fi);
            blit_at(frame, icon, (int)dw(opts, 0x248) + dx, bar - (int)dw(opts, 0x24c), glob_addr(MASK));
            const char *s = zs_c_str(text);
            draw_text(f, frame, (int)dw(opts, 0x250) + (int)dw(opts, 0x248) + dx, (int)dw(opts, 0x254) - (int)dw(opts, 0x24c) + bar, s, 1, 0);
            zs_dtor(text);
        }
    } else {
        float t = 0.0f;
        if (dw(w, 0x58c) != 0xffffffffu && *glob<DWORD *>(GT_RECORDS) && record(glob_addr(GAME_TYPES), glob_addr(GT_RECORDS), glob_addr(GT_COUNT), dw(w, 0x58c)))
            t = fstp(vcall<LD>(game_type(w), 31));
        zax::fn<void *(THISCALL *)(void *, float)>(0x4e0b30)(glob<void *>(TEXT), t);
        BYTE *f = font_loading(dw(opts, 0x1f4));
        draw_text(f, frame, (int)dw(opts, 0x1f8) + dx, bar - (int)dw(opts, 0x1fc), zs_c_str(glob<void *>(TEXT)), 0, 0);
    }

    // the HUD records
    for (DWORD i = 0; i < dw(player, 0x104); i++) {
        BYTE *rec = bp(reinterpret_cast<DWORD *>(dw(player, 0x100))[i]);
        BYTE *def = record_loading(glob_addr(ITEMS), glob_addr(ITEM_RECORDS), glob_addr(ITEM_COUNT), glob_addr(ITEM_FOLDER), dw(rec, 0));
        if (def && !is_kind_of(def, zax_item_definition_class()))
            def = nullptr;
        BYTE *r = vcall<BYTE *>(def, 32);
        if (!r)
            r = bp(dw(zax::fn<BYTE *(THISCALL *)(void *)>(0x5b7af0)(def), 0x24));
        int off = item_dx;
        if (item_dx && r && is_kind_of(r, class_at(fn_addr(0x56ed30))))
            off = -0x50;
        vcall<DWORD>(r, 28, dw(bp(reinterpret_cast<DWORD *>(dw(player, 0x100))[i]), 4), frame, off);
    }
}

// 0x5a0a30
void THISCALL ZaxWorld_DrawStatusText(BYTE *w)
{
    float scale = 1.0f;
    if (vcall<DWORD>(bp(dw(w, 0x2a8)), 29) == 8)
        scale = 0.5f;
    const float saved = fstp(zax::fn<LD(THISCALL *)(void *)>(0x433c20)(glob<void *>(MASK)));
    float alpha = 1.0f;
    if (x87_lt(fl<float>(w, 0x368), scale))
        alpha = fstp(fld(fl<float>(w, 0x368)) / scale);
    zax::fn<void(THISCALL *)(void *, float)>(0x433be0)(glob<void *>(MASK), alpha);
    BYTE *f = font(list_find(glob_addr(FONTS), glob_addr(0x624d84)));
    const DWORD cx = fw(bp(dw(w, 0x2a8))) >> 1;
    const DWORD cy = fh(bp(dw(w, 0x2a8))) >> 1;
    const int bottom = zax::fn<int(THISCALL *)(void *, const char *)>(0x4a6bf0)(f, zs_c_str(w + 0x364));
    const int width = (int)fw(bp(dw(w, 0x2a8)));
    int rect[4] = {0, (int)cy - bottom, width, (int)cy + bottom};
    if (vcall<DWORD>(bp(dw(w, 0x2a8)), 29) != 8) {
        BYTE colour[16];
        zax::fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(colour, 0, 0, 0, (DWORD)x87_ftol((LD)alpha * cst(glob_addr(0x5eee14)) * cst(glob_addr(0x5eb728))));
        if (!w[0x36c])
            vcall<DWORD>(bp(dw(w, 0x2a8)), 28, rect, colour);
    }
    const char *s = zs_c_str(w + 0x364);
    draw_text(f, bp(dw(w, 0x2a8)), (int)cx, (int)cy - bottom / 2, s, 1, 0);
    zax::fn<void(THISCALL *)(void *, float)>(0x433be0)(glob<void *>(MASK), saved);
}

// 0x5a0c00 (text (a ZString), fraction, no bar)
void THISCALL ZaxWorld_ShowStatusText(BYTE *w, const void *text, float fraction, BYTE no_bar)
{
    zax::fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(w + 0x364, text);
    fl<float>(w, 0x368) = fraction;
    w[0x36c] = no_bar;
    zax::fn<void(THISCALL *)(void *)>(0x5a0a30)(w);
    for (DWORD n = dw(bp(dw(w, 4)), 0x44) + 1; n; n--) {
        blit_at(bp(dw(w, 4)), bp(dw(w, 0x2a8)), 0, 0, glob_addr(COPY));
        zax::fn<DWORD(THISCALL *)(void *)>(0x5693a0)(bp(dw(w, 4)));
    }
}

extern "C" void *_AddressOfReturnAddress();   // the compiler's intrinsic (MSVC's <intrin.h>)

namespace {
BYTE *net_player() { return zax::fn<BYTE *(__fastcall *)(DWORD)>(0x5ba810)(*glob<DWORD *>(0x5fe8f8)); }
}  // namespace

// 0x4698f0 (frame, x, y)
//
// The team names' walk past its end, as the exe does it [verified 2026-11-11: the instructions 0x4698f0-0x469b6c].
// The exe's frame after its prologue (sub esp, 0x14; push ebx, ebp, esi, edi): [esp + 0x10] the text, +0x14 the
// player, +0x18 a scratch slot, +0x1c and +0x20 the two names (0x469a0a, 0x469a13), +0x24 the return address,
// +0x28/+0x2c/+0x30 the arguments frame, x, y. After reading the arguments (frame stays in ebx) it reuses their
// slots: +0x28 the scoreboard (0x469a1e), +0x2c the team index (0x469a22), +0x30 a pointer into the frame that
// starts at the first name (0x469a37) and moves on by 4 per team, drawn or not (0x469ad0), without a bound. Per
// team with a non-null TeamAt it formats "%s = " with the dword the pointer points to (0x469a5d). So team 0 and 1
// get the names, team 2 the return address (the caller's code bytes as text), team 3 the scoreboard (its bytes as
// text), team 4 the index itself (4: the format reads address 4 and the game crashes), team 5 the pointer itself
// (the stack address's bytes, then the stack after it), team 6 on the caller's frame.
// Here: the hook enters this function by a jump, so its return address and arguments lie where the exe's do;
// _AddressOfReturnAddress() is the exe's [esp + 0x24]. The scoreboard, the index and the pointer are kept in the
// argument slots as the exe keeps them (the arguments are read through the same pointer first and never used
// after), the pointer holds the exe's value (the address of [esp + 0x1c] plus 4 per team), and a read through it
// takes the two names from `names` (this frame cannot hold them right below the return address, which is where
// the exe has them) and every later dword from the stack itself: the return address, the scoreboard, the index,
// the pointer, the caller's frame - the same memory as the exe's.
void THISCALL CaptureTheFlag_DrawHud(BYTE *gt, void *, int, int)
{
    volatile DWORD *const args = static_cast<volatile DWORD *>(_AddressOfReturnAddress());   // the exe's [esp + 0x24]
    const DWORD names_at = (DWORD)(uintptr_t)args - 8;                                       // the exe's [esp + 0x1c]
    BYTE *p = net_player();
    BYTE *f = font(dw(gt, 0xd4));
    BYTE text[4];
    zs_ctor(text);
    void *const frame = reinterpret_cast<void *>(args[1]);
    const int x = (int)args[2], y = (int)args[3];
    BYTE *flag = record(glob_addr(IMAGES), glob_addr(IMAGE_RECORDS), glob_addr(IMAGE_COUNT), dw(p, 0x14) ? dw(gt, 0xec) : dw(gt, 0xe8));
    blit_at(frame, flag, (int)dw(gt, 0xe0) + x, (int)dw(gt, 0xe4) + y, glob_addr(COPY));
    const int tx = (int)dw(gt, 0xd8) + x;
    int ty = (int)dw(gt, 0xdc) + y;
    DWORD names[2];
    names[0] = (DWORD)(uintptr_t)zs_c_str(glob<void *>(0x6bd124));
    names[1] = (DWORD)(uintptr_t)zs_c_str(glob<void *>(0x6bd114));
    args[1] = (DWORD)(uintptr_t)zax::fn<void *(__cdecl *)()>(0x566a80)();   // [esp + 0x28] the scoreboard
    args[2] = 0;                                                              // [esp + 0x2c] the team index
    if (zax::fn<DWORD(THISCALL *)(void *)>(0x566cf0)(reinterpret_cast<void *>(args[1]))) {
        args[3] = names_at;   // [esp + 0x30] the pointer, at the first name
        do {
            void *team = zax::fn<void *(THISCALL *)(void *, DWORD)>(0x566d00)(reinterpret_cast<void *>(args[1]), args[2]);
            if (team) {
                const DWORD at = args[3];
                const DWORD name = at - names_at < 8 ? names[(at - names_at) >> 2] : *reinterpret_cast<const DWORD *>(at);
                zs_format(text, glob<const char *>(0x60f5b4), reinterpret_cast<const char *>(name));
                draw_text(f, frame, tx, ty, zs_c_str(text), 2, 0);
                zs_format(text, glob<const char *>(0x60a5a0), zax::fn<DWORD(THISCALL *)(void *)>(0x5669d0)(team));
                draw_text(f, frame, tx, ty, zs_c_str(text), 0, 0);
                ty += (int)seq_height(f);
            }
            args[2] = args[2] + 1;
            args[3] = args[3] + 4;
        } while (args[2] < zax::fn<DWORD(THISCALL *)(void *)>(0x566cf0)(reinterpret_cast<void *>(args[1])));
    }
    zs_format(text, glob<const char *>(0x60f5b4), zs_c_str(glob<void *>(0x6bd11c)));
    draw_text(f, frame, tx, ty, zs_c_str(text), 2, 0);
    zs_format(text, glob<const char *>(0x60a5a0), (int)fl<short>(p, 0xf6));
    draw_text(f, frame, tx, ty, zs_c_str(text), 0, 0);
    seq_height(f);
    zs_dtor(text);
}

// 0x4796a0 (frame, x, y)
void THISCALL DeathMatch_DrawHud(BYTE *gt, void *frame, int x, int y)
{
    BYTE *p = net_player();
    BYTE *f1 = font(dw(gt, 0xc8));
    BYTE text[4];
    zs_ctor(text);
    zs_format(text, glob<const char *>(0x60a5a0), (int)fl<short>(p, 0xf6));
    draw_text(f1, frame, (int)dw(gt, 0xcc) + x, (int)dw(gt, 0xd0) + y, zs_c_str(text), 1, 0);
    BYTE *f2 = font(dw(gt, 0xd4));
    const int tx = (int)dw(gt, 0xd8) + x;
    int ty = (int)dw(gt, 0xdc) + y;
    void *board = zax::fn<void *(__cdecl *)()>(0x566a80)();
    DWORD n = dw(gt, 0xe0);
    const DWORD rows = zax::fn<DWORD(THISCALL *)(void *)>(0x482de0)(board);
    if (n >= rows)
        n = rows;
    for (DWORD i = 0; i < n; i++) {
        zs_format(text, glob<const char *>(0x60a5a0), i + 1);
        draw_text(f2, frame, tx + (int)dw(gt, 0xe8), ty, zs_c_str(text), dw(gt, 0xec), 0);
        void *row = zax::fn<void *(THISCALL *)(void *, DWORD)>(0x566e50)(board, i);
        const char *name = zs_c_str(zax::fn<void *(THISCALL *)(void *)>(0x5664b0)(row));
        draw_text(f2, frame, tx + (int)dw(gt, 0xf0), ty, name, dw(gt, 0xf4), 0);
        void *row2 = zax::fn<void *(THISCALL *)(void *, DWORD)>(0x566e50)(board, i);
        zs_format(text, glob<const char *>(0x60a5a0), zax::fn<DWORD(THISCALL *)(void *)>(0x483750)(row2));
        draw_text(f2, frame, tx + (int)dw(gt, 0xf8), ty, zs_c_str(text), dw(gt, 0xfc), 0);
        ty += zax::fn<int(THISCALL *)(void *)>(0x4a6140)(f2) + (int)dw(gt, 0xe4);
    }
    zs_dtor(text);
}

// 0x561760 (frame, x, y)
void THISCALL SalvageKing_DrawHud(BYTE *gt, void *frame, int x, int y)
{
    BYTE *f = font_loading(dw(gt, 0xd0));
    BYTE text[4];
    zs_ctor(text);
    const int tx = (int)dw(gt, 0xd4) + x, ty = (int)dw(gt, 0xd8) + y;
    if (BYTE *p = net_player()) {
        zs_format(text, glob<const char *>(0x60a5a0), (int)fl<short>(p, 0xf6));
        draw_text(f, frame, tx, ty, zs_c_str(text), 1, 0);
    }
    zs_dtor(text);
}

ZAX_HOOKS(hud_world)
{
    const Hook list[] = {
#include "hud_world_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
