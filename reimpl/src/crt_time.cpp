// The time and environment functions of the C runtime (VC6 time.c, dtoxtime.c, tzset.c, gmtime.c,
// localtim.c, asctime.c, ctime.c, strftime.c, getenv.c, setenv.c, wtombenv.c, mbsnbicm.c, mbschr.c;
// docs/DECOMPILATION.md 11, "C runtime, phase 5b"). The game calls time/localtime/strftime for the save
// game description (0x5bc2a0) and the watch item (0x596850); _tzset reaches the environment through
// getenv("TZ"). The runtime's data stays in the exe (the time zone variables, the transition dates, the
// cached GetSystemTime, the environment tables); Windows through the exe's import slots.
#include "crt_common.h"

namespace zax::crt {

namespace {

// ---- data ---------------------------------------------------------------------------------------------------

constexpr DWORD TIMEZONE = 0x62c120, DAYLIGHT = 0x62c124, DSTBIAS = 0x62c128;   // _timezone, _daylight, _dstbias
constexpr DWORD TZNAME = 0x62c1ac;           // _tzname[2] (pointers to 64-byte buffers)
constexpr DWORD LPDAYS = 0x62c1d4, DAYS = 0x62c208;   // _lpdays[13], _days[13]: days before each month, -1 based
constexpr DWORD DSTSTART = 0x62c1b8, DSTEND = 0x62c1c8;   // struct { int yr, yd, ms; } dststart, dstend
constexpr DWORD TZINFO = 0x7159d0;           // TIME_ZONE_INFORMATION of the last GetTimeZoneInformation
constexpr DWORD TZAPIUSED = 0x7159c8, TZ_FIRST_TIME = 0x715a80, LAST_TZ = 0x715a7c;
constexpr DWORD GMT_CACHE = 0x7158c8, DST_CACHE = 0x7158c0;   // time(): the last GetSystemTime and its DST flag
constexpr DWORD GMTIME_STATIC = 0x7159a0, ASCTIME_STATIC = 0x715a84;   // without per-thread buffers
// "SunMon...", "JanFeb..." (asctime reads the month names at 0x60730f + i + 1: its index is the loop counter
// after the increment)
constexpr DWORD DNAMES = 0x6072f8, MNAMES = 0x607310;
constexpr DWORD LC_TIME_CURR = 0x629820;     // __lc_time_curr
constexpr DWORD ALTERNATE_FORM = 0x717548, NO_LEAD_ZEROS = 0x717548 + 4;   // strftime's '#' state
constexpr DWORD LC_CODEPAGE = 0x71597c;      // __lc_codepage
constexpr DWORD ENVIRON = 0x71589c, INITENV = 0x7158a0, WENVIRON = 0x7158a4, ENV_INITIALIZED = 0x717550;
constexpr DWORD MBCTYPE = 0x7162e0, ISMBCODEPAGE = 0x7161cc, MBCODEPAGE = 0x7161b0, MBLCID = 0x7163e4;
constexpr DWORD TXT_TZ = 0x607338, TXT_AMPM = 0x607078, TXT_AP = 0x607074;
constexpr int TZ_LOCK = 0xb, ENV_LOCK = 0xc, SETLOCALE_LOCK_ = 0x13, MB_CP_LOCK = 0x19;
constexpr int DAY_SEC = 86400, YEAR_SEC = 365 * DAY_SEC, LEAP_YEAR_SEC = 366 * DAY_SEC,
              FOUR_YEAR_SEC = 1461 * DAY_SEC, DAY_MS = 86400000;

namespace iatx {
constexpr DWORD WideCharToMultiByte = 0x5ea120, SetEnvironmentVariableA = 0x5ea060;
}

struct Tm {
    int sec, min, hour, mday, mon, year, wday, yday, isdst;
};
static_assert(sizeof(Tm) == 0x24, "struct tm");

struct TransitionDate {
    int yr, yd, ms;
};

struct SystemTime {
    WORD year, month, dayofweek, day, hour, minute, second, ms;
};

struct TimeZoneInformation {
    int bias;
    WORD standard_name[32];
    SystemTime standard_date;
    int standard_bias;
    WORD daylight_name[32];
    SystemTime daylight_date;
    int daylight_bias;
};
static_assert(sizeof(TimeZoneInformation) == 0xac, "TIME_ZONE_INFORMATION");

inline int &g(DWORD a) { return *at<int *>(a); }
inline TransitionDate &dststart() { return *glob<TransitionDate *>(DSTSTART); }
inline TransitionDate &dstend() { return *glob<TransitionDate *>(DSTEND); }
inline TimeZoneInformation &tzinfo() { return *glob<TimeZoneInformation *>(TZINFO); }
inline const int *lpdays() { return glob<const int *>(LPDAYS); }
inline const int *days() { return glob<const int *>(DAYS); }
inline char *tzname(int i) { return glob<char **>(TZNAME)[i]; }

// The runtime functions these call, through the exe's entry points as the original does
inline void exe_lock(int n) { fn<void(__cdecl *)(int)>(0x5d4305)(n); }
inline void exe_unlock(int n) { fn<void(__cdecl *)(int)>(0x5d4366)(n); }
inline size_t exe_strlen(const char *s) { return fn<size_t(__cdecl *)(const char *)>(0x5d3be0)(s); }
inline char *exe_strcpy(char *d, const char *s) { return fn<char *(__cdecl *)(char *, const char *)>(0x5daae0)(d, s); }
inline char *exe_strncpy(char *d, const char *s, size_t n) { return fn<char *(__cdecl *)(char *, const char *, size_t)>(0x5d0b50)(d, s, n); }
inline int exe_strcmp(const char *a, const char *b) { return fn<int(__cdecl *)(const char *, const char *)>(0x5db3e0)(a, b); }
inline int exe_stricmp(const char *a, const char *b) { return fn<int(__cdecl *)(const char *, const char *)>(0x5d3640)(a, b); }
inline long exe_atol(const char *s) { return fn<long(__cdecl *)(const char *)>(0x5d0348)(s); }
inline char *exe_strdup(const char *s) { return fn<char *(__cdecl *)(const char *)>(0x5d3606)(s); }
inline char *exe_strchr(const char *s, int c) { return fn<char *(__cdecl *)(const char *, int)>(0x5d08c0)(s, c); }
inline ThreadData *exe_getptd() { return fn<ThreadData *(__cdecl *)()>(0x5d4f28)(); }
inline void exe_amsg_exit(int n) { fn<void(__cdecl *)(int)>(0x5d3397)(n); }
inline bool isleadbyte_(BYTE c) { return (*glob<const WORD **>(PCTYPE))[c] & 0x8000; }

}  // namespace

// ---- time zones ----------------------------------------------------------------------------------------------

// The time zone functions of this module, through the exe's entry points (hooked) where the original calls them
void __cdecl tzset_();
int __cdecl isindst(Tm *tb);

// _tzset_lk 0x5d8ff0: the zone from TZ ("PST8PDT", "CET-1:00") or, without it, from GetTimeZoneInformation
void __cdecl tzset_lk()
{
    int negdiff = 0;
    exe_lock(ENV_LOCK);
    dststart().yr = dstend().yr = -1;
    g(glob_addr(TZAPIUSED)) = 0;
    const char *tz = fn<char *(__cdecl *)(const char *)>(0x5dca1c)(glob<const char *>(TXT_TZ));
    if (!tz) {
        exe_unlock(ENV_LOCK);
        if (api<DWORD(WINAPI *)(void *)>(iat::GetTimeZoneInformation)(&tzinfo()) == 0xffffffff)
            return;
        TimeZoneInformation &z = tzinfo();
        g(glob_addr(TZAPIUSED)) = 1;
        g(glob_addr(TIMEZONE)) = z.bias * 60;
        if (z.standard_date.month)
            g(glob_addr(TIMEZONE)) += z.standard_bias * 60;
        if (z.daylight_date.month && z.daylight_bias) {
            g(glob_addr(DAYLIGHT)) = 1;
            g(glob_addr(DSTBIAS)) = (z.daylight_bias - z.standard_bias) * 60;
        } else {
            g(glob_addr(DAYLIGHT)) = 0;
            g(glob_addr(DSTBIAS)) = 0;
        }
        using Wctmb = int(WINAPI *)(unsigned, DWORD, const WORD *, int, char *, int, const char *, int *);
        int defused;
        if (api<Wctmb>(iatx::WideCharToMultiByte)((unsigned)g(glob_addr(LC_CODEPAGE)), 0x220, z.standard_name, -1, tzname(0), 63, nullptr, &defused) && !defused)
            tzname(0)[63] = 0;
        else
            tzname(0)[0] = 0;
        if (api<Wctmb>(iatx::WideCharToMultiByte)((unsigned)g(glob_addr(LC_CODEPAGE)), 0x220, z.daylight_name, -1, tzname(1), 63, nullptr, &defused) && !defused)
            tzname(1)[63] = 0;
        else
            tzname(1)[0] = 0;
        return;
    }
    char *&last = *glob<char **>(LAST_TZ);
    if (!*tz || (last && !exe_strcmp(tz, last))) {
        exe_unlock(ENV_LOCK);
        return;
    }
    exe_free(last);
    if (!(last = (char *)exe_malloc(exe_strlen(tz) + 1))) {
        exe_unlock(ENV_LOCK);
        return;
    }
    exe_strcpy(last, tz);
    exe_unlock(ENV_LOCK);
    exe_strncpy(tzname(0), tz, 3);
    tzname(0)[3] = 0;
    tz += 3;
    if (*tz == '-') {
        negdiff = 1;
        tz++;
    }
    g(glob_addr(TIMEZONE)) = (int)exe_atol(tz) * 3600;
    while (*tz == '+' || (*tz >= '0' && *tz <= '9'))
        tz++;
    if (*tz == ':') {
        tz++;
        g(glob_addr(TIMEZONE)) += (int)exe_atol(tz) * 60;
        while (*tz >= '0' && *tz <= '9')
            tz++;
        if (*tz == ':') {
            tz++;
            g(glob_addr(TIMEZONE)) += (int)exe_atol(tz);
            while (*tz >= '0' && *tz <= '9')
                tz++;
        }
    }
    if (negdiff)
        g(glob_addr(TIMEZONE)) = -g(glob_addr(TIMEZONE));
    if ((g(glob_addr(DAYLIGHT)) = *tz) != 0) {
        exe_strncpy(tzname(1), tz, 3);
        tzname(1)[3] = 0;
    } else
        tzname(1)[0] = 0;
}

// __tzset 0x5d8fc2: _tzset_lk once
void __cdecl tzset_()
{
    if (g(glob_addr(TZ_FIRST_TIME)))
        return;
    exe_lock(TZ_LOCK);
    if (!g(glob_addr(TZ_FIRST_TIME))) {
        fn<void(__cdecl *)()>(0x5d8ff0)();
        g(glob_addr(TZ_FIRST_TIME))++;
    }
    exe_unlock(TZ_LOCK);
}

// cvtdate 0x5d9444: a transition date into dststart (trantype 1) or dstend: the day of the year and the
// milliseconds; datetype 1 = the week'th dayofweek of the month (5 the last), 0 = an absolute date
void __cdecl cvtdate(int trantype, int datetype, int year, int month, int week, int dayofweek, int date, int hour,
                     int min, int sec, int msec)
{
    int yearday;
    const bool leap = !(year & 3);
    if (datetype == 1) {
        yearday = 1 + (leap ? lpdays()[month - 1] : days()[month - 1]);
        const int monthdow = (yearday + ((year - 1) >> 2) + year * 365 - 25563) % 7;
        if (monthdow > dayofweek)
            yearday += week * 7 - monthdow + dayofweek;
        else
            yearday += week * 7 - monthdow + dayofweek - 7;
        if (week == 5 && yearday > (leap ? lpdays()[month] : days()[month]))
            yearday -= 7;
    } else
        yearday = (leap ? lpdays()[month - 1] : days()[month - 1]) + date;
    if (trantype == 1) {
        dststart().yd = yearday;
        dststart().ms = ((hour * 60 + min) * 60 + sec) * 1000 + msec;
        dststart().yr = year;
    } else {
        dstend().yd = yearday;
        int ms = (((hour * 60 + min) * 60 + g(glob_addr(DSTBIAS)) + sec) * 1000) + msec;
        dstend().ms = ms;
        if (ms < 0) {
            dstend().ms = ms + DAY_MS;
            dstend().yd = yearday - 1;
        } else if (ms >= DAY_MS) {
            dstend().ms = ms - DAY_MS;
            dstend().yd = yearday + 1;
        }
        dstend().yr = year;
    }
}

// _isindst_lk 0x5d9298: whether tb lies in daylight saving time (the transition dates of its year computed
// once per year: from the API's dates, or the US rule of the time: first Sunday in April to last in October)
int __cdecl isindst_lk(Tm *tb)
{
    if (!g(glob_addr(DAYLIGHT)))
        return 0;
    using Cvt = void(__cdecl *)(int, int, int, int, int, int, int, int, int, int, int);
    const Cvt cvt = code<Cvt>(0x5d9444);
    if (tb->year != dststart().yr || tb->year != dstend().yr) {
        if (g(glob_addr(TZAPIUSED))) {
            const SystemTime &d = tzinfo().daylight_date, &s = tzinfo().standard_date;
            if (!d.year)
                cvt(1, 1, tb->year, d.month, d.day, d.dayofweek, 0, d.hour, d.minute, d.second, d.ms);
            else
                cvt(1, 0, tb->year, d.month, 0, 0, d.day, d.hour, d.minute, d.second, d.ms);
            if (!s.year)
                cvt(0, 1, tb->year, s.month, s.day, s.dayofweek, 0, s.hour, s.minute, s.second, s.ms);
            else
                cvt(0, 0, tb->year, s.month, 0, 0, s.day, s.hour, s.minute, s.second, s.ms);
        } else {
            cvt(1, 1, tb->year, 4, 1, 0, 0, 2, 0, 0, 0);
            cvt(0, 1, tb->year, 10, 5, 0, 0, 2, 0, 0, 0);
        }
    }
    const int start = dststart().yd, end = dstend().yd, yday = tb->yday;
    if (start < end) {   // northern hemisphere
        if (yday < start || yday > end)
            return 0;
        if (yday > start && yday < end)
            return 1;
    } else {             // southern
        if (yday < end || yday > start)
            return 1;
        if (yday > end && yday < start)
            return 0;
    }
    const int ms = ((tb->hour * 60 + tb->min) * 60 + tb->sec) * 1000;
    if (yday == start)
        return ms >= dststart().ms;
    return ms < dstend().ms;
}

// _isindst 0x5d9277: under the time zone lock
int __cdecl isindst(Tm *tb)
{
    exe_lock(TZ_LOCK);
    const int r = fn<int(__cdecl *)(Tm *)>(0x5d9298)(tb);
    exe_unlock(TZ_LOCK);
    return r;
}

// ---- time and its conversions ----------------------------------------------------------------------------

// __loctotime_t 0x5d9584: a local date and time (years 1970-2038) as seconds since 1970 UTC; -1 outside.
// dstflag 1 adds the DST bias, -1 asks _isindst (with a tm of only year, month, hour and day of the year)
long __cdecl loctotime_t(int yr, int mo, int dy, int hr, int mn, int sc, int dstflag)
{
    yr -= 1900;
    if (yr < 70 || yr > 138)
        return -1;
    int tmpdays = dy + days()[mo - 1];
    if (!(yr & 3) && mo > 2)
        tmpdays++;
    fn<void(__cdecl *)()>(0x5d8fc2)();
    // the original's arithmetic, wrapping like it (the base 2208988800 s of 1900..1970 in one constant)
    unsigned t = (unsigned)yr * 365u + (unsigned)((yr - 1) >> 2) + (unsigned)tmpdays;
    t = ((t * 24u + (unsigned)hr) * 60u + (unsigned)mn) * 60u + (unsigned)g(glob_addr(TIMEZONE));
    t = t + (unsigned)sc + 0x7c558180u;
    Tm tb;   // only these fields, as the original
    tb.yday = tmpdays;
    tb.year = yr;
    tb.mon = mo - 1;
    tb.hour = hr;
    if (dstflag == 1 || (dstflag == -1 && g(glob_addr(DAYLIGHT)) && fn<int(__cdecl *)(Tm *)>(0x5d9277)(&tb)))
        t += (unsigned)g(glob_addr(DSTBIAS));
    return (long)t;
}

// time 0x5d20af: GetLocalTime as seconds; the DST flag from GetTimeZoneInformation, asked again only when
// the system time's minute changed
long __cdecl time_(long *timeptr)
{
    SystemTime loct, gmt;
    TimeZoneInformation tz;
    api<void(WINAPI *)(SystemTime *)>(iat::GetLocalTime)(&loct);
    api<void(WINAPI *)(SystemTime *)>(iat::GetSystemTime)(&gmt);
    SystemTime &cache = *glob<SystemTime *>(GMT_CACHE);
    int dstflag;
    if (gmt.minute == cache.minute && gmt.hour == cache.hour && gmt.day == cache.day && gmt.month == cache.month &&
        gmt.year == cache.year)
        dstflag = g(glob_addr(DST_CACHE));
    else {
        const DWORD id = api<DWORD(WINAPI *)(TimeZoneInformation *)>(iat::GetTimeZoneInformation)(&tz);
        if (id == 0xffffffff)
            dstflag = -1;
        else
            dstflag = (id == 2 && tz.daylight_date.month && tz.daylight_bias) ? 1 : 0;
        cache = gmt;
        g(glob_addr(DST_CACHE)) = dstflag;
    }
    const long tim = fn<long(__cdecl *)(int, int, int, int, int, int, int)>(0x5d9584)(loct.year, loct.month, loct.day,
                                                                                    loct.hour, loct.minute, loct.second, dstflag);
    if (timeptr)
        *timeptr = tim;
    return tim;
}

// gmtime 0x5d8eb8: seconds since 1970 as UTC calendar time, in the thread's buffer (malloc'd on first use)
Tm *__cdecl gmtime_(const long *timp)
{
    long caltim = *timp;
    int islpyr = 0;
    ThreadData *ptd = exe_getptd();
    if (caltim < 0)
        return nullptr;
    void *&buf = *reinterpret_cast<void **>(reinterpret_cast<BYTE *>(ptd) + 0x40);   // _gmtimebuf
    Tm *ptb;
    if (!buf) {
        buf = exe_malloc(sizeof(Tm));
        ptb = buf ? (Tm *)buf : glob<Tm *>(GMTIME_STATIC);
    } else
        ptb = (Tm *)buf;
    int tmptim = (int)(caltim / FOUR_YEAR_SEC);
    caltim -= tmptim * FOUR_YEAR_SEC;
    tmptim = tmptim * 4 + 70;
    if (caltim >= YEAR_SEC) {
        tmptim++;
        caltim -= YEAR_SEC;
        if (caltim >= YEAR_SEC) {
            tmptim++;
            caltim -= YEAR_SEC;
            if (caltim >= LEAP_YEAR_SEC) {
                tmptim++;
                caltim -= LEAP_YEAR_SEC;
            } else
                islpyr = 1;
        }
    }
    ptb->year = tmptim;
    ptb->yday = (int)(caltim / DAY_SEC);
    caltim -= ptb->yday * DAY_SEC;
    const int *mdays = islpyr ? lpdays() : days();
    for (tmptim = 1; mdays[tmptim] < ptb->yday; tmptim++)
        ;
    ptb->mon = --tmptim;
    ptb->mday = ptb->yday - mdays[tmptim];
    ptb->wday = (int)((*timp / DAY_SEC + 4) % 7);
    ptb->hour = (int)(caltim / 3600);
    caltim -= ptb->hour * 3600;
    ptb->min = (int)(caltim / 60);
    ptb->sec = (int)(caltim - ptb->min * 60);
    ptb->isdst = 0;
    return ptb;
}

// localtime 0x5d312f: gmtime of the time minus the zone (and DST); near the ends of the range the
// conversion is done afterwards, field by field
Tm *__cdecl localtime_(const long *ptime)
{
    using Gm = Tm *(__cdecl *)(const long *);
    using Isdst = int(__cdecl *)(Tm *);
    const Gm gm = code<Gm>(0x5d8eb8);
    const Isdst isdst_ = code<Isdst>(0x5d9277);
    if (*ptime < 0)
        return nullptr;
    fn<void(__cdecl *)()>(0x5d8fc2)();
    long ltime;
    Tm *ptm;
    if (*ptime > 3 * DAY_SEC && *ptime < 0x7fffffff - 3 * DAY_SEC) {
        ltime = *ptime - g(glob_addr(TIMEZONE));
        ptm = gm(&ltime);
        if (g(glob_addr(DAYLIGHT)) && isdst_(ptm)) {
            ltime -= g(glob_addr(DSTBIAS));
            ptm = gm(&ltime);
            ptm->isdst = 1;
        }
        return ptm;
    }
    ptm = gm(ptime);
    ltime = ptm->sec;
    if (isdst_(ptm))
        ltime -= g(glob_addr(DSTBIAS));
    ltime -= g(glob_addr(TIMEZONE));
    ptm->sec = (int)(ltime % 60);
    if (ptm->sec < 0) {
        ptm->sec += 60;
        ltime -= 60;
    }
    ltime = ltime / 60 + ptm->min;
    ptm->min = (int)(ltime % 60);
    if (ptm->min < 0) {
        ptm->min += 60;
        ltime -= 60;
    }
    ltime = ltime / 60 + ptm->hour;
    ptm->hour = (int)(ltime % 24);
    if (ptm->hour < 0) {
        ptm->hour += 24;
        ltime -= 24;
    }
    ltime /= 24;
    if (ltime > 0) {
        ptm->wday = (int)((ptm->wday + ltime) % 7);
        ptm->mday += (int)ltime;
        ptm->yday += (int)ltime;
    } else if (ltime < 0) {
        ptm->wday = (int)((ptm->wday + ltime + 7) % 7);
        if ((ptm->mday += (int)ltime) > 0)
            ptm->yday += (int)ltime;
        else {
            ptm->mday += 31;
            ptm->yday = 364;
            ptm->mon = 11;
            ptm->year--;
        }
    }
    return ptm;
}

// store_dt 0x5daab5: two digits of val (byte arithmetic: any value)
char *__cdecl store_dt(char *p, int val)
{
    *p++ = (char)((BYTE)(val / 10) + '0');
    *p++ = (char)((BYTE)(val % 10) + '0');
    return p;
}

// asctime 0x5da9eb: "Wed Jan 02 02:03:55 1980\n" in the thread's buffer
char *__cdecl asctime_(const Tm *tb)
{
    ThreadData *ptd = exe_getptd();
    char *&buf = *reinterpret_cast<char **>(reinterpret_cast<BYTE *>(ptd) + 0x38);   // _asctimebuf
    char *p;
    if (!buf) {
        buf = (char *)exe_malloc(26);
        p = buf ? buf : glob<char *>(ASCTIME_STATIC);
    } else
        p = buf;
    char *const start = p;
    const char *dn = glob<const char *>(DNAMES) + tb->wday * 3, *mn = glob<const char *>(MNAMES) + tb->mon * 3;
    for (int i = 0; i < 3; i++) {
        p[i] = dn[i];
        p[i + 4] = mn[i];
    }
    p[3] = ' ';
    p[7] = ' ';
    p += 8;
    using Dt = char *(__cdecl *)(char *, int);
    const Dt dt = code<Dt>(0x5daab5);
    p = dt(p, tb->mday);
    *p++ = ' ';
    p = dt(p, tb->hour);
    *p++ = ':';
    p = dt(p, tb->min);
    *p++ = ':';
    p = dt(p, tb->sec);
    *p++ = ' ';
    p = dt(p, tb->year / 100 + 19);
    p = dt(p, tb->year % 100);
    *p++ = '\n';
    *p = 0;
    return start;
}

// ctime 0x5d2864: asctime(localtime(t))
char *__cdecl ctime_(const long *t)
{
    Tm *tm = fn<Tm *(__cdecl *)(const long *)>(0x5d312f)(t);
    return tm ? fn<char *(__cdecl *)(const Tm *)>(0x5da9eb)(tm) : nullptr;
}

// ---- strftime ---------------------------------------------------------------------------------------------------

// _store_str 0x5d2d81: the text while there is room
void __cdecl store_str(const char *in, char **out, size_t *count)
{
    if (!*count)
        return;
    while (*in) {
        *(*out)++ = *in++;
        if (!--*count)
            break;
    }
}

// _store_number 0x5d2e17: without leading zeros: the digits backwards, then reversed in place (also when no
// digit fits: then the byte before the output is swapped with the first, as in the original)
void __cdecl store_number(int num, char **out, size_t *count)
{
    char *p = *out;
    while (*count > 1) {
        *p++ = (char)((BYTE)(num % 10) + '0');
        num /= 10;
        --*count;
        if (num <= 0)
            break;
    }
    char *left = *out;
    *out = p;
    p--;
    do {
        const char a = *left, b = *p;
        *p-- = a;
        *left++ = b;
    } while (left < p);
}

// _store_num 0x5d2da8: digits with leading zeros (none with '#'); nothing and *count 0 when they do not fit
void __cdecl store_num(int num, unsigned digits, char **out, size_t *count)
{
    if (g(glob_addr(NO_LEAD_ZEROS))) {
        fn<void(__cdecl *)(int, char **, size_t *)>(0x5d2e17)(num, out, count);
        return;
    }
    if (digits >= *count) {
        *count = 0;
        return;
    }
    int n = 0;
    for (int i = (int)digits - 1; i != -1; i--) {
        (*out)[i] = (char)((BYTE)(num % 10) + '0');
        num /= 10;
        n++;
    }
    *out += n;
    *count -= n;
}

void __cdecl expandtime(char specifier, const Tm *tm, char **out, size_t *count, const BYTE *lc);

// _store_winword 0x5d2e60: a Windows date or time picture ("dddd, MMMM dd, yyyy", "h:mm:ss tt") expanded
void __cdecl store_winword(const char *format, const Tm *tm, char **out, size_t *count, const BYTE *lc)
{
    using Expand = void(__cdecl *)(char, const Tm *, char **, size_t *, const BYTE *);
    const Expand expand = code<Expand>(0x5d2992);
    while (*format && *count) {
        g(glob_addr(NO_LEAD_ZEROS)) = 0;
        const char c = *format;
        const char *p = format + 1;
        int repeat = 1;
        while (*p == c) {
            repeat++;
            p++;
        }
        char spec = 0;
        switch (c) {
        case 'M':
            switch (repeat) {
            case 1: g(NO_LEAD_ZEROS) = 1; spec = 'm'; break;
            case 2: spec = 'm'; break;
            case 3: spec = 'b'; break;
            case 4: spec = 'B'; break;
            }
            break;
        case 'd':
            switch (repeat) {
            case 1: g(NO_LEAD_ZEROS) = 1; spec = 'd'; break;
            case 2: spec = 'd'; break;
            case 3: spec = 'a'; break;
            case 4: spec = 'A'; break;
            }
            break;
        case 'y':
            if (repeat == 2)
                spec = 'y';
            else if (repeat == 4)
                spec = 'Y';
            break;
        case 'h':
            if (repeat == 1) {
                g(glob_addr(NO_LEAD_ZEROS)) = 1;
                spec = 'I';
            } else if (repeat == 2)
                spec = 'I';
            break;
        case 'H':
            if (repeat == 1) {
                g(glob_addr(NO_LEAD_ZEROS)) = 1;
                spec = 'H';
            } else if (repeat == 2)
                spec = 'H';
            break;
        case 'm':
            if (repeat == 1) {
                g(glob_addr(NO_LEAD_ZEROS)) = 1;
                spec = 'M';
            } else if (repeat == 2)
                spec = 'M';
            break;
        case 's':
            if (repeat == 1) {
                g(glob_addr(NO_LEAD_ZEROS)) = 1;
                spec = 'S';
            } else if (repeat == 2)
                spec = 'S';
            break;
        case 'A': case 'a':
            // the whole rest compared, not a prefix
            if (!exe_stricmp(format, glob<const char *>(TXT_AMPM)))
                p = format + 5;
            else if (!exe_stricmp(format, glob<const char *>(TXT_AP)))
                p = format + 3;
            spec = 'p';
            break;
        case 't': {   // the AM/PM designator, repeat characters of it
            const char *s = *(const char *const *)(lc + (tm->hour > 11 ? 0x9c : 0x98));
            for (int n = repeat; n > 0 && *count; n--) {
                if (isleadbyte_((BYTE)*s) && *count > 1) {
                    *(*out)++ = *s++;
                    --*count;
                }
                *(*out)++ = *s++;
                --*count;
            }
            format = p;
            continue;
        }
        case '\'':   // a quoted literal (an even number of quotes: nothing)
            format += repeat;
            if (repeat & 1) {
                while (*format && *count) {
                    if (*format == '\'') {
                        format++;
                        break;
                    }
                    if (isleadbyte_((BYTE)*format) && *count > 1) {
                        *(*out)++ = *format++;
                        --*count;
                    }
                    *(*out)++ = *format++;
                    --*count;
                }
            }
            continue;
        }
        if (spec) {
            expand(spec, tm, out, count, lc);
            format = p;
            continue;
        }
        // any other character: copied (one at a time; the run is scanned again)
        if (isleadbyte_((BYTE)c)) {   // without the room check of the other copies
            *(*out)++ = c;
            --*count;
            format++;
        }
        *(*out)++ = *format++;
        --*count;
    }
}

// _expandtime 0x5d2992: one conversion of strftime
void __cdecl expandtime(char specifier, const Tm *tm, char **out, size_t *count, const BYTE *lc)
{
    using Num = void(__cdecl *)(int, unsigned, char **, size_t *);
    using Str = void(__cdecl *)(const char *, char **, size_t *);
    using Word = void(__cdecl *)(const char *, const Tm *, char **, size_t *, const BYTE *);
    const Num num = code<Num>(0x5d2da8);
    const Str str = code<Str>(0x5d2d81);
    const Word word = code<Word>(0x5d2e60);
    const auto lcs = [lc](int off) { return *(const char *const *)(lc + off); };
    int &alt = g(glob_addr(ALTERNATE_FORM)), &nolead = g(glob_addr(NO_LEAD_ZEROS));
    switch (specifier) {
    case 'a': str(lcs(tm->wday * 4), out, count); break;
    case 'A': str(lcs(0x1c + tm->wday * 4), out, count); break;
    case 'b': str(lcs(0x38 + tm->mon * 4), out, count); break;
    case 'B': str(lcs(0x68 + tm->mon * 4), out, count); break;
    case 'c':
        if (alt) {
            alt = 0;
            word(lcs(0xa4), tm, out, count, lc);
        } else
            word(lcs(0xa0), tm, out, count, lc);
        if (*count) {
            *(*out)++ = ' ';
            --*count;
            word(lcs(0xa8), tm, out, count, lc);
        }
        break;
    case 'd': nolead = alt; num(tm->mday, 2, out, count); break;
    case 'H': nolead = alt; num(tm->hour, 2, out, count); break;
    case 'I': {
        nolead = alt;
        const int h = tm->hour % 12;
        num(h ? h : 12, 2, out, count);
        break;
    }
    case 'j': nolead = alt; num(tm->yday + 1, 3, out, count); break;
    case 'm': nolead = alt; num(tm->mon + 1, 2, out, count); break;
    case 'M': nolead = alt; num(tm->min, 2, out, count); break;
    case 'p': str(lcs(tm->hour > 11 ? 0x9c : 0x98), out, count); break;
    case 'S': nolead = alt; num(tm->sec, 2, out, count); break;
    case 'U':
    case 'W': {
        nolead = alt;
        const int wdaynum = specifier == 'U' ? tm->wday : (tm->wday ? tm->wday - 1 : 6);
        int n;
        if (tm->yday < wdaynum)
            n = 0;
        else {
            n = tm->yday / 7;
            if (tm->yday % 7 >= wdaynum)
                n++;
        }
        num(n, 2, out, count);
        break;
    }
    case 'w': nolead = alt; num(tm->wday, 1, out, count); break;
    case 'x':
        if (alt) {
            alt = 0;
            word(lcs(0xa4), tm, out, count, lc);
        } else
            word(lcs(0xa0), tm, out, count, lc);
        break;
    case 'X': alt = 0; word(lcs(0xa8), tm, out, count, lc); break;
    case 'y': nolead = alt; num(tm->year % 100, 2, out, count); break;
    case 'Y': nolead = alt; num((tm->year / 100 + 19) * 100 + tm->year % 100, 4, out, count); break;
    case 'Z':
    case 'z':
        fn<void(__cdecl *)()>(0x5d8fc2)();
        str(glob<char **>(TZNAME)[tm->isdst != 0], out, count);
        break;
    case '%':
        *(*out)++ = '%';
        --*count;
        break;
    }
}

// _Strftime 0x5d2898: under the locale's read guard (or its lock while setlocale runs)
size_t __cdecl strftime_lc(char *s, size_t maxsize, const char *format, const Tm *tm, const BYTE *lc)
{
    size_t left = maxsize;
    auto inc = api<long(WINAPI *)(volatile long *)>(iat::InterlockedIncrement);
    auto dec = api<long(WINAPI *)(volatile long *)>(iat::InterlockedDecrement);
    volatile long *guard = glob<volatile long *>(UNGUARDED_READLC);
    inc(guard);
    bool locked;
    if (*glob<int *>(SETLC_ACTIVE)) {
        dec(guard);
        exe_lock(SETLOCALE_LOCK_);
        locked = true;
    } else
        locked = false;
    if (!lc)
        lc = *glob<const BYTE **>(LC_TIME_CURR);
    using Expand = void(__cdecl *)(char, const Tm *, char **, size_t *, const BYTE *);
    while (left > 0) {
        const char c = *format;
        if (!c)
            break;
        if (c == '%') {
            g(glob_addr(ALTERNATE_FORM)) = 0;
            format++;
            if (*format == '#') {
                g(glob_addr(ALTERNATE_FORM)) = 1;
                format++;
            }
            fn<Expand>(0x5d2992)(*format, tm, &s, &left, lc);
            format++;
        } else {
            if (isleadbyte_((BYTE)c) && left > 1) {
                *s++ = *format++;
                left--;
            }
            *s++ = *format++;
            left--;
        }
    }
    if (locked)
        exe_unlock(SETLOCALE_LOCK_);
    else
        dec(guard);
    if (left > 0) {
        *s = 0;
        return maxsize - left;
    }
    return 0;
}

// strftime 0x5d287d: in the current locale
size_t __cdecl strftime_(char *s, size_t maxsize, const char *format, const Tm *tm)
{
    return fn<size_t(__cdecl *)(char *, size_t, const char *, const Tm *, const BYTE *)>(0x5d2898)(s, maxsize, format, tm, nullptr);
}

// ---- the environment ---------------------------------------------------------------------------------------

// __crtCompareStringA 0x5ddd54 is in crt_sehc.cpp (its __try around _alloca)
inline int crt_compare_string(DWORD lcid, DWORD flags, const char *a, int na, const char *b, int nb, int cp)
{
    return fn<int(__cdecl *)(DWORD, DWORD, const char *, int, const char *, int, int)>(0x5ddd54)(lcid, flags, a, na, b, nb, cp);
}

// __mbsnbicoll 0x5ddac1: case-blind comparison of n bytes in the multibyte code page's locale; -1/0/1, or
// 0x7fffffff when CompareString fails
int __cdecl mbsnbicoll(const char *s1, const char *s2, size_t n)
{
    if (!n)
        return 0;
    const int r = crt_compare_string(*glob<DWORD *>(MBLCID), 1, s1, (int)n, s2, (int)n, *glob<int *>(MBCODEPAGE));
    if (!r)
        return 0x7fffffff;
    return r - 2;
}

// _mbschr 0x5de242: strchr, by characters in a multibyte code page
char *__cdecl mbschr(const char *s, unsigned c)
{
    if (!*glob<int *>(ISMBCODEPAGE))
        return exe_strchr(s, (int)c);
    exe_lock(MB_CP_LOCK);
    WORD ch;
    for (;; s++) {
        ch = (BYTE)*s;
        if (!ch)
            break;
        if (glob<BYTE *>(MBCTYPE)[ch + 1] & 4) {   // _M1: lead byte
            const BYTE next = (BYTE)s[1];
            s++;
            if (!next) {
                exe_unlock(MB_CP_LOCK);
                return nullptr;
            }
            if (c == ((unsigned)ch << 8 | next)) {
                exe_unlock(MB_CP_LOCK);
                return (char *)s - 1;
            }
        } else if (c == ch)
            break;
    }
    exe_unlock(MB_CP_LOCK);
    return c == ch ? (char *)s : nullptr;
}

// getenv 0x5dca1c: the value of name= in _environ (made from the wide environment when only that exists)
char *__cdecl getenv_(const char *option)
{
    if (!g(glob_addr(ENV_INITIALIZED)))
        return nullptr;
    char **search = *glob<char ***>(ENVIRON);
    if (!search) {
        if (!*glob<char ***>(WENVIRON))
            return nullptr;
        if (fn<int(__cdecl *)()>(0x5ddb00)())
            return nullptr;
        search = *glob<char ***>(ENVIRON);
        if (!search)
            return nullptr;
    }
    if (!option)
        return nullptr;
    const size_t length = exe_strlen(option);
    using Coll = int(__cdecl *)(const char *, const char *, size_t);
    for (; *search; search++)
        if (exe_strlen(*search) > length && (*search)[length] == '=' && !fn<Coll>(0x5ddac1)(*search, option, length))
            return *search + length + 1;
    return nullptr;
}

// findenv 0x5de183: the index of name (len bytes) in _environ, or minus the number of entries
int __cdecl findenv(const char *name, int len)
{
    char **env;
    using Coll = int(__cdecl *)(const char *, const char *, size_t);
    for (env = *glob<char ***>(ENVIRON); *env; env++)
        if (!fn<Coll>(0x5ddac1)(name, *env, (size_t)len) && ((*env)[len] == '=' || !(*env)[len]))
            return (int)(env - *glob<char ***>(ENVIRON));
    return -(int)(env - *glob<char ***>(ENVIRON));
}

// copy_environ 0x5de1db: a copy of an environment table with copies of its strings
char **__cdecl copy_environ(char **oldenviron)
{
    if (!oldenviron)
        return nullptr;
    int cnt = 0;
    for (char **p = oldenviron; *p; p++)
        cnt++;
    char **newenv = (char **)exe_malloc((size_t)(cnt + 1) * sizeof(char *));
    if (!newenv)
        exe_amsg_exit(9);   // _RT_SPACEENV
    char **q = newenv;
    for (char **p = oldenviron; *p; p++)
        *q++ = exe_strdup(*p);
    *q = nullptr;
    return newenv;
}

// __crtsetenv 0x5ddffc: "name=value" into _environ (the table copied first while it is the initial one),
// "name=" removes it; with primary also SetEnvironmentVariableA. 0, or -1 on an error.
int __cdecl crtsetenv(char *option, int primary)
{
    if (!option)
        return -1;
    char *equal = fn<char *(__cdecl *)(const char *, unsigned)>(0x5de242)(option, '=');
    if (!equal || option == equal)
        return -1;
    const int remove = equal[1] == 0;
    char **&environ_ = *glob<char ***>(ENVIRON);
    char **env = environ_;
    if (env == *glob<char ***>(INITENV)) {
        env = fn<char **(__cdecl *)(char **)>(0x5de1db)(env);
        environ_ = env;
    }
    if (!env) {
        if (primary && *glob<char ***>(WENVIRON)) {
            if (fn<int(__cdecl *)()>(0x5ddb00)())
                return -1;
        } else {
            if (remove)
                return 0;
            if (!(environ_ = (char **)exe_malloc(sizeof(char *))))
                return -1;
            *environ_ = nullptr;
            if (!*glob<char ***>(WENVIRON)) {
                char **w = (char **)exe_malloc(sizeof(char *));
                *glob<char ***>(WENVIRON) = w;
                if (!w)
                    return -1;
                *w = nullptr;
            }
        }
    }
    env = environ_;
    int ix = fn<int(__cdecl *)(const char *, int)>(0x5de183)(option, (int)(equal - option));
    if (ix >= 0 && *env) {
        if (remove) {
            exe_free(env[ix]);
            for (; env[ix]; ix++)
                env[ix] = env[ix + 1];
            if (char **e = (char **)exe_realloc(env, (size_t)ix * sizeof(char *)))
                environ_ = e;
        } else
            env[ix] = option;
    } else {
        if (remove)
            return 0;
        if (ix < 0)
            ix = -ix;
        char **e = (char **)exe_realloc(env, (size_t)(ix + 2) * sizeof(char *));
        if (!e)
            return -1;
        e[ix] = option;
        e[ix + 1] = nullptr;
        environ_ = e;
    }
    if (primary) {
        char *name = (char *)exe_malloc(exe_strlen(option) + 2);
        if (name) {
            exe_strcpy(name, option);
            char *value = name + (equal - option);
            *value++ = 0;
            api<BOOL(WINAPI *)(const char *, const char *)>(iatx::SetEnvironmentVariableA)(name, remove ? nullptr : value);
            exe_free(name);
        }
    }
    return 0;
}

// __wtomb_environ 0x5ddb00: the wide environment converted into _environ entry by entry; 0, or -1
int __cdecl wtomb_environ()
{
    using Wctmb = int(WINAPI *)(unsigned, DWORD, const WORD *, int, char *, int, const char *, int *);
    const Wctmb wctmb = api<Wctmb>(iatx::WideCharToMultiByte);
    for (WORD **w = *glob<WORD ***>(WENVIRON); *w; w++) {
        const int size = wctmb(1, 0, *w, -1, nullptr, 0, nullptr, nullptr);
        if (!size)
            return -1;
        char *env = (char *)exe_malloc((size_t)size);
        if (!env || !wctmb(1, 0, *w, -1, env, size, nullptr, nullptr))
            return -1;
        fn<int(__cdecl *)(char *, int)>(0x5ddffc)(env, 0);
    }
    return 0;
}

// strncnt 0x5ddfd1: the length of s, at most count (count when no 0 among the first count bytes)
int __cdecl strncnt(const char *s, int count)
{
    const char *p = s;
    for (int n = count; n && *p; n--)
        p++;
    if (*p)
        return count;
    return (int)(p - s);
}

}  // namespace zax::crt

namespace zax {

using namespace crt;

ZAX_HOOKS(crt_time)
{
    const Hook list[] = {
        {0x5d8ff0, (void *)&tzset_lk, "_tzset_lk", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0x53, 0x56, 0x57}},
        {0x5d8fc2, (void *)&tzset_, "__tzset", 7, {0x83, 0x3d, 0x80, 0x5a, 0x71, 0x00, 0x00, 0x75}},
        {0x5d9444, (void *)&cvtdate, "cvtdate", 7, {0x55, 0x8b, 0xec, 0x83, 0x7d, 0x0c, 0x01, 0x53}},
        {0x5d9298, (void *)&isindst_lk, "_isindst_lk", 5, {0x53, 0x56, 0x57, 0x33, 0xff, 0x39, 0x3d, 0x24}},
        {0x5d9277, (void *)&isindst, "_isindst", 8, {0x56, 0x6a, 0x0b, 0xe8, 0x86, 0xb0, 0xff, 0xff}, 4},
        {0x5d9584, (void *)&loctotime_t, "__loctotime_t", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x24, 0x53, 0x8b}},
        {0x5d20af, (void *)&time_, "time", 9, {0x55, 0x8b, 0xec, 0x81, 0xec, 0xcc, 0x00, 0x00}},
        {0x5d8eb8, (void *)&gmtime_, "gmtime", 5, {0x8b, 0x44, 0x24, 0x04, 0x53, 0x56, 0x57, 0x8b}},
        {0x5d312f, (void *)&localtime_, "localtime", 7, {0x55, 0x8b, 0xec, 0x56, 0x8b, 0x75, 0x08, 0x83}},
        {0x5daab5, (void *)&store_dt, "store_dt", 5, {0x8b, 0x44, 0x24, 0x08, 0x56, 0x6a, 0x0a, 0x99}},
        {0x5da9eb, (void *)&asctime_, "asctime", 9, {0x53, 0x55, 0x56, 0x57, 0xe8, 0x34, 0xa5, 0xff}, 5},
        {0x5d2864, (void *)&ctime_, "ctime", 9, {0xff, 0x74, 0x24, 0x04, 0xe8, 0xc2, 0x08, 0x00}, 5},
        {0x5d2d81, (void *)&store_str, "_store_str", 5, {0x8b, 0x4c, 0x24, 0x0c, 0x56, 0x57, 0x83, 0x39}},
        {0x5d2e17, (void *)&store_number, "_store_number", 5, {0x55, 0x8b, 0xec, 0x53, 0x56, 0x8b, 0x75, 0x10}},
        {0x5d2da8, (void *)&store_num, "_store_num", 6, {0x55, 0x8b, 0xec, 0x51, 0x33, 0xc0, 0x57, 0x39}},
        {0x5d2e60, (void *)&store_winword, "_store_winword", 5, {0x55, 0x8b, 0xec, 0x51, 0x53, 0x8b, 0x5d, 0x08}},
        {0x5d2992, (void *)&expandtime, "_expandtime", 7, {0x55, 0x8b, 0xec, 0x0f, 0xbe, 0x45, 0x08, 0x53}},
        {0x5d2898, (void *)&strftime_lc, "_Strftime", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0x8b, 0x45, 0x0c}},
        {0x5d287d, (void *)&strftime_, "strftime", 6, {0x6a, 0x00, 0xff, 0x74, 0x24, 0x14, 0xff, 0x74}},
        {0x5ddac1, (void *)&mbsnbicoll, "__mbsnbicoll", 7, {0x55, 0x8b, 0xec, 0x83, 0x7d, 0x10, 0x00, 0x75}},
        {0x5de242, (void *)&mbschr, "_mbschr", 10, {0x55, 0x8b, 0xec, 0x83, 0x3d, 0xcc, 0x61, 0x71}},
        {0x5dca1c, (void *)&getenv_, "getenv", 7, {0x83, 0x3d, 0x50, 0x75, 0x71, 0x00, 0x00, 0x53}},
        {0x5de183, (void *)&findenv, "findenv", 7, {0x56, 0x8b, 0x35, 0x9c, 0x58, 0x71, 0x00, 0x57}},
        {0x5de1db, (void *)&copy_environ, "copy_environ", 5, {0x57, 0x8b, 0x7c, 0x24, 0x08, 0x33, 0xc9, 0x85}},
        {0x5ddffc, (void *)&crtsetenv, "__crtsetenv", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0x53, 0x56, 0x57}},
        {0x5ddb00, (void *)&wtomb_environ, "__wtomb_environ", 10, {0x51, 0x53, 0x55, 0x56, 0x8b, 0x35, 0xa4, 0x58}},
        {0x5ddfd1, (void *)&strncnt, "strncnt", 8, {0x8b, 0x54, 0x24, 0x08, 0x8b, 0x44, 0x24, 0x04}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
