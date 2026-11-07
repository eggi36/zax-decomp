// Class registration of the action classes reimplemented (see action_classes.h): constructors (inlined
// into Create and CreateArray in the exe), Create, CreateArray, class getters and RegisterFields.
//
// Every constructor below is one level per class, as the exe inlines it: the parent's construct, the
// members, then vtable_and_defaults. RegisterFields construct one descriptor after the other (operator
// new, the descriptor kind's constructor, for "$Trigger"-style name fields the vtable 0x5ed924 of a
// subclass written over it) and append it to the class; a failed allocation appends a null descriptor
// as in the original. Values are the exe's: strings by address (text in the comment), floats as their
// bits (value in the comment).
#include "action_classes.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);
static void action_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x41a6b0)(o); }
static void obj_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x416610)(o); }
static void obj_init_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static CClassInfo *__cdecl action_class() { return fn<Getter>(0x41a650)(); }
static CClassInfo *class_at(DWORD getter) { return fn_dyn<Getter>(getter)(); }
template <DWORD A> static CClassInfo *__cdecl getter() { return fn<Getter>(A)(); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void *last_field(CClassInfo *c) { return fn<void *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
static DWORD set_help(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419020)(f, at<const char *>(text)); }
static DWORD set_alt_name(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419030)(f, at<const char *>(text)); }
static void *find_field(CClassInfo *c, DWORD name, DWORD index)
{
    return fn<void *(THISCALL *)(CClassInfo *, const char *, DWORD)>(0x4154d0)(c, at<const char *>(name), index);
}
static void zs_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static DWORD zs_assign(ZString *z, const ZString *v) { return (DWORD)(uintptr_t)fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(z, v); }
static DWORD zs_assign_text(ZString *z, DWORD text)
{
    return (DWORD)(uintptr_t)fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, at<const char *>(text));
}

constexpr DWORD TEXT_EMPTY = 0x62e558;   // ""
constexpr DWORD VT_ACTION = 0x5ece50;

// Descriptor constructors (thiscall; the arguments as the exe pushes them, see field_kinds.h)
constexpr DWORD STRING_FIELD_CTOR = 0x415dd0;           // 0x34 bytes
constexpr DWORD OBJECT_FIELD_CTOR = 0x4186f0;           // 0x3c
constexpr DWORD AI_FIELD_CTOR = 0x4184c0;               // 0x34
constexpr DWORD LIST_FIELD_CTOR = 0x417930;             // 0x44
constexpr DWORD OBJECT_ARRAY_FIELD_CTOR2 = 0x419230;    // 0x48
constexpr DWORD FLAGS_FIELD_CTOR = 0x414cf0;            // 0x38
constexpr DWORD EMBEDDED_FIELD_CTOR_VALUE = 0x5284c0;   // 0x34
constexpr DWORD INT_FIELD_CTOR_UNITS = 0x54d590;        // 0x40
constexpr DWORD ENUM_INT_FIELD_CTOR = 0x54d6e0;         // 0x44
constexpr DWORD ENUM_INT_FIELD_CTOR_UNITS = 0x54d7f0;   // 0x44
constexpr DWORD BYTE_FIELD_CTOR = 0x54ddf0;             // 0x30
constexpr DWORD RESOURCE_FIELD_CTOR_TYPED = 0x54e560;   // 0x38
constexpr DWORD FLOAT_FIELD_CTOR_UNITS = 0x54ec40;      // 0x44
constexpr DWORD FLOAT_FIELD_CTOR = 0x54ecd0;            // 0x44
constexpr DWORD OBJECT_REF_FIELD_CTOR = 0x523e30;        // 0x30
constexpr DWORD RESOURCE_FIELD_CTOR = 0x54e220;         // 0x34

// ---- The shared pattern ---------------------------------------------------------------------------------

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static ZString *zstring(void *p, DWORD off) { return reinterpret_cast<ZString *>(static_cast<BYTE *>(p) + off); }

// The end of every constructor level: the class's vtable, then Object_InitFields (the field defaults
// through the class info that vtable's slot 2 gives)
static void vtable_and_defaults(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vtable;
    obj_init_fields(o);
}

using Construct = void (*)(BYTE *);

// Create: a pool block, constructed (null when the pool gives none)
template <DWORD SIZE, Construct C> static void *create_one()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(SIZE));
    if (!o)
        return nullptr;
    C(o);
    return o;
}

// CreateArray: heap block with the count in front, every element constructed (none for a count below 1)
template <DWORD SIZE, Construct C> static void *create_array(int count)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc((DWORD)count * SIZE + 4));
    if (!block)
        return nullptr;
    block[0] = (DWORD)count;
    BYTE *e = reinterpret_cast<BYTE *>(block + 1);
    if ((int)((DWORD)count - 1) >= 0)
        for (DWORD i = 0; i < (DWORD)count; i++)
            C(e + i * SIZE);
    return e;
}

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is
// called after the block was allocated, parent null = derived from CObject)
static CClassInfo *class_getter(DWORD var, DWORD name, float version, Getter parent, DWORD size, DWORD abstract, DWORD create,
                                DWORD create_array, DWORD register_fields)
{
    CClassInfo *&v = *at<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, at<const char *>(name), version, parent ? parent() : nullptr, size, abstract, create,
                                     create_array, register_fields);
    return v;
}

// A descriptor: operator new, then the kind's constructor (null when operator new gives none)
template <typename... A> static void *new_field(DWORD size, DWORD ctor, A... args)
{
    void *f = heap_alloc(size);
    if (f)
        f = fn_dyn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...);
    return f;
}
// The same with a subclass's vtable written over the constructor's
template <typename... A> static void *new_field_vt(DWORD size, DWORD vtable, DWORD ctor, A... args)
{
    void *f = heap_alloc(size);
    if (f) {
        fn_dyn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...);
        dw(f, 0) = vtable;
    }
    return f;
}

// ---- CAction --------------------------------------------------------------------------------------------

// 0x41a6b0: CObject's constructor, CAction's vtable, Object_InitFields. eax: this.
static void *THISCALL Action_Ctor(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(VT_ACTION));
    return o;
}

// 0x41a650
static CClassInfo *__cdecl Action_ClassInfo()
{
    return class_getter(glob_addr(0x62e770), glob_addr(0x60ade0) /* "CAction" */, 1.0f, nullptr, 0x8, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x58ea40));
}

// ---- CRemoveVelocityAction ----------------------------------------------------------------------

// class 0x6bfd14, version 1, size 0x8, parent CAction, vtable 0x5f4eec
static void RemoveVelocity_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(0x5f4eec));
}
// (slot 2; the class registers no fields of its own)
static CClassInfo *__cdecl RemoveVelocity_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd14), glob_addr(0x6141f8) /* "CRemoveVelocityAction" */, 1.0f, action_class, 0x8, 0, fn_addr(0x4b62d0), fn_addr(0x4b6300), fn_addr(0x58ea40));
}
// 0x4b62d0
static void *__cdecl RemoveVelocity_Create() { return create_one<0x8, RemoveVelocity_construct>(); }
// 0x4b6300
static void *__fastcall RemoveVelocity_CreateArray(int count, int, int) { return create_array<0x8, RemoveVelocity_construct>(count); }

// ---- CDelayAction -------------------------------------------------------------------------------

// class 0x6bfd18, version 1, size 0x18, parent CAction, vtable 0x5f4f5c
static void Delay_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(0x5f4f5c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Delay_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd18), glob_addr(0x614210) /* "CDelayAction" */, 1.0f, action_class, 0x18, 0, fn_addr(0x4b6540), fn_addr(0x4b6570), fn_addr(0x4b66b0));
}
// 0x4b6540
static void *__cdecl Delay_Create() { return create_one<0x18, Delay_construct>(); }
// 0x4b6570
static void *__fastcall Delay_CreateArray(int count, int, int) { return create_array<0x18, Delay_construct>(count); }
// 0x4b66b0. eax: the last call's.
static DWORD __cdecl Delay_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x61424c) /* "Next Action" */, 0x10, 4, 0x10, action_class(), 1, 0, 0, 0);
    add_field(Delay_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x614244) /* "Delay" */, glob_addr(0x60b000) /* "Seconds" */, 8, 4, 8, 1, 0x3f800000 /* 1.0 */, 0, 0, 0x4b18967f /* 9999999.0 */, 0x3dcccccd /* 0.1 */);
    add_field(Delay_ClassInfo(), f);
    set_alt_name(last_field(Delay_ClassInfo()), glob_addr(0x614230) /* "Seconds to Delay" */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x60b28c) /* "Plus or Minus" */, glob_addr(0x60b000) /* "Seconds" */, 0xc, 4, 0xc, 1, 0, 0, 0, 0x497423f0 /* 999999.0 */, 0x3dcccccd /* 0.1 */);
    add_field(Delay_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x614220) /* "Forget Trigger" */, 0x14, 1, 0x14, 1, 0, 0);
    return add_field(Delay_ClassInfo(), f);
}

// ---- COnlyOnceAction ----------------------------------------------------------------------------

// class 0x6bfd1c, version 2, size 0xc, parent CAction, vtable 0x5f4fcc
static void OnlyOnce_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(0x5f4fcc));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl OnlyOnce_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd1c), glob_addr(0x614258) /* "COnlyOnceAction" */, 2.0f, action_class, 0xc, 0, fn_addr(0x4b6a40), fn_addr(0x4b6a70), fn_addr(0x4b6bb0));
}
// 0x4b6a40
static void *__cdecl OnlyOnce_Create() { return create_one<0xc, OnlyOnce_construct>(); }
// 0x4b6a70
static void *__fastcall OnlyOnce_CreateArray(int count, int, int) { return create_array<0xc, OnlyOnce_construct>(count); }
// 0x4b6bb0. eax: the class.
static DWORD __cdecl OnlyOnce_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60aed0) /* "Action" */, 8, 4, 8, action_class(), 1, 0, 0, 0);
    add_field(OnlyOnce_ClassInfo(), f);
    CClassInfo *c = OnlyOnce_ClassInfo();
    c->alt_name = glob<const char *>(0x614268);   // "CDoOnceAction", the old class name
    return (DWORD)(uintptr_t)c;
}

// ---- COnlyOnceEveryXSecondsAction ---------------------------------------------------------------

// class 0x6bfd20, version 2, size 0x18, parent CAction, vtable 0x5f503c
static void OnlyOnceEveryXSeconds_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(0x5f503c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl OnlyOnceEveryXSeconds_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd20), glob_addr(0x614278) /* "COnlyOnceEveryXSecondsAction" */, 2.0f, action_class, 0x18, 0, fn_addr(0x4b6d50), fn_addr(0x4b6d80), fn_addr(0x4b6ec0));
}
// 0x4b6d50
static void *__cdecl OnlyOnceEveryXSeconds_Create() { return create_one<0x18, OnlyOnceEveryXSeconds_construct>(); }
// 0x4b6d80
static void *__fastcall OnlyOnceEveryXSeconds_CreateArray(int count, int, int) { return create_array<0x18, OnlyOnceEveryXSeconds_construct>(count); }
// 0x4b6ec0. eax: the last call's.
static DWORD __cdecl OnlyOnceEveryXSeconds_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60aed0) /* "Action" */, 8, 4, 8, action_class(), 1, 0, 0, 0);
    add_field(OnlyOnceEveryXSeconds_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x6142ac) /* "Min Time" */, glob_addr(0x60b000) /* "Seconds" */, 0x10, 4, 0x10, 1, 0x40a00000 /* 5.0 */, 0, 0, 0x4b18967f /* 9999999.0 */, 0);
    add_field(OnlyOnceEveryXSeconds_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x614298) /* "Next Alowable Time" */, 0x14, 4, 0x14, 0, 0, 0, 0, 0, 0);
    add_field(OnlyOnceEveryXSeconds_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60ae44) /* "Fail Action" */, 0xc, 4, 0xc, action_class(), 1, 0, 0, 0);
    return add_field(OnlyOnceEveryXSeconds_ClassInfo(), f);
}

// ---- CImpersonateAction -------------------------------------------------------------------------

// class 0x6bfd24, version 1, size 0x14, parent CAction, vtable 0x5f50ac
static void Impersonate_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Trigger"
    zs_ctor(zstring(o, 0xc));   // "Instigator"
    vtable_and_defaults(o, vt(0x5f50ac));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Impersonate_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd24), glob_addr(0x6142b8) /* "CImpersonateAction" */, 1.0f, action_class, 0x14, 0, fn_addr(0x4b71e0), fn_addr(0x4b7220), fn_addr(0x4b73a0));
}
// 0x4b71e0
static void *__cdecl Impersonate_Create() { return create_one<0x14, Impersonate_construct>(); }
// 0x4b7220
static void *__fastcall Impersonate_CreateArray(int count, int, int) { return create_array<0x14, Impersonate_construct>(count); }
// 0x4b73a0. eax: the last call's.
static DWORD __cdecl Impersonate_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60aed0) /* "Action" */, 0x10, 4, 0x10, action_class(), 1, 0, 0, 0);
    add_field(Impersonate_ClassInfo(), f);
    f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6142d8) /* "Trigger" */, 8, 4, 8, 1, glob_addr(0x6142e0) /* "$Trigger" */, 0);
    add_field(Impersonate_ClassInfo(), f);
    f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x60aec4) /* "Instigator" */, 0xc, 4, 0xc, 1, glob_addr(0x6142cc) /* "$Instigator" */, 0);
    return add_field(Impersonate_ClassInfo(), f);
}

// ---- CWhileAction -------------------------------------------------------------------------------

// class 0x6bfd28, version 1, size 0x10, parent CAction, vtable 0x5f5134
static void While_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(0x5f5134));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl While_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd28), glob_addr(0x61432c) /* "CWhileAction" */, 1.0f, action_class, 0x10, 0, fn_addr(0x4b7920), fn_addr(0x4b7950), fn_addr(0x4b7a90));
}
// 0x4b7920
static void *__cdecl While_Create() { return create_one<0x10, While_construct>(); }
// 0x4b7950
static void *__fastcall While_CreateArray(int count, int, int) { return create_array<0x10, While_construct>(count); }
// 0x4b7a90. eax: the last call's.
static DWORD __cdecl While_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x614350) /* "Condition" */, 8, 4, 8, action_class(), 1, 0, 0, 0);
    add_field(While_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x61433c) /* "Action To Repeat" */, 0xc, 4, 0xc, action_class(), 1, 0, 0, 0);
    return add_field(While_ClassInfo(), f);
}

// ---- CRepeatAction ------------------------------------------------------------------------------

// class 0x6bfd2c, version 1, size 0x14, parent CAction, vtable 0x5f51a4
static void Repeat_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(0x5f51a4));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Repeat_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd2c), glob_addr(0x61438c) /* "CRepeatAction" */, 1.0f, action_class, 0x14, 0, fn_addr(0x4b7cc0), fn_addr(0x4b7cf0), fn_addr(0x4b7e30));
}
// 0x4b7cc0
static void *__cdecl Repeat_Create() { return create_one<0x14, Repeat_construct>(); }
// 0x4b7cf0
static void *__fastcall Repeat_CreateArray(int count, int, int) { return create_array<0x14, Repeat_construct>(count); }
// 0x4b7e30. eax: the last call's.
static DWORD __cdecl Repeat_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x6143ac) /* "Number of Repetitions" */, 8, 4, 8, 1, 0, 0, 0, 0);
    add_field(Repeat_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x61433c) /* "Action To Repeat" */, 0xc, 4, 0xc, action_class(), 1, 0, 0, 0);
    add_field(Repeat_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x61439c) /* "Delay Between" */, glob_addr(0x60b000) /* "Seconds" */, 0x10, 4, 0x10, 1, 0, 0, 0, 0x47c34f80 /* 99999.0 */, 0x3dcccccd /* 0.1 */);
    return add_field(Repeat_ClassInfo(), f);
}

// ---- CMultipleActionsAction ---------------------------------------------------------------------

// class 0x6bfd30, version 3, size 0x18, parent CAction, vtable 0x5f5220
static void MultipleActions_construct(BYTE *o)
{
    action_ctor(o);
    dw(o, 0xc) = 0;      // "Action": the embedded pointer array's data
    dw(o, 0x10) = 0;     // its count
    dw(o, 0x14) = 0;     // its capacity [confirmed: slot 23 and the deleting destructor, action_slots.cpp]
    dw(o, 0x8) = vt(0x5f5290);   // vtable of the embedded pointer array (the actions)
    vtable_and_defaults(o, vt(0x5f5220));
}
// 0x4b8460
static CClassInfo *__cdecl MultipleActions_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd30), glob_addr(0x6143dc) /* "CMultipleActionsAction" */, 3.0f, action_class, 0x18, 0, fn_addr(0x4b84b0), fn_addr(0x4b8500), fn_addr(0x4b86d0));
}
// 0x4b84b0
static void *__cdecl MultipleActions_Create() { return create_one<0x18, MultipleActions_construct>(); }
// 0x4b8500
static void *__fastcall MultipleActions_CreateArray(int count, int, int) { return create_array<0x18, MultipleActions_construct>(count); }
// 0x4b86d0. eax: the last call's.
static DWORD __cdecl MultipleActions_RegisterFields()
{
    void *element = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60aed0) /* "Action" */, 0, 4, 0, action_class(), 1, 0, 0, 0);
    void *f = new_field(0x48, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x60aed0) /* "Action" */, 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc, 4, 0xc, 1, element, 1, 0);
    return add_field(MultipleActions_ClassInfo(), f);
}

// ---- CMultipleDelayedActionsAction --------------------------------------------------------------

// class 0x6bfd34, version 1, size 0x1c, parent CMultipleActionsAction, vtable 0x5f52c0
static void MultipleDelayedActions_construct(BYTE *o)
{
    MultipleActions_construct(o);
    vtable_and_defaults(o, vt(0x5f52c0));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl MultipleDelayedActions_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd34), glob_addr(0x614428) /* "CMultipleDelayedActionsAction" */, 1.0f, MultipleActions_ClassInfo, 0x1c, 0, fn_addr(0x4b8930), fn_addr(0x4b8990), fn_addr(0x4b8c10));
}
// 0x4b8930
static void *__cdecl MultipleDelayedActions_Create() { return create_one<0x1c, MultipleDelayedActions_construct>(); }
// 0x4b8990
static void *__fastcall MultipleDelayedActions_CreateArray(int count, int, int) { return create_array<0x1c, MultipleDelayedActions_construct>(count); }
// 0x4b8c10. eax: the last call's.
static DWORD __cdecl MultipleDelayedActions_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x60b2b4) /* "Delay Between Actions" */, glob_addr(0x60b000) /* "Seconds" */, 0x18, 4, 0x18, 1, 0x3f800000 /* 1.0 */, 0, 0, 0x497423f0 /* 999999.0 */, 0);
    return add_field(MultipleDelayedActions_ClassInfo(), f);
}

// ---- CSeriesAction ------------------------------------------------------------------------------

// class 0x6bfd38, version 2, size 0x24, parent CMultipleActionsAction, vtable 0x5f533c
static void Series_construct(BYTE *o)
{
    MultipleActions_construct(o);
    vtable_and_defaults(o, vt(0x5f533c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Series_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd38), glob_addr(0x614448) /* "CSeriesAction" */, 2.0f, MultipleActions_ClassInfo, 0x24, 0, fn_addr(0x4b8ed0), fn_addr(0x4b8f30), fn_addr(0x4b91a0));
}
// 0x4b8ed0
static void *__cdecl Series_Create() { return create_one<0x24, Series_construct>(); }
// 0x4b8f30
static void *__fastcall Series_CreateArray(int count, int, int) { return create_array<0x24, Series_construct>(count); }
// 0x4b91a0. eax: the last call's.
static DWORD __cdecl Series_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x614494) /* "When Done" */, 0x20, 4, 0x20, glob_addr(0x6bfc38), 1, glob_addr(0x6141e8) /* "Repeat Series" */, 0, 0);
    add_field(Series_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x614478) /* "Require Success To Advance" */, 0x1c, 1, 0x1c, 1, 1, 0);
    add_field(Series_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x614458) /* "Debug/Next Action Index" */, glob_addr(0x614470) /* "0 Based" */, 0x18, 4, 0x18, 3, 0, 0, 0, 0);
    add_field(Series_ClassInfo(), f);
    dw(f, 0x38) = glob_addr(0x6be330);   // the converter 0x6be330 (as for "Team Number", layer_parts.cpp), the units cleared
    return zs_assign_text(zstring(f, 0x20), glob_addr(TEXT_EMPTY));
}

// ---- CShuffledSeriesAction ----------------------------------------------------------------------

// class 0x6bfd3c, version 2, size 0x24, parent CSeriesAction, vtable 0x5f53bc
static void ShuffledSeries_construct(BYTE *o)
{
    Series_construct(o);
    vtable_and_defaults(o, vt(0x5f53bc));
}
// (slot 2; the class registers no fields of its own)
static CClassInfo *__cdecl ShuffledSeries_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd3c), glob_addr(0x6144a0) /* "CShuffledSeriesAction" */, 2.0f, Series_ClassInfo, 0x24, 0, fn_addr(0x4b9620), fn_addr(0x4b9690), fn_addr(0x58ea40));
}
// 0x4b9620
static void *__cdecl ShuffledSeries_Create() { return create_one<0x24, ShuffledSeries_construct>(); }
// 0x4b9690
static void *__fastcall ShuffledSeries_CreateArray(int count, int, int) { return create_array<0x24, ShuffledSeries_construct>(count); }

// ---- CRandomAction ------------------------------------------------------------------------------

// class 0x6bfd40, version 1, size 0x18, parent CMultipleActionsAction, vtable 0x5f543c
static void Random_construct(BYTE *o)
{
    MultipleActions_construct(o);
    vtable_and_defaults(o, vt(0x5f543c));
}
// (slot 2; the class registers no fields of its own)
static CClassInfo *__cdecl Random_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd40), glob_addr(0x6144b8) /* "CRandomAction" */, 1.0f, MultipleActions_ClassInfo, 0x18, 0, fn_addr(0x4b9ac0), fn_addr(0x4b9b20), fn_addr(0x58ea40));
}
// 0x4b9ac0
static void *__cdecl Random_Create() { return create_one<0x18, Random_construct>(); }
// 0x4b9b20
static void *__fastcall Random_CreateArray(int count, int, int) { return create_array<0x18, Random_construct>(count); }

// ---- CAndAction ---------------------------------------------------------------------------------

// class 0x6bfd44, version 2, size 0x1c, parent CMultipleActionsAction, vtable 0x5f54b8
static void And_construct(BYTE *o)
{
    MultipleActions_construct(o);
    vtable_and_defaults(o, vt(0x5f54b8));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl And_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd44), glob_addr(0x6144c8) /* "CAndAction" */, 2.0f, MultipleActions_ClassInfo, 0x1c, 0, fn_addr(0x4b9ed0), fn_addr(0x4b9f30), fn_addr(0x4ba1b0));
}
// 0x4b9ed0
static void *__cdecl And_Create() { return create_one<0x1c, And_construct>(); }
// 0x4b9f30
static void *__fastcall And_CreateArray(int count, int, int) { return create_array<0x1c, And_construct>(count); }
// 0x4ba1b0. eax: the last call's.
static DWORD __cdecl And_RegisterFields()
{
    void *f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x6144d4) /* "Early Exit" */, 0x18, 1, 0x18, 1, 0, 0);
    return add_field(And_ClassInfo(), f);
}

// ---- CXOrAction ---------------------------------------------------------------------------------

// class 0x6bfd48, version 2, size 0x1c, parent CMultipleActionsAction, vtable 0x5f5534
static void XOr_construct(BYTE *o)
{
    MultipleActions_construct(o);
    vtable_and_defaults(o, vt(0x5f5534));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl XOr_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd48), glob_addr(0x6144e0) /* "CXOrAction" */, 2.0f, MultipleActions_ClassInfo, 0x1c, 0, fn_addr(0x4ba3f0), fn_addr(0x4ba450), fn_addr(0x4ba6d0));
}
// 0x4ba3f0
static void *__cdecl XOr_Create() { return create_one<0x1c, XOr_construct>(); }
// 0x4ba450
static void *__fastcall XOr_CreateArray(int count, int, int) { return create_array<0x1c, XOr_construct>(count); }
// 0x4ba6d0. eax: the last call's.
static DWORD __cdecl XOr_RegisterFields()
{
    void *f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x6144d4) /* "Early Exit" */, 0x18, 1, 0x18, 1, 0, 0);
    return add_field(XOr_ClassInfo(), f);
}

// ---- COrAction ----------------------------------------------------------------------------------

// class 0x6bfd4c, version 1, size 0x1c, parent CMultipleActionsAction, vtable 0x5f55b0
static void Or_construct(BYTE *o)
{
    MultipleActions_construct(o);
    vtable_and_defaults(o, vt(0x5f55b0));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Or_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd4c), glob_addr(0x6144ec) /* "COrAction" */, 1.0f, MultipleActions_ClassInfo, 0x1c, 0, fn_addr(0x4ba930), fn_addr(0x4ba990), fn_addr(0x4bac10));
}
// 0x4ba930
static void *__cdecl Or_Create() { return create_one<0x1c, Or_construct>(); }
// 0x4ba990
static void *__fastcall Or_CreateArray(int count, int, int) { return create_array<0x1c, Or_construct>(count); }
// 0x4bac10. eax: the last call's.
static DWORD __cdecl Or_RegisterFields()
{
    void *f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x6144d4) /* "Early Exit" */, 0x18, 1, 0x18, 1, 0, 0);
    return add_field(Or_ClassInfo(), f);
}

// ---- CSucceedAction -----------------------------------------------------------------------------

// class 0x6bfd50, version 1, size 0xc, parent CAction, vtable 0x5f562c
static void Succeed_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(0x5f562c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Succeed_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd50), glob_addr(0x6144f8) /* "CSucceedAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x4badf0), fn_addr(0x4bae20), fn_addr(0x4baf60));
}
// 0x4badf0
static void *__cdecl Succeed_Create() { return create_one<0xc, Succeed_construct>(); }
// 0x4bae20
static void *__fastcall Succeed_CreateArray(int count, int, int) { return create_array<0xc, Succeed_construct>(count); }
// 0x4baf60. eax: the last call's.
static DWORD __cdecl Succeed_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60aed0) /* "Action" */, 8, 4, 8, action_class(), 1, 0, 0, 0);
    return add_field(Succeed_ClassInfo(), f);
}

// ---- CFailAction --------------------------------------------------------------------------------

// class 0x6bfd54, version 1, size 0xc, parent CAction, vtable 0x5f569c
static void Fail_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(0x5f569c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Fail_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd54), glob_addr(0x614508) /* "CFailAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x4bb070), fn_addr(0x4bb0a0), fn_addr(0x4bb1e0));
}
// 0x4bb070
static void *__cdecl Fail_Create() { return create_one<0xc, Fail_construct>(); }
// 0x4bb0a0
static void *__fastcall Fail_CreateArray(int count, int, int) { return create_array<0xc, Fail_construct>(count); }
// 0x4bb1e0. eax: the last call's.
static DWORD __cdecl Fail_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60aed0) /* "Action" */, 8, 4, 8, action_class(), 1, 0, 0, 0);
    return add_field(Fail_ClassInfo(), f);
}

// ---- CNotAction ---------------------------------------------------------------------------------

// class 0x6bfd58, version 1, size 0xc, parent CAction, vtable 0x5f570c
static void Not_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(0x5f570c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Not_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd58), glob_addr(0x614514) /* "CNotAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x4bb2f0), fn_addr(0x4bb320), fn_addr(0x4bb460));
}
// 0x4bb2f0
static void *__cdecl Not_Create() { return create_one<0xc, Not_construct>(); }
// 0x4bb320
static void *__fastcall Not_CreateArray(int count, int, int) { return create_array<0xc, Not_construct>(count); }
// 0x4bb460. eax: the last call's.
static DWORD __cdecl Not_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60aed0) /* "Action" */, 8, 4, 8, action_class(), 1, 0, 0, 0);
    return add_field(Not_ClassInfo(), f);
}

// ---- CCreateLayerPartAction ---------------------------------------------------------------------

// class 0x6bfd60, version 1, size 0x18, abstract, parent CAction
static void CreateLayerPart_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "New Location"
    vtable_and_defaults(o, vt(0x5f57f0));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl CreateLayerPart_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd60), glob_addr(0x614530) /* "CCreateLayerPartAction" */, 1.0f, action_class, 0x18, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x4bbe00));
}
// 0x4bbe00. eax: the last call's.
static DWORD __cdecl CreateLayerPart_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x614548) /* "New Location" */, 8, 4, 8, 1, glob_addr(0x6142e0) /* "$Trigger" */, 0);
    add_field(CreateLayerPart_ClassInfo(), f);
    set_help(last_field(CreateLayerPart_ClassInfo()), glob_addr(0x614668) /* "Optional: If no new location is specified then the new entity will ..." */);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x61465c) /* "Copy Height" */, 0xc, 1, 0xc, 1, 0, 0);
    add_field(CreateLayerPart_ClassInfo(), f);
    set_help(last_field(CreateLayerPart_ClassInfo()), glob_addr(0x6145ac) /* "Checking this box will copy the height of the entity \nat \"New loca..." */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x614594) /* "Random Location Delta" */, glob_addr(0x60b360) /* "Pixels" */, 0x10, 4, 0x10, 1, 0, 0, 0, 0, 0);
    add_field(CreateLayerPart_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x614584) /* "Warp Behavior" */, 0x14, 4, 0x14, class_at(fn_addr(0x55e350)) /* CRenderEffectBehavior */, 1, 0, 0, 0);
    return add_field(CreateLayerPart_ClassInfo(), f);
}

// ---- CCloneAction -------------------------------------------------------------------------------

// class 0x6bfd5c, version 3, size 0x28, parent CCreateLayerPartAction, vtable 0x5f577c
static void Clone_construct(BYTE *o)
{
    CreateLayerPart_construct(o);
    zs_ctor(zstring(o, 0x18));   // "Source Name"
    zs_ctor(zstring(o, 0x1c));   // "New Name"
    vtable_and_defaults(o, vt(0x5f577c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Clone_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd5c), glob_addr(0x614520) /* "CCloneAction" */, 3.0f, CreateLayerPart_ClassInfo, 0x28, 0, fn_addr(0x4bb620), fn_addr(0x4bb680), fn_addr(0x4bb8a0));
}
// 0x4bb620
static void *__cdecl Clone_Create() { return create_one<0x28, Clone_construct>(); }
// 0x4bb680
static void *__fastcall Clone_CreateArray(int count, int, int) { return create_array<0x28, Clone_construct>(count); }
// 0x4bb8a0. eax: the last call's.
static DWORD __cdecl Clone_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x614578) /* "Source Name" */, 0x18, 4, 0x18, 1, 0, 0);
    add_field(Clone_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x61456c) /* "New Name" */, 0x1c, 4, 0x1c, 1, 0, 0);
    add_field(Clone_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x60b2f0) /* "Set Facing Angle" */, 0x24, 1, 0x24, 1, 0, 0);
    add_field(Clone_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x614558) /* "New Facing Angle" */, glob_addr(0x60ff9c) /* "Radians" */, 0x20, 4, 0x20, 1, 0, 0, 0, 0x40c90fdb /* 6.2831855 */, 0x3dcccccd /* 0.1 */);
    add_field(Clone_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);   // the unit converter 0x6be328 (the value shown in degrees)
    zs_assign_text(zstring(f, 0x20), glob_addr(0x60ff80) /* "Degrees" */);
    void *d = find_field(Clone_ClassInfo(), glob_addr(0x614548) /* "New Location" */, 0);
    return vcall<DWORD>(d, 48, glob<const char *>(TEXT_EMPTY));   // the inherited "New Location": "" instead of "$Trigger" [assumption: slot 48 sets the default]
}

// ---- CCreateDefaultEntityAction -----------------------------------------------------------------

// class 0x6bfd64, version 1, size 0x20, parent CCreateLayerPartAction, vtable 0x5f5864
static void CreateDefaultEntity_construct(BYTE *o)
{
    CreateLayerPart_construct(o);
    zs_ctor(zstring(o, 0x1c));   // "New Name"
    vtable_and_defaults(o, vt(0x5f5864));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl CreateDefaultEntity_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd64), glob_addr(0x61474c) /* "CCreateDefaultEntityAction" */, 1.0f, CreateLayerPart_ClassInfo, 0x20, 0, fn_addr(0x4bc510), fn_addr(0x4bc560), fn_addr(0x4bc760));
}
// 0x4bc510
static void *__cdecl CreateDefaultEntity_Create() { return create_one<0x20, CreateDefaultEntity_construct>(); }
// 0x4bc560
static void *__fastcall CreateDefaultEntity_CreateArray(int count, int, int) { return create_array<0x20, CreateDefaultEntity_construct>(count); }
// 0x4bc760. eax: the last call's.
static DWORD __cdecl CreateDefaultEntity_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x614768) /* "Model to Create" */, 0x18, 4, 0x18, glob_addr(0x6cfdd8), 1, 0, 0, 0);
    add_field(CreateDefaultEntity_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x61456c) /* "New Name" */, 0x1c, 4, 0x1c, 1, 0, 0);
    return add_field(CreateDefaultEntity_ClassInfo(), f);
}

// ---- CCreateCustomEntityAction ------------------------------------------------------------------

// class 0x6bfd68, version 1, size 0x1c, parent CCreateLayerPartAction, vtable 0x5f58d8
static void CreateCustomEntity_construct(BYTE *o)
{
    CreateLayerPart_construct(o);
    vtable_and_defaults(o, vt(0x5f58d8));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl CreateCustomEntity_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd68), glob_addr(0x614778) /* "CCreateCustomEntityAction" */, 1.0f, CreateLayerPart_ClassInfo, 0x1c, 0, fn_addr(0x4bca00), fn_addr(0x4bca50), fn_addr(0x4bcc50));
}
// 0x4bca00
static void *__cdecl CreateCustomEntity_Create() { return create_one<0x1c, CreateCustomEntity_construct>(); }
// 0x4bca50
static void *__fastcall CreateCustomEntity_CreateArray(int count, int, int) { return create_array<0x1c, CreateCustomEntity_construct>(count); }
// 0x4bcc50. eax: the last call's.
static DWORD __cdecl CreateCustomEntity_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x614794) /* "Entity" */, 0x18, 4, 0x18, class_at(fn_addr(0x487620)) /* CEntityBase */, 1, 0, 0, 0);
    return add_field(CreateCustomEntity_ClassInfo(), f);
}

// ---- CLockDoorAction ----------------------------------------------------------------------------

// class 0x6bfd6c, version 1, size 0xc, parent CAction, vtable 0x5f594c
static void LockDoor_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Door Name"
    vtable_and_defaults(o, vt(0x5f594c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl LockDoor_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd6c), glob_addr(0x6147bc) /* "CLockDoorAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x4bd100), fn_addr(0x4bd140), fn_addr(0x4bd2b0));
}
// 0x4bd100
static void *__cdecl LockDoor_Create() { return create_one<0xc, LockDoor_construct>(); }
// 0x4bd140
static void *__fastcall LockDoor_CreateArray(int count, int, int) { return create_array<0xc, LockDoor_construct>(count); }
// 0x4bd2b0. eax: the last call's.
static DWORD __cdecl LockDoor_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6147cc) /* "Door Name" */, 8, 4, 8, 1, 0, 0);
    return add_field(LockDoor_ClassInfo(), f);
}

// ---- CUnlockDoorAction --------------------------------------------------------------------------

// class 0x6bfd70, version 1, size 0xc, parent CAction, vtable 0x5f59bc
static void UnlockDoor_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Door Name"
    vtable_and_defaults(o, vt(0x5f59bc));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl UnlockDoor_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd70), glob_addr(0x6147d8) /* "CUnlockDoorAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x4bd3c0), fn_addr(0x4bd400), fn_addr(0x4bd570));
}
// 0x4bd3c0
static void *__cdecl UnlockDoor_Create() { return create_one<0xc, UnlockDoor_construct>(); }
// 0x4bd400
static void *__fastcall UnlockDoor_CreateArray(int count, int, int) { return create_array<0xc, UnlockDoor_construct>(count); }
// 0x4bd570. eax: the last call's.
static DWORD __cdecl UnlockDoor_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6147cc) /* "Door Name" */, 8, 4, 8, 1, 0, 0);
    return add_field(UnlockDoor_ClassInfo(), f);
}

// ---- CXDoorAction -------------------------------------------------------------------------------

// class 0x6bfd74, version 2, size 0x10, abstract, parent CAction
static void XDoor_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Door Name"
    vtable_and_defaults(o, vt(0x5f5a9c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl XDoor_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd74), glob_addr(0x6147ec) /* "CXDoorAction" */, 2.0f, action_class, 0x10, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x4bd710));
}
// 0x4bd710. eax: the last call's.
static DWORD __cdecl XDoor_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6147cc) /* "Door Name" */, 8, 4, 8, 1, 0, 0);
    add_field(XDoor_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x6147fc) /* "Delay Between Doors" */, glob_addr(0x60b000) /* "Seconds" */, 0xc, 4, 0xc, 1, 0, 0, 0, 0x4cbebc20 /* 100000000.0 */, 0x3dcccccd /* 0.1 */);
    return add_field(XDoor_ClassInfo(), f);
}

// ---- COpenDoorAction ----------------------------------------------------------------------------

// class 0x6bfd78, version 1, size 0x10, parent CXDoorAction, vtable 0x5f5a2c
static void OpenDoor_construct(BYTE *o)
{
    XDoor_construct(o);
    vtable_and_defaults(o, vt(0x5f5a2c));
}
// (slot 2; the class registers no fields of its own)
static CClassInfo *__cdecl OpenDoor_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd78), glob_addr(0x614868) /* "COpenDoorAction" */, 1.0f, XDoor_ClassInfo, 0x10, 0, fn_addr(0x4bde10), fn_addr(0x4bde60), fn_addr(0x58ea40));
}
// 0x4bde10
static void *__cdecl OpenDoor_Create() { return create_one<0x10, OpenDoor_construct>(); }
// 0x4bde60
static void *__fastcall OpenDoor_CreateArray(int count, int, int) { return create_array<0x10, OpenDoor_construct>(count); }

// ---- CCloseDoorAction ---------------------------------------------------------------------------

// class 0x6bfd7c, version 1, size 0x10, parent CXDoorAction, vtable 0x5f5b0c
static void CloseDoor_construct(BYTE *o)
{
    XDoor_construct(o);
    vtable_and_defaults(o, vt(0x5f5b0c));
}
// (slot 2; the class registers no fields of its own)
static CClassInfo *__cdecl CloseDoor_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd7c), glob_addr(0x614878) /* "CCloseDoorAction" */, 1.0f, XDoor_ClassInfo, 0x10, 0, fn_addr(0x4be130), fn_addr(0x4be180), fn_addr(0x58ea40));
}
// 0x4be130
static void *__cdecl CloseDoor_Create() { return create_one<0x10, CloseDoor_construct>(); }
// 0x4be180
static void *__fastcall CloseDoor_CreateArray(int count, int, int) { return create_array<0x10, CloseDoor_construct>(count); }

// ---- CToggleDoorAction --------------------------------------------------------------------------

// class 0x6bfd80, version 1, size 0x10, parent CXDoorAction, vtable 0x5f5b7c
static void ToggleDoor_construct(BYTE *o)
{
    XDoor_construct(o);
    vtable_and_defaults(o, vt(0x5f5b7c));
}
// (slot 2; the class registers no fields of its own)
static CClassInfo *__cdecl ToggleDoor_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd80), glob_addr(0x61488c) /* "CToggleDoorAction" */, 1.0f, XDoor_ClassInfo, 0x10, 0, fn_addr(0x4be450), fn_addr(0x4be4a0), fn_addr(0x58ea40));
}
// 0x4be450
static void *__cdecl ToggleDoor_Create() { return create_one<0x10, ToggleDoor_construct>(); }
// 0x4be4a0
static void *__fastcall ToggleDoor_CreateArray(int count, int, int) { return create_array<0x10, ToggleDoor_construct>(count); }

// ---- CPlayAnimationAction -----------------------------------------------------------------------

// class 0x6bfd84, version 2, size 0x20, parent CAction, vtable 0x5f5c10
static void PlayAnimation_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Animation"
    zs_ctor(zstring(o, 0xc));   // "End Sequence Name"
    zs_ctor(zstring(o, 0x10));   // "Target Name"
    vtable_and_defaults(o, vt(0x5f5c10));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl PlayAnimation_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd84), glob_addr(0x6148a0) /* "CPlayAnimationAction" */, 2.0f, action_class, 0x20, 0, fn_addr(0x4be860), fn_addr(0x4be8b0), fn_addr(0x4bea50));
}
// 0x4be860
static void *__cdecl PlayAnimation_Create() { return create_one<0x20, PlayAnimation_construct>(); }
// 0x4be8b0
static void *__fastcall PlayAnimation_CreateArray(int count, int, int) { return create_array<0x20, PlayAnimation_construct>(count); }
// 0x4bea50. eax: the last call's.
static DWORD __cdecl PlayAnimation_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x614900) /* "Animation" */, 8, 4, 8, 1, 0, 0);
    add_field(PlayAnimation_ClassInfo(), f);
    f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6148f4) /* "Target Name" */, 0x10, 4, 0x10, 1, 0, 0);
    add_field(PlayAnimation_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x614494) /* "When Done" */, 0x14, 4, 0x14, glob_addr(0x6bfc80), 1, glob_addr(0x6141b4) /* "Stop on last frame" */, fn_addr(0x4bed80), 0);
    add_field(PlayAnimation_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x6148e0) /* "End Sequence Name" */, 0xc, 4, 0xc, 1, glob_addr(0x60f8d0) /* "Idle" */, 0);
    add_field(PlayAnimation_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x6148c8) /* "Delay Between Targets" */, glob_addr(0x60b000) /* "Seconds" */, 0x18, 4, 0x18, 1, 0, 0, 0, 0x4cbebc20 /* 100000000.0 */, 0x3dcccccd /* 0.1 */);
    add_field(PlayAnimation_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x6148b8) /* "Play In Reverse" */, 0x1c, 1, 0x1c, 1, 0, 0);
    return add_field(PlayAnimation_ClassInfo(), f);
}

// ---- CSetCurSequenceAction ----------------------------------------------------------------------

// class 0x6bfd88, version 1, size 0x10, parent CAction, vtable 0x5f5c80
static void SetCurSequence_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Sequence"
    zs_ctor(zstring(o, 0xc));   // "Target Name"
    vtable_and_defaults(o, vt(0x5f5c80));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl SetCurSequence_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd88), glob_addr(0x61490c) /* "CSetCurSequenceAction" */, 1.0f, action_class, 0x10, 0, fn_addr(0x4beed0), fn_addr(0x4bef10), fn_addr(0x4bf090));
}
// 0x4beed0
static void *__cdecl SetCurSequence_Create() { return create_one<0x10, SetCurSequence_construct>(); }
// 0x4bef10
static void *__fastcall SetCurSequence_CreateArray(int count, int, int) { return create_array<0x10, SetCurSequence_construct>(count); }
// 0x4bf090. eax: the last call's.
static DWORD __cdecl SetCurSequence_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x60c3d4) /* "Sequence" */, 8, 4, 8, 1, 0, 0);
    add_field(SetCurSequence_ClassInfo(), f);
    set_alt_name(last_field(SetCurSequence_ClassInfo()), glob_addr(0x614900) /* "Animation" */);
    f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6148f4) /* "Target Name" */, 0xc, 4, 0xc, 1, 0, 0);
    return add_field(SetCurSequence_ClassInfo(), f);
}

// ---- CCheckCurSequenceAction --------------------------------------------------------------------

// class 0x6bfd8c, version 1, size 0x10, parent CAction, vtable 0x5f5cf0
static void CheckCurSequence_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target Name"
    zs_ctor(zstring(o, 0xc));   // "Animation"
    vtable_and_defaults(o, vt(0x5f5cf0));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl CheckCurSequence_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd8c), glob_addr(0x614948) /* "CCheckCurSequenceAction" */, 1.0f, action_class, 0x10, 0, fn_addr(0x4bf320), fn_addr(0x4bf360), fn_addr(0x4bf4e0));
}
// 0x4bf320
static void *__cdecl CheckCurSequence_Create() { return create_one<0x10, CheckCurSequence_construct>(); }
// 0x4bf360
static void *__fastcall CheckCurSequence_CreateArray(int count, int, int) { return create_array<0x10, CheckCurSequence_construct>(count); }
// 0x4bf4e0. eax: the last call's.
static DWORD __cdecl CheckCurSequence_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6148f4) /* "Target Name" */, 8, 4, 8, 1, 0, 0);
    add_field(CheckCurSequence_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x614900) /* "Animation" */, 0xc, 4, 0xc, 1, 0, 0);
    return add_field(CheckCurSequence_ClassInfo(), f);
}

// ---- CIsDoorOpenAction --------------------------------------------------------------------------

// class 0x6bfd90, version 1, size 0xc, parent CAction, vtable 0x5f5d60
static void IsDoorOpen_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Door Name"
    vtable_and_defaults(o, vt(0x5f5d60));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl IsDoorOpen_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd90), glob_addr(0x614960) /* "CIsDoorOpenAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x4bf690), fn_addr(0x4bf6d0), fn_addr(0x4bf840));
}
// 0x4bf690
static void *__cdecl IsDoorOpen_Create() { return create_one<0xc, IsDoorOpen_construct>(); }
// 0x4bf6d0
static void *__fastcall IsDoorOpen_CreateArray(int count, int, int) { return create_array<0xc, IsDoorOpen_construct>(count); }
// 0x4bf840. eax: the last call's.
static DWORD __cdecl IsDoorOpen_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6147cc) /* "Door Name" */, 8, 4, 8, 1, 0, 0);
    return add_field(IsDoorOpen_ClassInfo(), f);
}

// ---- CIsDoorClosedAction ------------------------------------------------------------------------

// class 0x6bfd94, version 1, size 0xc, parent CAction, vtable 0x5f5dd0
static void IsDoorClosed_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Door Name"
    vtable_and_defaults(o, vt(0x5f5dd0));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl IsDoorClosed_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd94), glob_addr(0x614974) /* "CIsDoorClosedAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x4bf950), fn_addr(0x4bf990), fn_addr(0x4bfb00));
}
// 0x4bf950
static void *__cdecl IsDoorClosed_Create() { return create_one<0xc, IsDoorClosed_construct>(); }
// 0x4bf990
static void *__fastcall IsDoorClosed_CreateArray(int count, int, int) { return create_array<0xc, IsDoorClosed_construct>(count); }
// 0x4bfb00. eax: the last call's.
static DWORD __cdecl IsDoorClosed_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6147cc) /* "Door Name" */, 8, 4, 8, 1, 0, 0);
    return add_field(IsDoorClosed_ClassInfo(), f);
}

// ---- CIsDoorLockedAction ------------------------------------------------------------------------

// class 0x6bfd98, version 1, size 0xc, parent CAction, vtable 0x5f5e40
static void IsDoorLocked_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Door Name"
    vtable_and_defaults(o, vt(0x5f5e40));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl IsDoorLocked_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd98), glob_addr(0x614988) /* "CIsDoorLockedAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x4bfc10), fn_addr(0x4bfc50), fn_addr(0x4bfdc0));
}
// 0x4bfc10
static void *__cdecl IsDoorLocked_Create() { return create_one<0xc, IsDoorLocked_construct>(); }
// 0x4bfc50
static void *__fastcall IsDoorLocked_CreateArray(int count, int, int) { return create_array<0xc, IsDoorLocked_construct>(count); }
// 0x4bfdc0. eax: the last call's.
static DWORD __cdecl IsDoorLocked_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6147cc) /* "Door Name" */, 8, 4, 8, 1, 0, 0);
    return add_field(IsDoorLocked_ClassInfo(), f);
}

// ---- CPlaySoundAction ---------------------------------------------------------------------------

// class 0x6bfd9c, version 2, size 0x10, parent CAction, vtable 0x5f5eb0
static void PlaySound_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0xc));   // "Position"
    vtable_and_defaults(o, vt(0x5f5eb0));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl PlaySound_ClassInfo()
{
    return class_getter(glob_addr(0x6bfd9c), glob_addr(0x61499c) /* "CPlaySoundAction" */, 2.0f, action_class, 0x10, 0, fn_addr(0x4bfee0), fn_addr(0x4bff20), fn_addr(0x4c0090));
}
// 0x4bfee0
static void *__cdecl PlaySound_Create() { return create_one<0x10, PlaySound_construct>(); }
// 0x4bff20
static void *__fastcall PlaySound_CreateArray(int count, int, int) { return create_array<0x10, PlaySound_construct>(count); }
// 0x4c0090. eax: the last call's.
static DWORD __cdecl PlaySound_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60c2f8) /* "Sound" */, 8, 4, 8, glob_addr(0x713708), 1, 0, 0, 0);
    add_field(PlaySound_ClassInfo(), f);
    f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6149b0) /* "Position" */, 0xc, 4, 0xc, 1, glob_addr(0x6142e0) /* "$Trigger" */, 0);
    return add_field(PlaySound_ClassInfo(), f);
}

// ---- SyfaceTypeSoundItem ------------------------------------------------------------------------

// class 0x6bfda0, version 2, size 0x10, parent none (CObject), vtable 0x5f5f20: one entry of
// CPlaySoundBySurfaceTypeAction's "Sound/Specializations" (a surface type and its sound)
static void SyfaceTypeSoundItem_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f5f20));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl SyfaceTypeSoundItem_ClassInfo()
{
    return class_getter(glob_addr(0x6bfda0), glob_addr(0x6149bc) /* "SyfaceTypeSoundItem" */, 2.0f, nullptr, 0x10, 0, fn_addr(0x4c0200), fn_addr(0x4c0230), fn_addr(0x4c0370));
}
// 0x4c0200
static void *__cdecl SyfaceTypeSoundItem_Create() { return create_one<0x10, SyfaceTypeSoundItem_construct>(); }
// 0x4c0230
static void *__fastcall SyfaceTypeSoundItem_CreateArray(int count, int, int) { return create_array<0x10, SyfaceTypeSoundItem_construct>(count); }
// 0x4c0370. eax: the last call's.
static DWORD __cdecl SyfaceTypeSoundItem_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x6149d0) /* "Surface Type" */, 8, 4, 8, glob_addr(0x713868), 1, glob_addr(0x614170) /* "Generic" */, 0, 0);
    add_field(SyfaceTypeSoundItem_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60c2f8) /* "Sound" */, 0xc, 4, 0xc, glob_addr(0x713708), 1, 0, 0, 0);
    return add_field(SyfaceTypeSoundItem_ClassInfo(), f);
}

// ---- CPositionedAction --------------------------------------------------------------------------

// class 0x6bdeb0, version 2, size 0x14, abstract, parent CAction; the class getter is CPositionedAction_ClassInfo (not in this module)
static void Positioned_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(0x5f5ff0));
}

// ---- CPlaySoundBySurfaceTypeAction --------------------------------------------------------------

// class 0x6bfda4, version 2, size 0x24, parent CPositionedAction, vtable 0x5f5f7c
static void PlaySoundBySurfaceType_construct(BYTE *o)
{
    Positioned_construct(o);
    zs_ctor(zstring(o, 0x18));   // "Position Relative To"
    vtable_and_defaults(o, vt(0x5f5f7c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl PlaySoundBySurfaceType_ClassInfo()
{
    return class_getter(glob_addr(0x6bfda4), glob_addr(0x6149e0) /* "CPlaySoundBySurfaceTypeAction" */, 2.0f, getter<0x49c280>, 0x24, 0, fn_addr(0x4c0500), fn_addr(0x4c0550), fn_addr(0x4c06e0));
}
// 0x4c0500
static void *__cdecl PlaySoundBySurfaceType_Create() { return create_one<0x24, PlaySoundBySurfaceType_construct>(); }
// 0x4c0550
static void *__fastcall PlaySoundBySurfaceType_CreateArray(int count, int, int) { return create_array<0x24, PlaySoundBySurfaceType_construct>(count); }
// 0x4c06e0. eax: the last call's.
static DWORD __cdecl PlaySoundBySurfaceType_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x614a38) /* "Sound/Default Sound" */, 0x14, 4, 0x14, glob_addr(0x713708), 1, 0, 0, 0);
    add_field(PlaySoundBySurfaceType_ClassInfo(), f);
    f = new_field(0x44, fn_addr(LIST_FIELD_CTOR), glob_addr(0x614a20) /* "Sound/Specializations" */, 0x20, 4, 0x20, 0x1c, 4, 0x1c, SyfaceTypeSoundItem_ClassInfo(), 1, 0);
    add_field(PlaySoundBySurfaceType_ClassInfo(), f);
    f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x614a00) /* "Position/Position Relative To" */, 0x18, 4, 0x18, 1, glob_addr(0x6142e0) /* "$Trigger" */, 0);
    add_field(PlaySoundBySurfaceType_ClassInfo(), f);
    return set_alt_name(last_field(PlaySoundBySurfaceType_ClassInfo()), glob_addr(0x6149b0) /* "Position" */);
}

// ---- CPlayVOAction ------------------------------------------------------------------------------

// class 0x6bfda8, version 2, size 0xc, parent CAction, vtable 0x5f6060
static void PlayVO_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(0x5f6060));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl PlayVO_ClassInfo()
{
    return class_getter(glob_addr(0x6bfda8), glob_addr(0x614a4c) /* "CPlayVOAction" */, 2.0f, action_class, 0xc, 0, fn_addr(0x4c0aa0), fn_addr(0x4c0ad0), fn_addr(0x4c0c10));
}
// 0x4c0aa0
static void *__cdecl PlayVO_Create() { return create_one<0xc, PlayVO_construct>(); }
// 0x4c0ad0
static void *__fastcall PlayVO_CreateArray(int count, int, int) { return create_array<0xc, PlayVO_construct>(count); }
// 0x4c0c10. eax: the last call's.
static DWORD __cdecl PlayVO_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x614a5c) /* "VO" */, 8, 4, 8, glob_addr(0x6c2480), 1, 0, 0, 0);
    return add_field(PlayVO_ClassInfo(), f);
}

// ---- CSwitchMapActionBase -----------------------------------------------------------------------

// class 0x6bfdac, version 2, size 0x1c, abstract, parent CAction
// 0x4c0fa0: the constructor (called by the Create of CSwitchMapAction, CRelocateAction and
// CTeleportAction); +8 and +0xc zeroed after Object_InitFields. eax: this.
static void *THISCALL SwitchMapActionBase_Ctor(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x10));   // "New Map Name"
    zs_ctor(zstring(o, 0x14));   // "New Location"
    zs_ctor(zstring(o, 0x18));   // "Who To Switch"
    vtable_and_defaults(o, vt(0x5f60d0));
    dw(o, 8) = 0;
    dw(o, 0xc) = 0;
    return o;
}
// 0x4c0cf0
static CClassInfo *__cdecl SwitchMapActionBase_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdac), glob_addr(0x614a60) /* "CSwitchMapActionBase" */, 2.0f, action_class, 0x1c, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x4c0da0));
}
// 0x4c0da0. eax: the last call's.
static DWORD __cdecl SwitchMapActionBase_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x614a94) /* "New Map Name" */, 0x10, 4, 0x10, 1, 0, 0);
    add_field(SwitchMapActionBase_ClassInfo(), f);
    f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x614548) /* "New Location" */, 0x14, 4, 0x14, 1, 0, 0);
    add_field(SwitchMapActionBase_ClassInfo(), f);
    set_alt_name(last_field(SwitchMapActionBase_ClassInfo()), glob_addr(0x614a88) /* "Spawn Point" */);
    f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x614a78) /* "Who To Switch" */, 0x18, 4, 0x18, 1, glob_addr(0x6142cc) /* "$Instigator" */, 0);
    return add_field(SwitchMapActionBase_ClassInfo(), f);
}

// ---- CShowAction --------------------------------------------------------------------------------

// class 0x6bfdb0, version 1, size 0xc, parent CAction, vtable 0x5f6144
static void Show_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target Name"
    vtable_and_defaults(o, vt(0x5f6144));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Show_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdb0), glob_addr(0x614b40) /* "CShowAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x4c1420), fn_addr(0x4c1460), fn_addr(0x4c15d0));
}
// 0x4c1420
static void *__cdecl Show_Create() { return create_one<0xc, Show_construct>(); }
// 0x4c1460
static void *__fastcall Show_CreateArray(int count, int, int) { return create_array<0xc, Show_construct>(count); }
// 0x4c15d0. eax: the last call's.
static DWORD __cdecl Show_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6148f4) /* "Target Name" */, 8, 4, 8, 1, 0, 0);
    return add_field(Show_ClassInfo(), f);
}

// ---- CHideAction --------------------------------------------------------------------------------

// class 0x6bfdb4, version 1, size 0xc, parent CAction, vtable 0x5f61b4
static void Hide_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target Name"
    vtable_and_defaults(o, vt(0x5f61b4));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Hide_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdb4), glob_addr(0x614b4c) /* "CHideAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x4c16e0), fn_addr(0x4c1720), fn_addr(0x4c1890));
}
// 0x4c16e0
static void *__cdecl Hide_Create() { return create_one<0xc, Hide_construct>(); }
// 0x4c1720
static void *__fastcall Hide_CreateArray(int count, int, int) { return create_array<0xc, Hide_construct>(count); }
// 0x4c1890. eax: the last call's.
static DWORD __cdecl Hide_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6148f4) /* "Target Name" */, 8, 4, 8, 1, 0, 0);
    return add_field(Hide_ClassInfo(), f);
}

// ---- CSetCollidableAction -----------------------------------------------------------------------

// class 0x6bfdb8, version 1, size 0x10, parent CAction, vtable 0x5f6224
static void SetCollidable_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target Name"
    vtable_and_defaults(o, vt(0x5f6224));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl SetCollidable_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdb8), glob_addr(0x614b58) /* "CSetCollidableAction" */, 1.0f, action_class, 0x10, 0, fn_addr(0x4c19a0), fn_addr(0x4c19e0), fn_addr(0x4c1b50));
}
// 0x4c19a0
static void *__cdecl SetCollidable_Create() { return create_one<0x10, SetCollidable_construct>(); }
// 0x4c19e0
static void *__fastcall SetCollidable_CreateArray(int count, int, int) { return create_array<0x10, SetCollidable_construct>(count); }
// 0x4c1b50. eax: the last call's.
static DWORD __cdecl SetCollidable_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6148f4) /* "Target Name" */, 8, 4, 8, 1, 0, 0);
    add_field(SetCollidable_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x614b70) /* "Collidable" */, 0xc, 1, 0xc, 1, 1, 0);
    return add_field(SetCollidable_ClassInfo(), f);
}

// ---- CSetModelAction ----------------------------------------------------------------------------

// class 0x6bfdbc, version 1, size 0x14, parent CAction, vtable 0x5f6294
static void SetModel_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target Name"
    vtable_and_defaults(o, vt(0x5f6294));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl SetModel_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdbc), glob_addr(0x614b7c) /* "CSetModelAction" */, 1.0f, action_class, 0x14, 0, fn_addr(0x4c1cf0), fn_addr(0x4c1d30), fn_addr(0x4c1ea0));
}
// 0x4c1cf0
static void *__cdecl SetModel_Create() { return create_one<0x14, SetModel_construct>(); }
// 0x4c1d30
static void *__fastcall SetModel_CreateArray(int count, int, int) { return create_array<0x14, SetModel_construct>(count); }
// 0x4c1ea0. eax: the last call's.
static DWORD __cdecl SetModel_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6148f4) /* "Target Name" */, 8, 4, 8, 1, 0, 0);
    add_field(SetModel_ClassInfo(), f);
    set_help(last_field(SetModel_ClassInfo()), glob_addr(0x614bd8) /* "The name of the layer part who's model will be changed" */);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x614bcc) /* "New Model" */, 0xc, 4, 0xc, glob_addr(0x6cfdd8), 2, 0, 0, 0);
    add_field(SetModel_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x614bbc) /* "New Child Model" */, 0x10, 4, 0x10, glob_addr(0x6cfdd8), 2, 0, 0, 0);
    add_field(SetModel_ClassInfo(), f);
    return set_help(last_field(SetModel_ClassInfo()), glob_addr(0x614b8c) /* "The model to use for the targets child/torso" */);
}

// ---- CSetCategoryAction -------------------------------------------------------------------------

// class 0x6bfdc0, version 1, size 0x10, parent CAction, vtable 0x5f6304
static void SetCategory_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target Name"
    vtable_and_defaults(o, vt(0x5f6304));
    dw(o, 0xc) = 0;   // "New Category"
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl SetCategory_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdc0), glob_addr(0x614c10) /* "CSetCategoryAction" */, 1.0f, action_class, 0x10, 0, fn_addr(0x4c2230), fn_addr(0x4c2270), fn_addr(0x4c23e0));
}
// 0x4c2230
static void *__cdecl SetCategory_Create() { return create_one<0x10, SetCategory_construct>(); }
// 0x4c2270
static void *__fastcall SetCategory_CreateArray(int count, int, int) { return create_array<0x10, SetCategory_construct>(count); }
// 0x4c23e0. eax: the last call's.
static DWORD __cdecl SetCategory_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6148f4) /* "Target Name" */, 8, 4, 8, 1, 0, 0);
    add_field(SetCategory_ClassInfo(), f);
    set_help(last_field(SetCategory_ClassInfo()), glob_addr(0x614c34) /* "The name of the layer part who's catagory will be changed" */);
    f = new_field(0x38, fn_addr(FLAGS_FIELD_CTOR), glob_addr(0x614c24) /* "New Category" */, 0xc, 4, 0xc, glob_addr(0x6bd140), 2, 0, 0);
    return add_field(SetCategory_ClassInfo(), f);
}

// ---- CActivateAction ----------------------------------------------------------------------------

// class 0x6bfdc4, version 3, size 0x18, parent CAction, vtable 0x5f6374
static void Activate_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target Name"
    vtable_and_defaults(o, vt(0x5f6374));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Activate_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdc4), glob_addr(0x614c70) /* "CActivateAction" */, 3.0f, action_class, 0x18, 0, fn_addr(0x4c25e0), fn_addr(0x4c2620), fn_addr(0x4c2790));
}
// 0x4c25e0
static void *__cdecl Activate_Create() { return create_one<0x18, Activate_construct>(); }
// 0x4c2620
static void *__fastcall Activate_CreateArray(int count, int, int) { return create_array<0x18, Activate_construct>(count); }
// 0x4c2790. eax: the last call's.
static DWORD __cdecl Activate_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6148f4) /* "Target Name" */, 8, 4, 8, 1, 0, 0);
    add_field(Activate_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x614c9c) /* "Play Activate Sound" */, 0xc, 1, 0xc, 1, 0, 0);
    add_field(Activate_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x614c80) /* "Delay Between Activations" */, glob_addr(0x60b000) /* "Seconds" */, 0x10, 4, 0x10, 1, 0, 0, 0, 0x4cbebc20 /* 100000000.0 */, 0x3dcccccd /* 0.1 */);
    add_field(Activate_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x614584) /* "Warp Behavior" */, 0x14, 4, 0x14, class_at(fn_addr(0x55e350)) /* CRenderEffectBehavior */, 1, 0, 0, 0);
    return add_field(Activate_ClassInfo(), f);
}

// ---- CDeactivateAction --------------------------------------------------------------------------

// class 0x6bfdc8, version 1, size 0xc, parent CAction, vtable 0x5f63e4
static void Deactivate_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target Name"
    vtable_and_defaults(o, vt(0x5f63e4));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Deactivate_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdc8), glob_addr(0x614cb0) /* "CDeactivateAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x4c2b20), fn_addr(0x4c2b60), fn_addr(0x4c2cd0));
}
// 0x4c2b20
static void *__cdecl Deactivate_Create() { return create_one<0xc, Deactivate_construct>(); }
// 0x4c2b60
static void *__fastcall Deactivate_CreateArray(int count, int, int) { return create_array<0xc, Deactivate_construct>(count); }
// 0x4c2cd0. eax: the last call's.
static DWORD __cdecl Deactivate_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6148f4) /* "Target Name" */, 8, 4, 8, 1, 0, 0);
    return add_field(Deactivate_ClassInfo(), f);
}

// ---- CDeleteAction ------------------------------------------------------------------------------

// class 0x6bfdcc, version 1, size 0xc, parent CAction, vtable 0x5f6454
static void Delete_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target Name"
    vtable_and_defaults(o, vt(0x5f6454));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Delete_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdcc), glob_addr(0x614cc4) /* "CDeleteAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x4c2de0), fn_addr(0x4c2e20), fn_addr(0x4c2f90));
}
// 0x4c2de0
static void *__cdecl Delete_Create() { return create_one<0xc, Delete_construct>(); }
// 0x4c2e20
static void *__fastcall Delete_CreateArray(int count, int, int) { return create_array<0xc, Delete_construct>(count); }
// 0x4c2f90. eax: the last call's.
static DWORD __cdecl Delete_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6148f4) /* "Target Name" */, 8, 4, 8, 1, 0, 0);
    return add_field(Delete_ClassInfo(), f);
}

// ---- CSpawnEffectAction -------------------------------------------------------------------------

// class 0x6bfdd0, version 3, size 0x24, parent CAction, vtable 0x5f64c4
static void SpawnEffect_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Location Entity"
    vtable_and_defaults(o, vt(0x5f64c4));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl SpawnEffect_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdd0), glob_addr(0x614cd4) /* "CSpawnEffectAction" */, 3.0f, action_class, 0x24, 0, fn_addr(0x4c30d0), fn_addr(0x4c3110), fn_addr(0x4c3280));
}
// 0x4c30d0
static void *__cdecl SpawnEffect_Create() { return create_one<0x24, SpawnEffect_construct>(); }
// 0x4c3110
static void *__fastcall SpawnEffect_CreateArray(int count, int, int) { return create_array<0x24, SpawnEffect_construct>(count); }
// 0x4c3280. eax: the last call's.
static DWORD __cdecl SpawnEffect_RegisterFields()
{
    SpawnEffect_ClassInfo()->alt_name = glob<const char *>(0x614d64);   // "CCreateExplosionAction", the old class name
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x614d54) /* "Explosion Model" */, 0xc, 4, 0xc, glob_addr(0x6cfdd8), 1, 0, 0, 0);
    add_field(SpawnEffect_ClassInfo(), f);
    f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x614d44) /* "Location Entity" */, 8, 4, 8, 1, 0, 0);
    add_field(SpawnEffect_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x614d38) /* "X Offset" */, glob_addr(0x60b360) /* "Pixels" */, 0x10, 4, 0x10, 1, 0, 0, 0, 0, 0x3f800000 /* 1.0 */);
    add_field(SpawnEffect_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x614d2c) /* "Y Offset" */, glob_addr(0x60b360) /* "Pixels" */, 0x14, 4, 0x14, 1, 0, 0, 0, 0, 0x3f800000 /* 1.0 */);
    add_field(SpawnEffect_ClassInfo(), f);
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR_UNITS), glob_addr(0x614d1c) /* "Height Offset" */, glob_addr(0x60b360) /* "Pixels" */, 0x18, 4, 0x18, 1, 0, 0, 0, 0);
    add_field(SpawnEffect_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x614d08) /* "Play Default Sound" */, 0x1c, 1, 0x1c, 1, 1, 0);
    add_field(SpawnEffect_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x614ce8) /* "Action to execute After Effect" */, 0x20, 4, 0x20, action_class(), 1, 0, 0, 0);
    return add_field(SpawnEffect_ClassInfo(), f);
}

// ---- CPainAction --------------------------------------------------------------------------------

// class 0x6bfdd4, version 3, size 0x18, parent CAction, vtable 0x5f0b24
static void Pain_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x10));   // "Target"
    vtable_and_defaults(o, vt(0x5f0b24));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Pain_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdd4), glob_addr(0x614d7c) /* "CPainAction" */, 3.0f, action_class, 0x18, 0, fn_addr(0x4c3840), fn_addr(0x4c3880), fn_addr(0x4c39f0));
}
// 0x4c3c20: the constructor with values (callers outside this module): "Hit Points" (float bits),
// "Target" (copied), "Damage Type"; "Sound" cleared. eax: this.
static void *THISCALL Pain_CtorValues(BYTE *o, DWORD hit_points, const ZString *target, DWORD damage_type)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x10));
    vtable_and_defaults(o, vt(0x5f0b24));
    dw(o, 0xc) = 0;
    dw(o, 8) = hit_points;
    zs_assign(zstring(o, 0x10), target);
    dw(o, 0x14) = damage_type;
    return o;
}
// 0x4c3840
static void *__cdecl Pain_Create() { return create_one<0x18, Pain_construct>(); }
// 0x4c3880
static void *__fastcall Pain_CreateArray(int count, int, int) { return create_array<0x18, Pain_construct>(count); }
// 0x4c39f0. eax: the last call's.
static DWORD __cdecl Pain_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x612578) /* "Hit Points" */, 8, 4, 8, 1, 0x3f800000 /* 1.0 */, 0, 0, 0x47c34f80 /* 99999.0 */, 0x3f800000 /* 1.0 */);
    add_field(Pain_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x610318) /* "Damage Type" */, 0x14, 4, 0x14, glob_addr(0x6bd760), 1, glob_addr(0x610324) /* "Unavoidable" */, 0, 0);
    add_field(Pain_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60c2f8) /* "Sound" */, 0xc, 4, 0xc, glob_addr(0x713708), 1, 0, 0, 0);
    add_field(Pain_ClassInfo(), f);
    f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x614d88) /* "Target" */, 0x10, 4, 0x10, 1, glob_addr(0x6142cc) /* "$Instigator" */, 0);
    return add_field(Pain_ClassInfo(), f);
}

// ---- CDestroyAction -----------------------------------------------------------------------------

// class 0x6bfdd8, version 2, size 0xc, parent CAction, vtable 0x5f6534
static void Destroy_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target"
    vtable_and_defaults(o, vt(0x5f6534));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl Destroy_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdd8), glob_addr(0x614d90) /* "CDestroyAction" */, 2.0f, action_class, 0xc, 0, fn_addr(0x4c3d40), fn_addr(0x4c3d80), fn_addr(0x4c3ef0));
}
// 0x4c3d40
static void *__cdecl Destroy_Create() { return create_one<0xc, Destroy_construct>(); }
// 0x4c3d80
static void *__fastcall Destroy_CreateArray(int count, int, int) { return create_array<0xc, Destroy_construct>(count); }
// 0x4c3ef0. eax: the last call's.
static DWORD __cdecl Destroy_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x614d88) /* "Target" */, 8, 4, 8, 1, 0, 0);
    return add_field(Destroy_ClassInfo(), f);
}

// ---- CFlipSwitchAction --------------------------------------------------------------------------

// class 0x6bfddc, version 1, size 0xc, parent CAction, vtable 0x5f65a4
static void FlipSwitch_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Switch Name"
    vtable_and_defaults(o, vt(0x5f65a4));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl FlipSwitch_ClassInfo()
{
    return class_getter(glob_addr(0x6bfddc), glob_addr(0x614df8) /* "CFlipSwitchAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x4c4060), fn_addr(0x4c40a0), fn_addr(0x4c4210));
}
// 0x4c4060
static void *__cdecl FlipSwitch_Create() { return create_one<0xc, FlipSwitch_construct>(); }
// 0x4c40a0
static void *__fastcall FlipSwitch_CreateArray(int count, int, int) { return create_array<0xc, FlipSwitch_construct>(count); }
// 0x4c4210. eax: the last call's.
static DWORD __cdecl FlipSwitch_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x614e0c) /* "Switch Name" */, 8, 4, 8, 1, 0, 0);
    return add_field(FlipSwitch_ClassInfo(), f);
}

// ---- CAddAIAction -------------------------------------------------------------------------------

// class 0x6bfde0, version 2, size 0x14, parent CAction, vtable 0x5f6614
static void AddAI_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target Name"
    vtable_and_defaults(o, vt(0x5f6614));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl AddAI_ClassInfo()
{
    return class_getter(glob_addr(0x6bfde0), glob_addr(0x614e18) /* "CAddAIAction" */, 2.0f, action_class, 0x14, 0, fn_addr(0x4c4330), fn_addr(0x4c4370), fn_addr(0x4c44e0));
}
// 0x4c4330
static void *__cdecl AddAI_Create() { return create_one<0x14, AddAI_construct>(); }
// 0x4c4370
static void *__fastcall AddAI_CreateArray(int count, int, int) { return create_array<0x14, AddAI_construct>(count); }
// 0x4c44e0. eax: the last call's.
static DWORD __cdecl AddAI_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6148f4) /* "Target Name" */, 8, 4, 8, 1, 0, 0);
    add_field(AddAI_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x614e3c) /* "AI" */, 0xc, 4, 0xc, class_at(fn_addr(0x4fd220)) /* CLayerPartAI */, 1, 0, 0, 0);
    add_field(AddAI_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x614e28) /* "Delay Between Adds" */, glob_addr(0x60b000) /* "Seconds" */, 0x10, 4, 0x10, 1, 0, 0, 0, 0x4cbebc20 /* 100000000.0 */, 0x3dcccccd /* 0.1 */);
    return add_field(AddAI_ClassInfo(), f);
}

// ---- CRemoveAIAction ----------------------------------------------------------------------------

// class 0x6bfde4, version 1, size 0x14, parent CAction, vtable 0x5f6684
static void RemoveAI_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target Name"
    vtable_and_defaults(o, vt(0x5f6684));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl RemoveAI_ClassInfo()
{
    return class_getter(glob_addr(0x6bfde4), glob_addr(0x614e40) /* "CRemoveAIAction" */, 1.0f, action_class, 0x14, 0, fn_addr(0x4c4710), fn_addr(0x4c4750), fn_addr(0x4c48c0));
}
// 0x4c4710
static void *__cdecl RemoveAI_Create() { return create_one<0x14, RemoveAI_construct>(); }
// 0x4c4750
static void *__fastcall RemoveAI_CreateArray(int count, int, int) { return create_array<0x14, RemoveAI_construct>(count); }
// 0x4c48c0. eax: the last call's.
static DWORD __cdecl RemoveAI_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6148f4) /* "Target Name" */, 8, 4, 8, 1, 0, 0);
    add_field(RemoveAI_ClassInfo(), f);
    f = new_field(0x34, fn_addr(AI_FIELD_CTOR), glob_addr(0x614e3c) /* "AI" */, 0xc, 4, 0xc, class_at(fn_addr(0x4fd220)) /* CLayerPartAI */, 1, 0);
    add_field(RemoveAI_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x614e50) /* "Delay Between Removes" */, glob_addr(0x60b000) /* "Seconds" */, 0x10, 4, 0x10, 1, 0, 0, 0, 0x4cbebc20 /* 100000000.0 */, 0x3dcccccd /* 0.1 */);
    return add_field(RemoveAI_ClassInfo(), f);
}

// ---- CDischargeWeaponThisFrameBaseAction --------------------------------------------------------

// class 0x6bfde8, version 4, size 0x20, parent CPositionedAction, vtable 0x5f66f4
static void DischargeWeaponThisFrameBase_construct(BYTE *o)
{
    Positioned_construct(o);
    vtable_and_defaults(o, vt(0x5f66f4));
}
// 0x4c4aa0
static CClassInfo *__cdecl DischargeWeaponThisFrameBase_ClassInfo()
{
    return class_getter(glob_addr(0x6bfde8), glob_addr(0x614e68) /* "CDischargeWeaponThisFrameBaseAction" */, 4.0f, getter<0x49c280>, 0x20, 0, fn_addr(0x4c4b00), fn_addr(0x4c4b40), fn_addr(0x4c4ca0));
}
// 0x4c4b00
static void *__cdecl DischargeWeaponThisFrameBase_Create() { return create_one<0x20, DischargeWeaponThisFrameBase_construct>(); }
// 0x4c4b40
static void *__fastcall DischargeWeaponThisFrameBase_CreateArray(int count, int, int) { return create_array<0x20, DischargeWeaponThisFrameBase_construct>(count); }
// 0x4c4ca0. eax: the last call's.
static DWORD __cdecl DischargeWeaponThisFrameBase_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x614ea0) /* "Position/Fire Up Angle" */, glob_addr(0x614eb8) /* "Rad" */, 0x18, 4, 0x18, 1, 0, 0, 0, 0, 0);
    add_field(DischargeWeaponThisFrameBase_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);   // the unit converter 0x6be328 (the value shown in degrees)
    zs_assign_text(zstring(f, 0x20), glob_addr(0x60ff80) /* "Degrees" */);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x614e8c) /* "Use This Pos To Aim" */, 0x1c, 1, 0x1c, 1, 1, 0);
    return add_field(DischargeWeaponThisFrameBase_ClassInfo(), f);
}

// ---- CChangeSoundEnvironmentTypeAction ----------------------------------------------------------

// class 0x6bfdec, version 1, size 0xc, parent CAction, vtable 0x5f676c
static void ChangeSoundEnvironmentType_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(0x5f676c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl ChangeSoundEnvironmentType_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdec), glob_addr(0x614fcc) /* "CChangeSoundEnvironmentTypeAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x4c5530), fn_addr(0x4c5560), fn_addr(0x4c56a0));
}
// 0x4c5530
static void *__cdecl ChangeSoundEnvironmentType_Create() { return create_one<0xc, ChangeSoundEnvironmentType_construct>(); }
// 0x4c5560
static void *__fastcall ChangeSoundEnvironmentType_CreateArray(int count, int, int) { return create_array<0xc, ChangeSoundEnvironmentType_construct>(count); }
// 0x4c56a0. eax: the last call's.
static DWORD __cdecl ChangeSoundEnvironmentType_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x614ff0) /* "Environment Type" */, 8, 4, 8, glob_addr(0x6bfce8), 1, glob_addr(0x614170) /* "Generic" */, 0, 0);
    return add_field(ChangeSoundEnvironmentType_ClassInfo(), f);
}

// ---- CCheckExistenceAction ----------------------------------------------------------------------

// class 0x6bfdf0, version 1, size 0x14, parent CAction, vtable 0x5f67dc
static void CheckExistence_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Name To Check For"
    vtable_and_defaults(o, vt(0x5f67dc));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl CheckExistence_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdf0), glob_addr(0x615004) /* "CCheckExistenceAction" */, 1.0f, action_class, 0x14, 0, fn_addr(0x4c57a0), fn_addr(0x4c57e0), fn_addr(0x4c5950));
}
// 0x4c57a0
static void *__cdecl CheckExistence_Create() { return create_one<0x14, CheckExistence_construct>(); }
// 0x4c57e0
static void *__fastcall CheckExistence_CreateArray(int count, int, int) { return create_array<0x14, CheckExistence_construct>(count); }
// 0x4c5950. eax: the last call's.
static DWORD __cdecl CheckExistence_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x615034) /* "Name To Check For" */, 8, 4, 8, 1, 0, 0);
    add_field(CheckExistence_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x61501c) /* "Desired Minimum Count" */, 0xc, 4, 0xc, 1, 1, 0, 1, 0xf423f);
    return add_field(CheckExistence_ClassInfo(), f);
}

// ---- CIsAliveAction -----------------------------------------------------------------------------

// class 0x6bfdf4, version 1, size 0x14, parent CAction, vtable 0x5f684c
static void IsAlive_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Name To Check For"
    vtable_and_defaults(o, vt(0x5f684c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl IsAlive_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdf4), glob_addr(0x615048) /* "CIsAliveAction" */, 1.0f, action_class, 0x14, 0, fn_addr(0x4c5ae0), fn_addr(0x4c5b20), fn_addr(0x4c5c90));
}
// 0x4c5ae0
static void *__cdecl IsAlive_Create() { return create_one<0x14, IsAlive_construct>(); }
// 0x4c5b20
static void *__fastcall IsAlive_CreateArray(int count, int, int) { return create_array<0x14, IsAlive_construct>(count); }
// 0x4c5c90. eax: the last call's.
static DWORD __cdecl IsAlive_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x615034) /* "Name To Check For" */, 8, 4, 8, 1, 0, 0);
    add_field(IsAlive_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x61501c) /* "Desired Minimum Count" */, 0xc, 4, 0xc, 1, 1, 0, 1, 0xf423f);
    return add_field(IsAlive_ClassInfo(), f);
}

// ---- CHealthIsAtLeast ---------------------------------------------------------------------------

// class 0x6bfdf8, version 1, size 0x18, parent CAction, vtable 0x5f68bc
static void HealthIsAtLeast_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Name To Check For"
    vtable_and_defaults(o, vt(0x5f68bc));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl HealthIsAtLeast_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdf8), glob_addr(0x615058) /* "CHealthIsAtLeast" */, 1.0f, action_class, 0x18, 0, fn_addr(0x4c5e70), fn_addr(0x4c5eb0), fn_addr(0x4c6020));
}
// 0x4c5e70
static void *__cdecl HealthIsAtLeast_Create() { return create_one<0x18, HealthIsAtLeast_construct>(); }
// 0x4c5eb0
static void *__fastcall HealthIsAtLeast_CreateArray(int count, int, int) { return create_array<0x18, HealthIsAtLeast_construct>(count); }
// 0x4c6020. eax: the last call's.
static DWORD __cdecl HealthIsAtLeast_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x615034) /* "Name To Check For" */, 8, 4, 8, 1, 0, 0);
    add_field(HealthIsAtLeast_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61506c) /* "Minimum Health" */, 0xc, 4, 0xc, 1, 0x42c80000 /* 100.0 */, 0, 0, 0x497423fe /* 999999.9 */, 0);
    add_field(HealthIsAtLeast_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x61501c) /* "Desired Minimum Count" */, 0x10, 4, 0x10, 1, 1, 0, 1, 0xf423f);
    return add_field(HealthIsAtLeast_ClassInfo(), f);
}

// ---- CSendMessageAction -------------------------------------------------------------------------

// class 0x6bfdfc, version 1, size 0x10, parent CAction, vtable 0x5f692c
static void SendMessage_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target"
    zs_ctor(zstring(o, 0xc));   // "Message"
    vtable_and_defaults(o, vt(0x5f692c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl SendMessage_ClassInfo()
{
    return class_getter(glob_addr(0x6bfdfc), glob_addr(0x61507c) /* "CSendMessageAction" */, 1.0f, action_class, 0x10, 0, fn_addr(0x4c6280), fn_addr(0x4c62c0), fn_addr(0x4c6440));
}
// 0x4c6280
static void *__cdecl SendMessage_Create() { return create_one<0x10, SendMessage_construct>(); }
// 0x4c62c0
static void *__fastcall SendMessage_CreateArray(int count, int, int) { return create_array<0x10, SendMessage_construct>(count); }
// 0x4c6440. eax: the last call's.
static DWORD __cdecl SendMessage_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x615090) /* "Message" */, 0xc, 4, 0xc, 1, glob_addr(TEXT_EMPTY), 0);
    add_field(SendMessage_ClassInfo(), f);
    f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x614d88) /* "Target" */, 8, 4, 8, 1, glob_addr(0x6142e0) /* "$Trigger" */, 0);
    return add_field(SendMessage_ClassInfo(), f);
}

// ---- CBroadcastMessageToAreaAction --------------------------------------------------------------

// class 0x6bfe00, version 1, size 0x14, parent CAction, vtable 0x5f699c
static void BroadcastMessageToArea_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Message"
    zs_ctor(zstring(o, 0xc));   // "Position"
    vtable_and_defaults(o, vt(0x5f699c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl BroadcastMessageToArea_ClassInfo()
{
    return class_getter(glob_addr(0x6bfe00), glob_addr(0x615098) /* "CBroadcastMessageToAreaAction" */, 1.0f, action_class, 0x14, 0, fn_addr(0x4c6640), fn_addr(0x4c6680), fn_addr(0x4c6800));
}
// 0x4c6640
static void *__cdecl BroadcastMessageToArea_Create() { return create_one<0x14, BroadcastMessageToArea_construct>(); }
// 0x4c6680
static void *__fastcall BroadcastMessageToArea_CreateArray(int count, int, int) { return create_array<0x14, BroadcastMessageToArea_construct>(count); }
// 0x4c6800. eax: the last call's.
static DWORD __cdecl BroadcastMessageToArea_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x615090) /* "Message" */, 8, 4, 8, 1, glob_addr(TEXT_EMPTY), 0);
    add_field(BroadcastMessageToArea_ClassInfo(), f);
    f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6149b0) /* "Position" */, 0xc, 4, 0xc, 1, glob_addr(0x6142e0) /* "$Trigger" */, 0);
    add_field(BroadcastMessageToArea_ClassInfo(), f);
    set_help(last_field(BroadcastMessageToArea_ClassInfo()), glob_addr(0x615134) /* "A message will be sent to everything is an area centered around thi..." */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x60b34c) /* "Radius" */, glob_addr(0x60b360) /* "Pixels" */, 0x10, 4, 0x10, 1, 0x42c80000 /* 100.0 */, 0, 0, 0x43960000 /* 300.0 */, 0x3f800000 /* 1.0 */);
    add_field(BroadcastMessageToArea_ClassInfo(), f);
    return set_help(last_field(BroadcastMessageToArea_ClassInfo()), glob_addr(0x6150b8) /* "This specifies how big the area is.  Everything in this area will r..." */);
}

// ---- CFakeRenderHeightAction --------------------------------------------------------------------

// class 0x6bfe04, version 1, size 0x10, parent CAction, vtable 0x5f6a0c
static void FakeRenderHeight_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target"
    vtable_and_defaults(o, vt(0x5f6a0c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl FakeRenderHeight_ClassInfo()
{
    return class_getter(glob_addr(0x6bfe04), glob_addr(0x6151b8) /* "CFakeRenderHeightAction" */, 1.0f, action_class, 0x10, 0, fn_addr(0x4c6b60), fn_addr(0x4c6ba0), fn_addr(0x4c6d10));
}
// 0x4c6b60
static void *__cdecl FakeRenderHeight_Create() { return create_one<0x10, FakeRenderHeight_construct>(); }
// 0x4c6ba0
static void *__fastcall FakeRenderHeight_CreateArray(int count, int, int) { return create_array<0x10, FakeRenderHeight_construct>(count); }
// 0x4c6d10. eax: the last call's.
static DWORD __cdecl FakeRenderHeight_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x614d88) /* "Target" */, 8, 4, 8, 1, glob_addr(TEXT_EMPTY), 0);
    add_field(FakeRenderHeight_ClassInfo(), f);
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR_UNITS), glob_addr(0x6151d0) /* "New Render Height" */, glob_addr(0x60b360) /* "Pixels" */, 0xc, 4, 0xc, 1, 0, 0, 0, 0);
    return add_field(FakeRenderHeight_ClassInfo(), f);
}

// ---- COffsetYSortAction -------------------------------------------------------------------------

// class 0x6bfe08, version 1, size 0x10, parent CAction, vtable 0x5f6a7c
static void OffsetYSort_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Target"
    vtable_and_defaults(o, vt(0x5f6a7c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl OffsetYSort_ClassInfo()
{
    return class_getter(glob_addr(0x6bfe08), glob_addr(0x6151e4) /* "COffsetYSortAction" */, 1.0f, action_class, 0x10, 0, fn_addr(0x4c6f00), fn_addr(0x4c6f40), fn_addr(0x4c70b0));
}
// 0x4c6f00
static void *__cdecl OffsetYSort_Create() { return create_one<0x10, OffsetYSort_construct>(); }
// 0x4c6f40
static void *__fastcall OffsetYSort_CreateArray(int count, int, int) { return create_array<0x10, OffsetYSort_construct>(count); }
// 0x4c70b0. eax: the last call's.
static DWORD __cdecl OffsetYSort_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x614d88) /* "Target" */, 8, 4, 8, 1, glob_addr(TEXT_EMPTY), 0);
    add_field(OffsetYSort_ClassInfo(), f);
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR_UNITS), glob_addr(0x6151f8) /* "Y Sort Offset" */, glob_addr(0x60b360) /* "Pixels" */, 0xc, 4, 0xc, 1, 0, 0, 0, 0);
    return add_field(OffsetYSort_ClassInfo(), f);
}

// ---- CSendWeaponDischargedMessage ---------------------------------------------------------------

// class 0x6bfe0c, version 1, size 0x8, parent CAction, vtable 0x5f6aec
static void SendWeaponDischargedMessage_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(0x5f6aec));
}
// (only in the vtable, slot 2)
static CClassInfo *__cdecl SendWeaponDischargedMessage_ClassInfo()
{
    return class_getter(glob_addr(0x6bfe0c), glob_addr(0x615208) /* "CSendWeaponDischargedMessage" */, 1.0f, action_class, 0x8, 0, fn_addr(0x4c7290), fn_addr(0x4c72c0), fn_addr(0x58ea40));
}
// 0x4c7290
static void *__cdecl SendWeaponDischargedMessage_Create() { return create_one<0x8, SendWeaponDischargedMessage_construct>(); }
// 0x4c72c0
static void *__fastcall SendWeaponDischargedMessage_CreateArray(int count, int, int) { return create_array<0x8, SendWeaponDischargedMessage_construct>(count); }

// ---- CSetHomeAction -----------------------------------------------------------------------------

// class 0x6bfe10, version 1, size 0x10, parent CAction, vtable 0x5f6b5c
static void SetHome_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Actor"
    zs_ctor(zstring(o, 0xc));   // "New Home Position"
    vtable_and_defaults(o, vt(0x5f6b5c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl SetHome_ClassInfo()
{
    return class_getter(glob_addr(0x6bfe10), glob_addr(0x615228) /* "CSetHomeAction" */, 1.0f, action_class, 0x10, 0, fn_addr(0x4c7490), fn_addr(0x4c74d0), fn_addr(0x4c7650));
}
// 0x4c7490
static void *__cdecl SetHome_Create() { return create_one<0x10, SetHome_construct>(); }
// 0x4c74d0
static void *__fastcall SetHome_CreateArray(int count, int, int) { return create_array<0x10, SetHome_construct>(count); }
// 0x4c7650. eax: the last call's.
static DWORD __cdecl SetHome_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6152ec) /* "Actor" */, 8, 4, 8, 1, glob_addr(TEXT_EMPTY), 0);
    add_field(SetHome_ClassInfo(), f);
    set_alt_name(last_field(SetHome_ClassInfo()), glob_addr(0x614d88) /* "Target" */);
    set_help(last_field(SetHome_ClassInfo()), glob_addr(0x6152b8) /* "This is the entity who is going to get a new home" */);
    f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6152a4) /* "New Home Position" */, 0xc, 4, 0xc, 1, glob_addr(TEXT_EMPTY), 0);
    add_field(SetHome_ClassInfo(), f);
    return set_help(last_field(SetHome_ClassInfo()), glob_addr(0x615238) /* "LEAVE THIS BLANK to set the home to the actor's current postion OR ..." */);
}

// ---- CAddEffectAction ---------------------------------------------------------------------------

// class 0x6bfe14, version 2, size 0x14, parent CAction, vtable 0x5f6bcc
static void AddEffect_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0x8));   // "Who To Warp"
    vtable_and_defaults(o, vt(0x5f6bcc));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl AddEffect_ClassInfo()
{
    return class_getter(glob_addr(0x6bfe14), glob_addr(0x6152f4) /* "CAddEffectAction" */, 2.0f, action_class, 0x14, 0, fn_addr(0x4c7980), fn_addr(0x4c79c0), fn_addr(0x4c7b30));
}
// 0x4c7980
static void *__cdecl AddEffect_Create() { return create_one<0x14, AddEffect_construct>(); }
// 0x4c79c0
static void *__fastcall AddEffect_CreateArray(int count, int, int) { return create_array<0x14, AddEffect_construct>(count); }
// 0x4c7b30. eax: the last call's.
static DWORD __cdecl AddEffect_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(0x5ed924), fn_addr(STRING_FIELD_CTOR), glob_addr(0x615320) /* "Who To Warp" */, 8, 4, 8, 1, glob_addr(0x6142cc) /* "$Instigator" */, 0);
    add_field(AddEffect_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x615308) /* "Warp Sound" */, 0xc, 4, 0xc, glob_addr(0x713708), 1, glob_addr(0x615314) /* "!None.wav" */, 0, 0);
    add_field(AddEffect_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x614584) /* "Warp Behavior" */, 0x10, 4, 0x10, class_at(fn_addr(0x55e350)) /* CRenderEffectBehavior */, 1, 0, 0, 0);
    return add_field(AddEffect_ClassInfo(), f);
}

// ---- CShakeScreenAction -------------------------------------------------------------------------

// class 0x6bfe18, version 2, size 0x30, parent CAction, vtable 0x5f6c3c
static void ShakeScreen_construct(BYTE *o)
{
    action_ctor(o);
    // +0xc "Pixels To Shake": an embedded CLengthEnvelope (0x24 bytes; CEnvelope's level, +0x20 zeroed)
    obj_ctor(o + 0xc);
    vtable_and_defaults(o + 0xc, vt(0x5f3cbc));
    dw(o, 0x2c) = 0;
    vtable_and_defaults(o + 0xc, vt(0x5f3f2c));
    vtable_and_defaults(o, vt(0x5f6c3c));
}
// (inlined in RegisterFields and other callers)
static CClassInfo *__cdecl ShakeScreen_ClassInfo()
{
    return class_getter(glob_addr(0x6bfe18), glob_addr(0x61532c) /* "CShakeScreenAction" */, 2.0f, action_class, 0x30, 0, fn_addr(0x4c7d90), fn_addr(0x4c7df0), fn_addr(0x4c7fb0));
}
// 0x4c7d90
static void *__cdecl ShakeScreen_Create() { return create_one<0x30, ShakeScreen_construct>(); }
// 0x4c7df0
static void *__fastcall ShakeScreen_CreateArray(int count, int, int) { return create_array<0x30, ShakeScreen_construct>(count); }
// 0x4c7fb0. eax: the last call's.
static DWORD __cdecl ShakeScreen_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x615350) /* "Time to Shake" */, glob_addr(0x60b000) /* "Seconds" */, 8, 4, 8, 1, 0x3f800000 /* 1.0 */, 0, 0, 0x4b189680 /* 10000000.0 */, 0);
    add_field(ShakeScreen_ClassInfo(), f);
    f = new_field(0x34, fn_addr(EMBEDDED_FIELD_CTOR_VALUE), glob_addr(0x615340) /* "Pixels To Shake" */, 0xc, 0x24, 0xc, class_at(fn_addr(0x49f4d0)) /* CLengthEnvelope */, glob_addr(0x613390) /* "Start Value" */, 1);
    return add_field(ShakeScreen_ClassInfo(), f);
}

// ---- The classes of 0x41b000-0x41ffff (their slots are in action_helpers.cpp) -----------------------------
//
// CConditionalAction and CIfAction inline CAction's constructor (CObject's, vtable 0x5ece50,
// Object_InitFields) and CAction's class getter; CActionQueueItem, CActionQueue and CDialogLine derive
// from CObject. CDialogActionBase is abstract (the null factories); CTriggerRelayAction's RegisterFields
// (0x420160) lies outside the range.

// CConditionalAction: class 0x62e778, version 1, size 0x14, parent CAction, vtable 0x5ecec4
static void Conditional_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(VT_ACTION));
    vtable_and_defaults(o, vt(0x5ecec4));
}
// 0x41b270 (slot 2; CAction's getter inlined)
static CClassInfo *__cdecl Conditional_ClassInfo()
{
    return class_getter(glob_addr(0x62e778), glob_addr(0x60ae30) /* "CConditionalAction" */, 1.0f, Action_ClassInfo, 0x14, 0, fn_addr(0x41b120), fn_addr(0x41b160), fn_addr(0x41b320));
}
// 0x41b120
static void *__cdecl Conditional_Create() { return create_one<0x14, Conditional_construct>(); }
// 0x41b160
static void *__fastcall Conditional_CreateArray(int count, int, int) { return create_array<0x14, Conditional_construct>(count); }
// 0x41b320. eax: the last call's.
static DWORD __cdecl Conditional_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60ae60) /* "Try Action" */, 8, 4, 8, Action_ClassInfo(), 1, 0, 0, 0);
    add_field(Conditional_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60ae50) /* "Succeed Action" */, 0xc, 4, 0xc, Action_ClassInfo(), 1, 0, 0, 0);
    add_field(Conditional_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60ae44) /* "Fail Action" */, 0x10, 4, 0x10, Action_ClassInfo(), 1, 0, 0, 0);
    return add_field(Conditional_ClassInfo(), f);
}

// CIfAction: class 0x62e77c, version 1, size 0x14, parent CAction, vtable 0x5ecf34
static void If_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(VT_ACTION));
    vtable_and_defaults(o, vt(0x5ecf34));
}
// 0x41b660 (slot 2; CAction's getter inlined)
static CClassInfo *__cdecl If_ClassInfo()
{
    return class_getter(glob_addr(0x62e77c), glob_addr(0x60ae6c) /* "CIfAction" */, 1.0f, Action_ClassInfo, 0x14, 0, fn_addr(0x41b700), fn_addr(0x41b740), fn_addr(0x41b860));
}
// 0x41b700
static void *__cdecl If_Create() { return create_one<0x14, If_construct>(); }
// 0x41b740
static void *__fastcall If_CreateArray(int count, int, int) { return create_array<0x14, If_construct>(count); }
// 0x41b860. eax: the last call's.
static DWORD __cdecl If_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60ae88) /* "If" */, 8, 4, 8, Action_ClassInfo(), 1, 0, 0, 0);
    add_field(If_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60ae80) /* "Then" */, 0xc, 4, 0xc, Action_ClassInfo(), 1, 0, 0, 0);
    add_field(If_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60ae78) /* "Else" */, 0x10, 4, 0x10, Action_ClassInfo(), 1, 0, 0, 0);
    return add_field(If_ClassInfo(), f);
}

// CActionQueueItem: class 0x62e784, version 3, size 0x34, parent none (CObject), vtable 0x5ecfa4; the
// references +0xc-+0x18 zeroed after Object_InitFields
static void ActionQueueItem_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5ecfa4));
    dw(o, 0xc) = 0;
    dw(o, 0x10) = 0;
    dw(o, 0x14) = 0;
    dw(o, 0x18) = 0;
}
// 0x41bd40 (slot 2)
static CClassInfo *__cdecl ActionQueueItem_ClassInfo()
{
    return class_getter(glob_addr(0x62e784), glob_addr(0x60ae8c) /* "CActionQueueItem" */, 3.0f, nullptr, 0x34, 0, fn_addr(0x41bc20), fn_addr(0x41bc60), fn_addr(0x41bda0));
}
// 0x41bc20
static void *__cdecl ActionQueueItem_Create() { return create_one<0x34, ActionQueueItem_construct>(); }
// 0x41bc60
static void *__fastcall ActionQueueItem_CreateArray(int count, int, int) { return create_array<0x34, ActionQueueItem_construct>(count); }
// 0x41bda0. eax: the last call's.
static DWORD __cdecl ActionQueueItem_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x60aed8) /* "Time To Execute" */, 8, 4, 8, 1, 0, 0, 0, 0x4b18967f /* 9999999.0 */, 0);
    add_field(ActionQueueItem_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60aed0) /* "Action" */, 0x1c, 4, 0x1c, action_class(), 1, 0, 0, 0);
    add_field(ActionQueueItem_ClassInfo(), f);
    f = new_field(0x30, fn_addr(OBJECT_REF_FIELD_CTOR), glob_addr(0x60aec4) /* "Instigator" */, 0xc, 4, 0xc, class_at(fn_addr(0x4f9bf0)) /* CLayerPart */, 0, 0);
    add_field(ActionQueueItem_ClassInfo(), f);
    f = new_field(0x30, fn_addr(OBJECT_REF_FIELD_CTOR), glob_addr(0x60aeb8) /* "TriggerPart" */, 0x10, 4, 0x10, class_at(fn_addr(0x4f9bf0)) /* CLayerPart */, 0, 0);
    add_field(ActionQueueItem_ClassInfo(), f);
    f = new_field(0x30, fn_addr(OBJECT_REF_FIELD_CTOR), glob_addr(0x60aeac) /* "TriggerAI" */, 0x14, 4, 0x14, class_at(fn_addr(0x4fd220)) /* CLayerPartAI */, 0, 0);
    add_field(ActionQueueItem_ClassInfo(), f);
    f = new_field(0x30, fn_addr(OBJECT_REF_FIELD_CTOR), glob_addr(0x60aea0) /* "TargetPart" */, 0x18, 4, 0x18, class_at(fn_addr(0x4f9bf0)) /* CLayerPart */, 0, 0);
    return add_field(ActionQueueItem_ClassInfo(), f);
}

// CActionQueue: class 0x62e788, version 3, size 0x1c, parent none (CObject), vtable 0x5ed000; +8 the
// embedded "Items" array (vtable 0x5ed05c, data, count, capacity), +0x18 the layer 0 after
// Object_InitFields (the constructor 0x41c6d0 inlined)
static void ActionQueue_construct(BYTE *o)
{
    obj_ctor(o);
    dw(o, 0xc) = 0;
    dw(o, 0x10) = 0;
    dw(o, 0x14) = 0;
    dw(o, 8) = vt(0x5ed05c);   // vtable of the embedded object
    vtable_and_defaults(o, vt(0x5ed000));
    dw(o, 0x18) = 0;
}
// 0x41c380 (slot 2)
static CClassInfo *__cdecl ActionQueue_ClassInfo()
{
    return class_getter(glob_addr(0x62e788), glob_addr(0x60af00) /* "CActionQueue" */, 3.0f, nullptr, 0x1c, 0, fn_addr(0x41c3d0), fn_addr(0x41c420), fn_addr(0x41c5c0));
}
// 0x41c3d0
static void *__cdecl ActionQueue_Create() { return create_one<0x1c, ActionQueue_construct>(); }
// 0x41c420
static void *__fastcall ActionQueue_CreateArray(int count, int, int) { return create_array<0x1c, ActionQueue_construct>(count); }
// 0x41c5c0: "Items", an object array of CActionQueueItem. eax: the last call's.
static DWORD __cdecl ActionQueue_RegisterFields()
{
    void *element = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60af18) /* "Item" */, 0, 4, 0, ActionQueueItem_ClassInfo(), 1, 0, 0, 0);
    void *f = new_field(0x48, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x60af10) /* "Items" */, 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc, 4, 0xc, 1, element, 1, 0);
    return add_field(ActionQueue_ClassInfo(), f);
}

// CDialogActionBase: class 0x62e7cc, version 4, size 0x78, abstract, parent CAction
// 0x41d5e0
static CClassInfo *__cdecl DialogActionBase_ClassInfo()
{
    return class_getter(glob_addr(0x62e7cc), glob_addr(0x60b020) /* "CDialogActionBase" */, 4.0f, action_class, 0x78, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x41d690));
}
// 0x41d690: "Script" (a resource of the dialogue script list 0x62e7e0, its callback 0x41d7d0) and "Line"
// (a resource descriptor with the vtable 0x5ed320). eax: the last call's.
static DWORD __cdecl DialogActionBase_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60b03c) /* "Script" */, 8, 4, 8, glob_addr(0x62e7e0), 1, glob_addr(0x60b044) /* "Dialog Script" */, fn_addr(0x41d7d0), 1);
    add_field(DialogActionBase_ClassInfo(), f);
    f = new_field_vt(0x34, vt(0x5ed320), fn_addr(RESOURCE_FIELD_CTOR), glob_addr(0x60b034) /* "Line" */, 0xc, 4, 0xc, 1, 0, 0, 0);
    return add_field(DialogActionBase_ClassInfo(), f);
}

// CDialogLine: class 0x62e7dc, version 1, size 0x14, parent none (CObject), vtable 0x5ed3e0; +0x10 zeroed
static void DialogLine_construct(BYTE *o)
{
    obj_ctor(o);
    zs_ctor(zstring(o, 8));     // "Name"
    zs_ctor(zstring(o, 0xc));   // "Text"
    vtable_and_defaults(o, vt(0x5ed3e0));
    dw(o, 0x10) = 0;
}
// 0x41deb0 (slot 2)
static CClassInfo *__cdecl DialogLine_ClassInfo()
{
    return class_getter(glob_addr(0x62e7dc), glob_addr(0x60b06c) /* "CDialogLine" */, 1.0f, nullptr, 0x14, 0, fn_addr(0x41dd30), fn_addr(0x41dd80), fn_addr(0x41df10));
}
// 0x41dd30
static void *__cdecl DialogLine_Create() { return create_one<0x14, DialogLine_construct>(); }
// 0x41dd80
static void *__fastcall DialogLine_CreateArray(int count, int, int) { return create_array<0x14, DialogLine_construct>(count); }
// 0x41df10. eax: the last call's.
static DWORD __cdecl DialogLine_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x60b080) /* "Name" */, 8, 4, 8, 1, glob_addr(0x60b088) /* "New Line" */, 0);
    add_field(DialogLine_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x60b078) /* "Text" */, 0xc, 4, 0xc, 1, 0, 0);
    return add_field(DialogLine_ClassInfo(), f);
}

// COtherMapAction: class 0x62e934, version 1, size 0x10, parent CAction, vtable 0x5ed710
static void OtherMap_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 0xc));   // "Other Map Name"
    vtable_and_defaults(o, vt(0x5ed710));
}
// 0x41efc0 (slot 2)
static CClassInfo *__cdecl OtherMap_ClassInfo()
{
    return class_getter(glob_addr(0x62e934), glob_addr(0x60b0bc) /* "COtherMapAction" */, 1.0f, action_class, 0x10, 0, fn_addr(0x41ee70), fn_addr(0x41eeb0), fn_addr(0x41f020));
}
// 0x41ee70
static void *__cdecl OtherMap_Create() { return create_one<0x10, OtherMap_construct>(); }
// 0x41eeb0
static void *__fastcall OtherMap_CreateArray(int count, int, int) { return create_array<0x10, OtherMap_construct>(count); }
// 0x41f020. eax: the last call's.
static DWORD __cdecl OtherMap_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60aed0) /* "Action" */, 8, 4, 8, action_class(), 1, 0, 0, 0);
    add_field(OtherMap_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x60b0cc) /* "Other Map Name" */, 0xc, 4, 0xc, 1, 0, 0);
    return add_field(OtherMap_ClassInfo(), f);
}

// CTriggerRelayAction: class 0x62e944, version 1, size 0xc, parent CAction, vtable 0x5ed8b4
static void TriggerRelay_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(zstring(o, 8));   // "Relay Name"
    vtable_and_defaults(o, vt(0x5ed8b4));
}
// 0x41ffb0
static void *__cdecl TriggerRelay_Create() { return create_one<0xc, TriggerRelay_construct>(); }
// 0x41fff0
static void *__fastcall TriggerRelay_CreateArray(int count, int, int) { return create_array<0xc, TriggerRelay_construct>(count); }


// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(action_classes)
{
    const Hook list[] = {
        {0x41a650, (void *)&Action_ClassInfo, "CAction::ClassInfo", 5, {0xa1, 0x70, 0xe7, 0x62, 0x00, 0x85, 0xc0, 0x75}},
        {0x41a6b0, (void *)&Action_Ctor, "CAction::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x58, 0xbf, 0xff, 0xff}, 4},
        {0x4b62d0, (void *)&RemoveVelocity_Create, "CRemoveVelocityAction::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0x35}},
        {0x4b6300, (void *)&RemoveVelocity_CreateArray, "CRemoveVelocityAction::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x4b63e0, (void *)&RemoveVelocity_ClassInfo, "CRemoveVelocityAction::ClassInfoSlot2", 5, {0xa1, 0x14, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4b6540, (void *)&Delay_Create, "CDelayAction::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0xc5}},
        {0x4b6570, (void *)&Delay_CreateArray, "CDelayAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4b6650, (void *)&Delay_ClassInfo, "CDelayAction::ClassInfoSlot2", 5, {0xa1, 0x18, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4b66b0, (void *)&Delay_RegisterFields, "CDelayAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x81, 0x9c, 0x11}, 5},
        {0x4b6a40, (void *)&OnlyOnce_Create, "COnlyOnceAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0xc5}},
        {0x4b6a70, (void *)&OnlyOnce_CreateArray, "COnlyOnceAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4b6b50, (void *)&OnlyOnce_ClassInfo, "COnlyOnceAction::ClassInfoSlot2", 5, {0xa1, 0x1c, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4b6bb0, (void *)&OnlyOnce_RegisterFields, "COnlyOnceAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x81, 0x97, 0x11}, 5},
        {0x4b6d50, (void *)&OnlyOnceEveryXSeconds_Create, "COnlyOnceEveryXSecondsAction::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0xb5}},
        {0x4b6d80, (void *)&OnlyOnceEveryXSeconds_CreateArray, "COnlyOnceEveryXSecondsAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4b6e60, (void *)&OnlyOnceEveryXSeconds_ClassInfo, "COnlyOnceEveryXSecondsAction::ClassInfoSlot2", 5, {0xa1, 0x20, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4b6ec0, (void *)&OnlyOnceEveryXSeconds_RegisterFields, "COnlyOnceEveryXSecondsAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x71, 0x94, 0x11}, 5},
        {0x4b71e0, (void *)&Impersonate_Create, "CImpersonateAction::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x4b7220, (void *)&Impersonate_CreateArray, "CImpersonateAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4b7340, (void *)&Impersonate_ClassInfo, "CImpersonateAction::ClassInfoSlot2", 5, {0xa1, 0x24, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4b73a0, (void *)&Impersonate_RegisterFields, "CImpersonateAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x91, 0x8f, 0x11}, 5},
        {0x4b7920, (void *)&While_Create, "CWhileAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x4b7950, (void *)&While_CreateArray, "CWhileAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4b7a30, (void *)&While_ClassInfo, "CWhileAction::ClassInfoSlot2", 5, {0xa1, 0x28, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4b7a90, (void *)&While_RegisterFields, "CWhileAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0xa1, 0x88, 0x11}, 5},
        {0x4b7cc0, (void *)&Repeat_Create, "CRepeatAction::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x4b7cf0, (void *)&Repeat_CreateArray, "CRepeatAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4b7dd0, (void *)&Repeat_ClassInfo, "CRepeatAction::ClassInfoSlot2", 5, {0xa1, 0x2c, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4b7e30, (void *)&Repeat_RegisterFields, "CRepeatAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x01, 0x85, 0x11}, 5},
        {0x4b8460, (void *)&MultipleActions_ClassInfo, "CMultipleActionsAction::ClassInfo", 5, {0xa1, 0x30, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4b84b0, (void *)&MultipleActions_Create, "CMultipleActionsAction::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x55}},
        {0x4b8500, (void *)&MultipleActions_CreateArray, "CMultipleActionsAction::CreateArray", 5, {0x51, 0x55, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0x76}},
        {0x4b8670, (void *)&MultipleActions_ClassInfo, "CMultipleActionsAction::ClassInfoSlot2", 5, {0xa1, 0x30, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4b86d0, (void *)&MultipleActions_RegisterFields, "CMultipleActionsAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x61, 0x7c, 0x11}, 5},
        {0x4b8930, (void *)&MultipleDelayedActions_Create, "CMultipleDelayedActionsAction::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x4b8990, (void *)&MultipleDelayedActions_CreateArray, "CMultipleDelayedActionsAction::CreateArray", 5, {0x51, 0x55, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd}},
        {0x4b8b50, (void *)&MultipleDelayedActions_ClassInfo, "CMultipleDelayedActionsAction::ClassInfoSlot2", 5, {0xa1, 0x34, 0xfd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4b8c10, (void *)&MultipleDelayedActions_RegisterFields, "CMultipleDelayedActionsAction::RegisterFields", 9, {0x53, 0x57, 0x6a, 0x44, 0xe8, 0x21, 0x77, 0x11}, 5},
        {0x4b8ed0, (void *)&Series_Create, "CSeriesAction::Create", 6, {0x56, 0xb9, 0x24, 0x00, 0x00, 0x00, 0xe8, 0x35}},
        {0x4b8f30, (void *)&Series_CreateArray, "CSeriesAction::CreateArray", 5, {0x51, 0x55, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff}},
        {0x4b90e0, (void *)&Series_ClassInfo, "CSeriesAction::ClassInfoSlot2", 5, {0xa1, 0x38, 0xfd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4b91a0, (void *)&Series_RegisterFields, "CSeriesAction::RegisterFields", 5, {0x53, 0x56, 0x57, 0x6a, 0x38, 0xe8, 0x90, 0x71}},
        {0x4b9620, (void *)&ShuffledSeries_Create, "CShuffledSeriesAction::Create", 6, {0x56, 0xb9, 0x24, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x4b9690, (void *)&ShuffledSeries_CreateArray, "CShuffledSeriesAction::CreateArray", 5, {0x51, 0x55, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff}},
        {0x4b9870, (void *)&ShuffledSeries_ClassInfo, "CShuffledSeriesAction::ClassInfoSlot2", 5, {0xa1, 0x3c, 0xfd, 0x6b, 0x00, 0x53, 0x85, 0xc0}},
        {0x4b9ac0, (void *)&Random_Create, "CRandomAction::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x4b9b20, (void *)&Random_CreateArray, "CRandomAction::CreateArray", 5, {0x51, 0x55, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f}},
        {0x4b9cd0, (void *)&Random_ClassInfo, "CRandomAction::ClassInfoSlot2", 5, {0xa1, 0x40, 0xfd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4b9ed0, (void *)&And_Create, "CAndAction::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0x35}},
        {0x4b9f30, (void *)&And_CreateArray, "CAndAction::CreateArray", 5, {0x51, 0x55, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd}},
        {0x4ba0f0, (void *)&And_ClassInfo, "CAndAction::ClassInfoSlot2", 5, {0xa1, 0x44, 0xfd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4ba1b0, (void *)&And_RegisterFields, "CAndAction::RegisterFields", 9, {0x53, 0x57, 0x6a, 0x30, 0xe8, 0x81, 0x61, 0x11}, 5},
        {0x4ba3f0, (void *)&XOr_Create, "CXOrAction::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x4ba450, (void *)&XOr_CreateArray, "CXOrAction::CreateArray", 5, {0x51, 0x55, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd}},
        {0x4ba610, (void *)&XOr_ClassInfo, "CXOrAction::ClassInfoSlot2", 5, {0xa1, 0x48, 0xfd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4ba6d0, (void *)&XOr_RegisterFields, "CXOrAction::RegisterFields", 9, {0x53, 0x57, 0x6a, 0x30, 0xe8, 0x61, 0x5c, 0x11}, 5},
        {0x4ba930, (void *)&Or_Create, "COrAction::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x4ba990, (void *)&Or_CreateArray, "COrAction::CreateArray", 5, {0x51, 0x55, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd}},
        {0x4bab50, (void *)&Or_ClassInfo, "COrAction::ClassInfoSlot2", 5, {0xa1, 0x4c, 0xfd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4bac10, (void *)&Or_RegisterFields, "COrAction::RegisterFields", 9, {0x53, 0x57, 0x6a, 0x30, 0xe8, 0x21, 0x57, 0x11}, 5},
        {0x4badf0, (void *)&Succeed_Create, "CSucceedAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x4bae20, (void *)&Succeed_CreateArray, "CSucceedAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4baf00, (void *)&Succeed_ClassInfo, "CSucceedAction::ClassInfoSlot2", 5, {0xa1, 0x50, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4baf60, (void *)&Succeed_RegisterFields, "CSucceedAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0xd1, 0x53, 0x11}, 5},
        {0x4bb070, (void *)&Fail_Create, "CFailAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x95}},
        {0x4bb0a0, (void *)&Fail_CreateArray, "CFailAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4bb180, (void *)&Fail_ClassInfo, "CFailAction::ClassInfoSlot2", 5, {0xa1, 0x54, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4bb1e0, (void *)&Fail_RegisterFields, "CFailAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x51, 0x51, 0x11}, 5},
        {0x4bb2f0, (void *)&Not_Create, "CNotAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x4bb320, (void *)&Not_CreateArray, "CNotAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4bb400, (void *)&Not_ClassInfo, "CNotAction::ClassInfoSlot2", 5, {0xa1, 0x58, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4bb460, (void *)&Not_RegisterFields, "CNotAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0xd1, 0x4e, 0x11}, 5},
        {0x4bb620, (void *)&Clone_Create, "CCloneAction::Create", 6, {0x56, 0xb9, 0x28, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x4bb680, (void *)&Clone_CreateArray, "CCloneAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4bb7e0, (void *)&Clone_ClassInfo, "CCloneAction::ClassInfoSlot2", 5, {0xa1, 0x5c, 0xfd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4bb8a0, (void *)&Clone_RegisterFields, "CCloneAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x91, 0x4a, 0x11}, 5},
        {0x4bbe00, (void *)&CreateLayerPart_RegisterFields, "CCreateLayerPartAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x31, 0x45, 0x11}, 5},
        {0x4bc510, (void *)&CreateDefaultEntity_Create, "CCreateDefaultEntityAction::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0xf5}},
        {0x4bc560, (void *)&CreateDefaultEntity_CreateArray, "CCreateDefaultEntityAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x05}},
        {0x4bc6a0, (void *)&CreateDefaultEntity_ClassInfo, "CCreateDefaultEntityAction::ClassInfoSlot2", 5, {0xa1, 0x64, 0xfd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4bc760, (void *)&CreateDefaultEntity_RegisterFields, "CCreateDefaultEntityAction::RegisterFields", 5, {0x53, 0x56, 0x57, 0x6a, 0x38, 0xe8, 0xd0, 0x3b}},
        {0x4bca00, (void *)&CreateCustomEntity_Create, "CCreateCustomEntityAction::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x4bca50, (void *)&CreateCustomEntity_CreateArray, "CCreateCustomEntityAction::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x4bcb90, (void *)&CreateCustomEntity_ClassInfo, "CCreateCustomEntityAction::ClassInfoSlot2", 5, {0xa1, 0x68, 0xfd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4bcc50, (void *)&CreateCustomEntity_RegisterFields, "CCreateCustomEntityAction::RegisterFields", 5, {0x53, 0x56, 0x57, 0x6a, 0x3c, 0xe8, 0xe0, 0x36}},
        {0x4bd100, (void *)&LockDoor_Create, "CLockDoorAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x4bd140, (void *)&LockDoor_CreateArray, "CLockDoorAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4bd250, (void *)&LockDoor_ClassInfo, "CLockDoorAction::ClassInfoSlot2", 5, {0xa1, 0x6c, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4bd2b0, (void *)&LockDoor_RegisterFields, "CLockDoorAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x81, 0x30, 0x11}, 5},
        {0x4bd3c0, (void *)&UnlockDoor_Create, "CUnlockDoorAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x4bd400, (void *)&UnlockDoor_CreateArray, "CUnlockDoorAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4bd510, (void *)&UnlockDoor_ClassInfo, "CUnlockDoorAction::ClassInfoSlot2", 5, {0xa1, 0x70, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4bd570, (void *)&UnlockDoor_RegisterFields, "CUnlockDoorAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xc1, 0x2d, 0x11}, 5},
        {0x4bd710, (void *)&XDoor_RegisterFields, "CXDoorAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x21, 0x2c, 0x11}, 5},
        {0x4bde10, (void *)&OpenDoor_Create, "COpenDoorAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xf5}},
        {0x4bde60, (void *)&OpenDoor_CreateArray, "COpenDoorAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4bdf80, (void *)&OpenDoor_ClassInfo, "COpenDoorAction::ClassInfoSlot2", 5, {0xa1, 0x78, 0xfd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4be130, (void *)&CloseDoor_Create, "CCloseDoorAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x4be180, (void *)&CloseDoor_CreateArray, "CCloseDoorAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4be2a0, (void *)&CloseDoor_ClassInfo, "CCloseDoorAction::ClassInfoSlot2", 5, {0xa1, 0x7c, 0xfd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4be450, (void *)&ToggleDoor_Create, "CToggleDoorAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xb5}},
        {0x4be4a0, (void *)&ToggleDoor_CreateArray, "CToggleDoorAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4be5c0, (void *)&ToggleDoor_ClassInfo, "CToggleDoorAction::ClassInfoSlot2", 5, {0xa1, 0x80, 0xfd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4be860, (void *)&PlayAnimation_Create, "CPlayAnimationAction::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x4be8b0, (void *)&PlayAnimation_CreateArray, "CPlayAnimationAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x05}},
        {0x4be9f0, (void *)&PlayAnimation_ClassInfo, "CPlayAnimationAction::ClassInfoSlot2", 5, {0xa1, 0x84, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4bea50, (void *)&PlayAnimation_RegisterFields, "CPlayAnimationAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xe1, 0x18, 0x11}, 5},
        {0x4beed0, (void *)&SetCurSequence_Create, "CSetCurSequenceAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x35}},
        {0x4bef10, (void *)&SetCurSequence_CreateArray, "CSetCurSequenceAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4bf030, (void *)&SetCurSequence_ClassInfo, "CSetCurSequenceAction::ClassInfoSlot2", 5, {0xa1, 0x88, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4bf090, (void *)&SetCurSequence_RegisterFields, "CSetCurSequenceAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xa1, 0x12, 0x11}, 5},
        {0x4bf320, (void *)&CheckCurSequence_Create, "CCheckCurSequenceAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x4bf360, (void *)&CheckCurSequence_CreateArray, "CCheckCurSequenceAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4bf480, (void *)&CheckCurSequence_ClassInfo, "CCheckCurSequenceAction::ClassInfoSlot2", 5, {0xa1, 0x8c, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4bf4e0, (void *)&CheckCurSequence_RegisterFields, "CCheckCurSequenceAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x51, 0x0e, 0x11}, 5},
        {0x4bf690, (void *)&IsDoorOpen_Create, "CIsDoorOpenAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x4bf6d0, (void *)&IsDoorOpen_CreateArray, "CIsDoorOpenAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4bf7e0, (void *)&IsDoorOpen_ClassInfo, "CIsDoorOpenAction::ClassInfoSlot2", 5, {0xa1, 0x90, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4bf840, (void *)&IsDoorOpen_RegisterFields, "CIsDoorOpenAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xf1, 0x0a, 0x11}, 5},
        {0x4bf950, (void *)&IsDoorClosed_Create, "CIsDoorClosedAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0xb5}},
        {0x4bf990, (void *)&IsDoorClosed_CreateArray, "CIsDoorClosedAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4bfaa0, (void *)&IsDoorClosed_ClassInfo, "CIsDoorClosedAction::ClassInfoSlot2", 5, {0xa1, 0x94, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4bfb00, (void *)&IsDoorClosed_RegisterFields, "CIsDoorClosedAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x31, 0x08, 0x11}, 5},
        {0x4bfc10, (void *)&IsDoorLocked_Create, "CIsDoorLockedAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0xf5}},
        {0x4bfc50, (void *)&IsDoorLocked_CreateArray, "CIsDoorLockedAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4bfd60, (void *)&IsDoorLocked_ClassInfo, "CIsDoorLockedAction::ClassInfoSlot2", 5, {0xa1, 0x98, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4bfdc0, (void *)&IsDoorLocked_RegisterFields, "CIsDoorLockedAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x71, 0x05, 0x11}, 5},
        {0x4bfee0, (void *)&PlaySound_Create, "CPlaySoundAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x4bff20, (void *)&PlaySound_CreateArray, "CPlaySoundAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4c0090, (void *)&PlaySound_RegisterFields, "CPlaySoundAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x38, 0xe8, 0xa1, 0x02, 0x11}, 5},
        {0x4c0200, (void *)&SyfaceTypeSoundItem_Create, "SyfaceTypeSoundItem::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x4c0230, (void *)&SyfaceTypeSoundItem_CreateArray, "SyfaceTypeSoundItem::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4c0370, (void *)&SyfaceTypeSoundItem_RegisterFields, "SyfaceTypeSoundItem::RegisterFields", 8, {0x56, 0x6a, 0x38, 0xe8, 0xc2, 0xff, 0x10, 0x00}, 4},
        {0x4c0500, (void *)&PlaySoundBySurfaceType_Create, "CPlaySoundBySurfaceTypeAction::Create", 6, {0x56, 0xb9, 0x24, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x4c0550, (void *)&PlaySoundBySurfaceType_CreateArray, "CPlaySoundBySurfaceTypeAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff, 0x8d, 0x0c}},
        {0x4c06e0, (void *)&PlaySoundBySurfaceType_RegisterFields, "CPlaySoundBySurfaceTypeAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x38, 0xe8, 0x51, 0xfc, 0x10}, 5},
        {0x4c0aa0, (void *)&PlayVO_Create, "CPlayVOAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x4c0ad0, (void *)&PlayVO_CreateArray, "CPlayVOAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4c0c10, (void *)&PlayVO_RegisterFields, "CPlayVOAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x38, 0xe8, 0x21, 0xf7, 0x10}, 5},
        {0x4c0cf0, (void *)&SwitchMapActionBase_ClassInfo, "CSwitchMapActionBase::ClassInfo", 5, {0xa1, 0xac, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c0da0, (void *)&SwitchMapActionBase_RegisterFields, "CSwitchMapActionBase::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x91, 0xf5, 0x10}, 5},
        {0x4c0fa0, (void *)&SwitchMapActionBase_Ctor, "CSwitchMapActionBase::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x08, 0x97, 0xf5, 0xff}, 4},
        {0x4c1420, (void *)&Show_Create, "CShowAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x4c1460, (void *)&Show_CreateArray, "CShowAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4c15d0, (void *)&Show_RegisterFields, "CShowAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x61, 0xed, 0x10}, 5},
        {0x4c16e0, (void *)&Hide_Create, "CHideAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x4c1720, (void *)&Hide_CreateArray, "CHideAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4c1890, (void *)&Hide_RegisterFields, "CHideAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xa1, 0xea, 0x10}, 5},
        {0x4c19a0, (void *)&SetCollidable_Create, "CSetCollidableAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x4c19e0, (void *)&SetCollidable_CreateArray, "CSetCollidableAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4c1b50, (void *)&SetCollidable_RegisterFields, "CSetCollidableAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xe1, 0xe7, 0x10}, 5},
        {0x4c1cf0, (void *)&SetModel_Create, "CSetModelAction::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x4c1d30, (void *)&SetModel_CreateArray, "CSetModelAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4c1ea0, (void *)&SetModel_RegisterFields, "CSetModelAction::RegisterFields", 8, {0x56, 0x6a, 0x34, 0xe8, 0x92, 0xe4, 0x10, 0x00}, 4},
        {0x4c2230, (void *)&SetCategory_Create, "CSetCategoryAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x4c2270, (void *)&SetCategory_CreateArray, "CSetCategoryAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4c23e0, (void *)&SetCategory_RegisterFields, "CSetCategoryAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x51, 0xdf, 0x10}, 5},
        {0x4c25e0, (void *)&Activate_Create, "CActivateAction::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x4c2620, (void *)&Activate_CreateArray, "CActivateAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4c2790, (void *)&Activate_RegisterFields, "CActivateAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xa1, 0xdb, 0x10}, 5},
        {0x4c2b20, (void *)&Deactivate_Create, "CDeactivateAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x4c2b60, (void *)&Deactivate_CreateArray, "CDeactivateAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4c2cd0, (void *)&Deactivate_RegisterFields, "CDeactivateAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x61, 0xd6, 0x10}, 5},
        {0x4c2de0, (void *)&Delete_Create, "CDeleteAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x4c2e20, (void *)&Delete_CreateArray, "CDeleteAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4c2f90, (void *)&Delete_RegisterFields, "CDeleteAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xa1, 0xd3, 0x10}, 5},
        {0x4c30d0, (void *)&SpawnEffect_Create, "CSpawnEffectAction::Create", 6, {0x56, 0xb9, 0x24, 0x00, 0x00, 0x00, 0xe8, 0x35}},
        {0x4c3110, (void *)&SpawnEffect_CreateArray, "CSpawnEffectAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff, 0x8d, 0x0c}},
        {0x4c3280, (void *)&SpawnEffect_RegisterFields, "CSpawnEffectAction::RegisterFields", 5, {0xa1, 0xd0, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c3840, (void *)&Pain_Create, "CPainAction::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0xc5}},
        {0x4c3880, (void *)&Pain_CreateArray, "CPainAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4c39f0, (void *)&Pain_RegisterFields, "CPainAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x41, 0xc9, 0x10}, 5},
        {0x4c3c20, (void *)&Pain_CtorValues, "CPainAction::CtorValues", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x87, 0x6a, 0xf5}, 5},
        {0x4c3d40, (void *)&Destroy_Create, "CDestroyAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0xc5}},
        {0x4c3d80, (void *)&Destroy_CreateArray, "CDestroyAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4c3ef0, (void *)&Destroy_RegisterFields, "CDestroyAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x41, 0xc4, 0x10}, 5},
        {0x4c4060, (void *)&FlipSwitch_Create, "CFlipSwitchAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x4c40a0, (void *)&FlipSwitch_CreateArray, "CFlipSwitchAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4c4210, (void *)&FlipSwitch_RegisterFields, "CFlipSwitchAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x21, 0xc1, 0x10}, 5},
        {0x4c4330, (void *)&AddAI_Create, "CAddAIAction::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x4c4370, (void *)&AddAI_CreateArray, "CAddAIAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4c44e0, (void *)&AddAI_RegisterFields, "CAddAIAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x51, 0xbe, 0x10}, 5},
        {0x4c4710, (void *)&RemoveAI_Create, "CRemoveAIAction::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0xf5}},
        {0x4c4750, (void *)&RemoveAI_CreateArray, "CRemoveAIAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4c48c0, (void *)&RemoveAI_RegisterFields, "CRemoveAIAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x71, 0xba, 0x10}, 5},
        {0x4c4aa0, (void *)&DischargeWeaponThisFrameBase_ClassInfo, "CDischargeWeaponThisFrameBaseAction::ClassInfo", 5, {0xa1, 0xe8, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c4b00, (void *)&DischargeWeaponThisFrameBase_Create, "CDischargeWeaponThisFrameBaseAction::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x4c4b40, (void *)&DischargeWeaponThisFrameBase_CreateArray, "CDischargeWeaponThisFrameBaseAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x05}},
        {0x4c4ca0, (void *)&DischargeWeaponThisFrameBase_RegisterFields, "CDischargeWeaponThisFrameBaseAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x91, 0xb6, 0x10}, 5},
        {0x4c5530, (void *)&ChangeSoundEnvironmentType_Create, "CChangeSoundEnvironmentTypeAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x4c5560, (void *)&ChangeSoundEnvironmentType_CreateArray, "CChangeSoundEnvironmentTypeAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4c56a0, (void *)&ChangeSoundEnvironmentType_RegisterFields, "CChangeSoundEnvironmentTypeAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x38, 0xe8, 0x91, 0xac, 0x10}, 5},
        {0x4c57a0, (void *)&CheckExistence_Create, "CCheckExistenceAction::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x4c57e0, (void *)&CheckExistence_CreateArray, "CCheckExistenceAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4c5950, (void *)&CheckExistence_RegisterFields, "CCheckExistenceAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xe1, 0xa9, 0x10}, 5},
        {0x4c5ae0, (void *)&IsAlive_Create, "CIsAliveAction::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x4c5b20, (void *)&IsAlive_CreateArray, "CIsAliveAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4c5c90, (void *)&IsAlive_RegisterFields, "CIsAliveAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xa1, 0xa6, 0x10}, 5},
        {0x4c5e70, (void *)&HealthIsAtLeast_Create, "CHealthIsAtLeast::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x95}},
        {0x4c5eb0, (void *)&HealthIsAtLeast_CreateArray, "CHealthIsAtLeast::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x4c6020, (void *)&HealthIsAtLeast_RegisterFields, "CHealthIsAtLeast::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x11, 0xa3, 0x10}, 5},
        {0x4c6280, (void *)&SendMessage_Create, "CSendMessageAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x85}},
        {0x4c62c0, (void *)&SendMessage_CreateArray, "CSendMessageAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4c6440, (void *)&SendMessage_RegisterFields, "CSendMessageAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xf1, 0x9e, 0x10}, 5},
        {0x4c6640, (void *)&BroadcastMessageToArea_Create, "CBroadcastMessageToAreaAction::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0xc5}},
        {0x4c6680, (void *)&BroadcastMessageToArea_CreateArray, "CBroadcastMessageToAreaAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4c6800, (void *)&BroadcastMessageToArea_RegisterFields, "CBroadcastMessageToAreaAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x31, 0x9b, 0x10}, 5},
        {0x4c6b60, (void *)&FakeRenderHeight_Create, "CFakeRenderHeightAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x4c6ba0, (void *)&FakeRenderHeight_CreateArray, "CFakeRenderHeightAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4c6d10, (void *)&FakeRenderHeight_RegisterFields, "CFakeRenderHeightAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x21, 0x96, 0x10}, 5},
        {0x4c6f00, (void *)&OffsetYSort_Create, "COffsetYSortAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x4c6f40, (void *)&OffsetYSort_CreateArray, "COffsetYSortAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4c70b0, (void *)&OffsetYSort_RegisterFields, "COffsetYSortAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x81, 0x92, 0x10}, 5},
        {0x4c7290, (void *)&SendWeaponDischargedMessage_Create, "CSendWeaponDischargedMessage::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x4c72c0, (void *)&SendWeaponDischargedMessage_CreateArray, "CSendWeaponDischargedMessage::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x4c7490, (void *)&SetHome_Create, "CSetHomeAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x4c74d0, (void *)&SetHome_CreateArray, "CSetHomeAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x4c7650, (void *)&SetHome_RegisterFields, "CSetHomeAction::RegisterFields", 8, {0x56, 0x6a, 0x34, 0xe8, 0xe2, 0x8c, 0x10, 0x00}, 4},
        {0x4c7980, (void *)&AddEffect_Create, "CAddEffectAction::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x85}},
        {0x4c79c0, (void *)&AddEffect_CreateArray, "CAddEffectAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4c7b30, (void *)&AddEffect_RegisterFields, "CAddEffectAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x01, 0x88, 0x10}, 5},
        {0x4c7d90, (void *)&ShakeScreen_Create, "CShakeScreenAction::Create", 6, {0x57, 0xb9, 0x30, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x4c7df0, (void *)&ShakeScreen_CreateArray, "CShakeScreenAction::CreateArray", 7, {0x51, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0x76, 0xc1}},
        {0x4c7fb0, (void *)&ShakeScreen_RegisterFields, "CShakeScreenAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x81, 0x83, 0x10}, 5},
        {0x4c0030, (void *)&PlaySound_ClassInfo, "CPlaySoundAction::ClassInfoSlot2", 5, {0xa1, 0x9c, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c0310, (void *)&SyfaceTypeSoundItem_ClassInfo, "SyfaceTypeSoundItem::ClassInfoSlot2", 5, {0xa1, 0xa0, 0xfd, 0x6b, 0x00, 0x85, 0xc0, 0x75}},
        {0x4c0680, (void *)&PlaySoundBySurfaceType_ClassInfo, "CPlaySoundBySurfaceTypeAction::ClassInfoSlot2", 5, {0xa1, 0xa4, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c0bb0, (void *)&PlayVO_ClassInfo, "CPlayVOAction::ClassInfoSlot2", 5, {0xa1, 0xa8, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c1570, (void *)&Show_ClassInfo, "CShowAction::ClassInfoSlot2", 5, {0xa1, 0xb0, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c1830, (void *)&Hide_ClassInfo, "CHideAction::ClassInfoSlot2", 5, {0xa1, 0xb4, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c1af0, (void *)&SetCollidable_ClassInfo, "CSetCollidableAction::ClassInfoSlot2", 5, {0xa1, 0xb8, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c1e40, (void *)&SetModel_ClassInfo, "CSetModelAction::ClassInfoSlot2", 5, {0xa1, 0xbc, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c2380, (void *)&SetCategory_ClassInfo, "CSetCategoryAction::ClassInfoSlot2", 5, {0xa1, 0xc0, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c2730, (void *)&Activate_ClassInfo, "CActivateAction::ClassInfoSlot2", 5, {0xa1, 0xc4, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c2c70, (void *)&Deactivate_ClassInfo, "CDeactivateAction::ClassInfoSlot2", 5, {0xa1, 0xc8, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c2f30, (void *)&Delete_ClassInfo, "CDeleteAction::ClassInfoSlot2", 5, {0xa1, 0xcc, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c3220, (void *)&SpawnEffect_ClassInfo, "CSpawnEffectAction::ClassInfoSlot2", 5, {0xa1, 0xd0, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c3990, (void *)&Pain_ClassInfo, "CPainAction::ClassInfoSlot2", 5, {0xa1, 0xd4, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c3e90, (void *)&Destroy_ClassInfo, "CDestroyAction::ClassInfoSlot2", 5, {0xa1, 0xd8, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c41b0, (void *)&FlipSwitch_ClassInfo, "CFlipSwitchAction::ClassInfoSlot2", 5, {0xa1, 0xdc, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c4480, (void *)&AddAI_ClassInfo, "CAddAIAction::ClassInfoSlot2", 5, {0xa1, 0xe0, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c4860, (void *)&RemoveAI_ClassInfo, "CRemoveAIAction::ClassInfoSlot2", 5, {0xa1, 0xe4, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c4c40, (void *)&DischargeWeaponThisFrameBase_ClassInfo, "CDischargeWeaponThisFrameBaseAction::ClassInfoSlot2", 5, {0xa1, 0xe8, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c5640, (void *)&ChangeSoundEnvironmentType_ClassInfo, "CChangeSoundEnvironmentTypeAction::ClassInfoSlot2", 5, {0xa1, 0xec, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c58f0, (void *)&CheckExistence_ClassInfo, "CCheckExistenceAction::ClassInfoSlot2", 5, {0xa1, 0xf0, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c5c30, (void *)&IsAlive_ClassInfo, "CIsAliveAction::ClassInfoSlot2", 5, {0xa1, 0xf4, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c5fc0, (void *)&HealthIsAtLeast_ClassInfo, "CHealthIsAtLeast::ClassInfoSlot2", 5, {0xa1, 0xf8, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c63e0, (void *)&SendMessage_ClassInfo, "CSendMessageAction::ClassInfoSlot2", 5, {0xa1, 0xfc, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c67a0, (void *)&BroadcastMessageToArea_ClassInfo, "CBroadcastMessageToAreaAction::ClassInfoSlot2", 5, {0xa1, 0x00, 0xfe, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c6cb0, (void *)&FakeRenderHeight_ClassInfo, "CFakeRenderHeightAction::ClassInfoSlot2", 5, {0xa1, 0x04, 0xfe, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c7050, (void *)&OffsetYSort_ClassInfo, "COffsetYSortAction::ClassInfoSlot2", 5, {0xa1, 0x08, 0xfe, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c73a0, (void *)&SendWeaponDischargedMessage_ClassInfo, "CSendWeaponDischargedMessage::ClassInfoSlot2", 5, {0xa1, 0x0c, 0xfe, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c75f0, (void *)&SetHome_ClassInfo, "CSetHomeAction::ClassInfoSlot2", 5, {0xa1, 0x10, 0xfe, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c7ad0, (void *)&AddEffect_ClassInfo, "CAddEffectAction::ClassInfoSlot2", 5, {0xa1, 0x14, 0xfe, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4c7f50, (void *)&ShakeScreen_ClassInfo, "CShakeScreenAction::ClassInfoSlot2", 5, {0xa1, 0x18, 0xfe, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x41b120, (void *)&Conditional_Create, "CConditionalAction::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x41b160, (void *)&Conditional_CreateArray, "CConditionalAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x41b270, (void *)&Conditional_ClassInfo, "CConditionalAction::ClassInfoSlot2", 5, {0xa1, 0x78, 0xe7, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x41b320, (void *)&Conditional_RegisterFields, "CConditionalAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x11, 0x50, 0x1b}, 5},
        {0x41b660, (void *)&If_ClassInfo, "CIfAction::ClassInfoSlot2", 5, {0xa1, 0x7c, 0xe7, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x41b700, (void *)&If_Create, "CIfAction::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x41b740, (void *)&If_CreateArray, "CIfAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x41b860, (void *)&If_RegisterFields, "CIfAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0xd1, 0x4a, 0x1b}, 5},
        {0x41bc20, (void *)&ActionQueueItem_Create, "CActionQueueItem::Create", 7, {0x56, 0x57, 0xb9, 0x34, 0x00, 0x00, 0x00, 0xe8}},
        {0x41bc60, (void *)&ActionQueueItem_CreateArray, "CActionQueueItem::CreateArray", 7, {0x53, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d}},
        {0x41bd40, (void *)&ActionQueueItem_ClassInfo, "CActionQueueItem::ClassInfoSlot2", 5, {0xa1, 0x84, 0xe7, 0x62, 0x00, 0x85, 0xc0, 0x75}},
        {0x41bda0, (void *)&ActionQueueItem_RegisterFields, "CActionQueueItem::RegisterFields", 8, {0x56, 0x6a, 0x44, 0xe8, 0x92, 0x45, 0x1b, 0x00}, 4},
        {0x41c380, (void *)&ActionQueue_ClassInfo, "CActionQueue::ClassInfoSlot2", 5, {0xa1, 0x88, 0xe7, 0x62, 0x00, 0x85, 0xc0, 0x75}},
        {0x41c3d0, (void *)&ActionQueue_Create, "CActionQueue::Create", 7, {0x56, 0x57, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8}},
        {0x41c420, (void *)&ActionQueue_CreateArray, "CActionQueue::CreateArray", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xf5}},
        {0x41c5c0, (void *)&ActionQueue_RegisterFields, "CActionQueue::RegisterFields", 8, {0x56, 0x6a, 0x3c, 0xe8, 0x72, 0x3d, 0x1b, 0x00}, 4},
        {0x41d5e0, (void *)&DialogActionBase_ClassInfo, "CDialogActionBase_ClassInfo", 5, {0xa1, 0xcc, 0xe7, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x41d690, (void *)&DialogActionBase_RegisterFields, "CDialogActionBase::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x38, 0xe8, 0xa1, 0x2c, 0x1b}, 5},
        {0x41dd30, (void *)&DialogLine_Create, "CDialogLine::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x41dd80, (void *)&DialogLine_CreateArray, "CDialogLine::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x41deb0, (void *)&DialogLine_ClassInfo, "CDialogLine::ClassInfoSlot2", 5, {0xa1, 0xdc, 0xe7, 0x62, 0x00, 0x85, 0xc0, 0x75}},
        {0x41df10, (void *)&DialogLine_RegisterFields, "CDialogLine::RegisterFields", 8, {0x56, 0x6a, 0x34, 0xe8, 0x22, 0x24, 0x1b, 0x00}, 4},
        {0x41ee70, (void *)&OtherMap_Create, "COtherMapAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x95}},
        {0x41eeb0, (void *)&OtherMap_CreateArray, "COtherMapAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x41efc0, (void *)&OtherMap_ClassInfo, "COtherMapAction::ClassInfoSlot2", 5, {0xa1, 0x34, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x41f020, (void *)&OtherMap_RegisterFields, "COtherMapAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x11, 0x13, 0x1b}, 5},
        {0x41ffb0, (void *)&TriggerRelay_Create, "CTriggerRelayAction::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x55}},
        {0x41fff0, (void *)&TriggerRelay_CreateArray, "CTriggerRelayAction::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
