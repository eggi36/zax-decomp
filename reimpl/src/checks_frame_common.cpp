// The frame check harness's shared state and its recording vtables (checks_frame_common.h): copies of the
// exe's frame vtables whose slots from 21 on are thunks (mov eax, k; jmp common) that log the call on the
// running side and jump on to the exe's slot with the registers and the stack as the caller left them.
#include "checks_frame_common.h"

namespace zax::framec {

Harness g_h = {};

}  // namespace zax::framec

extern "C" {
extern char framec_thunks[];
}
__asm__(".text\n"
        ".globl _framec_thunks\n"
        "_framec_thunks:\n"
        ".set framec_i, 0\n"
        ".rept 512\n"
        "  movl $framec_i, %eax\n"
        "  .byte 0xe9\n"
        "  .long _framec_common - . - 4\n"
        ".set framec_i, framec_i + 1\n"
        ".endr\n"
        "_framec_common:\n"
        "  pushl %edx\n"
        "  pushl %ecx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  call _framec_handler\n"
        "  addl $8, %esp\n"
        "  popl %ecx\n"
        "  popl %edx\n"
        "  jmp *%eax\n");

namespace zax::framec {

extern "C" DWORD __cdecl framec_handler(DWORD k, DWORD ecx)
{
    const int side = g_h.logging_side;
    if (side == 0 || side == 1) {
        CallLog &l = g_h.calls[side];
        const DWORD e[2] = {g_h.rec.slot_of[k], map_value(side, ecx)};
        l.hash = fnv(l.hash, e, sizeof e);
        if (l.count < 32) {
            l.first[l.count][0] = e[0];
            l.first[l.count][1] = e[1];
        }
        l.count++;
    }
    return g_h.rec.forward[k];
}

void init_recorder()
{
    if (g_h.rec.ready)
        return;
    DWORD k = 0;
    for (int c = 0; c < 5; c++)
        for (DWORD s = 0; s < CLASS_SLOTS[c]; s++) {
            const DWORD target = at<DWORD *>(CLASS_VTABLES[c])[s];
            if (s < 21 || k >= 512) {
                g_h.rec.vtables[c][s] = target;
                continue;
            }
            g_h.rec.forward[k] = target;
            g_h.rec.slot_of[k] = (DWORD)c << 8 | s;
            g_h.rec.vtables[c][s] = ptrv(framec_thunks) + 10 * k;
            k++;
        }
    g_h.rec.ready = true;
}

}  // namespace zax::framec
