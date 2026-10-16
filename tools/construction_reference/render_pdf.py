#!/usr/bin/env python3
"""Render the app's construction reference specification into printable A3 sheets.
Requires reportlab; JSON/schema and completion marker are checked before creation.
Output is create-only. No scene modification or guessed fabrication allowances.
"""
import argparse, hashlib, json, math, os
from pathlib import Path
from xml.sax.saxutils import escape
from reportlab.pdfgen import canvas
from reportlab.lib.pagesizes import A3, landscape
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import Paragraph
from reportlab.lib import colors

BLUE=colors.HexColor('#203b54'); GRAY=colors.HexColor('#5c6975')
W,H=landscape(A3); M=38
STYLE=ParagraphStyle('body',fontName='Helvetica',fontSize=10,leading=13,textColor=BLUE)
SMALL=ParagraphStyle('small',parent=STYLE,fontSize=8,leading=10)
def para(c,text,x,y,width,style=STYLE):
    p=Paragraph(escape(str(text)).replace('\n','<br/>'),style); _,height=p.wrap(width,H);p.drawOn(c,x,y-height);return height

def header(c,r,subtitle,page):
    c.setFillColor(BLUE);c.setFont('Helvetica-Bold',20);c.drawString(M,H-M,r['name'])
    c.setFont('Helvetica',11);c.drawString(M,H-M-22,subtitle);c.drawRightString(W-M,H-M-22,f'Sheet {page}')
    c.setLineWidth(1.5);c.line(M,H-M-34,W-M,H-M-34)
def footer(c):
    c.setStrokeColor(GRAY);c.setLineWidth(.5);c.line(M,37,W-M,37)
    c.setFillColor(GRAY);c.setFont('Helvetica',8);c.drawString(M,24,'MODEL REFERENCE | dimensions in mm | diagrams fit to page, never measure paper | physical verification required before cutting')
    c.showPage()

def projection(c,parts,a,b,box,lo=None,hi=None,labels=False):
    x,y,w,h=box
    if lo is None:lo=[min(p['frameMin_m'][k] for p in parts) for k in range(3)];hi=[max(p['frameMax_m'][k] for p in parts) for k in range(3)]
    span=[hi[k]-lo[k] for k in range(3)];scale=min((w-100)/max(span[a],.001),(h-90)/max(span[b],.001))
    ox=x+65+(w-100-span[a]*scale)/2;oy=y+55+(h-90-span[b]*scale)/2
    for part in parts:
        mesh=part['meshEnvelope'];c.setStrokeColor(colors.HexColor('#517f96' if mesh else '#7a5938'));c.setLineWidth(.65);c.setDash([3,2] if mesh else [])
        points=part['cornersFrame_m'];n=len(points)
        for k in range(n):
            edges=[(k//4)*4+(k+1)%4]+([k+4] if k<4 and n==8 else [])
            for j in edges:c.line(ox+(points[k][a]-lo[a])*scale,oy+(points[k][b]-lo[b])*scale,ox+(points[j][a]-lo[a])*scale,oy+(points[j][b]-lo[b])*scale)
    c.setDash([]);c.setStrokeColor(BLUE);c.setFillColor(BLUE);c.setFont('Helvetica',10)
    c.line(ox,oy-18,ox+span[a]*scale,oy-18)
    for ex in (ox,ox+span[a]*scale):c.line(ex,oy-23,ex,oy-13)
    c.drawCentredString(ox+span[a]*scale/2,oy-36,f'{"UVN"[a]} extent {span[a]*1000:.1f} mm')
    c.line(ox-20,oy,ox-20,oy+span[b]*scale)
    for ey in (oy,oy+span[b]*scale):c.line(ox-25,ey,ox-15,ey)
    c.saveState();c.translate(ox-35,oy+span[b]*scale/2);c.rotate(90);c.drawCentredString(0,0,f'{"UVN"[b]} extent {span[b]*1000:.1f} mm');c.restoreState()
    c.setFont('Helvetica-Bold',11);c.drawString(x+12,y+h-18,f'{"UVN"[a]} / {"UVN"[b]} projection')
    c.setFont('Helvetica',8);c.drawString(x+12,y+8,f'{"UVN"[a]} {lo[a]*1000:.1f} to {hi[a]*1000:.1f}; {"UVN"[b]} {lo[b]*1000:.1f} to {hi[b]*1000:.1f} mm | dashed: STL envelope')

def render(root,output):
    r=json.loads((root/'specification.json').read_text()); snapshot=root/r['snapshot']
    if r['schema']!='sculpt_construction_reference_v1' or r['lengthUnit']!='m' or not (root/'COMPLETE.txt').is_file() or not snapshot.is_file():raise ValueError('Incomplete or unsupported reference export')
    for part in r['parts']:
        values=part['localSize_m']+part['frameMin_m']+part['frameMax_m']+[v for point in part['cornersFrame_m'] for v in point]
        if not all(math.isfinite(v) for v in values):raise ValueError('Nonfinite dimensions')
    fd=os.open(output,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    try:
        with os.fdopen(fd,'wb') as stream:
            c=canvas.Canvas(stream,pagesize=(W,H));c.setTitle(r['name']+' - construction reference');c.setAuthor('sCulpt Main Edit');page=1
            header(c,r,'Scope, frame, provenance and build review',page);page+=1;y=H-M-62
            items=[r['qualification'],f"Scope {r['collection']} | {len(r['parts'])} modeled parts | {sum(p['meshEnvelope'] for p in r['parts'])} STL envelopes | {r['excluded']} excluded members",f"Frame entity: {r['frameEntity'] or 'World'} | origin world mm: "+' / '.join(f'{v*1000:.1f}' for v in r['frameOriginWorld_m']),f"Source label: {r['sourceLabel']}",f"Exact exported snapshot SHA-256: {hashlib.sha256(snapshot.read_bytes()).hexdigest()}",r['pose'],r['dimensionMeaning']]
            for item in items:y-=para(c,item,M,y,W-2*M)+12
            for k in range(3):
                ax='UVN'[k];v=r['axis'+ax];s=f'{ax}: {r["frameMin_m"][k]*1000:.1f} to {r["frameMax_m"][k]*1000:.1f} mm; extent {(r["frameMax_m"][k]-r["frameMin_m"][k])*1000:.1f}; world direction {v}'
                y-=para(c,s,M,y,W-2*M)+8
            y-=15
            for item in ['BUILD REVIEW REQUIRED','The source includes concept_not_measured and tentative material entries where authored. This packet is a model-derived draft, not an as-built or approved cut list.','1. Check wall/floor/ceiling datums and actual cabinet fit.','2. Select stock material, thickness, grain, edge treatment and finish.','3. Specify joints, fasteners, attachment locations, cut allowances and tolerances.','4. Confirm hinge/slide hardware dimensions, travel, opening sweeps and service access.','5. Review each modeled part and sign off dimensions before fabrication.','Geometry in the snapshot may retain external mesh asset paths. Keep the original project assets; this is not a portable mesh bundle.']:
                y-=para(c,item,M,y,W-2*M)+9
            footer(c)
            for a,b in ((0,1),(0,2),(1,2)):
                header(c,r,'Complete area placement - frame axes, current pose',page);page+=1;projection(c,r['parts'],a,b,(M,55,W-2*M,H-M-140),r['frameMin_m'],r['frameMax_m']);footer(c)
            # Durable assembly identity defines grouping; labels are never used to infer membership.
            groups={}
            for part in r['parts']:
                if part['assembly']:groups.setdefault(part['assembly'],[]).append(part)
            for identity,parts in groups.items():
                header(c,r,'Assembly detail: '+(parts[0]['assemblyName'] or identity),page);page+=1
                para(c,f'Assembly {identity} | parts '+', '.join(f'P{p["partNumber"]:03}' for p in parts),M,H-M-53,W-2*M,SMALL)
                width=(W-2*M)/3
                for i,(a,b) in enumerate(((0,1),(0,2),(1,2))):projection(c,parts,a,b,(M+i*width,105,width-10,H-M-205))
                para(c,'Assembly outline includes only its directly parented members in this collection. Coordinates remain in the collection frame. Whole-area pages include all descendants and direct collection members.',M,85,W-2*M,SMALL);footer(c)
            header(c,r,'Saved measurement schedule - evaluation in exported pose',page);page+=1;y=H-M-60
            features=['origin','envelope min corner','envelope center','envelope max corner'];kinds=['U extent','V extent','N extent','Point distance','Signed U projection','Signed V projection','Signed N projection']
            for m in r['measurements']:
                if y<150:footer(c);header(c,r,'Saved measurements (continued)',page);page+=1;y=H-M-60
                value=f'{m["value_m"]*1000:.3f} mm' if 'value_m' in m else m['status']
                y-=para(c,f'{m["name"]}: {value} | {m["id"]}',M,y,W-2*M)+6
                y-=para(c,f'{kinds[m["kind"]]} | frame {m["frameEntity"] or "World"}\nA: {m["datumA"] or "not used"} ({features[m["featureA"]]})\nB: {m["datumB"] or "not used"} ({features[m["featureB"]]})',M,y,W-2*M,SMALL)+20
            para(c,'Envelope datum distances are reference operations, not minimum face clearances. Invalid references remain explicit and have no numeric result.',M,90,W-2*M,SMALL);footer(c)
            header(c,r,'Modeled parts and draft build specification',page);page+=1;y=H-M-55
            widths=[270,190,230,W-2*M-690];xs=[M,M+270,M+460,M+690]
            def tablehead(y):
                c.setFillColor(colors.HexColor('#e8edf1'));c.rect(M,y-25,W-2*M,25,fill=1,stroke=0)
                for x,t in zip(xs,['Part / identity / assembly','Local U / V / N mm','Frame min / max U / V / N mm','Authored properties / build review']):para(c,t,x+5,y-6,W-2*M if x==M else widths[xs.index(x)]-10,SMALL)
                return y-30
            y=tablehead(y)
            for part in r['parts']:
                props=[]
                for prop in part['authoredProperties']:props.append(f'{prop["key"]}: '+str(prop.get('text',prop.get('value','')))+((' m') if prop.get('unit')=='m' else ''))
                if not props:props=['No physical stock specification authored.']
                props+=['Stock / joints / tolerance / hardware fit: REVIEW']
                cells=[f'P{part["partNumber"]:03}  {part["name"]}\n{part["entity"]}\n{part["assemblyName"] or "Direct collection member"}', ' / '.join(f'{v*1000:.1f}' for v in part['localSize_m'])+'\n'+part['geometryBasis']+'\nQuantity: 1 modeled instance', ' / '.join(f'{v*1000:.1f}' for v in part['frameMin_m'])+'\n'+' / '.join(f'{v*1000:.1f}' for v in part['frameMax_m']), '\n'.join(props)]
                heights=[]
                for text,width in zip(cells,widths):q=Paragraph(escape(text).replace('\n','<br/>'),SMALL);heights.append(q.wrap(width-12,H)[1])
                height=max(heights)+18
                if y-height<60:footer(c);header(c,r,'Modeled parts and draft build specification (continued)',page);page+=1;y=tablehead(H-M-55)
                for text,x,width in zip(cells,xs,widths):para(c,text,x+5,y-6,width-12,SMALL)
                c.setStrokeColor(colors.HexColor('#d5dce2'));c.setLineWidth(.4);c.line(M,y-height,W-M,y-height);y-=height
            footer(c);c.save()
    except BaseException:
        output.unlink(missing_ok=True)  # Only this invocation's exclusive new output.
        raise
    return page-1
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('reference_directory',type=Path);parser.add_argument('new_pdf',type=Path);args=parser.parse_args()
    pages=render(args.reference_directory,args.new_pdf);print(json.dumps({'pdf':str(args.new_pdf),'pages':pages,'sha256':hashlib.sha256(args.new_pdf.read_bytes()).hexdigest()}))
