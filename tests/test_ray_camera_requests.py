"""Camera translation refuses roll, preserves framing, and writes create-only requests."""
import importlib.util
import json
import math
from pathlib import Path
import tempfile
import sys
sys.dont_write_bytecode=True
repo=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('camera_requests',repo/'tools/export_ray_camera_requests.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
sample=dict(position=dict(x=2,y=3,z=1.6),forward=dict(x=1,y=0,z=0),
            up=dict(x=0,y=0,z=1),vertical_fov_degrees=65,near_clip=.01,far_clip=250)
request=module.build_request(sample,'../scene/scene_runtime.json','renders/one',960,540,'one')
assert request['inspection']['camera_look_at']==dict(x=3,y=3,z=1.6)
zoom=request['inspection']['camera_zoom']
assert abs(2*math.atan(math.tan(math.radians(27.5))/zoom)-math.radians(65))<1e-12
for bad in [dict(sample,up=dict(x=0,y=1,z=0)),dict(sample,vertical_fov_degrees=float('nan')),
            dict(sample,forward=dict(x=0,y=0,z=1),up=dict(x=1,y=0,z=0))]:
    try:module.build_request(bad,'runtime.json','out',960,540,'bad')
    except ValueError:pass
    else:raise AssertionError('unsupported camera accepted')
with tempfile.TemporaryDirectory(prefix='ray-camera-requests-') as tmp:
    root=Path(tmp);src=root/'scene';src.mkdir()
    (src/'scene_runtime.json').write_text('{}')
    doc=dict(schema='line_drawing_camera_samples_v1',width=960,height=540,anchors=[sample],frames=[sample,sample])
    (src/'camera_samples.json').write_text(json.dumps(doc))
    assert module.export(src,root/'requests','anchors')==1
    one=json.loads((root/'requests/anchors_0000.request.json').read_text())
    assert (root/'requests'/one['scene']['runtime_scene_path']).resolve()==(src/'scene_runtime.json').resolve()
    assert module.export(src,root/'frames','frames')==2
    before=(root/'requests/anchors_0000.request.json').read_bytes()
    try:module.export(src,root/'requests','anchors')
    except FileExistsError:pass
    else:raise AssertionError('existing destination accepted')
    assert before==(root/'requests/anchors_0000.request.json').read_bytes()
    doc['anchors'].append(dict(sample,up=dict(x=0,y=1,z=0)))
    (src/'camera_samples.json').write_text(json.dumps(doc))
    try:module.export(src,root/'bad','anchors')
    except ValueError:pass
    else:raise AssertionError('rolled batch accepted')
    assert not (root/'bad').exists()
print('optiC camera request position/FOV, relative paths, batch validation and refusal checks passed')
