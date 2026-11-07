// The broadcaster, the listener references, the update monitor list, the child list and their
// helpers reimplemented (see broadcast.h).
#include "broadcast.h"
#include "field_kinds.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD,
                                            void *, void *, void *);
static void add_field(CClassInfo *c, CFieldDesc *f) { fn<void(THISCALL *)(CClassInfo *, CFieldDesc *)>(0x415700)(c, f); }
static void zstring_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static void zstring_dtor(ZString *z) { fn<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
static void zstring_assign(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
static void remove_at(ZPtrArray *a, DWORD i) { fn<void(THISCALL *)(ZPtrArray *, DWORD)>(0x586fc0)(a, i); }
static DWORD crt_memmove(void *d, const void *s, DWORD n) { return fn<DWORD(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }
static DWORD append(ZPtrArray *a, void *p) { return vcall<DWORD>(a, 2, &p); }
static DWORD delete_object(void *o) { return vcall<DWORD>(o, 1, 1); }

static DWORD broadcaster_add(CBroadcaster *b, CObjectBase *l, int offset, const char *name)
{
    return fn<DWORD(THISCALL *)(CBroadcaster *, CObjectBase *, int, const char *)>(0x467e60)(b, l, offset, name);
}
static DWORD broadcaster_remove(CBroadcaster *b, CObjectBase *l, int offset, const char *name)
{
    return fn<DWORD(THISCALL *)(CBroadcaster *, CObjectBase *, int, const char *)>(0x467ee0)(b, l, offset, name);
}
// The layer part's AddListener/RemoveListener (0x4fc1b0, 0x4fc200), called on the part
static DWORD part_add_listener(CObjectBase *part, CObjectBase *l)
{
    return fn<DWORD(THISCALL *)(CObjectBase *, CObjectBase *, int, const char *)>(0x4fc1b0)(part, l, -1, glob<const char *>(EMPTY_TEXT));
}
static DWORD part_remove_listener(CObjectBase *part, CObjectBase *l)
{
    return fn<DWORD(THISCALL *)(CObjectBase *, CObjectBase *, int, const char *)>(0x4fc200)(part, l, -1, glob<const char *>(EMPTY_TEXT));
}

// Releases the data of a pool pointer array the way the inlined array destructors do: new vtable,
// block back to the pool (capacity * 4 bytes), data and capacity cleared, count cleared in any case
static void free_array(ZPtrArray &a, VtKey vftable)
{
    a.vftable = key_vt(vftable);
    if (a.data) {
        pool_get_free(a.data, a.capacity * 4);
        a.data = nullptr;
        a.capacity = 0;
    }
    a.count = 0;
}

// ---- The shared pattern of the classes ---------------------------------------------------------------

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is called
// after the block was allocated). The class info gets the exe addresses of Create, CreateArray and
// RegisterFields, as in the original (they are hooked).
static CClassInfo *class_getter(GlobKey var, GlobKey name, float version, CClassInfo *(*parent)(), DWORD size, DWORD abstract,
                                void *create, void *create_array, void *register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), version, parent ? parent() : nullptr, size, abstract,
                                     create, create_array, register_fields);
    return v;
}

// CreateArray: heap block with the count in front, every element constructed (none for a count below 1)
template <typename T> static void *create_array(DWORD count)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc(count * sizeof(T) + 4));
    if (!block)
        return nullptr;
    block[0] = count;
    T *e = reinterpret_cast<T *>(block + 1);
    if ((int)(count - 1) >= 0)
        for (DWORD i = 0; i < count; i++)
            e[i].construct();
    return e;
}

// Deleting destructor (slot 1): bit 1 destroys the array from the last element and returns the
// block, bit 0 frees (array: operator delete, single object: the pool)
template <typename T> static void *deleting_dtor(T *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                self[i - 1].destroy();
        if (flags & 1)
            heap_free(block);
        return block;
    }
    self->destroy();
    if (flags & 1)
        pool_free(self, sizeof(T));
    return self;
}

// ---- CListenerReference ------------------------------------------------------------------------------

static void *__fastcall ListenerReference_CreateArray(int count, int, int) { return create_array<CListenerReference>((DWORD)count); }

// 0x467640 (slot 2)
static CClassInfo *__cdecl ListenerReference_ClassInfo()
{
    return class_getter(0x6bced8, 0x60f1c8 /* "CListenerReference" */, 0.0f, object_class_info, 0x14, 0,
                        code<void *>(0x4674f0), code<void *>(0x467530), code<void *>(0x4676a0));
}

void CListenerReference::construct()
{
    object_ctor(this);
    zstring_ctor(&member_name);
    vftable = vt(0x5ef248);
    object_init_fields(this);
}

void CListenerReference::destroy()
{
    vftable = vt(0x5ef248);
    object_free_fields(this);
    zstring_dtor(&member_name);
    object_dtor(this);
}

// 0x4674f0
static void *__cdecl ListenerReference_Create()
{
    auto *r = static_cast<CListenerReference *>(pool_alloc(sizeof(CListenerReference)));
    if (!r)
        return nullptr;
    r->construct();
    return r;
}

// 0x4675a0
void *THISCALL CListenerReference::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x4676a0: "Listener" (reference to any CObject), "Member Offset", "Member Name"
static void __cdecl ListenerReference_RegisterFields()
{
    CFieldDesc *f = nullptr;
    if (void *p = heap_alloc(0x30))
        f = fn<CFieldDesc *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(0x523e30)(
            p, glob<const char *>(0x60f1f8) /* "Listener" */, 8, 4, 8, object_class_info(), 3, 0);
    add_field(ListenerReference_ClassInfo(), f);
    f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<CFieldDesc *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54d6e0)(
            p, glob<const char *>(0x60f1e8) /* "Member Offset" */, 0xc, 4, 0xc, 3, 0, 0, 0, 0);
    add_field(ListenerReference_ClassInfo(), f);
    f = nullptr;
    if (void *p = heap_alloc(0x34))
        f = fn<CFieldDesc *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x415dd0)(
            p, glob<const char *>(0x60f1dc) /* "Member Name" */, 0x10, 4, 0x10, 3, 0, 0);
    add_field(ListenerReference_ClassInfo(), f);
}

// A new listener reference as AddListener and slot 18 make it (a failed allocation ends the game, as
// in the original): constructed, slot 19 (after-create of its fields), then the members
static CListenerReference *new_reference(CObjectBase *listener, int offset, const char *name)
{
    auto *r = static_cast<CListenerReference *>(pool_alloc(sizeof(CListenerReference)));
    if (r)
        r->construct();
    vcall<DWORD>(r, 19);
    r->listener = listener;
    r->member_offset = offset;
    zstring_assign(&r->member_name, name);
    return r;
}

// ---- CBroadcaster ------------------------------------------------------------------------------------

static void *__fastcall Broadcaster_CreateArray(int count, int, int) { return create_array<CBroadcaster>((DWORD)count); }

// 0x467830 (slot 2): no parent class
static CClassInfo *__cdecl Broadcaster_ClassInfo()
{
    return class_getter(0x6bcedc, 0x60f204 /* "CBroadcaster" */, 2.0f, nullptr, 0x28, 0, code<void *>(0x467880),
                        code<void *>(0x4678e0), code<void *>(0x467b40));
}

// 0x467d00
CBroadcaster *THISCALL CBroadcaster::Ctor()
{
    object_ctor(this);
    listening = {vt(0x5ef30c), nullptr, 0, 0};
    pending = {vt(0x5ef300), nullptr, 0, 0};
    vftable = vt(0x5ef2a4);
    object_init_fields(this);
    return this;
}

// 0x467880
static void *__cdecl Broadcaster_Create()
{
    auto *b = static_cast<CBroadcaster *>(pool_alloc(sizeof(CBroadcaster)));
    return b ? b->Ctor() : nullptr;
}

// The listener references are deleted from the last one (the count read again after each), then the
// fields freed and both arrays released
void CBroadcaster::destroy()
{
    vftable = vt(0x5ef2a4);
    while (listening.count) {
        const DWORD i = listening.count - 1;
        if (void *r = listening.data[i])
            delete_object(r);
        remove_at(&listening, i);
    }
    object_free_fields(this);
    free_array(pending, 0x5ef324);
    free_array(listening, 0x5ef318);
    object_dtor(this);
}

// 0x467970
void *THISCALL CBroadcaster::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x467b40: "Listener" (array of references to any CObject, +0x20/+0x24/+0x1c), "Objects Listening"
// (array of owned CListenerReference, +0x10/+0x14/+0xc)
static void __cdecl Broadcaster_RegisterFields()
{
    using ArrayCtor = CFieldDesc *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                               DWORD, DWORD, DWORD, CFieldDesc *, DWORD, DWORD);
    CFieldDesc *element = nullptr, *f = nullptr;
    if (void *p = heap_alloc(0x30))
        element = fn<CFieldDesc *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(0x523e30)(
            p, glob<const char *>(0x60f1f8) /* "Listener" */, 0, 4, 0, object_class_info(), 0, 0);
    if (void *p = heap_alloc(0x48))
        f = fn<ArrayCtor>(0x419230)(p, glob<const char *>(0x60f1f8), 0x20, 4, 0x20, 0x24, 4, 0x24, 0x1c, 4, 0x1c, 1, element, 0, 0);
    add_field(Broadcaster_ClassInfo(), f);
    element = nullptr;
    f = nullptr;
    if (void *p = heap_alloc(0x3c))
        element = fn<CFieldDesc *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD)>(
            0x4186f0)(p, glob<const char *>(0x60f228) /* "Listener Ref" */, 0, 4, 0, ListenerReference_ClassInfo(), 0, 0, 0, 0);
    if (void *p = heap_alloc(0x48))
        f = fn<ArrayCtor>(0x419230)(p, glob<const char *>(0x60f214) /* "Objects Listening" */, 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc,
                                    4, 0xc, 1, element, 3, 0);
    add_field(Broadcaster_ClassInfo(), f);
}

// 0x467d40: slot 0 of every listener (sender, message), from the last reference to the first. The
// index runs down from the count at the start; the data pointer is read again for every reference
// (listeners may remove themselves). eax: the last slot 0 result, the data pointer if the first
// reference has no listener, the caller's eax without references.
DWORD THISCALL CBroadcaster::Broadcast(CObjectBase *sender, const char *message)
{
    DWORD r = 0;
    for (int i = (int)(listening.count - 1); i >= 0; i--) {
        CObjectBase *l = static_cast<CListenerReference *>(listening.data[i])->listener;
        r = l ? vcall<DWORD>(l, 0, sender, message) : (DWORD)(uintptr_t)listening.data;
    }
    return r;
}

// 0x467d80 (slot 18): every listener object of the "Listener" array, from the last, becomes a listener
// reference (member offset -1, empty member name) appended to "Objects Listening"; returns 0
DWORD THISCALL CBroadcaster::ResolvePending(DWORD)
{
    while (pending.count) {
        CListenerReference *r = static_cast<CListenerReference *>(pool_alloc(sizeof(CListenerReference)));
        if (r)
            r->construct();
        vcall<DWORD>(r, 19);
        r->listener = static_cast<CObjectBase *>(pending.data[pending.count - 1]);
        r->member_offset = -1;
        zstring_assign(&r->member_name, glob<const char *>(EMPTY_TEXT));
        append(&listening, r);
        pending.count--;
    }
    return 0;
}

// 0x467e60: returns the index from the array's Add
DWORD THISCALL CBroadcaster::AddListener(CObjectBase *listener, int offset, const char *name)
{
    return append(&listening, new_reference(listener, offset, name));
}

// 0x467ee0: deletes the first reference to the listener (offset and name are not compared) and removes
// it. eax: 0 without references, the count if the listener is not found, otherwise memmove's result or
// the index behind the removed one.
DWORD THISCALL CBroadcaster::RemoveListener(CObjectBase *listener, int, const char *)
{
    const DWORD n = listening.count;
    if (!n)
        return 0;
    DWORD found = 0xffffffff;
    for (DWORD i = 0; i < n; i++)
        if (static_cast<CListenerReference *>(listening.data[i])->listener == listener) {
            found = i;
            break;
        }
    if (found == 0xffffffff)
        return n;
    DWORD r = (DWORD)(uintptr_t)listening.data;
    if (void *ref = listening.data[found])
        r = delete_object(ref);
    const DWORD c = listening.count;
    if (found >= c)
        return r;
    const DWORD next = found + 1;
    if (next < c)
        r = crt_memmove(&listening.data[found], &listening.data[next], (c - next) * 4);
    else
        r = next;
    listening.count--;
    return r;
}

// ---- Broadcaster of a layer part (+0x10) or an AI object (+0xc) --------------------------------------

// 0x4fc1b0 / 0x4fd190: the broadcaster is created on the first listener
template <DWORD OFF> DWORD THISCALL BroadcasterOwner::AddListener(CObjectBase *listener, int offset, const char *name)
{
    CBroadcaster *&b = broadcaster<OFF>();
    if (!b) {
        void *p = pool_alloc(sizeof(CBroadcaster));
        b = p ? fn<CBroadcaster *(THISCALL *)(void *)>(0x467d00)(p) : nullptr;
    }
    return broadcaster_add(b, listener, offset, name);
}

// 0x4fc200 / 0x4fd1e0: and deleted with the last one. eax: the remaining count, else the deleting
// destructor's result; the caller's eax without a broadcaster.
template <DWORD OFF> DWORD THISCALL BroadcasterOwner::RemoveListener(CObjectBase *listener, int offset, const char *name)
{
    if (!broadcaster<OFF>())
        return 0;
    broadcaster_remove(broadcaster<OFF>(), listener, offset, name);
    CBroadcaster *b = broadcaster<OFF>();
    if (const DWORD n = b->listening.count)
        return n;
    const DWORD r = delete_object(b);
    broadcaster<OFF>() = nullptr;
    return r;
}

// 0x4fc240: the layer part as sender. eax: the part without a broadcaster, else Broadcast's.
DWORD THISCALL BroadcasterOwner::PartBroadcast(const char *message)
{
    CBroadcaster *b = broadcaster<0x10>();
    if (!b)
        return (DWORD)(uintptr_t)this;
    return fn<DWORD(THISCALL *)(CBroadcaster *, void *, const char *)>(0x467d40)(b, this, message);
}

// ---- CUpdateMonitorInfo ------------------------------------------------------------------------------

static void *__fastcall UpdateMonitorInfo_CreateArray(int count, int, int) { return create_array<CUpdateMonitorInfo>((DWORD)count); }

// 0x4e4960 (slot 2)
static CClassInfo *__cdecl UpdateMonitorInfo_ClassInfo()
{
    return class_getter(0x6c1ce4, 0x617590 /* "CUpdateMonitorInfo" */, 1.0f, object_class_info, 0x14, 0,
                        code<void *>(0x4e4830), code<void *>(0x4e4860), code<void *>(0x4e49c0));
}

void CUpdateMonitorInfo::construct()
{
    object_ctor(this);
    vftable = vt(0x5f8a18);
    object_init_fields(this);
}

void CUpdateMonitorInfo::destroy()
{
    vftable = vt(0x5f8a18);
    if (part)
        part = nullptr;
    object_free_fields(this);
    object_dtor(this);
}

// 0x4e4830
static void *__cdecl UpdateMonitorInfo_Create()
{
    auto *m = static_cast<CUpdateMonitorInfo *>(pool_alloc(sizeof(CUpdateMonitorInfo)));
    if (!m)
        return nullptr;
    m->construct();
    return m;
}

// 0x4e48c0
void *THISCALL CUpdateMonitorInfo::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x4e49c0: "Update Part" (reference to a CLayerPart), "Half Width", "Half Height"
static void __cdecl UpdateMonitorInfo_RegisterFields()
{
    using EnumIntCtor = CFieldDesc *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    CFieldDesc *f = nullptr;
    if (void *p = heap_alloc(0x30))
        f = fn<CFieldDesc *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(0x523e30)(
            p, glob<const char *>(0x6175bc) /* "Update Part" */, 8, 4, 8, fn<CClassInfo *(__cdecl *)()>(0x4f9bf0)(), 3, 0);
    add_field(UpdateMonitorInfo_ClassInfo(), f);
    f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<EnumIntCtor>(0x54d6e0)(p, glob<const char *>(0x6175b0) /* "Half Width" */, 0xc, 4, 0xc, 3, 0, 0, 0, 0);
    add_field(UpdateMonitorInfo_ClassInfo(), f);
    f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<EnumIntCtor>(0x54d6e0)(p, glob<const char *>(0x6175a4) /* "Half Height" */, 0x10, 4, 0x10, 3, 0, 0, 0, 0);
    add_field(UpdateMonitorInfo_ClassInfo(), f);
}

// ---- CUpdateMonitorList ------------------------------------------------------------------------------

static void *__fastcall UpdateMonitorList_CreateArray(int count, int, int) { return create_array<CUpdateMonitorList>((DWORD)count); }

// 0x4e4dd0 (slot 2) and its copy 0x4e4b60
static CClassInfo *__cdecl UpdateMonitorList_ClassInfo()
{
    return class_getter(0x6c1ce8, 0x6175c8 /* "CUpdateMonitorList" */, 2.0f, object_class_info, 0x18, 0,
                        code<void *>(0x4e4bb0), code<void *>(0x4e4c00), code<void *>(0x4e4e30));
}

// 0x4e4f40
CUpdateMonitorList *THISCALL CUpdateMonitorList::Ctor()
{
    object_ctor(this);
    monitors = {vt(0x5f8ad0), nullptr, 0, 0};
    vftable = vt(0x5f8a74);
    object_init_fields(this);
    return this;
}

// 0x4e4bb0
static void *__cdecl UpdateMonitorList_Create()
{
    auto *l = static_cast<CUpdateMonitorList *>(pool_alloc(sizeof(CUpdateMonitorList)));
    return l ? l->Ctor() : nullptr;
}

// The list stops listening to every monitored part (the count read once, the data for every entry);
// the entries themselves are not deleted here, then the fields are freed and the array released
void CUpdateMonitorList::destroy()
{
    vftable = vt(0x5f8a74);
    const DWORD n = monitors.count;
    for (DWORD i = 0; i < n; i++)
        part_remove_listener(static_cast<CUpdateMonitorInfo *>(monitors.data[i])->part, this);
    object_free_fields(this);
    free_array(monitors, 0x5f8adc);
    object_dtor(this);
}

// 0x4e4c80
void *THISCALL CUpdateMonitorList::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x4e4e30: "Monitor List" (array of owned CUpdateMonitorInfo, +0x10/+0x14/+0xc)
static void __cdecl UpdateMonitorList_RegisterFields()
{
    CFieldDesc *element = nullptr, *f = nullptr;
    if (void *p = heap_alloc(0x3c))
        element = fn<CFieldDesc *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD)>(
            0x4186f0)(p, glob<const char *>(0x6175ec) /* "Update Monitor Item" */, 0, 4, 0, UpdateMonitorInfo_ClassInfo(), 3, 0, 0, 0);
    if (void *p = heap_alloc(0x48))
        f = fn<CFieldDesc *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                        DWORD, CFieldDesc *, DWORD, DWORD)>(0x419230)(
            p, glob<const char *>(0x6175dc) /* "Monitor List" */, 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc, 4, 0xc, 1, element, 3, 0);
    add_field(UpdateMonitorList_ClassInfo(), f);
}

// Removes entry i: the list stops listening to its part, the entry is deleted and taken out of the
// array (the count read again). eax: the new count, the deleting destructor's result if the count
// shrank below i meanwhile.
static DWORD remove_monitor(CUpdateMonitorList *l, DWORD i)
{
    DWORD r = part_remove_listener(static_cast<CUpdateMonitorInfo *>(l->monitors.data[i])->part, l);
    if (void *m = l->monitors.data[i])
        r = delete_object(m);
    const DWORD c = l->monitors.count;
    if (i >= c)
        return r;
    if (i + 1 < c)
        crt_memmove(&l->monitors.data[i], &l->monitors.data[i + 1], (c - i - 1) * 4);
    return --l->monitors.count;
}

static DWORD find_monitor(const CUpdateMonitorList *l, const CObjectBase *part)
{
    for (DWORD i = 0; i < l->monitors.count; i++)
        if (static_cast<CUpdateMonitorInfo *>(l->monitors.data[i])->part == part)
            return i;
    return 0xffffffff;
}

// 0x4e4f70: a part already monitored gives al 0; otherwise a new entry (a failed allocation ends the
// game, as in the original), the list listens to the part, al 1. The upper bytes of eax are not
// defined.
DWORD THISCALL CUpdateMonitorList::Add(CObjectBase *part, int half_width, int half_height)
{
    if (find_monitor(this, part) != 0xffffffff)
        return 0;
    auto *m = static_cast<CUpdateMonitorInfo *>(pool_alloc(sizeof(CUpdateMonitorInfo)));
    if (m)
        m->construct();
    m->half_height = half_height;
    m->part = part;
    m->half_width = half_width;
    part_add_listener(part, this);
    append(&monitors, m);
    return 1;
}

// 0x4e5000: al 1 if the part was monitored (entry removed), else 0; upper bytes of eax not defined
DWORD THISCALL CUpdateMonitorList::Remove(CObjectBase *part)
{
    const DWORD i = find_monitor(this, part);
    if (i == 0xffffffff)
        return 0;
    remove_monitor(this, i);
    return 1;
}

// 0x4e5090 (slot 0): "Delete" and "Leaving Scope" from a monitored part remove its entry; every other
// message goes to CObject's handler 0x46cd50 (does nothing, eax as it was). eax undefined except the
// new count after a removal.
DWORD THISCALL CUpdateMonitorList::OnMessage(CObjectBase *sender, const char *message)
{
    if (!crt_stricmp(message, glob<const char *>(0x60aef8) /* "Delete" */) ||
        !crt_stricmp(message, glob<const char *>(0x60aee8) /* "Leaving Scope" */)) {
        const DWORD i = find_monitor(this, sender);
        if (i != 0xffffffff)
            return remove_monitor(this, i);
    }
    return fn<DWORD(THISCALL *)(void *, CObjectBase *, const char *)>(0x46cd50)(this, sender, message);
}

// ---- CChildList --------------------------------------------------------------------------------------

static void *__fastcall ChildList_CreateArray(int count, int, int) { return create_array<CChildList>((DWORD)count); }

// 0x4f97e0 (slot 2): no parent class
static CClassInfo *__cdecl ChildList_ClassInfo()
{
    return class_getter(0x6c29c8, 0x618a38 /* "CChildList" */, 1.0f, nullptr, 0x18, 0, code<void *>(0x4f9830),
                        code<void *>(0x4f9880), code<void *>(0x4f9aa0));
}

void CChildList::construct()
{
    object_ctor(this);
    children = {vt(0x5f8b8c), nullptr, 0, 0};
    vftable = vt(0x5f92b8);
    object_init_fields(this);
}

// The children are deleted from the last one (the count read again after each), then the fields freed
// and the array released
void CChildList::destroy()
{
    vftable = vt(0x5f92b8);
    while (children.count) {
        const DWORD i = children.count - 1;
        if (void *c = children.data[i])
            delete_object(c);
        remove_at(&children, i);
    }
    object_free_fields(this);
    free_array(children, 0x5f8b98);
    object_dtor(this);
}

// 0x4f9830
static void *__cdecl ChildList_Create()
{
    auto *l = static_cast<CChildList *>(pool_alloc(sizeof(CChildList)));
    if (!l)
        return nullptr;
    l->construct();
    return l;
}

// 0x4f9900
void *THISCALL CChildList::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x4f9aa0: "Child List" (array of owned objects of any class, +0x10/+0x14/+0xc); the element
// descriptor "Child" also goes into the global 0x6c29c4
static void __cdecl ChildList_RegisterFields()
{
    CFieldDesc *&child = *glob<CFieldDesc **>(0x6c29c4);
    void *p = heap_alloc(0x3c);
    child = p ? fn<CFieldDesc *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(
                    0x4186f0)(p, glob<const char *>(0x618a50) /* "Child" */, 0, 4, 0, 0, 1, 0, 0, 0)
              : nullptr;
    CFieldDesc *f = nullptr;
    if (void *q = heap_alloc(0x48))
        f = fn<CFieldDesc *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                        DWORD, CFieldDesc *, DWORD, DWORD)>(0x419230)(
            q, glob<const char *>(0x618a44) /* "Child List" */, 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc, 4, 0xc, 1, child, 1, 0);
    add_field(ChildList_ClassInfo(), f);
}

// 0x4f9b70: takes the child out of the list (if it is in it) and deletes it in any case; a null child
// does nothing (eax: the caller's). eax: the deleting destructor's result.
DWORD THISCALL CChildList::RemoveChild(CObjectBase *child)
{
    if (!child)
        return 0;
    const DWORD n = children.count;
    DWORD i = 0;
    while (i < n && children.data[i] != child)
        i++;
    if (i < n) {
        if (i + 1 < n)
            crt_memmove(&children.data[i], &children.data[i + 1], (n - i - 1) * 4);
        children.count--;
    }
    return delete_object(child);
}

// ---- CLayerPart's class getter -------------------------------------------------------------------------

// 0x4f9bf0 (slot 2 of the layer parts, 42 callers): abstract, version 26, no parent class; its Create
// (0x46b760) returns 0, its fields come from 0x4f9c50
static CClassInfo *__cdecl LayerPart_ClassInfo()
{
    return class_getter(0x6c29cc, 0x618a2c /* "CLayerPart" */, 26.0f, nullptr, 0x4c, 1, code<void *>(0x46b760),
                        code<void *>(0x52dc80), code<void *>(0x4f9c50));
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(broadcast)
{
    const Hook list[] = {
        {0x4674f0, (void *)&ListenerReference_Create, "CListenerReference::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x467530, (void *)&ListenerReference_CreateArray, "CListenerReference::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4675a0, ptr(&CListenerReference::DeletingDtor), "CListenerReference::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x467640, (void *)&ListenerReference_ClassInfo, "CListenerReference::ClassInfo", 5, {0xa1, 0xd8, 0xce, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4676a0, (void *)&ListenerReference_RegisterFields, "CListenerReference::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x30, 0xe8, 0x91, 0x8c, 0x16}, 5},
        {0x467830, (void *)&Broadcaster_ClassInfo, "CBroadcaster::ClassInfo", 5, {0xa1, 0xdc, 0xce, 0x6b, 0x00, 0x85, 0xc0, 0x75}},
        {0x467880, (void *)&Broadcaster_Create, "CBroadcaster::Create", 7, {0x56, 0x57, 0xb9, 0x28, 0x00, 0x00, 0x00, 0xe8}},
        {0x4678e0, (void *)&Broadcaster_CreateArray, "CBroadcaster::CreateArray", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xb6}},
        {0x467970, ptr(&CBroadcaster::DeletingDtor), "CBroadcaster::DeletingDtor", 7, {0x8a, 0x44, 0x24, 0x04, 0x83, 0xec, 0x0c, 0xa8}},
        {0x467b40, (void *)&Broadcaster_RegisterFields, "CBroadcaster::RegisterFields", 8, {0x56, 0x6a, 0x30, 0xe8, 0xf2, 0x87, 0x16, 0x00}, 4},
        {0x467d00, ptr(&CBroadcaster::Ctor), "CBroadcaster::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x08, 0xe9, 0xfa, 0xff}, 4},
        {0x467d40, ptr(&CBroadcaster::Broadcast), "CBroadcaster::Broadcast", 7, {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x77, 0x10, 0x4e}},
        {0x467d80, ptr(&CBroadcaster::ResolvePending), "CBroadcaster::ResolvePending", 7, {0x51, 0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x20, 0x85}},
        {0x467e60, ptr(&CBroadcaster::AddListener), "CBroadcaster::AddListener", 5, {0x51, 0x56, 0x57, 0x8b, 0xf9, 0xb9, 0x14, 0x00}},
        {0x467ee0, ptr(&CBroadcaster::RemoveListener), "CBroadcaster::RemoveListener", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0x83, 0xce, 0xff}},
        {0x4e4830, (void *)&UpdateMonitorInfo_Create, "CUpdateMonitorInfo::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x4e4860, (void *)&UpdateMonitorInfo_CreateArray, "CUpdateMonitorInfo::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4e48c0, ptr(&CUpdateMonitorInfo::DeletingDtor), "CUpdateMonitorInfo::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4e4960, (void *)&UpdateMonitorInfo_ClassInfo, "CUpdateMonitorInfo::ClassInfo", 5, {0xa1, 0xe4, 0x1c, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x4e49c0, (void *)&UpdateMonitorInfo_RegisterFields, "CUpdateMonitorInfo::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x30, 0xe8, 0x71, 0xb9, 0x0e}, 5},
        {0x4e4b60, (void *)&UpdateMonitorList_ClassInfo, "CUpdateMonitorList::ClassInfo2", 5, {0xa1, 0xe8, 0x1c, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x4e4bb0, (void *)&UpdateMonitorList_Create, "CUpdateMonitorList::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x55}},
        {0x4e4c00, (void *)&UpdateMonitorList_CreateArray, "CUpdateMonitorList::CreateArray", 5, {0x51, 0x55, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0x76}},
        {0x4e4c80, ptr(&CUpdateMonitorList::DeletingDtor), "CUpdateMonitorList::DeletingDtor", 7, {0x8a, 0x44, 0x24, 0x04, 0x83, 0xec, 0x08, 0xa8}},
        {0x4e4dd0, (void *)&UpdateMonitorList_ClassInfo, "CUpdateMonitorList::ClassInfo", 5, {0xa1, 0xe8, 0x1c, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x4e4e30, (void *)&UpdateMonitorList_RegisterFields, "CUpdateMonitorList::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x01, 0xb5, 0x0e}, 5},
        {0x4e4f40, ptr(&CUpdateMonitorList::Ctor), "CUpdateMonitorList::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xc8, 0x16, 0xf3, 0xff}, 4},
        {0x4e4f70, ptr(&CUpdateMonitorList::Add), "CUpdateMonitorList::Add", 6, {0x53, 0x56, 0x8b, 0x5c, 0x24, 0x0c, 0x57, 0x8b}},
        {0x4e5000, ptr(&CUpdateMonitorList::Remove), "CUpdateMonitorList::Remove", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6}},
        {0x4e5090, ptr(&CUpdateMonitorList::OnMessage), "CUpdateMonitorList::OnMessage", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x55, 0x56, 0x57}},
        {0x4f97e0, (void *)&ChildList_ClassInfo, "CChildList::ClassInfo", 5, {0xa1, 0xc8, 0x29, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x4f9830, (void *)&ChildList_Create, "CChildList::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x4f9880, (void *)&ChildList_CreateArray, "CChildList::CreateArray", 5, {0x51, 0x55, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0x76}},
        {0x4f9900, ptr(&CChildList::DeletingDtor), "CChildList::DeletingDtor", 7, {0x8a, 0x44, 0x24, 0x04, 0x83, 0xec, 0x08, 0xa8}},
        {0x4f9aa0, (void *)&ChildList_RegisterFields, "CChildList::RegisterFields", 7, {0x6a, 0x3c, 0xe8, 0x93, 0x68, 0x0d, 0x00, 0x83}, 3},
        {0x4f9b70, ptr(&CChildList::RemoveChild), "CChildList::RemoveChild", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x4f9bf0, (void *)&LayerPart_ClassInfo, "CLayerPart::ClassInfo", 5, {0xa1, 0xcc, 0x29, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x4fc1b0, ptr(&BroadcasterOwner::AddListener<0x10>), "LayerPart_AddListener", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x10, 0x85, 0xc0}},
        {0x4fc200, ptr(&BroadcasterOwner::RemoveListener<0x10>), "LayerPart_RemoveListener", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x10, 0x85, 0xc9}},
        {0x4fc240, ptr(&BroadcasterOwner::PartBroadcast), "LayerPart_Broadcast", 5, {0x8b, 0xc1, 0x8b, 0x48, 0x10, 0x85, 0xc9, 0x74}},
        {0x4fd190, ptr(&BroadcasterOwner::AddListener<0xc>), "AIBase_AddListener", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x0c, 0x85, 0xc0}},
        {0x4fd1e0, ptr(&BroadcasterOwner::RemoveListener<0xc>), "AIBase_RemoveListener", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x0c, 0x85, 0xc9}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
