// CSayQuip (0x55ac50-0x55b232) and the list of the quip categories (the function-local static 0x712370,
// 0x55afe0, 0x55b890-0x55c0ef) reimplemented: the action's Create, CreateArray, deleting destructor, class
// getters (both copies), RegisterFields and slot 23 (say a quip); the list's getter, constructor, exit
// function and slots, and the timers of the quips; CQuipCategory's static registration getter copies.
// The voice output itself (the voice manager 0x595b50 and its play 0x595be0) stays in the exe.
//
// CSayQuip [confirmed: Create 0x55aca0, RegisterFields] (0x10 bytes, class 0x7124b4, vtable 0x5fe6dc, parent
// CAction): +8 "Quip Category" (an entry of the quip list), +0xc "Probablity Of Saying Quip" (percent,
// 100). Slot 23 (layer, a2, a3, a4): the category's "Condition" action (+0x10 of CQuipCategory, slot 23
// through 0x41a720 with the category as the caller) decides; then a draw below 101 of the generator
// 0x712cc0 against the probability, and the decision goes to the quip packet writer (0x5bd300 gives it,
// 0x5bd560 writes it for the player of a2/a4, 0x5babc0) so that every machine says it. al 1, or the
// condition's al 0.
//
// The quip list [confirmed: code; a definition list of 0x140 bytes like the category list
// (docs/DECOMPILATION.md 6, "Definition classes")] (vtable 0x5fe74c, base vtable 0x5fe84c, folder
// "Quips", files "<name>.Quips"): made on first use by any of three copies of its guard code (bit 0 of
// 0x7124b1: RegisterFields through 0x55afe0, slot 23 inline, the getter 0x55b940), destroyed at exit by
// 0x55b9d0. Slots: 0 the deleting destructor, 14 the action verb "Play", 15 (i) plays a random sound of
// category i (loading it first; its ZDirList +0x1c: slot 22 the count, 0x4a32b0 the next path), 16
// calls 15, 34 CQuipCategory's class getter, 35 the file of an object, 37 (folder, name, out) the
// category made from its file plus its sounds (CQuipCategory::ReadSounds 0x55b7d0 on "<folder>/<name>";
// the base vtable's 0x55bf50 without them), 39 the file of entry i; 0x55bc60 loads the entries in use
// (slot 8), 0x55bd50 (seconds) counts their two timers down (+0x14 the delay before saying, +0x18 the
// time until the next quip; at 0 one of its sounds plays when no voice is playing, 0x595b30). Names of
// the list's functions ours.
#include "ai_helpers.h"
#include "def_list_slots.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD QUIP_LIST = 0x712370, QUIP_GUARD = 0x7124b1;
constexpr DWORD VT_QUIP_LIST = 0x5fe74c, VT_QUIP_LIST_BASE = 0x5fe84c, VT_SAY_QUIP = 0x5fe6dc;
constexpr DWORD TEXT_QUIPS = 0x61fffc;   // "Quips"
constexpr DWORD QUIP_LIST_EXIT = 0x55b9d0;

CClassInfo *__cdecl object_class() { return fn<Getter>(0x4163b0)(); }
void record_list_ctor(DWORD list, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u08)
{
    fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(at<void *>(list), sorted, stride, num, data, u08);
}
// The voice manager (0x595b50, made on first use) and its functions: 0x595b30 the number of voices
// playing, 0x595be0 (path, 1, 0) plays one
void *voices() { return fn<void *(__cdecl *)()>(0x595b50)(); }
void play_voice(const char *path) { fn<void(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x595be0)(voices(), path, 1, 0); }
// A random path of a ZDirList (0x4a32b0 with 0), 0 when it is empty
const char *next_sound(void *dirlist)
{
    if (!vcall<DWORD>(dirlist, 22))
        return nullptr;
    return fn<const char *(THISCALL *)(void *, DWORD)>(0x4a32b0)(dirlist, 0);
}

// Record i of a definition list (clamped to 0 when out of range) loaded when it is not loading and has no
// object (the list's slots 1 and 37 with "<folder>/<name>" split); the object or 0 (no records, loading)
BYTE *record_object(BYTE *list, DWORD i)
{
    BYTE *records = bp(dw(list, 0x30));
    if (!records)
        return nullptr;
    if (i >= dw(list, 0x34))
        i = 0;
    const DWORD off = i << 7;
    if (records[off + 0x7f])
        return nullptr;
    if (!dw(records + off, 0x78) && !records[off + 0x7f]) {
        records[off + 0x7f] = 1;
        char path[0x100];
        deflist::path_join(path, reinterpret_cast<const char *>(list + 0x38), vcall<const char *>(list, 1, i));
        char *name = deflist::path_name(path);
        DWORD o;
        if (name == path) {
            o = vcall<DWORD>(list, 37, glob<const char *>(TEXT_EMPTY), name, bp(dw(list, 0x30)) + off + 0x78);
        } else {
            name[-1] = 0;
            o = vcall<DWORD>(list, 37, path, name, bp(dw(list, 0x30)) + off + 0x78);
        }
        dw(bp(dw(list, 0x30)) + off, 0x78) = o;
        bp(dw(list, 0x30))[off + 0x7f] = 0;
    }
    return bp(dw(bp(dw(list, 0x30)) + off, 0x78));
}
// The end of the three guard codes: the list's records and monitor empty, its vtable, the exit function
void list_finish()
{
    *at<DWORD *>(glob_addr(QUIP_LIST) + 0x30) = 0;
    *at<DWORD *>(glob_addr(QUIP_LIST) + 0x34) = 0;
    *at<DWORD *>(glob_addr(QUIP_LIST) + 0x138) = 0;
    *at<DWORD *>(glob_addr(QUIP_LIST) + 0x13c) = 0;
    *glob<DWORD *>(QUIP_LIST) = vt(VT_QUIP_LIST);
    crt_atexit(code<void(__cdecl *)()>(QUIP_LIST_EXIT));
}

}  // namespace

// ---- The quip list ----------------------------------------------------------------------------------------

// 0x55afe0 (sorted, stride, count, data, u08): the record list's constructor, the aliases (+0x1c, vtable
// 0x5ed558, empty), +0x2c -1, the resource list's vtable. eax: this.
static BYTE *THISCALL QuipList_Ctor(BYTE *l, DWORD sorted, DWORD stride, DWORD num, void *data, DWORD u08)
{
    fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, sorted, stride, num, data, u08);
    dw(l, 0x1c) = vt(0x5ed558);
    dw(l, 0x20) = 0;
    dw(l, 0x24) = 0;
    dw(l, 0x28) = 0;
    dw(l, 0x2c) = 0xffffffff;
    dw(l, 0) = vt(RESOURCE_LIST_VTABLE);
    return l;
}
// 0x55b940: the list, made on first use (the record list's constructor, the aliases written inline).
// eax: the list.
static BYTE *__cdecl QuipList_Get()
{
    BYTE &guard = *glob<BYTE *>(QUIP_GUARD);
    if (!(guard & 1)) {
        guard |= 1;
        record_list_ctor(glob_addr(QUIP_LIST), 0, 0x80, 0, nullptr, 0);
        *at<DWORD *>(glob_addr(QUIP_LIST) + 0x1c) = vt(0x5ed558);
        *at<DWORD *>(glob_addr(QUIP_LIST) + 0x20) = 0;
        *at<DWORD *>(glob_addr(QUIP_LIST) + 0x24) = 0;
        *at<DWORD *>(glob_addr(QUIP_LIST) + 0x28) = 0;
        *at<DWORD *>(glob_addr(QUIP_LIST) + 0x2c) = 0xffffffff;
        list_finish();
    }
    return glob<BYTE *>(QUIP_LIST);
}
// 0x55b9d0: the list's exit function (the destructor's body with the base vtable)
static void __cdecl QuipList_Exit() { deflist::destroy_list(glob<ZDefinitionList *>(QUIP_LIST), VT_QUIP_LIST_BASE); }
// 0x55b030 (slot 14): the verb of the list's entries
static DWORD THISCALL QuipList_Verb(BYTE *) { return glob_addr(0x61fb70); /* "Play" */ }
// 0x55bb40 (slot 15: i): a random sound of category i (the category loaded first)
static void THISCALL QuipList_Play(BYTE *l, DWORD i)
{
    BYTE *q = record_object(l, i);
    if (const char *path = next_sound(bp(dw(q, 0x1c))))
        play_voice(path);
}
// 0x55b040 (slot 16: i): slot 15. eax: its.
static DWORD THISCALL QuipList_Play2(BYTE *l, DWORD i) { return vcall<DWORD>(l, 15, i); }
// 0x55bc60: the entries in use (slot 8) loaded (the argument unused)
static void THISCALL QuipList_LoadUsed(BYTE *l, DWORD)
{
    const DWORD n = vcall<DWORD>(l, 22);
    for (DWORD i = 0; i < n; i++) {
        if (!(vcall<DWORD>(l, 8, i) & 0xff) || !dw(l, 0x30))
            continue;
        const DWORD k = i < dw(l, 0x34) ? i : 0;
        BYTE *records = bp(dw(l, 0x30));
        const DWORD off = k << 7;
        if (records[off + 0x7f] || dw(records + off, 0x78) || records[off + 0x7f])
            continue;
        records[off + 0x7f] = 1;
        char path[0x100];
        deflist::path_join(path, reinterpret_cast<const char *>(l + 0x38), vcall<const char *>(l, 1, k));
        char *name = deflist::path_name(path);
        DWORD o;
        if (name == path) {
            o = vcall<DWORD>(l, 37, glob<const char *>(TEXT_EMPTY), name, bp(dw(l, 0x30)) + off + 0x78);
        } else {
            name[-1] = 0;
            o = vcall<DWORD>(l, 37, path, name, bp(dw(l, 0x30)) + off + 0x78);
        }
        dw(bp(dw(l, 0x30)) + off, 0x78) = o;
        bp(dw(l, 0x30))[off + 0x7f] = 0;
    }
}
// 0x55bd50 (seconds): the timers of the entries in use counted down: the delay +0x14 stops at 0; when the
// time +0x18 runs out a sound of the category plays unless a voice is playing
static void THISCALL QuipList_Update(BYTE *l, DWORD seconds)
{
    const DWORD n = vcall<DWORD>(l, 22);
    for (DWORD i = 0; i < n; i++) {
        if (!(vcall<DWORD>(l, 8, i) & 0xff))
            continue;
        BYTE *q = record_object(l, i);
        if (!c3(fld(fl<float>(q, 0x14)), cst(glob_addr(C_ZERO)))) {
            const long double v = fld(fl<float>(q, 0x14)) - raw_float(seconds);
            fl<float>(q, 0x14) = fstp(v);
            if (c0(v, cst(glob_addr(C_ZERO))))
                dw(q, 0x14) = 0;
        }
        if (c3(fld(fl<float>(q, 0x18)), cst(glob_addr(C_ZERO))))
            continue;
        const long double v = fld(fl<float>(q, 0x18)) - raw_float(seconds);
        fl<float>(q, 0x18) = fstp(v);
        if (!c03(v, cst(glob_addr(C_ZERO))))
            continue;
        dw(q, 0x18) = 0;
        if ((int)fn<DWORD(THISCALL *)(void *)>(0x595b30)(voices()) >= 1)
            continue;
        if (const char *path = next_sound(bp(dw(q, 0x1c))))
            play_voice(path);
    }
}
// 0x55b240 (CQuipCategory's static registration), 0x55bf00 (slot 34): CQuipCategory's class
static CClassInfo *__cdecl QuipCategory_ClassInfo()
{
    return class_getter(0x7124b8, 0x61fb78 /* "CQuipCategory" */, 1.0f, object_class, 0x20, 0, 0x55b290, 0x55b2e0, 0x55b460);
}
// 0x55ba90 (slot 37: folder, name, out): the category made from "<folder>/<name>.Quips" (written back
// after reading) and its sounds read from "<folder>/<name>". eax: the category.
static void *THISCALL QuipList_CreateEntry(BYTE *, const char *dir, const char *name, void **out)
{
    ZString ext, path, sounds;
    deflist::zs_ctor(&ext);
    deflist::zs_format(&ext, deflist::DOT_FORMAT, TEXT_QUIPS);
    deflist::zs_ctor(&path);
    deflist::zs_set_path3(&path, dir, name, deflist::zs_c_str(&ext));
    void *o = deflist::object_create_from_file(&path, 0, 1, out);
    deflist::zs_dtor(&path);
    deflist::zs_dtor(&ext);
    deflist::zs_ctor(&sounds);
    fn<DWORD(THISCALL *)(ZString *, const char *, const char *)>(0x4dfa50)(&sounds, dir, name);   // ZString_SetPath
    fn<DWORD(THISCALL *)(void *, const char *)>(0x55b7d0)(o, deflist::zs_c_str(&sounds));        // CQuipCategory::ReadSounds
    deflist::zs_dtor(&sounds);
    return o;
}

// ---- CSayQuip --------------------------------------------------------------------------------------------

static void say_quip_ctor(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(VT_SAY_QUIP));
}
static void say_quip_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SAY_QUIP));
    action_dtor(o);
}
static CClassInfo *__cdecl SayQuip_ClassInfo();
static void *__cdecl SayQuip_Create() { return create_one<0x10, say_quip_ctor>(); }
static void *__fastcall SayQuip_CreateArray(int count, int, int) { return create_array<0x10, say_quip_ctor>(count); }
// 0x55ac50 (static registration), 0x55adb0 (slot 2)
static CClassInfo *__cdecl SayQuip_ClassInfo()
{
    return class_getter(0x7124b4, 0x61faac /* "CSayQuip" */, 1.0f, action_class, 0x10, 0, 0x55aca0, 0x55acd0, 0x55ae10);
}
// 0x55ad30 (slot 1)
static void *THISCALL SayQuip_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, say_quip_destroy>(self, flags); }
// 0x55ae10. eax: the last FieldDesc_SetHelp's.
static DWORD __cdecl SayQuip_RegisterFields()
{
    void *f = heap_alloc(0x38);
    if (f) {
        BYTE &guard = *glob<BYTE *>(QUIP_GUARD);
        if (!(guard & 1)) {
            guard |= 1;
            fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x55afe0)(glob<void *>(QUIP_LIST), 0, 0x80, 0, nullptr, 0);
            list_finish();
        }
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(RESOURCE_FIELD_CTOR_TYPED)(
            f, glob_addr(0x61fb60) /* "Quip Category" */, 8, 4, 8, glob_addr(QUIP_LIST), 1, 0, 0, 0);
    }
    add_field(SayQuip_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61fb44) /* "Probablity Of Saying Quip" */, glob_addr(0x60f5d8), 0xc, 4, 0xc, 1, 0x42c80000 /* 100 */, 0, 0,
                  0x42c80000 /* 100 */, 0x40a00000 /* 5 */);
    add_field(SayQuip_ClassInfo(), f);
    return set_help(last_field(SayQuip_ClassInfo()), 0x61fab8);
}
// 0x55b050 (slot 23: layer, a2, a3, a4): see above. eax: the condition's (al 0) or the writer's with al 1.
static DWORD THISCALL SayQuip_Execute(BYTE *self, void *layer, BYTE *a2, void *a3, BYTE *a4)
{
    BYTE &guard = *glob<BYTE *>(QUIP_GUARD);
    if (!(guard & 1)) {
        guard |= 1;
        record_list_ctor(glob_addr(QUIP_LIST), 0, 0x80, 0, nullptr, 0);
        fn<void *(THISCALL *)(void *)>(0x468b30)(at<void *>(glob_addr(QUIP_LIST) + 0x1c));   // ResourceAliases::Ctor
        *at<DWORD *>(glob_addr(QUIP_LIST) + 0x2c) = 0xffffffff;
        list_finish();
    }
    BYTE *q = record_object(glob<BYTE *>(QUIP_LIST), dw(self, 8));
    if (dw(q, 0x10)) {
        const DWORD r = action_execute(bp(dw(q, 0x10)), layer, a2, a3, a4, q, q + 0x10);
        if (!(r & 0xff))
            return r;
    }
    const DWORD draw = random_below(RANDOM3, 0x65);
    const DWORD percent = (DWORD)x87_ftol(fld(fl<float>(self, 0xc)));
    const DWORD say = draw <= percent ? 1 : 0;
    BYTE *player = fn<BYTE *(__fastcall *)(void *, void *)>(0x5babc0)(a2, a4);   // ZaxNet_PlayerOfEither
    void *writer = fn<void *(__cdecl *)()>(0x5bd300)();                         // SayQuipReader
    const DWORD r = fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x5bd560)(writer, player + 0x3c, dw(self, 8), say);
    return (r & 0xffffff00u) | 1;
}

ZAX_HOOKS(say_quip)
{
    using deflist::Slots;
    const Hook list[] = {
#include "say_quip_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
