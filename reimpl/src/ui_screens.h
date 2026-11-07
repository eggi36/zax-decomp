// Shared calls of the game's screens and dialogs (ui_dialogs.cpp, ui_inventory.cpp, ui_dialogue.cpp,
// ui_pages.cpp, ui_options_screens.cpp): every call into the window toolkit, the ZString class and the rest of
// the game goes through its exe address, so the original's side of a check runs the original all the way down.
#pragma once
#include "ui_window.h"

namespace zax::ui {

constexpr DWORD NOWHERE = 0xfffffc18;   // -1000: AddChild's "keep the coordinate"
constexpr DWORD EMPTY_TEXT = 0x62e558;
constexpr DWORD G_REPORTER = 0x6c02cc, G_APP = 0x713f14, G_WORLD = 0x6c2080, G_SOUND_LIST = 0x713708;

inline BYTE *alloc(DWORD n) { return static_cast<BYTE *>(pool_alloc(n)); }
inline void pool_delete(void *p, DWORD n) { pool_free(p, n); }
inline BYTE *op_new(DWORD n) { return fn<BYTE *(__cdecl *)(DWORD)>(0x5d033a)(n); }
inline void op_delete(void *p) { fn<void(__cdecl *)(void *)>(0x5d0320)(p); }
inline void *memmove_(void *d, const void *s, DWORD n) { return fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }

// ZString (one dword: the rep, text at rep + 8) by its address
inline const char *c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
inline const char *c_str(DWORD z) { return c_str(bp(glob_addr_dyn(z))); }
inline void zs_ctor(void *z) { fn<void(THISCALL *)(void *)>(0x4dec90)(z); }
inline void zs_ctor_text(void *z, const char *t) { fn<void(THISCALL *)(void *, const char *)>(0x4dee00)(z, t); }
inline void zs_copy(void *z, DWORD from) { fn<void(THISCALL *)(void *, DWORD)>(0x4dedf0)(z, from); }
inline void zs_dtor(void *z) { fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
inline DWORD zs_assign(void *z, DWORD from) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4e18f0)(z, from); }
inline DWORD zs_length(const void *z) { return fn<DWORD(THISCALL *)(const void *)>(0x4e13b0)(z); }
inline bool zs_not_empty(const void *z) { return (BYTE)fn<DWORD(THISCALL *)(const void *)>(0x4e13e0)(z); }
template <typename... A> inline void zs_format(void *z, const char *fmt, A... a) { fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, fmt, a...); }

// The window toolkit
inline void add_child(BYTE *self, const void *child, DWORD how, DWORD rel, DWORD x, DWORD y, DWORD b)
{
    fn<void(THISCALL *)(BYTE *, const void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x40c7c0)(self, child, how, rel, x, y, b);
}
inline void add_child_front(BYTE *self, const void *child, DWORD how, DWORD rel, DWORD x, DWORD y, DWORD b)
{
    fn<void(THISCALL *)(BYTE *, const void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x40c8d0)(self, child, how, rel, x, y, b);
}
inline void set_width(const void *w, DWORD v) { fn<void(THISCALL *)(const void *, DWORD)>(0x40f5b0)(w, v); }
inline void set_height(const void *w, DWORD v) { fn<void(THISCALL *)(const void *, DWORD)>(0x40f5d0)(w, v); }
inline void set_left(const void *w, int v) { fn<void(THISCALL *)(const void *, int)>(0x40d680)(w, v); }
inline void set_top(const void *w, int v) { fn<void(THISCALL *)(const void *, int)>(0x40d6c0)(w, v); }
inline void move_to(const void *w, int x, int y) { fn<void(THISCALL *)(const void *, int, int)>(0x40d630)(w, x, y); }
inline void focus(BYTE *self, const void *child) { fn<void(THISCALL *)(BYTE *, const void *)>(0x40ca40)(self, child); }
inline void same_width(const void *first, const void *last) { fn<void(THISCALL *)(const void *, const void *)>(0x40d230)(first, last); }
inline void dialog_ctor(BYTE *self, DWORD parent, const char *title) { fn<void(THISCALL *)(BYTE *, DWORD, const char *)>(0x403d00)(self, parent, title); }
inline void dialog_dtor(BYTE *self) { fn<void(THISCALL *)(BYTE *)>(0x403d70)(self); }
inline void frame_window_ctor(BYTE *self, DWORD parent, const char *title) { fn<void(THISCALL *)(BYTE *, DWORD, const char *)>(0x402d80)(self, parent, title); }
inline void frame_window_dtor(BYTE *self) { fn<void(THISCALL *)(BYTE *)>(0x402f00)(self); }
inline void window_ctor(BYTE *self, DWORD parent, const char *name) { fn<void(THISCALL *)(BYTE *, DWORD, const char *)>(0x40a8a0)(self, parent, name); }
inline void window_dtor(BYTE *self) { fn<void(THISCALL *)(BYTE *)>(0x40b190)(self); }
inline void frame_notify(BYTE *self, DWORD from, DWORD what) { fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x4035f0)(self, from, what); }

// New windows of the toolkit (null when the pool gives no block)
inline BYTE *new_label(BYTE *parent, const char *text)   // TextLabel::TextLabel 0x4127d0 (0x128)
{
    BYTE *w = alloc(0x128);
    return w ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x4127d0)(w, parent, text) : nullptr;
}
inline BYTE *new_label_z(BYTE *parent, DWORD zstring)   // TextLabel::TextLabel(ZString) 0x412930
{
    BYTE *w = alloc(0x128);
    return w ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x412930)(w, parent, zstring) : nullptr;
}
inline BYTE *new_button_z(BYTE *parent, DWORD zstring, DWORD a = 0)   // Button_ctor 0x40fa60 (0x138)
{
    BYTE *w = alloc(0x138);
    return w ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *, DWORD, DWORD)>(0x40fa60)(w, parent, zstring, a) : nullptr;
}
inline BYTE *new_button(BYTE *parent, const char *text, DWORD a = 0)   // Button::Button 0x40f9b0 (0x138)
{
    BYTE *w = alloc(0x138);
    return w ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *, DWORD)>(0x40f9b0)(w, parent, text, a) : nullptr;
}
inline BYTE *new_edit(BYTE *parent, const char *text)   // EditField::EditField 0x410b50 (0x120)
{
    BYTE *w = alloc(0x120);
    return w ? fn<BYTE *(THISCALL *)(BYTE *, BYTE *, const char *)>(0x410b50)(w, parent, text) : nullptr;
}

// The pointer arrays of the exe's templates inside a window ({vtable, data, count, capacity} at `arr`):
// every element deleted from the last (`del`), each removed with the template's inlined RemoveAt
template <typename Del> inline void array_delete_all(BYTE *arr, Del del)
{
    while (DWORD n = m(arr, 8)) {
        const DWORD i = n - 1;
        if (DWORD e = m(bp(m(arr, 4)), i * 4))
            del(e);
        const DWORD count = m(arr, 8);
        if (i < count) {
            if (n < count)
                memmove_(bp(m(arr, 4)) + i * 4, bp(m(arr, 4)) + n * 4, (count - n) * 4);
            m(arr, 8) = m(arr, 8) - 1;
        }
    }
}
// The pool flavour's data freed (the vtable written first)
inline void array_free_pool(BYTE *arr, DWORD vt)
{
    m(arr, 0) = vt;
    if (DWORD d = m(arr, 4)) {
        pool_get_free(bp(d), m(arr, 0xc) << 2);
        m(arr, 4) = 0;
        m(arr, 0xc) = 0;
    }
    m(arr, 8) = 0;
}

// The records of a resource or definition list at `list` (+0x30 the records of 0x80 bytes: +0x78 the object,
// +0x7f being loaded; +0x34 the count; +0x38 the folder): index 0 when out of range, null when the list has no
// records or the record is being loaded, loaded on first use through DefinitionList_LoadRecord 0x55e090
inline DWORD record_loaded(DWORD list, DWORD i)
{
    DWORD recs = gd(list + 0x30);
    if (!recs)
        return 0;
    if (i >= gd(list + 0x34))
        i = 0;
    const DWORD rec = i << 7;
    if (gb(recs + rec + 0x7f))
        return 0;
    if (!m(bp(recs), rec + 0x78)) {
        fn<void(THISCALL *)(DWORD, DWORD)>(0x55e090)(list, i);
        recs = gd(list + 0x30);
    }
    return m(bp(recs), rec + 0x78);
}
// The same with the loader inlined (the list's slot 1 for the name, Path_Join with the folder, PathName, slot 37
// loads into the record)
inline DWORD record_inline(DWORD list, DWORD i)
{
    DWORD recs = gd(list + 0x30);
    if (!recs)
        return 0;
    if (i >= gd(list + 0x34))
        i = 0;
    const DWORD rec = i << 7;
    if (gb(recs + rec + 0x7f))
        return 0;
    if (!m(bp(recs), rec + 0x78) && recs && !gb(recs + rec + 0x7f)) {
        gb(recs + rec + 0x7f) = 1;
        char path[0x100];
        const DWORD name = vc(at<void *>(list), 1, i);
        fn<void(__fastcall *)(char *, DWORD, DWORD)>(0x4a4220)(path, list + 0x38, name);
        char *file = fn<char *(__fastcall *)(char *)>(0x4a4570)(path);
        DWORD loaded;
        if (file == path) {
            loaded = vc(at<void *>(list), 37, glob_addr(EMPTY_TEXT), file, gd(list + 0x30) + rec + 0x78);
        } else {
            file[-1] = 0;
            loaded = vc(at<void *>(list), 37, path, file, gd(list + 0x30) + rec + 0x78);
        }
        m(bp(gd(list + 0x30)), rec + 0x78) = loaded;
        gb(gd(list + 0x30) + rec + 0x7f) = 0;
    }
    return m(bp(gd(list + 0x30)), rec + 0x78);
}
// A resource of the sequence list 0x6c0540 by name (its slot 3: the index of the name, -1)
inline DWORD find_index(DWORD list, DWORD name) { return vc(at<void *>(list), 3, at<const char *>(name), -1); }
constexpr DWORD SEQUENCES = 0x6c0540, FONTS = 0x6be1e0;

// The rules object of the application's rules holder when it is a CZaxCustomRules (else null)
inline DWORD custom_rules()
{
    BYTE *holder = bp(m(bp(gd(glob_addr(G_APP))), 0x2e0));
    if (!m(holder, 0x38))
        return 0;
    const DWORD ci = fn<DWORD(__cdecl *)()>(0x5a1350)();
    if (!(BYTE)fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x416790)(m(holder, 0x38), ci))
        return 0;
    return m(holder, 0x38);
}

}  // namespace zax::ui
