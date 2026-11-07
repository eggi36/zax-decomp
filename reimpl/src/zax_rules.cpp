// The game's custom rules and CZaxWorld's game flow helpers 0x5a01a0-0x5a41b0 (phase 3, game flow):
// CZaxCustomRules (registration, its ten option objects made on first use), CCustomRules' deleting
// destructor, the helpers of the application (CZaxWorld, the global 0x713f14) that actions and AIs call
// (game type, end of game, the level cache, the status text, non-interactive sequences, the HUD's item
// display, the mission times), the net layer's slot 44, three array capacity functions (Core) and the
// three list loaders of the range (WorldRest::LoadQuipList etc. in world_rest.cpp).
//
// CZaxCustomRules (0x30, vtable 0x603028, class 0x713f24; parent CCustomRules: 8 bytes, abstract,
// vtable 0x603084, getter 0x560ac0) [confirmed: Create, RegisterFields, the getters]: +8 "Materials
// Generator Display" (CMaterialsGeneratorDisplayOptions, 0xf4, vtable 0x5f99f0), +0xc "Inventory
// Display" (CInventoryDisplayOptions, 0x2a0, 0x5ee1d0, with three embedded options objects), +0x10
// "Dialog Display" (0x78, 0x5f14f0), +0x14 "Main Menu Display" (0x34, 0x6030e0), +0x18 "Player Walking
// Control Options" (0x40, 0x5fcebc), +0x1c "HUD Options" (0x60, 0x5f9e98), +0x20 "Mission Over Options"
// and +0x24 "MultiPlayer Mission Over Options" (CMissionOverWindowOptions, 0xec, 0x5fa008), +0x28 "Load
// Save Screen Options" (0xc, 0x5fee7c), +0x2c "Sphere Of Zin Options" (0x10, 0x601178); every one an
// owned object field.
//
// CZaxWorld fields used here [confirmed: code]: +0x33c the map caching table
// (CSwappedLayerFilenameMappingTable), +0x34c the game data (+0x2c the playing non-interactive
// sequence), +0x364/+0x368/+0x36c the status text, its seconds and its flag, +0x589 fast forwarding a
// sequence, +0x58c the game type index, +0x590 end the game, +0x594 the longest mission time,
// +0x598/+0x59c/+0x5a0/+0x5a4 the HUD's item (definition's +0x10, index, flag, time), +0x5a8/+0x5ac/
// +0x5b0 the three mission times.
#include "class_info.h"
#include "exe_base.h"
#include "world_rest.h"
#include "x87.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);
static CClassInfo *class_at(FnKey getter) { return fn<Getter>(getter)(); }
static void obj_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x416610)(o); }
static void obj_dtor(void *o) { fn<DWORD(THISCALL *)(void *)>(0x4166d0)(o); }
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static void *object_clone(void *o) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffff); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void *last_field(CClassInfo *c) { return fn<void *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
static DWORD set_help(void *f, GlobKey text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419020)(f, glob<const char *>(text)); }
static void zs_ctor(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
static void zs_dtor(void *z) { fn<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }
static DWORD zs_assign(void *z, const void *from) { return (DWORD)(uintptr_t)fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(z, from); }
static void field_color_ctor(void *c) { fn<void *(THISCALL *)(void *)>(0x53f000)(c); }
static void local_entry_ctor(void *e) { fn<void *(THISCALL *)(void *)>(0x500740)(e); }
static void path_join(char *out, const char *dir, const char *name) { fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, dir, name); }
static char *path_name_mut(char *path) { return fn<char *(__fastcall *)(char *)>(0x4a4570)(path); }

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
static BYTE &by(void *p, DWORD off) { return *(static_cast<BYTE *>(p) + off); }
static BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
static DWORD with_al(DWORD upper, DWORD al) { return (upper & ~0xffu) | (al & 0xff); }

struct DefinitionRecord {
    char name[0x78];
    void *object;
    BYTE keep, in_use, b7e, loading;
};
static DefinitionRecord *&records_of(DWORD list) { return *glob_dyn<DefinitionRecord **>(list + 0x30); }
static DWORD count_of(DWORD list) { return *glob_dyn<DWORD *>(list + 0x34); }
// Record i of a definition list (0 when out of range) loaded inline as the original does it at these
// places; its object (null without records or while it is loaded)
static BYTE *load_inline(DWORD list, DWORD i)
{
    DefinitionRecord *&data = records_of(list);
    if (!data)
        return nullptr;
    if (i >= count_of(list))
        i = 0;
    if (data[i].loading)
        return nullptr;
    if (!data[i].object && data && !data[i].loading) {
        data[i].loading = 1;
        char path[0x100];
        path_join(path, glob_dyn<const char *>(list + 0x38), vcall<const char *>(glob_dyn<void *>(list), 1, i));
        char *p = path_name_mut(path);
        void *o;
        if (p == path)
            o = vcall<void *>(glob_dyn<void *>(list), 37, glob<const char *>(0x62e558), p, &data[i].object);
        else {
            p[-1] = 0;
            o = vcall<void *>(glob_dyn<void *>(list), 37, path, p, &data[i].object);
        }
        data[i].object = o;
        data[i].loading = 0;
    }
    return static_cast<BYTE *>(data[i].object);
}

constexpr DWORD GAME_TYPES = 0x6c02e8;   // the game type list (records 0x6c0318)
constexpr DWORD CATALOGUE = 0x6c0c08;    // the item catalogue

// ---- CZaxCustomRules ------------------------------------------------------------------------------------

constexpr DWORD VT_RULES_BASE = 0x603084, VT_RULES = 0x603028;

static CClassInfo *__cdecl custom_rules_class() { return fn<Getter>(0x560ac0)(); }
// 0x5a1350, 0x5a1520
static CClassInfo *__cdecl ZaxCustomRules_ClassInfo()
{
    CClassInfo *&v = *glob<CClassInfo **>(0x713f24);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(0x6249b0) /* "CZaxCustomRules" */, 1.0f, custom_rules_class(), 0x30, 0, fn_addr(0x5a13d0),
                                     fn_addr(0x5a1410), fn_addr(0x5a1580));
    return v;
}
static void rules_construct(BYTE *o)
{
    obj_ctor(o);
    dw(o, 0) = vt(VT_RULES_BASE);
    obj_init_fields(o);
    dw(o, 0) = vt(VT_RULES);
    obj_init_fields(o);
}
static void rules_destroy(BYTE *o)
{
    dw(o, 0) = vt(VT_RULES);
    obj_free_fields(o);
    dw(o, 0) = vt(VT_RULES_BASE);
    obj_free_fields(o);
    obj_dtor(o);
}
// 0x5a13d0
static void *__cdecl ZaxCustomRules_Create()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x30));
    if (!o)
        return nullptr;
    rules_construct(o);
    return o;
}
// 0x5a1410
static void *__fastcall ZaxCustomRules_CreateArray(int count, int, int)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc((DWORD)count * 0x30 + 4));
    if (!block)
        return nullptr;
    block[0] = (DWORD)count;
    BYTE *e = reinterpret_cast<BYTE *>(block + 1);
    if ((int)((DWORD)count - 1) >= 0)
        for (DWORD i = 0; i < (DWORD)count; i++)
            rules_construct(e + i * 0x30);
    return e;
}
// 0x5a1480
static void *THISCALL ZaxCustomRules_DeletingDtor(BYTE *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                rules_destroy(self + (i - 1) * 0x30);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    rules_destroy(self);
    if (flags & 1)
        pool_free(self, 0x30);
    return self;
}
// 0x5a13a0: CCustomRules' deleting destructor (single object only)
static void *THISCALL CustomRules_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(VT_RULES_BASE);
    obj_free_fields(self);
    obj_dtor(self);
    if (flags & 1)
        pool_free(self, 8);
    return self;
}
// An owned options object field (0x3c, the class's getter called after the allocation)
static void *options_field(GlobKey name, DWORD offset, FnKey getter)
{
    void *f = heap_alloc(0x3c);
    if (f)
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD)>(0x4186f0)(
            f, glob<const char *>(name), offset, 4, offset, class_at(getter), 1, 0, 0, 0);
    return f;
}
// 0x5a1580. eax: the last help's.
static DWORD __cdecl ZaxCustomRules_RegisterFields()
{
    void *f = options_field(0x6250e8 /* "Materials Generator Display" */, 8, 0x501350);
    add_field(ZaxCustomRules_ClassInfo(), f);
    f = options_field(0x6250d4 /* "Inventory Display" */, 0xc, 0x4271e0);
    add_field(ZaxCustomRules_ClassInfo(), f);
    f = options_field(0x610fd8 /* "Dialog Display" */, 0x10, 0x47d350);
    add_field(ZaxCustomRules_ClassInfo(), f);
    f = options_field(0x6250c0 /* "Main Menu Display" */, 0x14, 0x5c0bd0);
    add_field(ZaxCustomRules_ClassInfo(), f);
    f = options_field(0x6250a0 /* "Player Walking Control Options" */, 0x18, 0x542580);
    add_field(ZaxCustomRules_ClassInfo(), f);
    f = options_field(0x625094 /* "HUD Options" */, 0x1c, 0x507500);
    add_field(ZaxCustomRules_ClassInfo(), f);
    set_help(last_field(ZaxCustomRules_ClassInfo()), 0x62506c /* "Options for the HUD Bar and Mini-Map." */);
    f = options_field(0x625054 /* "Mission Over Options" */, 0x20, 0x50a710);
    add_field(ZaxCustomRules_ClassInfo(), f);
    set_help(last_field(ZaxCustomRules_ClassInfo()), 0x62502c /* "Options for the Mission Over window." */);
    f = options_field(0x625008 /* "MultiPlayer Mission Over Options" */, 0x24, 0x50a710);
    add_field(ZaxCustomRules_ClassInfo(), f);
    set_help(last_field(ZaxCustomRules_ClassInfo()), 0x624fc8 /* "Options for the MultiPlayer version of the Mission Over window." */);
    f = options_field(0x624fac /* "Load Save Screen Options" */, 0x28, 0x562650);
    add_field(ZaxCustomRules_ClassInfo(), f);
    set_help(last_field(ZaxCustomRules_ClassInfo()), 0x624f88 /* "Options for the Load Save Screen." */);
    f = options_field(0x624f70 /* "Sphere Of Zin Options" */, 0x2c, 0x582520);
    add_field(ZaxCustomRules_ClassInfo(), f);
    return set_help(last_field(ZaxCustomRules_ClassInfo()), 0x624f40 /* "Options for the way the Sphere Of Zin behaves." */);
}

// The option objects: made on first use (the constructor inlined), then their slot 19 (defaults)
using Build = void (*)(BYTE *);
template <DWORD OFF, DWORD SIZE, Build B> static BYTE *THISCALL options(BYTE *rules)
{
    if (!dw(rules, OFF)) {
        BYTE *o = static_cast<BYTE *>(pool_alloc(SIZE));
        if (o)
            B(o);
        dw(rules, OFF) = (DWORD)(uintptr_t)o;
        vcall<DWORD>(o, 19);
    }
    return pt(rules, OFF);
}
template <DWORD VT> static void plain_options(BYTE *o)
{
    obj_ctor(o);
    dw(o, 0) = vt(VT);
    obj_init_fields(o);
}
template <DWORD... OFFS> static void local_entries(BYTE *o) { (local_entry_ctor(o + OFFS), ...); }
// CMaterialsGeneratorDisplayOptions: two colours
static void materials_options(BYTE *o)
{
    obj_ctor(o);
    field_color_ctor(o + 0x78);
    field_color_ctor(o + 0x88);
    dw(o, 0) = vt(0x5f99f0);
    obj_init_fields(o);
}
// CInventoryDisplayOptions: three embedded options objects (+0x3c two colours, +0xe4 one Local.txt
// entry, +0x144 five), six Local.txt entries of its own
static void inventory_options(BYTE *o)
{
    obj_ctor(o);
    BYTE *a = o + 0x3c;
    obj_ctor(a);
    field_color_ctor(a + 0x88);
    field_color_ctor(a + 0x98);
    dw(a, 0) = vt(0x5ee2e4);
    obj_init_fields(a);
    BYTE *b = o + 0xe4;
    obj_ctor(b);
    local_entry_ctor(b + 0x18);
    dw(b, 0) = vt(0x5ee288);
    obj_init_fields(b);
    BYTE *c = o + 0x144;
    obj_ctor(c);
    local_entries<0x18, 0x38, 0x44, 0x50, 0x90>(c);
    dw(c, 0) = vt(0x5ee22c);
    obj_init_fields(c);
    local_entries<0x260, 0x268, 0x280, 0x288, 0x290, 0x298>(o);
    dw(o, 0) = vt(0x5ee1d0);
    obj_init_fields(o);
}
// CHudBarOptions: three colours
static void hud_options(BYTE *o)
{
    obj_ctor(o);
    field_color_ctor(o + 0x28);
    field_color_ctor(o + 0x38);
    field_color_ctor(o + 0x48);
    dw(o, 0) = vt(0x5f9e98);
    obj_init_fields(o);
}
// CMissionOverWindowOptions: a colour and eight Local.txt entries
static void mission_over_options(BYTE *o)
{
    obj_ctor(o);
    field_color_ctor(o + 0x84);
    local_entries<0xa4, 0xb0, 0xb8, 0xc0, 0xcc, 0xd4, 0xdc, 0xe4>(o);
    dw(o, 0) = vt(0x5fa008);
    obj_init_fields(o);
}

// ---- The application (CZaxWorld, this = 0x713f14) ---------------------------------------------------------

// 0x5a01a0: the game type's index
static DWORD THISCALL App_GameTypeIndex(BYTE *app) { return dw(app, 0x58c); }
// 0x5a01b0: the game type's bit (shl masks the index to 5 bits)
static DWORD THISCALL App_GameTypeBits(BYTE *app) { return 1u << (dw(app, 0x58c) & 31); }
// 0x5a0850: end the game
static void THISCALL App_EndGame(BYTE *app) { by(app, 0x590) = 1; }
// 0x5a0860: flush a level from the map caching table
static DWORD THISCALL App_FlushLevel(BYTE *app, DWORD name) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x589790)(pt(app, 0x33c), name); }
// 0x5a0880: expire a level in the map caching table unless it is expired already
static DWORD THISCALL App_ExpireLevel(BYTE *app, DWORD name)
{
    const DWORD r = fn<DWORD(THISCALL *)(void *, DWORD)>(0x5899b0)(pt(app, 0x33c), name);
    if ((BYTE)r)
        return r;
    return fn<DWORD(THISCALL *)(void *, DWORD)>(0x589570)(pt(app, 0x33c), name);
}
// 0x5a0a00: the status text, its seconds and its flag (shown later by 0x5a0a30). eax: the assign's.
static DWORD THISCALL App_SetStatusText(BYTE *app, void *text, DWORD seconds, BYTE flag)
{
    const DWORD r = zs_assign(app + 0x364, text);
    dw(app, 0x368) = seconds;
    by(app, 0x36c) = flag;
    return r;
}
// 0x5a0c60: the status text copied out
static DWORD THISCALL App_StatusText(BYTE *app, void *out) { return zs_assign(out, app + 0x364); }
// 0x5a2b10: start a non-interactive sequence (a clone of it into the game data's +0x2c, then 0x521e90
// with a2; its eax); while one plays only the warning (application slot 16; al 0)
static DWORD THISCALL App_StartSequence(BYTE *app, void *sequence, DWORD a2)
{
    BYTE *data = pt(app, 0x34c);
    if (dw(data, 0x2c))
        return with_al(vcall<DWORD>(app, 16, glob<const char *>(0x6252b8) /* "Attempting to start NonInteractiveSequence when one is already..." */),
                       0);
    dw(data, 0x2c) = (DWORD)(uintptr_t)object_clone(sequence);
    return fn<DWORD(THISCALL *)(void *, DWORD)>(0x521e90)(pt(pt(app, 0x34c), 0x2c), a2);
}
// 0x5a2b60: pass a1 to the playing sequence (0x522110), else the warning (slot 16)
static DWORD THISCALL App_SequenceEvent(BYTE *app, DWORD a1)
{
    if (BYTE *s = pt(pt(app, 0x34c), 0x2c))
        return fn<DWORD(THISCALL *)(void *, DWORD)>(0x522110)(s, a1);
    return vcall<DWORD>(app, 16, glob<const char *>(0x624ea4) /* "Attempting to end NonInteractiveSequence when there isn't one..." */);
}
// 0x5a2b90: end the playing sequence: fast forwarding stops (+0x589, the counter 0x6c1d0c) when the
// application's slot 84 says so, the sequence deleted. eax: the game data.
static DWORD THISCALL App_EndSequence(BYTE *app, DWORD)
{
    if (dw(pt(app, 0x34c), 0x2c)) {
        if (by(app, 0x589) && vcall<bool>(app, 84)) {
            by(app, 0x589) = 0;
            (*glob<int *>(0x6c1d0c))--;
        }
        if (BYTE *s = pt(pt(app, 0x34c), 0x2c)) {
            vcall<void *>(s, 1, (DWORD)1);
            dw(pt(app, 0x34c), 0x2c) = 0;
        }
    }
    return dw(app, 0x34c);
}
// 0x5a2bf0: has the part a camera tracker AI (slot 64 with CCameraTrakerAI's class). eax 0 or 1.
static DWORD THISCALL App_HasCameraTracker(BYTE *, BYTE *part)
{
    CClassInfo *c = class_at(0x5aa020);
    return vcall<DWORD>(part, 64, c, (DWORD)0) != 0;
}
// 0x5a2c80 (slot 106): the world's readers on a new session's packets, then the multiplayer ones
// (0x5bd8b0 on the session, 0x5abd90 on the second argument). eax: the last call's.
static DWORD THISCALL App_RegisterReaders(BYTE *app, void *session, void *a2)
{
    fn<DWORD(THISCALL *)(void *, void *, void *)>(0x4f6a90)(app, session, a2);
    fn<DWORD(THISCALL *)(void *)>(0x5bd8b0)(session);
    return fn<DWORD(THISCALL *)(void *)>(0x5abd90)(a2);
}
// 0x5a3180: the game type by name (NamedList_FindOrFail; -1 leaves it there), its object loaded and
// its slot 23 (start) called. eax: the slot's (-1 when not found).
static DWORD THISCALL App_SetGameTypeByName(BYTE *app, const char *name)
{
    const DWORD i = fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x591fc0)(glob<void *>(GAME_TYPES), name, 0xffffffff);
    dw(app, 0x58c) = i;
    if (i == 0xffffffff)
        return i;
    return vcall<DWORD>(load_inline(glob_addr(GAME_TYPES), i), 23);
}
// 0x5a32b0: the same by index
static DWORD THISCALL App_SetGameType(BYTE *app, DWORD i)
{
    dw(app, 0x58c) = i;
    if (i == 0xffffffff)
        return i;
    return vcall<DWORD>(load_inline(glob_addr(GAME_TYPES), i), 23);
}
// 0x5a3480: the HUD's item display (0x5bd7b0/0x5bd810 with the player's +0x3c, the definition, the flag)
static DWORD THISCALL App_HudItem(BYTE *, DWORD def, DWORD flag, DWORD player)
{
    void *hud = fn<void *(__cdecl *)()>(0x5bd7b0)();
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x5bd810)(hud, player + 0x3c, def, flag);
}
// 0x429de0 (static): the custom rules' mission timer object (+0x270 the time)
static BYTE *mission_timer() { return fn<BYTE *(__cdecl *)()>(0x429de0)(); }
// 0x5a34b0: the HUD's item: the definition's +0x10, its index, the flag and the time (the longest one
// kept in +0x594)
static void THISCALL App_SetHudItem(BYTE *app, DWORD def, DWORD flag)
{
    dw(app, 0x59c) = def;
    BYTE *d = load_inline(glob_addr(CATALOGUE), def);
    dw(app, 0x598) = dw(d, 0x10);
    dw(app, 0x5a0) = flag;
    const long double t = fld(fl(mission_timer(), 0x270));
    fl(app, 0x5a4) = fstp(t);
    if (!c03(t, fld(fl(app, 0x594))))
        fl(app, 0x594) = fstp(t);
}
// 0x5a3600: the time of mission phase 0, 1 or 2 (the longest one kept in +0x594)
static void THISCALL App_SetMissionTime(BYTE *app, DWORD kind)
{
    const long double t = fld(fl(mission_timer(), 0x270));
    if (kind == 0)
        fl(app, 0x5a8) = fstp(t);
    else if (kind == 1)
        fl(app, 0x5ac) = fstp(t);
    else if (kind == 2)
        fl(app, 0x5b0) = fstp(t);
    if (!c03(t, fld(fl(app, 0x594))))
        fl(app, 0x594) = fstp(t);
}
// 0x5a33e0 (slot 44): the net layer, made once (NetLocal 0x4f7df0 with the derived vtable 0x602fa8)
static void *__cdecl App_NetLayer()
{
    void *&net = *glob<void **>(0x713f18);
    if (!net) {
        void *o = heap_alloc(8);
        if (!o) {
            net = nullptr;
            return nullptr;
        }
        fn<void *(THISCALL *)(void *)>(0x4f7df0)(o);
        dw(o, 0) = vt(0x602fa8);
        net = o;
    }
    return net;
}
// 0x5a3420: its deleting destructor
static void *THISCALL NetLayer_DeletingDtor(void *self, DWORD flags)
{
    fn<DWORD(THISCALL *)(void *)>(0x4f7e30)(self);
    if (flags & 1)
        heap_free(self);
    return self;
}

// ---- Array capacities (Core) -------------------------------------------------------------------------

// 0x5a4080 / 0x5a41b0: a pool array (+4 data, +8 count, +0xc capacity) grown to n elements of 4 or 8
// bytes (memmove of the count's elements; the old block back to the pool)
template <DWORD SIZE> static DWORD THISCALL Array_Reserve(BYTE *a, DWORD n)
{
    const DWORD old = dw(a, 0xc);
    if (old >= n)
        return n;
    dw(a, 0xc) = n;
    void *p = pool_get_alloc(n * SIZE);
    if (dw(a, 8))
        fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(p, pt(a, 4), dw(a, 8) * SIZE);
    DWORD r = dw(a, 4);
    if (r) {
        r = fn<DWORD(THISCALL *)(void *, void *, unsigned)>(0x5065e0)(pool_get(), pt(a, 4), old * SIZE);
        dw(a, 4) = 0;
    }
    dw(a, 4) = (DWORD)(uintptr_t)p;
    return r;
}
// 0x5a40f0: a ZString array (+4 data from operator new with the count in front) grown to n strings,
// the old ones assigned and destroyed from the last. eax: the new data.
static DWORD THISCALL StringArray_Reserve(BYTE *a, DWORD n)
{
    if (dw(a, 0xc) >= n)
        return n;
    dw(a, 0xc) = n;
    BYTE *data = nullptr;
    if (DWORD *block = static_cast<DWORD *>(heap_alloc(n * 4 + 4))) {
        block[0] = n;
        data = reinterpret_cast<BYTE *>(block + 1);
        if ((int)(n - 1) >= 0)
            for (DWORD i = 0; i < n; i++)
                zs_ctor(data + i * 4);
    }
    if (const DWORD count = dw(a, 8))
        for (DWORD i = 0; i < count; i++)
            zs_assign(data + i * 4, pt(a, 4) + i * 4);
    if (BYTE *old = pt(a, 4)) {
        DWORD *block = reinterpret_cast<DWORD *>(old) - 1;
        const DWORD m = *block;
        if ((int)(m - 1) >= 0)
            for (DWORD i = m; i; i--)
                zs_dtor(old + (i - 1) * 4);
        heap_free(block);
    }
    dw(a, 4) = (DWORD)(uintptr_t)data;
    return (DWORD)(uintptr_t)data;
}

// 0x5a4060: a definition list's record cleared (the name, the object, the four flags). eax: this.
static DefinitionRecord *THISCALL DefinitionRecord_Init(DefinitionRecord *r)
{
    r->name[0] = 0;
    r->object = nullptr;
    r->b7e = 0;
    r->in_use = 0;
    r->keep = 0;
    r->loading = 0;
    return r;
}

ZAX_HOOKS(zax_rules)
{
    const Hook list[] = {
#include "zax_rules_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
