// Checks of the path and disk helpers (path_util.h): joining and appending with all kinds of folder ends,
// name, extension and cut on many paths (null included where the exe allows it), the disk tests on
// files, read-only files, folders and missing paths, creating folders, the backup to ".BAK", unused,
// temp and DOS time functions with fixed system time and temp names. Every test writes only below
// its own temp folder, separately for the original (0) and the reimplementation (1).
#include "check_util.h"
#include "init_list.h"
#include "path_util.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT DWORD WINAPI GetFileAttributesA(const char *path);
WINIMPORT BOOL WINAPI SetFileAttributesA(const char *path, DWORD attributes);
WINIMPORT DWORD WINAPI GetTickCount(void);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);

namespace zax {
namespace {

template <typename F> F pick(int v, DWORD address) { return v ? at<F>(address) : original<F>(address); }
DWORD addr(const void *p) { return (DWORD)(uintptr_t)p; }

char g_root[2][MAX_PATH];   // separate folders for the original and the reimplementation

void root_name(char *out, int v, const char *name) { wsprintfA(out, "%s%s", g_root[v], name); }
void write_text(const char *path, const char *s)
{
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD w;
    WriteFile(f, s, (DWORD)lstrlenA(s), &w, nullptr);
    CloseHandle(f);
}
// A path below root v as the text after the root (to compare outputs of both runs)
const char *below(int v, const char *p)
{
    DWORD n = (DWORD)lstrlenA(g_root[v]);
    for (DWORD i = 0; i < n; i++)
        if (p[i] != g_root[v][i])
            return p;
    return p + n;
}

const char *const DIRS[] = {"", "a", "a/", "a\\", "C:\\x\\y", "/", "dir.with.dots/sub", "\\\\server\\share", "x/y\\"};
const char *const NAMES[] = {"", "b.txt", "sub/c.zax", "\\lead", "no_ext", ".hidden"};

void test_text(Tally &t)
{
    char d[300], out[2][300];
    auto compare = [&](const char *what, DWORD r0, DWORD r1, const char *a, const char *b) {
        wsprintfA(d, "[%s \"%.40s\" \"%.40s\": %x %x, %.60s / %.60s]", what, a, b, r0, r1, out[0], out[1]);
        t.check(r0 == r1 && memcmp(out[0], out[1], sizeof out[0]) == 0, "paths", d);
    };
    for (const char *dir : DIRS)
        for (const char *name : NAMES) {
            DWORD r[2];
            for (int v = 0; v < 2; v++) {
                memset(out[v], 0x66, sizeof out[v]);
                r[v] = pick<DWORD(__fastcall *)(char *, const char *, const char *)>(v, 0x4a4220)(out[v], dir, name);
            }
            compare("Path_Join", r[0], r[1], dir, name);
            for (int v = 0; v < 2; v++) {
                memset(out[v], 0x66, sizeof out[v]);
                r[v] = pick<DWORD(__fastcall *)(char *, const char *, const char *, const char *)>(v, 0x4a42c0)(out[v], dir, name, ".bak");
            }
            compare("Path_JoinExt", r[0], r[1], dir, name);
            for (int v = 0; v < 2; v++) {
                memset(out[v], 0x66, sizeof out[v]);
                r[v] = pick<DWORD(__fastcall *)(char *, const char *, const char *, const char *, const char *)>(v, 0x4a4380)(out[v], dir, name, "-",
                                                                                                                         "2");
            }
            compare("Path_Join3", r[0], r[1], dir, name);
            for (DWORD fn : {0x4a4470u, 0x4a44f0u}) {
                for (int v = 0; v < 2; v++) {
                    memset(out[v], 0x66, sizeof out[v]);
                    lstrcpyA(out[v], dir);
                    r[v] = pick<DWORD(__fastcall *)(char *, const char *)>(v, fn)(out[v], name);
                }
                compare(fn == 0x4a4470 ? "Path_Append" : "Path_AppendBackslash", r[0], r[1], dir, name);
            }
        }
    const char *const PATHS[] = {"", "a", "a/b", "a\\b", "a/b\\c", "a\\b/c", "/", "\\", "a/", "C:\\Games\\Zax\\Data\\Levels\\x.zax",
                                 "dir.d/name", "name.ext", "a.b.c", "dir/.hidden", "x/y/"};
    for (const char *p : PATHS) {
        DWORD r[2];
        for (int v = 0; v < 2; v++)
            r[v] = addr(pick<const char *(__fastcall *)(const char *)>(v, 0x4a4570)(p)) - addr(p);
        wsprintfA(d, "[PathName \"%.60s\": %x %x]", p, r[0], r[1]);
        t.check(r[0] == r[1], "paths", d);
        for (int v = 0; v < 2; v++) {
            lstrcpyA(out[v], p);
            r[v] = addr(pick<char *(__fastcall *)(char *)>(v, 0x4a3f80)(out[v])) - addr(out[v]);
        }
        wsprintfA(d, "[Path_Extension \"%.60s\": %x %x]", p, r[0], r[1]);
        t.check(r[0] == r[1], "paths", d);
        for (const char *ext : {"", ".bak", "_x"}) {
            for (int v = 0; v < 2; v++) {
                memset(out[v], 0x66, sizeof out[v]);
                lstrcpyA(out[v], p);
                r[v] = pick<DWORD(__fastcall *)(char *, const char *)>(v, 0x4a3fb0)(out[v], ext);
            }
            compare("Path_SetExtension", r[0], r[1], p, ext);
        }
        for (int v = 0; v < 2; v++) {
            memset(out[v], 0x66, sizeof out[v]);
            lstrcpyA(out[v], p);
            DWORD x = addr(pick<char *(__fastcall *)(char *)>(v, 0x4a45b0)(out[v]));
            r[v] = x ? x - addr(out[v]) : 0xffffffff;
        }
        compare("Path_CutName", r[0], r[1], p, "");
    }
    DWORD r[2];
    for (int v = 0; v < 2; v++)
        r[v] = addr(pick<const char *(__fastcall *)(const char *)>(v, 0x4a4570)(nullptr));
    t.check(r[0] == r[1] && r[0] == PATH_NAME_NULL, "paths", "[PathName (null)]");
    StringState st[2];
    for (int v = 0; v < 2; v++) {
        ZString z;
        r[v] = addr(pick<ZString *(__fastcall *)(ZString *)>(v, 0x4a45f0)(&z)) - addr(&z);
        st[v] = state(z, nullptr);
        at<void(THISCALL *)(ZString *)>(0x4defd0)(&z);
    }
    wsprintfA(d, "[ConfigPath: %x %x, %.40s / %.40s]", r[0], r[1], st[0].text, st[1].text);
    t.check(r[0] == r[1] && memcmp(&st[0], &st[1], sizeof st[0]) == 0, "paths", d);
}

// GetSystemTimeAsFileTime and GetTempFileNameA through stand-ins
DWORD g_now[2];
void WINAPI fake_now(DWORD *ft)
{
    ft[0] = g_now[0];
    ft[1] = g_now[1];
}
unsigned g_temp_hash;
DWORD WINAPI fake_temp_name(const char *dir, const char *prefix, DWORD unique, char *out)
{
    g_temp_hash = fnv(fnv(fnv(2166136261u, dir, (DWORD)lstrlenA(dir)), prefix, (DWORD)lstrlenA(prefix)), &unique, 4);
    lstrcpyA(out, "C:\\fake\\name.tmp");
    return 0x4242;
}

void test_system(Tally &t)
{
    char d[200];
    ImportPatch now;
    patch_import(now, 0x5ea0b0, (void *)&fake_now);
    // 1601 (before the DOS range), 1980-01-01, 2001-06-15 12:34:57, 2026-10-23 23:59:59, 2108-01-01 (after it)
    const DWORD TIMES[][2] = {{0, 0}, {0xe1d58000, 0x01a8e79f}, {0x95e42e80, 0x01c0f597}, {0x9c2e6980, 0x01dd634a}, {0x6803c000, 0x023868b8}};
    for (const auto &tm : TIMES) {
        DWORD w[2][3];
        for (int v = 0; v < 2; v++) {
            g_now[0] = tm[0];
            g_now[1] = tm[1];
            WORD date = 0x1111, time = 0x2222;
            w[v][0] = pick<DWORD(__fastcall *)(WORD *, WORD *)>(v, 0x4a3c80)(&date, &time);
            w[v][1] = date;
            w[v][2] = time;
        }
        wsprintfA(d, "[DosTime_Now %x %x: %x %x %x / %x %x %x]", tm[1], tm[0], w[0][0], w[0][1], w[0][2], w[1][0], w[1][1], w[1][2]);
        t.check(memcmp(w[0], w[1], sizeof w[0]) == 0, "system", d);
    }
    unpatch_import(now);
    ImportPatch temp;
    patch_import(temp, 0x5ea0b4, (void *)&fake_temp_name);
    for (const char *prefix : {"Zax", "", "longer prefix"}) {
        DWORD r[2];
        unsigned h[2];
        char out[2][300];
        for (int v = 0; v < 2; v++) {
            memset(out[v], 0x66, sizeof out[v]);
            r[v] = pick<DWORD(__fastcall *)(char *, const char *)>(v, 0x4a3d30)(out[v], prefix);
            h[v] = g_temp_hash;
        }
        wsprintfA(d, "[File_TempName %s: %x %x, %x %x]", prefix, r[0], r[1], h[0], h[1]);
        t.check(r[0] == r[1] && h[0] == h[1] && memcmp(out[0], out[1], sizeof out[0]) == 0, "system", d);
    }
    unpatch_import(temp);
    // Real temp names: a new file each, in the same folder
    char out[2][300];
    DWORD r[2];
    for (int v = 0; v < 2; v++) {
        r[v] = pick<DWORD(__fastcall *)(char *, const char *)>(v, 0x4a3d30)(out[v], "Zx");
        DWORD a = GetFileAttributesA(out[v]);
        DeleteFileA(out[v]);
        r[v] = (r[v] != 0) | (a != 0xffffffff ? 2 : 0) | (DWORD)(addr(path_name(out[v])) - addr(out[v])) << 8;
    }
    wsprintfA(d, "[File_TempName real: %x %x]", r[0], r[1]);
    t.check(r[0] == r[1] && (r[0] & 3) == 3, "system", d);
}

void test_disk(Tally &t)
{
    char d[300], p[2][MAX_PATH];
    for (int v = 0; v < 2; v++) {
        root_name(p[v], v, "file.txt");
        write_text(p[v], "content");
        root_name(p[v], v, "readonly.txt");
        write_text(p[v], "read only");
        SetFileAttributesA(p[v], 1);
        root_name(p[v], v, "u0.tmp");
        write_text(p[v], "u");
        root_name(p[v], v, "u1.tmp");
        write_text(p[v], "u");
    }
    const char *const REL[] = {"file.txt", "readonly.txt", "", "missing.txt", "nofolder\\missing.txt", "nofolder/x/y", "*.txt", "u?.tmp", "file.txt\\"};
    for (const char *rel : REL)
        for (DWORD fn : {0x4a3cb0u, 0x4a3d70u}) {
            DWORD r[2];
            for (int v = 0; v < 2; v++) {
                root_name(p[v], v, rel);
                r[v] = pick<DWORD(__fastcall *)(const char *)>(v, fn)(p[v]);
            }
            wsprintfA(d, "[0x%x %s: %x %x]", fn, rel, r[0], r[1]);
            t.check(r[0] == r[1], "disk", d);
        }
    // Paths without a folder (the game folder) and the empty path
    for (const char *s : {"ZaxNoSuchFile.bin", "", "Zax.exe"})
        for (DWORD fn : {0x4a3cb0u, 0x4a3d70u}) {
            DWORD r[2];
            for (int v = 0; v < 2; v++)
                r[v] = pick<DWORD(__fastcall *)(const char *)>(v, fn)(s);
            wsprintfA(d, "[0x%x \"%s\": %x %x]", fn, s, r[0], r[1]);
            t.check(r[0] == r[1], "disk", d);
        }
    // Unused names
    for (const char *pattern : {"u%d.tmp", "v%d.tmp", "u%03d.tmp"}) {
        DWORD r[2];
        char out[2][MAX_PATH];
        for (int v = 0; v < 2; v++) {
            root_name(p[v], v, pattern);
            r[v] = pick<DWORD(__fastcall *)(char *, const char *)>(v, 0x4a3ce0)(out[v], p[v]);
        }
        wsprintfA(d, "[File_FindUnusedName %s: %x %x, %.40s / %.40s]", pattern, r[0], r[1], below(0, out[0]), below(1, out[1]));
        t.check(r[0] == r[1] && lstrcmpA(below(0, out[0]), below(1, out[1])) == 0, "disk", d);
    }

    // Creating folders: nested, with final separators, mixed separators, existing, below a file
    const char *const FOLDERS[] = {"cp\\a\\b\\c", "cp\\a\\b\\c", "cp2\\x\\", "cp3/y/", "cp4/a\\b/c", "file.txt\\sub", "cp5"};
    for (const char *rel : FOLDERS) {
        DWORD r[2], a[2];
        for (int v = 0; v < 2; v++) {
            root_name(p[v], v, rel);
            r[v] = pick<DWORD(__fastcall *)(const char *)>(v, 0x4a3ec0)(p[v]);
            a[v] = GetFileAttributesA(p[v]);
        }
        wsprintfA(d, "[Folder_CreatePath %s: %x %x, attributes %x %x]", rel, r[0], r[1], a[0], a[1]);
        t.check(r[0] == r[1] && a[0] == a[1], "disk", d);
    }
    for (int v = 0; v < 2; v++)
        for (const char *rel : {"cp\\a\\b\\c", "cp\\a\\b", "cp\\a", "cp", "cp2\\x", "cp2", "cp3\\y", "cp3", "cp4\\a\\b\\c", "cp4\\a\\b", "cp4\\a", "cp4", "cp5"}) {
            root_name(p[v], v, rel);
            RemoveDirectoryA(p[v]);
        }

    // Backups: with and without out, over an old .BAK, read-only, missing
    for (int k = 0; k < 5; k++) {
        DWORD r[2];
        unsigned h[2];
        char out[2][MAX_PATH];
        for (int v = 0; v < 2; v++) {
            char bak[MAX_PATH];
            root_name(p[v], v, k == 3 ? "readonly.txt" : k == 4 ? "missing.txt" : "backup.txt");
            if (k < 3)
                write_text(p[v], k == 2 ? "third" : "first");
            wsprintfA(bak, "%s.BAK", p[v]);
            if (k == 2)
                write_text(bak, "old backup");
            lstrcpyA(out[v], "unchanged");
            r[v] = pick<DWORD(__fastcall *)(const char *, char *)>(v, 0x4a4000)(p[v], k == 1 ? nullptr : out[v]);
            DWORD a[2] = {GetFileAttributesA(p[v]), GetFileAttributesA(bak)};
            h[v] = fnv(2166136261u, a, sizeof a);
            h[v] = fnv_text(h[v], below(v, out[v]));
            DeleteFileA(bak);
        }
        wsprintfA(d, "[File_Backup case %d: %x %x, %x %x, %.40s / %.40s]", k, r[0], r[1], h[0], h[1], below(0, out[0]), below(1, out[1]));
        t.check(r[0] == r[1] && h[0] == h[1], "disk", d);
    }
    for (int v = 0; v < 2; v++)
        for (const char *rel : {"file.txt", "readonly.txt", "u0.tmp", "u1.tmp", "backup.txt"}) {
            root_name(p[v], v, rel);
            SetFileAttributesA(p[v], 0x80);
            DeleteFileA(p[v]);
        }
}

}  // namespace

ZAX_CHECKS(path_util)
{
    (void)cls;
    (void)n;
    Tally text{"Paths (text)"}, system{"Paths (time, temp names)"}, disk{"Paths (disk)"};
    DWORD t0 = GetTickCount();
    test_text(text);
    test_system(system);
    char tmp[MAX_PATH];
    if (GetTempPathA(MAX_PATH - 40, tmp)) {
        for (int v = 0; v < 2; v++) {
            wsprintfA(g_root[v], "%sZaxPaths%d\\", tmp, v);
            CreateDirectoryA(g_root[v], nullptr);
        }
        test_disk(disk);
        for (int v = 0; v < 2; v++)
            if (!RemoveDirectoryA(g_root[v]))
                log("  could not remove %s", g_root[v]);
    }
    log("  path checks: %u ms", GetTickCount() - t0);
    text.report();
    system.report();
    disk.report();
}

}  // namespace zax
