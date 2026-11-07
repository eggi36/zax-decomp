// The checks of the MMX 16-bit spans (checks_spans_mmx.cpp for spans_mmx.cpp T18/T19,
// checks_spans_mmx_opacity.cpp for spans_mmx_opacity.cpp T20-T23), on the frame check harness and the span
// cases of checks_spans16.h:
//  - the live tables T18-T23: entry 0 the stub, 1-16 the module's hooks, 17-63 one hooked generic loop;
//  - every span directly, the original (its trampoline) against the hooked address on the same fake rows
//    (every entry with its own count and with counts it ignores, the generic loops with every count 0-100,
//    all word alignments, 5-6-5 and 5-5-5, random and real pixels, random, edge and real alpha bytes,
//    garbage in the upper bits of values and alphas, the 256 fade tables and tables of random dwords), and
//    the MMX state afterwards: the x87 tag word, the top of stack and mm0-mm7 (set to a known state before
//    each call), so a missing or added `emms` shows;
//  - every alpha and every opacity value 0-255 for the entries with groups and a tail (4, 7, 16, generic);
//  - the portable form (spans_mmx.h, C = true) against the original on the same cases;
//  - the MMX spans against their scalar twins T12-T17 (both originals) on random and real pixels with the
//    real fade tables: the scalar part (entries 1-3, the tail pixels of every span) must be the same (a
//    check), the MMX groups are counted (how many pixels differ and by how much per channel, logged);
//  - the 16-bit RLE blitters of five blend modes on real sprites through FindBlitter (the live registry: the
//    MMX blitters when the CPU has MMX), side 0 with the module's hooks out (checks_spans16.h).
#pragma once
#include "checks_spans16.h"
#include "spans_mmx.h"

namespace zax::spansmmxc {

using namespace spans16c;

constexpr int FIRST_TABLE = 18;
// The scalar twin of T k is T k - 6 (T12-T17), the same kind
inline int kind_of_table(int k) { return ALPHA_COPY + (k - FIRST_TABLE); }

// ---- The MMX state -----------------------------------------------------------------------------------------------

// A known state before a call: mm0-mm7 zero, all tags empty
inline void mmx_reset()
{
    __asm__ volatile("pxor %%mm0, %%mm0\n\tpxor %%mm1, %%mm1\n\tpxor %%mm2, %%mm2\n\tpxor %%mm3, %%mm3\n\t"
                     "pxor %%mm4, %%mm4\n\tpxor %%mm5, %%mm5\n\tpxor %%mm6, %%mm6\n\tpxor %%mm7, %%mm7\n\temms" ::
                         : "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7", "st", "st(1)", "st(2)", "st(3)", "st(4)",
                           "st(5)", "st(6)", "st(7)");
}
struct MmxState {
    DWORD tags, top;
    unsigned long long mm[8];
};
// The state after a call (fnstenv masks the exceptions, fldenv puts the control word back), then emms
inline MmxState mmx_state()
{
    MmxState s;
    DWORD env[7];
    __asm__ volatile("fnstenv %0\n\tfldenv %0" : "+m"(env));
    s.tags = env[2] & 0xffff;
    s.top = (env[1] >> 11) & 7;
    __asm__ volatile("movq %%mm0, 0(%0)\n\tmovq %%mm1, 8(%0)\n\tmovq %%mm2, 16(%0)\n\tmovq %%mm3, 24(%0)\n\t"
                     "movq %%mm4, 32(%0)\n\tmovq %%mm5, 40(%0)\n\tmovq %%mm6, 48(%0)\n\tmovq %%mm7, 56(%0)\n\temms"
                     :
                     : "r"(s.mm)
                     : "memory", "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)");
    return s;
}
inline DWORD state_hash(const MmxState &s) { return fnv(2166136261u, &s, sizeof s); }

// ---- The tables ----------------------------------------------------------------------------------------------------

inline void check_mmx_tables(Tally &t, int (*table)(Hook *), int first, int last, SpanList &out)
{
    for (int k = first; k <= last; k++) {
        const DWORD *e = at<const DWORD *>(render::span_table(k));
        const int kind = kind_of_table(k);
        char d[120];
        wsprintfA(d, "[T%d[0] %x]", k, e[0]);
        t.check(e[0] == (takes_alpha(kind) ? 0x4f0a80u : 0x401110u), "", d);
        for (int n = 1; n <= 16; n++) {
            const char *name = hook_name(table, e[n]);
            wsprintfA(d, "[T%d[%d] %x: not a hook of the module]", k, n, e[n]);
            t.check(name != nullptr, "", d);
            if (name && out.n < 80)
                out.list[out.n++] = {e[n], kind, n, name};
        }
        const char *generic = hook_name(table, e[17]);
        bool same = generic != nullptr;
        for (int n = 18; n < 64; n++)
            same = same && e[n] == e[17];
        wsprintfA(d, "[T%d[17..63] %x: not one hooked generic loop]", k, e[17]);
        t.check(same, "", d);
        if (generic && out.n < 80)
            out.list[out.n++] = {e[17], kind, 0, generic};
    }
}

// ---- The portable spans (C = true), by kind and entry (0 the generic loop) ---------------------------------------

#define ZAX_PORTABLE(F)                                                                                                   \
    {                                                                                                                     \
        (void *)&F<0, true>, (void *)&F<1, true>, (void *)&F<2, true>, (void *)&F<3, true>, (void *)&F<4, true>,          \
            (void *)&F<5, true>, (void *)&F<6, true>, (void *)&F<7, true>, (void *)&F<8, true>, (void *)&F<9, true>,      \
            (void *)&F<10, true>, (void *)&F<11, true>, (void *)&F<12, true>, (void *)&F<13, true>, (void *)&F<14, true>, \
            (void *)&F<15, true>, (void *)&F<16, true>                                                                    \
    }
inline void *const PORTABLE[6][17] = {ZAX_PORTABLE(render::Span16_Alpha_Copy_MMX),        ZAX_PORTABLE(render::Span16_Alpha_Fill_MMX),
                                      ZAX_PORTABLE(render::Span16_Opacity_Copy_MMX),      ZAX_PORTABLE(render::Span16_Opacity_Fill_MMX),
                                      ZAX_PORTABLE(render::Span16_AlphaOpacity_Copy_MMX), ZAX_PORTABLE(render::Span16_AlphaOpacity_Fill_MMX)};
#undef ZAX_PORTABLE
inline DWORD portable_address(const SpanFn &f) { return ptrv(PORTABLE[f.kind - ALPHA_COPY][f.n]); }

// ---- The spans directly ----------------------------------------------------------------------------------------------

enum Against { HOOK, PORTABLE_FORM };

// One case: the original's trampoline on side 0, the hook (with the MMX state) or the portable form on side 1
inline void run_case(Tally &t, const DirectCase &c, Against against, DWORD portable)
{
    begin_case(random());
    FrameSpec s;
    s.width = ROW;
    s.height = 2;
    s.format = c.dst_format;
    const int dst = make_frame(s);
    s.format = c.src_format;
    const int src = make_frame(s);
    const DWORD alpha = alloc_object(ROW), table = c.opacity < 0 ? alloc_object(0x400) : 0xffffffffu;
    if (dst < 0 || src < 0 || alpha == 0xffffffffu || (c.opacity < 0 && table == 0xffffffffu))
        return;
    if (c.real_dst)
        if (const FrameData *f = pick_source(false))
            copy_source(dst, f);
    BYTE bytes[ROW * 2];
    if (c.real_src && block_bytes(pick_source(true), 0, bytes, sizeof bytes))
        for (int side = 0; side < 2; side++)
            memcpy(frame(side, src)->pixels, bytes, sizeof bytes);
    BYTE a[ROW];
    for (int i = 0; i < ROW; i++)
        a[i] = (BYTE)(random() >> 13);
    if (c.alpha_mode == 1)
        memset(a, EDGE_ALPHA[rnd(sizeof EDGE_ALPHA)], sizeof a);
    else if (c.alpha_mode == 2)
        for (int i = 0; i < ROW; i++)
            a[i] = EDGE_ALPHA[rnd(sizeof EDGE_ALPHA)];
    else if (c.alpha_mode == 4)
        memset(a, (BYTE)c.alpha_value, sizeof a);
    if (c.real_alpha) {
        const FrameData *f = pick_source(true);
        if (!block_bytes(f, 4, a, sizeof a))
            block_bytes(f, 3, a, sizeof a);
    }
    for (int side = 0; side < 2; side++) {
        memcpy(object<BYTE>(side, alpha), a, sizeof a);
        if (c.opacity < 0) {
            DWORD *tb = object<DWORD>(side, table);
            for (int i = 0; i < 256; i++)
                tb[i] = side ? object<DWORD>(0, table)[i] : (rnd(4) ? random() >> rnd(32) : random() & 0xff);
        }
    }
    char what[110];
    wsprintfA(what, "%s%s n %u dst %s+%d src %s+%d op %d", c.name, against == HOOK ? "" : " portable", c.n, format_name(c.dst_format),
              c.dst_x, format_name(c.src_format), c.src_x, c.opacity);
    const DWORD saved_table = *at<DWORD *>(render::OPACITY_TABLE);
    run_sides(t, g_no_hooks, what, [&](int side) -> DWORD {
        WORD *d = reinterpret_cast<WORD *>(frame(side, dst)->pixels) + c.dst_x;
        const DWORD sv = src_pointer(c.kind) ? ptrv(reinterpret_cast<WORD *>(frame(side, src)->pixels) + c.src_x) : c.value;
        const DWORD av = alpha_pointer(c.kind) ? ptrv(object<BYTE>(side, alpha)) : c.alpha_value;
        *at<DWORD *>(render::OPACITY_TABLE) = c.opacity < 0 ? ptrv(object<DWORD>(side, table)) : render::FADE_TABLES + (DWORD)c.opacity * 0x400;
        const DWORD fn = side == 0 ? ptrv(original_address(c.address)) : against == HOOK ? c.address : portable;
        mmx_reset();
        if (takes_alpha(c.kind))
            reinterpret_cast<render::AlphaSpan>(fn)(d, sv, av, c.n);
        else
            reinterpret_cast<render::Span>(fn)(d, sv, c.n);
        const MmxState st = mmx_state();
        *at<DWORD *>(render::OPACITY_TABLE) = saved_table;
        return against == HOOK ? state_hash(st) : 0;   // compared as eax
    });
}

// Every span of the list: its own count (a generic loop: every count 17-63) `per` times, counts the unrolled
// ones ignore, the generic loops with 0-16 and 64-100
inline void check_mmx_direct(Tally &t, const SpanList &spans, int per, Against against)
{
    for (int i = 0; i < spans.n; i++) {
        const SpanFn &f = spans.list[i];
        const DWORD portable = portable_address(f);
        if (f.n) {
            for (int k = 0; k < per; k++)
                run_case(t, random_case(f, k % 4 == 3 ? rnd(100) : (DWORD)f.n), against, portable);
            continue;
        }
        for (DWORD n = 17; n < 64; n++)
            for (int k = 0; k < per; k++)
                run_case(t, random_case(f, n), against, portable);
        for (DWORD n = 0; n <= 16; n++)
            run_case(t, random_case(f, n), against, portable);
        for (int k = 0; k < per; k++)
            run_case(t, random_case(f, 64 + rnd(37)), against, portable);
    }
}

// The entries with MMX groups and a scalar tail: 4 (groups only), 7 (one group, three tail pixels), 16, generic
inline bool sampled_entry(const SpanFn &f) { return f.n == 0 || f.n == 4 || f.n == 7 || f.n == 16; }

// Every opacity 0-255 (the fade tables) for the spans that read one
inline void check_mmx_opacities(Tally &t, const SpanList &spans)
{
    for (int i = 0; i < spans.n; i++) {
        const SpanFn &f = spans.list[i];
        if (!reads_opacity(f.kind) || !sampled_entry(f))
            continue;
        for (int op = 0; op < 256; op++) {
            DirectCase c = random_case(f, f.n ? (DWORD)f.n : 17 + rnd(47));
            c.opacity = op;
            run_case(t, c, HOOK, 0);
        }
    }
}
// Every alpha value 0-255 for the alpha spans: all alpha bytes the value (alpha mode 4) or the fill's alpha
// with garbage in its upper bits
inline void check_mmx_alphas(Tally &t, const SpanList &spans)
{
    for (int i = 0; i < spans.n; i++) {
        const SpanFn &f = spans.list[i];
        if (!takes_alpha(f.kind) || !sampled_entry(f))
            continue;
        for (DWORD a = 0; a < 256; a++) {
            DirectCase c = random_case(f, f.n ? (DWORD)f.n : 17 + rnd(47));
            c.real_alpha = false;
            c.alpha_mode = 4;
            c.alpha_value = alpha_pointer(f.kind) ? a : a | (rnd(2) ? random() & 0xffffff00u : 0);
            run_case(t, c, HOOK, 0);
        }
    }
}

// ---- The MMX spans against their scalar twins (both originals) ------------------------------------------------------

struct ScalarStats {
    unsigned cases = 0, group_pixels = 0, differ = 0, differ_cases = 0, dev[3] = {0, 0, 0};
};

// `per` cases per span with random and real pixels, random, edge and real alpha bytes and a real fade table:
// the pixels the MMX span blends in C (entries 1-3, the tail) and the pixels after the span must be those of
// the scalar twin (a check); the pixels of the MMX groups are counted into `st`
inline void compare_scalar(Tally &t, const SpanList &spans, int per, ScalarStats &st)
{
    const DWORD saved_table = *at<DWORD *>(render::OPACITY_TABLE);
    for (int i = 0; i < spans.n; i++) {
        const SpanFn &f = spans.list[i];
        const int twin_table = FIRST_TABLE - 6 + (f.kind - ALPHA_COPY);
        const DWORD twin = at<const DWORD *>(render::span_table(twin_table))[f.n ? f.n : 17];
        for (int k = 0; k < per; k++) {
            const DWORD n = f.n ? (DWORD)f.n : 1 + rnd(63);
            WORD d0[ROW], d1[ROW], s[ROW];
            BYTE a[ROW];
            for (int j = 0; j < ROW; j++) {
                d0[j] = (WORD)(random() >> 9);
                s[j] = (WORD)(random() >> 11);
                a[j] = rnd(3) ? (BYTE)(random() >> 13) : EDGE_ALPHA[rnd(sizeof EDGE_ALPHA)];
            }
            if (rnd(2))
                block_bytes(pick_source(true), 0, reinterpret_cast<BYTE *>(s), sizeof s);
            if (rnd(2))
                block_bytes(pick_source(true), 0, reinterpret_cast<BYTE *>(d0), sizeof d0);
            if (rnd(3) == 0) {
                const FrameData *src = pick_source(true);
                if (!block_bytes(src, 4, a, sizeof a))
                    block_bytes(src, 3, a, sizeof a);
            }
            memcpy(d1, d0, sizeof d0);
            const DWORD value = rnd(2) ? s[rnd(ROW)] | (random() & 0xffff0000u) : random();
            const DWORD alpha_value = rnd(2) ? a[0] | (rnd(2) ? random() & 0xffffff00u : 0) : a[0];
            *at<DWORD *>(render::OPACITY_TABLE) = render::FADE_TABLES + (rnd(4) ? rnd(256) : 255) * 0x400;
            WORD *dd[2] = {d0, d1};
            const DWORD fns[2] = {ptrv(original_address(f.address)), ptrv(original_address(twin))};
            for (int side = 0; side < 2; side++) {
                const DWORD sv = src_pointer(f.kind) ? ptrv(s) : value;
                const DWORD av = alpha_pointer(f.kind) ? ptrv(a) : alpha_value;
                if (takes_alpha(f.kind))
                    reinterpret_cast<render::AlphaSpan>(fns[side])(dd[side], sv, av, n);
                else
                    reinterpret_cast<render::Span>(fns[side])(dd[side], sv, n);
                mmx_reset();
            }
            const DWORD groups = n >> 2;
            bool scalar_same = true, differs = false;
            for (DWORD j = 0; j < (DWORD)ROW; j++) {
                if (j >= groups * 4) {
                    scalar_same = scalar_same && d0[j] == d1[j];
                    continue;
                }
                st.group_pixels++;
                if (d0[j] == d1[j])
                    continue;
                st.differ++;
                differs = true;
                const int dr = (d0[j] >> 11) - (d1[j] >> 11), dg = ((d0[j] >> 5) & 63) - ((d1[j] >> 5) & 63), db = (d0[j] & 31) - (d1[j] & 31);
                const int dv[3] = {dr < 0 ? -dr : dr, dg < 0 ? -dg : dg, db < 0 ? -db : db};
                for (int c = 0; c < 3; c++)
                    if ((unsigned)dv[c] > st.dev[c])
                        st.dev[c] = (unsigned)dv[c];
            }
            st.cases++;
            st.differ_cases += differs;
            char d[120];
            wsprintfA(d, "[%s n %u against T%d %x: a scalar pixel differs]", f.name, n, twin_table, twin);
            t.check(scalar_same, "", d);
        }
    }
    *at<DWORD *>(render::OPACITY_TABLE) = saved_table;
}

inline void report_scalar(const char *what, const ScalarStats &st)
{
    const unsigned permille = st.group_pixels ? (st.differ < 4000000u ? st.differ * 1000 / st.group_pixels : st.differ / (st.group_pixels / 1000)) : 0;
    log("  %s: MMX against scalar: %u of %u pixels of the MMX groups differ (%u.%u %%) in %u of %u cases, the most "
        "per channel red %u green %u blue %u",
        what, st.differ, st.group_pixels, permille / 10, permille % 10, st.differ_cases, st.cases, st.dev[0], st.dev[1], st.dev[2]);
}

}  // namespace zax::spansmmxc
