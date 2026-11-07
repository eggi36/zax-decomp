/* libvorbis 1.0 beta 4 info.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libvorbis-1.0beta4/lib/info.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(vorbis_comment_init);
ZAXLIB_EXPORT(vorbis_comment_clear);
ZAXLIB_EXPORT(vorbis_info_init);
ZAXLIB_EXPORT(vorbis_info_clear);
ZAXLIB_EXPORT(vorbis_synthesis_headerin);
ZAXLIB_EXPORT(_v_readstring);
ZAXLIB_EXPORT(_vorbis_unpack_info);
ZAXLIB_EXPORT(_vorbis_unpack_comment);
ZAXLIB_EXPORT(_vorbis_unpack_books);
