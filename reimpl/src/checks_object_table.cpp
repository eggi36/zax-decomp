// Checks of the object table and the object stream's own methods (object_table.cpp) against the
// original, in the game thread (see checks_object_common.h for how a case runs). The table is put into
// random states (the four lists with random entries and spare capacity, location, flags, format,
// mode) from a seed; the streams hold texts and binary lists in many variants. Compared: the bytes
// in the stream and its position, the writer, every member of the table and the entries of its
// lists, the fields the references point to, the event log (error reporter) and the return value.
#include "checks_object_common.h"
#include "ptr_array.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);

namespace zax {

namespace {
using namespace objcheck;

constexpr int NOBJ = 12;
FakeObj g_objs[NOBJ];
CClassInfo g_class;
void *g_area[32];   // the fields Resolve writes
const DWORD IDS[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x12345678, 0xfffffff0};

void reset()
{
    for (int i = 0; i < 32; i++)
        g_area[i] = reinterpret_cast<void *>(static_cast<uintptr_t>(0xa0000000u + i));
}
void capture_area(CObjectWriter *, State &s) { s.extra[0] = fnv(2166136261u, g_area, sizeof g_area); }

void *obj(DWORD i) { return &g_objs[i % NOBJ]; }
void *id_ptr(DWORD id) { return reinterpret_cast<void *>(static_cast<uintptr_t>(id)); }

template <typename T> void fill(ZTableList<T> &l, DWORD n, DWORD spare, T (*gen)(DWORD i))
{
    if (l.data)
        heap_free(l.data);
    l.capacity = n + spare;
    l.count = n;
    l.data = l.capacity ? static_cast<T *>(heap_alloc(l.capacity * sizeof(T))) : nullptr;
    for (DWORD i = 0; i < n; i++)
        l.data[i] = gen(i);
}
ObjectIdEntry gen_loaded(DWORD) { return {IDS[random() % 8], random() % 6 ? obj(random()) : nullptr}; }
ObjectFixup gen_fixup(DWORD)
{
    ObjectFixup f;
    memset(&f, 0, sizeof f);
    f.id = IDS[random() % 8];
    f.field = &g_area[random() % 32];
    f.resolved = random() % 4 == 0;
    return f;
}
void *gen_object(DWORD) { return obj(random()); }
void *gen_mixed(DWORD) { return random() & 1 ? obj(random()) : id_ptr(IDS[random() % 8]); }

void random_table(ObjectTable *t)
{
    t->location = random() % 3 ? 0xffffffff : random() % 40;
    t->list_read = random() & 1;
    t->format = random() % 3;
    fill(t->loaded, random() % 6, random() % 3, gen_loaded);
    fill(t->fixups, random() % 4, random() % 3, gen_fixup);
    fill(t->referenced, random() % 6, random() % 3, gen_mixed);
    fill(t->written, random() % 6, random() % 3, gen_object);
}

// ---- Case parameters set by the loops, read by the setups and calls -------------------------------

DWORD g_location, g_format, g_mode, g_flags, g_open, g_arg;
int g_variant;
const char *g_text;
Bytes g_input;

void setup_random(CObjectWriter *w) { random_table(table_of(w)); }
// Attach and FileBuf_Open close the old stream first through slot 1 (the stream's finish): the table
// closed, so that finish does not write a list into the random state
void setup_random_closed(CObjectWriter *w)
{
    random_table(table_of(w));
    table_of(w)->open = 0;
}
void setup_located(CObjectWriter *w)
{
    ObjectTable *t = table_of(w);
    random_table(t);
    t->location = g_location;
    t->format = g_format;
    t->mode = g_mode;
}

// ---- Table constructor -------------------------------------------------------------------------------

void test_ctor(Tally &t, int &n)
{
    alignas(4) static BYTE buf[2][0x5c];
    const BYTE fills[] = {0, 0xff, 0xcd, 0x5a};
    for (BYTE f : fills) {
        DWORD ret[2];
        for (int s = 0; s < 2; s++) {
            memset(buf[s], f, sizeof buf[s]);
            if (s == 0)
                hooks_out();
            ret[s] = ptrv(fn<ObjectTable *(THISCALL *)(void *)>(0x55c6f0)(buf[s])) - ptrv(buf[s]);
            if (s == 0)
                hooks_in();
        }
        char d[80];
        wsprintfA(d, "[0x55c6f0 fill %x: return %x/%x]", f, ret[0], ret[1]);
        t.check(ret[0] == ret[1] && memcmp(buf[0], buf[1], sizeof buf[0]) == 0, "", d);
        n++;
    }
}

// ---- Open, the location writers, AddReference, AddWritten, Resolve, Register -----------------------

void test_simple(Tally &t, int &n)
{
    for (unsigned seed = 1; seed <= 40; seed++) {
        run(t, {"ObjectTable::Open", 0x55c750, 0, 0, nullptr, 0, setup_random,
                [](DWORD a, CObjectWriter *w) {
                    void *s = random() & 1 ? static_cast<void *>(w) : obj(random());
                    const DWORD flags = random() & 0xffff, mode = random() % 3;
                    return fn<DWORD(THISCALL *)(ObjectTable *, void *, DWORD, DWORD)>(a)(table_of(w), s, flags, mode);
                },
                nullptr, 0xffffffff, seed});
        n++;
    }
    // the location writers: on a stream with text before, at a random position
    for (DWORD address : {0x55c780u, 0x55c7d0u})
        for (DWORD loc : {0xffffffffu, 3u})
            for (unsigned seed = 1; seed <= 12; seed++) {
                g_input.size = 0;
                append_text(g_input, "Some text before\r\n\tand more\r\n");
                for (DWORD i = 0; i < seed % 4; i++)
                    append_text(g_input, "x");
                g_location = loc;
                g_format = seed % 3;
                g_mode = seed % 2;
                run(t, {address == 0x55c780 ? "ObjectTable::WriteLocation" : "ObjectTable::WriteLocationBinary", address, 2, 0,
                        &g_input, seed * 3 % (g_input.size + 1), setup_located,
                        [](DWORD a, CObjectWriter *w) { return fn<DWORD(THISCALL *)(ObjectTable *)>(a)(table_of(w)); }, nullptr,
                        loc == 0xffffffff ? 0xffffffff : 0, seed});
                n++;
            }
    // AddReference: null, an object in the list, a new one; modes 0, 1, 2
    for (DWORD mode = 0; mode < 3; mode++)
        for (int kind = 0; kind < 3; kind++)
            for (unsigned seed = 1; seed <= 10; seed++) {
                g_mode = mode;
                g_variant = kind;
                run(t, {"ObjectTable::AddReference", 0x55cad0, 0, 0, nullptr, 0,
                        [](CObjectWriter *w) {
                            random_table(table_of(w));
                            table_of(w)->mode = g_mode;
                        },
                        [](DWORD a, CObjectWriter *w) {
                            ObjectTable *t = table_of(w);
                            void *o = g_variant == 0 ? nullptr
                                      : g_variant == 1 && t->referenced.count ? t->referenced.data[random() % t->referenced.count]
                                                                               : obj(random());
                            return fn<DWORD(THISCALL *)(ObjectTable *, void *)>(a)(t, o);
                        },
                        nullptr, kind == 0 || mode == 1 ? 0 : 0xffffffff, seed});
                n++;
            }
    for (unsigned seed = 1; seed <= 20; seed++) {
        run(t, {"ObjectTable::AddWritten", 0x55cb30, 0, 0, nullptr, 0, setup_random,
                [](DWORD a, CObjectWriter *w) { return fn<DWORD(THISCALL *)(ObjectTable *, void *)>(a)(table_of(w), obj(random())); },
                nullptr, 0xffffffff, seed});
        n++;
    }
    // Resolve: ID 0, a loaded ID (with an object or null), an unknown one; modes 0, 1, 2
    for (DWORD mode = 0; mode < 3; mode++)
        for (int kind = 0; kind < 4; kind++)
            for (unsigned seed = 1; seed <= 12; seed++) {
                g_mode = mode;
                g_variant = kind;
                run(t, {"ObjectTable::Resolve", 0x55cb50, 0, 0, nullptr, 0,
                        [](CObjectWriter *w) {
                            random_table(table_of(w));
                            table_of(w)->mode = g_mode;
                        },
                        [](DWORD a, CObjectWriter *w) {
                            ObjectTable *t = table_of(w);
                            DWORD id = 0;
                            if (g_variant == 1 && t->loaded.count)
                                id = t->loaded.data[random() % t->loaded.count].id;
                            else if (g_variant >= 2)
                                id = g_variant == 2 ? IDS[random() % 8] : 0x7777;
                            const DWORD r = fn<DWORD(THISCALL *)(ObjectTable *, DWORD, const char *, void **)>(a)(t, id, "CSomeClass",
                                                                                                              &g_area[random() % 32]);
                            return relative_fixup(t, r);
                        },
                        capture_area, kind != 0 && mode == 1 ? 0 : 0xffffffff, seed});
                n++;
            }
    // Register: modes, with and without the list read, IDs in the referenced list or not
    for (DWORD mode = 0; mode < 3; mode++)
        for (DWORD read = 0; read < 2; read++)
            for (unsigned seed = 1; seed <= 12; seed++) {
                g_mode = mode;
                g_arg = read;
                run(t, {"ObjectTable::Register", 0x55cc90, 0, 0, nullptr, 0,
                        [](CObjectWriter *w) {
                            ObjectTable *t = table_of(w);
                            random_table(t);
                            t->mode = g_mode;
                            t->list_read = (BYTE)g_arg;
                        },
                        [](DWORD a, CObjectWriter *w) {
                            ObjectTable *t = table_of(w);
                            const DWORD id = random() & 1 && t->referenced.count ? ptrv(t->referenced.data[random() % t->referenced.count])
                                                                                 : IDS[random() % 8];
                            return fn<DWORD(THISCALL *)(ObjectTable *, DWORD, void *)>(a)(t, id, obj(random()));
                        },
                        nullptr, mode == 1 ? 0 : 0xffffffff, seed});
                n++;
            }
}

// ---- Reading the location ------------------------------------------------------------------------------

// Text variants of the list at a position behind some filler
const char *const LISTS[] = {
    "Referenced Objects\r\n{\r\n\t3\r\n\t0x00000010\r\n\t0x00000020\r\n\t0x00000030\r\n}\r\n",
    "referenced OBJECTS\r\n{\r\n\t1\r\n\t0x12345678\r\n}\r\n",
    "\t  Referenced Objects\r\n  {\r\n 2\r\n\t\t0x40\r\n  0x50\r\n\t}\r\nAfter=1\r\n",
    "Referenced Object\r\n{\r\n\t1\r\n\t0x10\r\n}\r\n",
    "Referenced Objects\r\n{\r\n\t0\r\n}\r\n",
    "Referenced Objects\r\n{\r\n\t9\r\n\t0x10\r\n\t0x20\r\n",
    "Referenced Objects\r\n{\r\n\t2\r\n\t0x10\r\n\t0x20\r\n\t0x30\r\n\t0x40\r\n\t0x50\r\n}\r\n",
    "Referenced Objects\r\n{\r\n\t5\r\n\t16\r\n\t020\r\n\tabc\r\n\t-1\r\n\t0xffffffff\r\n}\r\n",
    "Referenced Objects\r\n",
    "Referenced Objects\r\n{\r\n2\r\n0x10\r\n }\r\n0x20\r\n}\r\n",
    "Referenced Objects\n{\n\t1\n\t0x10\n}\n",
    ("Referenced Objects\r\n{\r\n\t1\r\n\t0x10xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n}\r\n"),
    "",
    "Referenced Objects\r\n{\r\n\t3\r\n\t\r\n\t0x10\r\n\r\n}\r\n",
};
char g_value[64];
constexpr DWORD LOCATION_FILL = 16;
DWORD g_location_fill[LOCATION_FILL];   // the stack below ReadLocationBinary's call (the cut-short cases)

void test_read_location(Tally &t, int &n)
{
    const int nlists = sizeof LISTS / sizeof LISTS[0];
    for (int v = 0; v < nlists; v++)
        for (int arg = 0; arg < 5; arg++) {
            // a list position at or beyond the end leaves the line buffer uninitialised (in both)
            if (arg == 3 || !LISTS[v][0])
                continue;
            for (DWORD loc : {0xffffffffu, 5u}) {
                g_input.size = 0;
                append_text(g_input, "Header=1\r\n\tReferenced Object List Location=0x00420042\r\nField=2\r\n");
                const DWORD at_ = g_input.size;
                append_text(g_input, LISTS[v]);
                const DWORD values[] = {at_, at_, at_ + 1, g_input.size + 10, 0};
                if (arg == 1)
                    wsprintfA(g_value, "%u", values[arg]);
                else if (arg == 4)
                    lstrcpyA(g_value, "junk");
                else
                    wsprintfA(g_value, "0x%08X", values[arg]);
                g_location = loc;
                g_format = 2;
                g_mode = 0;
                run(t, {"ObjectTable::ReadLocation", 0x55c800, 4, 0, &g_input, 12, setup_located,
                        [](DWORD a, CObjectWriter *w) { return fn<DWORD(THISCALL *)(ObjectTable *, const char *)>(a)(table_of(w), g_value); },
                        nullptr, loc == 0xffffffff ? 0xffffffff : 0, (unsigned)(v * 17 + arg)});
                n++;
            }
        }
    // binary: a dword with the position, there magic, count and the IDs
    for (DWORD count = 0; count < 9; count++)
        for (int gap = 0; gap < 3; gap++)
            for (DWORD loc : {0xffffffffu, 2u}) {
                g_input.size = 0;
                append_dword(g_input, 0x41d80000);
                const DWORD p = g_input.size;
                append_dword(g_input, 0);   // position, filled below
                for (int i = 0; i < gap * 3; i++)
                    append_dword(g_input, 0xbeef0000 + i);
                const DWORD list = g_input.size;
                memcpy(g_input.data + p, &list, 4);
                append_dword(g_input, gap == 2 ? 0x11111111 : LIST_MAGIC);
                append_dword(g_input, count);
                for (DWORD i = 0; i < count; i++)
                    append_dword(g_input, IDS[i % 8] + i);
                g_location = loc;
                g_format = 0;
                g_mode = 0;
                run(t, {"ObjectTable::ReadLocationBinary", 0x55c9b0, 4, 0, &g_input, p, setup_located,
                        [](DWORD a, CObjectWriter *w) { return fn<DWORD(THISCALL *)(ObjectTable *)>(a)(table_of(w)); }, nullptr,
                        loc == 0xffffffff ? 0xffffffff : 0, count * 7 + gap});
                n++;
            }
    // binary, cut short, in the exe's frame (batch decomp-faithful2): called through zax_call_stack with a fill of the
    // case on both sides, so the dwords the stream does not hold are the fill's at the exe's addresses (after its
    // `sub esp, 0x14`: the count at [esp + 0] = fill[5], the position [esp + 4] = fill[4], the ID [esp + 8] = fill[3]),
    // the same on both sides; the count's dword set to 0-3 (the list gets that many), the position's to the
    // stream's end or 0 (the exe's memory stream must not be read past its end: the test's own limit). With the
    // location known the exe returns the caller's eax, here the fill's last dword. The cuts: after the header (no
    // position), after the position (the list at the stream's end), after the magic (no count), after the count
    // (no IDs), after one ID.
    for (int cut = 0; cut < 5; cut++)
        for (int k = 0; k < 6; k++) {
            g_seed = 0x55c9b0u + (DWORD)(cut * 16 + k);
            stack_pattern(g_location_fill, LOCATION_FILL, random());
            g_location_fill[fill_index(0, 0x14, 0)] = random() % 4;
            g_input.size = 0;
            append_dword(g_input, 0x41d80000);
            const DWORD p = g_input.size;
            if (cut >= 1)
                append_dword(g_input, p + 4);   // the list right after it (cut 1: the stream ends there)
            if (cut >= 2)
                append_dword(g_input, LIST_MAGIC);
            if (cut >= 3)
                append_dword(g_input, 2 + (DWORD)k % 2);
            if (cut >= 4)
                append_dword(g_input, IDS[k % 8]);
            g_location_fill[fill_index(0, 0x14, 4)] = k % 3 == 0 ? 0 : g_input.size;
            g_location = k == 5 ? 2u : 0xffffffffu;
            g_format = 0;
            g_mode = 0;
            run(t, {"ObjectTable::ReadLocationBinary (cut short, stack pattern)", 0x55c9b0, 4, 0, &g_input, p, setup_located,
                    [](DWORD a, CObjectWriter *w) {
                        return zax_call_stack(at<void *>(a), ptrv(table_of(w)), nullptr, 0, g_location_fill, LOCATION_FILL);
                    },
                    nullptr, 0xffffffff, (unsigned)(cut * 16 + k)});
            n++;
        }
}

// ---- Finish and the list ---------------------------------------------------------------------------------

// A stream that starts with the placeholder (text or binary) and some written content; the location
// points at it
void build_written(DWORD format)
{
    g_input.size = 0;
    if (format == 1) {
        append_dword(g_input, 0x41d80000);
        append_dword(g_input, LOCATION_BINARY_PLACEHOLDER);
        append_dword(g_input, 0x1234);
    } else {
        append_text(g_input, "CSomething\r\n{\r\n\tReferenced Object List Location=0x00420042\r\n\tObject ID=0x00000010\r\n}\r\n");
    }
}
void setup_finish(CObjectWriter *w)
{
    ObjectTable *t = table_of(w);
    random_table(t);
    t->location = g_location;
    t->format = g_format;
    t->mode = g_mode;
    t->flags = g_flags;
    t->open = (BYTE)g_open;
    // some referenced objects written, some not
    if (g_variant == 1 && t->referenced.count)
        for (DWORD i = 0; i < t->written.count; i++)
            t->written.data[i] = t->referenced.data[i % t->referenced.count];
    if (g_variant == 2) {
        fill(t->written, 0, 0, gen_object);
        fill(t->referenced, 0, 1, gen_object);
    }
    // fixups whose IDs are loaded
    if (g_variant == 3)
        for (DWORD i = 0; i < t->fixups.count && t->loaded.count; i++)
            t->fixups.data[i].id = t->loaded.data[(i + 1) % t->loaded.count].id;
}

void test_finish(Tally &t, int &n)
{
    for (DWORD address : {0x55cd90u, 0x55ce50u})
        for (DWORD format = 0; format < 3; format++)
            for (DWORD mode = 0; mode < 3; mode++)
                for (DWORD flags : {0u, 4u, 6u})
                    for (int variant = 0; variant < 4; variant++)
                        for (DWORD open = 0; open < 2; open++) {
                            if (address == 0x55ce50 && (open == 0 || flags))
                                continue;
                            build_written(format);
                            g_location = format == 2 ? 0xffffffff : format == 1 ? 4 : 16;
                            g_format = format;
                            g_mode = mode;
                            g_flags = flags;
                            g_open = open;
                            g_variant = variant;
                            run(t, {address == 0x55cd90 ? "ObjectTable::Finish" : "ObjectTable::WriteList", address, 2, 0, &g_input,
                                    g_input.size, setup_finish,
                                    [](DWORD a, CObjectWriter *w) { return fn<DWORD(THISCALL *)(ObjectTable *)>(a)(table_of(w)); },
                                    capture_area,
                                    // not open: the caller's eax; the reading finish: operator delete's
                                    !open || (address == 0x55cd90 && mode == 0 && (flags & 4)) ? 0 : 0xffffffff, format * 1000 + mode * 100 + flags * 10 + variant * 2 + open});
                            n++;
                        }
}

// ---- The object stream ----------------------------------------------------------------------------------

DWORD *g_open_count() { return at<DWORD *>(FILES_OPEN); }
DWORD g_open_count_before;
void setup_dtor(CObjectWriter *w)
{
    setup_finish(w);
    w->finished = (BYTE)g_arg;
}
DWORD call_dtor(DWORD a, CObjectWriter *w) { return fn<DWORD(THISCALL *)(CObjectWriter *)>(a)(w); }
DWORD call_deleting(DWORD a, CObjectWriter *w) { return ptrv(fn<CObjectWriter *(THISCALL *)(CObjectWriter *, DWORD)>(a)(w, 0)) - ptrv(w); }
// The deleting destructor with flag 1 frees the stream itself: moved into a pool block first
void after_dtor(CObjectWriter *, State &s) { s.extra[1] = *g_open_count() - g_open_count_before; }
DWORD call_deleting_pool(DWORD a, CObjectWriter *w)
{
    CObjectWriter *p = static_cast<CObjectWriter *>(pool_alloc(sizeof(CObjectWriter)));
    memcpy(p, w, sizeof *p);
    table_of(p)->stream = p;
    return fn<CObjectWriter *(THISCALL *)(CObjectWriter *, DWORD)>(a)(p, 1) == p;
}


void test_stream(Tally &t, int &n, const char *dir)
{
    // constructor on a filled buffer: everything but the table pointer, the table's members
    for (BYTE f : {0, 0xff, 0x5a}) {
        alignas(4) static BYTE buf[2][0x24];
        BYTE table[2][0x5c];
        DWORD ret[2];
        for (int s = 0; s < 2; s++) {
            memset(buf[s], f, sizeof buf[s]);
            if (s == 0)
                hooks_out();
            ret[s] = ptrv(fn<CObjectWriter *(THISCALL *)(void *)>(0x5240b0)(buf[s])) - ptrv(buf[s]);
            if (s == 0)
                hooks_in();
            CObjectWriter *w = reinterpret_cast<CObjectWriter *>(buf[s]);
            memcpy(table[s], w->table, 0x5c);
        }
        const bool same_writer = memcmp(buf[0], buf[1], 0x1c) == 0 && memcmp(buf[0] + 0x20, buf[1] + 0x20, 4) == 0;
        // open, location, format and the four lists (the other members stay uninitialised)
        const bool same_table = table[0][0] == table[1][0] && memcmp(table[0] + 4, table[1] + 4, 4) == 0 &&
                                memcmp(table[0] + 0xc, table[1] + 0xc, 4) == 0 && memcmp(table[0] + 0x1c, table[1] + 0x1c, 0x40) == 0;
        for (int s = 0; s < 2; s++) {
            CObjectWriter *w = reinterpret_cast<CObjectWriter *>(buf[s]);
            w->finished = 1;
            at<DWORD(THISCALL *)(CObjectWriter *)>(0x5241e0)(w);
        }
        char d[80];
        wsprintfA(d, "[0x5240b0 fill %x: return %x/%x]", f, ret[0], ret[1]);
        t.check(ret[0] == ret[1] && same_writer && same_table, "", d);
        n++;
    }
    log("  stream: Open");
    // Open on a stream (flags, modes) and on files
    for (unsigned seed = 1; seed <= 12; seed++) {
        run(t, {"ObjectStream_Open", 0x5242f0, 0, 0, nullptr, 0, setup_random_closed,
                [](DWORD a, CObjectWriter *w) {
                    const DWORD flags = random() & 0x3ff, mode = random() % 3;
                    w->finished = (BYTE)(random() & 1);
                    return fn<DWORD(THISCALL *)(CObjectWriter *, ZStream *, DWORD, DWORD)>(a)(w, new_stream(nullptr, 0, 0), flags, mode);
                },
                nullptr, 0xffffffff, seed});
        n++;
    }
    log("  stream: OpenFile");
    static char path[MAX_PATH], missing[MAX_PATH];
    wsprintfA(path, "%sstream.txt", dir);
    wsprintfA(missing, "%smissing\\none.txt", dir);
    for (int v = 0; v < 6; v++) {
        g_variant = v;
        g_text = v < 2 ? missing : path;
        DeleteFileA(path);
        run(t, {"ObjectStream_OpenFile", 0x5242b0, 0, 0, nullptr, 0, setup_random_closed,
                [](DWORD a, CObjectWriter *w) {
                    const DWORD flags = g_variant == 0 ? 4 : g_variant == 1 ? 0x24 : g_variant == 2 ? 3 : g_variant == 3 ? 0x23 : 3;
                    const DWORD r = fn<DWORD(THISCALL *)(CObjectWriter *, const char *, DWORD, DWORD)>(a)(w, g_text, flags, g_variant % 3);
                    if (g_variant >= 4)   // write through it, then read it back as a check
                        at<DWORD(THISCALL *)(CFileBuf *, const char *)>(0x4a1a40)(w, "written line");
                    return r;
                },
                [](CObjectWriter *w, State &s) {
                    s.extra[1] = w->stream ? 1 : 0;
                    s.extra[2] = w->stream ? vcall<DWORD>(w->stream, STREAM_TELL) : 0;
                },
                v < 2 ? 0xff : 0xffffffff, (unsigned)v + 1});
        n++;
        DeleteFileA(path);
    }
    log("  stream: FinishTable, Finish");
    // FinishTable and Finish: finished or not, modes, formats
    for (DWORD address : {0x524320u, 0x524340u})
        for (DWORD finished = 0; finished < 2; finished++)
            for (DWORD format = 0; format < 3; format++)
                for (DWORD mode = 0; mode < 2; mode++) {
                    build_written(format);
                    g_location = format == 2 ? 0xffffffff : format == 1 ? 4 : 16;
                    g_format = format;
                    g_mode = mode;
                    g_flags = 2;
                    g_open = 1;
                    g_variant = (int)(format + mode) % 4;
                    g_arg = finished;
                    run(t, {address == 0x524320 ? "ObjectStream_FinishTable" : "ObjectStream_Finish", address, 2, 0, &g_input,
                            g_input.size,
                            [](CObjectWriter *w) {
                                setup_finish(w);
                                w->finished = (BYTE)g_arg;
                            },
                            [](DWORD a, CObjectWriter *w) { return fn<DWORD(THISCALL *)(CObjectWriter *)>(a)(w); },
                            [](CObjectWriter *, State &s) { s.extra[3] = (DWORD)g_closes[g_side]; },
                            address == 0x524320 && finished ? 0xff : 0xffffffff, format * 10 + mode + finished * 100, address == 0x524340});
                    n++;
                }
    log("  stream: destructors");
    // destructors: finished or not, formats, a table with entries; the deleting one also on a pool block
    for (DWORD address : {0x5241e0u, 0x5240f0u})
        for (DWORD flags : {0u, 1u})
            for (DWORD finished = 0; finished < 2; finished++)
                for (DWORD format = 0; format < 3; format++) {
                    if (address == 0x5241e0 && flags)
                        continue;
                    build_written(format);
                    g_location = format == 2 ? 0xffffffff : format == 1 ? 4 : 16;
                    g_format = format;
                    g_mode = format == 2 ? 1 : 0;
                    g_flags = 2;
                    g_open = 1;
                    g_variant = 1;
                    g_arg = finished;
                    g_open_count_before = *g_open_count();
                    run(t, {address == 0x5241e0 ? "ObjectStream_Dtor" : "ObjectStream_DeletingDtor", address, 2, 0, &g_input, g_input.size,
                            setup_dtor, address == 0x5241e0 ? call_dtor : flags ? call_deleting_pool : call_deleting, after_dtor, 0xffffffff,
                            format * 10 + finished + flags * 100, true});
                    n++;
                }
}

}  // namespace


ZAX_CHECKS(object_table)
{
    (void)cls;
    (void)n;
    Tally table{"object table: 0x55c6f0-0x55ce50"}, stream{"object stream: 0x5240b0-0x524340"};
    init_stream_vtable();
    init_fake_vtables();
    reporter_in();
    memset(&g_class, 0, sizeof g_class);
    g_class.name = "CFakeTableObject";
    for (int i = 0; i < NOBJ; i++) {
        memset(&g_objs[i], 0, sizeof(FakeObj));
        g_objs[i].vftable = ptrv(g_fobj_vt);
        g_objs[i].id = i + 1;
        g_objs[i].cls = &g_class;
    }
    g_reset = reset;
    int ntable = 0, nstream = 0;
    test_ctor(table, ntable);
    test_simple(table, ntable);
    test_read_location(table, ntable);
    test_finish(table, ntable);
    table.report();
    char tmp[MAX_PATH], dir[MAX_PATH];
    dir[0] = 0;
    if (GetTempPathA(MAX_PATH - 40, tmp)) {
        wsprintfA(dir, "%sZaxObjectTable\\", tmp);
        CreateDirectoryA(dir, nullptr);
    }
    test_stream(stream, nstream, dir);
    stream.report();
    if (dir[0] && !RemoveDirectoryA(dir))
        log("  could not remove %s", dir);
    g_reset = nullptr;
    reporter_out();
    log("  object table cases %d, object stream cases %d", ntable, nstream);
}

}  // namespace zax
