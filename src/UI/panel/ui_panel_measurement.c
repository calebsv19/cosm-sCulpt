#include "UI/ui_panel_measurement.h"
#include "UI/ui_panel_shell.h"
#include "Core/global_state.h"
#include "Editor/editor_reference_edit.h"
#include "Layout/layout_constraints.h"
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include "Layout/scene/layout_object_faces.h"
#include "UI/font_manager.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"
#include <stdio.h>
#include <string.h>

static void cycle_object(int step) {
    UIPanelState* ui = UIPanel_Get();
    ui->measurement.constraint_index = -1;
    ui->measurement.observed_rule_valid = false;
    const LayoutObjectStore* store = &Global_Get()->layout.objectStore;
    EditorGeometricReference* ref = &ui->measurement.refs[ui->measurement.slot];
    if (!store->count) return;
    size_t current = step > 0 ? store->count - 1 : 0;
    for (size_t i = 0; i < store->count; ++i)
        if (strcmp(store->items[i].coreMeta.object_id, ref->entity_id) == 0) current = i;
    for (size_t n = 0; n < store->count; ++n) {
        current = step > 0 ? (current + 1) % store->count : (current + store->count - 1) % store->count;
        const Object3D* o = &store->items[current];
        if (o->isDeleted || (o->kind != OBJECT3D_KIND_PLANE && o->kind != OBJECT3D_KIND_RECT_PRISM)) continue;
        snprintf(ref->entity_id, sizeof(ref->entity_id), "%s", o->coreMeta.object_id);
        ref->kind = EDITOR_REFERENCE_ORIGIN;
        ref->face = OBJECT3D_FACE_NONE;
        memset(ref->local_offset_meters, 0, sizeof(ref->local_offset_meters));
        return;
    }
}

static void cycle_feature(int step) {
    UIPanelState* ui = UIPanel_Get();
    ui->measurement.constraint_index = -1;
    ui->measurement.observed_rule_valid = false;
    EditorGeometricReference* ref = &ui->measurement.refs[ui->measurement.slot];
    const LayoutObjectStore* store = &Global_Get()->layout.objectStore;
    const Object3D* object = NULL;
    for (size_t i = 0; i < store->count; ++i)
        if (!store->items[i].isDeleted && strcmp(store->items[i].coreMeta.object_id, ref->entity_id) == 0) object = &store->items[i];
    if (!object) return;
    const int count = object->kind == OBJECT3D_KIND_PLANE ? 5 : 10;
    int index = (int)ref->kind;
    if (ref->kind == EDITOR_REFERENCE_FACE && object->kind == OBJECT3D_KIND_RECT_PRISM)
        index = 4 + ref->face - OBJECT3D_FACE_RECT_PRISM_NEG_N;
    index = (index + count + step) % count;
    ref->kind = index < 4 ? (EditorReferenceKind)index : EDITOR_REFERENCE_FACE;
    ref->face = index < 4 ? OBJECT3D_FACE_NONE : object->kind == OBJECT3D_KIND_PLANE
        ? OBJECT3D_FACE_PLANE_SURFACE : (Object3DFaceKind)(OBJECT3D_FACE_RECT_PRISM_NEG_N + index - 4);
}

bool UIPanel_MotionMatchesEntity(const LayoutConstraint* rule, const char* id) {
    if (!rule || !id || !id[0] || (rule->kind != LAYOUT_CONSTRAINT_LINEAR_TRAVEL &&
        rule->kind != LAYOUT_CONSTRAINT_ANGULAR_TRAVEL)) return false;
    const LayoutObjectStore* store = &Global_Get()->layout.objectStore;
    const LayoutEntityInfo* info = Layout_EntityInfo(store, id);
    if (!info) return false;
    if (!strcmp(rule->b.entity_id, id)) return true;
    return rule->motion_assembly[0] && (!strcmp(rule->motion_assembly, id) ||
        Layout_IsDescendant(store, info->parent_id, rule->motion_assembly));
}

bool UIPanel_BeginEntityMotion(const char* entity_id) {
    const LayoutObjectStore* store = &Global_Get()->layout.objectStore;
    int found = -1;
    for (size_t i = 0; i < store->constraintCount; ++i) {
        if (!UIPanel_MotionMatchesEntity(&store->constraints[i], entity_id)) continue;
        if (found < 0) found = (int)i;
        if (!strcmp(store->constraints[i].b.entity_id, entity_id)) { found = (int)i; break; }
    }
    if (found < 0) return false;
    UIPanelState* ui = UIPanel_Get();
    UIPanel_MeasurementStopInput();
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_MEASURE);
    UIPanel_MeasurementSelectRule(found);
    ui->measurement.motion_preview = true;
    ui->measurement.motion_edit_open = false;
    ui->measurement.motion_selection = Global_Get()->editor.selectedObject3DId;
    snprintf(ui->measurement.motion_entity, 64, "%s", entity_id);
    ui->rightScroll[UI_PANEL_RIGHT_TAB_MEASURE].scrollOffsetPx = 0;
    return true;
}

bool UIPanel_BeginSelectedMotion(void) {
    const Object3D* o = Layout_ObjectStore_FindConst(&Global_Get()->layout.objectStore,
        Global_Get()->editor.selectedObject3DId);
    return o && !o->isDeleted && UIPanel_BeginEntityMotion(o->coreMeta.object_id);
}

void UIPanel_MeasurementRefreshHistory(void) {
    UIPanelState* ui = UIPanel_Get();
    UIPanel_MeasurementStopInput();
    if (!ui->measurement.observed_rule_valid) return;
    char id[64]; snprintf(id, sizeof(id), "%s", ui->measurement.observed_rule.id);
    const LayoutObjectStore* store = &Global_Get()->layout.objectStore;
    for (size_t i = 0; i < store->constraintCount; ++i) if (!strcmp(id, store->constraints[i].id)) {
        UIPanel_MeasurementSelectRule((int)i);
        return;
    }
    ui->measurement.constraint_index = -1;
    ui->measurement.motion_preview = false;
    snprintf(ui->measurement.placement_message, 128, "This constraint no longer exists. Undo can restore it.");
}

bool UIPanel_BeginMeasurement(void) {
    UIPanelState* ui = UIPanel_Get();
    memset(&ui->measurement, 0, sizeof(ui->measurement));
    ui->measurement.active = true;
    ui->measurement.motion_selection = Global_Get()->editor.selectedObject3DId;
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_MEASURE);
    ui->measurement.constraint_index = -1;
    const Object3D* selected = Layout_ObjectStore_FindConst(&Global_Get()->layout.objectStore,
        Global_Get()->editor.selectedObject3DId);
    if (selected && (selected->kind == OBJECT3D_KIND_PLANE || selected->kind == OBJECT3D_KIND_RECT_PRISM))
        snprintf(ui->measurement.refs[0].entity_id, sizeof(ui->measurement.refs[0].entity_id), "%s", selected->coreMeta.object_id);
    else cycle_object(1);
    ui->measurement.refs[1] = ui->measurement.refs[0];
    ui->measurement.slot = 1;
    cycle_object(1);
    ui->measurement.slot = 0;
    (void)UIPanel_BeginSelectedMotion();
    UIPanel_OnWindowResized(Global_GetScreenWidth(),Global_GetScreenHeight());
    return true;
}

static void finish_placement(void) {
    UIPanelState* ui = UIPanel_Get();
    UIPanel_TravelStageInput();
    if (ui->measurement.placement_started_text_input) SDL_StopTextInput();
    ui->measurement.placement_started_text_input = false;
    ui->measurement.placing = 0;
}

static void begin_placement(int mode) {
    UIPanelState* ui = UIPanel_Get();
    ui->measurement.placing = mode;
    ui->measurement.placement_text[0] = '\0';
    ui->measurement.placement_message[0] = '\0';
    if ((mode == 1 || mode == 3 || mode == 5 || mode >= 7) && !SDL_IsTextInputActive()) {
        SDL_StartTextInput();
        ui->measurement.placement_started_text_input = true;
    }
}

bool UIPanel_MeasurementText(const char* text) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui->measurement.active || (ui->measurement.placing != 1 && ui->measurement.placing != 3 && ui->measurement.placing != 5 && ui->measurement.placing < 7) || !text) return false;
    if (ui->measurement.replace_text) {
        ui->measurement.placement_text[0]=0;
        ui->measurement.replace_text=false;
    }
    const size_t have = strlen(ui->measurement.placement_text), added = strlen(text);
    if (have + added >= sizeof(ui->measurement.placement_text)) return true;
    /* Length expressions here use an ASCII scalar and optional unit suffix. */
    for (size_t i = 0; i < added; ++i) if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126) return true;
    memcpy(ui->measurement.placement_text + have, text, added + 1);
    return true;
}

static const LayoutConstraint* selected_rule(void) {
    int index = UIPanel_Get()->measurement.constraint_index;
    const LayoutObjectStore* store = &Global_Get()->layout.objectStore;
    return index >= 0 && (size_t)index < store->constraintCount ? &store->constraints[index] : NULL;
}

static void cycle_rule(void) {
    UIPanelState* ui = UIPanel_Get();
    if (++ui->measurement.constraint_index >= (int)Global_Get()->layout.objectStore.constraintCount)
        ui->measurement.constraint_index = -1;
    const LayoutConstraint* rule = selected_rule();
    ui->measurement.use_rule_axis = rule != NULL;
    if (rule) {
        ui->measurement.observed_rule=*rule;ui->measurement.observed_rule_valid=true;
        if((rule->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL || rule->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL))UIPanel_TravelSelect(rule);
        ui->measurement.refs[0] = rule->a;
        ui->measurement.refs[1] = rule->b;
        ui->measurement.operation=rule->kind==LAYOUT_CONSTRAINT_DISTANCE ? 0 : rule->kind==LAYOUT_CONSTRAINT_COINCIDENT ? 1 : rule->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL ? 3 : rule->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL ? 4 : 2;
        double value=rule->target;
        if(rule->kind!=LAYOUT_CONSTRAINT_PLANAR_MATE && rule->kind!=LAYOUT_CONSTRAINT_ANGULAR_TRAVEL)(void)core_units_convert(value,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&value);
        snprintf(ui->measurement.placement_text,64,"%.9g %s",value,(rule->kind==LAYOUT_CONSTRAINT_PLANAR_MATE || rule->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL) ? "" : UIPanel_GetDisplayUnitSymbol());
        snprintf(ui->measurement.placement_message,128,"Edit the target, then click Apply changes.");
    } else snprintf(ui->measurement.placement_message, sizeof(ui->measurement.placement_message), "New rule mode; choose references and axis.");
}

static bool apply_rule(void) {
    UIPanelState* ui = UIPanel_Get();
    const LayoutConstraint* selected = selected_rule();
    if(selected && (selected->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL || selected->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL) && ui->measurement.placing!=6) {
        snprintf(ui->measurement.placement_message,128,"Use Travel to update this rule, or remove it first.");return false;
    }
    const Vec3 axes[] = {{1,0,0}, {0,1,0}, {0,0,1}};
    LayoutConstraint rule = {.a=ui->measurement.refs[0], .b=ui->measurement.refs[1],
        .kind=ui->measurement.placing==3 ? LAYOUT_CONSTRAINT_DISTANCE : ui->measurement.placing==4
            ? LAYOUT_CONSTRAINT_COINCIDENT : LAYOUT_CONSTRAINT_PLANAR_MATE,
        .axis=ui->measurement.placing==5 ? axes[(ui->measurement.angle_plane+2)%3] : axes[ui->measurement.projection_axis]};
    if (selected) {
        snprintf(rule.id, sizeof(rule.id), "%s", selected->id);
        if (selected->kind == rule.kind && ui->measurement.use_rule_axis) rule.axis = selected->axis;
    }
    if (ui->measurement.placing == 3 && !Editor_ParseLength(ui->measurement.placement_text, UIPanel_GetDisplayUnit(), &rule.target)) {
        snprintf(ui->measurement.placement_message, sizeof(ui->measurement.placement_message), "Enter a finite length, e.g. 20 mm.");
        return false;
    }
    if (ui->measurement.placing == 5) {
        char* end;
        rule.target = strtod(ui->measurement.placement_text, &end);
        if (end == ui->measurement.placement_text) {
            snprintf(ui->measurement.placement_message, sizeof(ui->measurement.placement_message), "Enter a signed angle in degrees, e.g. 30.");
            return false;
        }
        while (isspace((unsigned char)*end)) ++end;
        if (*end || !isfinite(rule.target) || rule.target <= -180 || rule.target > 180) {
            snprintf(ui->measurement.placement_message, sizeof(ui->measurement.placement_message), "Enter degrees greater than -180 and at most 180, e.g. 30.");
            return false;
        }
    }
    if (ui->measurement.placing == 6 && !selected) return false;
    char remove_id[64] = {0};
    if (selected) snprintf(remove_id, sizeof(remove_id), "%s", selected->id);
    bool ok = Layout_ConstraintEdit(&Global_Get()->layout, ui->measurement.placing==6 ? NULL : &rule,
        remove_id, Editor_ReserveGeometryHistory, &Global_Get()->editor);
    snprintf(ui->measurement.placement_message, sizeof(ui->measurement.placement_message), "%s",
        ok ? "Saved rule change; dependent geometry validated. Undo restores both." : Global_Get()->layout.geometryMessage);
    if (ok) ui->measurement.constraint_index = -1;
    return ok;
}

static void apply_placement(void) {
    UIPanelState* ui = UIPanel_Get();
    if(ui->measurement.placing==13){(void)UIPanel_ApplyEngineeringGrid(ui->measurement.placement_text);return;}
    if (ui->measurement.placing >= 10) { finish_placement(); return; }
    if (ui->measurement.placing >= 7) {
        double meters;
        if (!Editor_ParseLength(ui->measurement.placement_text,UIPanel_GetDisplayUnit(),&meters)) {
            snprintf(ui->measurement.placement_message,sizeof(ui->measurement.placement_message),"Enter a finite local offset, e.g. 25 mm; 0 resets it.");
            return;
        }
        EditorGeometricReference candidate=ui->measurement.refs[ui->measurement.slot];
        candidate.local_offset_meters[ui->measurement.placing-7]=meters;
        EditorResolvedReference resolved;
        if (Editor_ResolveReference(&Global_Get()->layout,&candidate,&resolved)!=EDITOR_MEASUREMENT_OK) {
            snprintf(ui->measurement.placement_message,sizeof(ui->measurement.placement_message),"Offset cannot resolve on this reference.");
            return;
        }
        ui->measurement.refs[ui->measurement.slot]=candidate;
        snprintf(ui->measurement.placement_message,sizeof(ui->measurement.placement_message),"Offset staged. Save or update a rule to keep it.");
        finish_placement();
        return;
    }
    if (ui->measurement.placing >= 3) {
        if (apply_rule()) finish_placement();
        return;
    }
    const Vec3 axes[] = {{1,0,0}, {0,1,0}, {0,0,1}};
    EditorReferencePlacement command = {.a = ui->measurement.refs[0], .b = ui->measurement.refs[1],
        .kind = ui->measurement.placing == 1 ? EDITOR_PLACE_PROJECTED_DISTANCE : EDITOR_PLACE_COINCIDENT_POINTS,
        .world_axis = axes[ui->measurement.projection_axis]};
    if (ui->measurement.placing == 1 && !Editor_ParseLength(ui->measurement.placement_text,
        UIPanel_GetDisplayUnit(), &command.target_meters)) {
        snprintf(ui->measurement.placement_message, sizeof(ui->measurement.placement_message), "Enter a finite length, e.g. 20 mm or -0.5 m.");
        return;
    }
    EditorNumericEditResult r = Editor_ApplyReferencePlacement(&Global_Get()->editor, &Global_Get()->layout, &command);
    snprintf(ui->measurement.placement_message, sizeof(ui->measurement.placement_message), "%s", r.message);
    if (r.status == EDITOR_NUMERIC_APPLIED || r.status == EDITOR_NUMERIC_UNCHANGED) finish_placement();
}

bool UIPanel_MeasurementKey(SDL_Keycode key) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui->measurement.active) return false;
    if (ui->measurement.placing) {
        if (key == SDLK_ESCAPE) finish_placement();
        else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) apply_placement();
        else if (key == SDLK_BACKSPACE) {
            if (ui->measurement.replace_text) {
                ui->measurement.placement_text[0]=0;
                ui->measurement.replace_text=false;
            }
            size_t n = strlen(ui->measurement.placement_text);
            if (n) ui->measurement.placement_text[n-1] = '\0';
        }
        return true;
    }
    if (!ui->measurement.picking && (key == SDLK_x || key == SDLK_y || key == SDLK_z)) {
        begin_placement(key==SDLK_x ? 7 : key==SDLK_y ? 8 : 9);
    } else if (!ui->measurement.picking && key == SDLK_q) {
        cycle_rule();
    } else if (!ui->measurement.picking && (key == SDLK_r || key == SDLK_o || key == SDLK_m)) {
        begin_placement(key == SDLK_r ? 3 : key == SDLK_o ? 4 : 5);
    } else if (!ui->measurement.picking && key == SDLK_DELETE && selected_rule()) {
        begin_placement(6);
    } else if (!ui->measurement.picking && (key == SDLK_d || key == SDLK_c)) {
        begin_placement(key == SDLK_d ? 1 : 2);
    } else if (key == SDLK_k) {
        ui->measurement.picking = !ui->measurement.picking;
        ui->measurement.pick_message[0] = '\0';
    } else if (key == SDLK_ESCAPE || key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        if (ui->measurement.picking) ui->measurement.picking = false;
        else UIPanel_SetActiveRightTab(ui,UI_PANEL_RIGHT_TAB_VIEW);
    }
    else if (ui->measurement.picking && (key == SDLK_1 || key == SDLK_2 || key == SDLK_3)) {
        GlobalState* state = Global_Get();
        state->activePlane = (ViewPlane){.axis = key == SDLK_1 ? VIEW_PLANE_XY : key == SDLK_2 ? VIEW_PLANE_YZ : VIEW_PLANE_XZ, .offset = 0};
        state->freeViewCamera.enabled = false;
        Global_FlagGridChanged();
        Global_FlagHitboxesDirty();
    }
    else if (key == SDLK_TAB) ui->measurement.slot = 1 - ui->measurement.slot;
    else if (key == SDLK_UP || key == SDLK_DOWN) cycle_object(key == SDLK_DOWN ? 1 : -1);
    else if (key == SDLK_LEFT || key == SDLK_RIGHT) cycle_feature(key == SDLK_RIGHT ? 1 : -1);
    else if (key == SDLK_p) { ui->measurement.use_rule_axis = false; ui->measurement.projection_axis = (ui->measurement.projection_axis + 1) % 3; }
    else if (key == SDLK_n) { ui->measurement.use_rule_axis = false; ui->measurement.angle_plane = (ui->measurement.angle_plane + 1) % 3; }
    return true;
}


void UIPanel_MeasurementStopInput(void) {
    UIPanel_TravelEndDrag();
    finish_placement();
    UIPanel_Get()->measurement.picking=false;
    UIPanel_Get()->measurement.chooser=0;
}
void UIPanel_MeasurementStartValue(int mode) {
    char previous[64];
    snprintf(previous,sizeof(previous),"%s",UIPanel_Get()->measurement.placement_text);
    finish_placement();
    begin_placement(mode);
    snprintf(UIPanel_Get()->measurement.placement_text,sizeof(previous),"%s",previous);
    UIPanel_Get()->measurement.replace_text=true;
}
void UIPanel_MeasurementApplyButton(int mode) {
    UIPanelState* ui=UIPanel_Get();
    int index=ui->measurement.constraint_index;
    ui->measurement.placing=mode;
    apply_placement();
    if (mode>=7 && mode<=9 && !ui->measurement.placing) ui->measurement.placement_text[0]=0;
    if (mode>=3 && mode<=5 && !ui->measurement.placing) {
        if (index<0) index=(int)Global_Get()->layout.objectStore.constraintCount-1;
        UIPanel_MeasurementSelectRule(index);
        snprintf(ui->measurement.placement_message,128,"Constraint applied. Undo is available.");
    }
}
void UIPanel_MeasurementSelectRule(int index) {
    UIPanel_Get()->measurement.constraint_index=index-1;
    cycle_rule();
    const LayoutConstraint* c=selected_rule();
    if (c) {
        UIPanel_Get()->measurement.operation=c->kind==LAYOUT_CONSTRAINT_DISTANCE ? 0 : c->kind==LAYOUT_CONSTRAINT_COINCIDENT ? 1 : c->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL ? 3 : c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL ? 4 : 2;
        double value=c->target;
        if(c->kind!=LAYOUT_CONSTRAINT_PLANAR_MATE && c->kind!=LAYOUT_CONSTRAINT_ANGULAR_TRAVEL)(void)core_units_convert(value,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&value);
        snprintf(UIPanel_Get()->measurement.placement_text,64,"%.9g %s",value,(c->kind==LAYOUT_CONSTRAINT_PLANAR_MATE || c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL) ? "" : UIPanel_GetDisplayUnitSymbol());
        UIPanel_Get()->measurement.placement_message[0]=0;
    }
}
