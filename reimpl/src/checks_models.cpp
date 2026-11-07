// Checks of models.cpp (CModel, the model list, the entity made from a model, CSpawnableModel, the map
// caching table, COtherMapList) against the original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out, side 1
// through the reimplementation. Compared: eax (masked where the original's is not defined), an event log
// of the fakes and the state afterwards. Objects are compared dword by dword: equal raw values are equal,
// pointers into known blocks by their offset, other heap pointers by the first dword of what they point
// to.
//  - Objects: Create, CreateArray 0-3 (only the dwords the constructor sets: the rest keeps the heap's
//    bytes), the deleting destructors (single and array, freeing or not) and
//    the constructor (on two blocks with different patterns) of CModel (with fake sequences, names, path,
//    default entity and file buffer), CSpawnableModel and CSpawnableItem's destructor, the mapping (with
//    names and temp files; remove() recorded), the mapping table (with mappings, the busy flags) and
//    COtherMapList (with fake actions); CModel::Clear.
//  - Classes: the 6 class getters (ClassInfo_Register replaced), RegisterFields of the five classes into
//    fake class infos, compared per field kind and against the descriptors registered at startup; the
//    five startup registrations (ClassInfo_StaticRegister replaced).
//  - CModel's accessors on fake models: sequence names, the search by name and by object, the collection
//    by prefix, the default sequence, the references between sequences, the short name, the activities,
//    the turn speed in radians (all 80 bits), the size, Precache.
//  - The model list: the static initializer and the exit destructor on the global (saved and restored,
//    atexit replaced), the deleting destructor and FileOfObject on fake lists, the preview factory.
//  - The map caching table with a fake world (find layer, current layer, unload, save, file name), error
//    reporter, application and temp file names: random sequences of LoadPath, SavePath, Expire, Flush,
//    FlushAll, IsExpired, Touch, Forget, Unexpire, RestartMission, Update, Clear and LevelPath (avoiding
//    the paths that end the game).
//  - COtherMapList: RunFor, CountFor, RemoveFor and Clear on lists of fake actions.
//  - Real data: models of the running game's model list loaded from their files (Load; the model then
//    written with Object_WriteBinary into a temp file and the bytes compared, its size and sequences),
//    one read back through the base list's CreateFromFile; the file names for every colour depth and the
//    extension; entities made from the loaded models (Entity_CreateFromModel with CEntity and
//    CEntityAnimated, CSpawnableModel's slot 23) compared field by field, their children synchronized
//    (0x517e70) after changing the parent's angles.
#include "check_util.h"
#include "models.h"

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

int hooks_models(Hook *out);

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
constexpr int SLOG = 4096;
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
    const int n = hooks_models(nullptr);
    if (!hook_sites_fit("models", n, MAX_SITES))
        return;
    hooks_models(list);
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
    if (ptrv(p) < 0x10000 || ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000)
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0xee))
            return false;
    return true;
}
bool heap_pointer(DWORD p) { return p >= 0x10000 && !(p & 3) && (p < 0x400000 || p >= IMAGE_END) && p < 0x80000000u; }

// ---- Fakes ---------------------------------------------------------------------------------------------

// Fake objects: slot 1 records its deletion, 2 gives a class (by the id: the CModel class for odd ids,
// CObject's for even ones), 7 records, 16/17 record a text (error reporter), 21 a size, 23 records the
// four arguments (actions), 27 records (error reporter), 39 records (lists), 109/110 record a text
// (application)
constexpr int NFAKE = 24;
struct FakeObj {
    DWORD vftable;
    DWORD dynamic;
    DWORD id;
    DWORD calls;
};
FakeObj g_fakes[NFAKE];
DWORD g_fake_vt[120];
unsigned text_hash(const char *s) { return s ? fnv_text(2166136261u, s) : 0; }
FakeObj *THISCALL fake_delete(FakeObj *f, DWORD flags)
{
    ev('D', f->id, flags);
    return f;
}
CClassInfo *THISCALL fake_class(FakeObj *f)
{
    return f->id & 1 ? *at<CClassInfo **>(CLASS_MODEL) : at<CClassInfo *(__cdecl *)()>(0x4163b0)();
}
DWORD THISCALL fake_slot7(FakeObj *f)
{
    ev('7', f->id);
    return 7;
}
void THISCALL fake_slot16(FakeObj *f, const char *s) { ev('E', f->id, 16, text_hash(s)); }
void THISCALL fake_slot17(FakeObj *f, const char *s) { ev('E', f->id, 17, text_hash(s)); }
DWORD THISCALL fake_slot21(FakeObj *f)
{
    ev('2', f->id);
    return f->id * 100 + 7;
}
DWORD THISCALL fake_slot23(FakeObj *f, DWORD a, DWORD b, DWORD c, DWORD d)
{
    ev('F', f->id, a, b ^ c ^ d);
    return 0;
}
DWORD THISCALL fake_slot27(FakeObj *f, DWORD a, DWORD b)
{
    ev('R', f->id, a, b);
    return 0;
}
DWORD THISCALL fake_slot39(FakeObj *f, DWORD i, ZString *out)
{
    ev('9', ptrv(f) & 0xfff, i, ptrv(out) ? 1 : 0);
    return 0x3900 + i;
}
void THISCALL fake_slot109(FakeObj *f, const char *s) { ev('A', f->id, 109, text_hash(s)); }
void THISCALL fake_slot110(FakeObj *f, const char *s) { ev('A', f->id, 110, text_hash(s)); }
void init_fakes()
{
    for (DWORD &s : g_fake_vt)
        s = 0;
    g_fake_vt[1] = ptrv((void *)&fake_delete);
    g_fake_vt[2] = ptrv((void *)&fake_class);
    g_fake_vt[7] = ptrv((void *)&fake_slot7);
    g_fake_vt[16] = ptrv((void *)&fake_slot16);
    g_fake_vt[17] = ptrv((void *)&fake_slot17);
    g_fake_vt[21] = ptrv((void *)&fake_slot21);
    g_fake_vt[23] = ptrv((void *)&fake_slot23);
    g_fake_vt[27] = ptrv((void *)&fake_slot27);
    g_fake_vt[39] = ptrv((void *)&fake_slot39);
    g_fake_vt[109] = ptrv((void *)&fake_slot109);
    g_fake_vt[110] = ptrv((void *)&fake_slot110);
    for (int i = 0; i < NFAKE; i++)
        g_fakes[i] = {ptrv(g_fake_vt), 0, (DWORD)i + 1, 0};
}

// Fake sequences (CRotationSequence stand-ins): +8 no frames (0x56bb40 returns at once), +0x1c the flags,
// +0x50 the sequence they refer to; slots 1, 7, 21 as the fake objects (the id at +0x10)
constexpr int NSEQ = 12;
struct FakeSeq {
    DWORD vftable;
    DWORD dynamic;
    DWORD frames;      // 0x08
    DWORD u0c;
    DWORD id;          // 0x10
    DWORD u14[2];
    DWORD flags;       // 0x1c
    DWORD u20[12];
    DWORD next;        // 0x50
    DWORD u54[6];
};
static_assert(offsetof(FakeSeq, next) == 0x50);
FakeSeq g_seqs[NSEQ];
DWORD g_seq_vt[24];
FakeSeq *THISCALL seq_delete(FakeSeq *s, DWORD flags)
{
    ev('d', s->id, flags);
    return s;
}
DWORD THISCALL seq_slot7(FakeSeq *s)
{
    ev('s', s->id);
    return 0;
}
DWORD THISCALL seq_slot21(FakeSeq *s)
{
    ev('z', s->id);
    return 0x40 + s->id;
}
void init_seqs()
{
    for (DWORD &s : g_seq_vt)
        s = 0;
    g_seq_vt[1] = ptrv((void *)&seq_delete);
    g_seq_vt[7] = ptrv((void *)&seq_slot7);
    g_seq_vt[21] = ptrv((void *)&seq_slot21);
    for (int i = 0; i < NSEQ; i++) {
        memset(&g_seqs[i], 0, sizeof g_seqs[i]);
        g_seqs[i].vftable = ptrv(g_seq_vt);
        g_seqs[i].id = (DWORD)i + 1;
    }
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
    for (int i = 0; i < NSEQ; i++)
        if (v >= ptrv(&g_seqs[i]) && v < ptrv(&g_seqs[i]) + sizeof(FakeSeq))
            return 0xf5000000u + (DWORD)i * 0x100 + (v - ptrv(&g_seqs[i]));
    for (int i = g_nknown[g_side] - 1; i >= 0; i--)
        if (v >= g_known[g_side][i].at_ && v < g_known[g_side][i].at_ + g_known[g_side][i].size)
            return 0xb0000000u + (DWORD)i * 0x10000 + (v - g_known[g_side][i].at_);
    return v;
}

// An object's dwords: raw, or relative to the known blocks, or (other heap pointers) the first dword
// of what they point to when that looks like a vtable
DWORD mapped(DWORD v)
{
    const DWORD r = rel(v);
    if (r == v && heap_pointer(v)) {
        const DWORD first = readable(reinterpret_cast<void *>(v), 4) ? *reinterpret_cast<const DWORD *>(v) : 0;
        return 0xc0000000u | (first >= 0x5e0000 && first < 0x610000 ? first : 0);
    }
    return r;
}
void st_obj(const void *o, DWORD size)
{
    const DWORD *d = static_cast<const DWORD *>(o);
    for (DWORD i = 0; i < size / 4; i++)
        st(mapped(d[i]));
}
void st_text(const char *s) { st(text_hash(s ? s : "(null)")); }
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
        init_seqs();
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
    t.check(same, "models", d);
    return same;
}

void zs_assign(ZString *z, const char *s) { at<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
void zs_ctor(ZString *z) { at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
void zs_dtor(ZString *z) { at<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }

// A pool block of the size filled with a pattern and freed, so the next allocation of that size (most
// likely the same block) starts with the same bytes on both sides
void prime(DWORD size)
{
    void *p = pool_alloc(size);
    memset(p, 0xa5, size);
    pool_free(p, size);
}

char *heap_text(const char *s)
{
    const int n = lstrlenA(s);
    char *p = static_cast<char *>(heap_alloc((DWORD)n + 1));
    memcpy(p, s, (DWORD)n + 1);
    return p;
}

// remove() of the C runtime (0x5d17a6), the temp file name (0x4a3d30) and ClassInfo_StaticRegister
// (0x417690) replaced by recorders while a case runs
int __cdecl fake_remove(const char *p)
{
    ev('X', text_hash(p));
    return 0;
}
DWORD g_temp_counter;
void __fastcall fake_temp_name(char *out, const char *prefix)
{
    wsprintfA(out, "zaxtmp%u.tmp", ++g_temp_counter);
    ev('T', text_hash(prefix) ? 1 : 0, g_temp_counter);
}
DWORD THISCALL fake_static_register(DWORD self, DWORD getter)
{
    ev('G', self, getter);
    return self;
}
struct Patches {
    CodePatch p[8];
    int n = 0;
    void add(DWORD address, void *target) { patch_jump(p[n++], address, target); }
    ~Patches()
    {
        while (n)
            unpatch(p[--n]);
    }
};

// ---- Objects -------------------------------------------------------------------------------------------

const char *const NAMES[] = {"Idle", "Walk", "Walk Shadow", "Run", "idle", "Idle Shadow", "Shoot1", "Death", "Run Energy", "W"};
constexpr int NNAMES = sizeof NAMES / sizeof NAMES[0];
const char *const PATHS[] = {"Models/Characters/Crater/-Crater", "abc-", "-", "", "no dash", "a-b-c", "Models/x/-y z"};

// A sequence's reference: a later one, the end or beyond (so the references never form a cycle)
DWORD rand_next(DWORD i, DWORD n) { return random() % 4 ? i + 1 + random() % (n - i + 1) : random() | 0x10000; }

// A model's sequences, names, path, activities filled with fakes (all blocks known)
void fill_model(CModel *m, DWORD n, bool entity, bool buffer)
{
    m->num_sequences = n;
    if (n) {
        m->sequences = static_cast<BYTE **>(heap_alloc(n * 4));
        know(m->sequences, n * 4);
        DWORD *block = static_cast<DWORD *>(heap_alloc(n * 4 + 4));
        block[0] = n;
        m->sequence_names = reinterpret_cast<ZString *>(block + 1);
        know(block, n * 4 + 4);
        for (DWORD i = 0; i < n; i++) {
            m->sequences[i] = random() % 5 ? reinterpret_cast<BYTE *>(&g_seqs[i % NSEQ]) : nullptr;
            g_seqs[i % NSEQ].flags = random() % 3 ? 0x200u * (random() % 2) : random();
            g_seqs[i % NSEQ].next = rand_next(i, n);
            zs_ctor(&m->sequence_names[i]);
            zs_assign(&m->sequence_names[i], NAMES[random() % NNAMES]);
        }
    }
    m->path = heap_text(PATHS[random() % 7]);
    know(m->path, (DWORD)lstrlenA(m->path) + 1);
    zs_assign(&m->default_sequence, NAMES[random() % NNAMES]);
    if (entity)
        m->default_entity = &g_fakes[random() % 8];
    if (buffer) {
        m->file_buffer = heap_alloc(0x40);
        know(m->file_buffer, 0x40);
    }
    for (DWORD i = random() % 4; i; i--) {
        void *p = &g_fakes[8 + i];
        vcall<DWORD>(&m->activity, 2, &p);
    }
    know(m->activity.data, m->activity.capacity * 4);
}

// What the fakes leave behind freed by the test itself, so the model's own destructor does not meet them
void unfill_model(CModel *m)
{
    if (m->sequences)
        heap_free(m->sequences);
    m->sequences = nullptr;
    if (m->sequence_names) {
        DWORD *block = reinterpret_cast<DWORD *>(m->sequence_names) - 1;
        for (DWORD i = 0; i < block[0]; i++)
            zs_dtor(&m->sequence_names[i]);
        heap_free(block);
        m->sequence_names = nullptr;
    }
    m->num_sequences = 0;
    if (m->path)
        heap_free(m->path);
    m->path = nullptr;
    m->default_entity = nullptr;
    if (m->file_buffer)
        heap_free(m->file_buffer);
    m->file_buffer = nullptr;
    if (m->activity.data)
        heap_free(m->activity.data);
    m->activity = {m->activity.vftable, nullptr, 0, 0};
}

CModel *new_model()
{
    prime(sizeof(CModel));
    auto *m = static_cast<CModel *>(at<void *(__cdecl *)()>(0x5158c0)());
    know(m, sizeof(CModel));
    return m;
}
void delete_model(CModel *m) { vcall<void *>(m, 1, 1); }

// A model fresh from CreateArray (an unprimed heap block): the constructor leaves 0x20-0x40, 0x50-0x64,
// 0xa4-0xac and the bytes behind 0xa0 as the block had them (slot 6 of those fields does nothing, their
// defaults come with slot 19), so only the dwords it sets
void st_constructed(const BYTE *m)
{
    for (DWORD off = 0; off < sizeof(CModel); off += 4) {
        if ((off >= 0x20 && off <= 0x40) || (off >= 0x50 && off <= 0x64) || (off >= 0xa4 && off <= 0xac))
            continue;
        const DWORD v = *reinterpret_cast<const DWORD *>(m + off);
        st(off == 0xa0 ? v & 0xff : mapped(v));
    }
}

DWORD model_object_case(DWORD &mask)
{
    (void)mask;
    const DWORD op = random() % 6;
    DWORD r = 0;
    if (op <= 3) {
        CModel *m = new_model();
        st_obj(m, sizeof(CModel));
        st_zs(&m->default_sequence);
        fill_model(m, random() % 5, random() % 2, random() % 2);
        if (op == 0) {
            at<void(THISCALL *)(void *)>(0x516b60)(m);
            st_obj(m, sizeof(CModel));
            unfill_model(m);
            delete_model(m);
        } else {
            const DWORD flags = op == 1 ? 1 : op == 2 ? 0 : random() % 2;
            r = rel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x515930)(m, flags)));
            if (!flags) {
                st(*reinterpret_cast<DWORD *>(m));
                st(*reinterpret_cast<DWORD *>(reinterpret_cast<BYTE *>(m) + 0xb4));
                pool_free(m, sizeof(CModel));
            }
        }
    } else if (op == 4) {
        // The constructor on two blocks with different patterns: the dwords that come out equal
        BYTE *o = static_cast<BYTE *>(pool_alloc(sizeof(CModel)));
        BYTE *b = static_cast<BYTE *>(pool_alloc(sizeof(CModel)));
        memset(o, 0xa5, sizeof(CModel));
        memset(b, 0x5a, sizeof(CModel));
        know(o, sizeof(CModel));
        know(b, sizeof(CModel));
        r = rel(ptrv(at<void *(THISCALL *)(void *)>(0x516a90)(o)));
        at<void *(THISCALL *)(void *)>(0x516a90)(b);
        for (DWORD off = 0; off < sizeof(CModel); off += 4) {
            DWORD x = rel(*reinterpret_cast<DWORD *>(o + off)), y = rel(*reinterpret_cast<DWORD *>(b + off));
            if ((x >> 28) == 0xb && (y >> 28) == 0xb)   // into the own block: the offset
                x &= 0xffff, y &= 0xffff;
            st(x == y ? x : 0xdddddddd);
        }
        st_zs(&reinterpret_cast<CModel *>(o)->default_sequence);
        delete_model(reinterpret_cast<CModel *>(o));
        delete_model(reinterpret_cast<CModel *>(b));
    } else {
        const DWORD n = random() % 4;
        BYTE *a = at<BYTE *(__fastcall *)(DWORD, DWORD, DWORD)>(0x5158e0)(n, 0, 0);
        know(a - 4, n * sizeof(CModel) + 4);
        for (DWORD i = 0; i < n; i++)
            st_constructed(a + i * sizeof(CModel));
        const DWORD flags = 2 | (random() % 2);
        r = rel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x515930)(a, flags)));
        if (!(flags & 1))
            heap_free(a - 4);
    }
    return r;
}

// CSpawnableModel, CSpawnableItem's destructor, the mapping, the table, COtherMapList
struct SmallClass {
    DWORD create, create_array, dtor, size;
};
constexpr SmallClass SMALL[] = {{0x4cf000, 0x4cf040, 0x4cf0a0, 0x10},
                                {0x5888b0, 0x5888e0, 0x588940, 0x18},
                                {0x588ce0, 0x588d60, 0x588e10, 0x50},
                                {0x41f290, 0x41f2e0, 0x41f360, 0x18}};

void fill_small(int cls, BYTE *o)
{
    switch (cls) {
    case 0: reinterpret_cast<CSpawnableModel *>(o)->model = random() % 4; break;
    case 1: {
        auto *m = reinterpret_cast<CSwappedLayerFilenameMapping *>(o);
        if (random() % 2) {
            m->name = static_cast<char *>(heap_alloc(0x100));
            lstrcpyA(m->name, NAMES[random() % NNAMES]);
        }
        if (random() % 2)
            m->current_temp = heap_text("c:\\temp\\zax1.tmp");
        if (random() % 2)
            m->mission_temp = heap_text("c:\\temp\\zax2.tmp");
        m->expired = random() % 2;
        break;
    }
    case 2: {
        auto *t = reinterpret_cast<CSwappedLayerFilenameMappingTable *>(o);
        const DWORD n = random() % 3;
        for (DWORD i = 0; i < n; i++) {
            BYTE *m = at<BYTE *(__cdecl *)()>(0x5888b0)();
            fill_small(1, m);
            void *p = m;
            vcall<DWORD>(&t->mappings, 2, &p);
        }
        if (random() % 2)
            t->marker_temp = heap_text("c:\\temp\\marker.tmp");
        for (ModelArray *a : (ModelArray *const[3]){&t->unload, &t->flush, &t->lru})
            for (DWORD i = random() % 3; i; i--) {
                void *p = &g_fakes[random() % 8];
                vcall<DWORD>(a, 2, &p);
            }
        t->busy = random() % 4 == 0;
        t->mission_pending = random() % 4 == 0;
        break;
    }
    case 3: {
        auto *l = reinterpret_cast<COtherMapList *>(o);
        for (DWORD i = random() % 4; i; i--) {
            void *p = &g_fakes[random() % 8];
            vcall<DWORD>(&l->actions, 2, &p);
        }
        break;
    }
    }
}

DWORD small_object_case(DWORD &mask)
{
    (void)mask;
    Patches patches;
    patches.add(0x5d17a6, (void *)&fake_remove);
    const int cls = (int)(random() % 5);
    const DWORD op = random() % 3;
    DWORD r = 0;
    if (cls == 4) {   // CSpawnableItem's destructor on a block built like its part of the constructor
        prime(0xc);
        BYTE *o = static_cast<BYTE *>(pool_alloc(0xc));
        at<void *(THISCALL *)(void *)>(0x416610)(o);
        *reinterpret_cast<DWORD *>(o) = 0x5f76cc;
        at<void(THISCALL *)(void *)>(0x4173c0)(o);
        know(o, 0xc);
        st_obj(o, 0xc);
        const DWORD flags = random() % 2;
        r = rel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x4cefd0)(o, flags)));
        if (!flags) {
            st(*reinterpret_cast<DWORD *>(o));
            pool_free(o, 0xc);
        }
        return r;
    }
    const SmallClass &c = SMALL[cls];
    if (op < 2) {
        prime(c.size);
        BYTE *o = at<BYTE *(__cdecl *)()>(c.create)();
        know(o, c.size);
        st_obj(o, c.size);
        fill_small(cls, o);
        const DWORD flags = op;
        r = rel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(c.dtor)(o, flags)));
        if (!flags) {
            st(*reinterpret_cast<DWORD *>(o));
            pool_free(o, c.size);
        }
    } else {
        const DWORD n = random() % 4;
        BYTE *a = at<BYTE *(__fastcall *)(DWORD, DWORD, DWORD)>(c.create_array)(n, 0, 0);
        know(a - 4, n * c.size + 4);
        for (DWORD i = 0; i < n; i++) {
            st_obj(a + i * c.size, c.size);
            fill_small(cls, a + i * c.size);
        }
        const DWORD flags = 2 | (random() % 2);
        r = rel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(c.dtor)(a, flags)));
        if (!(flags & 1))
            heap_free(a - 4);
    }
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
constexpr GetterDef GETTERS[] = {{0x515870, CLASS_MODEL}, {0x4cef30, CLASS_SPAWNABLE_MODEL}, {0x588860, CLASS_MAPPING},
                                 {0x588c90, CLASS_MAPPING_TABLE}, {0x41f4f0, CLASS_OTHER_MAP_LIST}, {0x41f240, CLASS_OTHER_MAP_LIST}};
struct RegFields {
    const char *name;
    DWORD address, var;
};
constexpr RegFields REGISTER_FIELDS[] = {{"CModel", 0x5159b0, CLASS_MODEL},
                                         {"CSpawnableModel", 0x4cf150, CLASS_SPAWNABLE_MODEL},
                                         {"CSwappedLayerFilenameMapping", 0x588a20, CLASS_MAPPING},
                                         {"CSwappedLayerFilenameMappingTable", 0x589030, CLASS_MAPPING_TABLE},
                                         {"COtherMapList", 0x41f550, CLASS_OTHER_MAP_LIST}};
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
                "models", d);
    }
    for (const RegFields &rf : REGISTER_FIELDS)
        for (int var_zero = 0; var_zero < 2; var_zero++) {
            static CFieldDesc *fields[2][MAXF];
            DWORD n[2], result[2], alt[2];
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
                if (side == 1 && hook_sites_fit("models", hooks_models(nullptr), MAX_SITES)) {
                    Hook list[MAX_SITES];
                    const int nh = hooks_models(list);
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
                alt[side] = ptrv(c->alt_name);
                if (c->fields)
                    heap_free(c->fields);
                if (var_zero)
                    pool_free(c, sizeof(CClassInfo));
                var = saved;
                if (side == 0)
                    hooks_in();
            }
            bool same = n[0] == n[1] && n[0] > 0 && result[0] == result[1] && alt[0] == alt[1];
            DWORD first_diff = 0xffffffff;
            for (DWORD i = 0; i < n[0] && same; i++)
                if (!(same = same_desc(reinterpret_cast<const BYTE *>(fields[0][i]), reinterpret_cast<const BYTE *>(fields[1][i]))))
                    first_diff = i;
            char d[180];
            wsprintfA(d, "[RegisterFields %s var %d: %u / %u fields, eax %x / %x, alt %x / %x, first difference %d]", rf.name, var_zero,
                      n[0], n[1], result[0], result[1], alt[0], alt[1], first_diff);
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

DWORD static_register_case(DWORD &mask)
{
    (void)mask;
    static constexpr DWORD STUBS[] = {0x5159a0, 0x4cf140, 0x588a10, 0x589020, 0x41f540};
    Patches patches;
    patches.add(0x417690, (void *)&fake_static_register);
    DWORD r = 0;
    for (DWORD s : STUBS)
        r = fnv(r, &(s = at<DWORD(__cdecl *)()>(s)()), 4);
    return r;
}

// ---- CModel's accessors ------------------------------------------------------------------------------------

// The int array of CollectSequences: slot 2 records the index
DWORD g_int_vt[3];
struct FakeIntArray {
    DWORD vftable;
    void *data;
    DWORD count, capacity;
};
DWORD THISCALL int_add(FakeIntArray *a, const DWORD *v)
{
    ev('+', *v);
    return a->count++;
}

void THISCALL fake_object_precache(void *o) { ev('P', rel(ptrv(o))); }

long double call_turn_speed(CModel *m)
{
    return at<long double(THISCALL *)(CModel *)>(0x517510)(m);
}

DWORD accessor_case(DWORD &mask)
{
    CModel *m = new_model();
    const DWORD n = 1 + random() % 6;
    fill_model(m, n, false, false);
    for (DWORD i = 0; i < n; i++)   // the sequences all present (the original dereferences them)
        if (!m->sequences[i])
            m->sequences[i] = reinterpret_cast<BYTE *>(&g_seqs[n + i]);   // (unused by the others: no cycle)
    DWORD r = 0;
    for (int k = 0; k < 24; k++) {
        const DWORD op = random() % 15;
        const DWORD i = random() % 3 ? random() % (n + 1) : random();
        const char *name = NAMES[random() % NNAMES];
        DWORD v = 0;
        if (g_trace)
            log("      op %u i %u", op, i);
        switch (op) {
        case 0: v = text_hash(at<const char *(THISCALL *)(CModel *, DWORD)>(0x517200)(m, i)); break;
        case 1: v = rel(ptrv(at<void *(THISCALL *)(CModel *, DWORD)>(0x517220)(m, i))); break;
        case 2: v = at<DWORD(THISCALL *)(CModel *, const char *)>(0x517240)(m, name); break;
        case 3: v = at<DWORD(THISCALL *)(CModel *, const void *)>(0x517280)(m, random() % 2 ? (void *)m->sequences[i % n] : (void *)&g_seqs[random() % NSEQ]); break;
        case 4: {
            g_int_vt[2] = ptrv((void *)&int_add);
            FakeIntArray a = {ptrv(g_int_vt), random() % 2 ? heap_alloc(16) : nullptr, random() % 3, 4};
            const char *prefix = random() % 3 ? NAMES[random() % NNAMES] : "";
            for (DWORD s = 0; s < n; s++)
                if (random() % 2)
                    g_seqs[s % NSEQ].next = rand_next(s, n);
            at<DWORD(THISCALL *)(CModel *, void *, const char *, DWORD)>(0x5172b0)(m, &a, prefix, random() % 2);
            v = a.count * 16 + (a.data ? 1 : 0) + a.capacity * 256;
            break;
        }
        case 5: v = rel(ptrv(at<void *(THISCALL *)(CModel *)>(0x517360)(m))); break;
        case 6: v = at<DWORD(THISCALL *)(CModel *)>(0x5173b0)(m); break;
        case 7: v = rel(ptrv(at<void *(THISCALL *)(CModel *, DWORD)>(0x5173f0)(m, i))); break;
        case 8: v = rel(ptrv(at<void *(THISCALL *)(CModel *, const void *, DWORD)>(0x517410)(m, &g_seqs[random() % NSEQ], random() % 2))); break;
        case 9: v = rel(ptrv(at<void *(THISCALL *)(CModel *, DWORD, DWORD)>(0x517460)(m, i % n, random() % 2))); break;
        case 10: {
            heap_free(m->path);
            m->path = heap_text(PATHS[random() % 7]);
            know(m->path, (DWORD)lstrlenA(m->path) + 1);
            const char *s = at<const char *(THISCALL *)(CModel *)>(0x5174a0)(m);
            v = s ? (DWORD)(s - m->path) + 0x1000 : 0;
            break;
        }
        case 11: v = rel(ptrv(at<void *(THISCALL *)(CModel *, DWORD)>(0x5174f0)(m, random() % 5))); break;
        case 12: {
            DWORD bitsv = random();
            if (random() % 3 == 0)
                bitsv = (bitsv & 0x807fffffu) | (random() % 2 ? 0x7f800000u : 0);
            memcpy(&m->turn_speed, &bitsv, 4);
            const long double x = call_turn_speed(m);
            BYTE b[10];
            memcpy(b, &x, 10);
            v = fnv(2166136261u, b, 10);
            break;
        }
        case 13: v = at<DWORD(THISCALL *)(CModel *)>(0x516f80)(m); break;
        default: {
            // CObject_Precache replaced by a recorder (on a model of fakes it ends the game in the original
            // too); the real one runs in the real-data checks
            Patches patches;
            patches.add(0x4167b0, (void *)&fake_object_precache);
            at<void(THISCALL *)(CModel *)>(0x516cd0)(m);
            break;
        }
        }
        st(op);
        st(v);
        r = fnv(r, &v, 4);
    }
    mask = 0xffffffff;
    unfill_model(m);
    delete_model(m);
    return r;
}

// ---- The model list -----------------------------------------------------------------------------------------

DWORD g_list_vt[48];
alignas(4) BYTE g_list_mem[0x140];

// A list in the static initializer's state, plus records with fake objects and a monitor
void fake_list_state(BYTE *l, bool own_vtable)
{
    memset(l, 0x33, 0x140);
    at<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, 0, 0x80, 0, nullptr, 0);
    DWORD *d = reinterpret_cast<DWORD *>(l);
    d[7] = 0x5ed558;
    d[8] = d[9] = d[10] = 0;
    d[11] = 0xffffffff;
    d[12] = d[13] = 0;
    d[0x138 / 4] = 0;
    d[0x13c / 4] = random() % 2 ? ptrv(&g_fakes[20]) : 0;
    d[0] = own_vtable ? ptrv(g_list_vt) : MODEL_LIST_VTABLE;
    const DWORD n = random() % 4;
    if (n) {
        DWORD *block = static_cast<DWORD *>(heap_alloc(n * 0x80 + 4));
        block[0] = n;
        BYTE *e = reinterpret_cast<BYTE *>(block + 1);
        memset(e, 0, n * 0x80);
        know(block, n * 0x80 + 4);
        for (DWORD i = 0; i < n; i++) {
            wsprintfA(reinterpret_cast<char *>(e + i * 0x80), "entry %u", i);
            *reinterpret_cast<void **>(e + i * 0x80 + 0x78) = random() % 3 ? &g_fakes[1 + (random() % 8)] : nullptr;
        }
        d[12] = ptrv(e);
        d[13] = n;
    }
}

void *__fastcall fake_create_from_file(ZString *path, DWORD flags, DWORD write_back, void **out)
{
    ev('C', text_hash(path->c_str()), flags, write_back);
    if (out)
        *out = &g_fakes[5];
    return &g_fakes[5];
}

DWORD list_case(DWORD &mask)
{
    for (int i = 0; i < 48; i++)
        g_list_vt[i] = at<DWORD *>(MODEL_LIST_VTABLE)[i];
    g_list_vt[39] = ptrv((void *)&fake_slot39);
    const DWORD op = random() % 6;
    DWORD r = 0;
    switch (op) {
    case 0: {   // the static initializer on the global
        SavedBytes saved;
        save_bytes(saved, MODEL_LIST, 0x140);
        Patches patches;
        patches.add(0x5d0863, (void *)&fake_atexit);
        g_atexit_calls = g_atexit_function = 0;
        memset(at<void *>(MODEL_LIST), 0x77, 0x140);
        r = at<DWORD(__cdecl *)()>(0x517570)();
        st_obj(at<void *>(MODEL_LIST), 0x140);
        st(g_atexit_calls);
        st(g_atexit_function);
        restore_bytes(saved);
        break;
    }
    case 1: {   // the exit destructor on the global
        SavedBytes saved;
        save_bytes(saved, MODEL_LIST, 0x140);
        fake_list_state(at<BYTE *>(MODEL_LIST), false);
        r = at<DWORD(__cdecl *)()>(0x5175e0)();
        st_obj(at<void *>(MODEL_LIST), 0x140);
        restore_bytes(saved);
        mask = 0;
        break;
    }
    case 2: {   // the deleting destructor
        const bool pool = random() % 2;
        BYTE *l = pool ? static_cast<BYTE *>(pool_alloc(0x140)) : g_list_mem;
        know(l, 0x140);
        fake_list_state(l, false);
        r = rel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x517820)(l, pool ? 1 : 0)));
        if (!pool)
            st_obj(l, 0x140);
        break;
    }
    case 3: {   // FileOfObject
        fake_list_state(g_list_mem, true);
        know(g_list_mem, 0x140);
        for (int k = 0; k < 6; k++) {
            void *o = &g_fakes[random() % 10];
            ZString out;
            const DWORD v = at<DWORD(THISCALL *)(void *, void *, ZString *)>(0x517950)(g_list_mem, o, &out);
            st(v);
            r = fnv(r, &v, 4);
        }
        if (DWORD e = *reinterpret_cast<DWORD *>(g_list_mem + 0x30))
            heap_free(reinterpret_cast<void *>(e - 4));
        break;
    }
    case 4: {   // the base list's slot 37 with Object_CreateFromFile recorded (the path, flags, out)
        Patches patches;
        patches.add(0x4164c0, (void *)&fake_create_from_file);
        void *out = nullptr;
        static const char *const DIRS[] = {"Models", "", "Cache/Models/Characters", "a/b/"};
        r = rel(ptrv(at<void *(THISCALL *)(void *, const char *, const char *, void **)>(0x5178d0)(
            g_list_mem, DIRS[random() % 4], NAMES[random() % NNAMES], random() % 2 ? &out : nullptr)));
        st(rel(ptrv(out)));
        break;
    }
    default: r = at<DWORD(THISCALL *)(void *)>(0x5176e0)(at<void *>(MODEL_LIST)); break;
    }
    return r;
}

// ---- The map caching table ------------------------------------------------------------------------------------

// The fake world: eight layers (0x200 bytes; +0x3c the save data, whose +0x164 points back), named by
// the table NAMES; the current one chosen per case
constexpr int NLAYER = 6;
alignas(4) BYTE g_layers[NLAYER][0x200];
const char *const LAYER_NAMES[] = {"Act 1/A", "Act 1/B", "act 1/a.zax", "C", "D", "Act 1/B.zax", "E"};
constexpr int NLAYER_NAMES = sizeof LAYER_NAMES / sizeof LAYER_NAMES[0];
int g_current;
BYTE *THISCALL fake_find_layer(void *, const char *name)
{
    for (int i = 0; i < NLAYER; i++)
        if (crt_stricmp(name, LAYER_NAMES[i]) == 0)
            return g_layers[i];
    return nullptr;
}
BYTE *THISCALL fake_current_layer(void *) { return g_current >= 0 ? g_layers[g_current] : nullptr; }
void THISCALL fake_world_unload(void *, BYTE *l)
{
    ev('U', rel(ptrv(l)));
}
void THISCALL fake_world_save(void *, BYTE *l, DWORD a, DWORD b) { ev('S', rel(ptrv(l)), a, b); }
const char *THISCALL fake_file_name(BYTE *l)
{
    for (int i = 0; i < NLAYER; i++)
        if (l == g_layers[i])
            return LAYER_NAMES[i];
    return "nowhere";
}
void THISCALL fake_call_flagged(void *) { ev('L'); }

DWORD layer_index(const void *p)
{
    for (int i = 0; i < NLAYER; i++) {
        if (p == g_layers[i])
            return 0x100 + (DWORD)i;
        if (p == g_layers[i] + 0x3c)
            return 0x200 + (DWORD)i;
    }
    return mapped(ptrv(p));
}

void st_table(CSwappedLayerFilenameMappingTable *t)
{
    st(t->mappings.count);
    for (DWORD i = 0; i < t->mappings.count; i++) {
        auto *m = static_cast<CSwappedLayerFilenameMapping *>(t->mappings.data[i]);
        st(m->vftable);
        st_text(m->name);
        st_text(m->current_temp);
        st_text(m->mission_temp);
        st(m->expired + 2 * m->expired_this_mission);
    }
    st(t->busy + 2 * t->mission_pending);
    st_text(t->marker_temp);
    for (ModelArray *a : (ModelArray *const[3]){&t->unload, &t->flush, &t->lru}) {
        st(a->vftable);
        st(a->count);
        for (DWORD i = 0; i < a->count; i++)
            st(layer_index(a->data[i]));
    }
}

bool lru_resolvable(CSwappedLayerFilenameMappingTable *t)
{
    for (DWORD i = 0; i < t->lru.count; i++) {
        BYTE *save = static_cast<BYTE *>(t->lru.data[i]);
        BYTE *layer = *reinterpret_cast<BYTE **>(save + 0x164);
        if (!fake_find_layer(nullptr, fake_file_name(layer)) || fake_find_layer(nullptr, fake_file_name(layer)) == fake_current_layer(nullptr))
            return false;
    }
    return true;
}

DWORD table_case(DWORD &mask)
{
    Patches patches;
    patches.add(0x5d17a6, (void *)&fake_remove);
    patches.add(0x4a3d30, (void *)&fake_temp_name);
    patches.add(0x4f4d70, (void *)&fake_find_layer);
    patches.add(0x4f1050, (void *)&fake_current_layer);
    patches.add(0x4f3490, (void *)&fake_world_unload);
    patches.add(0x4f4380, (void *)&fake_world_save);
    patches.add(0x4eb670, (void *)&fake_file_name);
    patches.add(0x4a3760, (void *)&fake_call_flagged);
    void *&reporter = *at<void **>(0x6c02cc);
    void *&application = *at<void **>(0x713f14);
    void *const saved_reporter = reporter, *const saved_app = application;
    reporter = &g_fakes[21];
    application = &g_fakes[22];
    g_temp_counter = 100;
    memset(g_layers, 0, sizeof g_layers);
    for (int i = 0; i < NLAYER; i++)
        *reinterpret_cast<BYTE **>(g_layers[i] + 0x3c + 0x164) = g_layers[i];
    g_current = (int)(random() % (NLAYER + 1)) - 1;
    prime(0x50);
    auto *t = static_cast<CSwappedLayerFilenameMappingTable *>(at<void *(__cdecl *)()>(0x588ce0)());
    know(t, 0x50);
    DWORD r = 0;
    char out[0x200];
    for (int k = 0; k < 30; k++) {
        const DWORD op = random() % 14;
        const char *name = LAYER_NAMES[random() % NLAYER_NAMES];
        DWORD v = 0;
        out[0] = 0;
        // The paths that end the game in the original are left out
        auto *m = [&]() -> CSwappedLayerFilenameMapping * {
            char key[0x100];
            lstrcpyA(key, name);
            for (DWORD i = 0; i < t->mappings.count; i++) {
                auto *x = static_cast<CSwappedLayerFilenameMapping *>(t->mappings.data[i]);
                if (crt_stricmp(x->name, key) == 0)
                    return x;
            }
            return nullptr;
        }();
        switch (op) {
        case 0:
            if (m && !m->expired && !m->current_temp && !m->mission_temp)
                break;
            v = at<DWORD(THISCALL *)(void *, const char *, char *)>(0x5892b0)(t, name, out) & 0xff;
            break;
        case 1:
            if (m && (m->expired || (!m->current_temp && !m->mission_temp)))
                break;
            v = at<DWORD(THISCALL *)(void *, const char *, char *)>(0x5893c0)(t, name, out) & 0xff;
            break;
        case 2: at<void(THISCALL *)(void *, const char *)>(0x589570)(t, name); break;
        case 3: at<void(THISCALL *)(void *, const char *)>(0x589790)(t, name); break;
        case 4:
            if (lru_resolvable(t))
                at<void(THISCALL *)(void *)>(0x589890)(t);
            break;
        case 5: v = at<DWORD(THISCALL *)(void *, const char *)>(0x5899b0)(t, name) & 0xff; break;
        case 6:
        case 7: at<void(THISCALL *)(void *, const char *)>(0x589a50)(t, name); break;
        case 8: at<void(THISCALL *)(void *, const char *)>(0x589af0)(t, name); break;
        case 9: at<void(THISCALL *)(void *, const char *)>(0x589b80)(t, name); break;
        case 10:
            if (random() % 2 && !t->marker_temp)
                t->marker_temp = heap_text("c:\\temp\\marker.tmp");
            at<void(THISCALL *)(void *)>(0x589cc0)(t);
            break;
        case 11:
            t->mission_pending = random() % 2;
            at<DWORD(THISCALL *)(void *)>(0x589db0)(t);
            break;
        case 12:
            if (random() % 4 == 0)
                at<void(THISCALL *)(void *)>(0x58a030)(t);
            break;
        default: v = at<DWORD(THISCALL *)(void *, const char *, char *)>(0x589270)(t, name, out) & 0xff; break;
        }
        st(op);
        st(v);
        st_text(out);
        r = fnv(r, &v, 4);
    }
    st_table(t);
    mask = 0xffffffff;
    t->busy = 0;
    t->mission_pending = 0;
    vcall<void *>(t, 1, 1);
    reporter = saved_reporter;
    application = saved_app;
    return r;
}

// ---- COtherMapList -----------------------------------------------------------------------------------------

// Fake COtherMapAction objects: the fake vtable, +8 the action (a fake), +0xc the map name
struct FakeOtherMapAction {
    DWORD vftable;
    DWORD dynamic;
    void *action;
    ZString map;
};
FakeOtherMapAction g_oma[10];
FakeOtherMapAction *THISCALL oma_delete(FakeOtherMapAction *a, DWORD flags)
{
    ev('o', (DWORD)(a - g_oma), flags);
    return a;
}
DWORD g_oma_vt[3];

DWORD other_map_case(DWORD &mask)
{
    (void)mask;
    g_oma_vt[1] = ptrv((void *)&oma_delete);
    for (int i = 0; i < 10; i++) {
        g_oma[i].vftable = ptrv(g_oma_vt);
        g_oma[i].action = &g_fakes[10 + i];
        zs_ctor(&g_oma[i].map);
        zs_assign(&g_oma[i].map, LAYER_NAMES[random() % NLAYER_NAMES]);
    }
    prime(0x18);
    auto *l = static_cast<COtherMapList *>(at<void *(__cdecl *)()>(0x41f290)());
    know(l, 0x18);
    for (DWORD i = random() % 8; i; i--) {
        void *p = &g_oma[random() % 10];
        vcall<DWORD>(&l->actions, 2, &p);
    }
    DWORD r = 0;
    for (int k = 0; k < 6; k++) {
        const DWORD op = random() % 4;
        const char *name = LAYER_NAMES[random() % NLAYER_NAMES];
        DWORD v = 0;
        switch (op) {
        case 0: at<void(THISCALL *)(void *, DWORD, const char *)>(0x41f6c0)(l, random() % 100, name); break;
        case 1: v = at<DWORD(THISCALL *)(void *, const char *)>(0x41f780)(l, name); break;
        case 2: at<void(THISCALL *)(void *, const char *)>(0x41f7d0)(l, name); break;
        default:
            if (random() % 3 == 0)
                at<void(THISCALL *)(void *)>(0x41f660)(l);
            break;
        }
        st(op);
        st(v);
        st(l->actions.count);
        for (DWORD i = 0; i < l->actions.count; i++)
            st((DWORD)(static_cast<FakeOtherMapAction *>(l->actions.data[i]) - g_oma));
        r = fnv(r, &v, 4);
    }
    l->actions.count = 0;
    vcall<void *>(l, 1, 1);
    for (int i = 0; i < 10; i++)
        zs_dtor(&g_oma[i].map);
    return r;
}

// ---- Real data ---------------------------------------------------------------------------------------------

char g_temp[MAX_PATH], g_temp_rel[MAX_PATH];

bool make_temp_paths()
{
    char tmp[MAX_PATH], data[MAX_PATH];
    g_temp[0] = g_temp_rel[0] = 0;
    if (!GetTempPathA(MAX_PATH - 40, tmp))
        return false;
    wsprintfA(g_temp, "%sZaxModels\\", tmp);
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
void game_path(char *out, const char *name) { wsprintfA(out, "%s\\%s", g_temp_rel, name); }

void st_file(const char *name)
{
    DWORD n;
    BYTE *p = read_file(name, &n);
    st(n);
    st(p ? fnv(2166136261u, p, n) : 0);
    if (p)
        heap_free(p);
}

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

// A model written binary (Object_WriteBinary: its fields, then slot 13) into a temp file and hashed
void st_model(CModel *m, const char *file)
{
    if (g_trace)
        log("      st_model %u sequences", m->num_sequences);
    st(m->num_sequences);
    for (DWORD i = 0; i < m->num_sequences; i++)
        st_zs(&m->sequence_names[i]);
    st_text(m->path);
    st(vcall<DWORD>(m, 21));
    st(m->default_entity ? ptrv(vcall<CClassInfo *>(m->default_entity, 2)) : 0);
    Stream s;
    if (open_stream(s, file, true))
        at<DWORD(THISCALL *)(void *, void *)>(0x416f50)(m, s.b);
    close_stream(s);
    st_file(file);
}

BYTE *list_entry(DWORD i) { return *at<BYTE **>(MODEL_LIST + 0x30) + i * 0x80; }
DWORD list_count() { return *at<BYTE **>(MODEL_LIST + 0x30) ? *at<DWORD *>(MODEL_LIST + 0x34) : 0; }
const char *list_folder() { return vcall<const char *>(at<void *>(MODEL_LIST), 5); }

// The models to load: every n-th record and the loaded ones (up to 24 each). Entities only from loaded models with
// sequences: a weapon's projectile model that the HUD detail checks' projectile values loaded (0x4d6300 through this
// list) has none and no default entity, and the original ends the game making a CEntityAnimated of it (run
// 20261101-134307-0467: "shot heavy barrel", 0 sequences; docs/DECOMPILATION.md 13, "Batch section-12 leftovers", 5b)
DWORD g_load_set[64], g_nload, g_entity_set[32], g_nentity;
void pick_models()
{
    const DWORD n = list_count();
    g_nload = g_nentity = 0;
    const DWORD step = n / 24 + 1;
    for (DWORD i = 0; i < n && g_nload < 24; i += step)
        g_load_set[g_nload++] = i;
    for (DWORD i = 0; i < n && g_nentity < 24; i++) {
        BYTE *e = list_entry(i);
        CModel *m = *reinterpret_cast<CModel **>(e + 0x78);
        if (m && !e[0x7f] && m->num_sequences) {
            g_entity_set[g_nentity++] = i;
            if (g_nload < 48)
                g_load_set[g_nload++] = i;
        }
    }
}

DWORD load_case(DWORD which, DWORD &mask)
{
    const char *name = reinterpret_cast<const char *>(list_entry(g_load_set[which]));
    if (g_trace)
        log("      load %u: '%s' from '%s'", g_load_set[which], name, list_folder());
    CModel *m = new_model();
    const DWORD r = at<DWORD(THISCALL *)(void *, const char *, const char *)>(0x516eb0)(m, list_folder(), name) & 0xff;
    st_model(m, "model.bin");
    vcall<void>(m, 7);   // Precache: the sequences' and CObject's
    st_model(m, "model.bin");
    mask = 0xff;
    if (which == 0) {
        // The same model loaded again through the list's slot 37
        void *out = nullptr;
        CModel *again = static_cast<CModel *>(
            at<void *(THISCALL *)(void *, const char *, const char *, void **)>(0x5176a0)(at<void *>(MODEL_LIST), list_folder(), name, &out));
        st(again == out);
        st_model(again, "model3.bin");
        delete_model(again);
    }
    delete_model(m);
    return r;
}

// The file names for the depths of a fake surface (+0xc) and of the display, the extension
DWORD file_name_case(DWORD &mask)
{
    (void)mask;
    static const int DEPTHS[] = {8, 16, 24, 32, 15, 0, -1};
    DWORD r = 0;
    for (int d : DEPTHS) {
        DWORD surface[4] = {0, 0, 0, (DWORD)d};
        ZString out;
        zs_ctor(&out);
        zs_assign(&out, "unchanged");
        at<DWORD(__fastcall *)(ZString *, const char *, const char *, void *)>(0x516da0)(&out, "Models", "Characters/Crater/Crater",
                                                                                       d < 0 ? nullptr : surface);
        st_zs(&out);
        zs_dtor(&out);
    }
    r = text_hash(at<const char *(__cdecl *)()>(0x516a20)());
    return r;
}

// An entity's dwords (the class's size, at most 0x400) and its children's
void st_entity(BYTE *e, int depth)
{
    CClassInfo *c = vcall<CClassInfo *>(e, 2);
    st(ptrv(c));
    const DWORD size = c->size < 0x400 ? c->size : 0x400;
    know(e, size);
    st_obj(e, size);
    BYTE *children = *reinterpret_cast<BYTE **>(e + 8);
    if (depth < 3 && children && readable(children, 0x14) && *reinterpret_cast<DWORD *>(children + 0x10) < 16)
        for (DWORD i = 0; i < *reinterpret_cast<DWORD *>(children + 0x10); i++)
            if (BYTE *ch = (*reinterpret_cast<BYTE ***>(children + 0xc))[i])
                st_entity(ch, depth + 1);
}

DWORD entity_case(DWORD which, DWORD &mask)
{
    const DWORD index = g_entity_set[which % g_nentity];
    const DWORD op = which % 3;
    BYTE *e;
    if (op < 2) {
        CClassInfo *cls = op ? at<CClassInfo *(__cdecl *)()>(0x48dcc0)() : at<CClassInfo *(__cdecl *)()>(0x487620)();
        e = at<BYTE *(THISCALL *)(void *, DWORD, CClassInfo *, DWORD, DWORD, DWORD)>(0x5176f0)(at<void *>(MODEL_LIST), index, cls, 0, 0,
                                                                                            0xffffffff);
    } else {
        prime(0x10);
        BYTE *s = at<BYTE *(__cdecl *)()>(0x4cf000)();
        reinterpret_cast<CSpawnableModel *>(s)->model = index;
        e = vcall<BYTE *>(s, 23, 0, 0);
        vcall<void *>(s, 1, 1);
    }
    if (!e)
        return 0;
    st_entity(e, 0);
    if (at<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(e, at<CClassInfo *(__cdecl *)()>(0x48dcc0)()) & 0xff) {
        DWORD a = random(), b = random();
        a = (a & 0x007fffff) | 0x3f800000;
        b = (b & 0x007fffff) | 0x40000000;
        *reinterpret_cast<DWORD *>(e + 0xc8) = a;
        *reinterpret_cast<DWORD *>(e + 0xc4) = b;
        at<void(__fastcall *)(void *)>(0x517e70)(e);
        st_entity(e, 0);
    }
    vcall<void *>(e, 1, 1);
    mask = 0xffffffff;
    return 1;
}

}  // namespace

ZAX_CHECKS(models)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally objects{"models: Create, CreateArray, constructors, deleting destructors, Clear"},
        classes{"models: class getters, RegisterFields, startup registrations"},
        registered{"models: RegisterFields against the registered fields (real data)"},
        accessors{"models: CModel's accessors on fake models"}, lists{"models: the model list"},
        tables{"models: the map caching table with a fake world"}, other{"models: COtherMapList"},
        real{"models: models loaded from their files, file names, entities from models (real data)"};
    for (unsigned seed = 1; seed <= (unsigned)scaled(300); seed++) {
        two_sides(objects, "model objects", seed, [](DWORD &mask) { return model_object_case(mask); });
        two_sides(objects, "small objects", seed, [](DWORD &mask) { return small_object_case(mask); });
    }
    objects.report();
    test_classes(classes, registered);
    two_sides(classes, "startup registrations", 1, [](DWORD &mask) { return static_register_case(mask); });
    classes.report();
    registered.report();
    for (unsigned seed = 1; seed <= (unsigned)scaled(400); seed++)
        two_sides(accessors, "accessors", seed, [](DWORD &mask) { return accessor_case(mask); });
    accessors.report();
    for (unsigned seed = 1; seed <= (unsigned)scaled(200); seed++)
        two_sides(lists, "list", seed, [](DWORD &mask) { return list_case(mask); });
    lists.report();
    // g_trace = true: every case logged before it runs (the last line names a case that ends the game)
    for (unsigned seed = 1; seed <= (unsigned)scaled(400); seed++)
        two_sides(tables, "table", seed, [](DWORD &mask) { return table_case(mask); });
    tables.report();
    for (unsigned seed = 1; seed <= (unsigned)scaled(300); seed++)
        two_sides(other, "other map list", seed, [](DWORD &mask) { return other_map_case(mask); });
    other.report();
    two_sides(real, "file names", 1, [](DWORD &mask) { return file_name_case(mask); });
    if (!list_count())
        log("  models: the model list is empty, real data skipped");
    else if (!make_temp_paths())
        log("  models: no temp folder reachable from the data folder, real data skipped");
    else {
        pick_models();
        g_trace = false;   // true: every real-data case logged before it runs
        log("  models: %u records, %u models to load, %u loaded ones for entities; files in %s", list_count(), g_nload, g_nentity, g_temp);
        for (DWORD i = 0; i < g_nload; i++)
            two_sides(real, "load", i + 1, [i](DWORD &mask) { return load_case(i, mask); });
        for (DWORD i = 0; g_nentity && i < 3 * g_nentity; i++)
            two_sides(real, "entity", i + 1, [i](DWORD &mask) { return entity_case(i, mask); });
        static const char *const FILES[] = {"model.bin", "model3.bin"};
        for (const char *f : FILES)
            delete_file(f);
        if (!RemoveDirectoryA(g_temp))
            log("  could not remove %s", g_temp);
    }
    real.report();
    log("  models checks took %u ms", GetTickCount() - start);
}

}  // namespace zax
