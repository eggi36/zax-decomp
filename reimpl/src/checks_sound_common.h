// The shared machinery of the sound checks (checks_ambient_sound.cpp, checks_music.cpp,
// checks_sound_list.cpp, checks_sound_resource.cpp, checks_sound_stream.cpp), in the game thread.
//
// Every case runs twice from the same seed: side 0 with the module's hooks taken out (the original's
// first bytes put back, so nested calls run the original too), side 1 through the reimplementation.
//  - Memory: while a side runs, operator new (0x5d033a), free (0x5d33e0), the pool's operator new
//    (0x417710, size in ecx) and its delete (0x54d130) give and take blocks of an arena of that side
//    (filled with the same pattern), so what a case makes lies at the same offsets on both sides and is
//    compared by offset. Other threads (the multimedia timer of the music stream) still get the real
//    allocators.
//  - Recorders: the exe functions a case reaches beyond the module (DirectSound, ACM, the file buffer,
//    the resource cache, the layer parts ...) jump to recorder thunks ("mov eax, id; jmp" into one
//    entry), and the fake objects (DirectSound and its buffers, readers, resources) have vtables of
//    them. A recorder logs the call (its id and the arguments as their kinds say: arena pointers by
//    offset, stack addresses by their low byte, floats by their bits, strings by their text), pops the
//    callee's arguments and answers from a sequence of values that depends only on the case and the
//    number of the call (or a custom answer that writes outputs).
//  - The fakes lie in one static block, built from a template before each side; compared afterwards.
//  - Never put out: the hook sites the multimedia timer thread runs through (the stream's timer callback
//    and what it calls); their cases call the original through its trampoline instead (nested calls
//    then run the reimplementation).
// Never plays a sound: DirectSoundCreate, the buffers and ACM are recorders in every case.
#pragma once
#include "check_util.h"

WINIMPORT DWORD WINAPI GetCurrentThreadId(void);
WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);
WINIMPORT int WINAPI lstrcmpiA(const char *a, const char *b);

namespace zax::sndc {

inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
template <typename T> inline T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
inline DWORD &dw(void *p, DWORD off) { return fl<DWORD>(p, off); }
inline float fbits(DWORD v)
{
    float f;
    memcpy(&f, &v, 4);
    return f;
}
bool readable(const void *p, DWORD n);

// ---- Arenas -----------------------------------------------------------------------------------------------

constexpr DWORD ARENA_SIZE = 0x180000;
extern BYTE *g_arena[2];
extern DWORD g_used[2];
extern int g_side;          // the side running (-1: none)
extern bool g_overflow;
inline bool in_arena(DWORD p, int side) { return g_arena[side] && p >= ptrv(g_arena[side]) && p < ptrv(g_arena[side]) + ARENA_SIZE; }
// A block of the running side's arena (for the case's own objects), cleared
BYTE *alloc(DWORD n);
// The same offset on the other side
inline BYTE *other(const void *p, int to)
{
    const int from = 1 - to;
    return in_arena(ptrv(p), from) ? g_arena[to] + (ptrv(p) - ptrv(g_arena[from])) : (BYTE *)p;
}

// ---- Recorders --------------------------------------------------------------------------------------------

// Argument kinds (one character per stack dword): 'v' value, 'p' pointer (mapped), 'f' float bits,
// 's' C string (its text), 'z' ZString* (its text), 'x' a stack address (low byte), '-' not logged,
// 'w' the WAVEFORMATEX behind a pointer (18 bytes), 'q' the three floats behind a pointer, 'd' the dword behind a pointer (mapped), 'D' the 16 bytes behind a pointer, 'b' the low
// byte.
// Conventions: 't' thiscall (ecx logged as 'p'), 's' stdcall, 'c' cdecl (the caller pops), 'f' fastcall
// (ecx and edx logged as 'v'), 'F' fastcall with ecx and edx not logged, 'C' COM (stdcall with this first).
// Answers: 'i' a small int, 'h' an HRESULT (0, sometimes a failure), 'z' 0, '1' 1, 'b' a bool with
// random upper bytes, 'f' a float in st(0), 'n' a value of the sequence, 'c' custom.
struct Call;
using Custom = DWORD (*)(Call &c);
struct Spec {
    DWORD address;     // direct stand-in: the exe function (0 for a virtual slot)
    const char *name;
    char conv;
    const char *args;
    char ret;
    Custom custom;
};
struct Call {
    const Spec *spec;
    DWORD *a;          // the stack arguments
    DWORD ecx, edx;
    long double fret;
    bool fset;
    DWORD edx_out;
};

// Fake objects: up to NFAKE in the static block, each with the vtable of its kind
constexpr int NKIND = 16, NSLOT = 48, NFAKE = 24;
constexpr DWORD FAKE_SIZE = 0x100;
struct alignas(16) FakeBlock {
    BYTE obj[NFAKE][FAKE_SIZE];
};
extern FakeBlock g_fakes, g_template, g_after[2];
inline BYTE *fake(int i) { return g_fakes.obj[i]; }
inline BYTE *tmpl(int i) { return g_template.obj[i]; }
inline bool in_fakes(DWORD p) { return p >= ptrv(&g_fakes) && p < ptrv(&g_fakes) + sizeof g_fakes; }
// The vtable of a kind (thunks); a kind's slots are described by its table
extern DWORD g_vt[NKIND][NSLOT];
struct Kind {
    const char *name;
    const Spec *slots;   // NSLOT entries may be shorter: a null name ends the table
    int nslots;
};
void set_kind(int kind, const Spec *slots, int n);
// A fake of a kind in the template (its vtable at +0)
inline BYTE *make_fake(int i, int kind)
{
    memset(tmpl(i), 0, FAKE_SIZE);
    dw(tmpl(i), 0) = ptrv(g_vt[kind]);
    return fake(i);
}

// Direct stand-ins of a module: patched in for the module's cases (world_on), out at the end
struct Standins {
    const Spec *specs;
    int n;
};
bool world_on(const Standins *lists, int nlists);
void world_off();

// ---- The case -------------------------------------------------------------------------------------------

extern unsigned g_vseed;
extern DWORD g_vindex;
DWORD nv();                       // the next value of the case's sequence
long double nfloat(float lo, float hi);   // one in 16 a special value
extern int g_fail_one_in;         // HRESULT answers: 1 in n a failure (0: never)

constexpr int LOGN = 40;
struct Log {
    unsigned hash;
    int calls;
    DWORD ev[LOGN][8];
};
extern Log g_log[2];
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0);   // an event of the case's own code
DWORD map(DWORD v);               // a value as logged: arena pointers by offset, fakes as they are

// The module's hooks out (side 0) and in; `never_out` sites stay hooked
struct Module {
    int (*table)(Hook *);
    const DWORD *never_out;
    int n_never;
};
void hooks_out(const Module &m);
void hooks_in();

// A side: the arena fresh (the pattern), the fakes from the template, the value sequence from the seed;
// side 0 with the hooks out
void side_begin(int side, unsigned seed, const Module &m, BYTE pattern);
void side_end();
// Both sides alike: eax, the logs, the fakes and the arenas (used bytes, values mapped)
struct Result {
    DWORD eax[2];
    bool float_ret;
    DWORD st0[2];
};
bool compare(Tally &t, const char *what, unsigned seed, const Result &r, DWORD eax_mask = 0xffffffff);
// Values of side 0 and 1 alike (raw, same arena offset, same string text, stack low byte)
bool same_value(DWORD a, DWORD b);

// Generator for the cases' own values
struct Gen {
    unsigned s;
    DWORD next()
    {
        s = s * 1664525u + 1013904223u;
        DWORD x = s ^ (s >> 15);
        x *= 0x2c1b3c6du;
        return x ^ (x >> 12);
    }
    DWORD fbits_(float lo, float hi)
    {
        const DWORD v = next();
        if ((v & 15) == 0) {
            const DWORD sp[] = {0, 0x80000000u, 0x7fc00000u, 0x7f800000u, 0xff800000u, 0x3f800000u, 0x501502f9u, 0x00000001u};
            return sp[(v >> 4) & 7];
        }
        const float f = lo + (hi - lo) * (float)(int)(v >> 8) * (1.0f / 16777216.0f);
        DWORD b;
        memcpy(&b, &f, 4);
        return b;
    }
    DWORD below(DWORD n) { return n ? next() % n : 0; }
};

// Call a function with ecx/edx and stack arguments; st(0) of a float function as a hash of its 80 bits
DWORD call_fn(DWORD fn, DWORD ecx, DWORD edx, int nargs, const DWORD *a, bool float_ret, DWORD *st0, bool caller_pops = false);

}  // namespace zax::sndc
