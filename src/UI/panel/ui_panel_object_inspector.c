#include "UI/ui_panel_object_inspector.h"

#include "Core/global_state.h"
#include "Layout/scene/layout_object_faces.h"
#include "ObjectAuthoring/object_authoring_document.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/ui_panel_object_layout.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

static int UIPanelObjectInspector_FontHeight(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) h = TTF_FontHeight(font);
    if (h < 12) h = 12;
    return h;
}

static int UIPanelObjectInspector_LineGap(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).section_gap;
}

static int UIPanelObjectInspector_PanelPad(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).pad_y;
}

enum {
    UI_OBJECT_INSPECTOR_SUMMARY_LINE_COUNT = 4,
    UI_OBJECT_INSPECTOR_DETAILS_LINE_COUNT = 12
};

static void UIPanelObjectInspector_DrawTextClipped(SDL_Renderer* renderer,
                                                   TTF_Font* font,
                                                   const char* text,
                                                   int x,
                                                   int y,
                                                   int max_width,
                                                   SDL_Color color) {
    UIPanelSummary_DrawTextClipped(renderer,
                                   font,
                                   text,
                                   x,
                                   y,
                                   max_width,
                                   UIPanelVisual_MakeMetrics(font).line_h,
                                   color);
}

static int UIPanelObjectInspector_DrawWrappedLine(SDL_Renderer* renderer,
                                                  TTF_Font* font,
                                                  const char* text,
                                                  SDL_Rect panel,
                                                  int y,
                                                  int font_h,
                                                  int line_gap,
                                                  SDL_Color color) {
    int max_lines = 0;
    int content_bottom = panel.y + panel.h - UIPanelVisual_MakeMetrics(font).pad_y;
    if (!renderer || !font || !text || !text[0]) return 0;
    max_lines = (content_bottom - y + line_gap) / (font_h + line_gap);
    if (max_lines <= 0) return 0;
    return UIPanelSummary_DrawWrappedText(renderer,
                                          font,
                                          text,
                                          panel.x + UIPanelVisual_MakeMetrics(font).pad_x,
                                          y,
                                          panel.w - (UIPanelVisual_MakeMetrics(font).pad_x * 2),
                                          font_h,
                                          line_gap,
                                          max_lines,
                                          color);
}

static const char* UIPanelObjectInspector_KindLabel(Object3DKind kind) {
    switch (kind) {
        case OBJECT3D_KIND_PLANE: return "Plane";
        case OBJECT3D_KIND_RECT_PRISM: return "Rect Prism";
        case OBJECT3D_KIND_MESH_ASSET_INSTANCE: return "Mesh Asset";
        case OBJECT3D_KIND_UNKNOWN:
        default: return "Unknown";
    }
}

static const char* UIPanelObjectInspector_BodyKindLabel(ObjectAuthoringBodyKind kind) {
    switch (kind) {
        case OBJECT_AUTHORING_BODY_KIND_PLANE_PRIMITIVE: return "Plane";
        case OBJECT_AUTHORING_BODY_KIND_RECT_PRISM_PRIMITIVE: return "Rect Prism";
        case OBJECT_AUTHORING_BODY_KIND_UNKNOWN:
        default: return "Body";
    }
}

static const char* UIPanelObjectInspector_OperationLabel(ObjectAuthoringOperationKind kind) {
    switch (kind) {
        case OBJECT_AUTHORING_OPERATION_CREATE_PRIMITIVE: return "Create Primitive";
        case OBJECT_AUTHORING_OPERATION_SKETCH_RECTANGLE: return "Sketch Rect";
        case OBJECT_AUTHORING_OPERATION_EXTRUDE_ADD: return "Extrude Add";
        case OBJECT_AUTHORING_OPERATION_EXTRUDE_CUT: return "Extrude Cut";
        case OBJECT_AUTHORING_OPERATION_NONE:
        default: return "Operation";
    }
}

static const char* UIPanelObjectInspector_CorePlaneLabel(CoreObjectPlane plane) {
    switch (plane) {
        case CORE_OBJECT_PLANE_YZ: return "YZ";
        case CORE_OBJECT_PLANE_XZ: return "XZ";
        case CORE_OBJECT_PLANE_XY:
        default: return "XY";
    }
}

static bool UIPanelObjectInspector_FaceDimensions(const Object3D* object,
                                                  Object3DFaceKind face,
                                                  float* out_u,
                                                  float* out_v,
                                                  float* out_depth) {
    if (out_u) *out_u = 0.0f;
    if (out_v) *out_v = 0.0f;
    if (out_depth) *out_depth = 0.0f;
    if (!object || face == OBJECT3D_FACE_NONE) return false;

    if (object->kind == OBJECT3D_KIND_PLANE && face == OBJECT3D_FACE_PLANE_SURFACE) {
        if (out_u) *out_u = object->plane.width;
        if (out_v) *out_v = object->plane.height;
        return true;
    }

    if (object->kind != OBJECT3D_KIND_RECT_PRISM ||
        !Layout_Object3DFaceKind_IsRectPrismFace(face)) {
        return false;
    }

    switch (face) {
        case OBJECT3D_FACE_RECT_PRISM_NEG_N:
        case OBJECT3D_FACE_RECT_PRISM_POS_N:
            if (out_u) *out_u = object->rectPrism.width;
            if (out_v) *out_v = object->rectPrism.height;
            if (out_depth) *out_depth = object->rectPrism.depth;
            return true;
        case OBJECT3D_FACE_RECT_PRISM_NEG_V:
        case OBJECT3D_FACE_RECT_PRISM_POS_V:
            if (out_u) *out_u = object->rectPrism.width;
            if (out_v) *out_v = object->rectPrism.depth;
            if (out_depth) *out_depth = object->rectPrism.height;
            return true;
        case OBJECT3D_FACE_RECT_PRISM_NEG_U:
        case OBJECT3D_FACE_RECT_PRISM_POS_U:
            if (out_u) *out_u = object->rectPrism.height;
            if (out_v) *out_v = object->rectPrism.depth;
            if (out_depth) *out_depth = object->rectPrism.width;
            return true;
        case OBJECT3D_FACE_NONE:
        case OBJECT3D_FACE_PLANE_SURFACE:
        default:
            return false;
    }
}

static size_t UIPanelObjectInspector_CountSketchesForFace(const ObjectAuthoringDocument* doc,
                                                          ObjectAuthoringFaceRef face_ref) {
    size_t count = 0u;
    if (!doc || !ObjectAuthoringFaceRef_IsSet(face_ref)) return 0u;
    for (size_t i = 0u; i < doc->sketchCount; ++i) {
        const ObjectAuthoringSketch* sketch = &doc->sketches[i];
        if (ObjectAuthoringFaceRef_Matches(sketch->faceRef, face_ref)) {
            count++;
        }
    }
    return count;
}

static size_t UIPanelObjectInspector_CountOperationsForFace(const ObjectAuthoringDocument* doc,
                                                            ObjectAuthoringFaceRef face_ref) {
    size_t count = 0u;
    if (!doc || !ObjectAuthoringFaceRef_IsSet(face_ref)) return 0u;
    for (size_t i = 0u; i < doc->operationCount; ++i) {
        const ObjectAuthoringOperation* operation = &doc->operations[i];
        if (ObjectAuthoringFaceRef_Matches(operation->faceRef, face_ref)) {
            count++;
        }
    }
    return count;
}

static const char* UIPanelObjectInspector_DimensionalModeLabel(CoreObjectDimensionalMode mode) {
    switch (mode) {
        case CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED: return "Plane locked";
        case CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D:
        default: return "Full 3D";
    }
}

static void UIPanelObjectInspector_FormatDimension(float world_value,
                                                   char* out,
                                                   size_t out_size) {
    double display = 0.0;
    const char* symbol = UIPanel_GetDisplayUnitSymbol();
    if (!out || out_size == 0) return;
    if (UIPanel_ConvertWorldToDisplay((double)world_value, &display)) {
        snprintf(out, out_size, "%.2f%s", display, symbol);
    } else {
        snprintf(out, out_size, "%.2f", world_value);
    }
}

int UIPanel_ObjectInspectorReservedHeight(const UIPanelState* ui) {
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_OBJECT || !Global_Get()) return 0;
    font_h = UIPanelObjectInspector_FontHeight();
    line_gap = UIPanelObjectInspector_LineGap();
    pad = UIPanelObjectInspector_PanelPad();
    return (pad * 2) +
           (font_h * UI_OBJECT_INSPECTOR_SUMMARY_LINE_COUNT) +
           (line_gap * (UI_OBJECT_INSPECTOR_SUMMARY_LINE_COUNT - 1));
}

int UIPanel_ObjectInspectorDetailsHeight(const UIPanelState* ui) {
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_OBJECT || !Global_Get()) return 0;
    font_h = UIPanelObjectInspector_FontHeight();
    line_gap = UIPanelObjectInspector_LineGap();
    pad = UIPanelObjectInspector_PanelPad();
    return (pad * 2) +
           (font_h * UI_OBJECT_INSPECTOR_DETAILS_LINE_COUNT) +
           (line_gap * (UI_OBJECT_INSPECTOR_DETAILS_LINE_COUNT - 1));
}

static void UIPanelObjectInspector_DrawSummaryCard(const UIPanelState* ui,
                                                   SDL_Renderer* renderer,
                                                   TTF_Font* font,
                                                   const Object3D* object,
                                                   SDL_Color label_color,
                                                   SDL_Color value_color,
                                                   SDL_Color accent_color,
                                                   SDL_Color fill_color,
                                                   SDL_Color border_color) {
    SDL_Rect panel = {0, 0, 0, 0};
    int font_h = UIPanelObjectInspector_FontHeight();
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    int line_gap = metrics.section_gap;
    int y = 0;
    char line_identity[160];
    char line_context[160];
    char line_selection[160];
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    const ObjectAuthoringDocument* doc =
        object_mode && Global_Get()->objectAuthoring.attached
            ? &Global_Get()->objectAuthoring.document
            : NULL;
    const ObjectAuthoringOperation* selected_op =
        doc ? ObjectAuthoringDocument_FindOperation(doc, doc->selectedOperationId) : NULL;
    const ObjectAuthoringSketch* selected_sketch =
        doc ? ObjectAuthoringDocument_FindSketch(doc, doc->selectedSketchId) : NULL;

    if (!UIPanel_GetObjectPaneRects(ui, &panel, NULL, NULL, NULL, NULL, NULL)) return;
    if (panel.h <= 0) return;

    UIPanelSummary_DrawCard(renderer, panel, fill_color, border_color, accent_color, metrics.accent_h);
    y = panel.y + metrics.pad_y;

    UIPanelSummary_DrawText(renderer,
                            font,
                            object_mode ? "Selected Entity" : "Selection",
                            panel.x + metrics.pad_x,
                            y,
                            label_color);
    y += font_h + line_gap;

    if (!object) {
        UIPanelObjectInspector_DrawTextClipped(renderer,
                                               font,
                                               "No object selected.",
                                               panel.x + metrics.pad_x,
                                               y,
                                               panel.w - (metrics.pad_x * 2),
                                               value_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer,
                                               font,
                                               object_mode
                                                   ? "Select a row or viewport face."
                                                   : "Select in Scene or click in the viewport.",
                                               panel.x + metrics.pad_x,
                                               y,
                                               panel.w - (metrics.pad_x * 2),
                                               label_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer,
                                               font,
                                               object_mode ? "Use Tools for commands." : "Use Create for new geometry.",
                                               panel.x + metrics.pad_x,
                                               y,
                                               panel.w - (metrics.pad_x * 2),
                                               label_color);
        return;
    }

    if (selected_op) {
        snprintf(line_identity,
                 sizeof(line_identity),
                 "Operation #%u  %s",
                 selected_op->operationId,
                 UIPanelObjectInspector_OperationLabel(selected_op->kind));
    } else if (selected_sketch) {
        snprintf(line_identity,
                 sizeof(line_identity),
                 "Sketch #%u  Body #%u",
                 selected_sketch->sketchId,
                 selected_sketch->faceRef.bodyId);
    } else {
        snprintf(line_identity,
                 sizeof(line_identity),
                 "#%u  %s",
                 object->objectId,
                 UIPanelObjectInspector_KindLabel(object->kind));
    }
    snprintf(line_context,
             sizeof(line_context),
             "%s   Plane %s",
             UIPanelObjectInspector_DimensionalModeLabel(object->coreMeta.dimensional_mode),
             UIPanelObjectInspector_CorePlaneLabel(object->coreMeta.locked_plane));
    if (doc && ObjectAuthoringFaceRef_IsSet(doc->selectedFace)) {
        snprintf(line_selection,
                 sizeof(line_selection),
                 "%s   Face %s   FaceID %u",
                 object->coreMeta.object_id,
                 Layout_Object3DFaceKind_Label(doc->selectedFace.primitiveFace),
                 doc->selectedFace.faceId);
    } else {
        snprintf(line_selection,
                 sizeof(line_selection),
                 "%s",
                 object->coreMeta.object_id);
    }

    UIPanelObjectInspector_DrawTextClipped(renderer, font, line_identity, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), accent_color);
    y += font_h + line_gap;
    UIPanelSummary_DrawDivider(renderer, panel, y - (line_gap / 2), metrics.pad_x, accent_color, 90);
    UIPanelObjectInspector_DrawTextClipped(renderer, font, line_context, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), value_color);
    y += font_h + line_gap;
    UIPanelObjectInspector_DrawTextClipped(renderer, font, line_selection, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), label_color);
}

static void UIPanelObjectInspector_DrawDetailsCard(const UIPanelState* ui,
                                                   SDL_Renderer* renderer,
                                                   TTF_Font* font,
                                                   const Object3D* object,
                                                   SDL_Color label_color,
                                                   SDL_Color value_color,
                                                   SDL_Color accent_color,
                                                   SDL_Color fill_color,
                                                   SDL_Color border_color) {
    SDL_Rect panel = {0, 0, 0, 0};
    int font_h = UIPanelObjectInspector_FontHeight();
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    int line_gap = metrics.section_gap;
    int y = 0;
    const GlobalState* global = Global_Get();
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    const ObjectAuthoringDocument* doc =
        object_mode && global && global->objectAuthoring.attached
            ? &global->objectAuthoring.document
            : NULL;
    const ObjectAuthoringOperation* selected_op =
        doc ? ObjectAuthoringDocument_FindOperation(doc, doc->selectedOperationId) : NULL;
    const ObjectAuthoringSketch* selected_sketch =
        doc ? ObjectAuthoringDocument_FindSketch(doc, doc->selectedSketchId) : NULL;
    const ObjectAuthoringBody* selected_body =
        doc && global ? ObjectAuthoringDocument_FindBody(doc, global->editor.selectedObjectAssetBodyId) : NULL;
    const ObjectAuthoringFaceRef selected_face =
        doc ? doc->selectedFace
            : ObjectAuthoringFaceRef_FromPrimitive(0u, OBJECT3D_FACE_NONE);
    const ObjectAuthoringFaceRefStatus selected_face_status =
        doc ? ObjectAuthoringDocument_CheckFaceRef(doc, selected_face)
            : OBJECT_AUTHORING_FACE_REF_STATUS_UNSET;

    if (!UIPanel_GetObjectPaneRects(ui, NULL, &panel, NULL, NULL, NULL, NULL)) return;
    if (panel.h <= 0) return;

    UIPanelSummary_DrawCard(renderer, panel, fill_color, border_color, accent_color, metrics.accent_h);
    y = panel.y + metrics.pad_y;

    UIPanelSummary_DrawText(renderer,
                            font,
                            "Properties",
                            panel.x + metrics.pad_x,
                            y,
                            label_color);
    y += font_h + line_gap;

    if (!object) {
        y += UIPanelObjectInspector_DrawWrappedLine(renderer,
                                                    font,
                                                    "Nothing to inspect yet.",
                                                    panel,
                                                    y,
                                                    font_h,
                                                    line_gap,
                                                    value_color) * (font_h + line_gap);
        y += UIPanelObjectInspector_DrawWrappedLine(renderer,
                                                    font,
                                                    object_mode
                                                        ? "Select a model item to see details."
                                                        : "The Scene tab owns the object list.",
                                                    panel,
                                                    y,
                                                    font_h,
                                                    line_gap,
                                                    label_color) * (font_h + line_gap);
        y += UIPanelObjectInspector_DrawWrappedLine(renderer,
                                                    font,
                                                    object_mode
                                                        ? "Dimension and transform edits are below."
                                                        : "Select from Scene or use Create, then this pane becomes the object editor.",
                                                    panel,
                                                    y,
                                                    font_h,
                                                    line_gap,
                                                    label_color) * (font_h + line_gap);
        return;
    }

    {
        char line_identity[160];
        char line_dims[160];
        char line_pos[160];
        char line_rot[160];
        char line_state[192];
        char line_editing[192];
        char line_authoring0[192] = {0};
        char line_authoring1[192] = {0};
        char line_face0[192] = {0};
        char line_face1[192] = {0};
        char line_face2[192] = {0};
        char w_text[32] = {0};
        char h_text[32] = {0};
        char d_text[32] = {0};
        bool lock_plane = false;
        bool lock_bounds = false;

        if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            lock_plane = object->rectPrism.lockToConstructionPlane;
            lock_bounds = object->rectPrism.lockToBounds;
            UIPanelObjectInspector_FormatDimension(object->rectPrism.width, w_text, sizeof(w_text));
            UIPanelObjectInspector_FormatDimension(object->rectPrism.height, h_text, sizeof(h_text));
            UIPanelObjectInspector_FormatDimension(object->rectPrism.depth, d_text, sizeof(d_text));
        } else if (object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE) {
            const Vec3 span = {
                (object->meshInstance.localBoundsMax.x - object->meshInstance.localBoundsMin.x) *
                    object->transform.scale.x,
                (object->meshInstance.localBoundsMax.y - object->meshInstance.localBoundsMin.y) *
                    object->transform.scale.y,
                (object->meshInstance.localBoundsMax.z - object->meshInstance.localBoundsMin.z) *
                    object->transform.scale.z
            };
            lock_plane = false;
            lock_bounds = object->meshInstance.lockToBounds;
            UIPanelObjectInspector_FormatDimension(span.x, w_text, sizeof(w_text));
            UIPanelObjectInspector_FormatDimension(span.y, h_text, sizeof(h_text));
            UIPanelObjectInspector_FormatDimension(span.z, d_text, sizeof(d_text));
        } else {
            lock_plane = object->plane.lockToConstructionPlane;
            lock_bounds = object->plane.lockToBounds;
            UIPanelObjectInspector_FormatDimension(object->plane.width, w_text, sizeof(w_text));
            UIPanelObjectInspector_FormatDimension(object->plane.height, h_text, sizeof(h_text));
            snprintf(d_text, sizeof(d_text), "n/a");
        }

        snprintf(line_identity,
                 sizeof(line_identity),
                 "Object  #%u   %s   Plane %s",
                 object->objectId,
                 UIPanelObjectInspector_KindLabel(object->kind),
                 UIPanelObjectInspector_CorePlaneLabel(object->coreMeta.locked_plane));
        snprintf(line_dims,
                 sizeof(line_dims),
                 "Dimensions  W %s   H %s   D %s",
                 w_text,
                 h_text,
                 d_text);
        snprintf(line_pos,
                 sizeof(line_pos),
                 "Position  %.2f, %.2f, %.2f",
                 object->transform.position.x,
                 object->transform.position.y,
                 object->transform.position.z);
        snprintf(line_rot,
                 sizeof(line_rot),
                 "Rotation  %.1f, %.1f, %.1f deg",
                 object->transform.rotationDeg.x,
                 object->transform.rotationDeg.y,
                 object->transform.rotationDeg.z);
        snprintf(line_state,
                 sizeof(line_state),
                 "State  Visible:%s  Locked:%s  Selectable:%s  Plane:%s  Bounds:%s",
                 object->coreMeta.flags.visible ? "On" : "Off",
                 object->coreMeta.flags.locked ? "On" : "Off",
                 object->coreMeta.flags.selectable ? "On" : "Off",
                 lock_plane ? "On" : "Off",
                 lock_bounds ? "On" : "Off");
        snprintf(line_editing,
                 sizeof(line_editing),
                 "Editing  %s   Gizmo:%s   Face:%s   Unit:%s",
                 UIPanelObjectInspector_DimensionalModeLabel(object->coreMeta.dimensional_mode),
                 global ? UIPanel_ObjectGizmoModeLabel() : "Move",
                 global
                     ? Layout_Object3DFaceKind_Label(global->editor.selectedObjectAssetFace)
                     : "None",
                 UIPanel_GetDisplayUnitSymbol());

        if (selected_face_status == OBJECT_AUTHORING_FACE_REF_STATUS_OK && global) {
            const Object3D* face_object =
                Layout_ObjectStore_FindConst(&global->layout.objectStore, selected_face.bodyId);
            PlaneFrame3 frame = {0};
            float face_u = 0.0f;
            float face_v = 0.0f;
            float face_depth = 0.0f;
            char face_u_text[32] = {0};
            char face_v_text[32] = {0};
            char face_depth_text[32] = {0};
            if (face_object &&
                Layout_Object3DFace_GetFrame(face_object, selected_face.primitiveFace, &frame)) {
                snprintf(line_face0,
                         sizeof(line_face0),
                         "FaceID  %u   Body #%u  %s",
                         selected_face.faceId,
                         selected_face.bodyId,
                         Layout_Object3DFaceKind_Label(selected_face.primitiveFace));
                snprintf(line_face1,
                         sizeof(line_face1),
                         "Normal  %.2f, %.2f, %.2f",
                         frame.normal.x,
                         frame.normal.y,
                         frame.normal.z);
                if (UIPanelObjectInspector_FaceDimensions(face_object,
                                                          selected_face.primitiveFace,
                                                          &face_u,
                                                          &face_v,
                                                          &face_depth)) {
                    UIPanelObjectInspector_FormatDimension(face_u, face_u_text, sizeof(face_u_text));
                    UIPanelObjectInspector_FormatDimension(face_v, face_v_text, sizeof(face_v_text));
                    UIPanelObjectInspector_FormatDimension(face_depth,
                                                           face_depth_text,
                                                           sizeof(face_depth_text));
                    snprintf(line_face2,
                             sizeof(line_face2),
                             "Size  U %s  V %s  Depth %s  Sketches %zu  Ops %zu",
                             face_u_text,
                             face_v_text,
                             face_depth_text,
                             UIPanelObjectInspector_CountSketchesForFace(doc, selected_face),
                             UIPanelObjectInspector_CountOperationsForFace(doc, selected_face));
                } else {
                    snprintf(line_face2,
                             sizeof(line_face2),
                             "Face details unavailable for this primitive");
                }
            }
        } else if (selected_face_status != OBJECT_AUTHORING_FACE_REF_STATUS_UNSET) {
            snprintf(line_face0,
                     sizeof(line_face0),
                     "Face Ref  %s",
                     ObjectAuthoringFaceRefStatus_Label(selected_face_status));
            snprintf(line_face1,
                     sizeof(line_face1),
                     "FaceID  %u   Body #%u  %s",
                     selected_face.faceId,
                     selected_face.bodyId,
                     Layout_Object3DFaceKind_Label(selected_face.primitiveFace));
        }

        if (selected_op) {
            char depth_text[32] = {0};
            ObjectAuthoringFaceRefStatus op_face_status =
                doc ? ObjectAuthoringDocument_CheckFaceRef(doc, selected_op->faceRef)
                    : OBJECT_AUTHORING_FACE_REF_STATUS_UNSET;
            UIPanelObjectInspector_FormatDimension(selected_op->depth, depth_text, sizeof(depth_text));
            snprintf(line_authoring0,
                     sizeof(line_authoring0),
                     "Operation  #%u  %s  Depth %s",
                     selected_op->operationId,
                     UIPanelObjectInspector_OperationLabel(selected_op->kind),
                     depth_text);
            snprintf(line_authoring1,
                     sizeof(line_authoring1),
                     "Target  FaceID %u  Body #%u  %s  Ref %s",
                     selected_op->faceRef.faceId,
                     selected_op->faceRef.bodyId,
                     Layout_Object3DFaceKind_Label(selected_op->faceRef.primitiveFace),
                     ObjectAuthoringFaceRefStatus_Label(op_face_status));
        } else if (selected_sketch) {
            ObjectAuthoringFaceRefStatus sketch_face_status =
                doc ? ObjectAuthoringDocument_CheckFaceRef(doc, selected_sketch->faceRef)
                    : OBJECT_AUTHORING_FACE_REF_STATUS_UNSET;
            snprintf(line_authoring0,
                     sizeof(line_authoring0),
                     "Sketch  #%u  FaceID %u  Body #%u  %s",
                     selected_sketch->sketchId,
                     selected_sketch->faceRef.faceId,
                     selected_sketch->faceRef.bodyId,
                     Layout_Object3DFaceKind_Label(selected_sketch->faceRef.primitiveFace));
            snprintf(line_authoring1,
                     sizeof(line_authoring1),
                     "Ref %s  UV  %.2f, %.2f -> %.2f, %.2f",
                     ObjectAuthoringFaceRefStatus_Label(sketch_face_status),
                     selected_sketch->minUV.x,
                     selected_sketch->minUV.y,
                     selected_sketch->maxUV.x,
                     selected_sketch->maxUV.y);
        } else if (selected_body) {
            snprintf(line_authoring0,
                     sizeof(line_authoring0),
                     "Body  #%u  %s",
                     selected_body->bodyId,
                     UIPanelObjectInspector_BodyKindLabel(selected_body->authoringKind));
            snprintf(line_authoring1,
                     sizeof(line_authoring1),
                     "Source object #%u",
                     selected_body->sourceObjectId);
        }

        if (line_authoring0[0]) {
            y += UIPanelObjectInspector_DrawWrappedLine(renderer, font, line_authoring0, panel, y, font_h, line_gap, accent_color) * (font_h + line_gap);
        }
        if (line_authoring1[0]) {
            y += UIPanelObjectInspector_DrawWrappedLine(renderer, font, line_authoring1, panel, y, font_h, line_gap, value_color) * (font_h + line_gap);
        }
        if (line_face0[0]) {
            y += UIPanelObjectInspector_DrawWrappedLine(renderer, font, line_face0, panel, y, font_h, line_gap, accent_color) * (font_h + line_gap);
        }
        if (line_face1[0]) {
            y += UIPanelObjectInspector_DrawWrappedLine(renderer, font, line_face1, panel, y, font_h, line_gap, value_color) * (font_h + line_gap);
        }
        if (line_face2[0]) {
            y += UIPanelObjectInspector_DrawWrappedLine(renderer, font, line_face2, panel, y, font_h, line_gap, label_color) * (font_h + line_gap);
        }
        y += UIPanelObjectInspector_DrawWrappedLine(renderer, font, line_identity, panel, y, font_h, line_gap, value_color) * (font_h + line_gap);
        y += UIPanelObjectInspector_DrawWrappedLine(renderer, font, line_pos, panel, y, font_h, line_gap, value_color) * (font_h + line_gap);
        y += UIPanelObjectInspector_DrawWrappedLine(renderer, font, line_rot, panel, y, font_h, line_gap, value_color) * (font_h + line_gap);
        y += UIPanelObjectInspector_DrawWrappedLine(renderer, font, line_dims, panel, y, font_h, line_gap, value_color) * (font_h + line_gap);
        y += UIPanelObjectInspector_DrawWrappedLine(renderer, font, line_state, panel, y, font_h, line_gap, label_color) * (font_h + line_gap);
        y += UIPanelObjectInspector_DrawWrappedLine(renderer, font, line_editing, panel, y, font_h, line_gap, label_color) * (font_h + line_gap);
    }
}

void Render_UIPanelObjectInspector(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    const Object3D* object = NULL;
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Color accent_color = {140, 170, 210, 255};
    SDL_Color fill_color = {20, 20, 24, 170};
    SDL_Color border_color = {90, 100, 115, 200};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    UIPanelVisualPalette palette = {0};

    if (!ui || !renderer || !state || !font) return;
    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_OBJECT) return;

    if (state->editor.selectedObject3DId != 0u) {
        object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                              state->editor.selectedObject3DId);
    }

    (void)UIPanelVisual_ResolvePalette(&palette);
    label_color = palette.text_muted;
    value_color = palette.text_primary;
    accent_color = palette.accent;
    fill_color = palette.pane_fill;
    fill_color.a = 170;
    border_color = palette.pane_border;
    border_color.a = 210;

    UIPanelObjectInspector_DrawSummaryCard(ui,
                                           renderer,
                                           font,
                                           object,
                                           label_color,
                                           value_color,
                                           accent_color,
                                           fill_color,
                                           border_color);
    fill_color = palette.workspace_fill;
    fill_color.a = palette.workspace_fill.a;
    UIPanelObjectInspector_DrawDetailsCard(ui,
                                           renderer,
                                           font,
                                           object,
                                           label_color,
                                           value_color,
                                           accent_color,
                                           fill_color,
                                           border_color);
}
