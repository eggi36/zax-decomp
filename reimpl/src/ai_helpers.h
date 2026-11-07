// Building blocks of the AI modules of 0x568000-0x577fff (ai_simple.cpp, ai_shield.cpp, ai_skeleton.cpp)
// on top of ai_common.h: more descriptor constructors and setters, the 12-byte message object the AIs
// send, the definition list lookup, the level-by-level destructors, and exe functions these modules
// call (always through their exe addresses, so Disable= and the checks' stand-ins apply).
#pragma once
#include "class_info.h"
#include "ai_common.h"

namespace zax::ai {

// More descriptor constructors (thiscall; the arguments as the exe pushes them)
constexpr DWORD OBJECT_ARRAY_FIELD_CTOR2 = 0x419230;    // 0x48: an array of owned objects
constexpr DWORD ENUM_INT_FIELD_CTOR_UNITS = 0x54d7f0;   // 0x44
constexpr DWORD LIST_FIELD_CTOR2 = 0x417a20;            // 0x44: a heap array of embedded objects
constexpr DWORD TEXT_EMPTY = 0x62e558;                  // ""
inline DWORD set_alt_name(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419030)(f, at<const char *>(text)); }
inline DWORD set_units(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419060)(f, at<const char *>(text)); }
inline DWORD set_name(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x51f770)(f, at<const char *>(text)); }
inline DWORD ptr_value(const void *p) { return (DWORD)(uintptr_t)p; }

// The message object the AIs and entities pass around (12 bytes on the stack: vtable 0x5eeb64, the
// message id, a byte "handled")
struct Message {
    DWORD vtable, id;
    BYTE handled;
};
inline Message message(DWORD id_at) { return {vt(0x5eeb64), *at<DWORD *>(id_at), 0}; }
constexpr DWORD MSG_AI_DONE = 0x6c29d8;       // an AI has finished (CSeriesAI advances, CActionAI reports)
constexpr DWORD MSG_DEACTIVATE_AI = 0x6c29e8; // g_MsgLayerPartDeactivateAI
// Slots: an entity's 63 (message; al: handled), an AI's 29 (entity, message; al: handled)
inline DWORD send_to_part(void *part, Message *m) { return vcall<DWORD>(part, 63, m); }
inline DWORD send_to_ai(void *ai, void *e, Message *m) { return vcall<DWORD>(ai, 29, e, m); }
// CLayerPartAI's slot 29 0x4fd100 (the base: a message asking for an AI of a class gets this one)
inline DWORD layer_part_ai_message(void *ai, void *e, void *m) { return fn<DWORD(THISCALL *)(void *, void *, void *)>(0x4fd100)(ai, e, m); }
// 0x4fd0b0 (on an AI: entity, view): the debug drawing of the AI (through user-defined class refs)
inline DWORD ai_debug_draw(void *ai, void *e, void *view) { return fn<DWORD(THISCALL *)(void *, void *, void *)>(0x4fd0b0)(ai, e, view); }

// The inline lookup of a definition list's record object as 0x48d8f0 does it (records, count, folder;
// the list's slots 1 (the name of entry i) and 37 (load an object))
inline BYTE *list_object(DWORD list, DWORD records_at, DWORD count_at, DWORD folder, DWORD i)
{
    BYTE *records = *at<BYTE **>(records_at);
    if (i >= *at<DWORD *>(count_at))
        i = 0;
    const DWORD off = i << 7;
    if (records[off + 0x7f])
        return nullptr;
    if (!dw(records + off, 0x78) && records && !records[off + 0x7f]) {
        records[off + 0x7f] = 1;
        char path[0x100];
        fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, at<const char *>(folder), vcall<const char *>(at<void *>(list), 1, i));
        char *name = const_cast<char *>(path_name(path));
        DWORD o;
        if (name == path) {
            o = vcall<DWORD>(at<void *>(list), 37, glob<const char *>(TEXT_EMPTY), name, *at<BYTE **>(records_at) + off + 0x78);
        } else {
            name[-1] = 0;
            o = vcall<DWORD>(at<void *>(list), 37, path, name, *at<BYTE **>(records_at) + off + 0x78);
        }
        dw(*at<BYTE **>(records_at) + off, 0x78) = o;
        (*at<BYTE **>(records_at))[off + 0x7f] = 0;
        records = *at<BYTE **>(records_at);
    }
    return bp(dw(records + off, 0x78));
}

// An embedded pointer array whose data comes from the pool ({vtable, data, count, capacity}) destroyed:
// the base vtable, the data back to the pool (capacity · 4 bytes), the fields 0
inline void pool_array_dtor(BYTE *a, DWORD base_vtable)
{
    dw(a, 0) = base_vtable;
    if (dw(a, 4)) {
        pool_get_free(bp(dw(a, 4)), dw(a, 0xc) << 2);
        dw(a, 4) = 0;
        dw(a, 0xc) = 0;
    }
    dw(a, 8) = 0;
}

// The CLayerPartAI levels every AI destructor ends with: CLayerPartAI's vtable and fields, CAIBase
inline void layer_part_ai_dtor(BYTE *o)
{
    vtable_and_free(o, vt(0x5ed1d0));
    ai_base_dtor(o);
}
// The CFlyweightLayerPartAI level (vtable 0x5ef1a0) and the CLayerPartAI levels
inline void flyweight_ai_dtor(BYTE *o)
{
    vtable_and_free(o, vt(0x5ef1a0));
    layer_part_ai_dtor(o);
}

// CAction (Actions, reimplemented in action_classes.cpp): constructor 0x41a6b0, destructor 0x41a700
inline void action_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x41a6b0)(o); }
inline void action_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x41a700)(o); }
// CInventoryItem (inventory_items.cpp): constructor 0x42a2b0, destructor 0x42a300
inline void item_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x42a2b0)(o); }
inline DWORD item_dtor(BYTE *o) { return fn<DWORD(THISCALL *)(void *)>(0x42a300)(o); }

// Class getters of other classes
inline CClassInfo *entity_base_class() { return fn<Getter>(0x487620)(); }
inline CClassInfo *entity_destructable_class() { return fn<Getter>(0x48bdf0)(); }
inline CClassInfo *entity_character_class() { return fn<Getter>(0x49a930)(); }
inline CClassInfo *entity_flying_class() { return fn<Getter>(0x49bde0)(); }
inline CClassInfo *layer_part_class() { return fn<Getter>(0x4f9bf0)(); }
inline CClassInfo *inventory_item_class() { return fn<Getter>(0x429e50)(); }
inline CClassInfo *energy_item_class() { return fn<Getter>(0x5afbd0)(); }
inline CClassInfo *zax_item_definition_class() { return fn<Getter>(0x5b5f10)(); }
inline CClassInfo *flyweight_ai_class() { return fn<Getter>(0x4fd390)(); }

// Layer part helpers (layer_part_helpers.cpp, broadcast.cpp, entities.cpp)
inline BYTE *part_inventory(void *part) { return fn<BYTE *(THISCALL *)(void *)>(0x4267e0)(part); }
inline void add_listener(void *part, void *listener, int offset, DWORD name) { fn<DWORD(THISCALL *)(void *, void *, int, const char *)>(0x4fc1b0)(part, listener, offset, at<const char *>(name)); }
inline void remove_listener(void *part, void *listener, int offset, DWORD name) { fn<DWORD(THISCALL *)(void *, void *, int, const char *)>(0x4fc200)(part, listener, offset, at<const char *>(name)); }
inline DWORD part_died(void *part) { return fn<DWORD(THISCALL *)(void *)>(0x4fbf30)(part); }
inline DWORD part_layer_add(void *part, DWORD what) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4fc8a0)(part, what); }
inline DWORD part_layer_add_named(void *part, DWORD name) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x4fc8c0)(part, at<const char *>(name)); }
inline BYTE *entity_model(void *e) { return fn<BYTE *(THISCALL *)(void *)>(0x48b290)(e); }
inline long double hit_points(void *e) { return fn<long double(THISCALL *)(void *)>(0x48d150)(e); }
// The item category list 0x6c0800 (Find: the index of a name) and the damage type list 0x6bd760
inline DWORD category_index(DWORD name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(0x6c0800), at<const char *>(name), -1); }
inline DWORD damage_type_index(DWORD name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(0x6bd760), at<const char *>(name), -1); }
// CInventory (inventory.cpp, inventory_items.cpp)
inline DWORD inventory_count_of(BYTE *inv, DWORD category) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4251c0)(inv, category); }
inline DWORD inventory_first_of(BYTE *inv, DWORD category) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x425340)(inv, category); }
inline DWORD inventory_next_of(BYTE *inv, DWORD i, DWORD category) { return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x425470)(inv, i, category); }
inline BYTE *inventory_item(BYTE *inv, DWORD i) { return vcall<BYTE *>(inv, 26, i); }
// 0x4dd480: an item's definition (ItemRef_Definition)
inline BYTE *item_definition(void *item) { return fn<BYTE *(THISCALL *)(void *)>(0x4dd480)(item); }

constexpr DWORD CATEGORY_PLAYER = 0x6c29a0, CATEGORY_ENEMY = 0x6c29a8;
constexpr DWORD TEXT_SHIELD = 0x61968c, TEXT_DIED = 0x610178, TEXT_DELETE = 0x60aef8;

}  // namespace zax::ai
