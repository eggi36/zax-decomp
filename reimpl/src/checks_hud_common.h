// Shared by the HUD checks (checks_hud_render.cpp, checks_hud_details.cpp): run_sides of the frame check harness
// with the blend modes' FindBlitter cache set alike before each side.
//
// Every static blend mode (CBlendMode, DECOMPILATION.md 9 "The blitter range") keeps a one-entry cache of the
// last blitter it found (+0x2c the key, +0x30 the function). A side's blits leave it at their last key, so the
// next side would find it in another state and ask the frames for their flags once more (a frame call more in
// the log, the same pixels). The cache is put back to its state before the case at the start of each side.
#pragma once
#include "checks_frame_common.h"

namespace zax::hudcheck {

constexpr DWORD BLEND_MODES[15] = {0x6bb4e0, 0x6bcc68, 0x6b9728, 0x6b9c60, 0x6b85f0, 0x6bb520, 0x6b9ca0, 0x6bc618,
                                   0x6bc4e0, 0x6bc4a8, 0x6bbaa0, 0x6bb968, 0x6bbad8, 0x6bc328, 0x6bc468};
struct BlendCaches {
    DWORD v[15][2];
    void save()
    {
        for (int i = 0; i < 15; i++)
            memcpy(v[i], at<void *>(BLEND_MODES[i] + 0x2c), 8);
    }
    void restore() const
    {
        for (int i = 0; i < 15; i++)
            memcpy(at<void *>(BLEND_MODES[i] + 0x2c), v[i], 8);
    }
};
inline BlendCaches g_blend_caches;

template <typename F> inline bool run_sides(Tally &t, framec::ModuleHooks &hooks, const char *what, F call)
{
    g_blend_caches.save();
    const bool same = framec::run_sides(t, hooks, what, [&](int side) {
        g_blend_caches.restore();
        return call(side);
    });
    g_blend_caches.restore();
    return same;
}

}  // namespace zax::hudcheck
