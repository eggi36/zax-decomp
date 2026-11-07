// The ambient sounds 0x4eda70-0x4ee747 (phase 4, sound): CAmbientSound, one looping sound of the sound
// list that a CAmbientSoundAI (section 8) puts into its layer, and the layer's list of them (+0xf0: add,
// remove, start, the distances). Update decides from the listener position whether the sound plays:
// beyond the resource's range it is muted, inside the inner radius it is centred (no position), between
// the two it plays at the part's position; while its part is in another layer it stays muted.
//
// Layout [confirmed: Create, RegisterFields, docs/fields.csv]: CAmbientSound (0x2c, class 0x6c1d10,
// version 2, vtable 0x5f8dec, parent CObject): +8 the sound resource (a CSoundResource of the sound list
// 0x713708, kept and of type 0x101), +0xc "Ambient Sound Index" (-1), +0x10 "Who Started" (a layer part,
// listened to), +0x14 the part's layer (its +0x48), +0x18 "Muted", +0x19 the "outside the inner radius"
// state (1 at the start), +0x1a "ShouldBePlaying", +0x1c "Inner Ambient Radius", +0x20 "Inner Ambient
// Radius Sqrd", +0x24 "Outer Ambient Radius", +0x28 "Max Distance".
// The 3D position of a part is (x, 0, -y) of its centre (LayerPart_Position 0x4fb0a0); the listener
// position comes as the argument, scaled by the sound manager's distance factor 0x7136ac (0x57c200).
// The current layer is 0x6c1c70.
#include "class_info.h"
#include "ai_common.h"
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD CLS_AMBIENT = 0x6c1d10, VT_AMBIENT = 0x5f8dec;
constexpr DWORD SOUND_LIST = 0x713708;       // "Sound List"
constexpr DWORD CURRENT_LAYER = 0x6c1c70;
constexpr DWORD OBJECT_REF_FIELD = 0x523e30;

CClassInfo *object_class() { return fn<Getter>(0x4163b0)(); }
// The sound list's resource of an index, kept (0x57e8e0; null for an index beyond the list)
BYTE *sound_of(DWORD index) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x57e8e0)(glob<void *>(SOUND_LIST), index); }
// Resource methods of the resource cache (resource_cache.cpp)
void res_keep(BYTE *r, DWORD on) { fn<DWORD(THISCALL *)(void *, DWORD)>(0x57bbc0)(r, on); }
void res_type(BYTE *r, DWORD type) { fn<void(THISCALL *)(void *, DWORD)>(0x57bc50)(r, type); }
void res_flag20(BYTE *r, DWORD on) { fn<DWORD(THISCALL *)(void *, DWORD)>(0x57bb60)(r, on); }
bool res_valid(BYTE *r) { return fn<bool(THISCALL *)(void *)>(0x57b740)(r); }
void res_reload_then(BYTE *r, float *pos, DWORD b) { fn<DWORD(THISCALL *)(void *, float *, DWORD)>(0x57b290)(r, pos, b); }
long double res_range(BYTE *r) { return fn<long double(THISCALL *)(void *)>(0x57b730)(r); }
void *resource_cache() { return fn<void *(__cdecl *)()>(0x57aef0)(); }
void cache_release(BYTE *r) { fn<DWORD(THISCALL *)(void *, void *)>(0x57af60)(resource_cache(), r); }
long double distance_factor() { return fn<long double(THISCALL *)(void *)>(0x57c200)(resource_cache()); }
DWORD message_base(void *o, DWORD a, const char *text) { return fn<DWORD(THISCALL *)(void *, DWORD, const char *)>(0x46cd50)(o, a, text); }

// The 3D position of a part: (x, 0, -y)
void part_point(BYTE *part, float *out)
{
    float c[2];
    part_position(part, c);
    out[0] = fstp(fld(c[0]));
    dw(out, 4) = 0;
    out[2] = fstp(-fld(c[1]));
}

// The constructor as Create inlines it
void ambient_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(VT_AMBIENT));
    dw(o, 8) = 0;
    fl<BYTE>(o, 0x19) = 1;
    dw(o, 0x14) = 0;
}
// The destructor: the sound released (its slot 11, then the cache), the part no longer listened to
void ambient_destroy(BYTE *o)
{
    dw(o, 0) = vt(VT_AMBIENT);
    if (BYTE *r = bp(dw(o, 8))) {
        vcall<DWORD>(r, 11);
        cache_release(bp(dw(o, 8)));
        dw(o, 8) = 0;
    }
    if (BYTE *part = bp(dw(o, 0x10))) {
        remove_listener(part, o, -1, glob_addr(TEXT_EMPTY));
        dw(o, 0x10) = 0;
    }
    dw(o, 0x14) = 0;
    obj_free_fields(o);
    obj_dtor(o);
}

// The sound plays (again) at its part, or without a position when it has none; then "ShouldBePlaying"
void start_sound(BYTE *s)
{
    if (dw(s, 0x14) == *glob<DWORD *>(CURRENT_LAYER)) {
        if (BYTE *part = bp(dw(s, 0x10))) {
            float v[3];
            part_point(part, v);
            res_reload_then(bp(dw(s, 8)), v, 0);
        } else {
            vcall<DWORD>(bp(dw(s, 8)), 2, 0);
        }
    }
    fl<BYTE>(s, 0x1a) = 1;
}

// ---- The class ----------------------------------------------------------------------------------------------

// 0x4eda70, 0x4edc70 (slot 2)
CClassInfo *__cdecl AmbientSound_ClassInfo()
{
    return class_getter(CLS_AMBIENT, 0x617898 /* "CAmbientSound" */, 2.0f, object_class, 0x2c, 0, 0x4edac0, 0x4edb00, 0x4edcd0);
}
// 0x4edac0, 0x4edb00, 0x4edb70 (slot 1)
void *__cdecl AmbientSound_Create() { return create_one<0x2c, ambient_construct>(); }
void *__fastcall AmbientSound_CreateArray(int count, int, int) { return create_array<0x2c, ambient_construct>(count); }
void *THISCALL AmbientSound_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x2c, ambient_destroy>(self, flags); }

// 0x4edcd0
DWORD __cdecl AmbientSound_RegisterFields()
{
    void *f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x61800c) /* "Ambient Sound Index" */, 0xc, 4, 0xc, 0, -1, 0, 0, 0);
    add_field(AmbientSound_ClassInfo(), f);
    f = heap_alloc(0x30);
    if (f)
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(OBJECT_REF_FIELD)(
            f, glob_addr(0x618000) /* "Who Started" */, 0x10, 4, 0x10, layer_part_class(), 0, 0);
    add_field(AmbientSound_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x617fe8) /* "Outer Ambient Radius" */, 0x24, 4, 0x24, 0, 0, 0, 0, 0, 0);
    add_field(AmbientSound_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60c2a8) /* "Max Distance" */, 0x28, 4, 0x28, 0, 0, 0, 0, 0, 0);
    add_field(AmbientSound_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x617fd0) /* "Inner Ambient Radius" */, 0x1c, 4, 0x1c, 0, 0, 0, 0, 0, 0);
    add_field(AmbientSound_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x617fb4) /* "Inner Ambient Radius Sqrd" */, 0x20, 4, 0x20, 0, 0, 0, 0, 0, 0);
    add_field(AmbientSound_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x617fac) /* "Muted" */, 0x18, 1, 0x18, 0, 0, 0);
    add_field(AmbientSound_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x617f9c) /* "ShouldBePlaying" */, 0x1a, 1, 0x1a, 0, 0, 0);
    return add_field(AmbientSound_ClassInfo(), f);
}

// 0x4ee440 (slot 0: a message): "Delete" forgets the part, "Leaving Scope" also stops listening to it;
// then CObject's
DWORD THISCALL AmbientSound_OnMessage(BYTE *self, DWORD a, const char *text)
{
    if (!crt_stricmp(text, glob<const char *>(TEXT_DELETE))) {
        dw(self, 0x10) = 0;
    } else if (!crt_stricmp(text, glob<const char *>(0x60aee8 /* "Leaving Scope" */))) {
        remove_listener(bp(dw(self, 0x10)), self, -1, glob_addr(TEXT_EMPTY));
        dw(self, 0x10) = 0;
    }
    return message_base(self, a, text);
}

// 0x4ee4a0: the part that started the sound (listened to; its layer kept)
void THISCALL AmbientSound_SetWhoStarted(BYTE *self, BYTE *part)
{
    if (BYTE *old = bp(dw(self, 0x10)))
        remove_listener(old, self, -1, glob_addr(TEXT_EMPTY));
    dw(self, 0x10) = ptr_value(part);
    if (part) {
        dw(self, 0x14) = dw(part, 0x48);
        add_listener(part, self, -1, glob_addr(TEXT_EMPTY));
    }
}

// 0x4ee100 (listener position): the sound's frame
void THISCALL AmbientSound_Update(BYTE *self, const float *listener)
{
    if (!dw(self, 8) && dw(self, 0xc) != 0xffffffff) {
        const DWORD index = dw(self, 0xc);
        BYTE *part = bp(dw(self, 0x10));
        BYTE *r = sound_of(index);
        dw(self, 8) = ptr_value(r);
        if (!r) {
            dw(self, 0xc) = 0xffffffff;
            if (BYTE *p = bp(dw(self, 0x10))) {
                remove_listener(p, self, -1, glob_addr(TEXT_EMPTY));
                dw(self, 0x10) = 0;
            }
        } else {
            vcall<DWORD>(r, 3, 1);
            dw(self, 0xc) = index;
            AmbientSound_SetWhoStarted(self, part);
            res_keep(bp(dw(self, 8)), 1);
            res_type(bp(dw(self, 8)), 0x101);
            vcall<DWORD>(bp(dw(self, 8)), 5, dw(self, 0x24), dw(self, 0x28), 1);
            if (fl<BYTE>(self, 0x18))
                res_flag20(bp(dw(self, 8)), 1);
            fl<BYTE>(self, 0x1a) = 1;
        }
    }
    if (!dw(self, 0x14) && dw(self, 0x10))
        dw(self, 0x14) = dw(bp(dw(self, 0x10)), 0x48);
    if (dw(self, 0x14) != *glob<DWORD *>(CURRENT_LAYER)) {
        // in another layer: muted
        if (fl<BYTE>(self, 0x18))
            return;
        if (BYTE *r = bp(dw(self, 8)))
            res_flag20(r, 1);
        fl<BYTE>(self, 0x18) = 1;
        return;
    }
    BYTE *r = bp(dw(self, 8));
    if (!r)
        return;
    if (!dw(self, 0x10)) {
        // no part: a sound that is no longer valid stops
        if (!res_valid(r))
            return;
        if (BYTE *rr = bp(dw(self, 8)))
            vcall<DWORD>(rr, 11);
        fl<BYTE>(self, 0x1a) = 0;
        return;
    }
    if (fl<BYTE>(self, 0x1a) && !res_valid(r) && !fl<BYTE>(self, 0x18) && dw(self, 8)) {
        // it should play but was dropped: again
        start_sound(self);
    }
    float c[2];
    part_position(bp(dw(self, 0x10)), c);
    float p[3];
    p[0] = fstp(fld(c[0]));
    dw(p, 4) = 0;
    p[2] = fstp(-fld(c[1]));
    float d[3];
    d[0] = fstp(fld(listener[0]) - fld(p[0]));
    d[1] = fstp(fld(listener[1]));
    d[2] = fstp(fld(listener[2]) - fld(p[2]));
    const long double scale = distance_factor();
    d[0] = fstp(fld(d[0]) * scale);
    d[1] = fstp(fld(d[1]) * scale);
    const long double z = scale * fld(d[2]);
    const float dist2 = fstp(z * z + fld(d[1]) * fld(d[1]) + fld(d[0]) * fld(d[0]));
    const long double range = res_range(bp(dw(self, 8)));
    if (!c0(fld(dist2), range * range)) {
        // beyond the range: muted
        if (fl<BYTE>(self, 0x18))
            return;
        res_flag20(bp(dw(self, 8)), 1);
        fl<BYTE>(self, 0x18) = 1;
        return;
    }
    if (c0(fld(dist2), fld(fl<float>(self, 0x20)))) {
        // inside the inner radius: without a position
        if (fl<BYTE>(self, 0x19) == 1) {
            fl<BYTE>(self, 0x19) = 0;
            vcall<DWORD>(bp(dw(self, 8)), 3, 0);
        }
    } else {
        if (fl<BYTE>(self, 0x19) == 0)
            fl<BYTE>(self, 0x19) = 1;
        vcall<DWORD>(bp(dw(self, 8)), 4, p, 1);
    }
    if (!fl<BYTE>(self, 0x18))
        return;
    // it was muted: heard again, started again unless it still plays
    res_flag20(bp(dw(self, 8)), 0);
    if (!dw(self, 8) || !res_valid(bp(dw(self, 8)))) {
        if (dw(self, 8))
            start_sound(self);
    }
    fl<BYTE>(self, 0x18) = 0;
}

// ---- The layer's ambient sounds (CLayer +0xf0: a pointer array, +0xf4 the entries, +0xf8 the count) ----------

// 0x4ee4e0 (index into the sound list, the part): a new CAmbientSound in the first free entry (or
// appended); its index, -1 when the sound list has no such sound
DWORD THISCALL Layer_AddAmbientSound(BYTE *layer, DWORD index, BYTE *part)
{
    BYTE *s = static_cast<BYTE *>(AmbientSound_Create());
    vcall<DWORD>(s, 19);
    BYTE *r = sound_of(index);
    dw(s, 8) = ptr_value(r);
    if (!r) {
        if (s)
            vcall<void *>(s, 1, 1);
        return 0xffffffff;
    }
    vcall<DWORD>(r, 3, 1);
    dw(s, 0xc) = index;
    if (BYTE *old = bp(dw(s, 0x10)))
        remove_listener(old, s, -1, glob_addr(TEXT_EMPTY));
    dw(s, 0x10) = ptr_value(part);
    if (part) {
        dw(s, 0x14) = dw(part, 0x48);
        add_listener(part, s, -1, glob_addr(TEXT_EMPTY));
    }
    res_keep(bp(dw(s, 8)), 1);
    res_type(bp(dw(s, 8)), 0x101);
    const DWORD n = dw(layer, 0xf8);
    DWORD *entries = reinterpret_cast<DWORD *>(bp(dw(layer, 0xf4)));
    for (DWORD i = 0; i < n; i++)
        if (!entries[i]) {
            entries[i] = ptr_value(s);
            return i;
        }
    return vcall<DWORD>(layer + 0xf0, 2, &s);
}
// 0x4ee610 (index): the sound destroyed and its entry cleared
void THISCALL Layer_RemoveAmbientSound(BYTE *layer, int index)
{
    if (index == -1 || index >= (int)dw(layer, 0xf8))
        return;
    BYTE **entries = reinterpret_cast<BYTE **>(bp(dw(layer, 0xf4)));
    if (BYTE *s = entries[index]) {
        vcall<void *>(s, 1, 1);
        reinterpret_cast<BYTE **>(bp(dw(layer, 0xf4)))[index] = nullptr;
    }
}
// 0x4ee650 (index): the sound starts (at its part in the current layer)
void THISCALL Layer_StartAmbientSound(BYTE *layer, int index)
{
    if (index == -1 || index >= (int)dw(layer, 0xf8))
        return;
    BYTE *s = reinterpret_cast<BYTE **>(bp(dw(layer, 0xf4)))[index];
    if (!s || !dw(s, 8))
        return;
    start_sound(s);
}
// 0x4ee6e0 (index, inner, outer, max distance): the radii (the resource's range: slot 5 with outer and
// max); the entry is not checked against the count, as in the exe
void THISCALL Layer_SetAmbientDistances(BYTE *layer, int index, DWORD inner, DWORD outer, DWORD max)
{
    if (index == -1)
        return;
    BYTE *s = reinterpret_cast<BYTE **>(bp(dw(layer, 0xf4)))[index];
    if (!dw(s, 8))
        return;
    vcall<DWORD>(bp(dw(s, 8)), 5, outer, max, 1);
    dw(s, 0x24) = outer;
    dw(s, 0x28) = max;
    dw(s, 0x1c) = inner;
    fl<float>(s, 0x20) = fstp(fld(raw_float(inner)) * fld(raw_float(inner)));
}
// 0x4ee730 (a, b, c): CLayerSaveData::OnPartFlags of the layer's save data (+0x3c)
DWORD THISCALL Layer_OnPartFlags(BYTE *layer, DWORD a, DWORD b, DWORD c)
{
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4e76e0)(layer + 0x3c, a, b, c);
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(ambient_sound)
{
    const Hook list[] = {
#include "ambient_sound_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
