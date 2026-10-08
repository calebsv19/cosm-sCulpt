"""Native material edits, round-trip preservation and normal compiled scene handoff."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

material, camera = map(lambda p: str(Path(p).resolve()), sys.argv[1:3])
repo = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='surface-material-') as tmp:
    root = Path(tmp)
    doc = json.loads((repo/'config/examples/van_construction_f4.layout.json').read_text())
    c = doc['sceneAuthoring']['cameras'][0]
    c['pathId'] = 'test_path'
    c['orientationMode'] = 'per_point'
    path = dict(pathId='test_path', label='Material test', role='camera', curveType='linear',
                boundCameraId=c['cameraId'], boundLightId='', closed=False, playbackMode='once',
                durationSeconds=1, normalizedDistance=0, playing=False,
                controlPoints=[dict(x=0,y=0,z=1.6),dict(x=1,y=0,z=1.6)], tangentModes=[],
                cameraPoses=dict(version=1,next_id=2,orientation='quaternion_wxyz_x_forward_z_up',
                    keys=[dict(id='a',rotation=[1,0,0,0],fov=65,seconds=1),
                          dict(id='b',rotation=[1,0,0,0],fov=65,seconds=1)]))
    doc['sceneAuthoring']['paths']=[path]
    ids=['bed_deck','oem_floor','bed_rail_driver','bed_mattress','driver_cushion','sink','water_tank']
    presets=['wood','painted_metal','brushed_metal','fabric','foam','rubber','plastic']
    src=root/'source.json';src.write_text(json.dumps(doc))
    digest=hashlib.sha256(src.read_bytes()).digest()
    cmd=root/'command.json'
    def apply(rows,out,source=src):
        cmd.write_text(json.dumps(dict(schema='line_drawing_surface_materials_v1',assignments=rows)))
        return subprocess.run([material,'apply',str(source),str(cmd),str(out)],capture_output=True,text=True)
    rows=[dict(object_id=i,preset=p) for i,p in zip(ids,presets)]
    out=root/'finished.json';result=apply(rows,out)
    assert result.returncode==0,result.stderr
    finished=json.loads(out.read_text())
    entities={e['id']:e for e in finished['engineering']['entities']}
    for i,p in zip(ids,presets):assert entities[i]['surfaceMaterial']==p
    # Compare through native serialization, so geometry is not confused with formatting/default normalization.
    baseline=root/'baseline.json';assert apply([dict(object_id='bed_deck',preset='')],baseline).returncode==0
    without=copy.deepcopy(finished)
    for e in without['engineering']['entities']:e.pop('surfaceMaterial',None)
    assert without==json.loads(baseline.read_text()),'non-material scene data changed'
    def export(source,dest):
        r=subprocess.run([camera,str(source),'test_path',str(dest),'surface_test','1','320','180'],capture_output=True,text=True)
        assert r.returncode==0,r.stderr
        return json.loads((dest/'scene_authoring.json').read_text()),json.loads((dest/'scene_runtime.json').read_text())
    authoring,runtime=export(out,root/'export')
    objects={o['object_id']:o for o in authoring['objects']}
    mats={m['material_id']:m for m in authoring['materials']}
    bindings={b['object_id']:b for b in authoring['material_bindings'] if b['target_kind']=='object'}
    dependencies=json.loads((root/'export'/'scene_dependencies.json').read_text())
    for dependency in dependencies['dependencies']:
        payload=root/'export'/dependency['path']
        assert payload.is_file() and len(payload.read_bytes())==dependency['bytes']
        assert hashlib.sha256(payload.read_bytes()).hexdigest()==dependency['sha256']
        consumers=[o for o in authoring['objects'] if o['geometry_ref']['kind']=='mesh_asset' and o['geometry_ref']['id']==dependency['identity']]
        for o in consumers:assert o['extensions']['line_drawing']['runtime_mesh_path']==dependency['path']
    assert len(dependencies['dependencies'])>=3
    assert (root/'export'/'scene_export_receipt.json').is_file()
    native=authoring['extensions']['ray_tracing']['authoring']['object_materials']
    assert native==runtime['extensions']['ray_tracing']['authoring']['object_materials']
    for i,p in zip(ids,presets):
        mid='ld_surface_'+p
        assert objects[i]['material_ref']['id']==bindings[i]['material_id']==mid
        assert mats[mid]['rgba'][3]==1 and 0<=mats[mid]['roughness']<=1
        entry=next(n for n in native if n['object_id']==i)
        assert 0<=entry['reflectivity']<=1 and 0<=entry['roughness']<=1
    wood=next(n for n in native if n['object_id']=='bed_deck')
    assert wood['material_texture_stack']['layers'][0]['kind']=='wood'
    metal=next(n for n in native if n['object_id']=='bed_rail_driver')
    assert metal['material_texture_stack']['layers'][0]['kind']=='brushed_metal'
    assert not any(i in objects for i in ['walkway','chair_space_driver','upper_halo_driver'])
    again=root/'again.json';assert apply(rows,again,out).returncode==0
    a2,r2=export(again,root/'export2');assert a2==authoring and r2==runtime
    reset=root/'reset.json';assert apply([dict(object_id=i,preset='') for i in ids],reset,out).returncode==0
    assert json.loads(reset.read_text())==json.loads(baseline.read_text())
    for invalid in [[dict(object_id='bed_deck',preset='typo')],
                    [dict(object_id='missing',preset='wood')],
                    [dict(object_id='walkway',preset='wood')],
                    [rows[0],rows[0]], [rows[0],dict(object_id='missing',preset='wood')]]:
        bad=root/'bad.json';assert apply(invalid,bad).returncode!=0;assert not bad.exists()
    before=out.read_bytes();assert apply(rows,out).returncode!=0;assert out.read_bytes()==before
    assert hashlib.sha256(src.read_bytes()).digest()==digest
print('Surface finishes: all presets, native round-trip, compiled bindings/textures, guide omission, preservation and refusal checks passed')
