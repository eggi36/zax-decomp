// CZaxDialogAction's dialogue line 0x41d830 and 0x41dac0 (phase 3 remains, band A; docs/DECOMPILATION.md
// section 8, "Phase 3 remains, band A"; the rest of the class: zax_actions.cpp, action_helpers.cpp).
//
// The dialogue scripts are a definition list (0x62e7e0: records 0x62e810, count 0x62e814, folder 0x62e818;
// a record's +0x78 the loaded script, +0x7f "loading"); a script's +8 is its pointer array of CDialogLine,
// a line's +8 its name. The action holds the script's index (+8) and the line's (+0xc).
//
//   0x41d830  slot 29 (five arguments; al): the voice "<line name>.wav" is looked up in the sound manager
//             0x6c2480 (0x57e690, -1: none) and, when there, prepared (0x57e5d0); the script is looked up
//             again inline (loaded on first use like 0x48d8f0), the line's text taken (0x41e010), and both
//             strings go with the five arguments to slot 33 (the display), whose al is returned.
//   0x41dac0  the action's CDialogLine (the inline lookup; a missing script is read through null, as the
//             original).
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD SCRIPTS = 0x62e7e0, RECORDS = 0x62e810, COUNT = 0x62e814, FOLDER = 0x62e818;
constexpr DWORD SOUNDS = 0x6c2480;

BYTE *line_of(BYTE *script, BYTE *self) { return bp(reinterpret_cast<const DWORD *>(static_cast<uintptr_t>(dw(script, 8)))[dw(self, 0xc)]); }

// 0x41d830 (slot 29)
BYTE THISCALL DialogAction_ShowLine(BYTE *self, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5)
{
    BYTE voice[4], text[4];
    zs_ctor(voice);
    BYTE *script = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(SCRIPTS), dw(self, 8));
    fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(voice, glob<const char *>(0x60b054) /* "%s.wav" */,
                                                              zs_c_str(line_of(script, self) + 8));
    const int sound = fn<int(THISCALL *)(void *, const char *, int)>(0x57e690)(glob<void *>(SOUNDS), zs_c_str(voice), -1);
    if (sound != -1)
        fn<void(THISCALL *)(void *, int, void *)>(0x57e5d0)(glob<void *>(SOUNDS), sound, voice);
    zs_ctor(text);
    BYTE *s2 = *glob<BYTE **>(RECORDS) ? list_object(glob_addr(SCRIPTS), glob_addr(RECORDS), glob_addr(COUNT), glob_addr(FOLDER), dw(self, 8)) : nullptr;
    fn<DWORD(THISCALL *)(void *, void *)>(0x41e010)(line_of(s2, self), text);
    const BYTE r = vcall<BYTE>(self, 33, voice, text, a1, a2, a3, a4, a5);
    zs_dtor(text);
    zs_dtor(voice);
    return r;
}

// 0x41dac0
BYTE *THISCALL DialogAction_Line(BYTE *self)
{
    BYTE *script = *glob<BYTE **>(RECORDS) ? list_object(glob_addr(SCRIPTS), glob_addr(RECORDS), glob_addr(COUNT), glob_addr(FOLDER), dw(self, 8)) : nullptr;
    return line_of(script, self);
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(dialog_rest_a)
{
    const Hook list[] = {
        {0x41d830, (void *)&DialogAction_ShowLine, "CZaxDialogAction::ShowLine29", 6, {0x81, 0xec, 0x08, 0x01, 0x00, 0x00, 0x53, 0x56}},
        {0x41dac0, (void *)&DialogAction_Line, "CZaxDialogAction::Line", 5, {0xa1, 0x10, 0xe8, 0x62, 0x00, 0x81, 0xec, 0x00}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
