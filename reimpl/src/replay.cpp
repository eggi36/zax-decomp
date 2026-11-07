// Input recording and replay for the comparison runs of phase 6 (docs/DECOMPILATION.md 13, "Comparison runs: input
// recording and replay"). Active in both builds and with /Original: like the logged start (api_trace.cpp) it lives in
// the exe's import table, installed by DllMain while the main thread is still suspended, so it sees every answer that
// enters the game from outside from the C runtime's first static initializer on.
//
// [Reimpl] Record=1: every answer the main thread gets from the clocks (timeGetTime, GetTickCount,
// QueryPerformanceCounter/Frequency, GetSystemTimeAsFileTime, GetLocalTime, GetSystemTime, GetTimeZoneInformation),
// the message queue (PeekMessageA: the message, the window as "the game's window" / "another" / none, a thread
// timer's procedure as its offset in its module), IsIconic, the joystick (joyGetNumDevs, joyGetDevCapsA, joyGetPosEx)
// and GlobalMemoryStatus is recorded in call order, as "Rec <hex>" lines of Zax-Reimpl.log (the VM helper fetches
// the log of every job; zax-testtools/tools/replay_compare.py --extract makes the replay file of them). The random generators
// 0x7124c0 and 0x712cc0 are seeded from timeGetTime by their static initializers and 0x6d06c0 with a constant, so the
// clocks are their seeds too. A frame mark (below) goes in after every frame, and the lines are flushed there (the VM
// helper kills the game).
// [Reimpl] Replay=1: Zax-Replay.rec next to Zax.exe is read at the start and the main thread's calls of these
// functions get the recorded answers in the recorded order instead of Windows'. PeekMessageA first empties the real
// queue (sent messages reach the window procedure inside it as usual; posted and input messages are dropped, WM_PAINT
// validated) and hands out the recorded message (another window's goes nowhere); for a recorded thread timer (the
// launcher's timer that starts the map) it waits for the real one of the same procedure and calls the procedure
// itself. When the game asks for something else than the recording holds next, or for a different frame, the replay
// logs "Replay: live after DIVERGED: …" with the last calls' call sites and goes live; at the end of the recording it
// logs "Replay: live after end of the recording …" and goes live.
// With either (or [Reimpl] FrameLog=1 alone) every frame logs a line "Frame <n>: …" at IDirectDrawSurface::Flip of
// the primary surface (the vtable slot patched at the first message pump after the surfaces exist): a hash of the
// back buffer about to be shown, a hash of the world (every part in the grids of all layers: class, flags, team, id,
// position, model, sequence, damage, the animated entities' frame and angles, the AIs' classes), the two game
// generators' states and draw counts, the player's position and damage, the playing time, the fixed heap's use.
// zax-testtools/tools/replay_compare.py compares two such logs and names the first differing frame. [Reimpl] ScreenGrid=1 adds the
// screen as 8 x 8 cell hashes.
// Save games: every file written below SaveGames is logged byte for byte when closed and deleted (KeepSaves=1 keeps
// it). The C runtime's heap is served at a fixed address with Record or Replay (FixedHeap=0 turns it off; see "A heap
// at a fixed address"; HeapStartApart=0 puts the C runtime's start-up blocks back into the main thread's part), since
// the save games hold object addresses; HeapLog=<n> traces the first n heap calls, PoolLog=<n> with PoolLogSize=<bytes>
// (and PoolLogFrom=<frame>) the game pool's calls of one size class, PoolStats=<bytes> that class's use at every frame.
// Since batch 6-12 a recording also holds what the main thread saw of the sounds' state, which their timer thread and
// the sound card change in real time (Stream_IsPlaying, CThreadedStream::Frame/Stop/Play, CSoundResource::IsPlaying;
// "The streamed sounds' state" below; the recording's version event E_VERSION, older recordings replay as before).
// Since batch 6-13 (version 6, "Other real-time answers" below) also the sound card's play position where the main
// thread refills a stream itself (Use Threaded Stream=0), Bink's clock and wait (the movies) and the DOS times of the
// files the game lists (their write times).
// Since batch 6-14 (version 7, "The network" below) also what DirectPlay 8 and Winsock bring into a LAN game's side: the
// message handler's callbacks on DirectPlay's threads (with their contents, placed at the main thread's next entry of
// the lock they took), the synchronous DirectPlay calls' results and outputs (fake objects answer them on replay, so a
// side replays alone, without the network) and the GameSpy SDK's Winsock calls; what the side sends is logged as "Net"
// lines (zax-testtools/tools/replay_compare.py --net).
// Since batch 6-16a (version 8, "Focus, surface loss and the clipboard" below) also the window's messages that come
// with a focus change and Windows' answers to its default handling, DirectDraw's surface loss (IsLost, Restore, failed
// Lock and Flip), the clipboard's data (GetClipboardData), and a DirectPlay enumeration's answers with their session
// description (given back with fake address objects).
//
// Other threads (sound, music streams, DirectPlay) always get the real answers. Reads only: the hashes read game
// memory and call no exe code.
#include "reimpl.h"
#include "pool.h"

WINIMPORT BOOL WINAPI ReadFile(HANDLE f, void *buf, DWORD n, DWORD *read, void *overlapped);
WINIMPORT DWORD WINAPI GetFileSize(HANDLE f, DWORD *high);
WINIMPORT BOOL WINAPI ValidateRect(void *hwnd, const void *rect);
WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT BOOL WINAPI DeleteFileA(const char *name);
WINIMPORT BOOL WINAPI VirtualFree(void *addr, size_t size, DWORD type);

namespace zax {
const char *hook_near(DWORD address, DWORD *start);   // main.cpp
}

namespace zax::replay {

namespace {

inline DWORD rd(DWORD a) { return *at<const DWORD *>(a); }
inline DWORD rd(const BYTE *p, DWORD off) { return *reinterpret_cast<const DWORD *>(p + off); }
// A COM object's method by its vtable slot
template <typename F> inline F com_slot(DWORD obj, int slot) { return reinterpret_cast<F>(static_cast<uintptr_t>(rd(rd(obj) + slot * 4))); }

enum Kind : BYTE {
    E_PEEK = 1, E_TIME, E_TICK, E_QPC, E_QPF, E_SYSFT, E_LOCAL, E_SYSTIME, E_TZ, E_JOYNUM, E_JOYCAPS, E_JOYPOS, E_MEMSTAT,
    E_ICONIC, E_STREAM, E_FRAME = 0x40,
};
// Version 7 (batch 6-14): the network ("The network" below)
constexpr BYTE E_DP_CB = E_STREAM + 9, E_DP_LOCK = E_STREAM + 10, E_DP_CALL = E_STREAM + 11, E_COCREATE = E_STREAM + 12,
               E_WS = E_STREAM + 13;
// Version 8 (batch 6-16a): the window procedure's sent messages, DirectDraw's surface loss, the clipboard ("Focus, surface
// loss and the clipboard" below); an enumeration's answer (E_DP_CB 0xffff000b) carries its session description
constexpr BYTE E_SENT = E_STREAM + 14, E_DDRAW = E_STREAM + 15, E_CLIP = E_STREAM + 16, E_DEFPROC = E_STREAM + 17;
// Version 6 (batch 6-13; E_STREAM + 1 … + 4 are the streams' events below)
constexpr BYTE E_DS_POSITION = E_STREAM + 5, E_BINK_TIMER = E_STREAM + 6, E_BINK_WAIT = E_STREAM + 7, E_DOS_TIME = E_STREAM + 8;
// The first event of a recording made since batch 6-12: its answer is the recording's version (5: it holds what the main
// thread saw of the sounds' state, E_STREAM … E_SOUND_PLAYING, below; 2 to 4 came from this batch's steps on the way;
// 6 since batch 6-13: E_DS_POSITION … E_DOS_TIME; 7 since batch 6-14: E_DP_CB … E_WS; 8 since batch 6-16a: E_SENT …
// E_CLIP and the enumeration's answers). A recording without it (version 1) or of an older version is replayed as before:
// those calls stay live.
constexpr BYTE E_VERSION = 0x42;
constexpr DWORD VERSION = 8;
const char *kind_name(BYTE k)
{
    static const char *const N[] = {"?", "PeekMessageA", "timeGetTime", "GetTickCount", "QueryPerformanceCounter",
                                    "QueryPerformanceFrequency", "GetSystemTimeAsFileTime", "GetLocalTime", "GetSystemTime",
                                    "GetTimeZoneInformation", "joyGetNumDevs", "joyGetDevCapsA", "joyGetPosEx",
                                    "GlobalMemoryStatus", "IsIconic", "Stream_IsPlaying", "CThreadedStream::Frame", "CThreadedStream::Stop", "CThreadedStream::Play", "CSoundResource::IsPlaying",
                                    "IDirectSoundBuffer::GetCurrentPosition", "RADTimerRead", "BinkWait", "FileTimeToDosDateTime",
                                    "DirectPlay callback", "DirectPlay lock", "DirectPlay call", "CoCreateInstance", "Winsock",
                                    "a sent message", "IDirectDrawSurface", "GetClipboardData", "DefWindowProcA"};
    if (k == E_FRAME)
        return "frame";
    return k < sizeof N / sizeof N[0] ? N[k] : "?";
}

enum Mode { LIVE, RECORD, REPLAY };
Mode g_mode = LIVE;
bool g_frame_log;
bool g_stream_events;   // Stream_IsPlaying recorded / replayed (a version 5 recording, E_VERSION)
bool g_v6_events;       // the other real-time answers of batch 6-13 (a version 6 recording)
bool g_v7_events;       // the network of batch 6-14 (a version 7 recording)
bool g_v8_events;       // sent messages, surface loss, the clipboard, the enumeration's answers (batch 6-16a, version 8)
DWORD g_main_thread;
char g_dir[MAX_PATH + 32];

// The recording: events {kind, length (word), answer (dword), length bytes}
constexpr DWORD OUT_SIZE = 0x10000;
BYTE *g_out;
DWORD g_out_used;
BYTE *g_in;
DWORD g_in_size, g_in_pos;
DWORD g_events;   // events written or replayed
DWORD g_frame;    // frames (flips of the primary surface)
DWORD g_heap_peak;   // the fixed heap's use at most (below)
DWORD g_lost_blocks;
const char MAGIC[8] = {'Z', 'A', 'X', 'R', 'E', 'C', '1', 0};

inline bool main_thread() { return g_mode != LIVE && GetCurrentThreadId() == g_main_thread; }

// Bytes as "<prefix> <hex>" lines of the log, at most 400 bytes a line
void hex_lines(const char *prefix, const BYTE *p, DWORD n)
{
    static const char HEX[] = "0123456789abcdef";
    char line[900];
    for (DWORD i = 0; i < n;) {
        int o = wsprintfA(line, "%s ", prefix);
        for (DWORD k = 0; k < 400 && i < n; k++, i++) {
            line[o++] = HEX[p[i] >> 4];
            line[o++] = HEX[p[i] & 15];
        }
        line[o] = 0;
        log("%s", line);
    }
}

// The recording goes into Zax-Reimpl.log as "Rec <hex>" lines (the VM helper fetches the log of every job;
// zax-testtools/tools/replay_compare.py --extract makes the file Replay= reads from them)
void flush_out()
{
    if (g_mode == RECORD && g_out_used)
        hex_lines("Rec", g_out, g_out_used);
    g_out_used = 0;
}

// A cycle of two events repeated (the menu's and the loading screen's waits poll PeekMessageA and GetTickCount in
// turns, hundreds of thousands of times) is written once as E_REPEAT2 with the number of events that repeat it
constexpr BYTE E_REPEAT2 = 0x41;
// An event's data at most (its length is a word; a DirectPlay receive or a call's output can be longer than the 0x400
// bytes of version 6, whose events all fit below that)
constexpr DWORD MAX_DATA = 0xfff8, MAX_EVENT = 7 + MAX_DATA;
BYTE g_last[2][MAX_EVENT];   // the last two events written ([0] the older)
DWORD g_last_len[2];
DWORD g_repeat;              // events since then that repeat the cycle, not written yet

void write_event(const BYTE *e, DWORD n)
{
    if (g_out_used + n > OUT_SIZE)
        flush_out();
    memcpy(g_out + g_out_used, e, n);
    g_out_used += n;
}

void flush_repeat()
{
    if (!g_repeat)
        return;
    BYTE e[7] = {E_REPEAT2, 0, 0};
    memcpy(e + 3, &g_repeat, 4);
    write_event(e, 7);
    if (g_repeat & 1) {   // the cycle ended on the older one: the last two swap
        static BYTE t[MAX_EVENT];   // the main thread's only
        const DWORD tl = g_last_len[0];
        memcpy(t, g_last[0], tl);
        memcpy(g_last[0], g_last[1], g_last_len[1]);
        g_last_len[0] = g_last_len[1];
        memcpy(g_last[1], t, tl);
        g_last_len[1] = tl;
    }
    g_repeat = 0;
}

void check_surfaces();   // the surfaces' patched vtables written back (rehook_surfaces, below)
void put(BYTE kind, DWORD ret, const void *data, DWORD len)
{
    check_surfaces();
    BYTE e[7];
    if (len > MAX_DATA)
        len = MAX_DATA;
    e[0] = kind;
    e[1] = (BYTE)len;
    e[2] = (BYTE)(len >> 8);
    memcpy(e + 3, &ret, 4);
    const DWORD n = 7 + len;
    g_events++;
    const int expect = g_repeat & 1;   // the cycle's next event: the older first
    if (g_last_len[0] && g_last_len[expect] == n && !memcmp(g_last[expect], e, 7) &&
        (!len || !memcmp(g_last[expect] + 7, data, len))) {
        g_repeat++;
        return;
    }
    flush_repeat();
    write_event(e, 7);
    if (len)
        write_event(static_cast<const BYTE *>(data), len);
    memcpy(g_last[0], g_last[1], g_last_len[1]);
    g_last_len[0] = g_last_len[1];
    memcpy(g_last[1], e, 7);
    if (len)
        memcpy(g_last[1] + 7, data, len);
    g_last_len[1] = n;
}

extern DWORD g_stream_waits, g_stream_wait_ms;
void go_live(const char *why)
{
    if (g_stream_waits)
        log("Replay: %u waits for a stream's end (%u ms in all)", g_stream_waits, g_stream_wait_ms);
    log("Replay: live after %s at frame %u, event %u (byte %u of %u), fixed heap %u KB at most, %u blocks freed by other threads",
        why, g_frame, g_events, g_in_pos, g_in_size, g_heap_peak >> 10, g_lost_blocks);
    g_mode = LIVE;
}

// The next recorded event, if it is of this kind and length; otherwise the replay goes live and the caller asks Windows
// The calls' return addresses (the game's call sites), the last 16 kept for the log of a divergence
DWORD g_caller;
constexpr int NRING = 16;
DWORD g_ring_caller[NRING];
BYTE g_ring_kind[NRING];
DWORD g_ring_pos;
const char *caller_name(DWORD a, DWORD *off);

DWORD g_cycle[2];       // the input positions of the last two events read ([0] the older)
DWORD g_repeat_left;    // events of an E_REPEAT2 still to hand out
int g_repeat_next;

void log_ring(BYTE kind)
{
    char line[1024];
    int o = wsprintfA(line, "Replay: the last calls before:");
    for (DWORD i = 0; i < NRING; i++) {
        const DWORD k = (g_ring_pos + i) % NRING;
        if (!g_ring_kind[k])
            continue;
        DWORD off;
        const char *name = caller_name(g_ring_caller[k], &off);
        o += wsprintfA(line + o, " %s@%s+%x", kind_name(g_ring_kind[k]), name, off);
        if (o > 900)
            break;
    }
    log("%s", line);
    DWORD off;
    const char *name = caller_name(g_caller, &off);
    log("Replay: the diverging call: %s from %s+%x (%x)", kind_name(kind), name, off, g_caller);
}

// got: an event of up to len bytes is taken and its length put there (the network's events, version 7); without it
// the event must have exactly len bytes (PeekMessageA's up to len)
bool take(BYTE kind, DWORD *ret, void *data, DWORD len, DWORD *got = nullptr)
{
    if (g_mode != REPLAY)
        return false;
    check_surfaces();
    g_ring_kind[g_ring_pos % NRING] = kind;
    g_ring_caller[g_ring_pos % NRING] = g_caller;
    g_ring_pos++;
    DWORD pos;
    if (g_repeat_left) {
        pos = g_cycle[g_repeat_next];
    } else {
        if (g_in_pos + 7 <= g_in_size && g_in[g_in_pos] == E_REPEAT2) {
            memcpy(&g_repeat_left, g_in + g_in_pos + 3, 4);
            g_repeat_next = 0;
            g_in_pos += 7;
            return take(kind, ret, data, len, got);
        }
        if (g_in_pos + 7 > g_in_size) {
            go_live("end of the recording");
            return false;
        }
        pos = g_in_pos;
    }
    const BYTE *e = g_in + pos;
    const DWORD n = e[1] | (DWORD)e[2] << 8;
    if (e[0] != kind || (kind != E_PEEK && !got && n != len) || n > len || pos + 7 + n > g_in_size) {
        char why[160];
        wsprintfA(why, "DIVERGED: the game calls %s (%u bytes), the recording has %s (%u bytes)", kind_name(kind), len,
                  kind_name(e[0]), n);
        go_live(why);
        g_ring_pos--;
        log_ring(kind);
        return false;
    }
    memcpy(ret, e + 3, 4);
    if (data && n)
        memcpy(data, e + 7, n);
    if (got)
        *got = n;
    if (g_repeat_left) {
        g_repeat_next ^= 1;
        if (!--g_repeat_left && g_repeat_next) {   // ended on the older one: the last two swap
            const DWORD t = g_cycle[0];
            g_cycle[0] = g_cycle[1];
            g_cycle[1] = t;
        }
    } else {
        g_cycle[0] = g_cycle[1];
        g_cycle[1] = pos;
        g_in_pos += 7 + n;
    }
    g_events++;
    return true;
}

// The kind of the next recorded event without taking it (-1 at the end or while not replaying)
int peek_kind()
{
    if (g_mode != REPLAY)
        return -1;
    if (g_repeat_left)
        return g_in[g_cycle[g_repeat_next]];
    if (g_in_pos + 7 > g_in_size)
        return -1;
    return g_in[g_in_pos] == E_REPEAT2 ? g_in[g_cycle[0]] : g_in[g_in_pos];
}
// The next recorded event itself (kind, length, answer, data) without taking it; null at the end or while not replaying
const BYTE *peek_event()
{
    if (g_mode != REPLAY)
        return nullptr;
    if (g_repeat_left)
        return g_in + g_cycle[g_repeat_next];
    if (g_in_pos + 7 > g_in_size)
        return nullptr;
    return g_in + (g_in[g_in_pos] == E_REPEAT2 ? g_cycle[0] : g_in_pos);
}

// ---- The import table's replacements ----------------------------------------------------------------------------------

struct Slot {
    ImpKey iat;   // a key (batch 6-10a): our own IAT's slot in the standalone exe once it moved
    void *replacement;
    DWORD real;
};

#define REAL(i, T) reinterpret_cast<T>(static_cast<uintptr_t>(g_slots[i].real))
extern Slot g_slots[];

// A call without arguments that answers a dword
template <int I, BYTE K> DWORD WINAPI dword0()
{
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    if (!main_thread())
        return REAL(I, DWORD(WINAPI *)())();
    DWORD r;
    if (g_mode == REPLAY && take(K, &r, nullptr, 0))
        return r;
    r = REAL(I, DWORD(WINAPI *)())();
    if (g_mode == RECORD)
        put(K, r, nullptr, 0);
    return r;
}

// A call that fills a structure of n bytes through its only argument (and answers a dword or nothing)
template <int I, BYTE K, DWORD N> DWORD WINAPI fill1(void *p)
{
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    if (!main_thread())
        return REAL(I, DWORD(WINAPI *)(void *))(p);
    DWORD r;
    if (g_mode == REPLAY && take(K, &r, p, N))
        return r;
    r = REAL(I, DWORD(WINAPI *)(void *))(p);
    if (g_mode == RECORD)
        put(K, r, p, N);
    return r;
}

// ---- Other real-time answers (version 6, batch 6-13) ----------------------------------------------------------------
// Found by a search of every import and COM method the game calls for answers that change in real time
// (docs/DECOMPILATION.md 13, "Batch 6-13"); recorded and replayed on the main thread only, other threads ask live.
// - RADTimerRead and BinkWait (binkw32): a movie's clock and whether its next frame is due. CBinkMovie's loop on the main
//   thread (bink_rest5a.cpp) decodes, draws and pumps messages by them; /SkipMovies skips only the start's movies, a
//   level's CPlayMovieAction plays one in the middle of a play.
// - FileTimeToDosDateTime (kernel32): the file search (file_search.cpp, 0x5ea0ac) turns the write time of every file it
//   lists into a DOS date and time, and save games are written by the play itself, at its real time; the DOS "now"
//   (DosTime_Now) comes from GetSystemTimeAsFileTime, recorded already, and is recorded again here (harmless).
// - IDirectSoundBuffer::GetCurrentPosition: "The streams' play position" below.
// - Not recorded: RADTimerRead inside binkw32's file callbacks (BinkIo_ReadHeader … BinkIo_Idle, 0x4323d0-0x432a9b;
//   bink_rest5a.cpp). binkw32 calls them on the main thread (inside its own calls) or on its I/O thread as its buffer
//   and the disk allow, so their clock reads have no fixed place among the main thread's calls: the first replay of a
//   /Movies play left its recording at frame 9 of the start movie, "the game calls BinkWait, the recording has
//   RADTimerRead" from BinkIo_ReadFrame, with /Original, hooked and the exe alike (batch gaps-menu, jobs
//   20261101-092544-2bd3, -6969, -092733-6de0). The reads only add to the file's I/O times (+0x28 … +0x34), which only
//   BinkGetSummary reports, and the game takes the width and height of the summary (BinkMovie_Open 0x431cf0): they are
//   answered live.
bool bink_io_caller(DWORD a)
{
    if (a >= 0x4323d0 && a < 0x432a9b)   // the original's callbacks (/Original)
        return true;
    if (a >= 0x00401000 && a < layer::TEXT_END)
        return false;
    // ours (hooked, the standalone exe): a replacement named BinkIo_*, but not BinkIo_Open (the main thread's open)
    const char *name = hook_near(a, nullptr);
    const char prefix[] = "BinkIo_";
    for (int i = 0; prefix[i]; i++)
        if (name[i] != prefix[i])
            return false;
    return !(name[7] == 'O' && name[8] == 'p');
}
template <int I, BYTE K> DWORD WINAPI dword0_v6()
{
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    if (!g_v6_events || !main_thread() || (K == E_BINK_TIMER && bink_io_caller(g_caller)))
        return REAL(I, DWORD(WINAPI *)())();
    DWORD r;
    if (g_mode == REPLAY && take(K, &r, nullptr, 0))
        return r;
    r = REAL(I, DWORD(WINAPI *)())();
    if (g_mode == RECORD)
        put(K, r, nullptr, 0);
    return r;
}
template <int I, BYTE K> DWORD WINAPI dword1_v6(DWORD a)
{
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    if (!g_v6_events || !main_thread())
        return REAL(I, DWORD(WINAPI *)(DWORD))(a);
    DWORD r;
    if (g_mode == REPLAY && take(K, &r, nullptr, 0))
        return r;
    r = REAL(I, DWORD(WINAPI *)(DWORD))(a);
    if (g_mode == RECORD)
        put(K, r, nullptr, 0);
    return r;
}
template <int I> DWORD WINAPI my_FileTimeToDosDateTime(const void *ft, WORD *date, WORD *time)
{
    using F = DWORD(WINAPI *)(const void *, WORD *, WORD *);
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    if (!g_v6_events || !main_thread())
        return REAL(I, F)(ft, date, time);
    WORD dt[2] = {0, 0};
    DWORD r;
    if (g_mode == REPLAY && take(E_DOS_TIME, &r, dt, 4)) {
        if (date)
            *date = dt[0];
        if (time)
            *time = dt[1];
        return r;
    }
    r = REAL(I, F)(ft, &dt[0], &dt[1]);
    if (date)
        *date = dt[0];
    if (time)
        *time = dt[1];
    if (g_mode == RECORD)
        put(E_DOS_TIME, r, dt, 4);
    return r;
}

DWORD WINAPI my_IsIconic(void *hwnd);
DWORD WINAPI my_PeekMessageA(DWORD *msg, void *hwnd, DWORD lo, DWORD hi, DWORD flags);
DWORD WINAPI my_joyGetDevCapsA(DWORD id, void *caps, DWORD size);
DWORD WINAPI my_joyGetPosEx(DWORD id, DWORD *info);
HANDLE WINAPI my_CreateFileA(const char *name, DWORD access, DWORD share, void *sec, DWORD disposition, DWORD flags, HANDLE tmpl);
BOOL WINAPI my_CloseHandle(HANDLE h);
HANDLE WINAPI my_HeapCreate(DWORD flags, DWORD initial, DWORD maximum);
void *WINAPI my_HeapAlloc(HANDLE heap, DWORD flags, DWORD n);
BOOL WINAPI my_HeapFree(HANDLE heap, DWORD flags, void *p);
void *WINAPI my_HeapReAlloc(HANDLE heap, DWORD flags, void *p, DWORD n);
DWORD WINAPI my_HeapSize(HANDLE heap, DWORD flags, const void *p);

enum { S_PEEK, S_TIME, S_TICK, S_QPC, S_QPF, S_SYSFT, S_LOCAL, S_SYSTIME, S_TZ, S_JOYNUM, S_JOYCAPS, S_JOYPOS, S_MEMSTAT, S_ICONIC, S_CREATE, S_CLOSE, S_HEAPCREATE, S_HEAPALLOC, S_HEAPFREE, S_HEAPREALLOC, S_HEAPSIZE,
       S_BINK_TIMER, S_BINK_WAIT, S_DOS_TIME,
       // version 7 (batch 6-14, "The network" below)
       S_COCREATE, S_ENTERCS, S_WS_STARTUP, S_WS_CLEANUP, S_WS_SOCKET, S_WS_CLOSESOCKET, S_WS_BIND, S_WS_GETSOCKNAME,
       S_WS_SETSOCKOPT, S_WS_CONNECT, S_WS_SEND, S_WS_RECV, S_WS_SENDTO, S_WS_RECVFROM, S_WS_SELECT, S_WS_GETHOSTBYNAME,
       // version 8 (batch 6-16a, "Focus, surface loss and the clipboard" below)
       S_REGCLASS, S_GETCLIP, S_DEFPROC, S_MOVEWINDOW, S_BEGINPAINT, S_ENDPAINT, S_DISPATCH,
       // DirectDrawCreate: DirectDraw's vtable wrapped (rehook_surfaces below); records nothing
       S_DDCREATE, NSLOT };
DWORD WINAPI my_DirectDrawCreate(void *guid, DWORD *out, void *outer);

// Focus, surface loss and the clipboard (defined below)
DWORD WINAPI my_RegisterClassA(const DWORD *wc);
DWORD WINAPI my_GetClipboardData(DWORD format);
DWORD WINAPI my_DefWindowProcA(DWORD hwnd, DWORD msg, DWORD wp, DWORD lp);
template <int I> DWORD WINAPI my_sending(DWORD a, DWORD b, DWORD c, DWORD d, DWORD e, DWORD f);
template <int I> DWORD WINAPI my_sending2(DWORD a, DWORD b);
DWORD WINAPI my_DispatchMessageA(const DWORD *msg);

// The network's replacements in the import table (defined in "The network" below)
DWORD WINAPI my_CoCreateInstance(const BYTE *clsid, void *outer, DWORD context, const BYTE *iid, void **out);
void WINAPI my_EnterCriticalSection(void *cs);
void ws_record(int slot, const DWORD *args, DWORD r);
DWORD ws_replay(int slot, const DWORD *args);
void ws_replay_startup(int slot, const DWORD *args);
// A Winsock call of the GameSpy SDK (stdcall, every argument a dword): recorded and replayed on the main thread; the
// pure ones (htons, inet_addr, inet_ntoa, __WSAFDIsSet …) stay live
template <int I, typename... A> struct Ws {
    static DWORD WINAPI f(A... a)
    {
        using F = DWORD(WINAPI *)(A...);
        g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
        if (!g_v7_events || !main_thread())
            return REAL(I, F)(a...);
        const DWORD args[sizeof...(A) + 1] = {(DWORD)a..., 0};
        if (g_mode == REPLAY) {
            ws_replay_startup(I, args);
            return ws_replay(I, args);
        }
        const DWORD r = REAL(I, F)(a...);
        if (g_mode == RECORD)
            ws_record(I, args, r);
        return r;
    }
};
template <int I, int N, typename... A> struct WsN : WsN<I, N - 1, DWORD, A...> {};
template <int I, typename... A> struct WsN<I, 0, A...> {
    using T = Ws<I, A...>;
};
#define WS(I, N) ptr(&WsN<I, N>::T::f)
// The replacements are filled in by replay_install: the DLL has no C runtime start, so a table initialised with
// function addresses at load (a dynamic initializer, ptr() is not constexpr) would stay zero
Slot g_slots[NSLOT] = {
    {0x5ea230, nullptr, 0}, {0x5ea294, nullptr, 0}, {0x5ea0e8, nullptr, 0}, {0x5ea0f8, nullptr, 0}, {0x5ea0fc, nullptr, 0},
    {0x5ea0b0, nullptr, 0}, {0x5ea138, nullptr, 0}, {0x5ea13c, nullptr, 0}, {0x5ea140, nullptr, 0}, {0x5ea298, nullptr, 0},
    {0x5ea28c, nullptr, 0}, {0x5ea29c, nullptr, 0}, {0x5ea0ec, nullptr, 0}, {0x5ea22c, nullptr, 0}, {0x5ea0d4, nullptr, 0},
    {0x5ea0d8, nullptr, 0}, {0x5ea1a8, nullptr, 0}, {0x5ea154, nullptr, 0}, {0x5ea16c, nullptr, 0}, {0x5ea170, nullptr, 0},
    {0x5ea174, nullptr, 0}, {0x5ea334, nullptr, 0}, {0x5ea328, nullptr, 0}, {0x5ea0ac, nullptr, 0},
    // CoCreateInstance, EnterCriticalSection, wsock32 (by ordinal: WSAStartup, WSACleanup, socket, closesocket, bind,
    // getsockname, setsockopt, connect, send, recv, sendto, recvfrom, select, gethostbyname; gamespy_thunks.cpp)
    {0x5ea340, nullptr, 0}, {0x5ea098, nullptr, 0}, {0x5ea2e8, nullptr, 0}, {0x5ea2f0, nullptr, 0}, {0x5ea2a4, nullptr, 0},
    {0x5ea2a8, nullptr, 0}, {0x5ea2d8, nullptr, 0}, {0x5ea2d4, nullptr, 0}, {0x5ea2c8, nullptr, 0}, {0x5ea2c0, nullptr, 0},
    {0x5ea2b8, nullptr, 0}, {0x5ea2bc, nullptr, 0}, {0x5ea2ac, nullptr, 0}, {0x5ea2b0, nullptr, 0}, {0x5ea2b4, nullptr, 0},
    {0x5ea2e0, nullptr, 0},
    // RegisterClassA, GetClipboardData
    {0x5ea200, nullptr, 0}, {0x5ea1ec, nullptr, 0}, {0x5ea21c, nullptr, 0},
    // MoveWindow, BeginPaint, EndPaint (the window procedure's calls that send messages)
    {0x5ea218, nullptr, 0}, {0x5ea220, nullptr, 0}, {0x5ea224, nullptr, 0},
    // DispatchMessageA
    {0x5ea234, nullptr, 0},
    // DirectDrawCreate
    {0x5ea010, nullptr, 0},
};
void fill_slots()
{
    void *const r[NSLOT] = {
        ptr(&my_PeekMessageA), ptr(&dword0<S_TIME, E_TIME>), ptr(&dword0<S_TICK, E_TICK>), ptr(&fill1<S_QPC, E_QPC, 8>),
        ptr(&fill1<S_QPF, E_QPF, 8>), ptr(&fill1<S_SYSFT, E_SYSFT, 8>), ptr(&fill1<S_LOCAL, E_LOCAL, 16>),
        ptr(&fill1<S_SYSTIME, E_SYSTIME, 16>), ptr(&fill1<S_TZ, E_TZ, 172>), ptr(&dword0<S_JOYNUM, E_JOYNUM>),
        ptr(&my_joyGetDevCapsA), ptr(&my_joyGetPosEx), ptr(&fill1<S_MEMSTAT, E_MEMSTAT, 32>), ptr(&my_IsIconic),
        ptr(&my_CreateFileA), ptr(&my_CloseHandle), ptr(&my_HeapCreate), ptr(&my_HeapAlloc), ptr(&my_HeapFree),
        ptr(&my_HeapReAlloc), ptr(&my_HeapSize), ptr(&dword0_v6<S_BINK_TIMER, E_BINK_TIMER>),
        ptr(&dword1_v6<S_BINK_WAIT, E_BINK_WAIT>), ptr(&my_FileTimeToDosDateTime<S_DOS_TIME>),
        ptr(&my_CoCreateInstance), ptr(&my_EnterCriticalSection), WS(S_WS_STARTUP, 2), WS(S_WS_CLEANUP, 0),
        WS(S_WS_SOCKET, 3), WS(S_WS_CLOSESOCKET, 1), WS(S_WS_BIND, 3), WS(S_WS_GETSOCKNAME, 3), WS(S_WS_SETSOCKOPT, 5),
        WS(S_WS_CONNECT, 3), WS(S_WS_SEND, 4), WS(S_WS_RECV, 4), WS(S_WS_SENDTO, 6), WS(S_WS_RECVFROM, 6),
        WS(S_WS_SELECT, 5), WS(S_WS_GETHOSTBYNAME, 1), ptr(&my_RegisterClassA), ptr(&my_GetClipboardData),
        ptr(&my_DefWindowProcA), ptr(&my_sending<S_MOVEWINDOW>), ptr(&my_sending2<S_BEGINPAINT>),
        ptr(&my_sending2<S_ENDPAINT>), ptr(&my_DispatchMessageA), ptr(&my_DirectDrawCreate),
    };
    for (int i = 0; i < NSLOT; i++)
        g_slots[i].replacement = r[i];
}

DWORD WINAPI my_IsIconic(void *hwnd)
{
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    if (!main_thread())
        return REAL(S_ICONIC, DWORD(WINAPI *)(void *))(hwnd);
    DWORD r;
    if (g_mode == REPLAY && take(E_ICONIC, &r, nullptr, 0))
        return r;
    r = REAL(S_ICONIC, DWORD(WINAPI *)(void *))(hwnd);
    if (g_mode == RECORD)
        put(E_ICONIC, r, nullptr, 0);
    return r;
}

DWORD WINAPI my_joyGetDevCapsA(DWORD id, void *caps, DWORD size)
{
    using F = DWORD(WINAPI *)(DWORD, void *, DWORD);
    if (!main_thread() || size > 0x400)
        return REAL(S_JOYCAPS, F)(id, caps, size);
    DWORD r;
    if (g_mode == REPLAY && take(E_JOYCAPS, &r, caps, size))
        return r;
    r = REAL(S_JOYCAPS, F)(id, caps, size);
    if (g_mode == RECORD)
        put(E_JOYCAPS, r, caps, size);
    return r;
}

DWORD WINAPI my_joyGetPosEx(DWORD id, DWORD *info)
{
    using F = DWORD(WINAPI *)(DWORD, DWORD *);
    const DWORD size = info ? info[0] : 0;
    if (!main_thread() || size > 0x100)
        return REAL(S_JOYPOS, F)(id, info);
    DWORD r;
    if (g_mode == REPLAY && take(E_JOYPOS, &r, info, size))
        return r;
    r = REAL(S_JOYPOS, F)(id, info);
    if (g_mode == RECORD)
        put(E_JOYPOS, r, info, size);
    return r;
}

// ---- A heap at a fixed address ------------------------------------------------------------------------------------

// The game writes the addresses of its objects into its save games ("Object ID=0x0B5DAB90", the references), and
// Windows puts the C runtime's heap at a different base in every process (two /Original replays: the same layout,
// bases 0x08bc0000 apart). With [Reimpl] FixedHeap=1 (the default with Record or Replay) the C runtime's heap (the
// one HeapCreate makes for it at the start) is served from a region reserved at a fixed address by a simple
// deterministic allocator: blocks of 16-byte steps up to 4 KB from exact-size free lists, bigger ones in whole pages
// from exact page-count lists, else from the end of the used part; an 8-byte header holds the requested size
// (HeapSize) and the block's size. Freed blocks keep their bytes (as Windows' heap mostly does), new memory is zero.
// The main thread allocates from the first part of the region, the other threads (the sound and music streams, at
// times of their own) from the second, so their timing does not move the main thread's blocks. The main thread's
// part is only ever appended to (a freed block is not used again, see my_HeapFree), so its addresses follow from the
// main thread's sequence of allocations alone; the other part reuses freed blocks.
// The C runtime's start-up blocks (the thread data, the file table, the copy of the environment, __argv and every
// _environ string) go to the other part as well, up to the main thread's first free (the environment copy, which
// _setenvp frees last): their number and sizes follow the process's environment block and command line (the
// standalone exe is started as Zax-Standalone.exe without /Original, a VM restart changed the environment: the main
// part shifted by 0x50-0x160 bytes and 1-3 blocks, DECOMPILATION.md 13, "Batch 6-11"), and in the main part they moved
// every later block, so the save games' object ids and Cave1's Auto-Animate phases (an entity's address). With
// [Reimpl] HeapStartApart=0 they stay in the main part (the layout before batch 6-11).
constexpr DWORD HEAP_BASES[] = {0x30000000, 0x20000000, 0x40000000};
constexpr DWORD HEAP_SIZE = 0x30000000, HEAP_MAIN = 0x28000000, HEAP_COMMIT_STEP = 0x100000, SMALL_MAX = 4096,
                LARGE_PAGES = 4096;
struct Region {
    DWORD base, end, top, committed;
    DWORD small_free[SMALL_MAX / 16 + 1];
    DWORD large_free[LARGE_PAGES + 1];
    DWORD huge_free;   // blocks above LARGE_PAGES pages: a list searched for an exact size
    DWORD allocs;
};
Region g_region[2];   // [0] the main thread's, [1] the other threads'
bool g_fixed_heap;
bool g_heap_start;   // the C runtime's start-up blocks still go to the other part (HeapStartApart, above)
DWORD g_heap_start_allocs, g_heap_start_bytes;
DWORD g_heap_base, g_heap_end;
HANDLE g_crt_heap;
volatile long g_heap_lock;
bool g_heap_full_logged;
DWORD g_heap_spins, g_heap_frees, g_heap_moves, g_heap_inplace;
unsigned long long g_heap_copied;
// [Reimpl] HeapLog=<n>: the first n calls logged ("Heap" lines: a/f/r, the size, the block's offset in the region, the
// thread (main or other) and the caller's return address), to find where two runs' heaps part
int g_heap_log;

// An exe address as itself, an address of this DLL as the hook before it (main.cpp's hook_near)
const char *caller_name(DWORD a, DWORD *off)
{
    if (a >= 0x00401000 && a < layer::TEXT_END) {
        *off = a;
        return "exe";
    }
    DWORD f;
    const char *name = hook_near(a, &f);
    *off = a - f;
    return name;
}

void heap_line(char op, DWORD n, DWORD p, DWORD caller)
{
    if (g_heap_log <= 0)
        return;
    g_heap_log--;
    // the callers: the first return-address-like dwords on the stack in the exe's code or this DLL's image
    struct {
        void *base, *allocation_base;
        DWORD allocation_protect;
        size_t size;
        DWORD state, protect, type;
    } mi;
    static DWORD dll_base;
    if (!dll_base && VirtualQuery(reinterpret_cast<void *>(&heap_line), &mi, sizeof mi))
        dll_base = (DWORD)(uintptr_t)mi.allocation_base;
    char callers[700] = "";
    int o = 0, found = 0;
    DWORD *sp = static_cast<DWORD *>(__builtin_frame_address(0));
    for (int i = 0; i < 400 && found < 12; i++) {
        const DWORD v = sp[i];
        if (v >= 0x00401000 && v < layer::TEXT_END) {
            o += wsprintfA(callers + o, " %x", v);
            found++;
        } else if (dll_base && v > dll_base + 0x1000 && v < dll_base + 0x1000000) {
            DWORD f;
            const char *name = hook_near(v, &f);
            o += wsprintfA(callers + o, " %.28s+%x", name, v - f);
            found++;
        }
    }
    log("Heap %c %x %x %s %x%s", op, n, p ? p - g_heap_base : 0, GetCurrentThreadId() == g_main_thread ? "m" : "o", caller, callers);
}

// [Reimpl] PoolLog=<n> with PoolLogSize=<bytes>: the first n allocations and frees of the game's pool (0x417710) in
// the size class of PoolLogSize, as "Pool" lines with the frame and the callers (batch 6-12, to find where two runs'
// pool blocks part; the pool hands out a freed block first, so the order of the frees decides the next addresses)
int g_pool_log;
DWORD g_pool_stats;
DWORD g_pool_log_from;   // [Reimpl] PoolLogFrom=<frame>: PoolLog's lines from that frame on   // [Reimpl] PoolStats=<bytes>: that size class's use logged at every frame
void pool_line_impl(char op, DWORD n, const void *p)
{
    g_pool_log--;
    char callers[900] = "";
    int o = 0, found = 0;
    static DWORD dll_base;
    struct {
        void *base, *allocation_base;
        DWORD allocation_protect;
        size_t size;
        DWORD state, protect, type;
    } mi;
    if (!dll_base && VirtualQuery(reinterpret_cast<void *>(&pool_line_impl), &mi, sizeof mi))
        dll_base = (DWORD)(uintptr_t)mi.allocation_base;
    DWORD *sp = static_cast<DWORD *>(__builtin_frame_address(0));
    for (int i = 0; i < 400 && found < 16; i++) {
        const DWORD v = sp[i];
        if (v >= 0x00401000 && v < layer::TEXT_END) {
            o += wsprintfA(callers + o, " %x", v);
            found++;
        } else if (dll_base && v > dll_base + 0x1000 && v < dll_base + 0x1000000) {
            DWORD f;
            const char *name = hook_near(v, &f);
            o += wsprintfA(callers + o, " %.28s+%x", name, v - f);
            found++;
        }
    }
    log("Pool %c %x %x %s frame %u%s", op, n, (DWORD)(uintptr_t)p, GetCurrentThreadId() == g_main_thread ? "m" : "o", g_frame, callers);
}

struct HeapGuard {
    HeapGuard()
    {
        while (__atomic_exchange_n(&g_heap_lock, 1, __ATOMIC_ACQUIRE)) {
            g_heap_spins++;
            Sleep(0);
        }
    }
    ~HeapGuard() { __atomic_store_n(&g_heap_lock, 0, __ATOMIC_RELEASE); }
};

inline bool in_heap(const void *p) { const DWORD a = (DWORD)(uintptr_t)p; return a >= g_heap_base && a < g_heap_end; }
inline Region &region_of(DWORD block) { return g_region[block >= g_region[1].base]; }
// header: +0 the requested size, +4 the block's size (header included)
inline DWORD &block_size(DWORD block) { return *at<DWORD *>(block + 4); }
inline DWORD &block_request(DWORD block) { return *at<DWORD *>(block); }

void reserve_heap()
{
    for (DWORD base : HEAP_BASES)
        if (VirtualAlloc(at<void *>(base), HEAP_SIZE, MEM_RESERVE, PAGE_READWRITE)) {
            g_heap_base = base;
            g_heap_end = base + HEAP_SIZE;
            g_region[0].base = g_region[0].top = g_region[0].committed = base;
            g_region[0].end = g_region[1].base = g_region[1].top = g_region[1].committed = base + HEAP_MAIN;
            g_region[1].end = g_heap_end;
            g_fixed_heap = true;
            log("Replay: fixed heap at %x (%u MB reserved, %u MB for the main thread)", base, HEAP_SIZE >> 20, HEAP_MAIN >> 20);
            return;
        }
    log("Replay: no fixed heap (the address range is taken), the C runtime's heap stays Windows'");
}

DWORD heap_take(Region &r, DWORD total)
{
    DWORD *list = total <= SMALL_MAX ? &r.small_free[total / 16] : total / 0x1000 <= LARGE_PAGES ? &r.large_free[total / 0x1000] : nullptr;
    if (list && *list) {
        const DWORD b = *list;
        *list = *at<DWORD *>(b + 8);
        return b;
    }
    if (!list)
        for (DWORD *link = &r.huge_free; *link; link = at<DWORD *>(*link + 8))
            if (block_size(*link) == total) {
                const DWORD b = *link;
                *link = *at<DWORD *>(b + 8);
                return b;
            }
    if (total > r.end - r.top)
        return 0;
    const DWORD b = r.top;
    r.top += total;
    if (r.top > r.committed) {
        const DWORD upto = (r.top + HEAP_COMMIT_STEP - 1) & ~(HEAP_COMMIT_STEP - 1);
        if (!VirtualAlloc(at<void *>(r.committed), upto - r.committed, MEM_COMMIT, PAGE_READWRITE)) {
            r.top = b;
            return 0;
        }
        r.committed = upto;
    }
    if (&r == &g_region[0] && r.top - r.base > g_heap_peak)
        g_heap_peak = r.top - r.base;
    block_size(b) = total;
    return b;
}

void heap_give(DWORD b)
{
    Region &r = region_of(b);
    const DWORD total = block_size(b);
    DWORD *list = total <= SMALL_MAX ? &r.small_free[total / 16] : total / 0x1000 <= LARGE_PAGES ? &r.large_free[total / 0x1000] : &r.huge_free;
    *at<DWORD *>(b + 8) = *list;
    *list = b;
}

// Version 7 (batch 6-14): the 8 bytes after a block's requested size, where they lie in the block's own padding, are
// 0xab instead of zero. The DirectPlay message handler's create player (dplay.cpp) takes the highest free player slot
// from 100 down, and slot 100 lies past the 0x900-byte net game object (docs/ORIGINAL_BUGS.md 1.16): "free" when the
// dword at +0x904 is zero. Behind a Windows heap block that dword belongs to the next block's header, which is not
// zero [assumption: Windows' heap entry header; the LAN games of batch 6-11 without the fixed heap all joined], so a
// joining player gets slot 99; in the fixed heap's zeroed padding it got slot 100, which ProcessSlots (slots 0-99)
// never adds, and the client never got the map (the first LAN recording, run 20261030-214158-f9a0). Only with a
// version 7 recording (or Record), so the single-player recordings of version 6 and older replay as before.
bool g_heap_tail_mark;
inline void mark_tail(DWORD b, DWORD n)
{
    const DWORD end = b + 8 + n, limit = b + block_size(b);
    for (DWORD k = 0; k < 8 && end + k < limit; k++)
        *at<BYTE *>(end + k) = 0xab;
}

inline DWORD block_total(DWORD n) { const DWORD t = (n + 8 + 15) & ~15u; return t <= SMALL_MAX ? t : (t + 0xfff) & ~0xfffu; }

HANDLE WINAPI my_HeapCreate(DWORD flags, DWORD initial, DWORD maximum)
{
    HANDLE h = REAL(S_HEAPCREATE, HANDLE(WINAPI *)(DWORD, DWORD, DWORD))(flags, initial, maximum);
    if (g_fixed_heap && !g_crt_heap)
        g_crt_heap = h;   // the C runtime's heap, made first by _heap_init
    return h;
}

// A block from region r (0 the main thread's, 1 the others'); Windows' heap when the region is full
void *heap_alloc_in(HANDLE heap, DWORD flags, DWORD n, int region)
{
    const DWORD total = block_total(n);
    Region &r = g_region[region];
    DWORD b;
    {
        HeapGuard guard;
        b = heap_take(r, total);
        if (b)
            r.allocs++;
    }
    if (!b) {
        if (!g_heap_full_logged) {
            g_heap_full_logged = true;
            log("Replay: the fixed heap is full (%u bytes asked), Windows' heap from here on", n);
        }
        return REAL(S_HEAPALLOC, void *(WINAPI *)(HANDLE, DWORD, DWORD))(heap, flags, n);
    }
    block_request(b) = n;
    if (g_heap_tail_mark)
        mark_tail(b, n);
    heap_line('a', n, b, (DWORD)(uintptr_t)__builtin_return_address(0));
    if (flags & 8)   // HEAP_ZERO_MEMORY
        memset(at<void *>(b + 8), 0, n);
    return at<void *>(b + 8);
}

void *WINAPI my_HeapAlloc(HANDLE heap, DWORD flags, DWORD n)
{
    if (heap != g_crt_heap || !g_crt_heap || n > 0x7fff0000)
        return REAL(S_HEAPALLOC, void *(WINAPI *)(HANDLE, DWORD, DWORD))(heap, flags, n);
    const bool main = GetCurrentThreadId() == g_main_thread;
    if (main && g_heap_start) {
        g_heap_start_allocs++;
        g_heap_start_bytes += n;
    }
    return heap_alloc_in(heap, flags, n, !main || g_heap_start);
}

BOOL WINAPI my_HeapFree(HANDLE heap, DWORD flags, void *p)
{
    if (!p || !in_heap(p))
        return REAL(S_HEAPFREE, BOOL(WINAPI *)(HANDLE, DWORD, void *))(heap, flags, p);
    const DWORD b = (DWORD)(uintptr_t)p - 8;
    heap_line('f', block_request(b), b, (DWORD)(uintptr_t)__builtin_return_address(0));
    if (g_heap_start && GetCurrentThreadId() == g_main_thread) {
        g_heap_start = false;
        log("Replay: the C runtime's %u start-up blocks (%u bytes) in the other threads' part, the main part from here on",
            g_heap_start_allocs, g_heap_start_bytes);
    }
    // The main thread's blocks are never used again: which thread frees some of them, and when, follows the sound
    // threads' timing (two /Original replays of Cave1 parted on whether one block went back to the main thread's
    // lists), and the game's drawing order depends on the addresses
    if (&region_of(b) == &g_region[0]) {
        if (GetCurrentThreadId() != g_main_thread)
            g_lost_blocks++;
        g_heap_frees++;
        return 1;
    }
    HeapGuard guard;
    heap_give(b);
    g_heap_frees++;
    return 1;
}

void *WINAPI my_HeapReAlloc(HANDLE heap, DWORD flags, void *p, DWORD n)
{
    if (!p || !in_heap(p))
        return REAL(S_HEAPREALLOC, void *(WINAPI *)(HANDLE, DWORD, void *, DWORD))(heap, flags, p, n);
    const DWORD b = (DWORD)(uintptr_t)p - 8, old = block_request(b);
    if (n + 8 <= block_size(b) && n <= 0x7fff0000) {   // fits: in place
        if ((flags & 8) && n > old)
            memset(static_cast<BYTE *>(p) + old, 0, n - old);
        block_request(b) = n;
        if (g_heap_tail_mark)
            mark_tail(b, n);
        g_heap_inplace++;
        heap_line('r', n, b, (DWORD)(uintptr_t)__builtin_return_address(0));
        return p;
    }
    if (flags & 0x10)   // HEAP_REALLOC_IN_PLACE_ONLY
        return nullptr;
    // a moved block goes to the other threads' part: which thread grows a shared block first (the C runtime's atexit
    // table, appended by the first call of a function with a local static, from the main or a sound thread) follows
    // the threads' timing, and in the main thread's part it would move every later block
    void *q = heap_alloc_in(heap, flags & 8, n, 1);
    if (!q)
        return nullptr;
    memcpy(q, p, old < n ? old : n);
    g_heap_moves++;
    g_heap_copied += old < n ? old : n;
    my_HeapFree(heap, 0, p);
    return q;
}

DWORD WINAPI my_HeapSize(HANDLE heap, DWORD flags, const void *p)
{
    if (!p || !in_heap(p))
        return REAL(S_HEAPSIZE, DWORD(WINAPI *)(HANDLE, DWORD, const void *))(heap, flags, p);
    return block_request((DWORD)(uintptr_t)p - 8);
}

// ---- Save games ----------------------------------------------------------------------------------------------------

// A file the game writes below SaveGames is logged byte for byte when the game closes it ("Save file" and "Save" hex
// lines, compared by zax-testtools/tools/replay_compare.py --saves) and deleted again unless [Reimpl] KeepSaves=1, so a comparison
// run leaves no save game in the VM's game folder (the shot set's load dialog expects none)
bool g_keep_saves;
constexpr int NSAVE = 8;
HANDLE g_save_handle[NSAVE];
char g_save_name[NSAVE][MAX_PATH];

bool is_save_path(const char *name)
{
    static const char KEY[] = "savegames";
    for (const char *p = name; *p; p++) {
        int k = 0;
        while (KEY[k] && (p[k] | 0x20) == KEY[k])
            k++;
        if (!KEY[k] && (p[k] == '\\' || p[k] == '/'))
            return true;
    }
    return false;
}

HANDLE WINAPI my_CreateFileA(const char *name, DWORD access, DWORD share, void *sec, DWORD disposition, DWORD flags, HANDLE tmpl)
{
    using F = HANDLE(WINAPI *)(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
    HANDLE h = REAL(S_CREATE, F)(name, access, share, sec, disposition, flags, tmpl);
    if (h != INVALID_HANDLE_VALUE && (access & GENERIC_WRITE) && name && lstrlenA(name) < MAX_PATH && is_save_path(name))
        for (int i = 0; i < NSAVE; i++)
            if (!g_save_handle[i]) {
                g_save_handle[i] = h;
                lstrcpyA(g_save_name[i], name);
                break;
            }
    return h;
}

void dump_save(const char *name)
{
    HANDLE f = CreateFileA(name, 0x80000000u, FILE_SHARE_READ, nullptr, 3, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        log("Save file \"%s\": cannot be read", name);
        return;
    }
    const DWORD size = GetFileSize(f, nullptr);
    BYTE *data = size < 0x4000000 ? static_cast<BYTE *>(VirtualAlloc(nullptr, size + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)) : nullptr;
    DWORD got = 0;
    if (data && ReadFile(f, data, size, &got, nullptr) && got == size) {
        DWORD h = 2166136261u;
        for (DWORD i = 0; i < size; i++)
            h = (h ^ data[i]) * 16777619u;
        log("Save file \"%s\": %u bytes, fnv %08x, frame %u", name, size, h, g_frame);
        hex_lines("Save", data, size);
        log("Save file end");
    } else
        log("Save file \"%s\": read failed", name);
    CloseHandle(f);
    if (data)
        VirtualFree(data, 0, 0x8000 /* MEM_RELEASE */);
    if (!g_keep_saves)
        log("Save file \"%s\": %s", name, DeleteFileA(name) ? "deleted" : "NOT deleted");
}

BOOL WINAPI my_CloseHandle(HANDLE h)
{
    const BOOL r = REAL(S_CLOSE, BOOL(WINAPI *)(HANDLE))(h);
    if (h)
        for (int i = 0; i < NSAVE; i++)
            if (g_save_handle[i] == h) {
                g_save_handle[i] = nullptr;
                dump_save(g_save_name[i]);
            }
    return r;
}

// ---- The message queue ---------------------------------------------------------------------------------------------

constexpr DWORD G_WINDOW = 0x713e18, G_PRIMARY = 0x713348;
constexpr DWORD WM_PAINT = 0xf, WM_TIMER = 0x113;
// Thread timers (hwnd 0, the procedure in lParam: the launcher's timer that starts the map, a timer of a system DLL)
// are recorded with their procedure as an offset in its module (hwnd code 3 + the offset in lParam), since the
// modules' bases differ between processes and VMs. The replay keeps the real ones it takes out of the queue by
// procedure, and for a recorded one waits (up to 5 s) for the real timer of the same procedure and calls the
// procedure itself with the real timer id (the game's pump then dispatches a WM_TIMER without procedure, which does
// nothing). Handing out a real timer message for the recorded one ran the wrong procedure when another timer was the
// one waiting (the launcher's start of the map at the frame of a system timer).
constexpr int NTIMER = 8;
DWORD g_timer[NTIMER][7];   // real thread timer messages the replay took out of the queue, by procedure
void hook_flip();

using PeekFn = DWORD(WINAPI *)(DWORD *, void *, DWORD, DWORD, DWORD);

// A code address as (module base, offset): the base of the allocation that holds it
DWORD module_offset(DWORD a, DWORD *base)
{
    struct {
        void *base, *allocation_base;
        DWORD allocation_protect;
        size_t size;
        DWORD state, protect, type;
    } mi;
    *base = 0;
    if (!VirtualQuery(at<void *>(a), &mi, sizeof mi))
        return a;
    *base = (DWORD)(uintptr_t)mi.allocation_base;
    return a - *base;
}

void keep_timer(const DWORD *m)
{
    int slot = -1;
    for (int i = 0; i < NTIMER; i++)
        if (g_timer[i][1] && g_timer[i][3] == m[3]) {
            slot = i;
            break;
        } else if (!g_timer[i][1] && slot < 0)
            slot = i;
    if (slot >= 0)
        memcpy(g_timer[slot], m, sizeof g_timer[slot]);
}

// The real queue emptied: sent messages are handled inside PeekMessageA, the rest dropped (WM_PAINT validated so it
// does not come back, a thread timer's WM_TIMER kept for a recorded one)
int g_in_peek;   // the main thread inside the real PeekMessageA (version 8: the sent messages it delivers, below)
DWORD real_peek(DWORD *m)
{
    g_in_peek++;
    const DWORD r = REAL(S_PEEK, PeekFn)(m, nullptr, 0, 0, 1);
    g_in_peek--;
    return r;
}
void drain()
{
    DWORD m[7];
    for (int k = 0; k < 256 && real_peek(m); k++)
        if (m[1] == WM_PAINT)
            ValidateRect(reinterpret_cast<void *>(static_cast<uintptr_t>(m[0])), nullptr);
        else if (m[1] == WM_TIMER && !m[0] && m[3])
            keep_timer(m);
}
void deliver_sent();   // "Focus, surface loss and the clipboard" below

#ifdef ZAX_STANDALONE
}  // namespace
}  // namespace zax::replay
WINIMPORT HANDLE WINAPI GetModuleHandleA(const char *name);
extern "C" const unsigned long zax_getter_vars[][2];   // own_blocks.cpp (tools/extract_exe_data.py)
extern "C" const int zax_getter_var_count;
namespace zax::replay {
namespace {
#endif
// The recorded thread timer's procedure called with the real timer of the same procedure
void replay_thread_timer(const DWORD *rec)
{
    // 5 s by the real clock. Until 2026-11-01 it was 5,000 rounds of Sleep(1): about 5 s on the ARM VMs, but 80 s on the
    // x86 VMs, 16 ms a Sleep(1) [assumption: the default timer period of 15.6 ms there]; a system DLL's timer recorded at
    // frame 0 or 39 that never fires there made 151 of the final checks' 320 single-player replays on x86 80 s longer
    // (docs/DECOMPILATION.md 13, "Batch test speed"). The frames do not depend on the wait: the ARM and x86 replays of
    // the same recordings were the same.
    const DWORD start = REAL(S_TICK, DWORD(WINAPI *)())();
    for (;;) {
        for (int i = 0; i < NTIMER; i++) {
            DWORD base;
            // In the standalone exe the launcher's code is built into the exe (batch 6-1): its timer procedure (the
            // map's start) lies in our own module, not at its offset in Zax-Launcher.dll, which the recordings of the
            // hooked build hold. A thread timer of our own module stands in for the recorded one there (the game
            // itself sets no thread timer; a system DLL's lies in that DLL) [assumption: the launcher's is the only one]
            const bool same = g_timer[i][1] && module_offset(g_timer[i][3], &base) == rec[3];
#ifdef ZAX_STANDALONE
            const DWORD own_module = (DWORD)(uintptr_t)GetModuleHandleA(nullptr);   // 0x400000 unless built at a second base
            if (g_timer[i][1] && !same && base == own_module)
                log("Replay: the recorded thread timer offset %x at frame %u replayed with our own module's procedure %x", rec[3],
                    g_frame, g_timer[i][3]);
            if (g_timer[i][1] && (same || base == own_module)) {
#else
            if (same) {
#endif
                using TimerProc = void(WINAPI *)(DWORD, DWORD, DWORD, DWORD);
                const DWORD id = g_timer[i][2], proc = g_timer[i][3];
                g_timer[i][1] = 0;
                reinterpret_cast<TimerProc>(static_cast<uintptr_t>(proc))(0, WM_TIMER, id, rec[4]);
                return;
            }
        }
        if (REAL(S_TICK, DWORD(WINAPI *)())() - start >= 5000)
            break;
        Sleep(1);
        drain();
    }
    log("Replay: no real thread timer of procedure offset %x for the recorded one at frame %u", rec[3], g_frame);
}

DWORD WINAPI my_PeekMessageA(DWORD *msg, void *hwnd, DWORD lo, DWORD hi, DWORD flags)
{
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    if (GetCurrentThreadId() == g_main_thread && g_frame_log)
        hook_flip();
    if (!main_thread() || hwnd || lo || hi || flags != 1)
        return REAL(S_PEEK, PeekFn)(msg, hwnd, lo, hi, flags);
    if (g_mode == REPLAY) {
        drain();
        deliver_sent();
        DWORD rec[7], r;
        if (take(E_PEEK, &r, rec, sizeof rec)) {
            if (!r)
                return 0;
            memcpy(msg, rec, sizeof rec);
            msg[0] = rec[0] == 1 ? rd(glob_addr(G_WINDOW)) : 0;   // another window's message (2) goes nowhere
            if (rec[1] == WM_TIMER && rec[0] == 3) {
                msg[3] = 0;
                replay_thread_timer(rec);
            }
            return r;
        }
        return REAL(S_PEEK, PeekFn)(msg, hwnd, lo, hi, flags);
    }
    const DWORD r = real_peek(msg);
    if (g_mode == RECORD) {
        if (r) {
            DWORD rec[7], base;
            memcpy(rec, msg, sizeof rec);
            rec[0] = !msg[0] ? 0 : msg[0] == rd(glob_addr(G_WINDOW)) ? 1 : 2;
            if (msg[1] == WM_TIMER && !msg[0] && msg[3]) {
                rec[0] = 3;
                rec[3] = module_offset(msg[3], &base);
            }
            put(E_PEEK, r, rec, sizeof rec);
        } else
            put(E_PEEK, 0, nullptr, 0);
    }
    return r;
}

// ---- Focus, surface loss and the clipboard (version 8, batch 6-16a) -------------------------------------------------
// What the main thread gets from Windows when the game loses and regains the focus (Alt+Tab) and when it pastes:
// - The window's messages (E_SENT, E_DEFPROC): Windows delivers a message another thread or the system sends
//   (WM_ACTIVATEAPP, WM_ACTIVATE, WM_SIZE, WM_MOVE, WM_SETCURSOR …) inside the main thread's PeekMessageA, straight to the
//   window procedure; the game's (0x599d50) sets its active and minimized flags from them and tells the application
//   whether it runs, which changes how the main loop waits (PumpMessages 0x5998f0: Sleep(100) and IsIconic while
//   inactive) and stops the frames. The class's window procedure is ours (RegisterClassA in the import table hands
//   Windows a wrapper that calls the game's). Recorded: every message that reaches it inside the real PeekMessageA or
//   inside DispatchMessageA of a posted message to the game's window (the dispatched one itself too: DirectDraw's
//   window procedure in front of the game's passes WM_ACTIVATEAPP on or not by its own state), at any depth, before the
//   game's procedure runs; and the answers of the calls in which Windows sends such messages back to it
//   (DefWindowProcA, MoveWindow, BeginPaint, EndPaint and DispatchMessageA itself; E_DEFPROC with the import slot) after
//   them. On replay the replay's window is in another state (never minimized, active), so Windows' own reactions differ:
//   DispatchMessageA and DefWindowProcA are not called, MoveWindow and the paint calls run live; every message Windows
//   sends meanwhile, and those of the real queue, get DefWindowProcA instead of the game; and the recorded messages
//   are given to the game's procedure where they came (inside the replay's PeekMessageA before its recorded answer,
//   inside those calls before their recorded answer). What the procedure asks Windows otherwise (IsIconic, the clocks)
//   is recorded in order as usual. Messages DirectDraw or CreateWindowExA send inside the game's other calls (not
//   inside PeekMessageA or DispatchMessageA) stay live: they follow from the call, the same on replay.
// - DirectDraw's surface loss (E_DDRAW): while another application has the screen, the surfaces are lost; the game's
//   Lock (0x567bb0) asks IsLost (slot 24) of the primary surface and the back buffer every frame and restores a lost
//   one (Restore, slot 27), retries a failed Lock (slot 25); a failed Flip (slot 11) does not advance the page. Only the
//   answers that are not DD_OK are recorded (and every Restore): IsLost, Lock and Flip answer DD_OK on replay unless the
//   recording has their failure next, IsLost without asking DirectDraw (the replay's surfaces are never lost), Lock
//   and Flip calling DirectDraw (a recorded failure is answered without the call, as DirectDraw failed it). Patched in
//   the surfaces' vtable with Flip (hook_flip). The frame log's own Lock of the back buffer passes (g_dd_own).
// - The clipboard (E_CLIP): GetClipboardData's handle and the data behind it (GlobalSize bytes); on replay a movable
//   global block of the recorded bytes is handed out (GlobalLock, GlobalSize and GlobalUnlock run live on it; the
//   previous one is freed at the next call). OpenClipboard, EmptyClipboard, SetClipboardData and CloseClipboard (the
//   game ignores their answers but CloseClipboard's, which its callers drop) stay live.
// Only with a version 8 recording (or Record=1) and only on the main thread.
WINIMPORT DWORD WINAPI RegisterClassA(const void *wc);
WINIMPORT DWORD WINAPI DefWindowProcA(DWORD hwnd, DWORD msg, DWORD wp, DWORD lp);
WINIMPORT DWORD WINAPI GlobalAlloc(DWORD flags, size_t n);
WINIMPORT void *WINAPI GlobalLock(DWORD h);
WINIMPORT BOOL WINAPI GlobalUnlock(DWORD h);
WINIMPORT size_t WINAPI GlobalSize(DWORD h);
WINIMPORT DWORD WINAPI GlobalFree(DWORD h);

using WndProcFn = DWORD(WINAPI *)(DWORD, DWORD, DWORD, DWORD);
DWORD g_wndproc;     // the game's window procedure (the class's, behind ours)
int g_replaying;     // the main thread inside a replayed delivery
DWORD g_sent_count;

int g_in_dispatch;   // the main thread inside DispatchMessageA (a recorded posted message's own handling)
int g_wnd_depth;     // the game's window procedure running (inside DispatchMessageA: 1 for the dispatched message)

// Recorded: a message that reaches the window procedure inside the real PeekMessageA or inside DispatchMessageA of a
// posted message to the game's window (the dispatched message itself too: DirectDraw's own window procedure in front
// of the game's decides by its state whether it passes WM_ACTIVATEAPP on)
DWORD WINAPI my_WndProc(DWORD hwnd, DWORD msg, DWORD wp, DWORD lp)
{
    const WndProcFn game = reinterpret_cast<WndProcFn>(static_cast<uintptr_t>(g_wndproc));
    if (!g_v8_events || g_mode == LIVE || GetCurrentThreadId() != g_main_thread)
        return game(hwnd, msg, wp, lp);
    const bool sent = g_in_peek || g_in_dispatch;
    if (g_mode == REPLAY && (sent || g_replaying))   // the real queue's, or sent by a live call: not the recording's
        return DefWindowProcA(hwnd, msg, wp, lp);
    if (g_mode == RECORD && sent) {
        const DWORD rec[4] = {hwnd == rd(glob_addr(G_WINDOW)) ? 1u : 2u, wp, lp, 0};
        put(E_SENT, msg, rec, 12);
        g_sent_count++;
    }
    g_wnd_depth++;
    const DWORD r = game(hwnd, msg, wp, lp);
    g_wnd_depth--;
    return r;
}
inline bool recorded_zone() { return g_in_peek || g_in_dispatch; }
inline bool replayed_zone() { return g_replaying; }

DWORD sending_call(int slot, DWORD r_live, bool *took);
// DispatchMessageA of a posted message to the game's window: recorded as a call that sends (the messages that reach the
// game's procedure, then its answer); on replay not called, the recorded deliveries and answer given instead
DWORD WINAPI my_DispatchMessageA(const DWORD *msg)
{
    using F = DWORD(WINAPI *)(const DWORD *);
    // only the game window's (a thread timer's procedure, the launcher's map start, is called by the replay itself)
    if (!g_v8_events || !main_thread() || !msg || msg[0] != rd(glob_addr(G_WINDOW)))
        return REAL(S_DISPATCH, F)(msg);
    bool took;
    if (g_mode == REPLAY) {
        g_replaying++;   // what a live call sends meanwhile is not the recording's
        const DWORD r = sending_call(S_DISPATCH, 0, &took);
        g_replaying--;
        return took ? r : REAL(S_DISPATCH, F)(msg);
    }
    g_in_dispatch++;
    const DWORD r = REAL(S_DISPATCH, F)(msg);
    const DWORD answer = sending_call(S_DISPATCH, r, &took);   // inside the zone: recorded
    g_in_dispatch--;
    return answer;
}

// The recorded sent messages that come next, given to the game's window procedure (the replay's PeekMessageA, and
// the game's DefWindowProcA inside a replayed delivery for the messages it sent in the recording)
void deliver_sent()
{
    while (g_v8_events && g_mode == REPLAY && peek_kind() == E_SENT) {
        DWORD msg, rec[3];
        if (!take(E_SENT, &msg, rec, 12))
            return;
        const DWORD hwnd = rec[0] == 1 ? rd(glob_addr(G_WINDOW)) : 0;
        if (!hwnd || !g_wndproc) {
            log("Replay: a sent message %x to another window (frame %u) not given", msg, g_frame);
            continue;
        }
        g_sent_count++;
        g_replaying++;
        g_wnd_depth++;
        reinterpret_cast<WndProcFn>(static_cast<uintptr_t>(g_wndproc))(hwnd, msg, rec[1], rec[2]);
        g_wnd_depth--;
        g_replaying--;
    }
}

// The window procedure's other calls that send messages (MoveWindow in WM_MOVE/WM_SIZE in full screen, BeginPaint and
// EndPaint in WM_PAINT): on recording their answer after them (E_DEFPROC with the slot), the messages they send recorded
// as they come; on replay, inside a replayed delivery, they run live (the replay's window moved and painted; what they
// send does not reach the game) and the recorded messages and answer are given
DWORD sending_call(int slot, DWORD r_live, bool *took)
{
    *took = false;
    if (g_mode == RECORD) {
        if (recorded_zone()) {
            const BYTE d = (BYTE)slot;
            put(E_DEFPROC, r_live, &d, 1);
        }
        return r_live;
    }
    deliver_sent();
    DWORD r;
    BYTE d = 0;
    if (take(E_DEFPROC, &r, &d, 1)) {
        *took = true;
        return r;
    }
    return r_live;
}
template <int I> DWORD WINAPI my_sending(DWORD a, DWORD b, DWORD c, DWORD d, DWORD e, DWORD f)
{
    using F = DWORD(WINAPI *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    const DWORD r = REAL(I, F)(a, b, c, d, e, f);
    if (!g_v8_events || !main_thread() || (g_mode == REPLAY && !replayed_zone()))
        return r;
    bool took;
    return sending_call(I, r, &took);
}
template <int I> DWORD WINAPI my_sending2(DWORD a, DWORD b)
{
    using F = DWORD(WINAPI *)(DWORD, DWORD);
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    const DWORD r = REAL(I, F)(a, b);
    if (!g_v8_events || !main_thread() || (g_mode == REPLAY && !replayed_zone()))
        return r;
    bool took;
    sending_call(I, r, &took);
    return r;   // BeginPaint's device context and EndPaint's answer: the live ones (the game drops them)
}

// The game's DefWindowProcA inside a recorded delivery (the window procedure's default handling): its answer recorded,
// with the messages it sends (recorded by my_WndProc as they come); on replay those given and the answer returned
// without asking Windows (the replay's window is in another state: not minimized, active)
DWORD WINAPI my_DefWindowProcA(DWORD hwnd, DWORD msg, DWORD wp, DWORD lp)
{
    using F = DWORD(WINAPI *)(DWORD, DWORD, DWORD, DWORD);
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    if (!g_v8_events || !main_thread())
        return REAL(S_DEFPROC, F)(hwnd, msg, wp, lp);
    if (g_mode == REPLAY) {
        if (!replayed_zone())
            return REAL(S_DEFPROC, F)(hwnd, msg, wp, lp);
        deliver_sent();
        DWORD r;
        BYTE d = 0;
        if (take(E_DEFPROC, &r, &d, 1))
            return r;
        return REAL(S_DEFPROC, F)(hwnd, msg, wp, lp);
    }
    const DWORD r = REAL(S_DEFPROC, F)(hwnd, msg, wp, lp);
    if (recorded_zone()) {
        const BYTE d = S_DEFPROC;
        put(E_DEFPROC, r, &d, 1);
    }
    return r;
}

// The game registers its window class (WinMain 0x477940): our procedure in front of the game's
DWORD WINAPI my_RegisterClassA(const DWORD *wc)
{
    using F = DWORD(WINAPI *)(const DWORD *);
    if (!g_v8_events || g_mode == LIVE || !wc || !wc[1] || g_wndproc)
        return REAL(S_REGCLASS, F)(wc);
    DWORD copy[10];   // WNDCLASSA: style, lpfnWndProc, …
    memcpy(copy, wc, sizeof copy);
    g_wndproc = wc[1];
    copy[1] = (DWORD)(uintptr_t)ptr(&my_WndProc);
    log("Replay: the window procedure %x behind ours (sent messages %s)", g_wndproc, g_mode == RECORD ? "recorded" : "replayed");
    return REAL(S_REGCLASS, F)(copy);
}

DWORD g_clip_handle;   // the replay's last block
DWORD WINAPI my_GetClipboardData(DWORD format)
{
    using F = DWORD(WINAPI *)(DWORD);
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    if (!g_v8_events || !main_thread())
        return REAL(S_GETCLIP, F)(format);
    static BYTE buf[MAX_EVENT];
    if (g_mode == REPLAY) {
        DWORD r, n;
        if (!take(E_CLIP, &r, buf, MAX_DATA, &n))
            return REAL(S_GETCLIP, F)(format);
        if (g_clip_handle)
            GlobalFree(g_clip_handle);
        g_clip_handle = 0;
        DWORD size = 0;
        if (n >= 8)
            memcpy(&size, buf + 4, 4);
        if (!r || n < 8 || size > n - 8)
            return 0;
        g_clip_handle = GlobalAlloc(2 /* GMEM_MOVEABLE */, size);
        if (void *p = g_clip_handle ? GlobalLock(g_clip_handle) : nullptr) {
            memcpy(p, buf + 8, size);
            GlobalUnlock(g_clip_handle);
        }
        if (g_clip_handle && GlobalSize(g_clip_handle) != size)
            log("Replay: the clipboard's block has %u bytes, the recording's %u", (DWORD)GlobalSize(g_clip_handle), size);
        return g_clip_handle;
    }
    const DWORD h = REAL(S_GETCLIP, F)(format);
    DWORD size = 0, n = 8;
    memcpy(buf, &format, 4);
    if (h) {
        size = (DWORD)GlobalSize(h);
        if (const BYTE *p = static_cast<const BYTE *>(GlobalLock(h))) {
            const DWORD k = size < MAX_DATA - 8 ? size : MAX_DATA - 8;
            memcpy(buf + 8, p, k);
            n += k;
            GlobalUnlock(h);
        }
        if (n - 8 < size)
            log("Replay: the clipboard's %u bytes recorded as %u (frame %u)", size, n - 8, g_frame);
    }
    memcpy(buf + 4, &size, 4);
    if (g_mode == RECORD)
        put(E_CLIP, h != 0, buf, n);
    return h;
}

// The surfaces' methods (patched by hook_flip): 24 IsLost, 25 Lock, 27 Restore; Flip (11) is my_Flip
bool g_dd_own;   // the frame log's own call
struct DdVt {
    DWORD vt, islost, lock, restore;
};
DdVt g_ddvt[8];
const DdVt *dd_vt(DWORD self)
{
    const DWORD vt = rd(self);
    for (const DdVt &v : g_ddvt)
        if (v.vt == vt)
            return &v;
    return &g_ddvt[0];
}
BYTE dd_surface(DWORD self) { return self == rd(glob_addr(G_PRIMARY)) ? 0 : self == rd(glob_addr(0x713338)) ? 1 : 2; }
inline bool dd_ours() { return g_v8_events && main_thread() && !g_dd_own; }
// A recorded failure of this slot and surface next: taken, its answer in *r
bool dd_take(BYTE slot, BYTE surface, DWORD *r)
{
    const BYTE *e = peek_event();
    if (!e || e[0] != E_DDRAW || (e[1] | e[2] << 8) != 2 || e[7] != slot || e[8] != surface)
        return false;
    BYTE d[2];
    return take(E_DDRAW, r, d, 2);
}
void dd_put(BYTE slot, BYTE surface, DWORD r)
{
    const BYTE d[2] = {slot, surface};
    put(E_DDRAW, r, d, 2);
}
long WINAPI my_IsLost(DWORD self)
{
    using F = long(WINAPI *)(DWORD);
    const F real = reinterpret_cast<F>(static_cast<uintptr_t>(dd_vt(self)->islost));
    if (!dd_ours())
        return real(self);
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    const BYTE s = dd_surface(self);
    DWORD r = 0;
    if (g_mode == REPLAY)
        return dd_take(24, s, &r) ? (long)r : 0;
    r = (DWORD)real(self);
    if (r)
        dd_put(24, s, r);
    return (long)r;
}
long WINAPI my_Restore(DWORD self)
{
    using F = long(WINAPI *)(DWORD);
    const F real = reinterpret_cast<F>(static_cast<uintptr_t>(dd_vt(self)->restore));
    if (!dd_ours())
        return real(self);
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    const BYTE s = dd_surface(self);
    DWORD r;
    if (g_mode == REPLAY) {
        if (!dd_take(27, s, &r))
            go_live("DIVERGED: the game restores a surface the recording did not");
        return g_mode == REPLAY ? (long)r : real(self);
    }
    r = (DWORD)real(self);
    static DWORD last_restore = 1;
    if (r != last_restore)
        log("Replay: Restore of surface %u at frame %u: %x (the same until another answer)", s, g_frame, r);
    last_restore = r;
    dd_put(27, s, r);
    return (long)r;
}
long WINAPI my_Lock(DWORD self, void *rect, void *desc, DWORD flags, void *event)
{
    using F = long(WINAPI *)(DWORD, void *, void *, DWORD, void *);
    const F real = reinterpret_cast<F>(static_cast<uintptr_t>(dd_vt(self)->lock));
    if (!dd_ours())
        return real(self, rect, desc, flags, event);
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    const BYTE s = dd_surface(self);
    DWORD r;
    if (g_mode == REPLAY && dd_take(25, s, &r))
        return (long)r;
    r = (DWORD)real(self, rect, desc, flags, event);
    if (g_mode == RECORD && r)
        dd_put(25, s, r);
    return (long)r;
}
// The three slots of a known vtable patched again when DirectDraw wrote them back (rehook_surfaces); true if it did
bool repatch_loss(DdVt &v)
{
    DWORD *slots = at<DWORD *>(v.vt);
    const DWORD mine[3] = {(DWORD)(uintptr_t)ptr(&my_IsLost), (DWORD)(uintptr_t)ptr(&my_Lock), (DWORD)(uintptr_t)ptr(&my_Restore)};
    if (slots[24] == mine[0] && slots[25] == mine[1] && slots[27] == mine[2])
        return false;
    DWORD old;
    if (!VirtualProtect(slots + 24, 4 * 4, PAGE_EXECUTE_READWRITE, &old))
        return false;
    if (slots[24] != mine[0])   // what is there now is what the call goes on to
        v.islost = slots[24];
    if (slots[25] != mine[1])
        v.lock = slots[25];
    if (slots[27] != mine[2])
        v.restore = slots[27];
    slots[24] = (DWORD)(uintptr_t)ptr(&my_IsLost);
    slots[25] = (DWORD)(uintptr_t)ptr(&my_Lock);
    slots[27] = (DWORD)(uintptr_t)ptr(&my_Restore);
    VirtualProtect(slots + 24, 4 * 4, old, &old);
    return true;
}
void hook_surfaces(DWORD surface)
{
    if (!surface)
        return;
    const DWORD vt = rd(surface);
    for (DdVt &v : g_ddvt) {
        if (v.vt == vt) {
            repatch_loss(v);
            return;
        }
        if (v.vt)
            continue;
        DWORD *slots = at<DWORD *>(vt);
        DWORD old;
        if (!VirtualProtect(slots + 24, 4 * 4, PAGE_EXECUTE_READWRITE, &old)) {
            log("Replay: the surface's vtable cannot be written, its loss not recorded");
            return;
        }
        v.islost = slots[24];
        v.lock = slots[25];
        v.restore = slots[27];
        v.vt = vt;
        slots[24] = (DWORD)(uintptr_t)ptr(&my_IsLost);
        slots[25] = (DWORD)(uintptr_t)ptr(&my_Lock);
        slots[27] = (DWORD)(uintptr_t)ptr(&my_Restore);
        VirtualProtect(slots + 24, 4 * 4, old, &old);
        log("Replay: the surfaces' loss %s (vtable %x)", g_mode == RECORD ? "recorded" : "replayed", vt);
        return;
    }
}

// ---- The network: DirectPlay 8 and Winsock (version 7, batch 6-14) ------------------------------------------------
// What enters one side of a LAN game from the network, recorded with Record=1 and given back with Replay=1, so each
// side replays alone (no network, no second machine) and what it sends is compared (docs/DECOMPILATION.md 13,
// "Batch 6-14"). The game side of DirectPlay is dplay.cpp / dplay_rest5a.cpp: three COM objects (server, client,
// address; CoCreateInstance), a message handler that DirectPlay calls on its own threads (registered by Initialize,
// slot 3), which queues receives under the queue lock and marks player slots under the slot lock (the terminate text
// under its own), and the main thread that reads them by entering those locks (ProcessMessages, ProcessSlots, Frame).
// - The callbacks (E_DP_CB): the handler's wrapper records the message (its structure, a receive's data, a terminate
//   text) and on DirectPlay's threads which of the three locks the handler took and how often the main thread had
//   entered that lock by then. The record goes into the recording at the main thread's next entry of that lock (the
//   first one that can see it; the main thread waits there for a callback that has left the lock but not returned),
//   a callback that took no lock at the next entry of any of them or at the end of the main thread's next DirectPlay
//   call (an enumeration's answers are read after it), and one DirectPlay makes on the main thread itself inside a
//   call (the local player's create and destroy) where it happens. The scheduling is not changed: the callbacks run
//   when DirectPlay calls them, as without the recording.
// - The main thread's entries of the three locks (E_DP_LOCK, EnterCriticalSection in the import table): the replay
//   delivers the callbacks recorded before an entry before it enters, on a thread of its own for those of DirectPlay's
//   threads (their heap blocks go to the fixed heap's other part, as on DirectPlay's threads) and on the main thread for
//   the main thread's.
// - The synchronous calls (E_DP_CALL, E_COCREATE): every method the game calls on the main thread (the vtable slots of
//   dplay_rest5a.cpp's list, patched in DirectPlay's vtables on recording) with its result and outputs (sizes, buffers
//   such as GetClientInfo's player info, handles, connection info, URLs, socket addresses, the objects it hands out);
//   on replay fake objects answer them from the recording.
// - Winsock (E_WS): the GameSpy SDK's calls of wsock32 on the main thread (the host's query and reporting socket, the
//   master server's address), with their outputs (received datagrams and their senders, select's sets, the host entry);
//   the pure ones (byte order, inet_addr, inet_ntoa, __WSAFDIsSet) stay live.
// What the side sends to DirectPlay and Winsock (SendTo/Send payloads, player and session info, the URLs and components
// of addresses, sendto's datagrams, the handler's answers to DirectPlay: its return value and the player context it
// sets) is logged as "Net <n> frame <f>: …" lines with "NetData <n>" hex lines, in the main thread's order, the same on
// recording and on replay; zax-testtools/tools/replay_compare.py --net compares them.
// Only with a version 7 recording (or Record=1) and only on the main thread; other threads and other recordings go live.

WINIMPORT HANDLE WINAPI GetProcessHeap(void);
WINIMPORT void *WINAPI HeapAlloc(HANDLE heap, DWORD flags, size_t n);
WINIMPORT BOOL WINAPI HeapFree(HANDLE heap, DWORD flags, void *p);
WINIMPORT HANDLE WINAPI CreateEventA(void *sec, BOOL manual, BOOL initial, const char *name);
WINIMPORT BOOL WINAPI SetEvent(HANDLE event);
WINIMPORT DWORD WINAPI WaitForSingleObject(HANDLE h, DWORD ms);
WINIMPORT void WINAPI ExitProcess(DWORD code);

// The process heap, not the C runtime's (which the fixed heap serves): the network's records and the replay's buffers
void *net_alloc(DWORD n) { return HeapAlloc(GetProcessHeap(), 8 /* HEAP_ZERO_MEMORY */, n); }
void net_free(void *p)
{
    if (p)
        HeapFree(GetProcessHeap(), 0, p);
}

struct NetSpin {
    static volatile long lock;
    NetSpin()
    {
        while (__atomic_exchange_n(&lock, 1, __ATOMIC_ACQUIRE))
            Sleep(0);
    }
    ~NetSpin() { __atomic_store_n(&lock, 0, __ATOMIC_RELEASE); }
};
volatile long NetSpin::lock;

// Bytes for an event's data / read back from one
struct Out {
    BYTE *p;
    DWORD n, cap;
    void bytes(const void *s, DWORD k)
    {
        if (n + k > cap)
            k = cap > n ? cap - n : 0;
        if (k && s)
            memcpy(p + n, s, k);
        else if (k)
            memset(p + n, 0, k);
        n += k;
    }
    void dw(DWORD v) { bytes(&v, 4); }
    void b(BYTE v) { bytes(&v, 1); }
};
struct In {
    const BYTE *p;
    DWORD n, pos;
    DWORD dw()
    {
        DWORD v = 0;
        if (pos + 4 <= n)
            memcpy(&v, p + pos, 4);
        pos += 4;
        return v;
    }
    BYTE b() { return pos < n ? p[pos++] : (pos++, 0); }
    const BYTE *bytes(DWORD k)
    {
        const BYTE *q = pos + k <= n ? p + pos : nullptr;
        pos += k;
        return q;
    }
};
DWORD wide_len(const WORD *w)
{
    DWORD n = 0;
    if (w)
        while (n < 0x400 && w[n])
            n++;
    return n;
}

// "Net" lines: what the side sends, on the main thread in its order
DWORD g_net_seq;
void net_line(const char *text, const void *data, DWORD n)
{
    log("Net %u frame %u: %s", g_net_seq, g_frame, text);
    if (n) {
        char prefix[24];
        wsprintfA(prefix, "NetData %u", g_net_seq);
        hex_lines(prefix, static_cast<const BYTE *>(data), n);
    }
    g_net_seq++;
}

// The game's three locks of the DirectPlay net game (dplay.h): 1 the slots (DPLAY_LOCK), 2 the received messages
// (DPLAY_QUEUE_LOCK), 3 the terminate text (DPLAY_TERMINATE_LOCK)
int lock_of(const void *cs)
{
    const DWORD a = (DWORD)(uintptr_t)cs;
    if (a == glob_addr(0x6bdbf0))
        return 1;
    if (a == glob_addr(0x6bdc08))
        return 2;
    if (a == glob_addr(0x6bdad0))
        return 3;
    return 0;
}

// The DirectPlay objects' kinds and the methods the game calls (stdcall, this and n dword arguments; the slot numbers
// and arguments from dplay_rest5a.cpp's list, the names after the DirectPlay 8 headers [assumption, as there])
enum { K_SERVER, K_CLIENT, K_ADDRESS, K_IP, NKIND_DP };
constexpr int NSLOTS_DP = 40;
const char *const KIND_DP[] = {"server", "client", "address", "address IP"};
struct DpMethod {
    BYTE kind, slot, args;
    const char *name;
};
const DpMethod DP_METHODS[] = {
    {K_SERVER, 0, 2, "QueryInterface"}, {K_SERVER, 1, 0, "AddRef"}, {K_SERVER, 2, 0, "Release"},
    {K_SERVER, 3, 3, "Initialize"}, {K_SERVER, 4, 6, "EnumServiceProviders"}, {K_SERVER, 5, 2, "CancelAsyncOperation"},
    {K_SERVER, 8, 4, "SetServerInfo"}, {K_SERVER, 9, 4, "GetClientInfo"}, {K_SERVER, 10, 3, "GetClientAddress"},
    {K_SERVER, 11, 3, "GetLocalHostAddresses"}, {K_SERVER, 13, 7, "Host"}, {K_SERVER, 14, 7, "SendTo"},
    {K_SERVER, 23, 1, "Close"}, {K_SERVER, 24, 4, "DestroyClient"}, {K_SERVER, 25, 2, "ReturnBuffer"},
    {K_SERVER, 32, 3, "GetConnectionInfo"},
    {K_CLIENT, 0, 2, "QueryInterface"}, {K_CLIENT, 1, 0, "AddRef"}, {K_CLIENT, 2, 0, "Release"},
    {K_CLIENT, 3, 3, "Initialize"}, {K_CLIENT, 4, 6, "EnumServiceProviders"}, {K_CLIENT, 5, 11, "EnumHosts"},
    {K_CLIENT, 6, 2, "CancelAsyncOperation"}, {K_CLIENT, 7, 10, "Connect"}, {K_CLIENT, 8, 6, "Send"},
    {K_CLIENT, 11, 4, "SetClientInfo"}, {K_CLIENT, 14, 1, "Close"}, {K_CLIENT, 15, 2, "ReturnBuffer"},
    {K_CLIENT, 20, 2, "GetConnectionInfo"},
    {K_ADDRESS, 0, 2, "QueryInterface"}, {K_ADDRESS, 1, 0, "AddRef"}, {K_ADDRESS, 2, 0, "Release"},
    {K_ADDRESS, 4, 1, "BuildFromURLA"}, {K_ADDRESS, 10, 2, "GetURLA"}, {K_ADDRESS, 13, 1, "SetSP"},
    {K_ADDRESS, 18, 4, "AddComponent"},
    {K_IP, 0, 2, "QueryInterface"}, {K_IP, 1, 0, "AddRef"}, {K_IP, 2, 0, "Release"}, {K_IP, 6, 2, "GetSockAddress"},
};
constexpr int NMETHODS = sizeof DP_METHODS / sizeof DP_METHODS[0];
const char *dp_name(int k, int s)
{
    for (const DpMethod &m : DP_METHODS)
        if (m.kind == k && m.slot == s)
            return m.name;
    return "?";
}

constexpr DWORD CLSID_SERVER = 0x606190, CLSID_CLIENT = 0x6061a0, CLSID_ADDRESS = 0x6061d0, IID_ADDRESS_IP = 0x6061b0;
bool same_guid(const void *a, DWORD exe_guid) { return a && !memcmp(a, reinterpret_cast<const void *>(static_cast<uintptr_t>(glob_addr_dyn(exe_guid))), 16); }

// ---- Recording: DirectPlay's vtables patched -------------------------------------------------------------------------
struct PatchedVt {
    DWORD vt;
    int kind;
    DWORD real[NSLOTS_DP];
};
PatchedVt g_patched[16];
volatile long g_npatched;
int g_dp_depth;   // the main thread inside a recorded call (DirectPlay's own calls of its objects are not the game's)
bool g_in_cb_main;   // the main thread runs the handler inside a DirectPlay call
DWORD g_dp_handler;   // the game's message handler (0x47ead0, Initialize's argument)
using HandlerFn = DWORD(WINAPI *)(void *, DWORD, BYTE *);

DWORD dp_real(void *self, int slot)
{
    const DWORD vt = *static_cast<DWORD *>(self);
    for (long i = 0; i < g_npatched; i++)
        if (g_patched[i].vt == vt)
            return g_patched[i].real[slot];
    return 0;
}
bool dp_outer()
{
    if (!g_v7_events || g_mode != RECORD || GetCurrentThreadId() != g_main_thread || g_dp_depth)
        return false;
    g_dp_depth = 1;
    return true;
}
void dp_record_call(int k, int s, void *self, const DWORD *args, DWORD r);
DWORD dp_fake(int k, int s, void *self, const DWORD *args);
DWORD dp_init_rec(int k, void *self, const DWORD *args);

template <int K, int S, typename... A> struct Dp {
    static DWORD WINAPI rec(void *self, A... a)
    {
        using F = DWORD(WINAPI *)(void *, A...);
        const DWORD args[sizeof...(A) + 1] = {(DWORD)a..., 0};
        if constexpr (S == 3 && K <= K_CLIENT)
            return dp_init_rec(K, self, args);
        else {
            const F real = reinterpret_cast<F>(static_cast<uintptr_t>(dp_real(self, S)));
            if (!dp_outer())
                return real(self, a...);
            const DWORD r = real(self, a...);
            dp_record_call(K, S, self, args, r);
            return r;
        }
    }
    static DWORD WINAPI fake(void *self, A... a)
    {
        const DWORD args[sizeof...(A) + 1] = {(DWORD)a..., 0};
        return dp_fake(K, S, self, args);
    }
};
template <int K, int S, int N, typename... A> struct DpN : DpN<K, S, N - 1, DWORD, A...> {};
template <int K, int S, typename... A> struct DpN<K, S, 0, A...> {
    using T = Dp<K, S, A...>;
};
struct DpEntry {
    void *rec, *fake;
};
DpEntry g_dp_entry[NMETHODS];
#define DPE(K, S, N) {ptr(&DpN<K, S, N>::T::rec), ptr(&DpN<K, S, N>::T::fake)}
void fill_dp_entries()
{
    const DpEntry e[NMETHODS] = {
        DPE(0, 0, 2), DPE(0, 1, 0), DPE(0, 2, 0), DPE(0, 3, 3), DPE(0, 4, 6), DPE(0, 5, 2), DPE(0, 8, 4), DPE(0, 9, 4),
        DPE(0, 10, 3), DPE(0, 11, 3), DPE(0, 13, 7), DPE(0, 14, 7), DPE(0, 23, 1), DPE(0, 24, 4), DPE(0, 25, 2), DPE(0, 32, 3),
        DPE(1, 0, 2), DPE(1, 1, 0), DPE(1, 2, 0), DPE(1, 3, 3), DPE(1, 4, 6), DPE(1, 5, 11), DPE(1, 6, 2), DPE(1, 7, 10),
        DPE(1, 8, 6), DPE(1, 11, 4), DPE(1, 14, 1), DPE(1, 15, 2), DPE(1, 20, 2),
        DPE(2, 0, 2), DPE(2, 1, 0), DPE(2, 2, 0), DPE(2, 4, 1), DPE(2, 10, 2), DPE(2, 13, 1), DPE(2, 18, 4),
        DPE(3, 0, 2), DPE(3, 1, 0), DPE(3, 2, 0), DPE(3, 6, 2),
    };
    for (int i = 0; i < NMETHODS; i++)
        g_dp_entry[i] = e[i];
}

// An object DirectPlay handed out: its vtable's slots of the list to the recording wrappers (once per vtable)
void dp_patch(void *obj, int kind)
{
    if (!obj)
        return;
    const DWORD vt = *static_cast<DWORD *>(obj);
    for (long i = 0; i < g_npatched; i++)
        if (g_patched[i].vt == vt) {
            if (g_patched[i].kind != kind)
                log("Replay: a DirectPlay vtable %x of a %s is also a %s's, its calls recorded as the first", vt,
                    KIND_DP[kind], KIND_DP[g_patched[i].kind]);
            return;
        }
    if (g_npatched >= 16) {
        log("Replay: too many DirectPlay vtables, a %s's calls not recorded", KIND_DP[kind]);
        return;
    }
    PatchedVt &p = g_patched[g_npatched];
    p.vt = vt;
    p.kind = kind;
    DWORD *slots = reinterpret_cast<DWORD *>(static_cast<uintptr_t>(vt));
    for (int s = 0; s < NSLOTS_DP; s++)
        p.real[s] = 0;
    DWORD old;
    if (!VirtualProtect(slots, NSLOTS_DP * 4, PAGE_EXECUTE_READWRITE, &old)) {
        log("Replay: a DirectPlay %s's vtable cannot be written, its calls not recorded", KIND_DP[kind]);
        return;
    }
    for (int i = 0; i < NMETHODS; i++)
        if (DP_METHODS[i].kind == kind)
            p.real[DP_METHODS[i].slot] = slots[DP_METHODS[i].slot];
    __atomic_store_n(&g_npatched, g_npatched + 1, __ATOMIC_RELEASE);
    for (int i = 0; i < NMETHODS; i++)
        if (DP_METHODS[i].kind == kind)
            slots[DP_METHODS[i].slot] = (DWORD)(uintptr_t)g_dp_entry[i].rec;
    VirtualProtect(slots, NSLOTS_DP * 4, old, &old);
    log("Replay: DirectPlay %s vtable %x recorded", KIND_DP[kind], vt);
}

// ---- Recording: the callbacks -----------------------------------------------------------------------------------------
// A callback on DirectPlay's threads, until the main thread writes it into the recording. data: flags (1 the main
// thread, 2 a null data pointer), the lock it took (0 none), the structure's size (word), the structure, the data's
// length (dword) and the data (a receive's buffer, a terminate's text).
struct CbRec {
    DWORD seq;
    DWORD id;
    volatile long done;
    long lock;   // 1..3, set while the callback holds it
    DWORD tag;   // the main thread's entries of that lock before it
    DWORD ret, out;
    DWORD len;
    BYTE data[4];
};
constexpr int NPEND = 1024, NCTX = 32;
CbRec *g_pend[NPEND];
DWORD g_npend;
volatile long g_cb_seq;
volatile DWORD g_ctx_thread[NCTX];
CbRec *g_ctx_rec[NCTX];
DWORD g_main_entries[4];   // the main thread's entries of each lock so far
DWORD g_cb_dropped;

DWORD cb_out(DWORD id, const BYTE *msg)
{
    if (!msg)
        return 0;
    if (id == 0xffff0007)   // create player: the context (the slot index) it sets
        return reinterpret_cast<const DWORD *>(msg)[2];
    if (id == 0xffff000e)   // indicate connect: the player context
        return reinterpret_cast<const DWORD *>(msg)[6];
    return 0;
}
void cb_payload(DWORD id, const BYTE *msg, const BYTE **p, DWORD *n)
{
    *p = nullptr;
    *n = 0;
    if (!msg)
        return;
    const DWORD *m = reinterpret_cast<const DWORD *>(msg);
    if (id == 0xffff0011) {   // receive: pReceiveData, dwReceiveDataSize
        *p = reinterpret_cast<const BYTE *>(static_cast<uintptr_t>(m[3]));
        *n = m[4];
    } else if (id == 0xffff0016) {   // terminate session: pvTerminateData, dwTerminateDataSize
        *p = reinterpret_cast<const BYTE *>(static_cast<uintptr_t>(m[2]));
        *n = m[3];
    }
    if (*n > MAX_DATA - 0x200)
        *n = MAX_DATA - 0x200;
}
// An enumeration's answer (version 8): its application description (DPN_APPLICATION_DESC, by its dwSize, at most 0x48
// bytes) and the session name it points to at +0x30 (wide, its length in characters first, 0 for none); the message
// handler reads only that name (and keeps both address objects) [confirmed: 0x47ff50, dplay.cpp]
constexpr DWORD ENUM_DESC = 0x48, ENUM_NAME = 0x400;
DWORD enum_payload(const BYTE *msg, BYTE *out)
{
    const DWORD *m = reinterpret_cast<const DWORD *>(msg);
    const BYTE *desc = reinterpret_cast<const BYTE *>(static_cast<uintptr_t>(m[3]));
    DWORD n = desc ? *reinterpret_cast<const DWORD *>(desc) : 0;
    if (n > ENUM_DESC)
        n = ENUM_DESC;
    memset(out, 0, ENUM_DESC);
    if (n)
        memcpy(out, desc, n);
    const WORD *name = n >= 0x34 ? *reinterpret_cast<const WORD *const *>(desc + 0x30) : nullptr;
    DWORD chars = name ? wide_len(name) + 1 : 0;
    if (chars > ENUM_NAME)
        chars = ENUM_NAME;
    memcpy(out + ENUM_DESC, &chars, 4);
    if (chars)
        memcpy(out + ENUM_DESC + 4, name, chars * 2);
    return ENUM_DESC + 4 + chars * 2;
}
CbRec *cb_new(DWORD id, const BYTE *msg, bool main)
{
    const DWORD size = msg ? (*reinterpret_cast<const DWORD *>(msg) < 0x100 ? *reinterpret_cast<const DWORD *>(msg) : 0x100) : 0;
    const BYTE *payload;
    DWORD plen;
    cb_payload(id, msg, &payload, &plen);
    BYTE enum_buf[ENUM_DESC + 4 + ENUM_NAME * 2];
    if (id == 0xffff000b && msg && g_v8_events) {
        plen = enum_payload(msg, enum_buf);
        payload = enum_buf;
    }
    CbRec *r = static_cast<CbRec *>(net_alloc(sizeof(CbRec) + 8 + size + plen));
    if (!r)
        return nullptr;
    r->id = id;
    BYTE *d = r->data;
    d[0] = (main ? 1 : 0) | (msg && !payload && (id == 0xffff0011 || id == 0xffff0016) ? 2 : 0);
    d[1] = 0;
    d[2] = (BYTE)size;
    d[3] = (BYTE)(size >> 8);
    if (size)
        memcpy(d + 4, msg, size);
    memcpy(d + 4 + size, &plen, 4);
    if (plen && payload)
        memcpy(d + 8 + size, payload, plen);
    r->len = 8 + size + plen;
    return r;
}
// Into the recording (the main thread), and its answers as a Net line
void cb_write(CbRec *r)
{
    r->data[1] = (BYTE)r->lock;
    put(E_DP_CB, r->id, r->data, r->len);
    char text[96];
    wsprintfA(text, "callback %x (%s) returned %x out %x", r->id, r->data[0] & 1 ? "main" : "other", r->ret, r->out);
    net_line(text, nullptr, 0);
    net_free(r);
}

DWORD WINAPI dp_callback_rec(void *context, DWORD id, BYTE *msg)
{
    const HandlerFn handler = reinterpret_cast<HandlerFn>(static_cast<uintptr_t>(g_dp_handler));
    if (g_mode != RECORD || !g_v7_events)
        return handler(context, id, msg);
    const bool main = GetCurrentThreadId() == g_main_thread;
    if (id == 0xffff000b && msg && g_v8_events) {   // the answer's address objects: the game releases them on the main thread
        dp_patch(reinterpret_cast<void *>(static_cast<uintptr_t>(reinterpret_cast<const DWORD *>(msg)[1])), K_ADDRESS);
        dp_patch(reinterpret_cast<void *>(static_cast<uintptr_t>(reinterpret_cast<const DWORD *>(msg)[2])), K_ADDRESS);
    }
    CbRec *r = cb_new(id, msg, main);
    if (!r)
        return handler(context, id, msg);
    if (main) {   // inside one of the main thread's DirectPlay calls: where it happens
        const bool was = g_in_cb_main;
        g_in_cb_main = true;
        r->ret = handler(context, id, msg);
        g_in_cb_main = was;
        r->out = cb_out(id, msg);
        cb_write(r);
        return r->ret;
    }
    int slot = -1;
    {
        NetSpin guard;
        r->seq = (DWORD)__atomic_fetch_add(&g_cb_seq, 1, __ATOMIC_RELAXED);
        if (g_npend < NPEND)
            g_pend[g_npend++] = r;
        else {
            g_cb_dropped++;
            net_free(r);
            r = nullptr;
        }
        for (int i = 0; r && i < NCTX; i++)
            if (!g_ctx_thread[i]) {
                g_ctx_thread[i] = GetCurrentThreadId();
                g_ctx_rec[i] = r;
                slot = i;
                break;
            }
    }
    const DWORD ret = handler(context, id, msg);
    if (r) {
        r->ret = ret;
        r->out = cb_out(id, msg);
        __atomic_store_n(&r->done, 1, __ATOMIC_RELEASE);
    }
    if (slot >= 0) {
        NetSpin guard;
        g_ctx_rec[slot] = nullptr;
        g_ctx_thread[slot] = 0;
    }
    return ret;
}

// The main thread writes the callbacks the next lock entry (lock, its entry number) or call end (lock 0) can see: those
// done that took that lock at that entry number, and those done that took none; in the order they took their locks
void cb_flush(int lock, DWORD entry)
{
    if (lock)
        for (;;) {   // a callback that left the lock but has not returned yet
            bool busy = false;
            {
                NetSpin guard;
                for (DWORD i = 0; i < g_npend; i++)
                    if (!__atomic_load_n(&g_pend[i]->done, __ATOMIC_ACQUIRE) && g_pend[i]->lock == lock && g_pend[i]->tag == entry)
                        busy = true;
            }
            if (!busy)
                break;
            Sleep(0);
        }
    CbRec *ready[NPEND];
    DWORD n = 0;
    {
        NetSpin guard;
        DWORD keep = 0;
        for (DWORD i = 0; i < g_npend; i++) {
            CbRec *r = g_pend[i];
            if (__atomic_load_n(&r->done, __ATOMIC_ACQUIRE) && (!r->lock || (r->lock == lock && r->tag == entry)))
                ready[n++] = r;
            else
                g_pend[keep++] = r;
        }
        g_npend = keep;
    }
    for (DWORD i = 1; i < n; i++)   // by their sequence (few)
        for (DWORD j = i; j > 0 && ready[j - 1]->seq > ready[j]->seq; j--) {
            CbRec *t = ready[j];
            ready[j] = ready[j - 1];
            ready[j - 1] = t;
        }
    for (DWORD i = 0; i < n; i++)
        cb_write(ready[i]);
    if (g_cb_dropped) {
        log("Replay: %u DirectPlay callbacks not recorded (too many waiting)", g_cb_dropped);
        g_cb_dropped = 0;
    }
}

// ---- Replay: fake objects and the callbacks ----------------------------------------------------------------------------
struct Fake {
    DWORD vt;
    DWORD kind;
    long refs;
};
DWORD g_fake_vt[NKIND_DP][NSLOTS_DP];
DWORD g_cb_context;   // Initialize's context and handler on replay
DWORD g_cb_handler;
DWORD g_enum_context;

DWORD WINAPI fake_unknown(void *)
{
    log("Replay: a DirectPlay method the recording does not know was called (frame %u); the process ends", g_frame);
    flush_out();
    ExitProcess(0xdeadu);
    return 0;
}
void fill_fake_vtables()
{
    for (int k = 0; k < NKIND_DP; k++)
        for (int s = 0; s < NSLOTS_DP; s++)
            g_fake_vt[k][s] = (DWORD)(uintptr_t)ptr(&fake_unknown);
    for (int i = 0; i < NMETHODS; i++)
        g_fake_vt[DP_METHODS[i].kind][DP_METHODS[i].slot] = (DWORD)(uintptr_t)g_dp_entry[i].fake;
}
void *fake_new(int kind)
{
    Fake *f = static_cast<Fake *>(net_alloc(sizeof(Fake)));
    if (f) {
        f->vt = (DWORD)(uintptr_t)g_fake_vt[kind];
        f->kind = kind;
        f->refs = 1;
    }
    return f;
}

// A receive's buffer until the game returns it (ReturnBuffer with its handle)
constexpr int NBUF = 4096;
DWORD g_buf_handle[NBUF];
BYTE *g_buf_data[NBUF];
void buf_keep(DWORD handle, BYTE *data)
{
    for (int i = 0; i < NBUF; i++)
        if (!g_buf_data[i]) {
            g_buf_handle[i] = handle;
            g_buf_data[i] = data;
            return;
        }
    log("Replay: too many DirectPlay receive buffers held, one leaked");
}
void buf_return(DWORD handle)
{
    for (int i = 0; i < NBUF; i++)
        if (g_buf_data[i] && g_buf_handle[i] == handle) {
            net_free(g_buf_data[i]);
            g_buf_data[i] = nullptr;
            return;
        }
}

// The thread that runs the callbacks of DirectPlay's threads on replay (the main thread waits for each)
HANDLE g_cb_go, g_cb_done;
volatile DWORD g_cb_job_id, g_cb_job_ret;
BYTE *volatile g_cb_job_msg;
DWORD WINAPI cb_thread(void *)
{
    for (;;) {
        WaitForSingleObject(g_cb_go, 0xffffffffu);
        g_cb_job_ret = reinterpret_cast<HandlerFn>(static_cast<uintptr_t>(g_cb_handler))(
            reinterpret_cast<void *>(static_cast<uintptr_t>(g_cb_context)), g_cb_job_id, g_cb_job_msg);
        SetEvent(g_cb_done);
    }
}
DWORD run_callback(DWORD id, BYTE *msg, bool main)
{
    const HandlerFn handler = reinterpret_cast<HandlerFn>(static_cast<uintptr_t>(g_cb_handler));
    if (main) {
        const bool was = g_in_cb_main;
        g_in_cb_main = true;
        const DWORD r = handler(reinterpret_cast<void *>(static_cast<uintptr_t>(g_cb_context)), id, msg);
        g_in_cb_main = was;
        return r;
    }
    if (!g_cb_go) {
        g_cb_go = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        g_cb_done = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        DWORD tid;
        CloseHandle(CreateThread(nullptr, 0x10000, &cb_thread, nullptr, 0, &tid));
        log("Replay: DirectPlay's callbacks replayed on thread %u", tid);
    }
    g_cb_job_id = id;
    g_cb_job_msg = msg;
    SetEvent(g_cb_go);
    if (WaitForSingleObject(g_cb_done, 30000) != 0) {
        log("Replay: a DirectPlay callback %x did not return in 30 s (frame %u); the process ends", id, g_frame);
        flush_out();
        ExitProcess(0xdeadu);
    }
    return g_cb_job_ret;
}

// The callbacks the recording has next, given to the game's handler as DirectPlay gave them
BYTE g_cb_buf[MAX_EVENT];
void deliver_callbacks()
{
    while (g_mode == REPLAY && peek_kind() == E_DP_CB) {
        DWORD id, n;
        if (!take(E_DP_CB, &id, g_cb_buf, MAX_DATA, &n))
            return;
        In in = {g_cb_buf, n, 0};
        const BYTE flags = in.b(), lock = in.b();
        DWORD size = in.b();
        size |= (DWORD)in.b() << 8;
        static DWORD msg[0x40 + 4];
        memset(msg, 0, sizeof msg);
        if (const BYTE *s = in.bytes(size))
            memcpy(msg, s, size);
        const DWORD plen = in.dw();
        const BYTE *payload = in.bytes(plen);
        (void)lock;
        if (!g_cb_handler) {
            log("Replay: a DirectPlay callback %x before Initialize (frame %u), not given", id, g_frame);
            continue;
        }
        static BYTE enum_desc[ENUM_DESC];
        static WORD enum_name[ENUM_NAME + 1];
        if (id == 0xffff000b) {   // an enumeration's answer (version 8: with its description)
            DWORD chars = 0;
            if (payload && plen >= ENUM_DESC + 4)
                memcpy(&chars, payload + ENUM_DESC, 4);
            if (!g_v8_events || !payload || plen < ENUM_DESC + 4 || chars > ENUM_NAME || plen < ENUM_DESC + 4 + chars * 2) {
                log("Replay: a DirectPlay enumeration answer (frame %u) is not replayed (a version 7 recording holds no description)",
                    g_frame);
                continue;
            }
            memcpy(enum_desc, payload, ENUM_DESC);
            memset(enum_name, 0, sizeof enum_name);
            memcpy(enum_name, payload + ENUM_DESC + 4, chars * 2);
            reinterpret_cast<DWORD *>(enum_desc)[12] = chars ? (DWORD)(uintptr_t)enum_name : 0;
            reinterpret_cast<DWORD *>(enum_desc)[13] = reinterpret_cast<DWORD *>(enum_desc)[14] = 0;   // password, reserved
            reinterpret_cast<DWORD *>(enum_desc)[16] = 0;                                              // app reserved
            msg[1] = msg[1] ? (DWORD)(uintptr_t)fake_new(K_ADDRESS) : 0;   // the host's address
            msg[2] = msg[2] ? (DWORD)(uintptr_t)fake_new(K_ADDRESS) : 0;   // the device's
            msg[3] = msg[3] ? (DWORD)(uintptr_t)enum_desc : 0;
            msg[4] = 0;   // the response data (the handler does not read it)
            msg[6] = g_enum_context;   // EnumHosts' user context (the replay's own)
        }
        static char text_buf[MAX_DATA];
        if (id == 0xffff0011) {
            BYTE *data = static_cast<BYTE *>(net_alloc(plen + 1));
            if (data && payload)
                memcpy(data, payload, plen);
            msg[3] = (flags & 2) ? 0 : (DWORD)(uintptr_t)data;
            if (data)
                buf_keep(msg[5], data);
        } else if (id == 0xffff0016) {
            if (payload)
                memcpy(text_buf, payload, plen);
            text_buf[plen < MAX_DATA ? plen : MAX_DATA - 1] = 0;
            msg[2] = (flags & 2) ? 0 : (DWORD)(uintptr_t)text_buf;
        }
        BYTE *m = size ? reinterpret_cast<BYTE *>(msg) : nullptr;
        const DWORD ret = run_callback(id, m, flags & 1);
        char text[96];
        wsprintfA(text, "callback %x (%s) returned %x out %x", id, flags & 1 ? "main" : "other", ret, cb_out(id, m));
        net_line(text, nullptr, 0);
    }
}

// ---- The calls' outputs and inputs ------------------------------------------------------------------------------------
BYTE g_dp_buf[MAX_EVENT];   // the main thread's
inline DWORD *dptr(DWORD a) { return reinterpret_cast<DWORD *>(static_cast<uintptr_t>(a)); }
inline BYTE *bptr(DWORD a) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(a)); }

// An out handle (a dword through a pointer argument)
void out_dword(Out &o, DWORD p) { o.dw(p ? *dptr(p) : 0); }
void in_dword(In &i, DWORD p)
{
    const DWORD v = i.dw();
    if (p)
        *dptr(p) = v;
}
// A buffer of *size bytes (rebased later where it holds pointers into itself)
void out_sized(Out &o, DWORD buf, DWORD size_ptr, bool ok)
{
    const DWORD size = size_ptr ? *dptr(size_ptr) : 0;
    const DWORD n = buf && ok && size < MAX_DATA - 0x40 ? size : 0;
    o.dw(size);
    o.dw(buf);
    o.dw(n);
    o.bytes(bptr(buf), n);
}
void rebase(DWORD buf, DWORD old_base, DWORD n, DWORD off)
{
    DWORD *p = dptr(buf + off);
    if (*p >= old_base && *p < old_base + n)
        *p = *p - old_base + buf;
}
// returns the bytes copied (0 when none)
DWORD in_sized(In &i, DWORD buf, DWORD size_ptr, DWORD *old_base)
{
    const DWORD size = i.dw();
    *old_base = i.dw();
    const DWORD n = i.dw();
    const BYTE *b = i.bytes(n);
    if (size_ptr)
        *dptr(size_ptr) = size;
    if (buf && n && b)
        memcpy(bptr(buf), b, n);
    return buf && b ? n : 0;
}
int obj_kind_of_iid(int self_kind, DWORD iid) { return same_guid(bptr(iid), IID_ADDRESS_IP) ? K_IP : self_kind; }

// What a call hands back besides its result
void dp_outputs(int k, int s, const DWORD *a, DWORD r, Out &o)
{
    const bool ok = (int)r >= 0;
    switch (k * 100 + s) {
    case K_SERVER * 100 + 0: case K_CLIENT * 100 + 0: case K_ADDRESS * 100 + 0: case K_IP * 100 + 0: {   // QueryInterface
        void *obj = a[1] ? *reinterpret_cast<void **>(a[1]) : nullptr;
        o.b(obj != nullptr);
        if (obj && ok)
            dp_patch(obj, obj_kind_of_iid(k, a[0]));
        break;
    }
    case K_SERVER * 100 + 4: case K_CLIENT * 100 + 4: {   // EnumServiceProviders(sp, app, buffer, *size, *count, flags)
        o.dw(a[4] ? *dptr(a[4]) : 0);
        out_sized(o, a[2], a[3], ok);
        break;
    }
    case K_SERVER * 100 + 8: case K_CLIENT * 100 + 11:   // SetServerInfo / SetClientInfo: the async handle
        out_dword(o, a[2]);
        break;
    case K_SERVER * 100 + 9:   // GetClientInfo(dpnid, info, *size, flags)
        out_sized(o, a[1], a[2], ok);
        break;
    case K_SERVER * 100 + 10: {   // GetClientAddress(dpnid, **address, flags)
        void *obj = a[1] ? *reinterpret_cast<void **>(a[1]) : nullptr;
        o.b(obj != nullptr);
        if (obj && ok)
            dp_patch(obj, K_ADDRESS);
        break;
    }
    case K_SERVER * 100 + 11: {   // GetLocalHostAddresses(list, *count, flags)
        const DWORD count = a[1] ? *dptr(a[1]) : 0, objects = a[0] && ok ? count : 0;
        o.dw(count);
        o.dw(objects);
        for (DWORD i = 0; i < objects && i < 64; i++)
            dp_patch(reinterpret_cast<void *>(static_cast<uintptr_t>(dptr(a[0])[i])), K_ADDRESS);
        break;
    }
    case K_SERVER * 100 + 14: out_dword(o, a[5]); break;   // SendTo: the async handle
    case K_CLIENT * 100 + 8: out_dword(o, a[4]); break;    // Send
    case K_CLIENT * 100 + 7: out_dword(o, a[8]); break;    // Connect
    case K_CLIENT * 100 + 5: out_dword(o, a[9]); break;    // EnumHosts
    case K_SERVER * 100 + 32: case K_CLIENT * 100 + 20: {   // GetConnectionInfo: the structure (its dwSize)
        const DWORD info = k == K_SERVER ? a[1] : a[0];
        const DWORD n = info ? (*dptr(info) < 0x100 ? *dptr(info) : 0x100) : 0;
        o.dw(n);
        o.bytes(bptr(info), n);
        break;
    }
    case K_ADDRESS * 100 + 10: case K_IP * 100 + 6:   // GetURLA / GetSockAddress(buffer, *size)
        out_sized(o, a[0], a[1], ok);
        break;
    default:
        break;
    }
}
void dp_apply(int k, int s, const DWORD *a, In &i)
{
    DWORD base;
    switch (k * 100 + s) {
    case K_SERVER * 100 + 0: case K_CLIENT * 100 + 0: case K_ADDRESS * 100 + 0: case K_IP * 100 + 0:
        if (a[1])
            *reinterpret_cast<void **>(a[1]) = i.b() ? fake_new(obj_kind_of_iid(k, a[0])) : nullptr;
        break;
    case K_SERVER * 100 + 4: case K_CLIENT * 100 + 4: {
        const DWORD count = i.dw();
        if (a[4])
            *dptr(a[4]) = count;
        const DWORD n = in_sized(i, a[2], a[3], &base);
        for (DWORD e = 0; n && e < count && (e + 1) * 0x20 <= n; e++)   // DPN_SERVICE_PROVIDER_INFO: the name at +0x14
            rebase(a[2], base, n, e * 0x20 + 0x14);
        break;
    }
    case K_SERVER * 100 + 8: case K_CLIENT * 100 + 11: in_dword(i, a[2]); break;
    case K_SERVER * 100 + 9: {
        const DWORD n = in_sized(i, a[1], a[2], &base);
        if (n >= 0x10) {   // DPN_PLAYER_INFO: the name at +8, the data at +0xc
            rebase(a[1], base, n, 8);
            rebase(a[1], base, n, 0xc);
        }
        break;
    }
    case K_SERVER * 100 + 10:
        if (a[1])
            *reinterpret_cast<void **>(a[1]) = i.b() ? fake_new(K_ADDRESS) : nullptr;
        break;
    case K_SERVER * 100 + 11: {
        const DWORD count = i.dw(), objects = i.dw();
        if (a[1])
            *dptr(a[1]) = count;
        for (DWORD e = 0; e < objects && e < 64; e++)
            dptr(a[0])[e] = (DWORD)(uintptr_t)fake_new(K_ADDRESS);
        break;
    }
    case K_SERVER * 100 + 14: in_dword(i, a[5]); break;
    case K_CLIENT * 100 + 8: in_dword(i, a[4]); break;
    case K_CLIENT * 100 + 7: in_dword(i, a[8]); break;
    case K_CLIENT * 100 + 5: in_dword(i, a[9]); break;
    case K_SERVER * 100 + 32: case K_CLIENT * 100 + 20: {
        const DWORD info = k == K_SERVER ? a[1] : a[0];
        const DWORD n = i.dw();
        const BYTE *b = i.bytes(n);
        if (info && b)
            memcpy(bptr(info), b, n);
        break;
    }
    case K_ADDRESS * 100 + 10: case K_IP * 100 + 6:
        in_sized(i, a[0], a[1], &base);
        break;
    default:
        break;
    }
}

// What the game hands DirectPlay (the Net line): values and the bytes behind pointers, never the pointers themselves
void wide_bytes(Out &o, DWORD w)
{
    const DWORD n = wide_len(reinterpret_cast<const WORD *>(static_cast<uintptr_t>(w)));
    o.dw(n);
    o.bytes(bptr(w), n * 2);
}
void app_desc_bytes(Out &o, DWORD p)
{
    if (!p) {
        o.dw(0);
        return;
    }
    DWORD d[0x12];
    memcpy(d, dptr(p), sizeof d);
    const DWORD name = d[12], reserved = d[16], reserved_size = d[17];
    d[12] = d[13] = d[14] = d[16] = 0;   // DPN_APPLICATION_DESC's pointers
    o.dw(1);
    o.bytes(d, sizeof d);
    wide_bytes(o, name);
    o.bytes(bptr(reserved), reserved && reserved_size < 0x1000 ? reserved_size : 0);
}
void buffers_bytes(Out &o, DWORD descs, DWORD count)
{
    for (DWORD i = 0; descs && i < count && i < 16; i++) {   // DPN_BUFFER_DESC {size, data}
        const DWORD size = dptr(descs)[i * 2], data = dptr(descs)[i * 2 + 1];
        o.dw(size);
        o.bytes(bptr(data), data && size < MAX_DATA - 0x100 ? size : 0);
    }
}
void player_info_bytes(Out &o, DWORD p)
{
    if (!p)
        return;
    const DWORD *info = dptr(p);   // DPN_PLAYER_INFO {size, info flags, name, data, data size, player flags}
    o.dw(info[0]);
    o.dw(info[1]);
    o.dw(info[5]);
    wide_bytes(o, info[2]);
    o.bytes(bptr(info[3]), info[3] && info[4] < 0x1000 ? info[4] : 0);
}
void dp_inputs(int k, int s, const DWORD *a, char *text, Out &o)
{
    int t = wsprintfA(text, "%s.%s", KIND_DP[k], dp_name(k, s));
    switch (k * 100 + s) {
    case K_SERVER * 100 + 0: case K_CLIENT * 100 + 0: case K_ADDRESS * 100 + 0: case K_IP * 100 + 0:
    case K_ADDRESS * 100 + 13:   // the IID / the provider's GUID
        o.bytes(bptr(a[0]), a[0] ? 16 : 0);
        break;
    case K_SERVER * 100 + 3: case K_CLIENT * 100 + 3: wsprintfA(text + t, " flags %x", a[2]); break;
    case K_SERVER * 100 + 4: case K_CLIENT * 100 + 4:
        wsprintfA(text + t, " buffer %u size %u flags %x", a[2] != 0, a[3] ? *dptr(a[3]) : 0, a[5]);
        break;
    case K_SERVER * 100 + 5: case K_CLIENT * 100 + 6: wsprintfA(text + t, " %x flags %x", a[0], a[1]); break;
    case K_SERVER * 100 + 8: case K_CLIENT * 100 + 11:
        wsprintfA(text + t, " flags %x", a[3]);
        player_info_bytes(o, a[0]);
        break;
    case K_SERVER * 100 + 9: wsprintfA(text + t, " %x buffer %u size %u flags %x", a[0], a[1] != 0, a[2] ? *dptr(a[2]) : 0, a[3]); break;
    case K_SERVER * 100 + 10: case K_SERVER * 100 + 32: wsprintfA(text + t, " %x flags %x", a[0], a[2]); break;
    case K_SERVER * 100 + 11: wsprintfA(text + t, " list %u count %u flags %x", a[0] != 0, a[1] ? *dptr(a[1]) : 0, a[2]); break;
    case K_SERVER * 100 + 13:
        wsprintfA(text + t, " devices %u context %x flags %x", a[2], a[5], a[6]);
        app_desc_bytes(o, a[0]);
        break;
    case K_SERVER * 100 + 14:
        wsprintfA(text + t, " %x buffers %u timeout %u context %x flags %x", a[0], a[2], a[3], a[4], a[6]);
        buffers_bytes(o, a[1], a[2]);
        break;
    case K_SERVER * 100 + 23: case K_CLIENT * 100 + 14: wsprintfA(text + t, " flags %x", a[0]); break;
    case K_SERVER * 100 + 24:
        wsprintfA(text + t, " %x size %u flags %x", a[0], a[2], a[3]);
        o.bytes(bptr(a[1]), a[1] && a[2] < 0x1000 ? a[2] : 0);
        break;
    case K_SERVER * 100 + 25: case K_CLIENT * 100 + 15: wsprintfA(text + t, " %x flags %x", a[0], a[1]); break;
    case K_CLIENT * 100 + 5:
        wsprintfA(text + t, " enum %u retry %u timeout %u flags %x", a[5], a[6], a[7], a[10]);
        app_desc_bytes(o, a[0]);
        o.bytes(bptr(a[3]), a[3] && a[4] < 0x1000 ? a[4] : 0);
        break;
    case K_CLIENT * 100 + 7:
        wsprintfA(text + t, " context %x flags %x", a[7], a[9]);
        app_desc_bytes(o, a[0]);
        o.bytes(bptr(a[5]), a[5] && a[6] < 0x1000 ? a[6] : 0);
        break;
    case K_CLIENT * 100 + 8:
        wsprintfA(text + t, " buffers %u timeout %u context %x flags %x", a[1], a[2], a[3], a[5]);
        buffers_bytes(o, a[0], a[1]);
        break;
    case K_CLIENT * 100 + 20: wsprintfA(text + t, " flags %x", a[1]); break;
    case K_ADDRESS * 100 + 4: {
        const char *url = reinterpret_cast<const char *>(static_cast<uintptr_t>(a[0]));
        o.bytes(url, url ? lstrlenA(url) : 0);
        break;
    }
    case K_ADDRESS * 100 + 10: case K_IP * 100 + 6:
        wsprintfA(text + t, " buffer %u size %u", a[0] != 0, a[1] ? *dptr(a[1]) : 0);
        break;
    case K_ADDRESS * 100 + 18:
        wsprintfA(text + t, " size %u type %u", a[2], a[3]);
        wide_bytes(o, a[0]);
        o.bytes(bptr(a[1]), a[1] && a[2] < 0x1000 ? a[2] : 0);
        break;
    default:
        break;
    }
}

void dp_net_line(int k, int s, const DWORD *a, DWORD r)
{
    static BYTE data[MAX_EVENT];
    char text[256];
    Out o = {data, 0, MAX_DATA};
    dp_inputs(k, s, a, text, o);
    const int t = lstrlenA(text);
    wsprintfA(text + t, " = %x", r);
    net_line(text, data, o.n);
}

// Recording: after the main thread's call (the callbacks it saw written first)
void dp_record_call(int k, int s, void *self, const DWORD *args, DWORD r)
{
    (void)self;
    g_dp_depth = 0;
    cb_flush(0, 0);
    Out o = {g_dp_buf, 0, MAX_DATA};
    o.b((BYTE)k);
    o.b((BYTE)s);
    dp_outputs(k, s, args, r, o);
    put(E_DP_CALL, r, o.p, o.n);
    dp_net_line(k, s, args, r);
}

DWORD dp_init_rec(int k, void *self, const DWORD *args)
{
    using F = DWORD(WINAPI *)(void *, DWORD, DWORD, DWORD);
    const F real = reinterpret_cast<F>(static_cast<uintptr_t>(dp_real(self, 3)));
    if (g_mode != RECORD || !g_v7_events)
        return real(self, args[0], args[1], args[2]);
    g_dp_handler = args[1];   // the game's handler behind ours
    if (const BYTE *game = reinterpret_cast<const BYTE *>(static_cast<uintptr_t>(args[0]))) {
        // What lies around the 0x900-byte net game object (its handler's context): the dword at +0x904 decides whether
        // slot 100 counts as free (docs/ORIGINAL_BUGS.md 1.16); with FixedHeap=0 Windows' heap, else the fixed heap's
        struct {
            void *base, *allocation_base;
            DWORD allocation_protect;
            size_t size;
            DWORD state, protect, type;
        } mi;
        const bool readable = VirtualQuery(game + 0x90f, &mi, sizeof mi) && mi.state == 0x1000 /* MEM_COMMIT */ &&
                              !(mi.protect & 0x101) /* PAGE_NOACCESS, PAGE_GUARD */;
        const DWORD *a = reinterpret_cast<const DWORD *>(game - 8), *b = reinterpret_cast<const DWORD *>(game + 0x8f8);
        if (readable)
            log("Replay: the net game object %x (%s heap): before it %08x %08x; from +0x8f8 %08x %08x %08x %08x %08x %08x",
                args[0], g_fixed_heap ? "the fixed" : "Windows'", a[0], a[1], b[0], b[1], b[2], b[3], b[4], b[5]);
        else
            log("Replay: the net game object %x: the bytes past it are not readable", args[0]);
    }
    const bool outer = dp_outer();
    const DWORD r = real(self, args[0], (DWORD)(uintptr_t)ptr(&dp_callback_rec), args[2]);
    if (outer)
        dp_record_call(k, 3, self, args, r);
    return r;
}

// Replay: a fake object's method
bool g_dp_live_logged;
DWORD dp_fake(int k, int s, void *self, const DWORD *args)
{
    Fake *f = static_cast<Fake *>(self);
    if (s == 1 || s == 2) {   // AddRef / Release from another thread (the handler keeps an enumeration's addresses)
        if (GetCurrentThreadId() != g_main_thread)
            return (DWORD)(s == 1 ? __atomic_add_fetch(&f->refs, 1, __ATOMIC_RELAXED) : __atomic_sub_fetch(&f->refs, 1, __ATOMIC_RELAXED));
    }
    if (g_mode != REPLAY || GetCurrentThreadId() != g_main_thread) {
        if (!g_dp_live_logged) {
            g_dp_live_logged = true;
            log("Replay: DirectPlay called after the replay went live (%s.%s, frame %u): fake objects answer E_FAIL", KIND_DP[k],
                dp_name(k, s), g_frame);
        }
        return s == 1 || s == 2 ? 1 : 0x80004005u;
    }
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    if (k == K_CLIENT && s == 5)   // its answers come before its own event: they need its context
        g_enum_context = args[8];
    deliver_callbacks();
    DWORD r, n;
    if (!take(E_DP_CALL, &r, g_dp_buf, MAX_DATA, &n))
        return 0x80004005u;
    In in = {g_dp_buf, n, 0};
    const int rk = in.b(), rs = in.b();
    if (rk != k || rs != s) {
        char why[160];
        wsprintfA(why, "DIVERGED: the game calls DirectPlay %s.%s, the recording has %s.%s", KIND_DP[k], dp_name(k, s),
                  rk < NKIND_DP ? KIND_DP[rk] : "?", rk < NKIND_DP ? dp_name(rk, rs) : "?");
        go_live(why);
        return 0x80004005u;
    }
    dp_apply(k, s, args, in);
    if ((k == K_SERVER || k == K_CLIENT) && s == 3) {
        g_cb_context = args[0];
        g_cb_handler = args[1];
    }
    if (k == K_CLIENT && s == 5)
        g_enum_context = args[8];
    if ((k == K_SERVER && s == 25) || (k == K_CLIENT && s == 15))
        buf_return(args[0]);
    if (s == 1)
        __atomic_add_fetch(&f->refs, 1, __ATOMIC_RELAXED);
    dp_net_line(k, s, args, r);
    return r;
}

// ---- CoCreateInstance and the locks ----------------------------------------------------------------------------------
int dp_kind_of_clsid(const BYTE *clsid)
{
    if (same_guid(clsid, CLSID_SERVER))
        return K_SERVER;
    if (same_guid(clsid, CLSID_CLIENT))
        return K_CLIENT;
    if (same_guid(clsid, CLSID_ADDRESS))
        return K_ADDRESS;
    return -1;
}
DWORD WINAPI my_CoCreateInstance(const BYTE *clsid, void *outer, DWORD context, const BYTE *iid, void **out)
{
    using F = DWORD(WINAPI *)(const BYTE *, void *, DWORD, const BYTE *, void **);
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    const int kind = g_v7_events && main_thread() ? dp_kind_of_clsid(clsid) : -1;
    if (kind < 0)
        return REAL(S_COCREATE, F)(clsid, outer, context, iid, out);
    char text[64];
    wsprintfA(text, "CoCreateInstance %s context %x", KIND_DP[kind], context);
    if (g_mode == REPLAY) {
        DWORD r, n;
        BYTE d[2] = {0, 0};
        if (!take(E_COCREATE, &r, d, 2, &n))
            return REAL(S_COCREATE, F)(clsid, outer, context, iid, out);
        if (d[0] != kind) {
            go_live("DIVERGED: CoCreateInstance of another DirectPlay object than the recording's");
            return 0x80004005u;
        }
        if (out)
            *out = d[1] ? fake_new(kind) : nullptr;
        net_line(text, nullptr, 0);
        return r;
    }
    g_dp_depth = 1;
    const DWORD r = REAL(S_COCREATE, F)(clsid, outer, context, iid, out);
    g_dp_depth = 0;
    const bool got = out && *out && (int)r >= 0;
    if (got)
        dp_patch(*out, kind);
    const BYTE d[2] = {(BYTE)kind, got};
    put(E_COCREATE, r, d, 2);
    net_line(text, nullptr, 0);
    return r;
}

void WINAPI my_EnterCriticalSection(void *cs)
{
    using F = void(WINAPI *)(void *);
    const F real = REAL(S_ENTERCS, F);
    if (!g_v7_events || g_mode == LIVE) {
        real(cs);
        return;
    }
    const int lock = lock_of(cs);
    if (!lock) {
        real(cs);
        return;
    }
    const DWORD self = GetCurrentThreadId();
    if (self != g_main_thread) {   // a callback on DirectPlay's threads: which lock, after how many of the main thread's entries
        real(cs);
        if (g_mode == RECORD)
            for (int i = 0; i < NCTX; i++)
                if (g_ctx_thread[i] == self) {
                    CbRec *r = g_ctx_rec[i];
                    if (r && !r->lock) {
                        r->tag = g_main_entries[lock];
                        r->seq = (DWORD)__atomic_fetch_add(&g_cb_seq, 1, __ATOMIC_RELAXED);
                        __atomic_store_n(&r->lock, lock, __ATOMIC_RELEASE);
                    }
                    break;
                }
        return;
    }
    if (g_in_cb_main) {   // the handler on the main thread
        real(cs);
        return;
    }
    if (g_mode == REPLAY) {
        g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
        deliver_callbacks();
        DWORD r;
        if (take(E_DP_LOCK, &r, nullptr, 0) && r != (DWORD)lock) {
            char why[96];
            wsprintfA(why, "DIVERGED: the main thread enters DirectPlay lock %u, the recording has lock %u", lock, r);
            go_live(why);
        }
        real(cs);
        return;
    }
    real(cs);
    const DWORD entry = g_main_entries[lock]++;
    cb_flush(lock, entry);
    put(E_DP_LOCK, lock, nullptr, 0);
}

// ---- Winsock ----------------------------------------------------------------------------------------------------------
// What a call hands back: select's three sets (their sockets after it), recv's and recvfrom's bytes and sender,
// getsockname's address, WSAStartup's WSADATA, gethostbyname's host entry
struct HostEnt {
    DWORD name, aliases;
    WORD type, length;
    DWORD addresses;
};
void ws_set(Out &o, DWORD set)
{
    const DWORD count = set ? (*dptr(set) < 64 ? *dptr(set) : 64) : 0;
    o.dw(set ? count : 0xffffffffu);
    o.bytes(bptr(set + 4), set ? count * 4 : 0);
}
void ws_set_in(In &i, DWORD set)
{
    const DWORD count = i.dw();
    if (count == 0xffffffffu)
        return;
    const BYTE *b = i.bytes(count * 4);
    if (set && b && count <= 64) {
        *dptr(set) = count;
        memcpy(bptr(set + 4), b, count * 4);
    }
}
const char *WS_NAME[] = {"WSAStartup", "WSACleanup", "socket", "closesocket", "bind", "getsockname", "setsockopt", "connect",
                         "send", "recv", "sendto", "recvfrom", "select", "gethostbyname"};

// A socket address: an IPv4 sockaddr_in without its 8 bytes sin_zero, which Winsock ignores and which the GameSpy SDK
// leaves as the stack had them (qr_init's and gcd's SockAddr, gamespy_qr.cpp, gamespy_gcd.cpp: family, port and address
// set; the first LAN replays differed only there, the original's stack against the reimplementation's)
void sockaddr_bytes(Out &o, DWORD p, DWORD len)
{
    if (!p || len >= 0x100)
        return;
    o.bytes(bptr(p), len >= 8 && *reinterpret_cast<const WORD *>(bptr(p)) == 2 ? 8 : len);
}

// The Net line of what the side sends or sets up (the receiving calls' inputs are not logged)
void ws_net_line(int slot, const DWORD *a, DWORD r)
{
    static BYTE data[MAX_EVENT];
    Out o = {data, 0, MAX_DATA};
    char text[160];
    const char *name = WS_NAME[slot - S_WS_STARTUP];
    switch (slot) {
    case S_WS_STARTUP: wsprintfA(text, "%s %x = %x", name, a[0], r); break;
    case S_WS_CLEANUP: wsprintfA(text, "%s = %x", name, r); break;
    case S_WS_SOCKET: wsprintfA(text, "%s %u %u %u = %x", name, a[0], a[1], a[2], r); break;
    case S_WS_CLOSESOCKET: wsprintfA(text, "%s %x = %x", name, a[0], r); break;
    case S_WS_BIND: case S_WS_CONNECT:
        wsprintfA(text, "%s %x = %x", name, a[0], r);
        sockaddr_bytes(o, a[1], a[2]);
        break;
    case S_WS_SETSOCKOPT:
        wsprintfA(text, "%s %x %x %x = %x", name, a[0], a[1], a[2], r);
        o.bytes(bptr(a[3]), a[3] && a[4] < 0x100 ? a[4] : 0);
        break;
    case S_WS_SEND:
        wsprintfA(text, "%s %x flags %x = %x", name, a[0], a[3], r);
        o.bytes(bptr(a[1]), a[1] && a[2] < MAX_DATA ? a[2] : 0);
        break;
    case S_WS_SENDTO:
        wsprintfA(text, "%s %x flags %x = %x", name, a[0], a[3], r);
        sockaddr_bytes(o, a[4], a[5]);
        o.bytes(bptr(a[1]), a[1] && a[2] < MAX_DATA - 0x100 ? a[2] : 0);
        break;
    case S_WS_GETHOSTBYNAME: {
        const char *host = reinterpret_cast<const char *>(static_cast<uintptr_t>(a[0]));
        wsprintfA(text, "%s %.100s = %s", name, host ? host : "(null)", r ? "found" : "none");
        break;
    }
    default:
        return;
    }
    net_line(text, data, o.n);
}

void ws_record(int slot, const DWORD *a, DWORD r)
{
    static BYTE data[MAX_EVENT];
    Out o = {data, 0, MAX_DATA};
    o.b((BYTE)slot);
    switch (slot) {
    case S_WS_STARTUP: o.bytes(bptr(a[1]), a[1] ? 400 : 0); break;   // WSADATA
    case S_WS_GETSOCKNAME: {
        const DWORD n = a[2] ? *dptr(a[2]) : 0;
        o.dw(n);
        o.bytes(bptr(a[1]), a[1] && r == 0 && n < 0x100 ? n : 0);
        break;
    }
    case S_WS_RECV: o.bytes(bptr(a[1]), (int)r > 0 ? r : 0); break;
    case S_WS_RECVFROM: {
        const DWORD n = (int)r > 0 ? r : 0, from = a[5] ? *dptr(a[5]) : 0;
        o.dw(n);
        o.bytes(bptr(a[1]), n);
        o.dw(from);
        o.bytes(bptr(a[4]), a[4] && n && from < 0x100 ? from : 0);
        break;
    }
    case S_WS_SELECT: ws_set(o, a[1]); ws_set(o, a[2]); ws_set(o, a[3]); break;
    case S_WS_GETHOSTBYNAME:
        if (r) {
            const HostEnt *h = reinterpret_cast<const HostEnt *>(static_cast<uintptr_t>(r));
            DWORD n = 0;
            while (h->addresses && n < 8 && dptr(h->addresses)[n])
                n++;
            const DWORD length = h->length < 16 ? h->length : 16;
            o.dw(h->type);
            o.dw(length);
            o.dw(n);
            for (DWORD i = 0; i < n; i++)
                o.bytes(bptr(dptr(h->addresses)[i]), length);
            const char *hname = reinterpret_cast<const char *>(static_cast<uintptr_t>(h->name));
            const DWORD nl = hname ? lstrlenA(hname) : 0;
            o.dw(nl < 200 ? nl : 200);
            o.bytes(hname, nl < 200 ? nl : 200);
        }
        break;
    default:
        break;
    }
    put(E_WS, r, o.p, o.n);
    ws_net_line(slot, a, r);
}

// WSAStartup and WSACleanup also run live on replay, so the pure functions (inet_ntoa) find Winsock started
void ws_replay_startup(int slot, const DWORD *a)
{
    using F = DWORD(WINAPI *)(DWORD, void *);
    if (slot == S_WS_STARTUP) {
        static BYTE scratch[512];
        REAL(S_WS_STARTUP, F)(a[0], scratch);
    } else if (slot == S_WS_CLEANUP)
        REAL(S_WS_CLEANUP, DWORD(WINAPI *)())();
}

DWORD ws_replay(int slot, const DWORD *a)
{
    static BYTE data[MAX_EVENT];
    DWORD r, n;
    if (!take(E_WS, &r, data, MAX_DATA, &n))
        return 0xffffffffu;   // SOCKET_ERROR (the replay is live now; no network is opened)
    In i = {data, n, 0};
    if (i.b() != slot) {
        go_live("DIVERGED: the game calls another Winsock function than the recording");
        return 0xffffffffu;
    }
    switch (slot) {
    case S_WS_STARTUP: {
        const BYTE *b = i.bytes(400);
        if (a[1] && b)
            memcpy(bptr(a[1]), b, 400);
        break;
    }
    case S_WS_GETSOCKNAME: {
        const DWORD len = i.dw();
        if (a[2])
            *dptr(a[2]) = len;
        const BYTE *b = i.bytes(a[1] && r == 0 && len < 0x100 ? len : 0);
        if (a[1] && b && r == 0)
            memcpy(bptr(a[1]), b, len);
        break;
    }
    case S_WS_RECV:
        if ((int)r > 0)
            if (const BYTE *b = i.bytes(r))
                memcpy(bptr(a[1]), b, r);
        break;
    case S_WS_RECVFROM: {
        const DWORD k = i.dw();
        if (const BYTE *b = i.bytes(k))
            memcpy(bptr(a[1]), b, k);
        const DWORD from = i.dw();
        if (a[5])
            *dptr(a[5]) = from;
        if (const BYTE *b = i.bytes(a[4] && k && from < 0x100 ? from : 0))
            memcpy(bptr(a[4]), b, from);
        break;
    }
    case S_WS_SELECT: ws_set_in(i, a[1]); ws_set_in(i, a[2]); ws_set_in(i, a[3]); break;
    case S_WS_GETHOSTBYNAME: {
        if (!r)
            break;
        static HostEnt host;
        static DWORD aliases[1], addresses[9];
        static BYTE address_bytes[8][16];
        static char name[208];
        host.type = (WORD)i.dw();
        host.length = (WORD)i.dw();
        const DWORD count = i.dw();
        for (DWORD k = 0; k < 9; k++)
            addresses[k] = 0;
        for (DWORD k = 0; k < count && k < 8; k++) {
            if (const BYTE *b = i.bytes(host.length))
                memcpy(address_bytes[k], b, host.length);
            addresses[k] = (DWORD)(uintptr_t)address_bytes[k];
        }
        const DWORD nl = i.dw();
        const BYTE *b = i.bytes(nl);
        memset(name, 0, sizeof name);
        if (b && nl < 200)
            memcpy(name, b, nl);
        host.name = (DWORD)(uintptr_t)name;
        host.aliases = (DWORD)(uintptr_t)aliases;
        host.addresses = (DWORD)(uintptr_t)addresses;
        r = (DWORD)(uintptr_t)&host;
        break;
    }
    default:
        break;
    }
    ws_net_line(slot, a, r);
    return r;
}

// ---- The frame log -------------------------------------------------------------------------------------------------

inline DWORD fnv(DWORD h, DWORD v)
{
    for (int i = 0; i < 4; i++, v >>= 8)
        h = (h ^ (v & 0xff)) * 16777619u;
    return h;
}
constexpr DWORD FNV0 = 2166136261u;

bool readable(const void *p, DWORD n)
{
    const DWORD a = (DWORD)(uintptr_t)p;
    if (a < 0x10000 || a >= 0x80000000u || a + n < a)
        return false;
    // the fixed heap's committed part without asking Windows: VirtualQuery measures the run of pages alike behind the
    // address, which in one big committed block made every frame line slow (replays ten times slower)
    for (const Region &r : g_region)
        if (a >= r.base && a + n <= r.committed)
            return true;
    struct {
        void *base, *allocation_base;
        DWORD allocation_protect;
        size_t size;
        DWORD state, protect, type;
    } mi;
    for (DWORD q = a & ~0xfffu; q < a + n; q += 0x1000)
        if (!VirtualQuery(at<void *>(q), &mi, sizeof mi) || mi.state != 0x1000 || (mi.protect & 0x101) || !(mi.protect & 0xee))
            return false;
    return true;
}

// [Reimpl] ScreenGrid=1: the screen also as 8 x 8 cells (rows of cells top to bottom), logged as a "Grid" line per
// frame, to see where two runs' screens differ
bool g_screen_grid;
DWORD g_grid[64];

// The back buffer about to be shown, row by row (its own lock, or the game's when it holds one)
DWORD screen_hash(DWORD *bits_out)
{
    const DWORD back = rd(glob_addr(0x713338));
    *bits_out = 0;
    if (!back)
        return 0;
    DWORD d[0x1b];
    const DWORD *desc = d;
    bool locked = false;
    if (rd(glob_addr(0x713318)))
        desc = glob<const DWORD *>(0x7132a8);
    else {
        memset(d, 0, sizeof d);
        d[0] = 0x6c;
        using Lock = long(WINAPI *)(DWORD, void *, void *, DWORD, void *);
        g_dd_own = true;
        const long locked_r = com_slot<Lock>(back, 25)(back, nullptr, d, 0x11, nullptr);
        g_dd_own = false;
        if (locked_r < 0)
            return 0;
        locked = true;
    }
    const DWORD h = desc[2], w = desc[3], bits = desc[0x15];
    const int pitch = (int)desc[4];
    const BYTE *base = at<const BYTE *>(desc[9]);
    DWORD hash = FNV0;
    const DWORD row_dwords = w * bits / 32;
    for (int i = 0; i < 64; i++)
        g_grid[i] = FNV0;
    if (base && h <= 4096 && row_dwords <= 4096)
        for (DWORD y = 0; y < h; y++) {
            const DWORD *row = reinterpret_cast<const DWORD *>(base + (int)y * pitch);
            for (DWORD x = 0; x < row_dwords; x++)
                hash = (hash ^ row[x]) * 16777619u;
            if (g_screen_grid)
                for (DWORD x = 0; x < row_dwords; x++) {
                    DWORD &c = g_grid[(y * 8 / h) * 8 + x * 8 / row_dwords];
                    c = (c ^ row[x]) * 16777619u;
                }
        }
    if (locked) {
        using Unlock = long(WINAPI *)(DWORD, void *);
        com_slot<Unlock>(back, 32)(back, nullptr);
    }
    // 8 bit: the pixels are palette indices, so the DirectDraw palette (0x713344; the fades and the brightness change
    // only it) is folded in too, read with IDirectDrawPalette::GetEntries (slot 4: flags 0, from 0, 256 entries;
    // it only reads). The 16-bit lines stay as they were.
    if (bits == 8)
        if (const DWORD pal = rd(glob_addr(0x713344))) {
            DWORD entries[256];
            using GetEntries = long(WINAPI *)(DWORD, DWORD, DWORD, DWORD, DWORD *);
            if (com_slot<GetEntries>(pal, 4)(pal, 0, 0, 256, entries) >= 0)
                for (DWORD e : entries)
                    hash = (hash ^ e) * 16777619u;
            else
                hash = (hash ^ 0xbad0a1u) * 16777619u;
        }
    *bits_out = bits;
    return hash;
}

// The class kinds of a part by its vtable (the getter's class variable, as the checks' class_of): bit 0 a position at
// +0x4c (CSimpleLayerPart or CEntityBase), 1 CEntityBase, 2 CEntityDestructable, 3 CEntityAnimated; 0x80 known
constexpr int NKIND = 2048;
DWORD g_kind_vt[NKIND];
BYTE g_kind[NKIND];
BYTE kind_of(DWORD vt)
{
    DWORD i = (vt >> 2) % NKIND;
    for (int probe = 0; probe < NKIND; probe++, i = (i + 1) % NKIND) {
        if (g_kind_vt[i] == vt)
            return g_kind[i];
        if (!g_kind_vt[i])
            break;
    }
    BYTE k = 0x80;
    // bounds as numbers (exe addresses compared with exe addresses; written with eight digits, so the tools do not take
    // them for data the layer translates: glob_addr(0x610000) gave the own block 0x610000 once it moved, and
    // zax::vt(0x600000) named a "vtable" that was the flag mask 0x600000, batch 6-10a)
    if (vt >= 0x005e0000 && vt < 0x00610000 && !(vt & 3)) {
        // the slot from our own vtable when it has moved; its value our replacement since batch 6-10b (the code values
        // of the data are the function table's entries), so taken back to the exe address whose code is read below
        const DWORD getter = exe_code_of(rd(glob_addr_dyn(vt) + 8));
        if (getter >= 0x00401000 && getter < 0x005e0000) {
#ifdef ZAX_STANDALONE
            // the getter's first instruction from the table tools/extract_exe_data.py makes (no code of the exe here
            // since batch 6-10c)
            DWORD var = 0;
            for (int lo = 0, hi = zax_getter_var_count; lo < hi;) {
                const int mid = (lo + hi) / 2;
                if (zax_getter_vars[mid][0] == getter) {
                    var = zax_getter_vars[mid][1];
                    break;
                }
                if (zax_getter_vars[mid][0] < getter)
                    lo = mid + 1;
                else
                    hi = mid;
            }
#else
            const BYTE *code = static_cast<const BYTE *>(original_address(getter));
            const DWORD var = code[0] == 0xa1 ? *reinterpret_cast<const DWORD *>(code + 1) : 0;
#endif
            if (var >= 0x00600000 && var < 0x720000) {
                // the class variables through the layer: the one read from the getter's code may be an own block's
                // (batch 6-5: CUnderConstructionLayerPart's 0x713a3c, CWayPointsPolygon's 0x713df4), poisoned in the image
                const DWORD simple = rd(glob_addr_dyn(0x713560)), base = rd(glob_addr_dyn(0x6bdd44)),
                            destr = rd(glob_addr_dyn(0x6bdd4c)), anim = rd(glob_addr_dyn(0x6bdd98));
                for (DWORD ci = rd(glob_addr_dyn(var)), n = 0; ci && n < 32; ci = rd(ci + 0x18), n++) {
                    if (ci == simple || ci == base)
                        k |= 1;
                    if (ci == base)
                        k |= 2;
                    if (ci == destr)
                        k |= 4;
                    if (ci == anim)
                        k |= 8;
                }
            }
        }
    }
    if (!g_kind_vt[i]) {
        g_kind_vt[i] = vt;
        g_kind[i] = k;
    }
    return k;
}

DWORD part_hash(DWORD h, const BYTE *p)
{
    const DWORD vt = exe_address_of(rd(p, 0));   // an own vtable of the standalone exe by its exe address (batch 6-3)
    const BYTE k = kind_of(vt);
    h = fnv(h, vt);
    h = fnv(h, rd(p, 0x1c));
    h = fnv(h, rd(p, 0x24));
    h = fnv(h, rd(p, 0x28) & 0xffff);
    if (k & 1) {
        h = fnv(h, rd(p, 0x4c));
        h = fnv(h, rd(p, 0x50));
    }
    if (k & 2) {
        h = fnv(h, rd(p, 0x74));
        h = fnv(h, rd(p, 0x78));
    }
    if (k & 4)
        h = fnv(h, rd(p, 0x7c));
    if (k & 8)
        for (DWORD off = 0xb0; off <= 0xcc; off += 4)
            h = fnv(h, rd(p, off));
    // the AIs ("Activity", a pointer array at +0x38: data +0x3c, count +0x40)
    const DWORD n = rd(p, 0x40), data = rd(p, 0x3c);
    h = fnv(h, n);
    if (n < 256 && readable(at<void *>(data), n * 4))
        for (DWORD i = 0; i < n; i++) {
            const DWORD ai = rd(data + i * 4);
            h = fnv(h, readable(at<void *>(ai), 4) ? exe_address_of(rd(ai)) : 0);
        }
    return h;
}

// [Reimpl] WorldLogFrame=<n> (and WorldLogFrames=<count>, default 1; 0 = off): from frame n every part the world hash
// takes is logged as a "World" line (layer, cell, index, the vtable by its exe address, its kind, the part's hash
// alone and the raw fields it hashes), its AIs' and an animated entity's behaviours' vtables (behaviours with their
// first dwords), so two runs whose world hashes differ can be diffed part by part (batch saves). The raw fields are
// not mapped to exe addresses: compare the hashes
DWORD g_world_log_frame, g_world_log_frames = 1;

// Every part in the grids of all layers, cell by cell in the grid's order (a part in several cells counts in each)
DWORD world_hash(DWORD *parts_out)
{
    DWORD h = FNV0, parts = 0;
    const DWORD world = rd(glob_addr(0x6c2080));
    if (readable(at<void *>(world), 0x338)) {
        const BYTE *w = at<const BYTE *>(world);
        const DWORD nlayers = rd(w, 0x2c0), layers = rd(w, 0x2bc);
        h = fnv(h, nlayers);
        for (DWORD l = 0; l < nlayers && l < 64 && readable(at<void *>(layers), nlayers * 4); l++) {
            const BYTE *layer = at<const BYTE *>(rd(layers + l * 4));
            if (!readable(layer, 0x6c))
                continue;
            const DWORD columns = rd(layer, 0x60), rows = rd(layer, 0x64), cells = rd(layer, 0x68);
            h = fnv(fnv(h, columns), rows);
            if (columns > 4096 || rows > 4096 || !readable(at<void *>(cells), columns * rows * 16))
                continue;
            for (DWORD c = 0; c < columns * rows; c++) {
                const DWORD *cell = at<const DWORD *>(cells + c * 16);   // vtable, data, count, capacity
                const DWORD count = cell[2];
                if (!count)
                    continue;
                h = fnv(fnv(h, c), count);
                if (count > 10000 || !readable(at<void *>(cell[1]), count * 4))
                    continue;
                for (DWORD k = 0; k < count; k++) {
                    const BYTE *p = at<const BYTE *>(rd(cell[1] + k * 4));
                    if (!readable(p, 0xd0))
                        continue;
                    h = part_hash(h, p);
                    parts++;
                    if (g_world_log_frame && g_frame >= g_world_log_frame && g_frame - g_world_log_frame < g_world_log_frames) {
                        const DWORD vt = exe_address_of(rd(p, 0));
                        log("World %u %u %u: vt %x kind %x hash %08x: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x "
                            "AIs %u", l, c, k, vt, kind_of(vt), part_hash(FNV0, p), rd(p, 0x1c), rd(p, 0x24), rd(p, 0x28),
                            rd(p, 0x4c), rd(p, 0x50), rd(p, 0x74), rd(p, 0x78), rd(p, 0x7c), rd(p, 0xb0), rd(p, 0xb4),
                            rd(p, 0xb8), rd(p, 0xbc), rd(p, 0xc0), rd(p, 0xc4), rd(p, 0xc8), rd(p, 0xcc), rd(p, 0x40));
                        const DWORD n = rd(p, 0x40), data = rd(p, 0x3c);
                        for (DWORD i = 0; i < n && i < 256 && readable(at<void *>(data), n * 4); i++) {
                            const DWORD ai = rd(data + i * 4);
                            log("World AI %u: %x (%x)", i, readable(at<void *>(ai), 4) ? exe_address_of(rd(ai)) : 0,
                                readable(at<void *>(ai), 4) ? rd(ai) : 0);
                        }
                        // an animated entity's behaviours (the CEntityBehaviorStack at +0x90: data +0x14, count +0x18)
                        const DWORD bn = (kind_of(vt) & 8) ? rd(p, 0xa8) : 0, bdata = rd(p, 0xa4);
                        for (DWORD i = 0; i < bn && i < 16 && readable(at<void *>(bdata), bn * 4); i++) {
                            const DWORD b = rd(bdata + i * 4);
                            if (readable(at<void *>(b), 0x28))
                                log("World behaviour %u: %x: +4.. %x %x %x %x %x %x %x %x %x", i, exe_address_of(rd(b)),
                                    rd(b + 4), rd(b + 8), rd(b + 0xc), rd(b + 0x10), rd(b + 0x14), rd(b + 0x18), rd(b + 0x1c),
                                    rd(b + 0x20), rd(b + 0x24));
                        }
                    }
                }
            }
        }
    }
    *parts_out = parts;
    return h;
}

DWORD block_hash(DWORD a, DWORD n)
{
    DWORD h = FNV0;
    for (DWORD i = 0; i < n; i += 4)
        h = fnv(h, rd(a + i));
    return h;
}

void frame_line()
{
    DWORD bits, parts;
    const DWORD screen = screen_hash(&bits);
    const DWORD world = world_hash(&parts);
    char player[96] = "none";
    const DWORD w = rd(glob_addr(0x6c2080));
    if (readable(at<void *>(w), 0x338) && rd(w + 0x294)) {
        const DWORD p = readable(at<void *>(rd(w + 0x290)), 4) ? rd(rd(w + 0x290)) : 0;
        if (readable(at<void *>(p), 0x80))
            wsprintfA(player, "%x %x damage %x", rd(p + 0x4c), rd(p + 0x50), rd(p + 0x7c));
    }
    // the world's time since the game started (+0x2f4) only once a game runs or a level is loaded (+0x310, +0x311):
    // before that it is memory the constructor leaves (different from run to run)
    const DWORD time = readable(at<void *>(w), 0x338) && (rd(w + 0x310) & 0xffff) ? rd(w + 0x2f4) : 0;
    log("Frame %u: screen %08x (%u bit) world %08x (%u parts) random %08x/%u %08x/%u player %s time %x events %u heap %u/%x", g_frame,
        screen, bits, world, parts, block_hash(glob_addr(0x7124c0), 0x400), rd(glob_addr(0x7124c4)), block_hash(glob_addr(0x6d06c0), 0x400), rd(glob_addr(0x6d06c4)), player,
        time, g_events, g_region[0].allocs, g_region[0].top - g_region[0].base);
    if (g_fixed_heap && !(g_frame % 200))
        log("Heap at frame %u: allocations %u + %u, frees %u, reallocations in place %u, moved %u (%u KB copied), lock waits %u, "
            "used %u KB + %u KB",
            g_frame, g_region[0].allocs, g_region[1].allocs, g_heap_frees, g_heap_inplace, g_heap_moves, (DWORD)(g_heap_copied >> 10),
            g_heap_spins, (g_region[0].top - g_region[0].base) >> 10, (g_region[1].top - g_region[1].base) >> 10);
    if (g_screen_grid) {
        char line[1024];
        int o = wsprintfA(line, "Grid %u:", g_frame);
        for (int i = 0; i < 64; i++)
            o += wsprintfA(line + o, " %08x", g_grid[i]);
        log("%s", line);
    }
}

using FlipFn = long(WINAPI *)(DWORD, DWORD, DWORD);
// ---- The streamed sounds' state: Stream_IsPlaying, CThreadedStream::Frame, ::Stop, ::Play (batch 6-12) --------------

// A streamed sound (the music, the voice-overs) is refilled by a multimedia timer thread (Stream_TimerCallback
// 0x587700), which, once the end has played (by the sound hardware's play position, in real time), sets the stop
// (+0x58) and clears "playing" (+0xd). The main thread reads both: the dialogue window starts its next line once the
// voice has finished (DialogueWindow_Update 0x47c810 through Voice_IsPlaying 0x595bd0; the line's voice opens its Ogg
// stream, 369 allocations, 1.1 MB), the music picks its next song by Stream_IsPlaying, the music's frame (App_UpdateMusic
// 0x4ce510 → CThreadedStream::Frame) deletes the readers of a stopped stream (their ZZipStream a 0x24-byte pool block),
// and Stop and Play stop a stream only while it plays (the dialogue window's destructor stops its voice, Voice_Stop
// 0x595af0). A replay runs faster than real time, so these came 1–4 frames apart from run to run (01 Main, /Original
// against itself, frames 521–525; 06 Elec Fence Main from about frame 700) and the pool handed out other blocks. So
// what the main thread sees is recorded (E_STREAM the "playing" byte, E_STREAM_END whether the frame deleted the
// readers, E_STREAM_STOP and E_STREAM_PLAY the byte at their entry) and the replay makes the live state agree: where
// the recording saw the end earlier, the main thread waits for the timer thread to reach it (at most 20 s); where it
// saw it later, the stream counts as still playing, its frame is skipped, and a stop is taken as for a playing stream.
// Other threads ask live; so do the stream's own functions (their waiting loops call the frame from inside
// 0x587060–0x5879f5 in the original, directly in the reimplementation). Installed with the Flip slot (the hooks are
// in place by then): in the DLL a jump at each address (over the hook's jump, or over the original's first
// instructions, which a trampoline runs), in the exe the function table's entries.
constexpr BYTE E_STREAM_END = E_STREAM + 1, E_STREAM_STOP = E_STREAM + 2, E_STREAM_PLAY = E_STREAM + 3,
               E_SOUND_PLAYING = E_STREAM + 4;
constexpr DWORD STREAM_IS_PLAYING = 0x5876e0, STREAM_FRAME = 0x587160, STREAM_STOP = 0x587600, STREAM_PLAY = 0x587310,
                STREAM_CODE = 0x587060, STREAM_CODE_END = 0x5879f5;
using StreamFn = DWORD(THISCALL *)(BYTE **);
using PlayFn = DWORD(THISCALL *)(BYTE **, const char *, DWORD);
StreamFn g_stream_frame_real, g_stream_stop_real;
PlayFn g_stream_play_real;
DWORD g_stream_waits;   // waits for the timer thread, and how long in all (ms)
DWORD g_stream_wait_ms;

inline DWORD GetTickCount_real() { return REAL(S_TICK, DWORD(WINAPI *)())(); }

// Until the timer thread has cleared s's "playing" byte (the recording saw the stream end earlier)
void wait_stream_end(BYTE *s)
{
    const DWORD start = GetTickCount_real();
    g_stream_waits++;
    while (s[0xd] && GetTickCount_real() - start < 20000)
        Sleep(1);
    g_stream_wait_ms += GetTickCount_real() - start;
    if (s[0xd])
        log("Replay: a stream still plays 20 s after the recording saw its end (frame %u)", g_frame);
}

void patch_ds_position(BYTE **holder);   // "The streams' play position" below

DWORD THISCALL my_stream_playing(BYTE **holder)
{
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    patch_ds_position(holder);
    BYTE *s = *holder;
    const DWORD high = (DWORD)(uintptr_t)s & 0xffffff00u;
    if (!s || !main_thread())
        return s ? high | s[0xd] : 0;
    DWORD r;
    if (g_mode == REPLAY && take(E_STREAM, &r, nullptr, 0)) {
        if (!(r & 0xff) && s[0xd])
            wait_stream_end(s);
        return high | (r & 0xff);
    }
    const BYTE live = s[0xd];
    if (g_mode == RECORD)
        put(E_STREAM, live, nullptr, 0);
    return high | live;
}

// A stream's length in seconds (Stream_Seconds 0x5875e0) is 0 while the timer callback runs (+0xe). Voice_Play
// (0x595be0) asks it right after CThreadedStream::Play has started the stream's timer (every 50 ms), and the dialogue
// window (DialogueWindow_StartLine 0x47cfa0) takes a voice of under 0.1 s as none: the line scrolls at the options'
// speed, and its end is not waited for by Voice_IsPlaying, so the main thread's calls change (zax-redone's g_newgame
// outlier 20261104-164943-3ec0 on a loaded VM: the recording's Stream_IsPlaying after the dialogue's GetTickCount was
// not called, frame 1808). In the original it is a race of the real clock, lost only when the main thread is held
// between the timer's start and the question for longer than the timer's first period. A replay on the main thread
// waits until the callback is done (at most 1 s), so it gets the reader's length as the recordings did; a recording
// that itself caught the callback would now differ there (none known). Other threads and recording ask live.
using SecondsFn = long double(THISCALL *)(BYTE **);
SecondsFn g_stream_seconds_real;
constexpr DWORD STREAM_SECONDS = 0x5875e0;
long double THISCALL my_stream_seconds(BYTE **holder)
{
    volatile BYTE *s = *holder;
    if (g_mode == REPLAY && s && main_thread() && s[0xe]) {
        const DWORD start = GetTickCount_real();
        while (s[0xe] && GetTickCount_real() - start < 1000)
            Sleep(0);
        log("Replay: a stream's length asked while its timer callback ran, waited %u ms for it (frame %u)",
            GetTickCount_real() - start, g_frame);
    }
    return g_stream_seconds_real(holder);
}

// The streams' play position (version 6). With "Use Threaded Stream=0" (Zax.CFG; the default 1 refills in the timer's
// thread) CThreadedStream::Frame runs the refill (0x587700) on the main thread: IDirectSoundBuffer::GetCurrentPosition,
// the sound card's position in real time, decides how much is read from the reader, when the end has played and the
// stream stops. Its answers on the main thread are recorded and replayed (E_DS_POSITION: the result, the play and the
// write position). DirectSound's buffer vtable is patched (slot 4; all secondary buffers share it, other threads and
// the sound effects' buffers, which never ask it, stay live) at the first of the stream's functions the game calls from
// outside with such a stream that has its buffer: Frame, IsPlaying and Stop before, Play after their work. Not only at
// the frame: the reimplementation's Stop waits by calling its frame directly, not through the exe's address (the first
// unthreaded replays diverged there, 20261030-194814-1926).
using PositionFn = long(WINAPI *)(void *, DWORD *, DWORD *);
PositionFn g_ds_position_real;
long WINAPI my_ds_position(void *buffer, DWORD *play, DWORD *write)
{
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    if (!g_v6_events || !main_thread())
        return g_ds_position_real(buffer, play, write);
    DWORD pw[2] = {0, 0}, r;
    if (g_mode == REPLAY && take(E_DS_POSITION, &r, pw, 8)) {
        if (play)
            *play = pw[0];
        if (write)
            *write = pw[1];
        return (long)r;
    }
    r = (DWORD)g_ds_position_real(buffer, &pw[0], &pw[1]);
    if (play)
        *play = pw[0];
    if (write)
        *write = pw[1];
    if (g_mode == RECORD)
        put(E_DS_POSITION, r, pw, 8);
    return (long)r;
}
void patch_ds_position(BYTE **holder)
{
    if (!main_thread())
        return;
    BYTE *s = *holder;
    const BYTE *h = reinterpret_cast<const BYTE *>(holder);
    if (!g_v6_events || !s || h[4] || !rd(s, 0x50))   // threaded (+4) or no buffer yet
        return;
    DWORD *slot = reinterpret_cast<DWORD *>(rd(rd(s, 0x50)) + 4 * 4);
    if (*slot == (DWORD)(uintptr_t)&my_ds_position)
        return;
    DWORD old;
    if (!VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &old)) {
        log("Replay: DirectSound's buffer vtable cannot be written, the play position not recorded");
        return;
    }
    g_ds_position_real = reinterpret_cast<PositionFn>(static_cast<uintptr_t>(*slot));
    *slot = (DWORD)(uintptr_t)&my_ds_position;
    VirtualProtect(slot, 4, old, &old);
    log("Replay: a stream without its timer thread (Use Threaded Stream=0): its play position %s (frame %u)",
        g_mode == RECORD ? "recorded" : "replayed", g_frame);
}

DWORD THISCALL my_stream_frame(BYTE **holder)
{
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    patch_ds_position(holder);
    BYTE *s = *holder;
    if (!s || !main_thread() || (g_caller >= STREAM_CODE && g_caller < STREAM_CODE_END))
        return g_stream_frame_real(holder);
    const DWORD readers = rd(s, 0x14);
    DWORD r;
    if (g_mode == REPLAY && take(E_STREAM_END, &r, nullptr, 0)) {
        if (r && readers && !rd(s, 0x58))
            wait_stream_end(s);
        else if (!r && readers && rd(s, 0x58))
            return 0;   // the recording's frame found the stream still going: its readers stay until then
        const DWORD ret = g_stream_frame_real(holder);
        if (!r && readers && !rd(s, 0x14))
            log("Replay: a stream's readers deleted out of turn (frame %u)", g_frame);
        return ret;
    }
    const DWORD ret = g_stream_frame_real(holder);
    if (g_mode == RECORD)
        put(E_STREAM_END, readers && !rd(s, 0x14), nullptr, 0);
    return ret;
}

// CThreadedStream::Stop and ::Play begin with the "playing" byte: a stream that plays is stopped first (the stop asked
// for, the frame until the timer has stopped it: the readers deleted, the timer killed). Its value at the entry is
// recorded; when the replay finds the stream still playing where the recording had it ended, it waits for the end;
// when it finds it ended where the recording had it playing (a stream whose end came sooner live), the stop is asked
// for and the byte set again, so the function takes the recording's path (the timer thread, seeing the stop, stops
// the buffer and clears the byte, as it would have). Only while the stream has its readers and its timer, else that
// path could not end (logged).
bool sync_entry(BYTE kind, BYTE *s)
{
    if (!s || !main_thread())
        return false;
    DWORD r;
    if (g_mode == REPLAY && take(kind, &r, nullptr, 0)) {
        if ((r & 0xff) && !s[0xd]) {
            if (rd(s, 0x14) && rd(s, 0x10)) {
                *reinterpret_cast<DWORD *>(s + 0x58) = 1;
                s[0xd] = 1;
            } else
                log("Replay: a stream ended without its readers or timer where the recording had it playing (frame %u)", g_frame);
        } else if (!(r & 0xff) && s[0xd])
            wait_stream_end(s);
        return true;
    }
    if (g_mode == RECORD)
        put(kind, s[0xd], nullptr, 0);
    return true;
}

DWORD THISCALL my_stream_stop(BYTE **holder)
{
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    patch_ds_position(holder);
    if (!(g_caller >= STREAM_CODE && g_caller < STREAM_CODE_END))
        sync_entry(E_STREAM_STOP, *holder);
    return g_stream_stop_real(holder);
}

DWORD THISCALL my_stream_play(BYTE **holder, const char *path, DWORD skip)
{
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    if (!(g_caller >= STREAM_CODE && g_caller < STREAM_CODE_END) && path && *path)
        sync_entry(E_STREAM_PLAY, *holder);
    const DWORD r = g_stream_play_real(holder, path, skip);
    patch_ds_position(holder);
    return r;
}

// CSoundResource::IsPlaying (0x57c780, slot 14 of the vtable 0x600b8c, reached only through it: the dword at 0x600bc4
// is its one reference): IDirectSoundBuffer::GetStatus of a sound effect's buffer, in real time as well. The resource
// cache asks it to decide whether a sound may be played again on its buffer or needs a new resource (FindReplacement
// 0x57b630, a 0x9c-byte pool block; 12 WaterTempleMain's save differed in the next CRenderablePolygons' ids in two of five
// hooked replays). Recorded and replayed as an answer (al; the rest of eax as the live call gives it): the game then
// takes the recording's path, and DirectSound only plays the sound again or on another buffer.
constexpr DWORD SOUND_IS_PLAYING_SLOT = 0x600bc4;
StreamFn g_sound_playing_real;
DWORD THISCALL my_sound_playing(BYTE **r)
{
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    const DWORD live = g_sound_playing_real(r);
    if (!main_thread())
        return live;
    DWORD rec;
    if (g_mode == REPLAY && take(E_SOUND_PLAYING, &rec, nullptr, 0))
        return (live & 0xffffff00u) | (rec & 0xff);
    if (g_mode == RECORD)
        put(E_SOUND_PLAYING, live & 0xff, nullptr, 0);
    return live;
}

// The function at `address` to `to`; the previous code in *real (a trampoline in the DLL)
template <typename F> bool redirect(DWORD address, const void *to, F *real, const BYTE *first, int n)
{
#ifdef ZAX_STANDALONE
    (void)first, (void)n;
    const int i = layer::find(layer::FN, address);
    if (i < 0)
        return false;
    if (real)
        *real = reinterpret_cast<F>(static_cast<uintptr_t>(layer::zax_fn_table[i]));
    layer::zax_fn_table[i] = (DWORD)(uintptr_t)to;
    return true;
#else
    BYTE *code = at<BYTE *>(address);
    if (real) {
        BYTE *t = static_cast<BYTE *>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!t)
            return false;
        DWORD back;
        int k = 0;
        if (code[0] == 0xe9) {   // a hook's jump: the trampoline jumps where it goes
            INT32 rel;
            memcpy(&rel, code + 1, 4);
            back = address + 5 + (DWORD)rel;
        } else {   // the original's first whole instructions, then back behind them
            if (memcmp(code, first, n))
                return false;
            memcpy(t, code, n);
            k = n;
            back = address + n;
        }
        t[k] = 0xe9;
        const INT32 rel = (INT32)(back - ((DWORD)(uintptr_t)t + k + 5));
        memcpy(t + k + 1, &rel, 4);
        FlushInstructionCache(GetCurrentProcess(), t, 32);
        *real = reinterpret_cast<F>(t);
    }
    DWORD old;
    if (!VirtualProtect(code, 5, PAGE_EXECUTE_READWRITE, &old))
        return false;
    const INT32 rel = (INT32)((DWORD)(uintptr_t)to - ((DWORD)(uintptr_t)code + 5));
    code[0] = 0xe9;
    memcpy(code + 1, &rel, 4);
    VirtualProtect(code, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), code, 5);
    return true;
#endif
}

// Stream_Seconds to my_stream_seconds. Over the original's code (the DLL with /Original) its first instructions end in
// a short jump (mov eax, [ecx]; test eax, eax; je +0xf, to fld [0x5ebed0]; ret at +0x15), which a trampoline cannot
// copy as it is: the trampoline runs the first two, the jump as a near one, and goes on behind them (+6)
bool redirect_stream_seconds()
{
#ifndef ZAX_STANDALONE
    static const BYTE SECONDS_FIRST[6] = {0x8b, 0x01, 0x85, 0xc0, 0x74, 0x0f};
    const BYTE *code = at<BYTE *>(STREAM_SECONDS);
    if (code[0] != 0xe9) {
        if (memcmp(code, SECONDS_FIRST, 6))
            return false;
        BYTE *t = static_cast<BYTE *>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!t)
            return false;
        memcpy(t, SECONDS_FIRST, 4);
        t[4] = 0x0f;
        t[5] = 0x84;
        INT32 rel = (INT32)(STREAM_SECONDS + 0x15 - ((DWORD)(uintptr_t)t + 10));
        memcpy(t + 6, &rel, 4);
        t[10] = 0xe9;
        rel = (INT32)(STREAM_SECONDS + 6 - ((DWORD)(uintptr_t)t + 15));
        memcpy(t + 11, &rel, 4);
        FlushInstructionCache(GetCurrentProcess(), t, 32);
        g_stream_seconds_real = reinterpret_cast<SecondsFn>(t);
        return redirect<SecondsFn>(STREAM_SECONDS, ptr(&my_stream_seconds), nullptr, nullptr, 0);
    }
#endif
    return redirect(STREAM_SECONDS, ptr(&my_stream_seconds), &g_stream_seconds_real, nullptr, 0);
}

void hook_streams()
{
    if (!g_stream_events || g_mode == LIVE)
        return;
    static const BYTE FRAME_FIRST[6] = {0x56, 0x8b, 0xf1, 0x8a, 0x46, 0x04};   // push esi; mov esi, ecx; mov al, [esi+4]
    static const BYTE STOP_FIRST[6] = {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x37};    // push esi; push edi; mov edi, ecx; mov esi, [edi]
    static const BYTE PLAY_FIRST[5] = {0x83, 0xec, 0x14, 0x53, 0x55};          // sub esp, 0x14; push ebx; push ebp
    if (!redirect(STREAM_FRAME, ptr(&my_stream_frame), &g_stream_frame_real, FRAME_FIRST, 6) ||
        !redirect(STREAM_STOP, ptr(&my_stream_stop), &g_stream_stop_real, STOP_FIRST, 6) ||
        !redirect(STREAM_PLAY, ptr(&my_stream_play), &g_stream_play_real, PLAY_FIRST, 5) ||
        !redirect<StreamFn>(STREAM_IS_PLAYING, ptr(&my_stream_playing), nullptr, nullptr, 0) ||
        !redirect_stream_seconds()) {
        log("Replay: the streams' functions cannot be redirected, their state not recorded");
        return;
    }
    DWORD *slot = at<DWORD *>(SOUND_IS_PLAYING_SLOT);   // the exe: our own vtable block (at<>'s translation)
    DWORD old;
    if (VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &old)) {
        g_sound_playing_real = reinterpret_cast<StreamFn>(static_cast<uintptr_t>(*slot));
        *slot = (DWORD)(uintptr_t)&my_sound_playing;
        VirtualProtect(slot, 4, old, &old);
    } else
        log("Replay: the sound resources' vtable cannot be written, their state not recorded");
    log("Replay: the streams' state %s from here on (events so far %u)", g_mode == RECORD ? "recorded" : "replayed", g_events);
}

// Flip's real method per patched vtable (hook_flip): the game makes its primary surface again when the display mode
// changes [assumption: the new one could come with a vtable of its own; on the VMs so far it has the same one, its
// slots written over (rehook_surfaces)]
struct FlipVt {
    DWORD vt, real;
};
FlipVt g_flipvt[8];
bool g_flip_hooked;
DWORD flip_real(DWORD self)
{
    const DWORD vt = rd(self);
    for (const FlipVt &v : g_flipvt)
        if (v.vt == vt)
            return v.real;
    return g_flipvt[0].real;
}

void after_dd_call(const char *what, DWORD slot);
long WINAPI my_Flip(DWORD self, DWORD target, DWORD flags)
{
    g_caller = (DWORD)(uintptr_t)__builtin_return_address(0);
    if (GetCurrentThreadId() == g_main_thread && self == rd(glob_addr(G_PRIMARY))) {
        frame_line();
        if (g_pool_stats)
            if (ZPool *pool = *glob<ZPool **>(POOL_GLOBAL))
                if (g_pool_stats / 4 < pool->num_classes) {
                    const ZPoolClass &c = pool->classes[g_pool_stats / 4];
                    log("Pool class %x at frame %u: %u used of %u", g_pool_stats, g_frame, c.used, c.total);
                }
        DWORD r = g_frame;
        if (g_mode == RECORD) {
            put(E_FRAME, g_frame, nullptr, 0);
            flush_out();
        } else if (g_mode == REPLAY && take(E_FRAME, &r, nullptr, 0) && r != g_frame) {
            char why[96];
            wsprintfA(why, "DIVERGED: frame %u where the recording has frame %u", g_frame, r);
            go_live(why);
        }
        g_frame++;
        if (g_v8_events && g_mode != LIVE) {   // a failed flip (the surfaces lost): "Focus, surface loss and the clipboard"
            DWORD r;
            if (g_mode == REPLAY && dd_take(11, 0, &r))
                return (long)r;
            r = (DWORD)reinterpret_cast<FlipFn>(static_cast<uintptr_t>(flip_real(self)))(self, target, flags);
            after_dd_call("the surface's", 11);
            if (g_mode == RECORD && r)
                dd_put(11, 0, r);
            return (long)r;
        }
    }
    const long r = reinterpret_cast<FlipFn>(static_cast<uintptr_t>(flip_real(self)))(self, target, flags);
    after_dd_call("the surface's", 11);
    return r;
}

// IDirectDrawSurface::Flip (slot 11) of the primary surface's vtable to my_Flip, once the surfaces exist, and again
// whenever the primary surface's vtable no longer leads to my_Flip (checked at every PeekMessageA of the main thread):
// a new vtable is patched, a known one written over is patched again (rehook_surfaces)
void rehook_surfaces(const char *where);
void patch_ddraw(DWORD dd);
void patch_surface(DWORD surface);
void hook_flip()
{
    const DWORD primary = rd(glob_addr(G_PRIMARY));
    if (!primary)
        return;
    const DWORD vt = rd(primary);
    DWORD *slot = at<DWORD *>(vt + 11 * 4);
    if (*slot == (DWORD)(uintptr_t)&my_Flip)
        return;
    FlipVt *entry = nullptr;
    for (FlipVt &v : g_flipvt)
        if (v.vt == vt)
            return rehook_surfaces("at PeekMessageA");   // the known vtable written over (rehook_surfaces)
        else if (!v.vt && !entry)
            entry = &v;
    if (!entry) {
        static bool told;
        if (!told)
            log("Replay: more than 8 vtables of the primary surface, the frame log ends at frame %u", g_frame);
        told = true;
        return;
    }
    DWORD old;
    if (!VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &old)) {
        log("Replay: the surface's vtable cannot be written, no frame log");
        return;
    }
    const bool again = g_flip_hooked;
    entry->vt = vt;
    entry->real = *slot;
    *slot = (DWORD)(uintptr_t)&my_Flip;
    VirtualProtect(slot, 4, old, &old);
    if (again)
        log("Replay: the primary surface %x with another vtable %x (Flip %x) at frame %u, events %u: hooked", primary, vt,
            entry->real, g_frame, g_events);
    else
        log("Replay: frame log from frame %u (Flip of the primary surface, events so far %u)", g_frame, g_events);
    g_flip_hooked = true;
    patch_ddraw(rd(glob_addr(0x713334)));
    patch_surface(primary);
    patch_surface(rd(glob_addr(0x713338)));
    if (g_v8_events && g_mode != LIVE) {
        hook_surfaces(primary);
        hook_surfaces(rd(glob_addr(0x713338)));
    }
    if (!again)
        hook_streams();
}

// DirectDraw's other methods the game calls, wrapped so the vtables are checked right after each call (after_call):
// the slot, our wrapper and the method that was there
struct Patch {
    DWORD vt, slot, mine, real;
};
Patch g_patch[32];
DWORD patch_real(DWORD self, DWORD slot)
{
    const DWORD vt = rd(self);
    for (const Patch &p : g_patch)
        if (p.vt == vt && p.slot == slot)
            return p.real;
    return 0;
}
// The slot set to our wrapper (an entry made the first time); true if it held something else
bool patch_slot(DWORD vt, DWORD slot, DWORD mine)
{
    Patch *e = nullptr;
    for (Patch &p : g_patch)
        if (p.vt == vt && p.slot == slot) {
            e = &p;
            break;
        } else if (!p.vt && !e)
            e = &p;
    DWORD *s = at<DWORD *>(vt + slot * 4);
    if (!e || *s == mine)
        return false;
    DWORD old;
    if (!VirtualProtect(s, 4, PAGE_EXECUTE_READWRITE, &old))
        return false;
    e->vt = vt;
    e->slot = slot;
    e->mine = mine;
    e->real = *s;
    *s = mine;
    VirtualProtect(s, 4, old, &old);
    return true;
}
void after_dd_call(const char *what, DWORD slot)
{
    if (GetCurrentThreadId() != g_main_thread || !g_flip_hooked)
        return;
    char where[48];
    wsprintfA(where, "after %s slot %u", what, slot);
    rehook_surfaces(where);
}
// K: 0 IDirectDraw, 1 IDirectDrawSurface
template <int K, int S, typename... A> DWORD WINAPI after_call(DWORD self, A... a)
{
    using F = DWORD(WINAPI *)(DWORD, A...);
    const DWORD r = reinterpret_cast<F>(static_cast<uintptr_t>(patch_real(self, S)))(self, a...);
    after_dd_call(K ? "the surface's" : "DirectDraw's", S);
    return r;
}
// IDirectDraw (v1): 6 CreateSurface, 20 SetCooperativeLevel, 21 SetDisplayMode; IDirectDrawSurface: 5 Blt, 12
// GetAttachedSurface (display_ddraw.cpp's list of the slots the game uses; Flip, IsLost, Lock and Restore have their own)
void patch_ddraw(DWORD dd)
{
    if (!dd)
        return;
    const DWORD vt = rd(dd);
    patch_slot(vt, 6, (DWORD)(uintptr_t)ptr(&after_call<0, 6, DWORD, DWORD, DWORD>));
    patch_slot(vt, 20, (DWORD)(uintptr_t)ptr(&after_call<0, 20, DWORD, DWORD>));
    patch_slot(vt, 21, (DWORD)(uintptr_t)ptr(&after_call<0, 21, DWORD, DWORD, DWORD>));
}
void patch_surface(DWORD surface)
{
    if (!surface)
        return;
    const DWORD vt = rd(surface);
    patch_slot(vt, 5, (DWORD)(uintptr_t)ptr(&after_call<1, 5, DWORD, DWORD, DWORD, DWORD, DWORD>));
    patch_slot(vt, 12, (DWORD)(uintptr_t)ptr(&after_call<1, 12, DWORD, DWORD>));
}

// The module a code address lies in, by its file name without the path ("?" when none)
const char *module_name(DWORD a, char *buf, DWORD size)
{
    DWORD base;
    module_offset(a, &base);
    if (!base || !GetModuleFileNameA(reinterpret_cast<HINSTANCE>(static_cast<uintptr_t>(base)), buf, size))
        return "?";
    const char *name = buf;
    for (const char *p = buf; *p; p++)
        if (*p == '\\' || *p == '/')
            name = p + 1;
    return name;
}

// The vtables patched so far (Flip; IsLost, Lock, Restore; the wrapped methods), patched again where something else
// wrote them over. On the x86 VMs' Windows 11 (WoW64) the compatibility layer (apphelp.dll, which hooks DirectDraw's
// COM methods of Zax.exe) writes its hooks back into the surface vtable inside IDirectDraw::CreateSurface (slot 6) when
// the game makes its surfaces again for another display mode (the mode set 0x567f20: the multiplayer's 640 x 480
// after the 800 x 600 menu) [verified: VM, 2026-11-01, docs/DECOMPILATION.md 12; a diagnostic thread stopped the main
// thread at the write: inside CreateSurface under 0x567f20]. Until then Flip no longer reached my_Flip from there on: the
// mode set's own page flips and every later frame went unmarked, the frame log ended while the game played on, a
// replay on another machine met the next frame mark where the game called on (0x47fe9a, the client's
// CoCreateInstance of DirectPlay8Client right after the mode set). The ARM VMs' Windows has no such hook. Checked right
// after each wrapped DirectDraw call (after_call; CreateSurface is the one that writes), after Flip, before every
// recorded or replayed event and at every PeekMessageA; logged when found.
void rehook_surfaces(const char *where)
{
    int n = 0;
    DWORD found = 0;
    for (FlipVt &v : g_flipvt) {
        if (!v.vt)
            continue;
        DWORD *slot = at<DWORD *>(v.vt + 11 * 4);
        if (*slot == (DWORD)(uintptr_t)&my_Flip)
            continue;
        DWORD old;
        if (!VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &old))
            continue;
        found = v.real = *slot;
        *slot = (DWORD)(uintptr_t)&my_Flip;
        VirtualProtect(slot, 4, old, &old);
        n++;
    }
    if (g_v8_events && g_mode != LIVE)
        for (DdVt &v : g_ddvt)
            if (v.vt && repatch_loss(v))
                n++;
    for (const Patch &p : g_patch)
        if (p.vt && patch_slot(p.vt, p.slot, p.mine))
            n++;
    if (!n)
        return;
    char buf[260];
    log("Replay: the surfaces' vtable written over (%u slot groups; Flip now %x in %s), patched again %s at frame %u, event "
        "%u, after the call from %x", n, found, found ? module_name(found, buf, sizeof buf) : "-", where, g_frame, g_events,
        g_caller);
}

// Before every event of the main thread: a cheap test of the patched slots, rehook_surfaces when one changed
void check_surfaces()
{
    if (!g_flip_hooked || GetCurrentThreadId() != g_main_thread)
        return;
    for (const FlipVt &v : g_flipvt)
        if (v.vt && *at<const DWORD *>(v.vt + 11 * 4) != (DWORD)(uintptr_t)&my_Flip)
            return rehook_surfaces("before an event");
    for (const DdVt &v : g_ddvt)
        if (v.vt && *at<const DWORD *>(v.vt + 24 * 4) != (DWORD)(uintptr_t)ptr(&my_IsLost) && g_v8_events && g_mode != LIVE)
            return rehook_surfaces("before an event");
    for (const Patch &p : g_patch)
        if (p.vt && *at<const DWORD *>(p.vt + p.slot * 4) != p.mine)
            return rehook_surfaces("before an event");
}

DWORD WINAPI my_DirectDrawCreate(void *guid, DWORD *out, void *outer)
{
    using F = DWORD(WINAPI *)(void *, DWORD *, void *);
    const DWORD r = REAL(S_DDCREATE, F)(guid, out, outer);
    if (GetCurrentThreadId() == g_main_thread && g_flip_hooked && !r && out) {
        patch_ddraw(*out);
        rehook_surfaces("after DirectDrawCreate");
    }
    return r;
}

// The game's main thread: the other thread of the process while DllMain runs; in the standalone exe the thread of its
// entry, which goes on to WinMainCRTStartup (as api_trace.cpp's)
DWORD find_main_thread()
{
#ifdef ZAX_STANDALONE
    return GetCurrentThreadId();
#endif
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    THREADENTRY32 e;
    e.dwSize = sizeof e;
    DWORD found = 0;
    const DWORD self = GetCurrentThreadId(), process = GetCurrentProcessId();
    for (BOOL ok = Thread32First(snap, &e); ok; ok = Thread32Next(snap, &e))
        if (e.th32OwnerProcessID == process && e.th32ThreadID != self && !found)
            found = e.th32ThreadID;
    CloseHandle(snap);
    return found;
}

bool load_recording(const char *path)
{
    HANDLE f = CreateFileA(path, 0x80000000u /* GENERIC_READ */, FILE_SHARE_READ, nullptr, 3 /* OPEN_EXISTING */, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    const DWORD size = GetFileSize(f, nullptr);
    g_in = static_cast<BYTE *>(VirtualAlloc(nullptr, size + 16, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    DWORD got = 0;
    const bool ok = g_in && ReadFile(f, g_in, size, &got, nullptr) && got == size && size >= 8 && !memcmp(g_in, MAGIC, 8);
    CloseHandle(f);
    g_in_size = size;
    g_in_pos = 8;
    if (ok && size >= 15 && g_in[8] == E_VERSION) {
        DWORD v;
        memcpy(&v, g_in + 11, 4);
        g_stream_events = v >= 5;
        g_v6_events = v >= 6;
        g_v7_events = v >= 7;
        g_v8_events = v >= 8;
        g_in_pos = 15;
        g_events++;
    }
    return ok;
}

}  // namespace

}  // namespace zax::replay

namespace zax {

DWORD g_pool_log_size;   // PoolLog's size class (pool.cpp asks it before calling pool_line), 0 while not logging

// Called by DllMain after the logged start's recorders and before the hooks (and with /Original)
void replay_install(const char *ini)
{
    using namespace replay;
    const bool record = GetPrivateProfileIntA("Reimpl", "Record", 0, ini) != 0;
    const bool play = GetPrivateProfileIntA("Reimpl", "Replay", 0, ini) != 0;
    g_frame_log = record || play || GetPrivateProfileIntA("Reimpl", "FrameLog", 0, ini) != 0;
    g_keep_saves = GetPrivateProfileIntA("Reimpl", "KeepSaves", 0, ini) != 0;
    g_screen_grid = GetPrivateProfileIntA("Reimpl", "ScreenGrid", 0, ini) != 0;
    g_world_log_frame = GetPrivateProfileIntA("Reimpl", "WorldLogFrame", 0, ini);
    g_world_log_frames = GetPrivateProfileIntA("Reimpl", "WorldLogFrames", 1, ini);
    g_heap_log = (int)GetPrivateProfileIntA("Reimpl", "HeapLog", 0, ini);
    g_pool_log = (int)GetPrivateProfileIntA("Reimpl", "PoolLog", 0, ini);
    g_pool_log_from = GetPrivateProfileIntA("Reimpl", "PoolLogFrom", 0, ini);
    g_pool_stats = (GetPrivateProfileIntA("Reimpl", "PoolStats", 0, ini) + 3) & ~3u;
    g_pool_log_size = g_pool_log > 0 ? (GetPrivateProfileIntA("Reimpl", "PoolLogSize", 0, ini) + 3) & ~3u : 0;
    if (GetPrivateProfileIntA("Reimpl", "FixedHeap", record || play, ini)) {
        reserve_heap();
        g_heap_start = g_fixed_heap && GetPrivateProfileIntA("Reimpl", "HeapStartApart", 1, ini) != 0;
    }
    if (!g_frame_log)
        return;
    lstrcpyA(g_dir, ini);
    for (int n = lstrlenA(g_dir); n > 0 && g_dir[n - 1] != '\\'; n--)
        g_dir[n - 1] = 0;
    g_main_thread = find_main_thread();
    if (!g_main_thread) {
        log("Replay: main thread not found, not installed");
        return;
    }
    char path[MAX_PATH + 32];
    if (play) {
        lstrcpyA(path, g_dir);
        lstrcatA(path, "Zax-Replay.rec");
        if (!load_recording(path)) {
            log("Replay: %s missing or not a recording, not installed", path);
            return;
        }
        g_mode = REPLAY;
    } else if (record) {
        g_out = static_cast<BYTE *>(VirtualAlloc(nullptr, OUT_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!g_out) {
            log("Replay: no memory for the recording, not installed");
            return;
        }
        g_mode = RECORD;
        g_stream_events = true;
        g_v6_events = true;
        g_v7_events = true;
        g_v8_events = true;
        put(E_VERSION, VERSION, nullptr, 0);
    }
    g_heap_tail_mark = g_v7_events;
    if (g_v7_events) {   // the network (version 7)
        fill_dp_entries();
        fill_fake_vtables();
    }
    // FrameLog alone still needs the PeekMessageA replacement (it patches Flip)
    fill_slots();
    for (int i = 0; i < NSLOT; i++) {
        DWORD *slot = imp_slot<DWORD *>(g_slots[i].iat);
        g_slots[i].real = *slot;
        DWORD old;
        VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &old);
        *slot = (DWORD)(uintptr_t)g_slots[i].replacement;
        VirtualProtect(slot, 4, old, &old);
    }
    log("Replay: %s, main thread %u", g_mode == RECORD ? "recording (Rec lines)" : g_mode == REPLAY ? "replaying Zax-Replay.rec" : "frame log only",
        g_main_thread);
    if (g_mode == REPLAY)
        log("Replay: %u bytes of events", g_in_size - 8);
}

// pool.cpp: a pool call while g_pool_log_size is set (PoolLog, above); the calls of the logged size class
void pool_line(char op, DWORD n, const void *p)
{
    using namespace replay;
    if (g_pool_log <= 0)
        g_pool_log_size = 0;
    else if (((n + 3) & ~3u) == g_pool_log_size && g_frame >= g_pool_log_from)
        pool_line_impl(op, n, p);
}

// DllMain at the process's end
void replay_flush()
{
    replay::flush_out();
}

}  // namespace zax
