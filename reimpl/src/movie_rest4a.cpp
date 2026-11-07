// The movie object and the parts of the Bink wrapper that do not call Bink themselves, the application's
// movie player and its frame counter (phase 4, rendering; docs/DECOMPILATION.md 9, "Phase 4 remains, band A").
//
// The movie (0x48 bytes, vtable 0x5eece0) [confirmed: Movie_NewFrame 0x431ac0, its destructors]: a CFrame
// that Bink decodes into, +0x40 the Bink system's movie index (-1 none), +0x44 "keep showing" (IsDone and
// ShowFrame then ignore the end), +0x45 the index is open.
// The Bink system (the global 0x62eaf0, vtable 0x5eede0, BinkSystem_Get) [confirmed: code]: +8 an array of
// 0x18-byte records {Bink handle, +4, +8 the frame shown, ..., +0x14 byte 1}, +0xc their count. AddMovie
// takes the first record with no handle or grows the array by one (the old block is only freed when the
// count was not 0). The functions that call binkw32.dll (open, close, decode, wait, pause, the summary, the
// system's destructor) and Bink's file callbacks (they call Sleep, BinkService and RADTimerRead) stay for
// phase 5a.
// BinkIo_Open (stdcall io, name, flags): the 200-byte callback block cleared, the file (flag 0x800000: a
// stream from Stream_New with +0x70 its slot 6; else, without flag 0x4000000, File_Open with mode 4) behind a
// FileBuf of 0x400 bytes at +0x48, the six callbacks at +0.
// App_PlayMovie (thiscall app, name, screen image, restore): unless /SkipMovies, the keys cleared, the
// display set to 640 x 480 x 16 (0x590ce0), "Movies/<name>.bik" played into a movie frame centred on the
// screen until it ends or a key is pressed (pausing while the application is inactive, app +0x60); a movie
// that does not open shows "Interface/<name>.bmp" through the screen image when one is given (0x6c02d4 = 1).
// App_DrawFrame (the application's frame end): the frame rate counters and, with the counter on (app +0x1b)
// and a font (+0x30), the line "U:%3.1f D:%3.1f P:%3.1f A:%3.1f FPS" at the screen's top centre on a black
// box, red/yellow when the rate falls below 30/20 (the thresholds 0x6bfe80/0x6bff98 set on first use), then
// the page flip when +0x29 is set.
#include "reimpl.h"
#include "exe_base.h"

namespace zax {

namespace {

inline BYTE *bp_(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
template <typename T = DWORD> inline T &f(void *o, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(o) + off); }

constexpr DWORD MOVIE_VTABLE = 0x5eece0, BINK_SYSTEM = 0x62eaf0;

inline void frame_ctor(void *m) { fn<void *(THISCALL *)(void *)>(0x4a8a90)(m); }
inline DWORD frame_dtor(void *m) { return fn<DWORD(THISCALL *)(void *)>(0x4a8b00)(m); }
inline void *bink_system() { return fn<void *(__cdecl *)()>(0x431f10)(); }
inline DWORD close_movie(void *s, DWORD id) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x432080)(s, id); }
inline DWORD at_end(void *s, DWORD id) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x432370)(s, id); }

// The close shared by Close and the destructors: eax -1 without a movie, the index when it was not open,
// else CloseMovie's
DWORD close_inner(BYTE *m)
{
    const DWORD id = f(m, 0x40);
    if (id == 0xffffffff)
        return id;
    DWORD r = id;
    if (m[0x45]) {
        r = close_movie(bink_system(), id);
        m[0x45] = 0;
    }
    f(m, 0x40) = 0xffffffff;
    return r;
}

BYTE *THISCALL Movie_NewFrame(BYTE *m)
{
    frame_ctor(m);
    f(m, 0) = vt(MOVIE_VTABLE);
    m[0x44] = 0;
    m[0x45] = 0;
    f(m, 0x40) = 0xffffffff;
    return m;
}

BYTE *THISCALL BinkMovie_DeletingDtor(BYTE *m, DWORD flags)
{
    f(m, 0) = vt(MOVIE_VTABLE);
    close_inner(m);
    frame_dtor(m);
    if (flags & 1)
        fn<void(__fastcall *)(void *, DWORD)>(0x54d130)(m, 0x48);
    return m;
}
DWORD THISCALL BinkMovie_Dtor(BYTE *m)
{
    f(m, 0) = vt(MOVIE_VTABLE);
    close_inner(m);
    return frame_dtor(m);
}
DWORD THISCALL BinkMovie_Close(BYTE *m) { return close_inner(m); }

DWORD THISCALL BinkMovie_ShowFrame(BYTE *m)
{
    if (f(m, 0x40) == 0xffffffff)
        return 0xffffffff;   // the original returns the caller's eax here (callers ignore it)
    void *s = bink_system();
    if (!m[0x44]) {
        const DWORD r = at_end(s, f(m, 0x40));
        if (r & 0xff)
            return r;
    }
    const DWORD w = fn<DWORD(THISCALL *)(void *, DWORD)>(0x4320d0)(bink_system(), f(m, 0x40));
    if (!(w & 0xff))
        return w;
    const DWORD bits = vcall<DWORD>(m, 29);
    const DWORD height = vcall<DWORD>(m, 39);
    DWORD pitch = vcall<DWORD>(m, 45);
    pitch = (pitch * vcall<DWORD>(m, 29)) >> 3;
    const DWORD pixels = vcall<DWORD>(m, 31);
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x432110)(bink_system(), f(m, 0x40), pixels, pitch,
                                                                                                   height, 0, 0, bits);
}

DWORD THISCALL BinkMovie_Open(BYTE *m, const char *name)
{
    const DWORD id = fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x431fd0)(bink_system(), name, 0);
    if (id == 0xffffffff)
        return 0xffffff00;
    DWORD summary[4];
    fn<void(THISCALL *)(void *, DWORD, DWORD *)>(0x432320)(bink_system(), id, summary);
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(m, summary[0], summary[1], 0x10);
    const DWORD r = close_inner(m);
    f(m, 0x40) = id;
    m[0x45] = 1;
    return (r & 0xffffff00) | 1;
}

DWORD THISCALL BinkMovie_Pause(BYTE *m, DWORD pause)
{
    const DWORD id = f(m, 0x40);
    if (id == 0xffffffff)
        return id;
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4323a0)(bink_system(), id, pause);
}

DWORD THISCALL BinkMovie_IsDone(BYTE *m)
{
    const DWORD id = f(m, 0x40);
    if (id == 0xffffffff)
        return 0xffffff01;
    if (m[0x44])
        return 0;
    return at_end(bink_system(), id) & 0xff ? 1 : 0;
}

BYTE *THISCALL BinkSystem_DeletingDtor(BYTE *s, DWORD flags)
{
    fn<void(THISCALL *)(void *)>(0x431e50)(s);
    if (flags & 1)
        heap_free(s);
    return s;
}

DWORD __cdecl BinkSystem_Get() { return glob_addr(BINK_SYSTEM); }

DWORD THISCALL BinkSystem_AddMovie(BYTE *s, DWORD handle)
{
    const int n = (int)f(s, 0xc);
    int i = 0;
    if (n > 0) {
        BYTE *r = bp_(f(s, 8));
        while (f(r, 0)) {
            i++;
            r += 0x18;
            if (i >= n)
                break;
        }
    }
    if (i == n) {
        BYTE *a = static_cast<BYTE *>(heap_alloc((DWORD)(n * 3 + 3) * 8));
        if (f(s, 0xc)) {
            memcpy(a, bp_(f(s, 8)), f(s, 0xc) * 0x18);
            heap_free(bp_(f(s, 8)));
        }
        f(s, 8) = (DWORD)(uintptr_t)a;
        f(s, 0xc) += 1;
    }
    BYTE *r = bp_(f(s, 8)) + i * 0x18;
    f(r, 0) = handle;
    f(r, 8) = 0;
    r[0x14] = 1;
    f(r, 4) = 0;
    return (DWORD)i;
}

// "At end": the record's frame equals the handle's frame count (+8); eax keeps the record's frame above al
DWORD THISCALL BinkMovie_AtEnd(BYTE *s, DWORD id)
{
    BYTE *r = bp_(f(s, 8)) + id * 0x18;
    const DWORD frame = f(r, 8);
    return (frame & 0xffffff00) | (frame == f(bp_(f(r, 0)), 8) ? 1 : 0);
}

DWORD STDCALL BinkIo_Open(DWORD *io, const char *name, DWORD flags)
{
    for (int i = 0; i < 0x32; i++)
        io[i] = 0;
    BYTE *stream;
    if (flags & 0x800000) {
        stream = fn<BYTE *(__fastcall *)(const char *)>(0x4a2c20)(name);
        io[0x70 / 4] = vcall<DWORD>(stream, 6);
    } else {
        if (flags & 0x4000000)
            return 0;
        stream = fn<BYTE *(__fastcall *)(const char *, DWORD)>(0x4a2440)(name, 4);
    }
    if (!stream)
        return 0;
    BYTE *fb = static_cast<BYTE *>(pool_alloc(0x1c));
    if (fb)
        fb = fn<BYTE *(THISCALL *)(void *)>(0x4a1780)(fb);
    fn<void(THISCALL *)(void *, void *, DWORD)>(0x4a1970)(fb, stream, 0x400);
    io[0x48 / 4] = (DWORD)(uintptr_t)fb;
    io[0] = fn_addr(0x4323d0);
    io[1] = fn_addr(0x432480);
    io[2] = fn_addr(0x432770);
    io[3] = fn_addr(0x4327e0);
    io[4] = fn_addr(0x4328d0);
    io[5] = fn_addr(0x432870);
    return 1;
}

// ---- The application ---------------------------------------------------------------------------------------

inline void keys_clear_all() { fn<void(THISCALL *)(void *, DWORD)>(0x4e27a0)(glob<void *>(0x6c1050), 0); }
inline void set_display(DWORD bits, DWORD w, DWORD h) { fn<void(__fastcall *)(DWORD, DWORD, DWORD)>(0x590ce0)(bits, w, h); }
inline void zs_ctor(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
inline void zs_dtor(void *z) { fn<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }
inline const char *zs_c_str(void *z) { return fn<const char *(THISCALL *)(void *)>(0x4e13a0)(z); }
inline void zs_format(void *z, DWORD fmt, const char *arg) { fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, at<const char *>(fmt), arg); }

void THISCALL App_PlayMovie(BYTE *app, const char *name, BYTE *screen_image, BYTE restore)
{
    if (fn<BYTE(__fastcall *)(const char *)>(0x4d22c0)(glob<const char *>(0x6155a8)))
        return;
    keys_clear_all();
    set_display(0x10, 0x280, 0x1e0);
    DWORD zs;
    zs_ctor(&zs);
    zs_format(&zs, glob_addr(0x615598), name);
    alignas(4) BYTE movie[0x48];
    Movie_NewFrame(movie);
    if (BinkMovie_Open(movie, zs_c_str(&zs)) & 0xff) {
        BYTE active = app[0x60];
        fn<void(THISCALL *)(void *)>(0x5694b0)(bp_(f(app, 4)));
        if (!(BinkMovie_IsDone(movie) & 0xff)) {
            for (;;) {
                if (active != app[0x60]) {
                    BinkMovie_Pause(movie, app[0x60] ? 0 : 1);
                    active = app[0x60];
                }
                if (active) {
                    BinkMovie_ShowFrame(movie);
                    BYTE *screen = bp_(f(app, 4));
                    const DWORD h = f<WORD>(movie, 0x14), w = f<WORD>(movie, 0x12);
                    const DWORD y = (vcall<DWORD>(screen, 39) - h) >> 1;
                    const DWORD x = (vcall<DWORD>(screen, 45) - w) >> 1;
                    fn<void(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x4b3280)(bp_(f(app, 4)), movie, x, y, glob_addr(0x6bb4e0));
                    fn<void(THISCALL *)(void *)>(0x5693a0)(bp_(f(app, 4)));
                    if (fn<DWORD(THISCALL *)(void *)>(0x4e2900)(glob<void *>(0x6c1050))) {
                        keys_clear_all();
                        break;
                    }
                }
                fn<void(__cdecl *)()>(0x5998f0)();
                if (BinkMovie_IsDone(movie) & 0xff)
                    break;
            }
        }
        BinkMovie_Close(movie);
        *glob<BYTE *>(0x6c02d4) = 0;
    } else if (screen_image) {
        zs_format(&zs, glob_addr(0x615584), name);
        fn<void(THISCALL *)(void *, const char *, DWORD)>(0x578cb0)(screen_image, zs_c_str(&zs), 0);
        *glob<BYTE *>(0x6c02d4) = 1;
    }
    if (restore)
        set_display(0, 0, 0);
    BinkMovie_Dtor(movie);
    zs_dtor(&zs);
}

inline long double rate_sample(DWORD counter) { return fn<long double(THISCALL *)(void *)>(0x4a8290)(at<void *>(counter)); }
inline DWORD seq_height(void *font) { return fn<DWORD(THISCALL *)(void *)>(0x4a6be0)(font); }
inline void color_set(void *c, DWORD r, DWORD g, DWORD b, DWORD a)
{
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f080)(c, r, g, b, a);
}

DWORD THISCALL App_DrawFrame(BYTE *app)
{
    if (*glob<DWORD *>(0x6c02dc) < f(app, 0x38)) {
        fn<void(THISCALL *)(void *)>(0x4a83c0)(glob<void *>(0x6bfe88));
        *glob<DWORD *>(0x6c02dc) = f(app, 0x38);
    }
    f(app, 0x3c) += 1;
    if (f(app, 0x30)) {
        volatile float u_ = (float)rate_sample(glob_addr(0x6c00b0));   // stored as floats (fstp dword), as the original
        volatile float d_ = (float)rate_sample(glob_addr(0x6bfe88));
        volatile float p_ = (float)rate_sample(glob_addr(0x6bffa0));
        const float u = u_, d = d_, p = p_;
        BYTE &guard = *glob<BYTE *>(0x6bff94);
        if (!(guard & 1)) {
            *glob<DWORD *>(0x6bfe80) = 0x41f00000;   // 30.0f
            guard |= 1;
        }
        if (!(guard & 2)) {
            *glob<DWORD *>(0x6bff98) = 0x41a00000;   // 20.0f
            guard |= 2;
        }
        if (app[0x1b]) {
            BYTE &level = *glob<BYTE *>(0x6be7dc);
            const BYTE saved = level;
            level = 0;
            const long double pl = p;
            if (pl > *glob<float *>(0x5ebed0) || pl < *glob<float *>(0x5ebed0)) {   // not equal and not NaN
                char buf[0x100];
                fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(buf, glob<const char *>(0x615680), (double)*glob<float *>(0x6c02d0),
                                                                        (double)d, (double)u, (double)p);
                const DWORD line = f(app, 0x20);
                f(app, 0x20) = line + 1;
                BYTE *font = bp_(f(app, 0x30));
                const DWORD top = seq_height(font) * line;
                fn<DWORD(THISCALL *)(void *, const char *, int)>(0x4a7310)(font, buf, -1);
                DWORD half = fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a6290)(bp_(f(app, 0x30)), 0x58);
                DWORD len = 0;
                while (buf[len])
                    len++;
                half *= len;
                alignas(4) BYTE color[0x10];
                fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(color, 0, 0, 1, 0xff);
                const long double red = *glob<float *>(0x6bfe80), yellow = *glob<float *>(0x6bff98);
                if (x87_lt(u, red)) {
                    if (x87_lt(u, yellow)) {
                        const DWORD n = *glob<DWORD *>(0x6c02e4);
                        *glob<DWORD *>(0x6c02e4) = n + 1;
                        if (n & 1)
                            color_set(color, 0xff, 0xff, 0, 0xff);
                        else
                            color_set(color, 0xff, 0, 0, 0xff);
                    } else {
                        const long double k = *glob<float *>(0x5eb728);
                        const int v = x87_ftol((red - u) * k / (red - yellow));
                        color_set(color, (DWORD)v, 0, 0, 0xff);
                    }
                }
                DWORD x = vcall<DWORD>(bp_(f(app, 4)), 45);
                fn<void(THISCALL *)(void *)>(0x567bb0)(bp_(f(app, 4)));
                x >>= 1;
                half >>= 1;
                const DWORD bottom = seq_height(bp_(f(app, 0x30))) + top;
                fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, void *)>(0x4b3a70)(bp_(f(app, 4)), x - half, top, half + x, bottom, color);
                fn<void(THISCALL *)(void *, void *, DWORD, DWORD, const char *, DWORD, DWORD)>(0x4a6470)(bp_(f(app, 0x30)), bp_(f(app, 4)), x, top,
                                                                                                       buf, 1, 0x58);
                fn<void(THISCALL *)(void *)>(0x567c90)(bp_(f(app, 4)));
            }
            level = saved;
        }
    }
    f(app, 0x20) = 0;
    if (app[0x29])
        return fn<DWORD(THISCALL *)(void *)>(0x5693a0)(bp_(f(app, 4)));
    return 0;
}

}  // namespace

ZAX_HOOKS(movie_rest4a)
{
    const Hook list[] = {
#include "movie_rest4a_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
