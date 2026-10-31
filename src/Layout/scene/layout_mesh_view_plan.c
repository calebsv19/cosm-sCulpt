#include "Layout/scene/layout_mesh_view_plan.h"
#include "Layout/scene/layout_mesh_preview_cache.h"
#include "Layout/scene/layout_mesh_prepared_store.h"
#include "Layout/layout_engineering.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>
typedef struct {uint32_t id;unsigned tier;bool initialized;} History;
typedef struct {size_t index;float score;} Priority;
static LayoutMeshViewPlan plan;
static History* history;
static Priority* order;
static size_t capacity;
static uint64_t generation;
static Vec3 corner(Vec3 lo,Vec3 hi,int i) {return (Vec3){i&1?hi.x:lo.x,i&2?hi.y:lo.y,i&4?hi.z:lo.z};}
bool Layout_MeshBoundsVisible(Vec3 lo,Vec3 hi,const SpaceViewContext* v,const Grid* g,SDL_Rect clip) {
    if(!isfinite(lo.x+lo.y+lo.z+hi.x+hi.y+hi.z) || lo.x>hi.x || lo.y>hi.y || lo.z>hi.z)return true;
    if(v->perspective.enabled) {
        const PerspectiveView* p=&v->perspective;float tx=p->tan_half_fov*p->aspect,ty=p->tan_half_fov;
        Vec3 normals[6]={p->forward,Vec3_Scale(p->forward,-1),Vec3_Add(Vec3_Scale(p->forward,tx),p->right),
            Vec3_Sub(Vec3_Scale(p->forward,tx),p->right),Vec3_Add(Vec3_Scale(p->forward,ty),p->up),Vec3_Sub(Vec3_Scale(p->forward,ty),p->up)};
        float offsets[6]={-p->near_clip,p->far_clip,0,0,0,0};
        for(int plane=0;plane<6;++plane) {
            bool outside=true;
            for(int i=0;i<8;++i) {float d=Vec3_Dot(Vec3_Sub(corner(lo,hi,i),p->eye),normals[plane])+offsets[plane];if(!isfinite(d) || d>=-1e-4f) {outside=false;break;}}
            if(outside)return false;
        }
        return true;
    }
    float minx=FLT_MAX,miny=FLT_MAX,maxx=-FLT_MAX,maxy=-FLT_MAX;
    for(int i=0;i<8;++i) {
        Vec2 p=WorldToScreen(SpaceAdapter_ProjectToView(corner(lo,hi,i),v),g);
        if(!isfinite(p.x) || !isfinite(p.y))return true;
        minx=fminf(minx,p.x);miny=fminf(miny,p.y);maxx=fmaxf(maxx,p.x);maxy=fmaxf(maxy,p.y);
    }
    return maxx>=clip.x-1 && maxy>=clip.y-1 && minx<=clip.x+clip.w+1 && miny<=clip.y+clip.h+1;
}
unsigned Layout_MeshScreenTier(float d,unsigned previous,bool initialized) {
    const float boundaries[3]={32,128,512};unsigned tier=0;
    if(!isfinite(d))return 3;
    if(!initialized) {while(tier<3 && d>boundaries[tier])++tier;return tier;}
    tier=previous<4?previous:3;
    while(tier<3 && d>boundaries[tier]*1.2f)++tier;
    while(tier>0 && d<boundaries[tier-1]*.8f)--tier;
    return tier;
}
static bool bounds(const Object3D* o,Vec3* lo,Vec3* hi) {
    Vec3 p[8];size_t n=8;
    bool ok=o->kind==OBJECT3D_KIND_PLANE ? (n=4,Layout_Object3D_ComputePlaneCorners(o,p)) :
        o->kind==OBJECT3D_KIND_RECT_PRISM ? Layout_Object3D_ComputeRectPrismCorners(o,p) :
        o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE && Layout_Object3D_ComputeMeshInstanceCorners(o,p);
    if(!ok)return false;*lo=*hi=p[0];
    for(size_t i=0;i<n;++i) {
        if(!isfinite(p[i].x+p[i].y+p[i].z))return false;
        *lo=(Vec3){fminf(lo->x,p[i].x),fminf(lo->y,p[i].y),fminf(lo->z,p[i].z)};
        *hi=(Vec3){fmaxf(hi->x,p[i].x),fmaxf(hi->y,p[i].y),fmaxf(hi->z,p[i].z)};
    }
    return true;
}
static float diameter(Vec3 lo,Vec3 hi,const SpaceViewContext* v,const Grid* g) {
    float minx=FLT_MAX,miny=FLT_MAX,maxx=-FLT_MAX,maxy=-FLT_MAX;
    for(int i=0;i<8;++i) {
        Vec3 p=corner(lo,hi,i);
        if(v->perspective.enabled && PerspectiveView_Depth(&v->perspective,p)<=v->perspective.near_clip)return INFINITY;
        Vec2 s=WorldToScreen(SpaceAdapter_ProjectToView(p,v),g);
        if(!isfinite(s.x+s.y))return INFINITY;
        minx=fminf(minx,s.x);miny=fminf(miny,s.y);maxx=fmaxf(maxx,s.x);maxy=fmaxf(maxy,s.y);
    }
    return fmaxf(maxx-minx,maxy-miny);
}
static int priority_compare(const void* a,const void* b) {
    const Priority *x=a,*y=b;return x->score<y->score?-1:x->score>y->score?1:x->index<y->index?-1:x->index>y->index;
}
static size_t cost(const Object3D* o,unsigned tier) {
    size_t limit=tier<4?Layout_MeshPreviewTriangleLimits[tier]:12;
    return o->meshInstance.triangleCount && o->meshInstance.triangleCount<limit?o->meshInstance.triangleCount:limit;
}
const LayoutMeshViewPlan* Layout_MeshViewPlanBuild(const Layout* l,const SpaceViewContext* v,const Grid* g,
    SDL_Rect clip,bool moving,bool inspecting,uint32_t selected,size_t budget) {
    if(moving && budget>32000)budget=32000;
    if(l->objectStore.count>16384 || l->objectStore.assembly_count>LAYOUT_MAX_ASSEMBLIES)return NULL;
    if(l->objectStore.count>capacity) {
        size_t n=l->objectStore.count;LayoutMeshViewEntry* e=calloc(n,sizeof(*e));History* h=calloc(n,sizeof(*h));Priority* p=calloc(n,sizeof(*p));
        if(!e || !h || !p) {free(e);free(h);free(p);return NULL;}
        if(history)memcpy(h,history,capacity*sizeof(*h));free(plan.entries);free(history);free(order);
        plan.entries=e;history=h;order=p;capacity=n;
    }
    uint64_t current=Layout_MeshPreviewCacheRead().generation;
    if(current!=generation) {if(history)memset(history,0,capacity*sizeof(*history));generation=current;}
    plan.count=l->objectStore.count;plan.culled_objects=plan.culled_assemblies=plan.planned_triangles=0;plan.budget=budget;plan.budget_overflow=false;
    plan.scratch_bytes=capacity*(sizeof(*plan.entries)+sizeof(*history)+sizeof(*order));
    Vec3 group_lo[LAYOUT_MAX_ASSEMBLIES],group_hi[LAYOUT_MAX_ASSEMBLIES];bool group_found[LAYOUT_MAX_ASSEMBLIES]={0},group_safe[LAYOUT_MAX_ASSEMBLIES],group_visible[LAYOUT_MAX_ASSEMBLIES];
    for(size_t j=0;j<l->objectStore.assembly_count;++j) {group_safe[j]=true;group_visible[j]=true;}
    bool disable=getenv("LINE_DRAWING_PREVIEW_CULLING_OFF")!=NULL;bool oracle=getenv("LINE_DRAWING_PREVIEW_DETAIL_ORACLE")!=NULL;
    size_t priorities=0;
    for(size_t i=0;i<plan.count;++i) {
        const Object3D* o=&l->objectStore.items[i];LayoutMeshViewEntry* e=&plan.entries[i];*e=(LayoutMeshViewEntry){0};
        if(!Layout_ObjectShown(&l->objectStore,o) || Layout_EntityIsSpatialGuide(&o->info))continue;
        bool known=bounds(o,&e->lo,&e->hi);e->visible=true;
        for(size_t j=0;j<l->objectStore.assembly_count;++j)if(Layout_IsDescendant(&l->objectStore,o->info.parent_id,l->objectStore.assemblies[j].id)) {
            if(!known || o->objectId==selected) {group_safe[j]=false;continue;}
            if(!group_found[j]) {group_lo[j]=e->lo;group_hi[j]=e->hi;group_found[j]=true;}
            else {group_lo[j]=(Vec3){fminf(group_lo[j].x,e->lo.x),fminf(group_lo[j].y,e->lo.y),fminf(group_lo[j].z,e->lo.z)};
                group_hi[j]=(Vec3){fmaxf(group_hi[j].x,e->hi.x),fmaxf(group_hi[j].y,e->hi.y),fmaxf(group_hi[j].z,e->hi.z)};}
        }
        if(!disable && known && o->objectId!=selected)e->visible=Layout_MeshBoundsVisible(e->lo,e->hi,v,g,clip);
        if(!e->visible) {++plan.culled_objects;continue;}
        if(o->kind!=OBJECT3D_KIND_MESH_ASSET_INSTANCE) {e->triangles=o->kind==OBJECT3D_KIND_PLANE?2:24;plan.planned_triangles+=e->triangles;continue;}
        e->diameter=known?diameter(e->lo,e->hi,v,g):INFINITY;
        History* h=&history[i];if(h->id!=o->objectId)h->initialized=false;
        e->tier=Layout_MeshScreenTier(e->diameter,h->tier,h->initialized);h->id=o->objectId;h->tier=e->tier;h->initialized=true;
        if(o->objectId==selected || inspecting)e->tier=3;
        if(oracle)e->tier=3;
        /* Motion takes precedence over Inspection, selection, and diagnostic detail. */
        if(moving && e->tier>1)e->tier=1;
        e->triangles=cost(o,e->tier);plan.planned_triangles+=e->triangles;
        order[priorities++]=(Priority){i,o->objectId==selected?FLT_MAX:e->diameter};
    }
    for(size_t j=0;j<l->objectStore.assembly_count;++j)if(!disable && group_found[j] && group_safe[j] &&
        !Layout_MeshBoundsVisible(group_lo[j],group_hi[j],v,g,clip)) {group_visible[j]=false;++plan.culled_assemblies;}
    for(size_t i=0;i<plan.count;++i)if(plan.entries[i].visible)for(size_t j=0;j<l->objectStore.assembly_count;++j)
        if(!group_visible[j] && Layout_IsDescendant(&l->objectStore,l->objectStore.items[i].info.parent_id,l->objectStore.assemblies[j].id)) {
            plan.entries[i].visible=false;plan.planned_triangles-=plan.entries[i].triangles;++plan.culled_objects;break;
        }
    if((!oracle || moving) && plan.planned_triangles>budget) {
        qsort(order,priorities,sizeof(*order),priority_compare);
        for(size_t j=0;j<priorities && plan.planned_triangles>budget;++j) {
            LayoutMeshViewEntry* e=&plan.entries[order[j].index];if(!e->visible || (!moving && l->objectStore.items[order[j].index].objectId==selected))continue;
            const Object3D* o=&l->objectStore.items[order[j].index];
            while(e->tier>0 && plan.planned_triangles>budget) {plan.planned_triangles-=e->triangles;e->triangles=cost(o,--e->tier);plan.planned_triangles+=e->triangles;}
        }
        /* Keep coherent coarse surfaces everywhere before resorting to bounds proxies. */
        for(size_t j=0;j<priorities && plan.planned_triangles>budget;++j) {
            LayoutMeshViewEntry* e=&plan.entries[order[j].index];if(!e->visible || (!moving && l->objectStore.items[order[j].index].objectId==selected))continue;
            if(plan.planned_triangles>budget && e->triangles>12) {plan.planned_triangles-=e->triangles;e->tier=4;e->triangles=12;plan.planned_triangles+=12;}
        }
    }
    plan.budget_overflow=plan.planned_triangles>budget;return &plan;
}
void Layout_MeshViewPlanShutdown(void) {free(plan.entries);free(history);free(order);plan=(LayoutMeshViewPlan){0};history=NULL;order=NULL;capacity=0;generation=0;}
