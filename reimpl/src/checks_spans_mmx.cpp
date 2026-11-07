// Checks of spans_mmx.cpp (T18-T19) with the MMX span checks of checks_spans_mmx.h, in the game thread: the live
// tables, every span directly with the MMX state afterwards, every alpha and opacity value, the portable
// form, the comparison with the scalar twins and the 16-bit RLE blitters on real sprites.
#include "checks_spans_mmx.h"

namespace zax {

int hooks_spans_mmx(Hook *out);

namespace {

using namespace spansmmxc;

ModuleHooks g_hooks{"spans_mmx", &hooks_spans_mmx, {}, {}, {}, 0, true};

}  // namespace

ZAX_CHECKS(spans_mmx)
{
    (void)cls;
    (void)n;
    render::g_span_mutation = read_span_mutation();
    r1::read_frame_trace();
    collect_sources(g_sources, 12);
    log("  spans_mmx: the CPU has MMX: %d", cpu_has_mmx() ? 1 : 0);
    SpanList spans;
    Tally tables{"spans_mmx: the tables T18-T19"};
    check_mmx_tables(tables, &hooks_spans_mmx, 18, 19, spans);
    tables.report();
    g_seed = 0x451310;
    Tally direct{"spans_mmx: T18-T19 directly with the MMX state (counts, alignment, 5-6-5 and 5-5-5, real pixels and alpha)"};
    check_mmx_direct(direct, spans, scaled(12), HOOK);
    direct.report();
    Tally alphas{"spans_mmx: every alpha value"};
    check_mmx_alphas(alphas, spans);
    alphas.report();
    Tally portable{"spans_mmx: the portable form against the original"};
    check_mmx_direct(portable, spans, scaled(3), PORTABLE_FORM);
    portable.report();
    Tally scalar{"spans_mmx: the scalar pixels against the scalar twins T12-T13"};
    ScalarStats st;
    compare_scalar(scalar, spans, scaled(40), st);
    scalar.report();
    report_scalar("spans_mmx", st);
    g_seed = 0x451311;
    Tally blits{"spans_mmx: 16-bit RLE blitters on real sprites through FindBlitter (Copy, Transparent Mask, Black Bias, Amplify Light, Overlap)"};
    check_blits(blits, g_hooks, scaled(1200), 1);
    blits.report();
    render::g_span_mutation = 0;
}

}  // namespace zax
