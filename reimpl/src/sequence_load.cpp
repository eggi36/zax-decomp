// Loading the sequences (phase 4, rendering; docs/DECOMPILATION.md 9, "Effects and text: the hue change effect and
// its cache, the text overlays, …"): the frame filters a sequence's frames get, the cache file name and the binary
// cache file, the binary slots 13 and 15 of CSequence and CRotationSequence, appending a frame. Not here: 0x56b320
// (the Properties.txt, read or written; the game data has none, so a check would write one).
//
// The frame filter list 0x713360 (the loaders' shared list; FrameFilterList_* 0x4b1d80-0x4b2160 in
// frame_load_filters.cpp) [confirmed: code]: the default set is 3, 6, 7, 11, 8; a sequence's load flags
// (+0x1c) change it: 0x10 takes 12 for 11, 8 takes 9 for 8 and sets filter 3's +0x14 to 3, 1 adds 2, without
// 2 filter 7 goes, without 4 filter 6 goes; the blit types "Blend Black Bias" and "Blend Amplify Light" take 4
// and 5 for 7.
// The cache file [confirmed: code]: Files_CacheFolder of the path + the path, then "<that>/<name>.seq" (8
// bits), ".seq16" (16 bits; both ZString_SetPath3), "-…seq24"/"-…seq32" (ZString_SetDashPath), by the
// filter list's target depth or the display's colour depth; false only for 32 bits.
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD FILTERS = 0x713360;

inline void filters_clear(DWORD l) { fn<void(THISCALL *)(void *)>(0x4b1d80)(at<void *>(l)); }
inline void filters_add(DWORD l, DWORD kind) { fn<void(THISCALL *)(void *, DWORD)>(0x4b1db0)(at<void *>(l), kind); }
inline void filters_remove(DWORD l, DWORD kind) { fn<void(THISCALL *)(void *, DWORD)>(0x4b20e0)(at<void *>(l), kind); }
inline BYTE *filters_find(DWORD l, DWORD kind) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x4b2140)(at<void *>(l), kind); }
inline DWORD blend_mode_of(DWORD index)
{
    void *names = fn<void *(__cdecl *)()>(0x4669a0)();
    return fn<DWORD(THISCALL *)(void *, DWORD)>(0x592310)(names, index);
}
void default_filters()
{
    filters_clear(glob_addr(FILTERS));
    filters_add(glob_addr(FILTERS), 3);
    filters_add(glob_addr(FILTERS), 6);
    filters_add(glob_addr(FILTERS), 7);
    filters_add(glob_addr(FILTERS), 0xb);
    filters_add(glob_addr(FILTERS), 8);
}

struct ZStr {
    DWORD buffer;
    ZStr() { zs_ctor(reinterpret_cast<BYTE *>(this)); }
    ~ZStr() { zs_dtor(reinterpret_cast<BYTE *>(this)); }
};
struct Stream {   // ObjectStream (0x24, a CFileBuf first: +0x14 the sum of its Advance calls)
    BYTE b[0x24];
    Stream() { fn<void *(THISCALL *)(void *)>(0x5240b0)(this); }
    ~Stream() { fn<void(THISCALL *)(void *)>(0x5241e0)(this); }
    bool open(const char *path, DWORD mode, DWORD a) { return fn<BYTE(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x5242b0)(this, path, mode, a) != 0; }
};
inline void free_frames(BYTE *seq) { fn<void(THISCALL *)(void *)>(0x56b6a0)(seq); }

}  // namespace

// 0x56b260: the default filter set in the shared list; eax the list
static DWORD __cdecl Sequence_DefaultFilters()
{
    default_filters();
    return glob_addr(FILTERS);
}

// 0x56b440 (sequence; filter list or null): the sequence's filters (the default set in the shared list without
// one); eax the list
static DWORD THISCALL Sequence_SetupFilters(BYTE *seq, DWORD list)
{
    if (!list) {
        default_filters();
        list = glob_addr(FILTERS);
    }
    if (seq[0x1c] & 0x10) {
        filters_remove(list, 0xb);
        filters_add(list, 0xc);
    }
    if (seq[0x1c] & 8) {
        filters_remove(list, 8);
        filters_add(list, 9);
        if (BYTE *f = filters_find(list, 3))
            dw(f, 0x14) = 3;
    }
    if (seq[0x1c] & 1)
        filters_add(list, 2);
    if (!(seq[0x1c] & 2))
        filters_remove(list, 7);
    if (blend_mode_of(dw(seq, 0x20)) == glob_addr(0x6b9728)) {   // Blend Black Bias
        filters_remove(list, 7);
        filters_add(list, 4);
    }
    if (blend_mode_of(dw(seq, 0x20)) == glob_addr(0x6b85f0)) {   // Blend Amplify Light
        filters_remove(list, 7);
        filters_add(list, 5);
    }
    if (!(seq[0x1c] & 4))
        filters_remove(list, 6);
    return list;
}

// 0x56b780 (fastcall ZString *out, path; name, filter list or null): the cache file of the sequence (head
// comment); al false only for 32 bits
static BYTE __fastcall Sequence_CacheFileName(void *out, const char *path, const char *name, DWORD list)
{
    ZStr dir;
    fn<void(THISCALL *)(void *, const char *, const char *)>(0x4dfa50)(&dir, fn<const char *(__cdecl *)(const char *)>(0x4a1140)(path), path);
    BYTE r = 1;
    // data_depth: the opt-in TrueColorData16 (reimpl.h); the original's depth without it
    const DWORD depth = data_depth(list ? fn<DWORD(THISCALL *)(void *)>(0x4b2160)(at<void *>(list)) : fn<DWORD(__cdecl *)()>(0x55d030)());
    using SetPath = void(THISCALL *)(void *, const char *, const char *, const char *);
    switch (depth) {
    case 8:
        fn<SetPath>(0x4e02c0)(out, zs_c_str(&dir), name, glob<const char *>(0x621840));
        break;
    case 16:
        fn<SetPath>(0x4e02c0)(out, zs_c_str(&dir), name, glob<const char *>(0x621838));
        break;
    case 24:
        fn<SetPath>(0x4dffd0)(out, zs_c_str(&dir), name, glob<const char *>(0x621830));
        break;
    case 32:
        fn<SetPath>(0x4dffd0)(out, zs_c_str(&dir), name, glob<const char *>(0x621828));
        r = 0;
        break;
    }
    return r;
}

// 0x56b890 (sequence; path, name): the frames freed, then the binary cache file read (Object_ReadBinary with
// the file buffer's option 0x18 set; a non-empty buffer is kept as the frames' block +0x34); al true when it
// read and the sequence has frames (the frames freed after a failed read)
static BYTE THISCALL Sequence_LoadCache(BYTE *seq, const char *path, const char *name)
{
    ZStr file;
    fn<BYTE(__fastcall *)(void *, const char *, const char *, DWORD)>(0x56b780)(&file, path, name, 0);
    free_frames(seq);
    Stream s;
    BYTE r = 0;
    if (s.open(zs_c_str(&file), 0x204, 1)) {
        fn<void(THISCALL *)(void *, DWORD)>(0x4a2140)(s.b, 1);
        const BYTE ok = fn<BYTE(THISCALL *)(void *, void *)>(0x416fe0)(seq, &s);
        if (dw(s.b, 0x14))
            dw(seq, 0x34) = fn<DWORD(THISCALL *)(void *)>(0x4a2170)(s.b);
        if (!ok)
            free_frames(seq);
        else if (dw(seq, 0x28))
            r = 1;
    }
    return r;
}

// 0x56b950 (sequence; path, name, unused): 0x56b890 as a bool
static bool THISCALL Sequence_LoadCacheBool(BYTE *seq, const char *path, const char *name, DWORD)
{
    return fn<BYTE(THISCALL *)(void *, const char *, const char *)>(0x56b890)(seq, path, name) != 0;
}

// 0x56b9d0 (slot 15; file buffer): the bounding rectangle (0x10 bytes at +0xc), then per frame a byte (0: none)
// and the frame (a new CFrame read by 0x4a9340); a failed frame frees the frames, al 0. The byte is read into
// the file argument's stack slot, so after the end of the file the pointer's low byte decides.
// Written as the exe's instructions (batch nulltest, docs/DECOMPILATION.md section 16): on a stream that ends,
// CFrame::ReadFrm 0x4a9340 tests and takes its header from its own stack slots, which its failed read leaves as the
// stack had them. A C++ body calls it at the compiler's depth (one dword deeper: the byte's own local), so it read
// other bytes than in the exe; here every push and call is the exe's, so 0x4a9340 and the callees before it run at
// the exe's depth. Calls go through eax where the exe's relative call leaves eax free (ZAX_ASM_CODE_ATT).
__attribute__((naked)) static BYTE THISCALL Sequence_ReadBinary(BYTE *, void *)
{
    __asm__("pushl %ebx\n\t"
            "movl 8(%esp), %ebx\n\t"                       // file
            "pushl %ebp\n\t"
            "pushl %esi\n\t"
            "movl %ecx, %esi\n\t"
            "pushl %edi\n\t"
            "pushl $0x10\n\t"
            "movl %ebx, %ecx\n\t"
            "leal 0xc(%esi), %eax\n\t"
            "pushl %eax\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4a1b00) ", %eax\n\t"   // FileBuf_Read: the rectangle
            "calll *%eax\n\t"
            "movl 0x28(%esi), %eax\n\t"
            "leal (,%eax,4), %edi\n\t"
            "pushl %edi\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x5d033a) ", %eax\n\t"   // operator new: the frame pointers
            "calll *%eax\n\t"
            "movl %edi, %ecx\n\t"
            "movl %eax, %edx\n\t"
            "movl %ecx, %ebp\n\t"
            "xorl %eax, %eax\n\t"
            "movl %edx, %edi\n\t"
            "addl $4, %esp\n\t"
            "shrl $2, %ecx\n\t"
            "rep stosl\n\t"
            "movl %ebp, %ecx\n\t"
            "andl $3, %ecx\n\t"
            "rep stosb\n\t"
            "movl 0x28(%esi), %eax\n\t"
            "xorl %edi, %edi\n\t"
            "testl %eax, %eax\n\t"
            "movl %edx, 8(%esi)\n\t"
            "jbe 6f\n"
            "1:\n\t"
            "leal 0x14(%esp), %ecx\n\t"                    // the file argument's slot
            "pushl %ecx\n\t"
            "movl %ebx, %ecx\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4a22a0) ", %eax\n\t"   // FileBuf_ReadByte
            "calll *%eax\n\t"
            "movb 0x14(%esp), %al\n\t"
            "testb %al, %al\n\t"
            "je 3f\n\t"
            "movl $0x40, %ecx\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x417710) ", %eax\n\t"   // operator new (ecx)
            "calll *%eax\n\t"
            "testl %eax, %eax\n\t"
            "je 2f\n\t"
            "movl %eax, %ecx\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4a8a90) ", %eax\n\t"   // CFrame::Ctor
            "calll *%eax\n\t"
            "jmp 4f\n"
            "2:\n\t"
            "xorl %eax, %eax\n"
            "4:\n\t"
            "movl 8(%esi), %edx\n\t"
            "pushl %ebx\n\t"
            "movl %eax, %ecx\n\t"
            "movl %eax, (%edx,%edi,4)\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4a9340) ", %eax\n\t"   // CFrame::ReadFrm
            "calll *%eax\n\t"
            "testb %al, %al\n\t"
            "je 7f\n\t"
            "jmp 5f\n"
            "3:\n\t"
            "movl 8(%esi), %eax\n\t"
            "movl $0, (%eax,%edi,4)\n"
            "5:\n\t"
            "movl 0x28(%esi), %eax\n\t"
            "incl %edi\n\t"
            "cmpl %eax, %edi\n\t"
            "jb 1b\n"
            "6:\n\t"
            "popl %edi\n\t"
            "popl %esi\n\t"
            "popl %ebp\n\t"
            "movb $1, %al\n\t"
            "popl %ebx\n\t"
            "retl $4\n"
            "7:\n\t"
            "movl %esi, %ecx\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x56b6a0) ", %eax\n\t"   // CSequence::FreeFrames
            "calll *%eax\n\t"
            "popl %edi\n\t"
            "popl %esi\n\t"
            "popl %ebp\n\t"
            "xorb %al, %al\n\t"
            "popl %ebx\n\t"
            "retl $4");
}

// 0x56ba90 (slot 13; file buffer): the bounding rectangle, then per frame a byte (1 for a frame with a width
// and a height) and such a frame (0x4a9740; null when it is no CFrame)
static void THISCALL Sequence_WriteBinary(BYTE *seq, void *file)
{
    fn<DWORD(THISCALL *)(void *, const void *, DWORD)>(0x4a19d0)(file, seq + 0xc, 0x10);
    for (DWORD i = 0; i < dw(seq, 0x28); i++) {
        BYTE has = 0;
        if (BYTE *f = bp(reinterpret_cast<DWORD *>(bp(dw(seq, 8)))[i]))
            has = vcall<DWORD>(f, 45) && vcall<DWORD>(bp(reinterpret_cast<DWORD *>(bp(dw(seq, 8)))[i]), 39);
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a21e0)(file, has);
        if (has) {
            BYTE *f = bp(reinterpret_cast<DWORD *>(bp(dw(seq, 8)))[i]);
            if (f && !is_kind_of(f, fn<CClassInfo *(__cdecl *)()>(0x4a8760)()))
                f = nullptr;
            fn<DWORD(THISCALL *)(void *, void *)>(0x4a9740)(f, file);
        }
    }
}

// 0x56bb70 (sequence; frame, filter 6, filter 2): the frame appended (a new pointer array), first filter 2
// then filter 6 run on it when asked, then the bounding rectangle (+0xc..+0x18) rebuilt from every frame's
// slot 36 (starting at 99999, 999999, -999999, -999999)
static void THISCALL Sequence_AddFrame(BYTE *seq, BYTE *frame, DWORD filter6, DWORD filter2)
{
    const DWORD n = dw(seq, 0x28);
    DWORD *frames = static_cast<DWORD *>(heap_alloc(n * 4 + 4));
    if (n)
        memcpy(frames, bp(dw(seq, 8)), n * 4);
    if (dw(seq, 8)) {
        heap_free(bp(dw(seq, 8)));
        dw(seq, 8) = 0;
    }
    alignas(4) BYTE filter[0x20];
    if (filter2) {
        fn<void *(THISCALL *)(void *)>(0x4b22d0)(filter);
        fn<void(THISCALL *)(void *, void *)>(0x4b21a0)(filter, frame);
        fn<void(THISCALL *)(void *)>(0x4b1d20)(filter);
    }
    if (filter6) {
        fn<void *(THISCALL *)(void *)>(0x4b2680)(filter);
        fn<void(THISCALL *)(void *, void *)>(0x4b21a0)(filter, frame);
        fn<void(THISCALL *)(void *)>(0x4b1d20)(filter);
    }
    dw(seq, 8) = (DWORD)(uintptr_t)frames;
    frames[n] = (DWORD)(uintptr_t)frame;
    dw(seq, 0x28)++;
    dw(seq, 0xc) = 99999;
    dw(seq, 0x10) = 999999;
    dw(seq, 0x14) = 0xfff0bdc1;
    dw(seq, 0x18) = 0xfff0bdc1;
    for (DWORD i = 0; i < dw(seq, 0x28); i++) {
        BYTE *f = bp(reinterpret_cast<DWORD *>(bp(dw(seq, 8)))[i]);
        if (!f)
            continue;
        int r[4];
        vcall<void>(f, 36, r);
        int *b = reinterpret_cast<int *>(seq + 0xc);
        if (!(b[0] < r[0]))
            b[0] = r[0];
        if (!(b[1] < r[1]))
            b[1] = r[1];
        if (!(b[2] > r[2]))
            b[2] = r[2];
        if (!(b[3] > r[3]))
            b[3] = r[3];
    }
}

// 0x55f910 (CRotationSequence slot 15; file buffer): CSequence's, then the frame deltas (+0x44: count × 8
// bytes, a new block). Written as the exe's instructions (batch nulltest), so CSequence's ReadBinary and with it
// CFrame::ReadFrm 0x4a9340 run at the exe's depth (above); with a new block the exe also stores count - 1 into the
// file argument's slot.
__attribute__((naked)) static BYTE THISCALL RotationSequence_ReadBinary(BYTE *, void *)
{
    __asm__("pushl %ebx\n\t"
            "movl 8(%esp), %ebx\n\t"                       // file
            "pushl %esi\n\t"
            "movl %ecx, %esi\n\t"
            "pushl %ebx\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x56b9d0) ", %eax\n\t"   // CSequence::ReadBinary
            "calll *%eax\n\t"
            "testb %al, %al\n\t"
            "je 9f\n\t"
            "movl 0x44(%esi), %eax\n\t"
            "testl %eax, %eax\n\t"
            "je 1f\n\t"
            "pushl %eax\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x5d0320) ", %eax\n\t"   // operator delete
            "calll *%eax\n\t"
            "addl $4, %esp\n\t"
            "movl $0, 0x44(%esi)\n"
            "1:\n\t"
            "pushl %edi\n\t"
            "movl 0x28(%esi), %edi\n\t"
            "leal (,%edi,8), %eax\n\t"
            "pushl %eax\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x5d033a) ", %eax\n\t"   // operator new
            "calll *%eax\n\t"
            "addl $4, %esp\n\t"
            "testl %eax, %eax\n\t"
            "je 2f\n\t"
            "decl %edi\n\t"
            "movl %edi, 0x10(%esp)\n\t"                    // the file argument's slot
            "jmp 3f\n"
            "2:\n\t"
            "xorl %eax, %eax\n"
            "3:\n\t"
            "movl 0x28(%esi), %ecx\n\t"
            "movl %eax, 0x44(%esi)\n\t"
            "shll $3, %ecx\n\t"
            "pushl %ecx\n\t"
            "pushl %eax\n\t"
            "movl %ebx, %ecx\n\t"
            "movl " ZAX_ASM_CODE_ATT(0x4a1b00) ", %eax\n\t"   // FileBuf_Read: the deltas
            "calll *%eax\n\t"
            "popl %edi\n\t"
            "popl %esi\n\t"
            "movb $1, %al\n\t"
            "popl %ebx\n\t"
            "retl $4\n"
            "9:\n\t"
            "popl %esi\n\t"
            "xorb %al, %al\n\t"
            "popl %ebx\n\t"
            "retl $4");
}

// 0x55f980 (CRotationSequence slot 13; file buffer): CSequence's, then the frame deltas
static void THISCALL RotationSequence_WriteBinary(BYTE *seq, void *file)
{
    fn<void(THISCALL *)(void *, void *)>(0x56ba90)(seq, file);
    fn<DWORD(THISCALL *)(void *, const void *, DWORD)>(0x4a19d0)(file, bp(dw(seq, 0x44)), dw(seq, 0x28) << 3);
}

ZAX_HOOKS(sequence_load)
{
    const Hook list[] = {
#include "sequence_load_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
