// Checks of spans_mmx_opacity.cpp (T20-T23) with the MMX span checks of checks_spans_mmx.h, in the game thread: the live
// tables, every span directly with the MMX state afterwards, every alpha and opacity value, the portable
// form, the comparison with the scalar twins and the 16-bit RLE blitters on real sprites.
#include "checks_spans_mmx.h"

namespace zax {

int hooks_spans_mmx_opacity(Hook *out);

namespace {

using namespace spansmmxc;

ModuleHooks g_hooks{"spans_mmx_opacity", &hooks_spans_mmx_opacity, {}, {}, {}, 0, true};

}  // namespace

ZAX_CHECKS(spans_mmx_opacity)
{
    (void)cls;
    (void)n;
    render::g_span_mutation = read_span_mutation();
    r1::read_frame_trace();
    collect_sources(g_sources, 12);
    log("  spans_mmx_opacity: the CPU has MMX: %d", cpu_has_mmx() ? 1 : 0);
    SpanList spans;
    Tally tables{"spans_mmx_opacity: the tables T20-T23"};
    check_mmx_tables(tables, &hooks_spans_mmx_opacity, 20, 23, spans);
    tables.report();
    g_seed = 0x4550a0;
    Tally direct{"spans_mmx_opacity: T20-T23 directly with the MMX state (counts, alignment, 5-6-5 and 5-5-5, real pixels and alpha)"};
    check_mmx_direct(direct, spans, scaled(12), HOOK);
    direct.report();
    Tally opacities{"spans_mmx_opacity: every opacity (the 256 fade tables)"};
    check_mmx_opacities(opacities, spans);
    opacities.report();
    Tally alphas{"spans_mmx_opacity: every alpha value"};
    check_mmx_alphas(alphas, spans);
    alphas.report();
    Tally portable{"spans_mmx_opacity: the portable form against the original"};
    check_mmx_direct(portable, spans, scaled(3), PORTABLE_FORM);
    portable.report();
    Tally scalar{"spans_mmx_opacity: the scalar pixels against the scalar twins T14-T17"};
    ScalarStats st;
    compare_scalar(scalar, spans, scaled(40), st);
    scalar.report();
    report_scalar("spans_mmx_opacity", st);
    g_seed = 0x4550a1;
    Tally blits{"spans_mmx_opacity: 16-bit RLE blitters on real sprites through FindBlitter (Copy, Transparent Mask, Black Bias, Amplify Light, Overlap)"};
    check_blits(blits, g_hooks, scaled(1200), 1);
    blits.report();
    render::g_span_mutation = 0;
}

}  // namespace zax
