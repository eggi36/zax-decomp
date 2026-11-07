// Batch C4 (phase 5a), multiplayer: the read slots (slot 2) of the game's block readers (net_session.h,
// their Write functions and accessors are there), the local net layer's slots (NetLocal, vtable
// 0x5f91d4, shared with ZaxNetGame 0x602fa8), ZaxNetGame's own slots 5, 6, 7, 10 and 27, the exit
// function of the play-sound reader, the GameSpy text callbacks of qr_init, CZaxWorld's slot 87, and
// the registration of the save game's name callbacks (0x4f8720 with 0x4f86b0).
//
// Block readers (slot 2, thiscall reader (packet)) [confirmed: code, the reader names]:
//   0x4f7820 (vtable 0x5f9184): the local player's object (net layer slot 23 of slot 28) gets the
//     dword read at +0xe0                         0x4f78b0 CClientSyncBlockReader: world slot 50 (player)
//   0x4f7920 CClientReadyBlockReader: world slot 49 (player)
//   0x4f79b0 CClientConfigControlBlockReader: the sender's control (+0xe8 of net layer slot 29) gets two
//     words as a vector (w * 2 / 0x5f91b4 - 1.0f), its bit set read, the time and +0x1c5 = 0, then
//     the sender's +0x20 a bool
//   0x4f7b90 CClientFrameRecievedBlockReader: the sender's frames in flight (+0x34) up to 5
//   0x4f8780 CClientInvalidateRectBlockReader: a rectangle released in the current layer's tile map
//   0x4f8860 CClientSetLayerPartFlagBlockReader: the part with that id (CSortList2D_FindId) gets slot 60
//   0x4f89a0 CServerTellsClientToCreateExplosionBlockReader: World::SpawnExplosion
//   0x4f8b00 CClientPlaySimpleSoundBlockReader / 0x4f8d40 CClientPlaySoundBlockReader: the sound played
//     at the position (or none without a layer: then a dword is skipped); the full one also keeps the
//     sound's channel in its playing table (SoundTable_Add)
//   0x4f9320 CClientPlayVOBlockReader: the voice-over played when "Play Voiceovers" is on
//   0x4f93a0 CRequestSyncFromClientsBlockReader: a sync block sent to the server, the session's +0x38 = 1
//   0x49cd30 (vtable 0x5f3a98): a new object of 0x13c bytes (CEntityWalking's constructor, vtable
//     0x5f3aa4, +0x134/+0x138 = -1) reads itself (slot 27) and becomes the last light beam
// Where the original passes dwords of which a read only set the low byte or word (the bool of the
// explosion, the channel of the sound) the upper bytes are stack garbage of the original; the
// reimplementation passes 0 there. The play-sound reader's "keep" argument is the packet argument's stack
// slot with its low byte replaced, as in the original.
//
// Not hookable: 0x4f7ef0 (slot 11 of NetLocal: mov ecx, [ecx+4]; jmp NetGame_PlayerCount, a relative
// jump in the first 5 bytes) and 0x4f8700 (xor al, al; ret: 3 bytes, the save game's third callback).
//
// Return values are what the original leaves in eax (the exe functions called through their addresses).
#include "exe_base.h"
#include "init_list.h"
#include "x87.h"

namespace zax {
namespace {

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
DWORD as_dw(const void *p) { return (DWORD)(uintptr_t)p; }
DWORD with_al(DWORD upper, DWORD al) { return (upper & ~0xffu) | (al & 0xff); }

void *world() { return *glob<void **>(0x6c2080); }
void *app() { return *glob<void **>(0x713f14); }
void *net_layer() { return vcall<void *>(world(), 44); }
BYTE *current_layer() { return fn<BYTE *(THISCALL *)(void *)>(0x4f1050)(world()); }

DWORD read_byte(void *p, void *out) { return fn<DWORD(THISCALL *)(void *, void *)>(0x520490)(p, out); }
DWORD read16(void *p, void *out) { return fn<DWORD(THISCALL *)(void *, void *)>(0x5204c0)(p, out); }
DWORD read_word(void *p, void *out) { return fn<DWORD(THISCALL *)(void *, void *)>(0x520510)(p, out); }
DWORD read_dword(void *p, void *out) { return fn<DWORD(THISCALL *)(void *, void *)>(0x520550)(p, out); }
DWORD read_bool(void *p, void *out) { return fn<DWORD(THISCALL *)(void *, void *)>(0x520580)(p, out); }
DWORD read_dword_token(void *p, void *out, DWORD table) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x5205d0)(p, out, table); }
DWORD read_word_token(void *p, void *out, DWORD table) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x520670)(p, out, table); }
DWORD write_bytes(void *p, DWORD a, DWORD b) { return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x5203d0)(p, a, b); }
BYTE *player_at(void *game, DWORD i) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x51f440)(game, i); }
// CLayer::ReadPosition (packet, pos, ok byte) and CLayer::PlaySoundHere (sound, pos, mode, keep, channel)
DWORD read_position(void *layer, void *p, void *pos, void *ok) { return fn<DWORD(THISCALL *)(void *, void *, void *, void *)>(0x4eaf80)(layer, p, pos, ok); }
DWORD play_sound_here(void *layer, DWORD sound, void *pos, DWORD mode, DWORD keep, DWORD channel)
{
    return fn<DWORD(THISCALL *)(void *, DWORD, void *, DWORD, DWORD, DWORD)>(0x4eaca0)(layer, sound, pos, mode, keep, channel);
}
DWORD zs_ctor(void *z) { return fn<DWORD(THISCALL *)(void *)>(0x4dec90)(z); }
DWORD zs_copy(void *z, const void *from) { return fn<DWORD(THISCALL *)(void *, const void *)>(0x4dedf0)(z, from); }
DWORD zs_dtor(void *z) { return fn<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }
const char *zs_cstr(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
DWORD zs_length(const void *z) { return fn<DWORD(THISCALL *)(const void *)>(0x4e13b0)(z); }
DWORD is_kind_of(void *o, void *cls) { return fn<DWORD(THISCALL *)(void *, void *)>(0x416790)(o, cls); }
void *class_of(DWORD getter) { return fn_dyn<void *(__cdecl *)()>(getter)(); }

// ---- Block readers -------------------------------------------------------------------------------------

// 0x4f7820. The dword's local is the saved ecx (the reader) until the read sets it.
DWORD THISCALL Reader5f9184_Read(void *self, void *packet)
{
    const DWORD id = vcall<DWORD>(net_layer(), 28);
    BYTE *o = vcall<BYTE *>(net_layer(), 23, id);
    DWORD v = as_dw(self);
    const DWORD r = read_dword(packet, &v);
    dw(o, 0xe0) = v;
    return r;
}
// 0x4f78b0 / 0x4f7920
DWORD THISCALL ClientSync_Read(void *, void *) { return vcall<DWORD>(world(), 50, vcall<DWORD>(net_layer(), 28)); }
DWORD THISCALL ClientReady_Read(void *, void *) { return vcall<DWORD>(world(), 49, vcall<DWORD>(net_layer(), 28)); }
// 0x4f79b0. The words go into the packet argument's stack slot (its upper half from the pointer, masked).
DWORD THISCALL ClientConfigControl_Read(void *, void *packet)
{
    BYTE *sender = vcall<BYTE *>(net_layer(), 29);
    BYTE *control = pt(sender, 0xe8);
    DWORD slot = as_dw(packet);
    read_word(packet, &slot);
    slot &= 0xffff;
    float v[2];
    v[0] = fstp((fild((int)slot) + fild((int)slot)) / cst(glob_addr(0x5f91b4)) - cst(glob_addr(0x5ec3f4)));
    read_word(packet, &slot);
    slot &= 0xffff;
    v[1] = fstp((fild((int)slot) + fild((int)slot)) / cst(glob_addr(0x5f91b4)) - cst(glob_addr(0x5ec3f4)));
    fn<DWORD(THISCALL *)(void *, float *)>(0x470030)(control, v);
    fn<DWORD(THISCALL *)(void *, void *)>(0x46ee50)(control + 0xc, packet);
    const DWORD tick = fn<DWORD(__cdecl *)()>(0x4d2460)();
    dw(control, 0x58) = tick;
    control[0x1c5] = 0;
    return read_bool(packet, sender + 0x20);
}
// 0x4f7b90. eax: the player.
DWORD THISCALL ClientFrameRecieved_Read(void *, void *)
{
    const DWORD id = vcall<DWORD>(net_layer(), 28);
    BYTE *p = player_at(vcall<void *>(world(), 1), id);
    if ((int)dw(p, 0x34) < 5)
        dw(p, 0x34)++;
    return as_dw(p);
}
// 0x4f8780
DWORD THISCALL ClientInvalidateRect_Read(void *, void *packet)
{
    BYTE *layer = current_layer();
    DWORD rect[4];
    DWORD r = read16(packet, rect);
    if (layer)
        r = fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x540640)(pt(layer, 0x7c), rect, 1);
    return r;
}
// 0x4f8860
DWORD THISCALL ClientSetLayerPartFlag_Read(void *, void *packet)
{
    BYTE *layer = current_layer();
    DWORD id = 0, key[2], flag;
    read_word(packet, &id);
    read_dword(packet, &key[0]);
    read_dword(packet, &key[1]);
    DWORD r = read_dword(packet, &flag);
    if (!layer)
        return r;
    void *part = fn<void *(THISCALL *)(void *, void *, DWORD)>(0x57a9a0)(layer + 0x50, key, id & 0xffff);
    if (!part)
        return 0;
    return vcall<DWORD>(part, 60, flag);
}
// 0x4f89a0
DWORD THISCALL ServerExplosion_Read(void *, void *packet)
{
    BYTE *layer = current_layer();
    DWORD token = 0, flag = 0, extra;
    float pos[2];
    WORD w;
    read_word_token(packet, &token, glob_addr(0x6cfdd8));
    read_position(layer, packet, pos, nullptr);
    read_word(packet, &w);
    read_bool(packet, &flag);
    read_dword(packet, &extra);
    return fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, int, DWORD, DWORD)>(0x4f6320)(
        world(), layer, token & 0xffff, *reinterpret_cast<DWORD *>(&pos[0]), *reinterpret_cast<DWORD *>(&pos[1]), (short)w, flag, extra);
}
// 0x4f8b00. Without a layer the position's dword is skipped (its local set to -1 first).
DWORD THISCALL ClientPlaySimpleSound_Read(void *, void *packet)
{
    BYTE *layer = current_layer();
    DWORD token = 0;
    read_word_token(packet, &token, glob_addr(0x713708));
    DWORD slot = as_dw(packet);
    if (!layer) {
        slot = 0xffffffff;
        return read_dword(packet, &slot);
    }
    float pos[2];
    read_position(layer, packet, pos, &slot);
    return play_sound_here(layer, token & 0xffff, (BYTE)slot ? pos : nullptr, 0, 0, 0xff);
}
// 0x4f8d40 (this: the reader, its playing table at +0x18)
DWORD THISCALL ClientPlaySound_Read(BYTE *self, void *packet)
{
    BYTE *layer = current_layer();
    DWORD token = 0;
    read_word_token(packet, &token, glob_addr(0x713708));
    BYTE has_pos = 0;
    float pos[2];
    DWORD slot = as_dw(packet);
    if (layer)
        read_position(layer, packet, pos, &has_pos);
    else {
        slot = 0xffffffff;
        read_dword(packet, &slot);
    }
    DWORD mode_byte = 0;
    read_byte(packet, &mode_byte);
    const DWORD mode = (mode_byte & 0xff) >> 1;
    slot = (slot & ~0xffu) | (mode_byte & 1);
    DWORD channel = 0;
    read_byte(packet, &channel);
    if ((BYTE)channel != 0xff)
        fn<DWORD(__fastcall *)(void *, DWORD, DWORD, DWORD)>(0x4f91a0)(self + 0x18, token & 0xffff, channel, 1);
    if (!layer)
        return channel;   // eax: the channel's dword (reloaded after the add)
    return play_sound_here(layer, token & 0xffff, has_pos ? pos : nullptr, mode, slot, channel);
}
// 0x4f9320. The index goes into the packet argument's stack slot.
DWORD THISCALL ClientPlayVO_Read(void *, void *packet)
{
    DWORD slot = as_dw(packet);
    read_dword_token(packet, &slot, glob_addr(0x6c2480));
    const DWORD on = fn<DWORD(__fastcall *)(DWORD, DWORD, const char *, DWORD)>(0x4a4670)(glob_addr(0x6bd330), glob_addr(0x6bd430), glob<const char *>(0x60b05c), 1);
    if (!on)
        return on;
    return fn<DWORD(THISCALL *)(void *, DWORD)>(0x57e890)(glob<void *>(0x6c2480), slot);
}
// 0x4f93a0
DWORD THISCALL RequestSyncFromClients_Read(void *, void *)
{
    void *n = net_layer();
    void *packet = vcall<void *>(n, 12, 0);
    void *reader = fn<void *(__cdecl *)()>(0x4f78a0)();
    fn<DWORD(THISCALL *)(void *, void *)>(0x5ac330)(reader, packet);
    BYTE *s = vcall<BYTE *>(n, 23, 0);
    s[0x38] = 1;
    return as_dw(s);
}
// 0x49cd30 (vtable 0x5f3a98). The object is used without a null test.
DWORD THISCALL Reader5f3a98_Read(void *, void *packet)
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x13c));
    if (o) {
        fn<DWORD(THISCALL *)(void *)>(0x49bcb0)(o);
        dw(o, 0) = vt(0x5f3aa4);
        fn<DWORD(THISCALL *)(void *)>(0x4173c0)(o);
        dw(o, 0x134) = 0xffffffff;
        dw(o, 0x138) = 0xffffffff;
    }
    vcall<DWORD>(o, 27, packet);
    return fn<DWORD(THISCALL *)(void *)>(0x4fd5c0)(o);
}
// 0x4f8c50: atexit of the play-sound reader 0x6c2960: both tables (+0x18 playing, +8 free channels)
// emptied with their base vtable 0x5f9288 (the entries are not freed), then the reader's base vtable.
// eax: the last delete's, or 0.
DWORD __cdecl ClientPlaySound_AtExit()
{
    DWORD r = 0;
    for (BYTE *a : {glob<BYTE *>(0x6c2978), glob<BYTE *>(0x6c2968)}) {
        r = dw(a, 4);
        dw(a, 0) = vt(0x5f9288);
        if (r) {
            r = fn<DWORD(__cdecl *)(void *)>(0x5d0320)(pt(a, 4));
            dw(a, 4) = 0;
            dw(a, 0xc) = 0;
        }
        dw(a, 8) = 0;
    }
    *glob<DWORD *>(0x6c2960) = vt(0x5f1dd0);
    return r;
}

// ---- NetLocal (0x5f91d4) and ZaxNetGame (0x602fa8) -------------------------------------------------------

DWORD THISCALL NetLocal_LocalPlayer(void *) { return *glob<DWORD *>(0x6c2938); }                                // 0x4f7e50, slot 28
DWORD THISCALL NetLocal_LocalSession(void *self) { return vcall<DWORD>(self, 23, *glob<DWORD *>(0x6c2938)); }  // 0x4f7e60, slot 29
DWORD THISCALL NetLocal_Player(BYTE *self, DWORD i) { return as_dw(player_at(pt(self, 4), i)); }              // 0x4f7e70, slot 22
// 0x4f7e80, slot 2 (player, data, size): the bytes appended to the server's packet (player 0: its
// session's +0x3c) or the player's (+0x70; nothing without a session)
DWORD THISCALL NetLocal_Send(void *self, DWORD player, DWORD data, DWORD size)
{
    if (!player)
        return write_bytes(vcall<BYTE *>(self, 23, 0) + 0x3c, data, size);
    BYTE *s = vcall<BYTE *>(self, 23, player);
    if (!s)
        return 0;
    return write_bytes(s + 0x70, data, size);
}
// 0x4f7f00, slot 24 / 0x4f7f50, slot 26 (player): the packet of slot 12 (0) / 13 (player), when it has
// data, read by the net game's slot 14 (0, packet)
DWORD THISCALL NetLocal_ReadIncoming(void *self)
{
    void *p = vcall<void *>(self, 12, 0);
    const DWORD r = fn<DWORD(THISCALL *)(void *)>(0x520f50)(p);
    if (!(BYTE)r)
        return r;
    return vcall<DWORD>(vcall<void *>(world(), 1), 14, 0, p);
}
DWORD THISCALL NetLocal_ReadIncomingFrom(void *self, DWORD player)
{
    void *p = vcall<void *>(self, 13, player);
    const DWORD r = fn<DWORD(THISCALL *)(void *)>(0x520f50)(p);
    if (!(BYTE)r)
        return r;
    return vcall<DWORD>(vcall<void *>(world(), 1), 14, 0, p);
}
DWORD THISCALL NetLocal_IncomingPacket(void *self) { return vcall<DWORD>(self, 12, 0); }                // 0x4f7f30, slot 25
DWORD THISCALL NetLocal_PlayerPacket(void *self, DWORD i) { return vcall<DWORD>(self, 23, i) + 0x70; }  // 0x4f7f40, slot 12
DWORD THISCALL NetLocal_ServerPacket(void *self, DWORD i) { return vcall<DWORD>(self, 23, i) + 0x3c; }  // 0x4f7f90, slot 13
DWORD THISCALL NetLocal_Graph(void *self, DWORD i) { return vcall<DWORD>(self, 23, i) + 0xa4; }         // 0x4f7fa0, slot 14

// 0x5ba830, slot 5 (reason, message): the sounds stopped, the mission over window closed, a modal box
// with the message (0x714448's text, or 0x714444's format with the message) and one button; for the reason
// "CD KEY" the CD key cleared and the message kept as its last error; then a new main menu (0x158 bytes)
// opened by the error reporter's slot 23 (menu, 1), the world's slot 81 (1). eax: the ZString's destructor.
DWORD THISCALL ZaxNetGame_Disconnected(void *self, const char *reason, const char *message)
{
    fn<DWORD(THISCALL *)(void *, const char *, const char *)>(0x4f7ed0)(self, reason, message);
    fn<DWORD(__cdecl *)()>(0x50db70)();
    BYTE text[4];
    zs_copy(text, glob<void *>(0x714448));
    if (message && *message)
        fn<DWORD(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(text, zs_cstr(glob<void *>(0x714444)), message);
    BYTE menu[0x150];
    fn<DWORD(THISCALL *)(void *, DWORD, void *)>(0x4de290)(menu, glob_addr(0x714450), text);
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4de790)(menu, glob_addr(0x71444c), 0);
    fn<DWORD(THISCALL *)(void *)>(0x4de740)(menu);
    if (crt_stricmp(reason, glob<const char *>(0x627020)) == 0) {
        fn<DWORD(__fastcall *)(DWORD)>(0x46b500)(glob_addr(0x62e558));
        fn<DWORD(__fastcall *)(const char *)>(0x46b520)(message);
    }
    BYTE *main_menu = static_cast<BYTE *>(pool_alloc(0x158));
    void *made = nullptr;
    if (main_menu)
        made = fn<void *(THISCALL *)(void *, void *)>(0x5be4a0)(main_menu, fn<void *(THISCALL *)(void *)>(0x4cdf30)(*glob<void **>(0x6c02cc)));
    vcall<DWORD>(*glob<void **>(0x6c02cc), 23, made, 1);
    vcall<DWORD>(world(), 81, 1);
    fn<DWORD(THISCALL *)(void *)>(0x4de5e0)(menu);
    return zs_dtor(text);
}
// 0x5ba940, slot 6: Layer_SetDrawDirect (0x4f7ee0 with this in ecx), then CZaxWorld::SetGameType(-1)
DWORD THISCALL ZaxNetGame_Ended(void *self)
{
    fn<DWORD(THISCALL *)(void *)>(0x4f7ee0)(self);
    return fn<DWORD(THISCALL *)(void *, DWORD)>(0x5a32b0)(app(), 0xffffffff);
}
// 0x5baba0, slot 7: a tail jump to the client options reader's Write with the slot's own arguments
__attribute__((naked)) void ZaxNetGame_WriteOptions()
{
    __asm__("movl " ZAX_ASM_CODE_ATT(0x5abe30) ", %eax\n\tcall *%eax\n\tmovl %eax, %ecx\n\tmovl " ZAX_ASM_CODE_ATT(0x5abfe0) ", %eax\n\tjmp *%eax");
}
// 0x5baaa0, slot 10 (player): "<name> ..." (0x714458's format) sent to everyone when a player leaves, the
// base slot (0x4186e0), the player's character taken out of the application's list (+0x284) and, when it
// is a character, its attachments deleted and the hit points it has left dealt to it as "Unavoidable"
// damage by itself (slot 99)
DWORD THISCALL ZaxNetGame_PlayerLeft(void *self, DWORD player)
{
    if (player) {
        BYTE text[4];
        zs_ctor(text);
        void *name = fn<void *(THISCALL *)(void *)>(0x5b7aa0)(player_at(vcall<void *>(world(), 1), player));
        const char *fmt = zs_cstr(glob<void *>(0x714458));
        fn<DWORD(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(text, fmt, zs_cstr(name));
        fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x59b260)(app(), zs_cstr(text), 0xffffffff);
        zs_dtor(text);
    }
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4186e0)(self, player);
    BYTE *list = static_cast<BYTE *>(app()) + 0x284;
    BYTE *c = *fn<BYTE **(THISCALL *)(void *, DWORD)>(0x4ef8e0)(list, player);
    if (c && !(BYTE)is_kind_of(c, class_of(fn_addr(0x49a930))))
        c = nullptr;
    DWORD r = fn<DWORD(THISCALL *)(void *, DWORD)>(0x4ef950)(static_cast<BYTE *>(app()) + 0x284, player);
    if (!c)
        return r;
    fn<DWORD(THISCALL *)(void *)>(0x4fbd00)(c);
    const DWORD type = fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x523df0)(glob<void *>(0x6bd760), glob<const char *>(0x610324), 0xffffffff);
    const float hp = fstp(fn<long double(THISCALL *)(void *)>(0x48d030)(c));
    return vcall<DWORD>(c, 99, hp, type, c);
}
// 0x5babb0, slot 27: NetLocal_SetLocalPlayer
DWORD THISCALL ZaxNetGame_SetLocalPlayer(void *self, DWORD i) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4f7e40)(self, i); }

// ---- GameSpy, CZaxWorld's slot 87, the save game's name callbacks -----------------------------------------

// 0x51ef40 / 0x51efd0 / 0x51f060 / 0x51f0f0 (out, size, game): the text of the net game's slot 24 / 25 / 26
// / 27 (a ZString) copied into out, cut to size - 1 characters
template <DWORD SLOT_JUMP> DWORD __cdecl GameSpy_KeyText(char *out, DWORD size, void *game)
{
    const DWORD max = size - 1;
    BYTE z[4];
    zs_ctor(z);
    fn<DWORD(THISCALL *)(void *, void *)>(SLOT_JUMP)(game, z);
    if (zs_length(z) <= max) {
        const char *s = zs_cstr(z);
        DWORD n = 0;
        while (s[n])
            n++;
        memcpy(out, s, n + 1);
        return zs_dtor(z);
    }
    fn<DWORD(__cdecl *)(char *, const char *, DWORD)>(0x5d0b50)(out, zs_cstr(z), max);
    out[max] = 0;
    return zs_dtor(z);
}
// 0x5a3440, slot 87 (a, part): World::FixPart, then a part with a COldBadPatrolAI (slot 64 (class, 0)) gets
// slot 58 (0x6c29a8)
DWORD THISCALL ZaxWorld_FixPart(void *self, DWORD a, void *part)
{
    fn<DWORD(THISCALL *)(void *, DWORD, void *)>(0x4f5660)(self, a, part);
    const DWORD ai = vcall<DWORD>(part, 64, class_of(fn_addr(0x53a940)), 0);
    if (!ai)
        return ai;
    return vcall<DWORD>(part, 58, *glob<DWORD *>(0x6c29a8));
}
// 0x4f86b0 (fastcall object, out byte): a dialogue line's property list saved (the result in out), a layer's
// save data marked; al 1 for both, 0 for anything else
DWORD __fastcall SaveName_Changed(BYTE *object, BYTE *out)
{
    DWORD r = is_kind_of(object, class_of(fn_addr(0x41deb0)));
    if ((BYTE)r) {
        r = fn<DWORD(THISCALL *)(void *)>(0x41e360)(pt(object, 0x10));
        *out = (BYTE)r;
        return with_al(r, 1);
    }
    r = is_kind_of(object, class_of(fn_addr(0x4e52f0)));
    if (!(BYTE)r)
        return r;
    *out = 1;
    return with_al(r, 1);
}
// 0x4f8720 (through CallMember): the three save game callbacks added to their static lists
DWORD __cdecl SaveName_Register()
{
    fn<DWORD(__fastcall *)(DWORD)>(0x419ea0)(fn_addr(0x4f8120));
    fn<DWORD(__fastcall *)(DWORD)>(0x419f00)(fn_addr(0x4f86b0));
    return fn<DWORD(__fastcall *)(DWORD)>(0x419f60)(fn_addr(0x4f8700));
}

}  // namespace

ZAX_HOOKS(c4_net)
{
    const Hook list[] = {
#include "c4_net_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
