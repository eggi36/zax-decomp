// Checks of sound_list.cpp against the original, in the game thread.
//
// In the sound fake world (checks_sound_common.h): sound lists built in each side's arena with the exe's
// own record list functions (RecordList_Ctor/Set, the binary search, the file name search, ZString),
// records with names (.wav and .ogg, sorted), the "used" flags and fake resources (recording vtables: copy,
// play); the resource cache, the resource base's methods, the registry of the resource lists, the file
// search, the name tree and the resource list's Find are recorders. Cases: the name, the constructor,
// both destructors and Clear, the size, Load (folders with and without a final '/', masks, a second
// extension, 0-6 files, unsorted names), the residents, the path of an index (in and beyond the list),
// Get, Find (the name as it is, with the other extension, in other case, missing, with hints), the
// precaches, playing by name and index (empty lists), the copy for the ambient sounds, every flag slot,
// Loaded, MakeResident and Reload (loaded or not).
// Real data: the level's sound list (0x713708: its records and names) as a copy with fake resources,
// through Find with every real name (and with the other extension), PathOf, Size and Loaded.
#include "checks_sound_common.h"

namespace zax {

int hooks_sound_list(Hook *out);

namespace {

using namespace sndc;

constexpr DWORD VT_LIST = 0x600c2c, RECORD = 0x108;
enum { K_RES, K_CACHE, K_TREE };
enum { F_CACHE, F_REGISTRY, F_RES, N_RES = 8, F_TREE = F_RES + N_RES };

DWORD c_new_resource(Call &)
{
    const DWORD v = nv();
    return ptrv(fake(F_RES + (int)(v % N_RES)));
}
DWORD c_cache(Call &) { return ptrv(fake(F_CACHE)); }
DWORD c_registry(Call &) { return ptrv(fake(F_REGISTRY)); }
DWORD c_index(Call &) { return nv() % 50; }
DWORD c_this(Call &c) { return c.ecx; }
DWORD c_tree(Call &c)
{
    memset(at<void *>(c.ecx), 0, 0x34);
    dw(at<void *>(c.ecx), 0) = ptrv(g_vt[K_TREE]);
    return c.ecx;
}
// The file search: 0-6 sound files of the folder (unsorted)
const char *const FILES[] = {"walk.wav", "Shot.wav", "AMBIENT/wind.ogg", "explode.wav", "beep.ogg", "zap.WAV", "a.wav"};
DWORD g_nfiles;
DWORD c_count(Call &) { return g_nfiles; }
DWORD c_list_names(Call &c)
{
    BYTE *out = at<BYTE *>(c.edx);
    const DWORD stride = c.a[1], max = c.a[2];
    const DWORD n = g_nfiles < max ? g_nfiles : max;
    const DWORD folder = nv() % 2;   // the names with the folder in front (cut off by Text_CutPrefix) or without
    for (DWORD i = 0; i < n; i++)
        wsprintfA(reinterpret_cast<char *>(out + i * stride), "%s%s", folder ? "Sounds/" : "", FILES[(i * 5 + g_nfiles) % 7]);
    return n;
}
// ResourceList_Find (name, hint) on the list: an index of the list or -1, as the real one
DWORD c_find(Call &c)
{
    const DWORD v = nv(), n = dw(at<void *>(c.ecx), 0x14);
    return v % 3 == 0 || !n ? 0xffffffff : v % n;
}

const Spec DIRECT[] = {
    {0x57aef0, "ResourceCache_Get", 'c', "", 'c', c_cache},
    {0x57af60, "ResourceCache_Release", 't', "p", 'n', nullptr},
    {0x57bcf0, "Resource_SetF30", 't', "v", 'n', nullptr},
    {0x57b560, "Resource_MakeResident", 't', "v", 'n', nullptr},
    {0x57b760, "Resource_Load", 't', "svvv", 'n', nullptr},
    {0x4a35d0, "ResourceLists_Get", 'c', "", 'c', c_registry},
    {0x4a34d0, "ResourceLists_Add", 't', "spvv", 'c', c_index},
    {0x523e20, "ResourceList::SetRegistryIndex", 't', "v", 'n', nullptr},
    {0x4a12d0, "Files_Count", 'F', "", 'c', c_count},   // ecx the file spec, a local
    {0x4a1150, "Files_ListNames", 'F', "vvvv", 'c', c_list_names},
    {0x47e560, "NameTree_Ctor", 't', "p", 'c', c_tree},
    {0x523df0, "ResourceList_Find", 't', "sv", 'c', c_find},
};
#define NOSLOT {0, nullptr, 0, "", 0, nullptr}
const Spec RES_SLOTS[] = {NOSLOT, {0, "SoundResource::Assign", 't', "p", 'b', nullptr}, {0, "Resource::Insert", 't', "v", 'b', nullptr}};
const Spec CACHE_SLOTS[] = {NOSLOT, NOSLOT, NOSLOT, {0, "SoundManager::NewResource", 't', "", 'c', c_new_resource}};
const Spec TREE_SLOTS[] = {NOSLOT, NOSLOT, NOSLOT, NOSLOT, NOSLOT, NOSLOT, NOSLOT, {0, "NameTree::DeletingDtor", 't', "v", 'c', c_this}};
#undef NOSLOT
const Standins STANDINS[] = {{DIRECT, sizeof DIRECT / sizeof DIRECT[0]}};
const Module MODULE = {hooks_sound_list, nullptr, 0};

// ---- Lists of a case -------------------------------------------------------------------------------------

const char *const NAMES[] = {"ambient1.wav", "Beep.wav", "door.ogg", "explode.wav", "ShieldHit.wav", "step2.ogg", "zap.wav", "zz_last.wav"};
struct RealList {
    DWORD n;
    char names[64][0x100];
    BYTE flags[64][2];
    DWORD sizes[64], loaded[64];
};
RealList *g_real;

// A list in the running side's arena: the exe's constructor (the original's, through its trampoline on
// both sides, so the setup is the same), then records
BYTE *make_list(Gen &g, DWORD n)
{
    BYTE *l = alloc(0x40);
    at<BYTE *(THISCALL *)(void *)>(ptrv(original_address(0x57e070)))(l);
    if (g_real && g_real->n < n)
        n = g_real->n;
    if (!n)
        return l;
    BYTE *data = alloc((n + 1) * RECORD);
    for (DWORD i = 0; i < n; i++) {
        BYTE *rec = data + i * RECORD;
        if (g_real) {
            lstrcpyA(reinterpret_cast<char *>(rec), g_real->names[i]);
            rec[0x100] = g_real->flags[i][0];
            rec[0x101] = g_real->flags[i][1];
        } else {
            lstrcpyA(reinterpret_cast<char *>(rec), NAMES[i]);
            rec[0x100] = (BYTE)g.below(2);
            rec[0x101] = (BYTE)g.below(2);
        }
        dw(rec, 0x104) = g.next() % 8 ? ptrv(fake(F_RES + (int)(i % N_RES))) : 0;
    }
    at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(l, 1, RECORD, n, data, 0);
    dw(l, 0x34) = ptrv(data);
    at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(l + 0x30, "Sounds/");
    at<void *(THISCALL *)(void *, const char *)>(0x4e1930)(l + 0x3c, "Sounds");
    if (g.next() % 3 == 0) {
        BYTE *t = alloc(0x34);
        dw(t, 0) = ptrv(g_vt[K_TREE]);
        dw(l, 0x38) = ptrv(t);
    }
    return l;
}
void set_resources(Gen &g)
{
    memset(&g_template, 0, sizeof g_template);
    make_fake(F_CACHE, K_CACHE);
    make_fake(F_REGISTRY, K_CACHE);
    make_fake(F_TREE, K_TREE);
    for (int i = 0; i < N_RES; i++) {
        make_fake(F_RES + i, K_RES);
        dw(tmpl(F_RES + i), 8) = g.below(0x10000);              // the size
        dw(tmpl(F_RES + i), 0x14) = g.next() & 0x3f;            // the flags (0x10 loaded)
        if (g_real) {
            dw(tmpl(F_RES + i), 8) = g_real->sizes[i % 64];
            dw(tmpl(F_RES + i), 0x14) = g_real->loaded[i % 64];
        }
    }
}

enum What {
    W_NAME, W_CTOR, W_DTOR_DELETING, W_DTOR, W_CLEAR, W_SIZE, W_LOAD, W_ALL_RESIDENT, W_PATH, W_GET, W_FIND, W_PRECACHE_BY_NAME,
    W_PRECACHE_NAME, W_PLAY_NAME, W_PLAY_INDEX, W_COPY, W_PLAY, W_PRECACHE, W_CLEAR_USED, W_USED, W_SET_USED, W_FLAG, W_SET_FLAG,
    W_LOADED, W_RESIDENT, W_RELOAD, NWHAT
};
const DWORD ADDR[] = {0x57df90, 0x57e070, 0x57e0d0, 0x57e190, 0x57e240, 0x57e2c0, 0x57e2f0, 0x57e590, 0x57e5d0, 0x57e650, 0x57e690, 0x57e7b0, 0x57e7f0,
                      0x57e820, 0x57e890, 0x57e8e0, 0x57e940, 0x57e990, 0x57e9e0, 0x57ea10, 0x57ea30, 0x57ea50, 0x57ea70, 0x57ea90, 0x57eac0, 0x57eae0};
const char *const WHAT[] = {"Name", "Ctor", "DeletingDtor", "Dtor", "Clear", "Size", "Load", "MakeAllResident", "PathOf", "Get", "Find",
                            "PrecacheByName", "PrecacheName", "PlayName", "PlayIndex", "Copy", "Play", "Precache", "ClearUsed", "Used",
                            "SetUsed", "Flag101", "SetFlag101", "Loaded", "MakeResident", "Reload"};
const char *const LOOKUPS[] = {"Beep.wav", "beep.WAV", "door.wav", "door.ogg", "explode.ogg", "missing.wav", "nothing", "zz_last.wav", ""};

// A name to look up: a real one (or with the other extension) or one of the table
const char *lookup(Gen &g, char *buf)
{
    if (g_real && g_real->n) {
        const DWORD i = g.below(g_real->n);
        lstrcpyA(buf, g_real->names[i]);
        const int n = lstrlenA(buf);
        if (g.next() % 3 == 0 && n > 4) {
            char *ext = buf + n - 4;
            if (!lstrcmpiA(ext, ".wav"))
                lstrcpyA(ext, ".ogg");
            else if (!lstrcmpiA(ext, ".ogg"))
                lstrcpyA(ext, ".wav");
        }
        return buf;
    }
    return LOOKUPS[g.below(sizeof LOOKUPS / sizeof LOOKUPS[0])];
}

void run_case(Tally &t, What w, unsigned seed)
{
    Result r{};
    char name[0x110];
    for (int side = 0; side < 2; side++) {
        Gen g{seed * 31u + 7u};
        set_resources(g);
        side_begin(side, seed, MODULE, (BYTE)(0x5a + seed % 3));
        g_nfiles = g.below(7);
        const DWORD n = g_real ? g_real->n : g.below(9);
        BYTE *l = w == W_CTOR ? alloc(0x40) : make_list(g, n);
        if (w == W_CTOR)
            memset(l, 0x66, 0x40);
        const DWORD count = dw(l, 0x14);
        DWORD a[4] = {0, 0, 0, 0};
        int nargs = 0;
        // an index in the list (the slots that do not test it), or anywhere
        const DWORD inside = count ? g.below(count) : 0;
        const DWORD any = g.next() % 4 ? inside : g.below(count + 3);
        switch (w) {
        case W_DTOR_DELETING: a[0] = g.below(2); nargs = 1; break;
        case W_SIZE: case W_PRECACHE: case W_USED: case W_SET_USED: case W_FLAG: case W_LOADED: case W_RESIDENT: case W_RELOAD:
        case W_PLAY:
            if (!count)
                goto skip;   // the exe reads record 0 of an empty list
            // Play calls the record's resource unchecked (Load gives every record one)
            if (w == W_PLAY && !dw(at<BYTE *>(dw(l, 0x34)) + inside * RECORD, 0x104))
                goto skip;
            a[0] = inside;
            nargs = 1;
            break;
        case W_SET_FLAG:
            if (!count)
                goto skip;
            a[0] = inside;
            a[1] = g.next();
            nargs = 2;
            break;
        case W_GET: case W_PLAY_INDEX: case W_COPY: a[0] = any; nargs = 1; break;
        case W_PATH: {
            BYTE *out = alloc(4);
            at<void *(THISCALL *)(void *)>(0x4dec90)(out);
            a[0] = any;
            a[1] = ptrv(out);
            nargs = 2;
            break;
        }
        case W_FIND:
            a[0] = ptrv(lookup(g, name));
            // the hint: -1 or an index of the list (the binary search 0x591b20 starts there unchecked)
            a[1] = g.next() % 3 || !count ? 0xffffffff : g.below(count);
            nargs = 2;
            break;
        case W_PRECACHE_BY_NAME: case W_PRECACHE_NAME: case W_PLAY_NAME:
            a[0] = ptrv(lookup(g, name));
            nargs = 1;
            break;
        case W_LOAD: {
            const char *const FOLDERS[] = {"Sounds", "Sounds/", "Sounds/AMBIENT", ""};
            // as the exe's two callers (0x4cdf70: "Sounds", "Sounds", ".wav", ".ogg"; the world's voice-overs
            // 0x6c2480: "Voiceovers", "Levels", ".wav", ".ogg"); every mask has a dot (FileSpec_Set reads
            // what strchr finds)
            const char *const MASKS[] = {".wav", "*.wav", ".ogg", "a.b.wav"};
            a[0] = ptrv(g.next() % 2 ? "Sounds" : "Voiceovers");
            a[1] = ptrv(FOLDERS[g.below(4)]);
            a[2] = ptrv(MASKS[g.below(4)]);
            a[3] = g.next() % 3 ? ptrv(".ogg") : 0;
            nargs = 4;
            break;
        }
        default: break;
        }
        r.eax[side] = call_fn(ADDR[w], ptrv(l), 0, nargs, a, false, nullptr);
    skip:
        side_end();
    }
    DWORD mask = 0xffffffff;
    switch (w) {
    case W_DTOR: case W_ALL_RESIDENT: case W_PATH: case W_PRECACHE_BY_NAME: case W_PRECACHE_NAME: case W_PRECACHE: case W_SET_USED:
    case W_SET_FLAG: case W_RELOAD:
        mask = 0;
        break;
    case W_USED: case W_FLAG: case W_LOADED:
        mask = 0xff;
        break;
    default: break;
    }
    compare(t, WHAT[w], seed, r, mask);
}

void test_all(Tally &t)
{
    const int COUNTS[NWHAT] = {5, 20, 60, 30, 60, 60, 300, 40, 150, 100, 600, 150, 100, 150, 100, 150, 100, 100, 40, 60, 40, 60, 60, 60, 60, 150};
    for (int w = 0; w < NWHAT; w++)
        for (int k = 0; k < scaled(COUNTS[w]); k++)
            run_case(t, (What)w, 0x5000u * (unsigned)(w + 1) + (unsigned)k);
}

RealList g_real_list;
int collect_real()
{
    BYTE *list = at<BYTE *>(0x713708);
    const DWORD n = dw(list, 0x14);
    const BYTE *records = at<const BYTE *>(dw(list, 0x34));
    if (dw(list, 0) != VT_LIST || !n || !records || !readable(records, RECORD * n))
        return 0;
    // the first 64 records in their order (the binary search needs them sorted, as they are)
    g_real_list.n = n < 64 ? n : 64;
    for (DWORD i = 0; i < g_real_list.n; i++) {
        const BYTE *rec = records + RECORD * i;
        memcpy(g_real_list.names[i], rec, 0x100);
        g_real_list.names[i][0xff] = 0;
        g_real_list.flags[i][0] = rec[0x100];
        g_real_list.flags[i][1] = rec[0x101];
        const BYTE *res = at<const BYTE *>(dw((void *)rec, 0x104));
        g_real_list.sizes[i] = res && readable(res, 0x18) ? dw((void *)res, 8) : 0;
        g_real_list.loaded[i] = res && readable(res, 0x18) ? dw((void *)res, 0x14) : 0;
    }
    return (int)n;
}

}  // namespace

ZAX_CHECKS(sound_list)
{
    (void)cls;
    (void)n;
    Tally all{"sound_list: the sound list's functions in the fake world"},
        real{"sound_list: the level's sound list as a copy: Find with its names, PathOf, Size, Loaded (real data)"};
    const int found = collect_real();
    set_kind(K_RES, RES_SLOTS, sizeof RES_SLOTS / sizeof RES_SLOTS[0]);
    set_kind(K_CACHE, CACHE_SLOTS, sizeof CACHE_SLOTS / sizeof CACHE_SLOTS[0]);
    set_kind(K_TREE, TREE_SLOTS, sizeof TREE_SLOTS / sizeof TREE_SLOTS[0]);
    if (world_on(STANDINS, 1)) {
        test_all(all);
        if (found) {
            g_real = &g_real_list;
            for (unsigned k = 0; k < 200; k++)
                run_case(real, W_FIND, 0x95000u + k);
            for (unsigned k = 0; k < 40; k++) {
                run_case(real, W_PATH, 0x95400u + k);
                run_case(real, W_SIZE, 0x95500u + k);
                run_case(real, W_LOADED, 0x95600u + k);
            }
            g_real = nullptr;
        }
        world_off();
    } else {
        all.check(false, "sound_list", "[the fake world could not be set up]");
    }
    log("  sound_list real data: %d records in the level's sound list", found);
    all.report();
    real.report();
}

}  // namespace zax
