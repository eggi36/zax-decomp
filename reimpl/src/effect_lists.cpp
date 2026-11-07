// The list and text slots the effects batches left (phase 4, rendering; docs/DECOMPILATION.md 9, "The last
// effects: …"): the definition list slots of "Frame Overlay Options" (vtable 0x5f4b80, objects
// CFrameOverlayOptions, files "<name>.FOO") and of "Water Warp Effects" (vtable 0x602a88, objects
// CWaterWarpEffectFrame, files "<name>.txt"), and the image field kind's text slots.
//
// [confirmed: code; VM] The list slots have the shapes of def_list_slots.h (the game type and surface type
// lists'): slot 0 the deleting destructor, slot 35 the file of an object (the class test, the entry's index,
// slot 39), slot 37 the object a file makes ("<dir>/<name>.<ext>", written back), slot 39 an entry's path
// ("<folder>/<name>.<ext>"; the extension from Path_Extension, else ".<ext>"). 0x598910 (in the water frames'
// vtable) makes the object of "<dir>/<name>.txt" and keeps it only when it is a CWaterWarpEffectFrame (else 0,
// the object left as it is).
// The image field (CImageFieldDesc, the frame at the object + the descriptor's +0x10): its text is
// "CFrame(width,height,depth)", "{", a line of hex per row (width · bytes per pixel), "}". 0x4b5b20 (object, file)
// writes it; 0x4b59f0 (object, line, file) reads it: unless the line starts with "CFrame(" and sscanf finds
// three numbers nothing happens; else the frame takes the size (CFrame_SetSize), the "{" line is read, then
// a row per height line (leading spaces and tabs skipped, Hex_Decode); a "}" ends early without the closing
// line, otherwise the closing line is read. The 0x1000-byte line buffer of the original is on its stack (with
// a stack probe); here it is a buffer of the module's (the slot runs in the game thread only).
#include "def_list_slots.h"
#include "ai_common.h"

namespace zax {

using deflist::Slots;
using ai::dw;

namespace {

constexpr DWORD TEXT_FOO = 0x613e74, TEXT_TXT = 0x618880, TEXT_DOT_TXT = 0x60f62c;   // "FOO", "txt", ".txt"
char g_line[0x1000];

DWORD read_line(void *file, char *buf) { return fn<DWORD(THISCALL *)(void *, char *, DWORD)>(0x4a20a0)(file, buf, 0x1000); }
DWORD write_line(void *file, const char *s) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x4a1a40)(file, s); }

}  // namespace

CClassInfo *__cdecl OverlayOptions_Class() { return fn<CClassInfo *(__cdecl *)()>(0x4b4f90)(); }
CClassInfo *__cdecl WaterWarpFrame_Class() { return fn<CClassInfo *(__cdecl *)()>(0x597f30)(); }

// 0x598910 (dir, name, out): the object of "<dir>/<name>.txt" (written back), kept when it is a
// CWaterWarpEffectFrame. eax: it or 0.
void *THISCALL WaterWarpList_CreateFrame(void *, const char *dir, const char *name, void **out)
{
    ZString path;
    deflist::zs_ctor(&path);
    deflist::zs_set_path3(&path, dir, name, glob<const char *>(TEXT_DOT_TXT));
    void *o = deflist::object_create_from_file(&path, 0, 1, out);
    if (o && !(deflist::is_kind_of(o, WaterWarpFrame_Class()) & 0xff))
        o = nullptr;
    deflist::zs_dtor(&path);
    return o;
}

// 0x4b59f0 (object, line, file): read the image (see the head comment)
DWORD THISCALL ImageField_ReadText(BYTE *desc, BYTE *object, const char *line, void *file)
{
    BYTE *frame = object + dw(desc, 0x10);
    const char *tag = glob<const char *>(0x613e8c);   // "CFrame("
    const DWORD r = fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5d17d0)(line, tag, (DWORD)lstrlenA(tag));
    if (r)
        return r;
    int w = 0, h = 0, depth = 0;
    const int got = fn<int(__cdecl *)(const char *, const char *, ...)>(0x5d1539)(line, glob<const char *>(0x613e78), &w, &h, &depth);
    if (got != 3)
        return (DWORD)got;
    fn<void(THISCALL *)(void *, int, int, int)>(0x4ab5a0)(frame, w, h, depth);
    read_line(file, g_line);
    const DWORD bpp = vcall<DWORD>(frame, 30);
    const DWORD row = vcall<DWORD>(frame, 45) * bpp;
    BYTE *px = vcall<BYTE *>(frame, 31);
    for (DWORD y = 0; y < (DWORD)h; y++) {
        const DWORD rl = read_line(file, g_line);
        const char *p = g_line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '}')
            return deflist::with_low_byte(rl, '}');
        fn<void(__fastcall *)(void *, const char *, DWORD)>(0x587ff0)(px, p, row);
        px += row;
    }
    return read_line(file, g_line);
}

// 0x4b5b20 (object, file): write the image (see the head comment)
DWORD THISCALL ImageField_WriteText(BYTE *desc, BYTE *object, void *file)
{
    BYTE *frame = object + dw(desc, 0x10);
    ZString s;
    deflist::zs_ctor(&s);
    const DWORD depth = vcall<DWORD>(frame, 29), h = vcall<DWORD>(frame, 39), w = vcall<DWORD>(frame, 45);
    fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(&s, glob<const char *>(0x613e78), w, h, depth);
    write_line(file, deflist::zs_c_str(&s));
    write_line(file, glob<const char *>(0x60a7f8));   // "{"
    fn<DWORD(THISCALL *)(void *)>(0x4a1ab0)(file);
    const DWORD bpp = vcall<DWORD>(frame, 30);
    const DWORD row = vcall<DWORD>(frame, 45) * bpp;
    const BYTE *px = vcall<const BYTE *>(frame, 31);
    for (DWORD y = 0; y < vcall<DWORD>(frame, 39); y++) {
        fn<void(THISCALL *)(ZString *, const void *, DWORD)>(0x4e1880)(&s, px, row);
        px += row;
        write_line(file, deflist::zs_c_str(&s));
    }
    fn<DWORD(THISCALL *)(void *)>(0x4a1ae0)(file);
    write_line(file, glob<const char *>(0x60a7e4));   // "}"
    return deflist::zs_dtor(&s);
}

ZAX_HOOKS(effect_lists)
{
    const Hook list[] = {
#include "effect_lists_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
