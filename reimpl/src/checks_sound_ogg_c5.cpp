// Checks of the Ogg Vorbis reader's slots (sound_ogg_c5.cpp) against the original: the reader object is a copy per
// side (same offsets of its format block); vorbisfile's functions (ov_read, ov_time_tell, ov_time_seek,
// ov_open_callbacks, ov_info, ov_pcm_total, ov_clear) and the file buffer's size are stand-ins (patched jumps;
// another thread gets the source-built vorbisfile) that log their arguments and answer from the case: ov_read from a script of results
// (counts, 0, OV_HOLE, OV_EBADLINK, other errors) and writes its bitstream; the reader's slot 5 (Close) is a
// logging fake; the game's sound streams are paused meanwhile (checks_libs_common.h). Compared: eax (Open: al and whether the upper bytes are the format's address), the log, the
// object (the format pointer by its offset in the side's block), the format's bytes and 0x6d0160.
#include "check_util.h"
#include "checks_ui_common.h"
#include "checks_libs_common.h"

struct OvCallbacks {
    unsigned long read, seek, close, tell;
};
// vorbisfile built from source (lib_vorbis.cpp, fastcall like the exe's copy): what another thread gets while the
// stand-ins are in (the game's sound streams are paused, but a stream may be opened or closed meanwhile)
extern "C" long __fastcall ov_read(void *vf, char *buffer, int length, int bigendianp, int word, int sgned, int *bitstream);
extern "C" double __fastcall ov_time_tell(void *vf);
extern "C" int __fastcall ov_time_seek(void *vf, double pos);
extern "C" int __fastcall ov_open_callbacks(void *datasource, void *vf, char *initial, long ibytes, OvCallbacks callbacks);
extern "C" void *__fastcall ov_info(void *vf, int link);
extern "C" long long __fastcall ov_pcm_total(void *vf, int i);
extern "C" int __fastcall ov_clear(void *vf);

namespace zax {

int hooks_sound_ogg_c5(Hook *out);

namespace {

using uic::ModuleHooks;

constinit ModuleHooks g_hooks{"sound_ogg_c5", &hooks_sound_ogg_c5};

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

struct Side {
    alignas(16) BYTE obj[0x40];
    alignas(16) BYTE pool[0x40];
    BYTE buffer[0x200];
    unsigned log;
    int calls;
};
Side g_side[2];
int g_cur;
DWORD g_check_thread;
inline bool other_thread() { return GetCurrentThreadId() != g_check_thread; }
unsigned g_case_seed;
int g_script[12], g_nscript, g_next;
DWORD g_info[4];
long long g_total;
DWORD g_open_result, g_size, g_seek_result;
double g_time;
DWORD g_vt[8];

void log_it(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_side[g_cur].log = fnv(g_side[g_cur].log, e, sizeof e);
    g_side[g_cur].calls++;
}
DWORD off(const void *p) { return ptrv(p) - ptrv(&g_side[g_cur]); }

int __fastcall fake_read(void *vf, BYTE *buf, int len, int be, int word, int sgned, int *bs)
{
    if (other_thread())
        return (int)ov_read(vf, reinterpret_cast<char *>(buf), len, be, word, sgned, bs);
    log_it(0x1001, off(vf), off(buf), (DWORD)len);
    log_it(0x1002, (DWORD)be, (DWORD)word, (DWORD)sgned);
    *bs = 7;
    const int r = g_next < g_nscript ? g_script[g_next++] : 0;
    return r;
}
long double __fastcall fake_tell(void *vf)
{
    if (other_thread())
        return ov_time_tell(vf);
    log_it(0x1003, off(vf));
    return g_time;
}
int __fastcall fake_seek(void *vf, DWORD, double t)
{
    if (other_thread())
        return ov_time_seek(vf, t);
    DWORD b[2];
    memcpy(b, &t, 8);
    log_it(0x1004, off(vf), b[0], b[1]);
    return (int)g_seek_result;
}
using Callbacks = OvCallbacks;
DWORD __fastcall fake_open(void *ds, void *vf, DWORD initial, DWORD ibytes, Callbacks cb)
{
    if (other_thread())
        return (DWORD)ov_open_callbacks(ds, vf, reinterpret_cast<char *>(initial), (long)ibytes, cb);
    log_it(0x1005, off(ds), off(vf), initial ^ ibytes << 8);
    log_it(0x1006, cb.read ^ cb.seek, cb.close, cb.tell);
    return g_open_result;
}
BYTE *__fastcall fake_info(void *vf, int link)
{
    if (other_thread())
        return static_cast<BYTE *>(ov_info(vf, link));
    log_it(0x1007, off(vf), (DWORD)link);
    return reinterpret_cast<BYTE *>(g_info);
}
long long __fastcall fake_total(void *vf, int link)
{
    if (other_thread())
        return ov_pcm_total(vf, link);
    log_it(0x1008, off(vf), (DWORD)link);
    return g_total;
}
int __fastcall fake_clear(void *vf)
{
    if (other_thread())
        return ov_clear(vf);
    log_it(0x1009, off(vf));
    return 0;
}
DWORD THISCALL fake_size(void *f)
{
    if (other_thread()) {   // FileBuf_Size 0x4a2190: the source's slot 7, 0 without one
        void *src = *reinterpret_cast<void **>(static_cast<BYTE *>(f) + 4);
        return src ? vcall<DWORD>(src, 7) : 0;
    }
    log_it(0x100a, ptrv(f));
    return g_size;
}
DWORD THISCALL fake_close(void *self)
{
    log_it(0x100c, off(self));
    return 0;
}

struct Patches {
    CodePatch p[8];
    void on()
    {
        const DWORD at_[8] = {0x5c4e80, 0x5c4d90, 0x5c4ca0, 0x5c3ce0, 0x5c4e40, 0x5c4590, 0x5c3bf0, 0x4a2190};
        void *to[8] = {(void *)&fake_read, (void *)&fake_tell, (void *)&fake_seek, (void *)&fake_open, (void *)&fake_info,
                       (void *)&fake_total, (void *)&fake_clear, (void *)&fake_size};
        for (int i = 0; i < 8; i++)
            patch_jump(p[i], at_[i], to[i]);
    }
    void off()
    {
        for (int i = 7; i >= 0; i--)
            unpatch(p[i]);
    }
};

unsigned rnd_state;
DWORD rnd()
{
    rnd_state = rnd_state * 1664525u + 1013904223u;
    return rnd_state ^ (rnd_state >> 13);
}

// One case of a slot: both sides from the same object, compared
void run_case(Tally &t, int slot, const char *name)
{
    rnd_state = g_case_seed * 2654435761u + (unsigned)slot;
    BYTE obj[0x40];
    for (BYTE &b : obj)
        b = (BYTE)rnd();
    g_vt[5] = ptrv((void *)&fake_close);
    *reinterpret_cast<DWORD *>(obj) = ptrv(g_vt);
    obj[0x1c] = (BYTE)(rnd() & 1);
    obj[0x1d] = (BYTE)(rnd() % 4 == 0);
    const DWORD rates[5] = {22050, 44100, 11025, 0x7fffffff, 1};
    DWORD fmt_rate = rates[rnd() % 5];
    g_nscript = (int)(rnd() % 12);
    for (int i = 0; i < g_nscript; i++) {
        const int pick = (int)(rnd() % 8);
        g_script[i] = pick == 0 ? -137 : pick == 1 ? -3 : pick == 2 ? 0 : pick == 3 ? -1 - (int)(rnd() % 5) : (int)(rnd() % 300);
    }
    g_info[0] = 0, g_info[1] = 1 + rnd() % 2, g_info[2] = rates[rnd() % 3], g_info[3] = rnd();
    g_total = (long long)(rnd() % 1000000) | (long long)(rnd() % 4) << 32;
    g_open_result = rnd() % 3 == 0 ? (DWORD)-1 - rnd() % 5 : 0;
    g_size = rnd();
    g_seek_result = rnd();
    const double times[4] = {0.0, 1.5, 123.456789, 1e9};
    g_time = times[rnd() % 4];
    const DWORD arg = rnd() % 4 == 0 ? rnd() : rnd() % 0x400;
    const DWORD file = rnd() % 4 == 0 ? 0 : 0x12345678;
    const DWORD saved_max = *at<DWORD *>(0x6d0160);
    const DWORD start_max = rnd() % 2 ? 0 : rnd() % 200;
    DWORD eax[2], maxv[2];
    Patches p;
    p.on();
    for (int s = 0; s < 2; s++) {
        g_cur = s;
        Side &sd = g_side[s];
        memcpy(sd.obj, obj, sizeof obj);
        *reinterpret_cast<DWORD *>(sd.obj + 4) = ptrv(sd.pool);   // the format of an open reader
        *reinterpret_cast<DWORD *>(sd.obj + 0x18) = ptrv(sd.obj + 0x20);   // (the OggVorbis_File: an address of the side)
        memset(sd.pool, 0x5a, sizeof sd.pool);
        *reinterpret_cast<DWORD *>(sd.pool + 8) = fmt_rate;
        memset(sd.buffer, 0, sizeof sd.buffer);
        sd.log = 2166136261u;
        sd.calls = 0;
        g_next = 0;
        *at<DWORD *>(0x6d0160) = start_max;
        if (s == 0)
            g_hooks.out();
        switch (slot) {
        case 2: eax[s] = (DWORD)at<int(THISCALL *)(BYTE *, BYTE *, int)>(0x5259b0)(sd.obj, sd.buffer, (int)(arg % 0x200)); break;
        case 3: eax[s] = (DWORD)at<int(THISCALL *)(BYTE *)>(0x525a30)(sd.obj); break;
        case 4: eax[s] = (DWORD)at<int(THISCALL *)(BYTE *, DWORD)>(0x525a60)(sd.obj, arg); break;
        case 0: {
            const DWORD r = at<DWORD(THISCALL *)(BYTE *, DWORD)>(0x525aa0)(sd.obj, file);
            const DWORD fmt = *reinterpret_cast<DWORD *>(sd.obj + 4);
            eax[s] = (r & 0xff) | ((r & 0xffffff00u) == (fmt & 0xffffff00u) ? 0x100 : 0) |
                     ((r & 0xffffff00u) == ((g_open_result) & 0xffffff00u) ? 0x400 : 0);
            if (fmt != ptrv(sd.pool)) {   // a new format block of the pool: its bytes compared, then freed
                memcpy(sd.pool, at<void *>(fmt), 0x12);
                pool_free(at<void *>(fmt), 0x12);
                *reinterpret_cast<DWORD *>(sd.obj + 4) = 0xf0f0f0f0u;
            }
            break;
        }
        default: at<void(THISCALL *)(BYTE *)>(0x525ba0)(sd.obj); eax[s] = 0; break;
        }
        if (s == 0)
            g_hooks.in();
        maxv[s] = *at<DWORD *>(0x6d0160);
        // the object: the format pointer and the vf pointer by their offsets
        static const DWORD PTRS[2] = {4, 0x18};
        for (DWORD o : PTRS) {
            DWORD &v = *reinterpret_cast<DWORD *>(sd.obj + o);
            if (v >= ptrv(&sd) && v < ptrv(&sd) + sizeof sd)
                v = 0xa0000000u | (v - ptrv(&sd));
        }
    }
    p.off();
    *at<DWORD *>(0x6d0160) = saved_max;
    // 0x6d0160 only for Read, and only when both sides' values come from the case (the start or a scripted result):
    // the game's own readers in another thread may raise it meanwhile (runs 20261028-152314-7bfa: Close, 0x2d /
    // 0x1000; 20261028-153101-109d: Read, 0x1000 / 0x61)
    auto ours = [&](DWORD v) {
        bool k = v == start_max;
        for (int i = 0; i < g_nscript; i++)
            k = k || v == (DWORD)g_script[i];
        return k;
    };
    const bool max_same = slot != 2 || !ours(maxv[0]) || !ours(maxv[1]) || maxv[0] == maxv[1];
    const bool same = eax[0] == eax[1] && max_same && g_side[0].log == g_side[1].log && g_side[0].calls == g_side[1].calls &&
                      memcmp(g_side[0].obj, g_side[1].obj, sizeof g_side[0].obj) == 0 &&
                      memcmp(g_side[0].pool, g_side[1].pool, sizeof g_side[0].pool) == 0;
    char d[200];
    wsprintfA(d, "[%s seed %u: eax %x/%x, max %x/%x, calls %d/%d]", name, g_case_seed, eax[0], eax[1], maxv[0], maxv[1], g_side[0].calls, g_side[1].calls);
    t.check(same, "", d);
}

}  // namespace

ZAX_CHECKS(sound_ogg_c5)
{
    (void)cls;
    (void)n;
    Tally t{"sound ogg c5: the Ogg Vorbis reader's slots"};
    g_check_thread = GetCurrentThreadId();
    libchk::PausedStreams streams;   // (the music thread decodes through vorbisfile, which the stand-ins replace)
    streams.pause();
    const int slots[5] = {2, 3, 4, 0, 5};
    const char *const names[5] = {"Read", "Tell", "Seek", "Open", "Close"};
    for (int i = 0; i < 5; i++)
        for (int k = 0; k < scaled(600); k++) {
            g_case_seed = 0x5259b0u + (unsigned)(i * 10007 + k);
            run_case(t, slots[i], names[i]);
        }
    streams.resume();
    t.report();
}

}  // namespace zax
