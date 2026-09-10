"""Reject unsafe output roots before any package command executes."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

SOURCE = pathlib.Path(__file__).resolve().parents[1]

class DisposableRootTests(unittest.TestCase):
    def test_invalid_roots_leave_existing_bytes_untouched(self):
        with tempfile.TemporaryDirectory(prefix="sculpt-package-root-") as temporary:
            root = pathlib.Path(temporary)
            shutil.copytree(SOURCE / "make", root / "make")
            shutil.copyfile(SOURCE / "Makefile", root / "Makefile")
            shutil.copyfile(SOURCE / "VERSION", root / "VERSION")
            parent = root / "build/release-authenticated"
            parent.mkdir(parents=True)
            existing = parent / "existing-job"
            existing.mkdir()
            (existing / "sentinel").write_text("keep")
            (parent / "linked-job").symlink_to(existing, target_is_directory=True)
            cases = ["", "/tmp/forbidden", "../outside", "build/release",
                     "build/release-authenticated/ab", "build/release-authenticated/../escape",
                     "build/release-authenticated/existing-job", "build/release-authenticated/linked-job"]
            for value in cases:
                with self.subTest(root=value):
                    result = subprocess.run(["make", "RELEASE_ROOT=" + value,
                        "release-artifact-disposable"], cwd=root, capture_output=True, text=True)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("RELEASE_ROOT", result.stdout + result.stderr)
                    self.assertEqual((existing / "sentinel").read_text(), "keep")
                    self.assertFalse((root / "dist").exists())
            shutil.rmtree(parent)
            parent.symlink_to(root, target_is_directory=True)
            result = subprocess.run(["make", "RELEASE_ROOT=build/release-authenticated/ancestor-job",
                "release-artifact-disposable"], cwd=root, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("ancestor must not be a symlink", result.stdout + result.stderr)
            self.assertFalse((root / "ancestor-job").exists())

if __name__ == "__main__":
    unittest.main()
