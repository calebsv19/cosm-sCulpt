#include "UI/ui_panel_section.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/font_manager.h"
#include "Core/global_state.h"
#include "Core/viewport_zoom.h"
#include "Layout/layout_engineering.h"
#include "Layout/layout_saved_views.h"
#include "Editor/editor_numeric_edit.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool contains(SDL_Rect r, int x, int y) {
    return r.w > 0 && r.h > 0 && x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}
static int row_height(void) {
    TTF_Font* f = FontManager_Get(FONT_DEFAULT);
    return (f ? TTF_FontHeight(f) : 16) + 8;
}
int UIPanel_SectionHeight(void) {
    UIPanelState* ui = UIPanel_Get();
    bool on = Global_Get() && Global_Get()->sectionView.mode != LAYOUT_SECTION_OFF;
    int rows = on ? 7 : 3;
    if (on && ui->sectionInput) ++rows;
    if (ui->sectionHelpOpen || ui->sectionMessage[0]) rows += 3;
    return row_height() * rows + 8;
}
static bool active(void) {
    return Global_Get() && !Global_Get()->cameraView.active && Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_SCENE &&
           UIPanel_Get()->activeRightTab == UI_PANEL_RIGHT_TAB_VIEW;
}
static SDL_Rect rect_for(int action) {
    UIPanelState* ui = UIPanel_Get();
    SDL_Rect body = ui->viewPane.workspaceRect;
    int h = row_height(), row = 0, col = 0, cols = 1;
    bool on = Global_Get()->sectionView.mode != LAYOUT_SECTION_OFF;
    switch (action) {
    case UI_SECTION_HELP:
        row = 0;
        break;
    case UI_SECTION_OFF:
    case UI_SECTION_EXACT:
    case UI_SECTION_CUTAWAY:
        row = 1;
        cols = 3;
        col = action - UI_SECTION_OFF;
        break;
    case UI_SECTION_ALONG:
    case UI_SECTION_ACROSS:
    case UI_SECTION_HEIGHT:
        row = 2;
        cols = 3;
        col = action == UI_SECTION_ACROSS ? 0 : action == UI_SECTION_ALONG ? 1 : 2;
        break;
    case UI_SECTION_TRACK:
        row = 3;
        break;
    case UI_SECTION_POSITION:
    case UI_SECTION_STEP:
        row = 4;
        cols = 2;
        col = action - UI_SECTION_POSITION;
        break;
    case UI_SECTION_PREVIOUS:
    case UI_SECTION_NEXT:
    case UI_SECTION_FLIP:
        row = 5;
        cols = 3;
        col = action - UI_SECTION_PREVIOUS;
        break;
    case UI_SECTION_APPLY:
    case UI_SECTION_CANCEL:
        row = 6;
        cols = 2;
        col = action - UI_SECTION_APPLY;
        break;
    case UI_SECTION_SELECTED:
    case UI_SECTION_ALL:
        row = on ? 6 + (ui->sectionInput != 0) : 2;
        cols = 2;
        col = action - UI_SECTION_SELECTED;
        break;
    default:
        return (SDL_Rect){0};
    }
    if (!on && action >= UI_SECTION_ALONG && action <= UI_SECTION_CANCEL) return (SDL_Rect){0};
    if ((action == UI_SECTION_APPLY || action == UI_SECTION_CANCEL) && !ui->sectionInput) return (SDL_Rect){0};
    if (action == UI_SECTION_FLIP && Global_Get()->sectionView.mode != LAYOUT_SECTION_CUTAWAY) return (SDL_Rect){0};
    int width = (body.w - 16 - 4 * (cols - 1)) / cols;
    return (SDL_Rect){body.x + 4 + col * (width + 4), body.y + 4 + row * h, width, h - 3};
}
bool UIPanel_SectionControlRect(int action, SDL_Rect* rect) {
    if (!active() || !rect)
        return false;
    *rect = rect_for(action);
    return rect->w > 0 && rect->h > 0;
}
static void format_length(double meters, char* text, size_t n) {
    double value = meters;
    (void)core_units_convert(meters, CORE_UNIT_METER, UIPanel_GetDisplayUnit(), &value);
    snprintf(text, n, "%.6g %s", value, UIPanel_GetDisplayUnitSymbol());
}
static void paint(SDL_Renderer* r, SDL_Rect box, const char* text, bool button, bool selected, bool enabled) {
    if (box.w <= 0 || box.h <= 0) return;
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    UIPanelVisualPalette p = {0};
    (void)UIPanelVisual_ResolvePalette(&p);
    if (button)
        UIPanelVisual_DrawFrame(r, box, selected ? p.button_fill_active : p.button_fill,
                                selected ? p.accent : p.button_border, 0);
    if (font)
        UIPanelSummary_DrawTextClipped(r, font, text, box.x + 4, box.y + 3, box.w - 8, box.h - 4,
                                       enabled ? p.text_primary : p.text_muted);
}
void UIPanel_RenderSection(SDL_Renderer* renderer) {
    if (!renderer || !active())
        return;
    GlobalState* state = Global_Get();
    UIPanelState* ui = UIPanel_Get();
    LayoutSectionView* s = &state->sectionView;
    SDL_Rect body = ui->viewPane.workspaceRect;
    char heading[96];
    snprintf(heading, sizeof(heading), "Section%s  %s", ui->sectionHelpOpen ? " - help" : " + help",
             s->mode == LAYOUT_SECTION_OFF ? "" : (const char*[]){"YZ / X", "XZ / Y", "XY / Z"}[s->axis]);
    paint(renderer, rect_for(UI_SECTION_HELP), heading, true, ui->sectionHelpOpen, true);
    const char* names[] = {"Off", "Slice", "Cutaway", "Y", "X", "Z"};
    for (int a = UI_SECTION_OFF; a <= UI_SECTION_HEIGHT; ++a) {
        bool selected = a <= UI_SECTION_CUTAWAY ? (int)s->mode == a - UI_SECTION_OFF
                                                : s->axis == (a == UI_SECTION_ALONG    ? 1
                                                              : a == UI_SECTION_ACROSS ? 0
                                                                                       : 2);
        paint(renderer, rect_for(a), names[a - 1], true, selected, true);
    }
    double lo = 0, hi = 1;
    bool range = Layout_SectionRange(&state->layout, s->axis, &lo, &hi);
    SDL_Rect track = rect_for(UI_SECTION_TRACK);
    UIPanelVisualPalette palette = {0};
    (void)UIPanelVisual_ResolvePalette(&palette);
    if (track.w > 0) {
        UIPanelVisual_DrawFrame(renderer, track, palette.workspace_fill, palette.button_border, 0);
        SDL_Rect rail = {track.x + 10, track.y + track.h / 2 - 2, track.w - 20, 4};
        SDL_SetRenderDrawColor(renderer, 110, 125, 145, 255);
        SDL_RenderFillRect(renderer, &rail);
        double t = range ? fmax(0, fmin(1, (s->position_meters - lo) / (hi - lo))) : 0;
        SDL_Rect thumb = {rail.x + (int)(t * rail.w) - 5, track.y + 4, 10, track.h - 8};
        SDL_SetRenderDrawColor(renderer, 100, 195, 235, 255);
        SDL_RenderFillRect(renderer, &thumb);
    }
    char value[96], label[160];
    for (int a = UI_SECTION_POSITION; a <= UI_SECTION_STEP; ++a) {
        int field = a == UI_SECTION_POSITION ? 1 : 2;
        format_length(field == 1 ? s->position_meters : s->step_meters, value, sizeof(value));
        snprintf(label, sizeof(label), "%s: %s%s", field == 1 ? "Position" : "Step",
                 ui->sectionInput == field ? ui->sectionText : value, ui->sectionInput == field ? " |" : "");
        paint(renderer, rect_for(a), label, true, ui->sectionInput == field, true);
    }
    paint(renderer, rect_for(UI_SECTION_PREVIOUS), "Previous", true, false, range);
    paint(renderer, rect_for(UI_SECTION_NEXT), "Next", true, false, range);
    paint(renderer, rect_for(UI_SECTION_FLIP), s->flipped ? "Keep + side" : "Keep - side", true, s->flipped,
          s->mode == LAYOUT_SECTION_CUTAWAY);
    if (ui->sectionInput) {
        paint(renderer, rect_for(UI_SECTION_APPLY), "Apply", true, true, true);
        paint(renderer, rect_for(UI_SECTION_CANCEL), "Cancel", true, false, true);
    }
    const Object3D* selected =
        Layout_ObjectStore_FindConst(&state->layout.objectStore, state->editor.selectedObject3DId);
    paint(renderer, rect_for(UI_SECTION_SELECTED), "Focus assembly", true, false, selected != NULL);
    paint(renderer, rect_for(UI_SECTION_ALL), "Show all / fit", true, false, true);
    if (ui->sectionHelpOpen || ui->sectionMessage[0]) {
        int row = (s->mode == LAYOUT_SECTION_OFF ? 3 : 7 + (ui->sectionInput != 0));
        SDL_Rect note = {body.x + 4, body.y + 4 + row * row_height(), body.w - 16, row_height() * 2};
        TTF_Font* font = FontManager_Get(FONT_DEFAULT);
        if (font) {
            char left[48], right[48], limits[128];
            format_length(lo, left, sizeof(left));
            format_length(hi, right, sizeof(right));
            snprintf(limits, sizeof(limits), "Range: %s to %s", left, right);
            UIPanelSummary_DrawTextClipped(renderer, font, limits, note.x + 4, note.y + 2,
                                           note.w - 8, TTF_FontHeight(font), palette.text_muted);
            UIPanelSummary_DrawWrappedText(renderer, font,
                ui->sectionMessage[0] ? ui->sectionMessage :
                "X/Y/Z: travel direction. Native solids; mesh cuts uncapped. View only.",
                note.x + 4, note.y + row_height(), note.w - 8, TTF_FontHeight(font), 2, 2,
                palette.text_muted);
        }
    }

}
static void refresh(void) {
    Global_FlagGridChanged();
    Global_FlagHitboxesDirty();
}
static void orient(void) {
    GlobalState* state = Global_Get();
    state->freeViewCamera.enabled = false;
    state->editor.viewportTool = VIEWPORT_TOOL_SELECT;
    (void)Global_SetSpaceMode(SPACE_MODE_3D, false);
    (void)Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_MATERIAL);
    (void)LineDrawingViewportZoom_FitVisibleGeometry(state);
}
static void scrub(int x) {
    GlobalState* state = Global_Get();
    double lo, hi;
    if (!Layout_SectionRange(&state->layout, state->sectionView.axis, &lo, &hi))
        return;
    SDL_Rect r = rect_for(UI_SECTION_TRACK);
    double t = fmax(0, fmin(1, (double)(x - r.x - 10) / (r.w - 20)));
    state->sectionView.position_meters = lo + t * (hi - lo);
    refresh();
}
bool UIPanel_SectionClick(int x, int y) {
    if (!active() || !contains(UIPanel_Get()->viewPane.workspaceRect, x, y) ||
        !contains(UIPanel_Get()->rightBodyRect, x, y))
        return false;
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    if (contains(rect_for(UI_SECTION_HELP), x, y)) {
        ui->sectionHelpOpen = !ui->sectionHelpOpen;
        UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
        return true;
    }
    LayoutSectionView* s = &state->sectionView;
    int action = 0;
    for (int a = 1; a <= UI_SECTION_ALL; ++a)
        if (contains(rect_for(a), x, y))
            action = a;
    double lo = 0, hi = 0;
    bool range = Layout_SectionRange(&state->layout, s->axis, &lo, &hi);
    ui->sectionMessage[0] = 0;
    if (!ui->sectionInput && (action == UI_SECTION_APPLY || action == UI_SECTION_CANCEL))
        return true;
    if (action >= UI_SECTION_OFF && action <= UI_SECTION_CUTAWAY) {
        ui->sectionInput = 0;
        LayoutSectionMode previous = s->mode;
        s->mode = (LayoutSectionMode)(action - UI_SECTION_OFF);
        /* Orthographic depth looks from the negative side: initially remove its near half. */
        if (s->mode == LAYOUT_SECTION_CUTAWAY && previous != LAYOUT_SECTION_CUTAWAY)
            s->flipped = true;
        if (s->mode != LAYOUT_SECTION_OFF)
            orient();
    } else if (action >= UI_SECTION_ALONG && action <= UI_SECTION_HEIGHT) {
        ui->sectionInput = 0;
        s->axis = action == UI_SECTION_ALONG ? 1 : action == UI_SECTION_ACROSS ? 0 : 2;
        if (Layout_SectionRange(&state->layout, s->axis, &lo, &hi))
            s->position_meters = (lo + hi) * .5;
        if (s->mode != LAYOUT_SECTION_OFF)
            orient();
    } else if (action == UI_SECTION_TRACK && range) {
        ui->sectionInput = 0;
        ui->sectionDragging = true;
        scrub(x);
    } else if (action == UI_SECTION_POSITION || action == UI_SECTION_STEP) {
        ui->sectionInput = action == UI_SECTION_POSITION ? 1 : 2;
        format_length(ui->sectionInput == 1 ? s->position_meters : s->step_meters, ui->sectionText,
                      sizeof(ui->sectionText));
        ui->sectionReplace = true;
        SDL_StartTextInput();
    } else if (action == UI_SECTION_CANCEL) {
        ui->sectionInput = 0;
        SDL_StopTextInput();
    } else if (action == UI_SECTION_APPLY && ui->sectionInput) {
        double meters;
        if (!Editor_ParseLength(ui->sectionText, UIPanel_GetDisplayUnit(), &meters) ||
            (ui->sectionInput == 2 && meters <= 0) || fabs(meters) > 1e6)
            snprintf(ui->sectionMessage, sizeof(ui->sectionMessage),
                     "Enter a finite length; step must be positive.");
        else {
            if (ui->sectionInput == 1)
                s->position_meters = meters;
            else
                s->step_meters = meters;
            ui->sectionInput = 0;
            SDL_StopTextInput();
        }
    } else if ((action == UI_SECTION_PREVIOUS || action == UI_SECTION_NEXT) && range) {
        s->position_meters =
            fmax(lo, fmin(hi, s->position_meters + (action == UI_SECTION_NEXT ? 1 : -1) * s->step_meters));
    } else if (action == UI_SECTION_FLIP && s->mode == LAYOUT_SECTION_CUTAWAY)
        s->flipped = !s->flipped;
    else if (action == UI_SECTION_SELECTED) {
        const Object3D* o =
            Layout_ObjectStore_FindConst(&state->layout.objectStore, state->editor.selectedObject3DId);
        if (o) {
            Layout_ShowAllViews(&state->layout.objectStore);
            LayoutEntityQuery q = {0};
            if (o->info.parent_id[0])
                snprintf(q.assembly_id, sizeof(q.assembly_id), "%s", o->info.parent_id);
            else {
                snprintf(ui->sectionMessage, sizeof(ui->sectionMessage),
                         "Selected object needs an assembly to focus its unit.");
                return true;
            }
            state->layout.objectStore.view_query = q;
            if (Layout_SectionRange(&state->layout, s->axis, &lo, &hi))
                s->position_meters = (lo + hi) * .5;
            (void)Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_MATERIAL);
            (void)LineDrawingViewportZoom_FitVisibleGeometry(state);
        }
    } else if (action == UI_SECTION_ALL) {
        memset(&state->layout.objectStore.view_query, 0, sizeof(LayoutEntityQuery));
        Layout_ShowAllViews(&state->layout.objectStore);
        (void)LineDrawingViewportZoom_FitVisibleGeometry(state);
    }
    if (!ui->sectionInput)
        SDL_StopTextInput();
    refresh();
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    return true;
}
bool UIPanel_SectionEvent(const SDL_Event* event) {
    UIPanelState* ui = UIPanel_Get();
    if (!event || !ui)
        return false;
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST)
        ui->sectionDragging = false;
    if (!active()) {
        ui->sectionDragging = false;
        if (ui->sectionInput)
            SDL_StopTextInput();
        ui->sectionInput = 0;
        return false;
    }
    if (ui->sectionDragging) {
        if (event->type == SDL_MOUSEMOTION) {
            scrub(event->motion.x);
            return true;
        }
        if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
            ui->sectionDragging = false;
            return true;
        }
    }
    if (ui->sectionInput && event->type == SDL_TEXTINPUT) {
        if (ui->sectionReplace) {
            ui->sectionText[0] = 0;
            ui->sectionReplace = false;
        }
        size_t n = strlen(ui->sectionText);
        snprintf(ui->sectionText + n, sizeof(ui->sectionText) - n, "%s", event->text.text);
        return true;
    }
    if (ui->sectionInput && event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            SDL_Rect r = rect_for(UI_SECTION_APPLY);
            UIPanel_SectionClick(r.x + 2, r.y + 2);
        } else if (key == SDLK_ESCAPE) {
            ui->sectionInput = 0;
            SDL_StopTextInput();
        } else if (key == SDLK_a && event->key.keysym.mod & (KMOD_CTRL | KMOD_GUI))
            ui->sectionReplace = true;
        else if (key == SDLK_BACKSPACE) {
            size_t n = strlen(ui->sectionText);
            if (ui->sectionReplace) {
                ui->sectionText[0] = 0;
                ui->sectionReplace = false;
            } else if (n)
                ui->sectionText[n - 1] = 0;
        }
        return true;
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
        return UIPanel_SectionClick(event->button.x, event->button.y);
    return false;
}
