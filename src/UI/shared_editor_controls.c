#include "UI/shared_special.h"
#include "UI/shared_controls.h"
#include "Core/global_state.h"
#include "UI/ui_panel.h"
#include "UI/input_ui_panel.h"
#include "UI/ui_panel_shell.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "UI/topbar/line_drawing_view_controls.h"

static KitUiSurface editor;
void LDControls_CancelAll(void) {
    LDControls_Cancel(LDControls_Menu());
    LDControls_Cancel(&editor);
}
static bool collect(void) {
    UIPanelState* ui = UIPanel_Get();
    /* Modal and editable text owners keep their existing event precedence. */
    bool blocked = UIPanel_IsSaveDialogActive() || UIPanel_IsRootDialogActive() ||
        UIPanel_IsPrismDimensionDialogActive() || UIPanel_IsSceneBoundsDialogActive() ||
        UIPanel_IsConstructionPlaneDialogActive() || UIPanel_IsObjectTransformDialogActive() ||
        UIPanel_IsScenePropertyDialogActive();
    int popup = LDViewControls_ActivePopup();

    unsigned scope = (unsigned)popup*4096u + 1u + (unsigned)Global_GetWorkspaceMode()*256u +
        (unsigned)ui->activeRightTab*16u + (unsigned)ui->activeLeftTab*4u + (ui->contextMenuOpen ? 2u : 0u);
    kit_ui_surface_begin(&editor, blocked ? 0u : scope);
    if (!blocked) {
        if (!ui->contextMenuOpen && !popup ) LineDrawingEditorTopbar_Collect(&editor);
        if (!popup) UIPanel_CollectShellControls(&editor);
        if (popup!=LD_VIEW_VISIBILITY && popup) {
            int first=popup==LD_VIEW_APPEARANCE ? LD_VIEW_WIRE : LD_VIEW_EXPLORE;
            int last=popup==LD_VIEW_APPEARANCE ? LD_VIEW_EDGES : LD_VIEW_MATERIAL;
            for (int i=first; i<=last; ++i) {
                SDL_Rect rect;
                if (LDViewControls_Rect(i,&rect)) LDControls_Add(&editor,6,(uint64_t)i,rect,NULL,
                    i!=LD_VIEW_SAVED || Global_Get()->layout.sceneAuthoring.camera_count>0);
            }
        }
    }
    if(!blocked && !ui->contextMenuOpen && (!popup || popup==LD_VIEW_VISIBILITY)) LDSpecial_Collect(&editor);
    return kit_ui_surface_end(&editor).code == CORE_OK;
}
bool LDControls_EditorEvent(const SDL_Event* event) {
    SDL_Point activate;
    if (event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_ESCAPE &&
        (LDViewControls_ActivePopup() || UIPanel_Get()->contextMenuOpen)) {
        LDControls_CancelAll();
        return false; /* The existing popup owner dismisses without losing the editor. */
    }
    if ((event->type==SDL_KEYDOWN || event->type==SDL_KEYUP) && UIPanel_IsCapturingKeyboard()) {
        LDControls_Cancel(&editor);
        return false;
    }
    if (!collect()) { LDControls_Cancel(&editor); return true; }
    if (!editor.count) { LDControls_Cancel(&editor); return false; }
    bool consumed = LDControls_Route(&editor, event, &activate);
    if (activate.x >= 0) {
        if (LDSpecial_Dispatch(activate.x,activate.y)) return true;
        if (LDViewControls_ActivePopup() && LDViewControls_Click(activate.x,activate.y)) return true;
        /* Dispatch once through the existing action owners, with current geometry. */
        {
            SDL_Event click = {.type=SDL_MOUSEBUTTONDOWN};
            click.button.button=SDL_BUTTON_LEFT; click.button.x=activate.x; click.button.y=activate.y;
            if (UIPanel_ContextEvent(&click)) return true;
        }
        if (!LineDrawingEditorTopbar_HandleClick(activate.x,activate.y))
            (void)UIPanel_HandleClick(activate.x,activate.y);
    }
    return consumed;
}
void LDControls_EditorDraw(SDL_Renderer* renderer) {
    LDInline_Draw(renderer);
    if (collect()) LDControls_Draw(renderer,&editor);
}
