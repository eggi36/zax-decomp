// The blend mode core (layouts in frame.h): CBlendMode's constructor, destructor, registry, FindBlitter and
// opacity, the constructors of the nine modes built by a real constructor (the other six, Normal Scale,
// ScreenCopy, Tile, TileWrap, Overlap and Filter, are built inline in their static initializers), the
// deleting destructors, the screen modes' levels, the hue remap (constructor, destructors, the two table
// builders and Hue_Lerp), the Overlap colour ramp and its FindBlitter, the 5-5-5 pixel, the game palette's
// 16-bit forms and the 8-bit blend helper tables (docs/DECOMPILATION.md 9, "The blitter range").
//
// Not here: the static initializers and atexit stubs (they run before the DLL is loaded or at the exit;
// their effect, the registries and the dynamic span tables, is compared in the running game), the name
// registrations (RegisterBlend_* add to the blend name list once), InitBlendTables 0x433000 (file I/O:
// it loads, rebuilds and saves the .pal caches), the spans and blitters (R2-R6), the Filter internals.
#include "frame.h"

namespace zax {

using namespace render;

namespace {

void reg(CBlendMode *m, DWORD sb, DWORD sf, DWORD db, DWORD df, DWORD fn)
{
    zax::fn<DWORD(THISCALL *)(CBlendMode *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x432de0)(m, sb, sf, db, df, fn);
}
void base_ctor(CBlendMode *m) { zax::fn<CBlendMode *(THISCALL *)(CBlendMode *)>(0x432d60)(m); }
void base_dtor(CBlendMode *m) { zax::fn<void(THISCALL *)(CBlendMode *)>(0x432da0)(m); }
// rep stosd of a span table: n entries of fn
void fill_table(DWORD table, DWORD fn, DWORD n = 0x40)
{
    DWORD *t = at<DWORD *>(table);
    for (DWORD i = 0; i < n; i++)
        t[i] = fn;
}
DWORD min31(DWORD v) { return v < 0x1f ? v : 0x1f; }

}  // namespace

// ---- CBlendMode ----------------------------------------------------------------------------------------------------

// 0x432d60: the palette (empty), the registry (Array20), no cache, opacity 1.0 (255)
CBlendMode *THISCALL BlendMode_Ctor(CBlendMode *m)
{
    palette_ctor(&m->palette);
    m->registry.vftable = zax::vt(VT_BLEND_ARRAY);
    m->registry.data = nullptr;
    m->registry.count = 0;
    m->registry.capacity = 0;
    m->cache_fn = 0;
    m->opacity_entry = 0;
    m->cache_key = 0;
    m->vftable = zax::vt(VT_BLEND_MODE);
    m->opacity = raw_float(0x3f800000);
    m->opacity255 = 0xff;
    return m;
}

// 0x432da0: the registry's records freed, the palette freed
void THISCALL BlendMode_Dtor(CBlendMode *m)
{
    m->vftable = zax::vt(VT_BLEND_MODE);
    m->registry.vftable = zax::vt(VT_BLEND_ARRAY);
    if (m->registry.data) {
        heap_free(m->registry.data);
        m->registry.data = nullptr;
        m->registry.capacity = 0;
    }
    m->registry.count = 0;
    palette_free(&m->palette);
}

// 0x432de0 (src bpp, src flags, dst bpp, dst flags, function): replaces the function of an existing key,
// else appends the record (the registry's slot 2, Array20_AddHeap); eax the record or Add's result
DWORD THISCALL BlendMode_RegisterBlitter(CBlendMode *m, DWORD sb, DWORD sf, DWORD db, DWORD df, DWORD fn)
{
    BlendRecord *r = m->registry.data;
    for (DWORD i = 0; i < m->registry.count; i++, r++)
        if (r->src_bpp == sb && r->src_flags == sf && r->dst_bpp == db && r->dst_flags == df) {
            r->fn = fn;
            return (DWORD)(uintptr_t)r;
        }
    BlendRecord rec = {sb, sf, db, df, fn};
    return vcall<DWORD>(&m->registry, 2, &rec);
}

// 0x432e80 (slot 2; src, dst): the key (src flags & 0x1ee) << 16 + (dst flags & 0x1ee) against the one-entry
// cache; else the registry searched for (src bpp, src flags & 0xe, dst bpp, dst flags & 0xe) and the
// cache set (also to 0 when none fits)
DWORD THISCALL BlendMode_FindBlitter(CBlendMode *m, void *src, void *dst)
{
    const DWORD sk = f_flags(src) & 0x1ee;
    const DWORD key = (sk << 16) + (f_flags(dst) & 0x1ee);
    if (key == m->cache_key)
        return m->cache_fn;
    const DWORD df = f_flags(dst) & 0xe, db = f_bits(dst), sf = f_flags(src) & 0xe, sb = f_bits(src);
    DWORD fn = 0;
    for (DWORD i = 0; i < m->registry.count; i++) {
        const BlendRecord r = m->registry.data[i];
        if (r.src_bpp == sb && r.src_flags == sf && r.dst_bpp == db && r.dst_flags == df) {
            fn = r.fn;
            break;
        }
    }
    m->cache_fn = fn;
    m->cache_key = key;
    return fn;
}

// 0x433be0 (opacity): stored as given, the 0..255 value rounded (fistp) and its table entry
void THISCALL BlendMode_SetOpacity(CBlendMode *m, DWORD opacity_bits)
{
    m->opacity = raw_float(opacity_bits);
    const int o = x87_fistp(fld(fstp(fld(raw_float(opacity_bits)) * cst(glob_addr(0x5eb728)))));
    m->opacity255 = (DWORD)o;
    m->opacity_entry = glob<DWORD *>(OPACITY_ENTRIES)[o];
}
// 0x432f90 (slot 4; stream): the opacity written (the stream's slot 2, 4 bytes)
DWORD THISCALL BlendMode_WriteOpacity(CBlendMode *m, void *stream) { return vcall<DWORD>(stream, 2, &m->opacity, 4); }
// 0x432fb0 (slot 5; stream): 4 bytes read (slot 3), then as SetOpacity (the value stored through fst)
void THISCALL BlendMode_ReadOpacity(CBlendMode *m, void *stream)
{
    float v;
    vcall<DWORD>(stream, 3, &v, 4);
    const long double l = fld(v);
    m->opacity = fstp(l);
    const int o = x87_fistp(fld(fstp(l * cst(glob_addr(0x5eb728)))));
    m->opacity255 = (DWORD)o;
    m->opacity_entry = glob<DWORD *>(OPACITY_ENTRIES)[o];
}

// 0x441b50 (slot 0 of 11 mode vtables), 0x433d90 (Amplify Light), 0x438640 (Black Bias): the base destructor,
// operator delete with bit 0
void *THISCALL BlendMode_DeletingDtor(CBlendMode *m, DWORD flags)
{
    base_dtor(m);
    if (flags & 1)
        heap_free(m);
    return m;
}
void *THISCALL AmplifyLight_DeletingDtor(CBlendMode *m, DWORD flags)
{
    m->vftable = zax::vt(0x5eee34);
    base_dtor(m);
    if (flags & 1)
        heap_free(m);
    return m;
}
void *THISCALL BlackBias_DeletingDtor(CBlendMode *m, DWORD flags)
{
    m->vftable = zax::vt(0x5eee50);
    base_dtor(m);
    if (flags & 1)
        heap_free(m);
    return m;
}

// ---- The mode constructors ---------------------------------------------------------------------------------------

// 0x433df0: the amplify tables (word tables 0x6b7cf0 red << 11, 0x6b8928 << 6, 0x6b67f0 of 32 x 32:
// min(31, s + j * s / 31)), its dynamic span tables (16-bit literal/run, the 8-bit table spans; the edge span
// tables 0x6b76f0, 0x6b70f0, 0x6b75f0, 0x6b84f0 zero) and 8 blitters
CBlendMode *THISCALL AmplifyLight_Ctor(CBlendMode *m)
{
    base_ctor(m);
    m->vftable = zax::vt(0x5eee34);
    for (DWORD s = 0; s < 0x20; s++)
        for (DWORD j = 0, acc = 0; j < 0x20; j++, acc += s) {
            const DWORD v = acc / 31 + s;
            glob<WORD *>(0x6b7cf0)[s * 32 + j] = (WORD)(min31(v) << 11);
            glob<WORD *>(0x6b8928)[s * 32 + j] = (WORD)(min31(v) << 6);
            glob<WORD *>(0x6b67f0)[s * 32 + j] = (WORD)min31(v);
        }
    fill_table(glob_addr(0x6b74f0), fn_addr(0x434310));   // Amplify16_Fill
    fill_table(glob_addr(0x6b6ff0), fn_addr(0x434280));   // Amplify16_Literal
    fill_table(glob_addr(0x6b7af0), fn_addr(0x434310));   // Amplify16_Fill
    fill_table(glob_addr(0x6b78f0), fn_addr(0x434280));   // Amplify16_Literal
    fill_table(glob_addr(0x6b8828), fn_addr(0x434250));   // Span8_ModeTableD_Fill
    fill_table(glob_addr(0x6b73f0), fn_addr(0x434210));   // Span8_ModeTableD_Copy
    fill_table(glob_addr(0x6b72f0), fn_addr(0x4341e0));   // Span8_ModeTableC_Fill
    fill_table(glob_addr(0x6b8728), fn_addr(0x4341a0));   // Span8_ModeTableC_Copy
    fill_table(glob_addr(0x6b8628), fn_addr(0x434170));   // Span8_ModeTableB_Fill
    fill_table(glob_addr(0x6b9128), fn_addr(0x434130));   // Span8_ModeTableB_Copy
    fill_table(glob_addr(0x6b71f0), fn_addr(0x434100));   // Span8_ModeTableA_Fill
    fill_table(glob_addr(0x6b77f0), fn_addr(0x4340c0));   // Span8_ModeTableA_Copy
    fill_table(glob_addr(0x6b7bf0), fn_addr(0x434090));   // Span8_ModeTable_Fill
    fill_table(glob_addr(0x6b79f0), fn_addr(0x434060));   // Span8_ModeTable_Copy
    fill_table(glob_addr(0x6b76f0), 0);
    fill_table(glob_addr(0x6b70f0), 0);
    fill_table(glob_addr(0x6b75f0), 0);
    fill_table(glob_addr(0x6b84f0), 0);
    reg(m, 8, 0, 8, 0, fn_addr(0x434390));   // BlitBlackBias_8To8
    reg(m, 16, 0, 16, 0, cpu_has_mmx() ? fn_addr(0x434600) : fn_addr(0x434a10));   // BlitBlackBias_16To16_MMX / BlitCopy_16To16
    reg(m, 24, 0, 24, 0, fn_addr(0x434cf0));   // BlitCopy_24To24
    reg(m, 32, 0, 24, 0, fn_addr(0x434f50));   // BlitCopy_32To24
    reg(m, 32, 0, 32, 0, fn_addr(0x435300));   // BlitCopy_32To32
    reg(m, 8, 2, 8, 0, fn_addr(0x435570));   // BlitAmplifyLight_8RleTo8
    reg(m, 16, 4, 16, 0, cpu_has_mmx() ? fn_addr(0x4355f0) : fn_addr(0x435950));   // BlitAmplifyLight_16RleTo16_MMX / _NoMMX
    reg(m, 24, 4, 24, 0, fn_addr(0x435cb0));   // BlitTransparentMask_24RleTo24
    return m;
}

// 0x4386f0 (Black Bias and Test Light): the 8-bit table spans and 8 blitters
CBlendMode *THISCALL BlackBias_Ctor(CBlendMode *m)
{
    base_ctor(m);
    m->vftable = zax::vt(0x5eee50);
    fill_table(glob_addr(0x6b9528), fn_addr(0x434250));   // Span8_ModeTableD_Fill
    fill_table(glob_addr(0x6b9328), fn_addr(0x434210));   // Span8_ModeTableD_Copy
    fill_table(glob_addr(0x6b995c), fn_addr(0x4341e0));   // Span8_ModeTableC_Fill
    fill_table(glob_addr(0x6b985c), fn_addr(0x4341a0));   // Span8_ModeTableC_Copy
    fill_table(glob_addr(0x6b9228), fn_addr(0x434170));   // Span8_ModeTableB_Fill
    fill_table(glob_addr(0x6b9628), fn_addr(0x434130));   // Span8_ModeTableB_Copy
    fill_table(glob_addr(0x6b975c), fn_addr(0x434100));   // Span8_ModeTableA_Fill
    fill_table(glob_addr(0x6b9b5c), fn_addr(0x4340c0));   // Span8_ModeTableA_Copy
    fill_table(glob_addr(0x6b9a5c), fn_addr(0x434090));   // Span8_ModeTable_Fill
    fill_table(glob_addr(0x6b9428), fn_addr(0x434060));   // Span8_ModeTable_Copy
    reg(m, 8, 0, 8, 0, fn_addr(0x434390));   // BlitBlackBias_8To8
    reg(m, 16, 0, 16, 0, cpu_has_mmx() ? fn_addr(0x434600) : fn_addr(0x434a10));   // BlitBlackBias_16To16_MMX / BlitCopy_16To16
    reg(m, 24, 0, 24, 0, fn_addr(0x434cf0));   // BlitCopy_24To24
    reg(m, 32, 0, 24, 0, fn_addr(0x434f50));   // BlitCopy_32To24
    reg(m, 32, 0, 32, 0, fn_addr(0x435300));   // BlitCopy_32To32
    reg(m, 8, 2, 8, 0, fn_addr(0x438870));   // BlitBlackBias_8RleTo8
    reg(m, 16, 4, 16, 0, cpu_has_mmx() ? fn_addr(0x4388f0) : fn_addr(0x438c40));   // BlitCopy_16RleTo16_MMX / _NoMMX
    reg(m, 24, 4, 24, 0, fn_addr(0x435cb0));   // BlitTransparentMask_24RleTo24
    return m;
}

// 0x43eac0: the default blit, 18 blitters
CBlendMode *THISCALL Copy_Ctor(CBlendMode *m)
{
    base_ctor(m);
    m->vftable = zax::vt(0x5eef54);
    reg(m, 8, 0, 8, 0, fn_addr(0x43ec50));   // BlitCopy_8To8
    reg(m, 8, 0, 16, 0, fn_addr(0x43ee70));   // BlitCopy_8To16
    reg(m, 8, 0, 24, 0, fn_addr(0x43f070));   // BlitCopy_8To24
    reg(m, 8, 0, 32, 0, fn_addr(0x43f300));   // BlitCopy_8To32
    reg(m, 16, 0, 8, 0, fn_addr(0x43f570));   // BlitCopy_16To8
    reg(m, 16, 0, 16, 0, fn_addr(0x434a10));   // BlitCopy_16To16
    reg(m, 16, 0, 24, 0, fn_addr(0x43f780));   // BlitCopy_16To24
    reg(m, 16, 0, 32, 0, fn_addr(0x43f9b0));   // BlitCopy_16To32
    reg(m, 24, 0, 8, 0, fn_addr(0x43fbf0));   // BlitCopy_24To8
    reg(m, 24, 0, 16, 0, fn_addr(0x43fe50));   // BlitCopy_24To16
    reg(m, 24, 0, 24, 0, fn_addr(0x434cf0));   // BlitCopy_24To24
    reg(m, 24, 0, 32, 0, fn_addr(0x4400b0));   // BlitCopy_24To32
    reg(m, 32, 0, 24, 0, fn_addr(0x434f50));   // BlitCopy_32To24
    reg(m, 32, 0, 32, 0, fn_addr(0x435300));   // BlitCopy_32To32
    reg(m, 8, 2, 8, 0, fn_addr(0x4402d0));   // BlitCopy_8RleTo8
    reg(m, 8, 2, 16, 0, fn_addr(0x440330));   // BlitCopy_8RleTo16
    reg(m, 16, 4, 16, 0, cpu_has_mmx() ? fn_addr(0x4388f0) : fn_addr(0x438c40));   // BlitCopy_16RleTo16_MMX / _NoMMX
    reg(m, 16, 4, 32, 0, fn_addr(0x440390));   // BlitCopy_16RleTo32
    return m;
}

// The screen modes (+0x34 the level): 0x443220/0x443310 (5-5-5) fill 64 entries min(i, 31) << 10, min(i, 31),
// min(i, 31) << 5; 0x444ea0/0x444fd0 (5-6-5) 128 entries min(i, 63) << 5, then 64 entries min(i, 31) << 11 and
// min(i, 31)
struct ScreenTables {
    DWORD vt, red, plain, green, fn16_mmx, fn16, fn8, fn24;
};
CBlendMode *screen555(CBlendMode *m, const ScreenTables &t)
{
    base_ctor(m);
    m->vftable = t.vt;
    reinterpret_cast<DWORD *>(m)[0xd] = 0;
    for (DWORD i = 0; i < 0x40; i++) {
        at<DWORD *>(t.red)[i] = min31(i) << 10;
        at<DWORD *>(t.plain)[i] = min31(i);
        at<DWORD *>(t.green)[i] = min31(i) << 5;
    }
    reg(m, 8, 0, 16, 0, t.fn8);
    reg(m, 16, 0, 16, 0, cpu_has_mmx() ? t.fn16_mmx : t.fn16);
    reg(m, 24, 0, 16, 0, t.fn24);
    return m;
}
CBlendMode *screen565(CBlendMode *m, const ScreenTables &t)
{
    base_ctor(m);
    m->vftable = t.vt;
    reinterpret_cast<DWORD *>(m)[0xd] = 0;
    for (DWORD i = 0; i < 0x80; i++)
        at<DWORD *>(t.green)[i] = (i < 0x3f ? i : 0x3f) << 5;
    for (DWORD i = 0; i < 0x40; i++) {
        at<DWORD *>(t.red)[i] = min31(i) << 11;
        at<DWORD *>(t.plain)[i] = min31(i);
    }
    reg(m, 8, 0, 16, 0, t.fn8);
    reg(m, 16, 0, 16, 0, cpu_has_mmx() ? t.fn16_mmx : t.fn16);
    reg(m, 24, 0, 16, 0, t.fn24);
    return m;
}
CBlendMode *THISCALL ScreenUp555_Ctor(CBlendMode *m)
{
    const ScreenTables t = {vt(0x5eefa8), glob_addr(0x6bb9a0), glob_addr(0x6bb764), glob_addr(0x6bb664),
                             fn_addr(0x443fd0), fn_addr(0x444410), fn_addr(0x443400), fn_addr(0x443d70)};
    return screen555(m, t);
}
CBlendMode *THISCALL ScreenDown555_Ctor(CBlendMode *m)
{
    const ScreenTables t = {vt(0x5eefc4), glob_addr(0x6bb560), glob_addr(0x6bbb14), glob_addr(0x6bb864),
                             fn_addr(0x444720), fn_addr(0x444b60), fn_addr(0x443400), fn_addr(0x443d70)};
    return screen555(m, t);
}
CBlendMode *THISCALL ScreenUp565_Ctor(CBlendMode *m)
{
    const ScreenTables t = {vt(0x5eefe0), glob_addr(0x6bc224), glob_addr(0x6bbf24), glob_addr(0x6bbd24),
                             fn_addr(0x4450d0), fn_addr(0x445630), fn_addr(0x43ee70), fn_addr(0x43fe50)};
    return screen565(m, t);
}
CBlendMode *THISCALL ScreenDown565_Ctor(CBlendMode *m)
{
    const ScreenTables t = {vt(0x5eeffc), glob_addr(0x6bbc20), glob_addr(0x6bc364), glob_addr(0x6bc024),
                             fn_addr(0x4458f0), fn_addr(0x445e50), fn_addr(0x43ee70), fn_addr(0x43fe50)};
    return screen565(m, t);
}

// The levels (+0x34, clamped to 0..255) and the two shifted values the screen spans read: 5-5-5 >> 3 and >> 2
// (0x4432d0, 0x4433c0), 5-6-5 >> 2 and >> 3 (0x444f60, 0x445090)
void set_level(CBlendMode *m, int level, DWORD first, int s1, DWORD second, int s2)
{
    if (level < 0)
        level = 0;
    else if (level >= 0xff)
        level = 0xff;
    int &stored = reinterpret_cast<int *>(m)[0xd];
    stored = level;
    *at<int *>(first) = level >> s1;
    *at<int *>(second) = stored >> s2;
}
void THISCALL ScreenUp555_SetLevel(CBlendMode *m, int level) { set_level(m, level, glob_addr(0x6bb558), 3, glob_addr(0x6bbb10), 2); }
void THISCALL ScreenDown555_SetLevel(CBlendMode *m, int level) { set_level(m, level, glob_addr(0x6bb55c), 3, glob_addr(0x6bb660), 2); }
void THISCALL ScreenUp565_SetLevel(CBlendMode *m, int level) { set_level(m, level, glob_addr(0x6bc360), 2, glob_addr(0x6bbc18), 3); }
void THISCALL ScreenDown565_SetLevel(CBlendMode *m, int level) { set_level(m, level, glob_addr(0x6bbd20), 2, glob_addr(0x6bbc1c), 3); }

// 0x4479c0: the generic entries 17-63 of the span tables T0-T35 (47 each), the 8 -> 16 palette spans, 15
// blitters (raw 8/16 sources through its own transparent blitters beyond 0x460000)
CBlendMode *THISCALL TransparentMask_Ctor(CBlendMode *m)
{
    base_ctor(m);
    m->vftable = zax::vt(0x5ef06c);
    fill_table(glob_addr(0x6bcda0), fn_addr(0x447dd0));   // Span8To16_Fill (the runs)
    fill_table(glob_addr(0x6bcc9c), fn_addr(0x447da0));   // Span8To16_Copy (the literals)
    fill_table(glob_addr(0x60e7d4), fn_addr(0x448540), 0x2f);   // Span16To32_Alpha_Copy_N
    fill_table(glob_addr(0x60e5d4), fn_addr(0x4484f0), 0x2f);   // Span16To32_Copy_N
    fill_table(glob_addr(0x60e8d4), fn_addr(0x4481b0), 0x2f);   // Span16To32_Alpha_Fill_N
    fill_table(glob_addr(0x60e6d4), fn_addr(0x448090), 0x2f);   // Span16To32_Fill_N
    fill_table(glob_addr(0x60efd4), fn_addr(0x448d50), 0x2f);   // Span24_AlphaOpacity_Copy_N
    fill_table(glob_addr(0x60edd4), fn_addr(0x448c40), 0x2f);   // Span24_Opacity_Copy_N
    fill_table(glob_addr(0x60ebd4), fn_addr(0x448b20), 0x2f);   // Span24_Alpha_Copy_N
    fill_table(glob_addr(0x60e9d4), fn_addr(0x448af0), 0x2f);   // Span24_Copy_N
    fill_table(glob_addr(0x60f0d4), fn_addr(0x4489c0), 0x2f);   // Span24_AlphaOpacity_Fill_N
    fill_table(glob_addr(0x60eed4), fn_addr(0x4488a0), 0x2f);   // Span24_Opacity_Fill_N
    fill_table(glob_addr(0x60ecd4), fn_addr(0x448790), 0x2f);   // Span24_Alpha_Fill_N
    fill_table(glob_addr(0x60ead4), fn_addr(0x448760), 0x2f);   // Span24_Fill_N
    fill_table(glob_addr(0x60e3d4), fn_addr(0x449cd0), 0x2f);   // Span16_AlphaOpacity_Copy_MMX_N
    fill_table(glob_addr(0x60e1d4), fn_addr(0x449880), 0x2f);   // Span16_Opacity_Copy_MMX_N
    fill_table(glob_addr(0x60dfd4), fn_addr(0x449aa0), 0x2f);   // Span16_Alpha_Copy_MMX_N
    fill_table(glob_addr(0x60ddd4), fn_addr(0x448670), 0x2f);   // Span16_AlphaOpacity_Copy_N
    fill_table(glob_addr(0x60dbd4), fn_addr(0x4483e0), 0x2f);   // Span16_Opacity_Copy_N
    fill_table(glob_addr(0x60d9d4), fn_addr(0x448590), 0x2f);   // Span16_Alpha_Copy_N
    fill_table(glob_addr(0x60e4d4), fn_addr(0x449520), 0x2f);   // Span16_AlphaOpacity_Fill_MMX_N
    fill_table(glob_addr(0x60e2d4), fn_addr(0x4491c0), 0x2f);   // Span16_Opacity_Fill_MMX_N
    fill_table(glob_addr(0x60e0d4), fn_addr(0x448e70), 0x2f);   // Span16_Alpha_Fill_MMX_N
    fill_table(glob_addr(0x60ded4), fn_addr(0x4482f0), 0x2f);   // Span16_AlphaOpacity_Fill_N
    fill_table(glob_addr(0x60dcd4), fn_addr(0x448200), 0x2f);   // Span16_Opacity_Fill_N
    fill_table(glob_addr(0x60dad4), fn_addr(0x4480d0), 0x2f);   // Span16_Alpha_Fill_N
    fill_table(glob_addr(0x60d8d4), fn_addr(0x448050), 0x2f);   // Span16_Fill_N
    fill_table(glob_addr(0x60d7d4), fn_addr(0x4484c0), 0x2f);   // Span16_Copy_N
    fill_table(glob_addr(0x60d6d4), fn_addr(0x447fc0), 0x2f);   // Span8_Alpha2080SD_Fill_N
    fill_table(glob_addr(0x60d5d4), fn_addr(0x447f90), 0x2f);   // Span8_Alpha2080SD_Copy_N
    fill_table(glob_addr(0x60d4d4), fn_addr(0x447f60), 0x2f);   // Span8_Alpha4060SD_Fill_N
    fill_table(glob_addr(0x60d3d4), fn_addr(0x447f30), 0x2f);   // Span8_Alpha4060SD_Copy_N
    fill_table(glob_addr(0x60d2d4), fn_addr(0x447f00), 0x2f);   // Span8_Alpha4060DS_Fill_N
    fill_table(glob_addr(0x60d1d4), fn_addr(0x447ed0), 0x2f);   // Span8_Alpha4060DS_Copy_N
    fill_table(glob_addr(0x60d0d4), fn_addr(0x447ea0), 0x2f);   // Span8_Alpha2080DS_Fill_N
    fill_table(glob_addr(0x60cfd4), fn_addr(0x447e70), 0x2f);   // Span8_Alpha2080DS_Copy_N
    fill_table(glob_addr(0x60ced4), fn_addr(0x447ff0), 0x2f);   // Span8_Fill_N
    fill_table(glob_addr(0x60cdd4), fn_addr(0x447e20), 0x2f);   // Span8_Copy_N
    reg(m, 8, 0, 8, 0, fn_addr(0x464fe0));
    reg(m, 8, 0, 16, 0, fn_addr(0x4651d0));
    reg(m, 8, 0, 24, 0, fn_addr(0x4653d0));
    reg(m, 8, 0, 32, 0, fn_addr(0x465660));
    reg(m, 16, 0, 16, 0, fn_addr(0x4658d0));
    reg(m, 16, 0, 24, 0, fn_addr(0x465ae0));
    reg(m, 24, 0, 24, 0, fn_addr(0x434cf0));   // BlitCopy_24To24
    reg(m, 24, 0, 32, 0, fn_addr(0x4400b0));   // BlitCopy_24To32
    reg(m, 32, 0, 24, 0, fn_addr(0x434f50));   // BlitCopy_32To24
    reg(m, 32, 0, 32, 0, fn_addr(0x435300));   // BlitCopy_32To32
    reg(m, 8, 2, 8, 0, fn_addr(0x4402d0));   // BlitCopy_8RleTo8
    reg(m, 8, 2, 16, 0, fn_addr(0x440330));   // BlitCopy_8RleTo16
    reg(m, 16, 4, 16, 0, cpu_has_mmx() ? fn_addr(0x4388f0) : fn_addr(0x438c40));   // BlitCopy_16RleTo16_MMX / _NoMMX
    reg(m, 24, 4, 24, 0, fn_addr(0x435cb0));   // BlitTransparentMask_24RleTo24
    reg(m, 24, 4, 32, 0, fn_addr(0x465d10));
    return m;
}

// ---- The hue remap (CBlendHueRemap, 0x48: the mode, +0x34/+0x38/+0x3c the hue, new hue and range as given,
// +0x44 a 64K-entry 16-bit table) ---------------------------------------------------------------------------------

// 0x43bfe0: the palette allocated (kind 2) as the identity, the 128 KB table, 2 blitters
CBlendMode *THISCALL HueRemap_Ctor(CBlendMode *m)
{
    base_ctor(m);
    m->vftable = zax::vt(0x5eeeb4);
    palette_alloc(&m->palette, 2);
    reinterpret_cast<DWORD *>(m)[0x11] = (DWORD)(uintptr_t)heap_alloc(0x20000);
    for (DWORD i = 0; i < 0x100; i++)
        m->palette.data[i] = (BYTE)i;
    reg(m, 8, 2, 8, 0, fn_addr(0x43c6b0));   // BlitHueRemap_8RleTo8
    reg(m, 16, 4, 16, 0, cpu_has_mmx() ? fn_addr(0x43ced0) : fn_addr(0x43d2d0));   // BlitHueRemap_16RleTo16_MMX / _NoMMX
    return m;
}
// 0x43c0b0 / 0x43c070: the table freed, the base destructor (operator delete with bit 0)
void THISCALL HueRemap_Dtor(CBlendMode *m)
{
    DWORD *d = reinterpret_cast<DWORD *>(m);
    m->vftable = zax::vt(0x5eeeb4);
    if (d[0x11]) {
        heap_free(reinterpret_cast<void *>(static_cast<uintptr_t>(d[0x11])));
        d[0x11] = 0;
    }
    base_dtor(m);
}
void *THISCALL HueRemap_DeletingDtor(CBlendMode *m, DWORD flags)
{
    HueRemap_Dtor(m);
    if (flags & 1)
        heap_free(m);
    return m;
}

// 0x43c390 (fastcall p, q; h): one channel for the hue position h (wrapped once into 0..0x600): rising from p to
// q below 0x100, q up to 0x300, falling back to p up to 0x400, else p (divisions by 256 rounded towards 0)
int __fastcall Hue_Lerp(int p, int q, int h)
{
    if (h > 0x600)
        h -= 0x600;
    else if (h < 0)
        h += 0x600;
    if (h < 0x100)
        return (q - p) * h / 256 + p;
    if (h < 0x300)
        return q;
    if (h < 0x400)
        return (q - p) * (0x400 - h) / 256 + p;
    return p;
}

namespace {

// The common part of the two builders: the parameters stored, the palette remapped (0x5280e0 with the game
// palette; `kind` 1 or 0), then every 5-6-5 pixel but black whose hue (0..0x600, HSL with 0..255 channels)
// lies within the range around the hue replaced by the same lightness and saturation at the new hue
void hue_build(CBlendMode *m, DWORD hue_bits, DWORD new_hue_bits, DWORD range_bits, DWORD kind, bool identity)
{
    DWORD *d = reinterpret_cast<DWORD *>(m);
    d[0xe] = new_hue_bits;
    d[0xd] = hue_bits;
    d[0xf] = range_bits;
    Palette *gp = zax::fn<Palette *(__cdecl *)()>(0x433a80)();
    zax::fn<void(THISCALL *)(Palette *, Palette *, void *, DWORD)>(0x5280e0)(&m->palette, gp, d + 0xd, kind);
    const long double k = cst(glob_addr(0x5eeed0));
    const int hc = x87_fistp(fld(fstp(fld(raw_float(hue_bits)) * k)));
    const int hn = x87_fistp(fld(fstp(fld(raw_float(new_hue_bits)) * k)));
    const int w = x87_fistp(fld(fstp(fld(raw_float(range_bits)) * k)));
    WORD *table = reinterpret_cast<WORD *>(static_cast<uintptr_t>(d[0x11]));
    for (DWORD i = 0; i < 0x10000; i++) {
        if (identity)
            table[i] = (WORD)i;
        const int r = (int)((i >> 8) & 0xf8), g = (int)((i >> 3) & 0xfc), b = (int)((i & 0x1f) << 3);
        if (!r && !g && !b)
            continue;
        int mx = r > g ? r : g;
        if (!(mx > b))
            mx = b;
        int mn = r < g ? r : g;
        if (!(mn < b))
            mn = b;
        const int sum = mx + mn, l = sum / 2;
        int s, h;
        if (mx == mn) {
            s = 0;
            h = 0;
        } else {
            const int dd = mx - mn;
            s = l <= 0x80 ? (dd << 8) / sum : (dd << 8) / (0x200 - mn - mx);
            if (r == mx)
                h = ((g - b) << 8) / dd;
            else if (g == mx)
                h = ((b - r) << 8) / dd + 0x200;
            else
                h = ((r - g) << 8) / dd + 0x400;
            if (h < 0)
                h += 0x600;
        }
        if (h < hc - w || h > w + hc)
            continue;
        const int q = l <= 0x80 ? (s + 0x100) * l / 256 : s - s * l / 256 + l;
        const int p = (l << 9) / 256 - q;
        int nr, ng, nb;
        if (!s) {
            nr = ng = nb = l;
        } else {
            nr = zax::fn<int(__fastcall *)(int, int, int)>(0x43c390)(p, q, hn + 0x200);
            ng = zax::fn<int(__fastcall *)(int, int, int)>(0x43c390)(p, q, hn);
            nb = zax::fn<int(__fastcall *)(int, int, int)>(0x43c390)(p, q, hn - 0x200);
        }
        const DWORD rr = (DWORD)(nr >= 0xff ? 0xff : nr) & 0xff, gg = (DWORD)(ng >= 0xff ? 0xff : ng) & 0xff,
                    bb = (DWORD)(nb >= 0xff ? 0xff : nb) & 0xff;
        table[i] = (WORD)zax::fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x433a10)(rr, gg, bb);
    }
}

}  // namespace

// 0x43c0e0 (hue, new hue, range; floats): the palette remapped (kind 1), the table from the identity
void THISCALL HueRemap_Build8(CBlendMode *m, DWORD hue, DWORD new_hue, DWORD range) { hue_build(m, hue, new_hue, range, 1, true); }
// 0x43c410: the palette remapped (kind 0), only the pixels in the range written
void THISCALL HueRemap_Build16(CBlendMode *m, DWORD hue, DWORD new_hue, DWORD range) { hue_build(m, hue, new_hue, range, 0, false); }

// ---- Overlap Blit ----------------------------------------------------------------------------------------------

namespace {

// The ramp's 16-bit map (256 words, indexed by the low byte of a pixel) and 8-bit map (256 bytes, by palette index)
WORD *overlap16() { return glob<WORD *>(0x6bc64c); }
BYTE *overlap8() { return glob<BYTE *>(0x6bcb4c); }
BYTE color_idx(FieldColor *c) { return (c->flags & 1) ? c->index : color_index(c); }

}  // namespace

// 0x446a90: once (0x6bcc60): both maps cleared (0xc8 / 0xff), entry 0 the base colour 0x6bcc50, then the ramp
// base -> (1, 250, 8) -> (200, 250, 24) -> (245, 162, 32) -> (250, 3, 40) -> white, white and (0, 0, 200)
// mapping to themselves (each colour's entry: the next colour; r, g, b of FieldColor_CtorRGBA)
void __cdecl Overlap_BuildColor()
{
    if (*glob<BYTE *>(0x6bcc60))
        return;
    WORD *m16 = overlap16();
    BYTE *m8 = overlap8();
    for (DWORD i = 0; i < 0x100; i++)
        m16[i] = 0xc8;
    for (DWORD i = 0; i < 0x100; i++)
        m8[i] = 0xff;
    FieldColor *base = glob<FieldColor *>(0x6bcc50);
    m8[0] = color_idx(base);
    m16[0] = base->pixel16;
    FieldColor c[6];
    static_assert(sizeof c == 0x60);
    const DWORD RGB[5][3] = {{1, 0xfa, 8}, {0xc8, 0xfa, 0x18}, {0xf5, 0xa2, 0x20}, {0xfa, 3, 0x28}, {0xff, 0xff, 0xff}};
    FieldColor *prev = base;
    for (int k = 0; k < 5; k++) {
        color_ctor_rgba(&c[k], RGB[k][0], RGB[k][1], RGB[k][2], 0xff);
        const BYTE next = color_idx(&c[k]);
        const BYTE from = color_idx(prev);
        m16[*reinterpret_cast<DWORD *>(&prev->pixel16) & 0xff] = c[k].pixel16;
        m8[from] = next;
        prev = &c[k];
    }
    {
        const BYTE a = color_idx(&c[4]);
        const BYTE b = color_idx(&c[4]);
        m8[b] = a;
        m16[*reinterpret_cast<DWORD *>(&c[4].pixel16) & 0xff] = c[4].pixel16;
    }
    color_ctor_rgba(&c[5], 0, 0, 0xc8, 0xff);
    const BYTE a = color_idx(&c[5]);
    const BYTE b = color_idx(&c[5]);
    *glob<BYTE *>(0x6bcc60) = 1;
    m8[b] = a;
    m16[*reinterpret_cast<DWORD *>(&c[5].pixel16) & 0xff] = c[5].pixel16;
}

// 0x446e10 (slot 2; src, dst): the ramp built, then CBlendMode::FindBlitter
DWORD THISCALL Overlap_FindBlitter(CBlendMode *m, void *src, void *dst)
{
    zax::fn<void(__cdecl *)()>(0x446a90)();
    return zax::fn<DWORD(THISCALL *)(CBlendMode *, void *, void *)>(0x432e80)(m, src, dst);
}

// ---- Pixels and palettes ----------------------------------------------------------------------------------------

// 0x4339a0 (fastcall r, g; b): the 5-5-5 pixel (tables 0x633b94, 0x6753b8, 0x630378 when built, else rounded)
DWORD __fastcall render::Pixel15_FromRGB(DWORD r, DWORD g, DWORD b)
{
    if (*glob<DWORD *>(PIXEL16_BUILT))
        return glob<DWORD *>(PIXEL15_RED)[r] + glob<DWORD *>(PIXEL15_GREEN)[g] + glob<DWORD *>(PIXEL16_BLUE)[b];
    DWORD r5 = (r + 4) >> 3, g5 = (g + 4) >> 3, b5 = (b + 4) >> 3;
    if (r5 >= 0x20)
        r5 = 0x1f;
    if (g5 >= 0x20)
        g5 = 0x1f;
    if (b5 >= 0x20)
        b5 = 0x1f;
    return (((r5 << 5) + g5) << 5) + b5;
}

namespace {

// 0x433a80
Palette *__cdecl GamePalette_Get() { return glob<Palette *>(GAME_PALETTE); }

DWORD pixel565(DWORD r, DWORD g, DWORD b)
{
    if (*glob<DWORD *>(PIXEL16_BUILT))
        return glob<DWORD *>(PIXEL16_RED)[r] + glob<DWORD *>(PIXEL16_GREEN)[g] + glob<DWORD *>(PIXEL16_BLUE)[b];
    DWORD r5 = (r + 4) >> 3, g6 = (g + 2) >> 2, b5 = (b + 4) >> 3;
    if (r5 >= 0x20)
        r5 = 0x1f;
    if (g6 >= 0x40)
        g6 = 0x3f;
    if (b5 >= 0x20)
        b5 = 0x1f;
    return (((r5 << 6) + g6) << 5) + b5;
}

// The 16-bit forms of entries 1..255 of a palette (b, g, r per entry; black taken as blue 8), entry 0 0:
// 0x62fb30 5-5-5, 0x6343b0 5-6-5
void palette16(const BYTE *pal, bool inline_pixels)
{
    DWORD *p15 = at<DWORD *>(glob_addr(PALETTE15) - 4), *p16 = at<DWORD *>(glob_addr(PALETTE16) - 4);
    p15[0] = 0;
    p16[0] = 0;
    for (DWORD i = 1; i < 0x100; i++) {
        const DWORD r = pal[i * 3 + 2], g = pal[i * 3 + 1];
        DWORD b = pal[i * 3];
        if (r == g && g == b && !b)
            b = 8;
        if (inline_pixels) {
            p15[i] = render::Pixel15_FromRGB(r, g, b) & 0xffff;
            p16[i] = pixel565(r, g, b) & 0xffff;
        } else {
            p15[i] = zax::fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x4339a0)(r, g, b) & 0xffff;
            p16[i] = zax::fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x433a10)(r, g, b) & 0xffff;
        }
    }
}

// 0x433a90 (ecx the palette): its 16-bit forms
void THISCALL Pixel16_BuildTables(const Palette *pal) { palette16(pal->data, true); }

// 0x4337e0 (fastcall palette): taken as the game palette; the product table 0x6767dc (256 x 256: r * c), the fade
// tables 0x6347b0 (256 x 256: c * b / 255), the game palette's 16-bit forms, the opacity entries 0x6757b8
// (5 - (6i >> 8)) unless entry 128 is set
void __fastcall BuildInvColorTable(const Palette *pal, DWORD, DWORD)
{
    palette_assign(glob<Palette *>(GAME_PALETTE), pal);
    DWORD *prod = glob<DWORD *>(0x6767dc);
    for (DWORD r = 0; r < 0x100; r++)
        for (DWORD c = 0; c < 0x100; c++)
            prod[r * 256 + c] = c * r;
    DWORD *fade = glob<DWORD *>(FADE_TABLES);
    for (DWORD b = 0; b < 0x100; b++)
        for (DWORD c = 0; c < 0x100; c++)
            fade[b * 256 + c] = c * b / 255;
    palette16(glob<Palette *>(GAME_PALETTE)->data, false);
    DWORD *op = glob<DWORD *>(OPACITY_ENTRIES);
    if (!op[0x80])
        for (DWORD i = 0; i < 0x100; i++)
            op[i] = 5 - ((i * 6) >> 8);
}

}  // namespace

ZAX_HOOKS(blend_core)
{
    const Hook list[] = {
        {0x432d60, ptr(&BlendMode_Ctor), "CBlendMode_Ctor", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x04, 0xe8, 0x65}},
        {0x432da0, ptr(&BlendMode_Dtor), "CBlendMode_Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xe4, 0xed, 0x5e}},
        {0x432de0, ptr(&BlendMode_RegisterBlitter), "BlendMode_RegisterBlitter", 6, {0x83, 0xec, 0x14, 0x53, 0x8b, 0xd9, 0x55, 0x56}},
        {0x432e80, ptr(&BlendMode_FindBlitter), "CBlendMode::FindBlitter", 5, {0x83, 0xec, 0x20, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x433be0, ptr(&BlendMode_SetOpacity), "CBlendMode_SetOpacity", 7, {0x55, 0x8b, 0xec, 0x51, 0xd9, 0x45, 0x08, 0xd8}},
        {0x432f90, ptr(&BlendMode_WriteOpacity), "CBlendMode::WriteOpacity", 8, {0x8b, 0xc1, 0x6a, 0x04, 0x8b, 0x4c, 0x24, 0x08}},
        {0x432fb0, ptr(&BlendMode_ReadOpacity), "CBlendMode::ReadOpacity", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x56, 0x8b}},
        {0x441b50, ptr(&BlendMode_DeletingDtor), "CBlendMode_DeletingDtor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x48, 0x12, 0xff, 0xff}, 4},
        {0x433d90, ptr(&AmplifyLight_DeletingDtor), "CBlendAmplifyLight_DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x34, 0xee, 0x5e}},
        {0x438640, ptr(&BlackBias_DeletingDtor), "CBlendBlackBias_DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x50, 0xee, 0x5e}},
        {0x433df0, ptr(&AmplifyLight_Ctor), "CBlendAmplifyLight_Ctor", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xd9, 0xe8, 0x65}},
        {0x4386f0, ptr(&BlackBias_Ctor), "CBlendBlackBias_Ctor", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x67, 0xa6, 0xff}, 5},
        {0x43eac0, ptr(&Copy_Ctor), "CBlendCopy_Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x98, 0x42, 0xff, 0xff}, 4},
        {0x443220, ptr(&ScreenUp555_Ctor), "CBlendScreenUp555_Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x38, 0xfb, 0xfe, 0xff}, 4},
        {0x443310, ptr(&ScreenDown555_Ctor), "CBlendScreenDown555_Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x48, 0xfa, 0xfe, 0xff}, 4},
        {0x444ea0, ptr(&ScreenUp565_Ctor), "CBlendScreenUp565_Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xb8, 0xde, 0xfe, 0xff}, 4},
        {0x444fd0, ptr(&ScreenDown565_Ctor), "CBlendScreenDown565_Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x88, 0xdd, 0xfe, 0xff}, 4},
        {0x4432d0, ptr(&ScreenUp555_SetLevel), "CBlendScreenUp555_SetLevel", 6, {0x8b, 0x44, 0x24, 0x04, 0x85, 0xc0, 0x7d, 0x04}},
        {0x4433c0, ptr(&ScreenDown555_SetLevel), "CBlendScreenDown555_SetLevel", 6, {0x8b, 0x44, 0x24, 0x04, 0x85, 0xc0, 0x7d, 0x04}},
        {0x444f60, ptr(&ScreenUp565_SetLevel), "CBlendScreenUp565_SetLevel", 6, {0x8b, 0x44, 0x24, 0x04, 0x85, 0xc0, 0x7d, 0x04}},
        {0x445090, ptr(&ScreenDown565_SetLevel), "CBlendScreenDown565_SetLevel", 6, {0x8b, 0x44, 0x24, 0x04, 0x85, 0xc0, 0x7d, 0x04}},
        {0x4479c0, ptr(&TransparentMask_Ctor), "CBlendTransparentMask_Ctor", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x97, 0xb3, 0xfe}, 5},
        {0x43bfe0, ptr(&HueRemap_Ctor), "CBlendHueRemap_Ctor", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x77, 0x6d, 0xff}, 5},
        {0x43c0b0, ptr(&HueRemap_Dtor), "CBlendHueRemap_Dtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x44, 0xc7, 0x06}},
        {0x43c070, ptr(&HueRemap_DeletingDtor), "CBlendHueRemap_DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x44, 0xc7, 0x06}},
        {0x43c390, (void *)&Hue_Lerp, "Hue_Lerp", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x81, 0xfe, 0x00}},
        {0x43c0e0, ptr(&HueRemap_Build8), "CBlendHueRemap_Build8", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x20, 0x53, 0x56}},
        {0x43c410, ptr(&HueRemap_Build16), "CBlendHueRemap_Build16", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x20, 0x53, 0x56}},
        {0x446a90, (void *)&Overlap_BuildColor, "CBlendOverlap_BuildColor", 5, {0xa0, 0x60, 0xcc, 0x6b, 0x00, 0x83, 0xec, 0x64}},
        {0x446e10, ptr(&Overlap_FindBlitter), "CBlendOverlap::FindBlitter", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x78, 0xfc, 0xff, 0xff}, 4},
        {0x4339a0, (void *)&render::Pixel15_FromRGB, "Pixel15_FromRGB", 5, {0xa1, 0x78, 0x05, 0x63, 0x00, 0x85, 0xc0, 0x74}},
        {0x433a80, (void *)&GamePalette_Get, "GamePalette_Get", 5, {0xb8, 0x98, 0x3f, 0x63, 0x00, 0xc3, 0x90, 0x90}},
        {0x433a90, ptr(&Pixel16_BuildTables), "Pixel16_BuildTables", 7, {0x83, 0xec, 0x08, 0x89, 0x4c, 0x24, 0x04, 0x53}},
        {0x4337e0, (void *)&BuildInvColorTable, "BuildInvColorTable", 6, {0x81, 0xec, 0x04, 0x02, 0x00, 0x00, 0x53, 0x55}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
