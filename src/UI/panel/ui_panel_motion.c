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
    double t=(double)ui->spatial.preview_index/(e->samples-1),position=c->travel_min*(1-t)+c->travel_max*t;
    return Layout_SampleMotionSet(&Global_Get()->layout,c->id,position,preview_poses,LAYOUT_MAX_MOTION_MEMBERS,&preview_count);
}
void UIPanel_MotionInspectionBuild(PartsPane* p, const LayoutSpatialResult* r) {
    UIPanelState* ui=UIPanel_Get();const char* target=NULL;const LayoutMotionEnvelope* e=result_envelope(r,&target);
    if (!e || !r->measurable || !Layout_MotionEnvelopeCurrent(&Global_Get()->layout,e)) return;
    row(p,PARTS_MOTION_INSPECT,"Inspect motion",true);
    if (!ui->spatial.preview_active || strcmp(ui->spatial.preview_target,target)) return;
    const Object3D* o=Layout_ObjectStore_FindConst(&Global_Get()->layout.objectStore,e->object_id);
    if (!o || strcmp(ui->spatial.preview_envelope,o->coreMeta.object_id)) return;
    const LayoutConstraint* c=rule(e);if (!c) return;char text[160];
    const LayoutEntityInfo* obstacle=Layout_EntityInfo(&Global_Get()->layout.objectStore,target);
    snprintf(text,sizeof(text),"Obstacle: %s",obstacle && obstacle->label[0]?obstacle->label:target);note(p,text);
    if (ui->spatial.inspection.hit) {
        snprintf(text,sizeof(text),"First sampled failure: %.4g %s",ui->spatial.inspection.position,c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL?"deg":"m");note(p,text);
        const LayoutEntityInfo* member=Layout_EntityInfo(&Global_Get()->layout.objectStore,ui->spatial.inspection.member_id);
        snprintf(text,sizeof(text),"Part: %s",member && member->label[0]?member->label:ui->spatial.inspection.member_id);note(p,text);
    } else note(p,"No sampled pose fails. Bounds may overestimate; gaps between samples remain untested.");
    double t=(double)ui->spatial.preview_index/(e->samples-1);
    snprintf(text,sizeof(text),"Preview: %.4g %s (%u/%u)",c->travel_min*(1-t)+c->travel_max*t,c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL?"deg":"m",ui->spatial.preview_index+1,e->samples);note(p,text);
    cell(p,PARTS_MOTION_PREVIOUS,"Previous",NULL,0,2,ui->spatial.preview_index>0,false);
    cell(p,PARTS_MOTION_NEXT,"Next",NULL,1,2,ui->spatial.preview_index+1<e->samples,false);p->y+=p->h;
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
        const Object3D* o=Layout_ObjectStore_FindConst(&l->objectStore,e->object_id);
        snprintf(ui->spatial.preview_envelope,64,"%s",o->coreMeta.object_id);snprintf(ui->spatial.preview_target,64,"%s",target);
    } else if (action==PARTS_MOTION_PREVIOUS && ui->spatial.preview_index>0) --ui->spatial.preview_index;
    else if (action==PARTS_MOTION_NEXT && ui->spatial.preview_index+1<e->samples) ++ui->spatial.preview_index;
    else return false;
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
