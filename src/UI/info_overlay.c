#include "UI/info_overlay.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Layout/layout.h"
#include "Layout/Grid/grid.h"
#include "UI/font_manager.h"
#include "UI/ui_panel.h"
#include "UI/shared_theme_font_adapter.h"
#include "Editor/editor.h"
#include "Render/vulkan_adapter.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

static void DrawText(SDL_Renderer* renderer, const char* text, int x, int y, SDL_Color color) {
    if (!text || !renderer) return;
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;

#if USE_VULKAN
    VulkanAdapter_DrawText(renderer, font, text, x, y, color);
#else
    SDL_Surface* surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }

    SDL_Rect dst = {
        .x = x,
        .y = y,
        .w = surf->w,
        .h = surf->h
    };

    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
#endif
}

static int InfoOverlay_FontHeightPx(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) {
        h = TTF_FontHeight(font);
    }
    if (h < 12) h = 12;
    return h;
}

static int InfoOverlay_LineGapPx(void) {
    int gap = InfoOverlay_FontHeightPx() / 3;
    if (gap < 4) gap = 4;
    return gap;
}

static int InfoOverlay_PaddingPx(void) {
    int pad = InfoOverlay_FontHeightPx() / 2;
    if (pad < 10) pad = 10;
    return pad;
}

int InfoOverlay_HeightPx(void) {
    const int line_h = InfoOverlay_FontHeightPx();
    const int line_gap = InfoOverlay_LineGapPx();
    const int pad = InfoOverlay_PaddingPx();
    /* line1 + line2 + status */
    return (pad * 2) + (line_h * 3) + (line_gap * 2);
}

static const char* PlaneResizeHandle_Label(int handleKind) {
    switch ((PlaneResizeHandleKind)handleKind) {
        case PLANE_RESIZE_HANDLE_CORNER_NEG_U_NEG_V: return "Corner(-U,-V)";
        case PLANE_RESIZE_HANDLE_CORNER_POS_U_NEG_V: return "Corner(+U,-V)";
        case PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V: return "Corner(+U,+V)";
        case PLANE_RESIZE_HANDLE_CORNER_NEG_U_POS_V: return "Corner(-U,+V)";
        case PLANE_RESIZE_HANDLE_EDGE_NEG_V: return "Edge(-V)";
        case PLANE_RESIZE_HANDLE_EDGE_POS_U: return "Edge(+U)";
        case PLANE_RESIZE_HANDLE_EDGE_POS_V: return "Edge(+V)";
        case PLANE_RESIZE_HANDLE_EDGE_NEG_U: return "Edge(-U)";
        case PLANE_RESIZE_HANDLE_NONE:
        default: return "None";
    }
}

static const char* RectPrismResizeHandle_Label(int handleKind) {
    switch ((RectPrismResizeHandleKind)handleKind) {
        case RECT_PRISM_RESIZE_HANDLE_CORNER_0: return "Corner(0)";
        case RECT_PRISM_RESIZE_HANDLE_CORNER_1: return "Corner(1)";
        case RECT_PRISM_RESIZE_HANDLE_CORNER_2: return "Corner(2)";
        case RECT_PRISM_RESIZE_HANDLE_CORNER_3: return "Corner(3)";
        case RECT_PRISM_RESIZE_HANDLE_CORNER_4: return "Corner(4)";
        case RECT_PRISM_RESIZE_HANDLE_CORNER_5: return "Corner(5)";
        case RECT_PRISM_RESIZE_HANDLE_CORNER_6: return "Corner(6)";
        case RECT_PRISM_RESIZE_HANDLE_CORNER_7: return "Corner(7)";
        case RECT_PRISM_RESIZE_HANDLE_EDGE_0: return "Edge(0)";
        case RECT_PRISM_RESIZE_HANDLE_EDGE_1: return "Edge(1)";
        case RECT_PRISM_RESIZE_HANDLE_EDGE_2: return "Edge(2)";
        case RECT_PRISM_RESIZE_HANDLE_EDGE_3: return "Edge(3)";
        case RECT_PRISM_RESIZE_HANDLE_EDGE_4: return "Edge(4)";
        case RECT_PRISM_RESIZE_HANDLE_EDGE_5: return "Edge(5)";
        case RECT_PRISM_RESIZE_HANDLE_EDGE_6: return "Edge(6)";
        case RECT_PRISM_RESIZE_HANDLE_EDGE_7: return "Edge(7)";
        case RECT_PRISM_RESIZE_HANDLE_EDGE_8: return "Edge(8)";
        case RECT_PRISM_RESIZE_HANDLE_EDGE_9: return "Edge(9)";
        case RECT_PRISM_RESIZE_HANDLE_EDGE_10: return "Edge(10)";
        case RECT_PRISM_RESIZE_HANDLE_EDGE_11: return "Edge(11)";
        case RECT_PRISM_RESIZE_HANDLE_NONE:
        default: return "None";
    }
}

static const char* Object3DKind_Label(Object3DKind kind) {
    switch (kind) {
        case OBJECT3D_KIND_PLANE: return "Plane";
        case OBJECT3D_KIND_RECT_PRISM: return "RectPrism";
        case OBJECT3D_KIND_UNKNOWN:
        default: return "Unknown";
    }
}

static void FormatDimensionDisplay(float world_value, char* out, size_t out_size) {
    double display = 0.0;
    const char* symbol = UIPanel_GetDisplayUnitSymbol();
    if (!out || out_size == 0) return;
    if (UIPanel_ConvertWorldToDisplay((double)world_value, &display)) {
        snprintf(out, out_size, "%.2f%s", display, symbol);
    } else {
        snprintf(out, out_size, "%.2f", world_value);
    }
}

void Render_InfoOverlay(SDL_Renderer* renderer) {
    if (!renderer) return;

    GlobalState* state = Global_Get();
    if (!state) return;

    int width = Global_GetScreenWidth();
    LineDrawing3dThemePalette palette = {0};
    const bool has_shared_palette = line_drawing3d_shared_theme_resolve_palette(&palette);

    SDL_Rect panel = { 0, 0, width, InfoOverlay_HeightPx() };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 28, 30, 35, 235);
    }
    SDL_RenderFillRect(renderer, &panel);

    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 60, 62, 70, 255);
    }
    SDL_RenderDrawRect(renderer, &panel);

    EditorState* editor = &state->editor;
    Layout* layout = &state->layout;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    ViewPlane plane = viewCtx.plane;
    const char* planeLabel = "XY";
    const char* planeCoordLabel = "z";
    if (plane.axis == VIEW_PLANE_YZ) {
        planeLabel = "YZ";
        planeCoordLabel = "x";
    } else if (plane.axis == VIEW_PLANE_XZ) {
        planeLabel = "XZ";
        planeCoordLabel = "y";
    }

    char line1[256] = {0};
    char line2[256] = {0};
    const Object3D* selectedObject = NULL;
    const Object3D* hoveredObject = NULL;

    if (editor->selectedObject3DId != 0u) {
        selectedObject = Layout_ObjectStore_FindConst(&layout->objectStore, editor->selectedObject3DId);
    }
    if (editor->hoveredObject3DId != 0u) {
        hoveredObject = Layout_ObjectStore_FindConst(&layout->objectStore, editor->hoveredObject3DId);
    }

    if (selectedObject) {
        snprintf(line1, sizeof(line1),
                 "Object3D #%u  Kind:%s  Pos:(%.2f, %.2f, %.2f)",
                 selectedObject->objectId,
                 Object3DKind_Label(selectedObject->kind),
                 selectedObject->transform.position.x,
                 selectedObject->transform.position.y,
                 selectedObject->transform.position.z);
        if (selectedObject->kind == OBJECT3D_KIND_PLANE) {
            const char* selectedHandle = PlaneResizeHandle_Label(editor->selectedObject3DResizeHandle);
            char widthText[32] = {0};
            char heightText[32] = {0};
            FormatDimensionDisplay(selectedObject->plane.width, widthText, sizeof(widthText));
            FormatDimensionDisplay(selectedObject->plane.height, heightText, sizeof(heightText));
            snprintf(line2, sizeof(line2),
                     "Plane: W %s  H %s  LockPlane:%s  BoundsLock:%s  Gizmo:%s  Resize:%s%s%s",
                     widthText,
                     heightText,
                     selectedObject->plane.lockToConstructionPlane ? "Yes" : "No",
                     selectedObject->plane.lockToBounds ? "Yes" : "No",
                     editor->object3DRotateMode ? "Rotate" : "Move",
                     selectedHandle,
                     editor->isResizingObject3D
                         ? (editor->isPreciseDrag ? "  Drag:Precise" : "  Drag:Snapped")
                         : "",
                     editor->isRotatingObject3D
                         ? (editor->isPreciseDrag ? "  Rotate:Free" : "  Rotate:Snapped")
                         : "");
        } else if (selectedObject->kind == OBJECT3D_KIND_RECT_PRISM) {
            const char* selectedHandle =
                (editor->selectedObject3DPrismHandle != RECT_PRISM_RESIZE_HANDLE_NONE)
                    ? RectPrismResizeHandle_Label(editor->selectedObject3DPrismHandle)
                    : PlaneResizeHandle_Label(editor->selectedObject3DResizeHandle);
            char widthText[32] = {0};
            char heightText[32] = {0};
            char depthText[32] = {0};
            FormatDimensionDisplay(selectedObject->rectPrism.width, widthText, sizeof(widthText));
            FormatDimensionDisplay(selectedObject->rectPrism.height, heightText, sizeof(heightText));
            FormatDimensionDisplay(selectedObject->rectPrism.depth, depthText, sizeof(depthText));
            snprintf(line2, sizeof(line2),
                     "RectPrism: W %s  H %s  D %s  LockPlane:%s  BoundsLock:%s  Gizmo:%s  Resize:%s%s%s",
                     widthText,
                     heightText,
                     depthText,
                     selectedObject->rectPrism.lockToConstructionPlane ? "Yes" : "No",
                     selectedObject->rectPrism.lockToBounds ? "Yes" : "No",
                     editor->object3DRotateMode ? "Rotate" : "Move",
                     selectedHandle,
                     editor->isResizingObject3D
                         ? (editor->isPreciseDrag ? "  Drag:Precise" : "  Drag:Snapped")
                         : "",
                     editor->isRotatingObject3D
                         ? (editor->isPreciseDrag ? "  Rotate:Free" : "  Rotate:Snapped")
                         : "");
        }
    } else if (editor->selectedAnchorIndex >= 0 &&
        editor->selectedAnchorIndex < (int)layout->anchorCount) {
        Anchor* anchor = &layout->anchors[editor->selectedAnchorIndex];
        int selectedCount = Editor_SelectedAnchorCount(editor);
        if (selectedCount > 1) {
            snprintf(line1, sizeof(line1),
                     "Anchor #%d  Pos:(%.2f, %.2f, %.2f)  Group:%d",
                     editor->selectedAnchorIndex,
                     anchor->pos.x, anchor->pos.y, anchor->pos.z,
                     selectedCount);
        } else {
            snprintf(line1, sizeof(line1),
                     "Anchor #%d  Pos:(%.2f, %.2f, %.2f)",
                     editor->selectedAnchorIndex,
                     anchor->pos.x, anchor->pos.y, anchor->pos.z);
        }
        const char* typeLabel = (anchor->type == ANCHOR_TYPE_CURVE) ? "Curve" : "Corner";
        bool draggingSelected = editor->isDraggingAnchor;
        if (anchor->type == ANCHOR_TYPE_CURVE) {
            snprintf(line2, sizeof(line2),
                     "Persistent:%s  Connections:%d  Type:%s%s  Handles: In %.2f@%.1f°  Out %.2f@%.1f°%s",
                     anchor->isPersistent ? "Yes" : "No",
                     anchor->connectionCount,
                     typeLabel,
                     anchor->handlesLinked ? " (Linked)" : "",
                     anchor->handleInLength,
                     anchor->handleInAngleDeg,
                     anchor->handleOutLength,
                     anchor->handleOutAngleDeg,
                     draggingSelected ? (editor->isPreciseDrag ? "  Drag:Precise" : "  Drag:Snapped") : "");
        } else {
            snprintf(line2, sizeof(line2),
                     "Persistent:%s  Connections:%d  Type:%s%s",
                     anchor->isPersistent ? "Yes" : "No",
                     anchor->connectionCount,
                     typeLabel,
                     draggingSelected ? (editor->isPreciseDrag ? "  Drag:Precise" : "  Drag:Snapped") : "");
        }
    } else if (editor->selectedWallIndex >= 0 &&
               editor->selectedWallIndex < (int)layout->wallCount) {
        Wall* wall = &layout->walls[editor->selectedWallIndex];
        if (wall->isDeleted) {
            snprintf(line1, sizeof(line1), "Wall #%d (pending deletion)", editor->selectedWallIndex);
        } else {
            Anchor* a = &layout->anchors[wall->anchorA];
            Anchor* b = &layout->anchors[wall->anchorB];
            float dx = b->pos.x - a->pos.x;
            float dy = b->pos.y - a->pos.y;
            float dz = b->pos.z - a->pos.z;
            float length = sqrtf(dx * dx + dy * dy + dz * dz);
            snprintf(line1, sizeof(line1),
                     "Wall #%d  A:%d (%.2f, %.2f, %.2f)  B:%d (%.2f, %.2f, %.2f)",
                     editor->selectedWallIndex,
                     wall->anchorA, a->pos.x, a->pos.y, a->pos.z,
                     wall->anchorB, b->pos.x, b->pos.y, b->pos.z);
            snprintf(line2, sizeof(line2),
                     "Length: %.2f units  LockLength:%s",
                     length,
                     wall->lockLength ? "Yes" : "No");
        }
    } else if (hoveredObject) {
        snprintf(line1, sizeof(line1),
                 "Hover Object3D #%u  Kind:%s  Pos:(%.2f, %.2f, %.2f)",
                 hoveredObject->objectId,
                 Object3DKind_Label(hoveredObject->kind),
                 hoveredObject->transform.position.x,
                 hoveredObject->transform.position.y,
                 hoveredObject->transform.position.z);
        if (hoveredObject->kind == OBJECT3D_KIND_PLANE) {
            const char* hoveredHandle = PlaneResizeHandle_Label(editor->hoveredObject3DResizeHandle);
            char widthText[32] = {0};
            char heightText[32] = {0};
            FormatDimensionDisplay(hoveredObject->plane.width, widthText, sizeof(widthText));
            FormatDimensionDisplay(hoveredObject->plane.height, heightText, sizeof(heightText));
            snprintf(line2, sizeof(line2),
                     "Plane: W %s  H %s  LockPlane:%s  BoundsLock:%s  HoverResize:%s",
                     widthText,
                     heightText,
                     hoveredObject->plane.lockToConstructionPlane ? "Yes" : "No",
                     hoveredObject->plane.lockToBounds ? "Yes" : "No",
                     hoveredHandle);
        } else if (hoveredObject->kind == OBJECT3D_KIND_RECT_PRISM) {
            const char* hoveredHandle =
                (editor->hoveredObject3DPrismHandle != RECT_PRISM_RESIZE_HANDLE_NONE)
                    ? RectPrismResizeHandle_Label(editor->hoveredObject3DPrismHandle)
                    : PlaneResizeHandle_Label(editor->hoveredObject3DResizeHandle);
            char widthText[32] = {0};
            char heightText[32] = {0};
            char depthText[32] = {0};
            FormatDimensionDisplay(hoveredObject->rectPrism.width, widthText, sizeof(widthText));
            FormatDimensionDisplay(hoveredObject->rectPrism.height, heightText, sizeof(heightText));
            FormatDimensionDisplay(hoveredObject->rectPrism.depth, depthText, sizeof(depthText));
            snprintf(line2, sizeof(line2),
                     "RectPrism: W %s  H %s  D %s  LockPlane:%s  BoundsLock:%s  HoverResize:%s",
                     widthText,
                     heightText,
                     depthText,
                     hoveredObject->rectPrism.lockToConstructionPlane ? "Yes" : "No",
                     hoveredObject->rectPrism.lockToBounds ? "Yes" : "No",
                     hoveredHandle);
        }
    } else if (editor->hoveredAnchorIndex >= 0 &&
               editor->hoveredAnchorIndex < (int)layout->anchorCount) {
        Anchor* anchor = &layout->anchors[editor->hoveredAnchorIndex];
        snprintf(line1, sizeof(line1),
                 "Hover Anchor #%d  Pos:(%.2f, %.2f, %.2f)",
                 editor->hoveredAnchorIndex,
                 anchor->pos.x, anchor->pos.y, anchor->pos.z);
        const char* typeLabel = (anchor->type == ANCHOR_TYPE_CURVE) ? "Curve" : "Corner";
        if (anchor->type == ANCHOR_TYPE_CURVE) {
            snprintf(line2, sizeof(line2),
                     "Persistent:%s  Connections:%d  Type:%s%s  Handles: In %.2f@%.1f°  Out %.2f@%.1f°",
                     anchor->isPersistent ? "Yes" : "No",
                     anchor->connectionCount,
                     typeLabel,
                     anchor->handlesLinked ? " (Linked)" : "",
                     anchor->handleInLength,
                     anchor->handleInAngleDeg,
                     anchor->handleOutLength,
                     anchor->handleOutAngleDeg);
        } else {
            snprintf(line2, sizeof(line2),
                     "Persistent:%s  Connections:%d  Type:%s",
                     anchor->isPersistent ? "Yes" : "No",
                     anchor->connectionCount,
                     typeLabel);
        }
    } else if (editor->hoveredWallIndex >= 0 &&
               editor->hoveredWallIndex < (int)layout->wallCount) {
        Wall* wall = &layout->walls[editor->hoveredWallIndex];
        Anchor* a = &layout->anchors[wall->anchorA];
        Anchor* b = &layout->anchors[wall->anchorB];
        float dx = b->pos.x - a->pos.x;
        float dy = b->pos.y - a->pos.y;
        float dz = b->pos.z - a->pos.z;
        float length = sqrtf(dx * dx + dy * dy + dz * dz);
        snprintf(line1, sizeof(line1),
                 "Hover Wall #%d  A:%d (%.2f, %.2f, %.2f)  B:%d (%.2f, %.2f, %.2f)",
                 editor->hoveredWallIndex,
                 wall->anchorA, a->pos.x, a->pos.y, a->pos.z,
                 wall->anchorB, b->pos.x, b->pos.y, b->pos.z);
        snprintf(line2, sizeof(line2),
                 "Length: %.2f units  LockLength:%s",
                 length,
                 wall->lockLength ? "Yes" : "No");
    } else {
        snprintf(line1, sizeof(line1),
                 "Select anchor/wall/object for details. 3D: Add Plane/Add Prism or Shift+P/Shift+R.");
    }

    const char* path = Global_GetCurrentConfigPath();
    const char* base = path ? strrchr(path, '/') : NULL;
    base = base ? base + 1 : (path ? path : "(unsaved)");
    bool dirty = Global_IsLayoutDirty();
    size_t undoCount = Editor_UndoCount(editor);
    size_t redoCount = Editor_RedoCount(editor);
    size_t objectCount = Layout_ObjectStore_LiveCount(&layout->objectStore);

    char statusLine[768];
    const char* modeLabel = Global_GetSpaceModeLabel(state->spaceMode);
    const char* viewLabel = SpaceAdapter_IsFreeViewEnabled(&viewCtx) ? "FREE" : "PLANE";
    {
        char constructionPlaneSummary[168];
        char boundsSummary[176];
        const ConstructionPlane3D* cp = &layout->scene3d.constructionPlane;
        const SceneBounds3D* bounds = &layout->scene3d.bounds;
        if (cp->mode == CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED) {
            const char* cpPlaneLabel = "XY";
            const char* cpCoordLabel = "z";
            if (cp->axisAligned.axis == VIEW_PLANE_YZ) {
                cpPlaneLabel = "YZ";
                cpCoordLabel = "x";
            } else if (cp->axisAligned.axis == VIEW_PLANE_XZ) {
                cpPlaneLabel = "XZ";
                cpCoordLabel = "y";
            }
            snprintf(constructionPlaneSummary,
                     sizeof(constructionPlaneSummary),
                     "CP:Axis %s (%s=%.2f)",
                     cpPlaneLabel,
                     cpCoordLabel,
                     cp->axisAligned.offset);
        } else {
            snprintf(constructionPlaneSummary,
                     sizeof(constructionPlaneSummary),
                     "CP:Custom O(%.2f,%.2f,%.2f) N(%.2f,%.2f,%.2f)",
                     cp->customFrame.origin.x,
                     cp->customFrame.origin.y,
                     cp->customFrame.origin.z,
                     cp->customFrame.normal.x,
                     cp->customFrame.normal.y,
                     cp->customFrame.normal.z);
        }
        snprintf(boundsSummary,
                 sizeof(boundsSummary),
                 "Bounds:%s Clamp:%s Min(%.1f,%.1f,%.1f) Max(%.1f,%.1f,%.1f)",
                 bounds->enabled ? "On" : "Off",
                 bounds->clampOnEdit ? "On" : "Off",
                 bounds->min.x, bounds->min.y, bounds->min.z,
                 bounds->max.x, bounds->max.y, bounds->max.z);
        snprintf(statusLine, sizeof(statusLine),
             "File:%s%s  |  Mode:%s  View:%s  Plane:%s (%s=%.2f)  |  %s  |  %s  |  Gizmo:%s Obj3D:%zu Undo:%zu Redo:%zu Delete:%s",
             base ? base : "(unsaved)",
             dirty ? " *" : "",
             modeLabel,
             viewLabel,
             planeLabel,
             planeCoordLabel,
             plane.offset,
             constructionPlaneSummary,
             boundsSummary,
             editor->object3DRotateMode ? "Rotate" : "Move",
             objectCount,
             undoCount,
             redoCount,
             editor->deleteMode == DELETE_MODE_SAFE ? "SAFE" : "AUTO_PRUNE");
    }

    SDL_Color textMain = has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 230, 255};
    SDL_Color textSub = has_shared_palette ? palette.text_muted : (SDL_Color){200, 200, 210, 255};
    int padding = InfoOverlay_PaddingPx();
    int lineHeight = InfoOverlay_FontHeightPx();
    int lineGap = InfoOverlay_LineGapPx();
    DrawText(renderer, line1, padding, padding, textMain);
    if (line2[0]) {
        DrawText(renderer, line2, padding, padding + lineHeight + lineGap, textSub);
        DrawText(renderer, statusLine, padding, padding + (lineHeight + lineGap) * 2, textSub);
    } else {
        DrawText(renderer, statusLine, padding, padding + lineHeight + lineGap, textSub);
    }
}
