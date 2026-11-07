// CZaxSaveGameStats::Fill 0x5bc2a0 (docs/DECOMPILATION.md 10, "Batch C5"; the class in section 8, "The game types
// ..., the save game data"): fills a save game's statistics from the running game. Every call goes through its exe
// address.
//
// [confirmed: code; VM] "Save Date and Time" (+8) strftime "%c" of now (time 0x5d20af, localtime 0x5d312f, strftime
// 0x5d287d; a 0xff-byte buffer, its last byte 0), +0xc the time; +0x10 the current mission (the mission manager of
// the application's slot 97, 0x5127e0) cloned (Object_Clone, the old one deleted); +0x14 the screen shot: the frame
// sized 200 × 150 in the screen's depth and the world's view (+0x2a8) scaled into it after a redraw without the
// status text (the window's slot 26 (0, 0), the application's slot 3 (0), the status text taken (0x5a0c60), cleared
// and put back around the application's slot 4 (0x5a0a00 with +0x368), the window's slot 26 (1, 0)); +0x54 the
// player's hit points left (0x48d030), +0x58 the elapsed time (slot 14), +0x60/+0x64 the counts of "Ore Deposits" and
// "Crystals" (0x4257f0); +0x5c "Map Name": the current layer's +0xc4, trimmed, else (with the reporter's message
// 0x71446c) the layer's file name from its last '/' or '\' up to its first '.'; +0x68 "SaveGame FileName":
// "SaveGames/<name>.sav", <name> the letters of the expanded save name (0x5bc810) and its digits after the first
// character ("QuickSave" when none, at most 56 characters), "SaveGames/<name>_<n>.sav" (n from 2) while that file
// can be opened (0x4a1570, mode 0x20); +0x6c "Item List": emptied (each item deleted from the last), then for every
// item of the player's inventory in category order (0x5b2aa0, unless the first is -1) the item's statistics object
// (slot 31) appended (slot 2) when it gives one.
#include "ui_screens.h"
#include "x87.h"

namespace zax::savestats {

using namespace ui;

inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
inline BYTE *app() { return bp(gd(glob_addr(0x713f14))); }
inline BYTE *first_character() { return fn<BYTE *(THISCALL *)(BYTE *)>(0x4f5d80)(app()); }
inline BYTE *inventory_of(BYTE *part) { return fn<BYTE *(THISCALL *)(BYTE *)>(0x4267e0)(part); }
inline BYTE *current_layer() { return fn<BYTE *(THISCALL *)(BYTE *)>(0x4f1050)(app()); }
inline DWORD zs_length(void *z) { return fn<DWORD(THISCALL *)(void *)>(0x4e13b0)(z); }
inline void zs_set_text(void *z, const char *t) { fn<void(THISCALL *)(void *, const char *)>(0x4df010)(z, t); }
inline void zs_set_string(void *z, const void *from) { fn<void(THISCALL *)(void *, const void *)>(0x4df270)(z, from); }
inline char zs_at(void *z, int i) { return (char)fn<DWORD(THISCALL *)(void *, int)>(0x4e1aa0)(z, i); }

}  // namespace zax::savestats

namespace zax {

using namespace savestats;

namespace {

void set_status(void *z)
{
    BYTE *a = app();
    fn<void(THISCALL *)(BYTE *, void *, DWORD, DWORD)>(0x5a0a00)(a, z, float_bits(fstp(fld(m<float>(a, 0x368)))), 0);   // (fld, fstp)
}

// 0x5bc2a0
void THISCALL SaveGameStats_Fill(BYTE *stats)
{
    DWORD now = fn<DWORD(__cdecl *)(DWORD *)>(0x5d20af)(nullptr);
    BYTE name[4];
    zs_ctor(name);
    BYTE *inv = nullptr;
    if (first_character())
        inv = inventory_of(first_character());
    char date[0x100];
    {
        void *tm = fn<void *(__cdecl *)(DWORD *)>(0x5d312f)(&now);
        fn<DWORD(__cdecl *)(char *, DWORD, const char *, void *)>(0x5d287d)(date, 0xff, glob<const char *>(0x62723c) /* "%c" */, tm);
    }
    date[0xff] = 0;
    zs_set_text(stats + 8, date);
    m(stats, 0xc) = now;
    if (BYTE *old = bp(m(stats, 0x10))) {
        vc(old, 1, 1);
        m(stats, 0x10) = 0;
    }
    {
        BYTE *mm = bp(vc(app(), 97));
        if (BYTE *mission = fn<BYTE *(THISCALL *)(DWORD)>(0x5127e0)(m(mm, 8)))
            m(stats, 0x10) = fn<DWORD(THISCALL *)(BYTE *, DWORD, int)>(0x416760)(mission, 0, -1);
    }
    // the screen shot
    BYTE *shot = stats + 0x14;
    {
        const DWORD depth = fn<DWORD(__cdecl *)()>(0x55d030)();
        fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(shot, 0xc8, 0x96, depth);
    }
    int src[4], dst[4];
    src[0] = 0, src[1] = 0;
    src[2] = (int)vc(bp(m(app(), 0x2a8)), 45);
    src[3] = (int)vc(bp(m(app(), 0x2a8)), 39);
    dst[0] = 0, dst[1] = 0;
    dst[2] = (int)vc(shot, 45);
    dst[3] = (int)vc(shot, 39);
    vc(bp(fn<DWORD(THISCALL *)(BYTE *)>(0x4cdf30)(app())), 26, 0, 0);
    vc(app(), 3, 0);
    BYTE saved_status[4], empty[4];
    zs_ctor(saved_status);
    zs_ctor(empty);
    fn<void(THISCALL *)(BYTE *, void *)>(0x5a0c60)(app(), saved_status);
    set_status(empty);
    vc(app(), 4);
    set_status(saved_status);
    vc(shot, 23, m(app(), 0x2a8), src, dst, glob_addr(0x6b9ca0));
    vc(bp(fn<DWORD(THISCALL *)(BYTE *)>(0x4cdf30)(app())), 26, 1, 0);
    // the values
    m<float>(stats, 0x54) = fstp(fn<long double(THISCALL *)(BYTE *)>(0x48d030)(first_character()));
    m<float>(stats, 0x58) = fstp(vc<long double>(app(), 14));
    {
        const DWORD ore = fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x523df0)(glob_addr(0x6c0c08), glob<const char *>(0x60b7d4) /* "Ore Deposits" */, -1);
        m(stats, 0x60) = fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x4257f0)(inventory_of(first_character()), ore);
        const DWORD crystals = fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x523df0)(glob_addr(0x6c0c08), glob<const char *>(0x60b7c8) /* "Crystals" */, -1);
        m(stats, 0x64) = fn<DWORD(THISCALL *)(BYTE *, DWORD)>(0x4257f0)(inventory_of(first_character()), crystals);
    }
    // the map name
    BYTE *map = stats + 0x5c;
    zs_set_string(map, fn<void *(THISCALL *)(BYTE *)>(0x5007b0)(current_layer() + 0xc0));
    fn<void(THISCALL *)(void *)>(0x4e0f20)(map);
    fn<void(THISCALL *)(void *)>(0x4e10f0)(map);
    if (!zs_length(map)) {
        const char *p = fn<const char *(THISCALL *)(BYTE *)>(0x4eb670)(current_layer());
        void *rep = *glob<void **>(0x6c02cc);
        vc(rep, 17, c_str(glob_addr(0x71446c)));
        int dot = -1;
        if (*p) {
            int i = 0;
            char c = p[0];
            do {
                c = p[i];
                if (c == '/' || c == '\\') {
                    p = p + i + 1;
                    i = -1;
                    dot = -1;
                } else if (c == '.' && dot == -1) {
                    dot = i;
                }
                c = p[i + 1];
                i++;
            } while (c);
        }
        if (dot == -1)
            zs_set_text(map, p);
        else
            fn<void(THISCALL *)(void *, const char *, int)>(0x4df160)(map, p, dot);
    }
    // the file name
    BYTE *file = stats + 0x68;
    {
        BYTE expanded[4];
        zs_set_string(file, fn<void *(THISCALL *)(BYTE *, void *)>(0x5bc810)(stats, expanded));
        zs_dtor(expanded);
    }
    for (int i = 0; i < (int)zs_length(file); i++) {
        bool keep = false;
        if (zs_at(file, i) >= 'A' && zs_at(file, i) <= 'Z')
            keep = true;
        else if (zs_at(file, i) >= 'a' && zs_at(file, i) <= 'z')
            keep = true;
        else if (zs_at(file, i) >= '0' && zs_at(file, i) <= '9' && i > 0)
            keep = true;
        if (keep)
            fn<void(THISCALL *)(void *, DWORD)>(0x4e1d10)(name, (DWORD)(BYTE)zs_at(file, i));
    }
    if (!zs_length(name))
        zs_set_text(name, glob<const char *>(0x627230) /* "QuickSave" */);
    if (zs_length(name) > 0x38) {
        BYTE part[4];
        fn<void *(THISCALL *)(void *, void *, DWORD, DWORD)>(0x4e14c0)(name, part, 0, 0x38);
        fn<void *(THISCALL *)(void *, void *)>(0x4e18f0)(name, part);
        zs_dtor(part);
    }
    zs_format(file, glob<const char *>(0x62721c) /* "SaveGames/%s.sav" */, c_str(name));
    using CanOpen = BYTE(__fastcall *)(const char *, DWORD);
    for (DWORD n = 2; fn<CanOpen>(0x4a1570)(c_str(file), 0x20); n++) {
        const char *s = c_str(name);
        zs_format(file, glob<const char *>(0x627208) /* "SaveGames/%s_%u.sav" */, s, n);
    }
    // the item list: emptied, then the inventory's items' statistics
    BYTE *list = stats + 0x6c;
    for (DWORD count = m(list, 8); count; count = m(list, 8)) {
        const DWORD i = count - 1;
        if (BYTE *item = bp(m(bp(m(list, 4)), i * 4)))
            vc(item, 1, 1);
        const DWORD n = m(list, 8);
        if (i < n) {
            if (i + 1 < n) {
                BYTE *data = bp(m(list, 4));
                fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(data + i * 4, data + (i + 1) * 4, (n - (i + 1)) * 4);
            }
            m(list, 8)--;
        }
    }
    if (inv) {
        const DWORD items = m(inv, 0x20);
        using Order = DWORD(__fastcall *)(BYTE *, DWORD);
        if (fn<Order>(0x5b2aa0)(fn<BYTE *(__cdecl *)()>(0x428b10)(), 0) != 0xffffffffu) {
            for (DWORD k = 0; k < items; k++) {
                const DWORD idx = fn<Order>(0x5b2aa0)(fn<BYTE *(__cdecl *)()>(0x428b10)(), k);
                BYTE *item = fn<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x424f60)(inv, idx);
                DWORD stat = vc(item, 31, inv);
                if (stat)
                    vc(list, 2, &stat);
            }
        }
    }
    zs_dtor(empty);
    zs_dtor(saved_status);
    zs_dtor(name);
}

}  // namespace

ZAX_HOOKS(save_stats_fill_c5)
{
    const Hook list[] = {
#include "save_stats_fill_c5_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
