// Checks of misc_rest.cpp against the original, in the game thread (machinery: checks_inventory_common.h:
// side 0 with the module's hooks out, the original's first bytes put back, so nested calls and the two
// functions without a trampoline (Text_ToUpper, Text_ReplaceChar) run the original too).
//  - Classes: CEntityOverlayWalking's Create, CreateArray and slot 1, both class getters of it and of
//    CPositionedAction, RegisterFields into a fake class info and the registered descriptors (real data).
//  - CPositionedAction's Set and the turned offset on fake parts (their slot 2 gives the class asked for,
//    slot 46 the angle), Entity_SequenceObject 0x48b590, the rotation sequence's 0x55fdd0/0x55fe90 and
//    LayerPart_Position 0x4fb0a0 recording stand-ins.
//  - CEntityOverlayWalking's block write and read and its precaching on a copy of a made object with a
//    copy of its vtable (slot 26 recording), the packet functions, CEntityCharacter's block functions and
//    CObject_Precache recording stand-ins, the model list 0x6cfdd8 a fake during a case (its vtable,
//    records and count).
//  - The plasma generators on byte maps from random corners and a copy of the game's random generator.
//  - Stream_New, the Properties.txt path of a fake list, and the C string helpers on many texts (texts,
//    returned pointers by their offset, eax where defined).
#include "checks_inventory_common.h"
#include "x87.h"

namespace zax {

int hooks_misc_rest(Hook *out);

namespace {

using namespace invcheck;

const ClassDef CLASSES[] = {
    {"CEntityOverlayWalking", 0x13c, 0x6bdec8, 0x49cdf0, 0x49ce30, 0x49cfa0, {0x49cd90, 0x49cf30}, true},
    {"CPositionedAction", 0x14, 0x6bdeb0, 0, 0, 0x49c330, {0x49c280, 0x49c2d0}, false},
};

DWORD fbits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}
float some_float()
{
    const DWORD r = random() >> 8;
    switch (r % 12) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: { DWORD nan = 0x7fc00000u | (r >> 8 & 0xff); float f; memcpy(&f, &nan, 4); return f; }
    case 3: return -(float)(int)(r >> 12 & 0x3ff) / 8.0f;
    case 4: return 1e30f;
    default: return (float)(int)(r >> 12 & 0x3ff) / 16.0f;
    }
}

void copy_text(char *out, const char *s, int max)
{
    int i = 0;
    for (; s[i] && i < max - 1; i++)
        out[i] = s[i];
    out[i] = 0;
}

struct Stand {
    DWORD address;
    void *fn;
};
template <int N> struct Patches {
    CodePatch p[N];
    void in(const Stand (&s)[N])
    {
        for (int i = 0; i < N; i++)
            patch_jump(p[i], s[i].address, s[i].fn);
    }
    void out()
    {
        for (int i = N; i--;)
            unpatch(p[i]);
    }
};

// ---- CPositionedAction ---------------------------------------------------------------------------------------

struct Part {
    DWORD vtable;
    BYTE body[0xd0];
};
DWORD g_part_vt[64];
CClassInfo *g_part_class, *g_classes[3];
float g_part_angle;
DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
CClassInfo *THISCALL f_class(void *) { return g_part_class; }
long double THISCALL f_angle(void *p) { ev(0x46, *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + 4)); return fld(g_part_angle); }
BYTE g_seq[8];
BYTE *THISCALL s_sequence_object(void *) { ev(0x801); return g_seq; }
DWORD g_rotation;
DWORD THISCALL s_angle_to_rotation(void *seq, DWORD angle) { ev(0x802, seq == g_seq, angle); return g_rotation; }
float g_snapped;
long double THISCALL s_rotation_to_angle(void *seq, DWORD r) { ev(0x803, seq == g_seq, r); return fld(g_snapped); }
float g_pos[2];
float *THISCALL s_position(void *, float *out)
{
    ev(0x804);
    out[0] = g_pos[0];
    out[1] = g_pos[1];
    return out;
}
const Stand POS_STANDS[] = {
    {0x48b590, (void *)&s_sequence_object}, {0x55fdd0, (void *)&s_angle_to_rotation}, {0x55fe90, (void *)&s_rotation_to_angle},
    {0x4fb0a0, (void *)&s_position},
};
Patches<4> g_pos_patches;

void test_positioned(Tally &t, unsigned seed)
{
    BYTE action[2][0x14];
    float out[2][2];
    DWORD r[2];
    Part part;
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        for (DWORD k = 0; k < 0x14; k += 4)
            *reinterpret_cast<DWORD *>(action[side] + k) = random();
        *reinterpret_cast<DWORD *>(action[side] + 8) = fbits(some_float());
        *reinterpret_cast<DWORD *>(action[side] + 0xc) = fbits(some_float());
        memset(&part, 0, sizeof part);
        part.vtable = ptrv(g_part_vt);
        *reinterpret_cast<DWORD *>(part.body + 4) = random();
        *reinterpret_cast<DWORD *>(part.body + 0xc4 - 4) = fbits(some_float());
        g_part_class = g_classes[random() % 3];
        g_part_angle = some_float();
        g_rotation = random() % 16;
        g_snapped = some_float();
        g_pos[0] = some_float();
        g_pos[1] = some_float();
        const DWORD with = random() % 3 ? random() : 0;
        log_reset(side);
        side_begin(side);
        g_pos_patches.in(POS_STANDS);
        if (seed % 4 == 0)
            r[side] = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x49c600)(action[side], random(), random(), random()), out[side][0] = 0,
            out[side][1] = 0;
        else
            at<void(THISCALL *)(void *, void *, float *, DWORD, DWORD)>(0x49c620)(action[side], &part, out[side], random(), with), r[side] = 0;
        g_pos_patches.out();
        side_end(side);
    }
    DWORD o[4];
    memcpy(o, out, 16);
    char d[200];
    wsprintfA(d, "[seed %u: eax %x / %x, log %d / %d, out %x %x / %x %x, in %x %x, angle %x %x %x, class %d]", seed, r[0], r[1], g_nlog[0],
              g_nlog[1], o[0], o[1], o[2], o[3], *reinterpret_cast<DWORD *>(action[0] + 8), *reinterpret_cast<DWORD *>(action[0] + 0xc),
              fbits(g_part_angle), fbits(g_snapped), *reinterpret_cast<DWORD *>(part.body + 0xc0),
              g_part_class == g_classes[0] ? 0 : g_part_class == g_classes[1] ? 1 : 2);
    t.check(r[0] == r[1] && !memcmp(action[0], action[1], 0x14) && !memcmp(out[0], out[1], 8) && same_log(), "CPositionedAction", d);
}

// ---- CEntityOverlayWalking -------------------------------------------------------------------------------------

DWORD g_walk_vt[160];
DWORD THISCALL f_count(void *, void *packet) { ev(0x26, packet == (void *)0x5e7a0001); return 3; }
DWORD THISCALL s_block_type(void *packet, void *type, DWORD n) { ev(0x901, ptrv(packet), ptrv(type), n); return result(1); }
DWORD THISCALL s_char_write(void *self, void *packet, DWORD a) { ev(0x902, *static_cast<DWORD *>(self) != 0, ptrv(packet), a); return result(2); }
DWORD THISCALL s_word_token(void *packet, DWORD word, DWORD list) { ev(0x903, ptrv(packet), word, list); return result(3); }
DWORD THISCALL s_char_read(void *self, void *packet) { ev(0x904, *static_cast<DWORD *>(self) != 0, ptrv(packet)); return result(4); }
WORD g_token;
DWORD THISCALL s_read_token(void *packet, WORD *out, DWORD list)
{
    ev(0x905, ptrv(packet), list);
    *out = g_token;
    return result(5);
}
DWORD THISCALL s_precache(void *self) { ev(0x906, *static_cast<DWORD *>(self) != 0); return result(6); }
const Stand WALK_STANDS[] = {
    {0x51ff50, (void *)&s_block_type}, {0x49b2a0, (void *)&s_char_write}, {0x520da0, (void *)&s_word_token},
    {0x49b300, (void *)&s_char_read},  {0x520670, (void *)&s_read_token}, {0x4167b0, (void *)&s_precache},
};
Patches<6> g_walk_patches;

// The model list during a precaching case: a fake vtable (slot 1 the entry's name, slot 37 load), records
BYTE g_records[4][0x80];
DWORD g_list_vt[64];
const char *const ENTRY_NAMES[] = {"Characters/Zax", "Kor", "Things/Barrel", ""};
const char *THISCALL f_entry_name(void *, DWORD i) { ev(0x101, i); return ENTRY_NAMES[i & 3]; }
DWORD THISCALL f_load(void *, const char *path, const char *name, DWORD *slot)
{
    ev(0x137, fnv_text(2166136261u, path), fnv_text(2166136261u, name), ptrv(slot));
    return 0x5e7a3700u + (ptrv(slot) >> 2) % 16;
}

void test_walking(Tally &t, int what, unsigned seed)
{
    BYTE o[2][0x13c];
    BYTE rec[2][4][0x80];
    DWORD r[2];
    BYTE *made = static_cast<BYTE *>(at<void *(__cdecl *)()>(0x49cdf0)());
    if (!made)
        return;
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        memcpy(o[side], made, 0x13c);
        *reinterpret_cast<DWORD *>(o[side]) = ptrv(g_walk_vt);
        *reinterpret_cast<DWORD *>(o[side] + 0x130) = random() % 3 ? random() % 6 : random();
        g_token = (WORD)random();
        for (int i = 0; i < 4; i++) {
            for (DWORD k = 0; k < 0x80; k += 4)
                *reinterpret_cast<DWORD *>(g_records[i] + k) = random();
            *reinterpret_cast<DWORD *>(g_records[i] + 0x78) = random() % 2 ? 0 : random();
            g_records[i][0x7f] = (BYTE)(random() % 6 == 0);
        }
        const DWORD with = random() % 3 ? random() : 0;
        log_reset(side);
        side_begin(side);
        g_walk_patches.in(WALK_STANDS);
        BYTE *const list = at<BYTE *>(0x6cfdd8);
        const DWORD saved_vt = *reinterpret_cast<DWORD *>(list), saved_records = *at<DWORD *>(0x6cfe08), saved_count = *at<DWORD *>(0x6cfe0c);
        *reinterpret_cast<DWORD *>(list) = ptrv(g_list_vt);
        *at<DWORD *>(0x6cfe08) = seed % 7 ? ptrv(g_records) : 0;
        *at<DWORD *>(0x6cfe0c) = 4;
        switch (what) {
        case 0: r[side] = at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x49d040)(o[side], 0x5e7a0001, with); break;
        case 1: r[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x49d0a0)(o[side], 0x5e7a0001); break;
        default: at<void(THISCALL *)(void *)>(0x49d0e0)(o[side]), r[side] = 0; break;
        }
        *at<DWORD *>(0x6cfe0c) = saved_count;
        *at<DWORD *>(0x6cfe08) = saved_records;
        *reinterpret_cast<DWORD *>(list) = saved_vt;
        g_walk_patches.out();
        side_end(side);
        memcpy(rec[side], g_records, sizeof g_records);
    }
    hooks_out();
    vcall<void *>(made, 1, 1);
    hooks_in();
    const char *const names[] = {"CEntityOverlayWalking::WriteBlock", "CEntityOverlayWalking::ReadBlock", "CEntityOverlayWalking::Precache"};
    char d[120];
    wsprintfA(d, "[seed %u: eax %x / %x, log %d / %d]", seed, r[0], r[1], g_nlog[0], g_nlog[1]);
    t.check(r[0] == r[1] && !memcmp(o[0], o[1], 0x13c) && !memcmp(rec[0], rec[1], sizeof rec[0]) && same_log(), names[what], d);
}

// ---- The plasma generators ---------------------------------------------------------------------------------------

constexpr DWORD MAP = 65 * 66;
BYTE g_map[2][2 * MAP + 0x100];   // Plasma_Column reads up to two columns' length past its start
BYTE g_gen[2][GAME_RANDOM_BYTES];
DWORD g_image_vt[64];
struct Image {
    DWORD vtable;
    BYTE *pixels;
    DWORD width, height;
};
BYTE *THISCALL f_pixels(Image *i) { return i->pixels; }
DWORD THISCALL f_width(Image *i) { return i->width; }
DWORD THISCALL f_height(Image *i) { return i->height; }

void test_plasma(Tally &t, int what, unsigned seed)
{
    DWORD r[2] = {0, 0};
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        memcpy(g_gen[side], at<void *>(0x7124c0), GAME_RANDOM_BYTES);
        const BYTE fill = random() % 3 ? 0 : (BYTE)random();
        memset(g_map[side], fill, sizeof g_map[side]);
        for (DWORD k = 0; k < 16; k++)
            g_map[side][random() % MAP] = (BYTE)random();
        g_map[side][0] = (BYTE)random();
        g_map[side][64] = (BYTE)random();
        g_map[side][64 * 65] = (BYTE)random();
        g_map[side][64 * 65 + 64] = (BYTE)random();
        const DWORD sizes[] = {65, 33, 17, 9, 5, 3, 2};   // 2^k + 1 (the exe: 65)
        Image img{ptrv(g_image_vt), g_map[side], sizes[random() % 7], sizes[random() % 7]};
        const DWORD a = random(), b = random();
        side_begin(side);
        switch (what) {
        case 0: r[side] = at<DWORD(__fastcall *)(DWORD, DWORD, void *)>(0x49fda0)(a % 300, b % 70, g_gen[side]); break;
        case 1: at<void(__fastcall *)(void *, DWORD, void *)>(0x49f9d0)(g_map[side], sizes[a % 7] - 1 + a % 2, g_gen[side]); break;
        case 2: {
            const DWORD column = a % 65, rows = sizes[b % 7] - 1 + b % 2 * (b % 3);
            at<void(__fastcall *)(void *, DWORD, DWORD, void *)>(0x49fe10)(&img, column, rows, g_gen[side]);
            r[side] = column | rows << 8;   // the case (for the report)
            break;
        }
        default: at<void(__fastcall *)(void *, void *)>(0x49fad0)(&img, g_gen[side]); break;
        }
        side_end(side);
    }
    const char *const names[] = {"Plasma_Displace", "Plasma_Line", "Plasma_Column", "Plasma_Map"};
    char d[120];
    int at_ = -1;
    for (DWORD k = 0; k < sizeof g_map[0] && at_ < 0; k++)
        if (g_map[0][k] != g_map[1][k])
            at_ = (int)k;
    wsprintfA(d, "[seed %u: eax %x / %x, map +%d: %x / %x, %d]", seed, r[0], r[1], at_, at_ >= 0 ? g_map[0][at_] : 0, at_ >= 0 ? g_map[1][at_] : 0,
              memcmp(g_gen[0], g_gen[1], GAME_RANDOM_BYTES) != 0);
    t.check(r[0] == r[1] && !memcmp(g_map[0], g_map[1], sizeof g_map[0]) && !memcmp(g_gen[0], g_gen[1], GAME_RANDOM_BYTES), names[what], d);
}

// ---- Stream_New and the Properties.txt path ------------------------------------------------------------------------

DWORD g_props_vt[16];
const char *THISCALL f_list_name(void *) { ev(0x201); return "Sequences"; }
const char *g_folder;
const char *THISCALL f_list_folder(void *, const char *name) { ev(0x205, fnv_text(2166136261u, name)); return g_folder; }

void test_small(Tally &t, unsigned seed)
{
    BYTE s[2][0xc];
    char text[2][160];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        log_reset(side);
        side_begin(side);
        const DWORD file = random();
        BYTE *p = at<BYTE *(__fastcall *)(DWORD)>(0x4a2c20)(file);
        memcpy(s[side], p, 0xc);
        pool_free(p, 0xc);
        const char *const folders[] = {"Data/Sequences", "Seq/", "", "A\\B"};
        const char *const entries[] = {"Zax Walk", "x", "", "Idle/Front"};
        g_folder = folders[random() % 4];
        DWORD list = ptrv(g_props_vt);
        ZString out;
        at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&out);
        r[side] = at<DWORD(THISCALL *)(void *, const char *, ZString *)>(0x4a7d40)(&list, entries[random() % 4], &out);
        copy_text(text[side], out.c_str(), 160);
        at<void(THISCALL *)(ZString *)>(0x4defd0)(&out);
        side_end(side);
    }
    char d[220];
    wsprintfA(d, "[seed %u: %.60s / %.60s]", seed, text[0], text[1]);
    (void)r;   // SetDashPathSub's eax is not defined
    t.check(!memcmp(s[0], s[1], 9) && !lstrcmpA(text[0], text[1]) && same_log(), "Stream_New, List_PropertiesPath", d);   // +9 to +0xb never written
}

// ---- The C string helpers ---------------------------------------------------------------------------------------

const char *const TEXTS[] = {"",           " ",        "  \t x",      "abc",       "Abc Def",   "  lead",     "trail  \r\n", "MiXeD cAsE",
                             "\t\t",       "a  b   c", "x\xe9y",      "\xff\x80",  "Fidget",    "fidgetcrouch", "Hello World", " a ",
                             "0123abcdEF", "zz",       "ABCDEFGHIJ", "a\nb",       "  multiple   spaces  here ", "\x7f\x01"};
constexpr int NTEXTS = sizeof TEXTS / sizeof TEXTS[0];
const char *some_text() { return TEXTS[random() % NTEXTS]; }

char g_buf[2][96];
BYTE g_hex[2][96];

// One case: the function on copies of texts; texts, returned pointers (by offset) and eax compared
void test_text(Tally &t, int f, unsigned seed)
{
    DWORD r[2];
    char extra[2][96];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        memset(g_buf[side], 0x55, sizeof g_buf[side]);
        memset(g_hex[side], 0x66, sizeof g_hex[side]);
        memset(extra[side], 0, sizeof extra[side]);
        const char *a = some_text(), *b = some_text();
        lstrcpyA(g_buf[side], a);
        const DWORD n = random() % 24, c = random() % 3 ? (DWORD)(int)(signed char)a[random() % 4 < 3 && a[0] ? 0 : 1] : random();
        side_begin(side);
        char *buf = g_buf[side];
        DWORD v = 0;
        switch (f) {
        case 0: v = at<DWORD(__fastcall *)(char *, const char *)>(0x587a00)(buf, random() % 3 ? b : a); break;
        case 1: v = at<DWORD(__fastcall *)(DWORD)>(0x587a50)(random() % 3 ? (DWORD)(BYTE)a[0] : random()); break;
        case 2: v = at<DWORD(__fastcall *)(const char *)>(0x587a70)(buf) & 0xff; break;
        case 3: v = at<DWORD(__fastcall *)(const char *)>(0x587aa0)(buf) - ptrv(buf); break;
        case 4: v = at<DWORD(__fastcall *)(const char *)>(0x587b00)(buf) - ptrv(buf); break;
        case 5: at<void(__fastcall *)(char *)>(0x587b30)(buf); break;
        case 6: at<void(__fastcall *)(char *)>(0x587b80)(buf); break;
        case 7: at<void(__fastcall *)(char *, const char *)>(0x587bc0)(buf, b); break;
        case 8: at<void(__fastcall *)(char *)>(0x587c00)(buf); break;
        case 9: {
            char *p = at<char *(__fastcall *)(const char *, DWORD, DWORD)>(0x587c20)(random() % 8 ? buf : nullptr, 0, 0);
            v = p ? 1 : 0;
            if (p) {
                copy_text(extra[side], p, 96);
                heap_free(p);
            }
            break;
        }
        case 10: {
            const char *p = at<const char *(__fastcall *)(const char *, const char *)>(0x587c70)(buf, random() % 2 ? b : a + random() % 3 % (lstrlenA(a) + 1));
            v = p ? ptrv(p) - ptrv(buf) + 1 : 0;
            break;
        }
        case 11: {
            const char *p = at<const char *(__fastcall *)(const char *, DWORD, DWORD)>(0x587cb0)(buf, n, c);
            v = p ? ptrv(p) - ptrv(buf) + 1 : 0;
            break;
        }
        case 12: at<void(__fastcall *)(char *, DWORD, DWORD)>(0x587ce0)(buf, c, random()); break;
        case 13: v = at<DWORD(__fastcall *)(const char *, const char *)>(0x587d00)(buf, random() % 3 ? b : a); break;
        case 14: v = at<DWORD(__fastcall *)(const char *, const char *, DWORD, DWORD)>(0x587d30)(buf, b, n, random() % 24); break;
        case 15: v = at<DWORD(__fastcall *)(void *, const void *, DWORD)>(0x587fa0)(g_hex[side], buf, n % 20); v = n % 20 ? v : 0; break;
        default: v = at<DWORD(__fastcall *)(void *, const char *, DWORD)>(0x587ff0)(g_hex[side], buf, n); break;
        }
        side_end(side);
        r[side] = v;
    }
    const char *const names[] = {"Text_CutPrefix", "Text_IsWhite",    "Text_IsBlank",       "Text_SkipWhite",   "Text_SkipWord",
                                 "Text_TrimLeft",  "Text_TrimRight",  "Text_KeepChars",     "Text_ToUpper",     "Text_Copy",
                                 "Text_FindNoCase", "Text_FindCharN", "Text_ReplaceChar",   "Text_CompareWordsAll", "Text_CompareWords",
                                 "Text_Hex",       "Hex_Decode"};
    char d[160];
    wsprintfA(d, "[seed %u: eax %x / %x, \"%.30s\" / \"%.30s\"]", seed, r[0], r[1], g_buf[0], g_buf[1]);
    t.check(r[0] == r[1] && !memcmp(g_buf[0], g_buf[1], sizeof g_buf[0]) && !memcmp(g_hex[0], g_hex[1], sizeof g_hex[0]) &&
                !memcmp(extra[0], extra[1], sizeof extra[0]),
            names[f], d);
}

void init()
{
    for (DWORD &v : g_part_vt)
        v = ptrv((void *)&f_bad);
    g_part_vt[2] = ptrv((void *)&f_class);
    g_part_vt[46] = ptrv((void *)&f_angle);
    g_classes[0] = at<CClassInfo *(__cdecl *)()>(0x48dcc0)();   // CEntityAnimated
    g_classes[1] = at<CClassInfo *(__cdecl *)()>(0x487620)();   // CEntityBase
    g_classes[2] = at<CClassInfo *(__cdecl *)()>(0x4163b0)();   // CObject
    memcpy(g_walk_vt, at<void *>(0x5f3aa4), sizeof g_walk_vt);
    g_walk_vt[26] = ptrv((void *)&f_count);
    for (DWORD &v : g_list_vt)
        v = ptrv((void *)&f_bad);
    g_list_vt[1] = ptrv((void *)&f_entry_name);
    g_list_vt[37] = ptrv((void *)&f_load);
    for (DWORD &v : g_image_vt)
        v = ptrv((void *)&f_bad);
    g_image_vt[31] = ptrv((void *)&f_pixels);
    g_image_vt[39] = ptrv((void *)&f_height);
    g_image_vt[45] = ptrv((void *)&f_width);
    for (DWORD &v : g_props_vt)
        v = ptrv((void *)&f_bad);
    g_props_vt[1] = ptrv((void *)&f_list_name);
    g_props_vt[5] = ptrv((void *)&f_list_folder);
}

}  // namespace

ZAX_CHECKS(misc_rest)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_misc_rest;
    if (!arena_setup()) {
        log("Check misc rest: no arena");
        return;
    }
    init();
    Tally objects{"misc rest: Create, CreateArray, destructors"}, getters{"misc rest: class getters"},
        fields{"misc rest: RegisterFields"}, registered{"misc rest: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    objects.report();
    getters.report();
    fields.report();
    registered.report();

    Tally positioned{"misc rest: CPositionedAction's position"}, walking{"misc rest: CEntityOverlayWalking's blocks and precaching"},
        plasma{"misc rest: plasma generators"}, small{"misc rest: Stream_New, List_PropertiesPath"}, text{"misc rest: C string helpers"};
    for (unsigned k = 0; k < (unsigned)scaled(800); k++)
        test_positioned(positioned, k);
    positioned.report();
    for (int w = 0; w < 3; w++)
        for (unsigned k = 0; k < (unsigned)scaled(300); k++)
            test_walking(walking, w, 1000u * (unsigned)w + k);
    walking.report();
    for (int w = 0; w < 4; w++)
        for (unsigned k = 0; k < (unsigned)scaled(w == 3 ? 150 : 300); k++)
            test_plasma(plasma, w, 3000u * (unsigned)w + k);
    plasma.report();
    for (unsigned k = 0; k < (unsigned)scaled(200); k++)
        test_small(small, k);
    small.report();
    for (int f = 0; f < 17; f++)
        for (unsigned k = 0; k < (unsigned)scaled(400); k++)
            test_text(text, f, 7000u * (unsigned)f + k);
    text.report();
}

}  // namespace zax
