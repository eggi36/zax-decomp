// Checks of the shields and the segmented items (ai_shield.cpp) against the original, in the game thread.
//
//  - The classes (checks_inventory_common.h): getters (both copies), Create/CreateArray with slot 1,
//    RegisterFields and the registered descriptors, the registered class infos.
//  - The slots and helpers in the fake world (checks_ai_world.h): the object under test a copy of a
//    template made by Create with random fields; entities, inventories, items, definitions, renderers (the
//    fake "other" objects), the item catalogue, the damage type list and the world's settings fakes. The
//    float results (the damage slots) compared by their 80 bits.
//  - The detail text of a segmented item (0x56a5a0) on a real renderer and item: the line object's
//    strings and fields, the object deleted again.
//  - Real data: the level's AIs of these classes and the template items of the loaded item definitions
//    (copies) through their slots.
#include "checks_ai_misc.h"

namespace zax {

int hooks_ai_shield(Hook *out);

namespace {

using namespace aiw;
using namespace aimisc;

// The world every case of the module needs: entities with few AIs (+0x40), items with a fake segment
// array (+0x18 a recorder vtable, data in extra 2), renderers (the "other" fakes: +8 a catalogue index,
// +0x10/+0x14 the segments needed, +0x1c an action), the damage type records all loaded
void world_tweaks(Gen &g)
{
    for (int i = 0; i < N_ENTITY; i++)
        fl<DWORD>(obj(O_ENTITY + i), 0x40) = g.next() % 4;
    BYTE *segs = extra(2);
    for (DWORD k = 0; k < 8; k++)
        fl<DWORD>(segs, k * 4) = g.next() % 6;
    BYTE *needed = extra(1);
    for (DWORD k = 0; k < 8; k++)
        fl<DWORD>(needed, k * 4) = g.next() % 6;
    for (int i = 0; i < N_ITEM; i++) {
        BYTE *it = obj(O_ITEM + i);
        fl<DWORD>(it, 0x18) = ptrv(g_fvt[T_OTHER]);
        fl<DWORD>(it, 0x1c) = ptrv(segs);
        fl<DWORD>(it, 0x20) = g.next() % 5;
    }
    for (int i = 0; i < N_OTHER; i++) {
        BYTE *o = obj(O_OTHER + i);
        fl<DWORD>(o, 8) = g.next() % 4;
        fl<DWORD>(o, 0x10) = ptrv(needed);
        fl<DWORD>(o, 0x14) = g.next() % 5;
        fl<DWORD>(o, 0x1c) = g.next() % 3 ? ptrv(obj(O_OTHER + (int)(g.next() % N_OTHER))) : 0;
    }
    for (int r = 0; r < 4; r++) {
        BYTE *d = obj(O_DMG_RECORDS) + r * 0x80;
        fl<DWORD>(d, 0x78) = ptrv(obj(O_OTHER + r));
        d[0x7f] = 0;
    }
}

void fill_renderer(Gen &g, BYTE *s, bool real)
{
    world_tweaks(g);
    if (!real)
        fl<DWORD>(s, 8) = g.next() % 4;
    fl<DWORD>(s, 0x10) = ptrv(extra(1));
    if (fl<DWORD>(s, 0x14) > 8 || !real)
        fl<DWORD>(s, 0x14) = g.next() % 5;
}
void fill_segmented(Gen &g, BYTE *s, bool real)
{
    world_tweaks(g);
    BYTE *own = extra(0);
    for (DWORD k = 0; k < 8; k++)
        fl<DWORD>(own, k * 4) = g.next() % 6;
    if (!real || fl<DWORD>(s, 0x20) > 8) {
        fl<DWORD>(s, 0x20) = g.next() % 5;
        fl<DWORD>(s, 0x24) = fl<DWORD>(s, 0x20);
    }
    fl<DWORD>(s, 0x1c) = ptrv(own);
    if (!real)
        fl<DWORD>(s, 8) = g.next() % 4;
}
void fill_shield_ai(Gen &g, BYTE *s, bool real)
{
    world_tweaks(g);
    if (!real || fl<DWORD>(s, 0x10))
        fl<DWORD>(s, 0x10) = g.next() % 3 || real ? ptrv(any_entity(g)) : 0;
    if (!real)
        fl<DWORD>(s, 0x14) = g.next() % 4;
}
void fill_percent(Gen &g, BYTE *s, bool real)
{
    world_tweaks(g);
    if (!real) {
        fl<DWORD>(s, 0x10) = g.fbits_(0.0f, 2.0f);
        fl<DWORD>(s, 0x14) = g.next();
    }
}
void fill_shield_item(Gen &g, BYTE *s, bool real)
{
    world_tweaks(g);
    if (!real) {
        fl<DWORD>(s, 0x18) = g.fbits_(0.0f, 300.0f);
        fl<DWORD>(s, 0x20) = g.next() % 4;
        fl<DWORD>(s, 0x24) = g.next() % 8;
    }
    fl<DWORD>(s, 0x1c) = ptrv(obj(O_AI + (int)(g.next() % N_AI)));   // the AI template: a fake
}
void fill_charge(Gen &g, BYTE *s, bool real)
{
    world_tweaks(g);
    if (!real)
        fl<DWORD>(s, 0x18) = g.fbits_(0.0f, 300.0f);
}
void fill_recharge(Gen &g, BYTE *s, bool real)
{
    world_tweaks(g);
    if (!real) {
        fl<DWORD>(s, 0xc) = g.fbits_(0.0f, 100.0f);
        fl<DWORD>(s, 0x10) = g.next() % 2 ? 0 : g.fbits_(0.0f, 100.0f);
        fl<DWORD>(s, 0x14) = g.next() % 4;
    }
}

enum { C_RENDERER, C_SEGMENTED, C_SHIELD_AI, C_PERCENT, C_SHIELD_ITEM, C_CHARGE, C_RECHARGE, NCLASSES };
const SlotClass CLASSES[NCLASSES] = {
    {{"CSegmentedInventoryDetailRenderer", 0x20, 0x713354, 0x569ea0, 0x569f00, 0x56a100, {0x569e50, 0x56a0a0}, true}, 0x5ff53c, fill_renderer},
    {{"CSegmentedInventoryItem", 0x28, 0x713350, 0x5695b0, 0x569600, 0x5697d0, {0x569560, 0x569770}, true}, 0x5ff478, fill_segmented},
    {{"CShieldAI", 0x18, 0x71350c, 0x56d4b0, 0x56d500, 0x56d6b0, {0x56d460, 0x56d650}, true}, 0x5ffa9c, fill_shield_ai},
    {{"CPercentInvulnerableAI", 0x18, 0x713510, 0x56dcf0, 0x56dd30, 0x56dea0, {0x56dca0, 0x56de40}, true}, 0x5ffb44, fill_percent},
    {{"CShieldInventoryItem", 0x28, 0x713520, 0x56e060, 0x56e0b0, 0x56e200, {0x56e010, 0x56e1a0}, true}, 0x5ffbec, fill_shield_item},
    {{"CShieldChargeInventoryItem", 0x1c, 0x71352c, 0x56f4f0, 0x56f530, 0x56f690, {0x56f4a0, 0x56f630}, true, true}, 0x5ffe18, fill_charge},
    {{"CRechargeShieldAction", 0x18, 0x713534, 0x56f960, 0x56f9a0, 0x56fb10, {0x56f910, 0x56fab0}, true}, 0x5ffee4, fill_recharge},
};

void a_none(Gen &, DWORD *) {}
void a_entity(Gen &g, DWORD *a) { a[0] = ptrv(any_entity(g)); }
void a_entity_seconds(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = g.fbits_(0.0f, 0.2f);
}
void a_taker_part(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(any_entity(g));
}
void a_part_or_null(Gen &g, DWORD *a) { a[0] = ptrv(any_entity(g, 6)); }
void a_taker_part_or_null(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(any_entity(g, 6));
}
void a_collide(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = ptrv(any_entity(g, 4));
    BYTE *p = extra(6);
    fl<DWORD>(p, 0) = g.fbits_(-2000.0f, 6000.0f);
    fl<DWORD>(p, 4) = g.fbits_(-2000.0f, 6000.0f);
    a[2] = ptrv(p);
    a[3] = g.next();
    a[4] = g.next();
}
void a_damage(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = g.fbits_(-5.0f, 200.0f);
    a[2] = g.next() % 5;
}
void a_message(Gen &g, DWORD *a)
{
    const DWORD texts[] = {0x610178 /* "Died" */, 0x60aef8 /* "Delete" */, 0x60aee8 /* "Leaving Scope" */};
    a[0] = ptrv(any_entity(g));
    a[1] = texts[g.next() % 3];
}
void a_find_of_type(Gen &g, DWORD *a)
{
    a[0] = ptrv(any_entity(g));
    a[1] = g.next() % 4;
}
void a_target(Gen &g, DWORD *a)
{
    a[0] = g.next();
    a[1] = g.next();
    a[2] = g.next();
    a[3] = ptrv(any_entity(g, 6));
}

const ClassSlot SLOTS[] = {
    {C_RENDERER, {"AsZaxDefinition", 0x56a7e0, 0, 0xffffffff, 50, a_none}},
    {C_RENDERER, {"AsSegmentedRenderer", 0x56a800, 0, 0xffffffff, 50, a_none}},
    {C_RENDERER, {"AsInventoryItem", 0x56a870, 0, 0xffffffff, 50, a_none}},
    {C_RENDERER, {"CSegmentedInventoryDetailRenderer::CompletedItem", 0x56a6b0, 0, 0xffffffff, 300, a_none}},
    {C_SEGMENTED, {"CSegmentedInventoryItem::Renderer", 0x569da0, 0, 0xffffffff, 200, a_none}},
    {C_SEGMENTED, {"CSegmentedInventoryItem::Take", 0x5698a0, 2, 0xffffffff, 800, a_taker_part}},
    {C_SEGMENTED, {"CSegmentedInventoryItem::MayTake", 0x569b70, 1, 0xff, 800, a_entity}},
    {C_SHIELD_AI, {"CShieldAI::FindItem", 0x56d7d0, 1, 0xffffffff, 300, a_entity}},
    {C_SHIELD_AI, {"CShieldAI::Update", 0x56d880, 2, 0, 400, a_entity_seconds}},
    {C_SHIELD_AI, {"CShieldAI::Collide", 0x56d960, 5, 0, 500, a_collide}},
    {C_SHIELD_AI, {"CShieldAI::Damage", 0x56da70, 3 | SLOT_FLOAT, 0xffffffff, 800, a_damage}},
    {C_SHIELD_AI, {"CShieldAI::OnMessage", 0x56dc50, 2, 0, 200, a_message}},
    {C_PERCENT, {"CPercentInvulnerableAI::Damage", 0x56dfe0, 3 | SLOT_FLOAT, 0xffffffff, 300, a_damage}},
    {C_SHIELD_ITEM, {"CShieldInventoryItem::FindOfType", 0x56e410, SLOT_FASTCALL, 0xffffffff, 300, a_find_of_type}},
    {C_SHIELD_ITEM, {"CShieldInventoryItem::MayTake", 0x56e3b0, 1, 0xff, 400, a_part_or_null}},
    {C_SHIELD_ITEM, {"CShieldInventoryItem::Take", 0x56e520, 2, 0xffffffff, 600, a_taker_part_or_null}},
    {C_CHARGE, {"CShieldChargeInventoryItem::MayTake", 0x56f710, 1, 0xff, 300, a_part_or_null}},
    {C_CHARGE, {"CShieldChargeInventoryItem::Take", 0x56f7c0, 2, 0xffffffff, 300, a_taker_part_or_null}},
    {C_CHARGE, {"HeapPointerArray_Ctor", 0x56f8a0, 0, 0xffffffff, 20, a_none}},
    {C_RECHARGE, {"CRechargeShieldAction::Target", 0x56fd40, 4, 0xff, 600, a_target}},
};

Module g_mod{CLASSES, NCLASSES, SLOTS, (int)(sizeof SLOTS / sizeof SLOTS[0]), {}, 0, nullptr, nullptr};

Policy policy()
{
    Policy p{};
    p.find_minus_one = 4;
    p.loading_never = true;
    // the exe reads these without a test where the game guarantees them
    p.nonnull = (1u << T_DEF) | (1u << T_OTHER) | (1u << T_ITEM) | (1u << T_INVENTORY) | (1u << T_ENTITY);
    p.always[0] = at<CClassInfo *(__cdecl *)()>(0x49a930)();   // CEntityCharacter
    p.always[1] = at<CClassInfo *(__cdecl *)()>(0x5b5f10)();   // CZaxInventoryItemDefinition
    p.always[2] = *at<CClassInfo **>(0x713354);                // CSegmentedInventoryDetailRenderer
    return p;
}

// ---- The detail text ----------------------------------------------------------------------------------------

struct Text {
    DWORD vtable, f8, f10, f18;
    unsigned name, pieces;
    bool made;
};
unsigned zs_hash(const BYTE *z)
{
    const ZString *s = reinterpret_cast<const ZString *>(z);
    if (!s->buffer)
        return 1;
    unsigned h = 2166136261u;
    for (const char *p = s->c_str(); *p; p++)
        h = (h ^ (BYTE)*p) * 16777619u;
    return h;
}
void test_describe(Tally &t)
{
    BYTE *r = static_cast<BYTE *>(at<void *(__cdecl *)()>(0x569ea0)());
    BYTE *item = static_cast<BYTE *>(at<void *(__cdecl *)()>(0x5695b0)());
    if (!r || !item) {
        t.check(false, "Describe", "[no objects]");
        return;
    }
    const DWORD count = *at<DWORD *>(0x6c0c3c);
    for (int k = 0; k < scaled(60); k++) {
        g_seed = 991u * (unsigned)k + 7;
        Text x[2];
        for (int side = 0; side < 2; side++) {
            fl<DWORD>(item, 8) = count ? random() % count : 0;
            fl<DWORD>(item, 0x20) = random() % 6;
            fl<DWORD>(r, 0x14) = random() % 6;
            BYTE *arg = k % 7 == 6 ? nullptr : (k % 7 == 5 ? r : item);   // a non-segmented object and none too
            if (side == 0)
                invcheck::hooks_out();
            BYTE *o = at<BYTE *(THISCALL *)(void *, void *, DWORD)>(0x56a5a0)(r, arg, 0);
            if (side == 0)
                invcheck::hooks_in();
            x[side] = {};
            x[side].made = o != nullptr;
            if (o) {
                x[side] = {fl<DWORD>(o, 0), fl<DWORD>(o, 8), fl<DWORD>(o, 0x10), fl<DWORD>(o, 0x18), zs_hash(o + 0xc), zs_hash(o + 0x14), true};
                vcall<void *>(o, 1, 1);
            }
            g_seed = 991u * (unsigned)k + 7;
        }
        char d[160];
        wsprintfA(d, "[case %d: made %d / %d, name %x / %x, pieces %x / %x]", k, x[0].made, x[1].made, x[0].name, x[1].name, x[0].pieces,
                  x[1].pieces);
        t.check(!memcmp(&x[0], &x[1], sizeof x[0]), "CSegmentedInventoryDetailRenderer::Describe", d);
    }
    vcall<void *>(item, 1, 1);
    vcall<void *>(r, 1, 1);
}

// ---- Real data: the template items of the loaded item definitions ----------------------------------------------

void test_template_items(Tally &t, const Policy &p)
{
    BYTE *records = *at<BYTE **>(0x6c0c38);
    const DWORD count = *at<DWORD *>(0x6c0c3c);
    int found[NCLASSES] = {}, ran = 0;
    for (DWORD i = 0; records && i < count && i < 4096; i++) {
        if (records[i * 0x80 + 0x7f])
            continue;
        BYTE *def = *reinterpret_cast<BYTE **>(records + i * 0x80 + 0x78);
        if (!def || !readable(def, 0x20))
            continue;
        BYTE *item = *reinterpret_cast<BYTE **>(def + 0x1c);
        const CClassInfo *ci = class_of(item);
        for (int c = 0; c < NCLASSES; c++)
            if (ci && ci == *at<CClassInfo **>(CLASSES[c].def.var) && readable(item, CLASSES[c].def.size) && found[c]++ < 12) {
                g_mod.self_class = c;
                g_mod.real_self = item;
                for (const ClassSlot &s : SLOTS)
                    if (s.cls == c)
                        for (unsigned seed = 1; seed <= 3; seed++) {
                            run_slot(t, s.t, seed * 69069u + i, make_self, p);
                            ran++;
                        }
                g_mod.real_self = nullptr;
            }
    }
    log("  ai_shield real data: template items %d CSegmentedInventoryItem, %d CShieldInventoryItem, %d CShieldChargeInventoryItem; %d cases",
        found[C_SEGMENTED], found[C_SHIELD_ITEM], found[C_CHARGE], ran);
}

}  // namespace

ZAX_CHECKS(ai_shield)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally getters{"ai_shield: class getters"}, objects{"ai_shield: Create, CreateArray, destructors"}, fields{"ai_shield: RegisterFields"},
        real_fields{"ai_shield: registered fields and classes (real data)"}, slots{"ai_shield: slots in the fake world"},
        describe{"ai_shield: the segmented item's detail text"}, registered{"ai_shield: registered class infos (real data)"},
        real_slots{"ai_shield: the level's AIs and the catalogue's template items through their slots (real data)"};
    SavedBytes saved_random, saved_random2, saved_random3;
    save_bytes(saved_random, 0x7124c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random2, 0x6d06c0, GAME_RANDOM_BYTES);
    save_bytes(saved_random3, 0x712cc0, GAME_RANDOM_BYTES);
    run_classes(g_mod, hooks_ai_shield, "ai_shield", objects, getters, fields, real_fields);
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    invcheck::g_hook_table = hooks_ai_shield;
    test_describe(describe);
    describe.report();
    make_templates(g_mod);
    if (world_on()) {
        const Policy p = policy();
        run_slots(g_mod, slots, p);
        run_real(g_mod, "ai_shield", registered, real_slots, p, 30);
        test_template_items(real_slots, p);
        world_off();
    } else {
        slots.check(false, "ai_shield", "[the fake world could not be set up]");
    }
    free_templates(g_mod);
    slots.report();
    registered.report();
    real_slots.report();
    restore_bytes(saved_random3);
    restore_bytes(saved_random2);
    restore_bytes(saved_random);
    log("  ai_shield: %u ms", GetTickCount() - start);
}

}  // namespace zax
