"""Existing full native scene grows to 128; overflow refuses atomically."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

exe,source,work=map(lambda p:Path(p).resolve(),sys.argv[1:4])
repo=Path(__file__).resolve().parents[1]
work.mkdir(parents=True,exist_ok=False)
original=source.read_bytes();doc=json.loads(original)
initial=len(doc['engineering']['assemblies']);assert initial+8<=128
mesh=str(repo/'tests/fixtures/agent_mesh_assets/asset_imported_tetrahedron_01.runtime.json')
command=work/'command.json'
def run(src,ops,out,expect=True):
    command.write_text(json.dumps(dict(schema='line_drawing_layout_edit_v1',operations=ops)))
    result=subprocess.run([str(exe),'edit',str(src),str(command),str(out)],capture_output=True,text=True)
    assert (result.returncode==0)==expect,result.stderr
    return result
no_op=next(dict(operation='object',object_id=o['persistentId'],visible=o['flags']['visible']) for o in doc['objects3d'] if not o['flags']['locked'])
baseline=work/'baseline.layout.json';run(source,[no_op],baseline)
base=json.loads(baseline.read_text())
def group(n):
    return dict(operation='add_mesh_group',name=f'Capacity proof {n}',position_m=[0,0,0],rotation_deg=[0,0,0],
                parts=[dict(name=f'Test part {n}',runtime_mesh=mesh,finish=dict(rgb=[.015,.39,.64],roughness=.38,reflectivity=.2))])
growth=work/'growth.layout.json';run(source,[group(n) for n in range(8)],growth)
p128=work/'128.layout.json';run(growth,[group(n) for n in range(8,128-initial)],p128)
reload=work/'128_reloaded.layout.json';run(p128,[no_op],reload)
for path,count in [(growth,initial+8),(p128,128),(reload,128)]:
    after=json.loads(path.read_text())
    assert len(after['engineering']['assemblies'])==count
    assert after['engineering']['assemblies'][:initial]==base['engineering']['assemblies']
    assert after['objects3d'][:len(base['objects3d'])]==base['objects3d']
    assert after['engineering']['entities'][:len(base['engineering']['entities'])]==base['engineering']['entities']
    for key in ['furnitureUnits','furnitureContacts','relationships','routes','savedViews']:
        assert after['engineering'][key]==base['engineering'][key],key
    assert after['sceneAuthoring']==base['sceneAuthoring']
    entities={e['id']:e for e in after['engineering']['entities']}
    for a in after['engineering']['assemblies'][initial:]:
        members=[e for e in entities.values() if e['parent']==a['id']]
        assert len(members)==1 and 'surfaceFinish' in members[0]
    assert len(set(a['id'] for a in after['engineering']['assemblies']))==count
assert json.loads(p128.read_text())==json.loads(reload.read_text())
before=p128.read_bytes();overflow=work/'129.layout.json';run(p128,[group(128-initial)],overflow,False)
assert not overflow.exists() and p128.read_bytes()==before
run(source,[group(0)],p128,False);assert p128.read_bytes()==before
assert source.read_bytes()==original
print(json.dumps(dict(ok=True,source_sha256=hashlib.sha256(original).hexdigest(),counts=[initial,initial+8,128],reload_128=True,
    refused_129=True,original_bytes_unchanged=True,existing_objects_assemblies_contacts_routes_cameras_preserved=True,
    colored_group_members_valid=True),indent=2))
