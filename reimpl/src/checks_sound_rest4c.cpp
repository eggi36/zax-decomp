// Checks of the last small sound pieces (sound_rest4c.cpp) against the original with the toolkit's harness
// (checks_ui_common.h): the stream holder and its state are fakes in the extra area (null states too), the
// reader's Seconds, the stream's Stop and the file's Tell are recording stand-ins.
#include "checks_ui_common.h"

namespace zax {

int hooks_sound_rest4c(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"sound_rest4c", &hooks_sound_rest4c};

const Stub STUBS[] = {
    {0x4a2180, CV_THIS, 0, K_VALUE, "FileBuf_Tell"},
    {0x57d0e0, CV_THIS, 0, K_FLOAT, "CSoundReader::Seconds"},
    {0x587600, CV_THIS, 0, K_VALUE, "CThreadedStream::Stop"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

// A state of 0x68 random bytes with the flags +0xc/+0xd/+0xe each 0 or 1 (sometimes another value), or none
BYTE *make_state(Gen &g)
{
    if (g.chance(5))
        return nullptr;
    BYTE *s = xalloc(0x68);
    for (DWORD i = 0; i < 0x68; i += 4)
        at_(s, i) = g.next();
    for (DWORD f = 0xc; f <= 0xe; f++)
        s[f] = g.chance(6) ? (BYTE)g.next() : (BYTE)g.below(2);
    return s;
}

void base(Gen &) {}

// The holder (and the voice-over player: the same shape, its +0 the holder)
void s_holder(Gen &g)
{
    BYTE *h = xalloc(8);
    at_(h, 0) = ptrv(make_state(g));
    at_(h, 4) = g.below(2);
    g_self = h;
    g_args[0] = g.chance(4) ? 0x3f800000u : g.next();
}
void s_player(Gen &g)
{
    s_holder(g);
    BYTE *p = xalloc(8);
    at_(p, 0) = g.chance(6) ? 0 : ptrv(g_self);   // (the exe never calls these without a holder)
    if (!at_(p, 0))
        at_(p, 0) = ptrv(xalloc(8));
    g_self = p;
}
void s_source(Gen &g)
{
    BYTE *src = xalloc(0x10);
    for (DWORD i = 0; i < 0x10; i += 4)
        at_(src, i) = g.next();
    g_self = src;
}

const Test TESTS[] = {
    {"OggCallback Tell", 0x57d310, CALL_THIS, 0, ~0u, 50, s_source, STUBS, NSTUBS},
    {"Stream SetCrossFadeGain", 0x587290, CALL_THIS, 1, ~0u, 200, s_holder, STUBS, NSTUBS},
    {"Stream Volume", 0x5876c0, CALL_THIS, 0, ~0u, 100, s_holder, STUBS, NSTUBS},
    {"Stream IsMuted", 0x5876d0, CALL_THIS, 0, ~0u, 100, s_holder, STUBS, NSTUBS},
    {"Stream IsPlaying", 0x5876e0, CALL_THIS, 0, ~0u, 100, s_holder, STUBS, NSTUBS},
    {"Voice Stop", 0x595af0, CALL_THIS, 0, ~0u, 50, s_player, STUBS, NSTUBS},
    {"Voice IsPlaying", 0x595bd0, CALL_THIS, 0, ~0u, 100, s_player, STUBS, NSTUBS},
};

// Stream_Seconds returns its value in st0: called through a long double pointer; its 80 bits logged
void seconds_build(Gen &g)
{
    g_self = nullptr;
    memset(g_args, 0, sizeof g_args);
    s_holder(g);
}
DWORD seconds_call()
{
    const long double v = at<long double(THISCALL *)(void *)>(0x5875e0)(g_self);
    DWORD w[3] = {};
    memcpy(w, &v, 10);
    log_event(0x5875, w[0], w[1] ^ (w[2] << 16));
    return w[1];
}

}  // namespace

ZAX_CHECKS(sound_rest4c)
{
    (void)cls;
    (void)n;
    Tally t{"sound rest 4c: the Ogg tell callback, the stream holder's getters and gain, the voice-over's stop"};
    run_tests(t, g_hooks, nullptr, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 41000);
    if (ui_selected("Stream Seconds")) {
        const Check c{&g_hooks, nullptr};
        stubs_on(STUBS, NSTUBS);
        const int count = scaled(200);
        for (int k = 0; k < count; k++)
            run_case(t, c, "Stream Seconds", 42000u + (unsigned)k, &seconds_build, &seconds_call);
        stubs_off();
    }
    t.report();
}

}  // namespace zax
