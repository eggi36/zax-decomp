// The exe's own stack frame for functions that read a local they never set (batch decomp-faithful2,
// docs/DECOMPILATION.md section 16).
//
// Some functions of Zax.exe read a stack slot that nothing on their path writes: a value a reader leaves
// unwritten when the file ends, a channel byte stored into a dword, a "part hit" the sweep writes only on
// some paths. What they read is whatever an earlier call left at that address. A C++ local would lie
// elsewhere in the compiler's frame (and the compiler's own prologue may write there), so the value would
// not be the exe's. ZAX_EXE_FRAME_ENTRY gives the C++ body the exe's frame instead:
//
//   ZAX_EXE_FRAME_ENTRY(entry, body, frame, nargs, pop)
//
// `entry` is a naked function, the hook's replacement. It does what the exe's prologue does to the stack
// (`sub esp, frame`), so the exe's locals [esp + 0] .. [esp + frame) lie at the same addresses relative to
// the return address and keep whatever the stack held there; nothing writes them. Then it calls
//
//   extern "C" <ret> __cdecl body(<ecx>, arg0, ..., arg(nargs - 1), BYTE *frame_base)
//
// with ecx (this), copies of the nargs stack arguments and frame_base = the exe's esp after its `sub`. Its
// own frame and its callees' frames lie below frame_base, as the exe's pushed registers and callees' frames
// do. The body reads and writes the exe's locals as frame_base[offset] and the argument slots as
// frame_base[frame + 4 + 4 * i] (the exe writes some of them, e.g. sscanf into an argument). On return
// eax, edx and st(0) are the body's; the entry removes `pop` bytes of arguments (ret pop) as the exe does.
//
// ZAX_EXE_FRAME_ENTRY_EAX is the same with the entry's eax as one more parameter before frame_base, for a
// function that returns the caller's eax on a path.
#pragma once

#define ZAX_EXE_FRAME_ENTRY(ENTRY, BODY, FRAME, NARGS, POP)                                 \
    extern "C" __attribute__((naked)) void ENTRY()                                           \
    {                                                                                        \
        __asm__("subl $" #FRAME ", %esp\n\t"         /* the exe's locals */                 \
                "pushl %esp\n\t"                     /* frame_base (push takes the old esp) */ \
                ".rept " #NARGS "\n\t"                                                       \
                "pushl (" #FRAME "+4+4*" #NARGS ")(%esp)\n\t" /* the arguments, last first */ \
                ".endr\n\t"                                                                  \
                "pushl %ecx\n\t"                                                             \
                "calll _" #BODY "\n\t"                                                       \
                "addl $(" #FRAME "+8+4*" #NARGS "), %esp\n\t"                                \
                "retl $" #POP);                                                              \
    }

#define ZAX_EXE_FRAME_ENTRY_EAX(ENTRY, BODY, FRAME, NARGS, POP)                             \
    extern "C" __attribute__((naked)) void ENTRY()                                           \
    {                                                                                        \
        __asm__("subl $" #FRAME ", %esp\n\t"                                                 \
                "pushl %esp\n\t"                                                             \
                "pushl %eax\n\t"                     /* the caller's eax */                  \
                ".rept " #NARGS "\n\t"                                                       \
                "pushl (" #FRAME "+8+4*" #NARGS ")(%esp)\n\t"                                \
                ".endr\n\t"                                                                  \
                "pushl %ecx\n\t"                                                             \
                "calll _" #BODY "\n\t"                                                       \
                "addl $(" #FRAME "+12+4*" #NARGS "), %esp\n\t"                               \
                "retl $" #POP);                                                              \
    }
