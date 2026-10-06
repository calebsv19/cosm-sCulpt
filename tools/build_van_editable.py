#!/usr/bin/env python3
"""Create an ID-preserving F3/F4 draft from a saved F2 van. Never overwrite working files."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess


def build(source, output, tool):
    if output.exists() or output.with_suffix('.review.json').exists():
        raise ValueError('Output/review exists; choose a new draft name')
    raw = source.read_bytes()
    x = copy.deepcopy(json.loads(raw))
    e = x['engineering']
    if len(e.get('furnitureUnits', [])) != 3 or any(u.get('sinkOpening') for u in e['furnitureUnits']):
        raise ValueError('Expected a saved F2 draft with three managed units')
    if x['physicalContext'] != dict(coordinateSystem='right_handed_z_up_meters', metersPerWorldUnit=1):
        raise ValueError('Expected canonical meters and Z-up')
    if subprocess.run([str(tool), 'inspect', str(source)], capture_output=True).returncode:
        raise ValueError('Source failed native validation')
    objects = {o['persistentId']: o for o in x['objects3d']}
    infos = {i['id']: i for i in e['entities']}
    assemblies = {a['id']: a for a in e['assemblies']}
    identity = [0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1]
    changed = []

    def pos(o): return [o['transform']['position'][k] for k in 'xyz']
    def dims(o): return [o['rectPrism'][k] for k in ('width', 'height', 'depth')]
    def member(oid, aid):
        o = objects[oid]
        if infos[oid]['parent'] != aid or o['kind'] != 'rect_prism' or o['transform']['scale'] != dict(x=1,y=1,z=1):
            raise ValueError('Unsupported member '+oid)
        if [o['rectPrism']['frame'][k] for k in ('axisU','axisV','normal')] != [dict(x=1,y=0,z=0),dict(x=0,y=1,z=0),dict(x=0,y=0,z=1)]:
            raise ValueError('Rotated member needs manual conversion: '+oid)
        return o
    def recipe(prefix):
        aid = prefix+'_unit'
        if assemblies[aid]['worldFrame'] != identity:
            raise ValueError('Moved assembly needs manual conversion: '+aid)
        back = member(prefix,aid)
        sign = 1 if pos(back)[0] > 0 else -1
        door = member(prefix+'_door',aid) if prefix+'_door' in objects else None
        end = member(prefix+'_end_front',aid)
        b = dims(back)[0]; t = dims(end)[1]; f = dims(door)[0] if door else 0
        d = dims(end)[0]+b+f
        wall = pos(back)[0]+sign*b/2
        center = [wall-sign*d/2,pos(back)[1],pos(back)[2]]
        u = dict(assembly=aid,backSign=sign,centerMeters=center,
                 sizeMeters=[d,dims(back)[1],dims(back)[2]],thicknessMeters=[t,b,f or t,.04],
                 overhangMeters=[0,0,0,0],shelfFraction=.5,sinkOpening=False,parts=[])
        names = [(prefix,0),(prefix+'_end_rear',1),(prefix+'_end_front',2),(prefix+'_bottom',3),(prefix+'_top',4)]
        for suffix,role in [('_door',5),('_shelf',6),('_mount_rail',9)]:
            if prefix+suffix in objects: names.append((prefix+suffix,role))
        for oid,role in names:
            o=member(oid,aid); offset=[0,0,0]; span=[0,0,0]
            if role==9:
                offset=[pos(o)[k]-center[k] for k in range(3)];span=[dims(o)[0],0,dims(o)[2]]
            u['parts'].append(dict(entity=oid,role=role,offsetMeters=offset,spanMeters=span))
        return u

    for prefix in ('driver_bench','storage_driver','upper_cabinet_driver','upper_door_header_cabinet','drawers_driver','drawers_passenger'):
        u = recipe(prefix)
        if prefix == 'driver_bench':
            o=member('driver_cushion',u['assembly'])
            u['parts'].append(dict(entity='driver_cushion',role=10,offsetMeters=[0,0,0],spanMeters=[0,0,dims(o)[2]]))
        if prefix.startswith('drawers_'):
            side=prefix.split('_')[1];oid='desk_'+side;o=member(oid,oid+'_unit')
            # One physical desktop belongs to its cabinet recipe; the existing desk
            # assembly remains the transform parent of that support assembly.
            infos[oid]['parent']=u['assembly'];changed.append(oid)
            wall=u['centerMeters'][0]+u['backSign']*u['sizeMeters'][0]/2
            front=u['centerMeters'][0]-u['backSign']*u['sizeMeters'][0]/2
            q=pos(o);v=dims(o)
            u['thicknessMeters'][3]=v[2]
            u['overhangMeters']=[u['backSign']*(q[0]+u['backSign']*v[0]/2-wall),
                -u['backSign']*(q[0]-u['backSign']*v[0]/2-front),
                u['centerMeters'][1]-u['sizeMeters'][1]/2-(q[1]-v[1]/2),
                q[1]+v[1]/2-(u['centerMeters'][1]+u['sizeMeters'][1]/2)]
            u['overhangMeters']=[max(0,v) for v in u['overhangMeters']]
            u['parts'].append(dict(entity=oid,role=7,offsetMeters=[0,0,0],spanMeters=[0,0,0]))
            assemblies[u['assembly']]['name']=side.title()+' desk / support cabinet'
        e['furnitureUnits'].append(u)
    # Separate bench/wardrobe and upper rules preserve the front entry datum.
    next_id=e['nextFurnitureContactId']
    for driver,de,follower,fe,behavior in [('driver_bench_unit',-1,'storage_driver_unit',1,0),
            ('storage_driver_unit',1,'upper_cabinet_driver_unit',-1,1),
            ('upper_cabinet_passenger_unit',1,'upper_door_header_cabinet_unit',-1,1)]:
        e['furnitureContacts'].append(dict(id='furniture_fit_'+str(next_id),driver=driver,driverEnd=de,
            follower=follower,followerEnd=fe,behavior=behavior,gapMeters=0,enabled=True));next_id+=1
    e['nextFurnitureContactId']=next_id
    kitchen=next(u for u in e['furnitureUnits'] if u['assembly']=='kitchen_base_unit')
    # Existing F2 kitchen may have been rigidly moved. Opening coordinates stay in
    # each member frame; use the saved evaluated fixture, not original generator dimensions.
    kitchen['sinkOpening']=True
    sink=next(p for p in kitchen['parts'] if p['entity']=='sink')
    sink['offsetMeters'][2]=kitchen['thicknessMeters'][3]-sink['spanMeters'][2]/2
    # Kitchen frame conversion supports translation but refuses rotated assembly
    # rather than guessing from screenshots.
    frame=assemblies[kitchen['assembly']]['worldFrame']
    if frame[3:]!=identity[3:]: raise ValueError('Rotated kitchen needs native manual conversion')
    o=objects['sink']
    z=frame[2]+kitchen['centerMeters'][2]+kitchen['sizeMeters'][2]/2+sink['offsetMeters'][2]
    o['transform']['position']['z']=z;o['rectPrism']['frame']['origin']['z']=z
    changed.append('sink')
    for oid in ('kitchen_base_top','kitchen_worktop','sink'):
        o=objects[oid];p=pos(o);v=dims(o);sp=pos(objects['sink']);sv=sink['spanMeters']
        opening=dict(u=sp[0]-p[0],v=sp[1]-p[1],width=sv[0]+.004,height=sv[1]+.004,floor=0)
        if oid=='sink': opening=dict(u=0,v=0,width=sv[0]-.004,height=sv[1]-.004,floor=.002)
        o['rectPrism']['opening']=opening;changed.append(oid)
    x['file']['schemaVersion']=24
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('x') as f: json.dump(x,f,indent=2);f.write('\n')
    check=subprocess.run([str(tool),'inspect',str(output)],capture_output=True,text=True)
    if check.returncode:
        output.unlink();raise ValueError('Native validation refused draft: '+check.stderr[:800])
    report=dict(status='provisional_editable_construction',source=str(source.resolve()),
        source_sha256=hashlib.sha256(raw).hexdigest(),managed_units=[u['assembly'] for u in e['furnitureUnits']],
        changed_ids=sorted(set(changed)),contact_count=len(e['furnitureContacts']),
        preserved_object_count=len(objects),preserved_route_count=len(e['routes']),
        limitations=['sink rectangle, 2 mm bowl walls and 2 mm installation gap are provisional',
            'desk worktop overhangs are editable; support cabinet size is the unit size',
            'contacts preserve run faces, not vehicle-fit or structural certification',
            'wheel wells, fasteners, doors and physical hardware need measured construction details'])
    with output.with_suffix('.review.json').open('x') as f: json.dump(report,f,indent=2);f.write('\n')
    return report

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for k in ('input','output','tool'):p.add_argument('--'+k,required=True,type=Path)
    a=p.parse_args();print(json.dumps(build(a.input,a.output,a.tool)))
