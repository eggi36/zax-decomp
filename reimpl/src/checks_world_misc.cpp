// Shared parts of the world misc checks (see checks_world_misc.h).
#include "checks_world_misc.h"
#include "ptr_array.h"
#include "x87.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);

namespace zax::misc {

int g_side;
bool g_trace;
static unsigned g_loghash[2], g_statehash[2];
static int g_nlog[2], g_ndump[2];
constexpr int LOG = 256, DUMP = 24;
static DWORD g_log[2][LOG][4], g_dump[2][DUMP];

void ev(DWORD a, DWORD b, DWORD c, DWORD d)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    if (g_nlog[g_side] < LOG)
        memcpy(g_log[g_side][g_nlog[g_side]], e, sizeof e);
    g_nlog[g_side]++;
}
void st(DWORD v) { g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4); }
void st_bytes(const void *p, DWORD n) { g_statehash[g_side] = fnv(g_statehash[g_side], p, n); }
void st_text(const char *s) { g_statehash[g_side] = fnv_text(g_statehash[g_side], s ? s : "(null)"); }
void dump(DWORD v)
{
    if (g_ndump[g_side] < DUMP)
        g_dump[g_side][g_ndump[g_side]++] = v;
}

static void log_first_difference()
{
    for (int i = 0; i < g_nlog[0] && i < g_nlog[1] && i < LOG; i++)
        if (memcmp(g_log[0][i], g_log[1][i], 16)) {
            log("    event %d: %x %x %x %x / %x %x %x %x", i, g_log[0][i][0], g_log[0][i][1], g_log[0][i][2], g_log[0][i][3],
                g_log[1][i][0], g_log[1][i][1], g_log[1][i][2], g_log[1][i][3]);
            return;
        }
}

// ---- Hooks out for side 0 -----------------------------------------------------------------------------------

static int (*g_table)(Hook *);
struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 64;
static SavedSite g_sites[MAX_SITES];
static int g_nsites;

void use_hooks(int (*table)(Hook *)) { g_table = table; }

void hooks_out()
{
    Hook list[MAX_SITES];
    g_nsites = 0;
    const int n = g_table(nullptr);
    if (!hook_sites_fit("world_misc", n, MAX_SITES))
        return;
    g_table(list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (!memcmp(site, list[i].expected, 5))
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, list[i].expected);
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

void side_begin(int side, unsigned seed)
{
    g_side = side;
    g_seed = seed;
    g_loghash[side] = g_statehash[side] = 2166136261u;
    g_nlog[side] = 0;
    g_ndump[side] = 0;
    if (side == 0)
        hooks_out();
}
void side_end(int side)
{
    if (side == 0)
        hooks_in();
}

bool compare_sides(Tally &t, const char *what, unsigned seed, DWORD r0, DWORD r1, DWORD mask)
{
    char d[200];
    wsprintfA(d, "[%s seed %u: eax %x / %x (mask %x), log %d %x / %d %x, state %x / %x]", what, seed, r0, r1, mask,
              g_nlog[0], g_loghash[0], g_nlog[1], g_loghash[1], g_statehash[0], g_statehash[1]);
    const bool same = ((r0 ^ r1) & mask) == 0 && g_loghash[0] == g_loghash[1] && g_nlog[0] == g_nlog[1] &&
                      g_statehash[0] == g_statehash[1];
    if (!same && t.mismatches < 10) {
        log_first_difference();
        for (int i = 0; i < g_ndump[0] && i < g_ndump[1]; i += 6) {
            char line[200];
            int n = 0;
            for (int k = i; k < i + 6 && k < g_ndump[0]; k++)
                n += wsprintfA(line + n, " %x/%x", g_dump[0][k], g_dump[1][k]);
            log("    dump%s", line);
        }
    }
    t.check(same, "world misc", d);
    return same;
}

// ---- Memory ---------------------------------------------------------------------------------------------------

bool readable(const void *p, DWORD n)
{
    struct {
        void *base, *allocation_base;
        DWORD allocation_protect;
        size_t size;
        DWORD state, protect, type;
    } mi;
    if (ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000)
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0xee))
            return false;
    return true;
}

bool heap_pointer(const void *p) { return ptrv(p) >= 0x10000 && !(ptrv(p) & 3) && (ptrv(p) < 0x400000 || ptrv(p) >= IMAGE_END); }

CClassInfo *class_of(const void *p)
{
    if (ptrv(p) < 0x10000 || (ptrv(p) & 3) || !readable(p, 0x14))
        return nullptr;
    const DWORD vt = *static_cast<const DWORD *>(p);
    if (vt < 0x5e0000 || vt >= 0x610000 || (vt & 3))
        return nullptr;
    if (!readable(at<void *>(vt), 12))
        return nullptr;
    const DWORD getter = *at<DWORD *>(vt + 8);
    if (getter < 0x401000 || getter >= 0x5e0000 || !readable(at<void *>(getter), 5) ||
        *static_cast<const BYTE *>(original_address(getter)) != 0xa1)
        return nullptr;
    const DWORD var = *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(original_address(getter)) + 1);
    if (var < 0x600000 || var >= 0x720000 || !readable(at<void *>(var), 4))
        return nullptr;
    return *at<CClassInfo **>(var);
}

// ---- The running level ------------------------------------------------------------------------------------------


BYTE *running_layer()
{
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (!heap_pointer(world))
        return nullptr;
    return real_player_layer();
}

int real_parts(BYTE **out, int max)
{
    BYTE *layer = running_layer();
    if (!layer || !readable(layer + 0x50, 0x28))
        return 0;
    const DWORD columns = *reinterpret_cast<DWORD *>(layer + 0x60), rows = *reinterpret_cast<DWORD *>(layer + 0x64);
    const ZPtrArray *cells = *reinterpret_cast<ZPtrArray **>(layer + 0x68);
    if (columns > 4096 || rows > 4096 || !cells || !readable(cells, columns * rows * 16))
        return 0;
    int n = 0;
    for (DWORD i = 0; i < columns * rows; i++) {
        const ZPtrArray *c = &cells[i];
        if (!c->count || !readable(c->data, c->count * 4))
            continue;
        for (DWORD k = 0; k < c->count; k++) {
            BYTE *p = static_cast<BYTE *>(c->data[k]);
            if (!heap_pointer(p) || !readable(p, 0x54))
                continue;
            bool seen = false;
            for (int j = 0; j < n && !seen; j++)
                seen = out[j] == p;
            if (!seen && n < max)
                out[n++] = p;
        }
    }
    return n;
}

float random_float()
{
    switch (random() % 8) {
    case 0: {
        constexpr DWORD V[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x3f000000, 0x00000001, 0x80000001, 0x00800000,
                               0x4b000000, 0xcb000000, 0x4b7fffff, 0x58635fa9, 0xd8635fa9, 0x3effffff, 0x3f7fffff};
        return raw_float(V[random() % (sizeof V / 4)]);
    }
    case 1: return (float)((int)(random() % 2000001) - 1000000) / 64.0f;
    case 2: return (float)((int)(random() % 20001) - 10000) + (float)(random() % 1000) / 999.0f;
    default: return (float)((int)(random() % 4001) - 2000) / ((random() & 1) ? 1.0f : 4.0f);
    }
}

}  // namespace zax::misc
