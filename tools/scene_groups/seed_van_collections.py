#!/usr/bin/env python3
"""Emit explicit native collection edits for the reviewed articulated van snapshot.

This is a one-time identity-bound migration. Runtime grouping never guesses from
labels, material names or coordinates. Unrecognized future parts remain unassigned.
"""
import argparse
import hashlib
import json
from collections import OrderedDict
from pathlib import Path

SOURCE_SHA = '94e19d3d00268e8485bb2ca5f364ec1c0905a51366d5403f4323f2fb3ee46209'

def commands(source):
    raw=Path(source).read_bytes()
    if hashlib.sha256(raw).hexdigest()!=SOURCE_SHA:
        raise ValueError('Source identity changed; review membership before regenerating')
    doc=json.loads(raw);eng=doc['engineering'];assemblies={a['id']:a for a in eng['assemblies']}
    groups=OrderedDict()
    def group(key,name,parent=''):
        groups[key]={'operation':'scene_collection','collection_id':'@scene.'+key,'name':name,'parent':'@scene.'+parent if parent else '', 'members':[]}
    for key,name in [('shell','Vehicle shell'),('roof','Roof equipment'),('driver','Driver-side cabinetry'),('passenger','Passenger-side cabinetry'),('rear','Rear workspace'),('bed','Bed and seating'),('electrical','Electrical bay'),('water','Water system')]:group(key,name)
    for key,name,parent in [
        ('shell.exterior','Exterior body','shell'),('shell.roof','Roof skin and lining','shell'),('shell.oem','OEM reference surfaces','shell'),
        ('roof.solar','Solar panels and mounts','roof'),('roof.fans','Fans and adapters','roof'),('roof.entry','Cable entry and service hardware','roof'),
        ('driver.lower','Lower storage / dresser','driver'),('driver.upper','Overhead cabinets','driver'),
        ('passenger.sink','Sink cabinet and folding worktop','passenger'),('passenger.storage','Pantry and tool drawers','passenger'),('passenger.fridge','Refrigerator and supports','passenger'),('passenger.upper','Overhead cabinets','passenger'),('passenger.entry','Entry overhead cabinet','passenger'),
        ('rear.driver','Driver desk and drawers','rear'),('rear.passenger','Passenger desk and drawers','rear'),
        ('bed.platform','Bed platform and lift structure','bed'),('bed.bench','Bench and seating','bed'),
        ('electrical.housing','Bay enclosure and access','electrical'),('electrical.battery','Batteries and power equipment','electrical'),('electrical.distribution','Distribution and service boards','electrical'),('electrical.wiring','Wiring and control hardware','electrical'),
        ('water.fresh','Fresh tank and pump','water'),('water.gray','Gray tank and supports','water'),('water.sink','Sink, faucet and drain','water')]:group(key,name,parent)
    owner={'oem':'shell.oem','storage_driver_unit':'driver.lower','upper_cabinet_driver_unit':'driver.upper','storage_passenger_unit':'passenger.storage',
           'upper_cabinet_passenger_unit':'passenger.upper','upper_door_header_cabinet_unit':'passenger.entry','kitchen_base_unit':'passenger.sink',
           'desk_driver_unit':'rear.driver','drawers_driver_unit':'rear.driver','desk_passenger_unit':'rear.passenger','drawers_passenger_unit':'rear.passenger',
           'driver_bench_unit':'electrical.housing','electrical':'electrical.wiring','bed':'bed.platform','fixed_lift':'bed.platform'}
    for n in range(1,16):owner[f'assembly_{n}']='electrical.battery' if n in [1,2,3,12,13,14] else 'electrical.distribution'
    for n,key in [(16,'water.fresh'),(17,'water.gray'),(18,'water.fresh'),(19,'water.sink'),(20,'water.sink'),(21,'water.sink'),(22,'passenger.fridge'),(24,'shell.roof'),(25,'shell.roof'),(39,'shell.exterior'),(58,'passenger.fridge')]:owner[f'assembly_{n}']=key
    for n in range(26,32):owner[f'assembly_{n}']='roof.fans'
    for n in [32,33,34,35,37,38]:owner[f'assembly_{n}']='roof.entry'
    owner['assembly_36']='electrical.wiring'
    def source_owner(parent):
        for _ in range(len(assemblies)+1):
            if parent in owner:return owner[parent]
            if parent not in assemblies:return None
            parent=assemblies[parent]['parent']
        raise ValueError('assembly cycle')
    def by_range(n):
        ranges=[(124,130,'electrical.wiring'),(131,238,'electrical.distribution'),(239,249,'passenger.sink'),(250,252,'water.fresh'),(253,253,'water.sink'),(263,271,'water.gray'),
                (272,275,'passenger.fridge'),(276,328,'passenger.storage'),(335,356,'passenger.upper'),(357,397,'driver.lower'),(398,433,'driver.upper'),
                (434,463,'bed.platform'),(464,465,'rear.driver'),(466,467,'rear.passenger'),(468,472,'driver.lower'),(473,477,'passenger.storage'),
                (478,484,'rear.driver'),(485,491,'rear.passenger'),(492,495,'passenger.storage'),(496,499,'driver.lower'),(500,502,'passenger.fridge'),(503,505,'rear.driver'),(524,526,'rear.passenger'),
                (553,554,'rear.driver'),(555,556,'rear.passenger'),(557,561,'driver.lower'),(562,566,'passenger.storage'),(567,573,'rear.driver'),(574,580,'rear.passenger'),(581,584,'passenger.storage'),(585,588,'driver.lower'),
                (589,590,'shell.roof'),(591,667,'roof.solar'),(668,695,'roof.fans'),(696,701,'roof.entry')]
        return next((key for lo,hi,key in ranges if lo<=n<=hi),None)
    unassigned=[]
    for entity in eng['entities']:
        id=entity['id'];key=source_owner(entity['parent'])
        n=int(id[6:]) if id.startswith('obj3d_') else -1
        if key is None:key=by_range(n)
        # Explicit reviewed sources whose broad original parent is not their area.
        if id=='obj3d_545':key='rear.driver'
        if id=='obj3d_548':key='rear.passenger'
        if key is None and entity['parent']=='upper':key='electrical.wiring'
        if key is None and entity['parent']=='kitchen':key='passenger.sink'
        if key is None:unassigned.append(id);continue
        groups[key]['members'].append(id)
    # Assembly selectors make new parts added to an established furniture unit inherit its group.
    for id,key in owner.items():
        if id in assemblies:groups[key]['members'].append(id)
    # Deliberately shared membership: sink plumbing is part of the complete sink cabinet;
    # the electrical housing is also the bench/seating assembly.
    groups['passenger.sink']['members'] += ['assembly_19','assembly_20','assembly_21']
    groups['bed.bench']['members'] += ['driver_bench_unit','obj3d_237','obj3d_238']
    # Roof source 23 mixes skin/mounts; it must NEVER be included as a whole in shell.
    views=[('interior','Interior - shell hidden','',['@scene.shell','@layer.guides']),
           ('driver','Driver cabinetry - floor to ceiling','@scene.driver',['@layer.guides']),
           ('kitchen','Passenger kitchen - floor to ceiling','@scene.passenger',['@layer.guides']),
           ('sink','Sink cabinet','@scene.passenger.sink',['@layer.guides']),
           ('electrical','Electrical bay service','@scene.electrical',['@layer.guides','@coverings.access','@coverings.doors.electrical']),
           ('roof','Roof installation','',['@scene.shell.exterior','@scene.shell.oem','@scene.driver','@scene.passenger','@scene.rear','@scene.bed','@scene.electrical','@scene.water','@layer.guides'])]
    ops=list(groups.values())+[{'operation':'working_view','view_id':'working_'+key,'name':name,'only':only,'hidden':hidden,'guides':False} for key,name,only,hidden in views]
    return {'schema':'line_drawing_layout_edit_v1','operations':ops},unassigned

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('source');parser.add_argument('command');args=parser.parse_args()
    ops,unassigned=commands(args.source);Path(args.command).write_text(json.dumps(ops,indent=2)+'\n')
    print(json.dumps({'collections':sum(o['operation']=='scene_collection' for o in ops['operations']),'working_views':6,'unassigned_ids':unassigned}))
