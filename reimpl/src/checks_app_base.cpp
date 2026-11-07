// Checks of app_base.cpp against the original, in the game thread (side 0 with the module's hooks out,
// side 1 through the reimplementation):
//  - the application base: the display defaults; the fixed steps 0x4cdd80 on a fake application (its
//    slots 3 and 4 recorders, the timer functions 0x53d150/0x53d170 stand-ins answering from the case);
//    the flag setters; the frame timers (0x4a8240/0x4a83c0 recorders, the frame globals saved);
//  - the command line: a test command line in 0x6c047c (the game's put back after), options with and
//    without values, quoted, at the end, as prefixes of other words;
//  - Win32: the current directory, the exe's path and base name, the memory, the screen depth, MulDiv;
//  - the handle heap: random sequences of allocations (direct and through handles, discardable and
//    locked blocks), frees, unlocks, compactions and resets on a heap of each side, the heap's fields,
//    its memory and the handles compared after every step (pointers into the heap, the heap object and
//    the handles as offsets).
#include "check_util.h"
#include "checks_inventory_common.h"
#include "zstring.h"

namespace zax {

int hooks_app_base(Hook *out);

namespace {

DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }
template <typename T> T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }

void out_() { invcheck::hooks_out(); }
void in_() { invcheck::hooks_in(); }

// ---- The application base ----------------------------------------------------------------------------------

DWORD g_events[2][64];
int g_nevents[2];
int g_side;
void ev(DWORD v)
{
    if (g_nevents[g_side] < 64)
        g_events[g_side][g_nevents[g_side]] = v;
    g_nevents[g_side]++;
}
bool same_events() { return g_nevents[0] == g_nevents[1] && !memcmp(g_events[0], g_events[1], 4 * (g_nevents[0] < 64 ? g_nevents[0] : 64)); }

void test_defaults(Tally &t)
{
    const DWORD fns[] = {0x4cdc70, 0x4cdc80, 0x4cdc90};
    for (DWORD f : fns) {
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                out_();
            r[side] = at<DWORD(__cdecl *)()>(f)();
            if (side == 0)
                in_();
        }
        char d[80];
        wsprintfA(d, "[%x: %x / %x]", f, r[0], r[1]);
        t.check(r[0] == r[1], "App defaults", d);
    }
}

unsigned long long g_counter, g_frequency;
unsigned long long __cdecl fake_counter()
{
    ev(0xc0);
    return g_counter;
}
unsigned long long __cdecl fake_frequency()
{
    ev(0xf0);
    return g_frequency;
}
void THISCALL fake_step(void *self, float seconds)
{
    (void)self;
    ev(0x53);
    ev(bits(seconds));
}
void THISCALL fake_after_steps(void *) { ev(0x54); }
DWORD g_app_vtable[8];

DWORD pick_float(float lo, float hi)
{
    const DWORD k = random() % 16;
    const DWORD sp[] = {0, 0x80000000u, 0x7fc00000u, 0x7f800000u, 0x3a83126fu /* 0.001 */};
    if (k < 5)
        return sp[k];
    return bits(lo + (hi - lo) * (float)(random() >> 8) * (1.0f / 16777216.0f));
}

void test_steps(Tally &t)
{
    g_app_vtable[3] = pv((void *)&fake_step);
    g_app_vtable[4] = pv((void *)&fake_after_steps);
    for (unsigned seed = 1; seed <= (unsigned)scaled(400); seed++) {
        alignas(16) BYTE app[2][0x60];
        long double r[2];
        for (int side = 0; side < 2; side++) {
            g_side = side;
            g_nevents[side] = 0;
            g_seed = seed;
            for (DWORD i = 0; i < 0x60; i += 4)
                fl<DWORD>(app[side], i) = random();
            fl<DWORD>(app[side], 0) = pv(g_app_vtable);
            fl<BYTE>(app[side], 0x1d) = (BYTE)(random() % 3 != 0);
            fl<double>(app[side], 0x40) = (double)(random() % 1000);
            g_frequency = 3579545 + random() % 1000;
            const unsigned long long last = (unsigned long long)(random() % 100000000);
            fl<double>(app[side], 0x48) = (double)last;
            g_counter = last + (unsigned long long)(random() % 2000000);
            fl<DWORD>(app[side], 0x50) = pick_float(0.0f, 0.1f);
            fl<DWORD>(app[side], 0x54) = random() % 8 ? bits(0.01f + (float)(random() % 50) * 0.001f) : pick_float(0.0f, 0.2f);
            fl<DWORD>(app[side], 0x58) = random() % 8 ? bits(0.05f + (float)(random() % 10) * 0.01f) : pick_float(0.0f, 0.3f);
            fl<DWORD>(app[side], 0x5c) = random() % 8 ? bits(0.2f + (float)(random() % 10) * 0.05f) : pick_float(0.0f, 1.0f);
            // a step that would run for ever (a tiny or no step with time left) is not a case
            if (!(fl<float>(app[side], 0x54) >= 0.001f) || !(fl<float>(app[side], 0x58) >= 0.001f) || fl<float>(app[side], 0x5c) > 1e6f ||
                !(fl<float>(app[side], 0x5c) == fl<float>(app[side], 0x5c)))
                fl<DWORD>(app[side], 0x54) = bits(0.02f), fl<DWORD>(app[side], 0x58) = bits(0.1f), fl<DWORD>(app[side], 0x5c) = bits(0.5f);
            CodePatch pc, pf;
            patch_jump(pc, 0x53d150, (void *)&fake_counter);
            patch_jump(pf, 0x53d170, (void *)&fake_frequency);
            if (side == 0)
                out_();
            r[side] = at<long double(THISCALL *)(void *)>(0x4cdd80)(app[side]);
            if (side == 0)
                in_();
            unpatch(pf);
            unpatch(pc);
        }
        char d[160];
        wsprintfA(d, "[0x4cdd80 seed %u: %u / %u events, left %x / %x]", seed, g_nevents[0], g_nevents[1], fl<DWORD>(app[0], 0x50),
                  fl<DWORD>(app[1], 0x50));
        t.check(!memcmp(app[0], app[1], 0x60) && !memcmp(&r[0], &r[1], 10) && same_events(), "App::RunSteps", d);
    }
}

void test_small(Tally &t)
{
    for (unsigned seed = 1; seed <= (unsigned)scaled(40); seed++) {
        alignas(16) BYTE o[2][0x40];
        DWORD r[2][4];
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            for (DWORD i = 0; i < 0x40; i += 4)
                fl<DWORD>(o[side], i) = random();
            const DWORD v = random();
            if (side == 0)
                out_();
            r[side][0] = at<DWORD(THISCALL *)(void *, DWORD)>(0x4cde90)(o[side], v) & 0xff;
            r[side][1] = at<DWORD(THISCALL *)(void *, DWORD)>(0x4cdeb0)(o[side], v >> 8) & 0xff;
            r[side][2] = at<DWORD(THISCALL *)(void *, DWORD)>(0x4cdf20)(o[side], v) & 0xff;
            r[side][3] = at<DWORD(THISCALL *)(void *, DWORD)>(0x4ce500)(o[side], v);
            if (side == 0)
                in_();
        }
        t.check(!memcmp(o[0], o[1], 0x40) && !memcmp(r[0], r[1], sizeof r[0]), "App setters", "[0x4cde90, 0x4cdeb0, 0x4cdf20, 0x4ce500]");
    }
}

DWORD THISCALL fake_timer_reset(void *timer)
{
    ev(0x240000 + (pv(timer) & 0xffff));
    return pv(timer);
}
void THISCALL fake_timer_start(void *timer) { ev(0x3c0000 + (pv(timer) & 0xffff)); }
DWORD THISCALL fake_call_all(void *) { ev(0x28); return 0x2828; }

void test_timers(Tally &t)
{
    SavedBytes frames;
    save_bytes(frames, 0x6c02d8, 12);
    for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++) {
        DWORD app[2][0x10], globals[2][3], r[2];
        for (int side = 0; side < 2; side++) {
            g_side = side;
            g_nevents[side] = 0;
            g_seed = seed;
            for (DWORD &v : app[side])
                v = random() % 6;
            for (DWORD i = 0; i < 3; i++)
                at<DWORD *>(0x6c02d8)[i] = random() % 6;
            CodePatch a, b, c;
            patch_jump(a, 0x4a8240, (void *)&fake_timer_reset);
            patch_jump(b, 0x4a83c0, (void *)&fake_timer_start);
            patch_jump(c, 0x4a3730, (void *)&fake_call_all);
            if (side == 0)
                out_();
            r[side] = at<DWORD(__cdecl *)()>(0x4ce130)();
            at<void(THISCALL *)(void *)>(0x4ce470)(app[side]);
            at<void(THISCALL *)(void *)>(0x4ce4c0)(app[side]);
            r[side] += at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4ce4f0)(app[side], 1, 2);
            if (side == 0)
                in_();
            unpatch(c);
            unpatch(b);
            unpatch(a);
            memcpy(globals[side], at<void *>(0x6c02d8), 12);
        }
        t.check(r[0] == r[1] && !memcmp(globals[0], globals[1], 12) && same_events(), "App timers", "[0x4ce130, 0x4ce470, 0x4ce4c0, 0x4ce4f0]");
    }
    restore_bytes(frames);
}

// ---- The command line ------------------------------------------------------------------------------------------

const char *zs_text(const ZString *z) { return at<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
unsigned text_hash(const char *s)
{
    unsigned h = 2166136261u;
    for (; *s; s++)
        h = (h ^ (BYTE)*s) * 16777619u;
    return h;
}

void test_command_line(Tally &t)
{
    ZString &line = *at<ZString *>(0x6c047c);
    const ZString saved = line;
    line.buffer = nullptr;
    at<void *(THISCALL *)(void *)>(0x4dec90)(&line);
    const char *const lines[] = {"",
                                 "/Map \"Act 1/M1M2/01 Main\" /Spawn Respawn2 /SkipMovies",
                                 "/skipmovies/Original /Difficulty 2",
                                 "  /Width 800 /Height   600  /Depth",
                                 "/Mapper x /Map",
                                 "/Spawn \"unterminated /Map 3",
                                 "/Res 1024x768 /res2 7\t/Last"};
    const char *const options[] = {"/Map", "/Spawn", "/SkipMovies", "/Original", "/Difficulty", "/Width", "/Height", "/Depth", "/Res", "/Last",
                                   "/Missing", "/"};
    for (const char *l : lines) {
        // setting it (0x4d2190) on both sides
        unsigned set[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                out_();
            at<DWORD(__fastcall *)(const char *)>(0x4d2190)(l);
            if (side == 0)
                in_();
            set[side] = text_hash(zs_text(&line));
        }
        t.check(set[0] == set[1] && set[0] == text_hash(l), "CommandLine_Set", l);
        for (const char *o : options) {
            DWORD has[2], found[2];
            int num[2];
            unsigned value[2];
            for (int side = 0; side < 2; side++) {
                ZString v;
                at<void *(THISCALL *)(void *)>(0x4dec90)(&v);
                if (side == 0)
                    out_();
                has[side] = at<DWORD(__fastcall *)(const char *)>(0x4d22c0)(o);
                found[side] = at<DWORD(__fastcall *)(const char *, ZString *)>(0x4d21a0)(o, &v) & 0xff;
                num[side] = at<int(__fastcall *)(const char *, int)>(0x4d2220)(o, -77);
                if (side == 0)
                    in_();
                value[side] = text_hash(zs_text(&v));
                at<void(THISCALL *)(void *)>(0x4defd0)(&v);
            }
            char d[160];
            wsprintfA(d, "[\"%.40s\" %s: has %x / %x, value %x / %x (%x / %x), int %d / %d]", l, o, has[0], has[1], found[0], found[1], value[0],
                      value[1], num[0], num[1]);
            t.check(has[0] == has[1] && found[0] == found[1] && value[0] == value[1] && num[0] == num[1], "CommandLine", d);
        }
    }
    at<void(THISCALL *)(void *)>(0x4defd0)(&line);
    line = saved;
}

// ---- Win32 -------------------------------------------------------------------------------------------------

void test_platform(Tally &t)
{
    char a[2][0x110];
    DWORD r[2];
    const DWORD sizes[] = {0x10, 0x104};
    for (DWORD n : sizes) {
        for (int side = 0; side < 2; side++) {
            memset(a[side], 0x5a, sizeof a[side]);
            if (side == 0)
                out_();
            r[side] = at<DWORD(__fastcall *)(char *, DWORD)>(0x4d2300)(a[side], n) & 0xff;
            if (side == 0)
                in_();
        }
        t.check(r[0] == r[1] && !memcmp(a[0], a[1], sizeof a[0]), "Platform_CurrentDirectory", "");
        for (int side = 0; side < 2; side++) {
            memset(a[side], 0x5a, sizeof a[side]);
            if (side == 0)
                out_();
            r[side] = at<DWORD(__fastcall *)(char *, DWORD)>(0x4d2370)(a[side], n) & 0xff;
            if (side == 0)
                in_();
        }
        t.check(r[0] == r[1] && !memcmp(a[0], a[1], sizeof a[0]), "Platform_ExePath", "");
    }
    const DWORD lengths[] = {1, 3, 8, 0x40};
    for (DWORD n : lengths) {
        for (int side = 0; side < 2; side++) {
            memset(a[side], 0x5a, sizeof a[side]);
            if (side == 0)
                out_();
            r[side] = at<DWORD(__fastcall *)(char *, DWORD)>(0x4d2390)(a[side], n);
            if (side == 0)
                in_();
            r[side] = (r[side] & 0xff) | (r[side] >> 8 == pv(a[side]) >> 8 ? 0x100 : 0);
        }
        char d[80];
        wsprintfA(d, "[ExeBaseName %u: %x / %x \"%.20s\"]", n, r[0], r[1], a[1]);
        t.check(r[0] == r[1] && !memcmp(a[0], a[1], sizeof a[0]), "ExeBaseName", d);
    }
    const DWORD fns[] = {0x4d2490, 0x4d2530};
    for (DWORD f : fns) {
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                out_();
            r[side] = at<DWORD(__cdecl *)()>(f)();
            if (side == 0)
                in_();
        }
        char d[80];
        wsprintfA(d, "[%x: %x / %x]", f, r[0], r[1]);
        t.check(r[0] == r[1], "Platform", d);
    }
    for (unsigned seed = 1; seed <= (unsigned)scaled(200); seed++) {
        int m[2][2];
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            const int x = (int)random() >> (random() % 24), y = (int)random() >> (random() % 24), z = (int)random() >> (random() % 28);
            const DWORD shift = random() % 40;
            if (side == 0)
                out_();
            m[side][0] = at<int(__fastcall *)(int, int, DWORD)>(0x4d2620)(x, z, shift);
            m[side][1] = at<int(__fastcall *)(int, int, int)>(0x4d2640)(x, y, z);
            if (side == 0)
                in_();
        }
        t.check(!memcmp(m[0], m[1], sizeof m[0]), "MulDiv", "[0x4d2620, 0x4d2640]");
    }
}

// ---- The handle heap ------------------------------------------------------------------------------------------

constexpr int NH = 12;
alignas(16) BYTE g_heap[2][0x44];
DWORD g_handles[2][NH];

DWORD g_old_base[2];   // the block before the destructor freed it (the tree root still points into it)
DWORD norm(DWORD v, int side)
{
    const DWORD base = fl<DWORD>(g_heap[side], 8) ? fl<DWORD>(g_heap[side], 8) : g_old_base[side], size = fl<DWORD>(g_heap[side], 4);
    if (base && v >= base && v < base + size)
        return 0x10000000u + (v - base);
    if (v >= pv(g_heap[side]) && v < pv(g_heap[side]) + 0x44)
        return 0x20000000u + (v - pv(g_heap[side]));
    if (v >= pv(g_handles[side]) && v < pv(g_handles[side]) + sizeof g_handles[side])
        return 0x30000000u + (v - pv(g_handles[side]));
    return v;
}
unsigned heap_state(int side)
{
    unsigned h = 2166136261u;
    for (DWORD i = 0; i < 0x44; i += 4) {
        const DWORD v = i == 8 ? 0 : norm(fl<DWORD>(g_heap[side], i), side);
        h = fnv(h, &v, 4);
    }
    for (DWORD v : g_handles[side]) {
        const DWORD n = norm(v, side);
        h = fnv(h, &n, 4);
    }
    const DWORD base = fl<DWORD>(g_heap[side], 8), size = fl<DWORD>(g_heap[side], 4);
    for (DWORD i = 0; base && i + 4 <= size; i += 4) {
        const DWORD n = norm(*at<DWORD *>(base + i), side);
        h = fnv(h, &n, 4);
    }
    return h;
}

// The normalized words of a side's heap (the object, the handles, the memory) for the report of a difference
DWORD g_words[2][0x900];
int g_nwords[2];
void heap_words(int side)
{
    int n = 0;
    for (DWORD i = 0; i < 0x44; i += 4)
        g_words[side][n++] = i == 8 ? 0 : norm(fl<DWORD>(g_heap[side], i), side);
    for (DWORD v : g_handles[side])
        g_words[side][n++] = norm(v, side);
    const DWORD base = fl<DWORD>(g_heap[side], 8), size = fl<DWORD>(g_heap[side], 4);
    for (DWORD i = 0; base && i + 4 <= size && n < 0x900; i += 4)
        g_words[side][n++] = norm(*at<DWORD *>(base + i), side);
    g_nwords[side] = n;
}
DWORD g_ops[2][48];

void test_heap(Tally &t)
{
    for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++) {
        unsigned states[2][48];
        DWORD results[2][48];
        int steps = 0;
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            memset(g_heap[side], 0xcc, sizeof g_heap[side]);
            memset(g_handles[side], 0, sizeof g_handles[side]);
            g_old_base[side] = 0;
            const DWORD size = 0x200 + 0x40 * (random() % 24);
            if (side == 0)
                out_();
            at<void *(THISCALL *)(void *, DWORD)>(0x4d27c0)(g_heap[side], size);
            if (side == 0)
                in_();
            // the free memory a pattern, the first block's list links (not written by the constructor) too
            memset(at<void *>(fl<DWORD>(g_heap[side], 8) + 0x30), 0xa5, size - 0x30);
            memset(at<void *>(fl<DWORD>(g_heap[side], 8)), 0xa5, 8);
            steps = 0;
            for (int k = 0; k < 40; k++) {
                // sizes of whole dwords: the heap does not round, and the comparison maps pointers at dword offsets only
                const DWORD op = random() % 16, i = random() % NH, n = (random() % 3 ? random() % 0xc0 : random() % 0x400) & ~3u;
                const DWORD flags = random() % 3 == 0 ? 1 : 0;
                DWORD *handle = &g_handles[side][i];
                BYTE *b = at<BYTE *>(*handle);
                DWORD r = 0;
                if (side == 0)
                    out_();
                if (op < 5) {
                    at<void(THISCALL *)(void *, DWORD, void *, DWORD)>(0x4d2680)(handle, n, g_heap[side], flags);
                } else if (op < 7) {
                    if (!b)
                        r = at<DWORD(THISCALL *)(void *, DWORD, void *, DWORD)>(0x4d2980)(g_heap[side], n, handle, flags) & 0xff;
                } else if (op < 9) {
                    if (b)
                        at<void(THISCALL *)(void *, void *)>(0x4d2a70)(g_heap[side], b);
                } else if (op < 11) {
                    if (b) {
                        fl<DWORD>(b, 8) += random() % 3;   // locks
                        r = norm(pv(at<BYTE *(__stdcall *)(void *)>(0x4d2bc0)(b + 0x20)), side);
                    }
                } else if (op < 12) {
                    if (fl<DWORD>(g_heap[side], 4) - fl<DWORD>(g_heap[side], 0) >= 0x30)   // as the callers: room for a free block
                        at<void(THISCALL *)(void *)>(0x4d28e0)(g_heap[side]);
                } else if (op < 13) {
                    if (random() % 4 == 0)
                        at<void(THISCALL *)(void *)>(0x4d2b50)(g_heap[side]);
                } else if (op < 14) {
                    r = norm(pv(at<DWORD *(THISCALL *)(DWORD *)>(0x4d2660)(handle)), side);
                } else {
                    at<void(THISCALL *)(void *)>(0x4d2670)(handle);   // not hookable: frees through 0x4d2a70
                }
                if (side == 0)
                    in_();
                states[side][steps] = heap_state(side);
                results[side][steps] = r;
                g_ops[side][steps] = op << 24 | i << 16 | n;
                if (steps == 0)
                    heap_words(side);
                steps++;
            }
            g_old_base[side] = fl<DWORD>(g_heap[side], 8);
            if (side == 0)
                out_();
            at<void(THISCALL *)(void *)>(0x4d2880)(g_heap[side]);
            if (side == 0)
                in_();
            states[side][steps] = heap_state(side);
            results[side][steps] = fl<DWORD>(g_heap[side], 8);
        }
        int first = -1;
        for (int k = 0; k <= steps && first < 0; k++)
            if (states[0][k] != states[1][k] || results[0][k] != results[1][k])
                first = k;
        if (first == 0 && t.mismatches < 3) {
            log("    heap seed %u step 0: op %x / %x, result %x / %x, %d / %d words", seed, g_ops[0][0], g_ops[1][0], results[0][0], results[1][0],
                g_nwords[0], g_nwords[1]);
            int shown = 0;
            for (int w = 0; w < g_nwords[0] && w < g_nwords[1] && shown < 6; w++)
                if (g_words[0][w] != g_words[1][w]) {
                    log("      word %d: %x / %x", w, g_words[0][w], g_words[1][w]);
                    shown++;
                }
        }
        if (first > 0 && t.mismatches < 3)
            log("    heap seed %u step %d: op %x / %x, result %x / %x", seed, first, g_ops[0][first], g_ops[1][first], results[0][first],
                results[1][first]);
        char d[120];
        wsprintfA(d, "[heap seed %u: first different step %d of %d]", seed, first, steps);
        t.check(first < 0, "HandleHeap", d);
    }
}

}  // namespace

ZAX_CHECKS(app_base)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    invcheck::g_hook_table = hooks_app_base;
    Tally app{"app_base: application base (defaults, steps, setters, timers)"}, line{"app_base: command line"},
        platform{"app_base: Win32 helpers"}, heap{"app_base: handle heap"};
    test_defaults(app);
    test_steps(app);
    test_small(app);
    test_timers(app);
    app.report();
    test_command_line(line);
    line.report();
    test_platform(platform);
    platform.report();
    test_heap(heap);
    heap.report();
    log("  app_base: %u ms", GetTickCount() - start);
}

}  // namespace zax
