/* libvorbis 1.0 beta 4 registry.c, built unchanged (reimpl/third_party/README.md). No function of it is in the
 * exe (the backends' tables only). */
#include "libs.h"
#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)
#include "../../third_party/libvorbis-1.0beta4/lib/registry.c"
#pragma clang attribute pop
