// Checks of the zip archive streams (zip_stream.h): opening files of a test archive of our own and of the
// mounted archive, reading them byte for byte against the archive file (chunks around the boundaries,
// seeks, skips, at the end), writing into entries of copies of the test archive (moving the entry to the
// end, the central directory at Close) and appending files with the writer; the archive files written by
// the original and the reimplementation must be identical.
#include "check_util.h"
#include "init_list.h"
#include "zip_stream.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT DWORD WINAPI GetTickCount(void);
WINIMPORT BOOL WINAPI VirtualFree(void *addr, size_t size, DWORD type);

namespace zax {

// checks_file_buffer.cpp
ZZipDir *mounted_archive();
int pick_entries(ZZipDir *d, DWORD *out, int max);
DWORD zip_truth_bytes(ZZipDir *d, const ZZipEntry *e, BYTE *out, DWORD max);
DWORD read_file(const char *path, void *p, DWORD max, DWORD offset);

template <typename F> static F pick(int v, DWORD address) { return v ? at<F>(address) : original<F>(address); }
static DWORD addr(const void *p) { return (DWORD)(uintptr_t)p; }
static DWORD &files_open() { return *at<DWORD *>(FILES_OPEN); }
static unsigned text_hash(unsigned h, const char *s) { return s ? fnv_text(h, s) : fnv(h, "\x01null", 5); }

using OpenFileFn = ZStream *(THISCALL *)(ZZipDir *, const char *, DWORD);
using SlotFn0 = DWORD(THISCALL *)(void *);
using SlotFn1 = DWORD(THISCALL *)(void *, DWORD);
using SlotFn2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
// The slots of the archive stream and the writer
constexpr DWORD ZS_DELETE = 0x5c31f0, ZS_WRITE = 0x5c3220, ZS_READ = 0x5c3290, ZS_SKIP = 0x5c3320, ZS_SEEK = 0x5c3380,
                ZS_TELL = 0x5c33c0, ZS_SIZE = 0x5c33f0, ZS_SET_SIZE = 0x5c3400, ZS_CLOSE = 0x5c3440, ZS_OPEN = 0x5c35c0;
constexpr DWORD ZW_CREATE = 0x5c3660, ZW_DELETE = 0x5c36c0, ZW_CLOSE = 0x5c3700, ZW_OPEN = 0x5c38d0, ZW_READ = 0x5c3a70,
                ZW_WRITE = 0x5c3a90, ZW_SKIP = 0x5c3ab0, ZW_SEEK = 0x5c3ac0, ZW_TELL = 0x5c3ae0, ZW_SIZE = 0x5c3b00, ZW_SET_SIZE = 0x5c3b20;

// ---- Test archives ---------------------------------------------------------------------------------

struct TestEntry {
    const char *name;
    DWORD size, extra;
};
static const TestEntry ENTRIES[] = {{"a.txt", 10, 0},      {"Sub/b.bin", 70000, 0}, {"Sub/c.bin", 0, 0},     {"d.dat", 2000, 4},
                                    {"e/f/g.bin", 1999, 0}, {"Sub/h.bin", 65536, 0}, {"z.bin", 140000, 6}, {"y.txt", 1, 0}};
constexpr int N_ENTRIES = sizeof ENTRIES / sizeof *ENTRIES;
constexpr DWORD MAX_ARCHIVE = 400000;
static BYTE *g_zip, *g_file[2], *g_data;
static DWORD g_zip_len;
static DWORD g_data_at[N_ENTRIES];   // where the data of each entry starts in g_zip
static char g_root[MAX_PATH];

static void put(const void *p, DWORD n)
{
    memcpy(g_zip + g_zip_len, p, n);
    g_zip_len += n;
}
static void put16(DWORD v) { put(&v, 2); }
static void put32(DWORD v) { put(&v, 4); }

// A stored archive (local headers with extra fields as in the central directory); RE: Data.dat signature
static void make_archive(bool re)
{
    g_zip_len = 0;
    DWORD offsets[N_ENTRIES];
    for (int k = 0; k < N_ENTRIES; k++) {
        const TestEntry &e = ENTRIES[k];
        DWORD n = (DWORD)lstrlenA(e.name);
        offsets[k] = g_zip_len;
        put32(ZIP_LOCAL);
        put16(10);
        put16(0);
        put16(0);
        put16(0x4000 + k);
        put16(0x2b00 + k);
        put32(0x1234567u * (k + 1));
        put32(e.size);
        put32(e.size);
        put16(n);
        put16(e.extra);
        put(e.name, n);
        put("\x11\x22\x33\x44\x55\x66", e.extra);
        g_data_at[k] = g_zip_len;
        g_seed = 300 + k;
        for (DWORD i = 0; i < e.size; i++)
            g_zip[g_zip_len++] = (BYTE)(random() >> 11);
    }
    DWORD cd = g_zip_len;
    for (int k = 0; k < N_ENTRIES; k++) {
        const TestEntry &e = ENTRIES[k];
        DWORD n = (DWORD)lstrlenA(e.name);
        put32(ZIP_CENTRAL);
        put16(20);
        put16(10);
        put16(0);
        put16(0);
        put16(0x4000 + k);
        put16(0x2b00 + k);
        put32(0x1234567u * (k + 1));
        put32(e.size);
        put32(e.size);
        put16(n);
        put16(e.extra);
        put16(0);
        put16(0);
        put16(k & 1);
        put32(0x20);
        put32(offsets[k]);
        put(e.name, n);
        put("\x11\x22\x33\x44\x55\x66", e.extra);
    }
    DWORD size = g_zip_len - cd;
    put32(re ? ZIP_END_RE : ZIP_END);
    put16(0);
    put16(0);
    put16(N_ENTRIES);
    put16(N_ENTRIES);
    put32(size);
    put32(cd);
    put16(0);
}
static void archive_path(char *out, const char *name) { wsprintfA(out, "%s%s", g_root, name); }
static void write_archive(const char *name, const void *p, DWORD n)
{
    char path[MAX_PATH];
    archive_path(path, name);
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD w;
    WriteFile(f, p, n, &w, nullptr);
    CloseHandle(f);
}
static const char *const SCRATCH[] = {"test.zip", "w0.zip", "w1.zip", "r0.zip", "r1.zip", "bad0.zip", "bad1.zip", "none0.zip", "none1.zip"};
static void remove_archives()
{
    char p[MAX_PATH];
    for (const char *s : SCRATCH) {
        archive_path(p, s);
        DeleteFileA(p);
    }
    if (!RemoveDirectoryA(g_root))
        log("  could not remove %s", g_root);
}
// Hash of an archive file (or "missing")
static unsigned archive_hash(const char *name, DWORD *length = nullptr)
{
    char p[MAX_PATH];
    archive_path(p, name);
    DWORD n = read_file(p, g_file[0], MAX_ARCHIVE, 0);
    if (length)
        *length = n;
    return n == 0xffffffff ? 0xdead : fnv(fnv(2166136261u, &n, 4), g_file[0], n);
}

// ---- Directories and streams -----------------------------------------------------------------------

static ZZipDir *new_dir(const char *name, const char *mount, DWORD lazy, DWORD check, DWORD flags = 0)
{
    char p[MAX_PATH];
    archive_path(p, name);
    auto *d = static_cast<ZZipDir *>(heap_alloc(0x2c));
    memset(d, 0, 0x2c);
    original<ZZipDir *(THISCALL *)(ZZipDir *)>(0x5c2040)(d);
    original<bool(THISCALL *)(ZZipDir *, const char *, DWORD, const char *, DWORD, DWORD)>(0x5c2710)(d, p, check, mount, lazy, flags);
    return d;
}
static void delete_dir(ZZipDir *d)
{
    original<DWORD(THISCALL *)(ZZipDir *)>(0x5c2780)(d);
    heap_free(d);
}

// Everything of a directory but the archive path (the two copies have different names)
static unsigned dir_hash(unsigned h, const ZZipDir *d)
{
    auto *f = static_cast<const CFileBuf *>(d->file);
    BYTE b[3] = {d->loaded, d->unknown01, d->keep_open};
    h = fnv(h, b, 3);
    DWORD v[10] = {f != nullptr, f && f->stream, f ? f->flags : 0, f && f->stream ? vcall<DWORD>(f->stream, STREAM_TELL) : 0,
                   d->count,     d->unknown1c,   d->open_flags,  d->end_signature,
                   d->check_disk, d->entries != nullptr};
    h = text_hash(fnv(h, v, sizeof v), d->prefix);
    for (DWORD i = 0; d->entries && i < d->count; i++)
        h = fnv(text_hash(h, d->entries[i].name), &d->entries[i].version_made, 0x2c);
    if (d->sorted && d->count > 1)
        h = fnv(h, d->sorted, d->count * 4);
    return h;
}
static unsigned stream_hash(unsigned h, const ZZipStream *s, const ZZipDir *d)
{
    if (!s)
        return fnv(h, "\x02null", 6);
    DWORD file_kind = !s->file ? 0 : s->file == d->file ? 1 : 2;
    DWORD v[11] = {s->vftable, addr(s->handle), s->owned, s->own_file, s->position,
                   s->entry ? (DWORD)(s->entry - d->entries) : 0xffffffff, file_kind,
                   file_kind == 2 ? s->file->flags : 0, file_kind == 2 && s->file->stream ? s->file->stream->vftable : 0,
                   s->dir == d, s->mode};
    return fnv(h, v, sizeof v);
}

// Reads a whole stream in chunks through the slots of v; returns the bytes (into g_file[v + ...])
static DWORD read_stream(int v, void *s, DWORD chunk, BYTE *out, DWORD max, unsigned *h)
{
    DWORD total = 0;
    for (int k = 0;; k++) {
        if (k % 4 == 3)
            *h = fnv(*h, "s", 1 + (pick<SlotFn1>(v, ZS_SEEK)(s, total) & 0xff));
        DWORD want = total + chunk > max ? max - total : chunk;
        DWORD r = pick<SlotFn2>(v, ZS_READ)(s, addr(out + total), want);
        *h = fnv(*h, &r, 4);
        if (!r || r > want)
            break;
        total += r;
    }
    DWORD tail[2] = {pick<SlotFn0>(v, ZS_TELL)(s), pick<SlotFn0>(v, ZS_SIZE)(s)};
    *h = fnv(*h, tail, sizeof tail);
    return total;
}

// Seeks, skips and reads at the boundaries of an entry, against its bytes
static void probe_stream(int v, void *s, const BYTE *truth, DWORD size, unsigned *h, bool *ok)
{
    const DWORD at_[] = {0, 1, size / 2, size ? size - 1 : 0u, size, size + 1, 0xffffffff, 1999, 2000, 65536};
    for (DWORD p : at_) {
        DWORD r = pick<SlotFn1>(v, ZS_SEEK)(s, p) & 0xff;
        BYTE buf[2100];
        DWORD n = pick<SlotFn2>(v, ZS_READ)(s, addr(buf), sizeof buf);
        if (r && p <= size) {
            DWORD want = size - p < sizeof buf ? size - p : sizeof buf;
            if (n != want || memcmp(buf, truth + p, n) != 0)
                *ok = false;
        }
        DWORD x[3] = {r, n, pick<SlotFn0>(v, ZS_TELL)(s)};
        *h = fnv(*h, x, sizeof x);
        for (DWORD d : {(DWORD)-5, (DWORD)3, (DWORD)0, size, (DWORD)-1}) {
            DWORD k = pick<SlotFn1>(v, ZS_SKIP)(s, d) & 0xff;
            DWORD y[2] = {k, pick<SlotFn0>(v, ZS_TELL)(s)};
            *h = fnv(*h, y, sizeof y);
        }
    }
}

// Opens name with mode through OpenFile of v, reads it in chunks and probes it, deletes the stream
// through the deleting destructor of v; everything into the hash, the bytes against truth
static void read_case(Tally &t, ZZipDir *d, const char *name, DWORD mode, const BYTE *truth, DWORD size, const char *label)
{
    char what[300];
    unsigned h[2];
    bool ok[2] = {true, true};
    DWORD got[2];
    for (int v = 0; v < 2; v++) {
        files_open() = 3;
        if (d->file && static_cast<CFileBuf *>(d->file)->stream)   // the same start position for both
            at<bool(THISCALL *)(void *, DWORD)>(0x4a20e0)(d->file, 0);
        auto *s = static_cast<ZZipStream *>(pick<OpenFileFn>(v, 0x5c2e50)(d, name, mode));
        h[v] = stream_hash(dir_hash(2166136261u, d), s, d);
        got[v] = 0;
        if (s) {
            for (DWORD chunk : {1u, 1999u, 2000u, 65536u, 400000u}) {
                if (size > 2001 && chunk < 1999)   // byte by byte is slow in the VM
                    continue;
                pick<SlotFn1>(v, ZS_SEEK)(s, 0);
                DWORD n = read_stream(v, s, chunk, g_file[v], MAX_ARCHIVE, &h[v]);
                if (truth && (n != size || memcmp(g_file[v], truth, size) != 0))
                    ok[v] = false;
                got[v] = n;
            }
            if (truth)
                probe_stream(v, s, truth, size, &h[v], &ok[v]);
            h[v] = stream_hash(h[v], s, d);
            DWORD r = pick<SlotFn1>(v, ZS_DELETE)(s, 1) == addr(s);
            h[v] = fnv(h[v], &r, 4);
        }
        h[v] = fnv(dir_hash(h[v], d), &files_open(), 4);
    }
    wsprintfA(what, "[%s %.100s mode %x: %u %u bytes, truth %d %d]", label, name, mode, got[0], got[1], ok[0], ok[1]);
    t.check(h[0] == h[1] && ok[0] && ok[1], "zip stream", what);
}

static void test_read(Tally &t)
{
    char p[MAX_PATH];
    archive_path(p, "test.zip");
    write_archive("test.zip", g_zip, g_zip_len);
    // A directory read at once (keep_open) and a lazy one (the file is opened and closed per stream)
    for (DWORD lazy = 0; lazy < 2; lazy++)
        for (const char *mount : {"", "Pack/"}) {
            ZZipDir *d = new_dir("test.zip", mount, lazy, 0);
            for (int k = 0; k < N_ENTRIES; k++) {
                char name[100];
                wsprintfA(name, "%s%s", mount, ENTRIES[k].name);
                for (DWORD mode : {0u, 4u, 0x14u, 0x10u, 0x8004u}) {
                    if (mode == 0x8004)
                        d->entries[0].listed = 0;   // the order of the entries is by offset: entry 0 is a.txt
                    read_case(t, d, name, mode, g_zip + g_data_at[k], ENTRIES[k].size, lazy ? "lazy" : "loaded");
                }
            }
            // Missing names, other folders, empty and null paths, write without check_disk
            for (const char *name : {"zzz", "Sub/", "", "a.TXT", "Other/a.txt", "Pack/a.txt", "sub/B.BIN", "Pack/Sub/zz.bin"})
                for (DWORD mode : {0u, 4u, 1u, 3u})
                    read_case(t, d, name, mode, nullptr, 0, "missing");
            delete_dir(d);
        }
    // The stream's Open directly on new objects: own file, the archive's file open and closed
    for (DWORD lazy = 0; lazy < 2; lazy++)
        for (DWORD mode : {0u, 4u, 0x10u, 0x14u, 0x16u}) {
            unsigned h[2];
            for (int v = 0; v < 2; v++) {   // a new directory each (Close with mode 2 changes the entry)
                ZZipDir *d = new_dir("test.zip", "", lazy, 0, lazy ? 0x20u : 0u);
                if (lazy)
                    at<bool(THISCALL *)(ZZipDir *)>(0x5c2240)(d);
                files_open() = 3;
                auto *s = static_cast<ZZipStream *>(pool_alloc(0x24));
                memset(s, 0xcd, 0x24);
                original<ZStream *(THISCALL *)(ZStream *)>(0x4a23a0)(s);
                s->vftable = ZIP_STREAM_VTABLE;
                ZZipEntry *e = &d->entries[3];
                DWORD r = pick<bool(THISCALL *)(ZZipStream *, DWORD, ZZipEntry *, CFileBuf *, const char *, ZZipDir *, DWORD)>(v, ZS_OPEN)(
                              s, 777, e, static_cast<CFileBuf *>(d->file), d->path, d, mode) & 0xff;
                h[v] = stream_hash(dir_hash(fnv(2166136261u, &r, 4), d), s, d);
                pick<SlotFn0>(v, ZS_CLOSE)(s);
                h[v] = stream_hash(dir_hash(h[v], d), s, d);
                pick<SlotFn0>(v, ZS_CLOSE)(s);   // a second Close does nothing
                original<void(THISCALL *)(ZStream *)>(0x4a2410)(s);
                pool_free(s, 0x24);
                h[v] = fnv(dir_hash(h[v], d), &files_open(), 4);
                delete_dir(d);
            }
            char what[100];
            wsprintfA(what, "[Open lazy %u mode %x]", lazy, mode);
            t.check(h[0] == h[1], "zip stream", what);
        }
    // The slots without an entry
    {
        unsigned h[2];
        for (int v = 0; v < 2; v++) {
            ZZipStream s;
            memset(&s, 0, sizeof s);
            s.vftable = ZIP_STREAM_VTABLE;
            BYTE buf[8];
            DWORD x[6] = {pick<SlotFn2>(v, ZS_WRITE)(&s, addr(buf), 8), pick<SlotFn2>(v, ZS_READ)(&s, addr(buf), 8),
                          pick<SlotFn1>(v, ZS_SKIP)(&s, 1) & 0xff, pick<SlotFn1>(v, ZS_SEEK)(&s, 1) & 0xff,
                          pick<SlotFn0>(v, ZS_TELL)(&s), pick<SlotFn0>(v, ZS_SIZE)(&s)};
            pick<SlotFn0>(v, ZS_CLOSE)(&s);
            h[v] = fnv(2166136261u, x, sizeof x);
        }
        t.check(h[0] == h[1], "zip stream", "[no entry]");
    }
    // The mounted archive (read only; mode 1 fails at once without check_disk)
    ZZipDir *d = mounted_archive();
    if (!d) {
        log("  zip stream: no loaded archive mounted");
        return;
    }
    DWORD idx[16];
    int n = pick_entries(d, idx, 16);
    BYTE *truth = static_cast<BYTE *>(VirtualAlloc(nullptr, 0x400000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    for (int k = 0; k < n; k++) {
        const ZZipEntry &e = d->entries[idx[k]];
        if (e.size > MAX_ARCHIVE)
            continue;
        DWORD got = zip_truth_bytes(d, &e, truth, 0x400000);
        if (got != e.size)
            log("  zip stream: cannot read %s from %s (%u of %u)", e.name, d->path, got, e.size);
        for (DWORD mode : {0u, 4u, 0x14u, 1u, 3u})
            if (!(mode & 1) || !d->check_disk)   // never write into a real archive
                read_case(t, d, e.name, mode, got == e.size ? truth : nullptr, e.size, "mounted");
    }
    VirtualFree(truth, 0, 0x8000);
}

// ---- Writing into entries --------------------------------------------------------------------------

static void __fastcall fixed_now(WORD *date, WORD *time)
{
    *date = 0x5a21;
    *time = 0x6b42;
}

// One step of a write scenario on dir d through the functions of v; returns a value for the hash
struct WriteStep {
    enum Kind { OPEN, WRITE, READ, SEEK, SKIP, TELL, SIZE, SET_SIZE, CLOSE, DELETE, MOVE, LOCAL_HEADER, END, OPEN_FILE } kind;
    DWORD a, b;
};
static ZZipStream *g_streams[4];
static DWORD do_step(int v, ZZipDir *d, const WriteStep &s)
{
    ZZipStream *&st = g_streams[s.b & 3];
    if (!st && s.kind >= WriteStep::WRITE && s.kind <= WriteStep::DELETE)
        return 0xdeadbeef;
    switch (s.kind) {
    case WriteStep::OPEN:
        st = static_cast<ZZipStream *>(pick<OpenFileFn>(v, 0x5c2e50)(d, ENTRIES[s.a].name, s.b >> 4));
        return st != nullptr;
    case WriteStep::WRITE:
        return pick<SlotFn2>(v, ZS_WRITE)(st, addr(g_data + (s.a & 0xffff)), s.a >> 16 << 4);
    case WriteStep::READ: {
        DWORD r = pick<SlotFn2>(v, ZS_READ)(st, addr(g_file[1]), s.a);
        return fnv(r, g_file[1], r <= s.a ? r : 0);
    }
    case WriteStep::SEEK:
        return pick<SlotFn1>(v, ZS_SEEK)(st, s.a) & 0xff;
    case WriteStep::SKIP:
        return pick<SlotFn1>(v, ZS_SKIP)(st, s.a) & 0xff;
    case WriteStep::TELL:
        return pick<SlotFn0>(v, ZS_TELL)(st);
    case WriteStep::SIZE:
        return pick<SlotFn0>(v, ZS_SIZE)(st);
    case WriteStep::SET_SIZE:
        return pick<SlotFn1>(v, ZS_SET_SIZE)(st, s.a);
    case WriteStep::CLOSE:
        pick<SlotFn0>(v, ZS_CLOSE)(st);
        return 0;
    case WriteStep::DELETE: {
        DWORD r = pick<SlotFn1>(v, ZS_DELETE)(st, 1) == addr(st);
        st = nullptr;
        return r;
    }
    case WriteStep::MOVE:
        return (DWORD)(pick<ZZipEntry *(THISCALL *)(ZZipDir *, ZZipEntry *)>(v, 0x5c2080)(d, &d->entries[s.a]) - d->entries);
    case WriteStep::LOCAL_HEADER:
        return pick<DWORD(THISCALL *)(ZZipDir *, const ZZipEntry *)>(v, 0x5c30d0)(d, &d->entries[s.a]);
    case WriteStep::END:
        return pick<DWORD(THISCALL *)(ZZipDir *, DWORD)>(v, 0x5c2dd0)(d, s.a);
    case WriteStep::OPEN_FILE:   // the archive's file buffer for writing (for the direct calls)
        return at<bool(THISCALL *)(CFileBuf *, const char *, DWORD)>(0x4a1840)(static_cast<CFileBuf *>(d->file), d->path, s.a) & 0xff;
    }
    return 0;
}

#define W(kind, a, b) WriteStep{WriteStep::kind, (DWORD)(a), (DWORD)(b)}
// Write sizes are a >> 16 << 4 bytes from g_data + (a & 0xffff)
static DWORD wsize(DWORD bytes, DWORD from = 0) { return (bytes >> 4) << 16 | from; }

struct Scenario {
    const char *name;
    DWORD check_disk, lazy;
    const WriteStep *steps;
    int n;
};

static void run_scenario(Tally &t, const Scenario &sc)
{
    char what[200];
    unsigned h[2], files[2];
    DWORD length[2];
    CodePatch now;
    patch_jump(now, 0x4a3c80, (void *)&fixed_now);
    for (int v = 0; v < 2; v++) {
        const char *file = v ? "w1.zip" : "w0.zip";
        write_archive(file, g_zip, g_zip_len);
        ZZipDir *d = new_dir(file, "", sc.lazy, sc.check_disk);
        memset(g_streams, 0, sizeof g_streams);
        files_open() = 3;
        h[v] = dir_hash(2166136261u, d);
        for (int i = 0; i < sc.n; i++) {
            DWORD r = do_step(v, d, sc.steps[i]);
            h[v] = fnv(dir_hash(h[v], d), &r, 4);
            for (ZZipStream *s : g_streams)
                h[v] = stream_hash(h[v], s, d);
        }
        for (ZZipStream *&s : g_streams)
            if (s) {
                pick<SlotFn1>(v, ZS_DELETE)(s, 1);
                s = nullptr;
            }
        h[v] = fnv(dir_hash(h[v], d), &files_open(), 4);
        delete_dir(d);
        files[v] = archive_hash(file, &length[v]);
    }
    unpatch(now);
    wsprintfA(what, "[write %s: archives %u %u bytes]", sc.name, length[0], length[1]);
    t.check(h[0] == h[1] && files[0] == files[1], "zip stream", what);
}

static void test_write(Tally &t)
{
    // Write into the first, a middle and the last entry, with reads, seeks and size changes
    const WriteStep first[] = {W(OPEN, 0, 0x30), W(WRITE, wsize(32), 0), W(TELL, 0, 0), W(SIZE, 0, 0), W(SEEK, 5, 0), W(WRITE, wsize(16, 100), 0),
                               W(SEEK, 0, 0), W(READ, 100, 0), W(SKIP, -3, 0), W(SET_SIZE, 20, 0), W(TELL, 0, 0), W(SET_SIZE, 40, 0),
                               W(READ, 50, 0), W(DELETE, 0, 0)};
    const WriteStep middle[] = {W(OPEN, 3, 0x30), W(WRITE, wsize(3008, 7), 0), W(SIZE, 0, 0), W(SEEK, 2999, 0), W(READ, 100, 0),
                                W(SKIP, 1, 0), W(SKIP, -3008, 0), W(CLOSE, 0, 0), W(DELETE, 0, 0)};
    const WriteStep last[] = {W(OPEN, 7, 0x30), W(WRITE, wsize(70000), 0), W(SEEK, 65536, 0), W(READ, 5000, 0), W(DELETE, 0, 0)};
    const WriteStep big[] = {W(OPEN, 1, 0x30), W(WRITE, wsize(80000), 0), W(SET_SIZE, 100, 0), W(DELETE, 0, 0),
                             W(OPEN, 6, 0x30), W(WRITE, wsize(0), 0), W(DELETE, 0, 0), W(OPEN, 2, 0x30), W(DELETE, 0, 0)};
    // Without mode 2 no CRC; a second write stream and one while a read stream is open fail
    const WriteStep plain[] = {W(OPEN, 4, 0x10), W(WRITE, wsize(48, 3), 0), W(OPEN, 5, 0x31), W(DELETE, 0, 0), W(OPEN, 4, 0x40), W(READ, 30, 0),
                               W(OPEN, 5, 0x31), W(DELETE, 0, 0), W(OPEN, 5, 0x31), W(WRITE, wsize(16), 1), W(DELETE, 0, 1)};
    // An own file buffer (mode 0x10): the archive buffer opened for writing stays open
    const WriteStep own[] = {W(OPEN, 3, 0x130), W(WRITE, wsize(64), 0), W(READ, 10, 0), W(DELETE, 0, 0), W(OPEN, 0, 0x40), W(READ, 10, 0), W(DELETE, 0, 0)};
    // The directory functions directly on the archive opened for writing
    const WriteStep direct[] = {W(OPEN_FILE, 6, 0), W(MOVE, 0, 0), W(MOVE, 3, 0), W(MOVE, 7, 0), W(MOVE, 2, 0), W(LOCAL_HEADER, 1, 0),
                                W(LOCAL_HEADER, 7, 0), W(END, 200000, 0), W(END, 0, 0), W(MOVE, 6, 0)};
    const WriteStep no_open[] = {W(MOVE, 1, 0), W(MOVE, 7, 0)};   // the archive's buffer without a stream
    const Scenario scenarios[] = {
        {"first", 1, 1, first, sizeof first / sizeof *first},        {"middle", 1, 1, middle, sizeof middle / sizeof *middle},
        {"last", 1, 1, last, sizeof last / sizeof *last},            {"big", 1, 1, big, sizeof big / sizeof *big},
        {"plain", 1, 1, plain, sizeof plain / sizeof *plain},        {"own", 1, 1, own, sizeof own / sizeof *own},
        {"loaded", 1, 0, middle, sizeof middle / sizeof *middle},    {"no check_disk", 0, 1, first, sizeof first / sizeof *first},
        {"direct", 1, 0, direct, sizeof direct / sizeof *direct},    {"direct loaded", 0, 0, direct, sizeof direct / sizeof *direct},
        {"no file", 1, 0, no_open, sizeof no_open / sizeof *no_open},
    };
    for (const Scenario &sc : scenarios)
        run_scenario(t, sc);
}

// ---- The writer ------------------------------------------------------------------------------------

static unsigned writer_hash(unsigned h, const ZZipWriter *w)
{
    if (!w)
        return fnv(h, "\x03null", 6);
    DWORD v[8] = {w->vftable, addr(w->handle), w->owned, w->file.vftable, w->file.stream != nullptr, w->file.flags, w->header_offset, w->data_offset};
    h = text_hash(fnv(fnv(h, v, sizeof v), &w->end, sizeof w->end), w->name);
    if (w->directory)
        h = fnv(h, w->directory, w->end.directory_size);
    return h;
}

static void writer_case(Tally &t, const char *label, int kind, const char *name, int via)
{
    char what[200];
    unsigned h[2], files[2];
    DWORD length[2];
    for (int v = 0; v < 2; v++) {
        const char *file = v ? "r1.zip" : "r0.zip";
        char path[MAX_PATH];
        archive_path(path, file);
        if (kind == 0)
            write_archive(file, g_zip, g_zip_len);
        else if (kind == 1)
            write_archive(file, "not an archive, not at all, no end record anywhere", 50);
        else
            DeleteFileA(path);
        files_open() = 3;
        h[v] = 2166136261u;
        if (via == 0) {   // Create, the slots and the deleting destructor
            auto *w = pick<ZZipWriter *(__fastcall *)(const char *, const char *)>(v, ZW_CREATE)(path, name);
            h[v] = writer_hash(h[v], w);
            if (w) {
                DWORD x[12];
                x[0] = pick<SlotFn2>(v, ZW_WRITE)(w, addr(g_data), 3000);
                x[1] = pick<SlotFn0>(v, ZW_TELL)(w);
                x[2] = pick<SlotFn0>(v, ZW_SIZE)(w);
                x[3] = pick<SlotFn1>(v, ZW_SEEK)(w, 10) & 0xff;
                x[4] = pick<SlotFn2>(v, ZW_WRITE)(w, addr(g_data + 5000), 70000);
                x[5] = pick<SlotFn1>(v, ZW_SKIP)(w, (DWORD)-30000) & 0xff;
                x[6] = pick<SlotFn2>(v, ZW_READ)(w, addr(g_file[1]), 2000);
                x[6] = fnv(x[6], g_file[1], x[6] <= 2000 ? x[6] : 0);
                x[7] = pick<SlotFn1>(v, ZW_SET_SIZE)(w, 50000);
                x[8] = pick<SlotFn0>(v, ZW_SIZE)(w);
                x[9] = pick<SlotFn1>(v, ZW_SEEK)(w, 0) & 0xff;
                x[10] = pick<SlotFn2>(v, ZW_READ)(w, addr(g_file[1]), 100);
                x[10] = fnv(x[10], g_file[1], x[10] <= 100 ? x[10] : 0);
                x[11] = pick<SlotFn0>(v, ZW_TELL)(w);
                h[v] = writer_hash(fnv(h[v], x, sizeof x), w);
                DWORD r = pick<SlotFn1>(v, ZW_DELETE)(w, 1) == addr(w);
                h[v] = fnv(h[v], &r, 4);
            }
        } else if (via == 1) {   // Open and Close directly on a new object; Close twice
            auto *w = static_cast<ZZipWriter *>(pool_alloc(sizeof(ZZipWriter)));
            memset(w, 0xcd, sizeof *w);
            original<ZStream *(THISCALL *)(ZStream *)>(0x4a23a0)(w);
            original<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(&w->file);
            w->vftable = ZIP_WRITER_VTABLE;
            w->directory = nullptr;
            DWORD r = pick<DWORD(THISCALL *)(ZZipWriter *, const char *, const char *)>(v, ZW_OPEN)(w, path, name) & 0xff;
            if (r)
                at<DWORD(THISCALL *)(void *, const void *, DWORD)>(ZW_WRITE)(w, g_data + 77, 1234);
            memset(w->name + lstrlenA(w->name) + 1, 0, sizeof w->name - lstrlenA(w->name) - 1);
            h[v] = writer_hash(fnv(h[v], &r, 4), w);
            pick<SlotFn0>(v, ZW_CLOSE)(w);
            h[v] = writer_hash(h[v], w);
            pick<SlotFn0>(v, ZW_CLOSE)(w);
            h[v] = writer_hash(h[v], w);
            original<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(&w->file);
            original<void(THISCALL *)(ZStream *)>(0x4a2410)(w);
            pool_free(w, sizeof *w);
        } else {   // FileBuf_Open with "archive;name" and flags 3 (File_Open creates the writer)
            char full[MAX_PATH + 100];
            wsprintfA(full, "%s;%s", path, name);
            CFileBuf f;
            original<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(&f);
            DWORD r = pick<bool(THISCALL *)(CFileBuf *, const char *, DWORD)>(v, 0x4a1840)(&f, full, 3) & 0xff;
            DWORD x[3] = {r, f.stream ? f.stream->vftable : 0, f.flags};
            h[v] = fnv(h[v], x, sizeof x);
            if (r) {
                DWORD w = at<DWORD(THISCALL *)(CFileBuf *, const void *, DWORD)>(0x4a19d0)(&f, g_data + 9, 4321);
                h[v] = fnv(h[v], &w, 4);
            }
            original<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(&f);
        }
        h[v] = fnv(h[v], &files_open(), 4);
        files[v] = archive_hash(file, &length[v]);
    }
    wsprintfA(what, "[writer %s %s via %d: archives %x %x bytes]", label, name, via, length[0], length[1]);
    t.check(h[0] == h[1] && files[0] == files[1], "zip stream", what);
}

static void test_writer(Tally &t)
{
    const char *labels[] = {"archive", "garbage", "missing"};
    for (int kind = 0; kind < 3; kind++)
        for (const char *name : {"new/file.txt", "a.txt", ""})
            for (int via = 0; via < 3; via++)
                writer_case(t, labels[kind], kind, name, via);
}

// ---- CRC -------------------------------------------------------------------------------------------

static void test_crc(Tally &t)
{
    for (int k = 0; k < scaled(40); k++) {
        DWORD crc = k < 2 ? (k ? 0 : 0xffffffff) : random();
        DWORD n = k < 20 ? (DWORD)k : random() % 70000;
        DWORD from = random() % 1000;
        compare_call<DWORD(__fastcall *)(DWORD, const void *, DWORD)>(t, 0x5c31b0, "Crc_Update", crc, (const void *)(g_data + from), n);
        compare_call<DWORD(__fastcall *)(DWORD)>(t, 0x5c31a0, "Crc_Final", crc);
    }
}

ZAX_CHECKS(zip_stream)
{
    (void)cls;
    (void)n;
    Tally read{"Zip stream reading"}, write{"Zip stream writing"}, writer{"Zip writer"}, crc{"Zip CRC"};
    char tmp[MAX_PATH];
    if (!GetTempPathA(MAX_PATH - 40, tmp)) {
        log("  zip stream checks: no temp folder");
        return;
    }
    wsprintfA(g_root, "%sZaxZipStream\\", tmp);
    CreateDirectoryA(g_root, nullptr);
    g_zip = static_cast<BYTE *>(VirtualAlloc(nullptr, MAX_ARCHIVE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_data = static_cast<BYTE *>(VirtualAlloc(nullptr, 0x20000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    for (int v = 0; v < 2; v++)
        g_file[v] = static_cast<BYTE *>(VirtualAlloc(nullptr, MAX_ARCHIVE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_seed = 99;
    for (DWORD i = 0; i < 0x20000; i++)
        g_data[i] = (BYTE)(random() >> 9);
    DWORD saved_open = files_open(), saved_max = *at<DWORD *>(FILES_OPEN_MAX);
    DWORD t0 = GetTickCount();
    make_archive(false);
    test_read(read);
    DWORD t1 = GetTickCount();
    log("  zip stream read checks: %u ms", t1 - t0);
    make_archive(false);
    test_write(write);
    make_archive(true);
    test_writer(writer);
    make_archive(false);
    test_writer(writer);
    test_crc(crc);
    log("  zip stream write checks: %u ms", GetTickCount() - t1);
    files_open() = saved_open;
    *at<DWORD *>(FILES_OPEN_MAX) = saved_max;
    remove_archives();
    VirtualFree(g_zip, 0, 0x8000);
    VirtualFree(g_data, 0, 0x8000);
    for (int v = 0; v < 2; v++)
        VirtualFree(g_file[v], 0, 0x8000);
    read.report();
    write.report();
    writer.report();
    crc.report();
}

}  // namespace zax
