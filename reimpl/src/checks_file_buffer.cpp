// Checks of the file buffer (file_buffer.h): every CFileBuf method on recording streams of our own (data,
// short reads and writes, null streams), FileBuf_Open and the file functions on test files in the temp
// folder (sizes around the 2,000-byte, 64 KB and 512 KB boundaries, read byte for byte with seeks and at
// the end) and on real game files from the disk and the mounted archive, compared with the bytes of the
// archive file itself.
#include "check_util.h"
#include "file_buffer.h"
#include "init_list.h"
#include "zip_archive.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT BOOL WINAPI ReadFile(HANDLE f, void *buf, DWORD n, DWORD *read, void *overlapped);
WINIMPORT DWORD WINAPI SetFilePointer(HANDLE f, long distance, long *high, DWORD method);
WINIMPORT DWORD WINAPI GetFileAttributesA(const char *path);
WINIMPORT DWORD WINAPI GetTickCount(void);
WINIMPORT BOOL WINAPI VirtualFree(void *addr, size_t size, DWORD type);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);

namespace zax {

constexpr DWORD GENERIC_READ_ = 0x80000000, OPEN_EXISTING_ = 3, MEM_RELEASE_ = 0x8000;

template <typename F> static F pick(int v, DWORD address) { return v ? at<F>(address) : original<F>(address); }
static DWORD addr(const void *p) { return (DWORD)(uintptr_t)p; }
static DWORD &files_open() { return *at<DWORD *>(FILES_OPEN); }
// bool functions (fastcall path, flags): only al is the result
static void compare_low(Tally &t, DWORD address, const char *what, const char *path, DWORD flags)
{
    DWORD o = original<DWORD(__fastcall *)(const char *, DWORD)>(address)(path, flags) & 0xff;
    DWORD r = at<DWORD(__fastcall *)(const char *, DWORD)>(address)(path, flags) & 0xff;
    char d[200];
    wsprintfA(d, "[0x%x %s %.100s %x: original %x, reimpl %x]", address, what, path, flags, o, r);
    t.check(o == r, "", d);
}
static DWORD &files_open_max() { return *at<DWORD *>(FILES_OPEN_MAX); }

// ---- A stream of our own that records every call --------------------------------------------------

struct FakeStream : ZStream {
    BYTE *data;
    DWORD capacity, size, pos;
    DWORD max_io;      // reads and writes stop after this many bytes
    unsigned log;      // hash over all calls
    DWORD calls, deleted;
};
static void record(FakeStream *s, DWORD slot, DWORD a, DWORD b)
{
    DWORD r[3] = {slot, a, b};
    s->log = fnv(s->log, r, sizeof r);
    s->calls++;
}
static FakeStream *THISCALL fs_delete(FakeStream *s, DWORD flags)
{
    record(s, 0, flags, 0);
    s->deleted = flags;
    return s;
}
static DWORD THISCALL fs_unused(FakeStream *s)
{
    record(s, 1, 0, 0);
    return 0x1111;
}
static DWORD THISCALL fs_write(FakeStream *s, const void *p, DWORD n)
{
    DWORD w = n < s->max_io ? n : s->max_io;
    if (s->pos + w > s->capacity)
        w = s->capacity > s->pos ? s->capacity - s->pos : 0;
    memcpy(s->data + s->pos, p, w);
    record(s, 2, n, fnv(2166136261u, p, n));
    s->pos += w;
    if (s->pos > s->size)
        s->size = s->pos;
    return w;
}
static DWORD THISCALL fs_read(FakeStream *s, void *p, DWORD n)
{
    DWORD rest = s->pos < s->size ? s->size - s->pos : 0;
    DWORD w = n < s->max_io ? n : s->max_io;
    if (w > rest)
        w = rest;
    memcpy(p, s->data + s->pos, w);
    s->pos += w;
    record(s, 3, n, w);
    return w;
}
static bool THISCALL fs_skip(FakeStream *s, DWORD d)
{
    DWORD p = s->pos + d;
    bool ok = p <= s->size;
    if (ok)
        s->pos = p;
    record(s, 4, d, ok);
    return ok;
}
static bool THISCALL fs_seek(FakeStream *s, DWORD p)
{
    bool ok = p <= s->size;
    if (ok)
        s->pos = p;
    record(s, 5, p, ok);
    return ok;
}
static DWORD THISCALL fs_tell(FakeStream *s)
{
    record(s, 6, 0, 0);
    return s->pos;
}
static DWORD THISCALL fs_size(FakeStream *s)
{
    record(s, 7, 0, 0);
    return s->size;
}
static DWORD THISCALL fs_set_size(FakeStream *s, DWORD n)
{
    record(s, 8, n, 0);
    s->size = n < s->capacity ? n : s->capacity;
    return 0x5e7;
}
static DWORD THISCALL fs_close(FakeStream *s)
{
    record(s, 9, 0, 0);
    return 0xc105e;
}
static BYTE *THISCALL fs_advance(FakeStream *s, DWORD n)
{
    record(s, 11, n, 0);
    BYTE *r = s->data + s->pos;
    s->pos += n;
    return r;
}
static BYTE *THISCALL fs_detach(FakeStream *s)
{
    record(s, 12, 0, 0);
    return s->data;
}
static bool THISCALL fs_read_line(FakeStream *s, char *p, DWORD n)
{
    DWORD k = 0;
    while (k + 1 < n && s->pos < s->size && s->data[s->pos] != '\n')
        p[k++] = (char)s->data[s->pos++];
    p[k] = 0;
    record(s, 13, n, k);
    return k > 0;
}
static DWORD g_fake_vtable[14];
static void init_fake_vtable()
{
    void *f[14] = {(void *)&fs_delete, (void *)&fs_unused, (void *)&fs_write,    (void *)&fs_read,    (void *)&fs_skip,
                   (void *)&fs_seek,   (void *)&fs_tell,   (void *)&fs_size,     (void *)&fs_set_size, (void *)&fs_close,
                   (void *)&fs_unused, (void *)&fs_advance, (void *)&fs_detach,  (void *)&fs_read_line};
    for (int i = 0; i < 14; i++)
        g_fake_vtable[i] = addr(f[i]);
}
// Contents: byte i from the seed; `differ` changes one byte (for Equals)
static void fake_init(FakeStream *s, DWORD size, DWORD extra, unsigned seed, DWORD differ = 0xffffffff)
{
    s->vftable = addr(g_fake_vtable);
    s->handle = nullptr;
    s->owned = 0;
    s->capacity = size + extra;
    s->data = static_cast<BYTE *>(heap_alloc(s->capacity ? s->capacity : 1));
    for (DWORD i = 0; i < s->capacity; i++)
        s->data[i] = (BYTE)((i * 7 + (i >> 8) * 13 + seed) ^ (i >> 16));
    if (differ < size)
        s->data[differ] ^= 0x5a;
    s->size = size;
    s->pos = 0;
    s->max_io = 0xffffffff;
    s->log = 2166136261u;
    s->calls = 0;
    s->deleted = 0xffffffff;
}
static unsigned fake_hash(unsigned h, const FakeStream *s)
{
    h = fnv(h, s->data, s->size);
    DWORD v[5] = {s->size, s->pos, s->log, s->calls, s->deleted};
    return fnv(h, v, sizeof v);
}

// ---- The fake cases --------------------------------------------------------------------------------

struct Env {
    CFileBuf f, g;   // g: the second buffer of CopyTo and Equals
    FakeStream a, b;
};
static DWORD stream_id(const Env &e, const ZStream *s)
{
    if (!s)
        return 0;
    if (s == &e.a)
        return 1;
    if (s == &e.b)
        return 2;
    return 3;
}
static unsigned buf_hash(unsigned h, const Env &e, const CFileBuf &f)
{
    DWORD v[7] = {f.vftable, stream_id(e, f.stream), f.indent, f.flags, f.small_reads, f.advanced, f.option18};
    return fnv(h, v, sizeof v);
}
static DWORD rel(const Env &e, const void *p)
{
    if (p == &e.f)
        return 0x7f000001;
    if (p == &e.g)
        return 0x7f000002;
    if (p >= e.a.data && p < e.a.data + e.a.capacity + 1)
        return 0x7e000000 + (DWORD)((const BYTE *)p - e.a.data);
    return addr(p);
}

static unsigned g_progress_hash;
static DWORD g_progress_calls;
static void STDCALL rec_progress(float f)
{
    g_progress_hash = fnv(g_progress_hash, &f, 4);
    g_progress_calls++;
}

struct Setup {
    DWORD size_a = 64, size_b = 64, extra = 64;
    bool null_a = false, null_b = false, g_has_b = false;
    DWORD max_io = 0xffffffff;
    unsigned seed_b = 99;
    DWORD differ = 0xffffffff;
};

// Runs fn(v, env) for the original (v = 0) and the reimplementation (v = 1) on equal start states
// and compares the result (fn normalises it), both buffers, both streams, the counters and progress
template <typename Fn> static void fake_case(Tally &t, const char *what, const Setup &s, Fn fn)
{
    unsigned h[2];
    DWORD r[2];
    for (int v = 0; v < 2; v++) {
        Env e;
        fake_init(&e.a, s.size_a, s.extra, 17);
        fake_init(&e.b, s.size_b, s.extra, s.seed_b, s.differ);
        e.a.max_io = e.b.max_io = s.max_io;
        e.f = {FILE_BUF_VTABLE, s.null_a ? nullptr : &e.a, 2, 0x44, 5, 7, 3};
        e.g = {FILE_BUF_VTABLE, s.g_has_b && !s.null_b ? &e.b : nullptr, 1, 0x84, 9, 0, 0};
        files_open() = 10;
        files_open_max() = 11;
        g_progress_hash = 2166136261u;
        g_progress_calls = 0;
        *at<int *(__cdecl *)()>(0x5d1014)() = 77;
        g_seed = 4711;
        r[v] = fn(v, e);
        unsigned x = buf_hash(buf_hash(2166136261u, e, e.f), e, e.g);
        x = fake_hash(fake_hash(x, &e.a), &e.b);
        DWORD c[5] = {files_open(), files_open_max(), g_progress_hash, g_progress_calls, (DWORD)*at<int *(__cdecl *)()>(0x5d1014)()};
        h[v] = fnv(x, c, sizeof c);
        heap_free(e.a.data);
        heap_free(e.b.data);
    }
    char d[200];
    wsprintfA(d, "[%.120s: %x %x]", what, r[0], r[1]);
    t.check(h[0] == h[1] && r[0] == r[1], "file buffer", d);
}

using BufFn0 = DWORD(THISCALL *)(CFileBuf *);
using BufFn1 = DWORD(THISCALL *)(CFileBuf *, DWORD);
using BufFn2 = DWORD(THISCALL *)(CFileBuf *, DWORD, DWORD);
using BufFn3 = DWORD(THISCALL *)(CFileBuf *, DWORD, DWORD, DWORD);

static void test_fake(Tally &t)
{
    char what[120];
    // Constructor on prefilled memory
    {
        BYTE obj[2][0x1c];
        DWORD r[2];
        for (int v = 0; v < 2; v++) {
            memset(obj[v], 0xcd, 0x1c);
            r[v] = (DWORD)(pick<CFileBuf *(THISCALL *)(void *)>(v, 0x4a1780)(obj[v])) - addr(obj[v]);
        }
        t.check(memcmp(obj[0], obj[1], 0x1c) == 0 && r[0] == r[1], "file buffer", "[constructor]");
    }
    // Close, the destructors (also with a pool block and flags 1), Attach
    for (int null_a = 0; null_a < 2; null_a++) {
        Setup s;
        s.null_a = null_a;
        fake_case(t, "Close", s, [](int v, Env &e) { return rel(e, (void *)pick<BufFn0>(v, 0x4a2100)(&e.f)); });
        fake_case(t, "Dtor", s, [](int v, Env &e) { return rel(e, (void *)pick<BufFn0>(v, 0x4a1800)(&e.f)); });
        fake_case(t, "DeletingDtor 0", s, [](int v, Env &e) { return rel(e, (void *)pick<BufFn1>(v, 0x4a17a0)(&e.f, 0)); });
        fake_case(t, "DeletingDtor 1", s, [](int v, Env &e) {
            auto *p = static_cast<CFileBuf *>(pool_alloc(0x1c));
            *p = e.f;
            DWORD r = pick<BufFn1>(v, 0x4a17a0)(p, 1) == (DWORD)addr(p);
            e.f.stream = nullptr;
            return r;
        });
        for (int which = 0; which < 3; which++)
            for (DWORD flags : {0u, 4u, 0x23u}) {
                wsprintfA(what, "Attach %d %d %x", null_a, which, flags);
                fake_case(t, what, s, [which, flags](int v, Env &e) {
                    ZStream *st = which == 0 ? nullptr : which == 1 ? (ZStream *)&e.b : (ZStream *)&e.a;
                    return pick<BufFn2>(v, 0x4a1970)(&e.f, addr(st), flags) & 0xff;
                });
            }
    }
    // Writes
    const char *texts[] = {"", "x", "Hello, world", LONG_TEXT};
    for (int null_a = 0; null_a < 2; null_a++)
        for (DWORD max_io : {0xffffffffu, 3u, 0u}) {
            Setup s;
            s.null_a = null_a;
            s.max_io = max_io;
            for (DWORD n : {0u, 1u, 5u, 40u}) {
                wsprintfA(what, "Write %u null %d io %x", n, null_a, max_io);
                fake_case(t, what, s, [n](int v, Env &e) { return pick<BufFn2>(v, 0x4a19d0)(&e.f, addr(LONG_TEXT), n); });
            }
            for (const char *tx : texts) {
                wsprintfA(what, "WriteChar/WriteText/WriteCountedText %.20s null %d io %x", tx, null_a, max_io);
                fake_case(t, what, s, [tx, null_a](int v, Env &e) {
                    DWORD r1 = pick<BufFn1>(v, 0x4a19f0)(&e.f, (BYTE)tx[0] + 0x100);
                    DWORD r2 = pick<BufFn1>(v, 0x4a1a10)(&e.f, addr(tx));
                    DWORD r3 = rel(e, (void *)pick<BufFn1>(v, 0x4a2220)(&e.f, addr(tx)));
                    return null_a ? r3 : r1 * 0x10000 + r2 * 0x100 + r3;
                });
                for (DWORD indent = 0; indent < 4; indent++) {
                    wsprintfA(what, "WriteLine %.20s indent %u null %d io %x", tx, indent, null_a, max_io);
                    fake_case(t, what, s, [tx, indent](int v, Env &e) {
                        e.f.indent = indent;
                        DWORD r = pick<BufFn1>(v, 0x4a1a40)(&e.f, addr(tx));
                        return r + pick<BufFn1>(v, 0x4a1a40)(&e.f, 0) * 0x100;
                    });
                    wsprintfA(what, "Indent/Unindent %u null %d io %x", indent, null_a, max_io);
                    fake_case(t, what, s, [indent, null_a](int v, Env &e) {
                        e.f.indent = indent;
                        DWORD r = pick<BufFn0>(v, 0x4a1ae0)(&e.f);
                        r += pick<BufFn0>(v, 0x4a1ae0)(&e.f) * 0x10;
                        DWORD i = pick<BufFn0>(v, 0x4a1ab0)(&e.f);
                        return null_a ? r : r + i * 0x100;
                    });
                }
            }
            wsprintfA(what, "WriteDword/Byte/Qword null %d io %x", null_a, max_io);
            fake_case(t, what, s, [](int v, Env &e) {
                DWORD r = rel(e, (void *)pick<BufFn1>(v, 0x4a21c0)(&e.f, 0x12345678));
                r ^= rel(e, (void *)pick<BufFn1>(v, 0x4a21e0)(&e.f, 0x1ab)) * 3;
                r ^= rel(e, (void *)pick<BufFn2>(v, 0x4a2200)(&e.f, 0x9abcdef0, 0x13579bdf)) * 5;
                return r;
            });
        }
    // Reads, seeks and the rest of the stream calls
    for (int null_a = 0; null_a < 2; null_a++)
        for (DWORD size : {0u, 10u, 2100u, 5000u})
            for (DWORD max_io : {0xffffffffu, 7u}) {
                Setup s;
                s.null_a = null_a;
                s.size_a = size;
                s.max_io = max_io;
                for (DWORD n : {0u, 1u, 9u, 1999u, 2000u, 4000u, 6000u}) {
                    wsprintfA(what, "Read %u of %u null %d io %x", n, size, null_a, max_io);
                    fake_case(t, what, s, [n](int v, Env &e) {
                        BYTE *buf = static_cast<BYTE *>(heap_alloc(n + 1));
                        DWORD r = pick<BufFn2>(v, 0x4a1b00)(&e.f, addr(buf), n);
                        r = fnv(r, buf, r <= n ? r : 0);
                        heap_free(buf);
                        return r;
                    });
                }
                for (DWORD p : {(DWORD)0, (DWORD)1, size, size + 1, (DWORD)0xffffffff}) {
                    wsprintfA(what, "Seek/Skip/Tell/Size/SetSize/ReadLine %u of %u null %d", p, size, null_a);
                    fake_case(t, what, s, [p, null_a](int v, Env &e) {
                        DWORD r = pick<BufFn1>(v, 0x4a20e0)(&e.f, p) & 0xff;
                        r = r * 3 + (pick<BufFn1>(v, 0x4a20c0)(&e.f, p) & 0xff);
                        r = r * 3 + (pick<BufFn1>(v, 0x4a20c0)(&e.f, (DWORD)-1) & 0xff);
                        r = r * 7 + pick<BufFn0>(v, 0x4a2180)(&e.f);
                        r = r * 7 + pick<BufFn0>(v, 0x4a2190)(&e.f);
                        char line[16];
                        memset(line, 0x11, sizeof line);
                        r = r * 3 + (pick<BufFn2>(v, 0x4a20a0)(&e.f, addr(line), sizeof line) & 0xff);
                        r = fnv(r, line, sizeof line);
                        DWORD ss = pick<BufFn1>(v, 0x4a21a0)(&e.f, p / 2);
                        return null_a ? r : r * 7 + ss;
                    });
                }
                wsprintfA(what, "Advance/Detach/SetOption18 %u null %d", size, null_a);
                fake_case(t, what, s, [null_a](int v, Env &e) {
                    DWORD r = pick<BufFn1>(v, 0x4a2140)(&e.f, 0x1c5) & 0xff;
                    r = r * 7 + rel(e, (void *)pick<BufFn0>(v, 0x4a2170)(&e.f));
                    if (!null_a) {
                        r = r * 7 + rel(e, (void *)pick<BufFn1>(v, 0x4a2150)(&e.f, 3));
                        r = r * 7 + rel(e, (void *)pick<BufFn1>(v, 0x4a2150)(&e.f, 0));
                    }
                    return r;
                });
                wsprintfA(what, "ReadDword/Byte/Qword %u null %d io %x", size, null_a, max_io);
                fake_case(t, what, s, [](int v, Env &e) {
                    BYTE out[16];
                    memset(out, 0xee, sizeof out);
                    DWORD r = rel(e, (void *)pick<BufFn1>(v, 0x4a2270)(&e.f, addr(out)));
                    r ^= rel(e, (void *)pick<BufFn1>(v, 0x4a22a0)(&e.f, addr(out + 4))) * 3;
                    r ^= rel(e, (void *)pick<BufFn1>(v, 0x4a22d0)(&e.f, addr(out + 5))) * 5;
                    return fnv(r, out, sizeof out);
                });
            }
    // Counted texts and ReadString: the length in front, also longer than the rest
    for (DWORD length : {0u, 5u, 40u, 61u, 62u, 1000u, 0xffffffffu})
        for (int null_a = 0; null_a < 2; null_a++)
            for (int old = 0; old < 2; old++) {
                Setup s;
                s.null_a = null_a;
                wsprintfA(what, "ReadCountedText/ReadString length %u null %d old %d", length, null_a, old);
                fake_case(t, what, s, [length, old, null_a](int v, Env &e) {
                    memcpy(e.a.data, &length, 4);
                    char out[128];
                    memset(out, 0x33, sizeof out);
                    DWORD r = rel(e, (void *)pick<BufFn1>(v, 0x4a2300)(&e.f, addr(out)));
                    r = fnv(r, out, sizeof out);
                    r = fnv(r, at<int *(__cdecl *)()>(0x5d1014)(), 4);
                    if (length > 100)   // ReadString allocates length + 1 bytes
                        return r;
                    pick<BufFn1>(v, 0x4a20e0)(&e.f, 0);
                    char *p = old ? static_cast<char *>(heap_alloc(8)) : nullptr;
                    DWORD r2 = pick<BufFn1>(v, 0x4a2010)(&e.f, addr(&p)) & 0xff;
                    DWORD avail = e.a.size - 4 < length ? e.a.size - 4 : length;   // the rest of the block is not written
                    r = r * 3 + r2;
                    if (p)
                        r = fnv(fnv(r, p, avail), p + length, 1);
                    if (p)
                        heap_free(p);
                    (void)null_a;
                    return r;
                });
            }
    // Move inside the file (chunks of 64 KB, failures through short reads and writes)
    struct MoveCase {
        DWORD size, dst, src, n, max_io;
    };
    const MoveCase moves[] = {{100, 10, 10, 50, 0xffffffff}, {100, 10, 20, 0, 0xffffffff}, {100, 30, 20, 10, 0xffffffff},
                              {100, 0, 20, 80, 0xffffffff},  {100, 5, 50, 60, 0xffffffff}, {200000, 10, 70000, 130000, 0xffffffff},
                              {200000, 0, 65536, 65536, 0xffffffff}, {200000, 1, 2, 199998, 0xffffffff}, {100, 0, 20, 80, 30},
                              {100, 0, 20, 80, 0}, {3000, 0, 1000, 2000, 1999}};
    for (const MoveCase &m : moves)
        for (int null_a = 0; null_a < 2; null_a++) {
            Setup s;
            s.size_a = m.size;
            s.null_a = null_a;
            s.max_io = m.max_io;
            wsprintfA(what, "Move %u %u %u of %u io %x null %d", m.dst, m.src, m.n, m.size, m.max_io, null_a);
            fake_case(t, what, s, [m](int v, Env &e) { return pick<BufFn3>(v, 0x4a1b30)(&e.f, m.dst, m.src, m.n) & 0xff; });
        }
    // CopyTo with and without progress, into a stream and into a buffer without one
    for (DWORD size : {0u, 10u, 3000u, 600000u, 1100000u})
        for (int target = 0; target < 3; target++)
            for (int progress = 0; progress < 2; progress++) {
                Setup s;
                s.size_a = size;
                s.size_b = target == 2 ? 50 : 0;
                s.extra = size + 64;
                s.g_has_b = target > 0;
                s.null_a = size == 10 && target == 0;
                wsprintfA(what, "CopyTo %u target %d progress %d", size, target, progress);
                fake_case(t, what, s, [progress](int v, Env &e) {
                    return pick<BufFn2>(v, 0x4a1c40)(&e.f, addr(&e.g), progress ? addr((void *)&rec_progress) : 0) & 0xff;
                });
            }
    // Equals: equal contents, a different byte at chunk boundaries, different sizes, null streams
    struct EqualCase {
        DWORD size_a, size_b, differ, max_io;
        bool null_a, null_b;
    };
    const EqualCase equals[] = {{0, 0, 0xffffffff, 0xffffffff, 0, 0},     {100, 100, 0xffffffff, 0xffffffff, 0, 0},
                                {100, 100, 0, 0xffffffff, 0, 0},          {100, 100, 99, 0xffffffff, 0, 0},
                                {70000, 70000, 0xffffffff, 0xffffffff, 0, 0}, {140000, 140000, 65535, 0xffffffff, 0, 0},
                                {140000, 140000, 65536, 0xffffffff, 0, 0},    {140000, 140000, 139999, 0xffffffff, 0, 0},
                                {100, 101, 0xffffffff, 0xffffffff, 0, 0},     {100, 100, 0xffffffff, 40, 0, 0},
                                {0, 100, 0xffffffff, 0xffffffff, 1, 0},       {100, 0, 0xffffffff, 0xffffffff, 0, 1},
                                {0, 0, 0xffffffff, 0xffffffff, 1, 1}};
    for (const EqualCase &q : equals) {
        Setup s;
        s.size_a = q.size_a;
        s.size_b = q.size_b;
        s.seed_b = 17;
        s.differ = q.differ;
        s.max_io = q.max_io;
        s.null_a = q.null_a;
        s.null_b = q.null_b;
        s.g_has_b = true;
        wsprintfA(what, "Equals %u %u differ %x io %x null %d %d", q.size_a, q.size_b, q.differ, q.max_io, q.null_a, q.null_b);
        fake_case(t, what, s, [](int v, Env &e) { return pick<BufFn1>(v, 0x4a1d60)(&e.f, addr(&e.g)) & 0xff; });
    }
    // FileBuf_Open with "~CFileBuf 0x" and the address of a stream (bytes in memory order): the address,
    // null, only 6 digits, lower case with more behind
    for (int k = 0; k < 4; k++) {
        Setup s;
        wsprintfA(what, "Open ~CFileBuf %d", k);
        fake_case(t, what, s, [k](int v, Env &e) {
            char path[64];
            DWORD p = k == 1 ? 0 : addr(&e.b);
            const BYTE *b = reinterpret_cast<const BYTE *>(&p);
            const char *prefix = *at<const char **>(FILE_BUF_PREFIX);
            if (k == 2)
                wsprintfA(path, "%s%02X%02X%02X", prefix, b[0], b[1], b[2]);
            else if (k == 3)
                wsprintfA(path, "%s%02x%02x%02x%02xzz", prefix, b[0], b[1], b[2], b[3]);
            else
                wsprintfA(path, "%s%02X%02X%02X%02X", prefix, b[0], b[1], b[2], b[3]);
            return pick<BufFn2>(v, 0x4a1840)(&e.f, addr(path), 0x14) & 0xff;
        });
    }
}

// ---- Test files ------------------------------------------------------------------------------------

static char g_root[MAX_PATH];
static const DWORD SIZES[] = {0, 1, 1999, 2000, 2001, 65535, 65536, 65537, 600000};
constexpr int N_SIZES = sizeof SIZES / sizeof *SIZES;
static BYTE *g_expect[N_SIZES];
static BYTE *g_read[2];
constexpr DWORD READ_BUFFER = 700000;

static void file_name(char *out, const char *name) { wsprintfA(out, "%s%s", g_root, name); }
static void test_file(char *out, int k) { wsprintfA(out, "%sfile%u.bin", g_root, SIZES[k]); }
static void write_file(const char *path, const void *p, DWORD n)
{
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD w;
    WriteFile(f, p, n, &w, nullptr);
    CloseHandle(f);
}
// The whole file (at most max bytes) or 0xffffffff if it cannot be opened
DWORD read_file(const char *path, void *p, DWORD max, DWORD offset = 0)
{
    HANDLE f = CreateFileA(path, GENERIC_READ_, FILE_SHARE_READ | 2, nullptr, OPEN_EXISTING_, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return 0xffffffff;
    SetFilePointer(f, (long)offset, nullptr, 0);
    DWORD r = 0;
    ReadFile(f, p, max, &r, nullptr);
    CloseHandle(f);
    return r;
}
static unsigned file_hash(const char *path)
{
    DWORD n = read_file(path, g_read[0], READ_BUFFER);
    return n == 0xffffffff ? 0xdead : fnv(fnv(2166136261u, &n, 4), g_read[0], n);
}

static bool build_files()
{
    char tmp[MAX_PATH], p[MAX_PATH];
    if (!GetTempPathA(MAX_PATH - 40, tmp))
        return false;
    wsprintfA(g_root, "%sZaxFileBuffer\\", tmp);
    CreateDirectoryA(g_root, nullptr);
    for (int k = 0; k < N_SIZES; k++) {
        g_expect[k] = static_cast<BYTE *>(VirtualAlloc(nullptr, SIZES[k] + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        g_seed = 1000 + k;
        for (DWORD i = 0; i < SIZES[k]; i++)
            g_expect[k][i] = (BYTE)(random() >> 13);
        test_file(p, k);
        write_file(p, g_expect[k], SIZES[k]);
    }
    for (int v = 0; v < 2; v++)
        g_read[v] = static_cast<BYTE *>(VirtualAlloc(nullptr, READ_BUFFER, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    file_name(p, "u0.tmp");
    write_file(p, "u", 1);
    file_name(p, "u1.tmp");
    write_file(p, "u", 1);
    return true;
}
static const char *const SCRATCH[] = {"u0.tmp", "u1.tmp", "w0.bin", "w1.bin", "c0.bin", "c1.bin", "d0.bin", "d1.bin", "new0.bin", "new1.bin"};
static void remove_files()
{
    char p[MAX_PATH];
    for (int k = 0; k < N_SIZES; k++) {
        test_file(p, k);
        DeleteFileA(p);
    }
    for (const char *s : SCRATCH) {
        file_name(p, s);
        DeleteFileA(p);
    }
    for (int k = 0; k < N_SIZES; k++)
        VirtualFree(g_expect[k], 0, MEM_RELEASE_);
    for (int v = 0; v < 2; v++)
        VirtualFree(g_read[v], 0, MEM_RELEASE_);
    if (!RemoveDirectoryA(g_root))
        log("  could not remove %s", g_root);
}

// State of a real buffer: the fields, the stream's vtable and position, the counters
static unsigned real_hash(unsigned h, CFileBuf *f)
{
    DWORD v[9] = {f->vftable, f->stream ? f->stream->vftable : 0, f->stream ? (DWORD)f->stream->owned : 0x100, f->indent, f->flags,
                  f->small_reads, f->advanced, f->option18, files_open()};
    h = fnv(h, v, sizeof v);
    if (f->stream) {
        DWORD w[2] = {vcall<DWORD>(f->stream, STREAM_TELL), vcall<DWORD>(f->stream, STREAM_SIZE)};
        h = fnv(h, w, sizeof w);
    }
    return h;
}
static CFileBuf new_buf()
{
    CFileBuf f;
    original<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(&f);
    return f;
}
static void close_buf(CFileBuf *f) { original<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(f); }

// Reads a whole open buffer in chunks of n through the original (v = 0) or reimplemented Read, with
// a seek before every third chunk; returns the bytes read (into g_read[v]) and hashes the results
static DWORD read_all(int v, CFileBuf *f, DWORD n, unsigned *h)
{
    DWORD total = 0, k = 0;
    for (;;) {
        if (k % 3 == 2)
            *h = fnv(*h, "s", 1 + (pick<BufFn1>(v, 0x4a20e0)(f, total) & 0xff));
        DWORD want = total + n > READ_BUFFER ? READ_BUFFER - total : n;
        DWORD r = pick<BufFn2>(v, 0x4a1b00)(f, addr(g_read[v] + total), want);
        *h = fnv(*h, &r, 4);
        if (!r || r > want)
            break;
        total += r;
        k++;
    }
    DWORD tail[2] = {pick<BufFn0>(v, 0x4a2180)(f), pick<BufFn0>(v, 0x4a2190)(f)};
    *h = fnv(*h, tail, sizeof tail);
    return total;
}

static void test_files(Tally &t)
{
    char d[240], p[MAX_PATH], q[MAX_PATH];
    // FileBuf_Open on the test files with several flags; read in chunks around the boundaries
    const DWORD chunks[] = {1, 7, 1999, 2000, 4096, 65536, 65537, 700000};
    for (int k = 0; k < N_SIZES; k++) {
        test_file(p, k);
        for (DWORD flags : {4u, 0x24u, 0x204u, 0x84u}) {
            for (DWORD n : chunks) {
                if (SIZES[k] > 70000 && n < 1999)
                    continue;
                unsigned h[2];
                DWORD got[2];
                bool truth[2];
                for (int v = 0; v < 2; v++) {
                    CFileBuf f = new_buf();
                    files_open() = 3;
                    DWORD r = pick<BufFn2>(v, 0x4a1840)(&f, addr(p), flags) & 0xff;
                    h[v] = real_hash(fnv(2166136261u, &r, 4), &f);
                    got[v] = read_all(v, &f, n, &h[v]);
                    truth[v] = got[v] == SIZES[k] && memcmp(g_read[v], g_expect[k], SIZES[k]) == 0;
                    h[v] = real_hash(h[v], &f);
                    close_buf(&f);
                }
                wsprintfA(d, "[Open %u flags %x chunks %u: %u %u bytes, truth %d %d]", SIZES[k], flags, n, got[0], got[1], truth[0], truth[1]);
                t.check(h[0] == h[1] && truth[0] && truth[1], "file buffer", d);
            }
            // Seeks to the boundaries, skips, reads at the end
            unsigned h[2];
            for (int v = 0; v < 2; v++) {
                CFileBuf f = new_buf();
                DWORD r = pick<BufFn2>(v, 0x4a1840)(&f, addr(p), flags) & 0xff;
                h[v] = fnv(2166136261u, &r, 4);
                DWORD size = SIZES[k];
                const DWORD at_[] = {0, 1, 1999, 2000, 65535, 65536, size ? size - 1 : 0u, size, size + 1, size / 2};
                for (DWORD pos : at_) {
                    // The memory stream (0x200) seeks past its end and then reads beyond its block
                    bool memory = flags & 0x200;
                    if (memory && pos > size)
                        continue;
                    DWORD s = pick<BufFn1>(v, 0x4a20e0)(&f, pos) & 0xff;
                    BYTE buf[2100];
                    DWORD n2 = pick<BufFn2>(v, 0x4a1b00)(&f, addr(buf), sizeof buf);
                    bool ok = pos >= size ? n2 == 0 : n2 == (size - pos < sizeof buf ? size - pos : sizeof buf) &&
                                                           memcmp(buf, g_expect[k] + pos, n2) == 0;
                    DWORD x[4] = {s, n2, ok, pick<BufFn0>(v, 0x4a2180)(&f)};
                    h[v] = fnv(h[v], x, sizeof x);
                    DWORD sk = memory && pick<BufFn0>(v, 0x4a2180)(&f) < 10 ? 0x100 : pick<BufFn1>(v, 0x4a20c0)(&f, (DWORD)-10) & 0xff;
                    h[v] = fnv(fnv(h[v], &sk, 4), "t", 1 + pick<BufFn0>(v, 0x4a2180)(&f) % 7);
                }
                h[v] = real_hash(h[v], &f);
                close_buf(&f);
            }
            wsprintfA(d, "[seeks %u flags %x]", SIZES[k], flags);
            t.check(h[0] == h[1], "file buffer", d);
        }
        // File_ReadExact with the exact size, one less and one more; File_Exists and File_CanOpen
        for (int delta = -1; delta <= 1; delta++) {
            DWORD n = SIZES[k] + delta;
            if ((int)n < 0 || n > READ_BUFFER)
                continue;
            unsigned h[2];
            for (int v = 0; v < 2; v++) {
                memset(g_read[v], 0x5c, n + 1);
                files_open() = 3;
                DWORD r = pick<DWORD(__fastcall *)(const char *, void *, DWORD)>(v, 0x4a1300)(p, g_read[v], n) & 0xff;
                h[v] = fnv(fnv(fnv(2166136261u, &r, 4), g_read[v], n + 1), &files_open(), 4);
            }
            wsprintfA(d, "[ReadExact %u of %u]", n, SIZES[k]);
            t.check(h[0] == h[1] && memcmp(g_read[0], g_expect[k], delta ? 0 : n) == 0, "file buffer", d);
        }
        for (DWORD flags : {0u, 0x20u}) {
            compare_low(t, 0x4a1650, "File_Exists", (const char *)p, flags);
            files_open() = 3;
            compare_low(t, 0x4a1570, "File_CanOpen", (const char *)p, flags);
        }
    }
    // Missing files, empty paths, relative paths
    const char *missing[] = {"", "x", "\\ZaxNoSuchFolder\\x.bin", "/ZaxNoSuch.bin", "C:ZaxNo.bin", "Levels/ZaxNoSuchLevel.zax", "Rules.txt", "ZaxNo/"};
    for (const char *m : missing)
        for (DWORD flags : {4u, 0x24u, 0x204u, 0x44u}) {
            unsigned h[2];
            for (int v = 0; v < 2; v++) {
                CFileBuf f = new_buf();
                files_open() = 3;
                DWORD r = pick<BufFn2>(v, 0x4a1840)(&f, addr(m), flags) & 0xff;
                h[v] = real_hash(fnv(2166136261u, &r, 4), &f);
                close_buf(&f);
                DWORD e = pick<DWORD(__fastcall *)(const char *, DWORD)>(v, 0x4a1650)(m, flags & 0x20) & 0xff;
                files_open() = 3;
                DWORD c = pick<DWORD(__fastcall *)(const char *, DWORD)>(v, 0x4a1570)(m, flags & 0x20) & 0xff;
                h[v] = fnv(fnv(fnv(h[v], &e, 4), &c, 4), &files_open(), 4);
            }
            wsprintfA(d, "[missing %s flags %x]", m, flags);
            t.check(h[0] == h[1], "file buffer", d);
        }
    // File_WriteAll into new files, File_Copy with and without progress, File_Delete
    for (int k = 0; k < N_SIZES; k++) {
        unsigned h[2];
        for (int v = 0; v < 2; v++) {
            file_name(q, v ? "w1.bin" : "w0.bin");
            files_open() = 3;
            DWORD r = pick<DWORD(__fastcall *)(const char *, const void *, DWORD)>(v, 0x4a13f0)(q, g_expect[k], SIZES[k]) & 0xff;
            h[v] = fnv(fnv(file_hash(q), &r, 4), &files_open(), 4);
        }
        wsprintfA(d, "[WriteAll %u]", SIZES[k]);
        t.check(h[0] == h[1], "file buffer", d);
        test_file(p, k);
        for (int progress = 0; progress < 2; progress++) {
            for (int v = 0; v < 2; v++) {
                file_name(q, v ? "c1.bin" : "c0.bin");
                g_progress_hash = 2166136261u;
                g_progress_calls = 0;
                files_open() = 3;
                DWORD r = pick<DWORD(__fastcall *)(const char *, DWORD, const char *, DWORD, void(STDCALL *)(float))>(v, 0x4a1ea0)(
                              p, 0x20, q, 0x20, progress ? &rec_progress : nullptr) &
                          0xff;
                DWORD x[4] = {r, files_open(), g_progress_hash, g_progress_calls};
                h[v] = fnv(file_hash(q), x, sizeof x);
            }
            wsprintfA(d, "[Copy %u progress %d]", SIZES[k], progress);
            t.check(h[0] == h[1] && file_hash(q) == fnv(fnv(2166136261u, &SIZES[k], 4), g_expect[k], SIZES[k]), "file buffer", d);
        }
    }
    {   // File_Copy from a missing file, to a missing folder
        DWORD r[2][2];
        char bad[MAX_PATH];
        file_name(bad, "ZaxNoSuchFolder\\x.bin");
        test_file(p, 2);
        for (int v = 0; v < 2; v++) {
            file_name(q, v ? "c1.bin" : "c0.bin");
            files_open() = 3;
            r[v][0] = pick<DWORD(__fastcall *)(const char *, DWORD, const char *, DWORD, void(STDCALL *)(float))>(v, 0x4a1ea0)(
                          "\\ZaxNoSuch.bin", 0, q, 0x20, nullptr) & 0xff;
            r[v][1] = pick<DWORD(__fastcall *)(const char *, DWORD, const char *, DWORD, void(STDCALL *)(float))>(v, 0x4a1ea0)(
                          p, 0, bad, 0x20, nullptr) & 0xff;
            r[v][1] += files_open() * 0x100;
        }
        wsprintfA(d, "[Copy failures %x %x %x %x]", r[0][0], r[0][1], r[1][0], r[1][1]);
        t.check(memcmp(r[0], r[1], sizeof r[0]) == 0, "file buffer", d);
    }
    for (int v = 0; v < 2; v++) {
        file_name(q, v ? "d1.bin" : "d0.bin");
        write_file(q, "abc", 3);
        DWORD r1 = pick<DWORD(__fastcall *)(const char *, DWORD)>(v, 0x4a14e0)(q, 0x20);
        DWORD r2 = pick<DWORD(__fastcall *)(const char *, DWORD)>(v, 0x4a14e0)(q, 0x20);
        bool gone = GetFileAttributesA(q) == 0xffffffff;
        wsprintfA(d, "[Delete %d: %x %x %d]", v, r1, r2, gone);
        t.check(r1 == 1 && r2 == 0xffffff00 && gone, "file buffer", d);
    }
    // File_UniqueName: u0 and u1 exist, so u2; relative patterns below the data folder
    const char *patterns[] = {"u%d.tmp", "new%d.bin"};
    for (const char *pt : patterns) {
        char out[2][300];
        DWORD r[2];
        wsprintfA(p, "%s%s", g_root, pt);
        for (int v = 0; v < 2; v++)
            r[v] = pick<DWORD(__fastcall *)(char *, const char *, DWORD)>(v, 0x4a16e0)(out[v], p, 0x20);
        wsprintfA(d, "[UniqueName %s: %x %x %s]", pt, r[0], r[1], out[1]);
        t.check(r[0] == r[1] && lstrcmpA(out[0], out[1]) == 0, "file buffer", d);
    }
    const char *relative[] = {"ZaxNoSuch%d.bin", "Levels/ZaxNo%03d.zax", "%d"};
    for (const char *pt : relative) {
        char out[2][300];
        DWORD r[2];
        for (int v = 0; v < 2; v++)
            r[v] = pick<DWORD(__fastcall *)(char *, const char *, DWORD)>(v, 0x4a16e0)(out[v], pt, 0);
        wsprintfA(d, "[UniqueName %s: %x %x %s]", pt, r[0], r[1], out[1]);
        t.check(r[0] == r[1] && lstrcmpA(out[0], out[1]) == 0, "file buffer", d);
    }
}

// File_Delete without 0x20 and File_BackupData work below the data folder: the runtime's remove
// (0x5d17a6) and the backup to ".BAK" (0x4a4000) are redirected to recorders
static char g_rec_path[300];
static DWORD g_rec_arg, g_rec_result;
static int __cdecl rec_remove(const char *path)
{
    lstrcpyA(g_rec_path, path);
    return (int)g_rec_result;
}
static DWORD __fastcall rec_folders(const char *path, DWORD n)
{
    lstrcpyA(g_rec_path, path);
    g_rec_arg = n;
    return g_rec_result;
}
static void test_redirected(Tally &t)
{
    char d[240];
    const char *paths[] = {"x.bin", "Levels/Sub/x.zax", "", "Saves\\"};
    CodePatch remove, folders;
    patch_jump(remove, 0x5d17a6, (void *)&rec_remove);
    patch_jump(folders, 0x4a4000, (void *)&rec_folders);
    for (const char *p : paths)
        for (DWORD result : {0u, 0xffffffffu, 5u}) {
            unsigned h[2];
            for (int v = 0; v < 2; v++) {
                g_rec_result = result;
                g_rec_path[0] = 0;
                g_rec_arg = 0x99;
                DWORD r = pick<DWORD(__fastcall *)(const char *, DWORD)>(v, 0x4a14e0)(p, 0);
                h[v] = fnv(fnv_text(2166136261u, g_rec_path), &r, 4);
                g_rec_path[0] = 0;
                r = pick<DWORD(__fastcall *)(const char *)>(v, 0x4a1540)(p);
                DWORD x[2] = {r, g_rec_arg};
                h[v] = fnv(fnv_text(h[v], g_rec_path), x, sizeof x);
            }
            wsprintfA(d, "[Delete/BackupData %s %x]", p, result);
            t.check(h[0] == h[1], "file buffer", d);
        }
    unpatch(folders);
    unpatch(remove);
}

// ---- Real game files ---------------------------------------------------------------------------------

// The first loaded archive directory of the mounted archives
static ZZipDir *find_dir(ZZipNode *n, int depth)
{
    if (!n || depth > 16)
        return nullptr;
    auto *d = static_cast<ZZipDir *>(n->archive);
    if (d && d->loaded && d->count)
        return d;
    for (DWORD i = 0; i < n->children.count; i++)
        if (ZZipDir *c = find_dir(static_cast<ZZipNode *>(n->children.data[i]), depth + 1))
            return c;
    return nullptr;
}
ZZipDir *mounted_archive()
{
    if (!*at<BYTE *>(ZIP_LOADED))
        return nullptr;
    ZZipList *l = at<ZZipList *(__cdecl *)()>(0x5c1650)();
    for (int i = (int)l->count - 1; i >= 0; i--)
        if (ZZipDir *d = find_dir(static_cast<ZZipNode *>(l->data[i]), 0))
            return d;
    return nullptr;
}

// Entries to test: the smallest and largest, the ones closest to the boundaries, a few spread out
int pick_entries(ZZipDir *d, DWORD *out, int max)
{
    int n = 0;
    auto add = [&](DWORD i) {
        for (int k = 0; k < n; k++)
            if (out[k] == i)
                return;
        if (n < max)
            out[n++] = i;
    };
    DWORD small = 0, large = 0;
    for (DWORD i = 0; i < d->count; i++) {
        if (d->entries[i].size < d->entries[small].size)
            small = i;
        if (d->entries[i].size > d->entries[large].size && d->entries[i].size < 0x400000)
            large = i;
    }
    add(small);
    add(large);
    for (DWORD target : {1999u, 2000u, 65536u, 524288u}) {
        DWORD best = 0;
        for (DWORD i = 0; i < d->count; i++)
            if ((int)(d->entries[i].size - target) >= 0 && d->entries[i].size - target < d->entries[best].size - target)
                best = i;
        add(best);
    }
    for (DWORD k = 1; k < 7; k++)
        add(d->count * k / 7);
    return n;
}

// The bytes of an entry read from the archive file itself (data behind the local header)
DWORD zip_truth_bytes(ZZipDir *d, const ZZipEntry *e, BYTE *out, DWORD max)
{
    BYTE h[30];
    if (read_file(d->path, h, 30, e->offset) != 30)
        return 0xffffffff;
    DWORD name = h[26] | h[27] << 8, extra = h[28] | h[29] << 8;
    DWORD n = e->size < max ? e->size : max;
    return read_file(d->path, out, n, e->offset + 30 + name + extra);
}

static DWORD g_from_disk;
static void test_game_files(Tally &t)
{
    char d[300];
    ZZipDir *dir = mounted_archive();
    if (!dir) {
        log("  file buffer: no loaded archive mounted");
        return;
    }
    log("  file buffer: archive %s, %u entries, prefix \"%s\"", dir->path ? dir->path : "(null)", dir->count, dir->prefix ? dir->prefix : "");
    DWORD idx[16];
    int n = pick_entries(dir, idx, 16);
    for (int k = 0; k < n; k++) {
        const ZZipEntry &e = dir->entries[idx[k]];
        DWORD expect = zip_truth_bytes(dir, &e, g_read[0], READ_BUFFER);
        DWORD size = e.size < READ_BUFFER ? e.size : READ_BUFFER;
        BYTE *truth = static_cast<BYTE *>(VirtualAlloc(nullptr, size + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        memcpy(truth, g_read[0], size);
        for (DWORD flags : {4u, 0x204u, 0x14u})
            for (DWORD chunk : {2000u, 65536u, 700000u}) {
                unsigned h[2];
                DWORD got[2], vtable = 0;
                for (int v = 0; v < 2; v++) {
                    CFileBuf f = new_buf();
                    files_open() = 3;
                    DWORD r = pick<BufFn2>(v, 0x4a1840)(&f, addr(e.name), flags) & 0xff;
                    if (f.stream)
                        vtable = f.stream->vftable;
                    h[v] = real_hash(fnv(2166136261u, &r, 4), &f);
                    got[v] = read_all(v, &f, chunk, &h[v]);
                    close_buf(&f);
                    h[v] = fnv(h[v], &files_open(), 4);
                }
                bool same = got[0] == got[1] && memcmp(g_read[0], g_read[1], got[0]) == 0;
                // From the archive (or into memory) the bytes must be those of the archive file; a copy on disk may differ
                bool ok = vtable != 0x5f40dc ? got[0] == size && expect == size && memcmp(g_read[0], truth, size) == 0 : got[0] > 0;
                if (vtable == 0x5f40dc)
                    g_from_disk++;
                wsprintfA(d, "[game file %s (%u bytes) flags %x chunks %u: %u %u, truth %u, stream %x]", e.name, e.size, flags, chunk, got[0],
                          got[1], expect, vtable);
                t.check(h[0] == h[1] && same && ok, "file buffer", d);
            }
        VirtualFree(truth, 0, MEM_RELEASE_);
        for (DWORD flags : {0u, 0x20u})
            compare_low(t, 0x4a1650, "File_Exists (archive)", (const char *)e.name, flags);
        files_open() = 3;
        compare_low(t, 0x4a1570, "File_CanOpen (archive)", (const char *)e.name, 0u);
    }
}

ZAX_CHECKS(file_buffer)
{
    (void)cls;
    (void)n;
    Tally fake{"File buffer (recording streams)"}, files{"File buffer (test files)"}, game{"File buffer (game files)"};
    DWORD saved_open = files_open(), saved_max = files_open_max();
    init_fake_vtable();
    DWORD t0 = GetTickCount();
    test_fake(fake);
    files_open() = saved_open;
    files_open_max() = saved_max;
    DWORD t1 = GetTickCount();
    log("  file buffer fake checks: %u ms", t1 - t0);
    if (build_files()) {
        test_files(files);
        test_redirected(files);
        files_open() = saved_open;
        files_open_max() = saved_max;
        DWORD t2 = GetTickCount();
        log("  file buffer test file checks: %u ms", t2 - t1);
        test_game_files(game);
        files_open() = saved_open;
        files_open_max() = saved_max;
        log("  file buffer game file checks: %u ms (%u opens from a copy on disk)", GetTickCount() - t2, g_from_disk);
        remove_files();
    } else {
        log("  file buffer checks: no temp folder");
    }
    fake.report();
    files.report();
    game.report();
}

}  // namespace zax
