// Checks of the mission objectives, missions, the mission manager and the mission actions (missions.cpp)
// against the original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls through vtables and exe addresses run the original
// too), side 1 through the reimplementation. Each side builds its own objects.
//  - Objects: Create of the 17 classes on a pool block filled with a pattern (all bytes compared; a
//    dword that differs counts as equal when both point to blocks with the same first 16 bytes: the
//    "XXX"/"M00" string buffers of the defaults), CreateArray with 0-3 elements (the bytes the
//    constructor writes), the deleting destructors on filled objects (strings set, objectives in the
//    mission, missions in the manager, the maps not to flush) with flags 0-3: eax and the bytes the
//    destructor writes (vtables, the arrays' data, count and capacity).
//  - Classes: the 17 class getters with the class variable set and 0 (ClassInfo_Register 0x415780
//    replaced by a stand-in recording its arguments); the 15 RegisterFields into a fake class info with
//    the variable set and 0 (the descriptors dword by dword, strings by text, the element descriptors of
//    the arrays the same way; eax; the alternative class name), 0x6cfd84 saved around CMission's.
//  - Behaviour in a fake world: the world (0x6c2080) and the application (0x713f14, the same object in
//    the game) are one fake (slot 98 the side's mission manager, +0x2bc/+0x2c0 the cached layers,
//    +0x33c the map table, +0x361), the reporter 0x6c02cc a fake (slot 13 the time, 16/17 the texts,
//    23 the window, +0x34), the game type a fake (slot 2 CQuestGameType's class or CObject's, slot 29),
//    its panel 6 a fake. Stand-ins replace 0x59ff80, 0x5a0c00, 0x5a3600, 0x5a0860, 0x5528c0, 0x4f1050,
//    0x4eb670, 0x5899b0, 0x552940, 0x5525b0, 0x555930, 0x50cab0 and record their arguments. The
//    objects are real (managers, missions, objectives of the three classes, the completed actions are
//    real CSelectNoMission/CIsMissionComplete objects); Object_Clone, the pointer arrays, IsKindOf, the
//    string functions and 0x41a720 run for real. Compared: eax (al for the bool slots), the event log
//    of the fakes and stand-ins, the whole object tree afterwards (texts by content, objects by class),
//    the fake application's bytes.
//  - Real data: the registered class infos (size, Create, CreateArray, alternative name), the
//    descriptors registered at startup against RegisterFields; the objects of the module's classes in
//    the running level (from the parts in its grid through the AIs and their actions, every owned
//    object reached): their vtable against Create's, copies (Object_Clone) on both sides compared, every
//    action executed (a copy) in the fake world in the level's order, after the level's CDefineMission
//    copies (so their missions and objectives are the real ones), and the objective slots 23-25 on
//    copies of the real objectives; the world's own mission manager (slot 98 of the running world) the
//    same way through a copy. The running game is not changed: everything runs on copies, in the fakes.
#include "check_util.h"
#include "field_desc.h"
#include "field_kinds.h"
#include "layer_parts.h"
#include "local_table.h"
#include "missions.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);
WINIMPORT DWORD WINAPI GetTickCount(void);

// Event tags are four-character constants ('SEND')
#pragma clang diagnostic ignored "-Wmultichar"

namespace zax {

int hooks_missions(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD dwc(const void *p, DWORD off) { return *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(p) + off); }

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
constexpr int MAX_SITES = 160;
SavedSite g_sites[MAX_SITES];
int g_nsites;
Hook g_hook_list[MAX_SITES];
void hooks_out()
{
    g_nsites = 0;
    const int n = hooks_missions(nullptr);
    if (!hook_sites_fit("missions", n, MAX_SITES))
        return;
    hooks_missions(g_hook_list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(g_hook_list[i].address);
        if (original_address(g_hook_list[i].address) == site)
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
void *create(const MissionClassDef &c) { return at<Create0>(c.create)(); }
void *clone(void *o) { return at<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffff); }
void zs_ctor(void *z) { at<void *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_dtor(void *z) { at<void(THISCALL *)(void *)>(0x4defd0)(z); }
void zs_set(void *z, const char *s) { at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, s); }
void array_add(void *array, void *p) { vcall<DWORD>(array, 2, &p); }

enum Kind { K_OBJECTIVE, K_COUNT, K_SERIES, K_MISSION, K_MANAGER, K_DEFINE, K_SELECT, K_SELECT_NO, K_ADD, K_UPDATE, K_INCREMENT,
            K_DELETE, K_IS_COMPLETE, K_IS_DEFINED, K_IS_MISSION_COMPLETE, K_FAILED, K_COMPLETED, NKINDS };
static_assert(sizeof MISSION_CLASSES / sizeof MISSION_CLASSES[0] == NKINDS);
// The module's class of an object (-1: none)
int kind_of(const void *p)
{
    const CClassInfo *c = class_of(p);
    if (!c)
        return -1;
    for (int k = 0; k < NKINDS; k++)
        if (c == *at<CClassInfo **>(MISSION_CLASSES[k].var))
            return k;
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
bool g_log_all = false;
void log_first_difference()
{
    if (g_log_all) {
        g_log_all = false;
        for (int i = 0; i < LOG && (i < g_nlog[0] || i < g_nlog[1]); i++)
            log("    %2d: %x %x %x %x / %x %x %x %x", i, g_log[0][i][0], g_log[0][i][1], g_log[0][i][2], g_log[0][i][3], g_log[1][i][0],
                g_log[1][i][1], g_log[1][i][2], g_log[1][i][3]);
    }
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

constexpr int NLAYER = 6;
struct Fakes {
    DWORD vt_world[124], vt_reporter[32], vt_game[32], vt_panel[4];
    BYTE world[0x600];     // also the application: +0x2bc layers, +0x2c0 count, +0x33c table, +0x361
    BYTE reporter[0x40];   // +0x34 (0x4cdf30)
    BYTE game[0x10], panel[0x10], table[0x10];
    BYTE data[0x10], data2[0x20];   // 0x5525b0: +0xc -> data2, +0x18 the value
    DWORD layers[NLAYER][2];
    void *layer_list[NLAYER];
    // scripted answers
    BYTE *manager;           // slot 98
    void *game_type;         // 0x59ff80: &game or null
    BYTE quest, panel_kind, has_panel, expired_mask;
    int current_layer;       // index into layers, -1 none
    float time;
};
Fakes g_f;
const char *const LAYER_NAMES[NLAYER] = {"ACT 2/M9/09 Assimilate Cave1", "ACT 2/M9/09 Assimilate Main", "Test/Nachbau1",
                                         "ACT 2/M9/09 assimilate cave1", "Eigene/Level 1", ""};

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
BYTE *THISCALL f_manager(void *self)
{
    ev('W98', id(self));
    return g_f.manager;
}
float THISCALL f_time(void *self)
{
    g_f.time += 0.25f;
    ev('TIME', id(self), bits(g_f.time));
    return g_f.time;
}
DWORD THISCALL f_warn(void *, const char *text)
{
    ev('WARN', text_hash(text));
    return 0x1600;
}
DWORD THISCALL f_message(void *, const char *text)
{
    ev('MSG', text_hash(text));
    return 0x1700;
}
DWORD THISCALL f_show(void *, void *window, DWORD flag)
{
    ev('SHOW', id(window), flag);
    if (window && id(window) == 0x30000000)
        pool_free(window, 0x1b4);
    return 0x2300;
}
CClassInfo *THISCALL f_game_class(void *)
{
    return g_f.quest ? at<Getter>(0x551c20)() /* CQuestGameType */ : at<Getter>(0x4163b0)() /* CObject */;
}
CClassInfo *THISCALL f_panel_class(void *) { return g_f.panel_kind ? at<Getter>(0x554ae0)() : at<Getter>(0x4163b0)(); }
DWORD THISCALL f_game_29(void *self, void *mission, DWORD a2, const ZString *text)
{
    ev('G29', id(self), id(mission), a2);
    ev('G29t', zs_hash(text));
    return 0x2900;
}
void THISCALL f_bad(void *self)
{
    ev('BAD', id(self));
}

// Stand-ins for exe functions
void *THISCALL s_game_type(void *app)
{
    ev('GTYP', id(app));
    return g_f.game_type;
}
void THISCALL s_5a0c00(void *app, void *name, float f, DWORD b)
{
    ev('A0C0', id(app), ptrv(name), bits(f));
    ev('A0Cb', b);
}
void THISCALL s_mission_time(void *app, DWORD which) { ev('MTIM', id(app), which); }
void THISCALL s_flush(void *app, const char *name) { ev('FLSH', id(app), text_hash(name)); }
void THISCALL s_quest_mission(void *game, void *mission) { ev('QMIS', id(game), id(mission), mission ? zs_hash(static_cast<BYTE *>(mission) + 8) : 0); }
void *THISCALL s_current_layer(void *w)
{
    ev('CLAY', id(w));
    return g_f.current_layer >= 0 ? g_f.layers[g_f.current_layer] : nullptr;
}
const char *THISCALL s_file_name(DWORD *layer)
{
    ev('FNAM', id(layer));
    return LAYER_NAMES[layer[0] % NLAYER];
}
bool THISCALL s_is_expired(void *table, const char *name)
{
    ev('EXPD', id(table), text_hash(name));
    for (int i = 0; i < NLAYER; i++)
        if (!lstrcmpA(name, LAYER_NAMES[i]))
            return (g_f.expired_mask >> i) & 1;
    return false;
}
void *THISCALL s_panel(void *game, DWORD i)
{
    ev('PANL', id(game), i);
    return g_f.has_panel ? g_f.panel : nullptr;
}
void *__cdecl s_quest_data()
{
    ev('QDAT');
    return g_f.data;
}
void THISCALL s_panel_add(void *panel, DWORD value, DWORD one) { ev('PADD', id(panel), value, one); }
void *THISCALL s_window(void *window, void *owner, void *mission, DWORD zero, const ZString *text)
{
    ev('WIND', id(owner), id(mission), zero);
    ev('WINt', zs_hash(text));
    return window;
}

struct Patch {
    DWORD address;
    void *target;
};
const Patch PATCHES[] = {{0x59ff80, (void *)&s_game_type},  {0x5a0c00, (void *)&s_5a0c00},      {0x5a3600, (void *)&s_mission_time},
                         {0x5a0860, (void *)&s_flush},      {0x5528c0, (void *)&s_quest_mission}, {0x4f1050, (void *)&s_current_layer},
                         {0x4eb670, (void *)&s_file_name},  {0x5899b0, (void *)&s_is_expired},  {0x552940, (void *)&s_panel},
                         {0x5525b0, (void *)&s_quest_data}, {0x555930, (void *)&s_panel_add},   {0x50cab0, (void *)&s_window}};
constexpr int NPATCH = sizeof PATCHES / sizeof PATCHES[0];
CodePatch g_patches[NPATCH];
DWORD g_saved_world, g_saved_app, g_saved_reporter;

void fakes_in()
{
    memset(&g_f, 0, sizeof g_f);
    for (DWORD &s : g_f.vt_world)
        s = ptrv((void *)&f_bad);
    g_f.vt_world[98] = ptrv((void *)&f_manager);
    for (DWORD &s : g_f.vt_reporter)
        s = ptrv((void *)&f_bad);
    g_f.vt_reporter[13] = ptrv((void *)&f_time);
    g_f.vt_reporter[16] = ptrv((void *)&f_warn);
    g_f.vt_reporter[17] = ptrv((void *)&f_message);
    g_f.vt_reporter[23] = ptrv((void *)&f_show);
    for (DWORD &s : g_f.vt_game)
        s = ptrv((void *)&f_bad);
    g_f.vt_game[2] = ptrv((void *)&f_game_class);
    g_f.vt_game[29] = ptrv((void *)&f_game_29);
    g_f.vt_panel[2] = ptrv((void *)&f_panel_class);
    g_saved_world = dw(at<void *>(0x6c2080), 0);
    g_saved_app = dw(at<void *>(0x713f14), 0);
    g_saved_reporter = dw(at<void *>(0x6c02cc), 0);
    dw(at<void *>(0x6c2080), 0) = ptrv(g_f.world);
    dw(at<void *>(0x713f14), 0) = ptrv(g_f.world);
    dw(at<void *>(0x6c02cc), 0) = ptrv(g_f.reporter);
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
}

// The fakes for one case (from the case's random numbers; the same on both sides)
void setup_fakes(BYTE *manager)
{
    dw(g_f.world, 0) = ptrv(g_f.vt_world);
    dw(g_f.reporter, 0) = ptrv(g_f.vt_reporter);
    dw(g_f.reporter, 0x34) = ptrv(g_f.panel) + 4;   // the owner 0x4cdf30 gives
    dw(g_f.game, 0) = ptrv(g_f.vt_game);
    dw(g_f.panel, 0) = ptrv(g_f.vt_panel);
    dw(g_f.data, 0xc) = ptrv(g_f.data2);
    dw(g_f.data2, 0x18) = random() & 0xffff;
    memset(g_f.world + 4, 0, sizeof g_f.world - 4);
    const DWORD n = random() % (NLAYER + 1);
    for (DWORD i = 0; i < NLAYER; i++) {
        g_f.layers[i][0] = random() % NLAYER;
        g_f.layer_list[i] = random() % 5 ? g_f.layers[i] : nullptr;
    }
    dw(g_f.world, 0x2bc) = ptrv(g_f.layer_list);
    dw(g_f.world, 0x2c0) = random() % 9 ? n : 0xffffffff;
    dw(g_f.world, 0x33c) = ptrv(g_f.table);
    g_f.world[0x361] = (BYTE)(random() & 1);
    g_f.manager = manager;
    const DWORD r = random();
    g_f.game_type = r % 5 ? g_f.game : nullptr;
    g_f.quest = (BYTE)(r / 5 % 3 != 0);
    g_f.has_panel = (BYTE)(r / 15 % 4 != 0);
    g_f.panel_kind = (BYTE)(r / 60 % 3 != 0);
    g_f.expired_mask = (BYTE)(random() & 0x3f);
    g_f.current_layer = (int)(random() % (NLAYER + 1)) - 1;
    g_f.time = (float)(random() % 1000);
}

// ---- Building objects (both sides from the same random numbers) -----------------------------------------

const char *const TAGS[] = {"O1", "o1", "O2", "Kill The Guards", "", "M01", "m01", "M02", "Main", "Find Zin 3"};
const char *const STATUSES[] = {"Incomplete", "Complete", "Completed", "Failed", "Fail", "complete", "",
                                "Objective Incomplete", "INCOMPLETE now complete", "Done"};
const char *const DESCRIPTIONS[] = {"Log Entry 12 - The Land of nothing", "", "abc7def", "Mission 03: Cave", "no digits", "9"};
const char *pick(const char *const *list, DWORD n) { return list[random() % n]; }
#define PICK(list) pick(list, sizeof list / sizeof list[0])

// A real action for the completed actions and the next status: CSelectNoMission or CIsMissionComplete
void *make_small_action()
{
    return create(MISSION_CLASSES[random() % 2 ? K_SELECT_NO : K_IS_MISSION_COMPLETE]);
}
BYTE *make_objective(int kind)
{
    BYTE *o = static_cast<BYTE *>(create(MISSION_CLASSES[kind]));
    zs_set(o + 8, PICK(TAGS));
    zs_set(o + 0xc, PICK(DESCRIPTIONS));
    zs_set(o + 0x10, PICK(DESCRIPTIONS));
    zs_set(o + 0x14, PICK(STATUSES));
    zs_set(o + 0x18, PICK(STATUSES));
    if (random() % 3 == 0)
        dw(o, 0x1c) = ptrv(make_small_action());
    if (kind == K_COUNT) {
        dw(o, 0x24) = random() % 5;
        dw(o, 0x28) = random() % 6;
        if (random() % 8 == 0)
            dw(o, 0x24) = 0xffffffff;
    } else if (kind == K_SERIES && random() % 2)
        dw(o, 0x24) = ptrv(make_small_action());
    return o;
}
BYTE *make_mission()
{
    BYTE *m = static_cast<BYTE *>(create(MISSION_CLASSES[K_MISSION]));
    zs_set(m + 8, PICK(TAGS));
    zs_set(m + 0xc, PICK(DESCRIPTIONS));
    zs_set(m + 0x10, PICK(DESCRIPTIONS));
    const DWORD n = random() % 5;
    for (DWORD i = 0; i < n; i++)
        array_add(m + 0x14, make_objective((int)(random() % 3)));
    if (random() % 3 == 0)
        dw(m, 0x24) = ptrv(make_small_action());
    dw(m, 0x28) = random() % 3 ? 0xffffffff : random() % 5;
    dw(m, 0x2c) = random() % 2 ? 0xffffffff : random() % 30;
    return m;
}
BYTE *make_manager()
{
    BYTE *g = static_cast<BYTE *>(create(MISSION_CLASSES[K_MANAGER]));
    const DWORD n = random() % 4;
    for (DWORD i = 0; i < n; i++)
        array_add(g + 8, make_mission());
    dw(g, 0x18) = random() % 6 ? random() % (n + 1) : 0xffffffff;
    *reinterpret_cast<float *>(g + 0x1c) = (float)(random() % 100);
    return g;
}

// The canonical description of an object tree: classes, texts, numbers; pointers only as classes
unsigned hash_obj(unsigned h, const BYTE *o, int depth)
{
    if (!o)
        return fnv(h, "null", 4);
    const int k = kind_of(o);
    const DWORD tag = k >= 0 ? (DWORD)k : id(o);
    h = fnv(h, &tag, 4);
    if (k < 0 || depth > 6)
        return h;
    auto num = [&](DWORD off) { h = fnv(h, o + off, 4); };
    auto text = [&](DWORD off) {
        const DWORD t = zs_hash(o + off);
        h = fnv(h, &t, 4);
    };
    auto array = [&](DWORD off) {   // a pointer array: count and the elements
        const DWORD n = dwc(o, off + 8);
        h = fnv(h, &n, 4);
        for (DWORD i = 0; i < n && i < 64; i++)
            h = hash_obj(h, reinterpret_cast<BYTE *const *>(static_cast<uintptr_t>(dwc(o, off + 4)))[i], depth + 1);
    };
    switch (k) {
    case K_OBJECTIVE:
    case K_COUNT:
    case K_SERIES:
        text(8), text(0xc), text(0x10), text(0x14), text(0x18);
        h = hash_obj(h, reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dwc(o, 0x1c))), depth + 1);
        h = hash_obj(h, reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dwc(o, 0x20))), depth + 1);
        if (k == K_COUNT)
            num(0x24), num(0x28);
        if (k == K_SERIES)
            h = hash_obj(h, reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dwc(o, 0x24))), depth + 1);
        break;
    case K_MISSION:
        text(8), text(0xc), text(0x10), array(0x14), num(0x28), num(0x2c), num(0x38);
        h = hash_obj(h, reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dwc(o, 0x24))), depth + 1);
        break;
    case K_MANAGER:
        array(8), num(0x18), num(0x1c);
        break;
    case K_DEFINE: {
        h = hash_obj(h, reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dwc(o, 8))), depth + 1);
        const DWORD n = dwc(o, 0x14);
        h = fnv(h, &n, 4);
        for (DWORD i = 0; i < n && i < 64; i++) {
            const DWORD t = zs_hash(reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dwc(o, 0x10))) + i * 4);
            h = fnv(h, &t, 4);
        }
        break;
    }
    case K_SELECT:
    case K_INCREMENT:
    case K_DELETE:
    case K_IS_COMPLETE:
    case K_IS_DEFINED:
        text(8);
        break;
    case K_UPDATE:
        text(8), text(0xc), text(0x10);
        break;
    case K_ADD:
        h = hash_obj(h, reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dwc(o, 8))), depth + 1);
        break;
    case K_FAILED:
    case K_COMPLETED:
        text(8), text(0xc);
        break;
    default:
        break;
    }
    return h;
}

// The actions a copy of real data holds (completed actions, next status) deleted, so nothing of the
// game runs from them: only the module's classes are followed
void strip_actions(BYTE *o, int depth = 0)
{
    const int k = kind_of(o);
    if (k < 0 || depth > 6)
        return;
    auto drop = [&](DWORD off) {
        if (void *a = reinterpret_cast<void *>(static_cast<uintptr_t>(dwc(o, off)))) {
            destroy(a, 1);
            dw(o, off) = 0;
        }
    };
    auto array = [&](DWORD off) {
        for (DWORD i = 0; i < dwc(o, off + 8); i++)
            strip_actions(reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dwc(o, off + 4)))[i], depth + 1);
    };
    switch (k) {
    case K_OBJECTIVE:
    case K_COUNT:
    case K_SERIES:
        drop(0x1c);
        if (k == K_SERIES)
            drop(0x24);
        strip_actions(reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dwc(o, 0x20))), depth + 1);
        break;
    case K_MISSION:
        drop(0x24);
        array(0x14);
        break;
    case K_MANAGER:
        array(8);
        break;
    case K_DEFINE:
    case K_ADD:
        strip_actions(reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dwc(o, 8))), depth + 1);
        break;
    default:
        break;
    }
}

// ---- Both sides of a case ----------------------------------------------------------------------------------

struct Result {
    DWORD eax, state;
};
Result g_r[2];
bool g_trace = false;
template <typename C> bool two_sides(Tally &t, const char *what, unsigned seed, C run)
{
    if (g_trace)
        log("  > %s %u", what, seed);
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
    t.check(same, "missions", line);
    return same;
}

// The whole state after a case: the object trees, the fake application's bytes, the time
unsigned state_hash(const BYTE *a, const BYTE *b = nullptr, const BYTE *c = nullptr)
{
    unsigned h = hash_obj(hash_obj(hash_obj(2166136261u, a, 0), b, 0), c, 0);
    h = fnv(h, g_f.world + 0x340, 0x40);
    return fnv(h, &g_f.time, 4);
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
bool class_mask(const MissionClassDef &c)
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

void test_objects(Tally &creates, Tally &arrays, Tally &dtors)
{
    for (int k = 0; k < NKINDS; k++) {
        const MissionClassDef &c = MISSION_CLASSES[k];
        if (!class_mask(c)) {
            memset(g_class_mask, 0, sizeof g_class_mask);
            memset(g_class_mask, 1, 4);
        }
        BYTE all[0x80];
        memset(all, 1, sizeof all);
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
        // The deleting destructor on a filled object: flags 0 (bytes after, then freed here), 1 (eax), 2/3
        // on a filled array of 1-3 (bytes of the first element after flags 2)
        for (unsigned seed = 1; seed <= (unsigned)scaled(8); seed++) {
            const DWORD flags = seed % 4;
            DWORD eax[2];
            for (int side = 0; side < 2; side++) {
                g_side = side;
                g_seed = seed * 13 + (unsigned)k;
                if (side == 0)
                    hooks_out();
                const int n = flags & 2 ? (int)(random() % 3) + 1 : 1;
                BYTE *o = flags & 2 ? static_cast<BYTE *>(at<CreateN>(c.create_array)(n, 0, 0)) : static_cast<BYTE *>(create(c));
                for (int i = 0; i < n; i++) {
                    BYTE *e = o + i * c.size;
                    switch (k) {
                    case K_OBJECTIVE:
                    case K_COUNT:
                    case K_SERIES:
                        zs_set(e + 8, PICK(TAGS));
                        zs_set(e + 0x10, PICK(DESCRIPTIONS));
                        zs_set(e + 0x14, PICK(STATUSES));
                        break;
                    case K_MISSION:
                        zs_set(e + 0x10, PICK(DESCRIPTIONS));
                        for (DWORD j = random() % 3; j; j--)
                            array_add(e + 0x14, make_objective((int)(random() % 3)));
                        break;
                    case K_MANAGER:
                        for (DWORD j = random() % 3; j; j--)
                            array_add(e + 8, make_mission());
                        break;
                    case K_DEFINE: {
                        const DWORD m = random() % 3;
                        if (m) {
                            DWORD *block = static_cast<DWORD *>(heap_alloc(4 + 4 * m));
                            block[0] = m;
                            for (DWORD j = 0; j < m; j++) {
                                zs_ctor(block + 1 + j);
                                zs_set(block + 1 + j, LAYER_NAMES[random() % NLAYER]);
                            }
                            dw(e, 0x10) = ptrv(block + 1);
                            dw(e, 0x14) = m;
                            dw(e, 0x18) = m;
                        }
                        break;
                    }
                    case K_SELECT:
                    case K_INCREMENT:
                    case K_DELETE:
                    case K_IS_COMPLETE:
                    case K_IS_DEFINED:
                    case K_UPDATE:
                        zs_set(e + 8, PICK(TAGS));
                        break;
                    case K_FAILED:
                    case K_COMPLETED:
                        zs_set(e + 0xc, PICK(STATUSES));
                        break;
                    default:
                        break;
                    }
                }
                BYTE *r = static_cast<BYTE *>(destroy(o, flags));
                const DWORD base = flags & 2 ? ptrv(o) - 4 : ptrv(o);
                eax[side] = ptrv(r) - base;
                if (!(flags & 1)) {
                    // what the destructor writes: the vtables and the arrays (data, count, capacity)
                    DWORD keep[12], nk = 0;
                    keep[nk++] = dwc(o, 0);
                    // the mission's two pointer arrays, the manager's, the definition's names
                    const DWORD from = k == K_MISSION ? 0x14 : k == K_MANAGER ? 8 : k == K_DEFINE ? 0xc : 0;
                    const DWORD to = k == K_MISSION ? 0x40 : k == K_MANAGER ? 0x18 : k == K_DEFINE ? 0x1c : 0;
                    for (DWORD off = from; off < to; off += 4)
                        if (!(k == K_MISSION && off >= 0x24 && off < 0x30))
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
DWORD desc_size(DWORD vtable)
{
    switch (vtable) {
    case VT_STRING: return 0x34;
    case VT_OBJECT: return 0x3c;
    case VT_OBJECT_ARRAY: return 0x48;
    case VT_ENUM_INT: return 0x44;
    case VT_FLOAT: return 0x44;
    }
    return 0x34;   // the LocalEntry kind (0x415ea0)
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
// kind's byte +0x28 (a heap block's old contents). class_a/class_b: the class each side registered into
// (an object field of the class itself, "Old Objective", points to it).
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
        DWORD x = dwc(a, off), y = dwc(b, off);
        if ((off == 0x28 && a->vftable == VT_STRING) || (off == 0x44 && a->vftable == VT_OBJECT_ARRAY))
            x &= 0xff, y &= 0xff;
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
    if (f->vftable == VT_STRING && heap_pointer(d[11]) && readable(at<void *>(d[11]), 0x40))
        log("      +0x2c: '%.40s'", at<const ZStringBuffer *>(d[11])->text);
}

constexpr DWORD MAXF = 8;
struct Made {
    CFieldDesc *fields[MAXF];
    DWORD n, result, alt_name, registered, par_stat, cls;
};
void run_register_fields(const MissionClassDef &c, bool var_zero, Made &out)
{
    CClassInfo *&var = *at<CClassInfo **>(c.var);
    CClassInfo *const saved = var;
    const DWORD saved_par = dw(at<void *>(MISSION_PAR_STAT_FIELD), 0);
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
    out.alt_name = ptrv(ci->alt_name);
    out.registered = (DWORD)g_nregister;
    for (DWORD i = 0; i < MAXF; i++)
        out.fields[i] = i < ci->num_fields ? ci->fields[i] : nullptr;
    const DWORD last = ci->num_fields ? ptrv(ci->fields[ci->num_fields - 1]) : 0;
    out.result = r == ptrv(ci) ? 'C' : last && r >= last && r < last + 0x48 ? 0xd0000000u + (r - last) : r;
    out.par_stat = dw(at<void *>(MISSION_PAR_STAT_FIELD), 0);
    if (out.par_stat == saved_par)
        out.par_stat = 0;
    if (ci->fields)
        heap_free(ci->fields);
    if (var_zero)
        pool_free(ci, sizeof(CClassInfo));
    var = saved;
    dw(at<void *>(MISSION_PAR_STAT_FIELD), 0) = saved_par;
}
// The descriptors made (the element descriptor kept in 0x6cfd84 is owned by "Par Stat Data List" and
// goes with it, 0x4191f0: deleting it a second time corrupted the heap)
void free_made(Made &m)
{
    for (DWORD i = 0; i < m.n && i < MAXF; i++)
        if (m.fields[i])
            vcall<void *>(m.fields[i], 0, 1);
}

void test_classes(Tally &t, Tally &real)
{
    for (const MissionClassDef &g : MISSION_CLASSES) {
        CClassInfo *&var = *at<CClassInfo **>(g.var);
        CClassInfo *const saved = var;
        RegisterCall calls[2];
        DWORD results[2], set[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            set[side] = ptrv(at<Getter>(g.getter)());
            var = nullptr;
            g_nregister = 0;
            memset(&g_register, 0, sizeof g_register);
            CodePatch p;
            patch_jump(p, 0x415780, (void *)&fake_register);
            void *made = at<Getter>(g.getter)();
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
        wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, size %x / %x, version %x / %x]", g.getter, set[0], set[1], results[0],
                  results[1], calls[0].size, calls[1].size, calls[0].version, calls[1].version);
        t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                    memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
                g.name, d);
    }
    for (const MissionClassDef &c : MISSION_CLASSES) {
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
            bool same = m[0].n == m[1].n && m[0].result == m[1].result && m[0].alt_name == m[1].alt_name &&
                        m[0].registered == m[1].registered && memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0 &&
                        !m[0].par_stat == !m[1].par_stat &&
                        (!m[0].par_stat || same_desc(at<CFieldDesc *>(m[0].par_stat), at<CFieldDesc *>(m[1].par_stat)));
            for (DWORD i = 0; i < m[0].n && i < MAXF && same; i++)
                same = same_desc(m[0].fields[i], m[1].fields[i], 0, m[0].cls, m[1].cls);
            char d[160];
            wsprintfA(d, "[RegisterFields %s var %d: %u / %u fields, eax %x / %x, alt name %x / %x, registered %u / %u]", c.name,
                      var_zero, m[0].n, m[1].n, m[0].result, m[1].result, m[0].alt_name, m[1].alt_name, m[0].registered, m[1].registered);
            if (!same && t.mismatches < 3)
                for (DWORD i = 0; i < m[0].n && i < MAXF; i++) {
                    log_desc("original", m[0].fields[i]);
                    log_desc("reimpl", m[1].fields[i]);
                }
            t.check(same, c.name, d);
            // Real data: the class's own descriptors as registered at startup, and its alternative name
            const CClassInfo *ci = *at<CClassInfo **>(c.var);
            bool same_real = ci && ci->num_fields - ci->num_inherited == m[0].n && ptrv(ci->alt_name) == m[0].alt_name;
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
    // The registered class infos: size, Create, CreateArray, the alternative name of the failed screen
    for (const MissionClassDef &c : MISSION_CLASSES) {
        const CClassInfo *ci = *at<CClassInfo **>(c.var);
        char d[160];
        wsprintfA(d, "[%s: class info %x, size %x, create %x, create array %x]", c.name, ptrv(ci), ci ? ci->size : 0,
                  ci ? ptrv((void *)ci->create) : 0, ci ? ptrv((void *)ci->create_array) : 0);
        real.check(ci && ci->size == c.size && ptrv((void *)ci->create) == c.create && ptrv((void *)ci->create_array) == c.create_array &&
                       !lstrcmpA(ci->name, c.name),
                   c.name, d);
    }
}

// ---- Behaviour in the fake world ------------------------------------------------------------------------

// The functions called directly (thiscall)
using Slot0 = DWORD(THISCALL *)(void *);
using Slot1 = DWORD(THISCALL *)(void *, DWORD);
using Exec = DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD);

BYTE *nth_mission(BYTE *manager, DWORD i)
{
    return dwc(manager, 0x10) ? reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dwc(manager, 0xc)))[i % dwc(manager, 0x10)] : nullptr;
}
BYTE *any_objective(BYTE *manager)
{
    for (DWORD i = 0; i < dwc(manager, 0x10); i++) {
        BYTE *m = nth_mission(manager, i + random());
        if (m && dwc(m, 0x1c))
            return reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dwc(m, 0x18)))[random() % dwc(m, 0x1c)];
    }
    return nullptr;
}
// A tag for an action: one of the manager's objectives' or missions' tags, or one of the list
const char *tag_for(BYTE *manager, bool mission)
{
    if (random() % 3) {
        BYTE *o = mission ? nth_mission(manager, random()) : any_objective(manager);
        if (o)
            return static_cast<const ZString *>(static_cast<void *>(o + 8))->c_str();
    }
    return PICK(TAGS);
}

// An action of the module, filled from the manager (the tags it uses)
BYTE *make_action(int kind, BYTE *manager)
{
    BYTE *a = static_cast<BYTE *>(create(MISSION_CLASSES[kind]));
    switch (kind) {
    case K_DEFINE:
        dw(a, 8) = random() % 6 ? ptrv(make_mission()) : 0;
        if (dwc(a, 8) && random() % 3 == 0 && dwc(manager, 0x10))
            zs_set(reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dwc(a, 8))) + 8, tag_for(manager, true));
        if (DWORD m = random() % 4) {
            DWORD *block = static_cast<DWORD *>(heap_alloc(4 + 4 * m));
            block[0] = m;
            for (DWORD j = 0; j < m; j++) {
                zs_ctor(block + 1 + j);
                zs_set(block + 1 + j, LAYER_NAMES[random() % NLAYER]);
            }
            dw(a, 0x10) = ptrv(block + 1);
            dw(a, 0x14) = m;
            dw(a, 0x18) = m;
        }
        break;
    case K_SELECT:
        zs_set(a + 8, tag_for(manager, true));
        break;
    case K_ADD:
        dw(a, 8) = ptrv(make_objective((int)(random() % 3)));
        if (random() % 2)
            zs_set(reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dwc(a, 8))) + 8, tag_for(manager, false));
        break;
    case K_UPDATE:
        zs_set(a + 8, tag_for(manager, false));
        zs_set(a + 0xc, PICK(STATUSES));
        zs_set(a + 0x10, PICK(STATUSES));
        break;
    case K_INCREMENT:
    case K_DELETE:
    case K_IS_COMPLETE:
    case K_IS_DEFINED:
        zs_set(a + 8, tag_for(manager, false));
        break;
    case K_FAILED:
    case K_COMPLETED:
        zs_set(a + 0xc, PICK(STATUSES));
        break;
    default:
        break;
    }
    return a;
}

// The functions under test (index into this list = the case's function)
enum Fn { F_COMPLETE, F_FAILED, F_INCREMENT, F_SET_STATUS, F_COUNT_STATUS, F_ADD_OBJECTIVE, F_NUMBER, F_FORGET_OLD, F_OLD_COUNT,
          F_PAR_STAT, F_CURRENT, F_FIND, F_EXECUTE, F_SLOT18, NFN };
const char *const FN_NAMES[NFN] = {"objective slot 24", "objective slot 25", "objective slot 23", "0x510e20 SetStatus",
                                   "0x5114d0 count status", "0x512170 AddObjective", "0x512280 Number", "0x5122f0 ForgetOld",
                                   "0x512330 OldCount", "0x512350 ParStatData", "0x5127e0 Current", "0x512800 Find",
                                   "actions slot 23", "CSelectNoMission slot 18"};

// One case of a function on the side's own manager (built from the seed unless a manager is given:
// then a copy of it)
void behaviour_case(int fn, int action_kind, BYTE *given, Result &r)
{
    BYTE *manager = given ? static_cast<BYTE *>(clone(given)) : make_manager();
    if (given)
        strip_actions(manager);
    setup_fakes(manager);
    BYTE *extra = nullptr;   // a second object the case uses (compared and deleted afterwards)
    switch (fn) {
    case F_COMPLETE:
    case F_FAILED:
    case F_INCREMENT: {
        BYTE *o = any_objective(manager);
        if (!o) {
            extra = o = make_objective((int)(random() % 3));
        }
        r.eax = vcall<DWORD>(o, fn == F_COMPLETE ? 24 : fn == F_FAILED ? 25 : 23);
        if (fn != F_INCREMENT)
            r.eax &= 0xff;   // al (the callers test al)
        else
            r.eax = 0;   // void in use, except the base's reporter result
        break;
    }
    case F_SET_STATUS: {
        BYTE *o = any_objective(manager);
        if (!o)
            extra = o = make_objective((int)(random() % 3));
        if (random() % 4 == 0 && !dwc(o, 0x20))
            dw(o, 0x20) = ptrv(make_objective(K_OBJECTIVE));
        LocalEntry status;
        zs_ctor(&status.key);
        zs_ctor(&status.text);
        zs_set(&status.key, PICK(STATUSES));
        zs_set(&status.text, PICK(STATUSES));
        at<void(THISCALL *)(void *, const LocalEntry *, BYTE)>(0x510e20)(o, &status, (BYTE)(random() % 3 ? 1 : 0x10 * (random() % 2)));
        zs_dtor(&status.text);
        zs_dtor(&status.key);
        break;
    }
    case F_COUNT_STATUS: {
        extra = make_objective(K_COUNT);
        at<void(THISCALL *)(void *)>(0x5114d0)(extra);
        break;
    }
    case F_ADD_OBJECTIVE: {
        BYTE *m = nth_mission(manager, random());
        if (!m)
            extra = m = make_mission();
        BYTE *o = make_objective((int)(random() % 3));
        if (random() % 2)
            zs_set(o + 8, tag_for(manager, false));
        r.eax = at<BYTE(THISCALL *)(void *, void *)>(0x512170)(m, o);
        break;
    }
    case F_NUMBER:
    case F_FORGET_OLD:
    case F_OLD_COUNT:
    case F_PAR_STAT: {
        BYTE *m = nth_mission(manager, random());
        if (!m)
            extra = m = make_mission();
        if (fn == F_NUMBER)
            r.eax = at<Slot0>(0x512280)(m);
        else if (fn == F_FORGET_OLD) {
            for (DWORD i = 0; i < dwc(m, 0x1c); i++) {
                BYTE *o = reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dwc(m, 0x18)))[i];
                if (random() % 2 && !dwc(o, 0x20))
                    dw(o, 0x20) = ptrv(make_objective(K_OBJECTIVE));
            }
            at<Slot0>(0x5122f0)(m);
        } else if (fn == F_OLD_COUNT)
            r.eax = at<Slot0>(0x512330)(m);
        else {
            DWORD list[4] = {random(), random(), random(), random()};
            const DWORD saved = dwc(m, 0x34);
            dw(m, 0x34) = ptrv(list);
            r.eax = at<Slot1>(0x512350)(m, random() % 4);
            dw(m, 0x34) = saved;
        }
        break;
    }
    case F_CURRENT:
        r.eax = id(at<void *(THISCALL *)(void *)>(0x5127e0)(manager));
        break;
    case F_FIND:
        r.eax = id(at<void *(THISCALL *)(void *, const char *)>(0x512800)(manager, tag_for(manager, true)));
        break;
    case F_EXECUTE: {
        extra = make_action(action_kind, manager);
        if (action_kind == K_FAILED)
            g_f.game_type = g_f.game;   // the failed screen calls the game type without a null check
        const DWORD a2 = random() % 7;
        r.eax = at<Exec>(dwc(at<void *>(dwc(extra, 0)), 23 * 4))(extra, g_f.layers[0], a2, random() % 3, 0) & (action_kind == K_IS_COMPLETE ? 0xffffffff : 0xff);
        break;
    }
    case F_SLOT18:
        extra = make_action(K_SELECT_NO, manager);
        r.eax = at<Slot1>(0x5134c0)(extra, 0);
        break;
    }
    r.state = state_hash(manager, extra);
    destroy(manager, 1);
    if (extra)
        destroy(extra, 1);
}

void test_behaviour(Tally &t)
{
    for (int fn = 0; fn < NFN; fn++) {
        if (fn == F_EXECUTE) {
            for (int k = K_DEFINE; k < NKINDS; k++)
                for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++)
                    two_sides(t, MISSION_CLASSES[k].name, seed * 31 + (unsigned)k, [&](Result &r) { behaviour_case(fn, k, nullptr, r); });
            continue;
        }
        for (unsigned seed = 1; seed <= (unsigned)scaled(80); seed++)
            two_sides(t, FN_NAMES[fn], seed * 17 + (unsigned)fn, [&](Result &r) { behaviour_case(fn, 0, nullptr, r); });
    }
}

// ---- Real data: the module's objects in the running level ----------------------------------------------


// Objects reached from the level's parts (pointers to objects and pointer arrays of objects, eight
// levels deep), each once; the module's objects kept in the order found
constexpr int MAX_SEEN = 16384, MAX_FOUND = 2048;
DWORD g_seen[MAX_SEEN];
int g_nseen, g_objects;
BYTE *g_found[MAX_FOUND];
int g_nfound, g_count[NKINDS];
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
    const int k = kind_of(o);
    if (k >= 0) {
        g_count[k]++;
        if (g_nfound < MAX_FOUND)
            g_found[g_nfound++] = o;
    }
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

// Copies of the level's objects on both sides, compared; then the level's mission definitions (copies)
// run in order into an empty manager, then every other action (a copy) in the order found, each
// compared; the objective slots on copies of the objectives the definitions brought
void test_real_objects(bool found, Tally &copies, Tally &actions)
{
    if (!found) {
        log("  missions real data: no level grid found");
        return;
    }
    int classes = 0;
    for (int k = 0; k < NKINDS; k++)
        if (g_count[k]) {
            classes++;
            log("    %s: %d in the level", MISSION_CLASSES[k].name, g_count[k]);
        }
    log("  missions real data: %d objects reached from the level's parts, %d of this module (%d classes)", g_objects, g_nfound,
        classes);
    for (int i = 0; i < g_nfound; i++) {
        const int k = kind_of(g_found[i]);
        // the level's object has the vtable Create gives
        BYTE *o = static_cast<BYTE *>(create(MISSION_CLASSES[k]));
        const DWORD vt = dwc(o, 0);
        destroy(o, 1);
        char d[120];
        wsprintfA(d, "[%s: level vtable %x, Create %x]", MISSION_CLASSES[k].name, dwc(g_found[i], 0), vt);
        copies.check(vt == dwc(g_found[i], 0), MISSION_CLASSES[k].name, d);
        // a copy through the class's Create and the descriptors, on both sides
        two_sides(copies, MISSION_CLASSES[k].name, (unsigned)i, [&](Result &r) {
            BYTE *c = static_cast<BYTE *>(clone(g_found[i]));
            r.state = hash_obj(2166136261u, c, 0);
            r.eax = hash_obj(2166136261u, g_found[i], 0);
            destroy(c, 1);
        });
    }
    // The level's actions in the fake world, in the order found, on a manager the definitions filled.
    // Round 0 starts empty; from round 1 a current mission is there whose count objectives carry the
    // tags the level's actions name (a level started directly has no mission defined), from round 3
    // as many times as the tags occur, with their statuses complete in round 4.
    g_log_all = true;
    for (unsigned round = 0; round < 5; round++)
        two_sides(actions, "level actions", round, [&](Result &r) {
            BYTE *manager = static_cast<BYTE *>(create(MISSION_CLASSES[K_MANAGER]));
            setup_fakes(manager);
            g_f.game_type = g_f.game;   // the failed screen calls the game type without a null check
            if (round) {
                BYTE *mission = static_cast<BYTE *>(create(MISSION_CLASSES[K_MISSION]));
                for (int i = 0; i < g_nfound; i++) {
                    const int k = kind_of(g_found[i]);
                    if (k != K_UPDATE && k != K_INCREMENT && k != K_DELETE && k != K_IS_COMPLETE && k != K_IS_DEFINED)
                        continue;
                    BYTE *o = static_cast<BYTE *>(create(MISSION_CLASSES[K_COUNT]));
                    zs_set(o + 8, static_cast<const ZString *>(static_cast<void *>(g_found[i] + 8))->c_str());
                    zs_set(o + 0x14, round == 4 ? "Complete" : "Incomplete");
                    dw(o, 0x24) = round;
                    dw(o, 0x28) = 0;   // "Current Count": no constructor writes it (pool contents)
                    vcall<DWORD>(mission + 0x14, 2, &o);
                    if (round < 3)
                        break;
                }
                vcall<DWORD>(manager + 8, 2, &mission);
                dw(manager, 0x18) = 0;
            }
            unsigned h = 2166136261u;
            for (int pass = 0; pass < 2; pass++)
                for (int i = 0; i < g_nfound; i++) {
                    const int k = kind_of(g_found[i]);
                    if (k < K_DEFINE || (pass == 0) != (k == K_DEFINE) || (round == 0 && k == K_DELETE) || (k == K_ADD && !dwc(g_found[i], 8)))
                        continue;   // (an add action without its objective would copy null)
                    BYTE *a = static_cast<BYTE *>(clone(g_found[i]));
                    strip_actions(a);
                    const DWORD eax = at<Exec>(dwc(at<void *>(dwc(a, 0)), 23 * 4))(a, g_f.layers[0], round, 0, 0);
                    const DWORD result = k == K_IS_COMPLETE ? eax : eax & 0xff;
                    h = fnv(h, &result, 4);
                    destroy(a, 1);
                    ev('ACT', (DWORD)k, result);
                    for (DWORD m = 0; m < dwc(manager, 0x10); m++) {   // the objectives field by field
                        BYTE *mission = nth_mission(manager, m);
                        for (DWORD j = 0; j < dwc(mission, 0x1c); j++) {
                            BYTE *o = reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dwc(mission, 0x18)))[j];
                            ev('OBJ', zs_hash(o + 8), zs_hash(o + 0x14), zs_hash(o + 0x18));
                            // +0x24 and +0x28 only where the objective has them: CMissionObjective is 0x24 bytes,
                            // CSeriesMissionObjective 0x28, CCountMissionObjective 0x2c (docs/classes.csv); beyond its end
                            // lies the next pool block, whose stale contents differ between the sides (ACT 3/M17/17 Tech
                            // Main and ACT 4/M22/22 Ruined Main add plain objectives by their actions)
                            const int ko = kind_of(o);
                            ev('OBJn', ko == K_COUNT || ko == K_SERIES ? dwc(o, 0x24) : 0, ko == K_COUNT ? dwc(o, 0x28) : 0,
                               hash_obj(2166136261u, at<BYTE *>(dwc(o, 0x20)), 0));
                            ev('OBJd', zs_hash(o + 0xc), zs_hash(o + 0x10), hash_obj(2166136261u, at<BYTE *>(dwc(o, 0x1c)), 0));
                        }
                    }
                }
            // the objective slots on every objective the manager has now (copies)
            for (DWORD m = 0; m < dwc(manager, 0x10); m++) {
                BYTE *mission = nth_mission(manager, m);
                const DWORD number = at<Slot0>(0x512280)(mission);
                h = fnv(h, &number, 4);
                for (DWORD j = 0; j < dwc(mission, 0x1c); j++) {
                    BYTE *o = static_cast<BYTE *>(clone(reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dwc(mission, 0x18)))[j]));
                    const DWORD res[2] = {vcall<DWORD>(o, 24) & 0xff, vcall<DWORD>(o, 25) & 0xff};
                    h = fnv(h, res, 8);
                    if (kind_of(o) == K_COUNT)
                        vcall<DWORD>(o, 23);
                    h = hash_obj(h, o, 0);
                    destroy(o, 1);
                }
            }
            r.eax = h;
            r.state = state_hash(manager);
            destroy(manager, 1);
        });
}

// The running world's own mission manager (taken before fakes_in replaced the world): a copy as the
// fixture of every behaviour case
void test_real_manager(Tally &t, BYTE *manager)
{
    if (!manager || kind_of(manager) != K_MANAGER) {
        log("  missions real data: the world has no mission manager (%x)", ptrv(manager));
        return;
    }
    log("  missions real data: the world's mission manager has %u missions, current %d", dwc(manager, 0x10), (int)dwc(manager, 0x18));
    for (int fn = 0; fn < NFN; fn++) {
        if (fn == F_EXECUTE) {
            for (int k = K_DEFINE; k < NKINDS; k++)
                for (unsigned seed = 1; seed <= 4; seed++)
                    two_sides(t, MISSION_CLASSES[k].name, seed * 37 + (unsigned)k, [&](Result &r) { behaviour_case(fn, k, manager, r); });
            continue;
        }
        for (unsigned seed = 1; seed <= 8; seed++)
            two_sides(t, FN_NAMES[fn], seed * 19 + (unsigned)fn, [&](Result &r) { behaviour_case(fn, 0, manager, r); });
    }
}

}  // namespace

ZAX_CHECKS(missions)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally creates{"missions: Create of 17 classes"}, arrays{"missions: CreateArray of 17 classes"},
        dtors{"missions: deleting destructors"}, classes{"missions: class getters and RegisterFields"},
        registered{"missions: registered fields and class infos (real data)"}, behaviour{"missions: slots and helpers in a fake world"},
        level{"missions: the level's mission objects (real data)"}, level_actions{"missions: the level's actions in a fake world (real data)"},
        world{"missions: the world's mission manager (real data)"};
    test_objects(creates, arrays, dtors);
    creates.report();
    arrays.report();
    dtors.report();
    test_classes(classes, registered);
    classes.report();
    registered.report();
    // the level's objects and the world's manager are found before the world pointer is replaced
    const bool found = find_level_objects();
    BYTE *real_world = *at<BYTE **>(0x6c2080);
    BYTE *real_manager = real_world ? vcall<BYTE *>(real_world, 98) : nullptr;
    fakes_in();
    test_behaviour(behaviour);
    behaviour.report();
    test_real_objects(found, level, level_actions);
    test_real_manager(world, real_manager);
    fakes_out();
    level.report();
    level_actions.report();
    world.report();
    log("  missions: %u ms", GetTickCount() - start);
}

}  // namespace zax
