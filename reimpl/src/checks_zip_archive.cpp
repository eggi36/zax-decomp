// Checks of the zip archives (zip_archive.h): test archives of our own in the temp folder (stored zips in
// several variants and broken files), the directory (Load, Find, ListDir, Exists, Close), the folder tree
// (Mount, Open, Exists, the destructors) and mounting at start with the global list swapped out.
#include "check_util.h"
#include "zip_archive.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT DWORD WINAPI GetTickCount(void);

namespace zax {

// Calls fn with ecx, edx and n stack arguments (callee cleans up: thiscall and fastcall); the exit
// stand-in zax_escape_exit returns from here with 0x7fff0001
extern "C" int __cdecl zax_call_escapable_regs(void *fn, DWORD ecx, DWORD edx, DWORD n, const DWORD *args);
__asm__(".text\n"
        ".globl _zax_call_escapable_regs\n"
        "_zax_call_escapable_regs:\n"
        "  pushl %ebp\n  pushl %ebx\n  pushl %esi\n  pushl %edi\n"
        "  movl %esp, _zax_escape_esp\n"
        "  movl 32(%esp), %eax\n"   // n (return address at 16, fn 20, ecx 24, edx 28, n 32, args 36)
        "  movl 36(%esp), %esi\n"
        "  movl 20(%esp), %ebx\n"
        "  movl 24(%esp), %ecx\n"
        "  movl 28(%esp), %edx\n"
        "1:\n  testl %eax, %eax\n  jz 2f\n  decl %eax\n  pushl (%esi,%eax,4)\n  jmp 1b\n"
        "2:\n  call *%ebx\n"
        "  popl %edi\n  popl %esi\n  popl %ebx\n  popl %ebp\n  ret\n");

static unsigned text_hash(unsigned h, const char *s) { return s ? fnv_text(h, s) : fnv(h, "\x01null", 5); }
template <typename F> static F pick(int v, DWORD address) { return v ? at<F>(address) : original<F>(address); }
static DWORD file_tell(void *f) { return at<DWORD(THISCALL *)(void *)>(0x4a2180)(f); }
static void join(char *out, const char *a, const char *b)
{
    lstrcpyA(out, a);
    lstrcatA(out, b);
}

// ---- Test archives -------------------------------------------------------------------------------

static const char *const NAMES_A[] = {
    "Sounds/Ambient/wind.wav", "Sounds/zap.WAV", "Sounds/-Old/beep.wav", "Sounds/-Old/Deep/x.wav", "Sounds/readme.txt",
    "sounds/Lower.wav", "Levels/ACT 1/M1/01 Main.zax", "Levels/ACT 1/M1/01 Main.way", "Levels/ACT 1/-Test/t.zax",
    "Levels/act 1/M2/02.zax", "Levels/Readme", "Rules.txt", "-hidden.txt", "a.wav", "B.wav", "Cache/Fonts/f.seq16",
    "Cache/Fonts/f.seq", "Cache/Textures/-x/t.frm16", "Characters/Crater/Crater.mdl16", "Characters/Crater/Crater Torso.mdl16",
    "X/Y/Z/deep.wav", "X/Y/Z/", "X/-Y/q.wav", "wav", "Sounds/Ambient/", "Sounds/Ambient/rain.wav", "Sounds/Ambient/Rain.ogg",
    "Music/track01.mp3", "Music/-old/track00.mp3", "zz/last.wav",
};
static const char *const NAMES_B[] = {"a.txt", "Sounds/b.wav", "c/d/e.zax", "-z/y.wav", "Sounds/-q/r.wav", "A.TXT2", "c/f.zax", "zz.wav"};
static const char *const NAMES_C[] = {"one.wav", "Two/three.wav", "two/Four.txt"};
static const char *const NAMES_1[] = {"single.txt"};
constexpr int N_A = sizeof NAMES_A / sizeof *NAMES_A, N_B = sizeof NAMES_B / sizeof *NAMES_B,
              N_C = sizeof NAMES_C / sizeof *NAMES_C;

enum ZipKind { PLAIN, RE_SIGNATURE, COMMENT, NO_OFFSET, FEWER, COMPRESSED, GARBAGE, TINY_END, TINY_LOCAL, EMPTY_FILE };

static BYTE g_buf[32768];
static DWORD g_len;
static void put(const void *p, DWORD n)
{
    memcpy(g_buf + g_len, p, n);
    g_len += n;
}
static void put16(DWORD v) { put(&v, 2); }
static void put32(DWORD v) { put(&v, 4); }

static void write_file(const char *path)
{
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD w;
    WriteFile(f, g_buf, g_len, &w, nullptr);
    CloseHandle(f);
}

// A stored zip with the names (data: k + 3 bytes), central directory and end record
static void make_zip(const char *path, const char *const *names, int n, ZipKind kind)
{
    g_len = 0;
    if (kind == GARBAGE) {
        put("This is not a zip archive at all, only some text without any signature in it.", 79);
    } else if (kind == TINY_END) {
        put32(ZIP_END);
        put16(0);
    } else if (kind == TINY_LOCAL) {
        put32(ZIP_LOCAL);
        put32(0x0a);
    }
    if (kind >= GARBAGE) {
        write_file(path);
        return;
    }
    DWORD offsets[64];
    for (int k = 0; k < n; k++) {
        DWORD length = (DWORD)lstrlenA(names[k]), size = (DWORD)k + 3;
        offsets[k] = g_len;
        put32(ZIP_LOCAL);
        put16(10);
        put16(0);
        put16(kind == COMPRESSED && k == 2 ? 8 : 0);
        put16(0x4000 + k * 37);
        put16(0x2b00 + k * 3);
        put32(0x12345678u * (k + 1));
        put32(size);
        put32(size);
        put16(length);
        put16(0);
        put(names[k], length);
        for (DWORD b = 0; b < size; b++)
            put(&"abcdefghij"[b % 10], 1);
    }
    DWORD cd = g_len;
    for (int k = 0; k < n; k++) {
        DWORD length = (DWORD)lstrlenA(names[k]), size = (DWORD)k + 3, extra = k % 3 == 1 ? 4 : 0, comment = k % 4 == 2 ? 3 : 0;
        put32(ZIP_CENTRAL);
        put16(20);
        put16(10);
        put16(k & 1 ? 8 : 0);
        put16(kind == COMPRESSED && k == 2 ? 8 : 0);
        put16(0x4000 + k * 37);
        put16(0x2b00 + k * 3);
        put32(0x12345678u * (k + 1));
        put32(size);
        put32(size);
        put16(length);
        put16(extra);
        put16(comment);
        put16(0);
        put16(k & 1);
        put32(k & 2 ? 0x10 : 0x20);
        put32(offsets[k]);
        put(names[k], length);
        put("\x01\x02\x03\x04", extra);
        put("cmt", comment);
    }
    DWORD cd_size = g_len - cd, count = kind == FEWER ? (DWORD)n - 2 : (DWORD)n;
    put32(kind == RE_SIGNATURE ? ZIP_END_RE : ZIP_END);
    put16(0);
    put16(0);
    put16(count);
    put16(count);
    put32(cd_size);
    put32(kind == NO_OFFSET ? 0xffffffff : cd);
    put16(kind == COMMENT ? 5 : 0);
    if (kind == COMMENT)
        put("hello", 5);
    write_file(path);
}

static char g_root[MAX_PATH];
struct TestZip {
    const char *file;
    const char *const *names;
    int n;
    ZipKind kind;
};
static const TestZip ZIPS[] = {
    {"a.zip", NAMES_A, N_A, PLAIN},           {"re.dat", NAMES_A, N_A, RE_SIGNATURE},   {"comment.zip", NAMES_A, N_A, COMMENT},
    {"nooffset.zip", NAMES_B, N_B, NO_OFFSET}, {"fewer.zip", NAMES_A, N_A, FEWER},       {"packed.zip", NAMES_B, N_B, COMPRESSED},
    {"garbage.zip", nullptr, 0, GARBAGE},      {"tinyend.zip", nullptr, 0, TINY_END},    {"tinylocal.zip", nullptr, 0, TINY_LOCAL},
    {"emptyfile.zip", nullptr, 0, EMPTY_FILE}, {"empty.zip", nullptr, 0, PLAIN},         {"b.zip", NAMES_B, N_B, PLAIN},
    {"c.zip", NAMES_C, N_C, PLAIN},            {"single.zip", NAMES_1, 1, PLAIN},        {"Sub/s1.zip", NAMES_C, N_C, PLAIN},
    {"cache/skip.zip", NAMES_C, N_C, PLAIN},   {"-dash/d.zip", NAMES_B, N_B, PLAIN},     {"Sub/Deeper/s2.ZIP", NAMES_1, 1, PLAIN},
};
static const char *const DIRS[] = {"Sub", "Sub\\Deeper", "cache", "-dash"};
// Archives of the zip search of Zip_MountAll (the folder "zips")
static char g_zip_root[MAX_PATH];

static void zip_path(char *out, const char *file) { join(out, g_root, file); }
static bool build_zips()
{
    char tmp[MAX_PATH], p[MAX_PATH];
    if (!GetTempPathA(MAX_PATH - 40, tmp))
        return false;
    join(g_root, tmp, "ZaxZipArchive\\");
    CreateDirectoryA(g_root, nullptr);
    for (const char *d : DIRS) {
        zip_path(p, d);
        CreateDirectoryA(p, nullptr);
    }
    for (const TestZip &z : ZIPS) {
        zip_path(p, z.file);
        make_zip(p, z.names, z.n, z.kind);
    }
    join(g_zip_root, g_root, "Sub\\");
    return true;
}
static void remove_zips()
{
    char p[MAX_PATH];
    for (const TestZip &z : ZIPS) {
        zip_path(p, z.file);
        if (!DeleteFileA(p))
            log("  could not delete %s", p);
    }
    for (int k = (int)(sizeof DIRS / sizeof *DIRS) - 1; k >= 0; k--) {
        zip_path(p, DIRS[k]);
        RemoveDirectoryA(p);
    }
    RemoveDirectoryA(g_root);
}

// ---- State of directories and trees --------------------------------------------------------------

// Everything of a directory; entries only up to `complete` with their headers (the others by name)
static unsigned dir_hash(unsigned h, const ZZipDir *d, DWORD complete = 0xffffffff)
{
    if (!d)
        return fnv(h, "\x02nodir", 6);
    BYTE b[3] = {d->loaded, d->unknown01, d->keep_open};
    h = fnv(h, b, 3);
    DWORD v[9] = {d->file != nullptr, d->file ? file_tell(d->file) : 0, d->count, d->unknown1c, d->open_flags,
                  d->end_signature, d->check_disk, d->entries != nullptr, d->sorted != nullptr};
    h = fnv(h, v, sizeof v);
    h = text_hash(text_hash(h, d->path), d->prefix);
    if (d->entries) {
        DWORD n = reinterpret_cast<const DWORD *>(d->entries)[-1];
        h = fnv(h, &n, 4);
        for (DWORD i = 0; i < n; i++) {
            const ZZipEntry &e = d->entries[i];
            if (i < complete)   // the entry that failed has its name read but not yet terminated
                h = fnv(text_hash(h, e.name), &e.version_made, 0x2c);
            else
                h = fnv(h, e.name ? "\x04" : "\x05", 1);
        }
        if (d->sorted && d->count > 1 && complete == 0xffffffff)
            h = fnv(h, d->sorted, d->count * 4);
    }
    return h;
}

static unsigned node_hash(unsigned h, const ZZipNode *n, int depth)
{
    if (!n)
        return fnv(h, "\x03null", 5);
    h = text_hash(h, n->name.buffer ? n->name.c_str() : nullptr);
    DWORD v[4] = {n->children.vftable, n->children.count, n->children.capacity, n->parent != nullptr};
    h = fnv(h, v, sizeof v);
    h = dir_hash(h, static_cast<const ZZipDir *>(n->archive));
    for (DWORD i = 0; i < n->children.count && depth < 16; i++) {
        auto *c = static_cast<const ZZipNode *>(n->children.data[i]);
        bool parent_ok = c && c->parent == n;
        h = node_hash(fnv(h, &parent_ok, 1), c, depth + 1);
    }
    return h;
}

static unsigned list_hash(unsigned h, const ZPtrArray *l)
{
    DWORD v[4] = {l->vftable, l->count, l->capacity, l->data != nullptr};
    h = fnv(h, v, sizeof v);
    for (DWORD i = 0; i < l->count; i++) {
        auto *n = static_cast<const ZZipNode *>(l->data[i]);
        bool root = n && !n->parent;
        h = node_hash(fnv(h, &root, 1), n, 0);
    }
    return h;
}

// ---- A sink of our own that records everything ---------------------------------------------------

struct RecordSink {
    DWORD vftable;
    DWORD count;
    unsigned hash;
    DWORD contains_calls;
};
static char g_rec_names[512][100];
static DWORD THISCALL rec_count(RecordSink *s) { return s->count; }
static void THISCALL rec_add(RecordSink *s, const char *name, const ZFileRecord *r)
{
    if (s->count < 512) {
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
    for (DWORD i = 0; i < s->count && i < 512; i++)
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
static DWORD g_names_listed;

static void make_spec(ZFileSpec *s) { original<ZFileSpec *(THISCALL *)(ZFileSpec *)>(0x4a09f0)(s); }
static void free_spec(ZFileSpec *s)
{
    StringOps o = original_string_ops();
    for (int k = 4; k >= 0; k--)
        o.dtor(&s->ext[k]);
}

// ---- Directories ---------------------------------------------------------------------------------

// A directory made with the original functions: Init (lazy 1 leaves loading to the caller)
static ZZipDir *new_dir(const char *file, const char *mount, DWORD lazy, DWORD check = 0, DWORD flags = 0)
{
    char p[MAX_PATH];
    zip_path(p, file);
    auto *d = static_cast<ZZipDir *>(heap_alloc(0x2c));
    memset(d, 0, 0x2c);   // the padding byte +3 is never written
    original<ZZipDir *(THISCALL *)(ZZipDir *)>(0x5c2040)(d);
    original<bool(THISCALL *)(ZZipDir *, const char *, DWORD, const char *, DWORD, DWORD)>(0x5c2710)(d, p, check, mount, lazy, flags);
    return d;
}
static void delete_dir(ZZipDir *d)
{
    original<DWORD(THISCALL *)(ZZipDir *)>(0x5c2780)(d);
    heap_free(d);
}

static void test_dir(Tally &t)
{
    char d[240];
    CodePatch box, exit;
    patch_jump(box, 0x4d24c0, (void *)&capture_box);
    patch_jump(exit, 0x5d0e36, (void *)&zax_escape_exit);
    // Constructor on prefilled memory
    {
        BYTE obj[2][0x2c];
        DWORD r[2];
        for (int v = 0; v < 2; v++) {
            memset(obj[v], 0xcd, 0x2c);
            r[v] = (DWORD)(uintptr_t)pick<ZZipDir *(THISCALL *)(void *)>(v, 0x5c2040)(obj[v]) - (DWORD)(uintptr_t)obj[v];
        }
        t.check(memcmp(obj[0], obj[1], 0x2c) == 0 && r[0] == r[1], "dir", "[constructor]");
    }
    // Init (lazy and at once, with mount folders) and Load of every test archive, plus a missing file
    const char *mounts[] = {"", "/", "Pack/Sub/", "/x/", "pack/"};
    for (int z = -1; z < (int)(sizeof ZIPS / sizeof *ZIPS); z++) {
        const char *file = z < 0 ? "missing.zip" : ZIPS[z].file;
        for (const char *mount : mounts)
            for (DWORD lazy = 0; lazy < 3; lazy++)
                for (DWORD keep = 0; keep < 2; keep++) {
                    if (lazy == 2 && keep)
                        continue;
                    unsigned h[2];
                    for (int v = 0; v < 2; v++) {
                        char p[MAX_PATH];
                        zip_path(p, file);
                        auto *dir = static_cast<ZZipDir *>(heap_alloc(0x2c));
                        original<ZZipDir *(THISCALL *)(ZZipDir *)>(0x5c2040)(dir);
                        g_box_text[0] = 0;
                        g_box_caption = nullptr;
                        zax_exit_code = 0;
                        int r;
                        if (lazy < 2) {
                            // Init itself (Load inside it is the reimplementation for both)
                            DWORD args[5] = {(DWORD)(uintptr_t)p, keep ? 1u : 0u, (DWORD)(uintptr_t)mount, lazy, keep ? 0x20u : 0u};
                            r = zax_call_escapable_regs(pick<void *>(v, 0x5c2710), (DWORD)(uintptr_t)dir, 0, 5, args);
                        } else {
                            // Load on its own after a lazy Init
                            original<bool(THISCALL *)(ZZipDir *, const char *, DWORD, const char *, DWORD, DWORD)>(0x5c2710)(
                                dir, p, 0, mount, 1, 0);
                            dir->keep_open = (BYTE)(mount[0] == 'p');
                            r = zax_call_escapable_regs(pick<void *>(v, 0x5c2240), (DWORD)(uintptr_t)dir, 0, 0, nullptr);
                        }
                        r = r == 0x7fff0001 ? r : (r & 0xff);
                        bool packed = z >= 0 && ZIPS[z].kind == COMPRESSED && !(lazy == 1);
                        h[v] = fnv(fnv(2166136261u, &r, 4), &zax_exit_code, 4);
                        h[v] = text_hash(fnv_text(h[v], g_box_text), g_box_caption);
                        h[v] = dir_hash(h[v], dir, packed ? 2 : 0xffffffff);
                        at<DWORD(THISCALL *)(ZZipDir *)>(0x5c2780)(dir);
                        heap_free(dir);
                    }
                    wsprintfA(d, "[%s %s %s lazy %u keep %u]", lazy < 2 ? "Init" : "Load", file, mount, lazy, keep);
                    t.check(h[0] == h[1], "dir", d);
                }
    }
    log("  zip Init/Load done (%u ms)", GetTickCount());
    // Close on loaded, lazy and closed directories; the return value too
    for (int z = 0; z < 3; z++) {
        unsigned h[2];
        DWORD rs[2];
        for (int v = 0; v < 2; v++) {
            ZZipDir *dir = new_dir(z == 2 ? "missing.zip" : "a.zip", z ? "Pack/" : "/", z == 1);
            DWORD r = pick<DWORD(THISCALL *)(ZZipDir *)>(v, 0x5c2780)(dir);
            DWORD r2 = pick<DWORD(THISCALL *)(ZZipDir *)>(v, 0x5c2780)(dir);
            rs[v] = r;
            h[v] = fnv(fnv(2166136261u, &r2, 4), dir, 0x2c);
            heap_free(dir);
        }
        wsprintfA(d, "[Close %d: %x %x]", z, rs[0], rs[1]);
        log("  Close %d returns %x (original) %x (reimpl)", z, rs[0], rs[1]);
        t.check(h[0] == h[1], "dir", d);
    }
    // The comparisons of the sort
    {
        ZZipDir *dir = new_dir("a.zip", "", 0);
        *at<ZZipEntry **>(ZIP_SORT_ENTRIES) = dir->entries;
        for (DWORD i = 0; i < dir->count; i++)
            for (DWORD j = 0; j < dir->count; j++) {
                compare_call<int(__cdecl *)(const void *, const void *)>(t, 0x5c2220, "CompareOffset", (const void *)&dir->entries[i],
                                                                        (const void *)&dir->entries[j]);
                compare_call<int(__cdecl *)(const void *, const void *)>(t, 0x5c21e0, "CompareName", (const void *)&i, (const void *)&j);
            }
        delete_dir(dir);
    }
    // Find, Exists and ListDir on loaded directories (also with a mount folder and the RE signature)
    struct Loaded {
        const char *file, *mount;
    };
    const Loaded loaded[] = {{"a.zip", ""}, {"a.zip", "Pack/Sub/"}, {"re.dat", "/"}, {"fewer.zip", ""}, {"b.zip", "Pack/"}, {"empty.zip", ""}};
    const char *finds[] = {"Rules.txt", "rules.TXT", "Sounds/", "Sounds/zap.wav", "Sounds/Ambient/", "", "zzz", "0", "Levels/ACT 1/M1/01 Main.zax",
                           "Pack/Sub/Rules.txt", "pack/sub/sounds/zap.wav", "Pack/a.txt", "Pack/Sounds/b.wav", "c/d/e.zax", "-hidden.txt",
                           "M", "X/Y/Z/", "Pack/zz.wav", "Pack/", "pack/sub/-hidden.txt"};
    const char *folders[] = {"", "Sounds/", "sounds", "Sounds/-Old/", "Levels/ACT 1/", "Pack/", "Pack/Sub/", "Pack/Sub/Sounds/",
                             "pack", "Pack/Sub/Levels/ACT 1/", "X/", "Q/", "Pack/Sounds/"};
    const char *exts[][2] = {{"", nullptr}, {"wav", nullptr}, {"WAV", "txt"}, {"av", nullptr}, {"zax", "mdl16"}};
    const char *prefixes[] = {"", "-", "S", "lev"};
    for (const Loaded &l : loaded) {
        ZZipDir *dir = new_dir(l.file, l.mount, 0);
        log("  zip queries on %s %s: loaded %u, %u entries (%u ms)", l.file, l.mount, dir->loaded, dir->count, GetTickCount());
        for (const char *f : finds) {
            char upper[100];
            copy_upper(upper, f, 100);
            const char *variants[] = {f, upper};
            for (const char *name : variants)
                for (int with_pos = 0; with_pos < 2; with_pos++) {
                    DWORD pos[2] = {0xcdcdcdcd, 0xcdcdcdcd};
                    ZZipEntry *r[2];
                    for (int v = 0; v < 2; v++)
                        r[v] = pick<ZZipEntry *(THISCALL *)(ZZipDir *, const char *, DWORD *)>(v, 0x5c2d50)(dir, name, with_pos ? &pos[v] : nullptr);
                    wsprintfA(d, "[Find %s %s %s]", l.file, l.mount, name);
                    t.check(r[0] == r[1] && pos[0] == pos[1], "dir", d);
                }
            for (DWORD check = 0; check < 2; check++) {
                dir->check_disk = check;
                BYTE out[2] = {0xcd, 0xcd};
                bool r[2];
                for (int v = 0; v < 2; v++)
                    r[v] = pick<bool(THISCALL *)(ZZipDir *, const char *, BYTE *)>(v, 0x5c2830)(dir, f, &out[v]);
                wsprintfA(d, "[Exists %s %s %s check %u]", l.file, l.mount, f, check);
                t.check(r[0] == r[1] && out[0] == out[1], "dir", d);
            }
        }
        dir->check_disk = 0;
        for (const char *f : folders)
            for (const auto &e : exts)
                for (DWORD mode = 0; mode < 3; mode++)
                    for (DWORD folders_only = 0; folders_only < 2; folders_only++)
                        for (DWORD names_only = 0; names_only < 3; names_only++)
                            for (const char *prefix : prefixes) {
                                if (!folders_only && prefix[0])
                                    continue;   // the prefix only applies to folders
                                unsigned h[2];
                                for (int v = 0; v < 2; v++) {
                                    for (DWORD i = 0; i < dir->count; i++)
                                        dir->entries[i].listed = 0xcd;
                                    ZFileSpec s;
                                    make_spec(&s);
                                    original<DWORD(THISCALL *)(ZFileSpec *, const char *, const char *)>(0x4a0a60)(&s, f, e[0]);
                                    if (e[1])
                                        original<DWORD(THISCALL *)(ZFileSpec *, const char *)>(0x4a0a30)(&s, e[1]);
                                    s.mode = mode;
                                    s.folders = folders_only;
                                    s.names_only = names_only;
                                    s.prefix = prefix;
                                    RecordSink sink;
                                    init_sink(&sink);
                                    DWORD r = pick<DWORD(THISCALL *)(ZZipDir *, ZFileSink *, const ZFileSpec *)>(v, 0x5c2970)(
                                        dir, reinterpret_cast<ZFileSink *>(&sink), &s);
                                    h[v] = fnv(sink_hash(sink), &r, 4);
                                    for (DWORD i = 0; i < dir->count; i++)
                                        h[v] = fnv(h[v], &dir->entries[i].listed, 1);
                                    if (!v)
                                        g_names_listed += sink.count;
                                    free_spec(&s);
                                }
                                wsprintfA(d, "[ListDir %s %s folder %s ext %s mode %u folders %u names %u prefix %s]", l.file, l.mount, f,
                                          e[0], mode, folders_only, names_only, prefix);
                                t.check(h[0] == h[1], "dir", d);
                            }
        delete_dir(dir);
    }
    // ListDir and Exists on lazy directories load them first. A directory that cannot be read would end
    // the game, but the message is formatted with the path that Close has already cleared (null): the
    // game thread hangs there with the original as well, so that case is left out.
    const char *lazy_files[] = {"a.zip", "b.zip", "re.dat", "empty.zip"};
    for (const char *file : lazy_files)
        for (int fn = 0; fn < 2; fn++) {
            unsigned h[2];
            for (int v = 0; v < 2; v++) {
                ZZipDir *dir = new_dir(file, "", 1, 1);
                ZFileSpec s;
                make_spec(&s);
                original<DWORD(THISCALL *)(ZFileSpec *, const char *, const char *)>(0x4a0a60)(&s, "Sounds/", "wav");
                RecordSink sink;
                init_sink(&sink);
                BYTE out = 0xcd;
                g_box_text[0] = 0;
                g_box_caption = nullptr;
                zax_exit_code = 0;
                DWORD args[2] = {fn ? (DWORD)(uintptr_t)&sink : (DWORD)(uintptr_t) "Sounds/zap.wav",
                                 fn ? (DWORD)(uintptr_t)&s : (DWORD)(uintptr_t)&out};
                int r = zax_call_escapable_regs(pick<void *>(v, fn ? 0x5c2970 : 0x5c2830), (DWORD)(uintptr_t)dir, 0, 2, args);
                r = r == 0x7fff0001 ? r : (fn ? r : (r & 0xff));
                h[v] = fnv(fnv(fnv(2166136261u, &r, 4), &zax_exit_code, 4), &out, 1);
                h[v] = text_hash(fnv_text(h[v], g_box_text), g_box_caption);
                h[v] = dir_hash(sink_hash(sink) ^ h[v], dir);
                free_spec(&s);
                delete_dir(dir);
            }
            wsprintfA(d, "[lazy %s %s]", fn ? "ListDir" : "Exists", file);
            log("  zip %s (%u ms)", d, GetTickCount());
            t.check(h[0] == h[1], "dir", d);
        }
    unpatch(exit);
    unpatch(box);
}

// ---- Folder tree ---------------------------------------------------------------------------------

static void new_list(ZZipList *l)
{
    l->vftable = NODE_ARRAY_VTABLE;
    l->data = nullptr;
    l->count = 0;
    l->capacity = 0;
}

struct MountDef {
    const char *file;   // nullptr: missing
    DWORD new_root, check;
    const char *mount;
    DWORD lazy, flags;
};
static const MountDef MOUNTS[] = {
    {"a.zip", 1, 0, "/", 0, 0},           {"b.zip", 0, 1, "Pack/Sub/", 1, 0}, {"c.zip", 0, 1, "pack/Other/", 1, 0x20},
    {nullptr, 0, 0, "Gone/Deep/", 0, 0},  {nullptr, 1, 0, "/", 0, 0},         {"a.zip", 1, 0, "Two//x/", 0, 0},
    {"empty.zip", 0, 0, "", 0, 0},        {"b.zip", 0, 0, "PACK/sub/More/", 0, 0}, {"c.zip", 1, 1, "/Lead/", 1, 0},
    {"comment.zip", 0, 0, "Bad/", 0, 0},  {"comment.zip", 0, 0, "Bad2/", 1, 0},
};

static unsigned g_open_hash;
static DWORD g_open_calls;
static DWORD THISCALL fake_open(ZZipDir *dir, const char *path, DWORD mode)
{
    g_open_calls++;
    g_open_hash = text_hash(text_hash(text_hash(g_open_hash, dir->path), dir->prefix), path);
    g_open_hash = fnv(g_open_hash, &mode, 4);
    return dir->prefix && dir->prefix[0] && mode == 1 ? 0x1234 : 0;
}

static unsigned mount_all(int v, ZZipList *l)
{
    unsigned h = 2166136261u;
    for (const MountDef &m : MOUNTS) {
        char p[MAX_PATH];
        zip_path(p, m.file ? m.file : "missing.zip");
        bool r = pick<bool(THISCALL *)(ZZipList *, const char *, DWORD, DWORD, const char *, DWORD, DWORD)>(v, 0x5c1a30)(
            l, p, m.new_root, m.check, m.mount, m.lazy, m.flags);
        h = list_hash(fnv(h, &r, 1), l);
        log("  zip mount %s %s: %u", m.file, m.mount, r);
    }
    return h;
}

static void test_tree(Tally &t)
{
    char d[200];
    const char *paths[] = {"Pack/Sub/a.txt", "pack/sub/Sounds/b.wav", "Pack/Other/one.wav", "pack/other/two/Four.txt", "Sounds/zap.wav",
                           "Rules.txt", "Gone/Deep/a", "nothing", "", "Two//x/a.wav", "Two/x/a.wav", "Pack/Sub/", "/a", "Pack/Sub\\c/d/e.zax",
                           "PACK/SUB/MORE/c/f.zax", "Lead/one.wav", "Bad/x"};
    // Mounting step by step, then the queries, Open (0x5c2e50 redirected) and the destructors
    ZZipList l[2];
    unsigned h[2];
    for (int v = 0; v < 2; v++) {
        new_list(&l[v]);
        h[v] = mount_all(v, &l[v]);
    }
    t.check(h[0] == h[1], "tree", "[Mount]");
    log("  zip tree step %s (%u ms)", "[Mount]", GetTickCount());
    CodePatch open;
    patch_jump(open, 0x5c2e50, (void *)&fake_open);
    for (const char *p : paths)
        for (DWORD mode = 0; mode < 2; mode++) {
            DWORD r[2];
            for (int v = 0; v < 2; v++) {
                g_open_calls = 0;
                g_open_hash = 2166136261u;
                r[v] = pick<DWORD(THISCALL *)(ZZipList *, const char *, DWORD)>(v, 0x5c1c70)(&l[v], p, mode);
                h[v] = fnv(fnv(g_open_hash, &g_open_calls, 4), &r[v], 4);
            }
            wsprintfA(d, "[Open %s %u]", p, mode);
            t.check(h[0] == h[1], "tree", d);
        }
    unpatch(open);
    for (const char *p : paths) {
        BYTE out[2] = {0xcd, 0xcd};
        for (int v = 0; v < 2; v++) {
            bool r = pick<bool(THISCALL *)(ZZipList *, const char *, BYTE *)>(v, 0x5c1d00)(&l[v], p, &out[v]);
            h[v] = list_hash(fnv(fnv(2166136261u, &r, 1), &out[v], 1), &l[v]);
        }
        wsprintfA(d, "[Exists %s]", p);
        t.check(h[0] == h[1], "tree", d);
    }
    // AddChild on the first root
    for (int k = 0; k < 4; k++) {
        const char *texts[] = {"abc", "Sounds/x", "", "longer name"};
        for (int v = 0; v < 2; v++) {
            auto *root = static_cast<ZZipNode *>(l[v].data[0]);
            auto *c = pick<ZZipNode *(THISCALL *)(ZZipNode *, const char *, DWORD)>(v, 0x5c1320)(root, texts[k], (DWORD)k + 1 > 3 ? 4 : (DWORD)k);
            DWORD idx = root->children.count;
            bool last = idx && root->children.data[idx - 1] == c;
            h[v] = node_hash(fnv(2166136261u, &last, 1), root, 0);
        }
        wsprintfA(d, "[AddChild %d]", k);
        t.check(h[0] == h[1], "tree", d);
    }
    // DeleteNodes on the children of a root, the destructor of a root, Clear of the rest
    {
        DWORD r[2];
        for (int v = 0; v < 2; v++) {
            auto *root = static_cast<ZZipNode *>(l[v].data[0]);
            r[v] = pick<DWORD(THISCALL *)(ZPtrArray *)>(v, 0x5c1e20)(&root->children);
            h[v] = fnv(list_hash(2166136261u, &l[v]), &r[v], 4);
        }
        t.check(h[0] == h[1], "tree", "[DeleteNodes]");
    log("  zip tree step %s (%u ms)", "[DeleteNodes]", GetTickCount());
        for (int v = 0; v < 2; v++) {
            auto *root = static_cast<ZZipNode *>(l[v].data[1]);
            pick<void(THISCALL *)(ZZipNode *)>(v, 0x5c1260)(root);
            DWORD state[4] = {root->children.vftable, (DWORD)(root->children.data != nullptr), root->children.count, root->children.capacity};
            h[v] = fnv(fnv(2166136261u, state, sizeof state), &root->archive, 4);
            heap_free(root);
            l[v].data[1] = nullptr;
        }
        t.check(h[0] == h[1], "tree", "[Dtor]");
        for (int v = 0; v < 2; v++) {
            r[v] = pick<DWORD(THISCALL *)(ZZipList *)>(v, 0x5c1570)(&l[v]);
            h[v] = fnv(list_hash(2166136261u, &l[v]), &r[v], 4);
            r[v] = pick<DWORD(THISCALL *)(ZZipList *)>(v, 0x5c1570)(&l[v]);
            h[v] = fnv(h[v], &r[v], 4);
            at<void(THISCALL *)(ZPtrArray *)>(0x5c2010)(&l[v]);
        }
        t.check(h[0] == h[1], "tree", "[Clear]");
    }
    // The atexit function on the global list (swapped for a tree of our own)
    {
        SavedBytes saved;
        save_bytes(saved, ZIP_LIST, 0x10);
        for (int v = 0; v < 2; v++) {
            auto *g = at<ZZipList *>(ZIP_LIST);
            new_list(g);
            mount_all(0, g);
            pick<void(__cdecl *)()>(v, 0x5c16a0)();
            h[v] = list_hash(2166136261u, g);
        }
        restore_bytes(saved);
        t.check(h[0] == h[1], "tree", "[AtExit]");
    log("  zip tree step %s (%u ms)", "[AtExit]", GetTickCount());
    }
    // Reserve and Free of pool arrays
    for (DWORD cap = 0; cap < 40; cap += 3)
        for (DWORD fill = 0; fill <= cap; fill += 4)
            for (DWORD need : (DWORD[]){0, 1, cap, cap + 1, cap + 7, cap * 2 + 1}) {
                unsigned hh[2];
                for (int v = 0; v < 2; v++) {
                    ZZipNodeArray a;
                    a.vftable = NODE_ARRAY_VTABLE;
                    a.data = cap ? static_cast<void **>(pool_get_alloc(cap * 4)) : nullptr;
                    a.count = fill;
                    a.capacity = cap;
                    for (DWORD i = 0; i < fill; i++)
                        a.data[i] = (void *)(uintptr_t)(i * 7 + 1);
                    pick<void(THISCALL *)(ZPtrArray *, DWORD)>(v, 0x5c11c0)(&a, need);
                    hh[v] = fnv(fnv(2166136261u, &a.count, 8), a.data ? a.data : (void **)&a.count, a.data ? fill * 4 : 4);
                    pick<void(THISCALL *)(ZPtrArray *)>(v, 0x5c2010)(&a);
                    hh[v] = fnv(hh[v], &a, sizeof a);
                }
                wsprintfA(d, "[Reserve capacity %u count %u need %u]", cap, fill, need);
                t.check(hh[0] == hh[1], "tree", d);
            }
}

// ---- Mounting at start ---------------------------------------------------------------------------

// The zip search of Zip_MountAll (0x4a3ae0) redirected to the test folder Sub\ (absolute paths)
static DWORD THISCALL fake_read_zips(void *list, ZFileSpec *spec)
{
    const char *folder = spec->folder;
    spec->folder = g_zip_root;
    DWORD n = at<DWORD(__fastcall *)(void *, ZFileSpec *, DWORD)>(0x4a1190)(list, spec, 0x20);
    spec->folder = folder;
    return n;
}

static void test_mount_all(Tally &t)
{
    char d[200];
    SavedBytes list, flags, folder;
    save_bytes(list, ZIP_LIST, 0x10);
    save_bytes(flags, ZIP_LOADED, 1);
    save_bytes(folder, DATA_FOLDER, 0x40);
    CodePatch read, box, exit;
    patch_jump(read, 0x4a3ae0, (void *)&fake_read_zips);
    patch_jump(box, 0x4d24c0, (void *)&capture_box);
    patch_jump(exit, 0x5d0e36, (void *)&zax_escape_exit);
    struct Def {
        const char *name;
        DWORD mode, skip, loaded;
    };
    const Def defs[] = {{"Data", 0, 0, 0}, {"Data", 1, 0, 0}, {"Data", 0, 1, 0}, {"Nothing", 0, 1, 0}, {"Nothing", 2, 0, 0}, {"Data", 0, 0, 1}};
    for (const Def &def : defs)
        for (int unmount = 0; unmount < 2; unmount++) {
            unsigned h[2];
            for (int v = 0; v < 2; v++) {
                auto *g = at<ZZipList *>(ZIP_LIST);
                new_list(g);
                *at<BYTE *>(ZIP_LOADED) = (BYTE)def.loaded;
                lstrcpyA(at<char *>(DATA_FOLDER), "unchanged");
                g_box_text[0] = 0;
                zax_exit_code = 0;
                DWORD args[1] = {def.skip};
                int r = zax_call_escapable_regs(pick<void *>(v, 0x4a0c40), (DWORD)(uintptr_t)def.name, def.mode, 1, args);
                r = r == 0x7fff0001 ? 1 : 0;
                h[v] = fnv(fnv(2166136261u, &r, 4), &zax_exit_code, 4);
                h[v] = list_hash(fnv_text(fnv(h[v], at<BYTE *>(ZIP_LOADED), 1), at<char *>(DATA_FOLDER)), g);
                if (!v && !unmount)
                    log("  Zip_MountAll %s %u %u: loaded %u, %u roots, data folder %s", def.name, def.mode, def.skip,
                        *at<BYTE *>(ZIP_LOADED), g->count, at<char *>(DATA_FOLDER));
                // Unmounting twice (the second time nothing is mounted)
                for (int k = 0; k < 2; k++) {
                    bool u = pick<bool(__cdecl *)()>(unmount ? v : 0, 0x4a1120)();
                    h[v] = list_hash(fnv(fnv(h[v], &u, 1), at<BYTE *>(ZIP_LOADED), 1), g);
                }
                at<void(THISCALL *)(ZPtrArray *)>(0x5c2010)(g);
            }
            wsprintfA(d, "[Zip_MountAll %s %u %u loaded %u, unmount %d]", def.name, def.mode, def.skip, def.loaded, unmount);
            log("  zip %s (%u ms)", d, GetTickCount());
            t.check(h[0] == h[1], "mount", d);
        }
    unpatch(exit);
    unpatch(box);
    unpatch(read);
    restore_bytes(folder);
    restore_bytes(flags);
    restore_bytes(list);
    compare_call<const char *(__cdecl *)()>(t, 0x4a1140, "Files_CacheFolder");
}

ZAX_CHECKS(zip_archive)
{
    (void)cls;
    (void)n;
    Tally dir{"Zip directory"}, tree{"Zip tree"}, mount{"Zip mounting"};
    if (!build_zips()) {
        log("  zip archive checks: no temp folder");
        return;
    }
    DWORD t0 = GetTickCount();
    test_dir(dir);
    DWORD t1 = GetTickCount();
    log("  zip directory checks: %u ms", t1 - t0);
    test_tree(tree);
    DWORD t2 = GetTickCount();
    log("  zip tree checks: %u ms", t2 - t1);
    test_mount_all(mount);
    log("  zip mounting checks: %u ms", GetTickCount() - t2);
    remove_zips();
    log("  zip archives: %u names listed into a recording sink", g_names_listed);
    dir.report();
    tree.report();
    mount.report();
}

}  // namespace zax
