#!/usr/bin/env python3
"""Native route CLI acceptance, create-only output and invalid-command isolation."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
TOOL=Path(sys.argv[1]).resolve()
INPUT=ROOT/'config/examples/van_layout_promaster_2023_nominal_v1.layout.json'

def run(*args,success=True):
    result=subprocess.run([str(TOOL),*map(str,args)],capture_output=True,text=True)
    assert (result.returncode==0)==success,(result.stdout,result.stderr)
    return json.loads(result.stdout if success else result.stderr)

with tempfile.TemporaryDirectory(prefix='ld-route-smoke-') as directory:
    root=Path(directory)
    before=INPUT.read_bytes()
    empty=run('inspect',INPUT)
    assert empty['count']==0
    native=json.loads(INPUT.read_text())
    objects={o['persistentId']:o for o in native['objects3d']}
    a=[objects['can_hub']['transform']['position'][k] for k in 'xyz']
    b=[objects['water_controller']['transform']['position'][k] for k in 'xyz']
    route=dict(id='',name='CAN stub',type='Cable',parent='',reference=False,volumeRole=0,volumeOwner='',properties=[],
        source=dict(entity_id='can_hub',local_offset_m=[0,0,0]),
        destination=dict(entity_id='water_controller',local_offset_m=[0,0,0]),points_m=[a,[a[0],a[1],1.88],b])
    command=dict(schema='line_drawing_route_edit_v1',operation='upsert',route=route)
    request=root/'command.json';request.write_text(json.dumps(command))
    saved=root/'saved.layout.json';created=run('edit',INPUT,request,saved)
    assert created['count']==1 and created['routes'][0]['id']=='route_1'
    assert created['routes'][0]['endpoint_status']=='current' and created['routes'][0]['length_m']>0
    assert created==run('inspect',saved)
    saved_bytes=saved.read_bytes()
    assert not run('edit',INPUT,request,saved,success=False)['ok'] and saved.read_bytes()==saved_bytes
    command['route']['points_m'][1][0]+=.025
    command['route']['id']='route_1';request.write_text(json.dumps(command))
    edited=root/'edited.layout.json';report=run('edit',saved,request,edited)
    assert abs(report['routes'][0]['points_m'][1][0]-a[0]-.025)<1e-12
    # Moving an endpoint outside the route API deliberately leaves captured points stale.
    shifted=json.loads(edited.read_text())
    for object in shifted['objects3d']:
        if object['persistentId']=='water_controller':
            object['transform']['position']['y']+=.1
            object['rectPrism']['frame']['origin']['y']+=.1
    moved=root/'moved.layout.json';moved.write_text(json.dumps(shifted))
    assert run('inspect',moved)['routes'][0]['endpoint_status']=='stale'
    request.write_text(json.dumps(dict(schema=command['schema'],operation='refresh',id='route_1')))
    refreshed=root/'refreshed.layout.json';assert run('edit',moved,request,refreshed)['routes'][0]['endpoint_status']=='current'
    request.write_text(json.dumps(dict(schema=command['schema'],operation='remove',id='route_1')))
    removed=root/'removed.layout.json';assert run('edit',refreshed,request,removed)['count']==0
    for index,change in enumerate(('missing_endpoint','short_points','bad_schema','infinite')):
        bad=copy.deepcopy(command)
        if change=='missing_endpoint':bad['route']['source']['entity_id']='absent'
        if change=='short_points':bad['route']['points_m']=[]
        if change=='bad_schema':bad['schema']='unknown'
        if change=='infinite':bad['route']['points_m'][1][0]=float('inf')
        request.write_text(json.dumps(bad));output=root/f'bad{index}.json'
        assert not run('edit',saved,request,output,success=False)['ok'] and not output.exists()
    request.write_text(json.dumps(dict(schema=command['schema'],operation='default_views')))
    views=root/'views.json';view_report=run('edit',removed,request,views)
    assert len(view_report['savedViews'])==10 and view_report==run('inspect',views)
    eligible=next(e for e in json.loads(saved.read_text())['engineering']['entities'] if not e['reference'] and not e['volumeRole'] and e['id'] not in ['can_hub','water_controller'])
    numeric=next(o['id'] for o in json.loads(saved.read_text())['objects3d'] if o['persistentId']==eligible['id'])
    request.write_text(json.dumps(dict(schema=command['schema'],operation='mark_corridor',object_id=numeric)))
    region=root/'region.json';run('edit',saved,request,region)
    assert any(e['type']=='RoutingCorridor' and e['id']==eligible['id'] for e in json.loads(region.read_text())['engineering']['entities'])
    request.write_text(json.dumps(dict(schema=command['schema'],operation='split',id='route_1',point_index=1)))
    split_file=root/'split.json';split=run('edit',saved,request,split_file)
    assert split['count']==2
    assert abs(sum(r['length_m'] for r in split['routes'])-created['routes'][0]['length_m'])<1e-9
    junction=split['routes'][0]['destination']['entity_id']
    assert junction==split['routes'][1]['source']['entity_id']
    node=next(n for n in split['connections'] if n['entity_id']==junction)
    assert node['degree']==2 and node['passive_junction'] and not node['domain_conflict']
    request.write_text(json.dumps(dict(schema=command['schema'],operation='inventory',items=[dict(entity_id='route_1',subsystem='electrical.data',design_amps=None)])))
    inventory_file=root/'inventory.json';inv=run('edit',split_file,request,inventory_file)
    assert next(i for i in inv['inventory']['items'] if i['entity_id']=='route_1')['subsystem']=='electrical.data'
    for rows in [[dict(entity_id='missing',name='unknown')],[dict(entity_id='route_1',design_amps=-1)],[dict(entity_id='route_1',dimensions_m=[1,2,3])]]:
        request.write_text(json.dumps(dict(schema=command['schema'],operation='inventory',items=rows)))
        bad_file=root/'inventory_bad.json'
        assert not run('edit',split_file,request,bad_file,success=False)['ok'] and not bad_file.exists()
    # Generate the finite panel/section acceptance fixture through native edits.
    connected=root/'connected'
    subprocess.run([sys.executable,str(ROOT/'tools/build_van_sections.py'),'--output',str(connected),'--tool',str(TOOL)],check=True,capture_output=True,text=True)
    document=json.loads((connected/'van_connected_sections.layout.json').read_text())
    proof=run('inspect',connected/'van_connected_sections.layout.json')
    assert proof['count']==22 and len(document['engineering']['assemblies'])==21
    assert not any(n['domain_conflict'] or n['stale_sections'] for n in proof['connections'])
    nodes={n['entity_id']:n for n in proof['connections']}
    assert nodes['water_24_tap']['degree']==3 and nodes['rear_halo_tap']['degree']==3 and nodes['can_rear_tap']['degree']==3
    assert nodes['rear_halo_tap']['sections']!=nodes['can_rear_tap']['sections']
    old=json.loads((ROOT/'config/examples/van_wiring_tentative_s5b.layout.json').read_text())
    for family in ['route_1','route_2','route_3']:
        sections=[r for r in proof['routes'] if any(p['key']=='section_of' and p['value']==family for p in r['properties'])]
        original=next(r for r in old['engineering']['routes'] if r['id']==family)
        if family!='route_1':
            old_length=sum(sum((a-b)**2 for a,b in zip(u,v))**.5 for u,v in zip(original['points_m'],original['points_m'][1:]))
            assert abs(sum(r['length_m'] for r in sections)-old_length)<1e-6
            assert abs(sum(r['design']['electrical']['return_m'] for r in sections)-original['design']['electrical']['return_m'])<1e-9
    objects={o['persistentId']:o for o in document['objects3d']}
    assert abs(objects['bed_deck']['rectPrism']['height']-.82)<1e-6
    assert abs(objects['bed_deck']['rectPrism']['width']-1.8)<1e-6
    assert objects['driver_bench']['rectPrism']['width']<.019
    assert any(e['id']=='driver_bench_top' and e['parent']=='driver_bench_unit' for e in document['engineering']['entities'])
    envelope=document['engineering']['motionEnvelopes'][0]
    assert len(envelope['members'])==6 and envelope['samples']==16
    assert json.loads((connected/'provenance.json').read_text())['panel_contact_audit'].startswith('connected')
    assert INPUT.read_bytes()==before
print('route-smoke passed: connected panel van, split conservation, junction incidence, inventory atomicity, motion refresh, native inspect/edit/reload, physical point edit, stale/refresh, removal, overwrite refusal and invalid-input isolation')
