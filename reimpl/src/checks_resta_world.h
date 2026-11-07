// The fake world of the band A checks of ai_rest_a.cpp, input_rest_a.cpp and dialog_rest_a.cpp
// (checks_ai_rest_a.cpp, checks_input_rest_a.cpp, checks_dialog_rest_a.cpp), in the game thread: a copy of
// checks_rest_world.h (itself a copy of the task AIs' world) with its own stand-ins (path finding, the
// entity's sequence, frames and motion, the key table, mouse, joystick, timeGetTime, the INI and the network
// block, the dialogue scripts and the sound manager), case values for the move behaviour's static checker,
// the control configuration's statics and the script list, so the worlds can grow apart without touching
// each other. The ZString functions and the allocators are the exe's (a stand-in of operator_new_ecx would
// also get the strings' buffers).
//
// Every case runs twice from the same seed: side 0 with the hooks of the three modules taken out (the
// original's first bytes put back, so nested calls run the original too), side 1 through the
// reimplementation. The exe functions the slots reach beyond the modules jump to recorder thunks: the
// recorder logs the call (its id, ecx/edx and the stack arguments as their kinds say), pops the callee's
// arguments and answers from a sequence of values that depends only on the case and the number of the
// call. The fake objects lie in one static block (the same addresses on both sides); each kind has its
// own vtable of recorder thunks. While a side runs, the globals the slots read point into the fakes.
// Compared per case: eax (masked), the call log, the whole fake block and the saved globals afterwards.
#pragma once
#include "check_util.h"

WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);
WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);

namespace zax::ra {

inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
template <typename T> inline T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
inline float fbits(DWORD v)
{
    float f;
    memcpy(&f, &v, 4);
    return f;
}

bool readable(const void *p, DWORD n);
CClassInfo *class_of(const void *p);

// ---- Fake objects ---------------------------------------------------------------------------------------

enum Type : int {
    T_ENTITY, T_BEHAVIOR, T_INVENTORY, T_ITEM, T_DEF, T_WORLD, T_APP, T_REPORTER, T_GAMETYPE, T_VIEW, T_AI, T_OTHER, T_LAYER,
    T_ACTION, T_NET, T_NETLAYER, T_PACKET, T_CATALOGUE, NTYPES, T_NONE = -1
};
constexpr int NOBJ = 64;
constexpr DWORD OBJ = 0x200;
// Roles (object numbers)
constexpr int O_SELF = 0, O_ENTITY = 1, N_ENTITY = 6, O_BEHAVIOR = 7, N_BEHAVIOR = 4, O_INVENTORY = 11, N_INVENTORY = 2,
              O_ITEM = 13, N_ITEM = 4, O_DEF = 17, N_DEF = 3, O_WORLD = 20, O_APP = 21, O_REPORTER = 22, O_GAMETYPE = 23,
              O_VIEW = 24, O_LAYER = 25, O_AI = 26, N_AI = 3, O_OTHER = 29, N_OTHER = 4, O_ACTION = 33, N_ACTION = 3,
              O_NET = 36, O_NETLAYER = 37, O_PACKET = 38, O_PLAYER = 39, O_OPTIONS = 40, O_RULES_HOLDER = 41, O_RULES = 42,
              O_MODEL = 43, O_DYN = 44, O_POINT = 45, O_MSG = 46, O_SLOT = 47, O_SPARE = 48, N_SPARE = 16, O_SEQ = 49,
              O_SCRIPT = 60, O_LINES = 62, O_RECORDS = 63;   // the dialogue scripts (in the spare range, after the
                                                             // input checks' areas 50-59)
struct alignas(16) Block {
    BYTE obj[NOBJ][OBJ];
};
extern Block g_block;
inline BYTE *obj(int i) { return g_block.obj[i]; }
extern DWORD g_fvt[NTYPES][256];

// ---- The case ---------------------------------------------------------------------------------------------

extern int g_side;
extern unsigned g_vseed;
extern DWORD g_vindex;
DWORD nv();
float frange(float lo, float hi);   // one in 16 a special value (0, -0, NaN, inf, the bounds, huge)
extern bool g_trapped;

// Per case: which kinds of answers may be null, which classes IsKindOf always affirms, other settings
struct Policy {
    DWORD nonnull;          // bit per Type: 'n' answers of that type never 0
    DWORD always_null;      // bit per Type: 'n' answers of that type always 0
    CClassInfo *always[4];  // IsKindOf with one of these classes: true
    bool loading_never;     // the fake records are there and never "loading"
    int find_minus_one;     // 1 in n answers of a Find is -1 (0: never)
    bool real_list_vtables; // the item catalogue and the model list keep their vtables (field defaults read them)
    DWORD real[2];          // stand-ins taken out for the case (the exe's own function runs)
};
extern Policy g_policy;

// The log of a side: the hash of every call and the first calls in full
constexpr int LOGN = 48;
struct Log {
    unsigned hash;
    int calls;
    DWORD ev[LOGN][9];
};
extern Log g_log[2];
// An extra entry of the side's log (e.g. the hash of a block a check made comparable)
void log_event(DWORD id, DWORD a, DWORD b);

// Build the fake world from a seed (both sides alike); then the case may set up its own values
void build_world(unsigned seed);
// The valid ZStrings of the function-local statics (made once, before the first case)
void make_static_texts();
extern DWORD g_texts[3];   // ZStrings "", "Walk", "No Such Sequence"
// The value a saved global gets for the case (set by build_world; a check may change it before the call)
DWORD *case_global(DWORD address);
// The world's globals in (pointing into the fakes) and out again
void globals_on();
void globals_off();
// The stand-ins in for the module's checks (the exe's code writable meanwhile) and out
bool world_on();
void world_off();
// The hooks of ai_rest_a, input_rest_a and dialog_rest_a out (side 0) and in
void hooks_out();
void hooks_in();
void side_begin(int side, unsigned seed);   // log, values, world (build_world must follow for the fakes)
DWORD side_end();                           // the state hash (fake block and globals)

// Both sides' state for the comparison
struct Side {
    DWORD eax, state, logh, calls;
    bool trapped;
};
bool compare(Tally &t, const char *what, unsigned seed, const Side *s, DWORD mask);

// Values for the cases (a separate generator, seeded per case)
struct Gen {
    unsigned s;
    DWORD next()
    {
        s = s * 1664525u + 1013904223u;
        DWORD x = s ^ (s >> 15);
        x *= 0x2c1b3c6du;
        return x ^ (x >> 12);
    }
    float f(float lo, float hi)
    {
        const DWORD v = next();
        if ((v & 15) == 0) {
            const DWORD sp[] = {0, 0x80000000u, 0x7fc00000u, 0x7f800000u, 0xff800000u, 0x3f800000u, 0x501502f9u, 0x00000001u};
            return fbits(sp[(v >> 4) & 7]);
        }
        return lo + (hi - lo) * (float)(int)(v >> 8) * (1.0f / 16777216.0f);
    }
    DWORD fbits_(float lo, float hi)
    {
        const float x = f(lo, hi);
        DWORD b;
        memcpy(&b, &x, 4);
        return b;
    }
};

// A slot test: the function, its number of stack arguments (thiscall on the object 0; -1: fastcall with
// the object in ecx and the first argument in edx), the bits of eax
// compared, the number of random cases and the arguments (the setup may change the fakes too)
// More conventions (nargs): fastcall with five arguments (ecx, edx, three on the stack), cdecl without and
// with four arguments; the object under test is not passed
constexpr int CALL_FASTCALL5 = -4, CALL_CDECL0 = -5, CALL_CDECL4 = -6, CALL_FASTCALL7 = -7;
struct SlotTest {
    const char *name;
    DWORD address;
    int nargs;
    DWORD mask;
    int count;
    void (*setup)(Gen &g, DWORD *args);
};
using SelfMaker = void (*)(Gen &g);   // fills the object under test (object 0)
extern void (*g_after_side)();        // run after each side (e.g. to free what the side made)
extern void (*g_before_state)();      // run after the call, before the state is taken (to tag side-made pointers)
extern bool g_trace;                  // log every case and side (to find a crash)
extern DWORD g_last_eax;              // the slot's eax (for g_before_state and g_after_side)
DWORD call_slot(DWORD fn, void *self, int n, const DWORD *a);
bool run_slot(Tally &t, const SlotTest &st, unsigned seed, SelfMaker make_self, const Policy &policy);
// A fake of a type, chosen by the generator (or 0 one time in `null_one_in`)
inline BYTE *any_entity(Gen &g, int null_one_in = 0) { return null_one_in && g.next() % (DWORD)null_one_in == 0 ? nullptr : obj(O_ENTITY + (int)(g.next() % N_ENTITY)); }

// The level's parts (the layer grid of the running level), each once
int level_parts(BYTE **out, int max);
// A part's AIs (its "Activity" array) and the objects their fields point to (a CSkeletonAI's "Patrol AI",
// a CForceUpdatingAI's "AI To Force" ...), three levels deep, without the level's parts; each once
int part_ais(const BYTE *part, BYTE *const *parts, int nparts, BYTE **out, int max);
bool derived_from(const CClassInfo *c, const CClassInfo *base);

}  // namespace zax::ra
