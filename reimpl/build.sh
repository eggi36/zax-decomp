#!/bin/bash
# Builds Zax-Reimpl.dll (32-bit Windows) without a C/C++ runtime library (only kernel32/user32).
# Compiled with clang and the MSVC ABI (i386-pc-windows-msvc) so vtables, destructors, struct
# layout and calling conventions match those of Zax.exe (VC++ 6); no Windows SDK needed
# (src/win32.h). Linked with zig (ships lld and the import libraries).
# Needs: brew install llvm zig. Other clang: CLANG=/path/clang.
# The launcher loads the DLL if it sits next to Zax.exe (/Original skips the hooks).
# Usage: reimpl/build.sh [output dir]   (default: out/reimpl)
#        reimpl/build.sh --exe [output dir]   the standalone exe Zax-Standalone.exe instead (default: out/standalone;
#        docs/DECOMPILATION.md 13, batch 6-1): the production modules without the checks and main.cpp, the loader of
#        standalone/, the launcher's switches built in; needs the user's Zax.exe at build time for the icon, the
#        string table and the version (ZAX_EXE, default the build 1308 exe below GAME_DATA found by tools/game_paths.py, see below) and next to the exe at run time
# ZAX_KEEP_TMP=dir (an absolute path) copies the build's temporary folder (the objects, the generated includes) there at the end, for
# tools/mmx_x87_scan.py (a folder outside the repository's tracked files, e.g. below out/)
# Reproducible: the same sources, game data and tools give the same bytes from any folder (docs/DECOMPILATION.md
# section 5, "Reproducible builds"). The object order on the link line comes from the globs below, whose order follows
# the locale's collation (en_US/de_DE.UTF-8 put ai_chase_scan.cpp before ai_chase.cpp, C after it): fixed to C.
# The game data folder (ZAX_GAME_DATA; default gamedata/ of the checkout, else that of the main checkout when this
# is a git worktree) supplies Zax.exe for --exe; the DLL takes nothing from it (until 2026-11-01 it embedded Local.txt
# of the German language pack for the Local.txt checks, now zax-testtools/tools/vmtest.py sends that file with the job).
set -e
export LC_ALL=C
cd "$(dirname "$0")"
GAME_DATA=${ZAX_GAME_DATA:-}
if [ -z "$GAME_DATA" ]; then
  GAME_DATA=../gamedata
  if [ ! -d "$GAME_DATA" ]; then
    COMMON=$(git rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)
    [ -n "$COMMON" ] && [ -d "$COMMON/../gamedata" ] && GAME_DATA="$COMMON/../gamedata"
  fi
fi
MODE=dll
if [ "$1" = --exe ]; then MODE=exe; shift; fi
DEFAULT_OUT=../out/reimpl
[ $MODE = exe ] && DEFAULT_OUT=../out/standalone
OUT=$(mkdir -p "${1:-$DEFAULT_OUT}" && cd "${1:-$DEFAULT_OUT}" && pwd)
CLANG=${CLANG:-$(ls -d /opt/homebrew/opt/llvm*/bin/clang 2>/dev/null | sort -V | tail -1)}
[ -x "$CLANG" ] || { echo "clang not found (brew install llvm or CLANG=...)" >&2; exit 1; }
TMP=$(mktemp -d)
trap '[ -n "$ZAX_KEEP_TMP" ] && mkdir -p "$ZAX_KEEP_TMP" && cp -R "$TMP"/. "$ZAX_KEEP_TMP"/; rm -rf "$TMP"' EXIT
# x87 instead of SSE: the exe computes with the FPU (80 bit internally), the reimplementation must
# round the same way. With the MSVC ABI long double is otherwise only 64 bit; -mlong-double-80 only
# applies internally (the exe never passes long double, floating-point results come back in st(0)).
# -fno-delete-null-pointer-checks: VC++ 6 never takes a dereference as proof that a pointer is not null, so the
# exe keeps every null test and makes every access through a null pointer; clang otherwise drops a null path
# whose pointer is dereferenced afterwards (docs/DECOMPILATION.md 16, "Batch nulltest"). Also for the libraries.
CXXFLAGS="-target i386-pc-windows-msvc -march=pentiumpro -mno-sse -mlong-double-80 -O2 -std=c++20 -Wall -Wextra
  -ffreestanding -fno-builtin -fno-exceptions -fno-rtti -fno-stack-protector -mno-stack-arg-probe
  -ffp-contract=off -fno-delete-null-pointer-checks"
# The address layer's keys for reimpl.h (docs/DECOMPILATION.md 13, "Batch 6-3"): docs/reimpl.csv and docs/exe_layout.csv
python3 -I ../tools/exe_layout.py tables "$TMP" >/dev/null
# Class variables of all registered classes for the checks (checks.cpp)
awk -F, 'NR > 1 && $9 != "" { print $9 "," }' ../docs/classes.csv > "$TMP/class_vars.inc"
# Hook tables (ZAX_HOOKS in reimpl.h) and checks (ZAX_CHECKS in check_util.h) of all modules, so a
# new module needs no change in main.cpp or checks.cpp
grep -ho '^ZAX_HOOKS([A-Za-z0-9_]*)' src/*.cpp | sed 's/^ZAX_HOOKS/ZAX_HOOK_MODULE/' > "$TMP/hook_tables.inc"
grep -ho '^ZAX_CHECKS([A-Za-z0-9_]*)' src/*.cpp | sed 's/^ZAX_CHECKS/ZAX_CHECK_MODULE/' > "$TMP/check_list.inc"
# The milestone audit's tables for checks_not_hookable.cpp: the addresses of docs/reimpl.csv, and the rows of
# docs/not_reimplemented.csv (tools/audit.py) that stay the exe's code, with the hash of their bytes
awk -F, 'NR > 1 { print $1 "," }' ../docs/reimpl.csv > "$TMP/reimpl_addresses.inc"
awk -F, 'NR > 1 && ($3 == "identical by bytes" || $3 == "covered by caller" || $3 == "no function" || $3 == "dead code") {
  print "{" $1 ", " $2 ", " $4 ", \"" $7 "\", \"" $3 "\"}," }' ../docs/not_reimplemented.csv > "$TMP/not_hookable_table.inc"
# zlib 1.1.3, libogg and libvorbis 1.0 beta 4 from their original sources (third_party/, unchanged; one
# wrapper per source file in src/libs/, src/libs/libs.h). As C with __fastcall as the default convention,
# like the exe's copies (/Gr); clang only applies that default with SSE2 in the target, which the wrappers
# turn off again for every function (x87 only). Their C runtime is src/libc/ (the exe's own functions).
# Structures packed to 4 bytes like the exe's copies (/Zp4: OggVorbis_File's bittrack at +0x64, its os at
# +0x74, vorbis_block 0x6c bytes; zlib has no 8-byte members). Stack probes stay on for their alloca
# (_chkstk in lib_vorbis.cpp); -w: the sources are K&R-era C.
LIBCFLAGS="-target i386-pc-windows-msvc -march=pentiumpro -Xclang -target-feature -Xclang +sse2
  -Xclang -fdefault-calling-conv=fastcall -fpack-struct=4 -mlong-double-80 -O2 -std=gnu89 -w -ffreestanding -fno-builtin
  -fno-stack-protector -ffp-contract=off -fno-delete-null-pointer-checks -fno-vectorize -fno-slp-vectorize -fms-extensions -nostdlibinc
  -isystem src/libc -isystem $("$CLANG" -print-resource-dir)/include -Ithird_party/zlib-1.1.3
  -Ithird_party/libogg-1.0beta4/include -Ithird_party/libvorbis-1.0beta4/include -Ithird_party/libvorbis-1.0beta4/lib"
# The standalone exe: standalone/build_exe.sh with the flags above (the same compiler flags as the DLL)
if [ $MODE = exe ]; then
  . ./standalone/build_exe.sh
  exit 0
fi
OBJS=
for f in src/*.cpp; do
  o="$TMP/$(basename "${f%.cpp}").o"
  "$CLANG" $CXXFLAGS -I"$TMP" -c "$f" -o "$o"
  OBJS="$OBJS $o"
done
for f in src/libs/*.c; do
  o="$TMP/lib_$(basename "${f%.c}").o"
  "$CLANG" $LIBCFLAGS -c "$f" -o "$o"
  OBJS="$OBJS $o"
done
(cd "$TMP" &&
 zig build-lib -dynamic $OBJS -target x86-windows.xp-gnu -lkernel32 -luser32 -O ReleaseSmall -fstrip \
   -fentry=DllMain@12 -fno-dll-export-fns --name Zax-Reimpl)
cp "$TMP/Zax-Reimpl.dll" "$OUT/"
ls -l "$OUT"
