// Batch C3 (docs/DECOMPILATION.md 10, "Batch C3"): the atexit destructors of the rendering subsystem that their
// static initializers pass to atexit (so the map counted them as callbacks): the inlined destructors of three
// resource lists (the font list 0x6be1e0, the overlay options list 0x6bf9b8, the water warp list 0x713c80) and six
// exit stubs that put the base vtable 0x5ece44 back into a global object (0x52a3f0-0x52a850). Like batch C2's
// texture lists (static_c2_manual.cpp) without the frame filter: B+0x30 the entries of 0x80 bytes (count at -4,
// the object at +0x78 deleted through its slot 1 with 1, from the last), the array freed with operator delete,
// B+0x34 cleared, the record list's Set(0, 0, 0, 0, 0), B+0x13c an object deleted through its slot 7, B+0x1c the
// resource aliases; the vtables: the list's own, then 0x5ed564 and NamedList 0x5ec7cc while the bases go.
#include "static_c2.h"

namespace zax {

using namespace sinit;

namespace {

DWORD list_exit(DWORD b, DWORD own_vtable)
{
    const DWORD data = get32(b + 0x30);
    put32(b, own_vtable);
    if (data) {
        const DWORD count = get32(data - 4);
        DWORD slot = data + (count << 7) + 0x78;
        for (DWORD i = (int)(count - 1) < 0 ? 0 : count; i != 0; i--) {   // dec; js: no entry
            const DWORD obj = get32(slot - 0x80);
            slot -= 0x80;
            if (obj) {
                at<DWORD(THISCALL *)(void *, DWORD)>(get32(get32(obj) + 4))(at<void *>(obj), 1);
                put32(slot, 0);
            }
        }
        c_call(0x5d0320 /* operator delete */, data - 4);
        put32(b + 0x30, 0);
    }
    put32(b + 0x34, 0);
    this_call(0x591cd0 /* RecordList_Set */, b, 0, 0, 0, 0, 0);
    if (const DWORD o = get32(b + 0x13c)) {
        at<DWORD(THISCALL *)(void *, DWORD)>(get32(get32(o) + 0x1c))(at<void *>(o), 1);
        put32(b + 0x13c, 0);
    }
    put32(b, vt(0x5ed564));
    const DWORD r = this_call(0x55d0c0 /* ResourceAliases_Dtor */, b + 0x1c);
    put32(b, vt(0x5ec7cc));
    return r;
}

}  // namespace

DWORD __cdecl StaticExit_FontList() { return list_exit(glob_addr(0x6be1e0), vt(0x5f4604)); }              // 0x4a7a60
DWORD __cdecl StaticExit_OverlayOptionsList() { return list_exit(glob_addr(0x6bf9b8), vt(0x5f4b80)); }    // 0x4b49a0
DWORD __cdecl StaticExit_WaterWarpList() { return list_exit(glob_addr(0x713c80), vt(0x602a88)); }        // 0x598850

// The base vtable 0x5ece44 back into a global object (eax the caller's)
void __cdecl StaticExit_52a3f0() { put32(glob_addr(0x6d04c0), vt(0x5ece44)); }   // 0x52a3f0
void __cdecl StaticExit_52a530() { put32(glob_addr(0x6d04a8), vt(0x5ece44)); }   // 0x52a530
void __cdecl StaticExit_52a670() { put32(glob_addr(0x6d0218), vt(0x5ece44)); }   // 0x52a670
void __cdecl StaticExit_52a7b0() { put32(glob_addr(0x6d0270), vt(0x5ece44)); }   // 0x52a7b0
void __cdecl StaticExit_52a800() { put32(glob_addr(0x6d03f8), vt(0x5ece44)); }   // 0x52a800
void __cdecl StaticExit_52a850() { put32(glob_addr(0x6d03e0), vt(0x5ece44)); }   // 0x52a850

ZAX_HOOKS(static_c3)
{
    const Hook list[] = {
#include "static_c3_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
