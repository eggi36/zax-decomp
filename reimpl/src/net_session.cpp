// The net session, the local net layer and the game's block readers (layouts and what stays with the
// original: net_session.h).
#include "net_session.h"
#include "exe_base.h"

namespace zax {

namespace {

// memmove of the exe (0x5d04b0), returns the target
void *crt_memmove(void *d, const void *s, DWORD n) { return fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }

constexpr DWORD SESSION_VTABLE = 0x5f91c8, LOCAL_VTABLE = 0x5f91d4, CHANNELS_VTABLE = 0x5f9294;
constexpr DWORD TABLE_VTABLE = 0x5f9288, READER_VTABLE = 0x5f1dd0;
// The readers of other subsystems that 0x4f93e0 registers first (thiscall on the packet)
constexpr DWORD OTHER_REGISTRATIONS[] = {0x487f60, 0x48e8b0, 0x49b2f0, 0x492f40, 0x4ee750, 0x4ff4f0, 0x49d390};

DWORD block_type(NetPacket *p, void *reader, DWORD kind)
{
    return fn<DWORD(THISCALL *)(NetPacket *, void *, DWORD)>(0x51ff50)(p, reader, kind);
}
DWORD write_word(NetPacket *p, DWORD v) { return fn<DWORD(THISCALL *)(NetPacket *, DWORD)>(0x5209c0)(p, v); }
DWORD write_dword(NetPacket *p, DWORD v) { return fn<DWORD(THISCALL *)(NetPacket *, DWORD)>(0x520940)(p, v); }
DWORD write_byte(NetPacket *p, DWORD v) { return fn<DWORD(THISCALL *)(NetPacket *, DWORD)>(0x520820)(p, v); }
DWORD write_bool(NetPacket *p, DWORD v) { return fn<DWORD(THISCALL *)(NetPacket *, DWORD)>(0x520a50)(p, v); }
DWORD write_word_token(NetPacket *p, DWORD v, DWORD table)
{
    return fn<DWORD(THISCALL *)(NetPacket *, DWORD, DWORD)>(0x520da0)(p, v, table);
}
// 0x4eae90: a position as two words scaled to the level (thiscall on the level, (packet, pos))
DWORD write_position(void *level, NetPacket *p, const void *pos)
{
    return fn<DWORD(THISCALL *)(void *, NetPacket *, const void *)>(0x4eae90)(level, p, pos);
}
DWORD register_reader(NetPacket *p, DWORD reader, DWORD name)
{
    return fn<DWORD(THISCALL *)(NetPacket *, DWORD, DWORD)>(0x51ff30)(p, reader, name);
}
DWORD ok(DWORD eax) { return (eax & ~0xffu) | 1; }

// The local player's session (0x4f1050, thiscall on the world)
void *world_level() { return fn<void *(THISCALL *)(void *)>(0x4f1050)(*glob<void **>(0x6c2080)); }

// The inlined RemoveAt of the 4-byte and 8-byte arrays: memmove's result, or i + 1 when nothing moves
template <typename T> DWORD remove_at(T *data, DWORD &count, DWORD i)
{
    DWORD r = i + 1;
    if (i + 1 < count)
        r = (DWORD)(uintptr_t)crt_memmove(&data[i], &data[i + 1], (count - (i + 1)) * sizeof(T));
    count--;
    return r;
}

// The inlined destructor of an entry: the channel array's vtable, its data freed, then the entry
void delete_entry(SoundEntry *e)
{
    e->channels.vftable = vt(CHANNELS_VTABLE);
    if (e->channels.data) {
        heap_free(e->channels.data);
        e->channels.data = nullptr;
        e->channels.capacity = 0;
    }
    e->channels.count = 0;
    heap_free(e);
}

DWORD find_entry(const ZPtrArray *t, DWORD id)
{
    DWORD i = 0;
    while (i < t->count && ((SoundEntry *)t->data[i])->id != id)
        i++;
    return i;
}

}  // namespace

// ---- Static readers ----------------------------------------------------------------------------------

BlockReader *__cdecl Reader_ClientSync() { return glob<BlockReader *>(0x6c2930); }
BlockReader *__cdecl Reader_ClientReady() { return glob<BlockReader *>(0x6c2920); }
BlockReader *__cdecl Reader_ClientConfigControl() { return glob<BlockReader *>(0x6c2910); }
BlockReader *__cdecl Reader_ClientFrameRecieved() { return glob<BlockReader *>(0x6c2918); }
BlockReader *__cdecl Reader_ClientInvalidateRect() { return glob<BlockReader *>(0x6c2948); }
BlockReader *__cdecl Reader_ClientSetLayerPartFlag() { return glob<BlockReader *>(0x6c2958); }
BlockReader *__cdecl Reader_ServerExplosion() { return glob<BlockReader *>(0x6c2940); }
BlockReader *__cdecl Reader_ClientPlaySimpleSound() { return glob<BlockReader *>(0x6c2988); }
BlockReader *__cdecl Reader_ClientPlaySound() { return glob<BlockReader *>(0x6c2960); }
BlockReader *__cdecl Reader_RequestSyncFromClients() { return glob<BlockReader *>(0x6c2950); }

DWORD THISCALL NetReaderRegistry::RegisterClientReaders()
{
    register_reader(&packet, glob_addr(0x6c2920), glob_addr(0x6188b0));           // "CClientReadyBlockReader"
    register_reader(&packet, glob_addr(0x6c2930), glob_addr(0x618898));           // "CClientSyncBlockReader"
    register_reader(&packet, glob_addr(0x6c2910), glob_addr(0x6188c8));           // "CClientConfigControlBlockReader"
    register_reader(&packet, glob_addr(0x6c2918), glob_addr(0x6188e8));           // "CClientFrameRecievedBlockReader"
    return register_reader(&packet, glob_addr(0x6c2928), glob_addr(0x618884));    // "CSetZoomBlockReader"
}

// The registrations of other subsystems first (their own readers), then the server's
DWORD THISCALL NetReaderRegistry::RegisterServerReaders()
{
    for (DWORD f : OTHER_REGISTRATIONS)
        fn_dyn<void(THISCALL *)(NetPacket *)>(f)(&packet);
    register_reader(&packet, glob_addr(0x6c2958), glob_addr(0x618944));           // "CClientSetLayerPartFlagBlockReader"
    register_reader(&packet, glob_addr(0x6c2950), glob_addr(0x6189f4));           // "CRequestSyncFromClientsBlockReader"
    register_reader(&packet, glob_addr(0x6c2940), glob_addr(0x618968));           // "CServerTellsClientToCreateExplosionBlockReader"
    register_reader(&packet, glob_addr(0x6c2988), glob_addr(0x618998));           // "CClientPlaySimpleSoundBlockReader"
    register_reader(&packet, glob_addr(0x6c2960), glob_addr(0x6189bc));           // "CClientPlaySoundBlockReader"
    register_reader(&packet, glob_addr(0x6c2948), glob_addr(0x618920));           // "CClientInvalidateRectBlockReader"
    return register_reader(&packet, glob_addr(0x6c2990), glob_addr(0x6189d8));    // "CClientPlayVOBlockReader"
}

// ---- Write functions -----------------------------------------------------------------------------------

DWORD THISCALL NetBlockWriter::WriteReady(NetPacket *p, DWORD) { return ok(block_type(p, this, 2)); }

DWORD THISCALL NetBlockWriter::WriteFrameRecieved(NetPacket *p) { return ok(block_type(p, this, 2)); }

// Two floats in [-1, 1] as words: (v + 1) * 65535 * 0.5 on the FPU, below 0 (or NaN) 0, above 65535
// 65535, truncated by __ftol. The block kind is 0 (reliable) when the last argument is set, else 2.
// Then the bit set (0x46ef00: its dwords) and the flag.
DWORD THISCALL NetBlockWriter::WriteConfigControl(NetPacket *p, void *bits, const float *v, DWORD flag, DWORD reliable)
{
    block_type(p, this, (reliable & 0xff) ? 0 : 2);
    for (int k = 0; k < 2; k++) {
        long double x = (long double)v[k];
        x = x - (long double)*glob<const float *>(0x5eeccc);   // -1.0
        x = x * (long double)*glob<const float *>(0x5f91b8);   // 65535.0
        x = x * (long double)*glob<const float *>(0x5eee2c);   // 0.5
        if (x87_lt(x, (long double)*glob<const float *>(0x5ebed0)))
            x = (long double)*glob<const float *>(0x5ebed0);
        else if (!x87_le(x, (long double)*glob<const float *>(0x5f91b8)))
            x = (long double)*glob<const float *>(0x5f91b8);
        write_word(p, (DWORD)x87_ftol(x));
    }
    fn<DWORD(THISCALL *)(void *, NetPacket *)>(0x46ef00)(bits, p);
    return ok(write_bool(p, flag));
}

// Nothing when the part's +0x48 is 0 or its slot 37 gives no rectangle (al = 0); otherwise the 16-byte
// rectangle
DWORD THISCALL NetBlockWriter::WriteInvalidateRect(NetPacket *p, DWORD, void *part)
{
    if (!((DWORD *)part)[0x12])
        return 0;
    DWORD rect[4];
    const DWORD r = vcall<DWORD>(part, 37, rect, 0, 0);
    if (!(r & 0xff))
        return r;
    block_type(p, this, 0);
    return ok(fn<DWORD(THISCALL *)(NetPacket *, const DWORD *)>(0x5208a0)(p, rect));
}

// Into the session's first packet: the part's word +0x28 (0: nothing, al = 0 with the session's upper
// bytes), the 8 bytes of its slot 43 and its dword +0x1c (nothing either when its +0x48 is 0)
DWORD THISCALL NetBlockWriter::WriteSetLayerPartFlag(BYTE *session, void *part)
{
    NetPacket *p = (NetPacket *)(session + 0x3c);
    BYTE *b = (BYTE *)part;
    if (!*(WORD *)(b + 0x28))
        return (DWORD)(uintptr_t)session & ~0xffu;
    if (!*(DWORD *)(b + 0x48))
        return 0;
    DWORD v[2];
    vcall<void>(part, 43, v);
    block_type(p, this, 0);
    write_word(p, *(WORD *)(b + 0x28));
    write_dword(p, v[0]);
    write_dword(p, v[1]);
    return ok(write_dword(p, *(DWORD *)(b + 0x1c)));
}

// Into the session's first packet: the kind as a token of the list 0x6cfdd8, the position (x, y)
// through the level, a word, a bool and a dword
DWORD THISCALL NetBlockWriter::WriteExplosion(BYTE *session, void *level, DWORD kind, DWORD x, DWORD y, DWORD a,
                                              DWORD flag, DWORD b)
{
    NetPacket *p = (NetPacket *)(session + 0x3c);
    block_type(p, this, 3);
    write_word_token(p, kind, glob_addr(0x6cfdd8));
    DWORD pos[2] = {x, y};
    write_position(level, p, pos);
    write_word(p, a);
    write_bool(p, flag);
    return ok(write_dword(p, b));
}

// The sound as a token of the sound manager, the position through the local player's level (-1 when
// there is none)
DWORD THISCALL NetBlockWriter::WritePlaySimpleSound(NetPacket *p, DWORD, DWORD, DWORD sound, const void *pos)
{
    block_type(p, this, 3);
    void *level = world_level();
    write_word_token(p, sound, glob_addr(SOUND_MANAGER));
    if (level)
        return ok(write_position(level, p, pos));
    return ok(write_dword(p, 0xffffffff));
}

// Mode 2 first takes the channel out of the free channel table. The sound as a token, the position
// through the level (-1 without one), a byte (flag != 0) | mode << 1 and the channel.
DWORD THISCALL PlaySoundReader::Write(NetPacket *p, DWORD, void *level, DWORD sound, const void *pos, DWORD mode,
                                      DWORD flag, DWORD channel)
{
    if (mode == 2)
        SoundTable_Remove(&free_channels, sound, channel);
    block_type(p, this, 0);
    write_word_token(p, sound, glob_addr(SOUND_MANAGER));
    if (level)
        write_position(level, p, pos);
    else
        write_dword(p, 0xffffffff);
    write_byte(p, (BYTE)(((flag & 0xff) != 0) | (BYTE)(mode << 1)));
    return ok(write_byte(p, channel));
}

// ---- The PlaySound reader's tables -----------------------------------------------------------------------

PlaySoundReader *THISCALL PlaySoundReader::DeletingDtor(DWORD flags)
{
    playing.vftable = vt(TABLE_VTABLE);
    if (playing.data) {
        heap_free(playing.data);
        playing.data = nullptr;
        playing.capacity = 0;
    }
    playing.count = 0;
    free_channels.vftable = vt(TABLE_VTABLE);
    if (free_channels.data) {
        heap_free(free_channels.data);
        free_channels.data = nullptr;
        free_channels.capacity = 0;
    }
    free_channels.count = 0;
    base.vftable = vt(READER_VTABLE);
    if (flags & 1)
        heap_free(this);
    return this;
}

// Capacity n: the elements copied into a new block (8 bytes each), the old one freed. eax: n when
// nothing grows, otherwise 0 without an old block (or operator delete's).
DWORD THISCALL SoundChannels::Reserve(DWORD n)
{
    if (capacity >= n)
        return n;
    capacity = n;
    auto *d = (SoundChannel *)heap_alloc(n * 8);
    if (data)
        for (DWORD i = 0; i < count; i++)
            d[i] = data[i];
    DWORD r = 0;
    if (data) {
        heap_free(data);
        r = 0;
    }
    data = d;
    return r;
}

// The entry of the id (made and appended when missing; the id is set after the append), then the
// channel unless it is there (eax: its index). A new channel grows the array to max(count + 1,
// capacity * 1.2) and gets the sound of the id from the sound manager (0x57e650, then its
// replacement 0x57b630) when lookup is set, else none (eax: the old count * 8).
DWORD __fastcall SoundTable_Add(ZPtrArray *table, DWORD id, DWORD channel, DWORD lookup)
{
    DWORD i = find_entry(table, id);
    if (i >= table->count) {
        auto *e = (SoundEntry *)heap_alloc(0x14);
        if (e) {
            e->channels.vftable = vt(CHANNELS_VTABLE);
            e->channels.data = nullptr;
            e->channels.count = 0;
            e->channels.capacity = 0;
        }
        i = vcall<DWORD>(table, 2, &e);
        ((SoundEntry *)table->data[i])->id = id;
    }
    SoundChannels *c = &((SoundEntry *)table->data[i])->channels;
    const DWORD n = c->count;
    for (DWORD k = 0; k < n; k++)
        if (c->data[k].channel == (BYTE)channel)
            return k;
    DWORD want = n + 1;
    if (c->capacity < want) {
        unsigned long long cap = c->capacity;
        long double g = (long double)cap * (long double)*glob<const float *>(0x5ea5f4);   // 1.2f
        const DWORD v = (DWORD)x87_ftol(g);
        if (v > want)
            want = v;
        c->Reserve(want);
    }
    c->count++;
    ((SoundEntry *)table->data[i])->channels.data[n].channel = (BYTE)channel;
    SoundChannel *slot = &((SoundEntry *)table->data[i])->channels.data[n];
    if (lookup & 0xff) {
        void *r = fn<void *(THISCALL *)(void *, DWORD)>(0x57e650)(glob<void *>(SOUND_MANAGER), id);
        r = fn<void *(THISCALL *)(void *)>(0x57b630)(r);
        slot->sound = r;
        return (DWORD)(uintptr_t)r;
    }
    slot->sound = nullptr;
    return n * 8;
}

// The channel taken out of the id's entry; an entry left without channels is deleted and removed.
// eax: the table count (no entry), the channel count (no channel), the new channel count, or the
// removal's (memmove's result or the index + 1).
DWORD __fastcall SoundTable_Remove(ZPtrArray *table, DWORD id, DWORD channel)
{
    const DWORD i = find_entry(table, id);
    if (i >= table->count)
        return table->count;
    SoundEntry *e = (SoundEntry *)table->data[i];
    DWORD k = 0;
    while (k < e->channels.count && e->channels.data[k].channel != (BYTE)channel)
        k++;
    if (k >= e->channels.count)
        return e->channels.count;
    remove_at(e->channels.data, e->channels.count, k);
    e = (SoundEntry *)table->data[i];
    if (e->channels.count)
        return e->channels.count;
    if (e)
        delete_entry(e);
    if (i >= table->count)
        return 0;   // not reached: the entry was found below the count
    return remove_at(table->data, table->count, i);
}

// The lowest channel not in the id's free channel entry (the scan starts over after every hit, but at
// the second element, as in the original), added to it; al = that channel, the upper bytes Add's
DWORD THISCALL PlaySoundReader::FreeChannel(DWORD id)
{
    const DWORD i = find_entry(&free_channels, id);
    BYTE ch = 0;
    if (i < free_channels.count) {
        SoundEntry *e = (SoundEntry *)free_channels.data[i];
        const DWORD n = e->channels.count;
        for (DWORD k = 0; k < n; k++)
            if (e->channels.data[k].channel == ch) {
                ch++;
                k = 0;
            }
    }
    // The channel argument is the dword of the id's stack slot with the low byte replaced
    const DWORD r = SoundTable_Add(&free_channels, id, (id & ~0xffu) | ch, 0);
    return (r & ~0xffu) | ch;
}

void *THISCALL PlaySoundReader::SoundAt(DWORD id, DWORD channel)
{
    const DWORD i = find_entry(&playing, id);
    if (i >= playing.count)
        return nullptr;
    SoundEntry *e = (SoundEntry *)playing.data[i];
    for (DWORD k = 0; k < e->channels.count; k++)
        if (e->channels.data[k].channel == (BYTE)channel)
            return e->channels.data[k].sound;
    return nullptr;
}

DWORD THISCALL PlaySoundReader::Remove(DWORD id, DWORD channel)
{
    return fn<DWORD(__fastcall *)(ZPtrArray *, DWORD, DWORD)>(0x4f9090)(&playing, id, channel);
}

// Every playing sound stopped (slot 11), then all entries deleted from the last
DWORD THISCALL PlaySoundReader::StopAll()
{
    for (DWORD i = 0; i < playing.count; i++)
        for (DWORD k = 0; k < ((SoundEntry *)playing.data[i])->channels.count; k++)
            if (void *s = ((SoundEntry *)playing.data[i])->channels.data[k].sound)
                vcall<void>(s, 11);
    while (playing.count) {
        const DWORD i = playing.count - 1;
        if (auto *e = (SoundEntry *)playing.data[i])
            delete_entry(e);
        if (i < playing.count)
            remove_at(playing.data, playing.count, i);
    }
    return 0;
}

// ---- Net session and local net layer ---------------------------------------------------------------------

NetSession *THISCALL NetSession::Ctor(DWORD arg)
{
    fn<void(THISCALL *)(void *)>(0x51f780)(this);
    fn<void(THISCALL *)(void *)>(0x51fe60)(&packet1);
    fn<void(THISCALL *)(void *)>(0x51fe60)(&packet2);
    fn<void(THISCALL *)(void *)>(0x51f8e0)(&graph);
    fn<void(THISCALL *)(void *)>(0x4dec90)(&text);
    base.vftable = vt(SESSION_VTABLE);
    for (DWORD &d : unknownc0)
        d = 0;
    *(DWORD *)&unknownd8 = 0x3f800000;
    unknowndc = 0;
    flag39 = 0;
    flag38 = 0;
    unknowne0 = 0;
    void *world = *glob<void **>(0x6c2080);
    vcall<void>(world, 106, &packet1, &packet2);
    control = vcall<void *>(*glob<void **>(0x6c2080), 41, arg);
    return this;
}

void THISCALL NetSession::Dtor()
{
    void *c = control;
    base.vftable = vt(SESSION_VTABLE);
    if (c) {
        fn<void(THISCALL *)(void *)>(0x46f2f0)(c);
        pool_free(c, 0x1c8);
        control = nullptr;
    }
    fn<void(THISCALL *)(void *)>(0x4defd0)(&text);
    fn<void(THISCALL *)(void *)>(0x51f960)(&graph);
    fn<void(THISCALL *)(void *)>(0x51fec0)(&packet2);
    fn<void(THISCALL *)(void *)>(0x51fec0)(&packet1);
    fn<void(THISCALL *)(void *)>(0x51f830)(this);
}

NetSession *THISCALL NetSession::DeletingDtor(DWORD flags)
{
    Dtor();
    if (flags & 1)
        heap_free(this);
    return this;
}

NetLocal *THISCALL NetLocal::Ctor()
{
    game = nullptr;
    vftable = vt(LOCAL_VTABLE);
    return this;
}

NetLocal *THISCALL NetLocal::DeletingDtor(DWORD flags)
{
    vftable = vt(LOCAL_VTABLE);
    fn<void(THISCALL *)(void *)>(0x51ec40)(this);
    if (flags & 1)
        heap_free(this);
    return this;
}

void THISCALL NetLocal::Dtor()
{
    vftable = vt(LOCAL_VTABLE);
    fn<void(THISCALL *)(void *)>(0x51ec40)(this);
}

DWORD THISCALL NetLocal::SetLocalPlayer(DWORD i)
{
    *glob<DWORD *>(G_LOCAL_PLAYER) = i;
    return i;
}

DWORD THISCALL NetLocal::StopSounds(DWORD, DWORD)
{
    auto *r = fn<PlaySoundReader *(__cdecl *)()>(0x4f8d30)();
    return fn<DWORD(THISCALL *)(PlaySoundReader *)>(0x4f8fb0)(r);
}

ZAX_HOOKS(net_session)
{
    const Hook list[] = {
        {0x4f78a0, (void *)&Reader_ClientSync, "Reader_ClientSync", 5, {0xb8, 0x30, 0x29, 0x6c, 0x00, 0xc3, 0x90, 0x90}},
        {0x4f7910, (void *)&Reader_ClientReady, "Reader_ClientReady", 5, {0xb8, 0x20, 0x29, 0x6c, 0x00, 0xc3, 0x90, 0x90}},
        {0x4f7950, ptr(&NetBlockWriter::WriteReady), "NetBlockWriter::WriteReady", 7, {0x6a, 0x02, 0x51, 0x8b, 0x4c, 0x24, 0x0c, 0xe8}},
        {0x4f79a0, (void *)&Reader_ClientConfigControl, "Reader_ClientConfigControl", 5, {0xb8, 0x10, 0x29, 0x6c, 0x00, 0xc3, 0x90, 0x90}},
        {0x4f7a70, ptr(&NetBlockWriter::WriteConfigControl), "NetBlockWriter::WriteConfigControl", 5, {0x8a, 0x44, 0x24, 0x14, 0x56, 0xf6, 0xd8, 0x8b}},
        {0x4f7b80, (void *)&Reader_ClientFrameRecieved, "Reader_ClientFrameRecieved", 5, {0xb8, 0x18, 0x29, 0x6c, 0x00, 0xc3, 0x90, 0x90}},
        {0x4f7bd0, ptr(&NetBlockWriter::WriteFrameRecieved), "NetBlockWriter::WriteFrameRecieved", 7, {0x6a, 0x02, 0x51, 0x8b, 0x4c, 0x24, 0x0c, 0xe8}},
        {0x4f7bf0, ptr(&NetReaderRegistry::RegisterClientReaders), "NetPacket::RegisterClientReaders", 6, {0x56, 0x68, 0xb0, 0x88, 0x61, 0x00, 0x8b, 0xf1}},
        {0x4f7c50, ptr(&NetSession::Ctor), "NetSession::NetSession", 5, {0x53, 0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x26, 0x7b}},
        {0x4f7d00, ptr(&NetSession::DeletingDtor), "NetSession::DeletingDtor", 10, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0xbe, 0xe8, 0x00}},
        {0x4f7d80, ptr(&NetSession::Dtor), "NetSession::~NetSession", 10, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0xbe, 0xe8, 0x00}},
        {0x4f7df0, ptr(&NetLocal::Ctor), "NetLocal::NetLocal", 9, {0x8b, 0xc1, 0xc7, 0x40, 0x04, 0x00, 0x00, 0x00}},
        {0x4f7e00, ptr(&NetLocal::DeletingDtor), "NetLocal::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xd4, 0x91, 0x5f}},
        {0x4f7e30, ptr(&NetLocal::Dtor), "NetLocal::~NetLocal", 6, {0xc7, 0x01, 0xd4, 0x91, 0x5f, 0x00, 0xe9, 0x05}},
        {0x4f7e40, ptr(&NetLocal::SetLocalPlayer), "NetLocal::SetLocalPlayer", 9, {0x8b, 0x44, 0x24, 0x04, 0xa3, 0x38, 0x29, 0x6c}},
        {0x4f7ed0, ptr(&NetLocal::StopSounds), "NetLocal::StopSounds", 5, {0xe8, 0x5b, 0x0e, 0x00, 0x00, 0x8b, 0xc8, 0xe8}, 1},
        {0x4f8770, (void *)&Reader_ClientInvalidateRect, "Reader_ClientInvalidateRect", 5, {0xb8, 0x48, 0x29, 0x6c, 0x00, 0xc3, 0x90, 0x90}},
        {0x4f87c0, ptr(&NetBlockWriter::WriteInvalidateRect), "NetBlockWriter::WriteInvalidateRect", 6, {0x83, 0xec, 0x10, 0x56, 0x8b, 0xf1, 0x8b, 0x4c}},
        {0x4f8850, (void *)&Reader_ClientSetLayerPartFlag, "Reader_ClientSetLayerPartFlag", 5, {0xb8, 0x58, 0x29, 0x6c, 0x00, 0xc3, 0x90, 0x90}},
        {0x4f88e0, ptr(&NetBlockWriter::WriteSetLayerPartFlag), "NetBlockWriter::WriteSetLayerPartFlag", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xec, 0x08, 0x53}},
        {0x4f8990, (void *)&Reader_ServerExplosion, "Reader_ServerExplosion", 5, {0xb8, 0x40, 0x29, 0x6c, 0x00, 0xc3, 0x90, 0x90}},
        {0x4f8a40, ptr(&NetBlockWriter::WriteExplosion), "NetBlockWriter::WriteExplosion", 7, {0x83, 0xec, 0x08, 0x8b, 0x44, 0x24, 0x0c, 0x56}},
        {0x4f8af0, (void *)&Reader_ClientPlaySimpleSound, "Reader_ClientPlaySimpleSound", 5, {0xb8, 0x88, 0x29, 0x6c, 0x00, 0xc3, 0x90, 0x90}},
        {0x4f8b90, ptr(&NetBlockWriter::WritePlaySimpleSound), "NetBlockWriter::WritePlaySimpleSound", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x6a, 0x03}},
        {0x4f8cc0, ptr(&PlaySoundReader::DeletingDtor), "PlaySoundReader::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b, 0x46}},
        {0x4f8d30, (void *)&Reader_ClientPlaySound, "Reader_ClientPlaySound", 5, {0xb8, 0x60, 0x29, 0x6c, 0x00, 0xc3, 0x90, 0x90}},
        {0x4f8e30, ptr(&PlaySoundReader::Write), "PlaySoundReader::Write", 5, {0x53, 0x8b, 0x5c, 0x24, 0x1c, 0x55, 0x8b, 0x6c}},
        {0x4f8ec0, ptr(&PlaySoundReader::FreeChannel), "PlaySoundReader::FreeChannel", 5, {0x8b, 0x54, 0x24, 0x04, 0x53, 0x56, 0x8b, 0x71}},
        {0x4f8f30, ptr(&PlaySoundReader::SoundAt), "PlaySoundReader::SoundAt", 5, {0x53, 0x56, 0x8b, 0x71, 0x20, 0x33, 0xc0, 0x33}},
        {0x4f8f90, ptr(&PlaySoundReader::Remove), "PlaySoundReader::Remove", 8, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x54, 0x24, 0x04}},
        {0x4f8fb0, ptr(&PlaySoundReader::StopAll), "PlaySoundReader::StopAll", 6, {0x53, 0x8b, 0xd9, 0x55, 0x33, 0xed, 0x8b, 0x43}},
        {0x4f9090, (void *)&SoundTable_Remove, "SoundTable_Remove", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0xf2, 0x8b, 0xd1}},
        {0x4f91a0, (void *)&SoundTable_Add, "SoundTable_Add", 5, {0x83, 0xec, 0x14, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4f9390, (void *)&Reader_RequestSyncFromClients, "Reader_RequestSyncFromClients", 5, {0xb8, 0x50, 0x29, 0x6c, 0x00, 0xc3, 0x90, 0x90}},
        {0x4f93e0, ptr(&NetReaderRegistry::RegisterServerReaders), "NetPacket::RegisterServerReaders", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x78, 0xeb, 0xf8, 0xff}, 4},
        {0x4f9530, ptr(&SoundChannels::Reserve), "SoundChannels::Reserve", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x39}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
