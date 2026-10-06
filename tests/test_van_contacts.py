#!/usr/bin/env python3
"""Finite F2 conversion preserves geometry and rejects accidental overwrites."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
TOOL = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else ROOT/'build/toolchains/clang/bin/physical_route_tool'

class Contacts(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location('builder', ROOT/'tools/build_van_contacts.py')
        self.builder = importlib.util.module_from_spec(spec); spec.loader.exec_module(self.builder)
        self.source = ROOT/'config/examples/van_construction_f1.layout.json'

    def test_native_conversion_keeps_every_physical_object_and_other_system(self):
        with tempfile.TemporaryDirectory(prefix='ld_f2_') as temp:
            output = Path(temp)/'draft.json'; raw = self.source.read_bytes()
            report = self.builder.build(self.source, output, TOOL)
            self.assertEqual(self.source.read_bytes(), raw)
            old, new = json.loads(raw), json.loads(output.read_bytes())
            self.assertEqual(old['objects3d'], new['objects3d'])
            for key in old:
                if key not in ['engineering', 'file']: self.assertEqual(old[key], new[key])
            for key in old['engineering']:
                if key != 'furnitureUnits': self.assertEqual(old['engineering'][key], new['engineering'][key])
            self.assertEqual(new['file']['schemaVersion'], 23)
            self.assertEqual(len(new['engineering']['furnitureUnits']), 3)
            self.assertEqual(len(new['engineering']['furnitureContacts']), 2)
            self.assertFalse(report['geometry_changed'])
            run = subprocess.run([str(TOOL), 'inspect', str(output)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertEqual(json.loads(run.stdout)['count'], 22)
            before = output.read_bytes()
            with self.assertRaises(ValueError): self.builder.build(self.source, output, TOOL)
            self.assertEqual(output.read_bytes(), before)
            with self.assertRaises(ValueError): self.builder.build(output, Path(temp)/'double.json', TOOL)
            self.assertFalse((Path(temp)/'double.json').exists())

    def test_refuses_moved_or_inconsistent_geometry_without_publishing(self):
        with tempfile.TemporaryDirectory(prefix='ld_f2_') as temp:
            scene = json.loads(self.source.read_bytes())
            assembly = next(a for a in scene['engineering']['assemblies'] if a['id'] == 'storage_passenger_unit')
            assembly['worldFrame'][0] = .1
            source, output = Path(temp)/'input.json', Path(temp)/'draft.json'
            source.write_text(json.dumps(scene))
            with self.assertRaises(ValueError): self.builder.build(source, output, TOOL)
            self.assertFalse(output.exists())

if __name__ == '__main__': unittest.main()
