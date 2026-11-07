// The last Core and Multiplayer functions of 0x480000-0x4dffff (phase 3 remains, band B; docs/DECOMPILATION.md
// section 8): the dynamic property list's lookups 0x482940-0x482db0, the point descriptor's dynamic value
// 0x4a8080/0x4a80b0 and its value array 0x4a81c0, the row table 0x4a8440/0x4a8480, the Win32 wrappers
// 0x4d2310-0x4d2650 and the multiplayer server options 0x4d2c50/0x4d2ca0.
//
// The dynamic property list [confirmed: code; checked] (ZDynamicProperties, the list of an object's dynamic
// fields, object_base.h): +4 the entries {descriptor, value} (8 bytes each, from the pool), +8 their number,
// +0xc the room. Growing takes max(n + 1, room * 1.2 (the float 0x5ea5f4, __ftol)) entries, copies the old
// ones and gives the old block back to the pool. A new entry's value comes from the descriptor's slot 35
// (0, -1) and is initialised by its slot 6; removing calls slot 8 and slot 36 with the value (free it),
// 0x482db0 calls slot 32 for every entry (writes it). Get-or-add and add look the descriptor up first: the
// first entry of the descriptor counts; one whose value is 0 is not reused (a second entry is appended),
// add then returns 0 for an existing value. The functions take the object as their first argument and do
// not use it.
//
// The point descriptor (vtables 0x5f4790, 0x5fd264; the dynamic value of a CPointFieldDesc, two floats): the
// value of a dynamic field is the object's dynamic block + the descriptor's offset (+0x10), or the default
// point +0x28 when the object has none; setting goes through the setter +0x30 when there is one. 0x4a81c0
// (slot of the descriptor: count, two unused) gives a heap block of count points without constructing them.
//
// The row table (RowTable_Alloc, fastcall columns, rows, an unused stack argument): max(rows, 2) row
// offsets i * columns from operator new, without a null test (the exe writes through a failed allocation).
//
// The Win32 wrappers go through the exe's import table (so the checks can put stand-ins there): 0x4d2310
// makes the exe's folder the current directory; 0x4d2470 posts WM_CLOSE to the main window 0x713e18;
// ShowErrorBox 0x4d24c0 (fastcall caption, text) shows the arrow cursor, brings the desktop window to the
// front and shows the message box, then clears the cursor; 0x4d2510 the bare message box; 0x4d2570 (fastcall
// exe, argument; show) starts `"exe" "argument"` with CreateProcessA (STARTF_USESHOWWINDOW, SW_HIDE for 1,
// SW_SHOWNORMAL otherwise); 0x4d2650 the tick count's low 24 bits.
//
// The server options (Multiplayer; the 0x1c-byte options of the server dialog 0x4d3080, which is user
// interface) [confirmed: code]: +0 the server name (the player's name 0x6c0488, then "Net Game"/"Multi-player
// Server Name" of the INI 0x6bd330), +4 1, +0xc 16, +0x10 20, +0x14 0, +0x18 1, +0x19 0 (+8 is left as it
// is); 0x4d2ca0 writes the name back.
//
// Return values: the original's eax where it is defined by the code (the lookups, the wrappers' API
// results, the setters' last value); al for the remove.
#include "exe_base.h"
#include "x87.h"

namespace zax {

namespace {

template <typename T> T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
DWORD &dw(void *p, DWORD off) { return fl<DWORD>(p, off); }
DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }
template <typename F> F import(DWORD slot) { return *at<F *>(slot); }

// ---- The dynamic property list ---------------------------------------------------------------------------

struct DynEntry {
    BYTE *desc;
    DWORD value;
};
struct DynList {
    DWORD vtable;
    DynEntry *data;
    DWORD count, room;
};

void *memmove_(void *d, const void *s, DWORD n) { return fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }

// The append shared by get-or-add and add: room for one more, the entry with a new value
DWORD dyn_append(DynList *l, BYTE *desc)
{
    DWORD need = l->count + 1;
    if (l->room < need) {
        const DWORD grown = (DWORD)x87_ftol(fild64((long long)l->room) * cst(glob_addr(0x5ea5f4)));
        if (grown > need)
            need = grown;
        const DWORD old = l->room;
        if (old < need) {
            l->room = need;
            DynEntry *n = static_cast<DynEntry *>(pool_get_alloc(need * 8));
            if (l->count)
                memmove_(n, l->data, l->count * 8);
            if (l->data) {
                pool_get_free(l->data, old * 8);
                l->data = nullptr;
            }
            l->data = n;
        }
    }
    DynEntry *e = &l->data[l->count++];
    e->desc = desc;
    e->value = vcall<DWORD>(desc, 35, 0, 0xffffffffu);
    vcall<DWORD>(e->desc, 6, e->value);
    return e->value;
}

// 0x482940 (list; descriptor): the value of the descriptor's first entry, 0 without one
DWORD THISCALL DynList_Get(DynList *l, BYTE *desc)
{
    for (DWORD i = 0; i < l->count; i++)
        if (l->data[i].desc == desc)
            return l->data[i].value;
    return 0;
}
// 0x482970 (list; object (unused), descriptor): the existing value or a new entry's
DWORD THISCALL DynList_GetOrAdd(DynList *l, void *, BYTE *desc)
{
    for (DWORD i = 0; i < l->count; i++)
        if (l->data[i].desc == desc) {
            if (l->data[i].value)
                return l->data[i].value;
            break;
        }
    return dyn_append(l, desc);
}
// 0x482a70 (list; object (unused), descriptor): 0 when the descriptor has a value, otherwise a new entry's
DWORD THISCALL DynList_Add(DynList *l, void *, BYTE *desc)
{
    for (DWORD i = 0; i < l->count; i++)
        if (l->data[i].desc == desc) {
            if (l->data[i].value)
                return 0;
            break;
        }
    return dyn_append(l, desc);
}
// 0x482b80 (list; descriptor): the first entry of the descriptor freed (slots 8 and 36) and removed. al: found.
DWORD THISCALL DynList_Remove(DynList *l, BYTE *desc)
{
    DWORD i = 0;
    for (; i < l->count; i++)
        if (l->data[i].desc == desc)
            break;
    if (i == l->count)
        return 0;
    vcall<DWORD>(l->data[i].desc, 8, l->data[i].value);
    vcall<DWORD>(l->data[i].desc, 36, l->data[i].value);
    if (i < l->count) {
        if (i + 1 < l->count)
            memmove_(&l->data[i], &l->data[i + 1], (l->count - (i + 1)) * 8);
        l->count--;
    }
    return 1;
}
// 0x482db0 (list): slot 32 of every entry's descriptor with its value (the count read once)
void THISCALL DynList_ForEach(DynList *l)
{
    const DWORD n = l->count;
    for (DWORD i = 0; i < n; i++)
        vcall<DWORD>(l->data[i].desc, 32, l->data[i].value);
}

// ---- The point descriptor --------------------------------------------------------------------------------

// 0x4a8080 (descriptor; object): the object's dynamic point, or the default point
DWORD THISCALL PointDesc_DynamicValue(BYTE *desc, void *object)
{
    const DWORD v = fn<DWORD(THISCALL *)(void *, void *)>(0x417320)(object, desc);   // Object_GetDynamic
    if (v)
        return dw(desc, 0x10) + v;
    return pv(desc + 0x28);
}
// 0x4a80b0 (descriptor; object, point): the point set through the setter or copied into the dynamic value
DWORD THISCALL PointDesc_SetDynamicValue(BYTE *desc, void *object, const DWORD *point)
{
    const DWORD v = fn<DWORD(THISCALL *)(void *, void *)>(0x417340)(object, desc);   // Object_GetOrAddDynamic
    if (const DWORD setter = dw(desc, 0x30))
        return fn_dyn<DWORD(THISCALL *)(DWORD, const DWORD *)>(setter)(v, point);
    DWORD *to = at<DWORD *>(dw(desc, 0x10) + v);
    to[0] = point[0];
    to[1] = point[1];
    return point[1];
}
// 0x4a81c0 (count, two unused): a heap block of count points
void *__stdcall PointDesc_NewValues(DWORD count, DWORD, DWORD) { return heap_alloc(count * 8); }

// ---- The row table -----------------------------------------------------------------------------------------

// 0x4a8440 RowTable_Alloc
DWORD *__fastcall RowTable_Alloc(DWORD columns, DWORD rows, DWORD)
{
    if (rows < 2)
        rows = 2;
    DWORD *t = static_cast<DWORD *>(heap_alloc(rows * 4));
    for (DWORD i = 0, v = 0; i < rows; i++, v += columns)
        t[i] = v;
    return t;
}
// 0x4a8480 RowTable_Free (the table's pointer)
void THISCALL RowTable_Free(DWORD **table)
{
    if (*table) {
        heap_free(*table);
        *table = nullptr;
    }
    *table = nullptr;
}

// ---- Win32 ---------------------------------------------------------------------------------------------------

constexpr DWORD G_MAIN_WINDOW = 0x713e18;
char *strrchr_(char *s, int c) { return fn<char *(__cdecl *)(char *, int)>(0x5d0d90)(s, c); }
int sprintf_(char *out, const char *fmt, const char *a) { return fn<int(__cdecl *)(char *, const char *, const char *)>(0x5d0db7)(out, fmt, a); }

// 0x4d2310: the exe's folder (GetModuleFileNameA up to the last '\\' or else '/') made the current directory
DWORD __cdecl Platform_SetExeDirectory()
{
    char path[0x104];
    path[0] = 0;
    if (imp<DWORD(WINAPI *)(void *, char *, DWORD)>(0x5ea0e0)(nullptr, path, 0x104)) {
        char *p = strrchr_(path, '\\');
        if (!p)
            p = strrchr_(path, '/');
        if (p)
            p[1] = 0;
    }
    return imp<DWORD(WINAPI *)(const char *)>(0x5ea0e4)(path);
}
// 0x4d2470: WM_CLOSE posted to the main window
DWORD __cdecl Platform_CloseWindow() { return imp<DWORD(WINAPI *)(DWORD, DWORD, DWORD, DWORD)>(0x5ea25c)(*glob<DWORD *>(G_MAIN_WINDOW), 0x10, 0, 0); }
// 0x4d24c0 ShowErrorBox (caption, text)
DWORD __fastcall ShowErrorBox(const char *caption, const char *text)
{
    const auto set_cursor = imp<DWORD(WINAPI *)(DWORD)>(0x5ea24c);
    set_cursor(imp<DWORD(WINAPI *)(DWORD, DWORD)>(0x5ea204)(0, 0x7f00));
    imp<DWORD(WINAPI *)(DWORD)>(0x5ea254)(imp<DWORD(WINAPI *)()>(0x5ea250)());
    imp<DWORD(WINAPI *)(DWORD, const char *, const char *, DWORD)>(0x5ea258)(*glob<DWORD *>(G_MAIN_WINDOW), text, caption, 0);
    return set_cursor(0);
}
// 0x4d2510 (caption, text): the message box
DWORD __fastcall Platform_MessageBox(const char *caption, const char *text)
{
    return imp<DWORD(WINAPI *)(DWORD, const char *, const char *, DWORD)>(0x5ea258)(*glob<DWORD *>(G_MAIN_WINDOW), text, caption, 0);
}
// 0x4d2570 (exe, argument or null; show): `"exe" "argument"` started. 1: started.
DWORD __fastcall Platform_StartProcess(const char *exe, const char *argument, DWORD show)
{
    struct {
        DWORD process[4];   // PROCESS_INFORMATION
        DWORD si[0x11];     // STARTUPINFOA: cb, then 0x40 bytes
        char line[0x400];
    } s;
    sprintf_(s.line, "\"%s\"", exe);
    if (argument) {
        DWORD n = 0;
        while (s.line[n])
            n++;
        sprintf_(s.line + n, " \"%s\"", argument);
    }
    memset(&s.si[1], 0, 0x40);
    s.si[0] = 0x44;
    BYTE *si = reinterpret_cast<BYTE *>(s.si);
    fl<WORD>(si, 0x30) = 1;   // wShowWindow
    fl<DWORD>(si, 0x2c) = 1;  // dwFlags
    if (show == 1)
        fl<WORD>(si, 0x30) = 0;
    const DWORD r = imp<DWORD(WINAPI *)(void *, char *, void *, void *, DWORD, DWORD, void *, void *, void *, void *)>(0x5ea0f0)(
        nullptr, s.line, nullptr, nullptr, 0, 0, nullptr, nullptr, s.si, s.process);
    return r ? 1 : 0;
}
// 0x4d2650: the tick count's low 24 bits
DWORD __cdecl Platform_Ticks24() { return imp<DWORD(WINAPI *)()>(0x5ea0e8)() & 0xffffff; }

// ---- The multiplayer server options --------------------------------------------------------------------------

const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }

// 0x4d2c50 (options): the defaults, the name from the INI
DWORD THISCALL ServerOptions_Init(BYTE *o)
{
    fn<void *(THISCALL *)(void *, const char *)>(0x4e1930)(o, zs_c_str(glob<void *>(0x6c0488)));
    fn<DWORD(THISCALL *)(void *, const char *, const char *, void *)>(0x46ec70)(glob<void *>(0x6bd330), glob<const char *>(0x60fa48) /* "Net Game" */,
                                                                                 glob<const char *>(0x615bb8) /* "Multi-player Server Name" */, o);
    dw(o, 4) = 1;
    dw(o, 0xc) = 0x10;
    dw(o, 0x10) = 0x14;
    dw(o, 0x14) = 0;
    o[0x18] = 1;
    o[0x19] = 0;
    return 0;
}
// 0x4d2ca0 (the name): written to the INI
DWORD THISCALL ServerOptions_SaveName(const void *name)
{
    const char *text = zs_c_str(name);
    return fn<DWORD(__fastcall *)(void *, const char *, const char *, const char *)>(0x4a47c0)(glob<void *>(0x6bd330), glob<const char *>(0x60fa48),
                                                                                               glob<const char *>(0x615bb8), text);
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(core_rest_b)
{
    const Hook list[] = {
#include "core_rest_b_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
