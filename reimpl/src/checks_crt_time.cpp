// Checks of the time and environment functions of the C runtime (crt_time.cpp, phase 5b,
// docs/DECOMPILATION.md 11) against the exe's functions through the trampolines, in the game thread. The
// hooks stay in: an original that calls another of these functions calls its reimplementation, which has
// its own cases. The runtime's state these functions change (the time zone variables, the transition
// dates, the cached system time, the environment table) is saved before a case and put back between the
// two sides, so both start from the same state, and compared after each side.
//  - gmtime, localtime (also near both ends of the range, where it converts field by field), asctime,
//    ctime, __loctotime_t (years 1960-2045, all three DST flags), _isindst under the system's zone, a
//    TZ zone (the fixed US rules) and made-up Windows zones (day-of-week and absolute transition dates),
//    cvtdate;
//  - _tzset_lk under TZ strings ("PST8PDT", "CET-1:00", seconds, no DST, the same TZ again) and without
//    TZ, __tzset once;
//  - time: the original before and after the reimplementation (the reimplementation's value one of the
//    two), with the cached minute kept and invalidated;
//  - strftime and _Strftime on formats of all specifiers with and without '#' and every buffer size,
//    _expandtime, _store_winword (the locale's date and time pictures and random ones), _store_num,
//    _store_number, _store_str, store_dt;
//  - getenv, findenv, __mbsnbicoll, _mbschr, strncnt, copy_environ, and __crtsetenv on sequences of
//    adding, changing and removing test variables on a copy of _environ (each side its own names; the
//    return values, the values getenv and GetEnvironmentVariableA see, the table's size); __wtomb_environ
//    on a private wide table (the game has none).
#include "check_util.h"
#include "crt_common.h"

WINIMPORT DWORD WINAPI GetEnvironmentVariableA(const char *name, char *buf, DWORD size);
WINIMPORT char *WINAPI lstrcpynA(char *out, const char *s, int n);

namespace zax {
namespace {

template <typename F> F orig(DWORD a) { return original<F>(a); }
bool g_time_trace = false;   // log the steps of the environment cases (to find one that crashes)
template <typename F> F mine(DWORD a) { return at<F>(a); }

struct Tm {
    int sec, min, hour, mday, mon, year, wday, yday, isdst;
};

constexpr DWORD TIMEZONE = 0x62c120, DAYLIGHT = 0x62c124, DSTBIAS = 0x62c128, TZNAME = 0x62c1ac;
constexpr DWORD DSTSTART = 0x62c1b8, TZINFO = 0x7159d0, TZAPIUSED = 0x7159c8;
constexpr DWORD TZ_FIRST_TIME = 0x715a80, LAST_TZ = 0x715a7c, GMT_CACHE = 0x7158c8, DST_CACHE = 0x7158c0;
constexpr DWORD ALTERNATE_FORM = 0x717548, NO_LEAD_ZEROS = 0x71754c, LC_TIME_CURR = 0x629820;
constexpr DWORD ENVIRON = 0x71589c, WENVIRON = 0x7158a4;

inline int &g(DWORD a) { return *at<int *>(a); }
inline bool eq(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}
inline void exe_free(void *p) { at<void(__cdecl *)(void *)>(0x5d33e0)(p); }
inline char *exe_strdup(const char *s) { return at<char *(__cdecl *)(const char *)>(0x5d3606)(s); }

// The time zone state: the variables, both _tzname buffers, the transition dates, the zone of
// GetTimeZoneInformation, the flags and the cached system time of time()
struct ZoneState {
    int timezone, daylight, dstbias, apiused, first_time, dst_cache;
    char name0[64], name1[64];
    BYTE dst[0x20];      // dststart, dstend (0x62c1b8-0x62c1d4)
    BYTE info[0xac];
    BYTE gmt[16];
    char *last;
    void save()
    {
        timezone = g(TIMEZONE);
        daylight = g(DAYLIGHT);
        dstbias = g(DSTBIAS);
        apiused = g(TZAPIUSED);
        first_time = g(TZ_FIRST_TIME);
        dst_cache = g(DST_CACHE);
        memcpy(name0, at<char **>(TZNAME)[0], 64);
        memcpy(name1, at<char **>(TZNAME)[1], 64);
        memcpy(dst, at<BYTE *>(DSTSTART), sizeof dst);
        memcpy(info, at<BYTE *>(TZINFO), sizeof info);
        memcpy(gmt, at<BYTE *>(GMT_CACHE), sizeof gmt);
        last = *at<char **>(LAST_TZ);
    }
    void load() const
    {
        g(TIMEZONE) = timezone;
        g(DAYLIGHT) = daylight;
        g(DSTBIAS) = dstbias;
        g(TZAPIUSED) = apiused;
        g(TZ_FIRST_TIME) = first_time;
        g(DST_CACHE) = dst_cache;
        memcpy(at<char **>(TZNAME)[0], name0, 64);
        memcpy(at<char **>(TZNAME)[1], name1, 64);
        memcpy(at<BYTE *>(DSTSTART), dst, sizeof dst);
        memcpy(at<BYTE *>(TZINFO), info, sizeof info);
        memcpy(at<BYTE *>(GMT_CACHE), gmt, sizeof gmt);
        *at<char **>(LAST_TZ) = last;
    }
    // the same state (the last TZ by its text, the buffers by their strings)
    bool same(const ZoneState &o) const
    {
        const bool lasts = (!last && !o.last) || (last && o.last && eq(last, o.last));
        return timezone == o.timezone && daylight == o.daylight && dstbias == o.dstbias && apiused == o.apiused &&
               first_time == o.first_time && dst_cache == o.dst_cache && eq(name0, o.name0) &&
               eq(name1, o.name1) && !memcmp(dst, o.dst, sizeof dst) && !memcmp(info, o.info, sizeof info) &&
               !memcmp(gmt, o.gmt, sizeof gmt) && lasts;
    }
};

Tm random_tm()
{
    Tm t;
    t.sec = (int)(random() % 62);
    t.min = (int)(random() % 60);
    t.hour = (int)(random() % 24);
    t.mday = 1 + (int)(random() % 31);
    t.mon = (int)(random() % 12);
    t.year = random() % 4 ? 70 + (int)(random() % 69) : (int)(random() % 200);
    t.wday = (int)(random() % 7);
    t.yday = (int)(random() % 366);
    t.isdst = (int)(random() % 2);
    return t;
}

long random_time()
{
    switch (random() % 6) {
    case 0: return (long)(random() % (4 * 86400));                       // near the start
    case 1: return 0x7fffffff - (long)(random() % (4 * 86400));          // near the end
    case 2: return -(long)(random() % 100000) - 1;                       // negative: no result
    default: return (long)(random() & 0x7fffffff);
    }
}

void describe_tm(char *d, const char *what, long t, const Tm *a, const Tm *b)
{
    wsprintfA(d, "[%s %ld: %d %d %d %d %d %d %d %d %d / %d %d %d %d %d %d %d %d %d]", what, t,
              a ? a->sec : -1, a ? a->min : -1, a ? a->hour : -1, a ? a->mday : -1, a ? a->mon : -1, a ? a->year : -1,
              a ? a->wday : -1, a ? a->yday : -1, a ? a->isdst : -1, b ? b->sec : -1, b ? b->min : -1, b ? b->hour : -1,
              b ? b->mday : -1, b ? b->mon : -1, b ? b->year : -1, b ? b->wday : -1, b ? b->yday : -1, b ? b->isdst : -1);
}

using TmFn = Tm *(__cdecl *)(const long *);
using IsdstFn = int(__cdecl *)(Tm *);
using LocFn = long(__cdecl *)(int, int, int, int, int, int, int);
using CvtFn = void(__cdecl *)(int, int, int, int, int, int, int, int, int, int, int);
using TzFn = void(__cdecl *)();

bool same_tm(const Tm *a, const Tm *b) { return (!a && !b) || (a && b && !memcmp(a, b, sizeof(Tm))); }

// One call on both sides from the same zone state: the result and the state after it
template <typename Call> bool both_sides(const ZoneState &start, Call call, ZoneState &after0, ZoneState &after1)
{
    start.load();
    call(0);
    after0.save();
    start.load();
    call(1);
    after1.save();
    return after0.same(after1);
}

// gmtime, localtime, asctime, ctime, __loctotime_t, _isindst, cvtdate under the zone state as it is
void check_conversions(Tally &t, const ZoneState &zone, int count)
{
    char d[600];
    ZoneState a0, a1;
    for (int k = 0; k < count; k++) {
        const long tm_in = random_time();
        Tm ra{}, rb{};
        bool na = false, nb = false;
        // gmtime (no state)
        {
            const Tm *pa = orig<TmFn>(0x5d8eb8)(&tm_in);
            if (pa) ra = *pa; else na = true;
            const Tm *pb = mine<TmFn>(0x5d8eb8)(&tm_in);
            if (pb) rb = *pb; else nb = true;
            describe_tm(d, "gmtime", tm_in, na ? nullptr : &ra, nb ? nullptr : &rb);
            t.check(same_tm(na ? nullptr : &ra, nb ? nullptr : &rb), "", d);
        }
        // localtime (reads the zone, may set the transition dates)
        {
            const bool st = both_sides(zone, [&](int side) {
                const Tm *p = (side ? mine<TmFn>(0x5d312f) : orig<TmFn>(0x5d312f))(&tm_in);
                (side ? nb : na) = !p;
                if (p) (side ? rb : ra) = *p;
            }, a0, a1);
            describe_tm(d, "localtime", tm_in, na ? nullptr : &ra, nb ? nullptr : &rb);
            t.check(st && same_tm(na ? nullptr : &ra, nb ? nullptr : &rb), "", d);
        }
        // asctime of a calendar time, ctime of a time
        {
            Tm tb = random_tm();
            char sa[32] = {}, sb[32] = {};
            const char *pa = orig<char *(__cdecl *)(const Tm *)>(0x5da9eb)(&tb);
            lstrcpynA(sa, pa, 32);
            const char *pb = mine<char *(__cdecl *)(const Tm *)>(0x5da9eb)(&tb);
            lstrcpynA(sb, pb, 32);
            wsprintfA(d, "[asctime %d.%d.%d %d:%d:%d wday %d: \"%s\" / \"%s\"]", tb.mday, tb.mon, tb.year, tb.hour, tb.min, tb.sec, tb.wday, sa, sb);
            t.check(pa == pb && eq(sa, sb), "", d);
            bool ca = false, cb = false;
            const bool st = both_sides(zone, [&](int side) {
                const char *p = (side ? mine<char *(__cdecl *)(const long *)>(0x5d2864) : orig<char *(__cdecl *)(const long *)>(0x5d2864))(&tm_in);
                (side ? cb : ca) = !p;
                lstrcpynA(side ? sb : sa, p ? p : "", 32);
            }, a0, a1);
            wsprintfA(d, "[ctime %ld: \"%s\" / \"%s\"]", tm_in, sa, sb);
            t.check(st && ca == cb && eq(sa, sb), "", d);
        }
        // __loctotime_t
        {
            const int yr = 1960 + (int)(random() % 86), mo = 1 + (int)(random() % 12), dy = 1 + (int)(random() % 31);
            const int hr = (int)(random() % 24), mn = (int)(random() % 60), sc = (int)(random() % 60);
            const int flag = (int)(random() % 3) - 1;
            long la = 0, lb = 0;
            const bool st = both_sides(zone, [&](int side) {
                (side ? lb : la) = (side ? mine<LocFn>(0x5d9584) : orig<LocFn>(0x5d9584))(yr, mo, dy, hr, mn, sc, flag);
            }, a0, a1);
            // with DST flag -1 the original asks _isindst with a tm of only year, month, hour and day of the year:
            // minute and second are whatever was on its stack (any value). On the day of a transition the
            // answer depends on them; then both answers count (the two results one DST bias apart).
            bool undefined = false;
            if (flag == -1 && la != lb && (la - lb == g(DSTBIAS) || lb - la == g(DSTBIAS))) {
                Tm probe{};
                probe.year = yr - 1900;
                probe.mon = mo - 1;
                probe.hour = hr;
                probe.yday = dy + at<const int *>(0x62c208)[mo - 1] + (!((yr - 1900) & 3) && mo > 2);
                zone.load();
                orig<IsdstFn>(0x5d9277)(&probe);   // the transition dates of that year
                const int *start = at<const int *>(DSTSTART), *end = start + 4;
                undefined = start[0] == probe.year && (probe.yday == start[1] || probe.yday == end[1]);
                zone.load();
            }
            wsprintfA(d, "[__loctotime_t %d-%d-%d %d:%d:%d dst %d: %ld / %ld%s]", yr, mo, dy, hr, mn, sc, flag, la, lb,
                      undefined ? " (a transition day: minute and second undefined in the original)" : "");
            if (undefined)
                log("  %s", d);
            t.check(st && (la == lb || undefined), "", d);
        }
        // _isindst, half of the cases with the transition dates' year forgotten (computed again)
        {
            Tm tb = random_tm();
            tb.year = 67 + (int)(random() % 75);
            const bool forget = random() % 2;
            int ia = 0, ib = 0;
            ZoneState from = zone;
            if (forget)
                memset(from.dst, 0xff, 4), memset(from.dst + 0x10, 0xff, 4);
            Tm ta = tb, tbb = tb;
            const bool st = both_sides(from, [&](int side) {
                (side ? ib : ia) = (side ? mine<IsdstFn>(0x5d9277) : orig<IsdstFn>(0x5d9277))(side ? &tbb : &ta);
            }, a0, a1);
            wsprintfA(d, "[_isindst year %d yday %d hour %d mon %d forget %d: %d / %d]", tb.year, tb.yday, tb.hour, tb.mon, forget, ia, ib);
            t.check(st && ia == ib && !memcmp(&ta, &tbb, sizeof ta), "", d);
        }
        // cvtdate
        {
            const int tran = (int)(random() % 2), type = (int)(random() % 2), year = 67 + (int)(random() % 75);
            const int month = 1 + (int)(random() % 12), week = 1 + (int)(random() % 5), dow = (int)(random() % 7);
            const int date = 1 + (int)(random() % 28), hour = (int)(random() % 24), min = (int)(random() % 60);
            const int sec = (int)(random() % 60), ms = (int)(random() % 1000);
            const bool st = both_sides(zone, [&](int side) {
                (side ? mine<CvtFn>(0x5d9444) : orig<CvtFn>(0x5d9444))(tran, type, year, month, week, dow, date, hour, min, sec, ms);
            }, a0, a1);
            const int *x = (const int *)a0.dst, *y = (const int *)a1.dst;
            wsprintfA(d, "[cvtdate %d %d %d %d %d %d %d %d:%d:%d.%d: %d %d %d %d %d %d / %d %d %d %d %d %d]", tran, type, year, month,
                      week, dow, date, hour, min, sec, ms, x[0], x[1], x[2], x[4], x[5], x[6], y[0], y[1], y[2], y[4], y[5], y[6]);
            t.check(st, "", d);
        }
    }
    zone.load();
}

// A made-up Windows zone: bias, standard and daylight dates in the day-of-week form (year 0) or absolute
void make_zone(ZoneState &z, int k)
{
    WORD *w = reinterpret_cast<WORD *>(z.info);
    *reinterpret_cast<int *>(z.info) = (int)(random() % 1500) - 720;
    WORD *std_date = w + 2 + 32, *day_date = w + 2 + 32 + 8 + 2 + 32;
    WORD *const dates[] = {std_date, day_date};
    for (WORD *dt : dates) {
        dt[0] = k % 2 ? (WORD)(1990 + random() % 40) : 0;
        dt[1] = (WORD)(k % 5 == 0 ? 0 : 1 + random() % 12);
        dt[2] = (WORD)(random() % 7);
        dt[3] = (WORD)(k % 2 ? 1 + random() % 28 : 1 + random() % 5);
        dt[4] = (WORD)(random() % 24);
        dt[5] = (WORD)(random() % 60);
        dt[6] = (WORD)(random() % 60);
        dt[7] = (WORD)(random() % 1000);
    }
    *reinterpret_cast<int *>(z.info + 0x54) = 0;
    *reinterpret_cast<int *>(z.info + 0xa8) = k % 3 ? -60 : 0;
    z.apiused = 1;
    z.daylight = 1;
    z.dstbias = -3600;
    memset(z.dst, 0xff, 4);
    memset(z.dst + 0x10, 0xff, 4);
}

// _tzset_lk with TZ strings (put into the environment through __crtsetenv) and without TZ; __tzset
void check_tzset(Tally &t, const ZoneState &zone)
{
    static const char *const zones[] = {"PST8PDT", "CET-1:00", "EST5", "GMT0BST", "ABC+3:30:15XYZ", "IST-5:30",
                                        "X", "NZST-12NZDT", "UTC0", "AAA-12:59:59BBB", "Q+99"};
    using SetEnv = int(__cdecl *)(char *, int);
    char d[600];
    ZoneState start = zone, a0, a1;
    start.last = nullptr;   // each side allocates its own copy of TZ
    for (unsigned k = 0; k <= sizeof zones / sizeof zones[0]; k++) {
        const bool with_tz = k < sizeof zones / sizeof zones[0];
        char text[40];
        wsprintfA(text, "TZ=%s", with_tz ? zones[k] : "");
        mine<SetEnv>(0x5ddffc)(exe_strdup(text), 0);
        char *lasts[2] = {};
        const bool st = both_sides(start, [&](int side) {
            const TzFn f = side ? mine<TzFn>(0x5d8ff0) : orig<TzFn>(0x5d8ff0);
            f();
            if (with_tz)
                f();   // the same TZ again: nothing changes
            lasts[side] = *at<char **>(LAST_TZ);
        }, a0, a1);
        wsprintfA(d, "[_tzset_lk %s: tz %d dl %d bias %d \"%s\" \"%s\" / tz %d dl %d bias %d \"%s\" \"%s\"]", text, a0.timezone,
                  a0.daylight, a0.dstbias, a0.name0, a0.name1, a1.timezone, a1.daylight, a1.dstbias, a1.name0, a1.name1);
        t.check(st, "", d);
        for (char *p : lasts)
            exe_free(p);
    }
    char remove[] = "TZ=";
    mine<SetEnv>(0x5ddffc)(exe_strdup(remove), 0);
    // __tzset: once (the flag cleared), then not again
    ZoneState first = zone;
    first.first_time = 0;
    first.last = nullptr;
    const bool st = both_sides(first, [&](int side) {
        (side ? mine<TzFn>(0x5d8fc2) : orig<TzFn>(0x5d8fc2))();
        (side ? mine<TzFn>(0x5d8fc2) : orig<TzFn>(0x5d8fc2))();
    }, a0, a1);
    t.check(st, "", "[__tzset]");
    zone.load();
}

// time: the reimplementation's value between two originals
void check_time(Tally &t, const ZoneState &zone, int count)
{
    using TimeFn = long(__cdecl *)(long *);
    char d[600];
    for (int k = 0; k < count; k++) {
        zone.load();
        if (k % 2)
            memset(at<BYTE *>(GMT_CACHE), 0xff, 16);   // the cached minute invalidated
        long pa = 0, pb = 0, pc = 0;
        const long a = orig<TimeFn>(0x5d20af)(k % 3 ? &pa : nullptr);
        if (k % 2)
            memset(at<BYTE *>(GMT_CACHE), 0xff, 16);
        const long b = mine<TimeFn>(0x5d20af)(k % 3 ? &pb : nullptr);
        const int cache_b = g(DST_CACHE);
        if (k % 2)
            memset(at<BYTE *>(GMT_CACHE), 0xff, 16);
        const long c = orig<TimeFn>(0x5d20af)(k % 3 ? &pc : nullptr);
        const int cache_c = g(DST_CACHE);
        wsprintfA(d, "[time %d: %ld %ld %ld, dst %d %d]", k, a, b, c, cache_b, cache_c);
        t.check((b == a || b == c) && (k % 3 == 0 || pb == b) && cache_b == cache_c, "", d);
    }
    zone.load();
}

// ---- strftime and its pieces ---------------------------------------------------------------------------------

// An output buffer with guard bytes; the pieces write at buf + 4 (_store_number may touch the byte before)
struct OutBuf {
    char b[160];
    char *out;
    size_t count;
    void reset(size_t n)
    {
        memset(b, 0xcc, sizeof b);
        out = b + 4;
        count = n;
    }
    bool same(const OutBuf &o) const { return !memcmp(b, o.b, sizeof b) && out - b == o.out - o.b && count == o.count; }
};

const BYTE *lc_time() { return *at<const BYTE **>(LC_TIME_CURR); }

void describe_out(char *d, const char *what, const OutBuf &a, const OutBuf &b)
{
    char ta[48], tb[48];
    lstrcpynA(ta, a.b + 4, 40);
    lstrcpynA(tb, b.b + 4, 40);
    char *const texts[] = {ta, tb};
    for (char *p : texts)
        for (; *p; p++)
            if ((BYTE)*p == 0xcc)
                *p = '~';
    wsprintfA(d, "[%s: \"%s\" +%d left %d / \"%s\" +%d left %d]", what, ta, (int)(a.out - a.b), (int)a.count, tb,
              (int)(b.out - b.b), (int)b.count);
}

void check_store(Tally &t, int count)
{
    using Str = void(__cdecl *)(const char *, char **, size_t *);
    using Num = void(__cdecl *)(int, unsigned, char **, size_t *);
    using Number = void(__cdecl *)(int, char **, size_t *);
    using Dt = char *(__cdecl *)(char *, int);
    using Expand = void(__cdecl *)(char, const Tm *, char **, size_t *, const BYTE *);
    using Word = void(__cdecl *)(const char *, const Tm *, char **, size_t *, const BYTE *);
    static const char specifiers[] = "aAbBcdHIjmMpSUwWxXyYzZ%q";
    static const char picture_letters[] = "dddMMMyyyyhhHHmmsstt'g, :/";
    char d[600], what[120];
    OutBuf a, b;
    const int saved_alt = g(ALTERNATE_FORM), saved_nolead = g(NO_LEAD_ZEROS);
    for (int k = 0; k < count; k++) {
        const size_t n = random() % 12;
        // _store_str
        {
            static const char *const texts[] = {"", "x", "Sunday", "a rather longer text"};
            const char *s = texts[k % 4];
            a.reset(n);
            orig<Str>(0x5d2d81)(s, &a.out, &a.count);
            b.reset(n);
            mine<Str>(0x5d2d81)(s, &b.out, &b.count);
            wsprintfA(what, "_store_str \"%s\" %d", s, (int)n);
            describe_out(d, what, a, b);
            t.check(a.same(b), "", d);
        }
        // _store_num with and without leading zeros, _store_number
        {
            const int num = k % 5 ? (int)(random() % 100000) : (int)random();
            const unsigned digits = 1 + random() % 4;
            const int nolead = (int)(random() % 2);
            g(NO_LEAD_ZEROS) = nolead;
            a.reset(n);
            orig<Num>(0x5d2da8)(num, digits, &a.out, &a.count);
            const int la = g(NO_LEAD_ZEROS);
            g(NO_LEAD_ZEROS) = nolead;
            b.reset(n);
            mine<Num>(0x5d2da8)(num, digits, &b.out, &b.count);
            wsprintfA(what, "_store_num %d %u nolead %d n %d", num, digits, nolead, (int)n);
            describe_out(d, what, a, b);
            t.check(a.same(b) && la == g(NO_LEAD_ZEROS), "", d);
            a.reset(n);
            orig<Number>(0x5d2e17)(num, &a.out, &a.count);
            b.reset(n);
            mine<Number>(0x5d2e17)(num, &b.out, &b.count);
            wsprintfA(what, "_store_number %d n %d", num, (int)n);
            describe_out(d, what, a, b);
            t.check(a.same(b), "", d);
        }
        // store_dt (byte arithmetic: any value)
        {
            const int v = (int)(random() % 2001) - 1000;
            a.reset(0);
            b.reset(0);
            const char *ra = orig<Dt>(0x5daab5)(a.out, v), *rb = mine<Dt>(0x5daab5)(b.out, v);
            wsprintfA(d, "[store_dt %d: %d %d / %d %d]", v, a.out[0], a.out[1], b.out[0], b.out[1]);
            t.check(!memcmp(a.b, b.b, sizeof a.b) && ra - a.b == rb - b.b, "", d);
        }
        // _expandtime, every specifier with and without '#'
        {
            const Tm tm = random_tm();
            const char spec = specifiers[k % (sizeof specifiers - 1)];
            const int alt = (int)(random() % 2);
            const size_t room = random() % 4 ? 1 + random() % 80 : random() % 3;
            g(ALTERNATE_FORM) = alt;
            g(NO_LEAD_ZEROS) = 0;
            a.reset(room);
            orig<Expand>(0x5d2992)(spec, &tm, &a.out, &a.count, lc_time());
            const int aa = g(ALTERNATE_FORM), an = g(NO_LEAD_ZEROS);
            g(ALTERNATE_FORM) = alt;
            g(NO_LEAD_ZEROS) = 0;
            b.reset(room);
            mine<Expand>(0x5d2992)(spec, &tm, &b.out, &b.count, lc_time());
            wsprintfA(what, "_expandtime %c alt %d room %d", spec, alt, (int)room);
            describe_out(d, what, a, b);
            t.check(a.same(b) && aa == g(ALTERNATE_FORM) && an == g(NO_LEAD_ZEROS), "", d);
        }
        // _store_winword: the locale's pictures (short and long date, time) and random ones
        {
            const Tm tm = random_tm();
            char picture[24];
            const char *format;
            if (k % 4 < 3)
                format = *reinterpret_cast<const char *const *>(lc_time() + 0xa0 + (k % 4) * 4);
            else {
                const int len = (int)(random() % 20);
                for (int i = 0; i < len; i++)
                    picture[i] = picture_letters[random() % (sizeof picture_letters - 1)];
                picture[len] = 0;
                format = picture;
            }
            const size_t room = random() % 4 ? 1 + random() % 80 : random() % 3;
            a.reset(room);
            orig<Word>(0x5d2e60)(format, &tm, &a.out, &a.count, lc_time());
            const int an = g(NO_LEAD_ZEROS);
            b.reset(room);
            mine<Word>(0x5d2e60)(format, &tm, &b.out, &b.count, lc_time());
            wsprintfA(what, "_store_winword \"%.24s\" room %d", format, (int)room);
            describe_out(d, what, a, b);
            t.check(a.same(b) && an == g(NO_LEAD_ZEROS), "", d);
        }
    }
    g(ALTERNATE_FORM) = saved_alt;
    g(NO_LEAD_ZEROS) = saved_nolead;
}

void check_strftime(Tally &t, int count)
{
    using Strftime = size_t(__cdecl *)(char *, size_t, const char *, const Tm *);
    using StrftimeLc = size_t(__cdecl *)(char *, size_t, const char *, const Tm *, const BYTE *);
    static const char *const formats[] = {
        "%a %A %b %B", "%c", "%#c", "%x", "%#x", "%X", "%#X", "%d.%m.%Y %H:%M:%S", "%#d.%#m.%#y %#H:%#I:%#M:%#S %#j",
        "%I %p %j %U %W %w %y %Y", "%#U %#W %#w", "%z %Z", "%#z %#Z", "%%", "100%% %q %", "Saved: %d %b %Y, %H:%M",
        "", "plain text", "%Y-%m-%d"};
    char d[600], what[120];
    OutBuf a, b;
    for (int k = 0; k < count; k++) {
        const Tm tm = random_tm();
        const char *f = formats[k % (sizeof formats / sizeof formats[0])];
        const size_t room = random() % 3 ? 1 + random() % 120 : random() % 20;
        const bool lc = k % 2;
        a.reset(0);
        b.reset(0);
        const size_t ra = lc ? orig<StrftimeLc>(0x5d2898)(a.out, room, f, &tm, lc_time()) : orig<Strftime>(0x5d287d)(a.out, room, f, &tm);
        const size_t rb = lc ? mine<StrftimeLc>(0x5d2898)(b.out, room, f, &tm, lc_time()) : mine<Strftime>(0x5d287d)(b.out, room, f, &tm);
        wsprintfA(what, "%s \"%s\" room %d: %d / %d", lc ? "_Strftime" : "strftime", f, (int)room, (int)ra, (int)rb);
        describe_out(d, what, a, b);
        t.check(ra == rb && !memcmp(a.b, b.b, sizeof a.b), "", d);
    }
}

// ---- the environment -------------------------------------------------------------------------------------------

int env_count()
{
    char **e = *at<char ***>(ENVIRON);
    int n = 0;
    if (e)
        while (e[n])
            n++;
    return n;
}

void check_env_lookup(Tally &t, int count)
{
    using Getenv = char *(__cdecl *)(const char *);
    using Findenv = int(__cdecl *)(const char *, int);
    using Coll = int(__cdecl *)(const char *, const char *, size_t);
    using Mbschr = char *(__cdecl *)(const char *, unsigned);
    using Strncnt = int(__cdecl *)(const char *, int);
    using Copy = char **(__cdecl *)(char **);
    static const char *const names[] = {"PATH", "path", "Path", "TZ", "windir", "WINDIR", "PAT", "PATHX", "", "=", "ZAXNONE", "temp"};
    char d[600];
    char **env = *at<char ***>(ENVIRON);
    const int n_env = env_count();
    for (int k = 0; k < count; k++) {
        if (g_time_trace)
            log("T env %d", k);
        // getenv of fixed names and of the names in the table
        char name[64];
        if (k % 2 && n_env) {
            const char *e = env[random() % (unsigned)n_env];
            int i = 0;
            while (e[i] && e[i] != '=' && i < 62)
                i++;
            lstrcpynA(name, e, i + 1);
            if (random() % 3 == 0 && name[0])
                name[random() % (unsigned)lstrlenA(name)] ^= 0x20;   // other case
        } else
            lstrcpynA(name, names[k % (sizeof names / sizeof names[0])], 64);
        if (g_time_trace)
            log("T %s", "getenv");
        const char *ga = orig<Getenv>(0x5dca1c)(name), *gb = mine<Getenv>(0x5dca1c)(name);
        wsprintfA(d, "[getenv \"%s\": %x / %x]", name, (DWORD)(uintptr_t)ga, (DWORD)(uintptr_t)gb);
        t.check(ga == gb, "", d);
        const int len = random() % 4 ? lstrlenA(name) : (int)(random() % 8);
        if (g_time_trace)
            log("T %s", "findenv");
        const int fa = orig<Findenv>(0x5de183)(name, len), fb = mine<Findenv>(0x5de183)(name, len);
        wsprintfA(d, "[findenv \"%s\" %d: %d / %d]", name, len, fa, fb);
        t.check(fa == fb, "", d);
        // __mbsnbicoll on case variants and other texts
        char s1[24], s2[24];
        const int l1 = (int)(random() % 20);
        for (int i = 0; i < l1; i++)
            s1[i] = (char)(random() % 4 ? 'A' + random() % 26 : 0x20 + random() % 0xdf);
        s1[l1] = 0;
        lstrcpyA(s2, s1);
        for (int i = 0; i < l1; i++)
            if (random() % 3 == 0)
                s2[i] = (char)(random() % 2 ? s2[i] ^ 0x20 : 'a' + random() % 26);
        const size_t cn = random() % (unsigned)(l1 + 3);
        if (g_time_trace)
            log("T %s", "coll");
        const int ca = orig<Coll>(0x5ddac1)(s1, s2, cn), cb = mine<Coll>(0x5ddac1)(s1, s2, cn);
        wsprintfA(d, "[__mbsnbicoll \"%s\" \"%s\" %d: %d / %d]", s1, s2, (int)cn, ca, cb);
        t.check(ca == cb, "", d);
        // _mbschr, strncnt
        const unsigned ch = random() % 3 ? (BYTE)s1[l1 ? random() % (unsigned)l1 : 0] : random() % 2 ? '=' : random() & 0xffff;
        if (g_time_trace)
            log("T %s", "mbschr");
        const char *ma = orig<Mbschr>(0x5de242)(s1, ch), *mb = mine<Mbschr>(0x5de242)(s1, ch);
        wsprintfA(d, "[_mbschr \"%s\" %x: %d / %d]", s1, ch, ma ? (int)(ma - s1) : -1, mb ? (int)(mb - s1) : -1);
        t.check(ma == mb, "", d);
        const int sc = (int)(random() % 24);
        if (g_time_trace)
            log("T %s", "strncnt");
        const int na = orig<Strncnt>(0x5ddfd1)(s1, sc), nb = mine<Strncnt>(0x5ddfd1)(s1, sc);
        wsprintfA(d, "[strncnt \"%s\" %d: %d / %d]", s1, sc, na, nb);
        t.check(na == nb, "", d);
    }
    if (g_time_trace)
        log("T copy_environ");
    // copy_environ: the same strings, then both copies freed
    char **ca = orig<Copy>(0x5de1db)(env), **cb = mine<Copy>(0x5de1db)(env);
    bool same = (ca == nullptr) == (cb == nullptr);
    for (int i = 0; same && ca && (ca[i] || cb[i]); i++)
        same = ca[i] && cb[i] && ca[i] != env[i] && cb[i] != env[i] && eq(ca[i], env[i]) && eq(cb[i], env[i]);
    char **const copies[] = {ca, cb};
    for (char **c : copies) {
        for (int i = 0; c && c[i]; i++)
            exe_free(c[i]);
        exe_free(c);
    }
    wsprintfA(d, "[copy_environ of %d entries]", n_env);
    t.check(same, "", d);
    char **null_env = nullptr;
    t.check(orig<Copy>(0x5de1db)(null_env) == mine<Copy>(0x5de1db)(null_env), "", "[copy_environ of null]");
}

// __crtsetenv: the same sequence of operations on test variables of each side's own name (ZAXCHKA_n,
// ZAXCHKB_n): adding, changing, removing, removing what is not there, errors ("=x", no '='); with
// primary also in the process environment
void check_setenv(Tally &t, int count)
{
    using SetEnv = int(__cdecl *)(char *, int);
    using Getenv = char *(__cdecl *)(const char *);
    char d[600];
    // on a copy of the table (the game's own stays as it is)
    char **const game_env = *at<char ***>(ENVIRON);
    *at<char ***>(ENVIRON) = orig<char **(__cdecl *)(char **)>(0x5de1db)(game_env);
    const int start_count = env_count();
    for (int k = 0; k < count; k++) {
        const int var = (int)(random() % 4), kind = (int)(random() % 8), primary = (int)(random() % 2);
        if (g_time_trace)
            log("T setenv %d: var %d kind %d primary %d, %d entries", k, var, kind, primary, env_count());
        char value[16];
        wsprintfA(value, "%u", random() % 100000);
        int r[2], count_after[2];
        char seen[2][4][24], process[2][24];
        for (int side = 0; side < 2; side++) {
            const char letter = side ? 'B' : 'A';
            char text[48];
            switch (kind) {
            case 0: wsprintfA(text, "ZAXCHK%c_%d=", letter, var); break;   // remove
            case 1: wsprintfA(text, "=ZAXCHK%c_%d", letter, var); break;   // no name
            case 2: wsprintfA(text, "ZAXCHK%c_%d", letter, var); break;    // no '='
            default: wsprintfA(text, "ZAXCHK%c_%d=%s", letter, var, value); break;
            }
            char *option = exe_strdup(text);
            const int before = env_count();
            if (g_time_trace)
                log("T side %d \"%s\" %x env %x initenv %x", side, text, (DWORD)(uintptr_t)option, *at<DWORD *>(ENVIRON), *at<DWORD *>(0x7158a0));
            r[side] = (side ? mine<SetEnv>(0x5ddffc) : orig<SetEnv>(0x5ddffc))(option, primary);
            if (g_time_trace)
                log("T side %d: %d", side, r[side]);
            count_after[side] = env_count() - before;
            if (g_time_trace)
                log("T count %d", count_after[side]);
            for (int v = 0; v < 4; v++) {
                char name[24];
                wsprintfA(name, "ZAXCHK%c_%d", letter, v);
                const char *g = mine<Getenv>(0x5dca1c)(name);
                lstrcpynA(seen[side][v], g ? g : "(none)", 24);
            }
            if (g_time_trace)
                log("T getenv done");
            char name[24];
            wsprintfA(name, "ZAXCHK%c_%d", letter, var);
            if (!GetEnvironmentVariableA(name, process[side], 24))
                lstrcpyA(process[side], "(none)");
        }
        bool same = r[0] == r[1] && count_after[0] == count_after[1] && eq(process[0], process[1]);
        for (int v = 0; v < 4; v++)
            same = same && eq(seen[0][v], seen[1][v]);
        wsprintfA(d, "[__crtsetenv %d var %d primary %d: %d entries change %d %s %s %s %s (%s) / %d entries change %d %s %s %s %s (%s)]", kind, var,
                  primary, r[0], count_after[0], seen[0][0], seen[0][1], seen[0][2], seen[0][3], process[0], r[1], count_after[1],
                  seen[1][0], seen[1][1], seen[1][2], seen[1][3], process[1]);
        t.check(same, "", d);
    }
    // the table as before: every test variable removed (in the process environment too)
    for (int side = 0; side < 2; side++)
        for (int v = 0; v < 4; v++) {
            char text[32];
            wsprintfA(text, "ZAXCHK%c_%d=", side ? 'B' : 'A', v);
            mine<SetEnv>(0x5ddffc)(exe_strdup(text), 1);
        }
    wsprintfA(d, "[__crtsetenv: %d entries before, %d after]", start_count, env_count());
    t.check(env_count() == start_count, "", d);
    char **copy = *at<char ***>(ENVIRON);
    for (int i = 0; copy[i]; i++)
        exe_free(copy[i]);
    exe_free(copy);
    // __wtomb_environ (the game has no wide environment, _wenviron is 0): a private wide table of three
    // entries (a new one, a changed one, one with '=' first) converted into a copy of the table
    static const WORD w1[] = {'Z', 'A', 'X', 'W', '_', 'A', '=', '1', 0}, w2[] = {'Z', 'A', 'X', 'W', '_', 'A', '=', 0xe4, 0},
                      w3[] = {'=', 'C', ':', '=', 'x', 0};
    const WORD *wide[] = {w1, w2, w3, nullptr};
    char texts[2][4][24] = {};
    int r[2], counts[2];
    for (int side = 0; side < 2; side++) {
        *at<char ***>(ENVIRON) = orig<char **(__cdecl *)(char **)>(0x5de1db)(game_env);
        *at<const WORD ***>(WENVIRON) = wide;
        r[side] = (side ? mine<int(__cdecl *)()>(0x5ddb00) : orig<int(__cdecl *)()>(0x5ddb00))();
        *at<const WORD ***>(WENVIRON) = nullptr;
        counts[side] = env_count();
        static const char *const names[] = {"ZAXW_A", "ZAXW_B", "", "PATH"};
        for (int v = 0; v < 4; v++) {
            const char *g = mine<char *(__cdecl *)(const char *)>(0x5dca1c)(names[v]);
            lstrcpynA(texts[side][v], g ? g : "(none)", 24);
        }
        char **e = *at<char ***>(ENVIRON);
        for (int i = 0; e[i]; i++)
            exe_free(e[i]);
        exe_free(e);
    }
    *at<char ***>(ENVIRON) = game_env;
    wsprintfA(d, "[__wtomb_environ: %d, %d entries, %s %s / %d, %d entries, %s %s]", r[0], counts[0], texts[0][0], texts[0][3], r[1],
              counts[1], texts[1][0], texts[1][3]);
    t.check(r[0] == r[1] && counts[0] == counts[1] && !memcmp(texts[0], texts[1], sizeof texts[0]), "", d);
}

}  // namespace

ZAX_CHECKS(crt_time)
{
    (void)cls;
    (void)n;
    Tally a{"crt_time: gmtime, localtime, asctime, ctime, __loctotime_t, _isindst, cvtdate"};
    Tally b{"crt_time: _tzset_lk, __tzset, time"};
    Tally c{"crt_time: strftime, _Strftime, _expandtime, _store_winword, _store_num(ber), _store_str, store_dt"};
    Tally e{"crt_time: getenv, findenv, __mbsnbicoll, _mbschr, strncnt, copy_environ, __crtsetenv"};
    log("  _wenviron %x, _environ %d entries", *at<DWORD *>(WENVIRON), env_count());
    at<void(__cdecl *)()>(0x5d8fc2)();   // __tzset as the game has it once a time was converted
    ZoneState zone;
    zone.save();
    g_seed = 0x5d9584;
    // the system's zone (GetTimeZoneInformation), a TZ zone (the fixed US rules), made-up Windows zones
    check_conversions(a, zone, scaled(2000));
    {
        using SetEnv = int(__cdecl *)(char *, int);
        char tz[] = "TZ=PST8PDT", none[] = "TZ=";
        mine<SetEnv>(0x5ddffc)(exe_strdup(tz), 0);
        ZoneState z = zone;
        z.last = nullptr;
        z.load();
        at<void(__cdecl *)()>(0x5d8ff0)();
        z.save();
        check_conversions(a, z, scaled(1000));
        exe_free(z.last);
        mine<SetEnv>(0x5ddffc)(exe_strdup(none), 0);
    }
    for (int k = 0; k < scaled(40); k++) {
        ZoneState z = zone;
        make_zone(z, k);
        check_conversions(a, z, 25);
    }
    zone.load();
    a.report();
    check_tzset(b, zone);
    check_time(b, zone, scaled(200));
    b.report();
    check_store(c, scaled(3000));
    check_strftime(c, scaled(3000));
    c.report();
    check_env_lookup(e, scaled(2000));
    check_setenv(e, scaled(400));
    e.report();
    zone.load();
}

}  // namespace zax
