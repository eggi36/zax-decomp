// Checks of spans16.cpp (T10-T13, Amplify Light, Overlap) with the span checks of checks_spans16.h, in the
// game thread: the live tables, every span directly (all counts, alignments, both pixel formats, random and
// real pixels, every alpha value), and the 16-bit RLE blitters of five blend modes on real sprites with the
// module's hooks out and in (mostly opacity 1.0: the paths through these spans).
// [Reimpl] SpanMutation= sets the reimplementation's deliberate bugs (spans16.h) for the run.
#include "checks_spans16.h"

namespace zax {

int hooks_spans16(Hook *out);

namespace {

using namespace spans16c;

ModuleHooks g_hooks{"spans16", &hooks_spans16, {}, {}, {}, 0, true};

}  // namespace

ZAX_CHECKS(spans16)
{
    (void)cls;
    (void)n;
    render::g_span_mutation = read_span_mutation();
    if (render::g_span_mutation)
        log("  SpanMutation=%d: the reimplementation's deliberate bugs are on", render::g_span_mutation);
    r1::read_frame_trace();
    collect_sources(g_sources, 12);
    SpanList spans;
    Tally tables{"spans16: the tables T10-T13, Amplify Light and Overlap"};
    check_tables(tables, &hooks_spans16, 10, 13, spans);
    for (DWORD tb : render::AMPLIFY_LITERAL_TABLES)
        check_dynamic_table(tables, tb, 0x434280);
    for (DWORD tb : render::AMPLIFY_RUN_TABLES)
        check_dynamic_table(tables, tb, 0x434310);
    check_dynamic_table(tables, render::OVERLAP_TABLES[0], 0x446a10);
    check_dynamic_table(tables, render::OVERLAP_TABLES[1], 0x446a10);
    check_dynamic_table(tables, render::OVERLAP_TABLES[2], 0x446a30);
    check_dynamic_table(tables, render::OVERLAP_TABLES[3], 0x446a30);
    tables.report();
    const SpanFn extra[] = {{0x434280, AMP_LITERAL, 0, "Amplify16_Literal"},
                            {0x434310, AMP_FILL, 0, "Amplify16_Fill"},
                            {0x446a10, OVL_FILL, 0, "Overlap16_Fill"},
                            {0x446a30, OVL_FILL_ALPHA, 0, "Overlap16_FillAlpha"}};
    SpanList others;
    for (const SpanFn &f : extra)
        others.list[others.n++] = f;
    g_seed = 0x44bbc0;
    Tally direct{"spans16: T10-T13 directly (counts, alignment, 5-6-5 and 5-5-5, real pixels and alpha)"};
    check_direct(direct, spans, scaled(12));
    direct.report();
    Tally alphas{"spans16: T12/T13 every alpha value"};
    check_alphas(alphas, spans);
    alphas.report();
    g_seed = 0x434280;
    Tally amplify{"spans16: Amplify Light and Overlap spans directly"};
    check_direct(amplify, others, scaled(12));
    amplify.report();
    g_seed = 0x438c40;
    Tally blits{"spans16: 16-bit RLE blitters on real sprites (Copy, Transparent Mask, Black Bias, Amplify Light, Overlap)"};
    check_blits(blits, g_hooks, scaled(1200), 3);
    blits.report();
    render::g_span_mutation = 0;
}

}  // namespace zax
