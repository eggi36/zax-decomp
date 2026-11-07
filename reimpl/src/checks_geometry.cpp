// Shared parts of the polygon and layer part helper checks, see checks_geometry.h.
#include "checks_geometry.h"
#include "file_buffer.h"
#include "x87.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);

namespace zax {

int hooks_polygon(Hook *out);
int hooks_layer_part_helpers(Hook *out);

namespace geo {

// ---- Sides, log and state --------------------------------------------------------------------------------

int g_side;
bool g_trace = false;
unsigned g_loghash[2], g_statehash[2];
int g_nlog[2];
constexpr int LOG = 256;
DWORD g_log[2][LOG][4];
DWORD g_stack_top;
DWORD g_calls;

void ev(DWORD a, DWORD b, DWORD c, DWORD d)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    if (g_nlog[g_side] < LOG)
        memcpy(g_log[g_side][g_nlog[g_side]], e, sizeof e);
    g_nlog[g_side]++;
}
static DWORD g_dump[2][96];
static int g_ndump[2];
void dump(DWORD v)
{
    if (g_ndump[g_side] < 96)
        g_dump[g_side][g_ndump[g_side]++] = v;
}
void st(DWORD v) { g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4); }
void st_bytes(const void *p, DWORD n) { g_statehash[g_side] = fnv(g_statehash[g_side], p, n); }
void st_float(float f) { st(bits(f)); }
void st_ld(long double v)
{
    BYTE b[10];
    memcpy(b, &v, 10);
    st_bytes(b, 10);
}
void st_poly(const Poly2D *p)
{
    st(p->count);
    st((DWORD)p->left), st((DWORD)p->top), st((DWORD)p->right), st((DWORD)p->bottom);
    st(p->flags), st_float(p->cx), st_float(p->cy);
    st(p->points ? 1 : 0), st(p->edges ? 1 : 0);
    if (p->points && p->count && p->count < 64)
        st_bytes(p->points, p->count * 8);
    if (p->edges && p->count > 1 && p->count < 64)
        st_bytes(p->edges, (p->count - 1) * 12);
}

static void log_first_difference()
{
    for (int i = 0; i < g_nlog[0] && i < g_nlog[1] && i < LOG; i++)
        if (memcmp(g_log[0][i], g_log[1][i], 16)) {
            log("    event %d: %x %x %x %x / %x %x %x %x", i, g_log[0][i][0], g_log[0][i][1], g_log[0][i][2],
                g_log[0][i][3], g_log[1][i][0], g_log[1][i][1], g_log[1][i][2], g_log[1][i][3]);
            return;
        }
    if (g_nlog[0] != g_nlog[1])
        log("    log lengths %d / %d", g_nlog[0], g_nlog[1]);
}

// ---- Hooks of both modules out and in ------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 128;
static SavedSite g_sites[MAX_SITES];
static int g_nsites;

static void take_out(int (*table)(Hook *))
{
    Hook list[MAX_SITES];
    const int n = table(nullptr);
    if (!hook_sites_fit("geometry", n, MAX_SITES))
        return;
    table(list);
    for (int i = 0; i < n && g_nsites < MAX_SITES; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (site[0] != 0xe9)
            continue;   // not installed (Disable=, or bytes did not match)
        const DWORD target = list[i].address + 5 + *reinterpret_cast<INT32 *>(site + 1);
        if (target != ptrv(list[i].replacement))
            continue;
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, list[i].expected);
    }
}
void hooks_out()
{
    g_nsites = 0;
    take_out(&hooks_polygon);
    take_out(&hooks_layer_part_helpers);
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

void side_begin(int side, unsigned seed)
{
    g_side = side;
    g_seed = seed;
    g_calls = 0;
    g_loghash[side] = g_statehash[side] = 2166136261u;
    g_nlog[side] = 0;
    g_ndump[side] = 0;
    DWORD top = 0;
    g_stack_top = ptrv(&top);
    if (side == 0)
        hooks_out();
}
void side_end(int side)
{
    if (side == 0)
        hooks_in();
}
bool compare_sides(Tally &t, const char *what, unsigned seed, DWORD r0, DWORD r1, DWORD mask)
{
    char d[200];
    wsprintfA(d, "[%s seed %u: eax %x / %x (mask %x), log %d %x / %d %x, state %x / %x]", what, seed, r0, r1, mask,
              g_nlog[0], g_loghash[0], g_nlog[1], g_loghash[1], g_statehash[0], g_statehash[1]);
    const bool same = ((r0 ^ r1) & mask) == 0 && g_loghash[0] == g_loghash[1] && g_nlog[0] == g_nlog[1] &&
                      g_statehash[0] == g_statehash[1];
    if (!same && t.mismatches < 10) {
        log_first_difference();
        for (int i = 0; i < g_ndump[0] && i < g_ndump[1]; i += 6) {
            char line[200];
            int n = 0;
            for (int k = i; k < i + 6 && k < g_ndump[0]; k++)
                n += wsprintfA(line + n, " %x/%x", g_dump[0][k], g_dump[1][k]);
            log("    dump%s", line);
        }
    }
    t.check(same, "geometry", d);
    return same;
}

// ---- Random values ------------------------------------------------------------------------------------------

float random_coord()
{
    switch (random() % 8) {
    case 0: {
        constexpr DWORD V[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x3f000000, 0x00000001, 0x80000001, 0x00800000,
                               0x4b000000, 0xcb000000, 0x4b7fffff, 0x58635fa9, 0xd8635fa9, 0x3effffff, 0x3f7fffff};
        return raw_float(V[random() % (sizeof V / 4)]);
    }
    case 1: return (float)((int)(random() % 2000001) - 1000000) / 64.0f;
    case 2: return (float)((int)(random() % 20001) - 10000) + (float)(random() % 1000) / 999.0f;
    default: return (float)((int)(random() % 4001) - 2000) / ((random() & 1) ? 1.0f : 4.0f);
    }
}
float random_small() { return (float)((int)(random() % 1001) - 500) + (float)(random() % 100) / 100.0f; }
DWORD random_mask()
{
    DWORD m = 0;
    for (int i = 0; i < 3; i++)
        m |= 1u << (16 + random() % 12);
    return random() % 4 ? m : 0;
}

// ---- Fakes and the recorder thunks ----------------------------------------------------------------------------

FakePart g_parts[NPART];
FakeAI g_ais[NAI];
FakeList g_lists[NLIST];
FakeSweep g_sweeps[NSWEEP];
BYTE g_layer[0x100];
DWORD g_vt[5][96];
LayerPart *g_query[16];
int g_nquery;
DynEntry g_dyn[16];
int g_ndyn;
int g_read_bool, g_read_words[2], g_nread_words;
void *g_lookup_result;
static BYTE g_packet[16];
static struct {
    DWORD vftable;
    LayerPart **data;
    DWORD count, capacity;
    LayerPart *items[16];
} g_temp;
static bool g_names_live;

extern "C" {
extern char geo_thunks[];
DWORD geo_pop, geo_ret_float, geo_fret;
DWORD __cdecl geo_record(DWORD *args, DWORD id, BYTE *self);
}
__asm__(".text\n"
        ".globl _geo_thunks\n"
        "_geo_thunks:\n"
        ".set geo_i, 0\n"
        ".rept 480\n"
        "  movl $geo_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _geo_common - . - 4\n"
        ".set geo_i, geo_i + 1\n"
        ".endr\n"
        "_geo_common:\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _geo_record\n"
        "  addl $12, %esp\n"
        "  cmpl $0, _geo_ret_float\n"
        "  je 1f\n"
        "  flds _geo_fret\n"
        "1:\n"
        "  movl _geo_pop, %ecx\n"
        "  popl %edx\n"
        "  addl %ecx, %esp\n"
        "  jmp *%edx\n");

static const Poly2D *g_known_polys[32];
static int g_nknown_polys;

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
    for (int i = 0; i < NSWEEP; i++)
        if (in(&g_sweeps[i], sizeof(FakeSweep)))
            return 0xf5000000u + ((DWORD)i << 12) + (v - ptrv(&g_sweeps[i]));
    if (in(g_layer, sizeof g_layer))
        return 0xf3000000u + (v - ptrv(g_layer));
    if (in(&g_temp, sizeof g_temp))
        return 0xf6000000u + (v - ptrv(&g_temp));
    if (in(g_packet, sizeof g_packet))
        return 0xf7000000u;
    for (int i = 0; i < g_nknown_polys; i++)
        if (in(g_known_polys[i], sizeof(Poly2D)))
            return 0xe0000000u + ((DWORD)i << 8) + (v - ptrv(g_known_polys[i]));
    if (v <= g_stack_top && v + 0x10000 > g_stack_top)
        return 0xf4000000u;
    return v;
}

static DWORD rec_hash(DWORD a, DWORD b, DWORD c)
{
    DWORD h = fnv(fnv(fnv(2166136261u, &a, 4), &b, 4), &c, 4);
    return h ^ (h >> 13);
}

static FakePart *fake_part(const void *p)
{
    for (int i = 0; i < NPART; i++)
        if (p == &g_parts[i])
            return &g_parts[i];
    return nullptr;
}

static CClassInfo *g_classes[3];

extern "C" DWORD __cdecl geo_record(DWORD *args, DWORD id, BYTE *self)
{
    const DWORD table = id / 96, slot = id % 96;
    geo_ret_float = 0;
    geo_pop = 0;
    g_calls++;
    const DWORD me = tag(ptrv(self));
    const DWORD r = rec_hash(id, me, g_calls);
    const DWORD code = 0x47000000u + id;
    if (table == T_PART) {
        FakePart *f = fake_part(self);
        if (!f) {
            ev('?', id, me);
            return 0;
        }
        switch (slot) {
        case 1:
            geo_pop = 4;
            f->deleted++;
            ev(code, me, args[0]);
            return ptrv(self);
        case 2: return ptrv(g_classes[f->cls % 3]);
        case 37: {
            geo_pop = 12;
            int *rect = reinterpret_cast<int *>(args[0]);
            for (int k = 0; k < 4; k++)
                rect[k] = (int)(rec_hash(r, k, 1) % 2000) - 1000 + (k >= 2 ? 1000 : 0);
            ev(code, me, args[1], tag(args[2]));
            return (r & ~0xffu) | f->r37;
        }
        case 40:
        case 43:
        case 50: {
            geo_pop = 4;
            float *out = reinterpret_cast<float *>(args[0]);
            const float *v = slot == 40 ? f->s40 : slot == 43 ? f->pos : f->off;
            out[0] = v[0];
            out[1] = v[1];
            ev(code, me, tag(args[0]));
            return r;
        }
        case 41:
            geo_pop = 8;
            ev(code, me, args[0], args[1]);
            return r;
        case 46:
            geo_ret_float = 1;
            geo_fret = bits(f->s46);
            ev(code, me);
            return r;
        case 48: ev(code, me); return ptrv(f->poly);
        case 52: ev(code, me); return (DWORD)f->i52;
        case 53: ev(code, me); return (DWORD)f->i53;
        case 54: ev(code, me); return (DWORD)f->i54;
        case 58:
            geo_pop = 4;
            ev(code, me, args[0]);
            return r;
        case 65:
        case 66:
            geo_pop = 8;
            ev(code, me, tag(args[0]), tag(args[1]));
            return (r & ~0xffu) | (slot == 65 ? f->r65 : f->r66);
        case 67: {
            geo_pop = 16;
            const float *h = reinterpret_cast<const float *>(args[1]);
            const float *n = reinterpret_cast<const float *>(args[2]);
            const DWORD v[4] = {bits(h[0]), bits(h[1]), bits(n[0]), bits(n[1])};
            ev(code, me, tag(args[0]), fnv(args[3], v, 16));
            return r;
        }
        }
    } else if (table == T_AI) {
        FakeAI *a = reinterpret_cast<FakeAI *>(self);
        switch (slot) {
        case 1:
            geo_pop = 4;
            a->deleted++;
            ev(code, me, args[0]);
            return ptrv(self);
        case 2: return ptrv(g_classes[0]);
        case 23:
        case 24:
            geo_pop = 4;
            ev(code, me, tag(args[0]));
            return r;
        case 35:
            geo_pop = 8;
            ev(code, me, tag(args[0]), args[1]);
            return r;
        }
    } else if (table == T_ARRAY && slot == 2) {
        geo_pop = 4;
        LPArray *a = reinterpret_cast<LPArray *>(self);
        void *item = *reinterpret_cast<void **>(args[0]);
        if (a->count < a->capacity)
            a->data[a->count++] = item;
        ev(code, me, tag(ptrv(item)));
        return a->count;
    } else if (table == T_SWEEP && slot == 0) {
        geo_pop = 8;
        ev(code, me, tag(args[0]), tag(args[1]));
        return (r & ~0xffu) | reinterpret_cast<FakeSweep *>(self)->r0;
    }
    log("  geometry: unexpected recorder call table %u slot %u", table, slot);
    ev('?', id, me);
    return 0;
}

// ---- Stand-ins ----------------------------------------------------------------------------------------------

static DWORD THISCALL rec_find_rect(void *grid, void *list, const int *rect, int kind, DWORD all, DWORD none, DWORD any)
{
    ev(0x4652, tag(ptrv(grid)), tag(ptrv(list)), fnv(2166136261u, rect, 16));
    ev(0x465232, (DWORD)kind, all, none ^ (any << 1));
    if (list == &g_temp) {
        g_temp.count = 0;
        for (int i = 0; i < g_nquery && i < 16; i++)
            g_temp.items[g_temp.count++] = g_query[i];
    }
    return g_temp.count;
}
static void *THISCALL rec_get_temp(void **slot)
{
    g_temp.vftable = 0;
    g_temp.data = g_temp.items;
    g_temp.count = 0;
    g_temp.capacity = 16;
    *slot = &g_temp;
    ev(0x4754);
    return &g_temp;
}
static void *THISCALL rec_release_temp(void **slot)
{
    ev(0x5254, tag(ptrv(*slot)));
    return *slot;
}
static DWORD THISCALL rec_layer_query(void *layer, void *list, const int *rect, DWORD a, DWORD b, DWORD c)
{
    ev(0x4c51, tag(ptrv(layer)), tag(ptrv(list)), fnv(2166136261u, rect, 16));
    ev(0x4c5132, a, b, c);
    return rec_hash(0x4c51, a, g_calls);
}
static DWORD THISCALL rec_layer_oval(void *layer, void *list, const float *center, DWORD r, DWORD ratio, DWORD a, DWORD b,
                                     DWORD c)
{
    ev(0x4c4f, tag(ptrv(layer)), tag(ptrv(list)), fnv(2166136261u, center, 8));
    ev(0x4c4f32, r, ratio, fnv(fnv(fnv(2166136261u, &a, 4), &b, 4), &c, 4));
    return rec_hash(0x4c4f, a, g_calls);
}
static DWORD THISCALL rec_layer_add(void *layer, void *what, void *p)
{
    ev(0x4c41, tag(ptrv(layer)), tag(ptrv(what)), tag(ptrv(p)));
    return rec_hash(0x4c41, 0, g_calls);
}
static void *THISCALL rec_lookup(void *list, const char *name, int k)
{
    ev(0x4e4c, ptrv(list), fnv_text(2166136261u, name), (DWORD)k);
    return g_lookup_result;
}
static DynEntry *dyn_find(const void *desc, const void *obj)
{
    for (int i = 0; i < g_ndyn; i++)
        if (g_dyn[i].desc == desc && g_dyn[i].obj == obj)
            return &g_dyn[i];
    return nullptr;
}
static DWORD THISCALL rec_dyn_is_in(void *desc, const void *obj)
{
    const bool in = dyn_find(desc, obj) != nullptr;
    ev(0x4449, ptrv(desc), tag(ptrv(obj)), in);
    return in;
}
static void *THISCALL rec_dyn_take(void *desc, const void *obj)
{
    DynEntry *e = dyn_find(desc, obj);
    const void *v = e ? e->value : nullptr;
    if (e)
        e->value = nullptr;
    ev(0x4454, ptrv(desc), tag(ptrv(obj)), tag(ptrv(v)));
    return const_cast<void *>(v);
}
static DWORD THISCALL rec_dyn_remove(void *desc, const void *obj)
{
    DynEntry *e = dyn_find(desc, obj);
    if (e)
        *e = g_dyn[--g_ndyn];
    ev(0x4452, ptrv(desc), tag(ptrv(obj)), e != nullptr);
    return e != nullptr;
}
static DWORD THISCALL rec_broadcast(void *bc, void *sender, const char *message)
{
    ev(0x4243, tag(ptrv(bc)), tag(ptrv(sender)), fnv_text(2166136261u, message));
    return 0;
}
static DWORD THISCALL rec_read_bool(void *packet, BYTE *out)
{
    ev(0x5242, tag(ptrv(packet)), (DWORD)g_read_bool);
    if (g_read_bool < 0)
        return 0;
    *out = (BYTE)g_read_bool;
    return 1;
}
static DWORD THISCALL rec_write_bool(void *packet, DWORD v)
{
    ev(0x5742, tag(ptrv(packet)), v & 0xff);
    return 1;
}
static DWORD THISCALL rec_read_word(void *packet, WORD *out)
{
    const int v = g_nread_words < 2 ? g_read_words[g_nread_words++] : -1;
    ev(0x5257, tag(ptrv(packet)), (DWORD)v);
    if (v < 0)
        return 0;
    *out = (WORD)v;
    return 1;
}
static DWORD THISCALL rec_write_word(void *packet, DWORD v)
{
    ev(0x5757, tag(ptrv(packet)), v & 0xffff);
    return 1;
}
// FileBuf_Read 0x4a1b00 on a memory block (the fake file is g_file itself)
BYTE g_file[256];
DWORD g_file_size, g_file_pos;
static DWORD THISCALL rec_file_read(void *file, void *dst, DWORD n)
{
    if (file != g_file)   // another thread (the music stream) reads a real file: the original code
        return static_cast<CFileBuf *>(file)->Read(dst, n);
    DWORD k = 0;
    for (; k < n && g_file_pos < g_file_size; k++)
        static_cast<BYTE *>(dst)[k] = g_file[g_file_pos++];
    ev(0x4642, n, k);
    return k;
}

BYTE g_filter_result;
BYTE __fastcall rec_filter(void *mover, void *part)
{
    ev(0x46490000, tag(ptrv(mover)), tag(ptrv(part)));
    return g_filter_result;
}

void Patches::on()
{
    n = 0;
    patch_jump(p[n++], 0x57a770, (void *)&rec_find_rect);
    patch_jump(p[n++], 0x59a470, (void *)&rec_get_temp);
    patch_jump(p[n++], 0x59a510, (void *)&rec_release_temp);
    patch_jump(p[n++], 0x4ed610, (void *)&rec_layer_query);
    patch_jump(p[n++], 0x4ed690, (void *)&rec_layer_oval);
    patch_jump(p[n++], 0x4ea880, (void *)&rec_layer_add);
    patch_jump(p[n++], 0x57e690, (void *)&rec_lookup);
    patch_jump(p[n++], 0x418fc0, (void *)&rec_dyn_is_in);
    patch_jump(p[n++], 0x418810, (void *)&rec_dyn_take);
    patch_jump(p[n++], 0x418fb0, (void *)&rec_dyn_remove);
    patch_jump(p[n++], 0x467d40, (void *)&rec_broadcast);
    patch_jump(p[n++], 0x520580, (void *)&rec_read_bool);
    patch_jump(p[n++], 0x520a50, (void *)&rec_write_bool);
    patch_jump(p[n++], 0x520510, (void *)&rec_read_word);
    patch_jump(p[n++], 0x5209c0, (void *)&rec_write_word);
    patch_jump(p[n++], 0x4a1b00, (void *)&rec_file_read);
}
void Patches::off()
{
    while (n > 0)
        unpatch(p[--n]);
}
Patches g_patches;

// ---- Setting up the fakes --------------------------------------------------------------------------------------

void init_fakes()
{
    for (int t = 0; t < 5; t++)
        for (int s = 0; s < 96; s++)
            g_vt[t][s] = ptrv(geo_thunks) + 10u * (t * 96 + s);
    for (DWORD k = 0; k < 480; k++) {
        const BYTE *th = reinterpret_cast<const BYTE *>(geo_thunks) + 10 * k;
        if (th[0] != 0xb8 || *reinterpret_cast<const DWORD *>(th + 1) != k || th[5] != 0xe9) {
            log("  geometry: recorder thunk %u is broken", k);
            break;
        }
    }
    g_classes[0] = at<CClassInfo *(__cdecl *)()>(0x4f9bf0)();   // CLayerPart
    g_classes[1] = at<CClassInfo *(__cdecl *)()>(0x48dcc0)();   // CEntityAnimated
    g_classes[2] = at<CClassInfo *(__cdecl *)()>(0x487620)();
}

LayerPart *part(int i) { return &g_parts[i].p; }

static void names_off()
{
    if (!g_names_live)
        return;
    for (int i = 0; i < NPART; i++)
        at<void(THISCALL *)(ZString *)>(0x4defd0)(&g_parts[i].p.name);
    g_names_live = false;
}

void setup_fakes(const Poly2D *const *polys, int npolys)
{
    names_off();
    memset(g_parts, 0, sizeof g_parts);
    memset(g_ais, 0, sizeof g_ais);
    memset(g_lists, 0, sizeof g_lists);
    memset(g_layer, 0, sizeof g_layer);
    g_nknown_polys = 0;
    for (int i = 0; i < npolys && g_nknown_polys < 32; i++)
        g_known_polys[g_nknown_polys++] = polys[i];
    for (int i = 0; i < NAI; i++) {
        g_ais[i].vftable = ptrv(g_vt[T_AI]);
        g_ais[i].done = random() % 3 == 0;
    }
    int next_list = 0;
    for (int i = 0; i < NPART; i++) {
        FakePart &f = g_parts[i];
        LayerPart &p = f.p;
        p.vftable = ptrv(g_vt[T_PART]);
        at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&p.name);
        p.flags = random() % 3 ? (random() & 0x0fff0000) | 0x20000 : random() & 0x0fff0000;
        p.team = random() % 4 - 1;
        p.id = (WORD)random();
        p.saved_flags = (WORD)random();
        p.layer = g_layer;
        p.parent = i > 0 && random() % 3 == 0 ? &g_parts[random() % i].p : nullptr;
        p.activity.vftable = ptrv(g_vt[T_ARRAY]);
        p.activity.data = f.items;
        p.activity.capacity = 16;
        p.activity.count = random() % 6;
        for (DWORD k = 0; k < p.activity.count; k++)
            f.items[k] = random() % 5 ? &g_ais[random() % NAI] : nullptr;
        p.broadcaster = random() % 3 == 0 ? g_layer + 0x80 : nullptr;
        f.x = random_small();
        f.y = random_small();
        f.facing = random_small() / 100.0f;
        f.cls = random() % 3;
        f.poly = npolys && random() % 4 ? polys[random() % npolys] : nullptr;
        f.off[0] = random_small(), f.off[1] = random_small();
        f.pos[0] = random_small(), f.pos[1] = random_small();
        f.s40[0] = random_small(), f.s40[1] = random_small();
        f.s46 = random_small() / 100.0f;
        f.i52 = (int)(random() % 2001) - 1000, f.i53 = (int)(random() % 2001) - 1000, f.i54 = (int)(random() % 200);
        constexpr BYTE B[] = {0, 1, 2, 0x80, 0xff};
        f.r37 = B[random() % 5], f.r65 = B[random() % 5], f.r66 = B[random() % 5];
    }
    g_names_live = true;
    // Child lists only towards higher numbers, so the trees have no cycles
    for (int i = 0; i < NPART && next_list < NLIST; i++) {
        if (random() % 2)
            continue;
        FakeList &l = g_lists[next_list++];
        l.l.vftable = 0;
        l.l.array_vftable = ptrv(g_vt[T_ARRAY]);
        l.l.data = l.items;
        l.l.capacity = 12;
        const int n = random() % 4;
        for (int k = 0; k < n; k++) {
            const int c = i + 1 + (int)(random() % (DWORD)(NPART - i > 1 ? NPART - i - 1 : 1));
            l.items[l.l.count++] = c < NPART && random() % 6 ? &g_parts[c].p : nullptr;
        }
        g_parts[i].p.children = &l.l;
    }
    for (int i = 0; i < NSWEEP; i++) {
        FakeSweep &s = g_sweeps[i];
        memset(&s, 0, sizeof s);
        s.s.vftable = ptrv(g_vt[T_SWEEP]);
        s.s.kind = random() % 4;
        s.s.notify = random() % 3 ? 1 : 0;
        s.r0 = random() % 4 ? 1 : 0;
    }
    g_nquery = 0;
    g_ndyn = 0;
    g_read_bool = -1;
    g_nread_words = 2;
    g_lookup_result = nullptr;
}

// ---- Memory helpers --------------------------------------------------------------------------------------------

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

bool heap_pointer(const void *p) { return ptrv(p) >= 0x10000 && !(ptrv(p) & 3) && (ptrv(p) < 0x400000 || ptrv(p) >= IMAGE_END); }

void build_poly(Poly2D *p, const float *xy, DWORD count)
{
    memset(p, 0, sizeof *p);
    float *buf = at<float *>(POINT_BUFFER);
    for (DWORD i = 0; i < count * 2; i++)
        buf[i] = xy[i];
    hooks_out();
    at<DWORD(THISCALL *)(Poly2D *, DWORD)>(0x546580)(p, count);
    hooks_in();
}

void free_poly(Poly2D *p)
{
    if (p->points)
        heap_free(p->points);
    if (p->edges)
        heap_free(p->edges);
    memset(p, 0, sizeof *p);
}

void pad_edges(Poly2D *p)
{
    if (!p->edges || p->count < 2)
        return;
    PolyEdge *e = static_cast<PolyEdge *>(heap_alloc(p->count * 12));
    memcpy(e, p->edges, (p->count - 1) * 12);
    e[p->count - 1].nx = raw_float(0x3f000000);
    e[p->count - 1].ny = raw_float(0xbf000000);
    e[p->count - 1].length = raw_float(0x40000000);
    heap_free(p->edges);
    p->edges = e;
}

}  // namespace geo
}  // namespace zax
