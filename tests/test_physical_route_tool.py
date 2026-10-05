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
    assert INPUT.read_bytes()==before
print('route-smoke passed: native inspect/edit/reload, physical point edit, stale/refresh, removal, overwrite refusal and invalid-input isolation')
