// The game side of the DirectPlay 8 net game (0x47ee70-0x480a70): creating the DirectPlay objects,
// the service provider list, the message handler, the player slots and the received message queue.
//
// The COM objects are globals: 0x6bdc2c the server, 0x6bdc30 the client, 0x6bdc34 the address
// (CoCreateInstance with the CLSIDs/IIDs at 0x606160-0x6061d0) [confirmed: CoCreateInstance, the slots
// used; interface names and slot names after the DirectPlay 8 headers: assumption]. Their methods are
// called unchanged (stdcall through the vtable): Release (2), Initialize (3, with the message
// trampoline 0x47ead0), EnumServiceProviders (client 4), GetClientInfo (server 9), ReturnBuffer
// (server 25, client 15).
//
// The DirectPlay net game (vtable 0x5f15a8; the NetGame part: net_game.h):
//   +0x38 byte (set when hosting), +0x39 byte "running" (players may leave), +0x3a byte "terminated"
//   with the terminate text at +0x3b (copied without a bound), +0x43c the received messages (array
//   {vtable 0x5f1618, data, count, capacity} of 20 bytes {player context, sender, data, size, buffer
//   handle}; lock 0x6bdc08), +0x44c 100 player slots of 12 bytes {byte added, byte removed, dpnid,
//   session} and +0x8fc "slots changed" (lock 0x6bdbf0), the terminate lock 0x6bdad0.
//   A new player takes the highest free slot from 100 down to 2 (slot 100 is +0x8fc/+0x900, one past
//   the array: kept as in the original); its slot index is its DirectPlay player context. 0x480800
//   turns added slots into net sessions (game type slot 20, name from GetClientInfo, a random
//   10-letter text from GameRandom 0x7124c0, game type slot 9) and removes left ones (game type slot
//   10, the GameSpy disconnect 0x5c5750, slot 0 of the session); 0x480a70 hands the queued messages to
//   slot 2 of the game type and returns the buffers.
//
// The message handler (thiscall, (message id, message)) switches on id - 0xffff0007 through the byte
// table 0x480300 and the jump table 0x4802e0:
//   0xffff0007 create player (slot, context = slot; 0x80158260 when none is free)
//   0xffff0009 destroy player (slot "removed", only while running)
//   0xffff000b enum hosts response (the session name and both addresses into a list item 0x5f1628)
//   0xffff000e indicate connect (player context 0xfeadface)
//   0xffff0011 receive (queued; DPNSUCCESS_PENDING 0x15800e keeps the buffer)
//   0xffff0014 send complete (an error counts in +0x10 of the player)
//   0xffff0016 terminate session (the text)
// Names of the ids after the DirectPlay 8 headers [assumption]. Everything else returns 0.
#pragma once
#include "net_game.h"

namespace zax {

struct DPlayMessage {
    DWORD context;   // the slot index
    DWORD sender;
    void *data;
    DWORD size;
    DWORD handle;
};

struct DPlaySlot {
    BYTE added, removed, pad[2];
    DWORD dpnid;
    void *session;
};

struct DPlayGame {
    NetGame game;
    BYTE pad34[4];
    BYTE hosting;              // 0x38
    BYTE running;              // 0x39
    BYTE terminated;           // 0x3a
    char terminate_text[0x401];   // 0x3b
    struct {
        DWORD vftable;
        DPlayMessage *data;
        DWORD count;
        DWORD capacity;
    } queue;                   // 0x43c
    DPlaySlot slots[100];      // 0x44c
    BYTE slots_changed;        // 0x8fc

    void THISCALL ServerInfo(ZString *out);                   // 0x47ee70 ("hostport", "numplayers", "maxplayers")
    DPlayGame *THISCALL DeletingDtor(DWORD flags);            // 0x47ef10 (slot 0)
    void *THISCALL EnumServiceProviders();                    // 0x47fc20 (the list, null on an error)
    DWORD THISCALL CreateDirectPlay(DWORD server);            // 0x47fdf0 (al)
    DWORD THISCALL MessageHandler(DWORD id, BYTE *msg);       // 0x47ff50
    void *THISCALL InitServiceProviders();                    // 0x4803c0 (slot 4)
    void THISCALL ProcessSlots();                             // 0x480800
    void THISCALL ProcessMessages();                          // 0x480a70
};
static_assert(offsetof(DPlayGame, queue) == 0x43c);
static_assert(offsetof(DPlayGame, slots) == 0x44c);
static_assert(offsetof(DPlayGame, slots_changed) == 0x8fc);

// The service provider list item (0x10 bytes, vtable 0x5f1624; base 0x51ea10 with vtable 0x5fb47c:
// +4 the name (operator new copy), +8 the next item, deleted with it): +0xc the GUID (a 16-byte copy)
struct DPlayProvider {
    DWORD vftable;
    char *name;
    DPlayProvider *next;
    BYTE *guid;
    DPlayProvider *THISCALL DeletingDtor(DWORD flags);   // 0x47fdb0 (slot 0)
};

// A host an enumeration found (0x14 bytes, vtable 0x5f1628; built by the enum hosts response of the message handler,
// put in front of the list the user context names): a name list node (NetNameNodeB, constructor 0x51eb40: +4 the
// session name, +8 the next host) with the host's and the device's address objects (IDirectPlay8Address, AddRef'd
// when stored) [confirmed: code]. Phase 6 batch 6-0: no table had its destructor.
struct DPlayHost {
    DWORD vftable;
    char *name;
    DPlayHost *next;
    void *host_address;
    void *device_address;
    DPlayHost *THISCALL DeletingDtor(DWORD flags);   // 0x480310 (slot 0)
};

constexpr DWORD DPLAY_SERVER = 0x6bdc2c, DPLAY_CLIENT = 0x6bdc30, DPLAY_ADDRESS = 0x6bdc34;
constexpr DWORD DPLAY_QUEUE_LOCK = 0x6bdc08, DPLAY_TERMINATE_LOCK = 0x6bdad0;
constexpr DWORD DPLAY_RECEIVED = 0x6bdc24, DPLAY_PROCESS_CALLS = 0x6bdc20, DPLAY_PROCESSED = 0x6bdc28;
constexpr DWORD IAT_ENTER_CS = 0x5ea098, IAT_LEAVE_CS = 0x5ea094, IAT_DELETE_CS = 0x5ea11c;
constexpr DWORD IAT_CO_CREATE = 0x5ea340, IAT_CO_UNINIT = 0x5ea33c, IAT_W2MB = 0x5ea120;

}  // namespace zax
