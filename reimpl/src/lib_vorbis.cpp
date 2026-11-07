// libogg, libvorbis and vorbisfile 1.0 beta 4 (phase 5b): Zax.exe's statically linked copies
// 0x5c3bf0-0x5c5280 (vorbisfile), 0x5de2f0-0x5e96f0 and 0x5e98b0-0x5ea000 (libogg's framing.c and
// bitwise.c, libvorbis; 181 functions, docs/DECOMPILATION.md 10) replaced by the original release built
// from source: reimpl/third_party/libogg-1.0beta4 and libvorbis-1.0beta4, compiled unchanged through the
// wrappers in libs/ (ogg_*.c, vorbis_*.c) with the C runtime of libc/. The game decodes its music,
// voice-overs and quips through vorbisfile (OggStream_* 0x525380-0x525ba0); the encoder parts are in the
// exe only because the backends' export bundles point to them, and are hooked as well.
// Checks: checks_vorbis.cpp.
#include "reimpl.h"
#include "vorbis_exports.inc"

// _chkstk for the libraries' alloca and big frames (the rest of the DLL is built without stack probes):
// the exe's own _chkstk 0x5d2080 (MSVC 6: eax = size, probes page by page, moves esp), as the exe's
// libraries call it. A push and ret jumps there without touching a register.
extern "C" __attribute__((naked)) void _chkstk()
{
    __asm__("pushl " ZAX_ASM_CODE_ATT(0x5d2080) "\n\tret");
}

namespace zax {

ZAX_HOOKS(vorbis)
{
    const Hook list[] = {
#include "vorbis_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
