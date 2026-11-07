// Checks of control_config.cpp against the original, in the game thread, in the fake world of
// checks_rest_world.h: the INI functions (Zax.CFG is never read or written), the key table, the joystick,
// the mouse, sounds, the time and the packet functions are stand-ins.
//
//  - The control configuration in the object under test with real blocks: the three bit sets' words and
//    the bindings and their two copies on the heap, the two pointer arrays (the real vtable 0x5ee138) with
//    their data from the pool, a joystick block on the heap; a recording callback or none. Before the state
//    is taken every block's bytes go into the log and the pointers become tags; after each side the blocks
//    are freed. Every function on 0 to 6 bindings: the constructor (the original's destructor frees what it
//    made), the destructor, the bit sets, the bindings' setters, the shares, the key capture (Delete,
//    Escape, forbidden keys, joystick and mouse bits, the wheel), the copies, the INI write and read
//    (bindings found or not), the joystick check.
//  - The INI helpers, the player options (constructor, load, save), the CD key: get, set, the check with
//    the samples of the stand-in (valid and invalid keys, O/I/L/B/S for digits, dashes) and the check of
//    the key in Zax.CFG. ZStrings by their text.
#include "checks_rest_world.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_control_config(Hook *out);

namespace {

using namespace rw;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD words(DWORD bits) { return ((bits & 0x1f) ? 1 : 0) + (bits >> 5); }
DWORD g_block_id;
// A block's bytes as a log entry (its number, size and hash)
void log_hash(const void *p, DWORD n) { log_event(0xb10c0000u + g_block_id++, n, fnv(2166136261u, p, n)); }
void log_value(DWORD v) { log_event(0xca11ba00u, v, 0); }

// The callback (+0x18): what it is told goes into the log
void __fastcall callback(BYTE *self, DWORD binding)
{
    log_value(0xcb000000u + (self == obj(O_SELF) ? 0 : 0xff0000) + binding);
}

const char *const WORD1[] = {"Fire", "Move Up", "Use Item"};
const char *const WORD2[] = {"Primary", "Left", ""};

DWORD *new_words(Gen &g, DWORD bits)
{
    const DWORD n = words(bits);
    DWORD *w = static_cast<DWORD *>(heap_alloc(n * 4 + 4));
    for (DWORD i = 0; i < n; i++)
        w[i] = g.next() % 3 == 0 ? 0 : g.next();
    return w;
}
void make_bitset(Gen &g, BYTE *b, DWORD bits)
{
    D(b, 0) = ptrv(new_words(g, bits));
    D(b, 4) = bits;
    b[8] = (BYTE)(g.next() % 2);
    b[9] = (BYTE)(g.next() % 2);
}
DWORD key_value(Gen &g)
{
    const DWORD keys[] = {0, 0, 0x1e, 0x1e, 0x39, 0x3b, 1, 0x153};
    return keys[g.next() % 8];
}
DWORD bits_value(Gen &g)
{
    const DWORD v = g.next() % 4;
    return v == 0 ? 0 : v == 1 ? 1u << (g.next() % 8) : v == 2 ? 3 : g.next();
}
void make_binding(Gen &g, BYTE *b)
{
    for (DWORD k = 0; k < 0x40; k += 4)
        D(b, k) = g.next();
    D(b, 0) = key_value(g);
    D(b, 4) = key_value(g);
    D(b, 8) = bits_value(g);
    D(b, 0xc) = bits_value(g);
    D(b, 0x10) = 1 + g.next() % 3;
    D(b, 0x24) = ptrv(WORD1[g.next() % 3]);
    D(b, 0x28) = ptrv(WORD2[g.next() % 3]);
}
BYTE *new_bindings(Gen &g, DWORD n, const BYTE *like)
{
    BYTE *b = static_cast<BYTE *>(heap_alloc(n * 0x40 + 4));
    for (DWORD i = 0; i < n; i++)
        if (like && g.next() % 2)
            memcpy(b + i * 0x40, like + i * 0x40, 0x40);
        else
            make_binding(g, b + i * 0x40);
    return b;
}
void make_array(Gen &g, BYTE *a, bool mouse)
{
    D(a, 0) = 0x5ee138;
    const DWORD n = g.next() % 4, cap = n + g.next() % 3;
    D(a, 4) = 0;
    if (cap) {
        DWORD *d = static_cast<DWORD *>(pool_get_alloc(cap * 4));
        const DWORD keys[] = {1, 0x3b, 0x44, 0x1e}, bits[] = {1, 2, 8, 0x10};
        for (DWORD i = 0; i < cap; i++)
            d[i] = mouse ? bits[g.next() % 4] : keys[g.next() % 4];
        D(a, 4) = ptrv(d);
    }
    D(a, 8) = n;
    D(a, 0xc) = cap;
}

// The configuration in object 0 (0x1c8 bytes); the functions given a binding get at least one (they do
// not test the index)
DWORD g_min_bindings;
void make_config(Gen &g)
{
    BYTE *s = obj(O_SELF);
    const DWORD n = g_min_bindings + g.next() % (7 - g_min_bindings), bits = g.next() % 4 == 0 ? 0 : g.next() % 70;
    make_bitset(g, s, bits);
    make_bitset(g, s + 0xc, bits);
    make_bitset(g, s + 0x64, bits);
    D(s, 0x18) = g.next() % 3 ? (DWORD)(uintptr_t)&callback : 0;
    make_array(g, s + 0x1c, false);
    make_array(g, s + 0x2c, true);
    D(s, 0x3c) = n;
    BYTE *b = new_bindings(g, n, nullptr);
    D(s, 0x40) = ptrv(b);
    D(s, 0x44) = ptrv(new_bindings(g, n, b));   // the copies are read without a test
    D(s, 0x48) = ptrv(new_bindings(g, n, b));
    for (DWORD k = 0x4c; k < 0x50; k++)
        s[k] = (BYTE)(g.next() % 2);
    if (g.next() % 3) {
        BYTE *j = static_cast<BYTE *>(heap_alloc(0x100));
        memset(j, 0, 0x100);
        D(j, 0) = g.next() % 4;
        D(j, 0x48) = ZSTRING_EMPTY;
        D(s, 0x70) = ptrv(j);
    } else {
        D(s, 0x70) = 0;
    }
    s[0x75] = (BYTE)(g.next() % 2);
    make_binding(g, s + 0x78);
    const char *const names[] = {"Gamepad Pro", "NO JOYSTICK", "", "Joystick"};
    const char *nm = names[g.next() % 4];
    DWORD k = 0;
    for (; nm[k]; k++)
        s[0xb8 + k] = nm[k];
    s[0xb8 + k] = 0;
    D(s, 0x1b8) = g.next();
    s[0x1bc] = (BYTE)(g.next() % 4 != 0);
    D(s, 0x1c0) = g.next() % 3;
    s[0x1c4] = (BYTE)(g.next() % 2);
}

// Before the state: every block's bytes into the log, the pointers become tags (kept for the free)
struct Kept {
    DWORD words[3], arrays[2], bindings[3], joystick;
};
Kept g_kept;
void tag_config()
{
    BYTE *s = obj(O_SELF);
    g_block_id = 0;
    const DWORD bitsets[3] = {0, 0xc, 0x64};
    for (int i = 0; i < 3; i++) {
        g_kept.words[i] = D(s, bitsets[i]);
        if (g_kept.words[i]) {
            log_hash(at<void *>(g_kept.words[i]), words(D(s, bitsets[i] + 4)) * 4);
            D(s, bitsets[i]) = 0xe1000000u + (DWORD)i;
        }
    }
    const DWORD arrays[2] = {0x1c, 0x2c};
    for (int i = 0; i < 2; i++) {
        g_kept.arrays[i] = D(s, arrays[i] + 4);
        if (g_kept.arrays[i]) {
            log_hash(at<void *>(g_kept.arrays[i]), D(s, arrays[i] + 8) * 4);
            D(s, arrays[i] + 4) = 0xe2000000u + (DWORD)i;
        }
    }
    const DWORD bindings[3] = {0x40, 0x44, 0x48};
    for (int i = 0; i < 3; i++) {
        g_kept.bindings[i] = D(s, bindings[i]);
        if (g_kept.bindings[i]) {
            log_hash(at<void *>(g_kept.bindings[i]), D(s, 0x3c) * 0x40);
            D(s, bindings[i]) = 0xe3000000u + (DWORD)i;
        }
    }
    g_kept.joystick = D(s, 0x70);
    if (g_kept.joystick)
        D(s, 0x70) = 0xe4000000u;
}
void free_config()
{
    BYTE *s = obj(O_SELF);
    for (DWORD w : g_kept.words)
        if (w)
            heap_free(at<void *>(w));
    const DWORD arrays[2] = {0x1c, 0x2c};
    for (int i = 0; i < 2; i++)
        if (g_kept.arrays[i])
            pool_get_free(at<void *>(g_kept.arrays[i]), D(s, arrays[i] + 0xc) * 4);
    for (DWORD b : g_kept.bindings)
        if (b)
            heap_free(at<void *>(b));
    if (g_kept.joystick)
        heap_free(at<void *>(g_kept.joystick));
    memset(&g_kept, 0, sizeof g_kept);
}

// ZStrings of the object under test (+0) and of the spare object (+0): by their text in the log, freed after
DWORD g_kept_text[2];
void tag_texts()
{
    BYTE *z[2] = {obj(O_SELF), obj(O_SPARE)};
    for (int i = 0; i < 2; i++) {
        g_kept_text[i] = D(z[i], 0);
        const char *t = at<const char *(THISCALL *)(const void *)>(0x4e13a0)(z[i]);
        log_value(fnv_text(2166136261u, t));
        D(z[i], 0) = 0xe5000000u + (DWORD)i;
    }
}
void free_texts()
{
    BYTE *z[2] = {obj(O_SELF), obj(O_SPARE)};
    for (int i = 0; i < 2; i++) {
        D(z[i], 0) = g_kept_text[i];
        at<void(THISCALL *)(void *)>(0x4defd0)(z[i]);
    }
}
void set_text(BYTE *z, const char *t) { at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(z, t); }

enum Self { S_CONFIG, S_CTOR, S_RAW, S_BITSET, S_TEXT, S_INI, S_OPTIONS, NSELF };
const char *const CD_KEY_SET[] = {"", "1234-5678-9012-3456-7890", "Bad CD Key"};
int g_self;
const char *const KEYS_TEXT[] = {"", "1234-5678-9012-3456-7890", "12345678901234567890", "o1il-bs00-1234-5678-90123", "ABCD",
                                 "9999999999-9999999999", "0000000000-0000000000", "1234567890123456789", "12345678901234567890X"};
void make_self(Gen &g)
{
    BYTE *s = obj(O_SELF);
    switch (g_self) {
    case S_CONFIG:
        make_config(g);
        break;
    case S_CTOR:
        break;
    case S_BITSET: {
        const DWORD bits = 1 + g.next() % 90;
        make_bitset(g, s, bits);
        make_bitset(g, obj(O_SPARE), bits);
        break;
    }
    case S_TEXT:
        set_text(s, KEYS_TEXT[g.next() % 9]);
        set_text(obj(O_SPARE), "");
        break;
    case S_RAW: {
        const char *t = CD_KEY_SET[g.next() % 3];
        DWORD k = 0;
        for (; t[k]; k++)
            s[k] = t[k];
        s[k] = 0;
        break;
    }
    case S_INI: {
        const char *section = g.next() % 2 ? "Zax" : "Net Game";
        DWORD k = 0;
        for (; section[k]; k++)
            s[0x100 + k] = section[k];
        s[0x100 + k] = 0;
        set_text(obj(O_SPARE), g.next() % 2 ? "" : "default text");
        D(s, 0) = ZSTRING_EMPTY;
        break;
    }
    case S_OPTIONS:
        D(s, 0) = ZSTRING_EMPTY;
        set_text(obj(O_SPARE), "");
        D(s, 4) = g.next() % 300;
        D(s, 8) = g.next() % 300;
        break;
    default:
        break;
    }
}
void tag_bitsets()
{
    g_block_id = 0;
    BYTE *b[2] = {obj(O_SELF), obj(O_SPARE)};
    for (int i = 0; i < 2; i++) {
        g_kept.words[i] = D(b[i], 0);
        log_hash(at<void *>(g_kept.words[i]), words(D(b[i], 4)) * 4);
        D(b[i], 0) = 0xe1000000u + (DWORD)i;
    }
}
void free_bitsets()
{
    for (int i = 0; i < 2; i++)
        heap_free(at<void *>(g_kept.words[i]));
    memset(&g_kept, 0, sizeof g_kept);
}
// The constructor's blocks: tagged like a configuration's, then freed by the original's destructor
void free_made_config()
{
    BYTE *s = obj(O_SELF);
    const DWORD bitsets[3] = {0, 0xc, 0x64};
    for (int i = 0; i < 3; i++)
        D(s, bitsets[i]) = g_kept.words[i];
    D(s, 0x20) = g_kept.arrays[0];
    D(s, 0x30) = g_kept.arrays[1];
    D(s, 0x40) = g_kept.bindings[0];
    D(s, 0x44) = g_kept.bindings[1];
    D(s, 0x48) = g_kept.bindings[2];
    D(s, 0x70) = 0;
    original<void(THISCALL *)(void *)>(0x46f2f0)(s);
    memset(&g_kept, 0, sizeof g_kept);
    invcheck::arena_off();
}
// The constructor's heap blocks come from the arena of the side, filled with a pattern (the bindings'
// bytes Init leaves are compared too)
void a_ctor_arena(Gen &g, DWORD *a)
{
    a[0] = g.next() % 2 ? g.next() & 0xff : g.next();
    a[1] = g.next() % 3;
    a[2] = g.next() % 4 ? g.next() % 3 : 0xffffffffu;
    // at most 50 bindings: 56 to 62 give blocks of a pool chunk's size (3588 to 4004 bytes), which the
    // arena leaves to the game's heap (from_pool_grow), so their bytes Init leaves would differ
    a[3] = g.next() % 8 == 0 ? 0 : 1 + g.next() % 50;
    invcheck::arena_on(g_side, 0x5a);
}

DWORD binding_index(Gen &g)
{
    const DWORD n = D(obj(O_SELF), 0x3c);
    return n ? g.next() % n : 0;
}
void a_none(Gen &, DWORD *) {}
void a_set_keys(Gen &g, DWORD *a)
{
    a[0] = binding_index(g);
    BYTE *b = D(obj(O_SELF), 0x3c) ? at<BYTE *>(D(obj(O_SELF), 0x40)) + (g.next() % D(obj(O_SELF), 0x3c)) * 0x40 : nullptr;
    const bool same = b && g.next() % 2;
    a[1] = same ? D(b, 0) : key_value(g);
    a[2] = same ? D(b, 4) : key_value(g);
    a[3] = same && g.next() % 2 ? D(b, 8) : bits_value(g);
    a[4] = same && g.next() % 2 ? D(b, 0xc) : bits_value(g);
}
void a_index(Gen &g, DWORD *a) { a[0] = binding_index(g); }
void a_index_value(Gen &g, DWORD *a)
{
    a[0] = binding_index(g);
    a[1] = g.next();
}
void a_set_binding(Gen &g, DWORD *a)
{
    a[0] = binding_index(g);
    for (int i = 1; i < 7; i++)
        a[i] = g.next();
    a[1] = ptrv(WORD2[g.next() % 3]);
    a[2] = ptrv(WORD1[g.next() % 3]);
}
void a_capture(Gen &g, DWORD *a)
{
    a[0] = binding_index(g);
    a[1] = ptrv(obj(O_SPARE) + 0x100);
}
void a_key(Gen &g, DWORD *a) { a[0] = key_value(g); }
void a_packet(Gen &, DWORD *a) { a[0] = ptrv(obj(O_PACKET)); }
void a_section(Gen &g, DWORD *a) { a[0] = g.next() % 2 ? 0 : ptrv(g.next() % 2 ? "Controls" : "Player 2 Gamepad Control Configuration"); }
void a_bit(Gen &g, DWORD *a) { a[0] = g.next() % (D(obj(O_SELF), 4) ? D(obj(O_SELF), 4) : 1); }
void a_other(Gen &, DWORD *a) { a[0] = ptrv(obj(O_SPARE)); }
void a_kind(Gen &g, DWORD *a) { a[0] = g.next() % 3 ? ptrv(obj(O_SPARE) + 0x100) : 0; }
void a_name(Gen &g, DWORD *a)
{
    const char *const names[] = {"Zax.CFG", "", "C:\\Games\\Zax\\Zax.CFG"};
    a[0] = ptrv(names[g.next() % 3]);
}
void a_key_out(Gen &g, DWORD *a)
{
    const char *const keys[] = {"CD Key", "Player Name", "Last CD Key Error Message"};
    a[0] = ptrv(keys[g.next() % 3]);
    a[1] = ptrv(obj(O_SPARE));
}
void a_section_key_out(Gen &g, DWORD *a)
{
    a[0] = ptrv(g.next() % 2 ? "Net Game" : "Zax");
    a[1] = ptrv(g.next() % 2 ? "Player Name" : "CD Key");
    a[2] = ptrv(obj(O_SPARE));
}

struct Test {
    int self;
    SlotTest t;
    void (*before)();
    void (*after)();
};
const Test TESTS[] = {
    {S_CTOR, {"ControlConfig::Ctor", 0x46ef80, 4, 0xffffffff, 200, a_ctor_arena}, tag_config, free_made_config},
    {S_CONFIG, {"ControlConfig::Dtor", 0x46f2f0, 0, 0, 100, a_none}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::ForbidKey", 0x46f3e0, 1, 0xffffffff, 150, a_key}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::SetBinding", 0x46f430, 7, 0xffffffff, 60, a_set_binding}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::SetItem", 0x46f480, 2, 0xffffffff, 40, a_index_value}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::GetItem", 0x46f4a0, 1, 0xffffffff, 40, a_index}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::SetKeys", 0x46f4c0, 5, 0, 500, a_set_keys}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::FindShared", 0x46f5c0, 0, 0xffffffff, 200, a_none}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::Capture", 0x46f680, 2, 0xff, 900, a_capture}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::SaveCopy", 0x46f980, 0, 0, 80, a_none}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::SaveCopy2", 0x46fa40, 0, 0, 80, a_none}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::RestoreCopy", 0x46fa10, 0, 0, 40, a_none}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::RestoreCopy2", 0x46fad0, 0, 0, 40, a_none}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::Changed", 0x46fb00, 0, 0xff, 150, a_none}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::WriteIni", 0x46fb60, 1, 0, 150, a_section}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::ReadIni", 0x46fcc0, 1, 0, 250, a_section}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::Activate", 0x46ff70, 0, 0xffffffff, 20, a_none}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::Deactivate", 0x46ff90, 0, 0, 20, a_none}, tag_config, free_config},
    {S_CONFIG, {"ControlConfig::CheckJoystick", 0x46ffa0, 0, 0xff, 200, a_none}, tag_config, free_config},
    {S_RAW, {"KeyBinding::Init", 0x46ef50, 0, 0xffffffff, 10, a_none}, nullptr, nullptr},
    {S_BITSET, {"BitSet::Test", 0x46ed70, 1, 0xffffffff, 100, a_bit}, tag_bitsets, free_bitsets},
    {S_BITSET, {"BitSet::Assign", 0x46eda0, 1, 0xffffffff, 60, a_other}, tag_bitsets, free_bitsets},
    {S_BITSET, {"BitSet::Clear", 0x46ee00, 0, 0xffffffff, 60, a_none}, tag_bitsets, free_bitsets},
    {S_BITSET, {"BitSet::Read", 0x46ee50, 1, 0, 100, a_packet}, tag_bitsets, free_bitsets},
    {S_BITSET, {"BitSet::Write", 0x46ef00, 1, 0xff, 60, a_packet}, tag_bitsets, free_bitsets},
    {S_RAW, {"IniFile::SetFile", 0x46ec00, 1, 0xffffffff, 20, a_name}, nullptr, nullptr},
    {S_INI, {"IniFile::GetDefault", 0x46ec30, 2, 0, 80, a_key_out}, tag_texts, free_texts},
    {S_INI, {"IniFile::GetText", 0x46ec70, 3, 0, 80, a_section_key_out}, tag_texts, free_texts},
    {S_TEXT, {"CdKey_Get", 0x46b4e0, -1, 0, 60, a_none}, tag_texts, free_texts},
    {S_RAW, {"CdKey_Set", 0x46b500, -1, 0, 20, a_none}, nullptr, nullptr},
    {S_RAW, {"CdKey_SetLastError", 0x46b520, -1, 0, 20, a_none}, nullptr, nullptr},
    {S_TEXT, {"CdKey_Check", 0x46b540, -1, 0xff, 300, a_kind}, tag_texts, free_texts},
    {S_TEXT, {"CdKey_IsValid", 0x46b6d0, CALL_CDECL0, 0xff, 150, a_none}, tag_texts, free_texts},
    {S_OPTIONS, {"PlayerOptions::Ctor", 0x46ce20, 0, 0xffffffff, 20, a_none}, tag_texts, free_texts},
    {S_OPTIONS, {"PlayerOptions::Load", 0x46ce50, 0, 0, 100, a_none}, tag_texts, free_texts},
    {S_OPTIONS, {"PlayerOptions::Save", 0x46ced0, 0, 0, 40, a_none}, tag_texts, free_texts},
};

void test_all(Tally &t)
{
    for (const Test &x : TESTS) {
        g_self = x.self;
        g_before_state = x.before;
        g_after_side = x.after;
        g_min_bindings = x.t.setup == a_set_keys || x.t.setup == a_index || x.t.setup == a_index_value || x.t.setup == a_set_binding ||
                                 x.t.setup == a_capture
                             ? 1
                             : 0;
        Policy p{};
        const unsigned n = (unsigned)scaled(x.t.count);
        for (unsigned seed = 1; seed <= n; seed++)
            run_slot(t, x.t, seed * 2654435761u + x.t.address, make_self, p);
    }
    g_before_state = nullptr;
    g_after_side = nullptr;
}

}  // namespace

ZAX_CHECKS(control_config)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally t{"control_config: control configuration, bit sets, INI helpers, player options, CD key in the fake world"};
    make_static_texts();
    if (!invcheck::arena_setup()) {
        t.check(false, "control_config", "[no arena]");
        return;
    }
    if (world_on()) {
        test_all(t);
        world_off();
    } else {
        t.check(false, "control_config", "[the fake world could not be set up]");
    }
    t.report();
    log("  control_config: %u ms", GetTickCount() - start);
}

}  // namespace zax
