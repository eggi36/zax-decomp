"""Where zax-formats lives (zaxtools, the format library, and its command line tools such as render_level.py and
selftest.py): ZAX_FORMATS, else the folder zax-formats next to this repository or, in a git worktree, next to its main
checkout. Importing this module puts it on sys.path, so ``from zaxtools import ...`` works in this repository's tools.
(Until 2026-11-07 zaxtools was part of zax-toolbox, found through ZAX_TOOLBOX and tools/toolbox_path.py.)"""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

_HERE = Path(__file__).resolve().parent.parent


def _find() -> Path:
    if os.environ.get('ZAX_FORMATS'):
        return Path(os.environ['ZAX_FORMATS'])
    near = _HERE.parent / 'zax-formats'
    if near.is_dir():
        return near
    common = subprocess.run(['git', '-C', str(_HERE), 'rev-parse', '--path-format=absolute', '--git-common-dir'],
                            capture_output=True, text=True).stdout.strip()
    return Path(common).parent.parent / 'zax-formats' if common else near


FORMATS = _find()
if not (FORMATS / 'zaxtools').is_dir():
    sys.exit(f'zax-formats not found at {FORMATS} (set ZAX_FORMATS or put it next to this repository)')
if str(FORMATS) not in sys.path:
    sys.path.insert(0, str(FORMATS))
# its own interpreter (.venv), else this repository's
PYTHON = FORMATS / '.venv' / 'bin' / 'python'
