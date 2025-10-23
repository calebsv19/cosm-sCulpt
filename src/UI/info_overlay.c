#include "UI/info_overlay.h"
#include "Core/global_state.h"
#include "Layout/layout.h"
#include "Layout/Grid/grid.h"
#include "UI/font_manager.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>

static void DrawText(SDL_Renderer* renderer, const char* text, int x, int y, SDL_Color color) {
    if (!text || !renderer) return;
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;

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
}

void Render_InfoOverlay(SDL_Renderer* renderer) {
    if (!renderer) return;

    GlobalState* state = Global_Get();
    if (!state) return;

    int width = 0, height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);

    SDL_Rect panel = { 0, 0, width, INFO_OVERLAY_HEIGHT };
    SDL_SetRenderDrawColor(renderer, 28, 30, 35, 235);
    SDL_RenderFillRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 60, 62, 70, 255);
    SDL_RenderDrawRect(renderer, &panel);

    EditorState* editor = &state->editor;
    Layout* layout = &state->layout;

    char line1[256] = {0};
    char line2[256] = {0};

    if (editor->selectedAnchorIndex >= 0 &&
        editor->selectedAnchorIndex < (int)layout->anchorCount) {
        Anchor* anchor = &layout->anchors[editor->selectedAnchorIndex];
        snprintf(line1, sizeof(line1),
                 "Anchor #%d  Pos:(%.2f, %.2f)",
                 editor->selectedAnchorIndex,
                 anchor->pos.x, anchor->pos.y);
        snprintf(line2, sizeof(line2),
                 "Persistent:%s  Connections:%d",
                 anchor->isPersistent ? "Yes" : "No",
                 anchor->connectionCount);
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
            float length = sqrtf(dx * dx + dy * dy);
            snprintf(line1, sizeof(line1),
                     "Wall #%d  A:%d (%.2f, %.2f)  B:%d (%.2f, %.2f)",
                     editor->selectedWallIndex,
                     wall->anchorA, a->pos.x, a->pos.y,
                     wall->anchorB, b->pos.x, b->pos.y);
            snprintf(line2, sizeof(line2),
                     "Length: %.2f units  LockLength:%s",
                     length,
                     wall->lockLength ? "Yes" : "No");
        }
    } else {
        snprintf(line1, sizeof(line1), "Select an anchor or wall to view details.");
        snprintf(line2, sizeof(line2), "Undo: Ctrl+Z   Redo: Ctrl+Y / Shift+Ctrl+Z   Delete Mode: %s",
                 editor->deleteMode == DELETE_MODE_SAFE ? "SAFE" : "AUTO_PRUNE");
    }

    SDL_Color textMain = { 230, 230, 230, 255 };
    SDL_Color textSub = { 200, 200, 210, 255 };
    int padding = 12;
    DrawText(renderer, line1, padding, padding, textMain);
    DrawText(renderer, line2, padding, padding + 20, textSub);
}
