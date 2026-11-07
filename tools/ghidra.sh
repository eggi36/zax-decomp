#!/bin/sh
# Create or update the persistent Ghidra project of Zax.exe in out/ghidra/.
#   tools/ghidra.sh           create the project (if needed, ~1 min analysis), import names, export functions
#   tools/ghidra.sh --new     discard the project and create it again
# Names come from docs/ (exe_symbols.csv, classes.csv, fields.csv) via tools/ghidra_names.py.
# Open in the GUI: ghidraRun, then out/ghidra/Zax.gpr.
set -e
cd "$(dirname "$0")/.."
ROOT=$(pwd)
EXE="$ROOT/gamedata/Zax - German/Zax.exe"
export GHIDRA_INSTALL_DIR=${GHIDRA_INSTALL_DIR:-/opt/homebrew/opt/ghidra/libexec}
export JAVA_HOME=${JAVA_HOME:-/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home}
HEADLESS="$GHIDRA_INSTALL_DIR/support/analyzeHeadless"
mkdir -p out/ghidra
[ "$1" = "--new" ] && rm -rf out/ghidra/Zax.gpr out/ghidra/Zax.rep
.venv/bin/python -I tools/ghidra_names.py "$EXE"
if [ -f out/ghidra/Zax.gpr ]; then
  MODE="-process Zax.exe -noanalysis"
else
  MODE="-import $EXE"
fi
"$HEADLESS" "$ROOT/out/ghidra" Zax $MODE -scriptPath "$ROOT/tools/ghidra" \
  -postScript ZaxNames.java "$ROOT" -postScript ZaxFunctions.java "$ROOT" 2>&1 \
  | grep -E "ZaxNames|ZaxFunctions|ERROR|Exception|named|Class structures|exported|no function" || true
