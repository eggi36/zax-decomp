// Checks of the quest game type and the quest statistics (quest_stats.cpp) against the original, in the
// game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls through vtables and exe addresses run the original
// too), side 1 through the reimplementation. Each side builds its own objects.
//  - Objects: Create of the 17 classes with one on a pool block filled with a pattern (all bytes the
//    constructor writes; a dword that differs counts as equal when both point to blocks with the same
//    first 16 bytes), CreateArray with 0-3 elements, the deleting destructors on filled objects (texts,
//    statistics and columns in the game type, data in the quest data) with flags 0-3, the four bases'
//    deleting destructors on their level; the column constructor 0x552ad0, the column array's add
//    0x552950 (growing) and deleting destructor 0x552b00.
//  - Classes: the 40 class getters (slot 2 and the copies) with the class variable set and 0
//    (ClassInfo_Register 0x415780 replaced by a stand-in recording its arguments); the 14 RegisterFields
//    into a fake class info with the variable set and 0 (the descriptors dword by dword, strings by
//    text, element descriptors the same way; eax), 0x712308 saved around CQuestGameType's.
//  - Behaviour in a fake world: the application and the world (0x713f14, 0x6c2080) are one fake (slot
//    97 the game data with the quest data at +0x30, 13/14 the clocks, 98 the mission manager), the
//    reporter 0x6c02cc a fake (slot 16 the warning, 23 the window), the game type list 0x6c02e8 gets a
//    fake vtable (slots 1, 3, 37) and fake entries for the "QuestMode" lookup, the cache 0x71232c and
//    the item type globals of slot 40 are set per case. Stand-ins replace 0x59ff80, 0x5a01a0, 0x5a3180,
//    0x4cebb0, 0x4cdf30, 0x50cab0, 0x4f5d80, 0x4f1050, 0x509890, 0x5bac80, 0x426860, 0x58fd80,
//    0x58fdd0, 0x5ba360-0x5ba3f0 and the statistics' drawing 0x5533e0 and record their arguments. The
//    objects are real (game types with statistics of the five classes and columns, quest data with the
//    data objects, par scores in a mission's list); Object_IsKindOf, the pointer arrays, the strings,
//    ZString_Format and the descriptors run for real. Compared: eax (al for the bool answers, st(0) as
//    its 10 bytes for the point values), the event log, the whole object state afterwards (texts by
//    content, objects by class), the cache.
//  - Real data: the registered class infos and descriptors against RegisterFields; the running game's
//    game type (0x59ff80 of the application) and quest data when it is the quest game: copies of its
//    statistics' slots on copies of the real data and the current mission's par scores, the game type
//    functions on a copy; the level's CIncrementCounterStatAction objects (copies) executed in the fake
//    world. The running game is not changed: everything runs on copies, in the fakes, with the globals
//    put back.
#include "check_util.h"
#include "field_desc.h"
#include "field_kinds.h"
#include "layer_parts.h"
#include "quest_stats.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);
WINIMPORT DWORD WINAPI GetTickCount(void);

// Event tags are four-character constants ('SEND')
#pragma clang diagnostic ignored "-Wmultichar"

namespace zax {

int hooks_quest_stats(Hook *out);

namespace {

using LD = long double;

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD dwc(const void *p, DWORD off) { return *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(p) + off); }
float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
BYTE *pt(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }

// ---- Memory helpers (as in checks_geometry.cpp) --------------------------------------------------------

bool readable(const void *p, DWORD n)
{
    struct {
        void *base, *allocation_base;
        DWORD allocation_protect;
        size_t size;
        DWORD state, protect, type;
    } mi;
    if (ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000)
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0xee))
            return false;
    return true;
}

CClassInfo *class_of(const void *p)
{
    if (ptrv(p) < 0x10000 || (ptrv(p) & 3) || !readable(p, 0x14))
        return nullptr;
    const DWORD vt = *static_cast<const DWORD *>(p);
    if (vt < 0x5e0000 || vt >= 0x610000 || (vt & 3))
        return nullptr;
    if (!readable(at<void *>(vt), 12))
        return nullptr;
    const DWORD getter = *at<DWORD *>(vt + 8);
    if (getter < 0x401000 || getter >= 0x5e0000 || !readable(at<void *>(getter), 5) || *static_cast<const BYTE *>(original_address(getter)) != 0xa1)
        return nullptr;
    // through the trampoline when the getter is hooked (layer.cpp hooks CLayer's)
    const DWORD var = *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(original_address(getter)) + 1);
    if (var < 0x600000 || var >= 0x720000 || !readable(at<void *>(var), 4))
        return nullptr;
    return *at<CClassInfo **>(var);
}

bool heap_pointer(DWORD p) { return p >= 0x10000 && !(p & 3) && (p < 0x400000 || p >= IMAGE_END) && p < 0x80000000u; }

// ---- Hooks of the module out and in --------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 256;
SavedSite g_sites[MAX_SITES];
int g_nsites;
Hook g_hook_list[MAX_SITES];
void hooks_out()
{
    g_nsites = 0;
    const int n = hooks_quest_stats(nullptr);
    if (!hook_sites_fit("quest_stats", n, MAX_SITES))
        return;
    hooks_quest_stats(g_hook_list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(g_hook_list[i].address);
        if (site[0] != 0xe9 || !memcmp(site, g_hook_list[i].expected, 5))
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, g_hook_list[i].expected);
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

// ---- Exe functions the checks use (through their exe addresses: per side the original or the hook) ----

using Create0 = void *(__cdecl *)();
using CreateN = void *(__fastcall *)(int, int, int);
using Getter = CClassInfo *(__cdecl *)();
void *destroy(void *o, DWORD flags) { return vcall<void *>(o, 1, flags); }
void *clone(void *o) { return at<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffff); }
void zs_ctor(void *z) { at<void *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_dtor(void *z) { at<void(THISCALL *)(void *)>(0x4defd0)(z); }
void zs_set(void *z, const char *s) { at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, s); }
void array_add(void *array, void *p) { vcall<DWORD>(array, 2, &p); }
bool is_kind_of(const void *o, DWORD getter) { return (BYTE)at<DWORD(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, at<Getter>(getter)()); }

enum Kind { K_COLUMN, K_GAME_TYPE, K_PAR_BASE, K_DATA_BASE, K_STAT_BASE, K_SPECIFIC, K_COUNTER_PAR, K_COUNTER_DATA, K_COUNTER_STAT,
            K_INCREMENT, K_FLOAT_PAR, K_FLOAT_DATA, K_FLOAT_STAT, K_TIME_PAR, K_TIME_DATA, K_TIME_STAT_BASE, K_TIME_STAT,
            K_TOTAL_TIME_STAT, K_PERCENTAGE_PAR, K_PERCENTAGE_DATA, K_PERCENTAGE_STAT, NKINDS };
static_assert(sizeof QUEST_STAT_CLASSES / sizeof QUEST_STAT_CLASSES[0] == NKINDS);
const QuestClassDef &def(int k) { return QUEST_STAT_CLASSES[k]; }
void *create(int k) { return at<Create0>(def(k).create)(); }
// The module's class of an object (-1: none)
int kind_of(const void *p)
{
    const CClassInfo *c = class_of(p);
    if (!c)
        return -1;
    for (int k = 0; k < NKINDS; k++)
        if (c == *at<CClassInfo **>(def(k).var))
            return k;
    return -1;
}

// The five statistics (with the total time) and their data and par score classes
constexpr int NSTAT = 5;
const int STAT_KINDS[NSTAT] = {K_COUNTER_STAT, K_FLOAT_STAT, K_TIME_STAT, K_TOTAL_TIME_STAT, K_PERCENTAGE_STAT};
const int DATA_KINDS[NSTAT] = {K_COUNTER_DATA, K_FLOAT_DATA, K_TIME_DATA, K_TIME_DATA, K_PERCENTAGE_DATA};
const int PAR_KINDS[NSTAT] = {K_COUNTER_PAR, K_FLOAT_PAR, K_TIME_PAR, K_TIME_PAR, K_PERCENTAGE_PAR};
int stat_index(int kind)
{
    for (int i = 0; i < NSTAT; i++)
        if (STAT_KINDS[i] == kind)
            return i;
    return -1;
}

// ---- The event log and the hashes ------------------------------------------------------------------------

int g_side;
unsigned g_loghash[2];
int g_nlog[2];
constexpr int LOG = 256;
DWORD g_log[2][LOG][4];
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    if (g_nlog[g_side] < LOG)
        memcpy(g_log[g_side][g_nlog[g_side]], e, sizeof e);
    g_nlog[g_side]++;
}
void log_first_difference()
{
    for (int i = 0; i < g_nlog[0] && i < g_nlog[1] && i < LOG; i++)
        if (memcmp(g_log[0][i], g_log[1][i], 16)) {
            log("    event %d: %x %x %x %x / %x %x %x %x", i, g_log[0][i][0], g_log[0][i][1], g_log[0][i][2], g_log[0][i][3],
                g_log[1][i][0], g_log[1][i][1], g_log[1][i][2], g_log[1][i][3]);
            return;
        }
    log("    events: %d / %d", g_nlog[0], g_nlog[1]);
}
DWORD text_hash(const char *s) { return s ? fnv_text(2166136261u, s) : 0xdead0001; }
DWORD zs_hash(const void *z)
{
    const ZString *s = static_cast<const ZString *>(z);
    if (!s || !s->buffer)
        return 0xdead0002;
    return text_hash(s->buffer->text);
}

// ---- The fakes ---------------------------------------------------------------------------------------------

constexpr int NENTRY = 4;
struct Fakes {
    DWORD vt_app[128], vt_reporter[32], vt_list[64], vt_entity[80];
    BYTE app[0x600];        // also the world
    BYTE game_data[0x40];   // +0x30 the quest data
    BYTE reporter[0x40];
    BYTE entity[0x20], character[0x10], player[0x10], owner[0x10], manager[0x10], other[0x10];
    BYTE entries[NENTRY][0x80];
    // scripted answers
    void *game_type;       // 0x59ff80
    int app_mode;          // 0x5a01a0
    DWORD list_index;      // slot 3 of the list
    void *load_result;     // slot 37 of the list
    void *entity_owner;    // the entity's slot 68
    BYTE entity_projectile;
    DWORD counts[4];       // 0x426860, 0x58fd80, 0x58fdd0
    DWORD row_height;      // 0x5533e0
    float time, total_time;
};
Fakes g_f;
const char *const ENTRY_NAMES[NENTRY] = {"QuestMode.zgt", "MultiPlayer/Death Match.zgt", "Game Types/QuestMode.zgt", "x"};

DWORD id(const void *p)
{
    if (!p)
        return 0;
    const BYTE *b = static_cast<const BYTE *>(p), *a = reinterpret_cast<const BYTE *>(&g_f);
    if (b >= a && b < a + sizeof g_f)
        return 0x10000000 + (DWORD)(b - a);
    const int k = kind_of(p);
    if (k >= 0)
        return 0x20000000 + (DWORD)k;
    return 0x30000000;   // another pointer (a new block)
}

// Fake slots
void *THISCALL f_game_data(void *self)
{
    ev('GDAT', id(self));
    return g_f.game_data;
}
float THISCALL f_time(void *self)
{
    g_f.time += 1.75f;
    ev('TIME', id(self), bits(g_f.time));
    return g_f.time;
}
float THISCALL f_total_time(void *self)
{
    g_f.total_time += 3.5f;
    ev('TTIM', id(self), bits(g_f.total_time));
    return g_f.total_time;
}
void *THISCALL f_manager(void *self)
{
    ev('W98', id(self));
    return g_f.manager;
}
DWORD THISCALL f_warn(void *, const char *text)
{
    ev('WARN', text_hash(text));
    return 0x1600;
}
DWORD THISCALL f_show(void *, void *window, DWORD flag)
{
    ev('SHOW', id(window), flag);
    if (window && id(window) == 0x30000000)
        pool_free(window, 0x1b4);
    return 0x2300;
}
const char *THISCALL f_list_name(void *self, DWORD i)
{
    ev('LNAM', id(self), i);
    return ENTRY_NAMES[i % NENTRY];
}
DWORD THISCALL f_list_find(void *self, const char *name, int hint)
{
    ev('LFND', id(self), text_hash(name), (DWORD)hint);
    return g_f.list_index;
}
void *THISCALL f_list_create(void *self, const char *dir, const char *name, void **out)
{
    ev('LMAK', id(self), text_hash(dir), text_hash(name));
    ev('LMKo', id(out));
    return g_f.load_result;
}
CClassInfo *THISCALL f_entity_class(void *) { return at<Getter>(g_f.entity_projectile ? 0x491930 : 0x4163b0)(); }
void *THISCALL f_entity_owner(void *self)
{
    ev('E68', id(self));
    return g_f.entity_owner;
}
void THISCALL f_bad(void *self) { ev('BAD', id(self)); }

// Stand-ins for exe functions
void *THISCALL s_game_type(void *app)
{
    ev('GTYP', id(app));
    return g_f.game_type;
}
int THISCALL s_app_mode(void *app)
{
    ev('MODE', id(app));
    return g_f.app_mode;
}
DWORD THISCALL s_set_mode(void *app, const char *name)
{
    ev('SMOD', id(app), text_hash(name));
    return 0x5a31;
}
void THISCALL s_base_start(void *self) { ev('BSTR', id(self)); }
void *THISCALL s_owner(void *reporter)
{
    ev('OWNR', id(reporter));
    return g_f.owner;
}
void *THISCALL s_window(void *window, void *owner, void *mission, DWORD one, DWORD text)
{
    ev('WIND', id(owner), id(mission), one);
    ev('WINt', text);
    return window;
}
void *THISCALL s_first_character(void *world)
{
    ev('CHR1', id(world));
    return g_f.character;
}
void *THISCALL s_current_layer(void *world)
{
    ev('CLAY', id(world));
    return g_f.other;
}
DWORD __fastcall s_mission_display(void *manager, DWORD a, DWORD flag)
{
    ev('MDSP', id(manager), a, flag & 0xff);
    return 0x5098;
}
void *THISCALL s_player(void *entity)
{
    ev('PLYR', id(entity));
    return g_f.player;
}
DWORD __fastcall s_item_count(void *entity, DWORD type)
{
    ev('ITEM', id(entity), type);
    return g_f.counts[type & 1];
}
DWORD THISCALL s_ore(void *entity)
{
    ev('ORE', id(entity));
    return g_f.counts[2];
}
DWORD THISCALL s_crystals(void *entity)
{
    ev('CRYS', id(entity));
    return g_f.counts[3];
}
DWORD THISCALL s_player_set0(void *p, DWORD v)
{
    ev('PS0', id(p), v);
    return v + 1;
}
DWORD THISCALL s_player_set1(void *p, DWORD v)
{
    ev('PS1', id(p), v);
    return v + 2;
}
DWORD THISCALL s_player_set2(void *p, DWORD v)
{
    ev('PS2', id(p), v);
    return v + 3;
}
DWORD THISCALL s_player_set3(void *p, DWORD v)
{
    ev('PS3', id(p), v);
    return v + 4;
}
DWORD THISCALL s_draw(void *stat, void *data, void *par, DWORD x, DWORD y, DWORD top)
{
    ev('DRAW', id(stat), id(data), id(par));
    ev('DRAx', x, y, top);
    return g_f.row_height + (top & 7);
}

struct Patch {
    DWORD address;
    void *target;
};
const Patch PATCHES[] = {
    {0x59ff80, (void *)&s_game_type},      {0x5a01a0, (void *)&s_app_mode},     {0x5a3180, (void *)&s_set_mode},
    {0x4cebb0, (void *)&s_base_start},     {0x4cdf30, (void *)&s_owner},        {0x50cab0, (void *)&s_window},
    {0x4f5d80, (void *)&s_first_character}, {0x4f1050, (void *)&s_current_layer}, {0x509890, (void *)&s_mission_display},
    {0x5bac80, (void *)&s_player},         {0x426860, (void *)&s_item_count},   {0x58fd80, (void *)&s_ore},
    {0x58fdd0, (void *)&s_crystals},       {0x5ba360, (void *)&s_player_set0},  {0x5ba390, (void *)&s_player_set1},
    {0x5ba3c0, (void *)&s_player_set2},    {0x5ba3f0, (void *)&s_player_set3},  {0x5533e0, (void *)&s_draw},
};
constexpr int NPATCH = sizeof PATCHES / sizeof PATCHES[0];
CodePatch g_patches[NPATCH];

// The globals the fakes replace and the module's own, saved around the checks
constexpr DWORD LIST = 0x6c02e8;
DWORD g_saved_app, g_saved_world, g_saved_reporter, g_saved_cache, g_saved_list_vt, g_saved_entries, g_saved_count;
DWORD g_saved_items[2];
BYTE g_saved_guard;
void save_globals()
{
    g_saved_cache = dw(at<void *>(QUEST_GAME_TYPE_CACHE), 0);
    g_saved_guard = *at<BYTE *>(QUEST_ITEM_GUARD);
    g_saved_items[0] = dw(at<void *>(QUEST_ITEM_CRYSTALS), 0);
    g_saved_items[1] = dw(at<void *>(QUEST_ITEM_ORE_DEPOSITS), 0);
}
void restore_globals()
{
    dw(at<void *>(QUEST_GAME_TYPE_CACHE), 0) = g_saved_cache;
    *at<BYTE *>(QUEST_ITEM_GUARD) = g_saved_guard;
    dw(at<void *>(QUEST_ITEM_CRYSTALS), 0) = g_saved_items[0];
    dw(at<void *>(QUEST_ITEM_ORE_DEPOSITS), 0) = g_saved_items[1];
}

void fakes_in()
{
    memset(&g_f, 0, sizeof g_f);
    for (DWORD &s : g_f.vt_app)
        s = ptrv((void *)&f_bad);
    g_f.vt_app[13] = ptrv((void *)&f_time);
    g_f.vt_app[14] = ptrv((void *)&f_total_time);
    g_f.vt_app[97] = ptrv((void *)&f_game_data);
    g_f.vt_app[98] = ptrv((void *)&f_manager);
    for (DWORD &s : g_f.vt_reporter)
        s = ptrv((void *)&f_bad);
    g_f.vt_reporter[16] = ptrv((void *)&f_warn);
    g_f.vt_reporter[23] = ptrv((void *)&f_show);
    // the list's own vtable with the three slots the lookup uses replaced
    g_saved_list_vt = dw(at<void *>(LIST), 0);
    memcpy(g_f.vt_list, at<void *>(g_saved_list_vt), sizeof g_f.vt_list);
    g_f.vt_list[1] = ptrv((void *)&f_list_name);
    g_f.vt_list[3] = ptrv((void *)&f_list_find);
    g_f.vt_list[37] = ptrv((void *)&f_list_create);
    for (DWORD &s : g_f.vt_entity)
        s = ptrv((void *)&f_bad);
    g_f.vt_entity[2] = ptrv((void *)&f_entity_class);
    g_f.vt_entity[68] = ptrv((void *)&f_entity_owner);
    g_saved_world = dw(at<void *>(0x6c2080), 0);
    g_saved_app = dw(at<void *>(0x713f14), 0);
    g_saved_reporter = dw(at<void *>(0x6c02cc), 0);
    g_saved_entries = dw(at<void *>(LIST), 0x30);
    g_saved_count = dw(at<void *>(LIST), 0x34);
    dw(at<void *>(0x6c2080), 0) = ptrv(g_f.app);
    dw(at<void *>(0x713f14), 0) = ptrv(g_f.app);
    dw(at<void *>(0x6c02cc), 0) = ptrv(g_f.reporter);
    dw(at<void *>(LIST), 0) = ptrv(g_f.vt_list);
    dw(at<void *>(LIST), 0x30) = ptrv(g_f.entries);
    dw(at<void *>(LIST), 0x34) = NENTRY;
    for (int i = 0; i < NPATCH; i++)
        patch_jump(g_patches[i], PATCHES[i].address, PATCHES[i].target);
}
void fakes_out()
{
    for (int i = NPATCH - 1; i >= 0; i--)
        unpatch(g_patches[i]);
    dw(at<void *>(0x6c2080), 0) = g_saved_world;
    dw(at<void *>(0x713f14), 0) = g_saved_app;
    dw(at<void *>(0x6c02cc), 0) = g_saved_reporter;
    dw(at<void *>(LIST), 0) = g_saved_list_vt;
    dw(at<void *>(LIST), 0x30) = g_saved_entries;
    dw(at<void *>(LIST), 0x34) = g_saved_count;
}

// ---- Building objects (both sides from the same random numbers) -----------------------------------------

const char *const TEXTS[] = {"Ore Collected", "", "A Cool Statistic", "%s (typical)", "Kills", "Time", "x"};
const char *pick(const char *const *list, DWORD n) { return list[random() % n]; }
#define PICK(list) pick(list, sizeof list / sizeof list[0])

float raw_float(DWORD b)
{
    float f;
    memcpy(&f, &b, 4);
    return f;
}
float rnd_float()
{
    switch (random() % 12) {
    case 0: return 0.0f;
    case 1: return raw_float(0x80000000);   // -0
    case 2: return 1.0f;
    case 3: return 0.5f;
    case 4: return 100.0f;
    case 5: return -7.25f;
    case 6: return raw_float(0x7f800000);   // infinity
    case 7: return raw_float(0x7fc00000);   // NaN
    case 8: return raw_float(0x33d6bf95);   // 1e-7
    case 9: return 16777217.0f;
    default: return (float)(int)(random() % 20001 - 10000) / 8.0f;
    }
}
float rnd_weight() { return random() % 3 ? 0.0f : rnd_float(); }
DWORD rnd_int()
{
    switch (random() % 8) {
    case 0: return 0;
    case 1: return 1;
    case 2: return 0x7fffffff;
    case 3: return 0xffffffff;
    case 4: return 0x80000000;
    default: return random() % 1000;
    }
}
void fill_data(BYTE *o, int k)
{
    switch (k) {
    case K_COUNTER_DATA:
        for (DWORD off = 8; off < 0x14; off += 4)
            dw(o, off) = rnd_int();
        break;
    case K_FLOAT_DATA:
        for (DWORD off = 8; off < 0x14; off += 4)
            fl(o, off) = rnd_float();
        break;
    case K_TIME_DATA:
        for (DWORD off = 8; off < 0x10; off += 4)
            fl(o, off) = rnd_float();
        break;
    case K_PERCENTAGE_DATA:
        for (DWORD off = 8; off < 0x20; off += 4)
            fl(o, off) = rnd_float();
        break;
    case K_COUNTER_PAR:
        dw(o, 8) = rnd_int();
        break;
    case K_FLOAT_PAR:
    case K_TIME_PAR:
        fl(o, 8) = rnd_float();
        break;
    case K_PERCENTAGE_PAR:
        fl(o, 8) = rnd_float();
        fl(o, 0xc) = rnd_float();
        break;
    default:
        break;
    }
}
BYTE *make_filled(int k)
{
    BYTE *o = static_cast<BYTE *>(create(k));
    fill_data(o, k);
    return o;
}
// A data or par score object for statistic class s: mostly the right class, sometimes another's or none
BYTE *make_entry(int s, bool par, bool exact)
{
    const DWORD r = random() % 10;
    if (exact || r < 7)
        return make_filled(par ? PAR_KINDS[s] : DATA_KINDS[s]);
    if (r < 9)
        return make_filled(par ? PAR_KINDS[(s + 1 + random() % 4) % NSTAT] : DATA_KINDS[(s + 1 + random() % 4) % NSTAT]);
    return nullptr;
}
BYTE *make_stat(int k)
{
    BYTE *o = static_cast<BYTE *>(create(k));
    o[8] = (BYTE)(random() % 4 == 0);
    o[9] = (BYTE)(random() % 4 == 0 ? random() % 3 : 0);
    fl(o, 0xc) = random() % 4 ? rnd_float() : 0.0f;
    fl(o, 0x10) = rnd_weight();
    zs_set(o + 0x14, PICK(TEXTS));
    zs_set(o + 0x18, PICK(TEXTS));
    dw(o, 0x1c) = random() % 8;
    dw(o, 0x20) = random() % 8;
    return o;
}
BYTE *make_column()
{
    BYTE *c = static_cast<BYTE *>(create(K_COLUMN));
    zs_set(c + 8, PICK(TEXTS));
    zs_set(c + 0xc, PICK(TEXTS));
    zs_set(c + 0x10, PICK(TEXTS));
    zs_set(c + 0x14, PICK(TEXTS));
    for (DWORD off = 0x18; off < 0x28; off += 4)
        dw(c, off) = random() % 300;
    return c;
}
void add_column(BYTE *game_type, BYTE *column) { at<DWORD(THISCALL *)(void *, const void *)>(0x552950)(game_type + 0x18, column); }

// A game type with 18 statistic slots (0-18 of them counted) of the five classes or none, and columns
constexpr DWORD NSLOTS = 0x12;
int g_slot_class[NSLOTS];   // the statistic class index of slot i (-1: none)
BYTE *make_game_type()
{
    BYTE *g = static_cast<BYTE *>(create(K_GAME_TYPE));
    for (DWORD i = 0; i < NSLOTS; i++) {
        const int s = random() % 6 ? (int)(random() % NSTAT) : -1;
        g_slot_class[i] = s;
        array_add(g + 0x28, s >= 0 ? make_stat(STAT_KINDS[s]) : nullptr);
    }
    if (random() % 4 == 0)
        dw(g, 0x30) = random() % (NSLOTS + 1);   // fewer counted (the rest stays in the block)
    for (DWORD n = random() % 4; n; n--) {
        BYTE *c = make_column();
        add_column(g, c);
        destroy(c, 1);
    }
    zs_set(g + 0x38, PICK(TEXTS));
    zs_set(g + 0x3c, PICK(TEXTS));
    fl(g, 0x40) = rnd_float();
    return g;
}
// The quest data for a game type: one entry per slot (exact = the statistic's class)
BYTE *make_quest_data(bool exact)
{
    BYTE *d = static_cast<BYTE *>(create(K_SPECIFIC));
    for (DWORD i = 0; i < NSLOTS; i++)
        array_add(d + 8, g_slot_class[i] >= 0 ? make_entry(g_slot_class[i], false, exact) : (random() % 3 ? nullptr : make_filled(K_COUNTER_DATA)));
    return d;
}
// A mission's par scores (its +0x34 the list, read by 0x512350): a block of 0x40 bytes
BYTE g_missions[2][0x40];
BYTE *g_pars[2][NSLOTS];
BYTE *make_mission(bool exact)
{
    BYTE *m = g_missions[g_side];
    memset(m, 0, sizeof g_missions[0]);
    for (DWORD i = 0; i < NSLOTS; i++)
        g_pars[g_side][i] = g_slot_class[i] >= 0 && random() % 6 ? make_entry(g_slot_class[i], true, exact) : nullptr;
    dw(m, 0x34) = ptrv(g_pars[g_side]);
    return m;
}
void free_mission()
{
    for (DWORD i = 0; i < NSLOTS; i++)
        if (g_pars[g_side][i]) {
            destroy(g_pars[g_side][i], 1);
            g_pars[g_side][i] = nullptr;
        }
}

// ---- The canonical description of objects ---------------------------------------------------------------

unsigned hash_obj(unsigned h, const BYTE *o, int depth = 0)
{
    if (!o)
        return fnv(h, "null", 4);
    const int k = kind_of(o);
    const DWORD tag = k >= 0 ? (DWORD)k : id(o);
    h = fnv(h, &tag, 4);
    if (k < 0 || depth > 4)
        return h;
    auto num = [&](DWORD off) { h = fnv(h, o + off, 4); };
    auto text = [&](DWORD off) {
        const DWORD t = zs_hash(o + off);
        h = fnv(h, &t, 4);
    };
    auto array = [&](DWORD off, DWORD shown) {   // a pointer array: count and the elements (all slots up to `shown`)
        const DWORD n = dwc(o, off + 8);
        h = fnv(h, &n, 4);
        const DWORD m = n > shown ? n : shown;
        for (DWORD i = 0; i < m && i < 64 && dwc(o, off + 4); i++)
            h = hash_obj(h, pt(dwc(pt(dwc(o, off + 4)), i * 4)), depth + 1);
    };
    switch (k) {
    case K_COLUMN:
        text(8), text(0xc), text(0x10), text(0x14), num(0x18), num(0x1c), num(0x20), num(0x24);
        break;
    case K_GAME_TYPE: {
        text(8), text(0xc), num(0x10), num(0x14);   // CGameType's own fields
        const DWORD n = dwc(o, 0x20);
        h = fnv(h, &n, 4);
        for (DWORD i = 0; i < n && i < 16; i++)
            h = hash_obj(h, pt(dwc(o, 0x1c) + i * 0x28), depth + 1);
        array(0x28, dwc(o, 0x2c) && dwc(o, 0x34) >= NSLOTS ? NSLOTS : 0);
        text(0x38), text(0x3c), num(0x40);
        break;
    }
    case K_SPECIFIC:
        array(8, dwc(o, 0xc) && dwc(o, 0x14) >= NSLOTS ? NSLOTS : 0);
        break;
    case K_COUNTER_DATA:
    case K_FLOAT_DATA:
        num(8), num(0xc), num(0x10);
        break;
    case K_TIME_DATA:
        num(8), num(0xc);
        break;
    case K_PERCENTAGE_DATA:
        for (DWORD off = 8; off < 0x20; off += 4)
            num(off);
        break;
    case K_COUNTER_PAR:
    case K_FLOAT_PAR:
    case K_TIME_PAR:
        num(8);
        break;
    case K_PERCENTAGE_PAR:
        num(8), num(0xc);
        break;
    case K_COUNTER_STAT:
    case K_FLOAT_STAT:
    case K_TIME_STAT:
    case K_TOTAL_TIME_STAT:
    case K_PERCENTAGE_STAT:
        h = fnv(h, o + 8, 2);
        num(0xc), num(0x10), text(0x14), text(0x18), num(0x1c), num(0x20);
        break;
    case K_INCREMENT:
        num(8), num(0xc);
        break;
    default:
        break;
    }
    return h;
}

// ---- Both sides of a case ----------------------------------------------------------------------------------

struct Result {
    DWORD eax, state;
};
Result g_r[2];
int g_trace = 0;   // cases still to log before they run (finding a crash)
bool g_tracing = false;
void stage(const char *what)
{
    if (g_tracing)
        log("    side %d: %s", g_side, what);
}
template <typename C> bool two_sides(Tally &t, const char *what, unsigned seed, C run)
{
    g_tracing = g_trace > 0;
    if (g_trace > 0) {
        g_trace--;
        log("  > %s %u", what, seed);
    }
    for (int side = 0; side < 2; side++) {
        g_side = side;
        g_seed = seed;
        g_loghash[side] = 2166136261u;
        g_nlog[side] = 0;
        g_r[side] = {0, 0};
        if (side == 0)
            hooks_out();
        run(g_r[side]);
        if (side == 0)
            hooks_in();
    }
    const bool same = g_r[0].eax == g_r[1].eax && g_r[0].state == g_r[1].state && g_loghash[0] == g_loghash[1];
    char line[200];
    wsprintfA(line, "%s seed %u [eax %x / %x, state %x / %x, events %d / %d]", what, seed, g_r[0].eax, g_r[1].eax, g_r[0].state,
              g_r[1].state, g_nlog[0], g_nlog[1]);
    if (!same && t.mismatches < 4)
        log_first_difference();
    t.check(same, "quest stats", line);
    return same;
}
DWORD ld_hash(LD v)
{
    BYTE b[10];
    memcpy(b, &v, 10);
    return fnv(2166136261u, b, 10);
}

// ---- Objects: Create, CreateArray, destructors -----------------------------------------------------------

struct Snap {
    DWORD base, n;
    BYTE bytes[0x80];
    BYTE head[0x20][16];
};
Snap g_snap[2];
BYTE g_class_mask[0x80];
void snapshot(const BYTE *p, DWORD n)
{
    Snap &s = g_snap[g_side];
    s.base = ptrv(p);
    s.n = n;
    memcpy(s.bytes, p, n);
    for (DWORD i = 0; i + 4 <= n; i += 4) {
        const DWORD v = *reinterpret_cast<const DWORD *>(p + i);
        memset(s.head[i / 4], 0, 16);
        if (heap_pointer(v) && (v < s.base || v >= s.base + n) && readable(at<void *>(v), 16))
            memcpy(s.head[i / 4], at<void *>(v), 16);
    }
}
DWORD rel(DWORD v)
{
    const Snap &s = g_snap[g_side];
    return v >= s.base && v <= s.base + s.n ? 0xb0000000u + (v - s.base) : v;
}
bool same_snaps(const BYTE *mask)
{
    const Snap &a = g_snap[0], &b = g_snap[1];
    if (a.n != b.n)
        return false;
    for (DWORD i = 0; i + 4 <= a.n; i += 4) {
        if (!(mask[i] | mask[i + 1] | mask[i + 2] | mask[i + 3]))
            continue;
        const DWORD w0 = *reinterpret_cast<const DWORD *>(a.bytes + i), w1 = *reinterpret_cast<const DWORD *>(b.bytes + i);
        if (w0 == w1)
            continue;   // equal raw values first
        if (w0 >= a.base && w0 < a.base + a.n && w1 >= b.base && w1 < b.base + b.n && w0 - a.base == w1 - b.base)
            continue;   // the same offset of the object
        if (heap_pointer(w0) && heap_pointer(w1) && !memcmp(a.head[i / 4], b.head[i / 4], 16))
            continue;   // new blocks with the same start (string buffers: references, capacity, text)
        return false;
    }
    return true;
}
void fill(BYTE *p, DWORD n, unsigned kind)
{
    switch (kind % 4) {
    case 0: memset(p, 0, n); break;
    case 1: memset(p, 0xff, n); break;
    case 2: memset(p, (int)(random() & 0xff), n); break;
    default:
        for (DWORD i = 0; i + 4 <= n; i += 4)
            *reinterpret_cast<DWORD *>(p + i) = random();
    }
}

// The bytes a class's constructor writes: the original Create on pool blocks filled with 0 and 0xff
bool class_mask(const QuestClassDef &c)
{
    BYTE first[0x80];
    for (int pass = 0; pass < 2; pass++) {
        BYTE *p = static_cast<BYTE *>(pool_alloc(c.size));
        memset(p, pass ? 0xff : 0, c.size);
        pool_free(p, c.size);
        BYTE *o = static_cast<BYTE *>(original<Create0>(c.create)());
        if (o != p) {
            if (o)
                destroy(o, 1);
            return false;
        }
        if (pass)
            for (DWORD i = 0; i < c.size; i++)
                g_class_mask[i] = first[i] == o[i];
        else
            memcpy(first, o, c.size);
        destroy(o, 1);
    }
    return true;
}

// Fills an object of the module before its destructor runs (texts, owned objects)
void fill_object(BYTE *e, int k)
{
    switch (k) {
    case K_COLUMN:
        zs_set(e + 8, PICK(TEXTS));
        zs_set(e + 0x14, PICK(TEXTS));
        break;
    case K_GAME_TYPE:
        for (DWORD j = random() % 3; j; j--)
            array_add(e + 0x28, make_stat(STAT_KINDS[random() % NSTAT]));
        for (DWORD j = random() % 3; j; j--) {
            BYTE *c = make_column();
            add_column(e, c);
            destroy(c, 1);
        }
        zs_set(e + 0x3c, PICK(TEXTS));
        break;
    case K_SPECIFIC:
        for (DWORD j = random() % 4; j; j--)
            array_add(e + 8, make_filled(DATA_KINDS[random() % NSTAT]));
        break;
    case K_COUNTER_STAT:
    case K_FLOAT_STAT:
    case K_TIME_STAT:
    case K_TOTAL_TIME_STAT:
    case K_PERCENTAGE_STAT:
        zs_set(e + 0x14, PICK(TEXTS));
        zs_set(e + 0x18, PICK(TEXTS));
        break;
    default:
        fill_data(e, k);
        break;
    }
}

void test_objects(Tally &creates, Tally &arrays, Tally &dtors)
{
    BYTE all[0x80];
    memset(all, 1, sizeof all);
    for (int k = 0; k < NKINDS; k++) {
        const QuestClassDef &c = def(k);
        if (!c.create)
            continue;
        if (!class_mask(c)) {
            memset(g_class_mask, 0, sizeof g_class_mask);
            memset(g_class_mask, 1, 4);
        }
        for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++) {
            bool reused[2];
            DWORD eax[2];
            for (int side = 0; side < 2; side++) {
                g_side = side;
                g_seed = seed * 7 + (unsigned)k;
                if (side == 0)
                    hooks_out();
                BYTE *p = static_cast<BYTE *>(pool_alloc(c.size));
                fill(p, c.size, seed);
                pool_free(p, c.size);
                BYTE *o = static_cast<BYTE *>(at<Create0>(c.create)());
                reused[side] = o == p;
                snapshot(o, c.size);
                eax[side] = rel(ptrv(o));
                destroy(o, 1);
                if (side == 0)
                    hooks_in();
            }
            char d[120];
            wsprintfA(d, "[%s Create seed %u: eax %x / %x]", c.name, seed, eax[0], eax[1]);
            creates.check(eax[0] == eax[1] && same_snaps(reused[0] && reused[1] ? all : g_class_mask), c.name, d);
        }
        for (int n = 0; n < 4; n++) {
            DWORD eax[2];
            for (int side = 0; side < 2; side++) {
                g_side = side;
                if (side == 0)
                    hooks_out();
                BYTE *e = static_cast<BYTE *>(at<CreateN>(c.create_array)(n, 0, 0));
                const DWORD size = n * c.size + 4 > 0x80 ? 0x80 : n * c.size + 4;
                snapshot(e - 4, size);
                eax[side] = rel(ptrv(e));
                if (n)
                    destroy(e, 3);
                else
                    heap_free(e - 4);
                if (side == 0)
                    hooks_in();
            }
            BYTE mask[0x80];
            memset(mask, 0, sizeof mask);
            memset(mask, 1, 4);
            for (int i = 0; i < n; i++)
                for (DWORD b = 0; b < c.size && 4 + i * c.size + b < 0x80; b++)
                    mask[4 + i * c.size + b] = g_class_mask[b];
            char d[120];
            wsprintfA(d, "[%s CreateArray %d: eax %x / %x]", c.name, n, eax[0], eax[1]);
            arrays.check(eax[0] == eax[1] && same_snaps(mask), c.name, d);
        }
        if (!c.dtor)
            continue;
        // The deleting destructor on a filled object: flags 0 (bytes after, then freed here), 1 (eax), 2/3
        // on a filled array of 1-3
        for (unsigned seed = 1; seed <= (unsigned)scaled(8); seed++) {
            const DWORD flags = seed % 4;
            DWORD eax[2];
            for (int side = 0; side < 2; side++) {
                g_side = side;
                g_seed = seed * 13 + (unsigned)k;
                if (side == 0)
                    hooks_out();
                const int n = flags & 2 ? (int)(random() % 3) + 1 : 1;
                BYTE *o = flags & 2 ? static_cast<BYTE *>(at<CreateN>(c.create_array)(n, 0, 0)) : static_cast<BYTE *>(create(k));
                for (int i = 0; i < n; i++)
                    fill_object(o + i * c.size, k);
                BYTE *r = static_cast<BYTE *>(destroy(o, flags));
                const DWORD base = flags & 2 ? ptrv(o) - 4 : ptrv(o);
                eax[side] = ptrv(r) - base;
                if (!(flags & 1)) {
                    // what the destructor writes: the vtables and the arrays (data, count, capacity)
                    DWORD keep[12], nk = 0;
                    keep[nk++] = dwc(o, 0);
                    const DWORD from = k == K_GAME_TYPE ? 0x18 : k == K_SPECIFIC ? 8 : 0;
                    const DWORD to = k == K_GAME_TYPE ? 0x38 : k == K_SPECIFIC ? 0x18 : 0;
                    for (DWORD off = from; off < to; off += 4)
                        keep[nk++] = dwc(o, off);
                    eax[side] = fnv(eax[side], keep, nk * 4);
                    if (flags & 2)
                        heap_free(o - 4);
                    else
                        pool_free(o, c.size);
                }
                if (side == 0)
                    hooks_in();
            }
            char d[120];
            wsprintfA(d, "[%s deleting destructor flags %u seed %u: %x / %x]", c.name, flags, seed, eax[0], eax[1]);
            dtors.check(eax[0] == eax[1], c.name, d);
        }
    }
    // The four bases' deleting destructors (no array branch): on a subclass's object with flags 0 (the
    // object freed here with its size), on an object made at the base level with flags 1
    const int bases[4][2] = {{K_PAR_BASE, K_FLOAT_PAR}, {K_DATA_BASE, K_TIME_DATA}, {K_STAT_BASE, K_PERCENTAGE_STAT}, {K_TIME_STAT_BASE, K_TIME_STAT}};
    for (const auto &b : bases)
        for (unsigned seed = 1; seed <= (unsigned)scaled(6); seed++) {
            const DWORD flags = seed % 2;
            DWORD eax[2];
            for (int side = 0; side < 2; side++) {
                g_side = side;
                g_seed = seed * 29 + (unsigned)b[0];
                if (side == 0)
                    hooks_out();
                BYTE *o;
                if (flags) {
                    o = static_cast<BYTE *>(pool_alloc(def(b[0]).size));
                    at<void *(THISCALL *)(void *)>(0x416610)(o);
                    if (b[0] == K_STAT_BASE || b[0] == K_TIME_STAT_BASE)
                        at<void *(THISCALL *)(void *)>(0x500740)(o + 0x14);
                    dw(o, 0) = def(b[0] == K_TIME_STAT_BASE ? K_STAT_BASE : b[0]).vtable;
                    at<void(THISCALL *)(void *)>(0x4173c0)(o);
                    if (b[0] == K_TIME_STAT_BASE) {
                        dw(o, 0) = def(K_TIME_STAT_BASE).vtable;
                        at<void(THISCALL *)(void *)>(0x4173c0)(o);
                    }
                } else
                    o = static_cast<BYTE *>(create(b[1]));
                // the subclass's fields only on its own object: a block made at the level of an 8-byte
                // base has none (filling it wrote into the next pool block)
                if (!flags || def(b[0]).size == def(b[1]).size)
                    fill_object(o, b[1]);
                BYTE *r = at<BYTE *(THISCALL *)(void *, DWORD)>(def(b[0]).dtor)(o, flags);
                eax[side] = ptrv(r) - ptrv(o);
                if (!flags) {
                    eax[side] = fnv(eax[side], o, 4);
                    pool_free(o, def(b[1]).size);
                }
                if (side == 0)
                    hooks_in();
            }
            char d[120];
            wsprintfA(d, "[%s deleting destructor flags %u seed %u: %x / %x]", def(b[0]).name, flags, seed, eax[0], eax[1]);
            dtors.check(eax[0] == eax[1], def(b[0]).name, d);
        }
}

// The column constructor, the column array's add (growing from capacities 0-5) and deleting destructor
void test_columns(Tally &t)
{
    for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++)
        two_sides(t, "column array", seed, [&](Result &r) {
            BYTE *g = static_cast<BYTE *>(create(K_GAME_TYPE));
            const DWORD fn = seed % 3;
            if (fn == 0) {   // 0x552ad0 on a pattern block
                BYTE *p = static_cast<BYTE *>(pool_alloc(0x28));
                fill(p, 0x28, seed);
                r.eax = at<BYTE *(THISCALL *)(void *)>(0x552ad0)(p) == p;
                r.state = hash_obj(2166136261u, p);
                destroy(p, 1);
            } else {
                for (DWORD n = random() % 7; n; n--) {
                    BYTE *c = make_column();
                    const DWORD old = at<DWORD(THISCALL *)(void *, const void *)>(0x552950)(g + 0x18, c);
                    r.eax = fnv(r.eax, &old, 4);
                    destroy(c, 1);
                }
                const DWORD shape[3] = {dwc(g, 0x20), dwc(g, 0x24), dwc(g, 0x1c) ? dwc(pt(dwc(g, 0x1c)) - 4, 0) : 0xffffffff};
                r.state = fnv(hash_obj(2166136261u, g), shape, sizeof shape);
                if (fn == 2) {   // 0x552b00 on a copy of the array (flags 0) and on a heap copy (flags 1)
                    BYTE *a = static_cast<BYTE *>(heap_alloc(0x10));
                    memcpy(a, g + 0x18, 0x10);
                    dw(g, 0x1c) = 0;
                    dw(g, 0x20) = 0;
                    dw(g, 0x24) = 0;
                    const DWORD flags = random() % 2;
                    BYTE *res = at<BYTE *(THISCALL *)(void *, DWORD)>(0x552b00)(a, flags);
                    r.eax = fnv(r.eax, &flags, 4);
                    r.eax += res == a ? 1 : 0;
                    if (!flags) {
                        r.state = fnv(r.state, a, 0x10);
                        heap_free(a);
                    }
                }
            }
            destroy(g, 1);
        });
}

// ---- Class getters and RegisterFields ----------------------------------------------------------------------

struct RegisterCall {
    DWORD var, name, version, parent, size, abstract, create, create_array, register_fields;
};
RegisterCall g_register;
int g_nregister;
CClassInfo *THISCALL fake_register(CClassInfo *self, CClassInfo **var, const char *name, float version, CClassInfo *parent,
                                   DWORD size, DWORD abstract, void *create, void *create_array, void *register_fields)
{
    memset(self, 0, sizeof *self);
    self->name = name;
    *var = self;
    g_register = {ptrv(var), ptrv(name), bits(version), ptrv(parent), size, abstract & 0xff, ptrv(create),
                  ptrv(create_array), ptrv(register_fields)};
    g_nregister++;
    return self;
}

// Descriptor sizes by kind (the ones these RegisterFields make)
constexpr DWORD VT_BYTE = 0x5fd738;
DWORD desc_size(DWORD vtable)
{
    switch (vtable) {
    case VT_STRING: return 0x34;
    case VT_LIST: return 0x44;
    case VT_OBJECT: return 0x3c;
    case VT_OBJECT_ARRAY: return 0x48;
    case VT_INT: return 0x40;
    case VT_ENUM_INT: return 0x44;
    case VT_TYPED_RESOURCE: return 0x38;
    case VT_FLOAT: return 0x44;
    }
    return 0x30;   // the byte kind; the others compared up to 0x30
}
// The bytes of a dword a descriptor kind's constructor writes (the rest is a heap block's old contents)
DWORD desc_mask(DWORD vtable, DWORD off)
{
    if ((off == 0x28 && vtable == VT_STRING) || (off == 0x44 && vtable == VT_OBJECT_ARRAY) || (off == 0x2c && vtable == VT_BYTE))
        return 0xff;
    if (off == 0x2c && vtable == VT_TYPED_RESOURCE)
        return 0xffff;
    return 0xffffffff;
}
bool same_text_ptr(DWORD a, DWORD b)
{
    if (!heap_pointer(a) || !heap_pointer(b) || !readable(at<void *>(a), 12) || !readable(at<void *>(b), 12))
        return false;
    const ZStringBuffer *x = at<const ZStringBuffer *>(a), *y = at<const ZStringBuffer *>(b);
    if (x->capacity > 0x1000 || y->capacity > 0x1000 || !readable(x, 8 + x->capacity) || !readable(y, 8 + y->capacity))
        return false;
    return !lstrcmpA(x->text, y->text);
}
// Two descriptors dword by dword: raw, strings by text, element descriptors the same way. Bytes no
// constructor writes are left out: +0x24 (the inherited flag) and the upper three bytes of the string
// kind's byte +0x28, the object array kind's +0x44, the byte kind's +0x2c and the upper two of the
// resource kind's +0x2c (a heap block's old contents). class_a/class_b:
// the class each side registered into.
bool same_desc(const CFieldDesc *a, const CFieldDesc *b, int depth = 0, DWORD class_a = 0, DWORD class_b = 0)
{
    if (!a || !b)
        return a == b;
    if (a->vftable != b->vftable)
        return false;
    const DWORD n = desc_size(a->vftable);
    for (DWORD off = 0; off < n; off += 4) {
        if (off == 0x24)
            continue;
        DWORD x = dwc(a, off) & desc_mask(a->vftable, off), y = dwc(b, off) & desc_mask(a->vftable, off);
        if (x == y || same_text_ptr(x, y) || (class_a && x == class_a && y == class_b))
            continue;
        if (depth == 0 && heap_pointer(x) && heap_pointer(y) && readable(at<void *>(x), 0x48) && readable(at<void *>(y), 0x48) &&
            same_desc(at<const CFieldDesc *>(x), at<const CFieldDesc *>(y), 1, class_a, class_b))
            continue;
        return false;
    }
    return true;
}
void log_desc(const char *what, const CFieldDesc *f)
{
    if (!f) {
        log("    %s: null", what);
        return;
    }
    const DWORD *d = reinterpret_cast<const DWORD *>(f);
    log("    %s: vt %x name '%s' off %x | %x %x %x %x %x %x %x %x", what, f->vftable, f->name.buffer ? f->name.c_str() : "", f->offset, d[10],
        d[11], d[12], d[13], d[14], d[15], d[16], d[17]);
}

constexpr DWORD MAXF = 8;
struct Made {
    CFieldDesc *fields[MAXF];
    DWORD n, result, registered, stat_options, cls;
};
void run_register_fields(const QuestClassDef &c, bool var_zero, Made &out)
{
    CClassInfo *&var = *at<CClassInfo **>(c.var);
    CClassInfo *const saved = var;
    const DWORD saved_options = dw(at<void *>(QUEST_STAT_OPTIONS_FIELD), 0);
    alignas(4) BYTE fake_class[sizeof(CClassInfo)];
    memset(fake_class, 0, sizeof fake_class);
    var = var_zero ? nullptr : reinterpret_cast<CClassInfo *>(fake_class);
    g_nregister = 0;
    memset(&g_register, 0, sizeof g_register);
    CodePatch p;
    patch_jump(p, 0x415780, (void *)&fake_register);
    const DWORD r = at<DWORD(__cdecl *)()>(c.register_fields)();
    unpatch(p);
    CClassInfo *ci = var;
    out.n = ci->num_fields;
    out.cls = ptrv(ci);
    out.registered = (DWORD)g_nregister;
    for (DWORD i = 0; i < MAXF; i++)
        out.fields[i] = i < ci->num_fields ? ci->fields[i] : nullptr;
    const DWORD last = ci->num_fields ? ptrv(ci->fields[ci->num_fields - 1]) : 0;
    out.result = r == ptrv(ci) ? 'C' : last && r >= last && r < last + 0x48 ? 0xd0000000u + (r - last) : r;
    out.stat_options = dw(at<void *>(QUEST_STAT_OPTIONS_FIELD), 0);
    if (out.stat_options == saved_options)
        out.stat_options = 0;
    if (ci->fields)
        heap_free(ci->fields);
    if (var_zero)
        pool_free(ci, sizeof(CClassInfo));
    var = saved;
    dw(at<void *>(QUEST_STAT_OPTIONS_FIELD), 0) = saved_options;
}
// The descriptors made (the element descriptor kept in 0x712308 is owned by "Stat/Row Options List" and
// goes with it: deleting it a second time corrupted the heap for later checks, sort_list's grids)
void free_made(Made &m)
{
    for (DWORD i = 0; i < m.n && i < MAXF; i++)
        if (m.fields[i])
            vcall<void *>(m.fields[i], 0, 1);
}

void test_getter(Tally &t, const char *name, DWORD getter, DWORD var_address)
{
    CClassInfo *&var = *at<CClassInfo **>(var_address);
    CClassInfo *const saved = var;
    RegisterCall calls[2];
    DWORD results[2], set[2];
    for (int side = 0; side < 2; side++) {
        if (side == 0)
            hooks_out();
        set[side] = ptrv(at<Getter>(getter)());
        var = nullptr;
        g_nregister = 0;
        memset(&g_register, 0, sizeof g_register);
        CodePatch p;
        patch_jump(p, 0x415780, (void *)&fake_register);
        void *made = at<Getter>(getter)();
        unpatch(p);
        results[side] = (made == var ? 1 : 0) + (DWORD)g_nregister * 2;
        calls[side] = g_register;
        if (var)
            pool_free(var, sizeof(CClassInfo));
        var = saved;
        if (side == 0)
            hooks_in();
    }
    char d[160];
    wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, size %x / %x, parent %x / %x]", getter, set[0], set[1], results[0], results[1],
              calls[0].size, calls[1].size, calls[0].parent, calls[1].parent);
    t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
            name, d);
}

void test_classes(Tally &t, Tally &real)
{
    for (const QuestClassDef &g : QUEST_STAT_CLASSES)
        test_getter(t, g.name, g.getter, g.var);
    for (const QuestGetterCopy &g : QUEST_GETTER_COPIES)
        test_getter(t, "getter copy", g.address, g.var);
    for (const QuestClassDef &c : QUEST_STAT_CLASSES) {
        if (!c.register_fields)
            continue;
        for (int var_zero = 0; var_zero < 2; var_zero++) {
            Made m[2];
            RegisterCall calls[2];
            for (int side = 0; side < 2; side++) {
                if (side == 0)
                    hooks_out();
                run_register_fields(c, var_zero != 0, m[side]);
                calls[side] = g_register;
                if (side == 0)
                    hooks_in();
            }
            bool same = m[0].n == m[1].n && m[0].result == m[1].result && m[0].registered == m[1].registered &&
                        memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0 && !m[0].stat_options == !m[1].stat_options &&
                        (!m[0].stat_options || same_desc(at<CFieldDesc *>(m[0].stat_options), at<CFieldDesc *>(m[1].stat_options)));
            for (DWORD i = 0; i < m[0].n && i < MAXF && same; i++)
                same = same_desc(m[0].fields[i], m[1].fields[i], 0, m[0].cls, m[1].cls);
            char d[160];
            wsprintfA(d, "[RegisterFields %s var %d: %u / %u fields, eax %x / %x, registered %u / %u]", c.name, var_zero, m[0].n, m[1].n,
                      m[0].result, m[1].result, m[0].registered, m[1].registered);
            if (!same && t.mismatches < 3)
                for (DWORD i = 0; i < m[0].n && i < MAXF; i++) {
                    log_desc("original", m[0].fields[i]);
                    log_desc("reimpl", m[1].fields[i]);
                }
            t.check(same, c.name, d);
            // Real data: the class's own descriptors as registered at startup
            const CClassInfo *ci = *at<CClassInfo **>(c.var);
            bool same_real = ci && ci->num_fields - ci->num_inherited == m[0].n;
            for (DWORD i = 0; same_real && i < m[0].n && i < MAXF; i++)
                same_real = same_desc(ci->fields[ci->num_inherited + i], m[0].fields[i], 0, ptrv(ci), m[0].cls);
            wsprintfA(d, "[registered %s: %u own fields, made %u]", c.name, ci ? ci->num_fields - ci->num_inherited : 0, m[0].n);
            if (!same_real && real.mismatches < 2)
                for (DWORD i = 0; i < m[0].n && i < MAXF; i++) {
                    log_desc("registered", ci->fields[ci->num_inherited + i]);
                    log_desc("made", m[0].fields[i]);
                }
            real.check(same_real, c.name, d);
            free_made(m[0]);
            free_made(m[1]);
        }
    }
    // The registered class infos: size, Create, CreateArray, name, abstract
    for (const QuestClassDef &c : QUEST_STAT_CLASSES) {
        const CClassInfo *ci = *at<CClassInfo **>(c.var);
        char d[160];
        wsprintfA(d, "[%s: class info %x, size %x, create %x, create array %x]", c.name, ptrv(ci), ci ? ci->size : 0,
                  ci ? ptrv((void *)ci->create) : 0, ci ? ptrv((void *)ci->create_array) : 0);
        const DWORD create = c.create ? c.create : 0x46b760, create_array = c.create ? c.create_array : 0x52dc80;
        real.check(ci && ci->size == c.size && ptrv((void *)ci->create) == create && ptrv((void *)ci->create_array) == create_array &&
                       !lstrcmpA(ci->name, c.name) && ci->is_abstract == (c.create ? 0 : 1),
                   c.name, d);
    }
}

// ---- Behaviour in the fake world ------------------------------------------------------------------------

// The world of one case: the game type (and the slot classes), the quest data, the mission, the cache
// and the list, from the case's random numbers (the same on both sides)
struct World {
    BYTE *game_type, *data, *mission;
    DWORD stats;   // the game type's statistic count as made (restored before it is deleted)
};
World g_w[2];
BYTE *g_other[2];   // an object of another class (a column) for the lookups and the game type

// The real game type of the quest game (Resources/Game Types/QuestMode.ZGT) read on the side
// (Object_CreateFromFile), its slot classes taken from its statistics
const char *const QUEST_MODE_FILE = "Resources/Game Types/QuestMode.ZGT";
bool g_from_file = false;
BYTE *create_from_file(const char *path)
{
    ZString p;
    original<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&p, path);
    void *o = at<void *(__fastcall *)(ZString *, DWORD, DWORD, void **)>(0x4164c0)(&p, 0, 0, nullptr);
    original<void(THISCALL *)(ZString *)>(0x4defd0)(&p);
    return static_cast<BYTE *>(o);
}
BYTE *g_file_template[2];   // read once per side before the fakes (side 0 with the hooks out)
BYTE *load_game_type()
{
    BYTE *g = static_cast<BYTE *>(clone(g_file_template[g_side]));
    for (DWORD i = 0; i < NSLOTS; i++)
        g_slot_class[i] = g && i < dwc(g, 0x30) && dwc(g, 0x34) >= NSLOTS ? stat_index(kind_of(pt(dwc(pt(dwc(g, 0x2c)), i * 4)))) : -1;
    return g;
}

void setup_world(bool exact, bool force_cache = false)
{
    World &w = g_w[g_side];
    w.game_type = g_from_file ? load_game_type() : make_game_type();
    stage("game type");
    w.stats = dwc(w.game_type, 0x30);
    w.data = make_quest_data(exact);
    w.mission = random() % 5 ? make_mission(exact) : nullptr;
    if (!w.mission)
        memset(g_pars[g_side], 0, sizeof g_pars[0]);
    g_other[g_side] = make_column();
    dw(g_f.app, 0) = ptrv(g_f.vt_app);
    dw(g_f.reporter, 0) = ptrv(g_f.vt_reporter);
    dw(g_f.entity, 0) = ptrv(g_f.vt_entity);
    memset(g_f.game_data, 0, sizeof g_f.game_data);
    const DWORD r = random() % 8;
    dw(g_f.game_data, 0x30) = r < 6 ? ptrv(w.data) : r == 6 ? ptrv(g_other[g_side]) : 0;
    const DWORD gt = random() % 8;
    g_f.game_type = gt < 6 ? w.game_type : gt == 6 ? g_other[g_side] : nullptr;
    g_f.app_mode = random() % 3 ? -1 : (int)(random() % 4);
    // the game type list: the lookup's index and the four entries (the game type, another object,
    // missing and loadable, loading)
    g_f.list_index = random() % 6;
    memset(g_f.entries, 0, sizeof g_f.entries);
    dw(g_f.entries[0], 0x78) = ptrv(w.game_type);
    dw(g_f.entries[1], 0x78) = ptrv(g_other[g_side]);
    dw(g_f.entries[2], 0x78) = 0;
    dw(g_f.entries[3], 0x78) = random() % 2 ? ptrv(w.game_type) : 0;
    g_f.entries[3][0x7f] = (BYTE)(random() % 2);
    const DWORD lr = random() % 3;
    g_f.load_result = lr == 0 ? w.game_type : lr == 1 ? g_other[g_side] : nullptr;
    // the cache holds a quest game type or nothing (the lookups only keep one); the point values' report
    // needs one (it searches the statistic in it): there the cache or the lookup gives the game type
    dw(at<void *>(QUEST_GAME_TYPE_CACHE), 0) = force_cache || random() % 2 ? ptrv(w.game_type) : 0;
    if (force_cache && random() % 2) {   // or the lookup (0x55e090 when the entry is missing) finds it
        dw(at<void *>(QUEST_GAME_TYPE_CACHE), 0) = 0;
        g_f.list_index = random() % 2 ? 0 : 2;
        g_f.load_result = w.game_type;
    }
    // slot 25 and 40
    g_f.entity_projectile = (BYTE)(random() % 4 != 0);
    g_f.entity_owner = random() % 4 ? g_f.character : g_f.other;
    for (DWORD &c : g_f.counts)
        c = rnd_int();
    *at<BYTE *>(QUEST_ITEM_GUARD) = (BYTE)(random() % 4 ? 3 : random() % 4);
    dw(at<void *>(QUEST_ITEM_CRYSTALS), 0) = 0x40 + random() % 8;
    dw(at<void *>(QUEST_ITEM_ORE_DEPOSITS), 0) = 0x50 + random() % 8;
    g_f.row_height = random() % 40;
    g_f.time = (float)(random() % 5000) / 4.0f;
    g_f.total_time = (float)(random() % 9000) / 4.0f;
}
// The whole state after a case
unsigned state_hash()
{
    const World &w = g_w[g_side];
    unsigned h = hash_obj(hash_obj(hash_obj(2166136261u, w.game_type), w.data), g_other[g_side]);
    for (DWORD i = 0; i < NSLOTS; i++)
        h = hash_obj(h, g_pars[g_side][i]);
    const DWORD globals[5] = {id(at<void *>(dw(at<void *>(QUEST_GAME_TYPE_CACHE), 0))), *at<BYTE *>(QUEST_ITEM_GUARD),
                              dw(at<void *>(QUEST_ITEM_CRYSTALS), 0), dw(at<void *>(QUEST_ITEM_ORE_DEPOSITS), 0),
                              id(at<void *>(dw(g_f.game_data, 0x30)))};
    h = fnv(h, globals, sizeof globals);
    for (int i = 0; i < NENTRY; i++) {
        const DWORD e[2] = {id(at<void *>(dw(g_f.entries[i], 0x78))), g_f.entries[i][0x7f]};
        h = fnv(h, e, sizeof e);
    }
    return fnv(fnv(h, &g_f.time, 4), &g_f.total_time, 4);
}
void free_world()
{
    World &w = g_w[g_side];
    // a new quest data made by the case (slot 23) goes too
    BYTE *d = pt(dw(g_f.game_data, 0x30));
    if (d && d != w.data && d != g_other[g_side])
        destroy(d, 1);
    dw(w.game_type, 0x30) = w.stats;
    destroy(w.game_type, 1);
    if (w.data)
        destroy(w.data, 1);
    if (g_other[g_side])
        destroy(g_other[g_side], 1);
    free_mission();
    w = {};
}

// The functions under test
enum Fn { F_STAT_SLOT, F_BASE_HIDDEN, F_BASE_PAR_HIDDEN, F_ADD, F_CLOCK, F_START, F_SLOT24, F_SLOT25, F_MISSION_OVER, F_SLOT40,
          F_SLOT42, F_SLOT44, F_SPECIFIC, F_COLUMN, F_ANY_SHOWN, F_ANY_SCORED, F_ROWS, F_TOTAL, F_DONE, F_STAT, F_RESET, F_INCREMENT,
          NFN };
const char *const FN_NAMES[NFN] = {"statistic slots", "0x554b80 base slot 29", "0x554ce0 base slot 30", "statistic add",
                                   "0x558d60 time clock", "game type slot 23", "game type slot 24", "game type slot 25",
                                   "game type slot 29", "game type slot 40", "game type slot 42", "game type slot 44",
                                   "0x5525b0 quest data", "0x5525e0 column", "0x5525f0 any shown", "0x552690 any scored",
                                   "0x552730 rows", "0x5527f0 total points", "0x5528c0 mission done", "0x552940 statistic",
                                   "quest data slot 19", "CIncrementCounterStatAction slot 23"};
const int STAT_SLOTS[] = {23, 25, 26, 27, 28, 29, 30, 31, 35};
constexpr int NSTAT_SLOTS = sizeof STAT_SLOTS / sizeof STAT_SLOTS[0];

DWORD slot_of(const void *o, int s) { return slot(o, s); }
using Slot0 = DWORD(THISCALL *)(void *);
using Slot1 = DWORD(THISCALL *)(void *, DWORD);
using Slot2 = DWORD(THISCALL *)(void *, void *, void *);
using Slot3 = DWORD(THISCALL *)(void *, void *, void *, void *);
using Points = LD(THISCALL *)(void *, void *, void *);

DWORD g_own_slot[2] = {0xffffffff, 0xffffffff};   // a game type slot holding a statistic made by the case
BYTE *g_own_kept[2];

// One case: the statistic of slot `which` (its class index), the slot or function
void behaviour_case(int fn, int stat_class, int slot, Result &r)
{
    const bool exact = fn == F_STAT_SLOT && slot >= 25 && slot <= 28;   // the text slots read their objects unchecked
    setup_world(exact, (fn == F_STAT_SLOT && slot == 31) || fn == F_TOTAL);
    stage("world");
    World &w = g_w[g_side];
    // a statistic of the class with its data and par score (from the world when it has one there)
    BYTE *stat = nullptr, *data = nullptr, *par = nullptr;
    BYTE *own[3] = {nullptr, nullptr, nullptr};   // made here, freed after the case
    if (stat_class >= 0) {
        DWORD i = random() % NSLOTS;
        for (DWORD j = 0; j < NSLOTS && g_slot_class[i] != stat_class; j++)
            i = (i + 1) % NSLOTS;
        // the point value's report searches the statistic in the game type: there it must be
        if (g_slot_class[i] == stat_class && (g_from_file || slot == 31 || random() % 4)) {
            stat = pt(dwc(pt(dwc(w.game_type, 0x2c)), i * 4));
            data = pt(dwc(pt(dwc(w.data, 0xc)), i * 4));
            par = g_pars[g_side][i];
            if (exact) {
                if (!data)
                    data = own[1] = make_entry(stat_class, false, true);
                if (!par)
                    par = own[2] = make_entry(stat_class, true, true);
            }
        } else if (slot == 31) {   // none of the class in the game type: one put into slot i
            stat = own[0] = make_stat(STAT_KINDS[stat_class]);
            data = own[1] = make_entry(stat_class, false, exact);
            par = own[2] = make_entry(stat_class, true, exact);
            g_own_slot[g_side] = i;
            g_own_kept[g_side] = pt(dwc(pt(dwc(w.game_type, 0x2c)), i * 4));
            dw(pt(dwc(w.game_type, 0x2c)), i * 4) = ptrv(stat);
        } else {
            stat = own[0] = make_stat(STAT_KINDS[stat_class]);
            data = own[1] = make_entry(stat_class, false, exact);
            par = own[2] = make_entry(stat_class, true, exact);
        }
    }
    stage("objects");
    switch (fn) {
    case F_STAT_SLOT:
        if (slot >= 25 && slot <= 28) {
            alignas(4) BYTE out[4];
            zs_ctor(out);
            if (slot == 25 || slot == 27)
                at<Slot2>(slot_of(stat, slot))(stat, data, out);
            else
                at<Slot3>(slot_of(stat, slot))(stat, data, par, out);
            r.eax = zs_hash(out);
            zs_dtor(out);
        } else if (slot == 31)
            r.eax = ld_hash(at<Points>(slot_of(stat, 31))(stat, data, par));
        else {
            r.eax = at<Slot2>(slot_of(stat, slot))(stat, data, par);
            if (slot == 23)   // a pointer (the par score) or a sum; the time statistics' is void
                r.eax = stat_class == 2 || stat_class == 3 ? 0 : heap_pointer(r.eax) ? id(at<void *>(r.eax)) : r.eax;
        }
        break;
    case F_BASE_HIDDEN:
        r.eax = at<Slot2>(0x554b80)(stat, data, par);
        break;
    case F_BASE_PAR_HIDDEN:
        r.eax = at<Slot2>(0x554ce0)(stat, data, par);
        break;
    case F_ADD: {
        const DWORD v = rnd_int();
        const float f = rnd_float();
        if (stat_class == 0)
            at<void(THISCALL *)(void *, void *, DWORD)>(0x555930)(stat, data, v);
        else if (stat_class == 1)
            at<void(THISCALL *)(void *, void *, float)>(0x5575b0)(stat, data, f);
        else
            at<void(THISCALL *)(void *, void *, float)>(random() % 2 ? 0x55aad0 : 0x55ab90)(stat, data, f);
        break;
    }
    case F_CLOCK:
        r.eax = ld_hash(at<LD(THISCALL *)(void *)>(0x558d60)(stat));
        break;
    case F_START: {
        // the old quest data is deleted by the slot
        const DWORD old = dw(g_f.game_data, 0x30);
        r.eax = vcall<DWORD>(w.game_type, 23) & 0;   // slot 19's eax: void in use
        if (old == ptrv(w.data))
            w.data = nullptr;
        else if (old == ptrv(g_other[g_side]))
            g_other[g_side] = nullptr;
        break;
    }
    case F_SLOT24:
        r.eax = at<Slot1>(0x552310)(w.game_type, random());
        break;
    case F_SLOT25:
        dw(g_f.game_data, 0x30) = ptrv(w.data);   // the quest data is read unchecked
        at<void(THISCALL *)(void *, void *, DWORD)>(0x552320)(w.game_type, random() % 6 ? g_f.entity : nullptr, random());
        break;
    case F_MISSION_OVER:
        r.eax = at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x552430)(w.game_type, w.mission, random(), random() % 100);
        break;
    case F_SLOT40:
        r.eax = at<DWORD(THISCALL *)(void *, void *, DWORD)>(0x552490)(w.game_type, g_f.entity, random());
        break;
    case F_SLOT42:
        r.eax = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x552560)(w.game_type, random(), random(), random());
        break;
    case F_SLOT44:
        r.eax = ptrv(vcall<CClassInfo *>(w.game_type, 44)) == dw(at<void *>(0x712330), 0);
        break;
    case F_SPECIFIC:
        r.eax = id(at<void *(__cdecl *)()>(0x5525b0)());
        break;
    case F_COLUMN: {
        const DWORD n = dwc(w.game_type, 0x20);
        const DWORD i = n ? random() % n : 0;
        r.eax = ptrv(at<BYTE *(THISCALL *)(void *, DWORD)>(0x5525e0)(w.game_type, i)) - dwc(w.game_type, 0x1c);
        break;
    }
    case F_ANY_SHOWN:
        r.eax = at<Slot1>(0x5525f0)(w.game_type, ptrv(w.mission));
        break;
    case F_ANY_SCORED:
        r.eax = at<Slot1>(0x552690)(w.game_type, ptrv(w.mission));
        break;
    case F_ROWS:
        r.eax = at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x552730)(w.game_type, w.mission, random() % 640, random() % 480,
                                                                                    random() % 100);
        break;
    case F_TOTAL:
        r.eax = ld_hash(at<LD(THISCALL *)(void *, void *)>(0x5527f0)(w.game_type, w.mission));
        break;
    case F_DONE:
        r.eax = at<Slot1>(0x5528c0)(w.game_type, ptrv(w.mission));
        break;
    case F_STAT:
        r.eax = id(at<void *(THISCALL *)(void *, DWORD)>(0x552940)(w.game_type, random() % NSLOTS));
        break;
    case F_RESET:
        if (w.data)
            vcall<DWORD>(w.data, 19);
        break;
    case F_INCREMENT: {
        BYTE *a = static_cast<BYTE *>(create(K_INCREMENT));
        dw(a, 8) = random() % NSLOTS;
        dw(a, 0xc) = rnd_int();
        dw(g_f.game_data, 0x30) = ptrv(w.data);   // the quest data is read unchecked
        r.eax = at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x555d90)(a, nullptr, random(), random(), random());
        // the upper bytes: IsKindOf's, the reporter's, or those of the data entry's address
        r.eax = (r.eax & 0xff) | (heap_pointer(r.eax & 0xffffff00) ? 0xaaaaaa00 : r.eax & 0xffffff00);
        destroy(a, 1);
        break;
    }
    }
    stage("called");
    if (g_own_kept[g_side] || g_own_slot[g_side] != 0xffffffff) {   // the game type's slot back
        if (g_own_slot[g_side] != 0xffffffff)
            dw(pt(dwc(w.game_type, 0x2c)), g_own_slot[g_side] * 4) = ptrv(g_own_kept[g_side]);
        g_own_slot[g_side] = 0xffffffff;
        g_own_kept[g_side] = nullptr;
    }
    r.state = state_hash();
    for (int i = 0; i < 3; i++)
        r.state = hash_obj(r.state, own[i]);
    stage("hashed");
    for (BYTE *o : own)
        if (o)
            destroy(o, 1);
    free_world();
}
void test_behaviour(Tally &stats, Tally &game, Tally &file)
{
    // the real game type from its file: every statistic slot of every class it has, the game type
    // functions
    BYTE *probe = g_file_template[1];
    if (probe && kind_of(probe) == K_GAME_TYPE && g_file_template[0] && kind_of(g_file_template[0]) == K_GAME_TYPE) {
        char d[96];
        const unsigned h[2] = {hash_obj(2166136261u, g_file_template[0]), hash_obj(2166136261u, g_file_template[1])};
        wsprintfA(d, "[%s read: %x / %x]", QUEST_MODE_FILE, h[0], h[1]);
        file.check(h[0] == h[1], "file", d);
        g_from_file = true;
        bool has[NSTAT] = {};
        for (DWORD i = 0; i < dwc(probe, 0x30) && dwc(probe, 0x34) >= NSLOTS && i < NSLOTS; i++) {
            const int s = stat_index(kind_of(pt(dwc(pt(dwc(probe, 0x2c)), i * 4))));
            if (s >= 0)
                has[s] = true;
        }
        log("  quest stats: %s has %u statistics, %u columns", QUEST_MODE_FILE, dwc(probe, 0x30), dwc(probe, 0x20));
        for (int s = 0; s < NSTAT; s++)
            for (int j = 0; j < NSTAT_SLOTS && has[s]; j++)
                for (unsigned seed = 1; seed <= (unsigned)scaled(10); seed++) {
                    char what[64];
                    wsprintfA(what, "file %s slot %d", def(STAT_KINDS[s]).name, STAT_SLOTS[j]);
                    two_sides(file, what, seed * 43 + (unsigned)(s * 16 + j), [&](Result &r) { behaviour_case(F_STAT_SLOT, s, STAT_SLOTS[j], r); });
                }
        for (int fn = F_START; fn < NFN; fn++)
            for (unsigned seed = 1; seed <= (unsigned)scaled(20); seed++)
                two_sides(file, FN_NAMES[fn], seed * 59 + (unsigned)fn, [&](Result &r) { behaviour_case(fn, (int)(seed % NSTAT), 0, r); });
        g_from_file = false;
    } else
        log("  quest stats: %s not read (%x / %x)", QUEST_MODE_FILE, ptrv(g_file_template[0]), ptrv(probe));

    for (int s = 0; s < NSTAT; s++)
        for (int j = 0; j < NSTAT_SLOTS; j++)
            for (unsigned seed = 1; seed <= (unsigned)scaled(40); seed++) {
                char what[64];
                wsprintfA(what, "%s slot %d", def(STAT_KINDS[s]).name, STAT_SLOTS[j]);
                two_sides(stats, what, seed * 41 + (unsigned)(s * 16 + j), [&](Result &r) { behaviour_case(F_STAT_SLOT, s, STAT_SLOTS[j], r); });
            }
    for (int fn = F_BASE_HIDDEN; fn < NFN; fn++) {
        const int n = fn == F_CLOCK ? 1 : fn == F_ADD ? 3 : fn <= F_BASE_PAR_HIDDEN ? NSTAT : 1;
        for (int c = 0; c < n; c++)
            for (unsigned seed = 1; seed <= (unsigned)scaled(fn <= F_ADD ? 40 : 80); seed++) {
                const int s = fn == F_CLOCK ? 2 : fn == F_ADD ? (c == 0 ? 0 : c == 1 ? 1 : 4) : fn <= F_BASE_PAR_HIDDEN ? c : (int)(seed % NSTAT);
                two_sides(fn <= F_CLOCK ? stats : game, FN_NAMES[fn], seed * 53 + (unsigned)(fn * 8 + c), [&](Result &r) { behaviour_case(fn, s, 0, r); });
            }
    }
}

// ---- Real data ---------------------------------------------------------------------------------------------

// The running game's game type and quest data (taken before the fakes)
BYTE *g_real_game_type, *g_real_data, *g_real_mission;

// Copies of the real objects on both sides: every statistic's slots with copies of its real data entry
// and par score, the game type functions on a copy of the game type with a copy of the quest data
void test_real(Tally &t)
{
    if (!g_real_game_type || !is_kind_of(g_real_game_type, 0x551c20)) {
        log("  quest stats real data: the running game is not the quest game (game type %x)", ptrv(g_real_game_type));
        return;
    }
    const DWORD n = dwc(g_real_game_type, 0x30);
    log("  quest stats real data: quest game type with %u statistics, %u columns, quest data %x (%u entries), mission %x", n,
        dwc(g_real_game_type, 0x20), ptrv(g_real_data), g_real_data ? dwc(g_real_data, 0x10) : 0, ptrv(g_real_mission));
    for (DWORD i = 0; i < n && i < NSLOTS; i++) {
        BYTE *stat = pt(dwc(pt(dwc(g_real_game_type, 0x2c)), i * 4));
        const int s = stat_index(kind_of(stat));
        if (s < 0)
            continue;
        BYTE *real_data = g_real_data && i < dwc(g_real_data, 0x10) ? pt(dwc(pt(dwc(g_real_data, 0xc)), i * 4)) : nullptr;
        BYTE *real_par = g_real_mission && dwc(g_real_mission, 0x3c) > i ? pt(dwc(pt(dwc(g_real_mission, 0x34)), i * 4)) : nullptr;
        for (int j = 0; j < NSTAT_SLOTS; j++) {
            const int sl = STAT_SLOTS[j];
            BYTE *d0 = real_data && kind_of(real_data) >= 0 ? real_data : nullptr, *p0 = real_par && kind_of(real_par) >= 0 ? real_par : nullptr;
            if (sl >= 25 && sl <= 28 && (kind_of(d0) != DATA_KINDS[s] || (sl != 25 && sl != 27 && kind_of(p0) != PAR_KINDS[s])))
                continue;   // the text slots read their objects unchecked
            char what[80];
            wsprintfA(what, "real %s %u slot %d", def(kind_of(stat)).name, i, sl);
            two_sides(t, what, i * 16 + (unsigned)j, [&](Result &r) {
                setup_world(true);
                BYTE *st = static_cast<BYTE *>(clone(stat));
                BYTE *d = d0 ? static_cast<BYTE *>(clone(d0)) : nullptr, *p = p0 ? static_cast<BYTE *>(clone(p0)) : nullptr;
                dw(at<void *>(QUEST_GAME_TYPE_CACHE), 0) = ptrv(g_w[g_side].game_type);
                BYTE **slots = reinterpret_cast<BYTE **>(pt(dwc(g_w[g_side].game_type, 0x2c)));
                BYTE *const kept = slots[i];
                slots[i] = st;
                if (sl >= 25 && sl <= 28) {
                    alignas(4) BYTE out[4];
                    zs_ctor(out);
                    if (sl == 25 || sl == 27)
                        at<Slot2>(slot_of(st, sl))(st, d, out);
                    else
                        at<Slot3>(slot_of(st, sl))(st, d, p, out);
                    r.eax = zs_hash(out);
                    zs_dtor(out);
                } else if (sl == 31)
                    r.eax = ld_hash(at<Points>(slot_of(st, 31))(st, d, p));
                else
                    r.eax = sl == 23 ? 0 : at<Slot2>(slot_of(st, sl))(st, d, p);
                if (sl == 23)
                    at<Slot2>(slot_of(st, 23))(st, d, p);
                slots[i] = kept;
                r.state = hash_obj(hash_obj(hash_obj(state_hash(), st), d), p);
                destroy(st, 1);
                if (d)
                    destroy(d, 1);
                if (p)
                    destroy(p, 1);
                free_world();
            });
        }
    }
    // the game type functions on a copy with a copy of the quest data and the real mission (read only)
    for (int fn = F_SPECIFIC; fn <= F_STAT; fn++)
        two_sides(t, FN_NAMES[fn], (unsigned)fn, [&](Result &r) {
            setup_world(true);
            BYTE *g = static_cast<BYTE *>(clone(g_real_game_type));
            BYTE *d = g_real_data ? static_cast<BYTE *>(clone(g_real_data)) : nullptr;
            dw(g_f.game_data, 0x30) = ptrv(d);
            dw(at<void *>(QUEST_GAME_TYPE_CACHE), 0) = ptrv(g);
            switch (fn) {
            case F_SPECIFIC: r.eax = id(at<void *(__cdecl *)()>(0x5525b0)()); break;
            case F_COLUMN: r.eax = dwc(g, 0x20) ? ptrv(at<BYTE *(THISCALL *)(void *, DWORD)>(0x5525e0)(g, dwc(g, 0x20) - 1)) - dwc(g, 0x1c) : 0; break;
            case F_ANY_SHOWN: r.eax = at<Slot1>(0x5525f0)(g, ptrv(g_real_mission)); break;
            case F_ANY_SCORED: r.eax = at<Slot1>(0x552690)(g, ptrv(g_real_mission)); break;
            case F_ROWS: r.eax = at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x552730)(g, g_real_mission, 10, 20, 30); break;
            case F_TOTAL: r.eax = ld_hash(at<LD(THISCALL *)(void *, void *)>(0x5527f0)(g, g_real_mission)); break;
            case F_DONE: r.eax = at<Slot1>(0x5528c0)(g, ptrv(g_real_mission)); break;
            case F_STAT: r.eax = id(at<void *(THISCALL *)(void *, DWORD)>(0x552940)(g, 0)); break;
            default: break;
            }
            r.state = hash_obj(hash_obj(state_hash(), g), d);
            destroy(g, 1);
            if (d)
                destroy(d, 1);
            dw(g_f.game_data, 0x30) = 0;
            free_world();
        });
}

// The level's CIncrementCounterStatAction objects (from the parts, their AIs and actions, like the
// missions' check), copies executed in the fake world
constexpr int MAX_SEEN = 16384, MAX_FOUND = 256;
DWORD g_seen[MAX_SEEN];
int g_nseen, g_objects;
BYTE *g_found[MAX_FOUND];
int g_nfound;
bool seen(DWORD p)
{
    for (int i = 0; i < g_nseen; i++)
        if (g_seen[i] == p)
            return true;
    if (g_nseen < MAX_SEEN)
        g_seen[g_nseen++] = p;
    return false;
}
void visit(BYTE *o, int depth)
{
    const CClassInfo *ci = class_of(o);
    if (!ci || g_nseen >= MAX_SEEN || seen(ptrv(o)))
        return;
    g_objects++;
    if (kind_of(o) == K_INCREMENT && g_nfound < MAX_FOUND)
        g_found[g_nfound++] = o;
    if (depth >= 8 || ci->size > 0x1000 || !readable(o, ci->size))
        return;
    for (DWORD off = 4; off + 4 <= ci->size; off += 4) {
        const DWORD v = dwc(o, off);
        if (!heap_pointer(v) || !readable(at<void *>(v), 4))
            continue;
        if (class_of(at<void *>(v)))
            visit(at<BYTE *>(v), depth + 1);
        else if (off + 8 <= ci->size && dwc(o, off + 4) && dwc(o, off + 4) <= 256 && readable(at<void *>(v), dwc(o, off + 4) * 4))
            for (DWORD i = 0; i < dwc(o, off + 4); i++)   // a pointer array's data and count
                visit(at<BYTE **>(v)[i], depth + 1);
    }
}
bool find_level_objects()
{
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (!world)
        return false;
    BYTE *layer = real_player_layer();
    if (!layer || !readable(layer + 0x50, 0x28))
        return false;
    const DWORD columns = dwc(layer, 0x60), rows = dwc(layer, 0x64);
    const ZPtrArray *cells = *reinterpret_cast<ZPtrArray **>(layer + 0x68);
    if (columns > 4096 || rows > 4096 || !cells || !readable(cells, columns * rows * 16))
        return false;
    for (DWORD i = 0; i < columns * rows; i++) {
        const ZPtrArray *c = &cells[i];
        if (c->count > 10000 || !readable(c->data, c->count * 4))
            return false;
        for (DWORD k = 0; k < c->count; k++) {
            BYTE *part = static_cast<BYTE *>(c->data[k]);
            visit(part, 0);
            const LayerPart *p = reinterpret_cast<const LayerPart *>(part);
            if (readable(p, sizeof *p) && p->activity.count <= 1000 && readable(p->activity.data, p->activity.count * 4))
                for (DWORD j = 0; j < p->activity.count; j++)
                    visit(static_cast<BYTE *>(p->activity.data[j]), 1);
        }
    }
    return true;
}
void test_level_actions(bool found, Tally &t)
{
    if (!found) {
        log("  quest stats real data: no level grid found");
        return;
    }
    log("  quest stats real data: %d objects reached from the level's parts, %d CIncrementCounterStatAction", g_objects, g_nfound);
    for (int i = 0; i < g_nfound; i++)
        for (unsigned round = 0; round < 3; round++)
            two_sides(t, "level CIncrementCounterStatAction", (unsigned)i * 4 + round, [&](Result &r) {
                setup_world(true);
                g_f.game_type = round ? g_w[g_side].game_type : g_other[g_side];
                dw(g_f.game_data, 0x30) = ptrv(g_w[g_side].data);
                BYTE *a = static_cast<BYTE *>(clone(g_found[i]));
                r.eax = at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x555d90)(a, nullptr, 0, 0, 0) & 0xff;
                r.state = hash_obj(state_hash(), a);
                destroy(a, 1);
                free_world();
            });
}

}  // namespace

ZAX_CHECKS(quest_stats)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally creates{"quest stats: Create of 17 classes"}, arrays{"quest stats: CreateArray of 17 classes"},
        dtors{"quest stats: deleting destructors"}, columns{"quest stats: the column array"},
        classes{"quest stats: class getters and RegisterFields"}, registered{"quest stats: registered fields and class infos (real data)"},
        stats{"quest stats: the statistics' slots in a fake world"}, game{"quest stats: the game type and quest data in a fake world"},
        file{"quest stats: QuestMode.ZGT in a fake world (real data)"},
        real{"quest stats: the running game's quest game type (real data)"}, level{"quest stats: the level's actions (real data)"};
    save_globals();
    test_objects(creates, arrays, dtors);
    creates.report();
    arrays.report();
    dtors.report();
    test_columns(columns);
    columns.report();
    test_classes(classes, registered);
    classes.report();
    registered.report();
    // the real objects are found before the fakes replace the application
    const bool found = find_level_objects();
    BYTE *app = *at<BYTE **>(0x713f14);
    g_real_game_type = app ? at<BYTE *(THISCALL *)(void *)>(0x59ff80)(app) : nullptr;
    g_real_data = app ? pt(dwc(vcall<BYTE *>(app, 97), 0x30)) : nullptr;
    if (g_real_data && kind_of(g_real_data) != K_SPECIFIC)
        g_real_data = nullptr;
    BYTE *world = *at<BYTE **>(0x6c2080);
    BYTE *manager = world ? vcall<BYTE *>(world, 98) : nullptr;
    g_real_mission = manager ? at<BYTE *(THISCALL *)(void *)>(0x5127e0)(manager) : nullptr;
    hooks_out();
    g_file_template[0] = create_from_file(QUEST_MODE_FILE);
    hooks_in();
    g_file_template[1] = create_from_file(QUEST_MODE_FILE);
    fakes_in();
    test_behaviour(stats, game, file);
    stats.report();
    game.report();
    file.report();
    test_real(real);
    test_level_actions(found, level);
    fakes_out();
    for (BYTE *&t : g_file_template)
        if (t) {
            destroy(t, 1);
            t = nullptr;
        }
    restore_globals();
    real.report();
    level.report();
    log("  quest stats: %u ms", GetTickCount() - start);
}

}  // namespace zax
