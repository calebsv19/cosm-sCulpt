#!/usr/bin/env python3
"""Create-only tentative wiring fixture; native CLI validates and writes every step.

Geometry and currents are illustrative. Original input and user projects are never
rewritten. This is a layout/voltage-drop example, not a protection or ampacity design.
"""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def build(source, output, tool):
    if output.exists():
        raise ValueError('Output already exists; choose a new directory')
    x = json.loads(source.read_text())
    objects = {o['persistentId']: o for o in x['objects3d']}
    infos = {e['id']: e for e in x['engineering']['entities']}
    def props(info, **values):
        old = {p['key']: p for p in info['properties']}
        old.update({k: dict(key=k, kind=0, value=str(v)) for k, v in values.items()})
        info['properties'] = list(old.values())
        if len(old) > 8:
            raise ValueError('Metadata capacity exceeded')
    def center(id):
        return [objects[id]['transform']['position'][k] for k in 'xyz']
    def add(id, name, typ, pos, size, parent='upper'):
        o = copy.deepcopy(objects['can_hub'])
        o['id'] = max(v['id'] for v in x['objects3d']) + 1
        o['persistentId'] = id
        o['transform']['position'] = dict(zip('xyz', pos))
        o['rectPrism']['frame']['origin'] = dict(zip('xyz', pos))
        o['rectPrism'].update(zip(('width','height','depth'),size))
        x['objects3d'].append(o); objects[id] = o
        info = dict(id=id, name=name, type=typ, parent=parent, reference=False,
                    volumeRole=0, volumeOwner='', properties=[])
        props(info, dimensions_status='concept_not_measured', subsystem='electrical',
              status='tentative_layout_not_hardware_selection')
        x['engineering']['entities'].append(info); infos[id] = info
    # Upgrade the existing named regions, without altering their dimensions.
    for id in ['upper_halo_driver','upper_halo_passenger','upper_halo_rear','driver_power_riser']:
        infos[id]['type'] = 'RoutingCorridor'
        infos[id]['reference'] = False
        props(infos[id], status='corridor_intent_not_cable')
    # Real return conductors accompany the power routes; no chassis return assumption.
    add('dc24_halo_start','24 V halo distribution start (tentative)','PowerBus',[-.865,1.74,1.88],[.03,.05,.03])
    add('dc24_halo_end','24 V halo far end (tentative)','PowerBus',[.865,1.74,1.88],[.03,.05,.03])
    add('dc12_converter','24 to 12 V converter (size TBD)','Device',[-.44,1.18,.45],[.08,.12,.08],'electrical')
    props(infos['dc12_converter'], input_domain='24V', output_domain='12V', capacity_status='TBD')
    add('dc12_halo_start','Optional 12 V distribution start','PowerBus',[-.855,1.74,1.87],[.025,.04,.025])
    add('dc12_halo_end','Optional 12 V far end','PowerBus',[.855,1.74,1.87],[.025,.04,.025])
    add('water_local_buck','Water ESP local 24 to 5 V buck (size TBD)','Device',[.43,.3188333213,.81],[.04,.06,.04],'kitchen')
    props(infos['water_local_buck'], input_domain='24V', output_domain='5V', capacity_status='TBD')
    add('water_24_tap','Water node 24 V halo tap','Connector',[.865,.3188333213,1.88],[.025,.025,.025])
    add('rear_esp_node','Rear ESP / CAN node (tentative)','Controller',[.865,-1.75,1.73],[.04,.07,.05])
    props(infos['rear_esp_node'], network='can0', power_domain='local_24V_to_5V')
    add('rear_local_buck','Rear ESP local buck (size TBD)','Device',[.865,-1.66,1.73],[.04,.06,.04])
    props(infos['rear_local_buck'], input_domain='24V', output_domain='5V')
    add('rear_halo_tap','Rear 24 V / CAN routing tap (concept only)','Connector',[.865,-1.75,1.88],[.025,.025,.025])
    # Box regions include the endpoint drops. A broad feeder box declares intent,
    # not permission to occupy cabinet/service volumes; checks may flag it.
    add('driver_feeder_region','Driver electrical feeder region','RoutingCorridor',[-.65,1.46,1.13],[.55,.66,1.60])
    add('water_drop_region','Passenger water node drop region','RoutingCorridor',[.645,.3188333213,1.28],[.56,.12,1.30])
    add('rear_node_region','Rear ESP short drop region','RoutingCorridor',[.865,-1.70,1.80],[.12,.25,.25])
    # Named definitions remain editable queries, rather than hard-coded layer bits.
    def q(type='', assembly='', key='', value='', designation=0):
        return dict(type=type, designation=designation, assembly=assembly, key=key, value=value)
    views = [('OEM',[q(designation=2)]),
             ('Furniture',[q(type=t,designation=1) for t in ['PhysicalObject','Panel','Rail','StructuralMember','Cabinet','Furniture']]),
             ('Devices',[q(type=t) for t in ['Controller','Device','Battery','Fuse','Actuator','Sensor','Connector','PowerBus']]),
             ('Wiring',[q(type='Cable')]),('24 V',[q(key='power_domain',value='24V')]),
             ('12 V',[q(key='power_domain',value='12V')]),('CAN',[q(key='bus',value='can0')]),
             ('Plumbing',[q(type='Pipe'),q(type='Tank')]),('Walkways',[q(key='view_group',value='walkways')]),
             ('Keepouts',[q(key='view_group',value='keepouts')]),('Service',[q(type='ServiceVolume')]),
             ('Motion',[q(type='MotionEnvelope')]),('Corridors',[q(type='RoutingCorridor')])]
    for id in ['sliding_door_access','rear_door_access']:
        props(infos[id],view_group='keepouts')
    props(infos['walkway'], view_group='walkways')
    x['engineering']['savedViews'] = [dict(id=f'van_view_{i+1}', name=name, queries=queries) for i,(name,queries) in enumerate(views)]
    x['file']['schemaVersion'] = 21
    halo = ['upper_halo_driver','upper_halo_rear','upper_halo_passenger']
    def electrical(kind, domain='', amps=0, awg=None, length=0, allowance=0):
        area = math.pi * (.127*92**((36-awg)/39))**2/4 if awg is not None else 0
        return dict(circuit=domain or 'CAN0', power_domain=domain, voltage_class=kind,
                    nominal_volts={'24V':24,'12V':12,'5V':5}.get(domain,0), design_amps=amps,
                    area_mm2=area, return_m=length, allowance_m=allowance,
                    fuse_amps=0, copper=awg is not None)
    def route(name, a, b, middle, corridors, kind=1, domain='', amps=0, awg=None, max_length=0):
        points = [center(a),*middle,center(b)]
        length = sum(math.dist(u,v) for u,v in zip(points,points[1:]))
        info = dict(id='', name=name,type='Cable',parent='',reference=False,volumeRole=0,volumeOwner='',properties=[])
        props(info, dimensions_status='concept_not_measured', power_domain=domain or 'signal', bus='can0' if kind==2 else '',
              current_basis='illustrative_assumption_not_inventory_load' if amps else 'unknown',
              wire_basis=f'comparison_AWG_{awg}_not_final' if awg is not None else '120_ohm_twisted_pair_TBD',
              topology_status='intent_only_no_network_solver')
        info.update(points_m=points,source=dict(entity_id=a,local_offset_m=[0,0,0]),destination=dict(entity_id=b,local_offset_m=[0,0,0]),
                    design=dict(corridors=corridors,radius_m=.003 if kind==1 else .002,clearance_m=0,maximum_length_m=max_length,
                                electrical=electrical(kind,domain,amps,awg,length if kind==1 else 0,.1 if kind==1 else 0)))
        return info
    can = x['engineering']['routes'][0]
    can['name'] = 'CAN0 main run - U-shaped halo, tentative'
    can['design'] = dict(corridors=halo+['driver_feeder_region','water_drop_region'],radius_m=.002,clearance_m=0,maximum_length_m=0,electrical=electrical(2))
    # Existing CAN route remains a linear bus candidate, not an electrical ring.
    candidates = [
        route('24 V halo trunk - AWG 10 / 10 A comparison','dc24_halo_start','dc24_halo_end',[[-.865,-1.94,1.88],[.865,-1.94,1.88]],halo,domain='24V',amps=10,awg=10),
        route('Optional 12 V halo - AWG 10 / 5 A comparison','dc12_halo_start','dc12_halo_end',[[-.855,-1.94,1.87],[.855,-1.94,1.87]],halo,domain='12V',amps=5,awg=10),
        route('24 V feed from protected distribution (fuse TBD)','fuse_box','dc24_halo_start',[[-.865,1.55,.39],[-.865,1.55,1.88]],['driver_feeder_region','upper_halo_driver'],domain='24V',amps=10,awg=10),
        route('24 V feed to 12 V converter (capacity TBD)','fuse_box','dc12_converter',[],['driver_feeder_region'],domain='24V',amps=0,awg=12),
        route('12 V converter to optional halo','dc12_converter','dc12_halo_start',[[-.855,1.18,.45],[-.855,1.18,1.87]],['driver_feeder_region','upper_halo_driver'],domain='12V',amps=5,awg=10),
        route('Water buck 24 V branch - load assumption 0.5 A','water_24_tap','water_local_buck',[[.865,.3188333213,.81]],['upper_halo_passenger','water_drop_region'],domain='24V',amps=.5,awg=18),
        route('Water buck 5 V output - load assumption 1 A','water_local_buck','water_controller',[],['water_drop_region'],domain='5V',amps=1,awg=20),
        route('Rear buck 24 V branch - load assumption 0.5 A','rear_halo_tap','rear_local_buck',[],['rear_node_region','upper_halo_passenger'],domain='24V',amps=.5,awg=18),
        route('Rear buck 5 V output - load assumption 1 A','rear_local_buck','rear_esp_node',[],['rear_node_region'],domain='5V',amps=1,awg=20),
        route('Rear CAN stub - illustrative 0.3 m limit at 1 Mbps','rear_halo_tap','rear_esp_node',[],['rear_node_region','upper_halo_passenger'],kind=2,max_length=.3)]
    with tempfile.TemporaryDirectory(prefix='sculpt-wiring-') as temp:
        base=Path(temp)/'base.json';base.write_text(json.dumps(x))
        subprocess.run([str(tool),'inspect',str(base)],check=True,stdout=subprocess.DEVNULL)
        current=base
        for i,r in enumerate(candidates):
            command=Path(temp)/f'command_{i}.json';command.write_text(json.dumps(dict(schema='line_drawing_route_edit_v1',operation='upsert',route=r)))
            nxt=Path(temp)/f'step_{i}.json'
            subprocess.run([str(tool),'edit',str(current),str(command),str(nxt)],check=True,stdout=subprocess.DEVNULL)
            current=nxt
        output.mkdir(parents=True)
        final=output/'van_wiring_tentative.layout.json';final.write_bytes(current.read_bytes())
        report=subprocess.check_output([str(tool),'inspect',str(final)])
        (output/'routing_report.json').write_bytes(report)
    (output/'provenance.json').write_text(json.dumps(dict(source=str(source),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        status='tentative_geometry_currents_gauges_and_fuse_ratings_require_inventory_and_measurements',
        return_basis='each_DC_route_explicit_equal_length_copper_return', bed_width_m=objects['bed_deck']['rectPrism']['height']),indent=2)+'\n')
    return final

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,default=ROOT/'config/examples/van_routing_s5a.layout.json')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--tool',type=Path,default=ROOT/'build/toolchains/clang/bin/physical_route_tool')
    args=parser.parse_args()
    print(build(args.input.resolve(),args.output.resolve(),args.tool.resolve()))
