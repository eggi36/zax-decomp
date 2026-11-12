"""Where the game files are: the Zax.exe of a build, its game folder, DATA.dat and the language pack, found by content
instead of by a folder name (since 2026-11-12; until then every tool named the folder of the German edition).

Usage from Python (this repository's tools put tools/ on sys.path):
    import game_paths
    game_paths.exe()            # the reference exe, build 1308 (every address refers to it)
    game_paths.game_dir()       # its game folder: the 1308 exe with the reference DATA.dat (the German data set)
    game_paths.data_dat()       # DATA.dat of that folder
    game_paths.lang_pack()      # its language pack (*.RED, any case; the last in mount order) or None
    game_paths.exe_for('1314')  # another edition by its key in BUILDS (1305, 1309, 1309-es, 1309-fr, 1314, ...)
From a shell:
    python3 -I tools/game_paths.py exe [BUILD]     (also: dir, data, pack, root, list, verify; -v: debug lines)

The rule, for a build key (default 1308):
1. Environment variables win. Build 1308: ZAX_EXE (the exe), ZAX_GAME_DIR (the folder: its Zax.exe, DATA.dat and
   pack when the more specific variable is not set), ZAX_DATA (DATA.dat or a Data folder), ZAX_LANGPACK (the pack).
   Another key K: ZAX_EXE_<K>, ZAX_GAME_DIR_<K> (K upper case, '-' as '_', e.g. ZAX_EXE_1309_ES). Taken as given.
2. The optional, git-ignored workspace file gamedata/paths.json: {"1308": "<folder>", "1314": "<folder>", ...}, each
   folder relative to the gamedata root or absolute. Taken as given too; a Zax.exe whose SHA-1 is not the key's gets
   a warning on stderr.
3. A scan: every folder one or two levels below the gamedata root (e.g. "<edition>/", "Demos/<demo>/") that holds a
   Zax.exe (any case) whose SHA-1 is the key's (BUILDS). A key with a reference data set (1308: DATA.dat of the
   German edition, size and SHA-256 below) keeps only the folders whose DATA.dat has that size, so the Russian
   edition's folder (the same exe, its own DATA.dat) is not taken for the reference. Several folders left: their
   DATA.dat (size, then SHA-256) and packs (names and sizes) are compared; all the same: the first by path is taken
   (a debug line names it); different: an error. None: an error. There is no fallback to a folder name.
The gamedata root: ZAX_GAME_DATA, else gamedata/ of this checkout (the link to the workspace's gamedata/), else, in a
git worktree, that of the main checkout. Nothing here writes anything.

The same module (same rule, same table) is in zax-decomp, zax-redone, zax-testtools (tools/game_paths.py) and
zax-formats (zaxtools/game_paths.py)."""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# The known exes: key -> (SHA-1 of Zax.exe, what it is, reference DATA.dat as (size, SHA-256) or None).
# zax-decomp docs/EXE_FINDINGS.md, "Exe variants". The one place for these hashes in this repository.
GERMAN_DATA = (718215852, '162c02978124907f6593a4aded683143eddd197cfda71ca30a0d9ee848c1d73c')
BUILDS = {
    '1308': ('a918c784209336ab0db6b0d43e4b553e3eea57cb', 'build 1308, German noCD (the reference exe; = the Russian '
             "edition's exe)", GERMAN_DATA),
    '1308-en': ('7a777491567a154c0322f4d1134ff5b1ab9e9779', 'build 1308, English CD (SecuROM)', None),
    '1308-en-nocd': ('5c0d4eda29f63bf77ea7a8512f300d00b10fb014', 'build 1308, English noCD', None),
    '1308-demo': ('b2887cae24922ac27b1858ead07b51dfecf04578', 'build 1308 without copy protection, demo 2001-08-06',
                  None),
    '1305': ('f41436975b64405077646d81efa4469c28972f2d', 'build 1305, German CD (SecuROM)', None),
    '1309': ('a2b2bf17285cc9e515d4e42811e9cd70e60c2ff1', 'build 1309, Polish (no copy protection)', None),
    '1309-es': ('f268d7a7de130799c3586838b022fee1b72ba3c8', 'build 1309, Spanish CD (SecuROM)', None),
    '1309-fr': ('6cf0554d37fcc2a1d709e17b49fd9a48f3fb5d84', 'build 1309, French CD (SecuROM)', None),
    '1314': ('09a10b67211b46222519c850dfb6fd1e662f894a', 'build 1314, Chinese Simplified', None),
    'demo-0719': ('ca98a54cacde9496930f30cf00f37f0b8fa6c259', 'build 950, demo 2001-07-19', None),
    'demo-0701': ('deb4bf00231dade7b03ab3234b78b3a508b1d398', 'build 890, demo 2001-07-01', None),
}
REFERENCE = '1308'
PATHS_FILE = 'paths.json'
DEBUG = bool(os.environ.get('ZAX_GAME_PATHS_DEBUG'))
_cache: dict = {}


class GamePathError(RuntimeError):
    """A game file that cannot be found (or not told apart); the message says what was looked for and how to set it."""


def _debug(msg: str) -> None:
    if DEBUG:
        print(f'game_paths: {msg}', file=sys.stderr)


def _key(build) -> str:
    key = str(build)
    if key not in BUILDS:
        raise GamePathError(f'unknown build {key!r}; known: {", ".join(BUILDS)}')
    return key


def _env_suffix(key: str) -> str:
    return '' if key == REFERENCE else '_' + key.upper().replace('-', '_')


def _find_ci(folder: Path, name: str) -> Path | None:
    """The entry of folder named name in any case (the game's file system is case-insensitive)."""
    try:
        for p in sorted(folder.iterdir()):
            if p.name.lower() == name.lower() and p.is_file():
                return p
    except OSError:
        pass
    return None


def _sha1(path: Path) -> str:
    k = ('sha1', str(path))
    if k not in _cache:
        _cache[k] = hashlib.sha1(path.read_bytes()).hexdigest()
    return _cache[k]


def _sha256(path: Path) -> str:
    k = ('sha256', str(path))
    if k not in _cache:
        h = hashlib.sha256()
        with open(path, 'rb') as f:
            for block in iter(lambda: f.read(1 << 22), b''):
                h.update(block)
        _cache[k] = h.hexdigest()
    return _cache[k]


def root() -> Path:
    """The gamedata root: ZAX_GAME_DATA, else gamedata/ of this checkout, else that of the main checkout."""
    if os.environ.get('ZAX_GAME_DATA'):
        return Path(os.environ['ZAX_GAME_DATA'])
    own = ROOT / 'gamedata'
    if own.is_dir():
        return own
    try:
        common = subprocess.run(['git', '-C', str(ROOT), 'rev-parse', '--path-format=absolute', '--git-common-dir'],
                                capture_output=True, text=True).stdout.strip()
    except OSError:
        common = ''
    if common and (Path(common).parent / 'gamedata').is_dir():
        return Path(common).parent / 'gamedata'
    return own


def lang_packs(folder) -> list[Path]:
    """The language packs of a game folder in the order the game mounts them (every *.RED in any case, by name;
    a later pack's entries win, zax-formats docs/FORMATS.md 1)."""
    try:
        return sorted((p for p in Path(folder).iterdir() if p.suffix.lower() == '.red' and p.is_file()),
                      key=lambda p: p.name.lower())
    except OSError:
        return []


def _scan(base: Path) -> list[Path]:
    """Every folder one or two levels below base that holds a Zax.exe (any case)."""
    out = []
    try:
        level1 = sorted(p for p in base.iterdir() if p.is_dir())
    except OSError:
        return out
    for d in level1:
        if _find_ci(d, 'Zax.exe'):
            out.append(d)
        try:
            out += [d2 for d2 in sorted(d.iterdir()) if d2.is_dir() and _find_ci(d2, 'Zax.exe')]
        except OSError:
            pass
    return out


def _how_to_set(key: str) -> str:
    s = _env_suffix(key)
    return (f'set ZAX_EXE{s} / ZAX_GAME_DIR{s}, or add "{key}": "<folder>" to {root() / PATHS_FILE} '
            f'(or ZAX_GAME_DATA for another gamedata root)')


def _content(folder: Path) -> tuple:
    data = _find_ci(folder, 'DATA.dat')
    return (data.stat().st_size if data else None, tuple((p.name.lower(), p.stat().st_size) for p in lang_packs(folder)))


def game_dir(build=REFERENCE) -> Path:
    """The game folder of a build (rule in the module's text)."""
    key = _key(build)
    if ('dir', key) in _cache:
        return _cache[('dir', key)]
    sha1, what, ref_data = BUILDS[key]
    s = _env_suffix(key)
    env = os.environ.get('ZAX_GAME_DIR' + s)
    if env:
        _debug(f'{key}: game folder from ZAX_GAME_DIR{s}: {env}')
        return _cache.setdefault(('dir', key), Path(env))
    base = root()
    cfg = base / PATHS_FILE
    if cfg.is_file():
        try:
            entry = json.loads(cfg.read_text(encoding='utf-8')).get(key)
        except (OSError, ValueError, AttributeError) as e:
            raise GamePathError(f'{cfg}: not readable as a JSON object ({e})') from None
        if entry:
            folder = Path(entry) if Path(entry).is_absolute() else base / entry
            exe = _find_ci(folder, 'Zax.exe')
            if exe is None:
                raise GamePathError(f'{cfg}: "{key}" names {folder}, which holds no Zax.exe')
            if _sha1(exe) != sha1:
                print(f'game_paths: warning: {cfg} names {folder} for {key}, but its Zax.exe is not {what} '
                      f'(SHA-1 {_sha1(exe)[:8]}, expected {sha1[:8]}); taken as configured', file=sys.stderr)
            _debug(f'{key}: game folder from {cfg}: {folder}')
            return _cache.setdefault(('dir', key), folder)
    scanned = _scan(base)
    found = [d for d in scanned if _sha1(_find_ci(d, 'Zax.exe')) == sha1]
    looked = f'a Zax.exe with SHA-1 {sha1[:8]}… ({what})'
    if ref_data:
        size, sha256 = ref_data
        with_exe = found
        found = [d for d in found if (dd := _find_ci(d, 'DATA.dat')) and dd.stat().st_size == size]
        looked += f' next to the reference DATA.dat (size {size:,}, SHA-256 {sha256[:16]}…)'
        if with_exe and not found:
            looked += f'; the exe alone is in: {", ".join(str(d) for d in with_exe)}'
    if not found:
        raise GamePathError(f'no game folder of {key} below {base} ({len(scanned)} folders with a Zax.exe scanned); '
                            f'looked for {looked}. {_how_to_set(key)}')
    if len(found) > 1:
        contents = {d: _content(d) for d in found}
        if len(set(contents.values())) == 1 and _find_ci(found[0], 'DATA.dat'):
            hashes = {d: _sha256(_find_ci(d, 'DATA.dat')) for d in found}
            if ref_data and any(h != ref_data[1] for h in hashes.values()):
                bad = [str(d) for d, h in hashes.items() if h != ref_data[1]]
                raise GamePathError(f'{key}: DATA.dat of the right size but another SHA-256 in {", ".join(bad)}. '
                                    f'{_how_to_set(key)}')
            if len(set(hashes.values())) != 1:
                contents = hashes
        if len(set(contents.values())) != 1:
            raise GamePathError(f'{key}: several game folders with {looked}, and they differ (DATA.dat or language '
                                f'packs): {", ".join(str(d) for d in found)}. {_how_to_set(key)}')
        _debug(f'{key}: {len(found)} identical game folders, taking the first: '
               f'{", ".join(str(d) for d in found)}')
    _debug(f'{key}: game folder {found[0]}')
    return _cache.setdefault(('dir', key), found[0])


def exe(build=REFERENCE) -> Path:
    """The Zax.exe of a build: ZAX_EXE (ZAX_EXE_<K>), else that of game_dir(build)."""
    key = _key(build)
    s = _env_suffix(key)
    if os.environ.get('ZAX_EXE' + s):
        _debug(f'{key}: exe from ZAX_EXE{s}')
        return Path(os.environ['ZAX_EXE' + s])
    folder = game_dir(key)
    path = _find_ci(folder, 'Zax.exe')
    if path is None:
        raise GamePathError(f'{key}: no Zax.exe in {folder}. {_how_to_set(key)}')
    return path


exe_for = exe


def exe_arg(build=REFERENCE) -> str:
    """exe(build) as a string for a tool's default --exe; ends the program with the message if it is not found."""
    try:
        return str(exe(build))
    except GamePathError as e:
        sys.exit(f'game_paths: {e}')


def data_dat(build=REFERENCE) -> Path:
    """DATA.dat of a build's game folder; for 1308 ZAX_DATA first (DATA.dat or a Data folder)."""
    key = _key(build)
    if key == REFERENCE and os.environ.get('ZAX_DATA'):
        _debug('data from ZAX_DATA')
        return Path(os.environ['ZAX_DATA'])
    folder = game_dir(key)
    path = _find_ci(folder, 'DATA.dat')
    if path is None:
        raise GamePathError(f'{key}: no DATA.dat in {folder}' + (' (set ZAX_DATA)' if key == REFERENCE else ''))
    return path


def lang_pack(build=REFERENCE) -> Path | None:
    """The language pack of a build's game folder (the last *.RED in mount order, the one whose entries win), or
    None; for 1308 ZAX_LANGPACK first."""
    key = _key(build)
    if key == REFERENCE and os.environ.get('ZAX_LANGPACK'):
        _debug('pack from ZAX_LANGPACK')
        return Path(os.environ['ZAX_LANGPACK'])
    packs = lang_packs(game_dir(key))
    if len(packs) > 1:
        _debug(f'{key}: {len(packs)} packs, the last in mount order wins: {", ".join(p.name for p in packs)}')
    return packs[-1] if packs else None


def _verify(key: str) -> list[str]:
    sha1, what, ref_data = BUILDS[key]
    lines = []
    e = exe(key)
    lines.append(f'exe   {e}  SHA-1 {_sha1(e)[:16]}… ' + ('ok' if _sha1(e) == sha1 else f'NOT {what}'))
    try:
        d = data_dat(key)
        if d.is_file():
            h = _sha256(d)
            ok = '' if not ref_data else (' ok' if h == ref_data[1] else ' NOT the reference data')
            lines.append(f'data  {d}  SHA-256 {h[:16]}…{ok}')
    except GamePathError as err:
        lines.append(f'data  {err}')
    p = lang_pack(key)
    lines.append(f'pack  {p}' if p else 'pack  (none)')
    return lines


def main(argv: list[str]) -> int:
    global DEBUG
    args = [a for a in argv if a not in ('-v', '--verbose')]
    if len(args) != len(argv):
        DEBUG = True
    if not args or args[0] in ('-h', '--help') or args[0] not in ('exe', 'dir', 'data', 'pack', 'root', 'list', 'verify'):
        print(__doc__)
        return 0 if args and args[0] in ('-h', '--help') else 2
    what, build = args[0], (args[1] if len(args) > 1 else REFERENCE)
    try:
        if what == 'root':
            print(root())
        elif what == 'list':
            base = root()
            by_sha = {v[0]: k for k, v in BUILDS.items()}
            for d in _scan(base):
                h = _sha1(_find_ci(d, 'Zax.exe'))
                packs = ', '.join(p.name for p in lang_packs(d)) or '-'
                print(f'{by_sha.get(h, "?"):13} {h[:8]}  {d.relative_to(base)}  (packs: {packs})')
        elif what == 'verify':
            print('\n'.join(_verify(_key(build))))
        elif what == 'pack':
            p = lang_pack(build)
            if p is None:
                print(f'game_paths: {build}: no language pack in {game_dir(build)}', file=sys.stderr)
                return 1
            print(p)
        else:
            print({'exe': exe, 'dir': game_dir, 'data': data_dat}[what](build))
    except GamePathError as e:
        print(f'game_paths: {e}', file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
