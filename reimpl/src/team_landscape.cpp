// CTeamInfo 0x4e4410-0x4e47d7, CLandscapeAI's registration and update 0x4e3970-0x4e4273 and the layer's
// network block reader 0x4e4380 (phase 3).
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CTeamInfo (0x14 bytes, version 4, vtable 0x5f89bc, class 0x6c1ce0; CObject): +8 "Spawn Point Name",
//     +0xc "Re-Spawn Point Name" (name fields, vtable 0x5ed924), +0x10 "Model Type" (a resource field of the
//     model list 0x6cfdd8). The layer's "Team/Team Info" array holds them (layer.h).
//   CLandscapeAI (0x34, version 3, vtable 0x5f8904, class 0x6c1b10; CLayerPartAI; Create/CreateArray in
//     ai_classes.cpp): +0x10 "Width", +0x14 "Height" (pixels), +0x18 "Change Rate" (units per second, 40),
//     +0x1c "Target Elevation", +0x20 "Leftover Error", +0x24 "Shape" (0 "Rectangle": the ellipse inside the
//     rectangle, else the whole rectangle; list 0x6c1af8), +0x28 "Stop When" (list 0x6c1ad8, 0 "Any Point
//     Reaches Target"), +0x2c "Do Not Modify If GreaterThan" (255), +0x30 "Do Not Modify If Less Than".
//     It raises or lowers the ground (the layer's CPlasmaTileMap +0x7c) around its part towards the target.
//   The layer block reader 0x6c1cd0 ("CLayerBlockReader", registered by 0x4ee750, vtable 0x5f89b0): its read
//     slot 2 is 0x4e4380.
#include "class_info.h"
#include "exe_base.h"
#include "x87.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);
static void obj_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x416610)(o); }
static DWORD obj_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4166d0)(o); }
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
template <DWORD A> static CClassInfo *__cdecl getter() { return code<Getter>(A)(); }
static DWORD ai_base_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4fd080)(o); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void zs_ctor(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
static void zs_dtor(void *z) { fn<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }
static DWORD monitor_add_part(void *save_data, void *part) { return fn<DWORD(THISCALL *)(void *, void *)>(0x4e7330)(save_data, part); }
static DWORD map_cell(void *map, int v) { return fn<DWORD(THISCALL *)(void *, int)>(0x540420)(map, v); }
static DWORD map_elevation(void *map, DWORD col, DWORD row) { return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x540440)(map, col, row); }
static void map_add_elevation(void *map, DWORD col, DWORD row, int delta) { fn<DWORD(THISCALL *)(void *, DWORD, DWORD, int)>(0x5405e0)(map, col, row, delta); }
static BYTE *current_layer() { return fn<BYTE *(THISCALL *)(void *)>(0x4f1050)(*glob<void **>(0x6c2080)); }
static DWORD packet_read_dword(void *p, DWORD *out) { return fn<DWORD(THISCALL *)(void *, DWORD *)>(0x520550)(p, out); }
static DWORD packet_skip(void *p, DWORD n) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x520470)(p, n); }
static void layer_read_frame(void *layer, void *p) { fn<void(THISCALL *)(void *, void *)>(0x4ea6e0)(layer, p); }

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
static BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
static DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

constexpr DWORD TEAM_INFO_VTABLE = 0x5f89bc, LANDSCAPE_VTABLE = 0x5f8904, AI_BASE_VTABLE = 0x5ed1d0;
constexpr DWORD F_ZERO = 0x5ebed0, F_32 = 0x5f89ac;   // 0.0 and 32.0
// Descriptor constructors (thiscall; the arguments as the exe pushes them, see field_kinds.h)
constexpr DWORD STRING_FIELD_CTOR = 0x415dd0;           // 0x34
constexpr DWORD NAME_FIELD_VTABLE = 0x5ed924;           // a "$Trigger"-style name field
constexpr DWORD RESOURCE_FIELD_CTOR_TYPED = 0x54e560;   // 0x38
constexpr DWORD ENUM_INT_FIELD_CTOR_UNITS = 0x54d7f0;   // 0x44
constexpr DWORD ELEVATION_FIELD_CTOR = 0x53f200;        // 0x44 (EnumIntField_CtorElevation)
constexpr DWORD FLOAT_FIELD_CTOR_UNITS = 0x54ec40;      // 0x44
constexpr DWORD FLOAT_FIELD_CTOR = 0x54ecd0;            // 0x44

// ---- The shared pattern ---------------------------------------------------------------------------------

using Construct = void (*)(BYTE *);
using Destroy = DWORD (*)(BYTE *);

template <DWORD SIZE, Construct C> static void *__cdecl create_one()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(SIZE));
    if (!o)
        return nullptr;
    C(o);
    return o;
}
template <DWORD SIZE, Construct C> static void *__fastcall create_array(int count, int, int)
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
template <DWORD SIZE, Destroy D> static void *THISCALL deleting_dtor(BYTE *self, DWORD flags)
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
template <typename... A> static void *new_field(DWORD size, DWORD ctor, A... args)
{
    void *f = heap_alloc(size);
    if (f)
        f = fn_dyn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...);
    return f;
}

// ---- CTeamInfo ---------------------------------------------------------------------------------------

// 0x4e4410, 0x4e45e0 (slot 2)
static CClassInfo *__cdecl TeamInfo_ClassInfo()
{
    return class_getter(glob_addr(0x6c1ce0), glob_addr(0x617550) /* "CTeamInfo" */, 4.0f, getter<0x4163b0>, 0x14, 0, fn_addr(0x4e4460), fn_addr(0x4e44b0), fn_addr(0x4e4640));
}
// Create 0x4e4460, CreateArray 0x4e44b0: CObject's constructor, the two strings, the vtable, InitFields,
// "Model Type" 0
static void TeamInfo_Construct(BYTE *o)
{
    obj_ctor(o);
    zs_ctor(o + 8);
    zs_ctor(o + 0xc);
    dw(o, 0) = vt(TEAM_INFO_VTABLE);
    obj_init_fields(o);
    dw(o, 0x10) = 0;
}
// 0x4e4530 (slot 1): the vtable, FreeFields, the strings (+0xc first), CObject's destructor
static DWORD TeamInfo_Destroy(BYTE *o)
{
    dw(o, 0) = vt(TEAM_INFO_VTABLE);
    obj_free_fields(o);
    zs_dtor(o + 0xc);
    zs_dtor(o + 8);
    return obj_dtor(o);
}
// A name field (a string field with the vtable 0x5ed924 when it was made)
static void *name_field(DWORD name, DWORD offset)
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), name, offset, 4, offset, 1, 0, 0);
    if (f)
        dw(f, 0) = vt(NAME_FIELD_VTABLE);
    return f;
}
// 0x4e4640. eax: the last AddField's.
// The descriptor is made before the class getter runs, as in the original
static DWORD add_team_field(void *f) { return add_field(TeamInfo_ClassInfo(), f); }
static DWORD __cdecl TeamInfo_RegisterFields()
{
    add_team_field(name_field(glob_addr(0x61757c) /* "Spawn Point Name" */, 8));
    add_team_field(name_field(glob_addr(0x617568) /* "Re-Spawn Point Name" */, 0xc));
    return add_team_field(new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x61755c) /* "Model Type" */, 0x10, 4, 0x10, glob_addr(0x6cfdd8), 0, 0, 0, 0));
}

// ---- CLandscapeAI --------------------------------------------------------------------------------------

// 0x4e3970, 0x4e3b10 (slot 2)
static CClassInfo *__cdecl Landscape_ClassInfo()
{
    return class_getter(glob_addr(0x6c1b10), glob_addr(0x6174b0) /* "CLandscapeAI" */, 3.0f, getter<0x4fd220>, 0x34, 0, fn_addr(0x4e39c0), fn_addr(0x4e3a00), fn_addr(0x4e3b70));
}
// 0x4e3a70 (slot 1): the vtable, FreeFields, CAIBase's vtable, FreeFields, CAIBase's destructor
static DWORD Landscape_Destroy(BYTE *o)
{
    dw(o, 0) = vt(LANDSCAPE_VTABLE);
    obj_free_fields(o);
    dw(o, 0) = vt(AI_BASE_VTABLE);
    obj_free_fields(o);
    return ai_base_dtor(o);
}
// 0x4e3b70 (each descriptor made before the class getter runs). eax: the last AddField's.
static DWORD add_landscape_field(void *f) { return add_field(Landscape_ClassInfo(), f); }
static DWORD __cdecl Landscape_RegisterFields()
{
    add_landscape_field(new_field(0x44, fn_addr(ELEVATION_FIELD_CTOR), glob_addr(0x61753c) /* "Target Elevation" */, 0x1c, 4, 0x1c, 1, 0x7f, 0));
    add_landscape_field(new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x60a5e8) /* "Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x10, 4,
                                               0x10, 1, 0x20, 0, 1, 0x1869f));
    add_landscape_field(new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x60a5e0) /* "Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x14, 4,
                                               0x14, 1, 0x20, 0, 1, 0x1869f));
    add_landscape_field(new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x617520) /* "Change Rate" */, glob_addr(0x61752c) /* "Units per sec" */,
                                               0x18, 4, 0x18, 1, 0x42200000 /* 40 */, 0, 0, 0x497423f0 /* 999999 */, 0x3f800000 /* 1 */));
    add_landscape_field(new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x617510) /* "Leftover Error" */, 0x20, 4, 0x20, 0, 0, 0, 0, 0, 0));
    add_landscape_field(new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x617508) /* "Shape" */, 0x24, 4, 0x24, glob_addr(0x6c1af8), 1,
                                               glob_addr(0x61749c) /* "Rectangle" */, 0, 0));
    add_landscape_field(new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x6174fc) /* "Stop When" */, 0x28, 4, 0x28, glob_addr(0x6c1ad8), 1,
                                               glob_addr(0x617480) /* "Any Point Reaches Target" */, 0, 0));
    add_landscape_field(new_field(0x44, fn_addr(ELEVATION_FIELD_CTOR), glob_addr(0x6174dc) /* "Do Not Modify If GreaterThan" */, 0x2c, 4, 0x2c, 1, 0xff, 0));
    return add_landscape_field(new_field(0x44, fn_addr(ELEVATION_FIELD_CTOR), glob_addr(0x6174c0) /* "Do Not Modify If Less Than" */, 0x30, 4, 0x30, 1, 0, 0));
}
// 0x4e4040 (slot 24, begin): the part goes into its layer's update monitor (CLayerSaveData::MonitorAddPart
// on layer +0x3c). eax: that function's.
static DWORD THISCALL Landscape_Begin(void *, BYTE *part) { return monitor_add_part(pt(part, 0x48) + 0x3c, part); }

// 0x4e4060 (slot 25, update with the time step): with a rate above 0 the whole units of rate · dt plus
// the leftover change every ground cell (CPlasmaTileMap +0x7c of the part's layer) in the rectangle of
// width × height around the part (slot 43, its position, truncated) towards "Target Elevation" by that
// many units (AddElevation), skipping cells whose elevation is above "Do Not Modify If GreaterThan" or
// below "Do Not Modify If Less Than"; with "Shape" 0 only the cells in the ellipse (the row distance
// scaled by width / height, inside (width / 2 + 32)²). A cell at the target stops a "Stop When" 0 AI
// (rate 0); when no cell was changed (or the rectangle is empty) the rate becomes 0 unless "Stop When"
// is 2. Cells are 64 pixels; the comparisons unsigned.
static void THISCALL Landscape_Update(BYTE *self, BYTE *part, float dt)
{
    if (!(fl(self, 0x18) > *glob<float *>(F_ZERO)))
        return;
    const long double st = (long double)dt * (long double)fl(self, 0x18) + (long double)fl(self, 0x20);
    const int n = x87_ftol(st);
    fl(self, 0x20) = (float)(st - (long double)n);
    if (n <= 0)
        return;
    BYTE *map = pt(pt(part, 0x48), 0x7c);
    float pos[2];
    vcall<DWORD>(part, 43, pos);
    const DWORD w = dw(self, 0x10), h = dw(self, 0x14);
    const float aspect = (float)((long double)(unsigned long long)w / (long double)(int)h);
    const long double half = (long double)(unsigned long long)(w >> 1) + (long double)*glob<float *>(F_32);
    const float r2 = (float)(half * half);
    const int x = x87_ftol(pos[0]);
    const int y = x87_ftol(pos[1]);
    const DWORD c0 = map_cell(map, x - (int)(w >> 1));
    const DWORD c1 = map_cell(map, (int)(w >> 1) + x);
    const DWORD r0 = map_cell(map, y - (int)(h >> 1));
    const DWORD r1 = map_cell(map, (int)(h >> 1) + y);
    bool untouched = true;
    if (r0 <= r1) {
        DWORD rowpix = r0 << 6;
        for (DWORD r = r0; r <= r1; r++, rowpix += 0x40) {
            if (c0 > c1)
                continue;
            DWORD colpix = c0 << 6;
            for (DWORD c = c0; c <= c1; c++, colpix += 0x40) {
                if (dw(self, 0x24) == 0) {
                    const int dx = (int)(colpix - (DWORD)x), dy = (int)(rowpix - (DWORD)y);
                    const long double t = (long double)dy * (long double)aspect;
                    if (t * t + (long double)(int)((DWORD)dx * (DWORD)dx) > (long double)r2)
                        continue;
                }
                const DWORD e = map_elevation(map, c, r);
                const DWORD target = dw(self, 0x1c);
                if (e == target) {
                    if (dw(self, 0x28) == 0)
                        dw(self, 0x18) = 0;
                } else if (e <= dw(self, 0x2c) && e >= dw(self, 0x30)) {
                    untouched = false;
                    map_add_elevation(map, c, r, e > target ? -n : n);
                }
            }
        }
        if (!untouched)
            return;
    }
    if (dw(self, 0x28) != 2)
        dw(self, 0x18) = 0;
}

// ---- The layer's block reader ------------------------------------------------------------------------------

// 0x4e4380 (slot 2, read): the local character 0x6bde9c cleared, a dword read from the packet (the frame's
// length; without one the packet's own address stays as the length, the argument slot of the original);
// with a current layer the layer reads the frame (0x4ea6e0) and gets +0x1cc and +0x1cd set, otherwise
// the frame is skipped. eax: the layer, or Skip's.
static DWORD THISCALL LayerBlockReader_Read(void *, BYTE *packet)
{
    DWORD n = ptrv(packet);
    *glob<DWORD *>(0x6bde9c) = 0;
    packet_read_dword(packet, &n);
    if (!current_layer())
        return packet_skip(packet, n);
    layer_read_frame(current_layer(), packet);
    current_layer()[0x1cc] = 1;
    BYTE *layer = current_layer();
    layer[0x1cd] = 1;
    return ptrv(layer);
}

ZAX_HOOKS(team_landscape)
{
    const Hook list[] = {
#include "team_landscape_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
