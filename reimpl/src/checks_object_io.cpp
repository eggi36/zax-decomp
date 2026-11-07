// Checks of the object reader and the binary object functions, the object reference slots 19/26/29,
// the temp file slot 23 and the white space skipper (object_io.cpp) against the original, in the game
// thread (see checks_object_common.h for how a case runs).
//
// Object_ReadText reads synthetic texts into fake objects whose classes have fake descriptors (slot 19
// records the value) and whose slot 11 records the own lines; the unexpected '{' runs with the error
// box and exit redirected (escape back into the check). The real data: the CObjects the game world
// points to (as in checks_field_streams.cpp) written as text and binary with modes 1 and 0 and the
// finish, and the text read back into a mirror object (a fake class with the real class's field
// names; reference fields read with the real slot 19 into a scratch object, the blocks of the other
// fields skipped line by line).
#include "checks_object_common.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT DWORD WINAPI GetTickCount(void);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);

extern "C" int __cdecl zax_call_escapable1(void *fn, void *self, void *arg);
__asm__(".globl _zax_call_escapable1\n"
        "_zax_call_escapable1:\n"
        "  pushl %ebp\n  pushl %ebx\n  pushl %esi\n  pushl %edi\n"
        "  movl %esp, _zax_escape_esp\n"
        "  pushl 28(%esp)\n"         // arg (return address at 16, fn 20, self 24, arg 28)
        "  movl 28(%esp), %ecx\n"    // self
        "  call *24(%esp)\n"         // thiscall, removes its argument
        "  popl %edi\n  popl %esi\n  popl %ebx\n  popl %ebp\n  ret\n");

namespace zax {

namespace {
using namespace objcheck;

// ---- Fakes -------------------------------------------------------------------------------------------

constexpr int NDESC = 7, NOBJ = 6;
FakeDesc g_descs[NDESC];
CFieldDesc *g_fields_a[NDESC], *g_fields_b[2];
CClassInfo g_cls[3];        // A: five fields, parent a real class; B: two (one with an alt name); C: none
FakeObj g_objs[NOBJ];
CClassInfo *g_real[3];      // a real class (A's and C's parent), another one, not related
const char *const DESC_NAMES[NDESC] = {"Alpha", "Beta Gamma", "Count", "", "Object ID", "Alpha", "Gamma"};
const char *const DESC_ALT[NDESC] = {"", "", "", "", "", "", "OldGamma"};

DWORD g_hook_calls;
void __cdecl fake_read_hook()
{
    g_hook_calls++;
    ev(0x400, g_hook_calls);
}

void init_fakes(CClassInfo **cls, int n)
{
    init_fake_vtables();
    for (int i = 0; i < NDESC; i++)
        init_fake_desc(g_descs[i], DESC_NAMES[i], DESC_ALT[i], 100 + i);
    for (int i = 0; i < 5; i++)
        g_fields_a[i] = &g_descs[i];
    g_fields_b[0] = &g_descs[5];
    g_fields_b[1] = &g_descs[6];
    g_real[0] = g_real[1] = g_real[2] = nullptr;
    for (int i = 0; i < n && !g_real[1]; i++)
        if (cls[i]->parent && cls[i]->parent->parent) {
            g_real[0] = cls[i];
            g_real[1] = cls[i]->parent;
        }
    for (int i = 0; i < n && !g_real[2]; i++)
        if (g_real[0] && !(at<DWORD(THISCALL *)(CClassInfo *, CClassInfo *)>(0x4155f0)(g_real[0], cls[i]) & 0xff))
            g_real[2] = cls[i];
    const char *names[3] = {"CFakeReadA", "CFakeReadB", "CFakeReadC"};
    const float versions[3] = {1.0f, 2.5f, 7.0f};
    for (int c = 0; c < 3; c++) {
        memset(&g_cls[c], 0, sizeof(CClassInfo));
        g_cls[c].name = names[c];
        g_cls[c].version = versions[c];
    }
    g_cls[0].fields = g_fields_a;
    g_cls[0].num_fields = 5;
    g_cls[0].parent = g_real[0];
    g_cls[1].fields = g_fields_b;
    g_cls[1].num_fields = 2;
    g_cls[2].parent = g_real[0];
    for (int i = 0; i < NOBJ; i++) {
        memset(&g_objs[i], 0, sizeof(FakeObj));
        g_objs[i].vftable = ptrv(g_fobj_vt);
        g_objs[i].id = i + 1;
        g_objs[i].cls = &g_cls[i % 3];
        g_objs[i].read_result = 1;
    }
}
void free_fakes()
{
    for (int i = 0; i < NDESC; i++)
        free_fake_desc(g_descs[i]);
}

// The globals the reader uses, saved for the checks and set the same before each side
void *g_saved_hook, *g_saved_stop_object, *g_saved_stop_field;
DWORD g_saved_find_cache;
void *g_stop_object;
const char *g_stop_field;
bool g_use_hook;
void reset()
{
    *at<void **>(READ_HOOK) = g_use_hook ? (void *)&fake_read_hook : nullptr;
    *at<void **>(READ_STOP_OBJECT) = g_stop_object;
    *at<const char **>(READ_STOP_FIELD) = g_stop_field;
    *at<DWORD *>(FIND_FIELD_CACHE) = 0;
    g_hook_calls = 0;
}
void save_globals()
{
    g_saved_hook = *at<void **>(READ_HOOK);
    g_saved_stop_object = *at<void **>(READ_STOP_OBJECT);
    g_saved_stop_field = *at<void **>(READ_STOP_FIELD);
    g_saved_find_cache = *at<DWORD *>(FIND_FIELD_CACHE);
}
void restore_globals()
{
    *at<void **>(READ_HOOK) = g_saved_hook;
    *at<void **>(READ_STOP_OBJECT) = g_saved_stop_object;
    *at<void **>(READ_STOP_FIELD) = g_saved_stop_field;
    *at<DWORD *>(FIND_FIELD_CACHE) = g_saved_find_cache;
}

// ---- The fatal error, redirected ----------------------------------------------------------------------

CodePatch g_box_patch, g_exit_patch;
DWORD g_box_hash;
void __fastcall record_box(const char *caption, const char *text)
{
    capture_box(caption, text);
    g_box_hash = fnv_text(text_hash(caption), text);
}
// Calls a thiscall function with one argument; an exit inside comes back as 0x7fff0001
DWORD call_escapable(DWORD address, void *self, void *arg)
{
    g_box_hash = 0;
    patch_jump(g_box_patch, 0x4d24c0, (void *)&record_box);
    patch_jump(g_exit_patch, 0x5d0e36, (void *)&zax_escape_exit);
    const DWORD r = (DWORD)zax_call_escapable1(at<void *>(address), self, arg);
    unpatch(g_exit_patch);
    unpatch(g_box_patch);
    ev(0xbad, g_box_hash, r == 0x7fff0001 ? zax_exit_code : 0);
    return r;
}

// ---- Object_ReadText with fakes ------------------------------------------------------------------------

Bytes g_input;
int g_obj;
DWORD g_after_read;   // 1: FinishTable after reading
void after_read(CObjectWriter *, State &s)
{
    s.extra[0] = *at<DWORD *>(FIND_FIELD_CACHE);
    s.extra[1] = g_hook_calls;
}
// The memory stream's ReadLine leaves its own address in the upper bytes (the reader passes them on)
DWORD stream_relative(CObjectWriter *w, DWORD r)
{
    return w->stream && (r & ~0xffu) == (ptrv(w->stream) & ~0xffu) ? 0x57000000 | (r & 0xff) : r;
}
DWORD call_read_text(DWORD a, CObjectWriter *w)
{
    DWORD r = stream_relative(w, call_escapable(a, &g_objs[g_obj], w));
    if (g_after_read)
        r ^= (at<DWORD(THISCALL *)(CObjectWriter *)>(0x524320)(w) & 0xff) << 16;
    return r;
}

void add_text(const char *s) { append_text(g_input, s); }

// Builds text variant v into g_input; returns the object to read into
int build_text(int v, DWORD &position)
{
    char line[400];
    g_input.size = 0;
    position = 0;
    switch (v) {
    case 0: add_text("{\r\n\tAlpha=1\r\n\tBeta Gamma=two words\r\n}\r\n"); return 0;
    case 1:
        wsprintfA(line, "%s\r\n{\r\n\tAlpha=1\r\n}\r\n", g_real[1] ? g_real[1]->name : "CObject");
        add_text(line);
        return 0;
    case 2:
        wsprintfA(line, "%s\r\n{\r\n\tAlpha=1\r\n}\r\n", g_real[2] ? g_real[2]->name : "CObject");
        add_text(line);
        return 0;
    case 3: add_text("CNoSuchClass\r\n{\r\n}\r\n"); return 0;
    case 4: return 0;
    case 5: add_text("Alpha=1\r\n"); return 0;
    case 6: add_text("{\r\nObject ID=0x00000010\r\nAlpha=1\r\nObject ID=0x20\r\n}\r\n"); return 0;
    case 7:
    case 8: {
        add_text("{\r\nReferenced Object List Location=0x00000000\r\n");
        if (v == 8)
            add_text("\tReferenced Object List Location=0x00000000\r\n");
        add_text("Object ID=16\r\nCount=3\r\nObject ID=0x30\r\n}\r\n");
        const DWORD list = g_input.size;
        add_text("Referenced Objects\r\n{\r\n\t2\r\n\t0x00000010\r\n\t0x00000030\r\n}\r\n");
        wsprintfA(line, "0x%08X", list);
        for (DWORD i = 0; i + 10 <= list; i++)
            if (memcmp(g_input.data + i, "0x00000000", 10) == 0)
                memcpy(g_input.data + i, line, 10);
        return 0;
    }
    case 9: add_text("{\r\nAlpha=1\r\nObject ID=0x10\r\n}\r\n"); return 0;
    case 10: add_text("{\r\nAlpha=1\r\nObject ID=0x10\r\n}\r\n"); return 1;
    case 11:
        add_text("{\r\n\r\n   \r\nno equals here\r\n\t\tAlpha  =x\r\n=empty name\r\nAlpha=\r\nALPHA=case\r\nOldGamma=alt\r\noldgamma=alt2\r\n}\r\n");
        return 1;
    case 12:
        add_text("{\r\n\r\n   \r\nno equals here\r\n\t\tAlpha  =x\r\n=empty name\r\nAlpha=\r\nALPHA=case\r\nOldGamma=alt\r\n}\r\n");
        return 0;
    case 13:
        add_text("{\r\nCFoo::Alpha=1\r\nX::Unknown=2\r\n::Beta Gamma=3\r\nA::B::Alpha=4\r\nCFoo::Referenced Object List Location=0x10\r\n"
                 "Alpha::=5\r\n::=6\r\n}\r\n");
        return 0;
    case 14: add_text("{\r\nAlpha=1\r\nBeta Gamma=2\r\nCount=3\r\n}\r\n"); return 0;
    case 15: add_text("{\r\nAlpha=1\r\n"); return 0;
    case 16: add_text("{\r\nAlpha=1\r\n  }  \r\n"); return 0;
    case 17: add_text("{\r\nAlpha=1\r\n}"); return 0;
    case 18: add_text("{\r\nAlpha=1\r\n{\r\nBeta Gamma=2\r\n}\r\n"); return 0;
    case 19: add_text("  {  \r\n}\r\n"); return 0;
    case 20: add_text("{\r\n}\r\n"); return 2;
    case 21: {
        add_text("{\r\nAlpha=");
        for (int i = 0; i < 1100; i++)
            add_text("x");
        add_text("\r\n}\r\n");
        return 0;
    }
    case 22: add_text("{\r\nTree List=5\r\nAlpha=1\r\n}\r\n"); return 1;
    case 23: add_text("{\nAlpha=1\n}\n"); return 0;
    case 24: add_text("\t{\r\n\tAlpha=1\r\n\t}\r\nAfter=2\r\n"); return 0;
    case 25: add_text("{\r\nAlpha=1\r\n"); add_text("x}\r\n"); return 0;
    case 26: add_text("{\r\n\tAlpha=\t value with = sign \r\n\tCount=0x7fffffff\r\n}\r\n"); return 2;
    case 27: add_text("CFakeReadA\r\n{\r\n}\r\n"); return 0;
    case 28:
        wsprintfA(line, "  %s  \r\n{\r\n}\r\n", g_real[0] ? g_real[0]->name : "CObject");
        add_text(line);
        return 0;
    default: add_text("{\r\nObject ID=junk\r\nObject ID=0x10\r\n}\r\n"); return 0;
    }
}
constexpr int NTEXTS = 30;

void test_read_text(Tally &t, int &n)
{
    for (int v = 0; v < NTEXTS; v++)
        for (DWORD mode : {0u, 1u})
            for (int extra = 0; extra < 4; extra++) {
                DWORD position;
                g_obj = build_text(v, position);
                g_use_hook = extra == 1;
                g_stop_object = extra == 2 ? &g_objs[g_obj] : extra == 3 ? &g_objs[(g_obj + 1) % NOBJ] : nullptr;
                g_stop_field = extra >= 2 ? (v % 2 ? "beta gamma" : "Beta Gamma") : nullptr;
                g_after_read = extra & 1;
                run(t, {"Object_ReadText", 0x416b60, 4, mode, &g_input, position, nullptr, call_read_text, after_read, 0xffffffff,
                        (unsigned)(v * 8 + mode * 4 + extra)});
                n++;
            }
    g_use_hook = false;
    g_stop_object = nullptr;
    g_stop_field = nullptr;
    g_after_read = 0;
}

// ---- Binary ------------------------------------------------------------------------------------------

void test_write_binary(Tally &t, int &n)
{
    for (DWORD mode : {0u, 1u, 2u})
        for (int o = 0; o < NOBJ; o++)
            for (int twice = 0; twice < 2; twice++) {
                g_obj = o;
                g_after_read = (DWORD)twice;
                run(t, {"Object_WriteBinary", 0x416f50, 2, mode, nullptr, 0, nullptr,
                        [](DWORD a, CObjectWriter *w) {
                            using F = DWORD(THISCALL *)(void *, CObjectWriter *);
                            DWORD r = fn<F>(a)(&g_objs[g_obj], w);
                            if (g_after_read) {
                                r ^= fn<F>(a)(&g_objs[(g_obj + 1) % NOBJ], w);
                                r ^= (at<DWORD(THISCALL *)(CObjectWriter *)>(0x524320)(w) & 0xff) << 8;
                            }
                            return r;
                        },
                        nullptr, 0xffffffff, (unsigned)(mode * 20 + o * 2 + twice)});
                n++;
            }
}

DWORD bits_of(float f) { return bits(f); }

const DWORD VERSIONS[] = {0x41d80000 /*27*/, 0x41c00000 /*24*/, 0x41c80000 /*25*/, 0x40800000 /*4*/, 0x40600000 /*3.5*/,
                          0x7fc00000 /*NaN*/, 0x41d7ffff, 0xc1d80000 /*-27*/};
DWORD g_mask;

void test_read_binary(Tally &t, int &n)
{
    for (DWORD mode : {0u, 1u})
        for (int v = 0; v < 8; v++)
            for (int o = 0; o < 3; o++)
                for (int k = 0; k < 6; k++) {
                    // k: 0 all fields true, 1 the second field false, 2 class version differs, 3 stored NaN,
                    // 4 slot 15 returns 0x100, 5 the ID is not in the referenced list
                    FakeObj &obj = g_objs[o];
                    CClassInfo *c = obj.cls;
                    obj.read_result = k == 4 ? 0x100 : 0xff01;
                    g_input.size = 0;
                    append_dword(g_input, VERSIONS[v]);
                    DWORD list_at = 0;
                    if (mode == 0) {
                        list_at = g_input.size;
                        append_dword(g_input, 0);
                        append_dword(g_input, 0x10 + o);
                    } else if (v == 1) {
                        append_dword(g_input, 0x99);   // skipped
                    }
                    append_dword(g_input, k == 2 ? bits_of(c->version + 1) : k == 3 ? 0x7fc00000 : bits_of(c->version));
                    for (DWORD i = 0; i < c->num_fields; i++)
                        append_dword(g_input, k == 1 && i == 1 ? 0x1200 : 0x3401);
                    append_dword(g_input, 0x15151515);
                    if (mode == 0) {
                        const DWORD list = g_input.size;
                        memcpy(g_input.data + list_at, &list, 4);
                        append_dword(g_input, LIST_MAGIC);
                        append_dword(g_input, 2);
                        append_dword(g_input, k == 5 ? 0x77 : 0x10 + o);
                        append_dword(g_input, 0x99);
                    }
                    g_obj = o;
                    // only the paths through the fields have a defined eax (the others the FPU status word)
                    g_mask = v == 0 && k != 2 ? 0xffffffff : 0xff;
                    run(t, {"Object_ReadBinary", 0x416fe0, 4, mode, &g_input, 0, nullptr,
                            [](DWORD a, CObjectWriter *w) {
                                return fn<DWORD(THISCALL *)(void *, CObjectWriter *)>(a)(&g_objs[g_obj], w);
                            },
                            [](CObjectWriter *w, State &s) { s.extra[0] = at<DWORD(THISCALL *)(CObjectWriter *)>(0x524320)(w) & 0xff; },
                            g_mask, (unsigned)(mode * 1000 + v * 100 + o * 10 + k)});
                    n++;
                }
    for (int o = 0; o < NOBJ; o++)
        g_objs[o].read_result = 1;
}

// ---- Object reference slots ---------------------------------------------------------------------------

CObjectRefFieldDesc *g_ref;   // a real object reference descriptor
alignas(4) BYTE g_ref_obj[0x2000];
char g_ref_value[2][64];
const char *const REF_TEXTS[] = {"0x00000010 (CSomeClass)", "0x10", "16 (X)", "(X)", "", "junk", "0x10 (unclosed", "x) (y", "0",
                                 "0x00000030 ()", "0x00000020 (CNull)", "0x77 (Unknown)", "  0x10", "0x10)", "0xffffffff (C)"};
const char *g_ref_text;
DWORD g_mode_arg;

void setup_ref(CObjectWriter *w)
{
    ObjectTable *t = table_of(w);
    t->mode = g_mode_arg;
    // loaded: 0x10 -> object 1, 0x20 -> null, 0x30 -> object 3
    const ObjectIdEntry e[3] = {{0x10, &g_objs[0]}, {0x20, nullptr}, {0x30, &g_objs[2]}};
    t->loaded.data = static_cast<ObjectIdEntry *>(heap_alloc(sizeof e));
    memcpy(t->loaded.data, e, sizeof e);
    t->loaded.count = t->loaded.capacity = 3;
    memset(g_ref_obj, 0xab, sizeof g_ref_obj);
    lstrcpyA(g_ref_value[g_side], g_ref_text);
}
void after_ref(CObjectWriter *w, State &s)
{
    s.extra[0] = *reinterpret_cast<DWORD *>(g_ref_obj + g_ref->offset);
    s.extra[1] = text_hash(g_ref_value[g_side]);
    // the table's fixup points into g_ref_obj: resolve with the finish
    s.extra[2] = at<DWORD(THISCALL *)(CObjectWriter *)>(0x524320)(w) & 0xff;
    s.extra[3] = *reinterpret_cast<DWORD *>(g_ref_obj + g_ref->offset);
}

void test_refs(Tally &t, int &n)
{
    if (!g_ref)
        return;
    for (const char *text : REF_TEXTS)
        for (DWORD mode : {0u, 1u}) {
            g_ref_text = text;
            g_mode_arg = mode;
            run(t, {"ObjectRefField_FromText", 0x523eb0, 4, mode, nullptr, 0, setup_ref,
                    [](DWORD a, CObjectWriter *w) {
                        using F = DWORD(THISCALL *)(CObjectRefFieldDesc *, BYTE *, char *, CObjectWriter *);
                        const DWORD r = fn<F>(a)(g_ref, g_ref_obj, g_ref_value[g_side], w);
                        return r == ptrv(g_ref_obj) ? 1 : relative_fixup(table_of(w), r);
                    },
                    // mode 1 with a number: 0x55cb50 leaves the caller's eax
                    after_ref, mode == 1 ? 0 : 0xffffffff, (unsigned)mode});
            n++;
        }
    // WriteBinary: null, objects, an ID; twice; modes 0 and 1
    for (DWORD mode : {0u, 1u})
        for (int v = 0; v < 5; v++)
            for (int twice = 0; twice < 2; twice++) {
                g_mode_arg = mode;
                g_obj = v;
                g_after_read = (DWORD)twice;
                run(t, {"ObjectRefField_WriteBinary", 0x524000, 2, mode, nullptr, 0, nullptr,
                        [](DWORD a, CObjectWriter *w) {
                            using F = DWORD(THISCALL *)(CObjectRefFieldDesc *, BYTE *, CObjectWriter *);
                            void *values[5] = {nullptr, &g_objs[0], &g_objs[1], reinterpret_cast<void *>(0x10), &g_objs[0]};
                            *reinterpret_cast<void **>(g_ref_obj + g_ref->offset) = values[g_obj];
                            DWORD r = fn<F>(a)(g_ref, g_ref_obj, w) == ptrv(w);
                            if (g_after_read) {
                                *reinterpret_cast<void **>(g_ref_obj + g_ref->offset) = values[(g_obj + 1) % 5];
                                r += 2 * (fn<F>(a)(g_ref, g_ref_obj, w) == ptrv(w));
                                fn<F>(a)(g_ref, g_ref_obj, w);
                            }
                            return r;
                        },
                        nullptr, 0xffffffff, (unsigned)(mode * 10 + v * 2 + twice)});
                n++;
            }
    // ReadBinary: IDs loaded or not, 0, a short stream (the reader's own address is the ID)
    const DWORD ids[] = {0x10, 0x20, 0x30, 0, 0x99};
    for (DWORD mode : {0u, 1u})
        for (int v = 0; v < 7; v++) {
            g_input.size = 0;
            if (v < 5)
                append_dword(g_input, ids[v]);
            else if (v == 6)
                append(g_input, "\x10\x00", 2);
            g_mode_arg = mode;
            g_ref_text = "";
            run(t, {"ObjectRefField_ReadBinary", 0x524030, 4, mode, &g_input, 0, setup_ref,
                    [](DWORD a, CObjectWriter *w) {
                        using F = DWORD(THISCALL *)(CObjectRefFieldDesc *, BYTE *, CObjectWriter *);
                        const DWORD r = fn<F>(a)(g_ref, g_ref_obj, w);
                        if (r == ((ptrv(g_ref_obj + g_ref->offset) & ~0xffu) | 1))
                            return (DWORD)1;
                        // the new fixup entry with al = 1: relative to the list's first entry
                        const ObjectTable *t = table_of(w);
                        return t->fixups.data ? (r & 0xff) | ((r & ~0xffu) - (ptrv(t->fixups.data) & ~0xffu)) << 8 : r;
                    },
                    after_ref, mode == 1 ? 0xff : 0xffffffff, (unsigned)(mode * 10 + v)});
            n++;
        }
}

// ---- Temp file slot 23 ---------------------------------------------------------------------------------

char g_dir[MAX_PATH];
FakeDesc g_temp_desc;
alignas(4) BYTE g_temp_obj[16];
char g_temp_path[MAX_PATH];
DWORD g_indent;

void write_file(const char *path, DWORD size, unsigned seed)
{
    HANDLE f = CreateFileA(path, 0x40000000, 0, nullptr, 2, 0x80, nullptr);
    if (f == (HANDLE)-1)
        return;
    g_seed = seed;
    BYTE block[4096];
    for (DWORD done = 0; done < size;) {
        DWORD k = size - done < sizeof block ? size - done : sizeof block;
        for (DWORD i = 0; i < k; i++) {
            const DWORD r = random() >> 24;
            block[i] = r < 30 ? '\r' : r < 60 ? '\n' : r < 70 ? '{' : (BYTE)r;
        }
        DWORD written;
        WriteFile(f, block, k, &written, nullptr);
        done += k;
    }
    CloseHandle(f);
}

void test_temp_file(Tally &t, int &n)
{
    if (!g_dir[0])
        return;
    init_fake_desc(g_temp_desc, "Temp", "", 900);
    g_temp_desc.offset = 4;
    const DWORD sizes[] = {0, 1, 100, 5000, 70000};
    for (int v = 0; v < 7; v++)
        for (DWORD indent : {0u, 2u}) {
            const char *path = nullptr;
            if (v < 5) {
                wsprintfA(g_temp_path, "%stemp%d.tmp", g_dir, v);
                write_file(g_temp_path, sizes[v], (unsigned)v + 3);
                path = g_temp_path;
            } else if (v == 5) {
                wsprintfA(g_temp_path, "%smissing.tmp", g_dir);
                path = g_temp_path;
            }
            *reinterpret_cast<const char **>(g_temp_obj + 4) = path;
            g_indent = indent;
            run(t, {"TempFileField_WriteText", 0x58bd50, 2, 1, nullptr, 0,
                    [](CObjectWriter *w) { w->indent = g_indent; },
                    [](DWORD a, CObjectWriter *w) {
                        using F = DWORD(THISCALL *)(CFieldDesc *, BYTE *, CObjectWriter *);
                        return fn<F>(a)(&g_temp_desc, g_temp_obj, w);
                    },
                    nullptr, 0xffffffff, (unsigned)v});
            n++;
            if (v < 5)
                DeleteFileA(g_temp_path);
        }
    free_fake_desc(g_temp_desc);
}

// ---- SkipSpace, SetReadHook ----------------------------------------------------------------------------

void test_small(Tally &t, int &n)
{
    const char *const texts[] = {"", "a", "  a", "\t\r\n x", "    ", "\t", "\xa0\xff text", " \x01 z", "abc  ", "\r\n",
                                 "  \t  \t\t= value", LONG_TEXT};
    for (const char *s : texts) {
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            r[side] = ptrv(fn<const char *(THISCALL *)(const char *)>(0x587ad0)(s)) - ptrv(s);
            if (side == 0)
                hooks_in();
        }
        char d[80];
        wsprintfA(d, "[0x587ad0 '%.20s': %u/%u]", s, r[0], r[1]);
        t.check(r[0] == r[1], "", d);
        n++;
    }
    void *const hooks[] = {nullptr, (void *)&fake_read_hook, reinterpret_cast<void *>(0x12345678)};
    for (void *h : hooks) {
        DWORD r[2];
        for (int side = 0; side < 2; side++) {
            *at<void **>(READ_HOOK) = reinterpret_cast<void *>(0xdead);
            if (side == 0)
                hooks_out();
            fn<void(__fastcall *)(void *)>(0x4163a0)(h);
            if (side == 0)
                hooks_in();
            r[side] = *at<DWORD *>(READ_HOOK);
        }
        t.check(r[0] == r[1] && r[0] == ptrv(h), "", "[0x4163a0]");
        n++;
    }
    *at<void **>(READ_HOOK) = nullptr;
}

// ---- Real data ------------------------------------------------------------------------------------------

struct MemoryInfo {
    void *base, *allocation_base;
    DWORD allocation_protect;
    size_t size;
    DWORD state, protect, type;
};
bool readable(const void *p, DWORD n)
{
    MemoryInfo mi;
    if (ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;   // kernel half, or the range wraps (then the loop below would not run)
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000) {
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0x66))
            return false;
    }
    return true;
}
// An object is recognised by its vtable: slot 2 is a class getter (mov eax, [class variable]) whose
// variable holds a registered class
CClassInfo *class_of_object(const void *p, CClassInfo **cls, int n)
{
    if (ptrv(p) < 0x10000 || (ptrv(p) & 3) || !readable(p, 4))
        return nullptr;
    const DWORD vt = *static_cast<const DWORD *>(p);
    if (vt < 0x5e0000 || vt >= 0x610000 || (vt & 3))
        return nullptr;
    const DWORD getter = *at<DWORD *>(vt + 8);
    if (getter < 0x401000 || getter >= 0x5e0000 || *static_cast<const BYTE *>(original_address(getter)) != 0xa1)
        return nullptr;
    // through the trampoline when the getter is hooked (layer.cpp hooks CLayer's)
    const DWORD var = *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(original_address(getter)) + 1);
    if (var < 0x600000 || var >= 0x720000)
        return nullptr;
    CClassInfo *c = *at<CClassInfo **>(var);
    for (int i = 0; i < n; i++)
        if (cls[i] == c)
            return c;
    return nullptr;
}
constexpr int MAX_REAL = 48;
void *g_real_objects[MAX_REAL];
int g_nreal;

void *g_real_object;
DWORD g_real_call;   // 0x4168f0 text, 0x416f50 binary
DWORD call_real_write(DWORD, CObjectWriter *w)
{
    const DWORD r = at<DWORD(THISCALL *)(void *, CObjectWriter *)>(g_real_call)(g_real_object, w);
    return r ^ (at<DWORD(THISCALL *)(CObjectWriter *)>(0x524320)(w) & 0xff) << 16;
}

// The mirror of a real class: our descriptors with the real field names
struct Mirror {
    CClassInfo cls;
    FakeDesc *descs;
    CFieldDesc **fields;
    DWORD n;
};
Mirror g_mirror;
FakeObj g_mirror_obj;
constexpr DWORD SCRATCH = 0x10000;
BYTE *g_scratch;
DWORD g_skipped;

void build_mirror(CClassInfo *real)
{
    g_mirror.n = real->num_fields;
    g_mirror.descs = static_cast<FakeDesc *>(heap_alloc(sizeof(FakeDesc) * (g_mirror.n + 1)));
    g_mirror.fields = static_cast<CFieldDesc **>(heap_alloc(4 * (g_mirror.n + 1)));
    for (DWORD i = 0; i < g_mirror.n; i++) {
        CFieldDesc *f = real->fields[i];
        init_fake_desc(g_mirror.descs[i], f->name.c_str(), f->alt_name.c_str(), i);
        g_mirror.descs[i].real = f;
        g_mirror.descs[i].offset = f->offset;
        g_mirror.fields[i] = &g_mirror.descs[i];
    }
    memset(&g_mirror.cls, 0, sizeof(CClassInfo));
    g_mirror.cls.name = real->name;
    g_mirror.cls.fields = g_mirror.fields;
    g_mirror.cls.num_fields = g_mirror.n;
    g_mirror.cls.parent = real;
    g_mirror.cls.version = real->version;
    memset(&g_mirror_obj, 0, sizeof g_mirror_obj);
    g_mirror_obj.vftable = ptrv(g_fobj_vt);
    g_mirror_obj.id = 0x100;
    g_mirror_obj.cls = &g_mirror.cls;
    g_mirror_obj.read_result = 1;
}
void free_mirror()
{
    for (DWORD i = 0; i < g_mirror.n; i++)
        free_fake_desc(g_mirror.descs[i]);
    heap_free(g_mirror.descs);
    heap_free(g_mirror.fields);
}

// Skips the block that follows a value ("{" ... "}", nested), if there is one
void skip_block(CObjectWriter *r)
{
    char line[0x400];
    const DWORD at_ = at<DWORD(THISCALL *)(CFileBuf *)>(0x4a2180)(r);
    if (!(at<DWORD(THISCALL *)(CFileBuf *, char *, DWORD)>(0x4a20a0)(r, line, sizeof line) & 0xff))
        return;
    const char *t = at<const char *(THISCALL *)(const char *)>(0x587ad0)(line);
    if (lstrcmpA(t, "{") != 0) {
        at<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a20e0)(r, at_);
        return;
    }
    int depth = 1;
    DWORD lines = 1;
    while (depth > 0 && (at<DWORD(THISCALL *)(CFileBuf *, char *, DWORD)>(0x4a20a0)(r, line, sizeof line) & 0xff)) {
        t = at<const char *(THISCALL *)(const char *)>(0x587ad0)(line);
        depth += lstrcmpA(t, "{") == 0 ? 1 : lstrcmpA(t, "}") == 0 ? -1 : 0;
        lines++;
    }
    g_skipped += lines;
    ev(0x5b, lines);
}
void mirror_from_text(FakeDesc *d, void *, char *value, CObjectWriter *r)
{
    if (d->real && d->real->vftable == VT_OBJECT_REF && d->real->offset + 4 <= SCRATCH)
        at<DWORD(THISCALL *)(CFieldDesc *, BYTE *, char *, CObjectWriter *)>(0x523eb0)(d->real, g_scratch, value, r);
    skip_block(r);
}
void mirror_own_line(FakeObj *, const char *, char *, CObjectWriter *r) { skip_block(r); }
void setup_mirror(CObjectWriter *) { memset(g_scratch, 0, SCRATCH); }
void after_mirror(CObjectWriter *, State &s)
{
    s.extra[0] = fnv(2166136261u, g_scratch, SCRATCH);
    s.extra[1] = *at<DWORD *>(FIND_FIELD_CACHE);
}
DWORD call_mirror_read(DWORD a, CObjectWriter *w)
{
    const DWORD r = stream_relative(w, call_escapable(a, &g_mirror_obj, w));
    return r ^ (at<DWORD(THISCALL *)(CObjectWriter *)>(0x524320)(w) & 0xff) << 16;
}

// The image kind's slot 26 (0x4a9740) writes a 0x24-byte frame header ('1', ...) from the stack whose
// bytes 0xa and 0xb are never set: they hold whatever the callers left there. Those two bytes are
// cleared in both outputs before the comparison (and counted).
DWORD g_padding;
void clear_frame_padding()
{
    Bytes &a = g_state[0].out, &b = g_state[1].out;
    if (a.size != b.size)
        return;
    for (DWORD i = 11; i < a.size; i++) {
        if (a.data[i] == b.data[i])
            continue;
        for (DWORD k : {10u, 11u})
            if (a.data[i - k] == 0x31 && b.data[i - k] == 0x31) {
                a.data[i - k + 10] = b.data[i - k + 10] = 0;
                a.data[i - k + 11] = b.data[i - k + 11] = 0;
                g_padding++;
                break;
            }
    }
}

// The bytes around the first difference of the two outputs, and whether the original writes the same
// bytes twice
void dump_difference(DWORD mode)
{
    const Bytes &a = g_state[0].out, &b = g_state[1].out;
    DWORD i = 0;
    while (i < a.size && i < b.size && a.data[i] == b.data[i])
        i++;
    const DWORD from = i >= 16 ? i - 16 : 0;
    for (int s = 0; s < 2; s++) {
        const Bytes &o = g_state[s].out;
        char line[200];
        int k = wsprintfA(line, "    side %d at %u:", s, from);
        for (DWORD j = from; j < from + 40 && j < o.size; j++)
            k += wsprintfA(line + k, " %02x", o.data[j]);
        log("%s", line);
    }
    Bytes first = {nullptr, 0, 0};
    store(first, a.data, a.size);
    Tally again{"(original twice)"};
    run(again, {"original twice", 0, 2, mode, nullptr, 0, nullptr, call_real_write, nullptr, 0xffffffff, 0});
    log("    the original again: %s", same_bytes(first, g_state[0].out) ? "same bytes" : "other bytes");
    VirtualFree(first.data, 0, 0x8000);
}

void test_real(CClassInfo **cls, int n, Tally &text, Tally &binary, Tally &read, DWORD &bytes, DWORD &bin_bytes, DWORD &ms)
{
    // the live objects only (check_real_objects.cpp), with up to 24 dialogue lines as field_streams
    g_nreal = find_real_objects(g_real_objects, nullptr, MAX_REAL, cls, n, 24);
    g_scratch = static_cast<BYTE *>(VirtualAlloc(nullptr, SCRATCH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    const DWORD start = GetTickCount();
    Bytes written = {nullptr, 0, 0};
    for (int i = 0; i < g_nreal; i++) {
        g_real_object = g_real_objects[i];
        CClassInfo *c = class_of_object(g_real_object, cls, n);
        log("  real object %d: %s", i, c->name);
        for (DWORD mode : {1u, 0u}) {
            log("    text and binary, mode %u", mode);
            g_real_call = 0x4168f0;
            run(text, {c->name, 0, 2, mode, nullptr, 0, nullptr, call_real_write, nullptr, 0xffffffff, (unsigned)i});
            bytes += g_state[0].out.size;
            if (mode == 0)
                store(written, g_state[0].out.data, g_state[0].out.size);
            // the original itself ends the game writing a CZaxSaveGameStats as binary (run
            // 20261023-165033-795a, side 0 with all hooks of this batch out; likely its image field
            // "Screen Shot" without a screenshot)
            if (lstrcmpA(c->name, "CZaxSaveGameStats") == 0)
                continue;
            g_real_call = 0x416f50;
            g_normalize = clear_frame_padding;
            run(binary, {c->name, 0, 2, mode, nullptr, 0, nullptr, call_real_write, nullptr, 0xffffffff, (unsigned)i});
            g_normalize = nullptr;
            bin_bytes += g_state[0].out.size;
            if (!same_bytes(g_state[0].out, g_state[1].out))
                dump_difference(mode);
        }
        // the text read back into the mirror, mode 0 (object IDs, the list, references) and 1
        build_mirror(c);
        g_on_from_text = mirror_from_text;
        g_on_own_line = mirror_own_line;
        for (DWORD mode : {0u, 1u}) {
            log("    read back, mode %u", mode);
            run(read, {c->name, 0x416b60, 4, mode, &written, 0, setup_mirror, call_mirror_read, after_mirror, 0xffffffff, (unsigned)i});
            if (g_state[0].ret != g_state[1].ret || (g_state[0].ret & 0xff) != 1)
                log("  read back %s mode %u: result %x/%x", c->name, mode, g_state[0].ret, g_state[1].ret);
        }
        g_on_from_text = nullptr;
        g_on_own_line = nullptr;
        free_mirror();
    }
    ms = GetTickCount() - start;
    if (written.data)
        VirtualFree(written.data, 0, 0x8000);
    VirtualFree(g_scratch, 0, 0x8000);
}

}  // namespace

ZAX_CHECKS(object_io)
{
    Tally read{"object io: Object_ReadText 0x416b60"}, binary{"object io: binary 0x416f50/0x416fe0"},
        refs{"object io: object reference slots 19/26/29"}, temp{"object io: temp file slot 23 0x58bd50"},
        small{"object io: 0x587ad0, 0x4163a0"}, real_text{"object io: objects of the running game as text with the list (real data)"},
        real_binary{"object io: objects of the running game as binary (real data)"},
        real_read{"object io: the game's objects read back as text (real data)"};
    save_globals();
    init_stream_vtable();
    init_fakes(cls, n);
    reporter_in();
    g_reset = reset;
    g_ref = nullptr;
    for (int i = 0; i < n && !g_ref; i++)
        for (DWORD f = 0; f < cls[i]->num_fields && !g_ref; f++)
            if (cls[i]->fields[f]->vftable == VT_OBJECT_REF && cls[i]->fields[f]->offset + 4 <= sizeof g_ref_obj)
                g_ref = static_cast<CObjectRefFieldDesc *>(cls[i]->fields[f]);
    int nread = 0, nbinary = 0, nrefs = 0, ntemp = 0, nsmall = 0;
    test_read_text(read, nread);
    read.report();
    test_write_binary(binary, nbinary);
    test_read_binary(binary, nbinary);
    binary.report();
    test_refs(refs, nrefs);
    refs.report();
    char tmp[MAX_PATH];
    g_dir[0] = 0;
    if (GetTempPathA(MAX_PATH - 40, tmp)) {
        wsprintfA(g_dir, "%sZaxObjectIO\\", tmp);
        CreateDirectoryA(g_dir, nullptr);
    }
    test_temp_file(temp, ntemp);
    temp.report();
    if (g_dir[0] && !RemoveDirectoryA(g_dir))
        log("  could not remove %s", g_dir);
    test_small(small, nsmall);
    small.report();
    DWORD bytes = 0, bin_bytes = 0, ms = 0;
    test_real(cls, n, real_text, real_binary, real_read, bytes, bin_bytes, ms);
    real_text.report();
    real_binary.report();
    real_read.report();
    log("  read cases %d, binary %d, reference %d, temp file %d, small %d; reference descriptor %s; real objects %d: "
        "%u text bytes, %u binary bytes (%u frame header paddings cleared), %u lines skipped, %u ms",
        nread, nbinary, nrefs, ntemp, nsmall, g_ref ? g_ref->name.c_str() : "-", g_nreal, bytes, bin_bytes, g_padding, g_skipped, ms);
    g_reset = nullptr;
    reporter_out();
    free_fakes();
    restore_globals();
}

}  // namespace zax
