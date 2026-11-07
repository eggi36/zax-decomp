// Checks of quest_ai.cpp against the original, in the game thread (machinery: checks_inventory_common.h;
// side 0 with all hooks of the module taken out, side 1 through the reimplementation).
//  - Classes: CPointOfIntrestAI and CWalkingUpperBodyAI: Create (ai_classes.cpp's) with this module's
//    deleting destructor, CreateArray with 0-3 elements, both class getters each, RegisterFields into a
//    fake class info, the registered descriptors and class infos (real data).
//  - CWeaponDischargedMessage's constructor on a block filled with a pattern.
//  - The slots on fakes: the AI under test made by Create (fields from the seed) or a copy of one of the
//    running level's AIs of the class (real data, at most 12 per class; the parts of the layer grid and
//    their children, where the upper bodies are); the part, its root, the
//    message, the layer, the inventory, the weapon, the sequence and the monitor list are fakes whose
//    class (slot 2) is one of six real class infos and whose slots record their calls; stand-ins record
//    the exe functions the slots reach (the dynamic property and monitor list functions, the part's
//    root, sequence name and object, the synch, CSameAsParentAI's Update, the message names, the item
//    category lookup, the inventory, the behaviour 0x5ab300 and the behaviour stack, the first character,
//    CAction::Execute, atexit). The AI's own bit functions (+0xb0) run for real on the fakes. Compared:
//    eax (only al for CWalkingUpperBodyAI's message slot, whose upper bytes can come from fnstsw), the
//    call log, the fakes and the AI afterwards (its string by text). The deleting destructors run after
//    every case under the same stand-ins (a copy of a level's AI never touches the real monitor lists).
#include "checks_inventory_common.h"
#include "checks_ai_world.h"
#include "x87.h"

namespace zax {

int hooks_quest_ai(Hook *out);

namespace {

using namespace invcheck;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE &B(void *p, DWORD off) { return static_cast<BYTE *>(p)[off]; }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }
unsigned text_hash(const char *s) { return s ? fnv_text(2166136261u, s) : 0x0badc0deu; }

// ---- Fakes -------------------------------------------------------------------------------------------------

constexpr int NFAKE = 16;
struct alignas(16) Fake {
    DWORD vftable;
    DWORD id;
    BYTE body[0x1f8];
};
Fake g_fake[NFAKE];
void *g_fvt[128];
CClassInfo *g_fake_class[NFAKE];
CClassInfo *g_classes[6];   // CObject, CEntityWalking, CEntityAnimated, CEntityAnimatedUpperBody, CEntityCharacter, CAction
enum { F_PART, F_ROOT, F_MSG, F_LAYER, F_ACTION, F_INV, F_WEAPON, F_SEQ, F_BEHAVIOUR, F_LIST, F_OWNER, F_ROOT2, F_OTHER, F_SAVE };
BYTE *fake(int i) { return reinterpret_cast<BYTE *>(&g_fake[i]); }

BYTE *g_self;   // the AI under test (a pool block of its side)
// A pointer for the log: fakes by number, the AI by a tag, null, others as they are (fake + offset too)
DWORD fid(const void *p)
{
    if (p && p == g_self)
        return 0x5e1f0000u;
    const DWORD a = ptrv(p), b = ptrv(g_fake);
    if (a >= b && a < b + sizeof g_fake)
        return 0xf0000000u + (a - b);
    return a;
}

unsigned g_answer_seed;
DWORD ans()
{
    g_answer_seed = g_answer_seed * 1103515245u + 12345u;
    return g_answer_seed >> 4;
}
DWORD ans_bool() { const DWORD a = ans(); return (a & 0xffff0000u) | ((a >> 7) & 1); }

// Fake slots
void *THISCALL f_delete(Fake *self, DWORD flags) { ev(0x9f1, fid(self), flags); return self; }
CClassInfo *THISCALL f_class(Fake *self) { return g_fake_class[self->id % NFAKE]; }
DWORD THISCALL f_bad(Fake *self) { ev(0x9ff, fid(self)); return 0; }
BYTE *THISCALL f_item(Fake *self, DWORD i) { ev(0x9a1, fid(self), i); return ans() % 4 ? fake(F_WEAPON) : nullptr; }
ZString g_sequences[4];
void *THISCALL f_discharge(Fake *self, void *part) { ev(0x9a2, fid(self), fid(part)); return &g_sequences[ans() % 4]; }
const DWORD READY[] = {0, 0x80000000u, 0x3f000000u, 0xbf800000u, 0x7fc00000u, 0x41200000u};
long double THISCALL f_ready(Fake *self) { ev(0x9a3, fid(self)); return fld(raw_float(READY[ans() % 6])); }
const DWORD FRAMES[] = {0, 0x3f800000u, 0x40e00000u, 0x41000000u, 0x40f00000u, 0x7fc00000u, 0xbf800000u, 0x4f800000u};
long double THISCALL f_frame(Fake *self) { ev(0x9a4, fid(self)); return fld(raw_float(FRAMES[ans() % 8])) + (long double)(ans() % 3) * 0.25L; }
DWORD THISCALL f_warp(Fake *self, CClassInfo *c) { ev(0x9a5, fid(self), ptrv(c)); const DWORD a = ans(); return a % 5 ? 0 : a; }
void THISCALL f_turn(Fake *self, DWORD angle, DWORD flag) { ev(0x9a6, fid(self), angle, flag); }

// ---- Stand-ins ---------------------------------------------------------------------------------------------

void *g_made[8];   // the monitor lists the original pool gave on a side (freed after it)
int g_nmade;
DWORD THISCALL s_dyn_is_in(void *desc, DWORD obj) { ev(0x801, ptrv(desc), fid(at<void *>(obj))); return ans_bool(); }
void THISCALL s_dyn_set(void *desc, DWORD obj, void *p) { ev(0x802, ptrv(desc), fid(at<void *>(obj)), p != nullptr); }
void *THISCALL s_dyn_ptr(void *desc, DWORD obj) { ev(0x803, ptrv(desc), fid(at<void *>(obj))); return ans() % 6 ? fake(F_LIST) : fake(F_OTHER); }
DWORD THISCALL s_dyn_remove(void *desc, DWORD obj) { ev(0x804, ptrv(desc), fid(at<void *>(obj))); return ans(); }
void *THISCALL s_monitor_ctor(void *p)
{
    ev(0x805);
    if (g_nmade < 8)
        g_made[g_nmade++] = p;
    return p;
}
DWORD THISCALL s_monitor_add(void *l, void *part, DWORD w, DWORD h) { ev(0x806, l ? (g_nmade && l == g_made[g_nmade - 1] ? 1u : fid(l)) : 0, fid(part), w ^ (h << 8)); return ans(); }
DWORD THISCALL s_monitor_remove(void *l, void *part) { ev(0x807, fid(l), fid(part)); return ans(); }
BYTE *THISCALL s_root(void *part) { ev(0x808, fid(part)); return ans() % 5 ? fake(F_ROOT) : fake(F_ROOT2); }
const char *const SEQUENCES[] = {"Death", "Shoot1", "shoot1", "Walk", "Fire Big", "", "DEATH"};
const char *THISCALL s_seq_name(void *e) { ev(0x809, fid(e)); return SEQUENCES[ans() % 7]; }
BYTE *THISCALL s_seq_object(void *e) { ev(0x80a, fid(e)); return fake(F_SEQ); }
DWORD THISCALL s_synch(void *e, DWORD on) { ev(0x80b, fid(e), on); return ans(); }
DWORD THISCALL s_parent_update(void *self, void *part, DWORD seconds) { ev(0x80c, fid(self), fid(part), seconds); return ans(); }
DWORD THISCALL s_named(void *msg, const char *name) { ev(0x80d, fid(msg), text_hash(name)); return ans_bool(); }
DWORD THISCALL s_find(void *list, const char *name, int hint) { ev(0x80e, ptrv(list), text_hash(name), (DWORD)hint); return ans() % 5; }
BYTE *THISCALL s_inventory(void *e) { ev(0x80f, fid(e)); return fake(F_INV); }
DWORD THISCALL s_cur_index(void *inv, DWORD cat) { ev(0x810, fid(inv), cat); return ans() % 4; }
BYTE *__stdcall s_behaviour(DWORD angle) { ev(0x811, angle); return fake(F_BEHAVIOUR); }
DWORD THISCALL s_push(void *stack, void *e, void *b, DWORD priority) { ev(0x812, fid(stack), fid(e), fid(b) ^ (priority << 24)); return ans(); }
BYTE *THISCALL s_first(void *) { ev(0x813); return ans() % 3 ? fake(F_ROOT) : fake(F_OTHER); }
DWORD THISCALL s_execute(void *action, void *layer, void *a2, void *a3, void *a4, void *self, void **field)
{
    ev(0x814, fid(action), fid(layer), fid(a2));
    ev(0x815, fid(a3), fid(a4), fid(self) ^ fid(field));
    if (ans() % 3 == 0)
        *field = nullptr;   // the action deleted itself
    return ans();
}
int __cdecl s_atexit(DWORD f) { ev(0x816, f); return 0; }

struct Stand {
    DWORD address;
    void *fn;
};
const Stand STANDS[] = {
    {0x418fc0, (void *)&s_dyn_is_in},    {0x4187c0, (void *)&s_dyn_set},       {0x418790, (void *)&s_dyn_ptr},
    {0x418fb0, (void *)&s_dyn_remove},   {0x4e4f40, (void *)&s_monitor_ctor},  {0x4e4f70, (void *)&s_monitor_add},
    {0x4e5000, (void *)&s_monitor_remove}, {0x4fc6f0, (void *)&s_root},        {0x48aee0, (void *)&s_seq_name},
    {0x48b590, (void *)&s_seq_object},   {0x48fb50, (void *)&s_synch},         {0x562010, (void *)&s_parent_update},
    {0x506620, (void *)&s_named},        {0x523df0, (void *)&s_find},          {0x4267e0, (void *)&s_inventory},
    {0x425290, (void *)&s_cur_index},    {0x5ab300, (void *)&s_behaviour},     {0x49a560, (void *)&s_push},
    {0x4f5d80, (void *)&s_first},        {0x41a720, (void *)&s_execute},       {0x5d0863, (void *)&s_atexit},
};
constexpr int NSTANDS = sizeof STANDS / sizeof STANDS[0];
CodePatch g_patches[NSTANDS];
void stands_on()
{
    for (int i = 0; i < NSTANDS; i++)
        patch_jump(g_patches[i], STANDS[i].address, STANDS[i].fn);
}
void stands_off()
{
    for (int i = NSTANDS - 1; i >= 0; i--)
        unpatch(g_patches[i]);
}

// ---- The world of a case -------------------------------------------------------------------------------------

constexpr DWORD MSG_DEACTIVATE = 0x6c29e8, MSG_DISCHARGED = 0x713df8, MSG_FIRE = 0x713c3c;
const DWORD ANGLES[] = {0, 0x3f800000u, 0x40490fdbu, 0xc0490fdbu, 0x40c90fdbu, 0x3f490fdbu, 0x3f490fdcu, 0x7fc00000u, 0xc0a00000u, 0x40400000u};

void build_fakes(unsigned seed)
{
    g_seed = seed;
    for (int i = 0; i < NFAKE; i++) {
        Fake &f = g_fake[i];
        f.vftable = ptrv(g_fvt);
        f.id = (DWORD)i;
        for (DWORD k = 0; k < sizeof f.body; k += 4)
            D(f.body, k) = random();
        g_fake_class[i] = g_classes[0];
    }
    // the part an upper body or an animated entity, the roots walkers (CWalkingUpperBodyAI's slots call
    // through both without a test, as the original); F_OTHER a plain object
    g_fake_class[F_PART] = g_classes[rnd(6) ? 3 : 2];
    g_fake_class[F_ROOT] = g_classes[1];
    g_fake_class[F_ROOT2] = g_classes[1];
    g_fake_class[F_ACTION] = g_classes[5];
    // every fake's layer (+0x48) and its save data's monitor list owner, the action at the layer's +0xc8
    for (int i = 0; i < NFAKE; i++)
        D(fake(i), 0x48) = ptrv(fake(F_LAYER));
    D(fake(F_LAYER), 0x3c + 0x164) = ptrv(fake(F_OWNER));
    D(fake(F_LAYER), 0xc8) = rnd(3) ? ptrv(fake(F_ACTION)) : 0;
    D(fake(F_LIST), 0x10) = rnd(3) ? rnd(3) : 0;
    D(fake(F_SEQ), 0x48) = rnd(4) ? rnd(12) : random();
    const int movers[3] = {F_ROOT, F_ROOT2, F_PART};
    for (int i : movers) {
        D(fake(i), 0xb0) = random();
        D(fake(i), 0xc4) = ANGLES[rnd(10)];
        D(fake(i), 0xc8) = rnd(3) ? ANGLES[rnd(10)] : random();
    }
    // the message: deactivate, discharged (from the root or another), fire primary or another id
    BYTE *m = fake(F_MSG);
    const DWORD ids[] = {*at<DWORD *>(MSG_DEACTIVATE), *at<DWORD *>(MSG_DISCHARGED), *at<DWORD *>(MSG_FIRE), random()};
    D(m, 4) = ids[rnd(4)];
    D(m, 0xc) = rnd(3) ? ptrv(fake(F_ROOT)) : ptrv(fake(F_ROOT2));
    B(m, 8) = (BYTE)rnd(2);
    g_answer_seed = random();
}
unsigned fakes_state() { return fnv(2166136261u, g_fake, sizeof g_fake); }

// The AI's bytes, its string at +0x18 by text (CWalkingUpperBodyAI); the dynamic properties pointer +4 only
// whether it is set (a copy has a block of its own); the padding bytes no constructor or copy writes
// (CSameAsParentAI's +0x12/+0x13 after its two flags, CPointOfIntrestAI's +0x19-+0x1b after its flag: the
// pool block's old contents, different for the two copies of a level's AI that live at the same time) as 0
unsigned self_state(BYTE *self, DWORD size, bool string18)
{
    BYTE b[0x1c];
    memcpy(b, self, sizeof b);
    D(b, 4) = D(b, 4) != 0;
    if (string18)
        b[0x12] = b[0x13] = 0;
    else
        b[0x19] = b[0x1a] = b[0x1b] = 0;
    unsigned h = fnv(2166136261u, b, string18 ? 0x18 : size);
    if (string18)
        h = fnv_text(h, reinterpret_cast<ZString *>(self + 0x18)->c_str());
    return h;
}

void fill_poi(BYTE *s)
{
    D(s, 0x10) = rnd(3) ? ptrv(fake(F_PART)) : rnd(2) ? ptrv(fake(F_OTHER)) : 0;
    D(s, 0x14) = ptrv(fake(F_SAVE)) + rnd(4) * 4;
    B(s, 0x18) = (BYTE)rnd(2);
}
void fill_wub(BYTE *s)
{
    const DWORD holds[] = {0, 0x3e4ccccdu, 0x3f800000u, 0xbf000000u, 0x7fc00000u, 0x3c23d70au, 0x40a00000u};
    D(s, 0x14) = holds[rnd(7)];
    const char *seq[] = {"Shoot1", "Walk", "shoot1", "", "Fire Big"};
    at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(s + 0x18, seq[rnd(5)]);
}

enum Slot { S_POI_BEGIN, S_POI_MESSAGE, S_POI_DTOR, S_WUB_UPDATE, S_WUB_MESSAGE, NSLOTS };
const char *const SLOT_NAMES[NSLOTS] = {"CPointOfIntrestAI::Begin 0x508b50", "CPointOfIntrestAI::Message 0x508be0",
                                         "CPointOfIntrestAI::DeletingDtor 0x5087f0 (in the list)", "CWalkingUpperBodyAI::Update 0x595ff0",
                                         "CWalkingUpperBodyAI::Message 0x596160"};
const DWORD SECONDS[] = {0x3d088889u, 0, 0x3f800000u, 0xbf800000u, 0x7fc00000u, 0x3e4ccccdu, 0x41200000u};

BYTE g_self_bytes[2][0x1c];
// One case: the AI made by Create (or copied from `real`), filled from the seed unless real, the slot run
// under the stand-ins, the state taken, the AI destroyed under them too
void test_slot(Tally &t, Slot s, unsigned seed, BYTE *const *real)
{
    const bool poi = s <= S_POI_DTOR;
    const DWORD size = 0x1c;
    DWORD eax[2], state[2], self_h[2], guard_calls[2];
    for (int side = 0; side < 2; side++) {
        build_fakes(seed);
        log_reset(side);
        side_begin(side);
        // the function-local "Shoot1" of 0x596160: put back afterwards when a side made it
        const BYTE guard = *at<BYTE *>(0x713c38);
        const DWORD shoot1 = *at<DWORD *>(0x713c40);
        if (real) {
            g_self = real[side];
        } else {
            g_self = static_cast<BYTE *>(at<void *(__cdecl *)()>(poi ? 0x508730 : 0x595cd0)());
            if (poi)
                fill_poi(g_self);
            else
                fill_wub(g_self);
        }
        g_nmade = 0;
        stands_on();
        // the part: the fake part, another object (not for the update, which calls through the part)
        const DWORD a = ptrv(fake(s == S_WUB_UPDATE || rnd(4) ? F_PART : F_OTHER));
        switch (s) {
        case S_POI_BEGIN: eax[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x508b50)(g_self, a); break;
        case S_POI_MESSAGE: eax[side] = at<DWORD(THISCALL *)(void *, DWORD, void *)>(0x508be0)(g_self, a, fake(F_MSG)); break;
        case S_POI_DTOR:
            B(g_self, 0x18) = 1;
            eax[side] = 0;
            break;
        case S_WUB_UPDATE: eax[side] = at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x595ff0)(g_self, a, SECONDS[rnd(7)]); break;
        default: eax[side] = at<DWORD(THISCALL *)(void *, DWORD, void *)>(0x596160)(g_self, a, fake(F_MSG)) & 0xff; break;
        }
        self_h[side] = self_state(g_self, size, !poi);
        memcpy(g_self_bytes[side], g_self, size);
        if (poi)
            g_self_bytes[side][0x19] = g_self_bytes[side][0x1a] = g_self_bytes[side][0x1b] = 0;
        else
            g_self_bytes[side][0x12] = g_self_bytes[side][0x13] = 0;
        state[side] = fakes_state();
        // destroyed under the stand-ins (slot 1 with 1: the pool block freed)
        ev(0x9e0, fid(vcall<void *>(g_self, 1, 1)) == 0x5e1f0000u);
        stands_off();
        guard_calls[side] = *at<BYTE *>(0x713c38) & 1;
        if (!(guard & 1) && (*at<BYTE *>(0x713c38) & 1)) {
            at<void(THISCALL *)(void *)>(0x4defd0)(at<void *>(0x713c40));
            *at<BYTE *>(0x713c38) = guard;
            *at<DWORD *>(0x713c40) = shoot1;
        }
        for (int i = 0; i < g_nmade; i++)
            pool_free(g_made[i], 0x18);
        g_self = nullptr;
        side_end(side);
    }
    DWORD at_ = 0;
    while (at_ + 4 < size && D(g_self_bytes[0], at_) == D(g_self_bytes[1], at_))
        at_ += 4;
    char d[240];
    wsprintfA(d, "[seed %u%s: eax %x / %x, AI %x / %x (first different dword +%x: %x / %x), fakes %x / %x, log %d / %d, static %u / %u]", seed,
              real ? " (level)" : "", eax[0], eax[1], self_h[0], self_h[1], at_, D(g_self_bytes[0], at_), D(g_self_bytes[1], at_), state[0], state[1],
              g_nlog[0], g_nlog[1], guard_calls[0], guard_calls[1]);
    const bool same = eax[0] == eax[1] && self_h[0] == self_h[1] && state[0] == state[1] && same_log() && guard_calls[0] == guard_calls[1];
    if (!same)
        log_first_difference();
    t.check(same, SLOT_NAMES[s], d);
}

// CWeaponDischargedMessage's constructor 0x599300 on a block filled with a pattern
void test_message(Tally &t, unsigned seed)
{
    BYTE obj[2][0x14];
    DWORD eax[2];
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        side_begin(side);
        memset(obj[side], (BYTE)random(), 0x14);
        const DWORD e = random(), i = random();
        eax[side] = at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x599300)(obj[side], e, i) == ptrv(obj[side]);
        side_end(side);
    }
    char d[120];
    wsprintfA(d, "[seed %u: this %u / %u, first difference +%x]", seed, eax[0], eax[1], g_diff_at);
    t.check(eax[0] == eax[1] && eax[0] && same_region(obj[0], obj[1], 0x14), "CWeaponDischargedMessage::Ctor", d);
}

const ClassDef CLASSES[] = {
    {"CPointOfIntrestAI", 0x1c, 0x6c2dc4, 0x508730, 0x508780, 0x5089b0, {0x5086e0, 0x508950}, true},
    {"CWalkingUpperBodyAI", 0x1c, 0x713c48, 0x595cd0, 0x595d20, 0x595ee0, {0x595c80, 0x595e80}, true},
};

void init_tables()
{
    for (void *&v : g_fvt)
        v = (void *)&f_bad;
    g_fvt[1] = (void *)&f_delete;
    g_fvt[2] = (void *)&f_class;
    g_fvt[26] = (void *)&f_item;
    g_fvt[35] = (void *)&f_discharge;
    g_fvt[39] = (void *)&f_ready;
    g_fvt[84] = (void *)&f_frame;
    g_fvt[86] = (void *)&f_warp;
    g_fvt[104] = (void *)&f_turn;
    const DWORD getters[6] = {0x4163b0, 0x49b810, 0x48dcc0, 0x494180, 0x49a930, 0x41a650};
    for (int i = 0; i < 6; i++)
        g_classes[i] = at<CClassInfo *(__cdecl *)()>(getters[i])();
    const char *texts[4] = {"Shoot1", "Shoot Big", "", "shoot1"};
    for (int i = 0; i < 4; i++) {
        if (!g_sequences[i].buffer)
            at<ZString *(THISCALL *)(void *)>(0x4dec90)(&g_sequences[i]);
        at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(&g_sequences[i], texts[i]);
    }
}

// The running level's AIs of the two classes: copies (Object_Clone, the original's, hooks in) for each
// side, through the slots on the fakes
BYTE *g_parts[8192];
BYTE *g_ais[256];
void test_level(Tally &t)
{
    int nparts = aiw::level_parts(g_parts, 8192);
    // and the parts' children (an entity's upper body is a child of the walker, not in the layer grid):
    // the child list at +8, its parts +0xc, count +0x10, two levels
    for (int p = 0, end = nparts, level = 0; level < 2; level++, p = end, end = nparts)
        for (; p < end; p++) {
            const BYTE *list = *reinterpret_cast<BYTE *const *>(g_parts[p] + 8);
            if (!list || !aiw::readable(list, 0x14))
                continue;
            const DWORD count = *reinterpret_cast<const DWORD *>(list + 0x10);
            BYTE *const *children = *reinterpret_cast<BYTE *const *const *>(list + 0xc);
            if (count > 64 || !aiw::readable(children, count * 4))
                continue;
            for (DWORD c = 0; c < count && nparts < 8192; c++)
                if (children[c] && aiw::readable(children[c], 0x48))
                    g_parts[nparts++] = children[c];
        }
    int found[2] = {0, 0};
    CClassInfo *const poi = *at<CClassInfo **>(0x6c2dc4), *const wub = *at<CClassInfo **>(0x713c48);
    for (int p = 0; p < nparts; p++) {
        const int n = aiw::part_ais(g_parts[p], g_parts, nparts, g_ais, 256);
        for (int k = 0; k < n; k++) {
            CClassInfo *c = aiw::class_of(g_ais[k]);
            const int which = c == poi ? 0 : c == wub ? 1 : -1;
            if (which < 0 || found[which] >= 12)
                continue;
            found[which]++;
            const Slot slots[2][3] = {{S_POI_BEGIN, S_POI_MESSAGE, S_POI_DTOR}, {S_WUB_UPDATE, S_WUB_MESSAGE, S_WUB_MESSAGE}};
            for (int j = 0; j < 3; j++)
                for (unsigned seed = 0; seed < 4; seed++) {
                    BYTE *copies[2];
                    for (BYTE *&cp : copies)
                        cp = static_cast<BYTE *>(at<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(g_ais[k], 0, 0xffffffff));
                    if (!copies[0] || !copies[1]) {
                        t.check(false, "level AI", "[no copy]");
                        return;
                    }
                    test_slot(t, slots[which][j], 0x700000u + (unsigned)(p * 64 + k * 8) + seed, copies);
                }
        }
    }
    log("  level AIs: %d CPointOfIntrestAI, %d CWalkingUpperBodyAI (copies)", found[0], found[1]);
}

}  // namespace

ZAX_CHECKS(quest_ai)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_quest_ai;
    if (!arena_setup()) {
        log("Check quest AI: no arena");
        return;
    }
    init_tables();
    Tally objects{"quest AI: Create, CreateArray, destructors, the message constructor"}, getters{"quest AI: class getters"},
        fields{"quest AI: RegisterFields"}, registered{"quest AI: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    for (unsigned s = 0; s < (unsigned)scaled(200); s++)
        test_message(objects, s);
    objects.report();
    getters.report();
    fields.report();
    registered.report();
    Tally slots{"quest AI: the slots (fakes)"};
    for (int s = 0; s < NSLOTS; s++) {
        log("  %s", SLOT_NAMES[s]);
        for (unsigned k = 0; k < (unsigned)scaled(600); k++)
            test_slot(slots, (Slot)s, 0x100000u * (unsigned)(s + 1) + k, nullptr);
    }
    slots.report();
    Tally level{"quest AI: the level's AIs of the two classes through their slots (real data, copies)"};
    test_level(level);
    level.report();
}

}  // namespace zax
