#include "Layout/scene/layout_folding_brace.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
bool Layout_FoldingLockValid(const Layout* l,const LayoutConstraint* c) {
    const LayoutFoldingBrace* f=&c->folding_brace;
    if(!f->manual_lock)return !f->lock_engaged;
    if(!f->compact_mount||!isfinite(f->lock_position_deg)||f->lock_position_deg<c->travel_min||f->lock_position_deg>c->travel_max)return false;
    if(!f->lock_engaged)return true;
    LayoutMeasurementResult m=Layout_Measure(l,&c->a,&c->b,LAYOUT_MEASURE_PLANAR_ANGLE,c->axis);
    return fabs(c->target-f->lock_position_deg)<=1e-5&&m.status==LAYOUT_MEASUREMENT_OK&&fabs(m.value-f->lock_position_deg)<=1e-4;
}
bool Layout_FoldingBraceSetLock(Layout* l,const char* id,bool engaged,LayoutGeometryBeforePublish hook,void* context) {
    if(!l||!id)return false;
    for(size_t i=0;i<l->objectStore.constraintCount;++i){LayoutConstraint c=l->objectStore.constraints[i];
        if(strcmp(c.id,id))continue;
        if(!c.folding_brace.enabled||!c.folding_brace.manual_lock)return false;
        c.folding_brace.lock_engaged=engaged;
        if(!Layout_FoldingLockValid(l,&c)){snprintf(l->geometryMessage,sizeof(l->geometryMessage),"Deploy to %.6g deg before engaging the manual support lock.",c.folding_brace.lock_position_deg);return false;}
        return Layout_ConstraintEdit(l,&c,NULL,hook,context);
    }return false;
}
