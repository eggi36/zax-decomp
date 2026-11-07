// The fake world of the checks of the sequences batch (checks_sequences.cpp, checks_sequence_player.cpp,
// checks_normal_attack_ai.cpp, checks_entity_om.cpp, checks_say_quip.cpp), in the game thread: a copy of
// checks_ai_world.h (the patrol AIs' fake world) with its own stand-ins, fake object roles and saved
// globals, so the copies can grow apart without touching each other.
//
// Every case runs twice from the same seed: side 0 with the hooks of the batch's modules taken out (the
// original's first bytes put back, so nested calls run the original too; sites that are stand-ins here,
// like Random_Below, keep their stand-in), side 1 through the reimplementation. The exe functions the
// slots reach beyond the modules jump to recorder thunks ("mov eax, id; jmp" into one entry): the recorder
// logs the call (its id, ecx/edx and the stack arguments as their kinds say: pointers made comparable,
// floats by their bits, strings and ZStrings by their text, the points behind pointers by value), pops the
// callee's arguments and answers from a sequence of values that depends only on the case and the number
// of the call. The fake objects lie in one static block (the same addresses on both sides); each kind
// has its own vtable of recorder thunks. While a side runs, the globals the slots read (the world, the
// application, the reporter, the point buffer, the quip list, ...) hold the case's values.
// Compared per case: eax (masked), the call log, the whole fake block and the globals afterwards.
#pragma once
#include "check_util.h"

WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);
WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);

namespace zax::sqw {

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
    T_ENTITY, T_BEHAVIOR, T_INVENTORY, T_ITEM, T_WORLD, T_APP, T_REPORTER, T_VIEW, T_AI, T_OTHER, T_LAYER, T_ACTION, T_SEQUENCE,
    T_LIST, T_DIRLIST, T_FRAME, NTYPES, T_NONE = -1
};
constexpr int NOBJ = 80;
constexpr DWORD OBJ = 0x200;
// Roles (object numbers); the world takes two objects (its +0x334 is the music mixer)
constexpr int O_SELF = 0, O_SELF2 = 1, O_ENTITY = 2, N_ENTITY = 6, O_BEHAVIOR = 8, N_BEHAVIOR = 4, O_INVENTORY = 12, N_INVENTORY = 2,
              O_ITEM = 14, N_ITEM = 4, O_WORLD = 18, O_APP = 20, O_REPORTER = 21, O_VIEW = 22, O_LAYER = 23, O_AI = 24, N_AI = 3,
              O_OTHER = 27, N_OTHER = 4, O_ACTION = 31, N_ACTION = 3, O_SEQUENCE = 34, O_FRAME = 35, O_LIST = 36, O_DIRLIST = 37,
              N_DIRLIST = 2, O_RECORDS = 39, N_RECORDS = 2, O_POLY = 41, N_POLY = 2, O_POINTS = 43, N_POINTS = 2, O_PARTS = 45,
              O_MIXER = 46, O_WRITER = 47, O_VOICES = 48, O_EXTRA = 49, N_EXTRA = 31;
struct alignas(16) Block {
    BYTE obj[NOBJ][OBJ];
};
extern Block g_block;
inline BYTE *obj(int i) { return g_block.obj[i]; }
extern DWORD g_fvt[NTYPES][256];

// Valid ZStrings with texts (made once, never freed): 0 "", 1 "Target", 2 "Player", 3 "Om", 4 "Quips"
constexpr int NTEXTS = 5;
extern ZString g_texts[NTEXTS];
void make_texts();

// ---- The case ---------------------------------------------------------------------------------------------

extern int g_side;
extern unsigned g_vseed;
extern DWORD g_vindex;
DWORD nv();
float frange(float lo, float hi);   // one in 16 a special value (0, -0, NaN, inf, the bounds, huge)
extern bool g_trapped;
void log_value(DWORD v);            // an extra value into the side's log (for the stand-ins' own checks)

// Per case: which kinds of answers may be null, which classes IsKindOf always affirms, other settings
struct Policy {
    DWORD nonnull;          // bit per Type: 'n' answers of that type never 0
    DWORD always_null;      // bit per Type: 'n' answers of that type always 0
    CClassInfo *always[4];  // IsKindOf with one of these classes: true
    CClassInfo *never[2];   // IsKindOf with one of these classes: false
    int find_minus_one;     // 1 in n answers of a Find is -1 (0: never)
    DWORD extra[4];         // optional stand-ins installed for the case only (their addresses)
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

// Build the fake world from a seed (both sides alike); then the case may set up its own values
void build_world(unsigned seed);
// The world's globals in (holding the case's values) and out again
void globals_on();
void globals_off();
// The case's value of a global (its address; the case may change it before globals_on)
BYTE *case_global(DWORD address);
// The stand-ins in for the module's checks (the exe's code writable meanwhile) and out
bool world_on();
void world_off();
// The hooks of the batch's modules out (side 0) and in; g_modules chooses the modules (bit 0 sequences,
// 1 sequence_player, 2 normal_attack_ai, 3 entity_om, 4 say_quip, 5 random_misc), so a case puts out only
// the sites of the module under test and the ones it calls
constexpr DWORD M_SEQUENCES = 1, M_PLAYER = 2, M_ATTACK = 4, M_OM = 8, M_QUIP = 0x10, M_RANDOM = 0x20;
extern DWORD g_modules;
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

// A slot test: the function, its number of stack arguments (thiscall on the object 0; with SLOT_FLOAT the
// result is st(0); with SLOT_FASTCALL a[0] and a[1] go into ecx and edx and the number counts the stack
// arguments after them; SLOT_CDECL: no object, the arguments on the stack), the bits of eax compared, the
// number of random cases and the arguments (the setup may change the fakes too)
constexpr int SLOT_FLOAT = 0x100, SLOT_FASTCALL = 0x200, SLOT_CDECL = 0x400;
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
extern void (*g_before_state)();      // run after the call, before the state is taken (to log and free side-made texts)
extern DWORD g_made[8];               // objects a side made through the pool (CMultipleActionsAction::Add's)
extern int g_nmade;
extern bool g_trace;                  // log every case and side (to find a crash)
DWORD call_slot(DWORD fn, void *self, int n, const DWORD *a);
bool run_slot(Tally &t, const SlotTest &st, unsigned seed, SelfMaker make_self, const Policy &policy);
// A fake of a type, chosen by the generator (or 0 one time in `null_one_in`)
inline BYTE *any_entity(Gen &g, int null_one_in = 0) { return null_one_in && g.next() % (DWORD)null_one_in == 0 ? nullptr : obj(O_ENTITY + (int)(g.next() % N_ENTITY)); }
inline BYTE *extra(int i) { return obj(O_EXTRA + i); }

// The level's parts (the layer grid of the running level), each once
int level_parts(BYTE **out, int max);
// A part's AIs (its "Activity" array) and the objects their fields point to, three levels deep, without
// the level's parts; each once
int part_ais(const BYTE *part, BYTE *const *parts, int nparts, BYTE **out, int max);

}  // namespace zax::sqw
