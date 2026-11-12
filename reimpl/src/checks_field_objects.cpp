// Checks of the object, pointer array, new[] array and class pointer slots (field_objects.cpp) against
// the original, in the game thread, on real descriptors and real values: every field of these kinds
// in the objects of the running game (and the objects they own, two levels down) is written with its
// slot 23 / 26 and read back with slot 19 / 29 into a fresh block of the owner's size; the text the
// read value gives with slot 23 (or 26) is compared between the sides. The texts are also varied
// (count lines removed or renamed, counts changed, unknown element names, nested blocks, no reader),
// and the owned object slot 19 runs into its three fatal errors with the box and exit redirected.
// Side 0 runs with the hooks of all object modules taken out (checks_object_common.h).
#include "checks_object_common.h"
#include "field_objects.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT DWORD WINAPI GetTickCount(void);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);
WINIMPORT char *WINAPI lstrcpynA(char *dst, const char *src, int n);

extern "C" int __cdecl zax_call_escapable3(void *fn, void *self, DWORD a1, DWORD a2, DWORD a3);

namespace zax {

namespace {
using namespace objcheck;

constexpr DWORD VT_OBJECT_FIELD = 0x5ecbf4, VT_ARRAY_FIELD = 0x5ecd6c, VT_LIST_FIELD = 0x5eca78, VT_CLASS_FIELD = 0x5ecb38;

void *registry() { return at<void *(__cdecl *)()>(0x415d20)(); }
void *create(const char *name) { return at<void *(THISCALL *)(void *, const char *, int, int)>(0x415c00)(registry(), name, 0, -1); }
CClassInfo *class_of(const void *o) { return vcall<CClassInfo *>(o, 2); }
DWORD is_derived(CClassInfo *a, CClassInfo *b) { return at<DWORD(THISCALL *)(CClassInfo *, CClassInfo *)>(0x4155f0)(a, b) & 0xff; }

bool g_trace = false;  // log each field and phase (to find where the game ends)

// The function on side 0 through its trampoline, on side 1 through the hooked exe address. The hooks
// of this module are not taken out: some of these functions also run in other threads, and putting
// their first bytes back during the checks ended the game later (runs 20261023-201316-7306 to
// -202751-ea13, in the next module's checks writing the running level)
template <typename F> F pick(DWORD address) { return g_side == 0 ? original<F>(address) : at<F>(address); }
void *pick_address(DWORD address) { return g_side == 0 ? original_address(address) : at<void *>(address); }
template <typename R, typename... A> R vslot(CFieldDesc *d, int n, A... a) { return pick<R(THISCALL *)(CFieldDesc *, A...)>(slot(d, n))(d, a...); }

template <typename F> void both(F body)
{
    for (int s = 0; s < 2; s++) {
        g_side = s;
        g_nlog[s] = 0;
        g_loghash[s] = 2166136261u;
        body(s);
        if (g_trace)
            log("      side %d done", s);
    }
}

// Slot 23 (text) or 26 (binary) of a descriptor on an owner, into out
void write_value(CFieldDesc *d, BYTE *owner, Bytes &out, int slot)
{
    CObjectWriter w;
    open_stream(&w, 2, 1, nullptr, 0, 0);
    vslot<DWORD>(d, slot, owner, &w);
    const MemoryStream *ms = static_cast<const MemoryStream *>(w.stream);
    store(out, ms->data, ms->data ? ms->size : 0);
    close_stream(&w);
}

Bytes g_value, g_rest, g_out[2], g_bin;
char g_text[2][0x800];
DWORD g_cases, g_bytes;

// The first line of a slot 23 text (the value on the field line) and the rest (what the reader reads)
void split(const Bytes &v, char *first, Bytes &rest)
{
    DWORD i = 0;
    while (i < v.size && v.data[i] != '\r' && v.data[i] != '\n')
        i++;
    const DWORD n = i < 0x7ff ? i : 0x7ff;
    memcpy(first, v.data, n);
    first[n] = 0;
    if (i < v.size && v.data[i] == '\r')
        i++;
    if (i < v.size && v.data[i] == '\n')
        i++;
    rest.size = 0;
    if (i < v.size)
        append(rest, v.data + i, v.size - i);
}

// Fresh owner blocks of each side
BYTE *g_block[2];
DWORD g_block_size;
void new_blocks(DWORD size)
{
    g_block_size = (size + 0xfff) & ~0xfffu;
    for (int s = 0; s < 2; s++)
        g_block[s] = static_cast<BYTE *>(VirtualAlloc(nullptr, g_block_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
}
void free_blocks()
{
    for (int s = 0; s < 2; s++)
        if (g_block[s])
            VirtualFree(g_block[s], 0, 0x8000);
}
// A fresh block as the game prepares an object before reading it: zeroed, slot 6 (the constructor's
// init) and slot 10 (the default, through the object's slot 19)
void prepare(CFieldDesc *d, BYTE *block)
{
    memset(block, 0, g_block_size);
    vcall<DWORD>(d, 6, block);
    vcall<DWORD>(d, 10, block);
}
// The value in the block released with the descriptor's slot 8 (owned objects deleted, data back)
void free_value(CFieldDesc *d, BYTE *block) { vcall<DWORD>(d, 8, block); }

// The fatal errors of the owned object slot 19: box and exit redirected
CodePatch g_box_patch, g_exit_patch;
unsigned g_box_hash;
void __fastcall record_box(const char *caption, const char *text)
{
    capture_box(caption, text);
    g_box_hash = fnv_text(text_hash(caption), text);
}

// Result tag: slot 19 returned the address of the owned object it deleted
constexpr DWORD DELETED_OBJECT = 0x7fff0002;
DWORD g_deleted_raw[2];   // the deleted object's address per side (evidence: how often the sides' blocks differ)
int g_deleted_owned, g_deleted_differ;

// Slot 19 on the block of side s with the text and the rest as the reader; returns the result
// (al) and records the reader's position and the box
DWORD read_text_value(CFieldDesc *d, int s, const char *text, const Bytes &rest, bool with_reader, DWORD &position, unsigned &box)
{
    lstrcpyA(g_text[s], text);
    CObjectWriter w;
    open_stream(&w, 4, 1, rest.data, rest.size, 0);
    g_box_hash = 0;
    // the owned object before the call: for "" slot 19 deletes it and returns the deleting destructor's
    // eax, which is the object's address (e.g. 0x58c39c "mov eax, esi"), a pool block whose low byte
    // differs when the two sides get different blocks (run 20261028-114701-c22b, CChaseInvestigateAI's
    // "Investigate AI": al 0xec / 0x20); it counts by what it is
    const DWORD before = d->vftable == VT_OBJECT_FIELD ? *reinterpret_cast<DWORD *>(g_block[s] + d->offset) : 0;
    patch_jump(g_box_patch, 0x4d24c0, (void *)&record_box);
    patch_jump(g_exit_patch, 0x5d0e36, (void *)&zax_escape_exit);
    const DWORD r = (DWORD)zax_call_escapable3(pick_address(slot(d, 19)), d, ptrv(g_block[s]), ptrv(g_text[s]), with_reader ? ptrv(&w) : 0);
    unpatch(g_exit_patch);
    unpatch(g_box_patch);
    box = r == 0x7fff0001 ? g_box_hash ^ zax_exit_code : 0;
    position = static_cast<const MemoryStream *>(w.stream)->position;
    close_stream(&w);
    if (r == 0x7fff0001)
        return r;
    if (before && r == before) {
        g_deleted_raw[s] = r;
        return DELETED_OBJECT;
    }
    return r & 0xff;
}

void report(Tally &t, CFieldDesc *d, const char *what, DWORD a, DWORD b)
{
    const bool same = same_bytes(g_out[0], g_out[1]) && a == b;
    char m[200];
    wsprintfA(m, "[%.40s %.30s: size %u/%u, %x/%x]", d->name.c_str(), what, g_out[0].size, g_out[1].size, a, b);
    t.check(same, "", m);
    if (!same) {
        DWORD i = 0;
        while (i < g_out[0].size && i < g_out[1].size && g_out[0].data[i] == g_out[1].data[i])
            i++;
        for (int s = 0; s < 2; s++) {
            char line[100];
            DWORD k = 0;
            for (DWORD j = i >= 40 ? i - 40 : 0; j < g_out[s].size && k < 90; j++, k++)
                line[k] = g_out[s].data[j] >= 32 && g_out[s].data[j] < 127 ? (char)g_out[s].data[j] : '|';
            line[k] = 0;
            log("    side %d at %u: %s", s, i, line);
        }
    }
    g_cases++;
    g_bytes += g_out[0].size;
}

// One text case: slot 19 into both blocks, then slot 23 of the result
void text_case(Tally &t, CFieldDesc *d, const char *text, const Bytes &rest, bool with_reader, const char *what, DWORD mask = 0xffffffff)
{
    DWORD r[2], pos[2];
    unsigned box[2];
    both([&](int s) {
        prepare(d, g_block[s]);
        if (g_trace)
            log("      side %d %s: read", s, what);
        r[s] = read_text_value(d, s, text, rest, with_reader, pos[s], box[s]);
        if (g_trace)
            log("      side %d: write", s);
        write_value(d, g_block[s], g_out[s], 23);
        if (g_trace)
            log("      side %d: free (count %u)", s, *reinterpret_cast<DWORD *>(g_block[s] + d->offset));
        free_value(d, g_block[s]);
        if (g_trace)
            log("      side %d: freed", s);
    });
    for (int s = 0; s < 2; s++)
        if (r[s] != DELETED_OBJECT)
            r[s] &= mask;
    if (r[0] == DELETED_OBJECT && r[1] == DELETED_OBJECT) {
        g_deleted_owned++;
        if (g_deleted_raw[0] != g_deleted_raw[1] && g_deleted_differ++ < 3)
            log("    %s %s: deleted owned object at %x / %x", d->name.c_str(), what, g_deleted_raw[0], g_deleted_raw[1]);
    }
    report(t, d, what, r[0] ^ pos[0] << 8 ^ box[0], r[1] ^ pos[1] << 8 ^ box[1]);
}

const char *const VARIANT_NAMES[8] = {"text", "without count", "other count name", "count + 1", "count - 1", "unknown names",
                                      "nested block", "junk lines"};

// Variants of an array text: 0 as it is, 1 without the count line, 2 the other count name, 3 count + 1,
// 4 count - 1, 5 element names replaced, 6 a nested block after "{", 7 an empty line and junk lines
void variant(const Bytes &src, Bytes &out, int v)
{
    out.size = 0;
    DWORD i = 0;
    bool first_brace = true;
    while (i < src.size) {
        DWORD e = i;
        while (e < src.size && src.data[e] != '\n')
            e++;
        if (e < src.size)
            e++;
        const char *line = reinterpret_cast<const char *>(src.data + i);
        const DWORD n = e - i;
        DWORD k = 0;
        while (k < n && (line[k] == '\t' || line[k] == ' '))
            k++;
        const bool count_line = n - k > 11 && (memcmp(line + k, "Item Count=", 11) == 0 || (n - k > 12 && memcmp(line + k, "Array Count=", 12) == 0));
        if (count_line && v >= 1 && v <= 4) {
            if (v == 1) {
                i = e;
                continue;
            }
            const bool item = memcmp(line + k, "Item", 4) == 0;
            int count = crt_atoi(line + k + (item ? 11 : 12));
            char buf[64];
            const char *name = (v == 2) != item ? "Item Count=" : "Array Count=";
            wsprintfA(buf, "%s%d\r\n", name, v == 3 ? count + 1 : v == 4 ? count - 1 : count);
            append(out, line, k);
            append_text(out, buf);
            i = e;
            continue;
        }
        if (v == 5 && !count_line) {
            DWORD eq = k;
            while (eq < n && line[eq] != '=')
                eq++;
            if (eq < n && eq > k && line[k] != '{' && line[k] != '}') {
                append(out, line, k);
                append_text(out, "Bogus Name");
                append(out, line + eq, n - eq);
                i = e;
                continue;
            }
        }
        append(out, line, n);
        if (first_brace && k < n && line[k] == '{') {
            first_brace = false;
            if (v == 6)
                append_text(out, "\t{\r\n\t\tInner=1\r\n\t}\r\n");
            if (v == 7)
                append_text(out, "\r\n\tno equals sign\r\n\t=\r\n");
        }
        i = e;
    }
}

// ---- Collecting the owners -------------------------------------------------------------------------

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
// The owners (2026-10-31; docs/DECOMPILATION.md section 12, "The real objects of field_streams on real x86"):
// the objects of registered classes find_real_objects takes from the world (the world read within its block,
// a pointer followed into the live block it points into up to that block's end, an object only at the start
// of a live block or static), then a spread of the level's parts (grid_parts), each followed by the objects
// it owns, two levels down. An owned object is
// taken only where a live block starts that holds it whole (the class's size; static objects as they are),
// an element of a new[] array only inside one live block, a pointer array's slot only inside one. Until then
// the world's first 0x1000 bytes (the world block has 0x5c0) and 0x100 bytes behind each of its pointers
// were scanned whatever block they lay in, the pattern that made field_streams take stale objects.
constexpr int MAX_OWNERS = 160;
void *g_owners[MAX_OWNERS];
int g_nowners, g_rejected_owned;

// Whether `bytes` from p lie in one live block (at its start when at_start), or p is in the exe's image
bool live_whole(const void *p, DWORD bytes, bool at_start)
{
    if (ptrv(p) >= 0x400000 && ptrv(p) < IMAGE_END)
        return true;   // a static object
    DWORD start, size;
    if (!live_block_of(ptrv(p), start, size) || (at_start && start != ptrv(p)))
        return false;
    return ptrv(p) - start <= size && bytes <= size - (ptrv(p) - start);
}

void add_owner(void *p, CClassInfo **cls, int n, int depth, bool embedded);
void add_owned(void *owner, CClassInfo **cls, int n, int depth)
{
    CClassInfo *c = class_of(owner);
    for (DWORD f = 0; f < c->num_fields; f++) {
        CFieldDesc *d = c->fields[f];
        BYTE *o = static_cast<BYTE *>(owner);
        if (d->vftable == VT_OBJECT_FIELD) {
            add_owner(*reinterpret_cast<void **>(o + d->offset), cls, n, depth + 1, false);
        } else if (d->vftable == VT_ARRAY_FIELD) {
            auto *a = static_cast<CObjectArrayFieldDesc *>(d);
            if (a->element->vftable != VT_OBJECT_FIELD)
                continue;
            const DWORD count = *reinterpret_cast<DWORD *>(o + d->offset);
            BYTE *data = *reinterpret_cast<BYTE **>(o + a->f30);
            for (DWORD i = 0; i < count && i < 8 && data; i++) {
                BYTE *slot = data + a->element_f3c * i + a->element->offset;
                if (!live_whole(slot, 4, false)) {
                    g_rejected_owned++;
                    continue;
                }
                add_owner(*reinterpret_cast<void **>(slot), cls, n, depth + 1, false);
            }
        } else if (d->vftable == VT_LIST_FIELD) {
            auto *l = static_cast<CListFieldDesc *>(d);
            const DWORD count = *reinterpret_cast<DWORD *>(o + d->offset);
            BYTE *data = *reinterpret_cast<BYTE **>(o + l->data_offset);
            for (DWORD i = 0; i < count && i < 4 && data; i++)
                add_owner(data + l->element_size * i, cls, n, depth + 1, true);
        }
    }
}
void add_owner(void *p, CClassInfo **cls, int n, int depth, bool embedded)
{
    if (depth > 2 || g_nowners >= MAX_OWNERS)
        return;
    CClassInfo *c = class_of_object(p, cls, n);
    if (!c)
        return;
    for (int i = 0; i < g_nowners; i++)
        if (g_owners[i] == p)
            return;
    if (!live_whole(p, c->size, !embedded)) {
        g_rejected_owned++;
        return;
    }
    g_owners[g_nowners++] = p;
    add_owned(p, cls, n, depth);
}

#ifdef ZAX_COMPARE_SCANS
// The scan before 2026-10-31 (comparison builds only)
void old_add_owner(void *p, CClassInfo **cls, int n, int depth);
void old_add_owned(void *owner, CClassInfo **cls, int n, int depth)
{
    CClassInfo *c = class_of(owner);
    for (DWORD f = 0; f < c->num_fields; f++) {
        CFieldDesc *d = c->fields[f];
        BYTE *o = static_cast<BYTE *>(owner);
        if (d->vftable == VT_OBJECT_FIELD) {
            old_add_owner(*reinterpret_cast<void **>(o + d->offset), cls, n, depth + 1);
        } else if (d->vftable == VT_ARRAY_FIELD) {
            auto *a = static_cast<CObjectArrayFieldDesc *>(d);
            if (a->element->vftable != VT_OBJECT_FIELD)
                continue;
            const DWORD count = *reinterpret_cast<DWORD *>(o + d->offset);
            BYTE *data = *reinterpret_cast<BYTE **>(o + a->f30);
            for (DWORD i = 0; i < count && i < 8 && data; i++)
                old_add_owner(*reinterpret_cast<void **>(data + a->element_f3c * i + a->element->offset), cls, n, depth + 1);
        } else if (d->vftable == VT_LIST_FIELD) {
            auto *l = static_cast<CListFieldDesc *>(d);
            const DWORD count = *reinterpret_cast<DWORD *>(o + d->offset);
            BYTE *data = *reinterpret_cast<BYTE **>(o + l->data_offset);
            for (DWORD i = 0; i < count && i < 4 && data; i++)
                old_add_owner(data + l->element_size * i, cls, n, depth + 1);
        }
    }
}
void old_add_owner(void *p, CClassInfo **cls, int n, int depth)
{
    if (depth > 2 || g_nowners >= MAX_OWNERS || !class_of_object(p, cls, n))
        return;
    for (int i = 0; i < g_nowners; i++)
        if (g_owners[i] == p)
            return;
    g_owners[g_nowners++] = p;
    old_add_owned(p, cls, n, depth);
}
void old_scan(const BYTE *block, DWORD size, CClassInfo **cls, int n, int depth)
{
    if (!block || !readable(block, size))
        return;
    for (DWORD off = 0; off < size && g_nowners < MAX_OWNERS; off += 4) {
        void *p = *reinterpret_cast<void *const *>(block + off);
        if (class_of_object(p, cls, n)) {
            old_add_owner(p, cls, n, 0);
        } else if (depth == 0 && ptrv(p) >= 0x10000 && !(ptrv(p) & 3) && (ptrv(p) < 0x400000 || ptrv(p) >= IMAGE_END)) {
            old_scan(static_cast<const BYTE *>(p), 0x100, cls, n, 1);
        }
    }
}
void *g_old[MAX_OWNERS];
int g_nold;
const char *old_mark(void *p)
{
    for (int k = 0; k < g_nold; k++)
        if (g_old[k] == p)
            return "";
    return " NEW";
}
#endif

// The parts of the level where the game keeps them, for the objects they own (AIs, child lists, particle
// infos ...), which only the overrun had reached before: the player's CLayer (real_player_layer), its grid of
// parts (columns +0x60, rows +0x64, cells +0x68, one pointer array per cell: vtable, data, count, capacity;
// as checks_inventory.cpp), the cell array and each cell's data read only within their live blocks; the
// parts of registered classes that start a live block, at most MAX_GRID of them. The parts themselves are
// CLayerParts, whose own fields are not read back (world_class); MAX_GRID_ROOTS of them, spread over the grid
constexpr int MAX_GRID = 2048, MAX_GRID_ROOTS = 48;
void *g_grid[MAX_GRID];
int grid_parts(BYTE *layer, CClassInfo **cls, int n)
{
    int count_found = 0;
    DWORD start, size;
    if (!live_block_of(ptrv(layer), start, size) || start != ptrv(layer) || size < 0x6c)
        return 0;
    const DWORD columns = *reinterpret_cast<DWORD *>(layer + 0x60), rows = *reinterpret_cast<DWORD *>(layer + 0x64);
    const DWORD *cells = *reinterpret_cast<DWORD **>(layer + 0x68);
    if (columns > 4096 || rows > 4096 || !live_block_of(ptrv(cells), start, size) || start + size - ptrv(cells) < columns * rows * 16)
        return 0;
    for (DWORD i = 0; i < columns * rows; i++) {
        const DWORD *cell = cells + i * 4;
        const DWORD count = cell[2];
        void *const *data = reinterpret_cast<void *const *>(static_cast<uintptr_t>(cell[1]));
        if (!count || count > 0x10000 || !live_block_of(ptrv(data), start, size) || start + size - ptrv(data) < count * 4)
            continue;
        for (DWORD k = 0; k < count && count_found < MAX_GRID; k++) {
            void *p = data[k];
            CClassInfo *c = class_of_object(p, cls, n);
            if (!c || !live_whole(p, c->size, true))
                continue;
            bool dup = false;
            for (int j = 0; j < count_found && !dup; j++)
                dup = g_grid[j] == p;
            if (!dup)
                g_grid[count_found++] = p;
        }
    }
    return count_found;
}

// Collects the owners into g_owners and logs how each was reached
void find_owners(CClassInfo **cls, int n)
{
#ifdef ZAX_COMPARE_SCANS
    g_nowners = 0;
    old_scan(*at<BYTE **>(0x6c2080), 0x1000, cls, n, 0);
    g_nold = g_nowners;
    for (int i = 0; i < g_nold; i++) {
        g_old[i] = g_owners[i];
        log("  old owner %d: %s %x", i, class_of(g_old[i])->name, ptrv(g_old[i]));
    }
#endif
    static void *roots[MAX_OWNERS];
    static char from[MAX_OWNERS][REAL_FROM];
    const int nroots = find_real_objects(roots, from, MAX_OWNERS, cls, n, 24);
    g_nowners = 0;
    g_rejected_owned = 0;
    if (!live_blocks_take())
        return;
    for (int r = 0; r < nroots && g_nowners < MAX_OWNERS; r++) {
        const int first = g_nowners;
        add_owner(roots[r], cls, n, 0, false);
        for (int i = first; i < g_nowners; i++)
#ifdef ZAX_COMPARE_SCANS
            log("  owner %d: %s %x%s (%s%s)", i, class_of(g_owners[i])->name, ptrv(g_owners[i]), old_mark(g_owners[i]), from[r],
                i == first ? "" : ", owned");
#else
            log("  owner %d: %s (%s%s)", i, class_of(g_owners[i])->name, from[r], i == first ? "" : ", owned");
#endif
    }
    const int from_world = g_nowners;
    int nparts = 0, picked = 0;
    BYTE *layer = real_player_layer();
    const int ngrid = layer ? grid_parts(layer, cls, n) : 0;
    for (int i = 0; i < ngrid && g_nowners < MAX_OWNERS; i++) {
        if (!spread_pick(i, ngrid, MAX_GRID_ROOTS))
            continue;
        picked++;
        const int first = g_nowners;
        add_owner(g_grid[i], cls, n, 0, false);
        for (int k = first; k < g_nowners; k++)
            log("  owner %d: %s%s (level grid, part %d of %d%s)", k, class_of(g_owners[k])->name,
#ifdef ZAX_COMPARE_SCANS
                old_mark(g_owners[k]),
#else
                "",
#endif
                i, ngrid, k == first ? "" : ", owned");
    }
    nparts = g_nowners - from_world;
    live_blocks_free();
    log("  owners: %d, %d from %d real objects (the world, the dialogue lines), %d from %d of the %d parts of registered classes in the "
        "level grid (CLayer %x); %d owned objects or slots not taken (no live block holds them whole)",
        g_nowners, from_world, nroots, nparts, picked, ngrid, ptrv(layer), g_rejected_owned);
#ifdef ZAX_COMPARE_SCANS
    int lost = 0;
    for (int k = 0; k < g_nold; k++) {
        bool in = false;
        for (int i = 0; i < g_nowners && !in; i++)
            in = g_owners[i] == g_old[k];
        if (!in) {
            lost++;
            log("  old owner %d not in the new set: %s %x", k, class_of(g_old[k])->name, ptrv(g_old[k]));
        }
    }
    log("  owner compare: old %d, new %d, old not in new %d", g_nold, g_nowners, lost);
#endif
}

// ---- The cases ----------------------------------------------------------------------------------------

int g_fields[4], g_variants, g_skipped_world;

// Whether the value holds objects of the world (CLayerPart: entities, polygons, triggers) or drawn
// objects: copies of those register in the world or the rendering when they are read, and after they
// are deleted the game ended writing the running level in the next module's checks (run
// 20261023-201316-7306). Those values are written, not read back.
bool is_class(CClassInfo *c, const char *name)
{
    for (; c; c = c->parent)
        if (lstrcmpA(c->name, name) == 0)
            return true;
    return false;
}
bool world_class(CClassInfo *c)
{
    return is_class(c, "CLayerPart") || is_class(c, "CSequence") || is_class(c, "CFrameInterface") || is_class(c, "CLayer");
}
bool holds_world_objects(CFieldDesc *d, BYTE *owner)
{
    const DWORD vt = d->vftable;
    if (vt == VT_OBJECT_FIELD) {
        void *o = *reinterpret_cast<void **>(owner + d->offset);
        return o && world_class(class_of(o));
    }
    if (vt == VT_ARRAY_FIELD) {
        auto *a = static_cast<CObjectArrayFieldDesc *>(d);
        if (a->element->vftable != VT_OBJECT_FIELD)
            return false;
        const DWORD count = *reinterpret_cast<DWORD *>(owner + d->offset);
        BYTE *data = *reinterpret_cast<BYTE **>(owner + a->f30);
        for (DWORD i = 0; i < count && data; i++) {
            void *o = *reinterpret_cast<void **>(data + a->element_f3c * i + a->element->offset);
            if (o && world_class(class_of(o)))
                return true;
        }
        return false;
    }
    if (vt == VT_LIST_FIELD)
        return world_class(static_cast<CClassInfo *>(static_cast<CListFieldDesc *>(d)->element_class));
    return false;
}
void trace(CFieldDesc *d, CClassInfo *c, const char *what)
{
    if (g_trace)
        log("    %s.%s: %s", c->name, d->name.c_str(), what);
}

Tally *g_trunc;   // the empty stream cases (test_truncated)
int g_trunc_descs;
void test_truncated(Tally &t, CFieldDesc *d, bool list);
void test_field(Tally &t, Tally &bin, CFieldDesc *d, BYTE *owner, CClassInfo *owner_class)
{
    const DWORD vt = d->vftable;
    const bool array = vt == VT_ARRAY_FIELD || vt == VT_LIST_FIELD;
    if (world_class(owner_class) || holds_world_objects(d, owner)) {
        g_skipped_world++;
        return;
    }
    new_blocks(owner_class->size + 0x40);
    if (!g_block[0] || !g_block[1]) {
        free_blocks();
        return;
    }
    // text: slot 23 of the real value, read back with slot 19
    trace(d, owner_class, "text");
    if (vt != VT_CLASS_FIELD) {
        write_value(d, owner, g_value, 23);
        char first[0x800];
        Bytes v = {nullptr, 0, 0};
        for (int k = 0; k < (array ? 8 : 1); k++) {
            // a count that does not match the elements leaves states the original cannot handle either:
            // the pointer array creates default objects (element slot 10) outside the world and the game
            // ends (runs 20261023-181036-a76c, -181358-e760, CChildList.Child List), the new[] array
            // reads elements past its data (run 20261023-183623-0b77, CRotationSequence.Cutting Lines)
            if (k == 3 || k == 4)
                continue;
            variant(g_value, v, k);
            split(v, first, g_rest);
            text_case(t, d, first, g_rest, true, VARIANT_NAMES[k], array ? 0 : 0xff);   // the array slots return 0
            g_variants += k > 0;
        }
        if (v.data)
            VirtualFree(v.data, 0, 0x8000);
        if (array) {
            // the result is the eax the resize leaves (void, not reproduced)
            text_case(t, d, "3", g_rest, false, "no reader", 0);
            text_case(t, d, "0", g_rest, false, "no reader 0", 0);
        }
    }
    // binary: slot 26 of the real value, read back with slot 29, written again
    trace(d, owner_class, "binary");
    // the original ends the game writing a CZaxSaveGameStats as binary (checks_object_io.cpp)
    const bool binary_ok = lstrcmpA(owner_class->name, "CZaxSaveGameStats") != 0;
    if (binary_ok) {
        write_value(d, owner, g_bin, 26);
        DWORD r[2];
        both([&](int s) {
            prepare(d, g_block[s]);
            CObjectWriter w;
            open_stream(&w, 4, 1, g_bin.data, g_bin.size, 0);
            r[s] = vslot<DWORD>(d, 29, g_block[s], &w);
            if (vt == VT_OBJECT_FIELD || vt == VT_CLASS_FIELD)
                r[s] &= 0xff;   // the upper bytes of a pointer
            r[s] ^= static_cast<const MemoryStream *>(w.stream)->position << 8;
            close_stream(&w);
            write_value(d, g_block[s], g_out[s], 26);
            free_value(d, g_block[s]);
        });
        report(bin, d, "binary", r[0], r[1]);
        if ((vt == VT_ARRAY_FIELD || vt == VT_LIST_FIELD) && g_trunc_descs < 40) {
            g_trunc_descs++;
            test_truncated(*g_trunc, d, vt == VT_LIST_FIELD);
        }
    }
    // slot 16 (owned object), slot 32, and append / remove of the pointer array on the read value
    if (vt == VT_OBJECT_FIELD) {
        DWORD r[2];
        both([&](int s) {
            ZString z;
            zs_ctor_text(&z, "previous");
            r[s] = 0;
            vslot<DWORD>(d, 16, owner, &z);
            g_out[s].size = 0;
            append_text(g_out[s], z.c_str());
            zs_dtor(&z);
        });
        report(t, d, "slot 16", r[0], r[1]);
    }
    trace(d, owner_class, "slot 16, append");
    if (vt == VT_ARRAY_FIELD && binary_ok) {
        auto *a = static_cast<CObjectArrayFieldDesc *>(d);
        DWORD r[2];
        both([&](int s) {
            prepare(d, g_block[s]);
            CObjectWriter w;
            open_stream(&w, 4, 1, g_bin.data, g_bin.size, 0);
            vslot<DWORD>(d, 29, g_block[s], &w);
            close_stream(&w);
            BYTE *e = pick<BYTE *(THISCALL *)(CFieldDesc *, BYTE *)>(0x4199b0)(d, g_block[s]);
            BYTE *data = *reinterpret_cast<BYTE **>(g_block[s] + a->f30);
            r[s] = (ptrv(e) - ptrv(data)) / (a->element_f3c ? a->element_f3c : 1);
            pick<DWORD(THISCALL *)(CFieldDesc *, BYTE *, DWORD)>(0x4199f0)(d, g_block[s], 0);
            r[s] |= *reinterpret_cast<DWORD *>(g_block[s] + d->offset) << 16;
            write_value(d, g_block[s], g_out[s], 23);
            free_value(d, g_block[s]);
        });
        report(t, d, "append/remove", r[0], r[1]);
    }
    free_blocks();
}

// Slot 29 of the pointer array (0x419810) and the new[] array (0x418050) on short streams (batch
// decomp-faithful2): a count the stream does not hold and a name it refuses are whatever the stack held in the exe's
// frame (the count at its [esp + 0], the name buffer from [esp + 8] / [esp + 4]). The call goes through
// zax_call_stack with a fill of the case on both sides, the count's dword set to 0, 1, 2 or 0x1234 (others would
// read that many elements); a copy of the real descriptor gets a fake name list of one name (slot 22), whose Find
// (slot 2) records the name it is given (up to 64 bytes) and answers -1, so the read ends at the first name.
// Compared: eax, the names asked for, and that the count and the name are the fill's at the exe's addresses.
constexpr DWORD TRUNC_FILL = 0x110;   // dwords: the frame of 0x408 bytes and the arguments
struct FakeNames {
    DWORD vftable;
};
DWORD g_names_vt[32];
FakeNames g_fake_names;
unsigned g_find_hash;
DWORD g_find_calls;
unsigned name_hash(const char *s)
{
    DWORD n = 0;
    while (n < 64 && s[n])
        n++;
    return fnv(fnv(2166136261u, &n, 4), s, n);
}
int THISCALL fake_find(FakeNames *, const char *name, int)
{
    g_find_calls++;
    g_find_hash = fnv(g_find_hash, &g_find_calls, 4) ^ name_hash(name);
    return -1;
}
DWORD THISCALL fake_count(FakeNames *) { return 1; }
const char *THISCALL fake_name(FakeNames *, DWORD) { return "N"; }
DWORD g_trunc_cases, g_trunc_asked;
void test_truncated(Tally &t, CFieldDesc *d, bool list)
{
    g_names_vt[1] = ptrv((void *)&fake_name);
    g_names_vt[2] = ptrv((void *)&fake_find);
    g_names_vt[22] = ptrv((void *)&fake_count);
    g_fake_names.vftable = ptrv(g_names_vt);
    alignas(4) BYTE copy[0x48];
    memcpy(copy, d, sizeof copy);
    *reinterpret_cast<DWORD *>(copy + 0x40) = ptrv(&g_fake_names);
    CFieldDesc *const dc = reinterpret_cast<CFieldDesc *>(copy);
    const DWORD frame = list ? 0x404 : 0x408, name_at = list ? 4 : 8;
    // k 0-3: an empty stream, the count from the fill (the name read then gives "": FileBuf_ReadCountedText 0x4a2300
    // takes the length 0 and ends the buffer); k 4-5: a count of 1 and a name length larger than the stream's rest,
    // which FileBuf_ReadCountedText refuses without touching the buffer: the name is the fill's bytes there
    static const BYTE long_name[8] = {1, 0, 0, 0, 0, 1, 0, 0};
    for (int k = 0; k < 6; k++) {
        DWORD fill[TRUNC_FILL];
        stack_pattern(fill, TRUNC_FILL, random());
        const DWORD counts[6] = {0, 1, 2, 0x1234, 1, 1};
        const DWORD count = counts[k];
        if (k < 4)
            fill[fill_index(2, frame, 0)] = count;
        if (random() % 3 == 0)   // a short name: a zero byte early in the buffer
            fill[fill_index(2, frame, name_at + 4 * (random() % 4))] &= 0xff00ffffu;
        // the name the exe's buffer holds: the fill's bytes from the frame's base + name_at upwards
        BYTE image[4 * TRUNC_FILL];
        for (DWORD i = 0; i < TRUNC_FILL; i++)
            memcpy(image + 4 * TRUNC_FILL - 4 * (i + 1), &fill[i], 4);
        const DWORD base = 4 * TRUNC_FILL - 4 * 2 - 4 - frame;
        const char *const name = k < 4 ? "" : reinterpret_cast<const char *>(image + base + name_at);
        DWORD one = 1;
        const unsigned want = count ? fnv(2166136261u, &one, 4) ^ name_hash(name) : 2166136261u;
        DWORD r[2], calls[2];
        unsigned h[2];
        both([&](int s) {
            prepare(dc, g_block[s]);
            CObjectWriter w;
            open_stream(&w, 4, 1, long_name, k < 4 ? 0 : 8, 0);
            g_find_hash = 2166136261u;
            g_find_calls = 0;
            const DWORD args[2] = {ptrv(g_block[s]), ptrv(&w)};
            r[s] = zax_call_stack(pick_address(slot(dc, 29)), ptrv(dc), args, 2, fill, TRUNC_FILL);
            h[s] = g_find_hash;
            calls[s] = g_find_calls;
            close_stream(&w);
            free_value(dc, g_block[s]);
        });
        g_trunc_cases++;
        g_trunc_asked += calls[0];
        char m[200];
        wsprintfA(m, "[%.40s %s, count %x: eax %x/%x, names asked %u/%u, hash %x/%x, expected %x]", d->name.c_str(),
                  k < 4 ? "empty stream" : "name too long", count, r[0], r[1], calls[0], calls[1], h[0], h[1], want);
        t.check(r[0] == r[1] && h[0] == h[1] && calls[0] == calls[1] && h[0] == want && calls[0] == (count ? 1u : 0u), "", m);
    }
}

// Slot 32 (precache) with fakes in place of the objects: their slot 7 records the call. On the game's
// objects it loads and releases sounds of the global sound manager (CPlayerWalkingControlAI::vf7,
// CZaxDialogAction::vf7), and the game ended in later checks writing the running level (runs
// 20261023-195810-d1e0, -200113-3b63)
struct Precached {
    DWORD vftable;
    DWORD id;
};
DWORD g_precached_vt[8];
DWORD THISCALL precached_slot7(Precached *p)
{
    ev(7, p->id);
    return 0x7000 + p->id;
}
Precached g_precached[2][8];
void test_slot32(Tally &t, CFieldDesc *d)
{
    const DWORD vt = d->vftable;
    g_precached_vt[7] = ptrv((void *)&precached_slot7);
    if (d->offset + 4 > 0x1ff0)
        return;
    if (vt == VT_ARRAY_FIELD) {
        auto *a = static_cast<CObjectArrayFieldDesc *>(d);
        if (a->element->vftable != VT_OBJECT_FIELD || a->f30 + 4 > 0x1ff0)
            return;
    } else if (vt == VT_LIST_FIELD) {
        auto *l = static_cast<CListFieldDesc *>(d);
        if (l->data_offset + 4 > 0x1ff0 || l->element_size < sizeof(Precached))
            return;
    }
    new_blocks(0x2000);
    for (DWORD count = 0; count < 4; count++) {
        DWORD r[2];
        both([&](int s) {
            memset(g_block[s], 0, g_block_size);
            for (DWORD k = 0; k < 8; k++)
                g_precached[s][k] = {ptrv(g_precached_vt), k + 1};
            BYTE *data = nullptr;
            if (vt == VT_OBJECT_FIELD) {
                *reinterpret_cast<void **>(g_block[s] + d->offset) = count ? &g_precached[s][count] : nullptr;
            } else if (vt == VT_ARRAY_FIELD) {
                auto *a = static_cast<CObjectArrayFieldDesc *>(d);
                data = static_cast<BYTE *>(heap_alloc(a->element_f3c * 4 + 4));
                memset(data, 0, a->element_f3c * 4 + 4);
                for (DWORD k = 0; k < count; k++)
                    *reinterpret_cast<void **>(data + a->element_f3c * k + a->element->offset) = k == 1 ? nullptr : &g_precached[s][k];
                *reinterpret_cast<DWORD *>(g_block[s] + d->offset) = count;
                *reinterpret_cast<BYTE **>(g_block[s] + a->f30) = data;
            } else {
                auto *l = static_cast<CListFieldDesc *>(d);
                data = static_cast<BYTE *>(heap_alloc(l->element_size * 4 + 4));
                for (DWORD k = 0; k < count; k++)
                    memcpy(data + l->element_size * k, &g_precached[s][k], sizeof(Precached));
                *reinterpret_cast<DWORD *>(g_block[s] + d->offset) = count;
                *reinterpret_cast<BYTE **>(g_block[s] + l->data_offset) = data;
            }
            r[s] = vslot<DWORD>(d, 32, g_block[s]);
            g_out[s].size = 0;
            append(g_out[s], g_log[s], 4 * (g_nlog[s] < LOG ? g_nlog[s] : LOG));
            if (data)
                heap_free(data);
        });
        // without objects the loops leave a count or the offset: compared too
        report(t, d, "slot 32", r[0], r[1]);
    }
    free_blocks();
}

// The owned object slot 19 with class names that end the game, with a name in other case, and ""
void test_errors(Tally &t, CClassInfo **cls, int n, CFieldDesc *d)
{
    auto *o = static_cast<CObjectFieldDesc *>(d);
    CClassInfo *expected = reinterpret_cast<CClassInfo *>(o->f2c);
    if (!expected)
        return;
    CClassInfo *good = nullptr, *wrong = nullptr, *abstract = nullptr;
    for (int i = 0; i < n; i++) {
        const bool derived = is_derived(cls[i], expected);
        if (derived && !cls[i]->is_abstract && !good)
            good = cls[i];
        if (derived && cls[i]->is_abstract && !abstract)
            abstract = cls[i];
        if (!derived && !cls[i]->is_abstract && !wrong && cls[i]->num_fields)
            wrong = cls[i];
    }
    new_blocks(0x1000);
    Bytes body = {nullptr, 0, 0};
    append_text(body, "{\r\n}\r\nNext=1\r\n");
    char lower[64] = "";
    if (good)
        for (int i = 0; i < 63 && good->name[i]; i++)
            lower[i + 1] = 0, lower[i] = good->name[i] >= 'A' && good->name[i] <= 'Z' ? good->name[i] + 32 : good->name[i];
    const char *const names[] = {"CNoSuchClass", wrong ? wrong->name : nullptr, abstract ? abstract->name : nullptr,
                                 good ? good->name : nullptr, good ? lower : nullptr, ""};
    for (const char *name : names) {
        if (!name)
            continue;
        // once into an empty block, once into a block that holds an object of the good class
        for (int filled = 0; filled < 2 && (!filled || good); filled++) {
            DWORD r[2], pos[2];
            unsigned box[2];
            both([&](int s) {
                prepare(d, g_block[s]);
                if (filled) {
                    free_value(d, g_block[s]);
                    *reinterpret_cast<void **>(g_block[s] + d->offset) = create(good->name);
                }
                r[s] = read_text_value(d, s, name, body, true, pos[s], box[s]);
                write_value(d, g_block[s], g_out[s], 23);
                free_value(d, g_block[s]);
            });
            report(t, d, name[0] ? name : "(empty)", r[0] ^ pos[0] << 8 ^ box[0], r[1] ^ pos[1] << 8 ^ box[1]);
        }
    }
    if (body.data)
        VirtualFree(body.data, 0, 0x8000);
    free_blocks();
}

// The class pointer slot 29 with names: known, other case, unknown (FindChecked drops it), ""
void test_class_names(Tally &t, CFieldDesc *d, CClassInfo **cls, int n)
{
    new_blocks(0x1000);
    const char *const names[] = {cls[0]->name, cls[n / 2]->name, "cnosuchclass", "", "cconditionalaction"};
    for (const char *name : names) {
        g_bin.size = 0;
        append_dword(g_bin, (DWORD)lstrlenA(name));
        append_text(g_bin, name);
        DWORD r[2];
        both([&](int s) {
            prepare(d, g_block[s]);
            CObjectWriter w;
            open_stream(&w, 4, 1, g_bin.data, g_bin.size, 0);
            r[s] = vslot<DWORD>(d, 29, g_block[s], &w) & 0xff;
            close_stream(&w);
            write_value(d, g_block[s], g_out[s], 26);
        });
        report(t, d, name, r[0], r[1]);
    }
    free_blocks();
}

}  // namespace

ZAX_CHECKS(field_objects)
{
    Tally text{"field objects: slots 16/19/23 of the object and array kinds (real data)"},
        bin{"field objects: slots 26/29 of the object, array and class kinds (real data)"},
        slot32{"field objects: slot 32 of the object and array kinds (real data)"},
        errors{"field objects: owned object slot 19, class names and fatal errors"},
        names{"field objects: class pointer slot 29 with names"},
        trunc{"field objects: array slots 29 on an empty stream (stack pattern)"};
    g_trunc = &trunc;
    init_stream_vtable();
    const DWORD start = GetTickCount();
    find_owners(cls, n);
    CFieldDesc *error_desc = nullptr, *class_desc = nullptr;
    for (int i = 0; i < g_nowners; i++) {
        CClassInfo *c = class_of(g_owners[i]);
        for (DWORD f = 0; f < c->num_fields; f++) {
            CFieldDesc *d = c->fields[f];
            const DWORD vt = d->vftable;
            const int kind = vt == VT_OBJECT_FIELD ? 0 : vt == VT_ARRAY_FIELD ? 1 : vt == VT_LIST_FIELD ? 2 : vt == VT_CLASS_FIELD ? 3 : -1;
            if (kind < 0)
                continue;
            g_fields[kind]++;
            test_field(kind == 3 ? bin : text, bin, d, static_cast<BYTE *>(g_owners[i]), c);
            if (kind < 3) {
                trace(d, c, "slot 32");
                        test_slot32(slot32, d);
            }
            if (kind == 0 && !error_desc && reinterpret_cast<CObjectFieldDesc *>(d)->f2c)
                error_desc = d;
            if (kind == 3 && !class_desc)
                class_desc = d;
        }
    }
    // a descriptor of each kind from the classes if the game's objects have none
    for (int i = 0; i < n && (!error_desc || !class_desc); i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            if (!error_desc && d->vftable == VT_OBJECT_FIELD && reinterpret_cast<CObjectFieldDesc *>(d)->f2c && d->offset < 0xf00)
                error_desc = d;
            if (!class_desc && d->vftable == VT_CLASS_FIELD && d->offset < 0xf00)
                class_desc = d;
        }
    if (error_desc)
        test_errors(errors, cls, n, error_desc);
    if (class_desc)
        test_class_names(names, class_desc, cls, n);
    text.report();
    bin.report();
    slot32.report();
    errors.report();
    names.report();
    trunc.report();
    log("  empty stream: %d descriptors, %u cases, %u names asked", g_trunc_descs, g_trunc_cases, g_trunc_asked);
    log("  owners %d; fields: object %d, pointer array %d, new[] array %d, class %d (%d of world objects not read back); "
        "%d text variants; %u cases, %u bytes; %d deleted owned objects (%d at other addresses on the two sides); "
        "error descriptor %s; %u ms",
        g_nowners, g_fields[0], g_fields[1], g_fields[2], g_fields[3], g_skipped_world, g_variants, g_cases, g_bytes, g_deleted_owned,
        g_deleted_differ,
        error_desc ? error_desc->name.c_str() : "-", GetTickCount() - start);
}

}  // namespace zax
