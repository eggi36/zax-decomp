// Checks of rest_c.cpp against the original, in the game thread (machinery: checks_inventory_common.h:
// side 0 with the module's hooks out, so nested calls run the original too; the C heap of a case from
// the side's arena where blocks are made; calls into fakes and stand-ins recorded in an event log).
//  - The stubs 0x4f0a70 and 0x55d200 with many values in eax (their upper bytes stay), the token map's
//    0x55d1f0.
//  - The model list's search on a fake list (records with and without a model, loading ones; the
//    model list's records and count swapped in, CModel::IndexOfSequence a stand-in) and on the real list
//    with real sequence names (read only).
//  - The broadcast message on pattern-filled blocks; the message registry on the real one (every name
//    by its id, in other case and unknown, every name's id) and, for new names, on a fresh registry made
//    in place of the real one (guard bit cleared, atexit a stand-in; destroyed and the real one put back).
//  - The memory stream helpers on streams of the exe's constructor (data from the arena).
//  - The world's message lines on blocks of random lines (owned blocks from the arena, the font list's
//    records swapped for fake fonts of several heights): constructor, destructor, Clear, RemoveFirst
//    and Update with ages around 12 s, NaN and negative times.
//  - Salvage King's slot 25 and the polygon kind's points on fakes and the arena; the enum int, byte and
//    bit kinds' slots on fake descriptors and objects (the dynamic property lookups, FileBuf_ReadByte
//    and the setters recording stand-ins).
//  - CPursueAI's two class getters (as checks_inventory_common.h's test_getter) and destructor.
//  - The command line start with stand-ins for the command line, the options, the join, the host window
//    and the main menu, g_World a fake.
#include "checks_inventory_common.h"
#include "ai_common.h"
#include "x87.h"

WINIMPORT int WINAPI lstrcmpiA(const char *a, const char *b);

namespace zax {

int hooks_rest_c(Hook *out);

namespace {

using namespace invcheck;
using ai::dw;
using ai::zs_assign_text;

extern "C" DWORD __cdecl zax_rc_call_eax(DWORD fn, DWORD eax, DWORD arg);
__asm__(".text\n"
        ".globl _zax_rc_call_eax\n"
        "_zax_rc_call_eax:\n"
        "  movl 4(%esp), %edx\n"
        "  movl 8(%esp), %eax\n"
        "  pushl 12(%esp)\n"
        "  calll *%edx\n"
        "  retl\n");
// (fn, eax, ecx, a, b): a thiscall with two arguments and eax set before
extern "C" DWORD __cdecl zax_rc_call_eax2(DWORD fn, DWORD eax, DWORD ecx, DWORD a, DWORD b);
__asm__(".text\n"
        ".globl _zax_rc_call_eax2\n"
        "_zax_rc_call_eax2:\n"
        "  movl 4(%esp), %edx\n"
        "  movl 8(%esp), %eax\n"
        "  movl 12(%esp), %ecx\n"
        "  pushl 20(%esp)\n"
        "  pushl 20(%esp)\n"
        "  calll *%edx\n"
        "  retl\n");

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

float from_bits(DWORD d)
{
    float f;
    memcpy(&f, &d, 4);
    return f;
}
unsigned text_hash(const char *s) { return s ? fnv_text(2166136261u, s) : 0; }

const char *const NAMES[] = {"Idle", "Walk", "Run", "Die", "Fire", "idle", "WALK", "Attack", "", "Nothing Like This",
                             "Hit", "Open", "Close", "Death", "Shoot", "Stand"};
constexpr int NNAMES = sizeof NAMES / sizeof NAMES[0];

// ---- The stub ------------------------------------------------------------------------------------------------

void test_stub(Tally &t)
{
    g_seed = 0x4f0a70;
    for (int k = 0; k < scaled(300); k++) {
        const DWORD eax = k < 4 ? (DWORD)k * 0x55555555u : random(), arg = random();
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            r[side] = zax_rc_call_eax(0x4f0a70, eax, arg);
            side_end(side);
        }
        char d[80];
        wsprintfA(d, "[stub eax %x: %x / %x]", eax, r[0], r[1]);
        t.check(r[0] == r[1], "", d);
    }
    // the token map 0x55d1f0 (the id) and 0x55d200 (al = 1, the rest of eax kept)
    for (int k = 0; k < scaled(300); k++) {
        const DWORD eax = random(), self = random(), a = random(), b = k < 3 ? (DWORD)k - 1 : random();
        DWORD r[2][2];
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            r[side][0] = zax_rc_call_eax2(0x55d1f0, eax, self, a, b);
            r[side][1] = zax_rc_call_eax2(0x55d200, eax, self, a, b);
            side_end(side);
        }
        char d[100];
        wsprintfA(d, "[token map eax %x id %x: %x %x / %x %x]", eax, b, r[0][0], r[0][1], r[1][0], r[1][1]);
        t.check(memcmp(r[0], r[1], sizeof r[0]) == 0, "", d);
    }
}

// ---- The model list ------------------------------------------------------------------------------------------

constexpr int FAKE_MODELS = 12;
BYTE g_records[FAKE_MODELS * 0x80];
BYTE g_models[FAKE_MODELS][0x20];
int g_sequence_of[FAKE_MODELS][NNAMES];
DWORD model_tag(DWORD p)
{
    for (int i = 0; i < FAKE_MODELS; i++)
        if (p == ptrv(g_models[i]))
            return 0x30000000u + (DWORD)i;
    return p;
}
int THISCALL s_index_of_sequence(void *model, const char *name)
{
    int n = -1;
    for (int i = 0; i < NNAMES; i++)
        if (name == NAMES[i])
            n = i;
    ev(0x7280, model_tag(ptrv(model)), (DWORD)n);
    for (int i = 0; i < FAKE_MODELS; i++)
        if (model == g_models[i])
            return n >= 0 ? g_sequence_of[i][n] : -1;
    return -1;
}
const Stand MODEL_STANDS[] = {{0x517280, (void *)&s_index_of_sequence}};
Patches<1> g_model_patches;

void test_find_sequence(Tally &t, unsigned seed)
{
    g_seed = seed;
    const DWORD count = random() % (FAKE_MODELS + 1);
    memset(g_records, 0, sizeof g_records);
    for (DWORD i = 0; i < FAKE_MODELS; i++) {
        BYTE *rec = g_records + i * 0x80;
        const DWORD kind = random() % 6;
        if (kind >= 2)
            dw(rec, 0x78) = ptrv(g_models[i]);
        if (kind == 5)
            rec[0x7f] = 1;   // loading: the list gives null
        for (int n = 0; n < NNAMES; n++)
            g_sequence_of[i][n] = random() % 4 == 0 ? (int)(random() % 20) : -1;
    }
    const int name = (int)(random() % NNAMES);
    const bool with_index = random() % 4 != 0;
    DWORD saved[2];
    memcpy(saved, at<void *>(0x6cfe08), 8);
    DWORD r[2];
    int index[2];
    for (int side = 0; side < 2; side++) {
        index[side] = 0x5a5a5a5a;
        *at<DWORD *>(0x6cfe08) = ptrv(g_records);
        *at<DWORD *>(0x6cfe0c) = count;
        log_reset(side);
        side_begin(side);
        g_model_patches.in(MODEL_STANDS);
        r[side] = model_tag(ptrv(at<BYTE *(__fastcall *)(const char *, int *)>(0x4f7fc0)(NAMES[name], with_index ? &index[side] : nullptr)));
        g_model_patches.out();
        side_end(side);
        memcpy(at<void *>(0x6cfe08), saved, 8);
    }
    char d[120];
    wsprintfA(d, "[find sequence seed %u: %x / %x, index %d / %d, events %d / %d]", seed, r[0], r[1], index[0], index[1], g_nlog[0],
              g_nlog[1]);
    const bool same = r[0] == r[1] && index[0] == index[1] && same_log();
    if (!same)
        log_first_difference();
    t.check(same, "", d);
}

// The real list with real names: read only (the models there are loaded, the search never loads one)
void test_find_sequence_real(Tally &t)
{
    if (!*at<DWORD *>(0x6cfe08))
        return;
    static const char *const REAL[] = {"Idle", "Walk", "Run", "Die", "Death", "Attack", "Fire", "Open", "Closed", "Hit", "Spawn",
                                       "Default", "idle", "No Such Sequence"};
    for (const char *name : REAL) {
        DWORD r[2];
        int index[2] = {-7, -7};
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            r[side] = ptrv(at<BYTE *(__fastcall *)(const char *, int *)>(0x4f7fc0)(name, &index[side]));
            side_end(side);
        }
        char d[120];
        wsprintfA(d, "[real models '%s': %x / %x, index %d / %d]", name, r[0], r[1], index[0], index[1]);
        t.check(r[0] == r[1] && index[0] == index[1], "", d);
    }
}

// ---- Messages ------------------------------------------------------------------------------------------------

void test_broadcast_message(Tally &t, unsigned seed)
{
    BYTE m[2][0x18];
    DWORD r[2];
    g_seed = seed;
    const DWORD a = random(), b = random();
    const BYTE fill = (BYTE)random();
    for (int side = 0; side < 2; side++) {
        memset(m[side], fill, sizeof m[side]);
        side_begin(side);
        r[side] = ptrv(at<BYTE *(THISCALL *)(void *, DWORD, DWORD)>(0x5011d0)(m[side], a, b)) - ptrv(m[side]);
        side_end(side);
    }
    t.check(r[0] == r[1] && r[0] == 0 && memcmp(m[0], m[1], sizeof m[0]) == 0, "", "[broadcast message]");
}

constexpr DWORD REGISTRY = 0x6c2d18, REGISTRY_GUARD = 0x6c2d10;
DWORD registry_count() { return *at<DWORD *>(REGISTRY + 0xc); }
const char *registry_name(DWORD i) { return *at<const char **>(*at<DWORD *>(*at<DWORD *>(REGISTRY + 8) + i * 4)); }

DWORD has_name(DWORD id, const char *name)
{
    DWORD msg[4] = {0x5eeb64, id, 0, 0};
    return at<DWORD(THISCALL *)(void *, const char *)>(0x506620)(msg, name);
}
DWORD id_of(const char *name) { return at<DWORD(__fastcall *)(const char *)>(0x506680)(name); }

// The real registry: read only (every name there, so nothing is added)
void test_registry_real(Tally &t)
{
    if (!(*at<BYTE *>(REGISTRY_GUARD) & 1))
        return;
    const DWORD n = registry_count();
    char upper[64];
    for (DWORD i = 0; i < n && i < 400; i++) {
        const char *name = registry_name(i);
        copy_upper(upper, name, sizeof upper);
        DWORD r[2][5];
        for (int side = 0; side < 2; side++) {
            side_begin(side);
            r[side][0] = has_name(i, name);
            r[side][1] = has_name(i, upper);
            r[side][2] = has_name(i, "Not A Message Name");
            r[side][3] = has_name(i, registry_name((i + 1) % n));
            r[side][4] = id_of(name);
            side_end(side);
        }
        char d[160];
        wsprintfA(d, "[registry %u '%.40s': %x %x %x %x %x / %x %x %x %x %x]", i, name, r[0][0], r[0][1], r[0][2], r[0][3], r[0][4],
                  r[1][0], r[1][1], r[1][2], r[1][3], r[1][4]);
        t.check(memcmp(r[0], r[1], sizeof r[0]) == 0, "", d);
    }
}

// New names on a fresh registry in place of the real one
void test_registry_fresh(Tally &t, unsigned seed)
{
    static const char *const NEW[] = {"Rc Alpha", "rc alpha", "Rc Beta", "Rc Gamma", "RC BETA", "Rc Delta", ""};
    constexpr int NNEW = sizeof NEW / sizeof NEW[0];
    SavedBytes saved;
    save_bytes(saved, REGISTRY, 0x14);
    const BYTE guard = *at<BYTE *>(REGISTRY_GUARD);
    unsigned h[2];
    DWORD atexit_calls[2], atexit_fn[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        *at<BYTE *>(REGISTRY_GUARD) = guard & ~1;
        g_atexit_calls = 0;
        g_atexit_function = 0;
        CodePatch p;
        side_begin(side);
        patch_jump(p, 0x5d0863, (void *)&fake_atexit);
        h[side] = 2166136261u;
        const int steps = 1 + (int)(random() % 10);
        for (int k = 0; k < steps; k++) {
            const DWORD id = id_of(NEW[random() % NNEW]);
            h[side] = fnv(h[side], &id, 4);
        }
        const DWORD n = registry_count();
        h[side] = fnv(h[side], &n, 4);
        for (DWORD i = 0; i < n; i++) {
            h[side] = fnv_text(h[side], registry_name(i));
            for (int j = 0; j < NNEW; j++) {
                const DWORD r = has_name(i, NEW[j]);
                h[side] = fnv(h[side], &r, 4);
            }
        }
        unpatch(p);
        side_end(side);
        atexit_calls[side] = g_atexit_calls;
        atexit_fn[side] = g_atexit_function;
        at<void(THISCALL *)(void *)>(0x592220)(at<void *>(REGISTRY));   // FieldNameList::Dtor
        restore_bytes(saved);
        *at<BYTE *>(REGISTRY_GUARD) = guard;
    }
    char d[120];
    wsprintfA(d, "[fresh registry seed %u: atexit %u %x / %u %x]", seed, atexit_calls[0], atexit_fn[0], atexit_calls[1], atexit_fn[1]);
    t.check(h[0] == h[1] && atexit_calls[0] == atexit_calls[1] && atexit_fn[0] == atexit_fn[1] && atexit_calls[0] == 1, "", d);
}

// ---- Memory streams --------------------------------------------------------------------------------------------

struct Stream {
    DWORD vtable, a, b, position, size, capacity, data;
    BYTE owns, pad[3];
};
static_assert(sizeof(Stream) == 0x20);

void make_stream(Stream *s, unsigned &seed, BYTE pattern)
{
    at<void *(THISCALL *)(void *)>(0x505f00)(s);
    g_seed = seed;
    const DWORD n = random() % 5 == 0 ? 0 : random() % 300;
    BYTE bytes[300];
    for (DWORD i = 0; i < n; i++)
        bytes[i] = (BYTE)(random() >> 16) ^ pattern;
    if (n)
        vcall<DWORD>(s, 2, bytes, n);
    if (random() % 3 == 0)
        vcall<DWORD>(s, 5, random() % (n + 1));
    seed = g_seed;
}

void test_streams(Tally &t, unsigned seed)
{
    Stream a[2], b[2];
    DWORD r[2];
    unsigned h[2];
    const int which = (int)(seed % 3);
    for (int side = 0; side < 2; side++) {
        arena_on(side, (BYTE)seed);
        unsigned s = seed;
        make_stream(&a[side], s, 0);
        make_stream(&b[side], s, 0x5a);
        side_begin(side);
        if (which == 0)
            r[side] = at<DWORD(THISCALL *)(void *)>(0x506350)(&a[side]);
        else
            r[side] = at<DWORD(THISCALL *)(void *, void *)>(0x506320)(&a[side], which == 1 ? &b[side] : &a[side]);
        side_end(side);
        h[side] = fnv(2166136261u, &a[side].position, 12);
        h[side] = fnv(h[side], &a[side].owns, 1);
        const DWORD rd = rel(a[side].data, side);
        h[side] = fnv(h[side], &rd, 4);
        if (a[side].data && a[side].size <= a[side].capacity)
            h[side] = fnv(h[side], at<void *>(a[side].data), a[side].size);
        h[side] = fnv(h[side], &b[side].position, 12);
        arena_off();
    }
    char d[120];
    wsprintfA(d, "[stream seed %u kind %d: eax %x / %x, size %u / %u]", seed, which, r[0], r[1], a[0].size, a[1].size);
    t.check(h[0] == h[1] && r[0] == r[1] && same_region(g_arena[0], g_arena[1], g_used[0]) && g_used[0] == g_used[1], "", d);
}

// ---- The world's message lines ----------------------------------------------------------------------------------

constexpr DWORD LINES_SIZE = 0x1e84;
BYTE g_lines[2][LINES_SIZE];
constexpr int FAKE_FONTS = 6;
BYTE g_font_records[FAKE_FONTS * 0x80];
BYTE g_fonts[FAKE_FONTS][0x20];

float some_age()
{
    const DWORD r = random() >> 8;
    switch (r % 10) {
    case 0: return 12.0f;
    case 1: return 11.99f;
    case 2: return 12.01f;
    case 3: return from_bits(0x7fc00000u);
    case 4: return -(float)(r >> 12 & 0x3f) / 4.0f;
    default: return (float)(r >> 12 & 0x3ff) / 64.0f;
    }
}

// Random lines (texts, fonts in and outside the list, ages; owned blocks from the side's arena)
void fill_lines(BYTE *l, unsigned seed, int side)
{
    g_seed = seed;
    for (DWORD k = 0; k < LINES_SIZE; k += 4)
        dw(l, k) = random();
    dw(l, 0) = 1 + random() % 25;
    for (int i = 0; i < 25; i++) {
        BYTE *line = l + 4 + i * 0x138;
        dw(line, 0) = random() % 8 == 0 ? random() : random() % (FAKE_FONTS + 2);
        const DWORD n = random() % 40;
        for (DWORD c = 0; c < n; c++)
            line[4 + c] = (BYTE)('A' + random() % 26);
        line[4 + n] = 0;
        dw(line, 0x130) = random() % 3 == 0 ? ptrv(heap_alloc(8 + random() % 40)) : 0;
        *reinterpret_cast<float *>(line + 0x134) = some_age();
    }
    (void)side;
}

void test_lines(Tally &t, int kind, unsigned seed)
{
    DWORD r[2];
    DWORD saved[2];
    memcpy(saved, at<void *>(0x6be210), 8);
    float seconds = 0;
    for (int side = 0; side < 2; side++) {
        arena_on(side, (BYTE)seed);
        fill_lines(g_lines[side], seed, side);
        g_seed = seed ^ 0x9e3779b9u;
        seconds = random() % 7 == 0 ? from_bits(0x7fc00000u) : random() % 5 == 0 ? -(float)(random() % 100) / 50.0f : (float)(random() % 400) / 100.0f;
        if (kind == 3 && random() % 4 == 0)
            dw(g_lines[side], 0) = 1;
        *at<DWORD *>(0x6be210) = ptrv(g_font_records);
        *at<DWORD *>(0x6be214) = FAKE_FONTS;
        side_begin(side);
        switch (kind) {
        case 0: r[side] = rel(ptrv(at<void *(THISCALL *)(void *, DWORD, DWORD)>(0x506730)(g_lines[side], random(), random())), side) - 0; break;
        case 1: r[side] = rel(at<DWORD(THISCALL *)(void *)>(0x5067b0)(g_lines[side]), side); break;
        case 2: r[side] = rel(at<DWORD(THISCALL *)(void *)>(0x506f70)(g_lines[side]), side); break;
        case 3: r[side] = at<DWORD(THISCALL *)(void *)>(0x506fb0)(g_lines[side]); break;
        default: r[side] = at<DWORD(THISCALL *)(void *, float)>(0x506a10)(g_lines[side], seconds); break;
        }
        side_end(side);
        memcpy(at<void *>(0x6be210), saved, 8);
        arena_off();
    }
    if (kind == 0)
        r[0] = r[0] == ptrv(g_lines[0]) ? 1 : r[0], r[1] = r[1] == ptrv(g_lines[1]) ? 1 : r[1];
    char d[140];
    const bool same = r[0] == r[1] && same_region(g_lines[0], g_lines[1], LINES_SIZE);
    wsprintfA(d, "[lines kind %d seed %u: eax %x / %x, first difference +%x: %x / %x]", kind, seed, r[0], r[1], g_diff_at, g_diff_a,
              g_diff_b);
    t.check(same, "", d);
}

// ---- Salvage King, the polygon kind, the field kinds ------------------------------------------------------------

DWORD g_rec_vt[64];
DWORD THISCALL f_slot48(void *, DWORD a, DWORD b)
{
    ev(48, a, b);
    return result(0x48);
}
void test_salvage(Tally &t, unsigned seed)
{
    DWORD obj[2] = {ptrv(g_rec_vt), 0};
    DWORD r[2];
    g_seed = seed;
    const DWORD a = random(), b = random();
    for (int side = 0; side < 2; side++) {
        log_reset(side);
        side_begin(side);
        r[side] = at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x51d320)(obj, a, b);
        side_end(side);
    }
    t.check(r[0] == r[1] && same_log(), "", "[salvage king slot 25]");
}

void test_points(Tally &t, unsigned seed)
{
    DWORD r[2];
    const int n = (int)(seed % 14);
    for (int side = 0; side < 2; side++) {
        arena_on(side, (BYTE)(seed * 7));
        side_begin(side);
        r[side] = rel(ptrv(at<void *(THISCALL *)(void *, int, DWORD, DWORD)>(0x548330)(nullptr, n, seed, ~seed)), side);
        side_end(side);
        arena_off();
    }
    char d[100];
    wsprintfA(d, "[points %d: %x / %x, %u / %u bytes]", n, r[0], r[1], g_used[0], g_used[1]);
    t.check(r[0] == r[1] && g_used[0] == g_used[1] && same_region(g_arena[0], g_arena[1], g_used[0]), "", d);
}

BYTE g_desc[2][0x40], g_object[2][0x80], g_block[2][0x80];
int g_block_kind;   // 0: the object itself, 1: a block, 2: null (Get only)
DWORD tag(DWORD p)
{
    for (int s = 0; s < 2; s++) {
        if (p >= ptrv(g_object[s]) && p < ptrv(g_object[s]) + 0x80)
            return 0x10000000u + (p - ptrv(g_object[s]));
        if (p >= ptrv(g_block[s]) && p < ptrv(g_block[s]) + 0x80)
            return 0x20000000u + (p - ptrv(g_block[s]));
        if (p >= ptrv(g_desc[s]) && p < ptrv(g_desc[s]) + 0x40)
            return 0x40000000u + (p - ptrv(g_desc[s]));
    }
    return p;
}
// A result whose upper bytes are those of the side's object or block address (al its own): tagged
DWORD tag_upper(DWORD r, int side)
{
    if ((r & ~0xffu) == (ptrv(g_object[side]) & ~0xffu))
        return 0x10000000u | (r & 0xff);
    if ((r & ~0xffu) == (ptrv(g_block[side]) & ~0xffu))
        return 0x20000000u | (r & 0xff);
    return tag(r);
}
void *dynamic_answer(void *object)
{
    if (g_block_kind == 0)
        return object;
    if (g_block_kind == 1)
        return g_block[g_side];
    return nullptr;
}
void *THISCALL s_get_or_add(void *object, void *desc)
{
    ev(0x7340, tag(ptrv(object)), tag(ptrv(desc)));
    return g_block_kind == 2 ? object : dynamic_answer(object);
}
void *THISCALL s_get(void *object, void *desc)
{
    ev(0x7320, tag(ptrv(object)), tag(ptrv(desc)));
    return dynamic_answer(object);
}
DWORD THISCALL s_setter(void *p, DWORD v)
{
    ev(0x5e7, tag(ptrv(p)), v);
    return result(0x5e);
}
BYTE g_read_byte;
bool g_read_writes;
BYTE g_stream[0x100];
void *THISCALL s_read_byte(void *stream, BYTE *out)
{
    ev(0x22a0, tag(ptrv(stream)));
    if (g_read_writes)
        *out = g_read_byte;
    return stream;
}
const Stand FIELD_STANDS[] = {{0x417340, (void *)&s_get_or_add}, {0x417320, (void *)&s_get}, {0x4a22a0, (void *)&s_read_byte}};
Patches<3> g_field_patches;

void test_field_kinds(Tally &t, int fn, unsigned seed)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        for (BYTE &b : g_desc[side])
            b = (BYTE)(random() >> 16);
        for (BYTE &b : g_object[side])
            b = (BYTE)(random() >> 16);
        for (BYTE &b : g_block[side])
            b = (BYTE)(random() >> 16);
        dw(g_desc[side], 0x10) = random() % 0x7d;
        g_block_kind = (int)(random() % 3);
        const bool setter = random() % 3 == 0;
        dw(g_desc[side], 0x3c) = fn == 0 && setter ? ptrv((void *)&s_setter) : 0;
        dw(g_desc[side], 0x28) = fn == 2 && setter ? ptrv((void *)&s_setter) : dw(g_desc[side], 0x28);
        if (fn == 2 && !setter)
            dw(g_desc[side], 0x28) = 0;
        if (fn == 3)
            dw(g_desc[side], 0x10) &= ~3u, dw(g_desc[side], 0x2c) = random() % 4 == 0 ? random() : 1u << (random() % 32);
        const DWORD value = random();
        g_read_byte = random() % 2 ? (BYTE)random() : 0;
        g_read_writes = random() % 5 != 0;
        // the same pointer on both sides: without a byte read its low byte is the value
        BYTE *stream = g_stream + (random() % 0x40) * 4;
        log_reset(side);
        side_begin(side);
        g_field_patches.in(FIELD_STANDS);
        switch (fn) {
        case 0: r[side] = at<DWORD(THISCALL *)(void *, void *, DWORD)>(0x54dae0)(g_desc[side], g_object[side], value); break;
        case 1: r[side] = at<DWORD(THISCALL *)(void *, void *)>(0x54de90)(g_desc[side], g_object[side]); break;
        case 2: r[side] = at<DWORD(THISCALL *)(void *, void *, DWORD)>(0x54dec0)(g_desc[side], g_object[side], value); break;
        default: r[side] = at<DWORD(THISCALL *)(void *, void *, void *)>(0x54e1b0)(g_desc[side], g_object[side], stream); break;
        }
        g_field_patches.out();
        side_end(side);
        if (fn == 3)   // eax: the value's address with al = 1
            r[side] = r[side] == (((ptrv(g_object[side]) + dw(g_desc[side], 0x10)) & ~0xffu) | 1) ? 1 : r[side] ^ 0xdead0000u;
        else
            r[side] = fn == 1 ? tag_upper(r[side], side) : tag(r[side]);
    }
    char d[120];
    wsprintfA(d, "[field slot %d seed %u: %x / %x]", fn, seed, r[0], r[1]);
    const bool same = r[0] == r[1] && same_log() && memcmp(g_object[0], g_object[1], 0x80) == 0 && memcmp(g_block[0], g_block[1], 0x80) == 0;
    if (!same)
        log_first_difference();
    t.check(same, "", d);
}

// ---- CPursueAI ----------------------------------------------------------------------------------------------------

const ClassDef PURSUE = {"CPursueAI", 0x10, 0x712230, 0, 0, 0x58ea40, {0x54fdc0, 0x54fe10}, false};

void test_pursue_dtor(Tally &t, unsigned seed)
{
    BYTE o[2][0x10];
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        for (BYTE &b : o[side])
            b = (BYTE)(random() >> 16);
        dw(o[side], 0) = 0x5efb54;
        dw(o[side], 4) = 0;     // no dynamic properties
        dw(o[side], 0xc) = 0;   // no broadcaster
        side_begin(side);
        r[side] = at<DWORD(THISCALL *)(void *)>(0x54fe70)(o[side]);
        side_end(side);
        if (r[side] >= ptrv(o[side]) && r[side] < ptrv(o[side]) + 0x10)
            r[side] = 0x10000000u + r[side] - ptrv(o[side]);
    }
    char d[100];
    wsprintfA(d, "[CPursueAI destructor seed %u: %x / %x]", seed, r[0], r[1]);
    t.check(r[0] == r[1] && memcmp(o[0], o[1], 0x10) == 0, "", d);
}

// ---- The command line start ----------------------------------------------------------------------------------------

DWORD g_cl_mask;   // bit 0 "-connect" given, 1 "-name", 2 "-hostname", 3 the join works, 4 "-host" given
BYTE g_world_fake[0x10];
DWORD g_world_vt[64];
void *g_window;
DWORD option_id(const char *o)
{
    static const char *const OPTIONS[] = {"-connect", "-name", "-hostname", "-host"};
    for (DWORD i = 0; i < 4; i++)
        if (lstrcmpiA(o, OPTIONS[i]) == 0)
            return i + 1;
    return 0x99;
}
DWORD out_tag(DWORD p)
{
    if (p == 0x6bd2f8 || p == 0x6c0490)
        return p;
    return 0x5ac0;   // the caller's local string
}
BYTE __fastcall s_value(const char *option, ZString *out)
{
    const DWORD id = option_id(option);
    ev(0x21a0, id, out_tag(ptrv(out)));
    if (id == 1 && (g_cl_mask & 1)) {
        zs_assign_text(out, "10.0.0.7:2300");
        return 1;
    }
    return id == 2 ? (BYTE)(g_cl_mask >> 1 & 1) : id == 3 ? (BYTE)(g_cl_mask >> 2 & 1) : 0;
}
BYTE __fastcall s_has(const char *option)
{
    ev(0x22c0, option_id(option));
    return (BYTE)(g_cl_mask >> 4 & 1);
}
void THISCALL s_load_options(void *o) { ev(0xce50, ptrv(o)); }
void THISCALL s_host_options(void *o) { ev(0x2c50, ptrv(o)); }
BYTE __fastcall s_join(DWORD a, const char *text, DWORD b, DWORD c, DWORD d)
{
    ev(0x2940, a, text_hash(text), b);
    ev(0x2941, c, d);
    return (BYTE)(g_cl_mask >> 3 & 1);
}
DWORD THISCALL f_world33(void *) { ev(0x33); return result(0x33); }
void *THISCALL s_window(void *w, DWORD a, DWORD b)
{
    ev(0x3080, a, b);
    g_window = w;
    return w;
}
void THISCALL s_push(void *menu, void *w, DWORD flag)
{
    ev(0x40c0, menu == *at<void **>(0x714558), w == g_window && w, flag);
    if (w)
        pool_free(w, 0x1a4);
}
const Stand CL_STANDS[] = {{0x4d21a0, (void *)&s_value}, {0x4d22c0, (void *)&s_has}, {0x46ce50, (void *)&s_load_options},
                           {0x4d2c50, (void *)&s_host_options}, {0x5a2940, (void *)&s_join}, {0x4d3080, (void *)&s_window},
                           {0x4040c0, (void *)&s_push}};
Patches<7> g_cl_patches;

void test_command_line(Tally &t, DWORD mask)
{
    DWORD r[2];
    const DWORD saved_world = *at<DWORD *>(0x6c2080);
    for (int side = 0; side < 2; side++) {
        g_cl_mask = mask;
        g_window = nullptr;
        dw(g_world_fake, 0) = ptrv(g_world_vt);
        *at<DWORD *>(0x6c2080) = ptrv(g_world_fake);
        log_reset(side);
        side_begin(side);
        g_cl_patches.in(CL_STANDS);
        r[side] = at<DWORD(__cdecl *)()>(0x519ef0)();
        g_cl_patches.out();
        side_end(side);
        *at<DWORD *>(0x6c2080) = saved_world;
    }
    char d[120];
    wsprintfA(d, "[command line %x: %x / %x, events %d / %d]", mask, r[0], r[1], g_nlog[0], g_nlog[1]);
    const bool same = r[0] == r[1] && same_log();
    if (!same)
        log_first_difference();
    t.check(same, "", d);
}

void init()
{
    for (DWORD &v : g_rec_vt)
        v = 0;
    g_rec_vt[48] = ptrv((void *)&f_slot48);
    for (DWORD &v : g_world_vt)
        v = 0;
    g_world_vt[33] = ptrv((void *)&f_world33);
    memset(g_font_records, 0, sizeof g_font_records);
    for (int i = 0; i < FAKE_FONTS; i++) {
        memset(g_fonts[i], 0, sizeof g_fonts[i]);
        dw(g_fonts[i], 0x10) = (DWORD)(3 * i);
        dw(g_fonts[i], 0x18) = (DWORD)(3 * i + 4 * i + (i == 3 ? 0 : 8));   // heights 8, 15, 22, 21, 36, 43
        dw(g_font_records + i * 0x80, 0x78) = ptrv(g_fonts[i]);
    }
}

}  // namespace

ZAX_CHECKS(rest_c)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_rest_c;
    if (!arena_setup()) {
        log("Check rest c: no arena");
        return;
    }
    init();
    {
        Tally t{"rest c: stubs, token map, model list search"};
        test_stub(t);
        for (unsigned k = 0; k < (unsigned)scaled(600); k++)
            test_find_sequence(t, 0x4f7fc0u + k);
        test_find_sequence_real(t);
        t.report();
    }
    {
        Tally t{"rest c: broadcast message, message registry"};
        for (unsigned k = 0; k < (unsigned)scaled(100); k++)
            test_broadcast_message(t, 0x5011d0u + k);
        test_registry_real(t);
        for (unsigned k = 0; k < (unsigned)scaled(200); k++)
            test_registry_fresh(t, 0x506680u + k);
        t.report();
    }
    {
        Tally t{"rest c: memory stream copy and clear"};
        for (unsigned k = 0; k < (unsigned)scaled(600); k++)
            test_streams(t, 0x506320u + k);
        t.report();
    }
    {
        Tally t{"rest c: message lines"};
        for (int kind = 0; kind < 5; kind++)
            for (unsigned k = 0; k < (unsigned)scaled(kind == 4 ? 1500 : 300); k++)
                test_lines(t, kind, 0x506a10u * (unsigned)(kind + 1) + k);
        t.report();
    }
    {
        Tally t{"rest c: Salvage King slot 25, polygon points, field kind slots"};
        for (unsigned k = 0; k < (unsigned)scaled(50); k++)
            test_salvage(t, 0x51d320u + k);
        for (unsigned k = 0; k < (unsigned)scaled(140); k++)
            test_points(t, k);
        for (int fn = 0; fn < 4; fn++)
            for (unsigned k = 0; k < (unsigned)scaled(400); k++)
                test_field_kinds(t, fn, 0x54d000u * (unsigned)(fn + 1) + k);
        t.report();
    }
    {
        Tally t{"rest c: CPursueAI getters and destructor"};
        for (DWORD g : PURSUE.getters)
            test_getter(t, PURSUE, g);
        for (unsigned k = 0; k < (unsigned)scaled(50); k++)
            test_pursue_dtor(t, 0x54fe70u + k);
        t.report();
    }
    {
        Tally t{"rest c: multiplayer start from the command line"};
        for (DWORD mask = 0; mask < 32; mask++)
            for (int k = 0; k < 2; k++)
                test_command_line(t, mask);
        t.report();
    }
}

}  // namespace zax
