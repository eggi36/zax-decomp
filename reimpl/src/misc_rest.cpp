// The rest of 0x49c000-0x4a7fff and 0x587a00-0x588057 outside the behaviours: CPositionedAction's
// registration and its position helpers, CEntityOverlayWalking (all but its drawing), the plasma height
// map generators the tile map's texture uses, two small stream and path helpers and the C string helpers
// of 0x587a00-0x588057.
//
//  - CPositionedAction (0x14, class 0x6bdeb0, version 2, abstract, under CAction; Create 0x46b760 returns
//    0): +8 "Position/Forward Position" ("Arm Length"), +0xc "Position/Lateral Position" ("Sholder
//    Width"), +0x10 "Position/Height" ("Gun Height"), all in pixels [confirmed: RegisterFields, the
//    callers 0x4c4de4, 0x4c099b, 0x5a8056 (CDischargeWeaponThisFrameBaseAction)].
//  - CEntityOverlayWalking (0x13c, class 0x6bdec8, vtable 0x5f3aa4, under CEntityWalking): +0x130
//    "Overlay Model" (model list 0x6cfdd8), +0x134 the overlay's sequence (-1 none), +0x138 the entity's
//    sequence it belongs to (-1). Slot 23 (the drawing with the overlay model, Entity_Draw) is Rendering.
//  - The plasma generators (0x49f9d0, 0x49fad0, 0x49fda0, 0x49fe10; only caller 0x540d90, the tile map's
//    texture): midpoint displacement on bytes of a 65-byte-wide map with the inline step of the game's
//    random generator layout (+4 count, +0x10/+0x14 indices, table +0x18, successors 0x620004).
#include "ai_helpers.h"
#include "crt_portable.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_OVERLAY_WALKING = 0x5f3aa4;
constexpr DWORD WHITE_SPACE = 0x622dd8;   // const char *: the characters the text helpers skip

// strchr(set, c) != 0 with the exe's char: the terminator counts as found (c 0)
bool in_set(const char *set, char c)
{
    for (;; set++) {
        if (*set == c)
            return true;
        if (!*set)
            return false;
    }
}
bool white(char c) { return in_set(*glob<const char **>(WHITE_SPACE), c); }
DWORD length(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}

// The inline step of the random generator (as 0x55c470 without the range): the next dword
DWORD random_step(BYTE *gen)
{
    dw(gen, 4)++;
    const DWORD i = dw(gen, 0x10), j = dw(gen, 0x14);
    dw(gen, 0x10) = *at<DWORD *>(glob_addr(0x620004) + i * 4);
    dw(gen, 0x14) = *at<DWORD *>(glob_addr(0x620004) + j * 4);
    const DWORD v = dw(gen, 0x18 + i * 4) ^ dw(gen, 0x18 + j * 4);
    dw(gen, 0x18 + i * 4) = v;
    return v;
}
// base - amp + a random number below 2 amp (the low bits), clamped: above 255 gives 255, below 0 gives 1
DWORD displaced(DWORD base, DWORD amp, BYTE *gen)
{
    const DWORD v = random_step(gen);
    DWORD r = ((amp + amp - 1) & v) - amp + base;
    if (r > 0xff)
        r = (int)r >= 1 ? 0xff : 1;
    return r;
}

}  // namespace

// ---- CPositionedAction ---------------------------------------------------------------------------------------

// 0x49c280 (static registration), 0x49c2d0 (slot 2); abstract, Create 0x46b760, CreateArray 0x52dc80
static CClassInfo *__cdecl PositionedAction_ClassInfo()
{
    return class_getter(0x6bdeb0, 0x6131a0 /* "CPositionedAction" */, 2.0f, action_class, 0x14, 1, 0x46b760, 0x52dc80, 0x49c330);
}
// 0x49c330: the three positions in pixels with their older names. eax: FieldDesc_SetAltName's.
static DWORD __cdecl PositionedAction_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x613208) /* "Position/Forward Position" */, glob_addr(0x60b360) /* "Pixels" */, 8, 4, 8, 1, 0, 0, 0, 0, 0);
    add_field(PositionedAction_ClassInfo(), f);
    set_alt_name(last_field(PositionedAction_ClassInfo()), glob_addr(0x6131fc) /* "Arm Length" */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6131e0) /* "Position/Lateral Position" */, glob_addr(0x60b360), 0xc, 4, 0xc, 1, 0, 0, 0, 0, 0);
    add_field(PositionedAction_ClassInfo(), f);
    set_alt_name(last_field(PositionedAction_ClassInfo()), glob_addr(0x6131d0) /* "Sholder Width" */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6131c0) /* "Position/Height" */, glob_addr(0x60b360), 0x10, 4, 0x10, 1, 0, 0, 0, 0, 0);
    add_field(PositionedAction_ClassInfo(), f);
    return set_alt_name(last_field(PositionedAction_ClassInfo()), glob_addr(0x6131b4) /* "Gun Height" */);
}
// 0x49c600 (forward, lateral, height). eax: the height.
static DWORD THISCALL PositionedAction_Set(BYTE *self, DWORD forward, DWORD lateral, DWORD height)
{
    dw(self, 8) = forward;
    dw(self, 0xc) = lateral;
    dw(self, 0x10) = height;
    return height;
}
// 0x49c620 (part, out point, -, with the position): the forward and lateral position turned by the part's
// angle (an animated entity's aiming angle +0xc4, else slot 46; an entity's angle snapped to its rotation
// sequence (0x55fdd0, 0x55fe90)), the lateral part scaled by the oval ratio, plus the part's position
// when asked
static void THISCALL PositionedAction_Offset(BYTE *self, BYTE *part, float *out, DWORD, DWORD with_position)
{
    out[0] = fl<float>(self, 8);
    out[1] = fl<float>(self, 0xc);
    DWORD angle;   // the bits (an animated entity's are copied by integer moves)
    if (part && is_kind_of(part, entity_animated_class()))
        angle = dw(part, 0xc4);
    else
        angle = float_bits(fstp(vcall<long double>(part, 46)));
    long double a;
    if (is_kind_of(part, entity_base_class())) {
        BYTE *seq = fn<BYTE *(THISCALL *)(void *)>(0x48b590)(part);
        const DWORD rot = fn<DWORD(THISCALL *)(void *, DWORD)>(0x55fdd0)(seq, angle);
        a = fn<long double(THISCALL *)(void *, DWORD)>(0x55fe90)(seq, rot);
    } else {
        a = fld(raw_float(angle));
    }
    float c1, c2, y;
    table_sin_cos(&c1, &c2, fstp(-a));
    // x = c2 x0 + c1 y0, y = (-c1 x0 + c2 y0) times the oval ratio, instruction for instruction as the exe
    // (with NaNs the operand order decides the result's payload)
    __asm__("flds %[c2]\n\tfmuls %[x0]\n\tflds %[c1]\n\tfmuls %[y0]\n\tfaddp\n\t"
            "flds %[c1]\n\tfchs\n\tfmuls %[x0]\n\tflds %[c2]\n\tfmuls %[y0]\n\tfaddp\n\t"
            "fstps %[y]\n\tfstps %[x0]\n\tflds %[y]\n\tfmuls %[ratio]\n\tfstps %[y0]"
            : [y] "=m"(y), [x0] "+m"(out[0]), [y0] "+m"(out[1])
            : [c1] "m"(c1), [c2] "m"(c2), [ratio] "m"(*glob<const float *>(C_OVAL_RATIO))
            : "st", "st(1)", "st(2)");
    if ((BYTE)with_position) {
        float pos[2];
        part_position(part, pos);
        // pos + out with the position as the destination operand (as the exe; it decides between two NaNs)
        __asm__("flds %[px]\n\tfadds %[x]\n\tfstps %[x]\n\tflds %[py]\n\tfadds %[y]\n\tfstps %[y]"
                : [x] "+m"(out[0]), [y] "+m"(out[1])
                : [px] "m"(pos[0]), [py] "m"(pos[1])
                : "st");
    }
}

// ---- CEntityOverlayWalking ---------------------------------------------------------------------------------------

// 0x49cd90 (static registration), 0x49cf30 (slot 2)
static CClassInfo *__cdecl OverlayWalking_ClassInfo()
{
    return class_getter(0x6bdec8, 0x613224 /* "CEntityOverlayWalking" */, 1.0f, entity_walking_class, 0x13c, 0, 0x49cdf0, 0x49ce30, 0x49cfa0);
}
static void OverlayWalking_construct(BYTE *o)
{
    fn<void *(THISCALL *)(void *)>(0x49bcb0)(o);   // CEntityWalking
    vtable_and_defaults(o, vt(VT_OVERLAY_WALKING));
    dw(o, 0x134) = 0xffffffffu;
    dw(o, 0x138) = 0xffffffffu;
}
static void OverlayWalking_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_OVERLAY_WALKING));
    fn<void(THISCALL *)(void *)>(0x49bd20)(o);   // ~CEntityWalking
}
// 0x49cdf0
static void *__cdecl OverlayWalking_Create() { return create_one<0x13c, OverlayWalking_construct>(); }
// 0x49ce30 (count in ecx)
static void *__fastcall OverlayWalking_CreateArray(int count, int, int) { return create_array<0x13c, OverlayWalking_construct>(count); }
// 0x49cea0 (slot 1)
static void *THISCALL OverlayWalking_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x13c, OverlayWalking_destroy>(self, flags); }
// 0x49cfa0. eax: ClassInfo_AddField's.
static DWORD __cdecl OverlayWalking_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61323c) /* "Overlay Model" */, 0x130, 4, 0x130, glob_addr(0x6cfdd8), 1, 0, 0, 0);
    return add_field(OverlayWalking_ClassInfo(), f);
}
// 0x49d040 (slot 25: packet, with the type): with the type the block count of slot 26 plus one and the
// block type 0x6bdec0 (2), then CEntityCharacter's block and the overlay model as a word token. The word
// is pushed in the low half of the register that held the object (its upper half goes with it). eax: the
// block count.
static DWORD THISCALL OverlayWalking_WriteBlock(BYTE *self, BYTE *packet, DWORD with_type)
{
    DWORD n = 1;
    if ((BYTE)with_type) {
        n = vcall<DWORD>(self, 26, packet) + 1;
        fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x51ff50)(packet, glob<void *>(0x6bdec0), 2);
    }
    fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x49b2a0)(self, packet, 0);
    const DWORD word = (ptr_value(self) & 0xffff0000u) | (dw(self, 0x130) & 0xffff);
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x520da0)(packet, word, glob_addr(0x6cfdd8));
    return n;
}
// 0x49d0a0 (slot 27: packet): CEntityCharacter's block, then the overlay model's word token
static DWORD THISCALL OverlayWalking_ReadBlock(BYTE *self, BYTE *packet)
{
    fn<DWORD(THISCALL *)(void *, void *)>(0x49b300)(self, packet);
    DWORD token = ptr_value(packet);
    const DWORD r = fn<DWORD(THISCALL *)(void *, DWORD *, DWORD)>(0x520670)(packet, &token, glob_addr(0x6cfdd8));
    dw(self, 0x130) = token & 0xffff;
    return r;
}
// 0x49d0e0 (slot 7): the object's own precaching (CObject_Precache), then the overlay model loaded into
// the model list's record (as DefinitionList_Object 0x48d8f0 does, without returning it)
static void THISCALL OverlayWalking_Precache(BYTE *self)
{
    fn<void(THISCALL *)(void *)>(0x4167b0)(self);
    BYTE *records = *glob<BYTE **>(0x6cfe08);
    DWORD i = dw(self, 0x130);
    if (!records)
        return;
    if (i >= *glob<DWORD *>(0x6cfe0c))
        i = 0;
    const DWORD off = i << 7;
    if (records[off + 0x7f] || dw(records + off, 0x78) || records[off + 0x7f])
        return;
    records[off + 0x7f] = 1;
    char path[0x100];
    fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, glob<const char *>(0x6cfe10),
                                                                          vcall<const char *>(glob<void *>(0x6cfdd8), 1, i));
    char *name = const_cast<char *>(path_name(path));
    DWORD o;
    if (name == path) {
        o = vcall<DWORD>(glob<void *>(0x6cfdd8), 37, glob<const char *>(TEXT_EMPTY), name, *glob<BYTE **>(0x6cfe08) + off + 0x78);
    } else {
        name[-1] = 0;
        o = vcall<DWORD>(glob<void *>(0x6cfdd8), 37, path, name, *glob<BYTE **>(0x6cfe08) + off + 0x78);
    }
    dw(*glob<BYTE **>(0x6cfe08) + off, 0x78) = o;
    (*glob<BYTE **>(0x6cfe08))[off + 0x7f] = 0;
}

// ---- The plasma generators -----------------------------------------------------------------------------------------

// 0x49fda0 (fastcall base, amplitude; generator): base displaced by up to the amplitude, clamped
static DWORD __fastcall Plasma_Displace(DWORD base, DWORD amp, BYTE *gen) { return displaced(base, amp, gen); }
// 0x49f9d0 (fastcall bytes, n; generator): a line of n bytes (n + 1 read) by midpoint displacement from
// the ends, the amplitude n / 8 halved per level
static void __fastcall Plasma_Line(BYTE *buf, DWORD n, BYTE *gen)
{
    DWORD amp = n >> 3;
    BYTE *end = buf + n;
    if (n <= 1)
        return;
    DWORD step = n, half;
    do {
        half = step >> 1;
        for (BYTE *p = buf; p < end; p += step) {
            DWORD v = ((DWORD)p[step] + p[0]) >> 1;
            if (amp)
                v = displaced(v, amp, gen);
            p[half] = (BYTE)v;
        }
        amp >>= 1;
        step = half;
    } while (half > 1);
}
// 0x49fe10 (fastcall image, column; n, generator): the same down a column of the image's bytes (slot 31)
// with rows of 65 bytes, from n rows apart
static void __fastcall Plasma_Column(void *image, DWORD column, DWORD n, BYTE *gen)
{
    BYTE *start = vcall<BYTE *>(image, 31) + column;
    DWORD step = n * 65, amp = n >> 3;
    BYTE *end = start + step;
    if (step <= 0x41)
        return;
    DWORD half;
    do {
        half = step >> 1;
        for (BYTE *p = start; p < end; p += step) {
            DWORD v = ((DWORD)p[step] + p[0]) >> 1;
            if (amp)
                v = displaced(v, amp, gen);
            p[half] = (BYTE)v;
        }
        amp >>= 1;
        step = half;
    } while (half > 0x41);
}
// 0x49fad0 (fastcall image, generator): the whole image (slots 45 width, 39 height, 31 the bytes, rows of
// 65 bytes) by diamond-square: each square's centre the mean of its corners, its edge midpoints (where
// still 0) the displaced mean of their two corners while the amplitude lasts, then plain means
static void __fastcall Plasma_Map(void *image, BYTE *gen)
{
    const DWORD w1 = vcall<DWORD>(image, 45) - 1;
    const DWORD h1 = vcall<DWORD>(image, 39) - 1;
    DWORD sx = w1, sy = h1 * 65, amp = h1 >> 3;
    BYTE *pixels = vcall<BYTE *>(image, 31);
    BYTE *end = pixels + sy;
    if (amp) {
        do {
            if (sx <= 1)
                return;
            for (BYTE *row = pixels; row < end; row += sy) {
                BYTE *row_end = row + w1;
                if (row >= row_end)
                    continue;
                const DWORD hx = sx >> 1, hy = sy >> 1, centre = hx + hy, right = hy + sx;
                for (BYTE *p = row; p < row_end;) {
                    const DWORD a = p[0], b = p[sx], d = p[sx + sy], c = p[sy];
                    p[centre] = (BYTE)((d + b + c + a + 2) >> 2);
                    if (!p[right])
                        p[right] = (BYTE)displaced((d + b) >> 1, amp, gen);
                    if (!p[hy])
                        p[hy] = (BYTE)displaced((c + a) >> 1, amp, gen);
                    p += hx;
                    if (!p[0])
                        p[0] = (BYTE)displaced((b + a) >> 1, amp, gen);
                    if (!p[sy])
                        p[sy] = (BYTE)displaced((c + d) >> 1, amp, gen);
                    p += hx;
                }
            }
            amp >>= 1;
            sy >>= 1;
            sx >>= 1;
        } while (amp);
    }
    if (sx <= 1)
        return;
    do {
        for (BYTE *row = pixels; row < end; row += sy) {
            BYTE *row_end = row + w1;
            if (row >= row_end)
                continue;
            const DWORD hx = sx >> 1, hy = sy >> 1, centre = hx + hy, right = hy + sx;
            for (BYTE *p = row; p < row_end;) {
                const DWORD a = p[0], c = p[sy], b = p[sx], d = p[sx + sy];
                p[centre] = (BYTE)((d + b + c + a + 2) >> 2);
                if (!p[right])
                    p[right] = (BYTE)((d + b) >> 1);
                if (!p[hy])
                    p[hy] = (BYTE)((c + a) >> 1);
                p += hx;
                if (!p[0])
                    p[0] = (BYTE)((b + a) >> 1);
                if (!p[sy])
                    p[sy] = (BYTE)((d + c) >> 1);
                p += hx;
            }
        }
        sx >>= 1;
        sy >>= 1;
    } while (sx > 1);
}

// ---- Small helpers -------------------------------------------------------------------------------------------------

// 0x4a2c20 (fastcall file): a stream object over it (pool, 0xc bytes: vtable 0x5f40dc, +4 the file, +8 0)
static BYTE *__fastcall Stream_New(DWORD file)
{
    BYTE *s = static_cast<BYTE *>(pool_alloc(0xc));
    if (!s)
        return nullptr;
    dw(s, 0) = vt(0x5f40dc);
    dw(s, 4) = 0;
    s[8] = 0;
    dw(s, 4) = file;
    s[8] = 0;
    return s;
}
// 0x4a7d40 (a list's slot: entry, ZString out): "<folder (slot 5 of slot 1)>-<entry>/Properties.txt"
// (ZString::SetDashPathSub)
static DWORD THISCALL List_PropertiesPath(void *self, const char *entry, ZString *out)
{
    const DWORD folder = vcall<DWORD>(self, 5, vcall<DWORD>(self, 1));
    return fn<DWORD(THISCALL *)(void *, DWORD, const char *, const char *, const char *)>(0x4dfc70)(out, folder, entry,
                                                                                                     glob<const char *>(0x613a3c), nullptr);
}

// ---- C string helpers (fastcall, in place) -------------------------------------------------------------------------

// 0x587a00 (text, prefix): the prefix (any case) cut off the front. eax: the remaining length + 1, or
// _strnicmp's result when the text does not start with it.
static DWORD __fastcall Text_CutPrefix(char *text, const char *prefix)
{
    const DWORD n = length(prefix);
    const int r = crt::strnicmp(text, prefix, n);
    if (r)
        return (DWORD)r;
    const char *rest = text + n;
    const DWORD m = length(rest) + 1;
    for (DWORD k = 0; k < m; k++)
        text[k] = rest[k];
    return m;
}
// 0x587a50 (c in cl): 1 when it is white space (the terminator counts)
static DWORD __fastcall Text_IsWhite(char c) { return white(c) ? 1 : 0; }
// 0x587a70 (text): al 1 when the text is only white space
static DWORD __fastcall Text_IsBlank(const char *s)
{
    for (; *s; s++)
        if (!white(*s))
            return 0;
    return 1;
}
// 0x587aa0 (the same as Text_SkipSpace 0x587ad0): the text behind the leading white space
static const char *__fastcall Text_SkipWhite(const char *s)
{
    if (!*s)
        return s;
    while (white(*s))
        if (!*++s)
            return s;
    return s;
}
// 0x587b00 (text): the first white space character (or the end)
static const char *__fastcall Text_SkipWord(const char *s)
{
    if (!*s)
        return s;
    while (!white(*s))
        if (!*++s)
            return s;
    return s;
}
// 0x587b30: the leading white space removed in place
static void __fastcall Text_TrimLeft(char *text)
{
    if (!*text)
        return;
    char *p = text;
    while (white(*p))
        if (!*++p)
            break;
    if (p == text)
        return;
    const DWORD m = length(p) + 1;
    for (DWORD k = 0; k < m; k++)
        text[k] = p[k];
}
// 0x587b80: the trailing white space removed in place
static void __fastcall Text_TrimRight(char *text)
{
    for (DWORD n = length(text); n && white(text[n - 1]); n--)
        text[n - 1] = 0;
}
// 0x587bc0 (text, set): only the characters found in the set kept
static void __fastcall Text_KeepChars(char *text, const char *set)
{
    char *out = text;
    if (*text)
        for (const char *p = text; *p; p++)
            if (in_set(set, *p))
                *out++ = *p;
    *out = 0;
}
// 0x587c00: a-z to upper case
static void __fastcall Text_ToUpper(char *text)
{
    for (; *text; text++)
        if (*text >= 'a' && *text <= 'z')
            *text -= 0x20;
}
// 0x587c20 (text; one stack argument unused): a copy from operator new, 0 for 0
static char *__fastcall Text_Copy(const char *text, DWORD, DWORD)
{
    if (!text)
        return nullptr;
    const DWORD m = length(text) + 1;
    char *p = static_cast<char *>(heap_alloc(m));
    for (DWORD k = 0; k < m; k++)
        p[k] = text[k];
    return p;
}
// 0x587c70 (text, needle): the first position where the needle starts (any case), 0 when none (or the text
// is empty)
static const char *__fastcall Text_FindNoCase(const char *text, const char *needle)
{
    const DWORD n = length(needle);
    for (; *text; text++)
        if (!crt::strnicmp(text, needle, n))
            return text;
    return nullptr;
}
// 0x587cb0 (text, n; c): the first c within n characters (before the end), else 0
static const char *__fastcall Text_FindCharN(const char *text, DWORD n, char c)
{
    for (DWORD i = 0; i < n && text[i]; i++)
        if (text[i] == c)
            return text + i;
    return nullptr;
}
// 0x587ce0 (text, from in dl; to)
static void __fastcall Text_ReplaceChar(char *text, char from, char to)
{
    for (; *text; text++)
        if (*text == from)
            *text = to;
}
// 0x587d30 (a, b; a's length, b's length): compares ignoring case and leading and trailing white space,
// with every run of white space as one blank. eax: <0, 0 or >0 (tolower differences; -1/1 when one is
// longer).
static int __fastcall Text_CompareWords(const char *a, const char *b, DWORD na, DWORD nb)
{
    int r = 0;
    DWORD i = 0, j = 0;
    while (i < na && a[i] && white(a[i]))
        i++;
    while (j < nb && b[j] && white(b[j]))
        j++;
    while (i < na && a[i] && j < nb && b[j]) {
        if (white(a[i])) {
            if (white(b[j])) {
                r = 0;
            } else {
                r = 0x20 - crt::to_lower(b[j]);
                if (r)
                    return r;
            }
            while (a[i] && white(a[i]) && ++i < na) {
            }
            while (b[j] && white(b[j]) && ++j < nb) {
            }
        } else if (white(b[j])) {
            r = crt::to_lower(a[i]) - 0x20;
            if (r)
                return r;
        } else {
            r = crt::to_lower(a[i]) - crt::to_lower(b[j]);
            if (r)
                return r;
            i++;
            j++;
        }
    }
    if (r)
        return r;
    if (i != na) {
        if (a[i] && j != nb && b[j])
            return 0;
        while (i < na && a[i] && white(a[i]))
            i++;
    }
    while (j < nb && b[j] && white(b[j]))
        j++;
    if (i != na && a[i])
        return -1;
    if (j != nb && b[j])
        return 1;
    return 0;
}
// 0x587d00 (a, b): Text_CompareWords over their whole lengths
static int __fastcall Text_CompareWordsAll(const char *a, const char *b)
{
    return fn<int(__fastcall *)(const char *, const char *, DWORD, DWORD)>(0x587d30)(a, b, length(a), length(b));
}
// 0x587fa0 (out, data; n): two hex digits per byte (digits from *0x622ddc) and the terminator. eax: the
// last digit (n > 0).
static DWORD __fastcall Text_Hex(char *out, const BYTE *data, DWORD n)
{
    DWORD r = 0;
    for (; n; n--, data++) {
        *out++ = at<const char *>(*glob<DWORD *>(0x622ddc))[*data >> 4];
        r = (BYTE)at<const char *>(*glob<DWORD *>(0x622ddc))[*data & 0xf];
        *out++ = (char)r;
    }
    *out = 0;
    return r;
}
// 0x587ff0 (out, hex; n): at most n bytes, at most half the text's length; each digit pair decoded with
// the sign-extended characters ('a'.. and above: -0x57, 'A'..: -0x37, else -0x30). eax: the last value
// computed (n when nothing is decoded).
static DWORD __fastcall Hex_Decode(BYTE *out, const char *hex, DWORD n)
{
    DWORD count = length(hex) >> 1;
    if (count >= n)
        count = n;
    if (!count)
        return n;
    DWORD hi = 0;
    for (; count; count--, hex += 2) {
        hi = (DWORD)(int)hex[0];
        hi += hi >= 0x61 ? 0xffffffa9u : hi >= 0x41 ? 0xffffffc9u : 0xffffffd0u;
        DWORD lo = (DWORD)(int)hex[1];
        lo += lo >= 0x61 ? 0xffffffa9u : lo >= 0x41 ? 0xffffffc9u : 0xffffffd0u;
        hi = (hi & 0xffffff00u) | (BYTE)(((BYTE)hi << 4) | (BYTE)lo);
        *out++ = (BYTE)hi;
    }
    return hi;
}

// ---- Hooks ---------------------------------------------------------------------------------------------------

ZAX_HOOKS(misc_rest)
{
    const Hook list[] = {
#include "misc_rest_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
