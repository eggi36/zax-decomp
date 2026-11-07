// The launcher's switches in the standalone exe (reimpl/standalone/launcher.cpp; docs/DECOMPILATION.md 13, "Batch
// 6-10c"): what the reimplemented code asks where the hooked build tests the patches Zax-Launcher.dll put into the exe's
// code. Only in the exe build (ZAX_STANDALONE); the DLL keeps testing the patch sites.
#pragma once
#include "reimpl.h"

#ifdef ZAX_STANDALONE
namespace zax::launcher {
void install();                // loader.cpp, once the function table holds the replacements
bool show_fps();               // /ShowFPS: the frame counter on whatever Zax.CFG says
bool debug_draw();             // /DebugDraw: the draw pass calls each part's slot 29 after slot 30
void add_load_map(BYTE *menu); // the game menu's "Load Map" button (unless /NoLoadMap), before "End Game"
}  // namespace zax::launcher
#endif
