// Batch C4 (phase 5a), second part: the chat commands of CZaxWorld (CObject_OnMessage handlers), the game
// type list's exit function, the world's slots 8 and 105, CInventory's slot 11, the dialogue script
// list's "file of an object" (C2 leftover 0x41e460) and the Ogg sound reader's deleting destructor (C2
// leftover 0x525820).
//
// Chat commands [confirmed: code; the cheat routines of EXE_FINDINGS.md]: 0x5a2250 (+0x358 = 1,
// +0x35c = 0), 0x5a2270 (the first character takes its hit points as "Unavoidable" damage by itself),
// 0x5a22b0 / 0x5a2540 / 0x5a2630 / 0x5a2670 / 0x5a26a0 (GiveItemsToPlayers with a null-terminated list
// of item names on the stack; 0x5a2540 a hundred times), 0x5a2500 (every item of the catalogue
// 0x6c0c08 given once), 0x5a2590 (outside the inventory screen states 1 and 2: the cursor to the
// middle of the screen frame, the HUD slid away and the inventory screen opened; then a materials
// window of 0x140 bytes made, MatWindow's constructor reached by a tail jump).
//
// Return values are what the original leaves in eax (the exe functions called through their addresses).
#include "def_list_slots.h"
#include "exe_base.h"
#include "init_list.h"
#include "x87.h"

namespace zax {
namespace {

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
DWORD as_dw(const void *p) { return (DWORD)(uintptr_t)p; }
BYTE *app() { return *glob<BYTE **>(0x713f14); }
DWORD give_items(const DWORD *names) { return fn<DWORD(__fastcall *)(const DWORD *)>(0x5a26f0)(names); }

// ---- Chat commands ------------------------------------------------------------------------------------------

DWORD __cdecl Command_5a2250()
{
    BYTE *a = app();
    a[0x358] = 1;
    dw(a, 0x35c) = 0;
    return as_dw(a);
}
// 0x5a2270: the first character (used without a null test) damages itself by its hit points left
DWORD __cdecl Command_Suicide()
{
    BYTE *c = fn<BYTE *(THISCALL *)(void *)>(0x4f5d80)(app());
    const DWORD type = fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x523df0)(glob<void *>(0x6bd760), glob<const char *>(0x610324), 0xffffffff);
    const float hp = fstp(fn<long double(THISCALL *)(void *)>(0x48d030)(c));
    return vcall<DWORD>(c, 99, hp, type, c);
}
const DWORD ALL_ITEMS[] = {
    0x624c84, 0x6251fc, 0x625248, 0x625248, 0x625248, 0x624bd4, 0x624bbc, 0x624c34, 0x624bac, 0x624c08, 0x625240, 0x625240, 0x625240,
    0x6251b0, 0x625228, 0x625228, 0x6251a0, 0x60b7c8, 0x60b7c8, 0x60b7c8, 0x624c5c, 0x625190, 0x624c20, 0x625184, 0x625174, 0x625168,
    0x62515c, 0x625148, 0x624c94, 0x62521c, 0x62521c, 0x62520c, 0x62520c, 0x624be4, 0x6251f0, 0x6251f0, 0x6251f0, 0x6251f0, 0x6251f0,
    0x6251f0, 0x6251f0, 0x6251f0, 0x6251f0, 0x624ca4, 0x60b7d4, 0x60b7d4, 0x6251e0, 0x6251e0, 0x6251e0, 0x6251e0, 0x624b98, 0x625134,
    0x625124, 0x624c70, 0x625114, 0x624bf8, 0x6251d0, 0x6251d0, 0x6251d0, 0x6251d0, 0x6251d0, 0x6251d0, 0x6251d0, 0x625104, 0x6251c0,
    0x6251c0, 0x6251c0, 0x6251c0, 0x6251c0, 0x624c84, 0};
static_assert(sizeof ALL_ITEMS / 4 == 71);
// 0x5a22b0: "loadup" / "load up" (the list built on the stack, as the original does)
DWORD __cdecl Command_LoadUp()
{
    DWORD names[71];
    memcpy(names, ALL_ITEMS, sizeof names);
    for (DWORD &n : names)   // the table holds exe addresses (the same in the DLL)
        n = n ? glob_addr_dyn(n) : 0;
    return give_items(names);
}
// 0x5a2500: every item of the catalogue 0x6c0c08 (its count +0x14) given one by one
DWORD __cdecl Command_GiveAll()
{
    DWORD names[2] = {glob_addr(0x623890), 0};
    const DWORD n = *glob<DWORD *>(0x6c0c1c);
    DWORD r = n;
    for (DWORD i = 0; i < n; i++) {
        names[0] = fn<DWORD(THISCALL *)(void *, DWORD)>(0x591d00)(glob<void *>(0x6c0c08), i);
        r = give_items(names);
    }
    return r;
}
// 0x5a2540: four ore deposits and two crystals, a hundred times
DWORD __cdecl Command_GiveOre()
{
    DWORD names[7] = {glob_addr(0x60b7d4), glob_addr(0x60b7d4), glob_addr(0x60b7d4), glob_addr(0x60b7d4), glob_addr(0x60b7c8), glob_addr(0x60b7c8), 0};
    DWORD r = 0;
    for (int i = 0; i < 100; i++)
        r = give_items(names);
    return r;
}
// 0x5a2630 / 0x5a2670 / 0x5a26a0
DWORD __cdecl Command_GiveDisruptor()
{
    DWORD names[6] = {glob_addr(0x624c84), glob_addr(0x6251fc), glob_addr(0x6251d0), glob_addr(0x6251d0), glob_addr(0x6251d0), 0};
    return give_items(names);
}
DWORD __cdecl Command_GiveBatteries()
{
    DWORD names[5] = {glob_addr(0x6251d0), glob_addr(0x6251d0), glob_addr(0x6251d0), glob_addr(0x6251d0), 0};
    return give_items(names);
}
DWORD __cdecl Command_GiveMines()
{
    DWORD names[11] = {glob_addr(0x6251e0), glob_addr(0x6251e0), glob_addr(0x6251e0), glob_addr(0x6251e0), glob_addr(0x6251e0), glob_addr(0x6251e0), glob_addr(0x6251e0), glob_addr(0x6251e0), glob_addr(0x6251e0), glob_addr(0x6251e0), 0};
    return give_items(names);
}
// 0x5a2590: the inventory screen and a materials window
DWORD __cdecl Command_Materials()
{
    BYTE *a = app();
    const DWORD state = dw(pt(a, 0x340), 0x100);
    if (state != 2 && state != 1) {
        fn<DWORD(THISCALL *)(void *, void *, void *)>(0x5184e0)(glob<void *>(0x6cff40), a + 0x278, a + 0x27c);
        const DWORD w = vcall<DWORD>(pt(a, 0x2a8), 39);
        const DWORD h = vcall<DWORD>(pt(a, 0x2a8), 45);
        fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x518570)(glob<void *>(0x6cff40), h >> 1, w >> 1);
        fn<DWORD(THISCALL *)(void *)>(0x508570)(pt(a, 0x344));
        fn<DWORD(THISCALL *)(void *)>(0x4294f0)(pt(a, 0x340));
    }
    void *m = pool_alloc(0x140);
    if (!m)
        return 0;
    return fn<DWORD(THISCALL *)(void *)>(0x504050)(m);
}

// ---- Lists, the world's slots, the inventory ---------------------------------------------------------------

// 0x4ce6e0: atexit of the game type list 0x6c02e8 (base vtable 0x5f7510)
DWORD __cdecl GameTypeList_AtExit() { return deflist::destroy_list(glob<ZDefinitionList *>(0x6c02e8), 0x5f7510); }

// 0x4f12f0, the world's slot 105 (out): a frame of 0x40 bytes at *out, "Interface/Menu Background" loaded
// into it through a filter list (3, 6, 7, then 0xb for 8-bit screens or 0xc); a failed load deletes it
DWORD THISCALL World_LoadMenuBackground(void *, void **out)
{
    void *f = pool_alloc(0x40);
    if (f)
        f = fn<void *(THISCALL *)(void *)>(0x4a8a90)(f);
    *out = f;
    BYTE filter[0x14];
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4b1cc0)(filter, 0);
    for (DWORD k : {3u, 6u, 7u})
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x4b1db0)(filter, k);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4b1db0)(filter, dw(*glob<void **>(0x6c02cc), 0x24) == 8 ? 0xb : 0xc);
    const DWORD ok = fn<DWORD(THISCALL *)(void *, const char *, const char *, void *, DWORD, DWORD)>(0x4a9a30)(
        *out, glob<const char *>(0x60a630), glob<const char *>(0x6181a0), filter, 1, 0);
    if (!(BYTE)ok && *out) {
        vcall<DWORD>(*out, 1, 1);
        *out = nullptr;
    }
    return fn<DWORD(THISCALL *)(void *)>(0x4b1d20)(filter);
}

// 0x4f2430, the world's slot 8 (the screen size changed): the view frame +0x2a4 sized to the view
// rectangle (+0x2e4..+0x2f0) times the local session's zoom (+0xd8), the HUD frame +0x2a8 to the screen
// frame +4, the view rectangle reset, the scale factors 0x618034/0x618038 (0x5eff98 / height and
// 0x5f9170 / height, the second at least 1.0), the zoom set again
DWORD THISCALL World_ScreenSizeChanged(BYTE *self)
{
    BYTE *session = vcall<BYTE *>(vcall<void *>(self, 44), 23, 0);
    const DWORD w = dw(self, 0x2f0) - dw(self, 0x2e8);
    const DWORD h = dw(self, 0x2ec) - dw(self, 0x2e4);
    const DWORD zoom = dw(session, 0xd8);
    const DWORD depth = fn<DWORD(__cdecl *)()>(0x55d030)();
    const int a = x87_ftol(fild64((long long)w) * fld(raw_float(zoom)));
    const int b = x87_ftol(fild64((long long)h) * fld(raw_float(zoom)));
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(pt(self, 0x2a4), (DWORD)b, (DWORD)a, depth);
    BYTE *screen = pt(self, 4);
    const DWORD depth2 = fn<DWORD(__cdecl *)()>(0x55d030)();
    const DWORD sw = vcall<DWORD>(screen, 39);
    const DWORD sh = vcall<DWORD>(screen, 45);
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(pt(self, 0x2a8), sh, sw, depth2);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4f6980)(self, 0);
    const DWORD h1 = vcall<DWORD>(pt(self, 4), 45);
    *glob<float *>(0x618034) = fstp(cst(glob_addr(0x5eff98)) / fild64((long long)h1));
    const DWORD h2 = vcall<DWORD>(pt(self, 4), 45);
    *glob<float *>(0x618038) = fstp(cst(glob_addr(0x5f9170)) / fild64((long long)h2));
    if (!c03(cst(glob_addr(0x5ec3f4)), fld(*glob<float *>(0x618038))))   // 1.0 above the factor
        *glob<DWORD *>(0x618038) = 0x3f800000;
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4f6770)(self, zoom, 0);
}

// 0x424da0, CInventory's slot 11 (name, value, stream): the legacy block "Inventory Holders" of objects
// (each read into a temporary object of the class 0x5ee008 and handed to slot 2 of the list +0x18 by its
// +8) up to "}"; any other line goes to CObject_ReadOwnLine
DWORD THISCALL Inventory_ReadOwnLine(BYTE *self, const char *name, const char *value, void *file)
{
    if (crt_stricmp(name, glob<const char *>(0x60b754)) != 0)
        return fn<DWORD(THISCALL *)(void *, const char *, const char *, void *)>(0x417500)(self, name, value, file);
    char line[0x100];
    fn<DWORD(THISCALL *)(void *, char *, DWORD)>(0x4a20a0)(file, line, 0xff);
    fn<char *(__fastcall *)(char *)>(0x587ad0)(line);
    fn<DWORD(THISCALL *)(void *, char *, DWORD)>(0x4a20a0)(file, line, 0xff);
    char *p = fn<char *(__fastcall *)(char *)>(0x587ad0)(line);
    if (*p == '}')
        return as_dw(p);
    BYTE *list = self + 0x18;
    DWORD r;
    do {
        BYTE tmp[0xc];
        fn<DWORD(THISCALL *)(void *)>(0x416610)(tmp);
        dw(tmp, 0) = vt(0x5ee008);
        fn<DWORD(THISCALL *)(void *)>(0x4173c0)(tmp);
        dw(tmp, 8) = 0;
        fn<DWORD(THISCALL *)(void *, void *)>(0x416af0)(tmp, file);
        vcall<DWORD>(list, 2, tmp + 8);
        dw(tmp, 8) = 0;
        fn<DWORD(THISCALL *)(void *, char *, DWORD)>(0x4a20a0)(file, line, 0xff);
        p = fn<char *(__fastcall *)(char *)>(0x587ad0)(line);
        dw(tmp, 0) = vt(0x5ee008);
        if (void *o = at<void *>(dw(tmp, 8))) {   // never: +8 was just cleared
            vcall<DWORD>(o, 1, 1);
            dw(tmp, 8) = 0;
        }
        fn<DWORD(THISCALL *)(void *)>(0x4173f0)(tmp);
        r = fn<DWORD(THISCALL *)(void *)>(0x4166d0)(tmp);
    } while (*p != '}');
    return r;
}

// ---- C2 leftovers ---------------------------------------------------------------------------------------------

// 0x41e460 (thiscall object, slot of 0x5ed458): the path of the object's record in the dialogue script list
// ("<folder>/<name>.RDS" into the function-local static ZString 0x62e7d4, guard bit 0 of 0x62e7d8, exit
// 0x41e540); null when the object is no loaded record. eax: the static's text.
DWORD THISCALL DialogScript_FileOfObject(void *self)
{
    const DWORD n = *glob<DWORD *>(0x62e814);
    DWORD i = 0;
    for (;; i++) {
        if (i >= n)
            return 0;
        if (dw(*glob<BYTE **>(0x62e810) + i * 0x80, 0x78) == as_dw(self))
            break;
    }
    if (i == 0xffffffff)
        return 0;
    BYTE &guard = *glob<BYTE *>(0x62e7d8);
    if (!(guard & 1)) {
        guard |= 1;
        fn<DWORD(THISCALL *)(void *)>(0x4dec90)(glob<void *>(0x62e7d4));
        fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x41e540));
    }
    ZString ext;
    deflist::zs_ctor_text(&ext, deflist::path_extension(glob<const char *>(0x60b0b0)));
    if (deflist::zs_is_empty(&ext))
        fn<DWORD(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(&ext, glob<const char *>(0x60b0ac), glob<const char *>(0x60b0b0));
    const char *e = deflist::zs_c_str(&ext);
    void *list = glob<void *>(0x62e7e0);
    const char *name = vcall<const char *>(list, 1, i);
    deflist::zs_set_path3(glob<ZString *>(0x62e7d4), vcall<const char *>(list, 5), name, e);
    deflist::zs_dtor(&ext);
    return (DWORD)(uintptr_t)deflist::zs_c_str(glob<ZString *>(0x62e7d4));
}

// Index of p in the Ogg state array 0x7160e0 (data +4, count +8), the count when it is not there
DWORD ogg_index(DWORD p)
{
    const DWORD n = *glob<DWORD *>(0x7160e8);
    DWORD i = 0;
    while (i < n && at<DWORD *>(*glob<DWORD *>(0x7160e4))[i] != p)
        i++;
    return i;
}
// 0x525820, the Ogg sound reader's deleting destructor (0x20 bytes, vtable 0x5fba24; +0x18 its
// OggVorbis_File from the shared pool, +0x1c open): the file cleared; the pool's arrays made on first use
// (guard bit 0 of 0x7160c0: the states 0x7160e0 and their free flags 0x7160f0, exit 0x525650); once per run
// (0x6d0164) two states are taken from the pool and given back (a free one or a new block of 0x2c0, its
// flag cleared; the second state taken is flagged free again); the reader's own state flagged free; then
// CSoundReader's destructor and the pool block
BYTE *THISCALL OggReader_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(0x5fba24);
    if (self[0x1c]) {
        fn<DWORD(__fastcall *)(void *)>(0x5c3bf0)(pt(self, 0x18));
        self[0x1c] = 0;
    }
    BYTE &guard = *glob<BYTE *>(0x7160c0);
    if (!(guard & 1)) {
        guard |= 1;
        fn<DWORD(THISCALL *)(void *)>(0x525740)(glob<void *>(0x7160e0));
        fn<DWORD(THISCALL *)(void *)>(0x525760)(glob<void *>(0x7160f0));
        fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x525650));
    }
    if (!*glob<BYTE *>(0x6d0164)) {
        *glob<BYTE *>(0x6d0164) = 1;
        DWORD i = 0;
        const DWORD n = *glob<DWORD *>(0x7160e8);
        while (i < n && !at<BYTE *>(*glob<DWORD *>(0x7160f4))[i])
            i++;
        if (i < n)
            at<BYTE *>(*glob<DWORD *>(0x7160f4))[i] = 0;
        else {
            DWORD block = (DWORD)(uintptr_t)heap_alloc(0x2c0);
            vcall<DWORD>(glob<void *>(0x7160e0), 2, &block);
            BYTE zero = 0;
            vcall<DWORD>(glob<void *>(0x7160f0), 2, &zero);
        }
        const DWORD state = at<DWORD *>(*glob<DWORD *>(0x7160e4))[i];
        const DWORD first = fn<DWORD(THISCALL *)(void *)>(0x5255e0)(glob<void *>(0x7160e0));
        const DWORD second = fn<DWORD(THISCALL *)(void *)>(0x5255e0)(glob<void *>(0x7160e0));
        const DWORD k = ogg_index(second);
        if (k < *glob<DWORD *>(0x7160e8))
            at<BYTE *>(*glob<DWORD *>(0x7160f4))[k] = 1;
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x57cdb0)(glob<void *>(0x7160e0), first);
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x57cdb0)(glob<void *>(0x7160e0), state);
    }
    const DWORD k = ogg_index(dw(self, 0x18));
    if (k < *glob<DWORD *>(0x7160e8))
        at<BYTE *>(*glob<DWORD *>(0x7160f4))[k] = 1;
    fn<DWORD(THISCALL *)(void *)>(0x57d110)(self);
    if (flags & 1)
        pool_free(self, 0x20);
    return self;
}

}  // namespace

ZAX_HOOKS(c4_rest)
{
    const Hook list[] = {
#include "c4_rest_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
