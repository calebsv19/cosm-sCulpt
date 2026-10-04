#include "Layout/layout_engineering.h"
#include "UI/ui_panel_measurement.h"
#include "Core/global_state.h"
#include "Layout/layout_constraints.h"
#include "Core/space_mode_adapter.h"
#include "Layout/scene/layout_object_faces.h"
#include "UI/font_manager.h"
#include "UI/ui_panel_summary_surface.h"
#include "core_screen_pick.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct MeasurementMarker {
    EditorGeometricReference reference;
    CoreScreenPickCandidate candidate;
} MeasurementMarker;

static bool viewport(CorePaneRect* rect) {
    return LineDrawingPaneHost_GetViewportRect(&Global_Get()->paneHost, rect) && rect->width > 0 && rect->height > 0;
}

static bool marker_visible(CorePaneRect r, double x, double y) {
    return x >= r.x && x < r.x+r.width && y >= r.y && y < r.y+r.height;
}

static bool inside(CorePaneRect r, double x, double y) {
    return x >= r.x && y >= r.y && x < r.x+r.width && y < r.y+r.height;
}

static bool project(const EditorGeometricReference* ref, Vec2* pixel, double* depth) {
    GlobalState* s = Global_Get();
    EditorResolvedReference resolved;
    if (Editor_ResolveReference(&s->layout, ref, &resolved) != EDITOR_MEASUREMENT_OK) return false;
    double scale = Layout_WorldScale(&s->layout);
    Vec3 point = {(float)(resolved.point_meters[0]/scale), (float)(resolved.point_meters[1]/scale),
                  (float)(resolved.point_meters[2]/scale)};
    SpaceViewContext view = SpaceAdapter_BuildViewContext(s);
    *pixel = WorldToScreen(SpaceAdapter_ProjectToView(point, &view), &s->grid);
    if (depth) {
        if (SpaceAdapter_IsFreeViewEnabled(&view)) *depth = Vec3_Dot(Vec3_Sub(point, view.camera.target), FreeView_Forward(&view.camera));
        else *depth = view.plane.axis == VIEW_PLANE_YZ ? point.x : view.plane.axis == VIEW_PLANE_XZ ? point.y : point.z;
    }
    return isfinite(pixel->x) && isfinite(pixel->y);
}

/* Only the active named feature is offered on each eligible object. Markers are
 * explicit X-ray datums, not an occlusion or nearest-surface query. The active
 * local offset is applied in each candidate object's own U/V/N frame. */
static MeasurementMarker* markers(size_t* count) {
    *count = 0;
    CorePaneRect rect;
    const LayoutObjectStore* store = &Global_Get()->layout.objectStore;
    if (!viewport(&rect) || store->count == 0) return NULL;
    MeasurementMarker* list = calloc(store->count, sizeof(*list));
    if (!list) return NULL;
    const UIPanelState* ui = UIPanel_Get();
    for (size_t i = 0; i < store->count; ++i) {
        const Object3D* o = &store->items[i];
        if (!Layout_ObjectShown(store,o) || !o->coreMeta.flags.selectable) continue;
        MeasurementMarker m = {.reference = ui->measurement.refs[ui->measurement.slot]};
        snprintf(m.reference.entity_id, sizeof(m.reference.entity_id), "%s", o->coreMeta.object_id);
        Vec2 pixel;
        double depth = 0;
        if (!project(&m.reference, &pixel, &depth) || !marker_visible(rect, pixel.x, pixel.y)) continue;
        m.candidate = (CoreScreenPickCandidate){.stable_key=o->objectId, .payload=(int64_t)*count,
            .screen_x=pixel.x, .screen_y=pixel.y, .view_depth=depth};
        list[(*count)++] = m;
    }
    return list;
}

bool UIPanel_MeasurementPickAt(int x, int y) {
    UIPanelState* ui = UIPanel_Get();
    CorePaneRect rect;
    if (!ui->measurement.active || !ui->measurement.picking || !viewport(&rect) || !marker_visible(rect,x,y)) return false;
    size_t count = 0;
    MeasurementMarker* list = markers(&count);
    CoreScreenPickCandidate* candidates = count ? calloc(count, sizeof(*candidates)) : NULL;
    CoreScreenPickIndex index = {0};
    CoreScreenPickResult picked = {0};
    CoreScreenPickConfig config = core_screen_pick_config_default();
    config.capture_radius_px = 12;
    bool ok = false;
    if (candidates) {
        for (size_t i=0; i<count; ++i) candidates[i] = list[i].candidate;
        if (core_screen_pick_index_init(&index,config).code == CORE_OK &&
            core_screen_pick_index_rebuild(&index,candidates,count,0).code == CORE_OK &&
            core_screen_pick_query_nearest(&index,x,y,&picked).code == CORE_OK && picked.found &&
            picked.payload >= 0 && (size_t)picked.payload < count) {
            ui->measurement.refs[ui->measurement.slot] = list[picked.payload].reference;
            ui->measurement.constraint_index = -1;
            ui->measurement.observed_rule_valid = false;
            ui->measurement.use_rule_axis = false;
            snprintf(ui->measurement.pick_message,sizeof(ui->measurement.pick_message),"%c: %s",
                'A'+ui->measurement.slot, list[picked.payload].reference.entity_id);
            ok = true;
        }
    }
    if (!ok) snprintf(ui->measurement.pick_message,sizeof(ui->measurement.pick_message),"No matching marker within 12 pixels; reference unchanged");
    core_screen_pick_index_destroy(&index);
    free(candidates); free(list);
    return ok;
}

static void pivot_marker(SDL_Renderer* renderer, CorePaneRect rect,
    const EditorGeometricReference* ref, const char* label, SDL_Color color);

static bool reference_visible(const EditorGeometricReference* ref);
static void travel_rail(SDL_Renderer* renderer, CorePaneRect rect, const LayoutConstraint* c) {
    if(!c || c->kind!=LAYOUT_CONSTRAINT_LINEAR_TRAVEL || !reference_visible(&c->a) || !reference_visible(&c->b))return;
    GlobalState* state=Global_Get();LayoutResolvedReference a;
    if(Layout_ResolveReference(&state->layout,&c->a,&a)!=LAYOUT_MEASUREMENT_OK)return;
    double length=hypot(hypot(c->axis.x,c->axis.y),c->axis.z),scale=Layout_WorldScale(&state->layout);
    if(!isfinite(length) || length<=1e-12)return;
    double axis[3]={c->axis.x/length,c->axis.y/length,c->axis.z/length};
    const double limits[]={c->travel_min,c->travel_max};Vec2 points[2];
    SpaceViewContext view=SpaceAdapter_BuildViewContext(state);
    TTF_Font* font=FontManager_Get(FONT_DEFAULT);
    SDL_Color color={105,240,170,255};
    SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,255);
    for(int i=0;i<2;++i) {
        Vec3 world={(float)((a.point_meters[0]+c->travel_offset[0]+axis[0]*limits[i])/scale),
                    (float)((a.point_meters[1]+c->travel_offset[1]+axis[1]*limits[i])/scale),
                    (float)((a.point_meters[2]+c->travel_offset[2]+axis[2]*limits[i])/scale)};
        points[i]=WorldToScreen(SpaceAdapter_ProjectToView(world,&view),&state->grid);
        if(!inside(rect,points[i].x,points[i].y))return;
    }
    SDL_RenderDrawLine(renderer,(int)points[0].x,(int)points[0].y,(int)points[1].x,(int)points[1].y);
    for(int i=0;i<2;++i) {
        SDL_RenderDrawLine(renderer,(int)points[i].x-8,(int)points[i].y,(int)points[i].x+8,(int)points[i].y);
        double value=limits[i];(void)core_units_convert(value,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&value);
        char text[64];snprintf(text,sizeof(text),"%s %.5g %s",i?"Max":"Min",value,UIPanel_GetDisplayUnitSymbol());
        int label_y=(int)points[i].y-18;
        if(i && hypot(points[1].x-points[0].x,points[1].y-points[0].y)<48)label_y+=(font ? TTF_FontHeight(font)+6 : 26);
        if(font)UIPanelSummary_DrawText(renderer,font,text,(int)points[i].x+12,label_y,color);
    }
}
/* Visual range guide only: it does not claim swept-volume or collision analysis. */
static void hinge_arc(SDL_Renderer* renderer,CorePaneRect rect,const LayoutConstraint* c) {
    if(!c || c->kind!=LAYOUT_CONSTRAINT_ANGULAR_TRAVEL || !reference_visible(&c->a) || !reference_visible(&c->b))return;
    GlobalState* state=Global_Get();LayoutResolvedReference a;
    if(Layout_ResolveReference(&state->layout,&c->a,&a)!=LAYOUT_MEASUREMENT_OK || !a.has_direction)return;
    double length=hypot(hypot(c->axis.x,c->axis.y),c->axis.z),scale=Layout_WorldScale(&state->layout);
    double axis[]={c->axis.x/length,c->axis.y/length,c->axis.z/length};
    double tangent[]={axis[1]*a.direction[2]-axis[2]*a.direction[1],axis[2]*a.direction[0]-axis[0]*a.direction[2],axis[0]*a.direction[1]-axis[1]*a.direction[0]};
    SpaceViewContext view=SpaceAdapter_BuildViewContext(state);Vec2 last={0};
    double radius=.35; /* A physical guide radius; independent of geometry dimensions. */
    SDL_SetRenderDrawColor(renderer,105,240,170,255);TTF_Font* font=FontManager_Get(FONT_DEFAULT);
    for(int i=0;i<=48;++i) {
        double value=c->travel_min+(c->travel_max-c->travel_min)*i/48,angle=value*3.14159265358979323846/180;
        Vec3 world={(float)((a.point_meters[0]+radius*(a.direction[0]*cos(angle)+tangent[0]*sin(angle)))/scale),
                    (float)((a.point_meters[1]+radius*(a.direction[1]*cos(angle)+tangent[1]*sin(angle)))/scale),
                    (float)((a.point_meters[2]+radius*(a.direction[2]*cos(angle)+tangent[2]*sin(angle)))/scale)};
        Vec2 point=WorldToScreen(SpaceAdapter_ProjectToView(world,&view),&state->grid);
        if(i && inside(rect,last.x,last.y) && inside(rect,point.x,point.y))SDL_RenderDrawLine(renderer,(int)last.x,(int)last.y,(int)point.x,(int)point.y);
        if((i==0 || i==48) && inside(rect,point.x,point.y)) {
            Vec2 pivot;if(project(&c->a,&pivot,NULL))SDL_RenderDrawLine(renderer,(int)pivot.x,(int)pivot.y,(int)point.x,(int)point.y);
            char text[64];snprintf(text,sizeof(text),"%s %.4g deg",i ? "Max" : "Min",value);
            if(font)UIPanelSummary_DrawText(renderer,font,text,(int)point.x+8,(int)point.y-18,(SDL_Color){105,240,170,255});
        }
        last=point;
    }
}
void UIPanel_RenderMeasurementViewport(SDL_Renderer* renderer) {
    CorePaneRect rect;
    if (!renderer || !viewport(&rect)) return;
    UIPanelState* ui = UIPanel_Get();
    SDL_Rect old_clip;
    bool had_clip = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer,&old_clip);
    SDL_Rect clip = {(int)rect.x,(int)rect.y,(int)rect.width,(int)rect.height};
    SDL_RenderSetClipRect(renderer,&clip);
    size_t count = 0;
    MeasurementMarker* list = ui->measurement.picking ? markers(&count) : NULL;
    SDL_SetRenderDrawColor(renderer,80,210,240,255);
    for (size_t i=0; i<count; ++i) {
        int x=(int)list[i].candidate.screen_x, y=(int)list[i].candidate.screen_y;
        SDL_Rect marker={x-5,y-5,10,10};
        SDL_RenderDrawRect(renderer,&marker);
    }
    free(list);
    int index=ui->measurement.constraint_index;
    if(index>=0 && (size_t)index<Global_Get()->layout.objectStore.constraintCount)
    {
        travel_rail(renderer,rect,&Global_Get()->layout.objectStore.constraints[index]);
        hinge_arc(renderer,rect,&Global_Get()->layout.objectStore.constraints[index]);
    }
    Vec2 points[2];
    bool valid[2];
    for (int i=0; i<2; ++i) valid[i]=project(&ui->measurement.refs[i],&points[i],NULL);
    bool joined=valid[0] && valid[1] && hypot(points[0].x-points[1].x,points[0].y-points[1].y)<16;
    for (int i=0; i<2; ++i) {
        if (valid[i] && inside(rect,points[i].x,points[i].y)) {
            const char* label=joined ? (i ? "" : "A/B") : (i ? "B" : "A");
            pivot_marker(renderer,rect,&ui->measurement.refs[i],label,i==0?(SDL_Color){100,210,255,255}:(SDL_Color){255,190,90,255});
        }
    }
    /* Clip endpoints before converting huge offscreen projections to integer pixels. */
    if (valid[0] && valid[1] && inside(rect,points[0].x,points[0].y) && inside(rect,points[1].x,points[1].y))
        SDL_RenderDrawLine(renderer,(int)points[0].x,(int)points[0].y,(int)points[1].x,(int)points[1].y);
    SDL_RenderSetClipRect(renderer,had_clip?&old_clip:NULL);
}

/* X-ray engineering overlay. Only rules touching the selected object are shown;
 * hidden participants are excluded, and nothing here mutates scene or history. */
static bool reference_visible(const EditorGeometricReference* ref) {
    const LayoutObjectStore* store=&Global_Get()->layout.objectStore;
    for (size_t i=0;i<store->count;++i) {
        const Object3D* o=&store->items[i];
        if (!o->isDeleted && !strcmp(o->coreMeta.object_id,ref->entity_id)) return Layout_ObjectShown(&Global_Get()->layout.objectStore,o);
    }
    return false;
}
static void pivot_marker(SDL_Renderer* renderer, CorePaneRect rect,
    const EditorGeometricReference* ref, const char* label, SDL_Color color) {
    Vec2 p;
    if (!reference_visible(ref) || !project(ref,&p,NULL) || !inside(rect,p.x,p.y)) return;
    int x=(int)p.x,y=(int)p.y;
    SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,255);
    SDL_Rect marker={x-6,y-6,12,12};
    SDL_RenderDrawRect(renderer,&marker);
    SDL_RenderDrawLine(renderer,x-10,y,x+10,y);
    SDL_RenderDrawLine(renderer,x,y-10,x,y+10);
    LayoutResolvedReference resolved;
    if (Layout_ResolveReference(&Global_Get()->layout,ref,&resolved)==LAYOUT_MEASUREMENT_OK && resolved.has_direction) {
        GlobalState* state=Global_Get();
        double scale=Layout_WorldScale(&state->layout);
        Vec3 point={(float)(resolved.point_meters[0]/scale),(float)(resolved.point_meters[1]/scale),(float)(resolved.point_meters[2]/scale)};
        Vec3 end=Vec3_Add(point,(Vec3){(float)resolved.direction[0],(float)resolved.direction[1],(float)resolved.direction[2]});
        SpaceViewContext view=SpaceAdapter_BuildViewContext(state);
        Vec2 e=WorldToScreen(SpaceAdapter_ProjectToView(end,&view),&state->grid);
        double dx=(double)e.x-p.x,dy=(double)e.y-p.y,length=hypot(dx,dy);
        if (isfinite(length) && length>1e-5) {
            /* Fixed screen length keeps direction rays readable at every zoom. */
            SDL_RenderDrawLine(renderer,x,y,x+(int)(48*dx/length),y+(int)(48*dy/length));
        }
    }
    TTF_Font* font=FontManager_Get(FONT_DEFAULT);
    if (font) UIPanelSummary_DrawText(renderer,font,label,x+12,y+8,color);
}
void UIPanel_RenderConstraintViewport(SDL_Renderer* renderer) {
    CorePaneRect rect;
    if (!renderer || !viewport(&rect)) return;
    GlobalState* state=Global_Get();
    const Object3D* selected=Layout_ObjectStore_FindConst(&state->layout.objectStore,state->editor.selectedObject3DId);
    if (!selected || selected->isDeleted || !Layout_ObjectShown(&state->layout.objectStore,selected)) return;
    SDL_Rect old_clip;
    bool had_clip=SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer,&old_clip);
    SDL_Rect clip={(int)rect.x,(int)rect.y,(int)rect.width,(int)rect.height};
    SDL_RenderSetClipRect(renderer,&clip);
    TTF_Font* font=FontManager_Get(FONT_DEFAULT);
    int h=font ? TTF_FontHeight(font)+6 : 24;
    int row=0,shown=0,total=0;
    int capacity=(clip.h-24)/h;
    if (capacity>6) capacity=6;
    for (size_t i=0;i<state->layout.objectStore.constraintCount;++i) {
        const LayoutConstraint* c=&state->layout.objectStore.constraints[i];
        if (strcmp(c->a.entity_id,selected->coreMeta.object_id) && strcmp(c->b.entity_id,selected->coreMeta.object_id)) continue;
        if (!reference_visible(&c->a) || !reference_visible(&c->b)) continue;
        ++total;
        if (shown>=capacity-1) continue;
        ++shown;
        travel_rail(renderer,rect,c);hinge_arc(renderer,rect,c);
        LayoutConstraintFeedback f=Layout_ConstraintFeedback(&state->layout,c);
        SDL_Color color=f.satisfied ? (SDL_Color){105,240,170,255} : (SDL_Color){255,160,95,255};
        Vec2 a,b;
        if (project(&c->a,&a,NULL) && project(&c->b,&b,NULL) && inside(rect,a.x,a.y) && inside(rect,b.x,b.y)) {
            SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,255);
            SDL_RenderDrawLine(renderer,(int)a.x,(int)a.y,(int)b.x,(int)b.y);
        }
        const UIPanelState* ui=UIPanel_Get();
        bool measure_labels=ui->measurement.active && ui->activeRightTab==UI_PANEL_RIGHT_TAB_MEASURE &&
            !memcmp(&c->a,&ui->measurement.refs[0],sizeof(c->a)) && !memcmp(&c->b,&ui->measurement.refs[1],sizeof(c->b));
        pivot_marker(renderer,rect,&c->a,measure_labels ? "" :
            (c->kind==LAYOUT_CONSTRAINT_DISTANCE || c->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL) ? "A pivot" : "A/B pivot",color);
        /* Coincident points deliberately share one marker, with two direction rays. */
        pivot_marker(renderer,rect,&c->b,!measure_labels && (c->kind==LAYOUT_CONSTRAINT_DISTANCE || c->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL) ? "B pivot" : "",color);
        char text[256];
        double actual=f.position.value,target=(c->kind==LAYOUT_CONSTRAINT_DISTANCE || c->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL) ? c->target : 0;
        if(fabs(actual)<1e-6)actual=0;
        (void)core_units_convert(actual,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&actual);
        (void)core_units_convert(target,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&target);
        if (f.position.status!=LAYOUT_MEASUREMENT_OK || ((c->kind==LAYOUT_CONSTRAINT_PLANAR_MATE || c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL) && f.angle.status!=LAYOUT_MEASUREMENT_OK))
            snprintf(text,sizeof(text),"%s: UNRESOLVED - inspect references in Measure",c->id);
        else if (c->kind==LAYOUT_CONSTRAINT_PLANAR_MATE || c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL)
            snprintf(text,sizeof(text),"%s: %s | angle %.5g / %.5g deg | pivot gap %.4g %s",c->id,f.satisfied ? "OK" : "CONFLICT",f.angle.value,c->target,actual,UIPanel_GetDisplayUnitSymbol());
        else snprintf(text,sizeof(text),"%s: %s | %s %.5g / %.5g %s",c->id,f.satisfied ? "OK" : "CONFLICT",
            (c->kind==LAYOUT_CONSTRAINT_DISTANCE || c->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL) ? "projection" : "pivot gap",actual,target,UIPanel_GetDisplayUnitSymbol());
        if (font) {
            SDL_Rect banner={clip.x+8,clip.y+8+row*h,clip.w-16,h};
            SDL_SetRenderDrawColor(renderer,25,30,38,255); SDL_RenderFillRect(renderer,&banner);
            UIPanelSummary_DrawTextClipped(renderer,font,text,banner.x+6,banner.y,banner.w-12,h,color);
        }
        ++row;
    }
    if (total && font && capacity>0) {
        char text[128];
        snprintf(text,sizeof(text),"X-ray pivots | actual / target | %d of %d rules | Measure tab: saved rules",shown,total);
        SDL_Rect banner={clip.x+8,clip.y+8+row*h,clip.w-16,h};
        SDL_SetRenderDrawColor(renderer,25,30,38,255); SDL_RenderFillRect(renderer,&banner);
        UIPanelSummary_DrawTextClipped(renderer,font,text,banner.x+6,banner.y,banner.w-12,h,(SDL_Color){220,230,245,255});
    }
    SDL_RenderSetClipRect(renderer,had_clip ? &old_clip : NULL);
}
