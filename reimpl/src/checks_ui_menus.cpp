// Checks of the game's menus (ui_menus.cpp) against the original with the toolkit's harness (checks_ui_common.h).
// The constructors run on real objects in the extra area; the toolkit's constructors they call (CDialog, CDialog2,
// CJWindow, the buttons, the text label, the sequence window, the edit field, the file dialog) are stand-ins that
// give the object the vtable of a fake of its class, a title and a font. The game's objects the menus reach
// through globals are fakes of the class "game object": the reporter (0x6c02cc) and its screen (+4: the width and
// height of the case), the application (0x713f14: the rules holder +0x2e0, the save data +0x33c/+0x34c), the
// world (0x6c2080), the object their slot 1 gives, the game type, the font list's records (0x6be210) and the
// difficulty count (0x6c2a14). Notify and the destructors run on a fake dialog (window 0).
#include "checks_ui_common.h"

namespace zax {

int hooks_ui_menus(Hook *out);

namespace {

using namespace uic;

constinit ModuleHooks g_hooks{"ui_menus", &hooks_ui_menus};
Globals g_globals;

const Stub STUBS[] = {
    UIC_ALLOC_STUBS,
    {0x403d00, CV_THIS, 2, 'e', "CDialog::CDialog", "vs"},
    {0x403ab0, CV_THIS, 2, 'e', "CDialog2::CDialog2", "vs"},
    {0x40a8a0, CV_THIS, 2, 'e', "CJWindow::CJWindow", "vs"},
    {0x40fa60, CV_THIS, 3, 'e', "Button_ctor"},
    {0x40f9b0, CV_THIS, 3, 'e', "Button::Button", "vsv"},
    {0x4127d0, CV_THIS, 2, 'e', "TextLabel::TextLabel", "vs"},
    {0x56c400, CV_THIS, 3, 'e', "SequenceWindow_Ctor", "vsv"},
    {0x410b50, CV_THIS, 2, 'e', "EditField_Ctor", "vs"},
    {0x414910, CV_THIS, 3, 'e', "FileDialog_Ctor", "vsv"},
    {0x46b050, CV_THIS, 1, 'e', "CdKeyDialog"},
    {0x562970, CV_THIS, 2, 'e', "LoadSaveScreen_Ctor"},
    {0x475eb0, CV_THIS, 2, 'e', "CreditsText_Ctor", "sv"},
    {0x4a8a90, CV_THIS, 0, 'e', "CFrame::Ctor"},
    {0x4a2650, CV_FAST, 0, K_VALUE, "DebugLog_SectionBegin", "sx"},
    {0x4a2670, CV_CDECL, 0, K_VALUE, "DebugLog_SectionEnd"},
    {0x523df0, CV_THIS, 2, K_FIXED4, "ResourceList_Find", "sv"},
    {0x40c8d0, CV_THIS, 6, K_VALUE, "CJWindow::AddChildFront"},
    {0x40c7c0, CV_THIS, 6, K_VALUE, "Window_AddChild"},
    {0x40f5b0, CV_THIS, 1, K_VALUE, "CJWindow::SetWidth"},
    {0x40f5d0, CV_THIS, 1, K_VALUE, "CJWindow::SetHeight"},
    {0x40d630, CV_THIS, 2, K_VALUE, "CJWindow::MoveTo"},
    {0x40d680, CV_THIS, 1, K_VALUE, "CJWindow::SetLeft"},
    {0x40d6c0, CV_THIS, 1, K_VALUE, "CJWindow::SetTop"},
    {0x40d510, CV_THIS, 2, K_VALUE, "CJWindow::SetSize"},
    {0x40b5c0, CV_THIS, 1, K_VALUE, "CJWindow::SetLook"},
    {0x4e3680, CV_CDECL, 0, K_VALUE, "LookFactory"},
    {0x40d230, CV_THIS, 1, K_VALUE, "CJWindow::SameWidth"},
    {0x40ca40, CV_THIS, 1, K_VALUE, "CJWindow::Focus"},
    {0x4038a0, CV_THIS, 0, K_VALUE, "CFrameWindow::AddCloseButton"},
    {0x4035f0, CV_THIS, 2, K_VALUE, "CFrameWindow::Notify"},
    {0x402f30, CV_THIS, 1, K_VALUE, "CFrameWindow::Draw"},
    {0x404000, CV_THIS, 2, K_VALUE, "CDialog::HandOver"},
    {0x4040c0, CV_THIS, 2, K_VALUE, "CDialog::ReplaceWindow"},
    {0x404180, CV_THIS, 1, K_VALUE, "CDialog::SetReplaced"},
    {0x403d70, CV_THIS, 0, K_VALUE, "CDialog::~CDialog"},
    {0x40b190, CV_THIS, 0, K_VALUE, "CJWindow::~CJWindow"},
    {0x414bc0, CV_THIS, 0, K_VALUE, "FileDialog::~FileDialog"},
    {0x4a8b00, CV_THIS, 0, K_VALUE, "CFrame::Dtor"},
    {0x475fd0, CV_THIS, 0, K_VALUE, "CreditsText_Dtor"},
    {0x40c6e0, CV_THIS, 2, K_VALUE, "CJWindow::AddModalToParent"},
    {0x40b5d0, CV_THIS, 1, K_VALUE, "CJWindow::Update"},
    {0x46b6d0, CV_CDECL, 0, K_BOOL, "CdKey_IsValid"},
    {0x48d8f0, CV_THIS, 1, K_WINDOW_NN, "DefinitionList_Object"},
    {0x5007a0, CV_THIS, 0, K_TEXT, "FieldDesc_Name"},
    {0x5d097c, CV_CDECL, 4, K_VALUE, "qsort"},
    {0x55e090, CV_THIS, 1, K_VALUE, "DefinitionList_LoadRecord"},
    {0x5a1350, CV_CDECL, 0, K_VALUE, "CZaxCustomRules_ClassInfo"},
    {0x416790, CV_THIS, 1, K_BOOL, "Object_IsKindOf"},
    {0x5a1ee0, CV_THIS, 0, K_FIXED3, "GetMainMenuDisplayOptions"},
    {0x5908f0, CV_THIS, 0, K_VALUE, "Game_FullVersionText"},
    {0x4e09b0, CV_CDECL, 3, K_VALUE, "ZString_Format", "ass"},
    {0x592310, CV_THIS, 1, K_VALUE, "FieldNameList::Value"},
    {0x5a32b0, CV_THIS, 1, K_VALUE, "CZaxWorld::SetGameType"},
    {0x5bb780, CV_THIS, 1, K_VALUE, "CZaxSaveData::SetDifficulty"},
    {0x5a3180, CV_THIS, 1, K_VALUE, "CZaxWorld::SetGameTypeByName", "s"},
    {0x51e450, CV_THIS, 0, K_VALUE, "CMusicMixer::Stop"},
    {0x4cd5a0, CV_THIS, 3, K_VALUE, "App_PlayMovie", "svv"},
    {0x59ff80, CV_THIS, 0, K_FIXED5, "Game_AppGameType"},
    {0x5ac4a0, CV_CDECL, 0, K_VALUE, "ChangeTeamReader_Get"},
    {0x5ac500, CV_THIS, 0, K_VALUE, "ContinueReader::Write"},
    {0x46ff90, CV_THIS, 0, K_VALUE, "ControlConfig::Deactivate"},
    {0x46ff70, CV_THIS, 0, K_VALUE, "ControlConfig::Activate"},
    {0x4e2760, CV_THIS, 1, K_VALUE, "Keys::ClearChars"},
    {0x5d0863, CV_CDECL, 1, K_ZERO, "atexit"},
    {0x4a2c50, CV_THIS, 0, 'e', "DirList_Ctor"},
    {0x4a2ea0, CV_THIS, 4, K_SMALL, "DirList_Read"},
    {0x4a3310, CV_THIS, 1, K_TEXT, "DirList_NameAt"},
    {0x404520, CV_THIS, 4, 'e', "CJListBox_Ctor2"},
    {0x4041f0, CV_THIS, 4, 'e', "CJListBox_Ctor"},
    {0x407870, CV_THIS, 1, K_VALUE, "CJListBox::SetRows"},
    {0x5bde70, CV_THIS, 0, K_WINDOW, "CSelectMapDialog::GameTypeList"},
};
constexpr int NSTUBS = sizeof STUBS / sizeof STUBS[0];

// Windows of the case: 0 the dialog under test (Notify, destructors), 1-4 the templates of the made buttons,
// labels, sequence windows and plain windows, 5 the reporter, 6 its screen, 7 the parent, 8 the application,
// 9 the world, 10 the object of their slot 1, 11 the game type, 12 a list, 13 the template of the edit field,
// 14 the file dialog's, 15 another button
constexpr int W_BUTTON_T = 1, W_LABEL_T = 2, W_SEQ_T = 3, W_PLAIN_T = 4, W_REPORTER = 5, W_SCREEN = 6, W_PARENT = 7,
              W_APP = 8, W_WORLD = 9, W_OBJECT = 10, W_TYPE = 11, W_LIST = 12, W_EDIT_T = 13, W_FILE_T = 14, W_OTHER = 15;

DWORD g_difficulties;
DWORD g_listbox_vt;     // the recording vtable of CJListBox   // the case's difficulty count (none: the original reads before its array)

// A made window: the template's vtable, a random rectangle, a title, the real font
void made(DWORD obj, int tmpl)
{
    if (!in_block(obj))
        return;
    BYTE *o = at<BYTE *>(obj);
    Gen &v = g_case.values;
    at_(o, 0) = at_(win(tmpl), 0);
    int *r = &at_<int>(o, 4);
    r[0] = v.range(-20, 500), r[1] = v.range(-20, 400), r[2] = r[0] + v.range(0, 300), r[3] = r[1] + v.range(0, 100);
    at_(o, 0x7c) = ptrv(real_font());
    // the title: a ZString rep of our own in the extra area (8 bytes before the text; c_str is rep + 8), so
    // the stand-in makes no calls of its own
    const char *t = TEXTS[v.below(8)];
    BYTE *rep = xalloc(8 + 48);
    if (rep) {
        for (int i = 0; t[i] && i < 47; i++)
            rep[8 + i] = (BYTE)t[i];
        at_(o, 0x50) = ptrv(rep);
    }
}

void effect(int cls, int slot, DWORD *args, DWORD answer)
{
    if (cls != -1)
        return;
    switch ((DWORD)slot) {
    case 0x40fa60:
    case 0x40f9b0: made(answer, W_BUTTON_T); break;
    case 0x4127d0: made(answer, W_LABEL_T); break;
    case 0x56c400: made(answer, W_SEQ_T); break;
    case 0x40a8a0:
    case 0x403d00:
    case 0x403ab0:
    case 0x46b050:
    case 0x562970: made(answer, W_PLAIN_T); break;
    case 0x410b50: made(answer, W_EDIT_T); break;
    case 0x414910: made(answer, W_FILE_T); break;
    case 0x4a2c50: made(answer, W_TYPE); break;
    case 0x404520:
    case 0x4041f0:
        made(answer, W_FILE_T);
        at_(at<BYTE *>(answer), 0) = g_listbox_vt;
        break;
    default: break;
    }
    (void)args;
}

void base(Gen &g)
{
    g_case.effect = &effect;
    g_case.nwin = 1;
    for (int i = 0; i < NWIN; i++)
        make_plain_window(i, C_WINDOW, g);
    make_plain_window(0, C_DIALOG, g);
    make_window(W_BUTTON_T, C_BUTTON);
    make_window(W_LABEL_T, C_LABEL);
    make_window(W_EDIT_T, C_EDIT);
    make_window(W_FILE_T, C_FILE_DIALOG);
    g_listbox_vt = at_(make_window(W_OTHER, C_LISTBOX), 0);
    make_plain_window(W_OTHER, C_BUTTON, g);
    const int OBJECTS[] = {W_REPORTER, W_SCREEN, W_APP, W_WORLD, W_OBJECT, W_TYPE, W_LIST};
    for (int i : OBJECTS)
        make_window(i, C_OBJECT);
    at_(win(W_REPORTER), 4) = ptrv(win(W_SCREEN));
    set_kind(C_OBJECT, 1, K_FIXED2);
    set_kind(C_OBJECT, 88, K_FIXED2);
    set_kind(C_OBJECT, 13, K_BOOL);
    set_kind(C_OBJECT, 37, K_BOOL);
    set_kind(C_OBJECT, 45, K_FIXED0);
    set_kind(C_OBJECT, 39, K_FIXED1);
    const DWORD widths[] = {640, 800, 1024};
    g_case.fixed[0] = g.chance(6) ? g.below(2000) : widths[g.below(3)];
    g_case.fixed[1] = g.chance(6) ? g.below(2000) : g_case.fixed[0] * 3 / 4;
    g_case.fixed[2] = ptrv(win(W_OBJECT));
    // the main menu's options (Main Menu Display: points at +8 … +0x2c, font indices +0x18 and +0x30)
    BYTE *opt = xalloc(0x40);
    for (DWORD o = 8; o < 0x34; o += 4)
        at_<int>(opt, o) = g.range(-100, 700);
    at_(opt, 0x18) = g.below(6), at_(opt, 0x30) = g.chance(5) ? g.next() : g.below(6);
    g_case.fixed[3] = ptrv(opt);
    g_case.fixed[4] = g.chance(2) ? 0xffffffff : g.below(40);
    g_case.fixed[5] = g.chance(3) ? 0 : ptrv(win(W_TYPE));
    // the application: the rules holder, the save data
    BYTE *app = win(W_APP);
    BYTE *holder = xalloc(0x40);
    at_(holder, 0x38) = g.chance(3) ? 0 : ptrv(win(W_LIST));
    at_(app, 0x2e0) = ptrv(holder);
    BYTE *save = xalloc(0x20);
    at_(save, 0x1c) = g.chance(2) ? 0 : g.next();
    at_(app, 0x33c) = ptrv(save);
    at_(app, 0x34c) = g.next();
    at_(win(W_WORLD), 0x334) = g.next();
    // the font list's records (0x80 bytes each: +0x78 the font, +0x7f being loaded)
    const DWORD nfonts = g.below(5);
    BYTE *recs = nfonts ? xalloc(nfonts * 0x80) : nullptr;
    for (DWORD i = 0; i < nfonts; i++) {
        at_(recs, i * 0x80 + 0x78) = g.chance(3) ? 0 : g.next();
        recs[i * 0x80 + 0x7f] = g.chance(4) ? 1 : 0;
    }
    case_global(0x6be210, g.chance(8) ? 0 : ptrv(recs));
    case_global(0x6be214, nfonts);
    case_global(0x6c02cc, ptrv(win(W_REPORTER)));
    case_global(0x713f14, ptrv(app));
    case_global(0x6c2080, ptrv(win(W_WORLD)));
    case_global(0x714558, g.chance(2) ? 0 : ptrv(win(W_OTHER)));
    case_global(0x62485c, g.chance(3) ? 1 : 0);
    case_global(0x714560, g.chance(3) ? 1 : 0);
    case_global(0x62e5e0, g.below(2));
    g_difficulties = 1 + g.below(6);
    case_global(0x6c2a14, g_difficulties);
    g_self = win(0);
}

// The constructors: a real object in the extra area, the parent window 7
void s_ctor(Gen &g)
{
    g_self = xalloc(0x200);
    g_args[0] = g.chance(5) ? 0 : ptrv(win(W_PARENT));
    g_args[1] = ptrv(TEXTS[g.below(8)]);
}
void s_ctor_chat(Gen &) { g_self = xalloc(0x200); }
void s_none(Gen &) {}
void s_flags(Gen &g) { g_args[0] = g.chance(4) ? 0 : g.chance(2) ? 1 : g.next(); }
void s_frame(Gen &g) { g_args[0] = g.next(); }

// The dialog under test with member windows at the given offsets (fake buttons, some null)
void members(Gen &g, DWORD first, DWORD last)
{
    for (DWORD o = first; o <= last; o += 4)
        at_(g_self, o) = g.chance(6) ? 0 : ptrv(xalloc(0x10));
}
// The child a notification comes from: one of the members, another window or null; what 0 mostly
void from_member(Gen &g, DWORD first, DWORD last)
{
    const DWORD n = (last - first) / 4 + 1;
    const DWORD k = g.below(n + 2);
    g_args[0] = k < n ? at_(g_self, first + 4 * k) : k == n ? ptrv(win(W_OTHER)) : 0;
    g_args[1] = g.chance(4) ? g.below(5) : 0;
}
void s_notify_zax(Gen &g)
{
    members(g, 0x138, 0x150);
    from_member(g, 0x138, 0x150);
}
void s_notify_difficulty(Gen &g)
{
    const DWORD n = g_difficulties;
    DWORD *buttons = reinterpret_cast<DWORD *>(xalloc(4 * (n + 1)));
    for (DWORD i = 0; i < n; i++)
        buttons[i] = g.chance(8) ? 0 : ptrv(xalloc(0x10));
    at_(g_self, 0x13c) = ptrv(buttons);
    at_(g_self, 0x14c) = ptrv(xalloc(0x10));
    const DWORD k = g.below(n + 3);
    g_args[0] = k < n ? buttons[k] : k == n ? at_(g_self, 0x14c) : k == n + 1 ? ptrv(win(W_OTHER)) : 0;
    g_args[1] = g.chance(4) ? g.below(5) : 0;
}
void s_ok(Gen &g)
{
    at_(g_self, 0x144) = ptrv(win(W_LIST));
    at_(g_self, 0x148) = g.next();
    g_args[0] = g.chance(3) ? 0 : g.next();
}
void s_select_map_dtor(Gen &g)
{
    at_(g_self, 0x148) = g.chance(3) ? 0 : ptrv(win(W_LIST));
    s_flags(g);
}
void s_difficulty_dtor(Gen &g)
{
    at_(g_self, 0x13c) = g.chance(3) ? 0 : ptrv(xalloc(0x20));
    at_(g_self, 0x140) = g.below(8), at_(g_self, 0x144) = g.below(8);
    s_flags(g);
}
void s_credits_dtor(Gen &g)
{
    at_(g_self, 0x100) = g.chance(3) ? 0 : ptrv(xalloc(0x20));
    s_flags(g);
}
void s_update(Gen &g)
{
    g_self[0x150] = g.chance(3) ? 1 : 0;
    g_args[0] = g.next();
}

// The objects the constructors and Notify make lie in the extra area; nothing to destroy (all stand-ins)
// The file dialog under test (window 0 as a file dialog with its members)
void file_dialog(Gen &g)
{
    BYTE *w = make_plain_window(0, C_FILE_DIALOG, g);
    if (g.chance(4))
        at_(w, 0x7c) = ptrv(fake_font(g.below(40)));
    at_(w, 0x134) = ptrv(win(W_PARENT));   // the list (Notify reads it on Open; the original faults without one)
    at_(w, 0x138) = g.chance(5) ? 0 : ptrv(xalloc(0x10));
    at_(w, 0x13c) = g.chance(5) ? 0 : ptrv(xalloc(0x10));
    at_(w, 0x140) = ptrv(win(W_TYPE));
    g_self = w;
}
void s_fd_ctor(Gen &g)
{
    s_ctor(g);
    g_args[2] = g.chance(2) ? 0 : g.next();
}
void s_fd_run(Gen &g)
{
    file_dialog(g);
    make_plain_window(W_PARENT, C_LISTBOX, g);
    set_kind(C_FILE_DIALOG, 79, K_WINDOW);
    set_kind(C_OBJECT, 11, g.chance(2) ? K_ZERO : K_VALUE);
    g_args[0] = g.chance(3) ? 0 : g.chance(2) ? 1 : g.next();
    g_args[1] = g.next();
    g_args[2] = g.next();
}
void s_fd_dtor(Gen &g)
{
    file_dialog(g);
    at_(g_self, 0x140) = g.chance(3) ? 0 : ptrv(win(W_LIST));
    s_flags(g);
}
void s_fd_notify(Gen &g)
{
    file_dialog(g);
    make_plain_window(W_PARENT, C_LISTBOX, g);
    set_kind(C_LISTBOX, 77, K_SMALL);
    set_kind(C_OBJECT, 22, K_SMALL);
    const DWORD k = g.below(5);
    g_args[0] = k < 3 ? at_(g_self, 0x134 + 4 * k) : k == 3 ? ptrv(win(W_OTHER)) : 0;
    g_args[1] = g.chance(4) ? g.below(5) : 0;
}

// The globals the menus read or write (dwords, bytes)
constexpr DWORD GLOBALS4[] = {0x6be210, 0x6be214, 0x6c02cc, 0x713f14, 0x6c2080, 0x714558, 0x6c2a14};
constexpr DWORD GLOBALS1[] = {0x62485c, 0x714560, 0x62e5e0, 0x62e5c4};

const Test TESTS[] = {
    {"MenuDialog ctor", 0x5bdc90, CALL_THIS, 2, ~0u, 300, s_ctor, STUBS, NSTUBS},
    {"MenuDialog dtor", 0x5bdd40, CALL_THIS, 1, ~0u, 60, s_flags, STUBS, NSTUBS},
    {"SelectMapBase dtor", 0x5bddd0, CALL_THIS, 1, ~0u, 60, s_flags, STUBS, NSTUBS},
    {"SelectMap dtor", 0x5bde10, CALL_THIS, 1, ~0u, 100, s_select_map_dtor, STUBS, NSTUBS},
    {"SelectMap OnOk", 0x5be050, CALL_THIS, 1, 0, 150, s_ok, STUBS, NSTUBS},
    {"MainMenu ctor", 0x5be4a0, CALL_THIS, 1, ~0u, 400, s_ctor, STUBS, NSTUBS},
    {"MainMenu background dtor", 0x5bec40, CALL_THIS, 1, ~0u, 50, s_flags, STUBS, NSTUBS},
    {"MainMenu fire dtor", 0x5bed70, CALL_THIS, 1, ~0u, 50, s_flags, STUBS, NSTUBS},
    {"MainMenu dtor", 0x5beda0, CALL_THIS, 1, ~0u, 60, s_flags, STUBS, NSTUBS},
    {"EndCredits dtor", 0x5bf200, CALL_THIS, 1, ~0u, 50, s_flags, STUBS, NSTUBS},
    {"MainMenu Update", 0x5bf230, CALL_THIS, 1, 0, 200, s_update, STUBS, NSTUBS},
    {"DifficultyMenu ctor", 0x5bf820, CALL_THIS, 1, ~0u, 400, s_ctor, STUBS, NSTUBS},
    {"DifficultyMenu dtor", 0x5bfa90, CALL_THIS, 1, ~0u, 100, s_difficulty_dtor, STUBS, NSTUBS},
    {"DifficultyMenu Notify", 0x5bfb70, CALL_THIS, 2, 0, 400, s_notify_difficulty, STUBS, NSTUBS},
    {"ZaxMenu ctor", 0x5bfc40, CALL_THIS, 1, ~0u, 400, s_ctor, STUBS, NSTUBS},
    {"ZaxMenu dtor", 0x5bfec0, CALL_THIS, 1, ~0u, 60, s_flags, STUBS, NSTUBS},
    {"ZaxMenu Notify", 0x5bff00, CALL_THIS, 2, 0, 600, s_notify_zax, STUBS, NSTUBS},
    {"QuitMenu ctor", 0x5c0220, CALL_THIS, 2, ~0u, 300, s_ctor, STUBS, NSTUBS},
    {"QuitMenu dtor", 0x5c03f0, CALL_THIS, 1, ~0u, 60, s_flags, STUBS, NSTUBS},
    {"CentredMenu Draw", 0x5c0430, CALL_THIS, 1, 0, 200, s_frame, STUBS, NSTUBS},
    {"CreditsControl dtor", 0x5c05c0, CALL_THIS, 1, ~0u, 100, s_credits_dtor, STUBS, NSTUBS},
    {"CreditsMenu ctor", 0x5c07f0, CALL_THIS, 1, ~0u, 300, s_ctor, STUBS, NSTUBS},
    {"CreditsMenu dtor", 0x5c0930, CALL_THIS, 1, ~0u, 60, s_flags, STUBS, NSTUBS},
    {"DiedMenu ctor", 0x5c09a0, CALL_THIS, 1, ~0u, 200, s_ctor, STUBS, NSTUBS},
    {"DiedMenu dtor", 0x5c0a30, CALL_THIS, 1, ~0u, 60, s_flags, STUBS, NSTUBS},
    {"ChatWindow Open", 0x5c1040, CALL_CDECL, 0, 0, 200, s_none, STUBS, NSTUBS},
    {"ChatWindow ctor", 0x5c1080, CALL_THIS, 0, ~0u, 200, s_ctor_chat, STUBS, NSTUBS},
    {"ChatWindow dtor", 0x5c1120, CALL_THIS, 1, ~0u, 100, s_flags, STUBS, NSTUBS},
    {"FileDialog ctor", 0x414910, CALL_THIS, 3, ~0u, 200, s_fd_ctor, STUBS, NSTUBS},
    {"FileDialog deleting dtor", 0x414940, CALL_THIS, 1, ~0u, 100, s_fd_dtor, STUBS, NSTUBS},
    {"FileDialog Run", 0x414990, CALL_THIS, 3, 0, 500, s_fd_run, STUBS, NSTUBS},
    {"FileDialog dtor", 0x414bc0, CALL_THIS, 0, ~0u, 100, s_fd_dtor, STUBS, NSTUBS},
    {"FileDialog Notify", 0x414bf0, CALL_THIS, 2, 0, 400, s_fd_notify, STUBS, NSTUBS},
};

}  // namespace

ZAX_CHECKS(ui_menus)
{
    (void)cls;
    (void)n;
    if (!g_globals.n) {
        for (DWORD a : GLOBALS4)
            g_globals.add(a, 4);
        for (DWORD a : GLOBALS1)
            g_globals.add(a, 1);
    }
    Tally t{"ui menus: the menu dialogs, main menu, difficulty, game menu, file dialog, quit, credits, chat"};
    // The launcher's patches in the game menu (its "Load Map" button at 0x5bfe14, the main menu call 0x5bff30
    // redirected) out for the cases, so both sides run the exe's code and the reimplementation runs on side 1
    BYTE saved_site[5], saved_call[5];
    BYTE *const site = at<BYTE *>(0x5bfe14), *const call = at<BYTE *>(0x5bff30);
    memcpy(saved_site, site, 5);
    memcpy(saved_call, call, 5);
    const BYTE exe_site[5] = {0xb9, 0x38, 0x01, 0x00, 0x00};   // mov ecx, 0x138
    BYTE exe_call[5] = {0xe8};
    const DWORD rel = 0x5be4a0 - 0x5bff35;
    memcpy(exe_call + 1, &rel, 4);
    write_code(site, exe_site);
    write_code(call, exe_call);
    run_tests(t, g_hooks, &g_globals, TESTS, sizeof TESTS / sizeof TESTS[0], &base, 20000);
    write_code(site, saved_site);
    write_code(call, saved_call);
    t.report();
}

}  // namespace zax
