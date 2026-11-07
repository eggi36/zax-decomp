// The multiplayer scoreboard 0x5663f0-0x5675cb and the server's fill of it 0x5bae00 (phase 3,
// multiplayer; docs/DECOMPILATION.md section 8): its rows and team rows, their setters, the order by
// score, the scoreboard update block written and read.
//
// Layout [confirmed: the static constructor inlined in 0x566a80 and in every setter, the atexit
// destructor 0x566b10, Write 0x566ea0, Read 0x566fd0]: one function-local static of 0x40 bytes at
// 0x713220 (guard bit 0 of 0x713260):
//   +0x00 the rows: a pool pointer array (vtable 0x5ff33c, 0x5ff348 after the destructor; data +4,
//         count +8, capacity +0xc) of rows (0x1c bytes, operator new): +0 the name (ZString), +4 a byte
//         (whether the player carries the multiplayer flag), +8 the team, +0xc the score, +0x10 and +0x14
//         the player's +0xf2 and +0xee, +0x18 the ping (the net layer's slot 22)
//   +0x10 the order: a pool pointer array (0x5ee138, 0x5ee15c) of row indices, sorted by score (0x567280)
//   +0x20 the team rows: a pool pointer array (0x5ff330, 0x5ff354) of 8 bytes: +0 the name, +4 the score
//   +0x30 changed (a byte), +0x34 the ticks until it counts as changed again (200), +0x38 the frag limit,
//   +0x3c the time limit
// Every setter of a row or team row marks the static scoreboard changed (+0x30 1, +0x34 200) when the
// value differs, constructing it first if needed (the guard and atexit inlined); the scoreboard's own
// setters mark themselves.
//
// The arrays' deleting destructors 0x5669e0 and 0x566a30 are the pool pointer arrays' of
// ptr_array_dtors.inc (Core). Not hookable (4 bytes or less): 0x5664b0 (a row's name: this), 0x566570 (its byte +4), 0x5669d0 (a team
// row's score), 0x566cf0 (the team count +0x28; the fill also calls it on the net game), 0x567550 and
// 0x567580 (+0x38, +0x3c).
#include "class_info.h"
#include "exe_base.h"

namespace zax {

namespace {

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
short &wd(void *p, DWORD off) { return *reinterpret_cast<short *>(static_cast<BYTE *>(p) + off); }
BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

void zs_ctor(void *z) { zax::fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_dtor(void *z) { zax::fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
int zs_compare(const void *a, const void *b) { return zax::fn<int(THISCALL *)(const void *, const void *)>(0x4e1700)(a, b); }
void zs_assign(void *z, const void *v) { zax::fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(z, v); }
const char *zs_c_str(const void *z) { return zax::fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
void ptr_array_remove_at(void *a, DWORD i) { zax::fn<void(THISCALL *)(void *, DWORD)>(0x586fc0)(a, i); }
void *__cdecl memmove_exe(void *d, const void *s, DWORD n) { return zax::fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }
void *app() { return *glob<void **>(0x713f14); }
void *world() { return *glob<void **>(0x6c2080); }

// Packet functions (thiscall on the packet)
DWORD pk(DWORD fn, void *p, DWORD v) { return fn_dyn<DWORD(THISCALL *)(void *, DWORD)>(fn)(p, v); }
DWORD pk_ptr(DWORD fn, void *p, void *out) { return fn_dyn<DWORD(THISCALL *)(void *, void *)>(fn)(p, out); }
constexpr DWORD WRITE_BYTE = 0x520820, WRITE_WORD = 0x5209c0, WRITE_DWORD = 0x520940, WRITE_BOOL = 0x520a50, WRITE_TEXT = 0x520770;
constexpr DWORD READ_BYTE = 0x520490, READ_WORD = 0x520510, READ_DWORD = 0x520550, READ_BOOL = 0x520580, READ_STRING = 0x520700;

constexpr DWORD BOARD = 0x713220, GUARD = 0x713260, BOARD_DTOR = 0x566b10;
constexpr DWORD VT_ROWS = 0x5ff33c, VT_ROWS_BASE = 0x5ff348, VT_ORDER = 0x5ee138, VT_ORDER_BASE = 0x5ee15c, VT_TEAMS = 0x5ff330,
                VT_TEAMS_BASE = 0x5ff354;
BYTE *board() { return glob<BYTE *>(BOARD); }

// The static scoreboard constructed on first use (as every user inlines it): the three arrays empty,
// the rest 0, its destructor registered with atexit
void board_static()
{
    BYTE &guard = *glob<BYTE *>(GUARD);
    if (guard & 1)
        return;
    guard |= 1;
    BYTE *b = board();
    dw(b, 4) = 0;
    dw(b, 8) = 0;
    dw(b, 0xc) = 0;
    dw(b, 0) = vt(VT_ROWS);
    dw(b, 0x14) = 0;
    dw(b, 0x18) = 0;
    dw(b, 0x1c) = 0;
    dw(b, 0x10) = vt(VT_ORDER);
    dw(b, 0x24) = 0;
    dw(b, 0x28) = 0;
    dw(b, 0x2c) = 0;
    dw(b, 0x20) = vt(VT_TEAMS);
    b[0x30] = 0;
    dw(b, 0x34) = 0;
    dw(b, 0x38) = 0;
    dw(b, 0x3c) = 0;
    crt_atexit(code<void(__cdecl *)()>(BOARD_DTOR));
}
// Changed: the static scoreboard marked (constructed first), or a scoreboard itself
void mark(BYTE *b)
{
    b[0x30] = 1;
    dw(b, 0x34) = 0xc8;
}
void mark_static()
{
    board_static();
    mark(board());
}

// A pool pointer array's capacity made at least n (data from the pool, the old contents copied, the
// old data given back), as 0x5a4080 and the inlined copies of it do
void reserve(BYTE *array, DWORD n)
{
    const DWORD cap = dw(array, 0xc);
    if (cap >= n)
        return;
    dw(array, 0xc) = n;
    void *data = pool_get_alloc(n * 4);
    if (dw(array, 8))
        memmove_exe(data, pt(array, 4), dw(array, 8) * 4);
    if (void *old = pt(array, 4)) {
        pool_get_free(old, cap * 4);
        dw(array, 4) = 0;
    }
    dw(array, 4) = ptrv(data);
}
// The order array (+0x10) emptied (slot 1), made big enough and filled with 0 ... count - 1 (slot 2)
void rebuild_order(BYTE *b)
{
    BYTE *order = b + 0x10;
    const DWORD n = dw(b, 8);
    vcall<DWORD>(order, 1);
    reserve(order, n);
    DWORD i = 0;
    while (i < n) {
        vcall<DWORD>(order, 2, &i);
        i++;
    }
}
void sort_order(BYTE *b)
{
    BYTE *order = b + 0x10;
    if (dw(order, 8))
        zax::fn<void(__cdecl *)(void *, DWORD, DWORD, void *)>(0x5d097c)(pt(order, 4), dw(order, 8), 4, code<void *>(0x567280));   // qsort
}

}  // namespace

// ---- Rows and team rows ------------------------------------------------------------------------------------

// 0x5663f0: the name assigned when it differs (ZString_Compare). Void in use [assumption].
static void THISCALL Row_SetName(BYTE *self, const ZString *v)
{
    if (!zs_compare(self, v))
        return;
    zs_assign(self, v);
    mark_static();
}
// 0x5664c0: the byte +4. Void in use [assumption].
static void THISCALL Row_SetFlag(BYTE *self, DWORD v)
{
    if (self[4] == (BYTE)v)
        return;
    self[4] = (BYTE)v;
    mark_static();
}
// 0x566630 (+8), 0x566580 (+0xc), 0x5666e0 (+0x10), 0x566790 (+0x14); 0x566920 the team row's score
// (+4). Void in use [assumption].
template <DWORD OFF> static void THISCALL Row_Set(BYTE *self, DWORD v)
{
    if (dw(self, OFF) == v)
        return;
    dw(self, OFF) = v;
    mark_static();
}
// 0x566840: the ping (+0x18), not marked. Void in use [assumption].
static void THISCALL Row_SetPing(BYTE *self, DWORD v)
{
    if (dw(self, 0x18) != v)
        dw(self, 0x18) = v;
}
// 0x567270: a row's (or team row's) constructor: the name. eax: this.
static BYTE *THISCALL Row_Ctor(BYTE *self)
{
    zs_ctor(self);
    return self;
}
// 0x5675b0: the deleting destructor of both: the name, bit 0 frees (operator delete). eax: this.
static BYTE *THISCALL Row_DeletingDtor(BYTE *self, DWORD flags)
{
    zs_dtor(self);
    if (flags & 1)
        heap_free(self);
    return self;
}

// ---- The scoreboard ---------------------------------------------------------------------------------------------

// 0x566a80: the static scoreboard (constructed on first use)
static BYTE *__cdecl Scoreboard_Get()
{
    board_static();
    return board();
}

// 0x566b10 (registered with atexit): the rows deleted (their name destroyed, operator delete) and
// removed, the three arrays' data given back to the pool. The team rows are not deleted (as in the
// original).
static void __cdecl Scoreboard_Destroy()
{
    BYTE *b = board();
    while (dw(b, 8)) {
        const DWORD i = dw(b, 8) - 1;
        if (BYTE *r = pt(pt(b, 4), i * 4)) {
            zs_dtor(r);
            heap_free(r);
        }
        ptr_array_remove_at(b, i);
    }
    struct BaseArray {
        DWORD off;
        VtKey vtable;
    };
    const BaseArray arrays[] = {{0x20, VT_TEAMS_BASE}, {0x10, VT_ORDER_BASE}, {0, VT_ROWS_BASE}};
    for (const auto &a : arrays) {
        BYTE *array = b + a.off;
        dw(array, 0) = key_vt(a.vtable);
        if (void *data = pt(array, 4)) {
            pool_get_free(data, dw(array, 0xc) * 4);
            dw(array, 4) = 0;
            dw(array, 0xc) = 0;
        }
        dw(array, 8) = 0;
    }
}

// 0x567370, 0x567390, 0x5673b0, 0x567590: the arrays' constructors (empty, the vtable). eax: this.
template <DWORD VTABLE> static BYTE *THISCALL Array_Ctor(BYTE *self)
{
    dw(self, 4) = 0;
    dw(self, 8) = 0;
    dw(self, 0xc) = 0;
    dw(self, 0) = vt(VTABLE);
    return self;
}
// 0x566c20: the number of team rows set: the last ones deleted (their name, operator delete; the removal
// inlined) or new ones (operator new, 8 bytes, the name constructed) appended through slot 2; marked.
// Void in use [assumption].
static void THISCALL Scoreboard_SetTeamCount(BYTE *self, DWORD n)
{
    BYTE *teams = self + 0x20;
    while (dw(teams, 8) > n) {
        const DWORD i = dw(teams, 8) - 1;
        if (BYTE *t = pt(pt(teams, 4), i * 4)) {
            zs_dtor(t);
            heap_free(t);
        }
        const DWORD count = dw(teams, 8);
        if (i < count) {
            if (i + 1 < count)
                memmove_exe(pt(teams, 4) + i * 4, pt(teams, 4) + (i + 1) * 4, (count - (i + 1)) * 4);
            dw(teams, 8) = count - 1;
        }
        mark(self);
    }
    while (dw(teams, 8) < n) {
        BYTE *t = static_cast<BYTE *>(heap_alloc(8));
        if (t)
            zs_ctor(t);
        vcall<DWORD>(teams, 2, &t);
        mark(self);
    }
}
// 0x566d00: team row i
static BYTE *THISCALL Scoreboard_TeamAt(BYTE *self, DWORD i) { return pt(pt(self, 0x24), i * 4); }

// 0x566d10: the number of rows set: the last ones deleted (their name, operator delete) and removed
// (PtrArray_RemoveAt), or new ones (operator new, 0x1c bytes, the name constructed) appended; marked;
// then the order rebuilt (0 ... count - 1, unsorted). Void in use [assumption].
static void THISCALL Scoreboard_SetRowCount(BYTE *self, DWORD n)
{
    while (dw(self, 8) > n) {
        const DWORD i = dw(self, 8) - 1;
        if (BYTE *r = pt(pt(self, 4), i * 4)) {
            zs_dtor(r);
            heap_free(r);
        }
        ptr_array_remove_at(self, i);
        mark(self);
    }
    while (dw(self, 8) < n) {
        BYTE *r = static_cast<BYTE *>(heap_alloc(0x1c));
        if (r)
            zs_ctor(r);
        vcall<DWORD>(self, 2, &r);
        mark(self);
    }
    rebuild_order(self);
}
// 0x566e40: row i; 0x566e50: row i in the order
static BYTE *THISCALL Scoreboard_RowAt(BYTE *self, DWORD i) { return pt(pt(self, 4), i * 4); }
static BYTE *THISCALL Scoreboard_SortedRowAt(BYTE *self, DWORD i) { return pt(pt(self, 4), dw(pt(self, 0x14), i * 4) * 4); }
// 0x566e70: not changed. Void in use [assumption].
static void THISCALL Scoreboard_ClearChanged(BYTE *self) { self[0x30] = 0; }
// 0x566e80: one tick: when the countdown runs out the scoreboard counts as changed again (200 more).
// al: changed (the upper bytes not reproduced [assumption: the caller tests al]).
static DWORD THISCALL Scoreboard_Tick(BYTE *self)
{
    const int left = (int)dw(self, 0x34) - 1;
    dw(self, 0x34) = (DWORD)left;
    if (left <= 0) {
        dw(self, 0x34) = 0xc8;
        self[0x30] = 1;
    }
    return self[0x30];
}
// 0x567530 (+0x38, the frag limit) and 0x567560 (+0x3c, the time limit), marked when they change. Void
// in use [assumption].
template <DWORD OFF> static void THISCALL Scoreboard_SetLimit(BYTE *self, DWORD v)
{
    if (dw(self, OFF) == v)
        return;
    dw(self, OFF) = v;
    mark(self);
}
// 0x5673d0: the order rebuilt; 0x567470: and sorted by score (qsort with 0x567280). Void in use
// [assumption].
static void THISCALL Scoreboard_RebuildOrder(BYTE *self) { rebuild_order(self); }
static void THISCALL Scoreboard_Sort(BYTE *self)
{
    rebuild_order(self);
    sort_order(self);
}
// 0x567280 (the qsort comparison of two row indices): the static scoreboard's second row's score minus
// the first's (highest first)
static int __cdecl Scoreboard_CompareRows(const DWORD *a, const DWORD *b)
{
    board_static();
    BYTE *first = pt(pt(board(), 4), *a * 4);
    board_static();
    return (int)(dw(pt(pt(board(), 4), *b * 4), 0xc) - dw(first, 0xc));
}

// 0x566ea0: the scoreboard update written (the reader 0x5bd880, block kind 2): the limits, every row
// (name, the byte +4, the team as a byte, the three dwords, the ping as a word), every team row (name,
// score) and the match time (the game type's slot 31, a float; 0 without a game type). Void in use
// [assumption].
static void THISCALL Scoreboard_Write(BYTE *self, void *packet)
{
    zax::fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x51ff50)(packet, zax::fn<void *(__cdecl *)()>(0x5bd880)(), 2);
    pk(fn_addr(WRITE_DWORD), packet, dw(self, 0x38));
    pk(fn_addr(WRITE_DWORD), packet, dw(self, 0x3c));
    const DWORD rows = dw(self, 8);
    pk(fn_addr(WRITE_BYTE), packet, rows);
    for (DWORD i = 0; i < rows; i++) {
        BYTE *r = pt(pt(self, 4), i * 4);
        zax::fn<DWORD(THISCALL *)(void *, const char *)>(WRITE_TEXT)(packet, zs_c_str(r));
        pk(fn_addr(WRITE_BOOL), packet, r[4]);
        pk(fn_addr(WRITE_BYTE), packet, r[8]);
        pk(fn_addr(WRITE_DWORD), packet, dw(r, 0xc));
        pk(fn_addr(WRITE_DWORD), packet, dw(r, 0x10));
        pk(fn_addr(WRITE_DWORD), packet, dw(r, 0x14));
        pk(fn_addr(WRITE_WORD), packet, (WORD)dw(r, 0x18));
    }
    const DWORD teams = dw(self, 0x28);
    pk(fn_addr(WRITE_BYTE), packet, teams);
    for (DWORD i = 0; i < teams; i++) {
        BYTE *t = pt(pt(self, 0x24), i * 4);
        zax::fn<DWORD(THISCALL *)(void *, const char *)>(WRITE_TEXT)(packet, zs_c_str(t));
        pk(fn_addr(WRITE_DWORD), packet, dw(t, 4));
    }
    float time = 0;
    if (zax::fn<void *(THISCALL *)(void *)>(0x59ff80)(app()))
        time = vcall<float>(zax::fn<void *(THISCALL *)(void *)>(0x59ff80)(app()), 31);
    DWORD bits;
    memcpy(&bits, &time, 4);
    pk(fn_addr(WRITE_DWORD), packet, bits);
}

// 0x566fd0: read: the limits, the row count (rows deleted through 0x5675b0 and removed, or made with
// 0x567270), the order rebuilt (0x5673d0), the rows, the order again (slot 1, 0x5a4080, the indices,
// sorted with 0x567280), the team rows likewise (removed inline through 0x586fc0 on +0x20), and the match
// time given to the game type (slot 32). eax: slot 32's, or the game type lookup's 0.
static DWORD THISCALL Scoreboard_Read(BYTE *self, void *packet)
{
    pk_ptr(fn_addr(READ_DWORD), packet, self + 0x38);
    pk_ptr(fn_addr(READ_DWORD), packet, self + 0x3c);
    DWORD count = 0;
    pk_ptr(fn_addr(READ_BYTE), packet, &count);
    const DWORD rows = count & 0xff;
    while (dw(self, 8) > rows) {
        const DWORD i = dw(self, 8) - 1;
        if (BYTE *r = pt(pt(self, 4), i * 4))
            zax::fn<void *(THISCALL *)(void *, DWORD)>(0x5675b0)(r, 1);
        ptr_array_remove_at(self, i);
        mark(self);
    }
    while (dw(self, 8) < rows) {
        BYTE *r = static_cast<BYTE *>(heap_alloc(0x1c));
        if (r)
            r = zax::fn<BYTE *(THISCALL *)(void *)>(0x567270)(r);
        vcall<DWORD>(self, 2, &r);
        mark(self);
    }
    zax::fn<void(THISCALL *)(void *)>(0x5673d0)(self);
    for (DWORD i = 0; i < (BYTE)count; i++) {
        BYTE *r = pt(pt(self, 4), i * 4);
        pk_ptr(fn_addr(READ_STRING), packet, r);
        pk_ptr(fn_addr(READ_BOOL), packet, r + 4);
        DWORD team = 0;
        pk_ptr(fn_addr(READ_BYTE), packet, &team);
        dw(r, 8) = team & 0xff;
        pk_ptr(fn_addr(READ_DWORD), packet, r + 0xc);
        pk_ptr(fn_addr(READ_DWORD), packet, r + 0x10);
        pk_ptr(fn_addr(READ_DWORD), packet, r + 0x14);
        DWORD ping = ptrv(packet);   // the original reads the word into its packet argument's slot
        pk_ptr(fn_addr(READ_WORD), packet, &ping);
        dw(r, 0x18) = (DWORD)(int)(short)ping;
    }
    BYTE *order = self + 0x10;
    const DWORD n = dw(self, 8);
    vcall<DWORD>(order, 1);
    zax::fn<void(THISCALL *)(void *, DWORD)>(0x5a4080)(order, n);
    DWORD i = 0;
    while (i < n) {
        vcall<DWORD>(order, 2, &i);
        i++;
    }
    sort_order(self);
    DWORD team_count = 0;
    pk_ptr(fn_addr(READ_BYTE), packet, &team_count);
    const DWORD teams = team_count & 0xff;
    BYTE *tarray = self + 0x20;
    while (dw(tarray, 8) > teams) {
        const DWORD k = dw(tarray, 8) - 1;
        if (BYTE *t = pt(pt(tarray, 4), k * 4))
            zax::fn<void *(THISCALL *)(void *, DWORD)>(0x5675b0)(t, 1);
        ptr_array_remove_at(tarray, k);
        mark(self);
    }
    while (dw(tarray, 8) < teams) {
        BYTE *t = static_cast<BYTE *>(heap_alloc(8));
        if (t)
            t = zax::fn<BYTE *(THISCALL *)(void *)>(0x567270)(t);
        vcall<DWORD>(tarray, 2, &t);
        mark(self);
    }
    for (DWORD k = 0; k < (BYTE)team_count; k++) {
        BYTE *t = pt(pt(tarray, 4), k * 4);
        pk_ptr(fn_addr(READ_STRING), packet, t);
        pk_ptr(fn_addr(READ_DWORD), packet, t + 4);
    }
    DWORD time = 0;
    pk_ptr(fn_addr(READ_DWORD), packet, &time);
    if (!zax::fn<void *(THISCALL *)(void *)>(0x59ff80)(app()))
        return 0;
    return vcall<DWORD>(zax::fn<void *(THISCALL *)(void *)>(0x59ff80)(app()), 32, time);
}

// 0x5bae00 (the net game's slot, on the server, thiscall on the net game): the scoreboard filled: the
// limits from the net game's settings, one row per player (name 0x5b7aa0, whether its entity's
// inventory holds the "Multiplayer Flag" category, team, score +0xf6, +0xf2, +0xee, ping from the net
// layer's slot 22), sorted, one team row per team (name, score); when it is due (0x566e80) sent to
// every connection but the first (the net game's slot 13). Void in use [assumption].
static void THISCALL Scoreboard_Fill(BYTE *self)
{
    if (!vcall<BYTE>(vcall<void *>(app(), 1), 12))
        return;
    vcall<DWORD>(vcall<void *>(app(), 1), 13);
    BYTE *b = zax::fn<BYTE *(__cdecl *)()>(0x566a80)();
    if (BYTE *settings = pt(vcall<void *>(world(), 1), 8)) {
        zax::fn<void(THISCALL *)(void *, DWORD)>(0x567530)(b, dw(settings, 0x14));
        zax::fn<void(THISCALL *)(void *, DWORD)>(0x567560)(b, dw(settings, 0x10));
    }
    const DWORD n = vcall<DWORD>(self, 11);
    zax::fn<void(THISCALL *)(void *, DWORD)>(0x566d10)(b, n);
    for (DWORD i = 0; i < n; i++) {
        BYTE *player = zax::fn<BYTE *(THISCALL *)(void *, DWORD)>(0x51f440)(vcall<void *>(world(), 1), i);
        BYTE *row = zax::fn<BYTE *(THISCALL *)(void *, DWORD)>(0x566e40)(b, i);
        BYTE *list = static_cast<BYTE *>(world()) + 0x284;
        bool flag = false;
        if (zax::fn<DWORD(THISCALL *)(void *)>(0x4ef8f0)(list) > i)
            if (BYTE *entity = *zax::fn<BYTE **(THISCALL *)(void *, DWORD)>(0x4ef8e0)(list, i))
                if (void *inv = zax::fn<void *(THISCALL *)(void *)>(0x4267e0)(entity)) {
                    BYTE &guard = *glob<BYTE *>(0x714460);
                    if (!(guard & 1)) {
                        guard |= 1;
                        *glob<DWORD *>(0x714454) =
                            zax::fn<DWORD(THISCALL *)(void *, const char *, int)>(0x591fc0)(glob<void *>(0x6c0800), glob<const char *>(0x627068), -1);
                    }
                    flag = zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x425290)(inv, *glob<DWORD *>(0x714454)) != 0xffffffff;
                }
        zax::fn<void(THISCALL *)(void *, const void *)>(0x5663f0)(row, zax::fn<void *(THISCALL *)(void *)>(0x5b7aa0)(player));
        zax::fn<void(THISCALL *)(void *, DWORD)>(0x5664c0)(row, flag);
        zax::fn<void(THISCALL *)(void *, DWORD)>(0x566630)(row, dw(player, 0x14));
        zax::fn<void(THISCALL *)(void *, DWORD)>(0x566580)(row, (DWORD)(int)wd(player, 0xf6));
        zax::fn<void(THISCALL *)(void *, DWORD)>(0x5666e0)(row, (DWORD)(int)wd(player, 0xf2));
        zax::fn<void(THISCALL *)(void *, DWORD)>(0x566790)(row, (DWORD)(int)wd(player, 0xee));
        const DWORD ping = vcall<DWORD>(vcall<void *>(app(), 1), 22, player);
        zax::fn<void(THISCALL *)(void *, DWORD)>(0x566840)(row, ping);
    }
    zax::fn<void(THISCALL *)(void *)>(0x567470)(b);
    const DWORD teams = zax::fn<DWORD(THISCALL *)(void *)>(0x566cf0)(vcall<void *>(world(), 1));
    zax::fn<void(THISCALL *)(void *, DWORD)>(0x566c20)(b, teams);
    for (DWORD i = 0; i < teams; i++) {
        BYTE *team = zax::fn<BYTE *(THISCALL *)(void *, DWORD)>(0x51f460)(vcall<void *>(world(), 1), i);
        BYTE *row = zax::fn<BYTE *(THISCALL *)(void *, DWORD)>(0x566d00)(b, i);
        zax::fn<void(THISCALL *)(void *, DWORD)>(0x566860)(row, zax::fn<DWORD(THISCALL *)(void *)>(0x5007b0)(team));
        zax::fn<void(THISCALL *)(void *, DWORD)>(0x566920)(row, zax::fn<DWORD(THISCALL *)(void *)>(0x482de0)(team));
    }
    if (!(BYTE)zax::fn<DWORD(THISCALL *)(void *)>(0x566e80)(b))
        return;
    zax::fn<void(THISCALL *)(void *)>(0x566e70)(b);
    for (DWORD i = 1; i < n; i++)
        zax::fn<void(THISCALL *)(void *, void *)>(0x566ea0)(b, vcall<void *>(self, 13, i));
}

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(scoreboard)
{
    const Hook list[] = {
#include "scoreboard_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
