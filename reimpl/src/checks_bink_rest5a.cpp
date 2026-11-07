// Checks of the Bink wrapper's binkw32 callers and Bink's file callbacks (bink_rest5a.cpp) against the
// original, in the game thread (side 0 with the module's hooks out, side 1 through the reimplementation;
// checks_net_common.h). No movie plays: the binkw32 imports, RADTimerRead and Sleep point to recording
// stand-ins for the test (Sleep only for the check thread; the game's other threads get the real one).
//  - The Bink system: a fake system object with 0-4 records (fake BINK handles with frame counts), a scratch
//    block or none; random sequences of OpenMovie (a name or the logo, every flag combination, BinkOpen
//    failing), CloseMovie (-1 too), Wait, DecodeToFrame (8, 16, 24, 32 bits and other depths, the 8-bit
//    path through a fake byte table put into 0x633388 for the test), GetSummary, SetPause, and the
//    destructor. Compared: eax, the call log with the arguments (BinkCopyToBuffer's destination by kind),
//    the system, the records, the scratch block's and the destinations' bytes.
//  - The file callbacks on a BINKIO block over the real "Movies/JoWooD.bik" (opened by BinkIo_Open, the real
//    FileBuf): SetInfo with random buffer sizes and simulated rates, ReadHeader, then random sequences of
//    ReadFrame (the current position, a jump inside the buffered data, a seek elsewhere, -1), Idle (the
//    background fill) and GetBufferSize, with the background counter set before some calls (the wait loop,
//    BinkService clears it after 1-3 calls) and the foreground counter before some Idle calls (the busy
//    path), then Close. Compared: the results, the block (the FileBuf by first appearance), the bytes read,
//    the log (Sleep, BinkService, RADTimerRead).
#include "checks_net_common.h"
#include "exe_base.h"

namespace zax {

int hooks_bink_rest5a(Hook *out);

namespace {

using namespace netcheck;

DWORD g_tid;
inline DWORD tid()
{
    DWORD t;
    __asm__("movl %%fs:0x24, %0" : "=r"(t));
    return t;
}
DWORD buffer_kind(const void *p)
{
    if (!p)
        return 0;
    if (in_arena(ptrv(p)))
        return ptrv(p);
    return 2;
}

// ---- The stand-ins -------------------------------------------------------------------------------------------

DWORD g_time;
BYTE *g_io;
int g_service_left;
DWORD g_real_sleep;

DWORD STDCALL s_pause(DWORD h, DWORD p) { ev(0x801, h, p); return random() & 0xff; }
DWORD STDCALL s_buffer_close(DWORD b) { ev(0x802, b); return 0; }
DWORD STDCALL s_close(DWORD h) { ev(0x803, h); return 0; }
DWORD STDCALL s_do_frame(DWORD h) { ev(0x804, h); return 0; }
DWORD STDCALL s_next_frame(DWORD h) { ev(0x805, h); return 0; }
DWORD STDCALL s_summary(DWORD h, DWORD *sum)
{
    ev(0x806, h, buffer_kind(sum) != 0);
    for (int i = 0; i < 0x1f; i++)
        sum[i] = random();
    return 0;
}
BYTE *g_handles[4];
DWORD STDCALL s_open(const char *name, DWORD flags)
{
    ev(0x807, name ? fnv_text(2166136261u, name) : 0, flags);
    const DWORD r = rnd(4);
    return r == 0 ? 0 : ptrv(g_handles[r]);
}
DWORD STDCALL s_sound_track(DWORD t) { ev(0x808, t); return 0; }
const char LOGO[] = "logo";
DWORD STDCALL s_logo() { ev(0x809); return ptrv(LOGO); }
DWORD STDCALL s_sound_system(DWORD fn, DWORD ds) { ev(0x80a, fn, ds); return 0; }
DWORD STDCALL s_set_io(DWORD fn) { ev(0x80b, fn); return 0; }
DWORD STDCALL s_wait(DWORD h) { ev(0x80c, h); return rnd(2); }
DWORD STDCALL s_service(DWORD h)
{
    ev(0x80d, h);
    if (g_io && --g_service_left <= 0)
        *(DWORD *)(g_io + 0x60) = 0;
    return 0;
}
DWORD STDCALL s_copy(DWORD h, BYTE *dest, DWORD pitch, DWORD height, DWORD x, DWORD y, DWORD flags)
{
    ev(0x80e, h, buffer_kind(dest), pitch);
    ev(0x80f, height, x, y);
    ev(0x810, flags);
    for (DWORD r = 0; r < height; r++)
        for (DWORD i = 0; i < pitch; i++)
            dest[r * pitch + i] = (BYTE)(random() >> 13);
    return 0;
}
DWORD STDCALL s_timer()
{
    g_time += 1 + rnd(12);
    ev(0x811, g_time);
    return g_time;
}
DWORD STDCALL s_sleep(DWORD ms)
{
    if (tid() != g_tid)
        return ((DWORD(STDCALL *)(DWORD))(uintptr_t)g_real_sleep)(ms);
    ev(0x812, ms);
    return 0;
}

struct Patches {
    ImportPatch p[17];
    void on()
    {
        const struct {
            DWORD iat;
            void *fn;
        } list[] = {{0x5ea2f8, (void *)&s_pause},  {0x5ea2fc, (void *)&s_buffer_close}, {0x5ea300, (void *)&s_close},
                    {0x5ea304, (void *)&s_do_frame}, {0x5ea308, (void *)&s_next_frame}, {0x5ea30c, (void *)&s_summary},
                    {0x5ea310, (void *)&s_open},   {0x5ea314, (void *)&s_sound_track},  {0x5ea318, (void *)&s_logo},
                    {0x5ea31c, (void *)&s_sound_system}, {0x5ea324, (void *)&s_set_io}, {0x5ea328, (void *)&s_wait},
                    {0x5ea32c, (void *)&s_service}, {0x5ea330, (void *)&s_copy},       {0x5ea334, (void *)&s_timer},
                    {0x5ea07c, (void *)&s_sleep}};
        g_real_sleep = *at<DWORD *>(0x5ea07c);
        int i = 0;
        for (auto &e : list)
            patch_import(p[i++], e.iat, e.fn);
    }
    void off()
    {
        for (int i = 15; i >= 0; i--)
            unpatch_import(p[i]);
    }
};

// ---- The Bink system -------------------------------------------------------------------------------------------

BYTE *g_byte_table;
bool g_eight_bit;

DWORD sys_call(DWORD fn, BYTE *s, int n, DWORD a = 0, DWORD b = 0, DWORD c = 0, DWORD d = 0, DWORD e = 0, DWORD f6 = 0, DWORD g7 = 0)
{
    switch (n) {
    case 0: return at<DWORD(THISCALL *)(BYTE *)>(fn)(s);
    case 1: return at<DWORD(THISCALL *)(BYTE *, DWORD)>(fn)(s, a);
    case 2: return at<DWORD(THISCALL *)(BYTE *, DWORD, DWORD)>(fn)(s, a, b);
    default: return at<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(fn)(s, a, b, c, d, e, f6, g7);
    }
}

void st_system(BYTE *s)
{
    st(s[4]);
    st(s[0x24]);
    st(*(DWORD *)s);
    const DWORD n = *(DWORD *)(s + 0xc);
    st(n);
    BYTE *r = *(BYTE **)(s + 8);
    st(r != nullptr);
    for (DWORD i = 0; r && i < n; i++) {   // the fields AddMovie sets (+0xc, +0x10, +0x15 stay as the heap had them)
        BYTE *e = r + i * 0x18;
        st_raw(*(DWORD *)e);
        st_raw(*(DWORD *)(e + 4));
        st_raw(*(DWORD *)(e + 8));
        st_raw(e[0x14]);
    }
    BYTE *scratch = *(BYTE **)(s + 0x1c);
    st(scratch != nullptr);
    st(*(DWORD *)(s + 0x20));
    if (scratch)
        st_bytes(scratch, *(DWORD *)(s + 0x20));
}

void system_case(int k)
{
    Patches pt;
    pt.on();
    g_io = nullptr;
    g_time = 1000;
    for (int i = 0; i < 4; i++) {
        g_handles[i] = (BYTE *)arena(0x40);
        *(DWORD *)(g_handles[i] + 8) = 1 + rnd(50);
    }
    BYTE *s = (BYTE *)arena(0x40);
    *(DWORD *)s = 0x5eede0;
    s[4] = (BYTE)rnd(2);
    s[0x24] = (BYTE)rnd(2);
    const DWORD n = rnd(5);
    *(DWORD *)(s + 0xc) = n;
    BYTE *recs = n ? (BYTE *)heap_alloc(n * 0x18) : nullptr;
    *(BYTE **)(s + 8) = recs;
    for (DWORD i = 0; i < n; i++) {
        BYTE *r = recs + i * 0x18;
        memset(r, 0, 0x18);
        *(DWORD *)r = rnd(5) ? ptrv(g_handles[rnd(4)]) : 0;
        *(DWORD *)(r + 4) = rnd(2) ? 0x5000 + rnd(100) : 0;
        *(DWORD *)(r + 8) = rnd(60);
        r[0x14] = (BYTE)rnd(2);
    }
    if (rnd(2)) {
        const DWORD size = rnd(3000);
        *(DWORD *)(s + 0x20) = size;
        *(BYTE **)(s + 0x1c) = (BYTE *)heap_alloc(size ? size : 1);
        memset(*(BYTE **)(s + 0x1c), 0x5a, size);
    }
    BYTE *dest = (BYTE *)arena(0x1000);
    bool destroyed = false;
    for (int step = 0, steps = 1 + (int)rnd(6); step < steps && !destroyed; step++) {
        const DWORD count = *(DWORD *)(s + 0xc);
        DWORD id = count ? rnd(count) : 0xffffffff;
        DWORD r = 0;
        DWORD op = rnd(8);
        if (op == 3 || op == 4) {   // DecodeToFrame reads the handle's frame count: a record with a handle
            BYTE *recs_now = *(BYTE **)(s + 8);
            id = 0xffffffff;
            for (DWORD i = 0, start = rnd(count ? count : 1); i < count; i++)
                if (*(DWORD *)(recs_now + ((start + i) % count) * 0x18)) {
                    id = (start + i) % count;
                    break;
                }
            if (id == 0xffffffff)
                op = 0;
        }
        if (op == 0 || (op >= 3 && op <= 6 && !count)) {
            const char *names[] = {"Movies/Intro.bik", "Movies/JoWooD.bik", nullptr};
            r = sys_call(0x431fd0, s, 2, ptrv(names[rnd(3)]), rnd(4) | (random() & 0xf0));
        } else if (op == 1) {
            r = sys_call(0x432080, s, 1, rnd(3) ? id : 0xffffffff);
        } else if (op == 2) {
            r = sys_call(0x4320d0, s, 1, rnd(3) ? id : 0xffffffff);
        } else if (op == 3 || op == 4) {
            static constexpr DWORD BITS[] = {8, 16, 24, 32, 15, 12, 40, 7};
            DWORD bits = BITS[rnd(8)];
            if (bits == 8 && !g_eight_bit)
                bits = 16;
            const DWORD pitch = 1 + rnd(40), height = rnd(9);
            memset(dest, 0x11, 0x1000);
            r = sys_call(0x432110, s, 7, id, ptrv(dest), pitch, height, rnd(4), rnd(4), bits);
            st_bytes(dest, pitch * height * 4 < 0x1000 ? pitch * height * 4 : 0x1000);
        } else if (op == 5) {
            DWORD *out = (DWORD *)arena(16);
            r = sys_call(0x432320, s, 2, id, ptrv(out));
            st_bytes(out, 16);
        } else if (op == 6) {
            r = sys_call(0x4323a0, s, 2, id, random());
        } else {
            r = sys_call(0x431e50, s, 0);
            destroyed = true;
            r = 0;   // eax: operator delete's when there was a scratch block (not defined)
        }
        // A record address (Wait with 1 in its low byte, DecodeToFrame) compared with the record the call used: the
        // records' block may be one AddMovie freed before on one side only, so its address is not compared
        const DWORD rec = *(DWORD *)(s + 8) + id * 0x18;
        if (r >= 0x10000 && (r & ~0xffu) == (rec & ~0xffu) && (op == 2 || op == 3 || op == 4))
            ev_raw(0x821, op, r == rec ? 0x100 : r & 0xff);
        else
            ev(0x820, op, r);
        st_system(s);
    }
    if (!destroyed) {
        if (BYTE *r = *(BYTE **)(s + 8))
            heap_free(r);
        if (BYTE *b = *(BYTE **)(s + 0x1c))
            heap_free(b);
    }
    (void)k;
    pt.off();
}

// ---- The file callbacks ---------------------------------------------------------------------------------------------

DWORD io_call(DWORD fn, int n, DWORD a = 0, DWORD b = 0, DWORD c = 0, DWORD d = 0, DWORD e = 0)
{
    switch (n) {
    case 1: return at<DWORD(STDCALL *)(DWORD)>(fn)(a);
    case 2: return at<DWORD(STDCALL *)(DWORD, DWORD)>(fn)(a, b);
    case 4: return at<DWORD(STDCALL *)(DWORD, DWORD, DWORD, DWORD)>(fn)(a, b, c, d);
    default: return at<DWORD(STDCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(fn)(a, b, c, d, e);
    }
}
void st_io(BYTE *io)
{
    for (DWORD o = 0; o < 0x80; o += 4)
        st(*(DWORD *)(io + o));
}
bool g_file_ok = true;

void io_case(int k)
{
    Patches pt;
    pt.on();
    g_time = 5000;
    BYTE *io = (BYTE *)arena(0xd0);
    g_io = io;
    g_service_left = 0;
    if (!at<DWORD(STDCALL *)(BYTE *, const char *, DWORD)>(0x432aa0)(io, "Movies/JoWooD.bik", 0)) {
        g_file_ok = false;
        g_io = nullptr;
        pt.off();
        return;
    }
    const DWORD io_ = ptrv(io);
    *(DWORD *)(io + 0x18) = 0x7770 + rnd(4);   // the Bink handle BinkService gets
    const DWORD bufsize = 0x2000 + rnd(0xe000);
    BYTE *buf = (BYTE *)arena(0x10000);
    BYTE *dest = (BYTE *)arena(0x4000);
    const DWORD file_size = 0x8000 + rnd(0x80000);
    const DWORD rates[] = {0, 0, 0, 50000 + rnd(2000000)};
    auto busy_background = [&] {
        if (rnd(6) == 0) {
            *(DWORD *)(io + 0x60) = 1;
            g_service_left = 1 + (int)rnd(3);
        }
    };
    busy_background();
    DWORD r = io_call(0x4327e0, 5, io_, ptrv(buf), bufsize, file_size, rates[rnd(4)]);
    ev(0x830, r);
    st_io(io);
    r = io_call(0x432770, 2, io_, random() & 0xfffff);
    ev(0x831, r);
    for (int i = 0, n = 1 + (int)rnd(2); i < n; i++) {
        busy_background();
        const DWORD size = 14 + rnd(64);
        r = io_call(0x4323d0, 4, io_, rnd(3) ? rnd(0x100) : 0xffffffff, ptrv(dest), size);
        ev(0x832, r);
        st_bytes(dest, size);
        st_io(io);
    }
    for (int step = 0, steps = 2 + (int)rnd(14); step < steps; step++) {
        const DWORD op = rnd(6);
        if (op <= 2) {
            busy_background();
            const DWORD pos = *(DWORD *)(io + 0x50), filepos = *(DWORD *)(io + 0x4c);
            DWORD offset;
            switch (rnd(5)) {
            case 0: offset = pos; break;
            case 1: offset = filepos > pos ? pos + 1 + rnd(filepos - pos) : pos; break;
            case 2: offset = rnd(file_size); break;
            case 3: offset = pos > 0x100 ? pos - rnd(0x100) : pos; break;
            default: offset = 0xffffffff; break;
            }
            const DWORD size = rnd(4) ? 1 + rnd(0x800) : rnd(0x3000);
            memset(dest, 0x33, 0x3000);
            r = io_call(0x432480, 5, io_, rnd(100), offset, ptrv(dest), size);
            ev(0x833, r, size);
            st_bytes(dest, size);
        } else if (op <= 4) {
            const DWORD pre = rnd(8);
            if (pre == 0)
                *(DWORD *)(io + 0x5c) = 1;
            if (pre == 1)
                *(DWORD *)(io + 0x24) = rnd(2);
            r = io_call(0x4328d0, 1, io_);
            ev(0x834, r);
            if (pre == 0)
                *(DWORD *)(io + 0x5c) = 0;
            *(DWORD *)(io + 0x60) = 0;
        } else {
            busy_background();
            r = io_call(0x432770, 2, io_, random() & 0xfffff);
            ev(0x835, r);
        }
        st_io(io);
    }
    st_bytes(buf, bufsize);
    busy_background();
    r = io_call(0x432870, 1, io_);
    ev(0x836, r - io_);
    st_io(io);
    g_io = nullptr;
    (void)k;
    pt.off();
}

}  // namespace

ZAX_CHECKS(bink_rest5a)
{
    (void)cls;
    (void)n;
    g_table = hooks_bink_rest5a;
    g_tid = tid();
    // The 8-bit path's byte table: the highest index the three tables can make, a fake table that size
    DWORD top = 0;
    const DWORD tables[] = {0x630378u, 0x6753b8u, 0x633b94u};
    for (DWORD t : tables) {
        DWORD m = 0;
        for (int i = 0; i < 256; i++)
            if (at<DWORD *>(t)[i] > m)
                m = at<DWORD *>(t)[i];
        top += m;
    }
    const DWORD saved_table = *at<DWORD *>(0x633388);
    g_eight_bit = top < 0x100000;
    if (g_eight_bit) {
        g_byte_table = (BYTE *)heap_alloc(top + 1);
        for (DWORD i = 0; i <= top; i++)
            g_byte_table[i] = (BYTE)(i * 7 + (i >> 8));
        DWORD old;
        VirtualProtect(at<void *>(0x633388), 4, PAGE_EXECUTE_READWRITE, &old);
        *at<BYTE **>(0x633388) = g_byte_table;
    } else {
        log("  bink: the 8-bit tables reach index %x, the 8-bit path is left out", top);
    }
    run_cases("bink: the Bink system (open, close, wait, decode, summary, pause, destructor)", scaled(400), 0x431e50u,
              [](int k) { system_case(k); });
    *at<DWORD *>(0x633388) = saved_table;
    if (g_byte_table)
        heap_free(g_byte_table);
    g_byte_table = nullptr;
    run_cases("bink: the file callbacks (set info, read header and frames, background, buffer size, close)", scaled(300), 0x4323d0u,
              [](int k) { io_case(k); });
    if (!g_file_ok)
        log("  bink: Movies/JoWooD.bik did not open, the file callback cases did nothing");
}

}  // namespace zax
