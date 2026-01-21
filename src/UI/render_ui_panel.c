#include "UI/render_ui_panel.h"
#include "UI/ui_panel.h"
#include "UI/font_manager.h"
#include "Render/vulkan_adapter.h"

#include <SDL2/SDL.h>

#include "UI/font_manager.h"  // Ensure this is included

void DrawButton(SDL_Renderer* r, const UIButton* btn) {
    // ─── Button Background ─────────────────────
    SDL_SetRenderDrawColor(r, 70, 70, 70, 200);
    SDL_RenderFillRect(r, &btn->bounds);

    // ─── Button Border ─────────────────────────
    SDL_SetRenderDrawColor(r, 180, 180, 180, 255);
    SDL_RenderDrawRect(r, &btn->bounds);

    // ─── Button Label Text ─────────────────────
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;

    SDL_Color textColor = { 255, 255, 255, 255 };
#if USE_VULKAN
    int textW = 0;
    int textH = 0;
    if (TTF_SizeText(font, btn->label, &textW, &textH) != 0) {
        return;
    }
#else
    SDL_Surface* surf = TTF_RenderText_Blended(font, btn->label, textColor);
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
    VulkanAdapter_DrawText(r, font, btn->label, dst.x, dst.y, textColor);
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
