// The inventory's item bookkeeping reimplemented (see inventory.h).
#include "inventory.h"

namespace zax {

// Functions of the exe; through their exe addresses so Disable= applies
static ItemDefinition *definition_of(const CInventoryItem *item) { return fn<ItemDefinition *(THISCALL *)(const CInventoryItem *)>(0x4dd480)(item); }
static DWORD definition_index(const CInventoryItem *item) { return fn<DWORD(THISCALL *)(const CInventoryItem *)>(0x482de0)(item); }
static bool item_before(CInventoryItem *a, CInventoryItem *b, DWORD ia, DWORD ib) { return (BYTE)fn<DWORD(THISCALL *)(CInventoryItem *, CInventoryItem *, DWORD, DWORD)>(0x42a3b0)(a, b, ia, ib) != 0; }
static bool item_after(CInventoryItem *a, CInventoryItem *b, DWORD ia, DWORD ib) { return (BYTE)fn<DWORD(THISCALL *)(CInventoryItem *, CInventoryItem *, DWORD, DWORD)>(0x42a3f0)(a, b, ia, ib) != 0; }
static bool is_kind_of(void *object, void *class_info) { return (BYTE)fn<DWORD(THISCALL *)(void *, void *)>(0x416790)(object, class_info) != 0; }
static int resource_find(DWORD list, const char *name, int hint) { return fn<int(THISCALL *)(void *, const char *, int)>(0x523df0)(at<void *>(list), name, hint); }
static DWORD crt_memmove(void *d, const void *s, DWORD n) { return fn<DWORD(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }

static CInventoryItem *item(CInventory *inv, DWORD i) { return vcall<CInventoryItem *>(inv, 26, i); }
static DWORD select(CInventoryItem *it, CInventory *inv) { return vcall<DWORD>(it, 44, inv); }
static DWORD deselect(CInventoryItem *it, CInventory *inv) { return vcall<DWORD>(it, 45, inv); }

// "Corrupt inventory repaired... -Loren (04-23-2001)"
constexpr DWORD TEXT_REPAIRED = 0x60b794;

// The selection change shared by RemoveAt, Repair and SetCurIndex: nothing happens if the new item is
// the old one; otherwise the old one is deselected, the index stored and the new one selected
DWORD CInventory::set_cur(DWORD category, DWORD index, CInventoryItem *old, CInventoryItem *now)
{
    if (now && old == now)
        return (DWORD)(uintptr_t)now;
    if (old)
        deselect(old, this);
    types[category].cur_index = index;
    if (now)
        return select(now, this);
    return (DWORD)(uintptr_t)types;
}

// ---- Categories ------------------------------------------------------------------------------------

int THISCALL CInventory::CountOfCategory(DWORD category)
{
    if (!inited)
        Repair();
    int n = 0;
    for (DWORD i = 0; i < items.count; i++) {
        CInventoryItem *it = item(this, i);
        if (!it)
            continue;
        ItemDefinition *d = definition_of(it);
        if (d && d->category == category)
            n++;
    }
    return n;
}

DWORD THISCALL CInventory::CollectOfCategory(DWORD category, ZPtrArray *out)
{
    if (!inited)
        Repair();
    // The index is passed by reference and read back after each call
    volatile DWORD i = 0;
    while (i < items.count) {
        CInventoryItem *it = item(this, i);
        if (it) {
            ItemDefinition *d = definition_of(it);
            if (d && d->category == category)
                vcall<DWORD>(out, 2, (DWORD *)&i);
        }
        i = i + 1;
    }
    return i;
}

DWORD THISCALL CInventory::CurIndex(DWORD category) { return types[category].cur_index; }

ItemDefinition *THISCALL CInventory::CurDefinition(DWORD category)
{
    CInventoryItem *it = item(this, types[category].cur_index);
    return it ? definition_of(it) : nullptr;
}

CInventoryItem *THISCALL CInventory::ChangingToItem(DWORD category) { return item(this, types[category].changing_to); }
float THISCALL CInventory::SecondsToChange(DWORD category) { return types[category].seconds_to_change; }
float THISCALL CInventory::SecondsIntoChange(DWORD category) { return types[category].seconds_into_change; }

DWORD THISCALL CInventory::FirstOfCategory(DWORD category) { return NextOfCategory(0xffffffff, category); }

// The neighbour of item `from` in the order of the category's items, wrapping around: Prev looks for
// the last item before `from` (else the last item of all), Next for the first one after it (else the
// first of all). The item list may not hold null items (the original resolves their definition).
template <bool PREV> DWORD CInventory::neighbour(DWORD from, DWORD category)
{
    bool (*const first)(CInventoryItem *, CInventoryItem *, DWORD, DWORD) = PREV ? item_before : item_after;
    bool (*const second)(CInventoryItem *, CInventoryItem *, DWORD, DWORD) = PREV ? item_after : item_before;
    DWORD extreme = 0xffffffff, best = 0xffffffff;
    if (!items.count)
        return extreme;
    for (DWORD i = 0; i < items.count; i++) {
        CInventoryItem *it = item(this, i);
        ItemDefinition *d = definition_of(it);
        if (!d || d->category != category)
            continue;
        if (extreme == 0xffffffff || first(item(this, extreme), it, extreme, i))
            extreme = i;
        if (!item(this, from) || !definition_of(item(this, from)))
            continue;
        if (definition_of(item(this, from))->category != category)
            continue;
        CInventoryItem *x = item(this, i);
        if (!second(item(this, from), x, from, i))
            continue;
        if (best == 0xffffffff || first(item(this, best), it, best, i))
            best = i;
    }
    return best != 0xffffffff ? best : extreme;
}

DWORD THISCALL CInventory::PrevOfCategory(DWORD from, DWORD category) { return neighbour<true>(from, category); }
DWORD THISCALL CInventory::NextOfCategory(DWORD from, DWORD category) { return neighbour<false>(from, category); }

// ---- Searches --------------------------------------------------------------------------------------

DWORD THISCALL CInventory::IndexOfSameDefinition(CInventoryItem *other)
{
    for (DWORD i = 0; i < items.count; i++) {
        CInventoryItem *it = item(this, i);
        if (it && !it->marked_for_deletion && definition_index(it) == definition_index(other))
            return i;
    }
    return 0xffffffff;
}

DWORD THISCALL CInventory::IndexOfDefinitionIndex(DWORD definition)
{
    for (DWORD i = 0; i < items.count; i++) {
        CInventoryItem *it = item(this, i);
        if (it && !it->marked_for_deletion && definition_index(it) == definition)
            return i;
    }
    return 0xffffffff;
}

int THISCALL CInventory::TotalCount(DWORD definition)
{
    int n = 0;
    for (DWORD i = 0; i < items.count; i++) {
        CInventoryItem *it = item(this, i);
        if (!it || it->marked_for_deletion || definition_index(it) != definition)
            continue;
        int c = vcall<int>(it, 41);
        n += c == -1 ? 1 : c;
    }
    return n;
}

CInventoryItem *THISCALL CInventory::FindOfClass(void *class_info)
{
    for (DWORD i = 0; i < items.count; i++) {
        CInventoryItem *it = item(this, i);
        if (it && is_kind_of(it, class_info))
            return it;
    }
    return nullptr;
}

CInventoryItem *THISCALL CInventory::FindByDefinitionIndex(DWORD definition)
{
    for (DWORD i = 0; i < items.count; i++) {
        CInventoryItem *it = item(this, i);
        if (it && definition_index(it) == definition)
            return it;
    }
    return nullptr;
}

DWORD THISCALL CInventory::IndexOfDefinition(ItemDefinition *definition)
{
    for (DWORD i = 0; i < items.count; i++) {
        CInventoryItem *it = item(this, i);
        if (it && !it->marked_for_deletion && definition_of(it) == definition)
            return i;
    }
    return 0xffffffff;
}

CInventoryItem *THISCALL CInventory::PrimaryItem()
{
    DWORD i = NextOfCategory(0xffffffff, resource_find(glob_addr(ITEM_CATEGORIES), "Primary", -1));
    return i != 0xffffffff ? item(this, i) : nullptr;
}

CInventoryItem *THISCALL CInventory::GetItem(DWORD i) { return i < items.count ? (CInventoryItem *)items.data[i] : nullptr; }

DWORD THISCALL CInventory::ItemInFocus()
{
    if (!items.count)
        focus = 0xffffffff;
    return focus;
}

DWORD THISCALL CInventory::IndexOf(CInventoryItem *it)
{
    for (DWORD i = 0; i < items.count; i++)
        if (it == item(this, i))
            return i;
    return 0xffffffff;
}

// ---- Removing ----------------------------------------------------------------------------------------

// The item moves to the disposed list. The indices behind it move down; a category without a
// selection selects its first remaining item. The item at i must exist (its definition is read before
// any check).
DWORD THISCALL CInventory::RemoveAt(DWORD i)
{
    const DWORD category = definition_of((CInventoryItem *)items.data[i])->category;
    if (types[category].cur_index == i) {
        CInventoryItem *old = item(this, types[category].cur_index), *none = item(this, 0xffffffff);
        set_cur(category, 0xffffffff, old, none);
    }
    vcall<DWORD>(&disposed, 2, &items.data[i]);
    const DWORD n = items.count;
    if (i < n) {
        if (i + 1 < n)
            crt_memmove(&items.data[i], &items.data[i + 1], (n - (i + 1)) * 4);
        items.count--;
    }
    for (DWORD k = 0; k < num_types; k++) {
        CItemTypeData &t = types[k];
        if (t.cur_index != 0xffffffff && t.cur_index >= i)
            t.cur_index--;
        if (t.changing_to != 0xffffffff && t.changing_to >= i)
            t.changing_to--;
    }
    DWORD eax = focus;
    if (focus != 0xffffffff) {
        if (focus > i)
            eax = --focus;
        else if (focus == i)
            focus = 0;
    }
    if (types[category].cur_index != 0xffffffff)
        return eax;
    if (!items.count)
        return 0;
    for (DWORD j = 0; j < items.count; j++) {
        if (definition_of(item(this, j))->category != category)
            continue;
        CInventoryItem *old = item(this, types[category].cur_index), *now = item(this, j);
        return set_cur(category, j, old, now);
    }
    return items.count;
}

DWORD THISCALL CInventory::Remove(CInventoryItem *it)
{
    DWORD i = 0;
    for (; i < items.count; i++)
        if (it == item(this, i))
            break;
    if (i >= items.count)
        i = 0xffffffff;
    return RemoveAt(i);
}

// Makes the selection consistent: the items' selected flags, the categories' selected and "changing
// to" indices, null entries removed (and deleted), every category with items selects its first one,
// the focus within the list. The reporter *0x6c02cc gets slot 17 with a text if something was wrong,
// the report byte 0x60b5d4 is set and the inventory was inited before.
DWORD THISCALL CInventory::Repair()
{
    bool repaired = false;
    for (DWORD i = 0; i < items.count; i++) {
        CInventoryItem *it = item(this, i);
        if (!it)
            continue;
        const DWORD cur = types[definition_of(it)->category].cur_index;
        if ((DWORD)(cur == i) == it->selected)
            continue;
        repaired = true;
        if (cur == i)
            select(it, this);
        else
            deselect(it, this);
    }
    for (DWORD k = 0; k < num_types; k++) {
        const DWORD cur = types[k].cur_index;
        if (cur != 0xffffffff && (cur >= items.count || !items.data[cur] || definition_of((CInventoryItem *)items.data[cur])->category != k)) {
            CInventoryItem *old = item(this, types[k].cur_index), *none = item(this, 0xffffffff);
            set_cur(k, 0xffffffff, old, none);
            repaired = true;
        }
        const DWORD next = types[k].changing_to;
        if (next != 0xffffffff && (next >= items.count || !items.data[next] || definition_of((CInventoryItem *)items.data[next])->category != k)) {
            repaired = true;
            types[k].changing_to = 0xffffffff;
        }
    }
    for (DWORD j = 0; j < items.count; j++) {
        if (CInventoryItem *it = item(this, j)) {
            const DWORD category = definition_of(it)->category;
            if (types[category].cur_index != 0xffffffff)
                continue;
            CInventoryItem *none = item(this, 0xffffffff), *now = item(this, j);
            set_cur(category, j, none, now);
            repaired = true;
            continue;
        }
        if (void *p = items.data[j]) {
            vcall<DWORD>(p, 1, (DWORD)1);
            items.data[j] = nullptr;
        }
        const DWORD n = items.count;
        if (j < n) {
            if (j + 1 < n)
                crt_memmove(&items.data[j], &items.data[j + 1], (n - (j + 1)) * 4);
            items.count--;
        }
        j--;   // wraps for the first item: then nothing below moves
        for (DWORD k = 0; k < num_types; k++) {
            CItemTypeData &t = types[k];
            if (t.cur_index > j && t.cur_index != 0xffffffff)
                t.cur_index--;
            if (t.changing_to > j && t.changing_to != 0xffffffff)
                t.changing_to--;
        }
        if (focus > j && focus != 0xffffffff)
            focus--;
        repaired = true;
    }
    DWORD eax = items.count;
    if (focus >= items.count && (focus != 0xffffffff || items.count)) {
        eax = focus = items.count ? 0 : 0xffffffff;
        repaired = true;
    }
    if (repaired) {
        const BYTE report = *glob<BYTE *>(INVENTORY_REPORT_REPAIR);
        eax = (eax & ~0xffu) | report;
        if (report) {
            void *reporter = *glob<void **>(0x6c02cc);
            if (reporter) {
                eax = (eax & ~0xffu) | inited;
                if (inited)
                    eax = vcall<DWORD>(reporter, 17, glob<const char *>(TEXT_REPAIRED));
            }
        }
    }
    inited = 1;
    return eax;
}

// ---- The rest -------------------------------------------------------------------------------------

DWORD THISCALL CInventory::AnyItemSlot34(DWORD a, DWORD b)
{
    BYTE any = 0;
    DWORD eax = 0;
    for (DWORD i = 0; i < items.count; i++) {
        if (CInventoryItem *it = item(this, i))
            any |= (BYTE)vcall<DWORD>(it, 34, a, b);
        eax = items.count;
    }
    return (eax & ~0xffu) | any;
}

static DWORD discovered_index(const ZPtrArray &a, void *definition)
{
    for (DWORD i = 0; i < a.count; i++)
        if (a.data[i] == definition)
            return i;
    return 0xffffffff;
}

DWORD THISCALL CInventory::IsDiscovered(void *definition)
{
    const DWORD i = discovered_index(discovered, definition);
    return (i & ~0xffu) | (i != 0xffffffff);
}

DWORD THISCALL CInventory::Undiscover(void *definition)
{
    const DWORD n = discovered.count;
    if (!n)
        return 0;
    const DWORD i = discovered_index(discovered, definition);
    if (i == 0xffffffff)
        return n;
    DWORD eax = i + 1;
    if (i + 1 < n)
        eax = crt_memmove(&discovered.data[i], &discovered.data[i + 1], (n - (i + 1)) * 4);
    discovered.count--;
    return eax;
}

DWORD THISCALL CInventory::SetCurIndex(DWORD index, DWORD category)
{
    CInventoryItem *old = item(this, types[category].cur_index), *now = item(this, index);
    return set_cur(category, index, old, now);
}

// 0x4267e0 jumps to slot 36 with the caller's stack (the characters' inventory at +0x108, the other
// layer parts ask their owner +0xc)
extern "C" void __cdecl zax_layer_part_get_inventory();
__asm__(".text\n"
        ".globl _zax_layer_part_get_inventory\n"
        "_zax_layer_part_get_inventory:\n"
        "  movl (%ecx), %eax\n"
        "  jmpl *0x90(%eax)\n");

// The count of the first item with the definition index (-1 means 0 here), 0 without one
int __fastcall Inventory_ItemCount(CInventory *inv, DWORD definition)
{
    if (!inv)
        return 0;
    for (DWORD i = 0; i < inv->items.count; i++) {
        CInventoryItem *it = item(inv, i);
        if (it && definition_index(it) == definition) {
            int c = vcall<int>(it, 41);
            return c == -1 ? 0 : c;
        }
    }
    return 0;
}

int __fastcall Part_ItemCount(void *part, DWORD definition)
{
    if (!part)
        return 0;
    CInventory *inv = vcall<CInventory *>(part, 36);
    if (!inv)
        return 0;
    return Inventory_ItemCount(inv, definition);
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(inventory)
{
    const Hook list[] = {
        {0x4251c0, ptr(&CInventory::CountOfCategory), "CInventory::CountOfCategory", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8a, 0x46, 0x08}},
        {0x425220, ptr(&CInventory::CollectOfCategory), "CInventory::CollectOfCategory", 7, {0x51, 0x56, 0x8b, 0xf1, 0x8a, 0x46, 0x08, 0x84}},
        {0x425290, ptr(&CInventory::CurIndex), "CInventory::CurIndex", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x49, 0x10, 0x8d}},
        {0x4252b0, ptr(&CInventory::CurDefinition), "CInventory::CurDefinition", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x10, 0x8d}},
        {0x4252e0, ptr(&CInventory::ChangingToItem), "CInventory::ChangingToItem", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x10, 0x8d}},
        {0x425300, ptr(&CInventory::SecondsToChange), "CInventory::SecondsToChange", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x49, 0x10, 0x8d}},
        {0x425320, ptr(&CInventory::SecondsIntoChange), "CInventory::SecondsIntoChange", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x49, 0x10, 0x8d}},
        {0x425340, ptr(&CInventory::FirstOfCategory), "CInventory::FirstOfCategory", 5, {0x8b, 0x44, 0x24, 0x04, 0x50, 0x6a, 0xff, 0xe8}},
        {0x425350, ptr(&CInventory::PrevOfCategory), "CInventory::PrevOfCategory", 5, {0x83, 0xec, 0x08, 0x53, 0x56, 0x8b, 0xf1, 0x57}},
        {0x425470, ptr(&CInventory::NextOfCategory), "CInventory::NextOfCategory", 5, {0x83, 0xec, 0x08, 0x53, 0x56, 0x8b, 0xf1, 0x57}},
        {0x425740, ptr(&CInventory::IndexOfSameDefinition), "CInventory::IndexOfSameDefinition", 6, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff}},
        {0x4257a0, ptr(&CInventory::IndexOfDefinitionIndex), "CInventory::IndexOfDefinitionIndex", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b}},
        {0x4257f0, ptr(&CInventory::TotalCount), "CInventory::TotalCount", 5, {0x53, 0x55, 0x57, 0x8b, 0xf9, 0x33, 0xed, 0x33}},
        {0x425860, ptr(&CInventory::FindOfClass), "CInventory::FindOfClass", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x57}},
        {0x4258b0, ptr(&CInventory::FindByDefinitionIndex), "CInventory::FindByDefinitionIndex", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x57}},
        {0x425900, ptr(&CInventory::IndexOfDefinition), "CInventory::IndexOfDefinition", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b}},
        {0x425950, ptr(&CInventory::PrimaryItem), "CInventory::PrimaryItem", 5, {0x56, 0x8b, 0xf1, 0x6a, 0xff, 0x68, 0x80, 0xb7}},
        {0x425990, ptr(&CInventory::GetItem), "CInventory::GetItem", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x20, 0x3b}},
        {0x426070, ptr(&CInventory::ItemInFocus), "CInventory::ItemInFocus", 5, {0x8b, 0x41, 0x20, 0x85, 0xc0, 0x75, 0x07, 0xc7}},
        {0x426090, ptr(&CInventory::RemoveAt), "CInventory::RemoveAt", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x426260, ptr(&CInventory::Remove), "CInventory::Remove", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b}},
        {0x4262a0, ptr(&CInventory::IndexOf), "CInventory::IndexOf", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b}},
        {0x4262e0, ptr(&CInventory::Repair), "CInventory::Repair", 5, {0x83, 0xec, 0x10, 0x53, 0x55, 0x56, 0x8b, 0xf1}},
        {0x426600, ptr(&CInventory::AnyItemSlot34), "CInventory::AnyItemSlot34", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x32, 0xdb, 0x33}},
        {0x426650, ptr(&CInventory::IsDiscovered), "CInventory::IsDiscovered", 8, {0x8b, 0x51, 0x30, 0x56, 0x8b, 0x74, 0x24, 0x08}},
        {0x426740, ptr(&CInventory::Undiscover), "CInventory::Undiscover", 6, {0x53, 0x56, 0x8b, 0x74, 0x24, 0x0c, 0x57, 0x8b}},
        {0x4267e0, (void *)&zax_layer_part_get_inventory, "LayerPart_GetInventory", 8, {0x8b, 0x01, 0xff, 0xa0, 0x90, 0x00, 0x00, 0x00}},
        {0x4267f0, ptr(&CInventory::SetCurIndex), "CInventory::SetCurIndex", 5, {0x8b, 0x44, 0x24, 0x08, 0x53, 0x55, 0x56, 0x57}},
        {0x426860, (void *)&Part_ItemCount, "Part_ItemCount", 5, {0x55, 0x8b, 0xea, 0x85, 0xc9, 0x74, 0x5e, 0x8b}},
        {0x4268d0, (void *)&Inventory_ItemCount, "Inventory_ItemCount", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x85, 0xf6, 0x57}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
