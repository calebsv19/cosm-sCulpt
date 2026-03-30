#include "UI/render_ui_panel.h"
#include "UI/ui_panel.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "Core/global_state.h"
#include "Render/vulkan_adapter.h"

#include <SDL2/SDL.h>
#include <stdio.h>

#include "UI/font_manager.h"  // Ensure this is included

void DrawButton(SDL_Renderer* r, const UIButton* btn) {
    LineDrawing3dThemePalette palette = {0};
    SDL_Color button_fill = {70, 70, 70, 200};
    SDL_Color button_border = {180, 180, 180, 255};
    SDL_Color textColor = {255, 255, 255, 255};
    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        button_fill = palette.button_fill;
        button_border = palette.button_border;
        textColor = palette.button_text;
    }

    // ─── Button Background ─────────────────────
    SDL_SetRenderDrawColor(r, button_fill.r, button_fill.g, button_fill.b, button_fill.a);
    SDL_RenderFillRect(r, &btn->bounds);

    // ─── Button Border ─────────────────────────
    SDL_SetRenderDrawColor(r, button_border.r, button_border.g, button_border.b, button_border.a);
    SDL_RenderDrawRect(r, &btn->bounds);

    // ─── Button Label Text ─────────────────────
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;

    char dynamicLabel[64];
    const char* label = btn->label;
    if (btn->id == 16) {
        GlobalState* state = Global_Get();
        const char* modeLabel = state ? Global_GetSpaceModeLabel(state->spaceMode) : "3D";
        snprintf(dynamicLabel, sizeof(dynamicLabel), "Mode: %s (M)", modeLabel);
        label = dynamicLabel;
    }

#if USE_VULKAN
    int textW = 0;
    int textH = 0;
    if (TTF_SizeText(font, label, &textW, &textH) != 0) {
        return;
    }
#else
    SDL_Surface* surf = TTF_RenderText_Blended(font, label, textColor);
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }

    int textW, textH;
    SDL_QueryTexture(tex, NULL, NULL, &textW, &textH);
#endif

    SDL_Rect dst = {
        btn->bounds.x + (btn->bounds.w - textW) / 2,
        btn->bounds.y + (btn->bounds.h - textH) / 2,
        textW, textH
    };

#if USE_VULKAN
    VulkanAdapter_DrawText(r, font, label, dst.x, dst.y, textColor);
#else
    SDL_RenderCopy(r, tex, NULL, &dst);

    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
#endif
}


void Render_UIPanel(const UIPanelState* ui, SDL_Renderer* renderer) {
    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        DrawButton(renderer, btn);
    }
}
