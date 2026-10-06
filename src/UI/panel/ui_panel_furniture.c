#include "UI/ui_panel_furniture.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/font_manager.h"
#include "Core/global_state.h"
#include "Layout/layout_furniture.h"
#include "Editor/editor_numeric_edit.h"
#include <stdio.h>
#include <string.h>

/* Bounded form state, never authoritative geometry. Selection/history changes discard drafts. */
static struct {
    LayoutFurnitureUnit observed;
    char fields[7][64], message[192];
    bool valid, open, panels, replace, edited[7];
    int input, keep;
} form;
static bool active(void) {
    return Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_SCENE &&
        UIPanel_Get()->activeRightTab == UI_PANEL_RIGHT_TAB_OBJECT;
}
static void length(double meters, char* text, size_t n) {
    double value = meters;
    (void)core_units_convert(meters, CORE_UNIT_METER, UIPanel_GetDisplayUnit(), &value);
    snprintf(text, n, "%.6g %s", value, UIPanel_GetDisplayUnitSymbol());
}
void UIPanel_FurnitureReset(void) {
    if (form.input) SDL_StopTextInput();
    memset(&form, 0, sizeof(form));
}
static void sync(void) {
    const LayoutFurnitureUnit* unit = active() ? Layout_FurnitureForObject(&Global_Get()->layout.objectStore,
        Global_Get()->editor.selectedObject3DId) : NULL;
    if (!unit) { UIPanel_FurnitureReset(); return; }
    if (form.valid && !memcmp(unit, &form.observed, sizeof(*unit))) return;
    bool reopen = form.valid && !strcmp(form.observed.assembly_id, unit->assembly_id) && form.open;
    bool panels = form.panels; int keep = form.keep;
    UIPanel_FurnitureReset();
    form.valid = true; form.open = reopen; form.keep = reopen ? keep : 1; form.panels = reopen && panels;
    form.observed = *unit;
    length(unit->size_m[1], form.fields[0], 64);
    length(unit->size_m[0], form.fields[1], 64);
    length(unit->size_m[2], form.fields[2], 64);
    for (int k = 0; k < 4; ++k) length(unit->thickness_m[k], form.fields[k + 3], 64);
}
static bool has_role(LayoutFurnitureRole role) {
    for (size_t i = 0; i < form.observed.part_count; ++i)
        if (form.observed.parts[i].role == role) return true;
    return false;
}
static bool thickness_present(int field_index) {
    return field_index != 5 && field_index != 6 ? true :
        has_role(field_index == 5 ? LAYOUT_FURNITURE_DOOR : LAYOUT_FURNITURE_WORKTOP);
}
static int thickness_count(void) {
    return 2 + (int)thickness_present(5) + (int)thickness_present(6);
}
bool UIPanel_FurnitureEditing(void) { sync(); return form.valid && form.open; }
static int h(void) { TTF_Font* f = FontManager_Get(FONT_DEFAULT); return (f ? TTF_FontHeight(f) : 16) + 10; }
int UIPanel_FurnitureHeight(void) {
    sync();
    if (!form.valid) return 0;
    return h() * (form.open ? 10 + (form.panels ? thickness_count() : 0) : 1) + 12;
}
static SDL_Rect rectangle(int action) {
    sync();
    if (!form.valid) return (SDL_Rect){0};
    SDL_Rect b = UIPanel_Get()->objectPane.furnitureRect;
    int row = 0, col = 0, cols = 1;
    if (action != UI_FURNITURE_OPEN && !form.open) return (SDL_Rect){0};
    switch (action) {
    case UI_FURNITURE_OPEN: row = 0; break;
    case UI_FURNITURE_LENGTH: case UI_FURNITURE_DEPTH: case UI_FURNITURE_HEIGHT:
        row = action - UI_FURNITURE_LENGTH + 2; break;
    case UI_FURNITURE_KEEP: row = 5; break;
    case UI_FURNITURE_PANELS: row = 7; break;
    case UI_FURNITURE_CASE: case UI_FURNITURE_BACK: case UI_FURNITURE_DOOR: case UI_FURNITURE_WORKTOP:
        if (!form.panels) return (SDL_Rect){0};
        {
            int k = 3 + action - UI_FURNITURE_CASE;
            if (!thickness_present(k)) return (SDL_Rect){0};
            row = 8;
            for (int i = 3; i < k; ++i) row += (int)thickness_present(i);
        }
        break;
    case UI_FURNITURE_APPLY: case UI_FURNITURE_CANCEL:
        row = 8 + (form.panels ? thickness_count() : 0); cols = 2; col = action - UI_FURNITURE_APPLY; break;
    default: return (SDL_Rect){0};
    }
    int width = (b.w - 16 - 4 * (cols - 1)) / cols;
    return (SDL_Rect){b.x + 6 + col * (width + 4), b.y + 5 + row * h(), width, h() - 4};
}
bool UIPanel_FurnitureControlRect(int action, SDL_Rect* rect) {
    if (!active() || !rect) return false;
    *rect = rectangle(action); return rect->w > 0 && rect->h > 0;
}
static void paint(SDL_Renderer* r, SDL_Rect box, const char* text, bool button, bool selected) {
    if (box.w <= 0 || box.h <= 0) return;
    UIPanelVisualPalette p = {0}; (void)UIPanelVisual_ResolvePalette(&p);
    if (button) UIPanelVisual_DrawFrame(r, box, selected ? p.button_fill_active : p.button_fill,
                                       selected ? p.accent : p.button_border, 0);
    TTF_Font* f = FontManager_Get(FONT_DEFAULT);
    if (f) UIPanelSummary_DrawTextClipped(r, f, text, box.x + 5, box.y + 3, box.w - 10,
                                         box.h - 5, p.text_primary);
}
static int field(int action) {
    if (action >= UI_FURNITURE_LENGTH && action <= UI_FURNITURE_HEIGHT) return action - UI_FURNITURE_LENGTH;
    if (action >= UI_FURNITURE_CASE && action <= UI_FURNITURE_WORKTOP) return 3 + action - UI_FURNITURE_CASE;
    return -1;
}
void UIPanel_RenderFurniture(SDL_Renderer* renderer) {
    if (!renderer || !active()) return;
    sync(); if (!form.valid) return;
    const LayoutAssembly* a = Layout_FindAssembly(&Global_Get()->layout.objectStore, form.observed.assembly_id);
    char text[256];
    snprintf(text, sizeof(text), "%s: %.180s", form.open ? "Unit" : "Edit unit", a ? a->info.label : form.observed.assembly_id);
    paint(renderer, rectangle(UI_FURNITURE_OPEN), text, true, form.open);
    if (!form.open) return;
    SDL_Rect body = UIPanel_Get()->objectPane.furnitureRect;
    SDL_Rect note = {body.x + 6, body.y + 5 + h(), body.w - 16, h() - 4};
    paint(renderer, note, "Wall face and floor stay fixed", false, false);
    const char* labels[] = {"Length", "Depth", "Height", "Case", "Back", "Door", "Worktop"};
    for (int action = UI_FURNITURE_LENGTH; action <= UI_FURNITURE_WORKTOP; ++action) {
        int k = field(action); if (k < 0) continue;
        SDL_Rect row = rectangle(action); if (row.w <= 0) continue;
        int label_w = row.w / 3;
        SDL_Rect value = {row.x + label_w, row.y, row.w - label_w, row.h};
        row.w = label_w - 4;
        paint(renderer, row, labels[k], false, false);
        snprintf(text, sizeof(text), "%s%s", form.fields[k], form.input == k + 1 ? " |" : "");
        paint(renderer, value, text, true, form.input == k + 1);
    }
    snprintf(text, sizeof(text), "Length anchor: %s", form.keep == 1 ? "Front end" : form.keep == -1 ? "Rear end" : "Center");
    paint(renderer, rectangle(UI_FURNITURE_KEEP), text, true, false);
    bool closed = false;
    for (size_t i = 0; i < form.observed.part_count; ++i) closed |= form.observed.parts[i].role == LAYOUT_FURNITURE_DOOR;
    char d[40], l[40], z[40];
    length(form.observed.size_m[0] - form.observed.thickness_m[1] - (closed ? form.observed.thickness_m[2] : 0), d, sizeof(d));
    length(form.observed.size_m[1] - 2 * form.observed.thickness_m[0], l, sizeof(l));
    length(form.observed.size_m[2] - 2 * form.observed.thickness_m[0], z, sizeof(z));
    snprintf(text, sizeof(text), "Current inside L/D/H: %s / %s / %s", l, d, z);
    note.y = body.y + 5 + 6 * h(); paint(renderer, note, text, false, false);
    paint(renderer, rectangle(UI_FURNITURE_PANELS), form.panels ? "Panels -" : "Panels +", true, form.panels);
    paint(renderer, rectangle(UI_FURNITURE_APPLY), "Apply unit", true, false);
    paint(renderer, rectangle(UI_FURNITURE_CANCEL), "Cancel / back", true, false);
    note.y = body.y + 5 + (9 + (form.panels ? thickness_count() : 0)) * h();
    paint(renderer, note, form.message[0] ? form.message : "One edit; fixed-size sink; adjoining units stay put", false, false);
}
static bool contains(SDL_Rect r, int x, int y) {
    return r.w > 0 && x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}
static void refresh(void) {
    UIPanel_OnWindowResized(Global_GetScreenWidth(), Global_GetScreenHeight());
    Global_FlagGridChanged(); Global_FlagHitboxesDirty();
}
static void apply(void) {
    LayoutFurnitureUnit next = form.observed;
    for (int k = 0; k < 7; ++k) {
        if (!form.edited[k]) continue;
        double v;
        if (!Editor_ParseLength(form.fields[k], UIPanel_GetDisplayUnit(), &v) || v <= 0) {
            snprintf(form.message, sizeof(form.message), "Enter a positive length in every field."); return;
        }
        if (k < 3) next.size_m[k == 0 ? 1 : k == 1 ? 0 : 2] = v;
        else next.thickness_m[k - 3] = v;
    }
    if (!memcmp(&next, &form.observed, sizeof(next))) {
        snprintf(form.message, sizeof(form.message), "Dimensions unchanged."); return;
    }
    if (!Layout_EditFurnitureUnit(&Global_Get()->layout, &next, form.keep, Layout_GeometryHistory, NULL)) {
        snprintf(form.message, sizeof(form.message), "%.190s", Global_Get()->layout.geometryMessage); return;
    }
    sync(); snprintf(form.message, sizeof(form.message), "Unit updated. Undo restores all parts.");
}
bool UIPanel_FurnitureClick(int x, int y) {
    if (!active() || !contains(UIPanel_Get()->rightBodyRect, x, y)) return false;
    sync(); if (!form.valid || !contains(UIPanel_Get()->objectPane.furnitureRect, x, y)) return false;
    for (int a = UI_FURNITURE_OPEN; a <= UI_FURNITURE_CANCEL; ++a) {
        if (!contains(rectangle(a), x, y)) continue;
        int k = field(a);
        if (k >= 0) { form.input = k + 1; form.replace = true; SDL_StartTextInput(); }
        else if (a == UI_FURNITURE_OPEN) {
            if (form.open) UIPanel_FurnitureReset();
            else form.open = true;
            form.input = 0; SDL_StopTextInput();
        }
        else if (a == UI_FURNITURE_KEEP) form.keep = form.keep == 1 ? 0 : form.keep == 0 ? -1 : 1;
        else if (a == UI_FURNITURE_PANELS) { form.panels = !form.panels; form.input = 0; SDL_StopTextInput(); }
        else if (a == UI_FURNITURE_APPLY) { form.input = 0; SDL_StopTextInput(); apply(); }
        else if (a == UI_FURNITURE_CANCEL) UIPanel_FurnitureReset();
        refresh(); return true;
    }
    return true;
}
bool UIPanel_FurnitureEvent(const SDL_Event* event) {
    if (!event) return false;
    sync();
    if (!form.valid || !form.open) return false;
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
        return UIPanel_FurnitureClick(event->button.x, event->button.y);
    if (!form.input) return false;
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        form.input = 0; SDL_StopTextInput(); return false;
    }
    char* text = form.fields[form.input - 1];
    if (event->type == SDL_TEXTINPUT) {
        form.edited[form.input - 1] = true;
        if (form.replace) { text[0] = 0; form.replace = false; }
        size_t n = strlen(text); snprintf(text + n, 64 - n, "%s", event->text.text);
        form.message[0] = 0; Global_FlagGridChanged(); return true;
    }
    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) { form.input = 0; SDL_StopTextInput(); apply(); }
        else if (key == SDLK_ESCAPE) UIPanel_FurnitureReset();
        else if (key == SDLK_a && event->key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) form.replace = true;
        else if (key == SDLK_BACKSPACE) {
            form.edited[form.input - 1] = true;
            size_t n = strlen(text);
            if (form.replace) text[0] = 0;
            else if (n) text[n - 1] = 0;
            form.replace = false;
        } else if (key == SDLK_TAB) { form.input = form.input % 3 + 1; form.replace = true; }
        refresh(); return true;
    }
    return false;
}
