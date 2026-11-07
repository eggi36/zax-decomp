// Small functions of the application base class (Game flow, 0x4cdc70-0x4ce500), the Win32 platform
// helpers 0x4d2190-0x4d2640 (Core) and the handle heap 0x4d2660-0x4d2bc0 (phase 3 batch of
// 0x4cc000-0x4d3fff).
//
// The application base (vtable 0x5f73e8; the world derives from it, section 7) [confirmed: code]: +0x1d
// "run on the clock", +0x24 a value set by 0x4ce500, +0x28/+0x29 two flags, +0x34 the window, +0x38 the
// frame number, +0x40 the total of the steps (a double), +0x48 the last counter (a double), +0x50 the time
// left over, +0x54 the step, +0x58 the longest step, +0x5c the most time carried over. 0x4cdd80 runs the
// fixed steps of a frame: the time since the last frame (QueryPerformanceCounter / -Frequency, 0x53d150 /
// 0x53d170) plus what was left, cut to +0x5c (the rest dropped), or one longest step when not on the
// clock; while at least one step is left, slot 3 (the step's seconds: at most the longest step, else all
// that is left) adds to the total; slot 4 after the steps. st0: the step minus what is left.
//
// The command line (the ZString 0x6c047c, set by 0x4d2190): an option is found anywhere, ignoring case
// (0x587c70), and counts when followed by the end or a white space (0x587a50); its value is the next word
// after white space (0x587aa0 / 0x587b00) or a quoted text.
//
// The handle heap [confirmed: code; checked]: a block of `size` bytes cut into blocks with a 0x20-byte
// header {prev +0, next +4 in the list of used blocks (the heap's +0x1c is its sentinel), locks +8, flags
// +0xc (bit 0: may be discarded), size +0x10, the block before +0x14, the handle +0x18 (0: free), the
// heap +0x1c}; a free block's +0x20 is its node in the size tree (0x431950 best fit, 0x431990 insert,
// 0x4319e0 remove, 0x431a70 re-key) whose root is the heap's +0xc. The heap {used +0, size +4, block +8,
// the root +0xc, the sentinel +0x1c, the number of used blocks +0x3c, the number of compactions +0x40}.
// A handle holds its block; allocating looks for the best fitting free block (at least 0x30 bytes are
// split off), compacts the heap when the free bytes would do, and discards unlocked discardable blocks
// from the oldest when not. Callers: the tile map (0x53f…, Rendering).
//
// Left out: the movies 0x4cd5a0 and Game_Startup 0x4cd780 (they run before the hooks), the frame counter's
// drawing 0x4ce160, the music 0x4ce3e0/0x4ce510 (Rendering, Sound, the start), 0x4cded0/0x4cdf40 and the two
// tail calls into the window 0x4cdec0/0x4cdf60 (user interface). The destructor 0x4cdcc0/0x4cdc50, the frame
// 0x4cdfd0, the display switch 0x4ce400 and the start-up loading 0x4cdf70 are in app_rest_b.cpp, the Win32
// wrappers 0x4d2310-0x4d2650 in core_rest_b.cpp (phase 3 remains, band B). Not hookable: 0x4cde80, 0x4cdea0,
// 0x4cdf30 (4 bytes each) and the handle functions 0x4d2670 (free), 0x4d2760 (lock), 0x4d27a0 (unlock), whose
// first five bytes hold a short branch.
#include "exe_base.h"
#include "x87.h"
#include "zstring.h"

namespace zax {

namespace {

template <typename T> T &fl(void *p, DWORD off) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + off); }
DWORD &dw(void *p, DWORD off) { return fl<DWORD>(p, off); }
BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }
template <typename F> F import(DWORD slot) { return *at<F *>(slot); }

// ---- The application base ------------------------------------------------------------------------------

// 0x4cdc70, 0x4cdc80: the default display size (Cpu_Class 0x515620 asked first, its answer unused)
DWORD __cdecl App_DefaultWidth()
{
    fn<DWORD(__cdecl *)()>(0x515620)();
    return 0x280;
}
DWORD __cdecl App_DefaultHeight()
{
    fn<DWORD(__cdecl *)()>(0x515620)();
    return 0x1e0;
}
// 0x4cdc90: the default colour depth: 16 for a processor of class 6 with more than 64,000,000 bytes of
// memory (0x4d2490), else 8
DWORD __cdecl App_DefaultDepth()
{
    const int c = fn<int(__cdecl *)()>(0x515620)();
    if (c > 0 && c > 5 && c == 6 && fn<DWORD(__cdecl *)()>(0x4d2490)() > 0x3d09000)
        return 0x10;
    return 8;
}

long double fld64(const void *p)
{
    long double r;
    __asm__("fldl %1" : "=t"(r) : "m"(*static_cast<const double *>(p)));
    return r;
}
void fstp64(long double v, void *p) { __asm__("fstpl %0" : "=m"(*static_cast<double *>(p)) : "t"(v) : "st"); }

// 0x4cdd80: the fixed steps of a frame (see above). st0: the step minus the time left.
long double THISCALL App_RunSteps(BYTE *self)
{
    const unsigned long long c = fn<unsigned long long(__cdecl *)()>(0x53d150)();
    double now;
    fstp64(fild64((long long)c), &now);
    float t;
    if (fl<BYTE>(self, 0x1d)) {
        double elapsed;
        fstp64(fld64(&now) - fld64(self + 0x48), &elapsed);
        const unsigned long long f = fn<unsigned long long(__cdecl *)()>(0x53d170)();
        const long double sum = fld64(&elapsed) / fild64((long long)f) + fld(fl<float>(self, 0x50));
        t = fstp(sum);
        if (!c03(sum, fld(fl<float>(self, 0x5c)))) {
            t = fl<float>(self, 0x5c);
            dw(self, 0x50) = 0;
        }
    } else {
        t = fl<float>(self, 0x58);
    }
    memcpy(self + 0x48, &now, 8);
    if (!c0(fld(t), fld(fl<float>(self, 0x54)))) {
        do {
            const float longest = fl<float>(self, 0x58);
            if (!c03(fld(t), fld(longest))) {
                vcall<void>(self, 3, longest);
                fstp64(fld(fl<float>(self, 0x58)) + fld64(self + 0x40), self + 0x40);
                t = fstp(fld(t) - fld(fl<float>(self, 0x58)));
            } else {
                vcall<void>(self, 3, t);
                fstp64(fld(t) + fld64(self + 0x40), self + 0x40);
                t = 0.0f;
            }
        } while (!c0(fld(t), fld(fl<float>(self, 0x54))));
        vcall<void>(self, 4);
    }
    const long double r = fld(fl<float>(self, 0x54)) - fld(t);
    fl<float>(self, 0x50) = t;
    return r;
}

// 0x4cde90, 0x4cdeb0: the flags +0x28 / +0x29 (al: the value)
template <DWORD OFF> DWORD THISCALL App_SetFlag(BYTE *self, DWORD v)
{
    fl<BYTE>(self, OFF) = (BYTE)v;
    return v & 0xff;
}
// 0x4cdf20 (al 0)
DWORD THISCALL App_No(BYTE *, DWORD) { return 0; }
// 0x4ce500: +0x24 (eax: the value)
DWORD THISCALL App_Set24(BYTE *self, DWORD v)
{
    dw(self, 0x24) = v;
    return v;
}
// 0x4ce130: the four frame timers 0x6c00b0, 0x6bfe88, 0x6bffa0, 0x6c01c0 reset (0x4a8240)
DWORD __cdecl App_ResetTimers()
{
    const auto reset = code<DWORD(THISCALL *)(void *)>(0x4a8240);
    reset(glob<void *>(0x6c00b0));
    reset(glob<void *>(0x6bfe88));
    reset(glob<void *>(0x6bffa0));
    return reset(glob<void *>(0x6c01c0));
}
// 0x4ce470, 0x4ce4c0: the timers started (0x4a83c0) once per frame (+0x38 against the frame they last
// started in, 0x6c02d8 / 0x6c02e0 / 0x6c02dc)
void timer_once(BYTE *self, DWORD last, DWORD timer)
{
    if (*at<DWORD *>(last) < dw(self, 0x38)) {
        fn<void(THISCALL *)(void *)>(0x4a83c0)(at<void *>(timer));
        *at<DWORD *>(last) = dw(self, 0x38);
    }
}
void THISCALL App_StartTimers(BYTE *self)
{
    timer_once(self, glob_addr(0x6c02d8), glob_addr(0x6c01c0));
    timer_once(self, glob_addr(0x6c02e0), glob_addr(0x6c00b0));
}
void THISCALL App_StartTimer2(BYTE *self) { timer_once(self, glob_addr(0x6c02dc), glob_addr(0x6bfe88)); }
// 0x4ce4f0 (two arguments, unused): the resource lists' slot 28 (ResourceLists_Get, CallAll28)
DWORD THISCALL App_ResourceLists28(BYTE *, DWORD, DWORD)
{
    void *r = fn<void *(__cdecl *)()>(0x4a35d0)();
    return fn<DWORD(THISCALL *)(void *)>(0x4a3730)(r);
}

// ---- The command line ------------------------------------------------------------------------------------

constexpr DWORD COMMAND_LINE = 0x6c047c;
const char *command_line() { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(glob<void *>(COMMAND_LINE)); }
const char *find_no_case(const char *text, const char *word) { return fn<const char *(__fastcall *)(const char *, const char *)>(0x587c70)(text, word); }
const char *skip_white(const char *s) { return fn<const char *(__fastcall *)(const char *)>(0x587aa0)(s); }
const char *skip_word(const char *s) { return fn<const char *(__fastcall *)(const char *)>(0x587b00)(s); }
DWORD length(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}

// 0x4d2190 (ecx: the text): the command line set
DWORD __fastcall CommandLine_Set(const char *text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x4e1930)(glob<void *>(COMMAND_LINE), text); }
// 0x4d22c0 (ecx: the option): al: given (followed by the end or white space)
DWORD __fastcall CommandLine_Has(const char *option)
{
    const char *p = find_no_case(command_line(), option);
    if (!p)
        return 0;
    const char c = p[length(option)];
    if (!c)
        return 1;
    return fn<DWORD(__fastcall *)(char)>(0x587a50)(c) & 0xff ? 1 : 0;
}
// The value of an option: its start and end, or 0
const char *option_value(const char *option, const char **end)
{
    const char *p = find_no_case(command_line(), option);
    if (!p)
        return nullptr;
    const char *s = skip_white(p + length(option));
    if (!s || !*s)
        return nullptr;
    const char *e = skip_word(s);
    if (*s == '"') {
        s++;
        if (const char *q = fn<const char *(__cdecl *)(const char *, int)>(0x5d08c0)(s, '"'))
            e = q;
    }
    *end = e;
    return s;
}
// 0x4d21a0 (ecx: the option, edx: the ZString for the value): al: found
DWORD __fastcall CommandLine_Value(const char *option, ZString *out)
{
    const char *e = nullptr;
    const char *s = option_value(option, &e);
    if (!s)
        return 0;
    fn<void(THISCALL *)(ZString *, const char *, DWORD)>(0x4df160)(out, s, (DWORD)(e - s));
    return 1;
}
// 0x4d2220 (ecx: the option, edx: the default): the option's value as a number (atoi)
int __fastcall CommandLine_Int(const char *option, int fallback)
{
    ZString v;
    fn<void *(THISCALL *)(void *)>(0x4dec90)(&v);
    int r = fallback;
    const char *e = nullptr;
    if (const char *s = option_value(option, &e)) {
        fn<void(THISCALL *)(ZString *, const char *, DWORD)>(0x4df160)(&v, s, (DWORD)(e - s));
        r = fn<int(__cdecl *)(const char *)>(0x5d03d3)(fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(&v));
    }
    fn<void(THISCALL *)(void *)>(0x4defd0)(&v);
    return r;
}

// ---- Win32 -------------------------------------------------------------------------------------------------

// 0x4d2300 (ecx: the buffer, edx: its size): the temporary folder (GetTempPathA, the import 0x5ea0b8; the name
// Platform_CurrentDirectory is older than this finding and kept, 2026-11-02). al 1.
DWORD __fastcall Platform_CurrentDirectory(char *buf, DWORD size)
{
    buf[0] = 0;
    return (imp<DWORD(WINAPI *)(DWORD, char *)>(0x5ea0b8)(size, buf) & ~0xffu) | 1;
}
// 0x4d2370 (ecx: the buffer, edx: its size): the exe's path (GetModuleFileNameA). al: got one.
DWORD __fastcall Platform_ExePath(char *buf, DWORD size)
{
    buf[0] = 0;
    const DWORD r = imp<DWORD(WINAPI *)(void *, char *, DWORD)>(0x5ea0e0)(nullptr, buf, size);
    return (r & ~0xffu) | (r ? 1 : 0);
}
// 0x4d2390 ExeBaseName (ecx: out, edx: its size): the exe's file name without folder, extension and
// anything from a space (strncpy). al: got one; eax's upper bytes strncpy's.
DWORD __fastcall ExeBaseName(char *out, DWORD n)
{
    char path[0x104];
    path[0] = 0;
    if (!imp<DWORD(WINAPI *)(void *, char *, DWORD)>(0x5ea0e0)(nullptr, path, 0x104))
        return 0;
    const auto strchr_ = code<char *(__cdecl *)(const char *, int)>(0x5d08c0);
    char *s = path;
    if (strchr_(path, '\\'))
        do
            s = strchr_(s, '\\') + 1;
        while (strchr_(s, '\\'));
    if (strchr_(s, '/'))
        do
            s = strchr_(s, '/') + 1;
        while (strchr_(s, '/'));
    if (char *dot = strchr_(s, '.'))
        *dot = 0;
    if (char *sp = strchr_(s, ' '))
        *sp = 0;
    const DWORD r = pv(fn<char *(__cdecl *)(char *, const char *, DWORD)>(0x5d0b50)(out, s, n));
    return (r & ~0xffu) | 1;
}
// 0x4d2490: the physical memory in bytes (GlobalMemoryStatus)
DWORD __cdecl Platform_PhysicalMemory()
{
    DWORD status[8];
    imp<void(WINAPI *)(DWORD *)>(0x5ea0ec)(status);
    return status[2];
}
// 0x4d2530: the screen's bits per pixel (GetDC(0), GetDeviceCaps 14 (planes) times 12 (bits), ReleaseDC)
DWORD __cdecl Platform_ScreenDepth()
{
    void *dc = imp<void *(WINAPI *)(void *)>(0x5ea260)(nullptr);
    const auto caps = imp<int(WINAPI *)(void *, int)>(0x5ea030);
    const int planes = caps(dc, 0xe);
    const int bits = caps(dc, 0xc);
    imp<int(WINAPI *)(void *, void *)>(0x5ea264)(nullptr, dc);
    return (DWORD)(planes * bits);
}
// 0x4d2620 (a, b; shift): MulDiv(a, 1 << shift, b); 0x4d2640 (a, b; c): MulDiv(a, b, c)
int __fastcall Platform_MulDivShift(int a, int b, DWORD shift) { return imp<int(WINAPI *)(int, int, int)>(0x5ea0f4)(a, 1 << (shift & 31), b); }
int __fastcall Platform_MulDiv(int a, int b, int c) { return imp<int(WINAPI *)(int, int, int)>(0x5ea0f4)(a, b, c); }

// ---- The handle heap ----------------------------------------------------------------------------------------

// The size tree (unassigned helpers of 0x431950-0x431a70, through their exe addresses)
BYTE *tree_best_fit(BYTE *root, DWORD size) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x431950)(root, size); }
void tree_insert(BYTE *node, BYTE *root, DWORD key) { fn<void(THISCALL *)(void *, void *, DWORD)>(0x431990)(node, root, key); }
void tree_remove(BYTE *node) { fn<void(THISCALL *)(void *)>(0x4319e0)(node); }
void tree_rekey(BYTE *node, BYTE *root, DWORD key) { fn<void(THISCALL *)(void *, void *, DWORD)>(0x431a70)(node, root, key); }

BYTE *heap_end(BYTE *h) { return bp(dw(h, 8) + dw(h, 4)); }
// A block: unlinked from the used list / linked after the list's last block
void unlink(BYTE *b)
{
    dw(bp(dw(b, 0)), 4) = dw(b, 4);
    dw(bp(dw(b, 4)), 0) = dw(b, 0);
}
void link_last(BYTE *b, BYTE *h)
{
    BYTE *last = bp(dw(h, 0x1c));
    dw(b, 4) = dw(last, 4);
    dw(last, 4) = pv(b);
    dw(b, 0) = pv(last);
    dw(bp(dw(b, 4)), 0) = pv(b);
}
// A free block's header
void free_header(BYTE *b, BYTE *h, DWORD size, DWORD before)
{
    dw(b, 8) = 0;
    dw(b, 0xc) = 0;
    dw(b, 0x10) = size;
    dw(b, 0x14) = before;
    dw(b, 0x18) = 0;
    dw(b, 0x1c) = pv(h);
}
// Every block's handle cleared, the counts and the list emptied
void forget_blocks(BYTE *h)
{
    for (BYTE *b = bp(dw(h, 8)); b;) {
        if (DWORD handle = dw(b, 0x18)) {
            dw(bp(handle), 0) = 0;
            dw(b, 0x18) = 0;
        }
        BYTE *next = b + dw(b, 0x10);
        b = next < heap_end(h) ? next : nullptr;
    }
    dw(h, 0x3c) = 0;
    dw(h, 0x40) = 0;
    dw(h, 0) = 0;
    dw(h, 0x20) = pv(h + 0x1c);
    dw(h, 0x1c) = pv(h + 0x1c);
}

// 0x4d27c0 (size): the constructor: an empty heap of `size` bytes (heap_alloc) holding one free block
BYTE *THISCALL Heap_Ctor(BYTE *h, DWORD size)
{
    dw(h, 0x3c) = 0;
    dw(h, 0x40) = 0;
    dw(h, 4) = 0;
    dw(h, 8) = 0;
    dw(h, 0x20) = pv(h + 0x1c);
    dw(h, 0x1c) = pv(h + 0x1c);
    forget_blocks(h);
    if (dw(h, 8)) {
        heap_free(bp(dw(h, 8)));
        dw(h, 8) = 0;
    }
    dw(h, 4) = size;
    if (size) {
        BYTE *b = static_cast<BYTE *>(heap_alloc(size));
        dw(h, 8) = pv(b);
        dw(h, 0xc) = 0;
        dw(h, 0x10) = 0;
        dw(h, 0x14) = 0;
        dw(h, 0x18) = 0;
        free_header(b, h, size, 0);
        tree_insert(b + 0x20, h + 0xc, size);
    }
    dw(h, 0) = 0;
    return h;
}
// 0x4d2880: the destructor's body: the handles cleared, the block freed
void THISCALL Heap_Dtor(BYTE *h)
{
    forget_blocks(h);
    if (dw(h, 8)) {
        heap_free(bp(dw(h, 8)));
        dw(h, 8) = 0;
    }
}
// 0x4d2b50: everything freed into one free block (the memory kept)
void THISCALL Heap_Reset(BYTE *h)
{
    forget_blocks(h);
    BYTE *b = bp(dw(h, 8));
    const DWORD size = dw(h, 4);
    dw(h, 0x14) = 0;
    dw(h, 0x18) = 0;
    free_header(b, h, size, 0);
    tree_insert(b + 0x20, h + 0xc, size);
}
// 0x4d28e0: the used blocks moved down together (memmove, their handles and list neighbours updated),
// the rest one free block
void THISCALL Heap_Compact(BYTE *h)
{
    dw(h, 0x40)++;
    BYTE *dst = bp(dw(h, 8)), *last = nullptr;
    for (BYTE *b = bp(dw(h, 8)); b < heap_end(h);) {
        const DWORD size = dw(b, 0x10);
        BYTE *next = b + size;
        if (dw(b, 0x18)) {
            if (dst != b) {
                fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(dst, b, size);
                dw(bp(dw(dst, 0x18)), 0) = pv(dst);
                dw(bp(dw(dst, 0)), 4) = pv(dst);
                dw(bp(dw(dst, 4)), 0) = pv(dst);
            }
            dw(dst, 0x14) = pv(last);
            last = dst;
            dst += dw(dst, 0x10);
        }
        b = next;
    }
    dw(h, 0x14) = 0;
    dw(h, 0x18) = 0;
    const DWORD rest = dw(h, 4) - dw(h, 0);
    free_header(dst, h, rest, pv(last));
    tree_insert(dst + 0x20, h + 0xc, rest);
}
// 0x4d2980 (size, handle, flags): a block of the best fitting free one (a rest of at least 0x30 bytes
// split off), given the handle and appended to the used list. al: done.
DWORD THISCALL Heap_TryAlloc(BYTE *h, DWORD n, DWORD *handle, DWORD flags)
{
    if (n < 0x10)
        n = 0x10;
    const DWORD need = n + 0x20;
    if (need > dw(h, 4) - dw(h, 0))
        return 0;
    BYTE *node = tree_best_fit(h + 0xc, need);
    BYTE *b = node ? node - 0x20 : nullptr;
    if (!b) {
        tree_best_fit(h + 0xc, need);   // asked again, as the exe
        return 0;
    }
    const DWORD rest = dw(b, 0x10) - need;
    tree_remove(node);
    if (rest >= 0x30) {
        dw(b, 0x10) = need;
        BYTE *r = b + need;
        dw(r, 8) = 0;
        dw(r, 0xc) = 0;
        dw(r, 0x18) = 0;
        dw(r, 0x10) = rest;
        dw(r, 0x14) = pv(b);
        dw(r, 0x1c) = pv(h);
        tree_insert(r + 0x20, h + 0xc, rest);
        BYTE *after = r + dw(r, 0x10);
        if (after < heap_end(h))
            dw(after, 0x14) = pv(r);
    }
    dw(b, 8) = 0;
    dw(b, 0xc) = flags;
    dw(b, 0x18) = pv(handle);
    *handle = pv(b);
    dw(b, 0x1c) = pv(h);
    link_last(b, h);
    dw(h, 0) += dw(b, 0x10);
    dw(h, 0x3c)++;
    return 1;
}
// 0x4d2a70 (block): freed: out of the used list, its handle cleared, merged with a free block after and
// before it (the tree updated)
void THISCALL Heap_Free(BYTE *h, BYTE *b)
{
    unlink(b);
    dw(bp(dw(b, 0x18)), 0) = 0;
    dw(b, 0x18) = 0;
    dw(h, 0x3c)--;
    dw(h, 0) -= dw(b, 0x10);
    BYTE *next = b + dw(b, 0x10);
    if (next < heap_end(h) && !dw(next, 0x18)) {
        tree_remove(next + 0x20);
        dw(b, 0x10) += dw(next, 0x10);
        BYTE *after = b + dw(b, 0x10);
        if (after < heap_end(h))
            dw(after, 0x14) = pv(b);
    }
    BYTE *before = bp(dw(b, 0x14));
    if (before && !dw(before, 0x18)) {
        dw(before, 0x10) += dw(b, 0x10);
        tree_rekey(before + 0x20, h + 0xc, dw(before, 0x10));
        BYTE *after = b + dw(b, 0x10);
        if (after < heap_end(h))
            dw(after, 0x14) = dw(b, 0x14);
        return;
    }
    tree_insert(b + 0x20, h + 0xc, dw(b, 0x10));
}
// 0x4d2660: a handle cleared
DWORD *THISCALL Handle_Init(DWORD *handle)
{
    *handle = 0;
    return handle;
}
// 0x4d2680 (size, heap, flags): the handle's old block freed, a new one of the heap: tried; while it fails
// and the heap has used blocks: compacted when the free bytes would do, else the oldest unlocked
// discardable block freed (none: give up); the new block becomes the list's last
void THISCALL Handle_Alloc(DWORD *handle, DWORD n, BYTE *h, DWORD flags)
{
    if (BYTE *old = bp(*handle))
        fn<void(THISCALL *)(void *, void *)>(0x4d2a70)(bp(dw(old, 0x1c)), old);
    const DWORD size = n < 0x10 ? 0x10 : n;
    if (size + 0x20 > dw(h, 4))
        return;
    const auto try_alloc = code<DWORD(THISCALL *)(void *, DWORD, DWORD *, DWORD)>(0x4d2980);
    BYTE *sentinel = h + 0x1c;
    if (!(BYTE)try_alloc(h, size, handle, flags)) {
        for (;;) {
            if (bp(dw(sentinel, 0)) == sentinel)
                return;
            if (dw(h, 4) - dw(h, 0) >= size + 0x20) {
                fn<void(THISCALL *)(void *)>(0x4d28e0)(h);
            } else {
                if (bp(dw(sentinel, 0)) == sentinel)
                    return;
                BYTE *b = bp(dw(h, 0x20));
                for (;;) {
                    if (!dw(b, 8) && (dw(b, 0xc) & 1))
                        break;
                    if (b == bp(dw(sentinel, 0)))
                        return;
                    b = bp(dw(b, 4));
                }
                fn<void(THISCALL *)(void *, void *)>(0x4d2a70)(h, b);
            }
            if ((BYTE)try_alloc(h, size, handle, flags))
                break;
        }
    }
    BYTE *b = bp(*handle);
    unlink(b);
    link_last(b, bp(dw(b, 0x1c)));
}
// 0x4d2bc0 (data): the block of the data (through its handle +0x18) unlocked. eax: the block.
BYTE *__stdcall Data_Unlock(BYTE *data)
{
    BYTE *b = bp(dw(bp(dw(data, (DWORD)-8)), 0));
    if (b && dw(b, 8))
        dw(b, 8)--;
    return b;
}

}  // namespace

ZAX_HOOKS(app_base)
{
    const Hook list[] = {
#include "app_base_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
