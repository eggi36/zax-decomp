// Assembly helpers of the checks (see check_util.h).
#include "check_util.h"

namespace zax {

extern "C" {
DWORD zax_escape_esp, zax_exit_code;
int __cdecl zax_call_escapable(void *fn, void *self, const char *name, int hint);
void __cdecl zax_escape_exit(int code);
}
__asm__(".text\n"
        ".globl _zax_call_escapable\n"
        "_zax_call_escapable:\n"
        "  pushl %ebp\n  pushl %ebx\n  pushl %esi\n  pushl %edi\n"
        "  movl %esp, _zax_escape_esp\n"
        "  pushl 32(%esp)\n"   // hint (return address at 16, fn 20, self 24, name 28, hint 32)
        "  pushl 32(%esp)\n"   // name
        "  movl 32(%esp), %ecx\n"
        "  call *28(%esp)\n"   // thiscall, removes its two arguments
        "  popl %edi\n  popl %esi\n  popl %ebx\n  popl %ebp\n  ret\n"
        ".globl _zax_escape_exit\n"
        "_zax_escape_exit:\n"
        "  movl 4(%esp), %eax\n  movl %eax, _zax_exit_code\n"
        "  movl _zax_escape_esp, %esp\n"
        "  movl $0x7fff0001, %eax\n"
        "  popl %edi\n  popl %esi\n  popl %ebx\n  popl %ebp\n  ret\n");

}  // namespace zax
