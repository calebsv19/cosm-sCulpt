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
    char fields[17][64], message[192];
    bool valid, open, panels, connections, sink, replace, edited[17];
    LayoutFurnitureContact link, observed_links[LAYOUT_MAX_FURNITURE_CONTACTS];
    size_t observed_link_count;
    char gap[64];
    bool gap_edited;
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
static bool related(const LayoutFurnitureContact* c) {
    return !strcmp(c->driver, form.observed.assembly_id) || !strcmp(c->follower, form.observed.assembly_id);
}
static void load_link(const LayoutFurnitureContact* link) {
    memset(&form.link, 0, sizeof(form.link));
    if (link) form.link = *link;
    else {
        snprintf(form.link.driver, sizeof(form.link.driver), "%s", form.observed.assembly_id);
        form.link.driver_end = -1; form.link.follower_end = 1; form.link.enabled = true;
        const LayoutObjectStore* s = &Global_Get()->layout.objectStore;
        for (size_t i = 0; i < s->furniture_count; ++i)
            if (strcmp(s->furniture[i].assembly_id, form.link.driver)) {
                snprintf(form.link.follower, sizeof(form.link.follower), "%s", s->furniture[i].assembly_id); break;
            }
    }
    length(form.link.gap_m, form.gap, sizeof(form.gap)); form.gap_edited = false;
}
static void choose_link(void) {
    const LayoutObjectStore* s = &Global_Get()->layout.objectStore;
    for (size_t i = 0; i < s->furniture_contact_count; ++i)
        if (!strcmp(form.link.id, s->furniture_contacts[i].id) && related(&s->furniture_contacts[i])) {
            load_link(&s->furniture_contacts[i]); return;
        }
    for (size_t i = 0; i < s->furniture_contact_count; ++i)
        if (related(&s->furniture_contacts[i])) { load_link(&s->furniture_contacts[i]); return; }
    load_link(NULL);
}
static void sync(void) {
    const LayoutFurnitureUnit* unit = active() ? Layout_FurnitureForObject(&Global_Get()->layout.objectStore,
        Global_Get()->editor.selectedObject3DId) : NULL;
    if (!unit) { UIPanel_FurnitureReset(); return; }
    const LayoutObjectStore* store = &Global_Get()->layout.objectStore;
    if (form.valid && !memcmp(unit, &form.observed, sizeof(*unit)) &&
        form.observed_link_count == store->furniture_contact_count &&
        !memcmp(form.observed_links, store->furniture_contacts, sizeof(form.observed_links))) return;
    bool reopen = form.valid && !strcmp(form.observed.assembly_id, unit->assembly_id) && form.open;
    bool panels = form.panels, connections = form.connections, sink = form.sink; int keep = form.keep;
    char link_id[64]; snprintf(link_id, sizeof(link_id), "%s", form.link.id);
    UIPanel_FurnitureReset();
    form.valid = true; form.open = reopen; form.keep = reopen ? keep : 1; form.panels = reopen && panels;
    form.observed = *unit; form.connections = reopen && connections; form.sink = reopen && sink;
    memcpy(form.observed_links, store->furniture_contacts, sizeof(form.observed_links));
    form.observed_link_count = store->furniture_contact_count;
    snprintf(form.link.id, sizeof(form.link.id), "%s", link_id); choose_link();
    length(unit->size_m[1], form.fields[0], 64);
    length(unit->size_m[0], form.fields[1], 64);
    length(unit->size_m[2], form.fields[2], 64);
    for (int k = 0; k < 4; ++k) {
        length(unit->thickness_m[k], form.fields[k + 3], 64);
        length(unit->overhang_m[k], form.fields[k + 12], 64);
    }
    length(unit->thickness_m[0] + unit->shelf_fraction * (unit->size_m[2]-2*unit->thickness_m[0]), form.fields[16],64);
    for (size_t j = 0; unit->sink_opening && j < unit->part_count; ++j) if (unit->parts[j].role == LAYOUT_FURNITURE_FIXTURE) {
        for (int k = 0; k < 3; ++k) length(unit->parts[j].span_m[k],form.fields[k+7],64);
        for (int k = 0; k < 2; ++k) length(unit->parts[j].offset_m[k],form.fields[k+10],64);
        break;
    }
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
static bool panel_field(int k) {
    if (k < 7) return thickness_present(k);
    if (k == 16) return has_role(LAYOUT_FURNITURE_SHELF);
    return has_role(LAYOUT_FURNITURE_WORKTOP);
}
static int panel_count(void) {
    int n=0;
    for (int k=3;k<17;++k) if ((k<7 || k>=12) && panel_field(k)) ++n;
    return n;
}
static int sink_rows(void) { return form.observed.sink_opening ? 1 + (form.sink ? 5 : 0) : 0; }
bool UIPanel_FurnitureEditing(void) { sync(); return form.valid && form.open; }
static int h(void) { TTF_Font* f = FontManager_Get(FONT_DEFAULT); return (f ? TTF_FontHeight(f) : 16) + 10; }
int UIPanel_FurnitureHeight(void) {
    sync();
    if (!form.valid) return 0;
    return h() * (form.open ? 11 + (form.panels ? panel_count() : 0) + (form.connections ? 7 : 0) + sink_rows() : 1) + 12;
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
    case UI_FURNITURE_OVERHANG_WALL: case UI_FURNITURE_OVERHANG_AISLE:
    case UI_FURNITURE_OVERHANG_REAR: case UI_FURNITURE_OVERHANG_FRONT: case UI_FURNITURE_SHELF:
        if (!form.panels) return (SDL_Rect){0};
        {
            int k = action <= UI_FURNITURE_WORKTOP ? 3 + action - UI_FURNITURE_CASE : 12 + action - UI_FURNITURE_OVERHANG_WALL;
            if (!panel_field(k)) return (SDL_Rect){0};
            row = 8;
            for (int i = 3; i < k; ++i) row += (int)((i<7 || i>=12) && panel_field(i));
        }
        break;
    case UI_FURNITURE_APPLY: case UI_FURNITURE_CANCEL:
        row = 9 + (form.panels ? panel_count() : 0) + (form.connections ? 7 : 0); cols = 2; col = action - UI_FURNITURE_APPLY; row += sink_rows(); break;
    case UI_FURNITURE_SINK:
        if (!form.observed.sink_opening) return (SDL_Rect){0};
        row = 8 + (form.panels ? panel_count() : 0); break;
    case UI_FURNITURE_SINK_WIDTH: case UI_FURNITURE_SINK_LENGTH: case UI_FURNITURE_SINK_DEPTH:
    case UI_FURNITURE_SINK_WALL_OFFSET: case UI_FURNITURE_SINK_FRONT_OFFSET:
        if (!form.observed.sink_opening || !form.sink) return (SDL_Rect){0};
        row = 9 + (form.panels ? panel_count() : 0) + action - UI_FURNITURE_SINK_WIDTH; break;
    case UI_FURNITURE_CONNECTIONS: row = 8 + (form.panels ? panel_count() : 0) + sink_rows(); break;
    case UI_FURNITURE_LINK: row = 9; break;
    case UI_FURNITURE_DRIVER: case UI_FURNITURE_DRIVER_END:
        row = 10; cols = 2; col = action - UI_FURNITURE_DRIVER; break;
    case UI_FURNITURE_FOLLOWER: case UI_FURNITURE_FOLLOWER_END:
        row = 11; cols = 2; col = action - UI_FURNITURE_FOLLOWER; break;
    case UI_FURNITURE_BEHAVIOR: case UI_FURNITURE_ENABLED:
        row = 12; cols = 2; col = action - UI_FURNITURE_BEHAVIOR; break;
    case UI_FURNITURE_GAP: row = 13; break;
    case UI_FURNITURE_SAVE_LINK: case UI_FURNITURE_REMOVE_LINK:
        row = 14; cols = 2; col = action - UI_FURNITURE_SAVE_LINK; break;
    case UI_FURNITURE_NEW_LINK: row = 15; break;
    default: return (SDL_Rect){0};
    }
    if (action >= UI_FURNITURE_LINK && action <= UI_FURNITURE_NEW_LINK) {
        if (!form.connections) return (SDL_Rect){0};
        row += (form.panels ? panel_count() : 0) + sink_rows();
    }
    if (action == UI_FURNITURE_DRIVER || action == UI_FURNITURE_DRIVER_END ||
        action == UI_FURNITURE_FOLLOWER || action == UI_FURNITURE_FOLLOWER_END) {
        int name_width = (b.w - 20) * 2 / 3;
        return (SDL_Rect){b.x + 6 + (col ? name_width + 4 : 0), b.y + 5 + row * h(),
                          col ? b.w - 20 - name_width : name_width, h() - 4};
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
    if (action >= UI_FURNITURE_SINK_WIDTH && action <= UI_FURNITURE_SINK_FRONT_OFFSET) return 7 + action - UI_FURNITURE_SINK_WIDTH;
    if (action >= UI_FURNITURE_OVERHANG_WALL && action <= UI_FURNITURE_SHELF) return 12 + action - UI_FURNITURE_OVERHANG_WALL;
    return -1;
}
static const char* unit_label(const char* id) {
    const LayoutAssembly* a = Layout_FindAssembly(&Global_Get()->layout.objectStore, id);
    static char short_label[64];
    snprintf(short_label, sizeof(short_label), "%s", a ? a->info.label : "Choose unit");
    char* split = strstr(short_label, " / "); if (split) *split = 0;
    return short_label;
}
static void render_connections(SDL_Renderer* r) {
    const LayoutObjectStore* s = &Global_Get()->layout.objectStore; size_t count = 0; char text[192];
    for (size_t i = 0; i < s->furniture_contact_count; ++i) count += (size_t)related(&s->furniture_contacts[i]);
    snprintf(text, sizeof(text), "Connections %zu %s", count, form.connections ? "-" : "+");
    paint(r, rectangle(UI_FURNITURE_CONNECTIONS), text, true, form.connections);
    if (!form.connections) return;
    size_t at = 0, selected = 0;
    for (size_t i = 0; i < s->furniture_contact_count; ++i) if (related(&s->furniture_contacts[i])) {
        ++at; if (!strcmp(form.link.id, s->furniture_contacts[i].id)) selected = at;
    }
    if (selected) snprintf(text, sizeof(text), "Link %zu of %zu%s", selected, count, count > 1 ? "  >" : "");
    else snprintf(text, sizeof(text), "New link");
    paint(r, rectangle(UI_FURNITURE_LINK), text, true, false);
    snprintf(text, sizeof(text), "Driver: %.140s >", unit_label(form.link.driver));
    paint(r, rectangle(UI_FURNITURE_DRIVER), text, true, false);
    paint(r, rectangle(UI_FURNITURE_DRIVER_END), form.link.driver_end < 0 ? "Rear end" : "Front end", true, false);
    snprintf(text, sizeof(text), "Follows: %.140s >", unit_label(form.link.follower));
    paint(r, rectangle(UI_FURNITURE_FOLLOWER), text, true, false);
    paint(r, rectangle(UI_FURNITURE_FOLLOWER_END), form.link.follower_end < 0 ? "Rear end" : "Front end", true, false);
    paint(r, rectangle(UI_FURNITURE_BEHAVIOR), form.link.behavior == LAYOUT_FURNITURE_FOLLOW_MOVE ? "Move unit >" : "Fit run >", true, false);
    paint(r, rectangle(UI_FURNITURE_ENABLED), form.link.enabled ? "Enabled" : "Disabled", true, form.link.enabled);
    snprintf(text, sizeof(text), "Gap: %s%s", form.gap, form.input == 18 ? " |" : "");
    paint(r, rectangle(UI_FURNITURE_GAP), text, true, form.input == 18);
    paint(r, rectangle(UI_FURNITURE_SAVE_LINK), "Save link", true, false);
    paint(r, rectangle(UI_FURNITURE_REMOVE_LINK), "Remove link", true, false);
    paint(r, rectangle(UI_FURNITURE_NEW_LINK), "New link", true, false);
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
    paint(renderer, note, has_role(LAYOUT_FURNITURE_WORKTOP) ? "Wall/floor fixed; top via Panels" : "Wall face and floor stay fixed", false, false);
    const char* labels[] = {"Length", "Depth", "Height", "Case", "Back", "Door", "Worktop", "Bowl width", "Bowl length", "Bowl depth", "From wall", "From front", "Wall overhang", "Aisle overhang", "Rear overhang", "Front overhang", "Shelf height"};
    for (int action = UI_FURNITURE_LENGTH; action <= UI_FURNITURE_SHELF; ++action) {
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
    if (form.observed.sink_opening) paint(renderer,rectangle(UI_FURNITURE_SINK),
        form.sink ? "Sink / opening -" : "Sink / opening +",true,form.sink);
    render_connections(renderer);
    paint(renderer, rectangle(UI_FURNITURE_APPLY), "Apply unit", true, false);
    paint(renderer, rectangle(UI_FURNITURE_CANCEL), "Cancel / back", true, false);
    note.y = body.y + 5 + (10 + (form.panels ? panel_count() : 0) + (form.connections ? 7 : 0) + sink_rows()) * h();
    paint(renderer, note, form.message[0] ? form.message : "Apply resizes the unit and its enabled followers", false, false);
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
    for (int k = 0; k < 17; ++k) {
        if (!form.edited[k]) continue;
        double v;
        if (!Editor_ParseLength(form.fields[k], UIPanel_GetDisplayUnit(), &v) || v < 0 || (k < 10 && v <= 0)) {
            snprintf(form.message, sizeof(form.message), "Enter a positive length in every field."); return;
        }
        if (k < 3) next.size_m[k == 0 ? 1 : k == 1 ? 0 : 2] = v;
        else if (k < 7) next.thickness_m[k - 3] = v;
        else if (k < 12) {
            for (size_t j=0;j<next.part_count;++j) if (next.parts[j].role == LAYOUT_FURNITURE_FIXTURE) {
                if (k < 10) {
                    if (k==9) next.parts[j].offset_m[2] += (next.parts[j].span_m[2]-v)/2;
                    next.parts[j].span_m[k-7]=v;
                } else next.parts[j].offset_m[k-10]=v;
                break;
            }
        } else if (k < 16) next.overhang_m[k-12]=v;
        else next.shelf_fraction = (v-next.thickness_m[0])/(next.size_m[2]-2*next.thickness_m[0]);
    }
    if (!memcmp(&next, &form.observed, sizeof(next))) {
        snprintf(form.message, sizeof(form.message), "Dimensions unchanged."); return;
    }
    if (!Layout_EditFurnitureUnit(&Global_Get()->layout, &next, form.keep, Layout_GeometryHistory, NULL)) {
        snprintf(form.message, sizeof(form.message), "%.190s", Global_Get()->layout.geometryMessage); return;
    }
    sync(); snprintf(form.message, sizeof(form.message), "Unit and followers updated. Run Checks for clearance.");
}
static void cycle_unit(char id[64], const char* exclude) {
    const LayoutObjectStore* s = &Global_Get()->layout.objectStore; size_t start = 0;
    for (size_t i = 0; i < s->furniture_count; ++i) if (!strcmp(id, s->furniture[i].assembly_id)) start = i + 1;
    for (size_t at = 0; at < s->furniture_count; ++at) {
        size_t i = (start + at) % s->furniture_count;
        if (strcmp(s->furniture[i].assembly_id, exclude)) { snprintf(id, 64, "%s", s->furniture[i].assembly_id); break; }
    }
}
static void cycle_link(void) {
    const LayoutObjectStore* s = &Global_Get()->layout.objectStore; size_t start = 0;
    for (size_t i = 0; i < s->furniture_contact_count; ++i) if (!strcmp(form.link.id, s->furniture_contacts[i].id)) start = i + 1;
    for (size_t at = 0; at < s->furniture_contact_count; ++at) {
        size_t i = (start + at) % s->furniture_contact_count;
        if (related(&s->furniture_contacts[i])) { load_link(&s->furniture_contacts[i]); break; }
    }
}
static void save_link(bool remove) {
    if (remove && !form.link.id[0]) { snprintf(form.message, sizeof(form.message), "Choose a saved link first."); return; }
    double gap = form.link.gap_m;
    if (!remove && form.gap_edited && (!Editor_ParseLength(form.gap, UIPanel_GetDisplayUnit(), &gap) || gap < 0 || gap > 1)) {
        snprintf(form.message, sizeof(form.message), "Gap must be between 0 and 1 m."); return;
    }
    if (!remove) form.link.gap_m = gap;
    bool new_link = !remove && !form.link.id[0];
    if (!Layout_EditFurnitureContact(&Global_Get()->layout, remove ? NULL : &form.link,
                                     remove ? form.link.id : NULL, Layout_GeometryHistory, NULL)) {
        snprintf(form.message, sizeof(form.message), "%.190s", Global_Get()->layout.geometryMessage); return;
    }
    char saved_id[64]; snprintf(saved_id, sizeof(saved_id), "%s", new_link ?
        Global_Get()->layout.objectStore.furniture_contacts[Global_Get()->layout.objectStore.furniture_contact_count - 1].id : form.link.id);
    sync(); snprintf(form.link.id, sizeof(form.link.id), "%s", saved_id); choose_link();
    snprintf(form.message, sizeof(form.message), "Link %s. Undo restores geometry and links.", remove ? "removed" : "saved");
}
bool UIPanel_FurnitureClick(int x, int y) {
    if (!active() || !contains(UIPanel_Get()->rightBodyRect, x, y)) return false;
    sync(); if (!form.valid || !contains(UIPanel_Get()->objectPane.furnitureRect, x, y)) return false;
    for (int a = UI_FURNITURE_OPEN; a <= UI_FURNITURE_SHELF; ++a) {
        if (!contains(rectangle(a), x, y)) continue;
        int k = field(a);
        if (k >= 0) { form.input = k + 1; form.replace = true; SDL_StartTextInput(); }
        else if (a == UI_FURNITURE_OPEN) {
            if (form.open) UIPanel_FurnitureReset();
            else form.open = true;
            form.input = 0; SDL_StopTextInput();
        }
        else if (a == UI_FURNITURE_KEEP) form.keep = form.keep == 1 ? 0 : form.keep == 0 ? -1 : 1;
        else if (a == UI_FURNITURE_SINK) { form.sink = !form.sink; form.input = 0; SDL_StopTextInput(); }
        else if (a == UI_FURNITURE_PANELS) { form.panels = !form.panels; form.input = 0; SDL_StopTextInput(); }
        else if (a == UI_FURNITURE_APPLY) { bool link_input = form.input == 18; form.input = 0; SDL_StopTextInput(); if (link_input) save_link(false); else apply(); }
        else if (a == UI_FURNITURE_CANCEL) UIPanel_FurnitureReset();
        else {
            form.input = 0; SDL_StopTextInput();
            switch (a) {
            case UI_FURNITURE_CONNECTIONS: form.connections = !form.connections; choose_link(); break;
            case UI_FURNITURE_LINK: cycle_link(); break;
            case UI_FURNITURE_DRIVER: cycle_unit(form.link.driver, form.link.follower); break;
            case UI_FURNITURE_DRIVER_END: form.link.driver_end *= -1; break;
            case UI_FURNITURE_FOLLOWER: cycle_unit(form.link.follower, form.link.driver); break;
            case UI_FURNITURE_FOLLOWER_END: form.link.follower_end *= -1; break;
            case UI_FURNITURE_BEHAVIOR: form.link.behavior = form.link.behavior == LAYOUT_FURNITURE_FOLLOW_MOVE ? LAYOUT_FURNITURE_FOLLOW_FIT_RUN : LAYOUT_FURNITURE_FOLLOW_MOVE; break;
            case UI_FURNITURE_ENABLED: form.link.enabled = !form.link.enabled; break;
            case UI_FURNITURE_GAP: form.input = 18; form.replace = true; SDL_StartTextInput(); break;
            case UI_FURNITURE_SAVE_LINK: save_link(false); break;
            case UI_FURNITURE_REMOVE_LINK: save_link(true); break;
            case UI_FURNITURE_NEW_LINK: load_link(NULL); break;
            default: break;
            }
        }
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
    char* text = form.input == 18 ? form.gap : form.fields[form.input - 1];
    if (event->type == SDL_TEXTINPUT) {
        if (form.input <= 17) form.edited[form.input - 1] = true;
        else form.gap_edited = true;
        if (form.replace) { text[0] = 0; form.replace = false; }
        size_t n = strlen(text); snprintf(text + n, 64 - n, "%s", event->text.text);
        form.message[0] = 0; Global_FlagGridChanged(); return true;
    }
    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) { bool link_input = form.input == 18; form.input = 0; SDL_StopTextInput(); if (link_input) save_link(false); else apply(); }
        else if (key == SDLK_ESCAPE) UIPanel_FurnitureReset();
        else if (key == SDLK_a && event->key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) form.replace = true;
        else if (key == SDLK_BACKSPACE) {
            if (form.input <= 17) form.edited[form.input - 1] = true;
        else form.gap_edited = true;
            size_t n = strlen(text);
            if (form.replace) text[0] = 0;
            else if (n) text[n - 1] = 0;
            form.replace = false;
        } else if (key == SDLK_TAB) { form.input = form.input % 3 + 1; form.replace = true; }
        refresh(); return true;
    }
    return false;
}
