// Checks of the disk stream (disk_stream.h): test files in the temp folder (sizes around 64 KB) opened with
// the read, change and create flags and read, sought and skipped against their contents; writes, size
// changes and the created folders compared byte for byte between original and reimplementation; the
// line reader on texts with every CR/LF combination and several buffer sizes (on disk and memory
// streams); File_Open on test files, archive entries and writer paths; the CRC of streams; the
// "~CFileBuf 0x" paths.
#include "check_util.h"
#include "disk_stream.h"
#include "init_list.h"
#include "memory_stream.h"
#include "zip_archive.h"
#include "zip_stream.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT BOOL WINAPI ReadFile(HANDLE f, void *buf, DWORD n, DWORD *read, void *overlapped);
WINIMPORT DWORD WINAPI GetFileAttributesA(const char *path);
WINIMPORT DWORD WINAPI GetTickCount(void);
WINIMPORT BOOL WINAPI VirtualFree(void *addr, size_t size, DWORD type);

namespace zax {

ZZipDir *mounted_archive();                         // checks_file_buffer.cpp
int pick_entries(ZZipDir *d, DWORD *out, int max);  // checks_file_buffer.cpp

namespace {

constexpr DWORD GENERIC_READ_ = 0x80000000, OPEN_EXISTING_ = 3, MEM_RELEASE_ = 0x8000;

template <typename F> F pick(int v, DWORD address) { return v ? at<F>(address) : original<F>(address); }
DWORD addr(const void *p) { return (DWORD)(uintptr_t)p; }

using Slot0 = DWORD(THISCALL *)(void *);
using Slot1 = DWORD(THISCALL *)(void *, DWORD);
using Slot2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
using OpenFn = DWORD(THISCALL *)(void *, const char *, DWORD);
using PathFn = DWORD(__fastcall *)(const char *, DWORD);

char g_root[MAX_PATH];
const DWORD SIZES[] = {0, 1, 100, 4096, 65535, 65536, 65537, 200000};
constexpr int N_SIZES = sizeof SIZES / sizeof *SIZES;
BYTE *g_data[N_SIZES];
BYTE *g_buf[2];
constexpr DWORD BUFFER = 0x100000;

const char *const TEXTS[] = {
    "one\r\ntwo\r\n\r\nthree",
    "a\nb\n\nc\n",
    "a\rb\r\rc",
    "a\n\rb\n\r",
    "a\r\r\nb\r\n\nc\n\r\nd\n\n\re",
    "",
    "no line end at all",
    "0123456789012345678901234567890123456789012345678901234567890123456789\r\nshort\r\n",
    "x\r",
    "\r\n\r\n",
    "\n",
    "tab\tand  spaces \r\n\x01\xff end",
};
constexpr int N_TEXTS = sizeof TEXTS / sizeof *TEXTS;

void file_name(char *out, const char *name) { wsprintfA(out, "%s%s", g_root, name); }
void data_name(char *out, int k) { wsprintfA(out, "%sdata%u.bin", g_root, SIZES[k]); }
void text_name(char *out, int k) { wsprintfA(out, "%stext%d.txt", g_root, k); }
void write_bytes(const char *path, const void *p, DWORD n)
{
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD w;
    WriteFile(f, p, n, &w, nullptr);
    CloseHandle(f);
}
// The whole file (at most max bytes) or 0xffffffff if it cannot be opened
DWORD read_bytes(const char *path, void *p, DWORD max)
{
    HANDLE f = CreateFileA(path, GENERIC_READ_, FILE_SHARE_READ | 2, nullptr, OPEN_EXISTING_, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return 0xffffffff;
    DWORD r = 0;
    ReadFile(f, p, max, &r, nullptr);
    CloseHandle(f);
    return r;
}
unsigned file_hash(const char *path)
{
    DWORD n = read_bytes(path, g_buf[0], BUFFER);
    return n == 0xffffffff ? 0xdead : fnv(fnv(2166136261u, &n, 4), g_buf[0], n);
}

bool build_files()
{
    char tmp[MAX_PATH], p[MAX_PATH];
    if (!GetTempPathA(MAX_PATH - 40, tmp))
        return false;
    wsprintfA(g_root, "%sZaxStreams\\", tmp);
    CreateDirectoryA(g_root, nullptr);
    for (int k = 0; k < N_SIZES; k++) {
        g_data[k] = static_cast<BYTE *>(VirtualAlloc(nullptr, SIZES[k] + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        g_seed = 2000 + k;
        for (DWORD i = 0; i < SIZES[k]; i++)
            g_data[k][i] = (BYTE)(random() >> 13);
        data_name(p, k);
        write_bytes(p, g_data[k], SIZES[k]);
    }
    for (int k = 0; k < N_TEXTS; k++) {
        text_name(p, k);
        write_bytes(p, TEXTS[k], (DWORD)lstrlenA(TEXTS[k]));
    }
    for (int v = 0; v < 2; v++)
        g_buf[v] = static_cast<BYTE *>(VirtualAlloc(nullptr, BUFFER, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    return true;
}
const char *const SCRATCH[] = {"w0.bin", "w1.bin", "f0.bin", "f1.bin", "nf0\\a\\b\\file.bin", "nf1\\a\\b\\file.bin", "nf0\\a\\b", "nf1\\a\\b",
                               "nf0\\a", "nf1\\a", "nf0", "nf1"};
void remove_files()
{
    char p[MAX_PATH];
    for (int k = 0; k < N_SIZES; k++) {
        data_name(p, k);
        DeleteFileA(p);
        VirtualFree(g_data[k], 0, MEM_RELEASE_);
    }
    for (int k = 0; k < N_TEXTS; k++) {
        text_name(p, k);
        DeleteFileA(p);
    }
    for (const char *s : SCRATCH) {
        file_name(p, s);
        if (!DeleteFileA(p))
            RemoveDirectoryA(p);
    }
    for (int v = 0; v < 2; v++)
        VirtualFree(g_buf[v], 0, MEM_RELEASE_);
    if (!RemoveDirectoryA(g_root))
        log("  could not remove %s", g_root);
}

// Open's result: al and whether the rest are the upper bytes of the FILE * (which differ per open)
DWORD open_result(DWORD r, const DiskStream &s) { return (r & 0xff) | ((r & 0xffffff00) == (addr(s.handle) & 0xffffff00) ? 0x100 : 0x200); }
DWORD stream_state(const DiskStream &s) { return (s.vftable == STREAM_VTABLE ? 1 : 0) | (s.handle ? 2 : 0) | (s.owned << 8); }

// Reads, seeks and skips on an open stream through the original (v = 0) or reimplemented slots;
// hashes the results and the bytes read and checks them against the file's contents
unsigned probe_read(int v, DiskStream *s, const BYTE *expect, DWORD size, bool *ok)
{
    unsigned h = 2166136261u;
    auto rec = [&](DWORD r) { h = fnv(h, &r, 4); };
    BYTE *b = g_buf[v];
    rec(pick<Slot0>(v, 0x4a2ba0)(s));   // size
    rec(pick<Slot0>(v, 0x4a2b80)(s));   // tell
    DWORD pos = 0;
    for (DWORD chunk : {1u, 7u, 4096u, 65536u, 300000u}) {
        DWORD r = pick<Slot2>(v, 0x4a2910)(s, addr(b), chunk);
        rec(r);
        DWORD want = pos >= size ? 0 : (size - pos < chunk ? size - pos : chunk);
        if (r != want || memcmp(b, expect + pos, r) != 0)
            *ok = false;
        h = fnv(h, b, r);
        pos += r;
        rec(pick<Slot0>(v, 0x4a2b80)(s));
    }
    for (DWORD to : {(DWORD)0, size / 2, size, size + 100}) {
        rec(pick<Slot1>(v, 0x4a2970)(s, to));
        rec(pick<Slot0>(v, 0x4a2b80)(s));
        DWORD r = pick<Slot2>(v, 0x4a2910)(s, addr(b), 10);
        rec(r);
        if (to < size && memcmp(b, expect + to, r) != 0)
            *ok = false;
        h = fnv(h, b, r);
    }
    for (DWORD d : {(DWORD)-3, (DWORD)5, (DWORD)-(int)(size + 200), (DWORD)0}) {
        rec(pick<Slot1>(v, 0x4a2940)(s, d));
        rec(pick<Slot0>(v, 0x4a2b80)(s));
    }
    rec(pick<Slot0>(v, 0x4a2ba0)(s));
    return h;
}

void test_reads(Tally &t)
{
    char path[MAX_PATH], d[260];
    for (int k = 0; k < N_SIZES; k++) {
        data_name(path, k);
        for (DWORD flags : {0x24u, 0x26u, 4u, 0x2u, 0xa4u}) {
            unsigned h[2];
            bool ok = true;
            for (int v = 0; v < 2; v++) {
                DiskStream s;
                memset(&s, 0x5a, sizeof s);
                DWORD c = pick<Slot0>(v, 0x4a23a0)(&s);
                h[v] = fnv(2166136261u, &c, 0);
                DWORD r = open_result(pick<OpenFn>(v, 0x4a2760)(&s, path, flags), s);
                DWORD st = stream_state(s);
                h[v] = fnv(fnv(fnv(h[v], &r, 4), &st, 4), "", c == addr(&s));
                if ((r & 0xff) != 1)
                    ok = false;
                h[v] = fnv(h[v], "p", 1);
                if (r & 0xff)
                    h[v] ^= probe_read(v, &s, g_data[k], SIZES[k], &ok);
                DWORD closed = pick<Slot0>(v, 0x4a29a0)(&s), again = pick<Slot0>(v, 0x4a29a0)(&s);
                st = stream_state(s);
                DWORD dt = pick<Slot0>(v, 0x4a2410)(&s);
                DWORD w[4] = {closed, again, st, dt};
                h[v] = fnv(h[v], w, sizeof w);
            }
            wsprintfA(d, "[disk read %u bytes flags %x: %x %x%s]", SIZES[k], flags, h[0], h[1], ok ? "" : ", bytes differ from the file");
            t.check(h[0] == h[1] && ok, "disk stream", d);
        }
    }
    // Missing files, a relative path that is not there, a stream without a file
    char missing[MAX_PATH];
    file_name(missing, "nothing\\here.bin");
    for (const char *p : {(const char *)missing, "ZaxNoSuchFile.bin"})
        for (DWORD flags : {4u, 0x24u, 2u, 0x22u, 0x23u, 3u}) {
            if ((flags & 1) && p != missing)
                continue;   // would create a file below the data folder or the game folder
            DWORD r[2], st[2];
            for (int v = 0; v < 2; v++) {
                DiskStream s;
                pick<Slot0>(v, 0x4a23a0)(&s);
                r[v] = pick<OpenFn>(v, 0x4a2760)(&s, p, flags);
                st[v] = stream_state(s);
            }
            wsprintfA(d, "[disk open missing %.120s flags %x: %x/%x %x/%x]", p, flags, r[0], st[0], r[1], st[1]);
            t.check(r[0] == r[1] && st[0] == st[1] && r[0] == 0, "disk stream", d);
        }
    unsigned h[2];
    for (int v = 0; v < 2; v++) {
        DiskStream s;
        pick<Slot0>(v, 0x4a23a0)(&s);
        BYTE b[4] = {1, 2, 3, 4};
        DWORD w[10] = {pick<Slot2>(v, 0x4a28e0)(&s, addr(b), 4), pick<Slot2>(v, 0x4a2910)(&s, addr(b), 4), pick<Slot1>(v, 0x4a2940)(&s, 1),
                       pick<Slot1>(v, 0x4a2970)(&s, 1),          pick<Slot0>(v, 0x4a2b80)(&s),               pick<Slot0>(v, 0x4a2ba0)(&s),
                       pick<Slot1>(v, 0x4a2bf0)(&s, 9),          pick<Slot0>(v, 0x4a29a0)(&s),               pick<Slot0>(v, 0x4a2410)(&s),
                       stream_state(s)};
        h[v] = fnv(2166136261u, w, sizeof w);
    }
    t.check(h[0] == h[1], "disk stream", "[slots without a file]");
    // A handle that is not owned: Close and the destructor return it, without fclose
    for (DWORD fn : {0x4a29a0u, 0x4a2410u}) {
        for (int v = 0; v < 2; v++) {
            DiskStream s;
            pick<Slot0>(v, 0x4a23a0)(&s);
            s.handle = reinterpret_cast<void *>(0x12345678);
            s.owned = 0;
            h[v] = pick<Slot0>(v, fn)(&s) ^ stream_state(s);
        }
        wsprintfA(d, "[0x%x on a handle that is not owned: %x %x]", fn, h[0], h[1]);
        t.check(h[0] == h[1] && h[0] == 0x12345678 + 1, "disk stream", d);
    }
}

// Writes, seeks, size changes and changes of an existing file; the files byte for byte
void test_writes(Tally &t)
{
    char d[260];
    unsigned h[2], fh[2];
    for (int v = 0; v < 2; v++) {
        char path[MAX_PATH];
        file_name(path, v ? "w1.bin" : "w0.bin");
        DiskStream s;
        pick<Slot0>(v, 0x4a23a0)(&s);
        h[v] = 2166136261u;
        auto rec = [&](DWORD r) { h[v] = fnv(h[v], &r, 4); };
        rec(open_result(pick<OpenFn>(v, 0x4a2760)(&s, path, 0x23), s));
        rec(stream_state(s));
        for (int k = 0; k < N_SIZES; k++) {
            rec(pick<Slot2>(v, 0x4a28e0)(&s, addr(g_data[k]), SIZES[k]));
            rec(pick<Slot0>(v, 0x4a2b80)(&s));
        }
        rec(pick<Slot0>(v, 0x4a2ba0)(&s));
        rec(pick<Slot1>(v, 0x4a2970)(&s, 1000));
        rec(pick<Slot2>(v, 0x4a28e0)(&s, addr(g_data[7]), 5000));
        rec(pick<Slot1>(v, 0x4a2940)(&s, (DWORD)-200));
        rec(pick<Slot2>(v, 0x4a28e0)(&s, addr("overwritten"), 11));
        rec(pick<Slot1>(v, 0x4a2970)(&s, 400000));
        rec(pick<Slot2>(v, 0x4a28e0)(&s, addr("behind the end"), 14));
        rec(pick<Slot0>(v, 0x4a2ba0)(&s));
        rec(pick<Slot0>(v, 0x4a29a0)(&s));
        // Change the existing file: read, overwrite, cut and grow it
        rec(open_result(pick<OpenFn>(v, 0x4a2760)(&s, path, 0x22), s));
        rec(pick<Slot0>(v, 0x4a2ba0)(&s));
        rec(pick<Slot2>(v, 0x4a2910)(&s, addr(g_buf[v]), 3000));
        h[v] = fnv(h[v], g_buf[v], 3000);
        rec(pick<Slot1>(v, 0x4a2970)(&s, 10));
        rec(pick<Slot2>(v, 0x4a28e0)(&s, addr("changed"), 7));
        rec(pick<Slot1>(v, 0x4a2970)(&s, 0));
        rec(pick<Slot1>(v, 0x4a2bf0)(&s, 70000));
        rec(pick<Slot0>(v, 0x4a2ba0)(&s));
        rec(pick<Slot1>(v, 0x4a2bf0)(&s, 90000));
        rec(pick<Slot0>(v, 0x4a2ba0)(&s));
        rec(pick<Slot2>(v, 0x4a2910)(&s, addr(g_buf[v]), 100));
        h[v] = fnv(h[v], g_buf[v], 100);
        rec(pick<Slot0>(v, 0x4a29a0)(&s));
        // Read-only open: writes fail
        rec(open_result(pick<OpenFn>(v, 0x4a2760)(&s, path, 0x24), s));
        rec(pick<Slot2>(v, 0x4a28e0)(&s, addr("x"), 1));
        rec(pick<Slot1>(v, 0x4a2bf0)(&s, 5));
        rec(pick<Slot0>(v, 0x4a2410)(&s));
        fh[v] = file_hash(path);
    }
    wsprintfA(d, "[disk writes: results %x %x, files %x %x]", h[0], h[1], fh[0], fh[1]);
    t.check(h[0] == h[1] && fh[0] == fh[1], "disk stream", d);

    // Create with missing folders: only with 0x100; an absolute path without 0x20 cannot be created
    for (DWORD flags : {0x23u, 0x123u, 3u}) {
        DWORD r[2], attrs[2];
        for (int v = 0; v < 2; v++) {
            char path[MAX_PATH], folder[MAX_PATH];
            file_name(path, v ? "nf1\\a\\b\\file.bin" : "nf0\\a\\b\\file.bin");
            file_name(folder, v ? "nf1\\a\\b" : "nf0\\a\\b");
            DiskStream s;
            pick<Slot0>(v, 0x4a23a0)(&s);
            r[v] = open_result(pick<OpenFn>(v, 0x4a2760)(&s, path, flags), s);
            if (s.handle)
                pick<Slot2>(v, 0x4a28e0)(&s, addr("new"), 3);
            pick<Slot0>(v, 0x4a2410)(&s);
            attrs[v] = GetFileAttributesA(folder) ^ (file_hash(path) << 8);
        }
        wsprintfA(d, "[disk create in missing folders flags %x: %x %x, folder %x %x]", flags, r[0], r[1], attrs[0], attrs[1]);
        t.check(r[0] == r[1] && attrs[0] == attrs[1], "disk stream", d);
    }
}

// The line reader (0x4a2ad0) on a disk and a memory stream with all texts and buffer sizes
void test_lines(Tally &t)
{
    char path[MAX_PATH], d[260];
    char line[2][1100];
    for (int k = 0; k < N_TEXTS; k++) {
        text_name(path, k);
        DWORD length = (DWORD)lstrlenA(TEXTS[k]);
        for (DWORD n : {2u, 3u, 4u, 8u, 64u, 1024u})
            for (int kind = 0; kind < 2; kind++) {
                unsigned h[2];
                int lines = 0;
                for (int v = 0; v < 2; v++) {
                    DiskStream disk;
                    MemoryStream *mem = nullptr;
                    ZStream *s;
                    if (kind == 0) {
                        at<Slot0>(0x4a23a0)(&disk);
                        at<OpenFn>(0x4a2760)(&disk, path, 0x24);
                        s = &disk;
                    } else {
                        mem = static_cast<MemoryStream *>(pool_alloc(sizeof(MemoryStream)));
                        BYTE *copy = static_cast<BYTE *>(heap_alloc(length + 1));
                        memcpy(copy, TEXTS[k], length + 1);
                        at<MemoryStream *(THISCALL *)(MemoryStream *, BYTE *, DWORD)>(0x505fb0)(mem, copy, length);
                        s = mem;
                    }
                    h[v] = 2166136261u;
                    for (lines = 0; lines < 40; lines++) {
                        memset(line[v], 0x77, sizeof line[v]);
                        DWORD r = pick<Slot2>(v, 0x4a2ad0)(s, addr(line[v]), n);
                        DWORD pos = vcall<DWORD>(s, STREAM_TELL);
                        h[v] = fnv(fnv(fnv(h[v], &r, 4), &pos, 4), line[v], n + 1);
                        if (!(r & 0xff))
                            break;
                    }
                    if (kind == 0)
                        at<Slot0>(0x4a2410)(&disk);
                    else
                        vcall<void *>(mem, STREAM_DELETE, 1u);
                }
                wsprintfA(d, "[line reader text %d n %u on %s: %x %x, %d lines]", k, n, kind ? "memory" : "disk", h[0], h[1], lines);
                t.check(h[0] == h[1], "disk stream", d);
            }
    }
}

// File_Open on test files, archive entries and writer paths: the stream's kind, owner flag, size and
// contents; then deleted through slot 0
unsigned stream_hash(ZStream *s, int v)
{
    if (!s)
        return 0x0;
    DWORD w[3] = {s->vftable, s->owned, vcall<DWORD>(s, STREAM_SIZE)};
    unsigned h = fnv(2166136261u, w, sizeof w);
    DWORD n = vcall<DWORD>(s, STREAM_READ, g_buf[v], BUFFER);
    return fnv(fnv(h, &n, 4), g_buf[v], n);
}

void test_file_open(Tally &t)
{
    char d[300], data[MAX_PATH], missing[MAX_PATH], w[2][MAX_PATH], writer[2][MAX_PATH];
    data_name(data, 6);
    file_name(missing, "missing.bin");
    for (int v = 0; v < 2; v++) {
        file_name(w[v], v ? "f1.bin" : "f0.bin");
        wsprintfA(writer[v], "%snozip%d.zip;inner.txt", g_root, v);
    }
    struct Case {
        const char *path[2];
        DWORD flags;
    };
    Case cases[] = {
        {{data, data}, 0xa4}, {{data, data}, 4},     {{data, data}, 0x24},       {{data, data}, 0x44},   {{data, data}, 0x204},
        {{data, data}, 0},    {{data, data}, 0x20},  {{missing, missing}, 0xa4}, {{missing, missing}, 4}, {{w[0], w[1]}, 0xa3},
        {{w[0], w[1]}, 0xa2}, {{w[0], w[1]}, 0xe3},  {{w[0], w[1]}, 0x23},       {{w[0], w[1]}, 0x63},   {{writer[0], writer[1]}, 3},
        {{missing, missing}, 0xa2},
    };
    for (const Case &c : cases) {
        unsigned h[2];
        for (int v = 0; v < 2; v++) {
            ZStream *s = pick<ZStream *(__fastcall *)(const char *, DWORD)>(v, 0x4a2440)(c.path[v], c.flags);
            h[v] = stream_hash(s, v);
            if (s && (c.flags & 2)) {
                vcall<DWORD>(s, STREAM_WRITE, "written", 7u);
                vcall<DWORD>(s, STREAM_CLOSE);
            }
            if (s)
                vcall<void *>(s, STREAM_DELETE, 1u);
            h[v] = fnv(h[v], "f", 1) ^ (c.flags & 2 ? file_hash(c.path[v]) : 0);
        }
        wsprintfA(d, "[File_Open %.150s flags %x: %x %x]", c.path[1], c.flags, h[0], h[1]);
        t.check(h[0] == h[1], "File_Open", d);
    }
    ZZipDir *dir = mounted_archive();
    if (!dir) {
        log("  disk stream: no loaded archive mounted");
        return;
    }
    DWORD idx[8];
    int n = pick_entries(dir, idx, 8);
    for (int k = 0; k < n; k++) {
        const char *name = dir->entries[idx[k]].name;
        for (DWORD flags : {4u, 0x24u, 0x44u, 0x14u}) {
            unsigned h[2];
            for (int v = 0; v < 2; v++) {
                ZStream *s = pick<ZStream *(__fastcall *)(const char *, DWORD)>(v, 0x4a2440)(name, flags);
                h[v] = stream_hash(s, v);
                if (s)
                    vcall<void *>(s, STREAM_DELETE, 1u);
            }
            wsprintfA(d, "[File_Open archive entry %.150s flags %x: %x %x]", name, flags, h[0], h[1]);
            t.check(h[0] == h[1], "File_Open", d);
        }
    }
}

// The CRC of disk and memory streams, also against the CRC of the bytes themselves
void test_crc(Tally &t)
{
    char path[MAX_PATH], d[200];
    for (int k = 0; k < N_SIZES; k++) {
        data_name(path, k);
        DWORD expect = Crc_Final(Crc_Update(0xffffffff, g_data[k], SIZES[k]));
        DWORD r[2], m[2];
        for (int v = 0; v < 2; v++) {
            DiskStream s;
            at<Slot0>(0x4a23a0)(&s);
            at<OpenFn>(0x4a2760)(&s, path, 0x24);
            vcall<DWORD>(&s, STREAM_SEEK, 77u);
            r[v] = pick<DWORD(__fastcall *)(ZStream *)>(v, 0x4a26c0)(&s);
            at<Slot0>(0x4a2410)(&s);
            ZStream *mem = at<ZStream *(__fastcall *)(const char *, DWORD)>(0x505e40)(path, 0x24);
            m[v] = mem ? pick<DWORD(__fastcall *)(ZStream *)>(v, 0x4a26c0)(mem) : 0xdead;
            if (mem)
                vcall<void *>(mem, STREAM_DELETE, 1u);
        }
        wsprintfA(d, "[Stream_Crc %u bytes: disk %x %x, memory %x %x, expected %x]", SIZES[k], r[0], r[1], m[0], m[1], expect);
        t.check(r[0] == r[1] && m[0] == m[1] && r[0] == expect && m[0] == expect, "Stream_Crc", d);
    }
}

// "~CFileBuf 0x" paths: building, parsing, deleting
void test_file_buf_paths(Tally &t)
{
    char d[300];
    const char *prefix = *at<const char **>(FILE_BUF_PREFIX);
    DWORD fake = 0x12345678;
    for (DWORD stream : {fake, (DWORD)0, (DWORD)0x00ab00cd})
        for (const char *name : {"Levels/x.zax", "", "a"}) {
            StringState st[2];
            for (int v = 0; v < 2; v++) {
                ZString z;
                at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&z);
                pick<void(__fastcall *)(DWORD, DWORD, ZString *, const char *)>(v, 0x4a29d0)(stream, 0x99, &z, name);
                st[v] = state(z, nullptr);
                at<void(THISCALL *)(ZString *)>(0x4defd0)(&z);
            }
            wsprintfA(d, "[Stream_FileBufPath %x %s: %.60s / %.60s]", stream, name, st[0].text, st[1].text);
            t.check(memcmp(&st[0], &st[1], sizeof st[0]) == 0, "file buffer paths", d);
        }
    char texts[8][64];
    wsprintfA(texts[0], "%s78563412/name", prefix);
    wsprintfA(texts[1], "%s1234567", prefix);
    wsprintfA(texts[2], "%s12345678", prefix);
    wsprintfA(texts[3], "%s", prefix);
    wsprintfA(texts[4], "~CFileBuf 0X12345678");
    wsprintfA(texts[5], "%s00000000", prefix);
    wsprintfA(texts[6], "x%s12345678", prefix);
    texts[7][0] = 0;
    for (int k = 0; k < 9; k++) {
        const char *p = k < 8 ? texts[k] : nullptr;
        DWORD r[2];
        for (int v = 0; v < 2; v++)
            r[v] = addr(pick<ZStream *(__fastcall *)(const char *)>(v, 0x4a2a10)(p));
        wsprintfA(d, "[Stream_FromFileBufPath %.60s: %x %x]", p ? p : "(null)", r[0], r[1]);
        t.check(r[0] == r[1], "file buffer paths", d);
        if (k == 0 || k == 2)
            continue;   // would delete the made-up address
        for (int v = 0; v < 2; v++)
            r[v] = pick<DWORD(__fastcall *)(const char *)>(v, 0x4a2a70)(p);
        wsprintfA(d, "[Stream_DeleteFileBufPath %.60s: %x %x]", p ? p : "(null)", r[0], r[1]);
        t.check(r[0] == r[1], "file buffer paths", d);
    }
    // Deleting real streams: the deleting destructor's result (the stream) and the pool block
    for (int v = 0; v < 2; v++) {
        DiskStream *s = static_cast<DiskStream *>(pool_alloc(sizeof(DiskStream)));
        at<Slot0>(0x4a23a0)(s);
        char p[64];
        DWORD a = addr(s);
        char hex[12];
        text_hex(hex, &a, 4);
        wsprintfA(p, "%s%s/file", prefix, hex);
        DWORD found = addr(pick<ZStream *(__fastcall *)(const char *)>(v, 0x4a2a10)(p));
        DWORD r = pick<DWORD(__fastcall *)(const char *)>(v, 0x4a2a70)(p);
        wsprintfA(d, "[Stream_DeleteFileBufPath of a stream (%s): found %x, result %x, stream %x]", v ? "reimpl" : "original", found, r, a);
        t.check(found == a && r == a, "file buffer paths", d);
    }
}

}  // namespace

ZAX_CHECKS(disk_stream)
{
    (void)cls;
    (void)n;
    Tally reads{"Disk stream (reads)"}, writes{"Disk stream (writes)"}, lines{"Line reader"}, open{"File_Open"}, crc{"Stream_Crc"},
        paths{"~CFileBuf paths"};
    DWORD files_open = *at<DWORD *>(FILES_OPEN), files_max = *at<DWORD *>(FILES_OPEN_MAX);
    DWORD t0 = GetTickCount();
    if (!build_files()) {
        log("  disk stream checks: no temp folder");
        return;
    }
    test_reads(reads);
    test_writes(writes);
    test_lines(lines);
    test_file_open(open);
    test_crc(crc);
    test_file_buf_paths(paths);
    *at<DWORD *>(FILES_OPEN) = files_open;
    *at<DWORD *>(FILES_OPEN_MAX) = files_max;
    remove_files();
    log("  disk stream checks: %u ms", GetTickCount() - t0);
    reads.report();
    writes.report();
    lines.report();
    open.report();
    crc.report();
    paths.report();
}

}  // namespace zax
