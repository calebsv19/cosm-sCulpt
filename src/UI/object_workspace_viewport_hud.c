#include "UI/object_workspace_viewport_hud.h"

#include "Core/line_drawing_pane_host.h"
#include "Core/workspace/line_drawing_object_workspace_view.h"
#include "Layout/scene/layout_object_faces.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/text_draw.h"

#include <string.h>

typedef struct {
    const char* label;
    Object3DFaceKind face;
    SDL_Rect rect;
} ObjectViewportHudButton;

typedef struct {
    SDL_Rect panel;
    ObjectViewportHudButton buttons[8];
    int button_count;
} ObjectViewportHudLayout;

static bool ObjectViewportHud_CanShow(const GlobalState* state, const Object3D** out_object) {
    const Object3D* object = NULL;
    if (out_object) *out_object = NULL;
    if (!state) return false;
    if (state->workspaceMode != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return false;
    if (state->editor.selectedObject3DId == 0u) return false;
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                          state->editor.selectedObject3DId);
    if (!object) return false;
    if (out_object) *out_object = object;
    return true;
}

static bool ObjectViewportHud_ResolveLayout(const GlobalState* state,
                                            const Object3D* object,
                                            ObjectViewportHudLayout* out_layout) {
    CorePaneRect viewport = {0};
    int panel_w = 0;
    int panel_h = 0;
    int x = 0;
    int y = 0;
    int i = 0;
    static const int kPad = 10;
    static const int kGap = 6;
    static const int kButtonW = 74;
    static const int kButtonH = 28;
    static const int kCols = 2;
    static const struct {
        const char* label;
        Object3DFaceKind face;
    } kPlaneButtons[] = {
        { "3D", OBJECT3D_FACE_NONE },
        { "Face", OBJECT3D_FACE_PLANE_SURFACE }
    };
    static const struct {
        const char* label;
        Object3DFaceKind face;
    } kPrismButtons[] = {
        { "3D", OBJECT3D_FACE_NONE },
        { "Top", OBJECT3D_FACE_RECT_PRISM_POS_N },
        { "Bottom", OBJECT3D_FACE_RECT_PRISM_NEG_N },
        { "Left", OBJECT3D_FACE_RECT_PRISM_NEG_U },
        { "Right", OBJECT3D_FACE_RECT_PRISM_POS_U },
        { "Front", OBJECT3D_FACE_RECT_PRISM_POS_V },
        { "Back", OBJECT3D_FACE_RECT_PRISM_NEG_V }
    };
    const int button_count = (object && object->kind == OBJECT3D_KIND_RECT_PRISM)
                                 ? (int)(sizeof(kPrismButtons) / sizeof(kPrismButtons[0]))
                                 : (int)(sizeof(kPlaneButtons) / sizeof(kPlaneButtons[0]));
    const int rows = (button_count + (kCols - 1)) / kCols;

    if (!state || !object || !out_layout) return false;
    if (!LineDrawingPaneHost_GetViewportRect(&state->paneHost, &viewport)) return false;
    if (viewport.width < 60.0f || viewport.height < 60.0f) return false;

    panel_w = (kPad * 2) + (kButtonW * kCols) + kGap;
    panel_h = (kPad * 2) + (kButtonH * rows) + (kGap * (rows - 1));
    x = (int)(viewport.x + viewport.width) - panel_w - 14;
    y = (int)(viewport.y + viewport.height) - panel_h - 14;

    memset(out_layout, 0, sizeof(*out_layout));
    out_layout->panel = (SDL_Rect){ x, y, panel_w, panel_h };
    out_layout->button_count = button_count;

    for (i = 0; i < button_count; ++i) {
        const int row = i / kCols;
        const int col = i % kCols;
        const int bx = x + kPad + (col * (kButtonW + kGap));
        const int by = y + kPad + (row * (kButtonH + kGap));
        out_layout->buttons[i].rect = (SDL_Rect){ bx, by, kButtonW, kButtonH };
        if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            out_layout->buttons[i].label = kPrismButtons[i].label;
            out_layout->buttons[i].face = kPrismButtons[i].face;
        } else {
            out_layout->buttons[i].label = kPlaneButtons[i].label;
            out_layout->buttons[i].face = kPlaneButtons[i].face;
        }
    }

    return true;
}

bool LineDrawingObjectWorkspaceViewportHud_HandleClick(GlobalState* state,
                                                       int mouse_x,
                                                       int mouse_y) {
    const Object3D* object = NULL;
    ObjectViewportHudLayout layout = {0};
    SDL_Point point = { mouse_x, mouse_y };
    int i = 0;

    if (!ObjectViewportHud_CanShow(state, &object)) return false;
    if (!ObjectViewportHud_ResolveLayout(state, object, &layout)) return false;
    if (!SDL_PointInRect(&point, &layout.panel)) return false;

    for (i = 0; i < layout.button_count; ++i) {
        if (!SDL_PointInRect(&point, &layout.buttons[i].rect)) continue;
        if (layout.buttons[i].face == OBJECT3D_FACE_NONE) {
            return LineDrawingObjectWorkspaceView_EnterFreeView(state,
                                                                state->editor.selectedObject3DId);
        }
        return LineDrawingObjectWorkspaceView_FocusFace(state,
                                                        state->editor.selectedObject3DId,
                                                        layout.buttons[i].face);
    }
    return true;
}

void LineDrawingObjectWorkspaceViewportHud_Render(SDL_Renderer* renderer,
                                                  const GlobalState* state) {
    const Object3D* object = NULL;
    ObjectViewportHudLayout layout = {0};
    LineDrawing3dThemePalette palette = {0};
    const bool has_palette = line_drawing3d_shared_theme_resolve_palette(&palette);
    TTF_Font* font = NULL;
    int i = 0;

    if (!renderer) return;
    if (!ObjectViewportHud_CanShow(state, &object)) return;
    if (!ObjectViewportHud_ResolveLayout(state, object, &layout)) return;

    font = FontManager_Get(FONT_DEFAULT);

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    SDL_SetRenderDrawColor(renderer,
                           has_palette ? palette.panel_fill.r : 26,
                           has_palette ? palette.panel_fill.g : 28,
                           has_palette ? palette.panel_fill.b : 34,
                           224);
    (void)SDL_RenderFillRect(renderer, &layout.panel);
    SDL_SetRenderDrawColor(renderer,
                           has_palette ? palette.panel_border.r : 92,
                           has_palette ? palette.panel_border.g : 98,
                           has_palette ? palette.panel_border.b : 112,
                           255);
    (void)SDL_RenderDrawRect(renderer, &layout.panel);

    for (i = 0; i < layout.button_count; ++i) {
        const bool active_face = layout.buttons[i].face != OBJECT3D_FACE_NONE &&
                                 state->editor.selectedObjectAssetFace == layout.buttons[i].face;
        const bool active_3d = layout.buttons[i].face == OBJECT3D_FACE_NONE &&
                               state->editor.selectedObjectAssetFace == OBJECT3D_FACE_NONE;
        SDL_Color fill = active_face || active_3d
                             ? (has_palette ? palette.button_fill
                                            : (SDL_Color){ 92, 114, 156, 240 })
                             : (has_palette ? palette.background_fill
                                            : (SDL_Color){ 19, 21, 26, 245 });
        SDL_Color border = has_palette ? palette.panel_border : (SDL_Color){ 108, 116, 132, 255 };
        SDL_Color text = has_palette ? palette.text_primary : (SDL_Color){ 234, 236, 240, 255 };

        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        (void)SDL_RenderFillRect(renderer, &layout.buttons[i].rect);
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
        (void)SDL_RenderDrawRect(renderer, &layout.buttons[i].rect);
        if (font && layout.buttons[i].label && layout.buttons[i].label[0]) {
            int text_w = 0;
            int text_h = 0;
            int text_x = layout.buttons[i].rect.x + 8;
            int text_y = layout.buttons[i].rect.y + 6;
            (void)line_drawing_text_measure_utf8(renderer,
                                                 font,
                                                 layout.buttons[i].label,
                                                 &text_w,
                                                 &text_h);
            text_x = layout.buttons[i].rect.x + ((layout.buttons[i].rect.w - text_w) / 2);
            text_y = layout.buttons[i].rect.y + ((layout.buttons[i].rect.h - text_h) / 2);
            (void)line_drawing_text_draw_utf8_at(renderer,
                                                 font,
                                                 layout.buttons[i].label,
                                                 text_x,
                                                 text_y,
                                                 text);
        }
    }
}
