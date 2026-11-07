// The window base class of the game's own window toolkit (CJWindow, vtable 0x5eb404, 0x100 bytes) as the
// toolkit's modules see it: the member offsets and the virtual slots they call (docs/DECOMPILATION.md 9,
// "The window toolkit").
//
// Layout [confirmed: the constructors 0x40a8a0/0x40a940, the init 0x40a9b0, the destructor 0x40b190 and the
// slots below]:
//   +0x04 rect (left, top, right, bottom) in the parent's coordinates
//   +0x14 visible rect on the screen (the screen rect cut to the parent's visible rect; empty when nothing
//         shows), the hit test of the parent's mouse dispatch
//   +0x24 screen rect (rect moved by the parent's screen origin minus its scroll position)
//   +0x34/+0x38 content width and height (the scroll info's when set, else the rect's)
//   +0x3c clip rect: the visible rect in the window's own content coordinates
//   +0x4c the look (a button look object, slot 1; ButtonLookFactory_Get 0x4e2610 by default)
//   +0x50 the title (ZString); +0x54/+0x58 the INI section the position is saved to and whether the size is
//   +0x59 index of the accelerator in the title (0xff none), +0x5a the title's width up to it, +0x5b the
//         accelerator (upper case; 0 none)
//   +0x5c active (has the focus within its parent), +0x5d may take the focus (tab stop), +0x5e, +0x5f,
//   +0x60 shown, +0x61 enabled, +0x63 framed, +0x64 modal (stops the dispatch to the windows behind it),
//   +0x65 transparent, +0x66 drawn at least once, +0x67 the mouse buttons it captured (bit per button)
//   +0x68/+0x6c the mouse position of the last button press, +0x70
//   +0x74 the window's dirty-rect cache (an object of 0x64 bytes, drawn through a proxy frame)
//   +0x78/+0x79/+0x7a border sizes (4 * (font height / 5) twice, 2 * (font height / 5) shared in 0x60a52c)
//   +0x7b the title takes an accelerator (&)
//   +0x7c the font (CFont; its height at +0x80); 0x62e5d0 is the default
//   +0x80, +0x90, +0xa0, +0xb0, +0xc0, +0xd0 six FieldColors (0x10 each; set by slots 49-56)
//   +0xe0/+0xe4 the window's own textures (active, inactive; deleted with slot 1)
//   +0xe8 parent, +0xec first child (the one with the focus), +0xf0 last child, +0xf4 next sibling,
//   +0xf8 previous sibling
//   +0xfc scroll info (0x1c bytes, operator new, freed again when all zero): +0 x, +4 y, +8 content width,
//         +0xc content height, +0x10 shows scroll bars, +0x14 the vertical, +0x18 the horizontal CJScrollBar
#pragma once
#include "exe_base.h"

namespace zax::ui {

inline BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
inline DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }
template <typename T = DWORD> inline T &m(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
inline int &mi(void *p, DWORD off) { return *reinterpret_cast<int *>(static_cast<BYTE *>(p) + off); }
inline BYTE &mb(void *p, DWORD off) { return *(static_cast<BYTE *>(p) + off); }
// Globals of the exe
inline DWORD &gd(DWORD a) { return *at<DWORD *>(a); }
inline BYTE &gb(DWORD a) { return *at<BYTE *>(a); }

// Offsets of CJWindow
constexpr DWORD W_RECT = 0x04, W_VISIBLE = 0x14, W_SCREEN = 0x24, W_CONTENT_W = 0x34, W_CONTENT_H = 0x38, W_CLIP = 0x3c,
                W_LOOK = 0x4c, W_TITLE = 0x50, W_INI = 0x54, W_INI_SIZE = 0x58, W_ACCEL_INDEX = 0x59, W_ACCEL_WIDTH = 0x5a,
                W_ACCEL = 0x5b, W_ACTIVE = 0x5c, W_TAB_STOP = 0x5d, W_5E = 0x5e, W_5F = 0x5f, W_SHOWN = 0x60,
                W_ENABLED = 0x61, W_FRAMED = 0x63, W_MODAL = 0x64, W_TRANSPARENT = 0x65, W_DRAWN = 0x66, W_CAPTURE = 0x67,
                W_PRESS_X = 0x68, W_PRESS_Y = 0x6c, W_70 = 0x70, W_CACHE = 0x74, W_BORDER = 0x78, W_BORDER2 = 0x79,
                W_BORDER3 = 0x7a, W_ACCEL_ON = 0x7b, W_FONT = 0x7c, W_TEXTURE = 0xe0, W_TEXTURE_INACTIVE = 0xe4,
                W_PARENT = 0xe8, W_FIRST = 0xec, W_LAST = 0xf0, W_NEXT = 0xf4, W_PREV = 0xf8, W_SCROLL = 0xfc;
// The scroll info
constexpr DWORD S_X = 0, S_Y = 4, S_W = 8, S_H = 0xc, S_SHOW = 0x10, S_VBAR = 0x14, S_HBAR = 0x18, SCROLL_SIZE = 0x1c;
// A scroll bar's thickness [confirmed: 0x40d490, 0x40f110, 0x412ea0]
constexpr int BAR = 15;

// Virtual slots of CJWindow (by number; the byte offset is 4 * slot)
enum Slot : int {
    V_DELETE = 0, V_SET_LOOK = 1, V_UPDATE = 2, V_DRAW = 4, V_CLOSE = 5, V_ON_CLOSE = 6, V_CLOSE_CHILDREN = 7,
    V_DRAW_FRAME = 8, V_MOUSE_9 = 9, V_MOUSE_DOWN = 10, V_MOUSE_UP = 11, V_MOUSE_12 = 12, V_MOUSE_MOVE = 13,
    V_MOUSE_DRAG = 14, V_MOUSE_15 = 15, V_KEY = 16, V_CHAR = 17, V_ACTIVATE = 18, V_SET_ACTIVE = 19, V_NOTIFY = 21,
    V_28 = 28, V_30 = 30, V_ADD_CHILD = 31, V_ADD_CHILD_FRONT = 32, V_REMOVE_CHILD = 33, V_FOCUS = 34,
    V_NEXT_FOCUS = 36, V_PREV_FOCUS = 37, V_INVALIDATE = 38, V_IS_SCREEN = 39, V_SET_TITLE = 43, V_SET_RECT = 57,
    V_GET_RECT = 58, V_SET_SIZE = 59, V_BORDER_H = 60, V_BORDER_W = 61, V_PLACE = 62, V_FIT_CHILD = 63, V_CURSOR = 69,
    V_PARENT_CURSOR = 70, V_TEXTURE = 72, V_UPDATE_SCROLL = 75
};

template <typename R = DWORD, typename... A> inline R vc(const void *obj, int slot, A... a) { return vcall<R>(obj, slot, a...); }

// The globals of the toolkit
constexpr DWORD G_FOCUS_BY_KEY = 0x62e5e0;   // byte: the focus moves by keyboard (tab), set around slots 36/37
constexpr DWORD G_DEFAULT_FONT = 0x62e5d0, G_BORDER = 0x60a52c, G_IN_DESTRUCTOR = 0x62e5c4, G_DIRTY = 0x60a528;
constexpr DWORD G_CURSOR = 0x6cff40;         // the mouse cursor object (+4 its sequence)
constexpr DWORD G_KEYS = 0x6c1050;           // the keyboard state (Keys::IsDown 0x4e2850)
constexpr DWORD G_SOUNDS = 0x713708;         // the sound manager (play 0x57e820)
constexpr DWORD SOUND_ERROR = 0x60a4d8, SOUND_SWITCH = 0x60a50c, SOUND_ACTION = 0x60a588;

inline bool key_down(DWORD scan) { return fn<bool(THISCALL *)(void *, DWORD)>(0x4e2850)(glob<void *>(G_KEYS), scan) ; }
inline void play_sound(DWORD name) { fn<void(THISCALL *)(void *, const char *)>(0x57e820)(glob<void *>(G_SOUNDS), at<const char *>(name)); }
// GetTickCount through the exe's import (so the checks' stand-in answers both sides)
inline DWORD tick_count() { return (*imp_slot<DWORD(WINAPI **)()>(0x5ea0e8))(); }
// The cursor table (0x40e9e0: entry i or entry 0)
DWORD __fastcall CursorEntry(int i);

}  // namespace zax::ui
