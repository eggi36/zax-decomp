// Band D of the last phase 3 round, game flow, inventory, multiplayer and two Core wrappers
// (docs/DECOMPILATION.md section 8, "Phase 3 remains, band D"):
//   - CDischargeWeaponThisFrameAction's firing (slot 28 0x5a7e80), the items handed to every player
//     (GiveItemsToPlayers 0x5a26f0, used by the custom rules' Sphere of Zin options), the n-th item of an
//     inventory in category order (0x5b2aa0), a net player's record of an item (0x5b7f00);
//   - the game object's default key bindings (0x59c550, slot 41 of the application) and its keys F5 (quick
//     save), F9 (quick load) and Escape while a sequence plays (0x5a10a0);
//   - the net game's "a player joined" slot (0x5ba9c0) and the client's read of the start game info
//     (0x5bce30, the reader 0x714490's slot 2; docs/functions.csv calls it CZaxSaveGameStats::vf28);
//   - Pack/Unpack (0x5c3b40/0x5c3ba0: zlib compress2/uncompress into a new block, for the temp file fields).
//
// Floating point follows the original instruction by instruction (x87.h); floats the original moves with
// integer moves are passed as their bits. Return values: al where the original sets it (the upper bytes
// of eax are those of the call before, not reproduced: the callers test al), otherwise eax of the last call.
#include "class_info.h"
#include "ai_common.h"
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD G_APP = 0x713f14, G_FLAGS = 0x6c29a0, G_SKIPS = 0x6c1d0c, G_CONTINUOUS = 0x62ea2c;
constexpr DWORD ITEM_LIST = 0x6c0c08;          // the item definitions (a definition list)
constexpr DWORD CATALOGUE = 0x6c0c38;          // the item catalogue's records (data, count +4; stride 0x80, object +0x78)
constexpr DWORD CATEGORIES = 0x6c0800;         // the item categories (count +0x34)
constexpr DWORD ORDER = 0x71415c;              // the category order 0x5b2aa0 sorts (dwords)
constexpr DWORD TEXT_QUICKSAVE = 0x624e8c;     // "SaveGames/QuickSave.sav"

inline BYTE *app() { return *glob<BYTE **>(G_APP); }
template <typename T> T &field(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }

BYTE *root_of(void *part) { return fn<BYTE *(THISCALL *)(void *)>(0x4fc6f0)(part); }
BYTE *inventory_of(void *part) { return fn<BYTE *(THISCALL *)(void *)>(0x4267e0)(part); }
void report(DWORD text) { vcall<void>(reporter(), 17, at<const char *>(text)); }
CClassInfo *continuous_fire_item_class() { return fn<Getter>(0x472880)(); }
CClassInfo *zax_type_definition_class() { return fn<Getter>(0x5b9400)(); }
BYTE *definition_object(DWORD list, DWORD i) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(at<void *>(list), i); }
void debug_begin(GlobKey text) { fn<void(__fastcall *)(const char *)>(0x4a2650)(glob<const char *>(text)); }
void debug_end() { fn<void(__cdecl *)()>(0x4a2670)(); }
BYTE *net_layer() { return vcall<BYTE *>(world(), 44); }

// ---- CDischargeWeaponThisFrameAction ---------------------------------------------------------------------

constexpr DWORD TEXT_NO_INVENTORY = 0x625990, TEXT_NOT_CHARACTER = 0x614f68, TEXT_NOT_CONTINUOUS = 0x625954, TEXT_I_MOVED = 0x610180;

bool continuous_has(void *part) { return fn<bool(THISCALL *)(void *, void *)>(0x54de90)(*glob<void **>(G_CONTINUOUS), part); }
void continuous_set(void *part, DWORD on) { fn<void(THISCALL *)(void *, void *, DWORD)>(0x54dec0)(*glob<void **>(G_CONTINUOUS), part, on); }

}  // namespace

struct DischargeAction {
    // 0x5a7e80 (slot 28: a1, part, a3, a4, item, definition): a part whose root is destructable and dead
    // (hit points, none left) does nothing (al 1). Otherwise the part needs an inventory and a character as
    // root. A continuous fire definition fires the continuous fire item (slot 49 with "Discharge Time"
    // +0x10 as an integer), moves its beam entity (+0x18) to the character's sequence offset relative to the
    // character and turns it to the character's facing, and keeps the character in the continuous fire list
    // while the energy source has energy (al 1). Any other definition only uses up its energy (the battery,
    // when the flag 0x6c29a0 of the part allows and it has no ammunition) and its items (al 0).
    DWORD THISCALL Fire(DWORD, BYTE *part, DWORD, DWORD, BYTE *item, BYTE *def)
    {
        BYTE *root = root_of(part);
        if (root && is_kind_of(root, entity_destructable_class()) && !c3(vcall_hp(root, fn_addr(0x48d150)), cst(glob_addr(C_ZERO))) &&
            c3(vcall_hp(root, fn_addr(0x48d030)), cst(glob_addr(C_ZERO))))
            return 1;
        BYTE *inv = inventory_of(part);
        if (!inv) {
            report(glob_addr(TEXT_NO_INVENTORY));
            return 0;
        }
        BYTE *ch = root_of(part);
        if (!ch || !is_kind_of(ch, entity_character_class())) {
            report(glob_addr(TEXT_NOT_CHARACTER));
            return 0;
        }
        BYTE *zdef = def && is_kind_of(def, zax_item_definition_class()) ? def : nullptr;
        BYTE *anim = part && is_kind_of(part, entity_animated_class()) ? part : nullptr;
        if (zdef && fn<bool(THISCALL *)(void *)>(0x5b7640)(zdef)) {
            BYTE *cf = item;
            if (!cf || !is_kind_of(cf, continuous_fire_item_class())) {
                report(glob_addr(TEXT_NOT_CONTINUOUS));
                cf = nullptr;
            }
            if (continuous_has(ch) && cf) {
                vcall<void>(cf, 49, anim, (DWORD)x87_ftol(fld(field<float>(this, 0x10))));
                if (field<BYTE *>(cf, 0x18)) {
                    float p[2], q[2], r[2];
                    part_position(ch, p);
                    slot_position(ch, q);
                    q[0] = fstp(fld(q[0]) - fld(p[0]));
                    q[1] = fstp(fld(q[1]) - fld(p[1]));
                    fn<void(THISCALL *)(void *, void *, float *, const char *, DWORD)>(0x49c620)(this, ch, r, fn<const char *(THISCALL *)(void *)>(0x48aee0)(ch), 0);
                    r[0] = fstp(fld(r[0]) - fld(q[0]));
                    r[1] = fstp(fld(r[1]) - fld(q[1]));
                    vcall<void>(field<BYTE *>(cf, 0x18), 41, float_bits(r[0]), float_bits(r[1]));
                    BYTE *beam = field<BYTE *>(cf, 0x18);
                    if (is_kind_of(beam, entity_animated_class())) {
                        const DWORD angle = field<DWORD>(ch, 0xc4);
                        const long double oval = fn<long double(THISCALL *)(DWORD, DWORD)>(0x42e820)(angle, angle);
                        vcall<void>(field<BYTE *>(cf, 0x18), 104, float_bits(fstp(oval)), 1u);
                    }
                    fn<void(THISCALL *)(void *, const char *)>(0x4fc240)(field<BYTE *>(cf, 0x18), glob<const char *>(TEXT_I_MOVED));
                }
            }
            BYTE *source = fn<BYTE *(THISCALL *)(void *, void *)>(0x5b7920)(zdef, inv);
            if (!source || c03(fld(field<float>(source, 0x18)), cst(glob_addr(C_ZERO))))
                continuous_set(ch, 0);
            if (!continuous_has(ch) && cf)
                vcall<void>(cf, 50, anim);
            return 1;
        }
        BYTE *raw = def;
        if ((*glob<DWORD *>(G_FLAGS) & field<DWORD>(ch, 0x1c)) && !fn<bool(THISCALL *)(void *)>(0x4d77c0)(zdef) &&
            c0(fld(field<float>(zdef, 0x4c)), cst(glob_addr(C_ZERO)))) {
            if (BYTE *battery = fn<BYTE *(THISCALL *)(void *)>(0x5b0da0)(inv)) {
                const float use = field<float>(zdef, 0x4c);
                BYTE *rules = vcall<BYTE *>(world(), 58);
                vcall<void>(battery, 50, float_bits(fstp(fld(field<float>(rules, 0x18)) * -fld(use))));
            }
        }
        if ((*glob<DWORD *>(G_FLAGS) & field<DWORD>(ch, 0x1c)) && fn<bool(THISCALL *)(void *)>(0x4d77c0)(raw))
            fn<void(__fastcall *)(void *, DWORD, DWORD)>(0x426930)(part, field<DWORD>(raw, 0x2c), field<DWORD>(raw, 0x50));
        return 0;
    }

    // The hit points getters (st0)
    static long double vcall_hp(void *e, DWORD f) { return fn_dyn<long double(THISCALL *)(void *)>(f)(e); }
};

namespace {

// ---- Items for every player ----------------------------------------------------------------------------------

constexpr DWORD TEXT_XMAS = 0x625278, TEXT_GENERATE = 0x625258;   // "XmasShopping Cheat", "Aquiring item cheat: Generate"

// The catalogue record of a definition, -1 if it has none
DWORD catalogue_index(const void *def)
{
    const DWORD n = *at<DWORD *>(glob_addr(CATALOGUE) + 4);
    const BYTE *records = *glob<BYTE **>(CATALOGUE);
    for (DWORD i = 0; i < n; i++)
        if (*reinterpret_cast<void *const *>(records + i * 0x80 + 0x78) == def)
            return i;
    return 0xffffffffu;
}

}  // namespace

// 0x5a26f0 (on a null-terminated list of item names): every player's character gets one new item of each
// definition (a copy of the definition's default item +0x1c, otherwise a new CInventoryItem), with its
// catalogue index, generated (slot 7), added when it can be (slots 32 and 33); when that did not change the
// inventory's count +0x20 the item's definition becomes the current one +0xc. The item is deleted after.
DWORD THISCALL GiveItemsToPlayers(const char **names)
{
    debug_begin(TEXT_XMAS);
    for (DWORD i = 0; i < vcall<DWORD>(net_layer(), 11); i++) {
        BYTE *ch = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x4f5d60)(app(), i);
        if (!ch)
            continue;
        BYTE *inv = inventory_of(ch);
        for (const char **p = names; *p; p++) {
            const DWORD k = fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(ITEM_LIST), *p, -1);
            BYTE *d = definition_object(ITEM_LIST, k);
            BYTE *def = d && is_kind_of(d, zax_item_definition_class()) ? d : nullptr;
            BYTE *item;
            if (field<DWORD>(def, 0x1c)) {
                item = fn<BYTE *(THISCALL *)(void *, DWORD, int)>(0x416760)(field<void *>(def, 0x1c), 0, -1);
            } else {
                item = static_cast<BYTE *>(pool_alloc(0x18));
                item = item ? fn<BYTE *(THISCALL *)(void *)>(0x42a2b0)(item) : nullptr;
                vcall<void>(item, 19);
            }
            fn<void(THISCALL *)(void *, DWORD)>(0x54fdb0)(item, catalogue_index(def));
            debug_begin(TEXT_GENERATE);
            vcall<void>(item, 7);
            debug_end();
            const DWORD count = field<DWORD>(inv, 0x20);
            if (vcall<bool>(item, 32, ch))
                vcall<void>(item, 33, 0u, ch);
            if (field<DWORD>(inv, 0x20) == count) {
                const DWORD at_ = fn<DWORD(THISCALL *)(void *, void *)>(0x425900)(inv, def);
                if (at_ != 0xffffffffu)
                    field<DWORD>(inv, 0xc) = at_;
            }
            if (item)
                vcall<void>(item, 1, 1u);
        }
    }
    debug_end();
    return 0;
}

// ---- The inventory in category order ----------------------------------------------------------------------

// 0x5b2aa0 (fastcall: inventory, n): the item index of the n-th item when the items are listed by category
// in the categories' order (qsort with 0x5b29a0 into the static 0x71415c) and within a category from the
// first one on (0x425340, 0x425470); -1 when there are not that many (count +0x20).
DWORD __fastcall InventoryItemInCategoryOrder(BYTE *inv, DWORD n)
{
    auto count_of = [inv](DWORD c) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4251c0)(inv, c); };
    auto first_of = [inv](DWORD c) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x425340)(inv, c); };
    DWORD *order = glob<DWORD *>(ORDER);
    const DWORD categories = *at<DWORD *>(glob_addr(CATEGORIES) + 0x34);
    for (DWORD i = 0; i < categories; i++)
        order[i] = i;
    fn<void(__cdecl *)(void *, DWORD, DWORD, void *)>(0x5d097c)(order, categories, 4, code<void *>(0x5b29a0));
    const DWORD total = field<DWORD>(inv, 0x20);
    if (!total)
        return 0xffffffffu;
    DWORD ci = 0;
    DWORD in_category = count_of(order[0]);
    DWORD item = first_of(order[0]);
    while (in_category < 1 && ci < *at<DWORD *>(glob_addr(CATEGORIES) + 0x34)) {
        ci++;
        in_category = count_of(order[ci]);
        item = first_of(order[ci]);
    }
    DWORD k = 0;
    for (DWORD pos = 0; pos < total; ) {
        if (pos == n)
            return item;
        k++;
        pos++;
        if (k == in_category) {
            in_category = 0;
            k = 0;
            while (ci < *at<DWORD *>(glob_addr(CATEGORIES) + 0x34)) {
                ci++;
                in_category = count_of(order[ci]);
                item = first_of(order[ci]);
                if (in_category >= 1)
                    break;
            }
        } else {
            item = fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x425470)(inv, item, order[ci]);
        }
    }
    return 0xffffffffu;
}

// ---- A net player's item record -------------------------------------------------------------------------------

// 0x5b7f00 (item reference, player part, a3, a4, a5): for a game item definition the icon object (its slot
// 32, otherwise +0x24 of its type definition) fills the player's next record (+0x100, used +0x114; one more
// record when they run out): the reference's property count as index, the icon from the icon object's slot 25
// (part, reference, a4, &icon) when that succeeds.
DWORD THISCALL NetPlayerItemRecord(void *, void *ref, DWORD who, DWORD, DWORD a4, DWORD)
{
    BYTE *player = fn<BYTE *(__fastcall *)(DWORD)>(0x5bac80)(who);
    BYTE *def = fn<BYTE *(THISCALL *)(void *)>(0x4dd480)(ref);
    if (!def || !is_kind_of(def, zax_item_definition_class()))
        return 0;
    BYTE *icon = vcall<BYTE *>(def, 32);
    if (!icon) {
        BYTE *t = definition_object(glob_addr(CATEGORIES), field<DWORD>(def, 0x18));
        if (!t || !is_kind_of(t, zax_type_definition_class()))
            t = nullptr;
        icon = field<BYTE *>(t, 0x24);
        if (!icon)
            return 0;
    }
    if (field<DWORD>(player, 0x114) >= field<DWORD>(player, 0x104))
        fn<void(THISCALL *)(void *, DWORD)>(0x5ba450)(player, field<DWORD>(player, 0x114) + 1);
    BYTE **records = field<BYTE **>(player, 0x100);
    fn<void(THISCALL *)(void *, DWORD)>(0x5ba100)(records[field<DWORD>(player, 0x114)], fn<DWORD(THISCALL *)(void *)>(0x482de0)(ref));
    BYTE *rec = field<BYTE **>(player, 0x100)[field<DWORD>(player, 0x114)];
    DWORD value = field<DWORD>(rec, 4);
    const DWORD r = vcall<DWORD>(icon, 25, who, ref, a4, &value);
    if ((BYTE)r)
        fn<void(THISCALL *)(void *, DWORD)>(0x5ba120)(field<BYTE **>(player, 0x100)[field<DWORD>(player, 0x114)], value);
    field<DWORD>(player, 0x114)++;
    return r;
}

// ---- The net game: a player joined, the start game info read ----------------------------------------------

constexpr DWORD JOIN_FORMAT = 0x71445c;   // a ZString: the "joined" message's format

struct NetLocal {
    // 0x5ba9c0 (slot 9 of the net local class 0x602fa8: player): the base slot (empty), then for a player that
    // has just joined (+6) a chat message to everyone (the format with the player's name: its options +0x1c,
    // their +0 ZString), the start game
    // info with the current level sent to him and an empty entry appended to the application's player list
    // (+0x284); the game type's slot 28 with the player.
    void THISCALL PlayerJoined(BYTE *player)
    {
        fn<void(THISCALL *)(void *, void *)>(0x4186e0)(this, player);
        if (player[6]) {
            ZString text;
            zs_ctor(reinterpret_cast<BYTE *>(&text));
            const char *fmt = zs_c_str(glob<void *>(JOIN_FORMAT));
            // 0x5b7aa0: the player's +0x1c, its options, whose +0 is the name
            fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(&text, fmt, zs_c_str(field<void *>(player, 0x1c)));
            fn<void(THISCALL *)(void *, const char *, int)>(0x59b260)(app(), zs_c_str(&text), -1);
            ZString path;
            zs_ctor(reinterpret_cast<BYTE *>(&path));
            fn<void(THISCALL *)(void *, ZString *)>(0x4f3ac0)(world(), &path);
            void *reader = fn<void *(__cdecl *)()>(0x5bcdf0)();
            fn<void(THISCALL *)(void *, void *, void *)>(0x5bcf60)(reader, player + 0x3c, player);
            fn<DWORD(THISCALL *)(void *, void *)>(0x4ef9b0)(app() + 0x284, nullptr);
            zs_dtor(reinterpret_cast<BYTE *>(&path));
            zs_dtor(reinterpret_cast<BYTE *>(&text));
        }
        if (fn<void *(THISCALL *)(void *)>(0x59ff80)(app()))
            vcall<void>(fn<void *(THISCALL *)(void *)>(0x59ff80)(app()), 28, player);
    }
};

struct StartGameInfoReader {
    // 0x5bce30 (slot 2: packet): the game type token (0x6c02e8's table), the level and the challenge; the CD
    // key's response (gcd_compute_response) to the server (CdKeyResponseReader 0x714068 for the net layer's
    // slot 12), the net layer's slot 24, the level loaded with the game type (the world's slots 80 and 79 between
    // BeginLoading and EndLoading), then "ready to spawn" (0x714058) sent.
    void THISCALL Read(void *packet)
    {
        DWORD token;
        // 0x4c as in the exe's frame (sub esp, 0x50: the token at esp, the response from esp+4 to the frame's end):
        // gcd_compute_response writes 73 bytes (32 + 8 + 32 characters and the end) [confirmed: code 0x5bce30, 0x5c9960]
        char response[0x4c];
        fn<DWORD(THISCALL *)(void *, DWORD *, void *)>(0x5205d0)(packet, &token, glob<void *>(0x6c02e8));
        const char *level = fn<const char *(THISCALL *)(void *)>(0x5206c0)(packet);
        const char *challenge = fn<const char *(THISCALL *)(void *)>(0x5206c0)(packet);
        ZString key;
        zs_ctor(reinterpret_cast<BYTE *>(&key));
        fn<void(THISCALL *)(ZString *)>(0x46b4e0)(&key);
        fn<void(__cdecl *)(const char *, const char *, char *)>(0x5c9960)(zs_c_str(&key), challenge, response);
        const DWORD id = vcall<DWORD>(net_layer(), 12, 0u);
        void *cd = fn<void *(__cdecl *)()>(0x5ac0a0)();
        fn<void(THISCALL *)(void *, DWORD, char *)>(0x5ac180)(cd, id, response);
        vcall<void>(net_layer(), 24);
        fn<void(THISCALL *)(void *)>(0x4f3cc0)(app());
        fn<void(THISCALL *)(void *, DWORD)>(0x5a32b0)(app(), token);
        vcall<void>(world(), 80);
        void *w = world();
        const DWORD players = vcall<DWORD>(net_layer(), 11);
        vcall<void>(w, 79, players, level, 0u, 0u);
        fn<void(THISCALL *)(void *)>(0x4f3d30)(app());
        const DWORD id2 = vcall<DWORD>(net_layer(), 12, 0u);
        void *ready = fn<void *(__cdecl *)()>(0x5ac210)();
        fn<void(THISCALL *)(void *, DWORD)>(0x5ac330)(ready, id2);
        zs_dtor(reinterpret_cast<BYTE *>(&key));
    }
};

// ---- The game object's key bindings and keys ---------------------------------------------------------------

namespace {

// One binding of 0x59c550: index, category and name (ZStrings of the game object's statics 0x713e40-
// 0x713f04, or for the weapons the item's name as a C string, which is also the binding's item), the kind
// (1, 2), a flag, and the default keys (key, key 2, joystick, mouse)
struct BindingRow {
    BYTE index;
    GlobKey category, name;   // key-typed (batch 6-7): the rows' addresses go through the address layer
    BYTE name_is_string, kind, flag, item;
    DWORD keys[4];
};
const BindingRow BINDINGS[] = {
#include "game_rest_d_keys.inc"
};
constexpr DWORD FORBIDDEN_KEYS[] = {1, 0x3b, 0x44, 0x57, 0x58, 0x39};   // Escape, F1, F10, F11, F12, Space

}  // namespace

// 0x59c550 (slot 41 of the application: read the INI): a new control configuration (pool, 0x1c8 bytes,
// ControlConfig::Ctor with 45 bindings) with the 45 default bindings, saved as the defaults (0x46f980), the
// player's bindings read from the INI when the flag is set, and the keys Escape (1), F1 (0x3b), F10 (0x44),
// F11 (0x57), F12 (0x58) and Space (0x39) forbidden. eax: the configuration.
void *THISCALL DefaultKeyBindings(void *, DWORD read_ini)
{
    void *cfg = pool_alloc(0x1c8);
    if (cfg)
        cfg = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x46ef80)(cfg, read_ini, 0, 0, 0x2d);
    for (const BindingRow &b : BINDINGS) {
        const char *category = zs_c_str(glob<void *>(b.category));
        const char *name = b.name_is_string ? zs_c_str(glob<void *>(b.name)) : glob<const char *>(b.name);
        fn<void(THISCALL *)(void *, DWORD, const char *, const char *, DWORD, DWORD, DWORD, DWORD)>(0x46f430)(cfg, b.index, category, name, b.kind,
                                                                                                          0xfa, 0, b.flag);
        if (b.item)
            fn<void(THISCALL *)(void *, DWORD, const char *)>(0x46f480)(cfg, b.index, glob<const char *>(b.name));
        fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x46f4c0)(cfg, b.index, b.keys[0], b.keys[1], b.keys[2], b.keys[3]);
    }
    fn<void(THISCALL *)(void *)>(0x46f980)(cfg);
    if ((BYTE)read_ini)
        fn<void(THISCALL *)(void *, DWORD)>(0x46fcc0)(cfg, 0);
    for (DWORD key : FORBIDDEN_KEYS)
        fn<void(THISCALL *)(void *, DWORD)>(0x46f3e0)(cfg, key);
    return cfg;
}

struct GameKeys {
    BYTE &b(DWORD off) { return field<BYTE>(this, off); }

    // 0x5a10a0 (the game object's key slot: key): nothing (al 0) while a skip runs (+0x589). Escape (0x20)
    // while a sequence plays (slot 84) skips it: the status text 0x713e98, then up to 300 seconds of the game
    // in steps of +0x5c (slot 3) until the sequence ends; a sequence that does not end is ended (the save
    // data's +0x2c, CBeginNonInteractiveSequenceAction::EndScene) or reported (slots 17 and 16). F5 (0x1e)
    // saves "SaveGames/QuickSave.sav" (slot 110) and then lets every waiting window of the reporter's list
    // (+0x34: +0xec on, +0xf4 next, +0xf0 the last) close (slots 73 and 5); F9 (0x1a) loads it (slot 109)
    // while the first character lives. Neither during a multiplayer game that has not started (+0x310, +0x311,
    // +0x2a0's slot 13). Other keys and the refused ones go to World::KeyEscape.
    DWORD THISCALL Key(DWORD key)
    {
        BYTE *self = reinterpret_cast<BYTE *>(this);
        if (b(0x589))
            return 0;
        auto busy = [this]() { return b(0x310) && b(0x311) && vcall<bool>(field<void *>(this, 0x2a0), 13); };
        if (key == 0x20) {
            if (vcall<bool>(self, 84)) {
                if (!vcall<bool>(self, 84))
                    return 1;
                fn<void(THISCALL *)(void *, void *, DWORD, DWORD)>(0x5a0c00)(self, glob<void *>(0x713e98), 0x3f800000, 0);
                b(0x589) = 1;
                ++*glob<DWORD *>(G_SKIPS);
                float t = raw_float(0);
                while (b(0x589)) {
                    if (!c0(fld(t), cst(glob_addr(0x5f9174))))
                        break;
                    vcall<void>(self, 3, float_bits(fstp(fld(field<float>(self, 0x5c)))));
                    t = fstp(fld(field<float>(self, 0x5c)) + fld(t));
                }
                if (!b(0x589))
                    return 1;
                vcall<void>(self, 17, glob<const char *>(0x624f00));
                void *layer = fn<void *(THISCALL *)(void *)>(0x4f1050)(self);
                void *nis = field<void *>(field<void *>(self, 0x34c), 0x2c);
                if (nis) {
                    fn<void(THISCALL *)(void *, void *)>(0x522110)(nis, layer);
                    return 1;
                }
                vcall<void>(self, 16, glob<const char *>(0x624ea4));
                return 1;
            }
        } else if (key == 0x1e) {
            if (!busy() && vcall<bool>(self, 110, glob<const char *>(TEXT_QUICKSAVE))) {
                auto windows = []() { return field<BYTE *>(reporter(), 0x34); };
                if (windows())
                    for (BYTE *w = field<BYTE *>(windows(), 0xec); w;) {
                        if (vcall<bool>(w, 73)) {
                            vcall<void>(w, 5);
                            w = field<BYTE *>(windows(), 0xec);
                        } else {
                            if (w == field<BYTE *>(windows(), 0xf0))
                                break;
                            w = field<BYTE *>(w, 0xf4);
                        }
                    }
                return 1;
            }
        } else if (key == 0x1a) {
            if (b(0x310) && b(0x311) && !vcall<bool>(field<void *>(self, 0x2a0), 13)) {
                BYTE *first = fn<BYTE *(THISCALL *)(void *)>(0x4f5d80)(self);
                if (first && !c03(fn<long double(THISCALL *)(void *)>(0x48d030)(fn<BYTE *(THISCALL *)(void *)>(0x4f5d80)(self)), cst(glob_addr(C_ZERO)))) {
                    vcall<void>(self, 109, glob<const char *>(TEXT_QUICKSAVE));
                    return 1;
                }
            }
        }
        return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4f31c0)(self, key);
    }
};

// ---- The game start from a menu ----------------------------------------------------------------------------

constexpr DWORD GAME_TYPES = 0x6c02e8;            // the game type list (records +0x30 at 0x6c0318, count 0x6c031c)
constexpr DWORD DIFFICULTIES = 0x6c2a00, TEXT_NORMAL = 0x6129f4;
constexpr DWORD LOAD_MAP_NAME = 0x713e9c;         // a ZString of the game object's statics

// 0x5a2940 (fastcall: the menu window, e; stack a1, a2, the map choice a3): the display mode set (0x590f00), the
// difficulty "Normal", the layer drawn directly; with a1 or e the game started through the reporter's starter
// (its slot 1, slot 3 with e, a1, a2), otherwise through its slot 2 (a2, a3): then a "load map" choice left in the
// game object (+0x364 equal to 0x713e9c) is cleared, and a started game loads the choice's level (+8) with its game
// type (+4, -1 none: kept in the game object's +0x58c and the game type's slot 23 run) between BeginLoading and
// EndLoading (the world's slots 80 and 79). A started game closes the window (0x404180, slot 5). al: started.
DWORD __fastcall StartGame(BYTE *window, DWORD e, DWORD a1, DWORD a2, BYTE *choice)
{
    fn<void(__cdecl *)()>(0x590f00)();
    BYTE *save = field<BYTE *>(app(), 0x34c);
    const DWORD normal = fn<DWORD(THISCALL *)(void *, const char *, int)>(0x591fc0)(glob<void *>(DIFFICULTIES), glob<const char *>(TEXT_NORMAL), -1);
    fn<void(THISCALL *)(void *, DWORD)>(0x5bb780)(save, normal);
    fn<void(__fastcall *)(BYTE)>(0x4e9d90)(1);
    void *started;
    if (a1 || e) {
        started = vcall<void *>(vcall<void *>(reporter(), 1), 3, e, a1, a2);
    } else {
        started = vcall<void *>(vcall<void *>(reporter(), 1), 2, a2, choice);
        BYTE *game = app();
        ZString *last = zstring(game, 0x364);
        if (fn<int(THISCALL *)(ZString *, const void *)>(0x4e1700)(last, glob<void *>(LOAD_MAP_NAME)) == 0) {
            ZString empty;
            zs_ctor(reinterpret_cast<BYTE *>(&empty));
            fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(last, &empty);
            field<DWORD>(game, 0x368) = 0;
            field<BYTE>(game, 0x36c) = 0;
            zs_dtor(reinterpret_cast<BYTE *>(&empty));
        }
        if (!started)
            return 0;
        fn<void(THISCALL *)(void *)>(0x4f3cc0)(app());
        const DWORD type = field<DWORD>(choice, 4);
        field<DWORD>(app(), 0x58c) = type;
        if (type != 0xffffffffu) {
            BYTE *records = *at<BYTE **>(glob_addr(GAME_TYPES) + 0x30);
            void *gt = nullptr;
            if (records) {
                const DWORD i = type < *at<DWORD *>(glob_addr(GAME_TYPES) + 0x34) ? type : 0;
                if (!records[i * 0x80 + 0x7f]) {
                    if (!field<void *>(records, i * 0x80 + 0x78)) {
                        fn<void *(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(GAME_TYPES), i);
                        records = *at<BYTE **>(glob_addr(GAME_TYPES) + 0x30);
                    }
                    gt = field<void *>(records, i * 0x80 + 0x78);
                }
            }
            vcall<void>(gt, 23);
        }
        vcall<void>(world(), 80);
        BYTE *net = net_layer();
        const char *level = zs_c_str(choice + 8);
        vcall<void>(world(), 79, vcall<DWORD>(net, 11), level, 0u, 0u);
        fn<void(THISCALL *)(void *)>(0x4f3d30)(app());
    }
    if (started && window) {
        fn<void(THISCALL *)(void *, DWORD)>(0x404180)(window, 0);
        vcall<void>(window, 5);
    }
    return started != nullptr;
}

// ---- Pack and Unpack ------------------------------------------------------------------------------------------

// 0x5c3b40 (thiscall, the object unused: out, source, n): *out = a new block of n * 101 / 100 + 12 bytes (32-bit
// arithmetic) with the source compressed into it (compress2 0x5caa90, default level); eax: the compressed size,
// 0 when compress2 fails (the block stays with the caller).
DWORD THISCALL Pack(void *, void **out, const void *src, DWORD n)
{
    DWORD size = (DWORD)(((unsigned long long)(n * 101u) * 0x51eb851full) >> 37) + 12;
    void *block = heap_alloc(size);
    *out = block;
    const int r = fn<int(__fastcall *)(void *, DWORD *, const void *, DWORD, int)>(0x5caa90)(block, &size, src, n, -1);
    return r == 0 || r == 1 || r == 2 ? size : 0;
}
// 0x5c3ba0 (thiscall, the object unused: out, size, source, n): *out = a new block of size bytes with the
// source uncompressed into it (uncompress 0x5cab30). al: 1 for Z_OK, Z_STREAM_END and Z_NEED_DICT, otherwise 0
// with the upper bytes of the result minus 2 (as the original's dec/dec leaves them).
DWORD THISCALL Unpack(void *, void **out, DWORD size, const void *src, DWORD n)
{
    void *block = heap_alloc(size);
    *out = block;
    DWORD length = size;
    const int r = fn<int(__fastcall *)(void *, DWORD *, const void *, DWORD)>(0x5cab30)(block, &length, src, n);
    if (r == 0 || r == 1 || r == 2)
        return 1;
    return (DWORD)(r - 2) & 0xffffff00u;
}

ZAX_HOOKS(game_rest_d)
{
    const Hook list[] = {
#include "game_rest_d_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
