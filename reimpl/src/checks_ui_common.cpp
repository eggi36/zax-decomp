// The window toolkit's check harness (checks_ui_common.h): the block of fakes, the recording vtables and
// stand-ins, the run of a case on both sides and the comparison.
#include "checks_ui_common.h"
#include "pool.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT DWORD WINAPI GetPrivateProfileStringA(const char *section, const char *key, const char *fallback, char *out, DWORD n,
                                                const char *file);
WINIMPORT void *WINAPI AddVectoredExceptionHandler(unsigned long first, long(WINAPI *handler)(void *info));

namespace zax::uic {

// Generated from Zax.exe (vtables written by the constructors of 0x401000-0x414cf0; stack arguments per slot
// from the `ret n` of each slot's implementation; '9' where a slot has several or none). The names of the
// classes without a name string in the exe describe what the class is (docs/DECOMPILATION.md 9).
const WinClass CLASSES[NCLASS] = {
    {"CJWindow", 0x5eb404, 76, "1111100023222342110112111120000661110010100100100113313331020052200221120040"},
    {"Grid Control", 0x5ea35c, 81, "111110002322234211011211112000066111001010010010011331333102005220022112004000303"},
    {"CJHeader", 0x5ea4ac, 79, "1111100023222342110112111120000661110010100100100113313331020052200221120040101"},
    {"frame window", 0x5ea5f8, 77, "11111000232223421101121111200006611100101001001001133133310200522002211200401"},
    {"frame window base", 0x5ea72c, 76, "1111100023222342110112111120000661110010100100100113313331020052200221120040"},
    {"frame window 2", 0x5ea85c, 77, "11111000232223421101121111200006611100101001001001133133310200522002211200401"},
    {"message box", 0x5ea990, 77, "11111000232223421101121111200006611100101001001001133133310200522002211200401"},
    {"dialog", 0x5eaac4, 77, "11111000232223421101121111200006611100101001001001133133310200522002211200401"},
    {"CJListBox", 0x5eabf8, 100, "1111100023222342110112111120000661110010100100100113313331020052200221120040200926410005555233300101"},
    {"drop list", 0x5ead94, 97, "1111100023222342110112111120000661110010100100100113313331020052200221120040200926410005555233300"},
    {"combo box", 0x5eaf18, 80, "11111000232223421101121111200006611100101001901001133133310200522002211200409921"},
    {"combo box 2", 0x5eb058, 80, "11111000232223421101121111200006611100101001901001133133310200522002211200409921"},
    {"CScreenWindow", 0x5eb198, 77, "11111000232223421101121111200006611100101001001001133133310200522002211200400"},
    {"CJTabControl", 0x5eb2d4, 76, "1111100023222342110112111120000661110010100100100113313331020052200221120040"},
    {"Resize Control", 0x5eb72c, 76, "1111100023222342110112111120000661110010100100100113313331020052200221120040"},
    {"button", 0x5eb85c, 86, "11111000232223421101121111200006611100101001001001133133310200522002211200409110000100"},
    {"CJSlider", 0x5eb9b4, 78, "111110002322234211011211112000066111001010010010011331333102005220022112004020"},
    {"button 2", 0x5ebaec, 86, "11111000232223421101121111200006611100101001001001133133310200522002211200409110000100"},
    {"button 3", 0x5ebc44, 86, "11111000232223421101121111200006611100101001001001133133310200522002211200409110000100"},
    {"edit field", 0x5ebd9c, 77, "11111000232223421101121111200006611100101001001001133133310200522002211200401"},
    {"number field", 0x5ebed4, 76, "1111100023222342110112111120000661110010100190100113313331020052200221120040"},
    {"check box", 0x5ec004, 91, "1111100023222342110112111120000661110010100100100113313331020052200221120040911000010033331"},
    {"text label", 0x5ec170, 79, "1111100023222342110112111120000661110010100100100113313331020052200221120040111"},
    {"CJScrollBar", 0x5ec2ac, 82, "1111100023222342110112111120000661110010100100100113313331020052200221120040544200"},
    {"scroll bar 2", 0x5ec3fc, 82, "1111100023222342110112111120000661110010100100100113313331020052200221120040544200"},
    // (the file dialog's slot 78 is a pure "Ok" the derived dialogs implement with one argument)
    {"file dialog", 0x5ec544, 80, "11111000232223421101121111200006611100101001001001133133310200522002211200401310"},
    // fakes of other classes the toolkit calls: frames (CFrame, slot 0 CObject's message slot) and the proxy frame
    {"CFrame", 0x5f4814, 64, "2101100011331111001010141011200000001000000000000005532322001144"},
    {"COffsetClippedProxyFrame", 0x5f8ca8, 63, "210110001133111100101014191129999999199999900999999553232200111"},
    // the toolkit's callback objects (the grid's on its header; slot 1 calls the bound member function)
    {"callback", 0x5ea350, 3, "100"},
    // the button look (ButtonLook 0x4e2220: slot 0 the deleting destructor, slot 1 draws, slot 2 updates) and the
    // look factory (0x5f88c0: slot 1 makes a look for the window); one fake for both
    {"button look", 0x5f88b4, 3, "110"},
    // an object of the game outside the toolkit (the application, the world, the reporter and its screen, a list):
    // the slots the screens call, with their stack arguments (0 a deleting destructor, 2 an array's add, 23 the
    // application's window, 79 the world's game start, 81 its level end; 112 slots for the entities' 104)
    {"game object", 0, 112, "1010000000000000000000020000000000000000000000000000000000000000000000000000000401000000000000000000000000000000"},
};

Block g_block;
Case g_case;
DWORD g_effect_ecx;
Log g_log[2];
int g_side;

namespace {

DWORD g_xnext;
DWORD g_vt[NCLASS][MAXSLOT];
bool g_ready;
const Stub *g_stubs;
int g_nstubs;
CodePatch g_patches[NSTUB];
int g_npatches;
ImportPatch g_tick_patch;
DWORD g_stack_base, g_stack_limit;
DWORD g_thread;   // the checks' thread (the game thread)

// ZStrings made for the side
constexpr int MAXTEXT = 64;
BYTE *g_texts[MAXTEXT];
int g_ntexts;

void (*g_after_side)();
void (*g_before_state)();

// Side 0's block after its state was taken, for the report of a difference
Block g_block0;
DWORD g_textv[2][64][4];   // per side: the texts' hash, capacity, references and their first chars
int g_ntextv[2];

// The saved globals of the running check
Globals *g_globals;
DWORD g_saved[Globals::MAX], g_values[Globals::MAX];
bool g_value_set[Globals::MAX];

DWORD read_global(DWORD a, DWORD sz)
{
    return sz == 1 ? *at<BYTE *>(a) : sz == 2 ? *at<WORD *>(a) : *at<DWORD *>(a);
}
void write_global(DWORD a, DWORD sz, DWORD v)
{
    if (sz == 1)
        *at<BYTE *>(a) = (BYTE)v;
    else if (sz == 2)
        *at<WORD *>(a) = (WORD)v;
    else
        *at<DWORD *>(a) = v;
}

}  // namespace

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

BYTE *xalloc(DWORD n)
{
    n = (n + 15) & ~15u;
    if (g_xnext + n > EXTRA)
        return nullptr;
    BYTE *p = g_block.extra + g_xnext;
    g_xnext += n;
    memset(p, 0, n);
    return p;
}

// ---- Thunks and the handler -----------------------------------------------------------------------------------

}  // namespace zax::uic

extern "C" {
extern char uic_thunks[];
DWORD uic_pop, uic_edx;
DWORD uic_fload, uic_fvalue;   // a float answer: loaded into st0 on the way back (kind K_FLOAT)
DWORD __cdecl uic_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx);
}
__asm__(".text\n"
        ".globl _uic_thunks\n"
        "_uic_thunks:\n"
        ".set uic_i, 0\n"
        ".rept 3600\n"   // NTHUNK = NCLASS * MAXSLOT + NSTUB
        "  movl $uic_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _uic_common - . - 4\n"
        ".set uic_i, uic_i + 1\n"
        ".endr\n"
        "_uic_common:\n"
        "  pushl %edx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  leal 12(%esp), %edx\n"
        "  pushl %edx\n"
        "  call _uic_handler\n"
        "  addl $16, %esp\n"
        "  cmpl $0, _uic_fload\n"
        "  je 1f\n"
        "  flds _uic_fvalue\n"
        "  movl $0, _uic_fload\n"
        "1:\n"
        "  movl _uic_edx, %edx\n"
        "  movl (%esp), %ecx\n"
        "  addl _uic_pop, %esp\n"
        "  jmp *%ecx\n");
static_assert(zax::uic::NTHUNK == 3600);

namespace zax::uic {

namespace {

DWORD thunk(int k) { return ptrv(uic_thunks) + 10u * (DWORD)k; }

void init()
{
    if (g_ready)
        return;
    for (int c = 0; c < NCLASS; c++)
        for (int s = 0; s < MAXSLOT; s++)
            g_vt[c][s] = thunk(c * MAXSLOT + s);
    g_ready = true;
}

// An argument as both sides can compare it
DWORD canon(DWORD v, char kind)
{
    switch (kind) {
    case A_RAW: return v;
    case A_BYTE: return v & 0xff;
    case A_IGNORE: return 0;
    case A_RECT: return readable(at<void *>(v), 16) ? 0x4ec70000u ^ fnv(2166136261u, at<void *>(v), 16) : v;
    case A_POINT: return readable(at<void *>(v), 8) ? 0x90170000u ^ fnv(2166136261u, at<void *>(v), 8) : v;
    case A_TEXT: {
        if (!v || !readable(at<void *>(v), 1))
            return v;
        DWORD n = 0;
        while (n < 256 && readable(at<char *>(v) + n, 1) && at<char *>(v)[n])
            n++;
        return 0x7e870000u ^ fnv(2166136261u, at<void *>(v), n);
    }
    default: break;
    }
    if (in_block(v) || v < 0x10000 || (v >= 0x400000 && v < IMAGE_END))
        return v;
    if (v >= g_stack_limit && v < g_stack_base)
        return readable(at<void *>(v), 4) ? 0x5ac00000u ^ fnv(2166136261u, at<void *>(v), 4) : 0x5ac00000u;
    if (readable(at<void *>(v), 4))
        return 0x7ea00000u ^ fnv(2166136261u, at<void *>(v), 4);
    return v;
}

void record(DWORD id, DWORD ecx, const DWORD *args, int n, const char *spec)
{
    if (g_side != 0 && g_side != 1)
        return;
    Log &l = g_log[g_side];
    DWORD e[6] = {id, canon(ecx, A_AUTO), 0, 0, 0, 0};
    for (int i = 0; i < n && i < 4; i++)
        e[2 + i] = canon(args[i], spec && spec[i] ? spec[i] : A_AUTO);
    l.hash = fnv(l.hash, e, sizeof e);
    if (l.calls < LOGN)
        memcpy(l.ev[l.calls], e, sizeof e);
    l.calls++;
}

constexpr DWORD FLOAT_SPECIAL[4] = {0, 0x3f800000, 0xbf800000, 0x7fc00000};

DWORD answer(char kind)
{
    Gen &g = g_case.values;
    g_case.answers++;
    switch (kind) {
    case K_BOOL: return (g.next() & ~0xffu) | (g.next() & 1);
    case K_SMALL: return g.below(21);
    case K_ZERO: return 0;
    case K_ONE: return 1;
    case K_WINDOW: {
        const DWORD i = g.below((DWORD)g_case.nwin + 2);
        return i < (DWORD)g_case.nwin ? ptrv(win((int)i)) : 0;
    }
    case K_WINDOW_NN: return ptrv(win((int)g.below((DWORD)(g_case.nwin ? g_case.nwin : 1))));
    case K_RECT: {
        int *r = reinterpret_cast<int *>(xalloc(16));
        if (!r)
            return 0;
        r[0] = g.range(-40, 600), r[1] = g.range(-40, 400);
        r[2] = r[0] + g.range(-5, 300), r[3] = r[1] + g.range(-5, 200);
        return ptrv(r);
    }
    case K_TEXT: return ptrv(TEXTS[g.below(8)]);
    case K_FLOAT: {   // a float in st0 (and its bits in eax): 0, 1, -1, a small integer, a fraction, NaN
        const DWORD k = g.below(6);
        float f;
        DWORD bits;
        if (k < 4) {
            bits = FLOAT_SPECIAL[k];
        } else {
            f = k == 4 ? (float)g.range(-10, 40) : (float)g.range(-3000, 30000) / 97.0f;
            memcpy(&bits, &f, 4);
        }
        uic_fvalue = bits;
        uic_fload = 1;
        return bits;
    }
    case K_FIXED0: return g_case.fixed[0];
    case K_FIXED1: return g_case.fixed[1];
    case K_FIXED2: return g_case.fixed[2];
    case K_FIXED3: return g_case.fixed[3];
    case K_FIXED4: return g_case.fixed[4];
    case K_FIXED5: return g_case.fixed[5];
    case K_FIXED6: return g_case.fixed[6];
    case K_FIXED7: return g_case.fixed[7];
    default: {
        const DWORD r = g.next();
        const DWORD k = r % 3;
        return k == 0 ? 0 : k == 1 ? 1 : g.next();
    }
    }
}

}  // namespace

const char *const TEXTS[8] = {"", "OK", "&Cancel", "Load &Game", "A longer text with && an ampersand", "x",
                              "Zax: The Alien Hunter", "&&"};

extern "C" DWORD __cdecl uic_handler(DWORD *frame, DWORD idx, DWORD ecx, DWORD edx)
{
    const DWORD *args = frame + 1;
    uic_edx = edx;
    if (idx < (DWORD)(NCLASS * MAXSLOT)) {
        const int c = (int)(idx / MAXSLOT), s = (int)(idx % MAXSLOT);
        const char *argstr = c == C_OBJECT && g_case.object_args ? g_case.object_args : CLASSES[c].args;
        const char a = s < CLASSES[c].slots ? argstr[s] : '9';
        const int n = a >= '0' && a <= '8' ? a - '0' : 0;
        uic_pop = 4 + 4u * (DWORD)n;
        record(0x10000u | idx, ecx, args, n, g_case.argspec[s]);
        const DWORD r = answer(g_case.kinds[c][s]);
        if (g_case.effect && (g_side == 0 || g_side == 1)) {
            // the effect may call recorded functions itself (e.g. make a ZString): this call's pop and edx stay
            const DWORD pop = uic_pop;
            g_effect_ecx = ecx;
            g_case.effect(c, s, const_cast<DWORD *>(args), r);
            uic_pop = pop, uic_edx = edx;
        }
        // The deleting destructor and a successful Close leave the parent's child list
        BYTE *self = at<BYTE *>(ecx);
        if (win_index(ecx) >= 0 && (s == 0 || (s == 5 && (r & 0xff) && g_case.unlink_on_close)))
            unlink_child(self);
        return r;
    }
    const int k = (int)idx - NCLASS * MAXSLOT;
    if (k < 0 || k >= g_nstubs) {
        uic_pop = 4;
        return 0;
    }
    const Stub &st = g_stubs[k];
    uic_pop = 4 + (st.conv == CV_CDECL ? 0u : 4u * (DWORD)st.nargs);
    // A new pool chunk (the pool growing inside the case): the real C heap, never a stub's block, or the
    // pool would keep giving out the stub's memory to the game and to later checks (check_util.h)
    if (st.address == 0x5d033a && pool_grow_call(frame[0], args[0]))
        return at<DWORD(__cdecl *)(DWORD, DWORD)>(0x5d1661)(args[0], 1);
    // The allocators called from another thread: the real ones (pool, C heap)
    if (GetCurrentThreadId() != g_thread) {
        switch (st.address) {
        case 0x417710: return ptrv(Pool_New(ecx));
        case 0x54d130: return Pool_Delete(at<void *>(ecx), edx);
        case 0x5065b0: return ptrv(at<ZPool *>(ecx)->Alloc(args[0]));
        case 0x5065e0: return at<ZPool *>(ecx)->Free(at<void *>(args[0]), args[1]);
        case 0x5d033a: return at<DWORD(__cdecl *)(DWORD, DWORD)>(0x5d1661)(args[0], 1);
        case 0x5d0320: at<void(__cdecl *)(DWORD)>(0x5d33e0)(args[0]); return 0;
        default: break;
        }
    }
    DWORD a[6] = {0, 0, 0, 0, 0, 0};
    int n = 0;
    if (st.conv == CV_FAST) {
        a[n++] = ecx;
        a[n++] = edx;
        ecx = 0;
    }
    for (int i = 0; i < st.nargs && n < 6; i++)
        a[n++] = args[i];
    record(0x20000u | (DWORD)k, st.conv == CV_THIS ? ecx : 0, a, n, st.args);
    if (st.kind == 'n')
        return ptrv(xalloc(st.conv == CV_THIS ? a[0] : a[0]));
    if (st.kind == 'p') {   // gives its first stack argument (an output object the effect may fill)
        if (g_case.effect && (g_side == 0 || g_side == 1)) {
            const DWORD pop = uic_pop;
            g_effect_ecx = ecx;
            g_case.effect(-1, (int)st.address, const_cast<DWORD *>(args), args[0]);
            uic_pop = pop, uic_edx = edx;
        }
        return args[0];
    }
    if (st.kind == 'e') {   // a constructor: the effect may make the object a fake
        if (g_case.effect && (g_side == 0 || g_side == 1)) {
            const DWORD pop = uic_pop;
            g_effect_ecx = ecx;
            g_case.effect(-1, (int)st.address, const_cast<DWORD *>(args), ecx);
            uic_pop = pop, uic_edx = edx;
        }
        return ecx;
    }
    // A block of the block freed through a stand-in: its memory is wiped (what a freed block holds is not
    // compared)
    if ((st.address == 0x54d130 && in_block(a[0]) && a[1] < EXTRA) || (st.address == 0x5065e0 && in_block(a[0]) && a[1] < EXTRA))
        memset(at<void *>(a[0]), 0xdd, a[1]);
    const DWORD r = answer(st.kind);
    if (g_case.effect && (g_side == 0 || g_side == 1)) {
        const DWORD pop = uic_pop;
        g_effect_ecx = ecx;
        g_case.effect(-1, (int)st.address, const_cast<DWORD *>(args), r);
        uic_pop = pop, uic_edx = edx;
    }
    return r;
}

namespace {

DWORD WINAPI fake_tick()
{
    if (GetCurrentThreadId() != g_thread)   // the game's other threads keep the real clock
        return at<DWORD(WINAPI *)()>(g_tick_patch.saved)();
    const DWORD t = g_case.tick;
    g_case.tick += g_case.tick_step;
    log_event(0x71c, t, 0);
    return t;
}

}  // namespace

// A stand-in's site jumps to a thread guard: the game's other threads (the music stream reads files, frees
// frames, ...) go to the original code (the hook's trampoline) and never reach the recording thunk; only the
// check's thread does. Without a trampoline (a function not hooked) they still reach the thunk as before.
// (Found in band A of the last phase 4 round: FileBuf stand-ins answered the music thread on Assimilate Cave1,
// unrelated cases differed now and then.)
//   mov eax, fs:[0x24]; cmp eax, tid; jne original; jmp thunk
BYTE *g_guards;
void *thread_guard(int i, DWORD address, DWORD thunk_at)
{
    if (!g_guards)
        g_guards = static_cast<BYTE *>(VirtualAlloc(nullptr, NSTUB * 32, 0x3000, PAGE_EXECUTE_READWRITE));
    const DWORD orig = ptrv(original_address(address));
    if (!g_guards || orig == address)
        return at<void *>(thunk_at);
    BYTE *c = g_guards + 32 * i;
    const BYTE code[22] = {0x64, 0xa1, 0x24, 0, 0, 0, 0x3d, 0, 0, 0, 0, 0x0f, 0x85, 0, 0, 0, 0, 0xe9, 0, 0, 0, 0};
    memcpy(c, code, sizeof code);
    memcpy(c + 7, &g_thread, 4);
    const DWORD rel_orig = orig - (ptrv(c) + 17), rel_thunk = thunk_at - (ptrv(c) + 22);
    memcpy(c + 13, &rel_orig, 4);
    memcpy(c + 18, &rel_thunk, 4);
    FlushInstructionCache(GetCurrentProcess(), c, 22);
    return c;
}

void stubs_on(const Stub *stubs, int n)
{
    g_thread = GetCurrentThreadId();
    g_stubs = stubs;
    g_nstubs = n < NSTUB ? n : NSTUB;
    g_npatches = 0;
    for (int i = 0; i < g_nstubs; i++) {
        patch_jump(g_patches[g_npatches++], stubs[i].address, thread_guard(i, stubs[i].address, thunk(NCLASS * MAXSLOT + i)));
    }
    patch_import(g_tick_patch, 0x5ea0e8, (void *)&fake_tick);
}
void stubs_off()
{
    for (int i = g_npatches - 1; i >= 0; i--)
        unpatch(g_patches[i]);
    g_npatches = 0;
    unpatch_import(g_tick_patch);
}

void log_event(DWORD id, DWORD a, DWORD b)
{
    if (g_side != 0 && g_side != 1)
        return;
    const DWORD e[3] = {0x30000u | id, a, b};
    Log &l = g_log[g_side];
    l.hash = fnv(l.hash, e, sizeof e);
    if (l.calls < LOGN) {
        memset(l.ev[l.calls], 0, sizeof l.ev[l.calls]);
        memcpy(l.ev[l.calls], e, sizeof e);
    }
    l.calls++;
}

// ---- Building the fakes ---------------------------------------------------------------------------------------

BYTE *make_window(int i, int cls)
{
    init();
    BYTE *w = win(i);
    memset(w, 0, WIN);
    at_(w, 0) = ptrv(g_vt[cls]);
    make_text(w + 0x50, "");   // the title
    return w;
}

BYTE *make_plain_window(int i, int cls, Gen &g)
{
    BYTE *w = make_window(i, cls);
    int *r = &at_<int>(w, 4);
    r[0] = g.range(-50, 600), r[1] = g.range(-50, 450);
    r[2] = r[0] + (g.chance(9) ? g.range(-20, 0) : g.range(0, 400)), r[3] = r[1] + (g.chance(9) ? g.range(-20, 0) : g.range(0, 300));
    for (int k = 0; k < 4; k++) {
        at_<int>(w, 0x14 + 4 * k) = r[k] + g.range(-10, 10);
        at_<int>(w, 0x24 + 4 * k) = r[k] + g.range(-10, 10);
        at_<int>(w, 0x3c + 4 * k) = r[k] + g.range(-10, 10);
    }
    at_(w, 0x34) = (DWORD)g.range(0, 500);
    at_(w, 0x38) = (DWORD)g.range(0, 400);
    for (DWORD o = 0x58; o < 0x68; o++)
        w[o] = g.chance(3) ? 0 : g.chance(2) ? 1 : (BYTE)g.next();
    w[0x59] = g.chance(2) ? 0xff : (BYTE)g.below(10);
    w[0x5b] = g.chance(2) ? 0 : (BYTE)('a' + g.below(26));
    at_(w, 0x68) = (DWORD)g.range(-20, 600), at_(w, 0x6c) = (DWORD)g.range(-20, 400), at_(w, 0x70) = g.next();
    w[0x78] = (BYTE)g.below(9), w[0x79] = (BYTE)g.below(9), w[0x7a] = (BYTE)g.below(5), w[0x7b] = (BYTE)g.below(2);
    at_(w, 0x7c) = ptrv(real_font());
    return w;
}

void link_child(BYTE *parent, BYTE *child)
{
    at_(child, 0xe8) = ptrv(parent);
    at_(child, 0xf4) = 0;
    at_(child, 0xf8) = at_(parent, 0xf0);
    if (at_(parent, 0xf0))
        at_(at<BYTE *>(at_(parent, 0xf0)), 0xf4) = ptrv(child);
    else
        at_(parent, 0xec) = ptrv(child);
    at_(parent, 0xf0) = ptrv(child);
}

void unlink_child(BYTE *child)
{
    BYTE *parent = at<BYTE *>(at_(child, 0xe8));
    if (!parent || !in_block(ptrv(parent)))
        return;
    const DWORD next = at_(child, 0xf4), prev = at_(child, 0xf8);
    if (prev && in_block(prev))
        at_(at<BYTE *>(prev), 0xf4) = next;
    else if (at_(parent, 0xec) == ptrv(child))
        at_(parent, 0xec) = next;
    if (next && in_block(next))
        at_(at<BYTE *>(next), 0xf8) = prev;
    else if (at_(parent, 0xf0) == ptrv(child))
        at_(parent, 0xf0) = prev;
    at_(child, 0xe8) = at_(child, 0xf4) = at_(child, 0xf8) = 0;
}

BYTE *make_scroll(BYTE *w, int x, int y, int cw, int ch, bool show, BYTE *vbar, BYTE *hbar)
{
    BYTE *s = static_cast<BYTE *>(at<void *(__cdecl *)(DWORD)>(0x5d033a)(0x1c));
    at_(s, 0) = (DWORD)x, at_(s, 4) = (DWORD)y, at_(s, 8) = (DWORD)cw, at_(s, 0xc) = (DWORD)ch;
    at_(s, 0x10) = show ? 1 : 0;
    at_(s, 0x14) = ptrv(vbar), at_(s, 0x18) = ptrv(hbar);
    at_(w, 0xfc) = ptrv(s);
    return s;
}

void make_tree(Gen &g, int n, int cls_root, int cls_child)
{
    make_plain_window(0, cls_root, g);
    for (int i = 1; i < n; i++) {
        make_plain_window(i, cls_child, g);
        link_child(win((int)g.below((DWORD)i)), win(i));
    }
    g_case.nwin = n;
}

void make_text(BYTE *at_zstring, const char *text)
{
    ZString *z = reinterpret_cast<ZString *>(at_zstring);
    for (int i = 0; i < g_ntexts; i++)
        if (g_texts[i] == at_zstring) {   // made before in this build: the old one goes first
            if (!in_block(ptrv(z->buffer)))
                original<void(THISCALL *)(ZString *)>(0x4defd0)(z);
            original<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, text);
            return;
        }
    if (g_ntexts >= MAXTEXT)
        return;
    original<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, text);
    g_texts[g_ntexts++] = at_zstring;
}

BYTE *real_font() { return *at<BYTE **>(0x62e5d0); }

BYTE *fake_font(DWORD height)
{
    BYTE *f = xalloc(0x100);
    if (f)
        at_(f, 0x80) = height;
    return f;
}

// ---- Running a case -------------------------------------------------------------------------------------------

void ModuleHooks::out()
{
    n = 0;
    const int count = table(nullptr);
    fits = hook_sites_fit(name, count, MAX);
    if (!fits)
        return;
    table(hooks);
    for (int i = 0; i < count; i++) {
        BYTE *site = at<BYTE *>(hooks[i].address);
        // installed: the jump to the replacement (also without a trampoline); a stand-in of the case (both sides
        // call it) or a site not hooked (Disable=) is left as it is
        if (site[0] != 0xe9 || ptrv(site) + 5 + *reinterpret_cast<const DWORD *>(site + 1) != ptrv(hooks[i].replacement))
            continue;
        sites[n] = site;
        memcpy(saved[n], site, 5);
        write_code(site, hooks[i].expected);
        n++;
    }
}
void ModuleHooks::in()
{
    for (int i = 0; i < n; i++)
        write_code(sites[i], saved[i]);
    n = 0;
}

void case_global(DWORD address, DWORD value)
{
    if (!g_globals)
        return;
    for (int i = 0; i < g_globals->n; i++)
        if (g_globals->address[i] == address) {
            g_values[i] = value;
            g_value_set[i] = true;
        }
}

namespace {

// The ZStrings of the block canonicalised: the text's hash and capacity in place of the pointer, the buffer
// released (side-made texts too: every dword of the block that points to a valid ZString buffer made for the
// side is one of g_texts)
int g_side_state_side = -1;
unsigned side_state()
{
    unsigned h = 2166136261u;
    for (int i = 0; i < g_ntexts; i++) {
        ZString *z = reinterpret_cast<ZString *>(g_texts[i]);
        const ZStringBuffer *b = z->buffer;
        DWORD v[3] = {0xdead, 0, 0};
        if (b && readable(b, 12)) {
            const bool shared = b == at<ZStringBuffer *>(ZSTRING_EMPTY);
            v[0] = shared ? 1u : fnv(2166136261u, b->text, (unsigned)lstrlenA(b->text));
            v[1] = b->capacity, v[2] = shared ? 0 : b->refs;   // the shared buffer's count is the game's
        }
        h = fnv(h, v, sizeof v);
        if (g_side_state_side >= 0 && i < 64) {
            g_textv[g_side_state_side][i][0] = v[0], g_textv[g_side_state_side][i][1] = v[1], g_textv[g_side_state_side][i][2] = v[2];
            g_textv[g_side_state_side][i][3] = b && readable(b, 12) ? *reinterpret_cast<const DWORD *>(b->text) : 0;
            g_ntextv[g_side_state_side] = i + 1;
        }
        if (b && !in_block(ptrv(b)) && readable(b, 12))
            original<void(THISCALL *)(ZString *)>(0x4defd0)(z);
        at_(g_texts[i], 0) = 0x2ec7a000u | (DWORD)i;
    }
    // Scroll infos on the heap (made by the build or by the call): their content in place of the pointer
    for (int i = 0; i < NWIN; i++) {
        BYTE *w = win(i);
        const DWORD sp = at_(w, 0xfc);
        if (!sp || in_block(sp))
            continue;
        DWORD v[8];
        if (readable(at<void *>(sp), 0x1c))
            memcpy(v, at<void *>(sp), 0x1c);
        else
            memset(v, 0xee, 0x1c);
        v[7] = 0x5c011;
        h = fnv(h, v, sizeof v);
        at<void(__cdecl *)(void *)>(0x5d0320)(at<void *>(sp));
        at_(w, 0xfc) = 0x5c011000u | (DWORD)i;
    }
    h = fnv(h, &g_block, sizeof g_block);
    if (g_globals)
        for (int i = 0; i < g_globals->n; i++) {
            const DWORD v = read_global(g_globals->address[i], g_globals->size[i]);
            h = fnv(h, &v, 4);
        }
    return h;
}

}  // namespace

bool g_trace_cases;   // [Reimpl] UiTrace=1: every case and side in the log (to find a crash)

// While a case runs, an exception is logged with its address and the registers (the game ends anyway)
bool g_in_case;
long WINAPI on_exception(void *info)
{
    if (!g_in_case)
        return 0;
    const DWORD *const *p = static_cast<const DWORD *const *>(info);
    const DWORD *rec = p[0], *ctx = p[1];
    log("UI CHECK EXCEPTION %x at %x (access %x) eax %x ecx %x edx %x ebx %x esi %x edi %x esp %x ebp %x side %d", rec[0],
        rec[3], rec[4] >= 2 ? rec[6] : 0, ctx[0xb0 / 4], ctx[0xac / 4], ctx[0xa8 / 4], ctx[0xa4 / 4], ctx[0xa0 / 4],
        ctx[0x9c / 4], ctx[0xc4 / 4], ctx[0xb4 / 4], g_side);
    const DWORD *sp = reinterpret_cast<const DWORD *>(static_cast<uintptr_t>(ctx[0xc4 / 4]));
    if (readable(sp, 32))
        log("  stack %x %x %x %x %x %x %x %x", sp[0], sp[1], sp[2], sp[3], sp[4], sp[5], sp[6], sp[7]);
    log("  uic_handler at %x (the DLL's addresses relative to it)", ptrv((const void *)&uic_handler));
    g_in_case = false;
    return 0;
}
bool g_handler;

bool run_case(Tally &t, const Check &c, const char *what, unsigned seed, Build build, CallFn call, DWORD eax_mask)
{
    __asm__("movl %%fs:4, %0\n\tmovl %%fs:8, %1" : "=r"(g_stack_base), "=r"(g_stack_limit));
    if (!g_handler) {
        AddVectoredExceptionHandler(1, &on_exception);
        g_handler = true;
    }
    g_in_case = true;
    g_globals = c.globals;
    if (g_globals)
        for (int i = 0; i < g_globals->n; i++) {
            g_saved[i] = read_global(g_globals->address[i], g_globals->size[i]);
            g_value_set[i] = false;
        }
    DWORD eax[2], state[2];
    for (int side = 0; side < 2; side++) {
        g_side = -1;
        g_xnext = 0;
        g_ntexts = 0;
        memset(&g_block, 0, sizeof g_block);
        init();
        g_case.seed = seed;
        g_case.values.s = seed * 2654435761u + 77;
        g_case.answers = 0;
        g_case.nwin = 0;
        g_case.tick = 1000000 + seed % 5000;
        g_case.tick_step = 7;
        g_case.unlink_on_close = true;
        g_case.effect = nullptr;
        g_case.object_args = nullptr;
        memset(g_case.fixed, 0, sizeof g_case.fixed);
        memset(g_case.argspec, 0, sizeof g_case.argspec);
        g_case.argspec[43][0] = A_TEXT;
        g_case.argspec[57][0] = A_RECT;
        g_case.argspec[14][0] = A_POINT, g_case.argspec[14][1] = A_POINT;
        for (int k = 0; k < NCLASS; k++)
            for (int s = 0; s < MAXSLOT; s++)
                g_case.kinds[k][s] = s == 36 || s == 37 ? K_WINDOW : s == 58 ? K_RECT : s == 60 || s == 61 ? K_SMALL : K_VALUE;
        Gen g{seed};
        if (g_globals)   // the build sees the game's values on both sides
            for (int i = 0; i < g_globals->n; i++)
                write_global(g_globals->address[i], g_globals->size[i], g_saved[i]);
        if (g_trace_cases)
            log("    %s case %u side %d build", what, seed, side);
        build(g);
        if (g_globals)
            for (int i = 0; i < g_globals->n; i++)
                write_global(g_globals->address[i], g_globals->size[i], g_value_set[i] ? g_values[i] : g_saved[i]);
        if (g_trace_cases)
            log("    globals in");
        g_log[side] = {2166136261u, 0, {}};
        if (g_trace_cases)
            log("    hooks");
        if (side == 0)
            c.hooks->out();
        g_side = side;
        if (g_trace_cases)
            log("    call");
        eax[side] = call() & eax_mask;
        g_side = -1;
        if (side == 0)
            c.hooks->in();
        if (g_before_state)
            g_before_state();
        g_side_state_side = side;
        g_ntextv[side] = 0;
        state[side] = side_state();
        g_side_state_side = -1;
        if (g_after_side)
            g_after_side();
        // The pool's free lists must never hold memory of the block (a block freed through the real pool)
        if (const ZPool *pool = *at<ZPool **>(POOL_GLOBAL))
            for (DWORD k = 0; k < pool->num_classes && k < 200; k++)
                if (in_block(ptrv(pool->classes[k].free_list))) {
                    log("POOL HOLDS BLOCK MEMORY after %s seed %u side %d (class %u, %x)", what, seed, side, k,
                        ptrv(pool->classes[k].free_list));
                    pool->classes[k].free_list = nullptr;   // dropped (a leak) rather than handed out again
                }
        if (side == 0)
            memcpy(&g_block0, &g_block, sizeof g_block);
    }
    if (g_globals)
        for (int i = 0; i < g_globals->n; i++)
            write_global(g_globals->address[i], g_globals->size[i], g_saved[i]);
    g_in_case = false;
    const bool same = c.hooks->fits && eax[0] == eax[1] && state[0] == state[1] && g_log[0].hash == g_log[1].hash &&
                      g_log[0].calls == g_log[1].calls;
    char d[200];
    wsprintfA(d, "[%.80s seed %u: eax %x/%x, state %x/%x, calls %d/%d]", what, seed, eax[0], eax[1], state[0], state[1],
              g_log[0].calls, g_log[1].calls);
    const int before = t.mismatches;
    t.check(same, "", d);
    if (!same && before < 6) {
        const DWORD *a = reinterpret_cast<const DWORD *>(&g_block0), *b = reinterpret_cast<const DWORD *>(&g_block);
        int shown = 0;
        for (DWORD i = 0; i < sizeof(Block) / 4 && shown < 4; i++)
            if (a[i] != b[i]) {
                log("    block +%x: %x / %x (self +%x; next %x %x / %x %x)", i * 4, a[i], b[i],
                    g_self ? i * 4 - (ptrv(g_self) - ptrv(&g_block)) : 0, a[i + 1], a[i + 2], b[i + 1], b[i + 2]);
                shown++;
            }
        for (int i = 0; i < g_ntextv[0] && i < g_ntextv[1]; i++)
            if (memcmp(g_textv[0][i], g_textv[1][i], 12))
                log("    text %d: %x %u %u '%.4s' / %x %u %u '%.4s'", i, g_textv[0][i][0], g_textv[0][i][1], g_textv[0][i][2],
                    (const char *)&g_textv[0][i][3], g_textv[1][i][0], g_textv[1][i][1], g_textv[1][i][2],
                    (const char *)&g_textv[1][i][3]);
        for (int i = 0; i < LOGN && i < g_log[0].calls && i < g_log[1].calls; i++)
            if (memcmp(g_log[0].ev[i], g_log[1].ev[i], sizeof g_log[0].ev[i])) {
                const DWORD *a = g_log[0].ev[i], *b = g_log[1].ev[i];
                log("    call %d: %x %x %x %x / %x %x %x %x", i, a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
                break;
            }
    }
    return same;
}

namespace {
char g_only[64];
bool g_only_read;
}  // namespace

bool ui_selected(const char *test_name)
{
    if (!g_only_read) {
        char ini[MAX_PATH + 32];
        GetModuleFileNameA(NULL, ini, MAX_PATH);
        for (int n = lstrlenA(ini); n > 0 && ini[n - 1] != '\\'; n--)
            ini[n - 1] = 0;
        lstrcatA(ini, "Zax-Reimpl.ini");
        GetPrivateProfileStringA("Reimpl", "UiOnly", "", g_only, sizeof g_only, ini);
        char trace[8];
        GetPrivateProfileStringA("Reimpl", "UiTrace", "", trace, sizeof trace, ini);
        g_trace_cases = trace[0] == '1';
        g_only_read = true;
        if (g_only[0])
            log("  UiOnly=%s: only these toolkit tests", g_only);
    }
    if (!g_only[0])
        return true;
    // a comma-separated list of texts
    for (const char *o = g_only; *o;) {
        int n = 0;
        while (o[n] && o[n] != ',')
            n++;
        for (const char *p = test_name; *p && n; p++) {
            int i = 0;
            while (i < n && p[i] == o[i])
                i++;
            if (i == n)
                return true;
        }
        o += n;
        if (*o == ',')
            o++;
    }
    return false;
}

// ---- A table of tests ----------------------------------------------------------------------------------------

BYTE *g_self;
DWORD g_args[9];

namespace {

const Test *g_test;
void (*g_base)(Gen &g);

void test_build(Gen &g)
{
    g_self = nullptr;
    memset(g_args, 0, sizeof g_args);
    g_base(g);
    g_test->setup(g);
}

DWORD test_call()
{
    switch (g_test->call) {
    case CALL_FAST: return call_fast(g_test->address, g_args[0], g_args[1], g_test->nargs - 2, g_args + 2);
    case CALL_CDECL: {
        using F = DWORD(__cdecl *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
        return at<F>(g_test->address)(g_args[0], g_args[1], g_args[2], g_args[3], g_args[4], g_args[5]);
    }
    default: return call_this(g_test->address, g_self, g_test->nargs, g_args);
    }
}

}  // namespace

void run_tests(Tally &t, ModuleHooks &hooks, Globals *globals, const Test *tests, int n, void (*base)(Gen &g), unsigned seed)
{
    g_base = base;
    const Check c{&hooks, globals};
    for (int i = 0; i < n; i++) {
        if (!ui_selected(tests[i].name))
            continue;
        g_test = &tests[i];
        g_after_side = tests[i].after;
        g_before_state = tests[i].before_state;
        stubs_on(tests[i].stubs, tests[i].nstubs);
        const int count = scaled(tests[i].count);
        for (int k = 0; k < count; k++)
            run_case(t, c, tests[i].name, seed + (unsigned)(i * 10007 + k), &test_build, &test_call, tests[i].mask);
        stubs_off();
        g_after_side = nullptr;
        g_before_state = nullptr;
    }
}

// ---- Calling exe functions ------------------------------------------------------------------------------------

DWORD call_this(DWORD fn, void *self, int n, const DWORD *a)
{
    using F0 = DWORD(THISCALL *)(void *);
    using F1 = DWORD(THISCALL *)(void *, DWORD);
    using F2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
    using F3 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD);
    using F4 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD);
    using F5 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD);
    using F6 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    using F7 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    using F8 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    using F9 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    switch (n) {
    case 0: return at<F0>(fn)(self);
    case 1: return at<F1>(fn)(self, a[0]);
    case 2: return at<F2>(fn)(self, a[0], a[1]);
    case 3: return at<F3>(fn)(self, a[0], a[1], a[2]);
    case 4: return at<F4>(fn)(self, a[0], a[1], a[2], a[3]);
    case 5: return at<F5>(fn)(self, a[0], a[1], a[2], a[3], a[4]);
    case 6: return at<F6>(fn)(self, a[0], a[1], a[2], a[3], a[4], a[5]);
    case 7: return at<F7>(fn)(self, a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
    case 8: return at<F8>(fn)(self, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
    default: return at<F9>(fn)(self, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8]);
    }
}

DWORD call_fast(DWORD fn, DWORD ecx, DWORD edx, int n, const DWORD *a)
{
    using F0 = DWORD(__fastcall *)(DWORD, DWORD);
    using F1 = DWORD(__fastcall *)(DWORD, DWORD, DWORD);
    using F2 = DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD);
    using F3 = DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD);
    using F4 = DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    using F5 = DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    using F6 = DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    switch (n) {
    case 0: return at<F0>(fn)(ecx, edx);
    case 1: return at<F1>(fn)(ecx, edx, a[0]);
    case 2: return at<F2>(fn)(ecx, edx, a[0], a[1]);
    case 3: return at<F3>(fn)(ecx, edx, a[0], a[1], a[2]);
    case 4: return at<F4>(fn)(ecx, edx, a[0], a[1], a[2], a[3]);
    case 5: return at<F5>(fn)(ecx, edx, a[0], a[1], a[2], a[3], a[4]);
    default: return at<F6>(fn)(ecx, edx, a[0], a[1], a[2], a[3], a[4], a[5]);
    }
}

}  // namespace zax::uic
