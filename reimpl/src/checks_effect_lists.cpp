// Checks of effect_lists.cpp against the original, in the game thread.
//  - The two lists' slots on fake lists of the check's own (0x140 bytes, a copy of the list's vtable whose
//    slots 1 (an entry's name) and 5 (the folder) are recorders; 0-5 entries in a heap block, their objects
//    fakes with a recording slot 1, real objects of the list's class made by its Create, or none; a fake
//    monitor with a recording slot 7; RecordList_Set and the aliases' destructor recording stand-ins): the
//    deleting destructor (flags 0 and 1), the file of an object (an entry's object, another object of the
//    class, an object of another class), an entry's path, the object a file makes and 0x598910 (the file
//    opening Object_CreateFromFile a recording stand-in answering 0, a CWaterWarpEffectFrame or a
//    CSimpleWarpEffect): the calls, eax, the output text, the list's bytes.
//  - The image field's text: 0x4b5b20 and 0x4b59f0 on real CFrames (8 and 16 bits, sizes 0-9 × 0-6, random
//    pixels) inside objects of the check's own, the file's line functions recording stand-ins (written
//    lines hashed; read lines from a script: what the writer wrote, broken first lines, a "}" early, short
//    hex, leading spaces and tabs): eax, the lines, the frame's size, flags and pixels.
#include "checks_inventory_common.h"
#include "ai_common.h"
#include "definitions.h"

namespace zax {

int hooks_effect_lists(Hook *out);

namespace {

using namespace ai;
using invcheck::ev;
using invcheck::ptrv;
using Create0 = void *(__cdecl *)();
DWORD rnd(DWORD n) { return n ? random() % n : 0; }
unsigned text_hash(const char *s) { return s ? fnv(2166136261u, s, (DWORD)lstrlenA(s)) : 0; }
const char *zs_text(const ZString *z) { return at<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }

struct ListKind {
    const char *name;
    DWORD vtable, dtor, create_from_file, file_of, entry_path, object_create;
};
const ListKind LISTS[2] = {
    {"Frame Overlay Options", 0x5f4b80, 0x4b5490, 0x4b56d0, 0x4b5750, 0x4b57f0, 0x4b4fe0},
    {"Water Warp Effects", 0x602a88, 0x598ac0, 0x598b70, 0x598bf0, 0x598ce0, 0x597ca0},
};

// ---- Recorders --------------------------------------------------------------------------------------------------

const char *const NAMES[] = {"Torch", "", "Sub/Deep Name", "Water Fall"};
const char *THISCALL rec_name(void *, DWORD i)
{
    ev('n', i);
    return NAMES[i & 3];
}
const char *THISCALL rec_folder(void *)
{
    ev('f');
    return "Cache/Some Folder";
}
DWORD THISCALL rec_set(void *l, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u)
{
    ev('s', ptrv(l) & 0xfff, sorted | stride << 8, num ^ ptrv(data) ^ u);
    return invcheck::result(1);
}
DWORD THISCALL rec_aliases(void *a) { ev('a', ptrv(a) & 0xfff); return invcheck::result(2); }
void *THISCALL rec_obj_delete(void *o, DWORD f) { ev('d', ptrv(o) & 0xffff, f); return o; }
DWORD THISCALL rec_monitor(void *, DWORD f) { ev('m', f); return invcheck::result(3); }
DWORD g_fake_vt[64], g_monitor_vt[64], g_list_vt[64];
DWORD g_fakes[6][2], g_monitor[2];
void *g_answer;
void *__fastcall rec_create_from_file(ZString *path, DWORD flags, DWORD write_back, void **out)
{
    ev('c', text_hash(zs_text(path)), flags, write_back);
    ev('o', out ? 1 : 0);
    return g_answer;
}

struct Patches {
    CodePatch p[8];
    int n = 0;
    void add(DWORD a, void *f) { patch_jump(p[n++], a, f); }
    void clear()
    {
        while (n)
            unpatch(p[--n]);
    }
};

alignas(16) BYTE g_list_buffer[0x140];
BYTE *g_list = g_list_buffer;   // a pool block for the deleting destructor with flag 1
BYTE *g_real_objects[2][6];
ZString g_out;

// A fake list for a case: the entries block (heap) with n records whose objects are of `kinds`
// (0 none, 1 a fake, 2 a real object of the class)
void build_list(const ListKind &k, DWORD n, const BYTE *kinds, int side, bool monitor)
{
    memset(g_list, 0, 0x140);
    dw(g_list, 0) = ptrv(g_list_vt);
    if (n) {
        DWORD *block = static_cast<DWORD *>(heap_alloc(n * 0x80 + 4));
        block[0] = n;
        BYTE *e = reinterpret_cast<BYTE *>(block + 1);
        memset(e, 0, n * 0x80);
        for (DWORD i = 0; i < n; i++) {
            lstrcpyA(reinterpret_cast<char *>(e + i * 0x80), NAMES[i & 3]);
            dw(e + i * 0x80, 0x78) = kinds[i] == 1 ? ptrv(g_fakes[i]) : kinds[i] == 2 ? ptrv(g_real_objects[side][i]) : 0;
        }
        dw(g_list, 0x30) = ptrv(e);
    }
    dw(g_list, 0x34) = n;
    lstrcpyA(reinterpret_cast<char *>(g_list + 0x38), "Cache/Fake List");
    dw(g_list, 0x13c) = monitor ? ptrv(g_monitor) : 0;
    (void)k;
}

void check_lists(Tally &t)
{
    for (int i = 0; i < 64; i++) {
        g_fake_vt[i] = ptrv((void *)&rec_obj_delete);
        g_monitor_vt[i] = ptrv((void *)&rec_monitor);
    }
    g_fake_vt[2] = 0x4163b0;   // the class (CObject's getter) for IsKindOf
    for (auto &f : g_fakes)
        f[0] = ptrv(g_fake_vt);
    g_monitor[0] = ptrv(g_monitor_vt);
    g_seed = 0x4b5490;
    for (int c = 0; c < scaled(400); c++) {
        const ListKind &k = LISTS[rnd(2)];
        memcpy(g_list_vt, at<void *>(k.vtable), sizeof g_list_vt);
        g_list_vt[1] = ptrv((void *)&rec_name);
        g_list_vt[5] = ptrv((void *)&rec_folder);
        const DWORD n = rnd(6);
        BYTE kinds[6];
        for (BYTE &b : kinds)
            b = (BYTE)rnd(3);
        const int what = (int)rnd(5);
        const DWORD flags = rnd(2), index = rnd(7);
        const int pick = (int)rnd(4);   // the object for the file of an object
        const int answer = (int)rnd(3);
        const bool monitor = rnd(2) != 0, with_dir = rnd(2) != 0, with_out = rnd(2) != 0, with_name = rnd(2) != 0;
        const DWORD entry_pick = n ? rnd(n) : 0;
        DWORD eax[2];
        unsigned h[2];
        BYTE after[2][0x140];
        for (int side = 0; side < 2; side++) {
            for (DWORD i = 0; i < 6; i++)
                g_real_objects[side][i] = static_cast<BYTE *>(original<Create0>(k.object_create)());
            const bool pooled = what == 0 && flags;
            g_list = pooled ? static_cast<BYTE *>(pool_alloc(0x140)) : g_list_buffer;
            build_list(k, n, kinds, side, monitor);
            BYTE *other = static_cast<BYTE *>(original<Create0>(k.object_create == 0x4b4fe0 ? 0x597ca0 : 0x4b4fe0)());
            invcheck::log_reset(side);
            at<void *(THISCALL *)(void *)>(0x4dec90)(&g_out);
            Patches p;
            p.add(0x591cd0, (void *)&rec_set);
            p.add(0x55d0c0, (void *)&rec_aliases);
            p.add(0x4164c0, (void *)&rec_create_from_file);
            BYTE *wanted = static_cast<BYTE *>(original<Create0>(0x597ca0)());
            BYTE *simple = static_cast<BYTE *>(original<Create0>(0x571920)());
            g_answer = answer == 0 ? nullptr : answer == 1 ? (void *)wanted : (void *)simple;
            void *out_slot = nullptr;
            invcheck::side_begin(side);
            switch (what) {
            case 0:
                eax[side] = ptrv(at<void *(THISCALL *)(void *, DWORD)>(k.dtor)(g_list, flags)) - ptrv(g_list);
                for (DWORD i = 0; i < n; i++)   // deleted by the destructor
                    if (kinds[i] == 2)
                        g_real_objects[side][i] = nullptr;
                break;
            case 1: {
                void *o = pick == 0 && n ? bp(dw(bp(dw(g_list, 0x30)), 0x80 * entry_pick + 0x78)) : pick == 1 ? (void *)g_real_objects[side][5] : pick == 2 ? (void *)other : nullptr;
                eax[side] = o ? at<DWORD(THISCALL *)(void *, void *, ZString *)>(k.file_of)(g_list, o, &g_out) : 0;
                break;
            }
            case 2:
                eax[side] = at<DWORD(THISCALL *)(void *, DWORD, ZString *)>(k.entry_path)(g_list, index, &g_out);
                break;
            case 3:
                eax[side] = ptrv(at<void *(THISCALL *)(void *, const char *, const char *, void **)>(k.create_from_file)(g_list, with_dir ? "Dir/Sub" : "", "Name",
                                                                                                                    with_out ? &out_slot : nullptr));
                break;
            default:
                eax[side] = ptrv(at<void *(THISCALL *)(void *, const char *, const char *, void **)>(0x598910)(g_list, "Effects", with_name ? "Lava" : "",
                                                                                                         with_out ? &out_slot : nullptr));
            }
            invcheck::side_end(side);
            p.clear();
            // eax: pointers to the answers named
            if (what >= 3)
                eax[side] = eax[side] == ptrv(wanted) ? 1 : eax[side] == ptrv(simple) ? 2 : eax[side];
            h[side] = text_hash(zs_text(&g_out));
            if (pooled) {   // freed by the destructor: its bytes are the pool's now
                memset(after[side], 0, 0x140);
                g_list = g_list_buffer;
                memset(g_list, 0, 0x140);
            }
            memcpy(after[side], g_list, 0x140);
            // the records block when the slot left it
            if (dw(g_list, 0x30)) {
                const DWORD *block = reinterpret_cast<DWORD *>(bp(dw(g_list, 0x30))) - 1;
                for (DWORD i = 0; i < block[0]; i++) {   // the records without their object pointers, the objects named
                    h[side] = fnv(h[side], bp(dw(g_list, 0x30)) + i * 0x80, 0x78);
                    h[side] = fnv(h[side], bp(dw(g_list, 0x30)) + i * 0x80 + 0x7c, 4);
                    const DWORD o = dw(bp(dw(g_list, 0x30)), i * 0x80 + 0x78);
                    DWORD id = 0;
                    for (DWORD j = 0; j < 6; j++)
                        if (o == ptrv(g_real_objects[side][j]))
                            id = 0x100 + j;
                        else if (o == ptrv(g_fakes[j]))
                            id = 0x200 + j;
                    h[side] = fnv(h[side], &id, 4);
                }
                heap_free(const_cast<DWORD *>(block));
            }
            dw(after[side], 0x30) = dw(after[side], 0x30) ? 1 : 0;
            if (dw(after[side], 0) == ptrv(g_list_vt))
                dw(after[side], 0) = 1;
            at<void(THISCALL *)(void *)>(0x4defd0)(&g_out);
            for (BYTE *o : g_real_objects[side])
                if (o)
                    vcall<void *>(o, 1, 1);
            vcall<void *>(other, 1, 1);
            vcall<void *>(wanted, 1, 1);
            vcall<void *>(simple, 1, 1);
        }
        char d[128];
        wsprintfA(d, "[%s case %d (%d), %u entries: eax %x / %x, out %x / %x]", k.name, c, what, n, eax[0], eax[1], h[0], h[1]);
        t.check(invcheck::same_log() && eax[0] == eax[1] && h[0] == h[1] && !memcmp(after[0], after[1], 0x140), "", d);
    }
}

// ---- The image field's text ---------------------------------------------------------------------------------------

const char *g_script[64];
int g_nscript, g_pos;
char g_written[2][0x4000];
int g_nwritten[2];
DWORD THISCALL rec_read_line(void *, char *buf, DWORD n)
{
    const char *s = g_pos < g_nscript ? g_script[g_pos] : "";
    g_pos++;
    DWORD i = 0;
    for (; s[i] && i + 1 < n; i++)
        buf[i] = s[i];
    if (n)
        buf[i] = 0;
    ev('r', text_hash(buf), n);
    return invcheck::result(g_pos < g_nscript + 1);
}
DWORD THISCALL rec_write_line(void *, const char *s)
{
    ev('w', text_hash(s));
    int &k = g_nwritten[invcheck::g_side];
    const int len = lstrlenA(s);
    if (k + len + 1 < (int)sizeof g_written[0]) {
        lstrcpyA(g_written[invcheck::g_side] + k, s);
        k += len + 1;
    }
    return invcheck::result(4);
}
DWORD THISCALL rec_indent(void *) { ev('i'); return invcheck::result(5); }
DWORD THISCALL rec_unindent(void *) { ev('u'); return invcheck::result(6); }

char g_lines[64][0x80];
BYTE *new_frame(DWORD w, DWORD h, DWORD depth)
{
    BYTE *f = static_cast<BYTE *>(pool_alloc(0x40));
    at<void *(THISCALL *)(void *)>(0x4a8a90)(f);
    at<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(f, w, h, depth);
    return f;
}

void check_image_text(Tally &t)
{
    g_seed = 0x4b5b20;
    for (int c = 0; c < scaled(200); c++) {
        const DWORD depth = rnd(2) ? 8 : 16, w = rnd(10), h = rnd(7);
        const bool write = rnd(2) == 0;
        // the script for reading: a header (also broken ones), "{", rows (some short, with spaces or
        // tabs, a "}" early), "}"
        g_nscript = 0;
        const DWORD rw = rnd(10), rh = rnd(7), rd = rnd(3) ? (rnd(2) ? 8 : 16) : 24;
        const int head = (int)rnd(6);
        wsprintfA(g_lines[0], head == 0 ? "CFrame(%d,%d)" : head == 1 ? "Frame(%d,%d,%d)" : "CFrame(%d,%d,%d)", rw, rh, rd);
        g_script[g_nscript++] = g_lines[0];
        g_script[g_nscript++] = "{";
        for (DWORD y = 0; y < rh && g_nscript < 60; y++) {
            char *l = g_lines[g_nscript];
            const DWORD r = rnd(12);
            if (r == 0)
                lstrcpyA(l, "}");
            else {
                int at_ = 0;
                if (r < 3)
                    l[at_++] = r == 1 ? ' ' : '\t';
                const DWORD digits = rnd(8) ? rw * (rd / 8) * 2 : rnd(20);
                for (DWORD i = 0; i < digits && at_ < 0x7e; i++)
                    l[at_++] = "0123456789abcdefABCDEF"[rnd(22)];
                l[at_] = 0;
            }
            g_script[g_nscript++] = l;
        }
        g_script[g_nscript++] = "}";
        BYTE pixels[9 * 6 * 2];
        for (BYTE &b : pixels)
            b = (BYTE)random();
        DWORD eax[2];
        unsigned h_[2];
        for (int side = 0; side < 2; side++) {
            BYTE *f = new_frame(w, h, depth);
            if (BYTE *px = bp(dw(f, 0xc)))
                memcpy(px, pixels, w * h * (depth / 8));
            BYTE desc[0x40] = {};
            dw(desc, 0x10) = 0;
            invcheck::log_reset(side);
            g_pos = 0;
            g_nwritten[side] = 0;
            Patches p;
            p.add(0x4a20a0, (void *)&rec_read_line);
            p.add(0x4a1a40, (void *)&rec_write_line);
            p.add(0x4a1ab0, (void *)&rec_indent);
            p.add(0x4a1ae0, (void *)&rec_unindent);
            invcheck::side_begin(side);
            // the descriptor's offset 0: the frame is the "object"
            if (write)
                eax[side] = at<DWORD(THISCALL *)(void *, void *, void *)>(0x4b5b20)(desc, f, nullptr);
            else
                eax[side] = at<DWORD(THISCALL *)(void *, void *, const char *, void *)>(0x4b59f0)(desc, f, g_script[0], nullptr);
            invcheck::side_end(side);
            p.clear();
            unsigned hh = fnv(2166136261u, f + 0x10, 6);
            hh = fnv(hh, f + 0x3c, 4);
            if (BYTE *px = bp(dw(f, 0xc)))
                hh = fnv(hh, px, (DWORD)*reinterpret_cast<WORD *>(f + 0x12) * *reinterpret_cast<WORD *>(f + 0x14) * vcall<DWORD>(f, 30));
            h_[side] = hh;
            vcall<void *>(f, 1, 1);
        }
        char d[128];
        wsprintfA(d, "[image %s case %d, %ux%u/%u, read %ux%u/%u head %d: eax %x / %x, frame %x / %x]", write ? "write" : "read", c, w, h, depth, rw, rh, rd,
                  head, eax[0], eax[1], h_[0], h_[1]);
        t.check(invcheck::same_log() && eax[0] == eax[1] && h_[0] == h_[1] && g_nwritten[0] == g_nwritten[1] &&
                    !memcmp(g_written[0], g_written[1], (DWORD)g_nwritten[0]),
                "", d);
    }
}

}  // namespace

ZAX_CHECKS(effect_lists)
{
    (void)cls;
    (void)n;
    invcheck::g_hook_table = &hooks_effect_lists;
    Tally lists{"effect lists: the list slots"}, image{"effect lists: the image field's text"};
    check_lists(lists);
    check_image_text(image);
    lists.report();
    image.report();
}

}  // namespace zax
