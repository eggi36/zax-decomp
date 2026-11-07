/* libvorbis 1.0 beta 4 floor0.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libvorbis-1.0beta4/lib/floor0.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(floor0_copy_info);
ZAXLIB_EXPORT(floor0_free_info);
ZAXLIB_EXPORT(floor0_free_look);
ZAXLIB_EXPORT(floor0_pack);
ZAXLIB_EXPORT(floor0_unpack);
ZAXLIB_EXPORT(floor0_look);
ZAXLIB_EXPORT(_curve_to_lpc);
ZAXLIB_EXPORT(floor0_forward);
ZAXLIB_EXPORT(_f0_fit);
ZAXLIB_EXPORT(floor0_inverse);
