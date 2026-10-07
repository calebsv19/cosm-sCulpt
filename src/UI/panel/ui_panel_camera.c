#include "Core/camera_path.h"
#include "UI/ui_panel_camera_path.h"
#include "UI/ui_panel_camera.h"
#include "Core/global_state.h"
#include "Layout/layout_inspection.h"
#include "Layout/layout_furniture.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/font_manager.h"
#include "Editor/editor_numeric_edit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool details;
static bool facing_details;
static bool facing_part;
static char facing_message[128];
static int camera_rows(void) {
    return Global_Get() && Global_Get()->cameraView.active ? 10 + (details ? 4 : 0) : 3;
}
static const char* facing_target(void) {
    const GlobalState* s=Global_Get();
    const Object3D* o=Layout_ObjectStore_FindConst(&s->layout.objectStore,s->editor.selectedObject3DId);
    if(!o) return NULL;
    const LayoutFurnitureUnit* unit=Layout_FurnitureForObject(&s->layout.objectStore,o->objectId);
    return unit && !facing_part ? unit->assembly_id : o->coreMeta.object_id;
}
static int input;
static bool replace;
static char text[96];
static int height(void) {
    TTF_Font *f = FontManager_Get(FONT_DEFAULT);
    return (f ? TTF_FontHeight(f) : 16) + 8;
}
static bool active(void) {
    return Global_Get() && Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_SCENE &&
           UIPanel_Get()->activeRightTab == UI_PANEL_RIGHT_TAB_VIEW;
}
int UIPanel_CameraHeight(void) {
    const GlobalState *s = Global_Get();
    (void)s;
    int rows = camera_rows() + 1 + (facing_details ? 6 : 0);
    return rows * height() + 8 + UIPanel_CameraPathHeight();
}
static int path_y(void) {
    return UIPanel_Get()->viewPane.summaryRect.y +
        (camera_rows()+1+(facing_details ? 6 : 0))*height()+8;
}
static SDL_Rect rect(int action) {
    SDL_Rect body = UIPanel_Get()->viewPane.summaryRect;
    int row = 0, col = 0, cols = 1;
    bool on = Global_Get()->cameraView.active;
    if (action >= UI_CAMERA_INSPECTION) {
        row=camera_rows();
        cols=3;
        if(action==UI_CAMERA_INSPECTION) cols=1;
        else if(action==UI_CAMERA_FACING_DETAILS) col=2;
        else {
            if(!facing_details) return (SDL_Rect){0};
            if(action==UI_CAMERA_FACING_SCOPE) { row+=1; col=2; }
            else { row+=2+(action-UI_CAMERA_FACING_INHERIT)/2; cols=2; col=(action-UI_CAMERA_FACING_INHERIT)%2; }
        }
    } else if (action == UI_CAMERA_PREVIOUS || action == UI_CAMERA_NEXT) {
        row = 0;
        cols = 6;
        col = action == UI_CAMERA_PREVIOUS ? 4 : 5;
    } else if (action >= UI_CAMERA_EXPLORE && action <= UI_CAMERA_EXIT) {
        row = 1;
        cols = 3;
        col = action - UI_CAMERA_EXPLORE;
    } else if (action >= UI_CAMERA_SAVE && action <= UI_CAMERA_DETAILS) {
        row = 2;
        cols = 3;
        col = action - UI_CAMERA_SAVE;
    } else if (action >= UI_CAMERA_LOOK && action <= UI_CAMERA_LEVEL) {
        row = 3;
        cols = 3;
        col = action - UI_CAMERA_LOOK;
    } else if (action >= UI_CAMERA_FORWARD && action <= UI_CAMERA_UP) {
        row = 4 + (action - UI_CAMERA_FORWARD) / 3;
        cols = 3;
        col = (action - UI_CAMERA_FORWARD) % 3;
    } else if (action >= UI_CAMERA_X && action <= UI_CAMERA_FAR) {
        row = 9 + (action - UI_CAMERA_X) / 3;
        cols = 3;
        col = (action - UI_CAMERA_X) % 3;
        if (!details)
            return (SDL_Rect){0};
    } else
        return (SDL_Rect){0};
    if (!on && action >= UI_CAMERA_SAVE && action <= UI_CAMERA_FAR)
        return (SDL_Rect){0};
    int w = (body.w - 12 - 4 * (cols - 1)) / cols;
    if(action==UI_CAMERA_INSPECTION) w=(body.w-12)*2/3-4;
    return (SDL_Rect){body.x + 4 + col * (w + 4), body.y + 4 + row * height(), w, height() - 3};
}
bool UIPanel_CameraControlRect(int action, SDL_Rect *out) {
    if (!active() || !out)
        return false;
    if(action>=UI_PATH_SHOW) return UIPanel_CameraPathRect(action,out,path_y());
    *out = rect(action);
    return out->w > 0 && out->h > 0;
}
static bool contains(SDL_Rect r, int x, int y) {
    return r.w > 0 && r.h > 0 && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}
static void paint(SDL_Renderer *renderer, SDL_Rect r, const char *label, bool button, bool selected,
                  bool enabled) {
    if (r.w <= 0 || r.h <= 0)
        return;
    UIPanelVisualPalette p = {0};
    (void)UIPanelVisual_ResolvePalette(&p);
    if (button)
        UIPanelVisual_DrawFrame(renderer, r, selected ? p.button_fill_active : p.button_fill,
                                selected ? p.accent : p.button_border, 0);
    TTF_Font *f = FontManager_Get(FONT_DEFAULT);
    if (f)
        UIPanelSummary_DrawTextClipped(renderer, f, label, r.x + 4, r.y + 3, r.w - 8, r.h - 4,
                                       enabled ? p.text_primary : p.text_muted);
}
static SDL_Rect line(int row) {
    SDL_Rect r = UIPanel_Get()->viewPane.summaryRect;
    return (SDL_Rect){r.x + 4, r.y + 4 + row * height(), r.w - 12, height() - 3};
}
static void refresh(void) {
    GlobalState *s = Global_Get();
    Global_FlagHitboxesDirty();
    UIPanel_OnWindowResized(s->screenWidth, s->screenHeight);
}
static void stop_input(void) {
    input = 0;
    SDL_StopTextInput();
}
static double value(int action) {
    const GlobalState *s = Global_Get();
    const CameraViewSession *c = &s->cameraView;
    switch (action) {
    case UI_CAMERA_X:
        return c->eye.x * s->layout.metersPerWorldUnit;
    case UI_CAMERA_Y:
        return c->eye.y * s->layout.metersPerWorldUnit;
    case UI_CAMERA_Z:
        return c->eye.z * s->layout.metersPerWorldUnit;
    case UI_CAMERA_YAW:
        return c->yaw;
    case UI_CAMERA_PITCH:
        return c->pitch;
    case UI_CAMERA_ROLL_VALUE:
        return c->roll;
    case UI_CAMERA_FOV:
        return c->fov;
    case UI_CAMERA_SPEED:
        return c->speed_mps;
    case UI_CAMERA_NEAR:
        return c->near_clip;
    case UI_CAMERA_FAR:
        return c->far_clip;
    default:
        return 0;
    }
}
static bool update_allowed(void) {
    const GlobalState *s = Global_Get();
    const LineDrawingSceneCamera *c = Layout_SceneAuthoringState_FindCameraByIdConst(
        &s->layout.sceneAuthoring, s->cameraView.source_id);
    return c && !c->path_id[0];
}
static void render_inspection(SDL_Renderer* renderer) {
    const GlobalState* s=Global_Get();
    paint(renderer,rect(UI_CAMERA_INSPECTION),s->inspectionView ? "Inspection: on" : "Inspection: off",
        true,s->inspectionView,true);
    paint(renderer,rect(UI_CAMERA_FACING_DETAILS),"Facing",true,facing_details,true);
    if(!facing_details) return;
    const char* id=facing_target();
    const LayoutEntityInfo* info=id ? Layout_EntityInfo(&s->layout.objectStore,id) : NULL;
    SDL_Rect r=line(camera_rows()+1); r.w=r.w*2/3-4;
    char label[192];
    snprintf(label,sizeof(label),"%s",info ? info->label[0] ? info->label : id : "Select an object");
    paint(renderer,r,label,false,false,true);
    const LayoutFurnitureUnit* unit=Layout_FurnitureForObject(&s->layout.objectStore,s->editor.selectedObject3DId);
    paint(renderer,rect(UI_CAMERA_FACING_SCOPE),unit && !facing_part ? "Unit" : "Part",true,facing_part,unit!=NULL);
    const char* names[]={"Inherit","Solid","+X","-X","+Y","-Y","+Z","-Z"};
    LayoutInspectionFacing facing=Layout_InspectionFacing(info);
    for(int a=UI_CAMERA_FACING_INHERIT;a<=UI_CAMERA_FACING_NEG_Z;++a)
        paint(renderer,rect(a),names[a-UI_CAMERA_FACING_INHERIT],true,
            info && facing==(LayoutInspectionFacing)(a-UI_CAMERA_FACING_INHERIT),info!=NULL);
    paint(renderer,line(camera_rows()+6),facing_message[0] ? facing_message : "Local axes; Undo restores facing.",false,false,true);
}
void UIPanel_RenderCamera(SDL_Renderer *renderer) {
    if (!renderer || !active())
        return;
    const GlobalState *s = Global_Get();
    const CameraViewSession *c = &s->cameraView;
    char label[192];
    SDL_Rect r = line(0);
    r.w = r.w * 2 / 3 - 6;
    const LineDrawingSceneCamera *selected = c->selected < s->layout.sceneAuthoring.camera_count
                                                 ? &s->layout.sceneAuthoring.cameras[c->selected]
                                                 : NULL;
    const LineDrawingSceneCamera *source=Layout_SceneAuthoringState_FindCameraByIdConst(&s->layout.sceneAuthoring,c->source_id);
    snprintf(label, sizeof(label), "%s: %s%s", c->active ? "Viewing" : "Camera",
             c->active && !c->source_id[0] ? "temporary"
             : c->active && source         ? source->label
             : selected                    ? selected->label
                                           : "none saved", c->path_modified ? " *" : "");
    paint(renderer, r, label, false, false, true);
    const char *names[] = {"<",      ">",       "Explore",   "Enter",     "Exit",  "Save new",
                           "Update", "Details", "Alt: Look", "Alt: Roll", "Level", "Forward W",
                           "Back S", "Left A",  "Right D",   "Down Q",    "Up E"};
    for (int a = UI_CAMERA_PREVIOUS; a <= UI_CAMERA_UP; ++a) {
        bool enabled = a == UI_CAMERA_ENTER    ? selected != NULL
                       : a == UI_CAMERA_EXIT   ? c->active
                       : a == UI_CAMERA_UPDATE ? update_allowed()
                                               : true;
        paint(renderer, rect(a), names[a - 1], true,
              (a == UI_CAMERA_DETAILS && details) || (a == UI_CAMERA_LOOK && !c->roll_mouse) ||
                  (a == UI_CAMERA_ROLL && c->roll_mouse),
              enabled);
    }
    render_inspection(renderer);
    UIPanel_CameraPathRender(renderer,path_y());
    if (!c->active) {
        if(!CameraPath_Selected(s)) paint(renderer, line(2), "Explore from origin; Enter uses saved camera.", false, false, true);
        return;
    }
    paint(renderer, line(6), "Alt + mouse: look; Alt + Shift: roll", false, false, true);
    paint(renderer, line(7), "Move over view to navigate. Esc: exit.", false, false, true);
    snprintf(label, sizeof(label), "Speed %.3g m/s | FOV %.3g deg", c->speed_mps, c->fov);
    paint(renderer, line(8), label, false, false, true);
    const char *fields[] = {"X", "Y", "Z", "Yaw", "Pitch", "Roll", "FOV", "m/s", "Near m", "Far m"};
    for (int a = UI_CAMERA_X; a <= UI_CAMERA_FAR; ++a) {
        double n = value(a);
        if (a <= UI_CAMERA_Z)
            (void)core_units_convert(n, CORE_UNIT_METER, UIPanel_GetDisplayUnit(), &n);
        if (input == a)
            snprintf(label, sizeof(label), "%s: %s |", fields[a - UI_CAMERA_X], text);
        else
            snprintf(label, sizeof(label), "%s: %.5g%s", fields[a - UI_CAMERA_X], n,
                     a <= UI_CAMERA_Z ? UIPanel_GetDisplayUnitSymbol() : "");
        paint(renderer, rect(a), label, true, input == a, true);
    }
    if (c->message[0])
        paint(renderer, line(details ? 13 : 9), c->message, false, false, true);
}
bool UIPanel_CameraClick(int x, int y) {
    if (!active() || !contains(UIPanel_Get()->rightBodyRect, x, y))
        return false;
    GlobalState *s = Global_Get();
    CameraViewSession *c = &s->cameraView;
    if(UIPanel_CameraPathClick(x,y,path_y())) { stop_input(); return true; }
    UIPanel_CameraPathReset();
    int action = 0;
    for (int a = UI_CAMERA_PREVIOUS; a <= UI_CAMERA_FACING_NEG_Z; ++a)
        if (contains(rect(a), x, y)) {
            action = a;
            break;
        }
    if (!action)
        return false;
    stop_input();
    CameraView_ResetInput(s);
    if(action==UI_CAMERA_INSPECTION) {
        s->inspectionView=!s->inspectionView;
        if(s->inspectionView) { s->spaceMode=SPACE_MODE_3D; s->previewMode=LINE_DRAWING_PREVIEW_MODE_MATERIAL; }
        Global_FlagGridChanged();
    } else if(action==UI_CAMERA_FACING_DETAILS) facing_details=!facing_details;
    else if(action==UI_CAMERA_FACING_SCOPE) { facing_part=!facing_part; facing_message[0]=0; }
    else if(action>=UI_CAMERA_FACING_INHERIT) {
        const char* id=facing_target();
        bool ok=id && Layout_SetInspectionFacing(&s->layout,id,
            (LayoutInspectionFacing)(action-UI_CAMERA_FACING_INHERIT),Layout_GeometryHistory,NULL);
        /* The Saved/Dirty and Undo header remains authoritative after history changes. */
        snprintf(facing_message,sizeof(facing_message),"%s",ok ? "" :
            id ? s->layout.geometryMessage : "Select an object first.");
    } else if (action == UI_CAMERA_PREVIOUS || action == UI_CAMERA_NEXT) {
        size_t n = s->layout.sceneAuthoring.camera_count;
        s->layout.sceneAuthoring.selected_kind=LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE;
        if (n)
            c->selected = (c->selected + n + (action == UI_CAMERA_NEXT ? 1 : n - 1)) % n;
    } else if (action == UI_CAMERA_EXPLORE)
        CameraView_Enter(s, false);
    else if (action == UI_CAMERA_ENTER) {
        if (s->layout.sceneAuthoring.camera_count)
            CameraView_Enter(s, true);
    } else if (action == UI_CAMERA_EXIT)
        CameraView_Exit(s);
    else if (action == UI_CAMERA_SAVE)
        CameraView_Save(s, false);
    else if (action == UI_CAMERA_UPDATE) {
        if (update_allowed())
            CameraView_Save(s, true);
    } else if (action == UI_CAMERA_DETAILS)
        details = !details;
    else if (action == UI_CAMERA_LOOK)
        c->roll_mouse = false;
    else if (action == UI_CAMERA_ROLL)
        c->roll_mouse = true;
    else if (action == UI_CAMERA_LEVEL) {
        CameraPath_NavigationChanged(s); c->roll = 0;
    }
    else if (action >= UI_CAMERA_FORWARD && action <= UI_CAMERA_UP) {
        const unsigned bits[] = {1, 2, 4, 8, 16, 32};
        c->held = bits[action - UI_CAMERA_FORWARD];
        c->focused = true;
        float speed = c->speed_mps;
        c->speed_mps = 2;
        (void)CameraView_Step(s, .05f);
        c->speed_mps = speed;
        CameraView_ResetInput(s);
    } else if (action >= UI_CAMERA_X && action <= UI_CAMERA_FAR) {
        input = action;
        replace = true;
        snprintf(text, sizeof(text), "%.8g", value(action));
        if (action <= UI_CAMERA_Z) {
            double n = value(action);
            (void)core_units_convert(n, CORE_UNIT_METER, UIPanel_GetDisplayUnit(), &n);
            snprintf(text, sizeof(text), "%.8g", n);
        }
        SDL_StartTextInput();
    }
    refresh();
    return true;
}
bool UIPanel_CameraCapturingKeyboard(void) { return active() && (input != 0 || UIPanel_CameraPathCapturing()); }
void UIPanel_CameraReset(void) { stop_input(); UIPanel_CameraPathReset(); }
static void apply_input(void) {
    GlobalState *s = Global_Get();
    CameraViewSession *c = &s->cameraView;
    double n;
    char *end = NULL;
    bool ok = input <= UI_CAMERA_Z
                  ? Editor_ParseLength(text, UIPanel_GetDisplayUnit(), &n)
                  : ((n = strtod(text, &end)), end != text && *end == 0 && isfinite(n));
    if (!ok) {
        snprintf(c->message, sizeof(c->message),
                 "Enter a finite value; XYZ accepts unit suffixes.");
        return;
    }
    if ((input == UI_CAMERA_FOV && (n < 1 || n > 179)) ||
        (input == UI_CAMERA_SPEED && (n <= 0 || n > 20)) ||
        (input == UI_CAMERA_NEAR && (n <= 0 || n >= c->far_clip)) ||
        (input == UI_CAMERA_FAR && n <= c->near_clip) ||
        (input == UI_CAMERA_PITCH && fabs(n) > 89.5) || fabs(n) > 1e6) {
        snprintf(c->message, sizeof(c->message), "Value outside camera range.");
        return;
    }
    if(input!=UI_CAMERA_SPEED) CameraPath_NavigationChanged(s);
    switch (input) {
    case UI_CAMERA_X:
        c->eye.x = (float)(n / s->layout.metersPerWorldUnit);
        break;
    case UI_CAMERA_Y:
        c->eye.y = (float)(n / s->layout.metersPerWorldUnit);
        break;
    case UI_CAMERA_Z:
        c->eye.z = (float)(n / s->layout.metersPerWorldUnit);
        break;
    case UI_CAMERA_YAW:
        c->yaw = Angle_NormalizeSignedDeg((float)n);
        break;
    case UI_CAMERA_PITCH:
        c->pitch = (float)n;
        break;
    case UI_CAMERA_ROLL_VALUE:
        c->roll = Angle_NormalizeSignedDeg((float)n);
        break;
    case UI_CAMERA_FOV:
        c->fov = (float)n;
        break;
    case UI_CAMERA_SPEED:
        c->speed_mps = (float)n;
        break;
    case UI_CAMERA_NEAR:
        c->near_clip = (float)n;
        break;
    case UI_CAMERA_FAR:
        c->far_clip = (float)n;
        break;
    default:
        break;
    }
    c->message[0] = 0;
    stop_input();
    refresh();
}
bool UIPanel_CameraEvent(const SDL_Event *e) {
    if (!e)
        return false;
    if (!active()) {
        if (input)
            stop_input();
        return false;
    }
    if(UIPanel_CameraPathEvent(e,path_y())) return true;
    if (input)
        CameraView_ResetInput(Global_Get());
    if (input && e->type == SDL_TEXTINPUT) {
        if (replace) {
            text[0] = 0;
            replace = false;
        }
        size_t n = strlen(text);
        snprintf(text + n, sizeof(text) - n, "%s", e->text.text);
        return true;
    }
    if (input && e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_KP_ENTER)
            apply_input();
        else if (e->key.keysym.sym == SDLK_ESCAPE)
            stop_input();
        else if (e->key.keysym.sym == SDLK_a && e->key.keysym.mod & (KMOD_CTRL | KMOD_GUI))
            replace = true;
        else if (e->key.keysym.sym == SDLK_BACKSPACE) {
            size_t n = strlen(text);
            if (replace) {
                text[0] = 0;
                replace = false;
            } else if (n)
                text[n - 1] = 0;
        }
        return true;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
        return UIPanel_CameraClick(e->button.x, e->button.y);
    return false;
}
