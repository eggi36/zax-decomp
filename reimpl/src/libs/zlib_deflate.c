/* zlib 1.1.3 deflate.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/zlib-1.1.3/deflate.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(deflateInit_);
ZAXLIB_EXPORT(deflateInit2_);
ZAXLIB_EXPORT(deflateReset);
ZAXLIB_EXPORT(deflate);
ZAXLIB_EXPORT(putShortMSB);
ZAXLIB_EXPORT(flush_pending);
ZAXLIB_EXPORT(deflateEnd);
ZAXLIB_EXPORT(lm_init);
ZAXLIB_EXPORT(deflate_stored);
ZAXLIB_EXPORT(fill_window);
ZAXLIB_EXPORT(read_buf);
ZAXLIB_EXPORT(deflate_fast);
ZAXLIB_EXPORT(longest_match);
ZAXLIB_EXPORT(deflate_slow);
