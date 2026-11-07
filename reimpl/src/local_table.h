// The Local.txt table of Zax.exe (0x6c2b90): loading, adding and sorting the entries, the list of
// strings registered for translation, the string pair and the static initialisers around them
// (0x4270f0, 0x500740-0x500ac0, 0x501030). The lookup itself (Local_Translate 0x500f50) and the
// field slot that uses it (0x5007c0) are in field_slots.cpp.
//
//   ZLocalTable (0x110, the one instance 0x6c2b90, loaded by Game_Startup 0x4cdbc5 from "Local.txt"):
//     +0x000 the file name as given (strcpy without a limit)
//     +0x100 0 when the file opened from the disk (flags 0x224: in memory, disk only, read), 1 when the
//            second attempt (flags 4, archives and the language pack too) was needed or both failed
//     +0x104 count, +0x108 capacity, +0x10c entries: a new[] block {count dword, LocalEntry[]} from
//            operator new, sorted with qsort by key (ZString::Compare) after every Add and after loading.
//   LocalEntry (8 bytes, the string pair: 0x500740 constructs it empty, 0x500760 from two strings,
//     0x4270f0 destroys the text, then the key): {ZString key, ZString text}.
//   The file: lines "ENGLISH: <key>[END]" followed by "LOCALIZED: <text>[END]"; a part without "[END]"
//     continues on the next lines (joined with "\n") until a line contains it. The prefix of the second
//     line is not checked, only skipped (its length). "[END]" is cut from the key only when its first
//     occurrence is at the very end, so a key with a context keeps "[END] (context)", which is the form
//     Local_Translate builds. Other lines are ignored.
//   Registered strings (0x501030, the UI's function-local static strings): a ZString built from the text
//     is appended with its context to the list 0x6c2ca8 (8-byte entries {ZString *, context}, vtable
//     0x5f99e0, built on first use with guard bit 0 of 0x6c2cbc, destructor 0x500a70 via atexit;
//     0x6c2cc4 points to it); every Load translates all of them through the table, whether the file
//     opened or not. 0x6c2cc8 is set to 1 by a successful Load and read nowhere else.
//   Static initialisers: the lengths of "ENGLISH: " (0x6c2cc0), "LOCALIZED: " (0x6c2ca4), "[END]"
//     (0x6c2ca0) and of " (%s)" minus "%s" (0x6c2cb8), read through the text pointers 0x6190a4-0x6190b0;
//     0x500870 clears the table's entries and registers its destructor 0x500890.
// Two neighbours are not part of it: 0x5007a0 FieldDesc_Name (add ecx, 4 + jmp, not hookable) and
// 0x5007b0 (lea eax, [ecx + 4]; ret: 4 bytes, not hookable).
#pragma once
#include "exe_base.h"

namespace zax {

struct LocalEntry {
    ZString key;
    ZString text;

    LocalEntry *THISCALL Ctor();                                          // 0x500740
    LocalEntry *THISCALL CtorFrom(const ZString *key, const ZString *text);   // 0x500760
    DWORD THISCALL Dtor();                                                // 0x4270f0 (the ZString destructor's eax)
    static int __cdecl Compare(const LocalEntry *a, const LocalEntry *b);   // 0x5008f0 (qsort)
};
static_assert(sizeof(LocalEntry) == 8);

struct ZLocalTable {
    char path[0x100];
    BYTE from_archive;   // 0x100
    DWORD count;         // 0x104
    DWORD capacity;      // 0x108
    LocalEntry *entries; // 0x10c

    DWORD THISCALL HasData();                                    // 0x500ab0 (0 or 1)
    DWORD THISCALL Load(const char *file, DWORD unused);         // 0x500ac0 (al: file read)
    DWORD THISCALL Add(const ZString *key, const ZString *text); // 0x500900 (qsort's eax)
};
static_assert(sizeof(ZLocalTable) == 0x110);

struct LocalPending {
    ZString *text;
    const char *context;
};
struct ZLocalPendingList {
    DWORD vftable;   // 0x5f99e0
    LocalPending *data;
    DWORD count;
    DWORD capacity;
};

// 0x501030 (thiscall on the new string): ZString(text), registered for translation; returns it
ZString *THISCALL LocalString_Ctor(ZString *self, const char *text, const char *context);

// LOCAL_TABLE (0x6c2b90) is in field_slots.h
constexpr DWORD LOCAL_PENDING = 0x6c2ca8, LOCAL_PENDING_PTR = 0x6c2cc4, LOCAL_PENDING_GUARD = 0x6c2cbc;
constexpr DWORD LOCAL_LOADED = 0x6c2cc8;
constexpr DWORD LOCAL_LEN_ENGLISH = 0x6c2cc0, LOCAL_LEN_LOCALIZED = 0x6c2ca4, LOCAL_LEN_END = 0x6c2ca0;
constexpr DWORD LOCAL_LEN_CONTEXT = 0x6c2cb8;

}  // namespace zax
