// The network packet of the multiplayer code and the small classes around it (0x51f6c0-0x520f60).
//
// docs/functions.csv counts 0x51f000-0x520fff as Core through the neighbours only; all of it is
// multiplayer: the GameSpy query answers (0x51f180-0x51f420: "gamemode", "timelimit", "fraglimit",
// "team"/"score", "openplaying", "Bind Host to IP Address"), the player list helpers (0x51f440-0x51f5a0),
// the net traffic graph ("Max %d, Cur %d", "Player %u", "All Non-Local Traffic") and the packet that
// the block readers ("CClientReadyBlockReader", "CClientSyncBlockReader", …, 0x4f7bf0) and the
// entities' slot 25/27 (state written into/read from the packet) use. Names are ours [assumption];
// the layouts are [confirmed: constructors, destructors, every accessor].
//
//   NetPacket (0x34, built by the net session 0x4f7c50 twice, freed by 0x4f7d00/0x4f7d80):
//     +0x00 the registered block readers: a pointer array (vtable 0x5fb580, heap flavour; slot 2 =
//           0x564f20 appends)
//     +0x10 data (operator new), +0x14 capacity, +0x18 "reliable" flag (set by block kinds 0, 1, 4),
//     +0x1c read position, +0x20 length, +0x24 bytes read before the last Reset
//     +0x28 the token map (0x10 bytes, a pointer array with vtable 0x5fb574 that is never filled):
//           0x55d1f0 (table, id) returns id, 0x55d200 (table, id) returns 1 ("known"), the remember
//           call is 0x46cd50 (ret 8, does nothing). So a token is always written as the bare id.
//     +0x2c the block reader of the last ReadBlock, +0x30 a byte cleared by the constructor
//   Writing grows the data to max(2 * capacity, capacity + n) when capacity < length + n (32-bit
//   arithmetic), copies the old bytes and frees the old block. Reading checks position + n <= length
//   and returns al = 1/0; the upper bytes of eax are whatever the original had there (documented per
//   function).
//   Block types: WriteBlockType writes the index of a registered reader as bytes of 0xff (each worth
//   0xfe) and a last byte < 0xff; ReadBlock decodes it, keeps the reader at +0x2c and calls its slot 2
//   with the packet. With no byte left the index is 0xcf; a packet ending in 0xff makes ReadBlock loop
//   for ever (as in the original).
//   BlockReader (8 bytes, base vtable 0x5f1dd0: slot 2 is _purecall 0x5d0331): +4 its name, set when it
//     is registered (0x51ff30). The game's readers are static objects (0x6c2910-0x6c2930 and others).
//   NetSessionBase (vtable 0x5fb568: 0 deleting destructor, 1 0x401270 (ret), 2 clears +0x18): +4/+5
//     bytes 0, +6 byte 1, +8 0, +0x10 0, +0x18 byte 0, +0x1c player settings (0x10 bytes: ZString with
//     the text of 0x6bd2f0, 50, 100, byte 1; constructor 0x46ce20), +0x20 byte 0, +0x24 ZString,
//     +0x28 byte 0, +0x2c 0, +0x30 0, +0x34 1. The net session (vtable 0x5f91c8, 0x4f7c50) derives
//     from it.
//   NetTrafficGraph (0x1c): +0/+4 position (set when drawn), +8 80 samples, +0xc height 50, +0x10 1,
//     +0x14 int[80] (0x140 bytes), +0x18 int[samples], both zeroed. The constructor counts the graphs
//     in 0x6d00e8, the destructor counts down.
//   NamedValue (0xc, vtable 0x5fb564, slot 0 the deleting destructor): +4 ZString, +8 a dword.
// Five functions of the range stay with the original: 0x51f880 (GameSpy player answer "name"/"ping",
// asks the world 0x6c2080), 0x51f9a0 (draws the traffic graph) and 0x520150 (updates the graphs of the
// session, draws), and the 4-byte getters 0x5203b0 (length) and 0x5203c0 (the flag), not hookable.
#pragma once
#include "reimpl.h"
#include "ptr_array.h"
#include "zstring.h"

namespace zax {

struct BlockReader {
    DWORD vftable;
    const char *name;   // 0x04

    BlockReader *THISCALL Ctor();   // 0x51f8d0
};

struct NetPacket {
    ZPtrArray readers;        // 0x00 BlockReader *
    BYTE *data;               // 0x10
    DWORD capacity;           // 0x14
    BYTE reliable;            // 0x18
    BYTE pad19[3];
    DWORD position;           // 0x1c
    DWORD length;             // 0x20
    DWORD consumed;           // 0x24
    ZPtrArray *tokens;        // 0x28
    BlockReader *current;     // 0x2c
    BYTE unknown30;           // 0x30
    BYTE pad31[3];

    void Grow(DWORD n);

    NetPacket *THISCALL Ctor();                                         // 0x51fe60
    void THISCALL Dtor();                                               // 0x51fec0 (eax undefined)
    DWORD THISCALL RegisterReader(BlockReader *reader, const char *name);   // 0x51ff30
    DWORD THISCALL WriteBlockType(BlockReader *reader, DWORD kind);     // 0x51ff50
    DWORD THISCALL ReadBlock();                                         // 0x5200d0
    DWORD THISCALL WriteBytes(const void *p, DWORD n);                  // 0x5203d0
    DWORD THISCALL Skip(DWORD n);                                       // 0x520470
    DWORD THISCALL ReadByte(BYTE *out);                                 // 0x520490
    DWORD THISCALL Read16(DWORD *out);                                  // 0x5204c0
    DWORD THISCALL ReadWord(WORD *out);                                 // 0x520510
    DWORD THISCALL ReadDword(DWORD *out);                               // 0x520550
    DWORD THISCALL ReadBool(BYTE *out);                                 // 0x520580
    DWORD THISCALL ReadDwordToken(DWORD *out, void *table);             // 0x5205d0
    DWORD THISCALL ReadByteToken(BYTE *out, void *table);               // 0x520620
    DWORD THISCALL ReadWordToken(WORD *out, void *table);               // 0x520670
    const char *THISCALL ReadText();                                    // 0x5206c0
    DWORD THISCALL ReadString(ZString *out);                            // 0x520700
    DWORD THISCALL Reset();                                             // 0x520750
    DWORD THISCALL WriteText(const char *s);                            // 0x520770
    DWORD THISCALL WriteByte(BYTE v);                                   // 0x520820
    DWORD THISCALL Write16(const DWORD *p);                             // 0x5208a0
    DWORD THISCALL WriteDword(DWORD v);                                 // 0x520940
    DWORD THISCALL WriteWord(WORD v);                                   // 0x5209c0
    DWORD THISCALL WriteBool(BYTE v);                                   // 0x520a50
    DWORD THISCALL WriteDwordToken(DWORD v, void *table);               // 0x520ae0
    DWORD THISCALL WriteByteToken(DWORD v, void *table);                // 0x520c40
    DWORD THISCALL WriteWordToken(DWORD v, void *table);                // 0x520da0
    DWORD THISCALL PatchDword(DWORD v, DWORD offset);                   // 0x520f10
    DWORD THISCALL PatchWord(WORD v, DWORD offset);                     // 0x520f30
    DWORD THISCALL CanRead();                                           // 0x520f50
    DWORD THISCALL SkipToEnd();                                         // 0x520f60
};
static_assert(sizeof(NetPacket) == 0x34);
static_assert(offsetof(NetPacket, tokens) == 0x28);

struct PlayerSettings;
struct NetSessionBase {
    DWORD vftable;
    BYTE flag4, flag5, flag6, pad7;   // 0x04
    DWORD unknown8;                   // 0x08
    DWORD unknownc;                   // 0x0c (not set by the constructor)
    DWORD unknown10;                  // 0x10
    DWORD unknown14;                  // 0x14 (not set)
    BYTE flag18, pad19[3];            // 0x18
    PlayerSettings *settings;         // 0x1c
    BYTE flag20, pad21[3];            // 0x20
    ZString text;                     // 0x24
    BYTE flag28, pad29[3];            // 0x28
    DWORD unknown2c;                  // 0x2c
    DWORD unknown30;                  // 0x30
    DWORD unknown34;                  // 0x34

    NetSessionBase *THISCALL Ctor();                   // 0x51f780
    NetSessionBase *THISCALL DeletingDtor(DWORD flags);   // 0x51f7e0 (slot 0)
    void THISCALL Dtor();                              // 0x51f830 (eax undefined)
    void THISCALL ClearFlag18();                       // 0x51f870 (slot 2, the caller's eax)
};
static_assert(offsetof(NetSessionBase, text) == 0x24);

struct NetTrafficGraph {
    int x, y;            // 0x00
    DWORD samples;       // 0x08
    DWORD height;        // 0x0c
    DWORD unknown10;     // 0x10
    int *sent;           // 0x14
    int *received;       // 0x18

    NetTrafficGraph *THISCALL Ctor();   // 0x51f8e0
    void THISCALL Dtor();               // 0x51f960 (eax undefined)
};

struct NamedValue {
    DWORD vftable;
    ZString name;   // 0x04
    DWORD value;    // 0x08

    NamedValue *THISCALL Ctor();                   // 0x51f700
    NamedValue *THISCALL DeletingDtor(DWORD flags);   // 0x51f720 (slot 0)
    DWORD THISCALL SetValue(DWORD v);              // 0x51f750 (identical code folded: any class with a dword at +8)
    ZString *THISCALL SetName(const char *s);      // 0x51f770 (folded: any ZString at +4, 25 callers)
};

constexpr DWORD NET_GRAPH_COUNT = 0x6d00e8;

}  // namespace zax
