// Portable versions of the MSVC 6 C runtime functions whose exact results reach game data (docs/
// DECOMPILATION.md, "Libraries and copy protection", C runtime behaviour). The port uses these instead of
// the platform's C library; checks_crt.cpp compares them with the exe's runtime. Only the "C" locale:
// the exe never calls setlocale, so __lc_handle[LC_CTYPE] stays 0 and the case functions are ASCII.
#pragma once
#include "reimpl.h"

namespace zax::crt {

// ---- character classes and case (C locale) ----------------------------------------------------------------

inline bool is_space(int c) { return c == ' ' || (c >= 9 && c <= 13); }
inline bool is_digit(int c) { return c >= '0' && c <= '9'; }
inline bool is_upper(int c) { return c >= 'A' && c <= 'Z'; }
inline bool is_lower(int c) { return c >= 'a' && c <= 'z'; }
inline bool is_alpha(int c) { return is_upper(c) || is_lower(c); }
inline int to_upper(int c) { return is_lower(c) ? c - 0x20 : c; }
inline int to_lower(int c) { return is_upper(c) ? c + 0x20 : c; }

// _stricmp 0x5d3640 / _strnicmp 0x5dd9c0: bytes lowered (A-Z only), -1/0/1
inline int stricmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        const int x = to_lower((BYTE)*a), y = to_lower((BYTE)*b);
        if (x != y)
            return x < y ? -1 : 1;
        if (!x)
            return 0;
    }
}
inline int strnicmp(const char *a, const char *b, unsigned n)
{
    for (; n; n--, a++, b++) {
        const int x = to_lower((BYTE)*a), y = to_lower((BYTE)*b);
        if (x != y)
            return x < y ? -1 : 1;
        if (!x)
            return 0;
    }
    return 0;
}

// ---- numbers from text ------------------------------------------------------------------------------------

// atol 0x5d0348 (atoi 0x5d03d3 is the same): white space, one sign, decimal digits; wraps on overflow
inline long atol(const char *s)
{
    const BYTE *p = (const BYTE *)s;
    while (is_space(*p))
        p++;
    const int sign = *p;
    if (*p == '-' || *p == '+')
        p++;
    unsigned total = 0;
    while (is_digit(*p))
        total = total * 10 + (unsigned)(*p++ - '0');
    return (long)(sign == '-' ? 0u - total : total);
}

// strtol 0x5d10b0 (strtoxl): base 0 detects 0x and 0; with base 16 a "0x" is skipped (no digits after it:
// nothing read, *end = s); overflow gives LONG_MAX/LONG_MIN and sets *range (the exe's errno = ERANGE)
inline long strtol(const char *s, const char **end, int base, bool *range = nullptr)
{
    const BYTE *p = (const BYTE *)s;
    int c = *p++;
    while (is_space(c))
        c = *p++;
    bool neg = false;
    if (c == '-') {
        neg = true;
        c = *p++;
    } else if (c == '+')
        c = *p++;
    if (base < 0 || base == 1 || base > 36) {
        if (end)
            *end = s;
        return 0;
    }
    if (base == 0)
        base = c != '0' ? 10 : (*p == 'x' || *p == 'X') ? 16 : 8;
    if (base == 16 && c == '0' && (*p == 'x' || *p == 'X')) {
        p++;
        c = *p++;
    }
    const unsigned maxval = 0xffffffffu / (unsigned)base, maxdig = 0xffffffffu % (unsigned)base;
    unsigned number = 0;
    bool digits = false, overflow = false;
    for (;;) {
        unsigned d;
        if (is_digit(c))
            d = (unsigned)(c - '0');
        else if (is_alpha(c))
            d = (unsigned)(to_upper(c) - 'A' + 10);
        else
            break;
        if (d >= (unsigned)base)
            break;
        digits = true;
        if (number < maxval || (number == maxval && d <= maxdig))
            number = number * (unsigned)base + d;
        else
            overflow = true;
        c = *p++;
    }
    --p;
    if (!digits) {
        p = (const BYTE *)s;
        number = 0;
    } else if (overflow || (neg && number > 0x80000000u) || (!neg && number > 0x7fffffffu)) {
        number = neg ? 0x80000000u : 0x7fffffffu;
        if (range)
            *range = true;
    }
    if (end)
        *end = (const char *)p;
    return (long)(neg ? 0u - number : number);
}

// ---- rounding ---------------------------------------------------------------------------------------------

// __ftol 0x5d0488 on a double: truncation toward zero to 64 bits (fistp with the chop mode); NaN and
// |x| >= 2^63 give the x87 integer indefinite 0x8000000000000000. The game uses the low 32 bits.
inline long long ftol(double x)
{
    unsigned long long u;
    memcpy(&u, &x, 8);
    const int e = (int)((u >> 52) & 0x7ff) - 1023;
    if (e < 0)
        return 0;
    if (e >= 63)
        return (long long)0x8000000000000000ull;
    const unsigned long long m = (u & 0xfffffffffffffull) | 0x10000000000000ull;
    const unsigned long long v = e >= 52 ? m << (e - 52) : m >> (52 - e);
    return (u >> 63) ? (long long)(0 - v) : (long long)v;
}

// ---- random numbers ---------------------------------------------------------------------------------------

// rand 0x5d2198 / srand 0x5d218b: per-thread holdrand (_getptd() + 0x14), start value 1
struct Rand {
    unsigned holdrand = 1;
    void seed(unsigned s) { holdrand = s; }
    int next() { return (int)(((holdrand = holdrand * 214013u + 2531011u) >> 16) & 0x7fff); }
};

// ---- qsort ------------------------------------------------------------------------------------------------

// qsort 0x5d097c: the middle element is swapped to the front and used as the pivot, ranges of up to 8
// elements go to shortsort (selection of the maximum from the back), the smaller part is sorted first
// through an explicit stack of 30. Not stable; the port must keep this exact order for equal keys.
using Compare = int(__cdecl *)(const void *, const void *);

inline void qsort_swap(char *a, char *b, unsigned width)
{
    if (a != b)
        while (width--) {
            const char t = *a;
            *a++ = *b;
            *b++ = t;
        }
}
inline void qsort_shortsort(char *lo, char *hi, unsigned width, Compare comp)
{
    while (hi > lo) {
        char *max = lo;
        for (char *p = lo + width; p <= hi; p += width)
            if (comp(p, max) > 0)
                max = p;
        qsort_swap(max, hi, width);
        hi -= width;
    }
}
inline void qsort(void *base, unsigned num, unsigned width, Compare comp)
{
    if (num < 2 || width == 0)
        return;
    char *lostk[30], *histk[30];
    int stkptr = 0;
    char *lo = (char *)base, *hi = (char *)base + width * (num - 1);
    for (;;) {
        const unsigned size = (unsigned)(hi - lo) / width + 1;
        if (size <= 8)
            qsort_shortsort(lo, hi, width, comp);
        else {
            char *mid = lo + (size / 2) * width;
            qsort_swap(mid, lo, width);
            char *loguy = lo, *higuy = hi + width;
            for (;;) {
                do
                    loguy += width;
                while (loguy <= hi && comp(loguy, lo) <= 0);
                do
                    higuy -= width;
                while (higuy > lo && comp(higuy, lo) >= 0);
                if (higuy < loguy)
                    break;
                qsort_swap(loguy, higuy, width);
            }
            qsort_swap(lo, higuy, width);
            if (higuy - 1 - lo >= hi - loguy) {
                if (lo + width < higuy) {
                    lostk[stkptr] = lo;
                    histk[stkptr] = higuy - width;
                    ++stkptr;
                }
                if (loguy < hi) {
                    lo = loguy;
                    continue;
                }
            } else {
                if (loguy < hi) {
                    lostk[stkptr] = loguy;
                    histk[stkptr] = hi;
                    ++stkptr;
                }
                if (lo + width < higuy) {
                    hi = higuy - width;
                    continue;
                }
            }
        }
        if (--stkptr < 0)
            return;
        lo = lostk[stkptr];
        hi = histk[stkptr];
    }
}

}  // namespace zax::crt
