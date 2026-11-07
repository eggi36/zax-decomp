// The live objects of the running game for the real-data checks (check_util.h, find_real_objects):
// field_streams, object_base, object_files and object_io write, copy or read back the CObjects that the
// world (0x6c2080) points to.
//
// Why it is done this way (2026-10-31; docs/DECOMPILATION.md section 12, "The real objects of
// field_streams on real x86"). Until then each of those checks read the world's first 0x1000 bytes
// and, behind every pointer in there that was no object, 0x100 bytes more, and took every pointer whose
// vtable named a registered class. The world block has only 0x5c0 bytes, and most of the pointers in it
// point into the pool (pool.h): blocks of 4 to a few dozen bytes inside a chunk of their size class, so
// the 0x100 bytes ran on into the neighbouring blocks of that chunk, whatever they held: live blocks of
// other owners, or blocks freed back to the pool (only their first dword becomes the free-list link, the
// rest stays). Which objects that gave depended on what earlier checks had allocated and freed: on
// Test/Nachbau1 mostly 23 CDialogLine objects (found 0xb8-0xfc bytes behind the pool blocks of world
// +0x3c8 and +0x3cc, in the neighbouring pointer array blocks of the dialogue script), on an ARM run
// with all modules none of them, and on one x86 run (20261031-175551-79bd) a CShootEntityBehavior, a
// CBroadcastEvilAI and a CFidgetSettings, the last of which crashed the game when written.
//
// Now only memory that is allocated is read, and only within its block:
// - a snapshot before the scan: the busy blocks of the C heap (HeapWalk over _crtheap 0x716520, the heap
//   malloc and operator new use on Windows NT; docs/DECOMPILATION.md section 11), the pool's chunks per
//   size class and its free blocks. The checks run in the game thread, so the pool does not change
//   meanwhile; the C heap is locked for the walk (the game's other threads use it too);
// - a live block is a pool block (in a chunk of its class, on a block boundary behind the chunk's link
//   dword, not on the free list) or a busy heap block that is no pool chunk;
// - the world is read within its block, a pointer of it that is no object within the live block it
//   points into, from there to the block's end and at most 0x100 bytes as before;
// - an object is taken only at the start of a live block (or as a static object in the exe's image), so a
//   stale pointer to a freed or reused block gives nothing;
// - optionally the dialogue lines through the place the game keeps them: the dialogue script list
//   (0x62e7e0: records 0x62e810, count 0x62e814; a record's +0x78 the loaded CPropertyList, whose pointer
//   array has its data at +8 and its count at +0xc; dialog_rest_a.cpp, ai_triggers.cpp), lines 0 to
//   count - 1 only, a spread of them when there are more than asked for.
//
// real_player_layer (check_util.h) replaced 26 copies of find_layer in the check modules (2026-10-31), which
// took the first CLayer 0x1000 bytes behind the world or 0x200 bytes behind one of its pointers, whatever
// block that lay in (on Assimilate Cave1 it came through world +4, +0x130, an overrun of world +4's
// block). It reads what World::CurrentLayer 0x4f1050 reads: the layer list and player 0's index into it.
#include "check_util.h"
#include "pool.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT BOOL WINAPI VirtualFree(void *addr, size_t size, DWORD type);
WINIMPORT BOOL WINAPI HeapWalk(HANDLE heap, void *entry);
WINIMPORT BOOL WINAPI HeapLock(HANDLE heap);
WINIMPORT BOOL WINAPI HeapUnlock(HANDLE heap);

namespace zax {

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

struct MemoryInfo {
    void *base, *allocation_base;
    DWORD allocation_protect;
    size_t size;
    DWORD state, protect, type;
};
bool readable(const void *p, DWORD n)
{
    MemoryInfo mi;
    if (ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;   // kernel half, or the range wraps (then the loop below would not run)
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000) {
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0x66))
            return false;
    }
    return true;
}
bool readable4(DWORD p) { return p >= 0x10000 && !(p & 3) && readable(reinterpret_cast<void *>(static_cast<uintptr_t>(p)), 4); }

// The class of an object: vtable slot 2 is a class getter (mov eax, [class variable]) and that variable
// holds one of the registered classes
CClassInfo *class_of_object(const void *p, CClassInfo **cls, int n)
{
    if (ptrv(p) < 0x10000 || (ptrv(p) & 3) || !readable(p, 4))
        return nullptr;
    const DWORD vt = *static_cast<const DWORD *>(p);
    if (vt < 0x5e0000 || vt >= 0x610000 || (vt & 3))
        return nullptr;
    const DWORD getter = *at<DWORD *>(vt + 8);
    if (getter < 0x401000 || getter >= 0x5e0000 || *static_cast<const BYTE *>(original_address(getter)) != 0xa1)
        return nullptr;
    // through the trampoline when the getter is hooked (layer.cpp hooks CLayer's)
    const DWORD var = *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(original_address(getter)) + 1);
    if (var < 0x600000 || var >= 0x720000)
        return nullptr;
    CClassInfo *c = *at<CClassInfo **>(var);
    for (int i = 0; i < n; i++)
        if (cls[i] == c)
            return c;
    return nullptr;
}

// ---- The snapshot ------------------------------------------------------------------------------------

struct HeapEntry {   // PROCESS_HEAP_ENTRY
    void *data;
    DWORD size;
    BYTE overhead, region;
    WORD flags;
    DWORD rest[4];
};
static_assert(sizeof(HeapEntry) == 28);
constexpr WORD HEAP_ENTRY_BUSY = 4;
constexpr DWORD CRT_HEAP_VAR = 0x716520;   // _crtheap
constexpr DWORD WORLD = 0x6c2080, SCRIPT_RECORDS = 0x62e810, SCRIPT_COUNT = 0x62e814, LAYER_CLASS_VAR = 0x6c1d04;
constexpr int MAX_BLOCKS = 0x80000;
constexpr DWORD MEM_RELEASE_ = 0x8000;
struct Snapshot {
    DWORD *heap;     // pairs start, size (busy heap blocks)
    DWORD *chunks;   // pairs start, size class (pool chunks)
    DWORD *free;     // pairs start, 0 (free pool blocks)
    int nheap, nchunks, nfree;
    bool full;       // every list fitted
} g_snap;

void sort_pairs(DWORD *a, int n)   // shell sort by the first of each pair
{
    for (int gap = n / 2; gap > 0; gap /= 2)
        for (int i = gap; i < n; i++) {
            const DWORD v0 = a[2 * i], v1 = a[2 * i + 1];
            int j = i;
            for (; j >= gap && a[2 * (j - gap)] > v0; j -= gap) {
                a[2 * j] = a[2 * (j - gap)];
                a[2 * j + 1] = a[2 * (j - gap) + 1];
            }
            a[2 * j] = v0;
            a[2 * j + 1] = v1;
        }
}
int last_at_or_below(const DWORD *a, int n, DWORD p)   // index of the last pair with start <= p, or -1
{
    int lo = 0, hi = n - 1, r = -1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (a[2 * mid] <= p) {
            r = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return r;
}
bool add(DWORD *a, int &n, DWORD v0, DWORD v1)
{
    if (n == MAX_BLOCKS) {
        g_snap.full = false;
        return false;
    }
    a[2 * n] = v0;
    a[2 * n + 1] = v1;
    n++;
    return true;
}

bool take_snapshot()
{
    Snapshot &s = g_snap;
    s = {};
    s.heap = static_cast<DWORD *>(VirtualAlloc(nullptr, 3 * MAX_BLOCKS * 8, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!s.heap)
        return false;
    s.chunks = s.heap + 2 * MAX_BLOCKS;
    s.free = s.chunks + 2 * MAX_BLOCKS;
    s.full = true;
    HANDLE heap = *at<HANDLE *>(CRT_HEAP_VAR);
    HeapLock(heap);
    HeapEntry e{};
    while (HeapWalk(heap, &e))
        if ((e.flags & HEAP_ENTRY_BUSY) && !add(s.heap, s.nheap, ptrv(e.data), e.size))
            break;
    HeapUnlock(heap);
    ZPool *pool = *at<ZPool **>(POOL_GLOBAL);
    if (pool && pool->classes)
        for (DWORD c = 0; c < pool->num_classes; c++) {
            const ZPoolClass &k = pool->classes[c];
            DWORD seen = 0;
            for (DWORD ch = ptrv(k.chunks); ch && seen++ < 0x100000 && readable4(ch); ch = *at<DWORD *>(ch))
                if (!add(s.chunks, s.nchunks, ch, c))
                    break;
            seen = 0;
            for (DWORD f = ptrv(k.free_list); f && seen++ < 0x200000 && readable4(f); f = *at<DWORD *>(f))
                if (!add(s.free, s.nfree, f, 0))
                    break;
        }
    sort_pairs(s.heap, s.nheap);
    sort_pairs(s.chunks, s.nchunks);
    sort_pairs(s.free, s.nfree);
    return true;
}
void free_snapshot()
{
    if (g_snap.heap)
        VirtualFree(g_snap.heap, 0, MEM_RELEASE_);
    g_snap = {};
}

// The live block p lies in: its start and size; false when p lies in none
bool live_block(DWORD p, DWORD &start, DWORD &size)
{
    const Snapshot &s = g_snap;
    const int h = last_at_or_below(s.heap, s.nheap, p);
    if (h < 0 || p >= s.heap[2 * h] + s.heap[2 * h + 1])
        return false;   // in no busy heap block
    start = s.heap[2 * h];
    size = s.heap[2 * h + 1];
    const int c = last_at_or_below(s.chunks, s.nchunks, start);
    if (c >= 0 && s.chunks[2 * c] == start) {   // a pool chunk: the pool block p lies in
        const ZPoolClass &k = (*at<ZPool **>(POOL_GLOBAL))->classes[s.chunks[2 * c + 1]];
        const DWORD bsize = k.size ? k.size : 4;
        if (p < start + 4 || p - start - 4 >= bsize * k.per_chunk)
            return false;
        start += 4 + (p - start - 4) / bsize * bsize;
        size = bsize;
        const int f = last_at_or_below(s.free, s.nfree, start);
        if (f >= 0 && s.free[2 * f] == start)
            return false;   // freed
    }
    return true;
}
bool live_start(DWORD p)
{
    DWORD start, size;
    return live_block(p, start, size) && start == p;
}
bool in_exe_image(DWORD p) { return p >= 0x400000 && p < IMAGE_END; }

// ---- The scan ----------------------------------------------------------------------------------------

struct Found {
    void **out;
    char (*from)[REAL_FROM];
    int max, n, rejected;
    CClassInfo **cls;
    int ncls;
} g_found;

void take(void *p, const char *from)
{
    Found &f = g_found;
    if (f.n >= f.max || !class_of_object(p, f.cls, f.ncls))
        return;
    if (!in_exe_image(ptrv(p)) && !live_start(ptrv(p))) {
        f.rejected++;
        return;
    }
    for (int i = 0; i < f.n; i++)
        if (f.out[i] == p)
            return;
    if (f.from) {
        DWORD k = 0;
        for (; from[k] && k + 1 < REAL_FROM; k++)
            f.from[f.n][k] = from[k];
        f.from[f.n][k] = 0;
    }
    f.out[f.n++] = p;
}

// The world's members and, one level down, the members of the live blocks they point into that are not
// objects themselves
void scan(const BYTE *block, DWORD size, int depth, DWORD from)
{
    if (!block || !readable(block, size))
        return;
    for (DWORD off = 0; off + 4 <= size && g_found.n < g_found.max; off += 4) {
        void *p = *reinterpret_cast<void *const *>(block + off);
        char where[REAL_FROM];
        if (depth == 0)
            wsprintfA(where, "world +%x", off);
        else
            wsprintfA(where, "world +%x, +%x", from, off);
        if (class_of_object(p, g_found.cls, g_found.ncls)) {
            take(p, where);
            continue;
        }
        DWORD start, bsize;
        if (depth == 0 && ptrv(p) >= 0x10000 && !(ptrv(p) & 3) && !in_exe_image(ptrv(p)) && live_block(ptrv(p), start, bsize)) {
            const DWORD rest = start + bsize - ptrv(p);
            scan(static_cast<const BYTE *>(p), rest < 0x100 ? rest : 0x100, 1, off);
        }
    }
}

// The lines of the loaded dialogue scripts, at most `cap` of them (spread_pick)
void scan_dialogue_lines(int cap)
{
    const BYTE *records = *at<const BYTE **>(SCRIPT_RECORDS);
    const DWORD count = *at<DWORD *>(SCRIPT_COUNT);
    if (!records || count > 0x10000 || !readable(records, count * 0x80))
        return;
    for (int pass = 0, total = 0, index = 0; pass < 2; pass++)   // count, then take
        for (DWORD r = 0; r < count; r++) {
            const BYTE *script = *reinterpret_cast<const BYTE *const *>(records + r * 0x80 + 0x78);
            if (!script || !live_start(ptrv(script)) || !readable(script, 0x10))
                continue;
            const DWORD *lines = *reinterpret_cast<const DWORD *const *>(script + 8);
            const DWORD nlines = *reinterpret_cast<const DWORD *>(script + 0xc);
            if (!lines || nlines > 0x10000 || !readable(lines, nlines * 4))
                continue;
            if (pass == 0) {
                total += nlines;
                continue;
            }
            for (DWORD i = 0; i < nlines; i++, index++)
                if (spread_pick(index, total, cap)) {
                    char where[REAL_FROM];
                    wsprintfA(where, "dialogue script %u line %u", r, i);
                    take(reinterpret_cast<void *>(static_cast<uintptr_t>(lines[i])), where);
                }
        }
}

#ifdef ZAX_COMPARE_SCANS
// The pattern the 26 check modules used before real_player_layer (comparison builds only): the first CLayer
// 0x1000 bytes behind the world or 0x200 bytes behind one of its pointers, whatever block that lies in
const BYTE *old_find_layer(const BYTE *block, DWORD size, int depth, char *path, DWORD from)
{
    const CClassInfo *layer_class = *at<CClassInfo **>(LAYER_CLASS_VAR);
    if (!readable(block, size))
        return nullptr;
    for (DWORD off = 0; off < size; off += 4) {
        const BYTE *p = *reinterpret_cast<const BYTE *const *>(block + off);
        const DWORD v = ptrv(p);
        if (v < 0x10000 || (v & 3) || (v >= 0x400000 && v < IMAGE_END) || v >= 0x80000000u)
            continue;
        if (readable(p, 4) && class_of_object(p, const_cast<CClassInfo **>(&layer_class), 1)) {
            if (depth == 0)
                wsprintfA(path, "world +%x", off);
            else
                wsprintfA(path, "world +%x, +%x", from, off);
            return p;
        }
        if (depth < 1)
            if (const BYTE *l = old_find_layer(p, 0x200, depth + 1, path, off))
                return l;
    }
    return nullptr;
}
#endif

}  // namespace

bool live_blocks_take()
{
    if (take_snapshot())
        return true;
    log("  live blocks: no memory for the heap snapshot");
    return false;
}
void live_blocks_free() { free_snapshot(); }
bool live_block_of(DWORD p, DWORD &start, DWORD &size) { return g_snap.heap && live_block(p, start, size); }
bool live_object_at(const void *p) { return in_exe_image(ptrv(p)) || (g_snap.heap && live_start(ptrv(p))); }
void live_blocks_log(const char *what)
{
    log("  %s: snapshot%s: %d heap blocks, %d pool chunks, %d free pool blocks", what, g_snap.full ? "" : " INCOMPLETE",
        g_snap.nheap, g_snap.nchunks, g_snap.nfree);
}

BYTE *real_player_layer()
{
    BYTE *world = *at<BYTE **>(WORLD);
    CClassInfo *layer_class = *at<CClassInfo **>(LAYER_CLASS_VAR);
    if (!world || !layer_class || !readable(world, 0x2c4))
        return nullptr;
    const DWORD count = *reinterpret_cast<const DWORD *>(world + 0x2c0);
    const DWORD *layers = *reinterpret_cast<const DWORD *const *>(world + 0x2bc);
    if (!count || count > 0x10000 || !layers || !readable(layers, count * 4))
        return nullptr;   // World::CurrentLayer: no layers, no current layer
    void *net = vcall<void *>(world, 44);
    const BYTE *player0 = net ? vcall<const BYTE *>(net, 23, 0) : nullptr;
    if (!player0 || !readable(player0 + 0xdc, 4))
        return nullptr;
    const int index = *reinterpret_cast<const int *>(player0 + 0xdc);
    if (index < 0 || index >= (int)count)
        return nullptr;   // -1 or outside the list: none, as in World::CurrentLayer
    BYTE *layer = reinterpret_cast<BYTE *>(static_cast<uintptr_t>(layers[index]));
    if (!class_of_object(layer, &layer_class, 1))
        return nullptr;
#ifdef ZAX_COMPARE_SCANS
    char path[REAL_FROM] = "-";
    const BYTE *old = old_find_layer(world, 0x1000, 0, path, 0);
    log("  layer compare: old %x (%s), new %x (world +2bc[%d] of %u, player 0 +dc)%s", ptrv(old), path, ptrv(layer), index,
        count, old == layer ? "" : " DIFFERENT");
#endif
    return layer;
}

int find_real_objects(void **out, char (*from)[REAL_FROM], int max, CClassInfo **cls, int n, int dialogue_lines)
{
    g_found = {out, from, max, 0, 0, cls, n};
    if (!take_snapshot()) {
        log("  real objects: no memory for the heap snapshot");
        return 0;
    }
    BYTE *world = *at<BYTE **>(WORLD);
    DWORD start = 0, size = 0;
    if (world && live_block(ptrv(world), start, size) && start == ptrv(world))
        scan(world, size < 0x1000 ? size : 0x1000, 0, 0);
    if (dialogue_lines > 0)
        scan_dialogue_lines(dialogue_lines);
    log("  real objects: %d (world block %x bytes; snapshot%s: %d heap blocks, %d pool chunks, %d free pool blocks; %d "
        "pointers with a registered vtable not taken: no live block starts there)",
        g_found.n, start == ptrv(world) ? size : 0, g_snap.full ? "" : " INCOMPLETE", g_snap.nheap, g_snap.nchunks,
        g_snap.nfree, g_found.rejected);
    free_snapshot();
    return g_found.n;
}

}  // namespace zax
