# Sourced by reimpl/build.sh --exe (in reimpl/, with OUT, TMP, CLANG, CXXFLAGS and LIBCFLAGS set): builds
# Zax-Standalone.exe, the scaffold of the standalone exe (docs/DECOMPILATION.md 13, "Batch 6-1: the scaffold").
#  - the production modules of src/ (not the check modules, not main.cpp's hook installer and self-tests, not coverage.cpp) with
#    -DZAX_STANDALONE and the DLL's flags, the libraries of src/libs/ as for the DLL;
#  - standalone/image_space.s first (the range 0x401000-0x75d000 of Zax.exe's image, reserved and inaccessible) and
#    standalone/loader.cpp (the entry: own blocks, imports, the function table, launcher switches, TraceApi, the code
#    values, WinMainCRTStartup; since batch 6-10c without any image of Zax.exe);
#  - the address layer's own data blocks (docs/exe_layout.csv) by tools/extract_exe_data.py (batch 6-3);
#  - the launcher's switches: standalone/launcher.cpp (the switches of zax-launcher/zax_launcher_dll.c through the layer);
#  - resources (icon, string table, version) from the user's Zax.exe by tools/standalone_pe.py into the build's
#    temporary folder; linked at 0x400000, then the original's header values (no ASLR, no DEP, version 4.0).
# Nothing of Zax.exe is in the repository; the built exe holds its icon, strings and version and the initial bytes of
# the exe's data (own_blocks.cpp, in the build's temporary folder). At run time it needs no Zax.exe (since batch 6-10c).
ZAX_EXE=${ZAX_EXE:-"$GAME_DATA/Zax - German/Zax.exe"}
[ -f "$ZAX_EXE" ] || { echo "Zax.exe not found (ZAX_EXE=...)" >&2; exit 1; }
PY=${PYTHON:-}
if [ -z "$PY" ]; then
  COMMON=$(git rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)
  for p in ../.venv/bin/python "${COMMON:+$COMMON/../.venv/bin/python}"; do
    [ -n "$p" ] && [ -x "$p" ] && { PY=$p; break; }
  done
fi
[ -n "$PY" ] || { echo "Python with pefile not found (PYTHON=...)" >&2; exit 1; }
"$PY" -I ../tools/standalone_pe.py resources "$ZAX_EXE" "$TMP"
# The address layer's own data blocks (docs/exe_layout.csv, home `own`) with their initial bytes and relocations from
# the user's Zax.exe (tools/extract_exe_data.py; batch 6-3), into the temporary folder; the pointer map next to the exe
"$PY" -I ../tools/extract_exe_data.py "$ZAX_EXE" "$TMP"

# The production modules only: the hook tables of those modules, no checks
PROD=$(ls src/*.cpp | grep -vE '/(check|[a-z0-9_]*_check)[^/]*\.cpp$' | grep -vE '^src/(main|coverage)\.cpp$')
grep -ho '^ZAX_HOOKS([A-Za-z0-9_]*)' $PROD | sed 's/^ZAX_HOOKS/ZAX_HOOK_MODULE/' > "$TMP/hook_tables.inc"
JOBS=${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)}
export CLANG CXXFLAGS LIBCFLAGS TMP
printf '%s\n' $PROD | xargs -P "$JOBS" -I{} sh -c 'f={}; "$CLANG" $CXXFLAGS -DZAX_STANDALONE -I"$TMP" -c "$f" -o "$TMP/$(basename "${f%.cpp}").o"'
# the libraries' C runtime calls through the function table (src/libc/zax_libc.h, addr_layer_c.h; batch 6-4)
ls src/libs/*.c | xargs -P "$JOBS" -I{} sh -c 'f={}; "$CLANG" $LIBCFLAGS -DZAX_STANDALONE -I"$TMP" -c "$f" -o "$TMP/lib_$(basename "${f%.c}").o"'
OBJS=
for f in $PROD; do OBJS="$OBJS $(basename "${f%.cpp}").o"; done
for f in src/libs/*.c; do OBJS="$OBJS lib_$(basename "${f%.c}").o"; done
# ZAX_EXE_BASE (since batch 6-10b): the test build at a second image base, e.g. 0x10000000: without the image space,
# the loader reserves the original's range 0x400000-0x760000 PAGE_NOACCESS itself (ZAX_IMAGE_ALLOC; docs/DECOMPILATION.md
# 13, "Batch 6-10b", "Batch 6-10c")
BASE=${ZAX_EXE_BASE:-0x400000}
LOADER_FLAGS=
FIRST=image_space.o
if [ "$BASE" != 0x400000 ]; then LOADER_FLAGS=-DZAX_IMAGE_ALLOC; FIRST=; fi
"$CLANG" -target i386-pc-windows-msvc -c standalone/image_space.s -o "$TMP/image_space.o"
"$CLANG" $CXXFLAGS -DZAX_STANDALONE $LOADER_FLAGS -I"$TMP" -c standalone/loader.cpp -o "$TMP/loader.o"
"$CLANG" $CXXFLAGS -DZAX_STANDALONE -c "$TMP/own_blocks.cpp" -o "$TMP/own_blocks.o"
# The launcher's switches through the address layer (since batch 6-10c; until then zax-launcher/zax_launcher_dll.c as it
# is, patching the copied image)
"$CLANG" $CXXFLAGS -DZAX_STANDALONE -I"$TMP" -c standalone/launcher.cpp -o "$TMP/launcher.o"
# image_space.o must be the first object: its .text chunk is then the first of the output's .text (0x401000)
(cd "$TMP" &&
 zig build-exe $FIRST loader.o own_blocks.o launcher.o $OBJS standalone.rc -target x86-windows.xp-gnu -lkernel32 -luser32 \
   -O ReleaseSmall -fstrip -fentry=zax_standalone_entry@0 --subsystem windows --image-base $BASE --name Zax-Standalone)
"$PY" -I ../tools/standalone_pe.py header "$ZAX_EXE" "$TMP/Zax-Standalone.exe" $BASE
cp "$TMP/Zax-Standalone.exe" "$OUT/"
cp "$TMP/pointer_map.csv" "$TMP/code_pointers_left.csv" "$OUT/"
ls -l "$OUT"
