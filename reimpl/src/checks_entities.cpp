// Checks of the entity classes (entities.cpp) against the original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls run the original too), side 1 through the
// reimplementation. Compared: eax (masked where the original leaves undefined bits; st(0) of the float
// functions as 10 bytes), an event log and the state of every fake afterwards. Pointers are compared
// raw first and only mapped (fakes by number and offset, the stack as one tag) when they differ.
//  - Slot scenarios on fake entities: objects of 0xe4 bytes (the CEntityAnimated layout) whose vtable is
//    a table of recorder thunks (every slot logs its arguments and returns a configured or hashed
//    value), fake AIs (the layer part's activity and the models' activity), fake models in fake records
//    of the model list (0x6cfe08/0x6cfe0c swapped, the list's vtable 0x6cfdd8 a recorder table for
//    loading), fake rotation sequences, fake death types, a fake world (0x6c2080), error reporter
//    (0x6c02cc), layer, net layer and player; the exe functions that reach further (the layer, the
//    listeners, the dynamic values, the network packet, the behaviour stack, the actions, the damage
//    queries, ...) jump to recorders. All 121 functions of the module except the getters, RegisterFields,
//    constructors and destructors run on random configurations.
//  - Objects: Create, CreateArray (heap blocks from an arena filled with a pattern) and the deleting
//    destructors of the three classes, the out-of-line constructors on pool blocks filled with a
//    pattern, the destructors of animated entities with a creator and attached objects.
//  - Class getters and RegisterFields (ClassInfo_Register replaced by a stand-in; descriptors from the
//    arena compared byte by byte, with the descriptors registered at startup).
//  - Real data: the entities of the running level (its layer grid and their children): the read-only
//    functions, and the multiplayer blocks (slot 25) written into recorders.
#include "check_util.h"
#include "entities.h"
#include "models.h"
#include "field_desc.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT DWORD WINAPI GetCurrentThreadId(void);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);

namespace zax {

int hooks_entities(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Memory helpers (as in checks_geometry.cpp) -----------------------------------------------------

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
    // through the trampoline when the getter is hooked
    const DWORD var = *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(original_address(getter)) + 1);
    if (var < 0x600000 || var >= 0x720000 || !readable(at<void *>(var), 4))
        return nullptr;
    return *at<CClassInfo **>(var);
}

bool is_kind_real(const void *o, CClassInfo *c) { return (at<DWORD(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c) & 0xff) != 0; }
float as_float_v(DWORD v)
{
    float f;
    memcpy(&f, &v, 4);
    return f;
}

// ---- Sides, event log and state ----------------------------------------------------------------------

int g_side;
unsigned g_loghash[2], g_statehash[2];
int g_nlog[2];
constexpr int LOG = 512;
DWORD g_log[2][LOG][4];
DWORD g_stack_top;
DWORD g_calls;

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
    if (g_nlog[0] != g_nlog[1])
        log("    log lengths %d / %d", g_nlog[0], g_nlog[1]);
}
void st(DWORD v) { g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4); }
void st_bytes(const void *p, DWORD n) { g_statehash[g_side] = fnv(g_statehash[g_side], p, n); }
void st_ld(long double v)
{
    BYTE b[10];
    memcpy(b, &v, 10);
    st_bytes(b, 10);
}
unsigned text_hash(const char *s) { return fnv_text(2166136261u, s ? s : "(null)"); }
DWORD g_dump[2][64];
int g_ndump[2];
void dump(DWORD v)
{
    if (g_ndump[g_side] < 64)
        g_dump[g_side][g_ndump[g_side]++] = v;
}

// ---- Hooks of the module out and in --------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 160;
SavedSite g_sites[MAX_SITES];
Hook g_hook_list[MAX_SITES];
int g_nsites;
void hooks_out()
{
    g_nsites = 0;
    const int n = hooks_entities(nullptr);
    if (!hook_sites_fit("entities", n, MAX_SITES))
        return;
    hooks_entities(g_hook_list);
    for (int i = 0; i < n && g_nsites < MAX_SITES; i++) {
        BYTE *site = at<BYTE *>(g_hook_list[i].address);
        if (site[0] != 0xe9)
            continue;   // not installed (Disable=, or bytes did not match)
        const DWORD target = g_hook_list[i].address + 5 + *reinterpret_cast<INT32 *>(site + 1);
        if (target != ptrv(g_hook_list[i].replacement))
            continue;
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

DWORD random_float()
{
    constexpr DWORD VALUES[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x3f000000, 0xbf000000, 0x3fc00000, 0x40200000,
                                0xc0200000, 0x7fc00000, 0x7f800000, 0xff800000, 0x00000001, 0x7f7fffff, 0x4f000000,
                                0xcf000000, 0x447a0000, 0x40490fdb, 0xc0490fdb, 0x40c90fdb, 0x40c90fda, 0x3f7fbe77,
                                0x41200000, 0xc1200000, 0x41200001, 0x42c80000, 0x3c23d70a, 0x43fa0000};
    if (random() % 3)
        return VALUES[random() % (sizeof VALUES / 4)];
    const float f = (float)((int)(random() % 8000) - 2000) / ((random() & 1) ? 1.0f : 64.0f);
    return bits(f);
}
DWORD random_angle()
{
    if (random() % 4 == 0)
        return random_float();
    return bits((float)((int)(random() % 2000) - 600) / 200.0f);
}

// ---- Fakes and the recorder thunks -----------------------------------------------------------------------

enum { T_ENT, T_AI, T_SEQ, T_WORLD, T_OTHER, T_LIST, NTABLES };
constexpr int NSLOTS = 128;
constexpr int NENT = 8, NAI = 12, NSEQ = 6, NMODEL = 4, NREC = 6, NOBJ = 8, NDEATH = 4;

struct FakeEnt {
    BYTE o[0xe4];     // the object (CEntityAnimated's layout)
    DWORD cls;        // slot 2: index into g_classes
    DWORD creator;    // slot 68: an index + 1 (a lower fake) or 0
    DWORD k26, k52, k53, k54, k56;   // the int slots
    DWORD f46, f84;   // the float slots
    BYTE upper;       // slot 102
    BYTE dyn;         // FieldDesc_IsDynamicIn
    BYTE pad[2];
    int updates;      // IntField_Get
    float cx, cy;     // LayerPart_Position
    DWORD deleted;
};
struct FakeAI {
    DWORD vftable;
    DWORD dynamic;
    BYTE done;
    BYTE pad[3];
    DWORD more[5];
    DWORD cls;
    DWORD deleted;
};
struct FakeSeq {
    BYTE s[0x70];
    DWORD k25;   // slot 25
    DWORD f26;   // slot 26
};
struct FakeModel {
    BYTE m[0xc4];
    void *activity[4];
    BYTE *sequences[128];
};
struct FakeObj {
    DWORD vftable;
    DWORD dynamic;
    DWORD more[6];
    DWORD cls;
    DWORD deleted;
};
struct FakeChildList {
    LPChildList l;
    LayerPart *items[6];
};

FakeEnt g_ents[NENT];
FakeAI g_ais[NAI];
FakeSeq g_seqs[NSEQ];
FakeModel g_models[NMODEL];
FakeObj g_objs[NOBJ];
FakeObj g_world, g_net, g_reporter;
struct FakeTex {
    DWORD vftable;
    BYTE b[0x4c];
    DWORD cls;
};
FakeTex g_tex;
DWORD &member_dword(FakeTex &t, DWORD off) { return *reinterpret_cast<DWORD *>(reinterpret_cast<BYTE *>(&t) + off); }
FakeChildList g_children[3];
BYTE g_records[NREC][0x80];
BYTE g_death_records[NDEATH][0x80];
DWORD g_death_objects[NDEATH][4];
alignas(4) BYTE g_layer[0x100];
alignas(4) BYTE g_player[0x100];
alignas(4) BYTE g_difficulty[0x20];
alignas(4) BYTE g_hue[0x20];
alignas(4) BYTE g_packet[0x40];
void *g_activity[NENT][6];
void *g_attached[NENT][4];
ZString g_names[6];
bool g_names_made;
DWORD g_first_char;   // FirstCharacter's answer: an entity index + 1 or 0
DWORD g_texture_on;
DWORD g_rnext;        // the packet reads' stream
float g_buf[16];

extern "C" {
extern char en_thunks[];
DWORD en_pop, en_ret_float, en_fret;
DWORD __cdecl en_record(DWORD *args, DWORD id, BYTE *self);
}
__asm__(".text\n"
        ".globl _en_thunks\n"
        "_en_thunks:\n"
        ".set en_i, 0\n"
        ".rept 768\n"
        "  movl $en_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _en_common - . - 4\n"
        ".set en_i, en_i + 1\n"
        ".endr\n"
        "_en_common:\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _en_record\n"
        "  addl $12, %esp\n"
        "  cmpl $0, _en_ret_float\n"
        "  je 1f\n"
        "  flds _en_fret\n"
        "1:\n"
        "  movl _en_pop, %ecx\n"
        "  popl %edx\n"
        "  addl %ecx, %esp\n"
        "  jmp *%edx\n");
DWORD g_vt[NTABLES][NSLOTS];

struct Spec {
    BYTE slot, nargs;
    const char *kinds;   // V value (tagged), F float bits, X text, M message (3 dwords), D the dword pointed to
    char ret;
};
// Returns: i hash, k the fake's int slot, a/b the fake's float slots (46/84), c class, e creator, u upper
// body byte, p 0 or a fake AI, q message answer (sets the flag byte), g float = argument 0 or a value,
// G the same for argument 1, n small count, d delete, w the net layer, r the player, h the difficulty
// factors, s a name text, m a fake model
constexpr Spec ENT_SPECS[] = {{1, 1, "V", 'd'},     {2, 0, "", 'c'},      {37, 3, "RVV", 'y'},      {26, 1, "V", 'k'},   {33, 0, "", 'i'},
                              {34, 0, "", 'i'},     {35, 1, "V", 'i'},    {41, 2, "FF", 'i'},  {46, 0, "", 'a'},
                              {52, 0, "", 'k'},     {53, 0, "", 'k'},     {54, 0, "", 'k'},    {56, 0, "", 'k'},
                              {57, 1, "V", 'i'},    {58, 1, "V", 'i'},    {59, 1, "V", 'i'},   {63, 1, "M", 'q'},
                              {64, 2, "VV", 'p'},   {68, 0, "", 'e'},     {69, 1, "V", 'i'},   {71, 1, "V", 'i'},
                              {78, 0, "", 'i'},     {84, 0, "", 'b'},     {93, 1, "V", 'i'},   {97, 0, "", 'i'},
                              {99, 3, "FVV", 'i'},  {100, 2, "FV", 'g'},  {101, 0, "", 'i'},   {102, 0, "", 'u'},
                              {103, 2, "FV", 'i'},  {104, 2, "FV", 'i'}};
constexpr Spec AI_SPECS[] = {{1, 1, "V", 'd'},       {2, 0, "", 'c'},       {24, 1, "V", 'i'},    {25, 2, "VV", 'i'},
                             {29, 2, "VM", 'q'},     {30, 3, "VFV", 'G'},   {31, 3, "VVV", 'i'},  {33, 3, "VVV", 'i'},
                             {34, 5, "VVVVV", 'i'},  {41, 2, "VV", 'i'}};
constexpr Spec SEQ_SPECS[] = {{2, 0, "", 'c'}, {23, 1, "V", 'i'}, {25, 0, "", 'k'}, {26, 0, "", 'a'}};
constexpr Spec WORLD_SPECS[] = {{17, 1, "X", 'i'}, {44, 0, "", 'w'},     {58, 0, "", 'h'},  {90, 1, "V", 'i'},
                                {91, 2, "VV", 'i'}, {92, 3, "VVV", 'i'}, {93, 1, "F", 'i'}, {94, 1, "F", 'i'}};
constexpr Spec OTHER_SPECS[] = {{1, 1, "V", 'd'}, {2, 1, "D", 'i'}, {16, 1, "X", 'i'}, {23, 2, "VV", 'i'}, {29, 0, "", 'r'}};
constexpr Spec LIST_SPECS[] = {{1, 1, "V", 's'}, {22, 0, "", 'l'}, {37, 3, "XXV", 'm'}};

const Spec *find_spec(DWORD table, DWORD slot)
{
    const Spec *s = nullptr;
    int n = 0;
    switch (table) {
    case T_ENT: s = ENT_SPECS, n = sizeof ENT_SPECS / sizeof *s; break;
    case T_AI: s = AI_SPECS, n = sizeof AI_SPECS / sizeof *s; break;
    case T_SEQ: s = SEQ_SPECS, n = sizeof SEQ_SPECS / sizeof *s; break;
    case T_WORLD: s = WORLD_SPECS, n = sizeof WORLD_SPECS / sizeof *s; break;
    case T_OTHER: s = OTHER_SPECS, n = sizeof OTHER_SPECS / sizeof *s; break;
    default: s = LIST_SPECS, n = sizeof LIST_SPECS / sizeof *s; break;
    }
    for (int i = 0; i < n; i++)
        if (s[i].slot == slot)
            return &s[i];
    return nullptr;
}

DWORD tag(DWORD v)
{
    if (!v)
        return 0;
    auto in = [v](const void *base, DWORD size) { return v >= ptrv(base) && v < ptrv(base) + size; };
    for (int i = 0; i < NENT; i++)
        if (in(&g_ents[i], sizeof(FakeEnt)))
            return 0xf0000000u + ((DWORD)i << 12) + (v - ptrv(&g_ents[i]));
    for (int i = 0; i < NAI; i++)
        if (in(&g_ais[i], sizeof(FakeAI)))
            return 0xf1000000u + ((DWORD)i << 12) + (v - ptrv(&g_ais[i]));
    for (int i = 0; i < NSEQ; i++)
        if (in(&g_seqs[i], sizeof(FakeSeq)))
            return 0xf2000000u + ((DWORD)i << 12) + (v - ptrv(&g_seqs[i]));
    for (int i = 0; i < NMODEL; i++)
        if (in(&g_models[i], sizeof(FakeModel)))
            return 0xf3000000u + ((DWORD)i << 12) + (v - ptrv(&g_models[i]));
    for (int i = 0; i < NOBJ; i++)
        if (in(&g_objs[i], sizeof(FakeObj)))
            return 0xf4000000u + ((DWORD)i << 12) + (v - ptrv(&g_objs[i]));
    for (int i = 0; i < 3; i++)
        if (in(&g_children[i], sizeof(FakeChildList)))
            return 0xf5000000u + ((DWORD)i << 12) + (v - ptrv(&g_children[i]));
    if (in(g_records, sizeof g_records))
        return 0xf6000000u + (v - ptrv(g_records));
    if (in(g_death_records, sizeof g_death_records))
        return 0xf6100000u + (v - ptrv(g_death_records));
    if (in(g_death_objects, sizeof g_death_objects))
        return 0xf6200000u + (v - ptrv(g_death_objects));
    if (in(g_layer, sizeof g_layer))
        return 0xf7000000u + (v - ptrv(g_layer));
    if (in(g_player, sizeof g_player))
        return 0xf7100000u + (v - ptrv(g_player));
    if (in(g_difficulty, sizeof g_difficulty))
        return 0xf7200000u + (v - ptrv(g_difficulty));
    if (in(g_hue, sizeof g_hue))
        return 0xf7300000u + (v - ptrv(g_hue));
    if (in(g_packet, sizeof g_packet))
        return 0xf7400000u + (v - ptrv(g_packet));
    if (in(g_buf, sizeof g_buf))
        return 0xf7500000u + (v - ptrv(g_buf));
    if (in(&g_world, sizeof g_world))
        return 0xf7600000u;
    if (in(&g_net, sizeof g_net))
        return 0xf7700000u;
    if (in(&g_reporter, sizeof g_reporter))
        return 0xf7800000u;
    if (in(&g_tex, sizeof g_tex))
        return 0xf7900000u;
    if (in(g_names, sizeof g_names))
        return 0xf7a00000u + (v - ptrv(g_names));
    if (v <= g_stack_top && v + 0x10000 > g_stack_top)
        return 0xf8000000u;
    return v;
}

CClassInfo *g_classes[9];
DWORD en_hash(DWORD a, DWORD b, DWORD c)
{
    DWORD h = fnv(fnv(fnv(2166136261u, &a, 4), &b, 4), &c, 4);
    return h ^ (h >> 13);
}
FakeEnt *ent_of(DWORD t) { return (t >> 24) == 0xf0 ? &g_ents[(t >> 12) & 0xfff] : nullptr; }

extern "C" DWORD __cdecl en_record(DWORD *args, DWORD id, BYTE *self)
{
    const DWORD table = id / NSLOTS, slot = id % NSLOTS;
    en_ret_float = 0;
    const Spec *sp = find_spec(table, slot);
    g_calls++;
    const DWORD me = tag(ptrv(self));
    if (!sp) {
        en_pop = 0;
        ev('?', id, me);
        log("  entities: unexpected slot %u of table %u", slot, table);
        return 0;
    }
    en_pop = sp->nargs * 4u;
    DWORD h = 2166136261u;
    for (int i = 0; i < sp->nargs; i++) {
        const DWORD a = args[i];
        DWORD v;
        switch (sp->kinds[i]) {
        case 'F': v = a; break;
        case 'X': v = text_hash(reinterpret_cast<const char *>(a)); break;
        case 'M': {
            const DWORD *m = reinterpret_cast<const DWORD *>(a);
            const DWORD w[3] = {m[0], m[1], m[2] & 0xff};
            v = fnv(2166136261u, w, 12);
            break;
        }
        case 'D': v = tag(*reinterpret_cast<const DWORD *>(a)); break;
        case 'R': v = tag(a); break;
        default: v = tag(a);
        }
        h = fnv(h, &v, 4);
    }
    ev(0x52000000u + id, me, h, sp->nargs ? tag(args[0]) : 0);
    const DWORD r = en_hash(id, me, g_calls);
    for (int i = 0; i < sp->nargs; i++)
        if (sp->kinds[i] == 'R')
            for (int k = 0; k < 4; k++)
                reinterpret_cast<int *>(args[i])[k] = (int)(en_hash(r, (DWORD)k, 1) % 4000) - 1000;
    FakeEnt *fe = table == T_ENT ? ent_of(me) : nullptr;
    FakeAI *fa = (me >> 24) == 0xf1 ? &g_ais[(me >> 12) & 0xfff] : nullptr;
    FakeSeq *fs = (me >> 24) == 0xf2 ? &g_seqs[(me >> 12) & 0xfff] : nullptr;
    FakeObj *fo = (me >> 24) == 0xf4 ? &g_objs[(me >> 12) & 0xfff] : nullptr;
    switch (sp->ret) {
    case 'k':
        if (fs)
            return fs->k25;
        if (!fe)
            return r % 4;
        return slot == 26 ? fe->k26 : slot == 52 ? fe->k52 : slot == 53 ? fe->k53 : slot == 54 ? fe->k54 : fe->k56;
    case 'a':
        en_ret_float = 1;
        en_fret = fs ? fs->f26 : fe ? fe->f46 : random_float();
        return r;
    case 'b':
        en_ret_float = 1;
        en_fret = fe ? fe->f84 : random_float();
        return r;
    case 'g':
    case 'G':
        en_ret_float = 1;
        en_fret = (r & 3) ? args[sp->ret == 'g' ? 0 : 1] : bits((float)(r % 50) - 10.0f);
        return r;
    case 'c': {
        const DWORD c = fe ? fe->cls : fa ? fa->cls : fo ? fo->cls : (me == 0xf7900000u ? g_tex.cls : 8);
        return ptrv(g_classes[c % 9]);
    }
    case 'e': return fe && fe->creator ? ptrv(&g_ents[fe->creator - 1]) : 0;
    case 'u': return fe ? (r & ~0xffu) | fe->upper : 0;
    case 'p': return (r & 3) == 0 ? ptrv(&g_ais[r % NAI]) : 0;
    case 'q': {
        BYTE *m = reinterpret_cast<BYTE *>(args[sp->nargs - 1]);
        if ((r & 0x30) == 0x30)
            m[8] = (BYTE)((r >> 6) & 1);
        return (r & ~0xffu) | ((r >> 9) & 1);
    }
    case 'd':
        if (fe)
            fe->deleted++;
        if (fa)
            fa->deleted++;
        if (fo)
            fo->deleted++;
        return ptrv(self);
    case 'y': return (r & ~0xffu) | ((r >> 9) & 1);
    case 'l': return *at<DWORD *>(MODEL_COUNT);
    case 'w': return ptrv(&g_net);
    case 'r': return ptrv(g_player);
    case 'h': return ptrv(g_difficulty);
    case 's': return ptrv(r & 1 ? "Characters/Test" : "Test Model");
    case 'm': return ptrv(&g_models[r % NMODEL]);
    }
    return r;
}

// ---- Stand-ins for the exe functions that reach further ------------------------------------------------

void THISCALL rec_layer_object(void *o, int *rect, DWORD flag) { ev('o', tag(ptrv(o)), fnv(2166136261u, rect, 16), flag); }
DWORD THISCALL rec_tell_rect(void *p) { ev('T', tag(ptrv(p))); return 0x7e11; }
void THISCALL rec_layer_remove(void *l, void *p) { ev('R', tag(ptrv(l)), tag(ptrv(p))); }
void THISCALL rec_layer_add(void *l, void *p) { ev('A', tag(ptrv(l)), tag(ptrv(p))); }
DWORD THISCALL rec_grid(void *g, void *p) { ev('G', tag(ptrv(g)), tag(ptrv(p))); return 0x6d; }
void THISCALL rec_layer_flags(void *l, void *p, DWORD old, DWORD b) { ev('F', tag(ptrv(l)), tag(ptrv(p)), old ^ (b << 1)); }
BYTE *THISCALL rec_current_layer(void *w) { ev('C', tag(ptrv(w))); return g_layer; }
void *THISCALL rec_first_character(void *w)
{
    ev('1', tag(ptrv(w)));
    return g_first_char ? &g_ents[g_first_char - 1] : nullptr;
}
void THISCALL rec_explosion(void *w, void *l, DWORD m, void *p, DWORD a, DWORD b) { ev('E', tag(ptrv(w)), tag(ptrv(l)) ^ m, tag(ptrv(p)) ^ (a << 4) ^ (b << 8)); }
void THISCALL rec_broadcast(void *p, const char *m) { ev('B', tag(ptrv(p)), text_hash(m)); }
DWORD THISCALL rec_add_listener(void *t, void *l, int o, const char *n) { ev('+', tag(ptrv(t)), tag(ptrv(l)), (DWORD)o ^ text_hash(n)); return 0x4c1; }
DWORD THISCALL rec_remove_listener(void *t, void *l, int o, const char *n) { ev('-', tag(ptrv(t)), tag(ptrv(l)), (DWORD)o ^ text_hash(n)); return 0x4c2; }
void THISCALL rec_mark_done(void *p) { ev('D', tag(ptrv(p))); }
void THISCALL rec_layer_add_what(void *p, DWORD w) { ev('Y', tag(ptrv(p)), w); }
void THISCALL rec_message_tree(void *p, void *m) { ev('W', tag(ptrv(p)), tag(ptrv(m))); }
void THISCALL rec_center(void *p, float *out)
{
    FakeEnt *f = ent_of(tag(ptrv(p)));
    out[0] = f ? f->cx : 1.5f;
    out[1] = f ? f->cy : -2.5f;
    ev('P', tag(ptrv(p)), tag(ptrv(out)));
}
void THISCALL rec_run_action(void *a, void *l, void *e, DWORD z, void *by, void *e2, void **slot)
{
    ev('X', tag(ptrv(a)), tag(ptrv(l)) ^ tag(ptrv(e)), z ^ tag(ptrv(by)) ^ tag(ptrv(e2)) ^ tag(ptrv(slot)));
}
void THISCALL rec_damage_message(DWORD *m, float amount, DWORD type, void *by)
{
    m[0] = 0x5f0b00;
    m[1] = bits(amount) ^ type;
    m[2] = tag(ptrv(by)) & 0xff;
    ev('M', bits(amount), type, tag(ptrv(by)));
}
void THISCALL rec_death_kill(void *dt, void *e) { ev('K', tag(ptrv(dt)), tag(ptrv(e))); }
void __fastcall rec_damage_radius(void *l, float *pt, void *e, DWORD r, float r2, DWORD type, void *by, DWORD a, DWORD b)
{
    ev('Q', tag(ptrv(l)) ^ bits(pt[0]) ^ (bits(pt[1]) << 1), tag(ptrv(e)) ^ r ^ bits(r2), type ^ tag(ptrv(by)) ^ a ^ (b << 3));
}
void *THISCALL rec_texture_at(void *map, int x, int y)
{
    ev('U', tag(ptrv(map)), (DWORD)x, (DWORD)y);
    return g_texture_on ? &g_tex : nullptr;
}
int __fastcall rec_frame_of_moving(DWORD n, DWORD flag, float f, void *e, int sum, int rot)
{
    ev('O', n ^ (flag << 16), bits(f) ^ tag(ptrv(e)), (DWORD)sum ^ ((DWORD)rot << 8));
    return (int)(en_hash(n, (DWORD)sum, (DWORD)rot) % 40) - 5;
}
DWORD THISCALL rec_is_dynamic(void *d, void *o)
{
    FakeEnt *f = ent_of(tag(ptrv(o)));
    ev('d', tag(ptrv(d)), tag(ptrv(o)));
    return 0x5500 | (f ? f->dyn : 0);
}
void *THISCALL rec_dynamic_pointer(void *d, void *o) { ev('p', tag(ptrv(d)), tag(ptrv(o))); return g_hue; }
void THISCALL rec_set_dynamic(void *d, void *o, void *v)
{
    const BYTE *h = static_cast<const BYTE *>(v);
    ev('s', tag(ptrv(d)), tag(ptrv(o)), h ? fnv(2166136261u, h + 8, 0x18) ^ *reinterpret_cast<const DWORD *>(h) : 0);
    if (h)
        vcall<void *>(v, 1, 1);   // its slot 1 (the holder's deleting destructor)
}
int THISCALL rec_int_get(void *d, void *o)
{
    FakeEnt *f = ent_of(tag(ptrv(o)));
    ev('i', tag(ptrv(d)), tag(ptrv(o)));
    return f ? f->updates : 3;
}
void THISCALL rec_int_set(void *d, void *o, DWORD v) { ev('I', tag(ptrv(d)), tag(ptrv(o)), v); }
void THISCALL rec_dyn_remove(void *d, void *o) { ev('r', tag(ptrv(d)), tag(ptrv(o))); }
void THISCALL rec_stack_update(void *s, DWORD a, void *e) { ev('S', tag(ptrv(s)), a, tag(ptrv(e))); }
void THISCALL rec_stack_message(void *s, void *e, DWORD a) { ev('s', tag(ptrv(s)), tag(ptrv(e)), a); }
void THISCALL rec_stack_clear(void *s, void *e) { ev('c', tag(ptrv(s)), tag(ptrv(e))); }
DWORD THISCALL rec_read_own_line(void *o, const char *n, DWORD v, DWORD r) { ev('L', tag(ptrv(o)), text_hash(n), v ^ r); return 0x10e; }
// The network packet
void THISCALL rec_block_type(void *p, void *reader, DWORD kind) { ev('b', tag(ptrv(p)), ptrv(reader), kind); }
void THISCALL rec_write_dword(void *p, DWORD v) { ev('4', tag(ptrv(p)), v); }
void THISCALL rec_write_word(void *p, DWORD v) { ev('2', tag(ptrv(p)), v & 0xffff); }
DWORD THISCALL rec_write_byte(void *p, DWORD v) { ev('8', tag(ptrv(p)), v & 0xff); return 0x77; }
DWORD next_read()
{
    g_rnext = en_hash(g_rnext, 0x52ead, 1);
    switch (g_rnext % 5) {
    case 0: return 0;
    case 1: return g_rnext & 0xff;
    case 2: return (g_rnext >> 3) & 0x1f;
    default: return g_rnext;
    }
}
DWORD THISCALL rec_read_dword(void *p, DWORD *out) { *out = next_read(); ev('r', tag(ptrv(p)), *out); return 1; }
DWORD THISCALL rec_read_word(void *p, WORD *out) { *out = (WORD)next_read(); ev('w', tag(ptrv(p)), *out); return 1; }
DWORD THISCALL rec_read_byte(void *p, BYTE *out) { *out = (BYTE)next_read(); ev('y', tag(ptrv(p)), *out); return 1; }
void THISCALL rec_read_block(void *p) { ev('k', tag(ptrv(p))); }
void *__cdecl rec_take_object()
{
    const DWORD v = next_read();
    ev('t', v & 3);
    return (v & 3) ? &g_objs[v % NOBJ] : nullptr;
}
void THISCALL rec_register_reader(void *p, void *reader, const char *name) { ev('g', tag(ptrv(p)), ptrv(reader), text_hash(name)); }

struct Patches {
    static constexpr int N = 48;
    CodePatch p[N];
    int n = 0;
    void add(DWORD a, void *f) { patch_jump(p[n++], a, f); }
    void on()
    {
        n = 0;
        add(0x4fa8c0, (void *)&rec_tell_rect);
        add(0x540640, (void *)&rec_layer_object);
        add(0x4ebcb0, (void *)&rec_layer_remove);
        add(0x4eb6f0, (void *)&rec_layer_add);
        add(0x579c10, (void *)&rec_grid);
        add(0x4ee730, (void *)&rec_layer_flags);
        add(0x4f1050, (void *)&rec_current_layer);
        add(0x4f5d80, (void *)&rec_first_character);
        add(0x4f60e0, (void *)&rec_explosion);
        add(0x4fc240, (void *)&rec_broadcast);
        add(0x4fc1b0, (void *)&rec_add_listener);
        add(0x4fc200, (void *)&rec_remove_listener);
        add(0x4fbd60, (void *)&rec_mark_done);
        add(0x4fc8a0, (void *)&rec_layer_add_what);
        add(0x4fb810, (void *)&rec_message_tree);
        add(0x4fb0a0, (void *)&rec_center);
        add(0x41a720, (void *)&rec_run_action);
        add(0x476450, (void *)&rec_damage_message);
        add(0x478260, (void *)&rec_death_kill);
        add(0x477640, (void *)&rec_damage_radius);
        add(0x541fd0, (void *)&rec_texture_at);
        add(0x55e500, (void *)&rec_frame_of_moving);
        add(0x418fc0, (void *)&rec_is_dynamic);
        add(0x418790, (void *)&rec_dynamic_pointer);
        add(0x4187c0, (void *)&rec_set_dynamic);
        add(0x54d630, (void *)&rec_int_get);
        add(0x54dae0, (void *)&rec_int_set);
        add(0x418fb0, (void *)&rec_dyn_remove);
        add(0x49a480, (void *)&rec_stack_update);
        add(0x49a530, (void *)&rec_stack_message);
        add(0x49a6b0, (void *)&rec_stack_clear);
        add(0x417500, (void *)&rec_read_own_line);
        add(0x51ff50, (void *)&rec_block_type);
        add(0x520940, (void *)&rec_write_dword);
        add(0x5209c0, (void *)&rec_write_word);
        add(0x520820, (void *)&rec_write_byte);
        add(0x520550, (void *)&rec_read_dword);
        add(0x520510, (void *)&rec_read_word);
        add(0x520490, (void *)&rec_read_byte);
        add(0x5200d0, (void *)&rec_read_block);
        add(0x55e4a0, (void *)&rec_take_object);
    }
    void off()
    {
        for (int i = n - 1; i >= 0; i--)
            unpatch(p[i]);
        n = 0;
    }
};
Patches g_patches;
CodePatch g_reader_patch;

// ---- Setting up the fakes and the globals ---------------------------------------------------------------

void init_tables()
{
    for (int t = 0; t < NTABLES; t++)
        for (int s = 0; s < NSLOTS; s++)
            g_vt[t][s] = ptrv(en_thunks) + 10u * (t * NSLOTS + s);
    for (DWORD k = 0; k < NTABLES * NSLOTS; k++) {
        const BYTE *th = reinterpret_cast<const BYTE *>(en_thunks) + 10 * k;
        if (th[0] != 0xb8 || *reinterpret_cast<const DWORD *>(th + 1) != k || th[5] != 0xe9) {
            log("  entities: recorder thunk %u is broken", k);
            break;
        }
    }
    g_classes[0] = EntityBase_ClassInfo();
    g_classes[1] = EntityDestructable_ClassInfo();
    g_classes[2] = EntityAnimated_ClassInfo();
    g_classes[3] = at<CClassInfo *(__cdecl *)()>(0x4f9bf0)();   // CLayerPart
    g_classes[4] = at<CClassInfo *(__cdecl *)()>(0x49a930)();   // CEntityCharacter
    g_classes[5] = at<CClassInfo *(__cdecl *)()>(0x4fcb00)();   // CAIBase
    g_classes[6] = at<CClassInfo *(__cdecl *)()>(0x42f310)();   // CBehaviorActivity
    g_classes[7] = at<CClassInfo *(__cdecl *)()>(0x4d1d50)();   // CGroundTextureFrame
    g_classes[8] = at<CClassInfo *(__cdecl *)()>(0x4163b0)();   // CObject
    if (!g_names_made) {
        constexpr const char *NAMES[] = {"Idle", "Walk", "Run", "Death", "Fire", "idle"};
        for (int i = 0; i < 6; i++)
            at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&g_names[i], NAMES[i]);
        g_names_made = true;
    }
}

DWORD g_tex_dummy;

// The exe's globals the fakes stand in for, saved before and restored after each side
struct Globals {
    DWORD world, reporter, list_vtable, records, count, death_records, death_count, mode_base, mode_animated, g6bdd18;
};
Globals g_saved;
void save_globals()
{
    g_saved = {*at<DWORD *>(0x6c2080), *at<DWORD *>(0x6c02cc), *at<DWORD *>(MODEL_LIST), *at<DWORD *>(MODEL_RECORDS),
               *at<DWORD *>(MODEL_COUNT), *at<DWORD *>(0x6bd8e0), *at<DWORD *>(0x6bd8e4), *at<DWORD *>(BASE_READ_MODE),
               *at<DWORD *>(ANIMATED_READ_MODE), *at<DWORD *>(0x6bdd18)};
}
void restore_globals()
{
    *at<DWORD *>(0x6c2080) = g_saved.world;
    *at<DWORD *>(0x6c02cc) = g_saved.reporter;
    *at<DWORD *>(MODEL_LIST) = g_saved.list_vtable;
    *at<DWORD *>(MODEL_RECORDS) = g_saved.records;
    *at<DWORD *>(MODEL_COUNT) = g_saved.count;
    *at<DWORD *>(0x6bd8e0) = g_saved.death_records;
    *at<DWORD *>(0x6bd8e4) = g_saved.death_count;
    *at<DWORD *>(BASE_READ_MODE) = g_saved.mode_base;
    *at<DWORD *>(ANIMATED_READ_MODE) = g_saved.mode_animated;
    *at<DWORD *>(0x6bdd18) = g_saved.g6bdd18;
}

DWORD random_flags()
{
    constexpr DWORD BITS[] = {0x10000, 0x20000, 0x40000, 0x80000, 0x100000, 0x200000, 0x400000, 0x800000, 0x2000000, 0x4000000};
    DWORD f = 0;
    for (DWORD b : BITS)
        if (random() % 3 == 0)
            f |= b;
    return random() % 6 ? f : random();
}
float bounded_float() { return (float)((int)(random() % 4000) - 1000) / 7.0f; }

Entity *ent(int i) { return reinterpret_cast<Entity *>(g_ents[i].o); }
Animated *anim(int i) { return reinterpret_cast<Animated *>(g_ents[i].o); }
CModel *model(int i) { return reinterpret_cast<CModel *>(g_models[i].m); }

void setup_fakes()
{
    memset(g_ents, 0, sizeof g_ents);
    memset(g_ais, 0, sizeof g_ais);
    memset(g_seqs, 0, sizeof g_seqs);
    memset(g_models, 0, sizeof g_models);
    memset(g_objs, 0, sizeof g_objs);
    memset(g_children, 0, sizeof g_children);
    memset(g_records, 0, sizeof g_records);
    memset(g_death_records, 0, sizeof g_death_records);
    memset(g_layer, 0, sizeof g_layer);
    memset(g_player, 0, sizeof g_player);
    memset(g_activity, 0, sizeof g_activity);
    memset(g_attached, 0, sizeof g_attached);
    memset(&g_tex, 0, sizeof g_tex);
    g_world = {};
    g_net = {};
    g_reporter = {};
    g_world.vftable = ptrv(g_vt[T_WORLD]);
    g_net.vftable = ptrv(g_vt[T_OTHER]);
    g_reporter.vftable = ptrv(g_vt[T_OTHER]);
    g_tex.vftable = ptrv(g_vt[T_SEQ]);   // slot 2 only (the class)
    g_tex.cls = random() % 3 ? 7 : random() % 9;
    member_dword(g_tex, 0x40) = random() % 3 ? random() % 5 : 0;
    member_dword(g_tex, 0x44) = random() % 4 ? bits((float)(random() % 40)) : random_float();
    g_texture_on = random() % 4 != 0;
    for (int i = 0; i < NAI; i++) {
        g_ais[i].vftable = ptrv(g_vt[T_AI]);
        g_ais[i].done = random() % 4 == 0;
        g_ais[i].cls = random() % 3 ? 5 : random() % 9;
    }
    for (int i = 0; i < NOBJ; i++) {
        g_objs[i].vftable = ptrv(g_vt[T_OTHER]);
        g_objs[i].cls = random() % 9;
    }
    for (int i = 0; i < NSEQ; i++) {
        FakeSeq &s = g_seqs[i];
        *reinterpret_cast<DWORD *>(s.s) = ptrv(g_vt[T_SEQ]);
        constexpr DWORD FLAGS[] = {0, 0x80, 0x100, 0x180, 0x200, 0x80};
        constexpr DWORD FRAMES[] = {0, 1, 2, 5, 8, 3};
        constexpr DWORD ROTATIONS[] = {1, 2, 8, 16, 0, 1};
        constexpr DWORD RANGES[] = {0, 0x40c90fdb, 0x40490fdb, 0x3f800000};
        *reinterpret_cast<DWORD *>(s.s + 0x1c) = FLAGS[random() % 6];
        *reinterpret_cast<DWORD *>(s.s + 0x38) = random_angle();
        *reinterpret_cast<DWORD *>(s.s + 0x3c) = random() % 4 ? RANGES[random() % 4] : random_float();
        *reinterpret_cast<DWORD *>(s.s + 0x48) = FRAMES[random() % 6];
        *reinterpret_cast<DWORD *>(s.s + 0x4c) = ROTATIONS[random() % 6];
        *reinterpret_cast<DWORD *>(s.s + 0x50) = random() % 8;
        s.k25 = random() % 4;
        s.f26 = random_float();
    }
    for (int i = 0; i < NMODEL; i++) {
        FakeModel &f = g_models[i];
        CModel *m = model(i);
        m->activity = {0, f.activity, random() % 4, 4};
        for (DWORD k = 0; k < m->activity.count; k++)
            f.activity[k] = random() % 5 ? &g_ais[random() % NAI] : nullptr;
        constexpr DWORD HP[] = {0, 0x41200000, 0x42c80000, 0x3f800000};
        m->hit_points = as_float_v(random() % 4 ? HP[random() % 4] : random_float());
        m->collide_damage = random() % 3 ? (float)(random() % 3) * 5.0f : as_float_v(random_float());
        m->collide_damage_type = random() % 4;
        m->explosion_damage = random() % 2 ? 0.0f : as_float_v(random() % 3 ? bits(25.0f) : random_float());
        m->sorting_y = (int)(random() % 200) - 50;
        m->num_sequences = 1 + random() % 4;   // at least one: the game ends on _stricmp(NULL) without (both)
        m->sequences = f.sequences;
        for (int k = 0; k < 128; k++)
            f.sequences[k] = g_seqs[(k + i) % NSEQ].s;
        m->explosion = random() % 3 ? 0 : random() % 4;
        m->death_sound = random() % 9;
        m->pain_sound = random() % 9;
        m->turn_speed = as_float_v(random() % 3 ? bits((float)(random() % 360)) : random_float());
        m->sequence_names = g_names;
        m->default_sequence.buffer = g_names[random() % 6].buffer;
    }
    for (int i = 0; i < NREC; i++) {
        BYTE *r = g_records[i];
        lstrcpyA(reinterpret_cast<char *>(r), i & 1 ? "Characters/Test/Model" : "Model");
        r[6] = (BYTE)('0' + i);
        *reinterpret_cast<void **>(r + 0x78) = random() % 4 ? g_models[(i + random() % 2) % NMODEL].m : nullptr;
        r[0x7f] = random() % 10 == 0;
    }
    for (int i = 0; i < NDEATH; i++) {
        *reinterpret_cast<void **>(g_death_records[i] + 0x78) = g_death_objects[i];
        g_death_records[i][0x7f] = random() % 10 == 0;
    }
    *at<DWORD *>(MODEL_RECORDS) = random() % 16 ? ptrv(g_records) : 0;
    *at<DWORD *>(MODEL_COUNT) = random() % 8 ? NREC : random() % NREC;
    *at<DWORD *>(MODEL_LIST) = ptrv(g_vt[T_LIST]);
    *at<DWORD *>(0x6bd8e0) = ptrv(g_death_records);
    *at<DWORD *>(0x6bd8e4) = NDEATH;
    *at<DWORD *>(0x6c2080) = ptrv(&g_world);
    *at<DWORD *>(0x6c02cc) = ptrv(&g_reporter);
    *reinterpret_cast<DWORD *>(g_layer + 0x7c) = ptrv(&g_tex_dummy);
    *reinterpret_cast<DWORD *>(g_layer + 0x88) = random() % 3000;
    *reinterpret_cast<DWORD *>(g_layer + 0x8c) = random() % 3000;
    *reinterpret_cast<DWORD *>(g_player + 0xc0) = random() % 4 ? bits((float)(random() % 3000)) : random_float();
    *reinterpret_cast<DWORD *>(g_player + 0xc4) = random() % 4 ? bits((float)(random() % 3000)) : random_float();
    *reinterpret_cast<DWORD *>(g_difficulty + 0x10) = random_float();
    *reinterpret_cast<DWORD *>(g_difficulty + 0x14) = random_float();
    for (int k = 0; k < 8; k++)
        reinterpret_cast<DWORD *>(g_hue)[k] = random() % 3 ? bits((float)(random() % 400) - 20.0f) : random_float();
    for (int i = 0; i < NENT; i++) {
        FakeEnt &f = g_ents[i];
        Animated *a = anim(i);
        a->vftable = ptrv(g_vt[T_ENT]);
        a->parent = i > 0 && random() % 3 == 0 ? ent(random() % i) : nullptr;
        a->target = random() % 4 == 0 ? ent(random() % NENT) : nullptr;
        a->flags = random_flags();
        a->used_in = random();
        a->team = random();
        a->id = (WORD)random();
        a->activity.vftable = VT_ACTIVITY;
        a->activity.data = g_activity[i];
        a->activity.count = random() % 5;
        a->activity.capacity = 6;
        for (DWORD k = 0; k < a->activity.count; k++)
            g_activity[i][k] = random() % 5 ? &g_ais[random() % NAI] : nullptr;
        a->layer = random() % 4 ? g_layer : nullptr;
        a->x = as_float_v(random_float());
        a->y = as_float_v(random_float());
        a->rx = (int)(random() % 4000) - 1000;
        a->ry = (int)(random() % 4000) - 1000;
        for (int &b : a->bounds)
            b = (int)random();
        a->render_height = random() % 3 ? (int)(random() % 40) - 10 : (int)random();
        a->render_height_f = random() % 3 ? (float)a->render_height : as_float_v(random_float());
        a->model = random() % 8 ? random() % (NREC + 2) : 0xffffffff;
        a->sequence = random() % 5 ? random() % 4 : random() % 2 ? 0xffffffff : random();
        a->damage = as_float_v(random() % 3 ? bits((float)(random() % 120) - 10.0f) : random_float());
        a->destroyed_action = random() % 3 ? nullptr : &g_objs[random() % NOBJ];
        a->damaged_action = random() % 3 ? nullptr : &g_objs[random() % NOBJ];
        a->hit_action = random() % 3 ? nullptr : &g_objs[random() % NOBJ];
        a->death_type = random() % 5;
        for (BYTE &b : a->behaviors)
            b = (BYTE)random();
        a->aflags = random() & 0x1ff;
        a->fade = random() % 2 ? 1.0f : as_float_v(random_float());
        a->play_ratio = as_float_v(random_float());
        a->frame = as_float_v(random_float());
        a->frame_index = random() % 4 ? random() % 6 : random();
        a->aiming = as_float_v(random_angle());
        a->movement = random() % 4 ? as_float_v(random_angle()) : a->aiming;
        a->start_facing = as_float_v(random_angle());
        a->created_by = random() % 3 ? nullptr : ent(random() % NENT);
        a->attached = {ptrv(g_vt[T_OTHER]), g_attached[i], random() % 4, 4};
        for (DWORD k = 0; k < a->attached.count; k++)
            g_attached[i][k] = random() % 4 ? &g_objs[random() % NOBJ] : nullptr;
        constexpr DWORD CLS[] = {2, 2, 2, 1, 0, 3, 4, 2};
        f.cls = CLS[random() % 8];
        f.creator = i > 0 && random() % 2 ? 1 + random() % i : 0;
        f.k26 = random() % 5;
        f.k52 = (DWORD)((int)(random() % 2000) - 1000);
        f.k53 = (DWORD)((int)(random() % 2000) - 1000);
        f.k54 = random() % 3 ? 0 : (DWORD)((int)(random() % 100) - 50);
        f.k56 = random() % 100;
        f.f46 = random_angle();
        f.f84 = random() % 3 ? bits(bounded_float()) : random_float();
        f.upper = random() % 3 ? (BYTE)(random() % 2) : 0x80;
        f.dyn = random() % 4 == 0;
        f.updates = (int)(random() % 14);
        f.cx = as_float_v(random_float());
        f.cy = as_float_v(random_float());
    }
    // Child lists of the first entities, towards higher numbers only
    for (int i = 0; i < 3; i++) {
        FakeChildList &l = g_children[i];
        l.l.vftable = 0;
        l.l.array_vftable = ptrv(g_vt[T_OTHER]);
        l.l.data = l.items;
        l.l.capacity = 6;
        l.l.count = random() % 4;
        for (DWORD k = 0; k < l.l.count; k++)
            l.items[k] = random() % 5 ? ent(i + 1 + (int)(random() % (NENT - i - 1))) : nullptr;
        anim(i)->children = random() % 3 ? nullptr : &l.l;
    }
    g_first_char = random() % 3 == 0 ? 1 + random() % NENT : 0;
    g_rnext = random();
    for (float &b : g_buf)
        b = as_float_v(random_float());
}

// An entity whose model can be looked up (records there, the record loaded)
void ensure_model(int i)
{
    *at<DWORD *>(MODEL_RECORDS) = ptrv(g_records);
    *at<DWORD *>(MODEL_COUNT) = NREC;
    const DWORD k = random() % NREC;
    *reinterpret_cast<void **>(g_records[k] + 0x78) = g_models[k % NMODEL].m;
    g_records[k][0x7f] = 0;
    ent(i)->model = k;
}
// ... and a current sequence (of up to 128 sequences of the model)
void ensure_sequence(int i, DWORD n = 0)
{
    ensure_model(i);
    CModel *m = static_cast<CModel *>(*reinterpret_cast<void **>(g_records[ent(i)->model] + 0x78));
    if (n)
        m->num_sequences = n;
    else if (!m->num_sequences)
        m->num_sequences = 1 + random() % 4;
    ent(i)->sequence = random() % m->num_sequences;
}

// The state of all fakes (pointers tagged); polygons given to the models are recorded and freed
void st_fakes()
{
    for (int i = 0; i < NENT; i++) {
        const DWORD *d = reinterpret_cast<const DWORD *>(g_ents[i].o);
        for (DWORD k = 0; k < 0xe4 / 4; k++) {
            st(tag(d[k]));
            if (k < 0x30)
                dump(tag(d[k]));
        }
        st(g_ents[i].deleted);
        for (DWORD k = 0; k < 4; k++)
            st(tag(ptrv(g_attached[i][k])));
        for (DWORD k = 0; k < 6; k++)
            st(tag(ptrv(g_activity[i][k])));
    }
    for (int i = 0; i < NAI; i++)
        st(g_ais[i].deleted + g_ais[i].done * 0x100);
    for (int i = 0; i < NOBJ; i++)
        st(g_objs[i].deleted);
    for (int i = 0; i < NMODEL; i++) {
        CModel *m = model(i);
        st((DWORD)m->sorting_y);
        ZPolygon *p = reinterpret_cast<ZPolygon *>(g_models[i].m + 0x78);
        st(p->count);
        if (p->points) {
            if (p->count < 64)
                st_bytes(p->points, p->count * 8);
            heap_free(p->points);
        }
        if (p->edges) {
            if (p->count > 1 && p->count < 64)
                st_bytes(p->edges, (p->count - 1) * 12);
            heap_free(p->edges);
        }
        st_bytes(&p->f0c, 0x1c);
        memset(p, 0, sizeof *p);
    }
    for (int i = 0; i < NREC; i++) {
        st(tag(*reinterpret_cast<DWORD *>(g_records[i] + 0x78)));
        st(g_records[i][0x7f]);
    }
    for (int i = 0; i < 3; i++) {
        st(g_children[i].l.count);
        for (DWORD k = 0; k < 6; k++)
            st(tag(ptrv(g_children[i].items[k])));
    }
    st_bytes(g_buf, sizeof g_buf);
    st(*at<DWORD *>(0x6bdd18));
}

// Both sides of a case; `run` returns eax (already made comparable) and the mask of its defined bits
template <typename C> bool two_sides(Tally &t, const char *what, unsigned seed, C run)
{
    DWORD r[2], mask = 0xffffffff;
    for (int side = 0; side < 2; side++) {
        g_side = side;
        g_seed = seed;
        g_calls = 0;
        g_loghash[side] = g_statehash[side] = 2166136261u;
        g_nlog[side] = 0;
        g_ndump[side] = 0;
        DWORD top = 0;
        g_stack_top = ptrv(&top);
        save_globals();
        if (side == 0)
            hooks_out();
        r[side] = run(mask);
        if (side == 0)
            hooks_in();
        st_fakes();
        restore_globals();
    }
    char d[200];
    wsprintfA(d, "[%s seed %u: eax %x / %x (mask %x), log %d %x / %d %x, state %x / %x]", what, seed, r[0], r[1], mask,
              g_nlog[0], g_loghash[0], g_nlog[1], g_loghash[1], g_statehash[0], g_statehash[1]);
    const bool same = ((r[0] ^ r[1]) & mask) == 0 && g_loghash[0] == g_loghash[1] && g_nlog[0] == g_nlog[1] &&
                      g_statehash[0] == g_statehash[1];
    if (!same && t.mismatches < 10) {
        log_first_difference();
        for (int i = 0; i < g_ndump[0] && i < g_ndump[1]; i++)
            if (g_dump[0][i] != g_dump[1][i]) {
                log("    entity dword %d differs first: %x / %x", i, g_dump[0][i], g_dump[1][i]);
                break;
            }
    }
    t.check(same, "entities", d);
    return same;
}

// ---- Slot scenarios on fake entities ----------------------------------------------------------------------

using F0 = DWORD(THISCALL *)(void *);
using F1 = DWORD(THISCALL *)(void *, DWORD);
using F2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
using F3 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD);
using F4 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD);
using FL0 = long double(THISCALL *)(void *);
using FL1 = long double(THISCALL *)(void *, DWORD);
using FL2 = long double(THISCALL *)(void *, DWORD, DWORD);
using FAST = DWORD(__fastcall *)(void *, void *, DWORD);

DWORD random_arg()
{
    switch (random() % 4) {
    case 0: return 0;
    case 1: return random() % 100;
    case 2: return ptrv(ent(random() % NENT));
    default: return random();
    }
}
// A polygon to copy (Polygon_Assign): points and edges from the heap, freed by the caller
ZPolygon g_poly;
void make_poly()
{
    memset(&g_poly, 0, sizeof g_poly);
    const DWORD n = random() % 5;
    g_poly.count = n;
    if (n) {
        g_poly.points = static_cast<DWORD *>(heap_alloc(n * 8));
        for (DWORD k = 0; k < n * 2; k++)
            g_poly.points[k] = bits((float)(random() % 300));
    }
    if (n > 1) {
        g_poly.edges = static_cast<DWORD *>(heap_alloc((n - 1) * 12));
        for (DWORD k = 0; k < (n - 1) * 3; k++)
            g_poly.edges[k] = random_float();
    }
    for (DWORD &v : g_poly.f0c)
        v = random();
    g_poly.f1c = random();
    g_poly.f20 = random();
    g_poly.f24 = random();
}
void free_poly()
{
    if (g_poly.points)
        heap_free(g_poly.points);
    if (g_poly.edges)
        heap_free(g_poly.edges);
    memset(&g_poly, 0, sizeof g_poly);
}
// The root of an entity made animated (slot 92 reads its fade)
void animated_root(int i)
{
    LayerPart *p = ent(i);
    while (p->parent)
        p = p->parent;
    for (int k = 0; k < NENT; k++)
        if (ent(k) == p)
            g_ents[k].cls = 2;
}

constexpr int NOPS = 96;
int g_op;
bool g_trace = false;

// One operation on a random fake entity; eax mask per function
DWORD slot_case(DWORD &mask)
{
    setup_fakes();
    const int op = g_op >= 0 ? g_op : (int)(random() % NOPS);
    const int i = (int)(random() % NENT);
    if (g_trace)
        log("  case op %d entity %d side %d", op, i, g_side);
    Entity *e = ent(i);
    Animated *a = anim(i);
    void *pk = g_packet;
    const DWORD x = random_arg(), y = random_arg(), z = random_arg(), w = random_arg();
    const DWORD f = random_float(), g = random_angle();
    DWORD r = 0;
    mask = 0;
    switch (op) {
    case 0: case 1: case 2: {
        constexpr DWORD A[] = {0x487240, 0x487250, 0x487260};
        r = at<F0>(A[op])(e), mask = 0xffffffff;
        break;
    }
    case 3: st_ld(at<FL0>(0x487270)(e)); break;
    case 4: {
        DWORD out[16];
        for (DWORD &v : out)
            v = random();
        at<F1>(0x487280)(e, ptrv(out));
        st_bytes(out, sizeof out);
        break;
    }
    case 5: {
        constexpr const char *N[] = {"Model", "model", "MODEL", "Position X", "Mode"};
        r = at<F3>(0x488170)(e, ptrv(N[random() % 5]), x, y), mask = 0xffffffff;
        break;
    }
    case 6: at<F1>(0x4881b0)(e, x); break;
    case 7: at<F0>(0x4882f0)(e); break;
    case 8: {
        DWORD m[3] = {0x5eeb64, random(), random() % 2};
        r = at<F1>(0x488420)(e, ptrv(m)), mask = 0xffffffff;
        st(m[2] & 0xff);
        break;
    }
    case 9: at<F1>(0x488560)(e, x); break;
    case 10: at<F0>(0x488cf0)(e); break;
    case 11: at<FAST>(0x4890c0)(pk, e, random() % 2); break;
    case 12: at<FAST>(0x489230)(pk, e, random() % 2); break;
    case 13: ensure_model(i), r = at<F2>(0x4893b0)(e, ptrv(pk), random() % 3 ? 1 : 0), mask = 0xffffffff; break;
    case 14: {
        *at<DWORD *>(BASE_READ_MODE) = random() % 3;
        if (random() % 2)
            ensure_sequence(i);
        else
            e->sequence = 0xffffffff;
        at<F1>(0x4896e0)(e, ptrv(pk));
        break;
    }
    case 15: at<F0>(0x489fb0)(e); break;
    case 16: at<F2>(0x489fd0)(e, f, random_float()); break;
    case 17: at<F1>(0x48a700)(e, random_flags()); break;
    case 18: at<F1>(0x48a750)(e, random_flags()); break;
    case 19: {
        *at<DWORD *>(MODEL_RECORDS) = ptrv(g_records);
        if (random() % 4 == 0)
            e->model = 0xffffffff;
        r = text_hash(reinterpret_cast<const char *>(at<F0>(0x48a7a0)(e))), mask = 0xffffffff;
        break;
    }
    case 20: r = at<F2>(0x48a7c0)(e, x, y), mask = 0xffffffff; break;
    case 21: r = at<F2>(0x48a920)(e, x, y), mask = 0xffffffff; break;
    case 22: at<F4>(0x48aa80)(e, x, y, z, w); break;
    case 23: r = at<F0>(0x48abf0)(e), mask = 0xffffffff; break;
    case 24: at<F1>(0x48ad50)(e, random() % 3 ? random() % 100 : g_ents[i].k56); break;
    case 25: r = at<F0>(0x48aed0)(e), mask = 0xffffffff; break;
    case 26: ensure_model(i), r = tag(at<F0>(0x48aee0)(e)), mask = 0xffffffff; break;
    case 27: ensure_model(i), r = tag(at<F0>(0x48b000)(e)), mask = 0xffffffff; break;
    case 28: {
        // an old sequence beyond the model's: slot 25 of a null sequence in both
        if (random() % 4)
            ensure_sequence(i);
        else if (random() % 2)
            e->sequence = 0xffffffff;
        else
            e->flags |= 0x200000;
        at<F1>(0x48b120)(e, random() % 3 ? random() % 4 : random());
        break;
    }
    case 29: r = tag(at<F0>(0x48b290)(e)), mask = 0xffffffff; break;
    case 30: at<F1>(0x48b390)(e, random() % 3 ? random() % (NREC + 2) : e->model); break;
    case 31: r = at<F0>(0x48b560)(e), mask = 0xffffffff; break;
    case 32: r = at<F1>(0x48b570)(e, random() % 4 ? ptrv(g_seqs[random() % NSEQ].s) : 0), mask = 0xffffffff; break;
    case 33: r = tag(at<F0>(0x48b590)(e)), mask = 0xffffffff; break;
    case 34: r = at<F1>(0x48b6b0)(e, random() % 2 ? (DWORD)e->render_height : (DWORD)((int)(random() % 1000) - 500)), mask = 0xffffffff; break;
    case 35: {
        const DWORD h = random() % 3 ? bits(e->render_height_f + (float)(random() % 3) * 0.006f) : random_float();
        st_ld(at<FL1>(0x48b770)(e, h));
        break;
    }
    case 36: r = tag(at<F0>(0x48b860)(e)), mask = 0xffffffff; break;
    case 37:
        make_poly();
        at<F1>(0x48b9c0)(e, random() % 5 ? ptrv(&g_poly) : 0);
        free_poly();
        break;
    case 38: r = at<F0>(0x48bb20)(e) & 0xff, mask = 0xff; break;
    case 39: r = tag(at<F2>(0x48bc90)(e, ptrv(g_classes[random() % 9]), random() % 3 ? 0 : random())), mask = 0xffffffff; break;
    case 40: {
        patch_jump(g_reader_patch, 0x51ff30, (void *)&rec_register_reader);
        at<F0>(random() % 2 ? 0x487f60 : 0x48e8b0)(pk);
        unpatch(g_reader_patch);
        break;
    }
    case 41: *at<DWORD *>(0x6bdd18) = random(), at<F0>(0x487f90)(nullptr); break;
    // CEntityDestructable
    case 42: at<F1>(0x48ca40)(e, x); break;
    case 43: at<F4>(0x48cb90)(e, ptrv(ent(random() % NENT)), y, z, w); break;
    case 44: at<F0>(0x48cdf0)(e); break;
    case 45: at<F0>(0x48d020)(e); break;
    case 46: st_ld(at<FL0>(0x48d030)(e)); break;
    case 47: st_ld(at<FL0>(0x48d150)(e)); break;
    case 48: st_ld(at<FL0>(0x48d260)(e)); break;
    case 49: at<F1>(0x48d330)(e, f); break;
    case 50: {
        ensure_model(i);
        const DWORD type = random() % 4 ? random() % 4 : at<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(at<void *>(0x6bd760), "!None", -1);
        at<F3>(0x48d380)(e, random() % 3 ? bits((float)(random() % 60)) : f, type, random() % 3 ? ptrv(ent(random() % NENT)) : random() % 2 ? ptrv(e) : 0);
        break;
    }
    case 51: st_ld(at<FL2>(0x48d6e0)(e, f, x)); break;
    case 52: at<F0>(0x48d6f0)(e); break;
    case 53: at<F1>(0x48d7f0)(e, f); break;
    case 54: {
        const bool deaths = random() % 3 == 0;
        r = tag(at<F1>(0x48d8f0)(at<void *>(deaths ? DEATH_TYPES : MODEL_LIST), random() % 3 ? random() % 8 : random())), mask = 0xffffffff;
        break;
    }
    // CEntityAnimated
    case 55: at<F1>(0x48e8f0)(e, g); break;
    case 56: st_ld(at<FL0>(0x48e9b0)(e)); break;
    case 57: r = at<F0>(0x48e9c0)(e), mask = 0xffffffff; break;
    case 58: r = at<F1>(0x48e9d0)(e, x), mask = 0xffffffff; break;
    case 59: at<F0>(0x48eb50)(e); break;
    case 60: r = at<F0>(0x48ebb0)(e), mask = 0xffffffff; break;
    case 61: r = at<F1>(0x48ebc0)(e, random() % 3 ? random() % 2 : random()), mask = 0xffffffff; break;
    case 62: r = at<F1>(0x48ebf0)(e, random() % 3 ? random() % 2 : random()), mask = 0xffffffff; break;
    case 63: r = at<F0>(0x48ec20)(e), mask = 0xffffffff; break;
    case 64: r = at<F0>(0x48ec30)(e), mask = 0xffffffff; break;
    case 65: r = at<F1>(0x48ec40)(e, random() % 3 ? random() % 2 : random()), mask = 0xffffffff; break;
    case 66: ensure_sequence(i), r = at<F2>(0x48ec70)(e, ptrv(pk), random() % 3 ? 1 : 0), mask = 0xffffffff; break;
    case 67: {
        *at<DWORD *>(ANIMATED_READ_MODE) = random() % 4;
        *at<DWORD *>(BASE_READ_MODE) = random() % 3;
        ensure_sequence(i, 128);
        at<F1>(0x48ef00)(e, ptrv(pk));
        break;
    }
    case 68: r = at<F0>(0x48f180)(e), mask = 0xffffffff; break;
    case 69: r = tag(at<F0>(0x48f190)(e)), mask = 0xffffffff; break;
    case 70: at<F1>(0x48f1a0)(e, random() % 3 ? ptrv(ent(random() % NENT)) : 0); break;
    case 71: {
        constexpr const char *T[] = {"Died", "dElEtE", "Leaving Scope", "Hello", "", "died ", "LEAVING SCOPE"};
        const DWORD sender = random() % 2 && a->created_by ? ptrv(a->created_by) : ptrv(ent(random() % NENT));
        r = at<F2>(0x48f1e0)(e, sender, ptrv(T[random() % 7])), mask = 0xffffffff;
        break;
    }
    case 72: r = at<F0>(0x48f260)(e) & 0xff, mask = 0xff; break;
    case 73: ensure_model(i), at<F1>(0x48f300)(e, random() % 3 ? bits((float)(random() % 100) / 100.0f) : f); break;
    case 74: at<F1>(0x48f490)(e, x); break;
    case 75:
        // the same model keeps the sequence, which then must be one of the model's (both end on _stricmp(NULL))
        if (random() % 2)
            ensure_sequence(i);
        else
            e->sequence = 0xffffffff;
        at<F1>(0x48f4b0)(e, random() % 3 ? random() % (NREC + 2) : e->model);
        break;
    case 76: animated_root(i), st_ld(at<FL0>(0x48f510)(e)); break;
    case 77: ensure_model(i), at<F0>(0x48f590)(e); break;
    case 78: ensure_sequence(i), st_ld(at<FL0>(0x48f600)(e)); break;
    case 79: at<F0>(0x48f630)(e); break;
    case 80: at<F1>(0x48f6a0)(e, f); break;
    case 81: {
        constexpr const char *N[] = {"Idle", "idle", "Walk", "Nope", "Death", "RUN"};
        // a sequence beyond the model's ends the game in both (_stricmp of a null name)
        if (random() % 2)
            ensure_sequence(i);
        else
            e->sequence = 0xffffffff;
        r = at<F1>(0x48f700)(e, ptrv(N[random() % 6])) & 0xff, mask = 0xff;
        break;
    }
    case 82: at<F2>(0x48f8b0)(e, random() % 4 ? g : bits(a->aiming), random() % 2); break;
    case 83: at<F2>(0x48fa00)(e, random() % 4 ? g : bits(a->movement), random() % 2); break;
    case 84: at<F1>(0x48fb50)(e, random() % 2); break;
    case 85: r = at<F0>(0x48fc40)(e), mask = 0xffffffff; break;
    case 86: st_ld(at<FL0>(0x48fc50)(e)); break;
    case 87: r = at<F1>(0x48fd40)(e, random() % 5 ? ptrv(g_seqs[random() % NSEQ].s) : 0), mask = 0xffffffff; break;
    case 88: r = at<F1>(0x48fd80)(e, random() % 5 ? ptrv(g_seqs[random() % NSEQ].s) : 0), mask = 0xffffffff; break;
    case 89: r = at<F0>(0x48fe30)(e), mask = 0xffffffff; break;
    case 90: at<F1>(0x48fe40)(e, random() % 3 ? random() % 14 : random()); break;
    case 91: r = at<F1>(0x48fe80)(e, random() % 5 ? ptrv(g_seqs[random() % NSEQ].s) : 0), mask = 0xffffffff; break;
    case 92: case 93: {
        a->frame = bounded_float();
        if (random() % 2)
            ensure_sequence(i);
        at<F1>(op == 92 ? 0x48feb0 : 0x48ff90)(e, bits(bounded_float()));
        break;
    }
    case 94: at<F0>(0x48ffb0)(e); break;
    case 95: {
        a->frame = bounded_float();
        ensure_sequence(i);
        constexpr DWORD V[] = {0x3f800000, 0, 0x3f000000, 0x3f7fffff, 0xbf000000};
        at<F1>(0x48ffc0)(e, random() % 2 ? V[random() % 5] : bits((float)(random() % 200) / 100.0f));
        break;
    }
    }
    return r;
}

// ---- The arena of each side (as in checks_action_classes.cpp) ----------------------------------------------

constexpr DWORD ARENA_SIZE = 0x100000;
constexpr int MAX_ALLOCS = 1024;
struct Alloc {
    DWORD offset, size;
};
BYTE *g_arena[2];
BYTE *g_copy;
DWORD g_used[2], g_high;
Alloc g_allocs[2][MAX_ALLOCS];
int g_nallocs[2];
int g_arena_side = -1;
DWORD g_thread;
BYTE *g_free_trampoline;
CodePatch g_new_patch, g_free_patch;

bool in_arena(DWORD p, int side) { return g_arena[side] && p >= ptrv(g_arena[side]) && p < ptrv(g_arena[side]) + ARENA_SIZE; }

void *__cdecl arena_new(DWORD n)
{
    const int s = g_arena_side;
    // a new pool chunk stays in the C heap (check_util.h, pool_grow_call)
    if (s >= 0 && GetCurrentThreadId() == g_thread && !pool_grow_call((DWORD)(uintptr_t)__builtin_return_address(0), n)) {
        const DWORD size = (n + 7) & ~7u;
        if (g_used[s] + size + 8 <= ARENA_SIZE && g_nallocs[s] < MAX_ALLOCS) {
            BYTE *p = g_arena[s] + g_used[s];
            g_allocs[s][g_nallocs[s]++] = {g_used[s], n};
            g_used[s] += size ? size : 8;
            return p;
        }
    }
    return at<void *(__cdecl *)(DWORD, int)>(0x5d1661)(n, 1);   // __nh_malloc, as operator new
}
DWORD __cdecl arena_free(void *p)
{
    if (in_arena(ptrv(p), 0) || in_arena(ptrv(p), 1))
        return 0;
    return reinterpret_cast<DWORD(__cdecl *)(void *)>(g_free_trampoline)(p);
}
bool arena_setup()
{
    if (g_arena[0])
        return true;
    g_arena[0] = static_cast<BYTE *>(VirtualAlloc(nullptr, ARENA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_arena[1] = static_cast<BYTE *>(VirtualAlloc(nullptr, ARENA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    check_region(g_arena[0], ARENA_SIZE, "entities arena 0");
    check_region(g_arena[1], ARENA_SIZE, "entities arena 1");
    g_copy = static_cast<BYTE *>(VirtualAlloc(nullptr, ARENA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_free_trampoline = static_cast<BYTE *>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!g_arena[0] || !g_arena[1] || !g_copy || !g_free_trampoline)
        return false;
    // free's first instructions (push ebp; mov ebp, esp; push -1), then back to 0x5d33e5
    const BYTE first[] = {0x55, 0x8b, 0xec, 0x6a, 0xff};
    if (memcmp(original_address(0x5d33e0), first, 5) != 0)   // free is hooked (crt_heap.cpp): its trampoline holds the original bytes
        return false;
    memcpy(g_free_trampoline, first, 5);
    g_free_trampoline[5] = 0xe9;
    *(INT32 *)(g_free_trampoline + 6) = (INT32)(0x5d33e5 - (ptrv(g_free_trampoline) + 10));
    g_thread = GetCurrentThreadId();
    return true;
}
void arena_on(int side, BYTE pattern, bool fresh = true)
{
    if (fresh) {
        const DWORD n = g_high + 0x10000 < ARENA_SIZE ? g_high + 0x10000 : ARENA_SIZE;
        memset(g_arena[side], pattern, n);
        g_used[side] = 0;
        g_nallocs[side] = 0;
    }
    g_arena_side = side;
    patch_jump(g_new_patch, 0x5d033a, (void *)&arena_new);
    patch_jump(g_free_patch, 0x5d33e0, (void *)&arena_free);
}
void arena_off()
{
    unpatch(g_free_patch);
    unpatch(g_new_patch);
    if (g_arena_side >= 0 && g_used[g_arena_side] > g_high)
        g_high = g_used[g_arena_side];
    g_arena_side = -1;
}

const char *buffer_text(DWORD p)
{
    if (p < 0x10000 || (p & 3) || !readable(at<void *>(p), 12))
        return nullptr;
    const char *t = at<const char *>(p + 8);
    for (DWORD i = 0; i < 0x400; i++) {
        if ((i & 0xff) == 0 && !readable(t + i, 0x100))
            return nullptr;
        if (!t[i])
            return t;
    }
    return nullptr;
}
bool same_text(const char *a, const char *b) { return a && b && lstrcmpA(a, b) == 0; }
bool same_value(DWORD a, DWORD b)
{
    if (a == b)
        return true;
    if (in_arena(a, 0) && in_arena(b, 1))
        return a - ptrv(g_arena[0]) == b - ptrv(g_arena[1]);
    return same_text(buffer_text(a), buffer_text(b));
}
DWORD g_diff_at, g_diff_a, g_diff_b;
bool same_region(const BYTE *a, const BYTE *b, DWORD n)
{
    for (DWORD i = 0; i + 4 <= n; i += 4) {
        const DWORD x = *reinterpret_cast<const DWORD *>(a + i), y = *reinterpret_cast<const DWORD *>(b + i);
        if (!same_value(x, y)) {
            g_diff_at = i;
            g_diff_a = x;
            g_diff_b = y;
            return false;
        }
    }
    return true;
}
DWORD g_fake_class[2];
DWORD rel(DWORD v, int side)
{
    if (in_arena(v, side))
        return 0xa0000000u + (v - ptrv(g_arena[side]));
    if (v && v == g_fake_class[side])
        return 0xc1a55000u;
    return v;
}

// ---- Objects ------------------------------------------------------------------------------------------------

struct ClassDef {
    const char *name;
    DWORD size, var, getter, getter2, create, create_array, register_fields, ctor, dtor;
};
constexpr ClassDef CLASSES[] = {
    {"CEntityBase", 0x7c, CLASS_ENTITY_BASE, 0x487620, 0x487a40, 0x487670, 0x4877f0, 0x487aa0, 0x487ff0, 0x488150},
    {"CEntityDestructable", 0x90, CLASS_ENTITY_DESTRUCTABLE, 0x48bdf0, 0x48c2c0, 0x48bea0, 0x48c030, 0x48c380, 0x48c890, 0x48ca00},
    {"CEntityAnimated", 0xe4, CLASS_ENTITY_ANIMATED, 0x48dcc0, 0x48de10, 0x48dd20, 0x48dd40, 0x48de80, 0x48e920, 0x48ea40}};

using Create0 = void *(__cdecl *)();
using CreateN = void *(__fastcall *)(int, int, int);
BYTE g_obj[2][0x100];

void side_begin(int side)
{
    g_side = side;
    if (side == 0)
        hooks_out();
}
void side_end(int side)
{
    if (side == 0)
        hooks_in();
}

// Create on a pool block filled with a pattern (all bytes), deleted through its own slot 1
void test_create(Tally &t, const ClassDef &c, unsigned seed)
{
    bool reused[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        BYTE *p = static_cast<BYTE *>(pool_alloc(c.size));
        memset(p, (BYTE)random(), c.size);
        pool_free(p, c.size);
        BYTE *o = static_cast<BYTE *>(at<Create0>(c.create)());
        reused[side] = o == p;
        memcpy(g_obj[side], o, c.size);
        vcall<void *>(o, 1, 1);
        side_end(side);
    }
    char d[160];
    const bool same = reused[0] && reused[1] && same_region(g_obj[0], g_obj[1], c.size);
    wsprintfA(d, "[Create seed %u: block reused %d / %d, first difference +%x: %x / %x]", seed, reused[0], reused[1], g_diff_at, g_diff_a,
              g_diff_b);
    t.check(same, c.name, d);
}

// CreateArray with 0-3 elements from the arena (the whole block), destroyed through slot 1 (array, not freed)
void test_create_array(Tally &t, const ClassDef &c, int count, BYTE pattern)
{
    DWORD e[2];
    void *a[2];
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        arena_on(side, pattern);
        a[side] = at<CreateN>(c.create_array)(count, 0, 0);
        e[side] = rel(ptrv(a[side]), side);
        arena_off();
        side_end(side);
    }
    char d[200];
    const bool same = g_used[0] == g_used[1] && e[0] == e[1] && same_region(g_arena[0], g_arena[1], g_used[0]);
    wsprintfA(d, "[CreateArray %d: %x / %x, %u / %u bytes, first difference +%x: %x / %x]", count, e[0], e[1], g_used[0], g_used[1],
              g_diff_at, g_diff_a, g_diff_b);
    t.check(same, c.name, d);
    for (int side = 0; side < 2; side++)
        if (count > 0 && a[side]) {
            side_begin(side);
            arena_on(side, pattern, false);
            const DWORD r = rel(ptrv(vcall<void *>(a[side], 1, 2)), side);
            arena_off();
            side_end(side);
            e[side] = r;
        }
    if (count > 0) {
        wsprintfA(d, "[array deleted %d: eax %x / %x, first difference +%x]", count, e[0], e[1], g_diff_at);
        t.check(e[0] == e[1] && same_region(g_arena[0], g_arena[1], g_used[0]), c.name, d);
    }
}

// The out-of-line constructor on a pool block filled with a pattern, then the destructor (eax, all bytes
// after each)
void test_ctor_dtor(Tally &t, const ClassDef &c, unsigned seed)
{
    DWORD r[2];
    BYTE after[2][0x100];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        BYTE *o = static_cast<BYTE *>(pool_alloc(c.size));
        memset(o, (BYTE)random(), c.size);
        r[side] = at<DWORD(THISCALL *)(void *)>(c.ctor)(o) == ptrv(o);
        memcpy(g_obj[side], o, c.size);
        at<void(THISCALL *)(void *)>(c.dtor)(o);
        memcpy(after[side], o, c.size);
        pool_free(o, c.size);
        side_end(side);
    }
    char d[160];
    bool same = r[0] && r[1] && same_region(g_obj[0], g_obj[1], c.size);
    wsprintfA(d, "[ctor seed %u: eax this %d / %d, first difference +%x: %x / %x]", seed, r[0], r[1], g_diff_at, g_diff_a, g_diff_b);
    t.check(same, c.name, d);
    same = same_region(after[0], after[1], c.size);
    wsprintfA(d, "[dtor seed %u: first difference +%x: %x / %x]", seed, g_diff_at, g_diff_a, g_diff_b);
    t.check(same, c.name, d);
}

// An animated entity with a creator (listened to through the real broadcaster) and attached objects
// (pool CHueRemapHolders deleted through slot 1) deleted; the creator's listeners compared after
void test_animated_dtor(Tally &t, unsigned seed)
{
    DWORD s[2][4];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        Animated *creator = static_cast<Animated *>(at<Create0>(0x48dd20)());
        Animated *o = static_cast<Animated *>(at<Create0>(0x48dd20)());
        if (random() % 4)
            at<void(THISCALL *)(void *, void *)>(0x48f1a0)(o, creator);
        const DWORD n = random() % 4;
        if (n) {
            o->attached.data = static_cast<void **>(pool_get_alloc(n * 4));
            o->attached.capacity = n;
            o->attached.count = n;
            for (DWORD k = 0; k < n; k++)
                o->attached.data[k] = random() % 4 ? at<void *(THISCALL *)(void *)>(0x4d4cc0)(pool_alloc(0x20)) : nullptr;
        }
        const BYTE *bc = creator->broadcaster;
        s[side][0] = bc ? *reinterpret_cast<const DWORD *>(bc + 0x10) : 0xffffffff;
        s[side][1] = ptrv(vcall<void *>(o, 1, 1)) == ptrv(o);
        bc = creator->broadcaster;
        s[side][2] = bc ? *reinterpret_cast<const DWORD *>(bc + 0x10) : 0xffffffff;
        s[side][3] = n;
        vcall<void *>(creator, 1, 1);
        side_end(side);
    }
    char d[160];
    wsprintfA(d, "[animated dtor seed %u: listeners %d -> %d / %d -> %d]", seed, s[0][0], s[0][2], s[1][0], s[1][2]);
    t.check(memcmp(s[0], s[1], sizeof s[0]) == 0 && s[0][1], "CEntityAnimated", d);
}

// ---- Class getters and RegisterFields ------------------------------------------------------------------------

struct RegisterCall {
    DWORD var, name, version, parent, size, abstract, create, create_array, register_fields;
};
RegisterCall g_register;
int g_nregister;
bool g_inherit;
CClassInfo *THISCALL fake_register(CClassInfo *self, CClassInfo **var, const char *name, float version, CClassInfo *parent,
                                   DWORD size, DWORD abstract, void *create, void *create_array, void *register_fields)
{
    memset(self, 0, sizeof *self);
    self->name = name;
    self->parent = parent;
    *var = self;
    g_register = {ptrv(var), ptrv(name), bits(version), ptrv(parent), size, abstract & 0xff, ptrv(create), ptrv(create_array),
                  ptrv(register_fields)};
    g_nregister++;
    if (g_inherit)
        at<void(THISCALL *)(CClassInfo *)>(0x415690)(self);   // InheritFields
    return self;
}

void test_getter(Tally &t, const ClassDef &c, DWORD getter)
{
    CClassInfo *&var = *at<CClassInfo **>(c.var);
    CClassInfo *const saved = var;
    RegisterCall calls[2];
    DWORD results[2], set[2];
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        set[side] = ptrv(at<CClassInfo *(__cdecl *)()>(getter)());
        var = nullptr;
        g_nregister = 0;
        g_inherit = false;
        memset(&g_register, 0, sizeof g_register);
        CodePatch p;
        patch_jump(p, 0x415780, (void *)&fake_register);
        void *made = at<CClassInfo *(__cdecl *)()>(getter)();
        unpatch(p);
        results[side] = (made == var ? 1 : 0) + (DWORD)g_nregister * 2;
        calls[side] = g_register;
        if (var)
            pool_free(var, sizeof(CClassInfo));
        var = saved;
        side_end(side);
    }
    char d[180];
    wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, size %x / %x, version %x / %x, parent %x / %x]", getter, set[0], set[1],
              results[0], results[1], calls[0].size, calls[1].size, calls[0].version, calls[1].version, calls[0].parent,
              calls[1].parent);
    t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
            c.name, d);
}

struct Made {
    CClassInfo *cls;
    alignas(4) BYTE fake[sizeof(CClassInfo)];
    DWORD result, cache, nregister;
    RegisterCall call;
};
Made g_made[2];
void run_register_fields(const ClassDef &c, bool var_zero, int side, BYTE pattern)
{
    Made &m = g_made[side];
    CClassInfo *&var = *at<CClassInfo **>(c.var);
    CClassInfo *const real = var;
    *at<DWORD *>(FIND_FIELD_CACHE) = 0;
    arena_on(side, pattern);
    memset(m.fake, 0, sizeof m.fake);
    CClassInfo *fake = reinterpret_cast<CClassInfo *>(m.fake);
    if (var_zero)
        var = nullptr;
    else {
        // without the inherited fields: InheritFields clones them (the enum field "Team Number" with a
        // new pool copy of its names), which is not the code under test
        fake->name = real->name;
        fake->parent = real->parent;
        var = fake;
    }
    g_nregister = 0;
    g_inherit = false;
    memset(&g_register, 0, sizeof g_register);
    CodePatch p;
    patch_jump(p, 0x415780, (void *)&fake_register);
    g_fake_class[side] = ptrv(var);
    const DWORD r = at<DWORD(__cdecl *)()>(c.register_fields)();
    g_fake_class[side] = ptrv(var);
    unpatch(p);
    m.cls = var;
    m.result = rel(r, side);
    m.cache = *at<DWORD *>(FIND_FIELD_CACHE);
    m.nregister = (DWORD)g_nregister;
    m.call = g_register;
    var = real;
    arena_off();
}
void free_made(int side)
{
    Made &m = g_made[side];
    arena_on(side, 0, false);
    for (DWORD i = 0; i < m.cls->num_fields; i++)
        if (m.cls->fields[i])
            vcall<void *>(m.cls->fields[i], 0, 0);
    arena_off();
    if (ptrv(m.cls) != ptrv(m.fake))
        pool_free(m.cls, sizeof(CClassInfo));
}
void class_state(int side, DWORD *out)
{
    const CClassInfo *k = g_made[side].cls;
    out[0] = k->num_fields;
    out[1] = k->num_inherited;
    out[2] = ptrv(k->alt_name);
    out[3] = ptrv(k->name);
    out[4] = ptrv(k->parent);
    out[5] = rel(ptrv(k->fields), side);
    out[6] = g_made[side].result;
    out[7] = g_made[side].cache;
    out[8] = g_made[side].nregister;
}
bool unwritten(DWORD offset, BYTE p, BYTE q) { return g_copy[offset] == q && g_arena[1][offset] == p; }
int find_alloc(DWORD p)
{
    if (!in_arena(p, 1))
        return -1;
    const DWORD off = p - ptrv(g_arena[1]);
    for (int i = 0; i < g_nallocs[1]; i++)
        if (g_allocs[1][i].offset == off)
            return i;
    return -1;
}
bool same_registered(const BYTE *real, DWORD made, BYTE p, BYTE q)
{
    const int k = find_alloc(made);
    if (k < 0 || !readable(real, g_allocs[1][k].size))
        return false;
    const DWORD base = g_allocs[1][k].offset, size = g_allocs[1][k].size;
    const BYTE *m = at<const BYTE *>(made);
    for (DWORD i = 0; i + 4 <= size; i += 4) {
        int written = 0;
        for (DWORD b = 0; b < 4; b++)
            written += !unwritten(base + i + b, p, q);
        const DWORD x = *reinterpret_cast<const DWORD *>(real + i), y = *reinterpret_cast<const DWORD *>(m + i);
        if (!written || x == y)
            continue;
        g_diff_at = i;
        g_diff_a = x;
        g_diff_b = y;
        if (written < 4) {
            for (DWORD b = 0; b < 4; b++)
                if (!unwritten(base + i + b, p, q) && real[i + b] != m[i + b])
                    return false;
            continue;
        }
        if (!same_text(buffer_text(x), buffer_text(y)))
            return false;
    }
    return true;
}

void test_register_fields(Tally &t, Tally &real, const ClassDef &c, unsigned seed)
{
    const DWORD saved_cache = *at<DWORD *>(FIND_FIELD_CACHE);
    g_seed = seed;
    const BYTE p = (BYTE)(random() | 1), q = (BYTE)~p;
    for (int var_zero = 0; var_zero < 2; var_zero++) {
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            run_register_fields(c, var_zero != 0, side, p);
            side_end(side);
        }
        DWORD s0[9], s1[9];
        class_state(0, s0);
        class_state(1, s1);
        bool same = g_used[0] == g_used[1] && g_nallocs[0] == g_nallocs[1] && same_region(g_arena[0], g_arena[1], g_used[0]) &&
                    memcmp(s0, s1, sizeof s0) == 0 && memcmp(&g_made[0].call, &g_made[1].call, sizeof g_made[0].call) == 0 &&
                    s0[0] > 0;
        char d[220];
        wsprintfA(d, "[RegisterFields var %d: %u / %u fields (%u inherited), %u / %u bytes, eax %x / %x, first difference +%x: %x / %x]",
                  var_zero, s0[0], s1[0], s0[1], g_used[0], g_used[1], s0[6], s1[6], g_diff_at, g_diff_a, g_diff_b);
        if (!same && t.mismatches < 4)
            log("    class state %x %x %x %x %x %x %x %x %x / %x %x %x %x %x %x %x %x %x", s0[0], s0[1], s0[2], s0[3], s0[4], s0[5], s0[6],
                s0[7], s0[8], s1[0], s1[1], s1[2], s1[3], s1[4], s1[5], s1[6], s1[7], s1[8]);
        t.check(same, c.name, d);
        free_made(0);
        free_made(1);
        if (var_zero)
            continue;
        // The registered descriptors against side 1's; bytes equal to the pattern of two runs were not written
        run_register_fields(c, false, 1, q);
        const DWORD used_q = g_used[1];
        memcpy(g_copy, g_arena[1], used_q);
        free_made(1);
        run_register_fields(c, false, 1, p);
        const CClassInfo *k = *at<CClassInfo **>(c.var);
        const CClassInfo *m = g_made[1].cls;
        // the class's own fields (after the inherited ones of the registered class)
        bool same_real = k && used_q == g_used[1] && k->num_fields - k->num_inherited == m->num_fields;
        const DWORD inh = k ? k->num_inherited : 0;
        DWORD bad = 0xffffffff;
        for (DWORD i = 0; same_real && i < m->num_fields; i++)
            if (!same_registered(reinterpret_cast<const BYTE *>(k->fields[inh + i]), ptrv(m->fields[i]), p, q)) {
                same_real = false;
                bad = i;
            }
        wsprintfA(d, "[registered: %u fields (%u inherited), made %u; field %d, +%x: %x / %x]", k ? k->num_fields : 0,
                  k ? k->num_inherited : 0, m->num_fields, (int)bad, g_diff_at, g_diff_a, g_diff_b);
        if (!same_real && bad != 0xffffffff) {
            const CFieldDesc *x = k->fields[inh + bad], *y = m->fields[bad];
            const DWORD *xd = reinterpret_cast<const DWORD *>(x), *yd = reinterpret_cast<const DWORD *>(y);
            log("    field %u '%s' vt %x / %x: %x %x %x %x %x / %x %x %x %x %x", bad, x->name.buffer ? x->name.c_str() : "?", xd[0], yd[0],
                xd[14], xd[15], xd[16], xd[17], xd[18], yd[14], yd[15], yd[16], yd[17], yd[18]);
        }
        real.check(same_real, c.name, d);
        free_made(1);
    }
    *at<DWORD *>(FIND_FIELD_CACHE) = saved_cache;
}

// ---- Real data: the entities of the running level ---------------------------------------------------------

constexpr int MAX_REAL = 3000;
LayerPart *g_real[MAX_REAL];
int g_nreal;

void add_real(LayerPart *p, int depth)
{
    if (!p || g_nreal >= MAX_REAL || !readable(p, 0xe4) || !class_of(p))
        return;
    for (int j = 0; j < g_nreal; j++)
        if (g_real[j] == p)
            return;
    if (is_kind_real(p, EntityBase_ClassInfo()))
        g_real[g_nreal++] = p;
    const LPChildList *l = p->children;
    if (depth < 4 && l && readable(l, sizeof *l) && l->count < 1000 && readable(l->data, l->count * 4))
        for (DWORD k = 0; k < l->count; k++)
            add_real(l->data[k], depth + 1);
}
bool find_real()
{
    g_nreal = 0;
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (!world)
        return false;
    BYTE *layer = at<BYTE *(THISCALL *)(void *)>(0x4f1050)(world);
    if (!layer || !readable(layer + 0x50, 0x28))
        return false;
    const DWORD columns = *reinterpret_cast<DWORD *>(layer + 0x60), rows = *reinterpret_cast<DWORD *>(layer + 0x64);
    const ZPtrArray *cells = *reinterpret_cast<ZPtrArray **>(layer + 0x68);
    if (columns > 4096 || rows > 4096 || !cells || !readable(cells, columns * rows * 16))
        return false;
    for (DWORD i = 0; i < columns * rows; i++) {
        const ZPtrArray *c = &cells[i];
        if (c->count > 10000 || !readable(c->data, c->count * 4))
            return false;
        for (DWORD k = 0; k < c->count; k++)
            add_real(static_cast<LayerPart *>(c->data[k]), 0);
    }
    return true;
}

// The model of a real entity is loaded (so no check loads one)
bool model_loaded(const Entity *e)
{
    const BYTE *r = *at<const BYTE **>(MODEL_RECORDS);
    if (!r || e->model >= *at<DWORD *>(MODEL_COUNT))
        return false;
    return *reinterpret_cast<void *const *>(r + e->model * 0x80 + 0x78) && !r[e->model * 0x80 + 0x7f];
}

struct RealPatches {
    CodePatch p[4];
    void on()
    {
        patch_jump(p[0], 0x51ff50, (void *)&rec_block_type);
        patch_jump(p[1], 0x520940, (void *)&rec_write_dword);
        patch_jump(p[2], 0x5209c0, (void *)&rec_write_word);
        patch_jump(p[3], 0x520820, (void *)&rec_write_byte);
    }
    void off()
    {
        for (int i = 3; i >= 0; i--)
            unpatch(p[i]);
    }
};

void test_real(Tally &t, Tally &blocks)
{
    if (!find_real()) {
        log("  entities real data: no level grid found");
        t.check(false, "entities", "[no level grid]");
        return;
    }
    // At most REAL_CAP entities per kind (plain, destructable, animated): Assimilate Cave1 has 1056
    // with a loaded model, 33660 calls and 38 s for all of them (2026-10-25)
    constexpr int REAL_CAP = 100;
    int calls = 0, nblocks = 0, nanimated = 0, kind_count[3] = {}, kind_index[3] = {}, ntested = 0;
    for (int n = 0; n < g_nreal; n++) {
        Entity *e = static_cast<Entity *>(g_real[n]);
        if (model_loaded(e))
            kind_count[is_kind_real(e, EntityAnimated_ClassInfo()) ? 2 : is_kind_real(e, EntityDestructable_ClassInfo())]++;
    }
    for (int n = 0; n < g_nreal; n++) {
        Entity *e = static_cast<Entity *>(g_real[n]);
        if (!model_loaded(e))
            continue;
        const bool destructable = is_kind_real(e, EntityDestructable_ClassInfo());
        const bool animated = is_kind_real(e, EntityAnimated_ClassInfo());
        nanimated += animated;
        const int kind = animated ? 2 : destructable;
        if (!spread_pick(kind_index[kind]++, kind_count[kind], REAL_CAP))
            continue;
        ntested++;
        const int nops = animated ? 40 : destructable ? 24 : 20;
        for (int op = 0; op < nops; op++) {
            calls++;
            two_sides(t, "real entity", (unsigned)(n * 64 + op), [&](DWORD &mask) -> DWORD {
                mask = 0xffffffff;
                Animated *a = static_cast<Animated *>(e);
                switch (op) {
                case 0: return at<F0>(0x487240)(e);
                case 1: return at<F0>(0x487250)(e);
                case 2: return at<F0>(0x487260)(e);
                case 3: return bits((float)at<FL0>(0x487270)(e));
                case 4: return text_hash(reinterpret_cast<const char *>(at<F0>(0x48a7a0)(e)));
                case 5: return at<F0>(0x48abf0)(e);
                case 6: return at<F0>(0x48aed0)(e);
                case 7: return text_hash(reinterpret_cast<const char *>(at<F0>(0x48aee0)(e)));
                case 8: return at<F0>(0x48b000)(e);
                case 9: return at<F0>(0x48b290)(e);
                case 10: return at<F0>(0x48b560)(e);
                case 11: return at<F0>(0x48b590)(e);
                case 12: return at<F0>(0x48b860)(e);
                case 13: mask = 0xff; return at<F0>(0x48bb20)(e);
                case 14: return at<F2>(0x48bc90)(e, ptrv(g_classes[5 + op % 2]), 0);
                case 15: return at<F2>(0x48bc90)(e, ptrv(at<CClassInfo *(__cdecl *)()>(0x42f310)()), 0);
                case 16: return at<F1>(0x48d8f0)(at<void *>(MODEL_LIST), e->model);
                case 17: return at<F1>(0x48b570)(e, 0);
                case 18: return at<F0>(0x48aed0)(e) == e->sequence;
                case 19: {
                    // the multiplayer block into recorders (entities without children and attached objects)
                    const DWORD f = (*reinterpret_cast<DWORD *const *>(e))[25];
                    if ((f != 0x4893b0 && f != 0x48ec70) || (e->children && e->children->count) ||
                        (animated && (a->attached.count || !at<F0>(0x48b590)(e))))
                        return 0;
                    nblocks += g_side;
                    RealPatches p;
                    p.on();
                    const DWORD r = at<F2>(f)(e, ptrv(g_packet), (DWORD)(n & 1));
                    p.off();
                    return r;
                }
                // CEntityDestructable
                case 20: st_ld(at<FL0>(0x48d030)(e)); return 0;
                case 21: st_ld(at<FL0>(0x48d150)(e)); return 0;
                case 22: st_ld(at<FL0>(0x48d260)(e)); return 0;
                case 23: st_ld(at<FL2>(0x48d6e0)(e, bits(12.5f), 1)); return 0;
                // CEntityAnimated
                case 24: return bits((float)at<FL0>(0x48e9b0)(e));
                case 25: return at<F0>(0x48e9c0)(e);
                case 26: return at<F1>(0x48e9d0)(e, 0);
                case 27: return at<F0>(0x48ebb0)(e) | at<F0>(0x48ec20)(e) << 1 | at<F0>(0x48ec30)(e) << 2 | at<F0>(0x48fc40)(e) << 3;
                case 28: return at<F0>(0x48f180)(e);
                case 29: return at<F0>(0x48f190)(e);
                case 30: return at<F0>(0x48fe30)(e);
                case 31: {
                    const DWORD saved = a->aflags;
                    const DWORD r = at<F0>(0x48f260)(e) & 0xff;
                    st(a->aflags);
                    a->aflags = saved;
                    return r;
                }
                case 32: {
                    const DWORD saved = a->aflags;
                    st_ld(at<FL0>(0x48fc50)(e));
                    st(a->aflags);
                    a->aflags = saved;
                    return 0;
                }
                case 33: {
                    LayerPart *root = e;
                    while (root->parent)
                        root = root->parent;
                    if (!is_kind_real(root, EntityAnimated_ClassInfo()))
                        return 0;
                    return bits((float)at<FL0>(0x48f510)(e));
                }
                case 34: {
                    if (!at<F0>(0x48b590)(e))
                        return 0;
                    st_ld(at<FL0>(0x48f600)(e));
                    return 0;
                }
                case 35: return at<F1>(0x48fe80)(e, at<F0>(0x48b590)(e));
                case 36: {
                    const DWORD s = at<F0>(0x48b590)(e);
                    return s && *at<DWORD *>(s + 0x48) != 1 && !(*at<DWORD *>(s + 0x1c) & 0x80) ? at<F1>(0x48fd80)(e, s) : 0;
                }
                case 37: return ptrv(a->created_by);
                case 38: return a->frame_index;
                default: return a->aflags;
                }
            });
        }
    }
    log("  entities real data: %d entities (%d animated; %d plain, %d destructable, %d animated with a model), %d tested, "
        "%d calls, %d multiplayer blocks", g_nreal, nanimated, kind_count[0], kind_count[1], kind_count[2], ntested, calls, nblocks);
    blocks.check(nblocks > 0 || g_nreal == 0, "entities", "[no multiplayer block written]");
}

}  // namespace

ZAX_CHECKS(entities)
{
    (void)cls;
    (void)n;
    init_tables();
    // Warm-up: message ids created once outside the compared runs
    at<DWORD(__fastcall *)(const char *)>(0x506680)("Is Upper Body Present");
    at<DWORD(__fastcall *)(const char *)>(0x506680)("Query AI Type");
    if (!arena_setup()) {
        log("  entities: no arena");
        return;
    }
    Tally slots{"entities: slots on fake entities"}, objects{"entities: Create, CreateArray, constructors, destructors"},
        classes{"entities: class getters and RegisterFields"}, registered{"entities: registered descriptors (real data)"},
        real{"entities: entities of the running level (real data)"}, blocks{"entities: multiplayer blocks of the running level"};
    log("  entities: slots");
    g_patches.on();
    g_op = -1;
    for (unsigned seed = 1; seed <= (unsigned)scaled(3000); seed++)
        two_sides(slots, "slot", seed, [](DWORD &mask) { return slot_case(mask); });
    for (int op = 0; op < NOPS; op++)
        for (int k = 0; k < scaled(12); k++) {
            g_op = op;
            two_sides(slots, "slot op", (unsigned)(100000 + op * 100 + k), [](DWORD &mask) { return slot_case(mask); });
        }
    g_op = -1;
    g_patches.off();
    slots.report();
    log("  entities: objects");
    for (const ClassDef &c : CLASSES) {
        for (unsigned seed = 1; seed <= (unsigned)scaled(40); seed++) {
            test_create(objects, c, seed);
            test_ctor_dtor(objects, c, seed);
        }
        for (int count = 0; count < 4; count++)
            for (int pattern = 0; pattern < 2; pattern++)
                test_create_array(objects, c, count, pattern ? 0xa5 : 0x3c);
    }
    for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++)
        test_animated_dtor(objects, seed);
    objects.report();
    log("  entities: classes");
    for (const ClassDef &c : CLASSES) {
        test_getter(classes, c, c.getter);
        test_getter(classes, c, c.getter2);
        test_register_fields(classes, registered, c, 7);
    }
    classes.report();
    registered.report();
    log("  entities: real data");
    test_real(real, blocks);
    real.report();
    blocks.report();
}

}  // namespace zax
