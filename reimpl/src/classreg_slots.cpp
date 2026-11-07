// The 24 functions of batch C1 that begin with an inlined class registration but do more (phase 5a,
// docs/DECOMPILATION.md 10, "Batch C1"); the map named them as class info getters by their first block. Written
// by hand from the code; the registrations themselves are the generated per-variable functions of
// classreg_vars.h (tools/gen_classreg.py). [confirmed: code, the checks in checks_classreg_slots.cpp]
// - Static initializers (run before WinMain through the CRT's initializer table, some through CallMember
//   0x55d050): nine register an extra-list entry of a class (FieldDesc_RegisterExtra 0x4190e0, thiscall entry
//   object, descriptor, class, help 0; eax its this); three create an object field descriptor (0x3c bytes from
//   operator new, ObjectField_Ctor 0x4186f0) of a class into a global (0 when operator new gives none; eax the
//   descriptor or 0); two make a field descriptor's class (+0x2c) a class after registering it and another.
// - The action wrappers' slot 1 (Describe, thiscall wrapper, action, CClassInfo **out): the text of the menu
//   entry that converts an action, 0 when it does not apply ("OtherMap %s action", "This Map %s action",
//   "Single %s action" in the global ZString 0x62e6e4, as CDelayWrapper::Describe in action_slots.cpp), and the
//   multi-action wrapper's slot 1 at 0x5f52b8 (the class's short name or the wrapper's own text at +8).
// - CLevelOfDifficulty's definition list slot (object, value) and the water warp effect's file callback
//   (fastcall object, ZString *out): the record of the list whose object is the given one.
// - CZaxApp's slots 99, 100 and 102: values of the custom rules' display object (+0x1c of the world's rules
//   +0x38, made on first use: 0x60 bytes, CObject with three FieldColors, vtable 0x5f9e98, slot 19).
#include "classreg_vars.h"
#include "exe_base.h"

namespace zax {

namespace {

using namespace classreg;

inline DWORD &dwa(GlobKey a) { return *glob<DWORD *>(a); }
inline DWORD &dwo(DWORD p, DWORD off) { return *at<DWORD *>(p + off); }
inline BYTE &byo(DWORD p, DWORD off) { return *at<BYTE *>(p + off); }

inline BYTE is_kind_of(DWORD obj, DWORD cls) { return fn<BYTE(THISCALL *)(void *, DWORD)>(0x416790)(at<void *>(obj), cls); }
inline DWORD action_class() { return fn<DWORD(__cdecl *)()>(0x41a650)(); }
inline DWORD short_name(DWORD cls) { return fn<DWORD(THISCALL *)(void *)>(0x415370)(at<void *>(cls)); }
inline void display_name(DWORD cls, ZString *out, DWORD suffix)
{
    fn<DWORD(THISCALL *)(void *, ZString *, DWORD)>(0x4153b0)(at<void *>(cls), out, suffix);
}
inline DWORD slot2(DWORD obj) { return vcall<DWORD>(at<void *>(obj), 2); }
inline void zs_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
inline void zs_dtor(ZString *z) { fn<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
inline DWORD zs_c_str(const void *z) { return fn<DWORD(THISCALL *)(const void *)>(0x4e13a0)(z); }
constexpr DWORD G_FORMAT = 0x62e6e4;   // the global ZString the Describe slots format into
inline void zs_format(GlobKey fmt, DWORD arg) { fn<void(__cdecl *)(void *, DWORD, DWORD)>(0x4e09b0)(glob<void *>(G_FORMAT), key_glob_addr(fmt), arg); }

// "<format>" with the class name of the action (slot 2), CAction's short name cut off: the global's text
DWORD describe(DWORD action, GlobKey fmt)
{
    ZString name;
    zs_ctor(&name);
    const DWORD strip = short_name(action_class());
    display_name(slot2(action), &name, strip);
    zs_format(fmt, zs_c_str(&name));
    const DWORD r = zs_c_str(glob<void *>(G_FORMAT));
    zs_dtor(&name);
    return r;
}

inline DWORD register_extra(GlobKey entry, DWORD desc, DWORD cls)
{
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4190e0)(glob<void *>(entry), desc, cls, 0);
}
inline DWORD object_field(GlobKey name, Parent cls, DWORD kind)
{
    void *f = fn<void *(__cdecl *)(DWORD)>(0x5d033a)(0x3c);
    if (!f)
        return 0;
    const DWORD c = cls();
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4186f0)(
        f, key_glob_addr(name), 0, 4, 0, c, kind, 0, 0, 0);
}

// ---- Static initializers: extra-list entries -------------------------------------------------------------------

// Each: the class registered (inlined getter), then the descriptor global read, then RegisterExtra
DWORD __cdecl StaticInit_ExtraEntry_6bdcb1() { const DWORD c = reg_6bdcc4(); return register_extra(0x6bdcb1, dwa(0x6bdca0), c); }   // 0x483ee0
DWORD __cdecl StaticInit_ExtraEntry_6bdd5a() { const DWORD c = reg_6bdd98(); return register_extra(0x6bdd5a, dwa(0x6bdd50), c); }   // 0x48dc50
DWORD __cdecl StaticInit_ExtraEntry_6be7cc() { const DWORD c = reg_6be7d4(); return register_extra(0x6be7cc, dwa(0x6be38c), c); }   // 0x4a86f0
DWORD __cdecl StaticInit_ExtraEntry_6c29ac() { const DWORD c = reg_6c29cc(); return register_extra(0x6c29ac, dwa(0x6c29b8), c); }   // 0x4f95d0
DWORD __cdecl StaticInit_ExtraEntry_6c29b4() { const DWORD c = reg_6c29cc(); return register_extra(0x6c29b4, dwa(0x6c299c), c); }   // 0x4f9680
DWORD __cdecl StaticInit_ExtraEntry_6c29ec() { const DWORD c = reg_6c29f0(); return register_extra(0x6c29ec, dwa(0x6c29dc), c); }   // 0x4fca90
DWORD __cdecl StaticInit_ExtraEntry_6d00f9() { const DWORD c = reg_6d0128(); return register_extra(0x6d00f9, dwa(0x6d00f4), c); }   // 0x521d20
DWORD __cdecl StaticInit_ExtraEntry_6d0120() { const DWORD c = reg_6d0128(); return register_extra(0x6d0120, dwa(0x6d00f0), c); }   // 0x521d90
DWORD __cdecl StaticInit_ExtraEntry_6d0101() { const DWORD c = reg_6d0128(); return register_extra(0x6d0101, dwa(0x6d00fc), c); }   // 0x521e00
DWORD __cdecl StaticInit_ExtraEntry_71437a() { const DWORD c = reg_7143c8(); return register_extra(0x71437a, dwa(0x7143ac), c); }   // 0x5b4590

// ---- Static initializers: object field descriptors -------------------------------------------------------------

// 0x4a8660: "Palette" (CPaletteHolder, kind 3) into g_PaletteHolderField 0x6be38c
DWORD __cdecl StaticInit_PaletteHolderField() { return dwa(0x6be38c) = object_field(0x613b80, &reg_6be7d0, 3); }
// 0x4d4410: "Hue Remap" (CHueRemapHolder, kind 1) into 0x6c0520
DWORD __cdecl StaticInit_HueRemapField() { return dwa(0x6c0520) = object_field(0x615de4, &reg_6c0528, 1); }
// 0x4fca00: "SubAI" (CAIBase, kind 3) into 0x6c29dc
DWORD __cdecl StaticInit_SubAIField() { return dwa(0x6c29dc) = object_field(0x618be8, &reg_6c29f0, 3); }

// ---- Static initializers: a descriptor's class ----------------------------------------------------------------

// The third registration's exits: the descriptor's +0x2c = the class; eax the descriptor when the registration
// ran, else the class
DWORD set_desc_class(GlobKey desc_at, DWORD cls, bool ran)
{
    const DWORD d = dwa(desc_at);
    dwo(d, 0x2c) = cls;
    return ran ? d : cls;
}
// 0x4f96f0 (through CallMember on 0x6c29a4): CLayerPart, CChildList, then g_ChildFieldDesc 0x6c29c4's class = CLayerPart
DWORD THISCALL StaticInit_ChildFieldClass(void *)
{
    reg_6c29cc();
    reg_6c29c8();
    bool ran;
    const DWORD c = reg_6c29cc_ran(ran);
    return set_desc_class(0x6c29c4, c, ran);
}
// 0x560660 (through CallMember on 0x713144): CCustomRules, CCustomRulesList, then 0x713148's class = CCustomRules
DWORD THISCALL StaticInit_CustomRulesListClass(void *)
{
    reg_713150();
    reg_71314c();
    bool ran;
    const DWORD c = reg_713150_ran(ran);
    return set_desc_class(0x713148, c, ran);
}

// ---- The action wrappers' Describe ------------------------------------------------------------------------------

// 0x41f880 (COtherMapWrapper, vtable 0x5ed7f4, slot 1): every action that is not a COtherMapAction
DWORD THISCALL OtherMapWrapper_Describe(void *, DWORD action, DWORD *cls)
{
    if (!is_kind_of(action, action_class()))
        return 0;
    if (is_kind_of(action, reg_62e934()))
        return 0;
    if (cls)
        *cls = reg_62e934();
    return describe(action, 0x60b1a0);   // "OtherMap %s action"
}

// 0x41fa50 (CThisMapWrapper, vtable 0x5ed800, slot 1): a COtherMapAction; *cls its action's class (+8) or
// CAction; "This Map Action" without one
DWORD THISCALL ThisMapWrapper_Describe(void *, DWORD action, DWORD *cls)
{
    if (!is_kind_of(action, reg_62e934()))
        return 0;
    if (cls) {
        const DWORD inner = dwo(action, 8);
        *cls = inner ? slot2(inner) : action_class();
    }
    if (!dwo(action, 8))
        return glob_addr(0x60b1b4);   // "This Map Action"
    return describe(dwo(action, 8), 0x60b1c4);   // "This Map %s action"
}

// 0x4b8300 (CSingleActionWrapper, vtable 0x5f52ac, slot 1): a CMultipleActionsAction with at most one action
// (+0x10 the count, +0xc the array); *cls that action's class or CAction; "Single Action" without one
DWORD THISCALL SingleActionWrapper_Describe(void *, DWORD action, DWORD *cls)
{
    if (!is_kind_of(action, reg_6bfd30()))
        return 0;
    if (dwo(action, 0x10) > 1)
        return 0;
    if (cls) {
        DWORD c;
        if (dwo(action, 0x10) && dwo(dwo(action, 0xc), 0))
            c = slot2(dwo(dwo(action, 0xc), 0));
        else
            c = action_class();
        *cls = c;
    }
    if (!dwo(action, 0x10) || !dwo(dwo(action, 0xc), 0))
        return glob_addr(0x6143f4);   // "Single Action"
    return describe(dwo(dwo(action, 0xc), 0), 0x614404);   // "Single %s action"
}

// 0x4c80f0 (vtable 0x5f52b8, slot 1): a CMultipleActionsAction of a subclass (its class (slot 2) not
// CMultipleActionsAction itself); *cls CMultipleActionsAction; the wrapper's text at +8 or the class's short name
DWORD THISCALL MultipleActionsBaseWrapper_Describe(BYTE *self, DWORD action, DWORD *cls)
{
    if (!is_kind_of(action, reg_6bfd30()))
        return 0;
    reg_6bfd30();
    const DWORD c = dwa(0x6bfd30);
    if (slot2(action) == c)
        return 0;
    if (cls)
        *cls = reg_6bfd30();
    if (const DWORD text = *reinterpret_cast<DWORD *>(self + 8))
        return text;
    return short_name(reg_6bfd30());
}

// ---- Definition list slots ---------------------------------------------------------------------------------------

// The index of the record (0x80 bytes, its object at +0x78) whose object is obj, or -1
inline DWORD record_of(DWORD records, DWORD count, DWORD obj)
{
    for (DWORD i = 0; i < count; i++)
        if (obj == dwo(records, 0x78 + i * 0x80))
            return i;
    return 0xffffffffu;
}

// 0x4fe160 (the CLevelOfDifficulty definition lists' vtables 0x5f9678/0x5f971c, slot 2): for a
// CLevelOfDifficulty in the list (records +0x30, count +0x34) the list's slot 39 (index, value); al 1, else 0
BYTE THISCALL LevelOfDifficultyList_SetObjectValue(BYTE *self, DWORD obj, DWORD value)
{
    if (!is_kind_of(obj, reg_6c2b48()))
        return 0;
    const DWORD i = record_of(dwo((DWORD)(uintptr_t)self, 0x30), dwo((DWORD)(uintptr_t)self, 0x34), obj);
    if (i == 0xffffffffu)
        return 0;
    vcall<DWORD>(self, 39, i, value);
    return 1;
}

// 0x598280 (a callback of the static list 0x62e6a8, added by CWaterWarpEffectFrame::RegisterFields; fastcall
// object, ZString *out): for a CWaterWarpEffect in the water warp list (0x713c80: records 0x713cb0, count
// 0x713cb4, folder 0x713cb8) out = "<folder>/<record name>.txt"; the record's object is loaded first when the
// record has none (the definition list's slots 1 and 37, as list_object in ai_helpers.h). al 1, else 0.
BYTE __fastcall WaterWarpEffect_FilePath(DWORD obj, ZString *out)
{
    constexpr DWORD LIST = 0x713c80, RECORDS = 0x713cb0, COUNT = 0x713cb4, FOLDER = 0x713cb8;
    if (!is_kind_of(obj, reg_713dc8()))
        return 0;
    const DWORD count = dwa(COUNT);
    DWORD records = dwa(RECORDS);
    const DWORD i = record_of(records, count, obj);
    if (i == 0xffffffffu)
        return 0;
    DWORD record = 0;
    if (records) {
        const DWORD k = i < count ? i : 0;
        const DWORD off = k << 7;
        if (!byo(records, off + 0x7f) && !dwo(records, off + 0x78) && !byo(records, off + 0x7f)) {
            byo(records, off + 0x7f) = 1;
            char path[0x104];
            fn<DWORD(__fastcall *)(char *, DWORD, DWORD)>(0x4a4220)(path, glob_addr(FOLDER), vcall<DWORD>(glob<void *>(LIST), 1, k));
            char *name = fn<char *(__fastcall *)(char *)>(0x4a4570)(path);
            DWORD o;
            if (name == path) {
                o = vcall<DWORD>(glob<void *>(LIST), 37, glob_addr(0x62e558) /* "" */, name, dwa(RECORDS) + off + 0x78);
            } else {
                name[-1] = 0;
                o = vcall<DWORD>(glob<void *>(LIST), 37, path, name, dwa(RECORDS) + off + 0x78);
            }
            dwo(dwa(RECORDS), off + 0x78) = o;
            byo(dwa(RECORDS), off + 0x7f) = 0;
            records = dwa(RECORDS);
        }
        record = records + (i << 7);
    }
    fn<DWORD(THISCALL *)(ZString *, DWORD, DWORD, DWORD)>(0x4e02c0)(out, glob_addr(FOLDER), record, glob_addr(0x60f62c) /* ".txt" */);
    return 1;
}

// ---- CZaxApp's custom rules display -------------------------------------------------------------------------------

// The world's (g_App 0x713f14 +0x2e0) rules (+0x38) if they are CZaxCustomRules, else 0; their display object
// (+0x1c) made on first use (an allocation failure crashes in the exe too: slot 19 through a null object)
DWORD rules_display()
{
    const DWORD world = dwo(dwa(0x713f14), 0x2e0);
    DWORD rules = 0;
    if (dwo(world, 0x38)) {
        reg_713f24();
        if (is_kind_of(dwo(world, 0x38), dwa(0x713f24)))
            rules = dwo(world, 0x38);
    }
    if (!dwo(rules, 0x1c)) {
        DWORD p = (DWORD)(uintptr_t)fn<void *(__fastcall *)(DWORD)>(0x417710)(0x60);
        if (p) {
            fn<void *(THISCALL *)(void *)>(0x416610)(at<void *>(p));        // CObject_Ctor
            fn<void *(THISCALL *)(void *)>(0x53f000)(at<void *>(p + 0x28)); // FieldColor_Ctor
            fn<void *(THISCALL *)(void *)>(0x53f000)(at<void *>(p + 0x38));
            fn<void *(THISCALL *)(void *)>(0x53f000)(at<void *>(p + 0x48));
            dwo(p, 0) = vt(0x5f9e98);
            fn<void(THISCALL *)(void *)>(0x4173c0)(at<void *>(p));          // Object_InitFields
        }
        dwo(rules, 0x1c) = p;
        vcall<void>(at<void *>(p), 19);
    }
    return dwo(rules, 0x1c);
}

// 0x59fca0 (slot 99): the two dwords at +0x10 into out; eax out
DWORD *THISCALL ZaxApp_RulesPair(void *, DWORD *out)
{
    const DWORD d = rules_display();
    out[0] = dwo(d, 0x10);
    out[1] = dwo(d, 0x14);
    return out;
}
// 0x59fd80 (slot 100): the float at +0x24 (on the FPU stack)
float THISCALL ZaxApp_RulesFloat(void *)
{
    const DWORD d = rules_display();
    return *at<float *>(d + 0x24);
}
// 0x59fe70 (slot 102): the FieldColor at +0x28 into out (bytes 0-6, 8-0xa, 0xc-0xf: 7 and 0xb stay); eax the
// last dword copied
DWORD THISCALL ZaxApp_RulesColor(void *, BYTE *out)
{
    const BYTE *c = at<BYTE *>(rules_display() + 0x28);
    *reinterpret_cast<DWORD *>(out) = *reinterpret_cast<const DWORD *>(c);
    *reinterpret_cast<WORD *>(out + 4) = *reinterpret_cast<const WORD *>(c + 4);
    out[6] = c[6];
    *reinterpret_cast<WORD *>(out + 8) = *reinterpret_cast<const WORD *>(c + 8);
    out[0xa] = c[0xa];
    const DWORD last = *reinterpret_cast<const DWORD *>(c + 0xc);
    *reinterpret_cast<DWORD *>(out + 0xc) = last;
    return last;
}

}  // namespace

ZAX_HOOKS(classreg_slots)
{
    const Hook list[] = {
#include "classreg_slots_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
