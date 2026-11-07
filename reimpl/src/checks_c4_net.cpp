// Checks of batch C4's multiplayer functions (c4_net.cpp) against the original, in the game thread
// (machinery: checks_net_common.h; side 0 with the module's hooks out, side 1 through the
// reimplementation; values logged and hashed through norm()).
// The world (0x6c2080), the application (0x713f14) and the error reporter (0x6c02cc) are swapped for fakes
// during a case; the net layer, the net game, the sessions and the parts are fakes whose slots record their
// calls. Every exe function the code calls is a recording stand-in for the case: the packet's reads write
// values of the case's seed into their outputs with the width of the real read (byte, word, dword, 16 bytes),
// the layer, sound, explosion, part, control, player, menu, CD key, chat and damage functions record their
// arguments and give values of the case. The stand-ins pass the uninitialised upper bytes of the
// original's stack slots (the explosion's bool, the sound's channel) only by their low byte.
#include "checks_net_common.h"
#include "init_list.h"

namespace zax {

int hooks_c4_net(Hook *out);

namespace {

using namespace netcheck;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }

CodePatch g_p[48];
int g_np;
void stand_in(DWORD a, void *f) { patch_jump(g_p[g_np++], a, f); }
void stand_ins_off()
{
    while (g_np)
        unpatch(g_p[--g_np]);
}
// A value of the case for a stand-in's result (the same sequence on both sides)
DWORD val(DWORD a) { return (a * 2654435761u) ^ (DWORD)(g_nlog[g_side] * 0x10001u) ^ g_seed; }

// ---- Generic recording stand-ins (thiscall/fastcall/cdecl with N dword arguments) -----------------------

template <DWORD A> DWORD THISCALL t0(void *s) { ev(A, ptrv(s)); return val(A); }
template <DWORD A> DWORD THISCALL t1(void *s, DWORD a) { ev(A, ptrv(s), a); return val(A); }
template <DWORD A> DWORD THISCALL t2(void *s, DWORD a, DWORD b) { ev(A, ptrv(s), a, b); return val(A); }
template <DWORD A> DWORD THISCALL t3(void *s, DWORD a, DWORD b, DWORD c) { ev(A, ptrv(s), a, b); ev(A + 1, c); return val(A); }
template <DWORD A> DWORD THISCALL t5(void *s, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
    ev(A, ptrv(s), a, b);
    ev(A + 1, c, d, e);
    return val(A);
}
template <DWORD A> DWORD __cdecl c0() { ev(A); return val(A); }
template <DWORD A> DWORD __fastcall f1(DWORD c) { ev(A, c); return val(A); }

// ---- The packet's reads -----------------------------------------------------------------------------------

DWORD THISCALL r_byte(void *p, BYTE *out) { *out = (BYTE)val(1); ev(0x10, ptrv(p), *out); return val(0x10); }
DWORD THISCALL r_16(void *p, DWORD *out) { for (int i = 0; i < 4; i++) out[i] = val(0x11 + i); ev(0x11, ptrv(p), out[0]); return val(0x11); }
DWORD THISCALL r_word(void *p, WORD *out) { *out = (WORD)val(2); ev(0x12, ptrv(p), *out); return val(0x12); }
DWORD THISCALL r_dword(void *p, DWORD *out) { *out = val(3); ev(0x13, ptrv(p), *out); return val(0x13); }
DWORD THISCALL r_bool(void *p, BYTE *out) { *out = (BYTE)(val(4) & 1); ev(0x14, ptrv(p), *out); return val(0x14); }
DWORD THISCALL r_dword_token(void *p, DWORD *out, DWORD table) { *out = val(5) & 0xff; ev(0x15, ptrv(p), table, *out); return val(0x15); }
DWORD THISCALL r_word_token(void *p, WORD *out, DWORD table) { *out = (WORD)(val(6) & 0x3ff); ev(0x16, ptrv(p), table, *out); return val(0x16); }
DWORD THISCALL r_position(void *layer, void *p, float *pos, BYTE *ok)
{
    pos[0] = (float)(val(7) & 0xfff);
    pos[1] = (float)(val(8) & 0xfff);
    if (ok)
        *ok = (BYTE)(val(9) & 1);
    ev(0x17, ptrv(layer), ptrv(p), ptrv(ok) != 0);
    return val(0x17);
}
DWORD THISCALL s_play_sound(void *layer, DWORD sound, const float *pos, DWORD mode, DWORD keep, DWORD channel)
{
    ev(0x18, ptrv(layer), sound, pos ? *(const DWORD *)pos ^ ((const DWORD *)pos)[1] : 0xdead);
    ev(0x19, mode, keep, channel & 0xff);
    return val(0x18);
}
DWORD THISCALL s_explosion(void *w, void *layer, DWORD token, DWORD x, DWORD y, int word, DWORD flag, DWORD extra)
{
    ev(0x1a, ptrv(w), ptrv(layer), token);
    ev(0x1b, x ^ y, (DWORD)word, (flag & 0xff) ^ (extra << 8));
    return val(0x1a);
}
DWORD __fastcall s_table_add(void *table, DWORD id, DWORD channel, DWORD lookup)
{
    ev(0x1c, ptrv(table), id, (channel & 0xff) ^ (lookup << 8));
    return val(0x1c);
}
DWORD __fastcall s_ini(DWORD a, DWORD b, const char *name, DWORD def)
{
    ev(0x1d, a ^ b, fnv_text(2166136261u, name), def);
    return val(0x1d) & 1;
}

// ---- Fakes ------------------------------------------------------------------------------------------------

Fake *g_world, *g_net, *g_game, *g_app, *g_reporter, *g_part, *g_session, *g_sender, *g_control, *g_player, *g_layer_obj;
BYTE *g_layer;   // the current layer (null in some cases)
DWORD g_saved_world, g_saved_app, g_saved_reporter;

void *THISCALL s_current_layer(void *w) { ev(0x20, ptrv(w)); return g_layer; }
void *THISCALL s_player_at(void *g, DWORD i) { ev(0x21, ptrv(g), i); return g_player->mem; }
void *THISCALL s_find_id(void *list, void *key, DWORD id)
{
    ev(0x22, ptrv(list), D(key, 0) ^ D(key, 4), id);
    return val(0x22) & 1 ? g_part : nullptr;
}

// Slots beyond the fakes' 64: a vtable of 128 in the arena (the first 64 copied), recorders of our own
DWORD THISCALL big_slot1(Fake *f, DWORD a) { ev(0x200, f->id, a); return 0x81; }
DWORD THISCALL big_slot2(Fake *f, DWORD a, DWORD b) { ev(0x201, f->id, a, b); return (g_seed & 1) ? 0 : 0x64; }
void big_vt(Fake *f, int slot, void *fn)
{
    if (f->vftable == ptrv(f->vt)) {
        DWORD *vt = static_cast<DWORD *>(arena(128 * 4));
        memcpy(vt, f->vt, sizeof f->vt);
        f->vftable = ptrv(vt);
    }
    reinterpret_cast<DWORD *>(static_cast<uintptr_t>(f->vftable))[slot] = ptrv(fn);
}

void make_fakes()
{
    g_world = new_fake(1);
    g_net = new_fake(2);
    g_game = new_fake(3);
    g_app = new_fake(4);
    g_reporter = new_fake(5);
    g_part = new_fake(6);
    g_session = new_fake(7);
    g_sender = new_fake(8);
    g_control = new_fake(9);
    g_player = new_fake(10);
    g_layer_obj = new_fake(11);
    set0<1>(g_world, ptrv(g_game));
    set0<44>(g_world, ptrv(g_net));
    set1<49>(g_world, 0x49);
    set1<50>(g_world, 0x50);
    big_vt(g_world, 81, (void *)&big_slot1);
    set0<28>(g_net, 3);
    set1<23>(g_net, ptrv(g_session->mem));
    set0<29>(g_net, ptrv(g_sender->mem));
    set1<12>(g_net, ptrv(g_net->mem + 0x80));
    set1<13>(g_net, ptrv(g_net->mem + 0x90));
    set2<14>(g_game, 0x14);
    set1<60>(g_part, 0x60);
    set1<23>(g_reporter, 0x23);
    set2<23>(g_reporter, 0x23);
    D(g_sender->mem, 0xe8) = ptrv(g_control->mem);
    D(g_player->mem, 0x34) = rnd(7);
    D(g_layer_obj->mem, 0x7c) = 0x7c7c7c7c;
    g_layer = rnd(4) ? g_layer_obj->mem : nullptr;
}
void swap_in()
{
    g_saved_world = *at<DWORD *>(0x6c2080);
    g_saved_app = *at<DWORD *>(0x713f14);
    g_saved_reporter = *at<DWORD *>(0x6c02cc);
    *at<DWORD *>(0x6c2080) = ptrv(g_world);
    *at<DWORD *>(0x713f14) = ptrv(g_app);
    *at<DWORD *>(0x6c02cc) = ptrv(g_reporter);
}
void swap_out()
{
    stand_ins_off();
    *at<DWORD *>(0x6c2080) = g_saved_world;
    *at<DWORD *>(0x713f14) = g_saved_app;
    *at<DWORD *>(0x6c02cc) = g_saved_reporter;
}
void packet_stand_ins()
{
    stand_in(0x520490, (void *)&r_byte);
    stand_in(0x5204c0, (void *)&r_16);
    stand_in(0x520510, (void *)&r_word);
    stand_in(0x520550, (void *)&r_dword);
    stand_in(0x520580, (void *)&r_bool);
    stand_in(0x5205d0, (void *)&r_dword_token);
    stand_in(0x520670, (void *)&r_word_token);
    stand_in(0x4eaf80, (void *)&r_position);
    stand_in(0x4f1050, (void *)&s_current_layer);
    stand_in(0x51f440, (void *)&s_player_at);
}

// ---- The block readers ---------------------------------------------------------------------------------------

const DWORD READERS[] = {0x4f7820, 0x4f78b0, 0x4f7920, 0x4f79b0, 0x4f7b90, 0x4f8780, 0x4f8860,
                         0x4f89a0, 0x4f8b00, 0x4f8d40, 0x4f9320, 0x4f93a0, 0x49cd30};
void *g_made;
DWORD THISCALL s_beam_ctor(void *o) { ev(0x30, 1); return ptrv(o); }
DWORD THISCALL s_init_fields(void *o) { ev(0x31, D(o, 0)); return val(0x31); }
DWORD THISCALL s_beam_read(void *o, void *p) { ev(0x32, D(o, 0x134) ^ D(o, 0x138), ptrv(p)); return val(0x32); }
DWORD THISCALL s_set_last(void *o) { g_made = o; ev(0x33, 1); return val(0x33); }
DWORD THISCALL s_set_vector(void *c, const DWORD *v) { ev(0x34, ptrv(c), v[0], v[1]); return val(0x34); }

void reader_case(int k)
{
    make_fakes();
    const DWORD fn = READERS[k % (sizeof READERS / sizeof READERS[0])];
    Fake *reader = new_fake(20);
    Fake *packet = new_fake(21);
    swap_in();
    packet_stand_ins();
    stand_in(0x540640, (void *)&t2<0x40>);   // CPlasmaTileMap_ReleaseRect
    stand_in(0x57a9a0, (void *)&s_find_id);
    stand_in(0x4f6320, (void *)&s_explosion);
    stand_in(0x4eaca0, (void *)&s_play_sound);
    stand_in(0x4f91a0, (void *)&s_table_add);
    stand_in(0x470030, (void *)&s_set_vector);
    stand_in(0x46ee50, (void *)&t1<0x42>);   // BitSet::Read
    stand_in(0x4d2460, (void *)&c0<0x43>);   // Platform_GetTickCount
    stand_in(0x4a4670, (void *)&s_ini);
    stand_in(0x57e890, (void *)&t1<0x44>);   // CSoundList::PlayIndex
    stand_in(0x5ac330, (void *)&t1<0x45>);   // BlockReader_Send
    stand_in(0x49bcb0, (void *)&s_beam_ctor);
    stand_in(0x4173c0, (void *)&s_init_fields);
    stand_in(0x4fd5c0, (void *)&s_set_last);
    // the made object's slot 27 (its real vtable 0x5f3aa4)
    stand_in(*at<DWORD *>(0x5f3aa4 + 27 * 4), (void *)&s_beam_read);
    g_made = nullptr;
    const DWORD r = at<DWORD(THISCALL *)(void *, void *)>(fn)(reader, packet);
    swap_out();
    // without a layer the full sound reader returns the channel's dword, of which the read set only the low
    // byte (the original's upper bytes are stack garbage)
    st(fn == 0x4f8d40 && !g_layer ? r & 0xff : r);
    st_dwords(g_session->mem, 0x100);
    st_dwords(g_sender->mem, 0x100);
    st_dwords(g_control->mem, 0x100);
    st_dwords(g_player->mem, 0x100);
    st_dwords(reader->mem, 0x40);
    if (g_made) {
        st(D(g_made, 0));   // the stand-ins write nothing else (the rest is the pool block's old bytes)
        st(D(g_made, 0x134));
        st(D(g_made, 0x138));
        pool_free(g_made, 0x13c);
    }
}

// ---- NetLocal and ZaxNetGame ----------------------------------------------------------------------------------

DWORD THISCALL s_can_read(void *p) { ev(0x50, ptrv(p)); return val(0x50); }
void *g_menu_made;
void *THISCALL s_main_menu(void *o, void *w) { g_menu_made = o; ev(0x51, ptrv(w)); return o; }
DWORD THISCALL s_popup_ctor(void *, DWORD a, void *text) { ev(0x52, a, fnv_text(2166136261u, (*(ZStringBuffer **)text)->text)); return val(0x52); }
DWORD __fastcall s_cd_last_error(const char *s) { ev(0x53, s ? fnv_text(2166136261u, s) : 0); return val(0x53); }
DWORD THISCALL s_options_write(void *r) { ev(0x54, ptrv(r)); return val(0x54); }
void *__cdecl s_options_get() { ev(0x55); return g_part; }
void *THISCALL s_player_name(void *pl)
{
    ev(0x56, ptrv(pl));
    return g_player->mem + 0x80;   // a ZString made in the case
}
DWORD THISCALL s_send_chat(void *a, const char *text, DWORD to) { ev(0x57, ptrv(a), fnv_text(2166136261u, text), to); return val(0x57); }
void *THISCALL s_char_slot(void *list, DWORD i) { ev(0x58, ptrv(list), i); return g_player->mem + 0x40; }
DWORD g_kind_al;
DWORD THISCALL s_is_kind_of(void *o, void *c) { ev(0x59, ptrv(o), ptrv(c)); return (val(0x59) & ~0xffu) | g_kind_al; }
long double THISCALL s_hit_points(void *c) { ev(0x5a, ptrv(c)); return (long double)(val(0x5a) & 0xffff) * 0.25L; }
DWORD THISCALL s_find(void *l, const char *name, DWORD hint) { ev(0x5b, ptrv(l), fnv_text(2166136261u, name), hint); return 4; }
DWORD THISCALL s_damage(void *c, float hp, DWORD type, void *by)
{
    DWORD b;
    memcpy(&b, &hp, 4);
    ev(0x5c, ptrv(c), b, type ^ ptrv(by));
    return val(0x5c);
}

void netlocal_case(int k)
{
    make_fakes();
    Fake *self = new_fake(30);
    set1<23>(self, k & 4 ? 0 : ptrv(g_session->mem));
    set1<12>(self, ptrv(self->mem));
    set1<13>(self, ptrv(self->mem + 0x10));
    D(self, 4) = ptrv(g_game);   // NetLocal's +4: the net game (overlaps id of the fake, set after)
    swap_in();
    stand_in(0x51f440, (void *)&s_player_at);
    stand_in(0x5203d0, (void *)&t2<0x60>);   // NetPacket_WriteBytes
    stand_in(0x520f50, (void *)&s_can_read);
    const DWORD saved_local = *at<DWORD *>(0x6c2938);
    *at<DWORD *>(0x6c2938) = 7;
    DWORD r[11];
    r[0] = at<DWORD(THISCALL *)(void *)>(0x4f7e50)(self);
    r[1] = at<DWORD(THISCALL *)(void *)>(0x4f7e60)(self);
    r[2] = at<DWORD(THISCALL *)(void *, DWORD)>(0x4f7e70)(self, (DWORD)k);
    r[3] = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4f7e80)(self, k & 1 ? 0 : 2, 0x1234, (DWORD)k);
    r[4] = at<DWORD(THISCALL *)(void *)>(0x4f7f00)(self);
    r[5] = at<DWORD(THISCALL *)(void *)>(0x4f7f30)(self);
    r[6] = at<DWORD(THISCALL *)(void *, DWORD)>(0x4f7f40)(self, 1);
    r[7] = at<DWORD(THISCALL *)(void *, DWORD)>(0x4f7f50)(self, 2);
    r[8] = at<DWORD(THISCALL *)(void *, DWORD)>(0x4f7f90)(self, 3);
    r[9] = at<DWORD(THISCALL *)(void *, DWORD)>(0x4f7fa0)(self, 4);
    r[10] = 0;
    *at<DWORD *>(0x6c2938) = saved_local;
    swap_out();
    for (DWORD v : r)
        st(v);
}

void netgame_case(int k)
{
    make_fakes();
    Fake *self = new_fake(31);
    swap_in();
    g_kind_al = k & 1;
    g_menu_made = nullptr;
    // ZString of the player's name at +0x80 of the player fake
    at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(g_player->mem + 0x80, "Player Two");
    D(g_player->mem, 0x40) = k & 2 ? ptrv(g_part) : 0;
    stand_in(0x4f7ed0, (void *)&t2<0x70>);   // NetLocal_StopSounds
    stand_in(0x50db70, (void *)&c0<0x71>);   // WindowMissionOver_Close
    stand_in(0x4de290, (void *)&s_popup_ctor);
    stand_in(0x4de790, (void *)&t2<0x72>);   // PopupMenu::AddItem
    stand_in(0x4de740, (void *)&t0<0x73>);   // PopupMenu::Run
    stand_in(0x4de5e0, (void *)&t0<0x74>);   // PopupMenu::Dtor
    stand_in(0x46b500, (void *)&f1<0x75>);   // CdKey_Set
    stand_in(0x46b520, (void *)&s_cd_last_error);
    stand_in(0x4cdf30, (void *)&t0<0x76>);   // App_Window
    stand_in(0x5be4a0, (void *)&s_main_menu);
    stand_in(0x4f7ee0, (void *)&t0<0x77>);   // Layer_SetDrawDirect
    stand_in(0x5a32b0, (void *)&t1<0x78>);   // CZaxWorld::SetGameType
    stand_in(0x5abe30, (void *)&s_options_get);
    stand_in(0x5abfe0, (void *)&s_options_write);
    stand_in(0x51f440, (void *)&s_player_at);
    stand_in(0x5b7aa0, (void *)&s_player_name);
    stand_in(0x59b260, (void *)&s_send_chat);
    stand_in(0x4186e0, (void *)&t1<0x79>);   // the base slot
    stand_in(0x4ef8e0, (void *)&s_char_slot);
    stand_in(0x4ef950, (void *)&t1<0x7a>);   // CCharacterListHolder::RemoveAt
    stand_in(0x416790, (void *)&s_is_kind_of);
    stand_in(0x4fbd00, (void *)&t0<0x7b>);   // LayerPart_DeleteAttached
    stand_in(0x523df0, (void *)&s_find);
    stand_in(0x48d030, (void *)&s_hit_points);
    stand_in(0x4f7e40, (void *)&t1<0x7c>);   // NetLocal_SetLocalPlayer
    big_vt(g_part, 99, (void *)&s_damage);
    static const char *const REASONS[] = {"CD KEY", "cd key", "Kicked", ""};
    static const char *const MESSAGES[] = {"Bad key", "", nullptr, "Server full"};
    DWORD r[5];
    r[0] = at<DWORD(THISCALL *)(void *, const char *, const char *)>(0x5ba830)(self, REASONS[k % 4], MESSAGES[(k / 4) % 4]);
    r[1] = at<DWORD(THISCALL *)(void *)>(0x5ba940)(self);
    r[2] = at<DWORD(THISCALL *)(void *)>(0x5baba0)(self);
    r[3] = at<DWORD(THISCALL *)(void *, DWORD)>(0x5baaa0)(self, (DWORD)(k % 3));
    r[4] = at<DWORD(THISCALL *)(void *, DWORD)>(0x5babb0)(self, (DWORD)k);
    swap_out();
    at<DWORD(THISCALL *)(void *)>(0x4defd0)(g_player->mem + 0x80);
    if (g_menu_made)
        pool_free(g_menu_made, 0x158);
    for (int i = 1; i < 5; i++)   // r[0]: the ZString destructor's eax (a pool address)
        st(r[i]);
}

// ---- GameSpy, CZaxWorld's slot 87, the save game's callbacks, the play-sound reader's exit ------------------------

DWORD g_text_k;
DWORD THISCALL s_key_text(void *game, void *z)
{
    static const char *const T[] = {"", "Zax", "A rather long server name for the list", "12345678"};
    ev(0x80, ptrv(game));
    at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, T[g_text_k % 4]);
    return val(0x80);
}
void misc_case(int k)
{
    make_fakes();
    swap_in();
    // the GameSpy texts into buffers of sizes 1..12
    for (DWORD fn : {0x51ef40u, 0x51efd0u, 0x51f060u, 0x51f0f0u}) {
        static const DWORD JUMP[] = {0x51f6c0, 0x51f6d0, 0x51f6e0, 0x51f6f0};
        const int i = fn == 0x51ef40 ? 0 : fn == 0x51efd0 ? 1 : fn == 0x51f060 ? 2 : 3;
        stand_in(JUMP[i], (void *)&s_key_text);
        g_text_k = (DWORD)k + (DWORD)i;
        char out[64];
        memset(out, 0x5a, sizeof out);
        const DWORD size = 1 + ((DWORD)k * 5 + (DWORD)i) % 12;
        at<DWORD(__cdecl *)(char *, DWORD, void *)>(fn)(out, size, g_game);
        st_bytes(out, sizeof out);
        stand_ins_off();
    }
    // CZaxWorld's slot 87
    stand_in(0x4f5660, (void *)&t2<0x81>);   // World::FixPart
    set1<58>(g_part, 0x58);   // before the big vtable copies the 64
    big_vt(g_part, 64, (void *)&big_slot2);
    const DWORD saved = *at<DWORD *>(0x6c29a8);
    *at<DWORD *>(0x6c29a8) = 0x29a8;
    st(at<DWORD(THISCALL *)(void *, DWORD, void *)>(0x5a3440)(g_app, 0x77, g_part));
    *at<DWORD *>(0x6c29a8) = saved;
    stand_ins_off();
    // 0x4f86b0 and 0x4f8720
    g_kind_al = k & 1;
    stand_in(0x416790, (void *)&s_is_kind_of);
    stand_in(0x41e360, (void *)&t0<0x82>);   // CPropertyList::Save
    stand_in(0x419ea0, (void *)&f1<0x83>);
    stand_in(0x419f00, (void *)&f1<0x84>);
    stand_in(0x419f60, (void *)&f1<0x85>);
    BYTE out = 0x33;
    D(g_part->mem, 0) = 0;
    st(at<DWORD(__fastcall *)(void *, BYTE *)>(0x4f86b0)(g_part, &out));
    st(out);
    st(at<DWORD(__cdecl *)()>(0x4f8720)());
    swap_out();
}

// The play-sound reader's exit on its global with arrays of our own (saved and put back)
void exit_case(int k)
{
    BYTE saved[0x28];
    memcpy(saved, at<void *>(0x6c2960), 0x28);
    BYTE *g = at<BYTE *>(0x6c2960);
    D(g, 0xc) = k & 1 ? ptrv(arena(16)) : 0;
    D(g, 0x10) = 2;
    D(g, 0x14) = 4;
    D(g, 0x1c) = k & 2 ? ptrv(arena(16)) : 0;
    D(g, 0x20) = 3;
    D(g, 0x24) = 5;
    // operator delete on our arena blocks: recorded
    stand_in(0x5d0320, (void *)&c0<0x90>);
    const DWORD r = at<DWORD(__cdecl *)()>(0x4f8c50)();
    stand_ins_off();
    st(r);
    st_dwords(g, 0x28);
    memcpy(at<void *>(0x6c2960), saved, 0x28);
}

void run(const char *what, int n, unsigned seed0, void (*f)(int))
{
    run_cases(what, scaled(n), seed0, [f](int k) { f(k); });
}

}  // namespace

ZAX_CHECKS(c4_net)
{
    (void)cls;
    (void)n;
    g_table = hooks_c4_net;
    run("c4 net: block readers' slot 2", 13 * 24, 0x4f7820u, reader_case);
    run("c4 net: the local net layer's slots", 64, 0x4f7e50u, netlocal_case);
    run("c4 net: ZaxNetGame's slots", 48, 0x5ba830u, netgame_case);
    run("c4 net: GameSpy texts, slot 87, save game callbacks", 48, 0x51ef40u, misc_case);
    run("c4 net: the play-sound reader's exit", 8, 0x4f8c50u, exit_case);
}

}  // namespace zax
