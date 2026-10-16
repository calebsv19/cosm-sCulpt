#include "UI/shared_overlay.h"
#include "UI/shared_special.h"
#include "UI/shared_text.h"
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
    if (!state || LDOverlay_Blocked()) return false;
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
    int cols=kCols;
    int button_w=kButtonW,button_h=kButtonH;

    if (!state || !object || !out_layout) return false;
    if (!LineDrawingPaneHost_GetViewportRect(&state->paneHost, &viewport)) return false;
    if (viewport.width < 60.0f || viewport.height < 60.0f) return false;

    TTF_Font* font=FontManager_Get(FONT_DEFAULT);int label_w=0;
    if(font){(void)TTF_SizeUTF8(font,"Bottom",&label_w,NULL);button_h=TTF_FontHeight(font)+10;if(button_h<kButtonH)button_h=kButtonH;}
    if(label_w+16>button_w)button_w=label_w+16;
    SDL_Rect notice;int reserved=LDOverlay_NoticeRect(state,&notice)?notice.h+8:0;
    int available_w=(int)viewport.width-28,available_h=(int)viewport.height-28-reserved;
    bool fits=false;
    for(int candidate=kCols;candidate<=button_count;++candidate){
        int rows=(button_count+candidate-1)/candidate;
        panel_w=kPad*2+button_w*candidate+kGap*(candidate-1);
        panel_h=kPad*2+button_h*rows+kGap*(rows-1);
        if(panel_w<=available_w && panel_h<=available_h){cols=candidate;fits=true;break;}
    }
    if(!fits && button_w+kPad*2<=available_w){cols=1;panel_w=button_w+kPad*2;panel_h=kPad*2+button_h*button_count+kGap*(button_count-1);fits=panel_h<=available_h;}
    if(!fits)return false;
    x = (int)(viewport.x + viewport.width) - panel_w - 14;
    y = (int)(viewport.y + viewport.height) - panel_h - 14 - reserved;

    memset(out_layout, 0, sizeof(*out_layout));
    out_layout->panel = (SDL_Rect){ x, y, panel_w, panel_h };
    out_layout->button_count = button_count;

    for (i = 0; i < button_count; ++i) {
        const int row = i / cols;
        const int col = i % cols;
        const int bx = x + kPad + (col * (button_w + kGap));
        const int by = y + kPad + (row * (button_h + kGap));
        out_layout->buttons[i].rect = (SDL_Rect){ bx, by, button_w, button_h };
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
            (void)LDText_Label(renderer,font,layout.buttons[i].label,
                (SDL_Rect){text_x,text_y,layout.buttons[i].rect.x+layout.buttons[i].rect.w-4-text_x,layout.buttons[i].rect.h-4},text,false);
        }
    }
}

bool LineDrawingObjectWorkspaceViewportHud_Rect(const GlobalState* state,int index,SDL_Rect* out) {
    const Object3D* object=NULL;ObjectViewportHudLayout l={0};
    if(!out || !ObjectViewportHud_CanShow(state,&object) || !ObjectViewportHud_ResolveLayout(state,object,&l))return false;
    if(index<0){*out=l.panel;return true;}if(index>=l.button_count)return false;*out=l.buttons[index].rect;return true;
}
void LDOverlay_CollectHud(void) {
    const GlobalState* s=Global_Get();const Object3D* object=NULL;ObjectViewportHudLayout l={0};
    if(!ObjectViewportHud_CanShow(s,&object) || !ObjectViewportHud_ResolveLayout(s,object,&l))return;
    SDL_Rect clip=l.panel;
    for(int i=0;i<l.button_count;++i)LDSpecial_Record(32,i+1,object->coreMeta.object_id,l.buttons[i].rect,&clip,true);
}
