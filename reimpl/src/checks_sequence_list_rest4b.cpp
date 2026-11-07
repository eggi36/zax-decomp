// Checks of sequence_list_rest4b.cpp against the original, in the game thread (machinery:
// checks_inventory_common.h: side 0 with the module's hooks out, an event log of the stand-ins).
//  - Slots 35 and 41 on fake lists: a vtable of recording slots 1 (a record's name), 5 (the folder text),
//    37 (the load: folder and name text, the object slot by offset) and 39; 0-6 records with random names,
//    objects (fake ones, some missing) and loading flags; folders empty, plain and ending in a separator;
//    fake objects whose slot 2 gives CSequence's class or CRenderablePolygon's, found, not found or a second
//    one; compared: al, the log, the records and the output string's text.
//  - Slot 0 on pool blocks of 0x158 bytes with SequenceList_Dtor 0x56c1a0 a recording stand-in, flags 0 and 1.
#include "checks_inventory_common.h"
#include "ai_common.h"

namespace zax {

int hooks_sequence_list_rest4b(Hook *out);

namespace {

using namespace invcheck;
using ai::zs_ctor;
using ai::zs_dtor;
using ai::zs_c_str;

void copy_text(char *out, const char *s, int max)
{
    int i = 0;
    for (; s[i] && i < max - 1; i++)
        out[i] = s[i];
    out[i] = 0;
}

DWORD rnd(DWORD n) { return n ? (random() >> 8) % n : 0; }

struct FakeObj {
    DWORD vtable;
    DWORD id;
};
DWORD g_seq_vt[8], g_other_vt[8];
CClassInfo *THISCALL s_seq_class(void *) { return at<CClassInfo *(__cdecl *)()>(0x56ab50)(); }
CClassInfo *THISCALL s_other_class(void *) { return at<CClassInfo *(__cdecl *)()>(0x55d2a0)(); }
FakeObj g_objs[8];

constexpr DWORD NREC = 6;
alignas(4) BYTE g_list[0x160];
alignas(4) BYTE g_records[NREC * 0x80];
DWORD g_list_vt[64];
const char *const NAMES[] = {"Water", "Lava/Hot", "", "Sub\\Dir/Name", "A rather long animated texture name"};
const char *const FOLDERS[] = {"Cache/Animated Textures", "", "Cache/", "x\\y"};
DWORD g_folder_pick;

const char *THISCALL s_name(void *, DWORD i)
{
    ev(0x101, i);
    return NAMES[i % 5];
}
const char *THISCALL s_folder(void *)
{
    ev(0x105);
    return FOLDERS[g_folder_pick];
}
DWORD THISCALL s_load(void *, const char *folder, const char *name, DWORD *slot)
{
    ev(0x137, fnv_text(2166136261u, folder), fnv_text(2166136261u, name), ptrv(slot) - ptrv(g_records));
    return 0x5e000000u + (DWORD)g_nlog[g_side];
}
DWORD THISCALL s_apply(void *, DWORD i, DWORD arg)
{
    ev(0x139, i, arg);
    return result(0x39);
}
DWORD THISCALL s_list_dtor(void *self)
{
    ev(0x56c1a0, ptrv(self) & 0xfff);
    return result(0xd7);
}

void make_case(unsigned seed)
{
    g_seed = seed;
    for (DWORD k = 0; k < 8; k++)
        g_objs[k] = {ptrv(k < 6 ? g_seq_vt : g_other_vt), k};
    memset(g_records, 0, sizeof g_records);
    const DWORD n = rnd(NREC + 1);
    for (DWORD i = 0; i < NREC; i++) {
        const char *nm = NAMES[rnd(5)];
        copy_text(reinterpret_cast<char *>(g_records + i * 0x80), nm, 0x78);
        const DWORD pick = rnd(10);
        *reinterpret_cast<DWORD *>(g_records + i * 0x80 + 0x78) = pick < 7 ? ptrv(&g_objs[pick]) : 0;
        g_records[i * 0x80 + 0x7f] = rnd(5) == 0 ? 1 : 0;
    }
    memset(g_list, 0, sizeof g_list);
    *reinterpret_cast<DWORD *>(g_list) = ptrv(g_list_vt);
    // no records only with a count of 0: both slots read the records before any test (a null with a count
    // would crash the original too)
    *reinterpret_cast<DWORD *>(g_list + 0x30) = n || rnd(4) ? ptrv(g_records) : 0;
    *reinterpret_cast<DWORD *>(g_list + 0x34) = n;
    g_folder_pick = rnd(4);
    copy_text(reinterpret_cast<char *>(g_list + 0x38), FOLDERS[rnd(4)], 0x40);
}

void check_slots(Tally &t)
{
    g_seq_vt[2] = ptrv((void *)&s_seq_class);
    g_other_vt[2] = ptrv((void *)&s_other_class);
    g_list_vt[1] = ptrv((void *)&s_name);
    g_list_vt[5] = ptrv((void *)&s_folder);
    g_list_vt[37] = ptrv((void *)&s_load);
    g_list_vt[39] = ptrv((void *)&s_apply);
    for (int k = 0; k < scaled(2000); k++) {
        const unsigned seed = 0x4ef000u + (unsigned)k;
        const bool path = k % 2 == 0;
        DWORD r[2];
        unsigned rec[2], text[2];
        for (int side = 0; side < 2; side++) {
            make_case(seed);
            const DWORD pick = rnd(8), arg = random();
            BYTE z[4];
            zs_ctor(z);
            log_reset(side);
            side_begin(side);
            if (path)
                r[side] = at<DWORD(THISCALL *)(void *, void *, void *)>(0x4ef000)(g_list, &g_objs[pick], z) & 0xff;
            else
                r[side] = at<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4f6ae0)(g_list, &g_objs[pick], arg) & 0xff;
            side_end(side);
            rec[side] = fnv(2166136261u, g_records, sizeof g_records);
            text[side] = fnv_text(2166136261u, zs_c_str(z));
            zs_dtor(z);
        }
        char d[160];
        wsprintfA(d, "[%s seed %u: al %x / %x, log %d / %d, records %x / %x, text %x / %x]", path ? "slot 41" : "slot 35", seed, r[0], r[1], g_nlog[0],
                  g_nlog[1], rec[0], rec[1], text[0], text[1]);
        const bool same = r[0] == r[1] && same_log() && rec[0] == rec[1] && text[0] == text[1];
        if (!same)
            log_first_difference();
        t.check(same, "", d);
    }
}

CodePatch g_dtor_patch;
void check_dtor(Tally &t)
{
    for (int k = 0; k < scaled(40); k++) {
        DWORD r[2], freed[2];
        const DWORD flags = (DWORD)k & 3;
        for (int side = 0; side < 2; side++) {
            BYTE *b = static_cast<BYTE *>(pool_alloc(0x158));
            log_reset(side);
            patch_jump(g_dtor_patch, 0x56c1a0, (void *)&s_list_dtor);
            side_begin(side);
            r[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x4ef170)(b, flags) == ptrv(b);
            side_end(side);
            unpatch(g_dtor_patch);
            // freed: the pool gives the block back first
            BYTE *again = static_cast<BYTE *>(pool_alloc(0x158));
            freed[side] = again == b;
            pool_free(again, 0x158);
            if (!(flags & 1))
                pool_free(b, 0x158);
        }
        char d[100];
        wsprintfA(d, "[flags %u: eax %u / %u, freed %u / %u]", flags, r[0], r[1], freed[0], freed[1]);
        t.check(r[0] && r[1] && freed[0] == freed[1] && same_log(), "", d);
    }
}

}  // namespace

ZAX_CHECKS(sequence_list_rest4b)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_sequence_list_rest4b;
    Tally slots{"sequence list: slots 35 and 41"};
    check_slots(slots);
    slots.report();
    Tally dtor{"sequence list: slot 0"};
    check_dtor(dtor);
    dtor.report();
}

}  // namespace zax
