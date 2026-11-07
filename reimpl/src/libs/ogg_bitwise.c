/* libogg 1.0 beta 4 bitwise.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libogg-1.0beta4/src/bitwise.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(oggpack_writeinit);
ZAXLIB_EXPORT(oggpack_writeclear);
ZAXLIB_EXPORT(oggpack_readinit);
ZAXLIB_EXPORT(oggpack_write);
ZAXLIB_EXPORT(oggpack_look);
ZAXLIB_EXPORT(oggpack_adv);
ZAXLIB_EXPORT(oggpack_read);
ZAXLIB_EXPORT(oggpack_read1);
