// A sequence's properties file 0x56b320, the image sequence list and the debug sequence window 0x56c080-0x56c64e (docs/DECOMPILATION.md 9, "Phase 4
// remains, band C"). Every call goes through its exe address.
//
// The sequence list (0x158 bytes, vtable 0x5ff64c; its base, the resource list of records of 0x80 bytes, 0x140
// bytes with vtable 0x5ff6f0) [confirmed: the constructor 0x56c080 and the destructors]: the record list
// (RecordList_Ctor 0x591c90; +0x30 the records, count at -4, +0x34 the count, +0x38 the folder), +0x1c the
// resource aliases (vtable 0x5ed558, +0x2c -1), +0x138, +0x13c an object deleted through its slot 7, +0x140 the
// frame filters of the sequences it loads (FrameFilter 0x4b1cc0), +0x154 they are set. Its slot 37 (0x56c250)
// loads a CSequence (0x38 bytes) from a cache file with those filters, the game's defaults (0x56b260) the first
// time.
// The sequence window ("!DEBUG! <name>", 0x10c bytes, vtable 0x5ff794): a window over one sequence of a list
// (the sequence list 0x713380 by default) sized to the sequence's rectangle; +0x100 the sequence, +0x104 the
// current frame (float), +0x108 the sequence's frames per second (its slot 26). Advance moves the frame on by
// the elapsed seconds and wraps at the sequence's frame count (slot 25), Draw blits the current frame (slot 23)
// in the sequence's blend mode (0x56bf00).
#include "ui_screens.h"

namespace zax::seqlist {

using namespace ui;

inline void filter_ctor(void *f, DWORD kind) { fn<void(THISCALL *)(void *, DWORD)>(0x4b1cc0)(f, kind); }
inline void filter_dtor(void *f) { fn<void(THISCALL *)(void *)>(0x4b1d20)(f); }
inline void filter_clone_into(DWORD from, void *to) { fn<void(THISCALL *)(DWORD, void *)>(0x4b1d50)(from, to); }

// (folder, name): the object's "<folder>/-<name>/Properties.txt" read (the last opened file 0x713c14 set to it
// meanwhile), else its defaults (slot 19) written there; a file that can be neither read nor written ends the game
// ("Can't Read Or Save Properties")
void THISCALL Sequence_Properties(BYTE *self, const char *folder, const char *name)   // 0x56b320
{
    alignas(4) BYTE stream[0x24];
    char path[0x100];
    fn<void(THISCALL *)(BYTE *)>(0x5240b0)(stream);
    fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(path, glob<const char *>(0x618908), folder, name);
    using Open = BYTE(THISCALL *)(BYTE *, const char *, DWORD, DWORD);
    if (fn<Open>(0x5242b0)(stream, path, 0x204, 1)) {
        BYTE saved[4];
        zs_copy(saved, glob_addr(0x713c14));
        fn<void(THISCALL *)(DWORD, const char *)>(0x4e1930)(glob_addr(0x713c14), path);
        fn<void(THISCALL *)(BYTE *, BYTE *)>(0x416af0)(self, stream);
        zs_assign(glob<void *>(0x713c14), pv(saved));
        zs_dtor(saved);
    } else {
        vc(self, 19);
        if (!fn<Open>(0x5242b0)(stream, path, 3, 1)) {
            BYTE text[4];
            zs_ctor(text);
            zs_format(text, glob<const char *>(0x62171c), folder, name);
            fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(0x6216fc), c_str(text));
            fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
        }
        fn<void(THISCALL *)(BYTE *, BYTE *)>(0x4168f0)(self, stream);
        fn<void(THISCALL *)(BYTE *)>(0x524340)(stream);
    }
    fn<void(THISCALL *)(BYTE *)>(0x5241e0)(stream);
}

BYTE *THISCALL SequenceList_Ctor(BYTE *self)   // 0x56c080
{
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x591c90)(self, 0, 0x80, 0, 0, 0);
    m(self, 0x1c) = vt(0x5ed558);
    m(self, 0x20) = 0;
    m(self, 0x24) = 0;
    m(self, 0x28) = 0;
    m(self, 0x2c) = 0xffffffff;
    m(self, 0) = vt(0x5ff6f0);
    m(self, 0x30) = 0;
    m(self, 0x34) = 0;
    m(self, 0x138) = 0;
    m(self, 0x13c) = 0;
    filter_ctor(self + 0x140, 0);
    m(self, 0) = vt(0x5ff64c);
    mb(self, 0x154) = 0;
    return self;
}

// The resource list part: every record's object deleted (slot 1, from the last), the records freed, the list
// emptied (RecordList_Set 0x591cd0), +0x13c deleted (slot 7), the aliases destroyed
void records_dtor(BYTE *self)
{
    BYTE *recs = bp(m(self, 0x30));
    m(self, 0) = vt(0x5ff6f0);
    if (recs) {
        const int n = mi(recs, (DWORD)-4);
        for (int i = n - 1; i >= 0; i--) {
            BYTE *rec = recs + (DWORD)i * 0x80;
            if (BYTE *obj = bp(m(rec, 0x78))) {
                vc(obj, 1, 1);
                m(rec, 0x78) = 0;
            }
        }
        op_delete(recs - 4);
        m(self, 0x30) = 0;
    }
    m(self, 0x34) = 0;
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x591cd0)(self, 0, 0, 0, 0, 0);
    if (BYTE *o = bp(m(self, 0x13c))) {
        vc(o, 7, 1);
        m(self, 0x13c) = 0;
    }
    m(self, 0) = vt(0x5ed564);
    fn<void(THISCALL *)(BYTE *)>(0x55d0c0)(self + 0x1c);
    m(self, 0) = vt(0x5ec7cc);
}

BYTE *THISCALL SequenceList_DeletingDtor(BYTE *self, DWORD flags)   // 0x56c0e0
{
    m(self, 0) = vt(0x5ff64c);
    filter_dtor(self + 0x140);
    records_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x158);
    return self;
}

void THISCALL SequenceList_Dtor(BYTE *self)   // 0x56c1a0
{
    m(self, 0) = vt(0x5ff64c);
    filter_dtor(self + 0x140);
    records_dtor(self);
}

BYTE *THISCALL SequenceList_DeletingDtor2(BYTE *self, DWORD flags)   // 0x56c320 (the base's)
{
    records_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x140);
    return self;
}

// Slot 37 (folder, file, out record): a new CSequence (CSequence::Init 0x56b730) loaded from its cache file
// (0x56b950) with the list's filters
BYTE *THISCALL SequenceList_Load(BYTE *self, DWORD folder, DWORD file, DWORD *out)   // 0x56c250
{
    BYTE *seq = alloc(0x38);
    if (seq) {
        fn<void(THISCALL *)(BYTE *)>(0x416610)(seq);
        m(seq, 0) = vt(0x5f43f0);
        fn<void(THISCALL *)(BYTE *)>(0x56b730)(seq);
    }
    if (out)
        *out = pv(seq);
    alignas(4) BYTE filters[0x14];
    filter_ctor(filters, 0);
    if (!mb(self, 0x154)) {
        filter_clone_into(fn<DWORD(__cdecl *)()>(0x56b260)(), self + 0x140);
        mb(self, 0x154) = 1;
    }
    filter_clone_into(pv(self + 0x140), filters);
    fn<DWORD(THISCALL *)(BYTE *, DWORD, DWORD, void *)>(0x56b950)(seq, folder, file, filters);
    filter_dtor(filters);
    return seq;
}

// (filters): the list's filters set
void THISCALL SequenceList_SetFilters(BYTE *self, DWORD filters)   // 0x56c300
{
    filter_clone_into(filters, self + 0x140);
    mb(self, 0x154) = 1;
}

BYTE *THISCALL SequenceWindow_DeletingDtor(BYTE *self, DWORD flags)   // 0x56c3d0
{
    window_dtor(self);
    if (flags & 1)
        pool_delete(self, 0x10c);
    return self;
}

// (parent, name, list)
BYTE *THISCALL SequenceWindow_Ctor(BYTE *self, DWORD parent, const char *name, BYTE *list)   // 0x56c400
{
    window_ctor(self, parent, nullptr);
    m(self, 0) = vt(0x5ff794);
    if (name) {
        BYTE title[4];
        zs_ctor(title);
        zs_format(title, glob<const char *>(0x621858), name);
        fn<void(THISCALL *)(BYTE *, const char *)>(0x40cdc0)(self, c_str(title));
        zs_dtor(title);
    }
    if (!list)
        list = glob<BYTE *>(0x713380);
    const DWORD i = vc(list, 3, name, -1);
    const DWORD seq = record_inline(pv(list), i);
    mb(self, 0x61) = 0;
    m(self, 0x100) = seq;
    m(self, 0x104) = 0;
    m<float>(self, 0x108) = (float)vc<long double>(bp(seq), 26);
    BYTE *s = bp(m(self, 0x100));
    vc(self, 59, m(s, 0x14), m(s, 0x18));
    return self;
}

// (seconds): the current frame moved on by the frame rate, wrapped once at the frame count
void THISCALL SequencePlayer_Advance(BYTE *self, float dt)   // 0x56c5a0
{
    m<float>(self, 0x104) = (float)((long double)dt * (long double)m<float>(self, 0x108) + (long double)m<float>(self, 0x104));
    const int count = (int)vc(bp(m(self, 0x100)), 25);
    if (x87_fistp((long double)m<float>(self, 0x104)) >= count)
        m<float>(self, 0x104) = (float)((long double)m<float>(self, 0x104) - (long double)count);
}

// (frame): the current frame blitted at 0, 0 in the sequence's blend mode
void THISCALL SequencePlayer_Draw(BYTE *self, DWORD frame)   // 0x56c600
{
    const int i = x87_fistp((long double)m<float>(self, 0x104));
    const DWORD f = vc(bp(m(self, 0x100)), 23, i);
    if (f) {
        const DWORD blend = fn<DWORD(THISCALL *)(DWORD)>(0x56bf00)(m(self, 0x100));
        fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4b3280)(frame, f, 0, 0, blend);
    }
}

}  // namespace zax::seqlist

namespace zax {

using namespace seqlist;

ZAX_HOOKS(sequence_list)
{
    const Hook list[] = {
#include "sequence_list_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
