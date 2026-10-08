#!/usr/bin/env python3
"""Translate native, unrolled camera samples into existing optiC still requests.

Does not render or alter either scene. Roll and clip parity require consumer
support; unsupported roll is refused, clip limitations are recorded explicitly.
"""
import argparse
import json
import math
import os
from pathlib import Path


def build_request(sample, runtime, output, width, height, run_id):
    position, forward, up = (sample[k] for k in ('position', 'forward', 'up'))
    vectors = [[v[k] for k in ('x', 'y', 'z')] for v in (position, forward, up)]
    if not all(math.isfinite(x) for v in vectors for x in v):
        raise ValueError('nonfinite camera sample')
    f, u = vectors[1:]
    if abs(sum(x*x for x in f)-1)>1e-4 or abs(sum(x*x for x in u)-1)>1e-4:
        raise ValueError('non-unit camera frame')
    horizontal=math.hypot(f[0],f[1])
    if horizontal<1e-6:
        raise ValueError('vertical camera cannot be represented unambiguously')
    expected=(-f[0]*f[2]/horizontal,-f[1]*f[2]/horizontal,horizontal)
    if max(abs(a-b) for a,b in zip(expected,u))>1e-4:
        raise ValueError('camera roll needs optiC orientation-frame ingestion')
    fov=sample['vertical_fov_degrees']
    if not math.isfinite(fov) or not 1<=fov<=179:
        raise ValueError('invalid FOV')
    return dict(schema_version='ray_tracing_agent_render_request_v1',run_id=run_id,
        scene=dict(runtime_scene_path=runtime),volume=dict(enabled=False),
        render=dict(start_frame=0,frame_count=1,width=width,height=height,normalized_t=0,
                    temporal_frames=1,integrator_3d='direct_light',denoise_enabled=False),
        resources=dict(cpu_percent=50,max_workers=2,reserve_cpu_count=1),
        inspection=dict(camera_position=position,
            camera_look_at={k:position[k]+forward[k] for k in ('x','y','z')},
            # Existing native camera uses tan(55deg/2)/zoom for vertical projection.
            camera_zoom=math.tan(math.radians(55/2))/math.tan(math.radians(fov/2)),
            environment_light_mode='ambient',ambient_strength=.25,
            environment_preset='neutral',background_brightness=.35),
        output=dict(root=output,overwrite=False))


def export(source, destination, kind):
    source=source.resolve();destination=destination.resolve()
    doc=json.loads((source/'camera_samples.json').read_text())
    if doc['schema']!='line_drawing_camera_samples_v1':
        raise ValueError('unsupported camera samples schema')
    if not (source/'scene_runtime.json').is_file():
        raise ValueError('missing compiled runtime scene')
    runtime=os.path.relpath(source/'scene_runtime.json',destination)
    requests=[]
    for i,sample in enumerate(doc[kind]):
        name=f'{kind}_{i:04d}'
        requests.append((name,build_request(sample,runtime,f'renders/{name}',doc['width'],doc['height'],name)))
    # Validate the complete batch before creating any output.
    destination.mkdir()
    for name,request in requests:
        with (destination/f'{name}.request.json').open('x') as f:
            json.dump(request,f,indent=2,allow_nan=False);f.write('\n')
    (destination/'README.txt').write_text(
        f'{len(requests)} ordinary optiC still requests from {kind}.\n'
        'Use ray_tracing_render_headless --request <file> --preflight, then --render.\n'
        'Positions, look directions and vertical FOV are adapted; roll is refused.\n'
        'Native near/far clipping is retained in camera_samples.json but cannot be overridden by this request API.\n'
        'Ambient inspection fill is provisional. Full renderer image parity is not claimed.\n')
    return len(requests)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('scene_export',type=Path)
    parser.add_argument('new_output_directory',type=Path)
    parser.add_argument('--samples',choices=['anchors','frames'],default='anchors')
    args=parser.parse_args()
    print(f'Exported {export(args.scene_export,args.new_output_directory,args.samples)} optiC requests')
