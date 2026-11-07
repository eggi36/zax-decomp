// Checks of ambient_sound.cpp against the original, in the game thread.
//
//  - The class (checks_inventory_common.h): both getter copies, Create/CreateArray and the deleting
//    destructor, RegisterFields and the registered descriptors.
//  - The slots in the sound fake world (checks_sound_common.h): CAmbientSound's Update, SetWhoStarted and
//    message, the layer's add, remove, start, distances and save data forward, on objects built in each
//    side's arena from the case seed: sounds with and without a resource, part, layer, mute and inner
//    state, listener positions inside, between and beyond the radii and the resource's range (also NaN),
//    the current layer equal or not. The sound list, the resources, the cache, the layer parts' listeners
//    and positions are recorders.
//  - Real data: the CAmbientSounds in the running level's layers, as copies (their radii, indices, flags
//    and layer) with the real listener position, current layer and part positions, through Update. Only
//    CAmbientSoundAI adds them, which three levels use (01 Main, 22 End Game Jungle twice); the check maps
//    have none.
#include "checks_sound_common.h"
#include "checks_inventory_common.h"
#include "checks_ai_world.h"

namespace zax {

int hooks_ambient_sound(Hook *out);

namespace {

using namespace sndc;

const invcheck::ClassDef CLASSES[] = {
    {"CAmbientSound", 0x2c, 0x6c1d10, 0x4edac0, 0x4edb00, 0x4edcd0, {0x4eda70, 0x4edc70}, true},
};

constexpr DWORD VT_AMBIENT = 0x5f8dec, CURRENT_LAYER = 0x6c1c70;
enum { K_RES, K_ARRAY };
enum { F_CACHE, F_RES, N_RES = 4, F_PART = F_RES + N_RES, N_PART = 3 };
constexpr DWORD LAYER_A = 0x00b0a000, LAYER_B = 0x00b0b000;   // two layer values (never dereferenced)

// ---- Recorders --------------------------------------------------------------------------------------------

DWORD c_sound(Call &c)
{
    (void)c;
    const DWORD v = nv();
    return v % 5 == 0 ? 0 : ptrv(fake(F_RES + (int)(v % N_RES)));
}
DWORD c_cache(Call &) { return ptrv(fake(F_CACHE)); }
DWORD c_factor(Call &c)
{
    c.fret = nfloat(0.25f, 2.0f);
    c.fset = true;
    return nv();
}
DWORD c_range(Call &c)
{
    c.fret = nfloat(0.0f, 1500.0f);
    c.fset = true;
    return nv();
}
// LayerPart_Position (out): a fake part with a real position (real data) gives it, else values of the case
DWORD c_position(Call &c)
{
    float *out = reinterpret_cast<float *>(c.a[0]);
    BYTE *part = reinterpret_cast<BYTE *>(c.ecx);
    if (part >= fake(0) && part < fake(NFAKE - 1) + FAKE_SIZE && dw(part, 0x80) == 1) {
        dw(out, 0) = dw(part, 0x84);
        dw(out, 4) = dw(part, 0x88);
    } else {
        out[0] = (float)nfloat(-500.0f, 4000.0f);
        out[1] = (float)nfloat(-500.0f, 4000.0f);
    }
    return c.a[0];
}

const Spec DIRECT[] = {
    {0x57e8e0, "SoundList_Get", 't', "v", 'c', c_sound},
    {0x57bbc0, "Resource_SetKeep", 't', "v", 'n', nullptr},
    {0x57bc50, "Resource_SetType", 't', "v", 'n', nullptr},
    {0x57bb60, "Resource_SetFlag20", 't', "v", 'n', nullptr},
    {0x57b740, "Resource_CheckValid", 't', "", 'b', nullptr},
    {0x57b290, "Resource_ReloadThen", 't', "qv", 'n', nullptr},
    {0x57b730, "Resource_GetF3c", 't', "", 'c', c_range},
    {0x57aef0, "ResourceCache_Get", 'c', "", 'c', c_cache},
    {0x57af60, "ResourceCache_Release", 't', "p", 'n', nullptr},
    {0x57c200, "SoundManager_DistanceFactor", 't', "", 'c', c_factor},
    {0x46cd50, "CObject_OnMessage", 't', "vs", 'n', nullptr},
    {0x4fc1b0, "LayerPart_AddListener", 't', "pvs", 'n', nullptr},
    {0x4fc200, "LayerPart_RemoveListener", 't', "pvs", 'n', nullptr},
    {0x4fb0a0, "LayerPart_Position", 't', "-", 'c', c_position},
    {0x4e76e0, "CLayerSaveData::OnPartFlags", 't', "vvv", 'n', nullptr},
};
const Spec RES_SLOTS[] = {
    {0, nullptr, 0, "", 0, nullptr},
    {0, nullptr, 0, "", 0, nullptr},
    {0, "Resource::Insert", 't', "v", 'b', nullptr},
    {0, "SoundResource::SetFlag2", 't', "v", 'b', nullptr},
    {0, "SoundResource::SetPosition", 't', "qv", 'b', nullptr},
    {0, "SoundResource::SetRange", 't', "ffv", 'b', nullptr},
    {0, nullptr, 0, "", 0, nullptr}, {0, nullptr, 0, "", 0, nullptr}, {0, nullptr, 0, "", 0, nullptr},
    {0, nullptr, 0, "", 0, nullptr}, {0, nullptr, 0, "", 0, nullptr},
    {0, "SoundResource::Stop", 't', "", 'b', nullptr},
};
DWORD c_add(Call &) { return nv() % 6; }
const Spec ARRAY_SLOTS[] = {
    {0, nullptr, 0, "", 0, nullptr},
    {0, nullptr, 0, "", 0, nullptr},
    {0, "PtrArray::Add", 't', "d", 'c', c_add},
};
const Standins STANDINS[] = {{DIRECT, sizeof DIRECT / sizeof DIRECT[0]}};
const Module MODULE = {hooks_ambient_sound, nullptr, 0};

// ---- The objects of a case --------------------------------------------------------------------------------

DWORD small_float(Gen &g, float lo, float hi) { return g.fbits_(lo, hi); }
void build_fakes(Gen &g)
{
    memset(&g_template, 0, sizeof g_template);
    make_fake(F_CACHE, K_ARRAY);
    for (int i = 0; i < N_RES; i++)
        make_fake(F_RES + i, K_RES);
    for (int i = 0; i < N_PART; i++) {
        BYTE *p = tmpl(F_PART + i);
        memset(p, 0, FAKE_SIZE);
        dw(p, 0x48) = g.next() % 4 ? LAYER_A : g.next() % 2 ? LAYER_B : 0;
    }
}
// A CAmbientSound in the running side's arena from the case seed (or a real one's copy)
BYTE *make_sound(Gen &g, const BYTE *real = nullptr)
{
    BYTE *s = alloc(0x2c);
    if (real) {
        memcpy(s, real, 0x2c);
        if (dw(s, 8))
            dw(s, 8) = ptrv(fake(F_RES));
        if (dw(s, 0x10))
            dw(s, 0x10) = ptrv(fake(F_PART));
        dw(s, 4) = 0;
        return s;
    }
    dw(s, 0) = VT_AMBIENT;
    dw(s, 4) = 0;
    dw(s, 8) = g.next() % 4 ? ptrv(fake(F_RES + (int)g.below(N_RES))) : 0;
    dw(s, 0xc) = g.next() % 5 ? g.below(40) : 0xffffffff;
    dw(s, 0x10) = g.next() % 4 ? ptrv(fake(F_PART + (int)g.below(N_PART))) : 0;
    dw(s, 0x14) = g.next() % 3 ? LAYER_A : g.next() % 2 ? LAYER_B : 0;
    fl<BYTE>(s, 0x18) = (BYTE)(g.next() % 2);
    fl<BYTE>(s, 0x19) = (BYTE)(g.next() % 3);
    fl<BYTE>(s, 0x1a) = (BYTE)(g.next() % 2);
    fl<BYTE>(s, 0x1b) = (BYTE)g.next();
    const DWORD inner = small_float(g, 0.0f, 300.0f);
    dw(s, 0x1c) = inner;
    dw(s, 0x20) = g.next() % 4 ? bits(fbits(inner) * fbits(inner)) : small_float(g, 0.0f, 90000.0f);
    dw(s, 0x24) = small_float(g, 0.0f, 900.0f);
    dw(s, 0x28) = small_float(g, 0.0f, 1500.0f);
    return s;
}
// A layer in the arena: +0xf0 the array (recording vtable), +0xf4 the entries, +0xf8 the count
BYTE *make_layer(Gen &g, int count, bool all_sounds)
{
    BYTE *l = alloc(0x100);
    dw(l, 0xf0) = ptrv(g_vt[K_ARRAY]);
    BYTE **e = reinterpret_cast<BYTE **>(alloc(4 * (DWORD)(count + 1)));
    for (int i = 0; i < count; i++)
        e[i] = all_sounds || g.next() % 3 ? make_sound(g) : nullptr;
    dw(l, 0xf4) = ptrv(e);
    dw(l, 0xf8) = (DWORD)count;
    dw(l, 0xfc) = (DWORD)(count + 4);
    return l;
}

// ---- Cases ------------------------------------------------------------------------------------------------

enum What { W_UPDATE, W_WHO, W_MESSAGE, W_ADD, W_REMOVE, W_START, W_DISTANCES, W_FLAGS, NWHAT };
const char *const NAMES[] = {"CAmbientSound::Update",           "CAmbientSound::SetWhoStarted",   "CAmbientSound::OnMessage",
                             "CLayer::AddAmbientSound",         "CLayer::RemoveAmbientSound",     "CLayer::StartAmbientSound",
                             "CLayer::SetAmbientDistances",     "CLayer::OnPartFlags"};
const DWORD ADDRESSES[] = {0x4ee100, 0x4ee4a0, 0x4ee440, 0x4ee4e0, 0x4ee610, 0x4ee650, 0x4ee6e0, 0x4ee730};
const char *const MESSAGES[] = {"Delete", "DELETE", "Leaving Scope", "leaving scope", "Died", ""};

struct Real {
    const BYTE *sound;     // a copy of a real CAmbientSound
    DWORD part_layer, pos[2], listener[3], current;
};
const Real *g_real;

void run_case(Tally &t, What w, unsigned seed)
{
    Result r{};
    const DWORD saved_layer = *at<DWORD *>(CURRENT_LAYER);
    Gen g0{seed};
    build_fakes(g0);
    if (g_real) {
        BYTE *p = tmpl(F_PART);
        dw(p, 0x48) = g_real->part_layer;
        dw(p, 0x80) = 1;
        dw(p, 0x84) = g_real->pos[0];
        dw(p, 0x88) = g_real->pos[1];
    }
    for (int side = 0; side < 2; side++) {
        Gen g{seed * 31u + 7u};
        side_begin(side, seed, MODULE, (BYTE)(0x5a + seed % 3));
        *at<DWORD *>(CURRENT_LAYER) = g_real ? g_real->current : g.next() % 4 ? LAYER_A : LAYER_B;
        DWORD a[4] = {0, 0, 0, 0};
        DWORD ecx = 0;
        int nargs = 0;
        switch (w) {
        case W_UPDATE: {
            BYTE *s = make_sound(g, g_real ? g_real->sound : nullptr);
            float *l = reinterpret_cast<float *>(alloc(12));
            if (g_real) {
                memcpy(l, g_real->listener, 12);
            } else {
                dw(l, 0) = small_float(g, -500.0f, 4000.0f);
                dw(l, 4) = g.next() % 4 ? 0 : small_float(g, -50.0f, 50.0f);
                dw(l, 8) = small_float(g, -4000.0f, 500.0f);
            }
            ecx = ptrv(s);
            a[0] = ptrv(l);
            nargs = 1;
            break;
        }
        case W_WHO:
            ecx = ptrv(make_sound(g));
            a[0] = g.next() % 4 ? ptrv(fake(F_PART + (int)g.below(N_PART))) : 0;
            nargs = 1;
            break;
        case W_MESSAGE: {
            BYTE *s = make_sound(g);
            const DWORD k = g.below(6);
            if ((k == 2 || k == 3) && !dw(s, 0x10))
                dw(s, 0x10) = ptrv(fake(F_PART));   // "Leaving Scope" stops listening without a null test
            ecx = ptrv(s);
            a[0] = g.next();
            a[1] = ptrv(MESSAGES[k]);
            nargs = 2;
            break;
        }
        case W_ADD:
            ecx = ptrv(make_layer(g, (int)g.below(5), false));
            a[0] = g.below(30);
            a[1] = g.next() % 4 ? ptrv(fake(F_PART + (int)g.below(N_PART))) : 0;
            nargs = 2;
            break;
        case W_REMOVE:
        case W_START: {
            const int count = (int)g.below(5);
            ecx = ptrv(make_layer(g, count, false));
            a[0] = g.next() % 6 == 0 ? 0xffffffff : g.below((DWORD)count + 2);
            nargs = 1;
            break;
        }
        case W_DISTANCES: {
            const int count = 1 + (int)g.below(4);
            ecx = ptrv(make_layer(g, count, true));
            a[0] = g.next() % 6 == 0 ? 0xffffffff : g.below((DWORD)count);
            a[1] = small_float(g, 0.0f, 300.0f);
            a[2] = small_float(g, 0.0f, 900.0f);
            a[3] = small_float(g, 0.0f, 1500.0f);
            nargs = 4;
            break;
        }
        case W_FLAGS:
            ecx = ptrv(make_layer(g, 0, false));
            a[0] = g.next();
            a[1] = g.next();
            a[2] = g.next();
            nargs = 3;
            break;
        default: break;
        }
        r.eax[side] = call_fn(ADDRESSES[w], ecx, 0, nargs, a, false, nullptr);   // side 0: the hooks are out
        side_end();
    }
    *at<DWORD *>(CURRENT_LAYER) = saved_layer;
    // void functions leave what the last call left in eax
    const DWORD mask = w == W_ADD || w == W_MESSAGE || w == W_FLAGS ? 0xffffffff : 0;
    compare(t, NAMES[w], seed, r, mask);
}

void test_slots(Tally &t)
{
    const int COUNTS[NWHAT] = {1500, 200, 200, 300, 200, 300, 200, 50};
    for (int w = 0; w < NWHAT; w++)
        for (int k = 0; k < scaled(COUNTS[w]); k++)
            run_case(t, (What)w, 0x1000u * (unsigned)(w + 1) + (unsigned)k);
}

// ---- Real data ----------------------------------------------------------------------------------------------

BYTE *g_parts[8192];
BYTE g_copies[48][0x2c];
Real g_reals[48];

int collect_real()
{
    BYTE **parts = g_parts;
    const int n = aiw::level_parts(parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0)
        return -1;
    const CClassInfo *cls = *at<CClassInfo **>(0x6c1d10);
    DWORD layers[16];
    int nlayers = 0, found = 0;
    for (int i = 0; i < n; i++) {
        const DWORD l = dw(parts[i], 0x48);
        bool known = false;
        for (int k = 0; k < nlayers; k++)
            known |= layers[k] == l;
        if (!known && nlayers < 16 && readable(at<void *>(l), 0x100))
            layers[nlayers++] = l;
    }
    for (int k = 0; k < nlayers && found < 48; k++) {
        BYTE *layer = at<BYTE *>(layers[k]);
        const DWORD count = dw(layer, 0xf8);
        BYTE **e = reinterpret_cast<BYTE **>(dw(layer, 0xf4));
        if (count > 4096 || (count && !readable(e, 4 * count)))
            continue;
        for (DWORD i = 0; i < count && found < 48; i++) {
            BYTE *s = e[i];
            if (!s || !readable(s, 0x2c) || invcheck::class_of(s) != cls)
                continue;
            Real &r = g_reals[found];
            memcpy(g_copies[found], s, 0x2c);
            r.sound = g_copies[found];
            r.current = *at<DWORD *>(CURRENT_LAYER);
            memcpy(r.listener, at<void *>(0x7136a0), 12);
            BYTE *part = at<BYTE *>(dw(s, 0x10));
            r.part_layer = 0;
            r.pos[0] = r.pos[1] = 0;
            if (part && readable(part, 0x50)) {
                r.part_layer = dw(part, 0x48);
                float c[2];
                at<float *(THISCALL *)(void *, float *)>(0x4fb0a0)(part, c);
                r.pos[0] = bits(c[0]);
                r.pos[1] = bits(c[1]);
            }
            found++;
        }
    }
    return found;
}

}  // namespace

ZAX_CHECKS(ambient_sound)
{
    (void)cls;
    (void)n;
    Tally getters{"ambient_sound: class getters"}, objects{"ambient_sound: Create, CreateArray, destructors"},
        fields{"ambient_sound: RegisterFields"}, real_fields{"ambient_sound: RegisterFields against the registered fields (real data)"},
        slots{"ambient_sound: CAmbientSound and the layer's ambient sounds in the fake world"},
        registered{"ambient_sound: registered class info (real data)"},
        real{"ambient_sound: the level's ambient sounds through Update (real data)"};
    invcheck::g_hook_table = hooks_ambient_sound;
    if (invcheck::arena_setup())
        invcheck::test_classes(objects, getters, fields, real_fields, CLASSES, 1);
    else
        objects.check(false, "ambient_sound", "[no arena]");
    getters.report();
    objects.report();
    fields.report();
    real_fields.report();
    invcheck::test_registered_class(registered, CLASSES[0]);
    const int found = collect_real();
    set_kind(K_RES, RES_SLOTS, sizeof RES_SLOTS / sizeof RES_SLOTS[0]);
    set_kind(K_ARRAY, ARRAY_SLOTS, sizeof ARRAY_SLOTS / sizeof ARRAY_SLOTS[0]);
    if (world_on(STANDINS, 1)) {
        test_slots(slots);
        for (int i = 0; i < found; i++) {
            g_real = &g_reals[i];
            for (unsigned k = 0; k < 3; k++)
                run_case(real, W_UPDATE, 0x90000u + 16u * (unsigned)i + k);
        }
        g_real = nullptr;
        world_off();
    } else {
        slots.check(false, "ambient_sound", "[the fake world could not be set up]");
    }
    log("  ambient_sound real data: %d ambient sounds in the level's layers", found);
    slots.report();
    registered.report();
    real.report();
}

}  // namespace zax
