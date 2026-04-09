#include "UI/render_ui_panel.h"
#include "UI/ui_panel.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "Core/global_state.h"
#include "Render/vulkan_adapter.h"

#include <SDL2/SDL.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void DrawTextBasic(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
#if USE_VULKAN
    VulkanAdapter_DrawText(renderer, font, text, x, y, color);
#else
    SDL_Surface* surf = TTF_RenderText_Blended(font, text, color);
    SDL_Texture* tex = NULL;
    SDL_Rect dst = {0, 0, 0, 0};
    if (!surf) return;
    tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }
    dst.x = x;
    dst.y = y;
    dst.w = surf->w;
    dst.h = surf->h;
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
#endif
}

static void BuildEllipsizedText(TTF_Font* font,
                                const char* text,
                                int maxWidth,
                                char* out,
                                size_t outSize) {
    static const char* k_ellipsis = "...";
    int width = 0;
    int ellipsisWidth = 0;
    size_t len = 0;
    if (!font || !text || !out || outSize == 0 || maxWidth <= 0) {
        if (out && outSize > 0) out[0] = '\0';
        return;
    }
    if (TTF_SizeUTF8(font, text, &width, NULL) == 0 && width <= maxWidth) {
        snprintf(out, outSize, "%s", text);
        return;
    }
    if (TTF_SizeUTF8(font, k_ellipsis, &ellipsisWidth, NULL) != 0 || ellipsisWidth >= maxWidth) {
        out[0] = '\0';
        return;
    }
    len = strlen(text);
    while (len > 0) {
        --len;
        if (len + strlen(k_ellipsis) + 1 >= outSize) continue;
        memcpy(out, text, len);
        out[len] = '\0';
        strcat(out, k_ellipsis);
        if (TTF_SizeUTF8(font, out, &width, NULL) == 0 && width <= maxWidth) {
            return;
        }
    }
    out[0] = '\0';
}

static void DrawTextClipped(SDL_Renderer* renderer,
                            TTF_Font* font,
                            const char* text,
                            int x,
                            int y,
                            int maxWidth,
                            SDL_Color color) {
    char clipped[512];
    BuildEllipsizedText(font, text, maxWidth, clipped, sizeof(clipped));
    DrawTextBasic(renderer, font, clipped, x, y, color);
}

static void DrawButton(SDL_Renderer* r, const UIButton* btn) {
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
    if (btn->id == UI_BTN_TOGGLE_SPACE_MODE) {
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

    DrawTextBasic(r, font, label, dst.x, dst.y, textColor);

#if !USE_VULKAN
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
#endif
}

static void RenderRootSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    LineDrawing3dThemePalette palette = {0};
    SDL_Color labelColor = {200, 200, 210, 255};
    SDL_Color valueColor = {230, 230, 235, 255};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    SDL_Rect panel = {0, 0, 0, 0};
    int minX = INT_MAX;
    int maxX = 0;
    int maxY = 0;
    if (!ui || !renderer || !font) return;

    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        labelColor = palette.text_muted;
        valueColor = palette.text_primary;
    }

    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        if (btn->side != UI_PANEL_LEFT) continue;
        if (btn->bounds.x < minX) minX = btn->bounds.x;
        if (btn->bounds.x + btn->bounds.w > maxX) maxX = btn->bounds.x + btn->bounds.w;
        if (btn->bounds.y + btn->bounds.h > maxY) maxY = btn->bounds.y + btn->bounds.h;
    }
    if (minX == INT_MAX || maxX <= minX) return;

    panel.x = minX;
    panel.y = maxY + 8;
    panel.w = maxX - minX;
    panel.h = 66;

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, 170);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, 200);
        SDL_RenderDrawRect(renderer, &panel);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 170);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 200);
        SDL_RenderDrawRect(renderer, &panel);
    }

    DrawTextBasic(renderer, font, "Input Root:", panel.x + 8, panel.y + 7, labelColor);
    DrawTextClipped(renderer,
                    font,
                    state && Global_GetInputRoot() ? Global_GetInputRoot() : "(unset)",
                    panel.x + 8,
                    panel.y + 22,
                    panel.w - 16,
                    valueColor);
    DrawTextBasic(renderer, font, "Output Root:", panel.x + 8, panel.y + 39, labelColor);
    DrawTextClipped(renderer,
                    font,
                    state && Global_GetOutputRoot() ? Global_GetOutputRoot() : "(unset)",
                    panel.x + 8,
                    panel.y + 54,
                    panel.w - 16,
                    valueColor);
}

void Render_UIPanel(const UIPanelState* ui, SDL_Renderer* renderer) {
    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        DrawButton(renderer, btn);
    }
    RenderRootSummary(ui, renderer);
}
