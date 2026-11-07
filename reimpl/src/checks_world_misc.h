// Shared parts of the checks of part_list.cpp, layer_view.cpp and plasma_tile_map.cpp
// (checks_world_misc.cpp): the two sides of a case, the event log and state hash, memory helpers and
// the running level.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module under test taken out
// (the original code throughout, also where the module's functions call each other), side 1 through
// the reimplementation. Compared: eax (masked where the original's is not defined), an event log of the
// fakes and stand-ins, and a hash of the state afterwards.
#pragma once
#include "check_util.h"

namespace zax::misc {

inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

extern int g_side;
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0);
void st(DWORD v);
void st_bytes(const void *p, DWORD n);
void st_text(const char *s);
void dump(DWORD v);   // shown for both sides when a case differs

// The hook table whose hooks side 0 takes out
void use_hooks(int (*table)(Hook *));
void hooks_out();
void hooks_in();
void side_begin(int side, unsigned seed);
void side_end(int side);
bool compare_sides(Tally &t, const char *what, unsigned seed, DWORD r0, DWORD r1, DWORD mask);

extern bool g_trace;   // one log line per case (to find a crash)
template <typename C> bool two_sides(Tally &t, const char *what, unsigned seed, C run)
{
    DWORD r[2], mask = 0xffffffff;
    if (g_trace)
        log("  > %s %u", what, seed);
    for (int side = 0; side < 2; side++) {
        side_begin(side, seed);
        r[side] = run(mask);
        side_end(side);
    }
    return compare_sides(t, what, seed, r[0], r[1], mask);
}

// Memory: committed and readable (no wrap-around), a heap-looking pointer, the class of an object read
// through the getter's trampoline (some getters are hooked)
bool readable(const void *p, DWORD n);
bool heap_pointer(const void *p);
CClassInfo *class_of(const void *p);

// The running level: the CLayer (class 0x6c1d04) the world reaches within two pointers, and the parts in
// its grid (each once)
BYTE *running_layer();
int real_parts(BYTE **out, int max);

// Random floats: finite values of every kind (fractions, integers, 0 and -0, tiny, huge)
float random_float();

}  // namespace zax::misc
