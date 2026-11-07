// Checks of the sequence classes (sequence_classes.cpp) against the original, in the game thread.
//  - The classes CSequence, CRotationSequence, CVerticalCut (machinery: checks_inventory_common.h): Create on a
//    pool block filled with a pattern, CreateArray with 0-3 elements, the deleting destructors, the class
//    getters, RegisterFields into a fake class info and with the class variable 0, the registered descriptors.
//  - The level's real sequences (the loaded models' CRotationSequences, read only): Frame (every index and
//    past the end), FrameAt (ratios 0, 0.5, below 1, 1, 1.5, -0.25), MemorySize, the blend mode, both slot 23s.
//  - Fake sequences (a CRotationSequence of 0x6c bytes per side with fake frames: CFrame objects with a copy
//    of CFrame's vtable whose slots 1 and 21 record; frames, frame arrays, deltas and file buffers from the
//    heap, one set per side, compared by contents): Finish (slot 27, both classes; deltas apply on and off,
//    with and without a delta per frame), the X/Y offset setters, FreeFrames (with and without a file
//    buffer), ClearFramePointers, the destructors (no freeing), ReadOwnLine ("Frame Delta <n>", "Rotation
//    Offset", "Load Black Bias"; CObject's own names need a reader), WriteText (FileBuf_WriteLine a recording stand-in),
//    Reset (slot 19), the extension of the colour depth.
#include "checks_inventory_common.h"
#include "x87.h"
#include "checks_frame_r1.h"

namespace zax {

int hooks_sequence_classes(Hook *out);

namespace {

using namespace invcheck;

const ClassDef CLASSES[] = {
    {"CSequence", 0x38, 0x713378, 0x56aba0, 0x56abf0, 0x56ad20, {0x56ab50, 0x0}, true},
    {"CRotationSequence", 0x6c, 0x713140, 0x55eb40, 0x55ebb0, 0x55ed20, {0x55eae0, 0x55ecc0}, true},
    {"CVerticalCut", 0x10, 0x71313c, 0x55e8c0, 0x55e8f0, 0x55e9e0, {0x55e870, 0x0}, true},
};

template <typename R = DWORD, typename... A> R exe(DWORD a, void *self, A... args) { return at<R(THISCALL *)(void *, A...)>(a)(self, args...); }
DWORD rnd(DWORD n) { return n ? (random() >> 8) % n : 0; }
DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }
BYTE *bptr(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
DWORD &d32(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD fbits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}

// A record of a side: values compared after both sides
struct Rec {
    static constexpr int MAX = 512;
    DWORD v[MAX];
    int n;
    unsigned hash;
    void reset()
    {
        n = 0;
        hash = 2166136261u;
    }
    void add(DWORD x)
    {
        hash = fnv(hash, &x, 4);
        if (n < MAX)
            v[n++] = x;
    }
    void add_bytes(const void *p, DWORD bytes)
    {
        for (DWORD i = 0; i + 4 <= bytes; i += 4)
            add(*reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(p) + i));
    }
};
Rec g_rec[2];
int g_side;

// Both sides, side 0 with the module's hooks out; f(side) fills g_rec[side]
template <typename F> void two(Tally &t, const char *what, F f)
{
    for (int side = 0; side < 2; side++) {
        g_rec[side].reset();
        g_side = side;
        if (side == 0)
            hooks_out();
        f(side);
        if (side == 0)
            hooks_in();
    }
    const Rec &a = g_rec[0], &b = g_rec[1];
    if (a.n == b.n && a.hash == b.hash) {
        t.check(true, "");
        return;
    }
    int k = 0;
    while (k < a.n && k < b.n && a.v[k] == b.v[k])
        k++;
    char d[200];
    wsprintfA(d, "[%.100s: %d / %d values, first difference at %d: %x / %x]", what, a.n, b.n, k, k < a.n ? a.v[k] : 0, k < b.n ? b.v[k] : 0);
    t.check(false, "", d);
}

// ---- Real sequences ----------------------------------------------------------------------------------------

void check_real(Tally &t)
{
    BYTE *records = *at<BYTE **>(0x6cfe08);
    const DWORD nmodels = *at<DWORD *>(0x6cfe0c);
    if (!records) {
        t.check(false, "", "[no model list]");
        return;
    }
    int nseq = 0, idx = 0;
    // count the loaded models' sequences first, then a spread of at most 150
    for (DWORD m = 0; m < nmodels; m++) {
        BYTE *model = bptr(d32(records + m * 0x80, 0x78));
        if (!model || records[m * 0x80 + 0x7f] || !readable(model, 0x50))
            continue;
        nseq += (int)d32(model, 0x48);
    }
    int tested = 0;
    static const float ratios[6] = {0.0f, 0.5f, 0.99f, 1.0f, 1.5f, -0.25f};
    for (DWORD m = 0; m < nmodels; m++) {
        BYTE *model = bptr(d32(records + m * 0x80, 0x78));
        if (!model || records[m * 0x80 + 0x7f] || !readable(model, 0x50))
            continue;
        for (DWORD s = 0; s < d32(model, 0x48); s++) {
            BYTE *seq = bptr(reinterpret_cast<DWORD *>(bptr(d32(model, 0x4c)))[s]);
            if (!spread_pick(idx++, nseq, 150) || !seq || !readable(seq, 0x6c))
                continue;
            tested++;
            const DWORD n = d32(seq, 0x28);
            if (r1::g_frame_trace)
                log("  real sequence %d: %d frames", idx, n);
            two(t, "real sequence", [&](int side) {
                Rec &r = g_rec[side];
                for (DWORD i = 0; i <= n + 1 && i < 64; i++) {
                    r.add(exe(0x56bea0, seq, i));
                    r.add(exe(0x55eb30, seq, i));
                }
                if (n)
                    for (float q : ratios)
                        r.add(exe(0x56bec0, seq, q));
                r.add(at<DWORD(__fastcall *)(void *)>(0x56b660)(seq));
                r.add(at<DWORD(__fastcall *)(void *)>(0x55f890)(seq));
                r.add(at<DWORD(__fastcall *)(void *)>(0x56bf00)(seq));
                r.add(at<DWORD(__cdecl *)()>(0x56b2b0)());
            });
        }
    }
    log("  sequence classes: %d of %d real sequences", tested, nseq);
}

// ---- Fake sequences ----------------------------------------------------------------------------------------

DWORD g_frame_vtable[64];
DWORD THISCALL rec_frame_delete(BYTE *f, DWORD flags)
{
    g_rec[g_side].add(0xde1e0000u | (DWORD)f[0x3f]);
    g_rec[g_side].add(flags);
    return pv(f);
}
DWORD THISCALL rec_frame_size(BYTE *f) { return 0x100u + f[0x3f]; }
DWORD __cdecl rec_write_line_body(const char *s)
{
    g_rec[g_side].add(fnv_text(2166136261u, s));
    return 1;
}
DWORD THISCALL rec_write_line(void *, const char *s) { return rec_write_line_body(s); }

constexpr int NFRAMES = 12;
struct Fake {
    BYTE seq[0x6c];
    BYTE *frames[NFRAMES];   // heap CFrame objects (0x40), byte 0x3f their number
    DWORD *array;            // the frame array (heap)
    DWORD *deltas;           // heap
    BYTE *buffer;            // heap file buffer or 0
    bool array_attached, deltas_attached;
};
Fake g_fake[2];

// Builds the same fake sequence on both sides from the case seed: n frames (some empty), rotations
void build(unsigned seed, int n, int rotations, bool deltas, bool buffer)
{
    for (int side = 0; side < 2; side++) {
        g_seed = seed;
        Fake &f = g_fake[side];
        memset(f.seq, 0, sizeof f.seq);
        d32(f.seq, 0) = 0x5fec14;
        d32(f.seq, 0x1c) = rnd(0x800);
        d32(f.seq, 0x20) = rnd(4);
        d32(f.seq, 0x24) = fbits(30.0f);
        d32(f.seq, 0x28) = (DWORD)n;
        d32(f.seq, 0x2c) = (DWORD)(int)(rnd(21) - 10);
        d32(f.seq, 0x30) = (DWORD)(int)(rnd(21) - 10);
        d32(f.seq, 0x4c) = (DWORD)rotations;
        d32(f.seq, 0x50) = 0xffffffffu;
        f.array = static_cast<DWORD *>(heap_alloc(NFRAMES * 4));
        f.array_attached = n || rnd(2);
        d32(f.seq, 8) = f.array_attached ? pv(f.array) : 0;
        for (int i = 0; i < NFRAMES; i++) {
            BYTE *fr = static_cast<BYTE *>(heap_alloc(0x40));
            memset(fr, 0, 0x40);
            d32(fr, 0) = pv(g_frame_vtable);
            *reinterpret_cast<short *>(fr + 8) = (short)(int)(rnd(41) - 20);
            *reinterpret_cast<short *>(fr + 0xa) = (short)(int)(rnd(41) - 20);
            *reinterpret_cast<WORD *>(fr + 0x12) = (WORD)(1 + rnd(60));
            *reinterpret_cast<WORD *>(fr + 0x14) = (WORD)(1 + rnd(60));
            d32(fr, 0xc) = 0x1000 + (DWORD)i;   // block pointers (never read: ClearPointers clears them)
            d32(fr, 0x18) = 0x2000 + (DWORD)i;
            d32(fr, 0x3c) = 0x40;
            fr[0x3f] = (BYTE)i;
            f.frames[i] = fr;
            f.array[i] = rnd(6) ? pv(fr) : 0;
        }
        f.deltas = static_cast<DWORD *>(heap_alloc(NFRAMES * 8 + 8));
        for (int i = 0; i < NFRAMES * 2; i++)
            f.deltas[i] = (DWORD)(int)(rnd(9) - 4);
        f.deltas_attached = deltas;
        d32(f.seq, 0x44) = deltas ? pv(f.deltas) : 0;
        d32(f.seq, 0x64) = deltas ? (DWORD)(n ? n - (int)rnd(2) : 0) : 0;   // a delta count needs the array
        f.buffer = buffer ? static_cast<BYTE *>(heap_alloc(16)) : nullptr;
        d32(f.seq, 0x34) = pv(f.buffer);
    }
}
// The state of a side's fake by contents (heap blocks by content, not address)
int g_op;
void record(int side)
{
    Rec &r = g_rec[side];
    Fake &f = g_fake[side];
    for (DWORD off = 0; off < 0x6c; off += 4) {
        const DWORD v = d32(f.seq, off);
        if (off == 8)
            r.add(v == pv(f.array) ? 0xa11a : v ? 0xbad : 0);
        else if (off == 0x34)
            r.add(v == pv(f.buffer) && v ? 0xb0ff : v ? 0xbad : 0);
        else if (off == 0x44)
            r.add(v ? 0xde17a : 0);
        else
            r.add(v);
    }
    for (int i = 0; i < NFRAMES; i++)
        r.add_bytes(f.frames[i], 0x40);
    if (d32(f.seq, 8) == pv(f.array))
        for (int i = 0; i < NFRAMES; i++)
            r.add(f.array[i] ? 0xf000u + bptr(f.array[i])[0x3f] : 0);
    if (DWORD *d = reinterpret_cast<DWORD *>(bptr(d32(f.seq, 0x44)))) {
        const DWORD mx = d32(f.seq, 0x64) > d32(f.seq, 0x28) ? d32(f.seq, 0x64) : d32(f.seq, 0x28);
        const DWORD n = pv(d) == pv(f.deltas) ? mx : g_op == 7 ? d32(f.seq, 0x64) : d32(f.seq, 0x28);
        for (DWORD i = 0; i < n * 2 && i < 64; i++)
            r.add(d[i]);
    }
}
// What the case left on the heap freed (the frames, the frame array, deltas and buffer when still owned)
void release(int side)
{
    Fake &f = g_fake[side];
    for (int i = 0; i < NFRAMES; i++)
        heap_free(f.frames[i]);
    if (!f.array_attached || d32(f.seq, 8) == pv(f.array))
        heap_free(f.array);
    if (d32(f.seq, 0x44) && d32(f.seq, 0x44) != pv(f.deltas))
        heap_free(bptr(d32(f.seq, 0x44)));
    if (!f.deltas_attached || d32(f.seq, 0x44) == pv(f.deltas))
        heap_free(f.deltas);
    if (f.buffer && d32(f.seq, 0x34) == pv(f.buffer))
        heap_free(f.buffer);
}

void check_fakes(Tally &t)
{
    memcpy(g_frame_vtable, at<void *>(0x5f4814), sizeof g_frame_vtable);
    g_frame_vtable[1] = pv((void *)&rec_frame_delete);
    g_frame_vtable[21] = pv((void *)&rec_frame_size);
    CodePatch line;
    patch_jump(line, 0x4a1a40, (void *)&rec_write_line);
    BYTE *apply = at<BYTE *>(0x62067c);
    const BYTE apply_saved = *apply;
    g_seed = 0x5600f0;
    static const char *const NAMES[11] = {"CSequence::Finish", "CRotationSequence::Finish", "SetXOffset", "SetYOffset", "FreeFrames",
                                          "ClearFramePointers", "CSequence::Dtor", "CRotationSequence::ReadOwnLine", "WriteText",
                                          "CRotationSequence::Reset", "MemorySize"};
    for (int k = 0; k < scaled(1100); k++) {
        const int op = k % 11;
        const unsigned seed = random();
        const int rotations = 1 + (int)rnd(4);
        const int n = (int)(rnd(NFRAMES / rotations + 1)) * rotations;
        const bool with_deltas = rnd(3) != 0, buffer = op == 4 ? rnd(2) != 0 : false;
        const BYTE apply_case = (BYTE)rnd(2);
        build(seed, n, rotations, with_deltas, buffer);
        const int value = (int)rnd(41) - 20;
        char name[48], text[48];
        switch (rnd(5)) {
        case 0: wsprintfA(name, "Frame Delta %d", (int)rnd(14)); wsprintfA(text, "%d, %d", (int)rnd(21) - 10, (int)rnd(21) - 10); break;
        case 1: lstrcpyA(name, "Rotation Offset"); wsprintfA(text, "%d.%d", (int)rnd(400) - 200, (int)rnd(10)); break;
        case 2: lstrcpyA(name, "Load Black Bias"); wsprintfA(text, "%d", (int)rnd(3)); break;
        case 3: lstrcpyA(name, "frame delta 2"); lstrcpyA(text, "7,8"); break;
        default: wsprintfA(name, "Frame Delta %d", (int)rnd(4)); lstrcpyA(text, "-3, 4"); break;   // (CObject's ReadOwnLine needs a reader)
        }
        g_op = op;
        if (r1::g_frame_trace)
            log("  fake case %d op %d (%d frames, %d rotations)", k, op, n, rotations);
        char what[80];
        wsprintfA(what, "%s case %d (%d frames, %d rotations)", NAMES[op], k, n, rotations);
        two(t, what, [&](int side) {
            BYTE *s = g_fake[side].seq;
            *apply = apply_case;
            Rec &r = g_rec[side];
            switch (op) {
            case 0: at<void(__fastcall *)(void *)>(0x56b570)(s); break;
            case 1: at<void(__fastcall *)(void *)>(0x5600f0)(s); r.add(*apply); break;
            case 2: r.add(exe(0x56bca0, s, value)); break;
            case 3: r.add(exe(0x56bda0, s, value)); break;
            case 4: exe(0x56b6a0, s); break;
            case 5: exe(0x56bb40, s); break;
            case 6:
                exe(0x56b760, s);
                d32(s, 0) = 0x5fec14;   // CObject's destructor leaves the vtable; put the class back for the record
                break;
            case 7: exe(0x55fa10, s, (const char *)name, (const char *)text, 0u); break;
            case 8:
                if (d32(s, 0x44))
                    exe(0x55f9b0, s, (void *)0x1234);
                break;
            case 9: r.add(exe(0x55f8e0, s) & 0xff); break;
            default:
                r.add(at<DWORD(__fastcall *)(void *)>(0x55f890)(s));
                r.add(at<DWORD(__fastcall *)(void *)>(0x56b660)(s));
                break;
            }
            record(side);
            release(side);
        });
    }
    *apply = apply_saved;
    unpatch(line);
}

}  // namespace

ZAX_CHECKS(sequence_classes)
{
    (void)cls;
    (void)n;
    r1::read_frame_trace();
    g_hook_table = &hooks_sequence_classes;
    if (!arena_setup()) {
        log("Check sequence classes: no arena");
        return;
    }
    Tally objects{"sequence classes: Create, CreateArray, destructors"}, getters{"sequence classes: class getters"},
        fields{"sequence classes: RegisterFields"}, registered{"sequence classes: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    objects.report();
    getters.report();
    fields.report();
    registered.report();
    Tally real{"sequence classes: real sequences (frames, ratio, size, blend mode)"};
    check_real(real);
    real.report();
    Tally fakes{"sequence classes: fake sequences (finish, offsets, frames, deltas, lines)"};
    check_fakes(fakes);
    fakes.report();
}

}  // namespace zax
