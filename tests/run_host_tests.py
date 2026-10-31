"""Run host tests with writable runtime state outside the source checkout."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile


def run(binary, arguments, source):
    # Copy only test inputs, never an existing runtime or build directory.
    work = Path(tempfile.mkdtemp(prefix="ld-host-tests-"))
    try:
        for name in ("config", "tests/fixtures", "src", "include"):
            shutil.copytree(source / name, work / name, symlinks=True)
        for name in ("tmp", "home", "runtime", "logs"):
            (work / name).mkdir()
        environment = os.environ.copy()
        environment.update(
            TMPDIR=str(work / "tmp") + "/",
            HOME=str(work / "home"),
            CFFIXED_USER_HOME=str(work / "home"),
            LINE_DRAWING_RUNTIME_DIR=str(work / "runtime"),
            LINE_DRAWING_LOG_DIR=str(work / "logs"),
        )
        result = subprocess.run([str(binary), *arguments], cwd=work, env=environment)
        if result.returncode:
            print(f"Host test failure retained at {work}", file=sys.stderr)
            return result.returncode
        shutil.rmtree(work)
        return 0
    except BaseException:
        print(f"Interrupted host test inputs retained at {work}", file=sys.stderr)
        raise


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit("usage: run_host_tests.py BINARY [GROUP]")
    raise SystemExit(run(Path(sys.argv[1]).resolve(), sys.argv[2:],
                         Path(__file__).resolve().parents[1]))
