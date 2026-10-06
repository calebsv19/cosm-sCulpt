#!/usr/bin/env python3
"""Acceptance checks for the finite full-van draft and safe draft generator."""
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

class Construction(unittest.TestCase):
    def setUp(self):
        self.before=json.loads((ROOT/'config/examples/van_connected_sections_s5.layout.json').read_text())
        self.after=json.loads((ROOT/'config/examples/van_construction_f1.layout.json').read_text())
        self.objects={o['persistentId']:o for o in self.after['objects3d']}

    def bound(self, oid, axis, upper):
        o=self.objects[oid]
        dim=o['rectPrism'][['width','height','depth']['xyz'.index(axis)]]
        return o['transform']['position'][axis]+(1 if upper else -1)*dim/2

    def test_fit_contacts_and_protected_state(self):
        a,b=self.before,self.after
        self.assertEqual([o['persistentId'] for o in a['objects3d']], [o['persistentId'] for o in b['objects3d']])
        changed=set(json.loads((ROOT/'config/examples/van_construction_f1.review.json').read_text())['changed_ids'])
        for o in a['objects3d']:
            if o['persistentId'] not in changed: self.assertEqual(o,self.objects[o['persistentId']])
        for key in a['engineering']:
            if key not in ['entities','assemblies','furnitureUnits']: self.assertEqual(a['engineering'][key],b['engineering'][key],key)
        for key in a:
            if key not in ['objects3d','engineering','file']: self.assertEqual(a[key],b[key],key)
        for side, lower in [('driver','driver_bench'),('passenger','kitchen_base')]:
            storage='storage_'+side
            self.assertAlmostEqual(self.bound(storage,'z',False),0)
            self.assertAlmostEqual(self.bound(storage,'z',True),1.91)
            self.assertAlmostEqual(self.bound(storage,'y',True),self.bound(lower,'y',False))
            self.assertAlmostEqual(self.bound(storage,'y',True),self.bound('upper_cabinet_'+side,'y',False))
        self.assertAlmostEqual(self.bound('kitchen_worktop','y',False),self.bound('storage_passenger','y',True))
        self.assertEqual(b['engineering']['furnitureUnits'][0]['assembly'],'kitchen_base_unit')
        self.assertEqual(len(b['engineering']['furnitureUnits']),1)
        self.assertEqual(b['file']['schemaVersion'],22)
        run=subprocess.run([str(TOOL),'inspect',str(ROOT/'config/examples/van_construction_f1.layout.json')],capture_output=True,text=True)
        self.assertEqual(run.returncode,0,run.stderr)
        self.assertEqual(json.loads(run.stdout)['count'],22)

    def test_generator_preserves_source_and_refuses_overwrite(self):
        spec=importlib.util.spec_from_file_location('builder',ROOT/'tools/build_van_construction.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory(prefix='ld_furniture_') as temp:
            src=Path(temp)/'source.json';src.write_text(json.dumps(self.before));original=src.read_bytes()
            out=Path(temp)/'draft.json';review=module.build(src,out,TOOL)
            self.assertEqual(src.read_bytes(),original)
            self.assertEqual(review['native_validation']['route_count'],22)
            saved=out.read_bytes()
            with self.assertRaises(ValueError): module.build(src,out,TOOL)
            self.assertEqual(out.read_bytes(),saved)
            self.before['objects3d'][0]['flags']['locked']=True
            # A locked reviewed part must fail without publishing a partial draft.
            part=next(o for o in self.before['objects3d'] if o['persistentId']=='kitchen_base')
            part['flags']['locked']=True;src.write_text(json.dumps(self.before))
            refused=Path(temp)/'refused.json'
            with self.assertRaises(ValueError): module.build(src,refused,TOOL)
            self.assertFalse(refused.exists())

if __name__=='__main__': unittest.main()
