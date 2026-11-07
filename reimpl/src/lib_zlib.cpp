// zlib 1.1.3 (phase 5b): Zax.exe's statically linked copy 0x5caa90-0x5d0312 (56 functions, docs/
// DECOMPILATION.md 10) replaced by the original release built from source: reimpl/third_party/zlib-1.1.3,
// compiled unchanged through the wrappers in libs/ (zlib_*.c) with the C runtime of libc/ (the exe's own
// heap and runtime functions). The game calls compress2 and uncompress only (Pack 0x5c3b40, Unpack
// 0x5c3ba0); every function of the copy is hooked, so the game's packing runs entirely on the source.
// Checks: checks_zlib.cpp.
#include "reimpl.h"
#include "zlib_exports.inc"

namespace zax {

ZAX_HOOKS(zlib)
{
    const Hook list[] = {
#include "zlib_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
