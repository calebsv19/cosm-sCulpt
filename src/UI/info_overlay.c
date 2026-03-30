#include "UI/info_overlay.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Layout/layout.h"
#include "Layout/Grid/grid.h"
#include "UI/font_manager.h"
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

void Render_InfoOverlay(SDL_Renderer* renderer) {
    if (!renderer) return;

    GlobalState* state = Global_Get();
    if (!state) return;

    int width = Global_GetScreenWidth();
    LineDrawing3dThemePalette palette = {0};
    const bool has_shared_palette = line_drawing3d_shared_theme_resolve_palette(&palette);

    SDL_Rect panel = { 0, 0, width, INFO_OVERLAY_HEIGHT };
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

    if (editor->selectedAnchorIndex >= 0 &&
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
        snprintf(line1, sizeof(line1), "Select an anchor or wall to view details. Hover shows quick info.");
    }

    const char* path = Global_GetCurrentConfigPath();
    const char* base = path ? strrchr(path, '/') : NULL;
    base = base ? base + 1 : (path ? path : "(unsaved)");
    bool dirty = Global_IsLayoutDirty();
    size_t undoCount = Editor_UndoCount(editor);
    size_t redoCount = Editor_RedoCount(editor);

    char statusLine[256];
    const char* modeLabel = Global_GetSpaceModeLabel(state->spaceMode);
    const char* viewLabel = SpaceAdapter_IsFreeViewEnabled(&viewCtx) ? "FREE" : "PLANE";
    snprintf(statusLine, sizeof(statusLine),
             "File: %s%s  |  Mode:%s  View:%s  Plane: %s (%s=%.2f)  |  Undo:%zu  Redo:%zu  |  Delete Mode: %s",
             base ? base : "(unsaved)",
             dirty ? " *" : "",
             modeLabel,
             viewLabel,
             planeLabel,
             planeCoordLabel,
             plane.offset,
             undoCount,
             redoCount,
             editor->deleteMode == DELETE_MODE_SAFE ? "SAFE" : "AUTO_PRUNE");

    SDL_Color textMain = has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 230, 255};
    SDL_Color textSub = has_shared_palette ? palette.text_muted : (SDL_Color){200, 200, 210, 255};
    int padding = 12;
    DrawText(renderer, line1, padding, padding, textMain);
    if (line2[0]) {
        DrawText(renderer, line2, padding, padding + 20, textSub);
        DrawText(renderer, statusLine, padding, padding + 40, textSub);
    } else {
        DrawText(renderer, statusLine, padding, padding + 24, textSub);
    }
}
