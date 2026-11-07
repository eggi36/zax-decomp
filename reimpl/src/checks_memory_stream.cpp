// Checks of the memory stream (memory_stream.h): random sequences of writes, reads, seeks, skips,
// Advance, Reserve and the line reader on streams built by the original and the reimplemented
// constructors, the line reader on texts with every CR/LF combination, Reserve on owned, borrowed and
// empty buffers, Detach, Reset, Close, the destructors, Equals and File_OpenInMemory on test files.
#include "check_util.h"
#include "init_list.h"
#include "memory_stream.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT DWORD WINAPI GetTickCount(void);

namespace zax {
namespace {

template <typename F> F pick(int v, DWORD address) { return v ? at<F>(address) : original<F>(address); }
DWORD addr(const void *p) { return (DWORD)(uintptr_t)p; }

using M0 = DWORD(THISCALL *)(MemoryStream *);
using M1 = DWORD(THISCALL *)(MemoryStream *, DWORD);
using M2 = DWORD(THISCALL *)(MemoryStream *, DWORD, DWORD);

BYTE g_pattern[4096];

// Everything of a stream except the addresses
struct MemState {
    DWORD vftable, handle, owned, position, size, capacity, owns, has_data, hash;
};
MemState mem_state(const MemoryStream *s)
{
    MemState st = {s->vftable, addr(s->handle), s->owned, s->position, s->size, s->capacity, s->owns, s->data != nullptr, 0};
    if (s->data)
        st.hash = fnv(2166136261u, s->data, s->size < s->capacity ? s->size : s->capacity);
    return st;
}
unsigned state_hash(unsigned h, const MemoryStream *s)
{
    MemState st = mem_state(s);
    return fnv(h, &st, sizeof st);
}

MemoryStream *make(int v)
{
    MemoryStream *s = static_cast<MemoryStream *>(pool_alloc(sizeof(MemoryStream)));
    pick<M0>(v, 0x505f00)(s);
    return s;
}
MemoryStream *make_data(int v, const void *p, DWORD n)
{
    BYTE *copy = static_cast<BYTE *>(heap_alloc(n + 1));
    memcpy(copy, p, n);
    copy[n] = 0;
    MemoryStream *s = static_cast<MemoryStream *>(pool_alloc(sizeof(MemoryStream)));
    pick<M2>(v, 0x505fb0)(s, addr(copy), n);
    return s;
}
void destroy(int v, MemoryStream *s) { pick<M1>(v, 0x505f50)(s, 1); }

// One random operation on the stream; returns the result as it can be compared
DWORD random_op(int v, MemoryStream *s, unsigned r1, unsigned r2, char *line)
{
    DWORD rest = s->position <= s->size ? s->size - s->position : 0;
    switch (r1 % 9) {
    case 0:
    case 1:
        return pick<M2>(v, 0x506040)(s, addr(g_pattern + r2 % 1000), r2 % 3000);
    case 2:
        return pick<M2>(v, 0x5060a0)(s, addr(line), r2 % 3000) ^ fnv(2166136261u, line, r2 % 3000 < rest ? r2 % 3000 : rest);
    case 3:
        return pick<M1>(v, 0x506100)(s, s->size ? r2 % (s->size + 1) : 0);
    case 4:
        return pick<M1>(v, 0x5060e0)(s, rest ? r2 % (rest + 1) : (DWORD)-(int)(r2 % (s->position + 1)));
    case 5: {
        BYTE *before = s->data;
        DWORD r = addr(pick<BYTE *(THISCALL *)(MemoryStream *, DWORD)>(v, 0x506210)(s, rest ? r2 % (rest + 1) : 0));
        return r - addr(before);
    }
    case 6:
        pick<M1>(v, 0x506250)(s, r2 % 9000);
        return 0;
    case 7: {
        memset(line, 0x55, 300);
        DWORD r = pick<M2>(v, 0x506140)(s, addr(line), 1 + r2 % 260);
        return ((r & 0xffffff00) == (addr(s) & 0xffffff00) ? 0x100 : 0x200) + (r & 0xff) + fnv(2166136261u, line, 300);
    }
    default:
        return pick<M1>(v, 0x4a2bf0)(s, r2 % 100) + 0x10000;   // slot 8 on the null handle
    }
}

void test_random(Tally &t)
{
    char d[200];
    char line[2][3100];
    for (unsigned seed = 1; seed <= 24; seed++) {
        unsigned h[2];
        for (int v = 0; v < 2; v++) {
            MemoryStream *s = seed % 3 == 0 ? make_data(v, g_pattern, seed * 37) : make(v);
            g_seed = seed * 7919;
            h[v] = state_hash(2166136261u, s);
            for (int k = 0; k < 300; k++) {
                unsigned r1 = random() >> 8, r2 = random() >> 8;
                DWORD r = random_op(v, s, r1, r2, line[v]);
                h[v] = state_hash(fnv(h[v], &r, 4), s);
            }
            destroy(v, s);
        }
        wsprintfA(d, "[memory stream random sequence %u: %x %x]", seed, h[0], h[1]);
        t.check(h[0] == h[1], "memory stream", d);
    }
}

const char *const TEXTS[] = {
    "one\r\ntwo\r\n\r\nthree", "a\nb\n\nc\n", "a\rb\r\rc", "a\n\rb\n\r", "a\r\r\nb\r\n\nc\n\r\nd\n\n\re", "", "no line end at all",
    "0123456789012345678901234567890123456789012345678901234567890123456789\r\nshort\r\n", "x\r", "\r\n\r\n", "\n", "\r",
};

void test_lines(Tally &t)
{
    char d[200], line[2][1100];
    int k = 0;
    for (const char *text : TEXTS) {
        DWORD length = (DWORD)lstrlenA(text);
        for (DWORD n : {1u, 2u, 3u, 4u, 8u, 64u, 1024u}) {
            unsigned h[2];
            for (int v = 0; v < 2; v++) {
                MemoryStream *s = make_data(1, text, length);
                h[v] = 2166136261u;
                for (int i = 0; i < 40; i++) {
                    memset(line[v], 0x77, sizeof line[v]);
                    DWORD r = pick<M2>(v, 0x506140)(s, addr(line[v]), n);
                    DWORD rel = ((r & 0xffffff00) == (addr(s) & 0xffffff00) ? 0x100 : 0x200) + (r & 0xff);
                    h[v] = state_hash(fnv(fnv(h[v], &rel, 4), line[v], n + 1), s);
                    if (!(r & 0xff))
                        break;
                }
                destroy(1, s);
            }
            wsprintfA(d, "[memory line reader text %d n %u: %x %x]", k, n, h[0], h[1]);
            t.check(h[0] == h[1], "memory stream", d);
        }
        k++;
    }
}

// Reserve, Reset, Detach, Close, the destructors and Equals on prepared states
void test_states(Tally &t)
{
    char d[200];
    // 0: empty (Ctor), 1: own data of 10 bytes, 2: borrowed 10 bytes (owns cleared), 3: own data, size 3 of 10
    for (int kind = 0; kind < 4; kind++)
        for (DWORD n : {0u, 5u, 10u, 11u, 14u, 15u, 16u, 100u}) {
            unsigned h[2];
            for (int v = 0; v < 2; v++) {
                MemoryStream *s = kind == 0 ? make(1) : make_data(1, g_pattern, 10);
                if (kind == 3)
                    s->size = 3;
                BYTE *borrowed = s->data;
                if (kind == 2)
                    s->owns = 0;
                pick<M1>(v, 0x506250)(s, n);
                h[v] = state_hash(fnv(2166136261u, "", s->data == borrowed), s);
                if (kind == 2 && s->data == borrowed) {
                    s->owns = 1;
                }
                destroy(1, s);
            }
            wsprintfA(d, "[Reserve kind %d n %u: %x %x]", kind, n, h[0], h[1]);
            t.check(h[0] == h[1], "memory stream", d);
        }
    for (DWORD n : {0u, 1u, 33u}) {
        unsigned h[2];
        for (int v = 0; v < 2; v++) {
            MemoryStream *s = make_data(1, g_pattern, 50);
            pick<M1>(v, 0x5061e0)(s, n);
            h[v] = state_hash(2166136261u, s);
            pick<M2>(v, 0x506040)(s, addr(g_pattern), 40);
            h[v] = state_hash(h[v], s);
            destroy(1, s);
        }
        wsprintfA(d, "[Reset %u: %x %x]", n, h[0], h[1]);
        t.check(h[0] == h[1], "memory stream", d);
    }
    unsigned h[2];
    for (int v = 0; v < 2; v++) {
        MemoryStream *s = make_data(1, g_pattern, 70);
        BYTE *before = s->data;
        BYTE *r = pick<BYTE *(THISCALL *)(MemoryStream *)>(v, 0x506230)(s);
        h[v] = state_hash(fnv(2166136261u, "", r == before), s);
        heap_free(r);
        // Detach without data
        r = pick<BYTE *(THISCALL *)(MemoryStream *)>(v, 0x506230)(s);
        h[v] = state_hash(fnv(h[v], &r, 4), s);
        destroy(1, s);
    }
    t.check(h[0] == h[1], "memory stream", "[Detach]");
    for (int kind = 0; kind < 3; kind++) {
        for (int v = 0; v < 2; v++) {
            MemoryStream *s = kind == 0 ? make(1) : make_data(1, g_pattern, 20);
            BYTE *borrowed = s->data;
            if (kind == 2)
                s->owns = 0;
            pick<M0>(v, 0x506110)(s);
            h[v] = state_hash(2166136261u, s);
            if (kind == 2)
                heap_free(borrowed);
            destroy(1, s);
        }
        wsprintfA(d, "[Close kind %d: %x %x]", kind, h[0], h[1]);
        t.check(h[0] == h[1], "memory stream", d);
    }
    for (int kind = 0; kind < 3; kind++)
        for (DWORD flags : {0u, 1u}) {
            DWORD r[2];
            for (int v = 0; v < 2; v++) {
                MemoryStream *s = kind == 0 ? make(1) : make_data(1, g_pattern, 20);
                BYTE *borrowed = s->data;
                if (kind == 2)
                    s->owns = 0;
                DWORD x = pick<M1>(v, 0x505f50)(s, flags);
                r[v] = (x == addr(s) ? 1 : 2);
                if (!flags) {
                    r[v] = state_hash(r[v], s);
                    pool_free(s, sizeof(MemoryStream));
                }
                if (kind == 2)
                    heap_free(borrowed);
            }
            wsprintfA(d, "[DeletingDtor kind %d flags %u: %x %x]", kind, flags, r[0], r[1]);
            t.check(r[0] == r[1], "memory stream", d);
        }
    for (int v = 0; v < 2; v++) {
        MemoryStream *s = make_data(1, g_pattern, 20);
        DWORD r = pick<M0>(v, 0x506000)(s);
        h[v] = state_hash(fnv(2166136261u, &r, 4), s);
        pool_free(s, sizeof(MemoryStream));
    }
    t.check(h[0] == h[1], "memory stream", "[Dtor]");
    // Constructors: the fields and the vtable (and the base part)
    for (int v = 0; v < 2; v++) {
        MemoryStream *a = make(v), *b = make_data(v, g_pattern, 33);
        h[v] = state_hash(state_hash(2166136261u, a), b);
        destroy(1, a);
        destroy(1, b);
    }
    t.check(h[0] == h[1], "memory stream", "[constructors]");
    // Equals: same and different bytes, different sizes, no data
    for (int kind = 0; kind < 5; kind++) {
        DWORD r[2];
        for (int v = 0; v < 2; v++) {
            MemoryStream *a = make_data(1, g_pattern, 100), *b = make_data(1, g_pattern, kind == 2 ? 99 : 100);
            if (kind == 1)
                b->data[50] ^= 1;
            if (kind == 3) {
                pick<M0>(1, 0x506110)(a);
                pick<M0>(1, 0x506110)(b);
            }
            if (kind == 4) {
                b->size = 100;
                pick<M0>(1, 0x506110)(b);
                b->size = 100;
            }
            r[v] = pick<M1>(v, 0x5062d0)(a, addr(b));
            if (kind == 4)
                r[v] = r[v] == (addr(a->data) & 0xffffff00) ? 0x4444 : r[v];
            b->size = 0;
            destroy(1, a);
            destroy(1, b);
        }
        wsprintfA(d, "[Equals kind %d: %x %x]", kind, r[0], r[1]);
        t.check(r[0] == r[1], "memory stream", d);
    }
}

// File_OpenInMemory on test files of several sizes
void test_files(Tally &t)
{
    char tmp[MAX_PATH], root[MAX_PATH], path[MAX_PATH], d[260];
    if (!GetTempPathA(MAX_PATH - 40, tmp))
        return;
    wsprintfA(root, "%sZaxMemoryStream\\", tmp);
    CreateDirectoryA(root, nullptr);
    DWORD files_open = *at<DWORD *>(FILES_OPEN);
    for (DWORD size : {0u, 1u, 2000u, 65536u, 300000u}) {
        BYTE *data = static_cast<BYTE *>(heap_alloc(size + 1));
        g_seed = size + 3;
        for (DWORD i = 0; i < size; i++)
            data[i] = (BYTE)(random() >> 11);
        wsprintfA(path, "%sm%u.bin", root, size);
        HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        DWORD w;
        WriteFile(f, data, size, &w, nullptr);
        CloseHandle(f);
        for (DWORD flags : {0x24u, 0x84u, 0xa4u, 0x20u}) {
            unsigned h[2];
            bool ok = true;
            for (int v = 0; v < 2; v++) {
                MemoryStream *s = static_cast<MemoryStream *>(pick<ZStream *(__fastcall *)(const char *, DWORD)>(v, 0x505e40)(path, flags));
                h[v] = s ? state_hash(2166136261u, s) : 0;
                if (s) {
                    if (s->size != size || memcmp(s->data, data, size) != 0)
                        ok = false;
                    destroy(1, s);
                } else if (flags & 4) {
                    ok = false;
                }
                h[v] = fnv(h[v], at<DWORD *>(FILES_OPEN), 4);
            }
            wsprintfA(d, "[File_OpenInMemory %u bytes flags %x: %x %x%s]", size, flags, h[0], h[1], ok ? "" : ", bytes differ");
            t.check(h[0] == h[1] && ok, "memory stream", d);
        }
        DeleteFileA(path);
        heap_free(data);
    }
    wsprintfA(path, "%smissing.bin", root);
    DWORD r[2];
    for (int v = 0; v < 2; v++)
        r[v] = addr(pick<ZStream *(__fastcall *)(const char *, DWORD)>(v, 0x505e40)(path, 0x24));
    t.check(r[0] == r[1] && r[0] == 0, "memory stream", "[File_OpenInMemory of a missing file]");
    *at<DWORD *>(FILES_OPEN) = files_open;
    if (!RemoveDirectoryA(root))
        log("  could not remove %s", root);
}

}  // namespace

ZAX_CHECKS(memory_stream)
{
    (void)cls;
    (void)n;
    Tally t{"Memory stream"};
    DWORD t0 = GetTickCount();
    g_seed = 99;
    for (BYTE &b : g_pattern)
        b = (BYTE)(random() >> 9);
    for (DWORD i = 0; i < sizeof g_pattern; i += 97)
        g_pattern[i] = i % 2 ? '\r' : '\n';
    test_random(t);
    test_lines(t);
    test_states(t);
    test_files(t);
    log("  memory stream checks: %u ms", GetTickCount() - t0);
    t.report();
}

}  // namespace zax
