"""Atomic rear-retained furniture resizing preserves driven contacts and native IDs."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

exe=str(Path(sys.argv[1]).resolve())
repo=Path(__file__).resolve().parents[1]
source=Path(sys.argv[2]) if len(sys.argv)>2 else repo/'config/examples/van_construction_f4.layout.json'
root=Path(sys.argv[3]) if len(sys.argv)>3 else None
original=source.read_bytes();doc=json.loads(original)
units={u['assembly']:u for u in doc['engineering']['furnitureUnits']}
ops=[]
for id in ['driver_bench_unit','upper_cabinet_driver_unit']:
    size=units[id]['sizeMeters'].copy();size[1]+=.21700012683868408
    ops.append(dict(operation='resize_furniture',assembly_id=id,size_m=size,retain_run_end=-1))
def verify(root):
    root.mkdir(exist_ok=True)
    command=root/'retained_end_command.json';command.write_text(json.dumps(dict(schema='line_drawing_layout_edit_v1',operations=ops),indent=2))
    output=root/'rear_retained.layout.json'
    result=subprocess.run([exe,'edit',str(source),str(command),str(output)],capture_output=True,text=True)
    assert result.returncode==0,result.stderr
    updated=json.loads(output.read_text());after={u['assembly']:u for u in updated['engineering']['furnitureUnits']}
    report=[]
    for id,before in units.items():
        a=after[id]
        if id in [op['assembly_id'] for op in ops]:
            rear0=before['centerMeters'][1]-before['sizeMeters'][1]/2
            rear1=a['centerMeters'][1]-a['sizeMeters'][1]/2
            assert abs(rear1-rear0)<2e-6,(id,rear0,rear1)
            assert abs(a['sizeMeters'][1]-before['sizeMeters'][1]-.21700012683868408)<2e-6
            assert [(p['entity'],p['role']) for p in a['parts']]==[(p['entity'],p['role']) for p in before['parts']]
            report.append(dict(assembly_id=id,rear_before_m=rear0,rear_after_m=rear1,new_run_m=a['sizeMeters'][1]))
        else:
            for field in ['centerMeters','sizeMeters']:
                assert all(abs(x-y)<2e-6 for x,y in zip(a[field],before[field])),(id,field)
    for field in ['furnitureContacts','relationships','routes','savedViews']:
        assert updated['engineering'][field]==doc['engineering'][field],field
    assert updated['sceneAuthoring']==doc['sceneAuthoring']
    assert [o['persistentId'] for o in updated['objects3d']]==[o['persistentId'] for o in doc['objects3d']]
    for value in [-2,2,.5,'rear',False,None]:
        invalid=dict(ops[0],retain_run_end=value)
        command.write_text(json.dumps(dict(schema='line_drawing_layout_edit_v1',operations=[invalid])))
        bad=root/'invalid.layout.json'
        result=subprocess.run([exe,'edit',str(source),str(command),str(bad)],capture_output=True,text=True)
        assert result.returncode!=0 and not bad.exists(),value
    command.write_text(json.dumps(dict(schema='line_drawing_layout_edit_v1',operations=ops),indent=2))
    saved=output.read_bytes()
    result=subprocess.run([exe,'edit',str(source),str(command),str(output)],capture_output=True,text=True)
    assert result.returncode!=0 and output.read_bytes()==saved
    assert source.read_bytes()==original
    report=dict(ok=True,source_sha256=hashlib.sha256(original).hexdigest(),rear_retained=report,
                furniture_contacts_preserved=len(doc['engineering']['furnitureContacts']),ids_and_unrelated_units_preserved=True,
                invalid_values_refused=True,existing_output_preserved=True)
    (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
if root:verify(root)
else:
    with tempfile.TemporaryDirectory(prefix='sculpt-retained-end-') as tmp:verify(Path(tmp))
