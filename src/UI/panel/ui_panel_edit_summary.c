#include "UI/ui_panel_edit_summary.h"

#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Layout/scene/layout_object_faces.h"
#include "Layout/scene/layout_mesh_runtime_preview.h"
#include "ObjectAuthoring/object_authoring_document.h"
#include "UI/font_manager.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>

enum {
    UI_EDIT_SUMMARY_LINES = 5,
    UI_EDIT_SUMMARY_RESERVED_LINES = 6,
    UI_EDIT_CARD_ACCENT_HEIGHT = 4
};

static int UIPanelEditSummary_FontHeight(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) h = TTF_FontHeight(font);
    if (h < 12) h = 12;
    return h;
}

static int UIPanelEditSummary_LineGap(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).section_gap;
}

static int UIPanelEditSummary_PanelPad(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).pad_y;
}

static const char* UIPanelEditSummary_TargetKindLabel(const ObjectAuthoringDocument* doc) {
    if (!doc) return "None";
    switch (doc->selectionKind) {
        case OBJECT_AUTHORING_SELECTION_EDGE: return "Edge";
        case OBJECT_AUTHORING_SELECTION_VERTEX: return "Vertex";
        case OBJECT_AUTHORING_SELECTION_FACE: return "Face";
        case OBJECT_AUTHORING_SELECTION_BODY: return "Body";
        case OBJECT_AUTHORING_SELECTION_NONE:
        default: return "None";
    }
    return "None";
}

static const char* UIPanelEditSummary_ObjectKindLabel(Object3DKind kind) {
    switch (kind) {
        case OBJECT3D_KIND_PLANE: return "Plane";
        case OBJECT3D_KIND_RECT_PRISM: return "Rect Prism";
        case OBJECT3D_KIND_MESH_ASSET_INSTANCE: return "Mesh Asset";
        case OBJECT3D_KIND_UNKNOWN:
        default: return "Unknown";
    }
}

static void UIPanelEditSummary_FormatDimension(float world_value,
                                               char* out,
                                               size_t out_size) {
    double display = 0.0;
    const char* symbol = UIPanel_GetDisplayUnitSymbol();
    if (!out || out_size == 0u) return;
    if (UIPanel_ConvertWorldToDisplay((double)world_value, &display)) {
        snprintf(out, out_size, "%.2f%s", display, symbol);
    } else {
        snprintf(out, out_size, "%.2f", world_value);
    }
}

static bool UIPanelEditSummary_BuildSceneObjectLines(const GlobalState* state,
                                                     char* mode_line,
                                                     size_t mode_line_size,
                                                     char* target_line,
                                                     size_t target_line_size,
                                                     char* authoring_line,
                                                     size_t authoring_line_size,
                                                     char* counts_line,
                                                     size_t counts_line_size,
                                                     char* gizmo_line,
                                                     size_t gizmo_line_size) {
    const Object3D* object = NULL;
    char w_text[32] = {0};
    char h_text[32] = {0};
    char d_text[32] = {0};
    Vec3 world_min = {0};
    Vec3 world_max = {0};
    if (!state || state->editor.selectedObject3DId == 0u) return false;
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                          state->editor.selectedObject3DId);
    if (!object) return false;

    if (Layout_Object3D_ComputeWorldAABB(object, &world_min, &world_max)) {
        UIPanelEditSummary_FormatDimension(fabsf(world_max.x - world_min.x),
                                           w_text,
                                           sizeof(w_text));
        UIPanelEditSummary_FormatDimension(fabsf(world_max.y - world_min.y),
                                           h_text,
                                           sizeof(h_text));
        UIPanelEditSummary_FormatDimension(fabsf(world_max.z - world_min.z),
                                           d_text,
                                           sizeof(d_text));
    } else {
        snprintf(w_text, sizeof(w_text), "n/a");
        snprintf(h_text, sizeof(h_text), "n/a");
        snprintf(d_text, sizeof(d_text), "n/a");
    }

    snprintf(mode_line,
             mode_line_size,
             "Mode  Scene object edit");
    snprintf(target_line,
             target_line_size,
             "Target  #%u   %s",
             object->objectId,
             UIPanelEditSummary_ObjectKindLabel(object->kind));
    snprintf(authoring_line,
             authoring_line_size,
             "Position  %.2f, %.2f, %.2f",
             object->transform.position.x,
             object->transform.position.y,
             object->transform.position.z);
    snprintf(counts_line,
             counts_line_size,
             "Size  W %s   H %s   D %s",
             w_text,
             h_text,
             d_text);
    if (object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE) {
        LayoutMeshRuntimePreviewStats preview_stats = {0};
        char preview_diag[96] = {0};
        if (Layout_MeshRuntimePreview_LoadStats(object->meshInstance.runtimePath,
                                                &preview_stats,
                                                preview_diag,
                                                sizeof(preview_diag))) {
            snprintf(mode_line,
                     mode_line_size,
                     "Mode  Mesh preview %s%s",
                     preview_stats.previewMode[0] ? preview_stats.previewMode : "unknown",
                     preview_stats.metadataOnly ? " metadata" : "");
            snprintf(gizmo_line,
                     gizmo_line_size,
                     "Mesh src %zuV/%zuT prev %zuE/%zuT S %.2f R %.2f",
                     preview_stats.sourceVertexCount,
                     preview_stats.sourceTriangleCount,
                     preview_stats.previewEdgeCount,
                     preview_stats.previewTriangleCount,
                     object->transform.scale.x,
                     preview_stats.boundingSphereRadius);
        } else {
            snprintf(mode_line,
                     mode_line_size,
                     "Mode  Mesh preview unavailable");
            snprintf(gizmo_line,
                     gizmo_line_size,
                     "Mesh  src %zuV/%zuT   preview n/a   Scale %.2f",
                     object->meshInstance.vertexCount,
                     object->meshInstance.triangleCount,
                     object->transform.scale.x);
        }
    } else {
        snprintf(gizmo_line,
                 gizmo_line_size,
                 "Gizmo  %s   Face:%s",
                 UIPanel_ObjectGizmoModeLabel(),
                 Layout_Object3DFaceKind_Label(state->editor.selectedObjectAssetFace));
    }
    return true;
}

static void UIPanelEditSummary_BuildTargetLine(const GlobalState* state,
                                               char* out,
                                               size_t out_size) {
    const ObjectAuthoringDocument* doc = NULL;
    if (!out || out_size == 0u) return;
    out[0] = '\0';
    if (!state || !state->objectAuthoring.attached) {
        snprintf(out, out_size, "Target  none");
        return;
    }

    doc = &state->objectAuthoring.document;
    if (doc->selectionKind == OBJECT_AUTHORING_SELECTION_EDGE &&
        doc->selectedEdge.edgeId != 0u) {
        snprintf(out,
                 out_size,
                 "Target  edge #%u   body #%u",
                 doc->selectedEdge.primitiveEdgeIndex,
                 doc->selectedEdge.bodyId);
    } else if (doc->selectionKind == OBJECT_AUTHORING_SELECTION_VERTEX &&
               doc->selectedVertex.vertexId != 0u) {
        snprintf(out,
                 out_size,
                 "Target  vertex #%u   body #%u",
                 doc->selectedVertex.primitiveVertexIndex,
                 doc->selectedVertex.bodyId);
    } else if (doc->selectionKind == OBJECT_AUTHORING_SELECTION_FACE &&
               ObjectAuthoringFaceRef_IsSet(doc->selectedFace)) {
        snprintf(out,
                 out_size,
                 "Target  face %s   body #%u",
                 Layout_Object3DFaceKind_Label(doc->selectedFace.primitiveFace),
                 doc->selectedFace.bodyId);
    } else if (doc->selectionKind == OBJECT_AUTHORING_SELECTION_BODY &&
               doc->selectedFace.bodyId != 0u) {
        snprintf(out, out_size, "Target  body #%u", doc->selectedFace.bodyId);
    } else {
        snprintf(out, out_size, "Target  none");
    }
}

int UIPanel_EditSummaryReservedHeight(const UIPanelState* ui) {
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_EDIT) return 0;
    font_h = UIPanelEditSummary_FontHeight();
    line_gap = UIPanelEditSummary_LineGap();
    pad = UIPanelEditSummary_PanelPad();
    return (pad * 2) + (font_h * UI_EDIT_SUMMARY_RESERVED_LINES) +
           (line_gap * (UI_EDIT_SUMMARY_RESERVED_LINES - 1));
}

void Render_UIPanelEditSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    UIPanelVisualPalette palette = {0};
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    SDL_Rect rect = {0, 0, 0, 0};
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Color accent_color = {140, 170, 210, 255};
    SDL_Color fill_color = {20, 20, 24, 170};
    SDL_Color border_color = {90, 100, 115, 210};
    char mode_line[128];
    char target_line[128];
    char authoring_line[128];
    char counts_line[128];
    char gizmo_line[128];
    const char* lines[UI_EDIT_SUMMARY_LINES];
    int font_h = UIPanelEditSummary_FontHeight();
    int y = 0;

    if (!ui || !renderer || !font || !state) return;
    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_EDIT) return;
    rect = ui->editPane.summaryRect;
    if (rect.w <= 0 || rect.h <= 0) return;

    (void)UIPanelVisual_ResolvePalette(&palette);
    label_color = palette.text_muted;
    value_color = palette.text_primary;
    accent_color = palette.accent;
    fill_color = palette.pane_fill;
    fill_color.a = 170;
    border_color = palette.pane_border;
    border_color.a = 210;

    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_SCENE ||
        !UIPanelEditSummary_BuildSceneObjectLines(state,
                                                  mode_line,
                                                  sizeof(mode_line),
                                                  target_line,
                                                  sizeof(target_line),
                                                  authoring_line,
                                                  sizeof(authoring_line),
                                                  counts_line,
                                                  sizeof(counts_line),
                                                  gizmo_line,
                                                  sizeof(gizmo_line))) {
        snprintf(mode_line,
                 sizeof(mode_line),
                 "Mode  %s",
                 Editor_ObjectEditSelectionModeLabel(state->editor.objectEditSelectionMode));
        UIPanelEditSummary_BuildTargetLine(state, target_line, sizeof(target_line));
        snprintf(authoring_line,
                 sizeof(authoring_line),
                 "Authoring  %s",
                 state->objectAuthoring.attached ? "attached" : "detached");
        snprintf(counts_line,
                 sizeof(counts_line),
                 "Topology  %zu vertices   %zu edges",
                 state->objectAuthoring.document.vertexCount,
                 state->objectAuthoring.document.edgeCount);
        snprintf(gizmo_line,
                 sizeof(gizmo_line),
                 "Gizmo  %s target",
                 UIPanelEditSummary_TargetKindLabel(&state->objectAuthoring.document));
    }

    lines[0] = mode_line;
    lines[1] = target_line;
    lines[2] = authoring_line;
    lines[3] = counts_line;
    lines[4] = gizmo_line;

    UIPanelSummary_DrawCard(renderer,
                            rect,
                            fill_color,
                            border_color,
                            accent_color,
                            UI_EDIT_CARD_ACCENT_HEIGHT);
    y = rect.y + metrics.pad_y;
    UIPanelSummary_DrawText(renderer,
                            font,
                            "Edit Selection",
                            rect.x + metrics.pad_x,
                            y,
                            value_color);
    y += font_h + metrics.section_gap;
    UIPanelSummary_DrawDivider(renderer,
                               rect,
                               y - (metrics.section_gap / 2),
                               metrics.pad_x,
                               palette.pane_divider,
                               90);

    for (int i = 0; i < UI_EDIT_SUMMARY_LINES; ++i) {
        if (y + font_h > rect.y + rect.h - metrics.pad_y) break;
        UIPanelSummary_DrawTextClipped(renderer,
                                       font,
                                       lines[i],
                                       rect.x + metrics.pad_x,
                                       y,
                                       rect.w - (metrics.pad_x * 2),
                                       font_h + 4,
                                       i == 0 ? value_color : label_color);
        y += font_h + metrics.section_gap;
    }
}
