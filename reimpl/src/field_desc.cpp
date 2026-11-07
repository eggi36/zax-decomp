// CFieldDesc reimplemented (see field_desc.h).
#include "field_desc.h"

namespace zax {

// 0x418eb0. "Group/Name" splits at the first '/'. The full name is assigned first and replaced
// afterwards, like in the original (same allocations).
CFieldDesc *THISCALL CFieldDesc::Ctor(const char *name_, const char *units_, DWORD offset_, DWORD unknown14_,
                                      DWORD unknown18_)
{
    name.Ctor();
    help.Ctor();
    group.Ctor();
    alt_name.Ctor();
    units.Ctor();
    vftable = vt(VTABLE_CFIELDDESC);
    units.AssignText(units_);
    name.AssignText(name_);
    group.AssignText(nullptr);
    const char *slash = name_;
    while (*slash && *slash != '/')
        slash++;
    if (*slash) {
        group.AssignTextN(name_, (DWORD)(slash - name_));
        name.AssignText(slash + 1);
    }
    unknown14 = unknown14_;
    offset = offset_;
    unknown18 = unknown18_;
    return this;
}

// 0x418e10
CFieldDesc *THISCALL CFieldDesc::CopyCtor(const CFieldDesc *other)
{
    name.Ctor();
    help.Ctor();
    group.Ctor();
    alt_name.Ctor();
    units.Ctor();
    vftable = vt(VTABLE_CFIELDDESC);
    name.Assign(&other->name);
    help.Assign(&other->help);
    group.Assign(&other->group);
    units.Assign(&other->units);
    alt_name.Assign(&other->alt_name);
    offset = other->offset;
    unknown14 = other->unknown14;
    unknown18 = other->unknown18;
    inherited = other->inherited;
    return this;
}

// 0x418f60
void THISCALL CFieldDesc::Dtor()
{
    vftable = vt(VTABLE_CFIELDDESC);
    units.Dtor();
    alt_name.Dtor();
    group.Dtor();
    help.Dtor();
    name.Dtor();
}

// 0x418dc0
CFieldDesc *THISCALL CFieldDesc::DeletingDtor(DWORD flags)
{
    Dtor();
    if (flags & 1)
        heap_free(this);
    return this;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(field_desc)
{
    const Hook list[] = {
        {0x418eb0, ptr(&CFieldDesc::Ctor), "CFieldDesc::Ctor", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x8d, 0x5e}},
        {0x418e10, ptr(&CFieldDesc::CopyCtor), "CFieldDesc::CopyCtor", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x8d, 0x5e}},
        {0x418f60, ptr(&CFieldDesc::Dtor), "CFieldDesc::Dtor", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x20, 0xc7, 0x06}},
        {0x418dc0, ptr(&CFieldDesc::DeletingDtor), "CFieldDesc::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x20, 0xc7, 0x06}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
