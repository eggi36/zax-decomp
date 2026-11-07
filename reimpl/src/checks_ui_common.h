// The check harness of the window toolkit (phase 4, docs/DECOMPILATION.md 9, "The window toolkit"): fake
// windows whose virtual calls are recorded, the exe functions the toolkit reaches beyond its own code as
// recording stand-ins, a run of a function on side 0 (the original, the module's hooks out) and side 1 (the
// reimplementation) from the same seed, and the comparison of everything it can change.
//
// - The fakes lie in one static block (g_block): NWIN window objects of WIN bytes and an extra area for the
//   case's scroll infos, rectangles, texts, list rows and other objects, at the same addresses on both
//   sides, so pointers compare raw. Every side builds the block from the case seed again (the build
//   function of the check), runs the call and hashes the block.
// - Recording vtables: one per window class of the toolkit (CLASSES, generated from the exe: the vtable and
//   the stack arguments every slot pops, from the `ret n` of its implementation). Every slot is a thunk
//   (mov eax, k; jmp common) into uic_handler, which logs the call (class and slot, ecx, the first stack
//   arguments as values), pops the arguments and answers from a sequence of values that depends only on the
//   case seed and the number of the call. The answer's kind per slot (a value, a window of the block or
//   null, a rectangle) comes from the class's kinds string or the check's override. A deleting destructor
//   (slot 0) and a successful Close (slot 5) take the fake out of its parent's child list as the real ones
//   do, so loops that close or delete children end.
// - Stand-ins: exe functions outside the module (sounds, the cursor, the keyboard, the INI file, the
//   frames' helpers) jump to recording thunks of the same kind for the duration of the case (patch_jump;
//   their convention and arguments in a Stub entry); GetTickCount's import answers a case value.
// - Compared: eax (masked), the call log (hash and count; the first calls kept for the report), the block
//   (ZString members canonicalised to their texts first) and the saved globals of the check.
// Arguments in the log: a pointer into the block raw, a pointer to the stack or the heap by the hash of the
// 16 bytes it points to (the two sides' frames and heap blocks differ), anything else raw.
#pragma once
#include "check_util.h"

namespace zax::uic {

inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
template <typename T = DWORD> inline T &at_(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
bool readable(const void *p, DWORD n);

// ---- The window classes of the toolkit ------------------------------------------------------------------------

struct WinClass {
    const char *name;
    DWORD vtable;
    int slots;
    const char *args;   // stack arguments per slot ('0'-'8', '9' unknown)
};
enum ClassId : int {
    C_WINDOW, C_GRID, C_HEADER, C_FRAME, C_FRAME_BASE, C_FRAME2, C_MESSAGE_BOX, C_DIALOG, C_LISTBOX, C_DROPLIST,
    C_COMBO, C_COMBO2, C_SCREEN, C_TABS, C_RESIZE, C_BUTTON, C_SLIDER, C_BUTTON2, C_BUTTON3, C_EDIT, C_NUMBER,
    C_CHECKBOX, C_LABEL, C_SCROLLBAR, C_SCROLLBAR2, C_FILE_DIALOG, C_CFRAME, C_PROXY_FRAME, C_CALLBACK, C_LOOK, C_OBJECT, NCLASS
};
extern const WinClass CLASSES[NCLASS];
constexpr int MAXSLOT = 112;
constexpr int NSTUB = 128;
constexpr int NTHUNK = NCLASS * MAXSLOT + NSTUB;

// ---- The block of fakes ---------------------------------------------------------------------------------------

constexpr int NWIN = 16;
constexpr DWORD WIN = 0x400, EXTRA = 0x6000;
struct alignas(16) Block {
    BYTE win[NWIN][WIN];
    BYTE extra[EXTRA];
};
extern Block g_block;
inline BYTE *win(int i) { return g_block.win[i]; }
inline bool in_block(DWORD v) { return v >= ptrv(&g_block) && v < ptrv(&g_block) + sizeof(Block); }
inline int win_index(DWORD v)
{
    return v >= ptrv(g_block.win) && v < ptrv(g_block.win) + sizeof g_block.win ? (int)((v - ptrv(g_block.win)) / WIN) : -1;
}

// Room in the extra area (reset per side by begin_side; the same offsets on both sides)
BYTE *xalloc(DWORD n);

// ---- The case values ------------------------------------------------------------------------------------------

struct Gen {
    unsigned s;
    DWORD next()
    {
        s = s * 1664525u + 1013904223u;
        DWORD x = s ^ (s >> 15);
        x *= 0x2c1b3c6du;
        return x ^ (x >> 12);
    }
    DWORD below(DWORD n) { return n ? next() % n : 0; }
    int range(int lo, int hi) { return lo + (int)below((DWORD)(hi - lo + 1)); }
    bool chance(DWORD one_in) { return below(one_in) == 0; }
};

// Answer kinds of recorded calls
enum Kind : char {
    K_VALUE = 'v',   // 0, 1 or a random dword (a third each)
    K_BOOL = 'b',    // 0 or 1 in al, random upper bytes
    K_SMALL = 'i',   // 0..20
    K_WINDOW = 'w',  // one of the case's windows or null
    K_WINDOW_NN = 'W', // one of the case's windows
    K_RECT = 'r',    // a fresh rectangle in the extra area
    K_TEXT = 't',    // one of a few texts
    K_ZERO = '0',    // 0
    K_ONE = '1',     // 1
    K_FIXED0 = 'A',  // the case's fixed answers (g_case.fixed, set by the build)
    K_FIXED1 = 'B',
    K_FIXED2 = 'C',
    K_FIXED3 = 'D',
    K_FIXED4 = 'E',
    K_FIXED5 = 'F',
    K_FIXED6 = 'G',
    K_FIXED7 = 'H',
    K_FLOAT = 'f',   // a float, in st0 as well (for functions that return a float on the FPU stack)
};

// An exe function replaced by a recording stand-in for the case
enum Conv : char { CV_THIS = 't', CV_STD = 's', CV_CDECL = 'c', CV_FAST = 'f' };
struct Stub {
    DWORD address;
    Conv conv;
    int nargs;      // stack arguments
    char kind;      // the answer (Kind; 'n': a new block of the first argument's size in the extra area, 'e': ecx,
                    // 'p': the first stack argument, after the effect)
    const char *name;
    const char *args = "";   // how the arguments are logged (ArgKind per argument, ecx and edx first for fastcall)
};
// How an argument goes into the log
enum ArgKind : char {
    A_AUTO = 'a',   // raw, except a pointer to the stack or the heap: the 4 bytes it points to
    A_RAW = 'v',    // raw
    A_RECT = 'r',   // the 16 bytes it points to (wherever)
    A_POINT = 'q',  // the 8 bytes it points to
    A_TEXT = 's',   // the C string it points to
    A_BYTE = 'b',   // the low byte (the rest of the register is left over)
    A_IGNORE = 'x', // not compared (e.g. the address of a local of the caller)
};

// The log of a side
constexpr int LOGN = 40;
struct Log {
    unsigned hash;
    int calls;
    DWORD ev[LOGN][6];
};

struct Case {
    unsigned seed;
    Gen values;                 // the answers (reset per side)
    DWORD answers;              // calls answered
    char kinds[NCLASS][MAXSLOT];
    int nwin;                   // windows in play (K_WINDOW answers)
    DWORD fixed[8];             // the answers of the kinds K_FIXED0-7
    DWORD tick;                 // GetTickCount's answer (grows by tick_step per call)
    DWORD tick_step;
    bool unlink_on_close;
    char argspec[MAXSLOT][8];   // per slot: ArgKind of the stack arguments (default A_AUTO)
    // Called after a recorded slot call has its answer (e.g. to write through an output argument); for a
    // stand-in cls is -1 and slot the stand-in's address, args its stack arguments
    void (*effect)(int cls, int slot, DWORD *args, DWORD answer);
    // The stack arguments of the "game object" class's 112 slots for this case (default: the class table's),
    // for checks whose game objects take other arguments
    const char *object_args;
};
extern Case g_case;
extern DWORD g_effect_ecx;   // the object (ecx) of the recorded call whose effect runs
extern Log g_log[2];
extern int g_side;

// Kind of the answers of a class's slot for this case (the default comes from the class table)
inline void set_kind(int cls, int slot, char k) { g_case.kinds[cls][slot] = k; }
inline void set_kind_all(int slot, char k)
{
    for (int c = 0; c < NCLASS; c++)
        g_case.kinds[c][slot] = k;
}

// A fake window of class `cls` at block index i (the recording vtable, the rest zero)
BYTE *make_window(int i, int cls);
// A plain window as the base constructor leaves it, with a random rectangle and flags from the generator
BYTE *make_plain_window(int i, int cls, Gen &g);
// Child list: append `child` to `parent` (first child at +0xec, last +0xf0, next +0xf4, previous +0xf8, parent +0xe8)
void link_child(BYTE *parent, BYTE *child);
void unlink_child(BYTE *child);
// A scroll info (0x1c bytes from the exe's operator new, as the windows make it) for window w; side_state
// compares it by content and frees it again
BYTE *make_scroll(BYTE *w, int x, int y, int cw, int ch, bool show, BYTE *vbar, BYTE *hbar);
// A random tree of windows 0..n-1 (window 0 the root; each other window a child of an earlier one)
void make_tree(Gen &g, int n, int cls_root, int cls_child);

// ZString members of the fakes (made per side from the exe's ZString functions, compared by text and freed
// after the side): the address of the ZString in the block and its text
void make_text(BYTE *at_zstring, const char *text);
extern const char *const TEXTS[8];

// The real default font of the game (0x62e5d0) or a fake one of the given height in the extra area
BYTE *real_font();
BYTE *fake_font(DWORD height);

// ---- Running a case -------------------------------------------------------------------------------------------

// The hooks of a module out (side 0) and in again. Declared constinit (the DLL has no C runtime to run
// dynamic initialisers).
struct ModuleHooks {
    static constexpr int MAX = 160;
    const char *name;
    int (*table)(Hook *);
    Hook hooks[MAX] = {};
    BYTE *sites[MAX] = {};
    BYTE saved[MAX][5] = {};
    int n = 0;
    bool fits = true;
    void out();
    void in();
};

// The globals a module's functions read or write: saved before a case, put to the case values for every
// side, hashed after a side, restored after the case
struct Globals {
    static constexpr int MAX = 32;
    DWORD address[MAX];
    DWORD size[MAX];   // 1, 2 or 4
    int n = 0;
    void add(DWORD a, DWORD sz) { address[n] = a, size[n] = sz, n++; }
};

struct Check {
    ModuleHooks *hooks;
    Globals *globals;
};

// The allocators as stand-ins (operator_new_ecx 0x417710 and operator new 0x5d033a give blocks of the extra
// area, pool_delete 0x54d130 and operator delete 0x5d0320 only log): objects a call makes lie at the same
// addresses on both sides. Calls from another thread go to the real allocators.
#define UIC_ALLOC_STUBS \
    {0x417710, CV_FAST, 0, 'n', "operator_new_ecx", "vx"}, {0x54d130, CV_FAST, 0, K_VALUE, "pool_delete", "vv"}, \
        {0x5065b0, CV_THIS, 1, 'n', "ZPool::Alloc", "v"}, {0x5065e0, CV_THIS, 2, K_VALUE, "ZPool::Free", "vv"}, \
        {0x5d033a, CV_CDECL, 1, 'n', "operator new"}, {0x5d0320, CV_CDECL, 1, K_VALUE, "operator delete"}
// (the original ZString code allocates and frees through the pool's methods 0x5065b0/0x5065e0 directly, the
// reimplementation through 0x417710/0x54d130: both pairs are stand-ins, or a buffer made by one would be freed
// into the real pool by the other)

// The stand-ins in for a group of cases (the exe's code is writable while the checks run; nothing else runs
// in between) and out again
void stubs_on(const Stub *stubs, int n);
void stubs_off();

// Runs build(g) and call() on both sides; `what` names the case in the log. build gets a generator from the
// case seed (the same on both sides) and may set g_case's kinds and the globals' values (case_global).
// Returns whether both sides were the same.
using Build = void (*)(Gen &g);
using CallFn = DWORD (*)();
bool run_case(Tally &t, const Check &c, const char *what, unsigned seed, Build build, CallFn call, DWORD eax_mask = 0xffffffff);
// The value a saved global starts with on both sides (set by build; default: the game's value)
void case_global(DWORD address, DWORD value);

// [Reimpl] UiOnly= of Zax-Reimpl.ini (checks only, for work on one function): only the toolkit's tests whose
// name contains the text run; empty: all
bool ui_selected(const char *test_name);

// An extra entry of the side's log (e.g. a value the call wrote outside the block)
void log_event(DWORD id, DWORD a, DWORD b);

// Calling a function of the exe by address with up to 8 stack arguments (thiscall on self; fastcall with
// ecx/edx; cdecl/stdcall without self)
DWORD call_this(DWORD fn, void *self, int n, const DWORD *a);
DWORD call_fast(DWORD fn, DWORD ecx, DWORD edx, int n, const DWORD *a);

// ---- A table of tests ----------------------------------------------------------------------------------------

enum CallKind { CALL_THIS, CALL_FAST, CALL_CDECL };
struct Test {
    const char *name;
    DWORD address;
    CallKind call;
    int nargs;               // stack arguments (fastcall: ecx and edx count as the first two)
    DWORD mask;              // eax bits compared
    int count;               // random cases (scaled)
    void (*setup)(Gen &g);   // after the base build: g_self, g_args, kinds, globals
    const Stub *stubs = nullptr;
    int nstubs = 0;
    void (*after)() = nullptr;          // after each side's state was taken (e.g. to destroy what the call built)
    void (*before_state)() = nullptr;   // after the call, before the state is taken (e.g. to clear stack garbage)
};
extern BYTE *g_self;
extern DWORD g_args[9];
// Runs every selected test of the table: per test the stand-ins on, `count` cases (base(g) then the
// test's setup, the call on both sides), the stand-ins off. Seeds count up from `seed`.
void run_tests(Tally &t, ModuleHooks &hooks, Globals *globals, const Test *tests, int n, void (*base)(Gen &g), unsigned seed);

}  // namespace zax::uic
