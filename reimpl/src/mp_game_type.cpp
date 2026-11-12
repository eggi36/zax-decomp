// The multiplayer game mode base class and the match state reimplemented (see mp_game_type.h).
#include "mp_game_type.h"
#include "definitions.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD,
                                            void *, void *, void *);
static void obj_ctor(void *o) { fn<void(THISCALL *)(void *)>(0x416610)(o); }
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static void obj_dtor(void *o) { fn<void(THISCALL *)(void *)>(0x4166d0)(o); }
static CClassInfo *obj_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x4163b0)(); }
static DWORD obj_is_kind_of(void *o, CClassInfo *c) { return fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static DWORD set_help(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419020)(f, at<const char *>(text)); }
static DWORD add_enum(void *f, DWORD value, DWORD text) { return fn<DWORD(THISCALL *)(void *, DWORD, const char *)>(0x54da60)(f, value, at<const char *>(text)); }
static CClassInfo *game_type_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x4ce850)(); }   // CGameType
static CClassInfo *action_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x41a650)(); }      // CAction
static CClassInfo *entity_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x49a930)(); }
static CClassInfo *custom_rules_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x5a1350)(); }   // CZaxCustomRules

static void zs_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static DWORD zs_dtor(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
static const char *zs_global(DWORD z) { return zs_c_str(at<const void *>(z)); }
template <typename... A> static void zs_format(ZString *z, const char *fmt, A... a) { fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(z, fmt, a...); }
template <typename... A> static void zs_append_format(ZString *z, const char *fmt, A... a) { fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e0a30)(z, fmt, a...); }
static void path_join(char *out, const char *dir, const char *name) { fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, dir, name); }
static char *path_name(char *path) { return fn<char *(__fastcall *)(char *)>(0x4a4570)(path); }

static BYTE *app() { return *glob<BYTE **>(MP_APP); }
static BYTE *world() { return *glob<BYTE **>(MP_WORLD); }
static BYTE *net_game() { return vcall<BYTE *>(world(), 1); }
static DWORD dword_at(const void *o, DWORD off) { return *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(o) + off); }
static DWORD prop_count(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x482de0)(o); }        // +8
static DWORD extra_count(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x483750)(o); }       // +0xc
static DWORD array_count(void *a) { return fn<DWORD(THISCALL *)(void *)>(0x4ef8f0)(a); }       // +0x10
static void **array_slot(void *a, DWORD i) { return fn<void **(THISCALL *)(void *, DWORD)>(0x4ef8e0)(a, i); }
static DWORD dword14(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4fc260)(o); }           // +0x14
static DWORD dword18(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x51f450)(o); }           // +0x18
static void *team_at(void *ng, DWORD i) { return fn<void *(THISCALL *)(void *, DWORD)>(0x51f460)(ng, i); }
static DWORD set_team_count(void *ng, DWORD n) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x51f480)(ng, n); }
static DWORD free_player_slot(void *ng) { return fn<DWORD(THISCALL *)(void *)>(0x51f560)(ng); }
static DWORD smallest_team(void *ng, DWORD n) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x51f5a0)(ng, n); }
static DWORD named_set_value(void *o, DWORD v) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x51f750)(o, v); }
static BYTE *player_at(DWORD i) { return fn<BYTE *(__fastcall *)(DWORD)>(0x5ba810)(i); }
static DWORD set_lead_state(BYTE *p, BYTE s) { return fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x5ba440)(p, s); }
static DWORD set_score(BYTE *p, int s) { return fn<DWORD(THISCALL *)(BYTE *, int)>(0x5ba420)(p, s); }
static void *player_name(void *p) { return fn<void *(THISCALL *)(void *)>(0x5b7aa0)(p); }        // the ZString at +0x1c
static BYTE *player_of(void *entity) { return fn<BYTE *(THISCALL *)(void *)>(0x5bac80)(entity); }
static int player_index_of(void *entity) { return fn<int(THISCALL *)(void *)>(0x5bacd0)(entity); }
static DWORD fire_action(void *action, void *self, void **slot)
{
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, void *, void **)>(0x41a720)(action, 0, 0, 0, 0, self, slot);
}
static DWORD send_block(void *reader, void *connection) { return fn<DWORD(THISCALL *)(void *, void *)>(0x5ac330)(reader, connection); }
static DWORD show_message(const char *text) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x59b260)(app(), text, -1); }
static DWORD draw_text(void *font, void *surface, DWORD x, DWORD y, DWORD text, DWORD just)
{
    return fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4a6470)(font, surface, x, y, text, just, 0);
}
static DWORD draw_number(void *font, void *surface, DWORD x, DWORD y, DWORD value, DWORD just)
{
    return fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD)>(0x4a6b90)(font, surface, x, y, value, just);
}
static DWORD font_height(void *font) { return fn<DWORD(THISCALL *)(void *)>(0x4a6140)(font); }

constexpr DWORD MATCH_OVER_READER = 0x5bd6b0, NEW_MATCH_READER = 0x5bd760;   // they return the static block readers
constexpr DWORD EMPTY = 0x62e558;   // ""

// ---- The class getters ------------------------------------------------------------------------------

static DWORD __cdecl GameData_RegisterFields();

// Inlined in every user of the match state: registers CMultiPlayerGameData on first use
static CClassInfo *game_data_class()
{
    CClassInfo *&v = *glob<CClassInfo **>(MP_GAME_DATA_CLASS);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(0x61bef0) /* "CMultiPlayerGameData" */, 0.0f, obj_class_info(), 0x18,
                                     0, code<void *>(0x51a040), code<void *>(0x51a070), code<void *>(0x51a160));
    return v;
}

// 0x51a270 and its copy 0x51a2c0 (slot 2 of 0x5fb160); inlined into RegisterFields
static CClassInfo *__cdecl GameType_ClassInfo()
{
    CClassInfo *&v = *glob<CClassInfo **>(MP_GAME_TYPE_CLASS);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(0x61bf38) /* "CMultiPlayerGameType" */, 0.0f, game_type_class_info(),
                                     0xc8, 1, code<void *>(0x46b760), code<void *>(0x52dc80), code<void *>(0x51a320));
    return v;
}

// The current match state: the application's slot 97, +0x30, if it is a CMultiPlayerGameData, else null. Every
// user in the exe reads or writes the field through it without a test, so without a match state (or with an object
// of another class there) the exe reads address 8 or 0x10, or writes 0x10, and the game ends with an access
// violation [verified 2026-11-12: e.g. 0x51c25f-0x51c261 `xor esi, esi; fld qword ptr [esi + 0x10]`]. The empty
// asm hides the null from the compiler, which otherwise takes the dereference as proof of a non-null pointer and
// drops the null path (until 2026-11-12 it left out the null test and IsKindOf's answer and read the field of
// whatever object was there; batch decomp-faithful2, docs/DECOMPILATION.md 16), so the same field access happens
// at the same address near 0.
static CMultiPlayerGameData *game_data()
{
    void *o = *reinterpret_cast<void **>(vcall<BYTE *>(app(), 97) + 0x30);
    CMultiPlayerGameData *d = nullptr;
    if (o && (obj_is_kind_of(o, game_data_class()) & 0xff))
        d = static_cast<CMultiPlayerGameData *>(o);
    __asm__("" : "+r"(d));
    return d;
}

// ---- CMultiPlayerGameData ------------------------------------------------------------------------------

void CMultiPlayerGameData::construct()
{
    obj_ctor(this);
    vftable = zax::vt(MP_GAME_DATA_VTABLE);
    obj_init_fields(this);
}

void CMultiPlayerGameData::destroy()
{
    vftable = zax::vt(MP_GAME_DATA_VTABLE);
    obj_free_fields(this);
    obj_dtor(this);
}

// 0x51a040
static void *__cdecl GameData_Create()
{
    auto *o = static_cast<CMultiPlayerGameData *>(pool_alloc(sizeof(CMultiPlayerGameData)));
    if (!o)
        return nullptr;
    o->construct();
    return o;
}

// 0x51a070: heap block with the count in front, every element constructed (none for a count below 1)
static void *__fastcall GameData_CreateArray(int count, int, int)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc((DWORD)count * sizeof(CMultiPlayerGameData) + 4));
    if (!block)
        return nullptr;
    block[0] = (DWORD)count;
    auto *e = reinterpret_cast<CMultiPlayerGameData *>(block + 1);
    if (count - 1 >= 0)
        for (int i = 0; i < count; i++)
            e[i].construct();
    return e;
}

// 0x51a0d0: bit 1 destroys the array from the last element and returns the block, bit 0 frees
void *THISCALL CMultiPlayerGameData::DeletingDtor(DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(this) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                this[i - 1].destroy();
        if (flags & 1)
            heap_free(block);
        return block;
    }
    destroy();
    if (flags & 1)
        pool_free(this, sizeof(CMultiPlayerGameData));
    return this;
}

// 0x51a160: "Match Is Over" (byte +8), "Players Ready To Start New Game" (int +0xc, no limits).
// eax: the last AddField's.
static DWORD __cdecl GameData_RegisterFields()
{
    void *f = nullptr;
    if (void *p = heap_alloc(0x30))
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54ddf0)(
            p, glob<const char *>(0x61bf28) /* "Match Is Over" */, 8, 1, 8, 1, 0, 0);
    add_field(game_data_class(), f);
    f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54d6e0)(
            p, glob<const char *>(0x61bf08) /* "Players Ready To Start New Game" */, 0xc, 4, 0xc, 1, 0, 0, 0, 0);
    return add_field(game_data_class(), f);
}

// ---- CMultiPlayerGameType::RegisterFields ----------------------------------------------------------

// The 45 descriptors in the order of 0x51a320: allocation size, kind, name, the constructor's other
// arguments, the name added with AddEnum (value 0) and the help text (0 = none). The object fields get
// the CAction class (0x41a650, called after the allocation) as their fourth argument.
enum FieldKind : BYTE { K_ENUM_INT, K_BYTE, K_OBJECT, K_RESOURCE, K_TEXT, K_INT };
struct FieldReg {
    DWORD alloc;
    FieldKind kind;
    DWORD name;
    DWORD nargs;
    DWORD args[8];
    DWORD enum_name, help;
};
constexpr DWORD FIELD_CTOR[] = {0x54d6e0, 0x54ddf0, 0x4186f0, 0x54e560, 0x54d140, 0x54d480};
const FieldReg GAME_TYPE_FIELDS[] = {
    {0x44, K_ENUM_INT, 0x61c83c, 8, {0x18, 4, 0x18, 1, 0, 0, 1, 0xf423f}, 0x615c5c, 0x61c824},   // Team/Max Number of Teams
    {0x30, K_BYTE, 0x61c810, 6, {0x1c, 1, 0x1c, 1, 0, 0}, 0, 0x61c784},   // Team/Is Team Game
    {0x30, K_BYTE, 0x61c760, 6, {0x1d, 1, 0x1d, 1, 0, 0}, 0, 0x61c6ec},   // Team/All Teams Share Spawn Points
    {0x3c, K_OBJECT, 0x61c6d0, 8, {0x20, 4, 0x20, 0, 1, 0, 0, 0}, 0, 0},   // Events/Have Taken the Lead
    {0x3c, K_OBJECT, 0x61c6b4, 8, {0x24, 4, 0x24, 0, 1, 0, 0, 0}, 0, 0},   // Events/Have Lost the Lead
    {0x3c, K_OBJECT, 0x61c694, 8, {0x28, 4, 0x28, 0, 1, 0, 0, 0}, 0, 0},   // Events/Have Tied For the Lead
    {0x38, K_RESOURCE, 0x61c664, 8, {0x2c, 4, 0x2c, 0x6be1e0, 1, 0x60bb18, 0, 0}, 0, 0},   // Mini-Score Board/Default Mini-ScoreBoard Font
    {0x38, K_RESOURCE, 0x61c634, 8, {0x30, 4, 0x30, 0x6be1e0, 1, 0x60bb18, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Headings Font
    {0x38, K_RESOURCE, 0x61c604, 8, {0x34, 4, 0x34, 0x6be1e0, 1, 0x60c53c, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Blue Team Font
    {0x38, K_RESOURCE, 0x61c5d4, 8, {0x38, 4, 0x38, 0x6be1e0, 1, 0x60baa0, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Red Team Font
    {0x38, K_RESOURCE, 0x61c5a4, 8, {0x3c, 4, 0x3c, 0x6be1e0, 1, 0x60c53c, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Blue Flag Font
    {0x38, K_RESOURCE, 0x61c574, 8, {0x40, 4, 0x40, 0x6be1e0, 1, 0x60baa0, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Red Flag Font
    {0x30, K_TEXT, 0x61c540, 6, {0x44, 4, 0x44, 1, 0x61390c, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Blue Flag String
    {0x30, K_TEXT, 0x61c50c, 6, {0x48, 4, 0x48, 1, 0x61390c, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Red Flag String
    {0x40, K_INT, 0x61c4e4, 8, {0x4c, 4, 0x4c, 1, 0xa, 0, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Rank X
    {0x38, K_RESOURCE, 0x61c4b0, 8, {0x50, 4, 0x50, 0x6be1b8, 1, 0x6105cc, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Rank Justification
    {0x40, K_INT, 0x61c488, 8, {0x54, 4, 0x54, 1, 0xc, 0, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Flag X
    {0x38, K_RESOURCE, 0x61c454, 8, {0x58, 4, 0x58, 0x6be1b8, 1, 0x610630, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Flag Justification
    {0x40, K_INT, 0x61c42c, 8, {0x5c, 4, 0x5c, 1, 0x11, 0, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Name X
    {0x38, K_RESOURCE, 0x61c3f8, 8, {0x60, 4, 0x60, 0x6be1b8, 1, 0x610630, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Name Justification
    {0x40, K_INT, 0x61c3cc, 8, {0x64, 4, 0x64, 1, 0xaa, 0, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Score X
    {0x38, K_RESOURCE, 0x61c394, 8, {0x68, 4, 0x68, 0x6be1b8, 1, 0x6105cc, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Score Justification
    {0x40, K_INT, 0x61c36c, 8, {0x6c, 4, 0x6c, 1, 0x64, 0, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Ping X
    {0x38, K_RESOURCE, 0x61c338, 8, {0x70, 4, 0x70, 0x6be1b8, 1, 0x6105cc, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Ping Justification
    {0x40, K_INT, 0x61c308, 8, {0x74, 4, 0x74, 1, 0x78, 0, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard AltScore1 X
    {0x38, K_RESOURCE, 0x61c2cc, 8, {0x78, 4, 0x78, 0x6be1b8, 1, 0x6105cc, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard AltScore1 Justification
    {0x40, K_INT, 0x61c29c, 8, {0x7c, 4, 0x7c, 1, 0x8c, 0, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard AltScore2 X
    {0x38, K_RESOURCE, 0x61c260, 8, {0x80, 4, 0x80, 0x6be1b8, 1, 0x6105cc, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard AltScore2 Justification
    {0x38, K_RESOURCE, 0x61c228, 8, {0x84, 4, 0x84, 0x6be1b8, 1, 0x6139c8, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Rules Justification
    {0x40, K_INT, 0x61c1f8, 8, {0x88, 4, 0x88, 1, 2, 0, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Line Spacing
    {0x40, K_INT, 0x61c1c8, 8, {0x8c, 4, 0x8c, 1, 4, 0, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Header Spacing
    {0x40, K_INT, 0x61c1a0, 8, {0x90, 4, 0x90, 1, 2, 0, 0, 0}, 0, 0},   // Mini-Score Board/Mini-ScoreBoard Top
    {0x38, K_RESOURCE, 0x61c174, 8, {0x94, 4, 0x94, 0x6be1e0, 1, 0x60bb18, 0, 0}, 0, 0},   // End Score Board/Default End ScoreBoard Font
    {0x38, K_RESOURCE, 0x61c144, 8, {0x98, 4, 0x98, 0x6be1e0, 1, 0x60bb18, 0, 0}, 0, 0},   // End Score Board/End ScoreBoard Headings Font
    {0x38, K_RESOURCE, 0x61c114, 8, {0x9c, 4, 0x9c, 0x6be1e0, 1, 0x60c53c, 0, 0}, 0, 0},   // End Score Board/End ScoreBoard Blue Team Font
    {0x38, K_RESOURCE, 0x61c0e4, 8, {0xa0, 4, 0xa0, 0x6be1e0, 1, 0x60baa0, 0, 0}, 0, 0},   // End Score Board/End ScoreBoard Red Team Font
    {0x40, K_INT, 0x61c0bc, 8, {0xa4, 4, 0xa4, 1, 0xf, 0, 0, 0}, 0, 0},   // End Score Board/End ScoreBoard Rank X
    {0x38, K_RESOURCE, 0x61c088, 8, {0xa8, 4, 0xa8, 0x6be1b8, 1, 0x610630, 0, 0}, 0, 0},   // End Score Board/End ScoreBoard Rank Justification
    {0x40, K_INT, 0x61c060, 8, {0xac, 4, 0xac, 1, 0x46, 0, 0, 0}, 0, 0},   // End Score Board/End ScoreBoard Name X
    {0x38, K_RESOURCE, 0x61c02c, 8, {0xb0, 4, 0xb0, 0x6be1b8, 1, 0x6139c8, 0, 0}, 0, 0},   // End Score Board/End ScoreBoard Name Justification
    {0x40, K_INT, 0x61c004, 8, {0xb4, 4, 0xb4, 1, 0x8c, 0, 0, 0}, 0, 0},   // End Score Board/End ScoreBoard Score X
    {0x38, K_RESOURCE, 0x61bfd0, 8, {0xb8, 4, 0xb8, 0x6be1b8, 1, 0x6105cc, 0, 0}, 0, 0},   // End Score Board/End ScoreBoard Score Justification
    {0x40, K_INT, 0x61bfa4, 8, {0xbc, 4, 0xbc, 1, 2, 0, 0, 0}, 0, 0},   // End Score Board/End ScoreBoard Line Spacing
    {0x40, K_INT, 0x61bf74, 8, {0xc0, 4, 0xc0, 1, 4, 0, 0, 0}, 0, 0},   // End Score Board/End ScoreBoard Header Spacing
    {0x40, K_INT, 0x61bf50, 8, {0xc4, 4, 0xc4, 1, 2, 0, 0, 0}, 0, 0},   // End Score Board/End ScoreBoard Top
};

static void *make_field(const FieldReg &r)
{
    void *p = heap_alloc(r.alloc);
    if (!p)
        return nullptr;
    DWORD a[8];
    memcpy(a, r.args, sizeof a);
    if (r.kind == K_OBJECT)
        a[3] = (DWORD)(uintptr_t)action_class_info();
    if (r.kind == K_RESOURCE)   // the resource list and the default name: exe addresses through the layer (batch 6-10c)
        a[3] = glob_addr_dyn(a[3]), a[5] = glob_addr_dyn(a[5]);
    if (r.kind == K_TEXT)       // the default text
        a[4] = glob_addr_dyn(a[4]);
    const char *name = at<const char *>(r.name);
    const DWORD ctor = FIELD_CTOR[r.kind];
    if (r.nargs == 6)
        return fn_dyn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(ctor)(p, name, a[0], a[1], a[2],
                                                                                                            a[3], a[4], a[5]);
    return fn_dyn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(ctor)(
        p, name, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
}

// 0x51a320. eax: the last AddField's.
static DWORD __cdecl GameType_RegisterFields()
{
    DWORD r = 0;
    for (const FieldReg &reg : GAME_TYPE_FIELDS) {
        void *f = make_field(reg);
        if (reg.enum_name)
            add_enum(f, 0, reg.enum_name);
        if (reg.help)
            set_help(f, reg.help);
        r = add_field(GameType_ClassInfo(), f);
    }
    return r;
}

// ---- CMultiPlayerGameType ------------------------------------------------------------------------------

// 0x51bfa0: the CGameType constructor (0x4ceaa0), the abstract vtable 0x5fb160, the own fields' init
CMultiPlayerGameType *THISCALL CMultiPlayerGameType::Ctor()
{
    fn<void *(THISCALL *)(void *)>(0x4ceaa0)(this);
    vftable = zax::vt(MP_GAME_TYPE_VTABLE);
    obj_init_fields(this);
    return this;
}

// 0x51bff0. eax: the CGameType destructor's (0x4ceb20).
DWORD THISCALL CMultiPlayerGameType::Dtor()
{
    vftable = zax::vt(MP_GAME_TYPE_VTABLE);
    obj_free_fields(this);
    return fn<DWORD(THISCALL *)(void *)>(0x4ceb20)(this);
}

// 0x51bfc0 (slot 1 of 0x5fb160): the destructor, bit 0 gives the 0xc8 bytes back to the pool
CMultiPlayerGameType *THISCALL CMultiPlayerGameType::DeletingDtor(DWORD flags)
{
    vftable = zax::vt(MP_GAME_TYPE_VTABLE);
    obj_free_fields(this);
    fn<DWORD(THISCALL *)(void *)>(0x4ceb20)(this);
    if (flags & 1)
        pool_free(this, sizeof(CMultiPlayerGameType));
    return this;
}

// The font list's object i, loaded through 0x55e090 (slot 30 of the list) when it is missing; null when
// the list has no records or the record is being loaded. An index past the end means record 0.
static void *font_at(DWORD i)
{
    ZDefinitionRecord *e = *at<ZDefinitionRecord **>(glob_addr(FONT_LIST) + 0x30);
    if (!e)
        return nullptr;
    if (i >= *at<DWORD *>(glob_addr(FONT_LIST) + 0x34))
        i = 0;
    if (e[i].loading)
        return nullptr;
    if (!e[i].object) {
        fn<void *(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(FONT_LIST), i);
        e = *at<ZDefinitionRecord **>(glob_addr(FONT_LIST) + 0x30);
    }
    return e[i].object;
}

// The same with the load inlined: slot 37 of the list with the record's folder and name (slot 1)
static void *font_at_inlined(DWORD i)
{
    ZDefinitionRecord *e = *at<ZDefinitionRecord **>(glob_addr(FONT_LIST) + 0x30);
    if (!e)
        return nullptr;
    if (i >= *at<DWORD *>(glob_addr(FONT_LIST) + 0x34))
        i = 0;
    if (e[i].loading)
        return nullptr;
    if (!e[i].object && !e[i].loading) {
        e[i].loading = 1;
        void *list = glob<void *>(FONT_LIST);
        char path[0x100];
        path_join(path, at<const char *>(glob_addr(FONT_LIST) + 0x38), vcall<const char *>(list, 1, i));
        char *name = path_name(path);
        void *o;
        if (name == path) {
            o = vcall<void *>(list, 37, glob<const char *>(EMPTY), name, &(*at<ZDefinitionRecord **>(glob_addr(FONT_LIST) + 0x30))[i].object);
        } else {
            name[-1] = 0;
            o = vcall<void *>(list, 37, path, name, &(*at<ZDefinitionRecord **>(glob_addr(FONT_LIST) + 0x30))[i].object);
        }
        (*at<ZDefinitionRecord **>(glob_addr(FONT_LIST) + 0x30))[i].object = o;
        (*at<ZDefinitionRecord **>(glob_addr(FONT_LIST) + 0x30))[i].loading = 0;
        e = *at<ZDefinitionRecord **>(glob_addr(FONT_LIST) + 0x30);
    }
    return e[i].object;
}

// Sends the block of the reader to every connection (the world's slot 44: 11 count, 13 connection i)
static void send_to_all(DWORD reader)
{
    void *c = vcall<void *>(world(), 44);
    for (DWORD i = 0; i < vcall<DWORD>(c, 11); i++) {
        void *connection = vcall<void *>(c, 13, i);
        send_block(fn_dyn<void *(__cdecl *)()>(reader)(), connection);
    }
}

// The end of the match inlined in slots 24 and 26 and in 0x51c650: once only, the state marked over,
// nobody ready yet, and every connection told
static void end_match()
{
    if (game_data()->match_over)
        return;
    game_data()->match_over = 1;
    game_data()->players_ready = 0;
    send_to_all(fn_addr(MATCH_OVER_READER));
}

// 0x51c650
void THISCALL CMultiPlayerGameType::EndMatch() { end_match(); }

// 0x51c010: the CGameType part (0x4cebb0), a new match state (the old one is not deleted) and its
// defaults (slot 19), the team count for the net game (the maximum in team games, else 0), the state
// cleared, then slot 46 for every player entity of the world. eax: the entity count.
DWORD THISCALL CMultiPlayerGameType::StartMatch()
{
    fn<void(THISCALL *)(void *)>(0x4cebb0)(this);
    auto *d = static_cast<CMultiPlayerGameData *>(pool_alloc(sizeof(CMultiPlayerGameData)));
    if (d)
        d->construct();
    *reinterpret_cast<void **>(vcall<BYTE *>(app(), 97) + 0x30) = d;
    vcall<DWORD>(*reinterpret_cast<void **>(vcall<BYTE *>(app(), 97) + 0x30), 19);
    const DWORD teams = is_team_game && max_teams ? max_teams : 0;
    set_team_count(net_game(), teams);
    game_data()->match_over = 0;
    CMultiPlayerGameData *t = game_data();
    reinterpret_cast<DWORD *>(&t->match_time)[0] = 0;
    reinterpret_cast<DWORD *>(&t->match_time)[1] = 0;
    DWORD n = array_count(world() + 0x284);
    if (!n)
        return n;
    DWORD i = 0;
    do {
        if (*array_slot(world() + 0x284, i))
            vcall<DWORD>(this, 46, *array_slot(world() + 0x284, i));
        i++;
        n = array_count(world() + 0x284);
    } while (i < n);
    return n;
}

// 0x51c310: the application's update hook (its slot 1, slots 12/13), the base slot (0x4186e0, nothing),
// the match time advanced; on the server (the net game's slot 12) the match ends once the time limit
// (rules +0x10, minutes) is reached
void THISCALL CMultiPlayerGameType::Update(float dt)
{
    if (vcall<BYTE>(vcall<void *>(app(), 1), 12))
        vcall<DWORD>(vcall<void *>(app(), 1), 13);
    fn<void(THISCALL *)(void *, float)>(0x4186e0)(this, dt);
    CMultiPlayerGameData *d = game_data();
    d->match_time = (double)(to_float(dt) + (long double)d->match_time);
    if (!vcall<BYTE>(net_game(), 12))
        return;
    BYTE *rules = *reinterpret_cast<BYTE **>(net_game() + 8);
    if (!rules)
        return;
    if (!dword_at(rules, 0x10))
        return;
    CMultiPlayerGameData *t = game_data();
    const DWORD limit = dword_at(rules, 0x10) * 60;
    if (!x87_le((long double)limit, (long double)t->match_time))
        return;
    end_match();
}

// 0x51bb90: adds n to the score of team i of the net game (+8, through 0x51f750); the match ends when
// the score reaches the frag limit (rules +0x14, if set)
void THISCALL CMultiPlayerGameType::AddTeamScore(DWORD team, int n)
{
    void *t = team_at(net_game(), team);
    if (!t)
        return;
    const int score = (int)prop_count(t) + n;
    named_set_value(t, (DWORD)score);
    BYTE *rules = *reinterpret_cast<BYTE **>(net_game() + 8);
    if (!rules)
        return;
    const DWORD limit = dword_at(rules, 0x14);
    if (!limit || score < (int)limit)
        return;
    end_match();
}

// 0x51bda0: for every player compared with the best score (+0xf6) of the others: the lead state
// (+0xf8: 0 alone, 1 leads, 2 tied, 3 behind) and, when it changes to 1-3, the matching action fired.
// The connections' current index (slots 27/28) is set to each player and restored at the end. eax: that
// last slot 27's.
DWORD THISCALL CMultiPlayerGameType::UpdateLeads()
{
    const DWORD n = dword18(net_game());
    void *c = vcall<void *>(world(), 44);
    const DWORD saved = vcall<DWORD>(c, 28);
    for (DWORD i = 0; i < n; i++) {
        vcall<DWORD>(c, 27, i);
        BYTE *p = player_at(i);
        bool have = false;
        short best = 0;
        DWORD j = 0;
        do {
            BYTE *q = player_at(j);
            if (i != j && (!have || *reinterpret_cast<short *>(q + 0xf6) > best)) {
                best = *reinterpret_cast<short *>(q + 0xf6);
                have = true;
            }
        } while (++j < n);
        if (!have) {
            set_lead_state(p, 0);
            continue;
        }
        const short mine = *reinterpret_cast<short *>(p + 0xf6);
        const BYTE state = p[0xf8];
        void **slot;
        if (best < mine) {
            if (state == 1)
                continue;
            set_lead_state(p, 1);
            slot = &taken_lead;
        } else if (best == mine) {
            if (state == 2)
                continue;
            set_lead_state(p, 2);
            slot = &tied_lead;
        } else {
            if (state == 3)
                continue;
            set_lead_state(p, 3);
            slot = &lost_lead;
        }
        if (*slot)
            fire_action(*slot, this, slot);
    }
    return vcall<DWORD>(c, 27, saved);
}

// 0x51bef0: the team of a joining player (+0x14): in team games the smallest of the first max_teams
// (20 if unlimited), else a free player number. eax: that.
DWORD THISCALL CMultiPlayerGameType::AssignTeam(BYTE *player)
{
    *reinterpret_cast<DWORD *>(player + 0x14) = 0xffffffff;
    DWORD team;
    if (is_team_game)
        team = smallest_team(net_game(), max_teams ? max_teams : 0x14);
    else
        team = free_player_slot(net_game());
    *reinterpret_cast<DWORD *>(player + 0x14) = team;
    return team;
}

// 0x51d190: slot 46 with the second argument. eax: its.
DWORD THISCALL CMultiPlayerGameType::Slot29(DWORD, DWORD entity, DWORD) { return vcall<DWORD>(this, 46, entity); }

// 0x51c1f0: the match time (st0); without a match state it reads address 0x10, as the original
double THISCALL CMultiPlayerGameType::MatchTime() { return game_data()->match_time; }

// 0x51c270: sets the match time, except on the server (the net game's slot 12)
void THISCALL CMultiPlayerGameType::SetMatchTime(float t)
{
    if (vcall<BYTE>(net_game(), 12))
        return;
    game_data()->match_time = (double)to_float(t);
}

// 0x468f20, 0x51d150: al = the byte, the upper bytes of eax are the caller's (in assembly: the compiler
// would zero them)
extern "C" void zax_mp_share_spawn_points();
extern "C" void zax_mp_is_team_game();
__asm__(".text\n"
        ".globl _zax_mp_share_spawn_points\n"
        "_zax_mp_share_spawn_points:\n"
        "  movb 0x1d(%ecx), %al\n"
        "  retl\n"
        ".globl _zax_mp_is_team_game\n"
        "_zax_mp_is_team_game:\n"
        "  movb 0x1c(%ecx), %al\n"
        "  retl\n");

// 0x51d160: in team games the net game rules' byte +0x19 (1 without rules), else 1. eax: al with the
// upper bytes of slot 37's eax, 0 or the rules pointer.
DWORD THISCALL CMultiPlayerGameType::Slot38()
{
    const DWORD r = vcall<DWORD>(this, 37);
    if (!(r & 0xff))
        return (r & ~0xffu) | 1;
    BYTE *rules = *reinterpret_cast<BYTE **>(net_game() + 8);
    if (!rules)
        return 1;
    // mov al, [eax + 0x19] with eax = rules (written in assembly: clang turned the C++ form into movzx)
    DWORD v = (DWORD)(uintptr_t)rules;
    __asm__("movb 0x19(%1), %b0" : "+q"(v) : "r"(rules));
    return v;
}

// 0x51cba0: the mini score board: the rectangle of the custom rules' screen object (0x5a1fa0 + 0x28) set
// on the surface (slot 26), the headings, a line per entry of the ranking (0x566a80: rank, flag, name,
// score, the two alternative scores if not 0, ping) in the team or default font, then from the bottom up
// the time limit and the score limit lines in the default font, placed by the rules justification
void THISCALL CMultiPlayerGameType::DrawMiniScoreBoard(void *surface)
{
    BYTE *ui = *reinterpret_cast<BYTE **>(app() + 0x2e0);
    void *custom = nullptr;
    if (void *o = *reinterpret_cast<void **>(ui + 0x38))
        if (obj_is_kind_of(o, custom_rules_class_info()) & 0xff)
            custom = *reinterpret_cast<void **>(ui + 0x38);
    BYTE *screen = fn<BYTE *(THISCALL *)(void *)>(0x5a1fa0)(custom);
    vcall<DWORD>(surface, 26, screen + 0x28);
    void *font = font_at(mini_headings_font);
    DWORD y = mini_top;
    draw_text(font, surface, mini_name_x, y, (DWORD)(uintptr_t)zs_global(glob_addr(0x6d0018)), mini_name_just);
    draw_text(font, surface, mini_ping_x, y, (DWORD)(uintptr_t)zs_global(glob_addr(0x6cffe8)), mini_ping_just);
    draw_text(font, surface, mini_score_x, y, (DWORD)(uintptr_t)zs_global(glob_addr(0x6cfff8)), mini_score_just);
    y += font_height(font) + mini_header_spacing;
    void *ranking = fn<void *(__cdecl *)()>(0x566a80)();
    const DWORD n = prop_count(ranking);
    for (DWORD i = 0; i < n; i++) {
        void *entry = fn<void *(THISCALL *)(void *, DWORD)>(0x566e50)(ranking, i);
        if (is_team_game)
            font = font_at(prop_count(entry) == 0 ? mini_blue_font : mini_red_font);
        draw_number(font, surface, mini_rank_x, y, i + 1, mini_rank_just);
        if (fn<BYTE(THISCALL *)(void *)>(0x566570)(entry)) {
            void *flag_font = font_at(prop_count(entry) == 1 ? mini_blue_flag_font : mini_red_flag_font);
            const DWORD flag = prop_count(entry) == 1 ? mini_blue_flag : mini_red_flag;
            draw_text(flag_font, surface, mini_flag_x, y, flag, mini_flag_just);
        }
        void *name = fn<void *(THISCALL *)(void *)>(0x5664b0)(entry);
        draw_text(font, surface, mini_name_x, y, (DWORD)(uintptr_t)zs_c_str(name), mini_name_just);
        draw_number(font, surface, mini_score_x, y, extra_count(entry), mini_score_just);
        if (array_count(entry))
            draw_number(font, surface, mini_alt1_x, y, array_count(entry), mini_alt1_just);
        if (dword14(entry))
            draw_number(font, surface, mini_alt2_x, y, dword14(entry), mini_alt2_just);
        draw_number(font, surface, mini_ping_x, y, dword18(entry), mini_ping_just);
        y += font_height(font) + mini_line_spacing;
    }
    void *df = font_at_inlined(mini_font);
    DWORD x;
    if (mini_rules_just == 1)
        x = vcall<DWORD>(surface, 45) >> 1;
    else if (mini_rules_just == 0)
        x = mini_line_spacing;
    else
        x = vcall<DWORD>(surface, 45) - mini_line_spacing;
    DWORD yy = vcall<DWORD>(surface, 39);
    yy -= font_height(df) + mini_line_spacing;
    if (fn<DWORD(THISCALL *)(void *)>(0x567580)(ranking)) {
        ZString s;
        zs_ctor(&s);
        const char *a = zs_global(glob_addr(0x6cffe4));
        const char *b = zs_global(glob_addr(0x6d000c));
        const DWORD minutes = fn<DWORD(THISCALL *)(void *)>(0x567580)(ranking);
        zs_format(&s, glob<const char *>(0x61c858) /* "%s %d %s" */, a, minutes, b);
        draw_text(df, surface, x, yy, (DWORD)(uintptr_t)zs_c_str(&s), mini_rules_just);
        yy -= font_height(df) + mini_line_spacing;
        zs_dtor(&s);
    }
    if (fn<DWORD(THISCALL *)(void *)>(0x567550)(ranking)) {
        ZString s;
        zs_ctor(&s);
        void *game = fn<void *(THISCALL *)(void *)>(0x59ff80)(app());
        void *what = vcall<void *>(game, 45);
        const char *fmt = zs_global(glob_addr(0x6d0000));
        const char *name = zs_c_str(what);
        zs_format(&s, fmt, name);
        zs_append_format(&s, glob<const char *>(0x60a794) /* " %d" */, fn<DWORD(THISCALL *)(void *)>(0x567550)(ranking));
        draw_text(df, surface, x, yy, (DWORD)(uintptr_t)zs_c_str(&s), mini_rules_just);
        font_height(df);
        zs_dtor(&s);
    }
}

// 0x51d1a0: a player entity leaves: its player number (0x5bacd0) taken off the application's list
// (0x4ef900), the entity's "Died" broadcast, then the application's slot 113 and the player's view
// (0x4f5d60, slot 33). eax: the last call's.
DWORD THISCALL CMultiPlayerGameType::PlayerRemoved(void *entity)
{
    const int i = player_index_of(entity);
    if (i != -1)
        fn<DWORD(THISCALL *)(void *, int, DWORD)>(0x4ef900)(app() + 0x284, i, 0);
    const DWORD died = fn<DWORD(THISCALL *)(void *)>(0x4fbf30)(entity);
    if (i == -1)
        return died;
    const DWORD vt = *reinterpret_cast<DWORD *>(app());
    const DWORD x = fn<DWORD(THISCALL *)(void *)>(0x4f1050)(app());
    at<DWORD(THISCALL *)(void *, DWORD, int, DWORD, DWORD)>(*reinterpret_cast<DWORD *>(vt + 0x1c4))(app(), x, i, 0, 0);
    return vcall<DWORD>(fn<void *(THISCALL *)(void *, int)>(0x4f5d60)(app(), i), 33);
}

// 0x51d240: player i switches teams (+0x14: 0 <-> 1 [assumption: two teams]), +0x18 cleared, its entity
// killed with "Unavoidable" damage of its own size (0x48d030, slot 99), the score kept (0x5ba420), and
// "%s changed teams" shown. eax: the string destructor's.
DWORD THISCALL CMultiPlayerGameType::ChangeTeam(DWORD player)
{
    BYTE *p = player_at(player);
    p[0x18] = 0;
    const int score = *reinterpret_cast<short *>(p + 0xf6);
    *reinterpret_cast<DWORD *>(p + 0x14) = *reinterpret_cast<DWORD *>(p + 0x14) == 0;
    void *e = *array_slot(app() + 0x284, player);
    if (e && obj_is_kind_of(e, entity_class_info()) & 0xff) {
        const DWORD vt = *static_cast<DWORD *>(e);
        const int type = fn<int(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(DAMAGE_TYPE_LIST),
                                                                                 glob<const char *>(0x610324) /* "Unavoidable" */, -1);
        const float amount = fn<float(THISCALL *)(void *)>(0x48d030)(e);
        at<DWORD(THISCALL *)(void *, float, int, void *)>(*reinterpret_cast<DWORD *>(vt + 0x18c))(e, amount, type, e);
    }
    set_score(p, score);
    ZString s;
    zs_ctor(&s);
    void *name = player_name(p);
    const char *fmt = zs_global(glob_addr(0x6d0004));
    zs_format(&s, fmt, zs_c_str(name));
    show_message(zs_c_str(&s));
    return zs_dtor(&s);
}

// 0x51d400: the kill message: "%s killed himself" without another player, "%s killed TEAMMATE %s" in
// the same team (+0x14), else "%s killed %s" (the victim's player first). eax: 0 without a killer, else
// the string destructor's.
DWORD THISCALL CMultiPlayerGameType::KillMessage(void *killer, void *victim)
{
    BYTE *k = player_of(killer);
    if (!k)
        return 0;
    ZString s;
    zs_ctor(&s);
    const char *killer_name = zs_c_str(player_name(player_of(killer)));
    BYTE *v = player_of(victim);
    if (v && v != k) {
        const char *victim_name = zs_c_str(player_name(player_of(victim)));
        if (*reinterpret_cast<DWORD *>(v + 0x14) == *reinterpret_cast<DWORD *>(k + 0x14))
            zs_format(&s, zs_global(glob_addr(0x6d0008)), victim_name, killer_name);
        else
            zs_format(&s, zs_global(glob_addr(0x6d0010)), victim_name, killer_name);
    } else {
        zs_format(&s, zs_global(glob_addr(0x6cfff0)), killer_name);
    }
    show_message(zs_c_str(&s));
    return zs_dtor(&s);
}

// 0x51c800: one more player ready after the match; with half of the application's connections (slot
// 44, 11) ready the state is reset, every connection told (the second reader) and a new match started
// (slot 23)
void THISCALL CMultiPlayerGameType::PlayerReady()
{
    if (!game_data()->match_over)
        return;
    game_data()->players_ready++;
    CMultiPlayerGameData *d = game_data();
    if (d->players_ready < vcall<DWORD>(vcall<void *>(app(), 44), 11) >> 1)
        return;
    game_data()->match_over = 0;
    send_to_all(fn_addr(NEW_MATCH_READER));
    vcall<DWORD>(this, 23);
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(mp_game_type)
{
    using T = CMultiPlayerGameType;
    const Hook list[] = {
        {0x468f20, (void *)&zax_mp_share_spawn_points, "CMultiPlayerGameType::ShareSpawnPoints", 5, {0x8a, 0x41, 0x1d, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x51a040, (void *)&GameData_Create, "CMultiPlayerGameData::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0xc5}},
        {0x51a070, (void *)&GameData_CreateArray, "CMultiPlayerGameData::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x51a0d0, ptr(&CMultiPlayerGameData::DeletingDtor), "CMultiPlayerGameData::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x51a160, (void *)&GameData_RegisterFields, "CMultiPlayerGameData::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x30, 0xe8, 0xd1, 0x61, 0x0b}, 5},
        {0x51a270, (void *)&GameType_ClassInfo, "CMultiPlayerGameType::ClassInfo", 5, {0xa1, 0x20, 0x00, 0x6d, 0x00, 0x56, 0x85, 0xc0}},
        {0x51a2c0, (void *)&GameType_ClassInfo, "CMultiPlayerGameType::ClassInfo2", 5, {0xa1, 0x20, 0x00, 0x6d, 0x00, 0x56, 0x85, 0xc0}},
        {0x51a320, (void *)&GameType_RegisterFields, "CMultiPlayerGameType::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x11, 0x60, 0x0b}, 5},
        {0x51bb90, ptr(&T::AddTeamScore), "CMultiPlayerGameType::AddTeamScore", 6, {0x8b, 0x0d, 0x80, 0x20, 0x6c, 0x00, 0x8b, 0x44}},
        {0x51bfa0, ptr(&T::Ctor), "CMultiPlayerGameType::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xf8, 0x2a, 0xfb, 0xff}, 4},
        {0x51bfc0, ptr(&T::DeletingDtor), "CMultiPlayerGameType::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x60, 0xb1, 0x5f}},
        {0x51bff0, ptr(&T::Dtor), "CMultiPlayerGameType::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x60, 0xb1, 0x5f}},
        {0x51bda0, ptr(&T::UpdateLeads), "CMultiPlayerGameType::UpdateLeads", 7, {0x83, 0xec, 0x14, 0x89, 0x4c, 0x24, 0x00, 0x53}},
        {0x51bef0, ptr(&T::AssignTeam), "CMultiPlayerGameType::AssignTeam", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0xc7, 0x46, 0x14}},
        {0x51c010, ptr(&T::StartMatch), "CMultiPlayerGameType::StartMatch", 5, {0x55, 0x56, 0x57, 0x8b, 0xe9, 0xe8, 0x96, 0x2b}},
        {0x51c1f0, ptr(&T::MatchTime), "CMultiPlayerGameType::MatchTime", 6, {0x8b, 0x0d, 0x14, 0x3f, 0x71, 0x00, 0x56, 0x8b}},
        {0x51c270, ptr(&T::SetMatchTime), "CMultiPlayerGameType::SetMatchTime", 6, {0x8b, 0x0d, 0x80, 0x20, 0x6c, 0x00, 0x8b, 0x01}},
        {0x51c310, ptr(&T::Update), "CMultiPlayerGameType::Update", 5, {0x83, 0xec, 0x08, 0x55, 0x56, 0x8b, 0xf1, 0x57}},
        {0x51c650, ptr(&T::EndMatch), "CMultiPlayerGameType::EndMatch", 6, {0x8b, 0x0d, 0x14, 0x3f, 0x71, 0x00, 0x56, 0x57}},
        {0x51c800, ptr(&T::PlayerReady), "CMultiPlayerGameType::PlayerReady", 9, {0x53, 0x8b, 0xd9, 0x8b, 0x0d, 0x14, 0x3f, 0x71}},
        {0x51cba0, ptr(&T::DrawMiniScoreBoard), "CMultiPlayerGameType::DrawMiniScoreBoard", 5, {0xa1, 0x14, 0x3f, 0x71, 0x00, 0x81, 0xec, 0x14}},
        {0x51d150, (void *)&zax_mp_is_team_game, "CMultiPlayerGameType::IsTeamGame", 5, {0x8a, 0x41, 0x1c, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x51d160, ptr(&T::Slot38), "CMultiPlayerGameType::Slot38", 8, {0x8b, 0x01, 0xff, 0x90, 0x94, 0x00, 0x00, 0x00}},
        {0x51d190, ptr(&T::Slot29), "CMultiPlayerGameType::Slot29", 6, {0x8b, 0x54, 0x24, 0x08, 0x8b, 0x01, 0x52, 0xff}},
        {0x51d1a0, ptr(&T::PlayerRemoved), "CMultiPlayerGameType::PlayerRemoved", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xcf}},
        {0x51d240, ptr(&T::ChangeTeam), "CMultiPlayerGameType::ChangeTeam", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b}},
        {0x51d400, ptr(&T::KillMessage), "CMultiPlayerGameType::KillMessage", 6, {0x53, 0x56, 0x8b, 0x74, 0x24, 0x0c, 0x8b, 0xce}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
