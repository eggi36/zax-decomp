// Checks of the network packet and the classes around it (net_packet.cpp) against the original, in the
// game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls run the original too), side 1 through the
// reimplementation. The packets are real (heap blocks built by the side's constructor); around them:
//  - block readers whose slot 2 records (reader, packet) and returns a value of its own (up to 300, so
//    block type indices above 0xfe are written and read back);
//  - token tables whose slot 1 records the id and returns a name (or null); in some scenarios the
//    token map's 0x55d200 ("known") and 0x55d1f0 (translate) jump to stand-ins that record their
//    arguments, answer "unknown" for some ids and change the ids, so the name path runs as well.
// A scenario is a random sequence of the packet's functions (writes of every size, the token writes,
// reads of every size with the read position moved around, text, ZString, skip, reset, patch, block
// types, ReadBlock where it ends, registering readers). Compared: eax (made relative: offsets into the
// data, the reader by number, the masks of the al-only results), the outputs, the event log and the
// whole packet state (data bytes, capacity, positions, flag, readers, token map) after every scenario
// and after the destructor.
// Objects: the session base (constructor, slot 2, destructor, deleting destructor), the traffic graph
// (counter 0x6d00e8 saved and restored), the block reader constructor, the named value, the slot
// jumps 0x51f6c0-0x51f6f0 and the marker check, on heap blocks filled with a pattern.
// Real data: the five static block readers of the client (0x6c2910-0x6c2930 with the names 0x4f7bf0
// gives them) registered into a packet and written as block types; their names are restored.
#include "check_util.h"
#include "net_packet.h"
#include "exe_base.h"

namespace zax {

int hooks_net_packet(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Event log and state -------------------------------------------------------------------------------

int g_side;
unsigned g_loghash[2], g_statehash[2];
int g_nlog[2];
constexpr int LOG = 256;
DWORD g_log[2][LOG][4];
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    if (g_nlog[g_side] < LOG)
        memcpy(g_log[g_side][g_nlog[g_side]], e, sizeof e);
    g_nlog[g_side]++;
}
void log_first_difference()
{
    for (int i = 0; i < g_nlog[0] && i < g_nlog[1] && i < LOG; i++)
        if (memcmp(g_log[0][i], g_log[1][i], 16)) {
            log("    event %d: %x %x %x %x / %x %x %x %x", i, g_log[0][i][0], g_log[0][i][1], g_log[0][i][2], g_log[0][i][3],
                g_log[1][i][0], g_log[1][i][1], g_log[1][i][2], g_log[1][i][3]);
            return;
        }
    if (g_nlog[0] != g_nlog[1])
        log("    events: %d / %d", g_nlog[0], g_nlog[1]);
}
void st(DWORD v) { g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4); }
void st_bytes(const void *p, DWORD n) { g_statehash[g_side] = fnv(g_statehash[g_side], p, n); }

void begin_side(int side, unsigned seed)
{
    g_side = side;
    g_seed = seed;
    g_loghash[side] = g_statehash[side] = 2166136261u;
    g_nlog[side] = 0;
}
bool sides_equal() { return g_loghash[0] == g_loghash[1] && g_statehash[0] == g_statehash[1] && g_nlog[0] == g_nlog[1]; }

// ---- Hooks of the module out and in --------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 64;
SavedSite g_sites[MAX_SITES];
int g_nsites;
void hooks_out()
{
    Hook list[MAX_SITES];
    g_nsites = 0;
    const int n = hooks_net_packet(nullptr);
    if (!hook_sites_fit("net_packet", n, MAX_SITES))
        return;
    hooks_net_packet(list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (original_address(list[i].address) == site)
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, list[i].expected);
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

// Runs one case on both sides from the same seed
template <typename F> bool both_sides(unsigned seed, F run)
{
    begin_side(0, seed);
    hooks_out();
    run();
    hooks_in();
    begin_side(1, seed);
    run();
    return sides_equal();
}

// ---- Fakes -------------------------------------------------------------------------------------------

NetPacket *g_packet;

constexpr int NREADERS = 300;
struct FakeReader {
    DWORD vftable;
    const char *name;
    DWORD id;
};
FakeReader g_readers[NREADERS];
DWORD THISCALL fake_reader_read(FakeReader *r, NetPacket *p)
{
    ev(0x100, r->id, p == g_packet);
    return r->id * 7 + 1;
}
DWORD THISCALL fake_reader_other(FakeReader *r, DWORD a)
{
    ev(0x101, r->id, a);
    return 0;
}
DWORD g_reader_vtable[3];

DWORD reader_number(const void *p)
{
    for (int i = 0; i < NREADERS; i++)
        if (p == &g_readers[i])
            return (DWORD)i + 1;
    return p ? 0xeeee : 0;
}

const char *const NAMES[] = {"Reader A", "CClientSyncBlockReader", "", "x"};
DWORD name_number(const char *s)
{
    for (DWORD i = 0; i < 4; i++)
        if (s == NAMES[i])
            return i + 1;
    return s ? 0xeeee : 0;
}

const char *const TEXTS[] = {"", "a", "Player 1", "All Non-Local Traffic", "\\gamemode\\deathmatch\\fraglimit\\20", nullptr};

// Token tables: slot 1 gives the name of an id
struct FakeTable {
    DWORD vftable;
    DWORD id;
};
FakeTable g_tables[2];
const char *THISCALL fake_table_name(FakeTable *t, DWORD id)
{
    ev(0x200, t->id, id);
    return TEXTS[id % 6];
}
DWORD g_table_vtable[2];

// Stand-ins for the token map's methods
// The whole eax is defined (the packet returns it for a known token)
DWORD THISCALL fake_known(ZPtrArray *map, FakeTable *t, DWORD id)
{
    ev(0x300, map == g_packet->tokens, t ? t->id : 0, id);
    return id % 3 != 0 ? 0x5a5a5a01 : 0x5a5a5a00;
}
DWORD THISCALL fake_translate(ZPtrArray *map, FakeTable *t, DWORD id)
{
    ev(0x301, map == g_packet->tokens, t ? t->id : 0, id);
    return id * 3 + 0x10001;
}

void init_fakes()
{
    g_reader_vtable[0] = ptrv((void *)&fake_reader_other);
    g_reader_vtable[1] = ptrv((void *)&fake_reader_other);
    g_reader_vtable[2] = ptrv((void *)&fake_reader_read);
    for (int i = 0; i < NREADERS; i++)
        g_readers[i] = {ptrv(g_reader_vtable), nullptr, (DWORD)i + 1};
    g_table_vtable[0] = 0;
    g_table_vtable[1] = ptrv((void *)&fake_table_name);
    for (DWORD i = 0; i < 2; i++)
        g_tables[i] = {ptrv(g_table_vtable), i + 1};
}

// ---- The packet's functions (exe addresses: side 0 runs the original, side 1 the reimplementation) ---

template <typename R, typename... A> R pcall(DWORD address, A... a) { return at<R(THISCALL *)(NetPacket *, A...)>(address)(g_packet, a...); }

void packet_state(const NetPacket *p)
{
    st(p->length);
    st(p->position);
    st(p->consumed);
    st(p->reliable);
    st(p->capacity);
    st(p->unknown30);
    st(p->data != nullptr);
    if (p->data && p->length <= p->capacity)
        st_bytes(p->data, p->length);
    st(p->readers.vftable);
    st(p->readers.count);
    st(p->readers.capacity);
    for (DWORD i = 0; i < p->readers.count && p->readers.data; i++)
        st(reader_number(p->readers.data[i]));
    st(reader_number(p->current));
    st(p->tokens != nullptr);
    if (p->tokens) {
        st(p->tokens->vftable);
        st(ptrv(p->tokens->data));
        st(p->tokens->count);
        st(p->tokens->capacity);
    }
}

// ReadBlock only where it ends and finds a reader in the array
bool block_readable(const NetPacket *p)
{
    DWORD pos = p->position, extra = 0, b = 0xcf;
    if (pos + 1 <= p->length) {
        b = p->data[pos++];
        while (b == 0xff) {
            extra += 0xfe;
            if (pos + 1 > p->length)
                return false;
            b = p->data[pos++];
        }
    }
    return b + extra < p->readers.count;
}

// A text read must end inside the data or in the byte behind it, made 0 here
bool text_readable(NetPacket *p)
{
    if (!p->data || p->position > p->length)
        return false;
    for (DWORD i = p->position; i < p->length; i++)
        if (!p->data[i])
            return true;
    if (p->capacity > p->length) {
        p->data[p->length] = 0;
        return true;
    }
    return false;
}

void packet_scenario(int k)
{
    BYTE *block = (BYTE *)heap_alloc(sizeof(NetPacket));
    memset(block, 0xa5, sizeof(NetPacket));
    g_packet = (NetPacket *)block;
    const DWORD r0 = at<DWORD(THISCALL *)(void *)>(0x51fe60)(block);
    ev(1, r0 - ptrv(block));
    packet_state(g_packet);
    const int nreg = k % 7 == 0 ? NREADERS : (int)(random() % 12);
    for (int i = 0; i < nreg; i++) {
        const DWORD r = pcall<DWORD>(0x51ff30, &g_readers[i], NAMES[random() % 4]);
        ev(2, r, name_number(g_readers[i].name));
    }
    const bool patched = k % 4 == 3;
    CodePatch known, translate;
    if (patched) {
        patch_jump(known, 0x55d200, (void *)&fake_known);
        patch_jump(translate, 0x55d1f0, (void *)&fake_translate);
    }
    const int nops = 5 + (int)(random() % 60);
    for (int op = 0; op < nops; op++) {
        NetPacket *p = g_packet;
        const DWORD c = random() % 32, v = random();
        DWORD r = 0, out[4] = {random(), random(), random(), random()};
        FakeTable *table = &g_tables[random() % 2];
        switch (c) {
        case 0: r = pcall<DWORD>(0x520820, (BYTE)v); break;
        case 1: r = pcall<DWORD>(0x5209c0, (WORD)v); break;
        case 2: r = pcall<DWORD>(0x520940, v); break;
        case 3: r = pcall<DWORD>(0x5208a0, (const DWORD *)out); break;
        case 4: r = pcall<DWORD>(0x520a50, (BYTE)(v % 3 == 0 ? 0 : v)); break;
        case 5: {
            const DWORD n = v % 5 == 0 ? 0 : v % 41;
            const BYTE *src = v % 7 == 0 ? nullptr : (const BYTE *)LONG_TEXT;
            r = pcall<DWORD>(0x5203d0, (const void *)src, n);
            if (!n || !src)
                r = r == ptrv(src) ? 0xaaaa : 0xbbbb;
            break;
        }
        case 6: r = pcall<DWORD>(0x520770, TEXTS[v % 6]); break;
        case 7: {
            const DWORD old = p->length;
            r = pcall<DWORD>(0x520ae0, v % 2 ? v : v % 40, (void *)table);
            if (!patched && (r & 0xff) == 1 && p->length == old + 4)
                r ^= ptrv(p->data + old) & 0xffffff00;
            break;
        }
        case 8: r = pcall<DWORD>(0x520c40, v, (void *)table); break;
        case 9: r = pcall<DWORD>(0x520da0, v, (void *)table); break;
        case 10: r = pcall<DWORD>(0x520490, (BYTE *)out); break;
        case 11: r = pcall<DWORD>(0x520510, (WORD *)out); break;
        case 12: r = pcall<DWORD>(0x520550, out); break;
        case 13: r = pcall<DWORD>(0x5204c0, out); break;
        case 14: r = pcall<DWORD>(0x520580, (BYTE *)out); break;
        case 15: r = pcall<DWORD>(0x5205d0, out, (void *)table); break;
        case 16: r = pcall<DWORD>(0x520620, (BYTE *)out, (void *)table); break;
        case 17: r = pcall<DWORD>(0x520670, (WORD *)out, (void *)table); break;
        case 18:
            if (!text_readable(p))
                continue;
            {
                const char *s = pcall<const char *>(0x5206c0);
                r = s == at<const char *>(0x62e558) ? 0xeeee : ptrv(s) - ptrv(p->data);
            }
            break;
        case 19:
            if (!text_readable(p))
                continue;
            {
                ZString z;
                at<void(THISCALL *)(ZString *)>(0x4dec90)(&z);
                r = pcall<DWORD>(0x520700, &z);
                r ^= ptrv(&z) & 0xffffff00;
                out[0] = fnv_text(2166136261u, z.c_str());
                at<void(THISCALL *)(ZString *)>(0x4defd0)(&z);
            }
            break;
        case 20: r = pcall<DWORD>(0x520470, v % 9 == 0 ? v : v % 24); break;
        case 21:
            if (v % 3)
                continue;
            r = pcall<DWORD>(0x520750);
            break;
        case 22: r = pcall<DWORD>(0x520f50); break;
        case 23:
            if (v % 3)
                continue;
            r = pcall<DWORD>(0x520f60);
            break;
        case 24:
            if (p->length < 4)
                continue;
            r = pcall<DWORD>(0x520f10, random(), v % (p->length - 3)) - ptrv(p->data);
            break;
        case 25:
            if (p->length < 2)
                continue;
            r = pcall<DWORD>(0x520f30, (WORD)random(), v % (p->length - 1)) - ptrv(p->data);
            break;
        case 26: {
            const DWORD i = p->readers.count && v % 5 ? (v >> 8) % p->readers.count : (v >> 8) % NREADERS;
            const DWORD kind = random() % 6;
            r = pcall<DWORD>(0x51ff50, (void *)&g_readers[i], kind);
            break;
        }
        case 27:
            if (!block_readable(p))
                continue;
            r = pcall<DWORD>(0x5200d0);
            if (r == ptrv(p))
                r = 0xdddd;
            break;
        case 28:
            if (p->readers.count >= NREADERS)
                continue;
            r = pcall<DWORD>(0x51ff30, &g_readers[p->readers.count], NAMES[v % 4]);
            break;
        case 29: r = pcall<DWORD>(0x5205c0, 0xabababab) & 0xff; break;
        default:
            // Move the read position (also past the end now and then)
            p->position = v % 11 == 0 ? p->length + 1 + v % 3 : (p->length ? v % (p->length + 1) : 0);
            continue;
        }
        ev(0x10 + c, r, out[0], out[1] ^ out[2] ^ out[3]);
        st(p->length);
        st(p->position);
    }
    if (patched) {
        unpatch(translate);
        unpatch(known);
    }
    packet_state(g_packet);
    at<void(THISCALL *)(void *)>(0x51fec0)(block);
    st(g_packet->readers.vftable);
    st(ptrv(g_packet->readers.data));
    st(g_packet->readers.count);
    st(g_packet->readers.capacity);
    st(ptrv(g_packet->data));
    st(ptrv(g_packet->tokens));
    st(g_packet->length);
    st(g_packet->position);
    heap_free(block);
    for (int i = 0; i < NREADERS; i++)
        g_readers[i].name = nullptr;
}

// ---- Objects -----------------------------------------------------------------------------------------

struct FakeSlots {
    DWORD vftable;
    DWORD id;
};
DWORD g_slot_vtable[28];
FakeSlots g_slot_object;
template <int S> DWORD THISCALL fake_slot_n(FakeSlots *self, DWORD a, DWORD b)
{
    ev(0x400 + S, self == &g_slot_object, a, b);
    return S * 100 + a;
}

void session_case(int k)
{
    BYTE *block = (BYTE *)heap_alloc(0x38);
    memset(block, 0x5a + k, 0x38);
    NetSessionBase *s = (NetSessionBase *)block;
    ev(1, at<DWORD(THISCALL *)(void *)>(0x51f780)(block) - ptrv(block));
    for (DWORD o = 0; o < 0x38; o += 4)
        if (o != 0x1c && o != 0x24)
            st(*(DWORD *)(block + o));
    st(s->settings != nullptr);
    if (s->settings) {
        const BYTE *ps = (const BYTE *)s->settings;
        const StringState z = state(*(const ZString *)ps, nullptr);
        st_bytes(&z, sizeof z);
        st(*(const DWORD *)(ps + 4));
        st(*(const DWORD *)(ps + 8));
        st(ps[0xc]);
    }
    const StringState z = state(s->text, nullptr);
    st_bytes(&z, sizeof z);
    s->flag18 = (BYTE)(k + 1);
    at<void(THISCALL *)(void *)>(0x51f870)(block);
    st(s->flag18);
    if (k % 3 == 2) {
        at<void(THISCALL *)(void *)>(0x51f830)(block);
        st(s->vftable);
        st(ptrv(s->settings));
        heap_free(block);
    }
    else {
        const DWORD flags = k % 3;
        ev(2, at<DWORD(THISCALL *)(void *, DWORD)>(0x51f7e0)(block, flags) - ptrv(block));
        if (!(flags & 1)) {
            st(s->vftable);
            st(ptrv(s->settings));
            heap_free(block);
        }
    }
}

void graph_case(int k)
{
    DWORD *count = at<DWORD *>(NET_GRAPH_COUNT);
    const DWORD saved = *count;
    *count = k % 2 ? saved : random() % 5;
    const DWORD before = *count;
    BYTE *block = (BYTE *)heap_alloc(0x1c);
    memset(block, 0x33 + k, 0x1c);
    NetTrafficGraph *g = (NetTrafficGraph *)block;
    ev(1, at<DWORD(THISCALL *)(void *)>(0x51f8e0)(block) - ptrv(block), *count - before);
    st((DWORD)g->x);
    st((DWORD)g->y);
    st(g->samples);
    st(g->height);
    st(g->unknown10);
    st(g->sent != nullptr);
    st(g->received != nullptr);
    if (g->sent)
        st_bytes(g->sent, 0x140);
    if (g->received)
        st_bytes(g->received, g->samples * 4);
    if (k % 3 == 0) {
        heap_free(g->sent);
        g->sent = nullptr;
    }
    at<void(THISCALL *)(void *)>(0x51f960)(block);
    st(*count - before);
    st(ptrv(g->sent));
    st(ptrv(g->received));
    heap_free(block);
    *count = saved;
}

void small_objects_case(int k)
{
    // Block reader constructor
    DWORD reader[2] = {random(), random()};
    ev(1, at<DWORD(THISCALL *)(void *)>(0x51f8d0)(reader) - ptrv(reader), reader[0], reader[1]);
    // Named value
    BYTE *block = (BYTE *)heap_alloc(0xc);
    memset(block, 0x77 + k, 0xc);
    NamedValue *nv = (NamedValue *)block;
    ev(2, at<DWORD(THISCALL *)(void *)>(0x51f700)(block) - ptrv(block), nv->vftable, nv->value);
    for (int i = 0; i < 4; i++) {
        const DWORD v = i == 0 ? nv->value : random() % 3;
        ev(3, at<DWORD(THISCALL *)(void *, DWORD)>(0x51f750)(block, v), nv->value);
        const char *t = TEXTS[random() % 5];
        ev(4, at<DWORD(THISCALL *)(void *, const char *)>(0x51f770)(block, t) - ptrv(&nv->name), fnv_text(2166136261u, nv->name.c_str()));
    }
    const DWORD flags = k % 2;
    ev(5, at<DWORD(THISCALL *)(void *, DWORD)>(0x51f720)(block, flags) - ptrv(block));
    if (!flags) {
        st(nv->vftable);
        heap_free(block);
    }
    // The slot jumps and the marker check
    for (DWORD s = 0; s < 4; s++) {
        const DWORD a = random(), b = random();
        ev(6, at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x51f6c0 + s * 0x10)(&g_slot_object, a, b));
    }
    ev(7, at<DWORD(THISCALL *)(void *, DWORD)>(0x5205c0)(nullptr, random()) & 0xff);
}

// The client's block readers, registered as 0x4f7bf0 does
void real_readers_case(int k)
{
    const DWORD readers[5] = {0x6c2920, 0x6c2930, 0x6c2910, 0x6c2918, 0x6c2928};
    const DWORD names[5] = {0x6188b0, 0x618898, 0x6188c8, 0x6188e8, 0x618884};
    DWORD saved[5][2];
    for (int i = 0; i < 5; i++)
        memcpy(saved[i], at<void *>(readers[i]), 8);
    BYTE *block = (BYTE *)heap_alloc(sizeof(NetPacket));
    g_packet = (NetPacket *)block;
    at<DWORD(THISCALL *)(void *)>(0x51fe60)(block);
    for (int i = 0; i < 5; i++) {
        ev(1, pcall<DWORD>(0x51ff30, at<void *>(readers[i]), at<const char *>(names[i])),
           *at<DWORD *>(readers[i] + 4) - names[i], *at<DWORD *>(readers[i]));
    }
    for (int i = 0; i < 20 + k; i++) {
        const int j = (int)(random() % 6);
        ev(2, pcall<DWORD>(0x51ff50, j < 5 ? at<void *>(readers[j]) : (void *)&g_readers[0], random() % 6));
    }
    packet_state(g_packet);
    for (DWORD i = 0; i < g_packet->readers.count; i++)
        st(g_packet->readers.data[i] ? *(const DWORD *)g_packet->readers.data[i] : 0);
    at<void(THISCALL *)(void *)>(0x51fec0)(block);
    heap_free(block);
    for (int i = 0; i < 5; i++)
        memcpy(at<void *>(readers[i]), saved[i], 8);
}

}  // namespace

ZAX_CHECKS(net_packet)
{
    (void)cls;
    (void)n;
    init_fakes();
    g_slot_vtable[24] = ptrv((void *)&fake_slot_n<24>);
    g_slot_vtable[25] = ptrv((void *)&fake_slot_n<25>);
    g_slot_vtable[26] = ptrv((void *)&fake_slot_n<26>);
    g_slot_vtable[27] = ptrv((void *)&fake_slot_n<27>);
    g_slot_object = {ptrv(g_slot_vtable), 1};
    {
        Tally t{"net packet: scenarios of writes, reads, tokens, block types"};
        for (int k = 0; k < scaled(600); k++) {
            const bool same = both_sides(0x51fe60u + (unsigned)k * 7919u, [k] { packet_scenario(k); });
            char d[64];
            wsprintfA(d, "[scenario %d]", k);
            t.check(same, "", d);
            if (!same && t.mismatches <= 3)
                log_first_difference();
        }
        t.report();
    }
    {
        Tally t{"net packet: session base, traffic graph, reader, named value, slot jumps"};
        for (int k = 0; k < scaled(60); k++) {
            const bool same = both_sides(0x51f780u + (unsigned)k * 104729u, [k] {
                session_case(k);
                graph_case(k);
                small_objects_case(k);
            });
            char d[64];
            wsprintfA(d, "[object case %d]", k);
            t.check(same, "", d);
            if (!same && t.mismatches <= 3)
                log_first_difference();
        }
        t.report();
    }
    {
        Tally t{"net packet: the client's block readers"};
        for (int k = 0; k < scaled(10); k++) {
            const bool same = both_sides(0x4f7bf0u + (unsigned)k * 31u, [k] { real_readers_case(k); });
            char d[64];
            wsprintfA(d, "[real readers %d]", k);
            t.check(same, "", d);
            if (!same && t.mismatches <= 3)
                log_first_difference();
        }
        t.report();
    }
}

}  // namespace zax
