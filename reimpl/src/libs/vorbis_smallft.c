/* libvorbis 1.0 beta 4 smallft.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libvorbis-1.0beta4/lib/smallft.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(drft_forward);
ZAXLIB_EXPORT(drftf1);
ZAXLIB_EXPORT(dradf2);
ZAXLIB_EXPORT(dradf4);
ZAXLIB_EXPORT(dradfg);
ZAXLIB_EXPORT(drft_backward);
ZAXLIB_EXPORT(drftb1);
ZAXLIB_EXPORT(dradb2);
ZAXLIB_EXPORT(dradb3);
ZAXLIB_EXPORT(dradb4);
ZAXLIB_EXPORT(dradbg);
ZAXLIB_EXPORT(drft_init);
ZAXLIB_EXPORT(fdrffti);
ZAXLIB_EXPORT(drfti1);
ZAXLIB_EXPORT(drft_clear);
