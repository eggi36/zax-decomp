// CFieldDesc: descriptor of one field of a class (name, offset in the object, help text, units).
// Every class keeps a list of them (CClassInfo::fields); loading, saving and the editor go through
// them. The base class is abstract (vtable 0x5eccb0, 24 slots, several pure virtual); every field
// kind is a subclass with its own constructor (zax-formats/zaxtools/exeschema.py lists them).
//
// Known virtual methods: slot 0 scalar deleting destructor, slot 1 clone (copy for derived classes),
// slot 16 value as text (object, ZString *out), slot 23 write the value as text (0x418fe0, uses 16).
// The extra lists of the class infos hold descriptors too (0x419040 sets the flag at +0x24 when an
// entry is passed on to a subclass).
#pragma once
#include "exe_base.h"

namespace zax {

struct CFieldDesc {
    DWORD vftable;       // 0x00
    ZString name;        // 0x04 without the group prefix
    ZString help;        // 0x08 help text (0x419020)
    ZString group;       // 0x0c part of the name before '/', otherwise empty
    DWORD offset;        // 0x10 offset of the value in the object
    DWORD unknown14;     // 0x14
    DWORD unknown18;     // 0x18
    ZString alt_name;    // 0x1c alternative (old) name, searched by FindField (0x419030, 0x419050)
    ZString units;       // 0x20 unit for the editor, e.g. "Seconds", "Degrees" (0x419060)
    BYTE inherited;      // 0x24 set when passed on through an extra list (0x419040), not by the constructor
    BYTE pad25[3];

    CFieldDesc *clone() const { return vcall<CFieldDesc *>(this, 1); }

    CFieldDesc *THISCALL Ctor(const char *name, const char *units, DWORD offset, DWORD unknown14,
                              DWORD unknown18);                    // 0x418eb0
    CFieldDesc *THISCALL CopyCtor(const CFieldDesc *other);       // 0x418e10
    void THISCALL Dtor();                                         // 0x418f60
    CFieldDesc *THISCALL DeletingDtor(DWORD flags);               // 0x418dc0 (vtable slot 0)
};
static_assert(sizeof(CFieldDesc) == 0x28);
static_assert(offsetof(CFieldDesc, offset) == 0x10 && offsetof(CFieldDesc, alt_name) == 0x1c);
static_assert(offsetof(CFieldDesc, inherited) == 0x24);

constexpr DWORD VTABLE_CFIELDDESC = 0x5eccb0;

}  // namespace zax
