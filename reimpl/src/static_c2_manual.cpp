// The static initializers and atexit destructors of batch C2 (phase 5a, docs/DECOMPILATION.md 10) that
// tools/static_init_gen.py does not translate (loops, string copies, virtual calls on objects reached
// through globals); their hook entries and checks are generated with the others (static_c2*.cpp,
// checks_static_c2*.cpp, setups in checks_static_c2_manual.cpp). Layouts [confirmed: code]:
// - The texture lists (six globals, the destructor inlined in each exit stub; base B): B+0x30 an array of
//   0x80-byte entries with the count at -4 and an object pointer at +0x78 of each entry (deleted through
//   its slot 1 with 1, from the last entry to the first, then the array freed with operator delete from its
//   count), B+0x34 cleared, the record list's Set(0, 0, 0, 0, 0), B+0x13c an object deleted through its
//   slot 7, B+0x140 a frame filter, B+0x1c the resource aliases; the vtables TextureListBase 0x5f4a38,
//   then 0x5ed564 and NamedList 0x5ec7cc while the bases are destroyed (0x56bfb0: 0x5ff64c, 0x5ff6f0).
// - The pointer arrays {vtable, data, count, capacity} (0x5f1de8/0x5f1df4 at 0x6bdd08 with its elements
//   deleted through their slot 6 and closed up with memmove; 0x5fba3c/0x5fba48 and 0x600c14/0x5fba48 at
//   0x7160e0 and 0x716080 with an embedded second array at +0x10 whose size is in bytes): their memory goes
//   back to the pool (pool_get 0x506470, then ZPool::Free 0x5065e0 with the block and its size).
// - 0x713708 the sound resource list (entries of 0x108 bytes, the resource at +0x104 released through the
//   resource cache), 0x713e20 an intrusive list of 0x18-byte nodes with a pointer array in each.
#include "static_c2.h"

namespace zax {

using namespace sinit;

namespace {

constexpr DWORD ATEXIT = 0x5d0863, OP_DELETE = 0x5d0320, POOL_GET = 0x506470, POOL_FREE = 0x5065e0;

DWORD vcall1(DWORD obj, DWORD slot_offset, DWORD a)
{
    return at<DWORD(THISCALL *)(void *, DWORD)>(get32(get32(obj) + slot_offset))(at<void *>(obj), a);
}
DWORD pool_free(DWORD block, DWORD bytes)
{
    const DWORD pool = c_call(POOL_GET);
    return this_call(POOL_FREE, pool, block, bytes);
}
void copy_text(DWORD to, DWORD from)   // rep movsd + rep movsb of strlen + 1 bytes
{
    DWORD n = 0;
    while (get8(from + n))
        n++;
    for (DWORD i = 0; i <= n; i++)
        put8(to + i, get8(from + i));
}

// The inlined destructor of a texture list at b (eax: the resource aliases' destructor's)
DWORD texture_list_exit(DWORD b, DWORD first_vtable, DWORD base_vtable)
{
    if (first_vtable)
        put32(b, first_vtable);
    this_call(0x4b1d20 /* FrameFilter_Dtor */, b + 0x140);
    const DWORD data = get32(b + 0x30);
    put32(b, base_vtable);
    if (data) {
        const DWORD count = get32(data - 4);
        DWORD slot = data + (count << 7) + 0x78;
        for (DWORD i = (int)(count - 1) < 0 ? 0 : count; i != 0; i--) {   // dec; js: no entry
            const DWORD obj = get32(slot - 0x80);
            slot -= 0x80;
            if (obj) {
                vcall1(obj, 4, 1);
                put32(slot, 0);
            }
        }
        c_call(OP_DELETE, data - 4);
        put32(b + 0x30, 0);
    }
    put32(b + 0x34, 0);
    this_call(0x591cd0 /* RecordList_Set */, b, 0, 0, 0, 0, 0);
    if (const DWORD o = get32(b + 0x13c)) {
        vcall1(o, 0x1c, 1);
        put32(b + 0x13c, 0);
    }
    put32(b, vt(0x5ed564));
    const DWORD r = this_call(0x55d0c0 /* ResourceAliases_Dtor */, b + 0x1c);
    put32(b, vt(0x5ec7cc));
    return r;
}

// The two pointer arrays at b (vtable b, data b+4, count b+8, capacity b+0xc; the second at b+0x10) of
// 0x525650 and 0x57cde0
DWORD pointer_arrays_exit(DWORD b, DWORD vtable)
{
    DWORD eax = get32(b + 8);
    const bool any = eax != 0;
    eax = get32(b + 4);
    if (any) {
        DWORD i = 0;
        do {
            const DWORD p = get32(eax + i * 4);
            if (p) {
                c_call(OP_DELETE, p);
                put32(get32(b + 4) + i * 4, 0);
                eax = get32(b + 4);
            }
            i++;
        } while (i < get32(b + 8));
    }
    if (eax) {
        pool_free(eax, get32(b + 0xc) * 4);
        eax = 0;
        put32(b + 0xc, 0);
        put32(b + 4, eax);
    }
    const DWORD second = get32(b + 0x14);
    put32(b + 8, 0);
    put32(b + 0x10, vt(0x5fba48));
    if (second) {
        pool_free(second, get32(b + 0x1c));
        eax = get32(b + 4);
        put32(b + 0x14, 0);
        put32(b + 0x1c, 0);
    }
    put32(b + 0x18, 0);
    put32(b, vtable);
    if (eax) {
        eax = pool_free(eax, get32(b + 0xc) << 2);
        put32(b + 4, 0);
        put32(b + 0xc, 0);
    }
    put32(b + 8, 0);
    return eax;
}

}  // namespace

// 0x46eb90: the INI file's path (ConfigPath into a ZString on the stack, its text copied to 0x6bd330) and the
// section name "Options" (0x60fc08) copied to 0x6bd430; eax 0 (left by the string scan)
DWORD __cdecl DataPtr_46eb90()
{
    DWORD local;
    const DWORD z = this_call(0x4a45f0 /* ConfigPath */, (DWORD)(uintptr_t)&local);
    const DWORD text = this_call(0x4e13a0 /* ZString_c_str */, z);
    copy_text(glob_addr(0x6bd330), text);
    this_call(0x4defd0 /* ZString_Dtor */, (DWORD)(uintptr_t)&local);
    copy_text(glob_addr(0x6bd430), glob_addr(0x60fc08));
    return 0;
}

// 0x487550: the pointer array at 0x6bdd08 (data 0x6bdd0c, count 0x6bdd10, capacity 0x6bdd14) emptied from
// its end, each element deleted through its slot 6 with 1 and the rest closed up; then freed (eax: the
// caller's when it was empty, so not compared)
void __cdecl StaticExit_487550()
{
    constexpr DWORD DATA = 0x6bdd0c, COUNT = 0x6bdd10, CAP = 0x6bdd14;
    DWORD n = get32(glob_addr(COUNT));
    put32(glob_addr(0x6bdd08), vt(0x5f1de8));
    DWORD data;
    if (n != 0) {
        data = get32(glob_addr(DATA));
        do {
            const DWORD obj = get32(data + n * 4 - 4);
            const DWORD i = n - 1;
            if (obj) {
                vcall1(obj, 0x18, 1);
                n = get32(glob_addr(COUNT));
                data = get32(glob_addr(DATA));
            }
            if (i < n) {
                const DWORD next = i + 1;
                if (next < n) {
                    c_call(0x5d04b0 /* memmove */, data + i * 4, data + next * 4, (n - next) * 4);
                    n = get32(glob_addr(COUNT));
                    data = get32(glob_addr(DATA));
                }
                n--;
                put32(glob_addr(COUNT), n);
            }
        } while (n != 0);
    } else {
        data = get32(glob_addr(DATA));
    }
    put32(glob_addr(0x6bdd08), vt(0x5f1df4));
    if (data) {
        pool_free(data, get32(glob_addr(CAP)) << 2);
        put32(glob_addr(DATA), 0);
        put32(glob_addr(CAP), 0);
    }
    put32(glob_addr(COUNT), 0);
}

DWORD __cdecl StaticExit_4b4020() { return texture_list_exit(glob_addr(0x6bf850), 0, vt(0x5f4a38)); }
DWORD __cdecl StaticExit_4ef1f0() { return texture_list_exit(glob_addr(0x6c1d50), 0, vt(0x5f4a38)); }
DWORD __cdecl StaticExit_4ef320() { return texture_list_exit(glob_addr(0x6c22e0), 0, vt(0x5f4a38)); }
DWORD __cdecl StaticExit_56bfb0() { return texture_list_exit(glob_addr(0x713380), vt(0x5ff64c), vt(0x5ff6f0)); }
// The two texture lists of 0x6c0aa8 and 0x6c0540 share the guard byte 0x6c0538 (bits 1 and 2); with the bit
// set al holds the guard byte (0x4d5360) or the bit (0x4d5460)
DWORD __cdecl StaticExit_4d5360()
{
    const BYTE g = get8(glob_addr(0x6c0538));
    if (g & 1)
        return g;
    put8(glob_addr(0x6c0538), (BYTE)(g | 1));
    return texture_list_exit(glob_addr(0x6c0aa8), 0, vt(0x5f4a38));
}
DWORD __cdecl StaticExit_4d5460()
{
    const BYTE g = get8(glob_addr(0x6c0538));
    if (g & 2)
        return 2;
    put8(glob_addr(0x6c0538), (BYTE)(g | 2));
    return texture_list_exit(glob_addr(0x6c0540), 0, vt(0x5f4a38));
}

// 0x4dd890: the joystick object 0x6c0db0 (its name ZString at 0x6c0df8, flags, dead zones 0.2 and
// sensitivities 1.25, CJoystick::Init(0)), its 32 button dwords at 0x6c0e2c cleared, atexit
DWORD __cdecl StaticInit_4dd890()
{
    this_call(0x4dec90 /* ZString_Ctor */, glob_addr(0x6c0df8));
    put8(glob_addr(0x6c0eac), 0);
    put8(glob_addr(0x6c0e0c), 0);
    put8(glob_addr(0x6c0e24), 0);
    put8(glob_addr(0x6c0e18), 0);
    put32(glob_addr(0x6c0db4), 0x3e4ccccd);
    put32(glob_addr(0x6c0dbc), 0x3e4ccccd);
    put32(glob_addr(0x6c0db8), 0x3fa00000);
    put32(glob_addr(0x6c0dc0), 0x3fa00000);
    put32(glob_addr(0x6c0e08), 0);
    this_call(0x4dda00 /* CJoystick::Init */, glob_addr(0x6c0db0), 0);
    for (DWORD i = 0; i < 0x20; i++)
        put32(glob_addr(0x6c0e2c) + 4 * i, 0);
    return c_call(ATEXIT, fn_addr(0x4dd910));
}

// 0x4ef3f0/0x4ef420: four frames of 0x40 bytes at 0x6c2170, built in order and destroyed from the last
DWORD __cdecl StaticInit_4ef3f0()
{
    for (DWORD f = glob_addr(0x6c2170), k = 4; k; k--, f += 0x40)
        this_call(0x4a8a90 /* CFrame::Ctor */, f);
    return c_call(ATEXIT, fn_addr(0x4ef420));
}
DWORD __cdecl StaticExit_4ef420()
{
    DWORD r = 0;
    // the end from the frames' own start (since batch 6-10b): 0x6c2270 is the next block, which stays in the image while
    // 0x6c2170 is own, so the exe destroyed the image's poisoned copy at its exit (TraceApi run 20261030-074144-a4f0)
    for (DWORD f = glob_addr(0x6c2170) + 0x100, k = 4; k; k--) {
        f -= 0x40;
        r = this_call(0x4a8b00 /* CFrame::Dtor */, f);
    }
    return r;
}

// 0x53f470: 256 random generators of 0x400 bytes at 0x6d1030
DWORD __cdecl DataPtr_53f470()
{
    DWORD r = 0;
    for (DWORD g = glob_addr(0x6d1030), k = 0x100; k; k--, g += 0x400)
        r = this_call(0x55c2c0 /* Random_Ctor */, g);
    return r;
}

// 0x5470f0: ten pairs of polygons (0x88 bytes per pair, the second at +0x38) at 0x7111e8 cleared, atexit
DWORD __cdecl StaticInit_5470f0()
{
    for (DWORD p = glob_addr(0x7111e8), k = 10; k; k--, p += 0x88) {
        this_call(0x545ca0 /* Polygon_Clear0 */, p);
        this_call(0x545ca0 /* Polygon_Clear0 */, p + 0x38);
    }
    return c_call(ATEXIT, fn_addr(0x547130));
}

// 0x547200: 200 times the pair of dwords at 0x711738 + 8 * [0x711d78] cleared (the index read again each
// time), then the index; eax the last pair's address
DWORD __cdecl DataPtr_547200()
{
    DWORD a = 0;
    for (DWORD k = 0xc8; k; k--) {
        const DWORD i = get32(glob_addr(0x711d78));
        put32(i * 8 + glob_addr(0x711738), 0);
        put32(i * 8 + glob_addr(0x71173c), 0);
        a = i * 8 + glob_addr(0x711738);
    }
    put32(glob_addr(0x711d78), 0);
    return a;
}

DWORD __cdecl StaticExit_525650() { return pointer_arrays_exit(glob_addr(0x7160e0), vt(0x5fba3c)); }
DWORD __cdecl StaticExit_57cde0() { return pointer_arrays_exit(glob_addr(0x716080), vt(0x600c14)); }

// 0x57dfa0: the sound resource list at 0x713708 (count +0x14, entries +0x34, an object +0x38, two ZStrings)
DWORD __cdecl StaticExit_57dfa0()
{
    constexpr DWORD B = 0x713708;
    const DWORD count = get32(glob_addr(B) + 0x14);
    put32(glob_addr(B), vt(0x600c2c));
    if (count != 0) {
        DWORD off = 0;
        for (DWORD i = 0;;) {
            const DWORD res = get32(off + get32(glob_addr(B) + 0x34) + 0x104);
            if (res) {
                const DWORD cache = c_call(0x57aef0 /* ResourceCache_Get */);
                this_call(0x57af60 /* ResourceCache_Release */, cache, res);
                put32(off + get32(glob_addr(B) + 0x34) + 0x104, 0);
            }
            i++;
            off += 0x108;
            if (i >= get32(glob_addr(B) + 0x14))
                break;
        }
    }
    if (const DWORD entries = get32(glob_addr(B) + 0x34)) {
        c_call(OP_DELETE, entries);
        put32(B + 0x34, 0);
    }
    this_call(0x591cd0 /* RecordList_Set */, glob_addr(B), 1, 0, 0, 0, 0);
    if (const DWORD o = get32(glob_addr(B) + 0x38)) {
        vcall1(o, 0x1c, 1);
        put32(B + 0x38, 0);
    }
    this_call(0x4defd0 /* ZString_Dtor */, glob_addr(B) + 0x3c);
    this_call(0x4defd0 /* ZString_Dtor */, glob_addr(B) + 0x30);
    put32(glob_addr(B), vt(0x5ed564));
    const DWORD r = this_call(0x55d0c0 /* ResourceAliases_Dtor */, glob_addr(B) + 0x1c);
    put32(glob_addr(B), vt(0x5ec7cc));
    return r;
}

// 0x59a3c0: the list at 0x713e20 (sentinel: first, last) emptied: every node unlinked, its pointer array
// (vtable +8, data +0xc, count +0x10, capacity +0x14) freed, the node deleted; then the array at 0x713e28
void __cdecl StaticExit_59a3c0()
{
    constexpr DWORD HEAD = 0x713e20;
    for (DWORD n = get32(glob_addr(HEAD)); n != glob_addr(HEAD); n = get32(glob_addr(HEAD))) {
        put32(get32(n) + 4, get32(n + 4));
        put32(get32(n + 4), get32(n));
        if (n != 0) {
            const DWORD data = get32(n + 0xc);
            put32(n + 8, vt(0x5f8b98));
            if (data) {
                pool_free(data, get32(n + 0x14) << 2);
                put32(n + 0xc, 0);
                put32(n + 0x14, 0);
            }
            put32(n + 0x10, 0);
            c_call(OP_DELETE, n);
        }
    }
    const DWORD data = get32(glob_addr(0x713e2c));
    put32(glob_addr(0x713e28), vt(0x5f8b98));
    if (data) {
        pool_free(data, get32(glob_addr(0x713e34)) * 4);
        put32(glob_addr(0x713e2c), 0);
        put32(glob_addr(0x713e34), 0);
    }
    put32(glob_addr(0x713e30), 0);
}

}  // namespace zax
