/* libvorbis 1.0 beta 4 sharedbook.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libvorbis-1.0beta4/lib/sharedbook.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(_ilog);
ZAXLIB_EXPORT(_float32_unpack);
ZAXLIB_EXPORT(_make_words);
ZAXLIB_EXPORT(_make_decode_tree);
ZAXLIB_EXPORT(_book_maptype1_quantvals);
ZAXLIB_EXPORT(_book_unquantize);
ZAXLIB_EXPORT(vorbis_staticbook_clear);
ZAXLIB_EXPORT(vorbis_staticbook_destroy);
ZAXLIB_EXPORT(vorbis_book_clear);
ZAXLIB_EXPORT(vorbis_book_init_encode);
ZAXLIB_EXPORT(vorbis_book_init_decode);
ZAXLIB_EXPORT(_best);
ZAXLIB_EXPORT(_dist);
ZAXLIB_EXPORT(vorbis_book_besterror);
