/* libvorbis 1.0 beta 4 psy.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libvorbis-1.0beta4/lib/psy.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(_vi_psy_free);
ZAXLIB_EXPORT(_vp_psy_init);
ZAXLIB_EXPORT(set_curve);
ZAXLIB_EXPORT(min_curve);
ZAXLIB_EXPORT(setup_curve);
ZAXLIB_EXPORT(max_curve);
ZAXLIB_EXPORT(attenuate_curve);
ZAXLIB_EXPORT(interp_curve);
ZAXLIB_EXPORT(_vp_psy_clear);
ZAXLIB_EXPORT(_vp_compute_mask);
ZAXLIB_EXPORT(seed_loop);
ZAXLIB_EXPORT(seed_curve);
ZAXLIB_EXPORT(seed_peak);
ZAXLIB_EXPORT(bound_loop);
ZAXLIB_EXPORT(max_seeds);
ZAXLIB_EXPORT(seed_chase);
ZAXLIB_EXPORT(bark_noise_median);
ZAXLIB_EXPORT(_vp_apply_floor);
