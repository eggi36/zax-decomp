/* zlib 1.1.3 inftrees.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/zlib-1.1.3/inftrees.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(inflate_trees_bits);
ZAXLIB_EXPORT(huft_build);
ZAXLIB_EXPORT(inflate_trees_dynamic);
ZAXLIB_EXPORT(inflate_trees_fixed);
