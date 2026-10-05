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
print('van-concept-smoke passed: deterministic native motion/export, corrected sides, checks, overwrite refusal and invalid-input isolation')
