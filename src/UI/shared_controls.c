#include "UI/shared_controls.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_visual_style.h"
#include "kit_ui_interaction_sdl.h"

static KitUiSurface menu;

KitUiSurface* LDControls_Menu(void) { return &menu; }
void LDControls_Cancel(KitUiSurface* surface) {
    KitUiInteractionEvent cancel = {.type=KIT_UI_INTERACTION_CANCEL};
    KitUiInteractionResult result;
    (void)kit_ui_surface_route(surface, &cancel, &result);
}
void LDControls_Add(KitUiSurface* surface, unsigned domain, uint64_t value,
                    SDL_Rect bounds, const SDL_Rect* clip, bool enabled) {
    if (bounds.w <= 0 || bounds.h <= 0) return;
    KitRenderRect rect = {(float)bounds.x, (float)bounds.y, (float)bounds.w, (float)bounds.h};
    KitRenderRect limit;
    if (clip) limit = (KitRenderRect){(float)clip->x, (float)clip->y, (float)clip->w, (float)clip->h};
    (void)kit_ui_surface_register(surface, (KitUiSurfaceKey){domain,value}, rect,
                                  clip ? &limit : NULL, enabled, NULL);
}
bool LDControls_Route(KitUiSurface* surface, const SDL_Event* event, SDL_Point* activation) {
    KitUiInteractionEvent input;
    KitUiInteractionResult result = {0};
    if (activation) *activation = (SDL_Point){-1,-1};
    if (!kit_ui_interaction_event_from_sdl(event, &input)) return false;
    if (kit_ui_surface_route(surface, &input, &result).code != CORE_OK) {
        LDControls_Cancel(surface);
        return true; /* Failed snapshots never fall through to press actions. */
    }
    if (result.activated_id && kit_ui_surface_take_activation(surface, result.activated_id)) {
        for (uint32_t i=0; i<surface->count; ++i) if (surface->controls[i].id == result.activated_id) {
            KitRenderRect b = surface->controls[i].bounds;
            if (activation) *activation = (SDL_Point){(int)(b.x+b.width/2), (int)(b.y+b.height/2)};
            break;
        }
    }
    return result.consumed != 0;
}
void LDControls_Draw(SDL_Renderer* renderer, const KitUiSurface* surface) {
    UIPanelVisualPalette palette = {0};
    (void)UIPanelVisual_ResolvePalette(&palette);
    if (!renderer || !surface) return;
    for (uint32_t i=0; i<surface->count; ++i) {
        const KitUiInteractionControl* control = &surface->controls[i];
        KitUiButtonState state = kit_ui_interaction_button_state(&surface->interaction, control, 0);
        if (!state.pressed && surface->interaction.focused_id != control->id) continue;
        KitRenderRect b = control->bounds;
        SDL_Rect rect = {(int)b.x+1, (int)b.y+1, (int)b.width-2, (int)b.height-2};
        SDL_Color color = palette.text_primary;
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        if (state.pressed) { rect.y += rect.h-3; rect.h = 2; SDL_RenderFillRect(renderer, &rect); }
        else SDL_RenderDrawRect(renderer, &rect);
    }
}
