"""Native mesh replacement retains references and refuses invalid batches atomically."""
import copy,hashlib,json,subprocess,sys,tempfile
from pathlib import Path
tool=str(Path(sys.argv[1]).resolve());repo=Path(__file__).resolve().parents[1]
a=repo/'tests/fixtures/agent_mesh_assets/asset_sphere_8x4.runtime.json';b=repo/'tests/fixtures/agent_mesh_assets/asset_imported_tetrahedron_01.runtime.json'
with tempfile.TemporaryDirectory(prefix='mesh-fit-test-') as tmp:
 root=Path(tmp);seq=0
 def edit(source,ops,out):
  global seq
  seq+=1;cmd=root/f'{seq}.commands.json';cmd.write_text(json.dumps({'schema':'line_drawing_layout_edit_v1','operations':ops}));return subprocess.run([tool,'edit',str(source),str(cmd),str(out)],text=True,capture_output=True)
 seed=root/'seed.json';run=edit(repo/'config/examples/van_construction_f4.layout.json',[{'operation':'add_mesh_group','name':'Identity fixture','position_m':[.3,.4,.5],'rotation_deg':[12,23,34],'parts':[{'name':'Retained part','runtime_mesh':str(a),'finish':{'rgb':[.2,.4,.6],'roughness':.5,'reflectivity':.1}}]}],seed);assert run.returncode==0,run.stderr
 id=json.loads(run.stdout)[0]['object_ids'][0];before=json.loads(seed.read_text());digest=hashlib.sha256(seed.read_bytes()).digest();out=root/'replaced.json';op={'operation':'replace_mesh_asset','object_id':id,'runtime_mesh':str(b)};run=edit(seed,[op],out);assert run.returncode==0,run.stderr
 after=json.loads(out.read_text());old=next(o for o in before['objects3d'] if o['persistentId']==id);new=next(o for o in after['objects3d'] if o['persistentId']==id);assert old['meshAssetInstance']!=new['meshAssetInstance'];old=copy.deepcopy(old);new=copy.deepcopy(new);old.pop('meshAssetInstance');new.pop('meshAssetInstance');assert old==new,'Identity, transform or flags changed'
 assert before['engineering']==after['engineering'],'Membership or material changed';assert len(before['objects3d'])==len(after['objects3d']);assert {o['persistentId'] for o in before['objects3d']}=={o['persistentId'] for o in after['objects3d']}
 created=root/'next.json';run=edit(out,[{'operation':'add_prism','name':'Next ID','position_m':[0,0,0],'dimensions_m':[.02,.03,.04]}],created);assert run.returncode==0,run.stderr;assert json.loads(created.read_text())['objects3d'][-1]['id']==max(o['id'] for o in after['objects3d'])+1,'Replacement consumed identity'
 for label,operations in [('missing_asset',[dict(op,runtime_mesh=str(root/'missing.json'))]),('relative_asset',[dict(op,runtime_mesh='relative.json')]),('unknown_identity',[dict(op,object_id='missing')]),('non_mesh',[dict(op,object_id=before['objects3d'][0]['persistentId'])]),('batch_failure',[op,dict(op,object_id='missing')])]:
  denied=root/f'{label}.json';assert edit(seed,operations,denied).returncode!=0,label;assert not denied.exists(),label
 locked=copy.deepcopy(before);next(o for o in locked['objects3d'] if o['persistentId']==id)['flags']['locked']=True;lockfile=root/'locked.json';lockfile.write_text(json.dumps(locked));denied=root/'locked-output.json';assert edit(lockfile,[op],denied).returncode!=0 and not denied.exists()
 payload=out.read_bytes();assert edit(seed,[op],out).returncode!=0 and out.read_bytes()==payload;assert hashlib.sha256(seed.read_bytes()).digest()==digest
print('PASS: stable identity, pose, membership, material, next ID and seven atomic refusal cases')
