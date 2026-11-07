// Checks of the object file functions, the change test, reading from a stream and the ZString stream
// operators (object_files.cpp) against the original, in the game thread, mostly on real data: the
// objects of the running game and real level files. Side 0 runs with the hooks of all object modules
// taken out (checks_object_common.h), side 1 through the reimplementation; both create, read, write
// and delete their own objects, and what they produced is compared as text or binary (mode 1, no
// addresses). Files are only written into an own temp folder, which is removed at the end.
#include "checks_object_common.h"
#include "object_files.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT DWORD WINAPI GetTickCount(void);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);
WINIMPORT DWORD WINAPI GetFileSize(HANDLE f, DWORD *high);
WINIMPORT BOOL WINAPI ReadFile(HANDLE f, void *buf, DWORD n, DWORD *read, void *overlapped);
WINIMPORT char *WINAPI lstrcpynA(char *dst, const char *src, int n);

namespace zax {

namespace {
using namespace objcheck;

// ---- Helpers -----------------------------------------------------------------------------------------

void *registry() { return at<void *(__cdecl *)()>(0x415d20)(); }
void *create(const char *name) { return at<void *(THISCALL *)(void *, const char *, int, int)>(0x415c00)(registry(), name, 0, -1); }
void destroy(void *o)
{
    if (o)
        vcall<DWORD>(o, 1, 1);
}
CClassInfo *class_of(const void *o) { return vcall<CClassInfo *>(o, 2); }

// The object written as text (mode 1) or binary (mode 1) into out
void text_of(void *o, Bytes &out, bool binary = false)
{
    CObjectWriter w;
    open_stream(&w, 2, 1, nullptr, 0, 0);
    at<DWORD(THISCALL *)(void *, CObjectWriter *)>(binary ? 0x416f50 : 0x4168f0)(o, &w);
    const MemoryStream *ms = static_cast<const MemoryStream *>(w.stream);
    store(out, ms->data, ms->data ? ms->size : 0);
    close_stream(&w);
}

// Images and everything drawn (CSequence, CFrameInterface: frames, fonts, sequences, the layer) are not
// created or copied here: copies of the game's ones end the game (CLayer read back, run
// 20261023-173458-e0b5; CRotationSequence copied, run 20261023-183922-6202), and rendering stays out of
// this batch
// The game's single objects (rules, music mixer, save data, map lists): copies of them are not created
// either (after the round trips of run 20261023-195209-17ce the game ended in the next module's checks)
bool is_single_object(CClassInfo *c)
{
    const char *const names[] = {"CRules", "CMusicMixer", "CZaxSaveData", "CZaxSaveGameStats", "COtherMapList",
                                 "CSwappedLayerFilenameMappingTable"};
    for (const char *name : names)
        if (lstrcmpA(c->name, name) == 0)
            return true;
    return false;
}

bool is_render_class(CClassInfo *c)
{
    for (; c; c = c->parent)
        if (lstrcmpA(c->name, "CSequence") == 0 || lstrcmpA(c->name, "CFrameInterface") == 0)
            return true;
    return false;
}

// Both sides: side 0 with the object modules' hooks out
// The function on side 0 through its trampoline, on side 1 through the hooked exe address. The hooks
// of this module are not taken out: some of these functions also run in other threads, and putting
// their first bytes back during the checks ended the game later (runs 20261023-201316-7306 to
// -202751-ea13, in the next module's checks writing the running level)
template <typename F> F pick(DWORD address) { return g_side == 0 ? original<F>(address) : at<F>(address); }
template <typename R, typename... A> R vslot(CFieldDesc *d, int n, A... a) { return pick<R(THISCALL *)(CFieldDesc *, A...)>(slot(d, n))(d, a...); }

template <typename F> void both(F body)
{
    for (int s = 0; s < 2; s++) {
        g_side = s;
        g_nlog[s] = 0;
        g_loghash[s] = 2166136261u;
        body(s);
    }
}

// The image kind's slot 26 writes two uninitialised bytes of a frame header (see checks_object_io.cpp):
// cleared in both outputs before they are compared
DWORD g_padding;
void clear_frame_padding(Bytes &a, Bytes &b)
{
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

void check_pair(Tally &t, const Bytes &a, const Bytes &b, DWORD ra, DWORD rb, const char *what)
{
    const bool same = same_bytes(a, b) && ra == rb;
    char d[200];
    DWORD i = 0;
    while (i < a.size && i < b.size && a.data[i] == b.data[i])
        i++;
    wsprintfA(d, "[%.60s: size %u/%u, first difference %u, result %x/%x]", what, a.size, b.size, i, ra, rb);
    t.check(same, "", d);
}

bool file_bytes(const char *path, Bytes &out)
{
    out.size = 0;
    HANDLE f = CreateFileA(path, 0x80000000, 1, nullptr, 3, 0x80, nullptr);
    if (f == (HANDLE)-1)
        return false;
    const DWORD n = GetFileSize(f, nullptr);
    store(out, nullptr, 0);
    BYTE *buf = static_cast<BYTE *>(VirtualAlloc(nullptr, n + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    DWORD got = 0;
    if (buf) {
        ReadFile(f, buf, n, &got, nullptr);
        store(out, buf, got);
        VirtualFree(buf, 0, 0x8000);
    }
    CloseHandle(f);
    return true;
}
void put_file(const char *path, const Bytes &b)
{
    HANDLE f = CreateFileA(path, 0x40000000, 0, nullptr, 2, 0x80, nullptr);
    if (f == (HANDLE)-1)
        return;
    DWORD written;
    if (b.size)
        WriteFile(f, b.data, b.size, &written, nullptr);
    CloseHandle(f);
}

// ---- Real objects of the running game (as in checks_object_io.cpp) ------------------------------------

// The objects: find_real_objects (check_real_objects.cpp)
constexpr int MAX_REAL = 48;
void *g_real[MAX_REAL];
int g_nreal;

Bytes g_src, g_out[2], g_file[2];
bool g_steps = false;  // log each step (to find where the game ends)
void step(int s, const char *what)
{
    if (g_steps)
        log("    side %d %s", s, what);
}
DWORD g_bytes, g_identical;

// Every real object: written as text (and binary), read into a new object of its class with
// Object_ReadFromStream (and Object_ReadBinary), written again
void test_round_trips(Tally &text, Tally &binary)
{
    for (int i = 0; i < g_nreal; i++) {
        CClassInfo *c = class_of(g_real[i]);
        if (is_render_class(c) || is_single_object(c))
            continue;
        log("  round trip %d: %s", i, c->name);
        text_of(g_real[i], g_src);
        DWORD r[2];
        both([&](int s) {
            step(s, "create");
            void *o = create(c->name);
            CObjectWriter w;
            open_stream(&w, 4, 1, g_src.data, g_src.size, 0);
            step(s, "read");
            r[s] = pick<DWORD(THISCALL *)(void *, CObjectWriter *)>(0x416af0)(o, &w) & 0xff;
            close_stream(&w);
            step(s, "write");
            text_of(o, g_out[s]);
            step(s, "delete");
            destroy(o);
        });
        check_pair(text, g_out[0], g_out[1], r[0], r[1], c->name);
        g_bytes += g_out[0].size;
        g_identical += same_bytes(g_out[0], g_src);
        if (lstrcmpA(c->name, "CZaxSaveGameStats") == 0)
            continue;   // the original ends the game writing it as binary (checks_object_io.cpp)
        text_of(g_real[i], g_src, true);
        both([&](int s) {
            void *o = create(c->name);
            CObjectWriter w;
            open_stream(&w, 4, 1, g_src.data, g_src.size, 0);
            r[s] = at<DWORD(THISCALL *)(void *, CObjectWriter *)>(0x416fe0)(o, &w) & 0xff;
            close_stream(&w);
            text_of(o, g_out[s], true);
            destroy(o);
        });
        clear_frame_padding(g_out[0], g_out[1]);
        check_pair(binary, g_out[0], g_out[1], r[0], r[1], c->name);
        g_bytes += g_out[0].size;
    }
}

// ---- Object files and level files --------------------------------------------------------------------

// Files the game reads with 0x4164c0 (canned objects, death behaviours, inventory definitions,
// categories, damage types)
const char *const OBJECT_FILES[] = {
    "Resources/Canned Objects/Does player have a flag.can",
    "Resources/Canned Objects/Drop Cystals and Ore.can",
    "Resources/Canned Objects/Drop Ore in Container.can",
    "Resources/Canned Objects/Picked up a Flag.can",
    "Resources/Canned Objects/Returned a Flag.can",
    "Resources/Canned Objects/Stupid Print Test.can",
    "Resources/Canned Objects/Trigger Enemy Killed Quip.can",
    "Death Behaviors/!None.txt",
    "Death Behaviors/AnimalMultiDefault.txt",
    "Death Behaviors/Bug 1 Default.txt",
    "Death Behaviors/Butu Default.txt",
    "Death Behaviors/Cave Dweller Default.txt",
    "Death Behaviors/Companion Drone-Light Default.txt",
    "Death Behaviors/Crater Default.txt",
    "Death Behaviors/Crutch Default.txt",
    "Death Behaviors/Cyborg Korbo Default.txt",
    "Death Behaviors/Din Default.txt",
    "Death Behaviors/Exploding Crate Default.txt",
    "Death Behaviors/Goodie Box 1 Default.txt",
    "Inventory/Item Definitions/!None.txt",
    "Inventory/Item Definitions/Alien Electrical Weapon 02.txt",
    "Inventory/Item Definitions/Alien Electrical Weapon Parts 02.txt",
    "Inventory/Item Definitions/Autogun Medium.txt",
    "Inventory/Item Definitions/Battery Charge.txt",
    "Inventory/Item Definitions/Battery I.txt",
    "Inventory/Item Definitions/Battery Level 2.txt",
    "Inventory/Item Definitions/Battery Level 3.txt",
    "Inventory/Item Definitions/Battery Level 4.txt",
    "Inventory/Item Definitions/Blue Flag.txt",
    "Inventory/Item Definitions/Bullets.txt",
    "Inventory/Item Definitions/Companion Drone Light.txt",
    "Inventory/Item Types/Ammunition.txt",
    "Inventory/Item Types/Autogun.txt",
    "Inventory/Item Types/Character.txt",
    "Categories/Enemy Shot.txt",
    "Categories/Enemy.txt",
    "Categories/NonInteractiveSequence Actor.txt",
    "Damage Types/!None.txt",
    "Damage Types/Normal.txt",
    "Damage Types/Unavoidable.txt",
};
// Level files are not read here: the game reads them into the CLayerSaveData inside a new CLayer
// (0x4d33f9: 0x416a00 at +0x3c, stopping at "Tree List"); read on their own, with 0x4164c0 (run
// 20261023-194100-efba) or into a CLayerSaveData of their own with the stop (run
// 20261023-194846-2266), the original ends the game as well, and the CLayer belongs to the rendering.
// The running level (CLayer) goes through 0x4167e0 and 0x417100 below.
int g_files_read, g_level_bytes;

ZString *zs_ctor_text_(ZString *z, const char *s) { return at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
void *create_from_file(const char *path, DWORD flags, DWORD write_back, void **out)
{
    ZString z;
    zs_ctor_text_(&z, path);
    void *o = pick<void *(__fastcall *)(ZString *, DWORD, DWORD, void **)>(0x4164c0)(&z, flags, write_back, out);
    zs_dtor(&z);
    return o;
}
unsigned last_file_hash() { return text_hash(at<ZString *>(0x713c14)->c_str()); }

void test_files(Tally &t)
{
    for (const char *path : OBJECT_FILES) {
        DWORD r[2];
        unsigned last[2];
        char cls_name[64] = "";
        both([&](int s) {
            step(s, path);
            void *out = reinterpret_cast<void *>(0x1234);
            void *o = create_from_file(path, 0, 0, &out);
            r[s] = (o != nullptr) | (out == o ? 2 : out == reinterpret_cast<void *>(0x1234) ? 4 : 0);
            last[s] = last_file_hash();
            g_out[s].size = 0;
            if (o) {
                text_of(o, g_out[s]);
                if (s == 0)
                    lstrcpynA(cls_name, class_of(o)->name, sizeof cls_name);
            }
            destroy(o);
        });
        g_files_read += r[0] & 1;
        g_bytes += g_out[0].size;
        check_pair(t, g_out[0], g_out[1], r[0] ^ last[0], r[1] ^ last[1], path);
        if (!(r[0] & 1)) {
            log("  %s: not read", path);
            continue;
        }
        // the same file into an existing object (0x416a00)
        both([&](int s) {
            void *o = create(cls_name);
            r[s] = pick<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x416a00)(o, path, 0) & 0xff;
            last[s] = last_file_hash();
            text_of(o, g_out[s]);
            destroy(o);
        });
        check_pair(t, g_out[0], g_out[1], r[0] ^ last[0], r[1] ^ last[1], "Object_ReadFile");
    }
    // a file that does not exist: 0x4164c0 gives null, 0x416a00 false after slot 19
    DWORD r[2];
    both([&](int s) {
        void *o = create_from_file("Levels/No Such Level.zax", 0, 1, nullptr);
        void *p = create("CConditionalAction");   // not CRules: that replaces the game's rules object
        const DWORD k = pick<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x416a00)(p, "Levels/No Such Level.zax", 0) & 0xff;
        r[s] = ptrv(o) | k << 8;
        text_of(p, g_out[s]);
        destroy(p);
    });
    check_pair(t, g_out[0], g_out[1], r[0], r[1], "missing file");
}

// ---- Writing files (temp folder) -------------------------------------------------------------------------

char g_dir[MAX_PATH], g_path[MAX_PATH];

// The text with tabs replaced by two spaces and an empty line after "{": the same object, other bytes
void variant_text(const Bytes &src, Bytes &out)
{
    out.size = 0;
    for (DWORD i = 0; i < src.size; i++) {
        if (src.data[i] == '\t')
            append_text(out, "  ");
        else
            append(out, &src.data[i], 1);
        if (src.data[i] == '{' && i + 2 < src.size && src.data[i + 1] == '\r')
            append_text(out, "\r\n");
    }
}

void test_write_file(Tally &t, Tally &changed)
{
    if (!g_dir[0])
        return;
    wsprintfA(g_path, "%sobject.txt", g_dir);
    Bytes canonical = {nullptr, 0, 0}, other = {nullptr, 0, 0};
    for (int i = 0; i < g_nreal; i++) {
        void *object = g_real[i];
        CClassInfo *c = class_of(object);
        // the drawn objects (the running level among them) are only written, not created
        const bool render = is_render_class(c) || is_single_object(c);
        text_of(object, canonical);
        if (lstrcmpA(c->name, "CLayer") == 0)
            g_level_bytes += canonical.size;
        variant_text(canonical, other);
        // 0: no file, 1: the same text, 2: another text, 3: empty file
        for (int v = 0; v < 4; v++) {
            DWORD r[2];
            both([&](int s) {
                DeleteFileA(g_path);
                if (v == 1)
                    put_file(g_path, canonical);
                else if (v == 2)
                    put_file(g_path, other);
                else if (v == 3)
                    put_file(g_path, Bytes{nullptr, 0, 0});
                r[s] = pick<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x4167e0)(object, g_path, 0) & 0xff;
                if (!file_bytes(g_path, g_file[s]))
                    r[s] |= 0x100;
            });
            check_pair(t, g_file[0], g_file[1], r[0], r[1], c->name);
        }
        // write back on reading (0x4164c0 with write back): the variant becomes the canonical text
        DWORD r[2];
        if (!render) {
            both([&](int s) {
                put_file(g_path, other);
                void *o = create_from_file(g_path, 0, 1, nullptr);
                r[s] = o != nullptr;
                destroy(o);
                file_bytes(g_path, g_file[s]);
            });
            check_pair(t, g_file[0], g_file[1], r[0], r[1], "write back");
        }
        // the change test against itself, the next object and a fresh object of the class
        for (int k = 0; k < (render ? 2 : 3); k++) {
            both([&](int s) {
                void *fresh = k == 2 ? create(c->name) : nullptr;
                void *b = k == 0 ? object : k == 1 ? g_real[(i + 1) % g_nreal] : fresh;
                r[s] = pick<DWORD(THISCALL *)(void *, void *)>(0x417100)(object, b);
                destroy(fresh);
            });
            char d[120];
            wsprintfA(d, "[Object_HasChanged %s %d: %x/%x]", c->name, k, r[0], r[1]);
            changed.check(r[0] == r[1], "", d);
        }
    }
    DeleteFileA(g_path);
    if (canonical.data)
        VirtualFree(canonical.data, 0, 0x8000);
    if (other.data)
        VirtualFree(other.data, 0, 0x8000);
}

// ---- ZString stream operators -------------------------------------------------------------------------------

void test_zstrings(Tally &t)
{
    const char *const texts[] = {"", "a", "Some text", LONG_TEXT};
    for (const char *s0 : texts) {
        DWORD r[2];
        both([&](int s) {
            CObjectWriter w;
            open_stream(&w, 2, 1, nullptr, 0, 0);
            ZString z;
            zs_ctor_text(&z, s0);
            r[s] = pick<CFileBuf *(__fastcall *)(CFileBuf *, ZString *)>(0x4162d0)(&w, &z) == &w;
            zs_dtor(&z);
            const MemoryStream *ms = static_cast<const MemoryStream *>(w.stream);
            store(g_out[s], ms->data, ms->data ? ms->size : 0);
            close_stream(&w);
        });
        check_pair(t, g_out[0], g_out[1], r[0], r[1], "FileBuf_WriteZString");
    }
    // reading: counted texts, a length beyond the end, a stream shorter than the length dword
    for (int v = 0; v < 7; v++) {
        g_src.size = 0;
        const DWORD lengths[] = {0, 1, 9, 300, 1000, 5};
        if (v < 6) {
            append_dword(g_src, lengths[v]);
            for (DWORD i = 0; i < (v == 4 ? 20 : lengths[v]); i++)
                append(g_src, &LONG_TEXT[i % 200], 1);
            append_text(g_src, "tail");
        } else {
            append(g_src, "\x03\x00", 2);
        }
        for (int start : {0, 1}) {
            DWORD r[2];
            both([&](int s) {
                CObjectWriter w;
                open_stream(&w, 4, 1, g_src.data, g_src.size, 0);
                ZString z;
                zs_ctor_text(&z, start ? "previous" : "");
                *at<int *(__cdecl *)()>(0x5d1014)() = 77;
                const bool same = pick<CFileBuf *(__fastcall *)(CFileBuf *, ZString *)>(0x416310)(&w, &z) == &w;
                const DWORD err = (DWORD)*at<int *(__cdecl *)()>(0x5d1014)();
                const MemoryStream *ms = static_cast<const MemoryStream *>(w.stream);
                r[s] = same | err << 8 | ms->position << 16;
                g_out[s].size = 0;
                append_text(g_out[s], z.c_str());
                zs_dtor(&z);
                close_stream(&w);
            });
            check_pair(t, g_out[0], g_out[1], r[0], r[1], "FileBuf_ReadZString");
        }
    }
}

}  // namespace

ZAX_CHECKS(object_files)
{
    Tally trips{"object files: real objects read back as text (0x416af0, real data)"},
        binary{"object files: real objects read back as binary (real data)"},
        levels{"object files: object and level files 0x4164c0/0x416a00 (real data)"},
        files{"object files: Object_WriteFile 0x4167e0, write back (temp folder)"},
        changed{"object files: Object_HasChanged 0x417100"}, strings{"object files: ZString stream operators 0x4162d0/0x416310"};
    init_stream_vtable();
    const DWORD start = GetTickCount();
    // the live objects only (check_real_objects.cpp), with up to 24 dialogue lines as field_streams
    g_nreal = find_real_objects(g_real, nullptr, MAX_REAL, cls, n, 24);
    test_round_trips(trips, binary);
    trips.report();
    binary.report();
    test_files(levels);
    levels.report();
    char tmp[MAX_PATH];
    g_dir[0] = 0;
    if (GetTempPathA(MAX_PATH - 40, tmp)) {
        wsprintfA(g_dir, "%sZaxObjectFiles\\", tmp);
        CreateDirectoryA(g_dir, nullptr);
    }
    test_write_file(files, changed);
    files.report();
    changed.report();
    if (g_dir[0] && !RemoveDirectoryA(g_dir))
        log("  could not remove %s", g_dir);
    test_zstrings(strings);
    strings.report();
    log("  real objects %d (%u read back identical to the source), object files read %d, %u bytes compared, running "
        "level %u bytes, %u frame header paddings cleared, %u ms",
        g_nreal, g_identical, g_files_read, g_bytes, g_level_bytes, g_padding, GetTickCount() - start);
}

}  // namespace zax
