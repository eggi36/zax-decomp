// Batch C4 (phase 5a), small slots and callbacks of AI, actions, objects, sound and the application base:
// the application's window slots (vtable 0x5f73e8, shared by CZaxApp 0x602dc8), the dialogue action's
// slots 7 and 23, CEntityBehaviorMoveToPos's deleting destructor, the block reader of 0x5f3240, the
// model preview window's factory callback, three small deleting destructors, two exit functions and the
// forwarders of 0x6020e0.
//
// Not hookable (identical by their bytes): 0x474450 (jmp CObject_ResetFields: a relative jump, slot 19 of
// the continuous fire weapons' AI), 0x4cdd50/0x4cdd60 (the application's slots 28/29: inc/dec dword
// [ecx+0x2c]; ret, 4 bytes).
//
// Return values are what the original leaves in eax (the exe functions called through their addresses).
#include "exe_base.h"

namespace zax {
namespace {

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
DWORD crt_delete(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }

// ---- The application base (0x5f73e8) ---------------------------------------------------------------------

// 0x4cdd70, slot 30: the modal counter +0x2c above 0
DWORD THISCALL App_IsModal(BYTE *self) { return (int)dw(self, 0x2c) > 0 ? 1 : 0; }
// 0x4cdec0, slot 19: the window +0x34's slot 76 (a tail jump with the slot's arguments; none without a window)
__attribute__((naked)) void App_WindowSlot76()
{
    __asm__("movl 0x34(%ecx), %ecx\n\ttestl %ecx, %ecx\n\tje 1f\n\tmovl (%ecx), %eax\n\tjmp *0x130(%eax)\n1:\n\tret");
}
// 0x4cded0, slot 11 (window, textures, list, frame, b): 0x62e5d0 = frame, CJWindow::SetTextureList (fastcall
// textures, list), +0x30 = b or the frame, then with a window the window of slot 22 (window, frame) at +0x34.
// eax: slot 22's, 0 without a window.
DWORD THISCALL App_Init(BYTE *self, DWORD window, DWORD textures, DWORD list, DWORD frame, DWORD b)
{
    *glob<DWORD *>(0x62e5d0) = frame;
    fn<DWORD(__fastcall *)(DWORD, DWORD)>(0x40bf00)(textures, list);
    dw(self, 0x30) = b ? b : frame;
    if (!window)
        return 0;
    const DWORD r = vcall<DWORD>(self, 22, window, frame);
    dw(self, 0x34) = r;
    return r;
}
// 0x4ce640, slot 22 (a, b): a CScreenWindow (0x17c bytes) made from (a, b), or 0
void *THISCALL App_MakeScreenWindow(void *, DWORD a, DWORD b)
{
    void *w = pool_alloc(0x17c);
    if (!w)
        return nullptr;
    return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x408f20)(w, a, b);
}
// 0x4cdf40, slot 23 (child, front): the child added to the window +0x34 (CJWindow::AddToParentFront with 6
// for front, else 0)
DWORD THISCALL App_AddWindow(BYTE *self, DWORD child, DWORD front)
{
    const DWORD flags = (BYTE)front ? 6 : 0;
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x40c680)(pt(self, 0x34), child, flags);
}

// ---- The dialogue action (CDialogActionBase, 0x603238) --------------------------------------------------

// 0x41d810, slot 23 (a, b, c, d): slot 29 (a, b, c, d, 0)
DWORD THISCALL DialogAction_Execute(void *self, DWORD a, DWORD b, DWORD c, DWORD d) { return vcall<DWORD>(self, 29, a, b, c, d, 0); }
// 0x41da10, slot 7: CObject_Precache, then with "Play Voiceovers" on the voice of the line: the script
// (+8, an index of the dialogue script list 0x62e7e0, loaded if it is not) line +0xc's text + ".wav"
// precached in the sound list 0x6c2480. A script that is being loaded or a list without records gives a
// null script, whose line table is read anyway (as in the original).
DWORD THISCALL DialogAction_Precache(BYTE *self)
{
    DWORD r = fn<DWORD(THISCALL *)(void *)>(0x4167b0)(self);
    r = fn<DWORD(__fastcall *)(DWORD, DWORD, const char *, DWORD)>(0x4a4670)(glob_addr(0x6bd330), glob_addr(0x6bd430), glob<const char *>(0x60b05c), 1);
    if (!r)
        return r;
    BYTE *records = *glob<BYTE **>(0x62e810);
    DWORD i = dw(self, 8);
    BYTE *script = nullptr;
    if (records) {
        if (i >= *glob<DWORD *>(0x62e814))
            i = 0;
        if (!records[i * 0x80 + 0x7f]) {
            if (!dw(records + i * 0x80, 0x78)) {
                fn<DWORD(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(0x62e7e0), i);
                records = *glob<BYTE **>(0x62e810);
            }
            script = pt(records + i * 0x80, 0x78);
        }
    }
    BYTE *line = pt(pt(script, 8), dw(self, 0xc) * 4);
    const char *text = fn<const char *(THISCALL *)(void *)>(0x4e13a0)(line + 8);
    char name[0x100];
    fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(name, glob<const char *>(0x60b054), text);
    return fn<DWORD(THISCALL *)(void *, const char *)>(0x57e7f0)(glob<void *>(0x6c2480), name);
}

// ---- Objects and behaviours ----------------------------------------------------------------------------------

// The inlined destructor of a CEntityBehaviorMoveToPos (0xbc bytes): the class's vtable, FreeFields, the
// ZString +0xb8, the way point path +0x4c, the idle behaviour +0x1c, the turn-to-angle base
void destroy_move_to_pos(BYTE *o)
{
    dw(o, 0) = vt(0x5eeb68);
    fn<DWORD(THISCALL *)(void *)>(0x4173f0)(o);
    fn<DWORD(THISCALL *)(void *)>(0x4defd0)(o + 0xb8);
    fn<DWORD(THISCALL *)(void *)>(0x536e30)(o + 0x4c);
    fn<DWORD(THISCALL *)(void *)>(0x498dc0)(o + 0x1c);
    fn<DWORD(THISCALL *)(void *)>(0x495360)(o);
}
// 0x42f7a0, slot 1: with flag 2 the array (count in front, from new[]) destroyed from the last element and
// freed with operator delete (flag 1); else the object, freed to the pool (0xbc). eax: the block / this.
BYTE *THISCALL MoveToPos_DeletingDtor(BYTE *self, DWORD flags)
{
    if (flags & 2) {
        BYTE *block = self - 4;
        const DWORD n = dw(block, 0);
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                destroy_move_to_pos(self + (i - 1) * 0xbc);
        if (flags & 1)
            crt_delete(block);
        return block;
    }
    destroy_move_to_pos(self);
    if (flags & 1)
        pool_free(self, 0xbc);
    return self;
}
// 0x49a840, slot 2 of 0x5f3240 (a block reader): a new CEntityMovable of 0x12c bytes (vtable 0x5f324c, a
// ZString at +0x124, +0x114..+0x120 0, +0x128 0, the bounce filter EntityCharacter_BounceFilter) reads
// itself (slot 27) and becomes the last light beam. The object is used without a null test.
DWORD THISCALL Reader5f3240_Read(void *, void *packet)
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x12c));
    if (o) {
        fn<DWORD(THISCALL *)(void *)>(0x490f40)(o);
        fn<DWORD(THISCALL *)(void *)>(0x4dec90)(o + 0x124);
        dw(o, 0) = vt(0x5f324c);
        fn<DWORD(THISCALL *)(void *)>(0x4173c0)(o);
        dw(o, 0x114) = 0;
        dw(o, 0x118) = 0;
        dw(o, 0x11c) = 0;
        dw(o, 0x120) = 0;
        o[0x128] = 0;
        fn<DWORD(__fastcall *)(DWORD)>(0x491150)(fn_addr(0x49adf0));
    }
    vcall<DWORD>(o, 27, packet);
    return fn<DWORD(THISCALL *)(void *)>(0x4fd5c0)(o);
}
// 0x518230 (fastcall a, b): a WindowModelPreview (0x18 bytes from operator new) made by its Make (a, b), or 0
void *__fastcall ModelPreview_Make(DWORD a, DWORD b)
{
    void *w = fn<void *(__cdecl *)(DWORD)>(0x5d033a)(0x18);
    if (!w)
        return nullptr;
    return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x5179b0)(w, a, b);
}

// ---- Small deleting destructors, exit functions, callbacks --------------------------------------------------

// 0x476ce0 (0x5f0970 -> base 0x5f0974, operator delete) / 0x599530 (0x602dac -> 0x602da8, the pool: 8 bytes)
BYTE *THISCALL Dtor5f0970(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(0x5f0974);
    if (flags & 1)
        crt_delete(self);
    return self;
}
BYTE *THISCALL Dtor602dac(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(0x602da8);
    if (flags & 1)
        pool_free(self, 8);
    return self;
}
// 0x420ed0 / 0x421860: atexit of 0x62e960 / 0x62e990: the base vtable 0x5ece44 (eax: the caller's, not
// reproduced)
template <DWORD OBJECT> DWORD __cdecl BaseVtable_AtExit()
{
    *at<DWORD *>(OBJECT) = vt(0x5ece44);
    return 0;
}
// 0x4bed80 (pushed to ResourceField_CtorTyped): +0x14 = a
DWORD THISCALL Resource_Callback4bed80(BYTE *self, DWORD a)
{
    dw(self, 0x14) = a;
    return a;
}
// 0x52dc80 (the class registration's CreateArray of abstract classes): 0
DWORD THISCALL CreateArray_None(void *, DWORD) { return 0; }

// ---- 0x6020e0 / 0x5f99c8: forwarders to +0x28 ----------------------------------------------------------------

// 0x58f2d0, slot 2 (frame, colour): the rectangle of slot 3 drawn into the frame (Frame_DrawRect)
DWORD THISCALL Forwarder_DrawRect(void *self, void *frame, DWORD colour)
{
    DWORD rect[4];
    vcall<DWORD>(self, 3, rect);
    return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4b3430)(frame, rect, colour);
}
// 0x58f300, slot 3 (rect): +0x28's slot 37 (rect, 2, 0) / 0x58f320, slot 4 (a, b): +0x28's slot 38 (a, b, 2)
DWORD THISCALL Forwarder_Slot3(BYTE *self, DWORD rect) { return vcall<DWORD>(pt(self, 0x28), 37, rect, 2, 0); }
DWORD THISCALL Forwarder_Slot4(BYTE *self, DWORD a, DWORD b) { return vcall<DWORD>(pt(self, 0x28), 38, a, b, 2); }

}  // namespace

ZAX_HOOKS(c4_misc)
{
    const Hook list[] = {
#include "c4_misc_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
