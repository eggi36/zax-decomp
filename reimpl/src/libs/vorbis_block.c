/* libvorbis 1.0 beta 4 block.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libvorbis-1.0beta4/lib/block.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(vorbis_block_init);
ZAXLIB_EXPORT(_vorbis_block_alloc);
ZAXLIB_EXPORT(_vorbis_block_ripcord);
ZAXLIB_EXPORT(vorbis_block_clear);
ZAXLIB_EXPORT(_vds_shared_init);
ZAXLIB_EXPORT(ilog2);
ZAXLIB_EXPORT(vorbis_dsp_clear);
ZAXLIB_EXPORT(vorbis_synthesis_init);
ZAXLIB_EXPORT(vorbis_synthesis_blockin);
ZAXLIB_EXPORT(vorbis_synthesis_pcmout);
ZAXLIB_EXPORT(vorbis_synthesis_read);
