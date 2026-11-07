// Checks of sound_stream.cpp against the original, in the game thread.
//
// In the sound fake world (checks_sound_common.h): DirectSound (0x7136f4) and the stream's buffer are
// fake COM objects (the play and write positions, Lock's two parts of the ring buffer and the HRESULTs
// from the case), the readers fakes (Read gives the case's bytes, also fewer, Tell and Seek recorded),
// the reader factory, the ring buffer reads (sound_reader.cpp), Zax.CFG and exit are recorders, and the
// multimedia timer's timeSetEvent and timeKillEvent (the imports 0x5ea290/0x5ea288) point to recorders
// while the module's checks run. The holders and states lie in each side's arena.
//
// The game's stop waits in a loop until the callback has stopped the stream (with a thread, the timer
// thread does it): the cases give such states no callback that runs (+0xe 0), a buffer and a reader, and
// GetCurrentPosition succeeds, so every wait ends after one frame. The timer callback itself is called
// with every state (running, not playing, muted, stopping, no buffer, the end reached, positions around
// the write position and the 40,000-byte threshold, both silences, a queued reader that loops or not).
// Its hook site is never put out (the timer thread runs through it); its cases call the original through
// the trampoline.
#include "checks_sound_common.h"

namespace zax {

int hooks_sound_stream(Hook *out);

namespace {

using namespace sndc;

constexpr DWORD DSOUND = 0x7136f4, RING = 100000;
enum { K_DS, K_BUF, K_READER };
enum { F_DS, F_BUF, F_READER, N_READER = 3, F_FORMAT = F_READER + N_READER };
const DWORD NEVER_OUT[] = {0x587700};
const Module MODULE = {hooks_sound_stream, NEVER_OUT, 1};

DWORD c_reader(Call &) { return nv() % 6 == 0 ? 0 : ptrv(fake(F_READER + (int)(nv() % N_READER))); }
DWORD c_size(Call &c) { return dw(at<void *>(c.ecx), 0xc); }
DWORD c_ini(Call &) { return nv() % 3 ? 1 : 0; }
DWORD c_read_parts(Call &c)
{
    // (first, n1, second, n2): the case's bytes into both parts, the count given (also fewer)
    BYTE *p1 = at<BYTE *>(c.a[0]), *p2 = at<BYTE *>(c.a[2]);
    for (DWORD i = 0; i < c.a[1] && i < 0x200; i++)
        p1[i] = (BYTE)(i + 1);
    for (DWORD i = 0; p2 && i < c.a[3] && i < 0x200; i++)
        p2[i] = (BYTE)(i + 2);
    const DWORD v = nv(), all = c.a[1] + c.a[3];
    return v % 4 == 0 ? (all ? v % all : 0) : all;
}
DWORD c_create_buffer(Call &c)
{
    if (nv() % 5 == 0)
        return 0x8878000a;
    dw(at<void *>(c.a[2]), 0) = ptrv(fake(F_BUF));
    return 0;
}
// The ring buffer's memory of the case (both parts lie in it)
BYTE g_ring[0x800];
DWORD c_lock(Call &c)
{
    // (this, at, bytes, p1, n1, p2, n2, flags): two parts of at most 0x200 bytes (the real ones are bigger,
    // the code only writes what it is given)
    if (nv() % 6 == 0)
        return 0x887800a0;
    const DWORD v = nv();
    dw(at<void *>(c.a[3]), 0) = ptrv(g_ring);
    dw(at<void *>(c.a[4]), 0) = v % 0x200;
    const bool two = (v >> 12) % 3 == 0;
    dw(at<void *>(c.a[5]), 0) = two ? ptrv(g_ring + 0x400) : 0;
    dw(at<void *>(c.a[6]), 0) = two ? (v >> 16) % 0x200 : 0;
    return 0;
}
DWORD c_position(Call &c)
{
    const DWORD v = nv();
    dw(at<void *>(c.a[1]), 0) = v % RING;
    dw(at<void *>(c.a[2]), 0) = (v % RING + nv() % 50000) % (RING + 10);
    return 0;
}
DWORD c_tell(Call &) { return nv() % 50000; }
DWORD c_read(Call &c)
{
    BYTE *out = at<BYTE *>(c.a[0]);
    for (DWORD i = 0; i < c.a[1] && i < 0x200; i++)
        out[i] = (BYTE)(i * 5 + 1);
    const DWORD v = nv();
    return v % 4 == 0 ? (c.a[1] ? v % c.a[1] : 0) : c.a[1];
}
DWORD c_this(Call &c) { return c.ecx; }

const Spec DIRECT[] = {
    {0x57ca10, "SoundReader_ForFile", 'f', "", 'c', c_reader},
    {0x483750, "Reader_Size", 't', "", 'c', c_size},
    {0x57d330, "CSoundReader::ReadParts", 't', "pvpv", 'c', c_read_parts},
    {0x57d550, "CSoundReader::ReadMix", 't', "pvpvf", 'c', c_read_parts},
    {0x4a4670, "Ini_GetInt", 'f', "sv", 'c', c_ini},
    {0x5d0e36, "exit", 'c', "v", 'z', nullptr},
};
#define NOSLOT {0, nullptr, 0, "", 0, nullptr}
const Spec DS_SLOTS[] = {NOSLOT, NOSLOT, NOSLOT, {0, "IDirectSound::CreateSoundBuffer", 'C', "pDpv", 'c', c_create_buffer}};
const Spec BUF_SLOTS[] = {
    NOSLOT, NOSLOT, {0, "IDirectSoundBuffer::Release", 'C', "p", 'n', nullptr}, NOSLOT,
    {0, "IDirectSoundBuffer::GetCurrentPosition", 'C', "p--", 'c', c_position}, NOSLOT, NOSLOT, NOSLOT, NOSLOT, NOSLOT, NOSLOT,
    {0, "IDirectSoundBuffer::Lock", 'C', "pvv----v", 'c', c_lock},
    {0, "IDirectSoundBuffer::Play", 'C', "pvvv", 'h', nullptr},
    {0, "IDirectSoundBuffer::SetCurrentPosition", 'C', "pv", 'h', nullptr}, NOSLOT,
    {0, "IDirectSoundBuffer::SetVolume", 'C', "pv", 'h', nullptr}, NOSLOT, NOSLOT,
    {0, "IDirectSoundBuffer::Stop", 'C', "p", 'h', nullptr},
    {0, "IDirectSoundBuffer::Unlock", 'C', "ppvpv", 'h', nullptr},
};
const Spec READER_SLOTS[] = {
    NOSLOT, {0, "Reader::DeletingDtor", 't', "v", 'c', c_this}, {0, "Reader::Read", 't', "pv", 'c', c_read},
    {0, "Reader::Tell", 't', "", 'c', c_tell},
    {0, "Reader::Seek", 't', "v", 'n', nullptr},
};
#undef NOSLOT
const Standins STANDINS[] = {{DIRECT, sizeof DIRECT / sizeof DIRECT[0]}};

// ---- The timer imports ---------------------------------------------------------------------------------------

DWORD __stdcall fake_time_set(DWORD delay, DWORD resolution, DWORD fn, DWORD user, DWORD flags)
{
    ev(0x5ea290, delay * 0x10000 + resolution, fn, user);
    ev(0x5ea291, flags);
    return 0x7130 + nv() % 8;
}
DWORD __stdcall fake_time_kill(DWORD id)
{
    ev(0x5ea288, id);
    return 0;
}
ImportPatch g_time_set, g_time_kill;

// ---- States of a case -------------------------------------------------------------------------------------------

DWORD ff(Gen &g, float lo, float hi) { return g.fbits_(lo, hi); }
void set_fakes(Gen &g)
{
    memset(&g_template, 0, sizeof g_template);
    make_fake(F_DS, K_DS);
    make_fake(F_BUF, K_BUF);
    make_fake(F_FORMAT, K_READER);
    fl<WORD>(tmpl(F_FORMAT), 0xe) = (WORD)(g.next() % 2 ? 8 : 16);
    fl<WORD>(tmpl(F_FORMAT), 2) = 2;
    dw(tmpl(F_FORMAT), 4) = 22050;
    dw(tmpl(F_FORMAT), 8) = 88200;
    for (int i = 0; i < N_READER; i++) {
        BYTE *r = make_fake(F_READER + i, K_READER);
        (void)r;
        dw(tmpl(F_READER + i), 4) = ptrv(fake(i == 2 && g.next() % 3 == 0 ? F_READER : F_FORMAT));   // the format (+4, 0x5669d0)
        dw(tmpl(F_READER + i), 0xc) = g.below(300000);
    }
}
// A holder and its state in the arena; `waits`: a state whose stop the frame can end (see above)
BYTE *make_holder(Gen &g, bool waits)
{
    BYTE *h = alloc(8);
    if (g.next() % 8 == 0)
        return h;   // no state
    BYTE *s = alloc(0x68);
    dw(h, 0) = ptrv(s);
    fl<BYTE>(h, 4) = (BYTE)(waits ? 0 : g.below(2));
    dw(s, 0) = (DWORD)(-(int)g.below(5000));
    fl<BYTE>(s, 0xc) = (BYTE)(g.next() % 4 == 0);
    fl<BYTE>(s, 0xd) = (BYTE)(g.next() % 3 != 0);
    fl<BYTE>(s, 0xe) = (BYTE)(waits ? 0 : g.next() % 6 == 0);
    dw(s, 0x10) = g.next() % 2 ? 0x7120 : 0;
    dw(s, 0x14) = ptrv(fake(F_READER));   // a stream has a reader whenever it plays (the exe reads it without a test)
    dw(s, 0x18) = g.next() % 3 ? 0 : ptrv(fake(F_READER + 1));
    fl<BYTE>(s, 0x1c) = (BYTE)g.below(2);
    fl<BYTE>(s, 0x1d) = (BYTE)g.below(2);
    dw(s, 0x20) = ff(g, 0.0f, 1.0f);
    dw(s, 0x24) = g.below(300000);
    dw(s, 0x28) = g.next() % 5 ? g.below(200000) : 0;
    dw(s, 0x3c) = ptrv(fake(F_FORMAT));
    dw(s, 0x50) = waits || g.next() % 6 ? ptrv(fake(F_BUF)) : 0;
    dw(s, 0x54) = g.below(RING);
    dw(s, 0x58) = g.next() % 4 == 0;
    fl<BYTE>(s, 0x5c) = (BYTE)(g.next() % 4 == 0);
    dw(s, 0x60) = g.below(RING);
    dw(s, 0x64) = (DWORD)((int)g.below(80000) - 20000);
    if (waits && fl<BYTE>(s, 0xc))
        fl<BYTE>(s, 0xd) = 0;   // a muted stream that plays would make the stop wait forever in both
    return h;
}

enum What { W_CTOR, W_DTOR, W_FRAME, W_QUEUE, W_TAKE, W_PLAY, W_STOP, W_VOLUME, W_CALLBACK, NWHAT };
const DWORD ADDR[] = {0x587060, 0x5870d0, 0x587160, 0x5871f0, 0x5872b0, 0x587310, 0x587600, 0x587640, 0x587700};
const char *const WHAT[] = {"CThreadedStream::Ctor", "CThreadedStream::Dtor",       "CThreadedStream::Frame",
                            "CThreadedStream::Queue", "CThreadedStream::TakeQueued", "CThreadedStream::Play",
                            "CThreadedStream::Stop",  "CThreadedStream::SetVolume",  "CThreadedStream::TimerCallback"};
const char *const PATHS[] = {"Music/a.ogg", "Voice/b.wav", ""};

void run_case(Tally &t, What w, unsigned seed)
{
    Result r{};
    SavedBytes saved_ds;
    save_bytes(saved_ds, DSOUND, 4);
    for (int side = 0; side < 2; side++) {
        Gen g{seed * 31u + 7u};
        set_fakes(g);
        side_begin(side, seed, MODULE, (BYTE)(0x5a + seed % 3));
        memset(g_ring, 0x11, sizeof g_ring);
        *at<DWORD *>(DSOUND) = g.next() % 6 ? ptrv(fake(F_DS)) : 0;
        DWORD a[5] = {0, 0, 0, 0, 0}, ecx = 0;
        int nargs = 0;
        const bool waits = w == W_DTOR || w == W_FRAME || w == W_TAKE || w == W_PLAY || w == W_STOP;
        BYTE *h = make_holder(g, waits);
        ecx = ptrv(h);
        switch (w) {
        case W_CTOR:
            memset(h, 0x22, 8);
            a[0] = g.next();
            nargs = 1;
            break;
        case W_QUEUE:
            a[0] = ptrv(PATHS[g.below(3)]);
            a[1] = ff(g, 0.0f, 1.0f);
            a[2] = g.next();
            nargs = 3;
            break;
        case W_PLAY:
            a[0] = ptrv(PATHS[g.below(3)]);
            a[1] = g.next() % 3 ? 0 : g.below(5000);
            nargs = 2;
            break;
        case W_VOLUME:
            a[0] = (DWORD)((int)g.below(6000) - 5000);
            if (dw(h, 0) && g.next() % 4 == 0)
                a[0] = dw(at<void *>(dw(h, 0)), 0);
            nargs = 1;
            break;
        case W_CALLBACK:
            ecx = 0;
            a[0] = g.next();
            a[1] = g.next();
            a[2] = dw(h, 0);
            nargs = 5;
            break;
        default: break;
        }
        const DWORD fn = side == 0 && w == W_CALLBACK ? ptrv(original_address(ADDR[w])) : ADDR[w];
        r.eax[side] = call_fn(fn, ecx, 0, nargs, a, false, nullptr);
        memcpy(alloc(sizeof g_ring), g_ring, sizeof g_ring);
        BYTE *ds = alloc(4);
        dw(ds, 0) = *at<DWORD *>(DSOUND);
        side_end();
        restore_bytes(saved_ds);
    }
    DWORD mask = 0;
    if (w == W_QUEUE || w == W_TAKE || w == W_PLAY)
        mask = 0xff;
    if (w == W_CTOR)
        mask = 0xffffffff;
    compare(t, WHAT[w], seed, r, mask);
}

void test_all(Tally &t)
{
    const int COUNTS[NWHAT] = {60, 300, 300, 200, 200, 600, 200, 300, 2000};
    for (int w = 0; w < NWHAT; w++)
        for (int k = 0; k < scaled(COUNTS[w]); k++)
            run_case(t, (What)w, 0x6000u * (unsigned)(w + 1) + (unsigned)k);
}

// Real data: the live streams' states (the world's music, the application's music, the voice player) as
// copies through the callback, their buffers and readers replaced by fakes
BYTE g_copies[3][0x68];
int collect_real()
{
    BYTE *holders[3] = {nullptr, nullptr, nullptr};
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (world && readable(world + 0x334, 4)) {
        BYTE *m = *reinterpret_cast<BYTE **>(world + 0x334);
        if (m && readable(m, 0x34))
            holders[0] = *reinterpret_cast<BYTE **>(m + 0x30);
    }
    BYTE *app = *at<BYTE **>(0x713f14);
    if (app && readable(app + 0x274, 4))
        holders[1] = *reinterpret_cast<BYTE **>(app + 0x274);
    if (*at<BYTE *>(0x713c34) & 1)
        holders[2] = *at<BYTE **>(0x713c28);
    int n = 0;
    for (BYTE *h : holders) {
        if (!h || !readable(h, 8))
            continue;
        BYTE *s = *reinterpret_cast<BYTE **>(h);
        if (!s || !readable(s, 0x68))
            continue;
        memcpy(g_copies[n++], s, 0x68);
    }
    return n;
}
void test_real(Tally &t, int n)
{
    for (int i = 0; i < n; i++)
        for (unsigned k = 0; k < 8; k++) {
            Result r{};
            const unsigned seed = 0x96000u + 16u * (unsigned)i + k;
            for (int side = 0; side < 2; side++) {
                Gen g{seed};
                set_fakes(g);
                side_begin(side, seed, MODULE, 0x5c);
                BYTE *s = alloc(0x68);
                memcpy(s, g_copies[i], 0x68);
                fl<BYTE>(s, 0xe) = 0;   // paused for the checks; the copy's callback is not running
                if (dw(s, 0x14))
                    dw(s, 0x14) = ptrv(fake(F_READER));
                if (dw(s, 0x18))
                    dw(s, 0x18) = ptrv(fake(F_READER + 1));
                if (dw(s, 0x50))
                    dw(s, 0x50) = ptrv(fake(F_BUF));
                dw(s, 0x3c) = ptrv(fake(F_FORMAT));
                const DWORD a[5] = {0x7130, 0, ptrv(s), 0, 0};
                const DWORD fn = side == 0 ? ptrv(original_address(0x587700)) : 0x587700;
                r.eax[side] = call_fn(fn, 0, 0, 5, a, false, nullptr);
                memcpy(alloc(sizeof g_ring), g_ring, sizeof g_ring);
                side_end();
            }
            compare(t, "CThreadedStream::TimerCallback (real)", seed, r, 0);
        }
}

}  // namespace

ZAX_CHECKS(sound_stream)
{
    (void)cls;
    (void)n;
    Tally all{"sound_stream: the threaded streams in the fake world"},
        real{"sound_stream: the live streams' states as copies through the timer callback (real data)"};
    const int found = collect_real();
    set_kind(K_DS, DS_SLOTS, sizeof DS_SLOTS / sizeof DS_SLOTS[0]);
    set_kind(K_BUF, BUF_SLOTS, sizeof BUF_SLOTS / sizeof BUF_SLOTS[0]);
    set_kind(K_READER, READER_SLOTS, sizeof READER_SLOTS / sizeof READER_SLOTS[0]);
    const int saved_fail = g_fail_one_in;
    if (world_on(STANDINS, 1)) {
        patch_import(g_time_set, 0x5ea290, (void *)&fake_time_set);
        patch_import(g_time_kill, 0x5ea288, (void *)&fake_time_kill);
        test_all(all);
        test_real(real, found);
        unpatch_import(g_time_kill);
        unpatch_import(g_time_set);
        world_off();
    } else {
        all.check(false, "sound_stream", "[the fake world could not be set up]");
    }
    g_fail_one_in = saved_fail;
    log("  sound_stream real data: %d live stream states", found);
    all.report();
    real.report();
}

}  // namespace zax
