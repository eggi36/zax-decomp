// Path and disk helpers reimplemented (see path_util.h).
#include "path_util.h"

namespace zax {

// Windows functions through the exe's import table (0x5ea070-0x5ea0c0)
template <typename F> static F api(ImpKey slot) { return imp<F>(slot); }
using FindFirstFn = HANDLE(WINAPI *)(const char *, void *);
static HANDLE find_first(const char *spec, void *data) { return api<FindFirstFn>(0x5ea078)(spec, data); }
static DWORD find_close(HANDLE h) { return api<DWORD(WINAPI *)(HANDLE)>(0x5ea070)(h); }
static DWORD file_attributes(const char *path) { return api<DWORD(WINAPI *)(const char *)>(0x5ea0bc)(path); }
static DWORD create_directory(const char *path) { return api<DWORD(WINAPI *)(const char *, void *)>(0x5ea0c0)(path, nullptr); }

// C runtime of the exe
static void crt_sprintf(char *out, const char *fmt, DWORD i) { fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(out, fmt, i); }
static int crt_remove(const char *path) { return fn<int(__cdecl *)(const char *)>(0x5d17a6)(path); }
static DWORD crt_rename(const char *from, const char *to) { return fn<DWORD(__cdecl *)(const char *, const char *)>(0x5d2046)(from, to); }

constexpr DWORD FIND_DATA_SIZE = 0x140;   // WIN32_FIND_DATAA
constexpr DWORD NOT_FOUND = 0xffffff00;   // FindFirstFileA's -1 with al = 0

static DWORD text_length(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}
// strcpy; returns the length + 1 (ecx of the exe's rep movs, left in eax by some callers)
static DWORD copy_text(char *dst, const char *src)
{
    DWORD n = text_length(src) + 1;
    memcpy(dst, src, n);
    return n;
}
static char *last_char(char *s, char c)
{
    char *r = nullptr;
    for (; *s; s++)
        if (*s == c)
            r = s;
    return r;
}
// The later of the last '\\' and the last '/'
static char *last_separator(char *s)
{
    char *a = last_char(s, '\\'), *b = last_char(s, '/');
    return b > a ? b : a;
}
// Behind the text of a non-empty path: a separator unless it ends with one; returns where to go on
static char *after_separator(char *path, char separator)
{
    char *e = path + text_length(path) - 1;
    if (*e != '\\' && *e != '/')
        *++e = separator;
    return e + 1;
}

// The folder of path (up to its last separator) + "*.*" exists for FindFirstFileA: FindClose's result
// with al = 1, else 0xffffff00
static DWORD folder_found(const char *path)
{
    char dir[0x104], spec[0x140], data[FIND_DATA_SIZE];
    copy_text(dir, path);
    char *cut = last_separator(dir);
    *(cut ? cut + 1 : dir) = 0;
    copy_text(spec, dir);
    copy_text(spec[0] ? after_separator(spec, '/') : spec, glob<const char *>(0x613858));   // "*.*"
    HANDLE h = find_first(spec, data);
    if (h == INVALID_HANDLE_VALUE)
        return NOT_FOUND;
    return (find_close(h) & 0xffffff00) | 1;
}

DWORD __fastcall DosTime_Now(WORD *date, WORD *time)
{
    DWORD now[2];
    api<void(WINAPI *)(DWORD *)>(0x5ea0b0)(now);
    return api<DWORD(WINAPI *)(const DWORD *, WORD *, WORD *)>(0x5ea0ac)(now, date, time);
}

DWORD __fastcall File_Found(const char *path)
{
    char data[FIND_DATA_SIZE];
    HANDLE h = find_first(path, data);
    if (h == INVALID_HANDLE_VALUE)
        return NOT_FOUND;
    return (find_close(h) & 0xffffff00) | 1;
}

// 0x4a3ce0: out = sprintf(pattern, i) for i = 0, 1, ... until FindFirstFileA finds nothing
DWORD __fastcall File_FindUnusedName(char *out, const char *pattern)
{
    char data[FIND_DATA_SIZE];
    for (DWORD i = 0;; i++) {
        crt_sprintf(out, pattern, i);
        HANDLE h = find_first(out, data);
        if (h == INVALID_HANDLE_VALUE)
            return (DWORD)(uintptr_t)h;
        find_close(h);
    }
}

DWORD __fastcall File_TempName(char *out, const char *prefix)
{
    char dir[0x100];
    api<DWORD(WINAPI *)(DWORD, char *)>(0x5ea0b8)(0x100, dir);
    return api<DWORD(WINAPI *)(const char *, const char *, DWORD, char *)>(0x5ea0b4)(dir, prefix, 0, out);
}

// 0x4a3d70: true for a file that is not read-only (al = !(attributes & 1), the rest from the attributes)
// or, if GetFileAttributesA fails, when the folder of the path has any entry (also "." and ".."), i.e.
// a missing file in an existing folder counts as well
DWORD __fastcall File_DiskTest(const char *path)
{
    DWORD a = file_attributes(path);
    if (a != 0xffffffff)
        return (a & 0xffffff00) | (~a & 1);
    return folder_found(path);
}

// 0x4a3ec0: creates the folder (a final separator is dropped); if that fails, cuts the path back at the
// last '\\' (or, without one, the last '/') until a folder can be created and starts again from the
// full path. False when no separator is left.
DWORD __fastcall Folder_CreatePath(const char *path)
{
    char buf[0x104];
    for (;;) {
        copy_text(buf, path);
        DWORD n = text_length(buf);
        if (n && (buf[n - 1] == '/' || buf[n - 1] == '\\'))
            buf[n - 1] = 0;
        DWORD r = create_directory(buf);
        if (r)
            return (r & 0xffffff00) | 1;
        do {
            char *cut = last_char(buf, '\\');
            if (!cut)
                cut = last_char(buf, '/');
            if (!cut)
                return 0;
            *cut = 0;
        } while (!create_directory(buf));
    }
}

char *__fastcall Path_Extension(char *path)
{
    char *dot = last_char(path, '.');
    return dot ? dot : path + text_length(path);
}

DWORD __fastcall Path_SetExtension(char *path, const char *ext) { return copy_text(Path_Extension(path), ext); }

// 0x4a4000: renames an existing file that is not read-only to path + ".BAK" (an old one is removed
// first); with a missing attribute the folder test of 0x4a3d70 decides. out (if not null) gets the new
// name. al = renamed; the upper bytes: FindFirstFileA's -1, the attributes, rename's result or 0.
DWORD __fastcall File_Backup(const char *path, char *out)
{
    char data[FIND_DATA_SIZE];
    HANDLE h = find_first(path, data);
    if (h == INVALID_HANDLE_VALUE)
        return NOT_FOUND;
    find_close(h);
    DWORD a = file_attributes(path);
    if (a != 0xffffffff) {
        if (a & 1)
            return a & 0xffffff00;
    } else if (folder_found(path) == NOT_FOUND) {
        return NOT_FOUND;
    }
    char bak[0x100];
    DWORD n = copy_text(bak, path);
    copy_text(bak + n - 1, glob<const char *>(0x613870));   // ".BAK"
    crt_remove(bak);
    DWORD r = crt_rename(path, bak);
    if (!out)
        return (r & 0xffffff00) | 1;
    copy_text(out, bak);
    return 1;
}

// 0x4a4220: out = dir + '/' (unless dir is empty or ends with '/' or '\\') + name. Returns the length of
// name + 1, or 0 for an empty dir.
DWORD __fastcall Path_Join(char *out, const char *dir, const char *name)
{
    copy_text(out, dir);
    if (!*out) {
        copy_text(out, name);
        return 0;
    }
    return copy_text(after_separator(out, '/'), name);
}

DWORD __fastcall Path_JoinExt(char *out, const char *dir, const char *name, const char *ext)
{
    copy_text(out, dir);
    copy_text(*out ? after_separator(out, '/') : out, name);
    copy_text(out + text_length(out), ext);
    return 0;
}

DWORD __fastcall Path_Join3(char *out, const char *dir, const char *a, const char *b, const char *c)
{
    copy_text(out, dir);
    copy_text(*out ? after_separator(out, '/') : out, a);
    copy_text(out + text_length(out), b);
    copy_text(out + text_length(out), c);
    return 0;
}

// 0x4a4470/0x4a44f0: appends name with a separator (unless path is empty or ends with one); returns the
// length of name + 1, or 0 for an empty path
DWORD __fastcall Path_Append(char *path, const char *name)
{
    if (!*path) {
        copy_text(path, name);
        return 0;
    }
    return copy_text(after_separator(path, '/'), name);
}

DWORD __fastcall Path_AppendBackslash(char *path, const char *name)
{
    if (!*path) {
        copy_text(path, name);
        return 0;
    }
    return copy_text(after_separator(path, '\\'), name);
}

// 0x4a4570: the part behind the last '/' or '\\'; null gives "In Path Name Null!"
const char *__fastcall PathName(const char *path)
{
    if (!path)
        return glob<const char *>(PATH_NAME_NULL);
    char *cut = last_separator(const_cast<char *>(path));
    return cut ? cut + 1 : path;
}

// 0x4a45b0: cuts the path behind its last separator (empty without one). Returns the last '/' (eax of
// the second strrchr); null clears the first byte of "In Path Name Null!" and returns it.
char *__fastcall Path_CutName(char *path)
{
    if (!path) {
        char *s = glob<char *>(PATH_NAME_NULL);
        *s = 0;
        return s;
    }
    char *a = last_char(path, '\\'), *b = last_char(path, '/');
    char *cut = b > a ? b : a;
    if (cut)
        cut[1] = 0;
    else
        *path = 0;
    return b;
}

ZString *__fastcall ConfigPath(ZString *out)
{
    fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(out, glob<const char *>(0x613878));
    return out;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(path_util)
{
    const Hook list[] = {
        {0x4a3c80, ptr(&DosTime_Now), "DosTime_Now", 7, {0x83, 0xec, 0x08, 0x8d, 0x44, 0x24, 0x00, 0x56}},
        {0x4a3cb0, ptr(&File_Found), "File_Found", 6, {0x81, 0xec, 0x40, 0x01, 0x00, 0x00, 0x8d, 0x44}},
        {0x4a3ce0, ptr(&File_FindUnusedName), "File_FindUnusedName", 6, {0x81, 0xec, 0x40, 0x01, 0x00, 0x00, 0x53, 0x55}},
        {0x4a3d30, ptr(&File_TempName), "File_TempName", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x8d, 0x44}},
        {0x4a3d70, ptr(&File_DiskTest), "File_DiskTest", 6, {0x81, 0xec, 0x48, 0x03, 0x00, 0x00, 0x57, 0x8b}},
        {0x4a3ec0, ptr(&Folder_CreatePath), "Folder_CreatePath", 6, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0x53, 0x8b}},
        {0x4a3f80, ptr(&Path_Extension), "Path_Extension", 5, {0x56, 0x8b, 0xf1, 0x6a, 0x2e, 0x56, 0xe8, 0x05}},
        {0x4a3fb0, ptr(&Path_SetExtension), "Path_SetExtension", 5, {0x53, 0x56, 0x57, 0x8b, 0xf1, 0x6a, 0x2e, 0x8b}},
        {0x4a4000, ptr(&File_Backup), "File_Backup", 6, {0x81, 0xec, 0x84, 0x04, 0x00, 0x00, 0x8d, 0x84}},
        {0x4a4220, ptr(&Path_Join), "Path_Join", 5, {0x53, 0x56, 0x8b, 0xd9, 0x57, 0x8b, 0xfa, 0x83}},
        {0x4a42c0, ptr(&Path_JoinExt), "Path_JoinExt", 5, {0x53, 0x56, 0x8b, 0xd9, 0x57, 0x8b, 0xfa, 0x83}},
        {0x4a4380, ptr(&Path_Join3), "Path_Join3", 5, {0x53, 0x56, 0x8b, 0xd9, 0x57, 0x8b, 0xfa, 0x83}},
        {0x4a4470, ptr(&Path_Append), "Path_Append", 5, {0x53, 0x8b, 0xd9, 0x56, 0x57, 0x80, 0x3b, 0x00}},
        {0x4a44f0, ptr(&Path_AppendBackslash), "Path_AppendBackslash", 5, {0x53, 0x8b, 0xd9, 0x56, 0x57, 0x80, 0x3b, 0x00}},
        {0x4a4570, ptr(&PathName), "PathName", 6, {0x56, 0x57, 0x8b, 0xf9, 0x85, 0xff, 0x74, 0x2a}},
        {0x4a45b0, ptr(&Path_CutName), "Path_CutName", 6, {0x56, 0x57, 0x8b, 0xf9, 0x85, 0xff, 0x74, 0x2c}},
        {0x4a45f0, ptr(&ConfigPath), "ConfigPath", 8, {0x56, 0x8b, 0xf1, 0x68, 0x78, 0x38, 0x61, 0x00}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
