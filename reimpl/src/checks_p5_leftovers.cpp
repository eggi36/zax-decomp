// Checks of the phase 5 leftovers (p5_leftovers.cpp) against the original with the toolkit's harness
// (checks_ui_common.h): the mission over window's drawing, the properties file name callback and the model preview's
// slots 2 and 3, every exe function outside the module a recording stand-in (the arguments beyond the fourth logged
// by the effect), the windows, the frame, the game type, the entity and its sequence as fakes; the definition lists
// the functions read (records, counts, the list objects' vtables) are case values in the block, the lists' own slots
// (name 1, folder 5, load 37) a recording vtable of this check; ZStrings, Path_Join, PathName, _stricmp, __ftol and
// the class info getters real.
#include "checks_ui_common.h"
#include "checks_ui_screens.h"

extern "C" DWORD uic_edx, uic_fvalue;

namespace zax {

int hooks_p5_leftovers(Hook *out);

namespace {

using namespace uic;
using scr::object_args;

constinit ModuleHooks g_hooks{"p5_leftovers", &hooks_p5_leftovers};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    // the mission over window
    {0x4b3280, CV_THIS, 4, K_VALUE, "Frame_BlitAt"},
    {0x4a6470, CV_THIS, 6, K_VALUE, "CFont::DrawText", "vvvs"},
    {0x4a6790, CV_THIS, 7, K_SMALL, "CFont::DrawWrapped", "vvvs"},
    {0x4a6b90, CV_THIS, 5, K_VALUE, "CFont::DrawInt"},
    {0x4a6140, CV_THIS, 0, K_SMALL, "CFont::LineHeight"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
    {0x59ff80, CV_THIS, 0, K_FIXED0, "Game_AppGameType"},
    {0x416790, CV_THIS, 1, 'p', "Object_IsKindOf"},
    {0x5525f0, CV_THIS, 1, K_BOOL, "CQuestGameType::AnyShown"},
    {0x552690, CV_THIS, 1, K_BOOL, "CQuestGameType::AnyScored"},
    {0x5525e0, CV_THIS, 1, K_FIXED1, "CQuestGameType::Column"},
    {0x5007a0, CV_THIS, 0, K_TEXT, "FieldDesc_Name"},
    {0x5527f0, CV_THIS, 1, K_FLOAT, "CQuestGameType::TotalPoints"},
    {0x552730, CV_THIS, 4, K_VALUE, "CQuestGameType::StatRows"},
    {0x4d2650, CV_CDECL, 0, K_VALUE, "Platform_Ticks24"},
    {0x524f50, CV_THIS, 1, K_VALUE, "COffsetClippedProxyFrame_SetClip", "r"},
    {0x566a80, CV_CDECL, 0, K_FIXED2, "Scoreboard_Get"},
    {0x482de0, CV_THIS, 0, K_SMALL, "DynamicProperties_Count"},
    {0x566e50, CV_THIS, 1, K_VALUE, "Scoreboard::SortedRowAt"},
    {0x5664b0, CV_THIS, 0, K_FIXED3, "Identity_Cast"},
    {0x483750, CV_THIS, 0, K_SMALL, "ExtraList_Count"},
    {0x40b620, CV_THIS, 1, K_VALUE, "CJWindow::DrawChildren"},
    // the properties file name
    {0x4f3ac0, CV_THIS, 1, K_VALUE, "World::CurrentLevelPath"},
    {0x48d8f0, CV_THIS, 1, K_VALUE, "DefinitionList_Object"},
    {0x4f7fc0, CV_FAST, 0, K_FIXED4, "ModelList_FindSequence", "vx"},
    {0x517200, CV_THIS, 1, K_TEXT, "CModel::SequenceName"},
    // the model preview
    {0x48b290, CV_THIS, 0, K_VALUE, "Entity_Model"},
    {0x48aed0, CV_THIS, 0, K_SMALL, "Entity_Sequence"},
    {0x5173f0, CV_THIS, 1, K_FIXED5, "CModel::Sequence"},
    {0x517a80, CV_THIS, 1, K_VALUE, "WindowModelPreview::SetModel"},
    {0x48feb0, CV_THIS, 1, K_VALUE, "EntityAnimated_SetFrame"},
    {0x48aee0, CV_THIS, 0, K_FIXED6, "Entity_SequenceName"},
    {0x517e70, CV_THIS, 0, K_VALUE, "Entity_SyncChildren"},
    {0x55fe90, CV_THIS, 1, K_FLOAT, "CRotationSequence_RotationToAngle"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

char g_object_args[MAXSLOT + 1];

// The class infos the callback tests, in its order (the case's object is "of" one of them or of none)
constexpr DWORD KINDS[] = {0x515870, 0x4e52f0, 0x4d1d50, 0x41deb0, 0x55eae0, 0x560b20, 0x4d55c0, 0x4d70b0, 0x4a5910, 0x56ab50};
DWORD g_kind;   // the class info Object_IsKindOf says yes to (0: a random answer)

// The definition lists' recording vtable (slot 1 the record's name, 5 the folder, 37 load: (folder, file, record))
DWORD g_list_vt[40];
DWORD THISCALL list_name(DWORD self, DWORD i)
{
    log_event(0x1501, self, i);
    return ptrv(TEXTS[g_case.values.below(8)]);
}
DWORD THISCALL list_folder(DWORD self)
{
    log_event(0x1505, self, 0);
    return ptrv(TEXTS[g_case.values.below(8)]);
}
DWORD THISCALL list_load(DWORD self, const char *folder, const char *file, DWORD rec)
{
    DWORD h = 2166136261u;
    for (const char *s = folder; s && *s; s++)
        h = (h ^ (BYTE)*s) * 16777619u;
    for (const char *s = file; s && *s; s++)
        h = (h ^ (BYTE)*s) * 16777619u;
    log_event(0x1537, self ^ h, rec);
    return g_case.values.chance(4) ? 0 : ptrv(win(12 + (int)g_case.values.below(4)));
}
void list_vtable()
{
    for (DWORD &e : g_list_vt)
        e = 0;
    g_list_vt[1] = ptrv(reinterpret_cast<void *>(&list_name));
    g_list_vt[5] = ptrv(reinterpret_cast<void *>(&list_folder));
    g_list_vt[37] = ptrv(reinterpret_cast<void *>(&list_load));
}

// A list's records in the block (0x80 each: the name at +0, the object +0x78, loading +0x7f), `obj` in one of them
// now and then; the records pointer, the count and (with `vt`) the list's vtable as case values
void fake_list(Gen &g, DWORD list, DWORD obj, bool vt)
{
    const DWORD n = g.chance(6) ? 0 : 1 + g.below(5);
    BYTE *recs = n ? xalloc(n * 0x80) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        BYTE *r = recs + i * 0x80;
        const char *name = TEXTS[g.below(8)];
        for (DWORD k = 0; name[k] && k < 0x70; k++)
            r[k] = (BYTE)name[k];
        at_(r, 0x78) = g.chance(3) ? 0 : ptrv(win(12 + (int)g.below(4)));
        r[0x7f] = g.chance(6) ? 1 : 0;
    }
    if (n && obj && g.chance(2))
        at_(recs, g.below(n) * 0x80 + 0x78) = obj;
    case_global(list + 0x30, ptrv(recs));   // (no records only with the count 0: the searches read them)
    case_global(list + 0x34, n);
    if (vt)
        case_global(list, ptrv(g_list_vt));
}

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    Gen &v = g_case.values;
    if (cls == -1) {
        switch ((DWORD)slot) {
        case 0x4a6470: log_event(0x7a64, args[4], args[5]); break;
        case 0x4a6790: log_event(0x7a67, args[4], args[5] ^ (args[6] << 8)); break;
        case 0x4a6b90: log_event(0x7a6b, args[4], 0); break;
        case 0x416790:   // yes for the case's class info (a random answer when the case has none)
            args[0] = g_kind ? (args[0] == g_kind ? 1u : 0u) : (DWORD)(v.next() & 1);
            break;
        case 0x4f7fc0:   // the sequence's index into the second argument
            if (uic_edx && readable(at<void *>(uic_edx), 4))
                *at<DWORD *>(uic_edx) = v.chance(4) ? 0xffffffff : v.below(6);
            break;
        case 0x55e090: {   // the record loaded: an object (or none) in the list's record
            const DWORD recs = *at<DWORD *>(g_effect_ecx + 0x30);
            if (recs && args[0] < *at<DWORD *>(g_effect_ecx + 0x34))
                *at<DWORD *>(recs + (args[0] << 7) + 0x78) = v.chance(4) ? 0 : ptrv(win(12 + (int)v.below(4)));
            break;
        }
        default: break;
        }
        return;
    }
    if (cls == C_CFRAME && slot == 51)   // the line's colour
        log_event(0x5151, args[4], 0);
    if (cls == C_CFRAME && slot == 36 && args[0]) {   // the frame's clip rectangle
        int *r = at<int *>(args[0]);
        r[0] = v.range(-50, 200), r[1] = v.range(-50, 200), r[2] = r[0] + v.range(0, 600), r[3] = r[1] + v.range(0, 400);
    }
    if (cls == C_OBJECT && slot == 37 && args[0] && g_case.object_args[37] == '3') {   // the entity's rectangle
        int *r = at<int *>(args[0]);
        if (v.chance(4))
            r[0] = r[1] = r[2] = r[3] = 0;
        else
            r[0] = v.range(-200, 100), r[1] = v.range(-200, 100), r[2] = r[0] + v.range(-5, 300), r[3] = r[1] + v.range(-5, 300);
    }
    // the entity's angle (slot 46) never NaN: the original turns it up by 2 pi until it is not below 0
    if (cls == C_OBJECT && slot == 46 && (answer & 0x7f800000) == 0x7f800000) {
        const float f = (float)v.range(-20, 20) / 3.0f;
        memcpy(&uic_fvalue, &f, 4);
    }
}

void base(Gen &g)
{
    g_case.nwin = NWIN;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    g_case.effect = &effect;
    g_kind = 0;
    list_vtable();
}

BYTE *object(int i) { return make_window(i, C_OBJECT); }

// ---- The mission over window --------------------------------------------------------------------------------------

void s_draw(Gen &g)
{
    object_args(g_object_args, "37:0,39:0,45:0");
    BYTE *w = xalloc(0x1b4);
    for (DWORD o = 0x138; o < 0x1b4; o += 4)
        at_(w, o) = (DWORD)g.range(-50, 300);
    make_text(w + 0x134, TEXTS[g.below(8)]);
    make_text(w + 0x190, TEXTS[g.below(8)]);
    at_(w, 0x138) = g.chance(4) ? 0 : g.next();
    // the buttons laid out in a row (null-terminated), their image
    const int nb = (int)g.below(4);
    for (int k = 0; k < 4; k++)
        at_(w, 0x154 + 4 * k) = k < nb ? ptrv(win(2 + k)) : 0;
    at_(w, 0x164) = ptrv(xalloc(0x100));
    BYTE *opt = at<BYTE *>(at_(w, 0x164));
    for (DWORD o = 0; o < 0x100; o += 4)
        at_(opt, o) = (DWORD)g.range(-20, 300);
    opt[0x74] = (BYTE)(g.chance(2) ? 0 : 1);
    at_(opt, 0x78) = g.below(6);
    at_(w, 0x18c) = ptrv(win(10));
    at_(w, 0x1a4) = ptrv(object(7));
    at_(w, 0x1ac) = g.below(4);
    g_self = w;
    g_args[0] = ptrv(make_window(14, C_CFRAME));
    // the game type (quest columns at +0x40 the speed; the multiplayer layout +0x98 … +0xc4), a column, the
    // scoreboard, a player's name
    BYTE *gt = object(8);
    for (DWORD o = 0x98; o <= 0xc4; o += 4)
        at_(gt, o) = (DWORD)g.range(-10, 200);
    at_(gt, 0x98) = g.below(6), at_(gt, 0x9c) = g.below(6), at_(gt, 0xa0) = g.below(6);
    const float speeds[] = {0.0f, 0.02f, -0.035f, 1.5f, -3.0f};
    memcpy(gt + 0x40, &speeds[g.below(5)], 4);
    g_case.fixed[0] = g.chance(8) ? 0 : ptrv(gt);
    BYTE *col = xalloc(0x28);
    for (DWORD o = 0; o < 0x28; o += 4)
        at_(col, o) = (DWORD)g.range(-10, 200);
    at_(col, 0x18) = g.below(6);
    g_case.fixed[1] = ptrv(col);
    g_case.fixed[2] = ptrv(object(9));
    BYTE *name = xalloc(4);
    make_text(name, TEXTS[g.below(8)]);
    g_case.fixed[3] = ptrv(name);
    set_kind(C_OBJECT, 37, K_BOOL);
    set_kind(C_OBJECT, 39, K_SMALL);
    set_kind(C_OBJECT, 45, K_SMALL);
    case_global(0x713f14, g.chance(8) ? 0 : 0x123456);
    fake_list(g, 0x6be1e0, 0, true);
    g_case.argspec[36][0] = A_IGNORE;   // (the clip rectangle on the stack)
}

// ---- The properties file name -------------------------------------------------------------------------------------

void s_name(Gen &g)
{
    object_args(g_object_args, "");
    BYTE *o = xalloc(0x80);
    at_(o, 0x6c) = ptrv(TEXTS[g.below(8)]);
    BYTE *line = xalloc(0x18);
    make_text(line + 0x14, TEXTS[g.below(8)]);
    at_(o, 0x10) = ptrv(line);
    const DWORD k = g.below(12);
    g_kind = k < 10 ? at<DWORD(__cdecl *)()>(KINDS[k])() : k == 10 ? 1 : 0;   // 1: none of them
    BYTE *out = xalloc(4);
    make_text(out, TEXTS[g.below(8)]);
    g_args[0] = ptrv(o), g_args[1] = ptrv(out);
    BYTE *model = xalloc(0x80);
    at_(model, 0x6c) = ptrv(TEXTS[g.below(8)]);
    g_case.fixed[4] = g.chance(4) ? 0 : ptrv(model);
    const DWORD lists[] = {0x6c1d50, 0x6c0c08, 0x6c0800, 0x6be1e0, 0x6c0948, 0x6c06a0};
    for (DWORD l : lists)
        fake_list(g, l, ptrv(o), l == 0x6c1d50 || l == 0x6be1e0 || l == 0x6c06a0);
    case_global(0x6c2080, ptrv(win(11)));
}

// ---- The model preview --------------------------------------------------------------------------------------------

BYTE *preview(Gen &g)
{
    object_args(g_object_args, "26:0,37:3,46:0,84:0,104:2");
    BYTE *p = xalloc(0x20);
    at_(p, 0xc) = ptrv(object(3));
    const float timers[] = {0.0f, 0.01f, 0.5f, 2.0f, -1.0f};
    memcpy(p + 0x14, &timers[g.below(5)], 4);
    BYTE *seq = object(5);
    g_case.fixed[5] = ptrv(seq);
    set_kind(C_OBJECT, 26, K_FLOAT);
    set_kind(C_OBJECT, 46, K_FLOAT);
    set_kind(C_OBJECT, 84, K_FLOAT);
    g_case.argspec[37][0] = A_IGNORE;   // (the rectangle on the stack; the output block is compared)
    g_self = p;
    return seq;
}
void s_update(Gen &g)
{
    BYTE *seq = preview(g);
    at_(seq, 0x48) = 1 + g.below(30);
    const float dts[] = {0.0f, 0.016f, 1.0f / 30.0f, 0.5f, -0.25f, 2.5f};
    float dt = dts[g.below(6)];
    memcpy(&g_args[0], &dt, 4);
    const char *names[] = {"Idle", "idle", "Walk", ""};
    g_case.fixed[6] = ptrv(names[g.below(4)]);
}
void s_bounds(Gen &g)
{
    BYTE *seq = preview(g);
    at_(seq, 0x48) = g.below(5);
    at_(seq, 0x4c) = g.below(5);
    g_args[0] = ptrv(xalloc(16));
}

const Test TESTS[] = {
    {"MissionOver Draw", 0x50dc10, CALL_THIS, 1, 0, 400, s_draw, STUBS, NSTUBS},
    {"PropertiesFileName", 0x4f8120, CALL_FAST, 2, 0xff, 600, s_name, STUBS, NSTUBS},
    {"ModelPreview Update", 0x517f20, CALL_THIS, 1, 0, 400, s_update, STUBS, NSTUBS},
    {"ModelPreview Bounds", 0x518080, CALL_THIS, 1, ~0u, 300, s_bounds, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(p5_leftovers)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        const DWORD g4[] = {0x713f14, 0x6c2080, 0x6be1e0, 0x6be210, 0x6be214, 0x6c1d50, 0x6c1d80, 0x6c1d84, 0x6c0c38,
                            0x6c0c3c, 0x6c0830, 0x6c0834, 0x6c0978, 0x6c097c, 0x6c06a0, 0x6c06d0, 0x6c06d4};
        for (DWORD a : g4)
            g_globals.add(a, 4);
    }
    const char *const WHAT[] = {"p5 leftovers: the mission over window's drawing 0x50dc10", "p5 leftovers: the properties file name callback 0x4f8120",
                                "p5 leftovers: the model preview's slot 2 0x517f20", "p5 leftovers: the model preview's slot 3 0x518080"};
    for (int k = 0; k < 4; k++) {
        Tally t{WHAT[k]};
        run_tests(t, g_hooks, &g_globals, TESTS + k, 1, &base, 71000 + 1000u * (unsigned)k);
        t.report();
    }
}

}  // namespace zax
