// Field kinds: the subclasses of CFieldDesc, one per kind of value (number, text, flags, colour,
// pointer, array, ...). Every kind has its own vtable with the 24 slots of the base (field_desc.h)
// and one or two constructors that the RegisterFields functions of the classes call
// (zax-formats/zaxtools/exeschema.py, ENCODING). The names of the kinds are chosen here from the fields that use
// them; the vtables and constructors are the evidence.
//
// Vtable slots (the same for every kind; the kinds' vtables have at least 34 slots, more than the
// 24 counted for the base):
//   0 deleting destructor          1 clone (operator new + copy; 0x4011e0 returns 0)
//   2 "plain value" (0x401260 true, 0x401200 false)
//   4 CopyValue(dst object, src object, src descriptor)
//   6 InitValue(object)            8 FreeValue(object)            10 SetDefault(object)
//   13 VisitObjects(object, flag)  20 HasValue(object)  (field_streams.h)
//   16 value as text (object, ZString *out)   19 value from text
//   23 WriteText(object, writer) (0x418fe0 for the plain kinds, field_streams.h)
// A value lives at object + offset (+0x10). Many kinds have a setter (a member function of the
// object, thiscall with the new value or a pointer to it) that is used instead of writing directly.
//
// Constructor arguments: name, [units,] offset, size (+0x14), a second offset that is not used,
// flags (+0x18), then the kind's own. The base part comes from CFieldDesc::Ctor.
#pragma once
#include "field_desc.h"
#include "ptr_array.h"

namespace zax {

// Setter of a value: member function of the object
using ValueSetter = DWORD(THISCALL *)(void *object, DWORD value);

// List of names for a number field ({vtable, pool pointer array of entries}); entries are
// {char *name, value} with an int or float value (8 bytes) or a double (12 bytes). Built by
// FieldDesc_AddEnum 0x54da60, copied by the clones.
struct FieldNameList {
    DWORD vftable;
    ZPtrArray entries;
};
static_assert(sizeof(FieldNameList) == 0x14);

// Colour value (0x10 bytes, constructor 0x53f000 clears +0xc, 0x53f060 sets black, 0x53f0c0 from
// four bytes). Copies take bytes 0-6, 8-0xa and 0xc-0xf (the bytes 7 and 0xb are padding).
struct FieldColor {
    BYTE b[0x10];
};
void copy_color(FieldColor *dst, const FieldColor *src);

// 0x5ec684, 0x38 bytes, constructor 0x414cf0: 16-bit flags named by a list ("$all" = every flag)
// (CLayerPart.Category, CPickupAI.Who Can Pick Up)
struct CFlagsFieldDesc : CFieldDesc {
    const char *default_names;   // 0x28
    void *names;                 // 0x2c list of the flag names (slot 22: count)
    DWORD setter;                // 0x30
    BYTE f34;                    // 0x34 set to 1
    BYTE pad35[3];

    CFlagsFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD names_,
                                   DWORD u18, DWORD default_, DWORD setter_);   // 0x414cf0
    CFieldDesc *THISCALL Clone();                                                  // 0x414d40
    DWORD THISCALL CopyValue(void *dst, void *src, const CFieldDesc *from);        // 0x414d90
};
static_assert(sizeof(CFlagsFieldDesc) == 0x38);

// 0x5ec91c, 0x34 bytes, constructors 0x415dd0 and 0x415ea0 (+0x28 = 1): ZString value
// (CDialogLine.Name, COtherMapAction.Other Map Name; with +0x28 the texts of the display options)
struct CStringFieldDesc : CFieldDesc {
    BYTE f28;                    // 0x28 0 or 1 by constructor; 1 also handles the string at +4 (0x5007c0)
    BYTE pad29[3];
    ZString default_value;       // 0x2c
    DWORD setter;                // 0x30

    CStringFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                    const char *default_, DWORD setter_);   // 0x415dd0
    CStringFieldDesc *THISCALL Ctor1(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                     const char *default_, DWORD setter_);  // 0x415ea0
    CFieldDesc *THISCALL Clone();                                             // 0x415e20
    CStringFieldDesc *THISCALL DeletingDtor(DWORD flags);                     // 0x4fa740
};
static_assert(sizeof(CStringFieldDesc) == 0x34);

// 0x5eca78, 0x44 bytes, constructors 0x417930 and 0x417a20: array of objects created with new[]
// (count at the offset, data pointer at +0x30, optional capacity at +0x28) (CInventory.Item Type Info,
// CRotationSequence.Cutting Lines)
struct CListFieldDesc : CFieldDesc {
    DWORD capacity_offset;   // 0x28 0 = none
    DWORD f2c;               // 0x2c
    DWORD data_offset;       // 0x30
    DWORD f34;               // 0x34
    void *element_class;     // 0x38 class info of the elements
    DWORD element_size;      // 0x3c the class's size (+0x3c of the class info)
    DWORD f40;               // 0x40

    CListFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD data_offset_,
                                  DWORD f34_, DWORD a6, void *element_class_, DWORD u18, DWORD f40_);   // 0x417930
    CListFieldDesc *THISCALL Ctor2(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD capacity_offset_,
                                   DWORD f2c_, DWORD a6, DWORD data_offset_, DWORD f34_, DWORD a9, DWORD a10,
                                   void *element_class_, DWORD u18, DWORD f40_);                        // 0x417a20
    CFieldDesc *THISCALL Clone();                   // 0x417990
    DWORD THISCALL InitValue(BYTE *object);         // 0x417b00
    DWORD THISCALL FreeValue(BYTE *object);         // 0x417b30
};
static_assert(sizeof(CListFieldDesc) == 0x44);

// 0x5ecb38, 0x34 bytes, constructor 0x4184c0: a pointer value with a default (CRemoveAIAction.AI)
struct CAIFieldDesc : CFieldDesc {
    DWORD f28;             // 0x28
    DWORD default_value;   // 0x2c
    DWORD pad30;           // allocated by the clone, never written

    CAIFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD f28_, DWORD u18,
                                DWORD default_);   // 0x4184c0
    CFieldDesc *THISCALL Clone();                   // 0x418500
    DWORD THISCALL SetDefault(BYTE *object);        // 0x418540
};
static_assert(sizeof(CAIFieldDesc) == 0x34);

// 0x5ecbf4, 0x3c bytes, constructor 0x4186f0: an object the field owns (CConditionalAction.Try Action)
struct CObjectFieldDesc : CFieldDesc {
    DWORD f28, f2c, f30, f34, f38;   // 0x28-0x38

    CObjectFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD f2c_, DWORD u18,
                                    DWORD f34_, DWORD f38_, DWORD f28_);   // 0x4186f0
    CFieldDesc *THISCALL Clone();                     // 0x418740 (+0x28 is not copied)
    DWORD THISCALL FreeValue(BYTE *object);           // 0x418d90
};
static_assert(sizeof(CObjectFieldDesc) == 0x3c);

// 0x5ecd6c, 0x48 bytes, constructors 0x419110 and 0x419230: pointer array of objects with an own
// element descriptor (CActionQueue.Items, CGeneratorAI.Things to Generate)
struct CObjectArrayFieldDesc : CFieldDesc {
    DWORD f28, f2c, f30, f34;   // 0x28-0x34
    CFieldDesc *element;        // 0x38 owned
    DWORD element_f3c;          // 0x3c slot 33 of the element descriptor (0x418d70 for objects)
    DWORD f40;                  // 0x40
    BYTE f44;                   // 0x44
    BYTE pad45[3];

    CObjectArrayFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD f30_,
                                         DWORD f34_, DWORD a6, CFieldDesc *element_, DWORD u18, DWORD f40_);   // 0x419110
    CObjectArrayFieldDesc *THISCALL Ctor2(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD f28_,
                                          DWORD f2c_, DWORD a6, DWORD f30_, DWORD f34_, DWORD a9, DWORD f44_,
                                          CFieldDesc *element_, DWORD u18, DWORD f40_);                         // 0x419230
    CFieldDesc *THISCALL Clone();                                  // 0x419180
    CObjectArrayFieldDesc *THISCALL DeletingDtor(DWORD flags);     // 0x4191f0
};
static_assert(sizeof(CObjectArrayFieldDesc) == 0x48);

// 0x5efe30, 0x3c bytes, constructors 0x46dcc0 and 0x46de20: colour (CCategoryDefinition.MiniMap Color)
struct CColorFieldDesc : CFieldDesc {
    FieldColor default_value;   // 0x28
    DWORD setter;               // 0x38 called with a pointer to the colour

    CColorFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                   const FieldColor *default_, DWORD setter_);   // 0x46dcc0
    CColorFieldDesc *THISCALL CtorRGBA(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                       DWORD c0, DWORD c1, DWORD c2, DWORD c3, DWORD setter_);   // 0x46de20
    CFieldDesc *THISCALL Clone();                                          // 0x46dd50
    DWORD THISCALL CopyValue(BYTE *dst, BYTE *src, const CFieldDesc *from);   // 0x46ddc0
    BYTE *THISCALL SetDefault(BYTE *object);                               // 0x46dea0
};
static_assert(sizeof(CColorFieldDesc) == 0x3c);

// 0x5f17ec, 0x28 bytes, constructor 0x482ef0: CObject.Dynamic Properties
struct CDynamicPropertiesFieldDesc : CFieldDesc {
    CDynamicPropertiesFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused,
                                               DWORD u18);   // 0x482ef0
    CFieldDesc *THISCALL Clone();                            // 0x482f20
};
static_assert(sizeof(CDynamicPropertiesFieldDesc) == 0x28);

// 0x5f46fc (float point, constructor 0x4a7fa0) and 0x5fd1d0 (integer point, 0x545450 and 0x545530),
// 0x34 bytes: two dwords with a default and a setter (called with a pointer to the point)
struct CPointFieldDesc : CFieldDesc {
    DWORD default_x, default_y;   // 0x28, 0x2c
    DWORD setter;                 // 0x30

    CPointFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                   const DWORD *default_, DWORD setter_);   // 0x4a7fa0
    CPointFieldDesc *THISCALL CtorInt(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                      const DWORD *default_, DWORD setter_);   // 0x545450
    CPointFieldDesc *THISCALL CtorIntXY(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                        DWORD x, DWORD y, DWORD setter_);       // 0x545530
    CFieldDesc *THISCALL Clone();                                                // 0x4a8000
    CFieldDesc *THISCALL CloneInt();                                             // 0x5454b0
    DWORD THISCALL CopyValue(BYTE *dst, BYTE *src, const CFieldDesc *from);      // 0x4a8040
    DWORD THISCALL CopyValueInt(BYTE *dst, BYTE *src, const CFieldDesc *from);   // 0x5454f0
    BYTE *THISCALL SetDefault(BYTE *object);                                     // 0x4a80f0
    BYTE *THISCALL SetDefaultInt(BYTE *object);                                  // 0x545580
};
static_assert(sizeof(CPointFieldDesc) == 0x34);

// 0x5f4ce8, 0x44 bytes, constructor 0x4b58f0: CLayerSaveData.Fog Of War, CZaxSaveGameStats.Screen Shot
struct CImageFieldDesc : CFieldDesc {
    DWORD f28, f2c, f30;   // 0x28-0x30
    FieldColor color;      // 0x34

    CImageFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18, DWORD f28_,
                                   DWORD f2c_, DWORD f30_, const FieldColor *color_);   // 0x4b58f0
    CFieldDesc *THISCALL Clone();   // 0x4b5970 (copies only the base part)
};
static_assert(sizeof(CImageFieldDesc) == 0x44);

// 0x5fd8b0, 0x30 bytes, constructor 0x54e220: a named resource (CDialogActionBase.Line, music); the
// vtable has no clone (_purecall). Derived: 0x5fd970 (constructor 0x54e560, +0x34; fonts, scripts,
// overlays) and 0x5faf10 (constructor 0x5182d0, +0x34; CEntityBase.Cur Sequence).
struct CResourceFieldDesc : CFieldDesc {
    DWORD f28;    // 0x28
    BYTE f2c;     // 0x2c
    BYTE f2d;     // 0x2d cleared by the constructors
    BYTE pad2e[2];
    DWORD f30;    // 0x30
    DWORD f34;    // 0x34 only in the derived kinds

    CResourceFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                      DWORD f28_, DWORD f30_, DWORD f2c_);                    // 0x54e220
    CResourceFieldDesc *THISCALL CopyCtor(const CResourceFieldDesc *other);                // 0x54e270
    CResourceFieldDesc *THISCALL CtorTyped(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD f34_,
                                           DWORD u18, DWORD f28_, DWORD f30_, DWORD f2c_);  // 0x54e560
    CFieldDesc *THISCALL CloneTyped();                                                       // 0x54e5b0
    CResourceFieldDesc *THISCALL CtorSequence(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD f34_,
                                              DWORD u18, DWORD f28_, DWORD f30_);           // 0x5182d0
    CFieldDesc *THISCALL CloneSequence();                                                    // 0x518320
};
static_assert(sizeof(CResourceFieldDesc) == 0x38);

// 0x5fb840, 0x30 bytes, constructor 0x523e30: pointer to another object, not owned
// (CActionQueueItem.Instigator)
struct CObjectRefFieldDesc : CFieldDesc {
    DWORD f28, f2c;   // 0x28, 0x2c

    CObjectRefFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD f28_,
                                       DWORD u18, DWORD f2c_);   // 0x523e30
    CFieldDesc *THISCALL Clone();                                // 0x523e70
};
static_assert(sizeof(CObjectRefFieldDesc) == 0x30);

// 0x5fda30, 0x30 bytes: embedded object (constructor 0x54e690, the 'emb' fields) or pointer with a
// class (0x54e610, CDropCurrentItemOfTypeAction.Drop Item Action). Derived: 0x5fbc9c (constructor
// 0x5284c0, +0x30 a field of the embedded class; CShakeScreenAction.Pixels To Shake).
struct CEmbeddedFieldDesc : CFieldDesc {
    DWORD f28;              // 0x28
    DWORD f2c;              // 0x2c
    CFieldDesc *value;      // 0x30 only in 0x5fbc9c

    CEmbeddedFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18);   // 0x54e690
    CEmbeddedFieldDesc *THISCALL CtorPtr(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD f28_,
                                         DWORD u18);                                                      // 0x54e610
    CEmbeddedFieldDesc *THISCALL CopyCtor(const CEmbeddedFieldDesc *other);                               // 0x54e6d0
    CFieldDesc *THISCALL Clone();                                                                         // 0x54e650
    DWORD THISCALL CopyValue(BYTE *dst, BYTE *src, const CFieldDesc *from);   // 0x54e700 (slot 20 of the value)
    DWORD THISCALL SetDefault(BYTE *object);                                  // 0x54e730 (slot 19 of the value)
    CEmbeddedFieldDesc *THISCALL CtorValue(const char *name, DWORD offset, DWORD u14, DWORD unused, void *cls,
                                           const char *field, DWORD u18);     // 0x5284c0
    CFieldDesc *THISCALL CloneValue();                                        // 0x528510
};
static_assert(sizeof(CEmbeddedFieldDesc) == 0x34);

// 0x5fd2a0, 0x30 bytes, constructor 0x547fd0: polygon (CFreeRangePoly.Polygon)
struct CPolygonFieldDesc : CFieldDesc {
    DWORD f28;
    DWORD pad2c;   // allocated by the clone, never written

    CPolygonFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                     DWORD f28_, DWORD unused6);   // 0x547fd0 (7 arguments, the last unused)
    CFieldDesc *THISCALL Clone();                   // 0x548010 (+0x28 = 0, not copied)
};
static_assert(sizeof(CPolygonFieldDesc) == 0x30);

// 0x5fd374, 0x30 bytes, constructor 0x54d140: C string from operator new (CFlushMapAction.Other Map
// Name). Derived: 0x601c30 (constructor 0x58b840, no clone; CSwappedLayerFilenameMapping temp files).
struct CTextFieldDesc : CFieldDesc {
    const char *default_value;   // 0x28
    DWORD setter;                // 0x2c

    CTextFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                  const char *default_, DWORD setter_);   // 0x54d140
    CFieldDesc *THISCALL Clone();                                           // 0x54d180
    DWORD THISCALL CopyValue(BYTE *dst, BYTE *src, const CFieldDesc *from);   // 0x54d1d0
    DWORD THISCALL FreeValue(BYTE *object);                                // 0x54d270
    DWORD THISCALL SetDefault(BYTE *object);                               // 0x54d2a0
    CTextFieldDesc *THISCALL CtorTempFile(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                          DWORD unused5, DWORD unused6);   // 0x58b840 (the last two unused)
    DWORD THISCALL FreeTempFile(BYTE *object);                             // 0x58b8a0 (slot 10)
};
static_assert(sizeof(CTextFieldDesc) == 0x30);

// 0x5fd440 (constructors 0x54d480, 0x54d590) and 0x5fd66c (0x54dc70), 0x40 bytes: int with range,
// scale 1.0 and a converter object (function-local static)
struct CIntFieldDesc : CFieldDesc {
    DWORD min, max;          // 0x28, 0x2c
    DWORD scale;             // 0x30 float 1.0
    DWORD default_value;     // 0x34
    DWORD setter;            // 0x38
    DWORD *converter;        // 0x3c

    CIntFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                 DWORD default_, DWORD setter_, DWORD min_, DWORD max_);   // 0x54d480
    CIntFieldDesc *THISCALL CtorUnits(const char *name, const char *units, DWORD offset, DWORD u14, DWORD unused,
                                      DWORD u18, DWORD default_, DWORD setter_, DWORD min_, DWORD max_);   // 0x54d590
    CIntFieldDesc *THISCALL Ctor2(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                  DWORD default_, DWORD setter_, DWORD min_, DWORD max_);   // 0x54dc70
    CFieldDesc *THISCALL Clone();     // 0x54d510
    CFieldDesc *THISCALL Clone2();    // 0x54dd00
    DWORD THISCALL SetDefault(BYTE *object);   // 0x54dd80 and 0x54ee50 (the same code; enum ints, floats)
};
static_assert(sizeof(CIntFieldDesc) == 0x40);

// 0x5fd518, 0x44 bytes, constructors 0x54d6e0 and 0x54d7f0: int with a list of names
// (CInventory.Item In Focus). Derived: 0x5fcc34 (constructor 0x53f200; CLandscapeAI.Target Elevation),
// whose clone is the parent's and so returns the parent's vtable.
struct CEnumIntFieldDesc : CFieldDesc {
    DWORD min, max;           // 0x28, 0x2c (equal in the constructor: 0/-1 resp. 0/-2)
    DWORD scale;              // 0x30 float 1.0
    DWORD default_value;      // 0x34
    DWORD *converter;         // 0x38
    DWORD setter;             // 0x3c
    FieldNameList *names;     // 0x40

    CEnumIntFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                     DWORD default_, DWORD setter_, DWORD min_, DWORD max_);   // 0x54d6e0
    CEnumIntFieldDesc *THISCALL CtorUnits(const char *name, const char *units, DWORD offset, DWORD u14,
                                          DWORD unused, DWORD u18, DWORD default_, DWORD setter_, DWORD min_,
                                          DWORD max_);                                         // 0x54d7f0
    CEnumIntFieldDesc *THISCALL CopyCtor(const CEnumIntFieldDesc *other);                      // 0x54d8a0
    CFieldDesc *THISCALL Clone();                                                              // 0x53f240
    void THISCALL Dtor();                                                                      // 0x54da30
    CEnumIntFieldDesc *THISCALL DeletingDtor(DWORD flags);                                     // 0x54d790
    CEnumIntFieldDesc *THISCALL DeletingDtorElevation(DWORD flags);                            // 0x53f260
    CEnumIntFieldDesc *THISCALL CtorElevation(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                              DWORD default_, DWORD setter_);                  // 0x53f200
};
static_assert(sizeof(CEnumIntFieldDesc) == 0x44);

// 0x5fd738, 0x30 bytes, constructor 0x54ddf0: byte (CTriggerAI.Trigger Only Once)
struct CByteFieldDesc : CFieldDesc {
    DWORD setter;          // 0x28
    BYTE default_value;    // 0x2c
    BYTE pad2d[3];

    CByteFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                  DWORD default_, DWORD setter_);   // 0x54ddf0
    CFieldDesc *THISCALL Clone();                                     // 0x54de30
    DWORD THISCALL CopyValue(BYTE *dst, BYTE *src, const CFieldDesc *from);   // 0x54de70
    DWORD THISCALL SetDefault(BYTE *object);                               // 0x54def0
};
static_assert(sizeof(CByteFieldDesc) == 0x30);

// 0x5fd7f4, 0x34 bytes, constructor 0x54dfc0: one bit of a dword (CEntityAnimated.Facing Backwards)
struct CBitFieldDesc : CFieldDesc {
    BYTE default_value;   // 0x28
    BYTE pad29[3];
    DWORD mask;           // 0x2c
    DWORD f30;            // 0x30

    CBitFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD mask_, DWORD u18,
                                 DWORD default_, DWORD f30_);   // 0x54dfc0
    CFieldDesc *THISCALL Clone();                                 // 0x54e010
    BYTE *THISCALL CopyValue(BYTE *dst, BYTE *src, const CBitFieldDesc *from);   // 0x54e050
    BYTE *THISCALL SetDefault(BYTE *object);                                   // 0x54e0c0
};
static_assert(sizeof(CBitFieldDesc) == 0x34);

// 0x5fdb68, 0x44 bytes, constructors 0x54ec40 and 0x54ecd0: float with range, default, setter,
// converter and an optional list of names (CSpawnPointAI.Facing Angle)
struct CFloatFieldDesc : CFieldDesc {
    DWORD min, max, step;     // 0x28-0x30
    DWORD default_value;      // 0x34
    DWORD setter;             // 0x38
    DWORD *converter;         // 0x3c
    FieldNameList *names;     // 0x40

    CFloatFieldDesc *THISCALL CtorUnits(const char *name, const char *units, DWORD offset, DWORD u14,
                                        DWORD unused, DWORD u18, DWORD default_, DWORD setter_, DWORD min_,
                                        DWORD max_, DWORD step_);   // 0x54ec40
    CFloatFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                   DWORD default_, DWORD setter_, DWORD min_, DWORD max_, DWORD step_);   // 0x54ecd0
    CFieldDesc *THISCALL Clone();                                // 0x54e890
    CFloatFieldDesc *THISCALL DeletingDtor(DWORD flags);         // 0x54f760
};
static_assert(sizeof(CFloatFieldDesc) == 0x44);

// 0x5fdca4, 0x54 bytes, constructor 0x54f060: double (CLayerHolder.Simulation Running Time)
struct CDoubleFieldDesc : CFieldDesc {
    DWORD min[2], max[2], step[2];   // 0x28, 0x30, 0x38
    DWORD default_value[2];          // 0x40
    DWORD setter;                    // 0x48
    DWORD *converter;                // 0x4c
    FieldNameList *names;            // 0x50

    CDoubleFieldDesc *THISCALL Ctor(const char *name, DWORD offset, DWORD u14, DWORD unused, DWORD u18,
                                    DWORD default_lo, DWORD default_hi, DWORD setter_, DWORD min_lo, DWORD min_hi,
                                    DWORD max_lo, DWORD max_hi, DWORD step_lo, DWORD step_hi);   // 0x54f060
    CFieldDesc *THISCALL Clone();                                   // 0x54ea30
    CDoubleFieldDesc *THISCALL DeletingDtor(DWORD flags);           // 0x54f7a0
    DWORD THISCALL CopyValue(BYTE *dst, BYTE *src, const CFieldDesc *from);   // 0x54ec00
    BYTE *THISCALL SetDefault(BYTE *object);                               // 0x54f110
};
static_assert(sizeof(CDoubleFieldDesc) == 0x54);

// Kinds without own members and the shared slots
struct CFieldKinds {
    static CFieldDesc *THISCALL GenericDeletingDtor(CFieldDesc *self, DWORD flags);   // 0x54e0a0
    static DWORD THISCALL CopyDword(CFieldDesc *self, BYTE *dst, BYTE *src, const CFieldDesc *from);    // 0x41d7b0, 0x4186c0
    static DWORD THISCALL ClearDword(CFieldDesc *self, BYTE *object);                                    // 0x58b880
};

// Vtables
constexpr DWORD VT_FLAGS = 0x5ec684, VT_STRING = 0x5ec91c, VT_LIST = 0x5eca78, VT_AI = 0x5ecb38,
                VT_OBJECT = 0x5ecbf4, VT_OBJECT_ARRAY = 0x5ecd6c, VT_COLOR = 0x5efe30, VT_DYNAMIC = 0x5f17ec,
                VT_POINT = 0x5f46fc, VT_IMAGE = 0x5f4ce8, VT_SEQUENCE = 0x5faf10, VT_OBJECT_REF = 0x5fb840,
                VT_EMBEDDED_VALUE = 0x5fbc9c, VT_ELEVATION = 0x5fcc34, VT_INT_POINT = 0x5fd1d0,
                VT_POLYGON = 0x5fd2a0, VT_TEXT = 0x5fd374, VT_INT = 0x5fd440, VT_ENUM_INT = 0x5fd518,
                VT_INT2 = 0x5fd66c, VT_BYTE = 0x5fd738, VT_BIT = 0x5fd7f4, VT_RESOURCE = 0x5fd8b0,
                VT_TYPED_RESOURCE = 0x5fd970, VT_EMBEDDED = 0x5fda30, VT_FLOAT = 0x5fdb68, VT_DOUBLE = 0x5fdca4,
                VT_TEMP_FILE = 0x601c30;

}  // namespace zax
