// Checks of the entity classes above CEntityAnimated (entity_classes.cpp) against the original, in the game
// thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls run the original too), side 1 through the
// reimplementation. Compared: eax (masked where the original leaves undefined bits; st(0) of the float
// functions as 10 bytes), an event log and the state of every fake afterwards. Pointers are compared
// raw first and only mapped (fakes by number and offset, the stack as one tag) when they differ.
//  - Slot scenarios on fake entities: objects of 0x130 bytes (CEntityWalking's size) whose vtable is a
//    table of recorder thunks (every slot logs its arguments and returns a configured or hashed value),
//    fake inventories, AIs, attached objects, a fake world (0x6c2080), fake sequences and models; the
//    exe functions that reach further (CEntityAnimated's update, death and blocks, CEntityBase's position
//    and blocks, the layer's sweeps and the bounce, the model and sequence lookups, the rotation
//    sequence's motion, hit points, the behaviours, the inventory factory, the network packet, the strip
//    builder 0x493850) jump to recorders. The sweep set-up (0x545a00...), the angle tables, the kind
//    test, the AI list and the root are the exe's own.
//  - The fatal bounce factor of slot 94 (the message box and exit replaced by stand-ins).
//  - Objects: Create, CreateArray (heap blocks from an arena filled with a pattern) and the deleting
//    destructors of the seven concrete classes, the out-of-line constructors and destructors on pool
//    blocks filled with a pattern, CEntityCharacter's deleting destructor (with the local character),
//    the line reader's create function and the CEntityAnimated converter on real objects.
//  - Class getters and RegisterFields (ClassInfo_Register replaced by a stand-in; descriptors from the
//    arena compared byte by byte, with the descriptors registered at startup).
//  - Real data: copies of the entities of the running level (its layer grid and their children): the
//    read-only functions, the discharge turn, the bounce filter for pairs, the converter's test, and the
//    multiplayer blocks of characters and lines written into recorders.
#include "check_util.h"
#include "entity_classes.h"
#include "models.h"
#include "field_desc.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT DWORD WINAPI GetCurrentThreadId(void);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);

namespace zax {

int hooks_entity_classes(Hook *out);

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
CClassInfo *getter(DWORD a) { return at<CClassInfo *(__cdecl *)()>(a)(); }

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
DWORD g_dump[2][80];
int g_ndump[2];
void dump(DWORD v)
{
    if (g_ndump[g_side] < 80)
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
// The code pages of the hooks stay writable for the module's checks (two VirtualProtect calls per hook
// and case made the behaviour checks slow)
DWORD g_old_protect;
bool make_pages_writable()
{
    const int n = hooks_entity_classes(nullptr);
    if (!hook_sites_fit("entity_classes", n, MAX_SITES))
        return false;
    hooks_entity_classes(g_hook_list);
    return VirtualProtect(at<void *>(0x490000), 0xd000, PAGE_EXECUTE_READWRITE, &g_old_protect) != 0;
}
void restore_pages()
{
    DWORD old;
    VirtualProtect(at<void *>(0x490000), 0xd000, g_old_protect, &old);
}
void hooks_out()
{
    g_nsites = 0;
    const int n = hooks_entity_classes(nullptr);
    if (!hook_sites_fit("entity_classes", n, MAX_SITES))
        return;
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
        memcpy(site, g_hook_list[i].expected, 5);
    }
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x490000), 0xd000);
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++)
        memcpy(g_sites[i].site, g_sites[i].bytes, 5);
    FlushInstructionCache(GetCurrentProcess(), at<void *>(0x490000), 0xd000);
    g_nsites = 0;
}

DWORD random_float()
{
    constexpr DWORD VALUES[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x3f000000, 0xbf000000, 0x3fc00000, 0x40200000,
                                0xc0200000, 0x7fc00000, 0x7f800000, 0xff800000, 0x00000001, 0x7f7fffff, 0x4f000000,
                                0xcf000000, 0x447a0000, 0x40490fdb, 0xc0490fdb, 0x40c90fdb, 0x40c90fda, 0x3f7fbe77,
                                0x41200000, 0xc1200000, 0x41200001, 0x42c80000, 0x3c23d70a, 0x43fa0000, 0x7fa00000};
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
float bounded_float() { return (float)((int)(random() % 4000) - 1000) / 7.0f; }
DWORD small_float() { return random() % 4 ? bits(bounded_float()) : random_float(); }

// ---- Fakes and the recorder thunks -----------------------------------------------------------------------

enum { T_ENT, T_OBJ, T_WORLD, T_ARRAY, NTABLES };
constexpr int NSLOTS = 136;
constexpr int NENT = 8, NOBJ = 10, NSEQ = 4, NMODEL = 3, NCLASS = 12;

struct FakeEnt {
    BYTE o[0x130];      // the object (CEntityWalking's size)
    DWORD cls;          // slot 2: an index into g_classes
    DWORD seq;          // Entity_SequenceObject: a fake sequence + 1 or 0
    DWORD model;        // Entity_Model: a fake model
    DWORD k54, k85;     // the int slots
    DWORD f46, f55, f84;   // the float slots
    BYTE opaque;        // slot 77
    BYTE pad[3];
    float px, py;       // slot 43 (position) and LayerPart_Position
    DWORD hp, hp_left;  // the hit point queries (float bits)
    DWORD deleted;
};
struct FakeObj {
    DWORD vftable;
    DWORD dynamic;
    DWORD more[8];
    DWORD cls;
    BYTE flag39;
    BYTE pad[3];
    DWORD deleted;
};
struct FakeSeq {
    BYTE s[0x70];
    int motion[4];
};

FakeEnt g_ents[NENT];
FakeObj g_objs[NOBJ];
FakeObj g_world;
FakeSeq g_seqs[NSEQ];
BYTE g_models[NMODEL][0xc4];
alignas(4) BYTE g_layer[0x100];
alignas(4) BYTE g_packet[0x40];
alignas(4) BYTE g_poly[0x40];
alignas(4) BYTE g_converter[0x20];
void *g_activity[NENT][6];
void *g_attached[NENT][6];
float g_buf[16];
ZString g_names[6];
bool g_names_made;
BYTE *g_ai_block;    // the activity block 0x490060 makes (the stand-in of its constructor keeps it)
DWORD g_rnext;       // the packet reads' stream

extern "C" {
extern char ec_thunks[];
DWORD ec_pop, ec_ret_float, ec_fret;
DWORD __cdecl ec_record(DWORD *args, DWORD id, BYTE *self);
}
__asm__(".text\n"
        ".globl _ec_thunks\n"
        "_ec_thunks:\n"
        ".set ec_i, 0\n"
        ".rept 544\n"
        "  movl $ec_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _ec_common - . - 4\n"
        ".set ec_i, ec_i + 1\n"
        ".endr\n"
        "_ec_common:\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _ec_record\n"
        "  addl $12, %esp\n"
        "  cmpl $0, _ec_ret_float\n"
        "  je 1f\n"
        "  flds _ec_fret\n"
        "1:\n"
        "  movl _ec_pop, %ecx\n"
        "  popl %edx\n"
        "  addl %ecx, %esp\n"
        "  jmp *%edx\n");
DWORD g_vt[NTABLES][NSLOTS];

struct Spec {
    BYTE slot, nargs;
    const char *kinds;   // V value (tagged), F float bits, P two floats pointed to, M message (4 dwords), D the dword pointed to,
                         // R a rectangle written, W two dwords written, O a position written
    char ret;
};
// Returns: i hash, d delete, c class, n small count, k/K the fake's int slots 54/85, a/h/b the float slots
// 46/55/84, u al (the opaque byte for the entity), p 0 or a fake object, y the polygon, v a float buffer
constexpr Spec ENT_SPECS[] = {{1, 1, "V", 'd'},   {2, 0, "", 'c'},      {19, 0, "", 'i'},     {20, 1, "V", 'i'},    {26, 1, "V", 'n'},
                              {27, 1, "V", 'i'},  {37, 3, "RVV", 'i'},  {41, 2, "FF", 'i'},   {43, 1, "O", 'i'},    {46, 0, "", 'a'},
                              {48, 0, "", 'y'},   {54, 0, "", 'k'},     {55, 0, "", 'h'},     {58, 1, "V", 'i'},    {59, 1, "V", 'i'},
                              {77, 1, "V", 'u'},  {78, 0, "", 'i'},     {84, 0, "", 'b'},     {85, 0, "", 'K'},     {86, 1, "V", 'p'},
                              {87, 1, "V", 'p'},  {91, 1, "V", 'i'},    {97, 2, "PP", 'i'},   {98, 4, "FFFF", 'i'}, {99, 3, "FVV", 'i'},
                              {108, 1, "P", 'i'}, {114, 0, "", 'v'},    {116, 2, "WW", 'i'},  {126, 2, "WW", 'i'},  {130, 1, "V", 'i'}};
constexpr Spec OBJ_SPECS[] = {{1, 1, "V", 'd'},   {2, 0, "", 'c'},      {4, 3, "VVV", 'i'},   {19, 0, "", 'i'},     {23, 2, "VV", 'i'},
                              {24, 2, "VV", 'i'}, {25, 2, "VV", 'i'},   {28, 2, "VM", 'i'},   {38, 2, "VV", 'i'},   {39, 0, "", 'u'}};
constexpr Spec WORLD_SPECS[] = {{30, 0, "", 'u'}, {84, 0, "", 'u'}, {85, 1, "V", 'u'}, {91, 2, "VV", 'i'}};
constexpr Spec ARRAY_SPECS[] = {{2, 1, "D", 'i'}};

const Spec *find_spec(DWORD table, DWORD slot)
{
    const Spec *s = nullptr;
    int n = 0;
    switch (table) {
    case T_ENT: s = ENT_SPECS, n = sizeof ENT_SPECS / sizeof *s; break;
    case T_OBJ: s = OBJ_SPECS, n = sizeof OBJ_SPECS / sizeof *s; break;
    case T_WORLD: s = WORLD_SPECS, n = sizeof WORLD_SPECS / sizeof *s; break;
    default: s = ARRAY_SPECS, n = sizeof ARRAY_SPECS / sizeof *s; break;
    }
    for (int i = 0; i < n; i++)
        if (s[i].slot == slot)
            return &s[i];
    return nullptr;
}

// The arenas of the two sides (see "The arena of each side" below); their blocks are tagged by offset
constexpr DWORD ARENA_SIZE = 0x100000;
BYTE *g_arena[2];

DWORD tag(DWORD v)
{
    if (!v)
        return 0;
    // either arena by its offset (a random value inside one of them then tags alike on both sides)
    for (int k = 0; k < 2; k++)
        if (g_arena[k] && v >= ptrv(g_arena[k]) && v < ptrv(g_arena[k]) + ARENA_SIZE)
            return 0xfa000000u + (v - ptrv(g_arena[k]));
    auto in = [v](const void *base, DWORD size) { return v >= ptrv(base) && v < ptrv(base) + size; };
    for (int i = 0; i < NENT; i++)
        if (in(&g_ents[i], sizeof(FakeEnt)))
            return 0xf0000000u + ((DWORD)i << 12) + (v - ptrv(&g_ents[i]));
    for (int i = 0; i < NOBJ; i++)
        if (in(&g_objs[i], sizeof(FakeObj)))
            return 0xf1000000u + ((DWORD)i << 12) + (v - ptrv(&g_objs[i]));
    for (int i = 0; i < NSEQ; i++)
        if (in(&g_seqs[i], sizeof(FakeSeq)))
            return 0xf2000000u + ((DWORD)i << 12) + (v - ptrv(&g_seqs[i]));
    for (int i = 0; i < NMODEL; i++)
        if (in(g_models[i], 0xc4))
            return 0xf3000000u + ((DWORD)i << 12) + (v - ptrv(g_models[i]));
    if (in(g_layer, sizeof g_layer))
        return 0xf7000000u + (v - ptrv(g_layer));
    if (in(g_packet, sizeof g_packet))
        return 0xf7400000u + (v - ptrv(g_packet));
    if (in(g_buf, sizeof g_buf))
        return 0xf7500000u + (v - ptrv(g_buf));
    if (in(&g_world, sizeof g_world))
        return 0xf7600000u;
    if (in(g_poly, sizeof g_poly))
        return 0xf7700000u + (v - ptrv(g_poly));
    if (in(g_converter, sizeof g_converter))
        return 0xf7800000u + (v - ptrv(g_converter));
    if (in(g_names, sizeof g_names))
        return 0xf7a00000u + (v - ptrv(g_names));
    if (in(g_activity, sizeof g_activity))
        return 0xf7b00000u + (v - ptrv(g_activity));
    if (in(g_attached, sizeof g_attached))
        return 0xf7c00000u + (v - ptrv(g_attached));
    if (g_ai_block && in(g_ai_block, 0x34))
        return 0xf9000000u + (v - ptrv(g_ai_block));
    if (v <= g_stack_top && v + 0x10000 > g_stack_top)
        return 0xf8000000u;
    return v;
}

CClassInfo *g_classes[NCLASS];
DWORD ec_hash(DWORD a, DWORD b, DWORD c)
{
    DWORD h = fnv(fnv(fnv(2166136261u, &a, 4), &b, 4), &c, 4);
    return h ^ (h >> 13);
}
FakeEnt *ent_of(DWORD t) { return (t >> 24) == 0xf0 ? &g_ents[(t >> 12) & 0xfff] : nullptr; }
FakeObj *obj_of(DWORD t) { return (t >> 24) == 0xf1 ? &g_objs[(t >> 12) & 0xfff] : nullptr; }

extern "C" DWORD __cdecl ec_record(DWORD *args, DWORD id, BYTE *self)
{
    const DWORD table = id / NSLOTS, slot = id % NSLOTS;
    ec_ret_float = 0;
    const Spec *sp = find_spec(table, slot);
    g_calls++;
    const DWORD me = tag(ptrv(self));
    if (!sp) {
        ec_pop = 0;
        ev('?', id, me);
        log("  entity classes: unexpected slot %u of table %u", slot, table);
        return 0;
    }
    ec_pop = sp->nargs * 4u;
    DWORD h = 2166136261u;
    for (int i = 0; i < sp->nargs; i++) {
        const DWORD a = args[i];
        DWORD v;
        switch (sp->kinds[i]) {
        case 'F': v = a; break;
        case 'P': {
            const DWORD *p = reinterpret_cast<const DWORD *>(a);
            v = p ? fnv(tag(a), p, 8) : 0;
            break;
        }
        case 'M': {
            const DWORD *m = reinterpret_cast<const DWORD *>(a);
            const DWORD w[4] = {m[0], m[1], m[2] & 0xff, tag(m[3])};
            v = fnv(tag(a), w, 16);
            break;
        }
        case 'D': v = tag(*reinterpret_cast<const DWORD *>(a)); break;
        default: v = tag(a);
        }
        h = fnv(h, &v, 4);
    }
    ev(0x52000000u + id, me, h, sp->nargs ? tag(args[0]) : 0);
    const DWORD r = ec_hash(id, me, g_calls);
    FakeEnt *fe = table == T_ENT ? ent_of(me) : nullptr;
    FakeObj *fo = obj_of(me);
    for (int i = 0; i < sp->nargs; i++) {
        switch (sp->kinds[i]) {
        case 'R':
            for (int k = 0; k < 4; k++)
                reinterpret_cast<int *>(args[i])[k] = (int)(ec_hash(r, (DWORD)k, 1) % 4000) - 1000;
            break;
        case 'W': *reinterpret_cast<DWORD *>(args[i]) = ec_hash(r, (DWORD)i, 2) % 3 ? ec_hash(r, (DWORD)i, 3) : 0; break;
        case 'O':
            reinterpret_cast<float *>(args[i])[0] = fe ? fe->px : 1.5f;
            reinterpret_cast<float *>(args[i])[1] = fe ? fe->py : -2.5f;
            break;
        }
    }
    switch (sp->ret) {
    case 'k': return fe ? fe->k54 : r % 4;
    case 'K': return fe ? fe->k85 : r % 4;
    case 'a':
    case 'h':
    case 'b':
        ec_ret_float = 1;
        ec_fret = !fe ? bits((float)(int)(r % 200) - 50.0f) : sp->ret == 'a' ? fe->f46 : sp->ret == 'h' ? fe->f55 : fe->f84;
        return r;
    case 'c': {
        const DWORD c = fe ? fe->cls : fo ? fo->cls : NCLASS - 1;
        return ptrv(g_classes[c % NCLASS]);
    }
    case 'n': return r % 4;
    case 'u':
        if (fe)
            return (r & ~0xffu) | fe->opaque;
        if (fo)
            return (r & ~0xffu) | fo->flag39;
        return (r & ~0xffu) | ((r >> 9) & 1);
    case 'p': return (r & 3) == 0 ? ptrv(&g_objs[r % NOBJ]) : 0;
    case 'y': return (r & 7) ? ptrv(g_poly) : 0;
    case 'v': return ptrv(&g_buf[r % 8]);
    case 'd':
        if (fe)
            fe->deleted++;
        if (fo)
            fo->deleted++;
        return ptrv(self);
    }
    return r;
}

// ---- Stand-ins for the exe functions that reach further ------------------------------------------------

void THISCALL rec_animated_update(void *e, DWORD a) { ev('U', tag(ptrv(e)), a); }
void THISCALL rec_lp_slot31(void *e, DWORD a) { ev('3', tag(ptrv(e)), a); }
long double THISCALL rec_set_render_height(void *e, float h)
{
    ev('H', tag(ptrv(e)), bits(h));
    return (long double)h * 2.0L;
}
DWORD THISCALL rec_bounce(void *l, void *mover, BYTE *out, DWORD filter)
{
    ev('B', tag(ptrv(l)), tag(ptrv(mover)), filter ^ fnv(2166136261u, out, 4));
    return 0x5500 | (g_calls & 1);
}
// the sweep's kind, movement, direction, length and bytes (+0x1e.. are not set by 0x545a00/0x545ab0)
DWORD sweep_hash(const BYTE *s) { return s ? fnv(2166136261u, s + 4, 0x1a) : 0; }
DWORD THISCALL rec_sweep(void *l, void *poly, const float *off, const BYTE *sw, BYTE *result, void **out_part, void *mover, DWORD any,
                         DWORD none, DWORD filter)
{
    const DWORD h = sweep_hash(sw);
    ev('S', tag(ptrv(l)) ^ tag(ptrv(poly)), (off ? fnv(tag(ptrv(off)), off, 8) : 0) ^ h, tag(ptrv(result)) ^ tag(ptrv(out_part)) ^
           tag(ptrv(mover)) ^ any ^ (none << 1) ^ (filter << 2));
    const DWORD r = ec_hash(h, g_calls, 5);
    return (r & ~0xffu) | ((r >> 5) & 1);
}
DWORD THISCALL rec_sweep_point(void *l, const float *pt, const BYTE *sw, BYTE *result, void **out_part, DWORD any, DWORD none,
                               void *mover, DWORD filter)
{
    const DWORD h = sweep_hash(sw);
    ev('s', tag(ptrv(l)) ^ (pt ? fnv(tag(ptrv(pt)), pt, 8) : 0), h ^ tag(ptrv(result)), tag(ptrv(out_part)) ^ tag(ptrv(mover)) ^
           any ^ (none << 1) ^ (filter << 2));
    const DWORD r = ec_hash(h, g_calls, 6);
    if (out_part && (r & 0x30) == 0x10)
        *out_part = &g_ents[(r >> 8) % NENT];
    if (result && (r & 0x100))
        *reinterpret_cast<DWORD *>(result) = r;
    return (r & ~0xffu) | ((r >> 3) & 1);
}
void THISCALL rec_position(void *p, float *out)
{
    FakeEnt *f = ent_of(tag(ptrv(p)));
    out[0] = f ? f->px : 1.5f;
    out[1] = f ? f->py : -2.5f;
    ev('P', tag(ptrv(p)), tag(ptrv(out)));
}
void THISCALL rec_base_position(void *e, DWORD x, DWORD y) { ev('p', tag(ptrv(e)), x, y); }
BYTE *THISCALL rec_sequence_object(void *e)
{
    FakeEnt *f = ent_of(tag(ptrv(e)));
    ev('Q', tag(ptrv(e)));
    return f && f->seq ? g_seqs[f->seq - 1].s : nullptr;
}
BYTE *THISCALL rec_model(void *e)
{
    FakeEnt *f = ent_of(tag(ptrv(e)));
    ev('m', tag(ptrv(e)));
    return g_models[f ? f->model % NMODEL : 0];
}
int THISCALL rec_angle_to_rotation(void *s, DWORD a)
{
    ev('A', tag(ptrv(s)), a);
    return (int)(ec_hash(a, tag(ptrv(s)), 7) % 9) - 1;
}
void THISCALL rec_motion(void *s, float *out, int rotation, DWORD frame, DWORD from, DWORD loops)
{
    ev('O', tag(ptrv(s)) ^ tag(ptrv(out)), (DWORD)rotation ^ frame, from ^ (loops << 3));
    const DWORD r = ec_hash(frame, from, (DWORD)rotation);
    out[0] = as_float_v(r % 3 ? bits((float)(int)(r % 64) - 20.0f) : small_float());
    out[1] = as_float_v(r % 5 ? bits((float)(int)((r >> 8) % 64) - 30.0f) : small_float());
}
long double THISCALL rec_rotation_to_angle(void *s, int r)
{
    ev('a', tag(ptrv(s)), (DWORD)r);
    return (long double)r * 0.785398163397L;
}
long double THISCALL rec_hit_points(void *e)
{
    FakeEnt *f = ent_of(tag(ptrv(e)));
    ev('h', tag(ptrv(e)));
    return f ? as_float_v(f->hp) : 3.0f;
}
long double THISCALL rec_hit_points_left(void *e)
{
    FakeEnt *f = ent_of(tag(ptrv(e)));
    ev('l', tag(ptrv(e)));
    return f ? as_float_v(f->hp_left) : 2.0f;
}
void THISCALL rec_died(void *e) { ev('D', tag(ptrv(e))); }
void THISCALL rec_add_ai(void *e, void *ai)
{
    ev('+', tag(ptrv(e)), tag(ptrv(ai)));
    if (ai && ai == g_ai_block) {
        pool_free(ai, 0x34);
        g_ai_block = nullptr;
    }
}
void *__fastcall rec_play_once(void *e, const ZString *seq, DWORD speed, const ZString *done, DWORD frame, DWORD a, DWORD b)
{
    ev('1', tag(ptrv(e)), text_hash(seq ? seq->buffer->text : nullptr) ^ speed, text_hash(done ? done->buffer->text : nullptr) ^ frame ^ a ^ (b << 2));
    const DWORD r = ec_hash(speed, frame, 9);
    return r % 4 ? &g_objs[r % NOBJ] : nullptr;
}
void *__cdecl rec_mark_for_deletion()
{
    ev('2');
    return &g_objs[g_calls % NOBJ];
}
BYTE *THISCALL rec_activity_ctor(BYTE *block)
{
    ev('C', block ? 1 : 0);
    if (block) {
        memset(block, 0, 0x34);
        *reinterpret_cast<DWORD *>(block) = ptrv(g_vt[T_OBJ]);
        g_ai_block = block;
    }
    return block;
}
void THISCALL rec_queue_add(void *q, void *b) { ev('q', tag(ptrv(q)), tag(ptrv(b))); }
void *__fastcall rec_inventory_create(int a, int b)
{
    ev('I', (DWORD)a, (DWORD)b);
    return &g_objs[(g_calls + 3) % NOBJ];
}
void THISCALL rec_reset_fields(void *o) { ev('r', tag(ptrv(o))); }
void THISCALL rec_ground_damage(void *e, DWORD s) { ev('g', tag(ptrv(e)), s); }
void THISCALL rec_set_synch(void *e, DWORD on) { ev('y', tag(ptrv(e)), on); }
void THISCALL rec_animated_die(void *e) { ev('X', tag(ptrv(e))); }
DWORD THISCALL rec_write_block(void *e, void *p, DWORD h)
{
    ev('W', tag(ptrv(e)), tag(ptrv(p)), h);
    return 0x400 + (g_calls & 7);
}
void THISCALL rec_read_block(void *e, void *p) { ev('R', tag(ptrv(e)), tag(ptrv(p))); }
void THISCALL rec_block_type(void *p, void *reader, DWORD kind) { ev('b', tag(ptrv(p)), ptrv(reader), kind); }
void THISCALL rec_write_word(void *p, DWORD v) { ev('w', tag(ptrv(p)), v & 0xffff); }
void THISCALL rec_write_bool(void *p, DWORD v) { ev('o', tag(ptrv(p)), v & 0xff); }
DWORD next_read()
{
    g_rnext = ec_hash(g_rnext, 0x52ead, 1);
    switch (g_rnext % 5) {
    case 0: return 0;
    case 1: return g_rnext & 0xff;
    case 2: return (g_rnext >> 3) & 0x1f;
    default: return g_rnext;
    }
}
DWORD THISCALL rec_read_word(void *p, WORD *out) { *out = (WORD)next_read(); ev('v', tag(ptrv(p)), *out); return 1; }
DWORD THISCALL rec_read_bool(void *p, BYTE *out) { *out = (BYTE)(next_read() & 1); ev('n', tag(ptrv(p)), *out); return 1; }
DWORD THISCALL rec_register_reader(void *p, void *reader, const char *name) { ev('G', tag(ptrv(p)), ptrv(reader), text_hash(name)); return 0x4e9; }
void THISCALL rec_build_strips(BYTE *e) { ev('L', tag(ptrv(e)), fnv(2166136261u, e + 0xbc, 0x24)); }
DWORD THISCALL rec_find_sequence(void *m, const char *n)
{
    ev('f', tag(ptrv(m)), text_hash(n));
    const DWORD r = ec_hash(text_hash(n), 3, 4);
    return r % 3 ? r % 4 : 0xffffffff;
}
BYTE *THISCALL rec_model_sequence(void *m, DWORD i)
{
    ev('F', tag(ptrv(m)), i);
    return g_seqs[i % NSEQ].s;
}

struct Patches {
    static constexpr int N = 48;
    CodePatch p[N];
    int n = 0;
    void add(DWORD a, void *f) { patch_jump(p[n++], a, f); }
    void on()
    {
        n = 0;
        add(0x48f300, (void *)&rec_animated_update);
        add(0x4fab60, (void *)&rec_lp_slot31);
        add(0x48b770, (void *)&rec_set_render_height);
        add(0x4e96c0, (void *)&rec_bounce);
        add(0x4e8e00, (void *)&rec_sweep);
        add(0x4e8d40, (void *)&rec_sweep_point);
        add(0x4fb0a0, (void *)&rec_position);
        add(0x489fd0, (void *)&rec_base_position);
        add(0x48b590, (void *)&rec_sequence_object);
        add(0x48b290, (void *)&rec_model);
        add(0x55fdd0, (void *)&rec_angle_to_rotation);
        add(0x55fd30, (void *)&rec_motion);
        add(0x55fe90, (void *)&rec_rotation_to_angle);
        add(0x48d150, (void *)&rec_hit_points);
        add(0x48d030, (void *)&rec_hit_points_left);
        add(0x4fbf30, (void *)&rec_died);
        add(0x4fbc50, (void *)&rec_add_ai);
        add(0x496e20, (void *)&rec_play_once);
        add(0x497720, (void *)&rec_mark_for_deletion);
        add(0x42f5e0, (void *)&rec_activity_ctor);
        add(0x586f80, (void *)&rec_queue_add);
        add(0x4d4ff0, (void *)&rec_inventory_create);
        add(0x417230, (void *)&rec_reset_fields);
        add(0x48d7f0, (void *)&rec_ground_damage);
        add(0x48fb50, (void *)&rec_set_synch);
        add(0x48f590, (void *)&rec_animated_die);
        add(0x48ec70, (void *)&rec_write_block);
        add(0x48ef00, (void *)&rec_read_block);
        add(0x4893b0, (void *)&rec_write_block);
        add(0x4896e0, (void *)&rec_read_block);
        add(0x51ff50, (void *)&rec_block_type);
        add(0x5209c0, (void *)&rec_write_word);
        add(0x520a50, (void *)&rec_write_bool);
        add(0x520510, (void *)&rec_read_word);
        add(0x520580, (void *)&rec_read_bool);
        add(0x51ff30, (void *)&rec_register_reader);
        add(0x493850, (void *)&rec_build_strips);
        add(0x517240, (void *)&rec_find_sequence);
        add(0x5173f0, (void *)&rec_model_sequence);
    }
    void off()
    {
        for (int i = n - 1; i >= 0; i--)
            unpatch(p[i]);
        n = 0;
    }
};
Patches g_patches;

// ---- Setting up the fakes and the globals ---------------------------------------------------------------

void init_tables()
{
    for (int t = 0; t < NTABLES; t++)
        for (int s = 0; s < NSLOTS; s++)
            g_vt[t][s] = ptrv(ec_thunks) + 10u * (t * NSLOTS + s);
    for (DWORD k = 0; k < NTABLES * NSLOTS; k++) {
        const BYTE *th = reinterpret_cast<const BYTE *>(ec_thunks) + 10 * k;
        if (th[0] != 0xb8 || *reinterpret_cast<const DWORD *>(th + 1) != k || th[5] != 0xe9) {
            log("  entity classes: recorder thunk %u is broken", k);
            break;
        }
    }
    g_classes[0] = getter(0x487620);    // CEntityBase
    g_classes[1] = getter(0x48bdf0);    // CEntityDestructable
    g_classes[2] = getter(0x48dcc0);    // CEntityAnimated
    g_classes[3] = getter(0x4903f0);    // CEntityMovable
    g_classes[4] = getter(0x49a930);    // CEntityCharacter
    g_classes[5] = getter(0x49b400);    // CEntityStationaryCharacter
    g_classes[6] = getter(0x49b810);    // CEntityWalking
    g_classes[7] = getter(0x49bde0);    // CEntityFlying
    g_classes[8] = getter(0x491930);    // CEntityProjectile
    g_classes[9] = getter(0x4f9bf0);    // CLayerPart
    g_classes[10] = getter(0x4fcb00);   // CAIBase
    g_classes[11] = getter(0x4163b0);   // CObject
    if (!g_names_made) {
        constexpr const char *NAMES[] = {"Idle", "Fire", "Walk", "Shoot Left", "", "Death"};
        for (int i = 0; i < 6; i++)
            at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&g_names[i], NAMES[i]);
        g_names_made = true;
    }
}

// The exe's globals the fakes stand in for or the functions write, saved before and restored after each side
struct Globals {
    DWORD world, filter, local, last_read, mask, category;
    BYTE guard;
};
Globals g_saved;
void save_globals()
{
    g_saved = {*at<DWORD *>(0x6c2080), *at<DWORD *>(BOUNCE_FILTER), *at<DWORD *>(LOCAL_CHARACTER), *at<DWORD *>(LAST_READ_OBJECT),
               *at<DWORD *>(0x6bde78), *at<DWORD *>(0x6bde7c), *at<BYTE *>(0x6bde6b)};
}
void restore_globals()
{
    *at<DWORD *>(0x6c2080) = g_saved.world;
    *at<DWORD *>(BOUNCE_FILTER) = g_saved.filter;
    *at<DWORD *>(LOCAL_CHARACTER) = g_saved.local;
    *at<DWORD *>(LAST_READ_OBJECT) = g_saved.last_read;
    *at<DWORD *>(0x6bde78) = g_saved.mask;
    *at<DWORD *>(0x6bde7c) = g_saved.category;
    *at<BYTE *>(0x6bde6b) = g_saved.guard;
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

Walking *ent(int i) { return reinterpret_cast<Walking *>(g_ents[i].o); }
bool g_strings_live;

void free_strings()
{
    if (!g_strings_live)
        return;
    for (int i = 0; i < NENT; i++)
        at<void(THISCALL *)(void *)>(0x4defd0)(&ent(i)->discharge_sequence);
    g_strings_live = false;
}

void setup_fakes()
{
    free_strings();
    memset(g_ents, 0, sizeof g_ents);
    memset(g_objs, 0, sizeof g_objs);
    memset(g_seqs, 0, sizeof g_seqs);
    memset(g_models, 0, sizeof g_models);
    memset(g_layer, 0, sizeof g_layer);
    memset(g_activity, 0, sizeof g_activity);
    memset(g_attached, 0, sizeof g_attached);
    memset(g_converter, 0, sizeof g_converter);
    g_world = {};
    g_world.vftable = ptrv(g_vt[T_WORLD]);
    for (int i = 0; i < NOBJ; i++) {
        g_objs[i].vftable = ptrv(g_vt[T_OBJ]);
        g_objs[i].cls = random() % NCLASS;
        g_objs[i].flag39 = (BYTE)(random() % 3 ? random() % 2 : random());
    }
    for (int i = 0; i < NSEQ; i++) {
        FakeSeq &s = g_seqs[i];
        *reinterpret_cast<DWORD *>(s.s) = ptrv(g_vt[T_OBJ]);
        for (int &m : s.motion)
            m = random() % 3 ? 0 : (int)(random() % 20) - 10;
        *reinterpret_cast<int **>(s.s + 0x44) = s.motion;
        constexpr DWORD FRAMES[] = {0, 1, 2, 5, 8, 3};
        *reinterpret_cast<DWORD *>(s.s + 0x48) = FRAMES[random() % 6];
    }
    for (int i = 0; i < NMODEL; i++) {
        CModel *m = reinterpret_cast<CModel *>(g_models[i]);
        constexpr DWORD BF[] = {0, 0x3f800000, 0x3f000000, 0x3e99999a, 0x3f7fffff, 0x80000000, 0x00000001};
        m->bounce_factor = as_float_v(BF[random() % 7]);
        m->bounce_friction = as_float_v(random_float());
        m->min_bounce_friction = as_float_v(random_float());
        m->min_bounce = as_float_v(random_float());
    }
    for (DWORD k = 0; k < 16; k++)
        reinterpret_cast<DWORD *>(g_poly)[k] = random();
    *reinterpret_cast<DWORD *>(g_converter) = 0x5f1fe0;
    *reinterpret_cast<DWORD *>(g_converter + 8) = random() % 3 ? 0 : ptrv("CEntityStub");
    *reinterpret_cast<DWORD *>(g_layer + 0x7c) = random();
    for (float &b : g_buf)
        b = as_float_v(small_float());
    for (int i = 0; i < NENT; i++) {
        FakeEnt &f = g_ents[i];
        Walking *a = ent(i);
        a->vftable = ptrv(g_vt[T_ENT]);
        a->parent = i > 0 && random() % 3 == 0 ? ent(random() % i) : nullptr;
        a->flags = random_flags();
        a->activity.vftable = VT_ACTIVITY;
        a->activity.data = g_activity[i];
        a->activity.count = random() % 5;
        a->activity.capacity = 6;
        for (DWORD k = 0; k < a->activity.count; k++)
            g_activity[i][k] = random() % 5 ? &g_objs[random() % NOBJ] : nullptr;
        a->layer = random() % 8 ? g_layer : nullptr;
        a->x = as_float_v(small_float());
        a->y = as_float_v(small_float());
        a->rx = (int)(random() % 4000) - 1000;
        a->ry = (int)(random() % 4000) - 1000;
        for (int &b : a->bounds)
            b = (int)random();
        a->render_height = random() % 3 ? (int)(random() % 40) - 10 : (int)random();
        a->render_height_f = as_float_v(small_float());
        a->model = random() % 8;
        a->sequence = random() % 4;
        a->aflags = random() & 0x1ff;
        a->fade = 1.0f;
        a->play_ratio = as_float_v(random_float());
        a->frame = as_float_v(small_float());
        a->frame_index = random() % 4 ? random() % 6 : random();
        a->aiming = as_float_v(random_angle());
        a->movement = as_float_v(random_angle());
        a->attached = {ptrv(g_vt[T_ARRAY]), g_attached[i], random() % 5, 6};
        for (DWORD k = 0; k < a->attached.count; k++)
            g_attached[i][k] = random() % 4 ? &g_objs[random() % NOBJ] : nullptr;
        a->friction = as_float_v(random() % 3 ? bits((float)(random() % 100) / 100.0f) : random_float());
        for (float &v : a->velocity)
            v = as_float_v(small_float());
        for (float &v : a->acceleration)
            v = as_float_v(small_float());
        a->to[0] = random() % 4 ? a->x : as_float_v(small_float());
        a->to[1] = random() % 4 ? a->y : as_float_v(small_float());
        // CEntityProjectile's +0x108..+0x110 (the character's inventory and home there for the others)
        BYTE *o = g_ents[i].o;
        if (random() % 2) {
            *reinterpret_cast<DWORD *>(o + 0x10c) = random() % 3 ? bits((float)(int)(random() % 40) / 8.0f - 1.0f) : random_float();
            *reinterpret_cast<DWORD *>(o + 0x110) = random() % 3 ? 0x3c23d70a : random_float();
        } else {
            a->inventory = random() % 4 ? &g_objs[random() % NOBJ] : nullptr;
            a->home[0] = as_float_v(small_float());
            a->home[1] = as_float_v(small_float());
        }
        a->discharge[0] = as_float_v(small_float());
        a->discharge[1] = as_float_v(small_float());
        a->discharge_turned[0] = as_float_v(random());
        a->discharge_turned[1] = as_float_v(small_float());
        at<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4dedf0)(&a->discharge_sequence, &g_names[random() % 6]);
        a->local = (BYTE)(random() % 3 ? random() % 2 : random());
        a->movement_speed = random() % 4;
        constexpr DWORD CLS[] = {2, 3, 4, 5, 6, 7, 8, 0, 9, 11};
        f.cls = CLS[random() % 10];
        f.seq = random() % 4 ? 1 + random() % NSEQ : 0;
        f.model = random() % NMODEL;
        f.k54 = random() % 3 ? (DWORD)((int)(random() % 40) - 20) : random();
        f.k85 = random() % 4 ? random() % 8 : random();
        f.f46 = random_angle();
        f.f55 = small_float();
        f.f84 = small_float();
        f.opaque = (BYTE)(random() % 3 ? random() % 2 : random());
        f.px = random() % 3 ? a->to[0] : as_float_v(small_float());
        f.py = random() % 3 ? a->to[1] : as_float_v(small_float());
        constexpr DWORD HP[] = {0, 0x41200000, 0x80000000, 0x7fc00000, 0x3f800000};
        f.hp = HP[random() % 5];
        f.hp_left = small_float();
    }
    g_strings_live = true;
    *at<DWORD *>(0x6c2080) = ptrv(&g_world);
    *at<BYTE *>(0x6bde6b) = (BYTE)(random() % 4);
    *at<DWORD *>(0x6bde7c) = random() % 3 ? random() % 32 : 0xffffffff;
    *at<DWORD *>(0x6bde78) = random() % 3 ? 1u << (random() % 32) : random_flags();
    *at<DWORD *>(BOUNCE_FILTER) = random() % 2 ? 0x49adf0 : random();
    *at<DWORD *>(LOCAL_CHARACTER) = random() % 2 ? ptrv(ent(random() % NENT)) : 0;
    g_rnext = random();
}

// The state of all fakes (pointers tagged); the fakes' strings by text, then destroyed
void st_fakes()
{
    for (int i = 0; i < NENT; i++) {
        const DWORD *d = reinterpret_cast<const DWORD *>(g_ents[i].o);
        for (DWORD k = 0; k < 0x130 / 4; k++) {
            if (k == 0x124 / 4)
                continue;
            st(tag(d[k]));
            if (k >= 0x2c && k < 0x2c + 0x50)
                dump(tag(d[k]));
        }
        if (g_strings_live)
            st(text_hash(ent(i)->discharge_sequence.buffer->text));
        st(g_ents[i].deleted);
        for (DWORD k = 0; k < 6; k++) {
            st(tag(ptrv(g_attached[i][k])));
            st(tag(ptrv(g_activity[i][k])));
        }
    }
    free_strings();
    for (int i = 0; i < NOBJ; i++)
        st(g_objs[i].deleted);
    st_bytes(g_buf, sizeof g_buf);
    st(*at<DWORD *>(BOUNCE_FILTER));
    st(tag(*at<DWORD *>(LOCAL_CHARACTER)));
    st(tag(*at<DWORD *>(LAST_READ_OBJECT)));
    st(*at<DWORD *>(0x6bde78));
    st(*at<DWORD *>(0x6bde7c));
    st(*at<BYTE *>(0x6bde6b));
    if (g_ai_block) {
        // a block of 0x490060 not handed to AddAI (both sides alike): given back
        pool_free(g_ai_block, 0x34);
        g_ai_block = nullptr;
        st(0xb10c);
    }
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
                log("    entity dword +%x differs first: %x / %x", (DWORD)(0xb0 + i * 4), g_dump[0][i], g_dump[1][i]);
                break;
            }
    }
    t.check(same, "entity classes", d);
    return same;
}

// ---- Slot scenarios on fake entities ----------------------------------------------------------------------

// A bounce motion's state: kind, move, direction, length, the two flags and the hit data (+0x1e/+0x1f are
// padding nobody sets)
void st_motion(const BYTE *m)
{
    st_bytes(m + 4, 0x1a);
    st_bytes(m + 0x20, 0x18);
}

using F0 = DWORD(THISCALL *)(void *);
using F1 = DWORD(THISCALL *)(void *, DWORD);
using F2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
using F3 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD);
using F4 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD);
using F5 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD);
using FL0 = long double(THISCALL *)(void *);
using FAST2 = DWORD(__fastcall *)(void *, void *);
using FAST1 = DWORD(__fastcall *)(DWORD);

DWORD random_arg()
{
    switch (random() % 4) {
    case 0: return 0;
    case 1: return random() % 100;
    case 2: return ptrv(ent(random() % NENT));
    default: return random();
    }
}
DWORD random_obj() { return random() % 5 ? ptrv(&g_objs[random() % NOBJ]) : 0; }
DWORD random_class() { return random() % 4 ? ptrv(g_classes[random() % NCLASS]) : 0; }
// An attached object of the entity (or another one) to start from
DWORD attached_or_obj(int i)
{
    const Walking *a = ent(i);
    if (a->attached.count && random() % 3)
        return ptrv(a->attached.data[random() % a->attached.count]);
    return random_obj();
}

constexpr int NOPS = 90;
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
    Walking *e = ent(i);
    void *pk = g_packet;
    const DWORD f = small_float(), g = random_angle();
    float pt[4];
    for (float &v : pt)
        v = as_float_v(small_float());
    DWORD r = 0;
    mask = 0;
    switch (op) {
    // CEntityAnimated
    case 0: r = tag(at<F1>(0x490010)(e, random() % 3 ? random() % 9 : random())), mask = 0xffffffff; break;
    case 1: r = at<F2>(0x490060)(e, ptrv(random() % 5 ? &g_names[random() % 6] : nullptr), f) & 0xff, mask = 0xff; break;
    case 2: r = tag(at<F1>(0x490130)(e, attached_or_obj(i))), mask = 0xffffffff; break;
    case 3: r = at<F1>(0x490150)(e, attached_or_obj(i)), mask = 0xffffffff; break;
    case 4: r = tag(at<F1>(0x490190)(e, ptrv(&g_objs[random() % NOBJ]))), mask = 0xffffffff; break;
    case 5: r = tag(at<F1>(0x490230)(e, random_class())), mask = 0xffffffff; break;
    case 6: r = tag(at<F1>(0x490280)(e, random_class())), mask = 0xffffffff; break;
    case 7: r = tag(at<F2>(0x4902d0)(e, random() % 4 ? attached_or_obj(i) : 0, random_class())), mask = 0xffffffff; break;
    case 8: r = tag(at<F2>(0x490360)(e, random() % 4 ? attached_or_obj(i) : 0, random_class())), mask = 0xffffffff; break;
    // CEntityMovable's small slots
    case 9: {
        DWORD res[9];
        for (DWORD &v : res)
            v = random_float();
        at<F1>(0x4904a0)(e, ptrv(res));
        break;
    }
    case 10: at<F1>(0x4904c0)(e, f); break;
    case 11: st_ld(at<FL0>(0x4904d0)(e)); break;
    case 12: case 13: case 21: case 22: {
        DWORD v[3] = {random_float(), random_float(), random_float()};
        constexpr DWORD A[] = {0x4904e0, 0x490500, 0, 0, 0, 0, 0, 0, 0, 0x4905f0, 0x490610};
        at<F1>(A[op - 12])(e, ptrv(v));
        break;
    }
    case 14: at<F2>(0x491890)(e, random_float(), random_float()); break;
    case 15: at<F1>(0x491840)(e, f); break;
    case 16: case 17: case 25: case 26: {
        DWORD v[3] = {small_float(), small_float(), small_float()};
        const DWORD a = op == 16 ? 0x490560 : op == 17 ? 0x490530 : op == 25 ? 0x490670 : 0x490640;
        at<F1>(a)(e, ptrv(v));
        break;
    }
    case 18: at<F2>(0x4918b0)(e, small_float(), small_float()); break;
    case 19: r = tag(at<F0>(0x4905e0)(e)), mask = 0xffffffff; break;
    case 20: r = at<F1>(0x4905c0)(e, ptrv(g_buf + 4)), mask = 0xffffffff; break;
    case 23: at<F2>(0x4918e0)(e, random_float(), random_float()); break;
    case 24: at<F2>(0x4905a0)(e, ptrv(g_buf), ptrv(g_buf + 1)); break;
    case 27: at<F2>(0x491900)(e, small_float(), small_float()); break;
    case 28: r = tag(at<F0>(0x4906f0)(e)), mask = 0xffffffff; break;
    case 29: r = at<F1>(0x4906d0)(e, ptrv(g_buf + 6)), mask = 0xffffffff; break;
    case 30: at<F2>(0x4906b0)(e, ptrv(g_buf + 2), ptrv(g_buf + 3)); break;
    case 31: at<F1>(0x491610)(e, ptrv(g_buf + 8)); break;
    // CEntityMovable's physics and sweeps
    case 32: at<F1>(0x490fc0)(e, random() % 3 ? bits((float)(random() % 100) / 100.0f) : random_float()); break;
    case 33: at<F1>(0x491160)(e, f); break;
    case 34: at<FAST1>(0x491150)(random() % 2 ? 0x49adf0 : random()); break;
    case 35: at<F2>(0x491230)(e, random_float(), random_float()); break;
    case 36: r = at<F1>(0x491250)(e, ptrv(pt)), mask = 0xffffffff; break;
    case 37: r = at<F4>(0x4912b0)(e, ptrv(pt), random() % 2 ? 0x49adf0 : 0, random() % 4, random() % 3 ? 0 : ptrv(pt + 2)), mask = 0xffffffff; break;
    case 38:
        r = at<F5>(0x491380)(e, ptrv(ent(random() % NENT)), random() % 2 ? 0x49adf0 : 0, random() % 3 ? 0 : ptrv(pt + 2), random() % 4,
                             random() % 3 ? 0 : ptrv(pt)),
        mask = 0xffffffff;
        break;
    case 39: r = at<F0>(0x4914d0)(e), mask = 0xffffffff; break;
    case 40: {
        const DWORD x = random() % 3 ? bits(e->x) : small_float(), y = random() % 3 ? bits(e->y) : small_float();
        at<F2>(0x4914e0)(e, random() % 4 ? x : bits(e->x), y);
        break;
    }
    case 41: {
        BYTE sweep[0x38];
        at<void *(THISCALL *)(void *, DWORD, DWORD)>(0x545a00)(sweep, random() % 3, random() % 2);
        at<DWORD(THISCALL *)(void *, const float *, const float *)>(0x545ab0)(sweep, pt, pt + 2);
        int rect[4];
        r = at<F2>(0x4916f0)(e, ptrv(sweep), ptrv(rect)), mask = 0xffffffff;
        st_bytes(rect, sizeof rect);
        break;
    }
    case 42: case 43: {
        BYTE motion[0x38];
        at<void *(THISCALL *)(void *, DWORD, DWORD)>(0x545a00)(motion, random() % 3, random() % 2);
        r = at<F1>(op == 42 ? 0x491710 : 0x492060)(e, ptrv(motion));
        mask = op == 42 ? 0xffffffff : 0;
        st_motion(motion);
        break;
    }
    // CEntityProjectile
    case 44: st_ld(at<FL0>(0x492080)(e)); break;
    case 45: at<F1>(0x492150)(e, random() % 3 ? bits((float)(random() % 300) / 100.0f) : random_float()); break;
    // The CEntityAnimated converter
    case 46: {
        CClassInfo *out = reinterpret_cast<CClassInfo *>(0x1234);
        r = at<F2>(0x492200)(g_converter, ptrv(random() % 8 ? (void *)e : (void *)&g_objs[random() % NOBJ]), random() % 3 ? ptrv(&out) : 0);
        st(ptrv(out));
        mask = 0xffffffff;
        break;
    }
    // CEntityAnimatedLine
    case 47: {
        int rect[4] = {(int)random(), (int)random(), (int)random(), (int)random()};
        r = at<F3>(0x4931b0)(e, ptrv(rect), random() % 4 ? random() % 7 : random(), random()), mask = 0xffffffff;
        st_bytes(rect, sizeof rect);
        break;
    }
    case 48: r = at<F2>(0x493230)(e, ptrv(pk), random() % 3 ? 1 : 0), mask = 0xffffffff; break;
    case 49: at<F1>(0x4932e0)(e, ptrv(pk)); break;
    case 50: {
        if (random() % 4 == 0)
            pt[2] = pt[0], pt[3] = pt[1];
        at<F2>(0x493520)(e, ptrv(pt), ptrv(pt + 2));
        break;
    }
    case 51: at<F4>(0x493670)(e, small_float(), small_float(), small_float(), small_float()); break;
    case 52: r = at<F0>(0x492f40)(pk), mask = 0xffffffff; break;
    // CEntityAnimatedUpperBody
    case 53: at<F1>(0x494390)(e, f); break;
    case 54:
        reinterpret_cast<BYTE *>(e)[0xe4] = (BYTE)(random() % 2);   // the upper body's "updating" byte
        st_ld(at<FL0>(0x4943b0)(e));
        break;
    // CEntityCharacter
    case 55: r = tag(at<F0>(0x49a8c0)(e)), mask = 0xffffffff; break;
    case 56: r = at<F0>(0x49a8d0)(e), mask = 0xffffffff; break;
    case 57: {
        DWORD p[2] = {random_float(), random_float()};
        at<F1>(0x49a8e0)(e, ptrv(p));
        break;
    }
    case 58: {
        DWORD p[2] = {random(), random()};
        at<F1>(0x49a900)(e, ptrv(p));
        st_bytes(p, 8);
        break;
    }
    case 59: r = at<FAST2>(0x49adf0)(ent(random() % NENT), random() % 6 ? (void *)ent(random() % NENT) : (void *)&g_objs[0]), mask = 0xffffffff; break;
    case 60: at<F0>(0x49afd0)(e); break;
    case 61: at<F1>(0x49b000)(e, random_arg()); break;
    case 62: at<F1>(0x49b040)(e, random() % 3 ? bits((float)(random() % 100) / 100.0f) : random_float()); break;
    case 63: case 79: case 80: at<F1>(op == 63 ? 0x49b090 : op == 79 ? 0x49b7b0 : 0x49bd70)(e, f); break;
    case 64: {
        DWORD p[2] = {random_float(), random_float()};
        r = tag(at<F2>(0x49b1a0)(e, ptrv(p), ptrv(&g_names[random() % 6]))), mask = 0xffffffff;
        break;
    }
    // eax is the turned discharge (+0x11c): the shoot and point-weapon behaviours pass it to TurnToPart (batch 6-12)
    case 65: r = at<F0>(0x49b1d0)(e) - ptrv(e), mask = 0xffffffff; break;
    case 66: r = at<F2>(0x49b2a0)(e, ptrv(pk), random() % 3 ? 1 : 0), mask = 0xffffffff; break;
    case 67: r = at<F0>(0x49b2f0)(pk), mask = 0xffffffff; break;
    case 68: at<F1>(0x49b300)(e, ptrv(pk)); break;
    case 69: r = tag(at<F0>(0x49b330)(e)), mask = 0xffffffff; break;
    case 70: at<F1>(0x49b360)(e, random_obj()); break;
    case 71: at<F1>(0x49b3a0)(e, random_arg()); break;
    case 72: st_ld(at<FL0>(0x49b4b0)(e)); break;
    case 73: at<F0>(0x49bd90)(e); break;
    case 74: r = at<F0>(0x49c270)(e) & 0xff, mask = 0xff; break;
    // More of the same with other states
    case 75: e->flags |= 0x800000, e->flags &= ~0x200000u, at<F1>(0x490fc0)(e, bits((float)(random() % 100) / 50.0f)); break;
    case 76: e->flags |= 0x800000, e->flags &= ~0x200000u, at<F1>(0x491160)(e, f); break;
    case 77:
        e->flags |= 0x800000;
        *reinterpret_cast<DWORD *>(g_ents[i].o + 0x10c) = random() % 2 ? 0x3c23d70a : bits((float)(random() % 4));
        at<F1>(0x492150)(e, random() % 3 ? bits((float)(random() % 300) / 100.0f) : random_float());
        break;
    case 78: e->flags |= 0x800000, at<F1>(0x49b090)(e, f); break;
    case 81: {
        // the line's own state: two points, then the line read back
        at<F2>(0x493520)(e, ptrv(pt), ptrv(pt + 2));
        at<F1>(0x4932e0)(e, ptrv(pk));
        r = at<F2>(0x493230)(e, ptrv(pk), 1), mask = 0xffffffff;
        break;
    }
    case 82: {
        // the frame index on a locked frame
        e->aflags |= 0x10;
        r = tag(at<F1>(0x490010)(e, random() % 9)), mask = 0xffffffff;
        break;
    }
    case 83: {
        // add the same object twice, find it, remove it, delete it
        const DWORD o = ptrv(&g_objs[random() % NOBJ]);
        at<F1>(0x490190)(e, o);
        at<F1>(0x490190)(e, o);
        r = at<F1>(0x490150)(e, o);
        r ^= tag(at<F1>(0x490230)(e, 0)) << 1;
        mask = 0xffffffff;
        at<F1>(0x490130)(e, o);
        break;
    }
    case 84: {
        // a velocity of zero in x or y keeps it (slot 117)
        e->velocity[random() % 2] = 0;
        at<F1>(0x491610)(e, ptrv(g_buf + 8));
        break;
    }
    case 85: {
        // CanSee with the other part transparent
        g_ents[(i + 1) % NENT].opaque = 0;
        r = at<F5>(0x491380)(e, ptrv(ent((i + 1) % NENT)), 0, 0, 0, 0), mask = 0xffffffff;
        break;
    }
    case 86: r = at<F1>(0x49b000)(e, random_arg()), mask = 0; break;
    case 87: {
        // the next/previous attached object of each attached one
        for (DWORD k = 0; k < e->attached.count; k++) {
            r = r * 3 + tag(at<F2>(0x4902d0)(e, ptrv(e->attached.data[k]), random_class()));
            r = r * 3 + tag(at<F2>(0x490360)(e, ptrv(e->attached.data[k]), random_class()));
        }
        mask = 0xffffffff;
        break;
    }
    case 88: {
        // the character's message and death with an inventory
        e->inventory = &g_objs[random() % NOBJ];
        at<F1>(0x49b3a0)(e, random_arg());
        at<F0>(0x49bd90)(e);
        break;
    }
    case 89: {
        // the flying update's statics from scratch
        *at<BYTE *>(0x6bde6b) = 0;
        e->flags |= 0x800000;
        at<F1>(0x49b090)(e, f);
        break;
    }
    }
    (void)g;
    return r;
}

void arena_on(int side, BYTE pattern, bool fresh = true);
void arena_off();
extern DWORD g_used[2];

// ---- The strips of CEntityAnimatedLine on a fake line --------------------------------------------------------

// The fake entity as a line: its strips' array (the exe's vtable 0x5f25dc) from the arena, maybe with old
// strips of random bytes; random end points, perpendicular, width and colours
void setup_line(Walking *e, AnimatedLine *l)
{
    (void)e;
    BYTE *o = reinterpret_cast<BYTE *>(l);
    const DWORD n = random() % 4 ? random() % 6 : 0, cap = n + random() % 3;
    BYTE *data = cap ? static_cast<BYTE *>(heap_alloc(cap * 0x60)) : nullptr;
    for (DWORD k = 0; k < cap * 0x60; k += 4)
        *reinterpret_cast<DWORD *>(data + k) = random();
    l->strips = {VT_LINE_STRIPS, data, n, cap};
    for (int k = 0; k < 4; k++)
        l->bounds[k] = (int)random();
    for (DWORD k = 0x7c; k < 0x9c; k++)
        o[k] = (BYTE)random();
    auto coord = []() -> float {
        if (random() % 8 == 0)
            return as_float_v(random_float());
        return (float)((int)(random() % 1200) - 300) / ((random() & 3) ? 1.0f : 8.0f);
    };
    l->start[0] = coord();
    l->start[1] = coord();
    l->end[0] = random() % 6 ? coord() : l->start[0];
    l->end[1] = random() % 6 ? coord() : l->start[1];
    const float dx = l->end[0] - l->start[0], dy = l->end[1] - l->start[1];
    float len = dx * dx + dy * dy;
    for (int k = 0; k < 4; k++)
        len = len > 0.0f ? (len + (dx * dx + dy * dy) / len) * 0.5f : 1.0f;   // a rough square root
    l->normal[0] = random() % 4 ? dx / (len > 0.0f ? len : 1.0f) : as_float_v(random_float());
    l->normal[1] = random() % 4 ? dy / (len > 0.0f ? len : 1.0f) : as_float_v(random_float());
    l->perpendicular[0] = l->normal[1];
    l->perpendicular[1] = -l->normal[0];
    if (random() % 5 == 0)
        l->perpendicular[random() % 2] = as_float_v(random_float());
    constexpr DWORD W[] = {0x40800000, 0, 0x41800000, 0x3f800000, 0xc0800000, 0x42000000};
    l->width = as_float_v(random() % 8 ? W[random() % 6] : random_float());
}

constexpr int NSTRIP_OPS = 9;
DWORD strip_case(DWORD &mask)
{
    setup_fakes();
    arena_on(g_side, 0x6b);
    const int op = g_op >= 0 ? g_op : (int)(random() % NSTRIP_OPS);
    Walking *e = ent((int)(random() % NENT));
    AnimatedLine *l = reinterpret_cast<AnimatedLine *>(e);
    setup_line(e, l);
    DWORD r = 0;
    mask = 0;
    BYTE strip[0x60];
    for (BYTE &b : strip)
        b = (BYTE)random();
    switch (op) {
    case 0: case 1: at<F0>(0x493850)(l); break;
    case 2: {
        float a[2] = {l->start[0], l->start[1]}, b[2] = {l->end[0], l->end[1]};
        at<F2>(0x493520)(l, ptrv(a), ptrv(b));
        break;
    }
    case 3: {
        int rect[4] = {(int)(random() % 2000) - 500, (int)(random() % 2000) - 500, (int)random(), (int)(random() % 4000)};
        r = at<F3>(0x4936b0)(l, ptrv(rect), ptrv(l->start), ptrv(l->end)), mask = 0xffffffff;
        break;
    }
    case 4: at<F1>(0x493ff0)(&l->strips, random() % 3 ? random() % 12 : 0); break;
    case 5: at<F1>(0x493d20)(&l->strips, ptrv(strip)); break;
    case 6: {
        BYTE d[0x60];
        for (BYTE &b : d)
            b = (BYTE)random();
        r = tag(at<F1>(0x493fa0)(d, ptrv(strip))) ^ tag(at<F1>(0x493ef0)(d + 0x28, ptrv(strip + 0x28))) ^ tag(at<F1>(0x493f10)(d + 0x40, ptrv(strip + 0x40)));
        mask = 0xffffffff;
        st_bytes(d, sizeof d);
        break;
    }
    case 7: {
        // the strip's rectangle (its slot 3), of a strip built from the line
        for (int k = 0x28; k < 0x38; k += 4)
            *reinterpret_cast<DWORD *>(strip + k) = random() % 3 ? bits((float)((int)(random() % 800) - 200) / ((random() & 1) ? 1.0f : 4.0f)) : random_float();
        int out[4];
        r = at<F1>(0x492570)(strip, ptrv(out)), mask = 0xffffffff;
        st_bytes(out, sizeof out);
        break;
    }
    case 8: {
        // a long line (up to the 201 rows)
        l->start[1] = -3000.0f + (float)(random() % 100);
        l->end[1] = 3000.0f - (float)(random() % 100);
        at<F0>(0x493850)(l);
        break;
    }
    }
    // the arena: the strips (pointers tagged)
    st(g_used[g_side]);
    for (DWORD k = 0; k + 4 <= g_used[g_side]; k += 4)
        st(tag(*reinterpret_cast<DWORD *>(g_arena[g_side] + k)));
    st(*at<DWORD *>(0x6bddbc));
    arena_off();
    return r;
}

// ---- The fatal bounce factor (slot 94) ----------------------------------------------------------------------

extern "C" {
DWORD ec_escape_esp, ec_exit_code;
DWORD __cdecl ec_call_escapable1(void *fn, void *self, void *arg);
void __cdecl ec_escape_exit(int code);
}
__asm__(".text\n"
        ".globl _ec_call_escapable1\n"
        "_ec_call_escapable1:\n"
        "  pushl %ebp\n  pushl %ebx\n  pushl %esi\n  pushl %edi\n"
        "  movl %esp, _ec_escape_esp\n"
        "  pushl 28(%esp)\n"   // arg (return address at 16, fn 20, self 24, arg 28)
        "  movl 28(%esp), %ecx\n"
        "  call *24(%esp)\n"   // thiscall, removes its argument
        "  popl %edi\n  popl %esi\n  popl %ebx\n  popl %ebp\n  ret\n"
        ".globl _ec_escape_exit\n"
        "_ec_escape_exit:\n"
        "  movl 4(%esp), %eax\n  movl %eax, _ec_exit_code\n"
        "  movl _ec_escape_esp, %esp\n"
        "  movl $0x7fff0001, %eax\n"
        "  popl %edi\n  popl %esi\n  popl %ebx\n  popl %ebp\n  ret\n");

void test_fatal_bounce(Tally &t)
{
    constexpr DWORD BAD[] = {0x3f800001, 0x40000000, 0xbf800000, 0x80000001, 0x7fc00000, 0x7f800000, 0xff800000, 0x3f8ccccd};
    CodePatch box, exit;
    patch_jump(box, 0x4d24c0, (void *)&capture_box);
    patch_jump(exit, 0x5d0e36, (void *)&ec_escape_exit);
    for (DWORD k = 0; k < sizeof BAD / 4; k++) {
        struct {
            DWORD result, exit_code, length, hash;
            const char *caption;
        } r[2];
        memset(r, 0, sizeof r);
        for (int side = 0; side < 2; side++) {
            g_side = side;
            g_seed = 900 + k;
            save_globals();
            setup_fakes();
            g_patches.on();
            CModel *m = reinterpret_cast<CModel *>(g_models[g_ents[1].model % NMODEL]);
            m->bounce_factor = as_float_v(BAD[k]);
            // the model's record for the name in the message
            BYTE rec[0x80];
            memset(rec, 0, sizeof rec);
            lstrcpyA(reinterpret_cast<char *>(rec), "Test Model");
            *reinterpret_cast<void **>(rec + 0x78) = m;
            const DWORD saved_records = *at<DWORD *>(MODEL_RECORDS), saved_count = *at<DWORD *>(MODEL_COUNT);
            *at<DWORD *>(MODEL_RECORDS) = ptrv(rec);
            *at<DWORD *>(MODEL_COUNT) = 1;
            BYTE motion[0x38];
            at<void *(THISCALL *)(void *, DWORD, DWORD)>(0x545a00)(motion, 0, 1);
            g_box_caption = nullptr;
            g_box_length = 0;
            g_box_text[0] = 0;
            ec_exit_code = 0;
            if (side == 0)
                hooks_out();
            r[side].result = ec_call_escapable1(at<void *>(0x491710), ent(1), motion);
            if (side == 0)
                hooks_in();
            r[side].exit_code = ec_exit_code;
            r[side].length = g_box_length;
            r[side].hash = text_hash(g_box_text);
            r[side].caption = g_box_caption;
            *at<DWORD *>(MODEL_RECORDS) = saved_records;
            *at<DWORD *>(MODEL_COUNT) = saved_count;
            g_patches.off();
            free_strings();
            restore_globals();
        }
        char d[200];
        wsprintfA(d, "[bounce factor %x: result %x / %x, exit %x / %x, message %u / %u]", BAD[k], r[0].result, r[1].result, r[0].exit_code,
                  r[1].exit_code, r[0].length, r[1].length);
        t.check(memcmp(&r[0], &r[1], sizeof r[0]) == 0 && r[0].result == 0x7fff0001 && r[0].exit_code == 0xffffffff && r[0].length > 0,
                "CEntityMovable", d);
    }
    unpatch(exit);
    unpatch(box);
}

// ---- The arena of each side (as in checks_entities.cpp) ---------------------------------------------------

constexpr int MAX_ALLOCS = 1024;
struct Alloc {
    DWORD offset, size;
};
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
    check_region(g_arena[0], ARENA_SIZE, "entity_classes arena 0");
    check_region(g_arena[1], ARENA_SIZE, "entity_classes arena 1");
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
void arena_on(int side, BYTE pattern, bool fresh)
{
    if (fresh) {
        memset(g_arena[side], pattern, ARENA_SIZE);
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
    {"CEntityMovable", 0x108, CLASS_MOVABLE, 0x4903f0, 0x4908a0, 0x490700, 0x490770, 0x490960, 0x490f40, 0x490fa0},
    {"CEntityProjectile", 0x114, CLASS_PROJECTILE, 0x491930, 0x491c90, 0x491a40, 0x491b00, 0x491db0, 0x4920b0, 0x492030},
    {"CEntityAnimatedLine", 0xe0, CLASS_ANIMATED_LINE, 0x4926c0, 0x492890, 0x492720, 0x492740, 0x492900, 0x492f50, 0},
    {"CEntityAnimatedUpperBody", 0xe8, CLASS_UPPER_BODY, 0x494180, 0x494320, 0x4941e0, 0x494220, 0, 0, 0},
    {"CEntityStationaryCharacter", 0x12c, CLASS_STATIONARY, 0x49b400, 0x49b6f0, 0x49b4c0, 0x49b540, 0, 0, 0},
    {"CEntityWalking", 0x130, CLASS_WALKING, 0x49b810, 0x49bb00, 0x49b8c0, 0x49b940, 0x49bbc0, 0x49bcb0, 0x49bd20},
    {"CEntityFlying", 0x130, CLASS_FLYING, 0x49bde0, 0x49c0c0, 0x49be90, 0x49bf10, 0x49c180, 0, 0},
    {"CEntityCharacter", 0x12c, CLASS_CHARACTER, 0x49a930, 0x49a990, 0, 0, 0x49aa00, 0, 0},
};

using Create0 = void *(__cdecl *)();
using CreateN = void *(__fastcall *)(int, int, int);
BYTE g_obj[2][0x140];

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
    DWORD filter[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        const DWORD saved = *at<DWORD *>(BOUNCE_FILTER);
        *at<DWORD *>(BOUNCE_FILTER) = random();
        BYTE *p = static_cast<BYTE *>(pool_alloc(c.size));
        memset(p, (BYTE)random(), c.size);
        pool_free(p, c.size);
        BYTE *o = static_cast<BYTE *>(at<Create0>(c.create)());
        reused[side] = o == p;
        memcpy(g_obj[side], o, c.size);
        filter[side] = *at<DWORD *>(BOUNCE_FILTER);
        vcall<void *>(o, 1, 1);
        *at<DWORD *>(BOUNCE_FILTER) = saved;
        side_end(side);
    }
    char d[160];
    const bool same = reused[0] && reused[1] && filter[0] == filter[1] && same_region(g_obj[0], g_obj[1], c.size);
    wsprintfA(d, "[Create seed %u: block reused %d / %d, first difference +%x: %x / %x]", seed, reused[0], reused[1], g_diff_at, g_diff_a,
              g_diff_b);
    t.check(same, c.name, d);
}

// CreateArray with 0-3 elements from the arena (the whole block), destroyed through slot 1 (array, not freed)
void test_create_array(Tally &t, const ClassDef &c, int count, BYTE pattern)
{
    DWORD e[2];
    void *a[2];
    const DWORD saved = *at<DWORD *>(BOUNCE_FILTER);
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
    *at<DWORD *>(BOUNCE_FILTER) = saved;
}

// The out-of-line constructor on a pool block filled with a pattern, then the destructor (eax, all bytes
// after each); CEntityAnimatedLine has no destructor of its own (its slot 1 with flags 0)
void test_ctor_dtor(Tally &t, const ClassDef &c, unsigned seed)
{
    DWORD r[2];
    BYTE after[2][0x140];
    const DWORD saved = *at<DWORD *>(BOUNCE_FILTER);
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        BYTE *o = static_cast<BYTE *>(pool_alloc(c.size));
        memset(o, (BYTE)random(), c.size);
        r[side] = at<DWORD(THISCALL *)(void *)>(c.ctor)(o) == ptrv(o);
        memcpy(g_obj[side], o, c.size);
        if (c.dtor)
            at<void(THISCALL *)(void *)>(c.dtor)(o);
        else
            vcall<void *>(o, 1, 0);
        memcpy(after[side], o, c.size);
        pool_free(o, c.size);
        side_end(side);
    }
    *at<DWORD *>(BOUNCE_FILTER) = saved;
    char d[160];
    bool same = r[0] && r[1] && same_region(g_obj[0], g_obj[1], c.size);
    wsprintfA(d, "[ctor seed %u: eax this %d / %d, first difference +%x: %x / %x]", seed, r[0], r[1], g_diff_at, g_diff_a, g_diff_b);
    t.check(same, c.name, d);
    same = same_region(after[0], after[1], c.size);
    wsprintfA(d, "[dtor seed %u: first difference +%x: %x / %x]", seed, g_diff_at, g_diff_a, g_diff_b);
    t.check(same, c.name, d);
}

// CEntityCharacter's deleting destructor 0x49af70 on a stationary character (the local character or not)
void test_character_dtor(Tally &t, unsigned seed)
{
    DWORD s[2][3];
    const DWORD saved = *at<DWORD *>(LOCAL_CHARACTER), saved_filter = *at<DWORD *>(BOUNCE_FILTER);
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        void *o = at<Create0>(0x49b4c0)();
        const DWORD other = ptrv(o) + 0x1000;
        *at<DWORD *>(LOCAL_CHARACTER) = random() % 2 ? ptrv(o) : other;
        s[side][0] = ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x49af70)(o, 1)) == ptrv(o);
        s[side][1] = *at<DWORD *>(LOCAL_CHARACTER) == other ? 2 : *at<DWORD *>(LOCAL_CHARACTER) == 0 ? 1 : 3;
        s[side][2] = *static_cast<DWORD *>(o);
        side_end(side);
    }
    *at<DWORD *>(LOCAL_CHARACTER) = saved;
    *at<DWORD *>(BOUNCE_FILTER) = saved_filter;
    char d[120];
    wsprintfA(d, "[character dtor seed %u: %x %x / %x %x]", seed, s[0][1], s[0][2], s[1][1], s[1][2]);
    t.check(memcmp(s[0], s[1], sizeof s[0]) == 0 && s[0][0], "CEntityCharacter", d);
}

// The line reader's create function (0x492670): a real line read from recorded words, its strips in the
// arena; the line compared, then deleted
void test_line_reader(Tally &t, unsigned seed)
{
    BYTE obj[2][0xe0];
    DWORD last[2];
    CodePatch p[3];
    const DWORD saved = *at<DWORD *>(LAST_READ_OBJECT);
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        g_side = side;
        g_rnext = random();
        g_nlog[side] = 0;
        g_loghash[side] = 2166136261u;
        side_begin(side);
        patch_jump(p[0], 0x4896e0, (void *)&rec_read_block);
        patch_jump(p[1], 0x520510, (void *)&rec_read_word);
        arena_on(side, 0x5a);
        at<void(THISCALL *)(void *, void *)>(0x492670)(at<void *>(READER_ANIMATED_LINE), g_packet);
        void *o = *at<void **>(LAST_READ_OBJECT);
        last[side] = o ? 1 : 0;
        if (o) {
            memcpy(obj[side], o, 0xe0);
            for (DWORD k = 0; k < 0xe0; k += 4)
                *reinterpret_cast<DWORD *>(obj[side] + k) = rel(*reinterpret_cast<DWORD *>(obj[side] + k), side);
            vcall<void *>(o, 1, 1);
        }
        arena_off();
        unpatch(p[1]);
        unpatch(p[0]);
        side_end(side);
    }
    *at<DWORD *>(LAST_READ_OBJECT) = saved;
    char d[160];
    const bool same = last[0] && last[1] && g_loghash[0] == g_loghash[1] && same_region(obj[0], obj[1], 0xe0) && g_used[0] == g_used[1] &&
                      same_region(g_arena[0], g_arena[1], g_used[0]);
    wsprintfA(d, "[line reader seed %u: %u / %u bytes, first difference +%x: %x / %x]", seed, g_used[0], g_used[1], g_diff_at, g_diff_a,
              g_diff_b);
    t.check(same, "CEntityAnimatedLine", d);
}

// A model of the model list that is loaded (so the converter's copy loads none)
DWORD loaded_model()
{
    const BYTE *r = *at<const BYTE **>(MODEL_RECORDS);
    const DWORD n = *at<DWORD *>(MODEL_COUNT);
    for (DWORD i = 0; r && i < n; i++)
        if (*reinterpret_cast<void *const *>(r + i * 0x80 + 0x78) && !r[i * 0x80 + 0x7f])
            return i;
    return 0;
}

// The CEntityAnimated converter (0x492350) on a real CEntityBase: the new entity compared, both deleted
void test_converter(Tally &t, unsigned seed)
{
    BYTE obj[2][0xe4];
    DWORD ok[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        BYTE conv[0x20];
        memset(conv, 0, sizeof conv);
        *reinterpret_cast<DWORD *>(conv) = 0x5f1fe0;
        constexpr const char *FROM[] = {nullptr, "Model", "Position/Position X", "Nope", "Cur Sequence"};
        constexpr const char *TO[] = {"Model", "Position/Position X", "Position/Position Y", "Nope", "Play Ratio"};
        *reinterpret_cast<const char **>(conv + 0xc) = FROM[random() % 5];
        *reinterpret_cast<const char **>(conv + 0x10) = TO[random() % 5];
        Entity *o = static_cast<Entity *>(at<Create0>(0x487670)());
        o->x = bounded_float();
        o->y = bounded_float();
        o->model = loaded_model();
        void *n = at<void *(THISCALL *)(void *, void *)>(0x492350)(conv, o);
        ok[side] = n != nullptr;
        if (n) {
            memcpy(obj[side], n, 0xe4);
            vcall<void *>(n, 1, 1);
        }
        side_end(side);
    }
    char d[160];
    const bool same = ok[0] && ok[1] && same_region(obj[0], obj[1], 0xe4);
    wsprintfA(d, "[converter seed %u: first difference +%x: %x / %x]", seed, g_diff_at, g_diff_a, g_diff_b);
    t.check(same, "CEntityAnimated converter", d);
}

// ---- Class getters and RegisterFields ------------------------------------------------------------------------

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
    self->parent = parent;
    *var = self;
    g_register = {ptrv(var), ptrv(name), bits(version), ptrv(parent), size, abstract & 0xff, ptrv(create), ptrv(create_array),
                  ptrv(register_fields)};
    g_nregister++;
    return self;
}

void test_getter(Tally &t, const ClassDef &c, DWORD getter_address)
{
    CClassInfo *&var = *at<CClassInfo **>(c.var);
    CClassInfo *const saved = var;
    RegisterCall calls[2];
    DWORD results[2], set[2];
    for (int side = 0; side < 2; side++) {
        side_begin(side);
        set[side] = ptrv(getter(getter_address));
        var = nullptr;
        g_nregister = 0;
        memset(&g_register, 0, sizeof g_register);
        CodePatch p;
        patch_jump(p, 0x415780, (void *)&fake_register);
        void *made = getter(getter_address);
        unpatch(p);
        results[side] = (made == var ? 1 : 0) + (DWORD)g_nregister * 2;
        calls[side] = g_register;
        if (var)
            pool_free(var, sizeof(CClassInfo));
        var = saved;
        side_end(side);
    }
    char d[180];
    wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, size %x / %x, version %x / %x, parent %x / %x]", getter_address, set[0], set[1],
              results[0], results[1], calls[0].size, calls[1].size, calls[0].version, calls[1].version, calls[0].parent,
              calls[1].parent);
    t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
            c.name, d);
}

// The stack below the caller filled with a pattern: locals the code leaves partly unset (the colour of
// CEntityAnimatedLine's RegisterFields, copied with its unset byte 7) then hold the same bytes on both sides
__attribute__((noinline)) void fill_stack(BYTE v)
{
    volatile BYTE buf[0x1000];
    for (DWORD i = 0; i < sizeof buf; i++)
        buf[i] = v;
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
    memset(&g_register, 0, sizeof g_register);
    CodePatch p;
    patch_jump(p, 0x415780, (void *)&fake_register);
    g_fake_class[side] = ptrv(var);
    fill_stack(pattern);
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
        // a colour field's default (+0x28): its 16-bit value (+0x30) was computed for the display at startup
        // (Pixel16_FromRGB 0x433a10), and its byte +0xa is copied unset from the registration's local colour
        if (*reinterpret_cast<const DWORD *>(real) == 0x5efe30 && i == 0x30)
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
        if (!same && t.mismatches < 4) {
            const DWORD *a0 = reinterpret_cast<const DWORD *>(g_arena[0] + (g_diff_at & ~0x1fu)), *a1 = reinterpret_cast<const DWORD *>(g_arena[1] + (g_diff_at & ~0x1fu));
            log("    arena +%x: %x %x %x %x %x %x %x %x / %x %x %x %x %x %x %x %x", g_diff_at & ~0x1fu, a0[0], a0[1], a0[2], a0[3], a0[4], a0[5],
                a0[6], a0[7], a1[0], a1[1], a1[2], a1[3], a1[4], a1[5], a1[6], a1[7]);
        }
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
        real.check(same_real, c.name, d);
        free_made(1);
    }
    *at<DWORD *>(FIND_FIELD_CACHE) = saved_cache;
}

// ---- Real data: copies of the entities of the running level -----------------------------------------------

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
    if (is_kind_real(p, g_classes[0]))
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
    CodePatch p[5];
    void on()
    {
        patch_jump(p[0], 0x51ff50, (void *)&rec_block_type);
        patch_jump(p[1], 0x520940, (void *)&rec_write_word);   // the dword: the same record kind with all bits
        patch_jump(p[2], 0x5209c0, (void *)&rec_write_word);
        patch_jump(p[3], 0x520820, (void *)&rec_write_bool);
        patch_jump(p[4], 0x520a50, (void *)&rec_write_bool);
    }
    void off()
    {
        for (int i = 4; i >= 0; i--)
            unpatch(p[i]);
    }
};

alignas(4) BYTE g_copy_obj[0x140];

void test_real(Tally &t, Tally &blocks)
{
    if (!find_real()) {
        log("  entity classes real data: no level grid found");
        t.check(false, "entity classes", "[no level grid]");
        return;
    }
    int calls = 0, nblocks = 0, nmovable = 0, ncharacter = 0, nline = 0, nupper = 0;
    // the block cases reached and those that write no block by design (a part with children or attached objects, a
    // character whose 0x48b590 says no): a level whose characters all carry an upper body (ACT 4/M22/22 Ruined Main)
    // writes none, which is not a failure of the case
    int nblock_cases = 0, nblock_skipped = 0;
    for (int n = 0; n < g_nreal; n++) {
        Entity *e = static_cast<Entity *>(g_real[n]);
        if (!model_loaded(e))
            continue;
        CClassInfo *c = class_of(e);
        if (!c || c->size > sizeof g_copy_obj || !readable(e, c->size))
            continue;
        const bool animated = is_kind_real(e, g_classes[2]);
        const bool movable = is_kind_real(e, g_classes[3]);
        const bool character = is_kind_real(e, g_classes[4]);
        const bool projectile = is_kind_real(e, g_classes[8]);
        const bool line = is_kind_real(e, getter(0x4926c0));
        const bool upper = is_kind_real(e, getter(0x494180));
        nmovable += movable;
        ncharacter += character;
        nline += line;
        nupper += upper;
        if (!animated && !line)
            continue;
        Entity *other = static_cast<Entity *>(g_real[(n + 1) % g_nreal]);
        for (int op = 0; op < 28; op++) {
            const bool run = op < 6 ? animated : op < 16 ? movable : op < 18 ? projectile : op < 23 ? character : op < 25 ? line : op < 27 ? upper : true;
            if (!run)
                continue;
            calls++;
            two_sides(t, "real entity", (unsigned)(n * 64 + op), [&](DWORD &mask) -> DWORD {
                mask = 0xffffffff;
                // a copy of the entity (the functions that write only touch it)
                memcpy(g_copy_obj, e, c->size);
                Walking *k = reinterpret_cast<Walking *>(g_copy_obj);
                DWORD r = 0;
                switch (op) {
                case 0: return at<F1>(0x490230)(e, 0);
                case 1: return at<F1>(0x490280)(e, ptrv(g_classes[11]));
                case 2: {
                    const Animated *a = static_cast<Animated *>(static_cast<LayerPart *>(e));
                    for (DWORD i = 0; i < a->attached.count && i < 8; i++)
                        r = r * 7 + at<F2>(0x4902d0)(e, ptrv(a->attached.data[i]), 0) + at<F2>(0x490360)(e, ptrv(a->attached.data[i]), 0);
                    return r;
                }
                case 3: return at<FAST2>(0x49adf0)(e, other);
                case 4: {
                    CClassInfo *out = nullptr;
                    r = at<F2>(0x492200)(at<void *>(ANIMATED_CONVERTER), ptrv(e), ptrv(&out));
                    return r ^ ptrv(out);
                }
                case 5: return at<F1>(0x490230)(e, ptrv(g_classes[2]));
                // CEntityMovable
                case 6: st_ld(at<FL0>(0x4904d0)(e)); return 0;
                case 7: return at<F0>(0x4905e0)(g_copy_obj) - ptrv(g_copy_obj);
                case 8: return at<F0>(0x4906f0)(g_copy_obj) - ptrv(g_copy_obj);
                case 9: {
                    DWORD v[4];
                    at<F2>(0x4905a0)(e, ptrv(v), ptrv(v + 1));
                    at<F2>(0x4906b0)(e, ptrv(v + 2), ptrv(v + 3));
                    st_bytes(v, sizeof v);
                    return 0;
                }
                case 10: {
                    float v[4];
                    at<F1>(0x4905c0)(g_copy_obj, ptrv(v));
                    at<F1>(0x4906d0)(g_copy_obj, ptrv(v + 2));
                    st_bytes(v, sizeof v);
                    return 0;
                }
                case 11: return at<F0>(0x4914d0)(e);
                case 12: {
                    float v[2];
                    at<F1>(0x491610)(g_copy_obj, ptrv(v));
                    st_bytes(v, sizeof v);
                    return 0;
                }
                case 13: {
                    at<F1>(0x491840)(g_copy_obj, bits(37.5f));
                    st_bytes(&k->velocity, 12);
                    return 0;
                }
                case 14: {
                    float v[3] = {1.25f, -3.5f, 0.5f};
                    at<F1>(0x490560)(g_copy_obj, ptrv(v));
                    at<F1>(0x490670)(g_copy_obj, ptrv(v));
                    st_bytes(&k->velocity, 24);
                    return 0;
                }
                case 15: {
                    BYTE motion[0x38];
                    at<void *(THISCALL *)(void *, DWORD, DWORD)>(0x545a00)(motion, 0, 1);
                    const CModel *m = at<CModel *(THISCALL *)(void *)>(0x48b290)(e);
                    if (!m || !(m->bounce_factor >= 0.0f && m->bounce_factor <= 1.0f))
                        return 0;
                    r = at<F1>(0x491710)(g_copy_obj, ptrv(motion));
                    st_motion(motion);
                    return r;
                }
                // CEntityProjectile
                case 16: st_ld(at<FL0>(0x492080)(g_copy_obj)); return 0;
                case 17: {
                    BYTE motion[0x38];
                    at<void *(THISCALL *)(void *, DWORD, DWORD)>(0x545a00)(motion, 0, 1);
                    const CModel *m = at<CModel *(THISCALL *)(void *)>(0x48b290)(e);
                    if (!m || !(m->bounce_factor >= 0.0f && m->bounce_factor <= 1.0f))
                        return 0;
                    at<F1>(0x492060)(g_copy_obj, ptrv(motion));
                    st_motion(motion);
                    return 0;
                }
                // CEntityCharacter
                case 18: return at<F0>(0x49a8c0)(e);
                case 19: {
                    DWORD v[2];
                    at<F1>(0x49a900)(e, ptrv(v));
                    st_bytes(v, 8);
                    st_ld(at<FL0>(0x49b4b0)(e));
                    return 0;
                }
                case 20: {
                    // the copy's string is the entity's (no reference taken; only read)
                    r = at<F0>(0x49b1d0)(g_copy_obj) - ptrv(g_copy_obj);   // eax: +0x11c (batch 6-12)
                    st_bytes(g_copy_obj + 0x11c, 8);
                    return r;
                }
                case 21: {
                    // the multiplayer block into recorders (characters without children and attached objects)
                    const Animated *a = static_cast<Animated *>(static_cast<LayerPart *>(e));
                    nblock_cases += g_side;
                    if ((e->children && e->children->count) || a->attached.count || !at<F0>(0x48b590)(e)) {
                        nblock_skipped += g_side;
                        return 0;
                    }
                    nblocks += g_side;
                    RealPatches p;
                    p.on();
                    r = at<F2>(0x49b2a0)(e, ptrv(g_packet), (DWORD)(n & 1));
                    p.off();
                    return r;
                }
                case 22: return at<FAST2>(0x49adf0)(other, e);
                // CEntityAnimatedLine
                case 23: {
                    for (DWORD kind = 0; kind < 7; kind++) {
                        int rect[4] = {0, 0, 0, 0};
                        r = r * 3 + at<F3>(0x4931b0)(e, ptrv(rect), kind, 0);
                        st_bytes(rect, sizeof rect);
                    }
                    return r;
                }
                case 24: {
                    nblock_cases += g_side;
                    if (e->children && e->children->count) {
                        nblock_skipped += g_side;
                        return 0;
                    }
                    nblocks += g_side;
                    RealPatches p;
                    p.on();
                    r = at<F2>(0x493230)(e, ptrv(g_packet), (DWORD)(n & 1));
                    p.off();
                    return r;
                }
                // CEntityAnimatedUpperBody (the root's flags put back after its slot 46)
                case 25: case 26: {
                    g_copy_obj[0xe4] = (BYTE)(op - 25);
                    LayerPart *root = e;
                    while (root->parent)
                        root = root->parent;
                    if (root == e)
                        return 0;
                    const bool anim = is_kind_real(root, g_classes[2]);
                    const DWORD saved = anim ? static_cast<Animated *>(root)->aflags : 0;
                    st_ld(at<FL0>(0x4943b0)(g_copy_obj));
                    if (anim) {
                        st(static_cast<Animated *>(root)->aflags);
                        static_cast<Animated *>(root)->aflags = saved;
                    }
                    return 0;
                }
                default: return animated ? static_cast<Animated *>(static_cast<LayerPart *>(e))->aflags : e->flags;
                }
            });
        }
    }
    log("  entity classes real data: %d entities (%d movable, %d characters, %d lines, %d upper bodies), %d calls, %d multiplayer blocks"
        " (%d block cases, %d without a block by design)",
        g_nreal, nmovable, ncharacter, nline, nupper, calls, nblocks, nblock_cases, nblock_skipped);
    blocks.check(nblocks > 0 || ncharacter + nline == 0 || (nblock_cases > 0 && nblock_skipped == nblock_cases),
                 "entity classes", "[no multiplayer block written]");
}

}  // namespace

ZAX_CHECKS(entity_classes)
{
    (void)cls;
    (void)n;
    init_tables();
    if (!arena_setup()) {
        log("  entity classes: no arena");
        return;
    }
    if (!make_pages_writable()) {
        log("  entity classes: hook table too long or pages not writable");
        return;
    }
    // The function-local static "Idle" of 0x490060, built as the exe builds it on first use (so neither
    // side builds it and registers its atexit)
    BYTE &guard = *at<BYTE *>(0x6bdd58);
    if (!(guard & 1)) {
        guard |= 1;
        at<ZString *(THISCALL *)(void *, const char *)>(0x4dee00)(at<void *>(0x6bdd54), "Idle");
        crt_atexit(at<void(__cdecl *)()>(0x490120));
    }
    Tally slots{"entity classes: slots on fake entities"}, fatal{"entity classes: the fatal bounce factor"},
        objects{"entity classes: Create, CreateArray, constructors, destructors"}, classes{"entity classes: class getters and RegisterFields"},
        registered{"entity classes: registered descriptors (real data)"}, real{"entity classes: entities of the running level (real data)"},
        blocks{"entity classes: multiplayer blocks of the running level"};
    log("  entity classes: slots");
    g_patches.on();
    g_op = -1;
    for (unsigned seed = 1; seed <= (unsigned)scaled(3000); seed++)
        two_sides(slots, "slot", seed, [](DWORD &mask) { return slot_case(mask); });
    for (int op = 0; op < NOPS; op++)
        for (int k = 0; k < scaled(16); k++) {
            g_op = op;
            two_sides(slots, "slot op", (unsigned)(100000 + op * 100 + k), [](DWORD &mask) { return slot_case(mask); });
        }
    g_op = -1;
    g_patches.off();
    slots.report();
    log("  entity classes: strips");
    Tally strips{"entity classes: the strips of CEntityAnimatedLine"};
    for (unsigned seed = 1; seed <= (unsigned)scaled(1500); seed++)
        two_sides(strips, "strip", seed, [](DWORD &mask) { return strip_case(mask); });
    for (int op = 0; op < NSTRIP_OPS; op++)
        for (int k = 0; k < scaled(40); k++) {
            g_op = op;
            two_sides(strips, "strip op", (unsigned)(200000 + op * 100 + k), [](DWORD &mask) { return strip_case(mask); });
        }
    g_op = -1;
    strips.report();
    test_fatal_bounce(fatal);
    fatal.report();
    log("  entity classes: objects");
    for (const ClassDef &c : CLASSES) {
        if (!c.create)
            continue;
        for (unsigned seed = 1; seed <= (unsigned)scaled(30); seed++) {
            test_create(objects, c, seed);
            if (c.ctor)
                test_ctor_dtor(objects, c, seed);
        }
        for (int count = 0; count < 4; count++)
            for (int pattern = 0; pattern < 2; pattern++)
                test_create_array(objects, c, count, pattern ? 0xa5 : 0x3c);
    }
    for (unsigned seed = 1; seed <= (unsigned)scaled(20); seed++) {
        test_character_dtor(objects, seed);
        test_line_reader(objects, seed);
        test_converter(objects, seed);
    }
    objects.report();
    log("  entity classes: classes");
    for (const ClassDef &c : CLASSES) {
        test_getter(classes, c, c.getter);
        test_getter(classes, c, c.getter2);
        if (c.register_fields)
            test_register_fields(classes, registered, c, 7);
    }
    classes.report();
    registered.report();
    log("  entity classes: real data");
    test_real(real, blocks);
    real.report();
    blocks.report();
    restore_pages();
}

}  // namespace zax
