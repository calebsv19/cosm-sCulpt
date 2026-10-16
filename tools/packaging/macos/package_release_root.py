"""Reserve one absent sCulpt package slot in source or configured Registry data."""
import argparse
import os
from pathlib import Path
import re

SOURCE_ROOT = Path(__file__).resolve().parents[3]


def prepare_root(value, *, source_root=SOURCE_ROOT):
    path = Path(value)
    if (not value or str(path) != value or '//' in value
            or any(part in ('', '.', '..') for part in value.strip('/').split('/'))):
        raise RuntimeError('RELEASE_ROOT has ambiguous path components')
    if path.is_absolute():
        configured = os.environ.get('CODEWORKCTL_WORKSPACE_ROOT')
        if not configured or not (Path(configured) / 'production_registry').is_dir():
            raise RuntimeError('RELEASE_ROOT requires the configured Registry data root')
        base = Path(configured).absolute() / 'line_drawing'
        try:
            relative = path.relative_to(base)
        except ValueError as error:
            raise RuntimeError('RELEASE_ROOT escapes the configured program data root') from error
    else:
        base = source_root
        relative = path
        path = base / relative
    parts = relative.parts
    target_slot = (len(parts) == 5 and parts[3] == 'targets'
                   and re.fullmatch(r'rapt_[a-f0-9]{64}', parts[4]) is not None)
    if (len(parts) not in (3, 5) or parts[:2] != ('build', 'release-authenticated')
            or re.fullmatch(r'[a-z0-9][a-z0-9_-]{2,80}', parts[2]) is None
            or (len(parts) == 5 and not target_slot)):
        raise RuntimeError('RELEASE_ROOT must be a job-scoped release-authenticated slot')
    for parent in reversed((path, *path.parents)):
        if parent.is_symlink():
            raise RuntimeError('RELEASE_ROOT ancestor must not be a symlink')
        if parent.exists() and not parent.is_dir():
            raise RuntimeError('RELEASE_ROOT ancestor must be a directory')
    if path.exists():
        raise RuntimeError('RELEASE_ROOT must not already exist')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.mkdir()
    return path


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release-root', required=True)
    print(prepare_root(parser.parse_args().release_root))
