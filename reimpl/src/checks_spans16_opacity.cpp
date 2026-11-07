// Checks of spans16_opacity.cpp (T14-T17) with the span checks of checks_spans16.h, in the game thread: the
// live tables, every span directly (all counts, alignments, both pixel formats, random and real pixels,
// every opacity through the 256 fade tables and tables of random dwords, every alpha value), and the
// 16-bit RLE blitters of five blend modes on real sprites with the module's hooks out and in (mostly an
// opacity below 1.0: the paths through these spans).
#include "checks_spans16.h"

namespace zax {

int hooks_spans16_opacity(Hook *out);

namespace {

using namespace spans16c;

ModuleHooks g_hooks{"spans16_opacity", &hooks_spans16_opacity, {}, {}, {}, 0, true};

}  // namespace

ZAX_CHECKS(spans16_opacity)
{
    (void)cls;
    (void)n;
    render::g_span_mutation = read_span_mutation();
    r1::read_frame_trace();
    collect_sources(g_sources, 12);
    SpanList spans;
    Tally tables{"spans16 opacity: the tables T14-T17"};
    check_tables(tables, &hooks_spans16_opacity, 14, 17, spans);
    tables.report();
    g_seed = 0x44da00;
    Tally direct{"spans16 opacity: T14-T17 directly (counts, alignment, 5-6-5 and 5-5-5, real pixels and alpha)"};
    check_direct(direct, spans, scaled(12));
    direct.report();
    Tally opacities{"spans16 opacity: every opacity (the 256 fade tables)"};
    check_opacities(opacities, spans);
    opacities.report();
    Tally alphas{"spans16 opacity: T16/T17 every alpha value"};
    check_alphas(alphas, spans);
    alphas.report();
    g_seed = 0x438c41;
    Tally blits{"spans16 opacity: 16-bit RLE blitters on real sprites with opacity (Copy, Transparent Mask, Black Bias, Amplify Light, Overlap)"};
    check_blits(blits, g_hooks, scaled(1200), 1);
    blits.report();
    render::g_span_mutation = 0;
}

}  // namespace zax
