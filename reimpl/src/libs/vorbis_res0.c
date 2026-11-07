/* libvorbis 1.0 beta 4 res0.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libvorbis-1.0beta4/lib/res0.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(res0_copy_info);
ZAXLIB_EXPORT(res0_free_info);
ZAXLIB_EXPORT(res0_free_look);
ZAXLIB_EXPORT(res0_pack);
ZAXLIB_EXPORT(res0_unpack);
ZAXLIB_EXPORT(res0_look);
ZAXLIB_EXPORT(res0_forward);
ZAXLIB_EXPORT(_testhack);
ZAXLIB_EXPORT(_encodepart);
ZAXLIB_EXPORT(res0_inverse);
ZAXLIB_EXPORT(_decodepart);
