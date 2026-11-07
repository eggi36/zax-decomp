// Band D of the last phase 3 round, Objects and the layer's network block (0x4ee750-0x4efa10;
// docs/DECOMPILATION.md section 8, "Phase 3 remains, band D"): the player list holder CCharacterListHolder
// (registered as "CCharacterListHolder", editor name "CPlayerListHolder"; the application's +0x284 is one)
// with its class functions and its list functions, and the registration and write of the layer's block
// reader "CLayerBlockReader".
//
// Layout [confirmed: Create, RegisterFields, the functions below]:
//   CCharacterListHolder (0x18, vtable 0x5f8ef0, class 0x6c28ec, version 1, no parent): +8 "Player" (a
//     pointer array (vtable 0x5f8f4c, heap flavour: data +0xc, count +0x10, capacity +0x14; the destructor
//     writes the base vtable 0x5f8f58) of CEntityAnimated references). The holder listens to every part in
//     the list (LayerPart_AddListener 0x4fc1b0 / RemoveListener 0x4fc200 with the holder, -1 and the empty
//     text), and the part's "Delete" message (slot 0) takes it out of the list.
//
// Return values are what the original leaves in eax; where it is a register the original does not set on
// that path (the remove functions), the caller ignores it [confirmed: callers] and the reimplementation
// returns what its own last call left.
#include "class_info.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD HOLDER_VTABLE = 0x5f8ef0, HOLDER_VAR = 0x6c28ec, ARRAY_VTABLE = 0x5f8f4c, ARRAY_BASE_VTABLE = 0x5f8f58;
constexpr DWORD TEXT_EMPTY = 0x62e558, TEXT_DELETE = 0x60aef8, TEXT_PLAYER = 0x61807c;
constexpr DWORD TEXT_CLASS = 0x618050, TEXT_EDITOR_NAME = 0x618068;   // "CCharacterListHolder", "CPlayerListHolder"
constexpr DWORD LAYER_READER = 0x6c1cd0, TEXT_LAYER_READER = 0x618020;   // the static reader, "CLayerBlockReader"
constexpr DWORD G_RECTS_TOO = 0x6c1d08;                                  // also the parts' rectangles (byte)

DWORD remove_listener(void *part, void *listener)
{
    return fn<DWORD(THISCALL *)(void *, void *, int, const char *)>(0x4fc200)(part, listener, -1, glob<const char *>(TEXT_EMPTY));
}
DWORD add_listener(void *part, void *listener)
{
    return fn<DWORD(THISCALL *)(void *, void *, int, const char *)>(0x4fc1b0)(part, listener, -1, glob<const char *>(TEXT_EMPTY));
}
void *mem_move(void *d, const void *s, DWORD n) { return fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }

// The holder's list (the pointer array at +8)
struct List {
    DWORD vftable;
    void **data;
    DWORD count, capacity;
};
List &list_of(BYTE *holder) { return *reinterpret_cast<List *>(holder + 8); }

// Entry i taken out of the array (the rest moved down), as the inlined RemoveAt of the exe does
void list_remove(List &l, DWORD i)
{
    if (i >= l.count)
        return;
    if (i + 1 < l.count)
        mem_move(&l.data[i], &l.data[i + 1], (l.count - i - 1) * 4);
    l.count--;
}

// ---- Class functions ------------------------------------------------------------------------------------------

// 0x4ef6c0 (slot 2): the class getter (version 1, no parent)
CClassInfo *__cdecl Holder_ClassInfo()
{
    return class_getter(HOLDER_VAR, TEXT_CLASS, 1.0f, nullptr, 0x18, 0, 0x4ef440, 0x4ef490, 0x4ef720);
}
void holder_construct(BYTE *o)
{
    obj_ctor(o);
    List &l = list_of(o);
    l.data = nullptr;
    l.count = 0;
    l.capacity = 0;
    l.vftable = vt(ARRAY_VTABLE);
    vtable_and_defaults(o, vt(HOLDER_VTABLE));
}
// Every part let go from the last one down, then the array and the object
void holder_destroy(BYTE *o)
{
    dw(o, 0) = vt(HOLDER_VTABLE);
    List &l = list_of(o);
    for (int i = (int)l.count - 1; i >= 0; i--) {
        remove_listener(l.data[i], o);
        list_remove(l, (DWORD)i);
    }
    obj_free_fields(o);
    l.vftable = vt(ARRAY_BASE_VTABLE);
    if (l.data) {
        heap_free(l.data);
        l.data = nullptr;
        l.capacity = 0;
    }
    l.count = 0;
    obj_dtor(o);
}
// 0x4ef440, 0x4ef490, 0x4ef510 (slot 1)
void *__cdecl Holder_Create() { return create_one<0x18, holder_construct>(); }
void *__fastcall Holder_CreateArray(int count, int, int) { return create_array<0x18, holder_construct>(count); }
void *THISCALL Holder_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, holder_destroy>(self, flags); }

// 0x4ef720: +8 "Player" (an array of CEntityAnimated references, its element descriptor at offset 0);
// then the editor name "CPlayerListHolder" at +8 of the class info. eax: the class info.
CClassInfo *__cdecl Holder_RegisterFields()
{
    void *element = heap_alloc(0x30);
    if (element)
        element = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(OBJECT_REF_FIELD_CTOR)(
            element, glob<const char *>(TEXT_PLAYER), 0, 4, 0, entity_animated_class(), 0, 0);
    void *f = heap_alloc(0x48);
    if (f)
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, void *, DWORD,
                                  DWORD)>(0x419230)(f, glob<const char *>(TEXT_PLAYER), 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc, 4, 0xc, 0, element, 0, 0);
    add_field(Holder_ClassInfo(), f);
    CClassInfo *c = Holder_ClassInfo();
    dw(c, 8) = glob_addr(TEXT_EDITOR_NAME);
    return c;
}

// ---- The list --------------------------------------------------------------------------------------------------

struct Holder {
    // 0x4ef840 (slot 0, a message: part, name): "Delete" from a part in the list takes it out (eax: the new
    // count); everything else goes to CObject's slot 0
    DWORD THISCALL OnMessage(void *part, const char *name)
    {
        BYTE *self = reinterpret_cast<BYTE *>(this);
        List &l = list_of(self);
        if (crt_stricmp(name, glob<const char *>(TEXT_DELETE)) == 0)
            for (DWORD i = 0; i < l.count; i++)
                if (l.data[i] == part) {
                    const DWORD r = remove_listener(l.data[i], self);
                    if (i >= l.count)
                        return r;
                    list_remove(l, i);
                    return l.count;
                }
        return obj_on_message(self, part, (DWORD)(uintptr_t)name);
    }
    // 0x4ef8e0: the address of entry i
    void **THISCALL Slot(DWORD i) { return &list_of(reinterpret_cast<BYTE *>(this)).data[i]; }
    // 0x4ef900 (i, part): entry i set, the old part let go and the new one listened to
    DWORD THISCALL Set(DWORD i, void *part)
    {
        BYTE *self = reinterpret_cast<BYTE *>(this);
        List &l = list_of(self);
        DWORD r = (DWORD)(uintptr_t)l.data;
        if (l.data[i])
            r = remove_listener(l.data[i], self);
        if (part)
            r = add_listener(part, self);
        list_of(self).data[i] = part;
        return r;
    }
    // 0x4ef950 (i): entry i let go and taken out
    void THISCALL RemoveAt(DWORD i)
    {
        BYTE *self = reinterpret_cast<BYTE *>(this);
        List &l = list_of(self);
        if (i >= l.count)
            return;
        if (l.data[i])
            remove_listener(l.data[i], self);
        list_remove(l, i);
    }
    // 0x4ef9b0 (part): appended (a part, not null, is listened to); eax: the array's append (slot 2)
    DWORD THISCALL Append(void *part)
    {
        BYTE *self = reinterpret_cast<BYTE *>(this);
        if (part)
            add_listener(part, self);
        return vcall<DWORD>(&list_of(self), 2, &part);
    }
    // 0x4ef9e0 (part): its index, -1 if it is not in the list (the original's test of the entry before
    // the data with a count above 0xffffffff never runs)
    DWORD THISCALL IndexOf(void *part)
    {
        List &l = list_of(reinterpret_cast<BYTE *>(this));
        for (DWORD i = 0; i < l.count; i++)
            if (l.data[i] == part)
                return i;
        return 0xffffffffu;
    }
};

// ---- The layer's block -------------------------------------------------------------------------------------

// 0x4ee750 (on the server's reader registry): the static reader 0x6c1cd0 registered as "CLayerBlockReader"
DWORD THISCALL LayerBlock_Register(void *registry)
{
    return fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x51ff30)(registry, glob<void *>(LAYER_READER), glob<const char *>(TEXT_LAYER_READER));
}

// 0x4ee760 (on a layer: packet, a2, rectangle): the block of type 2 of the layer reader with a length dword
// patched at the end, the rectangle's four values as words and, when the byte 0x6c1d08 is set, the parts in
// it (CLayerSaveData::WriteRect on the save data +0x3c). eax: PatchDword's.
struct LayerBlock {
    DWORD THISCALL Write(void *packet, DWORD a2, const int *rect)
    {
        fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x51ff50)(packet, glob<void *>(LAYER_READER), 2);
        const DWORD start = fn<DWORD(THISCALL *)(void *, DWORD)>(0x520940)(packet, 0);
        for (int k = 0; k < 4; k++)
            fn<DWORD(THISCALL *)(void *, WORD)>(0x5209c0)(packet, (WORD)rect[k]);
        if (*glob<BYTE *>(G_RECTS_TOO))
            fn<DWORD(THISCALL *)(void *, void *, DWORD, const int *)>(0x4e79b0)(reinterpret_cast<BYTE *>(this) + 0x3c, packet, a2, rect);
        const DWORD length = fn<DWORD(THISCALL *)(void *)>(0x5203b0)(packet) - start - 4;
        return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x520f10)(packet, length, start);
    }
};

}  // namespace

ZAX_HOOKS(objects_rest_d)
{
    const Hook list[] = {
#include "objects_rest_d_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
