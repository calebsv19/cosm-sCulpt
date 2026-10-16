#include "Input/input_mouse_internal.h"
#include "UI/shared_overlay.h"
#include "UI/object_workspace_viewport_hud.h"
#include "Core/global_state.h"
#include "UI/input_ui_panel.h"
#include "UI/panel/ui_panel_file_browser_internal.h"
#include "UI/panel/ui_panel_stl_options.h"
#include "UI/panel/ui_panel_visibility.h"
#include "UI/shared_special.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_camera.h"
#include "UI/ui_panel_camera_path.h"
#include "UI/ui_panel_furniture.h"
#include "UI/ui_panel_routes.h"
#include "UI/ui_panel_section.h"
#include <stdio.h>
#include <string.h>

static KitUiSurface *collecting;
static uint64_t identity;
typedef struct Dispatch {
    SDL_Rect rect;
    unsigned domain;
} Dispatch;
static Dispatch dispatch[256];
static size_t dispatch_count;
static uint64_t hash(uint64_t h, const char *s) {
    for (; s && *s; ++s) {
        h ^= (unsigned char)*s;
        h *= UINT64_C(1099511628211);
    }
    return h;
}
void LDSpecial_Record(unsigned domain, int action, const char *key, SDL_Rect rect,
                      const SDL_Rect *clip, bool enabled) {
    if (!collecting || !action)
        return;
    SDL_Rect window = {0, 0, Global_GetScreenWidth(), Global_GetScreenHeight()}, visible = rect;
    if (!SDL_IntersectRect(&visible, &window, &visible) ||
        (clip && !SDL_IntersectRect(&visible, clip, &visible)))
        return;
    /* One command can have multiple visible placements (e.g. Show all).
       A moved placement cancels capture rather than retargeting the release. */
    char placement[96];
    snprintf(placement, sizeof(placement), "%u:%d:%d:%d", domain, action, rect.x, rect.y);
    uint64_t id = hash(hash(identity, key), placement);
    LDControls_Add(collecting, domain, id, rect, clip, enabled);
    if (dispatch_count < sizeof(dispatch) / sizeof(*dispatch))
        dispatch[dispatch_count++] = (Dispatch){visible, domain};
}
static void browser(void) {
    UIPanelState *u = UIPanel_Get();
    SDL_Rect clip = UIPanel_GetLoadMenuListClipRect(u);
    if (u->loadMenu.stlCollectionPicker) {
        SDL_Rect pane = UIPanel_GetLoadMenuRect(u);
        const LayoutObjectStore *s = &Global_Get()->layout.objectStore;
        for (unsigned i = 0; i < 8; ++i) {
            unsigned at = u->loadMenu.stlCollectionPage * 8 + i;
            const char *key = at == 0                           ? "unassigned"
                              : at <= s->scene_collection_count ? s->scene_collections[at - 1].id
                                                                : "empty";
            LDSpecial_Record(28, 3000 + (int)i, key,
                             (SDL_Rect){pane.x + 8, pane.y + 100 + (int)i * 24, pane.w - 16, 23},
                             &pane, !u->loadMenu.asyncStlActive);
        }
        LDSpecial_Record(28, 4000, "next page",
                         (SDL_Rect){pane.x + 8, pane.y + 292, pane.w - 16, 23}, &pane,
                         !u->loadMenu.asyncStlActive);
        return;
    }
    int y = clip.y - (int)u->loadMenu.scrollOffsetPx;
    for (int i = 0; i < u->loadMenu.count && y < clip.y + clip.h; ++i) {
        int h = UIPanel_LoadMenuRowHeight(u, i);
        LDSpecial_Record(28, i + 1, u->loadMenu.entryPaths[i], (SDL_Rect){clip.x, y, clip.w, h},
                         &clip, true);
        y += h;
    }
    SDL_Rect box = UIPanel_GetLoadMenuSetDirectoryButtonRect(u);
    LDSpecial_Record(28, -1, "directory", box, NULL, true);
    if (u->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT)
        for (int i = 0; i < 4; ++i) {
            box = UIPanel_StlOptionRect(u, i);
            LDSpecial_Record(28, 2000 + i, "STL", box, NULL, !u->loadMenu.asyncStlActive);
        }
}
void LDSpecial_Collect(KitUiSurface *surface) {
    collecting = surface;
    dispatch_count = 0;
    const GlobalState *s = Global_Get();
    const UIPanelState *u = UIPanel_Get();
    identity = UINT64_C(14695981039346656037) ^ (uint64_t)s->editor.selectedObject3DId;
    /* Row index actions are rebound when document identities/order change,
       including equal-label replacements by Undo or external document edits. */
    for (size_t i = 0; i < s->layout.objectStore.count; ++i)
        identity = hash(identity, s->layout.objectStore.items[i].coreMeta.object_id);
    for (size_t i = 0; i < s->layout.objectStore.assembly_count; ++i)
        identity = hash(identity, s->layout.objectStore.assemblies[i].id);
    for (size_t i = 0; i < s->layout.objectStore.route_count; ++i)
        identity = hash(identity, s->layout.objectStore.routes[i].id);
    for (size_t i = 0; i < s->layout.objectStore.relationship_count; ++i)
        identity = hash(identity, s->layout.objectStore.relationships[i].id);
    for (size_t i = 0; i < s->layout.objectStore.scene_collection_count; ++i)
        identity = hash(identity, s->layout.objectStore.scene_collections[i].id);
    for (size_t i = 0; i < s->layout.objectStore.working_view_count; ++i)
        identity = hash(identity, s->layout.objectStore.working_views[i].id);
    for (size_t i = 0; i < s->layout.objectStore.measurement_count; ++i)
        identity = hash(identity, s->layout.objectStore.measurements[i].id);
    identity = hash(identity, u->parts.id);
    identity = hash(identity, u->routes.id);
    if (LDViewControls_ActivePopup() == LD_VIEW_VISIBILITY)
        LDSpecial_Visibility();
    else {
        SDL_Rect box, clip = u->rightBodyRect;
        LDSpecial_Generic();
        if(UIPanel_IsLoadMenuOpen())browser();
        else {LDSpecial_SceneList();LDSpecial_ObjectTree();}
        LDSpecial_Parts();
        LDSpecial_Routes();
        LDSpecial_Measure();
        LDSpecial_Inspection();
        LDSpecial_Camera();
        LDSpecial_Path();
        LDSpecial_Section();
        LDOverlay_CollectHud();
        for (int a = UI_FURNITURE_OPEN; a <= UI_FURNITURE_SHELF; ++a)
            if (UIPanel_FurnitureControlRect(a, &box))
                LDSpecial_Record(26, a, "furniture", box, &clip, true);
    }
    collecting = NULL;
}
bool LDSpecial_Dispatch(int x, int y) {
    unsigned domain = 0;
    for (size_t i = dispatch_count; i > 0; --i)
        if (SDL_PointInRect(&(SDL_Point){x, y}, &dispatch[i - 1].rect)) {
            domain = dispatch[i - 1].domain;
            break;
        }
    if (!domain)
        return false;
    if(domain==32){bool handled=LineDrawingObjectWorkspaceViewportHud_HandleClick(Global_Get(),x,y);if(handled){SyncObjectFaceSketchTarget(&Global_Get()->editor);UpdateHover(x,y);}return handled;}
    if (domain == 27)
        return UIPanel_VisibilityClick(x, y);
    SDL_Event click = {.type = SDL_MOUSEBUTTONDOWN};
    click.button.button = SDL_BUTTON_LEFT;
    click.button.x = x;
    click.button.y = y;
    if (domain == 24)
        return UIPanel_CameraEvent(&click);
    if (domain == 25)
        return UIPanel_SectionEvent(&click);
    if (domain == 26)
        return UIPanel_FurnitureEvent(&click);
    if (domain == 21)
        return UIPanel_RoutesEvent(&click);
    return UIPanel_HandleClick(x, y);
}
