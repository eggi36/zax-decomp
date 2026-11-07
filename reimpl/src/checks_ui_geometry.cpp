// Checks of the three window toolkit helpers (ui_geometry.cpp) against the original: the CRect
// constructor on random values (result and the 16 bytes), the edge test of a resizable window on fake
// windows (random rectangles, font heights 0-60, points inside, on and beyond the border, wrapping
// extremes) and the scroll step on random and extreme values.
#include "check_util.h"

namespace zax {
namespace {

int extreme(DWORD r)
{
    static_assert(sizeof(int) == 4);
    switch (r % 8) {
    case 0: return (int)0x80000000;
    case 1: return 0x7fffffff;
    case 2: return -1;
    case 3: return 0;
    default: return (int)(random() >> (r % 24));
    }
}

struct FakeWindow {
    DWORD vftable;
    int left, top, right, bottom;
    BYTE unknown14[0x7c - 0x14];
    BYTE *font;
};
BYTE g_font[0x84];

}  // namespace

ZAX_CHECKS(ui_geometry)
{
    (void)cls;
    (void)n;
    Tally t{"ui geometry: CRect, window edge test, scroll step"};
    g_seed = 31337;
    for (int k = 0; k < 400; k++) {
        int r[2][4];
        DWORD ret[2];
        const int l = extreme(random()), tp = extreme(random()), rt = extreme(random()), b = extreme(random());
        using F = DWORD(THISCALL *)(int *, int, int, int, int);
        memset(r, 0xcc, sizeof r);
        ret[0] = original<F>(0x403240)(r[0], l, tp, rt, b) - (DWORD)(uintptr_t)r[0];
        ret[1] = at<F>(0x403240)(r[1], l, tp, rt, b) - (DWORD)(uintptr_t)r[1];
        t.check(ret[0] == ret[1] && memcmp(r[0], r[1], sizeof r[0]) == 0, "CRect");
    }
    FakeWindow w;
    memset(&w, 0, sizeof w);
    w.font = g_font;
    for (int k = 0; k < 3000; k++) {
        const DWORD h = k % 10 == 0 ? random() : random() % 61;
        memcpy(g_font + 0x80, &h, 4);
        if (k % 7 == 0) {
            w.left = extreme(random()), w.top = extreme(random()), w.right = extreme(random()), w.bottom = extreme(random());
        }
        else {
            w.left = (int)(random() % 800), w.top = (int)(random() % 600);
            w.right = w.left + (int)(random() % 400), w.bottom = w.top + (int)(random() % 300);
        }
        const int width = w.right - w.left, height = w.bottom - w.top;
        int p[2];
        p[0] = k % 9 == 0 ? extreme(random()) : (int)(random() % (DWORD)(width > 0 && width < 2000 ? width + 40 : 440)) - 20;
        p[1] = k % 11 == 0 ? extreme(random()) : (int)(random() % (DWORD)(height > 0 && height < 2000 ? height + 40 : 340)) - 20;
        compare_call<DWORD(THISCALL *)(FakeWindow *, const int *)>(t, 0x4039e0, "EdgeHit", &w, (const int *)p);
    }
    for (int k = 0; k < scaled(3000); k++) {
        const int from = k % 5 == 0 ? extreme(random()) : (int)(random() % 2000) - 1000;
        const int to = k % 6 == 0 ? extreme(random()) : (int)(random() % 2000) - 1000;
        const int a = k % 7 == 0 ? extreme(random()) : (int)(random() % 50);
        const int b = k % 8 == 0 ? extreme(random()) : (int)(random() % 300);
        compare_call<int(__fastcall *)(int, int, int, int)>(t, 0x407440, "ScrollStep", from, to, a, b);
    }
    t.report();
}

}  // namespace zax
