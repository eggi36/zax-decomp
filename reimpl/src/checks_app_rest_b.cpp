// Checks of app_rest_b.cpp against the original, in the game thread (side 0 with the module's hooks out,
// side 1 through the reimplementation):
//  - the debug log: the file 0x6bdf48 none or a fake, depths around 1, fprintf 0x5d19bc a stand-in that
//    records the file, the format and the text (the two globals put back after);
//  - the frame rate counter: random sequences of constructor, reset, start and sample on a counter of each
//    side, the counter 0x53d150 and the frequency 0x53d170 stand-ins answering from the case (counter
//    steps around and above the frequency, wrapping its low dword, zero); the counter's bytes and the
//    rate (st0, 10 bytes) compared after every step;
//  - the application base on a fake application (0x280 bytes; the window, the display, the resource cache
//    and the resource lists fakes with recorder vtables, the music stream a fake block): the destructor and
//    the deleting destructor (Zip_UnmountAll, the stream's 0x4e2020, operator delete, 0x42b6a0 and the
//    lists' reverse call stand-ins; the error reporter and the flag 0x6bda40 put back), the frame (the frame
//    timers' start and sample, the music update and volume, the display's 0x567ab0 stand-ins; the frame
//    globals put back), the switch between active and inactive (Keys::ClearAll and the mouse's
//    0x5185e0/0x5185d0 stand-ins) and the start-up loading (the blend tables, the sound manager and the INI
//    stand-ins). Nothing of the running game changes.
#include "check_util.h"
#include "checks_inventory_common.h"

namespace zax {

int hooks_app_rest_b(Hook *out);

namespace {

DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }
template <typename T> T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }

void out_() { invcheck::hooks_out(); }
void in_() { invcheck::hooks_in(); }

DWORD g_events[2][256];
int g_nevents[2];
int g_side;
void ev(DWORD v)
{
    if (g_nevents[g_side] < 256)
        g_events[g_side][g_nevents[g_side]] = v;
    g_nevents[g_side]++;
}
bool same_events() { return g_nevents[0] == g_nevents[1] && !memcmp(g_events[0], g_events[1], 4 * (g_nevents[0] < 256 ? g_nevents[0] : 256)); }
void begin_side(int side)
{
    g_side = side;
    g_nevents[side] = 0;
}
unsigned text_hash(const char *s)
{
    if (!s)
        return 0;
    unsigned h = 2166136261u;
    for (; *s; s++)
        h = (h ^ (BYTE)*s) * 16777619u;
    return h;
}
// A stand-in's answer: different for every event of the side
DWORD answer() { return 0x5e000000u + (DWORD)g_nevents[g_side] * 0x111u; }

// ---- The debug log ----------------------------------------------------------------------------------------

int __cdecl s_fprintf(DWORD file, const char *fmt, const char *text)
{
    ev(0x19bc);
    ev(file);
    ev(text_hash(fmt));
    ev(text_hash(text));
    return (int)answer();
}

// With the launcher's /DebugLog (as in the VM jobs) the launcher has put its own section log at both
// addresses and the hooks are not installed: side 0 then runs the original with its first bytes put back
// for the call, side 1 the reimplementation directly (its address from the hook table)
struct Direct {
    DWORD address;
    void *fn;
    BYTE expected[5], saved[5];
    bool hooked;
};
Direct g_direct[2] = {{0x4a2650, nullptr, {}, {}, false}, {0x4a2670, nullptr, {}, {}, false}};
void direct_setup()
{
    Hook list[64];
    const int n = hooks_app_rest_b(nullptr);
    if (n > 64)
        return;
    hooks_app_rest_b(list);
    for (Direct &d : g_direct)
        for (int i = 0; i < n; i++)
            if (list[i].address == d.address) {
                d.fn = list[i].replacement;
                memcpy(d.expected, list[i].expected, 5);
                d.hooked = original_address(d.address) != at<void *>(d.address);
            }
}
// the function to call on a side (and the bytes put back for side 0 when the hook is not installed)
void *direct_begin(Direct &d, int side)
{
    if (d.hooked)
        return at<void *>(d.address);
    if (side == 1)
        return d.fn;
    memcpy(d.saved, at<void *>(d.address), 5);
    write_code(at<void *>(d.address), d.expected);
    return at<void *>(d.address);
}
void direct_end(Direct &d, int side)
{
    if (!d.hooked && side == 0)
        write_code(at<void *>(d.address), d.saved);
}

void test_debug_log(Tally &t)
{
    direct_setup();
    SavedBytes saved;
    save_bytes(saved, 0x6bdf48, 0x24);   // the file 0x6bdf48 ... the depth 0x6bdf68
    const char *const names[] = {"Loading", "", "A section with a long name"};
    for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++) {
        DWORD r[2][2], depth[2];
        for (int side = 0; side < 2; side++) {
            begin_side(side);
            g_seed = seed;
            *at<DWORD *>(0x6bdf48) = random() % 3 ? 0x7e57f11e : 0;
            *at<DWORD *>(0x6bdf68) = random() % 3;
            const char *name = names[random() % 3];
            CodePatch p;
            patch_jump(p, 0x5d19bc, (void *)&s_fprintf);
            if (side == 0)
                out_();
            void *begin = direct_begin(g_direct[0], side), *end = direct_begin(g_direct[1], side);
            r[side][0] = reinterpret_cast<DWORD(__fastcall *)(const char *)>(begin)(name);
            r[side][1] = reinterpret_cast<DWORD(__cdecl *)()>(end)();
            r[side][1] += reinterpret_cast<DWORD(__cdecl *)()>(end)() * 3;
            direct_end(g_direct[1], side);
            direct_end(g_direct[0], side);
            if (side == 0)
                in_();
            unpatch(p);
            depth[side] = *at<DWORD *>(0x6bdf68);
        }
        char d[120];
        wsprintfA(d, "[debug log seed %u: %x %x / %x %x, depth %u / %u]", seed, r[0][0], r[0][1], r[1][0], r[1][1], depth[0], depth[1]);
        t.check(!memcmp(r[0], r[1], sizeof r[0]) && depth[0] == depth[1] && same_events(), "DebugLog", d);
    }
    restore_bytes(saved);
    log("  app_rest_b: debug log hooks %s", g_direct[0].hooked && g_direct[1].hooked ? "installed" : "not installed (the launcher's /DebugLog): called directly");
}

// ---- The frame rate counter ----------------------------------------------------------------------------------

unsigned long long g_counter;
DWORD g_frequency;
unsigned long long __cdecl s_counter()
{
    ev(0xd150);
    return g_counter;
}
unsigned long long __cdecl s_frequency()
{
    ev(0xd170);
    return (unsigned long long)g_frequency | 0x1234500000000ull;
}

void test_frame_rate(Tally &t)
{
    for (unsigned seed = 1; seed <= (unsigned)scaled(80); seed++) {
        alignas(16) BYTE c[2][0x110];
        unsigned states[2][48];
        int steps = 0;
        for (int side = 0; side < 2; side++) {
            begin_side(side);
            g_seed = seed;
            for (DWORD i = 0; i < 0x110; i += 4)
                fl<DWORD>(c[side], i) = random();
            g_frequency = random() % 4 ? 3579545 : random() % 1000;
            g_counter = random() % 4 ? (unsigned long long)random() << 8 : 0;
            CodePatch a, b;
            patch_jump(a, 0x53d150, (void *)&s_counter);
            patch_jump(b, 0x53d170, (void *)&s_frequency);
            if (side == 0)
                out_();
            steps = 0;
            at<void *(THISCALL *)(void *)>(0x4a81f0)(c[side]);
            for (int k = 0; k < 44; k++) {
                const DWORD op = random() % 12;
                DWORD r[3] = {};
                if (op == 0)
                    r[0] = at<DWORD(THISCALL *)(void *)>(0x4a8240)(c[side]);
                else if (op == 1)
                    r[0] = pv(at<void *(THISCALL *)(void *)>(0x4a81f0)(c[side])) - pv(c[side]);
                else if (op == 2)
                    at<void(THISCALL *)(void *)>(0x4a83c0)(c[side]);
                else {
                    const long double v = at<long double(THISCALL *)(void *)>(0x4a8290)(c[side]);
                    memcpy(r, &v, 10);
                }
                // the counter moves on: by about a frame, by more than the frequency, back, or over its low dword
                const DWORD how = random() % 10;
                if (how < 6)
                    g_counter += 10000 + random() % 200000;
                else if (how == 6)
                    g_counter += (unsigned long long)g_frequency * (1 + random() % 3);
                else if (how == 7)
                    g_counter -= random() % 1000;
                else if (how == 8)
                    g_counter += 0x100000000ull - random() % 50000;
                states[side][steps++] = fnv(fnv(2166136261u, c[side], 0x10c), r, 12);
            }
            if (side == 0)
                in_();
            unpatch(b);
            unpatch(a);
        }
        int first = -1;
        for (int k = 0; k < steps && first < 0; k++)
            if (states[0][k] != states[1][k])
                first = k;
        char d[120];
        wsprintfA(d, "[frame rate seed %u: first different step %d of %d]", seed, first, steps);
        t.check(first < 0 && same_events(), "FrameRate", d);
    }
}

// ---- The application base -------------------------------------------------------------------------------------

alignas(16) BYTE g_app[2][0x280];
alignas(16) BYTE g_stream[2][0x40];
DWORD g_window_vtable[4], g_display_vtable[4], g_cache_vtable[12];
alignas(16) BYTE g_window[8], g_display[8], g_cache[8], g_lists[8];
DWORD fake_id(const void *o)
{
    if (o == g_window)
        return 1;
    if (o == g_display)
        return 2;
    if (o == g_cache)
        return 3;
    if (o == g_lists)
        return 4;
    if (o == g_stream[0] || o == g_stream[1])
        return 5;
    if (o == g_app[0] || o == g_app[1])
        return 6;
    return pv(o);
}
// The fakes' slots: each records its object and number with one or no argument
template <int SLOT> DWORD THISCALL fake_slot0(void *o)
{
    ev(0x51000 + SLOT * 0x10 + fake_id(o));
    return answer();
}
template <int SLOT> DWORD THISCALL fake_slot1(void *o, DWORD a)
{
    ev(0x51000 + SLOT * 0x10 + fake_id(o));
    ev(a);
    return answer();
}
DWORD THISCALL s_this0(void *o)
{
    ev(0x7410);
    ev(fake_id(o));
    return answer();
}
DWORD THISCALL s_this1(void *o, DWORD a)
{
    ev(0x7411);
    ev(fake_id(o));
    ev(a);
    return answer();
}
DWORD __cdecl s_plain()
{
    ev(0x7400);
    return answer();
}
void __cdecl s_delete(void *p)
{
    ev(0xde1e);
    ev(fake_id(p));
}
void *__cdecl s_lists() { return ev(0x35d0), g_lists; }
void *__cdecl s_cache() { return ev(0xaef0), g_cache; }
long double g_rate;
long double THISCALL s_sample(void *timer)
{
    ev(0x8290);
    ev(pv(timer));
    return g_rate;
}
DWORD __fastcall s_blend(const char *a, const char *b)
{
    ev(0x3000);
    ev(text_hash(a) ^ text_hash(b));
    return answer();
}
DWORD THISCALL s_sounds(void *o, const char *a, const char *b, const char *c, const char *d)
{
    ev(0xe2f0);
    ev(pv(o));
    ev(text_hash(a) ^ text_hash(b) * 3 ^ text_hash(c) * 5 ^ text_hash(d) * 7);
    return answer();
}
DWORD g_ini_answer;
DWORD __fastcall s_ini_int(void *file, void *section, const char *key, DWORD def)
{
    ev(0x4670);
    ev(pv(file) ^ pv(section));
    ev(text_hash(key));
    ev(def);
    return g_ini_answer;
}

struct Stand {
    DWORD address;
    void *fn;
};
CodePatch g_stand_patches[16];
int g_nstand;
void stand(DWORD address, void *fn) { patch_jump(g_stand_patches[g_nstand++], address, fn); }
void unstand()
{
    while (g_nstand)
        unpatch(g_stand_patches[--g_nstand]);
}

void make_app(int side)
{
    BYTE *a = g_app[side];
    for (DWORD i = 0; i < 0x280; i += 4)
        fl<DWORD>(a, i) = random();
    fl<DWORD>(a, 0) = 0x5f73e8;
    fl<DWORD>(a, 4) = random() % 4 ? pv(g_display) : 0;
    fl<DWORD>(a, 0x34) = random() % 4 ? pv(g_window) : 0;
    fl<DWORD>(a, 0x38) = random() % 8;
    a[0x60] = (BYTE)(random() % 2);
    a[0x266] = (BYTE)(random() % 2);
    a[0x267] = (BYTE)(random() % 2);
    a[0x268] = (BYTE)(random() % 2);
    // the music after a fade: none, short or long (the next music is a char[0xff] at +0x68)
    const char *const names[] = {"", "Music/Act 1", "Music/A very long name of a piece of music in a folder"};
    const char *m = names[random() % 3];
    DWORD k = 0;
    for (; m[k]; k++)
        a[0x167 + k] = (BYTE)m[k];
    a[0x167 + k] = 0;
    fl<DWORD>(a, 0x26c) = random() % 4 ? bits(0.5f + (float)(random() % 100) * 0.01f) : (random() % 2 ? 0 : 0x7fc00000);
    fl<DWORD>(a, 0x270) = bits((float)(random() % 100) * 0.01f);
    fl<DWORD>(a, 0x274) = random() % 4 ? pv(g_stream[side]) : 0;
}

void test_app(Tally &t)
{
    // the window: slot 0 (deleting destructor, 1) and 2 (seconds); the display: slot 1 (deleting destructor,
    // 1); the resource cache: slots 1, 4, 8, 9 without arguments
    g_window_vtable[0] = pv((void *)&fake_slot1<0>);
    g_window_vtable[2] = pv((void *)&fake_slot1<2>);
    g_display_vtable[1] = pv((void *)&fake_slot1<1>);
    g_cache_vtable[1] = pv((void *)&fake_slot0<1>);
    g_cache_vtable[4] = pv((void *)&fake_slot0<4>);
    g_cache_vtable[8] = pv((void *)&fake_slot0<8>);
    g_cache_vtable[9] = pv((void *)&fake_slot0<9>);
    fl<DWORD>(g_window, 0) = pv(g_window_vtable);
    fl<DWORD>(g_display, 0) = pv(g_display_vtable);
    fl<DWORD>(g_cache, 0) = pv(g_cache_vtable);
    SavedBytes globals, flag, frames;
    save_bytes(globals, 0x6c02cc, 4);
    save_bytes(flag, 0x6bda40, 1);
    save_bytes(frames, 0x6c02d0, 0x14);   // the rate 0x6c02d0 and the frame numbers 0x6c02d8, 0x6c02e0
    for (unsigned seed = 1; seed <= (unsigned)scaled(240); seed++) {
        const DWORD op = seed % 6;
        DWORD r[2];
        DWORD g[2][4];
        for (int side = 0; side < 2; side++) {
            begin_side(side);
            g_seed = seed;
            make_app(side);
            *at<BYTE *>(0x6bda40) = (BYTE)(random() % 2);
            *at<DWORD *>(0x6c02d8) = random() % 8;
            *at<DWORD *>(0x6c02e0) = random() % 8;
            g_rate = (long double)(random() % 1000) / 7.0L;
            g_ini_answer = random() % 2 ? 0 : random();
            const DWORD seconds = random() % 8 ? bits((float)(random() % 1000) * 0.001f) : 0x7fc00000;
            const DWORD active = random() % 3;
            const DWORD flags = random() % 2;
            if (side == 0)
                out_();
            g_nstand = 0;
            if (op <= 1) {
                stand(0x4a1120, (void *)&s_plain);    // Zip_UnmountAll
                stand(0x4e2020, (void *)&s_this0);    // the music stream's destructor
                stand(0x5d0320, (void *)&s_delete);   // operator delete
                stand(0x42b6a0, (void *)&s_plain);
                stand(0x4a35d0, (void *)&s_lists);    // ResourceLists_Get
                stand(0x4a37a0, (void *)&s_this0);    // ResourceLists_CallAllReverse
                stand(0x57aef0, (void *)&s_cache);    // ResourceCache_Get
            } else if (op <= 3) {
                stand(0x4a83c0, (void *)&s_this0);    // FrameRate::Start
                stand(0x57aef0, (void *)&s_cache);
                stand(0x57af90, (void *)&s_this0);    // ResourceCache_NextStamp
                stand(0x587640, (void *)&s_this1);    // the stream's volume
                stand(0x4ce510, (void *)&s_this0);    // the music update
                stand(0x567ab0, (void *)&s_this1);    // the display's frame
                stand(0x4a8290, (void *)&s_sample);   // FrameRate::Sample
            } else if (op == 4) {
                stand(0x4e27a0, (void *)&s_this1);    // Keys::ClearAll
                stand(0x57aef0, (void *)&s_cache);
                stand(0x5185e0, (void *)&s_this0);
                stand(0x5185d0, (void *)&s_this0);
            } else {
                stand(0x433000, (void *)&s_blend);    // InitBlendTables
                stand(0x57e2f0, (void *)&s_sounds);   // the sound manager's folders
                stand(0x4a4670, (void *)&s_ini_int);  // Ini_GetInt
                stand(0x57e590, (void *)&s_this0);    // all sounds loaded
            }
            if (op == 0)
                r[side] = at<DWORD(THISCALL *)(void *)>(0x4cdcc0)(g_app[side]);
            else if (op == 1)
                r[side] = fake_id(at<void *(THISCALL *)(void *, DWORD)>(0x4cdc50)(g_app[side], flags));
            else if (op <= 3) {
                at<void(THISCALL *)(void *, DWORD)>(0x4cdfd0)(g_app[side], seconds);
                r[side] = 0;
            } else if (op == 4)
                r[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x4ce400)(g_app[side], active) & 0xff;
            else
                r[side] = at<DWORD(THISCALL *)(void *, DWORD)>(0x4cdf70)(g_app[side], seconds);
            unstand();
            if (side == 0)
                in_();
            g[side][0] = *at<DWORD *>(0x6c02cc);
            g[side][1] = *at<BYTE *>(0x6bda40);
            g[side][2] = *at<DWORD *>(0x6c02d0);
            g[side][3] = *at<DWORD *>(0x6c02d8) + 0x100 * *at<DWORD *>(0x6c02e0);
            restore_bytes(globals);
        }
        // the application's pointer to the side's stream block by what it is
        char d[160];
        wsprintfA(d, "[application op %u seed %u: %x / %x, %d / %d events, globals %x %x %x / %x %x %x]", op, seed, r[0], r[1], g_nevents[0], g_nevents[1],
                  g[0][0], g[0][2], g[0][3], g[1][0], g[1][2], g[1][3]);
        bool same_bytes = true;
        for (DWORD i = 0; i < 0x280; i += 4)
            if (fl<DWORD>(g_app[0], i) != fl<DWORD>(g_app[1], i) && !(fl<DWORD>(g_app[0], i) == pv(g_stream[0]) && fl<DWORD>(g_app[1], i) == pv(g_stream[1])))
                same_bytes = false;
        t.check(r[0] == r[1] && same_bytes && !memcmp(g[0], g[1], sizeof g[0]) && same_events(), "App", d);
    }
    restore_bytes(frames);
    restore_bytes(flag);
    restore_bytes(globals);
}

}  // namespace

ZAX_CHECKS(app_rest_b)
{
    (void)cls;
    (void)n;
    invcheck::g_hook_table = hooks_app_rest_b;
    if (!invcheck::arena_setup()) {
        log("Check app_rest_b: MISMATCH (no arena)");
        return;
    }
    Tally log_{"app_rest_b: debug log"}, rate{"app_rest_b: frame rate counter"}, app{"app_rest_b: application base"};
    test_debug_log(log_);
    log_.report();
    test_frame_rate(rate);
    rate.report();
    test_app(app);
    app.report();
}

}  // namespace zax
