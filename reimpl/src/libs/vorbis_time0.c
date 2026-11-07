/* libvorbis 1.0 beta 4 time0.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libvorbis-1.0beta4/lib/time0.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(time0_unpack);
ZAXLIB_EXPORT(time0_look);
ZAXLIB_EXPORT(time0_pack);
ZAXLIB_EXPORT(time0_forward);
