#include "Core/global_state.h"
#include "UI/font_manager.h"
#include "UI/shared_special.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_measurement.h"
#include "UI/ui_panel_visual_style.h"
#include <string.h>
#include <stdio.h>
static LDTextField state;
static char *owner;
static SDL_Rect previous;
void LDInline_Cancel(void) { LDText_CancelCapture(&state); }
static LDInlineField current(void) {
    LDInlineField f = {0};
    UIPanelState *u = UIPanel_Get();
    if (LDViewControls_ActivePopup() == LD_VIEW_VISIBILITY) {
        (void)LDInline_Visibility(&f);
        return f;
    }
    if(LDViewControls_ActivePopup() || u->contextMenuOpen)return f;
    if (UIPanel_IsSaveDialogActive() || UIPanel_IsRootDialogActive() ||
        UIPanel_IsPrismDimensionDialogActive() || UIPanel_IsSceneBoundsDialogActive() ||
        UIPanel_IsConstructionPlaneDialogActive() || UIPanel_IsObjectTransformDialogActive() ||
        UIPanel_IsScenePropertyDialogActive())
        return f;
    if (LDInline_Path(&f) || LDInline_Camera(&f) || LDInline_Section(&f) ||
        LDInline_Furniture(&f) || LDInline_Parts(&f) || LDInline_Routes(&f) ||
        LDInline_Inspection(&f))
        return f;
    int p = u->measurement.placing;
    if (u->activeRightTab == UI_PANEL_RIGHT_TAB_MEASURE && u->measurement.active &&
        (p == 1 || p == 3 || p == 5 || p >= 7)) {
        int action = p == 13   ? MEASURE_GRID
                     : p >= 10 ? MEASURE_TRAVEL_MIN + p - 10
                     : p >= 7  ? MEASURE_OFFSET_U + p - 7
                               : MEASURE_VALUE;
        SDL_Rect box;
        if (UIPanel_MeasurementControlRect(action, &box))
            f = (LDInlineField){u->measurement.placement_text,
                                sizeof(u->measurement.placement_text), &u->measurement.replace_text,
                                box, NULL};
    }
    return f;
}
static void bind(LDInlineField f) {
    if (owner != f.text) {
        memset(&state, 0, sizeof(state));
        owner = f.text;
    }
    LDText_Bind(&state, f.text, f.capacity,
                state.edit.text == f.text ? state.edit.cursor : strlen(f.text));
    if (f.replace && *f.replace) {
        (void)kit_ui_text_position(&state.edit, strlen(f.text), 0);
        *f.replace = false;
        state.expected_cursor = state.edit.cursor;
    }
    if (memcmp(&previous, &f.rect, sizeof(previous))) {
        LDText_CancelCapture(&state);
        previous = f.rect;
    }
}
bool LDInline_Event(const SDL_Event *event) {
    LDInlineField f = current();
    if (!f.text) {
        owner = NULL;
        LDInline_Cancel();
        return false;
    }
    bind(f);
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_TAB && f.edited && !state.edit.composition[0]) {
        owner = NULL;
        LDInline_Cancel();
        return false;
    }
    bool numeric = f.text == UIPanel_Get()->measurement.placement_text;
    if (numeric && event->type == SDL_TEXTINPUT) {
        for (const unsigned char *p = (const unsigned char *)event->text.text; *p; ++p)
            if (*p < 32 || *p > 126)
                return true;
    }
    if (numeric && event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_v &&
        (event->key.keysym.mod & (KMOD_CTRL | KMOD_GUI))) {
        if (event->key.repeat || state.edit.composition[0])
            return true;
        char *text = SDL_GetClipboardText();
        if (!text)
            return true;
        bool valid = true;
        for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
            if (*p < 32 || *p > 126)
                valid = false;
        if (valid)
            (void)kit_ui_text_insert(&state.edit, text);
        SDL_free(text);
        state.fingerprint = 0;
        return true;
    }
    KitUiTextEventResult result = LDText_Event(&state, event);
    if(result.status.code!=CORE_OK) {
        UIPanelState* u=UIPanel_Get();
        if(u->activeRightTab==UI_PANEL_RIGHT_TAB_PARTS)snprintf(u->parts.message,sizeof(u->parts.message),"Text exceeds field capacity or contains invalid text.");
        if(u->activeRightTab==UI_PANEL_RIGHT_TAB_ROUTES)snprintf(u->routes.message,sizeof(u->routes.message),"Text exceeds field capacity or contains invalid text.");
    }
    if (result.changed) {
        if (f.edited)
            *f.edited = true;
        LDInline_VisibilityChanged();
        Global_FlagGridChanged();
    }
    /* A committed field remains a draft until its existing owner handles the
       submit/cancel intent. Preedit Enter/Escape is consumed by shared editing. */
    if (result.submit || result.cancel) {
        owner = NULL;
        LDInline_Cancel();
        return false;
    }
    return result.consumed || event->type == SDL_KEYDOWN || event->type == SDL_KEYUP;
}
void LDInline_Draw(SDL_Renderer *renderer) {
    LDInlineField f = current();
    if (!f.text || !renderer)
        return;
    bind(f);
    SDL_Rect area = {f.rect.x + 7, f.rect.y + 3, f.rect.w - 14, f.rect.h - 6};
    SDL_Rect clip = UIPanel_Get()->rightBodyRect;
    if (LDViewControls_ActivePopup() == LD_VIEW_VISIBILITY)
        clip = f.rect;
    SDL_Rect old, visible;
    SDL_bool clipped = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &old);
    if (!SDL_IntersectRect(&area, &clip, &visible))
        return;
    if (clipped && !SDL_IntersectRect(&visible, &old, &visible))
        return;
    UIPanelVisualPalette p = {0};
    (void)UIPanelVisual_ResolvePalette(&p);
#if USE_VULKAN
    float old_color[4];
    memcpy(old_color, renderer->draw_state.current_color, sizeof(old_color));
#else
    Uint8 red, green, blue, alpha;
    SDL_GetRenderDrawColor(renderer, &red, &green, &blue, &alpha);
#endif
    SDL_RenderSetClipRect(renderer, &visible);
    SDL_SetRenderDrawColor(renderer, p.button_fill.r, p.button_fill.g, p.button_fill.b, 255);
    SDL_RenderFillRect(renderer, &area);
    LDText_Draw(&state, renderer, FontManager_Get(FONT_DEFAULT), area, p.text_primary, true);
    SDL_RenderSetClipRect(renderer, clipped ? &old : NULL);
#if USE_VULKAN
    vk_renderer_set_draw_color(renderer, old_color[0], old_color[1], old_color[2], old_color[3]);
#else
    SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
#endif
}

bool LDInline_Active(void) { return current().text != NULL; }
SDL_Rect LDInline_ValueRect(SDL_Rect box, const char *prefix) {
    int w = (int)strlen(prefix) * 8;
    TTF_Font *font = FontManager_Get(FONT_DEFAULT);
    if (font)
        (void)TTF_SizeUTF8(font, prefix, &w, NULL);
    box.x += w + 2;
    box.w -= w + 2;
    return box;
}

const KitUiTextPresentation *LDInline_Presentation(void) {
    return state.ready ? &state.view : NULL;
}
