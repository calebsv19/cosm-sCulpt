#include "UI/ui_panel_measurement.h"
#include "Core/global_state.h"
#include "Editor/editor_reference_edit.h"
#include "Layout/scene/layout_object_faces.h"
#include "UI/font_manager.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"
#include <stdio.h>
#include <string.h>

static void cycle_object(int step) {
    UIPanelState* ui = UIPanel_Get();
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
        return;
    }
}

static void cycle_feature(int step) {
    UIPanelState* ui = UIPanel_Get();
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

bool UIPanel_BeginMeasurement(void) {
    UIPanelState* ui = UIPanel_Get();
    memset(&ui->measurement, 0, sizeof(ui->measurement));
    ui->measurement.active = true;
    const Object3D* selected = Layout_ObjectStore_FindConst(&Global_Get()->layout.objectStore,
        Global_Get()->editor.selectedObject3DId);
    if (selected && (selected->kind == OBJECT3D_KIND_PLANE || selected->kind == OBJECT3D_KIND_RECT_PRISM))
        snprintf(ui->measurement.refs[0].entity_id, sizeof(ui->measurement.refs[0].entity_id), "%s", selected->coreMeta.object_id);
    else cycle_object(1);
    ui->measurement.refs[1] = ui->measurement.refs[0];
    ui->measurement.slot = 1;
    cycle_object(1);
    ui->measurement.slot = 0;
    return true;
}

static void finish_placement(void) {
    UIPanelState* ui = UIPanel_Get();
    if (ui->measurement.placement_started_text_input) SDL_StopTextInput();
    ui->measurement.placement_started_text_input = false;
    ui->measurement.placing = 0;
}

static void begin_placement(int mode) {
    UIPanelState* ui = UIPanel_Get();
    ui->measurement.placing = mode;
    ui->measurement.placement_text[0] = '\0';
    ui->measurement.placement_message[0] = '\0';
    if (mode == 1 && !SDL_IsTextInputActive()) {
        SDL_StartTextInput();
        ui->measurement.placement_started_text_input = true;
    }
}

bool UIPanel_MeasurementText(const char* text) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui->measurement.active || ui->measurement.placing != 1 || !text) return false;
    const size_t have = strlen(ui->measurement.placement_text), added = strlen(text);
    if (have + added >= sizeof(ui->measurement.placement_text)) return true;
    /* Length expressions here use an ASCII scalar and optional unit suffix. */
    for (size_t i = 0; i < added; ++i) if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126) return true;
    memcpy(ui->measurement.placement_text + have, text, added + 1);
    return true;
}

static void apply_placement(void) {
    UIPanelState* ui = UIPanel_Get();
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
            size_t n = strlen(ui->measurement.placement_text);
            if (n) ui->measurement.placement_text[n-1] = '\0';
        }
        return true;
    }
    if (!ui->measurement.picking && (key == SDLK_d || key == SDLK_c)) {
        begin_placement(key == SDLK_d ? 1 : 2);
    } else if (key == SDLK_k) {
        ui->measurement.picking = !ui->measurement.picking;
        ui->measurement.pick_message[0] = '\0';
    } else if (key == SDLK_ESCAPE || key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        if (ui->measurement.picking) ui->measurement.picking = false;
        else ui->measurement.active = false;
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
    else if (key == SDLK_p) ui->measurement.projection_axis = (ui->measurement.projection_axis + 1) % 3;
    else if (key == SDLK_n) ui->measurement.angle_plane = (ui->measurement.angle_plane + 1) % 3;
    return true;
}

static SDL_Rect panel_rect(int* line_height) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = font ? TTF_FontHeight(font) + 8 : 26;
    if (line_height) *line_height = h;
    int width = Global_GetScreenWidth() - 32;
    if (width > 820) width = 820;
    return (SDL_Rect){(Global_GetScreenWidth() - width)/2, 40, width, h * 16 + 24};
}

bool UIPanel_MeasurementClick(int x, int y) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui->measurement.active) return false;
    if (ui->measurement.picking) {
        (void)UIPanel_MeasurementPickAt(x, y);
        return true;
    }
    int h;
    SDL_Rect p = panel_rect(&h);
    if (x < p.x || x >= p.x+p.w || y < p.y+12) return true;
    int row = (y-p.y-12)/h;
    if (ui->measurement.placing) {
        if (row == 13) apply_placement();
        else if (row == 15) finish_placement();
        return true;
    }
    if (row == 13) begin_placement(x < p.x+p.w/2 ? 1 : 2);
    else if (row == 7) ui->measurement.picking = true;
    else if (row == 3 || row == 4) {
        ui->measurement.slot = row - 3;
        if (x < p.x+p.w/2) cycle_object(1);
        else cycle_feature(1);
    } else if (row == 5) ui->measurement.projection_axis = (ui->measurement.projection_axis + 1) % 3;
    else if (row == 6) ui->measurement.angle_plane = (ui->measurement.angle_plane + 1) % 3;
    else if (row == 15) ui->measurement.active = false;
    return true;
}

static const char* feature_label(const EditorGeometricReference* ref) {
    switch (ref->kind) {
        case EDITOR_REFERENCE_ORIGIN: return "Origin (point)";
        case EDITOR_REFERENCE_AXIS_U: return "+U axis";
        case EDITOR_REFERENCE_AXIS_V: return "+V axis";
        case EDITOR_REFERENCE_AXIS_N: return "+N axis";
        default: return Layout_Object3DFaceKind_Label(ref->face);
    }
}

void UIPanel_RenderMeasurement(SDL_Renderer* renderer) {
    UIPanelState* ui = UIPanel_Get();
    if (!renderer || !ui->measurement.active) return;
    if (ui->measurement.picking) {
        UIPanel_RenderMeasurementViewport(renderer);
        return;
    }
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;
    UIPanelVisualPalette palette = {0};
    (void)UIPanelVisual_ResolvePalette(&palette);
    palette.pane_fill.a = 255;
    int h;
    SDL_Rect p = panel_rect(&h);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(renderer, NULL);
    UIPanelSummary_DrawCard(renderer, p, palette.pane_fill, palette.pane_border, palette.accent, 3);
    char lines[16][256] = {{0}};
    snprintf(lines[0], sizeof(lines[0]), "Measure / place references (A fixed, B moves once)");
    snprintf(lines[1], sizeof(lines[1]), "Tab: A/B   Up/Down: object   Left/Right: feature");
    snprintf(lines[2], sizeof(lines[2]), "Click row left: next object; right: next feature");
    for (int i = 0; i < 2; ++i)
        snprintf(lines[3+i], sizeof(lines[3+i]), "%c %c: %.63s     |     %s",
            ui->measurement.slot == i ? '>' : ' ', 'A'+i,
            ui->measurement.refs[i].entity_id[0] ? ui->measurement.refs[i].entity_id : "No primitive",
            feature_label(&ui->measurement.refs[i]));
    const char* axes[] = {"+X", "+Y", "+Z"};
    const char* planes[] = {"XY (+Z normal)", "YZ (+X normal)", "ZX (+Y normal)"};
    const Vec3 vectors[] = {{1,0,0}, {0,1,0}, {0,0,1}};
    snprintf(lines[5], sizeof(lines[5]), "P / click: projection axis %s (world)", axes[ui->measurement.projection_axis]);
    snprintf(lines[6], sizeof(lines[6]), "N / click: signed angle plane %s (world)", planes[ui->measurement.angle_plane]);
    snprintf(lines[7], sizeof(lines[7]), "K / click: pick the active reference in the viewport");
    const char* labels[] = {"Point distance", "Signed projection A -> B", "Direction angle [0,180]", "Signed planar angle (-180,180]", "Signed plane gap along A normal"};
    for (int i = 0; i < 5; ++i) {
        Vec3 vector = i == EDITOR_MEASURE_PLANAR_ANGLE ? vectors[(ui->measurement.angle_plane+2)%3]
            : vectors[ui->measurement.projection_axis];
        EditorMeasurementResult r = Editor_Measure(&Global_Get()->layout, &ui->measurement.refs[0],
            &ui->measurement.refs[1], (EditorMeasurementKind)i, vector);
        if (r.status == EDITOR_MEASUREMENT_OK) {
            double value = r.value;
            const bool angle = i == EDITOR_MEASURE_DIRECTION_ANGLE || i == EDITOR_MEASURE_PLANAR_ANGLE;
            if (!angle) (void)core_units_convert(r.value, CORE_UNIT_METER, UIPanel_GetDisplayUnit(), &value);
            snprintf(lines[8+i], sizeof(lines[8+i]), "%s: %.6g %s", labels[i], value, angle ? "deg" : UIPanel_GetDisplayUnitSymbol());
        } else snprintf(lines[8+i], sizeof(lines[8+i]), "%s: %s", labels[i],
            !ui->measurement.refs[0].entity_id[0] || !ui->measurement.refs[1].entity_id[0]
                ? "Create a plane or prism to measure" : r.message);
    }
    snprintf(lines[13], sizeof(lines[13]), "D / left: set projected distance   C / right: align points");
    snprintf(lines[14], sizeof(lines[14]), "%s", ui->measurement.placement_message[0]
        ? ui->measurement.placement_message : "Face points are centers. No persistent constraints or clearance checks.");
    snprintf(lines[15], sizeof(lines[15]), "Close (Enter / Esc / click)");
    if (ui->measurement.placing) {
        snprintf(lines[1], sizeof(lines[1]), "A fixed; B translates once. Enter/click target row: apply. Esc: cancel.");
        snprintf(lines[2], sizeof(lines[2]), "Reference selection is frozen while entering a placement.");
        if (ui->measurement.placing == 1)
            snprintf(lines[13], sizeof(lines[13]), "Signed A -> B target [%s]: %s_", UIPanel_GetDisplayUnitSymbol(), ui->measurement.placement_text);
        else snprintf(lines[13], sizeof(lines[13]), "Confirm: align B reference point to A in all three axes");
        snprintf(lines[15], sizeof(lines[15]), "Cancel placement (Esc / click)");
    }
    for (int i = 0; i < 16; ++i) {
        if (i == 3 || i == 4) {
            int slot = i - 3;
            SDL_Rect row = {p.x+8, p.y+12+i*h, p.w-16, h};
            SDL_SetRenderDrawColor(renderer, palette.pane_border.r, palette.pane_border.g, palette.pane_border.b, 255);
            SDL_RenderDrawRect(renderer, &row);
            SDL_RenderDrawLine(renderer, p.x+p.w/2, row.y, p.x+p.w/2, row.y+h);
            snprintf(lines[i], sizeof(lines[i]), "%c %c: %s",
                ui->measurement.slot == slot ? '>' : ' ', 'A'+slot,
                ui->measurement.refs[slot].entity_id[0] ? ui->measurement.refs[slot].entity_id : "No primitive");
            UIPanelSummary_DrawTextClipped(renderer, font, lines[i], p.x+12, row.y,
                p.w/2-24, h, palette.text_primary);
            UIPanelSummary_DrawTextClipped(renderer, font, feature_label(&ui->measurement.refs[slot]),
                p.x+p.w/2+12, row.y, p.w/2-24, h, palette.accent);
        } else {
            UIPanelSummary_DrawTextClipped(renderer, font, lines[i], p.x+12, p.y+12+i*h,
                p.w-24, h, i == 15 ? palette.accent : palette.text_primary);
        }
    }
}
