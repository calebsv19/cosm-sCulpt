#include "UI/font_manager.h"
#include "UI/shared_overlay.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/ui_panel.h"
#include "UI/workspace_authoring/line_drawing_workspace_authoring_host.h"
#include "kit_workspace_authoring_ui.h"
#include <stdint.h>
static KitUiSurface authoring;
static const GlobalState *owner;
void LDOverlay_Cancel(void) { LDControls_Cancel(&authoring); }
bool LDOverlay_Blocked(void) {
    return UIPanel_IsSaveDialogActive() || UIPanel_IsRootDialogActive() ||
           UIPanel_IsPrismDimensionDialogActive() || UIPanel_IsSceneBoundsDialogActive() ||
           UIPanel_IsConstructionPlaneDialogActive() || UIPanel_IsObjectTransformDialogActive() ||
           UIPanel_IsScenePropertyDialogActive() || LDViewControls_ActivePopup() ||
           UIPanel_Get()->contextMenuOpen || LineDrawingWorkspaceAuthoringHost_Active(Global_Get());
}
static SDL_Rect rect(float x, float y, float w, float h) {
    return (SDL_Rect){(int)x, (int)y, (int)w, (int)h};
}
static SDL_Rect font_rect(const KitWorkspaceAuthoringFontThemeLayout *l, int id) {
    if (id == 1)
        return rect(l->text_size_dec_button.x, l->text_size_dec_button.y,
                    l->text_size_dec_button.width, l->text_size_dec_button.height);
    if (id == 2)
        return rect(l->text_size_inc_button.x, l->text_size_inc_button.y,
                    l->text_size_inc_button.width, l->text_size_inc_button.height);
    if (id == 3)
        return rect(l->text_size_reset_button.x, l->text_size_reset_button.y,
                    l->text_size_reset_button.width, l->text_size_reset_button.height);
    KitRenderRect b = id <= 6    ? l->font_preset_buttons[id - 4]
                      : id <= 11 ? l->theme_preset_buttons[id - 7]
                                 : l->custom_theme_buttons[id - 12];
    return rect(b.x, b.y, b.width, b.height);
}
static bool collect(const GlobalState *s) {
    if (owner != s) {
        LDOverlay_Cancel();
        owner = s;
    }
    unsigned scope = LineDrawingWorkspaceAuthoringHost_Active(s)
                         ? 1u + (unsigned)s->screenWidth * 65537u +
                               (unsigned)s->screenHeight * 17u +
                               (unsigned)s->workspaceAuthoring.overlay_mode
                         : 0u;
    kit_ui_surface_begin(&authoring, scope);
    SDL_Rect screen = {0, 0, s->screenWidth, s->screenHeight};
    if (scope) {
        if (LineDrawingWorkspaceAuthoringHost_FontThemeOverlayActive(s)) {
            KitWorkspaceAuthoringFontThemeLayout l = {0};
            if (LDOverlay_FontThemeLayout(s, &l))
                for (int id = 1; id <= 13; ++id)
                    LDControls_Add(&authoring, 41, (uint64_t)id, font_rect(&l, id), &screen,
                                   kit_workspace_authoring_ui_font_theme_button_enabled(
                                       (KitWorkspaceAuthoringFontThemeButtonId)id));
        }
        KitWorkspaceAuthoringOverlayButton buttons[4];
        uint32_t n = kit_workspace_authoring_ui_build_overlay_buttons(
            s->screenWidth, 1, LineDrawingWorkspaceAuthoringHost_PaneOverlayActive(s), buttons, 4);
        for (uint32_t i = 0; i < n; ++i)
            if (buttons[i].visible) {
                CorePaneRect b = buttons[i].rect;
                LDControls_Add(&authoring, 40, buttons[i].id, rect(b.x, b.y, b.width, b.height),
                               &screen, buttons[i].enabled);
            }
    }
    return kit_ui_surface_end(&authoring).code == CORE_OK;
}
bool LDOverlay_AuthoringEvent(GlobalState *s, const SDL_Event *event) {
    if (!s || !event)
        return false;
    if (!collect(s)) {
        LDOverlay_Cancel();
        return true;
    }
    if (!LineDrawingWorkspaceAuthoringHost_Active(s))
        return false;
    SDL_Event mapped = *event;
    if ((event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) &&
        (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_RIGHT)) {
        mapped.key.keysym.mod = event->key.keysym.sym == SDLK_LEFT ? KMOD_SHIFT : KMOD_NONE;
        mapped.key.keysym.sym = SDLK_TAB;
    } else if ((event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) &&
               event->key.keysym.sym != SDLK_SPACE)
        return false;
    SDL_Point activate;
    bool consumed = LDControls_Route(&authoring, &mapped, &activate);
    if (activate.x >= 0) {
        (void)LineDrawingWorkspaceAuthoringHost_Click(s, activate.x, activate.y);
        return true;
    }
    if (LineDrawingWorkspaceAuthoringHost_FontThemeOverlayActive(s) &&
        (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP ||
         event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEWHEEL))
        return true;
    return consumed;
}
void LDOverlay_AuthoringDraw(SDL_Renderer *renderer, const GlobalState *s) {
    if (s && collect(s))
        LDControls_Draw(renderer, &authoring);
}

bool LDOverlay_FontThemeLayout(const GlobalState *s, KitWorkspaceAuthoringFontThemeLayout *out) {
    if (!s || !out)
        return false;
    KitRenderContext metrics = {0};
    if (kit_render_context_init(&metrics, KIT_RENDER_BACKEND_NULL, CORE_THEME_PRESET_DARK_DEFAULT, FontManager_GetSharedFontPresetId()).code != CORE_OK)
        return false;
    (void)kit_render_set_font_preset(&metrics, FontManager_GetSharedFontPresetId());
    (void)kit_render_set_text_zoom_step(&metrics, FontManager_GetZoomStep());
    bool ok = kit_workspace_authoring_ui_font_theme_build_layout(&metrics, s->screenWidth,
                                                                 s->screenHeight, out) != 0;
    kit_render_context_shutdown(&metrics);
    return ok;
}
