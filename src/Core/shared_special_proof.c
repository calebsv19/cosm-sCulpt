#include "Core/shared_special_proof.h"
#include "Core/global_state.h"
#include "Input/input_handler.h"
#include "UI/panel/ui_panel_visibility.h"
#include "UI/shared_special.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_parts.h"
#include "UI/ui_panel_routes.h"
#include "UI/ui_panel_section.h"
#include "UI/ui_panel_shell.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include <stdio.h>
#include <string.h>
static size_t undo_before;
static bool click(SDL_Rect box) {
    if (box.w <= 0 || box.h <= 0)
        return false;
    SDL_Event e = {.type = SDL_MOUSEBUTTONDOWN};
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = box.x + box.w / 2;
    e.button.y = box.y + box.h / 2;
    Input_Handle(NULL, &e);
    e.type = SDL_MOUSEBUTTONUP;
    Input_Handle(NULL, &e);
    return true;
}
bool SharedSpecialProof_Stage(const char *mode) {
    if (!mode || strncmp(mode, "shared-special-", 15))
        return true;
    LineDrawingEditorTopbar_UpdateControls();
    SDL_Rect box;
    GlobalState *s = Global_Get();
    UIPanelState *u = UIPanel_Get();
    undo_before = Editor_UndoCount(&s->editor);
    if (!strcmp(mode, "shared-special-parts")) {
        UIPanel_PartsEnterMode(1);
        if (!UIPanel_PartsControlRect(PARTS_NEW, &box) || !click(box))
            return false;
        if (!UIPanel_PartsControlRect(PARTS_NAME, &box) || !click(box))
            return false;
    } else if (!strcmp(mode, "shared-special-routes")) {
        UIPanel_SetActiveRightTab(u, UI_PANEL_RIGHT_TAB_ROUTES);
        UIPanel_LayoutRoutes();
        if (!UIPanel_RoutesControlRect(ROUTES_NEW, &box) || !click(box))
            return false;
        if (!UIPanel_RoutesControlRect(ROUTES_NAME, &box) || !click(box))
            return false;
    } else if (!strcmp(mode, "shared-special-section")) {
        UIPanel_SetActiveRightTab(u, UI_PANEL_RIGHT_TAB_VIEW);
        UIPanel_OnWindowResized(s->screenWidth, s->screenHeight);
        if (!UIPanel_SectionControlRect(UI_SECTION_EXACT, &box) || !click(box))
            return false;
        if (!UIPanel_SectionControlRect(UI_SECTION_POSITION, &box) || !click(box))
            return false;
    } else if (!strcmp(mode, "shared-special-visibility")) {
        if (!LDViewControls_Rect(LD_VIEW_VISIBILITY, &box) || !click(box))
            return false;
        if (!UIPanel_VisibilityRect(LD_VIEW_SEARCH, &box) || !click(box))
            return false;
    } else
        return false;
    SDL_Event e = {.type = SDL_TEXTINPUT};
    const char *input =
        !strcmp(mode, "shared-special-section")
            ? "1250 mm"
            : "café-very-long-unsaved-name-café-very-long-unsaved-name-café-very-long-unsaved-name";
    /* SDL_TEXTINPUT messages are at most 31 bytes; keep UTF-8 boundaries. */
    for (const char *p = input; *p;) {
        size_t n = strlen(p);
        if (n > 28)
            n = 28;
        while (((unsigned char)p[n] & 0xc0) == 0x80)
            --n;
        memcpy(e.text.text, p, n);
        e.text.text[n] = 0;
        Input_Handle(NULL, &e);
        p += n;
    }
    if (strcmp(mode, "shared-special-section")) {
        e = (SDL_Event){.type = SDL_TEXTEDITING};
        strcpy(e.edit.text, "é");
        e.edit.length = 1;
        Input_Handle(NULL, &e);
    }
    return LDInline_Active() && Editor_UndoCount(&s->editor) == undo_before;
}
bool SharedSpecialProof_Check(const char *mode) {
    if (!mode || strncmp(mode, "shared-special-", 15))
        return true;
    const KitUiTextPresentation *v = LDInline_Presentation();
    bool ok =
        v && v->count == 1 && v->options.active && v->caret.x >= v->options.viewport.x &&
        v->caret.x + v->caret.width <= v->options.viewport.x + v->options.viewport.width + 0.01f &&
        Editor_UndoCount(&Global_Get()->editor) == undo_before;
    if (strcmp(mode, "shared-special-section"))
        ok = ok && v->composition_bytes > 0;
    else ok=ok && v && v->scroll_x==0;
    fprintf(
        stdout,
        "SHARED_SPECIAL mode=%s status=%s scroll_x=%.2f preedit_bytes=%zu unchanged_history=%d\n",
        mode, ok ? "pass" : "fail", v ? v->scroll_x : 0, v ? v->composition_bytes : 0,
        Editor_UndoCount(&Global_Get()->editor) == undo_before);
    return ok;
}
