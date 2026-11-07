// Checks inside the running game, once the classes are registered.
//
// Some tests allocate from the exe's pool allocator, which is not thread-safe, so they run in the
// game's main thread: after the delay the check thread raises a flag, and run_due_checks (called by
// the reimplemented ZString constructor, which the game calls constantly) runs them there.
#pragma once
#include "reimpl.h"

namespace zax {

// Called from DllMain (in the launcher's loader thread, before the exe starts). after_ms 0 = off.
// hooked: the hooks are installed (otherwise only the class table is written, for comparing a run
// with /Original). in_game_thread: the ZString constructor hook is installed; if not, the class
// table checks run in the check thread and the tests that allocate are skipped.
// only: [Reimpl] OnlyChecks= from Zax-Reimpl.ini, a comma list of module names (ZAX_CHECKS(name));
// "base" stands for the older checks in checks.cpp. Empty: all checks.
void start_checks(DWORD after_ms, bool hooked, bool in_game_thread, const char *only);

// [Reimpl] CheckScale= as text, e.g. "0.1" (check_util.h scaled()); empty or invalid: all cases
void set_check_scale(const char *text);

// The level's way point map before the modules' checks change the level (checks_waypoint_map.cpp)
void snapshot_level_way();

// Parts in the running level's layer grid, -1 without a level (checks_layer_parts.cpp)
int level_part_count();

// The C runtime's atexit table (__onexitbegin 0x71755c .. __onexitend 0x717558) in the log, every entry in
// registration order: the static initializers register their exit stubs in the order they run, so a run
// with /Original and one with the hooks show the start order is the same (batch C2, checks_static_c2.cpp)
void log_start_order();

// Cheap when nothing is due
void run_due_checks();

}  // namespace zax
