// Batch C4 (phase 5a), second part: CZaxWorld's large slots (the application g_App 0x713f14, vtable
// 0x602dc8; zax_rules.cpp, world_rest.cpp, app_flow.cpp have its other functions).
//
// CZaxWorld fields used here [confirmed: code]: +4 the screen frame, +0x2a8 the HUD frame, +0x2b0 the layer
// holder (+0x2bc layers, +0x2c0 their count), +0x310 the game has started, +0x324/+0x328 the playing time,
// +0x32c a save in progress, +0x33c the map caching table (CSwappedLayerFilenameMappingTable: +0x18/+0x19
// two flags, three arrays at +0x24/+0x34/+0x44), +0x340 the inventory screen, +0x344 the HUD slide, +0x34c
// the game data (+0x28 the difficulty, +0x2c the playing non-interactive sequence), +0x354 the other map
// list, +0x364/+0x368/+0x36c the status text, +0x58c the game type index (records of the game type list
// 0x6c02e8).
//
// Return values are what the original leaves in eax (the exe functions called through their addresses);
// where it is the caller's eax the reimplementation returns 0 (the comments say where).
#include "exe_base.h"
#include "file_search.h"
#include "init_list.h"
#include "x87.h"

namespace zax {
namespace {

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
DWORD as_dw(const void *p) { return (DWORD)(uintptr_t)p; }
DWORD with_al(DWORD upper, DWORD al) { return (upper & ~0xffu) | (al & 0xff); }
DWORD crt_delete(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }
DWORD is_kind_of(void *o, void *c) { return fn<DWORD(THISCALL *)(void *, void *)>(0x416790)(o, c); }
void *class_of(FnKey getter) { return fn<void *(__cdecl *)()>(getter)(); }
BYTE *app() { return *glob<BYTE **>(0x713f14); }

// ---- Small and middle slots ----------------------------------------------------------------------------------

// 0x59b9e0, slot 81 (a): when the game has started: the voice stopped, the non-interactive sequence deleted,
// the map caching table emptied unless it is busy (+0x18/+0x19: Clear, its three arrays freed), the other
// map list cleared, World::EndGame (a), the HUD slide reset. Without a started game the caller's eax (0).
DWORD THISCALL App_EndGame(BYTE *self, DWORD a)
{
    if (!self[0x310])
        return 0;
    fn<DWORD(THISCALL *)(void *)>(0x595af0)(fn<void *(__cdecl *)()>(0x595b50)());
    if (void *seq = pt(pt(self, 0x34c), 0x2c)) {
        vcall<DWORD>(seq, 1, 1);
        dw(pt(self, 0x34c), 0x2c) = 0;
    }
    BYTE *t = pt(self, 0x33c);
    if (!t[0x18] && !t[0x19]) {
        fn<DWORD(THISCALL *)(void *)>(0x58a030)(t);
        for (DWORD off : {0x24u, 0x34u, 0x44u}) {
            if (dw(t, off)) {
                crt_delete(pt(t, off));
                dw(t, off) = 0;
                dw(t, off + 8) = 0;
            }
            dw(t, off + 4) = 0;
        }
        t[0x18] = 0;
        t[0x19] = 0;
    }
    fn<DWORD(THISCALL *)(void *)>(0x41f660)(pt(self, 0x354));
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4f5150)(self, a);
    return fn<DWORD(THISCALL *)(void *)>(0x507f20)(pt(self, 0x344));
}

// 0x59be20, slot 39 (part, player): the part gets a CPlayerWalkingControlAI (0x120 bytes; its slot 19, the
// player number) and a CCameraTrakerAI (0x20 bytes, or null). The AI is used without a null test.
DWORD THISCALL App_AddPlayerAIs(void *, void *part, DWORD player)
{
    void *ai = pool_alloc(0x120);
    if (ai)
        ai = fn<void *(THISCALL *)(void *)>(0x543190)(ai);
    vcall<DWORD>(ai, 19);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x542550)(ai, player);
    fn<DWORD(THISCALL *)(void *, void *)>(0x4fbc50)(part, ai);
    void *cam = pool_alloc(0x20);
    if (cam)
        cam = fn<void *(THISCALL *)(void *)>(0x5aa4d0)(cam);
    return fn<DWORD(THISCALL *)(void *, void *)>(0x4fbc50)(part, cam);
}

// 0x5a08b0, slot 68 (name, mode, out): the name with ".zax" (Path_SetExtension) looked up in the map caching
// table: mode 0 its level path, 1 its save path, 2 its load path; another mode: al 0 (eax: mode - 2)
DWORD THISCALL App_MapPath(BYTE *self, const char *name, DWORD mode, DWORD out)
{
    char path[0x100];
    DWORD n = 0;
    while (name[n])
        n++;
    memcpy(path, name, n + 1);
    fn<DWORD(__fastcall *)(char *, const char *)>(0x4a3fb0)(path, glob<const char *>(0x61857c));
    using Fn = DWORD(THISCALL *)(void *, char *, DWORD);
    switch (mode) {
    case 0: return fn<Fn>(0x589270)(pt(self, 0x33c), path, out);
    case 1: return fn<Fn>(0x5893c0)(pt(self, 0x33c), path, out);
    case 2: return fn<Fn>(0x5892b0)(pt(self, 0x33c), path, out);
    }
    return (mode - 2) & ~0xffu;
}

// 0x5a0990, slot 82 (part, a, track, b): World::PlaceAt, then with track a part with a CCameraTrakerAI (slot
// 64) becomes the view target (its position, its player index)
DWORD THISCALL App_PlaceAt(BYTE *self, BYTE *part, DWORD a, DWORD track, DWORD b)
{
    DWORD r = fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x4f5360)(self, part, a, track, b);
    if (!track)
        return r;
    r = vcall<DWORD>(part, 64, class_of(0x5aa020), 0);
    if (!r)
        return r;
    DWORD pos[2];
    fn<DWORD(THISCALL *)(void *, DWORD *)>(0x4fb0a0)(part, pos);
    const DWORD player = fn<DWORD(THISCALL *)(void *)>(0x5bacd0)(part);
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x4f5dd0)(self, pos[0], pos[1], 0, player);
}

// 0x5a0090, slot 58: the difficulty object of the game data's +0x28 from the difficulty list 0x6c2a00,
// loaded inline (0 without records or while it is loaded)
DWORD THISCALL App_Difficulty(BYTE *self)
{
    DWORD i = dw(pt(self, 0x34c), 0x28);
    BYTE *records = *glob<BYTE **>(0x6c2a30);
    if (!records)
        return 0;
    if (i >= *glob<DWORD *>(0x6c2a34))
        i = 0;
    if (records[i * 0x80 + 0x7f])
        return 0;
    if (!dw(records + i * 0x80, 0x78) && records && !records[i * 0x80 + 0x7f]) {
        records[i * 0x80 + 0x7f] = 1;
        void *list = glob<void *>(0x6c2a00);
        char path[0x100];
        fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, glob<const char *>(0x6c2a38), vcall<const char *>(list, 1, i));
        char *name = fn<char *(__fastcall *)(char *)>(0x4a4570)(path);
        DWORD o;
        if (name == path)
            o = vcall<DWORD>(list, 37, glob<const char *>(0x62e558), name, *glob<BYTE **>(0x6c2a30) + i * 0x80 + 0x78);
        else {
            name[-1] = 0;
            o = vcall<DWORD>(list, 37, path, name, *glob<BYTE **>(0x6c2a30) + i * 0x80 + 0x78);
        }
        dw(*glob<BYTE **>(0x6c2a30) + i * 0x80, 0x78) = o;
        (*glob<BYTE **>(0x6c2a30))[i * 0x80 + 0x7f] = 0;
        records = *glob<BYTE **>(0x6c2a30);
    }
    return dw(records + i * 0x80, 0x78);
}

// The game type object of the index (the game type list 0x6c02e8, records 0x6c0318, count 0x6c031c),
// loaded through DefinitionList_LoadRecord; on the way out eax as the original leaves it
struct Found {
    BYTE *object;
    DWORD eax;
};
Found record_object(DWORD list, DWORD i)
{
    BYTE *records = *glob_dyn<BYTE **>(list + 0x30);
    if (!records)
        return {nullptr, 0};
    if (i >= *glob_dyn<DWORD *>(list + 0x34))
        i = 0;
    if (records[i * 0x80 + 0x7f])
        return {nullptr, as_dw(records)};
    if (!dw(records + i * 0x80, 0x78)) {
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x55e090)(glob_dyn<void *>(list), i);
        records = *glob_dyn<BYTE **>(list + 0x30);
    }
    return {pt(records + i * 0x80, 0x78), as_dw(records)};
}
Found game_type(DWORD i) { return record_object(glob_addr(0x6c02e8), i); }
// The quest game type's stat number k when it is of the class (getter), as the three slots below look it up.
// eax on the way out when there is none.
Found quest_stat(DWORD index, DWORD k, DWORD stat_getter)
{
    if (index == 0xffffffff)
        return {nullptr, 0};
    Found g = game_type(index);
    if (!g.object)
        return g;
    DWORD r = is_kind_of(g.object, class_of(0x551c20));
    if (!(BYTE)r)
        return {nullptr, r};
    BYTE *stat = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x552940)(g.object, k);
    if (!stat)
        return {nullptr, 0};
    r = is_kind_of(stat, fn_dyn<void *(__cdecl *)()>(stat_getter)());
    if (!(BYTE)r)
        return {nullptr, r};
    return {stat, r};
}
BYTE *quest_data() { return pt(fn<BYTE *(__cdecl *)()>(0x5525b0)(), 0xc); }

// 0x5a2f20, slot 92 (victim, a, killer): the first character killed by a destructable entity counts in the
// quest game type's stat 15 (a percentage: the quest data's +0x3c, weight 1.0). eax: the first character
// when the victim is not it, g_App when there is no game type (index -1).
DWORD THISCALL App_CountDeath(void *, void *victim, DWORD, void *killer)
{
    void *c = fn<void *(THISCALL *)(void *)>(0x4f5d80)(*glob<void **>(0x6c2080));
    if (victim != c)
        return as_dw(c);
    const DWORD index = dw(app(), 0x58c);
    if (index == 0xffffffff)
        return as_dw(app());
    Found s = quest_stat(index, 0xf, fn_addr(0x559e10));
    if (!s.object)
        return s.eax;
    const DWORD r = is_kind_of(killer, class_of(0x48bdf0));
    if (!(BYTE)r)
        return r;
    const DWORD value = dw(quest_data(), 0x3c);
    return fn<DWORD(THISCALL *)(void *, DWORD, float)>(0x55aad0)(s.object, value, 1.0f);
}
// 0x5a2ff0 / 0x5a30a0, slots 93/94 (amount): the quest game type's stat 14 / 13 (a float accumulator) adds
// the amount under the quest data's +0x38 / +0x34. Without a game type the caller's eax (0).
template <DWORD STAT, DWORD OFF> DWORD THISCALL App_QuestAdd(BYTE *self, DWORD amount)
{
    Found s = quest_stat(dw(self, 0x58c), STAT, fn_addr(0x556940));
    if (!s.object)
        return s.eax;
    const DWORD key = dw(quest_data(), OFF);
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x5575b0)(s.object, key, amount);
}

// 0x59e3e0, slot 76 (a, stream, b, c): a layer stream that starts with 0x3e9 is packed (the packed size, the
// unpacked size, the data: Unpack into a new memory stream read through an object stream of its own);
// any other is read as it is (the position put back). eax: World::LoadLayerStream's, or al = its result.
DWORD THISCALL App_LoadLayerStream(BYTE *self, DWORD a, BYTE *stream, DWORD b, DWORD c)
{
    using Load = DWORD(THISCALL *)(void *, DWORD, void *, DWORD, DWORD);
    const DWORD pos = fn<DWORD(THISCALL *)(void *)>(0x4a2180)(stream);
    DWORD magic;
    fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(stream, &magic, 4);
    if (magic != 0x3e9) {
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a20e0)(stream, pos);
        return fn<Load>(0x4f4500)(self, a, stream, b, c);
    }
    BYTE ident[4];
    fn<DWORD(THISCALL *)(void *)>(0x5664b0)(ident);
    BYTE *data = nullptr;
    DWORD packed, size;
    fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(stream, &packed, 4);
    fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(stream, &size, 4);
    const BYTE *src = fn<const BYTE *(THISCALL *)(void *, DWORD)>(0x4a2150)(stream, packed);
    fn<DWORD(THISCALL *)(void *, BYTE **, DWORD, const BYTE *, DWORD)>(0x5c3ba0)(ident, &data, size, src, packed);
    void *m = pool_alloc(0x20);
    if (m)
        m = fn<void *(THISCALL *)(void *, BYTE *, DWORD)>(0x505fb0)(m, data, size);
    BYTE os[0x24];
    fn<DWORD(THISCALL *)(void *)>(0x5240b0)(os);
    const DWORD flag = dw(pt(stream, 0x1c), 0x18) != 0;
    BYTE ok = 0;
    if ((BYTE)fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x5242f0)(os, m, 4, flag))
        ok = (BYTE)fn<Load>(0x4f4500)(self, a, os, b, c);
    fn<DWORD(THISCALL *)(void *)>(0x5241e0)(os);
    return with_al(fn<DWORD(THISCALL *)(void *)>(0x58ea40)(ident), ok);
}

// 0x5a0c80, slot 109 (path): a save game. Refused (al 0, "Player Error.wav") while slot 25 says so;
// otherwise +0x32c set: every layer other than the current one whose file the caching table has not expired
// is flushed, the table updated, the object stream opened on the file (0x123, 1); on success, unless the
// table is busy, the playing time added to the layer holder and cleared, the status text "game saved"
// (0x713ed4, 1.5 s) drawn and the HUD frame blitted to the screen and flipped as often as the screen has
// buffers (+0x44 + 1), then slot 111 (stream, 0, 1) writes it; al 1. +0x32c put back.
DWORD THISCALL App_SaveGame(BYTE *self, DWORD path)
{
    const BYTE was = self[0x32c];
    if ((BYTE)vcall<DWORD>(self, 25)) {
        return with_al(fn<DWORD(THISCALL *)(void *, const char *)>(0x57e820)(glob<void *>(0x713708), glob<const char *>(0x60a4d8)), 0);
    }
    self[0x32c] = 1;
    BYTE ok = 0;
    for (DWORD i = 0; i < dw(self, 0x2c0); i++) {
        BYTE *layer = pt(pt(self, 0x2bc), i * 4);
        if (!layer)
            continue;
        if (layer == fn<BYTE *(THISCALL *)(void *)>(0x4f1050)(self))
            continue;
        const char *file = fn<const char *(THISCALL *)(void *)>(0x4eb670)(layer);
        if ((BYTE)fn<DWORD(THISCALL *)(void *, const char *)>(0x5899b0)(pt(self, 0x33c), file))
            continue;
        file = fn<const char *(THISCALL *)(void *)>(0x4eb670)(pt(pt(self, 0x2bc), i * 4));
        fn<DWORD(THISCALL *)(void *, const char *)>(0x589790)(pt(self, 0x33c), file);
    }
    fn<DWORD(THISCALL *)(void *)>(0x589db0)(pt(self, 0x33c));
    BYTE os[0x24];
    fn<DWORD(THISCALL *)(void *)>(0x5240b0)(os);
    if ((BYTE)fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x5242b0)(os, path, 0x123, 1)) {
        if (!pt(self, 0x33c)[0x18]) {
            fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4efe90)(self + 0x2b0, dw(self, 0x324), dw(self, 0x328));
            dw(self, 0x324) = 0;
            fn<DWORD(THISCALL *)(void *, const void *)>(0x4e18f0)(self + 0x364, glob<void *>(0x713ed4));
            dw(self, 0x328) = 0;
            dw(self, 0x368) = 0x3fc00000;
            self[0x36c] = 0;
            fn<DWORD(THISCALL *)(void *)>(0x5a0a30)(self);
            DWORD n = dw(pt(self, 4), 0x44) + 1;
            if (n)
                do {
                    fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x4b3280)(pt(self, 4), pt(self, 0x2a8), 0, 0, glob_addr(0x6bb4e0));
                    fn<DWORD(THISCALL *)(void *)>(0x5693a0)(pt(self, 4));
                } while (--n);
        }
        vcall<DWORD>(self, 111, os, 0, 1);
        ok = 1;
    }
    self[0x32c] = was;
    return with_al(fn<DWORD(THISCALL *)(void *)>(0x5241e0)(os), ok);
}


// 0x59bba0, slot 32 (player): the player's character made from the game type's model (+0x14 of the game
// type object; the object is used without a null test) as a CEntityWalking, its +0x24 the player's +0x14;
// when its first child (or itself) has the dynamic property 0x6c0520, the property's value gets the
// player's start position (+0x1c: +4, +8 as unsigned integers made floats), first passed through the game
// type's slot 39 (player, {x, value, y}) when there is one. eax: the character.
DWORD THISCALL App_MakePlayerCharacter(BYTE *self, DWORD player)
{
    BYTE *pl = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x51f440)(vcall<void *>(app(), 1), player);
    const DWORD index = dw(app(), 0x58c);
    BYTE *gt = index == 0xffffffff ? nullptr : fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(0x6c02e8), index);
    BYTE *e = fn<BYTE *(THISCALL *)(void *, DWORD, void *, DWORD, DWORD, DWORD)>(0x5176f0)(glob<void *>(0x6cfdd8), dw(gt, 0x14),
                                                                                       class_of(0x49b810), 0, 0, 0xffffffff);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4ce500)(e, dw(pl, 0x14));
    BYTE *o = e;
    if (e && fn<DWORD(THISCALL *)(void *)>(0x4fc7c0)(e))
        o = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x4fc7d0)(e, 0);
    void *desc = *glob<void **>(0x6c0520);
    if (!(BYTE)fn<DWORD(THISCALL *)(void *, void *)>(0x418fc0)(desc, o))
        return as_dw(e);
    DWORD v[3];
    v[1] = fn<DWORD(THISCALL *)(void *, void *)>(0x418790)(*glob<void **>(0x6c0520), o);
    BYTE *start = pt(pl, 0x1c);
    v[0] = float_bits(fstp(fild64((long long)dw(start, 4))));
    v[2] = float_bits(fstp(fild64((long long)dw(start, 8))));
    if (dw(self, 0x58c) != 0xffffffff && game_type(dw(self, 0x58c)).object) {
        BYTE *g = dw(self, 0x58c) == 0xffffffff ? nullptr : game_type(dw(self, 0x58c)).object;
        vcall<DWORD>(g, 39, pl, v);
    }
    BYTE *value = reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v[1]));
    *reinterpret_cast<float *>(value + 0xc) = fstp(fld(raw_float(v[0])));
    dw(value, 0x18) = v[2];
    return as_dw(e);
}

// 0x59e520, slot 75 (layer, path, flags, text): a layer saved to a file between the slots 69 and 70 (layer,
// +0x314[, 0]): as text through an object stream of 0x24 bytes (CLayer::WriteText), or packed: written into
// a memory stream through an object stream on the stack, packed (Pack), then 0x3e9, the packed size, the
// unpacked size and the data into a file buffer of 0x1c bytes. The stream or file buffer is deleted through
// its slot 0. al: the write's result; eax: slot 70's upper bytes.
DWORD THISCALL App_SaveLayer(BYTE *self, void *layer, const char *path, DWORD flags, DWORD text)
{
    vcall<DWORD>(self, 69, layer, self + 0x314);
    BYTE ok;
    BYTE *out;
    if ((BYTE)text) {
        out = static_cast<BYTE *>(pool_alloc(0x24));
        if (out)
            out = fn<BYTE *(THISCALL *)(void *)>(0x5240b0)(out);
        ok = (BYTE)fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x5242b0)(out, path, 3, flags);
    } else {
        out = static_cast<BYTE *>(pool_alloc(0x1c));
        if (out)
            out = fn<BYTE *(THISCALL *)(void *)>(0x4a1780)(out);
        ok = (BYTE)fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x4a1840)(out, path, 3);
    }
    if (ok) {
        ok = 0;
        if ((BYTE)text)
            ok = (BYTE)fn<DWORD(THISCALL *)(void *, void *)>(0x4ebe10)(layer, out);
        else {
            BYTE os[0x24];
            fn<DWORD(THISCALL *)(void *)>(0x5240b0)(os);
            BYTE *ms = static_cast<BYTE *>(pool_alloc(0x20));
            if (ms)
                ms = fn<BYTE *(THISCALL *)(void *)>(0x505f00)(ms);
            if ((BYTE)fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x5242f0)(os, ms, 3, flags)) {
                ok = (BYTE)fn<DWORD(THISCALL *)(void *, void *)>(0x4ebe10)(layer, os);
                fn<DWORD(THISCALL *)(void *)>(0x524320)(os);
                BYTE ident[4];
                fn<DWORD(THISCALL *)(void *)>(0x5664b0)(ident);
                BYTE *data = nullptr;
                const BYTE *src = pt(ms, 0x18);
                const DWORD size = vcall<DWORD>(ms, 7);
                const DWORD packed = fn<DWORD(THISCALL *)(void *, BYTE **, const BYTE *, DWORD)>(0x5c3b40)(ident, &data, src, size);
                fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a21c0)(out, 0x3e9);
                fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a21c0)(out, packed);
                fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a21c0)(out, vcall<DWORD>(ms, 7));
                fn<DWORD(THISCALL *)(void *, const void *, DWORD)>(0x4a19d0)(out, data, packed);
                if (data) {
                    crt_delete(data);
                    data = nullptr;
                }
                fn<DWORD(THISCALL *)(void *)>(0x58ea40)(ident);
            }
            fn<DWORD(THISCALL *)(void *)>(0x5241e0)(os);
        }
    }
    if (out)
        vcall<DWORD>(out, 0, 1);
    return with_al(vcall<DWORD>(self, 70, layer, self + 0x314, 0), ok);
}


// Record i of a definition list loaded inline the way these slots do it (records +0x30, count +0x34, folder
// +0x38; an index beyond the count is 0): null without records or while the record is being loaded,
// otherwise its object (null when loading made none)
BYTE *list_object(DWORD list, DWORD i)
{
    BYTE *records = *glob_dyn<BYTE **>(list + 0x30);
    if (!records)
        return nullptr;
    if (i >= *glob_dyn<DWORD *>(list + 0x34))
        i = 0;
    if (records[i * 0x80 + 0x7f])
        return nullptr;
    if (!dw(records + i * 0x80, 0x78) && records && !records[i * 0x80 + 0x7f]) {
        records[i * 0x80 + 0x7f] = 1;
        void *l = glob_dyn<void *>(list);
        char path[0x100];
        fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, glob_dyn<const char *>(list + 0x38), vcall<const char *>(l, 1, i));
        char *name = fn<char *(__fastcall *)(char *)>(0x4a4570)(path);
        DWORD o;
        if (name == path)
            o = vcall<DWORD>(l, 37, glob<const char *>(0x62e558), name, *glob_dyn<BYTE **>(list + 0x30) + i * 0x80 + 0x78);
        else {
            name[-1] = 0;
            o = vcall<DWORD>(l, 37, path, name, *glob_dyn<BYTE **>(list + 0x30) + i * 0x80 + 0x78);
        }
        dw(*glob_dyn<BYTE **>(list + 0x30) + i * 0x80, 0x78) = o;
        (*glob_dyn<BYTE **>(list + 0x30))[i * 0x80 + 0x7f] = 0;
        records = *glob_dyn<BYTE **>(list + 0x30);
    }
    return pt(records + i * 0x80, 0x78);
}

// 0x5a0e70, slot 110 (path): a saved game loaded. Refused like the save (slot 25); a file that cannot be
// opened (File_CanOpen, 0x20) gives al 0. Otherwise slot 81 (0), the debug log section "Load from Saved
// games", g_LastFileOpened kept, the object stream opened (0x224, 0); slot 74 (stream, 0) reads the world,
// every layer's slot 7, and when the layer holder's unique id changed the playing time is cleared;
// g_LastFileOpened put back. Success: slot 80. Failure: the net game's slot 10, slot 81 (1), the
// application window's modal windows closed (slot 73 asks, slot 5 closes; +0xec the first, +0xf4 the next,
// +0xf0 the last), a new main menu in front (6) and the message box 0x713e54 / 0x713f10. al: success.
DWORD THISCALL App_LoadGame(BYTE *self, const char *path)
{
    const BYTE was = self[0x32c];
    if ((BYTE)vcall<DWORD>(self, 25))
        return with_al(fn<DWORD(THISCALL *)(void *, const char *)>(0x57e820)(glob<void *>(0x713708), glob<const char *>(0x60a4d8)), 0);
    self[0x32c] = 1;
    BYTE ok = 0;
    const DWORD can = fn<DWORD(__fastcall *)(const char *, DWORD)>(0x4a1570)(path, 0x20);
    if (!(BYTE)can) {
        self[0x32c] = was;
        return with_al(can, 0);
    }
    vcall<DWORD>(self, 81, 0);
    fn<DWORD(__fastcall *)(const char *)>(0x4a2650)(glob<const char *>(0x624e74));
    ZString saved;
    fn<ZString *(THISCALL *)(ZString *, const void *)>(0x4dedf0)(&saved, glob<void *>(0x713c14));
    BYTE os[0x24];
    fn<DWORD(THISCALL *)(void *)>(0x5240b0)(os);
    if ((BYTE)fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x5242b0)(os, path, 0x224, 0)) {
        const DWORD id = fn<DWORD(THISCALL *)(void *)>(0x4efe20)(self + 0x2b0);
        const BYTE r = (BYTE)vcall<DWORD>(self, 74, os, 0);
        for (DWORD i = 0; i < dw(self, 0x2c0); i++)
            if (void *layer = pt(pt(self, 0x2bc), i * 4))
                vcall<DWORD>(layer, 7);
        if (id != fn<DWORD(THISCALL *)(void *)>(0x4efe20)(self + 0x2b0)) {
            dw(self, 0x324) = 0;
            dw(self, 0x328) = 0;
        }
        ok = r;
    }
    fn<DWORD(THISCALL *)(void *, const ZString *)>(0x4e18f0)(glob<void *>(0x713c14), &saved);
    using Window = BYTE *(THISCALL *)(void *);
    if (ok)
        vcall<DWORD>(self, 80);
    else {
        vcall<DWORD>(vcall<void *>(self, 1), 10);
        vcall<DWORD>(self, 81, 1);
        if (fn<Window>(0x4cdf30)(*glob<void **>(0x6c02cc))) {
            BYTE *w = pt(fn<Window>(0x4cdf30)(*glob<void **>(0x6c02cc)), 0xec);
            while (w) {
                if ((BYTE)vcall<DWORD>(w, 73)) {
                    vcall<DWORD>(w, 5);
                    w = pt(fn<Window>(0x4cdf30)(*glob<void **>(0x6c02cc)), 0xec);
                } else {
                    if (as_dw(w) == dw(fn<Window>(0x4cdf30)(*glob<void **>(0x6c02cc)), 0xf0))
                        break;
                    w = pt(w, 0xf4);
                }
            }
            void *m = pool_alloc(0x158);
            if (m)
                m = fn<void *(THISCALL *)(void *, DWORD)>(0x5be4a0)(m, 0);
            fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x40c680)(fn<Window>(0x4cdf30)(self), m, 6);
            const char *a = fn<const char *(THISCALL *)(void *)>(0x4e13a0)(glob<void *>(0x713e54));
            const char *b = fn<const char *(THISCALL *)(void *)>(0x4e13a0)(glob<void *>(0x713f10));
            fn<DWORD(THISCALL *)(void *, const char *, const char *)>(0x40c740)(fn<Window>(0x4cdf30)(self), a, b);
        }
    }
    fn<DWORD(__cdecl *)()>(0x4a2670)();
    fn<DWORD(THISCALL *)(void *)>(0x5241e0)(os);
    const DWORD r = fn<DWORD(THISCALL *)(void *)>(0x4defd0)(&saved);
    self[0x32c] = was;
    return with_al(r, ok);
}

// 0x5a2cc0, slot 91 (victim, killer): a character killing another part tells the victim (slot 63 with the
// message {0x5eeb64, 0x713eb4, 0}); then the game type object (loaded inline twice, as the original does)
// gets slot 25 (killer, victim). eax: slot 25's; on the way out without a game type what the calls before
// left (not reproduced: 0).
DWORD THISCALL App_Killed(BYTE *self, BYTE *victim, BYTE *killer)
{
    if (victim && victim != killer && killer && (BYTE)is_kind_of(killer, class_of(0x49a930))) {
        DWORD msg[3] = {vt(0x5eeb64), *glob<DWORD *>(0x713eb4), 0};
        vcall<DWORD>(victim, 63, msg);
    }
    if (dw(self, 0x58c) == 0xffffffff || !list_object(glob_addr(0x6c02e8), dw(self, 0x58c)))
        return 0;
    BYTE *g = dw(self, 0x58c) == 0xffffffff ? nullptr : list_object(glob_addr(0x6c02e8), dw(self, 0x58c));
    return vcall<DWORD>(g, 25, killer, victim);
}


// 0x59ab00, slot 111 (stream, binary, packed): the world written for a save game. Unless the caching table
// is busy (+0x18): the quest game type's stat 5 (a counter) counts the save (the quest data's +0x14, 1), the
// table flushed and updated, the save game stats (+0x350) filled and written with the table. Then the world
// state (World::WriteState), the game data (+0x34c) and the other map list (+0x354), binary or as text,
// either directly into the stream or (packed) into a memory stream of 1,000,000 bytes that is packed and
// written as 0x3e9, the packed size, the size and the data. Unless the table is busy the error reporter's
// slot 27 (1, 1) and the flagged resource lists. +0x591 holds the packed flag meanwhile.
DWORD THISCALL App_WriteWorld(BYTE *self, BYTE *stream, DWORD binary, DWORD packed)
{
    using Write = DWORD(THISCALL *)(void *, void *);
    const DWORD write_fn = (BYTE)binary ? fn_addr(0x416f50) : fn_addr(0x4168f0);
    self[0x591] = (BYTE)packed;
    if (!pt(self, 0x33c)[0x18]) {
        Found s = quest_stat(dw(app(), 0x58c), 5, 0x554ae0);
        if (s.object)
            fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x555930)(s.object, dw(quest_data(), 0x14), 1);
        fn<DWORD(THISCALL *)(void *)>(0x589890)(pt(self, 0x33c));
        fn<DWORD(THISCALL *)(void *)>(0x589db0)(pt(self, 0x33c));
        fn<DWORD(THISCALL *)(void *)>(0x5bc2a0)(pt(self, 0x350));
        fn_dyn<Write>(write_fn)(pt(self, 0x350), stream);
        fn_dyn<Write>(write_fn)(pt(self, 0x33c), stream);
    }
    DWORD r;
    if ((BYTE)packed) {
        BYTE os[0x24];
        fn<DWORD(THISCALL *)(void *)>(0x5240b0)(os);
        BYTE *ms = static_cast<BYTE *>(pool_alloc(0x20));
        if (ms)
            ms = fn<BYTE *(THISCALL *)(void *)>(0x505f00)(ms);
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x5061e0)(ms, 1000000);
        if ((BYTE)fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x5242f0)(os, ms, 3, 0)) {
            fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4f3b40)(self, os, binary);
            fn_dyn<Write>(write_fn)(pt(self, 0x34c), os);
            fn_dyn<Write>(write_fn)(pt(self, 0x354), os);
            fn<DWORD(THISCALL *)(void *)>(0x524320)(os);
            BYTE ident[4];
            fn<DWORD(THISCALL *)(void *)>(0x5664b0)(ident);
            BYTE *data = nullptr;
            const BYTE *src = pt(ms, 0x18);
            const DWORD size = vcall<DWORD>(ms, 7);
            const DWORD n = fn<DWORD(THISCALL *)(void *, BYTE **, const BYTE *, DWORD)>(0x5c3b40)(ident, &data, src, size);
            fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a21c0)(stream, 0x3e9);
            fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a21c0)(stream, n);
            fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a21c0)(stream, vcall<DWORD>(ms, 7));
            fn<DWORD(THISCALL *)(void *, const void *, DWORD)>(0x4a19d0)(stream, data, n);
            if (data) {
                crt_delete(data);
                data = nullptr;
            }
            fn<DWORD(THISCALL *)(void *)>(0x58ea40)(ident);
        }
        r = fn<DWORD(THISCALL *)(void *)>(0x5241e0)(os);
    } else {
        fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4f3b40)(self, stream, binary);
        fn_dyn<Write>(write_fn)(pt(self, 0x34c), stream);
        r = fn_dyn<Write>(write_fn)(pt(self, 0x354), stream);
    }
    BYTE *table = pt(self, 0x33c);
    if (table[0x18])
        r = with_al(r, table[0x18]);
    else {
        vcall<DWORD>(*glob<void **>(0x6c02cc), 27, 1, 1);
        r = fn<DWORD(THISCALL *)(void *)>(0x4a3760)(fn<void *(__cdecl *)()>(0x4a35d0)());
    }
    self[0x591] = 0;
    return r;
}


// Entry i of a definition list marked used and loaded the way slot 27 does it inline: +0x7d = 1,
// DefinitionList_Object; on the first use (+0x7e) also +0x7e = 1 and the object's slot 7 (precache) when
// `precache`. Nothing without records. eax: the last call's.
DWORD use_entry(DWORD list, DWORD i, bool precache)
{
    BYTE *records = *glob_dyn<BYTE **>(list + 0x30);
    if (!records)
        return 0;
    records[i * 0x80 + 0x7d] = 1;
    using Obj = BYTE *(THISCALL *)(void *, DWORD);
    fn<Obj>(0x48d8f0)(glob_dyn<void *>(list), i);
    records = *glob_dyn<BYTE **>(list + 0x30);
    if (records[i * 0x80 + 0x7e])
        return as_dw(records);
    records[i * 0x80 + 0x7e] = 1;
    BYTE *o = fn<Obj>(0x48d8f0)(glob_dyn<void *>(list), i);
    if (!precache)
        return as_dw(o);
    return vcall<DWORD>(o, 7);
}
DWORD find_name(DWORD list, DWORD name) { return fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x523df0)(glob_dyn<void *>(list), glob_dyn<const char *>(name), 0xffffffff); }

// 0x59e0c0, slot 27 (a, mode): World::ReloadResources (a, mode), then the resources the game keeps used and
// precached: the dialogue script "Inventory Item Text"; with a nonzero a the game data's slot 7 and the game
// type (index +0x58c below the list's +0x14); with a = 2 every difficulty; unless a started game (+0x310)
// with +0x311 (and a not 2): the interface frames "Background", "Fire Left", "Fire Right", "Charms", "Skull"
// of the list 0x713380 and "UI/GameSpy Logo" through its Use
DWORD THISCALL App_PrecacheResources(BYTE *self, DWORD a, DWORD mode)
{
    DWORD r = fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4f4190)(self, a, mode);
    const DWORD text = find_name(glob_addr(0x62e7e0), glob_addr(0x616164));
    r = use_entry(glob_addr(0x62e7e0), text, false);
    if (void *data = pt(self, 0x34c)) {
        r = a;
        if (a)
            r = vcall<DWORD>(data, 7);
    }
    const DWORD gt = dw(self, 0x58c);
    r = *glob<DWORD *>(0x6c02fc);   // eax: the count, a, the records as the tests load them
    if (gt < r) {
        r = a;
        if (a)
            r = use_entry(glob_addr(0x6c02e8), gt, true);
    }
    if (a == 2)
        for (DWORD i = 0; i < *glob<DWORD *>(0x6c2a14); i++) {
            use_entry(glob_addr(0x6c2a00), i, true);
            r = *glob<DWORD *>(0x6c2a30);   // the loop reloads the records after every entry
        }
    if (self[0x310] && self[0x311] && a != 2)
        return with_al(r, self[0x311]);   // the test loaded the byte into al
    for (DWORD name : {0x624d14u, 0x624b80u, 0x624b8cu, 0x624d0cu, 0x624d04u})
        r = use_entry(glob_addr(0x713380), find_name(glob_addr(0x713380), name), true);
    return fn<DWORD(THISCALL *)(void *, DWORD)>(0x478940)(glob<void *>(0x713380), find_name(glob_addr(0x713380), glob_addr(0x61be08)));
}


// A pool array on the stack as slot 112 keeps them: {vtable, data, count, capacity}
struct StackArray {
    DWORD vftable, data, count, capacity;
};
// Room for one more element: the capacity grows to the count + 1 or capacity * 0x5ea5f4 (__ftol), whichever
// is bigger, through the array's reserve function
void grow(StackArray &a, FnKey reserve)
{
    DWORD want = a.count + 1;
    if (a.capacity >= want)
        return;
    const DWORD t = (DWORD)x87_ftol(fild64((long long)a.capacity) * cst(glob_addr(0x5ea5f4)));
    if (t > want)
        want = t;
    fn<DWORD(THISCALL *)(void *, DWORD)>(reserve)(&a, want);
}

// 0x59b400, slot 112 (relocation {point, name, start, keep, mode}): the switch to another map. The current
// layer is announced to slot 69 (or +0x314's slot 1 without one); the target layer is found or loaded
// (BeginLoading, LoadLayer (2), AddLayer, FinishLevelStart, the caching table's Unexpire, EndLoading); one
// point, layer and start name are collected in three pool arrays on the stack; with +0x10 the new layer is
// forgotten by the caching table and the current layer updated in steps of 1/30 s for a second; every
// character of +0x318 (CEntityCharacter or null) gets slot 41 (0, 0), slot 104 (character, layer i) and its
// slot 62 (layer i) - the layer array has one entry, the original reads past it for more characters; the
// layer kept gets slot 78 (layer, -1); the mode +0x14 2/3/4 touches, flushes or expires the current layer;
// the other map list runs for the layer; slot 83 (+0x314, names, points, layers); the arrays freed. al 1.
DWORD THISCALL App_SwitchMap(BYTE *self, BYTE *reloc)
{
    StackArray points = {vt(0x602f9c), 0, 0, 0}, layers = {vt(0x5f7b88), 0, 0, 0}, names = {vt(0x5fa6a4), 0, 0, 0};
    BYTE *kept = nullptr;
    BYTE *cur = fn<BYTE *(THISCALL *)(void *)>(0x4f1050)(self);
    if (cur)
        vcall<DWORD>(self, 69, cur, self + 0x314);
    else
        vcall<DWORD>(self + 0x314, 1);
    grow(points, 0x5a41b0);
    fn<DWORD(THISCALL *)(DWORD, void *)>(0x493ef0)(points.data + points.count * 8, reloc);
    points.count++;
    void *name = reloc + 8;
    using CStr = const char *(THISCALL *)(void *);
    BYTE *layer = fn<BYTE *(THISCALL *)(void *, const char *)>(0x4f4d70)(self, fn<CStr>(0x4e13a0)(name));
    if (!layer) {
        fn<DWORD(THISCALL *)(void *)>(0x4f3cc0)(self);
        layer = fn<BYTE *(THISCALL *)(void *, const char *, DWORD)>(0x4f45b0)(self, fn<CStr>(0x4e13a0)(name), 2);
        fn<DWORD(THISCALL *)(void *, void *)>(0x4f4650)(self, layer);
        fn<DWORD(THISCALL *)(void *, void *)>(0x4f4890)(self, layer);
        fn<DWORD(THISCALL *)(void *, const char *)>(0x589b80)(pt(self, 0x33c), fn<CStr>(0x4eb670)(layer));
        fn<DWORD(THISCALL *)(void *)>(0x4f3d30)(self);
    }
    grow(layers, 0x5a4080);
    at<DWORD *>(layers.data)[layers.count++] = as_dw(layer);
    grow(names, 0x5a40f0);
    fn<DWORD(THISCALL *)(DWORD, const void *)>(0x4e18f0)(names.data + names.count * 4, reloc + 0xc);
    names.count++;
    if (reloc[0x10]) {
        kept = layer;
        fn<DWORD(THISCALL *)(void *, const char *)>(0x589af0)(pt(self, 0x33c), fn<CStr>(0x4eb670)(layer));
        float t = 0;
        long double s;
        do {
            fn<DWORD(THISCALL *)(void *, DWORD)>(0x4ea220)(cur, 0x3d088889);
            s = fld(t) + cst(glob_addr(0x5f88cc));
            t = fstp(s);
        } while (c0(s, cst(glob_addr(0x5ec3f4))));
    }
    for (DWORD i = 0; i < dw(self, 0x31c); i++) {
        BYTE *c = pt(pt(self, 0x318), i * 4);
        if (c && !(BYTE)is_kind_of(c, class_of(0x49a930)))
            c = nullptr;
        vcall<DWORD>(c, 41, 0, 0);
        vcall<DWORD>(self, 104, c, at<DWORD *>(layers.data)[i]);
        vcall<DWORD>(c, 62, at<DWORD *>(layers.data)[i]);
    }
    if (kept)
        vcall<DWORD>(self, 78, kept, 0xffffffff);
    using ByName = DWORD(THISCALL *)(void *, const char *);
    switch (dw(reloc, 0x14)) {
    case 2: fn<ByName>(0x589a50)(pt(self, 0x33c), fn<CStr>(0x4eb670)(cur)); break;
    case 3: fn<ByName>(0x589790)(pt(self, 0x33c), fn<CStr>(0x4eb670)(cur)); break;
    case 4: fn<ByName>(0x589570)(pt(self, 0x33c), fn<CStr>(0x4eb670)(cur)); break;
    }
    const char *n1 = fn<CStr>(0x4e13a0)(name);
    const char *n2 = fn<CStr>(0x4e13a0)(name);
    BYTE *target = fn<BYTE *(THISCALL *)(void *, const char *)>(0x4f4d70)(self, n1);
    fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x41f6c0)(pt(self, 0x354), target, n2);
    vcall<DWORD>(self, 83, self + 0x314, &names, &points, &layers);
    if (names.data) {
        DWORD *block = at<DWORD *>(names.data) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                fn<DWORD(THISCALL *)(DWORD)>(0x4defd0)(names.data + (i - 1) * 4);
        crt_delete(block);
    }
    DWORD r = 0;
    if (layers.data)
        r = fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x5065e0)(pool_get(), layers.data, layers.capacity * 4);
    if (points.data)
        r = fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x5065e0)(pool_get(), points.data, points.capacity * 8);
    return with_al(r, 1);
}


// 0x59db80, slot 95 (frame): the frame's HUD part: every character's CPlayerControlAI gets slot 42 (frame);
// World's own slot 95 (0x4f27b0); in a started game with +0x311 the inventory screen's item records of every
// character (ItemRecords (character, HUD frame, inventory, 0)), the status text "pause" (0x713e78, 1.0 s)
// when +0x360 is set and the net game's slot 13 is false, and with a game type its slot 42 (frame, the
// spacer's height on an 800-pixel high frame, the room above the inventory screen less the "HUD Main"
// frame's slots 39 and 44); the status text drawn when there is one; the quip list's used quips loaded
// (frame); the net game's slot 5 (frame).
DWORD THISCALL App_FrameHud(BYTE *self, void *frame)
{
    BYTE *list = self + 0x284;
    using Count = DWORD(THISCALL *)(void *);
    using Slot = BYTE **(THISCALL *)(void *, DWORD);
    for (DWORD i = 0; i < fn<Count>(0x4ef8f0)(list); i++)
        if (BYTE *c = *fn<Slot>(0x4ef8e0)(list, i))
            if (void *ai = vcall<void *>(c, 64, class_of(0x542170), 0))
                vcall<DWORD>(ai, 42, frame);
    fn<DWORD(THISCALL *)(void *, void *)>(0x4f27b0)(self, frame);
    if (self[0x310] && self[0x311]) {
        for (DWORD i = 0; i < fn<Count>(0x4ef8f0)(list); i++)
            if (BYTE *c = *fn<Slot>(0x4ef8e0)(list, i)) {
                void *hud = pt(self, 0x2a8);
                void *inv = fn<void *(THISCALL *)(void *)>(0x4267e0)(c);
                fn<DWORD(THISCALL *)(void *, void *, void *, void *, DWORD)>(0x429ae0)(pt(self, 0x340), c, hud, inv, 0);
            }
        if (self[0x360] && !(BYTE)vcall<DWORD>(vcall<void *>(self, 1), 13)) {
            fn<DWORD(THISCALL *)(void *, const void *)>(0x4e18f0)(self + 0x364, glob<void *>(0x713e78));
            dw(self, 0x368) = 0x3f800000;
            self[0x36c] = 0;
        }
        if (dw(self, 0x58c) != 0xffffffff) {
            void *hudlist = glob<void *>(0x6c0540);
            BYTE *main = record_object(glob_addr(0x6c0540), vcall<DWORD>(hudlist, 3, glob<const char *>(0x624cd8), 0xffffffff)).object;
            BYTE *spacer = list_object(glob_addr(0x6c0540), vcall<DWORD>(hudlist, 3, glob<const char *>(0x624ccc), 0xffffffff));
            DWORD h = 0;
            if (vcall<DWORD>(frame, 45) == 0x320)
                h = vcall<DWORD>(spacer, 45);
            DWORD room = fn<DWORD(THISCALL *)(void *, void *)>(0x429880)(pt(self, 0x340), frame);
            room -= vcall<DWORD>(main, 39);
            room -= vcall<DWORD>(main, 44);
            BYTE *g = list_object(glob_addr(0x6c02e8), dw(self, 0x58c));
            vcall<DWORD>(g, 42, frame, h, room);
        }
    }
    if ((BYTE)fn<DWORD(THISCALL *)(void *)>(0x4e13e0)(self + 0x364))
        fn<DWORD(THISCALL *)(void *)>(0x5a0a30)(self);
    fn<DWORD(THISCALL *)(void *, void *)>(0x55bc60)(fn<void *(__cdecl *)()>(0x55b940)(), frame);
    return vcall<DWORD>(vcall<void *>(self, 1), 5, frame);
}


// The quest mode's game type: +0x58c = its index ("QuestMode", FindOrFail), then its slot 23 (the object
// loaded inline, used without a null test)
void select_quest_mode(BYTE *self)
{
    const DWORD i = fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x591fc0)(glob<void *>(0x6c02e8), glob<const char *>(0x6179f0), 0xffffffff);
    dw(self, 0x58c) = i;
    if (i != 0xffffffff)
        vcall<DWORD>(list_object(glob_addr(0x6c02e8), i), 23);
}

// 0x59add0, slot 74 (stream, binary): a save game read. The quest mode selected; the game data's slot 19;
// unless the caching table is busy the quest mode again and the save game stats (+0x350) and the table read
// (binary or as text; a failure ends here). Then the world (World::ReadSaveGame), the game data and the
// other map list, from a packed block (0x3e9, the packed size, the size, the data: unpacked into a memory
// stream read through an object stream on the stack, +0x591 set meanwhile) or directly from the stream (the
// position put back). al: every read succeeded.
DWORD THISCALL App_ReadSaveGame(BYTE *self, BYTE *stream, DWORD binary)
{
    using Read = DWORD(THISCALL *)(void *, void *);
    const DWORD read_fn = (BYTE)binary ? fn_addr(0x416fe0) : fn_addr(0x416af0);
    select_quest_mode(self);
    vcall<DWORD>(pt(self, 0x34c), 19);
    BYTE ok = 1;
    if (!pt(self, 0x33c)[0x18]) {
        select_quest_mode(self);
        const DWORD a = fn_dyn<Read>(read_fn)(pt(self, 0x350), stream);
        ok = (BYTE)a & 1;
        const DWORD b = fn_dyn<Read>(read_fn)(pt(self, 0x33c), stream);
        ok &= (BYTE)b;
        if (!ok)
            return with_al(b, 0);
    }
    const DWORD pos = fn<DWORD(THISCALL *)(void *)>(0x4a2180)(stream);
    DWORD magic;
    fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(stream, &magic, 4);
    if (magic != 0x3e9) {
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a20e0)(stream, pos);
        self[0x591] = 0;
        ok &= (BYTE)fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4f3e70)(self, stream, binary);
        ok &= (BYTE)fn_dyn<Read>(read_fn)(pt(self, 0x34c), stream);
        const DWORD r = fn_dyn<Read>(read_fn)(pt(self, 0x354), stream);
        ok &= (BYTE)r;
        return with_al(r, ok);
    }
    BYTE ident[4];
    fn<DWORD(THISCALL *)(void *)>(0x5664b0)(ident);
    BYTE *data = nullptr;
    DWORD packed, size;
    fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(stream, &packed, 4);
    fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(stream, &size, 4);
    const BYTE *src = fn<const BYTE *(THISCALL *)(void *, DWORD)>(0x4a2150)(stream, packed);
    fn<DWORD(THISCALL *)(void *, BYTE **, DWORD, const BYTE *, DWORD)>(0x5c3ba0)(ident, &data, size, src, packed);
    void *m = pool_alloc(0x20);
    if (m)
        m = fn<void *(THISCALL *)(void *, BYTE *, DWORD)>(0x505fb0)(m, data, size);
    BYTE os[0x24];
    fn<DWORD(THISCALL *)(void *)>(0x5240b0)(os);
    if ((BYTE)fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x5242f0)(os, m, 4, 0)) {
        self[0x591] = 1;
        ok &= (BYTE)fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4f3e70)(self, os, binary);
        ok &= (BYTE)fn_dyn<Read>(read_fn)(pt(self, 0x34c), os);
        ok &= (BYTE)fn_dyn<Read>(read_fn)(pt(self, 0x354), os);
        self[0x591] = 0;
    }
    fn<DWORD(THISCALL *)(void *)>(0x5241e0)(os);
    return with_al(fn<DWORD(THISCALL *)(void *)>(0x58ea40)(ident), ok);
}


// A timer of slot 3: counted down by dt while above 0 (stored as float, below 0 it becomes 0)
void count_down(BYTE *self, DWORD off, float dt)
{
    float &v = *reinterpret_cast<float *>(self + off);
    if (c03(fld(v), cst(glob_addr(0x5ebed0))))
        return;
    const long double s = fld(v) - fld(dt);
    v = fstp(s);
    if (c0(s, cst(glob_addr(0x5ebed0))))
        dw(self, off) = 0;
}

// 0x59d140, slot 3 (dt): the application's frame. The quip list's update; the game type's slot 24 (dt);
// requests set elsewhere carried out: +0x361 (the caching table's +0x19), +0x312 (RestartMission), +0x588
// (slot 112 with +0x570); World::Frame; the playing sequence's camera (CameraArrived); the scoreboard;
// +0x338 (slot 34); the hue remap cache cleared, the caching table updated, the voice's frame; the end of the
// game (+0x590: in the demo the "Demo Complete" image, then the net game's slot 10, a new main menu and the
// end credits, slot 81 (1)); the mission times +0x5a4..+0x5b0 counted down, +0x594 their longest; the status
// text's time; the current mission's ForgetOld; the music: the time 0x713f20 since the last switch and the
// count 0x713f1c of quiet frames choose the track (SwitchTrack 0, 1, 2), the count cleared every frame.
// eax: what the last steps leave (not reproduced: 0).
DWORD THISCALL App_Frame(BYTE *self, float dt)
{
    fn<DWORD(THISCALL *)(void *, float)>(0x55bd50)(fn<void *(__cdecl *)()>(0x55b940)(), dt);
    if (dw(self, 0x58c) != 0xffffffff && game_type(dw(self, 0x58c)).object) {
        BYTE *g = dw(self, 0x58c) == 0xffffffff ? nullptr : game_type(dw(self, 0x58c)).object;
        vcall<DWORD>(g, 24, dt);
    }
    BYTE *table = pt(self, 0x33c);
    if (self[0x361]) {
        self[0x361] = 0;
        table[0x19] = 1;
    }
    if (self[0x312]) {
        self[0x312] = 0;
        fn<DWORD(THISCALL *)(void *)>(0x589cc0)(pt(self, 0x33c));
    }
    if (self[0x588]) {
        vcall<DWORD>(self, 112, self + 0x570);
        self[0x588] = 0;
    }
    fn<DWORD(THISCALL *)(void *, float)>(0x4f2620)(self, dt);
    BYTE *data = pt(self, 0x34c);
    if (dw(data, 0x2c)) {
        void *layer = fn<void *(THISCALL *)(void *)>(0x4f1050)(self);
        fn<DWORD(THISCALL *)(void *, float, void *)>(0x5222a0)(pt(data, 0x2c), dt, layer);
    }
    fn<DWORD(THISCALL *)(void *)>(0x5bae00)(vcall<void *>(self, 44));
    if (self[0x338]) {
        self[0x338] = 0;
        vcall<DWORD>(self, 34);
    }
    fn<DWORD(__cdecl *)()>(0x4d4a50)();
    fn<DWORD(THISCALL *)(void *)>(0x589db0)(pt(self, 0x33c));
    fn<DWORD(THISCALL *)(void *)>(0x595b00)(fn<void *(__cdecl *)()>(0x595b50)());
    if (self[0x590]) {
        self[0x590] = 0;
        if (*glob<BYTE *>(0x62485c)) {
            DWORD images[2] = {glob_addr(0x624cbc), 0};
            fn<DWORD(THISCALL *)(void *)>(0x5be0d0)(images);
            vcall<DWORD>(pt(self, 4), 25);
            void *pal = fn<void *(__cdecl *)()>(0x433a80)();
            fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x568870)(pt(self, 4), pal, 1);
        }
        void *world = *glob<void **>(0x6c2080);
        vcall<DWORD>(vcall<void *>(world, 1), 10);
        void *m = pool_alloc(0x158);
        if (m)
            m = fn<void *(THISCALL *)(void *, void *)>(0x5be4a0)(m, fn<void *(THISCALL *)(void *)>(0x4cdf30)(*glob<void **>(0x6c2080)));
        vcall<DWORD>(*glob<void **>(0x6c2080), 23, m, 1);
        fn<DWORD(THISCALL *)(void *)>(0x5bede0)(m);
        vcall<DWORD>(*glob<void **>(0x6c2080), 81, 1);
    }
    if (!c03(fld(*reinterpret_cast<float *>(self + 0x594)), cst(glob_addr(0x5ebed0)))) {
        dw(self, 0x594) = 0;
        count_down(self, 0x5a4, dt);
        if (!c03(fld(*reinterpret_cast<float *>(self + 0x5a4)), cst(glob_addr(0x5ebed0))))
            dw(self, 0x594) = dw(self, 0x5a4);
        for (DWORD off : {0x5a8u, 0x5acu, 0x5b0u}) {
            count_down(self, off, dt);
            if (!c03(fld(*reinterpret_cast<float *>(self + off)), fld(*reinterpret_cast<float *>(self + 0x594))))
                dw(self, 0x594) = dw(self, off);
        }
    }
    float &status = *reinterpret_cast<float *>(self + 0x368);
    if (!c03(fld(status), cst(glob_addr(0x5ebed0)))) {
        const long double s = fld(status) - fld(dt);
        status = fstp(s);
        if (c03(s, cst(glob_addr(0x5ebed0)))) {
            fn<DWORD(THISCALL *)(void *)>(0x4df2b0)(self + 0x364);
            dw(self, 0x368) = 0;
        }
    }
    if (void *cur = fn<void *(THISCALL *)(void *)>(0x5127e0)(vcall<void *>(self, 98)))
        fn<DWORD(THISCALL *)(void *)>(0x5122f0)(cur);
    float &since = *glob<float *>(0x713f20);
    since = fstp(fld(since) + fld(dt));
    const DWORD quiet = *glob<DWORD *>(0x713f1c);
    void *mixer = pt(self, 0x334);
    const DWORD mode = dw(mixer, 0x24);
    using Switch = DWORD(THISCALL *)(void *, DWORD);
    if (quiet < 2 && mode != 0) {
        if (mode == 1 && !c03(fld(since), cst(glob_addr(0x5eecd8))))
            fn<Switch>(0x51e520)(mixer, 2);
        *glob<DWORD *>(0x713f1c) = 0;
        return 0;
    }
    if (quiet >= 4) {
        fn<Switch>(0x51e520)(mixer, 0);
        *glob<DWORD *>(0x713f20) = 0;
    } else if (mode == 2 || (mode == 0 && !c03(fld(since), cst(glob_addr(0x5eecd8))))) {
        fn<Switch>(0x51e520)(mixer, 1);
        *glob<DWORD *>(0x713f20) = 0;
    }
    *glob<DWORD *>(0x713f1c) = 0;
    return 0;
}


long double cst64(GlobKey address) { return (long double)*glob<const double *>(address); }
bool key(BYTE *self, DWORD k) { return (BYTE)fn<DWORD(THISCALL *)(void *, DWORD)>(0x46ed70)(vcall<void *>(self, 88), k); }

// 0x59d5c0, slot 48 (dt): the player's input and the world's update. With a game type: keys 8 or 0x2b slide
// the HUD (inventory screen closed), the mini map's fog, the chat window on key 0x2c when the game type's slot
// 33 allows it. With a first character: a requested inventory toggle (+0x348) opens the inventory screen
// (cursor to the middle, HUD away) or closes it (the materials window asked to close, the cursor back to
// +0x278/+0x27c and hidden); the character is put into the layer's monitor. World::Update (dt). The game
// type's slot 34 with key 0xf toggles the pause (+0x360, slots 28/29). Then in a multiplayer frame (slot 30
// and not 31) every player's control configuration gets its frame (slot 61's al, 1, the server's packet for
// player 0); otherwise the single player's regeneration (+0x358, +0x35c): the first character, a destructable
// one with more than 0x5ee660 hit points left, is healed by 0x5fbc98 of its hit points (at most to full, which
// ends the regeneration) every 0x603020 s. eax: not reproduced (0).
DWORD THISCALL App_PlayerFrame(BYTE *self, float dt)
{
    using Count = DWORD(THISCALL *)(void *);
    const DWORD dtw = float_bits(dt);
    if (dw(self, 0x58c) != 0xffffffff && fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(0x6c02e8), dw(self, 0x58c))) {
        if ((key(self, 8) || key(self, 0x2b)) && !dw(pt(self, 0x340), 0x100))
            fn<DWORD(THISCALL *)(void *)>(0x5084a0)(pt(self, 0x344));
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x508000)(pt(self, 0x344), dtw);
        BYTE *g = dw(self, 0x58c) == 0xffffffff ? nullptr : game_type(dw(self, 0x58c)).object;
        if ((BYTE)vcall<DWORD>(g, 33) && key(self, 0x2c))
            fn<DWORD(__cdecl *)()>(0x5c1040)();
    }
    if (fn<void *(THISCALL *)(void *)>(0x4f5d80)(self)) {
        if (self[0x348]) {
            BYTE *screen = pt(self, 0x340);
            const DWORD state = dw(screen, 0x100);
            void *cursor = glob<void *>(0x6cff40);
            if (state != 2 && state != 1) {
                fn<DWORD(THISCALL *)(void *, void *, void *)>(0x5184e0)(cursor, self + 0x278, self + 0x27c);
                const DWORD w = vcall<DWORD>(pt(self, 0x2a8), 39);
                const DWORD h = vcall<DWORD>(pt(self, 0x2a8), 45);
                fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x518570)(cursor, h >> 1, w >> 1);
                fn<DWORD(THISCALL *)(void *)>(0x508570)(pt(self, 0x344));
                fn<DWORD(THISCALL *)(void *)>(0x4294f0)(pt(self, 0x340));
            } else {
                fn<DWORD(__cdecl *)()>(0x505370)();
                fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x518570)(cursor, dw(self, 0x278), dw(self, 0x27c));
                fn<DWORD(THISCALL *)(void *)>(0x4295e0)(pt(self, 0x340));
                while ((BYTE)fn<Count>(0x518690)(cursor))
                    fn<DWORD(THISCALL *)(void *)>(0x5185f0)(cursor);
            }
            self[0x348] = 0;
        }
        void *c = fn<void *(THISCALL *)(void *)>(0x4f5d80)(self);
        BYTE *layer = fn<BYTE *(THISCALL *)(void *)>(0x4f1050)(self);
        fn<DWORD(THISCALL *)(void *, void *)>(0x4e7330)(layer + 0x3c, c);
    }
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4f3560)(self, dtw);
    if (dw(self, 0x58c) != 0xffffffff && game_type(dw(self, 0x58c)).object) {
        BYTE *g = dw(self, 0x58c) == 0xffffffff ? nullptr : list_object(glob_addr(0x6c02e8), dw(self, 0x58c));
        if ((BYTE)vcall<DWORD>(g, 34) && key(self, 0xf)) {
            if (self[0x360]) {
                self[0x360] = 0;
                vcall<DWORD>(self, 29);
            } else {
                self[0x360] = 1;
                vcall<DWORD>(self, 28);
            }
        }
    }
    const DWORD flag = (BYTE)vcall<DWORD>(self, 61);
    if ((BYTE)vcall<DWORD>(self, 30) && !(BYTE)vcall<DWORD>(self, 31)) {
        const DWORD n = fn<Count>(0x51f450)(vcall<void *>(self, 1));
        for (DWORD i = 0; i < n; i++) {
            const DWORD packet = i == 0 ? vcall<DWORD>(vcall<void *>(self, 44), 12, i) : 0;
            BYTE *s = vcall<BYTE *>(vcall<void *>(self, 44), 23, i);
            fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x470050)(pt(s, 0xe8), flag, 1, packet);
        }
        return 0;
    }
    BYTE *c = fn<BYTE *(THISCALL *)(void *)>(0x4f5d80)(self);
    if (!self[0x358] || !(BYTE)is_kind_of(c, class_of(0x48bdf0)))
        return 0;
    using Hp = long double(THISCALL *)(void *);
    float step = fstp(fn<Hp>(0x48d150)(c) * cst(glob_addr(0x5fbc98)));
    if (c03(fn<Hp>(0x48d030)(c), cst64(0x5ee660)))
        return 0;
    float &timer = *reinterpret_cast<float *>(self + 0x35c);
    if (c03(fld(timer) - fld(dt), cst(glob_addr(0x5ebed0)))) {
        do {
            if (!self[0x358])
                break;
            float amount = step;
            const float sum = fstp(fn<Hp>(0x48d030)(c) + fld(step));
            if (c03(fn<Hp>(0x48d150)(c), fld(sum))) {
                const float full = fstp(fn<Hp>(0x48d150)(c));
                amount = fstp(fld(full) - fn<Hp>(0x48d030)(c));
                self[0x358] = 0;
            }
            vcall<DWORD>(c, 98, amount);
            const long double s = fld(timer) + cst64(0x603020);
            timer = fstp(s);
            if (!c03(s - fld(dt), cst(glob_addr(0x5ebed0))))
                break;
        } while (true);
    }
    timer = fstp(fld(timer) - fld(dt));
    return 0;
}


// The game type list's records listed from a folder (the files "*.<ext>" of the prefix), as the inlined
// Init of slot 2 does it in both of its branches: records of 0x80 bytes (count + 1, the count in front,
// DefinitionRecord_Init on every one), the file names listed into them
void list_records(ZFileSpec *spec)
{
    BYTE *data = nullptr;
    const DWORD m = *glob<DWORD *>(0x6c031c) + 1;
    if (DWORD *block = static_cast<DWORD *>(heap_alloc(m * 0x80 + 4))) {
        data = reinterpret_cast<BYTE *>(block + 1);
        block[0] = m;
        fn<void(STDCALL *)(void *, DWORD, DWORD, DWORD)>(0x402090)(data, 0x80, m, fn_addr(0x5a4060));
    }
    *glob<BYTE **>(0x6c0318) = data;
    *glob<DWORD *>(0x6c031c) = fn<DWORD(__fastcall *)(ZFileSpec *, void *, DWORD, DWORD, DWORD, DWORD)>(0x4a1150)(spec, data, 0, 0x80,
                                                                                                          *glob<DWORD *>(0x6c031c), 0);
}

// 0x5a01c0, slot 2 (loading image): the game data loaded at the start. The splash screen 2; the game type
// list 0x6c02e8 ("Game Types", folder "Resources/Game Types", files ".ZGT" or in the cache folder
// ".property"; the inlined Init of the definition lists, definitions.cpp); the difficulty list; the game
// data's slot 19; the quip categories and the canned objects; the splash screen 3; World::LoadGameData;
// 0x62485c (the demo: no model "Om"); the damage type aliases "Electrical", "Laser", "Physical" of "Normal";
// the loading frame (+0x300, "Interface/Loading" through the filter 0xb/0xc) with the text 0x713e98 in the
// "Loading Font" at (width / 2, 0x118); the inventory screen (+0x340, 0x134 bytes) and its Init; the dialog
// display's and the materials window's preloads; the HUD frames "HUD Main", "HUD Spacer", "HUD Under
// MiniMap", "NEWMINIMAP" loaded; the walking control's direction pointer; without a network start from the
// command line slot 33.
DWORD THISCALL App_LoadGameData(BYTE *self, void *image)
{
    using Show = DWORD(THISCALL *)(void *, const char *, DWORD);
    fn<Show>(0x578cb0)(image, glob<const char *>(0x624e1c), 0);
    void *list = glob<void *>(0x6c02e8);
    *glob<DWORD *>(0x6c0420) = glob_addr(0x624e10);
    void *reg = fn<void *(__cdecl *)()>(0x4a35d0)();
    const DWORD index = fn<DWORD(THISCALL *)(void *, const char *, void *, DWORD, DWORD)>(0x4a34d0)(reg, glob<const char *>(0x624e10), list, 0, 1);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x523e20)(list, index);
    vcall<DWORD>(list, 40);
    lstrcpyA(glob<char *>(0x6c0320), glob<const char *>(0x624df8));
    using CStr = const char *(THISCALL *)(const void *);
    using SetPath = DWORD(THISCALL *)(void *, const char *, const char *);
    ZString prefix;
    fn<DWORD(THISCALL *)(void *)>(0x4dec90)(&prefix);
    fn<SetPath>(0x4dfa50)(&prefix, glob<const char *>(0x624df8), glob<const char *>(0x62e558));
    ZFileSpec spec;
    fn<DWORD(THISCALL *)(void *)>(0x4a09f0)(&spec);
    fn<DWORD(THISCALL *)(void *, const char *, const char *)>(0x4a0a60)(&spec, fn<CStr>(0x4e13a0)(&prefix), glob<const char *>(0x615708));
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a0b20)(&spec, 1);
    *glob<DWORD *>(0x6c031c) = fn<DWORD(__fastcall *)(void *, DWORD)>(0x4a12d0)(&spec, 0);
    *glob<DWORD *>(0x6c0318) = 0;
    if (*glob<DWORD *>(0x6c031c))
        list_records(&spec);
    else {
        ZString cache;
        fn<DWORD(THISCALL *)(void *)>(0x4dec90)(&cache);
        fn<SetPath>(0x4dfa50)(&cache, fn<const char *(__cdecl *)()>(0x4a1140)(), glob<const char *>(0x624df8));
        fn<SetPath>(0x4dfa50)(&prefix, fn<CStr>(0x4e13a0)(&cache), glob<const char *>(0x62e558));
        ZFileSpec spec2;
        fn<DWORD(THISCALL *)(void *)>(0x4a09f0)(&spec2);
        fn<DWORD(THISCALL *)(void *, const char *, const char *)>(0x4a0a60)(&spec2, fn<CStr>(0x4e13a0)(&prefix), glob<const char *>(0x60b094));
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a0b20)(&spec2, 1);
        *glob<DWORD *>(0x6c031c) = fn<DWORD(__fastcall *)(void *, DWORD)>(0x4a12d0)(&spec2, 0);
        if (*glob<DWORD *>(0x6c031c))
            list_records(&spec2);
        fn<void(STDCALL *)(void *, DWORD, DWORD, DWORD)>(0x408e70)(spec2.ext, 4, 5, fn_addr(0x4defd0));
        fn<DWORD(THISCALL *)(void *)>(0x4defd0)(&cache);
    }
    const DWORD n = *glob<DWORD *>(0x6c031c);
    if (n) {
        for (DWORD i = 0; i < *glob<DWORD *>(0x6c031c); i++) {
            const char *p = fn<CStr>(0x4e13a0)(&prefix);
            fn<DWORD(__fastcall *)(char *, const char *)>(0x587a00)(*glob<char **>(0x6c0318) + i * 0x80, p);
            fn<DWORD(__fastcall *)(char *, const char *)>(0x4a3fb0)(*glob<char **>(0x6c0318) + i * 0x80, glob<const char *>(0x62e558));
            char *name = fn<char *(__fastcall *)(char *)>(0x4a4570)(*glob<char **>(0x6c0318) + i * 0x80);
            fn<DWORD(__fastcall *)(char *, const char *)>(0x587a00)(name, glob<const char *>(0x60f634));
        }
        fn<void(__cdecl *)(void *, DWORD, DWORD, DWORD)>(0x5d097c)(*glob<void **>(0x6c0318), *glob<DWORD *>(0x6c031c), 0x80, fn_addr(0x5d3640));
        fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(list, 1, 0x80, *glob<DWORD *>(0x6c031c), *glob<void **>(0x6c0318), 0);
    } else
        *glob<DWORD *>(0x6c0318) = 0;
    void *tree = heap_alloc(0x34);
    if (tree)
        tree = fn<void *(THISCALL *)(void *, void *)>(0x47e560)(tree, list);
    *glob<void **>(0x6c0424) = tree;
    vcall<DWORD>(list, 27, tree);
    fn<void(STDCALL *)(void *, DWORD, DWORD, DWORD)>(0x408e70)(spec.ext, 4, 5, fn_addr(0x4defd0));
    fn<DWORD(THISCALL *)(void *)>(0x4defd0)(&prefix);
    using LoadList = DWORD(THISCALL *)(void *, const char *, const char *, DWORD);
    fn<LoadList>(0x5a3c50)(glob<void *>(0x6c2a00), glob<const char *>(0x624de8), glob<const char *>(0x618504), 0);
    vcall<DWORD>(pt(self, 0x34c), 19);
    fn<LoadList>(0x5a3650)(fn<void *(__cdecl *)()>(0x55b940)(), glob<const char *>(0x624dd8), glob<const char *>(0x618504), 0);
    fn<LoadList>(0x5a3950)(fn<void *(__cdecl *)()>(0x468000)(), glob<const char *>(0x60f2b0), glob<const char *>(0x618504), 0);
    fn<Show>(0x578cb0)(image, glob<const char *>(0x624db8), 0);
    fn<DWORD(THISCALL *)(void *, void *)>(0x4f13a0)(self, image);
    using Find = DWORD(THISCALL *)(void *, const char *, DWORD);
    *glob<BYTE *>(0x62485c) = fn<Find>(0x523df0)(glob<void *>(0x6cfdd8), glob<const char *>(0x624db4), 0xffffffff) == 0xffffffff;
    using Alias = DWORD(THISCALL *)(void *, const char *, const char *);
    for (DWORD a : {0x624da8u, 0x624da0u, 0x624d94u})
        fn<Alias>(0x523dc0)(glob<void *>(0x6bd760), glob_dyn<const char *>(a), glob<const char *>(0x6129f4));
    BYTE filter[0x14];
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4b1cc0)(filter, 0);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4b1db0)(filter, dw(*glob<void **>(0x6c02cc), 0x24) == 8 ? 0xb : 0xc);
    void *frame = pool_alloc(0x40);
    if (frame)
        frame = fn<void *(THISCALL *)(void *)>(0x4a8a90)(frame);
    dw(self, 0x300) = as_dw(frame);
    fn<DWORD(THISCALL *)(void *, const char *, const char *, void *, DWORD, DWORD)>(0x4a9a30)(frame, glob<const char *>(0x60a630), glob<const char *>(0x61b66c), filter, 1, 0);
    using Obj = BYTE *(THISCALL *)(void *, DWORD);
    void *fonts = glob<void *>(0x6be1e0);
    BYTE *font = fn<Obj>(0x48d8f0)(fonts, vcall<DWORD>(fonts, 3, glob<const char *>(0x624d84), 0xffffffff));
    BYTE *f = pt(self, 0x300);
    const char *text = fn<CStr>(0x4e13a0)(glob<void *>(0x713e98));
    const DWORD w = vcall<DWORD>(f, 45);
    fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, const char *, DWORD, DWORD)>(0x4a6470)(font, pt(self, 0x300), w >> 1, 0x118, text, 1, 0);
    void *inv = pool_alloc(0x134);
    if (inv)
        inv = fn<void *(THISCALL *)(void *, void *)>(0x428c00)(inv, fn<void *(THISCALL *)(void *)>(0x4cdf30)(self));
    dw(self, 0x340) = as_dw(inv);
    fn<DWORD(THISCALL *)(void *)>(0x4293c0)(inv);
    fn<DWORD(__cdecl *)()>(0x47caa0)();
    fn<DWORD(__cdecl *)()>(0x505390)();
    void *hud = glob<void *>(0x6c0540);
    for (DWORD name : {0x624cd8u, 0x624cccu, 0x624d70u, 0x624d64u})
        fn<Obj>(0x48d8f0)(hud, vcall<DWORD>(hud, 3, glob_dyn<const char *>(name), 0xffffffff));
    fn<DWORD(__cdecl *)()>(0x5432a0)();
    if (!(BYTE)fn<DWORD(__cdecl *)()>(0x519ef0)())
        vcall<DWORD>(self, 33);
    return fn<DWORD(THISCALL *)(void *)>(0x4b1d20)(filter);
}

}  // namespace

ZAX_HOOKS(c4_app2)
{
    const Hook list[] = {
#include "c4_app2_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
