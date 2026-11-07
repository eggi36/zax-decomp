// Checks of the file search (file_search.h): the file spec, the sinks, the search on disk (a test tree
// in the temp folder and real game folders) and the folder tree of the archives (a tree of our own
// with Zip_ListDir redirected, and the mounted archives).
#include "check_util.h"
#include "file_search.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);

namespace zax {

extern "C" int __cdecl zax_call_escapable_fast(void *fn, DWORD ecx, DWORD edx, DWORD a1, DWORD a2, DWORD a3);
__asm__(".text\n"
        ".globl _zax_call_escapable_fast\n"
        "_zax_call_escapable_fast:\n"
        "  pushl %ebp\n  pushl %ebx\n  pushl %esi\n  pushl %edi\n"
        "  movl %esp, _zax_escape_esp\n"
        "  pushl 40(%esp)\n"   // a3 (return address at 16, fn 20, ecx 24, edx 28, a1 32, a2 36, a3 40)
        "  pushl 40(%esp)\n"   // a2
        "  pushl 40(%esp)\n"   // a1
        "  movl 36(%esp), %ecx\n"
        "  movl 40(%esp), %edx\n"
        "  call *32(%esp)\n"   // fastcall, removes its three stack arguments
        "  popl %edi\n  popl %esi\n  popl %ebx\n  popl %ebp\n  ret\n");

static unsigned text_hash(unsigned h, const char *s) { return s ? fnv_text(h, s) : fnv(h, "\x01null", 5); }

// ---- A sink of our own that records everything ---------------------------------------------------

struct RecordSink {
    DWORD vftable;
    DWORD count;
    unsigned hash;
    DWORD contains_calls;
};
static char g_rec_names[4096][100];
static DWORD THISCALL rec_count(RecordSink *s) { return s->count; }
static void THISCALL rec_add(RecordSink *s, const char *name, const ZFileRecord *r)
{
    if (s->count < 4096) {
        int m = 0;
        for (; name[m] && m < 99; m++)
            g_rec_names[s->count][m] = name[m];
        g_rec_names[s->count][m] = 0;
    }
    s->hash = fnv(fnv_text(s->hash, name), r, sizeof *r);
    s->count++;
}
static bool THISCALL rec_contains(RecordSink *s, const char *name)
{
    s->contains_calls++;
    for (DWORD i = 0; i < s->count && i < 4096; i++)
        if (crt_stricmp(g_rec_names[i], name) == 0)
            return true;
    return false;
}
static DWORD g_rec_vtable[3];
static void init_sink(RecordSink *s)
{
    g_rec_vtable[0] = (DWORD)(uintptr_t)&rec_count;
    g_rec_vtable[1] = (DWORD)(uintptr_t)&rec_add;
    g_rec_vtable[2] = (DWORD)(uintptr_t)&rec_contains;
    *s = {(DWORD)(uintptr_t)g_rec_vtable, 0, 2166136261u, 0};
}
static unsigned sink_hash(const RecordSink &s) { return fnv(fnv(s.hash, &s.count, 4), &s.contains_calls, 4); }

// ---- File spec -----------------------------------------------------------------------------------

struct SpecState {
    DWORD dwords[6];
    StringState ext[5];
};
static void spec_state(SpecState *st, const ZFileSpec *s)
{
    memset(st, 0, sizeof *st);
    memcpy(st->dwords, s, 0x18);
    for (int k = 0; k < 5; k++) {
        StringState e = state(s->ext[k], nullptr);
        memcpy(&st->ext[k], &e, sizeof e);
    }
}
static void make_spec(ZFileSpec *s) { original<ZFileSpec *(THISCALL *)(ZFileSpec *)>(0x4a09f0)(s); }
static void free_spec(ZFileSpec *s)
{
    StringOps o = original_string_ops();
    for (int k = 4; k >= 0; k--)
        o.dtor(&s->ext[k]);
}
template <typename F> static F pick(int v, DWORD address) { return v ? at<F>(address) : original<F>(address); }

static void test_spec(Tally &t)
{
    char d[200];
    // Constructor and Init on prefilled objects
    {
        BYTE obj[2][0x2c];
        DWORD r[2];
        SpecState st[2];
        for (int v = 0; v < 2; v++) {
            memset(obj[v], 0xcd, sizeof obj[v]);
            r[v] = (DWORD)(uintptr_t)pick<ZFileSpec *(THISCALL *)(void *)>(v, 0x4a09f0)(obj[v]) - (DWORD)(uintptr_t)obj[v];
            spec_state(&st[v], reinterpret_cast<ZFileSpec *>(obj[v]));
            free_spec(reinterpret_cast<ZFileSpec *>(obj[v]));
        }
        t.check(memcmp(st, st + 1, sizeof st[0]) == 0 && r[0] == r[1], "spec", "[constructor]");
        for (int v = 0; v < 2; v++) {
            memset(obj[v], 0xcd, sizeof obj[v]);
            r[v] = pick<DWORD(THISCALL *)(void *)>(v, 0x4a09d0)(obj[v]);
        }
        t.check(memcmp(obj[0], obj[1], 0x2c) == 0 && r[0] == r[1], "spec", "[Init]");
    }
    const char *exts[] = {"wav", ".ogg", "", ".", "..x", "A very long extension that needs a bigger buffer"};
    const char *folders[] = {"Sounds", "Levels/ACT 1/", ""};
    for (const char *f : folders)
        for (int first = 0; first < 6; first++)
            for (int adds = 0; adds <= 7; adds++) {
                SpecState st[2];
                unsigned h[2];
                for (int v = 0; v < 2; v++) {
                    ZFileSpec s;
                    make_spec(&s);
                    s.mode = 7;
                    s.names_only = 3;
                    DWORD r = pick<DWORD(THISCALL *)(ZFileSpec *, const char *, const char *)>(v, 0x4a0a60)(&s, f, exts[first]);
                    h[v] = fnv(2166136261u, &r, 4);
                    for (int k = 0; k < adds; k++) {
                        r = pick<DWORD(THISCALL *)(ZFileSpec *, const char *)>(v, 0x4a0a30)(&s, exts[(first + k) % 6]);
                        r -= (DWORD)(uintptr_t)&s;   // the ZString assigned last, or the count
                        h[v] = fnv(h[v], &r, 4);
                    }
                    for (DWORD k = 0; k < s.count; k++)
                        h[v] = text_hash(h[v], pick<const char *(THISCALL *)(ZFileSpec *, DWORD)>(v, 0x4a0b30)(&s, k));
                    r = pick<DWORD(THISCALL *)(ZFileSpec *)>(v, 0x4a0b40)(&s);
                    h[v] = fnv(h[v], &r, 4);
                    spec_state(&st[v], &s);
                    free_spec(&s);
                }
                wsprintfA(d, "[Set/AddExtension %s %d %d]", f, first, adds);
                t.check(memcmp(st, st + 1, sizeof st[0]) == 0 && h[0] == h[1], "spec", d);
            }
    for (const char *f : folders)
        for (int dashed = 0; dashed < 2; dashed++) {
            SpecState st[2];
            unsigned h[2];
            for (int v = 0; v < 2; v++) {
                ZFileSpec s;
                make_spec(&s);
                original<DWORD(THISCALL *)(ZFileSpec *, const char *)>(0x4a0a30)(&s, "old");
                original<DWORD(THISCALL *)(ZFileSpec *, const char *)>(0x4a0a30)(&s, "older");
                DWORD r = pick<DWORD(THISCALL *)(ZFileSpec *, const char *, int)>(v, 0x4a0aa0)(&s, f, dashed ? 0x102 : 0x100);
                h[v] = fnv(2166136261u, &r, 4);
                r = (DWORD)(uintptr_t)pick<const char *(THISCALL *)(ZFileSpec *, const char *)>(v, 0x4a0b10)(&s, "other");
                h[v] = fnv(h[v], &r, 4);
                r = pick<DWORD(THISCALL *)(ZFileSpec *, DWORD)>(v, 0x4a0b20)(&s, 5 + dashed);
                h[v] = fnv(h[v], &r, 4);
                spec_state(&st[v], &s);
                free_spec(&s);
            }
            wsprintfA(d, "[SetFolders %s %d]", f, dashed);
            t.check(memcmp(st, st + 1, sizeof st[0]) == 0 && h[0] == h[1], "spec", d);
        }
}

// ---- Sinks ---------------------------------------------------------------------------------------

static void test_sinks(Tally &t)
{
    char d[200];
    const char *names[] = {"a.wav", "A.WAV", "b.wav", "Sounds/a.wav", "", "b.WAV", "c", "a.wav"};
    // AddIfNew on a recording sink and on the counter
    for (int n = 0; n <= 8; n++) {
        unsigned h[2];
        DWORD counts[2];
        for (int v = 0; v < 2; v++) {
            RecordSink s;
            init_sink(&s);
            ZFileSink c{FILE_COUNTER_VTABLE, 0};
            h[v] = 2166136261u;
            for (int k = 0; k < n; k++) {
                auto fn = pick<bool(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD)>(v, 0x49d3a0);
                bool r = fn(&s, names[k], 100u * k, 0xabcd0000u + k, 0x12340000u + 7u * k);
                bool rc = fn(&c, names[k], k, k, k);
                h[v] = fnv(fnv(h[v], &r, 1), &rc, 1);
                pick<void(THISCALL *)(void *, const char *, const void *)>(v, 0x49d400)(&c, names[k], nullptr);
            }
            h[v] = fnv(h[v], &c, 8);
            counts[v] = s.count;
            h[v] = fnv(h[v], &counts[v], 4) ^ sink_hash(s);
        }
        wsprintfA(d, "[AddIfNew %d]", n);
        t.check(h[0] == h[1], "sink", d);
    }
    // The name buffer: constructor, Add, Contains with and without names/values, full, stride
    for (int variant = 0; variant < 4; variant++)
        for (DWORD max = 0; max <= 9; max += 3)
            for (DWORD stride = 8; stride <= 24; stride += 16) {
                static BYTE nbuf[2][400], values[2][400], obj[2][0x20];
                unsigned h[2];
                for (int v = 0; v < 2; v++) {
                    memset(nbuf[v], 0xee, sizeof nbuf[v]);
                    memset(values[v], 0xdd, sizeof values[v]);
                    memset(obj[v], 0xcd, sizeof obj[v]);
                    char *np = variant & 1 ? nullptr : (char *)nbuf[v];
                    BYTE *vp = variant & 2 ? nullptr : values[v];
                    auto *b = reinterpret_cast<ZFileNameBuffer *>(obj[v]);
                    DWORD r = (DWORD)(uintptr_t)pick<void *(THISCALL *)(void *, DWORD, char *, BYTE *, DWORD, DWORD)>(
                                  v, 0x49d410)(b, stride == 24 ? 23 : 0, np, vp, max, stride) -
                              (DWORD)(uintptr_t)b;
                    h[v] = fnv(2166136261u, &r, 4);
                    for (int k = 0; k < 8; k++) {
                        DWORD value = 0x1000 + k;
                        if (np || !b->index) {
                            bool c = pick<bool(THISCALL *)(void *, const char *)>(v, 0x49d450)(b, names[k]);
                            h[v] = fnv(h[v], &c, 1);
                        }
                        r = pick<DWORD(THISCALL *)(void *, const char *, const DWORD *)>(v, 0x49d500)(b, names[k], &value);
                        h[v] = fnv(h[v], &r, 4);
                    }
                    BYTE copy[0x20];
                    memcpy(copy, obj[v], 0x20);
                    memset(copy + 0x14, 0, 8);   // the buffer addresses differ
                    h[v] = fnv(fnv(fnv(h[v], copy, 0x20), nbuf[v], 400), values[v], 400);
                }
                wsprintfA(d, "[name buffer variant %d max %u stride %u]", variant, max, stride);
                t.check(h[0] == h[1], "sink", d);
            }
    // A name longer than max_length ends the game: message box and exit jump to stand-ins
    CodePatch box, exit;
    patch_jump(box, 0x4d24c0, (void *)&capture_box);
    patch_jump(exit, 0x5d0e36, (void *)&zax_escape_exit);
    for (DWORD max_length = 1; max_length <= 9; max_length += 4) {
        unsigned h[2];
        for (int v = 0; v < 2; v++) {
            static char buf[400];
            memset(buf, 0, sizeof buf);
            ZFileNameBuffer b;
            original<void *(THISCALL *)(void *, DWORD, char *, BYTE *, DWORD, DWORD)>(0x49d410)(&b, max_length, buf, nullptr, 10, 20);
            g_box_text[0] = 0;
            g_box_caption = nullptr;
            zax_exit_code = 0;
            DWORD value = 5;
            void *fn = (void *)(v ? at<void *>(0x49d500) : original<void *>(0x49d500));
            int r = zax_call_escapable(fn, &b, "longname.wav", (int)(uintptr_t)&value);
            h[v] = fnv(fnv(fnv(2166136261u, &r, 4), &zax_exit_code, 4), &b, 0x14);
            h[v] = text_hash(text_hash(fnv_text(h[v], g_box_text), g_box_caption), buf);
        }
        wsprintfA(d, "[name too long %u]", max_length);
        t.check(h[0] == h[1], "sink", d);
    }
    unpatch(exit);
    unpatch(box);
}

// ---- Search on disk ------------------------------------------------------------------------------

static char g_root[MAX_PATH], g_tilde[MAX_PATH];
static const char *const TREE_DIRS[] = {"sub1", "sub1\\deep", "-dash", "cache", "Sub2", "sub3", "-dash\\inner", "Cache2"};
static const char *const TREE_FILES[] = {
    "a.wav",          "B.WAV",          "c.ogg",       "noext",           "x.y.wav",       "-d.wav",
    ".hidden.wav",    "i.zip",          "sub1\\d.wav", "sub1\\deep\\e.wav", "sub1\\deep\\f.ogg", "sub1\\k.zip",
    "-dash\\g.wav",   "-dash\\j.zip",   "cache\\h.zip", "cache\\i.wav",   "sub3\\a.wav",   "-dash\\inner\\m.wav",
    "Cache2\\n.zip"};

static void join(char *out, const char *dir, const char *name)
{
    lstrcpyA(out, dir);
    lstrcatA(out, name);
}
static void make_file(const char *dir, const char *name, DWORD size)
{
    char p[MAX_PATH], data[64];
    join(p, dir, name);
    memset(data, 'z', sizeof data);
    HANDLE f = CreateFileA(p, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD w;
    WriteFile(f, data, size % 64, &w, nullptr);
    CloseHandle(f);
}
static bool build_tree()
{
    char tmp[MAX_PATH], p[MAX_PATH];
    if (!GetTempPathA(MAX_PATH - 40, tmp))
        return false;
    join(g_root, tmp, "ZaxFileSearch\\");
    join(g_tilde, tmp, "ZaxFileSearchTilde\\");
    CreateDirectoryA(g_root, nullptr);
    CreateDirectoryA(g_tilde, nullptr);
    for (const char *dname : TREE_DIRS) {
        join(p, g_root, dname);
        CreateDirectoryA(p, nullptr);
    }
    DWORD size = 3;
    for (const char *f : TREE_FILES)
        make_file(g_root, f, size += 7);
    make_file(g_tilde, "ok.wav", 1);
    make_file(g_tilde, "bad~1.wav", 2);
    return true;
}
static void remove_tree()
{
    char p[MAX_PATH];
    for (const char *f : TREE_FILES) {
        join(p, g_root, f);
        DeleteFileA(p);
    }
    for (int k = (int)(sizeof TREE_DIRS / sizeof *TREE_DIRS) - 1; k >= 0; k--) {
        join(p, g_root, TREE_DIRS[k]);
        RemoveDirectoryA(p);
    }
    RemoveDirectoryA(g_root);
    join(p, g_tilde, "ok.wav");
    DeleteFileA(p);
    join(p, g_tilde, "bad~1.wav");
    DeleteFileA(p);
    RemoveDirectoryA(g_tilde);
}

struct SearchDef {
    const char *folder;   // nullptr: the test tree; "~": the test tree without the final separator
    const char *ext1, *ext2;
    DWORD mode, folders, names_only;
    bool dashed;
    DWORD flags;
};

static void setup_spec(ZFileSpec *s, const SearchDef &sd, char *folder)
{
    make_spec(s);
    if (!sd.folder)
        lstrcpyA(folder, g_root);
    else if (sd.folder[0] == '~' && !sd.folder[1]) {
        lstrcpyA(folder, g_root);
        folder[lstrlenA(folder) - 1] = 0;
    } else
        lstrcpyA(folder, sd.folder);
    if (sd.folders)
        original<DWORD(THISCALL *)(ZFileSpec *, const char *, int)>(0x4a0aa0)(s, folder, sd.dashed);
    else
        original<DWORD(THISCALL *)(ZFileSpec *, const char *, const char *)>(0x4a0a60)(s, folder, sd.ext1);
    if (sd.ext2)
        original<DWORD(THISCALL *)(ZFileSpec *, const char *)>(0x4a0a30)(s, sd.ext2);
    s->mode = sd.mode;
    s->names_only = sd.names_only;
}

static int g_files_found;

static void compare_search(Tally &t, const SearchDef &sd, int kind)
{
    // kind 0 Files_Search into a recording sink, 1 Files_Count, 2 Files_ListNames, 3..8 Files_SearchDisk
    // with strip_ext 0/1/2 and folder_ext "wav"/"zip"/null
    char d[220];
    unsigned h[2];
    for (int v = 0; v < 2; v++) {
        static char folder[MAX_PATH];
        ZFileSpec s;
        setup_spec(&s, sd, folder);
        RecordSink sink;
        init_sink(&sink);
        memset(at<char *>(FILE_PATH_BUFFER), 0, 0x100);
        DWORD r = 0;
        if (kind == 0) {
            r = pick<DWORD(__fastcall *)(void *, const ZFileSpec *, DWORD)>(v, 0x4a1190)(&sink, &s, sd.flags);
        } else if (kind == 1) {
            r = pick<DWORD(__fastcall *)(const ZFileSpec *, DWORD)>(v, 0x4a12d0)(&s, sd.flags);
        } else if (kind == 2) {
            static char names[64 * 100];
            static BYTE values[64 * 100];
            memset(names, 0xee, sizeof names);
            memset(values, 0xdd, sizeof values);
            r = pick<DWORD(__fastcall *)(const ZFileSpec *, char *, BYTE *, DWORD, DWORD, DWORD)>(v, 0x4a1150)(
                &s, names, values, 100, 60, sd.flags);
            sink.hash = fnv(fnv(sink.hash, names, sizeof names), values, sizeof values);
        } else {
            static const char *const fe[] = {"wav", "zip", nullptr};
            DWORD strip = (DWORD)(kind - 3) / 3 ? 1 : 0;
            if (kind == 8)
                strip = 2;
            const char *f = fe[(kind - 3) % 3];
            r = pick<DWORD(__fastcall *)(void *, const ZFileSpec *, DWORD, const char *, DWORD)>(v, 0x4a4840)(
                &sink, &s, strip, f, kind & 1 ? 0 : 3);
        }
        if (!v && kind == 0)
            g_files_found += (int)sink.count;
        h[v] = fnv(sink_hash(sink), &r, 4);
        h[v] = fnv_text(h[v], at<char *>(FILE_PATH_BUFFER));
        free_spec(&s);
    }
    wsprintfA(d, "[%s %s %s mode %u folders %u names %u dashed %d flags %x kind %d]", sd.folder ? sd.folder : "tree",
              sd.ext1 ? sd.ext1 : "-", sd.ext2 ? sd.ext2 : "-", sd.mode, sd.folders, sd.names_only, sd.dashed, sd.flags,
              kind);
    t.check(h[0] == h[1], "search", d);
}

static void test_disk(Tally &t)
{
    if (!build_tree()) {
        log("  file search: no temp folder");
        t.check(false, "search", "[temp folder]");
        return;
    }
    const char *exts[][2] = {{"wav", nullptr}, {"zip", nullptr}, {"ZIP", nullptr}, {"", nullptr}, {"wav", "ogg"}, {"*", nullptr}};
    const char *tree_folders[] = {nullptr, "~"};
    for (const char *folder : tree_folders)
        for (auto &e : exts)
            for (DWORD mode = folder ? 2 : 0; mode < 3; mode++)   // without the separator: no recursion through the temp folder
                for (DWORD names_only = 0; names_only < 3; names_only += 2) {
                    SearchDef sd{folder, e[0], e[1], mode, 0, names_only, false, 0x20};
                    for (int kind = 0; kind < 9; kind++)
                        compare_search(t, sd, kind);
                }
    for (DWORD mode = 0; mode < 3; mode++)
        for (DWORD names_only = 0; names_only < 2; names_only++)
            for (int dashed = 0; dashed < 2; dashed++) {
                SearchDef sd{nullptr, nullptr, nullptr, mode, 1, names_only, dashed != 0, 0x20};
                for (int kind = 0; kind < 9; kind++)
                    compare_search(t, sd, kind);
            }
    // Real game folders below "." (and the archives, if mounted)
    SearchDef real[] = {
        {"Sounds", "wav", nullptr, 1, 0, 0, false, 0},       {"Sounds/", ".wav", nullptr, 0, 0, 1, false, 0},
        {"Sounds/AMBIENT/", "wav", nullptr, 1, 0, 0, false, 0}, {"Music", "ogg", "wav", 1, 0, 0, false, 0},
        {".", "RED", nullptr, 2, 0, 0, false, 0},           {".", "ZIP", nullptr, 1, 0, 1, false, 0},
        {"Levels", "zax", nullptr, 1, 0, 0, false, 0},      {"Levels", nullptr, nullptr, 1, 1, 0, false, 0},
        {"Levels/", nullptr, nullptr, 2, 1, 1, false, 0},   {"Levels", nullptr, nullptr, 0, 1, 0, true, 0},
        {"Cache/Fonts", "seq16", "seq", 1, 0, 0, false, 0}, {"Does Not Exist", "wav", nullptr, 1, 0, 0, false, 0},
        {"", "txt", nullptr, 2, 0, 0, false, 0},            {"Characters", "mdl16", nullptr, 1, 0, 1, false, 0},
    };
    for (const SearchDef &sd : real)
        for (int kind = 0; kind < 3; kind++)
            compare_search(t, sd, kind);
    // A name with '~' ends the game: message box and exit jump to stand-ins
    CodePatch box, exit;
    patch_jump(box, 0x4d24c0, (void *)&capture_box);
    patch_jump(exit, 0x5d0e36, (void *)&zax_escape_exit);
    for (DWORD names_only = 0; names_only < 2; names_only++) {
        unsigned h[2];
        for (int v = 0; v < 2; v++) {
            ZFileSpec s;
            make_spec(&s);
            original<DWORD(THISCALL *)(ZFileSpec *, const char *, const char *)>(0x4a0a60)(&s, g_tilde, "wav");
            s.names_only = names_only;
            RecordSink sink;
            init_sink(&sink);
            g_box_text[0] = 0;
            g_box_caption = nullptr;
            zax_exit_code = 0;
            void *fn = (void *)(v ? at<void *>(0x4a4840) : original<void *>(0x4a4840));
            int r = zax_call_escapable_fast(fn, (DWORD)(uintptr_t)&sink, (DWORD)(uintptr_t)&s, 0, 0, 0);
            h[v] = fnv(fnv(fnv(2166136261u, &r, 4), &zax_exit_code, 4), &sink.count, 4);
            h[v] = text_hash(fnv_text(h[v], g_box_text), g_box_caption);
            free_spec(&s);
        }
        t.check(h[0] == h[1], "search", names_only ? "[tilde, names only]" : "[tilde]");
    }
    unpatch(exit);
    unpatch(box);
    remove_tree();
}

// ---- Folder tree of the archives -----------------------------------------------------------------

static DWORD g_list_dir_calls;
static unsigned g_list_dir_hash;
static void THISCALL fake_list_dir(void *archive, ZFileSink *sink, const ZFileSpec *spec)
{
    g_list_dir_calls++;
    g_list_dir_hash = fnv(fnv(fnv(g_list_dir_hash, &archive, 4), &sink, 4), &spec, 4);
}

struct TreeDef {
    int parent;   // index in the table, -1: a root
    const char *name;
    DWORD archive;
};
static const TreeDef TREE[] = {
    {-1, "", 0xa1},       {0, "Sounds", 0},   {1, "AMBIENT", 0xa2}, {2, "Deep", 0},     {1, "-hidden", 0},
    {4, "x", 0},          {0, "Levels", 0},   {6, "ACT 1", 0},      {6, "act 2", 0xa5}, {0, "SoundsX", 0},
    {0, "-dash", 0},      {-1, "", 0},        {11, "Sounds", 0xa3}, {12, "AMBIENT", 0}, {11, "Music", 0},
    {-1, "Data", 0xa4},   {15, "Levels", 0},  {16, "-old", 0},
};
constexpr int TREE_N = sizeof TREE / sizeof *TREE;
static ZZipNode g_nodes[TREE_N];
static void *g_children[TREE_N][TREE_N];
static void *g_roots[TREE_N];
static int g_num_roots;

static void build_zip_tree()
{
    g_num_roots = 0;
    for (int i = 0; i < TREE_N; i++) {
        ZZipNode &n = g_nodes[i];
        n.parent = TREE[i].parent >= 0 ? &g_nodes[TREE[i].parent] : nullptr;
        original<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&n.name, TREE[i].name);
        n.children = {0x6060c4, g_children[i], 0, TREE_N};
        n.archive = (void *)(uintptr_t)TREE[i].archive;
        if (n.parent)
            n.parent->children.data[n.parent->children.count++] = &n;
        else
            g_roots[g_num_roots++] = &n;
    }
}
static void free_zip_tree()
{
    for (int i = 0; i < TREE_N; i++)
        original_string_ops().dtor(&g_nodes[i].name);
}

static const DWORD FIND_FUNCTIONS[] = {0x5c1890, 0x5c17b0};

static void test_zip_tree(Tally &t)
{
    char d[220];
    build_zip_tree();
    const char *paths[] = {nullptr,          "",
                           "/",              "Sounds",
                           "Sounds/",        "sounds/",
                           "SOUNDS/ambient/", "Sounds/AMBIENT",
                           "Sounds/AMBIENT/", "Sounds/AMBIENT/Deep/",
                           "Sounds/AMBIENT/Deep", "Sounds/AMBIENT/deep/x/",
                           "Sounds/Nope/",   "Sounds//",
                           "Levels/ACT 1/",  "Levels/act 2/",
                           "Levels/act 2/z/", "Levels/act 1/x/",
                           "Sound/",         "SoundsX/",
                           "a/b/c/",         "Sounds/AMBIENTX/",
                           "Sounds/AMBIEN/", "Music/",
                           "Data/Levels/",   "Levels/-old/",
                           "-dash/",         "Sounds/-hidden/x/",
                           "Sounds/-hidden/x", "Levels/ACT 1"};
    ZZipList fake{};
    fake.vftable = 0x6060b8;
    fake.data = g_roots;
    fake.count = (DWORD)g_num_roots;
    fake.capacity = (DWORD)g_num_roots;
    // The searches by path, from every node
    for (int i = 0; i < TREE_N; i++)
        for (const char *p : paths)
            for (DWORD address : FIND_FUNCTIONS) {
                ZZipNode *r[2];
                for (int v = 0; v < 2; v++)
                    r[v] = pick<ZZipNode *(THISCALL *)(ZZipList *, const char *, ZZipNode *)>(v, address)(&fake, p, &g_nodes[i]);
                wsprintfA(d, "[0x%x node %d %s: %d %d]", address, i, p ? p : "null", r[0] ? (int)(r[0] - g_nodes) : -1,
                          r[1] ? (int)(r[1] - g_nodes) : -1);
                t.check(r[0] == r[1], "zip", d);
            }
    // List on every node and Search on the list, Zip_ListDir redirected
    CodePatch list_dir;
    patch_jump(list_dir, 0x5c2970, (void *)&fake_list_dir);
    const char *prefixes[] = {"", "-", "so", "LEV"};
    for (DWORD mode = 0; mode < 3; mode++)
        for (DWORD folders = 0; folders < 2; folders++)
            for (DWORD names_only = 0; names_only < 3; names_only++)
                for (const char *prefix : prefixes) {
                    for (int i = 0; i <= TREE_N; i++)
                        for (const char *p : paths) {
                            if (i < TREE_N ? p != paths[4] : !p)
                                continue;   // List does not use the folder; Search with every path
                            unsigned h[2];
                            for (int v = 0; v < 2; v++) {
                                ZFileSpec s;
                                make_spec(&s);
                                s.mode = mode;
                                s.folders = folders;
                                s.names_only = names_only;
                                s.prefix = prefix;
                                s.folder = p;
                                RecordSink sink;
                                init_sink(&sink);
                                g_list_dir_calls = 0;
                                g_list_dir_hash = 2166136261u;
                                if (i < TREE_N)
                                    pick<void(THISCALL *)(ZZipNode *, void *, const ZFileSpec *)>(v, 0x5c13c0)(&g_nodes[i], &sink, &s);
                                else
                                    pick<void(THISCALL *)(ZZipList *, void *, const ZFileSpec *)>(v, 0x5c1da0)(&fake, &sink, &s);
                                h[v] = fnv(fnv(sink_hash(sink), &g_list_dir_calls, 4), &g_list_dir_hash, 4);
                                free_spec(&s);
                            }
                            wsprintfA(d, "[%s %d %s mode %u folders %u names %u prefix %s]", i < TREE_N ? "List" : "Search", i, p,
                                      mode, folders, names_only, prefix);
                            t.check(h[0] == h[1], "zip", d);
                        }
                }
    unpatch(list_dir);
    free_zip_tree();
    // The getter on first use (atexit redirected) and afterwards
    {
        SavedBytes saved;
        save_bytes(saved, ZIP_LIST_GUARD, 0x18);
        CodePatch atexit_patch;
        patch_jump(atexit_patch, 0x5d0863, (void *)&fake_atexit);
        BYTE after[2][0x18];
        DWORD r[2], calls[2], fns[2];
        for (int v = 0; v < 2; v++) {
            restore_bytes(saved);
            memset(at<BYTE *>(ZIP_LIST_GUARD), 0xcd, 0x18);
            *at<BYTE *>(ZIP_LIST_GUARD) = 0xfe;
            g_atexit_calls = 0;
            g_atexit_function = 0;
            r[v] = (DWORD)(uintptr_t)pick<ZZipList *(__cdecl *)()>(v, 0x5c1650)();
            calls[v] = g_atexit_calls;
            fns[v] = g_atexit_function;
            memcpy(after[v], at<BYTE *>(ZIP_LIST_GUARD), 0x18);
        }
        restore_bytes(saved);
        unpatch(atexit_patch);
        t.check(memcmp(after[0], after[1], 0x18) == 0 && r[0] == r[1] && calls[0] == calls[1] && fns[0] == fns[1], "zip",
                "[Get on first use]");
        compare_call<ZZipList *(__cdecl *)()>(t, 0x5c1650, "Get");
    }
    // The mounted archives
    ZZipList *zips = at<ZZipList *>(ZIP_LIST);
    log("  mounted archives: loaded %u, %u roots", *at<BYTE *>(ZIP_LOADED), (*at<BYTE *>(ZIP_LIST_GUARD) & 1) ? zips->count : 0);
    for (DWORD i = 0; (*at<BYTE *>(ZIP_LIST_GUARD) & 1) && i < zips->count && i < 8; i++) {
        auto *n = static_cast<ZZipNode *>(zips->data[i]);
        char names[160];
        names[0] = 0;
        for (DWORD k = 0; k < n->children.count && lstrlenA(names) < 120; k++) {
            lstrcatA(names, static_cast<ZZipNode *>(n->children.data[k])->name.c_str());
            lstrcatA(names, " ");
        }
        log("  archive %u: name \"%s\", archive %s, %u children: %s", i, n->name.c_str(), n->archive ? "yes" : "no",
            n->children.count, names);
    }
    if ((*at<BYTE *>(ZIP_LIST_GUARD) & 1) && zips->count) {
        SearchDef real[] = {{"Sounds", "wav", nullptr, 1, 0, 0, false, 0},  {"Levels", nullptr, nullptr, 1, 1, 0, false, 0},
                            {"Levels/", nullptr, nullptr, 0, 1, 1, true, 0}, {"", "txt", nullptr, 2, 0, 0, false, 0},
                            {"Text", "RDS", nullptr, 1, 0, 1, false, 0},     {"Cache/Fonts", "seq16", nullptr, 1, 0, 0, false, 0}};
        for (const SearchDef &sd : real) {
            unsigned h[2];
            for (int v = 0; v < 2; v++) {
                static char folder[MAX_PATH];
                ZFileSpec s;
                setup_spec(&s, sd, folder);
                RecordSink sink;
                init_sink(&sink);
                pick<void(THISCALL *)(ZZipList *, void *, const ZFileSpec *)>(v, 0x5c1da0)(zips, &sink, &s);
                h[v] = sink_hash(sink);
                if (!v)
                    log("  archives %s: %u names", sd.folder, sink.count);
                free_spec(&s);
            }
            wsprintfA(d, "[mounted archives %s]", sd.folder);
            t.check(h[0] == h[1], "zip", d);
        }
    }
}

ZAX_CHECKS(file_search)
{
    (void)cls;
    (void)n;
    Tally spec{"ZFileSpec"}, sinks{"File sinks"}, disk{"File search"}, zip{"Zip folders"};
    test_spec(spec);
    test_sinks(sinks);
    test_disk(disk);
    test_zip_tree(zip);
    log("  file search: %d files found in the searches into a recording sink", g_files_found);
    spec.report();
    sinks.report();
    disk.report();
    zip.report();
}

}  // namespace zax
