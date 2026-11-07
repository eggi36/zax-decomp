// The frame check harness of phase 4 (rendering): fake frames the game's drawing code accepts, a run of a
// draw call on side 0 (the original) and side 1 (the reimplementation) on two identical frames, and the
// comparison of everything the call can change (docs/DECOMPILATION.md 9, "Frame classes and the frame
// check harness").
//
// The frame classes as the exe builds them [confirmed: constructors, destructors, getters]:
//  - CFrame (0x40, vtable 0x5f4814 with 64 slots, class 0x6be7d4, parent CFrameInterface (8 bytes, abstract,
//    vtable 0x5eb534); constructor 0x4a8a90, deleting destructor slot 1 0x4a8820): +0x08/+0x0a the origin
//    (signed words; the frame's own rectangle is (x, y, x + width, y + height), slot 36 0x4aa520), +0x0c
//    the pixels (operator new, cleared; 0x4aa5f0 allocates width * height * bytes per pixel), +0x10/+0x11
//    floor(log2) of width and height, +0x12/+0x14 width and height (words), +0x18 the row table (operator
//    new, max(height, 2) dwords: row i starts at pixel i * pitch; 0x4aa2c0 builds it, the drawing code
//    takes the pitch from entry 1), +0x1c..+0x38 the blocks of a compressed frame (+0x1c/+0x24/+0x2c/+0x34
//    data blocks whose first dword is their size, +0x20/+0x28/+0x30/+0x38 their tables; slot 21 0x4aca00
//    sums them, 0x4aa790 frees them) [assumption for the table lengths], +0x3c flags: 0x02/0x04 compressed,
//    the depth 0x20 8 bits (palette), 0x40 16 bits, 0x80 24 bits, 0x100 32 bits (0x4aa5f0; slot 29 bits,
//    slot 30 bytes per pixel), 0x4000 (slot 59; cleared by the destructor). The palette of an 8-bit frame is
//    not a member: slot 58 (0x4aac20) finds or creates a CPaletteHolder (0x14 bytes, the palette object
//    {data, count, ...} at +8) as a dynamic field of the frame (descriptor 0x6be38c).
//  - CDIBFrame (0x48, vtable 0x5f4460 with 64 slots, class 0x6be1dc, parent CFrame, Create 0x4a7800): a
//    CFrame over a GDI DIB section, +0x40/+0x44 two GDI objects (DeleteObject in the destructor 0x4a78a0,
//    which clears +0x0c instead of freeing the pixels). The drawing slots 21-63 are CFrame's.
//  - CProxyFrame (0x0c, vtable 0x5eb62c with 63 slots, class 0x712228, parent CFrameInterface): +0x08 the
//    target frame (slot 62 0x54fdb0 sets it); every drawing slot forwards to the target.
//  - COffsetClippedProxyFrame (0x3c, vtable 0x5f8ca8, class 0x6d0148, parent CProxyFrame; constructors
//    0x524770 and 0x5247c0 (on a frame)): the proxy part (+0x08 the target), +0x0c an offset frame (0x14,
//    vtable 0x5fb928, constructor 0x525000: +0x08 its target, +0x0c/+0x10 the offset x, y; slots 43/44;
//    0x524fe0 sets it) and +0x20 a clip frame (0x1c, vtable 0x5efd34, constructor 0x46d790: +0x08 its
//    target, +0x0c the clip rectangle; slot 36 0x46d900 intersects it with the target's, slot 62 0x46dbb0
//    takes target and rectangle from a frame). The constructor on a frame points the offset frame at the
//    clip frame and both proxy and clip frame at the frame. Drawing subtracts the offset (slot 43/44 of
//    the offset frame), clips with the clip frame's rectangle and calls the target. CLayer derives from it.
//  - CScreenFrame (0xe8, vtable 0x5ff360) holds the DirectDraw surface; +0xcc "Display is RGB 565"
//    (Zax.CFG, from the display mode: 15 bits give 0). Not faked here (phase 5).
// The 16-bit format: every 16-bit frame of the game is 5-6-5. Pixel16_FromRGB (0x433a10) makes 5-6-5 pixels
// from the tables 0x633394 (red << 11), 0x675bb8 (green << 5, six bits) and 0x630378 (blue) once
// InitBlendTables (0x433000) has filled them, else with rounding; 0x630578 is entry 0x80 of the blue
// table, its test "tables built". The 5-5-5 forms exist beside them: 0x4339a0 (the 5-5-5 pixel, tables
// 0x633b94 and 0x6753b8), 0x433a90 (both 16-bit forms of a palette, 0x62fb34 5-5-5 and 0x6343b4 5-6-5),
// the span rasterizer 0x42b6e0; CScreenFrame's present (slot 28 0x568f10) converts to 5-5-5 when +0xcc is
// 0. A frame has no 5-5-5 flag; the harness's RGB555 format is a 16-bit frame filled with 5-5-5 pixels
// for the functions that take them.
//
// The harness (namespace zax::framec):
//  - Fake frames in the harness's own memory (no DirectDraw surface; CDIBFrame without GDI objects): one
//    arena per side with the frame objects, the row tables and the pixels; the same offsets on both
//    sides. Every buffer lies between two guard bands of 64 bytes; buffer, padding and guards are filled
//    with a pattern from the case seed and compared whole, so overdraw outside the clip, into the pitch
//    padding or past the buffer shows. Any size, pitch >= width, origin; proxies with offset and clip.
//    Fake frames are never given to the exe's destructors (their memory is not the heap's).
//  - Recording vtables: copies of the exe's vtables whose drawing slots (21 and above) go through
//    thunks that log the slot and the object (mapped) before jumping to the exe's slot; the call log of a
//    side. Slots 0-20 (CObject) stay the exe's, so class_of and IsKindOf work.
//  - run_sides(): side 0 with the module's hooks out, side 1 through the hooks; compares eax (equal raw
//    values first, then pointers mapped: the arena by offset, the stack by its low byte), the call log,
//    the frame objects (pointers mapped), the row tables and all pixel memory. On a difference: up to
//    four first differing pixels (frame, x, y, both values; or the guard byte) and the bounding box.
//  - MmxSwitch: Cpu_HasMmx's result (0x6cfdc4, detected 0x6cfdc5) set for a run and put back.
//  - Pixel16Tables: the "tables built" test 0x630578 cleared for a run (the rounding path) and put back.
//  - Real sources: frames of the running level found read-only (sprites: the model list's sequences;
//    fonts; the frame and texture lists; the world's screen, tile map and fog frames), a deterministic
//    spread with a cap per kind (spread_pick), copied into fake frames; the game palette copied.
#pragma once
#include "check_util.h"

namespace zax::geo {
bool readable(const void *p, DWORD n);
CClassInfo *class_of(const void *p);
}  // namespace zax::geo

namespace zax::framec {

inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- The exe's frame classes ---------------------------------------------------------------------------------

constexpr DWORD VT_FRAME_INTERFACE = 0x5eb534, VT_CFRAME = 0x5f4814, VT_DIB_FRAME = 0x5f4460, VT_PROXY_FRAME = 0x5eb62c,
                VT_OFFSET_CLIPPED = 0x5f8ca8, VT_OFFSET_FRAME = 0x5fb928, VT_CLIP_FRAME = 0x5efd34;
constexpr DWORD CFRAME_CLASS = 0x6be7d4, CSEQUENCE_CLASS = 0x713378, CMODEL_CLASS = 0x6cfdd4;
constexpr DWORD F_COMPRESSED = 0x6, F_8 = 0x20, F_16 = 0x40, F_24 = 0x80, F_32 = 0x100, F_DEPTH = 0x1e0;

struct Rect {
    int left, top, right, bottom;
};

struct FrameData {           // CFrame, 0x40
    DWORD vftable;           // 0x00 0x5f4814
    DWORD object_04;         // 0x04 CObject (0 after its constructor)
    short x, y;              // 0x08 origin
    BYTE *pixels;            // 0x0c
    BYTE log2_width;         // 0x10
    BYTE log2_height;        // 0x11
    WORD width;              // 0x12
    WORD height;             // 0x14
    WORD pad_16;             // 0x16
    DWORD *rows;             // 0x18 row starts in pixels
    void *blocks[8];         // 0x1c compressed frames: data blocks and tables
    DWORD flags;             // 0x3c
};
static_assert(sizeof(FrameData) == 0x40 && __builtin_offsetof(FrameData, rows) == 0x18 && __builtin_offsetof(FrameData, flags) == 0x3c);

struct DibFrameData {        // CDIBFrame, 0x48
    FrameData frame;         // vtable 0x5f4460
    DWORD gdi_40, gdi_44;    // GDI objects (DeleteObject in the destructor); 0 here
};
static_assert(sizeof(DibFrameData) == 0x48);

struct ProxyFrameData {      // CProxyFrame, 0x0c
    DWORD vftable;           // 0x5eb62c
    DWORD object_04;
    void *target;            // 0x08
};
struct OffsetFrameData {     // 0x14, vtable 0x5fb928
    ProxyFrameData proxy;
    int dx, dy;              // 0x0c, 0x10
};
struct ClipFrameData {       // 0x1c, vtable 0x5efd34
    ProxyFrameData proxy;
    Rect clip;               // 0x0c
};
struct OffsetClippedProxyFrameData {   // COffsetClippedProxyFrame, 0x3c, vtable 0x5f8ca8
    ProxyFrameData proxy;              // 0x00
    OffsetFrameData offset;            // 0x0c
    ClipFrameData clip;                // 0x20
};
static_assert(sizeof(OffsetClippedProxyFrameData) == 0x3c && __builtin_offsetof(OffsetClippedProxyFrameData, clip) == 0x20);

// FieldColor (0x10): +0 b, g, r, a, +4 b, g, r again, +8 the 16-bit pixel, +0xa the palette index, +0xc flags
// (1 index valid, 2 from RGB); what Frame_FillRect and the colour fields take
struct FieldColor {
    BYTE b, g, r, a;
    BYTE b2, g2, r2, pad7;
    WORD pixel16;
    BYTE index, padb;
    DWORD flags;
};
static_assert(sizeof(FieldColor) == 0x10);

enum Format { PAL8, RGB565, RGB555, RGB24, RGB32 };
inline DWORD format_flag(Format f) { return f == PAL8 ? F_8 : f == RGB24 ? F_24 : f == RGB32 ? F_32 : F_16; }
inline DWORD format_bytes(Format f) { return f == PAL8 ? 1 : f == RGB24 ? 3 : f == RGB32 ? 4 : 2; }
inline const char *format_name(Format f)
{
    return f == PAL8 ? "8-bit" : f == RGB565 ? "565" : f == RGB555 ? "555" : f == RGB24 ? "24-bit" : "32-bit";
}

// ---- The arenas ------------------------------------------------------------------------------------------------

// ARENA_PIXELS holds an 800 x 600 frame of 16 bits (the world draw batch's HUD checks draw into a frame of the
// HUD frame's size; 0x30000 before)
constexpr DWORD GUARD = 64, ARENA_OBJECTS = 0x800, ARENA_ROWS = 0x1000, ARENA_PIXELS = 0xf0000, MAX_FRAMES = 8;
constexpr DWORD CLASS_SLOTS[5] = {64, 64, 63, 63, 63};   // CFrame, CDIBFrame (0x5f4560 starts the font list's vtable), offset clipped, offset, clip
constexpr DWORD CLASS_VTABLES[5] = {VT_CFRAME, VT_DIB_FRAME, VT_OFFSET_CLIPPED, VT_OFFSET_FRAME, VT_CLIP_FRAME};

struct Arena {
    alignas(16) BYTE objects[ARENA_OBJECTS];
    DWORD rows[ARENA_ROWS];
    alignas(16) BYTE pixels[ARENA_PIXELS];
};

// A frame of a case: where its buffer lies in the side's pixel arena (the same on both sides)
struct FrameInfo {
    DWORD object;            // offset in objects
    DWORD buffer, bytes;     // offset of the buffer in pixels, its size
    DWORD pitch, height, bpp;
};

struct FrameSpec {
    Format format = RGB565;
    int width = 16, height = 16;
    int pitch = 0;           // pixels; 0: the width
    int x = 0, y = 0;        // origin
    bool dib = false;        // CDIBFrame instead of CFrame
    bool recording = true;   // the recording vtable (call log)
};

// Recording vtables and their thunks (checks_frame_common.cpp)
struct Recorder {
    DWORD vtables[5][105];
    DWORD forward[512];      // thunk k jumps to forward[k]
    DWORD slot_of[512];
    bool ready;
};

// The call log of a side: (slot, object) per call to a recording vtable, hashed, the first 32 kept
struct CallLog {
    unsigned hash;
    int count;
    DWORD first[32][2];
};

struct Harness {
    Arena arena[2];
    DWORD next_object, next_rows, next_pixels;   // bump allocators (the same for both sides)
    int side_building;                           // side the next frames are for
    FrameInfo frames[MAX_FRAMES];
    int nframes;
    unsigned seed;
    CallLog calls[2];
    int logging_side;                            // -1: not logging
    Recorder rec;
    DWORD stack_base, stack_limit;
};
extern Harness g_h;

// Built once (checks_frame_common.cpp): the recording vtables and their thunks
void init_recorder();
// The thunks' handler: logs the call and returns the exe's slot
extern "C" DWORD __cdecl framec_handler(DWORD k, DWORD ecx);

// ---- Values as both sides see them ------------------------------------------------------------------------------

inline bool in_arena(int side, DWORD v)
{
    return v >= ptrv(&g_h.arena[side]) && v < ptrv(&g_h.arena[side]) + sizeof(Arena);
}
// A pointer into the side's arena by its offset, a stack address by its low byte, everything else raw
inline DWORD map_value(int side, DWORD v)
{
    if (in_arena(side, v))
        return 0xa0000000u | (v - ptrv(&g_h.arena[side]));
    if (in_arena(1 - side, v))
        return 0xa8000000u | (v - ptrv(&g_h.arena[1 - side]));
    if (v >= g_h.stack_limit && v < g_h.stack_base)
        return 0xb0000000u | (v & 0xff);
    return v;
}

// ---- Building frames ----------------------------------------------------------------------------------------

inline unsigned pattern_next(unsigned &s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}
inline void fill_pattern(BYTE *p, DWORD n, unsigned seed)
{
    unsigned s = seed * 2654435761u + 0x9e3779b9u;
    if (!s)
        s = 1;
    for (DWORD i = 0; i < n; i++)
        p[i] = (BYTE)(pattern_next(s) >> 7);
}

// Starts a case: empty arenas (objects and rows zeroed, all pixel memory a pattern of the seed)
inline void begin_case(unsigned seed)
{
    g_h.seed = seed;
    g_h.next_object = g_h.next_rows = g_h.next_pixels = 0;
    g_h.nframes = 0;
    for (int s = 0; s < 2; s++) {
        memset(g_h.arena[s].objects, 0, sizeof g_h.arena[s].objects);
        memset(g_h.arena[s].rows, 0, sizeof g_h.arena[s].rows);
    }
}

inline BYTE log2_of(WORD v)
{
    BYTE n = 0;
    while (v >> (n + 1))
        n++;
    return v >= 2 ? n : 0;
}

inline DWORD vtable_for(int cls, DWORD exe_vtable, bool recording)
{
    if (!recording)
        return exe_vtable;
    init_recorder();
    return ptrv(g_h.rec.vtables[cls]);
}

// The pixel memory of a frame: GUARD + buffer + GUARD, buffer and guards a pattern of the case seed and
// the frame's number; the buffer is pitch * max(height, 1) pixels (the exe allocates width * height)
// Builds the same frame on both sides; returns side 0's object (frame(side, i) for side 1's).
inline int make_frame(const FrameSpec &s)
{
    const DWORD bpp = format_bytes(s.format);
    const DWORD pitch = (DWORD)(s.pitch > s.width ? s.pitch : s.width);
    const DWORD h = (DWORD)s.height, rows = h < 2 ? 2 : h;
    const DWORD bytes = pitch * (h ? h : 1) * bpp;
    const DWORD total = (GUARD + bytes + GUARD + 15) & ~15u;
    const DWORD osize = s.dib ? sizeof(DibFrameData) : sizeof(FrameData);
    if (g_h.nframes >= (int)MAX_FRAMES || g_h.next_object + osize > ARENA_OBJECTS || g_h.next_rows + rows > ARENA_ROWS ||
        g_h.next_pixels + total > ARENA_PIXELS)
        return -1;
    FrameInfo &fi = g_h.frames[g_h.nframes];
    fi = {g_h.next_object, g_h.next_pixels + GUARD, bytes, pitch, h, bpp};
    for (int side = 0; side < 2; side++) {
        Arena &a = g_h.arena[side];
        BYTE *mem = a.pixels + g_h.next_pixels;
        fill_pattern(mem, total, g_h.seed * 31u + (unsigned)g_h.nframes * 7919u);
        FrameData *f = reinterpret_cast<FrameData *>(a.objects + g_h.next_object);
        memset(f, 0, osize);
        f->vftable = s.dib ? vtable_for(1, VT_DIB_FRAME, s.recording) : vtable_for(0, VT_CFRAME, s.recording);
        f->x = (short)s.x;
        f->y = (short)s.y;
        f->pixels = mem + GUARD;
        f->width = (WORD)s.width;
        f->height = (WORD)s.height;
        f->log2_width = log2_of(f->width);
        f->log2_height = log2_of(f->height);
        f->rows = a.rows + g_h.next_rows;
        for (DWORD i = 0; i < rows; i++)
            f->rows[i] = i * pitch;
        f->flags = format_flag(s.format);
        if (s.format == RGB555)   // 5-5-5 pixels: the top bit clear
            for (DWORD i = 0; i < bytes; i += 2)
                f->pixels[i + 1] &= 0x7f;
    }
    g_h.next_object += (osize + 15) & ~15u;
    g_h.next_rows += rows;
    g_h.next_pixels += total;
    return g_h.nframes++;
}

inline FrameData *frame(int side, int i)
{
    return reinterpret_cast<FrameData *>(g_h.arena[side].objects + g_h.frames[i].object);
}

// A COffsetClippedProxyFrame on frame `target` (as the constructor 0x5247c0 wires it) with an offset and a
// clip rectangle; on both sides. Returns its object offset.
inline DWORD make_offset_clipped(int target, int dx, int dy, const Rect &clip, bool recording = true)
{
    const DWORD off = g_h.next_object;
    if (off + sizeof(OffsetClippedProxyFrameData) > ARENA_OBJECTS)
        return 0xffffffffu;
    for (int side = 0; side < 2; side++) {
        auto *p = reinterpret_cast<OffsetClippedProxyFrameData *>(g_h.arena[side].objects + off);
        FrameData *f = frame(side, target);
        memset(p, 0, sizeof *p);
        p->proxy.vftable = vtable_for(2, VT_OFFSET_CLIPPED, recording);
        p->proxy.target = f;
        p->offset.proxy.vftable = vtable_for(3, VT_OFFSET_FRAME, recording);
        p->offset.proxy.target = &p->clip;
        p->offset.dx = dx;
        p->offset.dy = dy;
        p->clip.proxy.vftable = vtable_for(4, VT_CLIP_FRAME, recording);
        p->clip.proxy.target = f;
        p->clip.clip = clip;
    }
    g_h.next_object += (sizeof(OffsetClippedProxyFrameData) + 15) & ~15u;
    return off;
}
template <typename T> inline T *object(int side, DWORD off) { return reinterpret_cast<T *>(g_h.arena[side].objects + off); }

// A pixel of frame i on a side (x, y inside the buffer: pitch, rows)
inline DWORD pixel(int side, int i, DWORD x, DWORD y)
{
    const FrameInfo &fi = g_h.frames[i];
    const BYTE *p = g_h.arena[side].pixels + fi.buffer + (y * fi.pitch + x) * fi.bpp;
    return fi.bpp == 1 ? p[0] : fi.bpp == 2 ? (DWORD)(p[0] | p[1] << 8) : (DWORD)(p[0] | p[1] << 8 | p[2] << 16);
}

// ---- The CPU's MMX flag and the 16-bit tables, set for a run ---------------------------------------------------

constexpr DWORD CPU_MMX_FLAG = 0x6cfdc4, CPU_MMX_DETECTED = 0x6cfdc5, PIXEL16_TABLES_BUILT = 0x630578;

// Cpu_HasMmx (0x5156b0) answers the flag once it is detected: set both, put them back afterwards
struct MmxSwitch {
    BYTE saved[2];
    bool active = false;
    void set(bool mmx)
    {
        if (!active) {
            memcpy(saved, at<BYTE *>(CPU_MMX_FLAG), 2);
            active = true;
        }
        *at<BYTE *>(CPU_MMX_FLAG) = mmx ? 1 : 0;
        *at<BYTE *>(CPU_MMX_DETECTED) = 1;
    }
    void restore()
    {
        if (active)
            memcpy(at<BYTE *>(CPU_MMX_FLAG), saved, 2);
        active = false;
    }
    ~MmxSwitch() { restore(); }
};
// The real flag (whether this CPU has MMX), without changing anything when it is detected
inline bool cpu_has_mmx() { return *at<BYTE *>(CPU_MMX_DETECTED) ? *at<BYTE *>(CPU_MMX_FLAG) != 0 : false; }

// Entry 0x80 of the blue table, the test "tables built" of Pixel16_FromRGB and its 5-5-5 twin
struct Pixel16Tables {
    DWORD saved;
    bool active = false;
    void set(bool built)
    {
        if (!active) {
            saved = *at<DWORD *>(PIXEL16_TABLES_BUILT);
            active = true;
        }
        *at<DWORD *>(PIXEL16_TABLES_BUILT) = built ? saved : 0;
    }
    void restore()
    {
        if (active)
            *at<DWORD *>(PIXEL16_TABLES_BUILT) = saved;
        active = false;
    }
    ~Pixel16Tables() { restore(); }
};

// ---- A module's hooks out (side 0) and in ---------------------------------------------------------------------

struct ModuleHooks {
    static constexpr int MAX = 128;   // the biggest frame module (frame_proxy) has 98 hooks
    const char *name;
    int (*table)(Hook *);
    Hook hooks[MAX];
    BYTE *sites[MAX];
    BYTE saved[MAX][5];
    int n = 0;
    bool fits = true;
    void out()
    {
        n = 0;
        const int count = table(nullptr);
        fits = hook_sites_fit(name, count, MAX);
        if (!fits)
            return;
        table(hooks);
        for (int i = 0; i < count; i++) {
            BYTE *site = at<BYTE *>(hooks[i].address);
            if (original_address(hooks[i].address) == site)
                continue;   // not hooked (Disable=)
            sites[n] = site;
            memcpy(saved[n], site, 5);
            write_code(site, hooks[i].expected);
            n++;
        }
    }
    void in()
    {
        for (int i = 0; i < n; i++)
            write_code(sites[i], saved[i]);
        n = 0;
    }
};

// ---- Running both sides and comparing -------------------------------------------------------------------------

struct SideResult {
    DWORD eax;
    unsigned rows, pixels;   // hashes
};

inline void state(int side, SideResult &r)
{
    r.rows = fnv(2166136261u, g_h.arena[side].rows, g_h.next_rows * 4);
    r.pixels = fnv(2166136261u, g_h.arena[side].pixels, g_h.next_pixels);
}

// The objects of both sides dword by dword: equal raw values, else both mapped (pointers into the own
// arena by offset); the first differing offset or -1
inline int objects_difference()
{
    const DWORD *a = reinterpret_cast<const DWORD *>(g_h.arena[0].objects), *b = reinterpret_cast<const DWORD *>(g_h.arena[1].objects);
    for (DWORD i = 0; i < (g_h.next_object + 3) / 4; i++)
        if (a[i] != b[i] && map_value(0, a[i]) != map_value(1, b[i]))
            return (int)i * 4;
    return -1;
}

// Room for a case's own objects (rectangles, colours) next to the frames: the same offset on both sides
inline DWORD alloc_object(DWORD n)
{
    const DWORD off = g_h.next_object;
    if (off + n > ARENA_OBJECTS)
        return 0xffffffffu;
    g_h.next_object += (n + 15) & ~15u;
    return off;
}

// The first differing pixels and the bounding box of the differences, per frame; differing guard bytes
inline void log_pixel_differences()
{
    int shown = 0;
    for (int i = 0; i < g_h.nframes; i++) {
        const FrameInfo &fi = g_h.frames[i];
        const BYTE *a = g_h.arena[0].pixels, *b = g_h.arena[1].pixels;
        const DWORD start = fi.buffer - GUARD, end = fi.buffer + fi.bytes + GUARD;
        for (DWORD o = start; o < end && o < g_h.next_pixels; o++)
            if ((o < fi.buffer || o >= fi.buffer + fi.bytes) && a[o] != b[o]) {
                zax::log("    frame %d: guard byte %s%u differs: %02x / %02x", i, o < fi.buffer ? "-" : "+",
                         o < fi.buffer ? fi.buffer - o : o - (fi.buffer + fi.bytes), a[o], b[o]);
                break;
            }
        int x0 = 0x7fffffff, y0 = 0x7fffffff, x1 = -1, y1 = -1, count = 0;
        const DWORD h = fi.height ? fi.height : 1;
        for (DWORD y = 0; y < h; y++)
            for (DWORD x = 0; x < fi.pitch; x++) {
                const DWORD p0 = pixel(0, i, x, y), p1 = pixel(1, i, x, y);
                if (p0 == p1)
                    continue;
                if (shown < 4) {
                    zax::log("    frame %d (%u, %u): original %x, reimpl %x", i, x, y, p0, p1);
                    shown++;
                }
                count++;
                x0 = (int)x < x0 ? (int)x : x0;
                y0 = (int)y < y0 ? (int)y : y0;
                x1 = (int)x > x1 ? (int)x : x1;
                y1 = (int)y > y1 ? (int)y : y1;
            }
        if (count)
            zax::log("    frame %d: %d pixels differ, box (%d, %d)-(%d, %d), pitch %u", i, count, x0, y0, x1, y1, fi.pitch);
    }
}

inline void log_call_difference()
{
    const CallLog &a = g_h.calls[0], &b = g_h.calls[1];
    for (int i = 0; i < 32 && i < a.count && i < b.count; i++)
        if (a.first[i][0] != b.first[i][0] || a.first[i][1] != b.first[i][1]) {
            zax::log("    call %d: slot %u on %x / slot %u on %x", i, a.first[i][0], a.first[i][1], b.first[i][0], b.first[i][1]);
            return;
        }
    zax::log("    calls: %d / %d", a.count, b.count);
}

inline void stack_bounds()
{
    __asm__("movl %%fs:4, %0\n\tmovl %%fs:8, %1" : "=r"(g_h.stack_base), "=r"(g_h.stack_limit));
}

// Runs call(side) on both sides (side 0 with the module's hooks out) and compares eax, the call log, the
// frame objects (with the case's own objects), the row tables and all pixel memory; `what` names the case
// in the log. Both sides get the same arenas, built before by make_frame and alloc_object. Returns whether
// both sides were the same.
template <typename F> inline bool run_sides(Tally &t, ModuleHooks &hooks, const char *what, F call)
{
    stack_bounds();
    SideResult r[2];
    for (int side = 0; side < 2; side++) {
        g_h.calls[side] = {2166136261u, 0, {}};
        if (side == 0)
            hooks.out();
        g_h.logging_side = side;
        r[side].eax = call(side);
        g_h.logging_side = -1;
        if (side == 0)
            hooks.in();
        state(side, r[side]);
    }
    const bool eax_same = r[0].eax == r[1].eax || map_value(0, r[0].eax) == map_value(1, r[1].eax);
    const bool calls_same = g_h.calls[0].hash == g_h.calls[1].hash && g_h.calls[0].count == g_h.calls[1].count;
    const int object_diff = objects_difference();
    const bool same = hooks.fits && eax_same && calls_same && object_diff < 0 && r[0].rows == r[1].rows && r[0].pixels == r[1].pixels;
    char d[220];
    wsprintfA(d, "[%.90s: eax %x/%x, calls %d/%d, objects differ at %d, rows %x/%x, pixels %x/%x]", what, r[0].eax, r[1].eax,
              g_h.calls[0].count, g_h.calls[1].count, object_diff, r[0].rows, r[1].rows, r[0].pixels, r[1].pixels);
    const int before = t.mismatches;
    t.check(same, "", d);
    if (!same && before < 10) {
        if (!calls_same)
            log_call_difference();
        if (r[0].pixels != r[1].pixels)
            log_pixel_differences();
    }
    return same;
}

// ---- Real sources: frames of the running level (read only) ----------------------------------------------------

enum SourceKind { SRC_SPRITE, SRC_FONT, SRC_IMAGE, SRC_TEXTURE, SRC_WORLD, SRC_KINDS };
inline const char *source_kind_name(int k)
{
    static const char *const names[SRC_KINDS] = {"sprite", "font", "image", "texture", "world"};
    return names[k];
}

struct Source {
    const FrameData *frame;
    int kind;
};

inline bool kind_of(const void *obj, DWORD class_var)
{
    const CClassInfo *want = *at<CClassInfo **>(class_var);
    for (const CClassInfo *c = geo::class_of(obj); c && want; c = c->parent)
        if (c == want)
            return true;
    return false;
}

// A compressed frame (flags 2 or 4, the RLE frames of the .frm files, zax-formats/docs/FORMATS.md 5): five blocks in the
// order of the file, each with a table of `height` dwords (offsets into the block); a block starts with its
// size in bytes (slot 21 0x4aca00 adds those sizes): +0x0c/+0x18 the pixels, +0x1c/+0x20 the extra block,
// +0x24/+0x28, +0x2c/+0x30 the alpha block, +0x34/+0x38 the edge block; empty blocks are 0.
constexpr DWORD MAX_BLOCK = 0x20000;
inline const BYTE *block_data(const FrameData *f, int b) { return static_cast<const BYTE *>(b ? f->blocks[2 * b - 2] : f->pixels); }
inline const DWORD *block_table(const FrameData *f, int b) { return b ? static_cast<const DWORD *>(f->blocks[2 * b - 1]) : f->rows; }
// The bytes of all blocks, 0 when one of them or its table cannot be read whole
inline DWORD compressed_bytes(const FrameData *f)
{
    DWORD total = 0;
    for (int b = 0; b < 5; b++) {
        const BYTE *d = block_data(f, b);
        const DWORD *t = block_table(f, b);
        if (!d)
            continue;
        if (!geo::readable(d, 4))
            return 0;
        const DWORD n = *reinterpret_cast<const DWORD *>(d);
        if (n < 4 || n > MAX_BLOCK || !geo::readable(d, n) || (t && !geo::readable(t, f->height * 4u)))
            return 0;
        total += (n + 3) & ~3u;
    }
    return total;
}

// A frame of 8 or 16 bits with pixels, by its vtable (CFrame and the classes derived from it in
// docs/classes.csv: CDIBFrame, CGroundTextureFrame, CScreenFrame); cheap, for counting the candidates
constexpr DWORD FRAME_VTABLES[4] = {VT_CFRAME, VT_DIB_FRAME, 0x5f7954, 0x5ff360};
inline bool frame_candidate(const void *p)
{
    if (ptrv(p) < 0x10000 || (ptrv(p) & 3))
        return false;
    const FrameData *f = static_cast<const FrameData *>(p);
    bool known = false;
    for (DWORD vt : FRAME_VTABLES)
        known = known || f->vftable == vt;
    const DWORD depth = f->flags & F_DEPTH;
    return known && (depth == F_8 || depth == F_16) && f->width && f->height && f->pixels;
}

// A frame whose memory the harness can read whole: a CFrame of 8 or 16 bits with a row table and pixels
// (compressed frames: the object and their blocks with their tables)
inline bool usable_frame(const void *p)
{
    if (ptrv(p) < 0x10000 || (ptrv(p) & 3) || !geo::readable(p, sizeof(FrameData)) || !kind_of(p, CFRAME_CLASS))
        return false;
    const FrameData *f = static_cast<const FrameData *>(p);
    const DWORD depth = f->flags & F_DEPTH;
    if ((depth != F_8 && depth != F_16) || !f->width || !f->height || !f->pixels)
        return false;
    if (f->flags & F_COMPRESSED)
        return compressed_bytes(f) != 0;
    const DWORD h = f->height;
    if (!f->rows || !geo::readable(f->rows, (h < 2 ? 2 : h) * 4))
        return false;
    const DWORD last = f->rows[h - 1] + f->width;
    return last >= f->width && geo::readable(f->pixels, last * (depth == F_8 ? 1 : 2));
}

struct Sources {
    static constexpr int MAX = 128;
    Source list[MAX];
    int n = 0;
    int found[SRC_KINDS] = {}, compressed[SRC_KINDS] = {};
};

// The candidates of one kind in level order; visit(frame) for each usable frame
template <typename V> inline void each_sequence_frame(const void *seq, V &visit)
{
    if (!geo::readable(seq, 0x38) || !kind_of(seq, CSEQUENCE_CLASS))
        return;
    const BYTE *s = static_cast<const BYTE *>(seq);
    void *const *frames = *reinterpret_cast<void *const *const *>(s + 8);
    const DWORD count = *reinterpret_cast<const DWORD *>(s + 0x28);
    if (!frames || count > 4096 || !geo::readable(frames, count * 4))
        return;
    for (DWORD i = 0; i < count; i++)
        if (frames[i] && geo::readable(frames[i], sizeof(FrameData)) && frame_candidate(frames[i]))
            visit(static_cast<const FrameData *>(frames[i]));
}

// The loaded objects of a definition list (records of 0x80 bytes at +0x30, count +0x34, the object +0x78)
template <typename V> inline void each_list_object(DWORD list, V visit)
{
    const BYTE *records = *at<BYTE **>(list + 0x30);
    const DWORD count = records ? *at<DWORD *>(list + 0x34) : 0;
    if (!records || count > 20000 || !geo::readable(records, count * 0x80))
        return;
    for (DWORD i = 0; i < count; i++) {
        void *obj = *reinterpret_cast<void *const *>(records + i * 0x80 + 0x78);
        if (obj && geo::readable(obj, 8))
            visit(obj);
    }
}

// The definition lists World::LoadGameData fills ("Fonts" 0x6be1e0, then 0x6bf9b8 and 0x6bf850 for "Frame
// Overlay Options" and "Resources - Frames"; their objects are checked by class), the models and the
// ground textures
constexpr DWORD MODEL_LIST = 0x6cfdd8, FONT_LIST = 0x6be1e0, FRAME_LISTS[2] = {0x6bf9b8, 0x6bf850}, TEXTURE_LIST = 0x6c1d50,
                G_WORLD = 0x6c2080;

// visit(frame, kind) over every candidate frame of the level, in a fixed order
template <typename V> inline void each_source(V visit)
{
    // sprites: the sequences of the loaded models (+0x48 the count, +0x4c the CRotationSequence pointers)
    each_list_object(MODEL_LIST, [&](void *m) {
        if (!kind_of(m, CMODEL_CLASS))
            return;
        const BYTE *mb = static_cast<const BYTE *>(m);
        const DWORD n = *reinterpret_cast<const DWORD *>(mb + 0x48);
        void *const *seqs = *reinterpret_cast<void *const *const *>(mb + 0x4c);
        if (!seqs || n > 512 || !geo::readable(seqs, n * 4))
            return;
        auto v = [&](const FrameData *f) { visit(f, SRC_SPRITE); };
        for (DWORD i = 0; i < n; i++)
            if (seqs[i])
                each_sequence_frame(seqs[i], v);
    });
    // fonts (CFont is a CSequence of glyph frames)
    each_list_object(FONT_LIST, [&](void *o) {
        auto v = [&](const FrameData *f) { visit(f, SRC_FONT); };
        each_sequence_frame(o, v);
    });
    // "Resources - Frames" and the ground textures (CGroundTextureFrame)
    for (DWORD list : FRAME_LISTS)
        each_list_object(list, [&](void *o) {
            if (geo::readable(o, sizeof(FrameData)) && frame_candidate(o))
                visit(static_cast<const FrameData *>(o), SRC_IMAGE);
            auto v = [&](const FrameData *f) { visit(f, SRC_IMAGE); };
            each_sequence_frame(o, v);
        });
    each_list_object(TEXTURE_LIST, [&](void *o) {
        if (geo::readable(o, sizeof(FrameData)) && frame_candidate(o))
            visit(static_cast<const FrameData *>(o), SRC_TEXTURE);
    });
    // the world: zoom and screen frames (+0x2a4, +0x2a8), per layer the fog of war (+0x100 of the save data
    // embedded at +0x3c) and the tile map's elevation and seed frames (+0x2c6c, +0x2cac of the tile map
    // embedded at +0x40 of the save data)
    const BYTE *w = *at<BYTE **>(G_WORLD);
    if (!w || !geo::readable(w, 0x2c0))
        return;
    static const DWORD WORLD_FRAMES[2] = {0x2a4, 0x2a8};
    for (DWORD off : WORLD_FRAMES) {
        void *f = *reinterpret_cast<void *const *>(w + off);
        if (f && geo::readable(f, sizeof(FrameData)) && frame_candidate(f))
            visit(static_cast<const FrameData *>(f), SRC_WORLD);
    }
    static const DWORD TILE_FRAMES[2] = {0x2c6c, 0x2cac};
    void *const *layers = *reinterpret_cast<void *const *const *>(w + 0x2b8 + 4);
    const DWORD nlayers = *reinterpret_cast<const DWORD *>(w + 0x2b8 + 8);
    if (!layers || nlayers > 64 || !geo::readable(layers, nlayers * 4))
        return;
    for (DWORD i = 0; i < nlayers; i++) {
        const BYTE *layer = static_cast<const BYTE *>(layers[i]);
        if (!layer || !geo::readable(layer, 0x40))
            continue;
        const BYTE *save = layer + 0x3c;   // CLayerSaveData embedded (constructor 0x4e6cb0 on +0x3c)
        if (!geo::readable(save, 0x140))
            continue;
        if (frame_candidate(save + 0x100))
            visit(reinterpret_cast<const FrameData *>(save + 0x100), SRC_WORLD);
        const BYTE *tiles = save + 0x40;
        if (geo::readable(tiles, 0x2d00))
            for (DWORD off : TILE_FRAMES)
                if (frame_candidate(tiles + off))
                    visit(reinterpret_cast<const FrameData *>(tiles + off), SRC_WORLD);
    }
}

// The sources of the level, at most `cap` per kind (the first, the last and every k-th of the candidates
// in level order; a picked candidate whose memory cannot be read whole is left out)
inline void collect_sources(Sources &out, int cap)
{
    out.n = 0;
    for (int k = 0; k < SRC_KINDS; k++)
        out.found[k] = out.compressed[k] = 0;
    each_source([&](const FrameData *f, int kind) {
        out.found[kind]++;
        if (f->flags & F_COMPRESSED)
            out.compressed[kind]++;
    });
    int index[SRC_KINDS] = {};
    each_source([&](const FrameData *f, int kind) {
        const int i = index[kind]++;
        if (out.n < Sources::MAX && spread_pick(i, out.found[kind], cap) && usable_frame(f))
            out.list[out.n++] = {f, kind};
    });
}

// A plain source's pixels copied into fake frame i (both sides), the top left of the smaller of both
inline void copy_source(int i, const FrameData *src)
{
    const FrameInfo &fi = g_h.frames[i];
    const DWORD sbpp = (src->flags & F_DEPTH) == F_8 ? 1 : 2;
    if (src->flags & F_COMPRESSED || sbpp != fi.bpp)
        return;
    const DWORD w = src->width < fi.pitch ? src->width : fi.pitch, h = src->height < fi.height ? src->height : fi.height;
    for (int side = 0; side < 2; side++)
        for (DWORD y = 0; y < h; y++)
            memcpy(g_h.arena[side].pixels + fi.buffer + y * fi.pitch * fi.bpp, src->pixels + src->rows[y] * sbpp, w * sbpp);
}

// A compressed source copied whole (object, the five blocks into the pixel arena, their tables into the row
// arena; on both sides). The frame's FrameInfo covers the blocks as bytes (pitch = bytes, height 1).
// Returns the frame index or -1 when it does not fit.
inline int copy_compressed(const FrameData *src)
{
    const DWORD bytes = compressed_bytes(src);
    DWORD tables = 0;
    for (int b = 0; b < 5; b++)
        if (block_data(src, b) && block_table(src, b))
            tables += src->height;
    const DWORD total = (GUARD + bytes + GUARD + 15) & ~15u;
    if (!bytes || g_h.nframes >= (int)MAX_FRAMES || g_h.next_object + sizeof(FrameData) > ARENA_OBJECTS ||
        g_h.next_rows + tables > ARENA_ROWS || g_h.next_pixels + total > ARENA_PIXELS)
        return -1;
    FrameInfo &fi = g_h.frames[g_h.nframes];
    fi = {g_h.next_object, g_h.next_pixels + GUARD, bytes, bytes, 1, 1};
    for (int side = 0; side < 2; side++) {
        Arena &a = g_h.arena[side];
        BYTE *mem = a.pixels + g_h.next_pixels;
        fill_pattern(mem, total, g_h.seed * 31u + (unsigned)g_h.nframes * 7919u);
        FrameData *f = reinterpret_cast<FrameData *>(a.objects + g_h.next_object);
        *f = *src;
        f->vftable = vtable_for(0, VT_CFRAME, true);
        BYTE *at_data = mem + GUARD;
        DWORD *at_table = a.rows + g_h.next_rows;
        for (int b = 0; b < 5; b++) {
            const BYTE *d = block_data(src, b);
            const DWORD *t = block_table(src, b);
            BYTE *nd = nullptr;
            DWORD *nt = nullptr;
            if (d) {
                const DWORD n = *reinterpret_cast<const DWORD *>(d);
                memcpy(at_data, d, n);
                nd = at_data;
                at_data += (n + 3) & ~3u;
            }
            if (d && t) {
                memcpy(at_table, t, src->height * 4u);
                nt = at_table;
                at_table += src->height;
            }
            if (b == 0) {
                f->pixels = nd;
                f->rows = nt;
            } else {
                f->blocks[2 * b - 2] = nd;
                f->blocks[2 * b - 1] = nt;
            }
        }
    }
    g_h.next_object += sizeof(FrameData);
    g_h.next_rows += tables;
    g_h.next_pixels += total;
    return g_h.nframes++;
}

// The game palette (0x633f98: {data, ...}, 256 entries of 3 bytes, blue at +0 and red at +2 as 0x433a90
// reads them), copied; false when it is not loaded
constexpr DWORD GAME_PALETTE = 0x633f98;
inline bool copy_game_palette(BYTE out[768])
{
    const BYTE *data = *at<BYTE **>(GAME_PALETTE);
    if (!data || !geo::readable(data, 768))
        return false;
    memcpy(out, data, 768);
    return true;
}

}  // namespace zax::framec
