#include "UI/ui_panel_overlay_render.h"

#include "UI/ui_panel_internal.h"
#include "UI/font_manager.h"
#include "UI/info_overlay.h"
#include "UI/shared_theme_font_adapter.h"
#include "Render/vulkan_adapter.h"

#include <string.h>
#include <stdio.h>
static void DrawText(SDL_Renderer* renderer, const char* text, int x, int y, SDL_Color color) {
    if (!text || !*text) return;
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

    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
#endif
}

static void DrawTextClipped(SDL_Renderer* renderer,
                            const char* text,
                            int x,
                            int y,
                            int max_width,
                            SDL_Color color) {
    TTF_Font* font = NULL;
    static const char* k_ellipsis = "...";
    char clipped[512];
    size_t len = 0;
    int text_w = 0;
    int ellipsis_w = 0;

    if (!text || !text[0] || max_width <= 0) return;
    font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;

    if (TTF_SizeUTF8(font, text, &text_w, NULL) == 0 && text_w <= max_width) {
        DrawText(renderer, text, x, y, color);
        return;
    }

    if (TTF_SizeUTF8(font, k_ellipsis, &ellipsis_w, NULL) != 0 || ellipsis_w >= max_width) {
        return;
    }

    len = strlen(text);
    while (len > 0) {
        --len;
        if (len + strlen(k_ellipsis) + 1 >= sizeof(clipped)) continue;
        memcpy(clipped, text, len);
        clipped[len] = '\0';
        strcat(clipped, k_ellipsis);
        if (TTF_SizeUTF8(font, clipped, &text_w, NULL) == 0 && text_w <= max_width) {
            DrawText(renderer, clipped, x, y, color);
            return;
        }
    }
}

static void RenderSaveDialog(SDL_Renderer* renderer, const UIPanelState* ui) {
    if (!ui->saveDialog.active) return;
    LineDrawing3dThemePalette palette = {0};
    const bool has_shared_palette = line_drawing3d_shared_theme_resolve_palette(&palette);

    int width = Global_GetScreenWidth();
    int height = Global_GetScreenHeight();

    SDL_Rect backdrop = { 0, 0, width, height };
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.modal_scrim.r, palette.modal_scrim.g,
                               palette.modal_scrim.b, palette.modal_scrim.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
    }
    SDL_RenderFillRect(renderer, &backdrop);

    SDL_Rect panel = {
        width / 2 - 220,
        InfoOverlay_HeightPx() + 20,
        440,
        130
    };

    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 35, 40, 48, 240);
    }
    SDL_RenderFillRect(renderer, &panel);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 255);
    }
    SDL_RenderDrawRect(renderer, &panel);

    int textX = panel.x + 16;
    int textY = panel.y + 16;
    char line[256];

    snprintf(line, sizeof(line), "Save layout as (*.json):");
    DrawText(renderer, line, textX, textY,
             has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});

    SDL_Rect inputRect = {
        panel.x + 14,
        panel.y + 48,
        panel.w - 28,
        32
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.background_fill.r, palette.background_fill.g,
                               palette.background_fill.b, palette.background_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    }
    SDL_RenderFillRect(renderer, &inputRect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    }
    SDL_RenderDrawRect(renderer, &inputRect);

    char buffer[200];
    snprintf(buffer, sizeof(buffer), "%s", ui->saveDialog.buffer);
    DrawTextClipped(renderer,
                    buffer,
                    inputRect.x + 8,
                    inputRect.y + 6,
                    inputRect.w - 16,
                    has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255});

    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    if (font) {
        char caretBuf[128];
        size_t len = ui->saveDialog.cursor;
        if (len > sizeof(caretBuf) - 1) len = sizeof(caretBuf) - 1;
        memcpy(caretBuf, ui->saveDialog.buffer, len);
        caretBuf[len] = '\0';
        int caretOffset = 0;
        TTF_SizeUTF8(font, caretBuf, &caretOffset, NULL);
        int caretX = inputRect.x + 8 + caretOffset;
        if (caretX > inputRect.x + inputRect.w - 8) caretX = inputRect.x + inputRect.w - 8;
        int caretTop = inputRect.y + 4;
        int caretBottom = inputRect.y + inputRect.h - 4;
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.text_primary.r, palette.text_primary.g,
                                   palette.text_primary.b, 220);
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
        }
        SDL_RenderDrawLine(renderer, caretX, caretTop, caretX, caretBottom);
    }

    snprintf(line, sizeof(line), "Press Enter to confirm, Esc to cancel.");
    DrawText(renderer, line, textX, panel.y + panel.h - 36,
             has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 190, 255});
}

static void RenderRootDialog(SDL_Renderer* renderer, const UIPanelState* ui) {
    const char* title = NULL;
    const char* description = "Enter to apply, Esc to cancel.";
    int width = 0;
    int height = 0;
    int textX = 0;
    int textY = 0;
    SDL_Rect backdrop = {0, 0, 0, 0};
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect inputRect = {0, 0, 0, 0};
    TTF_Font* font = NULL;
    char caretBuf[256];
    int caretOffset = 0;
    int caretX = 0;
    int caretTop = 0;
    int caretBottom = 0;
    LineDrawing3dThemePalette palette = {0};
    const bool has_shared_palette = line_drawing3d_shared_theme_resolve_palette(&palette);

    if (!ui->rootDialog.active) return;

    switch (ui->rootDialog.target) {
        case UI_ROOT_TARGET_INPUT:
            title = "Set Input Root";
            break;
        case UI_ROOT_TARGET_OUTPUT:
            title = "Set Output Root";
            break;
        default:
            title = "Set Root";
            break;
    }

    width = Global_GetScreenWidth();
    height = Global_GetScreenHeight();
    backdrop = (SDL_Rect){0, 0, width, height};
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.modal_scrim.r, palette.modal_scrim.g,
                               palette.modal_scrim.b, palette.modal_scrim.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    }
    SDL_RenderFillRect(renderer, &backdrop);

    panel = (SDL_Rect){
        width / 2 - 300,
        InfoOverlay_HeightPx() + 20,
        600,
        154
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 35, 40, 48, 240);
    }
    SDL_RenderFillRect(renderer, &panel);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 255);
    }
    SDL_RenderDrawRect(renderer, &panel);

    textX = panel.x + 16;
    textY = panel.y + 14;
    DrawText(renderer,
             title,
             textX,
             textY,
             has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});
    DrawText(renderer,
             description,
             textX,
             textY + 22,
             has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 190, 255});

    inputRect = (SDL_Rect){
        panel.x + 14,
        panel.y + 58,
        panel.w - 28,
        34
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.background_fill.r, palette.background_fill.g,
                               palette.background_fill.b, palette.background_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    }
    SDL_RenderFillRect(renderer, &inputRect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    }
    SDL_RenderDrawRect(renderer, &inputRect);
    DrawTextClipped(renderer,
                    ui->rootDialog.buffer,
                    inputRect.x + 8,
                    inputRect.y + 8,
                    inputRect.w - 16,
                    has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255});

    font = FontManager_Get(FONT_DEFAULT);
    if (font) {
        size_t len = ui->rootDialog.cursor;
        if (len >= sizeof(caretBuf)) len = sizeof(caretBuf) - 1;
        memcpy(caretBuf, ui->rootDialog.buffer, len);
        caretBuf[len] = '\0';
        if (TTF_SizeUTF8(font, caretBuf, &caretOffset, NULL) == 0) {
            caretX = inputRect.x + 8 + caretOffset;
            if (caretX > inputRect.x + inputRect.w - 8) caretX = inputRect.x + inputRect.w - 8;
            caretTop = inputRect.y + 4;
            caretBottom = inputRect.y + inputRect.h - 4;
            if (has_shared_palette) {
                SDL_SetRenderDrawColor(renderer,
                                       palette.text_primary.r, palette.text_primary.g,
                                       palette.text_primary.b, 220);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
            }
            SDL_RenderDrawLine(renderer, caretX, caretTop, caretX, caretBottom);
        }
    }
}

static void RenderPrismDimensionDialog(SDL_Renderer* renderer, const UIPanelState* ui) {
    const char* title = NULL;
    char subtitle[160];
    int width = 0;
    int height = 0;
    int textX = 0;
    int textY = 0;
    SDL_Rect backdrop = {0, 0, 0, 0};
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect inputRect = {0, 0, 0, 0};
    TTF_Font* font = NULL;
    char caretBuf[96];
    int caretOffset = 0;
    int caretX = 0;
    int caretTop = 0;
    int caretBottom = 0;
    LineDrawing3dThemePalette palette = {0};
    const bool has_shared_palette = line_drawing3d_shared_theme_resolve_palette(&palette);

    if (!ui->prismDimensionDialog.active) return;

    title = UIPanel_PrismDimensionTargetLabel(ui->prismDimensionDialog.target);
    snprintf(subtitle,
             sizeof(subtitle),
             "Object #%u value in %s. Enter applies; Esc cancels.",
             ui->prismDimensionDialog.objectId,
             UIPanel_GetDisplayUnitSymbol());

    width = Global_GetScreenWidth();
    height = Global_GetScreenHeight();
    backdrop = (SDL_Rect){0, 0, width, height};
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.modal_scrim.r, palette.modal_scrim.g,
                               palette.modal_scrim.b, palette.modal_scrim.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    }
    SDL_RenderFillRect(renderer, &backdrop);

    panel = (SDL_Rect){
        width / 2 - 250,
        InfoOverlay_HeightPx() + 20,
        500,
        146
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 35, 40, 48, 240);
    }
    SDL_RenderFillRect(renderer, &panel);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 255);
    }
    SDL_RenderDrawRect(renderer, &panel);

    textX = panel.x + 16;
    textY = panel.y + 14;
    DrawText(renderer,
             title,
             textX,
             textY,
             has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});
    DrawText(renderer,
             subtitle,
             textX,
             textY + 22,
             has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 190, 255});

    inputRect = (SDL_Rect){
        panel.x + 14,
        panel.y + 58,
        panel.w - 28,
        34
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.background_fill.r, palette.background_fill.g,
                               palette.background_fill.b, palette.background_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    }
    SDL_RenderFillRect(renderer, &inputRect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    }
    SDL_RenderDrawRect(renderer, &inputRect);
    DrawTextClipped(renderer,
                    ui->prismDimensionDialog.buffer,
                    inputRect.x + 8,
                    inputRect.y + 8,
                    inputRect.w - 16,
                    has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255});

    font = FontManager_Get(FONT_DEFAULT);
    if (font) {
        size_t len = ui->prismDimensionDialog.cursor;
        if (len >= sizeof(caretBuf)) len = sizeof(caretBuf) - 1;
        memcpy(caretBuf, ui->prismDimensionDialog.buffer, len);
        caretBuf[len] = '\0';
        if (TTF_SizeUTF8(font, caretBuf, &caretOffset, NULL) == 0) {
            caretX = inputRect.x + 8 + caretOffset;
            if (caretX > inputRect.x + inputRect.w - 8) caretX = inputRect.x + inputRect.w - 8;
            caretTop = inputRect.y + 4;
            caretBottom = inputRect.y + inputRect.h - 4;
            if (has_shared_palette) {
                SDL_SetRenderDrawColor(renderer,
                                       palette.text_primary.r, palette.text_primary.g,
                                       palette.text_primary.b, 220);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
            }
            SDL_RenderDrawLine(renderer, caretX, caretTop, caretX, caretBottom);
        }
    }
}

static void RenderSceneBoundsDialog(SDL_Renderer* renderer, const UIPanelState* ui) {
    const char* title = NULL;
    char subtitle[192];
    int width = 0;
    int height = 0;
    int textX = 0;
    int textY = 0;
    SDL_Rect backdrop = {0, 0, 0, 0};
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect inputRect = {0, 0, 0, 0};
    TTF_Font* font = NULL;
    char caretBuf[160];
    int caretOffset = 0;
    int caretX = 0;
    int caretTop = 0;
    int caretBottom = 0;
    LineDrawing3dThemePalette palette = {0};
    const bool has_shared_palette = line_drawing3d_shared_theme_resolve_palette(&palette);

    if (!ui->sceneBoundsDialog.active) return;

    title = UIPanel_SceneBoundsTargetLabel(ui->sceneBoundsDialog.target);
    snprintf(subtitle,
             sizeof(subtitle),
             "Enter x, y, z in %s. Enter applies; Esc cancels.",
             UIPanel_GetDisplayUnitSymbol());

    width = Global_GetScreenWidth();
    height = Global_GetScreenHeight();
    backdrop = (SDL_Rect){0, 0, width, height};
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.modal_scrim.r, palette.modal_scrim.g,
                               palette.modal_scrim.b, palette.modal_scrim.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    }
    SDL_RenderFillRect(renderer, &backdrop);

    panel = (SDL_Rect){
        width / 2 - 260,
        InfoOverlay_HeightPx() + 20,
        520,
        146
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 35, 40, 48, 240);
    }
    SDL_RenderFillRect(renderer, &panel);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 255);
    }
    SDL_RenderDrawRect(renderer, &panel);

    textX = panel.x + 16;
    textY = panel.y + 14;
    DrawText(renderer,
             title,
             textX,
             textY,
             has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});
    DrawText(renderer,
             subtitle,
             textX,
             textY + 22,
             has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 190, 255});

    inputRect = (SDL_Rect){
        panel.x + 14,
        panel.y + 58,
        panel.w - 28,
        34
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.background_fill.r, palette.background_fill.g,
                               palette.background_fill.b, palette.background_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    }
    SDL_RenderFillRect(renderer, &inputRect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    }
    SDL_RenderDrawRect(renderer, &inputRect);
    DrawTextClipped(renderer,
                    ui->sceneBoundsDialog.buffer,
                    inputRect.x + 8,
                    inputRect.y + 8,
                    inputRect.w - 16,
                    has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255});

    font = FontManager_Get(FONT_DEFAULT);
    if (font) {
        size_t len = ui->sceneBoundsDialog.cursor;
        if (len >= sizeof(caretBuf)) len = sizeof(caretBuf) - 1;
        memcpy(caretBuf, ui->sceneBoundsDialog.buffer, len);
        caretBuf[len] = '\0';
        if (TTF_SizeUTF8(font, caretBuf, &caretOffset, NULL) == 0) {
            caretX = inputRect.x + 8 + caretOffset;
            if (caretX > inputRect.x + inputRect.w - 8) caretX = inputRect.x + inputRect.w - 8;
            caretTop = inputRect.y + 4;
            caretBottom = inputRect.y + inputRect.h - 4;
            if (has_shared_palette) {
                SDL_SetRenderDrawColor(renderer,
                                       palette.text_primary.r, palette.text_primary.g,
                                       palette.text_primary.b, 220);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
            }
            SDL_RenderDrawLine(renderer, caretX, caretTop, caretX, caretBottom);
        }
    }
}

static void RenderConstructionPlaneDialog(SDL_Renderer* renderer, const UIPanelState* ui) {
    const GlobalState* state = Global_Get();
    const ViewPlane plane = UIPanel_CurrentConstructionViewPlane(state);
    char title[96];
    char subtitle[192];
    int width = 0;
    int height = 0;
    int textX = 0;
    int textY = 0;
    SDL_Rect backdrop = {0, 0, 0, 0};
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect inputRect = {0, 0, 0, 0};
    TTF_Font* font = NULL;
    char caretBuf[96];
    int caretOffset = 0;
    int caretX = 0;
    int caretTop = 0;
    int caretBottom = 0;
    LineDrawing3dThemePalette palette = {0};
    const bool has_shared_palette = line_drawing3d_shared_theme_resolve_palette(&palette);

    if (!ui->constructionPlaneDialog.active) return;

    snprintf(title,
             sizeof(title),
             "Construction Plane Offset (%s)",
             UIPanel_ViewPlaneAxisLabel(plane.axis));
    snprintf(subtitle,
             sizeof(subtitle),
             "Enter %s offset in %s. Enter applies; Esc cancels.",
             UIPanel_ViewPlaneCoordinateLabel(plane.axis),
             UIPanel_GetDisplayUnitSymbol());

    width = Global_GetScreenWidth();
    height = Global_GetScreenHeight();
    backdrop = (SDL_Rect){0, 0, width, height};
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.modal_scrim.r, palette.modal_scrim.g,
                               palette.modal_scrim.b, palette.modal_scrim.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    }
    SDL_RenderFillRect(renderer, &backdrop);

    panel = (SDL_Rect){
        width / 2 - 240,
        InfoOverlay_HeightPx() + 20,
        480,
        146
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 35, 40, 48, 240);
    }
    SDL_RenderFillRect(renderer, &panel);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 255);
    }
    SDL_RenderDrawRect(renderer, &panel);

    textX = panel.x + 16;
    textY = panel.y + 14;
    DrawText(renderer,
             title,
             textX,
             textY,
             has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});
    DrawText(renderer,
             subtitle,
             textX,
             textY + 22,
             has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 190, 255});

    inputRect = (SDL_Rect){
        panel.x + 14,
        panel.y + 58,
        panel.w - 28,
        34
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.background_fill.r, palette.background_fill.g,
                               palette.background_fill.b, palette.background_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    }
    SDL_RenderFillRect(renderer, &inputRect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    }
    SDL_RenderDrawRect(renderer, &inputRect);
    DrawTextClipped(renderer,
                    ui->constructionPlaneDialog.buffer,
                    inputRect.x + 8,
                    inputRect.y + 8,
                    inputRect.w - 16,
                    has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255});

    font = FontManager_Get(FONT_DEFAULT);
    if (font) {
        size_t len = ui->constructionPlaneDialog.cursor;
        if (len >= sizeof(caretBuf)) len = sizeof(caretBuf) - 1;
        memcpy(caretBuf, ui->constructionPlaneDialog.buffer, len);
        caretBuf[len] = '\0';
        if (TTF_SizeUTF8(font, caretBuf, &caretOffset, NULL) == 0) {
            caretX = inputRect.x + 8 + caretOffset;
            if (caretX > inputRect.x + inputRect.w - 8) caretX = inputRect.x + inputRect.w - 8;
            caretTop = inputRect.y + 4;
            caretBottom = inputRect.y + inputRect.h - 4;
            if (has_shared_palette) {
                SDL_SetRenderDrawColor(renderer,
                                       palette.text_primary.r, palette.text_primary.g,
                                       palette.text_primary.b, 220);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
            }
            SDL_RenderDrawLine(renderer, caretX, caretTop, caretX, caretBottom);
        }
    }
}

static void RenderObjectTransformDialog(SDL_Renderer* renderer, const UIPanelState* ui) {
    char title[96];
    char subtitle[208];
    int width = 0;
    int height = 0;
    int textX = 0;
    int textY = 0;
    SDL_Rect backdrop = {0, 0, 0, 0};
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect inputRect = {0, 0, 0, 0};
    TTF_Font* font = NULL;
    char caretBuf[160];
    int caretOffset = 0;
    int caretX = 0;
    int caretTop = 0;
    int caretBottom = 0;
    LineDrawing3dThemePalette palette = {0};
    const bool has_shared_palette = line_drawing3d_shared_theme_resolve_palette(&palette);

    if (!ui->objectTransformDialog.active) return;

    snprintf(title,
             sizeof(title),
             "%s",
             UIPanel_ObjectTransformTargetLabel(ui->objectTransformDialog.target));
    if (ui->objectTransformDialog.target == UI_OBJECT_TRANSFORM_DIALOG_TARGET_POSITION) {
        snprintf(subtitle,
                 sizeof(subtitle),
                 "Object #%u position in %s. Enter x, y, z; Enter applies; Esc cancels.",
                 ui->objectTransformDialog.objectId,
                 UIPanel_GetDisplayUnitSymbol());
    } else {
        snprintf(subtitle,
                 sizeof(subtitle),
                 "Object #%u absolute world-axis rotation in degrees. Enter applies; Esc cancels.",
                 ui->objectTransformDialog.objectId);
    }

    width = Global_GetScreenWidth();
    height = Global_GetScreenHeight();
    backdrop = (SDL_Rect){0, 0, width, height};
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.modal_scrim.r, palette.modal_scrim.g,
                               palette.modal_scrim.b, palette.modal_scrim.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    }
    SDL_RenderFillRect(renderer, &backdrop);

    panel = (SDL_Rect){
        width / 2 - 280,
        InfoOverlay_HeightPx() + 20,
        560,
        146
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 35, 40, 48, 240);
    }
    SDL_RenderFillRect(renderer, &panel);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 255);
    }
    SDL_RenderDrawRect(renderer, &panel);

    textX = panel.x + 16;
    textY = panel.y + 14;
    DrawText(renderer,
             title,
             textX,
             textY,
             has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});
    DrawText(renderer,
             subtitle,
             textX,
             textY + 22,
             has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 190, 255});

    inputRect = (SDL_Rect){
        panel.x + 14,
        panel.y + 58,
        panel.w - 28,
        34
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.background_fill.r, palette.background_fill.g,
                               palette.background_fill.b, palette.background_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    }
    SDL_RenderFillRect(renderer, &inputRect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    }
    SDL_RenderDrawRect(renderer, &inputRect);
    DrawTextClipped(renderer,
                    ui->objectTransformDialog.buffer,
                    inputRect.x + 8,
                    inputRect.y + 8,
                    inputRect.w - 16,
                    has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255});

    font = FontManager_Get(FONT_DEFAULT);
    if (font) {
        size_t len = ui->objectTransformDialog.cursor;
        if (len >= sizeof(caretBuf)) len = sizeof(caretBuf) - 1;
        memcpy(caretBuf, ui->objectTransformDialog.buffer, len);
        caretBuf[len] = '\0';
        if (TTF_SizeUTF8(font, caretBuf, &caretOffset, NULL) == 0) {
            caretX = inputRect.x + 8 + caretOffset;
            if (caretX > inputRect.x + inputRect.w - 8) caretX = inputRect.x + inputRect.w - 8;
            caretTop = inputRect.y + 4;
            caretBottom = inputRect.y + inputRect.h - 4;
            if (has_shared_palette) {
                SDL_SetRenderDrawColor(renderer,
                                       palette.text_primary.r, palette.text_primary.g,
                                       palette.text_primary.b, 220);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
            }
            SDL_RenderDrawLine(renderer, caretX, caretTop, caretX, caretBottom);
        }
    }
}

static void RenderLoadMenu(SDL_Renderer* renderer, const UIPanelState* ui) {
    if (!ui->loadMenu.open) return;
    LineDrawing3dThemePalette palette = {0};
    const bool has_shared_palette = line_drawing3d_shared_theme_resolve_palette(&palette);

    SDL_Rect rect = UIPanel_GetLoadMenuRect(ui);
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 28, 32, 40, 240);
    }
    SDL_RenderFillRect(renderer, &rect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 255);
    }
    SDL_RenderDrawRect(renderer, &rect);

    int itemHeight = 24;
    int y = rect.y + 6;

    if (ui->loadMenu.count == 0) {
        DrawText(renderer, "(No layouts found)", rect.x + 10, y,
                 has_shared_palette ? palette.text_muted : (SDL_Color){190, 190, 195, 255});
        return;
    }

    for (int i = 0; i < ui->loadMenu.count; ++i) {
        if (i == ui->loadMenu.hoverIndex) {
            SDL_Rect highlight = { rect.x + 2, y - 2, rect.w - 4, itemHeight };
            if (has_shared_palette) {
                SDL_SetRenderDrawColor(renderer,
                                       palette.menu_highlight.r, palette.menu_highlight.g,
                                       palette.menu_highlight.b, palette.menu_highlight.a);
            } else {
                SDL_SetRenderDrawColor(renderer, 60, 90, 140, 180);
            }
            SDL_RenderFillRect(renderer, &highlight);
        }
        DrawTextClipped(renderer,
                        ui->loadMenu.entries[i],
                        rect.x + 10,
                        y + 2,
                        rect.w - 20,
                        has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});
        y += itemHeight;
    }
}


void UIPanel_RenderOverlayDialogs(SDL_Renderer* renderer, const UIPanelState* ui) {
    RenderLoadMenu(renderer, ui);
    RenderSaveDialog(renderer, ui);
    RenderRootDialog(renderer, ui);
    RenderPrismDimensionDialog(renderer, ui);
    RenderSceneBoundsDialog(renderer, ui);
    RenderConstructionPlaneDialog(renderer, ui);
    RenderObjectTransformDialog(renderer, ui);
}
