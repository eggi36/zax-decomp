// Shared by the checks of the source-built libraries (checks_zlib.cpp, checks_vorbis.cpp; phase 5b):
// putting a module's hooks out and in again around the original side, pausing the game's sound streams
// meanwhile, reading game files, the exe's heap.
//
// Every case runs twice from the same inputs: first with the module's hooks put out (the exe's own copy of
// the library, all of it, since every library function is hooked), then with them in (the library built
// from source). The library functions are called through their exe addresses on both sides.
#pragma once
#include "check_util.h"
#include "file_buffer.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);

namespace zax::libchk {

inline void *crt_malloc(DWORD n) { return at<void *(__cdecl *)(DWORD)>(0x5d164f)(n); }
inline void crt_free(void *p) { at<void(__cdecl *)(void *)>(0x5d33e0)(p); }

// ---- A module's hooks out and in ---------------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
    int index;   // in list
};
constexpr int MAX_SITES = 256;
struct HookSites {
    const char *module;
    int (*table)(Hook *);
    Hook list[MAX_SITES];   // static storage: no stack probes in the DLL (docs/DECOMPILATION.md 5)
    SavedSite sites[MAX_SITES];
    int n;
    void out()
    {
        n = 0;
        const int count = table(nullptr);
        if (!hook_sites_fit(module, count, MAX_SITES))
            return;
        table(list);
        for (int i = 0; i < count; i++) {
            BYTE *site = at<BYTE *>(list[i].address);
            if (!memcmp(site, list[i].expected, 5))
                continue;   // not hooked (Disable=)
            SavedSite &s = sites[n++];
            s.site = site;
            memcpy(s.bytes, site, 5);
            s.index = i;
            write_code(site, list[i].expected);
        }
    }
    // With the hooks out: one of them in again and out again (the checks of each function alone)
    void one_in(int k) { write_code(sites[k].site, sites[k].bytes); }
    void one_out(int k) { write_code(sites[k].site, list[sites[k].index].expected); }
    const char *name(int k) const { return list[sites[k].index].name; }
    void in()
    {
        for (int i = 0; i < n; i++)
            write_code(sites[i].site, sites[i].bytes);
        n = 0;
    }
};

// ---- The game's sound streams paused ------------------------------------------------------------------------
// The music, the menu music and the voice-overs are decoded in the multimedia timer's thread (sound_stream.cpp:
// Stream_TimerCallback, "Use Threaded Stream") through vorbisfile. While the Vorbis checks put the library's
// hooks out and in, no other thread may run in it: each stream state's "callback running" flag (+0xe) is
// set, as checks_sound_common.cpp does for its fake world (twice, with a wait longer than a timer period in
// between, so a refill that was already running has ended and cleared its flag). The flags are put back after.
struct PausedStreams {
    static constexpr int MAX = 4;
    BYTE *state[MAX];
    BYTE flag[MAX];
    int n;
    static bool readable(const void *p, DWORD bytes)
    {
        struct {
            void *base, *allocation_base;
            DWORD allocation_protect;
            size_t size;
            DWORD state, protect, type;
        } mi;
        const DWORD a0 = (DWORD)(uintptr_t)p;
        if (a0 < 0x10000 || a0 >= 0x80000000u || a0 + bytes > 0x80000000u)
            return false;
        for (DWORD a = a0 & ~0xfffu; a < a0 + bytes; a += 0x1000)
            if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
                (mi.protect & 0x100) || !(mi.protect & 0xee))
                return false;
        return true;
    }
    void add(BYTE *s)
    {
        if (!s || !readable(s, 0x68) || n >= MAX)
            return;
        for (int i = 0; i < n; i++)
            if (state[i] == s)
                return;
        state[n] = s;
        flag[n] = 0;
        n++;
    }
    void pause()
    {
        n = 0;
        BYTE *world = *at<BYTE **>(0x6c2080);
        if (world && readable(world + 0x334, 4)) {
            BYTE *mixer = *reinterpret_cast<BYTE **>(world + 0x334);
            if (mixer && readable(mixer, 0x34))
                add(*reinterpret_cast<BYTE **>(mixer + 0x30));
        }
        BYTE *app = *at<BYTE **>(0x713f14);
        if (app && readable(app + 0x274, 4))
            add(*reinterpret_cast<BYTE **>(app + 0x274));
        if (*at<BYTE *>(0x713c34) & 1)
            add(*at<BYTE **>(0x713c28));
        for (int pass = 0; pass < 2; pass++) {
            for (int i = 0; i < n; i++) {
                volatile BYTE &busy = state[i][0xe];
                if (pass == 0)
                    flag[i] = busy;
                busy = 1;
            }
            if (n)
                Sleep(120);
        }
    }
    void resume()
    {
        for (int i = 0; i < n; i++)
            *reinterpret_cast<volatile BYTE *>(state[i] + 0xe) = flag[i];
        n = 0;
    }
};

// ---- Game files ---------------------------------------------------------------------------------------------
// A whole file through the game's file layer (File_Open 0x4a2440 with flag 4: the mounted archives or the
// disk, as the game reads it), into a block of the exe's heap; null if it is missing or empty.
inline BYTE *read_game_file(const char *path, DWORD &size)
{
    size = 0;
    ZStream *s = at<ZStream *(__fastcall *)(const char *, DWORD)>(0x4a2440)(path, 4);
    if (!s)
        return nullptr;
    auto slot = [&](int i) { return (*reinterpret_cast<void *const *const *>(s))[i]; };
    const DWORD n = reinterpret_cast<DWORD(THISCALL *)(ZStream *)>(slot(STREAM_SIZE))(s);
    BYTE *p = n ? (BYTE *)crt_malloc(n) : nullptr;
    if (p && reinterpret_cast<DWORD(THISCALL *)(ZStream *, void *, DWORD)>(slot(STREAM_READ))(s, p, n) != n) {
        crt_free(p);
        p = nullptr;
    }
    reinterpret_cast<void *(THISCALL *)(ZStream *, DWORD)>(slot(STREAM_DELETE))(s, 1);
    if (p)
        size = n;
    return p;
}

}  // namespace zax::libchk
