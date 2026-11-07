// The game's random generator and small helpers of 0x51ea10-0x51ee30 and 0x55c100-0x55e4a0 reimplemented.
//
// Random generator [confirmed: code; the R250 shift register generator of Kirkpatrick and Stoll]
// (0x400 bytes): +0 a byte set to 1 by the constructor, +4 the number of draws since the seed, +8 and
// +0xc the seed (+0xc the LCG state while seeding), +0x10 and +0x14 two indices into the table, +0x18
// 250 dwords. Seeding fills the table from its end with the LCG x = x * 0x41c64e6d + 0x3039 (each entry
// the bits 16-47 of the 64-bit sum), then puts a diagonal of 32 bits into every 7th entry from entry 3,
// so the 32 entries are linearly independent. A draw steps both indices through the successor table
// 0x620004 (i -> i + 1 mod 250, j -> j + 1 mod 250 with j = i + 103 at the seed) and xors entry j into
// entry i, which is the result. The game has three generators: 0x7124c0 (the game's, "GameRandom"),
// 0x6d06c0 and 0x712cc0 (CSayQuip's, the multiplayer's); the static initializers of the first and the
// last seed them with timeGetTime. Every constructor also fills the table 0x7128c0 (256 dwords): the
// number of bits of the index (0 for 0, 8 for 255). Names ours.
//
// The other helpers [confirmed: code and callers; names ours where the exe gives none]:
//  - 0x55d030: the colour depth: the error reporter's +0x24, 8 without a reporter.
//  - 0x55d050: calls a member function (thiscall, no arguments) on ecx; eax ecx.
//  - 0x55d060 (static initializer): the message id of "CReinforcementsRequestMessage" into 0x7130c0.
//  - 0x55d070 (fastcall: part, value; one stack argument): a reinforcements request message (vtable
//    0x5fe8fc, the id, a byte 0, the part, the value) sent to the parts in an oval (0x4fb9e0 with the
//    radius, the category bit 0x800000 and the part's team +0x1c low word).
//  - 0x55e090: a definition list's record loaded (the list's slots 1 and 37 like LoadEntry, but without
//    the range test and without the "already loaded" test); eax the object (or the records / 0).
//  - CRenderEffect (0x8 bytes, vtable 0x5fea64): constructor 0x55e270, deleting destructor 0x55e290,
//    destructor 0x55e2c0, its class getters 0x55e1c0 and 0x55e210 (version 1, abstract, the shared
//    Create 0x46b760, CreateArray 0x52dc80 and RegisterFields 0x58ea40); the shared scratch buffer of
//    the effects 0x713108 (data, capacity 0x71310c, size 0x713110, in use 0x713114): 0x55e2f0 (fastcall
//    size) makes it at least that big (doubling) and marks it in use, 0x55e2e0 releases it; 0x55e150 and
//    0x55e180 are its static initializer and exit function.
//  - CRenderEffectBehavior (0xc bytes, vtable 0x5fead4, parent CEntityBehavior): the class getters
//    0x55e350/0x55e3a0, the deleting destructor 0x55e420 and destructor 0x55e450 (its constructor
//    0x55e400 is in ai_classes.cpp).
//  - 0x55e470: a block reader (vtable 0x5feb4c, BlockReader_Ctor 0x51f8d0).
//  - 0x55e490 / 0x55e4a0: put ecx into 0x713128 / take it out again (eax the value, the global 0).
//  - Multiplayer [confirmed: code; the classes' names unknown]: a node list of names (vtables 0x5fb47c
//    and 0x5fb480, 0xc bytes: +4 a copy of the name from the heap, +8 the next node, owned): the
//    constructors 0x51ea10/0x51eb40 (name, next), deleting destructors 0x51ea80/0x51ebb0 and destructors
//    0x51ead0/0x51ec00 (the rest of the list through its slot 0 first), 0x51eb10 the first node whose name
//    contains a text (0x587c70, case-insensitive) along the list from ecx; 0x51ec40 the base vtable 0x5fb484 and its
//    deleting destructor 0x51ec50; 0x51ec70 a new named value (NamedValue_Ctor 0x51f700); 0x51ec90 a call
//    through the function pointer 0x6d00a8 (set by 0x51eca0); a 0x34-byte object (vtable 0x5fb4dc): the
//    constructor 0x51ecb0 (owner; two pointer arrays 0x5fb558/0x5fb54c at +0x10/+0x20, a ZString at
//    +0x30, the owner's +4 set to it), the deleting destructor 0x51ed00 (the arrays' elements deleted
//    through slot 0 from the last, each taken out with memmove; the owner's back pointer cleared) and
//    0x51ee30 (the same code without the delete).
#include "ai_helpers.h"

namespace zax {

bool g_truecolor_data16;   // [Reimpl] TrueColorData16=1 (reimpl.h; set by main.cpp)

using namespace ai;

namespace {

constexpr DWORD NEXT_INDEX = 0x620004;   // successor indices of the table (250 entries)
constexpr DWORD BITS_TABLE = 0x7128c0;   // 256 dwords: the bit count of the index
constexpr DWORD C_RANDOM_32 = 0x5fe8f0;  // 2^-32
constexpr DWORD C_RANDOM_31 = 0x5fe8f4;  // 2^-31
constexpr DWORD C_MINUS_ONE = 0x5eeccc;  // -1

CClassInfo *__cdecl object_class() { return zax::fn<Getter>(0x4163b0)(); }
DWORD time_get_time() { return zax::fn<DWORD(__cdecl *)()>(0x4d2460)(); }

// The table filled from the seed in +0xc: from its last entry down, the LCG's bits 16-47
void seed_table(BYTE *g)
{
    for (DWORD off = 0x3fc; off >= 0x18; off -= 4) {
        const unsigned long long s = (unsigned long long)dw(g, 0xc) * 0x41c64e6du + 0x3039u;
        dw(g, 0xc) = (DWORD)s;
        dw(g, off) = (DWORD)(s >> 16);
    }
}
// Every 7th entry from entry 3: the bits above the diagonal cleared, the diagonal bit set
void diagonal(BYTE *g)
{
    DWORD keep = 0xffffffffu, bit = 0x80000000u;
    for (DWORD off = 0x24; bit; off += 0x1c, keep >>= 1, bit >>= 1)
        dw(g, off) = (dw(g, off) & keep) | bit;
}
void fill_bit_counts()
{
    for (DWORD i = 0; i < 256; i++) {
        DWORD n = 0;
        for (DWORD v = i; v; v >>= 1)
            n++;
        glob<DWORD *>(BITS_TABLE)[i] = n;
    }
}
// The seeding the constructor and the static initializers do: time, both indices, the table
void seed_from_time(BYTE *g)
{
    fl<BYTE>(g, 0) = 1;
    dw(g, 4) = 0;
    const DWORD t = time_get_time();
    dw(g, 4) = 0;
    dw(g, 8) = t;
    dw(g, 0xc) = t;
    dw(g, 0x10) = 0;
    dw(g, 0x14) = 0x67;
    seed_table(g);
    diagonal(g);
    fill_bit_counts();
}
// One draw: both indices one step on, entry i xored with entry j
DWORD draw(BYTE *g)
{
    dw(g, 4)++;
    const DWORD i = dw(g, 0x10);
    dw(g, 0x10) = glob<const DWORD *>(NEXT_INDEX)[i];
    const DWORD j = dw(g, 0x14);
    dw(g, 0x14) = glob<const DWORD *>(NEXT_INDEX)[j];
    const DWORD v = dw(g, 0x18 + i * 4) ^ dw(g, 0x18 + j * 4);
    dw(g, 0x18 + i * 4) = v;
    return v;
}

}  // namespace

// ---- The random generator ------------------------------------------------------------------------------

// 0x55c100, 0x55c1e0 (static initializers): the generators 0x7124c0 and 0x712cc0 seeded from the time.
// eax: the end of the bit count table.
static DWORD __cdecl Random_InitGame()
{
    seed_from_time(glob<BYTE *>(GAME_RANDOM));
    return glob_addr(BITS_TABLE) + 0x400;
}
static DWORD __cdecl Random_InitThird()
{
    seed_from_time(glob<BYTE *>(RANDOM3));
    return glob_addr(BITS_TABLE) + 0x400;
}
// 0x55c2c0: the constructor (seeded from the time). eax: this.
static BYTE *THISCALL Random_Ctor(BYTE *g)
{
    seed_from_time(g);
    return g;
}
// 0x55c390: seeded with a value (the bit count table untouched). eax: 0 (the diagonal's last bit).
static DWORD THISCALL Random_Seed(BYTE *g, DWORD seed)
{
    dw(g, 4) = 0;
    dw(g, 8) = seed;
    dw(g, 0xc) = seed;
    dw(g, 0x10) = 0;
    dw(g, 0x14) = 0x67;
    seed_table(g);
    diagonal(g);
    return 0;
}
// 0x55c430: the state of another generator copied (the draw count 0). eax: the other.
static BYTE *THISCALL Random_Assign(BYTE *g, BYTE *other)
{
    dw(g, 4) = 0;
    dw(g, 8) = dw(other, 8);
    dw(g, 0xc) = dw(other, 0xc);
    dw(g, 0x10) = dw(other, 0x10);
    dw(g, 0x14) = dw(other, 0x14);
    memcpy(g + 0x18, other + 0x18, 250 * 4);
    return other;
}
// 0x55c470: an integer below n (the upper half of n times the draw); 0 for n 0 without a draw
static DWORD THISCALL Random_Below(BYTE *g, DWORD n)
{
    if (!n)
        return 0;
    return (DWORD)(((unsigned long long)n * draw(g)) >> 32);
}
// 0x55c4e0: an integer from lo to hi (both included); lo without a draw when the range is empty (hi =
// lo - 1, the count 2^32)
static DWORD THISCALL Random_Between(BYTE *g, DWORD lo, DWORD hi)
{
    const DWORD n = hi - lo + 1;
    if (!n)
        return lo;
    return (DWORD)(((unsigned long long)n * draw(g)) >> 32) + lo;
}
// 0x55c550: a float from lo to hi (st0: (hi - lo) * draw * 2^-32 + lo)
static long double THISCALL Random_Range(BYTE *g, float lo, float hi)
{
    const DWORD v = draw(g);
    return (fld(hi) - lo) * fild64((long long)v) * cst(glob_addr(C_RANDOM_32)) + lo;
}
// 0x55c5b0: a float between lo and hi with more weight in the middle: x = draw * 2^-31 - 1 in [-1, 1),
// then x * |x| (x * x, negated for x below 0) scaled to the range around its middle. st0.
static long double THISCALL Random_RangeCentered(BYTE *g, float lo, float hi)
{
    const long long v = draw(g);
    long double r;
    __asm__ volatile("fildll %[v]\n\t"
                     "fmuls %[k]\n\t"
                     "fsubs %[one]\n\t"
                     "fcoms %[zero]\n\t"
                     "fld %%st(0)\n\t"
                     "fnstsw %%ax\n\t"
                     "testb $1, %%ah\n\t"
                     "je 1f\n\t"
                     "fmulp %%st, %%st(1)\n\t"
                     "fmuls %[minus]\n\t"
                     "jmp 2f\n"
                     "1:\n\t"
                     "fmulp %%st, %%st(1)\n"
                     "2:\n\t"
                     "flds %[hi]\n\t"
                     "fsubs %[lo]\n\t"
                     "fmuls %[half]\n\t"
                     "fmulp %%st, %%st(1)\n\t"
                     "flds %[lo]\n\t"
                     "fadds %[hi]\n\t"
                     "fmuls %[half]\n\t"
                     "faddp %%st, %%st(1)"
                     : "=t"(r)
                     : [v] "m"(v), [k] "m"(*glob<const float *>(C_RANDOM_31)), [one] "m"(*glob<const float *>(C_ONE)),
                       [zero] "m"(*glob<const float *>(C_ZERO)), [minus] "m"(*glob<const float *>(C_MINUS_ONE)), [hi] "m"(hi), [lo] "m"(lo),
                       [half] "m"(*glob<const float *>(C_HALF))
                     : "eax", "cc");
    return r;
}
// 0x55c650: the same with more weight at the ends: (1 - x * x), negated for x below 0. The sign test
// reads the status word after the multiplication (fcom's C0 survives fmulp on the x87); kept as in the
// original.
static long double THISCALL Random_RangeEdges(BYTE *g, float lo, float hi)
{
    const long long v = draw(g);
    long double r;
    __asm__ volatile("fildll %[v]\n\t"
                     "fmuls %[k]\n\t"
                     "fsubs %[one]\n\t"
                     "fcoms %[zero]\n\t"
                     "fld %%st(0)\n\t"
                     "fmulp %%st, %%st(1)\n\t"
                     "fnstsw %%ax\n\t"
                     "fsubrs %[one]\n\t"
                     "testb $1, %%ah\n\t"
                     "je 1f\n\t"
                     "fmuls %[minus]\n"
                     "1:\n\t"
                     "flds %[hi]\n\t"
                     "fsubs %[lo]\n\t"
                     "fmuls %[half]\n\t"
                     "fmulp %%st, %%st(1)\n\t"
                     "flds %[lo]\n\t"
                     "fadds %[hi]\n\t"
                     "fmuls %[half]\n\t"
                     "faddp %%st, %%st(1)"
                     : "=t"(r)
                     : [v] "m"(v), [k] "m"(*glob<const float *>(C_RANDOM_31)), [one] "m"(*glob<const float *>(C_ONE)),
                       [zero] "m"(*glob<const float *>(C_ZERO)), [minus] "m"(*glob<const float *>(C_MINUS_ONE)), [hi] "m"(hi), [lo] "m"(lo),
                       [half] "m"(*glob<const float *>(C_HALF))
                     : "eax", "cc");
    return r;
}

// ---- Core helpers ---------------------------------------------------------------------------------------

// 0x55d030: the colour depth (the reporter's +0x24, 8 without one)
static DWORD __cdecl ColorDepth()
{
    if (BYTE *r = static_cast<BYTE *>(reporter()))
        return dw(r, 0x24);
    return 8;
}
// 0x55d050: a member function without arguments called on ecx. eax: ecx.
static void *THISCALL CallMember(void *self, void(THISCALL *fn)(void *))
{
    fn(self);
    return self;
}
// 0x55d060 (static initializer): the message id of "CReinforcementsRequestMessage". eax: the id.
static DWORD __cdecl ReinforcementsMessage_Init()
{
    const DWORD id = zax::fn<DWORD(__fastcall *)(const char *)>(0x506680)(glob<const char *>(0x6204b4) /* "CReinforcementsRequestMessage" */);
    *glob<DWORD *>(0x7130c0) = id;
    return id;
}
// The reinforcements request message on the stack (bytes 9-11 are left as the stack holds them)
struct ReinforcementsMessage {
    DWORD vtable, id;
    BYTE handled, pad[3];
    void *part;
    DWORD value;
};
// 0x55d070 (fastcall part, value; the radius on the stack): the request sent to the parts in an oval
// (0x4fb9e0: the message, the radius, the category bit 0x800000, 0, the part's team). eax: 0x4fb9e0's.
static DWORD __fastcall RequestReinforcements(BYTE *part, DWORD value, DWORD radius)
{
    ReinforcementsMessage m;
    m.id = *glob<DWORD *>(0x7130c0);
    m.value = value;
    m.handled = 0;
    m.part = part;
    m.vtable = vt(0x5fe8fc);
    return zax::fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD)>(0x4fb9e0)(part, &m, radius, 0x800000, 0, dw(part, 0x1c) & 0xffff);
}
// 0x55e090: record i of a definition list loaded (marked loading, the list's slot 1 for the name, the
// folder +0x38 joined; slot 37 with the path's folder part (or "") and name and the record's object
// field). No test of the range or of an object already there. eax: the object, or the records when the
// record is loading, 0 without records.
static DWORD THISCALL List_LoadRecord(BYTE *list, DWORD i)
{
    BYTE *records = bp(dw(list, 0x30));
    if (!records)
        return 0;
    const DWORD off = i << 7;
    if (records[off + 0x7f])
        return (DWORD)(uintptr_t)records;
    records[off + 0x7f] = 1;
    char path[0x100];
    zax::fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, reinterpret_cast<const char *>(list + 0x38),
                                                                          vcall<const char *>(list, 1, i));
    char *name = const_cast<char *>(path_name(path));
    DWORD o;
    if (name == path) {
        o = vcall<DWORD>(list, 37, glob<const char *>(TEXT_EMPTY), name, bp(dw(list, 0x30)) + off + 0x78);
    } else {
        name[-1] = 0;
        o = vcall<DWORD>(list, 37, path, name, bp(dw(list, 0x30)) + off + 0x78);
    }
    dw(bp(dw(list, 0x30)) + off, 0x78) = o;
    bp(dw(list, 0x30))[off + 0x7f] = 0;
    return o;
}

// ---- CRenderEffect, its scratch buffer, CRenderEffectBehavior -----------------------------------------

constexpr DWORD VT_RENDER_EFFECT = 0x5fea64, VT_RENDER_EFFECT_BEHAVIOR = 0x5fead4;
constexpr DWORD BUF_DATA = 0x713108, BUF_CAPACITY = 0x71310c, BUF_SIZE = 0x713110, BUF_IN_USE = 0x713114;

// 0x55e1c0 (static registration), 0x55e210: CRenderEffect's class (version 1, abstract, the shared
// Create/CreateArray/RegisterFields of the abstract classes)
static CClassInfo *__cdecl RenderEffect_ClassInfo()
{
    return class_getter(0x71311c, 0x620644 /* "CRenderEffect" */, 1.0f, object_class, 8, 1, 0x46b760, 0x52dc80, 0x58ea40);
}
// 0x55e270: CObject's constructor, the vtable, the field defaults. eax: this.
static BYTE *THISCALL RenderEffect_Ctor(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(VT_RENDER_EFFECT));
    return o;
}
static void RenderEffect_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_RENDER_EFFECT));
    obj_dtor(o);
}
// 0x55e290 (slot 1; no array branch)
static BYTE *THISCALL RenderEffect_DeletingDtor(BYTE *o, DWORD flags)
{
    RenderEffect_destroy(o);
    if (flags & 1)
        pool_free(o, 8);
    return o;
}
// 0x55e2c0
static void THISCALL RenderEffect_Dtor(BYTE *o) { RenderEffect_destroy(o); }
// 0x55e150 (static initializer): the scratch buffer empty, its exit function registered
static DWORD __cdecl RenderBuffer_Init()
{
    *glob<DWORD *>(BUF_DATA) = 0;
    *glob<DWORD *>(BUF_CAPACITY) = 0;
    *glob<DWORD *>(BUF_SIZE) = 0;
    *glob<BYTE *>(BUF_IN_USE) = 0;
    return crt_atexit(code<void(__cdecl *)()>(0x55e180));
}
// 0x55e180 (its exit function, guarded by bit 0 of 0x713119): the data freed
static void __cdecl RenderBuffer_Exit()
{
    BYTE &guard = *glob<BYTE *>(0x713119);
    if (guard & 1)
        return;
    guard |= 1;
    if (void *d = *glob<void **>(BUF_DATA)) {
        heap_free(d);
        *glob<DWORD *>(BUF_DATA) = 0;
    }
}
// 0x55e2e0: the buffer released (size 0, not in use). eax: 0.
static DWORD __cdecl RenderBuffer_Release()
{
    *glob<DWORD *>(BUF_SIZE) = 0;
    *glob<BYTE *>(BUF_IN_USE) = 0;
    return 0;
}
// 0x55e2f0 (fastcall size): the buffer for that size, in use; a bigger size takes twice the capacity or
// the size, whichever is bigger (the old data freed first). eax: the data.
static void *__fastcall RenderBuffer_Acquire(DWORD n)
{
    const DWORD cap = *glob<DWORD *>(BUF_CAPACITY);
    *glob<DWORD *>(BUF_SIZE) = n;
    *glob<BYTE *>(BUF_IN_USE) = 1;
    if (n <= cap)
        return *glob<void **>(BUF_DATA);
    if (cap + cap > n)
        n = cap + cap;
    *glob<DWORD *>(BUF_CAPACITY) = n;
    if (void *d = *glob<void **>(BUF_DATA)) {
        heap_free(d);
        *glob<DWORD *>(BUF_DATA) = 0;
    }
    void *d = heap_alloc(*glob<DWORD *>(BUF_CAPACITY));
    *glob<void **>(BUF_DATA) = d;
    return d;
}
// 0x55e350 (static registration), 0x55e3a0: CRenderEffectBehavior's class (parent CEntityBehavior)
static CClassInfo *__cdecl entity_behavior_class() { return zax::fn<Getter>(0x494410)(); }
static CClassInfo *__cdecl RenderEffectBehavior_ClassInfo()
{
    return class_getter(0x713124, 0x620654 /* "CRenderEffectBehavior" */, 1.0f, entity_behavior_class, 0xc, 1, 0x46b760, 0x52dc80, 0x58ea40);
}
static void RenderEffectBehavior_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_RENDER_EFFECT_BEHAVIOR));
    zax::fn<void(THISCALL *)(void *)>(0x4944e0)(o);   // CEntityBehavior's destructor
}
// 0x55e420 (slot 1; no array branch)
static BYTE *THISCALL RenderEffectBehavior_DeletingDtor(BYTE *o, DWORD flags)
{
    RenderEffectBehavior_destroy(o);
    if (flags & 1)
        pool_free(o, 0xc);
    return o;
}
// 0x55e450
static void THISCALL RenderEffectBehavior_Dtor(BYTE *o) { RenderEffectBehavior_destroy(o); }
// 0x55e470: a block reader with the vtable 0x5feb4c. eax: this.
static BYTE *THISCALL EffectReader_Ctor(BYTE *o)
{
    zax::fn<void *(THISCALL *)(void *)>(0x51f8d0)(o);
    dw(o, 0) = vt(0x5feb4c);
    return o;
}
// 0x55e490: ecx into 0x713128
static void __fastcall Pending_Set(DWORD v) { *glob<DWORD *>(0x713128) = v; }
// 0x55e4a0: 0x713128 taken (eax its value, the global 0)
static DWORD __cdecl Pending_Take()
{
    const DWORD v = *glob<DWORD *>(0x713128);
    *glob<DWORD *>(0x713128) = 0;
    return v;
}

// ---- The node list of names and the 0x34-byte object (multiplayer) --------------------------------------

constexpr DWORD VT_NODE_A = 0x5fb47c, VT_NODE_B = 0x5fb480, VT_NODE_BASE = 0x5fb484, VT_HOLDER = 0x5fb4dc;

// The constructor of both node classes: the vtable, a heap copy of the name (none for null), the next
BYTE *node_ctor(BYTE *o, DWORD vtable, const char *name, DWORD next)
{
    dw(o, 0) = vtable;
    dw(o, 4) = 0;
    if (!name) {
        dw(o, 8) = next;
        return o;
    }
    const DWORD n = (DWORD)lstrlenA(name) + 1;
    char *copy = static_cast<char *>(heap_alloc(n));
    memcpy(copy, name, n);
    dw(o, 4) = (DWORD)(uintptr_t)copy;
    dw(o, 8) = next;
    return o;
}
// The destructor of both: the next node deleted through its slot 0 (flag 1), the name freed
void node_destroy(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vtable;
    if (BYTE *next = bp(dw(o, 8))) {
        vcall<void *>(next, 0, (DWORD)1);
        dw(o, 8) = 0;
    }
    if (void *name = bp(dw(o, 4))) {
        heap_free(name);
        dw(o, 4) = 0;
    }
}
// 0x51ea10, 0x51eb40 (name, next). eax: this.
static BYTE *THISCALL NodeA_Ctor(BYTE *o, const char *name, DWORD next) { return node_ctor(o, vt(VT_NODE_A), name, next); }
static BYTE *THISCALL NodeB_Ctor(BYTE *o, const char *name, DWORD next) { return node_ctor(o, vt(VT_NODE_B), name, next); }
// 0x51ea80, 0x51ebb0 (slot 0: bit 0 frees with operator delete). eax: this.
static BYTE *THISCALL NodeA_DeletingDtor(BYTE *o, DWORD flags)
{
    node_destroy(o, vt(VT_NODE_A));
    if (flags & 1)
        heap_free(o);
    return o;
}
static BYTE *THISCALL NodeB_DeletingDtor(BYTE *o, DWORD flags)
{
    node_destroy(o, vt(VT_NODE_B));
    if (flags & 1)
        heap_free(o);
    return o;
}
// 0x51ead0, 0x51ec00
static void THISCALL NodeA_Dtor(BYTE *o) { node_destroy(o, vt(VT_NODE_A)); }
static void THISCALL NodeB_Dtor(BYTE *o) { node_destroy(o, vt(VT_NODE_B)); }
// 0x51eb10: the first node from ecx on whose name contains the text (0x587c70: fastcall text, part; the
// match, case-insensitive). eax: the node or 0.
static BYTE *THISCALL Node_Find(BYTE *o, const char *text)
{
    for (; o; o = bp(dw(o, 8)))
        if (zax::fn<DWORD(__fastcall *)(DWORD, const char *)>(0x587c70)(dw(o, 4), text))
            return o;
    return nullptr;
}
// 0x51ec40: the base vtable (eax left as it was)
static void THISCALL NodeBase_Ctor(BYTE *o) { dw(o, 0) = vt(VT_NODE_BASE); }
// 0x51ec50 (slot 0 of the base)
static BYTE *THISCALL NodeBase_DeletingDtor(BYTE *o, DWORD flags)
{
    dw(o, 0) = vt(VT_NODE_BASE);
    if (flags & 1)
        heap_free(o);
    return o;
}
// 0x51ec70: a new named value (0xc bytes, NamedValue_Ctor 0x51f700). eax: it or 0.
static void *__cdecl NamedValue_New()
{
    void *p = heap_alloc(0xc);
    if (!p)
        return nullptr;
    return zax::fn<void *(THISCALL *)(void *)>(0x51f700)(p);
}
// 0x51ec90: the function pointer 0x6d00a8 called with the argument (callee pops; ecx passed on). eax: its.
static DWORD THISCALL CallHandler(void *self, DWORD a) { return (*glob<DWORD(THISCALL **)(void *, DWORD)>(0x6d00a8))(self, a); }
// 0x51eca0: the function pointer set (ecx)
static void __fastcall SetHandler(DWORD fn) { *glob<DWORD *>(0x6d00a8) = fn; }
// 0x51ecb0 (owner): the two pointer arrays, the ZString, the vtable, +8 and +0xc 0, the owner and its
// back pointer +4. eax: this.
static BYTE *THISCALL Holder_Ctor(BYTE *o, BYTE *owner)
{
    dw(o, 0x10) = vt(0x5fb558);
    dw(o, 0x14) = 0;
    dw(o, 0x18) = 0;
    dw(o, 0x1c) = 0;
    dw(o, 0x20) = vt(0x5fb54c);
    dw(o, 0x24) = 0;
    dw(o, 0x28) = 0;
    dw(o, 0x2c) = 0;
    zs_ctor(o + 0x30);
    dw(o, 0) = vt(VT_HOLDER);
    dw(o, 8) = 0;
    dw(o, 0xc) = 0;
    dw(o, 4) = (DWORD)(uintptr_t)owner;
    dw(owner, 4) = (DWORD)(uintptr_t)o;
    return o;
}
// The elements of an embedded pointer array (data +0, count +4) deleted through their slot 0 (flag 1)
// from the last, each taken out of the array (memmove of the ones behind it, none here)
void delete_elements(BYTE *a)
{
    while (dw(a, 4)) {
        const DWORD i = dw(a, 4) - 1;
        if (BYTE *e = bp(dw(bp(dw(a, 0)), i * 4)))
            vcall<void *>(e, 0, (DWORD)1);
        const DWORD n = dw(a, 4);
        if (i < n) {
            if (i + 1 < n) {
                BYTE *d = bp(dw(a, 0));
                zax::fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d + i * 4, d + (i + 1) * 4, (n - i - 1) * 4);
            }
            dw(a, 4)--;
        }
    }
}
void holder_destroy(BYTE *o)
{
    dw(o, 0) = vt(VT_HOLDER);
    if (BYTE *owner = bp(dw(o, 4)))
        if (bp(dw(owner, 4)) == o)
            dw(owner, 4) = 0;
    delete_elements(o + 0x14);
    delete_elements(o + 0x24);
    zs_dtor(o + 0x30);
    dw(o, 0x20) = vt(0x5fb54c);
    if (void *d = bp(dw(o, 0x24))) {
        heap_free(d);
        dw(o, 0x24) = 0;
        dw(o, 0x2c) = 0;
    }
    dw(o, 0x28) = 0;
    dw(o, 0x10) = vt(0x5fb558);
    if (void *d = bp(dw(o, 0x14))) {
        heap_free(d);
        dw(o, 0x14) = 0;
        dw(o, 0x1c) = 0;
    }
    dw(o, 0x18) = 0;
}
// 0x51ed00 (slot 0: bit 0 frees). eax: this.
static BYTE *THISCALL Holder_DeletingDtor(BYTE *o, DWORD flags)
{
    holder_destroy(o);
    if (flags & 1)
        heap_free(o);
    return o;
}

// 0x51ee30
static void THISCALL Holder_Dtor(BYTE *o) { holder_destroy(o); }

ZAX_HOOKS(random_misc)
{
    const Hook list[] = {
#include "random_misc_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
