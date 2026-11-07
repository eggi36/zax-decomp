// Checks of the layer part classes (layer_parts.cpp) against the original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls run the original too), side 1 through the
// reimplementation. Compared: eax (masked where the original leaves undefined bits), an event log and
// the state of every object afterwards. Pointers are compared raw first and only mapped (fakes by
// number, the stack as one tag, blocks of the side by order) when they differ.
//  - Slot scenarios on fake parts: parts whose vtable is a table of recorder thunks (every slot logs its
//    arguments, writes deterministic out values, returns a hash or a configured float/byte), with fake
//    child lists, fake AIs (recorder vtable, the "done" byte), real broadcasters (0x467d00) and a fake
//    layer; the exe functions that reach further (the layer's +0x7c object 0x540640, 0x4ee730, the grid
//    update 0x579c10, the drawable list 0x49ca30, the font functions 0x4a7450/0x4a7380/0x4a6c80) jump to
//    recorders, the world pointer 0x6c2080 points to a fake for slots 74/75. Every base slot, the flag
//    setters, the simple part's slots and the sign's slots run on random configurations.
//  - Objects: the constructors on pool blocks filled with a pattern (all bytes compared, ZStrings by
//    text), Create/CreateArray/deleting destructors of CFreeRangePoly and CUnderConstructionLayerPart,
//    the destructors with listeners, targets, child lists and activity arrays.
//  - Real objects: CFreeRangePoly with random polygons (set through the polygon field's slot 19 as a
//    level file does: 0-12 points, duplicates, huge values) through slots 37, 38, 41, 43-45, 48-53, the
//    dynamic point slots 72/73; signs with random texts and widths.
//  - Class getters and RegisterFields (ClassInfo_Register replaced by a stand-in; descriptors compared
//    member by member and with the descriptors registered at startup; the sign's RegisterFields on a
//    copy of its class with the inherited fields, so the hidden flags are set).
//  - Real data: the parts of the running level (the grid of the CLayer found from the world 0x6c2080):
//    the read-only slots of the module in their vtables (positions, rectangles of all kinds, hit tests
//    around them, display names, targets).
#include "check_util.h"
#include "layer_parts.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);

namespace zax {

int hooks_layer_parts(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Event log and state -------------------------------------------------------------------------------

int g_side;
unsigned g_loghash[2], g_statehash[2];
int g_nlog[2];
constexpr int LOG = 512;
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
    if (g_nlog[0] != g_nlog[1])
        log("    log lengths %d / %d", g_nlog[0], g_nlog[1]);
}
void st(DWORD v) { g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4); }
void st_text(const char *s) { g_statehash[g_side] = fnv_text(g_statehash[g_side], s ? s : "(null)"); }
unsigned text_hash(const char *s) { return fnv_text(2166136261u, s ? s : "(null)"); }

// ---- Hooks of the module out and in --------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 128;
SavedSite g_sites[MAX_SITES];
int g_nsites;
void hooks_out()
{
    Hook list[MAX_SITES];
    g_nsites = 0;
    const int n = hooks_layer_parts(nullptr);
    if (!hook_sites_fit("layer_parts", n, MAX_SITES))
        return;
    hooks_layer_parts(list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (original_address(list[i].address) == site)
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, list[i].expected);
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
                                0xc0200000, 0x7fc00000, 0xffc00000, 0x7f800000, 0xff800000, 0x00000001, 0x80000001,
                                0x7f7fffff, 0x4f000000, 0xcf000000, 0x4b000000, 0x447a0000, 0x44fa0000, 0x45bb8000,
                                0x3f7fffff, 0x3effffff, 0x40400000, 0x42c80000, 0xc2c80000};
    if (random() % 3)
        return VALUES[random() % (sizeof VALUES / 4)];
    const float f = (float)((int)(random() % 8000) - 2000) + (float)(random() % 1000) / 1000.0f;
    return bits(f);
}

// ---- Fakes and the recorder thunks -----------------------------------------------------------------------

constexpr int NPART = 10, NAI = 16, NLIST = 6;
struct FakePart {
    LayerPart p;      // the layer part's members
    float x, y;       // 0x4c (simple part)
    BYTE more[0x50];  // 0x54 (sign members)
    DWORD fx, fy;     // slots 44/45 (floats)
    DWORD px, py;     // slot 43 out
    DWORD deleted;
    DWORD cls;        // slot 2
};
struct FakeAI {
    DWORD vftable;
    DWORD dynamic;
    BYTE done;
    BYTE pad[3];
    DWORD f0c;
    DWORD more[4];
    DWORD deleted;
    DWORD cls;
};
struct FakeList {
    LPChildList l;
    LayerPart *items[12];
    DWORD deleted;
};
FakePart g_parts[NPART];
FakeAI g_ais[NAI];
FakeList g_lists[NLIST];
alignas(4) BYTE g_layer[0x100];
alignas(4) BYTE g_layer_object[0x10];
FakeAI g_world;   // vtable: the "other" table
void *g_activity[NPART][12];
float g_buf[8];   // out buffers given by the checks

// Recorder vtables: table 0 parts, 1 AIs, 2 other objects (child lists, the world); every slot is a thunk
// "mov eax, id; jmp lp_common" of 10 bytes, id = table * 96 + slot
extern "C" {
extern char lp_thunks[];
DWORD lp_pop, lp_ret_float, lp_fret;
DWORD __cdecl lp_record(DWORD *args, DWORD id, BYTE *self);
}
__asm__(".text\n"
        ".globl _lp_thunks\n"
        "_lp_thunks:\n"
        ".set lp_i, 0\n"
        ".rept 288\n"
        "  movl $lp_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _lp_common - . - 4\n"
        ".set lp_i, lp_i + 1\n"
        ".endr\n"
        "_lp_common:\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _lp_record\n"
        "  addl $12, %esp\n"
        "  cmpl $0, _lp_ret_float\n"
        "  je 1f\n"
        "  flds _lp_fret\n"
        "1:\n"
        "  movl _lp_pop, %ecx\n"
        "  popl %edx\n"
        "  addl %ecx, %esp\n"
        "  jmp *%edx\n");
DWORD g_vt[3][96];

struct Spec {
    BYTE slot, nargs;
    const char *kinds;   // V value (mapped), F float bits, X text, M message, R rect out, P point out
    char ret;            // i hash, b byte, x/y the part's floats, f float, d delete, c class, q AI message
};
constexpr Spec PART_SPECS[] = {{0, 2, "VX", 'i'},     {1, 1, "V", 'd'},       {2, 0, "", 'c'},    {23, 5, "VVVVV", 'i'},
                               {25, 2, "VV", 'i'},    {29, 1, "V", 'i'},      {30, 1, "V", 'i'},  {31, 1, "V", 'i'},
                               {33, 0, "", 'i'},      {34, 0, "", 'i'},       {35, 1, "V", 'i'},  {36, 0, "", 'i'},
                               {37, 3, "RVV", 'b'},   {41, 2, "FF", 'i'},     {43, 1, "P", 'i'},  {44, 0, "", 'x'},
                               {45, 0, "", 'y'},      {50, 1, "P", 'i'},      {52, 0, "", 'i'},   {53, 0, "", 'i'},
                               {57, 1, "V", 'i'},     {58, 1, "V", 'i'},      {59, 1, "V", 'i'},  {63, 1, "M", 'b'},
                               {71, 1, "V", 'i'},     {78, 0, "", 'i'},       {81, 1, "F", 'f'},  {82, 1, "F", 'f'}};
constexpr Spec AI_SPECS[] = {{1, 1, "V", 'd'},      {2, 0, "", 'c'},       {24, 1, "V", 'i'},     {25, 2, "VV", 'i'},
                             {26, 2, "VV", 'i'},    {27, 2, "VV", 'i'},    {28, 2, "VV", 'i'},    {29, 2, "VM", 'q'},
                             {31, 3, "VVV", 'b'},   {33, 3, "VVV", 'b'},   {34, 5, "VVVVV", 'i'}, {35, 2, "VV", 'i'}};
constexpr Spec OTHER_SPECS[] = {{1, 1, "V", 'd'}, {7, 1, "V", 'd'}, {46, 1, "V", 'i'}, {47, 1, "V", 'i'}};

const Spec *find_spec(DWORD table, DWORD slot)
{
    const Spec *s = table == 0 ? PART_SPECS : table == 1 ? AI_SPECS : OTHER_SPECS;
    const int n = table == 0 ? (int)(sizeof PART_SPECS / sizeof *s) : table == 1 ? (int)(sizeof AI_SPECS / sizeof *s)
                                                                                 : (int)(sizeof OTHER_SPECS / sizeof *s);
    for (int i = 0; i < n; i++)
        if (s[i].slot == slot)
            return &s[i];
    return nullptr;
}

// Blocks of the side (allocated by the checks, or found) in the order they appeared
constexpr int NKNOWN = 64;
struct Known {
    DWORD at_, size;
};
Known g_known[2][NKNOWN];
int g_nknown[2];
void know(const void *p, DWORD size)
{
    if (p && g_nknown[g_side] < NKNOWN)
        g_known[g_side][g_nknown[g_side]++] = {ptrv(p), size};
}
DWORD g_stack_top;
DWORD tag(DWORD v)
{
    if (!v)
        return 0;
    auto in = [v](const void *base, DWORD size) { return v >= ptrv(base) && v < ptrv(base) + size; };
    for (int i = 0; i < NPART; i++)
        if (in(&g_parts[i], sizeof(FakePart)))
            return 0xf0000000u + ((DWORD)i << 12) + (v - ptrv(&g_parts[i]));
    for (int i = 0; i < NAI; i++)
        if (in(&g_ais[i], sizeof(FakeAI)))
            return 0xf1000000u + ((DWORD)i << 12) + (v - ptrv(&g_ais[i]));
    for (int i = 0; i < NLIST; i++)
        if (in(&g_lists[i], sizeof(FakeList)))
            return 0xf2000000u + ((DWORD)i << 12) + (v - ptrv(&g_lists[i]));
    if (in(g_layer, sizeof g_layer))
        return 0xf3000000u + (v - ptrv(g_layer));
    if (in(g_layer_object, sizeof g_layer_object))
        return 0xf3100000u;
    if (in(&g_world, sizeof g_world))
        return 0xf3200000u;
    if (in(g_buf, sizeof g_buf))
        return 0xf3300000u + (v - ptrv(g_buf));
    for (int i = g_nknown[g_side] - 1; i >= 0; i--)
        if (v >= g_known[g_side][i].at_ && v < g_known[g_side][i].at_ + g_known[g_side][i].size)
            return 0xb0000000u + (DWORD)i * 0x10000 + (v - g_known[g_side][i].at_);
    if (v <= g_stack_top && v + 0x10000 > g_stack_top)
        return 0xf4000000u;
    return v;
}

DWORD g_calls;
CClassInfo *g_classes[6];
DWORD lp_hash(DWORD a, DWORD b, DWORD c)
{
    DWORD h = fnv(fnv(fnv(2166136261u, &a, 4), &b, 4), &c, 4);
    return h ^ (h >> 13);
}

extern "C" DWORD __cdecl lp_record(DWORD *args, DWORD id, BYTE *self)
{
    const DWORD table = id / 96, slot = id % 96;
    lp_ret_float = 0;
    const Spec *sp = find_spec(table, slot);
    g_calls++;
    const DWORD me = tag(ptrv(self));
    if (!sp) {
        lp_pop = 0;
        ev('?', table * 96 + slot, me);
        return 0;
    }
    lp_pop = sp->nargs * 4u;
    DWORD h = 2166136261u;
    for (int i = 0; i < sp->nargs; i++) {
        const DWORD a = args[i];
        DWORD v;
        switch (sp->kinds[i]) {
        case 'F': v = a; break;
        case 'X': v = text_hash(reinterpret_cast<const char *>(a)); break;
        case 'M': {
            const DWORD *m = reinterpret_cast<const DWORD *>(a);
            v = fnv(fnv(fnv(2166136261u, &m[0], 4), &m[1], 4), reinterpret_cast<const BYTE *>(m) + 8, 1);
            if (m[0] == VT_QUERY_MESSAGE) {
                const DWORD c = tag(m[3]);
                v = fnv(v, &c, 4);
            }
            break;
        }
        case 'R':
        case 'P': v = tag(a); break;
        default: v = tag(a);
        }
        h = fnv(h, &v, 4);
    }
    ev(0x52000000u + table * 96 + slot, me, h, sp->nargs ? tag(args[0]) : 0);
    const DWORD r = lp_hash(table * 96 + slot, me, g_calls);
    for (int i = 0; i < sp->nargs; i++) {
        DWORD *o = reinterpret_cast<DWORD *>(args[i]);
        if (sp->kinds[i] == 'R')
            for (int k = 0; k < 4; k++)
                o[k] = (lp_hash(r, k, 1) % 4000) - 1000;
        if (sp->kinds[i] == 'P') {
            if (table == 0 && (me >> 24) == 0xf0) {
                const FakePart &f = g_parts[(me >> 12) & 0xfff];
                o[0] = f.px;
                o[1] = f.py;
            } else {
                o[0] = bits((float)(r % 1000));
                o[1] = bits((float)(r % 777));
            }
        }
    }
    FakePart *fp = (me >> 24) == 0xf0 ? &g_parts[(me >> 12) & 0xfff] : nullptr;
    FakeAI *fa = (me >> 24) == 0xf1 ? &g_ais[(me >> 12) & 0xfff] : nullptr;
    switch (sp->ret) {
    case 'b': return (r & ~0xffu) | ((r >> 9) & 1);
    case 'x':
    case 'y':
        lp_ret_float = 1;
        lp_fret = fp ? (sp->ret == 'x' ? fp->fx : fp->fy) : random_float();
        return r;
    case 'f':
        lp_ret_float = 1;
        lp_fret = (r & 1) ? args[0] : bits((float)(r % 100));
        return r;
    case 'd':
        if (fp)
            fp->deleted++;
        if (fa)
            fa->deleted++;
        if ((me >> 24) == 0xf2)
            g_lists[(me >> 12) & 0xfff].deleted++;
        return ptrv(self);
    case 'c': {
        DWORD c = fp ? fp->cls : fa ? fa->cls : 0;
        return ptrv(g_classes[c % 6]);
    }
    case 'q': {
        DWORD *m = reinterpret_cast<DWORD *>(args[1]);
        if (m[0] == VT_QUERY_MESSAGE && (r & 0x30) == 0x30) {
            reinterpret_cast<BYTE *>(m)[8] = (r >> 6) & 1;
            m[4] = ptrv(&g_ais[r % NAI]);
        }
        return (r & ~0xffu) | ((r >> 9) & 1);
    }
    }
    return r;
}

// ---- Stand-ins for the exe functions that reach further ------------------------------------------------

void THISCALL rec_layer_object(void *obj, int *rect, DWORD flag)
{
    ev('L', tag(ptrv(obj)), fnv(2166136261u, rect, 16), flag);
}
void THISCALL rec_layer_flags(void *layer, void *part, DWORD old, DWORD bits_)
{
    ev('F', tag(ptrv(layer)), tag(ptrv(part)), old ^ (bits_ << 1));
}
void THISCALL rec_grid_update(void *grid, void *part) { ev('G', tag(ptrv(grid)), tag(ptrv(part))); }
void THISCALL rec_drawable(void *list, DWORD *d) { ev('D', tag(ptrv(list)), tag(ptrv(d)), fnv(2166136261u, d, 0x1c)); }
ZString *THISCALL rec_wrap(void *font, ZString *out, const char *text, int width)
{
    ev('W', tag(ptrv(font)), text_hash(text), (DWORD)width);
    at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(out, text);
    return out;
}
int THISCALL rec_width(void *font, const char *text)
{
    ev('w', tag(ptrv(font)), text_hash(text));
    return lstrlenA(text) * 7;
}
int THISCALL rec_height(void *font, const char *text, DWORD k)
{
    ev('h', tag(ptrv(font)), text_hash(text), k);
    return lstrlenA(text) / 3 + 11;
}

struct Patches {
    CodePatch p[7];
    void on()
    {
        patch_jump(p[0], 0x540640, (void *)&rec_layer_object);
        patch_jump(p[1], 0x4ee730, (void *)&rec_layer_flags);
        patch_jump(p[2], 0x579c10, (void *)&rec_grid_update);
        patch_jump(p[3], 0x49ca30, (void *)&rec_drawable);
        patch_jump(p[4], 0x4a7450, (void *)&rec_wrap);
        patch_jump(p[5], 0x4a7380, (void *)&rec_width);
        patch_jump(p[6], 0x4a6c80, (void *)&rec_height);
    }
    void off()
    {
        for (int i = 6; i >= 0; i--)
            unpatch(p[i]);
    }
};
Patches g_patches;

// ---- Setting up the fakes ------------------------------------------------------------------------------

void init_tables()
{
    for (int t = 0; t < 3; t++)
        for (int s = 0; s < 96; s++)
            g_vt[t][s] = ptrv(lp_thunks) + 10u * (t * 96 + s);
    for (DWORD k = 0; k < 288; k++) {
        const BYTE *th = reinterpret_cast<const BYTE *>(lp_thunks) + 10 * k;
        if (th[0] != 0xb8 || *reinterpret_cast<const DWORD *>(th + 1) != k || th[5] != 0xe9) {
            log("  layer parts: recorder thunk %u is broken", k);
            break;
        }
    }
    g_classes[0] = at<CClassInfo *(__cdecl *)()>(0x4163b0)();   // CObject
    g_classes[1] = at<CClassInfo *(__cdecl *)()>(0x4fcb00)();   // CAIBase
    g_classes[2] = at<CClassInfo *(__cdecl *)()>(0x4f9bf0)();   // CLayerPart
    g_classes[3] = *at<CClassInfo **>(0x6c1d04);                // CLayer
    g_classes[4] = at<CClassInfo *(__cdecl *)()>(0x467830)();   // CBroadcaster
    g_classes[5] = g_classes[1];
}

LayerPart *part(int i) { return &g_parts[i].p; }

// Random fake parts: flags, parents, children lists, AIs; the same on both sides (seeded)
void setup_fakes()
{
    memset(g_parts, 0, sizeof g_parts);
    memset(g_ais, 0, sizeof g_ais);
    memset(g_lists, 0, sizeof g_lists);
    memset(g_layer, 0, sizeof g_layer);
    memset(g_activity, 0, sizeof g_activity);
    g_world = {};
    g_world.vftable = ptrv(g_vt[2]);
    *reinterpret_cast<DWORD *>(g_layer + 0x7c) = random() % 4 ? ptrv(g_layer_object) : 0;
    for (int i = 0; i < NAI; i++) {
        g_ais[i].vftable = ptrv(g_vt[1]);
        g_ais[i].done = random() % 4 == 0;
        g_ais[i].cls = random() % 6;
        g_ais[i].f0c = random() % 2 ? ptrv(&g_ais[(i + 1) % NAI]) : 0;
    }
    for (int i = 0; i < NLIST; i++) {
        g_lists[i].l.vftable = ptrv(g_vt[2]);
        g_lists[i].l.data = g_lists[i].items;
        g_lists[i].l.count = random() % 7;
        g_lists[i].l.capacity = 12;
    }
    for (int i = 0; i < NPART; i++) {
        FakePart &f = g_parts[i];
        LayerPart &p = f.p;
        p.vftable = ptrv(g_vt[0]);
        p.name.buffer = nullptr;
        const DWORD flag_bits[] = {0x10000, 0x20000, 0x40000, 0x200000, 0x400000, 0x800000, 0x4000000, 0x8000000};
        p.flags = 0;
        for (DWORD b : flag_bits)
            if (random() % 2)
                p.flags |= b;
        if (random() % 5 == 0)
            p.flags = random();
        p.parent = random() % 3 == 0 && i > 0 ? part(random() % i) : nullptr;
        p.children = random() % 3 ? &g_lists[random() % NLIST].l : nullptr;
        p.layer = random() % 4 ? g_layer : nullptr;
        p.team = random();
        p.id = (WORD)random();
        p.cells[0] = (short)random();
        p.activity.vftable = VT_ACTIVITY;
        p.activity.data = g_activity[i];
        p.activity.count = random() % 6;
        p.activity.capacity = 12;
        for (DWORD k = 0; k < p.activity.count; k++)
            g_activity[i][k] = random() % 6 ? &g_ais[random() % NAI] : nullptr;
        f.fx = random_float();
        f.fy = random_float();
        f.px = random_float();
        f.py = random_float();
        f.x = (float)(random() % 3000);
        memcpy(&f.x, &f.px, 4);
        memcpy(&f.y, &f.py, 4);
        f.cls = 0;
    }
    for (int i = 0; i < NLIST; i++)
        for (DWORD k = 0; k < g_lists[i].l.count; k++)
            g_lists[i].items[k] = random() % 6 ? part(random() % NPART) : nullptr;
}

// The state of all fakes (pointers tagged; real broadcasters by their listener count and listeners)
void st_broadcaster(const BYTE *bc)
{
    if (!bc) {
        st(0);
        return;
    }
    const DWORD n = *reinterpret_cast<const DWORD *>(bc + 0x10);
    st(n);
    void *const *refs = *reinterpret_cast<void *const *const *>(bc + 0xc);
    for (DWORD i = 0; i < n && i < 16; i++)
        st(tag(*reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(refs[i]) + 8)));
}
void st_fakes()
{
    for (int i = 0; i < NPART; i++) {
        const FakePart &f = g_parts[i];
        const LayerPart &p = f.p;
        st(tag(p.vftable));
        st(tag(ptrv(p.children)));
        st(tag(ptrv(p.parent)));
        st_broadcaster(p.broadcaster);
        st(tag(ptrv(p.target)));
        st(p.flags);
        st(p.used_in);
        st(p.team);
        st(p.id | (DWORD)p.saved_flags << 16);
        st(p.activity.count);
        for (DWORD k = 0; k < p.activity.count && k < 12; k++)
            st(tag(ptrv(p.activity.data[k])));
        st(tag(ptrv(p.layer)));
        st(f.deleted);
        st(bits(f.x));
        st(bits(f.y));
        g_statehash[g_side] = fnv(g_statehash[g_side], f.more, sizeof f.more);
    }
    for (int i = 0; i < NAI; i++)
        st(g_ais[i].deleted + g_ais[i].done * 0x100);
    for (int i = 0; i < NLIST; i++) {
        st(g_lists[i].l.count);
        st(g_lists[i].deleted);
        for (DWORD k = 0; k < g_lists[i].l.count && k < 12; k++)
            st(tag(ptrv(g_lists[i].items[k])));
    }
}

DWORD g_dump[2][96][3];
int g_ndump[2];
void dump(DWORD off, DWORD v, DWORD raw)
{
    if (g_ndump[g_side] < 96)
        g_dump[g_side][g_ndump[g_side]][0] = off, g_dump[g_side][g_ndump[g_side]][2] = raw, g_dump[g_side][g_ndump[g_side]++][1] = v;
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
        g_nlog[side] = g_nknown[side] = 0;
        g_ndump[side] = 0;
        DWORD top = 0;
        g_stack_top = ptrv(&top);
        if (side == 0)
            hooks_out();
        r[side] = run(mask);
        if (side == 0)
            hooks_in();
    }
    char d[200];
    wsprintfA(d, "[%s seed %u: eax %x / %x (mask %x), log %d %x / %d %x, state %x / %x]", what, seed, r[0], r[1], mask,
              g_nlog[0], g_loghash[0], g_nlog[1], g_loghash[1], g_statehash[0], g_statehash[1]);
    const bool same = ((r[0] ^ r[1]) & mask) == 0 && g_loghash[0] == g_loghash[1] && g_nlog[0] == g_nlog[1] &&
                      g_statehash[0] == g_statehash[1];
    if (!same && t.mismatches < 10) {
        log_first_difference();
        for (int i = 0; i < g_ndump[0] && i < g_ndump[1]; i++)
            if (g_dump[0][i][1] != g_dump[1][i][1]) {
                log("    object dword +0x%x differs first (dump %d): %x / %x", g_dump[0][i][0], i, g_dump[0][i][2], g_dump[1][i][2]);
                break;
            }
    }
    t.check(same, "layer parts", d);
    return same;
}

// ---- Slot scenarios on fake parts ----------------------------------------------------------------------

using F0 = DWORD(THISCALL *)(void *);
using F1 = DWORD(THISCALL *)(void *, DWORD);
using F2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
using F3 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD);
using F4 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD);
using F5 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD);
using FF0 = long double(THISCALL *)(void *);
using FF1 = long double(THISCALL *)(void *, DWORD);

DWORD random_arg()
{
    switch (random() % 4) {
    case 0: return 0;
    case 1: return random() % 100;
    case 2: return ptrv(part(random() % NPART));
    default: return random();
    }
}
DWORD random_flags()
{
    constexpr DWORD BITS[] = {0x10000, 0x20000, 0x40000, 0x80000, 0x200000, 0x400000, 0x800000, 0x2000000, 0x4000000, 0x8000000};
    DWORD f = 0;
    for (DWORD b : BITS)
        if (random() % 3 == 0)
            f |= b;
    return random() % 6 ? f : random();
}

// One operation on a random fake part; eax mask per function
DWORD slot_case(DWORD &mask)
{
    setup_fakes();
    for (DWORD &v : reinterpret_cast<DWORD(&)[8]>(g_buf))
        v = random_float();
    const int op = (int)(random() % 40);
    LayerPart *p = part(random() % NPART);
    const DWORD a = random_arg(), b = random_arg(), c = random_arg(), d = random_arg(), e = random_arg();
    DWORD r = 0;
    mask = 0xffffffff;
    switch (op) {
    case 0: r = at<F5>(0x4fa9a0)(p, a, b, c, d, e); break;
    case 1: r = at<F1>(0x4faa90)(p, a); break;
    case 2: at<F1>(0x4fab00)(p, a), mask = 0; break;
    case 3: at<F1>(0x4fab60)(p, a), mask = 0; break;
    case 4: at<F1>(0x4fabd0)(p, a), mask = 0; break;
    case 5: at<F0>(0x4fac20)(p), mask = 0; break;
    case 6: at<F0>(0x4face0)(p), mask = 0; break;
    case 7: at<F1>(0x4fadc0)(p, a), mask = 0; break;
    case 8: r = tag(at<F0>(0x4faf30)(p)); break;
    case 9:
        at<F1>(0x4fafa0)(p, ptrv(g_buf)), mask = 0;
        st(*(DWORD *)&g_buf[0]);
        st(*(DWORD *)&g_buf[1]);
        break;
    case 10: at<F1>(0x4fb490)(p, a), mask = 0; break;
    case 11: at<F1>(0x4fb4d0)(p, random_flags()), mask = 0; break;
    case 12: at<F1>(0x4fb540)(p, random_flags()), mask = 0; break;
    case 13: at<F1>(0x4fb590)(p, random_flags()), mask = 0; break;
    case 14: {
        DWORD m[3] = {VT_MESSAGE, random(), random() % 2};
        r = at<F1>(0x4fb880)(p, ptrv(m)) & 0xff;
        break;
    }
    case 15: r = tag(at<F2>(0x4fbae0)(p, ptrv(g_classes[random() % 6]), random() % 3 ? random() : 0)); break;
    case 16: r = at<F2>(0x4fbe10)(p, a, b); break;
    case 17: r = at<F2>(0x4fbe60)(p, a, b); break;
    case 18: r = at<F4>(0x4fbeb0)(p, a, b, c, d); break;
    case 19: r = at<F0>(0x4fc170)(p); break;
    case 20: r = tag(at<F0>(0x4fc260)(p)); break;
    case 21: {
        // OnMessage: one of the three names (case varied) or another, from the target or not
        constexpr const char *TEXTS[] = {"Died", "dElEtE", "Leaving Scope", "Hello", "", "died ", "LEAVING SCOPE"};
        if (random() % 2)
            p->target = part(random() % NPART);
        LayerPart *sender = random() % 2 ? p->target : part(random() % NPART);
        at<F2>(0x4fc270)(p, ptrv(sender), ptrv(TEXTS[random() % 7]));
        mask = 0;
        break;
    }
    case 22: {
        // SetTarget with real broadcasters on the old and new targets
        LayerPart *old = random() % 2 ? part(random() % NPART) : nullptr;
        LayerPart *nw = random() % 3 ? part(random() % NPART) : nullptr;
        if (old && old != p) {
            p->target = old;
            if (random() % 3) {
                at<DWORD(THISCALL *)(void *, void *, DWORD, const char *)>(0x4fc1b0)(old, p, 0xffffffff, at<const char *>(0x62e558));
                if (random() % 2)
                    at<DWORD(THISCALL *)(void *, void *, DWORD, const char *)>(0x4fc1b0)(old, part(random() % NPART), 0xffffffff,
                                                                                        at<const char *>(0x62e558));
            }
        }
        if (nw && random() % 2 && !nw->broadcaster)
            at<DWORD(THISCALL *)(void *, void *, DWORD, const char *)>(0x4fc1b0)(nw, part(random() % NPART), 0xffffffff,
                                                                                at<const char *>(0x62e558));
        r = at<F1>(0x4fc2e0)(p, ptrv(nw));
        st_fakes();
        // Clean up the broadcasters the case made (the same way on both sides)
        for (int i = 0; i < NPART; i++)
            if (BYTE *bc = part(i)->broadcaster) {
                vcall<void *>(bc, 1, 1);
                part(i)->broadcaster = nullptr;
            }
        return r;
    }
    case 23: {
        // Slots 74/75 with a fake world
        void *&world = *at<void **>(0x6c2080);
        void *saved = world;
        world = &g_world;
        r = at<F0>(random() % 2 ? 0x4fc5e0 : 0x4fc600)(p);
        world = saved;
        break;
    }
    case 24: at<F0>(0x5717e0)(p), mask = 0; break;
    case 25: at<F1>(0x5001d0)(p, random_flags()), mask = 0; break;
    case 26: r = tag(at<F1>(0x487210)(p, ptrv(g_buf))); break;
    case 27: at<F1>(0x58fc90)(p, random_flags()), mask = 0; break;
    case 28: at<F0>(0x4fa8c0)(p), mask = 0; break;
    case 29: {
        const DWORD fn[] = {0x4fa900, 0x4fa930, 0x4fa970};
        r = at<F1>(fn[random() % 3])(p, random() % 3 ? random() % 2 : random());
        break;
    }
    case 30: r = at<F2>(0x4fe730)(p, random_float(), random_float()); break;
    case 31: r = tag(at<F1>(0x4fe760)(p, ptrv(g_buf))), st(*(DWORD *)&g_buf[0]), st(*(DWORD *)&g_buf[1]); break;
    case 32: r = bits((float)at<FF0>(random() % 2 ? 0x487220 : 0x487230)(p)), mask = 0xffffffff; break;
    case 33: r = at<F0>(random() % 2 ? 0x4fe780 : 0x4fe7a0)(p); break;
    case 34: {
        long double v = at<FF1>(random() % 2 ? 0x48a0a0 : 0x48a0d0)(p, random_float());
        float fv = (float)v;
        r = bits(fv);
        break;
    }
    case 35: {
        long double v = at<FF1>(random() % 2 ? 0x571800 : 0x571810)(p, random_float());
        float fv = (float)v;
        r = bits(fv);
        break;
    }
    case 36: {
        // Slot 23 of the sign with a fake list (0x49ca30 recorded)
        r = at<F5>(0x58fa30)(p, a, random() % 3 ? ptrv(&g_world) : 0, c, d, e);
        break;
    }
    case 37: {
        // Rectangle and hit test of the sign (its text size set)
        auto *s = reinterpret_cast<SignPart *>(p);
        s->text_width = (int)(random() % 600) - (random() % 4 == 0 ? 300 : 0);
        s->text_height = (int)(random() % 200);
        int off[2] = {(int)(random() % 200) - 100, (int)(random() % 200) - 100};
        int rect[4];
        if (random() % 2) {
            r = at<F3>(0x58fab0)(p, ptrv(rect), random() % 6, random() % 2 ? ptrv(off) : 0) & 0xff;
            st(rect[0]), st(rect[1]), st(rect[2]), st(rect[3]);
        } else {
            r = at<F3>(0x58fb60)(p, (DWORD)((int)(random() % 3000) - 1000), (DWORD)((int)(random() % 3000) - 1000), random() % 6);
        }
        break;
    }
    case 38: {
        // The sign's setters (slot 78 recorded)
        auto *s = reinterpret_cast<SignPart *>(p);
        if (random() % 2) {
            r = at<F1>(0x58fc30)(p, random() % 3 ? random() % 1000 : random());
        } else {
            ZString t;
            at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&t, random() % 2 ? "Bridge out" : "");
            at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&s->text);
            r = at<F1>(0x58fc60)(p, ptrv(&t)) == ptrv(&s->text);
            st_text(s->text.c_str());
            at<void(THISCALL *)(ZString *)>(0x4defd0)(&t);
            at<void(THISCALL *)(ZString *)>(0x4defd0)(&s->text);
            s->text.buffer = nullptr;
        }
        break;
    }
    default: {
        // Slot 61 of the sign and the free range poly's small slots on the fake memory
        auto *s = reinterpret_cast<SignPart *>(p);
        at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&s->text, random() % 2 ? "Closed" : "");
        st_text(reinterpret_cast<const char *>(at<F0>(0x58f4c0)(p)));
        at<void(THISCALL *)(ZString *)>(0x4defd0)(&s->text);
        s->text.buffer = nullptr;
        r = 0;
        break;
    }
    }
    st_fakes();
    return r;
}

// ---- Objects -------------------------------------------------------------------------------------------

bool readable(const void *p, DWORD n)
{
    struct {
        void *base, *allocation_base;
        DWORD allocation_protect;
        size_t size;
        DWORD state, protect, type;
    } mi;
    if (ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;   // kernel half, or the range wraps
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
bool heap_pointer(const void *p) { return ptrv(p) >= 0x10000 && !(ptrv(p) & 3) && (ptrv(p) < 0x400000 || ptrv(p) >= IMAGE_END); }

// The bytes of an object (ZStrings at the given offsets by text; the polygon's arrays by content)
void st_object(const BYTE *o, DWORD size, const DWORD *strings, int nstrings, DWORD polygon)
{
    const unsigned before = g_statehash[g_side];
    for (DWORD i = 0; i < size; i += 4) {
        const DWORD v = *reinterpret_cast<const DWORD *>(o + i);
        bool is_string = false;
        for (int k = 0; k < nstrings; k++)
            is_string |= strings[k] == i;
        if (is_string) {
            const ZString *z = reinterpret_cast<const ZString *>(o + i);
            st_text(z->buffer ? z->c_str() : nullptr);
        } else if (polygon && (i == polygon || i == polygon + 4)) {
            const ZPolygon *pg = reinterpret_cast<const ZPolygon *>(o + polygon);
            st(v ? 1 : 0);
            const DWORD n = i == polygon ? pg->count * 8 : (pg->count - 1) * 12;
            if (v && pg->count && pg->count < 64 && readable(reinterpret_cast<const void *>(v), n))
                g_statehash[g_side] = fnv(g_statehash[g_side], reinterpret_cast<const void *>(v), n);
            else if (v)
                log("    polygon array +0x%x not readable: %x, count %u", i, v, pg->count);
        } else if (tag(v) == v && heap_pointer(reinterpret_cast<void *>(v)) && class_of(reinterpret_cast<void *>(v))) {
            st(ptrv(class_of(reinterpret_cast<void *>(v))));   // an object InitFields made
        } else {
            st(tag(v));
        }
        dump(i, g_statehash[g_side] ^ before, v);
    }
}

constexpr DWORD PART_STRINGS[] = {0x18};
constexpr DWORD SIGN_STRINGS[] = {0x18, 0x54, 0x58};

// A constructor on a pool block filled with a pattern, then the deleting destructor through the vtable
DWORD ctor_case(DWORD &mask)
{
    const int k = (int)(random() % 4);
    const DWORD size[] = {0x4c, 0x54, 0x74, 0x9c};
    const DWORD ctor[] = {0x4fa770, 0x571680, 0x4b5fa0, 0x58f8a0};
    BYTE *o = static_cast<BYTE *>(pool_alloc(size[k]));
    memset(o, (int)(random() & 0xff), size[k]);
    know(o, size[k]);
    const DWORD r = tag(at<F0>(ctor[k])(o));
    st_object(o, size[k], k == 3 ? SIGN_STRINGS : PART_STRINGS, k == 3 ? 3 : 1, k == 2 ? 0x4c : 0);
    if (k < 2) {
        // The abstract classes: their own destructors directly
        at<F0>(k == 0 ? 0x4fa810 : 0x5716d0)(o);
        pool_free(o, size[k]);
    } else {
        vcall<void *>(o, 1, 1);
    }
    mask = 0xffffffff;
    return r;
}

// Create / CreateArray / the deleting destructors of CFreeRangePoly and CUnderConstructionLayerPart
DWORD create_case(DWORD &mask)
{
    const bool sign = random() % 2;
    const DWORD size = sign ? 0x9c : 0x74;
    const DWORD *strings = sign ? SIGN_STRINGS : PART_STRINGS;
    const int ns = sign ? 3 : 1;
    DWORD r = 0;
    if (random() % 2) {
        // Create on a block filled with a pattern (the pool hands it out again), so every byte is defined
        BYTE *p = static_cast<BYTE *>(pool_alloc(size));
        memset(p, (int)(random() & 0xff), size);
        pool_free(p, size);
        BYTE *o = static_cast<BYTE *>(at<void *(__cdecl *)()>(sign ? 0x58f3a0 : 0x4b5d50)());
        know(o, size);
        st(o == p);
        st_object(o, size, strings, ns, sign ? 0 : 0x4c);
        const DWORD flags = random() % 2;
        r = tag(ptrv(at<void *(THISCALL *)(void *, DWORD)>(sign ? 0x58f410 : 0x4b5e00)(o, flags)));
        if (!flags)
            pool_free(o, size);
    } else {
        const int n = (int)(random() % 4);
        BYTE *e = static_cast<BYTE *>(at<void *(__fastcall *)(int, int, int)>(sign ? 0x58f3c0 : 0x4b5d90)(n, 0, 0));
        know(e ? e - 4 : nullptr, n * size + 8);   // +8: e itself for an empty array
        st(tag(ptrv(e)));
        if (!e)
            return 0;
        // Heap bytes the constructors leave as they are: +0x24 (team), the sign's position and +0x74-+0x94
        for (int i = 0; i < n; i++) {
            BYTE *o = e + i * size;
            *reinterpret_cast<DWORD *>(o + 0x24) = 0;
            if (sign) {
                // The position, the width and the text size, the drawable: InitFields leaves them as they are
                memset(o + 0x4c, 0, 8);
                memset(o + 0x64, 0, 0xc);
                memset(o + 0x74, 0, 0x24);
            }
            st_object(o, size, strings, ns, sign ? 0 : 0x4c);
        }
        const DWORD flags = 2 | random() % 2;
        r = tag(ptrv(at<void *(THISCALL *)(void *, DWORD)>(sign ? 0x58f410 : 0x4b5e00)(e, flags)));
        if (!(flags & 1))
            heap_free(e - 4);
    }
    mask = 0xffffffff;
    return r;
}

// The base destructor on a fake part: listeners, a target with its broadcaster, a child list, an
// activity array from the pool
DWORD dtor_case(DWORD &mask)
{
    setup_fakes();
    LayerPart *p = part(0);
    // The activity array grown by its own slot 2 (FreeFields frees it through the real class)
    p->activity = {VT_ACTIVITY, nullptr, 0, 0};
    for (int k = (int)(random() % 5); k > 0; k--) {
        void *item = &g_ais[random() % NAI];
        vcall<DWORD>(&p->activity, 2, &item);
    }
    know(p->activity.data, p->activity.capacity * 4);
    at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&p->name);
    if (random() % 2)
        for (int i = (int)(random() % 3); i >= 0; i--)
            at<DWORD(THISCALL *)(void *, void *, DWORD, const char *)>(0x4fc1b0)(p, part(1 + random() % 9), 0xffffffff,
                                                                                at<const char *>(0x62e558));
    LayerPart *t = random() % 3 ? part(1 + random() % 9) : nullptr;
    p->target = t;
    if (t && random() % 4) {
        at<DWORD(THISCALL *)(void *, void *, DWORD, const char *)>(0x4fc1b0)(t, p, 0xffffffff, at<const char *>(0x62e558));
        if (random() % 2)
            at<DWORD(THISCALL *)(void *, void *, DWORD, const char *)>(0x4fc1b0)(t, part(5), 0xffffffff, at<const char *>(0x62e558));
    }
    g_parts[0].cls = 0;   // FreeFields through slot 2: CObject, no fields
    const DWORD r = at<F0>(0x4fa810)(p);
    st(p->vftable);
    st(p->activity.vftable);
    st(tag(ptrv(p->activity.data)) ^ p->activity.count ^ p->activity.capacity << 8);
    st(tag(ptrv(p->target)) ^ tag(ptrv(p->children)));
    st_fakes();
    for (int i = 0; i < NPART; i++)
        if (BYTE *bc = part(i)->broadcaster) {
            vcall<void *>(bc, 1, 1);
            part(i)->broadcaster = nullptr;
        }
    (void)r;
    mask = 0;   // CObject_Dtor's eax
    return 0;
}

// ---- Real objects: free range polygons and signs --------------------------------------------------------

// A random polygon text as in a level file
void polygon_text(char *out, int n)
{
    out[0] = 0;
    int x0 = 0, y0 = 0;
    for (int i = 0; i < n; i++) {
        int x, y;
        if (i == n - 1 && n > 2 && random() % 2) {
            x = x0, y = y0;   // closed
        } else if (random() % 8 == 0 && i) {
            x = x0 + (int)(random() % 3) - 1, y = y0;
        } else {
            const int range = random() % 6 == 0 ? 2000000 : 4000;
            x = (int)(random() % (DWORD)range) - range / 4;
            y = (int)(random() % (DWORD)range) - range / 4;
        }
        if (i == 0)
            x0 = x, y0 = y;
        char item[40];
        if (random() % 4 == 0)
            wsprintfA(item, "%s%d.5, %d.25", i ? ", " : "", x, y);
        else
            wsprintfA(item, "%s%d, %d", i ? ", " : "", x, y);
        lstrcatA(out, item);
    }
}

CFieldDesc *polygon_field()
{
    CClassInfo *c = *at<CClassInfo **>(CLASS_FREE_RANGE_POLY);
    for (DWORD i = 0; c && i < c->num_fields; i++)
        if (c->fields[i]->offset == 0x4c && c->fields[i]->vftable == 0x5fd2a0)
            return c->fields[i];
    return nullptr;
}

DWORD poly_case(DWORD &mask)
{
    FreeRangePoly *o = static_cast<FreeRangePoly *>(at<void *(__cdecl *)()>(0x4b5d50)());
    know(o, 0x74);
    char text[700];
    polygon_text(text, (int)(random() % 13));
    if (CFieldDesc *f = polygon_field())
        vcall<DWORD>(f, 19, o, text, 0);
    DWORD r = 0;
    mask = 0xffffffff;
    const int x = (int)(random() % 5000) - 1000, y = (int)(random() % 5000) - 1000;
    const DWORD op = random() % 12;
    switch (op) {
    case 0: {
        int rect[4] = {0, 0, 0, 0};
        const DWORD kind = random() % 8;
        DWORD *pt = reinterpret_cast<DWORD *>(o) + 0x6c / 4;
        const DWORD saved[2] = {pt[0], pt[1]};
        if (kind == 4 && random() % 2)
            pt[0] = random_float(), pt[1] = random_float();   // the point kind 4 reads
        r = at<F3>(0x4b61e0)(o, ptrv(rect), kind, 0);
        pt[0] = saved[0], pt[1] = saved[1];
        mask = kind > 5 ? 0xffffffff : 0xff;
        st(rect[0]), st(rect[1]), st(rect[2]), st(rect[3]);
        break;
    }
    case 1: r = at<F3>(0x4b6180)(o, (DWORD)x, (DWORD)y, random() % 6) & 0xff, mask = 0xff; break;   // upper bytes: fnstsw's
    case 2: r = at<F2>(0x4b60a0)(o, random_float(), random_float()), mask = 0; break;   // slot 78's eax
    case 3: r = tag(at<F1>(0x4b6100)(o, ptrv(g_buf))), st(*(DWORD *)&g_buf[0]), st(*(DWORD *)&g_buf[1]); break;
    case 4: r = bits((float)at<FF0>(random() % 2 ? 0x4b6140 : 0x4b6160)(o)); break;
    case 5: r = at<F0>(random() % 2 ? 0x4b5d10 : 0x4b5d30)(o); break;
    case 6: r = tag(at<F0>(0x4b5ce0)(o)) ^ at<F0>(0x4b5cd0)(o); break;
    case 7: r = tag(at<F1>(0x4b60e0)(o, ptrv(g_buf))), st(*(DWORD *)&g_buf[0]); break;
    case 8: {
        // Slot 49 with another polygon (or null)
        FreeRangePoly *src = static_cast<FreeRangePoly *>(at<void *(__cdecl *)()>(0x4b5d50)());
        polygon_text(text, (int)(random() % 9));
        if (CFieldDesc *f = polygon_field())
            vcall<DWORD>(f, 19, src, text, 0);
        r = at<F1>(0x4b5cf0)(o, random() % 5 ? ptrv(&src->polygon) : 0);
        vcall<void *>(src, 1, 1);
        break;
    }
    case 9: {
        // Slots 58/59/29/35 (slot 78 of the real object: the relisting with a null layer)
        const DWORD fl = random_flags();
        const DWORD fn[] = {0x4b6040, 0x4b6070, 0x4b5ff0, 0x4b6020};
        o->flags = random_flags() & ~0x10000u;
        at<F1>(fn[random() % 4])(o, fl);
        mask = 0;
        break;
    }
    case 10: {
        // The dynamic point property (slots 72/73)
        float pt[2] = {(float)x, (float)y};
        if (!*at<void **>(0x6c29b8))
            break;
        at<F1>(0x4fc940)(o, ptrv(pt));
        const float *q = reinterpret_cast<const float *>(at<F0>(0x4fc930)(o));
        st(bits(q[0])), st(bits(q[1]));
        mask = 0;
        break;
    }
    default: at<F5>(0x4b5cc0)(o, 1, 2, 3, 4, 5), r = bits((float)at<FF0>(0x4b5cb0)(o)); break;
    }
    st_object(reinterpret_cast<BYTE *>(o), 0x74, PART_STRINGS, 1, 0x4c);
    vcall<void *>(o, 1, 1);
    return r;
}

DWORD sign_case(DWORD &mask)
{
    SignPart *s = static_cast<SignPart *>(at<void *(__cdecl *)()>(0x58f3a0)());
    know(s, 0x9c);
    constexpr const char *TEXTS[] = {"", "Under Construction", "A", "This bridge is closed until the end of the mission",
                                     "Line one\nLine two"};
    ZString t;
    at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&t, TEXTS[random() % 5]);
    at<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(&s->text, &t);
    at<void(THISCALL *)(ZString *)>(0x4defd0)(&t);
    s->max_width = random() % 4 ? (int)(random() % 400) : (int)random();
    s->x = (float)(random() % 3000);
    s->y = (float)(random() % 3000);
    DWORD r = 0;
    mask = 0;
    switch (random() % 3) {
    case 0: at<F0>(0x58fbb0)(s); break;   // fonts recorded
    case 1: {
        int rect[4];
        r = at<F3>(0x58fab0)(s, ptrv(rect), 0, 0) & 0xff;
        st(rect[0]), st(rect[1]), st(rect[2]), st(rect[3]);
        mask = 0xff;
        break;
    }
    default: st_text(reinterpret_cast<const char *>(at<F0>(0x58f4c0)(s))); break;
    }
    st_object(reinterpret_cast<BYTE *>(s), 0x9c, SIGN_STRINGS, 3, 0);
    vcall<void *>(s, 1, 1);
    return r;
}

// ---- Class getters and RegisterFields ------------------------------------------------------------------

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

struct GetterDef {
    DWORD address, var;
};
constexpr GetterDef GETTERS[] = {{0x5714b0, CLASS_SIMPLE_PART}, {0x571500, CLASS_SIMPLE_PART}, {0x4b5ea0, CLASS_FREE_RANGE_POLY},
                                 {0x58f4e0, CLASS_SIGN_PART},   {0x58f340, CLASS_SIGN_PART}};

bool same_text_buffer(DWORD a, DWORD b)
{
    if (!readable(reinterpret_cast<void *>(a), 12) || !readable(reinterpret_cast<void *>(b), 12))
        return false;
    const char *x = reinterpret_cast<const char *>(a + 8), *y = reinterpret_cast<const char *>(b + 8);
    for (int i = 0; i < 400; i++) {
        if (x[i] != y[i])
            return false;
        if (!x[i])
            return true;
    }
    return false;
}
// Two descriptors compared on the bytes their constructors write: the base part (+0x24 is not written),
// then per kind; raw equal dwords are equal, otherwise ZString buffers by text, the activity array's
// element descriptor by its own comparison. Padding bytes and the heap behind a byte member are skipped.
DWORD g_fake_class_at, g_real_class_at;   // a class pointer of a fake class info that stands for the real one
bool same_dword(const BYTE *a, const BYTE *b, DWORD off)
{
    const DWORD x = *reinterpret_cast<const DWORD *>(a + off), y = *reinterpret_cast<const DWORD *>(b + off);
    return x == y || same_text_buffer(x, y) || (g_fake_class_at && x == g_real_class_at && y == g_fake_class_at);
}
bool same_desc(const BYTE *a, const BYTE *b, int depth = 0)
{
    if (!a || !b)
        return a == b;
    for (DWORD off = 0; off < 0x24; off += 4)
        if (!same_dword(a, b, off))
            return false;
    if (depth < 0)
        return true;   // the base part only (clones made by the check: the copy constructors leave kind bytes)
    auto d = [&](DWORD from, DWORD to) {
        for (DWORD off = from; off < to; off += 4)
            if (!same_dword(a, b, off))
                return false;
        return true;
    };
    auto byte = [&](DWORD off) { return a[off] == b[off]; };
    const DWORD vt = *reinterpret_cast<const DWORD *>(a);
    switch (vt) {
    case 0x5f9314:                                 // the layer part's "Name" (string kind, own vtable)
    case 0x5ec91c: return d(0x2c, 0x34);           // string: +0x28 is not written
    case 0x5ecbf4: return d(0x28, 0x3c);           // object
    case 0x5fd7f4: return byte(0x28) && d(0x2c, 0x34);   // bit: the default byte
    case 0x5ecd6c:                                 // object array: the element descriptor at +0x38
        if (!d(0x28, 0x38) || !d(0x3c, 0x44) || !byte(0x44))
            return false;
        return depth == 0 ? same_desc(*reinterpret_cast<BYTE *const *>(a + 0x38), *reinterpret_cast<BYTE *const *>(b + 0x38), 1)
                          : true;
    case 0x5ec684: return d(0x28, 0x34) && byte(0x34);   // flags
    case 0x5fd518:                                 // enum int: +0x40 the enum names
        return d(0x28, 0x40) && (*reinterpret_cast<const DWORD *>(a + 0x40) != 0) == (*reinterpret_cast<const DWORD *>(b + 0x40) != 0);
    case 0x5fb840: return d(0x28, 0x30);           // object reference
    case 0x5fd2a0: return d(0x28, 0x2c);           // polygon: +0x2c is not written
    case 0x5fdb68: return d(0x28, 0x40);           // float
    case 0x5fd970:                                 // typed resource: +0x2e/+0x2f padding
        return byte(0x28) && byte(0x29) && byte(0x2a) && byte(0x2b) && byte(0x2c) && byte(0x2d) && d(0x30, 0x38);
    }
    return true;   // other kinds (inherited clones such as "Dynamic Properties"): the base part
}
void log_desc(const char *what, const CFieldDesc *f)
{
    if (!f) {
        log("    %s: null", what);
        return;
    }
    const DWORD *d = reinterpret_cast<const DWORD *>(f);
    log("    %s: vt %x name '%s' off %x %x %x | %x %x %x %x %x %x %x %x", what, f->vftable, f->name.buffer ? f->name.c_str() : "?",
        f->offset, f->unknown14, f->unknown18, d[10], d[11], d[12], d[13], d[14], d[15], d[16], d[17]);
}

struct RegFields {
    const char *name;
    DWORD address, var;
    bool inherit;   // the fake class gets clones of the real class's inherited fields
};
constexpr RegFields REGISTER_FIELDS[] = {{"CLayerPart", 0x4f9c50, CLASS_LAYER_PART, false},
                                         {"CSimpleLayerPart", 0x571560, CLASS_SIMPLE_PART, false},
                                         {"CFreeRangePoly", 0x4b5f00, CLASS_FREE_RANGE_POLY, false},
                                         {"CUnderConstructionLayerPart", 0x58f550, CLASS_SIGN_PART, false},
                                         {"CUnderConstructionLayerPart", 0x58f550, CLASS_SIGN_PART, true}};

constexpr int MAXF = 40;
struct Made {
    CFieldDesc *fields[MAXF];
    DWORD n, first, result, fake;
};
void run_register_fields(const RegFields &rf, bool var_zero, Made &out)
{
    CClassInfo *&var = *at<CClassInfo **>(rf.var);
    CClassInfo *const saved = var;
    alignas(4) BYTE fake_class[sizeof(CClassInfo)];
    memset(fake_class, 0, sizeof fake_class);
    auto *fc = reinterpret_cast<CClassInfo *>(fake_class);
    out.first = 0;
    if (rf.inherit && saved) {
        // Clones of the inherited fields in a heap array like ClassInfo_AddField's
        fc->fields = static_cast<CFieldDesc **>(heap_alloc(saved->num_inherited * 4 + 4));
        for (DWORD i = 0; i < saved->num_inherited; i++)
            fc->fields[i] = saved->fields[i]->clone();
        fc->num_fields = fc->num_inherited = saved->num_inherited;
        out.first = saved->num_inherited;
    }
    var = var_zero ? nullptr : fc;
    out.fake = ptrv(fc);
    g_nregister = 0;
    CodePatch p;
    patch_jump(p, 0x415780, (void *)&fake_register);
    out.result = at<DWORD(__cdecl *)()>(rf.address)();
    unpatch(p);
    CClassInfo *c = var;
    out.n = c->num_fields < MAXF ? c->num_fields : MAXF;
    for (DWORD i = 0; i < MAXF; i++)
        out.fields[i] = i < out.n ? c->fields[i] : nullptr;
    if (c->fields)
        heap_free(c->fields);
    if (var_zero)
        pool_free(c, sizeof(CClassInfo));
    var = saved;
}
void free_made(Made &m)
{
    for (DWORD i = 0; i < m.n; i++)
        if (m.fields[i])
            vcall<void *>(m.fields[i], 0, 1);
}

void test_classes(Tally &t, Tally &real)
{
    for (const GetterDef &g : GETTERS) {
        CClassInfo *&var = *at<CClassInfo **>(g.var);
        CClassInfo *const saved = var;
        RegisterCall calls[2];
        DWORD results[2], set[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            set[side] = ptrv(at<CClassInfo *(__cdecl *)()>(g.address)());
            var = nullptr;
            g_nregister = 0;
            memset(&g_register, 0, sizeof g_register);
            CodePatch p;
            patch_jump(p, 0x415780, (void *)&fake_register);
            void *made = at<CClassInfo *(__cdecl *)()>(g.address)();
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
        wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, size %x / %x, version %x / %x, parent %x / %x]", g.address, set[0],
                  set[1], results[0], results[1], calls[0].size, calls[1].size, calls[0].version, calls[1].version,
                  calls[0].parent, calls[1].parent);
        t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                    memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
                "layer parts", d);
    }
    for (const RegFields &rf : REGISTER_FIELDS)
        for (int var_zero = 0; var_zero < 2; var_zero++) {
            if (rf.inherit && var_zero)
                continue;
            Made m[2];
            for (int side = 0; side < 2; side++) {
                if (side == 0)
                    hooks_out();
                run_register_fields(rf, var_zero != 0, m[side]);
                if (side == 0)
                    hooks_in();
            }
            bool same = m[0].n == m[1].n && m[0].n > 0;
            for (DWORD i = 0; i < m[0].n && same; i++)
                same = m[0].fields[i] && m[1].fields[i] &&
                       same_desc(reinterpret_cast<const BYTE *>(m[0].fields[i]), reinterpret_cast<const BYTE *>(m[1].fields[i]), i < m[0].first ? -1 : 0);
            // eax: AddField's (undefined), the polygon's destructor's, or FindField's descriptor (by index)
            char d[160];
            wsprintfA(d, "[RegisterFields %s var %d inherit %d: %u / %u fields, eax %x / %x]", rf.name, var_zero, rf.inherit, m[0].n,
                      m[1].n, m[0].result, m[1].result);
            if (!same && t.mismatches < 3)
                for (DWORD i = 0; i < m[0].n && i < MAXF; i++)
                    if (!m[0].fields[i] || !m[1].fields[i] ||
                        !same_desc(reinterpret_cast<const BYTE *>(m[0].fields[i]), reinterpret_cast<const BYTE *>(m[1].fields[i]))) {
                        log("    field %u differs:", i);
                        log_desc("original", m[0].fields[i]);
                        log_desc("reimpl", m[1].fields[i]);
                        if (m[0].fields[i] && m[1].fields[i])
                            for (DWORD k = 0; k < 9; k++) {
                                const DWORD x = reinterpret_cast<const DWORD *>(m[0].fields[i])[k], y = reinterpret_cast<const DWORD *>(m[1].fields[i])[k];
                                if (x != y)
                                    log("      +0x%x: %x '%s' / %x '%s'", k * 4, x, readable(reinterpret_cast<void *>(x), 12) ? reinterpret_cast<const char *>(x + 8) : "",
                                        y, readable(reinterpret_cast<void *>(y), 12) ? reinterpret_cast<const char *>(y + 8) : "");
                            }
                    }
            t.check(same, rf.name, d);
            // Real data: the class's own descriptors as registered at startup
            const CClassInfo *c = *at<CClassInfo **>(rf.var);
            g_fake_class_at = var_zero ? 0 : m[1].fake;
            g_real_class_at = ptrv(c);
            if (!rf.inherit && c) {
                const DWORD own = c->num_fields - c->num_inherited;
                bool same_real = own == m[1].n;
                for (DWORD i = 0; same_real && i < m[1].n; i++)
                    same_real = same_desc(reinterpret_cast<const BYTE *>(c->fields[c->num_inherited + i]), reinterpret_cast<const BYTE *>(m[1].fields[i]));
                wsprintfA(d, "[registered %s: %u own fields, made %u]", rf.name, own, m[1].n);
                if (!same_real && real.mismatches < 2)
                    for (DWORD i = 0; i < m[1].n && i < MAXF; i++) {
                        log_desc("registered", c->fields[c->num_inherited + i]);
                        log_desc("made", m[1].fields[i]);
                    }
                real.check(same_real, rf.name, d);
            }
            if (rf.inherit && c) {
                // The hidden flags against the registered (inherited) descriptors of the real class
                bool same_real = m[1].n == c->num_fields;
                for (DWORD i = 0; same_real && i < c->num_fields; i++)
                    same_real = m[1].fields[i]->unknown18 == c->fields[i]->unknown18;
                wsprintfA(d, "[registered %s: hidden flags of %u fields]", rf.name, c->num_fields);
                real.check(same_real, rf.name, d);
            }
            g_fake_class_at = 0;
            free_made(m[0]);
            free_made(m[1]);
        }
}

// ---- Real data: the parts of the running level ---------------------------------------------------------


constexpr int MAX_REAL = 400;
LayerPart *g_real[MAX_REAL];
int g_nreal;
bool find_real_parts()
{
    g_nreal = 0;
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (!world)
        return false;
    BYTE *layer = real_player_layer();
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
        for (DWORD k = 0; k < c->count; k++) {
            int j = 0;
            while (j < g_nreal && g_real[j] != c->data[k])
                j++;
            if (j == g_nreal && g_nreal < MAX_REAL)
                g_real[g_nreal++] = static_cast<LayerPart *>(c->data[k]);
        }
    }
    return true;
}

// The module's functions among a vtable's read-only slots
constexpr DWORD READ_ONLY[] = {0x4fe760, 0x487220, 0x487230, 0x4fe780, 0x4fe7a0, 0x4b6100, 0x4b6140, 0x4b6160, 0x4b5d10, 0x4b5d30,
                               0x4b5ce0, 0x4b5cd0, 0x4b60e0, 0x487210, 0x4b5cb0, 0x58f4c0, 0x4fc260, 0x4faf30, 0x4fafa0, 0x4b61e0,
                               0x4b6180, 0x58fab0, 0x58fb60};
bool is_read_only(DWORD f)
{
    for (DWORD r : READ_ONLY)
        if (r == f)
            return true;
    return false;
}

void test_real(Tally &t)
{
    if (!find_real_parts()) {
        log("  layer parts real data: no level grid found");
        t.check(false, "layer parts", "[no level grid]");
        return;
    }
    int calls = 0;
    for (int i = 0; i < g_nreal; i++) {
        LayerPart *p = g_real[i];
        const DWORD *vt = *reinterpret_cast<DWORD *const *>(p);
        for (int s = 36; s < 62; s++) {
            const DWORD f = vt[s];
            if (!is_read_only(f))
                continue;
            for (int rep = 0; rep < (s == 37 || s == 38 ? 12 : 1); rep++) {
                calls++;
                two_sides(t, "real part", (unsigned)(i * 1000 + s * 20 + rep), [&](DWORD &mask) -> DWORD {
                    mask = 0xffffffff;
                    float out[4] = {0, 0, 0, 0};
                    switch (s) {
                    case 37: {
                        int rect[4] = {0, 0, 0, 0}, off[2] = {(int)(random() % 50), -(int)(random() % 50)};
                        const DWORD kind = (DWORD)rep % 7;
                        const DWORD r = at<F3>(f)(p, ptrv(rect), kind, rep & 1 ? ptrv(off) : 0);
                        st(rect[0]), st(rect[1]), st(rect[2]), st(rect[3]);
                        mask = f == 0x4b61e0 && kind > 5 ? 0xffffffff : 0xff;   // kind 4: the rectangle pointer's upper bytes
                        return r;
                    }
                    case 38: {
                        int rect[4] = {0, 0, 0, 0};
                        vcall<DWORD>(p, 37, rect, 0, 0);
                        const int w = rect[2] - rect[0] + 40, h = rect[3] - rect[1] + 40;
                        const int x = rect[0] - 20 + (int)(random() % (DWORD)(w > 0 && w < 100000 ? w : 100));
                        const int y = rect[1] - 20 + (int)(random() % (DWORD)(h > 0 && h < 100000 ? h : 100));
                        mask = 0xff;   // upper bytes: fnstsw's or the edge test's
                        return at<F3>(f)(p, (DWORD)x, (DWORD)y, 0);
                    }
                    case 44: case 45: case 46: case 55: return bits((float)at<FF0>(f)(p));
                    case 52: case 53: return at<F0>(f)(p);
                    case 61: return text_hash(reinterpret_cast<const char *>(at<F0>(f)(p)));
                    case 48: return at<F0>(f)(p) - ptrv(p);
                    case 36: return at<F0>(f)(p);
                    default:
                        at<F1>(f)(p, ptrv(out));
                        st(bits(out[0])), st(bits(out[1]));
                        mask = 0;
                        return 0;
                    }
                });
            }
        }
        two_sides(t, "real target", (unsigned)i, [&](DWORD &) { return at<F0>(0x4fc260)(p); });
    }
    log("  layer parts real data: %d parts, %d slot calls", g_nreal, calls);
}

}  // namespace

// Parts in the running level's layer grid, -1 without a level (checks.h: the checks wait for it)
int level_part_count()
{
    return find_real_parts() ? g_nreal : -1;
}

ZAX_CHECKS(layer_parts)
{
    (void)cls;
    (void)n;
    init_tables();
    // Warm-up: the message id of "Query AI Type" and the sign's font are created/loaded once outside the
    // compared runs
    at<DWORD(__fastcall *)(const char *)>(0x506680)("Query AI Type");
    if (void *s = at<void *(__cdecl *)()>(0x58f3a0)())
        vcall<void *>(s, 1, 1);
    Tally slots{"layer parts: base slots on fake parts"}, objects{"layer parts: constructors, Create, destructors"},
        polys{"layer parts: free range polygons"}, signs{"layer parts: signs"},
        classes{"layer parts: class getters and RegisterFields"}, registered{"layer parts: registered descriptors (real data)"},
        real{"layer parts: parts of the running level (real data)"};
    log("  layer parts: slots");
    g_patches.on();
    for (unsigned seed = 1; seed <= (unsigned)scaled(2000); seed++)
        two_sides(slots, "slot", seed, [](DWORD &mask) { return slot_case(mask); });
    g_patches.off();
    slots.report();
    log("  layer parts: objects");
    for (unsigned seed = 1; seed <= (unsigned)scaled(200); seed++)
        two_sides(objects, "ctor", seed, [](DWORD &mask) { return ctor_case(mask); });
    log("  layer parts: create");
    for (unsigned seed = 1; seed <= (unsigned)scaled(200); seed++)
        two_sides(objects, "create", seed, [](DWORD &mask) { return create_case(mask); });
    log("  layer parts: destructor");
    g_patches.on();
    for (unsigned seed = 1; seed <= (unsigned)scaled(200); seed++)
        two_sides(objects, "dtor", seed, [](DWORD &mask) { return dtor_case(mask); });
    g_patches.off();
    objects.report();
    log("  layer parts: polygons");
    for (unsigned seed = 1; seed <= (unsigned)scaled(600); seed++)
        two_sides(polys, "polygon", seed, [](DWORD &mask) { return poly_case(mask); });
    polys.report();
    log("  layer parts: signs");
    g_patches.on();
    for (unsigned seed = 1; seed <= (unsigned)scaled(200); seed++)
        two_sides(signs, "sign", seed, [](DWORD &mask) { return sign_case(mask); });
    g_patches.off();
    signs.report();
    log("  layer parts: classes");
    test_classes(classes, registered);
    classes.report();
    registered.report();
    log("  layer parts: real data");
    test_real(real);
    real.report();
}

}  // namespace zax
