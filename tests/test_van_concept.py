#!/usr/bin/env python3
"""Concept acceptance: native pose/reload/export, sides, clearance and safe rebuild."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TOOL = Path(sys.argv[1]).resolve()
REQUEST = ROOT / 'config/concepts/van_layout_v1.json'


def build(out, request=REQUEST, success=True):
    result = subprocess.run([sys.executable, str(ROOT / 'tools/build_van_concept.py'),
        '--tool', str(TOOL), '--request', str(request), '--out', str(out)],
        capture_output=True, text=True)
    assert (result.returncode == 0) == success, result.stderr


with tempfile.TemporaryDirectory(prefix='ld-concept-smoke-') as tmp:
    root = Path(tmp)
    a, b = root / 'first', root / 'second'
    build(a); build(b)
    for name in ('van_layout_concept_v1.layout.json', 'scene_authoring.json', 'scene_runtime.json', 'validation.json'):
        assert (a / name).read_bytes() == (b / name).read_bytes(), name
    layout = json.loads((a / 'van_layout_concept_v1.layout.json').read_text())
    objects = {o['persistentId']: o for o in layout['objects3d']}
    assert objects['battery']['transform']['position']['x'] < 0
    assert objects['kitchen_worktop']['transform']['position']['x'] > 0
    assert objects['bed_deck']['rectPrism']['frame']['normal'] == {'x': 0, 'y': 0, 'z': 1}
    assert len(layout['engineering']['motionEnvelopes']) == 1
    assert layout['geometricConstraints'][0]['motionAssembly'] == 'bed'
    assert layout['physicalContext']['metersPerWorldUnit'] == 1
    assert not any('obstruction' in o for o in objects), 'Teaching obstacles leaked into design'
    validation = json.loads((a / 'validation.json').read_text())
    assert validation['nativeReloadAndTravelPassed']
    assert [round(p['deckCenter_m'], 4) for p in validation['motionPoseChecks']] == [1.65, .95]
    assert [round(p['mattressCenter_m'], 4) for p in validation['motionPoseChecks']] == [1.74, 1.04]
    assert (a / 'van_layout_concept_v1.layout.json').read_bytes() == (ROOT / 'config/examples/van_layout_concept_v1.layout.json').read_bytes()
    assert len(validation['results']) == 12
    assert all(r['severity'] == 'pass' for r in validation['results']), validation
    document = a / 'van_layout_concept_v1.layout.json'
    document.write_text('manual edits must survive\n')
    build(a, success=False)
    assert document.read_text() == 'manual edits must survive\n'
    invalid = root / 'invalid.json'
    request = json.loads(REQUEST.read_text())
    request['parts'][0]['parent'] = 'missing_assembly'
    invalid.write_text(json.dumps(request))
    build(root / 'failed', invalid, success=False)
    assert not (root / 'failed').exists(), 'Invalid candidate published'
    nominal_request = ROOT / 'config/concepts/van_layout_promaster_2023_nominal_v1.json'
    nominal_id = 'van_layout_promaster_2023_nominal_v1'
    n1, n2 = root / 'nominal_first', root / 'nominal_second'
    build(n1, nominal_request); build(n2, nominal_request)
    for name in (nominal_id + '.layout.json', 'scene_authoring.json', 'scene_runtime.json', 'validation.json'):
        assert (n1 / name).read_bytes() == (n2 / name).read_bytes(), name
    nominal = json.loads((n1 / (nominal_id + '.layout.json')).read_text())
    objs = {o['persistentId']: o for o in nominal['objects3d']}
    floor = objs['oem_floor']['rectPrism']
    assert abs(floor['width'] - 1.920) < 1e-6 and abs(floor['height'] - 4.097) < 1e-6
    driver = objs['wheel_well_driver']['rectPrism']
    passenger = objs['wheel_well_passenger']['rectPrism']
    gap = passenger['frame']['origin']['x'] - passenger['width']/2 - (driver['frame']['origin']['x'] + driver['width']/2)
    assert abs(gap - 1.422) < 1e-6
    opening = objs['oem_side_opening_size']['rectPrism']
    assert abs(opening['height'] - 1.250) < 1e-6 and abs(opening['depth'] - 1.755) < 1e-6
    for id in ('bed_deck', 'bed_mattress', 'battery', 'kitchen_worktop'):
        assert objs[id] == objects[id], 'Concept furniture was silently resized or relocated'
    entities = {e['id']: e for e in nominal['engineering']['entities']}
    status = {p['key']:p['value'] for p in entities['oem_floor']['properties']}
    assert status['dimensions_status'] == 'mixed_nominal_and_proxy'
    assert json.loads((n1 / 'scene_authoring.json').read_text())['scene_id'] == nominal_id
    report = json.loads((n1 / 'validation.json').read_text())
    assert report['dimensionsStatus'] == 'published_nominal_with_provisional_geometry'
    assert report['nativeReloadAndTravelPassed'] and len(report['results']) == 12
    assert (n1 / (nominal_id + '.layout.json')).read_bytes() == (ROOT / 'config/examples' / (nominal_id + '.layout.json')).read_bytes()
    request['scene_id'] = '../outside'
    invalid.write_text(json.dumps(request))
    build(root / 'invalid_identity', invalid, success=False)
    assert not (root / 'invalid_identity').exists()
print('van-concept-smoke passed: deterministic native motion/export, corrected sides, checks, overwrite refusal and invalid-input isolation')
