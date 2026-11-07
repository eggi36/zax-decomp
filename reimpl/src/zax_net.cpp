// The game's own multiplayer layer 0x5b9f20-0x5bd952 (phase 3, multiplayer; docs/DECOMPILATION.md
// section 8): the Zax net player with its HUD icon records and scores, the lookups from an entity to its
// player, the GameSpy keys of the game, and nine of the game's block readers (accessors, write
// functions, the reads that do not load a level, their registration).
//
// Layouts [confirmed: constructor 0x5ba780, destructor 0x5ba140, the functions below]:
//   the Zax net player (0x118 bytes, operator new; vtable 0x604de4, base NetSession 0x4f7c50/0x4f7d80,
//     net_session.h): +0x14 the team (NetSession), +0xec-+0xf6 six shorts (+0xf6 the score; the others
//     the ore and crystals of Salvage King and two more), +0xf8 a byte, +0xfc a pool pointer array
//     (vtable 0x604df4, 0x604e00 after the destructor: data +0x100, count +0x104, capacity +0x108) of
//     records, +0x10c the changed bits of the scores (1 +0xec, 0x80 +0xee, 0x100 +0xf0, 0x200 +0xf2,
//     0x400 +0xf4, 4 +0xf6), +0x110 the record count last sent. Slots: 0 the deleting destructor, 1 the
//     update sent when anything changed, 2 the scores reset, 3 everything sent.
//   a record (12 bytes, operator new): +0 the index of an item definition in the catalogue 0x6c0c08,
//     +4 its HUD icon data (CInventoryHUDRenderData, 0x14 bytes from the pool, vtable 0x5ede94; slot 23
//     writes what changed, 24 everything, 26 whether something changed), +8 flags (1 the icon data
//     changed, 2 the index changed).
//   The block readers are statics {vtable, BlockReader} made by the static initialisers (not in
//     docs/functions.csv): 0x714488 chat message from the server, 0x714490 start game info, 0x7144c8
//     player info from the server, 0x714480 say a quip, 0x7144b0 say a dialogue line, 0x7144b8 show the
//     match over window, 0x7144a8 hide it, 0x7144c0 an inventory icon in the HUD, 0x7144a0 the
//     scoreboard update. Their write functions are thiscall on the reader, the reads slot 2.
//
// Return values: the original's eax where it is defined (the accessors, the lookups, the setters (their
// argument), the item mask, the reads that end in a call); the write functions' al 1 (their upper bytes
// are the last call's, not reproduced); void in use otherwise [assumption]. Where the original reads through a null pointer (no definition for a record's
// index, no player in the update read) the reimplementation does the same.
#include "class_info.h"
#include "exe_base.h"

namespace zax {

namespace {

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
short &wd(void *p, DWORD off) { return *reinterpret_cast<short *>(static_cast<BYTE *>(p) + off); }
BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

// The net packet (net_packet.h, thiscall on the packet)
DWORD pk_block_type(void *p, void *reader, DWORD kind) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x51ff50)(p, reader, kind); }
DWORD pk_write_byte(void *p, DWORD v) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x520820)(p, v); }
DWORD pk_write_word(void *p, DWORD v) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x5209c0)(p, v); }
DWORD pk_write_dword(void *p, DWORD v) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x520940)(p, v); }
DWORD pk_write_bool(void *p, DWORD v) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x520a50)(p, v); }
DWORD pk_write_text(void *p, const char *t) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x520770)(p, t); }
DWORD pk_write_byte_token(void *p, DWORD v, void *t) { return fn<DWORD(THISCALL *)(void *, DWORD, void *)>(0x520c40)(p, v, t); }
DWORD pk_write_word_token(void *p, DWORD v, void *t) { return fn<DWORD(THISCALL *)(void *, DWORD, void *)>(0x520da0)(p, v, t); }
DWORD pk_write_dword_token(void *p, DWORD v, void *t) { return fn<DWORD(THISCALL *)(void *, DWORD, void *)>(0x520ae0)(p, v, t); }
DWORD pk_read_byte(void *p, void *out) { return fn<DWORD(THISCALL *)(void *, void *)>(0x520490)(p, out); }
DWORD pk_read_word(void *p, void *out) { return fn<DWORD(THISCALL *)(void *, void *)>(0x520510)(p, out); }
DWORD pk_read_dword(void *p, void *out) { return fn<DWORD(THISCALL *)(void *, void *)>(0x520550)(p, out); }
DWORD pk_read_bool(void *p, void *out) { return fn<DWORD(THISCALL *)(void *, void *)>(0x520580)(p, out); }
DWORD pk_read_byte_token(void *p, void *out, void *t) { return fn<DWORD(THISCALL *)(void *, void *, void *)>(0x520620)(p, out, t); }
DWORD pk_read_word_token(void *p, void *out, void *t) { return fn<DWORD(THISCALL *)(void *, void *, void *)>(0x520670)(p, out, t); }
const char *pk_read_text(void *p) { return fn<const char *(THISCALL *)(void *)>(0x5206c0)(p); }
DWORD pk_check_marker(void *p, DWORD m) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x5205c0)(p, m); }
// CObject's empty slot (`ret 4`): where the original "writes" its debug markers
void empty_slot(void *self, DWORD v) { fn<void(THISCALL *)(void *, DWORD)>(0x4186e0)(self, v); }

void net_session_ctor(void *s, DWORD arg) { fn<void *(THISCALL *)(void *, DWORD)>(0x4f7c50)(s, arg); }
void net_session_dtor(void *s) { fn<void(THISCALL *)(void *)>(0x4f7d80)(s); }
void *world() { return *glob<void **>(0x6c2080); }
void *app() { return *glob<void **>(0x713f14); }
void *net_game() { return vcall<void *>(world(), 1); }
BYTE *net_game_player(void *g, DWORD i) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x51f440)(g, i); }
bool is_kind_of(const void *o, CClassInfo *c) { return fn<bool(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c); }
CClassInfo *character_class() { return fn<CClassInfo *(__cdecl *)()>(0x49a930)(); }
// 0x4ef9e0: the index of an entity in the world's player entity list (+0x284), -1 when it is none
DWORD player_index_of(void *entity) { return fn<DWORD(THISCALL *)(void *, void *)>(0x4ef9e0)(static_cast<BYTE *>(world()) + 0x284, entity); }
bool is_server() { return vcall<BYTE>(vcall<void *>(world(), 1), 12) != 0; }
const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
void zs_ctor(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_dtor(void *z) { fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
void gamespy_text(void *out, DWORD key, const char *v) { fn<void(__fastcall *)(void *, const char *, const char *)>(0x51f290)(out, at<const char *>(key), v); }
void gamespy_int(void *out, DWORD key, DWORD v) { fn<void(__fastcall *)(void *, const char *, DWORD)>(0x51f2b0)(out, at<const char *>(key), v); }
void *reporter() { return *glob<void **>(0x6c02cc); }

constexpr DWORD CATALOGUE = 0x6c0c08;   // the item catalogue (definition list: records +0x30, count +0x34)
constexpr DWORD VT_PLAYER = 0x604de4, VT_RECORDS = 0x604df4, VT_RECORDS_BASE = 0x604e00;

}  // namespace

// ---- The records (HUD icons) ----------------------------------------------------------------------------

// 0x5b9f20: what changed written: the flags (bit 1 also when the icon data reports a change, slot 26),
// the index as a word token of the catalogue when it changed, then the icon data in full (slot 24, when
// it was replaced) or what changed (slot 23); the flags cleared. Void in use [assumption].
static void THISCALL Record_Write(BYTE *self, void *packet)
{
    BYTE flags = self[8];
    if (vcall<BYTE>(pt(self, 4), 26))
        flags |= 1;
    pk_write_byte(packet, flags);
    if (self[8] & 2)
        pk_write_word_token(packet, (WORD)dw(self, 0), glob<void *>(CATALOGUE));
    if (self[8] & 1) {
        vcall<DWORD>(pt(self, 4), 24, packet);
        self[8] = 0;
        return;
    }
    if (vcall<BYTE>(pt(self, 4), 26))
        vcall<DWORD>(pt(self, 4), 23, packet);
    self[8] = 0;
}

// 0x5b9fa0: everything written: flags 3, the index, the icon data in full; the flags cleared
static void THISCALL Record_WriteAll(BYTE *self, void *packet)
{
    pk_write_byte(packet, 3);
    pk_write_word_token(packet, (WORD)dw(self, 0), glob<void *>(CATALOGUE));
    vcall<DWORD>(pt(self, 4), 24, packet);
    self[8] = 0;
}

// 0x5b9fe0: a record read: the flags (0xcf when nothing is left); with bit 2 the index (a word token,
// 0xcfcf when nothing is left), marked changed when it differs; with bit 1 the icon data, read by the
// prototype of the index's definition (a CZaxInventoryItemDefinition's slot 32, else its type
// definition's +0x24) through its slot 26 into the record's (marked changed when it gives al 1).
static void THISCALL Record_Read(BYTE *self, void *packet)
{
    BYTE flags = 0xcf;
    pk_read_byte(packet, &flags);
    if (flags & 2) {
        DWORD v = 0xcfcf;
        pk_read_word_token(packet, &v, glob<void *>(CATALOGUE));
        v &= 0xffff;
        if (dw(self, 0) != v) {
            dw(self, 0) = v;
            self[8] |= 2;
        }
    }
    if (!(flags & 1))
        return;
    BYTE *list = glob<BYTE *>(CATALOGUE);
    void *icon = pt(self, 4);
    BYTE *def = nullptr;
    if (BYTE *records = pt(list, 0x30)) {
        DWORD i = dw(self, 0);
        if (i >= dw(list, 0x34))
            i = 0;
        if (!records[i * 0x80 + 0x7f]) {
            if (!dw(records + i * 0x80, 0x78)) {
                fn<void *(THISCALL *)(void *, DWORD)>(0x55e090)(list, i);
                records = pt(list, 0x30);
            }
            def = pt(records + i * 0x80, 0x78);
            if (def && !is_kind_of(def, fn<CClassInfo *(__cdecl *)()>(0x5b5f10)()))
                def = nullptr;
        }
    }
    void *proto = vcall<void *>(def, 32);
    if (!proto)
        proto = pt(fn<BYTE *(THISCALL *)(void *)>(0x5b7af0)(def), 0x24);
    if (vcall<BYTE>(proto, 26, packet, &icon)) {
        dw(self, 4) = ptrv(icon);
        self[8] |= 1;
    }
}

// 0x5ba0e0: whether anything is to be sent (flags, or the icon data's slot 26). eax: 1 or 0.
static DWORD THISCALL Record_Changed(BYTE *self)
{
    if (self[8] || vcall<BYTE>(pt(self, 4), 26))
        return 1;
    return 0;
}

// 0x5ba100: the index set, marked changed when it differs. Void in use [assumption].
static void THISCALL Record_SetIndex(BYTE *self, DWORD v)
{
    if (dw(self, 0) != v) {
        dw(self, 0) = v;
        self[8] |= 2;
    }
}

// 0x5ba120: the icon data set and marked changed. Void in use [assumption].
static void THISCALL Record_SetIcon(BYTE *self, DWORD v)
{
    dw(self, 4) = v;
    self[8] |= 1;
}

// ---- The net player ---------------------------------------------------------------------------------------

// 0x5ba780 (the net game's slot 20, thiscall; this unused): a new player of 0x118 bytes (operator new)
// on the net session constructor with the argument. eax: the player, or 0.
static BYTE *THISCALL Player_Make(void *, DWORD arg)
{
    BYTE *p = static_cast<BYTE *>(heap_alloc(0x118));
    if (!p)
        return nullptr;
    net_session_ctor(p, arg);
    dw(p, 0x100) = 0;
    dw(p, 0x104) = 0;
    dw(p, 0x108) = 0;
    dw(p, 0xfc) = vt(VT_RECORDS);
    for (DWORD off = 0xec; off <= 0xf6; off += 2)
        wd(p, off) = 0;
    dw(p, 0x10c) = 0;
    dw(p, 0) = vt(VT_PLAYER);
    return p;
}

// The last record deleted (its icon data through slot 1, the record with operator delete) and taken
// off the array, as the destructor and SetRecordCount inline it
static void delete_last_record(BYTE *self)
{
    const DWORD i = dw(self, 0x104) - 1;
    if (BYTE *r = pt(pt(self, 0x100), i * 4)) {
        if (void *icon = pt(r, 4)) {
            vcall<void *>(icon, 1, 1);
            dw(r, 4) = 0;
        }
        heap_free(r);
    }
    const DWORD count = dw(self, 0x104);
    if (i < count) {
        if (i + 1 < count)
            fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(pt(self, 0x100) + i * 4, pt(self, 0x100) + (i + 1) * 4,
                                                                      (count - (i + 1)) * 4);   // memmove
        dw(self, 0x104) = count - 1;
    }
}

// 0x5ba140 (slot 0): the records deleted, the array's data given back to the pool, the net session
// destroyed; bit 0 frees the player (operator delete). eax: this.
static BYTE *THISCALL Player_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(VT_PLAYER);
    while (dw(self, 0x104))
        delete_last_record(self);
    dw(self, 0xfc) = vt(VT_RECORDS_BASE);
    if (void *data = pt(self, 0x100)) {
        pool_get_free(data, dw(self, 0x108) * 4);
        dw(self, 0x100) = 0;
        dw(self, 0x108) = 0;
    }
    dw(self, 0x104) = 0;
    net_session_dtor(self);
    if (flags & 1)
        heap_free(self);
    return self;
}

// 0x5ba230 (slot 2): the net session's flag cleared (0x51f870), the scores +0xf6, +0xee, +0xf0, +0xf2
// and +0xf4 set to 0 (each marked changed when it was not; +0xec stays). eax: 0.
static DWORD THISCALL Player_ResetScores(BYTE *self)
{
    fn<void(THISCALL *)(void *)>(0x51f870)(self);
    struct {
        DWORD off, bit;
    } const scores[] = {{0xf6, 4}, {0xee, 0x80}, {0xf0, 0x100}, {0xf2, 0x200}, {0xf4, 0x400}};
    for (const auto &s : scores) {
        if (wd(self, s.off))
            dw(self, 0x10c) |= s.bit;
        wd(self, s.off) = 0;
    }
    return 0;
}

// 0x5ba2d0: the records to send as bits: 1 when the count changed since the last send (+0x110), bit
// i + 1 for record i when it is new, its flags are set or its icon data changed (slot 26)
static DWORD THISCALL Player_RecordMask(BYTE *self)
{
    DWORD mask = dw(self, 0x104) != dw(self, 0x110) ? 1 : 0;
    DWORD bit = 2;
    for (DWORD i = 0; i < dw(self, 0x104); i++, bit <<= 1) {
        if (i >= dw(self, 0x110)) {
            mask |= bit;
            continue;
        }
        BYTE *r = pt(pt(self, 0x100), i * 4);
        if (r[8] || vcall<BYTE>(pt(r, 4), 26))
            mask |= bit;
    }
    return mask;
}

// 0x5ba340-0x5ba420: the six scores set (a short), each marked changed when it differs. eax: the value.
template <DWORD OFF, DWORD BIT> static DWORD THISCALL Player_SetScore(BYTE *self, DWORD v)
{
    if ((int)v != (int)wd(self, OFF))
        dw(self, 0x10c) |= BIT;
    wd(self, OFF) = (short)v;
    return v;
}

// 0x5ba440: the byte +0xf8 set. Void in use [assumption].
static void THISCALL Player_SetF8(BYTE *self, DWORD v) { self[0xf8] = (BYTE)v; }

// 0x5ba450: the number of records set: new records (operator new, 12 bytes; their index is left as
// operator new gave it) with fresh icon data (pool 0x14, CObject with the vtable 0x5ede94, reset through
// slot 19) appended through the array's slot 2, or the last records deleted. Void in use [assumption].
static void THISCALL Player_SetRecordCount(BYTE *self, DWORD n)
{
    if (n == dw(self, 0x104))
        return;
    if (n > dw(self, 0x104)) {
        while (dw(self, 0x104) < n) {
            BYTE *r = static_cast<BYTE *>(heap_alloc(0xc));
            if (r) {
                BYTE *icon = static_cast<BYTE *>(pool_alloc(0x14));
                if (icon) {
                    fn<void *(THISCALL *)(void *)>(0x416610)(icon);   // CObject's constructor
                    dw(icon, 0) = vt(0x5ede94);
                    fn<void(THISCALL *)(void *)>(0x4173c0)(icon);     // Object_InitFields
                }
                dw(r, 4) = ptrv(icon);
                vcall<DWORD>(icon, 19);
                r[8] = 0;
            }
            BYTE *entry = r;
            vcall<DWORD>(self + 0xfc, 2, &entry);
        }
        return;
    }
    while (dw(self, 0x104) > n)
        delete_last_record(self);
}

// The update sent: through the player info reader (0x5bd020, its write 0x5bd1a0) on the player's packet
// (+0x3c)
static void send_update(BYTE *self)
{
    void *reader = fn<void *(__cdecl *)()>(0x5bd020)();
    fn<DWORD(THISCALL *)(void *, void *, void *)>(0x5bd1a0)(reader, self + 0x3c, self);
}

// 0x5ba580 (slot 1): on the server, for a player that is connected (+6), the update is sent when a score
// changed or a record is to be sent (0x5ba2d0, inlined); then nothing counts as changed (+0x10c 0, the
// record count noted in +0x110)
static void THISCALL Player_Update(BYTE *self)
{
    if (is_server() && self[6]) {
        bool send = dw(self, 0x10c) != 0;
        if (!send) {
            DWORD mask = dw(self, 0x104) != dw(self, 0x110) ? 1 : 0;
            DWORD bit = 2;
            for (DWORD i = 0; i < dw(self, 0x104); i++, bit <<= 1) {
                if (i >= dw(self, 0x110)) {
                    mask |= bit;
                    continue;
                }
                BYTE *r = pt(pt(self, 0x100), i * 4);
                if (r[8] || vcall<BYTE>(pt(r, 4), 26))
                    mask |= bit;
            }
            send = mask != 0;
        }
        if (send)
            send_update(self);
    }
    dw(self, 0x10c) = 0;
    dw(self, 0x110) = dw(self, 0x104);
}

// 0x5ba640 (slot 3): on the server, for a connected player, everything sent (the scores +0xec and +0xf6
// marked, every record new: +0x110 0, or 1 without records so the count goes too)
static void THISCALL Player_SendAll(BYTE *self)
{
    if (is_server() && self[6]) {
        dw(self, 0x10c) = 5;
        dw(self, 0x110) = dw(self, 0x104) ? 0 : 1;
        send_update(self);
    }
    dw(self, 0x10c) = 0;
    dw(self, 0x110) = dw(self, 0x104);
}

// ---- Lookups -----------------------------------------------------------------------------------------------

// 0x5ba810 (index in ecx): the net game's player i
static BYTE *__fastcall Player_At(DWORD i) { return net_game_player(net_game(), i); }

// 0x5bac80 (ecx: an entity): the net player of a CEntityCharacter in the world's player list, else 0
static BYTE *__fastcall Player_Of(void *entity)
{
    if (!entity || !is_kind_of(entity, character_class()))
        return nullptr;
    const DWORD i = player_index_of(entity);
    if (i == 0xffffffff)
        return nullptr;
    return net_game_player(net_game(), i);
}

// 0x5bacd0 (ecx: an entity): its index in the world's player list, -1 when it is none
static DWORD __fastcall Player_IndexOf(void *entity)
{
    if (!entity || !is_kind_of(entity, character_class()))
        return 0xffffffff;
    return player_index_of(entity);
}

// 0x5babc0 (fastcall: a part, another): the net player of the part, else of the other one, else the
// local player (the world's slot 44, its slot 28 the index)
static BYTE *__fastcall Player_OfEither(void *part, void *other)
{
    if (part && is_kind_of(part, character_class())) {
        const DWORD i = player_index_of(part);
        if (i != 0xffffffff)
            if (BYTE *p = net_game_player(net_game(), i))
                return p;
    }
    if (other && is_kind_of(other, character_class())) {
        const DWORD i = player_index_of(other);
        if (i != 0xffffffff)
            if (BYTE *p = net_game_player(net_game(), i))
                return p;
    }
    const DWORD local = vcall<DWORD>(vcall<void *>(world(), 44), 28);
    return net_game_player(net_game(), local);
}

// ---- GameSpy keys --------------------------------------------------------------------------------------------

// 0x5bad00 (the net game's GameSpy rules): "mapname" (the current layer's file name without its
// folder), "gameclass" (the game type's short class name) and "gametype" (its name, +8)
static void THISCALL GameSpy_GameKeys(void *self, void *out)
{
    empty_slot(self, ptrv(out));
    if (fn<void *(THISCALL *)(void *)>(0x4f1050)(world())) {
        BYTE *layer = fn<BYTE *(THISCALL *)(void *)>(0x4f1050)(world());
        const char *path = fn<const char *(THISCALL *)(void *)>(0x5007a0)(layer + 0xc0);
        gamespy_text(out, glob_addr(0x61bea8) /* "mapname" */, fn<const char *(__fastcall *)(const char *)>(0x4a4570)(path));
    }
    BYTE *type = fn<BYTE *(THISCALL *)(void *)>(0x59ff80)(app());
    if (!type)
        return;
    CClassInfo *c = vcall<CClassInfo *>(type, 2);
    gamespy_text(out, glob_addr(0x627050) /* "gameclass" */, fn<const char *(THISCALL *)(void *)>(0x415370)(c));
    gamespy_text(out, glob_addr(0x61be9c) /* "gametype" */, fn<const char *(THISCALL *)(void *)>(0x5007a0)(type + 8));
}

// 0x5bad90 (the net game's GameSpy basics): "gamename" "zax", "gamever" (0x5908c0) and "gamebuild"
// (0x5908a0)
static void THISCALL GameSpy_BasicKeys(void *self, void *out)
{
    empty_slot(self, ptrv(out));
    gamespy_text(out, glob_addr(0x62705c) /* "gamename" */, glob<const char *>(0x615d38) /* "zax" */);
    ZString version;
    zs_ctor(&version);
    fn<void *(THISCALL *)(void *)>(0x5908c0)(&version);
    gamespy_text(out, glob_addr(0x61be94) /* "gamever" */, zs_c_str(&version));
    gamespy_int(out, glob_addr(0x61be68) /* "gamebuild" */, fn<DWORD(__cdecl *)()>(0x5908a0)());
    zs_dtor(&version);
}

// ---- The block readers -------------------------------------------------------------------------------------

// The accessors (mov eax, reader; ret)
template <DWORD READER> static DWORD __cdecl Reader_Get() { return glob_addr(READER); }

// 0x5bcd80 (chat message from the server): text and font (a word token of the font list 0x6be1e0).
// al: 1.
static DWORD THISCALL Chat_Write(void *self, void *packet, const char *text, DWORD font)
{
    pk_block_type(packet, self, 3);
    pk_write_text(packet, text);
    pk_write_word_token(packet, font, glob<void *>(0x6be1e0));
    return 1;
}
// 0x5bcd40: read; shown by the application (0x59b370 with the text and font). eax: its.
static DWORD THISCALL Chat_Read(void *, void *packet)
{
    const char *text = pk_read_text(packet);
    DWORD font = ptrv(packet);   // the original reads the word into its packet argument's slot
    pk_read_word_token(packet, &font, glob<void *>(0x6be1e0));
    return fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x59b370)(app(), text, font & 0xffff);
}

// 0x5bcf60 (start game info): the game type's index (0x5a01a0, a dword token of the game type list
// 0x6c02e8), the current level's path behind "Levels/" (its length skipped without a test) and the
// player's name (+0x24 of the second argument). al: 1.
static DWORD THISCALL StartInfo_Write(void *self, void *packet, BYTE *player)
{
    pk_block_type(packet, self, 0);
    pk_write_dword_token(packet, fn<DWORD(THISCALL *)(void *)>(0x5a01a0)(app()), glob<void *>(0x6c02e8));
    ZString path;
    zs_ctor(&path);
    fn<void *(THISCALL *)(void *, void *)>(0x4f3ac0)(world(), &path);
    const char *levels = glob<const char *>(0x615d48) /* "Levels/" */;
    DWORD n = 0;
    while (levels[n])
        n++;
    pk_write_text(packet, zs_c_str(&path) + n);
    pk_write_text(packet, zs_c_str(player + 0x24));
    zs_dtor(&path);
    return 1;
}

// 0x5bd1a0 (player info from the server): the changed bits (+0x10c), the scores they name (+0xec,
// +0xee, +0xf2, +0xf6; +0xf0 and +0xf4 are not sent), the team, the record bits (0x5ba2d0), the record
// count when it changed and every record to send (in full when new, else what changed). The debug
// markers 0xabababab/0xcdcdcdcd go to the empty slot 0x4186e0. al: 1.
static DWORD THISCALL PlayerInfo_Write(void *self, void *packet, BYTE *player)
{
    pk_block_type(packet, self, 1);
    pk_write_dword(packet, dw(player, 0x10c));
    if (dw(player, 0x10c) & 1)
        pk_write_word(packet, (WORD)wd(player, 0xec));
    if (dw(player, 0x10c) & 0x80)
        pk_write_word(packet, (WORD)wd(player, 0xee));
    if (dw(player, 0x10c) & 0x200)
        pk_write_word(packet, (WORD)wd(player, 0xf2));
    if (dw(player, 0x10c) & 4)
        pk_write_word(packet, (WORD)wd(player, 0xf6));
    pk_write_byte(packet, (BYTE)dw(player, 0x14));
    empty_slot(packet, 0xabababab);
    pk_write_dword(packet, fn<DWORD(THISCALL *)(void *)>(0x5ba2d0)(player));
    if (dw(player, 0x104) != dw(player, 0x110))
        pk_write_byte(packet, dw(player, 0x104));
    for (DWORD i = 0; i < dw(player, 0x104); i++) {
        BYTE *r = pt(pt(player, 0x100), i * 4);
        if (i >= dw(player, 0x110))
            fn<void(THISCALL *)(void *, void *)>(0x5b9fa0)(r, packet);
        else if ((BYTE)fn<DWORD(THISCALL *)(void *)>(0x5ba0e0)(r))
            fn<void(THISCALL *)(void *, void *)>(0x5b9f20)(r, packet);
        empty_slot(packet, 0xcdcdcdcd);
    }
    return 1;
}

// 0x5bd030: read into the local player (0x5ba810 with the index 0x5fe8f8, after the application's net
// layer is made, 0x5a33e0): the scores the bits name through their setters, the team, the record count
// (0x5ba450) and the records (0x5b9fe0), the markers checked; then the player's slot 1. Values that
// are not in the packet stay as the original's stack had them (the packet argument's slot for two
// scores, 0x4fcf for the others, 0xcf for the count). eax: slot 1's.
static DWORD THISCALL PlayerInfo_Read(void *, void *packet)
{
    DWORD bits = 0;
    pk_read_dword(packet, &bits);
    fn<void *(THISCALL *)(void *)>(0x5a33e0)(app());
    BYTE *player = fn<BYTE *(__fastcall *)(DWORD)>(0x5ba810)(*glob<DWORD *>(0x5fe8f8));
    DWORD w = 0x4fcf, arg = ptrv(packet);
    if (bits & 1) {
        pk_read_word(packet, &w);
        fn<DWORD(THISCALL *)(void *, int)>(0x5ba340)(player, (short)w);
    }
    if (bits & 0x80) {
        pk_read_word(packet, &arg);
        fn<DWORD(THISCALL *)(void *, int)>(0x5ba360)(player, (short)arg);
    }
    if (bits & 0x200) {
        pk_read_word(packet, &arg);
        fn<DWORD(THISCALL *)(void *, int)>(0x5ba3c0)(player, (short)arg);
    }
    if (bits & 4) {
        pk_read_word(packet, &w);
        fn<DWORD(THISCALL *)(void *, int)>(0x5ba420)(player, (short)w);
    }
    DWORD team = 0;
    pk_read_byte(packet, &team);
    dw(player, 0x14) = team & 0xff;
    pk_check_marker(packet, 0xabababab);
    DWORD records = 0;
    pk_read_dword(packet, &records);
    if (records & 1) {
        DWORD n = (arg & ~0xffu) | 0xcf;
        pk_read_byte(packet, &n);
        fn<void(THISCALL *)(void *, DWORD)>(0x5ba450)(player, n & 0xff);
    }
    DWORD bit = 2;
    for (DWORD i = 0; i < dw(player, 0x104); i++, bit <<= 1) {
        if (records & bit)
            fn<void(THISCALL *)(void *, void *)>(0x5b9fe0)(pt(pt(player, 0x100), i * 4), packet);
        pk_check_marker(packet, 0xcdcdcdcd);
    }
    return vcall<DWORD>(player, 1);
}

// The quip category list (0x55b940) and its record i (clamped), loaded as the original inlines it;
// null without records or while it is loaded
static void *quip_category(BYTE *list, DWORD i)
{
    BYTE *records = pt(list, 0x30);
    if (!records)
        return nullptr;
    if (i >= dw(list, 0x34))
        i = 0;
    if (records[i * 0x80 + 0x7f])
        return nullptr;
    if (!dw(records + i * 0x80, 0x78) && !records[i * 0x80 + 0x7f]) {
        records[i * 0x80 + 0x7f] = 1;
        char path[0x100];
        fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, reinterpret_cast<const char *>(list + 0x38),
                                                                             vcall<const char *>(list, 1, i));
        char *name = fn<char *(__fastcall *)(char *)>(0x4a4570)(path);
        void *made;
        if (name == path)
            made = vcall<void *>(list, 37, glob<const char *>(0x62e558), name, pt(list, 0x30) + i * 0x80 + 0x78);
        else {
            name[-1] = 0;
            made = vcall<void *>(list, 37, path, name, pt(list, 0x30) + i * 0x80 + 0x78);
        }
        dw(pt(list, 0x30) + i * 0x80, 0x78) = ptrv(made);
        pt(list, 0x30)[i * 0x80 + 0x7f] = 0;
    }
    return pt(pt(list, 0x30) + i * 0x80, 0x78);
}

// 0x5bd560 (say a quip): the category (a byte token of the quip category list) and whether to retrigger.
// al: 1.
static DWORD THISCALL Quip_Write(void *self, void *packet, DWORD category, DWORD retrigger)
{
    pk_block_type(packet, self, 3);
    void *list = fn<void *(__cdecl *)()>(0x55b940)();
    pk_write_byte_token(packet, category, list);
    pk_write_bool(packet, retrigger);
    return 1;
}
// 0x5bd310: read; with the flag the category's 0x55b770 (TryStart), without it 0x55b730 (Retrigger), on
// null when it has no object. eax: theirs.
static DWORD THISCALL Quip_Read(void *, void *packet)
{
    DWORD category = 0;
    pk_read_byte_token(packet, &category, fn<void *(__cdecl *)()>(0x55b940)());
    BYTE retrigger = 0;
    pk_read_bool(packet, &retrigger);
    void *q = quip_category(fn<BYTE *(__cdecl *)()>(0x55b940)(), category & 0xff);
    if (retrigger)
        return fn<DWORD(THISCALL *)(void *)>(0x55b770)(q);
    return fn<DWORD(THISCALL *)(void *)>(0x55b730)(q);
}

// 0x5bd640 (say a dialogue line): a byte token of the list 0x62e7e0, a word and a byte. al: 1.
static DWORD THISCALL Dialog_Write(void *self, void *packet, DWORD token, DWORD word, DWORD byte)
{
    pk_block_type(packet, self, 3);
    pk_write_byte_token(packet, token, glob<void *>(0x62e7e0));
    pk_write_word(packet, word);
    pk_write_byte(packet, byte);
    return 1;
}
// 0x5bd5e0: read; passed to 0x5a5120 (fastcall: the token, the word; the byte). eax: its.
static DWORD THISCALL Dialog_Read(void *, void *packet)
{
    DWORD token = 0, word = 0, byte = ptrv(packet);
    pk_read_byte_token(packet, &token, glob<void *>(0x62e7e0));
    pk_read_word(packet, &word);
    pk_read_byte(packet, &byte);
    return fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x5a5120)(token & 0xff, word & 0xffff, byte & 0xff);
}

// 0x5bd6c0 (show the match over window): the window (pool 0x1b4, its constructor 0x50cab0 with the
// reporter's 0x4cdf30, 0, 2 and the game type's name text) shown through the reporter's slot 23 (null
// without a block). eax: slot 23's.
static DWORD THISCALL MatchOver_Read(void *, void *)
{
    void *window = pool_alloc(0x1b4);
    if (window) {
        BYTE *type = fn<BYTE *(THISCALL *)(void *)>(0x59ff80)(app());
        const DWORD text = fn<DWORD(THISCALL *)(void *)>(0x5007b0)(type + 8);
        const DWORD owner = fn<DWORD(THISCALL *)(void *)>(0x4cdf30)(reporter());
        window = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x50cab0)(window, owner, 0, 2, text);
    }
    return vcall<DWORD>(reporter(), 23, window, 1);
}

// 0x5bd770 (hide the match over window): 0x50db70
static DWORD THISCALL MatchOverHide_Read(void *, void *) { return fn<DWORD(__cdecl *)()>(0x50db70)(); }

// 0x5bd810 (an inventory icon in the HUD): the item definition (a word token of the catalogue) and a
// byte. al: 1.
static DWORD THISCALL Icon_Write(void *self, void *packet, DWORD def, DWORD flag)
{
    pk_block_type(packet, self, 3);
    pk_write_word_token(packet, def, glob<void *>(CATALOGUE));
    pk_write_byte(packet, flag);
    return 1;
}
// 0x5bd7c0: read; the application's 0x5a34b0 with them. eax: its.
static DWORD THISCALL Icon_Read(void *self, void *packet)
{
    DWORD def = ptrv(self), flag = ptrv(packet);   // the original reads into the slots of ecx and its argument
    pk_read_word_token(packet, &def, glob<void *>(CATALOGUE));
    pk_read_byte(packet, &flag);
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x5a34b0)(app(), def & 0xffff, flag & 0xff);
}

// 0x5bd890 (the scoreboard update): read into the scoreboard (0x566a80) by its 0x566fd0. eax: its.
static DWORD THISCALL Scoreboard_Read(void *, void *packet)
{
    return fn<DWORD(THISCALL *)(void *, void *)>(0x566fd0)(fn<void *(__cdecl *)()>(0x566a80)(), packet);
}

// 0x5bd8b0 (thiscall on the packet registry): the warp effect reader (0x571b60, thiscall on the same registry: the
// original's `mov esi, ecx; call 0x571b60` at 0x5bd8b1; until 2026-10-31 called without it, and ecx happened to still
// hold self, which the coverage build's instrumentation call broke) and the nine readers registered by name. eax: the
// last registration's.
static DWORD THISCALL Readers_Register(void *self)
{
    fn<DWORD(THISCALL *)(void *)>(0x571b60)(self);
    struct Reader {
        GlobKey reader, name;
    };
    const Reader readers[] = {{0x714490, 0x62727c}, {0x714488, 0x627258}, {0x7144c8, 0x6272b4}, {0x714480, 0x6272dc}, {0x7144b0, 0x627304},
                         {0x7144b8, 0x627330}, {0x7144a8, 0x627364}, {0x7144c0, 0x627398}, {0x7144a0, 0x6273d0}};
    DWORD r = 0;
    for (const auto &e : readers)
        r = fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x51ff30)(self, glob<void *>(e.reader), glob<const char *>(e.name));
    return r;
}

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(zax_net)
{
    const Hook list[] = {
#include "zax_net_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
