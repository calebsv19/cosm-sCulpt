"""Create-only native edits, offset mesh grouping, color export and failed-batch preservation."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

edit, export = [str(Path(p).resolve()) for p in sys.argv[1:3]]
repo=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='sculpt-layout-edit-') as tmp:
    root=Path(tmp)
    source=repo/'config/examples/van_construction_f4.layout.json'
    digest=hashlib.sha256(source.read_bytes()).digest()
    mesh=str(repo/'tests/fixtures/agent_mesh_assets/asset_imported_tetrahedron_01.runtime.json')
    blue=dict(rgb=[.015,.39,.64],roughness=.38,reflectivity=.2)
    red=dict(rgb=[.65,.025,.02],roughness=.5,reflectivity=0)
    ops=[dict(operation='object',object_id='bed_deck',visible=False),
         dict(operation='add_mesh_group',name='Two offset parts',position_m=[0,0,1],rotation_deg=[0,0,90],
              parts=[dict(name='Blue',runtime_mesh=mesh,finish=blue),dict(name='Red',runtime_mesh=mesh,finish=red)])]
    command=root/'command.json'
    def apply(operations,out,src=source):
        command.write_text(json.dumps(dict(schema='line_drawing_layout_edit_v1',operations=operations)))
        return subprocess.run([edit,'edit',str(src),str(command),str(out)],capture_output=True,text=True)
    ops.append(dict(operation='add_inspection_camera',name='Device review',eye_m=[0,-2,1],target_m=[0,0,1],fov_deg=60))
    out=root/'edited.json';r=apply(ops,out);assert r.returncode==0,r.stderr
    report=json.loads(r.stdout);group=report[1]['assembly_id'];ids=report[1]['object_ids']
    d=json.loads(out.read_text());objects={o['persistentId']:o for o in d['objects3d']}
    for id in ids:
        assert objects[id]['transform']['position']==dict(x=0,y=0,z=1),'offset bounds must not shift source origins'
    moved=root/'moved.json'
    r=apply([dict(operation='move_assembly',assembly_id=group,translation_m=[.25,0,0],rotation_deg=[0,0,0])],moved,out)
    assert r.returncode==0,r.stderr
    d=json.loads(moved.read_text());objects={o['persistentId']:o for o in d['objects3d']}
    assert all(abs(objects[id]['transform']['position']['x']-.25)<1e-6 for id in ids)
    finish={e['id']:e['surfaceFinish'] for e in d['engineering']['entities'] if 'surfaceFinish' in e}
    destination=root/'export';destination.mkdir()
    r=subprocess.run([export,str(moved),str(destination),'--profile','test_device_group','concept_not_measured'],capture_output=True,text=True)
    assert r.returncode==0,r.stderr
    author=json.loads((destination/'scene_authoring.json').read_text())
    mats={m['material_id']:m for m in author['materials']}
    for id in ids:
        rgb=finish[id]['rgb'];actual=mats['ld_surface_'+id]['rgba']
        expected=[((rgb>>16)&255)/255,((rgb>>8)&255)/255,(rgb&255)/255,1]
        assert all(abs(a-b)<1e-12 for a,b in zip(actual,expected))
    before=out.read_bytes();assert apply(ops,out).returncode!=0 and out.read_bytes()==before
    for bad in [dict(operation='object',object_id='missing',visible=False),
                dict(operation='object',object_id='bed_deck',finish=dict(blue,rgb=[2,0,0])),
                dict(operation='add_mesh_group',name='Missing',position_m=[0,0,0],rotation_deg=[0,0,0],parts=[dict(name='missing',runtime_mesh='/no/such/file',finish=blue)]),
                dict(operation='anything')]:
        failed=root/'failed.json';assert apply([ops[0],bad],failed).returncode!=0 and not failed.exists()
    assert hashlib.sha256(source.read_bytes()).digest()==digest
print('Typed native edits: offset grouping/movement, reload, colors/export, failed-batch and existing-output preservation passed')
