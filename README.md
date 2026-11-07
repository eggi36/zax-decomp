# zax-decomp

A faithful decompilation of "Zax: The Alien Hunter" (Reflexive Entertainment / JoWooD, 2001): every function of Zax.exe (German build 1308) reimplemented in C++ (`reimpl/`), first as a DLL hooked into the running original, then as a standalone 32-bit Windows exe that replays recorded single-player and LAN plays identically to the original in every frame (milestone `milestone-standalone-faithful`). Plan, approach and every batch with its evidence: `docs/DECOMPILATION.md`.

This is the faithful base: a decompilation whose code follows Zax.exe's structure; it changes only with proof that the behaviour stays the same. Related repositories, each next to this one in the same folder:

- **zax-redone** – the port built on this base (SDL3, macOS, 64 bit, widescreen, bug fixes as switches)
- **zax-launcher** – Zax-Launcher.exe, the classic launcher of the original Zax.exe (direct start into a map, debug switches)
- **zax-formats** – `zaxtools`, every file format of the game read and written byte for byte, and its command line tools
- **zax-editor** – the level editor
- **zax-testtools** – the test harness (VM jobs, comparison sets, the final check); not published

(Until 2026-11-07 the launcher, the VM helpers and the test tools were part of this repository, and the format library and the editor were one repository, zax-toolbox.)

## Contents

- `reimpl/` – the reimplementation (`build.sh`: `Zax-Reimpl.dll`, `--exe` the standalone exe)
- `tools/` – the decompilation tools (exe classes and fields, the function table, disassembler, Ghidra scripts, the audit, the rewrite and table generators)
- `docs/` – the decompilation, exe findings, bugs of the original, tables (symbols, functions, classes, fields)

## Setup

1. macOS with `brew install llvm zig` (the reimplementation builds with clang for the MSVC ABI, linked with zig).
2. Python 3.10 or newer: `python3 -m venv .venv && .venv/bin/pip install -r requirements.txt`.
3. zax-formats next to this repository (or `ZAX_FORMATS`): `tools/exe_classes.py` uses its `zaxtools`.
4. The game data of your own copy in `gamedata/Zax - German/` (`Zax.exe`, `DATA.dat`), never committed.
5. Build: `reimpl/build.sh` (DLL), `reimpl/build.sh --exe` (standalone exe).
6. `git config core.hooksPath tools/git-hooks` (the pre-push hook: no game data, no private names or paths).

The game data is not included and must never go into the repository: every player brings the original assets from their own copy.
