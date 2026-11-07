// Checks of the C runtime's floating point library (phase 5b, docs/DECOMPILATION.md 11: crt_x87.cpp,
// crt_fp.cpp). The time and environment functions have checks_crt_time.cpp, the start-up checks_crt_startup.cpp.
//
// The floating point library's original side is a copy of the exe's code (checks_crt_copy.inc, generated
// by tools/crt_asm_gen.py): the ranges of the library read from Zax.exe on disk (so the hooked entry
// points have their original bytes), the rel32 branches out of a range relocated, to the copy where they
// stay in the library. The original then runs as the original down to the runtime's other functions
// (_errno, RaiseException), not through the hooks into the reimplementation: _CIfmod alone is two
// instructions before __cintrindisp2, and through the trampoline it would compare the reimplementation
// with itself. Every case: the result as all 80 bits of st(0) (or eax), the status word, the control word
// after the call, the tag word (the x87 stack left empty) and errno, under six control words (the game's
// 0x27f, 64 and 24 bits, the three other rounding modes; all exceptions masked as in the game). The
// unmasked paths (_raise_exc and RaiseException) are checked by calling the error functions directly with
// unmasked control word values, with a stand-in for RaiseException in the exe's import slot that records the
// IEEE record, changes it the same way on both sides and returns as after a handler that continues.
#include "check_util.h"
#include "crt_common.h"

WINIMPORT BOOL WINAPI ReadFile(HANDLE f, void *buf, DWORD n, DWORD *read, void *overlapped);
WINIMPORT DWORD WINAPI SetFilePointer(HANDLE f, INT32 distance, INT32 *high, DWORD method);
WINIMPORT BOOL WINAPI VirtualFree(void *addr, size_t size, DWORD type);
WINIMPORT void *WINAPI AddVectoredExceptionHandler(unsigned long first, long(WINAPI *handler)(void *info));
WINIMPORT unsigned long WINAPI RemoveVectoredExceptionHandler(void *handle);

namespace zax {
namespace {

#include "checks_crt_copy.inc"

using D = unsigned long long;
unsigned long long rnd64() { return (unsigned long long)random() << 32 | random(); }

// ---- the copy of the exe's floating point library -------------------------------------------------------

BYTE *g_copy;
DWORD g_copy_at[sizeof FP_COPY_RANGES / sizeof FP_COPY_RANGES[0]];

// the copy's address of an exe address in a copied range, 0 outside
DWORD copy_of(DWORD a)
{
    for (unsigned k = 0; k < sizeof FP_COPY_RANGES / sizeof FP_COPY_RANGES[0]; k++)
        if (a >= FP_COPY_RANGES[k][0] && a < FP_COPY_RANGES[k][1])
            return g_copy_at[k] + (a - FP_COPY_RANGES[k][0]);
    return 0;
}

// Zax.exe's bytes at the addresses [lo, hi) from the file (the .text section's file offset from the headers)
bool read_exe(DWORD lo, DWORD hi, BYTE *out)
{
    const BYTE *base = at<const BYTE *>(0x400000);
    const BYTE *nt = base + *(const DWORD *)(base + 0x3c);
    const int nsec = *(const WORD *)(nt + 6);
    const BYTE *sec = nt + 0x18 + *(const WORD *)(nt + 0x14);
    DWORD fileoff = 0;
    for (int k = 0; k < nsec; k++, sec += 40) {
        const DWORD va = 0x400000 + *(const DWORD *)(sec + 12), size = *(const DWORD *)(sec + 8);
        if (lo >= va && hi <= va + size)
            fileoff = *(const DWORD *)(sec + 20) + (lo - va);
    }
    if (!fileoff)
        return false;
    char path[300];
    GetModuleFileNameA(nullptr, path, 260);
    HANDLE f = CreateFileA(path, 0x80000000, 1, nullptr, 3, 0x80, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    DWORD n = 0;
    const bool ok = SetFilePointer(f, (INT32)fileoff, nullptr, 0) == fileoff && ReadFile(f, out, hi - lo, &n, nullptr) && n == hi - lo;
    CloseHandle(f);
    return ok;
}

bool build_copy()
{
    if (g_copy)
        return true;
    const unsigned nr = sizeof FP_COPY_RANGES / sizeof FP_COPY_RANGES[0];
    DWORD total = 0;
    for (unsigned k = 0; k < nr; k++)
        total += (FP_COPY_RANGES[k][1] - FP_COPY_RANGES[k][0] + 15) & ~15u;
    BYTE *mem = static_cast<BYTE *>(VirtualAlloc(nullptr, total, 0x3000, 0x40));
    if (!mem)
        return false;
    DWORD off = 0;
    for (unsigned k = 0; k < nr; k++) {
        g_copy_at[k] = (DWORD)(uintptr_t)(mem + off);
        if (!read_exe(FP_COPY_RANGES[k][0], FP_COPY_RANGES[k][1], mem + off)) {
            VirtualFree(mem, 0, 0x8000);
            return false;
        }
        off += (FP_COPY_RANGES[k][1] - FP_COPY_RANGES[k][0] + 15) & ~15u;
    }
    for (DWORD field : FP_COPY_REL32) {   // the target from the file's bytes, then relative to the copy
        const DWORD copy_field = copy_of(field);
        const DWORD t = field + 4 + (DWORD) * (INT32 *)(uintptr_t)copy_field;
        const DWORD dest = copy_of(t) ? copy_of(t) : t;
        *(INT32 *)(uintptr_t)copy_field = (INT32)(dest - (copy_field + 4));
    }
    FlushInstructionCache(GetCurrentProcess(), mem, total);
    g_copy = mem;
    return true;
}

// ---- one call with the FPU state around it ---------------------------------------------------------------

// kind: 0 st(0) argument, 1 st(1) and st(0), 2 cdecl (double), 3 cdecl (double, double), 4 cdecl (double,
// int); +8: the result in eax instead of st(0)
struct FpCall {
    DWORD fn;        // +0
    int kind;        // +4
    D a, b;          // +8, +0x10
    int n;           // +0x18
    WORD cw;         // +0x1c
    WORD sw;         // +0x1e status word after the call
    BYTE r[12];      // +0x20 st(0) as 80 bits
    WORD cwout;      // +0x2c
    WORD tag;        // +0x2e tag word after the result is taken off
    DWORD eax;       // +0x30
};
static_assert(offsetof(FpCall, eax) == 0x30, "FpCall");

extern "C" void __cdecl zax_fpcall(FpCall *c);
__asm__(R"(
    .text
    .globl _zax_fpcall
_zax_fpcall:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl 8(%ebp), %ebx
    fldcw 0x1c(%ebx)
    fnclex
    movl 4(%ebx), %eax
    andl $7, %eax
    cmpl $0, %eax
    je 10f
    cmpl $1, %eax
    je 11f
    cmpl $2, %eax
    je 12f
    cmpl $3, %eax
    je 13f
    pushl 0x18(%ebx)
    pushl 0xc(%ebx)
    pushl 8(%ebx)
    call *(%ebx)
    addl $12, %esp
    jmp 20f
10: fldl 8(%ebx)
    call *(%ebx)
    jmp 20f
11: fldl 8(%ebx)
    fldl 0x10(%ebx)
    call *(%ebx)
    jmp 20f
12: pushl 0xc(%ebx)
    pushl 8(%ebx)
    call *(%ebx)
    addl $8, %esp
    jmp 20f
13: pushl 0x14(%ebx)
    pushl 0x10(%ebx)
    pushl 0xc(%ebx)
    pushl 8(%ebx)
    call *(%ebx)
    addl $16, %esp
20: movl 8(%ebp), %ebx
    movl %eax, 0x30(%ebx)
    fnstsw 0x1e(%ebx)
    testl $8, 4(%ebx)
    jnz 21f
    fstpt 0x20(%ebx)
21: fnstcw 0x2c(%ebx)
    subl $28, %esp
    fnstenv (%esp)
    movw 8(%esp), %ax
    movw %ax, 0x2e(%ebx)
    addl $28, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
)");

struct Out {
    BYTE r[10];
    WORD sw, cw, tag;
    DWORD eax;
    int err;
};

Out run(DWORD fn, int kind, D a, D b, int n, WORD cw)
{
    FpCall c{};
    c.fn = fn;
    c.kind = kind;
    c.a = a;
    c.b = b;
    c.n = n;
    c.cw = cw;
    *crt::errno_ptr() = 0;
    zax_fpcall(&c);
    Out o{};
    if (!(kind & 8))
        memcpy(o.r, c.r, 10);
    // the exception flags and the stack fault bit; not TOP and the condition codes, which the last instructions
    // (fld, fchs, fldpi, fstp) leave undefined: the VMs' x86 emulation sets C0 there depending on what ran before
    o.sw = c.sw & 0x7f;
    o.cw = c.cwout;
    o.tag = c.tag;
    o.eax = (kind & 8) ? c.eax : 0;
    o.err = *crt::errno_ptr();
    return o;
}

bool same_out(const Out &x, const Out &y)
{
    return !memcmp(x.r, y.r, 10) && x.sw == y.sw && x.cw == y.cw && x.tag == y.tag && x.eax == y.eax && x.err == y.err;
}

void describe(char *d, const char *name, D a, D b, int n, WORD cw, const Out &x, const Out &y)
{
    wsprintfA(d, "[%s %08x%08x %08x%08x %d cw %x: r %04x%08x%08x sw %x cw %x tag %x eax %x errno %d / r %04x%08x%08x sw %x cw %x tag %x eax %x errno %d]",
              name, (DWORD)(a >> 32), (DWORD)a, (DWORD)(b >> 32), (DWORD)b, n, cw,
              *(const WORD *)(x.r + 8), *(const DWORD *)(x.r + 4), *(const DWORD *)x.r, x.sw, x.cw, x.tag, x.eax, x.err,
              *(const WORD *)(y.r + 8), *(const DWORD *)(y.r + 4), *(const DWORD *)y.r, y.sw, y.cw, y.tag, y.eax, y.err);
}

// Doubles of every kind for the math functions
D some_double(int k)
{
    static const D specials[] = {
        0, 0x8000000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff8000000000000ull,
        0xfff8000000000000ull, 0x7ff0000000000001ull, 0x7ff4000000000000ull, 0xfff7ffffffffffffull, 1,
        0x800fffffffffffffull, 0x0010000000000000ull, 0x8010000000000000ull, 0x7fefffffffffffffull,
        0xffefffffffffffffull, 0x3ff0000000000000ull, 0xbff0000000000000ull, 0x3fe0000000000000ull,
        0xbfe0000000000000ull, 0x4000000000000000ull, 0xc000000000000000ull, 0x4008000000000000ull,
        0xc008000000000000ull, 0x3ff0000000000001ull, 0x3fefffffffffffffull, 0xbff0000000000001ull,
        0xbfefffffffffffffull, 0x400921fb54442d18ull, 0x4340000000000000ull, 0x4330000000000001ull,
        0xc330000000000001ull, 0x7fe0000000000000ull, 0x0000000000000003ull, 0x4024000000000000ull,
        0x408f400000000000ull, 0xc08f400000000000ull, 0x3f50624dd2f1a9fcull, 0x4090000000000000ull,
        0x40c3880000000000ull, 0x4062c00000000000ull};
    const int ns = sizeof specials / sizeof specials[0];
    if (k < ns)
        return specials[k];
    switch (random() % 6) {
    case 0: return rnd64();
    case 1: {   // short decimals and integers
        double d = (double)((int)(random() % 20001) - 10000) / (double)(1 << (random() % 6));
        D u;
        memcpy(&u, &d, 8);
        return u;
    }
    case 2: {   // in [-1.25, 1.25] (asin/acos), some exactly +-1 and near it
        double d = ((double)(int)(random() % 2000001) - 1000000) * 0.00000125;
        D u;
        memcpy(&u, &d, 8);
        return u;
    }
    case 3: return (D)(random() & 0x7ff) << 52 | (rnd64() & 0xfffffffffffffull) | (D)(random() & 1) << 63;
    case 4: return specials[random() % ns] + (D)((int)(random() % 5) - 2);
    default: {   // exponents near the ends
        const DWORD e = random() % 2 ? random() % 60 : 0x7ff - 60 + random() % 60;
        return (D)e << 52 | (rnd64() & 0xfffffffffffffull) | (D)(random() & 1) << 63;
    }
    }
}

const WORD CWS[] = {0x27f, 0x37f, 0x07f, 0x67f, 0xa7f, 0xe7f};

// ---- the math functions -----------------------------------------------------------------------------------

struct MathFn {
    DWORD address;
    const char *name;
    int kind;
    int arg;   // n: 1 ldexp's exponents, 2 _set_exp's
};

void check_math(Tally &t)
{
    static const MathFn fns[] = {
        {0x5d23e0, "_CIpow", 1, 0}, {0x5d23f9, "pow", 3, 0}, {0x5d12d0, "_CIasin", 0, 0}, {0x5d12e4, "asin", 2, 0},
        {0x5e97e0, "_CIacos", 0, 0}, {0x5e97f4, "acos", 2, 0}, {0x5d260a, "_CIfmod", 1, 0}, {0x5d2600, "fmod", 3, 0},
        {0x5d139b, "floor", 2, 0}, {0x5d146a, "ceil", 2, 0}, {0x5ddb6e, "ldexp", 4, 1}, {0x5d5b09, "_frnd", 2, 0},
        {0x5dca99, "_copysign", 3, 0}, {0x5d5b1b, "_set_exp", 4, 2}, {0x5d5b44, "_sptype", 2 | 8, 0},
        {0x5dcaba, "_fpclass", 2 | 8, 0}, {0x5d9987, "_d_inttype", 2 | 8, 0},
    };
    static const int ldexp_ns[] = {0, 1, -1, 2, 10, -10, 52, 53, 1022, 1023, 1024, -1021, -1022, -1023, -1074, -1075,
                                   2000, -2000, 2047, 2560, 2561, -2557, -2558, 0x7fffffff, (int)0x80000000, -0x7fffffff};
    char d[400];
    g_seed = 0x5d23e0;
    for (const MathFn &f : fns) {
        const DWORD orig = copy_of(f.address);
        const int cases = scaled(f.kind == 1 || f.kind == 3 ? 2400 : 1200);
        for (int k = 0; k < cases; k++) {
            const D a = some_double(k < 40 ? k : 40 + (int)(random() % 1000));
            D b = some_double(k / 40 < 40 ? k / 40 : 40 + (int)(random() % 1000));
            int n = 0;
            if (f.arg == 1)
                n = k < 26 ? ldexp_ns[k] : random() % 3 ? ldexp_ns[random() % 26] : (int)random();
            else if (f.arg == 2)
                n = (int)(random() % 2100) - 1050;
            const WORD cw = CWS[random() % 6];
            const Out x = run(orig, f.kind, a, b, n, cw);
            const Out y = run(f.address, f.kind, a, b, n, cw);
            describe(d, f.name, a, b, n, cw, x, y);
            t.check(same_out(x, y), "", d);
        }
    }
}

// _powhlp and _decomp write through a pointer; _control87/_controlfp, _abstract_cw, _hw_cw, _errcode,
// _getfname, _set_errno take integers
void check_fp_helpers(Tally &t)
{
    char d[300];
    g_seed = 0x5d9857;
    using Powhlp = int(__cdecl *)(D, D, D *);
    using Decomp = double(__cdecl *)(D, int *);
    for (int k = 0; k < scaled(2000); k++) {
        const D x = some_double(k < 40 ? k : 40 + (int)(random() % 1000));
        const D y = k % 3 ? some_double(k / 40 < 40 ? k / 40 : 40 + (int)(random() % 1000)) : (random() & 1 ? 0x7ff0000000000000ull : 0xfff0000000000000ull);
        D ra = 0x1234, rb = 0x1234;
        __asm__ volatile("fnclex");
        const int ia = reinterpret_cast<Powhlp>(copy_of(0x5d9857))(x, y, &ra);
        WORD swa, swb;
        __asm__ volatile("fnstsw %0\n\tfnclex" : "=m"(swa));
        const int ib = at<Powhlp>(0x5d9857)(x, y, &rb);
        __asm__ volatile("fnstsw %0\n\tfnclex" : "=m"(swb));
        wsprintfA(d, "[_powhlp %08x%08x %08x%08x: %d %08x%08x %x / %d %08x%08x %x]", (DWORD)(x >> 32), (DWORD)x, (DWORD)(y >> 32), (DWORD)y,
                  ia, (DWORD)(ra >> 32), (DWORD)ra, swa, ib, (DWORD)(rb >> 32), (DWORD)rb, swb);
        t.check(ia == ib && ra == rb && (swa & 0x3f) == (swb & 0x3f), "", d);
        int ea = 77, eb = 77;
        const long double ma = (long double)reinterpret_cast<Decomp>(copy_of(0x5d5b9e))(x, &ea);
        __asm__ volatile("fnstsw %0\n\tfnclex" : "=m"(swa));
        const long double mb = (long double)at<Decomp>(0x5d5b9e)(x, &eb);
        __asm__ volatile("fnstsw %0\n\tfnclex" : "=m"(swb));
        wsprintfA(d, "[_decomp %08x%08x: %d %x / %d %x]", (DWORD)(x >> 32), (DWORD)x, ea, swa, eb, swb);
        t.check(!memcmp(&ma, &mb, 10) && ea == eb && (swa & 0x3f) == (swb & 0x3f), "", d);
    }
    using U2 = unsigned(__cdecl *)(unsigned, unsigned);
    using U1 = unsigned(__cdecl *)(unsigned);
    for (int k = 0; k < scaled(3000); k++) {
        const unsigned v = k < 0x1000 ? (unsigned)k * 0x10 + (k & 0xf) : random(), m = random() % 3 ? random() : 0xffffffff;
        unsigned ra = reinterpret_cast<U1>(copy_of(0x5dbfef))(v & 0xffff), rb = at<U1>(0x5dbfef)(v & 0xffff);
        wsprintfA(d, "[_abstract_cw %x: %x / %x]", v & 0xffff, ra, rb);
        t.check(ra == rb, "", d);
        ra = reinterpret_cast<U1>(copy_of(0x5dc081))(v);
        rb = at<U1>(0x5dc081)(v);
        wsprintfA(d, "[_hw_cw %x: %x / %x]", v, ra, rb);
        t.check(ra == rb, "", d);
        // _control87/_controlfp load the result: the game's control word around them
        for (int which = 0; which < 2; which++) {
            const DWORD fn = which ? 0x5dbfd9 : 0x5dbfa4;
            WORD cwa, cwb, start = CWS[random() % 6];
            __asm__ volatile("fldcw %0" : : "m"(start));
            ra = reinterpret_cast<U2>(copy_of(fn))(v, m);
            __asm__ volatile("fnstcw %0\n\tfldcw %1" : "=m"(cwa) : "m"(start));
            rb = at<U2>(fn)(v, m);
            const WORD game = 0x27f;
            __asm__ volatile("fnstcw %0\n\tfldcw %1" : "=m"(cwb) : "m"(game));
            wsprintfA(d, "[%s %x %x from %x: %x cw %x / %x cw %x]", which ? "_controlfp" : "_control87", v, m, start, ra, cwa, rb, cwb);
            t.check(ra == rb && cwa == cwb, "", d);
        }
        const unsigned f = random() & 0x3f;
        ra = reinterpret_cast<U1>(copy_of(0x5d5adc))(f);
        rb = at<U1>(0x5d5adc)(f);
        t.check(ra == rb, "", "[_errcode]");
        const int op = (int)(random() % 40);
        ra = reinterpret_cast<U1>(copy_of(0x5d5ab7))((unsigned)op);
        rb = at<U1>(0x5d5ab7)((unsigned)op);
        t.check(ra == rb, "", "[_getfname]");
        const int type = (int)(random() % 10) - 1;
        *crt::errno_ptr() = 5;
        reinterpret_cast<U1>(copy_of(0x5d5a8f))((unsigned)type);
        const int ea = *crt::errno_ptr();
        *crt::errno_ptr() = 5;
        at<U1>(0x5d5a8f)((unsigned)type);
        t.check(ea == *crt::errno_ptr(), "", "[_set_errno]");
    }
    __asm__ volatile("fnclex");
}

// ---- the error functions with unmasked exceptions ---------------------------------------------------------

bool g_fp_trace = false;   // log every error case before it runs (to find one that crashes)
DWORD g_veh_thread;
int g_veh_raised;
BYTE g_veh_record[0x58];
DWORD g_veh_code;
unsigned g_veh_change;   // what the handler writes back (the same on both sides)

DWORD g_real_raise;

// RaiseException as the error functions call it through the exe's import slot: in the check thread the
// IEEE record is recorded and changed the way a handler would, then the call returns as after a handler
// that continues; other threads get the real one
void WINAPI fake_raise(DWORD code, DWORD flags, DWORD nargs, const DWORD *args)
{
    if (GetCurrentThreadId() != g_veh_thread || nargs != 1 || !args) {
        reinterpret_cast<void(WINAPI *)(DWORD, DWORD, DWORD, const DWORD *)>(g_real_raise)(code, flags, nargs, args);
        return;
    }
    BYTE *ieee = *reinterpret_cast<BYTE *const *>(args);
    g_veh_raised++;
    g_veh_code = code;
    memcpy(g_veh_record, ieee, 0x58);
    DWORD *w = reinterpret_cast<DWORD *>(ieee);
    w[2] = g_veh_change & 0x1f;
    w[0] = (w[0] & ~0x1fu) | ((g_veh_change >> 5) & 0x1f);
    if (g_veh_change & 0x400) {
        const D v = 0x400921fb54442d18ull ^ ((D)g_veh_change << 20);
        memcpy(ieee + 0x40, &v, 8);
    }
}

// The fields of the IEEE record the error functions set; the record is a stack variable of _except1/2 and
// _87except that is not cleared, so the rest of its bytes are what was on the stack before (in both)
bool same_record(const BYTE *x, const BYTE *y, unsigned cw)
{
    const DWORD *a = (const DWORD *)x, *b = (const DWORD *)y;
    // the precision field stays as it was for the reserved precision 0x100
    const DWORD mode_mask = (cw & 0x300) == 0x100 ? 0x1ffe3 : 0x1ffff;
    if (((a[0] ^ b[0]) & mode_mask) || a[1] != b[1] || a[2] != b[2] || a[3] != b[3])
        return false;
    if (memcmp(x + 0x10, y + 0x10, 8) || ((a[8] ^ b[8]) & 0x1f) || memcmp(x + 0x40, y + 0x40, 8) || ((a[0x14] ^ b[0x14]) & 0x1f))
        return false;
    if ((a[0xe] ^ b[0xe]) & 1)   // Operand2.OperandValid, its value and format when set
        return false;
    return !(a[0xe] & 1) || (!memcmp(x + 0x28, y + 0x28, 8) && !((a[0xe] ^ b[0xe]) & 0x1f));
}

// A clean FPU before a case: fninit and the game's control word. fnclex is not enough on the VMs: after the
// reimplementation's _handle_exc had raised zero divide (masked, _set_statfp), the x86 emulation still showed
// ZE after the fnclex that followed and the next case's original side ended with ZE set (2026-10-28; fnsave
// right before that case read status word 4; the same case repeated gave the same status on both sides).
void clean_fpu()
{
    const WORD game = 0x27f;
    __asm__ volatile("fninit\n\tfldcw %0" : : "m"(game));
}

// The x87 traps of one side. On real x86 hardware an exception the error functions leave pending under a
// control word that unmasks it traps (#MF, STATUS_FLOAT_*) at the next waiting FP instruction, in the function or
// where the check takes its result; nothing caught that, and Zax ended (2026-10-31, Windows XP and 11 on x86,
// docs/DECOMPILATION.md 12). The VMs' x86 emulation on ARM never traps. While check_errors runs, a vectored
// handler in the check thread records the code and the status word's exception flags, clears them with ES and
// B in the context (FloatSave and, where the context has it, the FXSAVE image, which WOW64 restores from) and
// continues: the waiting instruction runs again without a pending exception. Both sides are compared by them.
struct Traps {
    int n;
    DWORD code[4];
    WORD flags[4];
};
Traps g_traps;
bool g_catch_traps;
int g_traps_total;

long WINAPI on_fp_trap(void *info)
{
    DWORD *const *p = static_cast<DWORD *const *>(info);
    const DWORD code = p[0][0];
    BYTE *ctx = reinterpret_cast<BYTE *>(p[1]);
    // STATUS_FLOAT_DENORMAL_OPERAND .. STATUS_FLOAT_UNDERFLOW, STATUS_FLOAT_MULTIPLE_FAULTS/TRAPS
    const bool fp = (code >= 0xc000008d && code <= 0xc0000093) || code == 0xc00002b4 || code == 0xc00002b5;
    if (!g_catch_traps || !fp || GetCurrentThreadId() != g_veh_thread)
        return 0;
    const DWORD cflags = *reinterpret_cast<const DWORD *>(ctx);
    WORD *const sw = (cflags & 0x10008) == 0x10008 ? reinterpret_cast<WORD *>(ctx + 0x20) : nullptr;   // FloatSave.StatusWord
    WORD *const fsw = (cflags & 0x10020) == 0x10020 ? reinterpret_cast<WORD *>(ctx + 0xcc + 2) : nullptr;   // FXSAVE's FSW
    if (!sw && !fsw)
        return 0;
    if (g_traps.n < 4) {
        g_traps.code[g_traps.n] = code;
        g_traps.flags[g_traps.n] = (sw ? *sw : *fsw) & 0x3f;
    }
    g_traps.n++;
    g_traps_total++;
    if (sw)
        *sw &= (WORD)~0x80bf;
    if (fsw)
        *fsw &= (WORD)~0x80bf;
    // A trap that comes back the same would loop: after 32 the context masks every exception
    if (g_traps.n > 32) {
        if (sw)
            *reinterpret_cast<WORD *>(ctx + 0x1c) |= 0x3f;
        if (fsw)
            *reinterpret_cast<WORD *>(ctx + 0xcc) |= 0x3f;
    }
    return -1;   // EXCEPTION_CONTINUE_EXECUTION
}

bool same_traps(const Traps &x, const Traps &y)
{
    if (x.n != y.n)
        return false;
    for (int i = 0; i < x.n && i < 4; i++)
        if (x.code[i] != y.code[i] || x.flags[i] != y.flags[i])
            return false;
    return true;
}

// " traps n code/flags ... / n code/flags ..." after a case's description, when a side trapped
void describe_traps(char *d, const Traps &x, const Traps &y)
{
    if (!x.n && !y.n)
        return;
    int o = lstrlenA(d);
    o += wsprintfA(d + o, " traps");
    for (int s = 0; s < 2; s++) {
        const Traps &t = s ? y : x;
        o += wsprintfA(d + o, s ? " / %d" : " %d", t.n);
        for (int i = 0; i < t.n && i < 4; i++)
            o += wsprintfA(d + o, " %x/%x", t.code[i], t.flags[i]);
    }
}

struct ExcInfo {
    int type;
    const char *name;
    D arg1, arg2, retval;
};

void check_errors(Tally &t)
{
    g_veh_thread = GetCurrentThreadId();
    ImportPatch raise_patch;
    patch_import(raise_patch, 0x5ea198, (void *)&fake_raise);
    g_real_raise = raise_patch.saved;
    g_traps_total = 0;
    void *const veh = AddVectoredExceptionHandler(1, &on_fp_trap);
    g_catch_traps = true;
    int trap_cases = 0;
    char d[600];
    g_seed = 0x5d5400;
    using Except1 = double(__cdecl *)(unsigned, int, D, D, unsigned);
    using Except2 = double(__cdecl *)(unsigned, int, D, D, D, unsigned);
    using Qnan1 = double(__cdecl *)(int, D, int);
    using Qnan2 = double(__cdecl *)(int, D, D, int);
    using Umath = double(__cdecl *)(int, int, D, D, D, int);
    using X87 = void(__cdecl *)(int, ExcInfo *, WORD *);
    using HandleExc = int(__cdecl *)(unsigned, D *, int);
    static const unsigned flag_list[] = {8, 4, 1, 2, 0x10, 0x11, 0x12, 0x20, 0x18, 0x3f, 0};
    static const int types[] = {1, 2, 3, 4, 5, 6, 7, 8, 0, 9};
    int *flag = at<int *>(0x62c570);
    const int saved_flag = *flag;
    for (int k = 0; k < scaled(3000); k++) {
        const int fn = k % 6;
        const unsigned flags = k < 200 ? flag_list[k % 11] : random() & 0x3f;
        const int opcode = k % 7 == 0 ? (int)(random() % 40) : (int)(11 + random() % 26);
        D a = some_double(k < 40 ? k : 40 + (int)(random() % 1000)), b = some_double(40 + (int)(random() % 1000));
        D res = some_double(random() % 2 ? (int)(random() % 40) : 40 + (int)(random() % 1000));
        // the control word value the functions are given (not loaded): exceptions masked or not, any rounding and precision
        unsigned cw = random() % 3 ? 0x27f : random() & 0x1f3f;
        if (random() % 2)
            cw &= ~(random() & 0x3f);   // some exceptions unmasked
        const int utype = types[random() % 10];
        const unsigned cws_given = cw;
        if ((cw & 0x3f) != 0x3f) {
            // the functions load this control word at their end and then load the result (fld): a signalling NaN or
            // a denormal there would trap; only normal numbers then (the game always runs with all masked)
            const auto normal = [](D v) {
                const unsigned e = (unsigned)(v >> 52) & 0x7ff;
                return e && e != 0x7ff ? v : 0x3ff8000000000000ull ^ (v & 0x800fffffffffffffull);
            };
            a = normal(a);
            b = normal(b);
            res = normal(res);
        }
        const DWORD veh_change = random();
        *flag = (k / 2) % 5 == 0 ? 0 : saved_flag;   // a fifth with the _matherr path
        Out o[2];
        int raised[2];
        BYTE record[2][0x58];
        DWORD code[2];
        D results[2];
        ExcInfo infos[2];
        WORD cws[2];
        Traps traps[2];
        // Both sides of the case; run again with the same inputs when they differ (below)
        const auto run = [&]() -> bool {
            g_veh_change = veh_change;
            for (int side = 0; side < 2; side++) {
                const auto addr = [side](DWORD x) { return side ? x : copy_of(x); };
                g_veh_raised = 0;
                memset(g_veh_record, 0, sizeof g_veh_record);
                g_veh_code = 0;
                *crt::errno_ptr() = 0;
                clean_fpu();
                g_traps = Traps{};
                long double r = 0;
                results[side] = res;
                if (g_fp_trace)
                    log("T %d fn %d side %d flags %x op %d cw %x", k, fn, side, flags, opcode, cw);
                switch (fn) {
                case 0: r = reinterpret_cast<Except1>(addr(0x5d5400))(flags, opcode, a, res, cw); break;
                case 1: r = reinterpret_cast<Except2>(addr(0x5d5498))(flags, opcode, a, b, res, cw); break;
                case 2: r = reinterpret_cast<Qnan1>(addr(0x5d534c))(opcode, a, (int)cw); break;
                case 3: r = reinterpret_cast<Qnan2>(addr(0x5d53a0))(opcode, a, b, (int)cw); break;
                case 4: r = reinterpret_cast<Umath>(addr(0x5d5a07))(utype, opcode, a, b, res, (int)cw); break;
                default: {
                    ExcInfo &e = infos[side];
                    e.type = types[k / 6 % 10];
                    e.name = "x";
                    e.arg1 = a;
                    e.arg2 = b;
                    e.retval = res;
                    cws[side] = (WORD)cw;
                    reinterpret_cast<X87>(addr(0x5dc4c5))(opcode, &e, &cws[side]);
                    break;
                }
                }
                memcpy(o[side].r, &r, 10);
                WORD sw, cwnow;
                __asm__ volatile("fnstsw %0\n\tfnstcw %1\n\tfnclex" : "=m"(sw), "=m"(cwnow));
                traps[side] = g_traps;
                o[side].sw = sw & 0x3f;
                o[side].cw = cwnow;
                o[side].tag = 0;
                o[side].eax = 0;
                o[side].err = *crt::errno_ptr();
                raised[side] = g_veh_raised;
                memcpy(record[side], g_veh_record, 0x58);
                code[side] = g_veh_code;
                const WORD game = 0x27f;
                __asm__ volatile("fldcw %0" : : "m"(game));
            }
            static const char *const names[] = {"_except1", "_except2", "_handle_qnan1", "_handle_qnan2", "_umatherr", "_87except"};
            describe(d, names[fn], a, res, (int)flags, (WORD)cw, o[0], o[1]);
            describe_traps(d, traps[0], traps[1]);
            bool same = same_out(o[0], o[1]) && same_traps(traps[0], traps[1]) && raised[0] == raised[1] && code[0] == code[1] && (!raised[0] || same_record(record[0], record[1], cws_given));
            if (fn == 5)
                same = same && !memcmp(&infos[0], &infos[1], sizeof infos[0]) && cws[0] == cws[1];
            return same;
        };
        // The x86 emulation of the VMs now and then leaves a stale ZE in the status word even after clean_fpu
        // (2026-10-28: case _except2 78e397e5e3dfe800 40c3880000000000 4 differed in 1 of 3 runs of the same
        // DLL, only in that bit). Both sides are deterministic in their inputs, so a case that differs is run
        // once more and counts only if it differs again; a case that only differed once is logged.
        bool same = run();
        if ((traps[0].n || traps[1].n) && ++trap_cases <= 3)
            log("  crt_rest: a case that trapped: %s", d);
        if (!same) {
            char first[600];
            lstrcpyA(first, d);
            same = run();
            if (same)
                log("  crt_rest: differed once, the same when repeated (the emulation's status word): %s", first);
        }
        t.check(same, "", d);
        // _handle_exc alone: the result it writes
        D pa = res, pb = res;
        const int cwi = (int)(random() & 0xffff);
        // (it runs under the game's control word, so it should not trap; the traps are compared all the same)
        clean_fpu();
        g_traps = Traps{};
        const int ha = reinterpret_cast<HandleExc>(copy_of(0x5d57f0))(flags, &pa, cwi);
        WORD swa, swb;
        __asm__ volatile("fnstsw %0" : "=m"(swa));
        traps[0] = g_traps;
        clean_fpu();
        g_traps = Traps{};
        const int hb = at<HandleExc>(0x5d57f0)(flags, &pb, cwi);
        __asm__ volatile("fnstsw %0" : "=m"(swb));
        traps[1] = g_traps;
        clean_fpu();
        wsprintfA(d, "[_handle_exc %x cw %x %08x%08x: %d %08x%08x %x / %d %08x%08x %x]", flags, cwi, (DWORD)(res >> 32), (DWORD)res,
                  ha, (DWORD)(pa >> 32), (DWORD)pa, swa, hb, (DWORD)(pb >> 32), (DWORD)pb, swb);
        describe_traps(d, traps[0], traps[1]);
        t.check(ha == hb && pa == pb && (swa & 0x3f) == (swb & 0x3f) && same_traps(traps[0], traps[1]), "", d);
    }
    *flag = saved_flag;
    g_catch_traps = false;
    RemoveVectoredExceptionHandler(veh);
    unpatch_import(raise_patch);
    __asm__ volatile("fnclex");
    log("  crt_rest: %d x87 traps caught in %d error cases (real x86 only; both sides and repeats)", g_traps_total, trap_cases);
}

}  // namespace

ZAX_CHECKS(crt_rest)
{
    (void)cls;
    (void)n;
    BYTE env[28];
    __asm__ volatile("fnstenv %0" : "=m"(env));
    Tally a{"crt_rest: math functions against the exe's library (pow, asin, acos, fmod, floor, ceil, ldexp, ...)"};
    Tally b{"crt_rest: _powhlp, _decomp, the control word, _errcode, _getfname, _set_errno"};
    Tally c{"crt_rest: _except1/2, _handle_qnan1/2, _umatherr, _87except, _handle_exc, _raise_exc"};
    if (!build_copy()) {
        a.check(false, "", "[no copy of the library]");
        a.report();
        return;
    }
    log("  %s ...", "math functions");
    check_math(a);
    a.report();
    log("  %s ...", "fp helpers");
    check_fp_helpers(b);
    b.report();
    log("  %s ...", "fp errors");
    check_errors(c);
    c.report();
    __asm__ volatile("fldenv %0" : : "m"(env));
}

}  // namespace zax
