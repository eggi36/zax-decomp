// Checks of layer.cpp (CLayerSaveData, CZaxLayerSaveData, CLayer, loading and saving layers and the .way,
// the layer's part bookkeeping, CLayerHolder) against the original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out, side 1
// through the reimplementation. Compared: eax (masked where the original's is not defined), an event log
// of the fakes and the state afterwards. Objects are compared dword by dword: equal raw values are equal,
// pointers into the object by their offset, other heap pointers by the first dword of what they point to.
//  - Objects: Create, CreateArray 0-3, the constructors on pool blocks filled with a pattern, the deleting
//    destructors (single and array, freeing or not) of CZaxLayerSaveData (and CLayerCustomSaveData's),
//    CLayerHolder (with fake layers), CLayerSaveData and CLayer (with and without a frame).
//  - Classes: the 6 class getters (ClassInfo_Register replaced), RegisterFields of the four classes into
//    fake class infos, compared per field kind, and against the descriptors registered at startup.
//  - Small functions: the rectangle helpers, the dot product, the pointer array's RemoveAndDelete and the
//    16-byte array's Reserve with fake entries, the view rectangles on fake frames (slots 39/45
//    recorded), the draw byte, UniqueId (generator state restored), AddPlayingTime with random doubles.
//  - Save data methods on fresh layers: SetCustom, the objects made on first use, the music mix, the fog,
//    SetShake, the message handler, the file name, the listener environment (the running level's values
//    only). Precache runs on the running level (on a fresh layer it ends the game in the original too).
//  - Parts in fresh layers: CFreeRangePoly parts with random polygons moved between two layers (AddPart),
//    watched by the monitor list, removed, deleted, queried (FindRect, AnyAccepts, QueryRadius, MarkDied).
//  - Real data: the running level's layer written as text into a file and binary into a file; the
//    level file read into new layers (LoadText) and those written again; a layer read back binary
//    (LoadBinary; such a layer is left undeleted: alone it keeps references only a whole save game
//    resolves, deleting its parts ends the game in the original too); the world's layer holder written
//    as text and binary and read back into new holders;
//    the .way of a loaded layer written into the temp folder (SaveWay), loaded, calculated when missing
//    and recalculated when unreadable (LoadOrCalculateWay), each file compared byte for byte (texts of
//    different objects with the object ids numbered by first appearance).
#include "check_util.h"
#include "layer.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT BOOL WINAPI ReadFile(HANDLE f, void *buf, DWORD n, DWORD *read, void *overlapped);
WINIMPORT DWORD WINAPI GetFileSize(HANDLE f, DWORD *high);
WINIMPORT DWORD WINAPI GetFullPathNameA(const char *name, DWORD n, char *out, char **file);
WINIMPORT DWORD WINAPI GetTickCount(void);

namespace zax {

int hooks_layer(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

int g_side;
unsigned g_loghash[2], g_statehash[2];
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
}
constexpr int SLOG = 2048;
DWORD g_slog[2][SLOG];
int g_nslog[2];
bool g_trace;
void st(DWORD v)
{
    g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4);
    if (g_nslog[g_side] < SLOG)
        g_slog[g_side][g_nslog[g_side]] = v;
    g_nslog[g_side]++;
}
void log_first_state_difference()
{
    for (int i = 0; i < g_nslog[0] && i < g_nslog[1] && i < SLOG; i++)
        if (g_slog[0][i] != g_slog[1][i]) {
            log("    state %d: %x / %x (of %d / %d)", i, g_slog[0][i], g_slog[1][i], g_nslog[0], g_nslog[1]);
            return;
        }
}

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
    const int n = hooks_layer(nullptr);
    if (!hook_sites_fit("layer", n, MAX_SITES))
        return;
    hooks_layer(list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (original_address(list[i].address) == site)
            continue;
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
bool heap_pointer(DWORD p) { return p >= 0x10000 && !(p & 3) && (p < 0x400000 || p >= IMAGE_END) && p < 0x80000000u; }

// Fake objects: slot 1 records its deletion, 2 a class, 39/45 sizes from the seed, 63 records
constexpr int NFAKE = 24;
struct FakeObj {
    DWORD vftable;
    DWORD dynamic;
    DWORD id;
    DWORD calls;
};
FakeObj g_fakes[NFAKE];
DWORD g_fake_vt[100];
FakeObj *THISCALL fake_delete(FakeObj *f, DWORD flags)
{
    ev('D', f->id, flags);
    return f;
}
CClassInfo *THISCALL fake_class(FakeObj *) { return at<CClassInfo *(__cdecl *)()>(0x4163b0)(); }
DWORD fake_size(FakeObj *f, DWORD slot)
{
    const DWORD c = f->calls++;
    const DWORD v = fnv(fnv(f->id * 977u + slot, &c, 4), &g_seed, 4);
    ev('S', f->id, slot, c);
    switch (v % 5) {
    case 0: return v % 2000;
    case 1: return 640 + v % 3;
    case 2: return v;   // huge (unsigned)
    case 3: return 0;
    default: return v % 70000;
    }
}
DWORD THISCALL fake_slot39(FakeObj *f) { return fake_size(f, 39); }
DWORD THISCALL fake_slot45(FakeObj *f) { return fake_size(f, 45); }
DWORD THISCALL fake_slot63(FakeObj *f, DWORD arg)
{
    ev('M', f->id, arg);
    return f->id & 1;
}
DWORD THISCALL fake_slot19(FakeObj *f)
{
    ev('9', f->id);
    return 0x1900 + f->id;
}
void init_fakes()
{
    for (DWORD &s : g_fake_vt)
        s = 0;
    g_fake_vt[1] = ptrv((void *)&fake_delete);
    g_fake_vt[2] = ptrv((void *)&fake_class);
    g_fake_vt[19] = ptrv((void *)&fake_slot19);
    g_fake_vt[39] = ptrv((void *)&fake_slot39);
    g_fake_vt[45] = ptrv((void *)&fake_slot45);
    g_fake_vt[63] = ptrv((void *)&fake_slot63);
    for (int i = 0; i < NFAKE; i++)
        g_fakes[i] = {ptrv(g_fake_vt), 0, (DWORD)i + 1, 0};
}

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
DWORD rel(DWORD v)
{
    if (!v)
        return 0;
    for (int i = 0; i < NFAKE; i++)
        if (v == ptrv(&g_fakes[i]))
            return 0xfa000000u + (DWORD)i;
    for (int i = g_nknown[g_side] - 1; i >= 0; i--)
        if (v >= g_known[g_side][i].at_ && v < g_known[g_side][i].at_ + g_known[g_side][i].size)
            return 0xb0000000u + (DWORD)i * 0x10000 + (v - g_known[g_side][i].at_);
    return v;
}

// An object's dwords: raw, or relative to the known blocks, or (other heap pointers) the first dword
// of what they point to when that looks like a vtable
void st_obj(const void *o, DWORD size)
{
    const DWORD *d = static_cast<const DWORD *>(o);
    for (DWORD i = 0; i < size / 4; i++) {
        DWORD v = rel(d[i]);
        if (v == d[i] && heap_pointer(v)) {
            const DWORD first = readable(reinterpret_cast<void *>(v), 4) ? *reinterpret_cast<const DWORD *>(v) : 0;
            v = 0xc0000000u | (first >= 0x5e0000 && first < 0x610000 ? first : 0);
        }
        st(v);
    }
}
void st_text(const char *s) { st(fnv_text(2166136261u, s ? s : "(null)")); }
void st_zs(const ZString *z) { st_text(z->buffer ? z->c_str() : nullptr); }

template <typename C> bool two_sides(Tally &t, const char *what, unsigned seed, C run)
{
    DWORD r[2], mask = 0xffffffff;
    for (int side = 0; side < 2; side++) {
        g_side = side;
        g_seed = seed;
        g_loghash[side] = g_statehash[side] = 2166136261u;
        g_nlog[side] = g_nknown[side] = g_nslog[side] = 0;
        if (g_trace)
            log("    %s seed %u side %d", what, seed, side);
        init_fakes();
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
        log_first_state_difference();
    }
    t.check(same, "layer", d);
    return same;
}

void zs_assign(ZString *z, const char *s) { at<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }

// A pool block of the size filled with a pattern and freed, so the next allocation of that size (most
// likely the same block) starts with the same bytes on both sides
void prime(DWORD size)
{
    void *p = pool_alloc(size);
    memset(p, 0xa5, size);
    pool_free(p, size);
}

BYTE *world() { return *at<BYTE **>(0x6c2080); }

// ---- Objects -------------------------------------------------------------------------------------------

DWORD zax_case(DWORD &mask)
{
    (void)mask;
    const DWORD op = random() % 6;
    static const char *const TEXTS[] = {"", "North Gate", "A rather long compass target name to need a bigger buffer"};
    DWORD r = 0;
    switch (op) {
    case 0:
    case 1: {
        prime(0xc);
        auto *z = static_cast<ZaxLayerSaveData *>(op ? pool_alloc(0xc) : at<void *(__cdecl *)()>(0x5b9c80)());
        if (op) {
            memset(z, 0xa5, 0xc);
            at<void *(THISCALL *)(void *)>(0x5b9ef0)(z);
        }
        know(z, 0xc);
        zs_assign(&z->compass_target, TEXTS[random() % 3]);
        st_obj(z, 8);
        st_zs(&z->compass_target);
        const DWORD flags = random() % 2;
        r = rel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(random() % 2 ? 0x5b9d50 : 0x5b9c50)(z, flags)));
        if (!flags) {   // (a freed block holds the pool's free list)
            st(z->vftable);
            pool_free(z, 0xc);
        }
        break;
    }
    default: {
        const DWORD n = random() % 4;
        BYTE *a = at<BYTE *(__fastcall *)(DWORD, DWORD, DWORD)>(0x5b9cd0)(n, 0, 0);
        know(a - 4, n * 0xc + 4);
        st(n);
        for (DWORD i = 0; i < n; i++) {
            zs_assign(reinterpret_cast<ZString *>(a + i * 0xc + 8), TEXTS[(i + op) % 3]);
            st_obj(a + i * 0xc, 8);
        }
        const DWORD flags = 2 | (random() % 2);
        r = rel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x5b9d50)(a, flags)));
        if (!(flags & 1))
            heap_free(a - 4);
        break;
    }
    }
    return r;
}

DWORD holder_case(DWORD &mask)
{
    (void)mask;
    const DWORD op = random() % 4;
    DWORD r = 0;
    if (op < 2) {
        prime(0x2c);
        auto *h = static_cast<LayerHolder *>(at<void *(__cdecl *)()>(0x4efa20)());
        know(h, 0x2c);
        st_obj(h, 0x2c);
        const DWORD n = random() % 5;
        if (n) {
            h->layers.capacity = n + random() % 3;
            h->layers.data = static_cast<void **>(pool_get_alloc(h->layers.capacity * 4));
            know(h->layers.data, h->layers.capacity * 4);
            for (DWORD i = 0; i < n; i++)
                h->layers.data[i] = random() % 3 ? &g_fakes[i] : nullptr;
            h->layers.count = n;
        }
        const DWORD flags = op;
        r = rel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x4efb20)(h, flags)));
        if (!flags) {
            st(h->vftable);
            st(h->layers.count);
            st(rel(ptrv(h->layers.data)));
            pool_free(h, 0x2c);
        }
    } else {
        const DWORD n = random() % 4;
        BYTE *a = at<BYTE *(__fastcall *)(DWORD, DWORD, DWORD)>(0x4efa80)(n, 0, 0);
        know(a - 4, n * 0x2c + 4);
        for (DWORD i = 0; i < n; i++)
            st_obj(a + i * 0x2c, 0x2c);
        r = rel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x4efb20)(a, 2 | (op & 1))));
        if (!(op & 1))
            heap_free(a - 4);
    }
    return r;
}

// The game's generator for the unique ids (counter, two indices, the table): saved and restored
SavedBytes g_generator;

DWORD holder_small_case(DWORD &mask)
{
    restore_bytes(g_generator);
    alignas(8) BYTE h[0x2c];
    memset(h, 0, sizeof h);
    auto *lh = reinterpret_cast<LayerHolder *>(h);
    DWORD r = 0;
    const DWORD op = random() % 3;
    if (op == 0) {
        lh->unique_id = random() % 4 ? 0 : random();
        lh->playing_time[0] = random();
        lh->playing_time[1] = random();
        r = at<DWORD(THISCALL *)(void *)>(0x4efe20)(h);
        st(*at<DWORD *>(0x7124c4));
        st(*at<DWORD *>(0x7124d0));
        st(*at<DWORD *>(0x7124d4));
    } else {
        for (int i = 0; i < 6; i++) {
            DWORD t[2] = {random(), random()};
            if (random() % 2)
                t[1] = (t[1] & 0x800fffffu) | 0x40000000u;   // a moderate exponent
            lh->playing_time[0] = random();
            lh->playing_time[1] = random() % 2 ? (random() & 0x800fffffu) | 0x40800000u : random();
            double d;
            memcpy(&d, t, 8);
            at<void(THISCALL *)(void *, double)>(0x4efe90)(h, d);
            st(lh->playing_time[0]);
            st(lh->playing_time[1]);
        }
        mask = 0;
    }
    st_obj(h, 0x2c);
    return r;
}

// A dword of an object: relative to the object itself, a heap pointer by what it points to, else raw
DWORD own_mapped(const BYTE *o, DWORD size, DWORD off)
{
    const DWORD v = *reinterpret_cast<const DWORD *>(o + off);
    if (v >= ptrv(o) && v < ptrv(o) + size)
        return 0xb0000000u + (v - ptrv(o));
    const DWORD r = rel(v);
    if (r != v)
        return r;
    if (heap_pointer(v)) {
        const DWORD first = readable(reinterpret_cast<void *>(v), 4) ? *reinterpret_cast<const DWORD *>(v) : 0;
        return 0xc0000000u | (first >= 0x5e0000 && first < 0x610000 ? first : 0);
    }
    return v;
}

// The members the constructors certainly set (vtables, back pointers, the owned objects, Rolloff)
constexpr DWORD SAVE_DATA_KEY[] = {0, 0x3c, 0x40, 0x9c, 0xa4, 0xb4, 0x144, 0x14c};
constexpr DWORD LAYER_KEY[] = {0, 0x3c, 0x78, 0x7c, 0xd8, 0xe0, 0xf0, 0x180, 0x188, 0x1a0};
void st_key(const BYTE *o, bool layer)
{
    const DWORD size = layer ? 0x1d0 : 0x190;
    if (layer)
        for (DWORD off : LAYER_KEY)
            st(own_mapped(o, size, off));
    else
        for (DWORD off : SAVE_DATA_KEY)
            st(own_mapped(o, size, off));
}

void construct(BYTE *o, bool layer, DWORD op)
{
    if (!layer)
        at<void *(THISCALL *)(void *)>(0x4e6cb0)(o);
    else if (op == 1)
        at<void *(THISCALL *)(void *)>(0x4e9da0)(o);
    else
        at<void *(THISCALL *)(void *, void *)>(0x4e9e00)(o, *reinterpret_cast<void **>(world() + 0x2a8));
}

// Save data and layers: Create / CreateArray / constructors / deleting destructors. The constructors run
// on two blocks with different patterns; the dwords that come out equal are the ones they set, and
// those are compared (Create and CreateArray: the key members only)
DWORD big_case(bool layer, DWORD &mask)
{
    (void)mask;
    const DWORD size = layer ? 0x1d0 : 0x190;
    const DWORD saved_sound = *at<DWORD *>(SOUND_LAYER);
    *at<DWORD *>(SOUND_LAYER) = 0x12345;
    const DWORD op = random() % 5;
    DWORD r = 0;
    if (op < 3) {
        BYTE *o;
        if (op == 0) {
            o = at<BYTE *(__cdecl *)()>(layer ? 0x4e99b0 : 0x4e5210)();
            st_key(o, layer);
        } else {
            o = static_cast<BYTE *>(pool_alloc(size));
            BYTE *b = static_cast<BYTE *>(pool_alloc(size));
            memset(o, 0xa5, size);
            memset(b, 0x5a, size);
            construct(o, layer, op);
            construct(b, layer, op);
            unsigned written = 2166136261u;
            for (DWORD off = 0; off < size; off += 4) {
                const DWORD v = own_mapped(o, size, off);
                if (v == own_mapped(b, size, off)) {
                    written = fnv(written, &off, 4);
                    st(v);
                }
            }
            st(written);
            at<void *(THISCALL *)(void *, DWORD)>(layer ? 0x4e9ad0 : 0x4e5280)(b, 1);
        }
        know(o, size);
        st(*at<DWORD *>(SOUND_LAYER));
        const DWORD flags = random() % 2;
        r = rel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(layer ? 0x4e9ad0 : 0x4e5280)(o, flags)));
        if (!flags) {
            st(*reinterpret_cast<DWORD *>(o));
            pool_free(o, size);
        }
    } else {
        const DWORD n = 1 + random() % 2;
        BYTE *a = at<BYTE *(__fastcall *)(DWORD, DWORD, DWORD)>(layer ? 0x4e9a20 : 0x4e5230)(n, 0, 0);
        know(a - 4, n * size + 4);
        for (DWORD i = 0; i < n; i++)
            st_key(a + i * size, layer);
        r = rel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(layer ? 0x4e9ad0 : 0x4e5280)(a, 2 | (op & 1))));
        if (!(op & 1))
            heap_free(a - 4);
    }
    *at<DWORD *>(SOUND_LAYER) = saved_sound;
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
bool same_dword(const BYTE *a, const BYTE *b, DWORD off)
{
    const DWORD x = *reinterpret_cast<const DWORD *>(a + off), y = *reinterpret_cast<const DWORD *>(b + off);
    return x == y || same_text_buffer(x, y);
}
bool same_desc(const BYTE *a, const BYTE *b, int depth = 0)
{
    if (!a || !b)
        return a == b;
    for (DWORD off = 0; off < 0x24; off += 4)
        if (!same_dword(a, b, off))
            return false;
    auto d = [&](DWORD from, DWORD to) {
        for (DWORD off = from; off < to; off += 4)
            if (!same_dword(a, b, off))
                return false;
        return true;
    };
    auto byte = [&](DWORD off) { return a[off] == b[off]; };
    const DWORD vt = *reinterpret_cast<const DWORD *>(a);
    switch (vt) {
    case 0x5ec91c: return d(0x2c, 0x34);           // string: +0x28 is not written
    case 0x5ecbf4: return d(0x28, 0x3c);           // object
    case 0x5ecd6c:                                 // object array: the element descriptor at +0x38
        if (!d(0x28, 0x38) || !d(0x3c, 0x44) || !byte(0x44))
            return false;
        return depth == 0 ? same_desc(*reinterpret_cast<BYTE *const *>(a + 0x38), *reinterpret_cast<BYTE *const *>(b + 0x38), 1)
                          : true;
    case 0x5ec684: return d(0x28, 0x34) && byte(0x34);   // flags
    case 0x5fd518:                                 // enum int: +0x40 the enum names
        return d(0x28, 0x40) && (*reinterpret_cast<const DWORD *>(a + 0x40) != 0) == (*reinterpret_cast<const DWORD *>(b + 0x40) != 0);
    case 0x5fdb68: return d(0x28, 0x40);           // float
    case 0x5fd970:                                 // typed resource: +0x2e/+0x2f padding
        return byte(0x28) && byte(0x29) && byte(0x2a) && byte(0x2b) && byte(0x2c) && byte(0x2d) && d(0x30, 0x38);
    case 0x5fd738: return byte(0x28) && byte(0x29);   // byte: default and setter flag
    }
    return true;   // other kinds: the base part
}

struct GetterDef {
    DWORD address, var;
};
constexpr GetterDef GETTERS[] = {{0x4e5160, CLASS_CUSTOM_SAVE_DATA}, {0x4e52f0, CLASS_LAYER_SAVE_DATA},
                                 {0x5b9c00, CLASS_ZAX_LAYER_SAVE_DATA}, {0x5b9e00, CLASS_ZAX_LAYER_SAVE_DATA},
                                 {0x4e9c80, CLASS_LAYER}, {0x4efc20, CLASS_LAYER_HOLDER}};
struct RegFields {
    const char *name;
    DWORD address, var;
};
constexpr RegFields REGISTER_FIELDS[] = {{"CLayerSaveData", 0x4e5350, CLASS_LAYER_SAVE_DATA},
                                         {"CZaxLayerSaveData", 0x5b9e60, CLASS_ZAX_LAYER_SAVE_DATA},
                                         {"CLayer", 0x4e9cf0, CLASS_LAYER},
                                         {"CLayerHolder", 0x4efc80, CLASS_LAYER_HOLDER}};
constexpr DWORD MAXF = 48;

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
                "layer", d);
    }
    for (const RegFields &rf : REGISTER_FIELDS)
        for (int var_zero = 0; var_zero < 2; var_zero++) {
            static CFieldDesc *fields[2][MAXF];
            DWORD n[2], result[2];
            for (int side = 0; side < 2; side++) {
                if (side == 0)
                    hooks_out();
                CClassInfo *&var = *at<CClassInfo **>(rf.var);
                CClassInfo *const saved = var;
                alignas(4) BYTE fake_class[sizeof(CClassInfo)];
                memset(fake_class, 0, sizeof fake_class);
                var = var_zero ? nullptr : reinterpret_cast<CClassInfo *>(fake_class);
                CodePatch p;
                patch_jump(p, 0x415780, (void *)&fake_register);
                void *fn = at<void *>(rf.address);
                if (side == 1 && hook_sites_fit("layer", hooks_layer(nullptr), MAX_SITES)) {
                    Hook list[MAX_SITES];
                    const int nh = hooks_layer(list);
                    for (int i = 0; i < nh; i++)
                        if (list[i].address == rf.address)
                            fn = list[i].replacement;
                }
                result[side] = reinterpret_cast<DWORD(__cdecl *)()>(fn)();
                unpatch(p);
                CClassInfo *c = var;
                n[side] = c->num_fields < MAXF ? c->num_fields : MAXF;
                for (DWORD i = 0; i < MAXF; i++)
                    fields[side][i] = i < n[side] ? c->fields[i] : nullptr;
                result[side] = result[side] ? 1 : 0;
                if (c->fields)
                    heap_free(c->fields);
                if (var_zero)
                    pool_free(c, sizeof(CClassInfo));
                var = saved;
                if (side == 0)
                    hooks_in();
            }
            bool same = n[0] == n[1] && n[0] > 0 && result[0] == result[1];
            DWORD first_diff = 0xffffffff;
            for (DWORD i = 0; i < n[0] && same; i++)
                if (!(same = same_desc(reinterpret_cast<const BYTE *>(fields[0][i]), reinterpret_cast<const BYTE *>(fields[1][i]))))
                    first_diff = i;
            char d[160];
            wsprintfA(d, "[RegisterFields %s var %d: %u / %u fields, eax %x / %x, first difference %d]", rf.name, var_zero, n[0], n[1],
                      result[0], result[1], first_diff);
            if (!same && first_diff != 0xffffffff) {
                const DWORD *a = reinterpret_cast<const DWORD *>(fields[0][first_diff]), *b = reinterpret_cast<const DWORD *>(fields[1][first_diff]);
                for (DWORD k = 0; k < 0x48 / 4; k++)
                    if (a[k] != b[k] && !same_text_buffer(a[k], b[k]))
                        log("    field %u dword %u: %x / %x", first_diff, k, a[k], b[k]);
            }
            t.check(same, rf.name, d);
            const CClassInfo *c = *at<CClassInfo **>(rf.var);
            bool same_real = c && c->num_fields - c->num_inherited == n[1];
            for (DWORD i = 0; same_real && i < n[1]; i++)
                same_real = same_desc(reinterpret_cast<const BYTE *>(c->fields[c->num_inherited + i]), reinterpret_cast<const BYTE *>(fields[1][i]));
            wsprintfA(d, "[registered %s: %u own fields, made %u]", rf.name, c ? c->num_fields - c->num_inherited : 0, n[1]);
            real.check(same_real, rf.name, d);
            for (int side = 0; side < 2; side++)
                for (DWORD i = 0; i < n[side]; i++)
                    if (fields[side][i])
                        vcall<void *>(fields[side][i], 0, 1);
        }
}

// ---- Small functions -----------------------------------------------------------------------------------

int rnd_int()
{
    switch (random() % 4) {
    case 0: return (int)(random() % 200) - 100;
    case 1: return (int)random();
    case 2: return (int)(random() % 9) - 4;
    default: return (int)(random() % 20000);
    }
}
float rnd_float()
{
    DWORD b;
    switch (random() % 6) {
    case 0: b = 0x7f800000; break;
    case 1: b = 0xffc00000; break;   // the default NaN
    case 2: {
        const float f = (float)((int)(random() % 20001) - 10000) / 4.0f;
        return f;
    }
    case 3: b = (random() & 0x807fffffu) | ((100 + random() % 60) << 23); break;
    default: {
        const float f = (float)(int)(random() % 100000) * 0.37f - 5000.0f;
        return f;
    }
    }
    float f;
    memcpy(&f, &b, 4);
    return f;
}

DWORD small_case(DWORD &mask)
{
    const DWORD op = random() % 9;
    DWORD r = 0;
    switch (op) {
    case 0: {   // Set, Union, Intersects
        int a[4], b[4];
        for (int &v : a)
            v = rnd_int();
        for (int &v : b)
            v = rnd_int();
        r = at<DWORD(THISCALL *)(void *, int, int, int, int)>(0x4ee7f0)(a, b[0], b[1], b[2], b[3]);
        for (int &v : b)
            v = rnd_int();
        st(at<DWORD(THISCALL *)(void *, const int *)>(0x4ee860)(a, b) & 0xff);
        st(at<DWORD(THISCALL *)(void *, const int *)>(0x4ee810)(a, b));
        for (int v : a)
            st((DWORD)v);
        break;
    }
    case 1: {   // Dot
        float a[2] = {rnd_float(), rnd_float()}, b[2] = {rnd_float(), rnd_float()};
        const long double v = at<long double(THISCALL *)(const float *, const float *)>(0x4ee8c0)(a, b);
        BYTE raw[10];
        memcpy(raw, &v, 10);
        st(fnv(2166136261u, raw, 10));
        mask = 0;
        break;
    }
    case 2: {   // RemoveAndDelete
        const DWORD n = 1 + random() % 8;
        LayerArray arr{0x5f8ad0, static_cast<void **>(pool_get_alloc(n * 4)), n, n};
        know(arr.data, n * 4);
        for (DWORD i = 0; i < n; i++)
            arr.data[i] = random() % 4 ? &g_fakes[i] : nullptr;
        const DWORD k = random() % 3 ? 2 + random() % 4 : 1;
        for (DWORD i = 0; i < k && arr.count; i++)
            at<void(THISCALL *)(void *, DWORD)>(0x4eea50)(&arr, random() % arr.count);
        st(arr.count);
        for (DWORD i = 0; i < arr.count; i++)
            st(rel(ptrv(arr.data[i])));
        pool_get_free(arr.data, n * 4);
        mask = 0;
        break;
    }
    case 3: {   // Reserve16
        DWORD cap = random() % 6;
        LayerArray arr{0x5f8b74, cap ? static_cast<void **>(pool_get_alloc(cap * 16)) : nullptr, cap ? random() % (cap + 1) : 0, cap};
        for (DWORD i = 0; i < arr.count * 4; i++)
            arr.data[i] = reinterpret_cast<void *>(static_cast<uintptr_t>(random()));
        const DWORD n = random() % 10;
        at<void(THISCALL *)(void *, DWORD)>(0x4eeab0)(&arr, n);
        st(arr.capacity);
        st(arr.count);
        st(arr.data != nullptr);
        for (DWORD i = 0; i < arr.count * 4; i++)
            st(ptrv(arr.data[i]));
        if (arr.data)
            pool_get_free(arr.data, arr.capacity * 16);
        mask = 0;
        break;
    }
    case 4: {   // ViewRect
        int pos[2] = {rnd_int(), rnd_int()}, rect[4] = {7, 7, 7, 7};
        at<void(THISCALL *)(void *, const int *, int *)>(0x4ea110)(&g_fakes[random() % NFAKE], pos, rect);
        for (int v : rect)
            st((DWORD)v);
        mask = 0;
        break;
    }
    case 5: {   // ViewRectAt
        float pos[2] = {rnd_float(), rnd_float()};
        int rect[4] = {7, 7, 7, 7};
        r = (DWORD)at<int(THISCALL *)(void *, const float *, int *)>(0x4ea660)(&g_fakes[random() % NFAKE], pos, rect);
        for (int v : rect)
            st((DWORD)v);
        break;
    }
    case 6: {   // the draw byte
        const BYTE saved = *at<BYTE *>(DRAW_DIRECT);
        at<void(__fastcall *)(DWORD)>(0x4e9d90)(random() % 3 ? random() : 0);
        st(*at<BYTE *>(DRAW_DIRECT));
        r = at<DWORD(__cdecl *)()>(0x4e9d80)() & 0xff;
        *at<BYTE *>(DRAW_DIRECT) = saved;
        break;
    }
    case 7: {   // ClearByteBC
        BYTE buf[0xc0];
        for (BYTE &b : buf)
            b = (BYTE)random();
        at<void(THISCALL *)(void *)>(0x4ea870)(buf);
        st(fnv(2166136261u, buf, sizeof buf));
        mask = 0;
        break;
    }
    default: {   // the save data's small getters on a buffer
        alignas(4) BYTE buf[0x190];
        for (DWORD i = 0; i < sizeof buf; i++)
            buf[i] = (BYTE)random();
        know(buf, sizeof buf);
        st(rel(ptrv(at<void *(THISCALL *)(void *)>(0x4e7170)(buf))));
        r = rel(ptrv(at<void *(THISCALL *)(void *)>(0x4e7bf0)(buf)));
        break;
    }
    }
    return r;
}

// ---- Save data methods on a fresh layer ----------------------------------------------------------------

Layer *new_layer()
{
    const DWORD saved_sound = *at<DWORD *>(SOUND_LAYER);
    Layer *l = static_cast<Layer *>(at<void *(__cdecl *)()>(0x4e99b0)());
    *at<DWORD *>(SOUND_LAYER) = saved_sound;
    know(l, 0x1d0);
    return l;
}
void delete_layer(Layer *l) { vcall<void *>(l, 1, 1); }

DWORD methods_case(DWORD &mask)
{
    Layer *l = new_layer();
    LayerSaveData *s = &l->data;
    DWORD r = 0;
    const DWORD op = random() % 7;
    switch (op) {
    case 0: {   // SetCustom
        st(rel(ptrv(s->custom)));
        at<DWORD(THISCALL *)(void *, void *)>(0x4e6990)(s, &g_fakes[1]);   // eax: the caller's when there was none
        r = at<DWORD(THISCALL *)(void *, void *)>(0x4e6990)(s, random() % 2 ? &g_fakes[2] : nullptr);
        r = rel(r);
        st(rel(ptrv(s->custom)));
        s->custom = nullptr;
        break;
    }
    case 1: {   // objects made on first use
        for (int i = 0; i < 2; i++) {
            void *a = at<void *(THISCALL *)(void *)>(0x4e70d0)(s);
            void *b = at<void *(THISCALL *)(void *)>(0x4e7180)(s);
            void *c = at<void *(THISCALL *)(void *)>(0x4e7100)(s);
            st(*static_cast<DWORD *>(a));
            st(*static_cast<DWORD *>(b));
            st(*static_cast<DWORD *>(c));
            st(a == s->path_data && b == s->active_paths && c == s->frame_fc);
        }
        mask = 0;
        break;
    }
    case 2: {   // SetShake
        alignas(4) BYTE env[0x24];
        for (DWORD i = 0; i < sizeof env; i += 4) {
            const float f = (float)(random() % 1000) / 7.0f;
            memcpy(env + i, &f, 4);
        }
        env[0x1c] = (BYTE)(random() % 2);
        float t = (float)(random() % 100) / 3.0f;
        DWORD tb;
        memcpy(&tb, &t, 4);
        at<void(THISCALL *)(void *, DWORD, const BYTE *)>(0x4e9f00)(l, tb, env);
        st_obj(&s->shake_time, 0x30);
        mask = 0;
        break;
    }
    case 3: {   // message handler and Precache
        at<DWORD(THISCALL *)(void *, void *, const char *)>(0x4e6be0)(s, &g_fakes[0], "Delete");   // (Precache: real data)
        mask = 0;
        break;
    }
    case 4: {   // file name
        static const char *const NAMES[] = {"Test/Nachbau1.zax", "Levels/ACT 1/M1M2/01 Main", "a.b.c", ".zax", "", "NoDot"};
        for (int i = 0; i < 3; i++) {
            at<void(THISCALL *)(void *, const char *)>(0x4eb6b0)(l, NAMES[random() % 6]);
            st_text(at<const char *(THISCALL *)(void *)>(0x4eb670)(l));
        }
        mask = 0;
        break;
    }
    case 5: {   // the listener environment, with the values the running level has
        void *cache = at<void *(__cdecl *)()>(0x57aef0)();
        (void)cache;
        DWORD type = 0;
        float size = 7.5f;
        if (BYTE *w = world()) {
            const LayerArray *layers = reinterpret_cast<const LayerArray *>(w + 0x2b8);
            if (layers->count && layers->data[0]) {
                type = static_cast<Layer *>(layers->data[0])->data.environment_type;
                size = static_cast<Layer *>(layers->data[0])->data.environment_size;
            }
        }
        r = at<DWORD(THISCALL *)(void *, DWORD)>(0x4e6c20)(s, type);
        const float f = at<float(THISCALL *)(void *, float)>(0x4e6c80)(s, size);
        st(bits(f));
        st(s->environment_type);
        st(bits(s->environment_size));
        break;
    }
    default: {   // music mix and fog
        void *saved = s->music;
        s->music = &g_fakes[3];
        r = rel(ptrv(at<void *(THISCALL *)(void *)>(0x4e7bf0)(s)));
        s->music = saved;
        st(rel(ptrv(at<void *(THISCALL *)(void *)>(0x4e7170)(s))));
        break;
    }
    }
    st_obj(s, 0x14);
    st_obj(&s->tile_map, 0xa0 - 0x40);
    delete_layer(l);
    return r;
}

// ---- Parts in fresh layers -----------------------------------------------------------------------------

CFieldDesc *polygon_field()
{
    CClassInfo *c = *at<CClassInfo **>(0x6bfb08);
    for (DWORD i = 0; c && i < c->num_fields; i++)
        if (c->fields[i]->offset == 0x4c && c->fields[i]->vftable == 0x5fd2a0)
            return c->fields[i];
    return nullptr;
}

constexpr int NPARTS = 8;
BYTE *g_parts[NPARTS];

void sized(Layer *l, int w, int h)
{
    l->data.width = w;
    l->data.height = h;
    at<void *(THISCALL *)(void *, int, int)>(0x5793e0)(l->data.grid, w, h);
    at<void(THISCALL *)(void *, int, int)>(0x53f680)(l->data.tile_map, w, h);
}

DWORD st_part_index(const void *p)
{
    for (int i = 0; i < NPARTS; i++)
        if (p == g_parts[i])
            return 0x9a000000u + (DWORD)i;
    return rel(ptrv(p));
}

void st_layer_parts(Layer *l)
{
    const LayerArray &m = l->data.monitor;
    st(m.count);
    for (DWORD i = 0; i < m.count; i++) {
        const BYTE *e = static_cast<const BYTE *>(m.data[i]);
        st(*reinterpret_cast<const DWORD *>(e));
        st(st_part_index(*reinterpret_cast<void *const *>(e + 8)));
        st(*reinterpret_cast<const DWORD *>(e + 0xc));
        st(*reinterpret_cast<const DWORD *>(e + 0x10));
    }
    const BYTE *g = l->data.grid;
    const DWORD cols = *reinterpret_cast<const DWORD *>(g + 0x10), rows = *reinterpret_cast<const DWORD *>(g + 0x14);
    const LayerArray *cells = *reinterpret_cast<LayerArray *const *>(g + 0x18);
    for (DWORD i = 0; cells && i < cols * rows; i++) {
        st(cells[i].count);
        for (DWORD k = 0; k < cells[i].count; k++)
            st(st_part_index(cells[i].data[k]));
    }
}

DWORD parts_case(DWORD &mask)
{
    Layer *a = new_layer();
    Layer *b = new_layer();
    sized(a, 2000, 1500);
    sized(b, 1000 + (int)(random() % 3000), 1200);
    CFieldDesc *pf = polygon_field();
    for (int i = 0; i < NPARTS; i++) {
        g_parts[i] = static_cast<BYTE *>(at<void *(__cdecl *)()>(0x4b5d50)());
        char text[200];
        const int x = (int)(random() % 1800), y = (int)(random() % 1300), w = 10 + (int)(random() % 300), h = 10 + (int)(random() % 200);
        wsprintfA(text, "%d, %d, %d, %d, %d, %d, %d, %d, %d, %d", x, y, x + w, y, x + w, y + h, x, y + h, x, y);
        if (pf)
            vcall<DWORD>(pf, 19, g_parts[i], text, 0);
        *reinterpret_cast<DWORD *>(g_parts[i] + 0x1c) |= random() % 2 ? 0x20000 : 0;
    }
    DWORD r = 0;
    for (int step = 0; step < 24; step++) {
        BYTE *p = g_parts[random() % NPARTS];
        Layer *l = random() % 2 ? a : b;
        const DWORD op = random() % 9;
        DWORD v = 0;
        switch (op) {
        case 0:
        case 1: v = at<DWORD(THISCALL *)(void *, void *)>(0x4eb6f0)(l, p); break;
        case 2: v = at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x4e72a0)(&l->data, p, random() % 50, random() % 50); break;
        case 3: v = at<DWORD(THISCALL *)(void *, void *)>(0x4e7330)(&l->data, p); break;
        case 4: at<DWORD(THISCALL *)(void *, void *)>(0x4e73c0)(&l->data, p); break;
        case 5:
            if (*reinterpret_cast<Layer **>(p + 0x48) == l)
                at<void(THISCALL *)(void *, void *)>(0x4ebcb0)(l, p);
            break;
        case 6: {
            alignas(4) BYTE list[0x18];
            memset(list, 0, sizeof list);
            *reinterpret_cast<DWORD *>(list) = 0x602dbc;
            int rect[4] = {(int)(random() % 1000), (int)(random() % 800), 0, 0};
            rect[2] = rect[0] + (int)(random() % 1500);
            rect[3] = rect[1] + (int)(random() % 1000);
            void *tmp;
            at<void *(THISCALL *)(void **)>(0x59a470)(&tmp);
            v = at<DWORD(THISCALL *)(void *, void *, const int *, DWORD, DWORD, DWORD, DWORD)>(0x4ed5e0)(l, tmp, rect, random() % 6, 0, 0,
                                                                                                         0);
            const LayerArray *found = static_cast<const LayerArray *>(tmp);
            st(found->count);
            for (DWORD i = 0; i < found->count; i++)
                st(st_part_index(found->data[i]));
            at<void(THISCALL *)(void **)>(0x59a510)(&tmp);
            v = 0;
            break;
        }
        case 7: {
            int rect[4] = {0, 0, 2000, 1500};
            v = at<DWORD(THISCALL *)(void *, void *, const int *, DWORD, DWORD, DWORD)>(0x4ed610)(l, &g_fakes[5], rect, 0, 0, 0) & 0xff;
            break;
        }
        default: {
            const float point[2] = {(float)(random() % 2000), (float)(random() % 1500)};
            const float radius = (float)(random() % 800), yscale = 1.0f;
            at<DWORD(THISCALL *)(void *, void *, const float *, float, float, DWORD, DWORD, DWORD)>(0x4ed690)(l, &g_fakes[6], point, radius,
                                                                                                          yscale, 0, 0, 0);
            break;
        }
        }
        ev('P', op, st_part_index(reinterpret_cast<void *>(static_cast<uintptr_t>(v))) == v && heap_pointer(v) ? 0xc0000000u : st_part_index(reinterpret_cast<void *>(static_cast<uintptr_t>(v))));
    }
    at<DWORD(THISCALL *)(void *)>(0x4ed5c0)(a);
    st_layer_parts(a);
    st_layer_parts(b);
    for (int i = 0; i < NPARTS; i++) {
        st(st_part_index(*reinterpret_cast<void **>(g_parts[i] + 0x48)));
        st_obj(g_parts[i] + 0x1c, 8);   // flags, used in, id (the team +0x24 is not set, the stamp +0x2c is a global counter)
        st_obj(g_parts[i] + 0x28, 4);
        st_obj(g_parts[i] + 0x30, 8);
        st(*reinterpret_cast<DWORD *>(g_parts[i] + 0x10) != 0);
    }
    // Some parts deleted through the layer, then the layers (they delete the parts in their grids)
    for (int i = 0; i < NPARTS; i++) {
        Layer *l = *reinterpret_cast<Layer **>(g_parts[i] + 0x48);
        if (l && random() % 2) {
            at<void(THISCALL *)(void *, void *)>(0x4ebd60)(l, g_parts[i]);
            g_parts[i] = nullptr;
        }
    }
    st_layer_parts(a);
    st_layer_parts(b);
    for (int i = 0; i < NPARTS; i++)
        if (g_parts[i] && !*reinterpret_cast<void **>(g_parts[i] + 0x48))
            vcall<void *>(g_parts[i], 1, 1);
    delete_layer(a);
    delete_layer(b);
    for (BYTE *&p : g_parts)
        p = nullptr;
    mask = 0;
    return r;
}

// ---- Real data -----------------------------------------------------------------------------------------

char g_temp[MAX_PATH], g_temp_rel[MAX_PATH];

bool make_temp_paths()
{
    char tmp[MAX_PATH], data[MAX_PATH];
    g_temp[0] = g_temp_rel[0] = 0;
    if (!GetTempPathA(MAX_PATH - 40, tmp))
        return false;
    wsprintfA(g_temp, "%sZaxLayer\\", tmp);
    CreateDirectoryA(g_temp, nullptr);
    if (!GetFullPathNameA(at<const char *>(0x6135ec /* the data folder */), MAX_PATH, data, nullptr))
        return false;
    if ((data[0] | 0x20) != (g_temp[0] | 0x20) || data[1] != ':' || g_temp[1] != ':')
        return false;
    int depth = 0;
    for (const char *s = data + 2; *s; s++)
        if ((*s == '\\' || *s == '/') && s[1] && s[1] != '\\' && s[1] != '/')
            depth++;
    char *o = g_temp_rel;
    for (int i = 0; i < depth; i++) {
        lstrcpyA(o, "..\\");
        o += 3;
    }
    lstrcpyA(o, g_temp + 3);
    const int n = lstrlenA(g_temp_rel);
    if (n && g_temp_rel[n - 1] == '\\')
        g_temp_rel[n - 1] = 0;
    return true;
}

// A file of the temp folder read (heap block, size); null when missing
BYTE *read_file(const char *name, DWORD *size)
{
    char path[MAX_PATH];
    wsprintfA(path, "%s%s", g_temp, name);
    HANDLE f = CreateFileA(path, 0x80000000u, 1, nullptr, 3, 0, nullptr);
    *size = 0;
    if (f == INVALID_HANDLE_VALUE)
        return nullptr;
    const DWORD n = GetFileSize(f, nullptr);
    BYTE *p = static_cast<BYTE *>(heap_alloc(n + 1));
    DWORD got = 0;
    ReadFile(f, p, n, &got, nullptr);
    CloseHandle(f);
    *size = got;
    return p;
}
void delete_file(const char *name)
{
    char path[MAX_PATH];
    wsprintfA(path, "%s%s", g_temp, name);
    DeleteFileA(path);
}
void write_file(const char *name, const void *p, DWORD n)
{
    char path[MAX_PATH];
    wsprintfA(path, "%s%s", g_temp, name);
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return;
    DWORD w;
    WriteFile(f, p, n, &w, nullptr);
    CloseHandle(f);
}
// The game's path of a temp file (relative to the data folder)
void game_path(char *out, const char *name) { wsprintfA(out, "%s\\%s", g_temp_rel, name); }

// A hash of a text with every "0x" + 8 hex digits replaced by the number of its first appearance
unsigned norm_hash(const BYTE *p, DWORD n, DWORD *ids)
{
    static DWORD seen[4096];
    DWORD nseen = 0;
    unsigned h = 2166136261u;
    auto hex = [](BYTE c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); };
    for (DWORD i = 0; i < n; i++) {
        if (p[i] == '0' && i + 10 <= n && (p[i + 1] == 'x' || p[i + 1] == 'X')) {
            bool all = true;
            DWORD v = 0;
            for (DWORD k = 2; k < 10 && all; k++) {
                all = hex(p[i + k]);
                const BYTE c = p[i + k];
                v = v * 16 + (DWORD)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
            }
            if (all && !(i + 10 < n && hex(p[i + 10]))) {
                DWORD k = 0;
                while (k < nseen && seen[k] != v)
                    k++;
                if (k == nseen && nseen < 4096)
                    seen[nseen++] = v;
                const DWORD tag[2] = {0xbeef, k};
                h = fnv(h, tag, 8);
                i += 9;
                continue;
            }
        }
        h = fnv(h, &p[i], 1);
    }
    *ids = nseen;
    return h;
}

// A file of this side hashed into the state (normalized or raw), its size and hash logged
void st_file(const char *name, bool normalize, const char *what)
{
    DWORD n, ids = 0;
    BYTE *p = read_file(name, &n);
    const unsigned h = !p ? 0 : normalize ? norm_hash(p, n, &ids) : fnv(2166136261u, p, n);
    st(n);
    st(h);
    log("    %s side %d: %u bytes, hash %x%s", what, g_side, n, h, normalize ? " (ids numbered)" : "");
    if (p)
        heap_free(p);
}

// An object stream on a temp file: written (flags 3) or read (flags 0x204), mode 1
struct Stream {
    alignas(4) BYTE b[0x24];
    bool open;
};
bool open_stream(Stream &s, const char *name, bool write)
{
    char path[MAX_PATH];
    game_path(path, name);
    at<void *(THISCALL *)(void *)>(0x5240b0)(s.b);
    s.open = (at<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x5242b0)(s.b, path, write ? 3 : 0x204, 1) & 0xff) != 0;
    return s.open;
}
void close_stream(Stream &s) { at<DWORD(THISCALL *)(void *)>(0x5241e0)(s.b); }

Layer *running_layer()
{
    BYTE *w = world();
    if (!w)
        return nullptr;
    const LayerArray *layers = reinterpret_cast<const LayerArray *>(w + 0x2b8);
    if (!readable(layers, 0x10) || !layers->count || !readable(layers->data, 4))
        return nullptr;
    Layer *l = static_cast<Layer *>(layers->data[0]);
    return readable(l, 0x1d0) && *reinterpret_cast<DWORD *>(l) == VT_LAYER ? l : nullptr;
}

// The flags +0x7d/+0x7e of the entries 0x6cfe08 the loading clears: saved and restored
BYTE g_entry_flags[2 * 256];
DWORD g_nentries;
void save_entry_flags()
{
    g_nentries = *at<DWORD *>(0x6cfe0c) < 256 ? *at<DWORD *>(0x6cfe0c) : 256;
    for (DWORD i = 0; i < g_nentries; i++) {
        g_entry_flags[2 * i] = (*at<BYTE **>(0x6cfe08))[i * 0x80 + 0x7d];
        g_entry_flags[2 * i + 1] = (*at<BYTE **>(0x6cfe08))[i * 0x80 + 0x7e];
    }
}
void restore_entry_flags()
{
    for (DWORD i = 0; i < g_nentries; i++) {
        (*at<BYTE **>(0x6cfe08))[i * 0x80 + 0x7d] = g_entry_flags[2 * i];
        (*at<BYTE **>(0x6cfe08))[i * 0x80 + 0x7e] = g_entry_flags[2 * i + 1];
    }
}

Layer *load_level_text(const char *level)
{
    Layer *l = static_cast<Layer *>(pool_alloc(0x1d0));
    at<void *(THISCALL *)(void *, void *)>(0x4e9e00)(l, *reinterpret_cast<void **>(world() + 0x2a8));
    know(l, 0x1d0);
    alignas(4) BYTE s[0x24];
    at<void *(THISCALL *)(void *)>(0x5240b0)(s);
    DWORD ok = at<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x5242b0)(s, level, 0x204, 1) & 0xff;
    ev('O', ok);
    if (ok) {
        char line[256];
        at<DWORD(THISCALL *)(void *, char *, DWORD)>(0x4a20a0)(s, line, sizeof line);
        ev('L', fnv_text(2166136261u, line));
        ev('T', at<DWORD(THISCALL *)(void *, void *)>(0x4ec090)(l, s) & 0xff);
        at<DWORD(THISCALL *)(void *)>(0x524340)(s);
    }
    at<DWORD(THISCALL *)(void *)>(0x5241e0)(s);
    return l;
}

// Writes a layer as text into a temp file through 0x4ebe30
void save_text(Layer *l, const char *name)
{
    char path[MAX_PATH];
    game_path(path, name);
    ev('W', at<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x4ebe30)(l, path, 1) & 0xff);
}

DWORD real_case(int which, DWORD &mask)
{
    Layer *run = running_layer();
    if (!run)
        return 0;
    mask = 0;
    const DWORD saved_sound = *at<DWORD *>(SOUND_LAYER);
    save_entry_flags();
    char level[MAX_PATH];
    wsprintfA(level, "Levels/%s.zax", run->data.partial_name.c_str());
    switch (which) {
    case 0: {   // the running layer: text through SaveText, text and binary through the streams
        save_text(run, "run.txt");
        st_file("run.txt", false, "running layer, SaveText");
        Stream s;
        if (open_stream(s, "run_w.txt", true))
            ev('w', at<DWORD(THISCALL *)(void *, void *)>(0x4ebe10)(run, s.b) & 0xff);
        close_stream(s);
        st_file("run_w.txt", false, "running layer, WriteText");
        if (open_stream(s, "run.bin", true))
            ev('b', at<DWORD(THISCALL *)(void *, void *)>(0x4ebe90)(run, s.b) & 0xff);
        close_stream(s);
        st_file("run.bin", false, "running layer, WriteBinary");
        ev('p', at<DWORD(THISCALL *)(void *)>(0x4e99a0)(run));   // Precache of the loaded level (slot 7)
        ev('q', at<DWORD(THISCALL *)(void *)>(0x4e6c00)(&run->data));
        break;
    }
    case 1: {   // the level file read into a new layer, written again; then read back binary
        Layer *l = load_level_text(level);
        save_text(l, "loaded.txt");
        st_file("loaded.txt", true, "level read as text, written");
        Stream s;
        if (open_stream(s, "loaded.bin", true))
            ev('b', at<DWORD(THISCALL *)(void *, void *)>(0x4ebe90)(l, s.b) & 0xff);
        close_stream(s);
        Layer *m = static_cast<Layer *>(pool_alloc(0x1d0));
        at<void *(THISCALL *)(void *, void *)>(0x4e9e00)(m, *reinterpret_cast<void **>(world() + 0x2a8));
        if (open_stream(s, "loaded.bin", false)) {
            ev('B', at<DWORD(THISCALL *)(void *, void *)>(0x4ebeb0)(m, s.b) & 0xff);
            at<DWORD(THISCALL *)(void *)>(0x524340)(s.b);
        }
        close_stream(s);
        save_text(m, "reloaded.txt");
        st_file("reloaded.txt", true, "layer read back binary, written");
        // A layer read binary on its own keeps references that only a whole save game resolves: deleting
        // its parts ends the game in the original too, so it is left (its parts are registered nowhere:
        // the world's slots 46/47 are empty)
        delete_layer(l);
        break;
    }
    case 2: {   // the world's layer holder: text and binary, read back into new holders
        BYTE *holder = world() + 0x2b0;
        Stream s;
        if (open_stream(s, "holder.txt", true))
            at<DWORD(THISCALL *)(void *, void *)>(0x4168f0)(holder, s.b);
        close_stream(s);
        st_file("holder.txt", false, "layer holder as text");
        if (open_stream(s, "holder.bin", true))
            at<void(THISCALL *)(void *, void *)>(0x4f00e0)(holder, s.b);
        close_stream(s);
        st_file("holder.bin", false, "layer holder binary");
        for (int kind = 0; kind < 2; kind++) {
            LayerHolder *h = static_cast<LayerHolder *>(at<void *(__cdecl *)()>(0x4efa20)());
            if (open_stream(s, kind ? "holder.bin" : "holder.txt", false)) {
                if (kind)
                    ev('R', at<DWORD(THISCALL *)(void *, void *)>(0x4f0160)(h, s.b) & 0xff);
                else {
                    char line[256];
                    at<DWORD(THISCALL *)(void *, char *, DWORD)>(0x4a20a0)(s.b, line, sizeof line);
                    ev('r', at<DWORD(THISCALL *)(void *, void *)>(0x416af0)(h, s.b) & 0xff);
                }
                at<DWORD(THISCALL *)(void *)>(0x524340)(s.b);
            }
            close_stream(s);
            st(h->layers.count);
            Stream o;
            if (open_stream(o, "holder2.txt", true))
                at<DWORD(THISCALL *)(void *, void *)>(0x4168f0)(h, o.b);
            close_stream(o);
            st_file("holder2.txt", true, kind ? "holder read binary, as text" : "holder read as text, as text");
            for (DWORD i = 0; !kind && i < h->layers.count; i++)   // (layers read binary are left, see above)
                if (h->layers.data[i])
                    delete_layer(static_cast<Layer *>(h->layers.data[i]));
            vcall<void *>(h, 1, 1);
        }
        break;
    }
    default: {   // the .way of a loaded layer in the temp folder
        Layer *l = load_level_text(level);
        char name[MAX_PATH];
        wsprintfA(name, "../%s/layer", g_temp_rel);
        zs_assign(&l->data.partial_name, name);
        delete_file("layer.way");
        at<void(THISCALL *)(void *)>(0x4ec520)(l);
        st_file("layer.way", false, ".way written (SaveWay)");
        at<void(THISCALL *)(void *)>(0x4eca80)(l);   // the file is there: loaded
        st_file("layer.way", false, ".way after loading");
        delete_file("layer.way");
        at<void(THISCALL *)(void *)>(0x4eca80)(l);   // missing: calculated and written
        st_file("layer.way", false, ".way calculated (missing)");
        static const BYTE GARBAGE[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        write_file("layer.way", GARBAGE, sizeof GARBAGE);
        at<void(THISCALL *)(void *)>(0x4eca80)(l);   // unreadable: recalculated and rewritten
        st_file("layer.way", false, ".way recalculated (unreadable)");
        delete_file("layer.way");
        delete_layer(l);
        break;
    }
    }
    restore_entry_flags();
    *at<DWORD *>(SOUND_LAYER) = saved_sound;
    return 0;
}

}  // namespace

ZAX_CHECKS(layer)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally objects{"layer: Create, CreateArray, constructors, deleting destructors"},
        classes{"layer: class getters and RegisterFields"}, registered{"layer: RegisterFields against the registered fields (real data)"},
        small{"layer: rectangles, arrays, view rectangles, unique id, playing time"},
        methods{"layer: save data methods on fresh layers"}, parts{"layer: parts in fresh layers"},
        real{"layer: level files, layer holder and .way (real data)"};
    save_bytes(g_generator, 0x7124c0, GAME_RANDOM_BYTES);
    for (unsigned seed = 1; seed <= (unsigned)scaled(120); seed++) {
        two_sides(objects, "zax save data", seed, [](DWORD &mask) { return zax_case(mask); });
        two_sides(objects, "holder", seed, [](DWORD &mask) { return holder_case(mask); });
    }
    for (unsigned seed = 1; seed <= (unsigned)scaled(24); seed++) {
        two_sides(objects, "save data", seed, [](DWORD &mask) { return big_case(false, mask); });
        two_sides(objects, "layer", seed, [](DWORD &mask) { return big_case(true, mask); });
    }
    objects.report();
    test_classes(classes, registered);
    classes.report();
    registered.report();
    for (unsigned seed = 1; seed <= (unsigned)scaled(1500); seed++)
        two_sides(small, "small", seed, [](DWORD &mask) { return small_case(mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(200); seed++)
        two_sides(small, "holder small", seed, [](DWORD &mask) { return holder_small_case(mask); });
    restore_bytes(g_generator);
    small.report();
    g_trace = false;   // true: every case logged before it runs (the last line names a case that ends the game)
    for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++)
        two_sides(methods, "methods", seed, [](DWORD &mask) { return methods_case(mask); });
    methods.report();
    for (unsigned seed = 1; seed <= (unsigned)scaled(40); seed++)
        two_sides(parts, "parts", seed, [](DWORD &mask) { return parts_case(mask); });
    parts.report();
    if (!running_layer())
        log("  layer: no running layer found, real data skipped");
    else if (!make_temp_paths())
        log("  layer: no temp folder reachable from the data folder, real data skipped");
    else {
        log("  layer: files in %s (from the data folder: %s)", g_temp, g_temp_rel);
        for (int which = 0; which < 4; which++)
            two_sides(real, "real", (unsigned)which + 1, [which](DWORD &mask) { return real_case(which, mask); });
        static const char *const FILES[] = {"run.txt", "run_w.txt", "run.bin", "loaded.txt", "loaded.bin", "reloaded.txt",
                                            "holder.txt", "holder.bin", "holder2.txt", "layer.way"};
        for (const char *f : FILES)
            delete_file(f);
        if (!RemoveDirectoryA(g_temp))
            log("  could not remove %s", g_temp);
    }
    real.report();
    log("  layer checks took %u ms", GetTickCount() - start);
}

}  // namespace zax
