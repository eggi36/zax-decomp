// Checks of the resource and temp file value slots (resource_fields.cpp) against the original, in the
// game thread. Each case runs twice (side 0 original, side 1 reimplementation) on objects of our own.
//
// Resource kinds: a descriptor of our own with a copy of the vtable 0x5fd970 whose slot 47 returns a
// fake resource list (names, Find, FindOrFail, Use, count; every call recorded in an event log, Find
// also records the text of g_LastFileOpened); an optional fake setter. Compared: the object, the log,
// the return value, the string or stream written, g_LastFileOpened afterwards.
// Temp file kind: a descriptor with a copy of the vtable 0x601c30. The temp files the slots create
// are compared by contents and deleted; the text and binary input comes from memory streams, the
// output goes into memory streams. Everything the checks write lies in a temp folder of their own or
// is a temp file of the slots themselves, and is removed again.
#include "check_util.h"
#include "init_list.h"
#include "memory_stream.h"
#include "resource_fields.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT DWORD WINAPI GetFileAttributesA(const char *path);
WINIMPORT DWORD WINAPI GetTickCount(void);
WINIMPORT int WINAPI lstrcmpiA(const char *a, const char *b);
WINIMPORT BOOL WINAPI ReadFile(HANDLE f, void *buf, DWORD n, DWORD *read, void *overlapped);

namespace zax {
namespace {

template <typename F> F pick(int v, DWORD address) { return v ? at<F>(address) : original<F>(address); }

int g_cur;
constexpr int LOG = 256;
DWORD g_log[2][LOG];
int g_nlog[2];
BYTE g_obj[2][0x40];

DWORD rel_obj(const void *p)
{
    DWORD v = (DWORD)(uintptr_t)p, base = (DWORD)(uintptr_t)g_obj[g_cur];
    return v - base < 0x40 ? 0x51000000 + (v - base) : v;
}
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    int &n = g_nlog[g_cur];
    if (n + 4 <= LOG) {
        DWORD *l = &g_log[g_cur][n];
        l[0] = a;
        l[1] = b;
        l[2] = c;
        l[3] = d;
    }
    n += 4;
}
DWORD result(DWORD low) { return ((0x5e700000u + (DWORD)g_nlog[g_cur] * 0x100u) & 0xffffff00) | (low & 0xff); }
bool same_log()
{
    if (g_nlog[0] != g_nlog[1])
        return false;
    for (int i = 0; i < g_nlog[0] && i < LOG; i++)
        if (g_log[0][i] != g_log[1][i])
            return false;
    return true;
}
void begin(int v)
{
    g_cur = v;
    g_nlog[v] = 0;
}
DWORD text_hash(const char *s) { return s ? fnv_text(2166136261u, s) : 0; }

// ---- Fake resource list --------------------------------------------------------------------------

const char *const NAMES[] = {"Alpha", "beta", "Gamma 7", "0", "12", "Delta", "eps"};
struct FakeList {
    DWORD vftable;
    DWORD count;
};
FakeList g_list;
int find_name(const char *s, DWORD count)
{
    for (DWORD i = 0; i < count; i++)
        if (crt_stricmp(NAMES[i], s) == 0)
            return (int)i;
    return -1;
}
const char *THISCALL l_name(FakeList *l, DWORD i)
{
    ev(0x401, i);
    return i < l->count ? NAMES[i] : "(none)";
}
DWORD THISCALL l_find(FakeList *l, const char *s, DWORD hint)
{
    ev(0x402, text_hash(s), hint, text_hash(at<ZString *>(LAST_FILE_OPENED)->c_str()));
    return (DWORD)find_name(s, l->count);
}
DWORD THISCALL l_find_or_fail(FakeList *l, const char *s, DWORD hint)
{
    ev(0x403, text_hash(s), hint);
    int i = find_name(s, l->count);
    return i < 0 ? 0x777 : (DWORD)i;
}
DWORD THISCALL l_use(FakeList *, DWORD i) { ev(0x40c, i); return result(0x4c); }
DWORD THISCALL l_count(FakeList *l) { ev(0x416); return l->count; }
DWORD THISCALL l_bad(FakeList *) { ev(0x4ff); return result(0x4f); }
void *g_list_vtable[30];

// ---- Fake descriptors ----------------------------------------------------------------------------

DWORD g_res_vtable[50], g_temp_vtable[50];
CResourceFieldSlots g_res_desc;
CTempFileFieldSlots g_temp_desc;
void *THISCALL d_list(CResourceFieldSlots *, BYTE *object)
{
    ev(0x447, rel_obj(object));
    return &g_list;
}
DWORD THISCALL fake_setter(BYTE *object, DWORD v)
{
    ev(0x430, rel_obj(object), v);
    return result(0x30);
}

void init_fakes()
{
    for (int i = 0; i < 30; i++)
        g_list_vtable[i] = (void *)&l_bad;
    g_list_vtable[1] = (void *)&l_name;
    g_list_vtable[2] = (void *)&l_find;
    g_list_vtable[3] = (void *)&l_find_or_fail;
    g_list_vtable[12] = (void *)&l_use;
    g_list_vtable[22] = (void *)&l_count;
    g_list.vftable = (DWORD)(uintptr_t)g_list_vtable;
    memcpy(g_res_vtable, at<void *>(0x5fd970), sizeof g_res_vtable);
    g_res_vtable[47] = (DWORD)(uintptr_t)&d_list;
    memcpy(g_temp_vtable, at<void *>(VT_TEMP_FILE), sizeof g_temp_vtable);
    memset(&g_res_desc, 0, sizeof g_res_desc);
    g_res_desc.vftable = (DWORD)(uintptr_t)g_res_vtable;
    g_res_desc.offset = 0x10;
    memset(&g_temp_desc, 0, sizeof g_temp_desc);
    g_temp_desc.vftable = (DWORD)(uintptr_t)g_temp_vtable;
    g_temp_desc.offset = 0x14;
}

void fill_objects(unsigned seed)
{
    g_seed = seed;
    for (DWORD j = 0; j < 0x40; j += 4) {
        DWORD r = random();
        memcpy(g_obj[0] + j, &r, 4);
    }
    memcpy(g_obj[1], g_obj[0], 0x40);
}

void check(Tally &t, bool same, const char *what, DWORD r0, DWORD r1)
{
    char d[200];
    wsprintfA(d, "[%.120s: %x / %x, log %d / %d]", what, r0, r1, g_nlog[0], g_nlog[1]);
    t.check(same, "slots", d);
}

// ---- Memory streams in file buffers ----------------------------------------------------------------

void fb_ctor(CFileBuf *f) { at<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(f); }
void fb_dtor(CFileBuf *f) { at<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(f); }
DWORD fb_tell(CFileBuf *f) { return at<DWORD(THISCALL *)(CFileBuf *)>(0x4a2180)(f); }
// A file buffer reading a copy of p (flags 4) or writing into an empty memory stream (flags 3)
void open_memory(CFileBuf *f, const void *p, DWORD n)
{
    fb_ctor(f);
    BYTE *copy = (BYTE *)heap_alloc(n ? n : 1);
    memcpy(copy, p, n);
    void *m = pool_alloc(sizeof(MemoryStream));
    at<MemoryStream *(THISCALL *)(void *, BYTE *, DWORD)>(0x505fb0)(m, copy, n);
    at<DWORD(THISCALL *)(CFileBuf *, ZStream *, DWORD)>(0x4a1970)(f, (ZStream *)m, 4);
}
void open_output(CFileBuf *f)
{
    fb_ctor(f);
    void *m = pool_alloc(sizeof(MemoryStream));
    at<MemoryStream *(THISCALL *)(void *)>(0x505f00)(m);
    at<DWORD(THISCALL *)(CFileBuf *, ZStream *, DWORD)>(0x4a1970)(f, (ZStream *)m, 3);
}
DWORD output_hash(CFileBuf *f)
{
    auto *m = (MemoryStream *)f->stream;
    if (!m)
        return 0;
    return fnv(fnv(2166136261u, &m->size, 4), m->data ? m->data : (const BYTE *)"", m->data ? m->size : 0);
}

// ---- Resource kinds ------------------------------------------------------------------------------

DWORD &value(int v) { return *(DWORD *)(g_obj[v] + 0x10); }

void test_by_name(Tally &t)
{
    const char *const TEXTS[] = {"Alpha", "ALPHA", "beta", "Gamma 7", "gamma 7", "0", "1", "3", "5", "12", "007",
                                 "-1", "x", "", "4294967295", " 2", "6"};
    unsigned seed = 1;
    for (DWORD fn : {0x54e490u, 0x54e3a0u})
        for (const char *text : TEXTS)
            for (BYTE numbers = 0; numbers < 2; numbers++)
                for (int setter = 0; setter < 2; setter++)
                    for (DWORD count = 0; count <= 7; count++) {
                        g_res_desc.f2d = numbers;
                        g_res_desc.f30 = setter ? (DWORD)(uintptr_t)&fake_setter : 0;
                        g_list.count = count;
                        fill_objects(seed++);
                        DWORD r[2];
                        for (int v = 0; v < 2; v++) {
                            begin(v);
                            r[v] = fn == 0x54e490 ? pick<DWORD(THISCALL *)(void *, BYTE *, const char *)>(v, fn)(&g_res_desc, g_obj[v], text)
                                                  : pick<DWORD(THISCALL *)(void *, BYTE *, const char *, CFileBuf *)>(v, fn)(&g_res_desc, g_obj[v], text, nullptr);
                        }
                        char what[100];
                        wsprintfA(what, "%s \"%s\" numbers %d setter %d count %u", fn == 0x54e490 ? "SetByName" : "FromText", text, numbers, setter,
                                  count);
                        check(t, r[0] == r[1] && same_log() && memcmp(g_obj[0], g_obj[1], 0x40) == 0, what, r[0], r[1]);
                    }
}

void test_default(Tally &t)
{
    const char *const DEFAULTS[] = {nullptr, "Alpha", "zzz", "2", "beta"};
    ZString *last = at<ZString *>(LAST_FILE_OPENED);
    unsigned seed = 5000;
    for (const char *def : DEFAULTS)
        for (BYTE numbers = 0; numbers < 2; numbers++)
            for (int setter = 0; setter < 2; setter++)
                for (DWORD count : {0u, 3u, 7u}) {
                    g_res_desc.f28 = (DWORD)(uintptr_t)def;
                    g_res_desc.f2d = numbers;
                    g_res_desc.f30 = setter ? (DWORD)(uintptr_t)&fake_setter : 0;
                    g_list.count = count;
                    fill_objects(seed++);
                    DWORD r[2];
                    StringState st[2];
                    for (int v = 0; v < 2; v++) {
                        begin(v);
                        ZStringBuffer *before = last->buffer;
                        r[v] = pick<DWORD(THISCALL *)(void *, BYTE *)>(v, 0x54e2c0)(&g_res_desc, g_obj[v]);
                        st[v] = state(*last, before);
                    }
                    char what[100];
                    wsprintfA(what, "SetDefault \"%s\" numbers %d setter %d count %u", def ? def : "(null)", numbers, setter, count);
                    // With a default the result is the ZString destructor's eax (buffer addresses)
                    check(t, (def || r[0] == r[1]) && same_log() && memcmp(g_obj[0], g_obj[1], 0x40) == 0 && memcmp(&st[0], &st[1], sizeof st[0]) == 0,
                          what, r[0], r[1]);
                }
    g_res_desc.f28 = 0;
    // The static text not yet built: guard cleared, atexit redirected, the text destroyed afterwards
    SavedBytes saved_text, saved_guard;
    save_bytes(saved_text, RESOURCE_DEFAULT_TEXT, 4);
    save_bytes(saved_guard, RESOURCE_DEFAULT_GUARD, 1);
    CodePatch exit_patch;
    patch_jump(exit_patch, 0x5d0863, (void *)&fake_atexit);
    g_res_desc.f28 = (DWORD)(uintptr_t) "beta";
    g_list.count = 7;
    StringState st[2];
    DWORD fn[2], calls[2], guard[2];
    for (int v = 0; v < 2; v++) {
        begin(v);
        *at<BYTE *>(RESOURCE_DEFAULT_GUARD) &= ~1;
        g_atexit_calls = g_atexit_function = 0;
        pick<DWORD(THISCALL *)(void *, BYTE *)>(v, 0x54e2c0)(&g_res_desc, g_obj[v]);
        st[v] = state(*at<ZString *>(RESOURCE_DEFAULT_TEXT), nullptr);
        fn[v] = g_atexit_function;
        calls[v] = g_atexit_calls;
        guard[v] = *at<BYTE *>(RESOURCE_DEFAULT_GUARD);
        at<void(THISCALL *)(ZString *)>(0x4defd0)(at<ZString *>(RESOURCE_DEFAULT_TEXT));
    }
    unpatch(exit_patch);
    restore_bytes(saved_text);
    restore_bytes(saved_guard);
    g_res_desc.f28 = 0;
    check(t, memcmp(&st[0], &st[1], sizeof st[0]) == 0 && fn[0] == fn[1] && fn[0] == RESOURCE_DEFAULT_EXIT && calls[0] == calls[1] &&
                 guard[0] == guard[1] && same_log(),
          "SetDefault, static text not yet built", fn[0], fn[1]);
}

void test_resource_io(Tally &t)
{
    g_res_desc.f30 = 0;
    g_list.count = 7;
    for (DWORD i = 0; i < 9; i++) {
        // ToText (the result is the eax of ZString::SetText, not compared)
        StringState st[2];
        for (int v = 0; v < 2; v++) {
            begin(v);
            fill_objects(77);
            value(v) = i;
            ZString z;
            at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&z);
            pick<DWORD(THISCALL *)(void *, BYTE *, ZString *)>(v, 0x54e370)(&g_res_desc, g_obj[v], &z);
            st[v] = state(z, nullptr);
            at<void(THISCALL *)(ZString *)>(0x4defd0)(&z);
        }
        char what[60];
        wsprintfA(what, "ToText %u", i);
        check(t, memcmp(&st[0], &st[1], sizeof st[0]) == 0 && same_log(), what, 0, 0);
        // WriteBinary
        DWORD r[2], h[2];
        for (int v = 0; v < 2; v++) {
            begin(v);
            value(v) = i;
            CFileBuf f;
            open_output(&f);
            r[v] = pick<DWORD(THISCALL *)(void *, BYTE *, CFileBuf *)>(v, 0x54e3c0)(&g_res_desc, g_obj[v], &f) - (DWORD)(uintptr_t)&f;
            h[v] = output_hash(&f);
            fb_dtor(&f);
        }
        wsprintfA(what, "WriteBinary %u", i);
        check(t, r[0] == r[1] && r[0] == 0 && h[0] == h[1] && same_log(), what, h[0], h[1]);
        // Use
        for (int v = 0; v < 2; v++) {
            begin(v);
            value(v) = i;
            r[v] = pick<DWORD(THISCALL *)(void *, BYTE *)>(v, 0x54e460)(&g_res_desc, g_obj[v]);
        }
        wsprintfA(what, "Use %u", i);
        check(t, r[0] == r[1] && same_log(), what, r[0], r[1]);
    }
    // ReadBinary: counted texts, an unknown name, a length beyond the rest, an empty stream
    const char *const TEXTS[] = {"Alpha", "beta", "GAMMA 7", "0", "12", "eps", "nope", "", "Delta"};
    for (int k = 0; k < 12; k++) {
        BYTE in[64];
        DWORD n;
        if (k < 9) {
            DWORD len = (DWORD)lstrlenA(TEXTS[k]);
            memcpy(in, &len, 4);
            memcpy(in + 4, TEXTS[k], len);
            n = 4 + len;
        } else if (k == 9) {
            DWORD len = 40;
            memcpy(in, &len, 4);
            memcpy(in + 4, "short", 5);
            n = 9;
        } else if (k == 10) {
            n = 0;
        } else {
            DWORD len = 3;
            memcpy(in, &len, 4);
            memcpy(in + 4, "betaXYZ", 7);
            n = 11;
        }
        DWORD r[2], pos[2];
        fill_objects(99);
        for (int v = 0; v < 2; v++) {
            begin(v);
            CFileBuf f;
            open_memory(&f, in, n);
            *at<int *(__cdecl *)()>(0x5d1014)() = 0x16;
            r[v] = pick<DWORD(THISCALL *)(void *, BYTE *, CFileBuf *)>(v, 0x54e3f0)(&g_res_desc, g_obj[v], &f);
            pos[v] = fb_tell(&f);
            fb_dtor(&f);
        }
        char what[60];
        wsprintfA(what, "ReadBinary case %d", k);
        check(t, r[0] == r[1] && pos[0] == pos[1] && same_log() && memcmp(g_obj[0], g_obj[1], 0x40) == 0, what, r[0], r[1]);
    }
}

// ---- Temp file kind ------------------------------------------------------------------------------

char g_dir[MAX_PATH];

void write_file(const char *path, const BYTE *p, DWORD n)
{
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD w;
    if (n)
        WriteFile(f, p, n, &w, nullptr);
    CloseHandle(f);
}
// Size and hash of a file, or 0xffffffff when it cannot be read
DWORD file_hash(const char *path)
{
    HANDLE f = CreateFileA(path, 0x80000000, 0, nullptr, 3 /* OPEN_EXISTING */, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return 0xffffffff;
    unsigned h = 2166136261u;
    BYTE buf[4096];
    DWORD r;
    while (ReadFile(f, buf, sizeof buf, &r, nullptr) && r)
        h = fnv(h, buf, r);
    CloseHandle(f);
    return h;
}
bool exists(const char *path) { return GetFileAttributesA(path) != 0xffffffff; }
char *&path_value(int v) { return *(char **)(g_obj[v] + 0x14); }
// The value as a path buffer of 0xff bytes from operator new (as the slots make them)
char *path_buffer(const char *path)
{
    char *p = (char *)heap_alloc(0xff);
    lstrcpyA(p, path);
    return p;
}
// After a case: the temp file of the value deleted and its buffer freed
void drop_value(int v)
{
    if (char *p = path_value(v)) {
        DeleteFileA(p);
        heap_free(p);
        path_value(v) = nullptr;
    }
}

BYTE g_data[80000];

void test_temp_simple(Tally &t)
{
    // ToTextEmpty (slot 16, shared with the embedded kinds)
    StringState st[2];
    for (int v = 0; v < 2; v++) {
        begin(v);
        ZString z;
        at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&z);
        at<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(&z, "old text");
        pick<DWORD(THISCALL *)(void *, BYTE *, ZString *)>(v, 0x54e790)(&g_temp_desc, g_obj[v], &z);
        st[v] = state(z, nullptr);
        at<void(THISCALL *)(ZString *)>(0x4defd0)(&z);
    }
    check(t, memcmp(&st[0], &st[1], sizeof st[0]) == 0, "ToTextEmpty", 0, 0);
    // SetDefault: no file, a file, a path whose file is missing
    for (int k = 0; k < 3; k++) {
        DWORD r[2];
        bool gone[2];
        char path[2][MAX_PATH];
        fill_objects(3);
        for (int v = 0; v < 2; v++) {
            begin(v);
            wsprintfA(path[v], "%sdefault%d.tmp", g_dir, v);
            if (k == 1)
                write_file(path[v], g_data, 300);
            path_value(v) = k ? path_buffer(path[v]) : nullptr;
            r[v] = pick<DWORD(THISCALL *)(void *, BYTE *)>(v, 0x58b8b0)(&g_temp_desc, g_obj[v]);
            gone[v] = !exists(path[v]);
            DeleteFileA(path[v]);
        }
        char what[60];
        wsprintfA(what, "temp SetDefault case %d", k);
        // With a path the result is operator delete's eax
        check(t, (k || r[0] == r[1]) && gone[0] && gone[1] && memcmp(g_obj[0], g_obj[1], 0x40) == 0, what, r[0], r[1]);
    }
    // WriteBinary: no file, files of several sizes, a missing file
    for (DWORD n : {0xffffffffu, 0u, 5u, 3000u, 70000u, 0xfffffffeu}) {
        DWORD r[2], h[2];
        char path[2][MAX_PATH];
        for (int v = 0; v < 2; v++) {
            begin(v);
            fill_objects(4);
            wsprintfA(path[v], "%swrite%d.tmp", g_dir, v);
            if (n < 0xfffffffe)
                write_file(path[v], g_data, n);
            path_value(v) = n != 0xffffffff ? path_buffer(path[v]) : nullptr;
            CFileBuf f;
            open_output(&f);
            r[v] = pick<DWORD(THISCALL *)(void *, BYTE *, CFileBuf *)>(v, 0x58be70)(&g_temp_desc, g_obj[v], &f);
            h[v] = output_hash(&f);
            fb_dtor(&f);
            drop_value(v);
        }
        char what[60];
        wsprintfA(what, "temp WriteBinary %d", (int)n);
        check(t, r[0] == r[1] && h[0] == h[1], what, h[0], h[1]);
    }
}

// Input for ReadBinary: the size dword and the data (packed: starts with 0x3e9)
DWORD binary_input(BYTE *out, DWORD n, bool packed)
{
    memcpy(out, &n, 4);
    memcpy(out + 4, g_data, n);
    if (packed && n >= 4) {
        DWORD m = TEMP_FILE_PACKED;
        memcpy(out + 4, &m, 4);
    }
    return n + 4;
}

BYTE g_in[90000];

// One case of a temp file slot that reads (ReadBinary or FromText): old value none / an existing file
void compare_read(Tally &t, const char *what, DWORD fn, const BYTE *in, DWORD n, bool old_file)
{
    DWORD r[2], pos[2], h[2];
    bool old_gone[2], has[2];
    for (int v = 0; v < 2; v++) {
        begin(v);
        fill_objects(5);
        char old[MAX_PATH];
        wsprintfA(old, "%sold%d.tmp", g_dir, v);
        char *old_buffer = nullptr;
        if (old_file) {
            write_file(old, g_data, 10);
            old_buffer = path_buffer(old);
        }
        path_value(v) = old_buffer;
        CFileBuf f;
        open_memory(&f, in, n);
        if (fn == 0x58bf20)
            r[v] = pick<DWORD(THISCALL *)(void *, BYTE *, CFileBuf *)>(v, fn)(&g_temp_desc, g_obj[v], &f);
        else
            r[v] = pick<DWORD(THISCALL *)(void *, BYTE *, const char *, CFileBuf *)>(v, fn)(&g_temp_desc, g_obj[v], "TempFile", &f);
        pos[v] = fb_tell(&f);
        fb_dtor(&f);
        char *p = path_value(v);
        has[v] = p != nullptr;
        // The old file is removed when the buffer gets a new temp name (the buffer is reused)
        old_gone[v] = !exists(old);
        h[v] = 0;
        if (p) {
            h[v] = lstrcmpiA(p, old) == 0 ? 1 : file_hash(p);
            if (lstrcmpiA(p, old) != 0)
                DeleteFileA(p);
            heap_free(p);
        }
        if (old_buffer && old_buffer != p)   // left alone (ReadBinary with -1) or freed (too long text)
            if (fn == 0x58bf20)
                heap_free(old_buffer);
        path_value(v) = nullptr;
        DeleteFileA(old);
    }
    char d[140];
    wsprintfA(d, "%s (old file %d): pos %u/%u", what, old_file, pos[0], pos[1]);
    check(t, r[0] == r[1] && pos[0] == pos[1] && h[0] == h[1] && has[0] == has[1] && old_gone[0] == old_gone[1] && same_log(), d, h[0],
          h[1]);
}

void test_temp_read(Tally &t)
{
    for (int old = 0; old < 2; old++) {
        // ReadBinary: none, raw data of several sizes, packed data
        DWORD minus = 0xffffffff;
        memcpy(g_in, &minus, 4);
        compare_read(t, "temp ReadBinary -1", 0x58bf20, g_in, 4, old);
        for (DWORD n : {4u, 5u, 100u, 3000u, 70000u})
            for (int packed = 0; packed < 2; packed++) {
                DWORD m = binary_input(g_in, n, packed);
                char what[60];
                wsprintfA(what, "temp ReadBinary %u packed %d", n, packed);
                compare_read(t, what, 0x58bf20, g_in, m, old);
            }
        // FromText: the block as slot 23 writes it, with variations
        for (int k = 0; k < 11; k++) {
            DWORD n = k < 4 ? 4 + (DWORD)k * 997 : 300;
            char head[200];
            const char *before = k == 5 ? "\r\nNo equals here\r\n\r\n" : "";
            DWORD length = k == 6 ? 100000 : k == 7 ? 60 : n;
            wsprintfA(head, "{\r\n%s\tLength=%u bytes\r\n#### BEGIN TEMPORARY FILE ####\r\n", before, length);
            DWORD m = (DWORD)lstrlenA(head);
            memcpy(g_in, head, m);
            if (k == 4) {   // a block without data: '}' before any Length line
                m = (DWORD)lstrlenA("{\r\n}\r\nNext=1\r\n");
                memcpy(g_in, "{\r\n}\r\nNext=1\r\n", m);
            } else {
                memcpy(g_in + m, g_data, n);
                if (k == 3) {
                    DWORD p = TEMP_FILE_PACKED;
                    memcpy(g_in + m, &p, 4);
                }
                m += n;
                const char *tail = k == 8 ? "#### END TEMP\r\n}\r\n" : k == 9 ? "#### END TEMPORARY FILE ####\r\n\tExtra=2\r\n\r\n}\r\nNext=1\r\n"
                                                                                : "#### END TEMPORARY FILE ####\r\n}\r\nNext=1\r\n";
                if (k == 10)
                    tail = "";
                memcpy(g_in + m, tail, (DWORD)lstrlenA(tail));
                m += (DWORD)lstrlenA(tail);
            }
            char what[60];
            wsprintfA(what, "temp FromText case %d", k);
            compare_read(t, what, 0x58b8f0, g_in, m, old);
        }
    }
}

}  // namespace

ZAX_CHECKS(resource_fields)
{
    (void)cls;
    (void)n;
    Tally t{"Resource and temp file slots"};
    DWORD t0 = GetTickCount();
    init_fakes();
    g_seed = 31;
    for (DWORD i = 0; i < sizeof g_data; i += 4) {
        DWORD r = random();
        memcpy(g_data + i, &r, 4);
    }
    g_data[0] = 0x11;   // never the packed marker 0x3e9
    test_by_name(t);
    test_default(t);
    test_resource_io(t);
    char tmp[MAX_PATH];
    if (GetTempPathA(MAX_PATH - 40, tmp)) {
        wsprintfA(g_dir, "%sZaxTempSlots\\", tmp);
        CreateDirectoryA(g_dir, nullptr);
        test_temp_simple(t);
        test_temp_read(t);
        if (!RemoveDirectoryA(g_dir))
            log("  could not remove %s", g_dir);
    }
    log("  resource field checks: %u ms", GetTickCount() - t0);
    t.report();
}

}  // namespace zax
