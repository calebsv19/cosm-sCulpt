#include "UI/overlay/ui_panel_overlay_render_internal.h"

#include "UI/ui_panel_internal.h"
#include "UI/font_manager.h"
#include "UI/info_overlay.h"
#include "UI/shared_theme_font_adapter.h"

#include <stdio.h>
#include <string.h>

static void RenderPrismDimensionDialog(SDL_Renderer* renderer, const UIPanelState* ui) {
    const char* title = NULL;
    char subtitle[160];
    int width = 0;
    int height = 0;
    int text_x = 0;
    int text_y = 0;
    SDL_Rect backdrop = {0, 0, 0, 0};
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect input_rect = {0, 0, 0, 0};
    TTF_Font* font = NULL;
    char caret_buf[96];
    int caret_offset = 0;
    int caret_x = 0;
    int caret_top = 0;
    int caret_bottom = 0;
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

    text_x = panel.x + 16;
    text_y = panel.y + 14;
    UIPanelOverlay_DrawText(renderer,
                            title,
                            text_x,
                            text_y,
                            has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});
    UIPanelOverlay_DrawText(renderer,
                            subtitle,
                            text_x,
                            text_y + 22,
                            has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 190, 255});

    input_rect = (SDL_Rect){
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
    SDL_RenderFillRect(renderer, &input_rect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    }
    SDL_RenderDrawRect(renderer, &input_rect);
    UIPanelOverlay_DrawTextClipped(renderer,
                                   ui->prismDimensionDialog.buffer,
                                   input_rect.x + 8,
                                   input_rect.y + 8,
                                   input_rect.w - 16,
                                   has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255});

    font = FontManager_Get(FONT_DEFAULT);
    if (font) {
        size_t len = ui->prismDimensionDialog.cursor;
        if (len >= sizeof(caret_buf)) len = sizeof(caret_buf) - 1u;
        memcpy(caret_buf, ui->prismDimensionDialog.buffer, len);
        caret_buf[len] = '\0';
        if (TTF_SizeUTF8(font, caret_buf, &caret_offset, NULL) == 0) {
            caret_x = input_rect.x + 8 + caret_offset;
            if (caret_x > input_rect.x + input_rect.w - 8) caret_x = input_rect.x + input_rect.w - 8;
            caret_top = input_rect.y + 4;
            caret_bottom = input_rect.y + input_rect.h - 4;
            if (has_shared_palette) {
                SDL_SetRenderDrawColor(renderer,
                                       palette.text_primary.r, palette.text_primary.g,
                                       palette.text_primary.b, 220);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
            }
            SDL_RenderDrawLine(renderer, caret_x, caret_top, caret_x, caret_bottom);
        }
    }
}

static void RenderSceneBoundsDialog(SDL_Renderer* renderer, const UIPanelState* ui) {
    const char* title = NULL;
    char subtitle[192];
    int width = 0;
    int height = 0;
    int text_x = 0;
    int text_y = 0;
    SDL_Rect backdrop = {0, 0, 0, 0};
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect input_rect = {0, 0, 0, 0};
    TTF_Font* font = NULL;
    char caret_buf[160];
    int caret_offset = 0;
    int caret_x = 0;
    int caret_top = 0;
    int caret_bottom = 0;
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

    text_x = panel.x + 16;
    text_y = panel.y + 14;
    UIPanelOverlay_DrawText(renderer,
                            title,
                            text_x,
                            text_y,
                            has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});
    UIPanelOverlay_DrawText(renderer,
                            subtitle,
                            text_x,
                            text_y + 22,
                            has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 190, 255});

    input_rect = (SDL_Rect){
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
    SDL_RenderFillRect(renderer, &input_rect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    }
    SDL_RenderDrawRect(renderer, &input_rect);
    UIPanelOverlay_DrawTextClipped(renderer,
                                   ui->sceneBoundsDialog.buffer,
                                   input_rect.x + 8,
                                   input_rect.y + 8,
                                   input_rect.w - 16,
                                   has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255});

    font = FontManager_Get(FONT_DEFAULT);
    if (font) {
        size_t len = ui->sceneBoundsDialog.cursor;
        if (len >= sizeof(caret_buf)) len = sizeof(caret_buf) - 1u;
        memcpy(caret_buf, ui->sceneBoundsDialog.buffer, len);
        caret_buf[len] = '\0';
        if (TTF_SizeUTF8(font, caret_buf, &caret_offset, NULL) == 0) {
            caret_x = input_rect.x + 8 + caret_offset;
            if (caret_x > input_rect.x + input_rect.w - 8) caret_x = input_rect.x + input_rect.w - 8;
            caret_top = input_rect.y + 4;
            caret_bottom = input_rect.y + input_rect.h - 4;
            if (has_shared_palette) {
                SDL_SetRenderDrawColor(renderer,
                                       palette.text_primary.r, palette.text_primary.g,
                                       palette.text_primary.b, 220);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
            }
            SDL_RenderDrawLine(renderer, caret_x, caret_top, caret_x, caret_bottom);
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
    int text_x = 0;
    int text_y = 0;
    SDL_Rect backdrop = {0, 0, 0, 0};
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect input_rect = {0, 0, 0, 0};
    TTF_Font* font = NULL;
    char caret_buf[96];
    int caret_offset = 0;
    int caret_x = 0;
    int caret_top = 0;
    int caret_bottom = 0;
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

    text_x = panel.x + 16;
    text_y = panel.y + 14;
    UIPanelOverlay_DrawText(renderer,
                            title,
                            text_x,
                            text_y,
                            has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});
    UIPanelOverlay_DrawText(renderer,
                            subtitle,
                            text_x,
                            text_y + 22,
                            has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 190, 255});

    input_rect = (SDL_Rect){
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
    SDL_RenderFillRect(renderer, &input_rect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    }
    SDL_RenderDrawRect(renderer, &input_rect);
    UIPanelOverlay_DrawTextClipped(renderer,
                                   ui->constructionPlaneDialog.buffer,
                                   input_rect.x + 8,
                                   input_rect.y + 8,
                                   input_rect.w - 16,
                                   has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255});

    font = FontManager_Get(FONT_DEFAULT);
    if (font) {
        size_t len = ui->constructionPlaneDialog.cursor;
        if (len >= sizeof(caret_buf)) len = sizeof(caret_buf) - 1u;
        memcpy(caret_buf, ui->constructionPlaneDialog.buffer, len);
        caret_buf[len] = '\0';
        if (TTF_SizeUTF8(font, caret_buf, &caret_offset, NULL) == 0) {
            caret_x = input_rect.x + 8 + caret_offset;
            if (caret_x > input_rect.x + input_rect.w - 8) caret_x = input_rect.x + input_rect.w - 8;
            caret_top = input_rect.y + 4;
            caret_bottom = input_rect.y + input_rect.h - 4;
            if (has_shared_palette) {
                SDL_SetRenderDrawColor(renderer,
                                       palette.text_primary.r, palette.text_primary.g,
                                       palette.text_primary.b, 220);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
            }
            SDL_RenderDrawLine(renderer, caret_x, caret_top, caret_x, caret_bottom);
        }
    }
}

static void RenderObjectTransformDialog(SDL_Renderer* renderer, const UIPanelState* ui) {
    char title[96];
    char subtitle[208];
    int width = 0;
    int height = 0;
    int text_x = 0;
    int text_y = 0;
    SDL_Rect backdrop = {0, 0, 0, 0};
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect input_rect = {0, 0, 0, 0};
    TTF_Font* font = NULL;
    char caret_buf[160];
    int caret_offset = 0;
    int caret_x = 0;
    int caret_top = 0;
    int caret_bottom = 0;
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

    text_x = panel.x + 16;
    text_y = panel.y + 14;
    UIPanelOverlay_DrawText(renderer,
                            title,
                            text_x,
                            text_y,
                            has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});
    UIPanelOverlay_DrawText(renderer,
                            subtitle,
                            text_x,
                            text_y + 22,
                            has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 190, 255});

    input_rect = (SDL_Rect){
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
    SDL_RenderFillRect(renderer, &input_rect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    }
    SDL_RenderDrawRect(renderer, &input_rect);
    UIPanelOverlay_DrawTextClipped(renderer,
                                   ui->objectTransformDialog.buffer,
                                   input_rect.x + 8,
                                   input_rect.y + 8,
                                   input_rect.w - 16,
                                   has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255});

    font = FontManager_Get(FONT_DEFAULT);
    if (font) {
        size_t len = ui->objectTransformDialog.cursor;
        if (len >= sizeof(caret_buf)) len = sizeof(caret_buf) - 1u;
        memcpy(caret_buf, ui->objectTransformDialog.buffer, len);
        caret_buf[len] = '\0';
        if (TTF_SizeUTF8(font, caret_buf, &caret_offset, NULL) == 0) {
            caret_x = input_rect.x + 8 + caret_offset;
            if (caret_x > input_rect.x + input_rect.w - 8) caret_x = input_rect.x + input_rect.w - 8;
            caret_top = input_rect.y + 4;
            caret_bottom = input_rect.y + input_rect.h - 4;
            if (has_shared_palette) {
                SDL_SetRenderDrawColor(renderer,
                                       palette.text_primary.r, palette.text_primary.g,
                                       palette.text_primary.b, 220);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
            }
            SDL_RenderDrawLine(renderer, caret_x, caret_top, caret_x, caret_bottom);
        }
    }
}

void UIPanelOverlay_RenderEditDialogs(SDL_Renderer* renderer, const UIPanelState* ui) {
    RenderPrismDimensionDialog(renderer, ui);
    RenderSceneBoundsDialog(renderer, ui);
    RenderConstructionPlaneDialog(renderer, ui);
    RenderObjectTransformDialog(renderer, ui);
}
