#!/usr/bin/env python3
"""Run the production font/settings fault corpus with disposable preference files."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, required=True)
parser.add_argument('--output-root', type=Path, required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parents[2]
args.output_root.mkdir(parents=True, exist_ok=True)
work = Path(tempfile.mkdtemp(prefix='text-settings-', dir=args.output_root.resolve()))
try:
    for relative in ['include/fonts', 'third_party/codework_shared/assets/fonts']:
        target = work / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(source / relative, target)
    for mode in ['--settings', '--reopen']:
        subprocess.run([str(args.binary.resolve()), mode], cwd=work, check=True)
except Exception:
    print('Failed settings reproducer retained:', work)
    raise
else:
    shutil.rmtree(work)
