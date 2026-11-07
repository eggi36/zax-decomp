// The net session, the local net layer and the game's block readers (0x4f78a0-0x4f9530).
//
// Every block reader is a static object (BlockReader, 8 bytes, net_packet.h) with its own vtable of
// three slots (0 0x5718b0, 1 0x4f0ab0, 2 the read) whose name comes from the packet it is registered
// with (0x4f7bf0 on the client, 0x4f93e0 on the server). Next to each one the exe has an initialiser
// (constructor, vtable, atexit), an accessor that returns its address and a write function that puts
// the block type and the block's data into a packet (thiscall on the reader, not virtual). The reads
// (slot 2) are not in docs/functions.csv and stay with the original. [confirmed: code, the names the
// registration gives]
//   0x6c2910 CClientConfigControlBlockReader      0x6c2940 CServerTellsClientToCreateExplosionBlockReader
//   0x6c2918 CClientFrameRecievedBlockReader      0x6c2948 CClientInvalidateRectBlockReader
//   0x6c2920 CClientReadyBlockReader              0x6c2950 CRequestSyncFromClientsBlockReader
//   0x6c2928 CSetZoomBlockReader                  0x6c2958 CClientSetLayerPartFlagBlockReader
//   0x6c2930 CClientSyncBlockReader               0x6c2960 CClientPlaySoundBlockReader (0x28 bytes)
//   0x6c2988 CClientPlaySimpleSoundBlockReader    0x6c2990 CClientPlayVOBlockReader
// The write functions return al = 1 (the InvalidateRect and SetLayerPartFlag ones 0 when the part has
// nothing to send) with the upper bytes of eax left by the last call.
//
// CClientPlaySoundBlockReader keeps two tables of the sounds by id (the free channel table at +8 and
// the playing table at +0x18): pointer arrays (vtable 0x5f9288, heap, Add 0x564f20) of entries of
// 0x14 bytes {id, channels: {vtable 0x5f9294, data, count, capacity} of 8-byte {byte channel, sound}}.
//
// NetSession (0xec bytes, vtable 0x5f91c8: 0 the deleting destructor 0x4f7d00, 1 0x401270, 2
// 0x51f870): NetSessionBase, +0x38/+0x39 bytes, +0x3c and +0x70 two packets (slots 13 and 12 of the net
// layer), +0xa4 the traffic graph (slot 14), +0xc0-+0xd4 0, +0xd8 1.0f, +0xdc 0 (the local player's
// slot is read from it by 0x4f1050), +0xe0 0, +0xe4 a ZString, +0xe8 an object of 0x1c8 bytes from
// slot 41 of the world (the constructor's argument; destructor 0x46f2f0, freed to the pool). The
// players of the net game (net_game.h) are net sessions.
//
// NetLocal (8 bytes, vtable 0x5f91d4, base vtable 0x5fb484 with the destructor 0x51ec40): the net
// layer the world reaches through its slot 44; +4 the net game. Its slots 11-14, 22-29 (player count,
// the player's packets and graph, the player, the local player 0x6c2938) are not in
// docs/functions.csv and stay with the original except slot 27 (0x4f7e40).
//
// Not hookable: 0x4f7ee0 (slot 6 of NetLocal: xor cl, cl; jmp 0x4e9d90, 7 bytes ending in a relative
// jump).
#pragma once
#include "net_packet.h"

namespace zax {

struct SoundChannel {
    BYTE channel;
    BYTE pad[3];
    void *sound;   // a sound with slot 11 "stop" [assumption: name]
};

struct SoundChannels {
    DWORD vftable;   // 0x5f9294
    SoundChannel *data;
    DWORD count;
    DWORD capacity;
    DWORD THISCALL Reserve(DWORD n);   // 0x4f9530
};

struct SoundEntry {
    DWORD id;
    SoundChannels channels;   // 0x04
};
static_assert(sizeof(SoundEntry) == 0x14);

// Fastcall: the table in ecx, the id in edx
DWORD __fastcall SoundTable_Remove(ZPtrArray *table, DWORD id, DWORD channel);             // 0x4f9090
DWORD __fastcall SoundTable_Add(ZPtrArray *table, DWORD id, DWORD channel, DWORD lookup);  // 0x4f91a0

struct PlaySoundReader {
    BlockReader base;
    ZPtrArray free_channels;   // 0x08 SoundEntry *
    ZPtrArray playing;         // 0x18 SoundEntry *

    PlaySoundReader *THISCALL DeletingDtor(DWORD flags);   // 0x4f8cc0 (slot 0; the entries are not freed)
    DWORD THISCALL FreeChannel(DWORD id);                 // 0x4f8ec0
    void *THISCALL SoundAt(DWORD id, DWORD channel);      // 0x4f8f30
    DWORD THISCALL Remove(DWORD id, DWORD channel);       // 0x4f8f90
    DWORD THISCALL StopAll();                             // 0x4f8fb0
    DWORD THISCALL Write(NetPacket *p, DWORD unused, void *level, DWORD sound, const void *pos, DWORD mode,
                         DWORD flag, DWORD channel);    // 0x4f8e30
};

// The write functions of the other readers (thiscall on the reader)
struct NetBlockWriter {
    BlockReader base;
    DWORD THISCALL WriteReady(NetPacket *p, DWORD unused);                                  // 0x4f7950
    DWORD THISCALL WriteConfigControl(NetPacket *p, void *bits, const float *v, DWORD flag, DWORD reliable);   // 0x4f7a70
    DWORD THISCALL WriteFrameRecieved(NetPacket *p);                                        // 0x4f7bd0
    DWORD THISCALL WriteInvalidateRect(NetPacket *p, DWORD unused, void *part);             // 0x4f87c0
    DWORD THISCALL WriteSetLayerPartFlag(BYTE *session, void *part);                        // 0x4f88e0
    DWORD THISCALL WriteExplosion(BYTE *session, void *level, DWORD kind, DWORD x, DWORD y, DWORD a, DWORD flag,
                                  DWORD b);                                                 // 0x4f8a40
    DWORD THISCALL WritePlaySimpleSound(NetPacket *p, DWORD unused1, DWORD unused2, DWORD sound, const void *pos);   // 0x4f8b90
};

// The accessors of the static readers
BlockReader *__cdecl Reader_ClientSync();             // 0x4f78a0
BlockReader *__cdecl Reader_ClientReady();            // 0x4f7910
BlockReader *__cdecl Reader_ClientConfigControl();    // 0x4f79a0
BlockReader *__cdecl Reader_ClientFrameRecieved();    // 0x4f7b80
BlockReader *__cdecl Reader_ClientInvalidateRect();   // 0x4f8770
BlockReader *__cdecl Reader_ClientSetLayerPartFlag(); // 0x4f8850
BlockReader *__cdecl Reader_ServerExplosion();        // 0x4f8990
BlockReader *__cdecl Reader_ClientPlaySimpleSound();  // 0x4f8af0
BlockReader *__cdecl Reader_ClientPlaySound();        // 0x4f8d30
BlockReader *__cdecl Reader_RequestSyncFromClients(); // 0x4f9390

// Registration of the readers into a packet (thiscall on the packet)
struct NetReaderRegistry {
    NetPacket packet;
    DWORD THISCALL RegisterClientReaders();   // 0x4f7bf0
    DWORD THISCALL RegisterServerReaders();   // 0x4f93e0
};

struct NetSession {
    NetSessionBase base;
    BYTE flag38, flag39, pad3a[2];   // 0x38
    NetPacket packet1;               // 0x3c (slot 13 of the net layer)
    NetPacket packet2;               // 0x70 (slot 12)
    NetTrafficGraph graph;           // 0xa4 (slot 14)
    DWORD unknownc0[6];              // 0xc0
    float unknownd8;                 // 0xd8
    DWORD unknowndc;                 // 0xdc
    DWORD unknowne0;                 // 0xe0
    ZString text;                    // 0xe4
    void *control;                   // 0xe8 0x1c8 bytes

    NetSession *THISCALL Ctor(DWORD arg);            // 0x4f7c50
    NetSession *THISCALL DeletingDtor(DWORD flags);  // 0x4f7d00 (slot 0)
    void THISCALL Dtor();                            // 0x4f7d80 (eax: the base destructor's)
};
static_assert(offsetof(NetSession, graph) == 0xa4);
static_assert(offsetof(NetSession, control) == 0xe8);

struct NetLocal {
    DWORD vftable;
    void *game;   // 0x04

    NetLocal *THISCALL Ctor();                       // 0x4f7df0
    NetLocal *THISCALL DeletingDtor(DWORD flags);    // 0x4f7e00 (slot 0)
    void THISCALL Dtor();                            // 0x4f7e30 (eax: 0x51ec40's)
    DWORD THISCALL SetLocalPlayer(DWORD i);          // 0x4f7e40 (slot 27, the global 0x6c2938)
    DWORD THISCALL StopSounds(DWORD a, DWORD b);     // 0x4f7ed0 (slot 5: StopAll of the PlaySound reader)
};

constexpr DWORD G_LOCAL_PLAYER = 0x6c2938;
constexpr DWORD READER_PLAY_SOUND = 0x6c2960;
constexpr DWORD SOUND_MANAGER = 0x713708;

}  // namespace zax
