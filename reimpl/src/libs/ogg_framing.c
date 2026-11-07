/* libogg 1.0 beta 4 framing.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libogg-1.0beta4/src/framing.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(ogg_page_version);
ZAXLIB_EXPORT(ogg_page_continued);
ZAXLIB_EXPORT(ogg_page_bos);
ZAXLIB_EXPORT(ogg_page_eos);
ZAXLIB_EXPORT(ogg_page_granulepos);
ZAXLIB_EXPORT(ogg_page_serialno);
ZAXLIB_EXPORT(ogg_page_pageno);
ZAXLIB_EXPORT(ogg_stream_init);
ZAXLIB_EXPORT(_ogg_crc_init);
ZAXLIB_EXPORT(_ogg_crc_entry);
ZAXLIB_EXPORT(ogg_stream_clear);
ZAXLIB_EXPORT(_os_body_expand);
ZAXLIB_EXPORT(_os_lacing_expand);
ZAXLIB_EXPORT(_os_checksum);
ZAXLIB_EXPORT(ogg_sync_init);
ZAXLIB_EXPORT(ogg_sync_clear);
ZAXLIB_EXPORT(ogg_sync_buffer);
ZAXLIB_EXPORT(ogg_sync_wrote);
ZAXLIB_EXPORT(ogg_sync_pageseek);
ZAXLIB_EXPORT(ogg_stream_pagein);
ZAXLIB_EXPORT(ogg_sync_reset);
ZAXLIB_EXPORT(ogg_stream_reset);
ZAXLIB_EXPORT(ogg_stream_packetout);
