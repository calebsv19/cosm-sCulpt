#include "UI/ui_panel_overlay_render.h"

#include "UI/overlay/ui_panel_overlay_render_internal.h"

void UIPanel_RenderOverlayDialogs(SDL_Renderer* renderer, const UIPanelState* ui) {
    if (!renderer || !ui) return;
    UIPanelOverlay_RenderFileDialogs(renderer, ui);
    UIPanelOverlay_RenderEditDialogs(renderer, ui);
}
