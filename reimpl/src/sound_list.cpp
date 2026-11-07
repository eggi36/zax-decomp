// The sound list 0x57df90-0x57ebb5 (phase 4, sound): the named list "Sound List" (0x713708, class of
// the static 0x57df10) of the sound files of a folder, one CSoundResource per file. The action and AI
// fields "Sound" name its entries; it plays them (by name or index), gives copies of them to the
// ambient sounds, makes them resident and reloads them.
//
// Layout [confirmed: the constructor 0x57e070, Load 0x57e2f0]: a record list (RecordList, named_list.cpp)
// of 0x108-byte records {name (0x100), +0x100 "used" (precached), +0x101 a second flag, +0x104 the
// CSoundResource}: +0x14 the count, +0x1c the resource aliases (vtable 0x5ed558), +0x2c -1, +0x30 the
// folder (ZString, with a final '/'), +0x34 the records (operator new), +0x38 the name tree (0x47e560),
// +0x3c the path given to Load. Vtable 0x600c2c (named list slots of named_list.cpp with these): 0
// deleting destructor, 2 Find (.wav and .ogg tried for each other), 4 the list name, 10 loaded (the
// resource's flag 0x10), 12 precache, 17 play by index, 28 all "used" cleared, 29 "used", 30 resident,
// 31 reload, 32/33 the second flag, 37 "used" set.
#include "reimpl.h"
#include "exe_base.h"
#include "zstring.h"

namespace zax {

namespace {

template <typename T> inline T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
inline DWORD &dw(void *p, DWORD off) { return fl<DWORD>(p, off); }
inline BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
inline DWORD ptr_value(const void *p) { return (DWORD)(uintptr_t)p; }

constexpr DWORD VT_LIST = 0x600c2c, VT_ALIASES = 0x5ed558, VT_RECORDS = 0x5ed564, VT_NAMED = 0x5ec7cc;
constexpr DWORD RECORD = 0x108;
constexpr DWORD TEXT_EMPTY = 0x62e558, TEXT_WAV = 0x615658, TEXT_OGG = 0x615660;

BYTE *record(BYTE *l, DWORD i) { return bp(dw(l, 0x34)) + i * RECORD; }
BYTE *resource_of(BYTE *l, DWORD i) { return bp(dw(record(l, i), 0x104)); }
void *cache() { return zax::fn<void *(__cdecl *)()>(0x57aef0)(); }
void release(BYTE *r) { zax::fn<DWORD(THISCALL *)(void *, void *)>(0x57af60)(cache(), r); }
BYTE *new_resource() { return vcall<BYTE *>(cache(), 3); }
void res_set_f30(BYTE *r, DWORD v) { zax::fn<void(THISCALL *)(void *, DWORD)>(0x57bcf0)(r, v); }
DWORD res_make_resident(BYTE *r, DWORD a) { return zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x57b560)(r, a); }
void res_load(BYTE *r, const char *name, DWORD keep, DWORD mode, DWORD arg)
{
    zax::fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD)>(0x57b760)(r, name, keep, mode, arg);
}
DWORD records_set(BYTE *l, DWORD sorted, DWORD stride, DWORD count, void *data, DWORD a)
{
    return zax::fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(l, sorted, stride, count, data, a);
}
void zs_ctor(void *z) { zax::fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_dtor(void *z) { zax::fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
const char *zs_c_str(const void *z) { return zax::fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
void zs_assign_text(void *z, const char *s) { zax::fn<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, s); }
void zs_append_path_name(void *z, const char *name) { zax::fn<void(THISCALL *)(void *, const char *)>(0x4df650)(z, name); }

// Every record's resource released and the records freed
void release_records(BYTE *l)
{
    for (DWORD i = 0; i < dw(l, 0x14); i++)
        if (BYTE *r = resource_of(l, i)) {
            release(r);
            dw(record(l, i), 0x104) = 0;
        }
    if (void *d = bp(dw(l, 0x34))) {
        heap_free(d);
        dw(l, 0x34) = 0;
    }
}
// The destructor's levels below the vtable: the records, the name tree, the two strings, the aliases,
// the named list's vtable
void destroy(BYTE *l)
{
    dw(l, 0) = vt(VT_LIST);
    release_records(l);
    records_set(l, 1, 0, 0, nullptr, 0);
    if (BYTE *t = bp(dw(l, 0x38))) {
        vcall<void *>(t, 7, 1);
        dw(l, 0x38) = 0;
    }
    zs_dtor(l + 0x3c);
    zs_dtor(l + 0x30);
    dw(l, 0) = vt(VT_RECORDS);
    zax::fn<void(THISCALL *)(void *)>(0x55d0c0)(l + 0x1c);
    dw(l, 0) = vt(VT_NAMED);
}

// 0x57df90 (slot 4)
const char *THISCALL SoundList_Name(BYTE *) { return glob<const char *>(0x62222c /* "Sound List" */); }
// 0x57e070
BYTE *THISCALL SoundList_Ctor(BYTE *l)
{
    zax::fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x591c90)(l, 0, RECORD, 0, 0, 0);
    dw(l, 0x1c) = vt(VT_ALIASES);
    dw(l, 0x20) = 0;
    dw(l, 0x24) = 0;
    dw(l, 0x28) = 0;
    dw(l, 0x2c) = 0xffffffff;
    dw(l, 0) = vt(VT_RECORDS);
    zs_ctor(l + 0x30);
    zs_ctor(l + 0x3c);
    dw(l, 0x34) = 0;
    dw(l, 0x38) = 0;
    dw(l, 0) = vt(VT_LIST);
    return l;
}
// 0x57e0d0 (slot 0), 0x57e190 (the destructor)
BYTE *THISCALL SoundList_DeletingDtor(BYTE *l, DWORD flags)
{
    destroy(l);
    if (flags & 1)
        pool_free(l, 0x40);
    return l;
}
void THISCALL SoundList_Dtor(BYTE *l) { destroy(l); }
// 0x57e240: the records and their resources freed, the list empty
DWORD THISCALL SoundList_Clear(BYTE *l)
{
    release_records(l);
    return records_set(l, 1, 0, 0, nullptr, 0);
}
// 0x57e2c0 (i): the size of the record's resource (+8; 0 without one)
DWORD THISCALL SoundList_Size(BYTE *l, DWORD i)
{
    BYTE *r = resource_of(l, i);
    return r ? dw(r, 8) : 0;
}

// 0x57e2f0 (list name, folder, file mask, a second extension or null): the folder's sound files as the
// records (sorted by name, _stricmp), each with a new resource whose file name is kept (Resource_Load
// mode 1, nothing read); the list registered under its name, a new name tree; the count
DWORD THISCALL SoundList_Load(BYTE *l, const char *list_name, const char *folder, const char *mask, const char *extension)
{
    zax::fn<void(THISCALL *)(void *, const char *)>(0x4df010)(l + 0x3c, folder);
    void *reg = zax::fn<void *(__cdecl *)()>(0x4a35d0)();
    const DWORD index = zax::fn<DWORD(THISCALL *)(void *, const char *, void *, DWORD, DWORD)>(0x4a34d0)(reg, list_name, l, 0, 1);
    zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x523e20)(l, index);
    char path[0x104];
    lstrcpyA(path, folder);
    zax::fn<void(__fastcall *)(char *, const char *)>(0x4a4470)(path, glob<const char *>(TEXT_EMPTY));
    zs_assign_text(l + 0x30, path);
    release_records(l);
    records_set(l, 1, 0, 0, nullptr, 0);
    BYTE spec[0x2c];
    zax::fn<void *(THISCALL *)(void *)>(0x4a09f0)(spec);
    const char *dot = zax::fn<const char *(__cdecl *)(const char *, int)>(0x5d08c0)(mask, '.');
    zax::fn<DWORD(THISCALL *)(void *, const char *, const char *)>(0x4a0a60)(spec, path, dot);
    if (extension)
        zax::fn<DWORD(THISCALL *)(void *, const char *)>(0x4a0a30)(spec, extension);
    dw(l, 0x14) = zax::fn<DWORD(__fastcall *)(void *, DWORD)>(0x4a12d0)(spec, 0);
    BYTE *data = static_cast<BYTE *>(heap_alloc((dw(l, 0x14) + 1) * RECORD));
    dw(l, 0x34) = ptr_value(data);
    const DWORD n = zax::fn<DWORD(__fastcall *)(void *, void *, DWORD, DWORD, DWORD, DWORD)>(0x4a1150)(spec, data, 0, RECORD, dw(l, 0x14), 0);
    dw(l, 0x14) = n;
    memset(bp(dw(l, 0x34)) + n * RECORD, 0, RECORD);
    for (DWORD i = 0; i < dw(l, 0x14); i++) {
        fl<BYTE>(record(l, i), 0x100) = 0;
        fl<BYTE>(record(l, i), 0x101) = 0;
        zax::fn<DWORD(__fastcall *)(char *, const char *)>(0x587a00)(reinterpret_cast<char *>(record(l, i)), path);
        dw(record(l, i), 0x104) = ptr_value(new_resource());
        ZString z;
        zs_ctor(&z);
        zs_assign_text(&z, folder);
        zs_append_path_name(&z, reinterpret_cast<const char *>(record(l, i)));
        res_load(resource_of(l, i), zs_c_str(&z), 0, 1, 0);
        zs_dtor(&z);
    }
    if (dw(l, 0x14)) {
        zax::fn<void(__cdecl *)(void *, DWORD, DWORD, void *)>(0x5d097c)(bp(dw(l, 0x34)), dw(l, 0x14), RECORD, code<void *>(0x5d3640));
        records_set(l, 1, RECORD, dw(l, 0x14), bp(dw(l, 0x34)), 0);
    }
    void *tree = heap_alloc(0x34);
    if (tree)
        tree = zax::fn<void *(THISCALL *)(void *, void *)>(0x47e560)(tree, l);
    dw(l, 0x38) = ptr_value(tree);
    vcall<DWORD>(l, 27, tree);
    const DWORD count = dw(l, 0x14);
    for (int k = 4; k >= 0; k--)
        zs_dtor(spec + 0x18 + 4 * (DWORD)k);
    return count;
}

// 0x57e590: every resource made resident (its file read into memory)
void THISCALL SoundList_MakeAllResident(BYTE *l)
{
    for (DWORD i = 0; i < dw(l, 0x14); i++)
        res_make_resident(resource_of(l, i), 0);
}
// 0x57e5d0 (i, out): the record's path (folder + name), "" beyond the list
void THISCALL SoundList_PathOf(BYTE *l, DWORD i, void *out)
{
    if (i >= dw(l, 0x14)) {
        zs_assign_text(out, glob<const char *>(TEXT_EMPTY));
        return;
    }
    const char *name = reinterpret_cast<const char *>(record(l, i));
    BYTE *folder = l + 0x30;
    ZString z;
    const DWORD n = (DWORD)lstrlenA(name);
    const DWORD fn = zax::fn<DWORD(THISCALL *)(const void *)>(0x4e13b0)(folder);
    zax::fn<void *(THISCALL *)(void *, const char *, DWORD, const char *, DWORD)>(0x4def50)(&z, zs_c_str(folder), fn, name, n);
    zax::fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(out, &z);
    zs_dtor(&z);
}
// 0x57e650 (i): the record's resource with its +0x30 the index (null beyond the list)
BYTE *THISCALL SoundList_Get(BYTE *l, DWORD i)
{
    if (i >= dw(l, 0x14))
        return nullptr;
    res_set_f30(resource_of(l, i), i);
    return resource_of(l, i);
}
// The name with the other extension, looked up with the resource list's Find (0x523df0)
DWORD find_other(BYTE *l, const char *name, GlobKey ext)
{
    ZString z;
    zax::fn<ZString *(THISCALL *)(void *, const char *)>(0x4dee00)(&z, name);
    const DWORD n = zax::fn<DWORD(THISCALL *)(const void *)>(0x4e13b0)(&z);
    char *buf = zax::fn<char *(THISCALL *)(void *, DWORD)>(0x4e1590)(&z, n + 4);
    zax::fn<void(__fastcall *)(char *, const char *)>(0x4a3fb0)(buf, glob<const char *>(ext));
    zax::fn<void(THISCALL *)(void *, DWORD)>(0x4e16d0)(&z, 0xffffffff);
    const char *text = zs_c_str(&z);
    const DWORD r = zax::fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(l, text, -1);
    zs_dtor(&z);
    return r;
}
// 0x57e690 (slot 2: name, hint): slot 24 (the record lookup); a missing .wav is looked up as .ogg and a
// missing .ogg as .wav; at last slot 26 (the fail path)
DWORD THISCALL SoundList_Find(BYTE *l, const char *name, DWORD hint)
{
    const DWORD r = vcall<DWORD>(l, 24, name, hint);
    if (r != 0xffffffff)
        return r;
    DWORD again = 0xffffffff;
    if (!crt_stricmp(zax::fn<const char *(__fastcall *)(const char *)>(0x4a3f80)(name), glob<const char *>(TEXT_WAV)))
        again = find_other(l, name, TEXT_OGG);
    else if (!crt_stricmp(zax::fn<const char *(__fastcall *)(const char *)>(0x4a3f80)(name), glob<const char *>(TEXT_OGG)))
        again = find_other(l, name, TEXT_WAV);
    if (again != 0xffffffff)
        return again;
    return vcall<DWORD>(l, 26, name);
}
// 0x57e7b0 (name): precached (slot 12) and its second flag set (slot 33)
void THISCALL SoundList_PrecacheByName(BYTE *l, const char *name)
{
    const DWORD i = vcall<DWORD>(l, 2, name, -1);
    if (i == 0xffffffff)
        return;
    vcall<void>(l, 12, i);
    vcall<void>(l, 33, i, 1);
}
// 0x57e7f0 (name): precached
void THISCALL SoundList_PrecacheName(BYTE *l, const char *name)
{
    const DWORD i = vcall<DWORD>(l, 2, name, -1);
    if (i != 0xffffffff)
        vcall<void>(l, 12, i);
}
// The record's resource (index into +0x30) played (its slot 2 with 1)
BYTE *play(BYTE *l, DWORD i)
{
    BYTE *r = nullptr;
    if (i < dw(l, 0x14)) {
        res_set_f30(resource_of(l, i), i);
        r = resource_of(l, i);
    }
    if (r)
        vcall<DWORD>(r, 2, 1);
    return r;
}
// 0x57e820 (name): played by name (nothing for an empty list)
BYTE *THISCALL SoundList_PlayName(BYTE *l, const char *name)
{
    if (!dw(l, 0x14))
        return nullptr;
    return play(l, vcall<DWORD>(l, 2, name, -1));
}
// 0x57e890 (i): played by index
BYTE *THISCALL SoundList_PlayIndex(BYTE *l, DWORD i) { return play(l, i); }
// 0x57e8e0 (i): a new resource of the cache as a copy of the record's (its slot 1); null beyond the list
// or without a resource
BYTE *THISCALL SoundList_Copy(BYTE *l, DWORD i)
{
    if (i >= dw(l, 0x14))
        return nullptr;
    res_set_f30(resource_of(l, i), i);
    BYTE *r = resource_of(l, i);
    if (!r)
        return nullptr;
    BYTE *copy = new_resource();
    vcall<DWORD>(copy, 1, r);
    return copy;
}
// 0x57e940 (slot 17: i): played (beyond the list the exe calls through a null pointer)
DWORD THISCALL SoundList_Play(BYTE *l, DWORD i)
{
    BYTE *r = nullptr;
    if (i < dw(l, 0x14)) {
        res_set_f30(resource_of(l, i), i);
        r = resource_of(l, i);
    }
    return vcall<DWORD>(r, 2, 1);
}
// 0x57e990 (slot 12: i): resident, "used", +0x30 the index
void THISCALL SoundList_Precache(BYTE *l, DWORD i)
{
    res_make_resident(resource_of(l, i), 0);
    fl<BYTE>(record(l, i), 0x100) = 1;
    res_set_f30(resource_of(l, i), i);
}
// 0x57e9e0 (slot 28): every "used" cleared; eax the count
DWORD THISCALL SoundList_ClearUsed(BYTE *l)
{
    DWORD i = 0;
    for (; i < dw(l, 0x14); i++)
        fl<BYTE>(record(l, i), 0x100) = 0;
    return i;
}
// 0x57ea10, 0x57ea30, 0x57ea50, 0x57ea70 (slots 29, 37, 32, 33)
BYTE THISCALL SoundList_Used(BYTE *l, DWORD i) { return fl<BYTE>(record(l, i), 0x100); }
void THISCALL SoundList_SetUsed(BYTE *l, DWORD i) { fl<BYTE>(record(l, i), 0x100) = 1; }
BYTE THISCALL SoundList_Flag101(BYTE *l, DWORD i) { return fl<BYTE>(record(l, i), 0x101); }
void THISCALL SoundList_SetFlag101(BYTE *l, DWORD i, BYTE v) { fl<BYTE>(record(l, i), 0x101) = v; }
// 0x57ea90 (slot 10: i): the resource is loaded (its flag 0x10)
bool THISCALL SoundList_Loaded(BYTE *l, DWORD i)
{
    BYTE *r = resource_of(l, i);
    return r && ((dw(r, 0x14) >> 4) & 1);
}
// 0x57eac0 (slot 30: i)
DWORD THISCALL SoundList_MakeResident(BYTE *l, DWORD i) { return res_make_resident(resource_of(l, i), 0); }
// 0x57eae0 (slot 31: i): a loaded resource replaced by a new one of the same file (the flags cleared)
void THISCALL SoundList_Reload(BYTE *l, DWORD i)
{
    BYTE *rec = record(l, i);
    BYTE *r = bp(dw(rec, 0x104));
    if (!r || !((dw(r, 0x14) >> 4) & 1))
        return;
    fl<BYTE>(rec, 0x100) = 0;
    fl<BYTE>(record(l, i), 0x101) = 0;
    release(resource_of(l, i));
    dw(record(l, i), 0x104) = 0;
    dw(record(l, i), 0x104) = ptr_value(new_resource());
    ZString z;
    zs_ctor(&z);
    zs_assign_text(&z, zs_c_str(l + 0x3c));
    zs_append_path_name(&z, reinterpret_cast<const char *>(record(l, i)));
    res_load(resource_of(l, i), zs_c_str(&z), 0, 1, 0);
    zs_dtor(&z);
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(sound_list)
{
    const Hook list[] = {
#include "sound_list_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
