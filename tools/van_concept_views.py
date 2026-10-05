#!/usr/bin/env python3
"""Draw orthographic review diagrams from the authored concept, never from AI pixels."""
import html
import json
from pathlib import Path
import sys

request = json.loads(Path(sys.argv[1]).read_text())
output = Path(sys.argv[2])
parts = {p['id']: p for p in request['parts']}
W, L, H = (request['shell_m'][k] for k in ('width','length','height'))
svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="1240" height="1040" viewBox="0 0 1240 1040">',
       '<rect width="1240" height="1040" fill="#f5f7fa"/>',
       '<style>text{font-family:Arial,sans-serif;fill:#173045} .small{font-size:14px} .label{font-size:16px;font-weight:600}</style>']

def text(x,y,s,size=16,**kw):
    attrs=' '.join(f'{k}="{v}"' for k,v in kw.items())
    svg.append(f'<text x="{x}" y="{y}" font-size="{size}" {attrs}>{html.escape(s)}</text>')

def rect(x,y,w,h,fill,stroke='#526c7e',dash=False,opacity=1):
    d=' stroke-dasharray="7 5"' if dash else ''
    svg.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" fill="{fill}" stroke="{stroke}" stroke-width="1.5" opacity="{opacity}"{d}/>')

def plan(p,fill,stroke='#526c7e',dash=False,opacity=1):
    x,y,z=p['center_m'];w,l,h=p['size_m']
    rect(95+(x-w/2+W/2)*180,145+(L/2-y-l/2)*180,w*180,l*180,fill,stroke,dash,opacity)

nominal = 'dimension_basis' in request
text(42,42,'ProMaster 3500 — nominal reference layout' if nominal else 'Van layout concept v1 — corrected spatial interpretation',27)
text(42,72,'Likely 2023 High Roof / 159 extended body • factory maxima, box proxies and provisional furniture' if nominal else 'Provisional geometry • meters internally • no vehicle measurements adopted',16)
text(95,117,'TOP — front / cab ↑',19)
rect(95,145,W*180,L*180,'#fff')
for id in ['driver_bench','storage_driver','storage_passenger','kitchen_base','desk_driver','desk_passenger','drawers_driver','drawers_passenger']:
    color='#ead3a8' if 'desk' in id or 'storage' in id else '#c5dfd1' if id=='driver_bench' else '#cddfee'
    plan(parts[id],color)
plan(parts['walkway'],'#e1efe8',dash=True,opacity=.6)
for id in ['upper_halo_driver','upper_halo_passenger','upper_halo_rear']:
    plan(parts[id],'none','#8454b5',dash=True)
plan(parts['bed_deck'],'#c9dcf5','#30609b',dash=True,opacity=.5)
if nominal:
    for id in ['wheel_well_driver','wheel_well_passenger']:
        plan(parts[id],'none','#ad5c37',dash=True)
plan(parts['sliding_door_access'],'#fcf1c8','#a48a35',dash=True)
text(65,530,'DRIVER',15,transform='rotate(-90 65 530)')
text(461,470,'PASSENGER',15,transform='rotate(90 461 470)')
for id,num in [('driver_bench','1'),('kitchen_base','2'),('storage_driver','3'),('storage_passenger','4'),('bed_deck','5'),('sliding_door_access','6')]:
    p=parts[id];x=95+(p['center_m'][0]+W/2)*180;y=145+(L/2-p['center_m'][1])*180
    svg.append(f'<circle cx="{x}" cy="{y}" r="13" fill="white" stroke="#173045"/>')
    text(x,y+5,num,15,**{'text-anchor':'middle'})
text(95,914,'Rear doors ↓',17)
text(95,941,f'Extents: {W*1000:.0f} × {L*1000:.0f} × {H*1000:.0f} mm',14)
if nominal:
    text(95,969,'Wheel-well gap: 1422 mm nominal',14)
    text(95,994,'Width at bed/roof height: unverified',14)

def elevation(side,top):
    text(565,top-26,side.upper()+' ELEVATION — front left / rear right',18)
    rect(565,top,L*145,H*145,'#fff')
    ids=['driver_bench','driver_cushion','storage_driver','desk_driver','drawers_driver','upper_cabinet_driver'] if side=='driver' else ['kitchen_base','kitchen_worktop','storage_passenger','desk_passenger','drawers_passenger','upper_cabinet_passenger','upper_door_header_cabinet']
    for id in ids:
        p=parts[id];x,y,z=p['center_m'];w,l,h=p['size_m']
        rect(565+(L/2-y-l/2)*145,top+(H-z-h/2)*145,l*145,h*145,'#e7d5b6')
    for id in ['bed_deck','bed_mattress']:
        p=parts[id];x,y,z=p['center_m'];w,l,h=p['size_m']
        rect(565+(L/2-y-l/2)*145,top+(H-z-h/2)*145,l*145,h*145,'#b7cee8','#30609b')
        travel = request['bed_travel_m']
        offset = travel['max'] - travel['home']
        rect(565+(L/2-y-l/2)*145,top+(H-z-offset-h/2)*145,l*145,h*145,'none','#30609b',True)
    rect(565,top+(H-1.92)*145,L*145,.08*145,'none','#8454b5',True)
    text(900,top+75,'Raised bed (dashed)',14)
    text(900,top+164,'Lowered bed',14)
    travel = request['bed_travel_m']
    text(580,top+H*145+22,f"Deck center: {travel['min']*1000:.0f}–{travel['max']*1000:.0f} mm • same horizontal platform in both views",14)

elevation('driver',170)
elevation('passenger',580)
text(565,930,'1 Battery bench / lounge   2 Kitchen   3 Wardrobe   4 Pantry',15)
text(565,958,'5 Single rear lift bed over paired desks   6 Sliding-door entry',15)
text(565,986,'Purple dashed: upper wiring intent; cables/routing are still S5 work.',14)
text(42,1020,'Nominal maxima do not establish local fit; wheel-well boxes combine sources; furniture remains unmeasured.' if nominal else 'AI render zone lengths, voltage labels and structural details are not verified specifications.',14)
svg.append('</svg>')
output.parent.mkdir(parents=True,exist_ok=True)
output.write_text('\n'.join(svg)+'\n')
