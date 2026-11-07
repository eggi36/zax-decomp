// The HUD's render data (phase 4, rendering): what CInventoryHUDRenderData and its classes send and receive in
// a net game, slots 23 (write the changed values), 24 (write all), 25 (read) and 26 (anything changed), and the
// setters the HUD renderers call. Their classes are in hud_classes.cpp.
//
// Layouts [confirmed: RegisterFields, the slots below]:
//  - CInventoryHUDRenderData (0x14): +8 the opacity (float; sent as a byte, opacity * 255 (0x5eb728),
//    received as byte / 255 (0x5f172c)), +0xc the item catalogue index (sent as a word token of the
//    catalogue 0x6c0c08), +0x10 the changed bits (1 opacity, 2 index).
//  - CCompassRenderData (0x1c): +0x14 "Display Frame", +0x15 "Any Change", +0x16 "Frame Changed" (bytes),
//    +0x18 "Frame Number"; sent as a byte (0 hidden, 1 shown, 3 shown with a new frame) and the frame as a
//    word; read back sign-extended.
//  - CEnergyHUDRenderData (0x20): +0x14 "Capacity", +0x18 "Charge" (floats, sent raw), +0x1c the changed
//    bits.
//  - CShieldHUDRenderData (0x28): +0x14 "Old ListSize", +0x18 an embedded pointer array of
//    CEnergyHUDRenderData ("Sheild List": +0x1c data, +0x20 count, +0x24 capacity); sent as a dword of
//    bits (1 the count changed, 2 << i the element i is new or changed), the count as a byte, then every
//    new element whole and every changed one's changes.
//  - CWeaponHUDRenderData (0x28): +0x14 "Rounds", +0x18 "Continuous Fire Rounds", +0x1c "Batt Capacity",
//    +0x20 "Batt Charge", +0x24 the changed bits (1 rounds, 2 capacity, 4 charge). What it sends depends on
//    the item definition of +0xc: with ammunition the rounds (a float for a continuous fire weapon, else a
//    word), without the battery's capacity and charge.
// The packet functions (CNetPacket, 0x520490-0x520da0) are the exe's. Floats are compared as the original
// does (fcom, test ah, 0x40: a NaN never counts as changed). eax of the void slots is not reproduced.
#include "ai_helpers.h"
#include "x87.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD ITEM_LIST = 0x6c0c08, ITEM_RECORDS = 0x6c0c38, ITEM_COUNT = 0x6c0c3c, ITEM_FOLDER = 0x6c0c40;
constexpr DWORD C_255 = 0x5eb728, C_INV_255 = 0x5f172c;

void write_byte(void *p, DWORD v) { fn<void(THISCALL *)(void *, DWORD)>(0x520820)(p, v); }
void write_word(void *p, DWORD v) { fn<void(THISCALL *)(void *, DWORD)>(0x5209c0)(p, v); }
void write_dword(void *p, DWORD v) { fn<void(THISCALL *)(void *, DWORD)>(0x520940)(p, v); }
void write_token(void *p, DWORD v, DWORD list) { fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x520da0)(p, v, list); }
void read_byte(void *p, void *v) { fn<void(THISCALL *)(void *, void *)>(0x520490)(p, v); }
void read_word(void *p, void *v) { fn<void(THISCALL *)(void *, void *)>(0x520510)(p, v); }
void read_dword(void *p, void *v) { fn<void(THISCALL *)(void *, void *)>(0x520550)(p, v); }
void read_token(void *p, void *v, DWORD list) { fn<void(THISCALL *)(void *, void *, DWORD)>(0x520670)(p, v, list); }

bool has_ammunition(BYTE *def) { return fn<bool(THISCALL *)(void *)>(0x4d77c0)(def); }
bool continuous_fire(BYTE *def) { return fn<bool(THISCALL *)(void *)>(0x5b7640)(def); }

// The catalogue object of an index as slots 23 and 24 look it up (loaded through the list's slot 37 when
// missing, list_object); null without records or while the record is loading
BYTE *catalogue_object_loading(DWORD i)
{
    if (!*glob<BYTE **>(ITEM_RECORDS))
        return nullptr;
    return list_object(glob_addr(ITEM_LIST), glob_addr(ITEM_RECORDS), glob_addr(ITEM_COUNT), glob_addr(ITEM_FOLDER), i);
}
// The same as slot 25 does it (DefinitionList_LoadRecord 0x55e090 when missing)
BYTE *catalogue_object_record(DWORD i)
{
    BYTE *records = *glob<BYTE **>(ITEM_RECORDS);
    if (!records)
        return nullptr;
    if (i >= *glob<DWORD *>(ITEM_COUNT))
        i = 0;
    const DWORD off = i << 7;
    if (records[off + 0x7f])
        return nullptr;
    if (!dw(records + off, 0x78)) {
        fn<void(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(ITEM_LIST), i);
        records = *glob<BYTE **>(ITEM_RECORDS);
    }
    return bp(dw(records + off, 0x78));
}
// A game item definition (CZaxInventoryItemDefinition) or null
BYTE *zax_definition(BYTE *o) { return o && is_kind_of(o, zax_item_definition_class()) ? o : nullptr; }

// A float field set from a value when it differs (fcom, test ah, 0x40), with a changed bit
void set_float(BYTE *o, DWORD off, DWORD bits, DWORD status, DWORD bit)
{
    if (!c3(fld(fl<float>(o, off)), fld(raw_float(bits)))) {
        dw(o, off) = bits;
        dw(o, status) |= bit;
    }
}

}  // namespace

// ---- CInventoryHUDRenderData ------------------------------------------------------------------------------

// 0x5ac8a0 (slot 23: packet): the changed bits, the changed values; the bits cleared
static void THISCALL HudData_WriteChanged(BYTE *self, void *packet)
{
    write_byte(packet, self[0x10]);
    if (self[0x10] & 1)
        write_byte(packet, (DWORD)x87_ftol(fld(fl<float>(self, 8)) * cst(glob_addr(C_255))));
    if (self[0x10] & 2)
        write_token(packet, (WORD)dw(self, 0xc), glob_addr(ITEM_LIST));
    dw(self, 0x10) = 0;
}
// 0x5ac900 (slot 24: packet): everything
static void THISCALL HudData_WriteAll(BYTE *self, void *packet)
{
    write_byte(packet, 3);
    write_byte(packet, (DWORD)x87_ftol(fld(fl<float>(self, 8)) * cst(glob_addr(C_255))));
    write_token(packet, (WORD)dw(self, 0xc), glob_addr(ITEM_LIST));
    dw(self, 0x10) = 0;
}
// 0x5ac950 (slot 25: packet): what slot 23 or 24 wrote; a value that differs sets its changed bit
static void THISCALL HudData_Read(BYTE *self, void *packet)
{
    BYTE bits = 0xcf;
    read_byte(packet, &bits);
    if (bits & 1) {
        BYTE b = 0xcf;
        read_byte(packet, &b);
        const long double v = fild(b) * cst(glob_addr(C_INV_255));
        if (!c3(v, fld(fl<float>(self, 8)))) {
            fl<float>(self, 8) = fstp(v);
            dw(self, 0x10) |= 1;
        }
    }
    if (bits & 2) {
        DWORD v = 0xcfcf;
        read_token(packet, &v, glob_addr(ITEM_LIST));
        v &= 0xffff;
        if (dw(self, 0xc) != v) {
            dw(self, 0xc) = v;
            dw(self, 0x10) |= 2;
        }
    }
}
// 0x5aca00 (slot 26): anything changed
static DWORD THISCALL HudData_Changed(BYTE *self) { return dw(self, 0x10) != 0; }
// 0x5aca10 (opacity)
static void THISCALL HudData_SetValue(BYTE *self, DWORD bits) { set_float(self, 8, bits, 0x10, 1); }
// 0x5aca30 (catalogue index)
static void THISCALL HudData_SetCount(BYTE *self, DWORD v)
{
    if (dw(self, 0xc) != v) {
        dw(self, 0xc) = v;
        dw(self, 0x10) |= 2;
    }
}

namespace {
void base_write_changed(BYTE *o, void *p) { fn<void(THISCALL *)(void *, void *)>(0x5ac8a0)(o, p); }
void base_write_all(BYTE *o, void *p) { fn<void(THISCALL *)(void *, void *)>(0x5ac900)(o, p); }
void base_read(BYTE *o, void *p) { fn<void(THISCALL *)(void *, void *)>(0x5ac950)(o, p); }
bool base_changed(BYTE *o) { return (BYTE)fn<DWORD(THISCALL *)(void *)>(0x5aca00)(o) != 0; }
}  // namespace

// ---- CCompassRenderData -----------------------------------------------------------------------------------

// 0x423520 (slot 23)
static void THISCALL CompassData_WriteChanged(BYTE *self, void *packet)
{
    base_write_changed(self, packet);
    if (!self[0x14]) {
        write_byte(packet, 0);
    } else {
        write_byte(packet, self[0x16] ? 3 : 1);
        if (self[0x16])
            write_word(packet, (WORD)dw(self, 0x18));
    }
    self[0x16] = 0;
    self[0x15] = 0;
}
// 0x423580 (slot 24)
static void THISCALL CompassData_WriteAll(BYTE *self, void *packet)
{
    base_write_all(self, packet);
    if (!self[0x14]) {
        write_byte(packet, 0);
    } else {
        write_byte(packet, 3);
        write_word(packet, (WORD)dw(self, 0x18));
    }
    self[0x16] = 0;
    self[0x15] = 0;
}
// 0x4235d0 (slot 25)
static void THISCALL CompassData_Read(BYTE *self, void *packet)
{
    base_read(self, packet);
    BYTE bits = 0xcf;
    read_byte(packet, &bits);
    const BYTE shown = bits & 1;
    if (self[0x14] != shown) {
        self[0x15] = 1;
        self[0x16] = shown;
        self[0x14] = shown;
    }
    if (self[0x14] && (bits & 2)) {
        DWORD w = 0x4fcf;
        read_word(packet, &w);
        const int frame = (short)(WORD)w;
        if (dw(self, 0x18) != (DWORD)frame && self[0x14]) {
            self[0x15] = 1;
            self[0x16] = 1;
        }
        dw(self, 0x18) = (DWORD)frame;
    }
}
// 0x423650 (slot 26)
static DWORD THISCALL CompassData_Changed(BYTE *self) { return base_changed(self) || self[0x15]; }

// ---- CEnergyHUDRenderData ---------------------------------------------------------------------------------

// 0x581710 (slot 23)
static void THISCALL EnergyData_WriteChanged(BYTE *self, void *packet)
{
    base_write_changed(self, packet);
    write_byte(packet, self[0x1c]);
    if (self[0x1c] & 1)
        write_dword(packet, dw(self, 0x14));
    if (self[0x1c] & 2)
        write_dword(packet, dw(self, 0x18));
    dw(self, 0x1c) = 0;
}
// 0x581760 (slot 24)
static void THISCALL EnergyData_WriteAll(BYTE *self, void *packet)
{
    base_write_all(self, packet);
    write_byte(packet, 3);
    write_dword(packet, dw(self, 0x14));
    write_dword(packet, dw(self, 0x18));
    dw(self, 0x1c) = 0;
}
// 0x5817a0 (slot 25)
static void THISCALL EnergyData_Read(BYTE *self, void *packet)
{
    base_read(self, packet);
    BYTE bits = 0xcf;
    read_byte(packet, &bits);
    if (bits & 1) {
        DWORD v = 0;
        read_dword(packet, &v);
        set_float(self, 0x14, v, 0x1c, 1);
    }
    if (bits & 2) {
        DWORD v = 0;
        read_dword(packet, &v);
        set_float(self, 0x18, v, 0x1c, 2);
    }
}
// 0x581840 (slot 26)
static DWORD THISCALL EnergyData_Changed(BYTE *self) { return base_changed(self) || dw(self, 0x1c); }
// 0x581860 (capacity)
static void THISCALL EnergyData_SetValue(BYTE *self, DWORD bits) { set_float(self, 0x14, bits, 0x1c, 1); }
// 0x581880 (charge)
static void THISCALL EnergyData_SetMaximum(BYTE *self, DWORD bits) { set_float(self, 0x18, bits, 0x1c, 2); }

// ---- CShieldHUDRenderData ---------------------------------------------------------------------------------

namespace {
BYTE *shield_element(BYTE *o, DWORD i) { return bp(dw(bp(dw(o, 0x1c)), i * 4)); }
bool element_changed(BYTE *e) { return (BYTE)vcall<DWORD>(e, 26) != 0; }
}  // namespace

// 0x56ec50 (count): the list grown with new CEnergyHUDRenderData (pool 0x20, slot 19, the array's slot 2)
// or shrunk from its end (slot 1 with 1, the pointer removed)
static void THISCALL ShieldData_SetCount(BYTE *self, DWORD n)
{
    if (n == dw(self, 0x20))
        return;
    if (n > dw(self, 0x20)) {
        do {
            BYTE *e = static_cast<BYTE *>(pool_alloc(0x20));
            if (e) {
                obj_ctor(e);
                vtable_and_defaults(e, vt(0x5ede94));
                vtable_and_defaults(e, vt(0x5ffd30));
            }
            vcall<DWORD>(e, 19);
            vcall<DWORD>(self + 0x18, 2, &e);
        } while (dw(self, 0x20) < n);
        return;
    }
    while (dw(self, 0x20) > n) {
        const DWORD i = dw(self, 0x20) - 1;
        if (BYTE *e = shield_element(self, i))
            vcall<DWORD>(e, 1, 1);
        const DWORD count = dw(self, 0x20);
        if (i < count) {
            if (i + 1 < count) {
                BYTE *data = bp(dw(self, 0x1c));
                fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(data + i * 4, data + (i + 1) * 4, (count - (i + 1)) * 4);
            }
            dw(self, 0x20)--;
        }
    }
}
// 0x56ea30 (slot 23)
static void THISCALL ShieldData_WriteChanged(BYTE *self, void *packet)
{
    base_write_changed(self, packet);
    DWORD bits = dw(self, 0x20) != dw(self, 0x14), bit = 2;
    for (DWORD i = 0; i < dw(self, 0x20); i++, bit <<= 1)
        if (i >= dw(self, 0x14) || element_changed(shield_element(self, i)))
            bits |= bit;
    write_dword(packet, bits);
    if (dw(self, 0x20) != dw(self, 0x14))
        write_byte(packet, dw(self, 0x20));
    for (DWORD i = 0; i < dw(self, 0x20); i++) {
        if (i >= dw(self, 0x14))
            vcall<DWORD>(shield_element(self, i), 24, packet);
        else if (element_changed(shield_element(self, i)))
            vcall<DWORD>(shield_element(self, i), 23, packet);
    }
    dw(self, 0x14) = dw(self, 0x20);
}
// 0x56eb00 (slot 24)
static void THISCALL ShieldData_WriteAll(BYTE *self, void *packet)
{
    base_write_all(self, packet);
    DWORD bits = 1, bit = 2;
    for (DWORD k = dw(self, 0x20); k; k--, bit <<= 1)
        bits |= bit;
    write_dword(packet, bits);
    write_byte(packet, dw(self, 0x20));
    for (DWORD i = 0; i < dw(self, 0x20); i++)
        vcall<DWORD>(shield_element(self, i), 24, packet);
    dw(self, 0x14) = dw(self, 0x20);
}
// 0x56eb70 (slot 25)
static void THISCALL ShieldData_Read(BYTE *self, void *packet)
{
    base_read(self, packet);
    DWORD bits = 0xcfcfcfcf;
    read_dword(packet, &bits);
    if (bits & 1) {
        BYTE n = 0xcf;
        read_byte(packet, &n);
        fn<void(THISCALL *)(void *, DWORD)>(0x56ec50)(self, n);
    }
    DWORD bit = 2;
    for (DWORD i = 0; i < dw(self, 0x20); i++, bit <<= 1)
        if (bits & bit)
            vcall<DWORD>(shield_element(self, i), 25, packet);
}
// 0x56ec00 (slot 26)
static DWORD THISCALL ShieldData_Changed(BYTE *self)
{
    bool changed = base_changed(self) || dw(self, 0x20) != dw(self, 0x14);
    for (DWORD i = 0; !changed && i < dw(self, 0x20); i++)
        changed = element_changed(shield_element(self, i));
    return changed;
}

// ---- CWeaponHUDRenderData ---------------------------------------------------------------------------------

namespace {
// The definition of +0xc as slots 23 and 24 resolve it: the object (any class) and the game item
// definition (null when it is none)
struct Def {
    BYTE *object, *zax;
};
Def weapon_definition_loading(BYTE *self)
{
    BYTE *o = catalogue_object_loading(dw(self, 0xc));
    return {o, zax_definition(o)};
}
}  // namespace

// 0x4851e0 (slot 23)
static void THISCALL WeaponData_WriteChanged(BYTE *self, void *packet)
{
    base_write_changed(self, packet);
    write_byte(packet, self[0x24]);
    const Def d = weapon_definition_loading(self);
    if ((self[0x24] & 1) && has_ammunition(d.object)) {
        if (d.zax && continuous_fire(d.zax))
            write_dword(packet, dw(self, 0x18));
        else
            write_word(packet, (WORD)dw(self, 0x14));
    }
    if ((self[0x24] & 2) && !has_ammunition(d.object))
        write_dword(packet, dw(self, 0x1c));
    if ((self[0x24] & 4) && !has_ammunition(d.object))
        write_dword(packet, dw(self, 0x20));
    dw(self, 0x24) = 0;
}
// 0x4853a0 (slot 24)
static void THISCALL WeaponData_WriteAll(BYTE *self, void *packet)
{
    base_write_all(self, packet);
    write_byte(packet, 7);
    const Def d = weapon_definition_loading(self);
    if (has_ammunition(d.object)) {
        if (d.zax && continuous_fire(d.zax))
            write_dword(packet, dw(self, 0x18));
        else
            write_word(packet, (WORD)dw(self, 0x14));
    } else {
        write_dword(packet, dw(self, 0x1c));
        write_dword(packet, dw(self, 0x20));
    }
    dw(self, 0x24) = 0;
}
// 0x485550 (slot 25)
static void THISCALL WeaponData_Read(BYTE *self, void *packet)
{
    base_read(self, packet);
    BYTE bits = 0xcf;
    read_byte(packet, &bits);
    BYTE *def = catalogue_object_record(dw(self, 0xc));
    BYTE *zax = zax_definition(def);
    if ((bits & 1) && has_ammunition(def)) {
        if (zax && continuous_fire(zax)) {
            DWORD v = 0;
            read_dword(packet, &v);
            set_float(self, 0x18, v, 0x24, 1);
        } else {
            DWORD v = 0xcfcf;
            read_word(packet, &v);
            v &= 0xffff;
            if (dw(self, 0x14) != v) {
                dw(self, 0x14) = v;
                dw(self, 0x24) |= 1;
            }
        }
    }
    if ((bits & 2) && !has_ammunition(def)) {
        DWORD v = 0;
        read_dword(packet, &v);
        set_float(self, 0x1c, v, 0x24, 2);
    }
    if ((bits & 4) && !has_ammunition(def)) {
        DWORD v = 0;
        read_dword(packet, &v);
        set_float(self, 0x20, v, 0x24, 4);
    }
}
// 0x485700 (slot 26)
static DWORD THISCALL WeaponData_Changed(BYTE *self) { return base_changed(self) || dw(self, 0x24); }

ZAX_HOOKS(hud_data)
{
    const Hook list[] = {
        {0x5ac8a0, ptr(&HudData_WriteChanged), "CInventoryHUDRenderData::WriteChanged", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x5ac900, ptr(&HudData_WriteAll), "CInventoryHUDRenderData::WriteAll", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x5ac950, ptr(&HudData_Read), "CInventoryHUDRenderData::Read", 7, {0x51, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b}},
        {0x5aca00, ptr(&HudData_Changed), "CInventoryHUDRenderData::Changed", 5, {0x8b, 0x51, 0x10, 0x33, 0xc0, 0x85, 0xd2, 0x0f}},
        {0x5aca10, ptr(&HudData_SetValue), "CInventoryHUDRenderData::SetValue", 7, {0xd9, 0x41, 0x08, 0xd8, 0x5c, 0x24, 0x04, 0xdf}},
        {0x5aca30, ptr(&HudData_SetCount), "CInventoryHUDRenderData::SetCount", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x0c, 0x3b}},
        {0x423520, ptr(&CompassData_WriteChanged), "CCompassRenderData::WriteChanged", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b}},
        {0x423580, ptr(&CompassData_WriteAll), "CCompassRenderData::WriteAll", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b}},
        {0x4235d0, ptr(&CompassData_Read), "CCompassRenderData::Read", 7, {0x51, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b}},
        {0x423650, ptr(&CompassData_Changed), "CCompassRenderData::Changed", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xa8, 0x93, 0x18, 0x00}, 4},
        {0x581710, ptr(&EnergyData_WriteChanged), "CEnergyHUDRenderData::WriteChanged", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x581760, ptr(&EnergyData_WriteAll), "CEnergyHUDRenderData::WriteAll", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x5817a0, ptr(&EnergyData_Read), "CEnergyHUDRenderData::Read", 7, {0x51, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b}},
        {0x581840, ptr(&EnergyData_Changed), "CEnergyHUDRenderData::Changed", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xb8, 0xb1, 0x02, 0x00}, 4},
        {0x581860, ptr(&EnergyData_SetValue), "CEnergyHUDRenderData::SetValue", 7, {0xd9, 0x41, 0x14, 0xd8, 0x5c, 0x24, 0x04, 0xdf}},
        {0x581880, ptr(&EnergyData_SetMaximum), "CEnergyHUDRenderData::SetMaximum", 7, {0xd9, 0x41, 0x18, 0xd8, 0x5c, 0x24, 0x04, 0xdf}},
        {0x56ec50, ptr(&ShieldData_SetCount), "CShieldHUDRenderData::SetShieldCount", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x56, 0x57}},
        {0x56ea30, ptr(&ShieldData_WriteChanged), "CShieldHUDRenderData::WriteChanged", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x56, 0x57}},
        {0x56eb00, ptr(&ShieldData_WriteAll), "CShieldHUDRenderData::WriteAll", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x57, 0x8b}},
        {0x56eb70, ptr(&ShieldData_Read), "CShieldHUDRenderData::Read", 6, {0x51, 0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x55, 0x56}},
        {0x56ec00, ptr(&ShieldData_Changed), "CShieldHUDRenderData::Changed", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0xe8, 0xf6, 0xdd}},
        {0x4851e0, ptr(&WeaponData_WriteChanged), "CWeaponHUDRenderData::WriteChanged", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x53, 0x8b}},
        {0x4853a0, ptr(&WeaponData_WriteAll), "CWeaponHUDRenderData::WriteAll", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x53, 0x55}},
        {0x485550, ptr(&WeaponData_Read), "CWeaponHUDRenderData::Read", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x485700, ptr(&WeaponData_Changed), "CWeaponHUDRenderData::Changed", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xf8, 0x72, 0x12, 0x00}, 4},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
