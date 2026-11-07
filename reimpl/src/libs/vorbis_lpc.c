/* libvorbis 1.0 beta 4 lpc.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libvorbis-1.0beta4/lib/lpc.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(vorbis_lpc_from_data);
ZAXLIB_EXPORT(vorbis_lpc_from_curve);
ZAXLIB_EXPORT(lpc_init);
ZAXLIB_EXPORT(lpc_clear);
