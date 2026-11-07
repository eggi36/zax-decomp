// Zip archives of Zax.exe: the directory of one mounted archive (ZZipDir) and mounting it into the folder
// tree of the mounted archives (ZZipList, ZZipNode in file_search.h).
//
// Zip_MountAll (0x4a0c40) mounts at start the exe itself, otherwise Data.ZIP, otherwise Data.DAT, then the
// language packs ./*.RED, each as a root of its own at "/"; then every *.zip below "." (name without
// ".zip" as the mount folder, all into the root of the first one, read on first use). A root is a tree of
// ZZipNodes along the mount folder; the node of the last folder carries the directory. The directory
// holds the central directory of the archive: one entry per file with the mount folder in front of its
// name, in the order of the file and sorted by name through an index (binary search). Only stored files
// are allowed: a compressed one ends the game.
#pragma once
#include "file_search.h"

namespace zax {

#pragma pack(push, 2)
// An entry of the directory (0x30 bytes): the central directory header without its signature at +4
struct ZZipEntry {
    char *name;                  // 0x00 operator new: mount folder + name in the archive
    WORD version_made;           // 0x04
    WORD version_needed;         // 0x06
    WORD flags;                  // 0x08
    WORD method;                 // 0x0a only 0 (stored)
    WORD time;                   // 0x0c DOS
    WORD date;                   // 0x0e DOS
    DWORD crc;                   // 0x10
    DWORD packed_size;           // 0x14
    DWORD size;                  // 0x18
    WORD name_length;            // 0x1c
    WORD extra_length;           // 0x1e
    WORD comment_length;         // 0x20
    WORD disk;                   // 0x22
    WORD internal_attributes;    // 0x24
    DWORD external_attributes;   // 0x26
    DWORD offset;                // 0x2a of the local header
    BYTE unknown2e;              // 0x2e cleared by Load; 1 while a stream has the entry open (zip_stream.cpp)
    BYTE listed;                 // 0x2f result of the last AddIfNew in ListDir
};
#pragma pack(pop)
static_assert(sizeof(ZZipEntry) == 0x30);

// The directory of one archive (0x2c bytes, operator new; ZZipNode +0x18)
struct ZZipDir {
    BYTE loaded;           // 0x00 Load ran
    BYTE unknown01;        // 0x01 cleared by the constructor; 1: the central directory must be written again (zip_stream.cpp)
    BYTE keep_open;        // 0x02 1: Load leaves the file open (Init without lazy)
    void *file;            // 0x04 CFileBuf (file_buffer.h; 0x1c bytes from the pool)
    char *path;            // 0x08 operator new copy of the archive path
    char *prefix;          // 0x0c mount folder in front of every name ("" for one starting with '/')
    ZZipEntry *entries;    // 0x10 operator new with the count in the dword before
    DWORD *sorted;         // 0x14 entry indexes sorted by name (filled only with more than one entry)
    DWORD count;           // 0x18
    DWORD unknown1c;       // 0x1c cleared by the constructor; number of open streams on the archive buffer
    DWORD open_flags;      // 0x20 for FileBuf_Open (| 0x404) and File_Exists
    DWORD end_signature;   // 0x24 0x06054b50, or 0x06054552 ("RE\5\6", Data.dat)
    DWORD check_disk;      // 0x28 Exists also reports whether the archive file exists

    ZZipDir *THISCALL Ctor();                                                                       // 0x5c2040
    bool THISCALL Init(const char *archive, DWORD check, const char *mount, DWORD lazy, DWORD flags);   // 0x5c2710
    bool THISCALL Load();                                                                           // 0x5c2240
    DWORD THISCALL Close();                                                                         // 0x5c2780 (thunk 0x5c2070)
    bool THISCALL Exists(const char *path, BYTE *archive_on_disk);                                  // 0x5c2830
    DWORD THISCALL ListDir(ZFileSink *sink, const ZFileSpec *spec);                                 // 0x5c2970
    ZZipEntry *THISCALL Find(const char *name, DWORD *position);                                    // 0x5c2d50

    static int __cdecl CompareOffset(const void *a, const void *b);   // 0x5c2220 (qsort of the entries)
    static int __cdecl CompareName(const void *a, const void *b);     // 0x5c21e0 (qsort of the index, entries in 0x714584)
};
static_assert(sizeof(ZZipDir) == 0x2c);

// Pool pointer arrays (vtable 0x6060b8 for the roots and children, 0x6060c4 after destruction): the
// functions of this module that work on them
struct ZZipNodeArray : ZPtrArray {
    void THISCALL Reserve(DWORD n);   // 0x5c11c0 (also used by two other pool arrays)
    void THISCALL Free();             // 0x5c2010
    DWORD THISCALL DeleteNodes();     // 0x5c1e20
};

// The parts of ZZipNode and ZZipList in this module (the structures are in file_search.h)
struct ZZipNodeOps : ZZipNode {
    void THISCALL Dtor();                                   // 0x5c1260
    ZZipNode *THISCALL AddChild(const char *name, DWORD n);   // 0x5c1320
};

struct ZZipListOps : ZZipList {
    DWORD THISCALL Clear();                                                           // 0x5c1570
    static void __cdecl AtExit();                                                     // 0x5c16a0
    bool THISCALL Mount(const char *archive, DWORD new_root, DWORD check_disk, const char *mount, DWORD lazy,
                        DWORD flags);                                                 // 0x5c1a30
    DWORD THISCALL Open(const char *path, DWORD mode);                                // 0x5c1c70
    bool THISCALL Exists(const char *path, BYTE *archive_on_disk);                    // 0x5c1d00
};

void __fastcall Zip_MountAll(const char *name, DWORD mode, DWORD skip_zips);   // 0x4a0c40
bool __cdecl Zip_UnmountAll();                                                // 0x4a1120
const char *__cdecl Files_CacheFolder();                                      // 0x4a1140

constexpr DWORD ZIP_END = 0x06054b50, ZIP_END_RE = 0x06054552, ZIP_CENTRAL = 0x02014b50, ZIP_LOCAL = 0x04034b50;
constexpr DWORD ZIP_SORT_ENTRIES = 0x714584;   // entries for CompareName
constexpr DWORD DATA_FOLDER = 0x6135ec;        // char[]: "." while mounting, then the name ("Data")
constexpr DWORD NODE_ARRAY_VTABLE = 0x6060b8, NODE_ARRAY_BASE_VTABLE = 0x6060c4;

}  // namespace zax
