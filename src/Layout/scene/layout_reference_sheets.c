#include "Layout/layout_construction_reference.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static const cJSON* field(const cJSON* row,const char* key){return cJSON_GetObjectItemCaseSensitive(row,key);}
static const char* text(const cJSON* row,const char* key){const cJSON* v=field(row,key);return cJSON_IsString(v)?v->valuestring:"";}
static double number(const cJSON* row,const char* key){const cJSON* v=field(row,key);return cJSON_IsNumber(v)?v->valuedouble:0;}
static double coord(const cJSON* row,const char* key,int axis){return cJSON_GetArrayItem(field(row,key),axis)->valuedouble;}
/* All document strings are escaped; model labels cannot inject markup or scripts. */
static void escaped(FILE* f,const char* s){for(;*s;++s)switch(*s){case '&':fputs("&amp;",f);break;case '<':fputs("&lt;",f);break;case '>':fputs("&gt;",f);break;case '"':fputs("&quot;",f);break;case '\'':fputs("&#39;",f);break;default:fputc(*s,f);}}
static FILE* output(const char* dir,const char* leaf){char path[4096];if(snprintf(path,sizeof(path),"%s/%s",dir,leaf)>=(int)sizeof(path))return NULL;return fopen(path,"wx");}
static bool svg(FILE* f,const cJSON* root,int a,int b,const char* assembly) {
    double loA=coord(root,"frameMin_m",a),loB=coord(root,"frameMin_m",b),hiA=coord(root,"frameMax_m",a),hiB=coord(root,"frameMax_m",b);
    if(assembly){loA=loB=INFINITY;hiA=hiB=-INFINITY;const cJSON* part;cJSON_ArrayForEach(part,field(root,"parts")){if(strcmp(text(part,"assembly"),assembly))continue;loA=fmin(loA,coord(part,"frameMin_m",a));loB=fmin(loB,coord(part,"frameMin_m",b));hiA=fmax(hiA,coord(part,"frameMax_m",a));hiB=fmax(hiB,coord(part,"frameMax_m",b));}}
    double spanA=hiA-loA,spanB=hiB-loB;
    double scale=fmin(830/fmax(spanA,.001),490/fmax(spanB,.001)),x=70+(830-spanA*scale)/2,y=90+(490-spanB*scale)/2;
    fprintf(f,"<svg xmlns='http://www.w3.org/2000/svg' width='1000' height='700' viewBox='0 0 1000 700'><rect width='1000' height='700' fill='white'/><g font-family='Helvetica,Arial,sans-serif' fill='#18283b'><text x='40' y='30' font-size='20'>");escaped(f,text(root,"name"));fprintf(f," - %c/%c projection</text><text x='40' y='55' font-size='13'>Model geometry in mm | numbered parts in inventory | dashed = STL envelope</text></g>\n","UVN"[a],"UVN"[b]);
    const cJSON* row; cJSON_ArrayForEach(row,field(root,"parts")) {
        if(assembly&&strcmp(text(row,"assembly"),assembly))continue;
        const cJSON* points=field(row,"cornersFrame_m");int count=cJSON_GetArraySize(points);bool mesh=cJSON_IsTrue(field(row,"meshEnvelope"));
        fprintf(f,"<g fill='none' stroke='%s' stroke-width='1.1' %s><title>",mesh?"#507d96":"#785634",mesh?"stroke-dasharray='4 3'":"");escaped(f,text(row,"name"));fputs("</title>",f);
        /* Native corner ordering: lower face 0..3, upper face 4..7. */
        for(int k=0;k<count;++k){int edges[2]={(k/4)*4+(k+1)%4,k+4};for(int e=0;e<(k<4&&count==8?2:1);++e){int q=edges[e];const cJSON* p=cJSON_GetArrayItem(points,k),*r=cJSON_GetArrayItem(points,q);double px=cJSON_GetArrayItem(p,a)->valuedouble,py=cJSON_GetArrayItem(p,b)->valuedouble,rx=cJSON_GetArrayItem(r,a)->valuedouble,ry=cJSON_GetArrayItem(r,b)->valuedouble;fprintf(f,"<line x1='%.3f' y1='%.3f' x2='%.3f' y2='%.3f'/>",x+(px-loA)*scale,y+(spanB-py+loB)*scale,x+(rx-loA)*scale,y+(spanB-ry+loB)*scale);}}
        fputs("</g>\n",f);
    }
    fprintf(f,"<g font-family='Helvetica,Arial,sans-serif' font-size='15' fill='#18283b' stroke='#18283b'><path d='M %.2f 615 v 10 h %.2f v -10' fill='none'/><path d='M 35 %.2f h -10 v %.2f h 10' fill='none'/></g><g font-family='Helvetica,Arial,sans-serif' fill='#18283b' font-size='14'><text x='400' y='650'>%c extent: %.1f mm</text><text x='50' y='680'>%c extent: %.1f mm | %c range %.1f to %.1f | %c range %.1f to %.1f mm</text></g></svg>\n",x,spanA*scale,y,spanB*scale,"UVN"[a],spanA*1000,"UVN"[b],spanB*1000,"UVN"[a],loA*1000,(loA+spanA)*1000,"UVN"[b],loB*1000,(loB+spanB)*1000);
    return !ferror(f);
}
static bool view(const cJSON* root,const char* dir,const char* leaf,int a,int b){FILE* f=output(dir,leaf);if(!f)return false;bool ok=svg(f,root,a,b,NULL);return fclose(f)==0&&ok;}
static void title(FILE* f,const cJSON* root,const char* subtitle,int page){fputs("<section><header><h1>",f);escaped(f,text(root,"name"));fputs("</h1><p>",f);escaped(f,subtitle);fprintf(f," | Sheet %d</p></header>",page);}
static void footer(FILE* f){fputs("<footer>MODEL REFERENCE - verify vehicle fit, stock, joinery and hardware before cutting. Dimensions in mm; diagrams fit to page, never measure paper.</footer></section>",f);}
bool Layout_ReferenceSheetsWrite(const cJSON* root,const char* dir) {
    if(!view(root,dir,"view_uv.svg",0,1)||!view(root,dir,"view_un.svg",0,2)||!view(root,dir,"view_vn.svg",1,2))return false;
    FILE* f=output(dir,"reference_sheets.html");if(!f)return false;
    fputs("<!doctype html><html><head><meta charset='utf-8'><title>Construction reference</title><style>@page{size:A3 landscape;margin:14mm}*{box-sizing:border-box}body{font:13px Arial,sans-serif;color:#18283b;background:#e8edf1;margin:0}section{background:white;margin:24px auto;padding:32px;max-width:1400px;page-break-after:always}h1{font-size:26px;margin:0 0 8px}header{border-bottom:3px solid #28435a;margin-bottom:20px}footer{border-top:1px solid #aaa;padding-top:12px;font-size:12px;margin-top:20px}table{border-collapse:collapse;width:100%;table-layout:fixed;font-size:12px}td,th{border-bottom:1px solid #ccd4da;padding:9px 6px;text-align:left;overflow-wrap:anywhere}th{background:#e8edf1}img{display:block;width:100%;height:490px;object-fit:contain}code{overflow-wrap:anywhere}li{margin:10px 0}@media print{body{background:white}section{margin:0;padding:0;max-width:none}section:last-child{page-break-after:auto}tr{break-inside:avoid}}</style></head><body>",f);
    int page=1;title(f,root,"Scope, frame and construction review",page++);
    fputs("<p><strong>Current-pose construction reference.</strong> Native solid/plane geometry and conservative STL envelopes are included. A visual wood finish does not specify plywood, species, grade or stock thickness. Each inventory row is one modeled instance, not a procurement quantity.</p><p>Collection: <code>",f);escaped(f,text(root,"collection"));fputs("</code> | Frame: <code>",f);escaped(f,text(root,"frameEntity"));fputs("</code></p><p>Input label: <code>",f);escaped(f,text(root,"sourceLabel"));fputs("</code></p><p>Exact exported model: <a href='source_snapshot.layout.json'>source snapshot</a> | <a href='specification.json'>machine-readable specification</a>. Snapshot captures current unsaved edits as well. Re-export after geometry or motion changes.</p><table><tr><th>Axis</th><th>Min mm</th><th>Max mm</th><th>Extent mm</th><th>World direction</th></tr>",f);
    const char* axes[]={"axisU","axisV","axisN"};for(int k=0;k<3;++k)fprintf(f,"<tr><td>%c</td><td>%.1f</td><td>%.1f</td><td>%.1f</td><td>%.4g / %.4g / %.4g</td></tr>","UVN"[k],coord(root,"frameMin_m",k)*1000,coord(root,"frameMax_m",k)*1000,(coord(root,"frameMax_m",k)-coord(root,"frameMin_m",k))*1000,coord(root,axes[k],0),coord(root,axes[k],1),coord(root,axes[k],2));
    fprintf(f,"</table><p>Frame world origin: %.1f / %.1f / %.1f mm. Excluded nonphysical/obsolete members: %.0f.</p><ul><li>Confirm as-built wall, floor and ceiling datums and cabinet fit.</li><li>Select stock material, thickness, grain, edge treatment and finishes.</li><li>Define joints, fasteners, mounting points and cut allowances.</li><li>Check hinge/slide specifications, travel, door swings and service access.</li><li>Record fit/cut tolerances and physically approve each part before fabrication.</li></ul>",coord(root,"frameOriginWorld_m",0)*1000,coord(root,"frameOriginWorld_m",1)*1000,coord(root,"frameOriginWorld_m",2)*1000,number(root,"excluded"));footer(f);
    const char* views[]={"view_uv.svg","view_un.svg","view_vn.svg"};const char* names[]={"U / V placement projection","U / N placement projection","V / N placement projection"};for(int k=0;k<3;++k){title(f,root,names[k],page++);fprintf(f,"<img src='%s' alt='%s'>",views[k],names[k]);footer(f);}
    title(f,root,"Saved measurement schedule",page++);fputs("<table><tr><th>Name</th><th>Value mm / status</th><th>Operation / frame</th><th>Datum A / B (feature)</th></tr>",f);
    const cJSON* row;cJSON_ArrayForEach(row,field(root,"measurements")){fputs("<tr><td>",f);escaped(f,text(row,"name"));fputs("</td><td>",f);if(cJSON_IsNumber(field(row,"value_m")))fprintf(f,"%.3f",number(row,"value_m")*1000);else escaped(f,text(row,"status"));fprintf(f,"</td><td>Kind %.0f / ",number(row,"kind"));escaped(f,text(row,"frameEntity"));fputs("</td><td>",f);escaped(f,text(row,"datumA"));fprintf(f," (%g) / ",number(row,"featureA"));escaped(f,text(row,"datumB"));fprintf(f," (%g)</td></tr>",number(row,"featureB"));}
    fputs("</table><p>Operations: 0/1/2 = area extent U/V/N; 3 = point distance; 4/5/6 = signed U/V/N projection. Datum features: 0 origin, 1 envelope min corner, 2 center, 3 max corner. Envelope datum distance is not minimum surface clearance.</p>",f);footer(f);
    const cJSON* candidate;cJSON_ArrayForEach(candidate,field(root,"parts")){
        const char* identity=text(candidate,"assembly");if(!identity[0])continue;bool seen=false;const cJSON* previous;cJSON_ArrayForEach(previous,field(root,"parts")){if(previous==candidate)break;if(!strcmp(text(previous,"assembly"),identity)){seen=true;break;}}if(seen)continue;
        title(f,root,"Assembly detail - directly parented members",page++);fputs("<p><strong>",f);escaped(f,text(candidate,"assemblyName"));fputs("</strong> | <code>",f);escaped(f,identity);fputs("</code> | Parts: ",f);
        const cJSON* member;cJSON_ArrayForEach(member,field(root,"parts"))if(!strcmp(text(member,"assembly"),identity))fprintf(f,"P%03.0f ",number(member,"partNumber"));fputs("</p><div style='height:490px'>",f);
        fputs("<style>svg{width:100%;height:100%}</style>",f);(void)svg(f,root,1,2,identity);fputs("</div><p>V/N detail, collection coordinates. Only direct members of this assembly are drawn; complete area projections include all physical descendants.</p>",f);footer(f);
    }
    int index=0;cJSON_ArrayForEach(row,field(root,"parts")){
        if(index%6==0){title(f,root,"Modeled part inventory and placement",page++);fputs("<table><tr><th style='width:5%'>No.</th><th style='width:25%'>Part / stable ID / assembly</th><th style='width:20%'>Local U/V/N mm / basis</th><th style='width:25%'>Frame minimum / maximum U/V/N mm</th><th style='width:25%'>Authored properties / review</th></tr>",f);}
        fprintf(f,"<tr><td>P%03.0f</td><td><strong>",number(row,"partNumber"));escaped(f,text(row,"name"));fputs("</strong><br><code>",f);escaped(f,text(row,"entity"));fputs("</code><br>",f);escaped(f,text(row,"assemblyName"));fputs("</td><td>",f);
        fprintf(f,"%.1f / %.1f / %.1f<br>",coord(row,"localSize_m",0)*1000,coord(row,"localSize_m",1)*1000,coord(row,"localSize_m",2)*1000);escaped(f,text(row,"geometryBasis"));fprintf(f,"</td><td>%.1f / %.1f / %.1f<br>%.1f / %.1f / %.1f</td><td>",coord(row,"frameMin_m",0)*1000,coord(row,"frameMin_m",1)*1000,coord(row,"frameMin_m",2)*1000,coord(row,"frameMax_m",0)*1000,coord(row,"frameMax_m",1)*1000,coord(row,"frameMax_m",2)*1000);
        const cJSON* prop;cJSON_ArrayForEach(prop,field(row,"authoredProperties")){escaped(f,text(prop,"key"));fputs(": ",f);if(field(prop,"text"))escaped(f,text(prop,"text"));else fprintf(f,"%.5g%s",number(prop,"value"),text(prop,"unit"));fputs("<br>",f);}fputs("Stock, joints, tolerances: review required</td></tr>",f);++index;
        if(index%6==0||index==cJSON_GetArraySize(field(root,"parts"))){fputs("</table>",f);footer(f);}
    }
    fputs("</body></html>\n",f);bool ok=!ferror(f);return fclose(f)==0&&ok;
}
