#!/usr/bin/env python3
"""Render the explicit concept input as a dimensioned top-view SVG; no AI geometry."""
import argparse
from html import escape
import json
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--request',required=True,type=Path)
parser.add_argument('--out',required=True,type=Path)
args=parser.parse_args()
r=json.loads(args.request.read_text())
width,length=r['shell_m']['width'],r['shell_m']['length']
scale=250
ox,oy=110,150
front=length/2
items=['<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="820" viewBox="0 0 1280 820">',
'<rect width="1280" height="820" fill="#101a25"/>',
'<style>text{fill:#e7edf5;font-family:Arial,sans-serif;font-size:16px}.small{font-size:13px}.muted{fill:#a8b9cb}</style>',
'<text x="48" y="42" font-size="25">ProMaster narrow layout v2 — top view</text>',
'<text x="48" y="70" class="muted">Layout assumptions, not a measured vehicle. Front left / rear right. Driver above / passenger below.</text>']
def xy(x,y):return ox+(front-y)*scale,oy+(x+width/2)*scale
sx,sy=xy(-width/2,front)
items.append(f'<rect x="{sx}" y="{sy}" width="{length*scale}" height="{width*scale}" fill="#182b39" stroke="#a8b9cb" stroke-width="2"/>')
for z in r['layout_zones']:
 x,_=xy(0,z['front_y_m']);end,_=xy(0,z['rear_y_m']);mid=(x+end)/2
 items.append(f'<line x1="{x}" y1="110" x2="{x}" y2="{sy+width*scale}" stroke="#718498" stroke-dasharray="5 6"/>')
 title=z['label'].split(' - ')[0]
 items.append(f'<text x="{mid}" y="110" text-anchor="middle">{escape(title)}</text>')
 items.append(f'<text x="{mid}" y="134" class="small" text-anchor="middle">{z["length_m"]*1000:.1f} mm</text>')
palette={'electrical':'#577651','kitchen':'#93693e','storage':'#88604d','desks':'#6a718f','bed':'#70529a','reserved':'#197878'}
for part in r['parts']:
 parent=part['parent'];id=part['id']
 if id not in ('driver_bench','kitchen_base','storage_driver','storage_passenger','desk_driver','desk_passenger','bed_deck','sliding_door_access','walkway'):
  continue
 x,y,z=part['center_m'];w,h,d=part['size_m'];px,py=xy(x-w/2,y+h/2)
 alpha='.22' if id=='bed_deck' else '.3' if parent=='reserved' else '.85'
 dash=' stroke-dasharray="8 6"' if id=='bed_deck' else ''
 items.append(f'<rect x="{px}" y="{py}" width="{h*scale}" height="{w*scale}" fill="{palette[parent]}" fill-opacity="{alpha}" stroke="#dcdae4"{dash}/>')
 labels={'driver_bench':'Battery / lounge','kitchen_base':'Kitchen','storage_driver':'Wardrobe','storage_passenger':'Pantry','desk_driver':'Desk','desk_passenger':'Desk','bed_deck':'Bed 1800 × 820 mm','sliding_door_access':'Entry','walkway':'Aisle'}
 cx,cy=xy(x,y)
 if id=='bed_deck':cy=oy+width*scale/2
 items.append(f'<text x="{cx}" y="{cy+5}" text-anchor="middle" class="small">{labels[id]}</text>')
items.extend(['<text x="110" y="668">Maximum floor extents: 4097 × 1920 mm nominal; planning sections total 3880 mm (provisional).</text>',
'<text x="110" y="698">Bed: transverse sleeping length; narrow 820 mm front-to-back width. Maximum permitted width: 914.4 mm.</text>',
'<text x="110" y="728">Dashed bed footprint sits above the paired desks; deck-center Z travel remains 950–1650 mm.</text>',
'<text x="110" y="758" class="muted">Measure wall/rib clearance at bed heights before confirming the 1800 mm sleeping length or hardware fit.</text>', '</svg>'])
args.out.parent.mkdir(parents=True,exist_ok=True)
args.out.write_text('\n'.join(items)+'\n')
