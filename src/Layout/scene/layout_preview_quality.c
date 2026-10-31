#include "Layout/scene/layout_preview_quality.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
LayoutPreviewQuality Layout_PreviewQuality(uint64_t now,uint64_t changed) {
    uint64_t elapsed=now>=changed?now-changed:0;
    return elapsed<450000000ull?LAYOUT_PREVIEW_MOVING:
        elapsed<1100000000ull?LAYOUT_PREVIEW_BALANCED:LAYOUT_PREVIEW_DETAIL;
}
size_t Layout_PreviewTriangleBudget(LayoutPreviewQuality quality) {
    return quality==LAYOUT_PREVIEW_MOVING?32000:quality==LAYOUT_PREVIEW_BALANCED?120000:500000;
}
int Layout_PreviewRefinementDelayMs(uint64_t now,uint64_t changed) {
    uint64_t elapsed=now>=changed?now-changed:0;
    uint64_t next=elapsed<450000000ull?450000000ull:1100000000ull;
    return elapsed>=next?-1:(int)((next-elapsed+999999ull)/1000000ull);
}
float Layout_PreviewRasterScale(int w,int h,float dpi,bool moving) {
    if(moving)return w>0 && h>0?fminf(.30f,sqrtf(180000.0f/((float)w*h))):.30f;
    if(w<=0||h<=0)return .75f;
    float scale=fminf(1.25f,fmaxf(1,dpi));
    float cap=sqrtf(890000.0f/((float)w*(float)h));return fminf(scale,cap);
}
size_t Layout_PreviewEdges(uint8_t* rgba,const float* depth,const int32_t* owner,int w,int h,bool wire,bool edges,int selected,int hovered) {
    if(!wire && !edges && selected<0 && hovered<0)return 0;
    size_t count=0;
    for(int y=1;y<h-1;++y)for(int x=1;x<w-1;++x){size_t p=(size_t)y*w+x;int id=owner[p];if(id<0 || (!wire&&!edges&&id!=selected&&id!=hovered))continue;
        bool boundary=false;size_t neighbors[]={p-1,p+1,p-w,p+w};
        for(int i=0;i<4;++i){size_t q=neighbors[i];if(owner[q]!=id || fabsf(depth[q]-depth[p])>.18f*(1+fabsf(depth[p]))){boundary=true;break;}}
        uint8_t* pixel=rgba+p*4;
        if(boundary && (wire||edges||id==selected||id==hovered)) {
            const uint8_t color[3]={id==selected?255:id==hovered?104:70,id==selected?168:id==hovered?232:78,id==selected?76:id==hovered?255:90};
            for(int c=0;c<3;++c)pixel[c]=(wire||id==selected||id==hovered)?color[c]:(uint8_t)((pixel[c]*2+color[c])/3);
            pixel[3]=255;++count;
        }else if(wire)memset(pixel,0,4);
    }
    return count;
}
bool Layout_PreviewAntialias(uint8_t* rgba,const int32_t* owner,int w,int h) {
    size_t pixels=(size_t)w*h;if(pixels>900000)return false;
    uint8_t* source=malloc(pixels*4);if(!source)return false;memcpy(source,rgba,pixels*4);
    /* Local coverage/color filter changes display pixels only; depth and pick owner remain exact. */
    for(int y=1;y<h-1;++y)for(int x=1;x<w-1;++x){size_t p=(size_t)y*w+x;if(owner[p]<0)continue;
        size_t q[]={p-1,p+1,p-w,p+w};bool boundary=false;for(int i=0;i<4;++i)if(owner[q[i]]!=owner[p])boundary=true;if(!boundary)continue;
        unsigned alpha=source[p*4+3]*4, channels[3]={0};for(int c=0;c<3;++c)channels[c]=source[p*4+c]*source[p*4+3]*4;
        for(int i=0;i<4;++i){unsigned a=source[q[i]*4+3];alpha+=a;for(int c=0;c<3;++c)channels[c]+=source[q[i]*4+c]*a;}
        for(int c=0;c<3;++c)rgba[p*4+c]=alpha?(uint8_t)(channels[c]/alpha):0;rgba[p*4+3]=(uint8_t)(alpha/8);
    }
    free(source);return true;
}
