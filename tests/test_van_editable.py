#!/usr/bin/env python3
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
TOOL=Path(sys.argv.pop(1)).resolve() if len(sys.argv)>1 else ROOT/'build/toolchains/clang/bin/physical_route_tool'
class Editable(unittest.TestCase):
    def setUp(self):
        spec=importlib.util.spec_from_file_location('builder',ROOT/'tools/build_van_editable.py')
        self.builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(self.builder)
        self.source=ROOT/'config/examples/van_construction_f2.layout.json'
    def test_preserved_system_and_create_only_conversion(self):
        with tempfile.TemporaryDirectory() as temp:
            output=Path(temp)/'f4.json';raw=self.source.read_bytes();old=json.loads(raw)
            self.builder.build(self.source,output,TOOL);new=json.loads(output.read_bytes())
            self.assertEqual(self.source.read_bytes(),raw)
            self.assertEqual(len(new['engineering']['furnitureUnits']),9)
            self.assertEqual(len(new['engineering']['furnitureContacts']),5)
            for key in ('routes','relationships','constraints','savedViews','motionEnvelopes'):
                if key in old['engineering']:self.assertEqual(old['engineering'][key],new['engineering'][key])
            self.assertEqual([o['persistentId'] for o in old['objects3d']],[o['persistentId'] for o in new['objects3d']])
            changed={'sink','kitchen_base_top','kitchen_worktop'}
            for a,b in zip(old['objects3d'],new['objects3d']):
                if a['persistentId'] not in changed:self.assertEqual(a,b)
            for oid in changed:
                o=next(o for o in new['objects3d'] if o['persistentId']==oid)
                self.assertIn('opening',o['rectPrism'])
            before=output.read_bytes()
            with self.assertRaises(ValueError):self.builder.build(self.source,output,TOOL)
            self.assertEqual(output.read_bytes(),before)
            with self.assertRaises(ValueError):self.builder.build(output,Path(temp)/'double.json',TOOL)
            self.assertFalse((Path(temp)/'double.json').exists())
    def test_unreviewed_assembly_refused_without_partial_output(self):
        with tempfile.TemporaryDirectory() as temp:
            x=json.loads(self.source.read_bytes())
            next(a for a in x['engineering']['assemblies'] if a['id']=='driver_bench_unit')['worldFrame'][0]=.1
            source=Path(temp)/'input.json';source.write_text(json.dumps(x));output=Path(temp)/'f4.json'
            with self.assertRaises(ValueError):self.builder.build(source,output,TOOL)
            self.assertFalse(output.exists())
if __name__=='__main__':unittest.main()
