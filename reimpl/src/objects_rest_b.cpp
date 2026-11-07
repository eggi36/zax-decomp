// The last AI and Objects functions of 0x480000-0x49ffff (phase 3 remains, band B; docs/DECOMPILATION.md
// section 8): CDoorAI 0x481710-0x4828a8, the entity block readers' create functions 0x487080, 0x4872d0,
// 0x48dac0, 0x48db30, 0x48dbb0, the deleting destructor of the owning pointer array 0x4874a0 (vtable
// 0x5f1de8, the global 0x6bdd08), the overlay walking reader's registration 0x49d390 and CEntityBase's
// rectangle slot 37 0x48a100. CDoorAI's Create and CreateArray are in ai_classes.cpp.
//
// CDoorAI (0x48, vtable 0x5f1740, class 0x6bdc6c, version 2, parent CLayerPartAI) [confirmed: RegisterFields,
// the slots; checked]: +0x10 "Is Locked", +0x14 the state ("Debug/State": 0 closing, 1 closed, 2 opening,
// 3 open), +0x18 "Debug/Frame Andvance" (the frames per second of the open/close sequence, negative when
// it runs backwards), +0x1c "Debug/Next Pain" (seconds to the next closing damage, -1 none), +0x20 "Allow
// Open or Close?", +0x24 "Before Open", +0x28 "After Opened", +0x2c "Before Closed", +0x30 "After Closed",
// +0x34 "After Locked", +0x38 "After Unlocked" (actions run with the door's entity as the caller),
// +0x3c "Auto-Close Timeout", +0x40 "Debug/Timeout Time Left", +0x44/+0x45 "Triggered By/Triggered By
// Players"/"Enemies".
//  - Slot 24 (Begin: entity): an animated entity's sequence "Open" makes the state open, "Closed" closed.
//  - Open/Close (0x4825b0/0x482710: entity; al: started): only for an animated, unlocked door whose "Allow
//    Open or Close?" (when set) says yes, from closed/closing (open) or open/opening (close). The "Before"
//    action, the model's activate/close sound (+0xa4/+0xa8 through CLayerPart_LayerAdd), the flags (open:
//    0x200000 cleared; close: 0x40000 set, 0x200000 cleared), the "Opening"/"Closing" sequence (or the
//    other one backwards, the frame advance negative) at the fraction where the running one stands, the
//    frame advance from the sequence object's slot 26. Close arms the closing damage (Next Pain 0 when the
//    model's "Damage/Closing Damage" is above 0) and stops the auto-close timer; Open on an open door
//    restarts it.
//  - Slot 25 (Update: entity, seconds): while the frame advances, the frame moves on (with the sequence's
//    triggers) until it reaches the frame count or falls below 0; then the door is open (state 3, "Open",
//    flags 0x40000 cleared and 0x200000 set, "After Opened", the auto-close timer started) or closed (state
//    1, "Closed", 0x200000 set, "After Closed"). A closing or closed door whose Next Pain has run out (every
//    0.2 s) damages every living destructable "$Touching" part by Closing Damage * 0.2 of type "Unavoidable"
//    (slot 99) and turns the animated ones by a random angle in [-pi/4, pi/4] (slot 104); a closed door that
//    hurt nobody stops the damage. An open door with a timeout counts it down and closes at 0.
//  - Slot 29 (message: entity, message): the messages 0x6bdc60 (toggle: open when closed or closing, close
//    when open or opening), 0x6bdc64 (open), 0x6bdc3c (close) put the result into the message's +8; 0x6bdc38
//    locks and 0x6bdc40 unlocks (with their actions) when the state changes. al: handled.
//
// The block readers' create functions (slot 2 of the reader vtables 0x5f1c40, 0x5f1ddc, 0x5f1fec…;
// multiplayer, packet) [confirmed: code; checked]: a new CEntityBase (0x487080 the complete block, 0x4872d0
// the short one: the global 0x6bdd40 0 or 1 while reading) or CEntityAnimated (0x48dac0, 0x48db30, 0x48dbb0:
// 0x6bdd94 0, 1, 2) reads its block through slot 27 and becomes the light beam's last-made part
// (0x4fd5c0). eax: slot 27's.
//
// CEntityBase's slot 37 (rectangle: out, kind, offset or null; al: found one) [confirmed: code; checked]: the
// layer grid lists a part in every cell of kind 5. Kinds 0-2: the frames of the model's sequences in the
// part's rotation (kind 1 only sequences with flag 0x40, kind 2 only those without; the "Also Render" models
// after it) offset by the part's rounded position and rendering height (sequences with flag 0x20 shift x by
// height * 0.7015 and y by height * 0.5, those without 0x40 y by the height), united, then the children
// (CLayerPart_ChildrenRect); kind 3: the model's bounds polygon at the position; kind 4: the pixel at the
// position plus the model's +0x98/+0x9c; kind 5: the sequences' rectangles (+0xc..+0x18) and the bounds
// polygon of the model and its "Also Render" models, with the children, cached in +0x5c..+0x68 for a part
// without parent (and taken from there when the cache is not empty). Where the original reads through a null
// pointer (a model without the sequence of an index in kind 5, DefaultSequenceIndex of a missing "Also
// Render" model) the reimplementation does the same.
//
// Return values: the original's eax where callers can see it (the getters, RegisterFields, the destructors,
// the creates, slot 24's last _stricmp); al where it only sets al (Open/Close, slot 29, slot 37).
#include "class_info.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Functions of the exe ----------------------------------------------------------------------------------

CClassInfo *entity_destructable_class() { return fn<Getter>(0x48bdf0)(); }
const char *sequence_name(void *e) { return fn<const char *(THISCALL *)(void *)>(0x48aee0)(e); }
BYTE *sequence_object(void *e) { return fn<BYTE *(THISCALL *)(void *)>(0x48b590)(e); }
BYTE *entity_model(void *e) { return fn<BYTE *(THISCALL *)(void *)>(0x48b290)(e); }
void set_frame(void *e, DWORD frame) { fn<void(THISCALL *)(void *, DWORD)>(0x48feb0)(e, frame); }
void set_frame_fraction(void *e, DWORD f) { fn<void(THISCALL *)(void *, DWORD)>(0x48ffc0)(e, f); }
long double frame_part(void *e) { return fn<long double(THISCALL *)(void *)>(0x48f600)(e); }
DWORD play_sequence(void *e, GlobKey name) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x48f700)(e, glob<const char *>(name)); }
void layer_add(void *part, DWORD what) { fn<DWORD(THISCALL *)(void *, DWORD)>(0x4fc8a0)(part, what); }
// An action field of the door run with the door's entity as the caller (0x41a720)
DWORD run_action(BYTE *self, DWORD field, BYTE *part)
{
    return action_execute(bp(dw(self, field)), bp(dw(part, 0x48)), part, self, part, self, self + field);
}

// ---- CDoorAI -------------------------------------------------------------------------------------------------

constexpr DWORD C_PAIN_SCALE = 0x5f17e8;   // 0.2
enum : DWORD { CLOSING = 0, CLOSED = 1, OPENING = 2, OPEN = 3 };

// 0x481710, 0x4818c0 (slot 2)
CClassInfo *__cdecl DoorAI_ClassInfo()
{
    return class_getter(0x6bdc6c, 0x611ff4 /* "CDoorAI" */, 2.0f, layer_part_ai_class, 0x48, 0, 0x481760, 0x4817b0, 0x481920);
}
// 0x481920. eax: the last call's.
DWORD __cdecl DoorAI_RegisterFields()
{
    void *f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x6120c4) /* "Is Locked" */, 0x10, 1, 0x10, 1, 0, 0);
    add_field(DoorAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x6120ac) /* "Allow Open or Close?" */, 0x20, 4, 0x20, action_class(), 1, 0, 0, 0);
    add_field(DoorAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x6120a0) /* "Before Open" */, 0x24, 4, 0x24, action_class(), 1, 0, 0, 0);
    add_field(DoorAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x612090) /* "After Opened" */, 0x28, 4, 0x28, action_class(), 1, 0, 0, 0);
    add_field(DoorAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x612080) /* "Before Closed" */, 0x2c, 4, 0x2c, action_class(), 1, 0, 0, 0);
    add_field(DoorAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x612070) /* "After Closed" */, 0x30, 4, 0x30, action_class(), 1, 0, 0, 0);
    add_field(DoorAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x612060) /* "After Locked" */, 0x34, 4, 0x34, action_class(), 1, 0, 0, 0);
    add_field(DoorAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x612050) /* "After Unlocked" */, 0x38, 4, 0x38, action_class(), 1, 0, 0, 0);
    add_field(DoorAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61203c) /* "Auto-Close Timeout" */, 0x3c, 4, 0x3c, 1, 0, 0, 0, 0, 0);
    add_field(DoorAI_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x60f948) /* "Debug/State" */, 0x14, 4, 0x14, glob_addr(0x6bdc48), 3, glob_addr(0x611f90) /* "Unknown" */, 0, 0);
    add_field(DoorAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x612024) /* "Debug/Timeout Time Left" */, 0x40, 4, 0x40, 3, 0, 0, 0, 0, 0);
    add_field(DoorAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61200c) /* "Debug/Frame Andvance" */, 0x18, 4, 0x18, 3, 0, 0, 0, 0, 0);
    add_field(DoorAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x611ffc) /* "Debug/Next Pain" */, 0x1c, 4, 0x1c, 3, 0, 0, 0, 0, 0);
    add_field(DoorAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x60b4a4) /* "Triggered By/Triggered By Players" */, 0x44, 1, 0x44, 1, 1, 0);
    add_field(DoorAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x60b480) /* "Triggered By/Triggered By Enemies" */, 0x45, 1, 0x45, 1, 1, 0);
    return add_field(DoorAI_ClassInfo(), f);
}
void DoorAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f1740));
    vtable_and_free(o, vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
}
// 0x481820 (slot 1)
void *THISCALL DoorAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x48, DoorAI_destroy>(self, flags); }

// 0x482110 (slot 24: entity)
DWORD THISCALL DoorAI_Begin(BYTE *self, BYTE *part)
{
    const DWORD kind = fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(part, entity_animated_class());   // Object_IsKindOf
    if (!(kind & 0xff))
        return kind;
    const char *name = sequence_name(part);
    if (!crt_stricmp(name, glob<const char *>(0x611f98) /* "Open" */))
        dw(self, 0x14) = OPEN;
    const int r = crt_stricmp(name, glob<const char *>(0x611fa8) /* "Closed" */);
    if (!r)
        dw(self, 0x14) = CLOSED;
    return (DWORD)r;
}

// The start of the open or close sequence (shared by Open and Close): `first` plays forwards, `other`
// backwards; frac is the fraction of the running sequence (1 when none runs)
DWORD start_sequence(BYTE *self, BYTE *part, GlobKey first, GlobKey other, float frac)
{
    float dir = 1.0f;
    if (!(play_sequence(part, first) & 0xff)) {
        if (!(play_sequence(part, other) & 0xff))
            return 0;
        frac = fstp(cst(glob_addr(C_ONE)) - fld(frac));
        dir = -1.0f;
    }
    BYTE *seq = sequence_object(part);
    fl<float>(self, 0x18) = fstp(vcall<long double>(seq, 26) * fld(dir));
    set_frame_fraction(part, float_bits(fstp(cst(glob_addr(C_ONE)) - fld(frac))));
    return 1;
}
// The fraction where the running sequence stands (from the end when it runs backwards)
float running_fraction(BYTE *self, BYTE *part)
{
    float frac = fstp(frame_part(part));
    if (c0(fld(fl<float>(self, 0x18)), cst(glob_addr(C_ZERO))))
        frac = fstp(cst(glob_addr(C_ONE)) - fld(frac));
    return frac;
}
bool allowed(BYTE *self, BYTE *part)
{
    if (!is_kind_of(part, entity_animated_class()) || self[0x10])
        return false;
    return !dw(self, 0x20) || (run_action(self, 0x20, part) & 0xff);
}

// 0x4825b0 (entity): open (see above)
DWORD THISCALL DoorAI_Open(BYTE *self, BYTE *part)
{
    if (!allowed(self, part))
        return 0;
    const DWORD state = dw(self, 0x14);
    if (state == CLOSED || state == CLOSING) {
        if (dw(self, 0x24))
            run_action(self, 0x24, part);
        layer_add(part, dw(entity_model(part), 0xa4));
        slot_clear_flags(part, 0x200000);
        float frac = 1.0f;
        if (state == CLOSING)
            frac = running_fraction(self, part);
        if (!start_sequence(self, part, 0x611fa0 /* "Opening" */, 0x611fb0 /* "Closing" */, frac))
            return 0;
        dw(self, 0x14) = OPENING;
        return 1;
    }
    if (state == OPEN && !c03(fld(fl<float>(self, 0x3c)), cst(glob_addr(C_ZERO))))
        dw(self, 0x40) = dw(self, 0x3c);
    return 0;
}
// 0x482710 (entity): close (see above)
DWORD THISCALL DoorAI_Close(BYTE *self, BYTE *part)
{
    if (!allowed(self, part))
        return 0;
    const DWORD state = dw(self, 0x14);
    if (state != OPEN && state != OPENING)
        return 0;
    if (dw(self, 0x2c))
        run_action(self, 0x2c, part);
    layer_add(part, dw(entity_model(part), 0xa8));
    slot_set_flags(part, 0x40000);
    if (!c03(fld(fl<float>(entity_model(part), 0x2c)), cst(glob_addr(C_ZERO))))
        dw(self, 0x1c) = 0;
    else
        dw(self, 0x1c) = 0xbf800000;   // -1
    slot_clear_flags(part, 0x200000);
    float frac = 1.0f;
    if (state == OPENING)
        frac = running_fraction(self, part);
    if (!start_sequence(self, part, 0x611fb0 /* "Closing" */, 0x611fa0 /* "Opening" */, frac))
        return 0;
    dw(self, 0x14) = CLOSING;
    if (!c03(fld(fl<float>(self, 0x3c)), cst(glob_addr(C_ZERO))))
        dw(self, 0x40) = 0;
    return 1;
}
DWORD door_open(BYTE *self, BYTE *part) { return fn<DWORD(THISCALL *)(void *, void *)>(0x4825b0)(self, part); }
DWORD door_close(BYTE *self, BYTE *part) { return fn<DWORD(THISCALL *)(void *, void *)>(0x482710)(self, part); }

// The closing damage to the touching parts (al: someone was hurt)
bool closing_damage(BYTE *part, float damage)
{
    bool hurt = false;
    void *list = nullptr;
    fn<void *(THISCALL *)(void **)>(0x59a470)(&list);   // PartList_GetTemp
    fn<void(__fastcall *)(void *, const char *, void *, void *, void *)>(0x41a760)(bp(dw(part, 0x48)), glob<const char *>(0x6120d0) /* "$Touching" */,
                                                                                    part, part, list);
    for (DWORD i = 0; i < dw(list, 8); i++) {
        BYTE *o = reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(list, 4)))[i];
        if (!is_kind_of(o, entity_destructable_class()))
            continue;
        if (c03(fn<long double(THISCALL *)(void *)>(0x48d030)(o), cst(glob_addr(C_ZERO))))   // the hit points left
            continue;
        const DWORD type = fn<DWORD(THISCALL *)(void *, const char *, int)>(0x591fc0)(glob<void *>(0x6bd760), glob<const char *>(0x610324) /* "Unavoidable" */, -1);
        vcall<DWORD>(o, 99, float_bits(damage), type, 0);
        hurt = true;
        if (o && is_kind_of(o, entity_animated_class())) {
            const float angle = fl<float>(o, 0xc8);
            const float turned = fstp(random_range(GAME_RANDOM, 0xbec90fdb, 0x3ec90fdb) + fld(angle));
            vcall<DWORD>(o, 104, float_bits(turned), 1);
        }
    }
    fn<void *(THISCALL *)(void **)>(0x59a510)(&list);   // PartList_ReleaseTemp
    return hurt;
}

// 0x482170 (slot 25: entity, seconds): see above
void THISCALL DoorAI_Update(BYTE *self, BYTE *part, DWORD seconds)
{
    if (!is_kind_of(part, entity_animated_class()))
        return;
    if (!c3(fld(fl<float>(self, 0x18)), cst(glob_addr(C_ZERO)))) {
        const DWORD count = dw(sequence_object(part), 0x48);
        const long double cur = vcall<long double>(part, 84);
        const float cur_f = fstp(cur);
        const float next = fstp(fld(raw_float(seconds)) * fld(fl<float>(self, 0x18)) + cur);
        if (!c0(fld(next), cst(glob_addr(C_ZERO))) && !c03(fild64((long long)count), fld(next))) {
            set_frame(part, float_bits(next));
            fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, void *)>(0x560600)(sequence_object(part), dw(self, 0x18), float_bits(cur_f),
                                                                              float_bits(next), part);   // CRotationSequence::PlayTriggers
        } else if (dw(self, 0x14) == OPENING) {
            dw(self, 0x14) = OPEN;
            play_sequence(part, 0x611f98 /* "Open" */);
            set_frame(part, 0);
            slot_clear_flags(part, 0x40000);
            dw(self, 0x18) = 0;
            slot_set_flags(part, 0x200000);
            if (dw(self, 0x28))
                run_action(self, 0x28, part);
            if (!c03(fld(fl<float>(self, 0x3c)), cst(glob_addr(C_ZERO))))
                dw(self, 0x40) = dw(self, 0x3c);
        } else if (dw(self, 0x14) == CLOSING) {
            dw(self, 0x14) = CLOSED;
            play_sequence(part, 0x611fa8 /* "Closed" */);
            set_frame(part, 0);
            slot_set_flags(part, 0x200000);
            dw(self, 0x18) = 0;
            if (dw(self, 0x30))
                run_action(self, 0x30, part);
        }
    }
    const DWORD state = dw(self, 0x14);
    if (state == CLOSING || state == CLOSED) {
        if (c0(fld(fl<float>(self, 0x1c)), cst(glob_addr(C_ZERO))))
            return;
        const long double left = fld(fl<float>(self, 0x1c)) - fld(raw_float(seconds));
        fl<float>(self, 0x1c) = fstp(left);
        if (!c03(left, cst(glob_addr(C_ZERO))))
            return;
        dw(self, 0x1c) = 0x3e4ccccd;   // 0.2
        const long double d = fld(fl<float>(entity_model(part), 0x2c)) * cst(glob_addr(C_PAIN_SCALE));
        const float damage = fstp(d);
        if (!c03(d, cst(glob_addr(C_ZERO))) && closing_damage(part, damage))
            return;
        if (dw(self, 0x14) == CLOSED)
            dw(self, 0x1c) = 0xbf800000;   // -1
        return;
    }
    if (state == OPEN && !c03(fld(fl<float>(self, 0x3c)), cst(glob_addr(C_ZERO)))) {
        const long double left = fld(fl<float>(self, 0x40)) - fld(raw_float(seconds));
        fl<float>(self, 0x40) = fstp(left);
        if (c0(left, cst(glob_addr(C_ZERO)))) {
            dw(self, 0x40) = 0;
            door_close(self, part);
        }
    }
}

// 0x4824a0 (slot 29: entity, message): see above
DWORD THISCALL DoorAI_Message(BYTE *self, BYTE *part, BYTE *msg)
{
    const DWORD id = dw(msg, 4);
    if (id == *glob<DWORD *>(0x6bdc60)) {   // toggle
        const DWORD state = dw(self, 0x14);
        if (state == CLOSED || state == CLOSING)
            msg[8] = (BYTE)door_open(self, part);
        else if (state == OPEN || state == OPENING)
            msg[8] = (BYTE)door_close(self, part);
        else
            msg[8] = 0;
        return 1;
    }
    if (id == *glob<DWORD *>(0x6bdc64)) {
        msg[8] = (BYTE)door_open(self, part);
        return 1;
    }
    if (id == *glob<DWORD *>(0x6bdc3c)) {
        msg[8] = (BYTE)door_close(self, part);
        return 1;
    }
    if (id == *glob<DWORD *>(0x6bdc38) && !self[0x10]) {   // lock
        self[0x10] = 1;
        if (dw(self, 0x34))
            run_action(self, 0x34, part);
        return 1;
    }
    if (id == *glob<DWORD *>(0x6bdc40) && self[0x10]) {   // unlock
        self[0x10] = 0;
        if (dw(self, 0x38))
            run_action(self, 0x38, part);
        return 1;
    }
    return 0;
}

// ---- The entity block readers ---------------------------------------------------------------------------------

constexpr DWORD G_BASE_SHORT = 0x6bdd40, G_ANIMATED_KIND = 0x6bdd94;
void light_beam_set_last(void *part) { fn<void(THISCALL *)(void *)>(0x4fd5c0)(part); }

template <DWORD SIZE, DWORD CTOR, DWORD GLOBAL, DWORD VALUE, bool CLEAR> DWORD THISCALL read_entity(void *, void *packet)
{
    void *e = pool_alloc(SIZE);
    if (e)
        e = fn<void *(THISCALL *)(void *)>(CTOR)(e);
    *glob<DWORD *>(GLOBAL) = VALUE;
    const DWORD r = vcall<DWORD>(e, 27, packet);
    if (CLEAR)
        *glob<DWORD *>(GLOBAL) = 0;
    light_beam_set_last(e);
    return r;
}
// 0x487080, 0x4872d0: CEntityBase (constructor 0x487ff0, inlined in the original), the readers "Compleate"
// 0x6bdcf8 and "SimpleStationary" 0x6bdd20
DWORD THISCALL EntityBase_ReadComplete(void *r, void *p) { return read_entity<0x7c, 0x487ff0, G_BASE_SHORT, 0, false>(r, p); }
DWORD THISCALL EntityBase_ReadSimpleStationary(void *r, void *p) { return read_entity<0x7c, 0x487ff0, G_BASE_SHORT, 1, true>(r, p); }
// 0x48dac0, 0x48db30, 0x48dbb0: CEntityAnimated (constructor 0x48e920), the readers "Compleate" 0x6bdd88,
// "SimpleNonStationary" 0x6bdd60, "SimpleStationary" 0x6bdd68
DWORD THISCALL EntityAnimated_Read0(void *r, void *p) { return read_entity<0xe4, 0x48e920, G_ANIMATED_KIND, 0, false>(r, p); }
DWORD THISCALL EntityAnimated_Read1(void *r, void *p) { return read_entity<0xe4, 0x48e920, G_ANIMATED_KIND, 1, true>(r, p); }
DWORD THISCALL EntityAnimated_Read2(void *r, void *p) { return read_entity<0xe4, 0x48e920, G_ANIMATED_KIND, 2, true>(r, p); }

// 0x49d390 (packet): the reader 0x6bdec0 registered as "COverlayWalkingBlockReaderObject"
DWORD THISCALL OverlayWalking_RegisterReader(void *packet)
{
    return fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x51ff30)(packet, glob<void *>(0x6bdec0), glob<const char *>(0x61324c));
}

// 0x4874a0 (slot 0: flags): the owning pointer array's deleting destructor (each object's slot 6 with 1, from
// the last; then the pool array's level 0x5f1df4)
void *THISCALL OwningArray_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(0x5f1de8);
    while (dw(self, 8)) {
        const DWORD i = dw(self, 8) - 1;
        if (void *o = bp(reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(self, 4)))[i]))
            vcall<DWORD>(o, 6, 1);
        const DWORD n = dw(self, 8);
        if (i < n) {
            if (i + 1 < n) {
                DWORD *d = reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(self, 4)));
                fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d + i, d + i + 1, (n - (i + 1)) * 4);
            }
            dw(self, 8)--;
        }
    }
    dw(self, 0) = vt(0x5f1df4);
    if (void *d = bp(dw(self, 4))) {
        pool_get_free(d, dw(self, 0xc) * 4);
        dw(self, 4) = 0;
        dw(self, 0xc) = 0;
    }
    dw(self, 8) = 0;
    if (flags & 1)
        heap_free(self);
    return self;
}

// ---- CEntityBase's rectangle (slot 37) --------------------------------------------------------------------------

constexpr DWORD MODEL_LIST = 0x6cfdd8, C_HEIGHT_X = 0x5f1e34;   // 0.7015
BYTE *model_object(DWORD index) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(MODEL_LIST), index); }
BYTE *model_sequence(BYTE *model, DWORD index) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x5173f0)(model, index); }
BYTE *model_next_sequence(BYTE *model, BYTE *seq) { return fn<BYTE *(THISCALL *)(void *, void *, DWORD)>(0x517410)(model, seq, 0); }
DWORD model_default_sequence(BYTE *model) { return fn<DWORD(THISCALL *)(void *)>(0x5173b0)(model); }
DWORD polygon_rect(BYTE *poly, const float *at_, int *out) { return fn<DWORD(THISCALL *)(void *, const float *, int *)>(0x546ad0)(poly, at_, out); }
DWORD children_rect(BYTE *self, int *rect, DWORD kind, int *offset)
{
    return fn<DWORD(THISCALL *)(void *, int *, DWORD, int *)>(0x4faff0)(self, rect, kind, offset);
}
// The offset of a sequence's frames: flag 0x20 shifts by the height (x * 0.7015, y * 0.5), flag 0x40 keeps y
void sequence_shift(DWORD flags, const int *o, int &x, int &y)
{
    if (flags & 0x20) {
        x = x87_fistp(fstp(fild(o[0]) - fild(o[2]) * cst(glob_addr(C_HEIGHT_X))));
        y = x87_fistp(fstp(fild(o[1]) - fild(o[2]) * cst(glob_addr(C_HALF))));
    } else {
        x = o[0];
        y = o[1];
        if (!(flags & 0x40))
            y -= o[2];
    }
}
void offset_of(BYTE *self, const int *offset, int *o)
{
    if (offset) {
        o[0] = offset[0];
        o[1] = offset[1];
        o[2] = offset[2];
    } else
        o[0] = o[1] = o[2] = 0;
    o[0] += vcall<int>(self, 52);
    o[1] += vcall<int>(self, 53);
    o[2] += (int)dw(self, 0x6c);
}

// 0x48a100 (slot 37: rectangle, kind, offset): see above
DWORD THISCALL EntityBase_Rect(BYTE *self, int *rect, DWORD kind, int *offset)
{
    int o[3];
    switch (kind) {
    case 0:
    case 1:
    case 2: {
        offset_of(self, offset, o);
        BYTE *model = model_object(dw(self, 0x74));
        DWORD index = dw(self, 0x78);
        BYTE found = 0;
        while (model) {
            for (BYTE *seq = model_sequence(model, index); seq; seq = model_next_sequence(model, seq)) {
                const DWORD flags = dw(seq, 0x1c);
                if ((kind == 1 && !(flags & 0x40)) || (kind == 2 && (flags & 0x40)))
                    continue;
                const DWORD angle = float_bits(fstp(vcall<long double>(self, 46)));
                const DWORD sum = dw(self, 0x58) + dw(self, 0x54);
                const DWORD rotation = vcall<DWORD>(self, 85);
                const DWORD frame_index =
                    fn<DWORD(THISCALL *)(void *, DWORD, DWORD, void *, DWORD)>(0x55fee0)(seq, rotation, angle, self, sum);   // CRotationSequence_FrameIndex
                if (frame_index == 0xffffffffu)
                    continue;
                BYTE *frame = vcall<BYTE *>(seq, 23, frame_index);
                if (!frame || !vcall<DWORD>(frame, 31))
                    continue;
                int x, y;
                sequence_shift(dw(seq, 0x1c), o, x, y);
                if (!found) {
                    vcall<DWORD>(frame, 36, rect);
                    rect[0] += x;
                    rect[1] += y;
                    rect[2] += x;
                    rect[3] += y;
                    found = 1;
                } else {
                    int r[4];
                    vcall<DWORD>(frame, 36, r);
                    r[0] += x;
                    r[1] += y;
                    r[2] += x;
                    r[3] += y;
                    if (rect[0] >= r[0])
                        rect[0] = r[0];
                    if (rect[1] >= r[1])
                        rect[1] = r[1];
                    if (rect[2] <= r[2])
                        rect[2] = r[2];
                    if (rect[3] <= r[3])
                        rect[3] = r[3];
                }
            }
            const DWORD also = dw(model, 0xac);
            if (!also)
                break;
            model = model_object(also);
            index = model_default_sequence(model);
        }
        return (children_rect(self, rect, kind, o) & ~0xffu) | found;
    }
    case 3: {
        BYTE *model = model_object(dw(self, 0x74));
        if (!model)
            return 0;
        float pos[4];
        slot_position(self, pos);
        return (polygon_rect(model + 0x78, pos, rect) & ~0xffu) | 1;
    }
    case 4: {
        // the model inlined from the list (eax: the records' address with al 0 when there is none)
        DWORD records = *glob<DWORD *>(0x6cfe08);
        if (!records)
            return 0;
        DWORD index = dw(self, 0x74);
        if (index >= *glob<DWORD *>(0x6cfe0c))
            index = 0;
        if (*at<BYTE *>(records + index * 0x80 + 0x7f))
            return records & ~0xffu;
        if (!*at<DWORD *>(records + index * 0x80 + 0x78)) {
            fn<DWORD(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(MODEL_LIST), index);   // DefinitionList_LoadRecord
            records = *glob<DWORD *>(0x6cfe08);
        }
        BYTE *model = bp(*at<DWORD *>(records + index * 0x80 + 0x78));
        if (!model)
            return records & ~0xffu;
        float pos[4];
        slot_position(self, pos);
        const int x = x87_fistp(fstp(fld(pos[0]) + fld(fl<float>(model, 0x98))));
        rect[0] = x;
        rect[2] = x + 1;
        const int y = x87_fistp(fstp(fld(pos[1]) + fld(fl<float>(model, 0x9c))));
        rect[1] = y;
        rect[3] = y + 1;
        return (pv(rect) & ~0xffu) | 1;
    }
    case 5: {
        int *cache = reinterpret_cast<int *>(self + 0x5c);
        if (cache[2] - cache[0] != 0 && !dw(self, 0xc)) {
            rect[0] = cache[0];
            rect[1] = cache[1];
            rect[2] = cache[2];
            rect[3] = cache[3];
            return ((DWORD)cache[3] & ~0xffu) | 1;
        }
        offset_of(self, offset, o);
        BYTE *model = model_object(dw(self, 0x74));
        if (!model)
            return 0;
        int t[4] = {99999, 99999, -99999, -99999};
        do {
            for (DWORD i = 0; i < dw(model, 0x48); i++) {
                BYTE *seq = model_sequence(model, i);
                int x, y;
                sequence_shift(dw(seq, 0x1c), o, x, y);
                const int *r = reinterpret_cast<const int *>(seq + 0xc);
                if (t[0] >= r[0] + x)
                    t[0] = r[0] + x;
                if (t[1] >= r[1] + y)
                    t[1] = r[1] + y;
                if (t[2] <= r[2] + x)
                    t[2] = r[2] + x;
                if (t[3] <= r[3] + y)
                    t[3] = r[3] + y;
            }
            float pt[2] = {fstp(fild(o[0])), fstp(fild(o[1]))};
            int p[4];
            polygon_rect(model + 0x78, pt, p);
            if (t[0] >= p[0])
                t[0] = p[0];
            if (t[1] >= p[1])
                t[1] = p[1];
            if (t[2] <= p[2])
                t[2] = p[2];
            if (t[3] <= p[3])
                t[3] = p[3];
            rect[0] = t[0];
            rect[1] = t[1];
            rect[2] = t[2];
            rect[3] = t[3];
            children_rect(self, rect, 5, o);
            const DWORD also = dw(model, 0xac);
            model = also ? model_object(also) : nullptr;
        } while (model);
        if (dw(self, 0xc))
            return (dw(self, 0xc) & ~0xffu) | 1;
        cache[0] = rect[0];
        cache[1] = rect[1];
        cache[2] = rect[2];
        cache[3] = rect[3];
        return ((DWORD)rect[3] & ~0xffu) | 1;
    }
    }
    return kind & ~0xffu;   // an unknown kind: al 0, eax the kind
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(objects_rest_b)
{
    const Hook list[] = {
#include "objects_rest_b_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
