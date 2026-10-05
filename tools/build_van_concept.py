#!/usr/bin/env python3
"""Compose a native engineering document from explicit, provisional concept parts.

This authoring input is app-local, not the general agent request schema. The input uses legacy schema 19; native APIs migrate to the current schema,
derive motion and compile exports.
New output directories only: regenerating never overwrites an edited project.
"""
import argparse
import copy
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FRAME = [0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1]


def info(item):
    props = {'dimensions_status': 'concept_not_measured', 'subsystem': item.get('parent') or 'van',
             'source': 'user_ai_concept_reinterpreted'}
    props.update(item.get('properties', {}))
    return dict(id=item['id'], name=item['name'], type=item.get('type', 'Assembly'),
                parent=item.get('parent', ''), reference=item.get('reference', False),
                volumeRole=item.get('volume_role', 0), volumeOwner=item.get('owner', ''),
                properties=[dict(key=k, kind=0, value=v) for k, v in props.items()])


def compose(request):
    if request['schema'] != 'line_drawing_van_layout_concept_v1':
        raise ValueError('Unsupported concept input')
    expected_frame = dict(origin='cargo_floor_center', x_positive='passenger',
                          y_positive='front', z_positive='up', unit='m')
    if request['frame'] != expected_frame:
        raise ValueError('Concept frame requires explicit conversion before authoring')
    template = json.loads((ROOT / 'config/examples/illustrative_van.layout.json').read_text())
    template['objects3d'] = []
    template['file']['gridSize'] = .05
    template['geometricConstraints'] = []
    template['nextConstraintId'] = 1
    template['engineering'] = dict(nextAssemblyId=1, entities=[], assemblies=[], relationships=[],
        nextRelationshipId=1, spatialChecks=[], nextSpatialCheckId=1, motionEnvelopes=[])
    template['scene3d']['bounds'].update(min=dict(x=-1.05, y=-2.15, z=-.1),
                                         max=dict(x=1.05, y=2.15, z=2.05))
    for a in request['assemblies']:
        template['engineering']['assemblies'].append(dict(**info(a), worldFrame=FRAME))
    for index, part in enumerate(request['parts'], 1):
        pos = dict(zip('xyz', part['center_m']))
        dims = dict(zip(('width', 'height', 'depth'), part['size_m']))
        template['objects3d'].append(dict(id=index, persistentId=part['id'],
            flags=dict(visible=part.get('visible', True), locked=False, selectable=True),
            kind='rect_prism', objectType='rect_prism_primitive', dimensionalMode='full_3d',
            lockedPlane='xy', transform=dict(position=pos, rotationDeg=dict(x=0,y=0,z=0),
            scale=dict(x=1,y=1,z=1)), rectPrism=dict(**dims, lockToConstructionPlane=False,
            lockToBounds=False, frame=dict(origin=pos, axisU=dict(x=1,y=0,z=0),
            axisV=dict(x=0,y=1,z=0), normal=dict(x=0,y=0,z=1)))))
        template['engineering']['entities'].append(info(part))
    for i, relation in enumerate(request['relationships'], 1):
        template['engineering']['relationships'].append(dict(id=f'relationship_{i}', **relation))
    template['engineering']['nextRelationshipId'] = len(request['relationships']) + 1
    rule = copy.deepcopy(json.loads((ROOT / 'config/examples/illustrative_van.layout.json').read_text())['geometricConstraints'][0])
    travel = request['bed_travel_m']
    rule.update(target=travel['home'], motionAssembly='bed', travelMin_m=travel['min'],
                travelMax_m=travel['max'], travelHome_m=travel['home'])
    template['geometricConstraints'] = [rule]
    return template


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--request', type=Path, default=ROOT / 'config/concepts/van_layout_v1.json')
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--tool', type=Path, default=ROOT / 'build/toolchains/clang/bin/van_concept_tool')
    args = parser.parse_args()
    if args.out.exists():
        parser.error('Output exists; choose a new directory to preserve manual edits')
    request = json.loads(args.request.read_text())
    scene_id = request['scene_id']
    if not re.fullmatch(r'[a-z][a-z0-9_]{0,79}', scene_id):
        parser.error('Scene ID must be a lowercase identifier, not a path')
    status = request['status']
    if status not in ('concept_not_measured', 'published_nominal_with_provisional_geometry'):
        parser.error('Unsupported dimension provenance status')
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.van-concept-', dir=args.out.parent) as tmp:
        staging = Path(tmp)
        draft = staging / 'concept_draft.layout.json'
        draft.write_text(json.dumps(compose(request), indent=2, allow_nan=False) + '\n')
        targets = request['bed_check_targets']
        subprocess.run([str(args.tool.resolve()), str(draft), str(staging),
            '--profile', scene_id, status, *targets], check=True)
        (staging / 'concept_request.json').write_text(json.dumps(request, indent=2) + '\n')
        draft.unlink()
        # Directory publish happens only after native validation and export succeed.
        staging.rename(args.out)
    print(args.out.resolve())


if __name__ == '__main__':
    main()
