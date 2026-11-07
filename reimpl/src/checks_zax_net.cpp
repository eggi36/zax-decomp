// Checks of the game's own multiplayer layer (zax_net.cpp) against the original, in the game thread
// (machinery: checks_net_common.h; side 0 with all hooks of the module taken out, side 1 through the
// reimplementation; pointers that differ between the sides are numbered by first appearance).
//  - The packet functions (0x51ff50 and the reads and writes 0x520490-0x520da0) jump to stand-ins: the
//    writes record their values, the reads give values from the case's sequence (the words and bytes
//    the readers expect, with random upper bytes) and record what they were asked. So are the world and
//    application functions the code reaches: the net game's player (0x51f440), the player list index
//    (0x4ef9e0), the current layer and level path (0x4f1050, 0x4f3ac0), the game type (0x59ff80, 0x5a01a0),
//    the GameSpy appenders (0x51f290, 0x51f2b0), the version (0x5908c0, 0x5908a0), the net session's
//    constructor, destructor and flag (0x4f7c50, 0x4f7d80, 0x51f870), the catalogue's load (0x55e090) and
//    type definition (0x5b7af0), the quip list and its two starts (0x55b940, 0x55b770, 0x55b730), the
//    application's chat, dialogue and HUD icon (0x59b370, 0x5a5120, 0x5a34b0), the window functions
//    (0x4cdf30, 0x50cab0, 0x50db70), the scoreboard (0x566a80, 0x566fd0), the reader registration
//    (0x51ff30, 0x571b60). The world, the net game, the reporter, entities, definitions and icon data
//    are fakes; the players are blocks of 0x118 bytes (the heap) with records (the heap) and a pool
//    pointer array (the real vtable 0x604df4, its data from the pool).
//  - Compared: eax (al for the write functions), the event log, the player, its records and their
//    flags, and the fakes afterwards. The C heap comes from a pattern-filled arena of each side
//    (checks_inventory_common.h): new records keep what operator new gave them as their index, which the
//    reads compare with the packet's.
#include "checks_inventory_common.h"
#include "checks_net_common.h"

namespace zax {

int hooks_zax_net(Hook *out);

namespace {

using namespace netcheck;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
short &W(void *p, DWORD off) { return *reinterpret_cast<short *>(static_cast<BYTE *>(p) + off); }
BYTE *P(void *p, DWORD off) { return at<BYTE *>(D(p, off)); }

// ---- The case's world -------------------------------------------------------------------------------------

struct World {
    Fake *world, *net_game, *net_layer, *app, *reporter, *registry, *list, *type, *other;
    Fake *entities[3];
    Fake *defs[3];
    Fake *icons[6];
    Fake *protos[2];
    BYTE *records_list;   // the quip list or the catalogue's record block
    DWORD answers[16];
};
World g_w;
CClassInfo *g_character, *g_definition, *g_object;
DWORD ans(int k) { return g_w.answers[k]; }
// The next value of the case's sequence: a word or byte in the low bits, random upper bytes
DWORD next_value() { return random(); }

// Stand-ins for the packet functions (thiscall on the packet)
DWORD THISCALL p_block(Fake *p, void *reader, DWORD kind) { ev(0x200, p->id, ptrv(reader), kind); return random(); }
// the byte, word and bool writes take only the low bits of their argument (the originals push dwords with
// what their registers held above)
template <int K> DWORD THISCALL p_write(Fake *p, DWORD v) { ev(0x210 + K, p->id, K == 1 ? v & 0xffff : K == 2 ? v : v & 0xff); return random(); }
DWORD THISCALL p_write_text(Fake *p, const char *t) { ev(0x218, p->id, t ? fnv_text(2166136261u, t) : 0); return random(); }
template <int K> DWORD THISCALL p_write_token(Fake *p, DWORD v, void *table) { ev(0x220 + K, p->id, K == 1 ? v & 0xffff : v, ptrv(table)); return random(); }
template <int K, int BYTES> DWORD THISCALL p_read(Fake *p, void *out)
{
    // always read: where nothing is left the originals keep stack contents the reimplementation cannot know
    const DWORD v = next_value();
    memcpy(out, &v, BYTES);
    ev(0x230 + K, p->id, v);
    return random();
}
template <int K, int BYTES> DWORD THISCALL p_read_token(Fake *p, void *out, void *table)
{
    const DWORD v = next_value();
    memcpy(out, &v, BYTES);
    ev(0x240 + K, p->id, v, ptrv(table));
    return random();
}
const char *const TEXTS[] = {"", "Hello", "Act 2/M9/09 Assimilate Cave1", "Levels/Act 1/M1M2/01 Main", "x"};
const char *THISCALL p_read_text(Fake *p) { ev(0x250, p->id); return TEXTS[rnd(5)]; }
DWORD THISCALL p_marker(Fake *p, DWORD m) { ev(0x251, p->id, m); return random(); }

// Stand-ins for the world and the application
BYTE *g_players[3];   // the players the net game gives (heap blocks of the case)
BYTE *THISCALL s_player_at(Fake *g, DWORD i) { ev(0x301, g->id, i); return i < 3 ? g_players[i] : (ans(0) & 1 ? g_players[0] : nullptr); }
DWORD THISCALL s_index(void *list, void *e) { ev(0x302, list == static_cast<BYTE *>((void *)g_w.world) + 0x284, ptrv(e)); return ans(1) & 4 ? 0xffffffff : ans(1) % 4; }
void *THISCALL s_layer(void *w) { ev(0x303, w == g_w.world); return ans(2) & 1 ? (void *)g_w.other : nullptr; }
const char *THISCALL s_name(void *o) { ev(0x304, ptrv(o)); return TEXTS[ptrv(o) / 4 % 5]; }
DWORD THISCALL s_level_path(void *w, ZString *out)
{
    ev(0x305, w == g_w.world);
    at<void *(THISCALL *)(void *, const char *)>(0x4df010)(out, TEXTS[2 + rnd(2)]);
    return random();
}
void *THISCALL s_game_type(void *a) { ev(0x306, a == g_w.app); return ans(3) & 3 ? (void *)g_w.type : nullptr; }
DWORD THISCALL s_type_index(void *a) { ev(0x307, a == g_w.app); return ans(4) % 7; }
void __fastcall s_gs_text(void *out, const char *key, const char *v) { ev(0x308, ptrv(out), fnv_text(2166136261u, key), v ? fnv_text(1, v) : 0); }
void __fastcall s_gs_int(void *out, const char *key, DWORD v) { ev(0x309, ptrv(out), fnv_text(2166136261u, key), v); }
const char *THISCALL s_short_name(void *c) { ev(0x30a, ptrv(c)); return "Salvage King"; }
DWORD THISCALL s_version(ZString *z) { ev(0x30b); at<void *(THISCALL *)(void *, const char *)>(0x4df010)(z, "1.0.3"); return random(); }
DWORD __cdecl s_build() { ev(0x30c); return 1003; }
void *THISCALL s_session_ctor(BYTE *s, DWORD arg)
{
    ev(0x30d, arg);
    for (DWORD k = 4; k < 0xec; k += 4)
        D(s, k) = 0x5e000000u + k;
    return s;
}
void THISCALL s_session_dtor(BYTE *s) { ev(0x30e, D(s, 0), D(s, 0x104)); }
void THISCALL s_clear_flag(BYTE *s) { ev(0x30f, D(s, 0xec)); }
DWORD THISCALL s_list_load(void *list, DWORD i) { ev(0x310, ptrv(list), i); return random(); }
BYTE *THISCALL s_type_definition(void *def) { ev(0x311, ptrv(def)); return reinterpret_cast<BYTE *>(g_w.other); }
void *__cdecl s_quip_list() { ev(0x312); return g_w.records_list; }
DWORD THISCALL s_try_start(void *q) { ev(0x313, ptrv(q)); return random(); }
DWORD THISCALL s_retrigger(void *q) { ev(0x314, ptrv(q)); return random(); }
DWORD THISCALL s_chat(void *a, const char *t, DWORD font) { ev(0x315, a == g_w.app, fnv_text(1, t), font); return random(); }
DWORD __fastcall s_dialog(DWORD a, DWORD b, DWORD c) { ev(0x316, a, b, c); return random(); }
DWORD THISCALL s_hud_icon(void *a, DWORD d, DWORD f) { ev(0x317, a == g_w.app, d, f); return random(); }
DWORD THISCALL s_owner(void *r) { ev(0x318, r == g_w.reporter); return 0x5e7f0000; }
void *THISCALL s_window(void *block, DWORD owner, DWORD a, DWORD b, DWORD text)
{
    ev(0x319, owner, a, b);
    ev(0x31a, text);
    pool_free(block, 0x1b4);
    return g_w.other;
}
DWORD __cdecl s_hide() { ev(0x31b); return random(); }
void *__cdecl s_scoreboard() { ev(0x31c); return g_w.other; }
DWORD THISCALL s_scoreboard_read(void *s, void *p) { ev(0x31d, ptrv(s), ptrv(p)); return random(); }
DWORD THISCALL s_register(Fake *r, void *reader, const char *name) { ev(0x31e, r->id, ptrv(reader), fnv_text(1, name)); return random(); }
void __cdecl s_warp_register() { ev(0x31f); }
DWORD THISCALL s_net_layer(void *a) { ev(0x320, a == g_w.app); return random(); }
DWORD THISCALL s_second_member(void *o) { ev(0x321, ptrv(o)); return 0x5e7e0000; }
DWORD THISCALL s_send_info(void *reader, void *packet, void *player) { ev(0x322, ptrv(reader), ptrv(packet), ptrv(player)); return random(); }

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x51ff50, (void *)&p_block},           {0x520820, (void *)&p_write<0>},          {0x5209c0, (void *)&p_write<1>},
    {0x520940, (void *)&p_write<2>},        {0x520a50, (void *)&p_write<3>},          {0x520770, (void *)&p_write_text},
    {0x520c40, (void *)&p_write_token<0>},  {0x520da0, (void *)&p_write_token<1>},    {0x520ae0, (void *)&p_write_token<2>},
    {0x520490, (void *)&p_read<0, 1>},      {0x520510, (void *)&p_read<1, 2>},        {0x520550, (void *)&p_read<2, 4>},
    {0x520580, (void *)&p_read<3, 1>},      {0x520620, (void *)&p_read_token<0, 1>},  {0x520670, (void *)&p_read_token<1, 2>},
    {0x5206c0, (void *)&p_read_text},       {0x5205c0, (void *)&p_marker},            {0x51f440, (void *)&s_player_at},
    {0x4ef9e0, (void *)&s_index},           {0x4f1050, (void *)&s_layer},             {0x5007a0, (void *)&s_name},
    {0x4f3ac0, (void *)&s_level_path},      {0x59ff80, (void *)&s_game_type},         {0x5a01a0, (void *)&s_type_index},
    {0x51f290, (void *)&s_gs_text},         {0x51f2b0, (void *)&s_gs_int},            {0x415370, (void *)&s_short_name},
    {0x5908c0, (void *)&s_version},         {0x5908a0, (void *)&s_build},             {0x4f7c50, (void *)&s_session_ctor},
    {0x4f7d80, (void *)&s_session_dtor},    {0x51f870, (void *)&s_clear_flag},        {0x55e090, (void *)&s_list_load},
    {0x5b7af0, (void *)&s_type_definition}, {0x55b940, (void *)&s_quip_list},         {0x55b770, (void *)&s_try_start},
    {0x55b730, (void *)&s_retrigger},       {0x59b370, (void *)&s_chat},              {0x5a5120, (void *)&s_dialog},
    {0x5a34b0, (void *)&s_hud_icon},        {0x4cdf30, (void *)&s_owner},             {0x50cab0, (void *)&s_window},
    {0x50db70, (void *)&s_hide},            {0x566a80, (void *)&s_scoreboard},        {0x566fd0, (void *)&s_scoreboard_read},
    {0x51ff30, (void *)&s_register},        {0x571b60, (void *)&s_warp_register},     {0x5a33e0, (void *)&s_net_layer},
    {0x5007b0, (void *)&s_second_member},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_patches[NSTANDS + 1];
bool g_stand_send;   // the player info write 0x5bd1a0 a stand-in too (the player's own tests)

// Icon data: slot 1 deletes, 19 resets, 23/24 write, 26 changed (answer); definitions: slot 2 the class,
// 32 the prototype; prototypes: slot 26 reads (and may give a new icon)
template <int S> DWORD THISCALL f_icon(Fake *f, DWORD a) { ev(0x400 + S, f->id, a); return f->ret[S]; }
DWORD THISCALL f_icon_changed(Fake *f) { ev(0x41a, f->id); return (f->ret[26] & ~0xffu) | (rnd(3) == 0); }
CClassInfo *THISCALL f_class(Fake *f) { return reinterpret_cast<CClassInfo *>(static_cast<uintptr_t>(f->ret[2])); }
void *THISCALL f_proto(Fake *f) { ev(0x420, f->id); return f->ret[32] ? g_w.protos[f->ret[32] & 1] : nullptr; }
DWORD THISCALL f_proto_read(Fake *f, void *packet, void **icon)
{
    ev(0x41b, f->id, ptrv(packet), ptrv(*icon));
    if (rnd(2))
        *icon = g_w.icons[5];
    return (random() & ~0xffu) | (rnd(3) != 0);
}
DWORD THISCALL f_world_net_game(Fake *) { return ptrv(g_w.net_game); }
DWORD THISCALL f_server(Fake *f) { ev(0x430, f->id); return (random() & ~0xffu) | (ans(5) & 1); }
DWORD THISCALL f_net_layer(Fake *) { ev(0x431); return ptrv(g_w.net_layer); }
DWORD THISCALL f_local(Fake *) { ev(0x432); return ans(6) % 4; }
DWORD THISCALL f_list_name(Fake *, DWORD i) { ev(0x433, i); return ptrv(TEXTS[i % 5]); }
DWORD THISCALL f_list_make(Fake *, const char *folder, const char *name, void **out)
{
    ev(0x434, fnv_text(1, folder), fnv_text(1, name), (DWORD)(reinterpret_cast<BYTE *>(out) - g_w.records_list));
    return ptrv(g_w.defs[fnv_text(1, name) % 3]);
}
DWORD THISCALL f_window_show(Fake *r, void *w, DWORD a) { ev(0x435, r->id, ptrv(w), a); return random(); }
DWORD THISCALL f_player_update(BYTE *p) { ev(0x436, D(p, 0x104)); return random(); }
void *g_player_vtable[4];

void init_fake(Fake *f)
{
    f->vt[1] = ptrv((void *)&f_icon<1>);
    f->vt[2] = ptrv((void *)&f_class);
    f->vt[19] = ptrv((void *)&f_icon<19>);
    f->vt[23] = ptrv((void *)&f_icon<23>);
    f->vt[24] = ptrv((void *)&f_icon<24>);
    f->vt[26] = ptrv((void *)&f_icon_changed);
    f->vt[32] = ptrv((void *)&f_proto);
    f->ret[2] = ptrv(g_object);
    for (int k = 0; k < 64; k++)
        if (k != 2)
            f->ret[k] = random();
}

constexpr DWORD CATALOGUE = 0x6c0c08;
BYTE g_saved_catalogue[0x40];
DWORD g_saved_world, g_saved_app, g_saved_reporter, g_saved_local;

// A player of the case: a heap block filled from the seed, its records (heap) with fake icons, the array
// data from the pool
BYTE *make_player(DWORD count, DWORD last, bool null_records = false)
{
    BYTE *p = static_cast<BYTE *>(heap_alloc(0x118));
    for (DWORD k = 0; k < 0x118; k += 4)
        D(p, k) = random();
    D(p, 0) = ptrv(g_player_vtable);
    p[6] = (BYTE)rnd(2);
    D(p, 0x14) = rnd(3);
    for (DWORD k = 0xec; k <= 0xf6; k += 2)
        W(p, k) = (short)(rnd(4) ? rnd(100) : random());
    D(p, 0xfc) = 0x604df4;
    const DWORD cap = count + rnd(3);
    D(p, 0x104) = count;
    D(p, 0x108) = cap;
    D(p, 0x100) = cap ? ptrv(pool_get_alloc(cap * 4)) : 0;
    for (DWORD i = 0; i < count; i++) {
        BYTE *r = !null_records || rnd(8) ? static_cast<BYTE *>(heap_alloc(0xc)) : nullptr;
        if (r) {
            D(r, 0) = rnd(4) ? rnd(6) : random();
            D(r, 4) = !null_records || rnd(6) ? ptrv(g_w.icons[rnd(5)]) : 0;
            r[8] = (BYTE)(rnd(3) ? 0 : rnd(4));
        }
        D(P(p, 0x100), i * 4) = ptrv(r);
    }
    D(p, 0x10c) = rnd(3) ? 0 : random() & 0x787;
    D(p, 0x110) = last;
    return p;
}
// The player's state (through norm), then it is freed: its records, the array data, the block
void finish_player(BYTE *p, bool alive)
{
    if (!alive)
        return;
    st_dwords(p + 4, 0xf8 - 4);
    st_raw(D(p, 0x104));
    st_raw(D(p, 0x108));
    st_raw(D(p, 0x10c));
    st_raw(D(p, 0x110));
    for (DWORD i = 0; i < D(p, 0x104); i++) {
        BYTE *r = P(P(p, 0x100), i * 4);
        if (!r) {
            st_raw(0xdead0002);
            continue;
        }
        // a new record (its icon data real: the pool, vtable 0x5ede94): its index is what operator new
        // left, its icon data compared by its vtable
        if (D(r, 4) && *at<DWORD *>(D(r, 4)) == 0x5ede94) {
            st_raw(0x5ede94);
            st_raw(r[8]);
            vcall<void *>(P(r, 4), 1, 1);
        } else {
            st_raw(D(r, 0));
            st(D(r, 4));
            st_raw(r[8]);
        }
        heap_free(r);
    }
    if (P(p, 0x100))
        pool_get_free(P(p, 0x100), D(p, 0x108) * 4);
    heap_free(p);
}

void setup(unsigned seed)
{
    (void)seed;
    World &w = g_w;
    Fake **all[] = {&w.world, &w.net_game, &w.net_layer, &w.app, &w.reporter, &w.registry, &w.list, &w.type, &w.other};
    DWORD id = 1;
    for (Fake **f : all) {
        *f = new_fake(id++);
        init_fake(*f);
    }
    for (Fake *&f : w.entities) {
        f = new_fake(id++);
        init_fake(f);
        f->ret[2] = ptrv(rnd(5) ? g_character : g_object);
    }
    for (Fake *&f : w.defs) {
        f = new_fake(id++);
        init_fake(f);
        f->ret[2] = ptrv(rnd(5) ? g_definition : g_object);
        f->ret[32] = rnd(3);
    }
    for (Fake *&f : w.icons) {
        f = new_fake(id++);
        init_fake(f);
    }
    for (Fake *&f : w.protos) {
        f = new_fake(id++);
        init_fake(f);
        f->vt[26] = ptrv((void *)&f_proto_read);
    }
    w.other->ret[7] = ptrv(w.protos[0]);   // +0x24 of the fake: a type definition's prototype
    w.world->vt[1] = ptrv((void *)&f_world_net_game);
    w.world->vt[44] = ptrv((void *)&f_net_layer);
    w.net_game->vt[12] = ptrv((void *)&f_server);
    w.net_layer->vt[28] = ptrv((void *)&f_local);
    w.reporter->vt[23] = ptrv((void *)&f_window_show);
    w.list->vt[1] = ptrv((void *)&f_list_name);
    w.list->vt[37] = ptrv((void *)&f_list_make);
    // the record block of a definition list (the quip list's or the catalogue's): 4 records
    w.records_list = static_cast<BYTE *>(arena(0x80 * 4 + 0x40));
    for (DWORD i = 0; i < 4; i++) {
        BYTE *r = w.records_list + 0x40 + i * 0x80;
        lstrcpyA(reinterpret_cast<char *>(r), TEXTS[i]);
        D(r, 0x78) = rnd(3) ? ptrv(w.defs[i % 3]) : 0;
        r[0x7f] = (BYTE)(rnd(6) == 0);
    }
    for (DWORD &a : w.answers)
        a = random();
    g_player_vtable[1] = (void *)&f_player_update;
}
// The quip list fake: the list's vtable, +0x30 the records, +0x34 the count, +0x38 the folder
void make_quip_list()
{
    BYTE *l = g_w.records_list;
    D(l, 0) = g_w.list->vftable;
    D(l, 0x30) = rnd(6) ? ptrv(l + 0x40) : 0;
    D(l, 0x34) = 1 + rnd(4);
    lstrcpyA(reinterpret_cast<char *>(l + 0x38), rnd(2) ? "Q/" : "");
}

void begin_world()
{
    for (int i = 0; i < NSTANDS; i++)
        patch_jump(g_patches[i], STANDS[i].address, STANDS[i].fn);
    if (g_stand_send)
        patch_jump(g_patches[NSTANDS], 0x5bd1a0, (void *)&s_send_info);
    g_saved_world = *at<DWORD *>(0x6c2080);
    g_saved_app = *at<DWORD *>(0x713f14);
    g_saved_reporter = *at<DWORD *>(0x6c02cc);
    g_saved_local = *at<DWORD *>(0x5fe8f8);
    memcpy(g_saved_catalogue, at<void *>(CATALOGUE), sizeof g_saved_catalogue);
    *at<DWORD *>(0x6c2080) = ptrv(g_w.world);
    *at<DWORD *>(0x713f14) = ptrv(g_w.app);
    *at<DWORD *>(0x6c02cc) = ptrv(g_w.reporter);
    // the catalogue's records and count (+0x30, +0x34) for the records' reads
    D(at<void *>(CATALOGUE), 0x30) = rnd(8) ? ptrv(g_w.records_list + 0x40) : 0;
    D(at<void *>(CATALOGUE), 0x34) = 1 + rnd(4);
}
void end_world()
{
    memcpy(at<void *>(CATALOGUE), g_saved_catalogue, sizeof g_saved_catalogue);
    *at<DWORD *>(0x6c02cc) = g_saved_reporter;
    *at<DWORD *>(0x713f14) = g_saved_app;
    *at<DWORD *>(0x6c2080) = g_saved_world;
    if (g_stand_send)
        unpatch(g_patches[NSTANDS]);
    for (int i = NSTANDS - 1; i >= 0; i--)
        unpatch(g_patches[i]);
}

// ---- The cases --------------------------------------------------------------------------------------------

enum Fn {
    R_WRITE, R_WRITE_ALL, R_READ, R_CHANGED, R_SET_INDEX, R_SET_ICON,
    P_MAKE, P_DTOR, P_RESET, P_MASK, P_SET, P_SET_F8, P_COUNT, P_UPDATE, P_SEND_ALL,
    L_AT, L_OF, L_INDEX, L_EITHER, G_KEYS, G_BASIC,
    A_READERS, W_CHAT, W_START, W_INFO, W_QUIP, W_DIALOG, W_ICON,
    D_CHAT, D_INFO, D_QUIP, D_DIALOG, D_MATCH, D_HIDE, D_ICON, D_SCORE, REGISTER, NFN
};
const char *const NAMES[NFN] = {
    "record Write", "record WriteAll", "record Read", "record Changed", "record SetIndex", "record SetIcon",
    "player Make", "player DeletingDtor", "player ResetScores", "player RecordMask", "player score setters", "player SetF8",
    "player SetRecordCount", "player Update", "player SendAll",
    "PlayerAt", "PlayerOf", "PlayerIndexOf", "PlayerOfEither", "GameSpy game keys", "GameSpy basic keys",
    "reader accessors", "chat Write", "start info Write", "player info Write", "quip Write", "dialog Write", "icon Write",
    "chat Read", "player info Read", "quip Read", "dialog Read", "match over Read", "match over hide Read", "icon Read",
    "scoreboard Read", "reader registration"};

using T0 = DWORD(THISCALL *)(void *);
using T1 = DWORD(THISCALL *)(void *, DWORD);
using T2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
using T3 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD);
using T4 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD);

DWORD record_state(BYTE *r)
{
    st_raw(D(r, 0));
    st(D(r, 4));
    st_raw(r[8]);
    return 0;
}

void run(Fn f)
{
    World &w = g_w;
    invcheck::arena_on(g_side, 0x6d);
    setup(0);
    for (BYTE *&p : g_players)
        p = nullptr;
    g_stand_send = f >= P_MAKE && f <= P_SEND_ALL;
    begin_world();
    // the records' reads look up their definition without a test (the original reads through null):
    // a complete catalogue for them
    if (f == R_READ || f == D_INFO) {
        D(at<void *>(CATALOGUE), 0x30) = ptrv(w.records_list + 0x40);
        for (DWORD i = 0; i < 4; i++) {
            BYTE *e = w.records_list + 0x40 + i * 0x80;
            D(e, 0x78) = ptrv(w.defs[i % 3]);
            e[0x7f] = 0;
        }
        for (Fake *d : w.defs)
            d->ret[2] = ptrv(g_definition);
    }
    Fake *packet = new_fake(0x50);
    DWORD r = 0;
    switch (f) {
    case R_WRITE: case R_WRITE_ALL: case R_READ: case R_CHANGED: case R_SET_INDEX: case R_SET_ICON: {
        BYTE *rec = static_cast<BYTE *>(arena(0x10));
        D(rec, 0) = rnd(3) ? rnd(5) : random();
        D(rec, 4) = ptrv(w.icons[rnd(5)]);
        rec[8] = (BYTE)rnd(4);
        const DWORD fn[] = {0x5b9f20, 0x5b9fa0, 0x5b9fe0, 0x5ba0e0, 0x5ba100, 0x5ba120};
        const DWORD a = f == R_SET_INDEX ? (rnd(2) ? D(rec, 0) : rnd(6)) : f == R_SET_ICON ? ptrv(w.icons[rnd(6)]) : ptrv(packet);
        r = f == R_CHANGED ? at<T0>(fn[f])(rec) : at<T1>(fn[f])(rec, a);
        if (f != R_CHANGED)
            r = 0;   // void in use
        record_state(rec);
        break;
    }
    case P_MAKE: {
        BYTE *p = at<BYTE *(THISCALL *)(void *, DWORD)>(0x5ba780)(w.other, random());
        if (p) {   // +0xf8-+0xfb and +0x110 are what operator new left
            st_dwords(p, 0xf8);
            st_dwords(p + 0xfc, 0x14);
            D(p, 0) = 0;
            heap_free(p);
        }
        break;
    }
    case P_DTOR: {
        BYTE *p = make_player(rnd(5), rnd(5), true);
        const DWORD flags = rnd(2);
        r = at<T1>(0x5ba140)(p, flags) == ptrv(p);
        if (!flags) {
            st_dwords(p + 0xfc, 0x10);
            heap_free(p);
        }
        break;
    }
    case P_RESET: case P_MASK: case P_SET: case P_SET_F8: case P_COUNT: case P_UPDATE: case P_SEND_ALL: {
        BYTE *p = make_player(rnd(5), rnd(6), f == P_COUNT);
        if (f == P_RESET)
            r = at<T0>(0x5ba230)(p);
        else if (f == P_MASK)
            r = at<T0>(0x5ba2d0)(p);
        else if (f == P_SET) {
            const DWORD fns[6] = {0x5ba340, 0x5ba360, 0x5ba390, 0x5ba3c0, 0x5ba3f0, 0x5ba420};
            for (int k = 0; k < 3; k++) {
                const DWORD fn = fns[rnd(6)];
                const DWORD v = rnd(3) ? (DWORD)(int)W(p, 0xec + (fn == 0x5ba340 ? 0 : 2)) : rnd(2) ? random() : rnd(9) - 4;
                ev(0x500, fn, at<T1>(fn)(p, v));
            }
        } else if (f == P_SET_F8)
            at<T1>(0x5ba440)(p, random());
        else if (f == P_COUNT)
            at<T1>(0x5ba450)(p, rnd(7));
        else
            at<T0>(f == P_UPDATE ? 0x5ba580 : 0x5ba640)(p);
        finish_player(p, true);
        break;
    }
    case L_AT: case L_OF: case L_INDEX: case L_EITHER: {
        for (int i = 0; i < 3; i++)
            g_players[i] = rnd(4) ? static_cast<BYTE *>(arena(0x20)) : nullptr;
        void *e = rnd(5) ? (void *)w.entities[rnd(3)] : nullptr, *o = rnd(5) ? (void *)w.entities[rnd(3)] : nullptr;
        if (f == L_AT)
            r = at<DWORD(__fastcall *)(DWORD)>(0x5ba810)(rnd(5));
        else if (f == L_OF)
            r = at<DWORD(__fastcall *)(void *)>(0x5bac80)(e);
        else if (f == L_INDEX)
            r = at<DWORD(__fastcall *)(void *)>(0x5bacd0)(e);
        else
            r = at<DWORD(__fastcall *)(void *, void *)>(0x5babc0)(e, o);
        break;
    }
    case G_KEYS: at<T1>(0x5bad00)(w.other, ptrv(packet)); break;
    case G_BASIC: at<T1>(0x5bad90)(w.other, ptrv(packet)); break;
    case A_READERS: {
        const DWORD fns[9] = {0x5bcd30, 0x5bcdf0, 0x5bd020, 0x5bd300, 0x5bd5d0, 0x5bd6b0, 0x5bd760, 0x5bd7b0, 0x5bd880};
        for (DWORD fn : fns)
            st_raw(at<DWORD(__cdecl *)()>(fn)());
        break;
    }
    case W_CHAT: r = at<T3>(0x5bcd80)(w.other, ptrv(packet), ptrv(TEXTS[rnd(5)]), random()) & 0xff; break;
    case W_START: {
        BYTE *player = static_cast<BYTE *>(arena(0x30));
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(player + 0x24, TEXTS[rnd(5)]);
        r = at<T2>(0x5bcf60)(w.other, ptrv(packet), ptrv(player)) & 0xff;
        at<void(THISCALL *)(void *)>(0x4defd0)(player + 0x24);
        break;
    }
    case W_INFO: {
        BYTE *p = make_player(rnd(5), rnd(6));
        r = at<T2>(0x5bd1a0)(w.other, ptrv(packet), ptrv(p)) & 0xff;
        finish_player(p, true);
        break;
    }
    case W_QUIP: r = at<T3>(0x5bd560)(w.other, ptrv(packet), random(), random()) & 0xff; break;
    case W_DIALOG: r = at<T4>(0x5bd640)(w.other, ptrv(packet), random(), random(), random()) & 0xff; break;
    case W_ICON: r = at<T3>(0x5bd810)(w.other, ptrv(packet), random(), random()) & 0xff; break;
    case D_INFO: {
        for (int i = 0; i < 3; i++)
            g_players[i] = make_player(rnd(4), rnd(5));
        *at<DWORD *>(0x5fe8f8) = rnd(3);
        r = at<T1>(0x5bd030)(w.other, ptrv(packet));
        for (int i = 0; i < 3; i++)
            finish_player(g_players[i], true);
        break;
    }
    case D_QUIP: make_quip_list(); r = at<T1>(0x5bd310)(w.other, ptrv(packet)); break;
    case D_CHAT: r = at<T1>(0x5bcd40)(w.other, ptrv(packet)); break;
    case D_DIALOG: r = at<T1>(0x5bd5e0)(w.other, ptrv(packet)); break;
    case D_MATCH: r = at<T1>(0x5bd6c0)(w.other, ptrv(packet)); break;
    case D_HIDE: r = at<T1>(0x5bd770)(w.other, ptrv(packet)); break;
    case D_ICON: r = at<T1>(0x5bd7c0)(w.other, ptrv(packet)); break;
    case D_SCORE: r = at<T1>(0x5bd890)(w.other, ptrv(packet)); break;
    case REGISTER: r = at<T0>(0x5bd8b0)(w.registry); break;
    default: break;
    }
    *at<DWORD *>(0x5fe8f8) = g_saved_local;
    end_world();
    invcheck::arena_off();
    st(r);
}

// ---- Real data: the running game's lookups (read only) -----------------------------------------------------------

// The running game's first character and the world's player entities (+0x284) through the lookups
// 0x5bac80, 0x5bacd0, 0x5babc0 and the GameSpy keys into a string of our own, no stand-ins; the results
// compared raw
void test_real(Tally &t)
{
    void *world = *at<void **>(0x6c2080);
    if (!world) {
        log("  zax net: no world");
        return;
    }
    void *entities[6];
    int n = 0;
    entities[n++] = at<void *(THISCALL *)(void *)>(0x4f5d80)(world);
    BYTE *list = static_cast<BYTE *>(world) + 0x284;
    const DWORD count = at<DWORD(THISCALL *)(void *)>(0x4ef8f0)(list);
    for (DWORD i = 0; i < count && n < 5; i++)
        entities[n++] = *at<void **(THISCALL *)(void *, DWORD)>(0x4ef8e0)(list, i);
    entities[n++] = nullptr;
    for (int i = 0; i < n; i++)
        for (int k = 0; k < 4; k++) {
            DWORD r[2];
            unsigned h[2];
            for (int side = 0; side < 2; side++) {
                g_side = side;
                if (side == 0)
                    hooks_out();
                ZString out;
                original<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&out, "");
                h[side] = 0;
                switch (k) {
                case 0: r[side] = at<DWORD(__fastcall *)(void *)>(0x5bac80)(entities[i]); break;
                case 1: r[side] = at<DWORD(__fastcall *)(void *)>(0x5bacd0)(entities[i]); break;
                case 2: r[side] = at<DWORD(__fastcall *)(void *, void *)>(0x5babc0)(entities[i], entities[(i + 1) % n]); break;
                case 3:
                    at<void(THISCALL *)(void *, void *)>(i & 1 ? 0x5bad00 : 0x5bad90)(nullptr, &out);
                    r[side] = 0;
                    h[side] = fnv_text(2166136261u, out.c_str());
                    break;
                }
                original<void(THISCALL *)(ZString *)>(0x4defd0)(&out);
                if (side == 0)
                    hooks_in();
            }
            char d[96];
            wsprintfA(d, "[entity %d, lookup %d: %x / %x, %x / %x]", i, k, r[0], r[1], h[0], h[1]);
            t.check(r[0] == r[1] && h[0] == h[1], "real lookups", d);
        }
}

}  // namespace

ZAX_CHECKS(zax_net)
{
    (void)cls;
    (void)n;
    g_table = &hooks_zax_net;
    if (!hook_sites_fit("zax_net", hooks_zax_net(nullptr), MAX_SITES))
        return;
    if (!invcheck::arena_setup()) {
        log("Check zax net: no arena");
        return;
    }
    g_character = at<CClassInfo *(__cdecl *)()>(0x49a930)();
    g_definition = at<CClassInfo *(__cdecl *)()>(0x5b5f10)();
    g_object = at<CClassInfo *(__cdecl *)()>(0x4163b0)();
    const DWORD saved_local = *at<DWORD *>(0x5fe8f8);
    for (int f = 0; f < NFN; f++) {
        log("  %s", NAMES[f]);   // the last line before a case that ends the game
        const int cases = f == A_READERS || f == REGISTER ? 3 : f == D_INFO || f == W_INFO || f == P_COUNT || f == R_READ ? 200 : 100;
        char what[64];
        wsprintfA(what, "zax net: %s", NAMES[f]);
        run_cases(what, scaled(cases), 0x300000u + 0x10000u * (unsigned)f, [&](int) { run((Fn)f); });
    }
    *at<DWORD *>(0x5fe8f8) = saved_local;
    Tally real{"zax net: the running game's player lookups and GameSpy keys (real data)"};
    test_real(real);
    real.report();
}


// ---- The scoreboard (scoreboard.cpp) -------------------------------------------------------------------------
//  The scoreboard is the exe's static at 0x713220 (guard 0x713260): saved, set up for the case (rows and
//  team rows from the C heap of the side's arena, the arrays' data from the pool, or not constructed
//  yet with atexit 0x5d0863 a stand-in), compared (texts by content) and freed, then put back. The fill
//  0x5bae00 runs on a fake net game, world and application with stand-ins for the player, the player
//  list (0x4ef8f0, 0x4ef8e0), the inventory and its category (0x4267e0, 0x591fc0, 0x425290), the team
//  (0x51f460) and its name and score (0x5007b0, 0x482de0).

int hooks_scoreboard(Hook *out);

namespace {

constexpr DWORD BOARD = 0x713220, BOARD_GUARD = 0x713260;
BYTE g_saved_board[0x44];
ZString g_names[4];   // the names the stand-ins give (made once)
const char *const NAMES2[4] = {"Zax", "Crater", "", "Player 4"};

DWORD THISCALL p_read_string(Fake *p, ZString *out)
{
    ev(0x252, p->id);
    at<void *(THISCALL *)(void *, const char *)>(0x4df010)(out, TEXTS[rnd(5)]);
    return random();
}
DWORD THISCALL s_list_count(void *list) { ev(0x601, list == static_cast<BYTE *>((void *)g_w.world) + 0x284); return ans(7) % 6; }
DWORD g_entities[8];
DWORD *THISCALL s_list_at(void *, DWORD i) { ev(0x602, i); return &g_entities[i & 7]; }
void *THISCALL s_inventory2(void *e) { ev(0x603, ptrv(e)); return ans(8) >> (ptrv(e) & 7) & 1 ? (void *)g_w.other : nullptr; }
DWORD THISCALL s_find_or_fail2(void *list, const char *name, int hint) { ev(0x604, ptrv(list), fnv_text(1, name), (DWORD)hint); return 0x5e7b0000 + rnd(9); }
DWORD THISCALL s_cur_index(void *inv, DWORD cat) { ev(0x605, ptrv(inv), cat); return rnd(3) ? 0xffffffff : rnd(5); }
BYTE *g_teams[4];
BYTE *THISCALL s_team_at(void *g, DWORD i) { ev(0x606, ptrv(g), i); return g_teams[i & 3]; }
ZString *THISCALL s_team_name(BYTE *t) { ev(0x607, ptrv(t)); return &g_names[D(t, 0) & 3]; }
DWORD THISCALL s_team_score(BYTE *t) { ev(0x608, ptrv(t)); return D(t, 4); }
float THISCALL f_time(Fake *f) { ev(0x609, f->id); return (float)(int)(f->ret[5] % 1000) * 0.25f; }
DWORD THISCALL f_count(Fake *f) { ev(0x60a, f->id); return ans(9) % 4; }   // players 0-2 exist
DWORD THISCALL f_connection(Fake *f, DWORD i) { ev(0x60b, f->id, i); return ptrv(g_w.reporter); }
DWORD THISCALL f_ping(Fake *f, void *player) { ev(0x60c, f->id, ptrv(player)); return random(); }
DWORD THISCALL f_set_time(Fake *f, DWORD v) { ev(0x60d, f->id, v); return random(); }
DWORD THISCALL f_layer13(Fake *f) { ev(0x60e, f->id); return random(); }

const Stand BOARD_STANDS[] = {
    {0x51ff50, (void *)&p_block},          {0x520820, (void *)&p_write<0>},     {0x5209c0, (void *)&p_write<1>},
    {0x520940, (void *)&p_write<2>},       {0x520a50, (void *)&p_write<3>},     {0x520770, (void *)&p_write_text},
    {0x520490, (void *)&p_read<0, 1>},     {0x520510, (void *)&p_read<1, 2>},   {0x520550, (void *)&p_read<2, 4>},
    {0x520580, (void *)&p_read<3, 1>},     {0x520700, (void *)&p_read_string},  {0x51f440, (void *)&s_player_at},
    {0x59ff80, (void *)&s_game_type},      {0x4ef8f0, (void *)&s_list_count},   {0x4ef8e0, (void *)&s_list_at},
    {0x4267e0, (void *)&s_inventory2},     {0x591fc0, (void *)&s_find_or_fail2}, {0x425290, (void *)&s_cur_index},
    {0x51f460, (void *)&s_team_at},        {0x5007b0, (void *)&s_team_name},    {0x482de0, (void *)&s_team_score},
    {0x5d0863, (void *)&fake_atexit},
};
constexpr int NBOARD_STANDS = sizeof BOARD_STANDS / sizeof BOARD_STANDS[0];
CodePatch g_board_patches[NBOARD_STANDS];

// A row (0x1c) or team row (8) of the case: the C heap (the side's arena), its name constructed
BYTE *make_row(DWORD size)
{
    BYTE *r = static_cast<BYTE *>(heap_alloc(size));
    at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(r, NAMES2[rnd(4)]);
    for (DWORD k = 4; k < size; k += 4)
        D(r, k) = rnd(3) ? rnd(40) : random();
    return r;
}
// A pool array of the board with n pointers (or indices)
void make_array(BYTE *a, DWORD vtable, DWORD n)
{
    const DWORD cap = n + rnd(3);
    D(a, 0) = vtable;
    D(a, 8) = n;
    D(a, 0xc) = cap;
    D(a, 4) = cap ? ptrv(pool_get_alloc(cap * 4)) : 0;
}
void setup_board()
{
    BYTE *b = at<BYTE *>(BOARD);
    for (DWORD k = 0; k < 0x40; k += 4)
        D(b, k) = 0x5e5e0000 + k;
    if (!rnd(8)) {   // not constructed yet
        *at<BYTE *>(BOARD_GUARD) = (BYTE)(random() & 0xfe);
        return;
    }
    *at<BYTE *>(BOARD_GUARD) = (BYTE)(random() | 1);
    const DWORD rows = rnd(5), teams = rnd(4);
    make_array(b, 0x5ff33c, rows);
    for (DWORD i = 0; i < rows; i++)
        D(P(b, 4), i * 4) = ptrv(make_row(0x1c));
    make_array(b + 0x10, 0x5ee138, rows);
    for (DWORD i = 0; i < rows; i++)
        D(P(b, 0x14), i * 4) = rows - 1 - i;
    make_array(b + 0x20, 0x5ff330, teams);
    for (DWORD i = 0; i < teams; i++)
        D(P(b, 0x24), i * 4) = ptrv(make_row(8));
    b[0x30] = (BYTE)rnd(2);
    D(b, 0x34) = rnd(4) ? rnd(4) : 0xc8;
    D(b, 0x38) = rnd(30);
    D(b, 0x3c) = rnd(30);
}
// The board's state (texts by content), then its rows and arrays freed; the values also kept for the
// report of a difference
DWORD g_dbg[2][8192];
int g_ndbg[2];
void dst(DWORD v)
{
    if (g_ndbg[g_side] < 8192)
        g_dbg[g_side][g_ndbg[g_side]] = v;
    g_ndbg[g_side]++;
    st_raw(v);
}
void finish_board()
{
    BYTE *b = at<BYTE *>(BOARD);
    dst(*at<BYTE *>(BOARD_GUARD));
    dst(g_atexit_calls);
    dst(g_atexit_function);
    if (!(*at<BYTE *>(BOARD_GUARD) & 1)) {
        st_bytes(b, 0x40);
        return;
    }
    dst(b[0x30]);
    dst(D(b, 0x34));
    dst(D(b, 0x38));
    dst(D(b, 0x3c));
    for (DWORD a = 0; a < 0x30; a += 0x10) {
        dst(D(b, a));
        dst(D(b, a + 8));
        dst(D(b, a + 0xc));
        for (DWORD i = 0; i < D(b, a + 8); i++) {
            const DWORD e = D(P(b, a + 4), i * 4);
            if (a == 0x10 || !e) {
                dst(e);
                continue;
            }
            BYTE *r = at<BYTE *>(e);
            dst(fnv_text(1, reinterpret_cast<ZString *>(r)->c_str()));
            if (a == 0)
                for (DWORD k = 4; k < 0x1c; k += 4)
                    dst(D(r, k));
            else
                dst(D(r, 4));
            at<void(THISCALL *)(void *)>(0x4defd0)(r);
            heap_free(r);
        }
        if (P(b, a + 4) && D(b, a + 0xc))
            pool_get_free(P(b, a + 4), D(b, a + 0xc) * 4);
    }
}

enum BoardFn {
    B_ROW_NAME, B_ROW_FLAG, B_ROW_SET, B_ROW_PING, B_TEAM_NAME, B_TEAM_SCORE, B_GET, B_DESTROY, B_TEAM_COUNT, B_TEAM_AT,
    B_ROW_COUNT, B_ROW_AT, B_SORTED_AT, B_CLEAR, B_TICK, B_LIMIT, B_REBUILD, B_SORT, B_COMPARE, B_ARRAY_CTOR,
    B_ROW_CTOR, B_ROW_DTOR, B_WRITE, B_READ, B_FILL, NBOARD
};
const char *const BOARD_NAMES[NBOARD] = {
    "row SetName", "row SetFlag", "row setters", "row SetPing", "team row SetName", "team row SetScore", "Get", "Destroy",
    "SetTeamCount", "TeamAt", "SetRowCount", "RowAt", "SortedRowAt", "ClearChanged", "Tick", "the limits", "RebuildOrder",
    "Sort", "CompareRows", "array constructors", "row constructor", "row deleting destructor",
    "Write", "Read", "the fill from the net game"};

void run_board(BoardFn f)
{
    World &w = g_w;
    invcheck::arena_on(g_side, 0x7b);
    setup(0);
    for (int i = 0; i < 3; i++) {
        g_players[i] = static_cast<BYTE *>(arena(0x120));
        for (DWORD k = 0; k < 0x118; k += 4)
            D(g_players[i], k) = rnd(3) ? rnd(60) : random();
        D(g_players[i], 0x1c) = ptrv(&g_names[i]);   // the name (0x5b7aa0)
    }
    for (int i = 0; i < 4; i++) {
        g_teams[i] = static_cast<BYTE *>(arena(0x10));
        D(g_teams[i], 0) = rnd(4);
        D(g_teams[i], 4) = rnd(20);
    }
    for (DWORD &e : g_entities)
        e = rnd(4) ? ptrv(w.entities[rnd(3)]) : 0;
    w.type->vt[31] = ptrv((void *)&f_time);
    w.type->vt[32] = ptrv((void *)&f_set_time);
    w.net_game->ret[8] = rnd(4);   // +0x28 of the net game: its team count (0x566cf0)
    // +8 of the net game: its settings (+0x10 the time limit, +0x14 the frag limit) or none
    w.net_game->ret[0] = rnd(4) ? ptrv(arena(0x20)) : 0;
    if (w.net_game->ret[0]) {
        D(at<void *>(w.net_game->ret[0]), 0x10) = rnd(30);
        D(at<void *>(w.net_game->ret[0]), 0x14) = rnd(30);
    }
    w.net_layer->vt[12] = ptrv((void *)&f_server);
    w.net_layer->vt[13] = ptrv((void *)&f_layer13);
    w.net_layer->vt[22] = ptrv((void *)&f_ping);
    w.app->vt[1] = ptrv((void *)&f_net_layer);
    w.other->vt[11] = ptrv((void *)&f_count);
    w.other->vt[13] = ptrv((void *)&f_connection);
    memcpy(g_saved_board, at<void *>(BOARD), 0x40);
    const BYTE saved_guard = *at<BYTE *>(BOARD_GUARD);
    g_atexit_calls = 0;
    g_atexit_function = 0;
    for (int i = 0; i < NBOARD_STANDS; i++)
        patch_jump(g_board_patches[i], BOARD_STANDS[i].address, BOARD_STANDS[i].fn);
    const DWORD saved_world = *at<DWORD *>(0x6c2080), saved_app = *at<DWORD *>(0x713f14);
    *at<DWORD *>(0x6c2080) = ptrv(w.world);
    *at<DWORD *>(0x713f14) = ptrv(w.app);
    const DWORD saved_cat[2] = {*at<DWORD *>(0x714460), *at<DWORD *>(0x714454)};
    *at<BYTE *>(0x714460) = (BYTE)random();
    setup_board();
    BYTE *b = at<BYTE *>(BOARD);
    const bool made = *at<BYTE *>(BOARD_GUARD) & 1;
    Fake *packet = new_fake(0x60);
    DWORD r = 0;
    BYTE *row = made && D(b, 8) && rnd(4) ? P(P(b, 4), rnd(D(b, 8)) * 4) : nullptr;
    BYTE *team = made && D(b, 0x28) && rnd(4) ? P(P(b, 0x24), rnd(D(b, 0x28)) * 4) : nullptr;
    BYTE *loose = nullptr;
    if (!row || !team) {   // a row outside the board (freed below)
        loose = make_row(0x1c);
        if (!row)
            row = loose;
        if (!team)
            team = loose;
    }
    switch (f) {
    case B_ROW_NAME: case B_TEAM_NAME: {
        BYTE *t = f == B_ROW_NAME ? row : team;
        ZString *v = rnd(3) ? &g_names[rnd(4)] : reinterpret_cast<ZString *>(t);
        at<T1>(f == B_ROW_NAME ? 0x5663f0 : 0x566860)(t, ptrv(v));
        break;
    }
    case B_ROW_FLAG: at<T1>(0x5664c0)(row, rnd(2) ? row[4] : random()); break;
    case B_ROW_SET: {
        const DWORD fns[4] = {0x566630, 0x566580, 0x5666e0, 0x566790}, offs[4] = {8, 0xc, 0x10, 0x14};
        const int k = (int)rnd(4);
        at<T1>(fns[k])(row, rnd(2) ? D(row, offs[k]) : random());
        break;
    }
    case B_ROW_PING: at<T1>(0x566840)(row, rnd(2) ? D(row, 0x18) : random()); break;
    case B_TEAM_SCORE: at<T1>(0x566920)(team, rnd(2) ? D(team, 4) : random()); break;
    case B_GET: r = at<DWORD(__cdecl *)()>(0x566a80)(); break;
    case B_DESTROY:
        if (made)
            at<void(__cdecl *)()>(0x566b10)();
        break;
    case B_TEAM_COUNT: if (made) at<T1>(0x566c20)(b, rnd(6)); break;
    case B_TEAM_AT: if (made && D(b, 0x28)) r = at<T1>(0x566d00)(b, rnd(D(b, 0x28))); break;
    case B_ROW_COUNT: if (made) at<T1>(0x566d10)(b, rnd(7)); break;
    case B_ROW_AT: if (made && D(b, 8)) r = at<T1>(0x566e40)(b, rnd(D(b, 8))); break;
    case B_SORTED_AT: if (made && D(b, 0x18)) r = at<T1>(0x566e50)(b, rnd(D(b, 0x18))); break;
    case B_CLEAR: if (made) at<T0>(0x566e70)(b); break;
    case B_TICK: if (made) r = at<T0>(0x566e80)(b) & 0xff; break;
    case B_LIMIT: if (made) at<T1>(rnd(2) ? 0x567530 : 0x567560)(b, rnd(2) ? D(b, 0x38) : rnd(40)); break;
    case B_REBUILD: if (made) at<T0>(0x5673d0)(b); break;
    case B_SORT: if (made) at<T0>(0x567470)(b); break;
    case B_COMPARE:
        if (made && D(b, 8)) {
            const DWORD ia = rnd(D(b, 8)), ib = rnd(D(b, 8));
            r = at<DWORD(__cdecl *)(const DWORD *, const DWORD *)>(0x567280)(&ia, &ib);
        }
        break;
    case B_ARRAY_CTOR: {
        const DWORD fns[4] = {0x567370, 0x567390, 0x5673b0, 0x567590};
        BYTE *a = static_cast<BYTE *>(arena(0x10));
        for (DWORD k = 0; k < 0x10; k += 4)
            D(a, k) = random();
        r = at<T0>(fns[rnd(4)])(a) == ptrv(a);
        st_bytes(a, 0x10);
        break;
    }
    case B_ROW_CTOR: {
        BYTE *a = static_cast<BYTE *>(arena(0x10));
        r = at<T0>(0x567270)(a) == ptrv(a);
        st_zstring(reinterpret_cast<ZString *>(a));
        at<void(THISCALL *)(void *)>(0x4defd0)(a);
        break;
    }
    case B_ROW_DTOR: {
        BYTE *a = make_row(0x1c);
        r = at<T1>(0x5675b0)(a, rnd(2)) == ptrv(a);
        break;
    }
    case B_WRITE: if (made) at<T1>(0x566ea0)(b, ptrv(packet)); break;
    case B_READ: if (made) r = at<T1>(0x566fd0)(b, ptrv(packet)); break;
    case B_FILL: at<T0>(0x5bae00)(w.other); break;
    default: break;
    }
    if (loose) {
        dst(fnv_text(1, reinterpret_cast<ZString *>(loose)->c_str()));
        for (DWORD k = 4; k < 0x1c; k += 4)
            dst(D(loose, k));
        at<void(THISCALL *)(void *)>(0x4defd0)(loose);
    }
    finish_board();
    dst(norm(r));
    *at<DWORD *>(0x714460) = saved_cat[0];
    *at<DWORD *>(0x714454) = saved_cat[1];
    *at<DWORD *>(0x713f14) = saved_app;
    *at<DWORD *>(0x6c2080) = saved_world;
    for (int i = NBOARD_STANDS - 1; i >= 0; i--)
        unpatch(g_board_patches[i]);
    memcpy(at<void *>(BOARD), g_saved_board, 0x40);
    *at<BYTE *>(BOARD_GUARD) = saved_guard;
    invcheck::arena_off();
}

}  // namespace

ZAX_CHECKS(scoreboard)
{
    (void)cls;
    (void)n;
    g_table = &hooks_scoreboard;
    if (!hook_sites_fit("scoreboard", hooks_scoreboard(nullptr), MAX_SITES))
        return;
    if (!invcheck::arena_setup()) {
        log("Check scoreboard: no arena");
        return;
    }
    g_character = at<CClassInfo *(__cdecl *)()>(0x49a930)();
    g_definition = at<CClassInfo *(__cdecl *)()>(0x5b5f10)();
    g_object = at<CClassInfo *(__cdecl *)()>(0x4163b0)();
    for (int i = 0; i < 4; i++)
        original<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(&g_names[i], NAMES2[i]);
    for (int f = 0; f < NBOARD; f++) {
        log("  %s", BOARD_NAMES[f]);   // the last line before a case that ends the game
        char what[64];
        wsprintfA(what, "scoreboard: %s", BOARD_NAMES[f]);
        Tally t{what};
        const int cases = scaled(f == B_READ || f == B_FILL || f == B_WRITE ? 200 : 100);
        for (int k = 0; k < cases; k++) {
            g_ndbg[0] = g_ndbg[1] = 0;
            const bool same = both_sides(0x400000u + 0x10000u * (unsigned)f + (unsigned)k * 7919u, [&] { run_board((BoardFn)f); });
            char d[64];
            wsprintfA(d, "[case %d]", k);
            t.check(same, "", d);
            if (!same && t.mismatches <= 3) {
                log_first_difference();
                log("    board values %d / %d, events %d / %d", g_ndbg[0], g_ndbg[1], g_nlog[0], g_nlog[1]);
                for (int i = 0; i < g_ndbg[0] && i < g_ndbg[1] && i < 8192; i++)
                    if (g_dbg[0][i] != g_dbg[1][i]) {
                        log("    board value %d: %x / %x (of %d / %d)", i, g_dbg[0][i], g_dbg[1][i], g_ndbg[0], g_ndbg[1]);
                        break;
                    }
            }
        }
        t.report();
    }
    for (int i = 0; i < 4; i++)
        original<void(THISCALL *)(ZString *)>(0x4defd0)(&g_names[i]);
}

}  // namespace zax
