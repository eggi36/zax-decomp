// The 8-bit spans of the software renderer (phase 4, batch R2 of the blitter map, docs/DECOMPILATION.md 9):
// the span tables T0-T9, the per-mode 8-bit table spans of Amplify Light and Black Bias and the 8 -> 16
// palette spans of the Transparent Mask tables. spans8.cpp has the functions, checks_spans8.cpp the checks.
//
// A span draws one RLE segment into a row: fastcall (ecx dst, edx src or value; n on the stack), ret 4. A
// literal ("Copy") draws n source pixels, a run ("Fill") n pixels of the value's low byte (the upper bytes
// of edx are ignored everywhere). The callers, the RLE frame drawers and edge rows of R5, call
// `[n * 4 + table]` [confirmed: code]:
//  - T0-T35 at SPAN_TABLES + k * 0x100, 64 entries each: entry 0 a no-op (`ret 4`), entries 1-16 one unrolled
//    instance per count that ignores n (the template's N; dword moves for the multiples of 4, then the
//    tail from its last byte down), entries 17-63 one generic loop over n (CBlendTransparentMask_Ctor
//    0x4479c0 writes them at start). T0/T1 copy and fill, T2-T9 blend through the 64 KB tables of the
//    palette objects Alpha20_80 0x6767d0 and Alpha40_60 0x6767c0 (their data pointer read per pixel):
//    "DS" index [dst * 256 + src], "SD" [src * 256 + dst]; T2/T3 20/80 DS, T4/T5 40/60 DS, T6/T7 40/60 SD,
//    T8/T9 20/80 SD.
//  - The dynamic tables of Amplify Light and Black Bias (all 64 entries one function, entry 0 too: those
//    loops run 2^32 times for n = 0): the current mode table g_Blend8Table 0x6bcc64 (the blitter sets it
//    from the mode's palette data), plain [src * 256 + dst] or with a row offset from one of four
//    256-dword tables: [rows[src] + dst].
//  - The two Transparent Mask tables 0x6bcc9c (literal) and 0x6bcda0 (run): 8-bit sources to 16-bit
//    pixels through the low word of the dword table g_Palette16 0x6343b0 (entry i at 0x6343b0 + 4 i).
#pragma once
#include "frame.h"

namespace zax::render {

// fastcall (dst, src or value; n), ret 4
using Span = void(__fastcall *)(void *dst, DWORD src_or_value, DWORD n);

constexpr DWORD SPAN_ENTRIES = 64, SPAN_UNROLLED = 16;   // the tables themselves: blitters.h T0-T35 (the checks: span_table)

// The 8-bit tables T0-T9 (span_table(k)); the blend table of T2-T9 (a palette object, data at +0)
enum Span8Table { T_COPY8, T_FILL8, T_A2080DS_COPY, T_A2080DS_FILL, T_A4060DS_COPY, T_A4060DS_FILL, T_A4060SD_COPY,
                  T_A4060SD_FILL, T_A2080SD_COPY, T_A2080SD_FILL };
constexpr DWORD BLEND8_TABLE = 0x6bcc64;   // g_Blend8Table: the current 64 KB mode table of the 8-bit RLE blitters
// The row offset tables of the mode table spans A-D (256 dwords each)
constexpr DWORD MODE_ROWS_A = 0x632b80, MODE_ROWS_B = 0x633794, MODE_ROWS_C = 0x62ef18, MODE_ROWS_D = 0x674bb0;
constexpr DWORD PALETTE16_DWORDS = 0x6343b0;   // g_Palette16: 256 dwords, the low word the 5-6-5 pixel
// The Transparent Mask's 8 -> 16 tables
constexpr DWORD SPAN8TO16_COPY_TABLE = 0x6bcc9c, SPAN8TO16_FILL_TABLE = 0x6bcda0;

}  // namespace zax::render
