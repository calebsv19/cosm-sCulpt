#include "Layout/scene/layout_motion_interlock.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool travel(const LayoutConstraint* c) {
    return c->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL || c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL;
}
bool Layout_MotionInterlocksValidate(const Layout* layout, char* message, size_t size) {
    const LayoutObjectStore* store=&layout->objectStore;
    for(size_t i=0;i<store->constraintCount;++i) {
        const LayoutConstraint* a=&store->constraints[i];
        if(!a->closed_neighbor[0])continue;
        const LayoutConstraint* b=NULL;
        if(memchr(a->closed_neighbor,0,sizeof(a->closed_neighbor)))
            for(size_t j=0;j<store->constraintCount;++j)
                if(!strcmp(a->closed_neighbor,store->constraints[j].id))b=&store->constraints[j];
        if(!b || b==a || !travel(a) || !travel(b) || fabs(a->travel_home-a->travel_min)>1e-8 || fabs(b->travel_home-b->travel_min)>1e-8) {
            if(message&&size)snprintf(message,size,"Invalid closed-neighbour motion guard.");
            return false;
        }
        if(fabs(a->target-a->travel_home)>1e-8 && fabs(b->target-b->travel_home)>1e-8) {
            if(message&&size)snprintf(message,size,"Close %s before opening %s; these service openings share clearance.",b->id,a->id);
            return false;
        }
    }
    return true;
}

static const char* label(const LayoutObjectStore* store,const LayoutConstraint* c) {
    for(size_t i=0;i<store->assembly_count;++i)if(!strcmp(store->assemblies[i].id,c->motion_assembly))return store->assemblies[i].info.label;
    return c->id;
}
bool Layout_MotionInterlocksAllow(const Layout* layout,const char* id,double position,char* message,size_t size) {
    const LayoutObjectStore* store=&layout->objectStore;
    for(size_t i=0;i<store->constraintCount;++i){
        const LayoutConstraint* a=&store->constraints[i];if(!a->closed_neighbor[0])continue;
        for(size_t j=0;j<store->constraintCount;++j){
            const LayoutConstraint* b=&store->constraints[j];if(strcmp(a->closed_neighbor,b->id))continue;
            const LayoutConstraint* requested=!strcmp(id,a->id)?a:!strcmp(id,b->id)?b:NULL;
            if(!requested)continue;const LayoutConstraint* other=requested==a?b:a;
            if(fabs(position-requested->travel_home)>1e-8 && fabs(other->target-other->travel_home)>1e-8){
                if(message&&size)snprintf(message,size,"Close %s before opening %s; these service openings share clearance.",label(store,other),label(store,requested));return false;
            }
        }
    }
    return true;
}
