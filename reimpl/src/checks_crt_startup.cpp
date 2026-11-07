// Checks of the C runtime's start-up and signal helpers (crt_startup.cpp, phase 5b, docs/DECOMPILATION.md 11)
// against the exe's functions through the trampolines, in the game thread. Only what can run again without
// taking the running game apart: the functions that build the heap, the lock table, the TLS slot, the
// low-level file table, the stream table or run the initializers (_heap_init, _mtinit, _mtinitlocks,
// _ioinit, __initstdio, _cinit, _fcloseall, __endstdio, abort) are compared by the logged start against
// /Original instead (api_trace.cpp, TraceApi=1). The state a case changes is saved before it and put back
// between the two sides.
//  - parse_cmdline (counting and filling) and _setargv on random command lines (quotes, "" in quotes,
//    backslashes before quotes, tabs, high bytes), _wincmdln on the same lines, _setenvp on random
//    environment blocks;
//  - _setmbcp (the known East Asian code pages from the runtime's table, code pages from GetCPInfo, single
//    byte, unknown, the system's -2/-3/-4), setSBCS, setSBUpLow, __initmbctable, getSystemCP, CPtoLCID,
//    x_ismbbtype and _ismbblead under each of those tables;
//  - __crtGetEnvironmentStringsA (wide and ANSI path), __heap_select, _GetLinkerVersion, __sbh_heap_init,
//    __onexitinit, __inittime (a fixed GetSystemTimeAsFileTime), _flushall, __inittermination and
//    __termination, _ValidateExecute, _positive;
//  - xcptlookup, siglookup, _XcptFilter and raise on a private action table with recording handlers (never
//    the default action, which ends the process), __CxxUnhandledExceptionFilter with a recording previous
//    filter.
#include "check_util.h"
#include "crt_common.h"

WINIMPORT char *WINAPI lstrcpynA(char *out, const char *s, int n);

namespace zax {
namespace {

template <typename F> F orig(DWORD a) { return original<F>(a); }
template <typename F> F mine(DWORD a) { return at<F>(a); }

inline void exe_free(void *p) { at<void(__cdecl *)(void *)>(0x5d33e0)(p); }
inline void *exe_malloc(size_t n) { return at<void *(__cdecl *)(size_t)>(0x5d164f)(n); }

bool eq(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

// A saved block of the exe's data, put back on demand
struct Saved {
    DWORD address;
    DWORD size;
    BYTE bytes[0x300];
    void save(DWORD a, DWORD n)
    {
        address = a;
        size = n;
        memcpy(bytes, at<void *>(a), n);
    }
    void load() const { memcpy(at<void *>(address), bytes, size); }
};

// ---- command line and environment ------------------------------------------------------------------------

constexpr DWORD ACMDLN = 0x717544, ASPENVP = 0x7158d8, ENVIRON = 0x71589c;
constexpr DWORD PGMPTR = 0x7158ac, ARGV = 0x715894, ARGC = 0x715890;

void random_cmdline(char *s, int max)
{
    static const char pieces[] = "ab \t\"\\x\xe4\xfc C:\\Zax\\Zax.exe/Map";
    const int len = (int)(random() % (unsigned)max);
    int i = 0;
    if (random() % 2)
        s[i++] = '"';
    for (; i < len; i++)
        s[i] = pieces[random() % (sizeof pieces - 1)];
    s[i] = 0;
}

void check_cmdline(Tally &t, int count)
{
    using Parse = void(__cdecl *)(char *, char **, char *, int *, int *);
    using Cmdln = char *(__cdecl *)();
    char d[600];
    char line[64];
    char *const saved_acmdln = *at<char **>(ACMDLN);
    for (int k = 0; k < count; k++) {
        random_cmdline(line, 60);
        // counting, then filling into a buffer of that size (both sides from the same text)
        int na[2] = {}, nc[2] = {}, fa[2] = {}, fc[2] = {};
        BYTE out[2][64 * 4 + 128];
        for (int side = 0; side < 2; side++) {
            const Parse f = side ? mine<Parse>(0x5daef2) : orig<Parse>(0x5daef2);
            char text[64];
            memcpy(text, line, sizeof text);
            f(text, nullptr, nullptr, &na[side], &nc[side]);
            memset(out[side], 0xcc, sizeof out[side]);
            char **argv = reinterpret_cast<char **>(out[side]);
            if (na[side] <= 64 && nc[side] <= 128)
                f(text, argv, reinterpret_cast<char *>(argv + na[side]), &fa[side], &fc[side]);
            // the pointers as offsets into the buffer, so both sides compare
            for (int i = 0; i < na[side] && i < 64; i++)
                if (argv[i])
                    argv[i] = reinterpret_cast<char *>(reinterpret_cast<BYTE *>(argv[i]) - out[side]);
        }
        wsprintfA(d, "[parse_cmdline \"%s\": %d args %d chars, %d %d / %d args %d chars, %d %d]", line, na[0], nc[0], fa[0], fc[0],
                  na[1], nc[1], fa[1], fc[1]);
        t.check(na[0] == na[1] && nc[0] == nc[1] && fa[0] == fa[1] && fc[0] == fc[1] && !memcmp(out[0], out[1], sizeof out[0]), "", d);
        // _wincmdln on the same text
        *at<char **>(ACMDLN) = line;
        const char *wa = orig<Cmdln>(0x5dad48)(), *wb = mine<Cmdln>(0x5dad48)();
        wsprintfA(d, "[_wincmdln \"%s\": %d / %d]", line, (int)(wa - line), (int)(wb - line));
        t.check(wa == wb, "", d);
        // _setargv: __argv, __argc, _pgmptr (the new argv blocks freed, the game's put back)
        if (k % 8 == 0) {
            char **const saved_argv = *at<char ***>(ARGV);
            const int saved_argc = *at<int *>(ARGC);
            char *const saved_pgm = *at<char **>(PGMPTR);
            char texts[2][700];
            int argc[2];
            for (int side = 0; side < 2; side++) {
                (side ? mine<int(__cdecl *)()>(0x5dae59) : orig<int(__cdecl *)()>(0x5dae59))();
                argc[side] = *at<int *>(ARGC);
                char *p = texts[side];
                char **argv = *at<char ***>(ARGV);
                for (int i = 0; argv[i] && p < texts[side] + 600; i++) {
                    lstrcpynA(p, argv[i], 80);
                    p += lstrlenA(p) + 1;
                }
                *p = 0;
                exe_free(argv);
            }
            *at<char ***>(ARGV) = saved_argv;
            *at<int *>(ARGC) = saved_argc;
            const bool pgm_same = eq(*at<char **>(PGMPTR), saved_pgm);
            *at<char **>(PGMPTR) = saved_pgm;
            wsprintfA(d, "[_setargv \"%s\": argc %d / %d]", line, argc[0], argc[1]);
            t.check(argc[0] == argc[1] && !memcmp(texts[0], texts[1], (size_t)(lstrlenA(texts[0]) + 1)) && pgm_same, "", d);
        }
    }
    *at<char **>(ACMDLN) = saved_acmdln;
}

// _setenvp on random environment blocks (entries with '=' at the start left out)
void check_setenvp(Tally &t, int count)
{
    char d[600];
    char **const saved_environ = *at<char ***>(ENVIRON);
    for (int k = 0; k < count; k++) {
        char block[200];
        int n = 0;
        const int entries = (int)(random() % 8);
        for (int e = 0; e < entries; e++) {
            const int len = 1 + (int)(random() % 12);
            for (int i = 0; i < len; i++)
                block[n++] = (char)(random() % 5 == 0 ? '=' : 'A' + random() % 26);
            block[n++] = 0;
        }
        block[n++] = 0;
        char texts[2][300];
        for (int side = 0; side < 2; side++) {
            char *copy = static_cast<char *>(exe_malloc((size_t)n));
            memcpy(copy, block, (size_t)n);
            *at<char **>(ASPENVP) = copy;
            (side ? mine<void(__cdecl *)()>(0x5dada0) : orig<void(__cdecl *)()>(0x5dada0))();
            char **env = *at<char ***>(ENVIRON);
            char *p = texts[side];
            for (int i = 0; env[i]; i++) {
                lstrcpynA(p, env[i], 40);
                p += lstrlenA(p) + 1;
                exe_free(env[i]);
            }
            *p = 0;
            exe_free(env);
            wsprintfA(d, "[_setenvp %d entries: block %x]", entries, *at<DWORD *>(ASPENVP));
            t.check(*at<DWORD *>(ASPENVP) == 0, "", d);
        }
        *at<char ***>(ENVIRON) = saved_environ;
        int len0 = 0;
        while (texts[0][len0] || texts[0][len0 + 1])
            len0++;
        wsprintfA(d, "[_setenvp %d entries: \"%s\"... / \"%s\"...]", entries, texts[0], texts[1]);
        t.check(!memcmp(texts[0], texts[1], (size_t)len0 + 2), "", d);
    }
    *at<char ***>(ENVIRON) = saved_environ;
}

// __crtGetEnvironmentStringsA: the wide path (kind 0 and 1) and the ANSI path (kind 2); the blocks compared
void check_env_strings(Tally &t)
{
    using Get = char *(__cdecl *)();
    char d[200];
    int &kind = *at<int *>(0x715ba4);
    const int saved = kind;
    for (int k = 0; k < 4; k++) {
        char *blocks[2];
        int kinds[2];
        for (int side = 0; side < 2; side++) {
            kind = k;
            blocks[side] = (side ? mine<Get>(0x5db0a6) : orig<Get>(0x5db0a6))();
            kinds[side] = kind;
        }
        bool same = (blocks[0] == nullptr) == (blocks[1] == nullptr) && kinds[0] == kinds[1];
        int len = 0;
        if (same && blocks[0]) {
            while (blocks[0][len] || blocks[0][len + 1])
                len++;
            same = !memcmp(blocks[0], blocks[1], (size_t)len + 2);
        }
        wsprintfA(d, "[__crtGetEnvironmentStringsA kind %d: %d bytes, kind %d / kind %d]", k, len + 2, kinds[0], kinds[1]);
        t.check(same, "", d);
        for (char *b : blocks)
            if (b)
                exe_free(b);
    }
    kind = saved;
}

// ---- the multibyte code page ------------------------------------------------------------------------------

constexpr DWORD MBCTYPE = 0x7162e0, MBCASEMAP = 0x7161e0, MBCODEPAGE = 0x7161b0, ISMBCODEPAGE = 0x7161cc,
                MBLCID = 0x7163e4, MBULINFO = 0x7161c0, SYSTEM_SET = 0x715c18, MBCTYPE_INITIALIZED = 0x717554;

// The code page state: _mbctype, _mbcasemap, the code page, the flags, the locale, __mbulinfo
struct CodePageState {
    BYTE type[0x101], casemap[0x100], ulinfo[12];
    int codepage, ismb, lcid, system_set, initialized;
    void save()
    {
        memcpy(type, at<void *>(MBCTYPE), sizeof type);
        memcpy(casemap, at<void *>(MBCASEMAP), sizeof casemap);
        memcpy(ulinfo, at<void *>(MBULINFO), sizeof ulinfo);
        codepage = *at<int *>(MBCODEPAGE);
        ismb = *at<int *>(ISMBCODEPAGE);
        lcid = *at<int *>(MBLCID);
        system_set = *at<int *>(SYSTEM_SET);
        initialized = *at<int *>(MBCTYPE_INITIALIZED);
    }
    void load() const
    {
        memcpy(at<void *>(MBCTYPE), type, sizeof type);
        memcpy(at<void *>(MBCASEMAP), casemap, sizeof casemap);
        memcpy(at<void *>(MBULINFO), ulinfo, sizeof ulinfo);
        *at<int *>(MBCODEPAGE) = codepage;
        *at<int *>(ISMBCODEPAGE) = ismb;
        *at<int *>(MBLCID) = lcid;
        *at<int *>(SYSTEM_SET) = system_set;
        *at<int *>(MBCTYPE_INITIALIZED) = initialized;
    }
    bool same(const CodePageState &o) const
    {
        return !memcmp(type, o.type, sizeof type) && !memcmp(casemap, o.casemap, sizeof casemap) &&
               !memcmp(ulinfo, o.ulinfo, sizeof ulinfo) && codepage == o.codepage && ismb == o.ismb && lcid == o.lcid &&
               system_set == o.system_set && initialized == o.initialized;
    }
};

constexpr int MASKS[] = {4, 8, 0x10, 0x20};

void check_codepages(Tally &t)
{
    static const int pages[] = {-3, -2, -4, 1252, 932, 936, 949, 950, 1361, 0, 437, 850, 1250, 1251, 65001, 12345, -3};
    char d[200];
    CodePageState game, start, a, b;
    game.save();
    for (int k = 0; k < (int)(sizeof pages / sizeof pages[0]); k++) {
        // from the previous case's table, the code page forgotten (else _setmbcp returns at once) every other time
        start.save();
        if (k % 2)
            start.codepage = -1;
        int ra = 0, rb = 0;
        start.load();
        ra = orig<int(__cdecl *)(int)>(0x5dcb8e)(pages[k]);
        a.save();
        start.load();
        rb = mine<int(__cdecl *)(int)>(0x5dcb8e)(pages[k]);
        b.save();
        wsprintfA(d, "[_setmbcp %d: %d cp %d mb %d lcid %x / %d cp %d mb %d lcid %x]", pages[k], ra, a.codepage, a.ismb, a.lcid, rb,
                  b.codepage, b.ismb, b.lcid);
        t.check(ra == rb && a.same(b), "", d);
        // the byte classes under this table
        for (unsigned c = 0; c < 0x100; c++)
            for (int mask : MASKS) {
                const int cmask = (int)(c * 7 % 3 == 0 ? 0 : 0x103);
                const int xa = orig<int(__cdecl *)(unsigned, int, int)>(0x5dcb5d)(c, cmask, mask);
                const int xb = mine<int(__cdecl *)(unsigned, int, int)>(0x5dcb5d)(c, cmask, mask);
                if (xa != xb) {
                    wsprintfA(d, "[x_ismbbtype %x %x %x under %d: %d / %d]", c, cmask, mask, pages[k], xa, xb);
                    t.check(false, "", d);
                }
            }
        bool leads = true;
        for (unsigned c = 0; c < 0x100; c++)
            leads = leads && orig<int(__cdecl *)(unsigned)>(0x5dcb4c)(c) == mine<int(__cdecl *)(unsigned)>(0x5dcb4c)(c);
        wsprintfA(d, "[_ismbblead under %d]", pages[k]);
        t.check(leads, "", d);
        // setSBUpLow and setSBCS alone from this table
        start.save();
        orig<void(__cdecl *)()>(0x5dcde1)();
        a.save();
        start.load();
        mine<void(__cdecl *)()>(0x5dcde1)();
        b.save();
        wsprintfA(d, "[setSBUpLow under %d]", pages[k]);
        t.check(a.same(b), "", d);
        start.load();
    }
    // setSBCS, __initmbctable (once: with the flag cleared, then set)
    start.save();
    orig<void(__cdecl *)()>(0x5dcdb8)();
    a.save();
    start.load();
    mine<void(__cdecl *)()>(0x5dcdb8)();
    b.save();
    t.check(a.same(b), "", "[setSBCS]");
    for (int flag = 0; flag < 2; flag++) {
        start = game;
        start.initialized = flag;
        start.codepage = -1;
        start.load();
        orig<void(__cdecl *)()>(0x5dcf66)();
        a.save();
        start.load();
        mine<void(__cdecl *)()>(0x5dcf66)();
        b.save();
        wsprintfA(d, "[__initmbctable flag %d]", flag);
        t.check(a.same(b), "", d);
    }
    game.load();
    // getSystemCP, CPtoLCID
    for (int cp = -6; cp < 1400; cp += cp < 900 ? 37 : 1) {
        const int sa = orig<int(__cdecl *)(int)>(0x5dcd3b)(cp), fa = *at<int *>(SYSTEM_SET);
        const int sb = mine<int(__cdecl *)(int)>(0x5dcd3b)(cp), fb = *at<int *>(SYSTEM_SET);
        const int la = orig<int(__cdecl *)(int)>(0x5dcd85)(cp), lb = mine<int(__cdecl *)(int)>(0x5dcd85)(cp);
        wsprintfA(d, "[getSystemCP/CPtoLCID %d: %d %d %x / %d %d %x]", cp, sa, fa, la, sb, fb, lb);
        t.check(sa == sb && fa == fb && la == lb, "", d);
    }
    game.load();
}

// ---- the rest of the start and the signals ----------------------------------------------------------------

DWORD g_fixed_time[2] = {0x9b3c1e00, 0x01c7f3a4};
void WINAPI fixed_system_time(DWORD *ft)
{
    ft[0] = g_fixed_time[0];
    ft[1] = g_fixed_time[1];
}

void check_misc(Tally &t)
{
    char d[200];
    // __heap_select, _GetLinkerVersion
    const int ha = orig<int(__cdecl *)()>(0x5d67ee)(), hb = mine<int(__cdecl *)()>(0x5d67ee)();
    wsprintfA(d, "[__heap_select: %d / %d]", ha, hb);
    t.check(ha == hb, "", d);
    DWORD va = 0xcccccccc, vb = 0xcccccccc;
    orig<void(__cdecl *)(DWORD *)>(0x5d67c1)(&va);
    mine<void(__cdecl *)(DWORD *)>(0x5d67c1)(&vb);
    wsprintfA(d, "[_GetLinkerVersion: %x / %x]", va, vb);
    t.check(va == vb, "", d);
    // __sbh_heap_init: the V6 small-block globals (unused with the system heap) and the header list freed
    {
        Saved sbh;
        sbh.save(0x716504, 0x1c);   // __sbh_sizeHeaderList .. __sbh_threshold
        BYTE after[2][0x1c];
        int r[2];
        for (int side = 0; side < 2; side++) {
            r[side] = (side ? mine<int(__cdecl *)(int)>(0x5d6993) : orig<int(__cdecl *)(int)>(0x5d6993))(0x3f8);
            void *list = *at<void **>(0x716518);
            memcpy(after[side], at<void *>(0x716504), 0x1c);
            if (list) {
                crt::api<BOOL(WINAPI *)(HANDLE, DWORD, void *)>(crt::iat::HeapFree)(*at<HANDLE *>(crt::CRT_HEAP), 0, list);
                // the list pointers (the same offsets) compared as present or not
                static const DWORD offsets[] = {0x08, 0x14};
                for (DWORD off : offsets)
                    *reinterpret_cast<DWORD *>(after[side] + off) = *reinterpret_cast<DWORD *>(after[side] + off) == (DWORD)(uintptr_t)list;
            }
            sbh.load();
        }
        wsprintfA(d, "[__sbh_heap_init: %d / %d]", r[0], r[1]);
        t.check(r[0] == r[1] && !memcmp(after[0], after[1], 0x1c), "", d);
    }
    // __onexitinit on swapped table pointers
    {
        Saved table;
        table.save(0x717558, 8);
        DWORD shape[2][2];
        for (int side = 0; side < 2; side++) {
            (side ? mine<void(__cdecl *)()>(0x5d0875) : orig<void(__cdecl *)()>(0x5d0875))();
            DWORD *begin = *at<DWORD **>(0x71755c), *end = *at<DWORD **>(0x717558);
            shape[side][0] = begin == end;
            shape[side][1] = begin ? *begin : 0xffffffff;
            exe_free(begin);
            table.load();
        }
        t.check(!memcmp(shape[0], shape[1], sizeof shape[0]) && shape[0][0] == 1, "", "[__onexitinit]");
    }
    // __inittime with a fixed system time
    {
        Saved tick;
        tick.save(0x7158e8, 8);
        ImportPatch p;
        patch_import(p, 0x5ea0b0, (void *)&fixed_system_time);
        unsigned long long ta, tb;
        orig<void(__cdecl *)()>(0x5d3710)();
        ta = *at<unsigned long long *>(0x7158e8);
        mine<void(__cdecl *)()>(0x5d3710)();
        tb = *at<unsigned long long *>(0x7158e8);
        unpatch_import(p);
        tick.load();
        wsprintfA(d, "[__inittime: %x%08x / %x%08x]", (DWORD)(ta >> 32), (DWORD)ta, (DWORD)(tb >> 32), (DWORD)tb);
        t.check(ta == tb, "", d);
    }
    // _flushall: the count of streams flushed
    {
        const int fa = orig<int(__cdecl *)()>(0x5d190f)(), fb = mine<int(__cdecl *)()>(0x5d190f)();
        wsprintfA(d, "[_flushall: %d / %d]", fa, fb);
        t.check(fa == fb, "", d);
    }
    // __inittermination / __termination: the filter in and back (the previous one kept)
    {
        Saved prev;
        prev.save(0x715c28, 4);
        using SetFilter = DWORD(WINAPI *)(DWORD);
        const SetFilter set = crt::api<SetFilter>(0x5ea0a0);
        DWORD seen[2][3];
        for (int side = 0; side < 2; side++) {
            (side ? mine<void(__cdecl *)()>(0x5dd051) : orig<void(__cdecl *)()>(0x5dd051))();
            seen[side][0] = *at<DWORD *>(0x715c28);
            const DWORD current = set(0x5dd00b);
            seen[side][1] = current;
            (side ? mine<void(__cdecl *)()>(0x5dd062) : orig<void(__cdecl *)()>(0x5dd062))();
            seen[side][2] = set(seen[side][0]);
            set(seen[side][2]);
            prev.load();
        }
        wsprintfA(d, "[__inittermination/__termination: %x %x %x / %x %x %x]", seen[0][0], seen[0][1], seen[0][2], seen[1][0], seen[1][1],
                  seen[1][2]);
        t.check(!memcmp(seen[0], seen[1], sizeof seen[0]), "", d);
    }
    // _ValidateExecute
    {
        const DWORD addresses[] = {0x401000, 0x5dd06f, 0x62c000, 0, 1, 0x7ffe0000, (DWORD)(uintptr_t)&fixed_system_time};
        for (DWORD a : addresses) {
            const int ra = orig<int(__cdecl *)(DWORD)>(0x5dd06f)(a), rb = mine<int(__cdecl *)(DWORD)>(0x5dd06f)(a);
            wsprintfA(d, "[_ValidateExecute %x: %d / %d]", a, ra, rb);
            t.check(ra == rb, "", d);
        }
    }
    // _positive on doubles of every kind, the status word too
    for (int k = 0; k < scaled(400); k++) {
        static const unsigned long long specials[] = {0, 0x8000000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000000ull,
                                                      0x7ff8000000000000ull, 0x7ff0000000000001ull, 1, 0x800fffffffffffffull};
        const unsigned long long v = k < 8 ? specials[k] : (unsigned long long)random() << 32 | random();
        WORD swa, swb;
        __asm__ volatile("fnclex");
        const int ra = orig<int(__cdecl *)(const void *)>(0x5d3e70)(&v);
        __asm__ volatile("fnstsw %0\n\tfnclex" : "=m"(swa));
        const int rb = mine<int(__cdecl *)(const void *)>(0x5d3e70)(&v);
        __asm__ volatile("fnstsw %0\n\tfnclex" : "=m"(swb));
        wsprintfA(d, "[_positive %08x%08x: %d %x / %d %x]", (DWORD)(v >> 32), (DWORD)v, ra, swa & 0x3f, rb, swb & 0x3f);
        t.check(ra == rb && (swa & 0x3f) == (swb & 0x3f), "", d);
    }
}

struct XcptAction {
    DWORD xcptnum;
    int signum;
    DWORD action;
};
constexpr DWORD XCPT_ACT_TAB_COUNT = 0x62c484, GLOBAL_ACTIONS = 0x715c2c;   // SIGINT, SIGBREAK, SIGABRT, SIGTERM

// What the recording handlers saw: the arguments and the thread's exception pointers and FPE code then
struct Seen {
    int calls, a, b;
    DWORD infoptrs;
    int fpecode;
};
Seen g_seen;
BYTE *g_ptd;
void __cdecl record_one(int a)
{
    g_seen.calls++;
    g_seen.a = a;
    g_seen.infoptrs = *reinterpret_cast<DWORD *>(g_ptd + 0x54);
    g_seen.fpecode = *reinterpret_cast<int *>(g_ptd + 0x58);
}
void __cdecl record_two(int a, int b)
{
    record_one(a);
    g_seen.b = b;
}
int g_filter_calls;
void *g_filter_arg;
int WINAPI record_filter(void *p)
{
    g_filter_calls++;
    g_filter_arg = p;
    return 0x1234;
}

// The thread's state the signal functions change: its action table (a private copy), the exception pointers,
// the FPE code, the global actions
struct SignalState {
    XcptAction table[32];
    DWORD infoptrs;
    int fpecode;
    DWORD global[4];
    Seen seen;
    void save(int n)
    {
        memcpy(table, *reinterpret_cast<XcptAction **>(g_ptd + 0x50), (size_t)n * sizeof(XcptAction));
        infoptrs = *reinterpret_cast<DWORD *>(g_ptd + 0x54);
        fpecode = *reinterpret_cast<int *>(g_ptd + 0x58);
        memcpy(global, at<void *>(GLOBAL_ACTIONS), sizeof global);
        seen = g_seen;
    }
    void load(int n) const
    {
        memcpy(*reinterpret_cast<XcptAction **>(g_ptd + 0x50), table, (size_t)n * sizeof(XcptAction));
        *reinterpret_cast<DWORD *>(g_ptd + 0x54) = infoptrs;
        *reinterpret_cast<int *>(g_ptd + 0x58) = fpecode;
        memcpy(at<void *>(GLOBAL_ACTIONS), global, sizeof global);
        g_seen = seen;
    }
    bool same(const SignalState &o, int n) const
    {
        return !memcmp(table, o.table, (size_t)n * sizeof(XcptAction)) && infoptrs == o.infoptrs && fpecode == o.fpecode &&
               !memcmp(global, o.global, sizeof global) && !memcmp(&seen, &o.seen, sizeof seen);
    }
};

DWORD random_action(bool fpe)
{
    switch (random() % 3) {
    case 0: return 1;   // SIG_IGN
    case 1: return 5;   // SIG_ACKNOWLEDGE (only meaningful for _XcptFilter)
    default: return (DWORD)(uintptr_t)(fpe ? (void *)&record_two : (void *)&record_one);
    }
}

void check_signals(Tally &t, int count)
{
    char d[300];
    g_ptd = reinterpret_cast<BYTE *>(at<void *(__cdecl *)()>(0x5d4f28)());
    const int n = *at<int *>(XCPT_ACT_TAB_COUNT);
    if (n > 32) {
        t.check(false, "", "[action table larger than 32 entries]");
        return;
    }
    XcptAction *const game_table = *reinterpret_cast<XcptAction **>(g_ptd + 0x50);
    XcptAction table[32];
    memcpy(table, game_table, (size_t)n * sizeof(XcptAction));
    *reinterpret_cast<XcptAction **>(g_ptd + 0x50) = table;
    SignalState game, start, a, b;
    game.save(n);
    static const int signals[] = {2, 21, 22, 15, 4, 8, 11, 0, 3, -1, 99, 6};
    for (int k = 0; k < count; k++) {
        start = game;
        for (int i = 0; i < n; i++)
            start.table[i].action = random_action(start.table[i].signum == 8);
        for (DWORD &g : start.global)
            g = random() % 2 ? 1 : (DWORD)(uintptr_t)&record_one;
        start.infoptrs = random();
        start.fpecode = (int)(random() % 0x90);
        memset(&start.seen, 0, sizeof start.seen);
        // xcptlookup, siglookup
        const DWORD code = k % 3 ? table[random() % (unsigned)n].xcptnum : 0xc0000000 + random() % 0x100;
        const int sig = signals[random() % (sizeof signals / sizeof signals[0])];
        start.load(n);
        const void *xa = orig<void *(__cdecl *)(DWORD, XcptAction *)>(0x5dad0e)(code, table);
        const void *xb = mine<void *(__cdecl *)(DWORD, XcptAction *)>(0x5dad0e)(code, table);
        const void *sa = orig<void *(__cdecl *)(int, XcptAction *)>(0x5dd65d)(sig, table);
        const void *sb = mine<void *(__cdecl *)(int, XcptAction *)>(0x5dd65d)(sig, table);
        wsprintfA(d, "[xcptlookup %x / siglookup %d: %x %x / %x %x]", code, sig, (DWORD)(uintptr_t)xa, (DWORD)(uintptr_t)sa,
                  (DWORD)(uintptr_t)xb, (DWORD)(uintptr_t)sb);
        t.check(xa == xb && sa == sb, "", d);
        // _XcptFilter of an exception in the table
        const DWORD xcode = table[random() % (unsigned)n].xcptnum;
        const DWORD infoptrs = 0x12340000 + random() % 0x1000;
        start.load(n);
        const int fa = orig<int(__cdecl *)(DWORD, DWORD)>(0x5dabd0)(xcode, infoptrs);
        a.save(n);
        start.load(n);
        const int fb = mine<int(__cdecl *)(DWORD, DWORD)>(0x5dabd0)(xcode, infoptrs);
        b.save(n);
        wsprintfA(d, "[_XcptFilter %x: %d calls %d sig %d code %x / %d calls %d sig %d code %x]", xcode, fa, a.seen.calls, a.seen.a,
                  a.seen.b, fb, b.seen.calls, b.seen.a, b.seen.b);
        t.check(fa == fb && a.same(b, n), "", d);
        // raise (an action of 0 would end the process: never one)
        if (sig == 4 || sig == 8 || sig == 11)
            for (int i = 0; i < n; i++)
                if (start.table[i].action == 5)
                    start.table[i].action = 1;
        start.load(n);
        const int ra = orig<int(__cdecl *)(int)>(0x5dd4db)(sig);
        a.save(n);
        start.load(n);
        const int rb = mine<int(__cdecl *)(int)>(0x5dd4db)(sig);
        b.save(n);
        wsprintfA(d, "[raise %d: %d calls %d sig %d code %x / %d calls %d sig %d code %x]", sig, ra, a.seen.calls, a.seen.a, a.seen.b, rb,
                  b.seen.calls, b.seen.a, b.seen.b);
        t.check(ra == rb && a.same(b, n), "", d);
    }
    game.load(n);
    *reinterpret_cast<XcptAction **>(g_ptd + 0x50) = game_table;
    // __CxxUnhandledExceptionFilter: other exceptions to the previous filter (a recording one, none, one that
    // cannot be executed); never the C++ one (terminate)
    DWORD *const prev = at<DWORD *>(0x715c28);
    const DWORD saved_prev = *prev;
    static const DWORD records[][6] = {{0xc0000005, 0, 0, 0, 2, 0}, {0xe06d7363, 1, 0, 0, 2, 0x19930520},
                                       {0xe06d7363, 1, 0, 0, 3, 0x19930521}, {0x80000003, 0, 0, 0, 0, 0}};
    const DWORD filters[] = {(DWORD)(uintptr_t)&record_filter, 0, 1};
    for (const auto &rec : records)
        for (DWORD f : filters) {
            void *pointers[2] = {const_cast<DWORD *>(rec), nullptr};
            int calls[2];
            int r[2];
            for (int side = 0; side < 2; side++) {
                *prev = f;
                g_filter_calls = 0;
                g_filter_arg = nullptr;
                r[side] = (side ? mine<int(WINAPI *)(void **)>(0x5dd00b) : orig<int(WINAPI *)(void **)>(0x5dd00b))(pointers);
                calls[side] = g_filter_calls + (g_filter_arg == pointers ? 0x100 : 0);
            }
            wsprintfA(d, "[__CxxUnhandledExceptionFilter %x %d, filter %x: %x %x / %x %x]", rec[0], rec[4], f, r[0], calls[0], r[1], calls[1]);
            t.check(r[0] == r[1] && calls[0] == calls[1], "", d);
        }
    *prev = saved_prev;
}

}  // namespace

ZAX_CHECKS(crt_startup)
{
    (void)cls;
    (void)n;
    Tally a{"crt_startup: parse_cmdline, _setargv, _wincmdln, _setenvp, __crtGetEnvironmentStringsA"};
    Tally b{"crt_startup: _setmbcp, setSBCS, setSBUpLow, __initmbctable, getSystemCP, CPtoLCID, x_ismbbtype, _ismbblead"};
    Tally c{"crt_startup: __heap_select, _GetLinkerVersion, __sbh_heap_init, __onexitinit, __inittime, _flushall, filters, _positive"};
    Tally e{"crt_startup: xcptlookup, siglookup, _XcptFilter, raise, __CxxUnhandledExceptionFilter"};
    g_seed = 0x5daef2;
    check_cmdline(a, scaled(3000));
    check_setenvp(a, scaled(300));
    check_env_strings(a);
    a.report();
    check_codepages(b);
    b.report();
    check_misc(c);
    c.report();
    check_signals(e, scaled(1000));
    e.report();
}

}  // namespace zax
