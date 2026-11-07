// The C runtime's decimal conversions of floating point numbers (crt_float.h).
#include "crt_float.h"

namespace zax::crt {

namespace {

// The exe's power-of-ten tables (__pow10pos / __pow10neg): groups of 7 entries of 12 bytes, each group
// 0x54 bytes after the last, 10^(1..7 * 8^g); the first group starts 0x54 after the base
constexpr DWORD POW10_POS = 0x62c660, POW10_NEG = 0x62c7c0;
// The FpFormatDescriptors of _ld12tod and _ld12tof: max exp, min exp, precision, exponent width,
// format width, bias
constexpr DWORD FORMAT_DOUBLE = 0x62c538, FORMAT_FLOAT = 0x62c550;

size_t length(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}
void move(char *d, const char *s, size_t n)
{
    if (d < s)
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    else
        while (n--)
            d[n] = s[n];
}
void copy_text(char *d, const char *s)
{
    while ((*d++ = *s++))
        ;
}

// _shift 0x5d41c9: moves the rest of the string (with its terminator) n places to the right
void shift(char *s, int n)
{
    if (n)
        move(s + n, s, length(s) + 1);
}

}  // namespace

// ---- 96-bit multiplication and powers of ten ------------------------------------------------------------

void ld12mul(Ld12 *px, const Ld12 *py)
{
    Ld12 &x = *px;
    const Ld12 &y = *py;
    WORD ex = x.w(5), ey = y.w(5);
    const WORD sign = (WORD)((ex ^ ey) & 0x8000);
    ex &= 0x7fff;
    ey &= 0x7fff;
    WORD expsum = (WORD)(ex + ey);
    auto overflow = [&] {
        x.d(0) = 0;
        x.d(1) = 0;
        x.d(2) = sign ? 0xffff8000u : 0x7fff8000u;
    };
    if (ex >= 0x7fff || ey >= 0x7fff || expsum > 0xbffd) {
        overflow();
        return;
    }
    if (expsum <= 0x3fbf) {
        x.d(0) = x.d(1) = x.d(2) = 0;
        return;
    }
    if (ex == 0) {
        expsum++;
        if (!(x.d(2) & 0x7fffffff) && !x.d(1) && !x.d(0)) {
            x.w(5) = 0;
            return;
        }
    }
    if (ey == 0) {
        expsum++;
        if (!(y.d(2) & 0x7fffffff) && !y.d(1) && !y.d(0)) {
            x.d(0) = x.d(1) = x.d(2) = 0;
            return;
        }
    }
    // 80 x 80 bits in 16-bit words; the partial sums of row i start at word i, a carry goes into the next
    // word only (where it may wrap), as in the exe
    Ld12 prod{};
    for (int i = 0, count = 5; count > 0; i++, count--) {
        BYTE *cur = prod.b + 4 + 2 * i;
        for (int j = 0; j < count; j++) {
            const DWORD p = (DWORD)x.w(i + j) * y.w(4 - j);
            DWORD *sum = (DWORD *)(cur - 4);
            if (add_carry(*sum, p, sum))
                (*(WORD *)cur)++;
        }
    }
    expsum = (WORD)(expsum + 0xc002);
    bool sticky = false;
    if ((short)expsum > 0) {
        while (!(prod.b[11] & 0x80)) {
            shl_12(prod);
            expsum--;
            if ((short)expsum <= 0)
                break;
        }
    }
    if ((short)expsum <= 0) {
        expsum--;
        if ((short)expsum < 0) {
            int n = -(short)expsum;
            expsum = 0;
            for (; n; n--) {
                if (prod.b[0] & 1)
                    sticky = true;
                shr_12(prod);
            }
            if (sticky)
                prod.b[0] |= 1;
        }
    }
    if (prod.w(0) > 0x8000 || (prod.d(0) & 0x1ffff) == 0x18000) {
        if (prod.du(2) == 0xffffffff) {
            prod.du(2) = 0;
            if (prod.du(6) == 0xffffffff) {
                prod.du(6) = 0;
                if (prod.w(5) == 0xffff) {
                    expsum++;
                    prod.w(5) = 0x8000;
                } else
                    prod.w(5)++;
            } else
                prod.du(6)++;
        } else
            prod.du(2)++;
    }
    if (expsum >= 0x7fff) {
        overflow();
        return;
    }
    x.w(0) = prod.w(1);
    x.du(2) = prod.d(1);
    x.du(6) = prod.d(2);
    x.w(5) = (WORD)(expsum | sign);
}

void multtenpow12(Ld12 *x, int pow, int mult12)
{
    DWORD table = glob_addr(POW10_POS);
    if (!pow)
        return;
    if (pow < 0) {
        pow = -pow;
        table = glob_addr(POW10_NEG);
    }
    if (!mult12)
        x->w(0) = 0;
    while (pow) {
        table += 0x54;
        const int index = pow & 7;
        pow >>= 3;
        if (index) {
            const Ld12 *p = at<const Ld12 *>(table + 12 * (DWORD)index);
            Ld12 rounded;
            if (p->w(0) >= 0x8000) {   // an entry with its low word >= 0x8000 is used one unit lower
                rounded = *p;
                rounded.du(2)--;
                p = &rounded;
            }
            ld12mul(x, p);
        }
    }
}

void mtold12(const char *man, unsigned len, Ld12 *out)
{
    Ld12 &ld = *out;
    WORD exp = 0x404e;
    ld.d(0) = ld.d(1) = ld.d(2) = 0;
    for (; len; len--, man++) {
        const Ld12 copy = ld;
        shl_12(ld);
        shl_12(ld);
        add_12(ld, copy);
        shl_12(ld);
        Ld12 digit{};
        digit.d(0) = (DWORD)(int)(signed char)*man;
        add_12(ld, digit);
    }
    while (!ld.d(2)) {
        ld.d(2) = ld.d(1) >> 16;
        ld.d(1) = ld.d(1) << 16 | ld.d(0) >> 16;
        ld.d(0) <<= 16;
        exp = (WORD)(exp - 16);
    }
    while (!(ld.d(2) & 0x8000)) {
        shl_12(ld);
        exp--;
    }
    ld.w(5) = exp;
}

// ---- binary to decimal ----------------------------------------------------------------------------------

int i10_output(Ld10 ld, int ndigits, unsigned flags, Fos *fos)
{
    // 0.1 as a 96-bit number (the exe's constant on its stack: 0xcc x 10, exponent 0x3ffb)
    Ld12 tenth;
    for (int i = 0; i < 10; i++)
        tenth.b[i] = 0xcc;
    tenth.w(5) = 0x3ffb;
    const WORD sign = ld.exp & 0x8000, exp = ld.exp & 0x7fff;
    fos->sign = sign ? '-' : ' ';
    auto zero = [&] {
        fos->exp = 0;
        fos->sign = ' ';
        fos->man_len = 1;
        fos->man[0] = '0';
        fos->man[1] = 0;
        return 1;
    };
    if (!exp && !ld.hi && !ld.lo)
        return zero();
    if (exp == 0x7fff) {
        fos->exp = 1;
        const char *text;
        if ((ld.hi == 0x80000000 && !ld.lo) || (ld.hi & 0x40000000)) {
            if (sign && ld.hi == 0xc0000000) {
                text = !ld.lo ? "1#IND" : "1#QNAN";
            } else if (ld.hi == 0x80000000 && !ld.lo)
                text = "1#INF";
            else
                text = "1#QNAN";
        } else
            text = "1#SNAN";
        copy_text(fos->man, text);
        fos->man_len = (char)length(text);
        return 0;
    }
    // decimal exponent estimate from the binary exponent and the top byte of the mantissa (log10(2) as
    // 0x4d10 / 2^16), then the number scaled by 10^-estimate
    short e10 = (short)(((int)((exp >> 8) + (ld.hi >> 24) * 2) * 0x4d + (int)exp * 0x4d10 - 0x134312f4) >> 16);
    Ld12 t;
    t.w(0) = 0;
    t.du(2) = ld.lo;
    t.du(6) = ld.hi;
    t.w(5) = exp;
    multtenpow12(&t, -(int)e10, 1);
    if (t.w(5) >= 0x3fff) {
        e10++;
        ld12mul(&t, &tenth);
    }
    fos->exp = e10;
    int digits = ndigits;
    if (flags & 1) {
        digits = ndigits + e10;
        if (digits <= 0)
            return zero();
    }
    if (digits > 21)
        digits = 21;
    int shift = (int)t.w(5) - 0x3ffe;
    t.w(5) = 0;
    for (int i = 0; i < 8; i++)
        shl_12(t);
    if (shift < 0) {
        shift = -shift & 0xff;
        for (; shift > 0; shift--)
            shr_12(t);
    }
    char *p = fos->man;
    for (int n = digits + 1; n > 0; n--) {
        const Ld12 copy = t;
        shl_12(t);
        shl_12(t);
        add_12(t, copy);
        shl_12(t);
        *p++ = (char)(t.b[11] + '0');
        t.b[11] = 0;
    }
    char *q = p - 2;
    if (p[-1] >= '5') {
        while (q >= fos->man && *q == '9')
            *q-- = '0';
        if (q < fos->man) {
            q++;
            fos->exp++;
        }
        (*q)++;
    } else {
        while (q >= fos->man && *q == '0')
            q--;
        if (q < fos->man)
            return zero();
    }
    fos->man_len = (char)(q - fos->man + 1);
    fos->man[(int)fos->man_len] = 0;
    return 1;
}

void dtold(Ld10 *out, const double *x)
{
    DWORD lo, hi;
    memcpy(&lo, x, 4);
    memcpy(&hi, (const BYTE *)x + 4, 4);
    const WORD sign = (WORD)((hi >> 16) & 0x8000);
    const DWORD e = (hi >> 20) & 0x7ff;
    DWORD m = hi & 0xfffff, intbit = 0x80000000;
    WORD exp;
    if (!e) {
        if (!m && !lo) {
            out->lo = out->hi = 0;
            out->exp = 0;
            return;
        }
        exp = 0x3c01;
        intbit = 0;
    } else if (e == 0x7ff)
        exp = 0x7fff;
    else
        exp = (WORD)(e + 0x3c00);
    out->hi = lo >> 21 | m << 11 | intbit;
    out->lo = lo << 11;
    while (!(out->hi & 0x80000000)) {
        out->hi = out->hi << 1 | out->lo >> 31;
        out->lo <<= 1;
        exp--;
    }
    out->exp = (WORD)(sign | exp);
}

StrFlt *fltout2(double x, StrFlt *flt, char *digits)
{
    Ld10 ld;
    dtold(&ld, &x);
    Fos fos;
    flt->flag = i10_output(ld, 17, 0, &fos);
    flt->sign = fos.sign;
    flt->decpt = fos.exp;
    copy_text(digits, fos.man);
    flt->mantissa = digits;
    return flt;
}

void fptostr(char *buf, int digits, StrFlt *flt)
{
    const char *m = flt->mantissa;
    char *p = buf + 1;
    buf[0] = '0';
    for (int n = digits; n > 0; n--)
        *p++ = *m ? *m++ : '0';
    *p = 0;
    if (digits >= 0 && *m >= '5') {
        char *q = p - 1;
        while (*q == '9')
            *q-- = '0';
        (*q)++;
    }
    if (buf[0] == '1')
        flt->decpt++;
    else
        move(buf, buf + 1, length(buf + 1) + 1);
}

// ---- %e, %f, %g -----------------------------------------------------------------------------------------

char *cftoe2(char *buf, int ndec, int caps, StrFlt *flt, char g_fmt, char point)
{
    if (g_fmt)
        shift(buf + (flt->sign == '-'), ndec > 0);
    char *p = buf;
    if (flt->sign == '-')
        *p++ = '-';
    if (ndec > 0) {
        p[0] = p[1];
        *++p = point;
    }
    char *q = p + (g_fmt == 0) + ndec;
    copy_text(q, "e+000");
    if (caps)
        *q = 'E';
    q++;
    if (*flt->mantissa != '0') {
        int e = flt->decpt - 1;
        if (e < 0) {
            e = -e;
            *q = '-';
        }
        q++;
        if (e >= 100) {
            *q += (char)(e / 100);
            e %= 100;
        }
        q++;
        if (e >= 10) {
            *q += (char)(e / 10);
            e %= 10;
        }
        q[1] += (char)e;
    }
    return buf;
}

char *cftoe(const double *x, char *buf, int ndec, int caps, char point)
{
    StrFlt flt;
    char digits[24];
    fltout2(*x, &flt, digits);
    fptostr(buf + (flt.sign == '-') + (ndec > 0), ndec + 1, &flt);
    cftoe2(buf, ndec, caps, &flt, 0, point);
    return buf;
}

char *cftof2(char *buf, int ndec, StrFlt *flt, char g_fmt, char point)
{
    const int decpt = flt->decpt - 1;
    if (g_fmt && decpt == ndec) {
        char *p = buf + (flt->sign == '-') + decpt;
        p[0] = '0';
        p[1] = 0;
    }
    char *p = buf;
    if (flt->sign == '-')
        *p++ = '-';
    if (flt->decpt <= 0) {
        shift(p, 1);
        *p++ = '0';
    } else
        p += flt->decpt;
    if (ndec > 0) {
        shift(p, 1);
        *p++ = point;
        int n = flt->decpt;
        if (n < 0) {
            n = -n;
            if (g_fmt || ndec >= n)
                ndec = n;
            shift(p, ndec);
            memset(p, '0', (size_t)ndec);
        }
    }
    return buf;
}

char *cftof(const double *x, char *buf, int ndec, char point)
{
    StrFlt flt;
    char digits[24];
    fltout2(*x, &flt, digits);
    fptostr(buf + (flt.sign == '-'), flt.decpt + ndec, &flt);
    cftof2(buf, ndec, &flt, 0, point);
    return buf;
}

char *cftog(const double *x, char *buf, int ndec, int caps, char point)
{
    StrFlt flt;
    char digits[24];
    fltout2(*x, &flt, digits);
    const int before = flt.decpt - 1;
    char *p = buf + (flt.sign == '-');
    fptostr(p, ndec, &flt);
    const int after = flt.decpt - 1;
    const bool rounded_up = before < after;
    if (after < -4 || after >= ndec)
        cftoe2(buf, ndec, caps, &flt, 1, point);
    else {
        if (rounded_up) {
            while (*p++)
                ;
            p[-2] = 0;
        }
        cftof2(buf, ndec, &flt, 1, point);
    }
    return buf;
}

void cfltcvt(const double *x, char *buf, int format, int precision, int caps, char point)
{
    if (format == 'e' || format == 'E')
        cftoe(x, buf, precision, caps, point);
    else if (format == 'f')
        cftof(x, buf, precision, point);
    else
        cftog(x, buf, precision, caps, point);
}

void cropzeros(char *buf, char point)
{
    while (*buf && *buf != point)
        buf++;
    if (!*buf++)
        return;
    while (*buf && *buf != 'e' && *buf != 'E')
        buf++;
    char *stop = buf, *p = buf - 1;
    while (*p == '0')
        p--;
    if (*p == point)
        p--;
    while ((*++p = *stop++))
        ;
}

// isdigit is the exe's table lookup (only '0'-'9' in the C locale); the caller passes it in
void forcdecpt(char *buf, char point)
{
    char *p = buf;
    if (*p != 'e' && *p != 'E')
        do
            p++;
        while (*p >= '0' && *p <= '9');
    char held = *p;
    *p++ = point;
    for (;;) {   // up to and including the terminator
        const char next = *p, stored = held;
        *p++ = held;
        held = next;
        if (!stored)
            break;
    }
}

// ---- text to 96 bits ------------------------------------------------------------------------------------

unsigned strgtold12(Ld12 *out, const char **end, const char *s, int mult12, char point)
{
    // the exe's scale, decpt and implicit_E arguments are 0 at every call (_fltin, _atodbl, _atoflt)
    char man[25];
    char *manp = man;
    WORD sign = 0;
    int expsign = 1, manlen = 0, exp = 0, manexp = 0;
    bool found_digit = false, found_decpoint = false, found_exponent = false;
    const char *p = s, *saved = s;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    auto digit = [](int c) { return c >= '0' && c <= '9'; };
    int state = 0;
    for (;;) {
        int c = (BYTE)*p++;
        switch (state) {
        case 0:
            if (c >= '1' && c <= '9') {
                state = 3;
                p--;
            } else if (c == (BYTE)point)
                state = 5;
            else if (c == '+') {
                sign = 0;
                state = 2;
            } else if (c == '-') {
                sign = 0x8000;
                state = 2;
            } else if (c == '0')
                state = 1;
            else {
                p--;
                goto done;
            }
            continue;
        case 1:
            found_digit = true;
            if (c >= '1' && c <= '9') {
                state = 3;
                p--;
            } else if (c == (BYTE)point)
                state = 4;
            else if (c == '+' || c == '-') {
                p--;
                state = 11;
            } else if (c == '0')
                state = 1;
            else if (c == 'D' || c == 'E' || c == 'd' || c == 'e')
                state = 6;
            else {
                p--;
                goto done;
            }
            continue;
        case 2:
            if (c >= '1' && c <= '9') {
                state = 3;
                p--;
            } else if (c == (BYTE)point)
                state = 5;
            else if (c == '0')
                state = 1;
            else {
                p = saved;
                goto done;
            }
            continue;
        case 3:
            found_digit = true;
            while (digit(c)) {
                if (manlen < 25) {
                    manlen++;
                    *manp++ = (char)(c - '0');
                } else
                    manexp++;
                c = (BYTE)*p++;
            }
            if (c == (BYTE)point) {
                state = 4;
                continue;
            }
            goto after_digits;
        case 4:
            found_digit = true;
            found_decpoint = true;
            if (!manlen)
                while (c == '0') {
                    manexp--;
                    c = (BYTE)*p++;
                }
            while (digit(c)) {
                if (manlen < 25) {
                    manlen++;
                    *manp++ = (char)(c - '0');
                    manexp--;
                }
                c = (BYTE)*p++;
            }
        after_digits:
            if (c == '+' || c == '-') {
                p--;
                state = 11;
            } else if (c == 'D' || c == 'E' || c == 'd' || c == 'e')
                state = 6;
            else {
                p--;
                goto done;
            }
            continue;
        case 5:
            found_decpoint = true;
            if (digit(c)) {
                state = 4;
                p--;
                continue;
            }
            p = saved;
            goto done;
        case 6:
            saved = p - 2;
            if (c >= '1' && c <= '9') {
                state = 9;
                p--;
            } else if (c == '+')
                state = 7;
            else if (c == '-') {
                expsign = -1;
                state = 7;
            } else if (c == '0')
                state = 8;
            else {
                p = saved;
                goto done;
            }
            continue;
        case 7:
            if (c >= '1' && c <= '9') {
                state = 9;
                p--;
            } else if (c == '0')
                state = 8;
            else {
                p = saved;
                goto done;
            }
            continue;
        case 8:
            found_exponent = true;
            while (c == '0')
                c = (BYTE)*p++;
            if (c >= '1' && c <= '9') {
                state = 9;
                p--;
                continue;
            }
            p--;
            goto done;
        case 9: {
            found_exponent = true;
            int e = 0;
            while (digit(c)) {
                e = e * 10 + c - '0';
                if (e > 0x1450) {
                    e = 0x1451;
                    break;
                }
                c = (BYTE)*p++;
            }
            exp = e;
            while (digit(c))
                c = (BYTE)*p++;
            p--;
            goto done;
        }
        case 11:
            // implicit_E is 0: the sign is not part of the number
            p--;
            goto done;
        }
    }
done:
    *end = p;
    WORD w0, w5;
    DWORD d2, d6;
    unsigned flags = 0;
    if (!found_digit) {
        w0 = 0;
        d2 = d6 = 0;
        w5 = 0;
        flags = 4;
    } else {
        if (manlen > 24) {
            if (man[23] >= 5)
                man[23]++;
            manlen = 24;
            manp--;
            manexp++;
        }
        if (manlen > 0) {
            char *q = manp;
            while (!*--q) {
                manlen--;
                manexp++;
            }
            Ld12 t;
            mtold12(man, (unsigned)manlen, &t);
            int e = expsign < 0 ? -exp : exp;
            e += manexp;
            (void)found_exponent;
            (void)found_decpoint;
            if (e > 0x1450) {
                w0 = 0;
                d2 = 0;
                d6 = 0x80000000;
                w5 = 0x7fff;
                flags = 2;
            } else if (e < -0x1450) {
                w0 = 0;
                d2 = d6 = 0;
                w5 = 0;
                flags = 1;
            } else {
                multtenpow12(&t, e, mult12);
                w0 = t.w(0);
                d2 = t.du(2);
                d6 = t.du(6);
                w5 = t.w(5);
            }
        } else {
            w0 = 0;
            d2 = d6 = 0;
            w5 = 0;
        }
    }
    out->du(6) = d6;
    out->du(2) = d2;
    out->w(5) = (WORD)(w5 | sign);
    out->w(0) = w0;
    return flags;
}

// ---- 96 bits to double and float (_ld12cvt and its helpers 0x5db6e7-0x5db854) ---------------------------

namespace {

struct FpFormat {
    int max_exp, min_exp, precision, exp_width, format_width, bias;
};

// _ZeroTail: the bits after bit `nbit` (counted from the top of man[0]) are all zero; the bit itself is
// not looked at
bool zero_tail(const DWORD *man, int nbit)
{
    int nl = nbit / 32;
    const int nb = nbit % 32;
    const DWORD mask = ~(0xffffffffu << (31 - nb));
    if (man[nl] & mask)
        return false;
    for (nl++; nl < 3; nl++)
        if (man[nl])
            return false;
    return true;
}

// _IncMan: adds one at bit `nbit`; the carry out of man[0]
int inc_man(DWORD *man, int nbit)
{
    int nl = nbit / 32;
    const int nb = nbit % 32;
    int carry = add_carry(man[nl], 1u << (31 - nb), &man[nl]);
    for (nl--; nl >= 0 && carry; nl--)
        carry = add_carry(man[nl], 1, &man[nl]);
    return carry;
}

// _RoundMan: rounds at `precision` bits; up only when the next bit is set and some bit after that one
// (but not the one right after) is set, so ties and near-ties round down; the carry out of the top
int round_man(DWORD *man, int precision)
{
    int retval = 0;
    const int nl = precision / 32, nb = 31 - precision % 32;
    if (man[nl] & (1u << nb) && !zero_tail(man, precision + 1))
        retval = inc_man(man, precision - 1);
    man[nl] &= 0xffffffffu << nb;
    for (int i = nl + 1; i < 3; i++)
        man[i] = 0;
    return retval;
}

// _ShrMan
void shr_man(DWORD *man, int n)
{
    const int nl = n / 32, nb = n % 32;
    const DWORD mask = ~(0xffffffffu << nb);
    DWORD carry = 0;
    for (int i = 0; i < 3; i++) {
        const DWORD low = man[i] & mask;
        man[i] = man[i] >> nb | carry;
        carry = nb ? low << (32 - nb) : 0;
    }
    for (int i = 2; i >= 0; i--)
        man[i] = i >= nl ? man[i - nl] : 0;
}

int ld12cvt(const Ld12 *in, void *out, const FpFormat *f)
{
    const DWORD sign = in->w(5) & 0x8000;
    DWORD man[3] = {*(const DWORD *)(in->b + 6), *(const DWORD *)(in->b + 2), (DWORD)in->w(0) << 16};
    int exp = (int)(in->w(5) & 0x7fff) - 0x3fff;
    int retval = 0;
    DWORD biased = 0;
    if (exp == -0x3fff) {
        if (man[0] || man[1] || man[2]) {
            man[0] = man[1] = man[2] = 0;
            retval = 2;
        }
    } else {
        DWORD saved[3] = {man[0], man[1], man[2]};
        if (round_man(man, f->precision))
            exp++;
        if (exp < f->min_exp - f->precision) {
            man[0] = man[1] = man[2] = 0;
            retval = 2;
        } else if (exp <= f->min_exp) {
            for (int i = 0; i < 3; i++)
                man[i] = saved[i];
            shr_man(man, f->min_exp - exp);
            round_man(man, f->precision);
            shr_man(man, f->exp_width + 1);
            retval = 2;
        } else if (exp >= f->max_exp) {
            man[0] = 0x80000000;
            man[1] = man[2] = 0;
            shr_man(man, f->exp_width);
            biased = (DWORD)(f->bias + f->max_exp);
            retval = 1;
        } else {
            man[0] &= 0x7fffffff;
            biased = (DWORD)(f->bias + exp);
            shr_man(man, f->exp_width);
        }
    }
    const DWORD top = biased << (31 - f->exp_width) | (sign ? 0x80000000 : 0) | man[0];
    if (f->format_width == 64) {
        ((DWORD *)out)[1] = top;
        ((DWORD *)out)[0] = man[1];
    } else if (f->format_width == 32)
        ((DWORD *)out)[0] = top;
    return retval;
}

}  // namespace

// The helpers with the exe's signatures (_ZeroTail 0x5db6e7 ... _ld12cvt 0x5db8e1)
int __cdecl zero_tail_(const DWORD *man, int nbit) { return zero_tail(man, nbit); }
int __cdecl inc_man_(DWORD *man, int nbit) { return inc_man(man, nbit); }
int __cdecl round_man_(DWORD *man, int precision) { return round_man(man, precision); }
void __cdecl copy_man_(DWORD *dst, const DWORD *src)
{
    for (int i = 0; i < 3; i++)
        dst[i] = src[i];
}
void __cdecl fill_zero_man_(DWORD *man) { man[0] = man[1] = man[2] = 0; }
int __cdecl is_zero_man_(const DWORD *man) { return !man[0] && !man[1] && !man[2]; }
void __cdecl shr_man_(DWORD *man, int n) { shr_man(man, n); }
int __cdecl ld12cvt_(const Ld12 *in, void *out, const FpFormat *f) { return ld12cvt(in, out, f); }

int ld12tod(const Ld12 *in, double *out) { return ld12cvt(in, out, glob<const FpFormat *>(FORMAT_DOUBLE)); }
int ld12tof(const Ld12 *in, float *out) { return ld12cvt(in, out, glob<const FpFormat *>(FORMAT_FLOAT)); }

int atodbl(double *out, const char *s, char point)
{
    Ld12 ld;
    const char *end;
    strgtold12(&ld, &end, s, 0, point);
    return ld12tod(&ld, out);
}

int atoflt(float *out, const char *s, char point)
{
    Ld12 ld;
    const char *end;
    strgtold12(&ld, &end, s, 0, point);
    return ld12tof(&ld, out);
}

}  // namespace zax::crt

namespace zax {

using namespace crt;

ZAX_HOOKS(crt_float)
{
    const Hook list[] = {
        {0x5db6e7, (void *)&zero_tail_, "_ZeroTail", 5, {0x8b, 0x44, 0x24, 0x08, 0x56, 0x6a, 0x20, 0x99}},
        {0x5db730, (void *)&inc_man_, "_IncMan", 5, {0x8b, 0x44, 0x24, 0x08, 0x53, 0x56, 0x57, 0x6a}},
        {0x5db786, (void *)&round_man_, "_RoundMan", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0x8b, 0x45, 0x0c}},
        {0x5db812, (void *)&copy_man_, "_CopyMan", 8, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x4c, 0x24, 0x04}},
        {0x5db82d, (void *)&fill_zero_man_, "_FillZeroMan", 5, {0x57, 0x8b, 0x7c, 0x24, 0x08, 0x33, 0xc0, 0xab}},
        {0x5db839, (void *)&is_zero_man_, "_IsZeroMan", 6, {0x8b, 0x44, 0x24, 0x04, 0x33, 0xc9, 0x83, 0x38}},
        {0x5db854, (void *)&shr_man_, "_ShrMan", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x8b, 0x45}},
        {0x5db8e1, (void *)&ld12cvt_, "_ld12cvt", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x18, 0x8b, 0x45}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
