// The DirectDraw surface handling of the screen frame CScreenFrame (phase 5a, group G1, docs/DECOMPILATION.md 10):
// the mode set 0x567f20 with its EnumDisplayModes callback 0x567d60 and the mode list's qsort comparator 0x568600,
// the release 0x5678c0, Lock 0x567bb0, Unlock 0x567c90 and the flip 0x5693a0. The same DirectDraw calls as the
// original, in the same order and with the same arguments; every exe function they reach is called through its exe
// address (the frame's drawing logic is screen_frame.cpp, phase 4 band C).
//
// The DirectDraw objects [confirmed: the functions below]: 0x713334 the IDirectDraw (DirectDrawCreate through the
// import thunk 0x5caa82), 0x713348 the primary surface, 0x713338 the back buffer (full screen: the attached back
// buffer of a flipping chain; windowed: an offscreen surface of the window's size), 0x71333c a third surface (only
// tested for a lost surface by Lock; nothing in the exe creates it), 0x713340 the clipper (windowed), 0x713344 the
// palette (8 bits); 0x713284 the back buffer is a surface of its own (released separately), 0x713318 the back
// buffer is locked, 0x7132a8 Lock's DDSURFACEDESC (0x6c bytes; +0x24 the pixels), 0x6210c0 full screen, 0x713e18
// the game window. The frame (vtable 0x5ff360): +0xc the pixels while locked, +0x12/+0x14 the size (words), +0x40
// the pages (back buffers + 2), +0x44 the back buffers, +0x48 the page shown, +0x4c the number of modes, +0x54 the
// modes (width, height, depth; at most 10), +0x50 the primary surface is in video memory, +0xcc "Display is RGB
// 565". COM slots used: IDirectDraw 2 Release, 4 CreateClipper, 5 CreatePalette, 6 CreateSurface, 8
// EnumDisplayModes, 20 SetCooperativeLevel, 21 SetDisplayMode; IDirectDrawSurface 2 Release, 5 Blt, 11 Flip, 12
// GetAttachedSurface, 24 IsLost, 25 Lock, 27 Restore, 28 SetClipper, 31 SetPalette, 32 Unlock; IDirectDrawClipper 8
// SetHWnd.
#include "ui_screens.h"
#include "x87.h"

namespace zax::ddraw {

using namespace ui;

constexpr DWORD DDRAW = 0x713334, BACK = 0x713338, THIRD = 0x71333c, CLIPPER = 0x713340, PALETTE = 0x713344,
                PRIMARY = 0x713348, OWN_BACK = 0x713284, LOCKED = 0x713318, LOCK_DESC = 0x7132a8, FULL_SCREEN = 0x6210c0,
                WINDOW = 0x713e18, INI = 0x6bd330, INI_FILE = 0x6bd430;
constexpr DWORD DDERR_SURFACELOST = 0x887601c2;

// A COM method (stdcall, the object first)
template <typename... A> inline DWORD com(DWORD obj, int slot, A... a)
{
    using F = DWORD(__stdcall *)(DWORD, A...);
    return (*reinterpret_cast<F *const *>(obj))[slot](obj, a...);
}
inline BYTE *screen() { return bp(m(bp(gd(glob_addr(G_REPORTER))), 4)); }

// ---- Release --------------------------------------------------------------------------------------------------------

// Every page cleared (the screen frame's slot 25) and flipped when DirectDraw, the back buffer, the reporter and its
// screen are there; then the palette, the clipper, the back buffer (only when it is a surface of its own), the primary
// surface and the DirectDraw object released in that order and their globals cleared
void __cdecl Release()   // 0x5678c0
{
    const DWORD rep = gd(glob_addr(G_REPORTER));
    if (gd(glob_addr(DDRAW)) && gd(glob_addr(BACK)) && rep && m(bp(rep), 4) && m(bp(m(bp(rep), 4)), 0x44) + 1 != 0) {
        BYTE *s = bp(m(bp(rep), 4));
        DWORD i = 0;
        do {
            vc(s, 25);
            fn<DWORD(THISCALL *)(BYTE *)>(0x5693a0)(screen());
            i++;
            s = screen();
        } while (i < m(s, 0x44) + 1);
    }
    if (gd(glob_addr(PALETTE))) {
        com(gd(glob_addr(PALETTE)), 2);
        gd(glob_addr(PALETTE)) = 0;
    }
    if (gd(glob_addr(CLIPPER))) {
        com(gd(glob_addr(CLIPPER)), 2);
        gd(glob_addr(CLIPPER)) = 0;
    }
    if (gd(glob_addr(BACK)) && gb(glob_addr(OWN_BACK))) {
        com(gd(glob_addr(BACK)), 2);
        gd(glob_addr(BACK)) = 0;
    }
    if (gd(glob_addr(PRIMARY))) {
        com(gd(glob_addr(PRIMARY)), 2);
        gd(glob_addr(PRIMARY)) = 0;
    }
    if (gd(glob_addr(DDRAW))) {
        com(gd(glob_addr(DDRAW)), 2);
        gd(glob_addr(DDRAW)) = 0;
    }
}

// ---- Lock and Unlock ------------------------------------------------------------------------------------------------

// Nothing when the frame's slot 31 says so (pixels already there); else up to 100 tries: a lost primary surface, back
// buffer or third surface restored (a failed Restore counts as a try), then the back buffer locked with DDLOCK_WAIT
// into 0x7132a8; on success the pixels into +0xc and 0x713318 set
void THISCALL Lock(BYTE *self)   // 0x567bb0
{
    if (vc(self, 31))
        return;
    for (DWORD tries = 1; tries <= 100; tries++) {
        if (com(gd(glob_addr(PRIMARY)), 24) == DDERR_SURFACELOST && com(gd(glob_addr(PRIMARY)), 27))
            continue;
        if (com(gd(glob_addr(BACK)), 24) == DDERR_SURFACELOST && com(gd(glob_addr(BACK)), 27))
            continue;
        if (gd(glob_addr(THIRD)) && com(gd(glob_addr(THIRD)), 24) == DDERR_SURFACELOST && com(gd(glob_addr(THIRD)), 27))
            continue;
        memset(glob<void *>(LOCK_DESC), 0, 0x6c);
        gd(glob_addr(LOCK_DESC)) = 0x6c;
        if (com(gd(glob_addr(BACK)), 25, 0, glob_addr(LOCK_DESC), 1, 0))
            continue;
        gd(glob_addr(LOCKED)) = 1;
        m(self, 0xc) = gd(glob_addr(LOCK_DESC) + 0x24);
        return;
    }
}

void THISCALL Unlock(BYTE *self)   // 0x567c90
{
    if (!gd(glob_addr(LOCKED)))
        return;
    com(gd(glob_addr(BACK)), 32, 0);
    m(self, 0xc) = 0;
    gd(glob_addr(LOCKED)) = 0;
}

// ---- The flip -------------------------------------------------------------------------------------------------------

// Full screen: nothing while locked, else Flip(NULL, DDFLIP_WAIT) of the primary surface. Windowed (with a primary
// surface): the window's client rectangle (right at least 64 when below 128, bottom at least 64) moved to the screen
// (ClientToScreen of 0, 0; OffsetRect) and the back buffer's (0, 0, width, height) blitted there (DDBLT_WAIT, the Blt's
// answer not looked at). After a flip the page shown (+0x48) goes on and wraps after the back buffers (+0x44); al 1.
// al 0 when nothing was flipped.
BYTE THISCALL Flip(BYTE *self)   // 0x5693a0
{
    if (gb(glob_addr(FULL_SCREEN))) {
        if (gd(glob_addr(LOCKED)) || com(gd(glob_addr(PRIMARY)), 11, 0, 1))
            return 0;
    } else {
        if (!gd(glob_addr(PRIMARY)))
            return 0;
        int from[4];
        from[0] = 0;
        from[1] = 0;
        from[2] = (int)vc(self, 45);
        from[3] = (int)vc(self, 39);
        int to[4];
        imp<DWORD(WINAPI *)(DWORD, int *)>(0x5ea238)(gd(glob_addr(WINDOW)), to);
        if (to[2] < 0x80)
            to[2] = 0x40;
        if (to[3] < 0x40)
            to[3] = 0x40;
        int origin[2] = {0, 0};
        imp<DWORD(WINAPI *)(DWORD, int *)>(0x5ea240)(gd(glob_addr(WINDOW)), origin);
        imp<DWORD(WINAPI *)(int *, int, int)>(0x5ea23c)(to, origin[0], origin[1]);
        com(gd(glob_addr(PRIMARY)), 5, to, gd(glob_addr(BACK)), from, 0x1000000, 0);
    }
    const DWORD page = m(self, 0x48) + 1;
    m(self, 0x48) = page;
    if (page > m(self, 0x44))
        m(self, 0x48) = 0;
    return 1;
}

// ---- The mode list --------------------------------------------------------------------------------------------------

// Experiment "higher resolutions" (docs/DECOMPILATION.md, "Higher resolutions: experiment"; not the original's
// behaviour): [Reimpl] ExtraModes=1024x768,1280x960 in Zax-Reimpl.ini, or the environment variable
// ZAX_REIMPL_EXTRA_MODES that Zax-Launcher.exe /Resolution sets, lets these sizes into the mode list as well (without
// the 4:3 test, they are listed explicitly). Empty (the default): the callback is exactly the original's. Read once
// when the hook table is built (before the game runs); with a list every enumerated mode is logged.
constexpr int MAX_EXTRA = 8;
DWORD g_extra[MAX_EXTRA][2];
int g_extra_count;

WINIMPORT DWORD WINAPI GetEnvironmentVariableA(const char *name, char *out, DWORD size);

void parse_extra_modes(const char *p)
{
    while (*p && g_extra_count < MAX_EXTRA) {
        DWORD w = 0, h = 0;
        while (*p == ' ' || *p == ',' || *p == ';')
            p++;
        while (*p >= '0' && *p <= '9')
            w = w * 10 + (DWORD)(*p++ - '0');
        if (*p == 'x' || *p == 'X')
            for (p++; *p >= '0' && *p <= '9'; p++)
                h = h * 10 + (DWORD)(*p - '0');
        if (w >= 320 && w <= 4096 && h >= 200 && h <= 4096) {
            g_extra[g_extra_count][0] = w;
            g_extra[g_extra_count][1] = h;
            g_extra_count++;
            log("ExtraModes: %u x %u allowed (experiment, not the original)", w, h);
        }
        while (*p && *p != ',' && *p != ';')
            p++;
    }
}

void read_extra_modes()
{
    g_extra_count = 0;
    char ini[MAX_PATH + 32], text[256];
    GetModuleFileNameA(nullptr, ini, MAX_PATH);
    for (int n = lstrlenA(ini); n > 0 && ini[n - 1] != '\\'; n--)
        ini[n - 1] = 0;
    lstrcatA(ini, "Zax-Reimpl.ini");
    GetPrivateProfileStringA("Reimpl", "ExtraModes", "", text, sizeof text, ini);
    parse_extra_modes(text);
    const DWORD n = GetEnvironmentVariableA("ZAX_REIMPL_EXTRA_MODES", text, sizeof text);
    if (n && n < sizeof text)
        parse_extra_modes(text);
}

bool extra_mode(DWORD w, DWORD h)
{
    for (int i = 0; i < g_extra_count; i++)
        if (g_extra[i][0] == w && g_extra[i][1] == h)
            return true;
    return false;
}

// EnumDisplayModes' callback (the DDSURFACEDESC, the wanted depth): a mode of that depth whose width / height lies
// strictly between 1.2333 and 1.4333 (the x87 compares of the original, the ratio rounded to a float) and is 640 x 480
// or 800 x 600 goes into the screen's list while it has fewer than 10, with the depth (16 bits with a green mask other
// than 0x7e0: 15); always 1 (go on)
DWORD __stdcall EnumModesCallback(const BYTE *desc, const DWORD *depth)   // 0x567d60
{
    const DWORD h = m(const_cast<BYTE *>(desc), 8), w = m(const_cast<BYTE *>(desc), 0xc);
    const float ratio = (float)((long double)(unsigned long long)w / (long double)(int)h);
    if (g_extra_count)
        log("EnumModes: %u x %u, %u bits (wanted %u)", w, h, m(const_cast<BYTE *>(desc), 0x54), *depth);
    if (m(const_cast<BYTE *>(desc), 0x54) != *depth)
        return 1;
    if (g_extra_count && extra_mode(w, h))
        goto add;   // the experiment: a listed size skips the 4:3 and size tests
    if (!x87_lt((long double)*glob<float *>(0x5ff470), (long double)ratio))
        return 1;
    if (!((long double)*glob<float *>(0x5ff46c) > (long double)ratio))
        return 1;
    if (!((w == 0x280 && h == 0x1e0) || (w == 0x320 && h == 0x258)))
        return 1;
add:
    BYTE *s = screen();
    if (m(s, 0x4c) >= 10)
        return 1;
    m(s, 0x54 + m(s, 0x4c) * 0xc) = w;
    s = screen();
    m(s, 0x58 + m(s, 0x4c) * 0xc) = m(const_cast<BYTE *>(desc), 8);
    const DWORD bits = m(const_cast<BYTE *>(desc), 0x54);
    s = screen();
    if (bits == 0x10)
        m(s, 0x5c + m(s, 0x4c) * 0xc) = m(const_cast<BYTE *>(desc), 0x5c) == 0x7e0 ? 0x10 : 0xf;
    else
        m(s, 0x5c + m(s, 0x4c) * 0xc) = bits;
    m(screen(), 0x4c) += 1;
    return 1;
}

// qsort's comparator of the mode list: by width, then height (unsigned); -1 when smaller, else 1 (never 0)
int __cdecl CompareModes(const DWORD *a, const DWORD *b)   // 0x568600
{
    if (a[0] < b[0])
        return -1;
    if (a[0] == b[0] && a[1] < b[1])
        return -1;
    return 1;
}

// ---- The mode set ---------------------------------------------------------------------------------------------------

inline void error_exit(DWORD text)
{
    fn<void(__fastcall *)(DWORD, DWORD)>(0x4d24c0)(glob_addr(0x6212dc), text);   // "BAD SCREEN MODE"
    fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
}
inline DWORD create_surface(BYTE *desc, DWORD out) { return com(gd(glob_addr(DDRAW)), 6, desc, out, 0); }

// (&width, &height, depth, wanted depth): al 1 when the mode is set. Nothing to do (al 1) when the screen has that
// depth, width and height and DirectDraw and the back buffer exist. Else everything released; windowed, the
// desktop's depth (0x4d2530) replaces the depth and must fit the wanted one (8: not 24 or 32; 16: 16; 24: 24 or 32;
// else "BAD SCREEN MODE" and exit(-1)); DirectDraw created when missing, the cooperative level set (full screen
// 0x51, windowed 8), the modes of that depth enumerated (with the refresh rates first, without when that fails),
// sorted (qsort, 0x568600), duplicates dropped (0x567e90) and the nearest mode looked up (0x567cd0). Full screen: the
// display mode set and a flipping primary surface with two back buffers, else one, else one outside video memory
// (+0x50 whether in video memory), its back buffer attached; windowed: a primary surface and an offscreen back buffer
// of the size, a clipper on the window. A palette of a grey ramp (entry i: i, i, i; PC_RESERVED) created (0x713344 cleared when
// that fails) and set on both surfaces; the back buffer locked and unlocked for its pitch; the frame's size, depth and
// pitch (16 bits: "Display is RGB 565" of Zax.CFG, by default whether the mode's depth is not 15, written back),
// the palette holder's kind and the palette set (0x568870); every page cleared and flipped. Any failure: everything
// released, the width and height 0, al 0.
BYTE THISCALL SetMode(BYTE *self, DWORD *w, DWORD *h, DWORD depth, DWORD want)   // 0x567f20
{
    alignas(4) BYTE desc[0x6c];
    BYTE entries[0x400];
    mb(self, 0x50) = 0;
    if (depth == vc(self, 29) && *w == vc(self, 45) && *h == vc(self, 39) && gd(glob_addr(DDRAW)) && gd(glob_addr(BACK)))
        return 1;
    fn<void(__cdecl *)()>(0x5678c0)();
    if (!gb(glob_addr(FULL_SCREEN))) {
        depth = fn<DWORD(__cdecl *)()>(0x4d2530)();
        if (want == 8) {
            if (!gb(glob_addr(FULL_SCREEN)) && (depth == 0x18 || depth == 0x20))
                error_exit(glob_addr(0x6211c0));
        } else if (want == 0x10) {
            if (depth != 0x10)
                error_exit(glob_addr(0x621268));
        } else if (want == 0x18) {
            if (depth != 0x18 && depth != 0x20)
                error_exit(glob_addr(0x6212ec));
        }
    }
    if (!gd(glob_addr(DDRAW)) && import_thunk<DWORD(WINAPI *)(void *, DWORD, void *)>(0x5caa82, 0x5ea010)(nullptr, glob_addr(DDRAW), nullptr))
        goto fail;
    {
        DWORD result = com(gd(glob_addr(DDRAW)), 20, gd(glob_addr(WINDOW)), gb(glob_addr(FULL_SCREEN)) ? 0x51u : 8u);
        m(self, 0x4c) = 0;
        if (com(gd(glob_addr(DDRAW)), 8, 2, 0, &depth, fn_addr(0x567d60)))
            com(gd(glob_addr(DDRAW)), 8, 0, 0, &depth, fn_addr(0x567d60));
        fn<void(__cdecl *)(void *, DWORD, DWORD, DWORD)>(0x5d097c)(self + 0x54, m(self, 0x4c), 0xc, fn_addr(0x568600));
        fn<void(__cdecl *)()>(0x567e90)();
        fn<DWORD(THISCALL *)(BYTE *, DWORD *, DWORD *)>(0x567cd0)(self, w, h);
        if (!*w || !*h || result)
            goto fail;
        if (gb(glob_addr(FULL_SCREEN)) && com(gd(glob_addr(DDRAW)), 21, *w, *h, depth))
            goto fail;
        memset(desc, 0, sizeof desc);
        m(desc, 0) = 0x6c;
        if (gb(glob_addr(FULL_SCREEN))) {
            m(self, 0x44) = 2;
            m(desc, 4) = 0x21;
            m(desc, 0x14) = 2;
            m(desc, 0x68) = 0x4218;
            if (!create_surface(desc, glob_addr(PRIMARY))) {
                mb(self, 0x50) = 1;
            } else {
                m(self, 0x44) = 1;
                m(desc, 0x14) = 1;
                if (!create_surface(desc, glob_addr(PRIMARY))) {
                    mb(self, 0x50) = 1;
                } else {
                    m(desc, 0x68) &= 0xffffbfff;
                    if (create_surface(desc, glob_addr(PRIMARY)))
                        goto fail;
                    mb(self, 0x50) = 0;
                }
            }
            DWORD caps = 4;
            if (com(gd(glob_addr(PRIMARY)), 12, &caps, glob_addr(BACK)))
                goto fail;
            gb(glob_addr(OWN_BACK)) = 1;
        } else {
            m(self, 0x44) = 0;
            m(desc, 4) = 1;
            m(desc, 0x68) = 0x200;
            if (create_surface(desc, glob_addr(PRIMARY)))
                goto fail;
            memset(desc, 0, sizeof desc);
            m(desc, 0) = 0x6c;
            m(desc, 4) = 7;
            m(desc, 8) = *h;
            m(desc, 0xc) = *w;
            m(desc, 0x68) = 0x40;
            if (create_surface(desc, glob_addr(BACK)))
                goto fail;
            gb(glob_addr(OWN_BACK)) = 1;
            if (com(gd(glob_addr(DDRAW)), 4, 0, glob_addr(CLIPPER), 0) || com(gd(glob_addr(CLIPPER)), 8, 0, gd(glob_addr(WINDOW))) || result)
                goto fail;
            com(gd(glob_addr(PRIMARY)), 28, gd(glob_addr(CLIPPER)));
        }
        for (int i = 0; i < 0x100; i++) {   // a grey ramp, PC_RESERVED
            entries[i * 4] = (BYTE)i;
            entries[i * 4 + 1] = (BYTE)i;
            entries[i * 4 + 2] = (BYTE)i;
            entries[i * 4 + 3] = 1;
        }
        if (com(gd(glob_addr(DDRAW)), 5, 4, entries, glob_addr(PALETTE), 0))
            gd(glob_addr(PALETTE)) = 0;
        com(gd(glob_addr(PRIMARY)), 31, gd(glob_addr(PALETTE)));
        com(gd(glob_addr(BACK)), 31, gd(glob_addr(PALETTE)));
        memset(desc, 0, sizeof desc);
        m(desc, 0) = 0x6c;
        com(gd(glob_addr(BACK)), 25, 0, desc, 1, 0);
        com(gd(glob_addr(BACK)), 32, 0);
        m<WORD>(self, 0x12) = (WORD)*w;
        m<WORD>(self, 0x14) = (WORD)*h;
        using SetDepth = void(THISCALL *)(BYTE *, DWORD);
        fn<SetDepth>(0x4aa240)(self, depth);
        const int pitch = mi(desc, 0x10);
        int pixels;
        if (depth == 8) {
            pixels = pitch;
        } else if (depth == 0x10) {
            DWORD i = 0;
            while (i < m(self, 0x4c) && !(*w == m(self, 0x54 + i * 0xc) && *h == m(self, 0x58 + i * 0xc)))
                i++;
            fn<SetDepth>(0x4aa240)(self, 0x10);
            const DWORD rgb565 = m(self, 0x5c + i * 0xc) == 0xf ? 0 : 1;
            using GetInt = DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD);
            mb(self, 0xcc) = fn<GetInt>(0x4a4670)(glob_addr(INI), glob_addr(INI_FILE), glob_addr(0x6211ac), rgb565) != 0;
            fn<void(__fastcall *)(DWORD, DWORD, DWORD, DWORD)>(0x4a4690)(glob_addr(INI), glob_addr(INI_FILE), glob_addr(0x6211ac), mb(self, 0xcc));
            pixels = pitch / 2;
        } else if (depth == 0x18) {
            pixels = pitch / 3;   // (the original also looks the mode up here and does not use it)
        } else {
            pixels = pitch / 4;   // (likewise)
        }
        fn<void(THISCALL *)(BYTE *, int, DWORD)>(0x4aa2c0)(self, pixels, 1);
        fn<void(THISCALL *)(DWORD, DWORD)>(0x5268f0)(vc(self, 58), 1);
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x568870)(self, vc(self, 58), 1);
        m(self, 0x40) = m(self, 0x44) + 2;
        for (DWORD i = 0; i < m(self, 0x40); i++) {   // the pages cleared (black in 8 bits) and flipped
            int r[4];
            vc(self, 36, r);
            if (gd(glob_addr(0x633388))) {
                alignas(4) BYTE black[0x10];
                fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(black, 0, 0, 1, 0xff);
                vc(self, 28, r, black);
            } else {
                vc(self, 24, r);
            }
            fn<DWORD(THISCALL *)(BYTE *)>(0x5693a0)(self);
        }
        return 1;
    }
fail:
    fn<void(__cdecl *)()>(0x5678c0)();
    *w = 0;
    *h = 0;
    return 0;
}

}  // namespace zax::ddraw

namespace zax {

using namespace ddraw;

ZAX_HOOKS(display_ddraw)
{
    if (out)
        read_extra_modes();
    const Hook list[] = {
        {0x567f20, ptr(&SetMode), "ScreenFrame_SetMode", 6, {0x81, 0xec, 0x90, 0x04, 0x00, 0x00, 0x53, 0x55}},
        {0x5678c0, ptr(&Release), "ScreenFrame_Release", 5, {0xa1, 0x34, 0x33, 0x71, 0x00, 0x53, 0x33, 0xdb}},
        {0x567bb0, ptr(&Lock), "ScreenFrame_Lock", 5, {0x53, 0x55, 0x8b, 0xe9, 0x57, 0x8b, 0x45, 0x00}},
        {0x567c90, ptr(&Unlock), "ScreenFrame_Unlock", 5, {0xa1, 0x18, 0x33, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5693a0, ptr(&Flip), "ScreenFrame_Flip", 5, {0xa0, 0xc0, 0x10, 0x62, 0x00, 0x83, 0xec, 0x28}},
        {0x567d60, ptr(&EnumModesCallback), "ScreenFrame_EnumModesCallback", 7, {0x83, 0xec, 0x08, 0x8b, 0x4c, 0x24, 0x0c, 0x33}},
        {0x568600, ptr(&CompareModes), "ScreenFrame_CompareModes", 5, {0x8b, 0x54, 0x24, 0x04, 0x56, 0x8b, 0x74, 0x24}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
