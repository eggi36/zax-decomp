/* libvorbis 1.0 beta 4 lsp.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libvorbis-1.0beta4/lib/lsp.c"
#pragma clang attribute pop

/* comp 0x5e9430 is __cdecl in the exe (its arguments from the stack, unlike the rest of the /Gr build;
 * it is passed to qsort): the hook points to a cdecl entry into the source's comp. */
static int __cdecl comp_cdecl(const void *a, const void *b) { return comp(a, b); }

ZAXLIB_EXPORT(vorbis_lsp_to_curve);
ZAXLIB_EXPORT(vorbis_lpc_to_lsp);
ZAXLIB_EXPORT(cheby);
ZAXLIB_EXPORT(comp_cdecl);
ZAXLIB_EXPORT(Laguerre_With_Deflation);
ZAXLIB_EXPORT(Newton_Raphson);
