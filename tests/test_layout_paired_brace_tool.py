"""Real paired-mechanism round-trip, independent pins and atomic refusal fixture."""
from pathlib import Path
import json,subprocess,sys,copy,hashlib,math
binary,source,root=map(Path,sys.argv[1:]);root.mkdir(parents=True,exist_ok=False);original=source.read_bytes();scene=json.loads(original);n=0

def run(data,ops,success=True):
 global n
 n+=1;src=root/f'{n}.input.json';cmd=root/f'{n}.command.json';out=root/f'{n}.output.json';src.write_text(json.dumps(data));cmd.write_text(json.dumps({'schema':'line_drawing_layout_edit_v1','operations':ops}));p=subprocess.run([str(binary),'edit',str(src),str(cmd),str(out)],capture_output=True,text=True)
 assert (p.returncode==0)==success,p.stderr
 assert out.exists()==success
 return json.loads(out.read_text()) if success else None
rules=[c for c in scene['geometricConstraints'] if 'pairedFoldingBrace' in c];assert len(rules)>=1
for c in rules:
 id=c['id'];unlock=[dict(operation='set_brace_lock',rule_id=id,engaged=False)] if c['foldingBrace'].get('manualLock',{}).get('engaged') else []
 moved=run(scene,unlock+[dict(operation='set_travel_position',rule_id=id,position_deg=(c['travelMin_deg']+c['travelMax_deg'])/2)])
 saved=next(x for x in moved['geometricConstraints'] if x['id']==id);assert saved['pairedFoldingBrace']==c['pairedFoldingBrace'];assert moved['file']['schemaVersion']==30
 before={x['persistentId']:x for x in scene['objects3d']};after={x['persistentId']:x for x in moved['objects3d']}
 for key in ['foldingBrace','pairedFoldingBrace']:
  b=c[key];assert any(after[p]['transform']!=before[p]['transform'] for p in b['parts'][:2])
  for p in b['parts']:assert after[p]['meshAssetInstance']==before[p]['meshAssetInstance']
  pins=[after[p]['transform']['position'] for p in b['parts'][4:7]]
  axis=[c['axisX'],c['axisY'],c['axisZ']];pins=[[v[a]-axis[i]*b.get('compactMount',{}).get('pin_lateral_m',[0,0,0])[k] for i,a in enumerate('xyz')] for k,v in enumerate(pins)]
  for k,l in enumerate(b['length_m']):assert abs(math.dist(pins[k],pins[k+1])-l)<3e-6
 # Both halves belong to one hinge: duplicate, orphan and hidden secondary lock reject intact.
 for kind in ['duplicate','orphan','secondary_lock','old_schema']:
  bad=copy.deepcopy(scene);rule=next(x for x in bad['geometricConstraints'] if x['id']==id)
  if kind=='duplicate':rule['pairedFoldingBrace']['parts'][0]=rule['foldingBrace']['parts'][0]
  elif kind=='orphan':rule.pop('foldingBrace')
  elif kind=='secondary_lock':rule['pairedFoldingBrace']['manualLock']={'engaged':False,'position_deg':0}
  else:bad['file']['schemaVersion']=29
  run(bad,[dict(operation='set_travel_position',rule_id=id,position_deg=0)],False)
 # An immovable secondary link rejects the entire hinge movement.
 bad=copy.deepcopy(scene);part=c['pairedFoldingBrace']['parts'][0];next(o for o in bad['objects3d'] if o['persistentId']==part)['flags']['locked']=True
 run(bad,unlock+[dict(operation='set_travel_position',rule_id=id,position_deg=45)],False)
 if unlock:run(scene,[dict(operation='set_travel_position',rule_id=id,position_deg=45)],False)
 assert source.read_bytes()==original
print(json.dumps({'passed':True,'paired_rules':len(rules),'roundtrip_and_two_link_lengths':True,'atomic_refusals':True,'source_sha256':hashlib.sha256(original).hexdigest()}))
