// Checks of the game's own actions 0x5a4220-0x5a7fff (zax_actions2.cpp) against the original, in the
// game thread (machinery: checks_inventory_common.h).
//  - Classes: Create (pool block filled with a pattern, the C heap from the arena), CreateArray with 0-3
//    elements, the deleting destructors, both class getters of the 13 classes, RegisterFields into a
//    fake class info and with the class variable 0, the registered descriptors (real data);
//    CDialogActionBase's (0x5a4620) and CGiveInventoryItemActionBase's (0x5a7200) deleting destructors
//    on base-level objects of their own size, CLayerPartAI's constructor 0x5a71e0.
//  - Behaviour: real actions made by the original Create (their fields from a seed, strings assigned,
//    owned children fakes) on a fake world. The application 0x713f14, the world 0x6c2080, the reporter
//    0x6c02cc, the item catalogue's, quip categories' and dialogue scripts' records and the confiscated
//    inventory's descriptor are fakes while a case runs; every function of other subsystems the slots
//    call jumps to a recorder (STANDS below); CZaxDialogAction's slot 29 (the script, 0x41d830) is a
//    recorder in a copy of the action's vtable. Real objects the drop action makes (the pile's
//    CollideTriggerAI and its actions) are compared through the log (their vtables and fields) and
//    deleted after the case. Compared: eax (al where only al is defined), the event log, the fake
//    world, the action's bytes after the call (strings by text) and the arena.
//  - Real data: the module's actions in the running level (reached from the layer grid's parts), copies
//    made by Object_Clone with their owned children replaced by fakes, through the same slots and fakes.
#include "checks_inventory_common.h"
#include "layer_parts.h"
#include "ptr_array.h"
#include "x87.h"

WINIMPORT DWORD WINAPI GetTickCount(void);

namespace zax {

int hooks_zax_actions2(Hook *out);

namespace {

using namespace invcheck;

const ClassDef CLASSES[] = {
    {"CSetCompassTargetAction", 0xc, 0x713fb8, 0x5a4270, 0x5a42b0, 0x5a4420, {0x5a4220, 0x5a43c0}, true},
    {"CZaxDialogAction", 0x88, 0x713fbc, 0x5a4650, 0x5a4690, 0x5a4810, {0x5a45c0, 0x5a47a0}, true},
    {"CSwitchMapAction", 0x20, 0x713fc0, 0x5a5320, 0x5a5350, 0x5a54e0, {0x5a52d0, 0x5a5480}, true},
    {"CRelocateAction", 0x24, 0x713fc4, 0x5a56b0, 0x5a56f0, 0x5a5910, {0x5a5600, 0x5a5850}, true},
    {"CTeleportAction", 0x28, 0x713fc8, 0x5a5b80, 0x5a5bd0, 0x5a5e60, {0x5a5a80, 0x5a5d50}, true},
    {"CDiscoverInventoryItemAction", 0x10, 0x713fcc, 0x5a60b0, 0x5a60e0, 0x5a6220, {0x5a6060, 0x5a61c0}, true},
    {"CIsEnemyNearbyAction", 8, 0x713fd0, 0x5a65c0, 0x5a65f0, 0, {0x5a6570, 0x5a66d0}, true},
    {"CPrintAction", 0x14, 0x713fd4, 0x5a6860, 0x5a68a0, 0x5a6a20, {0x5a6810, 0x5a69c0}, true},
    {"CDropAllOreAndCrystalsAction", 0x10, 0x713fd8, 0x5a6bc0, 0x5a6bf0, 0x5a6d30, {0x5a6b70, 0x5a6cd0}, true},
    {"CDropCurrentItemOfTypeAction", 0x14, 0x713fdc, 0x5a72a0, 0x5a7300, 0x5a74b0, {0x5a7250, 0x5a7450}, true},
    {"CConfiscateEntireInventoryAction", 8, 0x713fe0, 0x5a7720, 0x5a7750, 0, {0x5a76d0, 0x5a7830}, true},
    {"CReturnConfiscatedInventoryAction", 8, 0x713fe4, 0x5a7960, 0x5a7990, 0, {0x5a7910, 0x5a7a70}, true},
    {"CDischargeWeaponThisFrameAction", 0x24, 0x713fe8, 0x5a7bb0, 0x5a7c00, 0x5a7d90, {0x5a7b60, 0x5a7d30}, true},
};
constexpr int NCLASSES = sizeof CLASSES / sizeof CLASSES[0];

using Create0 = void *(__cdecl *)();
using T1 = DWORD(THISCALL *)(void *, DWORD);
DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
void *vp(DWORD v) { return reinterpret_cast<void *>(static_cast<uintptr_t>(v)); }

// ---- The two base destructors and CLayerPartAI's constructor ---------------------------------------------

void test_base_dtors(Tally &t, unsigned seed)
{
    for (int kind = 0; kind < 3; kind++) {
        BYTE obj[2][0x78];
        DWORD r[2];
        const DWORD size = kind == 0 ? 0x78 : 0x10;
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            side_begin(side);
            arena_on(side, (BYTE)seed);
            BYTE *o = static_cast<BYTE *>(pool_alloc(size));
            for (DWORD k = 0; k < size; k += 4)
                D(o, k) = random();
            const DWORD flags = seed & 1;
            if (kind == 2) {   // CLayerPartAI's constructor, then its deleting destructor 0x41c950
                r[side] = at<DWORD(THISCALL *)(void *)>(0x5a71e0)(o) == ptrv(o);
                memcpy(obj[side], o, size);
                at<void *(THISCALL *)(void *, DWORD)>(0x41c950)(o, 1);
            } else {
                at<void *(THISCALL *)(void *)>(0x41a6b0)(o);   // CAction's constructor
                if (kind == 1)
                    at<void *(THISCALL *)(void *)>(0x4dec90)(o + 8);
                D(o, 0) = kind == 0 ? 0x603238 : 0x603664;
                at<void(THISCALL *)(void *)>(0x4173c0)(o);
                r[side] = at<T1>(kind == 0 ? 0x5a4620 : 0x5a7200)(o, flags) == ptrv(o);
                memcpy(obj[side], o, size);
                if (!flags)
                    pool_free(o, size);
            }
            arena_off();
            side_end(side);
        }
        char d[120];
        wsprintfA(d, "[kind %d seed %u: eax is this %d / %d, first difference +%x]", kind, seed, r[0], r[1], g_diff_at);
        const bool keep = kind == 2 || !(seed & 1);
        t.check(r[0] == r[1] && r[0] && (!keep || same_region(obj[0], obj[1], size)) && same_region(g_arena[0], g_arena[1], g_used[0]),
                kind == 0 ? "CDialogActionBase::DeletingDtor" : kind == 1 ? "CGiveInventoryItemActionBase::DeletingDtor" : "CLayerPartAI::Ctor", d);
    }
}

// ---- The fake world ----------------------------------------------------------------------------------

constexpr int NPART = 4, NOBJ = 8, NREC = 6, NLINES = 5;

struct Record {
    BYTE name[0x78];
    void *object;
    BYTE keep, in_use, unknown7e, loading;
};
struct FakeObject {   // parts, items, layers, holders, clones: slot 2 the class
    DWORD vftable;
    BYTE body[0x1fc];
    CClassInfo *cls;
    DWORD id;
    DWORD answer[4];
    float pos[2];
};
struct FakeScript {
    DWORD vftable;
    DWORD unused;
    DWORD lines;   // +8: the line pointers
};
struct World {
    FakeObject parts[NPART];
    FakeObject objs[NOBJ];
    FakeObject layer;
    BYTE app[0x600];
    BYTE app_data[0x40];
    BYTE world[0x300];
    FakeObject display;
    FakeObject templist;
    Record catalogue[NREC];
    Record quips[NREC];
    Record scripts[NREC];
    FakeScript script;
    DWORD script_lines[NLINES];
    DWORD reporter_vftable;
    DWORD answers[32];
    DWORD model[0x20];       // a model: +0x74 its default entity
    BYTE confiscated_field[0x10];
};
World g_world;

// The dialogue lines (CDialogLine: +8 name, +0xc text), made once
struct Line {
    DWORD vftable;
    DWORD unused;
    ZString name, text;
};
Line g_lines[NLINES];
const char *const LINE_TEXTS[NLINES][2] = {{"Zax01", "Zax: Hello there"},          {"Kiro02", "No colon here"},
                                           {"Long03", "A speaker name too long: x"}, {"Empty04", ": empty speaker"},
                                           {"Rog05", "Rogcoil:    spaces"}};

void *g_vtable[160];
void *g_app_vtable[100];
void *g_display_vtable[50];
void *g_reporter_vtable[20];
DWORD g_dialog_vtable[40];
CClassInfo *g_classes[10];
const char *const PART_NAMES[NPART] = {"Zax", "Door 1", "Fruit", "$Trigger"};

int obj_id(const void *p)
{
    const World &w = g_world;
    for (int i = 0; i < NPART; i++)
        if (p == &w.parts[i])
            return 0x10 + i;
    for (int i = 0; i < NOBJ; i++)
        if (p == &w.objs[i])
            return 0x20 + i;
    const BYTE *b = static_cast<const BYTE *>(p);
    if (b >= reinterpret_cast<const BYTE *>(&w) && b < reinterpret_cast<const BYTE *>(&w) + sizeof w)
        return 0x1000 + (int)(b - reinterpret_cast<const BYTE *>(&w));
    return p ? 0x99 : 0;
}
bool fake(const void *p)
{
    const BYTE *b = static_cast<const BYTE *>(p);
    return b >= reinterpret_cast<const BYTE *>(&g_world) && b < reinterpret_cast<const BYTE *>(&g_world) + sizeof g_world;
}
DWORD ans(int k) { return g_world.answers[k]; }
// A real object's identity for the log: its vtable and first fields
DWORD real_tag(const void *p)
{
    if (!p)
        return 0;
    if (fake(p))
        return (DWORD)obj_id(p);
    return D(const_cast<void *>(p), 0);
}
DWORD text_of(const char *t) { return t ? fnv_text(2166136261u, t) : 0x0bad; }
DWORD ztext(void *z) { return z && reinterpret_cast<ZString *>(z)->buffer ? text_of(reinterpret_cast<ZString *>(z)->c_str()) : 0x0bad; }

// Real objects made during a case (deleted afterwards)
constexpr int MAX_MADE = 16;
void *g_made_objects[MAX_MADE];
int g_nmade;
void note_made(void *p)
{
    if (!p || fake(p))
        return;
    for (int i = 0; i < g_nmade; i++)
        if (g_made_objects[i] == p)
            return;
    if (g_nmade < MAX_MADE)
        g_made_objects[g_nmade++] = p;
}

// The fakes' slots (each with the original's argument count)
DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
DWORD THISCALL f_delete(FakeObject *o, DWORD flags) { ev(0x101, (DWORD)obj_id(o), flags); return result(0x01); }
CClassInfo *THISCALL f_class(FakeObject *o) { ev(0x102, (DWORD)obj_id(o)); return o->cls; }
DWORD THISCALL f_precache(FakeObject *o) { ev(0x107, (DWORD)obj_id(o)); return result(0x07); }
DWORD THISCALL f_count(FakeObject *o) { ev(0x10b, (DWORD)obj_id(o)); return o->answer[1] % 3; }
DWORD THISCALL f_reset(FakeObject *o) { ev(0x113, (DWORD)obj_id(o)); return result(0x13); }
DWORD THISCALL f_start(FakeObject *o) { ev(0x117, (DWORD)obj_id(o)); return result(0x17); }
void *THISCALL f_item_at(FakeObject *o, DWORD i) { ev(0x11a, (DWORD)obj_id(o), i); return &g_world.objs[(i + 2) % NOBJ]; }
void *THISCALL f_inventory(FakeObject *o) { ev(0x124, (DWORD)obj_id(o)); return &g_world.objs[7]; }
DWORD THISCALL f_position(FakeObject *o, float *out)
{
    ev(0x12b, (DWORD)obj_id(o));
    out[0] = o->pos[0];
    out[1] = o->pos[1];
    return result(0x2b);
}
DWORD THISCALL f_set45(FakeObject *o, DWORD a) { ev(0x12d, (DWORD)obj_id(o), a); return result(0x2d); }
DWORD THISCALL f_find_ai(FakeObject *o, CClassInfo *c, DWORD a) { ev(0x140, (DWORD)obj_id(o), ptrv(c), a); return o->answer[2] & 1 ? 0x5e7f0000u : 0; }
DWORD THISCALL f_set_model(FakeObject *o, DWORD m) { ev(0x15d, (DWORD)obj_id(o), m); return result(0x5d); }
void *THISCALL f_take_inventory(FakeObject *o) { ev(0x17f, (DWORD)obj_id(o)); return &g_world.objs[3]; }
DWORD THISCALL f_give_inventory(FakeObject *o, void *inv) { ev(0x180, (DWORD)obj_id(o), (DWORD)obj_id(inv)); return result(0x80); }
// the application's slots
DWORD THISCALL a_warning(void *, const char *text) { ev(0x210, text_of(text)); return result(0x10); }
void *THISCALL a_players(void *) { ev(0x22c); return &g_world.objs[4]; }
DWORD THISCALL a_playing(void *) { ev(0x254); return result(ans(10) & 1); }
void *THISCALL a_save_data(void *) { ev(0x261); return ans(11) & 1 ? &g_world.objs[4] : nullptr; }
DWORD THISCALL d_height(void *) { ev(0x32d); return 0x1e0 + (ans(12) & 0x3f); }
DWORD THISCALL f_report(void *, const char *text) { ev(0x610, text_of(text)); return result(0x10); }
// CZaxDialogAction's slot 29 in the vtable copy: the line's delay +0x84 set
DWORD THISCALL f_dialog_show(BYTE *o, DWORD layer, DWORD a2, DWORD a3, DWORD a4, DWORD a5)
{
    ev(0x41d, (DWORD)obj_id(vp(layer)), a2 ^ (a3 << 1) ^ (a4 << 2), a5);
    D(o, 0x84) = float_bits((float)(ans(13) % 9) * 0.5f);
    return result(ans(14));
}

// Stand-ins
void *__fastcall s_resolve(void *layer, ZString *name, DWORD a2, DWORD a4, DWORD a3, void *caller, DWORD report, CClassInfo *cls)
{
    const DWORD h = ztext(name);
    ev(0x70a, (DWORD)obj_id(layer) ^ h, a2 ^ (a4 << 2) ^ (a3 << 4), (caller != nullptr) + report * 2 + ptrv(cls) * 4);
    const DWORD k = (h ^ ans(4)) % 5;
    return k < NPART ? &g_world.parts[k] : nullptr;
}
const char *THISCALL s_part_name(FakeObject *p) { ev(0x701, (DWORD)obj_id(p)); return PART_NAMES[(DWORD)obj_id(p) & 3]; }
void *THISCALL s_savedata_ctor(void *block) { ev(0x702); pool_free(block, 0xc); return &g_world.objs[5]; }
DWORD THISCALL s_set_custom(void *at_, void *data) { ev(0x703, (DWORD)obj_id(at_), (DWORD)obj_id(data)); return result(0x03); }
DWORD THISCALL s_set_help(void *o, const char *text) { ev(0x704, (DWORD)obj_id(o), text ? text_of(text) : 0); return result(0x04); }
DWORD THISCALL s_compass(void *p) { ev(0x705, (DWORD)obj_id(p)); return result(0x05); }
void *THISCALL s_dialog_record(void *save) { ev(0x706, (DWORD)obj_id(save)); return &g_world.objs[2]; }
DWORD THISCALL s_dialog_said(void *rec, void *action) { ev(0x707, (DWORD)obj_id(rec), real_tag(action)); return result(0x07); }
DWORD THISCALL s_queue_add(void *queue, DWORD delay, DWORD a4, DWORD a2, DWORD a3, DWORD a5, void *action)
{
    ev(0x708, (DWORD)obj_id(queue), delay, a4 ^ (a2 << 1) ^ (a3 << 2));
    ev(0x709, a5, (DWORD)obj_id(action));
    return result(0x08);
}
void *THISCALL s_clone(void *o, DWORD a, DWORD b) { ev(0x70b, real_tag(o), a, b); return &g_world.objs[6]; }
DWORD THISCALL s_execute_from(void *a, void *layer, DWORD a2, DWORD a3, DWORD a4, DWORD caller)
{
    ev(0x70c, real_tag(a), (DWORD)obj_id(layer), a2 ^ (a3 << 1) ^ (a4 << 2));
    ev(0x70d, caller < 0x10000 || fake(vp(caller)) ? caller : caller >= 0x600000 && caller < 0x700000 ? caller : real_tag(vp(caller)));
    return result(ans(15));
}
void *THISCALL s_line(void *action) { ev(0x70e, real_tag(action)); return &g_lines[ans(16) % NLINES]; }
DWORD THISCALL s_line_text(Line *line, ZString *out)
{
    ev(0x70f, (DWORD)(line - g_lines));
    return (DWORD)(uintptr_t)at<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(out, &line->text);
}
int __fastcall s_ini(DWORD a, DWORD b, const char *name, DWORD def) { ev(0x710, a ^ b, text_of(name), def); return (int)(ans(17) & 1); }
DWORD THISCALL s_sound_index(void *list, const char *name, DWORD k) { ev(0x711, ptrv(list), text_of(name), k); return ans(18) & 1 ? ans(18) % 40 : 0xffffffff; }
DWORD THISCALL s_sound_precache(void *list, DWORD i) { ev(0x712, ptrv(list), i); return result(0x12); }
DWORD THISCALL s_sound_name(void *list, DWORD i, ZString *out) { ev(0x713, ptrv(list), i, ztext(out)); return result(0x13); }
DWORD __fastcall s_speaker(const char *text) { ev(0x714, text_of(text)); return ans(19) % NREC; }
void *__cdecl s_voice_log() { ev(0x715); return &g_world.objs[3]; }
DWORD THISCALL s_voice_add(void *log, const char *text, DWORD kind, float *out)
{
    ev(0x716, (DWORD)obj_id(log), text_of(text), kind + (out ? (DWORD)obj_id(out) << 4 : 0));
    if (out)
        *out = (float)(ans(20) % 7);
    return result(ans(21) & 1);
}
DWORD THISCALL s_local_has(void *) { ev(0x717); return result(ans(22) & 1); }
DWORD THISCALL s_hud(void *hud, const char *text, DWORD method, DWORD h) { ev(0x718, (DWORD)obj_id(hud), text_of(text), method ^ (h << 8)); return result(0x18); }
DWORD THISCALL s_status(void *app, ZString *text, DWORD seconds, DWORD flag) { ev(0x719, (DWORD)obj_id(app), ztext(text), seconds ^ (flag & 0xff)); return result(0x19); }
DWORD __fastcall s_window(ZString *speaker, ZString *text, DWORD a1, DWORD flag) { ev(0x71a, ztext(speaker), ztext(text), a1 ^ ((flag & 0xff) << 24)); return result(0x1a); }
void *THISCALL s_game(void *) { ev(0x71b); return ans(23) & 1 ? &g_world.objs[0] : nullptr; }
DWORD __fastcall s_player(DWORD a, DWORD b) { ev(0x71c, a, b); return 0x5e7a0000u + (a & 0xff); }
DWORD __fastcall s_player_at(DWORD i) { ev(0x71d, i); return 0x5e7b0000u + i * 0x40; }
void *__cdecl s_net() { ev(0x71e); return &g_world.objs[1]; }
DWORD THISCALL s_net_send(void *net, DWORD p, DWORD script, DWORD line, DWORD type) { ev(0x71f, (DWORD)obj_id(net), p, script ^ (line << 8) ^ (type << 16)); return result(0x1f); }
DWORD __fastcall s_report(const char *text, DWORD e, DWORD s1, DWORD s2, DWORD s3, void *caller, DWORD s5)
{
    ev(0x720, text_of(text), e ^ s1, s2 ^ (s3 << 1));
    ev(0x721, real_tag(caller), s5);
    return result(0x20);
}
DWORD THISCALL s_broadcast(void *p, const char *text) { ev(0x722, (DWORD)obj_id(p), text_of(text)); return result(0x22); }
DWORD THISCALL s_switch(void *app, const char *name, const char *where, float *pos, DWORD one, DWORD ret)
{
    ev(0x723, (DWORD)obj_id(app), text_of(name), text_of(where));
    ev(0x724, D(pos, 0) ^ (D(pos, 4) << 1), one, ret);
    return result(0x24);
}
DWORD THISCALL s_position(FakeObject *p, float *out)
{
    ev(0x725, (DWORD)obj_id(p));
    out[0] = p->pos[0];
    out[1] = p->pos[1];
    return result(0x25);
}
DWORD THISCALL s_at_target(BYTE *o, DWORD a1, DWORD a2, DWORD a3, DWORD a4)
{
    ev(0x726, D(o, 8), D(o, 0xc), a1 ^ (a2 << 1) ^ (a3 << 2) ^ (a4 << 3));
    return result(0x26);
}
DWORD THISCALL s_switch_execute(void *, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    ev(0x727, (DWORD)obj_id(layer), a2 ^ (a3 << 1), (DWORD)obj_id(vp(a4)));
    return result(ans(24));
}
void *THISCALL s_holder_ctor(void *block) { ev(0x728); pool_free(block, 0x34); return &g_world.objs[1]; }
DWORD THISCALL s_holder_set(void *at_, void *clone) { ev(0x729, (DWORD)obj_id(at_), (DWORD)obj_id(clone)); return result(0x29); }
void *THISCALL s_push_behavior(void *block, void *part, void *holder, DWORD a)
{
    ev(0x72a, (DWORD)obj_id(part), (DWORD)obj_id(holder), a);
    pool_free(block, 0x1c);
    return block;
}
DWORD THISCALL s_world_sound(void *w, DWORD i) { ev(0x72b, (DWORD)obj_id(w), i); return result(0x2b); }
DWORD THISCALL s_discover(void *inv, DWORD d) { ev(0x72c, (DWORD)obj_id(inv), d); return result(0x2c); }
DWORD THISCALL s_undiscover(void *inv, DWORD d) { ev(0x72d, (DWORD)obj_id(inv), d); return result(0x2d); }
void *THISCALL s_inventory(void *p) { ev(0x72e, (DWORD)obj_id(p)); return ans(25) % 4 ? &g_world.objs[7] : nullptr; }
const char *THISCALL s_record_name(void *list, DWORD i) { ev(0x72f, ptrv(list), i); return i & 1 ? "Fruit Single" : "Laser Gun"; }
void *THISCALL s_first(void *) { ev(0x730); return &g_world.parts[ans(26) % NPART]; }   // never none: the callers do not test
DWORD THISCALL s_temp_get(void **out) { ev(0x731); *out = &g_world.templist; D(&g_world.templist, 8) = 0x5e7c0000u; return result(0x31); }
DWORD THISCALL s_temp_release(void **list) { ev(0x732, (DWORD)obj_id(*list), D(*list, 8)); return result(0x32); }
DWORD THISCALL s_find_rect(void *layer, void *list, const int *rect, DWORD kind, DWORD flags, DWORD a, DWORD b)
{
    ev(0x733, (DWORD)obj_id(layer) ^ ((DWORD)obj_id(list) << 16), fnv(2166136261u, rect, 16), kind ^ (a << 8) ^ (b << 9));
    ev(0x734, flags);
    D(list, 8) = ans(27) % 3;
    return result(0x34);
}
const char *THISCALL s_local_text(void *entry) { ev(0x735, real_tag(entry)); return "Printed text"; }
void *THISCALL s_find_def(void *inv, DWORD def) { ev(0x736, (DWORD)obj_id(inv), def); return ans(28) & (1u << (def & 3)) ? &g_world.objs[1 + def % 4] : nullptr; }
void *THISCALL s_def_object(void *list, DWORD i) { ev(0x737, ptrv(list), i); return g_world.model; }
void *THISCALL s_entity_ctor(void *block) { ev(0x738); pool_free(block, 0x7c); return &g_world.objs[2]; }
DWORD THISCALL s_place_near(void *layer, void *pile, void *near, float d) { ev(0x739, (DWORD)obj_id(layer), (DWORD)obj_id(pile) ^ ((DWORD)obj_id(near) << 8), bits(d)); return result(0x39); }
DWORD THISCALL s_set_action(void *ai, void *action)
{
    ev(0x73a, real_tag(ai), real_tag(action), D(action, 8));
    note_made(ai);
    note_made(action);
    return result(0x3a);
}
DWORD THISCALL s_set_item(void *g, void *item) { ev(0x73b, real_tag(g), (DWORD)obj_id(item)); return result(0x3b); }
DWORD THISCALL s_inv_remove(void *inv, void *item) { ev(0x73c, (DWORD)obj_id(inv), (DWORD)obj_id(item)); return result(0x3c); }
DWORD THISCALL s_add(void *actions, void *a)
{
    // a CDeleteAction (vtable 0x5f6454) by its name, the others by their tag
    const DWORD extra = !fake(a) && D(a, 0) == 0x5f6454 ? ztext(static_cast<BYTE *>(a) + 8) : 0;
    ev(0x73d, real_tag(actions), real_tag(a), extra);
    note_made(a);
    return result(0x3d);
}
DWORD THISCALL s_add_ai(void *pile, void *ai)
{
    ev(0x73e, (DWORD)obj_id(pile), real_tag(ai), fake(ai) ? 0 : D(ai, 0x34) & 0xff);
    note_made(ai);
    return result(0x3e);
}
DWORD THISCALL s_focus(void *inv) { ev(0x73f, (DWORD)obj_id(inv)); return ans(29) % 5 == 4 ? 0xffffffff : ans(29) % 5; }
DWORD THISCALL s_cur_index(void *inv, DWORD cat) { ev(0x740, (DWORD)obj_id(inv), cat); return ans(30) % 5 == 4 ? 0xffffffff : ans(30) % 5; }
DWORD THISCALL s_has_dyn(void *o, void *f) { ev(0x741, (DWORD)obj_id(o), (DWORD)obj_id(f)); return result(ans(31) & 1); }
void THISCALL s_set_dyn(void *f, void *o, void *v) { ev(0x742, (DWORD)obj_id(f), (DWORD)obj_id(o), (DWORD)obj_id(v)); }
void *THISCALL s_take_dyn(void *f, void *o) { ev(0x743, (DWORD)obj_id(f), (DWORD)obj_id(o)); return &g_world.objs[5]; }
void THISCALL s_remove_dyn(void *f, void *o) { ev(0x744, (DWORD)obj_id(f), (DWORD)obj_id(o)); }
DWORD THISCALL s_repair(void *inv) { ev(0x745, (DWORD)obj_id(inv)); return result(0x45); }
DWORD THISCALL s_weapon_data(void *item) { ev(0x746, (DWORD)obj_id(item)); return result(0x46); }
DWORD THISCALL s_obj_precache(void *o) { ev(0x748, real_tag(o)); return result(0x48); }
DWORD THISCALL s_list_load(void *list, DWORD i)
{
    ev(0x747, ptrv(list), i);
    g_world.scripts[i].object = &g_world.script;
    return result(0x47);
}

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x41ad40, (void *)&s_resolve},       {0x4fbf20, (void *)&s_part_name},    {0x5b9ef0, (void *)&s_savedata_ctor},
    {0x4e6990, (void *)&s_set_custom},    {0x419020, (void *)&s_set_help},     {0x423960, (void *)&s_compass},
    {0x47e910, (void *)&s_dialog_record}, {0x592900, (void *)&s_dialog_said},  {0x41c7a0, (void *)&s_queue_add},
    {0x416760, (void *)&s_clone},         {0x41a740, (void *)&s_execute_from}, {0x41dac0, (void *)&s_line},
    {0x41e010, (void *)&s_line_text},     {0x4a4670, (void *)&s_ini},          {0x57e690, (void *)&s_sound_index},
    {0x57ea30, (void *)&s_sound_precache}, {0x57e5d0, (void *)&s_sound_name},  {0x47ccc0, (void *)&s_speaker},
    {0x595b50, (void *)&s_voice_log},     {0x595be0, (void *)&s_voice_add},    {0x500ab0, (void *)&s_local_has},
    {0x506b90, (void *)&s_hud},           {0x5a0a00, (void *)&s_status},       {0x47ca10, (void *)&s_window},
    {0x59ff80, (void *)&s_game},          {0x5babc0, (void *)&s_player},       {0x5ba810, (void *)&s_player_at},
    {0x5bd5d0, (void *)&s_net},           {0x5bd640, (void *)&s_net_send},     {0x41abd0, (void *)&s_report},
    {0x4fc240, (void *)&s_broadcast},     {0x59b3a0, (void *)&s_switch},       {0x4fb0a0, (void *)&s_position},
    {0x4c1060, (void *)&s_at_target},     {0x4c1030, (void *)&s_switch_execute}, {0x42f5e0, (void *)&s_holder_ctor},
    {0x586f80, (void *)&s_holder_set},    {0x58b2a0, (void *)&s_push_behavior}, {0x4f6070, (void *)&s_world_sound},
    {0x426690, (void *)&s_discover},      {0x426740, (void *)&s_undiscover},   {0x4267e0, (void *)&s_inventory},
    {0x591d00, (void *)&s_record_name},   {0x4f5d80, (void *)&s_first},        {0x59a470, (void *)&s_temp_get},
    {0x59a510, (void *)&s_temp_release},  {0x4ed5e0, (void *)&s_find_rect},    {0x5007a0, (void *)&s_local_text},
    {0x4258b0, (void *)&s_find_def},      {0x48d8f0, (void *)&s_def_object},   {0x487ff0, (void *)&s_entity_ctor},
    {0x4eb7b0, (void *)&s_place_near},    {0x41d0a0, (void *)&s_set_action},   {0x5b3ff0, (void *)&s_set_item},
    {0x426260, (void *)&s_inv_remove},    {0x4b8800, (void *)&s_add},          {0x4fbc50, (void *)&s_add_ai},
    {0x426070, (void *)&s_focus},         {0x425290, (void *)&s_cur_index},    {0x417390, (void *)&s_has_dyn},
    {0x4187c0, (void *)&s_set_dyn},       {0x418810, (void *)&s_take_dyn},     {0x418fb0, (void *)&s_remove_dyn},
    {0x4262e0, (void *)&s_repair},        {0x5664b0, (void *)&s_weapon_data},  {0x55e090, (void *)&s_list_load},
    {0x4167b0, (void *)&s_obj_precache},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_stand_patches[NSTANDS];

void init_world_tables()
{
    for (int i = 0; i < 160; i++)
        g_vtable[i] = (void *)&f_bad;
    g_vtable[1] = (void *)&f_delete;
    g_vtable[2] = (void *)&f_class;
    g_vtable[7] = (void *)&f_precache;
    g_vtable[11] = (void *)&f_count;
    g_vtable[19] = (void *)&f_reset;
    g_vtable[23] = (void *)&f_start;
    g_vtable[26] = (void *)&f_item_at;
    g_vtable[36] = (void *)&f_inventory;
    g_vtable[43] = (void *)&f_position;
    g_vtable[45] = (void *)&f_set45;
    g_vtable[64] = (void *)&f_find_ai;
    g_vtable[93] = (void *)&f_set_model;
    g_vtable[127] = (void *)&f_take_inventory;
    g_vtable[128] = (void *)&f_give_inventory;
    for (int i = 0; i < 100; i++)
        g_app_vtable[i] = (void *)&f_bad;
    g_app_vtable[16] = (void *)&a_warning;
    g_app_vtable[44] = (void *)&a_players;
    g_app_vtable[84] = (void *)&a_playing;
    g_app_vtable[97] = (void *)&a_save_data;
    for (int i = 0; i < 50; i++)
        g_display_vtable[i] = (void *)&f_bad;
    g_display_vtable[45] = (void *)&d_height;
    for (int i = 0; i < 20; i++)
        g_reporter_vtable[i] = (void *)&f_bad;
    g_reporter_vtable[17] = (void *)&f_report;
    // CZaxDialogAction's vtable with slot 29 recording
    memcpy(g_dialog_vtable, at<void *>(0x6031ac), sizeof g_dialog_vtable);
    g_dialog_vtable[29] = ptrv((void *)&f_dialog_show);
    const DWORD getters[10] = {0x4163b0, 0x49a930, 0x48dcc0, 0x5b9c00, 0x5bb110, 0x51a270, 0x5b5f10, 0x41a650, 0x429e50, 0x5aa020};
    for (int i = 0; i < 10; i++)
        g_classes[i] = at<CClassInfo *(__cdecl *)()>(getters[i])();
    for (int i = 0; i < NLINES; i++) {
        Line &l = g_lines[i];
        l.vftable = 0x5e7d0000;
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&l.name, LINE_TEXTS[i][0]);
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&l.text, LINE_TEXTS[i][1]);
    }
    // the static "$trigger" of the drop action (0x713f90, guard bit 1 of 0x713f68) made now as the
    // action's first run would, so both sides find it made
    BYTE &guard = *at<BYTE *>(0x713f68);
    if (!(guard & 1)) {
        guard |= 1;
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(at<void *>(0x713f90), "$trigger");
        crt_atexit(reinterpret_cast<void(__cdecl *)()>(static_cast<uintptr_t>(0x5a7240)));
    }
}

DWORD rnd(DWORD n) { return (random() >> 8) % n; }

void setup(unsigned seed)
{
    g_seed = seed;
    World &w = g_world;
    memset(&w, 0, sizeof w);
    FakeObject *all[NPART + NOBJ + 3];
    int n = 0;
    for (int i = 0; i < NPART; i++)
        all[n++] = &w.parts[i];
    for (int i = 0; i < NOBJ; i++)
        all[n++] = &w.objs[i];
    all[n++] = &w.layer;
    all[n++] = &w.display;
    all[n++] = &w.templist;
    for (int i = 0; i < n; i++) {
        FakeObject &o = *all[i];
        o.vftable = ptrv(g_vtable);
        for (DWORD k = 0; k < sizeof o.body; k++)
            o.body[k] = (BYTE)random();
        o.cls = g_classes[rnd(10)];
        o.id = (DWORD)i;
        for (DWORD &a : o.answer)
            a = random();
        o.pos[0] = (float)(int)rnd(4000) + (float)rnd(4) * 0.25f;
        o.pos[1] = (float)(int)rnd(4000) - 0.5f;
    }
    w.display.vftable = ptrv(g_display_vtable);
    // parts: +0x48 the layer (or none)
    for (int i = 0; i < NPART; i++)
        D(&w.parts[i], 0x48) = rnd(5) ? ptrv(&w.layer) : 0;
    // the layer: +0x198 its custom save data (a CZaxLayerSaveData or another or none)
    D(&w.layer, 0x198) = rnd(3) ? ptrv(&w.objs[5]) : 0;
    // the application and the world
    for (DWORD k = 0; k < sizeof w.app; k += 4)
        D(w.app, k) = random();
    D(w.app, 0) = ptrv(g_app_vtable);
    D(w.app, 0x34c) = ptrv(w.app_data);
    w.app[0x589] = (BYTE)rnd(2);
    for (DWORD k = 0; k < sizeof w.world; k += 4)
        D(w.world, k) = random();
    D(w.world, 0x2a8) = ptrv(&w.display);
    D(w.world, 0x280) = ptrv(&w.objs[0]);
    // records: the catalogue's definitions (fakes, or none while "being loaded"), the quip categories,
    // the scripts (their objects made by the load stand-in when missing)
    for (int d = 0; d < NREC; d++) {
        Record &c = w.catalogue[d];
        c.name[0] = (BYTE)('a' + d);
        if (rnd(4)) {
            c.object = &w.objs[(d + 1) % NOBJ];
        } else
            c.loading = 1;
        w.quips[d].object = &w.objs[(d + 3) % NOBJ];
        w.quips[d].in_use = (BYTE)rnd(2);
        w.quips[d].unknown7e = (BYTE)rnd(2);
        w.scripts[d].object = rnd(3) ? &w.script : nullptr;   // never "being loaded": the original reads the script without a test
    }
    for (int i = 0; i < NOBJ; i++)
        w.objs[i].body[0x9c - 4] = (BYTE)rnd(2);   // +0x9c of a definition
    w.script.vftable = ptrv(g_vtable);
    w.script.lines = ptrv(w.script_lines);
    for (int i = 0; i < NLINES; i++)
        w.script_lines[i] = ptrv(&g_lines[(i + seed) % NLINES]);
    w.reporter_vftable = ptrv(g_reporter_vtable);
    for (DWORD &a : w.answers)
        a = random();
    for (DWORD k = 0; k < 0x20; k++)
        w.model[k] = random();
    w.model[0x74 / 4] = rnd(2) ? ptrv(&w.objs[4]) : 0;
}

constexpr DWORD SWAPS[] = {0x713f14, 0x6c2080, 0x6c02cc, 0x6c0c38, 0x6c0c3c, 0x6c0570, 0x6c0574, 0x62e810, 0x62e814, 0x713f6c};
constexpr int NSWAPS = sizeof SWAPS / sizeof SWAPS[0];
DWORD g_saved[NSWAPS];

void world_in()
{
    World &w = g_world;
    for (int i = 0; i < NSTANDS; i++)
        patch_jump(g_stand_patches[i], STANDS[i].address, STANDS[i].fn);
    const DWORD values[NSWAPS] = {ptrv(w.app),     ptrv(w.world), ptrv(&w.reporter_vftable), ptrv(w.catalogue), NREC, ptrv(w.quips), NREC,
                                  ptrv(w.scripts), NREC,          ptrv(w.confiscated_field)};
    for (int i = 0; i < NSWAPS; i++) {
        g_saved[i] = *at<DWORD *>(SWAPS[i]);
        *at<DWORD *>(SWAPS[i]) = values[i];
    }
}
void world_out()
{
    for (int i = NSWAPS - 1; i >= 0; i--)
        *at<DWORD *>(SWAPS[i]) = g_saved[i];
    for (int i = NSTANDS - 1; i >= 0; i--)
        unpatch(g_stand_patches[i]);
}
// The real objects made during a case deleted (outside the case, through the original destructors)
void delete_made()
{
    hooks_out();
    for (int i = g_nmade - 1; i >= 0; i--)
        vcall<void *>(g_made_objects[i], 1, (DWORD)1);
    hooks_in();
    g_nmade = 0;
}

// ---- Behaviour cases ---------------------------------------------------------------------------------------

enum Kind { SLOT23, ONE, TWO_PARTS, FOUR, SPLIT, PRECACHE, SHOW, SAY, SWITCH };
struct Case {
    const char *name;
    DWORD create, fn;
    Kind kind;
    bool al;
};
const Case CASES[] = {
    {"CSetCompassTargetAction::Execute", 0x5a4270, 0x5a44b0, SLOT23, true},
    {"CZaxDialogAction::Execute", 0x5a4650, 0x5a4a90, SLOT23, true},
    {"CZaxDialogAction::SetDisplayType", 0x5a4650, 0x5a4bc0, ONE, false},
    {"CZaxDialogAction::SplitLine", 0x5a4650, 0x5a4bd0, SPLIT, false},
    {"CZaxDialogAction::Precache", 0x5a4650, 0x5a4c90, PRECACHE, false},
    {"CZaxDialogAction::ShowLine", 0x5a4650, 0x5a4f40, SHOW, true},
    {"Dialog_SayLine", 0, 0x5a5120, SAY, false},
    {"CSwitchMapAction::Switch", 0x5a5320, 0x5a5570, SWITCH, false},
    {"CRelocateAction::SetOffset", 0x5a56b0, 0x5a59f0, TWO_PARTS, false},
    {"CRelocateAction::AtTarget", 0x5a56b0, 0x5a5a50, FOUR, false},
    {"CTeleportAction::Execute", 0x5a5b80, 0x5a5fa0, SLOT23, true},
    {"CDiscoverInventoryItemAction::Execute", 0x5a60b0, 0x5a6390, SLOT23, true},
    {"CIsEnemyNearbyAction::Execute", 0x5a65c0, 0x5a6730, SLOT23, true},
    {"CPrintAction::Execute", 0x5a6860, 0x5a6b30, SLOT23, true},
    {"CDropAllOreAndCrystalsAction::Execute", 0x5a6bc0, 0x5a6e50, SLOT23, true},
    {"CDropCurrentItemOfTypeAction::Execute", 0x5a72a0, 0x5a75c0, SLOT23, true},
    {"CConfiscateEntireInventoryAction::Execute", 0x5a7720, 0x5a7890, SLOT23, true},
    {"CReturnConfiscatedInventoryAction::Execute", 0x5a7960, 0x5a7ad0, SLOT23, true},
    {"CDischargeWeaponThisFrameAction::Weapon", 0x5a7bb0, 0x5a7e20, ONE, false},
};
constexpr int NCASES = sizeof CASES / sizeof CASES[0];

const char *const NAMES[] = {"", "$Instigator", "$Trigger", "Door 1", "Fruit", "Zax", "Respawn2"};
DWORD class_size(DWORD create)
{
    for (const ClassDef &c : CLASSES)
        if (c.create == create)
            return c.size;
    return 0;
}
// Strings of a class (bit k: the ZString at 4k)
DWORD string_bits(DWORD create)
{
    switch (create) {
    case 0x5a4270: return 1u << 2;                                // +8
    case 0x5a5320: case 0x5a56b0: case 0x5a5b80: return 0x70u << 0;   // +0x10, +0x14, +0x18
    case 0x5a6860: return (1u << 2) | (1u << 3);                  // the Local.txt entry +8, +0xc
    default: return 0;
    }
}
// The owned children a destructor would delete (fakes put in by the case) taken out again
void unfill(BYTE *o, DWORD create)
{
    switch (create) {
    case 0x5a4650: D(o, 0x80) = 0; break;
    case 0x5a5b80: D(o, 0x24) = 0; break;
    case 0x5a6bc0: D(o, 0xc) = 0; break;
    }
}
void assign(BYTE *o, DWORD off) { at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(o + off, NAMES[rnd(7)]); }
void fill(BYTE *o, DWORD create)
{
    World &w = g_world;
    switch (create) {
    case 0x5a4270: assign(o, 8); break;
    case 0x5a4650:
        D(o, 8) = rnd(5);
        D(o, 0xc) = rnd(5);
        D(o, 0x78) = rnd(4);
        D(o, 0x7c) = rnd(3);
        D(o, 0x80) = rnd(3) ? ptrv(&w.objs[6]) : 0;
        D(o, 0x84) = random();
        break;
    case 0x5a5320: case 0x5a56b0: case 0x5a5b80:
        D(o, 8) = float_bits((float)(int)rnd(400) - 200.0f);
        D(o, 0xc) = float_bits((float)(int)rnd(400) - 200.0f);
        assign(o, 0x10);
        assign(o, 0x14);
        assign(o, 0x18);
        D(o, 0x1c) = rnd(4);
        if (create != 0x5a5320)
            o[0x20] = (BYTE)rnd(2);
        if (create == 0x5a5b80)
            D(o, 0x24) = rnd(3) ? ptrv(&w.objs[6]) : 0;
        break;
    case 0x5a60b0: D(o, 8) = rnd(NREC + 2); o[0xc] = (BYTE)rnd(2); break;
    case 0x5a6860: D(o, 0x10) = rnd(4); break;
    case 0x5a6bc0: D(o, 8) = rnd(30); D(o, 0xc) = rnd(2) ? ptrv(&w.objs[6]) : 0; break;
    case 0x5a72a0: D(o, 0x10) = rnd(4); break;
    case 0x5a7bb0: o[0x20] = (BYTE)rnd(2); break;
    }
}

BYTE g_obj_after[2][0x88];
DWORD g_obj_size[2];
World g_after[2];
DWORD g_extra[2][4];

// Runs the case's function on an action (made here when `real` is null, else the given copy)
DWORD run_case(const Case &c, int side, BYTE *given)
{
    World &w = g_world;
    BYTE *o = given;
    if (!o && c.create) {
        o = static_cast<BYTE *>(at<Create0>(c.create)());
        fill(o, c.create);
    }
    const DWORD size = c.create ? class_size(c.create) : 0;
    if (o && c.create == 0x5a4650)
        D(o, 0) = ptrv(g_dialog_vtable);
    BYTE *part = reinterpret_cast<BYTE *>(&w.parts[rnd(NPART)]);
    BYTE *maybe = rnd(5) ? part : nullptr;   // where the code tests for none
    const DWORD a = random(), b = random(), x = random();
    DWORD r = 0;
    memset(g_extra[side], 0, sizeof g_extra[side]);
    switch (c.kind) {
    case SLOT23:
        r = at<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(c.fn)(o, &w.layer, c.fn == 0x5a6e50 || c.fn == 0x5a75c0 ? ptrv(maybe) : a, b,
                                                                            ptrv(part));
        break;
    case ONE: r = at<T1>(c.fn)(o, c.fn == 0x5a7e20 ? ptrv(maybe) : rnd(5)); break;
    case TWO_PARTS: r = at<DWORD(THISCALL *)(void *, void *, void *)>(c.fn)(o, &w.parts[rnd(NPART)], &w.parts[rnd(NPART)]); break;
    case FOUR: r = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(c.fn)(o, ptrv(&w.parts[rnd(NPART)]), a, b, ptrv(&w.parts[rnd(NPART)])); break;
    case SPLIT: {
        ZString s, t;
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&s, NAMES[rnd(7)]);
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&t, NAMES[rnd(7)]);
        at<void(THISCALL *)(void *, void *, void *)>(c.fn)(o, &s, &t);
        g_extra[side][0] = ztext(&s);
        g_extra[side][1] = ztext(&t);
        at<DWORD(THISCALL *)(void *)>(0x4defd0)(&t);
        at<DWORD(THISCALL *)(void *)>(0x4defd0)(&s);
        break;
    }
    case PRECACHE: at<void(THISCALL *)(void *)>(c.fn)(o); break;
    case SHOW: {
        ZString s, t;
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&s, NAMES[rnd(7)]);
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&t, NAMES[rnd(7)]);
        r = at<DWORD(THISCALL *)(void *, void *, void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(c.fn)(o, &s, &t, a, ptrv(part), b, x & 0xff,
                                                                                               rnd(3));
        at<DWORD(THISCALL *)(void *)>(0x4defd0)(&t);
        at<DWORD(THISCALL *)(void *)>(0x4defd0)(&s);
        break;
    }
    case SAY: at<void(__fastcall *)(DWORD, DWORD, DWORD)>(c.fn)(rnd(NREC + 1), rnd(NLINES), rnd(4)); break;
    case SWITCH: {
        ZString s, t;
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&s, NAMES[rnd(7)]);
        at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(&t, NAMES[rnd(7)]);
        r = at<DWORD(THISCALL *)(void *, void *, void *, void *)>(c.fn)(o, &s, &t, maybe);
        at<DWORD(THISCALL *)(void *)>(0x4defd0)(&t);
        at<DWORD(THISCALL *)(void *)>(0x4defd0)(&s);
        break;
    }
    }
    g_obj_size[side] = 0;
    if (o) {
        memcpy(g_obj_after[side], o, size);
        g_obj_size[side] = size;
        // the strings by their text now: their buffers go back to the pool with the action
        const DWORD strings = string_bits(c.create);
        for (DWORD k = 0; k < size / 4; k++)
            if (k < 32 && (strings >> k & 1))
                if (const char *text = buffer_text(*reinterpret_cast<DWORD *>(g_obj_after[side] + 4 * k)))
                    *reinterpret_cast<DWORD *>(g_obj_after[side] + 4 * k) = 0x7e700000u ^ fnv_text(2166136261u, text);
        if (c.create == 0x5a4650)
            D(o, 0) = 0x6031ac;
        unfill(o, c.create);
        if (!given)
            vcall<void *>(o, 1, (DWORD)1);
    }
    return r;
}

DWORD case_result(const Case &c, DWORD raw[2], DWORD r[2])
{
    if (raw[0] == raw[1])   // equal raw values are equal before the arena mapping
        r[0] = r[1] = raw[0];
    if (c.al) {
        r[0] &= 0xff;
        r[1] &= 0xff;
    }
    if (c.kind == SPLIT || c.kind == PRECACHE || c.kind == SAY || c.fn == 0x5a4bc0)
        r[0] = r[1] = 0;   // no defined eax
    return r[0] == r[1];
}

void test_case(Tally &t, const Case &c, unsigned seed, BYTE *const copies[2])
{
    DWORD r[2], raw[2];
    for (int side = 0; side < 2; side++) {
        setup(seed);
        log_reset(side);
        side_begin(side);
        world_in();
        arena_on(side, 0x5c);
        g_seed = seed * 13u + 5u;
        raw[side] = run_case(c, side, copies ? copies[side] : nullptr);
        r[side] = rel(raw[side], side);
        arena_off();
        world_out();
        side_end(side);
        memcpy(&g_after[side], &g_world, sizeof g_world);
        delete_made();
    }
    const bool same_eax = case_result(c, raw, r);
    const bool same_obj = g_obj_size[0] == g_obj_size[1] && same_region(g_obj_after[0], g_obj_after[1], g_obj_size[0]);
    const bool same = same_eax && same_obj && same_log() && g_used[0] == g_used[1] &&
                      memcmp(g_extra[0], g_extra[1], sizeof g_extra[0]) == 0 &&
                      same_region(reinterpret_cast<BYTE *>(&g_after[0]), reinterpret_cast<BYTE *>(&g_after[1]), sizeof(World)) &&
                      same_region(g_arena[0], g_arena[1], g_used[0]);
    char d[200];
    wsprintfA(d, "[seed %u: eax %x / %x, object %d, log %d / %d, heap %u / %u, first difference +%x: %x / %x]", seed, r[0], r[1], same_obj,
              g_nlog[0] / 4, g_nlog[1] / 4, g_used[0], g_used[1], g_diff_at, g_diff_a, g_diff_b);
    if (!same && t.mismatches < 4) {
        log_first_difference();
        for (int side = 0; side < 2; side++)
            for (int i = 0; i + 4 <= g_nlog[side] && i < 32; i += 4)
                log("    side %d event %d: %x %x %x %x", side, i / 4, g_log[side][i], g_log[side][i + 1], g_log[side][i + 2], g_log[side][i + 3]);
    }
    t.check(same, c.name, d);
}

// ---- Real data: the module's actions in the running level -------------------------------------------------

bool heap_pointer(DWORD p) { return p >= 0x10000 && !(p & 3) && (p < 0x400000 || p >= IMAGE_END) && p < 0x80000000u; }
constexpr int MAX_SEEN = 16384, MAX_FOUND = 1024;
DWORD g_seen[MAX_SEEN];
int g_nseen, g_objects;
BYTE *g_found[MAX_FOUND];
int g_found_class[MAX_FOUND];
int g_nfound;
int module_class(const void *p)
{
    CClassInfo *c = class_of(p);
    if (!c)
        return -1;
    for (int i = 0; i < NCLASSES; i++)
        if (*at<CClassInfo **>(CLASSES[i].var) == c)
            return i;
    return -1;
}
bool seen(DWORD p)
{
    for (int i = 0; i < g_nseen; i++)
        if (g_seen[i] == p)
            return true;
    if (g_nseen < MAX_SEEN)
        g_seen[g_nseen++] = p;
    return false;
}
void visit(BYTE *o, int depth)
{
    const CClassInfo *ci = class_of(o);
    if (!ci || g_nseen >= MAX_SEEN || seen(ptrv(o)))
        return;
    g_objects++;
    const int k = module_class(o);
    if (k >= 0 && g_nfound < MAX_FOUND) {
        g_found_class[g_nfound] = k;
        g_found[g_nfound++] = o;
    }
    if (depth >= 8 || ci->size > 0x1000 || !readable(o, ci->size))
        return;
    for (DWORD off = 4; off + 4 <= ci->size; off += 4) {
        const DWORD v = D(o, off);
        if (!heap_pointer(v) || !readable(vp(v), 4))
            continue;
        if (class_of(vp(v)))
            visit(static_cast<BYTE *>(vp(v)), depth + 1);
        else if (off + 8 <= ci->size && D(o, off + 4) && D(o, off + 4) <= 256 && readable(vp(v), D(o, off + 4) * 4))
            for (DWORD i = 0; i < D(o, off + 4); i++)   // a pointer array's data and count
                visit(static_cast<BYTE **>(vp(v))[i], depth + 1);
    }
}
bool find_level_actions()
{
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (!world)
        return false;
    BYTE *layer = real_player_layer();
    if (!layer || !readable(layer + 0x50, 0x28))
        return false;
    const DWORD columns = D(layer, 0x60), rows = D(layer, 0x64);
    const ZPtrArray *cells = *reinterpret_cast<ZPtrArray **>(layer + 0x68);
    if (columns > 4096 || rows > 4096 || !cells || !readable(cells, columns * rows * 16))
        return false;
    for (DWORD i = 0; i < columns * rows; i++) {
        const ZPtrArray *c = &cells[i];
        if (c->count > 10000 || (c->count && !readable(c->data, c->count * 4)))
            return false;
        for (DWORD k = 0; k < c->count; k++) {
            BYTE *part = static_cast<BYTE *>(c->data[k]);
            visit(part, 0);
            const LayerPart *p = reinterpret_cast<const LayerPart *>(part);
            if (readable(p, sizeof *p) && p->activity.count && p->activity.count <= 1000 && readable(p->activity.data, p->activity.count * 4))
                for (DWORD j = 0; j < p->activity.count; j++)
                    visit(static_cast<BYTE *>(p->activity.data[j]), 1);
        }
    }
    return true;
}
// A copy of a real action (Object_Clone of the original, hooks of no module out) with its owned
// children deleted (they are clones too) and replaced by a fake
BYTE *real_copy(BYTE *real, DWORD create)
{
    BYTE *copy = static_cast<BYTE *>(at<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(real, 0, 0xffffffff));
    const DWORD off = create == 0x5a4650 ? 0x80 : create == 0x5a5b80 ? 0x24 : create == 0x5a6bc0 ? 0xc : 0;
    if (off && D(copy, off)) {
        vcall<void *>(vp(D(copy, off)), 1, (DWORD)1);
        D(copy, off) = ptrv(&g_world.objs[6]);
    }
    return copy;
}

void test_level(bool found, Tally &t, int &cases)
{
    if (!found) {
        log("  zax actions2 real data: no level grid found");
        return;
    }
    int per_class[NCLASSES] = {}, index[NCLASSES] = {};
    for (int i = 0; i < g_nfound; i++)
        per_class[g_found_class[i]]++;
    log("  zax actions2 real data: %d objects reached from the level's parts, %d of the module's classes", g_objects, g_nfound);
    for (int i = 0; i < g_nfound; i++) {
        const int k = g_found_class[i];
        if (!spread_pick(index[k]++, per_class[k], 24))
            continue;
        const ClassDef &cd = CLASSES[k];
        for (int j = 0; j < NCASES; j++) {
            const Case &c = CASES[j];
            if (c.create != cd.create || c.fn == 0x5a4bc0)
                continue;
            BYTE *copies[2];
            for (int side = 0; side < 2; side++)
                copies[side] = real_copy(g_found[i], cd.create);
            // bytes no constructor and no clone writes (CDialogActionBase's +0x10..+0x77, the padding
            // after a byte field) hold the pool blocks' old contents: the second copy gets the first's
            const DWORD pad = cd.create == 0x5a56b0 || cd.create == 0x5a5b80 || cd.create == 0x5a7bb0 ? 0x21 : cd.create == 0x5a60b0 ? 0xd : 0;
            if (pad)
                memcpy(copies[1] + pad, copies[0] + pad, 3);
            if (cd.create == 0x5a4650)
                memcpy(copies[1] + 0x10, copies[0] + 0x10, 0x68);
            test_case(t, c, 77000u + (unsigned)i * 31u + (unsigned)j, copies);
            for (int side = 0; side < 2; side++) {
                unfill(copies[side], cd.create);
                vcall<void *>(copies[side], 1, (DWORD)1);
            }
            cases++;
        }
    }
}

}  // namespace

ZAX_CHECKS(zax_actions2)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_zax_actions2;
    if (!arena_setup()) {
        log("Check zax actions2: no arena");
        return;
    }
    init_world_tables();
    const bool found = find_level_actions();
    Tally objects{"zax actions2: Create, CreateArray, destructors"}, getters{"zax actions2: class getters"},
        fields{"zax actions2: RegisterFields"}, registered{"zax actions2: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, NCLASSES);
    for (unsigned k = 0; k < (unsigned)scaled(30); k++)
        test_base_dtors(objects, k);
    objects.report();
    getters.report();
    fields.report();
    registered.report();

    Tally behaviour{"zax actions2: execute and the other slots on a fake world"};
    for (int k = 0; k < NCASES; k++) {
        log("  %s", CASES[k].name);   // the last line before a case that ends the game
        for (unsigned s = 0; s < (unsigned)scaled(200); s++)
            test_case(behaviour, CASES[k], 9000u * (unsigned)k + s, nullptr);
    }
    behaviour.report();

    Tally level{"zax actions2: the level's actions as copies (real data)"};
    int cases = 0;
    test_level(found, level, cases);
    log("  %d real-data cases", cases);
    level.report();
}

}  // namespace zax
