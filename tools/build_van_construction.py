#!/usr/bin/env python3
"""Create a separate, ID-preserving F0/F1 construction draft; never overwrite input.
The finite layout review uses provisional datums, not measured OEM contours.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess


def build(source, output, tool):
    if not tool.is_file():
        raise ValueError("Native validation tool is missing: "+str(tool))
    raw = source.read_bytes()
    scene = json.loads(raw)
    x = copy.deepcopy(scene)
    if output.exists() or output.with_suffix('.review.json').exists():
        raise ValueError('Output/review already exists; choose a new draft name')
    if x['physicalContext'] != {'coordinateSystem': 'right_handed_z_up_meters', 'metersPerWorldUnit': 1}:
        raise ValueError('This finite fixture review requires the existing meter/Z-up document')
    e = x['engineering']
    if e.get('furnitureUnits'):
        raise ValueError('Input already has construction recipes; edit that draft in the UI')
    objects = {o['persistentId']: o for o in x['objects3d']}
    infos = {o['id']: o for o in e['entities']}
    assemblies = {a['id']: a for a in e['assemblies']}
    changed = set()

    def geometry(id, pos, dims):
        o = objects[id]
        if o['flags']['locked']:
            raise ValueError(f'Locked part: {id}')
        if o['kind'] != 'rect_prism' or o['transform']['scale'] != dict(x=1, y=1, z=1):
            raise ValueError(f'Unsupported native part: {id}')
        r = o['rectPrism']
        if r['lockToBounds'] or r['lockToConstructionPlane']:
            raise ValueError(f'Locked geometry: {id}')
        axes = [r['frame'][a] for a in ['axisU', 'axisV', 'normal']]
        if axes != [dict(x=1, y=0, z=0), dict(x=0, y=1, z=0), dict(x=0, y=0, z=1)]:
            raise ValueError(f'Rotated part requires manual review: {id}')
        o['transform']['position'] = dict(zip('xyz', pos))
        r['frame']['origin'] = dict(zip('xyz', pos))
        r.update(zip(['width', 'height', 'depth'], dims))
        changed.add(id)

    def shell(id, center, size, t, name):
        # Matching native role equations. Reuse every physical panel ID.
        u = dict(assembly=id+'_unit', backSign=1 if center[0] > 0 else -1,
                 centerMeters=center, sizeMeters=size,
                 thicknessMeters=[t, t, t, .04], overhangMeters=[0, 0, 0, 0],
                 shelfFraction=.5, parts=[])
        a = assemblies[u['assembly']]
        if a['worldFrame'] != [0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1]:
            raise ValueError('Moved unit requires manual review: '+u['assembly'])
        a['name'] = name
        roles = [(id, 0), (id+'_end_rear', 1), (id+'_end_front', 2),
                 (id+'_bottom', 3), (id+'_top', 4)]
        for suffix, role in [('_door', 5), ('_shelf', 6)]:
            if id+suffix in objects:
                roles.append((id+suffix, role))
        d, l, h = size
        front = t if any(role == 5 for _, role in roles) else 0
        inside = d-t-front
        for oid, role in roles:
            p = [u['backSign']*(front-t)/2, 0, 0]
            dims = [inside, l-2*t, t]
            if role == 0:
                p = [u['backSign']*(d-t)/2, 0, 0]; dims = [t, l, h]
            elif role in [1, 2]:
                p[1] = (-1 if role == 1 else 1)*(l-t)/2; dims = [inside, t, h]
            elif role in [3, 4]:
                p[2] = (-1 if role == 3 else 1)*(h-t)/2
            elif role == 5:
                p = [-u['backSign']*(d-t)/2, 0, 0]; dims = [t, l, h]
            geometry(oid, [center[k]+p[k] for k in range(3)], dims)
            u['parts'].append(dict(entity=oid, role=role, offsetMeters=[0, 0, 0], spanMeters=[0, 0, 0]))
        return u

    # Accepted narrow layout's zone datums; retain entry, bed and fixed guide geometry.
    entry_front = 1.8315
    kitchen_front = .6123
    tall_front = -.2746333333333333
    bed_front = -1.1615666666666667
    ceiling = 1.91  # Provisional finished-ceiling line, inherited from upper top.
    kitchen = shell('kitchen_base', [.655, (kitchen_front+tall_front)/2, .435],
                    [.5, kitchen_front-tall_front, .87], .018, 'Kitchen cabinet / connected run')
    kitchen['overhangMeters'] = [.02, .03, 0, 0]
    geometry('kitchen_worktop', [.65, kitchen['centerMeters'][1], .89], [.55, kitchen['sizeMeters'][1], .04])
    kitchen['parts'].append(dict(entity='kitchen_worktop', role=7, offsetMeters=[0, 0, 0], spanMeters=[0, 0, 0]))
    sink = objects['sink']
    pos = [sink['transform']['position'][k] for k in 'xyz']
    dims = [sink['rectPrism'][k] for k in ['width', 'height', 'depth']]
    kitchen['parts'].append(dict(entity='sink', role=8,
        offsetMeters=[.905-pos[0], kitchen_front-pos[1], pos[2]-.87], spanMeters=dims))
    infos['sink']['parent'] = 'kitchen_base_unit'
    changed.add('sink')
    bench = shell('driver_bench', [-.655, (entry_front+tall_front)/2, .25],
                  [.5, entry_front-tall_front, .5], .018, 'Driver bench / continuous run')
    cushion = objects['driver_cushion']
    p = [cushion['transform']['position'][k] for k in 'xyz']
    dims = [cushion['rectPrism'][k] for k in ['width', 'height', 'depth']]
    p[1] = bench['centerMeters'][1]; dims[1] = bench['sizeMeters'][1]
    geometry('driver_cushion', p, dims)
    for side, sign in [('driver', -1), ('passenger', 1)]:
        shell('storage_'+side, [sign*.655, (tall_front+bed_front)/2, ceiling/2],
              [.5, tall_front-bed_front, ceiling], .018,
              ('Driver wardrobe' if side == 'driver' else 'Passenger pantry')+' / floor to ceiling draft')
        start = tall_front
        end = entry_front if side == 'driver' else kitchen_front
        shell('upper_cabinet_'+side, [sign*.790, (start+end)/2, 1.745],
              [.25, end-start, .330], .012, side.title()+' upper run / ends at tall cabinet')
        geometry('upper_cabinet_'+side+'_mount_rail', [sign*.9375, (start+end)/2, 1.745], [.045, end-start, .035])
    shell('upper_door_header_cabinet', [.850, (kitchen_front+entry_front)/2, 1.845],
          [.13, entry_front-kitchen_front, .130], .012, 'Shallow passenger upper / entry transition')
    geometry('upper_door_header_cabinet_mount_rail', [.9375, (kitchen_front+entry_front)/2, 1.845],
             [.045, entry_front-kitchen_front, .035])
    # Only the kitchen is managed in F1. Neighbor-follow rules and other unit editors are later slices.
    e['furnitureUnits'] = [kitchen]
    x['file']['schemaVersion'] = 22
    # IDs, route/control/check payloads, geometry outside the reviewed set remain unchanged.
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('x') as f:
        json.dump(x, f, indent=2); f.write('\n')
    run = subprocess.run([str(tool), 'inspect', str(output)], capture_output=True, text=True)
    if run.returncode:
        output.unlink()
        raise ValueError('Native validation refused the draft: '+run.stderr[:600])
    report = json.loads(run.stdout)
    review = dict(status='provisional_construction_draft_not_measured', source=str(source.resolve()),
                  source_sha256=hashlib.sha256(raw).hexdigest(), changed_ids=sorted(changed),
                  preserved_object_ids=sorted(objects), managed_units=['kitchen_base_unit'],
                  datums_m=dict(entry_front=entry_front, kitchen_front=kitchen_front,
                                tall_front=tall_front, bed_front=bed_front, finished_ceiling=ceiling),
                  limitations=['sink opening is not cut yet', 'wheel-well accommodations require panel features',
                               'only kitchen supports coordinated resizing', 'no automatic neighbor-follow rules',
                               'ceiling, ribs and mounting suitability unmeasured', 'door gaps/hinges still tentative'],
                  native_validation=dict(route_count=report["count"],
                      connection_count=len(report["connections"]), saved_view_count=len(report["savedViews"])))
    with output.with_suffix('.review.json').open('x') as f:
        json.dump(review, f, indent=2); f.write('\n')
    return review


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--tool', required=True, type=Path)
    args = parser.parse_args()
    r = build(args.input, args.output, args.tool)
    print(json.dumps(dict(output=str(args.output), changed_parts=len(r['changed_ids']),
                         managed_units=r['managed_units'], datums_m=r['datums_m'])))
