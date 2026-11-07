// The sequence lists' shared slots (phase 4, rendering): the animated texture list 0x6c1ee8 (vtable 0x5f8e48)
// and the other image sequence lists (vtables 0x5ff6d4, 0x5ff778 share the slots below), 0x4ef000-0x4f6ae0.
// functions.csv names them after CAmbientSound's vtable (vf23, vf57, vf58, vf64); in 0x5f8e48 they are the
// slots 0, 34, 35 and 41.
//
// [confirmed: code; VM] A list's records (+0x30, count +0x34) are 0x80 bytes: the name inline at +0, +0x78 the
// loaded object, +0x7f set while it loads; +0x38 the folder (inline text).
//  - Slot 0 0x4ef170: SequenceList_Dtor 0x56c1a0, then with flag 1 the pool block (0x158 bytes) freed; eax the
//    object.
//  - Slot 35 0x4f6ae0 (object, argument): for a CSequence (Object_IsKindOf; no null test) found among the
//    records' objects slot 39 (index, argument) and al 1, otherwise al 0.
//  - Slot 41 0x4ef000 (object, ZString out): for a non-null CSequence found among the records' objects the
//    path of its Properties.txt: slot 5's text + "/-", the record's object loaded first when missing and not
//    loading (slot 1 (index) the name, joined with the folder (Path_Join 0x4a4220), the folder part cut
//    behind PathName 0x4a4570 and passed to slot 37 (folder or "", name, the object's slot)), then the
//    record's name and "/Properties.txt"; al 1. Without records the name appended is null (nothing).
//  - Slot 34 0x4f6ad0 is a jump to CSequence::ClassInfo (a relative jump in its first five bytes: not
//    hookable).
// Quirks kept [confirmed: code]: slot 41 loads the record at the index clamped to the count but appends the
// name of the unclamped one (the same: the index comes from the search); the loading flag is tested twice.
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

bool is_sequence(void *o) { return fn<bool(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, fn<CClassInfo *(__cdecl *)()>(0x56ab50)()); }
void zs_append(void *z, const char *s) { fn<void *(THISCALL *)(void *, const char *)>(0x4e1be0)(z, s); }

}  // namespace

// 0x4ef170 (slot 0)
BYTE *THISCALL SequenceList_DeletingDtor(BYTE *self, BYTE flags)
{
    fn<void(THISCALL *)(void *)>(0x56c1a0)(self);
    if (flags & 1)
        pool_free(self, 0x158);
    return self;
}

// 0x4f6ae0 (slot 35)
bool THISCALL SequenceList_ForObject(BYTE *self, void *obj, DWORD arg)
{
    if (!is_sequence(obj))
        return false;
    const DWORD n = dw(self, 0x34);
    if (!n)
        return false;
    const DWORD *p = reinterpret_cast<const DWORD *>(bp(dw(self, 0x30)) + 0x78);
    DWORD i = 0;
    while (ptr_value(obj) != *p) {
        i++;
        p += 0x20;
        if (i >= n)
            return false;
    }
    if (i == 0xffffffff)
        return false;
    vcall<DWORD>(self, 39, i, arg);
    return true;
}

// 0x4ef000 (slot 41)
bool THISCALL SequenceList_PropertiesPath(BYTE *self, void *obj, void *out)
{
    if (!is_sequence(obj) || !obj)
        return false;
    const DWORD n = dw(self, 0x34);
    if (!n)
        return false;
    const DWORD *p = reinterpret_cast<const DWORD *>(bp(dw(self, 0x30)) + 0x78);
    DWORD i = 0;
    while (ptr_value(obj) != *p) {
        i++;
        p += 0x20;
        if (i >= n)
            return false;
    }
    if (i == 0xffffffff)
        return false;
    zs_assign_text(out, vcall<const char *>(self, 5));
    zs_append(out, glob<const char *>(0x61804c) /* "/-" */);
    const char *name = nullptr;
    if (BYTE *recs = bp(dw(self, 0x30))) {
        const DWORD k = i < dw(self, 0x34) ? i : 0;
        const DWORD off = k << 7;
        BYTE *flag = recs + off + 0x7f;
        if (!*flag && !dw(recs, off + 0x78) && !*flag) {
            *flag = 1;
            const char *nm = vcall<const char *>(self, 1, k);
            char buf[0x100];
            fn<void(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(buf, reinterpret_cast<const char *>(self + 0x38), nm);
            char *pn = const_cast<char *>(path_name(buf));
            DWORD r;
            if (pn == buf) {
                r = vcall<DWORD>(self, 37, glob<const char *>(0x62e558), pn, bp(dw(self, 0x30)) + off + 0x78);
            } else {
                pn[-1] = 0;
                r = vcall<DWORD>(self, 37, buf, pn, bp(dw(self, 0x30)) + off + 0x78);
            }
            dw(bp(dw(self, 0x30)), off + 0x78) = r;
            bp(dw(self, 0x30))[off + 0x7f] = 0;
        }
        name = reinterpret_cast<const char *>(bp(dw(self, 0x30)) + (i << 7));
    }
    zs_append(out, name);
    zs_append(out, glob<const char *>(0x61803c) /* "/Properties.txt" */);
    return true;
}

ZAX_HOOKS(sequence_list_rest4b)
{
    const Hook list[] = {
#include "sequence_list_rest4b_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
