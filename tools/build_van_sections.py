#!/usr/bin/env python3
"""Create-only panel furniture and explicit section fixture from the S5B van.

Meters, fixed stable IDs, no guessed OEM steel or selected equipment ratings.
Native CLI validates every scene and splits each section transactionally.
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
        raise ValueError('Choose a new output directory; existing files are preserved')
    x = json.loads(source.read_text())
    eng = x['engineering']
    objects = {o['persistentId']: o for o in x['objects3d']}
    infos = {e['id']: e for e in eng['entities']}
    for route in eng['routes']:
        # Retire superseded intent label to leave an inventory property slot.
        route['properties'] = [p for p in route['properties'] if p['key'] != 'topology_status']
    originals = copy.deepcopy(objects)
    template = copy.deepcopy(objects['can_hub'])
    panels = {}
    def props(e, **values):
        old = {p['key']: p for p in e['properties']}
        old.update({k: dict(key=k, kind=0, value=str(v)) for k, v in values.items()})
        if len(old) > 8:
            raise ValueError(f'Metadata capacity: {e["id"]}')
        e['properties'] = list(old.values())
    def pos(o):
        return [o['transform']['position'][k] for k in 'xyz']
    def size(o):
        return [o['rectPrism'][k] for k in ['width', 'height', 'depth']]
    def geometry(o, p, d):
        o['transform']['position'] = dict(zip('xyz', p))
        o['rectPrism']['frame']['origin'] = dict(zip('xyz', p))
        o['rectPrism'].update(zip(['width', 'height', 'depth'], d))
    def add(id, name, typ, p, d, parent, subsystem):
        if id in objects or id in infos:
            raise ValueError(f'Identity already exists: {id}')
        o = copy.deepcopy(template)
        o['id'] = max(v['id'] for v in x['objects3d']) + 1
        o['persistentId'] = id
        geometry(o, p, d)
        e = dict(id=id, name=name, type=typ, parent=parent, reference=False,
                 volumeRole=0, volumeOwner='', properties=[])
        props(e, subsystem=subsystem, status='concept_not_measured')
        x['objects3d'].append(o); eng['entities'].append(e)
        objects[id] = o; infos[id] = e
        return id
    def link(a, b, typ='attached_to'):
        if any(r['source'] == a and r['target'] == b and r['type'] == typ for r in eng['relationships']):
            return
        n = eng['nextRelationshipId']
        eng['relationships'].append(dict(id=f'relationship_{n}', source=a, target=b, type=typ))
        eng['nextRelationshipId'] = n + 1
    def assembly(id, name, parent, subsystem):
        e = copy.deepcopy(eng['assemblies'][0])
        e.update(id=id, name=name, parent=parent, properties=[])
        props(e, subsystem=subsystem, status='concept_not_measured')
        eng['assemblies'].append(e)
    def shell(id, t=.018, open_front=False, shelf=False):
        # Preserve old object ID on the outside backing panel. Its new assembly
        # is the whole cabinet; original envelope is retained in provenance.
        o, e = objects[id], infos[id]
        c, d = pos(o), size(o)
        w, length, h = d; sign = 1 if c[0] > 0 else -1
        group = id + '_unit'; parent = e['parent']
        assembly(group, e['name'] + ' / panel assembly', parent, 'interior.structure')
        e['parent'] = group; e['name'] += ' / backing'; e['type'] = 'Panel'
        props(e, subsystem='interior.structure', construction='panelized_concept', status='concept_not_measured')
        geometry(o, [c[0]+sign*(w-t)/2, c[1], c[2]], [t,length,h])
        members = [id]
        def panel(suffix, p, d):
            child = add(id+'_'+suffix, suffix.replace('_',' ').title(), 'Panel', p,d,group,'interior.structure')
            props(infos[child], material='plywood_tentative', construction='panelized_concept')
            link(child,id);members.append(child)
        for end in [-1,1]:
            panel('end_'+('front' if end>0 else 'rear'), [c[0],c[1]+end*(length-t)/2,c[2]], [w-2*t,t,h])
        for end in [-1,1]:
            panel('top' if end>0 else 'bottom', [c[0],c[1],c[2]+end*(h-t)/2], [w-2*t,length-2*t,t])
        if not open_front:
            # Actual thin front panel, not a filled enclosure. Door motion is TBD.
            panel('door', [c[0]-sign*(w-t)/2,c[1],c[2]], [t,length,h])
            props(infos[id+'_door'], motion_status='hinge_and_service_access_TBD')
        if shelf:
            panel('shelf',c,[w-2*t,length-2*t,t])
        panels[group] = members
        link(id, 'oem_floor' if c[2]-h/2 < .05 else ('oem_driver_wall' if sign<0 else 'oem_passenger_wall'), 'supported_by')
        return group
    # Cabinets are air-filled panel assemblies; dimensions remain tentative.
    for id in ['storage_driver','storage_passenger','drawers_driver','drawers_passenger']:
        if id.startswith('drawers'):
            # Close the old 20 mm gap between cabinet and desktop.
            p = pos(objects[id]); p[2]=.355
            geometry(objects[id],p,[size(objects[id])[0],size(objects[id])[1],.710])
        shell(id,shelf=True)
    bench = shell('driver_bench',open_front=True)
    infos['driver_cushion']['parent'] = bench
    link('driver_cushion','driver_bench_top','supported_by')
    kitchen = shell('kitchen_base',open_front=True)
    infos['kitchen_worktop']['parent'] = kitchen
    # Raise the top panel to meet the existing worktop underside at 870 mm.
    old = originals['kitchen_base']; bottom=pos(old)[2]-size(old)[2]/2
    for id in panels[kitchen]:
        if id=='kitchen_base_top':
            p=pos(objects[id]);p[2]=.861;geometry(objects[id],p,size(objects[id]))
        elif id.endswith('end_front') or id.endswith('end_rear') or id=='kitchen_base':
            p=pos(objects[id]);d=size(objects[id]);d[2]=.870-bottom;p[2]=bottom+d[2]/2;geometry(objects[id],p,d)
    link('kitchen_worktop','kitchen_base_top','supported_by')
    for id in ['upper_cabinet_driver','upper_cabinet_passenger','upper_door_header_cabinet']:
        group=shell(id,t=.012)
        c=pos(originals[id]);d=size(originals[id]);sign=1 if c[0]>0 else -1
        outer=abs(c[0])+d[0]/2
        gap=.96-outer
        if gap>.001:
            rail=add(id+'_mount_rail','Wall mounting rail / unverified attachment','Rail',
                     [sign*(outer+gap/2),c[1],c[2]], [gap,d[1],.035],group,'interior.structure')
            link(id,rail);link(rail,'oem_passenger_wall' if sign>0 else 'oem_driver_wall')
    # Paired desks: each existing top + drawer unit + two legs form one assembly.
    for side in ['driver','passenger']:
        id='desk_'+side;c=pos(objects[id]);w,length,t=size(objects[id]);group=id+'_unit'
        assembly(group,side.title()+' desk / connected supports','desks','interior.structure')
        infos[id]['parent']=group
        next(a for a in eng['assemblies'] if a['id']=='drawers_'+side+'_unit')['parent']=group
        underside=c[2]-t/2
        for edge in [-1,1]:
            leg=add(id+'_leg_'+str(edge).replace('-','n'),'Desk support leg','StructuralMember',
                    [c[0]-(1 if side=='passenger' else -1)*(w/2-.025),c[1]+edge*(length/2-.025),underside/2],
                    [.04,.04,underside],group,'interior.structure')
            link(leg,'oem_floor','supported_by');link(id,leg,'supported_by')
        link(id,'drawers_'+side+'_top','supported_by')
    # Close the lift frame: these move with bed, not the fixed guide assembly.
    bed=objects['bed_deck'];c=pos(bed);w,length,t=size(bed)
    for edge in [-1,1]:
        rail=add('bed_crossrail_'+('front' if edge>0 else 'rear'),'Bed frame transverse crossrail','Rail',
                 [c[0],c[1]+edge*(length/2-.02),c[2]-t/2-.02],
                 [1.79,.04,.08],'bed','interior.structure')
        link('bed_deck',rail,'supported_by');link(rail,'bed_rail_driver');link(rail,'bed_rail_passenger')
    link('bed_deck','bed_rail_driver','supported_by');link('bed_deck','bed_rail_passenger','supported_by')
    link('bed_mattress','bed_deck','supported_by')
    # Existing envelope member list describes the prior moving assembly. It must
    # stay visibly stale after adding frame members; never relabel as current.
    # Separate CAN junctions from power taps. Crossing/nearby points do not connect.
    add('can_rear_tap','Rear CAN0 junction','Connector',[.875,-1.75,1.86],[.016,.016,.016],'upper','electrical.data')
    add('can_water_tap','Water CAN0 junction','Connector',[.875,.3188333213,1.86],[.016,.016,.016],'upper','electrical.data')
    props(infos['rear_halo_tap'],power_domain='24V')
    infos['rear_halo_tap']['name']='Rear 24 V halo junction'
    for id in ['can_rear_tap','can_water_tap']:
        props(infos[id],bus='can0',power_domain='signal')
    # Offset signal run by 10 mm laterally / 20 mm vertically from power run.
    can=eng['routes'][0]
    can['points_m']=[pos(objects['can_hub']),[-.875,1.25,.39],[-.875,1.25,1.86],[-.875,-1.94,1.86],
                     [.875,-1.94,1.86],pos(objects['can_rear_tap']),pos(objects['can_water_tap']),pos(objects['water_controller'])]
    # A node short drop uses the same signal junction as the CAN trunk.
    stub=next(r for r in eng['routes'] if r['id']=='route_11')
    stub['source']['entity_id']='can_rear_tap';stub['points_m'][0]=pos(objects['can_rear_tap'])
    power=next(r for r in eng['routes'] if r['id']=='route_2')
    power['points_m'].insert(3,pos(objects['rear_halo_tap']))
    power['points_m'].insert(4,pos(objects['water_24_tap']))
    # Entity-based subsystem grouping, not render-derived dimensions or loads.
    for id in ['battery','inverter','fuse_box','dc24_halo_start','dc24_halo_end','dc12_converter','dc12_halo_start','dc12_halo_end']:
        props(infos[id],subsystem='electrical.power')
    for id in ['water_tank','water_pump','water_controller','water_local_buck']:
        props(infos[id],subsystem='water')
    for id in ['can_hub','rear_esp_node','rear_local_buck']:
        props(infos[id],subsystem='embedded.control')
    # Split using native mutation, keeping original family IDs and load assumptions.
    with tempfile.TemporaryDirectory(prefix='van-sections-') as temp:
        current=Path(temp)/'base.json';current.write_text(json.dumps(x))
        subprocess.run([str(tool),'inspect',str(current)],check=True,stdout=subprocess.DEVNULL)
        step=0
        def run(command):
            nonlocal current,step
            step+=1;request=Path(temp)/f'command{step}.json';request.write_text(json.dumps(dict(schema='line_drawing_route_edit_v1',**command)))
            nxt=Path(temp)/f'step{step}.json'
            report=json.loads(subprocess.check_output([str(tool),'edit',str(current),str(request),str(nxt)]))
            current=nxt
            return report
        # Split furthest points first so original point indices remain valid.
        for point,junction in [(4,'water_24_tap'),(3,'rear_halo_tap'),(2,''),(1,'')]:
            run(dict(operation='split',id='route_2',point_index=point,junction_id=junction))
        for point,junction in [(6,'can_water_tap'),(5,'can_rear_tap'),(4,''),(3,''),(2,'')]:
            run(dict(operation='split',id='route_1',point_index=point,junction_id=junction))
        for point in [2,1]:
            run(dict(operation='split',id='route_3',point_index=point))
        for motion in x['geometricConstraints']:
            if motion.get('motion_assembly') == 'bed' or motion.get('motionAssembly') == 'bed':
                run(dict(operation='refresh_motion',id=motion['id'],samples=16))
        report=json.loads(subprocess.check_output([str(tool),'inspect',str(current)]))
        # Human labels for automatic corner junctions; backend IDs stay stable.
        items=[]
        native=json.loads(current.read_text())
        node_info={e['id']:e for e in native['engineering']['entities']}
        node_objects={o['persistentId']:o for o in native['objects3d']}
        for e in native['engineering']['entities']:
            if e['name'].startswith('Junction /'):
                p=pos(node_objects[e['id']]);domain=next((v['value'] for v in e['properties'] if v['key']=='power_domain'),'')
                side='driver' if p[0]<0 else 'passenger'
                zone='upper entry' if p[1]>0 else 'rear corner'
                name=f'{domain or "CAN0"} / {side} {zone}'
                e['name']=name
                items.append(dict(entity_id=e['id'],name=name,subsystem='electrical.data' if domain=='signal' else 'electrical.power'))
        for r in report['routes']:
            family=next((p['value'] for p in r['properties'] if p['key']=='section_of'),'')
            family_name={'route_1':'CAN0','route_2':'24 V','route_3':'Optional 12 V'}.get(family)
            if family_name:
                a,b=r['points_m'][0],r['points_m'][-1]
                if abs(a[1]-b[1])<.001 and a[1]<-1.9:
                    section='rear crossover'
                elif a[0]<0 and b[0]<0:
                    section='driver side' if a[2]>1 else 'hub to upper entry'
                elif a[1]<-1.9:
                    section='passenger rear to rear node'
                elif b[1]>1:
                    section='water node to passenger front'
                elif abs(a[0]-b[0])>.1:
                    section='water node drop'
                else:
                    section='rear node to water node'
                items.append(dict(entity_id=r['id'],name=f'{family_name} / {section}',subsystem='electrical.data' if family=='route_1' else 'electrical.power'))
        report=run(dict(operation='inventory',items=items))
        output.mkdir(parents=True)
        final=output/'van_connected_sections.layout.json';final.write_bytes(current.read_bytes())
        (output/'routing_report.json').write_text(json.dumps(report,indent=2)+'\n')
        (output/'subsystem_inventory.json').write_text(json.dumps(report['inventory'],indent=2)+'\n')
    # Offline contact audit of generated panel shells; no strength assertion.
    disconnected=[]
    for group,members in panels.items():
        seen={members[0]}
        def touch(a,b):
            return all(abs(pos(objects[a])[k]-pos(objects[b])[k]) <= (size(objects[a])[k]+size(objects[b])[k])/2+1e-6 for k in range(3))
        while True:
            found={m for m in members if any(touch(m,n) for n in seen)}
            if found<=seen:break
            seen|=found
        disconnected.extend(m for m in members if m not in seen)
    if disconnected:raise ValueError(f'Disconnected panels: {disconnected}')
    (output/'provenance.json').write_text(json.dumps(dict(source=str(source),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        original_furniture_envelopes={id:dict(center_m=pos(originals[id]),size_m=size(originals[id])) for id in ['driver_bench','kitchen_base','storage_driver','storage_passenger','upper_cabinet_driver','upper_cabinet_passenger']},
        panel_contact_audit='connected_within_1_um_not_structural_certification',
        bed_fore_aft_width_m=size(objects['bed_deck'])[1],bed_transverse_length_m=size(objects['bed_deck'])[0],
        load_basis='preserved_per_section_assumptions_no_aggregation', motion='native_envelope_refresh_after_new_bed_members',
        limitations=['unmeasured_nominal_OEM_shell','cabinet_door_hinges_TBD','holes_and_protection_TBD','routing_findings_remain_explicit']),indent=2)+'\n')
    return final


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--input',type=Path,default=ROOT/'config/examples/van_wiring_tentative_s5b.layout.json')
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--tool',type=Path,default=ROOT/'build/toolchains/clang/bin/physical_route_tool')
    a=p.parse_args()
    print(build(a.input.resolve(),a.output.resolve(),a.tool.resolve()))
