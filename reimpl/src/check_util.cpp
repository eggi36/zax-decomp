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

// ---- zax_call_stack, call_faulting (check_util.h)---------------------------------------------------------------

extern "C" {
DWORD zax_fault_esp;   // the stack pointer zax_fault_escape returns with (after the saved registers)
DWORD __cdecl zax_call_args(const void *fn, DWORD ecx, const DWORD *args, DWORD n);
void __cdecl zax_fault_escape();
}
__asm__(".text\n"
        ".globl _zax_call_stack\n"
        "_zax_call_stack:\n"   // (fn 8, ecx 12, args 16, n 20, fill 24, count 28 from ebp)
        "  pushl %ebp\n  movl %esp, %ebp\n  pushl %ebx\n  pushl %esi\n  pushl %edi\n"
        "  movl 24(%ebp), %esi\n  movl 28(%ebp), %ecx\n  xorl %eax, %eax\n  movl %esp, %edi\n"
        "  jecxz 2f\n"
        "1:\n  subl $4, %edi\n  movl (%esi), %eax\n  movl %eax, (%edi)\n  addl $4, %esi\n"
        "  decl %ecx\n  jnz 1b\n"
        "2:\n  movl 20(%ebp), %ecx\n  movl 16(%ebp), %esi\n"
        "3:\n  testl %ecx, %ecx\n  jz 4f\n  pushl -4(%esi,%ecx,4)\n  decl %ecx\n  jmp 3b\n"
        "4:\n  movl 12(%ebp), %ecx\n  calll *8(%ebp)\n"
        "  leal -12(%ebp), %esp\n  popl %edi\n  popl %esi\n  popl %ebx\n  popl %ebp\n  ret\n"
        ".globl _zax_call_args\n"
        "_zax_call_args:\n"   // (fn 8, ecx 12, args 16, n 20 from ebp); zax_fault_escape comes back here
        "  pushl %ebp\n  movl %esp, %ebp\n  pushl %ebx\n  pushl %esi\n  pushl %edi\n"
        "  movl %esp, _zax_fault_esp\n"
        "  movl 20(%ebp), %ecx\n  movl 16(%ebp), %esi\n"
        "5:\n  testl %ecx, %ecx\n  jz 6f\n  pushl -4(%esi,%ecx,4)\n  decl %ecx\n  jmp 5b\n"
        "6:\n  movl 12(%ebp), %ecx\n  calll *8(%ebp)\n"
        "7:\n  leal -12(%ebp), %esp\n  popl %edi\n  popl %esi\n  popl %ebx\n  popl %ebp\n  ret\n"
        ".globl _zax_fault_escape\n"
        "_zax_fault_escape:\n"
        "  movl _zax_fault_esp, %esp\n  leal 12(%esp), %ebp\n  movl $0x7fff0002, %eax\n  jmp 7b\n");

WINIMPORT void *WINAPI AddVectoredExceptionHandler(unsigned long first, long(WINAPI *handler)(void *info));
WINIMPORT unsigned long WINAPI RemoveVectoredExceptionHandler(void *handle);
WINIMPORT DWORD WINAPI WaitForSingleObject(HANDLE h, DWORD ms);
WINIMPORT BOOL WINAPI CloseHandle(HANDLE h);

namespace {
volatile DWORD g_fault_thread;
Fault *g_fault;
long WINAPI on_fault(void *info)
{
    DWORD *const *p = static_cast<DWORD *const *>(info);
    const DWORD *rec = p[0];
    DWORD *ctx = p[1];
    if (!g_fault_thread || GetCurrentThreadId() != g_fault_thread || rec[0] < 0xc0000000u || rec[0] == 0xc0000027 /* unwind */)
        return 0;   // EXCEPTION_CONTINUE_SEARCH
    g_fault->code = rec[0];
    g_fault->kind = rec[4] >= 1 ? rec[5] : 0xffffffffu;
    g_fault->address = rec[4] >= 2 ? rec[6] : 0;
    g_fault->at = ctx[0xb8 / 4];
    g_fault_thread = 0;
    ctx[0xb8 / 4] = (DWORD)(uintptr_t)&zax_fault_escape;   // Eip; zax_fault_escape sets esp itself
    return -1;   // EXCEPTION_CONTINUE_EXECUTION
}
struct FaultCall {
    Fault *fault;
    const void *fn;
    DWORD ecx;
    const DWORD *args;
    DWORD n, eax;
};
DWORD fault_call(FaultCall &c)
{
    memset(c.fault, 0, sizeof *c.fault);
    g_fault = c.fault;
    // the FPU as before the call: a fault after an fld leaves the value on the FPU stack (fnsave clears the FPU,
    // so it is loaded back at once)
    alignas(4) BYTE fpu[108];
    __asm__ volatile("fnsave %0\n\tfrstor %0" : "=m"(fpu) : : "memory");
    void *const veh = AddVectoredExceptionHandler(1, &on_fault);
    g_fault_thread = GetCurrentThreadId();
    c.eax = zax_call_args(c.fn, c.ecx, c.args, c.n);
    g_fault_thread = 0;
    RemoveVectoredExceptionHandler(veh);
    if (c.fault->code)
        __asm__ volatile("frstor %0" : : "m"(fpu) : "memory");
    return c.eax;
}
DWORD WINAPI fault_thread(void *p)
{
    fault_call(*static_cast<FaultCall *>(p));
    return 0;
}
}  // namespace

DWORD call_faulting(Fault &fault, const void *fn, DWORD ecx, const DWORD *args, DWORD n)
{
    FaultCall c = {&fault, fn, ecx, args, n, 0};
    return fault_call(c);
}

DWORD call_faulting_on_thread(Fault &fault, DWORD stack_reserve, const void *fn, DWORD ecx, const DWORD *args, DWORD n)
{
    FaultCall c = {&fault, fn, ecx, args, n, 0};
    HANDLE h = CreateThread(nullptr, stack_reserve, &fault_thread, &c, 0x10000 /* STACK_SIZE_PARAM_IS_A_RESERVATION */, nullptr);
    if (!h) {
        memset(&fault, 0, sizeof fault);
        fault.code = 0xffffffffu;   // no thread
        return 0;
    }
    WaitForSingleObject(h, 0xffffffffu);
    CloseHandle(h);
    return c.eax;
}

}  // namespace zax
