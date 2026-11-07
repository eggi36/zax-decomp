// The C runtime's compiler helpers: the 64-bit arithmetic (__allmul, __alldiv, __allrem, __aulldiv,
// __aullrem, __allshl, __allshr, __aullshr), __ftol and _chkstk (docs/DECOMPILATION.md 10, "C runtime").
// They have register conventions of their own (arguments on the stack popped by the callee, the count in
// cl, the value in st(0), the size in eax), so they are written in assembly. The same symbols serve the
// DLL's own compiled code, which has no C runtime of its own.
//
// Division by zero faults (#DE) as in the exe: the divisions use div with the divisor as given.
#include "crt_common.h"

extern "C" {
void zax_allmul();
void zax_alldiv();
void zax_allrem();
void zax_aulldiv();
void zax_aullrem();
void zax_allshl();
void zax_allshr();
void zax_aullshr();
void zax_ftol();
void zax_chkstk();
}

// Unsigned 64 / 64 bits: with a divisor below 2^32 two 32-bit divisions; otherwise the quotient fits in 32
// bits, estimated by shifting both numbers right until the divisor fits, then corrected by one.
// In: a at [esp+12] (after the two pushes), b at [esp+20]. Out: quotient in edx:eax, the remainder
// computed from it where needed.
__asm__(R"(
    .text
    .globl __aulldiv
    .globl _zax_aulldiv
__aulldiv:
_zax_aulldiv:
    pushl %ebx
    pushl %esi
    movl 24(%esp), %eax
    testl %eax, %eax
    jnz 1f
    movl 20(%esp), %ecx
    movl 16(%esp), %eax
    xorl %edx, %edx
    divl %ecx
    movl %eax, %ebx
    movl 12(%esp), %eax
    divl %ecx
    movl %ebx, %edx
    jmp 3f
1:  movl %eax, %ecx
    movl 20(%esp), %ebx
    movl 16(%esp), %edx
    movl 12(%esp), %eax
2:  shrl $1, %ecx
    rcrl $1, %ebx
    shrl $1, %edx
    rcrl $1, %eax
    testl %ecx, %ecx
    jnz 2b
    divl %ebx
    movl %eax, %esi
    mull 24(%esp)
    movl %eax, %ecx
    movl 20(%esp), %eax
    mull %esi
    addl %ecx, %edx
    jc 4f
    cmpl 16(%esp), %edx
    ja 4f
    jb 5f
    cmpl 12(%esp), %eax
    jbe 5f
4:  decl %esi
5:  xorl %edx, %edx
    movl %esi, %eax
3:  popl %esi
    popl %ebx
    ret $16

    .globl __aullrem
    .globl _zax_aullrem
__aullrem:
_zax_aullrem:
    pushl %ebx
    movl 20(%esp), %eax
    testl %eax, %eax
    jnz 1f
    movl 16(%esp), %ecx
    movl 12(%esp), %eax
    xorl %edx, %edx
    divl %ecx
    movl 8(%esp), %eax
    divl %ecx
    movl %edx, %eax
    xorl %edx, %edx
    jmp 3f
1:  movl %eax, %ecx
    movl 16(%esp), %ebx
    movl 12(%esp), %edx
    movl 8(%esp), %eax
2:  shrl $1, %ecx
    rcrl $1, %ebx
    shrl $1, %edx
    rcrl $1, %eax
    testl %ecx, %ecx
    jnz 2b
    divl %ebx
    movl %eax, %ecx
    mull 20(%esp)
    xchgl %eax, %ecx
    mull 16(%esp)
    addl %ecx, %edx
    jc 4f
    cmpl 12(%esp), %edx
    ja 4f
    jb 5f
    cmpl 8(%esp), %eax
    jbe 5f
4:  subl 16(%esp), %eax
    sbbl 20(%esp), %edx
5:  subl 8(%esp), %eax
    sbbl 12(%esp), %edx
    negl %edx
    negl %eax
    sbbl $0, %edx
3:  popl %ebx
    ret $16

    .globl __alldiv
    .globl _zax_alldiv
__alldiv:
_zax_alldiv:
    pushl %edi
    pushl %esi
    pushl %ebx
    xorl %edi, %edi
    movl 20(%esp), %eax
    testl %eax, %eax
    jge 1f
    incl %edi
    movl 16(%esp), %edx
    negl %eax
    negl %edx
    sbbl $0, %eax
    movl %eax, 20(%esp)
    movl %edx, 16(%esp)
1:  movl 28(%esp), %eax
    testl %eax, %eax
    jge 2f
    incl %edi
    movl 24(%esp), %edx
    negl %eax
    negl %edx
    sbbl $0, %eax
    movl %eax, 28(%esp)
    movl %edx, 24(%esp)
2:  testl %eax, %eax
    jnz 3f
    movl 24(%esp), %ecx
    movl 20(%esp), %eax
    xorl %edx, %edx
    divl %ecx
    movl %eax, %ebx
    movl 16(%esp), %eax
    divl %ecx
    movl %ebx, %edx
    jmp 6f
3:  movl %eax, %ebx
    movl 24(%esp), %ecx
    movl 20(%esp), %edx
    movl 16(%esp), %eax
4:  shrl $1, %ebx
    rcrl $1, %ecx
    shrl $1, %edx
    rcrl $1, %eax
    testl %ebx, %ebx
    jnz 4b
    divl %ecx
    movl %eax, %esi
    mull 28(%esp)
    movl %eax, %ecx
    movl 24(%esp), %eax
    mull %esi
    addl %ecx, %edx
    jc 5f
    cmpl 20(%esp), %edx
    ja 5f
    jb 7f
    cmpl 16(%esp), %eax
    jbe 7f
5:  decl %esi
7:  xorl %edx, %edx
    movl %esi, %eax
6:  decl %edi
    jnz 8f
    negl %edx
    negl %eax
    sbbl $0, %edx
8:  popl %ebx
    popl %esi
    popl %edi
    ret $16

    .globl __allrem
    .globl _zax_allrem
__allrem:
_zax_allrem:
    pushl %ebx
    pushl %edi
    xorl %edi, %edi
    movl 16(%esp), %eax
    testl %eax, %eax
    jge 1f
    incl %edi
    movl 12(%esp), %edx
    negl %eax
    negl %edx
    sbbl $0, %eax
    movl %eax, 16(%esp)
    movl %edx, 12(%esp)
1:  movl 24(%esp), %eax
    testl %eax, %eax
    jge 2f
    movl 20(%esp), %edx
    negl %eax
    negl %edx
    sbbl $0, %eax
    movl %eax, 24(%esp)
    movl %edx, 20(%esp)
2:  testl %eax, %eax
    jnz 3f
    movl 20(%esp), %ecx
    movl 16(%esp), %eax
    xorl %edx, %edx
    divl %ecx
    movl 12(%esp), %eax
    divl %ecx
    movl %edx, %eax
    xorl %edx, %edx
    decl %edi
    jns 7f
    jmp 8f
3:  movl %eax, %ebx
    movl 20(%esp), %ecx
    movl 16(%esp), %edx
    movl 12(%esp), %eax
4:  shrl $1, %ebx
    rcrl $1, %ecx
    shrl $1, %edx
    rcrl $1, %eax
    testl %ebx, %ebx
    jnz 4b
    divl %ecx
    movl %eax, %ecx
    mull 24(%esp)
    xchgl %eax, %ecx
    mull 20(%esp)
    addl %ecx, %edx
    jc 5f
    cmpl 16(%esp), %edx
    ja 5f
    jb 6f
    cmpl 12(%esp), %eax
    jbe 6f
5:  subl 20(%esp), %eax
    sbbl 24(%esp), %edx
6:  subl 12(%esp), %eax
    sbbl 16(%esp), %edx
    decl %edi
    jns 8f
7:  negl %edx
    negl %eax
    sbbl $0, %edx
8:  popl %edi
    popl %ebx
    ret $16

    .globl __allmul
    .globl _zax_allmul
__allmul:
_zax_allmul:
    movl 8(%esp), %eax
    movl 16(%esp), %ecx
    orl %eax, %ecx
    movl 12(%esp), %ecx
    jnz 1f
    movl 4(%esp), %eax
    mull %ecx
    ret $16
1:  pushl %ebx
    mull %ecx
    movl %eax, %ebx
    movl 8(%esp), %eax
    mull 20(%esp)
    addl %eax, %ebx
    movl 8(%esp), %eax
    mull %ecx
    addl %ebx, %edx
    popl %ebx
    ret $16

    .globl __allshl
    .globl _zax_allshl
__allshl:
_zax_allshl:
    cmpb $64, %cl
    jae 2f
    cmpb $32, %cl
    jae 1f
    shldl %cl, %eax, %edx
    shll %cl, %eax
    ret
1:  movl %eax, %edx
    xorl %eax, %eax
    andb $31, %cl
    shll %cl, %edx
    ret
2:  xorl %eax, %eax
    xorl %edx, %edx
    ret

    .globl __allshr
    .globl _zax_allshr
__allshr:
_zax_allshr:
    cmpb $64, %cl
    jae 2f
    cmpb $32, %cl
    jae 1f
    shrdl %cl, %edx, %eax
    sarl %cl, %edx
    ret
1:  movl %edx, %eax
    sarl $31, %edx
    andb $31, %cl
    sarl %cl, %eax
    ret
2:  sarl $31, %edx
    movl %edx, %eax
    ret

    .globl __aullshr
    .globl _zax_aullshr
__aullshr:
_zax_aullshr:
    cmpb $64, %cl
    jae 2f
    cmpb $32, %cl
    jae 1f
    shrdl %cl, %edx, %eax
    shrl %cl, %edx
    ret
1:  movl %edx, %eax
    xorl %edx, %edx
    andb $31, %cl
    shrl %cl, %eax
    ret
2:  xorl %eax, %eax
    xorl %edx, %edx
    ret

    .globl _zax_ftol
_zax_ftol:
    pushl %ebp
    movl %esp, %ebp
    addl $-12, %esp
    wait
    fnstcw -2(%ebp)
    wait
    movw -2(%ebp), %ax
    orb $12, %ah
    movw %ax, -4(%ebp)
    fldcw -4(%ebp)
    fistpll -12(%ebp)
    fldcw -2(%ebp)
    movl -12(%ebp), %eax
    movl -8(%ebp), %edx
    leave
    ret

    .globl _zax_chkstk
_zax_chkstk:
    pushl %ecx
    leal 8(%esp), %ecx
    cmpl $0x1000, %eax
    jb 2f
1:  subl $0x1000, %ecx
    subl $0x1000, %eax
    testl %eax, (%ecx)
    cmpl $0x1000, %eax
    jae 1b
2:  subl %eax, %ecx
    movl %esp, %eax
    testl %eax, (%ecx)
    movl %ecx, %esp
    movl (%eax), %ecx
    movl 4(%eax), %eax
    pushl %eax
    ret
)");

namespace zax {

ZAX_HOOKS(crt_math)
{
    const Hook list[] = {
        {0x5d2640, (void *)&zax_allmul, "__allmul", 8, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x4c, 0x24, 0x10}},
        {0x5d2680, (void *)&zax_alldiv, "__alldiv", 5, {0x57, 0x56, 0x53, 0x33, 0xff, 0x8b, 0x44, 0x24}},
        {0x5d1570, (void *)&zax_allrem, "__allrem", 8, {0x53, 0x57, 0x33, 0xff, 0x8b, 0x44, 0x24, 0x10}},
        {0x5db370, (void *)&zax_aulldiv, "__aulldiv", 6, {0x53, 0x56, 0x8b, 0x44, 0x24, 0x18, 0x0b, 0xc0}},
        {0x5dc450, (void *)&zax_aullrem, "__aullrem", 5, {0x53, 0x8b, 0x44, 0x24, 0x14, 0x0b, 0xc0, 0x75}},
        {0x5dc6c0, (void *)&zax_allshl, "__allshl", 0, {0x80, 0xf9, 0x40, 0x73, 0x15, 0x80, 0xf9, 0x20}},
        {0x5e9700, (void *)&zax_allshr, "__allshr", 0, {0x80, 0xf9, 0x40, 0x73, 0x16, 0x80, 0xf9, 0x20}},
        {0x5d1630, (void *)&zax_aullshr, "__aullshr", 0, {0x80, 0xf9, 0x40, 0x73, 0x15, 0x80, 0xf9, 0x20}},
        {0x5d0488, (void *)&zax_ftol, "__ftol", 6, {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf4, 0x9b, 0xd9}},
        {0x5d2080, (void *)&zax_chkstk, "_chkstk", 6, {0x51, 0x3d, 0x00, 0x10, 0x00, 0x00, 0x8d, 0x4c}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
