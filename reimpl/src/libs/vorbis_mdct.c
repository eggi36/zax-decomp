/* libvorbis 1.0 beta 4 mdct.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libvorbis-1.0beta4/lib/mdct.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(mdct_init);
ZAXLIB_EXPORT(mdct_clear);
ZAXLIB_EXPORT(mdct_backward);
ZAXLIB_EXPORT(mdct_butterfly_32);
ZAXLIB_EXPORT(mdct_butterfly_8);
ZAXLIB_EXPORT(mdct_butterfly_generic);
ZAXLIB_EXPORT(mdct_forward);
