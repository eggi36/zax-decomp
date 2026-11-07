// Checks of core_rest_b.cpp against the original, in the game thread (side 0 with the module's hooks out,
// side 1 through the reimplementation):
//  - the dynamic property list: random sequences of get, get-or-add, add, remove and for-each on a list of
//    each side with eight fake descriptors (their slots 6, 8, 32, 35 and 36 recorders; slot 35 now and then
//    gives the value 0), the list (count, room, entries) and the event log compared after every step;
//  - the point descriptor: the dynamic value and its setter on fake descriptors with Object_GetDynamic /
//    Object_GetOrAddDynamic stand-ins (a value block or none, a setter or none), the value array's size;
//  - the row table: tables of random columns and rows (compared by their entries), freed through the free;
//  - the Win32 wrappers with stand-ins in the import table (only while the call runs): the module's path,
//    the current directory, PostMessage, the cursor functions, the message box, CreateProcess (the command
//    line and the start-up information compared), the tick count; nothing of the game changes;
//  - the server options: the defaults and the name with stand-ins for the INI functions (IniFile::GetText
//    0x46ec70 assigns one of the test names, Ini_WriteString 0x4a47c0 records).
#include "check_util.h"
#include "checks_inventory_common.h"
#include "zstring.h"

namespace zax {

int hooks_core_rest_b(Hook *out);

namespace {

DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }
template <typename T> T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }

void out_() { invcheck::hooks_out(); }
void in_() { invcheck::hooks_in(); }

// The event log of a side
DWORD g_events[2][256];
int g_nevents[2];
int g_side;
void ev(DWORD v)
{
    if (g_nevents[g_side] < 256)
        g_events[g_side][g_nevents[g_side]] = v;
    g_nevents[g_side]++;
}
bool same_events() { return g_nevents[0] == g_nevents[1] && !memcmp(g_events[0], g_events[1], 4 * (g_nevents[0] < 256 ? g_nevents[0] : 256)); }
void begin_side(int side)
{
    g_side = side;
    g_nevents[side] = 0;
}
unsigned text_hash(const char *s)
{
    if (!s)
        return 0;
    unsigned h = 2166136261u;
    for (; *s; s++)
        h = (h ^ (BYTE)*s) * 16777619u;
    return h;
}

// ---- The dynamic property list ------------------------------------------------------------------------------

struct FakeDesc {
    DWORD vtable;
    DWORD id;
};
DWORD g_desc_vtable[40];
FakeDesc g_descs[8];
DWORD g_value_seed;
DWORD THISCALL desc_init(FakeDesc *d, DWORD v)
{
    ev(0x60000 + d->id * 0x100);
    ev(v);
    return 0x6006;
}
DWORD THISCALL desc_free1(FakeDesc *d, DWORD v)
{
    ev(0x80000 + d->id * 0x100);
    ev(v);
    return 0x8008;
}
DWORD THISCALL desc_write(FakeDesc *d, DWORD v)
{
    ev(0x200000 + d->id * 0x100);
    ev(v);
    return 0x2020;
}
// slot 35 (0, -1): a new value (a small number standing for a block, 0 one time in five)
DWORD THISCALL desc_new(FakeDesc *d, DWORD a, DWORD b)
{
    ev(0x230000 + d->id * 0x100);
    ev(a ^ (b << 8));
    g_value_seed = g_value_seed * 1103515245u + 12345u;
    return (g_value_seed >> 16) % 5 == 0 ? 0 : 0x10000 + (g_value_seed >> 16);
}
DWORD THISCALL desc_free2(FakeDesc *d, DWORD v)
{
    ev(0x240000 + d->id * 0x100);
    ev(v);
    return 0x2424;
}

struct DynList {
    DWORD vtable;
    DWORD *data;
    DWORD count, room;
};
DynList g_lists[2];

unsigned list_state(const DynList &l)
{
    unsigned h = fnv(2166136261u, &l.count, 8);
    for (DWORD i = 0; i < l.count && l.data; i++) {
        const DWORD e[2] = {l.data[i * 2] ? ((FakeDesc *)(uintptr_t)l.data[i * 2])->id : 0xffu, l.data[i * 2 + 1]};
        h = fnv(h, e, 8);
    }
    return h;
}

void test_dynamic(Tally &t)
{
    g_desc_vtable[6] = pv((void *)&desc_init);
    g_desc_vtable[8] = pv((void *)&desc_free1);
    g_desc_vtable[32] = pv((void *)&desc_write);
    g_desc_vtable[35] = pv((void *)&desc_new);
    g_desc_vtable[36] = pv((void *)&desc_free2);
    for (DWORD i = 0; i < 8; i++)
        g_descs[i] = {pv(g_desc_vtable), i};
    for (unsigned seed = 1; seed <= (unsigned)scaled(150); seed++) {
        unsigned states[2][48];
        DWORD results[2][48];
        int steps = 0;
        for (int side = 0; side < 2; side++) {
            begin_side(side);
            g_seed = seed;
            g_value_seed = seed;
            DynList &l = g_lists[side];
            l = {0x5f0240, nullptr, 0, 0};
            // a list with some entries to start with (room from the pool, like the exe's)
            const DWORD n0 = random() % 4;
            if (n0) {
                l.room = n0 + random() % 3;
                l.data = static_cast<DWORD *>(pool_get_alloc(l.room * 8));
                for (DWORD i = 0; i < n0; i++) {
                    l.data[i * 2] = pv(&g_descs[random() % 8]);
                    l.data[i * 2 + 1] = random() % 4 ? 0x700 + i : 0;
                }
                l.count = n0;
            }
            steps = 0;
            for (int k = 0; k < 40; k++) {
                const DWORD op = random() % 9;
                FakeDesc *d = &g_descs[random() % 8];
                void *object = &g_lists[side];   // unused by the functions
                DWORD r = 0;
                if (side == 0)
                    out_();
                if (op < 2)
                    r = at<DWORD(THISCALL *)(void *, void *)>(0x482940)(&l, d);
                else if (op < 4)
                    r = at<DWORD(THISCALL *)(void *, void *, void *)>(0x482970)(&l, object, d);
                else if (op < 6)
                    r = at<DWORD(THISCALL *)(void *, void *, void *)>(0x482a70)(&l, object, d);
                else if (op < 8)
                    r = at<DWORD(THISCALL *)(void *, void *)>(0x482b80)(&l, d) & 0xff;
                else
                    at<void(THISCALL *)(void *)>(0x482db0)(&l);
                if (side == 0)
                    in_();
                states[side][steps] = list_state(l) ^ (unsigned)g_nevents[side] * 0x9e3779b9u;
                results[side][steps] = r;
                steps++;
            }
            if (l.data)
                pool_get_free(l.data, l.room * 8);
        }
        int first = -1;
        for (int k = 0; k < steps && first < 0; k++)
            if (states[0][k] != states[1][k] || results[0][k] != results[1][k])
                first = k;
        char d[120];
        wsprintfA(d, "[dynamic list seed %u: first different step %d, %d / %d events]", seed, first, g_nevents[0], g_nevents[1]);
        t.check(first < 0 && same_events(), "DynamicProperties", d);
    }
}

// ---- The point descriptor -----------------------------------------------------------------------------------

alignas(16) BYTE g_value_block[2][0x40];
DWORD g_get_answer;   // 0: none, otherwise the offset into the side's value block + 1
DWORD THISCALL fake_get_dynamic(void *object, void *desc)
{
    ev(0x417320);
    ev(pv(object) == pv(g_value_block[g_side]) + 0x30 ? 1 : 2);
    (void)desc;
    return g_get_answer ? pv(g_value_block[g_side]) + g_get_answer - 1 : 0;
}
DWORD THISCALL fake_get_or_add(void *object, void *desc)
{
    ev(0x417340);
    (void)object;
    (void)desc;
    return pv(g_value_block[g_side]) + 4;
}
DWORD THISCALL fake_setter(DWORD *value, const DWORD *point)
{
    ev(0x5e77e4);
    ev(pv(value) - pv(g_value_block[g_side]));
    ev(point[0]);
    ev(point[1]);
    return 0x5e7;
}

void test_point(Tally &t)
{
    for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++) {
        alignas(16) BYTE desc[2][0x40];
        DWORD r[2][2];
        for (int side = 0; side < 2; side++) {
            begin_side(side);
            g_seed = seed;
            for (DWORD i = 0; i < 0x40; i += 4)
                fl<DWORD>(desc[side], i) = random();
            for (DWORD i = 0; i < 0x40; i += 4)
                fl<DWORD>(g_value_block[side], i) = random();
            fl<DWORD>(desc[side], 0x10) = random() % 0x20;
            fl<DWORD>(desc[side], 0x30) = random() % 2 ? pv((void *)&fake_setter) : 0;
            g_get_answer = random() % 3 ? 1 + random() % 8 : 0;
            const DWORD point[2] = {random(), random()};
            CodePatch a, b;
            patch_jump(a, 0x417320, (void *)&fake_get_dynamic);
            patch_jump(b, 0x417340, (void *)&fake_get_or_add);
            if (side == 0)
                out_();
            const DWORD v = at<DWORD(THISCALL *)(void *, void *)>(0x4a8080)(desc[side], g_value_block[side] + 0x30);
            r[side][1] = at<DWORD(THISCALL *)(void *, void *, const DWORD *)>(0x4a80b0)(desc[side], g_value_block[side] + 0x30, point);
            if (side == 0)
                in_();
            unpatch(b);
            unpatch(a);
            // pointers into the side's descriptor or value block by their offset
            r[side][0] = v >= pv(desc[side]) && v < pv(desc[side]) + 0x40       ? 0xd0000000u + (v - pv(desc[side]))
                         : v >= pv(g_value_block[side]) && v < pv(g_value_block[side]) + 0x40 ? 0xb0000000u + (v - pv(g_value_block[side]))
                                                                                                 : v;
        }
        char d[120];
        wsprintfA(d, "[point descriptor seed %u: %x %x / %x %x]", seed, r[0][0], r[0][1], r[1][0], r[1][1]);
        t.check(!memcmp(r[0], r[1], sizeof r[0]) && !memcmp(desc[0], desc[1], 0x40) && !memcmp(g_value_block[0], g_value_block[1], 0x40) &&
                    same_events(),
                "CPointFieldDesc", d);
    }
    // the value array: a heap block of count points (compared by being there; written and freed)
    const DWORD counts[] = {0, 1, 2, 7, 100};
    for (DWORD n : counts) {
        DWORD there[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                out_();
            void *p = at<void *(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4a81c0)(nullptr, n, 1, 2);
            if (side == 0)
                in_();
            there[side] = p ? 1 : 0;
            if (p) {
                memset(p, 0x5a, n * 8);
                heap_free(p);
            }
        }
        t.check(there[0] == there[1], "CPointFieldDesc::NewValues", "");
    }
}

// ---- The row table ------------------------------------------------------------------------------------------

void test_row_table(Tally &t)
{
    for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++) {
        unsigned h[2];
        DWORD after[2];
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            const DWORD columns = random() % 5 ? random() % 300 : random(), rows = random() % 6 ? random() % 40 : random() % 3;
            if (side == 0)
                out_();
            DWORD *table = at<DWORD *(__fastcall *)(DWORD, DWORD, DWORD)>(0x4a8440)(columns, rows, random());
            const DWORD n = rows < 2 ? 2 : rows;
            h[side] = fnv(2166136261u, table, n * 4);
            DWORD *slot = table;
            at<void(THISCALL *)(DWORD **)>(0x4a8480)(&slot);
            after[side] = pv(slot);
            DWORD *none = nullptr;
            at<void(THISCALL *)(DWORD **)>(0x4a8480)(&none);
            after[side] += pv(none);
            if (side == 0)
                in_();
        }
        t.check(h[0] == h[1] && after[0] == 0 && after[1] == 0, "RowTable", "");
    }
}

// ---- Win32 ------------------------------------------------------------------------------------------------------

char g_module_path[0x110];
DWORD g_api_answer;
DWORD WINAPI s_get_module_file_name(void *m, char *buf, DWORD n)
{
    ev(0xa0e0);
    ev(pv(m));
    ev(n);
    DWORD k = 0;
    for (; g_module_path[k] && k + 1 < n; k++)
        buf[k] = g_module_path[k];
    if (n)
        buf[k] = 0;
    return g_module_path[0] ? k : 0;
}
DWORD WINAPI s_set_current_directory(const char *dir)
{
    ev(0xa0e4);
    ev(text_hash(dir));
    return g_api_answer;
}
DWORD WINAPI s_post_message(DWORD w, DWORD m, DWORD a, DWORD b)
{
    ev(0xa25c);
    ev(w);
    ev(m);
    ev(a ^ b << 8);
    return g_api_answer;
}
DWORD WINAPI s_load_cursor(DWORD i, DWORD name)
{
    ev(0xa204);
    ev(i);
    ev(name);
    return 0xc0c0;
}
DWORD WINAPI s_set_cursor(DWORD c)
{
    ev(0xa24c);
    ev(c);
    return 0x5c5c + c;
}
DWORD WINAPI s_desktop() { return ev(0xa250), 0xde5c; }
DWORD WINAPI s_foreground(DWORD w)
{
    ev(0xa254);
    ev(w);
    return 1;
}
DWORD WINAPI s_message_box(DWORD w, const char *text, const char *caption, DWORD type)
{
    ev(0xa258);
    ev(w);
    ev(text_hash(text));
    ev(text_hash(caption));
    ev(type);
    return g_api_answer;
}
DWORD WINAPI s_create_process(const char *app, char *line, void *a, void *b, DWORD inherit, DWORD flags, void *env, const char *dir,
                              const BYTE *si, void *pi)
{
    ev(0xa0f0);
    ev(pv(app) ^ pv(a) ^ pv(b) ^ inherit ^ flags ^ pv(env) ^ pv(dir));
    ev(text_hash(line));
    ev(fnv(2166136261u, si, 0x44));
    (void)pi;
    return g_api_answer;
}
DWORD WINAPI s_tick_count()
{
    ev(0xa0e8);
    return g_api_answer;
}

struct Import {
    DWORD slot;
    void *fn;
};
const Import IMPORTS[] = {{0x5ea0e0, (void *)&s_get_module_file_name}, {0x5ea0e4, (void *)&s_set_current_directory},
                          {0x5ea25c, (void *)&s_post_message},         {0x5ea204, (void *)&s_load_cursor},
                          {0x5ea24c, (void *)&s_set_cursor},           {0x5ea250, (void *)&s_desktop},
                          {0x5ea254, (void *)&s_foreground},           {0x5ea258, (void *)&s_message_box},
                          {0x5ea0f0, (void *)&s_create_process},       {0x5ea0e8, (void *)&s_tick_count}};
constexpr int NIMPORTS = sizeof IMPORTS / sizeof IMPORTS[0];

void test_win32(Tally &t)
{
    const char *const paths[] = {"C:\\Games\\Zax\\Zax.exe", "D:/Zax/Zax.exe", "Zax.exe", "", "C:\\a/b\\c", "\\"};
    const char *const texts[] = {"Error", "A rather long text of an error that the game shows in its box", "", "%s"};
    const DWORD saved_window = *at<DWORD *>(0x713e18);
    for (unsigned seed = 1; seed <= (unsigned)scaled(48); seed++) {
        DWORD r[2][7];
        for (int side = 0; side < 2; side++) {
            begin_side(side);
            g_seed = seed;
            const char *p = paths[random() % 6];
            DWORD k = 0;
            for (; p[k] && k + 1 < sizeof g_module_path; k++)
                g_module_path[k] = p[k];
            g_module_path[k] = 0;
            g_api_answer = random() % 3 ? random() : 0;
            *at<DWORD *>(0x713e18) = random();
            const char *caption = texts[random() % 4], *text = texts[random() % 4];
            const char *argument = random() % 3 ? texts[random() % 4] : nullptr;
            const DWORD show = random() % 3;
            ImportPatch ip[NIMPORTS];
            for (int i = 0; i < NIMPORTS; i++)
                patch_import(ip[i], IMPORTS[i].slot, IMPORTS[i].fn);
            if (side == 0)
                out_();
            r[side][0] = at<DWORD(__cdecl *)()>(0x4d2310)();
            r[side][1] = at<DWORD(__cdecl *)()>(0x4d2470)();
            r[side][2] = at<DWORD(__fastcall *)(const char *, const char *)>(0x4d24c0)(caption, text);
            r[side][3] = at<DWORD(__fastcall *)(const char *, const char *)>(0x4d2510)(caption, text);
            r[side][4] = at<DWORD(__fastcall *)(const char *, const char *, DWORD)>(0x4d2570)(p, argument, show);
            r[side][5] = at<DWORD(__cdecl *)()>(0x4d2650)();
            if (side == 0)
                in_();
            for (int i = NIMPORTS; i--;)
                unpatch_import(ip[i]);
            r[side][6] = 0;
        }
        char d[160];
        wsprintfA(d, "[win32 seed %u: %x %x %x %x %x %x / %x %x %x %x %x %x, %d / %d events]", seed, r[0][0], r[0][1], r[0][2], r[0][3], r[0][4],
                  r[0][5], r[1][0], r[1][1], r[1][2], r[1][3], r[1][4], r[1][5], g_nevents[0], g_nevents[1]);
        t.check(!memcmp(r[0], r[1], sizeof r[0]) && same_events(), "Win32 wrappers", d);
    }
    *at<DWORD *>(0x713e18) = saved_window;
}

// ---- The server options ------------------------------------------------------------------------------------------

const char *g_ini_name;
DWORD THISCALL s_ini_get_text(void *ini, const char *section, const char *key, ZString *value)
{
    ev(0x46ec70);
    ev(pv(ini));
    ev(text_hash(section) ^ text_hash(key));
    ev(text_hash(value->c_str()));
    if (g_ini_name)
        at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(value, g_ini_name);
    return 0x7e47;
}
DWORD __fastcall s_ini_write(void *ini, const char *section, const char *key, const char *text)
{
    ev(0x4a47c0);
    ev(pv(ini));
    ev(text_hash(section) ^ text_hash(key));
    ev(text_hash(text));
    return 0x47c0;
}

void test_server_options(Tally &t)
{
    const char *const names[] = {nullptr, "My Server", "", "A server with a name longer than the small buffers of the strings, to be sure"};
    for (unsigned seed = 1; seed <= (unsigned)scaled(24); seed++) {
        alignas(16) BYTE o[2][0x20];
        DWORD r[2][2];
        unsigned text[2];
        for (int side = 0; side < 2; side++) {
            begin_side(side);
            g_seed = seed;
            for (DWORD i = 0; i < 0x20; i += 4)
                fl<DWORD>(o[side], i) = random();
            at<void *(THISCALL *)(void *)>(0x4dec90)(o[side]);   // the name: a fresh ZString
            g_ini_name = names[random() % 4];
            CodePatch a, b;
            patch_jump(a, 0x46ec70, (void *)&s_ini_get_text);
            patch_jump(b, 0x4a47c0, (void *)&s_ini_write);
            if (side == 0)
                out_();
            r[side][0] = at<DWORD(THISCALL *)(void *)>(0x4d2c50)(o[side]);
            r[side][1] = at<DWORD(THISCALL *)(void *)>(0x4d2ca0)(o[side]);
            if (side == 0)
                in_();
            unpatch(b);
            unpatch(a);
            text[side] = text_hash(reinterpret_cast<ZString *>(o[side])->c_str());
            at<void(THISCALL *)(void *)>(0x4defd0)(o[side]);
            fl<DWORD>(o[side], 0) = 0;
        }
        char d[120];
        wsprintfA(d, "[server options seed %u: %x %x / %x %x, name %x / %x]", seed, r[0][0], r[0][1], r[1][0], r[1][1], text[0], text[1]);
        t.check(!memcmp(r[0], r[1], sizeof r[0]) && text[0] == text[1] && !memcmp(o[0], o[1], 0x20) && same_events(), "ServerOptions", d);
    }
}

}  // namespace

ZAX_CHECKS(core_rest_b)
{
    (void)cls;
    (void)n;
    invcheck::g_hook_table = hooks_core_rest_b;
    if (!invcheck::arena_setup()) {
        log("Check core_rest_b: MISMATCH (no arena)");
        return;
    }
    Tally dyn{"core_rest_b: dynamic property list"}, point{"core_rest_b: point descriptor, row table"}, win{"core_rest_b: Win32 wrappers"},
        server{"core_rest_b: server options"};
    test_dynamic(dyn);
    dyn.report();
    test_point(point);
    test_row_table(point);
    point.report();
    test_win32(win);
    win.report();
    test_server_options(server);
    server.report();
}

}  // namespace zax
