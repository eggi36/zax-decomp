// Batch C4 (phase 5a), world: small slots and callbacks of the world's neighbours — the two converting
// wrappers' slot 2 (0x5ed7f4, 0x5ed800), the sound listener's float setter, the exit functions of three
// pool arrays and of the difficulty list, the "Top Hide" filter of World::RenderLayer, the part callback
// of CSortList2D_QueryRadius, CAction's slot 26 (0), the "AI" field's alternative name, the model's
// sequence name list (0x5faea4), the block reader base's destructor and the warp effect's block reader.
//
// Not hookable: 0x517810 (slot 34 of 0x5fad44: jmp CModel::ClassInfo, a relative jump).
//
// Return values are what the original leaves in eax (the exe functions called through their addresses).
#include "def_list_slots.h"
#include "exe_base.h"

// The body of 0x4e6c50 with C linkage, so the naked entry can call it by name (fastcall self, value)
extern "C" unsigned long __fastcall c4_listener_setter_impl(unsigned char *self, unsigned long v);

namespace zax {
namespace {

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }

// 0x41f9c0, slot 2 of 0x5ed7f4 (x): a new action of 0x10 bytes (CAction::Ctor, a ZString at +0xc, vtable
// 0x5ed710, InitFields), its slot 19, +8 = x. eax: the action. A failed allocation calls slot 19 on null.
BYTE *THISCALL ConvertWrapper5ed7f4_Make(void *, DWORD x)
{
    BYTE *a = static_cast<BYTE *>(pool_alloc(0x10));
    if (a) {
        fn<DWORD(THISCALL *)(void *)>(0x41a6b0)(a);
        fn<DWORD(THISCALL *)(void *)>(0x4dec90)(a + 0xc);
        dw(a, 0) = vt(0x5ed710);
        fn<DWORD(THISCALL *)(void *)>(0x4173c0)(a);
    }
    vcall<DWORD>(a, 19);
    dw(a, 8) = x;
    return a;
}
// 0x41fb50, slot 2 of 0x5ed800 (w): the object w holds at +8 taken out (0 written), w deleted (slot 1 with
// 1; w is read before the null test). eax: the object.
DWORD THISCALL ConvertWrapper5ed800_TakeObject(void *, BYTE *w)
{
    const DWORD r = dw(w, 8);
    dw(w, 8) = 0;
    if (w)
        vcall<DWORD>(w, 1, 1);
    return r;
}

// 0x4e6c50 (the setter of a float field, thiscall (value)): the resource cache's entry of the value made
// the listener of 0x713698, +0x9c = the value; returns the value on the FPU stack with eax from the
// listener's setter
__attribute__((naked)) void ListenerSetter()
{
    __asm__("movl 4(%esp), %edx\n\tcall \"@c4_listener_setter_impl@8\"\n\tflds 4(%esp)\n\tret $4");
}

// 0x4e88b0 / 0x4ea610 / 0x4f39a0: atexit of the pool arrays 0x6c1c50 (4-byte elements), 0x6c1bd8 (16)
// and 0x6c26f0 (8): the base vtable, the data back to the pool (capacity << SHIFT bytes), capacity and count 0.
// eax: the pool's free; without data the caller's (not reproduced: 0).
template <DWORD ARRAY, DWORD VTABLE, int SHIFT> DWORD __cdecl PoolArray_AtExit()
{
    BYTE *a = glob<BYTE *>(ARRAY);
    DWORD r = 0;
    dw(a, 0) = vt(VTABLE);
    if (pt(a, 4)) {
        r = fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x5065e0)(pool_get(), pt(a, 4), dw(a, 0xc) << SHIFT);
        dw(a, 4) = 0;
        dw(a, 0xc) = 0;
    }
    dw(a, 8) = 0;
    return r;
}
// 0x4fd650: atexit of the difficulty list 0x6c2a00 (base vtable 0x5f9698)
DWORD __cdecl DifficultyList_AtExit() { return deflist::destroy_list(glob<ZDefinitionList *>(0x6c2a00), 0x5f9698); }

// 0x4ed2b0 (thiscall part, pushed to World::RenderLayer): the function-local statics "Top Hide" (the index in
// the category list, guard bit 0 of 0x6c1cd8, 0x6c1bec) and its mask (bit 1, 0x6c1c44: 1 << index, 0 for
// -1); true for a character or a part in that category, false without a part
DWORD THISCALL RenderLayer_ShowPart(BYTE *part)
{
    BYTE &guard = *glob<BYTE *>(0x6c1cd8);
    DWORD index;
    if (!(guard & 1)) {
        guard |= 1;
        index = fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x523df0)(glob<void *>(0x6bd140), glob<const char *>(0x617f90), 0xffffffff);
        *glob<DWORD *>(0x6c1bec) = index;
    } else
        index = *glob<DWORD *>(0x6c1bec);
    if (!(guard & 2)) {
        guard |= 2;
        *glob<DWORD *>(0x6c1c44) = index == 0xffffffff ? 0 : 1u << (index & 31);
    }
    if (!part)
        return 0;
    if ((BYTE)fn<DWORD(THISCALL *)(void *, void *)>(0x416790)(part, fn<void *(__cdecl *)()>(0x49a930)()))
        return 1;
    return (*glob<DWORD *>(0x6c1c44) & dw(part, 0x1c)) ? 1 : 0;
}
// 0x4ed6d0 (fastcall part, a; one more stack argument): the part's slot 63 (a)
DWORD __fastcall QueryRadius_Callback(void *part, DWORD a, DWORD) { return vcall<DWORD>(part, 63, a); }

// 0x4fc960, CAction's slot 26 (five arguments): 0
DWORD THISCALL Action_Slot26(void *, DWORD, DWORD, DWORD, DWORD, DWORD) { return 0; }

// 0x516d70 (through CallMember): the "AI" field 0x6cfdcc gets the alternative name "AI" and is added to
// CAIBase's class
DWORD __cdecl AIField_Register()
{
    fn<DWORD(THISCALL *)(void *, const char *)>(0x419030)(*glob<void **>(0x6cfdcc), glob<const char *>(0x614e3c));
    void *cls = fn<void *(__cdecl *)()>(0x4fcb00)();
    return fn<DWORD(THISCALL *)(void *, void *)>(0x418fd0)(*glob<void **>(0x6cfdcc), cls);
}

// The model's sequence names (0x5faea4, 8 bytes: +4 the model): 0x518270 slot 0 (the named list's vtable,
// the pool), 0x5182a0 slot 1 (i: CModel::SequenceName, 0 beyond the count +0x48), 0x5182c0 slot 22 (the count)
BYTE *THISCALL SequenceNames_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(0x5ec7cc);
    if (flags & 1)
        pool_free(self, 8);
    return self;
}
DWORD THISCALL SequenceNames_Name(BYTE *self, DWORD i)
{
    BYTE *model = pt(self, 4);
    if (i >= dw(model, 0x48))
        return 0;
    return fn<DWORD(THISCALL *)(void *, DWORD)>(0x517200)(model, i);
}
DWORD THISCALL SequenceNames_Count(BYTE *self) { return dw(pt(self, 4), 0x48); }

// 0x5718b0, slot 0 of the block reader base (0x5f1dd0): the base vtable, operator delete
BYTE *THISCALL BlockReader_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(0x5f1dd0);
    if (flags & 1)
        fn<DWORD(__cdecl *)(void *)>(0x5d0320)(self);
    return self;
}
// 0x571840, slot 2 of 0x600490 (a block reader): a new warp effect of 0x30 bytes (CWarpEffect::Ctor, vtable
// 0x60049c, InitFields, +8 and +0x10..+0x28 0) reads itself (slot 24) and becomes pending (0x55e490). The
// object is used without a null test.
DWORD THISCALL WarpEffectReader_Read(void *, void *packet)
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x30));
    if (o) {
        fn<DWORD(THISCALL *)(void *)>(0x596490)(o);
        dw(o, 0) = vt(0x60049c);
        fn<DWORD(THISCALL *)(void *)>(0x4173c0)(o);
        dw(o, 0x10) = 0;
        dw(o, 0x14) = 0;
        dw(o, 0x18) = 0;
        dw(o, 0x1c) = 0;
        dw(o, 0x20) = 0;
        o[0x24] = 0;
        dw(o, 0x28) = 0;
        dw(o, 8) = 0;
    }
    vcall<DWORD>(o, 24, packet);
    return fn<DWORD(THISCALL *)(void *)>(0x55e490)(o);
}

}  // namespace

}  // namespace zax

extern "C" unsigned long __fastcall c4_listener_setter_impl(unsigned char *self, unsigned long v)
{
    using namespace zax;
    void *e = fn<void *(STDCALL *)(DWORD)>(0x57aef0)(v);
    const DWORD r = fn<DWORD(THISCALL *)(void *)>(0x57c210)(e);
    *reinterpret_cast<DWORD *>(self + 0x9c) = v;
    return r;
}

namespace zax {

ZAX_HOOKS(c4_world)
{
    const Hook list[] = {
#include "c4_world_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
