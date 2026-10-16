"""Reuse full-capacity inspection pairs; regenerate native poses and refuse moving paths."""
import copy, hashlib, json, math, subprocess, sys
from pathlib import Path
exe, exporter, source, root = [Path(p).resolve() for p in sys.argv[1:5]]
root.mkdir(parents=True, exist_ok=False)
original = source.read_bytes(); doc = json.loads(original)
command = root/'command.json'
def run(src, ops, out, ok=True):
    command.write_text(json.dumps(dict(schema='line_drawing_layout_edit_v1', operations=ops)))
    r = subprocess.run([str(exe),'edit',str(src),str(command),str(out)],capture_output=True,text=True)
    assert (r.returncode == 0) == ok, r.stderr
    if not ok: assert not out.exists()
    return r
noop = next(dict(operation='object',object_id=o['persistentId'],visible=o['flags']['visible']) for o in doc['objects3d'] if not o['flags']['locked'])
baseline = root/'baseline.json';run(source,[noop],baseline)
base=json.loads(baseline.read_text())
ops=[dict(operation='update_inspection_camera',camera_id='camera_006',path_id='path_camera_006',name='Exterior door review',eye_m=[2.5,1.42,1.3],target_m=[.55,1.05,.8],fov_deg=65),dict(operation='update_inspection_camera',camera_id='camera_007',path_id='path_camera_007',name='Sink closeup review',eye_m=[1.65,1.4,1.2],target_m=[.65,.87,.75],fov_deg=55)]
out=root/'updated.layout.json';result=json.loads(run(source,ops,out).stdout);after=json.loads(out.read_text())
assert result==[dict(camera_id=o['camera_id'],path_id=o['path_id']) for o in ops]
assert len(after['sceneAuthoring']['paths'])==8 and len(after['sceneAuthoring']['cameras'])==7
for key in base:
    if key!='sceneAuthoring': assert after[key]==base[key],key
for key in base['sceneAuthoring']:
    if key not in ['cameras','paths']: assert after['sceneAuthoring'][key]==base['sceneAuthoring'][key],key
for kind,idkey,opkey in [('paths','pathId','path_id'),('cameras','cameraId','camera_id')]:
    ids={o[opkey] for o in ops}
    assert [x[idkey] for x in after['sceneAuthoring'][kind]]==[x[idkey] for x in base['sceneAuthoring'][kind]]
    assert [x for x in after['sceneAuthoring'][kind] if x[idkey] not in ids]==[x for x in base['sceneAuthoring'][kind] if x[idkey] not in ids]
proof=[]
for i,op in enumerate(ops):
    path=next(p for p in after['sceneAuthoring']['paths'] if p['pathId']==op['path_id'])
    old=next(p for p in base['sceneAuthoring']['paths'] if p['pathId']==op['path_id'])
    assert path['cameraPoses']['keys']!=old['cameraPoses']['keys']
    export=root/f'export_{i}'
    r=subprocess.run([str(exporter),str(out),op['path_id'],str(export),'camera_update_proof','2','640','360'],capture_output=True,text=True)
    assert r.returncode==0,r.stderr
    samples=json.loads((export/'camera_samples.json').read_text())
    direction=[b-a for a,b in zip(op['eye_m'],op['target_m'])];length=math.sqrt(sum(v*v for v in direction));direction=[v/length for v in direction]
    for frame in samples['anchors']+samples['frames']:
        assert frame['vertical_fov_degrees']==op['fov_deg']
        assert max(abs(frame['position'][k]-v) for k,v in zip('xyz',op['eye_m']))<2e-6
        assert max(abs(frame['forward'][k]-v) for k,v in zip('xyz',direction))<3e-6
    proof.append(dict(camera_id=op['camera_id'],path_id=op['path_id'],anchors=len(samples['anchors']),frames=len(samples['frames']),fov_deg=op['fov_deg'],pose_verified=True))
# A second update must replace the keys again, while omitted name preserves labels.
again=root/'again.json';repeat=dict(ops[0],eye_m=[2,1.5,1.4],fov_deg=70);repeat.pop('name')
run(out,[repeat],again);a=json.loads(again.read_text())
assert a['sceneAuthoring']['cameras'][4]['label']==ops[0]['name']
assert all(k['fov']==70 for k in a['sceneAuthoring']['paths'][5]['cameraPoses']['keys'])
roundtrip=root/'roundtrip.json';run(out,[noop],roundtrip);assert json.loads(roundtrip.read_text())==after
bad=[dict(ops[0],camera_id='missing'),dict(ops[0],path_id='missing'),dict(ops[0],camera_id='camera_007'),dict(ops[0],camera_id='view_camera_1',path_id='view_path_1'),dict(ops[0],eye_m=ops[0]['target_m']),dict(ops[0],fov_deg=151),dict(ops[0],name='')]
for i,op in enumerate(bad):run(source,[op],root/f'bad_{i}.json',False)
for i,mutation in enumerate([dict(closed=True),dict(controlPoints=[dict(x=0,y=0,z=0),dict(x=1,y=0,z=0)])]):
    changed=copy.deepcopy(doc);p=next(p for p in changed['sceneAuthoring']['paths'] if p['pathId']=='path_camera_006');p.update(mutation)
    src=root/f'mutated_{i}.json';src.write_text(json.dumps(changed));run(src,[ops[0]],root/f'mutated_out_{i}.json',False)
run(source,[ops[0],bad[0]],root/'late_failure.json',False)
run(source,[dict(operation='add_inspection_camera',name='Overflow',eye_m=[2,1,1],target_m=[0,0,0],fov_deg=60)],root/'overflow.json',False)
prior=out.read_bytes();r=subprocess.run([str(exe),'edit',str(source),str(command),str(out)],capture_output=True,text=True);assert r.returncode!=0 and out.read_bytes()==prior
assert source.read_bytes()==original
(root/'verification.json').write_text(json.dumps(dict(source_sha256=hashlib.sha256(original).hexdigest(),non_camera_native_state_unchanged=True,other_paths_and_cameras_unchanged=True,persistent_ids_preserved=True,proof=proof,refusals=len(bad)+5),indent=2))
(root/'example.json').write_text(json.dumps(dict(schema='line_drawing_layout_edit_v1',operations=ops),indent=2))
print('Camera reuse, regenerated pose/FOV, full capacity, native round-trip, state preservation and atomic refusal checks passed')
