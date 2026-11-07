// The C part of the C runtime's floating point library (VC6 fpexcept.c, util.c, ieee87.c, fpctrl.c, floor.c,
// ceil.c, ldexp.c, powhlp.c, fpinit.c, testfdiv.c; docs/DECOMPILATION.md 11, "C runtime, phase 5b"): the IEEE
// exception handling of the math functions (_except1/2, _handle_exc, _raise_exc, _umatherr, _87except,
// _handle_qnan1/2, _set_errno, _errcode, _getfname), the helpers (_sptype, _decomp, _set_exp, _fpclass,
// _copysign, _powhlp, _d_inttype), floor, ceil, ldexp, the control word (_control87, _controlfp,
// _abstract_cw, _hw_cw) and the start-up pieces (_fpmath, _cfltcvt_init, _setdefaultprecision, the Pentium
// FDIV test). The x87 parts that are assembly or inline assembly are in crt_x87.cpp.
//
// The original is compiled C whose results depend on which values pass through the FPU: every fld of a
// signalling NaN makes it quiet and raises invalid, every fld of a denormal raises the denormal flag, and
// fcom raises invalid on any NaN. Doubles are therefore kept as their bits (D) and every x87 instruction of
// the original is done by the helpers below at the same place (a copy with fld/fstp, a comparison with
// fcomp, an fadd, fild, fchs); where the original copies a double with integer moves, so does this code.
#include "crt_common.h"

namespace zax::crt {

namespace {

using D = unsigned long long;   // a double's bits

constexpr D ZERO = 0, ONE = 0x3ff0000000000000ull;
constexpr DWORD C_INF = 0x62a078, C_NAN = 0x62a080, C_DBL_MAX = 0x62a088, C_NEG_ZERO = 0x62a098;
constexpr DWORD MATHERR_FLAG = 0x62c570;           // nonzero: no user _matherr (errno instead)
constexpr DWORD FNAME_TABLE = 0x629fa0, FNAME_TABLE_END = 0x62a078;   // {opcode, name} of the math functions
constexpr DWORD ADJUST_FDIV = 0x715878;            // _adjust_fdiv
constexpr DWORD CW_FLOOR = 0x6297a8, CW_CEIL = 0x6297ac;   // the masked control words with round down / up

inline D bits_at(DWORD a) { return *at<const D *>(a); }
inline WORD hiword(D v) { return (WORD)(v >> 48); }
inline DWORD hi(D v) { return (DWORD)(v >> 32); }
inline DWORD lo(D v) { return (DWORD)v; }

// fld m64; fstp m64
inline D fcopy(D v)
{
    D r;
    __asm__ volatile("fldl %1\n\tfstpl %0" : "=m"(r) : "m"(v));
    return r;
}
// fld m64 a; fcomp m64 b; fnstsw ax: the condition bits for sahf (C0 = CF 0x100, C3 = ZF 0x4000)
inline WORD fcomp(D a, D b)
{
    WORD sw;
    __asm__ volatile("fldl %1\n\tfcompl %2\n\tfnstsw %%ax" : "=a"(sw) : "m"(a), "m"(b));
    return sw;
}
inline bool sw_ja(WORD sw) { return !(sw & 0x4100); }
inline bool sw_jae(WORD sw) { return !(sw & 0x100); }
inline bool sw_je(WORD sw) { return sw & 0x4000; }
// fld a; fadd m64 b; fstp
inline D fadd(D a, D b)
{
    D r;
    __asm__ volatile("fldl %1\n\tfaddl %2\n\tfstpl %0" : "=m"(r) : "m"(a), "m"(b));
    return r;
}
// fld; fchs; fstp
inline D fneg(D a)
{
    D r;
    __asm__ volatile("fldl %1\n\tfchs\n\tfstpl %0" : "=m"(r) : "m"(a));
    return r;
}
// fild m32; fstp m64
inline D fild(int i)
{
    D r;
    __asm__ volatile("fildl %1\n\tfstpl %0" : "=m"(r) : "m"(i));
    return r;
}
// the return: fld m64 into st(0)
inline double fret(D v)
{
    double r;
    __asm__ volatile("fldl %1" : "=t"(r) : "m"(v));
    return r;
}
// a double result (st(0)) stored with fstp m64
inline D fstp(double v)
{
    D r;
    __asm__ volatile("fstpl %0" : "=m"(r) : "t"(v) : "st");
    return r;
}

// The x87 functions of crt_x87.cpp through the exe's entry points (hooked), as the original calls them
inline int ctrlfp(int newctrl, int mask) { return fn<int(__cdecl *)(int, int)>(0x5d5c7c)(newctrl, mask); }
inline int statfp() { return fn<int(__cdecl *)()>(0x5d5c5f)(); }
inline int clrfp() { return fn<int(__cdecl *)()>(0x5d5c6d)(); }
inline void set_statfp(int sw) { fn<void(__cdecl *)(int)>(0x5d5c9f)(sw); }
inline D frnd(D x) { return fstp(fn<double(__cdecl *)(D)>(0x5d5b09)(x)); }
// _matherr 0x5dc595 (xor eax, eax; ret): the exe's 3 bytes in the DLL; 0 in the standalone exe (batch 6-10c)
#ifdef ZAX_STANDALONE
inline int user_matherr(void *) { return 0; }
#else
inline int user_matherr(void *e) { return at<int(__cdecl *)(void *)>(0x5dc595)(e); }   // _matherr: return 0
#endif

// struct _exception (0x20 bytes) as _umatherr and _87except fill it
struct Exception {
    int type;
    const char *name;
    D arg1, arg2, retval;
};
static_assert(sizeof(Exception) == 0x20, "_exception");

// _FPIEEE_RECORD (0x58 bytes): the bit fields as the original sets them
struct FpieeeRecord {
    DWORD mode;      // +0x00 RoundingMode:2, Precision:3, Operation:12
    DWORD cause;     // +0x04
    DWORD enable;    // +0x08
    DWORD status;    // +0x0c
    D op1;           // +0x10 Operand1.Value
    BYTE pad1[8];
    DWORD op1_flags;   // +0x20 OperandValid:1, Format:4
    DWORD pad2;
    D op2;           // +0x28
    BYTE pad3[8];
    DWORD op2_flags;   // +0x38
    DWORD pad4;
    D result;        // +0x40
    BYTE pad5[8];
    DWORD result_flags;   // +0x50
    DWORD pad6;
};
static_assert(sizeof(FpieeeRecord) == 0x58, "_FPIEEE_RECORD");

}  // namespace

// ---- helpers (util.c) ----------------------------------------------------------------------------------

// _sptype 0x5d5b44: 1 +inf, 2 -inf, 3 quiet NaN, 4 signalling NaN, 0 otherwise
int __cdecl sptype(D x)
{
    if (hi(x) == 0x7ff00000 && lo(x) == 0)
        return 1;
    if (hi(x) == 0xfff00000 && lo(x) == 0)
        return 2;
    const WORD e = hiword(x) & 0x7ff8;
    if (e == 0x7ff8)
        return 3;
    if (e == 0x7ff0 && ((hi(x) & 0x7ffff) || lo(x)))
        return 4;
    return 0;
}

// _set_exp 0x5d5b1b: x with the exponent exp (unbiased + 1, i.e. the mantissa in [0.5, 1) for exp 0), sign kept
double __cdecl set_exp(D x, int exp)
{
    D r = fcopy(x);
    const WORD w = (WORD)(((exp + 0x3fe) << 4) | (hiword(x) & 0x800f));
    r = (r & 0x0000ffffffffffffull) | (D)w << 48;
    return fret(r);
}

// _decomp 0x5d5b9e: mantissa in [0.5, 1) and exponent; 0 (and NaN, which compares equal) give 0 and 0;
// a denormal is normalized by shifting
double __cdecl decomp(D x, int *pexp)
{
    D man;
    int expn;
    if (sw_je(fcomp(x, ZERO))) {
        man = fcopy(ZERO);
        expn = 0;
    } else if (!(hiword(x) & 0x7ff0) && ((hi(x) & 0xfffff) || lo(x))) {
        expn = -1021;
        const bool neg = !sw_jae(fcomp(x, ZERO));
        DWORD h = hi(x), l = lo(x);
        while (!(h & 0x100000)) {
            h <<= 1;
            if (l & 0x80000000)
                h |= 1;
            l <<= 1;
            expn--;
        }
        h &= 0xffefffff;
        if (neg)
            h |= 0x80000000;
        man = fstp(set_exp(fcopy((D)h << 32 | l), 0));
    } else {
        man = fstp(set_exp(fcopy(x), 0));
        expn = ((hiword(x) >> 4) & 0x7ff) - 0x3fe;
    }
    *pexp = expn;
    return fret(man);
}

// _copysign 0x5dca99: x's magnitude with y's sign, by integer moves
double __cdecl copysign_(D x, D y)
{
    const DWORD h = ((hi(y) ^ hi(x)) & 0x7fffffff) ^ hi(y);
    return fret((D)h << 32 | lo(x));
}

// _fpclass 0x5dcaba: _FPCLASS_SNAN 1 (never: _sptype gets the value after an fld, which quiets it), QNAN 2,
// NINF 4, PINF 0x200, NN 8, ND 0x10, NZ 0x20, PZ 0x40, PD 0x80, PN 0x100
int __cdecl fpclass(D x)
{
    const WORD e = hiword(x) & 0x7ff0;
    if (e == 0x7ff0) {
        switch (sptype(fcopy(x))) {
        case 1: return 0x200;
        case 2: return 4;
        case 3: return 2;
        default: return 1;
        }
    }
    const bool neg = hiword(x) & 0x8000;
    if (!e && ((hi(x) & 0xfffff) || lo(x)))
        return neg ? 0x10 : 0x80;
    if (sw_je(fcomp(x, ZERO)))
        return neg ? 0x20 : 0x40;
    return neg ? 8 : 0x100;
}

// ---- errors (fpexcept.c) ----------------------------------------------------------------------------------

// _set_errno 0x5d5a8f: _DOMAIN (1) EDOM, _SING/_OVERFLOW (2, 3) ERANGE
void __cdecl set_errno(int type)
{
    if (type == 1)
        *errno_ptr() = 0x21;
    else if (type > 1 && type <= 3)
        *errno_ptr() = 0x22;
}

// _getfname 0x5d5ab7: the name of the function with this opcode
const char *__cdecl getfname(int opcode)
{
    for (DWORD e = glob_addr(FNAME_TABLE); e < glob_addr(FNAME_TABLE_END); e += 8)
        if (*at<int *>(e) == opcode)
            return *at<const char **>(e + 4);
    return nullptr;
}

// _errcode 0x5d5adc: the _matherr type of the exception flags (FP_P 0x20, FP_I 8, FP_Z 4, FP_O 1, FP_U 2)
int __cdecl errcode(unsigned flags)
{
    const BYTE f = (BYTE)flags;
    if (f & 0x20)
        return 5;   // _TLOSS
    if (f & 8)
        return 1;   // _DOMAIN
    if (f & 4)
        return 2;   // _SING
    if (f & 1)
        return 3;   // _OVERFLOW
    return (f & 2) << 1;   // _UNDERFLOW 4 or 0
}

// _umatherr 0x5d5a07: the _exception for _matherr (the default one returns 0, then errno); its retval
double __cdecl umatherr(int type, int opcode, D arg1, D arg2, D retval, int cw)
{
    const char *name = getfname(opcode);
    if (!name) {
        ctrlfp(cw, 0xffff);
        set_errno(type);
        return fret(retval);
    }
    Exception e;
    e.type = type;
    e.name = name;
    e.arg1 = arg1;
    e.arg2 = arg2;
    e.retval = retval;
    ctrlfp(cw, 0xffff);
    if (!user_matherr(&e))
        set_errno(type);
    return fret(e.retval);
}

// _handle_exc 0x5d57f0: the masked exceptions of flags are handled here (the IEEE default result, the
// status bit set by raising it again); 1 when nothing is left for _raise_exc
int __cdecl handle_exc(unsigned flags, D *presult, int cw)
{
    unsigned left = flags & 0x1f;
    if ((flags & 8) && (cw & 1)) {   // invalid, masked
        set_statfp(1);
        left &= ~8u;
    } else if ((flags & 4) && (cw & 4)) {   // zero divide, masked
        set_statfp(4);
        left &= ~4u;
    } else if ((flags & 1) && (cw & 8)) {   // overflow, masked: infinity or the largest number by the rounding
        set_statfp(8);
        D v;
        bool store = true;
        switch (cw & 0xc00) {
        case 0:   // to nearest
            v = sw_ja(fcomp(*presult, ZERO)) ? fcopy(bits_at(glob_addr(C_INF))) : fneg(bits_at(glob_addr(C_INF)));
            break;
        case 0x400:   // down
            v = !sw_ja(fcomp(*presult, ZERO)) ? fneg(bits_at(glob_addr(C_INF))) : fcopy(bits_at(glob_addr(C_DBL_MAX)));
            break;
        case 0x800:   // up
            v = !sw_ja(fcomp(*presult, ZERO)) ? fneg(bits_at(glob_addr(C_DBL_MAX))) : fcopy(bits_at(glob_addr(C_INF)));
            break;
        case 0xc00:   // chop
            v = sw_ja(fcomp(*presult, ZERO)) ? fcopy(bits_at(glob_addr(C_DBL_MAX))) : fneg(bits_at(glob_addr(C_DBL_MAX)));
            break;
        default:
            store = false;
        }
        if (store)
            *presult = fcopy(v);
        left &= ~1u;
    } else if ((flags & 2) && (cw & 0x10)) {   // underflow, masked: the denormal (or 0) of the scaled result
        int inexact = (flags & 0x10) ? 1 : 0;
        D t = fcopy(*presult);
        if (sw_je(fcomp(t, ZERO)))
            inexact = 1;
        else {
            int exp;
            t = fstp(decomp(fcopy(t), &exp));
            const int e = exp - 0x600;
            if (e < -1074) {
                t = fcopy(ZERO);
                inexact = 1;
            } else {
                const bool neg = !sw_jae(fcomp(t, ZERO));
                const WORD w = (WORD)(((BYTE)(hiword(t) & 0xff) & 0xf) | 0x10);
                t = (t & 0x0000ffffffffffffull) | (D)w << 48;
                if (e < -1021) {
                    DWORD h = hi(t), l = lo(t);
                    for (int n = -1021 - e; n; n--) {
                        if ((l & 1) && !inexact)
                            inexact = 1;
                        l >>= 1;
                        if (h & 1)
                            l |= 0x80000000;
                        h >>= 1;
                    }
                    t = (D)h << 32 | l;
                }
                if (neg)
                    t = fneg(t);
            }
            *presult = fcopy(t);
        }
        if (inexact)
            set_statfp(0x10);
        left &= ~2u;
    }
    if ((flags & 0x10) && (cw & 0x20)) {   // inexact, masked
        set_statfp(0x20);
        left &= ~0x10u;
    }
    return left == 0;
}

// _raise_exc 0x5d553d: an unmasked exception: the IEEE record (cause, enables, status, rounding, precision,
// operation, operands), RaiseException with it, then the control word and the result the handler left
void __cdecl raise_exc(FpieeeRecord *rec, unsigned *pcw, unsigned flags, int opcode, D *parg1, D *presult)
{
    DWORD code = flags;
    rec->cause = 0;
    rec->enable = 0;
    rec->status = 0;
    if (flags & 0x10) {
        code = 0xc000008f;   // STATUS_FLOAT_INEXACT_RESULT
        rec->cause |= 1;
    }
    if (flags & 2) {
        code = 0xc0000093;   // underflow
        rec->cause |= 2;
    }
    if (flags & 1) {
        code = 0xc0000091;   // overflow
        rec->cause |= 4;
    }
    if (flags & 4) {
        code = 0xc000008e;   // divide by zero
        rec->cause |= 8;
    }
    if (flags & 8) {
        code = 0xc0000090;   // invalid operation
        rec->cause |= 0x10;
    }
    const unsigned cw = *pcw;
    rec->enable = (rec->enable & ~0x10u) | ((~cw & 1) << 4);
    rec->enable = (rec->enable & ~8u) | ((~cw & 4) << 1);
    rec->enable = (rec->enable & ~4u) | ((~cw >> 1) & 4);
    rec->enable = (rec->enable & ~2u) | ((~cw >> 3) & 2);
    rec->enable = (rec->enable & ~1u) | ((~cw >> 5) & 1);
    const int sw = statfp();
    if (sw & 1)
        rec->status |= 0x10;
    if (sw & 4)
        rec->status |= 8;
    if (sw & 8)
        rec->status |= 4;
    if (sw & 0x10)
        rec->status |= 2;
    if (sw & 0x20)
        rec->status |= 1;
    switch (*pcw & 0xc00) {
    case 0: rec->mode &= ~3u; break;
    case 0x400: rec->mode = (rec->mode & ~2u) | 1; break;
    case 0x800: rec->mode = (rec->mode & ~1u) | 2; break;
    case 0xc00: rec->mode |= 3; break;
    }
    switch (*pcw & 0x300) {
    case 0: rec->mode = (rec->mode & ~0x14u) | 8; break;
    case 0x200: rec->mode = (rec->mode & ~0x18u) | 4; break;
    case 0x300: rec->mode &= ~0x1cu; break;
    }
    rec->mode = ((opcode & 0xfff) << 5) | (rec->mode & 0xfffe001f);
    rec->op1_flags |= 1;
    rec->op1_flags = (rec->op1_flags & ~0x1cu) | 2;
    rec->op1 = fcopy(*parg1);
    rec->result_flags |= 1;
    rec->result_flags = (rec->result_flags & ~0x1cu) | 2;
    rec->result = fcopy(*presult);
    clrfp();
    FpieeeRecord *arg = rec;
    api<void(WINAPI *)(DWORD, DWORD, DWORD, const void *)>(iat::RaiseException)(code, 0, 1, &arg);
    // what a handler changed
    if (rec->enable & 0x10)
        *pcw &= ~1u;
    if (rec->enable & 8)
        *pcw &= ~4u;
    if (rec->enable & 4)
        *pcw &= ~8u;
    if (rec->enable & 2)
        *pcw &= ~0x10u;
    if (rec->enable & 1)
        *pcw &= ~0x20u;
    switch (rec->mode & 3) {
    case 0: *pcw &= 0xfffff3ff; break;
    case 1: *pcw = (*pcw & ~0x800u) | 0x400; break;
    case 2: *pcw = (*pcw & ~0x400u) | 0x800; break;
    case 3: *pcw |= 0xc00; break;
    }
    // the precision back: the original clears the rounding bits (0xc00) here, not the precision bits
    switch ((rec->mode >> 2) & 7) {
    case 0: *pcw = (*pcw & 0xfffff3ff) | 0x300; break;
    case 1: *pcw = (*pcw & 0xfffff3ff) | 0x200; break;
    case 2: *pcw &= 0xfffff3ff; break;
    }
    *presult = fcopy(rec->result);
}

// _except1 0x5d5400: a one-argument function's IEEE exception: masked ones handled, unmasked ones raised;
// then _matherr/errno and the control word back
double __cdecl except1(unsigned flags, int opcode, D arg, D res, unsigned cw)
{
    FpieeeRecord rec;
    if (!handle_exc(flags, &res, (int)cw)) {
        rec.op2_flags &= ~1u;
        raise_exc(&rec, &cw, flags, opcode, &arg, &res);
    }
    const int type = errcode(flags);
    if (!*glob<int *>(MATHERR_FLAG) && type)
        return umatherr(type, opcode, fcopy(arg), fcopy(ZERO), fcopy(res), (int)cw);
    set_errno(type);
    ctrlfp((int)cw, 0xffff);
    return fret(res);
}

// _except2 0x5d5498: the same with two arguments
double __cdecl except2(unsigned flags, int opcode, D arg1, D arg2, D res, unsigned cw)
{
    FpieeeRecord rec;
    if (!handle_exc(flags, &res, (int)cw)) {
        rec.op2_flags = (rec.op2_flags & 0xffffffe3) | 3;
        rec.op2 = fcopy(arg2);
        raise_exc(&rec, &cw, flags, opcode, &arg1, &res);
    }
    const int type = errcode(flags);
    if (!*glob<int *>(MATHERR_FLAG) && type)
        return umatherr(type, opcode, fcopy(arg1), fcopy(arg2), fcopy(res), (int)cw);
    set_errno(type);
    ctrlfp((int)cw, 0xffff);
    return fret(res);
}

// _handle_qnan1 0x5d534c: a quiet NaN argument: _DOMAIN, the NaN as the result
double __cdecl handle_qnan1(int opcode, D x, int savedcw)
{
    if (!*glob<int *>(MATHERR_FLAG))
        return umatherr(1, opcode, fcopy(x), fcopy(ZERO), fcopy(x), savedcw);
    *errno_ptr() = 0x21;
    ctrlfp(savedcw, 0xffff);
    return fret(x);
}

// _handle_qnan2 0x5d53a0: two arguments, x + y as the result
double __cdecl handle_qnan2(int opcode, D x, D y, int savedcw)
{
    const D r = fadd(x, y);
    if (!*glob<int *>(MATHERR_FLAG))
        return umatherr(1, opcode, fcopy(x), fcopy(y), fcopy(r), savedcw);
    *errno_ptr() = 0x21;
    ctrlfp(savedcw, 0xffff);
    return fret(r);
}

// _87except 0x5dc4c5: the error of an x87 function (__startOneArgErrorHandling): the _exception's type as
// IEEE flags, masked ones handled, unmasked ones raised (two-argument functions with their second
// argument), the control word back, _matherr/errno
void __cdecl x87except(int opcode, Exception *info, WORD *pcw)
{
    unsigned cw = *pcw;
    unsigned flags;
    switch (info->type) {
    case 1: case 5: flags = 8; break;     // _DOMAIN, _TLOSS: invalid
    case 2: flags = 4; break;             // _SING: zero divide
    case 3: flags = 0x11; break;          // _OVERFLOW: overflow, inexact
    case 4: flags = 0x12; break;          // _UNDERFLOW: underflow, inexact
    case 7: info->type = 1; flags = 0; break;
    case 8: flags = 0x10; break;          // precision loss: inexact
    default: flags = 0;
    }
    if (flags) {
        if (!handle_exc(flags, &info->retval, (int)cw)) {
            FpieeeRecord rec;
            if (opcode == 0x10 || opcode == 0x16 || opcode == 0x1d) {   // atan2, fmod, pow
                rec.op2 = fcopy(info->arg2);
                rec.op2_flags = (rec.op2_flags & 0xffffffe3) | 3;
            } else
                rec.op2_flags &= ~1u;
            raise_exc(&rec, &cw, flags, opcode, &info->arg1, &info->retval);
        }
    }
    ctrlfp((int)cw, 0xffff);
    if (info->type == 8 || *glob<int *>(MATHERR_FLAG) || !user_matherr(info))
        set_errno(info->type);
}

// ---- functions -----------------------------------------------------------------------------------------

namespace {

// floor 0x5d139b / ceil 0x5d146a: frndint with the rounding of cwslot; specials through _sptype
double round_fn(D x, DWORD cwslot, int opcode)
{
    const int savedcw = ctrlfp(*at<int *>(cwslot), 0xffff);
    if ((hiword(x) & 0x7ff0) == 0x7ff0) {
        const int t = sptype(fcopy(x));
        if (t > 0 && t <= 2) {
            ctrlfp(savedcw, 0xffff);
            return fret(x);
        }
        if (t == 3)
            return handle_qnan1(opcode, fcopy(x), savedcw);
        const D sum = fadd(x, ONE);
        return except1(8, opcode, fcopy(x), sum, (unsigned)savedcw);
    }
    const D r = frnd(fcopy(x));
    if (sw_je(fcomp(r, x)) || (savedcw & 0x20)) {
        ctrlfp(savedcw, 0xffff);
        return fret(r);
    }
    return except1(0x10, opcode, fcopy(x), fcopy(r), (unsigned)savedcw);
}

}  // namespace

double __cdecl floor_(D x) { return round_fn(x, glob_addr(CW_FLOOR), 0xb); }
double __cdecl ceil_(D x) { return round_fn(x, glob_addr(CW_CEIL), 0xc); }

// ldexp 0x5ddb6e: x * 2^n by the exponent (_decomp, _set_exp); overflow and underflow through _except2
double __cdecl ldexp_(D x, int n)
{
    const int savedcw = ctrlfp(0x133f, 0xffff);
    if ((hiword(x) & 0x7ff0) == 0x7ff0) {
        const int t = sptype(fcopy(x));
        if (t > 0 && t <= 2) {
            ctrlfp(savedcw, 0xffff);
            return fret(x);
        }
        if (t == 3)
            return handle_qnan2(0x19, fcopy(x), fcopy(fild(n)), savedcw);
        const D sum = fadd(x, ONE);
        return except2(8, 0x19, fcopy(x), fcopy(fild(n)), sum, (unsigned)savedcw);
    }
    if (sw_je(fcomp(x, ZERO))) {
        ctrlfp(savedcw, 0xffff);
        return fret(x);
    }
    int exp;
    D m = fstp(decomp(fcopy(x), &exp));
    const int e = (int)((unsigned)n + (unsigned)exp);   // the original's guard |n| > 0x7fffffff never holds
    if (e > 0xa00) {
        const D big = fstp(copysign_(fcopy(bits_at(glob_addr(C_INF))), fcopy(m)));
        return except2(0x11, 0x19, fcopy(x), fcopy(fild(n)), big, (unsigned)savedcw);
    }
    if (e > 0x400) {
        m = fstp(set_exp(fcopy(m), e - 0x600));
        return except2(0x11, 0x19, fcopy(x), fcopy(fild(n)), fcopy(m), (unsigned)savedcw);
    }
    if (e < -0x9fd)
        return except2(0x12, 0x19, fcopy(x), fcopy(fild(n)), fcopy(ZERO), (unsigned)savedcw);
    if (e < -0x3fd) {
        m = fstp(set_exp(fcopy(m), e + 0x600));
        return except2(0x12, 0x19, fcopy(x), fcopy(fild(n)), fcopy(m), (unsigned)savedcw);
    }
    m = fstp(set_exp(fcopy(m), e));
    ctrlfp(savedcw, 0xffff);
    return fret(m);
}

// _d_inttype 0x5d9987: 0 not an integer (or NaN/infinity), 1 odd, 2 even
int __cdecl d_inttype(D y)
{
    if (fpclass(fcopy(y)) & 0x90)   // _FPCLASS_ND | _FPCLASS_PD: a denormal is no integer
        return 0;
    const D r = frnd(fcopy(y));
    if (!sw_je(fcomp(r, y)))
        return 0;
    // the argument slot is overwritten with y / 2
    D half;
    __asm__ volatile("fldl %1\n\tfdivl %2\n\tfstpl %0" : "=m"(half) : "m"(y), "m"(*glob<const D *>(0x5ee680)));
    const D h = frnd(fcopy(half));
    return sw_je(fcomp(h, half)) ? 2 : 1;
}

// _powhlp 0x5d9857: pow's special cases with an infinite argument; the result into *res and 1 when it is the
// NaN of |x| = 1 with an infinite y; nothing stored when neither argument is an infinity
int __cdecl powhlp(D x, D y, D *res)
{
    const D ax = sw_jae(fcomp(x, ZERO)) ? fcopy(x) : fneg(x);   // |x|, negated when below 0 or unordered
    D r;
    if (hi(y) == 0x7ff00000 && lo(y) == 0) {   // y = +inf
        if (sw_ja(fcomp(ax, ONE)))
            r = fcopy(bits_at(glob_addr(C_INF)));
        else if (!sw_jae(fcomp(ax, ONE)))
            r = fcopy(ZERO);
        else {
            *res = fcopy(bits_at(glob_addr(C_NAN)));
            return 1;
        }
    } else if (hi(y) == 0xfff00000 && lo(y) == 0) {   // y = -inf
        if (sw_ja(fcomp(ax, ONE)))
            r = fcopy(ZERO);
        else if (sw_jae(fcomp(ax, ONE))) {
            *res = fcopy(bits_at(glob_addr(C_NAN)));
            return 1;
        } else
            r = fcopy(bits_at(glob_addr(C_INF)));
    } else if (hi(x) == 0x7ff00000) {   // x = +inf (a NaN with this high word: nothing)
        if (lo(x) != 0)
            return 0;
        if (sw_ja(fcomp(y, ZERO)))
            r = fcopy(bits_at(glob_addr(C_INF)));
        else if (sw_jae(fcomp(y, ZERO)))
            r = fcopy(ONE);
        else
            r = fcopy(ZERO);
    } else if (hi(x) == 0xfff00000 && lo(x) == 0) {   // x = -inf: the sign by y's parity
        const int t = d_inttype(fcopy(y));
        if (sw_ja(fcomp(y, ZERO)))
            r = fcopy(t == 1 ? fneg(bits_at(glob_addr(C_INF))) : fcopy(bits_at(glob_addr(C_INF))));
        else if (sw_jae(fcomp(y, ZERO)))
            r = fcopy(ONE);
        else
            r = fcopy(t == 1 ? fcopy(bits_at(glob_addr(C_NEG_ZERO))) : fcopy(ZERO));
    } else
        return 0;
    *res = fcopy(r);
    return 0;
}

// ---- the control word (fpctrl.c) ---------------------------------------------------------------------------

// _abstract_cw 0x5dbfef: the x87 control word as _controlfp's bits
unsigned __cdecl abstract_cw(WORD cw)
{
    unsigned r = 0;
    if (cw & 1) r = 0x10;          // _EM_INVALID
    if (cw & 4) r |= 8;            // _EM_ZERODIVIDE
    if (cw & 8) r |= 4;            // _EM_OVERFLOW
    if (cw & 0x10) r |= 2;         // _EM_UNDERFLOW
    if (cw & 0x20) r |= 1;         // _EM_INEXACT
    if (cw & 2) r |= 0x80000;      // _EM_DENORMAL
    switch (cw & 0xc00) {
    case 0x400: r |= 0x100; break;   // _RC_DOWN
    case 0x800: r |= 0x200; break;   // _RC_UP
    case 0xc00: r |= 0x300; break;   // _RC_CHOP
    }
    switch (cw & 0x300) {
    case 0: r |= 0x20000; break;     // _PC_24
    case 0x200: r |= 0x10000; break; // _PC_53
    }
    if (cw & 0x1000)
        r |= 0x40000;                // _IC_AFFINE
    return r;
}

// _hw_cw 0x5dc081: _controlfp's bits as the x87 control word
unsigned __cdecl hw_cw(unsigned a)
{
    unsigned r = 0;
    if (a & 0x10) r = 1;
    if (a & 8) r |= 4;
    if (a & 4) r |= 8;
    if (a & 2) r |= 0x10;
    if (a & 1) r |= 0x20;
    if (a & 0x80000) r |= 2;
    switch (a & 0x300) {
    case 0x100: r |= 0x400; break;
    case 0x200: r |= 0x800; break;
    case 0x300: r |= 0xc00; break;
    }
    switch (a & 0x30000) {
    case 0: r |= 0x300; break;
    case 0x10000: r |= 0x200; break;
    }
    if (a & 0x40000)
        r |= 0x1000;
    return r;
}

// _control87 0x5dbfa4: the abstract control word changed under mask, loaded; the new abstract one
unsigned __cdecl control87(unsigned newval, unsigned mask)
{
    WORD cw;
    __asm__ volatile("fwait\n\tfnstcw %0" : "=m"(cw));
    const unsigned a = (abstract_cw(cw) & ~mask) | (newval & mask);
    const DWORD hw = hw_cw(a);
    __asm__ volatile("fldcw %0" : : "m"(hw));
    return a;
}

// _controlfp 0x5dbfd9: _control87 without the denormal mask bit
unsigned __cdecl controlfp(unsigned newval, unsigned mask) { return control87(newval, mask & 0xfff7ffff); }

// ---- start-up (fpinit.c, testfdiv.c) ------------------------------------------------------------------------

// _cfltcvt_init 0x5d044d: the float conversion table of printf/scanf (_cfltcvt_tab 0x629d78) with the exe's functions
void __cdecl cfltcvt_init()
{
    *glob<DWORD *>(0x629d7c) = fn_addr(0x5d3e22);   // _cropzeros
    *glob<DWORD *>(0x629d78) = fn_addr(0x5d4178);   // _cfltcvt
    *glob<DWORD *>(0x629d80) = fn_addr(0x5d3e88);   // __fassign
    *glob<DWORD *>(0x629d84) = fn_addr(0x5d3dc8);   // _forcdecpt
    *glob<DWORD *>(0x629d88) = fn_addr(0x5d3e70);   // _positive
    *glob<DWORD *>(0x629d8c) = fn_addr(0x5d4178);   // _cfltcvt
}

// _ms_p5_test_fdiv 0x5d3d61: 1 when x - (x / y) * y > 1 for the known bad operands of the Pentium FDIV bug
int __cdecl ms_p5_test_fdiv()
{
    const D x = fcopy(bits_at(glob_addr(0x6070d8))), y = fcopy(bits_at(glob_addr(0x6070e0)));
    D r;
    __asm__ volatile("fldl %1\n\tfdivl %2\n\tfmull %2\n\tfsubrl %1\n\tfstpl %0" : "=m"(r) : "m"(x), "m"(y));
    return sw_ja(fcomp(r, ONE)) ? 1 : 0;
}

// _ms_p5_mp_test_fdiv 0x5d3d9f: IsProcessorFeaturePresent(PF_FLOATING_POINT_PRECISION_ERRATA) where
// kernel32 has it, otherwise the test
int __cdecl ms_p5_mp_test_fdiv()
{
    HANDLE k = api<HANDLE(WINAPI *)(const char *)>(iat::GetModuleHandleA)(glob<const char *>(0x607104));
    if (k)
        if (auto f = api<int(WINAPI *)(HANDLE, const char *)>(iat::GetProcAddress)(k, glob<const char *>(0x6070e8)))
            return reinterpret_cast<int(WINAPI *)(int)>(f)(0);
    return fn<int(__cdecl *)()>(0x5d3d61)();
}

// _setdefaultprecision 0x5d3d4f: 53 bits
void __cdecl setdefaultprecision() { fn<unsigned(__cdecl *)(unsigned, unsigned)>(0x5dbfd9)(0x10000, 0x30000); }

// _fpmath 0x5d0435 (through _fpinit in _cinit): the conversion table, _adjust_fdiv, 53-bit precision,
// the exceptions cleared
void __cdecl fpmath()
{
    fn<void(__cdecl *)()>(0x5d044d)();
    *glob<int *>(ADJUST_FDIV) = fn<int(__cdecl *)()>(0x5d3d9f)();
    fn<void(__cdecl *)()>(0x5d3d4f)();
    __asm__ volatile("fnclex");
}

// _fptrap 0x5dc293: the floating point library is not linked (R6002)
void __cdecl fptrap() { fn<void(__cdecl *)(int)>(0x5d3397)(2); }

}  // namespace zax::crt

namespace zax {

using namespace crt;

ZAX_HOOKS(crt_fp)
{
    const Hook list[] = {
        {0x5d139b, (void *)&floor_, "floor", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0x53, 0x56, 0xbe}},
        {0x5d146a, (void *)&ceil_, "ceil", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0x53, 0x56, 0xbe}},
        {0x5d534c, (void *)&handle_qnan1, "_handle_qnan1", 10, {0x55, 0x8b, 0xec, 0x83, 0x3d, 0x70, 0xc5, 0x62}},
        {0x5d53a0, (void *)&handle_qnan2, "_handle_qnan2", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0xdd, 0x45, 0x0c}},
        {0x5d5400, (void *)&except1, "_except1", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x58, 0xff, 0x75}},
        {0x5d5498, (void *)&except2, "_except2", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x58, 0xff, 0x75}},
        {0x5d553d, (void *)&raise_exc, "_raise_exc", 6, {0x55, 0x8b, 0xec, 0x8b, 0x4d, 0x08, 0x33, 0xc0}},
        {0x5d57f0, (void *)&handle_exc, "_handle_exc", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x8b, 0x45}},
        {0x5d5a07, (void *)&umatherr, "_umatherr", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x20, 0xff, 0x75}},
        {0x5d5a8f, (void *)&set_errno, "_set_errno", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xf8, 0x01, 0x74}},
        {0x5d5ab7, (void *)&getfname, "_getfname", 7, {0x33, 0xc9, 0xb8, 0xa0, 0x9f, 0x62, 0x00, 0x8b}},
        {0x5d5adc, (void *)&errcode, "_errcode", 6, {0x8a, 0x44, 0x24, 0x04, 0xa8, 0x20, 0x74, 0x04}},
        {0x5d5b1b, (void *)&set_exp, "_set_exp", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0x8b, 0x45, 0x10}},
        {0x5d5b44, (void *)&sptype, "_sptype", 5, {0x55, 0x8b, 0xec, 0x33, 0xd2, 0x81, 0x7d, 0x0c}},
        {0x5d5b9e, (void *)&decomp, "_decomp", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0xdd, 0x45, 0x08}},
        {0x5d9857, (void *)&powhlp, "_powhlp", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0xdd, 0x45, 0x08}},
        {0x5d9987, (void *)&d_inttype, "_d_inttype", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0xdd, 0x45, 0x08}},
        {0x5dc4c5, (void *)&x87except, "_87except", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x58, 0x8b, 0x45}},
        {0x5dca99, (void *)&copysign_, "_copysign", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0x8b, 0x45, 0x08}},
        {0x5dcaba, (void *)&fpclass, "_fpclass", 6, {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x0e, 0xb9, 0xf0}},
        {0x5ddb6e, (void *)&ldexp_, "ldexp", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x56, 0xbe}},
        {0x5dbfa4, (void *)&control87, "_control87", 5, {0x55, 0x8b, 0xec, 0x51, 0x56, 0x9b, 0xd9, 0x7d}},
        {0x5dbfd9, (void *)&controlfp, "_controlfp", 9, {0x8b, 0x44, 0x24, 0x08, 0x25, 0xff, 0xff, 0xf7}},
        {0x5dbfef, (void *)&abstract_cw, "_abstract_cw", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x33, 0xc0, 0x55}},
        {0x5dc081, (void *)&hw_cw, "_hw_cw", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x33, 0xc0, 0x56}},
        {0x5d044d, (void *)&cfltcvt_init, "_cfltcvt_init", 5, {0xb8, 0x78, 0x41, 0x5d, 0x00, 0xc7, 0x05, 0x7c}},
        {0x5d3d61, (void *)&ms_p5_test_fdiv, "_ms_p5_test_fdiv", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x18, 0xdd, 0x05}},
        {0x5d3d9f, (void *)&ms_p5_mp_test_fdiv, "_ms_p5_mp_test_fdiv", 5, {0x68, 0x04, 0x71, 0x60, 0x00, 0xff, 0x15, 0x30}},
        {0x5d3d4f, (void *)&setdefaultprecision, "_setdefaultprecision", 5, {0x68, 0x00, 0x00, 0x03, 0x00, 0x68, 0x00, 0x00}},
        {0x5d0435, (void *)&fpmath, "_fpmath", 5, {0xe8, 0x13, 0x00, 0x00, 0x00, 0xe8, 0x60, 0x39}, 1},
        {0x5dc293, (void *)&fptrap, "_fptrap", 7, {0x6a, 0x02, 0xe8, 0xfd, 0x70, 0xff, 0xff, 0x59}, 3},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
