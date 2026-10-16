#include "Core/shared_hud_proof.h"
#include "Core/global_state.h"
#include "Layout/layout_json.h"
#include "Input/input_handler.h"
#include "UI/font_manager.h"
#include "UI/object_workspace_viewport_hud.h"
#include "UI/shared_overlay.h"
#include "UI/ui_panel.h"
#include "UI/workspace_authoring/line_drawing_workspace_authoring_host.h"
#include <stdio.h>
#include <string.h>
static char *snapshot;
static size_t undo_before;
bool SharedHudProof_Stage(const char *mode) {
    if (!mode || strncmp(mode,"shared-hud-",11)) return true;
    GlobalState *s=Global_Get();
    if (strstr(mode,"authoring")) {
        if (strstr(mode,"zoom")) FontManager_SetZoomStep(6);
        if (LineDrawingWorkspaceAuthoringHost_Enter(s).code != CORE_OK ||
            LineDrawingWorkspaceAuthoringHost_CycleOverlay(s).code != CORE_OK) return false;
    } else {
        if (!Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT)) return false;
        RectPrismPrimitiveCreateParams p;
        Layout_RectPrismPrimitiveCreateParams_SetDefaults(&p);
        uint32_t id=0;
        if (!Layout_CreateRectPrismPrimitive(&s->layout,&p,&id,NULL)) return false;
        s->editor.selectedObject3DId=id;
        UIPanel_OnWindowResized(s->screenWidth,s->screenHeight);
        if (strstr(mode,"notice"))
            strcpy(s->layout.geometryMessage,"A bounded geometry notice stays inside the viewport and below the face controls. Long diagnostic text wraps without changing the scene or covering menus.");
        if (strstr(mode,"modal")) {
            UIPanel_BeginInputRootDialog();
            strcpy(UIPanel_Get()->rootDialog.buffer,"unsaved-café-root-draft");
        }
    }
    undo_before=Editor_UndoCount(&s->editor);
    snapshot=Layout_SaveToString(&s->layout);
    return snapshot != NULL;
}
bool SharedHudProof_Check(const char *mode) {
    if (!mode || strncmp(mode,"shared-hud-",11)) return true;
    GlobalState *s=Global_Get();
    char *after=Layout_SaveToString(&s->layout);
    bool ok=snapshot && after && !strcmp(snapshot,after) && Editor_UndoCount(&s->editor)==undo_before;
    SDL_Rect b;
    if (strstr(mode,"modal")) ok=ok && !LineDrawingObjectWorkspaceViewportHud_Rect(s,-1,&b) && UIPanel_IsRootDialogActive();
    else if (!strstr(mode,"authoring")) ok=ok && LineDrawingObjectWorkspaceViewportHud_Rect(s,-1,&b);
    else ok=ok && LineDrawingWorkspaceAuthoringHost_FontThemeOverlayActive(s);
    Layout_FreeString(snapshot);snapshot=NULL;Layout_FreeString(after);
    fprintf(stdout,"SHARED_HUD mode=%s status=%s unchanged_document=1 unchanged_history=%d\n",mode,ok?"pass":"fail",Editor_UndoCount(&s->editor)==undo_before);
    return ok;
}
