/* zlib 1.1.3 trees.c, built unchanged (reimpl/third_party/README.md). Exports the addresses of the functions
 * Zax.exe has a copy of, for the hook table (libs.h). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/zlib-1.1.3/trees.c"
#pragma clang attribute pop

ZAXLIB_EXPORT(_tr_init);
ZAXLIB_EXPORT(init_block);
ZAXLIB_EXPORT(_tr_stored_block);
ZAXLIB_EXPORT(_tr_align);
ZAXLIB_EXPORT(_tr_flush_block);
ZAXLIB_EXPORT(build_tree);
ZAXLIB_EXPORT(pqdownheap);
ZAXLIB_EXPORT(gen_bitlen);
ZAXLIB_EXPORT(gen_codes);
ZAXLIB_EXPORT(build_bl_tree);
ZAXLIB_EXPORT(scan_tree);
ZAXLIB_EXPORT(send_all_trees);
ZAXLIB_EXPORT(send_tree);
ZAXLIB_EXPORT(compress_block);
ZAXLIB_EXPORT(set_data_type);
ZAXLIB_EXPORT(bi_reverse);
ZAXLIB_EXPORT(bi_flush);
ZAXLIB_EXPORT(bi_windup);
ZAXLIB_EXPORT(copy_block);
