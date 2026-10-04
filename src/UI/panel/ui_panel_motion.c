#include "UI/ui_panel_motion.h"
#include "UI/ui_panel_parts_surface.h"
#include "UI/ui_panel_shell.h"
#include "Core/space_mode_adapter.h"
#include "Core/global_state.h"
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <string.h>

static Object3D preview_poses[LAYOUT_MAX_MOTION_MEMBERS];
static size_t preview_count;
static const LayoutMotionEnvelope* result_envelope(const LayoutSpatialResult* r, const char** target) {
    Layout* l=&Global_Get()->layout;
    for (size_t i=0;i<l->objectStore.count;++i) {
        const Object3D* o=&l->objectStore.items[i];const LayoutMotionEnvelope* e=Layout_FindMotionEnvelope(&l->objectStore,o->objectId);
        if (o->isDeleted || !e) continue;
        if (!strcmp(o->coreMeta.object_id,r->source) || !strcmp(o->coreMeta.object_id,r->target)) {
            *target=!strcmp(o->coreMeta.object_id,r->source)?r->target:r->source;return e;
        }
    }
    return NULL;
}
static const LayoutConstraint* rule(const LayoutMotionEnvelope* e) {
    const LayoutObjectStore* s=&Global_Get()->layout.objectStore;
    for (size_t i=0;i<s->constraintCount;++i) if (!strcmp(s->constraints[i].id,e->rule_id)) return &s->constraints[i];
    return NULL;
}
static bool preview(const LayoutMotionEnvelope* e) {
    UIPanelState* ui=UIPanel_Get();const LayoutConstraint* c=rule(e);if (!c || ui->spatial.preview_index>=e->samples) return false;
    double position=ui->spatial.preview_position;
    return Layout_SampleMotionSet(&Global_Get()->layout,c->id,position,preview_poses,LAYOUT_MAX_MOTION_MEMBERS,&preview_count);
}
void UIPanel_MotionInspectionBuild(PartsPane* p, const LayoutSpatialResult* r) {
    UIPanelState* ui=UIPanel_Get();const char* target=NULL;const LayoutMotionEnvelope* e=result_envelope(r,&target);
    if (!e || !r->measurable || !Layout_MotionEnvelopeCurrent(&Global_Get()->layout,e)) return;
    if (!ui->spatial.preview_active || strcmp(ui->spatial.preview_target,target)) {row(p,PARTS_MOTION_INSPECT,"Inspect motion",true);return;}
    const Object3D* o=Layout_ObjectStore_FindConst(&Global_Get()->layout.objectStore,e->object_id);
    if (!o || strcmp(ui->spatial.preview_envelope,o->coreMeta.object_id)) return;
    const LayoutConstraint* c=rule(e);if (!c) return;char text[160];
    const LayoutEntityInfo* obstacle=Layout_EntityInfo(&Global_Get()->layout.objectStore,target);
    snprintf(text,sizeof(text),"Obstacle: %s",obstacle && obstacle->label[0]?obstacle->label:target);note(p,text);
    const char* member_id=ui->spatial.inspection.member_id;
    const char* unit=c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL?"deg":"m";
    if (ui->spatial.range_checked) {
        const LayoutMotionRangeResult* result=&ui->spatial.range_result;
        if (result->status==LAYOUT_MOTION_RANGE_CLEAR) {
            note(p,"Range separated by conservative intervals.");
            if (r->severity==LAYOUT_SPATIAL_WARNING) note(p,"Interval warning retained.");
        } else if (result->status==LAYOUT_MOTION_RANGE_FAILURE) {
            snprintf(text,sizeof(text),"%s: %.4g %s",r->required_meters>0?"Clearance fails at":"Contact at tested pose",result->position,unit);note(p,text);
            member_id=result->member_id;
        } else {
            note(p,"Range unresolved; refinement limit reached.");
            snprintf(text,sizeof(text),"Interval: %.4g to %.4g %s",result->interval_min,result->interval_max,unit);note(p,text);
        }
        snprintf(text,sizeof(text),"%u poses tested",result->tested_poses);note(p,text);
    } else if (ui->spatial.inspection.hit) {
        snprintf(text,sizeof(text),"First sampled failure: %.4g %s",ui->spatial.inspection.position,unit);note(p,text);
    } else {
        note(p,"No sampled pose fails.");note(p,"Full range has not been checked.");
    }
    if ((!ui->spatial.range_checked && ui->spatial.inspection.hit) ||
        (ui->spatial.range_checked && ui->spatial.range_result.status==LAYOUT_MOTION_RANGE_FAILURE)) {
        const LayoutEntityInfo* member=Layout_EntityInfo(&Global_Get()->layout.objectStore,member_id);
        snprintf(text,sizeof(text),"Part: %s",member && member->label[0]?member->label:member_id);note(p,text);
    }
    snprintf(text,sizeof(text),"Preview: %.4g %s",ui->spatial.preview_position,unit);note(p,text);
    row(p,PARTS_MOTION_RANGE,"Check full range",true);
    cell(p,PARTS_MOTION_PREVIOUS,"Previous",NULL,0,2,ui->spatial.preview_position>c->travel_min,false);
    cell(p,PARTS_MOTION_NEXT,"Next",NULL,1,2,ui->spatial.preview_position<c->travel_max,false);p->y+=p->h;
    row(p,PARTS_MOTION_CLOSE,"Close preview",true);note(p,"Orange wire: preview only; saved pose unchanged.");
}
bool UIPanel_MotionInspectionClick(int action, const LayoutSpatialResult* r) {
    UIPanelState* ui=UIPanel_Get();Layout* l=&Global_Get()->layout;
    if (action==PARTS_MOTION_CLOSE) {ui->spatial.preview_active=false;return true;}
    const char* target=NULL;const LayoutMotionEnvelope* e=r?result_envelope(r,&target):NULL;
    if (!e || !Layout_MotionEnvelopeCurrent(l,e)) return false;
    if (action==PARTS_MOTION_INSPECT) {
        LayoutMotionInspection result;
        if (!Layout_InspectMotionObstruction(l,e,target,r->required_meters,r->required_meters>0,&result)) {
            ui->spatial.preview_active=false;snprintf(ui->parts.message,160,"Motion inspection unavailable: check scope, locks, bounds or target geometry.");return false;
        }
        ui->spatial.inspection=result;ui->spatial.preview_index=result.sample_index;
        ui->spatial.preview_position=result.position;ui->spatial.range_checked=false;
        const Object3D* o=Layout_ObjectStore_FindConst(&l->objectStore,e->object_id);
        snprintf(ui->spatial.preview_envelope,64,"%s",o->coreMeta.object_id);snprintf(ui->spatial.preview_target,64,"%s",target);
    } else if (action==PARTS_MOTION_RANGE && ui->spatial.preview_active) {
        LayoutMotionRangeResult result;
        if (!Layout_CheckMotionRange(l,e,target,r->required_meters,r->required_meters>0,257,&result)) {
            ui->spatial.range_checked=false;snprintf(ui->parts.message,160,"Full range unavailable: check static target, locks and bounds.");return false;
        }
        ui->spatial.range_checked=true;ui->spatial.range_result=result;
        if (result.status!=LAYOUT_MOTION_RANGE_CLEAR) ui->spatial.preview_position=result.position;
    } else if ((action==PARTS_MOTION_PREVIOUS || action==PARTS_MOTION_NEXT) && ui->spatial.preview_active) {
        const LayoutConstraint* c=rule(e);if (!c || c->travel_min==c->travel_max) return false;
        double t=(ui->spatial.preview_position-c->travel_min)/(c->travel_max-c->travel_min)*(e->samples-1);
        double index=action==PARTS_MOTION_PREVIOUS?ceil(t-1e-9)-1:floor(t+1e-9)+1;
        ui->spatial.preview_index=(uint32_t)fmax(0,fmin(e->samples-1,index));
        t=(double)ui->spatial.preview_index/(e->samples-1);ui->spatial.preview_position=c->travel_min*(1-t)+c->travel_max*t;
    } else return false;
    ui->spatial.preview_active=preview(e);return ui->spatial.preview_active;
}
/* Cached isolated poses are display-only. Hide preview as soon as checked inputs
 * drift, a different result/tab is chosen, or its envelope becomes stale. */
void UIPanel_RenderMotionPreview(SDL_Renderer* renderer) {
    UIPanelState* ui=UIPanel_Get();GlobalState* state=Global_Get();CorePaneRect viewport;
    if (!renderer || !ui->spatial.preview_active || ui->activeRightTab!=UI_PANEL_RIGHT_TAB_PARTS || ui->parts.mode!=5 ||
        !LineDrawingPaneHost_GetViewportRect(&state->paneHost,&viewport)) return;
    SDL_Rect old;bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&old);
    SDL_Rect clip={(int)viewport.x,(int)viewport.y,(int)viewport.width,(int)viewport.height};SDL_RenderSetClipRect(renderer,&clip);
    SDL_SetRenderDrawColor(renderer,255,180,70,255);SpaceViewContext view=SpaceAdapter_BuildViewContext(state);
    for (size_t i=0;i<preview_count;++i) {
        const Object3D* o=&preview_poses[i];if (!Layout_ObjectShown(&state->layout.objectStore,o)) continue;
        Vec3 corners[8];bool panel=o->kind==OBJECT3D_KIND_PLANE;
        if (!(panel?Layout_Object3D_ComputePlaneCorners(o,corners):Layout_Object3D_ComputeRectPrismCorners(o,corners))) continue;
        const int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        for (int j=0;j<(panel?4:12);++j) {
            Vec2 a=WorldToScreen(SpaceAdapter_ProjectToView(corners[edges[j][0]],&view),&state->grid);
            Vec2 b=WorldToScreen(SpaceAdapter_ProjectToView(corners[edges[j][1]],&view),&state->grid);
            if (isfinite(a.x) && isfinite(a.y) && isfinite(b.x) && isfinite(b.y) &&
                fabs(a.x)<INT_MAX && fabs(a.y)<INT_MAX && fabs(b.x)<INT_MAX && fabs(b.y)<INT_MAX)
                SDL_RenderDrawLine(renderer,(int)a.x,(int)a.y,(int)b.x,(int)b.y);
        }
    }
    TTF_Font* font=FontManager_Get(FONT_DEFAULT);
    if (font) UIPanelSummary_DrawTextClipped(renderer,font,"Motion preview (read-only)",clip.x+12,clip.y+12,clip.w-24,40,(SDL_Color){255,180,70,255});
    SDL_RenderSetClipRect(renderer,clipped?&old:NULL);
}
