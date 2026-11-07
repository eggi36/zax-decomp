/* libvorbis 1.0 beta 4 vorbisfile.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libvorbis-1.0beta4/lib/vorbisfile.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(ov_clear);
ZAXLIB_EXPORT(ov_open_callbacks);
ZAXLIB_EXPORT(_open_seekable);
ZAXLIB_EXPORT(_get_prev_page);
ZAXLIB_EXPORT(_seek_helper);
ZAXLIB_EXPORT(_get_next_page);
ZAXLIB_EXPORT(_get_data);
ZAXLIB_EXPORT(_bisect_forward_serialno);
ZAXLIB_EXPORT(_fetch_headers);
ZAXLIB_EXPORT(_prefetch_all_headers);
ZAXLIB_EXPORT(_open_nonseekable);
ZAXLIB_EXPORT(_make_decode_ready);
ZAXLIB_EXPORT(ov_pcm_total);
ZAXLIB_EXPORT(ov_time_total);
ZAXLIB_EXPORT(ov_raw_seek);
ZAXLIB_EXPORT(_decode_clear);
ZAXLIB_EXPORT(_process_packet);
ZAXLIB_EXPORT(ov_pcm_seek_page);
ZAXLIB_EXPORT(ov_pcm_seek);
ZAXLIB_EXPORT(ov_time_seek);
ZAXLIB_EXPORT(ov_time_tell);
ZAXLIB_EXPORT(ov_info);
ZAXLIB_EXPORT(ov_read);
