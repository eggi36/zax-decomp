// Shared parts of the checks of polygon.cpp and layer_part_helpers.cpp (checks_polygon.cpp,
// checks_layer_part_helpers.cpp).
//
// Every case runs twice from the same seed: side 0 with the hooks of both modules taken out (the
// original's first bytes put back, so nested calls run the original too), side 1 through the
// reimplementation. Compared are eax (masked where the original leaves undefined bits), an event log of
// the calls into fakes and stand-ins, and a hash of the state afterwards (floats by their bits, results
// in st(0) by all 80 bits, pointers into the fakes by number).
//
// Fakes: layer parts, AIs, pointer arrays and sweeps whose vtables are tables of recorder thunks
// ("mov eax, id; jmp" into one stub that logs the arguments, writes deterministic outputs and returns a
// configured value or float). Stand-ins replace the exe functions the code reaches beyond the modules:
// the grid query CSortList2D_FindRect 0x57a770 and the temporary part lists 0x59a470/0x59a510, the
// layer's queries 0x4ed610/0x4ed690 and 0x4ea880, the name lookup 0x57e690, the dynamic property
// functions 0x418fc0/0x418810/0x418fb0, the broadcaster 0x467d40 and the network packet's bool/word
// readers and writers.
#pragma once
#include "check_util.h"
#include "layer_part_helpers.h"
#include "layer_parts.h"
#include "polygon.h"

namespace zax::geo {

inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Sides, log and state --------------------------------------------------------------------------------

extern int g_side;
extern unsigned g_loghash[2], g_statehash[2];
extern int g_nlog[2];
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0);
void st(DWORD v);
void st_bytes(const void *p, DWORD n);
void st_float(float f);
void st_ld(long double v);
void st_poly(const Poly2D *p);
void dump(DWORD v);   // values shown for both sides when a case differs (24 per side)   // count, rectangle, flags, mean, the arrays' contents
DWORD tag(DWORD v);              // fakes, polygons and the stack by number
inline DWORD tagp(const void *p) { return tag(ptrv(p)); }

void hooks_out();
void hooks_in();
void side_begin(int side, unsigned seed);
void side_end(int side);
bool compare_sides(Tally &t, const char *what, unsigned seed, DWORD r0, DWORD r1, DWORD mask);

// Both sides of a case; `run` returns eax and sets the mask of its defined bits
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

// ---- Random values ----------------------------------------------------------------------------------------

float random_coord();           // finite: fractions, integers, 0 and -0, tiny, up to 1e15
float random_small();           // -500 .. 500 with fractions
DWORD random_mask();

// ---- Fakes ---------------------------------------------------------------------------------------------------

constexpr int NPART = 12, NAI = 12, NLIST = 8, NSWEEP = 2;
constexpr int T_PART = 0, T_AI = 1, T_ARRAY = 2, T_SWEEP = 3;

struct FakePart {
    LayerPart p;            // 0x00
    float x, y;             // 0x4c (simple part)
    BYTE pad[0x70];         // 0x54
    float facing;           // 0xc4 (CEntityAnimated)
    // configuration
    DWORD cls;              // slot 2: 0 CLayerPart, 1 CEntityAnimated, 2 the class of 0x487620
    const Poly2D *poly;     // slot 48
    float off[2];           // slot 50
    float pos[2];           // slot 43
    float s40[2];           // slot 40
    float s46;              // slot 46 (st(0))
    int i52, i53, i54;      // slots 52, 53, 54
    BYTE r37, r65, r66;     // slots 37, 65, 66 (al)
    BYTE pad2;
    void *items[16];        // activity storage
    int deleted;
};
struct FakeAI {
    DWORD vftable;
    DWORD dynamic;
    BYTE done;
    BYTE pad[3];
    DWORD f0c;
    int deleted;
};
struct FakeList {
    LPChildList l;
    LayerPart *items[12];
};
struct FakeSweep {
    Sweep s;
    BYTE r0;   // slot 0 (al)
};

extern FakePart g_parts[NPART];
extern FakeAI g_ais[NAI];
extern FakeList g_lists[NLIST];
extern FakeSweep g_sweeps[NSWEEP];
extern BYTE g_layer[0x100];
extern DWORD g_vt[5][96];

void init_fakes();   // once: thunks checked, tables filled
// Random fakes (the same on both sides when seeded alike): parents, children, AIs, flags, positions
void setup_fakes(const Poly2D *const *polys, int npolys);
LayerPart *part(int i);

// The query stand-ins: the parts FindRect puts into the temporary list
extern LayerPart *g_query[16];
extern int g_nquery;
// The dynamic property table of the stand-ins: (descriptor, object) -> pointer
struct DynEntry {
    const void *desc, *obj, *value;
};
extern DynEntry g_dyn[16];
extern int g_ndyn;
// Net packet stand-ins: what the readers return (consumed in order); words -1 = nothing left
extern int g_read_bool, g_read_words[2], g_nread_words;
extern void *g_lookup_result;   // 0x57e690
// FileBuf_Read stand-in: the bytes it hands out
extern BYTE g_file[256];
extern DWORD g_file_size, g_file_pos;
// The fastcall filter of 0x4e8e00 (records, returns g_filter_result)
extern BYTE g_filter_result;
BYTE __fastcall rec_filter(void *mover, void *part);

struct Patches {
    CodePatch p[16];
    int n = 0;
    void on();
    void off();
};
extern Patches g_patches;

// ---- Memory helpers --------------------------------------------------------------------------------------------

bool readable(const void *p, DWORD n);
CClassInfo *class_of(const void *p);
bool heap_pointer(const void *p);

// A polygon built by the original functions (side independent): count points from xy
void build_poly(Poly2D *p, const float *xy, DWORD count);
void free_poly(Poly2D *p);
// The edges copied into a bigger block whose extra entry is defined (0x547a30 reads one past the edges of an
// open outline)
void pad_edges(Poly2D *p);

}  // namespace zax::geo
