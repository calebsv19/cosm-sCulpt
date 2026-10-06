#!/usr/bin/env python3
"""Create a separate F2 connected-run draft from an unmodified F1 van draft."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess


def build(source, output, tool):
    if output.exists() or output.with_suffix('.review.json').exists():
        raise ValueError('Output/review already exists; choose a new draft name')
    raw = source.read_bytes()
    scene = json.loads(raw)
    if scene['physicalContext'] != {'coordinateSystem': 'right_handed_z_up_meters', 'metersPerWorldUnit': 1}:
        raise ValueError('This fixture requires the existing meter/Z-up frame')
    e = scene['engineering']
    if e.get('furnitureContacts') or len(e.get('furnitureUnits', [])) != 1 or e['furnitureUnits'][0]['assembly'] != 'kitchen_base_unit':
        raise ValueError('Expected an F1 kitchen-only draft without existing driving links')
    check = subprocess.run([str(tool), 'inspect', str(source)], capture_output=True, text=True)
    if check.returncode:
        raise ValueError('Input failed native validation: '+check.stderr[:400])
    x = copy.deepcopy(scene)
    e = x['engineering']
    objects = {o['persistentId']: o for o in x['objects3d']}
    infos = {v['id']: v for v in e['entities']}
    assemblies = {a['id']: a for a in e['assemblies']}

    def recipe(prefix):
        assembly = prefix+'_unit'
        if assemblies[assembly]['worldFrame'] != [0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1]:
            raise ValueError('Moved unit needs manual conversion: '+assembly)
        parts = [(prefix, 0), (prefix+'_end_rear', 1), (prefix+'_end_front', 2),
                 (prefix+'_bottom', 3), (prefix+'_top', 4), (prefix+'_door', 5)]
        if prefix+'_shelf' in objects:
            parts.append((prefix+'_shelf', 6))
        if prefix+'_mount_rail' in objects:
            parts.append((prefix+'_mount_rail', 9))
        for oid, _ in parts:
            o = objects[oid]
            if infos[oid]['parent'] != assembly or o['kind'] != 'rect_prism' or o['transform']['scale'] != dict(x=1, y=1, z=1):
                raise ValueError('Unsupported construction member: '+oid)
            if o['rectPrism']['frame']['axisU'] != dict(x=1, y=0, z=0) or o['rectPrism']['frame']['axisV'] != dict(x=0, y=1, z=0) or o['rectPrism']['frame']['normal'] != dict(x=0, y=0, z=1):
                raise ValueError('Rotated member needs manual conversion: '+oid)
        back, door = objects[prefix], objects[prefix+'_door']
        sign = 1 if back['transform']['position']['x'] > 0 else -1
        wall = back['transform']['position']['x']+sign*back['rectPrism']['width']/2
        front = door['transform']['position']['x']-sign*door['rectPrism']['width']/2
        center = [(wall+front)/2, back['transform']['position']['y'], back['transform']['position']['z']]
        u = dict(assembly=assembly, backSign=sign, centerMeters=center,
                 sizeMeters=[abs(wall-front), back['rectPrism']['height'], back['rectPrism']['depth']],
                 thicknessMeters=[objects[prefix+'_end_front']['rectPrism']['height'], back['rectPrism']['width'], door['rectPrism']['width'], .04],
                 overhangMeters=[0, 0, 0, 0], shelfFraction=.5, parts=[])
        for oid, role in parts:
            offset = [0, 0, 0]
            span = [0, 0, 0]
            if role == 9:
                offset = [objects[oid]['transform']['position'][k]-center[i] for i, k in enumerate('xyz')]
                span = [objects[oid]['rectPrism']['width'], 0, objects[oid]['rectPrism']['depth']]
            u['parts'].append(dict(entity=oid, role=role, offsetMeters=offset, spanMeters=span))
        return u

    e['furnitureUnits'].extend([recipe('storage_passenger'), recipe('upper_cabinet_passenger')])
    e['furnitureContacts'] = [
        dict(id='furniture_fit_1', driver='kitchen_base_unit', driverEnd=-1,
             follower='storage_passenger_unit', followerEnd=1, behavior=0, gapMeters=0, enabled=True),
        dict(id='furniture_fit_2', driver='storage_passenger_unit', driverEnd=1,
             follower='upper_cabinet_passenger_unit', followerEnd=-1, behavior=1, gapMeters=0, enabled=True)]
    e['nextFurnitureContactId'] = 3
    x['file']['schemaVersion'] = 23
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('x') as f:
        json.dump(x, f, indent=2); f.write('\n')
    validated = subprocess.run([str(tool), 'inspect', str(output)], capture_output=True, text=True)
    if validated.returncode:
        output.unlink()
        raise ValueError('Native validation refused contact draft: '+validated.stderr[:600])
    review = dict(status='provisional_connected_run_draft', source=str(source.resolve()),
                  source_sha256=hashlib.sha256(raw).hexdigest(), geometry_changed=False,
                  managed_units=[u['assembly'] for u in e['furnitureUnits']],
                  contact_count=2, preserved_routes=len(e['routes']),
                  limitations=['contact planes only, not structural attachment',
                               'pantry follows kitchen and may move into reserved bed or wheel-well areas; run Checks',
                               'sink opening and wheel-well cutouts remain unfinished',
                               'driver-side run is not yet managed', 'measure actual vehicle before construction'])
    with output.with_suffix('.review.json').open('x') as f:
        json.dump(review, f, indent=2); f.write('\n')
    return review


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--tool', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(build(args.input, args.output, args.tool)))
