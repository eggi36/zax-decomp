// Checks of game_rest_d.cpp against the original, in the game thread.
//
//  - In the fake world (checks_restd_world.h): CDischargeWeaponThisFrameAction's firing (dead roots, missing
//    inventories and characters, continuous fire with and without a beam and energy, the battery and the
//    items of the other definitions under the cheat flags), GiveItemsToPlayers (0-3 players, 0-3 names,
//    default items copied or new items), a net player's item record (records running out, icons from the
//    definition or its type), a player joined, the start game info read, the default key bindings (with and
//    without the INI, the flag's other bytes), the game object's keys (Escape skipping a sequence that ends
//    or not, F5 with the reporter's window list, F9 with a living or dead character, multiplayer refusals,
//    other keys, a skip already running).
//  - Directly (side 0 with the module's hooks out): Pack and Unpack on buffers of 0 to 6000 bytes (zeros,
//    text, random bytes), Unpack also with too small and too big sizes, corrupted and random input; the n-th
//    item in category order on the real inventories of the level's parts (the first character's and up to 30
//    others, every n from -1 to the count + 2; the sorted order 0x71415c compared and put back).
#include "checks_restd_world.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_game_rest_d(Hook *out);

namespace {

using namespace rdw;
using Getter = CClassInfo *(__cdecl *)();

DWORD time_value(Gen &g, float lo, float hi)
{
    const DWORD v = g.next() % 16;
    const DWORD sp[] = {0, 0x80000000u, 0x7fc00000u, 0x7f800000u, 0xbf800000u};
    return v < 5 ? sp[v] : g.fbits_(lo, hi);
}
DWORD entity(Gen &g) { return ptrv(obj(O_ENTITY + (int)(g.next() % N_ENTITY))); }
const char *zs_c_str_of(DWORD z) { return at<const char *(THISCALL *)(const void *)>(0x4e13a0)(at<void *>(z)); }
DWORD fake(Gen &g, int first, int count, int null_one_in)
{
    if (null_one_in && g.next() % (DWORD)null_one_in == 0)
        return 0;
    return ptrv(obj(first + (int)(g.next() % (DWORD)count)));
}

// ---- The fake world -------------------------------------------------------------------------------------

void make_action(Gen &g) { fl<DWORD>(obj(O_SELF), 0x10) = time_value(g, 0.0f, 40.0f); }
void a_fire(Gen &g, DWORD *a)
{
    a[0] = g.next();
    a[1] = g.next() % 10 == 0 ? 0 : entity(g);
    a[2] = g.next();
    a[3] = g.next();
    a[4] = fake(g, O_ITEM, N_ITEM, 5);
    a[5] = fake(g, O_DEF, N_DEF, 0);
}

const char *const ITEM_NAMES[] = {"Light Pistol", "Medkit", "", "Plasma Thrower", "No such item"};
void make_names(Gen &g)
{
    BYTE *list = obj(O_SPARE + 5);
    const DWORD n = g.next() % 4;
    for (DWORD i = 0; i < n; i++)
        fl<DWORD>(list, 4 * i) = ptrv(ITEM_NAMES[g.next() % 5]);
    fl<DWORD>(list, 4 * n) = 0;
    g_self = list;
}

void make_unused(Gen &) {}
void a_record(Gen &g, DWORD *a)
{
    a[0] = ptrv(obj(O_OTHER + 2));
    a[1] = g.next();
    a[2] = g.next();
    a[3] = g.next();
    a[4] = g.next();
}
void a_player(Gen &g, DWORD *a)
{
    BYTE *p = obj(O_PLAYER);
    fl<BYTE>(p, 6) = (BYTE)(g.next() % 3 != 0);
    a[0] = ptrv(p);
}
void a_packet(Gen &, DWORD *a) { a[0] = ptrv(obj(O_PACKET)); }
void a_flag(Gen &g, DWORD *a)
{
    const DWORD v[] = {0, 1, 0x100, 0xffffff01u};
    a[0] = v[g.next() % 4];
}
void make_game(Gen &g)
{
    BYTE *s = obj(O_SELF);
    fl<DWORD>(s, 0) = ptrv(g_fvt[T_WORLD]);
    fl<BYTE>(s, 0x589) = (BYTE)(g.next() % 8 == 0);
    fl<DWORD>(s, 0x5c) = g.next() % 8 == 0 ? time_value(g, 0.0f, 1.0f) : g.fbits_(0.5f, 120.0f);
    fl<BYTE>(s, 0x310) = (BYTE)(g.next() % 3 != 0);
    fl<BYTE>(s, 0x311) = (BYTE)(g.next() % 3 != 0);
    fl<DWORD>(s, 0x2a0) = ptrv(obj(O_OTHER + 2));
    BYTE *save = obj(O_SPARE + 6);
    fl<DWORD>(s, 0x34c) = ptrv(save);
    fl<DWORD>(save, 0x2c) = fake(g, O_OTHER, N_OTHER, 3);
}
void a_key(Gen &g, DWORD *a)
{
    const DWORD keys[] = {0x1a, 0x1a, 0x1e, 0x1e, 0x20, 0x20, 0x20, 0x01, 0x3b, 0x1b, 0x21, 0x1f};
    a[0] = keys[g.next() % 12];
}
// The configuration made by the default key bindings (a pool block of each side) by its contents
void tag_config() { log_event(0xc0f1, g_last_eax ? *at<DWORD *>(g_last_eax) : 0, 0); }

// The game start: the game object's "load map" name (+0x364) a fresh string of each side, equal to 0x713e9c's
// or not; the choice (+4 the game type, +8 the level name); the window a fake or none
void make_start(Gen &g)
{
    BYTE *game = obj(O_APP);
    const char *last = g.next() % 2 ? zs_c_str_of(0x713e9c) : "Some Other Map";
    at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(game + 0x364, last);
    fl<DWORD>(game, 0x368) = g.next();
    fl<BYTE>(game, 0x36c) = (BYTE)g.next();
    BYTE *choice = obj(O_SPARE + 8);
    const DWORD types[] = {0xffffffffu, 0, 1, 3, 4, 7};
    fl<DWORD>(choice, 4) = types[g.next() % 6];
    fl<DWORD>(choice, 8) = static_text(g.next() % 2);
}
void a_start(Gen &g, DWORD *a)
{
    a[0] = fake(g, O_OTHER, N_OTHER, 5);
    a[1] = g.next() % 3 == 0 ? g.next() : 0;
    a[2] = g.next() % 3 == 0 ? g.next() : 0;
    a[3] = g.next();
    a[4] = ptrv(obj(O_SPARE + 8));
}
// The game object's name by its text (the buffers of the two sides differ)
void tag_start()
{
    BYTE *game = obj(O_APP);
    const DWORD buffer = fl<DWORD>(game, 0x364);
    log_event(0x5a29, buffer && readable(at<void *>(buffer + 8), 1) ? fnv_text(2166136261u, at<const char *>(buffer + 8)) : 0, 0);
    fl<DWORD>(game, 0x364) = 0x5a5a5a5au;
}

struct Test {
    SlotTest t;
    SelfMaker make;
    DWORD always[2];   // getters of the classes IsKindOf affirms
    DWORD nonnull;     // types whose 'n' answers are never null
    void (*before_state)();
};
const Test TESTS[] = {
    {{"CDischargeWeaponThisFrameAction::Fire", 0x5a7e80, 6, 0xff, 1500, a_fire}, make_action, {0x5b5f10, 0}, 0, nullptr},
    {{"GiveItemsToPlayers", 0x5a26f0, 0, 0, 500, nullptr}, make_names, {0x5b5f10, 0}, (1u << T_DEF) | (1u << T_ITEM) | (1u << T_INVENTORY), nullptr},
    {{"ZaxNetPlayer::ItemRecord", 0x5b7f00, 5, 0, 600, a_record}, make_unused, {0x5b5f10, 0x5b9400}, 1u << T_DEF, nullptr},
    {{"ZaxNetLocal::PlayerJoined", 0x5ba9c0, 1, 0, 300, a_player}, make_unused, {0, 0}, 0, nullptr},
    {{"StartGameInfoReader::Read", 0x5bce30, 1, 0, 200, a_packet}, make_unused, {0, 0}, 0, nullptr},
    {{"Game_DefaultKeyBindings", 0x59c550, 1, 0, 40, a_flag}, make_unused, {0, 0}, 0, tag_config},
    {{"CZaxWorld::Key", 0x5a10a0, 1, 0xff, 1500, a_key}, make_game, {0, 0}, 0, nullptr},
    {{"Game_StartFromMenu", 0x5a2940, CALL_FASTCALL5, 0xff, 600, a_start}, make_start, {0, 0}, 0, tag_start},
};

void test_slots(Tally &t)
{
    for (const Test &x : TESTS) {
        Policy p{};
        p.nonnull = x.nonnull;
        for (int i = 0; i < 2; i++)
            p.always[i] = x.always[i] ? at<Getter>(x.always[i])() : nullptr;
        p.find_minus_one = 4;
        g_before_state = x.before_state;
        const unsigned n = (unsigned)scaled(x.t.count);
        for (unsigned seed = 1; seed <= n; seed++)
            run_slot(t, x.t, seed * 2654435761u + x.t.address, x.make, p);
    }
    g_before_state = nullptr;
}

// ---- Directly: Pack, Unpack -----------------------------------------------------------------------------------

template <typename F> void two_sides(F f)
{
    for (int side = 0; side < 2; side++) {
        invcheck::side_begin(side);
        f(side);
        invcheck::side_end(side);
    }
}

const char TEXT[] = "Zax: The Alien Hunter. Sir Zax lands on Kiith-Nar and meets the Korbo. ";
void fill_input(BYTE *b, DWORD n, int kind, unsigned seed)
{
    g_seed = seed;
    for (DWORD i = 0; i < n; i++)
        b[i] = kind == 0 ? 0 : kind == 1 ? (BYTE)TEXT[i % (sizeof TEXT - 1)] : kind == 2 ? (BYTE)(random() >> 8) : (BYTE)((i / 7) & 0x1f);
}

void test_pack(Tally &t)
{
    using Pk = DWORD(THISCALL *)(void *, void **, const void *, DWORD);
    using Up = DWORD(THISCALL *)(void *, void **, DWORD, const void *, DWORD);
    BYTE *in = static_cast<BYTE *>(heap_alloc(6000));
    for (unsigned k = 0; k < (unsigned)scaled(160); k++) {
        g_seed = 0x5ac000 + k;
        const DWORD sizes[] = {0, 1, 2, 17, 100, 1000, 4096, 6000};
        const DWORD n = k % 4 == 0 ? sizes[(k / 4) % 8] : (random() >> 8) % 6000;
        const int kind = (int)(k % 4);
        fill_input(in, n, kind, k);
        void *out[2] = {};
        DWORD r[2];
        two_sides([&](int side) { r[side] = at<Pk>(0x5c3b40)(nullptr, &out[side], in, n); });
        bool same = r[0] == r[1] && out[0] && out[1] && (r[0] == 0 || memcmp(out[0], out[1], r[0]) == 0);
        char d[160];
        wsprintfA(d, "[Pack %u bytes kind %d: %u / %u]", n, kind, r[0], r[1]);
        t.check(same, "Pack", d);
        // Unpack the original's result: the right size, too small, too big, corrupted, garbage
        for (int mode = 0; mode < 5 && r[0]; mode++) {
            DWORD size = n, len = r[0];
            BYTE *src = static_cast<BYTE *>(out[0]);
            BYTE *copy = nullptr;
            if (mode == 1)
                size = n / 2;
            if (mode == 2)
                size = n + 100;
            if (mode >= 3) {
                copy = static_cast<BYTE *>(heap_alloc(len));
                memcpy(copy, src, len);
                if (mode == 3)
                    copy[len / 2] ^= 0x5a;
                else
                    for (DWORD i = 0; i < len; i++)
                        copy[i] = (BYTE)(random() >> 8);
                src = copy;
            }
            void *o2[2] = {};
            DWORD u[2];
            two_sides([&](int side) { u[side] = at<Up>(0x5c3ba0)(nullptr, &o2[side], size, src, len); });
            const bool ok = (BYTE)u[0] == 1;
            same = u[0] == u[1] && o2[0] && o2[1] && (!ok || size < n || memcmp(o2[0], o2[1], n) == 0) && (!ok || mode != 0 || memcmp(o2[0], in, n) == 0);
            wsprintfA(d, "[Unpack %u bytes kind %d mode %d: %x / %x]", n, kind, mode, u[0], u[1]);
            t.check(same, "Unpack", d);
            for (void *p : o2)
                if (p)
                    heap_free(p);
            if (copy)
                heap_free(copy);
        }
        for (void *p : out)
            if (p)
                heap_free(p);
    }
    heap_free(in);
}

// ---- Directly: the n-th item in category order (real inventories) ---------------------------------------

void test_category_order(Tally &t)
{
    const DWORD categories = *at<DWORD *>(0x6c0834);
    if (categories > 256) {
        t.check(false, "category order", "[too many categories]");
        return;
    }
    DWORD saved[256], order[2][256];
    memcpy(saved, at<void *>(0x71415c), categories * 4);
    BYTE *app = *at<BYTE **>(0x713f14);
    BYTE *invs[32];
    int ninv = 0;
    if (app)
        if (BYTE *first = at<BYTE *(THISCALL *)(void *)>(0x4f5d80)(app))
            if (BYTE *inv = at<BYTE *(THISCALL *)(void *)>(0x4267e0)(first))
                invs[ninv++] = inv;
    BYTE **parts = static_cast<BYTE **>(heap_alloc(8192 * 4));
    const int nparts = level_parts(parts, 8192);
    for (int i = 0; i < nparts && ninv < 32; i++) {
        BYTE *inv = at<BYTE *(THISCALL *)(void *)>(0x4267e0)(parts[i]);
        bool dup = !inv;
        for (int k = 0; k < ninv && !dup; k++)
            dup = invs[k] == inv;
        if (!dup && readable(inv, 0x24))
            invs[ninv++] = inv;
    }
    heap_free(parts);
    int cases = 0;
    for (int i = 0; i < ninv; i++) {
        const DWORD total = fl<DWORD>(invs[i], 0x20);
        for (DWORD k = 0xffffffffu; k == 0xffffffffu || k <= total + 2; k++) {
            DWORD r[2];
            two_sides([&](int side) {
                r[side] = at<DWORD(__fastcall *)(void *, DWORD)>(0x5b2aa0)(invs[i], k);
                memcpy(order[side], at<void *>(0x71415c), categories * 4);
            });
            char d[120];
            wsprintfA(d, "[inventory %d (%u items) n %d: %x / %x]", i, total, (int)k, r[0], r[1]);
            t.check(r[0] == r[1] && memcmp(order[0], order[1], categories * 4) == 0, "category order", d);
            cases++;
        }
    }
    memcpy(at<void *>(0x71415c), saved, categories * 4);
    log("  game_rest_d: %d inventories, %u categories, %d cases of the category order", ninv, categories, cases);
}

}  // namespace

ZAX_CHECKS(game_rest_d)
{
    (void)cls;
    (void)n;
    Tally slots{"game_rest_d: weapon action, items, net game, key bindings and keys in the fake world"},
        pack{"game_rest_d: Pack and Unpack"}, order{"game_rest_d: the n-th item in category order (real inventories)"};
    SavedBytes saved_random;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    invcheck::g_hook_table = hooks_game_rest_d;
    make_static_texts();
    if (world_on()) {
        test_slots(slots);
        world_off();
    } else {
        slots.check(false, "game_rest_d", "[the fake world could not be set up]");
    }
    slots.report();
    test_pack(pack);
    pack.report();
    test_category_order(order);
    order.report();
    restore_bytes(saved_random);
}

}  // namespace zax
