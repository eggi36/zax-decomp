// The C runtime's decimal conversion of floating point numbers (docs/DECOMPILATION.md 10, "C runtime"):
// the 96-bit ("12-byte long double") arithmetic, $I10_OUTPUT (binary -> decimal digits), __dtold,
// _fltout2, _fptostr and the %e/%f/%g layouts (_cftoe, _cftof, _cftog with their second halves,
// _cropzeros, _forcdecpt), and the input side: __mtold12 (digits -> 96 bits), __strgtold12 (text ->
// 96 bits) and _ld12cvt (96 bits -> double or float), _atodbl, _atoflt.
//
// The numbers are kept as in the exe: an Ld12 is 12 bytes, a mantissa of 80 bits (bytes 0-9, the top bit
// explicit) and the sign and biased exponent (bytes 10-11, bias 0x3fff). Every step is the exe's: a
// product keeps only the upper 80 of its 96 bits, sums carry the way the exe's do (one carry into the next
// 16-bit word that may itself wrap), the powers of ten come from the exe's tables (0x62c6b4, 0x62c814:
// 10^1..10^7, 10^8..10^56, ... 10^4096; 26 of the 58 entries are not the correctly rounded value, so
// they cannot be recomputed), and the rounding of the digit strings is decimal, on the text.
#pragma once
#include "reimpl.h"

namespace zax::crt {

struct Ld12 {
    BYTE b[12];
    DWORD &d(int i) { return *(DWORD *)(b + 4 * i); }
    WORD &w(int i) { return *(WORD *)(b + 2 * i); }
    DWORD d(int i) const { return *(const DWORD *)(b + 4 * i); }
    WORD w(int i) const { return *(const WORD *)(b + 2 * i); }
    DWORD &du(int off) { return *(DWORD *)(b + off); }   // unaligned dword at a byte offset
};

// The 10-byte long double the exe passes around (mantissa, then sign and exponent)
struct Ld10 {
    DWORD lo, hi;
    WORD exp;
};

// _FOS of $I10_OUTPUT: decimal exponent, sign (' ' or '-'), number of digits, the digits
struct Fos {
    short exp;
    char sign;
    char man_len;
    char man[22];
};
static_assert(sizeof(Fos) == 26, "_FOS");

// _strflt: sign character, decimal exponent, the flag of $I10_OUTPUT, the digits
struct StrFlt {
    int sign, decpt, flag;
    char *mantissa;
};

// ---- 96-bit helpers (0x5dd09e add with carry, ___add_12 0x5dd0bf, shl 0x5dd11d, shr 0x5dd14b) -------------

inline int add_carry(DWORD x, DWORD y, DWORD *sum)
{
    const DWORD s = x + y;
    *sum = s;
    return s < x || s < y;
}
inline void add_12(Ld12 &a, const Ld12 &b)
{
    if (add_carry(a.d(0), b.d(0), &a.d(0)))
        if (add_carry(a.d(1), 1, &a.d(1)))
            a.d(2)++;
    if (add_carry(a.d(1), b.d(1), &a.d(1)))
        a.d(2)++;
    add_carry(a.d(2), b.d(2), &a.d(2));
}
inline void shl_12(Ld12 &a)
{
    const DWORD c0 = a.d(0) >> 31, c1 = a.d(1) >> 31;
    a.d(0) <<= 1;
    a.d(1) = a.d(1) << 1 | c0;
    a.d(2) = a.d(2) << 1 | c1;
}
inline void shr_12(Ld12 &a)
{
    const DWORD c2 = a.d(2) << 31, c1 = a.d(1) << 31;
    a.d(1) = a.d(1) >> 1 | c2;
    a.d(0) = a.d(0) >> 1 | c1;
    a.d(2) >>= 1;
}

// __ld12mul 0x5dd23f: *x *= *y
void ld12mul(Ld12 *x, const Ld12 *y);
// __multtenpow12 0x5dd45f: *x *= 10^pow from the exe's tables; mult12 = 0 clears the lowest 16 bits first
void multtenpow12(Ld12 *x, int pow, int mult12);
// __mtold12 0x5dd178: decimal digits (bytes 0-9) to a normalized 96-bit number
void mtold12(const char *man, unsigned len, Ld12 *out);

// $I10_OUTPUT 0x5dd69a: at most 21 digits (ndigits significant ones, or with flags & 1 ndigits after the
// decimal point), rounded on the text; returns 0 for infinities and NaNs ("1#INF", "1#QNAN", "1#SNAN",
// "1#IND" as the digits), else 1
int i10_output(Ld10 ld, int ndigits, unsigned flags, Fos *fos);
// __dtold 0x5dc1dd
void dtold(Ld10 *out, const double *x);
// _fltout2 0x5dc181: 17 digits of a double
StrFlt *fltout2(double x, StrFlt *flt, char *digits);
// _fptostr 0x5dc10a
void fptostr(char *buf, int digits, StrFlt *flt);

// The layouts; `point` is the decimal point character (the exe's __decimal_point 0x629d70, '.')
char *cftoe(const double *x, char *buf, int ndec, int caps, char point);
char *cftoe2(char *buf, int ndec, int caps, StrFlt *flt, char g_fmt, char point);
char *cftof(const double *x, char *buf, int ndec, char point);
char *cftof2(char *buf, int ndec, StrFlt *flt, char g_fmt, char point);
char *cftog(const double *x, char *buf, int ndec, int caps, char point);
void cfltcvt(const double *x, char *buf, int format, int precision, int caps, char point);
void cropzeros(char *buf, char point);
void forcdecpt(char *buf, char point);

// ---- input -----------------------------------------------------------------------------------------------

// __strgtold12 0x5dbad3: the result flags (SLD_UNDERFLOW 1, SLD_OVERFLOW 2, SLD_NODIGITS 4) and *end
unsigned strgtold12(Ld12 *out, const char **end, const char *s, int mult12, char point);
// _ld12cvt 0x5db8e1 through _ld12tod 0x5dba4d / _ld12tof 0x5dba63: INTRNCVT_OK 0, OVERFLOW 1, UNDERFLOW 2
int ld12tod(const Ld12 *in, double *out);
int ld12tof(const Ld12 *in, float *out);
// _atodbl 0x5dba79, _atoflt 0x5dbaa6
int atodbl(double *out, const char *s, char point);
int atoflt(float *out, const char *s, char point);

}  // namespace zax::crt
