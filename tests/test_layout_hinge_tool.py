"""Typed edge hinge: down-stow sign, native round-trip, coverage and atomic refusals."""
import hashlib,json,math,subprocess,sys
from pathlib import Path
exe,source,root=map(lambda p:Path(p).resolve(),sys.argv[1:4])
root.mkdir(parents=True,exist_ok=False)
original=source.read_bytes();doc=json.loads(original)
command=root/'command.json'
def run(src,ops,out,ok=True):
    command.write_text(json.dumps(dict(schema='line_drawing_layout_edit_v1',operations=ops),indent=2))
    r=subprocess.run([str(exe),'edit',str(src),str(command),str(out)],capture_output=True,text=True)
    assert (r.returncode==0)==ok,r.stderr
    return r
hinge=dict(operation='create_hinge',rule_id='worktop_hinge',fixed=dict(entity_id='obj3d_240',feature='axis_v',offset_m=[0,.009,.012]),moving=dict(entity_id='obj3d_239',feature='axis_v',offset_m=[0,-.2286,0]),axis=[-1,0,0],min_deg=0,max_deg=90,position_deg=0)
create=root/'created.layout.json'
r=run(source,[hinge,dict(operation='generate_motion_envelope',rule_id='worktop_hinge',samples=33)],create)
result=json.loads(r.stdout);envelope_id=result[1]['object_id'];created=json.loads(create.read_text())
assert result[0]['home_deg']==0 and result[1]['current']
assert created['sceneAuthoring']==doc['sceneAuthoring']
for key in ['assemblies','furnitureUnits','furnitureContacts','relationships','routes','savedViews']:
    assert created['engineering'][key]==doc['engineering'][key],key
assert created['geometricConstraints'][:-1]==doc['geometricConstraints']
objects={o['persistentId']:o for o in created['objects3d']}
def vec(o):return [o[k] for k in 'xyz']
def corners(o):
    p=o['rectPrism'];f=p['frame'];origin=vec(f['origin']);axes=[vec(f[k]) for k in ['axisU','axisV','normal']]
    dims=[p[k] for k in ['width','height','depth']]
    return [[origin[j]+sum(axes[k][j]*dims[k]*sign[k]/2 for k in range(3)) for j in range(3)] for sign in __import__('itertools').product([-1,1],repeat=3)]
box=corners(objects[envelope_id]);lo=[min(p[k] for p in box) for k in range(3)];hi=[max(p[k] for p in box) for k in range(3)]
fixed=objects['obj3d_240']['rectPrism']['frame'];pivot=[fixed['origin'][k]+v for k,v in zip('xyz',[0,.009,.012])]
poses=[]
for angle in [0,15,30,45,60,75,90]:
    out=root/f'pose_{angle}.layout.json';run(create,[dict(operation='set_travel_position',rule_id='worktop_hinge',position_deg=angle)],out)
    d=json.loads(out.read_text());o=next(o for o in d['objects3d'] if o['persistentId']=='obj3d_239');f=o['rectPrism']['frame'];center=vec(f['origin']);v=vec(f['axisV'])
    edge=[center[k]-.2286*v[k] for k in range(3)]
    assert max(abs(x-y) for x,y in zip(edge,pivot))<2e-6,(angle,edge,pivot)
    rad=math.radians(angle);expected=[pivot[0],pivot[1]+.2286*math.cos(rad),pivot[2]-.2286*math.sin(rad)]
    assert max(abs(x-y) for x,y in zip(center,expected))<2e-6,(angle,center,expected)
    for p in corners(o):assert all(lo[k]-2e-6<=p[k]<=hi[k]+2e-6 for k in range(3)),(angle,p,lo,hi)
    assert d['engineering']['motionEnvelopes']==created['engineering']['motionEnvelopes']
    poses.append(dict(degrees=angle,center_m=center,pivot_m=edge))
    if angle==90:
        reloaded=root/'reloaded.layout.json';run(out,[dict(operation='set_travel_position',rule_id='worktop_hinge',position_deg=90)],reloaded)
        assert json.loads(reloaded.read_text())==d
        refreshed=root/'refreshed.layout.json';report=run(out,[dict(operation='generate_motion_envelope',rule_id='worktop_hinge',samples=33)],refreshed)
        assert json.loads(report.stdout)[0]['current'] and json.loads(refreshed.read_text())==d
bad_ops=[dict(hinge,rule_id='bed_lift'),dict(hinge,axis=[0,0,0]),dict(hinge,min_deg=-180),dict(hinge,max_deg=181),dict(hinge,position_deg=91),dict(hinge,motion_assembly='missing'),dict(hinge,moving=dict(entity_id='missing',feature='axis_v'))]
for i,bad in enumerate(bad_ops):
    out=root/f'bad_{i}.layout.json';run(source,[bad],out,False);assert not out.exists()
for i,bad in enumerate([dict(operation='set_travel_position',rule_id='worktop_hinge',position_deg=91),dict(operation='set_travel_position',rule_id='worktop_hinge',position_m=.5),dict(operation='generate_motion_envelope',rule_id='worktop_hinge',samples=130)]):
    out=root/f'bad_move_{i}.layout.json';run(create,[dict(operation='set_travel_position',rule_id='worktop_hinge',position_deg=45),bad],out,False);assert not out.exists()
before=create.read_bytes();run(source,[hinge],create,False);assert create.read_bytes()==before
assert source.read_bytes()==original
proof=dict(ok=True,source_sha256=hashlib.sha256(original).hexdigest(),deployed_deg=0,down_stowed_deg=90,axis=[-1,0,0],pivot_m=pivot,poses=poses,envelope_samples=33,envelope_current_after_pose_changes=True,conservative_bounds_contain_tested_corners=True,native_roundtrip=True,refusals=True,other_native_data_preserved=True,original_bytes_unchanged=True)
(root/'verification.json').write_text(json.dumps(proof,indent=2)+'\n')
(root/'create_hinge_example.json').write_text(json.dumps(dict(schema='line_drawing_layout_edit_v1',operations=[hinge,dict(operation='generate_motion_envelope',rule_id='worktop_hinge',samples=33)]),indent=2)+'\n')
print(json.dumps(proof,indent=2))
