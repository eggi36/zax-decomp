// The per-frame update of a level reimplemented (see layer_update.h).
#include "layer_update.h"

namespace zax {

namespace {

void *world() { return *glob<void **>(0x6c2080); }
// The game type's slot 12: a multiplayer game
BYTE multiplayer() { return (BYTE)vcall<DWORD>(vcall<void *>(world(), 1), 12); }

void temp_list(LayerArray **l) { fn<void *(THISCALL *)(LayerArray **)>(0x59a470)(l); }
void release_list(LayerArray **l) { fn<DWORD(THISCALL *)(LayerArray **)>(0x59a510)(l); }
void find_rect(LayerSaveData *sd, LayerArray *list, const int *rect, DWORD kind, DWORD all, DWORD none, DWORD any)
{
    fn<DWORD(THISCALL *)(void *, LayerArray *, const int *, DWORD, DWORD, DWORD, DWORD)>(0x57a770)(sd->grid, list, rect, kind, all, none, any);
}
void query_rect(LayerSaveData *sd, LayerArray *list, const int *rect, DWORD flags)
{
    fn<DWORD(THISCALL *)(void *, LayerArray *, const int *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x57a100)(sd->grid, list, rect, 5, flags, 0, 0,
                                                                                                        0);
}
DWORD part_flags(void *p) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + 0x1c); }
void *crt_memmove(void *d, const void *s, DWORD n) { return fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }

// The monitor entries (CUpdateMonitorInfo): +8 the part, +0xc half width, +0x10 half height
struct MonitorEntry {
    DWORD vftable, fields;
    BYTE *part;
    int half_width, half_height;
};

}  // namespace

void THISCALL LayerUpdate::FirstUpdate(LayerSaveData *self)
{
    LayerArray *list;
    temp_list(&list);
    const DWORD flags = multiplayer() ? 0x800000 : 0x40800000;
    find_rect(self, list, nullptr, 0, flags, 0, 0);
    const DWORD n = list->count;
    for (DWORD i = 0; i < n; i++)
        vcall<DWORD>(list->data[i], 33);   // activate
    alignas(4) BYTE color[16];
    fn<void *(THISCALL *)(void *)>(0x53f000)(color);
    vcall<DWORD>(world(), 102, color);
    int size[2];
    vcall<DWORD>(world(), 99, size);
    const DWORD depth = fn<DWORD(__cdecl *)()>(0x55d030)();
    fn<DWORD(THISCALL *)(void *, int, int, DWORD)>(0x4ab5a0)(self->fog, size[0], size[1], depth);
    vcall<DWORD>(self->fog, 26, color);
    reinterpret_cast<DWORD *>(self->mini_map)[1] = 0;
    reinterpret_cast<DWORD *>(self->mini_map)[0] = 0;
    reinterpret_cast<DWORD *>(self->mini_map)[3] = 0;
    reinterpret_cast<DWORD *>(self->mini_map)[2] = 0;
    release_list(&list);
}

void THISCALL LayerUpdate::WorldUpdate(LayerSaveData *self, LayerArray *views, float dt)
{
    const DWORD nviews = views->count;
    const DWORD nmon = self->monitor.count;
    if (!self->begun) {
        fn<void(THISCALL *)(LayerSaveData *)>(0x4e71b0)(self);
        self->begun = 1;
    }
    LayerArray *list;
    temp_list(&list);
    fn<DWORD(THISCALL *)(LayerArray *)>(0x59a120)(list);
    const DWORD flags = multiplayer() ? 0x800000 : 0x40800000;
    // The monitored parts with a size: the grid around them
    for (DWORD i = 0; i < nmon; i++) {
        const MonitorEntry *e = static_cast<MonitorEntry *>(self->monitor.data[i]);
        BYTE *p = e->part;
        if (!p || (part_flags(p) & flags) != flags || !e->half_width || !e->half_height)
            continue;
        const DWORD x = vcall<DWORD>(p, 52);
        const DWORD y = vcall<DWORD>(p, 53);
        const DWORD w = (DWORD)e->half_width, h = (DWORD)e->half_height;   // wrapping sums as in the original
        const int rect[4] = {(int)(x - w), (int)(y - h), (int)(x + 1 + w), (int)(y + 1 + h)};
        query_rect(self, list, rect, flags);
    }
    fn<DWORD(THISCALL *)(void *, LayerArray *)>(0x57aeb0)(self->grid, list);
    // The monitored parts without a size themselves
    for (DWORD i = 0; i < nmon; i++) {
        const MonitorEntry *e = static_cast<MonitorEntry *>(self->monitor.data[i]);
        BYTE *p = e->part;
        if (p && (part_flags(p) & flags) == flags && (!e->half_width || !e->half_height))
            fn<DWORD(THISCALL *)(LayerArray *, void *)>(0x59a140)(list, p);
    }
    for (DWORD j = 0; j < nviews; j++)
        query_rect(self, list, reinterpret_cast<const int *>(reinterpret_cast<BYTE *>(views->data) + j * 16), flags);
    DWORD t;
    memcpy(&t, &dt, 4);
    fn<DWORD(THISCALL *)(void *, LayerArray *, DWORD)>(0x57a030)(self->grid, list, t);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x41c700)(self->action_queue, t);
    release_list(&list);
}

void THISCALL LayerUpdate::OnPartFlags(LayerSaveData *, BYTE *part, DWORD old_flags, DWORD changed)
{
    BYTE *w = static_cast<BYTE *>(world());
    if (w[0x29c] && !*reinterpret_cast<void **>(part + 0x48))
        return;
    const DWORD f = part_flags(part);
    if (f >> 29 & 1) {
        if ((f & 0x60000000) != 0x20000000 || !(changed & 0x830000))
            return;
        void *p = vcall<void *>(w, 44);
        for (DWORD j = 1; j < vcall<DWORD>(p, 11); j++) {
            void *pl = vcall<void *>(p, 23, j);
            void *reader = fn<void *(__cdecl *)()>(0x4f8850)();   // CClientSetLayerPartFlag
            fn<DWORD(THISCALL *)(void *, void *, void *)>(0x4f88e0)(reader, pl, part);
        }
        return;
    }
    if (!((changed & 0x830000) && (old_flags & 0x200000)) && !(changed & (0x400000 | 0x200000)))
        return;
    void *p = vcall<void *>(w, 44);
    for (DWORD j = 1; j < vcall<DWORD>(p, 11); j++) {
        void *pl = vcall<void *>(p, 23, j);
        void *packet = vcall<void *>(p, 13, j);
        void *reader = fn<void *(__cdecl *)()>(0x4f8770)();   // CClientInvalidateRect
        fn<DWORD(THISCALL *)(void *, void *, void *, void *)>(0x4f87c0)(reader, packet, pl, part);
    }
}

void THISCALL LayerUpdate::SendPartFlags(LayerSaveData *self, void *packet)
{
    LayerArray *list;
    temp_list(&list);
    find_rect(self, list, nullptr, 0, 0x20000000, 0x40000000, 0);
    for (DWORD i = 0; i < list->count; i++) {
        BYTE *p = static_cast<BYTE *>(list->data[i]);
        if (*reinterpret_cast<WORD *>(p + 0x28) && !(BYTE)fn<DWORD(THISCALL *)(void *, DWORD)>(0x4fc880)(p, 0xa20000)) {
            void *reader = fn<void *(__cdecl *)()>(0x4f8850)();
            fn<DWORD(THISCALL *)(void *, void *, void *)>(0x4f88e0)(reader, packet, p);
        }
    }
    release_list(&list);
}

void THISCALL LayerUpdate::Begin(LayerSaveData *self)
{
    const bool single = !multiplayer();
    if (single) {
        if (!self->byte18c) {
            vcall<DWORD>(*glob<void **>(0x6c02cc), 27, (DWORD)1, (DWORD)1);
            vcall<DWORD>(self->layer, 7);
            fn<DWORD(THISCALL *)(void *)>(0x4a3760)(fn<void *(__cdecl *)()>(0x4a35d0)());
        }
        self->byte18c = 1;
    }
    LayerArray *list;
    temp_list(&list);
    find_rect(self, list, nullptr, 0, 0, 0, 0);
    for (DWORD i = 0; i < list->count; i++) {
        void *p = list->data[i];
        fn<DWORD(THISCALL *)(void *)>(0x4fc870)(p);
        if (vcall<CClassInfo *>(p, 2) == fn<CClassInfo *(__cdecl *)()>(0x487620)() ||
            vcall<CClassInfo *>(p, 2) == fn<CClassInfo *(__cdecl *)()>(0x55d2a0)() ||
            vcall<CClassInfo *>(p, 2) == fn<CClassInfo *(__cdecl *)()>(0x4b5c60)() ||
            vcall<CClassInfo *>(p, 2) == fn<CClassInfo *(__cdecl *)()>(0x598d90)())
            vcall<DWORD>(p, 58, (DWORD)0x20000000);
        else if (single)
            fn<DWORD(THISCALL *)(void *)>(0x4fbf30)(p);
    }
    if (single)
        fn<DWORD(THISCALL *)(void *, LayerArray *, DWORD)>(0x57a030)(self->grid, list, 0);
    release_list(&list);
}

void THISCALL LayerUpdate::WriteRect(LayerSaveData *self, void *packet, DWORD, const int *rect)
{
    const BYTE *tm = self->tile_map;
    const int tw = *reinterpret_cast<const int *>(tm + 0x50), th = *reinterpret_cast<const int *>(tm + 0x54);
    int r[4];
    r[0] = rect[0] & ~(tw - 1);
    r[2] = (rect[2] & ~(tw - 1)) + tw;
    r[1] = rect[1] & ~(th - 1);
    r[3] = (rect[3] & ~(th - 1)) + th;
    LayerArray *list;
    temp_list(&list);
    const DWORD flags = *glob<BYTE *>(0x6c1cf5) ? 0x800000 : 0x820000;
    find_rect(self, list, r, 0, flags, 0x20010000, 0);
    const DWORD n = list->count;
    const DWORD at_word = fn<DWORD(THISCALL *)(void *, DWORD)>(0x5209c0)(packet, 0);
    DWORD written = 0;
    for (DWORD i = 0; i < n; i++)
        written += vcall<DWORD>(list->data[i], 25, packet, (DWORD)1);
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x520f30)(packet, written, at_word);
    release_list(&list);
}

void __cdecl StaticPartArray_DeleteAll()
{
    LayerArray *a = glob<LayerArray *>(STATIC_PART_ARRAY);
    while (a->count) {
        const DWORD i = a->count - 1;
        if (void *e = a->data[i])
            vcall<void *>(e, 1, 1);
        const DWORD count = a->count;
        if (i < count) {
            const DWORD next = i + 1;
            if (next < count)
                crt_memmove(&a->data[i], &a->data[next], (next * 0x3fffffff + count) * 4);
            a->count = count - 1;
        }
    }
}

ZAX_HOOKS(layer_update)
{
    using U = LayerUpdate;
    const Hook list[] = {
#include "layer_update_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
