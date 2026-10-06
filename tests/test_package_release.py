"""Exercise create-only package paths and diagnostics without signing or user data."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    'package_release_root', ROOT / 'tools/packaging/macos/package_release_root.py')
HELPER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HELPER)


class PackageReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='sculpt-package-contract-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()

    def test_relative_slot_and_collision_preserve_existing_bytes(self):
        path = HELPER.prepare_root('build/release-authenticated/rapcj_abc', source_root=self.root)
        marker = path / 'retained.txt'
        marker.write_text('keep')
        with self.assertRaisesRegex(RuntimeError, 'already exist'):
            HELPER.prepare_root('build/release-authenticated/rapcj_abc', source_root=self.root)
        self.assertEqual(marker.read_text(), 'keep')

    def test_invalid_paths_create_nothing(self):
        for value in ['', '.', 'dist/abc', 'build//release-authenticated/abc',
                      'build/release-authenticated/../abc', 'build/release-authenticated/abc/',
                      'build/release-authenticated/ab', 'build/release-authenticated/abc/unbound']:
            with self.subTest(value=value), self.assertRaises(RuntimeError):
                HELPER.prepare_root(value, source_root=self.root)
        self.assertEqual(list(self.root.iterdir()), [])

    def test_symlink_ancestor_refuses_without_touching_destination(self):
        outside = self.root / 'outside'
        outside.mkdir()
        (self.root / 'build').symlink_to(outside, target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, 'symlink'):
            HELPER.prepare_root('build/release-authenticated/abc', source_root=self.root)
        self.assertEqual(list(outside.iterdir()), [])

    def test_configured_data_slot_and_target_slot(self):
        data = self.root / 'data'
        (data / 'production_registry').mkdir(parents=True)
        with patch.dict(os.environ, CODEWORKCTL_WORKSPACE_ROOT=str(data)):
            slot = data / 'line_drawing/build/release-authenticated/rapcj_abc'
            self.assertEqual(HELPER.prepare_root(str(slot)), slot)
            target = slot / 'targets' / ('rapt_' + 'a' * 64)
            self.assertEqual(HELPER.prepare_root(str(target)), target)
            with self.assertRaisesRegex(RuntimeError, 'escapes'):
                HELPER.prepare_root(str(data / 'behavior_sim/build/release-authenticated/abc'))

    def test_absolute_slot_requires_configured_data(self):
        with patch.dict(os.environ, clear=True), self.assertRaisesRegex(RuntimeError, 'configured'):
            HELPER.prepare_root(str(self.root / 'line_drawing/build/release-authenticated/abc'))

    def test_make_rebinds_all_package_and_release_outputs(self):
        names = ['DIST_DIR', 'PACKAGE_APP_DIR', 'PACKAGE_FRAMEWORKS_DIR',
                 'RELEASE_DIR', 'RELEASE_APP_ZIP', 'RELEASE_NOTARY_ZIP',
                 'RELEASE_MANIFEST', 'RELEASE_ROUNDTRIP_RECEIPT', 'RELEASE_ARTIFACT_ZIP']
        recipe = 'package-path-test:\n\t@printf "%s\\n" ' + ' '.join(
            '"$(' + name + ')"' for name in names) + '\n'
        slot = self.root / 'data/line_drawing/build/release-authenticated/rapcj_abc'
        result = subprocess.run(['make', '-s', '--no-print-directory', '-f', 'Makefile', '-f', '-',
                                 'package-path-test', 'RELEASE_ROOT=' + str(slot),
                                 'RELEASE_VERSION=0.5.0', 'DIST_DIR=unbound-dist',
                                 'RELEASE_DIR=unbound-release'], cwd=ROOT, input=recipe,
                                text=True, capture_output=True, check=True)
        outputs = result.stdout.splitlines()
        self.assertEqual(len(outputs), len(names))
        for value in outputs:
            self.assertTrue(Path(value).is_relative_to(slot), value)
        self.assertIn('sCulpt-0.5.0-macOS-arm64-stable.zip', outputs[4])
        self.assertFalse(slot.exists())

    def test_linkage_checks_dependencies_and_ignores_inspected_file_headers(self):
        recipe = '\n'.join(line.strip()[1:] for line in
            (ROOT / 'make/release.mk').read_text().splitlines()
            if line.strip().startswith("@! awk"))
        self.assertEqual(len(recipe.splitlines()), 2)
        recipe = recipe.replace('$(RELEASE_DIR)', str(self.root))
        report = self.root / 'otool_fixture.txt'
        header = '/Users/operator/Registry/job/sCulpt.app/bin (architecture arm64):\n'
        report.write_text(header + '\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0)\n')
        result = subprocess.run(['sh', '-ec', recipe], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for value in ('/opt/homebrew/lib/bad.dylib', '/usr/local/lib/bad.dylib',
                      '/Users/operator/lib/bad.dylib', '@rpath/bad.dylib'):
            report.write_text(header + '\t' + value + ' (compatibility version 1.0.0)\n')
            with self.subTest(value=value):
                result = subprocess.run(['sh', '-ec', recipe], capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)

    def test_diagnostics_clear_inherited_resource_overrides_and_clean_up(self):
        launcher = self.root / 'launcher'
        launcher.write_text('#!/bin/sh\nset -eu\n'
                            'test -z "${VK_ICD_FILENAMES:-}"\n'
                            'test -z "${VK_DRIVER_FILES:-}"\n'
                            'test -z "${VK_RENDERER_SHADER_ROOT:-}"\n'
                            'test -z "${SHAPE_ASSET_DIR:-}"\n'
                            'mkdir -p "$LINE_DRAWING_RUNTIME_DIR" "$LINE_DRAWING_LOG_DIR"\n'
                            'echo "$LINE_DRAWING_RUNTIME_DIR"\n'
                            'echo "$LINE_DRAWING_LOG_DIR"\n')
        launcher.chmod(0o755)
        environment = dict(os.environ, TMPDIR=str(self.root), VK_ICD_FILENAMES='user-icd',
                           VK_DRIVER_FILES='user-driver', VK_RENDERER_SHADER_ROOT='user-shaders',
                           SHAPE_ASSET_DIR='user-assets', LINE_DRAWING_RUNTIME_DIR='user-runtime',
                           LINE_DRAWING_LOG_DIR='user-logs')
        result = subprocess.run(['sh', str(ROOT / 'tools/packaging/macos/package-self-test.sh'),
                                 str(launcher)], env=environment, capture_output=True,
                                text=True, check=True)
        paths = [Path(line) for line in result.stdout.splitlines()]
        self.assertEqual(len(paths), 2)
        self.assertTrue(all(path.is_relative_to(self.root) for path in paths))
        self.assertTrue(all(not path.exists() for path in paths))


if __name__ == '__main__':
    unittest.main()
