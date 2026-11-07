// The detail renderers of the inventory screen (phase 4, rendering): CInventoryDetailRenderer's classes draw an
// item's detail (slot 24: item, frame, x, y, inventory) and describe it as a line of the item list (slot 25:
// item, inventory; a new line object, 0 for nothing, or the static 0x714140 for "listed elsewhere"). Classes in
// hud_classes.cpp.
//
// The line object [confirmed: every slot 25]: 0x1c bytes from the pool, CObject with two ZStrings (+0xc the
// name, +0x14 the text), vtable 0x5f1140; +8 the column layout (0 or 1), +0x10 and +0x18 the colours (3; the
// weapon's +0x18 says how many shots are left: 0 none, 1 few, 2 more than "Warn Getting Low Threshold"). A
// failed pool allocation writes to address 8 as the exe does.
// Where they draw [confirmed: the slots below]: the text in the font of the renderer (or a named one) at the
// detail position plus (0x2d, 0x14), meters at (5, 0x11)-(0x55, 0x1c); the overlay sequences of the "Detail
// Image" list 0x6c0948 at the inventory display options' detail position (+0x44/+0x48), then the next
// renderer. The texts' formats come from Local.txt entries (the renderer's "Format", the registered strings
// 0x714088-0x7143xx).
#include "ai_helpers.h"
#include "x87.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD ITEM_LIST = 0x6c0c08, ITEM_RECORDS = 0x6c0c38, ITEM_COUNT = 0x6c0c3c;
constexpr DWORD FONT_LIST = 0x6be1e0, FONT_RECORDS = 0x6be210, FONT_COUNT = 0x6be214, FONT_FOLDER = 0x6be218;
constexpr DWORD IMAGE_LIST = 0x6c0948, IMAGE_RECORDS = 0x6c0978, IMAGE_COUNT = 0x6c097c, IMAGE_FOLDER = 0x6c0980;
constexpr DWORD MASK_BLEND = 0x6bcc68, LISTED_ELSEWHERE = 0x714140;

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
BYTE *record_object_loading(DWORD list, DWORD records_at, DWORD count_at, DWORD folder, DWORD i)
{
    if (!*at<BYTE **>(records_at))
        return nullptr;
    return list_object(list, records_at, count_at, folder, i);
}
BYTE *font_loading(DWORD i) { return record_object_loading(glob_addr(FONT_LIST), glob_addr(FONT_RECORDS), glob_addr(FONT_COUNT), glob_addr(FONT_FOLDER), i); }
BYTE *font_named(DWORD name) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(FONT_LIST), vcall<DWORD>(glob<void *>(FONT_LIST), 3, at<const char *>(name), -1)); }
DWORD font_index_named(DWORD name) { return vcall<DWORD>(glob<void *>(FONT_LIST), 3, at<const char *>(name), -1); }
bool kind(void *o, DWORD getter) { return is_kind_of(o, class_at(getter)); }

// The inventory display options as the slots inline 0x429de0
BYTE *display_options()
{
    BYTE *holder = bp(dw(*glob<BYTE **>(0x713f14), 0x2e0));
    BYTE *rules = bp(dw(holder, 0x38));
    if (rules && is_kind_of(rules, class_at(fn_addr(0x5a1350))))
        return fn<BYTE *(THISCALL *)(void *)>(0x5a1d40)(bp(dw(holder, 0x38)));
    return fn<BYTE *(THISCALL *)(void *)>(0x5a1d40)(nullptr);
}

void font_text(BYTE *font, void *frame, int x, int y, const char *text, DWORD a5, DWORD a6)
{
    fn<DWORD(THISCALL *)(void *, void *, int, int, const char *, DWORD, DWORD)>(0x4a6470)(font, frame, x, y, text, a5, a6);
}
void blit_at(void *frame, void *src, int x, int y, void *mode) { fn<DWORD(THISCALL *)(void *, void *, int, int, void *)>(0x4b3280)(frame, src, x, y, mode); }
void *sequence_blend(BYTE *seq) { return fn<void *(THISCALL *)(void *)>(0x56bf00)(seq); }
DWORD sync_frame(DWORD frames, DWORD offset, DWORD fps) { return fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x55e4b0)(frames, offset, fps); }
DWORD meter_draw(BYTE *meter, void *frame, void *rect, DWORD value, DWORD max, DWORD a5)
{
    return fn<DWORD(THISCALL *)(void *, void *, void *, DWORD, DWORD, DWORD)>(0x486d10)(meter, frame, rect, value, max, a5);
}
const char *local_text(BYTE *entry) { return fn<const char *(THISCALL *)(void *)>(0x5007a0)(entry); }
const char *registered(DWORD z) { return zs_c_str(at<void *>(z)); }
DWORD properties_count(void *ref) { return fn<DWORD(THISCALL *)(void *)>(0x482de0)(ref); }
void display_name(DWORD index, void *z) { fn<void(__fastcall *)(DWORD, void *)>(0x4d66d0)(index, z); }
long double max_charge(void *item) { return fn<long double(THISCALL *)(void *)>(0x5afe50)(item); }
DWORD total_count(void *inv, DWORD index) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4257f0)(inv, index); }
int sprintf_exe(char *out, const char *fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    const int n = crt_vsprintf(out, fmt, ap);
    __builtin_va_end(ap);
    return n;
}
template <typename... A> void zs_format(void *z, const char *fmt, A... a) { fn<int(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, fmt, a...); }
template <typename... A> void zs_append_format(void *z, const char *fmt, A... a) { fn<int(__cdecl *)(void *, const char *, ...)>(0x4e0a30)(z, fmt, a...); }
bool zs_empty(void *z) { return (BYTE)fn<DWORD(THISCALL *)(void *)>(0x4e13d0)(z) != 0; }

// A new line object of the item list (pool 0x1c; +8 2 from the constructor)
BYTE *new_line()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x1c));
    if (o) {
        obj_ctor(o);
        zs_ctor(o + 0xc);
        zs_ctor(o + 0x14);
        vtable_and_defaults(o, vt(0x5f1140));
        dw(o, 8) = 2;
    }
    return o;
}
// The common start of most lines: +8 0, +0x10 and +0x18 3, the item's name
void line_name(BYTE *line, void *item)
{
    dw(line, 8) = 0;
    dw(line, 0x10) = 3;
    dw(line, 0x18) = 3;
    display_name(properties_count(item), line + 0xc);
}
// A function-local static ZString of the exe (guard bit 1, object, atexit function)
void static_string(DWORD guard, DWORD z, DWORD exit_fn)
{
    BYTE &bits = *at<BYTE *>(guard);
    if (bits & 1)
        return;
    bits |= 1;
    zs_ctor(at<BYTE *>(z));
    crt_atexit(at<void(__cdecl *)()>(exit_fn));
}
// fldlg2, fxch, fyl2x: log10 as the x87 computes it
long double x87_log10(long double v)
{
    long double r;
    __asm__("fldlg2\n\tfxch %%st(1)\n\tfyl2x" : "=t"(r) : "0"(v));
    return r;
}
void next_draw(BYTE *self, void *item, void *frame, int x, int y, void *inv)
{
    if (BYTE *next = bp(dw(self, 0xc)))
        vcall<DWORD>(next, 24, item, frame, x, y, inv);
}

}  // namespace

// ---- CInventoryDetailRenderer ------------------------------------------------------------------------------

// 0x47ac40 (slot 23, four arguments): 0
static DWORD THISCALL Detail_Slot23(void *, DWORD, DWORD, DWORD, DWORD) { return 0; }
// 0x5ae060 (slot 25 of the overlays: item, inventory): the next renderer's description, 0 without one
static BYTE *THISCALL Detail_NextDescribe(BYTE *self, void *item, void *inv)
{
    BYTE *next = bp(dw(self, 0xc));
    return next ? vcall<BYTE *>(next, 25, item, inv) : nullptr;
}

// ---- Count, ammunition, mineral, charger, health, deployable ----------------------------------------------

// 0x5b12d0 (slot 24): the renderer's "Format" with the count (slot 41) in its "Font" at (0x2d, 0x14)
static void THISCALL Count_Draw(BYTE *self, BYTE *item, void *frame, int x, int y, void *)
{
    if (!kind(item, fn_addr(0x42a760)))
        return;
    char text[0x160];
    sprintf_exe(text, local_text(self + 8), vcall<DWORD>(item, 41));
    font_text(font_loading(dw(self, 0x10)), frame, x + 0x2d, y + 0x14, text, 1, 0);
}
// 0x5b1440 (slot 25)
static BYTE *THISCALL Count_Describe(BYTE *self, BYTE *item, void *)
{
    if (!kind(item, fn_addr(0x42a760)))
        return nullptr;
    BYTE *line = new_line();
    properties_count(item);
    line_name(line, item);
    zs_format(line + 0x14, local_text(self + 8), vcall<DWORD>(item, 41));
    return line;
}

// 0x5b16f0 (slot 24): 0x7140fc with the count in "Small Green"
static void THISCALL Ammunition_Draw(BYTE *, BYTE *item, void *frame, int x, int y, void *)
{
    if (!kind(item, fn_addr(0x5af700)))
        return;
    char text[0x160];
    const char *fmt = registered(glob_addr(0x7140fc));
    sprintf_exe(text, fmt, vcall<DWORD>(item, 41));
    font_text(font_loading(font_index_named(glob_addr(0x60bb18) /* "Small Green" */)), frame, x + 0x2d, y + 0x14, text, 1, 0);
}
// 0x5b18c0 (slot 25): nothing of its own when a weapon in the inventory uses it (its ammunition, +0x2c of the
// weapon's definition, is this item's definition): the static 0x714140
static BYTE *THISCALL Ammunition_Describe(BYTE *, BYTE *item, BYTE *inv)
{
    DWORD found = 0xffffffff;
    if (inv) {
        const DWORD n = dw(inv, 0x20);
        auto order = [&](DWORD k) { return fn<DWORD(__fastcall *)(void *, DWORD)>(0x5b2aa0)(inv, k); };
        if (order(0) != 0xffffffff)
            for (DWORD k = 0; k < n; k++) {
                if (found != 0xffffffff)
                    return glob<BYTE *>(LISTED_ELSEWHERE);
                const DWORD idx = order(k);
                BYTE *def = item_definition(vcall<BYTE *>(inv, 26, idx));
                const DWORD ammo = dw(def, 0x2c);
                if (ammo == 0xffffffff)
                    continue;
                BYTE *o = record_object(glob_addr(ITEM_LIST), glob_addr(ITEM_RECORDS), glob_addr(ITEM_COUNT), ammo);
                if (o == item_definition(item))
                    found = idx;
            }
        if (found != 0xffffffff)
            return glob<BYTE *>(LISTED_ELSEWHERE);
    }
    if (!kind(item, fn_addr(0x42a760)))
        return nullptr;
    BYTE *line = new_line();
    properties_count(item);
    dw(line, 8) = 1;
    dw(line, 0x10) = 0;
    dw(line, 0x18) = 3;
    fn<void(THISCALL *)(void *, const char *)>(0x4df010)(line + 0xc, registered(glob_addr(0x71411c)));
    display_name(properties_count(item), line + 0x14);
    const char *fmt = registered(glob_addr(0x714118));
    zs_append_format(line + 0x14, fmt, vcall<DWORD>(item, 41));
    return line;
}

// 0x5b1c60 (slot 24): "%d %s" with the count and 0x714318 (several) or 0x7140ec (one) in the options' font
// +0x70 at the detail position itself
static void THISCALL Mineral_Draw(BYTE *, BYTE *item, void *frame, int x, int y, void *)
{
    if (!kind(item, fn_addr(0x5af480)))
        return;
    BYTE *opts = display_options() + 0x3c;
    const DWORD n = vcall<DWORD>(item, 41);
    char text[0x160];
    sprintf_exe(text, glob<const char *>(0x623888) /* "%d %s" */, n, registered(n > 1 ? glob_addr(0x714318) : glob_addr(0x7140ec)));
    font_text(font_loading(dw(opts, 0x34)), frame, x, y, text, 0, 0);
}
// 0x5b1e60 (slot 25): "Ore Deposits" and "Crystals" are listed elsewhere
static BYTE *THISCALL Mineral_Describe(BYTE *, BYTE *item, void *)
{
    auto find = [](DWORD name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(ITEM_LIST), at<const char *>(name), -1); };
    DWORD k = find(glob_addr(0x60b7d4) /* "Ore Deposits" */);
    if (properties_count(item) == k)
        return glob<BYTE *>(LISTED_ELSEWHERE);
    k = find(glob_addr(0x60b7c8) /* "Crystals" */);
    if (properties_count(item) == k)
        return glob<BYTE *>(LISTED_ELSEWHERE);
    if (!kind(item, fn_addr(0x42a760)))
        return nullptr;
    BYTE *line = new_line();
    properties_count(item);
    line_name(line, item);
    const DWORD n = vcall<DWORD>(item, 41);
    zs_format(line + 0x14, registered(n > 1 ? glob_addr(0x714320) : glob_addr(0x7142f0)), n);
    return line;
}

// 0x5b2160 (slot 24): 0x71431c with the definition's "+0x4c" (the charge it gives) in the font +0x68
static void THISCALL Charger_Draw(BYTE *, BYTE *item, void *frame, int x, int y, void *)
{
    BYTE *opts = display_options() + 0x3c;
    const int v = x87_ftol(fld(fl<float>(item_definition(item), 0x4c)));
    char text[0x160];
    sprintf_exe(text, registered(glob_addr(0x71431c)), v);
    font_text(font_loading(dw(opts, 0x2c)), frame, x, y, text, 0, 0);
}
// 0x5b22f0 (slot 25)
static BYTE *THISCALL Charger_Describe(BYTE *, BYTE *item, void *)
{
    BYTE *line = new_line();
    line_name(line, item);
    const int v = x87_ftol(fld(fl<float>(item_definition(item), 0x4c)));
    zs_format(line + 0x14, registered(glob_addr(0x714110)), v);
    return line;
}

// 0x5b2580 (slot 24): "%s: +%g" (0x7140f4, the health item's +0x24, 40 without a CHealthItem: a message every
// 300 draws) in the font +0x70
static void THISCALL Health_Draw(BYTE *, BYTE *item, void *frame, int x, int y, void *)
{
    BYTE *opts = display_options() + 0x3c;
    DWORD heal = 0x42200000;
    if (kind(item, fn_addr(0x5aefe0))) {
        heal = dw(item, 0x24);
    } else {
        DWORD &counter = *glob<DWORD *>(0x62626c);
        if (counter >= 300) {
            counter = 0;
            vcall<DWORD>(reporter(), 17, registered(glob_addr(0x7142f4)));
        }
        counter++;
    }
    BYTE *font = font_loading(dw(opts, 0x34));
    char text[0x160];
    sprintf_exe(text, glob<const char *>(0x62661c) /* "%s: +%g" */, registered(glob_addr(0x7140f4)), (double)fld(raw_float(heal)));
    font_text(font, frame, x, y, text, 0, 0);
}
// 0x5b27b0 (slot 25): listed elsewhere when another item of the same catalogue index comes first in category
// order; else "%s %g%% (%s %u)" with the healing and the inventory's total of the item
static BYTE *THISCALL Health_Describe(BYTE *, BYTE *item, BYTE *inv)
{
    bool other = false, found = false;
    if (inv) {
        const DWORD n = dw(inv, 0x20);
        if (fn<DWORD(__fastcall *)(void *, DWORD)>(0x5b2aa0)(inv, 0) != 0xffffffff) {
            for (DWORD k = 0; k < n; k++) {
                if (other)
                    return glob<BYTE *>(LISTED_ELSEWHERE);
                if (found)
                    break;
                BYTE *it = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x424f60)(inv, fn<DWORD(__fastcall *)(void *, DWORD)>(0x5b2aa0)(inv, k));
                const DWORD a = properties_count(it);
                if (properties_count(item) == a) {
                    found = true;
                    other = item != it;
                }
            }
            if (other)
                return glob<BYTE *>(LISTED_ELSEWHERE);
        }
    }
    DWORD heal = 0x42200000;
    if (kind(item, fn_addr(0x5aefe0)))
        heal = dw(item, 0x24);
    else
        vcall<DWORD>(reporter(), 17, registered(glob_addr(0x7142f4)));
    BYTE *line = new_line();
    properties_count(item);
    line_name(line, item);
    const char *a = registered(glob_addr(0x71410c)), *b = registered(glob_addr(0x714104));
    const DWORD total = total_count(inv, properties_count(item));
    zs_format(line + 0x14, glob<const char *>(0x626624) /* "%s %g%% (%s %u)" */, b, (double)fld(raw_float(heal)), a, total);
    return line;
}

// 0x47af90 (slot 24): "Format" with the deployable's value (slot 49) · 0x5ee864 in its "Font"
static void THISCALL Deployable_Draw(BYTE *self, BYTE *item, void *frame, int x, int y, void *)
{
    if (!kind(item, fn_addr(0x479c40)))
        return;
    const int v = x87_ftol(vcall<long double>(item, 49) * cst(glob_addr(0x5ee864)));
    char text[0x160];
    sprintf_exe(text, local_text(self + 8), v);
    font_text(font_loading(dw(self, 0x10)), frame, x + 0x2d, y + 0x14, text, 1, 0);
}
// 0x47b150 (slot 25)
static BYTE *THISCALL Deployable_Describe(BYTE *self, BYTE *item, void *)
{
    BYTE *d = item && kind(item, fn_addr(0x479c40)) ? item : nullptr;
    if (!d)
        return nullptr;
    BYTE *line = new_line();
    line_name(line, item);
    const int v = x87_ftol(vcall<long double>(d, 49) * cst(glob_addr(0x5ee864)));
    zs_format(line + 0x14, local_text(self + 8), v);
    return line;
}

// ---- Battery, shield, energy text --------------------------------------------------------------------------

namespace {
// The energy meter at (5, 0x11)-(0x55, 0x1c) and "%d/%d %s" (charge, capacity, 0x6bdca4) in the font, centred
// on y + 0x16, at x + 0x2d (battery and shield detail)
void energy_detail(BYTE *item, void *frame, int x, int y, DWORD font_name)
{
    if (!item || !is_kind_of(item, energy_item_class()))
        return;
    const DWORD charge = dw(item, 0x18);
    const float cap = fstp(max_charge(item));
    int rect[4] = {x + 5, y + 0x11, x + 0x55, y + 0x1c};
    BYTE *def = item_definition(item);
    if (!def || !kind(def, fn_addr(0x486220)))
        return;
    meter_draw(def + 0x100, frame, rect, charge, float_bits(cap), 0);
    const char *units = registered(glob_addr(0x6bdca4));
    const int a = x87_ftol(fld(cap)), b = x87_ftol(fld(raw_float(charge)));
    char text[0x150];
    sprintf_exe(text, glob<const char *>(0x6121b8) /* "%d/%d %s" */, b, a, units);
    BYTE *font = font_loading(font_index_named(font_name));
    font_text(font, frame, x + 0x2d, y - (int)(dw(font, 0x80) >> 1) + 0x16, text, 1, 0);
}
}  // namespace

// 0x483b90 (slot 24): in "Small Blue"
static void THISCALL Battery_Draw(BYTE *, BYTE *item, void *frame, int x, int y, void *) { energy_detail(item, frame, x, y, glob_addr(0x60c53c) /* "Small Blue" */); }
// 0x483db0 (slot 25): "%s: %d/%d %s" (0x6bdcb4, charge, capacity, 0x6bdca4)
static BYTE *THISCALL Battery_Describe(BYTE *, BYTE *item, void *)
{
    BYTE *e = item && is_kind_of(item, energy_item_class()) ? item : nullptr;
    if (!e)
        return nullptr;
    BYTE *line = new_line();
    line_name(line, item);
    const DWORD charge = dw(e, 0x18);
    const float cap = fstp(max_charge(e));
    const char *a = registered(glob_addr(0x6bdcb4)), *b = registered(glob_addr(0x6bdca4));
    const int c = x87_ftol(fld(cap)), d = x87_ftol(fld(raw_float(charge)));
    zs_format(line + 0x14, glob<const char *>(0x6121c4) /* "%s: %d/%d %s" */, a, d, c, b);
    return line;
}
// 0x484a40 (slot 24): in "Small Gold"
static void THISCALL ShieldDetail_Draw(BYTE *, BYTE *item, void *frame, int x, int y, void *) { energy_detail(item, frame, x, y, glob_addr(0x612264) /* "Small Gold" */); }
// 0x484c60 (slot 25): "%s: %d/%d" (0x6bdcac, charge, capacity)
static BYTE *THISCALL ShieldDetail_Describe(BYTE *, BYTE *item, void *)
{
    BYTE *e = item && is_kind_of(item, energy_item_class()) ? item : nullptr;
    if (!e)
        return nullptr;
    BYTE *line = new_line();
    line_name(line, item);
    const DWORD charge = dw(e, 0x18);
    const int c = x87_ftol(max_charge(e)), d = x87_ftol(fld(raw_float(charge)));
    zs_format(line + 0x14, glob<const char *>(0x612270) /* "%s: %d/%d" */, registered(glob_addr(0x6bdcac)), d, c);
    return line;
}

// 0x4843e0 (slot 24): the charge with "Digits Of Precision" digits ("%.Nf") put into "Singular Format" (up to
// 1) or "Plural Format" in the "Font" at (0x2d, 0x14)
static void THISCALL EnergyText_Draw(BYTE *self, BYTE *item, void *frame, int x, int y, void *)
{
    if (!kind(item, fn_addr(0x5afbd0)))
        return;
    BYTE a[4], b[4], c[4];
    zs_ctor(a);
    zs_ctor(b);
    zs_ctor(c);
    zs_format(a, glob<const char *>(0x612234) /* "%%.%cf" */, dw(self, 0x18) + 0x30);
    zs_format(b, zs_c_str(a), (double)fld(fl<float>(item, 0x18)));
    const char *fmt = local_text(c03(fld(fl<float>(item, 0x18)), cst(glob_addr(0x5ec3f4))) ? self + 8 : self + 0x10);
    zs_format(c, fmt, zs_c_str(b));
    BYTE *font = font_loading(dw(self, 0x1c));
    font_text(font, frame, x + 0x2d, y + 0x14, zs_c_str(c), 1, 0);
    zs_dtor(c);
    zs_dtor(b);
    zs_dtor(a);
}
// 0x4845d0 (slot 25): the charge printed with "%.Nlf" into a buffer of N + log10(charge) + 3 characters
static BYTE *THISCALL EnergyText_Describe(BYTE *self, BYTE *item, void *)
{
    BYTE *e = item && kind(item, fn_addr(0x5afbd0)) ? item : nullptr;
    if (!e)
        return nullptr;
    BYTE s[4];
    zs_ctor(s);
    const long double v = fld(fl<float>(e, 0x18));
    const double dv = (double)v;
    int digits = x87_ftol(x87_log10(v));
    digits = digits > 0 ? digits : 0;
    char *buf = fn<char *(THISCALL *)(void *, int)>(0x4e1590)(s, (int)dw(self, 0x18) + digits + 3);
    char fmt[8];
    memcpy(fmt, glob<const char *>(0x61223c) /* "%.Xlf" */, 6);
    fmt[2] = (char)(self[0x18] + 0x30);
    fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(buf, fmt, dv);
    fn<void(THISCALL *)(void *, int)>(0x4e16d0)(s, -1);
    BYTE *line = new_line();
    line_name(line, item);
    const char *f = local_text(c03(fld(fl<float>(e, 0x18)), cst(glob_addr(0x5ec3f4))) ? self + 8 : self + 0x10);
    zs_format(line + 0x14, f, zs_c_str(s));
    zs_dtor(s);
    return line;
}
// 0x484740 (slot 18: a1): an old "Format" (dynamic field of descriptor 0x6bdca0) becomes both formats: its first
// "%…f" turned into "%s" (the text after the 'f' but one kept), the dynamic field removed
static DWORD THISCALL EnergyText_Upgrade(BYTE *self, DWORD)
{
    BYTE *desc = *glob<BYTE **>(0x6bdca0);
    if (!(BYTE)fn<DWORD(THISCALL *)(void *, void *)>(0x418fc0)(desc, self))
        return 0;
    BYTE old[4];
    fn<void *(THISCALL *)(void *, void *)>(0x4dedf0)(old, self + dw(*glob<BYTE **>(0x6bdca0), 0x10));
    fn<DWORD(THISCALL *)(void *, void *)>(0x418fb0)(*glob<void **>(0x6bdca0), self);
    auto strchr_exe = [](char *s, int c) { return fn<char *(__cdecl *)(char *, int)>(0x5d08c0)(s, c); };
    char *p = strchr_exe(fn<char *(THISCALL *)(void *, int)>(0x4e1590)(old, 0), '%');
    while (p) {
        char *q = p + 1;
        if (*q != '%' && *q) {
            const DWORD r = ptr_value(strchr_exe(q, 'f')) + 1;
            if (r) {
                p[1] = 's';
                const char *from = reinterpret_cast<const char *>(static_cast<uintptr_t>(r + 1));
                const DWORD n = (DWORD)lstrlenA(from) + 1;
                for (DWORD i = 0; i < n; i++)   // forward, as rep movs
                    p[2 + i] = from[i];
                break;
            }
        }
        if (!*q)
            break;
        p = strchr_exe(q, '%');
    }
    fn<void(THISCALL *)(void *, int)>(0x4e16d0)(old, -1);
    vcall<DWORD>(*glob<void **>(0x6bdcbc), 19, self, zs_c_str(old), 0);
    vcall<DWORD>(*glob<void **>(0x6bdcc0), 19, self, zs_c_str(old), 0);
    zs_dtor(old);
    return 1;
}

// ---- Character, Sphere of Zin --------------------------------------------------------------------------------

// 0x580df0 (slot 24): "Ability: %s level %d", "Have %d of %d %ss needed to advance" (the type definition's
// "Advance/Item Needed To Advance" and "Advance/Level"), the energy meter and "Special (%s) Charge", all in
// "Small Gold" at x + 0x2d
static void THISCALL CharacterDetail_Draw(BYTE *, BYTE *item, void *frame, int x, int y, BYTE *inv)
{
    BYTE *def = item_definition(item);
    BYTE *cdef = def && kind(def, fn_addr(0x5807f0)) ? def : nullptr;
    BYTE *citem = item && kind(item, fn_addr(0x57fd00)) ? item : nullptr;
    BYTE *type = record_object(glob_addr(0x6c0800), glob_addr(0x6c0830), glob_addr(0x6c0834), dw(cdef, 0x18));
    type = type && kind(type, fn_addr(0x581060)) ? type : nullptr;
    if (!cdef || !citem || !type)
        return;
    const char *name = fn<const char *(THISCALL *)(void *, DWORD)>(0x591e10)(glob<void *>(0x713790), dw(cdef, 0x174));
    const DWORD level = dw(citem, 0x1c);
    char text[0x58];
    sprintf_exe(text, glob<const char *>(0x622678) /* "Ability: %s level %d" */, name, level + 1);
    BYTE *font = font_named(glob_addr(0x612264) /* "Small Gold" */);
    font_text(font, frame, x + 0x2d, y - 5, text, 1, 0);
    const DWORD needed = level < dw(type, 0x30) ? dw(bp(dw(type, 0x2c)), level * 4) : 0;
    const DWORD wanted = dw(type, 0x28);
    const char *what = path_name(fn<const char *(THISCALL *)(void *, DWORD)>(0x591d00)(glob<void *>(ITEM_LIST), wanted));
    const DWORD have = fn<DWORD(__fastcall *)(void *, DWORD)>(0x4268d0)(inv, wanted);
    sprintf_exe(text, glob<const char *>(0x622654) /* "Have %d of %d %ss needed to advance" */, have, needed, what);
    font_text(font, frame, x + 0x2d, y + 5, text, 1, 0);
    const DWORD charge = dw(citem, 0x18);
    const DWORD cap = float_bits(fstp(max_charge(citem)));
    int rect[4] = {x + 5, y + 0x11, x + 0x55, y + 0x1c};
    meter_draw(cdef + 0x100, frame, rect, charge, cap, cap);
    sprintf_exe(text, glob<const char *>(0x622640) /* "Special (%s) Charge" */, name);
    font_text(font, frame, x + 0x2d, y + 0x14, text, 1, 0);
}

// 0x582ee0 (slot 24): the "Detail Image" at a frame swinging with a sine as the HUD's Sphere of Zin renderer
// (statics 0x713808, 0x7137d4, guard 0x7137fd), frame 0 for anything but an energy item; then the next renderer
static void THISCALL SphereOfZinDetail_Draw(BYTE *self, BYTE *item, void *frame, int x, int y, void *inv)
{
    BYTE *seq = record_object(glob_addr(IMAGE_LIST), glob_addr(IMAGE_RECORDS), glob_addr(IMAGE_COUNT), dw(self, 8));
    if (seq) {
        void *img;
        if (item && is_kind_of(item, energy_item_class())) {
            const DWORD n = vcall<DWORD>(seq, 25);
            const long double c = cst(glob_addr(0x5eee2c));
            const float scale = fstp((fld(fl<float>(self, 0x10)) + fld(fl<float>(self, 0x14))) * c + cst(glob_addr(0x5ec3f4)));
            float base = fstp(fld(fl<float>(self, 0x14)) * c);
            DWORD charge = dw(item, 0x18);
            long double r = fld(raw_float(charge)) / max_charge(item);
            charge = dw(item, 0x18);
            const float amp = fstp(((fld(fl<float>(self, 0x10)) - fld(fl<float>(self, 0x14))) * r + fld(fl<float>(self, 0x14))) * c);
            r = fld(raw_float(charge)) / max_charge(item);
            charge = dw(item, 0x18);
            base = fstp(r + fld(base));
            r = fld(raw_float(charge)) / max_charge(item);
            const float rate = fstp((fld(fl<float>(self, 0x18)) - fld(fl<float>(self, 0x1c))) * r + fld(fl<float>(self, 0x1c)));
            float &phase = *glob<float *>(0x713808);
            if (!c03(fld(phase), cst(glob_addr(0x5eeeac)))) {
                const DWORD k = (DWORD)x87_ftol(fld(phase));
                phase = fstp(fld(phase) - fild64((k / 7) * 2) * cst(glob_addr(0x5eecc8)));
            }
            BYTE &guard = *glob<BYTE *>(0x7137fd);
            DWORD &last = *glob<DWORD *>(0x7137d4);
            if (!(guard & 1)) {
                guard |= 1;
                last = fn<DWORD(__stdcall *)()>(0x4d2460)();
            }
            const DWORD delta = fn<DWORD(__stdcall *)()>(0x4d2460)() - last;
            last += delta;
            phase = fstp(fild64(delta) * fld(rate) * cst(glob_addr(0x60123c)) + fld(phase));
            const long double s = fn<long double(__stdcall *)(DWORD)>(0x508ec0)(float_bits(phase));
            int f = x87_ftol(fild64(n) * ((s * fld(amp) + fld(base)) * (long double)*glob<double *>(0x5ee620)) / fld(scale));
            if (self[0x20])
                f = (int)n - f - 1;
            img = vcall<void *>(seq, 23, f);
        } else {
            img = vcall<void *>(seq, 23, 0);
        }
        if (img)
            blit_at(frame, img, x, y, sequence_blend(seq));
    }
    next_draw(self, item, frame, x, y, inv);
}
// 0x583140 (slot 25): the next renderer's line, else "%d/%d" (charge, capacity) for an energy item
static BYTE *THISCALL SphereOfZinDetail_Describe(BYTE *self, BYTE *item, void *inv)
{
    BYTE *e = item && is_kind_of(item, energy_item_class()) ? item : nullptr;
    BYTE *next = bp(dw(self, 0xc));
    BYTE *r = next ? vcall<BYTE *>(next, 25, item, inv) : nullptr;
    if (!e || r)
        return r;
    BYTE *line = new_line();
    line_name(line, item);
    const DWORD charge = dw(e, 0x18);
    const int c = x87_ftol(max_charge(e)), d = x87_ftol(fld(raw_float(charge)));
    zs_format(line + 0x14, glob<const char *>(0x622980) /* "%d/%d" */, d, c);
    return line;
}

// ---- Overlays and segments ---------------------------------------------------------------------------------

// 0x5adea0 (slot 24): the "Detail Image" sequence at the clock's frame at the options' detail position, then
// the next renderer
static void THISCALL AnimationOverlay_Draw(BYTE *self, BYTE *item, void *frame, int x, int y, void *inv)
{
    BYTE *opts = display_options() + 0x3c;
    BYTE *seq = record_object_loading(glob_addr(IMAGE_LIST), glob_addr(IMAGE_RECORDS), glob_addr(IMAGE_COUNT), glob_addr(IMAGE_FOLDER), dw(self, 8));
    if (seq) {
        const DWORD n = vcall<DWORD>(seq, 25);
        const DWORD f = sync_frame(n, 0, float_bits(fstp(vcall<long double>(seq, 26))));
        if (void *img = vcall<void *>(seq, 23, f))
            blit_at(frame, img, (int)dw(opts, 8), (int)dw(opts, 0xc), sequence_blend(seq));
    }
    next_draw(self, item, frame, x, y, inv);
}
// 0x5ae480 (slot 24): a random frame of the "Detail Image" (the game's random generator), a new one when
// 1 / fps · 0x5eb2cc ticks have passed (+0x10 the time, +0x14 the frame), Transparent Mask; then the next
static void THISCALL RandomOverlay_Draw(BYTE *self, BYTE *item, void *frame, int x, int y, void *inv)
{
    BYTE *opts = display_options() + 0x3c;
    BYTE *seq = record_object_loading(glob_addr(IMAGE_LIST), glob_addr(IMAGE_RECORDS), glob_addr(IMAGE_COUNT), glob_addr(IMAGE_FOLDER), dw(self, 8));
    if (seq) {
        const DWORD now = fn<DWORD(__stdcall *)()>(0x4d2460)();
        const DWORD period = (DWORD)x87_ftol(cst(glob_addr(0x5ec3f4)) / vcall<long double>(seq, 26) * cst(glob_addr(0x5eb2cc)));
        if (now - dw(self, 0x10) > period) {
            dw(self, 0x10) = now;
            dw(self, 0x14) = fn<DWORD(THISCALL *)(void *, DWORD)>(0x55c470)(glob<void *>(0x7124c0), vcall<DWORD>(seq, 25));
        }
        if (void *img = vcall<void *>(seq, 23, dw(self, 0x14)))
            blit_at(frame, img, (int)dw(opts, 8), (int)dw(opts, 0xc), glob<void *>(MASK_BLEND));
    }
    next_draw(self, item, frame, x, y, inv);
}
// 0x56a3a0 (CSegmentedInventoryDetailRenderer slot 24): every segment's "Detail Image" at the clock's frame at
// the detail position; a message for an item that is not segmented
static void THISCALL Segmented_Draw(BYTE *, BYTE *item, void *frame, int x, int y, void *)
{
    if (!item || !kind(item, fn_addr(0x569560))) {
        vcall<DWORD>(reporter(), 17, glob<const char *>(0x6215b0));
        return;
    }
    for (DWORD k = 0; k < dw(item, 0x20); k++) {
        if (!*glob<BYTE **>(IMAGE_RECORDS))
            continue;
        BYTE *seq = list_object(glob_addr(IMAGE_LIST), glob_addr(IMAGE_RECORDS), glob_addr(IMAGE_COUNT), glob_addr(IMAGE_FOLDER), dw(bp(dw(item, 0x1c)), k * 4));
        if (!seq)
            continue;
        const DWORD n = vcall<DWORD>(seq, 25);
        const DWORD f = sync_frame(n, 0, float_bits(fstp(vcall<long double>(seq, 26))));
        if (void *img = vcall<void *>(seq, 23, f))
            blit_at(frame, img, x, y, sequence_blend(seq));
    }
}

// ---- CWeaponInventoryDetailRenderer --------------------------------------------------------------------------

namespace {
// The projectile line of a weapon: "+0x24" of the projectile alone (0x7140a4/0x7140c4) or with "+0x30" (the
// continuous fire weapon's data +0x18 when "+0x30" is below 0x5ec3f4) (0x7140cc/0x7140bc)
struct Projectile {
    float p24, p30;
    bool both;
};
Projectile projectile_values(BYTE *def)
{
    Projectile r;
    r.p24 = fstp(fn<long double(THISCALL *)(void *)>(0x4d6300)(def));
    const long double p30 = fn<long double(THISCALL *)(void *)>(0x4d6410)(def);
    r.p30 = fstp(p30);
    r.both = true;
    if (c0(p30, cst(glob_addr(0x5ec3f4)))) {
        BYTE *tmpl = bp(dw(def, 0x1c));
        if (tmpl && kind(tmpl, fn_addr(0x472880)) && kind(def, fn_addr(0x5b5f10))) {
            BYTE *data = bp(dw(def, 0xcc));
            if (data && kind(data, fn_addr(0x472e80)))
                r.p30 = fl<float>(data, 0x18);
        }
        r.both = !c0(fld(r.p30), cst(glob_addr(0x5ec3f4)));
    }
    return r;
}
}  // namespace

// 0x5ad0e0 (slot 24): the ammunition line (the ammunition's name with 0x714094 when it is another item, then
// the inventory's total of it, or a continuous fire weapon's energy source charge with "%.1f", through the
// static string 0x7140ac and 0x7140b8) in the font +0x28, or without ammunition 0x71408c with "+0x4c" in the
// font +0x2c; below it (by the font's height · 0x5fbc88) the projectile line in the font +0x30
static void THISCALL WeaponDetail_Draw(BYTE *, BYTE *item, void *frame, int x, int y, BYTE *inv)
{
    BYTE *opts = display_options() + 0x3c;
    BYTE *def = item_definition(item);
    BYTE *zax = def && kind(def, fn_addr(0x5b5f10)) ? def : nullptr;
    BYTE text[4];
    zs_ctor(text);
    const DWORD ammo = dw(def, 0x2c);
    BYTE *font;
    if (ammo) {
        zs_assign_text(text, nullptr);
        BYTE *o = record_object(glob_addr(ITEM_LIST), glob_addr(ITEM_RECORDS), glob_addr(ITEM_COUNT), ammo);
        if (o != def) {
            BYTE name[4];
            zs_ctor(name);
            display_name(ammo, name);
            const char *fmt = registered(glob_addr(0x714094));
            zs_format(text, fmt, zs_c_str(name));
            zs_dtor(name);
        }
        static_string(glob_addr(0x714090), glob_addr(0x7140ac), fn_addr(0x5ad640));
        if (zax && fn<bool(THISCALL *)(void *)>(0x5b7640)(zax)) {
            BYTE *src = fn<BYTE *(THISCALL *)(void *, void *)>(0x5b7920)(zax, inv);
            const DWORD v = src ? dw(src, 0x18) : 0;
            if (zs_empty(text)) {
                zs_format(glob<void *>(0x7140ac), glob<const char *>(0x626184) /* "%.1f" */, (double)fld(raw_float(v)));
                const char *a = registered(glob_addr(0x7140b8)), *b = registered(glob_addr(0x7140ac));
                zs_append_format(text, a, b);
            } else {
                zs_append_format(text, glob<const char *>(0x62617c) /* " %.1f" */, (double)fld(raw_float(v)));
            }
        } else {
            const DWORD n = total_count(inv, ammo);
            if (zs_empty(text)) {
                zs_format(glob<void *>(0x7140ac), glob<const char *>(0x60a5a0) /* "%d" */, n);
                const char *a = registered(glob_addr(0x7140b8)), *b = registered(glob_addr(0x7140ac));
                zs_append_format(text, a, b);
            } else {
                zs_append_format(text, glob<const char *>(0x60a794) /* " %d" */, n);
            }
        }
        font = record_object(glob_addr(FONT_LIST), glob_addr(FONT_RECORDS), glob_addr(FONT_COUNT), dw(opts, 0x28));
    } else {
        zs_format(text, registered(glob_addr(0x71408c)), x87_ftol(fld(fl<float>(def, 0x4c))));
        font = record_object(glob_addr(FONT_LIST), glob_addr(FONT_RECORDS), glob_addr(FONT_COUNT), dw(opts, 0x2c));
    }
    font_text(font, frame, x, y, zs_c_str(text), 0, 0);
    y += x87_ftol(fild64(dw(font, 0x80)) * (long double)*glob<double *>(0x5fbc88));
    const Projectile p = projectile_values(def);
    if (p.both)
        zs_format(text, registered(glob_addr(0x7140cc)), (double)fld(p.p24), (double)(fld(p.p30) + fld(p.p24)));
    else
        zs_format(text, registered(glob_addr(0x7140a4)), (double)fld(p.p24));
    font_text(font_loading(dw(opts, 0x30)), frame, x, y, zs_c_str(text), 0, 0);
    zs_dtor(text);
}

// 0x5ad650 (slot 25): the weapon's line: the ammunition's name and the total (0x714088), or the total through
// the static string 0x71409c ("%u", 0x7140b4) for a weapon that is its own ammunition, or the shots the battery
// and a charger give ("+0x4c" the charge per shot; 0x71408c); +0x18 2 above the threshold, 1 for some, 0 for
// none; then the projectile values (0x7140bc / 0x7140c4)
static BYTE *THISCALL WeaponDetail_Describe(BYTE *self, BYTE *item, BYTE *inv)
{
    BYTE *line = new_line();
    dw(line, 8) = 1;
    dw(line, 0x10) = 3;
    display_name(properties_count(item), line + 0xc);
    BYTE *def = item_definition(item);
    const DWORD ammo = dw(def, 0x2c);
    auto shots_colour = [&](DWORD n) {
        dw(line, 0x18) = 0;
        if (n > dw(self, 8))
            dw(line, 0x18) = 2;
        else if (n)
            dw(line, 0x18) = 1;
    };
    if (ammo) {
        BYTE *o = record_object(glob_addr(ITEM_LIST), glob_addr(ITEM_RECORDS), glob_addr(ITEM_COUNT), ammo);
        if (o != def) {
            BYTE name[4];
            zs_ctor(name);
            display_name(ammo, name);
            const DWORD n = total_count(inv, ammo);
            shots_colour(n);
            const char *fmt = registered(glob_addr(0x714088));
            zs_format(line + 0x14, fmt, zs_c_str(name), n);
            zs_dtor(name);
        } else {
            const DWORD n = total_count(inv, ammo);
            shots_colour(n);
            static_string(glob_addr(0x7140c0), glob_addr(0x71409c), fn_addr(0x5ada00));
            zs_format(glob<void *>(0x71409c), glob<const char *>(0x6173f4) /* "%u" */, n);
            const char *a = registered(glob_addr(0x7140b4)), *b = registered(glob_addr(0x71409c));
            zs_format(line + 0x14, a, b);
        }
    } else {
        const DWORD per_shot = dw(def, 0x4c);
        if (c3(fld(raw_float(per_shot)), cst(glob_addr(C_ZERO)))) {
            dw(line, 0x18) = 2;
        } else {
            BYTE *battery = fn<BYTE *(THISCALL *)(void *)>(0x5b0da0)(inv);
            BYTE *charger = fn<BYTE *(THISCALL *)(void *, CClassInfo *)>(0x425860)(inv, class_at(fn_addr(0x5af910)));
            BYTE *cdef = charger ? item_definition(charger) : nullptr;
            long double total;
            if (battery) {
                if (cdef) {
                    const float extra = fstp(fld(fl<float>(cdef, 0x4c)) * fild((int)dw(self, 8)));
                    total = max_charge(battery) + fld(extra);
                } else {
                    total = max_charge(battery) + fld(raw_float(0));
                }
            } else {
                total = cst(glob_addr(C_ZERO));
            }
            DWORD shots = (DWORD)x87_ftol(-(total / fld(raw_float(per_shot))));
            if (battery) {
                const long double m = max_charge(battery);
                if (!c03(-fld(raw_float(per_shot)), m))
                    shots = 0;
            }
            dw(line, 0x18) = 0;
            if (shots > dw(self, 8))
                dw(line, 0x18) = 2;
            else if (shots)
                dw(line, 0x18) = 1;
        }
        zs_format(line + 0x14, registered(glob_addr(0x71408c)), x87_ftol(-fld(raw_float(per_shot))));
    }
    const Projectile p = projectile_values(def);
    if (p.both)
        zs_append_format(line + 0x14, registered(glob_addr(0x7140bc)), (double)fld(p.p24), (double)(fld(p.p30) + fld(p.p24)));
    else
        zs_append_format(line + 0x14, registered(glob_addr(0x7140c4)), (double)fld(p.p24));
    return line;
}

// ---- The item definition's icons ---------------------------------------------------------------------------

namespace {
constexpr DWORD ICON_LIST = 0x6c0aa8, ICON_RECORDS = 0x6c0ad8, ICON_COUNT = 0x6c0adc, ICON_FOLDER = 0x6c0ae0;
constexpr DWORD HUD_LIST = 0x6c06a0, HUD_RECORDS = 0x6c06d0, HUD_COUNT = 0x6c06d4, HUD_FOLDER = 0x6c06d8;
}  // namespace

// 0x4d69d0 (CInventoryItemDefinition: frame, position): its two "Inventory Icons" frames (+0x10 at x + 0x58,
// +0x14 loaded on first use at x + 0x5c) with the Transparent Mask
static void THISCALL ItemDefinition_DrawIcon(BYTE *self, void *frame, const int *pos)
{
    if (dw(self, 0x10)) {
        BYTE *img = record_object(glob_addr(ICON_LIST), glob_addr(ICON_RECORDS), glob_addr(ICON_COUNT), dw(self, 0x10));
        blit_at(frame, img, (int)dw(self, 0x58) + pos[0], pos[1], glob<void *>(MASK_BLEND));
    }
    if (dw(self, 0x14)) {
        BYTE *img = record_object_loading(glob_addr(ICON_LIST), glob_addr(ICON_RECORDS), glob_addr(ICON_COUNT), glob_addr(ICON_FOLDER), dw(self, 0x14));
        blit_at(frame, img, (int)dw(self, 0x5c) + pos[0], pos[1], glob<void *>(MASK_BLEND));
    }
}
// 0x5b8ca0 (CZaxInventoryItemDefinition slot 27: frame, position): the detail image (slot 33, frame 0), an
// animation overlay detail renderer's image at the definition's animation frame (+0xe0), the HUD image (slot
// 31) at +0xe4/+0xe8, then the inventory icons (0x4d69d0) moved by +0xec
static void THISCALL ZaxDefinition_DrawIcon(BYTE *self, void *frame, const int *pos)
{
    if (vcall<DWORD>(self, 33)) {
        BYTE *seq = record_object(glob_addr(IMAGE_LIST), glob_addr(IMAGE_RECORDS), glob_addr(IMAGE_COUNT), vcall<DWORD>(self, 33));
        if (void *img = vcall<void *>(seq, 23, 0))
            blit_at(frame, img, pos[0], pos[1], sequence_blend(seq));
    }
    BYTE *r = vcall<BYTE *>(self, 34);
    if (r && kind(r, fn_addr(0x5adb50))) {
        if (BYTE *seq = record_object(glob_addr(IMAGE_LIST), glob_addr(IMAGE_RECORDS), glob_addr(IMAGE_COUNT), dw(r, 8)))
            if (void *img = vcall<void *>(seq, 23, x87_ftol(fld(fl<float>(self, 0xe0)))))
                blit_at(frame, img, pos[0], pos[1], sequence_blend(seq));
    }
    if (vcall<DWORD>(self, 31)) {
        BYTE *seq = record_object_loading(glob_addr(HUD_LIST), glob_addr(HUD_RECORDS), glob_addr(HUD_COUNT), glob_addr(HUD_FOLDER), vcall<DWORD>(self, 31));
        if (void *img = vcall<void *>(seq, 23, 0))
            blit_at(frame, img, (int)dw(self, 0xe4) + pos[0], (int)dw(self, 0xe8) + pos[1], sequence_blend(seq));
    }
    const int p2[2] = {pos[0] + (int)dw(self, 0xec), pos[1]};
    fn<void(THISCALL *)(void *, void *, const int *)>(0x4d69d0)(self, frame, p2);
}

ZAX_HOOKS(hud_details)
{
    const Hook list[] = {
        {0x47ac40, ptr(&Detail_Slot23), "CInventoryDetailRenderer::Slot23", 5, {0x33, 0xc0, 0xc2, 0x10, 0x00, 0x90, 0x90, 0x90}},
        {0x5ae060, ptr(&Detail_NextDescribe), "CInventoryDetailRenderer::NextDescribe", 5, {0x8b, 0x49, 0x0c, 0x85, 0xc9, 0x74, 0x12, 0x8b}},
        {0x5b12d0, ptr(&Count_Draw), "CCountInventoryDetailRenderer::Draw", 6, {0x81, 0xec, 0x64, 0x01, 0x00, 0x00, 0x56, 0x57}},
        {0x5b1440, ptr(&Count_Describe), "CCountInventoryDetailRenderer::Describe", 5, {0x53, 0x56, 0x57, 0x8b, 0xd9, 0x33, 0xf6, 0xe8}},
        {0x5b16f0, ptr(&Ammunition_Draw), "CAmmunitionInventoryDetailRenderer::Draw", 5, {0xa1, 0x34, 0x43, 0x71, 0x00, 0x81, 0xec, 0x64}},
        {0x5b18c0, ptr(&Ammunition_Describe), "CAmmunitionInventoryDetailRenderer::Describe", 5, {0x51, 0x53, 0x55, 0x56, 0x57, 0x8b, 0x7c, 0x24}},
        {0x5b1c60, ptr(&Mineral_Draw), "CMineralInventoryDetailRenderer::Draw", 5, {0xa1, 0x30, 0x43, 0x71, 0x00, 0x81, 0xec, 0x64}},
        {0x5b1e60, ptr(&Mineral_Describe), "CMineralInventoryDetailRenderer::Describe", 5, {0x53, 0x56, 0x57, 0x6a, 0xff, 0x68, 0xd4, 0xb7}},
        {0x5b2160, ptr(&Charger_Draw), "CChargerInventoryDetailRenderer::Draw", 5, {0xa1, 0x14, 0x3f, 0x71, 0x00, 0x81, 0xec, 0x64}},
        {0x5b22f0, ptr(&Charger_Describe), "CChargerInventoryDetailRenderer::Describe", 8, {0x53, 0x56, 0x57, 0xb9, 0x1c, 0x00, 0x00, 0x00}},
        {0x5b2580, ptr(&Health_Draw), "CHealthInventoryDetailRenderer::Draw", 5, {0xa1, 0x14, 0x3f, 0x71, 0x00, 0x81, 0xec, 0x68}},
        {0x5b27b0, ptr(&Health_Describe), "CHealthInventoryDetailRenderer::Describe", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x8b, 0x6c, 0x24}},
        {0x47af90, ptr(&Deployable_Draw), "CDeployableInventoryDetailRenderer::Draw", 5, {0xa1, 0x9c, 0xda, 0x6b, 0x00, 0x81, 0xec, 0x64}},
        {0x47b150, ptr(&Deployable_Describe), "CDeployableInventoryDetailRenderer::Describe", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x55, 0x56, 0x85}},
        {0x483b90, ptr(&Battery_Draw), "CBatteryInventoryDetailRenderer::Draw", 6, {0x81, 0xec, 0x7c, 0x01, 0x00, 0x00, 0x56, 0x8b}},
        {0x483db0, ptr(&Battery_Describe), "CBatteryInventoryDetailRenderer::Describe", 6, {0x51, 0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x85, 0xdb}},
        {0x484a40, ptr(&ShieldDetail_Draw), "CShieldInventoryDetailRenderer::Draw", 6, {0x81, 0xec, 0x7c, 0x01, 0x00, 0x00, 0x56, 0x8b}},
        {0x484c60, ptr(&ShieldDetail_Describe), "CShieldInventoryDetailRenderer::Describe", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x85, 0xdb, 0x57}},
        {0x4843e0, ptr(&EnergyText_Draw), "CEnergyTextInventoryDetailRenderer::Draw", 6, {0x81, 0xec, 0x0c, 0x01, 0x00, 0x00, 0x56, 0x57}},
        {0x4845d0, ptr(&EnergyText_Describe), "CEnergyTextInventoryDetailRenderer::Describe", 8, {0x83, 0xec, 0x18, 0x53, 0x8b, 0x5c, 0x24, 0x20}},
        {0x484740, ptr(&EnergyText_Upgrade), "CEnergyTextInventoryDetailRenderer::Upgrade", 10, {0x51, 0x53, 0x8b, 0xd9, 0x8b, 0x0d, 0xa0, 0xdc}},
        {0x580df0, ptr(&CharacterDetail_Draw), "CCharacterInventoryDetailRenderer::Draw", 6, {0x81, 0xec, 0x8c, 0x00, 0x00, 0x00, 0x53, 0x55}},
        {0x582ee0, ptr(&SphereOfZinDetail_Draw), "CSphereOfZinEnergyInventoryDetailRenderer::Draw", 5, {0xa1, 0x78, 0x09, 0x6c, 0x00, 0x83, 0xec, 0x14}},
        {0x583140, ptr(&SphereOfZinDetail_Describe), "CSphereOfZinEnergyInventoryDetailRenderer::Describe", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x57, 0x85}},
        {0x5adea0, ptr(&AnimationOverlay_Draw), "CAnimationOverlayInventoryDetailRenderer::Draw", 6, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0xa1, 0x14}},
        {0x5ae480, ptr(&RandomOverlay_Draw), "CRandomOverlayInventoryDetailRenderer::Draw", 5, {0xa1, 0x14, 0x3f, 0x71, 0x00, 0x81, 0xec, 0x00}},
        {0x56a3a0, ptr(&Segmented_Draw), "CSegmentedInventoryDetailRenderer::Draw", 6, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0x53, 0x56}},
        {0x5ad0e0, ptr(&WeaponDetail_Draw), "CWeaponInventoryDetailRenderer::Draw", 6, {0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x81, 0xec}},
        {0x5ad650, ptr(&WeaponDetail_Describe), "CWeaponInventoryDetailRenderer::Describe", 5, {0x83, 0xec, 0x0c, 0x53, 0x55, 0x8b, 0xd9, 0x56}},
        {0x4d69d0, ptr(&ItemDefinition_DrawIcon), "InventoryItem_DrawIcon", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x55, 0x56}},
        {0x5b8ca0, ptr(&ZaxDefinition_DrawIcon), "CZaxInventoryItemDefinition::DrawIcon", 6, {0x81, 0xec, 0x08, 0x01, 0x00, 0x00, 0x55, 0x56}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
