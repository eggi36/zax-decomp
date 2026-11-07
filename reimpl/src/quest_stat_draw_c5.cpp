// CQuestStat::Draw 0x5533e0 (docs/DECOMPILATION.md 10, "Batch C5"): a quest statistic's row of the mission over
// window. Every call goes through its exe address.
//
// [confirmed: code; VM] (a, b, frame, x, y): nothing (eax 0) when slot 29 (a, b) says so. The columns come from the
// quest game type (the cache 0x71232c, filled inline: "QuestMode" of the game types 0x6c02e8 when it is a
// CQuestGameType, else null; 0x5525e0 gives column k with +0x10 its format, +0x1c x, +0x20 width, +0x24 a value
// passed on). First row: column 0 the format with the statistic's name (+0x14) in the font +0x1c, columns 1 and 2
// the formats with the texts of slots 25 and 27 (a) in the font +0x20, column 3 (while slot 35 (a, b)) "%d" of slot
// 31 (a, b) (__ftol). Unless slot 30 (a, b): a second row below the first, the game type's format +0x38 with the
// name, and slots 26 and 28 (a, b). Every text drawn word-wrapped (CFont::DrawWrapped 0x4a6790) at (x + column x,
// y); eax: the height (the first row's highest + 2, then the second row's).
#include "ui_screens.h"
#include "x87.h"

namespace zax::queststat {

using namespace ui;

constexpr DWORD FONT_LIST = 0x6be1e0, GAME_TYPES = 0x6c02e8, CACHE = 0x71232c;
inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

inline const char *desc_name(void *field) { return fn<const char *(THISCALL *)(void *)>(0x5007a0)(field); }
// The quest game type, cached
inline BYTE *quest_type()
{
    if (!gd(glob_addr(CACHE))) {
        const DWORD i = vc(glob<void *>(GAME_TYPES), 3, glob<const char *>(0x6179f0) /* "QuestMode" */, -1);
        BYTE *o = fn<BYTE *(THISCALL *)(DWORD, DWORD)>(0x48d8f0)(glob_addr(GAME_TYPES), i);
        BYTE *q = nullptr;
        if (o && (BYTE)fn<DWORD(THISCALL *)(void *, void *)>(0x416790)(o, fn<void *(__cdecl *)()>(0x551c20)()))
            q = o;
        gd(glob_addr(CACHE)) = ptrv(q);
    }
    return bp(gd(glob_addr(CACHE)));
}
inline BYTE *column(DWORD k) { return fn<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x5525e0)(bp(gd(glob_addr(CACHE))), k); }
// CFont::DrawWrapped (frame, x, y, text, width, 2, the column's +0x24): the height
inline DWORD wrapped(DWORD font, void *frame, int x, int y, const char *s, BYTE *col)
{
    return fn<DWORD(THISCALL *)(DWORD, void *, int, int, const char *, DWORD, DWORD, DWORD)>(0x4a6790)(font, frame, x + (int)m(col, 0x1c), y, s,
                                                                                                    m(col, 0x20), 2, m(col, 0x24));
}

}  // namespace zax::queststat

namespace zax {

using namespace queststat;

namespace {

// 0x5533e0 (a, b, frame, x, y)
DWORD THISCALL QuestStat_Draw(BYTE *stat, DWORD a, DWORD b, void *frame, int x, int y)
{
    if ((BYTE)vc(stat, 29, a, b))
        return 0;
    BYTE line[4], value[4];
    zs_ctor(line);
    quest_type();
    BYTE *col = fn<BYTE *(THISCALL *)(DWORD, DWORD)>(0x5525e0)(gd(glob_addr(CACHE)), 0);
    {
        const char *name = desc_name(stat + 0x14);
        zs_format(line, desc_name(col + 0x10), name);
    }
    DWORD font = record_loaded(glob_addr(FONT_LIST), m(stat, 0x1c));
    DWORD top = wrapped(font, frame, x, y, c_str(line), col) + 2;
    DWORD height = top;
    zs_ctor(value);
    // columns 1 and 2 (slots 25 and 27 of a), column 3 (slot 31 while slot 35)
    const int slots[2] = {25, 27};
    for (int k = 0; k < 2; k++) {
        quest_type();
        col = column(1 + (DWORD)k);
        vc(stat, slots[k], a, value);
        const char *v = c_str(value);
        zs_format(line, desc_name(col + 0x10), v);
        font = record_loaded(glob_addr(FONT_LIST), m(stat, 0x20));
        const DWORD h = wrapped(font, frame, x, y, c_str(line), col) + 2;
        if (!(top > h)) {
            top = h;
            height = top;
        }
    }
    if ((BYTE)vc(stat, 35, a, b)) {
        quest_type();
        col = column(3);
        const int n = x87_ftol(vc<long double>(stat, 31, a, b));
        zs_format(value, glob<const char *>(0x60a5a0) /* "%d" */, n);
        const char *v = c_str(value);
        zs_format(line, desc_name(col + 0x10), v);
        font = record_loaded(glob_addr(FONT_LIST), m(stat, 0x20));
        const DWORD h = wrapped(font, frame, x, y, c_str(line), col) + 2;
        if (!(top > h))
            height = h;
    }
    if (!(BYTE)vc(stat, 30, a, b)) {
        const DWORD first = height;
        quest_type();
        BYTE *col0 = column(0);
        BYTE *q = quest_type();
        {
            const char *fmt = desc_name(q + 0x38);
            const char *name = desc_name(stat + 0x14);
            zs_format(line, fmt, name);
        }
        font = record_loaded(glob_addr(FONT_LIST), m(stat, 0x1c));
        const int y2 = (int)height + y;
        height = height + wrapped(font, frame, x, y2, c_str(line), col0) + 2;
        const int slots2[2] = {26, 28};
        for (int k = 0; k < 2; k++) {
            quest_type();
            col = column(1 + (DWORD)k);
            vc(stat, slots2[k], a, b, value);
            const char *v = c_str(value);
            zs_format(line, desc_name(col + 0x10), v);
            font = record_loaded(glob_addr(FONT_LIST), m(stat, 0x20));
            const DWORD h = wrapped(font, frame, x, y2, c_str(line), col) + first + 2;
            if (!(height > h))
                height = h;
        }
    }
    zs_dtor(value);
    zs_dtor(line);
    return height;
}

}  // namespace

ZAX_HOOKS(quest_stat_draw_c5)
{
    const Hook list[] = {
#include "quest_stat_draw_c5_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
