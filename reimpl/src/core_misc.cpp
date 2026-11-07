// The remaining small functions of the object system (see core_misc.h).
#include "core_misc.h"

namespace zax {

namespace {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD,
                                            void *, void *, void *);
CClassInfo *obj_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x4163b0)(); }
void *get_dynamic(void *o, void *d) { return fn<void *(THISCALL *)(void *, void *)>(0x417320)(o, d); }
void *get_or_add_dynamic(void *o, void *d) { return fn<void *(THISCALL *)(void *, void *)>(0x417340)(o, d); }
DWORD crt_delete(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }
DWORD pool_delete(void *p, DWORD n) { return fn<DWORD(__fastcall *)(void *, DWORD)>(0x54d130)(p, n); }
DWORD aliases_dtor(void *a) { return fn<DWORD(THISCALL *)(void *)>(0x55d0c0)(a); }

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE &by(void *p, DWORD off) { return static_cast<BYTE *>(p)[off]; }
bool texts_differ(const char *a, const char *b)
{
    for (;; a++, b++) {
        if (*a != *b)
            return true;
        if (!*a)
            return false;
    }
}

constexpr DWORD CLASS_INFO_BASE_VTABLE = 0x5ec740, CHILDREN_VTABLE = 0x5ec7c0, STRING_FIELD_VTABLE = 0x5ec91c;

// Class getter whose parent getter is inlined: the class's block is taken first, the parent's (when its
// variable is still empty) after it; nothing is registered when the first block cannot be had
struct ClassSpec {
    GlobKey var, name;
    float version;
    DWORD size, abstract_;
    CodeKey create, create_array, register_fields;
};
void register_class(void *p, const ClassSpec &c, CClassInfo *parent)
{
    fn<RegisterFn>(0x415780)(p, glob<CClassInfo **>(c.var), glob<const char *>(c.name), c.version, parent, c.size, c.abstract_,
                             code<void *>(c.create), code<void *>(c.create_array), code<void *>(c.register_fields));
}
CClassInfo *getter(const ClassSpec &c)
{
    CClassInfo *&v = *glob<CClassInfo **>(c.var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            register_class(p, c, obj_class_info());
    return v;
}
CClassInfo *getter_with_parent(const ClassSpec &c, const ClassSpec &parent)
{
    CClassInfo *&v = *glob<CClassInfo **>(c.var);
    if (v)
        return v;
    if (void *p = pool_alloc(sizeof(CClassInfo))) {
        CClassInfo *&pv = *glob<CClassInfo **>(parent.var);
        if (!pv)
            if (void *q = pool_alloc(sizeof(CClassInfo)))
                register_class(q, parent, obj_class_info());
        register_class(p, c, pv);
    }
    return v;
}

// The abstract parents of the quest statistics (Create 0x46b760, CreateArray 0x52dc80, no own fields)
constexpr ClassSpec QUEST_STAT = {0x712324, 0x61f018 /* "CQuestStatData" */, 0.0f, 8, 1, 0x46b760, 0x52dc80, 0x58ea40};
constexpr ClassSpec QUEST_PAR_SCORE = {0x712320, 0x61f000 /* "CQuestStatParScoreData" */, 0.0f, 8, 1, 0x46b760, 0x52dc80, 0x58ea40};

struct QuestClass {
    ClassSpec spec;
    const ClassSpec *parent;
};
constexpr QuestClass QUEST_CLASSES[] = {
    {{0x712338, 0x61f3f8 /* "CQuestCounterData" */, 0.0f, 0x14, 0, 0x554520, 0x554560, 0x554680}, &QUEST_STAT},
    {{0x712334, 0x61f398 /* "CQuestCounterParScore" */, 0.0f, 0xc, 0, 0x554170, 0x5541b0, 0x5542d0}, &QUEST_PAR_SCORE},
    {{0x712348, 0x61f69c /* "CQuestFloatAccumulatorData" */, 0.0f, 0x14, 0, 0x556380, 0x5563c0, 0x5564e0}, &QUEST_STAT},
    {{0x712344, 0x61f67c /* "CQuestFloatAccumulatorParScore" */, 0.0f, 0xc, 0, 0x556000, 0x556040, 0x556160}, &QUEST_PAR_SCORE},
    {{0x712354, 0x61f700 /* "CQuestTimeData" */, 0.0f, 0x10, 0, 0x557a90, 0x557ad0, 0x557c80}, &QUEST_STAT},
    {{0x712350, 0x61f6d4 /* "CQuestTimeParScore" */, 0.0f, 0xc, 0, 0x557710, 0x557750, 0x557870}, &QUEST_PAR_SCORE},
    {{0x712368, 0x61f8c4 /* "CQuestPercentageData" */, 0.0f, 0x20, 0, 0x559690, 0x5596d0, 0x559880}, &QUEST_STAT},
    {{0x712364, 0x61f7dc /* "CQuestPercentageParScore" */, 0.0f, 0x10, 0, 0x5591b0, 0x5591f0, 0x559300}, &QUEST_PAR_SCORE},
};

constexpr ClassSpec ITEM_TYPE_DATA = {0x62e9fc, 0x60b5fc /* "CItemTypeData" */, 0.0f, 0x18, 0, 0x424190, 0x4241d0, 0x424310};
constexpr ClassSpec MULTIPLAYER_GAME_DATA = {0x6d001c, 0x61bef0 /* "CMultiPlayerGameData" */, 0.0f, 0x18, 0, 0x51a040, 0x51a070, 0x51a160};
constexpr ClassSpec SPHERE_OF_ZIN_DATA = {0x713818, 0x622b24 /* "CSphereOfZinData" */, 0.0f, 0x18, 0, 0x584210, 0x584260, 0x584430};

}  // namespace

// ---- CClassInfo ---------------------------------------------------------------------------------------

// 0x4152a0: the fields deleted (slot 0 with 1; the count read each time) and their array freed, the extra
// list deleted, the children's pool array freed; the byte 0x6bdc90 set first
DWORD THISCALL CClassInfoDtors::BaseDtor(CClassInfo *self)
{
    self->vftable = vt(CLASS_INFO_BASE_VTABLE);
    *glob<BYTE *>(EXTRA_LIST_DELETING) = 1;
    for (DWORD i = 0; i < self->num_fields; i++)
        if (CFieldDesc *f = self->fields[i]) {
            vcall<void *>(f, 0, 1u);
            self->fields[i] = nullptr;
        }
    if (self->fields) {
        heap_free(self->fields);
        self->fields = nullptr;
    }
    if (self->extra) {
        vcall<void *>(self->extra, 0, 1u);
        self->extra = nullptr;
    }
    self->children.vftable = vt(CHILDREN_VTABLE);
    if (self->children.data) {
        pool_get_free(self->children.data, self->children.capacity * 4);
        self->children.data = nullptr;
        self->children.capacity = 0;
    }
    self->children.count = 0;
    self->vftable = vt(NAMED_LIST_VTABLE);
    return 0;
}

// 0x415270
CClassInfo *THISCALL CClassInfoDtors::BaseDeletingDtor(CClassInfo *self, DWORD flags)
{
    BaseDtor(self);
    if (flags & 1)
        pool_delete(self, 0x40);
    return self;
}

// 0x415820
CClassInfo *THISCALL CClassInfoDtors::DeletingDtor(CClassInfo *self, DWORD flags)
{
    BaseDtor(self);
    if (flags & 1)
        pool_delete(self, sizeof(CClassInfo));
    return self;
}

// 0x4153b0. eax: ReleaseBuffer's.
DWORD THISCALL CClassInfoDtors::DisplayName(CClassInfo *self, ZString *out, const char *suffix)
{
    auto short_name = [self]() {
        const char *s = self->name;
        if (s[0] == 'C' && crt_isupper(s[1]))
            s++;
        return s;
    };
    const char *s = short_name();
    DWORD n = 0;
    while (s[n])
        n++;
    char *buf = out->GetBuffer(n + 1);
    s = short_name();
    n = 0;
    while (s[n])
        n++;
    memcpy(buf, s, n + 1);
    if (suffix && texts_differ(suffix, buf)) {
        DWORD lb = 0, ls = 0;
        while (buf[lb])
            lb++;
        while (suffix[ls])
            ls++;
        if (lb >= ls && crt_stricmp(buf + lb - ls, suffix) == 0)
            buf[lb - ls] = 0;
    }
    return fn<DWORD(THISCALL *)(ZString *, int)>(0x4e16d0)(out, -1);
}

// 0x417690
void *THISCALL CClassInfoDtors::StaticRegister(void *self, CClassInfo *(__cdecl *get)())
{
    CClassInfo *cls = get();
    void *registry = fn<void *(__cdecl *)()>(0x415d20)();
    fn<bool(THISCALL *)(void *, CClassInfo *, DWORD)>(0x415a20)(registry, cls, 0);
    return self;
}

// ---- Field descriptor setters -------------------------------------------------------------------------

// 0x419020, 0x419030, 0x419060
ZString *THISCALL CFieldDescSetters::SetHelp(CFieldDesc *d, const char *s) { return d->help.AssignText(s); }
ZString *THISCALL CFieldDescSetters::SetAltName(CFieldDesc *d, const char *s) { return d->alt_name.AssignText(s); }
ZString *THISCALL CFieldDescSetters::SetUnits(CFieldDesc *d, const char *s) { return d->units.AssignText(s); }
// 0x419040
void THISCALL CFieldDescSetters::SetInherited(CFieldDesc *d) { d->inherited = 1; }

// ---- Field values -------------------------------------------------------------------------------------

// 0x415ef0
CStringFieldDesc *THISCALL CFieldValues::StringCopyCtor(CStringFieldDesc *self, const CStringFieldDesc *other)
{
    fn<CFieldDesc *(THISCALL *)(CFieldDesc *, const CFieldDesc *)>(0x418e10)(self, other);
    self->default_value.CopyCtor(&other->default_value);
    self->vftable = vt(STRING_FIELD_VTABLE);
    self->f28 = other->f28;
    self->setter = other->setter;
    return self;
}

// 0x415f50: the value in the object's dynamic properties, else the default
ZString *THISCALL CFieldValues::StringValue(CStringFieldDesc *self, void *object)
{
    if (BYTE *p = static_cast<BYTE *>(get_dynamic(object, self)))
        return reinterpret_cast<ZString *>(p + self->offset);
    return &self->default_value;
}

// 0x415f80: the text as a ZString handed by value to the kind's setter 0x4161d0 with the dynamic value.
// eax: the setter's... then the ZString destructor's (undefined).
DWORD THISCALL CFieldValues::StringSetText(CStringFieldDesc *self, void *object, const char *s)
{
    ZString text;
    text.CtorText(s);
    void *p = get_or_add_dynamic(object, self);
    ZString copy;   // owned by the callee
    copy.CopyCtor(&text);
    fn<void(THISCALL *)(CStringFieldDesc *, void *, ZString)>(0x4161d0)(self, p, copy);
    text.Dtor();
    return 0;
}

// 0x54d630
DWORD THISCALL CFieldValues::IntGet(CIntFieldDesc *self, void *object)
{
    if (BYTE *p = static_cast<BYTE *>(get_dynamic(object, self)))
        return dw(p, self->offset);
    return self->default_value;
}

// 0x54d660. eax: the setter's, else the dynamic block.
DWORD THISCALL CFieldValues::IntSet(CIntFieldDesc *self, void *object, DWORD v)
{
    BYTE *p = static_cast<BYTE *>(get_or_add_dynamic(object, self));
    if (self->setter)
        return fn_dyn<DWORD(THISCALL *)(void *, DWORD)>(self->setter)(p, v);
    dw(p, self->offset) = v;
    return (DWORD)(uintptr_t)p;
}

// 0x54ede0 (fld: a signalling NaN comes back quiet)
float THISCALL CFieldValues::FloatGet(CFloatFieldDesc *self, void *object)
{
    if (BYTE *p = static_cast<BYTE *>(get_dynamic(object, self)))
        return *reinterpret_cast<float *>(p + self->offset);
    return *reinterpret_cast<float *>(&self->default_value);
}

// 0x54ee10: the setter's st(0) is dropped; without a setter the value goes through fld/fstp. eax: the
// setter's, else the dynamic block.
DWORD THISCALL CFieldValues::FloatSet(CFloatFieldDesc *self, void *object, DWORD v)
{
    BYTE *p = static_cast<BYTE *>(get_or_add_dynamic(object, self));
    if (self->setter) {
        DWORD r;
        __asm__ volatile("pushl %2\n\tmovl %1, %%ecx\n\tcalll *%3\n\tfstp %%st(0)"
                         : "=a"(r)
                         : "r"(p), "r"(v), "r"(self->setter)
                         : "ecx", "edx", "memory", "st");
        return r;
    }
    float *dst = reinterpret_cast<float *>(p + self->offset);
    __asm__ volatile("flds %1\n\tfstps %0" : "=m"(*dst) : "m"(v));
    return (DWORD)(uintptr_t)p;
}

// 0x54d440: n dwords from operator new, cleared (without a null check)
DWORD *THISCALL CFieldValues::NewZeroedArray(void *, DWORD n, DWORD, DWORD)
{
    DWORD *p = static_cast<DWORD *>(heap_alloc(n * 4));
    for (DWORD i = 0; i < n; i++)
        p[i] = 0;
    return p;
}

// ---- Class getters --------------------------------------------------------------------------------------

// 0x4242b0
CClassInfo *__cdecl CItemTypeData_GetClassInfo() { return getter(ITEM_TYPE_DATA); }
// 0x51bf50
CClassInfo *__cdecl CMultiPlayerGameData_GetClassInfo() { return getter(MULTIPLAYER_GAME_DATA); }

// 0x5557f0, 0x555890, 0x557470, 0x557510, 0x557bd0, 0x5588d0, 0x5597d0, 0x55aa30
template <DWORD VAR> CClassInfo *__cdecl Quest_GetClassInfo()
{
    for (const QuestClass &q : QUEST_CLASSES)
        if (key_address(q.spec.var) == VAR)
            return getter_with_parent(q.spec, *q.parent);
    return nullptr;
}

// 0x585380: the object when it is a CSphereOfZinData (its getter inlined), else 0
void *__fastcall SphereOfZinData_Cast(void *object)
{
    if (!object)
        return nullptr;
    CClassInfo *cls = getter(SPHERE_OF_ZIN_DATA);
    return static_cast<BYTE>(fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(object, cls)) ? object : nullptr;
}

// ---- Destructors and the constructor ---------------------------------------------------------------------

// 0x424230: flags bit 1 an array (count in front, elements of 0x18 from the last, operator delete),
// otherwise one object (pool). eax: the block or the object.
void *THISCALL CMiscDtors::ItemTypeDataDeletingDtor(void *self, DWORD flags)
{
    auto destroy = [](BYTE *o) {
        dw(o, 0) = vt(0x5ee064);
        fn<void(THISCALL *)(void *)>(0x4173f0)(o);
        fn<void(THISCALL *)(void *)>(0x4166d0)(o);
    };
    BYTE *o = static_cast<BYTE *>(self);
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(o) - 1;
        for (DWORD i = *block; (int)i > 0; i--)
            destroy(o + (i - 1) * 0x18);
        if (flags & 1)
            crt_delete(block);
        return block;
    }
    destroy(o);
    if (flags & 1)
        pool_delete(o, 0x18);
    return o;
}

// 0x5853f0: {vtable 0x601374, data, count, capacity}; the data is a new[] array deleted through slot 1 of
// its first element with flags 3
void *THISCALL CMiscDtors::TriggerArrayDeletingDtor(void *self, DWORD flags)
{
    dw(self, 0) = vt(0x601374);
    if (void *data = at<void *>(dw(self, 4))) {
        vcall<void *>(data, 1, 3u);
        dw(self, 4) = 0;
        dw(self, 0xc) = 0;
    }
    dw(self, 8) = 0;
    if (flags & 1)
        crt_delete(self);
    return self;
}

// 0x4d4cc0
void *THISCALL CMiscDtors::HueRemapHolderCtor(void *self)
{
    fn<void(THISCALL *)(void *)>(0x416610)(self);
    dw(self, 0) = vt(0x5f7c10);
    fn<void(THISCALL *)(void *)>(0x4173c0)(self);
    return self;
}

// 0x41e830
void *THISCALL CMiscDtors::ResourceListDeletingDtor(void *self, DWORD flags)
{
    dw(self, 0) = vt(0x5ed564);
    aliases_dtor(static_cast<BYTE *>(self) + 0x1c);
    dw(self, 0) = vt(NAMED_LIST_VTABLE);
    if (flags & 1)
        pool_delete(self, 0x30);
    return self;
}

// 0x41e940: like the definition lists' destructors (0x46a890), but with the base vtable 0x5ed5f4 and the
// loaded objects deleted through slot 0
void *THISCALL CMiscDtors::DefinitionListBaseDeletingDtor(void *self, DWORD flags)
{
    dw(self, 0) = vt(0x5ed5f4);
    if (BYTE *data = at<BYTE *>(dw(self, 0x30))) {
        DWORD *block = reinterpret_cast<DWORD *>(data) - 1;
        for (DWORD i = *block; (int)i > 0; i--)
            if (void *o = at<void *>(dw(data, (i - 1) * 0x80 + 0x78))) {
                vcall<void *>(o, 0, 1u);
                dw(data, (i - 1) * 0x80 + 0x78) = 0;
            }
        crt_delete(block);
        dw(self, 0x30) = 0;
    }
    dw(self, 0x34) = 0;
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(self, 0, 0, 0, nullptr, 0);
    if (void *m = at<void *>(dw(self, 0x13c))) {
        vcall<void *>(m, 7, 1u);
        dw(self, 0x13c) = 0;
    }
    dw(self, 0) = vt(0x5ed564);
    aliases_dtor(static_cast<BYTE *>(self) + 0x1c);
    dw(self, 0) = vt(NAMED_LIST_VTABLE);
    if (flags & 1)
        pool_delete(self, 0x140);
    return self;
}

// 0x41ec30: the loaded object of entry i deleted (slot 0 with 1) and its flags cleared. eax: the records.
DWORD THISCALL CMiscDtors::DefinitionListBaseUnload(void *self, DWORD i)
{
    BYTE *data = at<BYTE *>(dw(self, 0x30));
    if (!data)
        return 0;
    BYTE *r = data + i * 0x80;
    if (void *o = at<void *>(dw(r, 0x78))) {
        vcall<void *>(o, 0, 1u);
        BYTE *d = at<BYTE *>(dw(self, 0x30)) + i * 0x80;   // re-read after the call, as the original does
        dw(d, 0x78) = 0;
        d = at<BYTE *>(dw(self, 0x30)) + i * 0x80;
        by(d, 0x7e) = 0;
        d = at<BYTE *>(dw(self, 0x30)) + i * 0x80;
        by(d, 0x7d) = 0;
        d = at<BYTE *>(dw(self, 0x30)) + i * 0x80;
        by(d, 0x7f) = 0;
        d = at<BYTE *>(dw(self, 0x30)) + i * 0x80;
        by(d, 0x7c) = 0;
    }
    return dw(self, 0x30);
}

// 0x47e540
DWORD THISCALL CMiscDtors::StringTripleDtor(ZString *self)
{
    self[3].Dtor();
    self[2].Dtor();
    self[0].Dtor();
    return 0;
}

ZAX_HOOKS(core_misc)
{
    using M = CMiscDtors;
    const Hook list[] = {
        {0x415270, (void *)&CClassInfoDtors::BaseDeletingDtor, "CClassInfo::BaseDeletingDtor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x28, 0x00, 0x00, 0x00}, 4},
        {0x4152a0, (void *)&CClassInfoDtors::BaseDtor, "CClassInfo::BaseDtor", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x33, 0xdb, 0x33}},
        {0x4153b0, (void *)&CClassInfoDtors::DisplayName, "CClassInfo::DisplayName", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7e}},
        {0x415820, (void *)&CClassInfoDtors::DeletingDtor, "CClassInfo::DeletingDtor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x78, 0xfa, 0xff, 0xff}, 4},
        {0x415ef0, (void *)&CFieldValues::StringCopyCtor, "CStringFieldDesc::CopyCtor", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x415f50, (void *)&CFieldValues::StringValue, "CStringFieldDesc::Value", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x56}},
        {0x415f80, (void *)&CFieldValues::StringSetText, "CStringFieldDesc::SetText", 5, {0x8b, 0x44, 0x24, 0x08, 0x56, 0x8b, 0xf1, 0x57}},
        {0x417690, (void *)&CClassInfoDtors::StaticRegister, "ClassInfo_StaticRegister", 7, {0x56, 0x8b, 0xf1, 0xff, 0x54, 0x24, 0x08, 0x6a}},
        {0x419020, (void *)&CFieldDescSetters::SetHelp, "FieldDesc_SetHelp", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xc1, 0x08, 0x50}},
        {0x419030, (void *)&CFieldDescSetters::SetAltName, "FieldDesc_SetAltName", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xc1, 0x1c, 0x50}},
        {0x419040, (void *)&CFieldDescSetters::SetInherited, "FieldDesc_SetInherited", 5, {0xc6, 0x41, 0x24, 0x01, 0xc3, 0x90, 0x90, 0x90}},
        {0x419060, (void *)&CFieldDescSetters::SetUnits, "FieldDesc_SetUnits", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xc1, 0x20, 0x50}},
        {0x41e830, (void *)&M::ResourceListDeletingDtor, "ResourceList::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x1c, 0xc7, 0x06}},
        {0x41e940, (void *)&M::DefinitionListBaseDeletingDtor, "DefinitionListBase::DeletingDtor", 6, {0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b, 0x4f}},
        {0x41ec30, (void *)&M::DefinitionListBaseUnload, "DefinitionListBase::Unload", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0x33, 0xdb, 0x8b}},
        {0x424230, (void *)&M::ItemTypeDataDeletingDtor, "CItemTypeData::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4242b0, (void *)&CItemTypeData_GetClassInfo, "CItemTypeData::GetClassInfo", 5, {0xa1, 0xfc, 0xe9, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x47e540, (void *)&M::StringTripleDtor, "StringTriple_Dtor", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x0c, 0xe8, 0x85}},
        {0x4d4cc0, (void *)&M::HueRemapHolderCtor, "CHueRemapHolder::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x48, 0x19, 0xf4, 0xff}, 4},
        {0x51bf50, (void *)&CMultiPlayerGameData_GetClassInfo, "CMultiPlayerGameData::GetClassInfo", 5, {0xa1, 0x1c, 0x00, 0x6d, 0x00, 0x56, 0x85, 0xc0}},
        {0x54d440, (void *)&CFieldValues::NewZeroedArray, "NewZeroedArray", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x8d, 0x04, 0xb5}},
        {0x54d630, (void *)&CFieldValues::IntGet, "CIntFieldDesc::Get", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x56}},
        {0x54d660, (void *)&CFieldValues::IntSet, "CIntFieldDesc::Set", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x56}},
        {0x54ede0, (void *)&CFieldValues::FloatGet, "CFloatFieldDesc::Get", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x56}},
        {0x54ee10, (void *)&CFieldValues::FloatSet, "CFloatFieldDesc::Set", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x56}},
        {0x5557f0, (void *)&Quest_GetClassInfo<0x712338>, "CQuestCounterData::GetClassInfo", 5, {0xa1, 0x38, 0x23, 0x71, 0x00, 0x57, 0x85, 0xc0}},
        {0x555890, (void *)&Quest_GetClassInfo<0x712334>, "CQuestCounterParScore::GetClassInfo", 5, {0xa1, 0x34, 0x23, 0x71, 0x00, 0x57, 0x85, 0xc0}},
        {0x557470, (void *)&Quest_GetClassInfo<0x712348>, "CQuestFloatAccumulatorData::GetClassInfo", 5, {0xa1, 0x48, 0x23, 0x71, 0x00, 0x57, 0x85, 0xc0}},
        {0x557510, (void *)&Quest_GetClassInfo<0x712344>, "CQuestFloatAccumulatorParScore::GetClassInfo", 5, {0xa1, 0x44, 0x23, 0x71, 0x00, 0x57, 0x85, 0xc0}},
        {0x557bd0, (void *)&Quest_GetClassInfo<0x712354>, "CQuestTimeData::GetClassInfo", 5, {0xa1, 0x54, 0x23, 0x71, 0x00, 0x57, 0x85, 0xc0}},
        {0x5588d0, (void *)&Quest_GetClassInfo<0x712350>, "CQuestTimeParScore::GetClassInfo", 5, {0xa1, 0x50, 0x23, 0x71, 0x00, 0x57, 0x85, 0xc0}},
        {0x5597d0, (void *)&Quest_GetClassInfo<0x712368>, "CQuestPercentageData::GetClassInfo", 5, {0xa1, 0x68, 0x23, 0x71, 0x00, 0x57, 0x85, 0xc0}},
        {0x55aa30, (void *)&Quest_GetClassInfo<0x712364>, "CQuestPercentageParScore::GetClassInfo", 5, {0xa1, 0x64, 0x23, 0x71, 0x00, 0x57, 0x85, 0xc0}},
        {0x585380, (void *)&SphereOfZinData_Cast, "SphereOfZinData_Cast", 5, {0x57, 0x8b, 0xf9, 0x85, 0xff, 0x74, 0x5c, 0xa1}},
        {0x5853f0, (void *)&M::TriggerArrayDeletingDtor, "SphereOfZinTriggerArray::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x04, 0xc7, 0x06}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
