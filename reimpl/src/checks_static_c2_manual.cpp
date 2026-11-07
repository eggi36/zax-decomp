// The case setups of the hand-written functions of batch C2 (static_c2_manual.cpp): every global they
// dereference points into the harness's block (arrays, entries, list nodes, fake objects of the "game
// object" class whose slots are recorded), never at the game's live objects (checks_static_c2.h).
#include "checks_static_c2.h"

namespace zax::sc2 {

using namespace uic;

namespace {

// The game objects' slot arguments: slot 1 (texture entries), 6 (the array's elements) and 7 (the texture
// list's object at +0x13c, the sound list's at +0x38) take one argument
constexpr char OBJ_ARGS[] = "0100001100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
static_assert(sizeof OBJ_ARGS == 113, "112 slots");

int g_next_win;
DWORD fake_object(Gen &g, int one_in_null)
{
    if (g.chance((DWORD)one_in_null) || g_next_win >= NWIN)
        return 0;
    BYTE *w = win(g_next_win++);
    memset(w, 0, WIN);
    at_(w, 0) = g_object_vtable;
    return ptrv(w);
}

void texture_list(Gen &g, DWORD b)
{
    g_next_win = 0;
    g_case.object_args = OBJ_ARGS;
    DWORD data = 0;
    if (!g.chance(4)) {
        const DWORD count = g.below(4);
        BYTE *block = xalloc(4 + count * 0x80);
        if (block) {
            at_(block, 0) = count;
            for (DWORD k = 0; k < count; k++)
                at_(block, 4 + k * 0x80 + 0x78) = fake_object(g, 3);
            data = ptrv(block) + 4;
        }
    }
    case_global(b + 0x30, data);
    case_global(b + 0x13c, fake_object(g, 3));
}

void pointer_arrays(Gen &g, DWORD b)
{
    const DWORD count = g.chance(4) ? 0 : 1 + g.below(4);
    DWORD data = 0;
    if (count || g.chance(2)) {
        BYTE *a = xalloc(4 * (count + 1));
        for (DWORD k = 0; a && k < count; k++)
            at_(a, 4 * k) = g.chance(3) ? 0 : g.next() | 1;
        data = ptrv(a);
    }
    case_global(b + 4, data);
    case_global(b + 8, data ? count : 0);
}

}  // namespace

void setup_texture_list_4b4020(Gen &g) { texture_list(g, 0x6bf850); }
void setup_texture_list_4d5360(Gen &g) { texture_list(g, 0x6c0aa8); }
void setup_texture_list_4d5460(Gen &g) { texture_list(g, 0x6c0540); }
void setup_texture_list_4ef1f0(Gen &g) { texture_list(g, 0x6c1d50); }
void setup_texture_list_4ef320(Gen &g) { texture_list(g, 0x6c22e0); }
void setup_texture_list_56bfb0(Gen &g) { texture_list(g, 0x713380); }

void setup_487550(Gen &g)
{
    g_next_win = 0;
    g_case.object_args = OBJ_ARGS;
    const DWORD n = g.below(6);
    DWORD data = 0;
    if (n || g.chance(2)) {
        BYTE *a = xalloc(4 * (n + 1));
        for (DWORD k = 0; a && k < n; k++)
            at_(a, 4 * k) = fake_object(g, 3);
        data = ptrv(a);
    }
    case_global(0x6bdd0c, data);
    case_global(0x6bdd10, data ? n : 0);
    case_global(0x6bdd14, n + g.below(3));
}

void setup_547200(Gen &g) { case_global(0x711d78, g.chance(8) ? 200 : g.below(200)); }

void setup_525650(Gen &g) { pointer_arrays(g, 0x7160e0); }
void setup_57cde0(Gen &g) { pointer_arrays(g, 0x716080); }

void setup_57dfa0(Gen &g)
{
    constexpr DWORD B = 0x713708;
    g_next_win = 0;
    g_case.object_args = OBJ_ARGS;
    const DWORD count = g.below(4);
    DWORD entries = 0;
    if (count || g.chance(2)) {
        BYTE *e = xalloc(0x108 * (count + 1));
        for (DWORD k = 0; e && k < count; k++)
            at_(e, k * 0x108 + 0x104) = g.chance(3) ? 0 : g.next() | 1;
        entries = ptrv(e);
    }
    case_global(B + 0x34, entries);
    case_global(B + 0x14, entries ? count : 0);
    case_global(B + 0x38, fake_object(g, 3));
}

void setup_59a3c0(Gen &g)
{
    constexpr DWORD HEAD = 0x713e20;
    const DWORD k = g.below(4);
    DWORD first = HEAD, last = HEAD;
    BYTE *nodes = k ? xalloc(0x18 * k) : nullptr;
    if (nodes) {
        for (DWORD i = 0; i < k; i++) {
            BYTE *n = nodes + 0x18 * i;
            at_(n, 0) = i + 1 < k ? ptrv(n + 0x18) : HEAD;
            at_(n, 4) = i ? ptrv(n - 0x18) : HEAD;
            at_(n, 8) = g.next();
            at_(n, 0xc) = g.chance(3) ? 0 : g.next() | 1;
            at_(n, 0x10) = g.below(9);
            at_(n, 0x14) = g.below(9);
        }
        first = ptrv(nodes);
        last = ptrv(nodes + 0x18 * (k - 1));
    }
    case_global(HEAD, first);
    case_global(HEAD + 4, last);
}

}  // namespace zax::sc2
