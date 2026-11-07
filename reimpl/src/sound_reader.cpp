// The sound readers 0x57c980-0x57df07 and 0x525410-0x525760 (phase 4, sound): the objects that give a
// sound file's PCM to a sound resource or a stream, the pools of their 100,000-byte stream buffers and
// of the Ogg Vorbis states, and the two ring buffer readers of the streams. The game parses RIFF/WAVE
// itself; compressed WAVE data (ADPCM) is converted by ACM (msacm32), Ogg by vorbisfile (section 11).
//
// Readers [confirmed: the factory 0x57ca10, the vtables]: six slots: 0 Open (file buffer) -> bool,
// 1 deleting destructor, 2 Read (buffer, bytes) -> bytes, 3 Tell, 4 Seek (position), 5 Close.
//   The base (0x18, vtable 0x600bfc, constructor 0x57d790): +4 the PCM format (WAVEFORMATEX, 0x12 bytes),
//     +8 the file buffer (CFileBuf), +0xc the PCM bytes, +0x10 the data bytes of the file, +0x14 a
//     stream buffer of the pool (taken by the ring buffer readers).
//   Raw PCM (0x18, vtable 0x600bcc): the whole file as 22,050 Hz mono 16-bit.
//   WAVE (0x2c, vtable 0x600be4): +0x18 the ACM stream (HACMSTREAM, none for PCM), +0x1c/+0x20 the
//     buffer of the compressed bytes and its size, +0x24 the PCM bytes given, +0x28 the file's bytes
//     taken.
//   Ogg Vorbis (0x20, vtable 0x5fba24, constructor 0x525410; its slots are the library side,
//     OggStream_* 0x5259b0-0x525ba0 in sound_ogg_c5.cpp): +0x18 an OggVorbis_File (0x2c0 bytes) of its own pool, +0x1c open,
//     +0x1d reading.
// The factory takes a file that starts with "RIFF" ... "WAVE" as WAVE; otherwise one whose extension
// (Path_Extension, with its dot) is "ogg" as raw PCM and every other as Ogg Vorbis, so for the game's
// files .wav is WAVE and .ogg Ogg. The raw reader is never reached [confirmed: code 0x57ca10 and Path_Extension
// 0x4a3f80: the extension starts with its dot or is empty, so it never equals "ogg"; docs/DECOMPILATION.md 9, "The
// readers"].
//
// The pools (function-local statics: guard bit 0x716078 / 0x7160c0): a pointer array of blocks
// (0x716080 / 0x7160e0, vtable 0x600c20 / 0x5fba54) and a byte array of their "free" flags (0x716090 /
// 0x7160f0, vtable 0x5fba60); the first use of a stream buffer anywhere takes three and gives them back
// (0x713704 / 0x6d0164), so three buffers exist from then on.
#include "reimpl.h"
#include "exe_base.h"
#include "x87.h"

namespace zax {

namespace {

template <typename T> inline T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
inline DWORD &dw(void *p, DWORD off) { return fl<DWORD>(p, off); }
inline BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
inline DWORD ptr_value(const void *p) { return (DWORD)(uintptr_t)p; }

constexpr DWORD VT_BASE = 0x600bfc, VT_RAW = 0x600bcc, VT_WAVE = 0x600be4, VT_OGG = 0x5fba24;
constexpr DWORD BUFFERS = 0x716080, BUFFERS_GUARD = 0x716078, BUFFERS_WARM = 0x713704, BUFFER_SIZE = 100000;
constexpr DWORD OGG_STATES = 0x7160e0, OGG_GUARD = 0x7160c0, OGG_WARM = 0x6d0164, OGG_STATE_SIZE = 0x2c0;
constexpr DWORD FOURCC_RIFF = 0x46464952, FOURCC_WAVE = 0x45564157, FOURCC_FMT = 0x20746d66, FOURCC_DATA = 0x61746164,
                FOURCC_FACT = 0x74636166;

// The file buffer (core: file_buffer.cpp)
DWORD fb_read(void *fb, void *buf, DWORD n) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(fb, buf, n); }
DWORD fb_seek(void *fb, DWORD pos) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a20e0)(fb, pos); }
DWORD fb_tell(void *fb) { return fn<DWORD(THISCALL *)(void *)>(0x4a2180)(fb); }
DWORD fb_size(void *fb) { return fn<DWORD(THISCALL *)(void *)>(0x4a2190)(fb); }
DWORD fb_skip(void *fb, int n) { return fn<DWORD(THISCALL *)(void *, int)>(0x4a20c0)(fb, n); }
void fb_delete(void *fb) { vcall<void *>(fb, 0, 1); }

// ACM (msacm32 through the import thunks; phase 5)
DWORD acm_close(DWORD h) { return import_thunk<DWORD(__stdcall *)(DWORD, DWORD)>(0x5caa58, 0x5ea1bc)(h, 0); }
DWORD acm_suggest(void *had, void *src, void *dst, DWORD n, DWORD flags)
{
    return import_thunk<DWORD(__stdcall *)(void *, void *, void *, DWORD, DWORD)>(0x5caa64, 0x5ea1c4)(had, src, dst, n, flags);
}
DWORD acm_open(DWORD *h, void *had, void *src, void *dst, void *filter, DWORD cb, DWORD inst, DWORD flags)
{
    return import_thunk<DWORD(__stdcall *)(DWORD *, void *, void *, void *, void *, DWORD, DWORD, DWORD)>(0x5caa5e, 0x5ea1c0)(h, had, src, dst, filter, cb, inst,
                                                                                                         flags);
}
DWORD acm_size(DWORD h, DWORD n, DWORD *out, DWORD flags) { return import_thunk<DWORD(__stdcall *)(DWORD, DWORD, DWORD *, DWORD)>(0x5caa7c, 0x5ea1c8)(h, n, out, flags); }
DWORD acm_prepare(DWORD h, void *hdr, DWORD f) { return import_thunk<DWORD(__stdcall *)(DWORD, void *, DWORD)>(0x5caa76, 0x5ea1b8)(h, hdr, f); }
DWORD acm_convert(DWORD h, void *hdr, DWORD f) { return import_thunk<DWORD(__stdcall *)(DWORD, void *, DWORD)>(0x5caa70, 0x5ea1cc)(h, hdr, f); }
DWORD acm_unprepare(DWORD h, void *hdr, DWORD f) { return import_thunk<DWORD(__stdcall *)(DWORD, void *, DWORD)>(0x5caa6a, 0x5ea1d0)(h, hdr, f); }

}  // namespace

// ---- The pools ------------------------------------------------------------------------------------------------

namespace {

// 0x57ced0: the pointer array's constructor (vtable 0x600c20)
BYTE *THISCALL Buffers_ArrayCtor(BYTE *a)
{
    dw(a, 4) = 0;
    dw(a, 8) = 0;
    dw(a, 0xc) = 0;
    dw(a, 0) = vt(0x600c20);
    return a;
}
// 0x525740 and 0x525760: the Ogg states' pointer array (0x5fba54) and both pools' flag arrays (0x5fba60)
BYTE *THISCALL OggStates_ArrayCtor(BYTE *a)
{
    dw(a, 4) = 0;
    dw(a, 8) = 0;
    dw(a, 0xc) = 0;
    dw(a, 0) = vt(0x5fba54);
    return a;
}
BYTE *THISCALL Flags_ArrayCtor(BYTE *a)
{
    dw(a, 4) = 0;
    dw(a, 8) = 0;
    dw(a, 0xc) = 0;
    dw(a, 0) = vt(0x5fba60);
    return a;
}

// 0x57cd40 and 0x5255e0 (the pool): the first free block, or a new one (operator new) appended with its
// flag cleared
template <DWORD SIZE> DWORD THISCALL Pool_Take(BYTE *pool)
{
    DWORD i = 0;
    const DWORD n = dw(pool, 8);
    for (; i < n; i++)
        if (bp(dw(pool, 0x14))[i]) {
            bp(dw(pool, 0x14))[i] = 0;
            return reinterpret_cast<DWORD *>(bp(dw(pool, 4)))[i];
        }
    void *block = heap_alloc(SIZE);
    vcall<DWORD>(pool, 2, &block);
    BYTE flag = 0;
    vcall<DWORD>(pool + 0x10, 2, &flag);
    return reinterpret_cast<DWORD *>(bp(dw(pool, 4)))[i];
}
// 0x57cdb0 (block): its flag set (free); a block that is not in the pool is ignored
void THISCALL Pool_Give(BYTE *pool, DWORD block)
{
    const DWORD n = dw(pool, 8);
    const DWORD *data = reinterpret_cast<const DWORD *>(bp(dw(pool, 4)));
    for (DWORD i = 0; i < n; i++)
        if (data[i] == block) {
            bp(dw(pool, 0x14))[i] = 1;
            return;
        }
}

// The first use of a pool in a function: the static's construction (with its atexit), then the warm-up
void buffers_ready()
{
    BYTE &guard = *glob<BYTE *>(BUFFERS_GUARD);
    if (!(guard & 1)) {
        guard |= 1;
        Buffers_ArrayCtor(glob<BYTE *>(BUFFERS));
        Flags_ArrayCtor(at<BYTE *>(glob_addr(BUFFERS) + 0x10));
        crt_atexit(code<void(__cdecl *)()>(0x57cde0));
    }
    if (!*glob<BYTE *>(BUFFERS_WARM)) {
        *glob<BYTE *>(BUFFERS_WARM) = 1;
        BYTE *pool = glob<BYTE *>(BUFFERS);
        const DWORD a = Pool_Take<BUFFER_SIZE>(pool), b = Pool_Take<BUFFER_SIZE>(pool), c = Pool_Take<BUFFER_SIZE>(pool);
        Pool_Give(pool, c);
        Pool_Give(pool, b);
        Pool_Give(pool, a);
    }
}
void ogg_states_ready()
{
    BYTE &guard = *glob<BYTE *>(OGG_GUARD);
    if (!(guard & 1)) {
        guard |= 1;
        OggStates_ArrayCtor(glob<BYTE *>(OGG_STATES));
        Flags_ArrayCtor(at<BYTE *>(glob_addr(OGG_STATES) + 0x10));
        crt_atexit(code<void(__cdecl *)()>(0x525650));
    }
    if (!*glob<BYTE *>(OGG_WARM)) {
        *glob<BYTE *>(OGG_WARM) = 1;
        BYTE *pool = glob<BYTE *>(OGG_STATES);
        const DWORD a = Pool_Take<OGG_STATE_SIZE>(pool), b = Pool_Take<OGG_STATE_SIZE>(pool), c = Pool_Take<OGG_STATE_SIZE>(pool);
        Pool_Give(pool, c);
        Pool_Give(pool, b);
        Pool_Give(pool, a);
    }
}
// A reader's stream buffer back to the pool (+0x14 stays)
void give_buffer(BYTE *r)
{
    if (!dw(r, 0x14))
        return;
    buffers_ready();
    Pool_Give(glob<BYTE *>(BUFFERS), dw(r, 0x14));
}
// A reader's stream buffer, taken on first use
BYTE *stream_buffer(BYTE *r)
{
    if (!dw(r, 0x14)) {
        buffers_ready();
        dw(r, 0x14) = Pool_Take<BUFFER_SIZE>(glob<BYTE *>(BUFFERS));
    }
    return bp(dw(r, 0x14));
}

// ---- The base reader ------------------------------------------------------------------------------------------

// The base's part of Close and of the destructors: the stream buffer back, the format and the file freed
void base_release(BYTE *r)
{
    give_buffer(r);
    if (void *f = bp(dw(r, 4))) {
        pool_free(f, 0x12);
        dw(r, 4) = 0;
    }
    if (void *fb = bp(dw(r, 8))) {
        fb_delete(fb);
        dw(r, 8) = 0;
    }
}

// 0x57d790
BYTE *THISCALL Reader_Ctor(BYTE *r)
{
    dw(r, 0) = vt(VT_BASE);
    dw(r, 4) = 0;
    dw(r, 8) = 0;
    dw(r, 0x14) = 0;
    return r;
}
// 0x57cf40 (slot 1 of the base and the raw reader), 0x57d110 (the destructor without the free)
BYTE *THISCALL Reader_DeletingDtor(BYTE *r, DWORD flags)
{
    dw(r, 0) = vt(VT_BASE);
    base_release(r);
    if (flags & 1)
        pool_free(r, 0x18);
    return r;
}
void THISCALL Reader_Dtor(BYTE *r)
{
    dw(r, 0) = vt(VT_BASE);
    base_release(r);
}
// 0x57d7b0 (slot 5 of the base and the raw reader)
void THISCALL Reader_Close(BYTE *r) { base_release(r); }
// 0x57d2f0 (slot 2): the file's bytes
DWORD THISCALL Reader_Read(BYTE *r, void *buf, DWORD n) { return fb_read(bp(dw(r, 8)), buf, n); }
// 0x57d320 (slot 4)
DWORD THISCALL Reader_Seek(BYTE *r, DWORD pos) { return fb_seek(bp(dw(r, 8)), pos); }
// 0x57d2a0 (out): the PCM format (PCM tag, cbSize 0)
void THISCALL Reader_Format(BYTE *r, BYTE *out)
{
    fl<WORD>(out, 0) = 1;
    fl<WORD>(out, 2) = fl<WORD>(bp(dw(r, 4)), 2);
    dw(out, 4) = dw(bp(dw(r, 4)), 4);
    dw(out, 8) = dw(bp(dw(r, 4)), 8);
    fl<WORD>(out, 0xc) = fl<WORD>(bp(dw(r, 4)), 0xc);
    fl<WORD>(out, 0x10) = 0;
    fl<WORD>(out, 0xe) = fl<WORD>(bp(dw(r, 4)), 0xe);
}
// 0x57d0e0: the length in seconds (the PCM bytes, unsigned, by the average bytes per second)
long double THISCALL Reader_Seconds(BYTE *r) { return fild64((long long)dw(r, 0xc)) / fild((int)dw(bp(dw(r, 4)), 8)); }

// 0x57d330 (first, first bytes, second, second bytes): a read into the two parts of a locked ring buffer:
// without a second part straight into the first, else through the stream buffer; eax: the read's
DWORD THISCALL Reader_ReadParts(BYTE *r, BYTE *first, DWORD n1, BYTE *second, DWORD n2)
{
    if (!second || !n2)
        return vcall<DWORD>(r, 2, first, n1);
    BYTE *temp = stream_buffer(r);
    const DWORD got = vcall<DWORD>(r, 2, temp, n1 + n2);
    memcpy(first, bp(dw(r, 0x14)), n1);
    memcpy(second, bp(dw(r, 0x14)) + n1, n2);
    return got;
}
// One region of the mix: out = (in - out) x (1 - gain) + out, truncated (__ftol), the low 16 bits with
// -32768 made -32767
inline void mix_sample(short *out, const short *in, long double factor)
{
    const int a = *out, diff = *in - a;
    short v = (short)x87_ftol(fild(diff) * factor + fild(a));
    if (v == -32768)
        v = -32767;
    *out = v;
}
// 0x57d550 (first, first bytes, second, second bytes, gain): the next bytes mixed into what the two
// parts hold (the cross-fade); the first part with the factor 1 - gain on the FPU, the second with it
// stored as a float; eax: the read's
DWORD THISCALL Reader_ReadMix(BYTE *r, BYTE *first, DWORD n1, BYTE *second, DWORD n2, float gain)
{
    BYTE *temp = stream_buffer(r);
    const DWORD got = vcall<DWORD>(r, 2, temp, n1 + n2);
    const short *in = reinterpret_cast<const short *>(bp(dw(r, 0x14)));
    short *out = reinterpret_cast<short *>(first);
    if (n1 >> 1) {
        const long double factor = cst(glob_addr(C_ONE)) - fld(gain);
        for (DWORD i = 0; i < (n1 >> 1); i++)
            mix_sample(out + i, in + i, factor);
    }
    in = reinterpret_cast<const short *>(bp(dw(r, 0x14)) + n1);
    out = reinterpret_cast<short *>(second);
    if (n2 >> 1) {
        const float factor = fstp(cst(glob_addr(C_ONE)) - fld(gain));
        for (DWORD i = 0; i < (n2 >> 1); i++)
            mix_sample(out + i, in + i, fld(factor));
    }
    return got;
}

// ---- Raw PCM ---------------------------------------------------------------------------------------------------

// 0x57c980 (slot 0: file buffer): the whole file as 22,050 Hz mono 16-bit PCM (the format from operator
// new)
bool THISCALL RawReader_Open(BYTE *r, void *fb)
{
    vcall<void>(r, 5);
    dw(r, 8) = ptr_value(fb);
    const DWORD n = fb_size(fb);
    dw(r, 0xc) = n;
    dw(r, 0x10) = n;
    BYTE *f = static_cast<BYTE *>(heap_alloc(0x12));
    dw(r, 4) = ptr_value(f);
    memset(f, 0, 0x12);
    fl<WORD>(bp(dw(r, 4)), 0) = 1;
    fl<WORD>(bp(dw(r, 4)), 2) = 1;
    dw(bp(dw(r, 4)), 4) = 22050;
    fl<WORD>(bp(dw(r, 4)), 0xe) = 16;
    f = bp(dw(r, 4));
    fl<WORD>(f, 0xc) = (WORD)(fl<WORD>(f, 0xe) >> 3);
    f = bp(dw(r, 4));
    dw(f, 8) = (DWORD)fl<WORD>(f, 0xc) * dw(f, 4);
    return true;
}

// ---- WAVE --------------------------------------------------------------------------------------------------------

// The WAVE reader's own part of Close and of its destructor: the ACM stream closed, the compressed bytes'
// buffer freed
void wave_release(BYTE *r)
{
    base_release(r);
    if (dw(r, 0x18)) {
        acm_close(dw(r, 0x18));
        dw(r, 0x18) = 0;
    }
    if (void *b = bp(dw(r, 0x1c))) {
        heap_free(b);
        dw(r, 0x1c) = 0;
    }
}

// 0x57d930 (slot 0: file buffer): RIFF/WAVE: the chunks "fmt " (the format into a pool block), "data"
// (its position and size) and "fact" (the samples) up to the RIFF size, until data and format are known;
// PCM gives the data as it is, other formats an ACM stream to PCM (acmFormatSuggest with the tag, then
// acmStreamOpen; the PCM size from the samples of "fact")
bool THISCALL WaveReader_Open(BYTE *r, void *fb)
{
    dw(r, 0x18) = 0;
    dw(r, 0x1c) = 0;
    dw(r, 0x20) = 0;
    dw(r, 0x24) = 0;
    dw(r, 0x28) = 0;
    vcall<void>(r, 5);
    dw(r, 8) = ptr_value(fb);
    DWORD riff, size, fact, id, length;
    fb_read(bp(dw(r, 8)), &riff, 4);
    fb_read(bp(dw(r, 8)), &size, 4);
    DWORD data_at = 0;
    fact = 0;
    fb_read(bp(dw(r, 8)), &riff, 4);
    dw(r, 4) = 0;
    bool found = false;
    for (;;) {
        if (found && dw(r, 4))
            break;
        if (fb_tell(bp(dw(r, 8))) >= size)
            break;
        fb_read(bp(dw(r, 8)), &id, 4);
        fb_read(bp(dw(r, 8)), &length, 4);
        const DWORD at_ = fb_tell(bp(dw(r, 8)));
        if (id == FOURCC_FMT) {
            void *f = pool_alloc(0x12);
            dw(r, 4) = ptr_value(f);
            fb_read(bp(dw(r, 8)), f, 0x12);
            fl<WORD>(bp(dw(r, 4)), 0x10) = 0;
        } else if (id == FOURCC_DATA) {
            data_at = fb_tell(bp(dw(r, 8)));
            dw(r, 0x10) = length;
            found = true;
        } else if (id == FOURCC_FACT) {
            fb_read(bp(dw(r, 8)), &fact, 4);
        }
        if (at_ + length < size)
            fb_seek(bp(dw(r, 8)), at_ + length);
    }
    if (!found || !dw(r, 4))
        return false;
    fb_seek(bp(dw(r, 8)), data_at);
    BYTE *src = bp(dw(r, 4));
    if (fl<WORD>(src, 0) == 1) {
        dw(r, 0xc) = dw(r, 0x10);
        return true;
    }
    BYTE *pcm = static_cast<BYTE *>(heap_alloc(0x12));
    dw(r, 4) = ptr_value(pcm);
    memset(pcm, 0, 0x12);
    fl<WORD>(bp(dw(r, 4)), 0) = 1;
    if (acm_suggest(nullptr, src, bp(dw(r, 4)), 0x12, 0x10000)) {
        vcall<void>(r, 5);
        return false;
    }
    if (acm_open(&dw(r, 0x18), nullptr, src, bp(dw(r, 4)), nullptr, 0, 0, 0)) {
        vcall<void>(r, 5);
        return false;
    }
    heap_free(src);
    BYTE *f = bp(dw(r, 4));
    dw(r, 0xc) = (DWORD)(fl<WORD>(f, 0xe) >> 3) * (DWORD)fl<WORD>(f, 2) * fact;
    return true;
}

// 0x57db50 (slot 2: buffer, bytes): PCM straight from the file; with ACM the source bytes for the wanted
// PCM bytes (acmStreamSize; the rest of the file for the last part), converted in one header (start,
// block align, end flags); bytes the conversion did not take are given back to the file; at the end of
// the file the PCM still missing is silence
DWORD THISCALL WaveReader_Read(BYTE *r, BYTE *buf, DWORD n)
{
    if (!dw(r, 0x18))
        return fb_read(bp(dw(r, 8)), buf, n);
    DWORD want = n;
    if (dw(r, 0x24) + want > dw(r, 0xc))
        want = dw(r, 0xc) - dw(r, 0x24);
    if (!want)
        return 0;
    DWORD flags = dw(r, 0x24) ? 4 : 0x14;
    DWORD source = n;   // the exe keeps it in the argument's slot
    if (dw(r, 0x24) + want == dw(r, 0xc)) {
        source = dw(r, 0x10) - dw(r, 0x28);
        flags = (flags & ~4u) | 0x20;
    } else if (acm_size(dw(r, 0x18), want, &source, 1)) {
        source = want;
    }
    if (dw(r, 0x28) + source > dw(r, 0x10))
        source = dw(r, 0x10) - dw(r, 0x28);
    if (dw(r, 0x28) + source == dw(r, 0x10))
        flags = (flags & ~4u) | 0x20;
    if (!source)
        return 0;
    if (dw(r, 0x20) < source) {
        if (void *b = bp(dw(r, 0x1c))) {
            heap_free(b);
            dw(r, 0x1c) = 0;
        }
        dw(r, 0x20) = source;
        dw(r, 0x1c) = ptr_value(heap_alloc(source));
    }
    const DWORD got = fb_read(bp(dw(r, 8)), bp(dw(r, 0x1c)), source);
    DWORD h[0x15];
    memset(h, 0, sizeof h);
    h[0] = 0x54;
    h[3] = dw(r, 0x1c);
    h[4] = got;
    h[7] = ptr_value(buf);
    h[8] = want;
    if (acm_prepare(dw(r, 0x18), h, 0))
        return 0;
    if (acm_convert(dw(r, 0x18), h, flags))
        return 0;
    if (acm_unprepare(dw(r, 0x18), h, 0))
        return 0;
    const DWORD used = h[5];
    if (used != h[4])
        fb_skip(bp(dw(r, 8)), (int)(used - h[4]));
    dw(r, 0x28) += h[5];
    DWORD done = h[9];
    dw(r, 0x24) += done;
    if (dw(r, 0x28) == dw(r, 0x10) && dw(r, 0x24) < dw(r, 0xc)) {
        DWORD pad = dw(r, 0xc) - dw(r, 0x24);
        if (pad + done > want)
            pad = want - done;
        memset(buf + done, 0, pad);
        done += pad;
        dw(r, 0x24) += pad;
    }
    return done;
}
// 0x57dd70 (slot 5)
void THISCALL WaveReader_Close(BYTE *r) { wave_release(r); }
// 0x57cb40 (slot 1): its Close, then the base's (the stream buffer given back a second time, as in the exe)
BYTE *THISCALL WaveReader_DeletingDtor(BYTE *r, DWORD flags)
{
    dw(r, 0) = vt(VT_WAVE);
    wave_release(r);
    dw(r, 0) = vt(VT_BASE);
    base_release(r);
    if (flags & 1)
        pool_free(r, 0x2c);
    return r;
}

// 0x57ca10 (fastcall: name, open flags): the file opened (a CFileBuf of the pool, flags | 4), its kind
// from the first bytes or the extension, its reader opened; null when either fails
BYTE *__fastcall Reader_ForFile(const char *name, DWORD flags)
{
    BYTE *fb = static_cast<BYTE *>(pool_alloc(0x1c));
    if (fb)
        fb = fn<BYTE *(THISCALL *)(void *)>(0x4a1780)(fb);
    if (!fn<bool(THISCALL *)(void *, const char *, DWORD)>(0x4a1840)(fb, name, flags | 4)) {
        if (fb)
            vcall<void *>(fb, 0, 1);
        return nullptr;
    }
    DWORD riff, size, wave;
    fb_read(fb, &riff, 4);
    fb_read(fb, &size, 4);
    fb_read(fb, &wave, 4);
    fb_seek(fb, 0);
    BYTE *r;
    if (riff == FOURCC_RIFF && wave == FOURCC_WAVE) {
        r = static_cast<BYTE *>(pool_alloc(0x2c));
        if (r) {
            dw(r, 4) = 0;
            dw(r, 8) = 0;
            dw(r, 0x14) = 0;
            dw(r, 0) = vt(VT_WAVE);
        }
    } else if (crt_stricmp(fn<const char *(__fastcall *)(const char *)>(0x4a3f80)(name), glob<const char *>(0x6155e8 /* "ogg" */))) {
        r = static_cast<BYTE *>(pool_alloc(0x20));
        if (r)
            r = fn<BYTE *(THISCALL *)(void *)>(0x525410)(r);
    } else {
        r = static_cast<BYTE *>(pool_alloc(0x18));
        if (r) {
            dw(r, 4) = 0;
            dw(r, 8) = 0;
            dw(r, 0x14) = 0;
            dw(r, 0) = vt(VT_RAW);
        }
    }
    if (!vcall<bool>(r, 0, fb)) {
        if (r)
            vcall<void *>(r, 1, 1);
        return nullptr;
    }
    return r;
}

// ---- Ogg Vorbis -------------------------------------------------------------------------------------------------

// 0x525410: the base, its own vtable, closed; an OggVorbis_File of the pool; the callbacks
// (g_OggCallbacks 0x6d0150: read, seek, close, tell) filled on first use
BYTE *THISCALL OggReader_Ctor(BYTE *r)
{
    Reader_Ctor(r);
    dw(r, 0) = vt(VT_OGG);
    fl<BYTE>(r, 0x1c) = 0;
    fl<BYTE>(r, 0x1d) = 0;
    ogg_states_ready();
    dw(r, 0x18) = Pool_Take<OGG_STATE_SIZE>(glob<BYTE *>(OGG_STATES));
    if (!*glob<DWORD *>(0x6d0158)) {
        *glob<DWORD *>(0x6d0158) = fn_addr(0x525400);
        *glob<DWORD *>(0x6d0150) = fn_addr(0x525380);
        *glob<DWORD *>(0x6d0154) = fn_addr(0x5253a0);
        *glob<DWORD *>(0x6d015c) = fn_addr(0x57d310);
    }
    return r;
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(sound_reader)
{
    const Hook list[] = {
#include "sound_reader_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
