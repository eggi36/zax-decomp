// Checks of the net session, the local net layer and the block readers' functions (net_session.cpp)
// against the original, in the game thread (side 0 with the module's hooks out, side 1 through the
// reimplementation; checks_net_common.h). No network is involved.
//  - Readers: the ten accessors; the client and server registrations into fresh packets (the other
//    subsystems' registrations that 0x4f93e0 calls run as they are): the readers, their names, eax.
//    The names of the static readers are saved and restored.
//  - Writers: random sequences of the seven write functions and the PlaySound reader's write on a real
//    packet with the readers registered: fake parts (+0x1c, +0x28, +0x48, slots 37 and 43 recording and
//    giving rectangles and pairs), fake levels (+0x48/+0x4c for the position 0x4eae90 scales), bit sets,
//    float pairs from -2 to 2 with the edges, NaN and infinity, the world 0x6c2080 swapped for a fake
//    for 0x4f1050 (no level, no local player, an index out of range, a level). Compared: the packet
//    (bytes, length, flag), eax, the parts' log.
//  - Sound tables: a PlaySound reader of our own (the real table vtables, heap data) and random
//    sequences of FreeChannel, Add (lookup through stand-ins for 0x57e650/0x57b630 that give fake
//    sounds), Remove on both tables, SoundAt, StopAll (the sounds' slot 11 recorded), Reserve, and the
//    write with mode 2; the deleting destructor at the end. Compared: both tables (ids, channels,
//    sounds, counts, capacities), eax (normalised), the log. NetLocal's slot 5 on the game's PlaySound
//    reader with its tables swapped for test ones (its 0x28 bytes saved and restored).
//  - Sessions: the constructor with the real world (slot 106 registers the readers into both packets,
//    slot 41 makes the key binding object) and the three ways out (deleting destructor with flags 1, with
//    0 and the destructor); the fields, both packets' readers, the graph, the text, eax.
//  - NetLocal: constructor, deleting destructor (flags 0/1), destructor; the local player global 0x6c2938
//    (saved and restored).
#include "checks_net_common.h"
#include "net_session.h"
#include "exe_base.h"

namespace zax {

int hooks_net_session(Hook *out);

namespace {

using namespace netcheck;

constexpr DWORD READERS[] = {0x6c2910, 0x6c2918, 0x6c2920, 0x6c2928, 0x6c2930, 0x6c2940,
                             0x6c2948, 0x6c2950, 0x6c2958, 0x6c2960, 0x6c2988, 0x6c2990};
struct ReaderNames {
    DWORD names[12];
    void save()
    {
        for (int i = 0; i < 12; i++)
            names[i] = at<DWORD *>(READERS[i])[1];
    }
    void restore()
    {
        for (int i = 0; i < 12; i++)
            at<DWORD *>(READERS[i])[1] = names[i];
    }
};

void st_readers(const NetPacket *p)
{
    st(p->readers.count);
    for (DWORD i = 0; i < p->readers.count; i++) {
        const BlockReader *r = (const BlockReader *)p->readers.data[i];
        st(ptrv(r));
        st_text(r ? r->name : nullptr);
    }
}

constexpr DWORD ACCESSORS[] = {0x4f78a0, 0x4f7910, 0x4f79a0, 0x4f7b80, 0x4f8770, 0x4f8850, 0x4f8990, 0x4f8af0, 0x4f8d30, 0x4f9390};

void readers_case(int k)
{
    for (DWORD a : ACCESSORS)
        ev(0x500, a, at<DWORD(__cdecl *)()>(a)());
    ReaderNames names;
    names.save();
    NetPacket *p = (NetPacket *)arena(sizeof(NetPacket));
    packet_ctor(p);
    for (int n = 0, steps = 1 + k % 3; n < steps; n++)
        ev(0x501, at<DWORD(THISCALL *)(NetPacket *)>(rnd(2) ? 0x4f7bf0 : 0x4f93e0)(p));
    st_readers(p);
    packet_dtor(p);
    names.restore();
}

// ---- Writers --------------------------------------------------------------------------------------------

struct Part {
    DWORD vftable;
    BYTE b[0x60];
};
DWORD g_part_vt[64];
DWORD THISCALL part_rect(Part *part, DWORD *rect, DWORD a, DWORD b)
{
    ev(0x510, ptrv(part), a, b);
    for (int i = 0; i < 4; i++)
        rect[i] = random();
    return (random() & 0xffffff00u) | (DWORD)(rnd(4) != 0);
}
DWORD THISCALL part_pair(Part *part, DWORD *out)
{
    ev(0x511, ptrv(part));
    out[0] = random();
    out[1] = random();
    return 0;
}
Part *new_part()
{
    g_part_vt[37] = ptrv((void *)&part_rect);
    g_part_vt[43] = ptrv((void *)&part_pair);
    Part *p = (Part *)arena(sizeof(Part));
    p->vftable = ptrv(g_part_vt);
    *(WORD *)(p->b + 0x28 - 4) = (WORD)(rnd(4) ? 1 + rnd(0xffff) : 0);
    *(DWORD *)(p->b + 0x48 - 4) = rnd(4) ? random() | 1 : 0;
    *(DWORD *)(p->b + 0x1c - 4) = random();
    return p;
}

// A level for 0x4eae90 (+0x48 width, +0x4c height)
BYTE *new_level()
{
    BYTE *l = (BYTE *)arena(0x60);
    *(int *)(l + 0x48) = (int)(rnd(8) == 0 ? 0 : 640 + rnd(8000));
    *(int *)(l + 0x4c) = (int)(rnd(8) == 0 ? 0 : 480 + rnd(8000));
    return l;
}

const DWORD FLOATS[] = {0xbf800000, 0x3f800000, 0x00000000, 0x80000000, 0x3f000000, 0xbf000000, 0x40000000,
                        0xc0000000, 0x7fc00000, 0x7f800000, 0xff800000, 0x3f7fffff, 0xbf7fffff, 0x00000001,
                        0x3dcccccd, 0x3f7ff000};
float random_float()
{
    DWORD u = rnd(3) ? FLOATS[rnd(16)] : 0;
    float f;
    if (u || rnd(2)) {
        memcpy(&f, &u, 4);
        return f;
    }
    return (float)((int)rnd(40001) - 20000) / 10000.0f;
}

// The world for 0x4f1050: +0x2bc the levels, +0x2c0 their count, slot 44 the net layer whose slot 23 (0)
// is the local session (+0xdc its level index)
struct FakeWorld {
    DWORD vftable;
    BYTE b[0x300];
};
DWORD g_world_vt[64], g_layer_vt[64];
void *g_layer_obj[2];
BYTE *g_local_session;
DWORD THISCALL world_layer(FakeWorld *) { return ptrv(g_layer_obj); }
DWORD THISCALL layer_player(void *, DWORD i)
{
    ev(0x520, i);
    return ptrv(g_local_session);
}
struct WorldSwap {
    DWORD saved;
    void on(FakeWorld *w)
    {
        saved = *at<DWORD *>(0x6c2080);
        *at<DWORD *>(0x6c2080) = ptrv(w);
    }
    void off() { *at<DWORD *>(0x6c2080) = saved; }
};
FakeWorld *new_world(BYTE *level)
{
    g_world_vt[44] = ptrv((void *)&world_layer);
    g_layer_vt[23] = ptrv((void *)&layer_player);
    g_layer_obj[0] = g_layer_vt;
    FakeWorld *w = (FakeWorld *)arena(sizeof(FakeWorld));
    w->vftable = ptrv(g_world_vt);
    BYTE **levels = (BYTE **)arena(16);
    levels[0] = new_level();
    levels[1] = level;
    *(BYTE ***)(w->b + 0x2bc - 4) = levels;
    *(DWORD *)(w->b + 0x2c0 - 4) = rnd(4) ? 2 : 0;
    g_local_session = (BYTE *)arena(0x100);
    *(DWORD *)(g_local_session + 0xdc) = rnd(4) ? rnd(2) : (rnd(2) ? 0xffffffff : 5);
    return w;
}

// The PlaySound reader of the test (real table vtables, heap data)
PlaySoundReader *new_sound_reader()
{
    auto *r = (PlaySoundReader *)arena(sizeof(PlaySoundReader));
    r->base.vftable = 0x5f927c;
    r->base.name = "test reader";
    r->free_channels = {0x5f9288, nullptr, 0, 0};
    r->playing = {0x5f9288, nullptr, 0, 0};
    return r;
}
void free_tables(PlaySoundReader *r)
{
    ZPtrArray *tables[2] = {&r->free_channels, &r->playing};
    for (ZPtrArray *t : tables) {
        for (DWORD i = 0; i < t->count; i++) {
            auto *e = (SoundEntry *)t->data[i];
            if (e->channels.data)
                heap_free(e->channels.data);
            heap_free(e);
        }
        t->count = 0;
    }
}
void st_table(const ZPtrArray *t)
{
    st(t->count);
    for (DWORD i = 0; i < t->count; i++) {
        const auto *e = (const SoundEntry *)t->data[i];
        st(e->id);
        st(e->channels.vftable);
        st(e->channels.count);
        st(e->channels.capacity);
        for (DWORD k = 0; k < e->channels.count; k++) {
            st(e->channels.data[k].channel);
            st(ptrv(e->channels.data[k].sound));
        }
    }
}

void writers_case(int k)
{
    (void)k;
    ReaderNames names;
    names.save();
    NetPacket *p = (NetPacket *)arena(sizeof(NetPacket));
    packet_ctor(p);
    at<DWORD(THISCALL *)(NetPacket *)>(0x4f7bf0)(p);
    at<DWORD(THISCALL *)(NetPacket *)>(0x4f93e0)(p);
    BYTE *session = (BYTE *)arena(0x100);
    NetPacket *sp = (NetPacket *)(session + 0x3c);
    packet_ctor(sp);
    at<DWORD(THISCALL *)(NetPacket *)>(0x4f93e0)(sp);
    PlaySoundReader *sound = new_sound_reader();
    auto *w = at<NetBlockWriter *>(0x6c2920);
    for (int n = 0, steps = 1 + (int)rnd(10); n < steps; n++) {
        DWORD r = 0;
        switch (rnd(8)) {
        case 0:
            r = at<DWORD(THISCALL *)(void *, NetPacket *, DWORD)>(0x4f7950)(rnd(2) ? at<void *>(0x6c2920) : at<void *>(0x6c2988), p, random());
            break;
        case 1: r = at<DWORD(THISCALL *)(void *, NetPacket *)>(0x4f7bd0)(at<void *>(rnd(2) ? 0x6c2918 : 0x6c2948), p); break;
        case 2: {
            DWORD *bits = (DWORD *)arena(0x40);
            const DWORD nbits = rnd(4) ? rnd(200) : 0;
            DWORD *store = (DWORD *)arena(0x20);
            for (int i = 0; i < 8; i++)
                store[i] = random();
            bits[0] = ptrv(store);
            bits[1] = nbits;
            float v[2] = {random_float(), random_float()};
            r = at<DWORD(THISCALL *)(void *, NetPacket *, void *, const float *, DWORD, DWORD)>(0x4f7a70)(
                at<void *>(0x6c2910), p, bits, v, random(), rnd(2) ? random() & ~0xffu : random() | 1);
            break;
        }
        case 3:
            r = at<DWORD(THISCALL *)(void *, NetPacket *, DWORD, void *)>(0x4f87c0)(at<void *>(0x6c2948), p, random(), new_part());
            break;
        case 4: r = at<DWORD(THISCALL *)(void *, BYTE *, void *)>(0x4f88e0)(at<void *>(0x6c2958), session, new_part()); break;
        case 5: {
            const DWORD kind = rnd(400), x = rnd(4) ? rnd(9000) : random(), y = rnd(4) ? rnd(9000) : random();
            r = at<DWORD(THISCALL *)(void *, BYTE *, void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4f8a40)(
                at<void *>(0x6c2940), session, new_level(), kind, x, y, random(), random(), random());
            break;
        }
        case 6: {
            BYTE *level = new_level();
            FakeWorld *fw = new_world(level);
            float *pos = (float *)arena(8);
            pos[0] = (float)rnd(9000) - 100.0f;
            pos[1] = (float)rnd(9000) - 100.0f;
            WorldSwap ws;
            ws.on(fw);
            r = at<DWORD(THISCALL *)(void *, NetPacket *, DWORD, DWORD, DWORD, const void *)>(0x4f8b90)(
                at<void *>(0x6c2988), p, random(), random(), rnd(500), pos);
            ws.off();
            break;
        }
        default: {
            float *pos = (float *)arena(8);
            pos[0] = (float)rnd(9000);
            pos[1] = (float)rnd(9000);
            const DWORD mode = rnd(4), sid = rnd(4), ch = rnd(4);
            if (mode == 2 && rnd(2))
                at<DWORD(__fastcall *)(ZPtrArray *, DWORD, DWORD, DWORD)>(0x4f91a0)(&sound->free_channels, sid, ch, 0);
            r = at<DWORD(THISCALL *)(void *, NetPacket *, DWORD, void *, DWORD, const void *, DWORD, DWORD, DWORD)>(0x4f8e30)(
                sound, p, random(), rnd(4) ? new_level() : nullptr, sid, pos, mode, random(), ch);
            break;
        }
        }
        ev(0x530, r);
    }
    (void)w;
    st_packet(p);
    st_packet(sp);
    st_table(&sound->free_channels);
    free_tables(sound);
    heap_free(sound->free_channels.data);
    heap_free(sound->playing.data);
    packet_dtor(sp);
    packet_dtor(p);
    names.restore();
}

// ---- Sound tables ------------------------------------------------------------------------------------------

DWORD g_sound_vt[64];
DWORD THISCALL sound_stop(void *s)
{
    ev(0x540, ptrv(s));
    return 0;
}
void *g_sounds[8];
DWORD THISCALL fake_lookup(void *mgr, DWORD id)
{
    ev(0x541, ptrv(mgr), id);
    return id < 8 ? ptrv(g_sounds[id]) : 0;
}
DWORD THISCALL fake_replacement(void *res)
{
    ev(0x542, ptrv(res));
    return ptrv(res);
}

void tables_case(int k)
{
    g_sound_vt[11] = ptrv((void *)&sound_stop);
    for (void *&s : g_sounds) {
        s = arena(16);
        *(DWORD *)s = ptrv(g_sound_vt);
    }
    CodePatch lookup, repl;
    patch_jump(lookup, 0x57e650, (void *)&fake_lookup);
    patch_jump(repl, 0x57b630, (void *)&fake_replacement);
    PlaySoundReader *r = new_sound_reader();
    const DWORD ids = 1 + rnd(5);
    for (int n = 0, steps = 1 + (int)rnd(40); n < steps; n++) {
        ZPtrArray *t = rnd(2) ? &r->playing : &r->free_channels;
        const DWORD id = rnd(ids), ch = rnd(4) ? rnd(5) : random();
        DWORD e = 0;
        switch (rnd(9)) {
        case 0: case 1:
            e = at<DWORD(__fastcall *)(ZPtrArray *, DWORD, DWORD, DWORD)>(0x4f91a0)(t, id, ch, rnd(2) ? random() | 1 : random() & ~0xffu);
            break;
        case 2: e = at<DWORD(__fastcall *)(ZPtrArray *, DWORD, DWORD)>(0x4f9090)(t, id, ch); break;
        case 3: e = at<DWORD(THISCALL *)(void *, DWORD)>(0x4f8ec0)(r, id); break;
        case 4: e = at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4f8f30)(r, id, ch); break;
        case 5: e = at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4f8f90)(r, id, ch); break;
        case 6:
            if (rnd(4) == 0)
                e = at<DWORD(THISCALL *)(void *)>(0x4f8fb0)(r);
            break;
        case 7:
            if (t->count) {
                auto *en = (SoundEntry *)t->data[rnd(t->count)];
                const DWORD want = rnd(3) ? en->channels.capacity + rnd(4) : rnd(3);
                const bool frees = en->channels.capacity < want && en->channels.data;
                e = at<DWORD(THISCALL *)(void *, DWORD)>(0x4f9530)(&en->channels, want);
                // eax is operator delete's when an old block was freed
                if (frees)
                    e = 0xffffffff;
            }
            break;
        default: {
            NetPacket *p = (NetPacket *)arena(sizeof(NetPacket));
            packet_ctor(p);
            e = at<DWORD(THISCALL *)(void *, NetPacket *, DWORD, void *, DWORD, const void *, DWORD, DWORD, DWORD)>(0x4f8e30)(
                r, p, 0, nullptr, id, nullptr, 2, rnd(2), ch);
            st_packet(p);
            packet_dtor(p);
            break;
        }
        }
        // memmove's result is an address in the table's data
        if (t->data && e >= ptrv(t->data) && e < ptrv(t->data) + t->capacity * 4)
            e = 0xc0000000u + (e - ptrv(t->data));
        ev(0x550, e);
    }
    st_table(&r->free_channels);
    st_table(&r->playing);
    free_tables(r);
    ev(0x551, at<DWORD(THISCALL *)(void *, DWORD)>(0x4f8cc0)(r, 0));
    st(r->base.vftable);
    st(ptrv(r->free_channels.data));
    st(r->playing.vftable);
    unpatch(repl);
    unpatch(lookup);
    // NetLocal slot 5 on the game's reader, its tables swapped for test ones
    if (k % 5 == 0) {
        SavedBytes saved;
        save_bytes(saved, READER_PLAY_SOUND, 0x28);
        auto *real = at<PlaySoundReader *>(READER_PLAY_SOUND);
        real->playing = {0x5f9288, nullptr, 0, 0};
        real->free_channels = {0x5f9288, nullptr, 0, 0};
        for (int i = 0, m = (int)rnd(5); i < m; i++)
            at<DWORD(__fastcall *)(ZPtrArray *, DWORD, DWORD, DWORD)>(0x4f91a0)(&real->playing, rnd(3), rnd(3), 0);
        for (DWORD i = 0; i < real->playing.count; i++) {
            auto *en = (SoundEntry *)real->playing.data[i];
            for (DWORD c = 0; c < en->channels.count; c++)
                en->channels.data[c].sound = g_sounds[rnd(8)];
        }
        void *local = arena(16);
        ev(0x552, at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4f7ed0)(local, random(), random()));
        st_table(&real->playing);
        if (real->playing.data)
            heap_free(real->playing.data);
        restore_bytes(saved);
    }
}

// ---- Sessions and the local layer ---------------------------------------------------------------------------

void st_session(const NetSession *s)
{
    st_dwords(s, 0x1c);
    st_zstring((const ZString *)s->base.settings);
    st_bytes((const BYTE *)s + 0x20, 4);
    st_zstring(&s->base.text);
    st_dwords((const BYTE *)s + 0x28, 0x14);
    st_readers(&s->packet1);
    st_readers(&s->packet2);
    st(s->graph.samples);
    st(s->graph.height);
    st(s->graph.unknown10);
    st_dwords(s->unknownc0, 0x28);
    st_zstring(&s->text);
    st(s->control != nullptr);
}

void sessions_case(int k)
{
    ReaderNames names;
    names.save();
    auto *s = (NetSession *)heap_alloc(0xec);
    memset(s, 0xcd, 0xec);
    ev(0x560, ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x4f7c50)(s, rnd(4))) == ptrv(s));
    st_session(s);
    switch (k % 3) {
    case 0: ev(0x561, at<DWORD(THISCALL *)(void *, DWORD)>(0x4f7d00)(s, 1) == ptrv(s)); break;
    case 1:
        ev(0x562, at<DWORD(THISCALL *)(void *, DWORD)>(0x4f7d00)(s, 0) == ptrv(s));
        st(s->base.vftable);
        st(ptrv(s->control));
        heap_free(s);
        break;
    default:
        at<void(THISCALL *)(void *)>(0x4f7d80)(s);
        st(s->base.vftable);
        st(ptrv(s->control));
        heap_free(s);
        break;
    }
    // The local layer
    auto *l = (NetLocal *)heap_alloc(8);
    memset(l, 0xab, 8);
    ev(0x570, at<DWORD(THISCALL *)(void *)>(0x4f7df0)(l) == ptrv(l));
    st_dwords(l, 8);
    const DWORD saved = *at<DWORD *>(G_LOCAL_PLAYER);
    const DWORD v = rnd(3) ? rnd(8) : random();
    ev(0x571, at<DWORD(THISCALL *)(void *, DWORD)>(0x4f7e40)(l, v));
    st_raw(*at<DWORD *>(G_LOCAL_PLAYER));
    *at<DWORD *>(G_LOCAL_PLAYER) = saved;
    switch (rnd(3)) {
    case 0: ev(0x572, at<DWORD(THISCALL *)(void *, DWORD)>(0x4f7e00)(l, 1) == ptrv(l)); break;
    case 1:
        ev(0x573, at<DWORD(THISCALL *)(void *, DWORD)>(0x4f7e00)(l, 0) == ptrv(l));
        st_dwords(l, 8);
        heap_free(l);
        break;
    default:
        at<void(THISCALL *)(void *)>(0x4f7e30)(l);
        st_dwords(l, 8);
        heap_free(l);
        break;
    }
    names.restore();
}

}  // namespace

ZAX_CHECKS(net_session)
{
    (void)cls;
    (void)n;
    g_table = hooks_net_session;
    run_cases("net session: readers and registration", 30, 0x4f7bf0u, [](int k) { readers_case(k); });
    run_cases("net session: block writers", 400, 0x4f7a70u, [](int k) { writers_case(k); });
    run_cases("net session: sound tables", 400, 0x4f91a0u, [](int k) { tables_case(k); });
    run_cases("net session: sessions and local layer", 60, 0x4f7c50u, [](int k) { sessions_case(k); });
}

}  // namespace zax
