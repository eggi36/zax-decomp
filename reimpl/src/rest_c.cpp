// The last non-drawing functions of 0x4f0000-0x55ffff (phase 3, band C): a shared vtable stub, the
// model list's search by sequence name, the message an entity broadcasts and the message name
// registry, two memory stream helpers, the world's message lines (all but their drawing), Salvage
// King's slot 25, the polygon kind's element array, four slots of the enum int, byte and bit field
// kinds, CPursueAI's class getters and destructor, the multiplayer start from the command line and the
// net packet's token map (0x55d1f0, 0x55d200).
//
//  - The model list 0x6cfdd8 (records of 0x80 bytes at 0x6cfe08, count 0x6cfe0c, folder 0x6cfe10;
//    +0x78 the model, +0x7f "loading"): 0x4f7fc0 goes through the records whose model is there and
//    returns the first that has a sequence of the name (CModel::IndexOfSequence 0x517280).
//  - The message registry (a function-local static at 0x6c2d18, guard bit 1 of 0x6c2d10, an int name
//    list with the vtable 0x5f0240, at exit 0x506720): slot 1 names an id, slot 2 (name, -1) looks a
//    name up, slot 18 adds one. The AI message (vtable 0x5eeb64 and derived): +4 its id.
//  - The world's message lines (+0x280 of the world, 0x1e84 bytes, constructor (0xb4, 0x41)): a
//    count, 25 lines of 0x138 bytes ({font index, text[0x12c], an owned block, age in seconds}), the
//    position x, y. Lines scroll up 7 pixels per second while the next line waits, and go after 12 s;
//    the drawing 0x5067f0 (fading from 11.5 s) is Rendering.
//  - The command line start: "-connect <address>" joins (0x5a2940 with the player options 0x6bd2f8 and
//    "-name"), "-host" hosts (the host options 0x6c0490 with "-name" and "-hostname", the world's slot
//    33, a 0x1a4-byte window 0x4d3080 pushed on the main menu g_MainMenu 0x714558).
#include "class_info.h"
#include "ai_common.h"
#include "field_kinds.h"
#include "memory_stream.h"
#include "zstring.h"

namespace zax {

using namespace ai;

namespace {

// ---- 0x4f0a70: xor al, al; ret 4 (slot 38 of CDeployableInventoryItem and 29 other vtables) ----------------

extern "C" void zax_rest_c_false4();
__asm__(".text\n"
        ".globl _zax_rest_c_false4\n"
        "_zax_rest_c_false4:\n"
        "  xorb %al, %al\n"
        "  retl $4\n");

// ---- 0x55d200: mov al, 1; ret 8 (the net packet's token map: every token counts as known) ------------------

extern "C" void zax_rest_c_known8();
__asm__(".text\n"
        ".globl _zax_rest_c_known8\n"
        "_zax_rest_c_known8:\n"
        "  movb $1, %al\n"
        "  retl $8\n");

// 0x55d1f0 (this: the packet's token map +0x28; table, id): the id unchanged
DWORD THISCALL NetTokenMap_Translate(void *, void *, DWORD id) { return id; }

// ---- The model list -------------------------------------------------------------------------------------

constexpr DWORD MODEL_LIST = 0x6cfdd8;
constexpr DWORD MODEL_RECORDS = 0x6cfe08, MODEL_COUNT = 0x6cfe0c, MODEL_FOLDER = 0x6cfe10;

// The resource list's inlined Get(i) as 0x4f7fc0 has it: the index clamped to 0, null while loading,
// an object not there yet made through the list's slot 37 (folder and name of slot 1)
BYTE *model_get(DWORD i)
{
    BYTE *data = *glob<BYTE **>(MODEL_RECORDS);
    if (!data)
        return nullptr;
    const DWORD k = i < *glob<DWORD *>(MODEL_COUNT) ? i : 0;
    BYTE *rec = data + k * 0x80;
    if (rec[0x7f])
        return nullptr;
    if (!dw(rec, 0x78) && !rec[0x7f]) {
        rec[0x7f] = 1;
        char path[0x104];
        fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, glob<const char *>(MODEL_FOLDER),
                                                                               vcall<const char *>(glob<void *>(MODEL_LIST), 1, k));
        char *p = (char *)path_name(path);
        DWORD o;
        BYTE *slot = *glob<BYTE **>(MODEL_RECORDS) + k * 0x80 + 0x78;
        if (p == path) {
            o = vcall<DWORD>(glob<void *>(MODEL_LIST), 37, glob<const char *>(0x62e558), p, slot);
        } else {
            p[-1] = 0;
            o = vcall<DWORD>(glob<void *>(MODEL_LIST), 37, path, p, slot);
        }
        dw(*glob<BYTE **>(MODEL_RECORDS), k * 0x80 + 0x78) = o;
        (*glob<BYTE **>(MODEL_RECORDS))[k * 0x80 + 0x7f] = 0;
        data = *glob<BYTE **>(MODEL_RECORDS);
    }
    return bp(dw(data, k * 0x80 + 0x78));
}

// 0x4f7fc0 (ecx: the sequence name, edx: where the sequence's index goes or null): the first model of
// the list that is there and has the sequence; 0 when none has
BYTE *__fastcall ModelList_FindSequence(const char *name, int *index)
{
    for (DWORD i = 0; i < *glob<DWORD *>(MODEL_COUNT); i++) {
        if (!dw(*glob<BYTE **>(MODEL_RECORDS), i * 0x80 + 0x78))
            continue;
        BYTE *model = model_get(i);
        const int k = fn<int(THISCALL *)(void *, const char *)>(0x517280)(model, name);
        if (k != -1) {
            if (index)
                *index = k;
            return model;
        }
    }
    return nullptr;
}

// ---- Messages ----------------------------------------------------------------------------------------------

// 0x5011d0 (this, sender, argument): the message CEntityCharacter's slot 62 broadcasts (vtable 0x5f99ec,
// 0x14 bytes on the stack: the AI message with the id 0x6c2ccc, +0xc the sender, +0x10 the argument)
BYTE *THISCALL BroadcastMessage_Ctor(BYTE *self, DWORD sender, DWORD argument)
{
    dw(self, 4) = *glob<DWORD *>(0x6c2ccc);
    self[8] = 0;
    dw(self, 0) = vt(0x5f99ec);
    dw(self, 0xc) = sender;
    dw(self, 0x10) = argument;
    return self;
}

constexpr DWORD REGISTRY = 0x6c2d18, REGISTRY_GUARD = 0x6c2d10;
void registry_init()
{
    BYTE &guard = *glob<BYTE *>(REGISTRY_GUARD);
    if (guard & 1)
        return;
    guard |= 1;
    fn<void *(THISCALL *)(void *)>(0x592200)(glob<void *>(REGISTRY));
    dw(glob<void *>(REGISTRY), 0) = vt(0x5f0240);
    crt_atexit(code<void(__cdecl *)()>(0x506720));
}

// 0x506620 (this: an AI message; name): 1 when the message's id has that name (case blind), else 0
DWORD THISCALL Message_HasName(BYTE *self, const char *name)
{
    const DWORD id = dw(self, 4);
    registry_init();
    const char *s = vcall<const char *>(glob<void *>(REGISTRY), 1, id);
    return crt_stricmp(name, s) == 0 ? 1 : 0;
}

// 0x506680 (ecx: a name): the id of the message name, added to the registry when it is new
DWORD __fastcall Message_IdOf(const char *name)
{
    registry_init();
    const DWORD k = vcall<DWORD>(glob<void *>(REGISTRY), 2, name, 0xffffffffu);
    if (k != 0xffffffffu)
        return k;
    registry_init();
    return vcall<DWORD>(glob<void *>(REGISTRY), 18, name);
}

// ---- Memory stream ----------------------------------------------------------------------------------------

// 0x506320 (other): empties the stream and writes the other's bytes into it (eax: the write's)
DWORD THISCALL MemoryStream_CopyFrom(MemoryStream *self, MemoryStream *other)
{
    self->position = 0;
    self->size = 0;
    vcall<DWORD>(self, 5, 0u);   // Seek(0)
    BYTE *data = other->data;
    const DWORD n = vcall<DWORD>(other, 7);   // Size
    return vcall<DWORD>(self, 2, data, n);   // Write
}

// 0x506350: position and size 0 (eax 0)
DWORD THISCALL MemoryStream_Clear(MemoryStream *self)
{
    self->position = 0;
    self->size = 0;
    return 0;
}

// ---- The world's message lines -------------------------------------------------------------------------------

struct MessageLine {
    DWORD font;          // 0x00 index into the font list 0x6be1e0
    char text[0x12c];    // 0x04
    void *owned;         // 0x130 deleted with the line
    float age;           // 0x134 seconds
};
static_assert(sizeof(MessageLine) == 0x138);
struct MessageLines {
    DWORD count;
    MessageLine lines[25];
    DWORD x, y;   // 0x1e7c, 0x1e80
};
static_assert(sizeof(MessageLines) == 0x1e84);

DWORD op_delete(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }

// 0x506730 (x, y)
MessageLines *THISCALL MessageLines_Ctor(MessageLines *self, DWORD x, DWORD y)
{
    for (MessageLine &l : self->lines) {
        l.font = 0;
        l.text[0] = 0;
        dw(&l, 0x134) = 0;
        l.owned = nullptr;
    }
    self->x = x;
    self->y = y;
    self->count = 0;
    for (MessageLine &l : self->lines) {
        l.text[0] = 0;
        if (l.owned) {
            op_delete(l.owned);
            l.owned = nullptr;
        }
    }
    return self;
}

// 0x5067b0: the owned blocks from the last line to the first (eax: the last one read or delete's)
DWORD THISCALL MessageLines_Dtor(MessageLines *self)
{
    DWORD r = 0;
    for (int k = 24; k >= 0; k--) {
        r = (DWORD)(uintptr_t)self->lines[k].owned;
        if (r) {
            r = op_delete(self->lines[k].owned);
            self->lines[k].owned = nullptr;
        }
    }
    return r;
}

// 0x506f70: no lines, the texts and owned blocks cleared (eax as the destructor's)
DWORD THISCALL MessageLines_Clear(MessageLines *self)
{
    self->count = 0;
    DWORD r = 0;
    for (MessageLine &l : self->lines) {
        l.text[0] = 0;
        r = (DWORD)(uintptr_t)l.owned;
        if (r) {
            r = op_delete(l.owned);
            l.owned = nullptr;
        }
    }
    return r;
}

// 0x506fb0: the first line goes, the others move up (its owned block deleted; eax: the new count).
// With no line at all the count wraps and the move runs past the lines (never called so).
DWORD THISCALL MessageLines_RemoveFirst(MessageLines *self)
{
    if (self->count - 1 != 0) {
        for (DWORD i = 0; i < self->count - 1; i++) {
            MessageLine *l = &self->lines[i], *next = l + 1;
            l->font = next->font;
            dw(l, 0x134) = dw(next, 0x134);
            char *d = l->text;
            const char *s = next->text;
            while ((*d++ = *s++) != 0) {
            }
            if (l->owned) {
                op_delete(l->owned);
                l->owned = nullptr;
            }
            l->owned = next->owned;
            next->owned = nullptr;
        }
    }
    return --self->count;
}

// The font list's inlined Get(i) (font list 0x6be1e0: records 0x6be210, count 0x6be214, folder 0x6be218)
BYTE *font_get(DWORD i)
{
    constexpr DWORD FONTS = 0x6be1e0, RECORDS = 0x6be210, COUNT = 0x6be214, FOLDER = 0x6be218;
    BYTE *data = *glob<BYTE **>(RECORDS);
    if (!data)
        return nullptr;
    const DWORD k = i < *glob<DWORD *>(COUNT) ? i : 0;
    BYTE *rec = data + k * 0x80;
    if (rec[0x7f])
        return nullptr;
    if (!dw(rec, 0x78) && !rec[0x7f]) {
        rec[0x7f] = 1;
        char path[0x104];
        fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, glob<const char *>(FOLDER),
                                                                               vcall<const char *>(glob<void *>(FONTS), 1, k));
        char *p = (char *)path_name(path);
        DWORD o;
        BYTE *slot = *glob<BYTE **>(RECORDS) + k * 0x80 + 0x78;
        if (p == path) {
            o = vcall<DWORD>(glob<void *>(FONTS), 37, glob<const char *>(0x62e558), p, slot);
        } else {
            p[-1] = 0;
            o = vcall<DWORD>(glob<void *>(FONTS), 37, path, p, slot);
        }
        dw(*glob<BYTE **>(RECORDS), k * 0x80 + 0x78) = o;
        (*glob<BYTE **>(RECORDS))[k * 0x80 + 0x7f] = 0;
        data = *glob<BYTE **>(RECORDS);
    }
    return bp(dw(data, k * 0x80 + 0x78));
}

// 0x506a10 (seconds): every line ages by the time, the next line only when this one has scrolled up a
// font height and a pixel (7 pixels per second); a line older than 12 s goes (the following line then
// moves into its place but is skipped). eax: the index the loop stopped at.
DWORD THISCALL MessageLines_Update(MessageLines *self, float seconds)
{
    DWORD i = 0;
    MessageLine *l = self->lines;
    while (i < self->count) {
        l->age = float_in_memory((long double)seconds + (long double)l->age);
        const DWORD next = i + 1;
        bool on = false;
        if (next < self->count) {
            BYTE *font = font_get(l->font);
            const int scrolled = x87_ftol((long double)l->age * (long double)*glob<float *>(0x5eeeac));   // 7.0
            const int height = fn<int(THISCALL *)(void *)>(0x4a6be0)(font);
            if (scrolled >= height + 1)
                on = true;
        }
        if (!x87_le(l->age, *glob<float *>(0x5ec3f8)))   // 12.0
            fn<DWORD(THISCALL *)(MessageLines *)>(0x506fb0)(self);
        l++;
        i = next;
        if (!on)
            break;
    }
    return i;
}

// ---- Game types, field kinds ---------------------------------------------------------------------------------

// 0x51d320 (a, b): CSalvageKingGameType's slot 25 passes on to its slot 48
DWORD THISCALL SalvageKing_Slot25(void *self, DWORD a, DWORD b) { return vcall<DWORD>(self, 48, a, b); }

// 0x548330 (n, unused, unused): slot 38 of the polygon kind: new[] of n zeroed 0x28-byte points with the
// count in front (null when operator new gives none)
BYTE *THISCALL PolygonField_NewPoints(void *, int n, DWORD, DWORD)
{
    BYTE *p = static_cast<BYTE *>(heap_alloc((DWORD)n * 0x28 + 4));
    if (!p)
        return nullptr;
    dw(p, 0) = (DWORD)n;
    p += 4;
    for (int i = 0; i < n; i++)
        memset(p + i * 0x28, 0, 0x28);
    return p;
}

void *get_or_add_dynamic(void *object, void *desc) { return fn<void *(THISCALL *)(void *, void *)>(0x417340)(object, desc); }
void *get_dynamic(void *object, void *desc) { return fn<void *(THISCALL *)(void *, void *)>(0x417320)(object, desc); }

// 0x54dae0 (object, value): the enum int kind's value set through the setter (+0x3c) or stored at the
// offset, in the object or its dynamic property (eax: the setter's or the value's block)
DWORD THISCALL EnumIntField_Set(BYTE *self, void *object, DWORD value)
{
    BYTE *p = static_cast<BYTE *>(get_or_add_dynamic(object, self));
    if (const DWORD setter = dw(self, 0x3c))
        return reinterpret_cast<ValueSetter>(setter)(p, value);
    dw(p, dw(self, 0x10)) = value;
    return (DWORD)(uintptr_t)p;
}

// 0x54de90 (object): the byte kind's value, the default (+0x2c) without a block (upper bytes 0)
DWORD THISCALL ByteField_Get(BYTE *self, void *object)
{
    BYTE *p = static_cast<BYTE *>(get_dynamic(object, self));
    if (p)
        return with_low_byte((DWORD)(uintptr_t)p, p[dw(self, 0x10)]);
    return self[0x2c];
}

// 0x54dec0 (object, value): the byte kind's value set through the setter (+0x28) or stored
DWORD THISCALL ByteField_Set(BYTE *self, void *object, DWORD value)
{
    BYTE *p = static_cast<BYTE *>(get_or_add_dynamic(object, self));
    if (const DWORD setter = dw(self, 0x28))
        return reinterpret_cast<ValueSetter>(setter)(p, value);
    p[dw(self, 0x10)] = (BYTE)value;
    return (DWORD)(uintptr_t)p;
}

// 0x54e1b0 (object, stream): slot 29 of the bit kind: one byte read (into the stream argument's low
// byte), the mask (+0x2c) set when it is not 0, cleared otherwise; true (upper bytes the value's address)
DWORD THISCALL BitField_ReadBinary(BYTE *self, BYTE *object, void *stream)
{
    DWORD v = (DWORD)(uintptr_t)stream;
    fn<void *(THISCALL *)(void *, BYTE *)>(0x4a22a0)(stream, reinterpret_cast<BYTE *>(&v));
    DWORD *value = reinterpret_cast<DWORD *>(object + dw(self, 0x10));
    if ((BYTE)v)
        *value |= dw(self, 0x2c);
    else
        *value &= ~dw(self, 0x2c);
    return ((DWORD)(uintptr_t)value & 0xffffff00u) | 1;
}

// ---- CPursueAI ---------------------------------------------------------------------------------------------

// 0x54fdc0, 0x54fe10 (two copies): CPursueAI (0x10 bytes, class 0x712230, version 1, abstract, under
// CLayerPartAI, no Create, no fields)
CClassInfo *__cdecl PursueAI_ClassInfo()
{
    return class_getter(0x712230, 0x61eb18 /* "CPursueAI" */, 1.0f, layer_part_ai_class, 0x10, 1, 0x46b760, 0x52dc80, 0x58ea40);
}

// 0x54fe70: CPursueAI's level (vtable 0x5efb54), CLayerPartAI's (0x5ed1d0), then CAIBase's destructor
// (eax: its)
DWORD THISCALL PursueAI_Dtor(BYTE *self)
{
    vtable_and_free(self, vt(0x5efb54));
    vtable_and_free(self, vt(0x5ed1d0));
    return fn<DWORD(THISCALL *)(void *)>(0x4fd080)(self);
}

// ---- The multiplayer start from the command line ------------------------------------------------------------

constexpr DWORD PLAYER_OPTIONS = 0x6bd2f8, HOST_OPTIONS = 0x6c0490;
BYTE value_of(DWORD option, void *out) { return fn<BYTE(__fastcall *)(const char *, void *)>(0x4d21a0)(at<const char *>(option), out); }

// 0x519ef0 (called by the application's start 0x5a070c): "-connect" joins the address given (true when
// the join starts), else "-host" opens the host window; false when neither. The upper bytes of eax are
// the local string's destructor's.
DWORD __cdecl Net_StartFromCommandLine()
{
    ZString address;
    zs_ctor(reinterpret_cast<BYTE *>(&address));
    if (value_of(glob_addr(0x61bee4) /* "-connect" */, &address)) {
        fn<void(THISCALL *)(void *)>(0x46ce50)(glob<void *>(PLAYER_OPTIONS));
        value_of(glob_addr(0x61bedc) /* "-name" */, glob<void *>(PLAYER_OPTIONS));
        const char *a = zs_c_str(&address);
        if (fn<BYTE(__fastcall *)(DWORD, const char *, DWORD, DWORD, DWORD)>(0x5a2940)(0, a, 0, glob_addr(PLAYER_OPTIONS), 0)) {
            const DWORD r = fn<DWORD(THISCALL *)(void *)>(0x4defd0)(&address);
            return (r & 0xffffff00u) | 1;
        }
    }
    if (!fn<BYTE(__fastcall *)(const char *)>(0x4d22c0)(glob<const char *>(0x61bed4 /* "-host" */))) {
        const DWORD r = fn<DWORD(THISCALL *)(void *)>(0x4defd0)(&address);
        return r & 0xffffff00u;
    }
    fn<void(THISCALL *)(void *)>(0x46ce50)(glob<void *>(PLAYER_OPTIONS));
    fn<void(THISCALL *)(void *)>(0x4d2c50)(glob<void *>(HOST_OPTIONS));
    value_of(glob_addr(0x61bedc) /* "-name" */, glob<void *>(PLAYER_OPTIONS));
    value_of(glob_addr(0x61bec8) /* "-hostname" */, glob<void *>(HOST_OPTIONS));
    vcall<DWORD>(*glob<void **>(0x6c2080), 33);   // g_World
    void *w = pool_alloc(0x1a4);
    if (w)
        w = fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x4d3080)(w, 0, 1);
    fn<void(THISCALL *)(void *, void *, DWORD)>(0x4040c0)(*glob<void **>(0x714558), w, 1);   // g_MainMenu
    const DWORD r = fn<DWORD(THISCALL *)(void *)>(0x4defd0)(&address);
    return (r & 0xffffff00u) | 1;
}

}  // namespace

// ---- Hooks ---------------------------------------------------------------------------------------------------

ZAX_HOOKS(rest_c)
{
    const Hook list[] = {
        {0x4f0a70, (void *)&zax_rest_c_false4, "Stub_ReturnFalse4", 5, {0x32, 0xc0, 0xc2, 0x04, 0x00, 0x90, 0x90, 0x90}},
        {0x4f7fc0, (void *)&ModelList_FindSequence, "ModelList_FindSequence", 6, {0x81, 0xec, 0x08, 0x01, 0x00, 0x00, 0xa1, 0x0c}},
        {0x5011d0, (void *)&BroadcastMessage_Ctor, "BroadcastMessage_Ctor", 6, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0xc1, 0x8b, 0x0d}},
        {0x506320, (void *)&MemoryStream_CopyFrom, "MemoryStream_CopyFrom", 6, {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xc0, 0x57, 0x50}},
        {0x506350, (void *)&MemoryStream_Clear, "MemoryStream_Clear", 5, {0x33, 0xc0, 0x89, 0x41, 0x0c, 0x89, 0x41, 0x10}},
        {0x506620, (void *)&Message_HasName, "Message_HasName", 10, {0x56, 0x8b, 0x71, 0x04, 0x8a, 0x0d, 0x10, 0x2d}},
        {0x506680, (void *)&Message_IdOf, "Message_IdOf", 5, {0xa0, 0x10, 0x2d, 0x6c, 0x00, 0x53, 0xb3, 0x01}},
        {0x506730, (void *)&MessageLines_Ctor, "MessageLines_Ctor", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x33, 0xdb}},
        {0x5067b0, (void *)&MessageLines_Dtor, "MessageLines_Dtor", 8, {0x56, 0x57, 0x8d, 0xb1, 0xac, 0x1f, 0x00, 0x00}},
        {0x506a10, (void *)&MessageLines_Update, "MessageLines_Update", 6, {0x81, 0xec, 0x08, 0x01, 0x00, 0x00, 0x53, 0x55}},
        {0x506f70, (void *)&MessageLines_Clear, "MessageLines_Clear", 8, {0x56, 0x57, 0xc7, 0x01, 0x00, 0x00, 0x00, 0x00}},
        {0x506fb0, (void *)&MessageLines_RemoveFirst, "MessageLines_RemoveFirst", 12, {0x51, 0x55, 0x8b, 0xe9, 0xc7, 0x44, 0x24, 0x04}},
        {0x519ef0, (void *)&Net_StartFromCommandLine, "Net_StartFromCommandLine", 5, {0x51, 0x8d, 0x4c, 0x24, 0x00, 0xe8, 0x96, 0x4d}},
        {0x51d320, (void *)&SalvageKing_Slot25, "CSalvageKingGameType::Slot25", 6, {0x8b, 0x54, 0x24, 0x08, 0x8b, 0x01, 0x52, 0x8b}},
        {0x548330, (void *)&PolygonField_NewPoints, "PolygonField_NewPoints", 5, {0x57, 0x8b, 0x7c, 0x24, 0x08, 0x8d, 0x04, 0xbf}},
        {0x54dae0, (void *)&EnumIntField_Set, "EnumIntField_Set", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x56}},
        {0x54de90, (void *)&ByteField_Get, "ByteField_Get", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x56}},
        {0x54dec0, (void *)&ByteField_Set, "ByteField_Set", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x56}},
        {0x54e1b0, (void *)&BitField_ReadBinary, "BitField_ReadBinary", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x0c, 0x8d}},
        {0x54fdc0, (void *)&PursueAI_ClassInfo, "CPursueAI_ClassInfo", 5, {0xa1, 0x30, 0x22, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x54fe10, (void *)&PursueAI_ClassInfo, "CPursueAI::ClassInfo2", 5, {0xa1, 0x30, 0x22, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x54fe70, (void *)&PursueAI_Dtor, "CPursueAI::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x54, 0xfb, 0x5e}},
        {0x55d1f0, (void *)&NetTokenMap_Translate, "NetTokenMap_Translate", 7, {0x8b, 0x44, 0x24, 0x08, 0xc2, 0x08, 0x00, 0x90}},
        {0x55d200, (void *)&zax_rest_c_known8, "NetTokenMap_IsKnown", 5, {0xb0, 0x01, 0xc2, 0x08, 0x00, 0x90, 0x90, 0x90}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
