// File search of Zax.exe: the file spec, the result sinks and the search on disk and in the mounted
// zip archives (the exe itself, Data.ZIP or Data.DAT and the language packs *.RED, mounted by
// 0x4a0c40).
//
// Files_Search (0x4a1190) first searches the folder on disk below "." (FindFirstFileA, recursive in
// 0x4a4840), then, if an archive is mounted, the folder tree of the archives from the last mounted to
// the first (0x5c1da0). Every hit goes through ZFileSink::AddIfNew (0x49d3a0), which skips names the
// sink already holds, so a file on disk hides the same file in an archive. The result is the sink's
// count. Sinks: the directory lists (ZDirListSink, vtable 0x5f418c), a counter (vtable 0x5f40c8) and a
// buffer of fixed-size names (vtable 0x5f3cb0).
#pragma once
#include "ptr_array.h"

namespace zax {

// File spec (0x2c bytes, on the stack of the callers; constructor 0x4a09f0, no destructor of its own:
// the callers destroy the 5 ZStrings)
struct ZFileSpec {
    DWORD mode;           // 0x00 subfolders: 0 all, 1 all but those starting with '-' (constructor), others none
    DWORD folders;        // 0x04 1: list folders instead of files (0x4a0aa0)
    DWORD names_only;     // 0x08 disk: nonzero = name only, else path below the folder; archives: 1 = name only
    const char *folder;   // 0x0c
    const char *prefix;   // 0x10 the names start with it ("" or "-", 0x4a0aa0); disk search only for files
    DWORD count;          // 0x14 number of extensions
    ZString ext[5];       // 0x18 extensions without the dot ("" = all)

    DWORD THISCALL Init();                                          // 0x4a09d0 (without the ZStrings; returns 0)
    ZFileSpec *THISCALL Ctor();                                     // 0x4a09f0
    DWORD THISCALL AddExtension(const char *ext);                   // 0x4a0a30
    DWORD THISCALL Set(const char *folder, const char *ext);        // 0x4a0a60
    DWORD THISCALL SetFolders(const char *folder, BYTE dashed);     // 0x4a0aa0
    const char *THISCALL SetFolder(const char *folder);             // 0x4a0b10
    DWORD THISCALL SetMode(DWORD mode);                             // 0x4a0b20
    const char *THISCALL Extension(DWORD i);                        // 0x4a0b30
    DWORD THISCALL Mode();                                          // 0x4a0b40
};
static_assert(sizeof(ZFileSpec) == 0x2c);

// Base of the sinks: slot 0 count, slot 1 Add(name, const ZFileRecord *), slot 2 Contains(name)
struct ZFileRecord {
    DWORD size;
    WORD date, time;   // DOS date and time of the last write (0 in the archives)
};

struct ZFileSink {
    DWORD vftable;
    DWORD count;   // 0x04 (the counter and the name buffer; the directory list sink holds its list here)

    bool THISCALL AddIfNew(const char *name, DWORD size, DWORD date, DWORD time);   // 0x49d3a0
};

// Counter (vtable 0x5f40c8: slot 0 0x592370 returns +4, slot 2 0x4f0a70 returns false)
struct ZFileCounter : ZFileSink {
    void THISCALL Add(const char *name, const ZFileRecord *record);   // 0x49d400 (slot 1)
};

// Buffer of names with a fixed stride and optionally one dword per name (vtable 0x5f3cb0, slot 0
// 0x592370), filled by Files_ListNames
struct ZFileNameBuffer : ZFileSink {
    DWORD max;          // 0x08
    DWORD index;        // 0x0c next record (counts like +4)
    DWORD max_length;   // 0x10 0: no check, otherwise a longer name ends the game
    char *names;        // 0x14 or null
    BYTE *values;       // 0x18 receives the size at the same offset as the name, or null
    DWORD stride;       // 0x1c

    ZFileNameBuffer *THISCALL Ctor(DWORD max_length, char *names, BYTE *values, DWORD max, DWORD stride);   // 0x49d410
    bool THISCALL Contains(const char *name);                         // 0x49d450 (slot 2)
    DWORD THISCALL Add(const char *name, const DWORD *value);         // 0x49d500 (slot 1)
};
static_assert(sizeof(ZFileNameBuffer) == 0x20);

// A folder in the tree of the mounted archives (operator new). The nodes of a mounted archive carry
// its directory at +0x18.
struct ZZipNode {
    ZZipNode *parent;   // 0x00
    ZString name;       // 0x04 without '/'
    ZPtrArray children; // 0x08 pool pointer array (vtable 0x6060c4)
    void *archive;      // 0x18 zip directory: Zip_ListDir 0x5c2970 lists its files

    void THISCALL List(ZFileSink *sink, const ZFileSpec *spec);   // 0x5c13c0
};

// The mounted archives (pool pointer array of ZZipNode roots, function-local static 0x714570,
// vtable 0x6060b8, guard bit 0 of 0x714568, destroyed at exit by 0x5c16a0)
struct ZZipList : ZPtrArray {
    static ZZipList *__cdecl Get();                                                // 0x5c1650
    void THISCALL Search(ZFileSink *sink, const ZFileSpec *spec);                  // 0x5c1da0
    ZZipNode *THISCALL FindArchiveNode(const char *path, ZZipNode *node);          // 0x5c1890
    ZZipNode *THISCALL FindNode(const char *path, ZZipNode *node);                 // 0x5c17b0
};

// Search functions (register arguments)
DWORD __fastcall Files_Search(ZFileSink *sink, const ZFileSpec *spec, DWORD flags);   // 0x4a1190
DWORD __fastcall Files_SearchDisk(ZFileSink *sink, const ZFileSpec *spec, DWORD strip_ext, const char *folder_ext,
                                  DWORD skip);                                        // 0x4a4840
DWORD __fastcall Files_Count(const ZFileSpec *spec, DWORD flags);                     // 0x4a12d0
DWORD __fastcall Files_ListNames(const ZFileSpec *spec, char *names, BYTE *values, DWORD stride, DWORD max,
                                 DWORD flags);                                        // 0x4a1150

constexpr DWORD FILE_COUNTER_VTABLE = 0x5f40c8;
constexpr DWORD FILE_NAME_BUFFER_VTABLE = 0x5f3cb0;
constexpr DWORD ZIP_LIST = 0x714570;
constexpr DWORD ZIP_LIST_GUARD = 0x714568;
constexpr DWORD ZIP_LOADED = 0x6bdf64;      // byte: at least one archive mounted
constexpr DWORD FILE_PATH_BUFFER = 0x6be0a4;  // static buffer of the disk search for paths below the folder

}  // namespace zax
