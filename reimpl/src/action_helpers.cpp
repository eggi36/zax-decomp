// The actions' helpers and small classes 0x418000-0x41ffff (phase 3, game logic): CAction's destructors,
// the execute wrappers, the name lookups ("$Trigger", "$Instigator", "$Touching", "$Target(...)", ranges
// "#a-b#", comma lists), the warning, the per-target loop, the conditional actions (CConditionalAction,
// CIfAction), the queue of delayed actions (CActionQueue, CActionQueueItem), the slots of
// COtherMapAction, CDialogLine and the trigger AIs CollideTriggerAI and CPassThroughTriggerAI in this
// range, and the editor's converting wrapper of the trigger AIs. The registration of the classes
// (Create, CreateArray, RegisterFields, the class getters) is in action_classes.cpp.
//
// The arguments a2, a3, a4 of the execute slot (slot 23) are the trigger part, the trigger AI and the
// instigator: the queue stores them as "TriggerPart", "TriggerAI" and "Instigator" [confirmed: 0x41c7a0
// and the fields of CActionQueueItem]. The name lookups take the trigger and the instigator.
#include "class_info.h"
#include "exe_base.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
static void obj_free_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static void obj_init_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x416610)(o); }
static void obj_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4166d0)(o); }
static void *obj_clone(void *o) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffff); }
static bool is_kind_of(void *o, CClassInfo *c) { return fn<bool(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
static DWORD obj_on_message(void *o, void *sender, const char *name)
{
    return fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x46cd50)(o, sender, name);
}
static CClassInfo *class_at(DWORD getter) { return fn_dyn<Getter>(getter)(); }
static void zs_ctor(void *z) { fn<ZString *(THISCALL *)(void *)>(0x4dec90)(z); }
static void zs_ctor_n(void *z, const char *t, DWORD n) { fn<ZString *(THISCALL *)(void *, const char *, DWORD)>(0x4deeb0)(z, t, n); }
static void zs_dtor(void *z) { fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
static const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
static DWORD zs_length(const void *z) { return fn<DWORD(THISCALL *)(const void *)>(0x4e13b0)(z); }
static bool zs_not_empty(const void *z) { return fn<bool(THISCALL *)(const void *)>(0x4e13e0)(z); }
static DWORD zs_assign(void *z, const void *v) { return fn<DWORD(THISCALL *)(void *, const void *)>(0x4e18f0)(z, v); }
static void zs_append(void *z, const char *t) { fn<void *(THISCALL *)(void *, const char *)>(0x4e1be0)(z, t); }
template <typename... A> static void zs_format(void *z, const char *fmt, A... a)
{
    fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, fmt, a...);
}
static const char *class_short_name(CClassInfo *c) { return fn<const char *(THISCALL *)(CClassInfo *)>(0x415370)(c); }
static void *find_field(CClassInfo *c, const char *name, DWORD index)
{
    return fn<void *(THISCALL *)(CClassInfo *, const char *, DWORD)>(0x4154d0)(c, name, index);
}

// C runtime
static int crt_strnicmp(const char *a, const char *b, DWORD n) { return fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5dd9c0)(a, b, n); }
static const char *crt_strchr(const char *s, int c) { return fn<const char *(__cdecl *)(const char *, int)>(0x5d08c0)(s, c); }
static DWORD text_length(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}
// 0x587cb0: the first `c` among the first n characters, or 0
static const char *text_find(const char *s, DWORD n, int c) { return fn<const char *(__fastcall *)(const char *, DWORD, int)>(0x587cb0)(s, n, c); }

// The temporary part lists (0x59a470 gives one, 0x59a510 takes it back; +4 data, +8 count)
struct PartList {
    DWORD vtable;
    void **data;
    DWORD count;
};
static PartList *part_list_get()
{
    PartList *l;
    fn<void(THISCALL *)(PartList **)>(0x59a470)(&l);
    return l;
}
static DWORD part_list_release(PartList *&l) { return fn<DWORD(THISCALL *)(PartList **)>(0x59a510)(&l); }
static void part_list_clear(PartList *l) { fn<void(THISCALL *)(PartList *)>(0x59a120)(l); }
static void part_list_add(PartList *l, void *p) { fn<void(THISCALL *)(PartList *, void *)>(0x59a140)(l, p); }
// The layer's sort list (+0x50): a new stamp for a list, the parts of a name
static void layer_new_stamp(BYTE *layer, PartList *l) { fn<void(THISCALL *)(void *, PartList *)>(0x57aeb0)(layer + 0x50, l); }
static const char *part_name(void *p) { return fn<const char *(THISCALL *)(void *)>(0x4fbf20)(p); }

static void *reporter() { return *glob<void **>(0x6c02cc); }
static BYTE *world() { return *glob<BYTE **>(0x6c2080); }

// The messages of the name lookups (static strings of the exe, set at startup)
constexpr DWORD MSG_NO_NAME = 0x62e75c, MSG_NOT_FOUND = 0x62e728, MSG_SEVERAL = 0x62e724, MSG_WRONG_CLASS = 0x62e718,
                FMT_WRONG_CLASS = 0x62e71c, MSG_BAD_RANGE = 0x62e73c, LABEL_NAME = 0x62e740, LABEL_PART = 0x62e76c,
                LABEL_TRIGGER = 0x62e6f0, LABEL_OBJECT = 0x62e74c, LABEL_CALLER = 0x62e760;

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
static BYTE &by(void *p, DWORD off) { return static_cast<BYTE *>(p)[off]; }
static void *ptr_at(void *p, DWORD off) { return reinterpret_cast<void *>(static_cast<uintptr_t>(dw(p, off))); }
static void *vp(DWORD v) { return reinterpret_cast<void *>(static_cast<uintptr_t>(v)); }
static DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }

static void vtable_and_free(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vtable;
    obj_free_fields(o);
}
using Destroy = void (*)(BYTE *);
template <DWORD SIZE, Destroy D> static void *deleting_dtor(BYTE *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                D(self + (i - 1) * SIZE);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    D(self);
    if (flags & 1)
        pool_free(self, SIZE);
    return self;
}
template <DWORD SIZE, Destroy D> static void *deleting_dtor_single(BYTE *self, DWORD flags)
{
    D(self);
    if (flags & 1)
        pool_free(self, SIZE);
    return self;
}

// ---- Core: the field descriptor's alternative name ---------------------------------------------------------

// 0x419050: the text of +0x1c
static const char *THISCALL FieldDesc_AltName(BYTE *f) { return zs_c_str(f + 0x1c); }

// ---- The editor's action wrappers: the base class --------------------------------------------------------------

// 0x41a0a0: the constructor of a wrapper (a global object; the static initialisers call it): the base
// vtable 0x5ece44, then the object goes to the front of the list 0x62e6ec (+4 the next). eax: this.
static BYTE *THISCALL Wrapper_Ctor(BYTE *o)
{
    dw(o, 0) = zax::vt(0x5ece44);
    dw(o, 4) = 0;
    dw(o, 4) = *glob<DWORD *>(0x62e6ec);
    *glob<DWORD *>(0x62e6ec) = pv(o);
    return o;
}

// ---- CAction ---------------------------------------------------------------------------------------------------

constexpr DWORD VT_ACTION = 0x5ece50;
// 0x41a700: CAction's level (its vtable, Object_FreeFields), then CObject's destructor
static void THISCALL Action_Dtor(BYTE *o)
{
    vtable_and_free(o, zax::vt(VT_ACTION));
    obj_dtor(o);
}
static void Action_destroy(BYTE *o) { Action_Dtor(o); }
// 0x41a6d0 (slot 1 of the abstract class: no array path)
static void *THISCALL Action_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor_single<0x8, Action_destroy>(self, flags); }

// 0x41a720: slot 23 (layer, a2, a3, a4) of the action; the caller and the field are only passed along.
// eax: slot 23's.
static DWORD THISCALL Action_Execute(void *a, void *layer, DWORD a2, DWORD a3, DWORD a4, void *, DWORD)
{
    return vcall<DWORD>(a, 23, layer, a2, a3, a4);
}
// 0x41a740: the same without the field
static DWORD THISCALL Action_ExecuteFrom(void *a, void *layer, DWORD a2, DWORD a3, DWORD a4, void *)
{
    return vcall<DWORD>(a, 23, layer, a2, a3, a4);
}

// ---- The name lookups -------------------------------------------------------------------------------------------

static void __fastcall Names_Resolve(BYTE *layer, const char *text, void *trigger, void *instigator, PartList *list);
static void __fastcall Name_ResolveRange(BYTE *layer, const char *text, DWORD len, PartList *list);

// 0x41a930: the parts one name (its first `len` characters) means, into `list`:
//  - "$touching" (compared over `len` characters, so any prefix of it): the parts the trigger touches: the
//    parts in the trigger's rectangle (slot 37 with 3, 0; CLayer::FindRect into a temporary list) whose
//    polygon (slot 48, at slot 43's position) overlaps the trigger's (slot 48 at slot 50's position);
//  - "$trigger", "$instigator" (the same comparison): that part;
//  - "$target(names)" when the character after the name is ')' (as the original tests it): every
//    part's target (slot 70) of the parts the names in the brackets mean;
//  - a name with '#': a range (0x41a800);
//  - else the parts of that name in the layer's sort list (CSortList2D_QueryName).
// The list is cleared and gets a new stamp only on the "$touching" and the comma list path (0x41a760).
static void __fastcall Name_Resolve(BYTE *layer, const char *text, DWORD len, void *trigger, void *instigator, PartList *list)
{
    if (crt_strnicmp(text, glob<const char *>(0x60ae24), len) == 0) {   // "$touching"
        if (!trigger)
            return;
        PartList *tmp = part_list_get();
        BYTE rect[0x10];
        vcall<void>(trigger, 37, rect, 3, 0);
        fn<void(THISCALL *)(void *, PartList *, void *, DWORD, DWORD, DWORD, DWORD)>(0x4ed5e0)(layer, tmp, rect, 3, 0, 0, 0);
        part_list_clear(list);
        layer_new_stamp(layer, list);
        float tpos[2];
        vcall<void>(trigger, 50, tpos);
        void *poly = vcall<void *>(trigger, 48);
        const DWORD n = tmp->count;
        for (DWORD i = 0; i < n; i++) {
            void *p = tmp->data[i];
            if (p == trigger)
                continue;
            float ppos[2];
            vcall<void>(p, 43, ppos);
            void *pp = vcall<void *>(p, 48);
            if (fn<bool(THISCALL *)(void *, float *, void *, float *)>(0x547d60)(poly, tpos, pp, ppos))
                part_list_add(list, p);
        }
        part_list_release(tmp);
        return;
    }
    if (crt_strnicmp(text, glob<const char *>(0x60ae18), len) == 0) {   // "$trigger"
        if (trigger)
            part_list_add(list, trigger);
        return;
    }
    if (crt_strnicmp(text, glob<const char *>(0x60ae0c), len) == 0) {   // "$instigator"
        if (instigator)
            part_list_add(list, instigator);
        return;
    }
    const char *open = glob<const char *>(0x60adf4), *both = glob<const char *>(0x60ae00);   // "$target(", "$target()"
    if (len >= text_length(both) && crt_strnicmp(text, open, text_length(open)) == 0 && text[len] == ')') {
        PartList *tmp = part_list_get();
        ZString inner;
        zs_ctor_n(&inner, text + text_length(open), len - text_length(both));
        Names_Resolve(layer, zs_c_str(&inner), trigger, instigator, tmp);
        for (DWORD i = 0; i < tmp->count; i++)
            if (void *p = tmp->data[i])
                if (vcall<void *>(p, 70))
                    part_list_add(list, vcall<void *>(p, 70));
        zs_dtor(&inner);
        part_list_release(tmp);
        return;
    }
    if (text_find(text, len, '#')) {
        Name_ResolveRange(layer, text, len, list);
        return;
    }
    fn<void(THISCALL *)(void *, PartList *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x57a7e0)(layer + 0x50, list, text, len, 0, 0,
                                                                                                    0, 0);
}

// 0x41a760: the parts a comma list of names means: the list cleared, a new stamp, then every name (spaces
// after a comma skipped, empty names between commas left out; the last one always) through 0x41a930
static void __fastcall Names_Resolve(BYTE *layer, const char *text, void *trigger, void *instigator, PartList *list)
{
    part_list_clear(list);
    layer_new_stamp(layer, list);
    if (!text || !*text)
        return;
    const char *p = text;
    for (const char *c = crt_strchr(p, ','); c; c = crt_strchr(p, ',')) {
        if (c > p)
            Name_Resolve(layer, p, (DWORD)(c - p), trigger, instigator, list);
        p = c + 1;
        while (*p == ' ')
            p++;
    }
    Name_Resolve(layer, p, text_length(p), trigger, instigator, list);
}

// 0x41a800: a range "prefix#a-b#suffix" (the first `len` characters): the names prefix + i + suffix for i
// from a to b in steps of one (either direction), each through 0x41a930 without trigger or instigator.
// Without the second '#' or the '-', or with a equal to b, the world gets two messages (slot 17: the
// warning 0x62e73c and the name).
static void __fastcall Name_ResolveRange(BYTE *layer, const char *text, DWORD len, PartList *list)
{
    ZString name;
    zs_ctor(&name);
    const char *h1 = text_find(text, len, '#');
    const char *dash = h1 ? crt_strchr(h1, '-') : nullptr;
    const char *h2 = dash ? crt_strchr(dash, '#') : nullptr;
    int from = 0, to = 0;
    if (h2) {
        from = crt_atoi(h1 + 1);
        to = crt_atoi(dash + 1);
    }
    if (!h2 || from == to) {
        vcall<void>(world(), 17, zs_c_str(glob<void *>(MSG_BAD_RANGE)));
        vcall<void>(world(), 17, text);
        zs_dtor(&name);
        return;
    }
    const int prefix = (int)(h1 - text);
    const int suffix = (int)(len - (DWORD)(h2 - text) - 1);
    const int step = from < to ? 1 : -1;
    const int end = to + step;
    for (int i = from; i != end; i += step) {
        zs_format(&name, glob<const char *>(0x60ade8), prefix, text, i, suffix, h2 + 1);   // "%.*s%d%.*s"
        Name_Resolve(layer, zs_c_str(&name), zs_length(&name), nullptr, nullptr, list);
    }
    zs_dtor(&name);
}

// 0x41abd0: with `report`, the warning `text` to the reporter (slot 17), then one line naming what is
// known (the name, the part's and the trigger's names, the classes of `object` and `caller`) when there
// is any.
static void __fastcall Action_Report(const char *text, const char *name, void *part, void *trigger, void *object, void *caller, DWORD report)
{
    if (!(BYTE)report)
        return;
    vcall<void>(reporter(), 17, text);
    ZString line;
    zs_ctor(&line);
    if (name && *name) {
        zs_append(&line, zs_c_str(glob<void *>(LABEL_NAME)));
        zs_append(&line, name);
    }
    if (part && part_name(part) && *part_name(part)) {
        zs_append(&line, zs_c_str(glob<void *>(LABEL_PART)));
        zs_append(&line, part_name(part));
    }
    if (trigger && part_name(trigger) && *part_name(trigger)) {
        zs_append(&line, zs_c_str(glob<void *>(LABEL_TRIGGER)));
        zs_append(&line, part_name(trigger));
    }
    if (object) {
        zs_append(&line, zs_c_str(glob<void *>(LABEL_OBJECT)));
        zs_append(&line, class_short_name(vcall<CClassInfo *>(object, 2)));
    }
    if (caller) {
        zs_append(&line, zs_c_str(glob<void *>(LABEL_CALLER)));
        zs_append(&line, class_short_name(vcall<CClassInfo *>(caller, 2)));
    }
    if (zs_not_empty(&line))
        vcall<void>(reporter(), 17, zs_c_str(&line));
    zs_dtor(&line);
}

// 0x41ad80: the one part a name means (a2 the trigger, a4 the instigator); warnings without a name, with
// none or several parts, or one not of class `cls`. eax: the part, 0 otherwise.
static void *__fastcall Name_ResolveOne(BYTE *layer, const char *text, void *a2, void *a4, void *a3, void *caller, DWORD report, CClassInfo *cls)
{
    if (!text || !*text) {
        Action_Report(zs_c_str(glob<void *>(MSG_NO_NAME)), text, nullptr, a2, a3, caller, report);
        return nullptr;
    }
    PartList *list = part_list_get();
    Names_Resolve(layer, text, a2, a4, list);
    DWORD msg = 0;
    void *part = nullptr;
    if (list->count == 0)
        msg = glob_addr(MSG_NOT_FOUND);
    else if (list->count > 1)
        msg = glob_addr(MSG_SEVERAL);
    else {
        part = list->data[0];
        if (cls && !is_kind_of(part, cls))
            msg = glob_addr(MSG_WRONG_CLASS);
    }
    if (msg) {
        Action_Report(zs_c_str(at<void *>(msg)), text, msg == glob_addr(MSG_WRONG_CLASS) ? part : nullptr, a2, a3, caller, report);
        part_list_release(list);
        return nullptr;
    }
    part_list_release(list);
    return part;
}
// 0x41ad40: the same with the name a ZString
static void *__fastcall Name_ResolveOneZ(BYTE *layer, const ZString *name, void *a2, void *a4, void *a3, void *caller, DWORD report, CClassInfo *cls)
{
    return Name_ResolveOne(layer, zs_c_str(name), a2, a4, a3, caller, report, cls);
}

// 0x41aed0: slot 27 (a2, a3, a4, part) of the action for every part the name means (a2 the trigger, a4
// the instigator), parts not of class `cls` warned about ("%s is not a %s"). With a delay above 0.001
// only the first runs now (without the class test, as the original) and copies of the action go into
// the layer's queue for the others, i · delay later; al then the first one's result, else the OR of
// all. eax's upper bytes: the release's (0x59a510), or the name's address without a name.
static DWORD THISCALL Action_EachTarget(BYTE *self, BYTE *layer, const char *text, DWORD a2, DWORD a3, DWORD a4, float delay, CClassInfo *cls)
{
    if (!text || !*text)
        return pv(text) & 0xffffff00;
    PartList *list = part_list_get();
    Names_Resolve(layer, text, vp(a2), vp(a4), list);
    const DWORD n = list->count;
    BYTE r = 0;
    auto wrong = [&](void *p) {
        if (!cls || is_kind_of(p, cls))
            return false;
        ZString msg;
        zs_ctor(&msg);
        const char *fmt = zs_c_str(glob<void *>(FMT_WRONG_CLASS));
        const char *want = class_short_name(cls);
        zs_format(&msg, fmt, class_short_name(vcall<CClassInfo *>(p, 2)), want);
        Action_Report(zs_c_str(&msg), text, p, vp(a2), nullptr, self, 1);
        zs_dtor(&msg);
        return true;
    };
    if (n) {
        if (delay > 0.001f) {
            r = (BYTE)vcall<DWORD>(self, 27, a2, a3, a4, list->data[0]);
            for (DWORD i = 1; i < n; i++) {
                void *p = list->data[i];
                if (wrong(p))
                    continue;
                void *copy = obj_clone(self);
                const float d = (float)((long double)(unsigned long long)i * delay);
                fn<void(THISCALL *)(void *, float, DWORD, DWORD, DWORD, void *, void *)>(0x41c7a0)(layer + 0xa0, d, a4, a2, a3, p, copy);
            }
        } else
            for (DWORD i = 0; i < n; i++) {
                void *p = list->data[i];
                if (!wrong(p))
                    r |= (BYTE)vcall<DWORD>(self, 27, a2, a3, a4, p);
            }
    }
    return (part_list_release(list) & 0xffffff00) | r;
}
// 0x41ae90: the same with the name a ZString
static DWORD THISCALL Action_EachTargetZ(BYTE *self, BYTE *layer, const ZString *name, DWORD a2, DWORD a3, DWORD a4, float delay, CClassInfo *cls)
{
    return Action_EachTarget(self, layer, zs_c_str(name), a2, a3, a4, delay, cls);
}

// ---- CConditionalAction ("Try Action", "Succeed Action", "Fail Action"), CIfAction ("If", "Then", "Else") --

static void conditional_destroy_level(BYTE *o, DWORD vt)
{
    vtable_and_free(o, vt);
    vtable_and_free(o, zax::vt(VT_ACTION));
    obj_dtor(o);
}
static void Conditional_destroy(BYTE *o) { conditional_destroy_level(o, zax::vt(0x5ecec4)); }
static void If_destroy(BYTE *o) { conditional_destroy_level(o, zax::vt(0x5ecf34)); }
// 0x41b1d0, 0x41b7b0 (slot 1)
static void *THISCALL Conditional_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, Conditional_destroy>(self, flags); }
static void *THISCALL If_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, If_destroy>(self, flags); }

// 0x41bba0 (slot 23 of both): the first action (slot 23 directly); on success the second (eax its, al 1
// without one), on failure the third (al 0). al 0 without a first action.
static DWORD THISCALL Conditional_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    void *cond = ptr_at(self, 8);
    if (!cond)
        return 0;
    const DWORD r = vcall<DWORD>(cond, 23, layer, a2, a3, a4);
    if ((BYTE)r) {
        if (void *t = ptr_at(self, 0xc))
            return vcall<DWORD>(t, 23, layer, a2, a3, a4);
        return (r & 0xffffff00) | 1;
    }
    if (void *e = ptr_at(self, 0x10))
        return vcall<DWORD>(e, 23, layer, a2, a3, a4) & 0xffffff00;
    return pv(self) & 0xffffff00;
}

// ---- CActionQueueItem (CObject, 0x34: +8 "Time To Execute", +0xc "Instigator", +0x10 "TriggerPart",
// +0x14 "TriggerAI", +0x18 "TargetPart", +0x1c "Action"; +0x24-+0x30 the referenced objects' class ids) --

constexpr DWORD VT_QUEUE_ITEM = 0x5ecfa4;
static DWORD part_listen(void *p, void *who, bool on)
{
    return at<DWORD(THISCALL *)(void *, void *, int, const char *)>(on ? fn_addr(0x4fc1b0) : fn_addr(0x4fc200))(p, who, -1, glob<const char *>(0x62e558));
}
static DWORD ai_listen(void *ai, void *who, bool on)
{
    return at<DWORD(THISCALL *)(void *, void *, int, const char *)>(on ? fn_addr(0x4fd190) : fn_addr(0x4fd1e0))(ai, who, -1, glob<const char *>(0x62e558));
}

// 0x41c0b0: the destructor: the listeners off the referenced objects (the AI first), then Object_FreeFields
// and CObject's
static void THISCALL QueueItem_Dtor(BYTE *o)
{
    dw(o, 0) = zax::vt(VT_QUEUE_ITEM);
    if (ptr_at(o, 0x14))
        ai_listen(ptr_at(o, 0x14), o, false);
    if (ptr_at(o, 0xc))
        part_listen(ptr_at(o, 0xc), o, false);
    if (ptr_at(o, 0x10))
        part_listen(ptr_at(o, 0x10), o, false);
    if (ptr_at(o, 0x18))
        part_listen(ptr_at(o, 0x18), o, false);
    obj_free_fields(o);
    obj_dtor(o);
}
static void QueueItem_destroy(BYTE *o) { QueueItem_Dtor(o); }
// 0x41bcd0 (slot 1)
static void *THISCALL QueueItem_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x34, QueueItem_destroy>(self, flags); }

// 0x41c120: sets the item: the listeners off the old objects, the values in, the listeners on the new ones
// (instigator, trigger part, target part, then the AI) with each one's class id (+0xc of its class) into
// +0x24, +0x28, +0x2c, +0x30
static void THISCALL QueueItem_Set(BYTE *o, DWORD time, void *instigator, void *trigger, void *ai, void *target, void *action)
{
    if (ptr_at(o, 0x14))
        ai_listen(ptr_at(o, 0x14), o, false);
    if (ptr_at(o, 0xc))
        part_listen(ptr_at(o, 0xc), o, false);
    if (ptr_at(o, 0x10))
        part_listen(ptr_at(o, 0x10), o, false);
    if (ptr_at(o, 0x18))
        part_listen(ptr_at(o, 0x18), o, false);
    dw(o, 0x1c) = pv(action);
    dw(o, 8) = time;
    dw(o, 0x10) = pv(trigger);
    dw(o, 0xc) = pv(instigator);
    dw(o, 0x14) = pv(ai);
    dw(o, 0x18) = pv(target);
    if (instigator) {
        part_listen(instigator, o, true);
        dw(o, 0x24) = dw(vcall<void *>(ptr_at(o, 0xc), 2), 0xc);
    }
    if (trigger) {
        part_listen(trigger, o, true);
        dw(o, 0x28) = dw(vcall<void *>(ptr_at(o, 0x10), 2), 0xc);
    }
    if (target) {
        part_listen(target, o, true);
        dw(o, 0x2c) = dw(vcall<void *>(ptr_at(o, 0x18), 2), 0xc);
    }
    if (ai) {
        ai_listen(ai, o, true);
        dw(o, 0x30) = dw(vcall<void *>(ptr_at(o, 0x14), 2), 0xc);
    }
}

// 0x41c230 (slot 0, a message from `sender`): "Delete" or "Leaving Scope" from one of the referenced
// objects (the AI first) clears that reference (its listener off); everything else to CObject's
static DWORD THISCALL QueueItem_OnMessage(BYTE *o, void *sender, const char *name)
{
    const DWORD names[] = {0x60aef8 /* "Delete" */, 0x60aee8 /* "Leaving Scope" */};
    const DWORD offs[] = {0xc, 0x10, 0x18};
    for (DWORD m : names)
        if (crt_stricmp(name, at<const char *>(m)) == 0) {
            if (ptr_at(o, 0x14) == sender) {
                const DWORD r = ai_listen(sender, o, false);
                dw(o, 0x14) = 0;
                return r;
            }
            for (DWORD off : offs)
                if (ptr_at(o, off) == sender) {
                    const DWORD r = part_listen(sender, o, false);
                    dw(o, off) = 0;
                    return r;
                }
        }
    return obj_on_message(o, sender, name);
}

// ---- CActionQueue (CObject, 0x1c: +8 the embedded "Items" array {vtable, data, count, capacity}, +0x18
// the layer) ---------------------------------------------------------------------------------------------------

static void Queue_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(0x5ed000));
    dw(o, 8) = zax::vt(0x5ed068);   // the pointer array's base vtable
    if (void *data = ptr_at(o, 0xc)) {
        pool_get_free(data, dw(o, 0x14) << 2);
        dw(o, 0xc) = 0;
        dw(o, 0x14) = 0;
    }
    dw(o, 0x10) = 0;
    obj_dtor(o);
}
// 0x41c4b0 (slot 1)
static void *THISCALL Queue_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, Queue_destroy>(self, flags); }

// 0x41c6d0: the constructor (CObject's, the array's vtable 0x5ed05c, the class's vtable, Object_InitFields,
// the layer 0). eax: this.
static BYTE *THISCALL Queue_Ctor(BYTE *o)
{
    obj_ctor(o);
    dw(o, 8) = zax::vt(0x5ed05c);
    dw(o, 0xc) = 0;
    dw(o, 0x10) = 0;
    dw(o, 0x14) = 0;
    dw(o, 0) = zax::vt(0x5ed000);
    obj_init_fields(o);
    dw(o, 0x18) = 0;
    return o;
}

static void *items_field(BYTE *q) { return find_field(vcall<CClassInfo *>(q, 2), glob<const char *>(0x60af10), 0); }   // "Items"

// 0x41c700: the queue's frame: from the last item down, the time runs down by dt; an item due (time not
// above 0) runs its action (with a target part slot 27 (trigger, AI, instigator, target), else slot 23
// through 0x41a740 in the queue's layer) and leaves the list ("Items" field, RemoveAt).
static void THISCALL Queue_Update(BYTE *q, float dt)
{
    for (int i = (int)dw(q, 0x10) - 1; i >= 0; i--) {
        BYTE *item = static_cast<BYTE **>(ptr_at(q, 0xc))[i];
        fl(item, 8) = (float)((long double)fl(item, 8) - dt);
        item = static_cast<BYTE **>(ptr_at(q, 0xc))[i];
        if (fl(item, 8) > 0)
            continue;
        if (void *action = ptr_at(item, 0x1c)) {
            if (ptr_at(item, 0x18))
                vcall<void>(action, 27, dw(item, 0x10), dw(item, 0x14), dw(item, 0xc), ptr_at(item, 0x18));
            else
                fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, void *)>(0x41a740)(action, ptr_at(q, 0x18), dw(item, 0x10), dw(item, 0x14),
                                                                                        dw(item, 0xc), q);
        }
        fn<void(THISCALL *)(void *, void *, DWORD)>(0x4199f0)(items_field(q), q, (DWORD)i);
    }
}

// 0x41c7a0: queues an action (owned by the item) to run after `delay`: a new item (pool 0x34; CObject's
// constructor, its vtable, Object_InitFields, the references 0) appended to "Items" and set (0x41c120)
static void THISCALL Queue_Add(BYTE *q, DWORD delay, void *a4, void *a2, void *a3, void *target, void *action)
{
    if (!action)
        return;
    void **slot = fn<void **(THISCALL *)(void *, void *)>(0x4199b0)(items_field(q), q);
    BYTE *item = static_cast<BYTE *>(pool_alloc(0x34));
    if (item) {
        obj_ctor(item);
        dw(item, 0) = zax::vt(VT_QUEUE_ITEM);
        obj_init_fields(item);
        dw(item, 0xc) = 0;
        dw(item, 0x10) = 0;
        dw(item, 0x14) = 0;
        dw(item, 0x18) = 0;
    }
    *slot = item;
    QueueItem_Set(item, delay, a4, a2, a3, target, action);
}

// ---- The trigger AIs CTriggerAI (0x20: +0x18 "Triggered By Name", +0x1c..+0x1e flags, +0x20 "Action"),
// CollideTriggerAI (0x38: +0x24 "Disallow Retrigger", +0x28 "Last Trigger Time", +0x30 "Action for Failed
// Retrigger", +0x34 hit) and
// CPassThroughTriggerAI (0x24) ------------------------------------------------------------------------------------

constexpr DWORD VT_LAYER_PART_AI = 0x5ed1d0, VT_TRIGGER_AI = 0x5ed128;
static void layer_part_ai_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(VT_LAYER_PART_AI));
    fn<void(THISCALL *)(void *)>(0x4fd080)(o);   // CAIBase's destructor
}
static void trigger_ai_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(VT_TRIGGER_AI));
    zs_dtor(o + 0x18);
    layer_part_ai_destroy(o);
}
static void CollideTrigger_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(0x5ed080));
    trigger_ai_destroy(o);
}
static void PassThroughTrigger_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(0x5ed278));
    trigger_ai_destroy(o);
}
static void RelayAI_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(0x5ed80c));
    layer_part_ai_destroy(o);
}
// 0x41c950, 0x41c980 (slot 1 of the abstract CLayerPartAI and CTriggerAI: no array path)
static void *THISCALL LayerPartAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor_single<0x10, layer_part_ai_destroy>(self, flags); }
static void *THISCALL TriggerAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor_single<0x20, trigger_ai_destroy>(self, flags); }
// 0x41cac0, 0x41d1f0, 0x41fcd0 (slot 1)
static void *THISCALL CollideTrigger_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x38, CollideTrigger_destroy>(self, flags); }
static void *THISCALL PassThroughTrigger_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x24, PassThroughTrigger_destroy>(self, flags);
}
static void *THISCALL RelayAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, RelayAI_destroy>(self, flags); }

// 0x41d0a0: CTriggerAI's action set (the old one deleted)
static void THISCALL TriggerAI_SetAction(BYTE *self, void *action)
{
    if (void *old = ptr_at(self, 0x20)) {
        vcall<void *>(old, 1, 1);
        dw(self, 0x20) = 0;
    }
    dw(self, 0x20) = pv(action);
}

// CTriggerAI's test (0x422d50: may the part trigger it) and its firing (0x422cd0: the action in a layer)
static bool trigger_allows(BYTE *self, BYTE *part, DWORD x) { return fn<bool(THISCALL *)(void *, void *, DWORD)>(0x422d50)(self, part, x); }
static void trigger_fire(BYTE *self, BYTE *part, void *action, DWORD x)
{
    fn<BYTE(THISCALL *)(void *, void *, void *, void *, DWORD)>(0x422cd0)(self, ptr_at(part, 0x48), action, part, x);
}

// 0x41cf10 (slot 34, a collision; not when `flag`): when the trigger allows the part, within "Disallow
// Retrigger" seconds of "Last Trigger Time" (both above 0) "Action for Failed Retrigger" fires, else the
// time is noted and "Action" fires; +0x34 set.
static void THISCALL CollideTrigger_Collide(BYTE *self, BYTE *part, DWORD x, DWORD, DWORD, BYTE flag)
{
    if (flag || !trigger_allows(self, part, x))
        return;
    const long double now = vcall<long double>(reporter(), 13);
    if (fl(self, 0x28) > 0 && fl(self, 0x24) > 0 && now < (long double)fl(self, 0x24) + fl(self, 0x28)) {
        trigger_fire(self, part, self + 0x30, x);
        by(self, 0x34) = 1;
        return;
    }
    fl(self, 0x28) = (float)now;
    trigger_fire(self, part, self + 0x20, x);
    by(self, 0x34) = 1;
}
// 0x41d3b0 (slot 34 of CPassThroughTriggerAI): the action when the trigger allows the part
static void THISCALL PassThroughTrigger_Collide(BYTE *self, BYTE *part, DWORD x, DWORD, DWORD, BYTE)
{
    if (trigger_allows(self, part, x))
        trigger_fire(self, part, self + 0x20, x);
}

// 0x41d3e0, 0x41d520: the editor's converting wrapper 0x62e790 (vtable 0x5ed074; +0xc "Enter Action",
// +0x10 "Action"): a CTouchingTriggerAI (or subclass) that is no CollideTriggerAI into a new
// CollideTriggerAI with the same fields and its "Enter Action" as "Action" (slots and field copy as the
// action wrappers of action_slots2.cpp)
static const char *THISCALL ToCollideTrigger_Describe(BYTE *self, void *ai, CClassInfo **cls)
{
    if (!is_kind_of(ai, class_at(fn_addr(0x421f40))))   // CTouchingTriggerAI
        return nullptr;
    if (vcall<CClassInfo *>(ai, 2) == class_at(fn_addr(0x41c900)))
        return nullptr;
    if (cls)
        *cls = class_at(fn_addr(0x41c900));
    if (const char *t = static_cast<const char *>(ptr_at(self, 8)))
        return t;
    return class_short_name(class_at(fn_addr(0x41c900)));
}
static BYTE *THISCALL ToCollideTrigger_Convert(BYTE *self, void *ai)
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x38));
    if (o) {
        fn<void *(THISCALL *)(void *)>(0x4fd060)(o);   // CAIBase's constructor
        dw(o, 0) = zax::vt(VT_LAYER_PART_AI);
        obj_init_fields(o);
        zs_ctor(o + 0x18);
        dw(o, 0) = zax::vt(VT_TRIGGER_AI);
        obj_init_fields(o);
        dw(o, 0) = zax::vt(0x5ed080);
        obj_init_fields(o);
        by(o, 0x34) = 0;
    }
    vcall<void>(o, 19);
    vcall<void>(o, 20, ai);
    if (const char *from = static_cast<const char *>(ptr_at(self, 0xc))) {
        void *f = find_field(vcall<CClassInfo *>(ai, 2), from, 0);
        void *t = find_field(vcall<CClassInfo *>(o, 2), static_cast<const char *>(ptr_at(self, 0x10)), 0);
        if (f && t)
            vcall<void>(t, 4, o, ai, f);
    }
    if (ai)
        vcall<void *>(ai, 1, 1);
    return o;
}

// ---- CDialogLine (CObject, 0x14: +8 "Name", +0xc "Text"), CDialogActionBase, CZaxDialogAction ----------------

static void DialogLine_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(0x5ed3e0));
    zs_dtor(o + 0xc);
    zs_dtor(o + 8);
    obj_dtor(o);
}
// 0x41de00 (slot 1)
static void *THISCALL DialogLine_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, DialogLine_destroy>(self, flags); }
// 0x41e010: the line's text into `out`. eax: ZString_Assign's.
static DWORD THISCALL DialogLine_Text(BYTE *self, ZString *out) { return zs_assign(out, self + 0xc); }
// 0x41dbe0 (CZaxDialogAction's slot 32, six arguments): 0
static DWORD THISCALL ZaxDialog_Slot32(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD) { return 0; }

// 0x41dbf0: the clone of a resource descriptor with the vtable 0x5ed320 (CDialogActionBase's "Line"):
// operator new 0x34, the copy constructor 0x54e270, the vtable. eax: the copy or 0.
static void *THISCALL ResourceField5ed320_Clone(void *f)
{
    BYTE *c = static_cast<BYTE *>(heap_alloc(0x34));
    if (!c)
        return nullptr;
    fn<void *(THISCALL *)(void *, void *)>(0x54e270)(c, f);
    dw(c, 0) = zax::vt(0x5ed320);
    return c;
}

// 0x41d7f0: with an object, its slot 3 and then slot 2, both with 0
static void THISCALL Dialog_ResetObject(void *, DWORD, void *o)
{
    if (o) {
        vcall<void>(o, 3, 0);
        vcall<void>(o, 2, 0);
    }
}

// ---- The property list (a named list of CDialogLine-like name/value items, 0x18: +8 data, +0xc count,
// +0x10 capacity, +0x14 its file) -------------------------------------------------------------------------------

// 0x41e020 (slot 1, no array path): every item deleted (slot 1) from the last, the file name, the
// array's data back to the pool, the named list's vtable
static void *THISCALL PropertyList_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = zax::vt(0x5ed448);
    while (DWORD n = dw(self, 0xc)) {
        const DWORD i = n - 1;
        if (void *item = static_cast<void **>(ptr_at(self, 8))[i])
            vcall<void *>(item, 1, 1);
        if (i < dw(self, 0xc)) {
            if (i + 1 < dw(self, 0xc)) {
                void **d = static_cast<void **>(ptr_at(self, 8));
                for (DWORD k = i; k + 1 < dw(self, 0xc); k++)   // memmove of the rest
                    d[k] = d[k + 1];
            }
            dw(self, 0xc)--;
        }
    }
    zs_dtor(self + 0x14);
    dw(self, 4) = zax::vt(0x5ed43c);
    if (void *data = ptr_at(self, 8)) {
        pool_get_free(data, dw(self, 0x10) << 2);
        dw(self, 8) = 0;
        dw(self, 0x10) = 0;
    }
    dw(self, 0xc) = 0;
    dw(self, 0) = zax::vt(0x5ec7cc);   // NamedList
    if (flags & 1)
        pool_free(self, 0x18);
    return self;
}
// 0x41e0e0 (slot): "property"
static const char *THISCALL PropertyList_Kind(void *) { return glob<const char *>(0x60b094); }

// ---- COtherMapAction (+8 "Action", +0xc "Other Map Name") ----------------------------------------------------

static void OtherMap_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(0x5ed710));
    zs_dtor(o + 0xc);
    Action_Dtor(o);
}
// 0x41ef20 (slot 1; the action is freed by Object_FreeFields)
static void *THISCALL OtherMap_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, OtherMap_destroy>(self, flags); }

// 0x41f130 (slot 23): a copy of the action goes to the other map (the application's +0x354 list, its
// slot 2 at +8) when the map exists; a name with '\\' gets the warning "Map Name *MAY NOT* contain
// backslashes ..." and is fixed to '/'. Without a name or with an unknown map, warnings (world slot 17;
// the application's slot 17) and al 0. al 1 otherwise.
static bool THISCALL OtherMap_Execute(BYTE *self, void *, DWORD, DWORD, DWORD)
{
    if (!zs_not_empty(self + 0xc)) {
        void *app = *glob<void **>(0x713f14);
        vcall<void>(app, 17, glob<const char *>(0x60b0dc));   // "No map name specified in OtherMap action"
        return false;
    }
    ZString map;
    fn<ZString *(THISCALL *)(void *, const void *)>(0x4dedf0)(&map, self + 0xc);
    if (crt_strchr(zs_c_str(&map), '\\')) {
        vcall<void>(world(), 17, glob<const char *>(0x60b118));
        fn<void(THISCALL *)(void *, char, char)>(0x4e12b0)(&map, '\\', '/');
    }
    if (!fn<bool(THISCALL *)(void *, const char *)>(0x4f4ca0)(world(), zs_c_str(&map))) {
        vcall<void>(world(), 17, glob<const char *>(0x60b108));   // "File Not found"
        vcall<void>(world(), 17, zs_c_str(&map));
        zs_dtor(&map);
        return false;
    }
    BYTE *list = static_cast<BYTE *>(ptr_at(*glob<void **>(0x713f14), 0x354));
    void *copy = obj_clone(self);
    vcall<void>(list + 8, 2, &copy);
    zs_dtor(&map);
    return true;
}

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(action_helpers)
{
    const Hook list[] = {
        {0x41a0a0, (void *)&Wrapper_Ctor, "CActionWrapper::Ctor", 8, {0x8b, 0xc1, 0xc7, 0x00, 0x44, 0xce, 0x5e, 0x00}},
        {0x419050, (void *)&FieldDesc_AltName, "FieldDesc_AltName", 0, {0x83, 0xc1, 0x1c, 0xe9, 0x48, 0x83, 0x0c, 0x00}},
        {0x41a6d0, (void *)&Action_DeletingDtor, "CAction::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x50, 0xce, 0x5e}},
        {0x41a700, (void *)&Action_Dtor, "CAction::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x50, 0xce, 0x5e}},
        {0x41a720, (void *)&Action_Execute, "CAction::Execute", 6, {0x8b, 0x54, 0x24, 0x10, 0x8b, 0x01, 0x52, 0x8b}},
        {0x41a740, (void *)&Action_ExecuteFrom, "CAction::ExecuteFrom", 6, {0x8b, 0x54, 0x24, 0x10, 0x8b, 0x01, 0x52, 0x8b}},
        {0x41a760, (void *)&Names_Resolve, "Action_ResolveNames", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x14, 0x8b, 0xd9}},
        {0x41a800, (void *)&Name_ResolveRange, "Action_ResolveRange", 5, {0x83, 0xec, 0x10, 0x53, 0x55, 0x56, 0x89, 0x4c}},
        {0x41a930, (void *)&Name_Resolve, "Action_ResolveName", 5, {0x83, 0xec, 0x20, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x41abd0, (void *)&Action_Report, "Action_Report", 5, {0x8a, 0x44, 0x24, 0x14, 0x56, 0x84, 0xc0, 0x8b}},
        {0x41ad40, (void *)&Name_ResolveOneZ, "Action_ResolveOneZ", 5, {0x8b, 0x44, 0x24, 0x18, 0x56, 0x8b, 0xf1, 0x50}},
        {0x41ad80, (void *)&Name_ResolveOne, "Action_ResolveOne", 5, {0x51, 0x53, 0x56, 0x8b, 0xf2, 0x57, 0x85, 0xf6}},
        {0x41ae90, (void *)&Action_EachTargetZ, "CAction::EachTargetZ", 8, {0x8b, 0x44, 0x24, 0x1c, 0x8b, 0x54, 0x24, 0x14}},
        {0x41aed0, (void *)&Action_EachTarget, "CAction::EachTarget", 8, {0x83, 0xec, 0x18, 0x56, 0x8b, 0x74, 0x24, 0x24}},
        {0x41b1d0, (void *)&Conditional_DeletingDtor, "CConditionalAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x41b7b0, (void *)&If_DeletingDtor, "CIfAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x41bba0, (void *)&Conditional_Execute, "CConditionalAction::Execute", 5, {0x51, 0x89, 0x4c, 0x24, 0x00, 0x8b, 0x49, 0x08}},
        {0x41bcd0, (void *)&QueueItem_DeletingDtor, "CActionQueueItem::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x41c0b0, (void *)&QueueItem_Dtor, "CActionQueueItem::Dtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x14, 0xc7, 0x06}},
        {0x41c120, (void *)&QueueItem_Set, "CActionQueueItem::Set", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x14, 0x85, 0xc9}},
        {0x41c230, (void *)&QueueItem_OnMessage, "CActionQueueItem::OnMessage", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x56, 0x57, 0x68}},
        {0x41c4b0, (void *)&Queue_DeletingDtor, "CActionQueue::DeletingDtor", 6, {0x51, 0x53, 0x8a, 0x5c, 0x24, 0x0c, 0x55, 0x56}},
        {0x41c6d0, (void *)&Queue_Ctor, "CActionQueue::Ctor", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x37, 0x9f, 0xff}, 5},
        {0x41c700, (void *)&Queue_Update, "CActionQueue::Update", 7, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7e, 0x10, 0x4f}},
        {0x41c7a0, (void *)&Queue_Add, "CActionQueue::Add", 5, {0x55, 0x8b, 0x6c, 0x24, 0x1c, 0x56, 0x57, 0x33}},
        {0x41c950, (void *)&LayerPartAI_DeletingDtor, "CLayerPartAI::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xd0, 0xd1, 0x5e}},
        {0x41c980, (void *)&TriggerAI_DeletingDtor, "CTriggerAI::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x28, 0xd1, 0x5e}},
        {0x41cac0, (void *)&CollideTrigger_DeletingDtor, "CollideTriggerAI::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x41cf10, (void *)&CollideTrigger_Collide, "CollideTriggerAI::Collide", 5, {0x8a, 0x44, 0x24, 0x14, 0x53, 0x56, 0x57, 0x84}},
        {0x41d0a0, (void *)&TriggerAI_SetAction, "CTriggerAI::SetAction", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x20, 0x85, 0xc9}},
        {0x41d1f0, (void *)&PassThroughTrigger_DeletingDtor, "CPassThroughTriggerAI::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x41d3b0, (void *)&PassThroughTrigger_Collide, "CPassThroughTriggerAI::Collide", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x56, 0x57, 0x8b}},
        {0x41d3e0, (void *)&ToCollideTrigger_Describe, "CToCollideTriggerWrapper::Describe", 5, {0x53, 0x56, 0x57, 0x8b, 0xd9, 0xe8, 0x56, 0x4b}},
        {0x41d520, (void *)&ToCollideTrigger_Convert, "CToCollideTriggerWrapper::Convert", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0xb9, 0x38}},
        {0x41d7f0, (void *)&Dialog_ResetObject, "CDialogActionBase::ResetObject", 5, {0x56, 0x8b, 0x74, 0x24, 0x0c, 0x85, 0xf6, 0x74}},
        {0x41dbe0, (void *)&ZaxDialog_Slot32, "CZaxDialogAction::Slot32", 5, {0x33, 0xc0, 0xc2, 0x18, 0x00, 0x90, 0x90, 0x90}},
        {0x41dbf0, (void *)&ResourceField5ed320_Clone, "ResourceField_Clone_5ed320", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x34, 0xe8, 0x3f}},
        {0x41de00, (void *)&DialogLine_DeletingDtor, "CDialogLine::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x41e010, (void *)&DialogLine_Text, "CDialogLine::Text", 8, {0x83, 0xc1, 0x0c, 0x51, 0x8b, 0x4c, 0x24, 0x08}},
        {0x41e020, (void *)&PropertyList_DeletingDtor, "CPropertyList::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x48, 0xd4, 0x5e}},
        {0x41e0e0, (void *)&PropertyList_Kind, "CPropertyList::Kind", 5, {0xb8, 0x94, 0xb0, 0x60, 0x00, 0xc3, 0x90, 0x90}},
        {0x41ef20, (void *)&OtherMap_DeletingDtor, "COtherMapAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x41f130, (void *)&OtherMap_Execute, "COtherMapAction::Execute", 5, {0x83, 0xec, 0x08, 0x56, 0x57, 0x8b, 0xf9, 0x8d}},
        {0x41fcd0, (void *)&RelayAI_DeletingDtor, "CRelayAI::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
