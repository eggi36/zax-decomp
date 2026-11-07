// The cached resource of Zax.exe (0x57b520-0x57bd30): an object that the resource cache (the global
// manager *0x713688) keeps in a linked list while it is loaded, with a load cost, a priority and an
// optional file name that can be made resident in memory ("~CFileBuf 0x" path, 0x57b560).
//
// Only the base methods are here; every resource kind has its own vtable, the methods call it:
// slot 0 (no arguments, before loading), 1 (other resource) takes the place of this one -> bool,
// 2 (bool) after clearing flag 0x20, 3 (0) after a search, 8 (int) the scaled priority, 9 (+0x28),
// 10 (0), 11 () unload, 12 () and 13 (1) release and delete a replacement, 14 () still valid -> bool,
// 15 (name, arg) load -> bool.
//
// The manager (only the fields these methods use): vtable slot 3 creates a resource; +4 int (below
// -3000 nothing is cached), +8 how many valid resources of a type may be loaded, +0xc byte (no caching),
// +0x14 stamp, +0x18 most resources loaded, +0x1c most cost loaded, +0x20 list head, +0x24 resources
// loaded, +0x28 float priority scale, +0x2c cost loaded, +0x34 priority bonus. 0x57afa0 unlinks a
// resource (and rescales the others if it had flag 1).
//
// The scaled priority is __ftol((priority + 3333) * scale - 3333.0f) with the int priority of the
// resource (+0x34) plus the manager's bonus (+0x34) unless flag 1 is set.
//
// The manager's own methods (0x57aef0-0x57b210, base vtable 0x600af8 with slot 3 pure; the sound
// manager derives from it) and the resource base's constructor, reset and assignment (0x57b230-
// 0x57b370, base vtable 0x600b24, 0x60 bytes from the pool) are in resource_manager.cpp.
#pragma once
#include "exe_base.h"

namespace zax {

struct CResourceCache;

struct CResource {
    DWORD vftable;
    DWORD type;           // 0x04 resources of one type count against the manager's limit (-1: any)
    DWORD cost;           // 0x08 added to the manager's +0x2c while loaded
    char *name;           // 0x0c operator new; a "~CFileBuf 0x" path when resident
    CResource *source;    // 0x10 loaded from this resource (released with slot 1)
    DWORD flags;          // 0x14 1 no priority bonus/locked, 2, 8 keep loaded, 0x10 was loaded, 0x20
    DWORD f18;            // 0x18
    DWORD stamp;          // 0x1c the manager's stamp of the last search
    CResource *replacement;   // 0x20 created by the manager's slot 3 when the limit is reached
    CResource *next;      // 0x24 in the manager's list
    DWORD f28;            // 0x28 passed to slot 9 after loading
    DWORD load_cost;      // 0x2c
    DWORD f30;            // 0x30
    DWORD priority;       // 0x34
    DWORD f38;            // 0x38
    float f3c;            // 0x3c

    DWORD THISCALL Reload();                                              // 0x57b520
    DWORD THISCALL MakeResident(DWORD reload);                            // 0x57b560
    CResource *THISCALL FindReplacement();                                // 0x57b630
    float THISCALL GetF3c();                                              // 0x57b730
    DWORD THISCALL CheckValid();                                          // 0x57b740 (al)
    DWORD THISCALL Load(const char *path, BYTE keep, DWORD mode, DWORD arg);   // 0x57b760 (al)
    DWORD THISCALL Insert(DWORD how);                                     // 0x57b950 (al)
    DWORD THISCALL ReloadFalse(DWORD unused);                             // 0x57bb10 (al = 0)
    DWORD THISCALL SetFlag20(BYTE on);                                    // 0x57bb60
    DWORD THISCALL SetFlag2(BYTE on);                                     // 0x57bba0 (al = 1)
    DWORD THISCALL SetKeep(BYTE on);                                      // 0x57bbc0 (flag 8)
    DWORD THISCALL SetLoadCost(DWORD n);                                  // 0x57bbf0 (al = changed)
    DWORD THISCALL SetType(DWORD t);                                      // 0x57bc50
    DWORD THISCALL SetPriority(DWORD p);                                  // 0x57bc60
    DWORD THISCALL SetF30(DWORD v);                                       // 0x57bcf0
    DWORD THISCALL Unlink();                                              // 0x57bd00 (al = slot 14)
    DWORD THISCALL Unload();                                              // 0x57bd30 (al = 1)

    // resource_manager.cpp
    CResource *THISCALL Ctor();                                           // 0x57b230
    CResource *THISCALL DeletingDtor(DWORD flags);                        // 0x57b250 (slot 13, pool 0x60)
    void THISCALL Dtor();                                                 // 0x57b280
    DWORD THISCALL ReloadThen(DWORD a, DWORD b);                          // 0x57b290 (slot 4 (a, 1), slot 2 (b))
    DWORD THISCALL Reset();                                               // 0x57b2f0 (al = 1)
    DWORD THISCALL Assign(CResource *from);                               // 0x57b370 (al)
};
// The base class is 0x60 bytes (0x57b250 returns that much to the pool; 0x57b370 copies +4-+0x5c);
// the bytes behind +0x40 are only copied here.
static_assert(sizeof(CResource) == 0x40);
constexpr DWORD RESOURCE_SIZE = 0x60;

struct CResourceCache {
    DWORD vftable;
    int f04;              // 0x04
    DWORD limit;          // 0x08
    BYTE disabled;        // 0x0c
    BYTE pad0d[3];
    DWORD f10;            // 0x10
    DWORD stamp;          // 0x14
    int max_loaded;       // 0x18
    int max_cost;         // 0x1c
    CResource *list;      // 0x20
    int loaded;           // 0x24
    float scale;          // 0x28
    int cost;             // 0x2c
    DWORD f30;            // 0x30 (the low byte set by 0x57b170)
    DWORD bonus;          // 0x34

    // resource_manager.cpp
    CResourceCache *THISCALL Ctor();                                      // 0x57af00
    CResourceCache *THISCALL DeletingDtor(DWORD flags);                   // 0x57af30 (slot 2)
    void THISCALL Dtor();                                                 // 0x57af50
    DWORD THISCALL Release(CResource *r);                                 // 0x57af60 (al = slot 12)
    void THISCALL NextStamp();                                            // 0x57af90
    DWORD THISCALL Unlink(CResource *r);                                  // 0x57afa0 (al)
    int THISCALL ScaledPriority(int p);                                   // 0x57b050
    DWORD THISCALL Rescale(int add);                                      // 0x57b080
    DWORD THISCALL SetLowPriority(BYTE on);                               // 0x57b0d0 (al = 1)
    DWORD THISCALL SetByte30(BYTE v);                                     // 0x57b170 (al = 1)
    DWORD THISCALL SetLevel(int v);                                       // 0x57b180 (al = 1)
    DWORD THISCALL SetLimit(DWORD v);                                     // 0x57b1b0 (al = 1)
    DWORD THISCALL SetDisabled(BYTE v);                                   // 0x57b1c0 (al = 1)
    DWORD THISCALL SetScale(DWORD v);                                     // 0x57b1d0 (al = 1, a float)
    DWORD THISCALL CheckEnabled();                                        // 0x57b1e0 (al)
    DWORD THISCALL ForgetAll();                                           // 0x57b200 (al = 1)
    DWORD THISCALL UnloadAll();                                           // 0x57b210 (al = 1)
};
static_assert(sizeof(CResourceCache) == 0x38);
CResourceCache *__cdecl ResourceCache_Get();                              // 0x57aef0

constexpr DWORD RESOURCE_CACHE = 0x713688;   // CResourceCache *
constexpr DWORD RESOURCE_PRIORITY_BASE = 0xd05;
constexpr DWORD RESOURCE_PRIORITY_OFFSET = 0x600b20;   // float 3333.0
constexpr DWORD RESOURCE_CACHE_VTABLE = 0x600af8, RESOURCE_VTABLE = 0x600b24;
constexpr int RESOURCE_LOW_PRIORITY_BONUS = -400;    // 0x57b0d0
constexpr int RESOURCE_UNLOAD_LEVEL = -3000;         // 0x57b180: below it the manager unloads everything

}  // namespace zax
