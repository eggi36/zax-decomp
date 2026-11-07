// Checks of sound_rest4b.cpp against the original, in the game thread (machinery: checks_inventory_common.h:
// side 0 with the module's hooks out). The callbacks run on a CFileBuf (the exe's constructor) over a memory
// stream of our own (a recording vtable); compared: eax, the bytes read, the stream's position, size and call
// log.
//  - Read: sizes 1-7 (and 0x1000), counts 0-300, positions inside, at and past the end.
//  - Seek: whence 0, 1, 2 and others, offsets inside and past the end, the high dword set.
//  - Close.
#include "checks_inventory_common.h"
#include "file_buffer.h"

namespace zax {

int hooks_sound_rest4b(Hook *out);

namespace {

using namespace invcheck;

DWORD rnd(DWORD n) { return n ? (random() >> 8) % n : 0; }

struct MemStream : ZStream {
    BYTE *data;
    DWORD capacity, size, pos;
    unsigned log;
};
void mrec(MemStream *s, DWORD slot, DWORD a, DWORD b)
{
    DWORD r[3] = {slot, a, b};
    s->log = fnv(s->log, r, sizeof r);
}
MemStream *THISCALL ms_delete(MemStream *s, DWORD f) { mrec(s, 0, f, 0); return s; }
DWORD THISCALL ms_unused(MemStream *s) { mrec(s, 1, 0, 0); return 0; }
DWORD THISCALL ms_write(MemStream *s, const void *, DWORD n) { mrec(s, 2, n, 0); return 0; }
DWORD THISCALL ms_read(MemStream *s, void *p, DWORD n)
{
    const DWORD rest = s->pos < s->size ? s->size - s->pos : 0, w = n < rest ? n : rest;
    memcpy(p, s->data + s->pos, w);
    s->pos += w;
    mrec(s, 3, n, w);
    return w;
}
bool THISCALL ms_skip(MemStream *s, DWORD d)
{
    const DWORD p = s->pos + d;
    const bool ok = p <= s->size;
    if (ok)
        s->pos = p;
    mrec(s, 4, d, ok);
    return ok;
}
bool THISCALL ms_seek(MemStream *s, DWORD p)
{
    const bool ok = p <= s->size;
    if (ok)
        s->pos = p;
    mrec(s, 5, p, ok);
    return ok;
}
DWORD THISCALL ms_tell(MemStream *s) { mrec(s, 6, 0, 0); return s->pos; }
DWORD THISCALL ms_size(MemStream *s) { mrec(s, 7, 0, 0); return s->size; }
DWORD THISCALL ms_set_size(MemStream *s, DWORD n) { mrec(s, 8, n, 0); return 0; }
DWORD THISCALL ms_close(MemStream *s) { mrec(s, 9, 0, 0); return 0; }
BYTE *THISCALL ms_advance(MemStream *s, DWORD n) { mrec(s, 11, n, 0); return nullptr; }
BYTE *THISCALL ms_detach(MemStream *s) { mrec(s, 12, 0, 0); return nullptr; }
bool THISCALL ms_read_line(MemStream *s, char *p, DWORD n)
{
    mrec(s, 13, n, 0);
    if (n)
        p[0] = 0;
    return false;
}
DWORD g_ms_vtable[14];
BYTE g_data[0x2000];
BYTE g_out[2][0x3000];

struct Source {
    DWORD a, b;
    CFileBuf *f;
};

void check_callbacks(Tally &t)
{
    void *f[14] = {(void *)&ms_delete, (void *)&ms_unused,   (void *)&ms_write,  (void *)&ms_read,     (void *)&ms_skip,
                   (void *)&ms_seek,   (void *)&ms_tell,     (void *)&ms_size,   (void *)&ms_set_size, (void *)&ms_close,
                   (void *)&ms_unused, (void *)&ms_advance,  (void *)&ms_detach, (void *)&ms_read_line};
    for (int i = 0; i < 14; i++)
        g_ms_vtable[i] = ptrv(f[i]);
    for (int k = 0; k < scaled(3000); k++) {
        const unsigned seed = 0x525380u + (unsigned)k;
        const int what = k % 7 == 6 ? 2 : k % 2;
        DWORD r[2], state[2][4];
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            for (DWORD i = 0; i < sizeof g_data; i++)
                g_data[i] = (BYTE)(random() >> 13);
            MemStream s;
            s.vftable = ptrv(g_ms_vtable);
            s.handle = nullptr;
            s.owned = 0;
            s.data = g_data;
            s.capacity = sizeof g_data;
            s.size = 0x100 + rnd(0x1f00);
            s.pos = rnd(4) ? rnd(s.size) : s.size;
            s.log = 2166136261u;
            alignas(4) BYTE fb[0x20];
            at<void *(THISCALL *)(void *)>(0x4a1780)(fb);
            *reinterpret_cast<DWORD *>(fb + 4) = ptrv(&s);
            Source src{random(), random(), reinterpret_cast<CFileBuf *>(fb)};
            memset(g_out[side], 0xcd, sizeof g_out[side]);
            side_begin(side);
            if (what == 0) {
                const DWORD size = rnd(9) ? 1 + rnd(7) : 0x1000, count = size == 0x1000 ? rnd(3) : rnd(301);
                r[side] = at<DWORD(__fastcall *)(void *, DWORD, DWORD, void *)>(0x525380)(g_out[side], size, count, &src);
            } else if (what == 1) {
                const int whence = rnd(6) ? (int)rnd(3) : (int)rnd(10) - 3;
                const DWORD off = rnd(4) ? rnd(s.size + 1) : rnd(0x3000), high = rnd(4) ? 0 : random();
                r[side] = at<DWORD(__fastcall *)(void *, int, DWORD, DWORD)>(0x5253a0)(&src, whence, off, high);
            } else {
                r[side] = at<DWORD(__fastcall *)(void *)>(0x525400)(&src);
            }
            side_end(side);
            state[side][0] = s.pos;
            state[side][1] = s.size;
            state[side][2] = s.log;
            state[side][3] = fnv(2166136261u, fb, sizeof fb - 4);
        }
        char d[160];
        wsprintfA(d, "[%s seed %u: eax %x / %x, pos %x / %x, log %x / %x]", what == 0 ? "Read" : what == 1 ? "Seek" : "Close", seed, r[0], r[1],
                  state[0][0], state[1][0], state[0][2], state[1][2]);
        t.check(r[0] == r[1] && !memcmp(state[0], state[1], sizeof state[0]) && !memcmp(g_out[0], g_out[1], sizeof g_out[0]), "", d);
    }
}

}  // namespace

ZAX_CHECKS(sound_rest4b)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_sound_rest4b;
    Tally t{"ogg callbacks: Read, Seek, Close"};
    check_callbacks(t);
    t.report();
}

}  // namespace zax
