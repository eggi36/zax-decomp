// Checks of the sequences' loading (sequence_load.cpp) against the original, in the game thread.
//  - The frame filters: DefaultFilters (0x56b260) and SetupFilters (0x56b440) with every load flag
//    combination and every blend type of the name list, with the shared list 0x713360 and with null (the default set first);
//    compared: the list's chain (vtable, kind, depth, lock, kind 3's and the converters' parameter), eax. The
//    shared list is the loaders' scratch state; the default set is left in it afterwards.
//  - The cache file name (0x56b780) with paths, names and the shared list with target depths 8/16/24/32 or
//    none (the display's depth); compared: the string, al.
//  - The binary slots on CFileBuf objects over a memory stream of our own: WriteBinary of CSequence (0x56ba90)
//    and CRotationSequence (0x55f980) from the level's real sequences (cursors, the models' rotation
//    sequences, read only); compared: the bytes written and the stream's call log. ReadBinary (0x56b9d0,
//    0x55f910) of those bytes into fresh objects (also cut short, so a frame fails); compared: al, the
//    rectangle, the frames (fields, blocks and tables hashed) and the deltas; the objects deleted afterwards.
//  - LoadCache (0x56b890, through 0x56b950) of the cursors' cache files the game loads at start (the names from
//    the start code 0x4f1d20-0x4f1fa0, folder "Interface/Cursors") and a missing name into fresh CSequences;
//    compared as above.
//  - AddFrame (0x56bb70): 1-5 new frames (CFrame, 16 bits, random sizes and origins, a pixel pattern) with
//    filter 2 and 6 on or off, appended to a fresh CSequence; compared: the frames' fields and pixels, the
//    rectangle. Not checked: ReadProperties (0x56b320, not hooked): the game data has no Properties.txt, so
//    it would write one.
#include "checks_inventory_common.h"
#include "checks_frame_common.h"
#include "ai_helpers.h"
#include "file_buffer.h"

namespace zax {

int hooks_sequence_load(Hook *out);

namespace {

using namespace ai;

struct Hooks {
    void out()
    {
        invcheck::g_hook_table = &hooks_sequence_load;
        invcheck::hooks_out();
    }
    void in() { invcheck::hooks_in(); }
} g_hooks;

template <typename F> void compare_sides(Tally &t, const char *what, F f)
{
    unsigned h[2];
    for (int side = 0; side < 2; side++) {
        if (side == 0)
            g_hooks.out();
        h[side] = f(side);
        if (side == 0)
            g_hooks.in();
    }
    char d[200];
    wsprintfA(d, "[%.150s: %x / %x]", what, h[0], h[1]);
    t.check(h[0] == h[1], "", d);
}

constexpr DWORD FILTERS = 0x713360;

unsigned filters_hash(DWORD list)
{
    unsigned h = 2166136261u;
    int n = 0;
    for (const BYTE *f = at<const BYTE *>(list); f && n < 40; f = bp(dw(const_cast<BYTE *>(f), 4)), n++) {
        DWORD v[5] = {dw(const_cast<BYTE *>(f), 0), dw(const_cast<BYTE *>(f), 8), dw(const_cast<BYTE *>(f), 0xc), f[0x10], 0};
        if (v[1] == 3 || v[1] == 11 || v[1] == 12)
            v[4] = dw(const_cast<BYTE *>(f), 0x14);
        if (f != at<const BYTE *>(list))
            h = fnv(h, v, sizeof v);
    }
    return fnv(h, &n, 4);
}
void default_filters_original() { original<DWORD(__cdecl *)()>(0x56b260)(); }

void check_filters(Tally &t)
{
    compare_sides(t, "DefaultFilters", [](int) -> unsigned {
        const DWORD r = at<DWORD(__cdecl *)()>(0x56b260)();
        return filters_hash(FILTERS) ^ (r == FILTERS);
    });
    // blend types within the name list (FieldNameList::Value reads the entry without a range test)
    BYTE *names = at<BYTE *(__cdecl *)()>(0x4669a0)();
    const DWORD blends = dw(names, 0xc) < 16 ? dw(names, 0xc) : 16;
    log("  sequence load: %d blend types", blends);
    for (DWORD flags = 0; flags < 0x20; flags++)
        for (DWORD blend = 0; blend < blends; blend++)
            for (int with_list = 0; with_list < 2; with_list++) {
                if (!(flags & 1) && (blend & 1) && with_list)
                    continue;   // fewer cases: the list argument with every second blend type
                BYTE seq[0x40] = {};
                seq[0x1c] = (BYTE)flags;
                dw(seq, 0x20) = blend;
                char what[64];
                wsprintfA(what, "SetupFilters flags %x blend %d list %d", flags, blend, with_list);
                compare_sides(t, what, [&](int) -> unsigned {
                    default_filters_original();
                    const DWORD r = at<DWORD(THISCALL *)(void *, DWORD)>(0x56b440)(seq, with_list ? FILTERS : 0);
                    return filters_hash(FILTERS) ^ (r == FILTERS);
                });
            }
    default_filters_original();
}

void check_cache_name(Tally &t)
{
    static const char *PATHS[] = {"Interface/Cursors", "Characters/Crater", "", "a/b/c/"};
    static const char *NAMES[] = {"Pointer", "Walk Shadow", "", "x.y"};
    static const DWORD DEPTHS[] = {0, 8, 16, 24, 32, 15};
    for (const char *p : PATHS)
        for (const char *nm : NAMES)
            for (DWORD depth : DEPTHS) {
                char what[96];
                wsprintfA(what, "CacheFileName '%s' '%s' depth %d", p, nm, depth);
                compare_sides(t, what, [&](int) -> unsigned {
                    default_filters_original();
                    DWORD list = 0;
                    if (depth) {
                        at<void(THISCALL *)(void *, DWORD)>(0x4b1db0)(at<void *>(FILTERS), 11);
                        if (BYTE *f = at<BYTE *(THISCALL *)(void *, DWORD)>(0x4b2140)(at<void *>(FILTERS), 11))
                            dw(f, 0x14) = depth;
                        list = FILTERS;
                    }
                    BYTE out[4];
                    zs_ctor(out);
                    const BYTE r = at<BYTE(__fastcall *)(void *, const char *, const char *, DWORD)>(0x56b780)(out, p, nm, list);
                    unsigned h = fnv_text(2166136261u, zs_c_str(out)) ^ r;
                    zs_dtor(out);
                    return h;
                });
            }
    default_filters_original();
}

// ---- A memory stream of our own under a CFileBuf ----------------------------------------------------------

struct MemStream : ZStream {
    BYTE *data;
    DWORD capacity, size, pos;
    unsigned log;
};
void mrec(MemStream *s, DWORD slot, DWORD a, DWORD b)
{
    DWORD r[3] = {slot, a, b};
    s->log = fnv(s->log, r, sizeof r);
}
MemStream *THISCALL ms_delete(MemStream *s, DWORD f)
{
    mrec(s, 0, f, 0);
    return s;
}
DWORD THISCALL ms_unused(MemStream *s)
{
    mrec(s, 1, 0, 0);
    return 0;
}
DWORD THISCALL ms_write(MemStream *s, const void *p, DWORD n)
{
    DWORD w = n;
    if (s->pos + w > s->capacity)
        w = s->capacity > s->pos ? s->capacity - s->pos : 0;
    memcpy(s->data + s->pos, p, w);
    mrec(s, 2, n, fnv(2166136261u, p, n));
    s->pos += w;
    if (s->pos > s->size)
        s->size = s->pos;
    return w;
}
DWORD THISCALL ms_read(MemStream *s, void *p, DWORD n)
{
    DWORD rest = s->pos < s->size ? s->size - s->pos : 0, w = n < rest ? n : rest;
    memcpy(p, s->data + s->pos, w);
    s->pos += w;
    mrec(s, 3, n, w);
    return w;
}
bool THISCALL ms_skip(MemStream *s, DWORD d)
{
    const DWORD p = s->pos + d;
    const bool ok = p <= s->size;
    if (ok)
        s->pos = p;
    mrec(s, 4, d, ok);
    return ok;
}
bool THISCALL ms_seek(MemStream *s, DWORD p)
{
    const bool ok = p <= s->size;
    if (ok)
        s->pos = p;
    mrec(s, 5, p, ok);
    return ok;
}
DWORD THISCALL ms_tell(MemStream *s)
{
    mrec(s, 6, 0, 0);
    return s->pos;
}
DWORD THISCALL ms_size(MemStream *s)
{
    mrec(s, 7, 0, 0);
    return s->size;
}
DWORD THISCALL ms_set_size(MemStream *s, DWORD n)
{
    mrec(s, 8, n, 0);
    s->size = n < s->capacity ? n : s->capacity;
    return 0;
}
DWORD THISCALL ms_close(MemStream *s)
{
    mrec(s, 9, 0, 0);
    return 0;
}
BYTE *THISCALL ms_advance(MemStream *s, DWORD n)
{
    mrec(s, 11, n, 0);
    BYTE *r = s->data + s->pos;
    s->pos += n;
    return r;
}
BYTE *THISCALL ms_detach(MemStream *s)
{
    mrec(s, 12, 0, 0);
    return nullptr;
}
bool THISCALL ms_read_line(MemStream *s, char *p, DWORD n)
{
    mrec(s, 13, n, 0);
    if (n)
        p[0] = 0;
    return false;
}
DWORD g_ms_vtable[14];
void init_ms_vtable()
{
    void *f[14] = {(void *)&ms_delete, (void *)&ms_unused, (void *)&ms_write,    (void *)&ms_read,    (void *)&ms_skip,
                   (void *)&ms_seek,   (void *)&ms_tell,   (void *)&ms_size,     (void *)&ms_set_size, (void *)&ms_close,
                   (void *)&ms_unused, (void *)&ms_advance, (void *)&ms_detach,  (void *)&ms_read_line};
    for (int i = 0; i < 14; i++)
        g_ms_vtable[i] = (DWORD)(uintptr_t)f[i];
}
constexpr DWORD STREAM_CAP = 0x200000;
BYTE *g_stream_data;   // heap, STREAM_CAP bytes
BYTE *g_written;       // a copy of what side 0 wrote
DWORD g_written_size;

struct Buf {
    MemStream s;
    BYTE fb[0x1c];
    Buf(DWORD size)
    {
        s.vftable = (DWORD)(uintptr_t)g_ms_vtable;
        s.handle = nullptr;
        s.owned = 0;
        s.data = g_stream_data;
        s.capacity = STREAM_CAP;
        s.size = size;
        s.pos = 0;
        s.log = 2166136261u;
        original<void *(THISCALL *)(void *)>(0x4a1780)(fb);
        dw(fb, 4) = (DWORD)(uintptr_t)&s;
    }
    ~Buf()
    {
        dw(fb, 4) = 0;
        original<DWORD(THISCALL *)(void *)>(0x4a1800)(fb);
    }
};

unsigned frame_hash(unsigned h, BYTE *f)
{
    if (!f)
        return fnv(h, "-", 1);
    h = fnv(h, f, 4);
    h = fnv(h, f + 8, 4);
    h = fnv(h, f + 0x10, 6);
    h = fnv(h, f + 0x3c, 4);
    const DWORD height = *reinterpret_cast<WORD *>(f + 0x14);
    if (dw(f, 0x3c) & 6) {
        static const DWORD BLOCKS[5] = {0xc, 0x1c, 0x24, 0x2c, 0x34};
        for (DWORD b : BLOCKS) {
            if (BYTE *p = bp(dw(f, b)))
                h = fnv(h, p, dw(p, 0) < 0x400000 ? dw(p, 0) : 4);
            if (BYTE *tb = bp(dw(f, b == 0xc ? 0x18 : b + 4)))
                h = fnv(h, tb, height * 4);
        }
    } else if (BYTE *px = bp(dw(f, 0xc))) {
        h = fnv(h, px, *reinterpret_cast<WORD *>(f + 0x12) * height * vcall<DWORD>(f, 30));
    }
    return h;
}
unsigned seq_hash(BYTE *seq, bool rotation)
{
    unsigned h = fnv(2166136261u, seq + 0xc, 0x10);
    h = fnv(h, seq + 0x28, 4);
    for (DWORD i = 0; dw(seq, 8) && i < dw(seq, 0x28); i++)
        h = frame_hash(h, bp(reinterpret_cast<DWORD *>(bp(dw(seq, 8)))[i]));
    if (rotation && dw(seq, 0x44))
        h = fnv(h, bp(dw(seq, 0x44)), dw(seq, 0x28) * 8);
    return h;
}
BYTE *new_sequence(bool rotation) { return static_cast<BYTE *>(at<void *(__cdecl *)()>(rotation ? 0x55eb40 : 0x56aba0)()); }
void delete_sequence(BYTE *s) { vcall<void>(s, 1, 1); }

BYTE *g_sources[24];
bool g_source_rotation[24];
int g_nsources;
void find_sources()
{
    g_nsources = 0;
    static const DWORD CURSORS[] = {0x6c2640, 0x6c2138};
    for (DWORD c : CURSORS)
        if (dw(at<void *>(c), 8) && dw(at<void *>(c), 0x28)) {
            g_source_rotation[g_nsources] = false;
            g_sources[g_nsources++] = at<BYTE *>(c);
        }
    int found = 0, seen = 0;
    framec::each_list_object(framec::MODEL_LIST, [&](void *m) {
        const BYTE *mb = static_cast<const BYTE *>(m);
        if (!invcheck::readable(mb, 0x50))
            return;
        const DWORD n = *reinterpret_cast<const DWORD *>(mb + 0x48);
        BYTE *const *seqs = *reinterpret_cast<BYTE *const *const *>(mb + 0x4c);
        if (!seqs || n > 512 || !invcheck::readable(seqs, n * 4))
            return;
        for (DWORD i = 0; i < n && g_nsources < 24; i++)
            if (seqs[i] && dw(seqs[i], 0) == 0x5fec14 && dw(seqs[i], 8) && dw(seqs[i], 0x28) && seen++ % 7 == 0 && found < 16) {
                g_source_rotation[g_nsources] = true;
                g_sources[g_nsources++] = seqs[i];
                found++;
            }
    });
}

void check_binary(Tally &t)
{
    for (int k = 0; k < g_nsources; k++) {
        BYTE *src = g_sources[k];
        const bool rot = g_source_rotation[k];
        const DWORD write_fn = rot ? 0x55f980 : 0x56ba90, read_fn = rot ? 0x55f910 : 0x56b9d0;
        char what[96];
        wsprintfA(what, "WriteBinary source %d (%s, %d frames)", k, rot ? "rotation" : "plain", dw(src, 0x28));
        // the frame writer 0x4a9740 leaves two bytes of its header (after the height) as the stack had them,
        // so what each side wrote is read back by the original and compared as frames
        compare_sides(t, what, [&](int side) -> unsigned {
            Buf b(0);
            at<void(THISCALL *)(void *, void *)>(write_fn)(src, b.fb);
            const DWORD size = b.s.size;
            if (side == 0) {
                memcpy(g_written, g_stream_data, size);
                g_written_size = size;
            }
            Buf r(size);
            BYTE *s = new_sequence(rot);
            dw(s, 0x28) = dw(src, 0x28);
            const BYTE ok = original<BYTE(THISCALL *)(void *, void *)>(read_fn)(s, r.fb);
            const unsigned h = seq_hash(s, rot) ^ ok ^ size << 8;
            if (!dw(s, 8))
                dw(s, 0x28) = 0;
            delete_sequence(s);
            return h;
        });
        // whole, or cut inside the rectangle (every frame byte then fails to read; a cut inside a frame leaves
        // the exe's frame reader 0x4a9340 with locals the stack had)
        for (int cut = 0; cut < 2; cut++) {
            const DWORD size = cut == 0 ? g_written_size : 9;
            wsprintfA(what, "ReadBinary source %d (%s), %d of %d bytes", k, rot ? "rotation" : "plain", size, g_written_size);
            compare_sides(t, what, [&](int) -> unsigned {
                memcpy(g_stream_data, g_written, g_written_size);
                Buf b(size);
                BYTE *s = new_sequence(rot);
                dw(s, 0x28) = dw(src, 0x28);
                const BYTE r = at<BYTE(THISCALL *)(void *, void *)>(read_fn)(s, b.fb);
                unsigned h = seq_hash(s, rot) ^ r;
                h = fnv(h, &b.s.log, 4);
                if (!dw(s, 8))
                    dw(s, 0x28) = 0;
                delete_sequence(s);
                return h;
            });
        }
    }
}

void check_load_cache(Tally &t)
{
    // the cursor names of the start code: push name; push "Interface/Cursors"; mov ecx, seq; call 0x56b950
    const char *names[24];
    int n = 0;
    for (DWORD a = 0x4f1d20; a < 0x4f1fa0 && n < 23; a++) {
        const BYTE *p = at<const BYTE *>(a);
        if (p[0] == 0x68 && p[5] == 0x68 && dw(const_cast<BYTE *>(p), 6) == 0x6184d0)
            names[n++] = at<const char *>(dw(const_cast<BYTE *>(p), 1));
    }
    names[n++] = "No Such Cursor";
    for (int i = 0; i < n; i++) {
        char what[96];
        wsprintfA(what, "LoadCache 'Interface/Cursors' '%.40s'", names[i]);
        compare_sides(t, what, [&](int) -> unsigned {
            BYTE *s = new_sequence(false);
            const BYTE r = at<BYTE(THISCALL *)(void *, const char *, const char *, DWORD)>(0x56b950)(s, at<const char *>(0x6184d0), names[i], 0);
            const unsigned h = seq_hash(s, false) ^ r ^ (dw(s, 0x34) ? 0x100 : 0);
            delete_sequence(s);
            return h;
        });
    }
    log("  sequence load: %d cursor files", n);
}

void check_add_frame(Tally &t)
{
    for (int k = 0; k < scaled(60); k++) {
        g_seed = 0x56bb70 + (unsigned)k;
        const int frames = 1 + (int)(random() % 5);
        unsigned seeds[5];
        DWORD flags[5];
        for (int i = 0; i < frames; i++) {
            seeds[i] = random();
            flags[i] = random() % 4;
        }
        char what[64];
        wsprintfA(what, "AddFrame case %d, %d frames", k, frames);
        compare_sides(t, what, [&](int) -> unsigned {
            BYTE *s = new_sequence(false);
            unsigned h = 2166136261u;
            for (int i = 0; i < frames; i++) {
                g_seed = seeds[i];
                BYTE *f = static_cast<BYTE *>(pool_alloc(0x40));
                at<void *(THISCALL *)(void *)>(0x4a8a90)(f);
                const DWORD w = 1 + random() % 40, hh = 1 + random() % 30;
                at<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(f, w, hh, 16);
                *reinterpret_cast<short *>(f + 8) = (short)(random() % 41 - 20);
                *reinterpret_cast<short *>(f + 10) = (short)(random() % 41 - 20);
                if (BYTE *px = bp(dw(f, 0xc)))
                    for (DWORD j = 0; j < w * hh * 2; j++)
                        px[j] = (BYTE)(random() >> 24);
                at<void(THISCALL *)(void *, void *, DWORD, DWORD)>(0x56bb70)(s, f, flags[i] & 1, flags[i] >> 1);
                const unsigned sh = seq_hash(s, false);
                h = fnv(h, &sh, 4);
            }
            delete_sequence(s);
            return h;
        });
    }
}

}  // namespace

ZAX_CHECKS(sequence_load)
{
    (void)cls;
    (void)n;
    invcheck::g_hook_table = &hooks_sequence_load;
    init_ms_vtable();
    g_stream_data = static_cast<BYTE *>(heap_alloc(STREAM_CAP));
    g_written = static_cast<BYTE *>(heap_alloc(STREAM_CAP));
    if (!g_stream_data || !g_written) {
        log("Check sequence load: no memory");
        return;
    }
    Tally filters{"sequence load: DefaultFilters, SetupFilters"};
    check_filters(filters);
    filters.report();
    Tally names{"sequence load: CacheFileName"};
    check_cache_name(names);
    names.report();
    find_sources();
    log("  sequence load: %d real sequences", g_nsources);
    Tally binary{"sequence load: WriteBinary, ReadBinary (CSequence, CRotationSequence) on a memory stream"};
    check_binary(binary);
    binary.report();
    Tally load{"sequence load: LoadCache of the cursors"};
    check_load_cache(load);
    load.report();
    Tally add{"sequence load: AddFrame with the filters 2 and 6"};
    check_add_frame(add);
    add.report();
    heap_free(g_stream_data);
    heap_free(g_written);
}

}  // namespace zax
