// The wsock32 import thunks of Zax.exe 0x5ca9e0-0x5caa52 (phase 5b; docs/DECOMPILATION.md 11, "GameSpy SDK (2001)"):
// six-byte indirect jumps through the import table (wsock32 by ordinal), called only by the GameSpy SDK. Kept as
// such: no frame, every register as the caller left it, the import entry read when called (so the checks' stand-ins
// in the import table see the calls).
#include "reimpl.h"

// The import slots the jumps read, through the address layer (batch 6-10a): the address itself in the DLL (the same
// text as before), in the standalone exe the symbol tools/extract_exe_data.py defines for it (our own IAT once its
// home is `own`; as crt_seh.cpp's ZAX_ASM_DATA)
#ifdef ZAX_STANDALONE
#define ZAX_ASM_DATA(a) "_zax_data_" #a
#else
#define ZAX_ASM_DATA(a) #a
#endif

namespace zax::gamespy {

extern "C" {
void Wsock_gethostbyname();   // 0x5ca9e0: ordinal 52, import entry 0x5ea2e0
void Wsock_inet_addr();   // 0x5ca9e6: ordinal 10, import entry 0x5ea2d0
void Wsock_getsockname();   // 0x5ca9ec: ordinal 6, import entry 0x5ea2d4
void Wsock_bind();   // 0x5ca9f2: ordinal 2, import entry 0x5ea2d8
void Wsock_htons();   // 0x5ca9f8: ordinal 9, import entry 0x5ea2dc
void Wsock_socket();   // 0x5ca9fe: ordinal 23, import entry 0x5ea2a4
void Wsock_closesocket();   // 0x5caa04: ordinal 3, import entry 0x5ea2a8
void Wsock_sendto();   // 0x5caa0a: ordinal 20, import entry 0x5ea2ac
void Wsock_recvfrom();   // 0x5caa10: ordinal 17, import entry 0x5ea2b0
void Wsock_select();   // 0x5caa16: ordinal 18, import entry 0x5ea2b4
void Wsock_send();   // 0x5caa1c: ordinal 19, import entry 0x5ea2b8
void Wsock_recv();   // 0x5caa22: ordinal 16, import entry 0x5ea2bc
void Wsock_connect();   // 0x5caa28: ordinal 4, import entry 0x5ea2c0
void Wsock_ntohs();   // 0x5caa2e: ordinal 15, import entry 0x5ea2c4
void Wsock_setsockopt();   // 0x5caa34: ordinal 21, import entry 0x5ea2c8
void Wsock_WSAFDIsSet();   // 0x5caa3a: ordinal 151, import entry 0x5ea2cc
void Wsock_inet_ntoa();   // 0x5caa40: ordinal 11, import entry 0x5ea2ec
void Wsock_htonl();   // 0x5caa46: ordinal 8, import entry 0x5ea2e4
void Wsock_WSAStartup();   // 0x5caa4c: ordinal 115, import entry 0x5ea2e8
void Wsock_WSACleanup();   // 0x5caa52: ordinal 116, import entry 0x5ea2f0
}
__asm__(".text\n"
        "_Wsock_gethostbyname:\n  jmp *" ZAX_ASM_DATA(0x5ea2e0) "\n"
        "_Wsock_inet_addr:\n  jmp *" ZAX_ASM_DATA(0x5ea2d0) "\n"
        "_Wsock_getsockname:\n  jmp *" ZAX_ASM_DATA(0x5ea2d4) "\n"
        "_Wsock_bind:\n  jmp *" ZAX_ASM_DATA(0x5ea2d8) "\n"
        "_Wsock_htons:\n  jmp *" ZAX_ASM_DATA(0x5ea2dc) "\n"
        "_Wsock_socket:\n  jmp *" ZAX_ASM_DATA(0x5ea2a4) "\n"
        "_Wsock_closesocket:\n  jmp *" ZAX_ASM_DATA(0x5ea2a8) "\n"
        "_Wsock_sendto:\n  jmp *" ZAX_ASM_DATA(0x5ea2ac) "\n"
        "_Wsock_recvfrom:\n  jmp *" ZAX_ASM_DATA(0x5ea2b0) "\n"
        "_Wsock_select:\n  jmp *" ZAX_ASM_DATA(0x5ea2b4) "\n"
        "_Wsock_send:\n  jmp *" ZAX_ASM_DATA(0x5ea2b8) "\n"
        "_Wsock_recv:\n  jmp *" ZAX_ASM_DATA(0x5ea2bc) "\n"
        "_Wsock_connect:\n  jmp *" ZAX_ASM_DATA(0x5ea2c0) "\n"
        "_Wsock_ntohs:\n  jmp *" ZAX_ASM_DATA(0x5ea2c4) "\n"
        "_Wsock_setsockopt:\n  jmp *" ZAX_ASM_DATA(0x5ea2c8) "\n"
        "_Wsock_WSAFDIsSet:\n  jmp *" ZAX_ASM_DATA(0x5ea2cc) "\n"
        "_Wsock_inet_ntoa:\n  jmp *" ZAX_ASM_DATA(0x5ea2ec) "\n"
        "_Wsock_htonl:\n  jmp *" ZAX_ASM_DATA(0x5ea2e4) "\n"
        "_Wsock_WSAStartup:\n  jmp *" ZAX_ASM_DATA(0x5ea2e8) "\n"
        "_Wsock_WSACleanup:\n  jmp *" ZAX_ASM_DATA(0x5ea2f0) "\n");

}  // namespace zax::gamespy

namespace zax {

ZAX_HOOKS(gamespy_thunks)
{
    using namespace gamespy;
    const Hook list[] = {
#include "gamespy_thunks_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
