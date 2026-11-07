/* libvorbis 1.0 beta 4 codebook.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libvorbis-1.0beta4/lib/codebook.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(vorbis_staticbook_unpack);
ZAXLIB_EXPORT(vorbis_book_encode);
ZAXLIB_EXPORT(vorbis_book_decode);
ZAXLIB_EXPORT(vorbis_book_decodevs);
ZAXLIB_EXPORT(s_vorbis_book_decodevs);
