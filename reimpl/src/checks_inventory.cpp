// Checks of the inventory's item bookkeeping (inventory.cpp) against the original, in the game thread.
//
// Every case runs twice: side 0 with all hooks of the module taken out (the original's first bytes put
// back, so nested calls run the original too), side 1 through the reimplementation. Both sides work on
// the same fake world, set up again from the same seed before each side:
//  - an item catalogue of our own (the globals 0x6c0c38/0x6c0c3c point to it during the case): 12
//    records whose definitions have random categories and sort orders (ties included); some records
//    are "being loaded" (the definition resolves to null), some cases have no catalogue at all;
//  - an inventory with the real vtable 0x5ee0c0 copied, slot 26 recording and forwarding to the side's
//    GetItem 0x425990 (some cases hide items there, so Repair deletes and removes them); random
//    selected and "changing to" indices (valid, -1, out of range), focus, inited byte, discovered list;
//  - items whose slots (class info, count, slot 34, select, deselect, delete) record their calls;
//  - the disposed list and the collecting array record what they get, the error reporter *0x6c02cc
//    records slot 17's text, the report byte 0x60b5d4 is random.
// Compared: the return value (eax), the event log and the bytes of the whole fake world.
// Real data: the inventories of the characters of the running game (reached from the world 0x6c2080,
// one level down, +0x108 of a character), read-only functions only, with the real catalogue.
#include "check_util.h"
#include "inventory.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);

namespace zax {

int hooks_inventory(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Event log ---------------------------------------------------------------------------------------

int g_side;
constexpr int LOG = 2048;
DWORD g_log[2][LOG];
int g_nlog[2];
unsigned g_loghash[2];
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    int &n = g_nlog[g_side];
    if (n + 4 <= LOG)
        memcpy(&g_log[g_side][n], e, sizeof e);
    n += 4;
}
bool same_log() { return g_nlog[0] == g_nlog[1] && g_loghash[0] == g_loghash[1]; }
DWORD result(DWORD low) { return ((0x5e700000u + (DWORD)g_nlog[g_side] * 0x100u) & 0xffffff00) | (low & 0xff); }

// ---- Hooks of the module out and in ------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
SavedSite g_sites[40];
int g_nsites;
void hooks_out()
{
    Hook list[40];
    g_nsites = 0;
    const int n = hooks_inventory(nullptr);
    if (!hook_sites_fit("inventory", n, 40))
        return;
    hooks_inventory(list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (original_address(list[i].address) == site)
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, list[i].expected);
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

// ---- The fake world ----------------------------------------------------------------------------------

constexpr int NDEF = 12, MAXI = 12, NTYPES = 64;

struct Record {
    BYTE name[0x78];
    ItemDefinition *definition;   // 0x78
    BYTE keep, in_use, unknown7e, loading;   // 0x7c-0x7f
};
static_assert(sizeof(Record) == 0x80);

struct FakeItem {
    CInventoryItem base;
    DWORD id;
    int count;           // slot 41
    void *class_info;    // slot 2
    DWORD pattern;       // slot 34
};

struct FakeArray {       // the collecting array: slot 2 records the index
    DWORD vftable;
};

struct World {
    Record records[NDEF];
    BYTE definitions[NDEF][0x100];
    FakeItem items[MAXI + 1];       // the last one is the sentinel at data[-1]
    void *data[MAXI + 2];           // data[0] is the sentinel, the inventory's data starts at data[1]
    void *discovered[8];
    CItemTypeData types[NTYPES];
    CInventory inv;
    FakeArray out;
    DWORD part_vtable_slot36_result;   // which object slot 36 of the fake part returns
    DWORD part_vftable;
    DWORD hide;                        // GetItem returns null for these indices (bits)
};
World g_world;

void *g_inv_vtable[64];
void *g_item_vtable[64];
void *g_out_vtable[4];
void *g_disposed_vtable[4];
void *g_part_vtable[40];
void *g_reporter_vtable[20];
DWORD g_reporter = 0;   // the fake reporter object: its vtable pointer

int item_id(const void *p)
{
    for (int i = 0; i <= MAXI; i++)
        if (p == &g_world.items[i])
            return i + 1;
    return p ? 0x99 : 0;
}

CInventoryItem *THISCALL f_get_item(CInventory *inv, DWORD i)
{
    const bool hidden = i < 32 && (g_world.hide >> i & 1);
    CInventoryItem *r = hidden ? nullptr : at<CInventoryItem *(THISCALL *)(CInventory *, DWORD)>(0x425990)(inv, i);
    ev(0x26, i, (DWORD)item_id(r), hidden);
    return r;
}
DWORD THISCALL f_item_delete(FakeItem *it, DWORD flags) { ev(0x01, it->id, flags); return result(1); }
void *THISCALL f_item_class(FakeItem *it) { ev(0x02, it->id); return it->class_info; }
DWORD THISCALL f_item_34(FakeItem *it, DWORD a, DWORD b)
{
    ev(0x34, it->id, a, b);
    return result(it->pattern);
}
int THISCALL f_item_count(FakeItem *it) { ev(0x41, it->id); return it->count; }
DWORD THISCALL f_item_select(FakeItem *it, CInventory *inv)
{
    ev(0x44, it->id, inv == &g_world.inv);
    it->base.selected = 1;
    return result(0x44);
}
DWORD THISCALL f_item_deselect(FakeItem *it, CInventory *inv)
{
    ev(0x45, it->id, inv == &g_world.inv);
    it->base.selected = 0;
    return result(0x45);
}
DWORD THISCALL f_bad(void *) { ev(0xbad); return result(0xba); }
DWORD THISCALL f_out_add(FakeArray *, const DWORD *i) { ev(0xc2, *i); return result(0xc2); }
DWORD THISCALL f_disposed_add(ZPtrArray *, void *const *p) { ev(0xd2, (DWORD)item_id(*p)); return result(0xd2); }
void *THISCALL f_part_inventory(void *) { ev(0x36); return (void *)(uintptr_t)g_world.part_vtable_slot36_result; }
DWORD THISCALL f_report(void *, const char *text) { ev(0x17, fnv_text(2166136261u, text)); return result(0x17); }

void init_vtables()
{
    memcpy(g_inv_vtable, at<void *>(INVENTORY_VTABLE), sizeof(void *) * 29);
    for (int i = 29; i < 64; i++)
        g_inv_vtable[i] = (void *)&f_bad;
    g_inv_vtable[26] = (void *)&f_get_item;
    for (int i = 0; i < 64; i++)
        g_item_vtable[i] = (void *)&f_bad;
    g_item_vtable[1] = (void *)&f_item_delete;
    g_item_vtable[2] = (void *)&f_item_class;
    g_item_vtable[34] = (void *)&f_item_34;
    g_item_vtable[41] = (void *)&f_item_count;
    g_item_vtable[44] = (void *)&f_item_select;
    g_item_vtable[45] = (void *)&f_item_deselect;
    for (int i = 0; i < 4; i++)
        g_out_vtable[i] = g_disposed_vtable[i] = (void *)&f_bad;
    g_out_vtable[2] = (void *)&f_out_add;
    g_disposed_vtable[2] = (void *)&f_disposed_add;
    for (int i = 0; i < 40; i++)
        g_part_vtable[i] = (void *)&f_bad;
    g_part_vtable[36] = (void *)&f_part_inventory;
    for (int i = 0; i < 20; i++)
        g_reporter_vtable[i] = (void *)&f_bad;
    g_reporter_vtable[17] = (void *)&f_report;
    g_reporter = ptrv(g_reporter_vtable);
}

// What a case allows: null items in the list, null definitions (or no catalogue)
struct Allow {
    bool null_items, null_definitions, hide;
};

CClassInfo *g_picked[8];
bool g_trace = false;   // a log line per case (to find a case that ends the game)
// With the real catalogue: the items get the indices of loaded records of the game's item catalogue
// (their real categories and sort orders); the catalogue is not swapped then
bool g_real;
DWORD g_loaded[NDEF];
int g_nloaded;
DWORD g_real_categories;
DWORD def_index(DWORD r) { return g_real ? g_loaded[r % (DWORD)g_nloaded] : r % (NDEF + 2); }
ItemDefinition *def_at(DWORD index)
{
    if (g_real)
        return *at<ItemDefinition **>(*at<DWORD *>(ITEM_CATALOGUE_DATA) + index * 0x80 + 0x78);
    return reinterpret_cast<ItemDefinition *>(g_world.definitions[index < NDEF ? index : 0]);
}
void *def_ptr(DWORD r) { return def_at(def_index(r)); }
DWORD g_primary;   // the category index of "Primary" in the real category list
CClassInfo **g_cls;
int g_ncls;

DWORD pick_index(DWORD count)
{
    const DWORD r = random() >> 8;
    switch (r % 6) {
    case 0: return 0xffffffff;
    case 1: return count + (r >> 4) % 3;
    default: return count ? (r >> 4) % count : 0xffffffff;
    }
}

// Odd seeds of the functions that allow null definitions have none (and a random inited byte, so
// CountOfCategory and CollectOfCategory run Repair as well)
bool null_definitions(const Allow &allow, unsigned seed) { return allow.null_definitions && (seed & 1); }

void setup(unsigned seed, const Allow &allow, bool need_item)
{
    g_seed = seed;
    const bool null_defs = !g_real && null_definitions(allow, seed);
    World &w = g_world;
    memset(&w, 0, sizeof w);
    for (int d = 0; d < NDEF; d++) {
        Record &r = w.records[d];
        for (int k = 0; k < 8; k++)
            r.name[k] = (BYTE)('A' + d);
        r.definition = reinterpret_cast<ItemDefinition *>(w.definitions[d]);
        r.loading = null_defs && random() % 7 == 0;
        ItemDefinition *def = r.definition;
        const DWORD c = random() >> 10;
        def->category = c % 5 == 0 && g_primary < NTYPES ? g_primary : c % 6;
        def->order = (random() >> 12) % 4;
    }
    const DWORD n = need_item ? 1 + (random() >> 9) % MAXI : (random() >> 9) % (MAXI + 1);
    const DWORD num_types = 1 + (random() >> 11) % 7;
    const DWORD types_used = g_real ? g_real_categories : num_types;
    for (int i = 0; i <= MAXI; i++) {
        FakeItem &it = w.items[i];
        it.base.vftable = ptrv(g_item_vtable);
        it.base.definition = def_index(random() >> 7);   // fake: beyond the count means record 0
        it.base.selected = (random() >> 13) % 5 == 0 ? 1 : 0;
        it.base.marked_for_deletion = (random() >> 13) % 4 == 0;
        it.id = (DWORD)i + 1;
        const DWORD k = random() >> 9;
        it.count = k % 5 == 0 ? -1 : (int)(k % 7);
        it.class_info = g_ncls ? g_cls[(random() >> 9) % (g_ncls < 6 ? g_ncls : 6)] : nullptr;
        it.pattern = (random() >> 15) % 3 == 0;
    }
    w.data[0] = &w.items[MAXI];
    for (DWORD i = 0; i < n; i++) {
        // mostly distinct items, now and then one twice or a null entry
        DWORD j = (random() >> 8) % 9 == 0 ? (random() >> 8) % MAXI : i;
        w.data[1 + i] = allow.null_items && (random() >> 10) % 6 == 0 ? nullptr : &w.items[j];
    }
    for (DWORD k = 0; k < NTYPES; k++) {
        CItemTypeData &t = w.types[k];
        t.vftable = 0x5ee064;
        t.cur_index = pick_index(n);
        t.changing_to = pick_index(n);
        t.seconds_into_change = (float)(int)(random() >> 20) / 64.0f;
        t.seconds_to_change = (float)(int)(random() >> 20) / 16.0f;
    }
    // the item marked selected usually is the category's selection
    for (DWORD i = 0; i < n; i++)
        if (w.data[1 + i] && (random() >> 10) % 3 == 0)
            w.types[def_at(static_cast<FakeItem *>(w.data[1 + i])->base.definition)->category].cur_index = i;
    for (int k = 0; k < 8; k++)
        w.discovered[k] = (random() >> 9) % 5 == 0 ? nullptr : def_ptr(random() >> 9);
    CInventory &inv = w.inv;
    inv.vftable = ptrv(g_inv_vtable);
    inv.unknown4 = 0x1234;
    inv.inited = null_defs ? 1 : (random() >> 12) % 3 != 0;
    inv.focus = pick_index(n);
    inv.types = w.types;
    inv.num_types = types_used;
    inv.items = {0x5ee144, &w.data[1], n, MAXI};
    inv.discovered = {0x5ee138, w.discovered, (random() >> 9) % 9, 8};
    inv.disposed = {ptrv(g_disposed_vtable), nullptr, 0, 0};
    w.out.vftable = ptrv(g_out_vtable);
    w.part_vftable = ptrv(g_part_vtable);
    w.part_vtable_slot36_result = (random() >> 9) % 4 == 0 ? 0 : ptrv(&inv);
    w.hide = allow.hide && (random() >> 9) % 2 ? random() & random() & 0xfff : 0;
}

// ---- Globals swapped during a case --------------------------------------------------------------------

struct Globals {
    DWORD data, count, reporter;
    BYTE report;
};
Globals g_saved;
void swap_in(unsigned seed, bool no_catalogue)
{
    g_saved = {*at<DWORD *>(ITEM_CATALOGUE_DATA), *at<DWORD *>(ITEM_CATALOGUE_COUNT), *at<DWORD *>(0x6c02cc),
               *at<BYTE *>(INVENTORY_REPORT_REPAIR)};
    if (!g_real) {
        *at<DWORD *>(ITEM_CATALOGUE_DATA) = no_catalogue ? 0 : ptrv(g_world.records);
        *at<DWORD *>(ITEM_CATALOGUE_COUNT) = NDEF;
    }
    *at<DWORD *>(0x6c02cc) = seed % 3 == 0 ? 0 : ptrv(&g_reporter);
    DWORD old;
    VirtualProtect(at<void *>(INVENTORY_REPORT_REPAIR), 1, PAGE_EXECUTE_READWRITE, &old);
    *at<BYTE *>(INVENTORY_REPORT_REPAIR) = (BYTE)(seed / 3 % 2);
}
void swap_out()
{
    *at<DWORD *>(ITEM_CATALOGUE_DATA) = g_saved.data;
    *at<DWORD *>(ITEM_CATALOGUE_COUNT) = g_saved.count;
    *at<DWORD *>(0x6c02cc) = g_saved.reporter;
    *at<BYTE *>(INVENTORY_REPORT_REPAIR) = g_saved.report;
}

// ---- The functions ---------------------------------------------------------------------------------

struct Fn {
    DWORD address;
    const char *name;
    Allow allow;
};
const Fn FNS[] = {
    {0x4251c0, "CountOfCategory", {true, true, false}},
    {0x425220, "CollectOfCategory", {true, true, false}},
    {0x425290, "CurIndex", {true, true, false}},
    {0x4252b0, "CurDefinition", {true, true, false}},
    {0x4252e0, "ChangingToItem", {true, true, false}},
    {0x425300, "SecondsToChange", {true, true, false}},
    {0x425320, "SecondsIntoChange", {true, true, false}},
    {0x425340, "FirstOfCategory", {false, true, false}},
    {0x425350, "PrevOfCategory", {false, true, false}},
    {0x425470, "NextOfCategory", {false, true, false}},
    {0x425740, "IndexOfSameDefinition", {true, true, false}},
    {0x4257a0, "IndexOfDefinitionIndex", {true, true, false}},
    {0x4257f0, "TotalCount", {true, true, false}},
    {0x425860, "FindOfClass", {true, true, false}},
    {0x4258b0, "FindByDefinitionIndex", {true, true, false}},
    {0x425900, "IndexOfDefinition", {true, true, false}},
    {0x425950, "PrimaryItem", {false, true, false}},
    {0x425990, "GetItem", {true, true, false}},
    {0x426070, "ItemInFocus", {true, true, false}},
    {0x426090, "RemoveAt", {false, false, false}},
    {0x426260, "Remove", {false, false, false}},
    {0x4262a0, "IndexOf", {true, true, false}},
    {0x4262e0, "Repair", {true, false, true}},
    {0x426600, "AnyItemSlot34", {true, true, false}},
    {0x426650, "IsDiscovered", {true, true, false}},
    {0x426740, "Undiscover", {true, true, false}},
    {0x4267e0, "LayerPart_GetInventory", {true, true, false}},
    {0x4267f0, "SetCurIndex", {true, true, false}},
    {0x426860, "Part_ItemCount", {true, true, false}},
    {0x4268d0, "Inventory_ItemCount", {true, true, false}},
};
constexpr int NFNS = sizeof FNS / sizeof FNS[0];

using F1 = DWORD(THISCALL *)(CInventory *, DWORD);
using F2 = DWORD(THISCALL *)(CInventory *, DWORD, DWORD);
using FF = float(THISCALL *)(CInventory *, DWORD);
using F0 = DWORD(THISCALL *)(CInventory *);
using FC = DWORD(__fastcall *)(void *, DWORD);

// The arguments of a case (drawn after the setup, the same on both sides)
struct Args {
    DWORD a, b;
    void *p;
};
Args draw_args(int f)
{
    World &w = g_world;
    const DWORD n = w.inv.items.count;
    const DWORD r = random() >> 6;
    Args x = {0, 0, nullptr};
    switch (f) {
    case 0: case 1: case 7: x.a = r % 8 == 0 ? g_primary : r % 7; break;
    case 2: case 3: case 4: case 5: case 6: case 27: x.a = r % NTYPES < 8 ? r % NTYPES : r % 7; break;
    case 8: case 9: x.a = pick_index(n); x.b = (random() >> 8) % 7; break;
    case 10:   // the argument is never null (the original reads its definition index)
        x.p = (random() >> 8) % 5 == 0 ? &w.items[(random() >> 8) % MAXI] : n ? w.data[1 + r % n] : nullptr;
        if (!x.p)
            x.p = &w.items[0];
        break;
    case 11: case 12: case 14: case 28: case 29: x.a = r % 4 == 0 ? r % (NDEF + 3) : def_index(r >> 2); break;
    case 13: {
        CClassInfo *c = g_ncls ? g_cls[r % (g_ncls < 8 ? g_ncls : 8)] : nullptr;
        if (c && (random() >> 9) % 3 == 0)
            c = c->parent;
        x.p = c;
        break;
    }
    case 15: x.p = r % 6 == 0 ? nullptr : def_ptr(r); break;
    case 17: x.a = pick_index(n); break;
    case 19: x.a = n ? r % n : 0; break;
    case 20: case 21: x.p = r % 5 == 0 ? &w.items[(r >> 3) % (MAXI + 1)] : n ? w.data[1 + r % n] : nullptr; break;
    case 23: x.a = random(); x.b = random(); break;
    case 24: case 25: x.p = r % 4 == 0 ? nullptr : def_ptr(r); break;
    case 26: x.p = &w.part_vftable; break;
    default: break;
    }
    if (f == 27)
        x.b = x.a, x.a = pick_index(n);
    if (f == 28)
        x.p = (random() >> 9) % 5 == 0 ? nullptr : &w.part_vftable;
    if (f == 29)
        x.p = (random() >> 9) % 5 == 0 ? nullptr : &w.inv;
    return x;
}

DWORD call(int f, const Args &x)
{
    CInventory *inv = &g_world.inv;
    const DWORD a = FNS[f].address;
    switch (f) {
    case 5: case 6: return bits(at<FF>(a)(inv, x.a));
    case 1: return at<F2>(a)(inv, x.a, ptrv(&g_world.out));
    case 8: case 9: case 27: return at<F2>(a)(inv, x.a, x.b);
    case 10: case 13: case 15: case 20: case 21: case 24: case 25: return at<F1>(a)(inv, ptrv(x.p));
    case 16: case 18: case 22: return at<F0>(a)(inv);
    case 23: return at<F2>(a)(inv, x.a, x.b);
    case 26: return at<F0>(a)(static_cast<CInventory *>(x.p));
    case 28: case 29: return at<FC>(a)(x.p, x.a);
    default: return at<F1>(a)(inv, x.a);
    }
}

void run_case(Tally &t, int f, unsigned seed)
{
    DWORD r[2];
    unsigned h[2];
    for (int side = 0; side < 2; side++) {
        g_side = side;
        g_nlog[side] = 0;
        g_loghash[side] = 2166136261u;
        setup(seed, FNS[f].allow, f == 19);
        const Args x = draw_args(f);
        swap_in(seed, null_definitions(FNS[f].allow, seed) && seed % 11 == 1);
        if (side == 0)
            hooks_out();
        r[side] = call(f, x);
        if (side == 0)
            hooks_in();
        swap_out();
        h[side] = fnv(2166136261u, &g_world, sizeof g_world);
    }
    char d[160];
    wsprintfA(d, "[%s seed %u: eax %x / %x, log %d / %d, world %x / %x]", FNS[f].name, seed, r[0], r[1], g_nlog[0],
              g_nlog[1], h[0], h[1]);
    t.check(r[0] == r[1] && same_log() && h[0] == h[1], "inventory", d);
}

// ---- Real data -----------------------------------------------------------------------------------------

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
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000)
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0x66))
            return false;
    return true;
}
bool is_object(const void *p, DWORD size)
{
    return ptrv(p) >= 0x10000 && !(ptrv(p) & 3) && readable(p, size) && *static_cast<const DWORD *>(p) >= 0x5e0000 &&
           *static_cast<const DWORD *>(p) < 0x610000;
}
// A character's vtable: slot 36 is 0x49a8c0 (the inventory at +0x108); reads only the first dword of p
bool has_character_vtable(const void *p)
{
    return is_object(p, 4) && *at<DWORD *>(*static_cast<const DWORD *>(p) + 36 * 4) == 0x49a8c0;
}

// The characters of the running game: the world's members and, one level down, the members of the live
// blocks they point into (check_real_objects.cpp, live_block_of: within the block, from the pointer to the
// block's end and at most 0x200 bytes); a character and its inventory are taken only at the start of a live
// block that holds them whole (or as static objects in the image), so a stale pointer into a freed or
// neighbouring block gives nothing (docs/DECOMPILATION.md section 12, "The real objects of field_streams
// on real x86")
constexpr int MAX_PARTS = 32;
constexpr DWORD CHARACTER_MIN = 0x10c;   // up to the inventory pointer at +0x108
void *g_parts[MAX_PARTS];
char g_part_from[MAX_PARTS][REAL_FROM];
int g_nparts, g_rejected;
void add_part(void *p, const char *from)
{
    for (int i = 0; i < g_nparts; i++)
        if (g_parts[i] == p)
            return;
    if (g_nparts == MAX_PARTS)
        return;
    int k = 0;
    for (; from[k] && k + 1 < REAL_FROM; k++)
        g_part_from[g_nparts][k] = from[k];
    g_part_from[g_nparts][k] = 0;
    g_parts[g_nparts++] = p;
}
// p is an object that fits whole into the live block starting there (or lies in the image)
bool live_whole(const void *p, DWORD size)
{
    DWORD start, bsize;
    if (ptrv(p) >= 0x400000 && ptrv(p) < IMAGE_END)
        return live_object_at(p);
    return live_block_of(ptrv(p), start, bsize) && start == ptrv(p) && bsize >= size;
}
void scan(const BYTE *block, DWORD size, int depth, DWORD from)
{
    if (!block || !readable(block, size))
        return;
    for (DWORD off = 0; off + 4 <= size; off += 4) {
        void *p = *reinterpret_cast<void *const *>(block + off);
        if (has_character_vtable(p)) {
            CInventory *inv = live_whole(p, CHARACTER_MIN) ? *reinterpret_cast<CInventory **>(static_cast<BYTE *>(p) + 0x108)
                                                          : nullptr;
            if (inv && is_object(inv, sizeof(CInventory)) && inv->vftable == INVENTORY_VTABLE && live_whole(inv, sizeof(CInventory))) {
                char where[REAL_FROM];
                if (depth == 0)
                    wsprintfA(where, "world +%x", off);
                else
                    wsprintfA(where, "world +%x, +%x", from, off);
                add_part(p, where);
            } else {
                g_rejected++;
            }
            continue;
        }
        DWORD start, bsize;
        if (depth == 0 && ptrv(p) >= 0x10000 && !(ptrv(p) & 3) && (ptrv(p) < 0x400000 || ptrv(p) >= IMAGE_END) &&
            live_block_of(ptrv(p), start, bsize)) {
            const DWORD rest = start + bsize - ptrv(p);
            scan(static_cast<const BYTE *>(p), rest < 0x200 ? rest : 0x200, 1, off);
        }
    }
}
// The characters of the level where the game keeps them: the CLayer (class 0x6c1d04, size 0x1d0) that
// find_real_objects reaches from the world, its grid of parts (columns +0x60, rows +0x64, cells +0x68, one
// pointer array per cell: vtable, data, count, capacity; checks_ai_world.cpp, level_parts), the cell array
// and each cell's data read only within their live blocks; up to MAX_PARTS in all, spread over the grid
constexpr int MAX_GRID = 1024;
void *g_grid[MAX_GRID];
int grid_characters(BYTE *layer, int &parts)
{
    int n = 0;
    parts = 0;
    DWORD start, size;
    if (!layer || !live_block_of(ptrv(layer), start, size) || start != ptrv(layer) || size < 0x6c)
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
        for (DWORD k = 0; k < count; k++) {
            void *p = data[k];
            parts++;
            if (!has_character_vtable(p))
                continue;
            CInventory *inv = live_whole(p, CHARACTER_MIN) ? *reinterpret_cast<CInventory **>(static_cast<BYTE *>(p) + 0x108)
                                                          : nullptr;
            if (!inv || !is_object(inv, sizeof(CInventory)) || inv->vftable != INVENTORY_VTABLE || !live_whole(inv, sizeof(CInventory))) {
                g_rejected++;
                continue;
            }
            bool dup = false;
            for (int j = 0; j < n && !dup; j++)
                dup = g_grid[j] == p;
            if (!dup && n < MAX_GRID)
                g_grid[n++] = p;
        }
    }
    return n;
}
void find_parts()
{
    g_nparts = g_rejected = 0;
    CClassInfo *layer_class = *at<CClassInfo **>(0x6c1d04);
    void *layer = nullptr;
    char layer_from[1][REAL_FROM] = {{0}};
    if (!layer_class || find_real_objects(&layer, layer_from, 1, &layer_class, 1, 0) != 1)
        layer = nullptr;
    if (!live_blocks_take())
        return;
    BYTE *world = *at<BYTE **>(0x6c2080);
    DWORD start = 0, size = 0;
    if (world && live_block_of(ptrv(world), start, size) && start == ptrv(world))
        scan(world, size < 0x1000 ? size : 0x1000, 0, 0);
    const int from_world = g_nparts;
    int parts = 0;
    const int ngrid = grid_characters(static_cast<BYTE *>(layer), parts);
    int fresh = 0;   // grid characters the world scan did not give
    for (int i = 0; i < ngrid; i++) {
        bool dup = false;
        for (int j = 0; j < from_world && !dup; j++)
            dup = g_parts[j] == g_grid[i];
        if (!dup)
            g_grid[fresh++] = g_grid[i];
    }
    for (int i = 0; i < fresh; i++)
        if (spread_pick(i, fresh, MAX_PARTS - from_world)) {
            char where[REAL_FROM];
            wsprintfA(where, "level grid, character %d of %d", i, fresh);
            add_part(g_grid[i], where);
        }
    live_blocks_log("real inventories");
    log("  real inventories: %d characters with an inventory, %d from the world (block %x bytes), %d of %d more in the "
        "level grid (CLayer %x, %s; %d parts); %d pointers with a character's vtable not taken: no inventory, or no live block holds the "
        "character and its inventory there",
        g_nparts, from_world, start == ptrv(world) ? size : 0, g_nparts - from_world, fresh, ptrv(layer),
        layer ? layer_from[0] : "not found", parts, g_rejected);
    for (int i = 0; i < g_nparts; i++)
        log("  real inventory %d: character %x (%s)", i, ptrv(g_parts[i]), g_part_from[i]);
    live_blocks_free();
}

template <typename C> void two_sides(Tally &t, const char *what, DWORD arg, C call_it)
{
    DWORD r[2];
    for (int side = 0; side < 2; side++) {
        if (side == 0)
            hooks_out();
        r[side] = call_it();
        if (side == 0)
            hooks_in();
    }
    char d[120];
    wsprintfA(d, "[real %s %x: %x / %x]", what, arg, r[0], r[1]);
    t.check(r[0] == r[1], "inventory", d);
}

void test_real(Tally &t, int &inventories, int &items)
{
    find_parts();
    for (int pi = 0; pi < g_nparts; pi++) {
        void *part = g_parts[pi];
        CInventory *inv = *reinterpret_cast<CInventory **>(static_cast<BYTE *>(part) + 0x108);
        const DWORD n = inv->items.count;
        log("  real inventory %d: %u items, %u categories, focus %d, inited %u", pi, n, inv->num_types, (int)inv->focus,
            inv->inited);
        inventories++;
        items += (int)n;
        two_sides(t, "GetInventory", 0, [&] { return at<F0>(0x4267e0)(static_cast<CInventory *>(part)); });
        two_sides(t, "PrimaryItem", 0, [&] { return at<F0>(0x425950)(inv); });
        if (n)
            two_sides(t, "ItemInFocus", 0, [&] { return at<F0>(0x426070)(inv); });
        for (DWORD c = 0; c < inv->num_types + 2; c++) {
            if (inv->inited)
                two_sides(t, "CountOfCategory", c, [&] { return at<F1>(0x4251c0)(inv, c); });
            two_sides(t, "FirstOfCategory", c, [&] { return at<F1>(0x425340)(inv, c); });
            for (DWORD from = 0xffffffff; from != n + 2; from++) {
                two_sides(t, "PrevOfCategory", c, [&] { return at<F2>(0x425350)(inv, from, c); });
                two_sides(t, "NextOfCategory", c, [&] { return at<F2>(0x425470)(inv, from, c); });
            }
            if (c >= inv->num_types)
                continue;
            two_sides(t, "CurIndex", c, [&] { return at<F1>(0x425290)(inv, c); });
            two_sides(t, "CurDefinition", c, [&] { return at<F1>(0x4252b0)(inv, c); });
            two_sides(t, "ChangingToItem", c, [&] { return at<F1>(0x4252e0)(inv, c); });
            two_sides(t, "SecondsToChange", c, [&] { return bits(at<FF>(0x425300)(inv, c)); });
            two_sides(t, "SecondsIntoChange", c, [&] { return bits(at<FF>(0x425320)(inv, c)); });
        }
        for (DWORD i = 0; i < n + 2; i++) {
            two_sides(t, "GetItem", i, [&] { return at<F1>(0x425990)(inv, i); });
            CInventoryItem *it = i < n ? static_cast<CInventoryItem *>(inv->items.data[i]) : nullptr;
            if (!it)
                continue;
            const DWORD def = it->definition;
            void *d = at<void *(THISCALL *)(CInventoryItem *)>(0x4dd480)(it);
            two_sides(t, "IndexOf", i, [&] { return at<F1>(0x4262a0)(inv, ptrv(it)); });
            two_sides(t, "IndexOfSameDefinition", i, [&] { return at<F1>(0x425740)(inv, ptrv(it)); });
            two_sides(t, "IndexOfDefinitionIndex", def, [&] { return at<F1>(0x4257a0)(inv, def); });
            two_sides(t, "TotalCount", def, [&] { return at<F1>(0x4257f0)(inv, def); });
            two_sides(t, "FindByDefinitionIndex", def, [&] { return at<F1>(0x4258b0)(inv, def); });
            two_sides(t, "IndexOfDefinition", def, [&] { return at<F1>(0x425900)(inv, ptrv(d)); });
            two_sides(t, "Part_ItemCount", def, [&] { return at<FC>(0x426860)(part, def); });
            two_sides(t, "Inventory_ItemCount", def, [&] { return at<FC>(0x4268d0)(inv, def); });
            two_sides(t, "IsDiscovered", def, [&] { return at<F1>(0x426650)(inv, ptrv(d)); });
        }
        for (DWORD k = 0; k < inv->discovered.count; k++) {
            void *d = inv->discovered.data[k];
            two_sides(t, "IsDiscovered", k, [&] { return at<F1>(0x426650)(inv, ptrv(d)); });
        }
        for (int c = 0; c < g_ncls; c++)
            two_sides(t, "FindOfClass", (DWORD)c, [&] { return at<F1>(0x425860)(inv, ptrv(g_cls[c])); });
    }
}

}  // namespace

ZAX_CHECKS(inventory)
{
    init_vtables();
    g_cls = cls;
    g_ncls = n;
    g_primary = (DWORD)at<int(THISCALL *)(void *, const char *, int)>(0x523df0)(at<void *>(ITEM_CATEGORIES), "Primary", -1);
    // Classes for the fake items: CInventoryItem and its relatives first, so FindOfClass finds some
    CClassInfo **picked = g_picked;
    int np = 0;
    const char *const names[] = {"CInventoryItem", "CCountInventoryItem", "CEnergyItem", "CInventoryItemBase", "CObject",
                                 "CChargerInventoryItem", "CSegmentedInventoryItem", "CContinuousFireWeaponItem"};
    for (const char *want : names)
        for (int i = 0; i < n && np < 8; i++)
            if (crt_stricmp(cls[i]->name, want) == 0) {
                picked[np++] = cls[i];
                break;
            }
    Tally t{"inventory: item bookkeeping on fake inventories"};
    for (int f = 0; f < NFNS; f++)
        for (unsigned k = 0; k < (unsigned)scaled(90); k++) {
            if (g_trace)
                log("  inventory case %s %u", FNS[f].name, k);
            g_cls = picked;
            g_ncls = np;
            run_case(t, f, 7000u * (unsigned)f + k);
        }
    t.report();
    // The real catalogue: up to NDEF loaded records, spread over the catalogue
    Tally cat{"inventory: fake inventories with the real item catalogue"};
    const DWORD records = *at<DWORD *>(ITEM_CATALOGUE_DATA), count = *at<DWORD *>(ITEM_CATALOGUE_COUNT);
    DWORD loaded = 0;
    g_nloaded = 0;
    g_real_categories = *at<DWORD *>(0x6c0834);
    // In the test level hardly any definition is loaded yet: every (count / NDEF + 1)th record is
    // loaded through the original 0x4dd480 (as when the player gets such an item)
    const DWORD step = count / NDEF + 1;
    for (DWORD i = 0; records && i < count; i++) {
        const BYTE *rec = at<BYTE *>(records + i * 0x80);
        if (i % step == 0 && !*(ItemDefinition *const *)(rec + 0x78) && !rec[0x7f]) {
            CInventoryItem probe = {};
            probe.definition = i;
            original<ItemDefinition *(THISCALL *)(CInventoryItem *)>(0x4dd480)(&probe);
        }
        ItemDefinition *d = *(ItemDefinition *const *)(rec + 0x78);
        if (!d || rec[0x7f] || d->category >= NTYPES)
            continue;
        if (loaded++ < (DWORD)NDEF)
            g_loaded[g_nloaded++] = i;
    }
    log("  item catalogue: %u records, %u loaded, %d used, %u categories", count, loaded, g_nloaded, g_real_categories);
    if (g_nloaded && g_real_categories <= NTYPES) {
        g_real = true;
        for (int f = 0; f < NFNS; f++)
            for (unsigned k = 0; k < (unsigned)scaled(40); k++) {
                g_cls = picked;
                g_ncls = np;
                run_case(cat, f, 9000u * (unsigned)f + k);
            }
        g_real = false;
    }
    cat.report();
    Tally real{"inventory: inventories of the running game (real data)"};
    g_cls = cls;
    g_ncls = n;
    int inventories = 0, items = 0;
    test_real(real, inventories, items);
    log("  %d inventories, %d items, Primary = %d", inventories, items, (int)g_primary);
    real.report();
}

}  // namespace zax
