#include "UI/ui_panel_object_workspace_summary.h"

#include "Core/global_state.h"
#include "Core/line_drawing_file_catalog.h"
#include "Core/workspace/line_drawing_object_workspace_view.h"
#include "Editor/editor.h"
#include "Editor/object_face_extrude.h"
#include "Editor/object_face_sketch.h"
#include "Layout/layout.h"
#include "Layout/scene/layout_object_faces.h"
#include "ObjectAuthoring/object_authoring_document.h"
#include "UI/font_manager.h"
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

enum {
    UI_OBJECT_MODEL_STATIC_BODY_ROWS = 3,
    UI_OBJECT_MODEL_STATIC_SKETCH_ROWS = 3,
    UI_OBJECT_MODEL_SCROLLBAR_W = 8,
    UI_OBJECT_MODEL_SCROLLBAR_MIN_THUMB_H = 24
};

static const char* UIPanelObjectWorkspace_BaseName(const char* path) {
    if (!path || !path[0]) return "(unset)";
    return LineDrawingFileCatalog_PathBasename(path);
}

static const char* UIPanelObjectWorkspace_BodyKindLabel(ObjectAuthoringBodyKind kind) {
    switch (kind) {
        case OBJECT_AUTHORING_BODY_KIND_PLANE_PRIMITIVE: return "Plane";
        case OBJECT_AUTHORING_BODY_KIND_RECT_PRISM_PRIMITIVE: return "Rect Prism";
        case OBJECT_AUTHORING_BODY_KIND_UNKNOWN:
        default: return "Body";
    }
}

static const char* UIPanelObjectWorkspace_OperationLabel(ObjectAuthoringOperationKind kind) {
    switch (kind) {
        case OBJECT_AUTHORING_OPERATION_CREATE_PRIMITIVE: return "Create Primitive";
        case OBJECT_AUTHORING_OPERATION_SKETCH_RECTANGLE: return "Sketch Rect";
        case OBJECT_AUTHORING_OPERATION_EXTRUDE_ADD: return "Extrude Add";
        case OBJECT_AUTHORING_OPERATION_EXTRUDE_CUT: return "Extrude Cut";
        case OBJECT_AUTHORING_OPERATION_NONE:
        default: return "Operation";
    }
}

static int UIPanelObjectWorkspace_FontHeight(TTF_Font* font) {
    int font_h = 14;
    if (font && TTF_FontHeight(font) > 12) font_h = TTF_FontHeight(font);
    if (font_h < 12) font_h = 12;
    return font_h;
}

static SDL_Rect UIPanelObjectWorkspace_RowRect(SDL_Rect detail,
                                               UIPanelVisualMetrics metrics,
                                               int y,
                                               int font_h) {
    return (SDL_Rect){
        detail.x + metrics.pad_x - 2,
        y - 2,
        detail.w - (metrics.pad_x * 2) + 4,
        font_h + metrics.section_gap
    };
}

static bool UIPanelObjectWorkspace_Contains(SDL_Rect rect, int x, int y) {
    return rect.w > 0 && rect.h > 0 &&
           x >= rect.x && x <= rect.x + rect.w &&
           y >= rect.y && y <= rect.y + rect.h;
}

static size_t UIPanelObjectWorkspace_StaticBodyCount(const ObjectAuthoringDocument* doc) {
    if (!doc) return 0u;
    return doc->bodyCount < UI_OBJECT_MODEL_STATIC_BODY_ROWS
               ? doc->bodyCount
               : UI_OBJECT_MODEL_STATIC_BODY_ROWS;
}

static size_t UIPanelObjectWorkspace_StaticSketchCount(const ObjectAuthoringDocument* doc) {
    if (!doc) return 0u;
    return doc->sketchCount < UI_OBJECT_MODEL_STATIC_SKETCH_ROWS
               ? doc->sketchCount
               : UI_OBJECT_MODEL_STATIC_SKETCH_ROWS;
}

static int UIPanelObjectWorkspace_OperationRowStep(UIPanelVisualMetrics metrics) {
    return (metrics.line_h * 2) + metrics.section_gap;
}

static int UIPanelObjectWorkspace_FirstStaticRowY(SDL_Rect detail,
                                                  UIPanelVisualMetrics metrics,
                                                  int font_h) {
    int y = detail.y + metrics.pad_y;
    y += font_h + metrics.section_gap;
    y += font_h + metrics.section_gap;
    y += font_h + metrics.section_gap;
    y += font_h + metrics.section_gap;
    return y;
}

static int UIPanelObjectWorkspace_OperationHeaderY(const ObjectAuthoringDocument* doc,
                                                   SDL_Rect detail,
                                                   UIPanelVisualMetrics metrics,
                                                   int font_h) {
    int y = UIPanelObjectWorkspace_FirstStaticRowY(detail, metrics, font_h);
    const size_t body_count = UIPanelObjectWorkspace_StaticBodyCount(doc);
    const size_t sketch_count = UIPanelObjectWorkspace_StaticSketchCount(doc);

    y += (int)body_count * (font_h + metrics.section_gap);
    if (sketch_count > 0u) {
        y += metrics.section_gap;
        y += font_h + metrics.section_gap;
        y += (int)sketch_count * (font_h + metrics.section_gap);
    }
    y += metrics.section_gap;
    return y;
}

static SDL_Rect UIPanelObjectWorkspace_OperationClipRect(const UIPanelState* ui,
                                                         const ObjectAuthoringDocument* doc,
                                                         UIPanelVisualMetrics metrics,
                                                         int font_h) {
    SDL_Rect detail = ui ? ui->objectWorkspacePane.browserRect : (SDL_Rect){0, 0, 0, 0};
    int header_y = UIPanelObjectWorkspace_OperationHeaderY(doc, detail, metrics, font_h);
    SDL_Rect clip = {
        detail.x + metrics.pad_x,
        header_y + font_h + metrics.section_gap,
        detail.w - (metrics.pad_x * 2),
        detail.y + detail.h - (header_y + font_h + metrics.section_gap) - metrics.pad_y
    };
    if (clip.h < 0) clip.h = 0;
    if (clip.w < 0) clip.w = 0;
    return clip;
}

static int UIPanelObjectWorkspace_OperationContentHeight(const ObjectAuthoringDocument* doc,
                                                         UIPanelVisualMetrics metrics) {
    if (!doc || doc->operationCount == 0u) return 0;
    return (int)doc->operationCount * UIPanelObjectWorkspace_OperationRowStep(metrics);
}

static float UIPanelObjectWorkspace_OperationMaxScroll(const UIPanelState* ui,
                                                       const ObjectAuthoringDocument* doc,
                                                       UIPanelVisualMetrics metrics,
                                                       int font_h) {
    SDL_Rect clip = UIPanelObjectWorkspace_OperationClipRect(ui, doc, metrics, font_h);
    const int content_h = UIPanelObjectWorkspace_OperationContentHeight(doc, metrics);
    if (content_h <= clip.h) return 0.0f;
    return (float)(content_h - clip.h);
}

static void UIPanelObjectWorkspace_ClampOperationScroll(UIPanelState* ui,
                                                        const ObjectAuthoringDocument* doc,
                                                        UIPanelVisualMetrics metrics,
                                                        int font_h) {
    float max_scroll = 0.0f;
    if (!ui) return;
    max_scroll = UIPanelObjectWorkspace_OperationMaxScroll(ui, doc, metrics, font_h);
    if (ui->objectModelTree.operationScrollOffsetPx < 0.0f) {
        ui->objectModelTree.operationScrollOffsetPx = 0.0f;
    }
    if (ui->objectModelTree.operationScrollOffsetPx > max_scroll) {
        ui->objectModelTree.operationScrollOffsetPx = max_scroll;
    }
}

static float UIPanelObjectWorkspace_ClampedOperationScroll(const UIPanelState* ui,
                                                           const ObjectAuthoringDocument* doc,
                                                           UIPanelVisualMetrics metrics,
                                                           int font_h) {
    float max_scroll = UIPanelObjectWorkspace_OperationMaxScroll(ui, doc, metrics, font_h);
    float offset = ui ? ui->objectModelTree.operationScrollOffsetPx : 0.0f;
    if (offset < 0.0f) offset = 0.0f;
    if (offset > max_scroll) offset = max_scroll;
    return offset;
}

static bool UIPanelObjectWorkspace_HasOperationScrollbar(const UIPanelState* ui,
                                                        const ObjectAuthoringDocument* doc,
                                                        UIPanelVisualMetrics metrics,
                                                        int font_h) {
    return UIPanelObjectWorkspace_OperationMaxScroll(ui, doc, metrics, font_h) > 0.0f;
}

static SDL_Rect UIPanelObjectWorkspace_OperationTrackRect(const UIPanelState* ui,
                                                         const ObjectAuthoringDocument* doc,
                                                         UIPanelVisualMetrics metrics,
                                                         int font_h) {
    SDL_Rect clip = UIPanelObjectWorkspace_OperationClipRect(ui, doc, metrics, font_h);
    return (SDL_Rect){
        clip.x + clip.w - UI_OBJECT_MODEL_SCROLLBAR_W,
        clip.y,
        UI_OBJECT_MODEL_SCROLLBAR_W,
        clip.h
    };
}

static SDL_Rect UIPanelObjectWorkspace_OperationContentClipRect(const UIPanelState* ui,
                                                               const ObjectAuthoringDocument* doc,
                                                               UIPanelVisualMetrics metrics,
                                                               int font_h) {
    SDL_Rect clip = UIPanelObjectWorkspace_OperationClipRect(ui, doc, metrics, font_h);
    if (UIPanelObjectWorkspace_HasOperationScrollbar(ui, doc, metrics, font_h)) {
        clip.w -= UI_OBJECT_MODEL_SCROLLBAR_W + 4;
        if (clip.w < 0) clip.w = 0;
    }
    return clip;
}

static SDL_Rect UIPanelObjectWorkspace_OperationThumbRect(const UIPanelState* ui,
                                                         const ObjectAuthoringDocument* doc,
                                                         UIPanelVisualMetrics metrics,
                                                         int font_h) {
    SDL_Rect track = UIPanelObjectWorkspace_OperationTrackRect(ui, doc, metrics, font_h);
    SDL_Rect thumb = track;
    const int content_h = UIPanelObjectWorkspace_OperationContentHeight(doc, metrics);
    float max_scroll = 0.0f;
    float travel = 0.0f;
    float ratio = 0.0f;
    if (!UIPanelObjectWorkspace_HasOperationScrollbar(ui, doc, metrics, font_h) ||
        track.h <= 0 || content_h <= 0) {
        return (SDL_Rect){0, 0, 0, 0};
    }
    ratio = (float)track.h / (float)content_h;
    if (ratio > 1.0f) ratio = 1.0f;
    thumb.h = (int)(ratio * (float)track.h);
    if (thumb.h < UI_OBJECT_MODEL_SCROLLBAR_MIN_THUMB_H) {
        thumb.h = UI_OBJECT_MODEL_SCROLLBAR_MIN_THUMB_H;
    }
    if (thumb.h > track.h) thumb.h = track.h;
    max_scroll = UIPanelObjectWorkspace_OperationMaxScroll(ui, doc, metrics, font_h);
    travel = (float)(track.h - thumb.h);
    if (travel > 0.0f && max_scroll > 0.0f) {
        ratio = UIPanelObjectWorkspace_ClampedOperationScroll(ui, doc, metrics, font_h) /
                max_scroll;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        thumb.y = track.y + (int)(ratio * travel);
    }
    return thumb;
}

static int UIPanelObjectWorkspace_OperationIndexAtPoint(const UIPanelState* ui,
                                                        const ObjectAuthoringDocument* doc,
                                                        int mouse_x,
                                                        int mouse_y,
                                                        UIPanelVisualMetrics metrics,
                                                        int font_h) {
    SDL_Rect clip = UIPanelObjectWorkspace_OperationContentClipRect(ui, doc, metrics, font_h);
    float content_y = 0.0f;
    int index = -1;
    if (!doc || doc->operationCount == 0u) return -1;
    if (!UIPanelObjectWorkspace_Contains(clip, mouse_x, mouse_y)) return -1;
    content_y = (float)(mouse_y - clip.y) +
                UIPanelObjectWorkspace_ClampedOperationScroll(ui, doc, metrics, font_h);
    index = (int)(content_y / (float)UIPanelObjectWorkspace_OperationRowStep(metrics));
    if (index < 0 || (size_t)index >= doc->operationCount) return -1;
    return index;
}

static bool UIPanelObjectWorkspace_JumpOperationScrollbarTo(UIPanelState* ui,
                                                           const ObjectAuthoringDocument* doc,
                                                           int mouse_y,
                                                           UIPanelVisualMetrics metrics,
                                                           int font_h) {
    SDL_Rect track = UIPanelObjectWorkspace_OperationTrackRect(ui, doc, metrics, font_h);
    SDL_Rect thumb = UIPanelObjectWorkspace_OperationThumbRect(ui, doc, metrics, font_h);
    float max_scroll = 0.0f;
    float usable_h = 0.0f;
    float ratio = 0.0f;
    int rel_y = 0;
    if (!ui || !doc || thumb.h <= 0) return false;
    max_scroll = UIPanelObjectWorkspace_OperationMaxScroll(ui, doc, metrics, font_h);
    usable_h = (float)(track.h - thumb.h);
    if (usable_h <= 0.0f || max_scroll <= 0.0f) return false;
    rel_y = mouse_y - track.y - (thumb.h / 2);
    ratio = (float)rel_y / usable_h;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    ui->objectModelTree.operationScrollOffsetPx = ratio * max_scroll;
    UIPanelObjectWorkspace_ClampOperationScroll(ui, doc, metrics, font_h);
    return true;
}

static bool UIPanelObjectWorkspace_DragOperationScrollbarTo(UIPanelState* ui,
                                                           const ObjectAuthoringDocument* doc,
                                                           int mouse_y,
                                                           UIPanelVisualMetrics metrics,
                                                           int font_h) {
    SDL_Rect track = UIPanelObjectWorkspace_OperationTrackRect(ui, doc, metrics, font_h);
    SDL_Rect thumb = UIPanelObjectWorkspace_OperationThumbRect(ui, doc, metrics, font_h);
    float max_scroll = 0.0f;
    float usable_h = 0.0f;
    float delta = 0.0f;
    if (!ui || !doc || thumb.h <= 0) return false;
    max_scroll = UIPanelObjectWorkspace_OperationMaxScroll(ui, doc, metrics, font_h);
    usable_h = (float)(track.h - thumb.h);
    if (usable_h <= 0.0f || max_scroll <= 0.0f) return false;
    delta = (float)(mouse_y - ui->objectModelTree.operationScrollbarDragStartY);
    ui->objectModelTree.operationScrollOffsetPx =
        ui->objectModelTree.operationScrollbarDragStartOffsetPx +
        ((delta / usable_h) * max_scroll);
    UIPanelObjectWorkspace_ClampOperationScroll(ui, doc, metrics, font_h);
    return true;
}

static void UIPanelObjectWorkspace_DrawSelectedRow(SDL_Renderer* renderer,
                                                   SDL_Rect rect,
                                                   SDL_Color accent) {
    SDL_Color fill = accent;
    SDL_Rect band = rect;
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;
    fill.a = 36;
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);
    band.w = 3;
    SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b, 180);
    SDL_RenderFillRect(renderer, &band);
}

static void UIPanelObjectWorkspace_SelectBody(UIPanelState* ui,
                                              GlobalState* state,
                                              ObjectAuthoringBodyId body_id) {
    if (!ui || !LineDrawingObjectWorkspaceView_SelectBody(state, body_id)) return;
    UIPanel_CloseFileBrowser(ui);
}

static void UIPanelObjectWorkspace_SelectSketch(UIPanelState* ui,
                                                GlobalState* state,
                                                const ObjectAuthoringSketch* sketch) {
    ObjectAuthoringDocument* doc = NULL;
    EditorState* editor = NULL;
    if (!ui || !state || !sketch || !state->objectAuthoring.attached) return;
    doc = &state->objectAuthoring.document;
    editor = &state->editor;

    Editor_ObjectFaceExtrudeClear(editor);
    doc->selectedFace = sketch->faceRef;
    doc->selectedSketchId = sketch->sketchId;
    doc->selectedOperationId = sketch->operationId;
    editor->selectedObject3DId = sketch->faceRef.bodyId;
    editor->selectedObjectAssetBodyId = sketch->faceRef.bodyId;
    editor->selectedObjectAssetFace = sketch->faceRef.primitiveFace;
    if (Editor_ObjectFaceSketchSyncFromAuthoring(state)) {
        (void)Editor_ObjectFaceSketchSelect(editor, OBJECT_FACE_SKETCH_HANDLE_BODY);
    } else {
        editor->objectAuthoringMode = Editor_ObjectAuthoringIdleMode(editor);
    }
    UIPanel_CloseFileBrowser(ui);
    Global_FlagHitboxesDirty();
}

static ObjectAuthoringBodyId UIPanelObjectWorkspace_OperationBodyId(
    const ObjectAuthoringOperation* operation) {
    if (!operation) return 0u;
    if (operation->faceRef.bodyId != 0u) return operation->faceRef.bodyId;
    if (operation->bodySnapshot.bodyId != 0u) return operation->bodySnapshot.bodyId;
    if (operation->resultBodyCount > 0u) return operation->resultBodyIds[0];
    return 0u;
}

static void UIPanelObjectWorkspace_SelectOperation(UIPanelState* ui,
                                                   GlobalState* state,
                                                   const ObjectAuthoringOperation* operation) {
    ObjectAuthoringDocument* doc = NULL;
    EditorState* editor = NULL;
    ObjectAuthoringBodyId body_id = 0u;
    Object3DFaceKind face = OBJECT3D_FACE_NONE;
    const ObjectAuthoringSketch* sketch = NULL;
    if (!ui || !state || !operation || !state->objectAuthoring.attached) return;
    doc = &state->objectAuthoring.document;
    editor = &state->editor;
    body_id = UIPanelObjectWorkspace_OperationBodyId(operation);
    face = operation->faceRef.primitiveFace;

    Editor_ObjectFaceExtrudeClear(editor);
    doc->selectedOperationId = operation->operationId;
    (void)ObjectAuthoringDocument_SetSelection(doc, body_id, face);
    editor->selectedObject3DId = body_id;
    editor->selectedObjectAssetBodyId = body_id;
    editor->selectedObjectAssetFace = face;

    sketch = ObjectAuthoringDocument_FindSketch(doc, operation->sketchId);
    if (sketch && sketch->active) {
        doc->selectedSketchId = sketch->sketchId;
        doc->selectedFace = sketch->faceRef;
        editor->selectedObject3DId = sketch->faceRef.bodyId;
        editor->selectedObjectAssetBodyId = sketch->faceRef.bodyId;
        editor->selectedObjectAssetFace = sketch->faceRef.primitiveFace;
        if (Editor_ObjectFaceSketchSyncFromAuthoring(state)) {
            (void)Editor_ObjectFaceSketchSelect(editor, OBJECT_FACE_SKETCH_HANDLE_BODY);
        }
    } else {
        doc->selectedSketchId = 0u;
        Editor_ObjectFaceSketchDeselect(editor);
        editor->objectAuthoringMode =
            face != OBJECT3D_FACE_NONE
                ? OBJECT_AUTHORING_MODE_FACE_SELECT
                : Editor_ObjectAuthoringIdleMode(editor);
    }
    UIPanel_CloseFileBrowser(ui);
    Global_FlagHitboxesDirty();
}

bool UIPanel_ObjectWorkspaceHandleModelTreeClick(UIPanelState* ui,
                                                GlobalState* state,
                                                int mouse_x,
                                                int mouse_y) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    SDL_Rect detail = {0, 0, 0, 0};
    int font_h = UIPanelObjectWorkspace_FontHeight(font);

    if (!ui || !state) return false;
    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return false;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return false;
    if (!state->objectAuthoring.attached) return false;

    detail = ui->objectWorkspacePane.browserRect;
    if (!UIPanelObjectWorkspace_Contains(detail, mouse_x, mouse_y)) return false;

    {
        ObjectAuthoringDocument* doc = &state->objectAuthoring.document;
        const size_t max_bodies = UIPanelObjectWorkspace_StaticBodyCount(doc);
        const size_t max_sketches = UIPanelObjectWorkspace_StaticSketchCount(doc);
        int y = UIPanelObjectWorkspace_FirstStaticRowY(detail, metrics, font_h);
        const bool has_scrollbar =
            UIPanelObjectWorkspace_HasOperationScrollbar(ui, doc, metrics, font_h);

        for (size_t i = 0u; i < max_bodies && y + font_h < detail.y + detail.h; ++i) {
            const ObjectAuthoringBody* body = &doc->bodies[i];
            if (UIPanelObjectWorkspace_Contains(
                    UIPanelObjectWorkspace_RowRect(detail, metrics, y, font_h),
                    mouse_x,
                    mouse_y)) {
                UIPanelObjectWorkspace_SelectBody(ui, state, body->bodyId);
                return true;
            }
            y += font_h + metrics.section_gap;
        }

        if (max_sketches > 0u && y + font_h < detail.y + detail.h) {
            y += metrics.section_gap;
            y += font_h + metrics.section_gap;
        }
        for (size_t i = 0u; i < max_sketches && y + font_h < detail.y + detail.h; ++i) {
            const ObjectAuthoringSketch* sketch = &doc->sketches[i];
            if (UIPanelObjectWorkspace_Contains(
                    UIPanelObjectWorkspace_RowRect(detail, metrics, y, font_h),
                    mouse_x,
                    mouse_y)) {
                UIPanelObjectWorkspace_SelectSketch(ui, state, sketch);
                return true;
            }
            y += font_h + metrics.section_gap;
        }

        if (has_scrollbar) {
            SDL_Rect track = UIPanelObjectWorkspace_OperationTrackRect(ui, doc, metrics, font_h);
            SDL_Rect thumb = UIPanelObjectWorkspace_OperationThumbRect(ui, doc, metrics, font_h);
            if (UIPanelObjectWorkspace_Contains(track, mouse_x, mouse_y)) {
                if (UIPanelObjectWorkspace_Contains(thumb, mouse_x, mouse_y)) {
                    ui->objectModelTree.operationScrollbarDragging = true;
                    ui->objectModelTree.operationScrollbarDragStartY = mouse_y;
                    ui->objectModelTree.operationScrollbarDragStartOffsetPx =
                        ui->objectModelTree.operationScrollOffsetPx;
                } else {
                    (void)UIPanelObjectWorkspace_JumpOperationScrollbarTo(
                        ui,
                        doc,
                        mouse_y,
                        metrics,
                        font_h);
                }
                return true;
            }
        }

        {
            int operation_index = UIPanelObjectWorkspace_OperationIndexAtPoint(
                ui,
                doc,
                mouse_x,
                mouse_y,
                metrics,
                font_h);
            if (operation_index >= 0) {
                UIPanelObjectWorkspace_SelectOperation(ui,
                                                       state,
                                                       &doc->operations[operation_index]);
                return true;
            }
        }
    }

    return UIPanelObjectWorkspace_Contains(detail, mouse_x, mouse_y);
}

bool UIPanel_ObjectWorkspaceHandleModelTreeWheel(int mouse_x,
                                                int mouse_y,
                                                float wheel_delta) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    int font_h = UIPanelObjectWorkspace_FontHeight(font);
    ObjectAuthoringDocument* doc = NULL;
    SDL_Rect clip = {0, 0, 0, 0};
    SDL_Rect track = {0, 0, 0, 0};

    if (!ui || !state || !state->objectAuthoring.attached) return false;
    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return false;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return false;
    doc = &state->objectAuthoring.document;
    clip = UIPanelObjectWorkspace_OperationClipRect(ui, doc, metrics, font_h);
    track = UIPanelObjectWorkspace_OperationTrackRect(ui, doc, metrics, font_h);
    if (!UIPanelObjectWorkspace_Contains(clip, mouse_x, mouse_y) &&
        !UIPanelObjectWorkspace_Contains(track, mouse_x, mouse_y)) {
        return false;
    }
    ui->objectModelTree.operationScrollOffsetPx -=
        wheel_delta * (float)(UIPanelObjectWorkspace_OperationRowStep(metrics) * 2);
    UIPanelObjectWorkspace_ClampOperationScroll(ui, doc, metrics, font_h);
    return true;
}

void UIPanel_ObjectWorkspaceHandleModelTreeMouseUp(void) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui) return;
    ui->objectModelTree.operationScrollbarDragging = false;
}

void UIPanel_ObjectWorkspaceHandleModelTreeMouseMotion(int mouse_x, int mouse_y) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    int font_h = UIPanelObjectWorkspace_FontHeight(font);
    ObjectAuthoringDocument* doc = NULL;
    if (!ui) return;
    ui->objectModelTree.hoverOperationIndex = -1;
    if (!state || !state->objectAuthoring.attached) return;
    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return;
    doc = &state->objectAuthoring.document;
    if (ui->objectModelTree.operationScrollbarDragging) {
        (void)UIPanelObjectWorkspace_DragOperationScrollbarTo(
            ui,
            doc,
            mouse_y,
            metrics,
            font_h);
    }
    ui->objectModelTree.hoverOperationIndex =
        UIPanelObjectWorkspace_OperationIndexAtPoint(ui, doc, mouse_x, mouse_y, metrics, font_h);
}

void Render_UIPanelObjectWorkspaceSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    UIPanelVisualPalette palette = {0};
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Color accent_color = {140, 170, 210, 255};
    SDL_Color fill_color = {20, 20, 24, 170};
    SDL_Color border_color = {90, 100, 115, 210};
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect detail = {0, 0, 0, 0};
    int y = 0;
    const int font_h = UIPanelObjectWorkspace_FontHeight(font);
    char line_asset[192];
    char line_status[128];
    char line_contents[160];
    char line_counts[160];
    char line_selection[160];
    char line_compile[192];

    if (!ui || !renderer || !font || !state) return;
    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return;

    panel = ui->objectWorkspacePane.summaryRect;
    detail = ui->objectWorkspacePane.browserRect;
    if (panel.w <= 0 || panel.h <= 0) return;

    (void)UIPanelVisual_ResolvePalette(&palette);
    label_color = palette.text_muted;
    value_color = palette.text_primary;
    accent_color = palette.accent;
    fill_color = palette.pane_fill;
    fill_color.a = 170;
    border_color = palette.pane_border;
    border_color.a = 210;

    snprintf(line_asset,
             sizeof(line_asset),
             "Asset  %s",
             UIPanelObjectWorkspace_BaseName(Global_GetCurrentObjectAssetPath()));
    snprintf(line_status,
             sizeof(line_status),
             "Save State  %s",
             state->layoutDirtySinceSave ? "Modified" : "Clean");
    snprintf(line_contents,
             sizeof(line_contents),
             "%zu bodies   %zu verts   %zu edges   %zu faces",
             state->objectAuthoring.attached ? state->objectAuthoring.document.bodyCount : 0u,
             state->objectAuthoring.attached ? state->objectAuthoring.document.vertexCount : 0u,
             state->objectAuthoring.attached ? state->objectAuthoring.document.edgeCount : 0u,
             state->objectAuthoring.attached ? state->objectAuthoring.document.faceCount : 0u);
    snprintf(line_counts,
             sizeof(line_counts),
             "Bodies  %zu visible",
             Layout_ObjectStore_LiveCount(&state->layout.objectStore));
    if (state->objectAuthoring.attached &&
        ObjectAuthoringFaceRef_IsSet(state->objectAuthoring.document.selectedFace)) {
        const ObjectAuthoringFaceRef selected_face =
            state->objectAuthoring.document.selectedFace;
        snprintf(line_selection,
                 sizeof(line_selection),
                 "Selection  FaceID %u   Body #%u   %s",
                 selected_face.faceId,
                 selected_face.bodyId,
                 Layout_Object3DFaceKind_Label(selected_face.primitiveFace));
    } else if (state->editor.selectedObjectAssetBodyId != 0u) {
        snprintf(line_selection,
                 sizeof(line_selection),
                 "Selection  Body #%u   Face %s",
                 state->editor.selectedObjectAssetBodyId,
                 Layout_Object3DFaceKind_Label(state->editor.selectedObjectAssetFace));
    } else if (state->editor.selectedObject3DId != 0u) {
        snprintf(line_selection,
                 sizeof(line_selection),
                 "Selection  Object #%u",
                 state->editor.selectedObject3DId);
    } else {
        snprintf(line_selection, sizeof(line_selection), "Selection  None");
    }
    snprintf(line_compile,
             sizeof(line_compile),
             "Compile  %s",
             Global_GetObjectRuntimeMeshStatus() && Global_GetObjectRuntimeMeshStatus()[0]
                 ? Global_GetObjectRuntimeMeshStatus()
                 : "Export Mesh to build runtime asset");

    UIPanelSummary_DrawCard(renderer, panel, fill_color, border_color, accent_color, metrics.accent_h);
    y = panel.y + metrics.pad_y;
    UIPanelSummary_DrawText(renderer, font, "Model Actions", panel.x + metrics.pad_x, y, label_color);
    y += font_h + metrics.section_gap;
    UIPanelSummary_DrawTextClipped(renderer, font, line_asset, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, accent_color);
    y += font_h + metrics.section_gap;
    UIPanelSummary_DrawDivider(renderer, panel, y - (metrics.section_gap / 2), metrics.pad_x, accent_color, 90);
    UIPanelSummary_DrawTextClipped(renderer, font, line_contents, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, value_color);
    y += font_h + metrics.section_gap;
    UIPanelSummary_DrawTextClipped(renderer, font, line_status, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, value_color);
    y += font_h + metrics.section_gap;
    UIPanelSummary_DrawTextClipped(renderer, font, line_compile, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, value_color);

    if (detail.w <= 0 || detail.h <= 0) return;

    UIPanelSummary_DrawCard(renderer, detail, fill_color, border_color, accent_color, metrics.accent_h);
    y = detail.y + metrics.pad_y;
    UIPanelSummary_DrawText(renderer, font, "Select / Navigate", detail.x + metrics.pad_x, y, label_color);
    y += font_h + metrics.section_gap;
    UIPanelSummary_DrawTextClipped(renderer, font, line_counts, detail.x + metrics.pad_x, y, detail.w - (metrics.pad_x * 2), font_h + 4, accent_color);
    y += font_h + metrics.section_gap;
    UIPanelSummary_DrawDivider(renderer, detail, y - (metrics.section_gap / 2), metrics.pad_x, accent_color, 90);
    UIPanelSummary_DrawTextClipped(renderer, font, line_selection, detail.x + metrics.pad_x, y, detail.w - (metrics.pad_x * 2), font_h + 4, value_color);
    y += font_h + metrics.section_gap;
    if (state->objectAuthoring.attached) {
        const ObjectAuthoringDocument* doc = &state->objectAuthoring.document;
        const size_t max_bodies = UIPanelObjectWorkspace_StaticBodyCount(doc);
        const size_t max_sketches = UIPanelObjectWorkspace_StaticSketchCount(doc);
        UIPanelSummary_DrawText(renderer, font, "Bodies  click to target", detail.x + metrics.pad_x, y, label_color);
        y += font_h + metrics.section_gap;
        for (size_t i = 0u; i < max_bodies && y + font_h < detail.y + detail.h; ++i) {
            char line[160];
            const ObjectAuthoringBody* body = &doc->bodies[i];
            const bool selected =
                doc->selectedSketchId == 0u &&
                doc->selectedOperationId == 0u &&
                state->editor.selectedObjectAssetBodyId == body->bodyId;
            snprintf(line,
                     sizeof(line),
                     "#%u  %s",
                     body->bodyId,
                     UIPanelObjectWorkspace_BodyKindLabel(body->authoringKind));
            if (selected) {
                UIPanelObjectWorkspace_DrawSelectedRow(
                    renderer,
                    UIPanelObjectWorkspace_RowRect(detail, metrics, y, font_h),
                    accent_color);
            }
            UIPanelSummary_DrawTextClipped(renderer,
                                           font,
                                           line,
                                           detail.x + metrics.pad_x,
                                           y,
                                           detail.w - (metrics.pad_x * 2),
                                           font_h + 4,
                                           selected ? accent_color : value_color);
            y += font_h + metrics.section_gap;
        }
        if (max_sketches > 0u && y + font_h < detail.y + detail.h) {
            y += metrics.section_gap;
            UIPanelSummary_DrawText(renderer, font, "Sketches  click to edit", detail.x + metrics.pad_x, y, label_color);
            y += font_h + metrics.section_gap;
        }
        for (size_t i = 0u; i < max_sketches && y + font_h < detail.y + detail.h; ++i) {
            char line[192];
            const ObjectAuthoringSketch* sketch = &doc->sketches[i];
            const bool selected = doc->selectedSketchId == sketch->sketchId;
            const ObjectAuthoringFaceRefStatus face_status =
                ObjectAuthoringDocument_CheckFaceRef(doc, sketch->faceRef);
            snprintf(line,
                     sizeof(line),
                     "%u  FaceID %u  %s  Ref %s",
                     sketch->sketchId,
                     sketch->faceRef.faceId,
                     Layout_Object3DFaceKind_Label(sketch->faceRef.primitiveFace),
                     ObjectAuthoringFaceRefStatus_Label(face_status));
            if (selected) {
                UIPanelObjectWorkspace_DrawSelectedRow(
                    renderer,
                    UIPanelObjectWorkspace_RowRect(detail, metrics, y, font_h),
                    accent_color);
            }
            UIPanelSummary_DrawTextClipped(renderer,
                                           font,
                                           line,
                                           detail.x + metrics.pad_x,
                                           y,
                                           detail.w - (metrics.pad_x * 2),
                                           font_h + 4,
                                           selected ? accent_color : value_color);
            y += font_h + metrics.section_gap;
        }
        if (y + font_h < detail.y + detail.h) {
            char title[96];
            y += metrics.section_gap;
            snprintf(title, sizeof(title), "Operations  %zu", doc->operationCount);
            UIPanelSummary_DrawText(renderer, font, title, detail.x + metrics.pad_x, y, label_color);
            y += font_h + metrics.section_gap;
        }
        {
            SDL_Rect clip = UIPanelObjectWorkspace_OperationContentClipRect(ui, doc, metrics, font_h);
            SDL_Rect previous_clip = {0, 0, 0, 0};
            SDL_bool had_clip = SDL_RenderIsClipEnabled(renderer);
            const int row_step = UIPanelObjectWorkspace_OperationRowStep(metrics);
            const int scroll_offset =
                (int)UIPanelObjectWorkspace_ClampedOperationScroll(ui, doc, metrics, font_h);
            int cursor_y = clip.y - scroll_offset;
            if (had_clip == SDL_TRUE) {
                SDL_RenderGetClipRect(renderer, &previous_clip);
            }
            if (clip.w > 0 && clip.h > 0) {
                SDL_RenderSetClipRect(renderer, &clip);
            } else {
                SDL_RenderSetClipRect(renderer, NULL);
            }
            if (doc->operationCount == 0u) {
                UIPanelSummary_DrawTextClipped(renderer,
                                               font,
                                               "No operations recorded yet.",
                                               clip.x,
                                               clip.y,
                                               clip.w,
                                               font_h + 4,
                                               label_color);
            }
            for (size_t i = 0u; i < doc->operationCount; ++i) {
                char line0[160];
                char line1[192];
                SDL_Rect row_rect = {clip.x, cursor_y, clip.w, row_step - 4};
                const ObjectAuthoringOperation* op = &doc->operations[i];
                const bool selected = doc->selectedOperationId == op->operationId;
                const bool hovered = ui->objectModelTree.hoverOperationIndex == (int)i;
                SDL_Color row_fill = selected ? palette.button_fill : palette.workspace_fill;
                SDL_Color row_border = selected ? palette.button_border : palette.pane_border;
                if (hovered && !selected) {
                    row_fill = palette.button_fill;
                    row_fill.a = 165;
                }
                if (row_rect.y + row_rect.h >= clip.y && row_rect.y <= clip.y + clip.h) {
                    UIPanelVisual_DrawInteractiveRow(renderer,
                                                     row_rect,
                                                     row_fill,
                                                     row_border,
                                                     accent_color,
                                                     hovered,
                                                     selected,
                                                     3,
                                                     55);
                    snprintf(line0,
                             sizeof(line0),
                             "#%u  %s",
                             op->operationId,
                             UIPanelObjectWorkspace_OperationLabel(op->kind));
                    if (op->faceRef.bodyId != 0u) {
                        const ObjectAuthoringFaceRefStatus face_status =
                            ObjectAuthoringDocument_CheckFaceRef(doc, op->faceRef);
                        snprintf(line1,
                                 sizeof(line1),
                                 "FaceID %u  %s  Ref %s",
                                 op->faceRef.faceId,
                                 Layout_Object3DFaceKind_Label(op->faceRef.primitiveFace),
                                 ObjectAuthoringFaceRefStatus_Label(face_status));
                    } else if (op->bodySnapshot.bodyId != 0u) {
                        snprintf(line1,
                                 sizeof(line1),
                                 "Body #%u  Result %zu",
                                 op->bodySnapshot.bodyId,
                                 op->resultBodyCount);
                    } else {
                        snprintf(line1, sizeof(line1), "Result bodies %zu", op->resultBodyCount);
                    }
                    UIPanelSummary_DrawTextClipped(renderer,
                                                   font,
                                                   line0,
                                                   row_rect.x + metrics.pad_x,
                                                   row_rect.y + metrics.row_text_y - 1,
                                                   row_rect.w - (metrics.pad_x * 2),
                                                   font_h + 4,
                                                   selected ? accent_color : value_color);
                    UIPanelSummary_DrawTextClipped(renderer,
                                                   font,
                                                   line1,
                                                   row_rect.x + metrics.pad_x,
                                                   row_rect.y + metrics.row_text_y + font_h + 1,
                                                   row_rect.w - (metrics.pad_x * 2),
                                                   font_h + 4,
                                                   label_color);
                }
                cursor_y += row_step;
            }
            if (had_clip == SDL_TRUE) {
                SDL_RenderSetClipRect(renderer, &previous_clip);
            } else {
                SDL_RenderSetClipRect(renderer, NULL);
            }
            if (UIPanelObjectWorkspace_HasOperationScrollbar(ui, doc, metrics, font_h)) {
                SDL_Rect track = UIPanelObjectWorkspace_OperationTrackRect(ui, doc, metrics, font_h);
                SDL_Rect thumb = UIPanelObjectWorkspace_OperationThumbRect(ui, doc, metrics, font_h);
                SDL_Color track_fill = palette.workspace_fill;
                SDL_Color thumb_fill = palette.button_border;
                track_fill.a = 175;
                thumb_fill.a = 220;
                UIPanelVisual_DrawScrollbar(renderer,
                                            track,
                                            thumb,
                                            track_fill,
                                            palette.pane_border,
                                            thumb_fill,
                                            palette.button_border,
                                            ui->objectModelTree.operationScrollbarDragging);
            }
        }
    } else {
        UIPanelSummary_DrawTextClipped(renderer,
                                       font,
                                       "No authoring document attached.",
                                       detail.x + metrics.pad_x,
                                       y,
                                       detail.w - (metrics.pad_x * 2),
                                       font_h + 4,
                                       label_color);
    }
}
