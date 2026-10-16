"""Native hardware migration: closed-pose preservation and atomic refusals."""
import hashlib,json,subprocess,sys
from pathlib import Path
tool,source,root=map(lambda x:Path(x).resolve(),sys.argv[1:4]);root.mkdir(exist_ok=False)
original=source.read_bytes();scene=json.loads(original)
hinge=next(c for c in scene['geometricConstraints'] if 'telescopingStay' in c)
def apply(src,ops,name,success=True):
 command=root/(name+'.json');out=root/(name+'.layout.json')
 command.write_text(json.dumps(dict(schema='line_drawing_layout_edit_v1',operations=ops)))
 r=subprocess.run([str(tool),'edit',str(src),str(command),str(out)],capture_output=True,text=True)
 assert (r.returncode==0)==success,r.stderr
 if not success:assert not out.exists()
 return out
detached=apply(source,[dict(operation='detach_telescoping_stay',rule_id=hinge['id'])],'detach')
d=json.loads(detached.read_text());c=next(c for c in d['geometricConstraints'] if c['id']==hinge['id'])
assert 'telescopingStay' not in c
assert d['objects3d']==scene['objects3d']
# Both attachment datums coincide in this fixture; moving both together changes
# the true rotation axis without shifting the closed leaf or imported members.
offset=[.006,0,0]
retargeted=apply(detached,[dict(operation='set_hinge_offsets',rule_id=c['id'],fixed_offset_m=offset,moving_offset_m=offset)],'retarget')
rd=json.loads(retargeted.read_text());assert rd['objects3d']==d['objects3d']
opened=apply(retargeted,[dict(operation='set_travel_position',rule_id=c['id'],position_deg=c['travelMax_deg'])],'open')
assert json.loads(opened.read_text())['objects3d']!=rd['objects3d']
scoped=apply(retargeted,[dict(operation='set_motion_scope',rule_id=c['id'],motion_assembly=c['motionAssembly'])],'scope')
assert json.loads(scoped.read_text())['objects3d']==rd['objects3d']
named=apply(scoped,[dict(operation='set_properties',entity_id=c['a']['entityId'],name='Retargeted fixed hardware datum',properties={'hardware_sku':'handed-fixture'})],'name')
nd=json.loads(named.read_text());assert nd['objects3d']==rd['objects3d']
assert next(e for e in nd['engineering']['entities'] if e['id']==c['a']['entityId'])['name']=='Retargeted fixed hardware datum'
for n,op in enumerate([
 dict(operation='detach_telescoping_stay',rule_id=c['id']),
 dict(operation='set_hinge_offsets',rule_id=c['id'],fixed_offset_m=[0,0],moving_offset_m=offset),
 dict(operation='set_motion_scope',rule_id=c['id'],motion_assembly='missing'),
 dict(operation='set_motion_scope',rule_id=c['id'],motion_assembly='van')]):
 apply(retargeted,[op],f'refusal_{n}',False)
apply(scoped,[dict(operation='set_properties',entity_id=c['a']['entityId'],name=17,properties={})],'invalid_name',False)
assert hashlib.sha256(source.read_bytes()).digest()==hashlib.sha256(original).digest()
print('Hardware retargeting: closed pose, articulated opening, invalid scope/input refusal and source preservation passed')
