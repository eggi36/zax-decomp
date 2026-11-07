// The helpers of the generated static initializers and atexit destructors (batch C2 of phase 5a,
// docs/DECOMPILATION.md 10; tools/static_init_gen.py): calls of exe functions by address with the
// convention the original call site uses, and the loads and stores of globals by address.
//
// These functions run inside the C runtime's initializer loop (_initterm over 0x609000-0x60a3ec) before
// WinMain, and at the exit (atexit's table) after it: they must not need anything of the DLL that is set
// up later (they use no DLL globals and do not log).
#pragma once
#include "reimpl.h"

namespace zax::sinit {

inline DWORD get32(DWORD a) { return *at<volatile DWORD *>(a); }
inline BYTE get8(DWORD a) { return *at<volatile BYTE *>(a); }
inline void put32(DWORD a, DWORD v) { *at<volatile DWORD *>(a) = v; }
inline void put8(DWORD a, BYTE v) { *at<volatile BYTE *>(a) = v; }

// The callee is a key of the address layer (reimpl.h, docs/DECOMPILATION.md 13): a reimplemented function, called
// through the function table in the standalone exe; the `_at` forms take a run-time address (a vtable slot's value).
// thiscall: `self` in ecx, the arguments on the stack, the callee pops them
template <typename... A> inline DWORD this_call(FnKey f, DWORD self, A... a)
{
    return fn<DWORD(THISCALL *)(void *, decltype(DWORD(a))...)>(f)(at<void *>(self), DWORD(a)...);
}
template <typename... A> inline DWORD this_call_at(DWORD f, DWORD self, A... a)
{
    return fn_dyn<DWORD(THISCALL *)(void *, decltype(DWORD(a))...)>(f)(at<void *>(self), DWORD(a)...);
}
// cdecl: the caller pops the arguments
template <typename... A> inline DWORD c_call(FnKey f, A... a)
{
    return fn<DWORD(__cdecl *)(decltype(DWORD(a))...)>(f)(DWORD(a)...);
}
template <typename... A> inline DWORD c_call_at(DWORD f, A... a)
{
    return fn_dyn<DWORD(__cdecl *)(decltype(DWORD(a))...)>(f)(DWORD(a)...);
}
// stdcall: the callee pops, no ecx
template <typename... A> inline DWORD std_call(FnKey f, A... a)
{
    return fn<DWORD(STDCALL *)(decltype(DWORD(a))...)>(f)(DWORD(a)...);
}
template <typename... A> inline DWORD std_call_at(DWORD f, A... a)
{
    return fn_dyn<DWORD(STDCALL *)(decltype(DWORD(a))...)>(f)(DWORD(a)...);
}

}  // namespace zax::sinit
