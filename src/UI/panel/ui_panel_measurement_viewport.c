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
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = font ? TTF_FontHeight(font)+6 : 24;
    return x >= r.x && x < r.x+r.width && y >= r.y+h*5+28 && y < r.y+r.height;
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
        if (o->isDeleted || !o->coreMeta.flags.visible || !o->coreMeta.flags.selectable) continue;
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

void UIPanel_RenderMeasurementViewport(SDL_Renderer* renderer) {
    CorePaneRect rect;
    if (!renderer || !viewport(&rect)) return;
    UIPanelState* ui = UIPanel_Get();
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = font ? TTF_FontHeight(font)+6 : 24;
    SDL_Rect old_clip;
    bool had_clip = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer,&old_clip);
    SDL_Rect clip = {(int)rect.x,(int)rect.y,(int)rect.width,(int)rect.height};
    SDL_RenderSetClipRect(renderer,&clip);
    size_t count = 0;
    MeasurementMarker* list = markers(&count);
    SDL_SetRenderDrawColor(renderer,80,210,240,255);
    for (size_t i=0; i<count; ++i) {
        int x=(int)list[i].candidate.screen_x, y=(int)list[i].candidate.screen_y;
        SDL_Rect marker={x-5,y-5,10,10};
        SDL_RenderDrawRect(renderer,&marker);
    }
    free(list);
    Vec2 points[2];
    bool valid[2];
    for (int i=0; i<2; ++i) {
        valid[i]=project(&ui->measurement.refs[i],&points[i],NULL);
        if (valid[i] && inside(rect,points[i].x,points[i].y)) {
            pivot_marker(renderer,rect,&ui->measurement.refs[i],i==0?"A":"B",(SDL_Color){255,230,100,255});
        }
    }
    /* Clip endpoints before converting huge offscreen projections to integer pixels. */
    if (valid[0] && valid[1] && inside(rect,points[0].x,points[0].y) && inside(rect,points[1].x,points[1].y))
        SDL_RenderDrawLine(renderer,(int)points[0].x,(int)points[0].y,(int)points[1].x,(int)points[1].y);
    SDL_Rect banner={clip.x+8,clip.y+8,clip.w-16,h*5+12};
    SDL_SetRenderDrawColor(renderer,25,30,38,255); SDL_RenderFillRect(renderer,&banner);
    if (font) {
        char lines[5][256];
        const EditorGeometricReference* ref=&ui->measurement.refs[ui->measurement.slot];
        const char* kind=ref->kind==EDITOR_REFERENCE_ORIGIN?"origin":ref->kind==EDITOR_REFERENCE_AXIS_U?"+U axis":
            ref->kind==EDITOR_REFERENCE_AXIS_V?"+V axis":ref->kind==EDITOR_REFERENCE_AXIS_N?"+N axis":Layout_Object3DFaceKind_Label(ref->face);
        snprintf(lines[0],sizeof(lines[0]),"Pick %c %s + local (%.4g, %.4g, %.4g) m [X-ray]",'A'+ui->measurement.slot,kind,
            ref->local_offset_meters[0],ref->local_offset_meters[1],ref->local_offset_meters[2]);
        snprintf(lines[1],sizeof(lines[1]),"Tab: A/B   Left/Right: feature   Enter/Esc: return to measurements");
        EditorMeasurementResult distance=Editor_Measure(&Global_Get()->layout,&ui->measurement.refs[0],&ui->measurement.refs[1],EDITOR_MEASURE_POINT_DISTANCE,(Vec3){0});
        if (distance.status==EDITOR_MEASUREMENT_OK) {
            double value=0; (void)core_units_convert(distance.value,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&value);
            snprintf(lines[2],sizeof(lines[2]),"A-B point distance: %.6g %s (reference only)",value,UIPanel_GetDisplayUnitSymbol());
        } else snprintf(lines[2],sizeof(lines[2]),"A-B: %s",distance.message);
        snprintf(lines[3],sizeof(lines[3]),"1: XY top   2: YZ side   3: XZ front (world views; geometry unchanged)");
        snprintf(lines[4],sizeof(lines[4]),"%s",ui->measurement.pick_message);
        for (int i=0;i<5;++i) UIPanelSummary_DrawTextClipped(renderer,font,lines[i],banner.x+6,banner.y+6+i*h,banner.w-12,h,(SDL_Color){235,240,250,255});
    }
    SDL_RenderSetClipRect(renderer,had_clip?&old_clip:NULL);
}

/* X-ray engineering overlay. Only rules touching the selected object are shown;
 * hidden participants are excluded, and nothing here mutates scene or history. */
static bool reference_visible(const EditorGeometricReference* ref) {
    const LayoutObjectStore* store=&Global_Get()->layout.objectStore;
    for (size_t i=0;i<store->count;++i) {
        const Object3D* o=&store->items[i];
        if (!o->isDeleted && !strcmp(o->coreMeta.object_id,ref->entity_id)) return o->coreMeta.flags.visible;
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
    if (!selected || selected->isDeleted || !selected->coreMeta.flags.visible) return;
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
        LayoutConstraintFeedback f=Layout_ConstraintFeedback(&state->layout,c);
        SDL_Color color=f.satisfied ? (SDL_Color){105,240,170,255} : (SDL_Color){255,160,95,255};
        Vec2 a,b;
        if (project(&c->a,&a,NULL) && project(&c->b,&b,NULL) && inside(rect,a.x,a.y) && inside(rect,b.x,b.y)) {
            SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,255);
            SDL_RenderDrawLine(renderer,(int)a.x,(int)a.y,(int)b.x,(int)b.y);
        }
        pivot_marker(renderer,rect,&c->a,c->kind==LAYOUT_CONSTRAINT_DISTANCE ? "A pivot" : "A/B pivot",color);
        /* Coincident points deliberately share one marker, with two direction rays. */
        pivot_marker(renderer,rect,&c->b,c->kind==LAYOUT_CONSTRAINT_DISTANCE ? "B pivot" : "",color);
        char text[256];
        double actual=f.position.value,target=c->kind==LAYOUT_CONSTRAINT_DISTANCE ? c->target : 0;
        (void)core_units_convert(actual,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&actual);
        (void)core_units_convert(target,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&target);
        if (f.position.status!=LAYOUT_MEASUREMENT_OK || (c->kind==LAYOUT_CONSTRAINT_PLANAR_MATE && f.angle.status!=LAYOUT_MEASUREMENT_OK))
            snprintf(text,sizeof(text),"%s: UNRESOLVED - inspect references in Measure",c->id);
        else if (c->kind==LAYOUT_CONSTRAINT_PLANAR_MATE)
            snprintf(text,sizeof(text),"%s: %s | angle %.5g / %.5g deg | pivot gap %.4g %s",c->id,f.satisfied ? "OK" : "CONFLICT",f.angle.value,c->target,actual,UIPanel_GetDisplayUnitSymbol());
        else snprintf(text,sizeof(text),"%s: %s | %s %.5g / %.5g %s",c->id,f.satisfied ? "OK" : "CONFLICT",
            c->kind==LAYOUT_CONSTRAINT_DISTANCE ? "projection" : "pivot gap",actual,target,UIPanel_GetDisplayUnitSymbol());
        if (font) {
            SDL_Rect banner={clip.x+8,clip.y+8+row*h,clip.w-16,h};
            SDL_SetRenderDrawColor(renderer,25,30,38,255); SDL_RenderFillRect(renderer,&banner);
            UIPanelSummary_DrawTextClipped(renderer,font,text,banner.x+6,banner.y,banner.w-12,h,color);
        }
        ++row;
    }
    if (total && font && capacity>0) {
        char text[128];
        snprintf(text,sizeof(text),"X-ray pivots | actual / target | %d of %d rules | Measure: Q inspect/edit",shown,total);
        SDL_Rect banner={clip.x+8,clip.y+8+row*h,clip.w-16,h};
        SDL_SetRenderDrawColor(renderer,25,30,38,255); SDL_RenderFillRect(renderer,&banner);
        UIPanelSummary_DrawTextClipped(renderer,font,text,banner.x+6,banner.y,banner.w-12,h,(SDL_Color){220,230,245,255});
    }
    SDL_RenderSetClipRect(renderer,had_clip ? &old_clip : NULL);
}
