#include "UI/overlay/ui_panel_overlay_render_internal.h"

#include "Core/global_state.h"
#include "UI/info_overlay.h"
#include "UI/shared_theme_font_adapter.h"

void UIPanelOverlay_RenderScenePropertyDialog(SDL_Renderer* renderer, const UIPanelState* ui) {
    LineDrawing3dThemePalette palette = {0};
    SDL_Rect backdrop = {0, 0, Global_GetScreenWidth(), Global_GetScreenHeight()};
    SDL_Rect panel = {backdrop.w / 2 - 280, InfoOverlay_HeightPx() + 24, 560, 176};
    SDL_Rect input_rect = {panel.x + 14, panel.y + 76, panel.w - 28, 34};
    const bool themed = line_drawing3d_shared_theme_resolve_palette(&palette);
    if (!renderer || !ui || !ui->scenePropertyDialog.active) return;
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    SDL_SetRenderDrawColor(renderer, themed ? palette.modal_scrim.r : 0,
                           themed ? palette.modal_scrim.g : 0,
                           themed ? palette.modal_scrim.b : 0,
                           themed ? palette.modal_scrim.a : 160);
    SDL_RenderFillRect(renderer, &backdrop);
    SDL_SetRenderDrawColor(renderer, themed ? palette.panel_fill.r : 35,
                           themed ? palette.panel_fill.g : 40,
                           themed ? palette.panel_fill.b : 48, 245);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, themed ? palette.panel_border.r : 100,
                           themed ? palette.panel_border.g : 110,
                           themed ? palette.panel_border.b : 125, 255);
    SDL_RenderDrawRect(renderer, &panel);
    UIPanelOverlay_DrawText(renderer, UIPanel_ScenePropertyDialogTitle(ui->scenePropertyDialog.target),
                            panel.x + 16, panel.y + 14,
                            themed ? palette.text_primary : (SDL_Color){235,235,240,255});
    UIPanelOverlay_DrawTextClipped(renderer, UIPanel_ScenePropertyDialogHint(ui->scenePropertyDialog.target),
                                   panel.x + 16, panel.y + 42, panel.w - 32,
                                   themed ? palette.text_muted : (SDL_Color){190,190,200,255});
    SDL_SetRenderDrawColor(renderer, themed ? palette.background_fill.r : 20,
                           themed ? palette.background_fill.g : 20,
                           themed ? palette.background_fill.b : 24, 255);
    SDL_RenderFillRect(renderer, &input_rect);
    SDL_SetRenderDrawColor(renderer, themed ? palette.panel_border.r : 130,
                           themed ? palette.panel_border.g : 140,
                           themed ? palette.panel_border.b : 155, 255);
    SDL_RenderDrawRect(renderer, &input_rect);
    UIPanelOverlay_DrawTextClipped(renderer, ui->scenePropertyDialog.buffer,
                                   input_rect.x + 8, input_rect.y + 8, input_rect.w - 16,
                                   themed ? palette.text_primary : (SDL_Color){255,255,255,255});
    if (ui->scenePropertyDialog.validationMessage[0]) {
        UIPanelOverlay_DrawTextClipped(renderer, ui->scenePropertyDialog.validationMessage,
                                       panel.x + 16, panel.y + 122, panel.w - 32,
                                       (SDL_Color){240,120,110,255});
    } else {
        UIPanelOverlay_DrawText(renderer, "Enter applies one undoable edit. Esc cancels.",
                                panel.x + 16, panel.y + 122,
                                themed ? palette.text_muted : (SDL_Color){180,180,190,255});
    }
}
