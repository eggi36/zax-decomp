// Helpers shared by the checks (checks.cpp and the checks_<module>.cpp files): hashing, random
// numbers, the tally of cases, string states, calling original and reimplementation side by side,
// patching exe code and imports for the duration of a test, stand-ins for atexit and FreeLibrary.
//
// A module's own checks go into checks_<module>.cpp as ZAX_CHECKS(module) { ... }; build.sh collects
// them into check_list.inc and checks.cpp runs them in the game thread after the older checks.
#pragma once
#include "checks.h"
#include "class_info.h"
#include "zstring.h"

#define ZAX_CHECKS(module) void checks_##module(CClassInfo **cls, int n)

WINIMPORT DWORD WINAPI GetTickCount(void);

namespace zax {

inline unsigned fnv(unsigned h, const void *p, unsigned n)
{
    for (const BYTE *b = (const BYTE *)p; n--; b++)
        h = (h ^ *b) * 16777619u;
    return h;
}
inline unsigned fnv_text(unsigned h, const char *s) { return fnv(h, s, (unsigned)lstrlenA(s) + 1); }
inline unsigned bits(float f) { unsigned u; memcpy(&u, &f, 4); return u; }

// The game's random generator 0x7124c0 as the checks save and restore it: +0x10/+0x14 the two indices,
// from +0x18 a table of 250 dwords (successor indices 0x620004), up to 0x7128c0. Until 2026-10-25 the
// checks saved 0x200 bytes only; with the indices beyond entry 122 (the state on Assimilate Cave1) the
// first side changed the table for the second.
constexpr DWORD GAME_RANDOM_BYTES = 0x400;

inline unsigned g_seed = 4711;
inline unsigned random() { return g_seed = g_seed * 1664525u + 1013904223u; }

// [Reimpl] CheckScale= of Zax-Reimpl.ini in thousandths (1000 = all cases, the default); a batch's
// end-of-batch run uses e.g. CheckScale=0.1. Every check loop over random cases takes its count
// from scaled(), at least one case.
inline int g_check_scale = 1000;
inline int scaled(int n)
{
    if (g_check_scale >= 1000)
        return n;
    int m = n / 1000 * g_check_scale + n % 1000 * g_check_scale / 1000;   // no 64-bit division (no C runtime)
    return m < 1 ? 1 : m;
}

// Time of the last check report (or of the module's start, checks.cpp): every report logs the
// milliseconds since then, so a slow check stands out within its module (docs/DECOMPILATION.md 5)
inline DWORD g_check_mark;

struct Tally {
    const char *what;
    int cases = 0, mismatches = 0;
    void check(bool same, const char *cls, const char *detail = "")
    {
        cases++;
        if (!same && mismatches++ < 10)
            log("  %s differs: %s %s", what, cls, detail);
    }
    void report()
    {
        log("Check %s: %s (%d of %d differ)", what, mismatches ? "MISMATCH" : "same", mismatches, cases);
        const DWORD now = GetTickCount();
        log("    %u ms", now - g_check_mark);
        g_check_mark = now;
    }
};

// Every check module saves the hook sites it puts back to the original bytes in fixed arrays. With
// more hooks than room the module's checks would compare the reimplementation with itself and report
// "same"; until 2026-10-25 hooks_out() returned silently then. Now it is a failed check (logged once
// per module, "MISMATCH" like a Tally) and the caller skips putting hooks out.
// The exe's code section (.text 0x401000-0x5ea000) stays writable while the modules' checks run
// (checks.cpp). Every case puts the module's hooks out and in again around its original side; with
// two VirtualProtect calls per hook site that was most of a case's time (2026-10-25: about 2 x 150
// sites per case pair in the bigger modules, 5-20 ms per case on the VMs).
constexpr DWORD CODE_START = 0x401000, CODE_END = 0x5ea000;
// The end of Zax.exe's image (0x400000 + SizeOfImage 0x35d000; .rsrc ends at 0x75c340). The checks take values in
// [0x400000, IMAGE_END) for the exe's own code and data (compared raw) and other aligned values for heap pointers.
// Until 2026-10-31 the bound was 0x800000: on real x86 Windows 11 (WoW64) the heap's first blocks lie right after
// the image (descriptors and strings registered at startup at 0x77xxxx-0x7exxxx, e.g. 0x7c78ec), so heap pointers
// were taken for exe constants and compared raw; under the ARM emulation the heap starts higher and it never
// showed (docs/DECOMPILATION.md 12, "Seven checks that differ only on real x86").
constexpr DWORD IMAGE_END = 0x75d000;
inline bool g_code_writable;
inline void write_code(void *site, const void *bytes, DWORD n = 5)
{
    const DWORD a = (DWORD)(uintptr_t)site;
    if (g_code_writable && a >= CODE_START && a + n <= CODE_END) {
        memcpy(site, bytes, n);
    } else {
        DWORD old;
        VirtualProtect(site, n, PAGE_EXECUTE_READWRITE, &old);
        memcpy(site, bytes, n);
        VirtualProtect(site, n, old, &old);
    }
    FlushInstructionCache(GetCurrentProcess(), site, n);
}

// Real-data budget: a case kind over a level's objects takes at most `cap` (at least 3) of its `count`
// objects, a deterministic spread: the first, the last and every k-th in between. `index` counts the
// objects of that kind in level order (docs/DECOMPILATION.md 5, check run time).
inline bool spread_pick(int index, int count, int cap)
{
    if (count <= cap || index == 0 || index == count - 1)
        return true;
    const int step = (count + cap - 3) / (cap - 2);
    return index % step == 0;
}

inline const char *g_sites_reported[32];
inline int g_nsites_reported;
inline bool hook_sites_fit(const char *module, int n, int room)
{
    if (n <= room)
        return true;
    for (int i = 0; i < g_nsites_reported; i++)
        if (g_sites_reported[i] == module)
            return false;
    if (g_nsites_reported < 32)
        g_sites_reported[g_nsites_reported++] = module;
    log("Check %s hook sites: MISMATCH (%d hooks, room for %d; the checks would compare the "
        "reimplementation with itself)", module, n, room);
    return false;
}

// State of a string that must be equal after the same operation through original and reimplementation
// (buffer addresses differ, so: text, capacity, reference count, shared empty buffer, written in place)
struct StringState {
    char text[48];
    DWORD capacity, refs, length, hash;   // hash over the whole text
    bool is_empty_buffer, same_buffer;
};

inline StringState state(const ZString &z, const ZStringBuffer *before)
{
    StringState st;
    memset(&st, 0, sizeof st);
    if (!z.buffer) {
        st.refs = 0xffffffff;
        return st;
    }
    DWORD n = 0;
    while (z.buffer->text[n])
        n++;
    st.length = n;
    st.hash = fnv(2166136261u, z.buffer->text, n);
    for (DWORD i = 0; i < n && i < sizeof st.text - 1; i++)
        st.text[i] = z.buffer->text[i];
    st.capacity = z.buffer->capacity;
    st.refs = z.buffer->refs;
    st.is_empty_buffer = z.buffer == at<ZStringBuffer *>(ZSTRING_EMPTY);
    st.same_buffer = z.buffer == before;
    return st;
}

inline const char LONG_TEXT[] =
    "A rather long text that does not fit into any of the small buffers of the test strings, so the "
    "string has to allocate a new buffer from the pool; it is longer than 256 characters to reach the "
    "bigger pool sizes as well. Zax: The Alien Hunter, Reflexive Entertainment, 2001. End.";

// Original functions (trampolines)
struct StringOps {
    ZString *(THISCALL *ctor)(ZString *);
    ZString *(THISCALL *copy)(ZString *, const ZString *);
    void(THISCALL *dtor)(ZString *);
    ZString *(THISCALL *assign)(ZString *, const ZString *);
    ZString *(THISCALL *assign_text)(ZString *, const char *);
    void(THISCALL *assign_n)(ZString *, const char *, DWORD);
};

inline StringOps original_string_ops()
{
    return {original<ZString *(THISCALL *)(ZString *)>(0x4dec90),
            original<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4dedf0),
            original<void(THISCALL *)(ZString *)>(0x4defd0),
            original<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0),
            original<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930),
            original<void(THISCALL *)(ZString *, const char *, DWORD)>(0x4df160)};
}

inline DWORD slot(const void *obj, int i) { return (*reinterpret_cast<const DWORD *const *>(obj))[i]; }
template <typename T> inline DWORD as_dword(T v)
{
    DWORD d = 0;
    memcpy(&d, &v, sizeof v < 4 ? sizeof v : 4);
    return d;
}

// Original (trampoline) and reimplementation (the hooked exe address) with the same arguments
template <typename F, typename... A> inline void compare_call(Tally &t, DWORD address, const char *what, A... a)
{
    DWORD o = as_dword(original<F>(address)(a...)), r = as_dword(at<F>(address)(a...));
    char d[160];
    wsprintfA(d, "[0x%x %.90s: original %x, reimpl %x]", address, what, o, r);
    t.check(o == r, "", d);
}

inline void copy_upper(char *out, const char *s, int max)
{
    int m = 0;
    for (; s[m] && m < max - 1; m++)
        out[m] = s[m] >= 'a' && s[m] <= 'z' ? s[m] - 32 : s[m];
    out[m] = 0;
}

// FindOrFail with a missing name ends the game: during the test the message box (0x4d24c0) and exit
// (0x5d0e36) jump to stand-ins. The exit stand-in returns from zax_call_escapable with the stack and
// registers saved there, like longjmp.
extern "C" {
extern DWORD zax_escape_esp, zax_exit_code;
int __cdecl zax_call_escapable(void *fn, void *self, const char *name, int hint);
void __cdecl zax_escape_exit(int code);
}

inline char g_box_text[8192];
inline DWORD g_box_length;
inline const char *g_box_caption;
inline void __fastcall capture_box(const char *caption, const char *text)
{
    g_box_caption = caption;
    g_box_length = (DWORD)lstrlenA(text);
    DWORD n = g_box_length < sizeof g_box_text - 1 ? g_box_length : sizeof g_box_text - 1;
    memcpy(g_box_text, text, n);
    g_box_text[n] = 0;
}

struct CodePatch {
    BYTE *site;
    BYTE saved[5];
};
inline void patch_jump(CodePatch &p, DWORD address, void *target)
{
    p.site = at<BYTE *>(address);
    memcpy(p.saved, p.site, 5);
    BYTE jump[5] = {0xe9};
    const INT32 rel = (INT32)((BYTE *)target - (p.site + 5));
    memcpy(jump + 1, &rel, 4);
    write_code(p.site, jump);
}
inline void unpatch(CodePatch &p)
{
    write_code(p.site, p.saved);
}

// ---- Stand-ins for atexit and FreeLibrary --------------------------------------------------------

// While a test builds a function-local static of the exe, atexit (0x5d0863) jumps to a stand-in that
// records the function instead of registering it a second time
inline DWORD g_atexit_calls, g_atexit_function;
inline int __cdecl fake_atexit(DWORD f)
{
    g_atexit_calls++;
    g_atexit_function = f;
    return 0;
}

// The import FreeLibrary (0x5ea064) points to a stand-in that records the module
inline DWORD g_freed[32];
inline int g_num_freed;
inline BOOL WINAPI fake_free_library(DWORD m)
{
    if (g_num_freed < 32)
        g_freed[g_num_freed] = m;
    g_num_freed++;
    return 1;
}
struct ImportPatch {
    DWORD *slot;
    DWORD saved;
};
inline void patch_import(ImportPatch &p, DWORD address, void *target)
{
    p.slot = at<DWORD *>(address);
    DWORD old;
    VirtualProtect(p.slot, 4, PAGE_EXECUTE_READWRITE, &old);
    p.saved = *p.slot;
    *p.slot = (DWORD)(uintptr_t)target;
    VirtualProtect(p.slot, 4, old, &old);
}
inline void unpatch_import(ImportPatch &p)
{
    DWORD old;
    VirtualProtect(p.slot, 4, PAGE_EXECUTE_READWRITE, &old);
    *p.slot = p.saved;
    VirtualProtect(p.slot, 4, old, &old);
}

// Objects whose deleting destructor (slot 0) records its id and flags
struct FakeEntry {
    DWORD vftable;
    DWORD id;
};
inline DWORD g_deleted[64];
inline int g_num_deleted;
inline FakeEntry *THISCALL fake_entry_delete(FakeEntry *e, DWORD flags)
{
    if (g_num_deleted < 64)
        g_deleted[g_num_deleted] = e->id * 16 + flags;
    g_num_deleted++;
    return e;
}
inline DWORD g_fake_entry_vtable[1];
inline FakeEntry g_fake_entries[16];
inline void init_fake_entries()
{
    g_fake_entry_vtable[0] = (DWORD)(uintptr_t)&fake_entry_delete;
    for (DWORD i = 0; i < 16; i++)
        g_fake_entries[i] = {(DWORD)(uintptr_t)g_fake_entry_vtable, i + 1};
}
inline unsigned deleted_hash() { return fnv(fnv(2166136261u, &g_num_deleted, 4), g_deleted, 4 * (g_num_deleted < 64 ? g_num_deleted : 64)); }
inline unsigned freed_hash() { return fnv(fnv(2166136261u, &g_num_freed, 4), g_freed, 4 * (g_num_freed < 32 ? g_num_freed : 32)); }

// Saved bytes of a global object, restored after the test
struct SavedBytes {
    BYTE *at_;
    DWORD n;
    BYTE bytes[0x400];
};
inline void save_bytes(SavedBytes &s, DWORD address, DWORD n)
{
    s.at_ = at<BYTE *>(address);
    s.n = n;
    memcpy(s.bytes, s.at_, n);
}
inline void restore_bytes(const SavedBytes &s) { memcpy(s.at_, s.bytes, s.n); }

// The pool audit (check_pool_audit.cpp): memory of the checks registered as such, the audit after
// every module, and whether a call of operator new comes from the pool growing (return address in
// ZPoolClass::Grow, or a chunk's size) – such a chunk must stay in the C heap, never in an arena or a
// stub's block
void check_region(const void *base, DWORD size, const char *name);
void pool_audit(const char *module);
bool pool_grow_call(DWORD return_address, DWORD n);

// The live CObjects of the running game (check_real_objects.cpp, why it reads only allocated blocks): the
// objects of registered classes (cls, n) the world 0x6c2080 points to directly or through one live block,
// then up to `dialogue_lines` lines of the loaded dialogue scripts; at most `max` into `out`, with how each
// was reached in `from` (may be null). Logs one summary line.
constexpr int REAL_FROM = 40;
int find_real_objects(void **out, char (*from)[REAL_FROM], int max, CClassInfo **cls, int n, int dialogue_lines);

// The live-block knowledge behind find_real_objects for other scans of the running game (checks_inventory.cpp,
// checks_world_data.cpp): live_blocks_take takes the snapshot of the C heap's busy blocks and the pool's
// blocks (false and a log line when there is no memory for it), live_blocks_free drops it. In between,
// live_block_of gives the live block p lies in (a pool block not on the free list, or a busy heap block that
// is no pool chunk), live_object_at tells whether an object may be taken at p (the start of a live block, or
// a static object in the exe's image), live_blocks_log logs the snapshot's counts.
bool live_blocks_take();
void live_blocks_free();
bool live_block_of(DWORD p, DWORD &start, DWORD &size);
bool live_object_at(const void *p);
void live_blocks_log(const char *what);

// The level the player is in, found the way the game finds it (check_real_objects.cpp; World::CurrentLayer
// 0x4f1050): the world's layer list (data +0x2bc, count +0x2c0) at the index player 0 keeps at +0xdc (world
// slot 44, its slot 23 (0)), which World::SetPlayerLayer 0x4f4700 (world slot 78) sets when a level is
// started or switched to (App_SwitchMap 0x59b400). Null without a world, layers or player, with an index of
// -1 or outside the list, or when the entry is no CLayer (class 0x6c1d04). Reads only those fields.
BYTE *real_player_layer();

}  // namespace zax
