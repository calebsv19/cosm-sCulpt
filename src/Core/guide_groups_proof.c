#include "Core/guide_groups_proof.h"
#include "Core/global_state.h"
#include "Core/camera_view.h"
#include "Layout/layout_json.h"
#include "Layout/layout_saved_views.h"
#include "Layout/layout_guide_groups.h"
#include "UI/topbar/visibility_tree.h"
#include "UI/topbar/visibility_guide_groups.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/ui_panel_camera.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_shell.h"
#include "vk_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool capture(AppContext* app,const AppCallbacks* c,const char* root,const char* name){char p[4096];snprintf(p,sizeof(p),"%s/%s.bmp",root,name);c->handleUpdate(app);if(!App_RenderOnce(app,c->handleRender))return false;return vk_renderer_request_capture(app->renderer,p)==VK_SUCCESS&&App_RenderOnce(app,c->handleRender);}
static bool click(AppContext* app,const AppCallbacks* c,int action){LineDrawingEditorTopbar_UpdateControls();SDL_Rect r;if(!LDViewControls_Rect(action,&r))return false;SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=r.x+2;e.button.y=r.y+2;c->handleInput(app,&e);return true;}
bool GuideGroupsProof_Run(AppContext* app,const AppCallbacks* c,const char* root){
    GlobalState* s=Global_Get();LayoutObjectStore* store=&s->layout.objectStore;char* before=Layout_SaveToString(&s->layout);size_t history=Editor_UndoCount(&s->editor);bool helpers=s->authoringGuides,bounds=s->layout.scene3d.bounds.enabled;bool ok=true;size_t guides=0,parented=0,hidden=0,categories[8]={0};
    LDViewControls_Close();LDVisibilityTree_Reset();CameraView_Exit(s);s->spaceMode=SPACE_MODE_3D;Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_MATERIAL);s->layout.scene3d.bounds.enabled=true;LDVisibilityTree_Build(store,"");
    ok=ok&&LDVisibilityTree_Hide(store,"@guides",false)&&capture(app,c,root,"functional_guides_on");
    UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_VIEW);UIPanel_OnWindowResized(s->screenWidth,s->screenHeight);c->handleUpdate(app);ok=ok&&App_RenderOnce(app,c->handleRender);SDL_Rect rect;
    ok=ok&&UIPanel_CameraControlRect(UI_CAMERA_GUIDES,&rect)&&UIPanel_CameraClick(rect.x+2,rect.y+2)&&capture(app,c,root,"functional_guides_off");
    for(size_t i=0;i<store->count;++i)if(!store->items[i].isDeleted&&LDVisibilityGuide_Category(store,&store->items[i])){++guides;++categories[LDVisibilityGuide_Category(store,&store->items[i])];if(store->items[i].info.parent_id[0])++parented;if(!Layout_ObjectShown(store,&store->items[i]))++hidden;}
    ok=ok&&guides>0&&guides==hidden&&!s->authoringGuides;
    /* The authored opening markers/reservations used to escape this switch. */
    const char* ids[]={"obj3d_674","obj3d_676","obj3d_698","obj3d_645","af1_drawer_fixed_attachment"};
    const int expected[]={1,1,5,7,7};
    for(int n=0;n<5;++n)for(size_t i=0;i<store->count;++i)if(!strcmp(store->items[i].coreMeta.object_id,ids[n]))ok=ok&&LDVisibilityGuide_Category(store,&store->items[i])==expected[n]&&!Layout_ObjectShown(store,&store->items[i]);
    ok=ok&&click(app,c,LD_VIEW_VISIBILITY);size_t rows=LDVisibilityTree_Build(store,"");int row=-1;for(size_t i=0;i<rows;++i){LDVisibilityRow item;LDVisibilityTree_Row(i,&item);if(!strcmp(item.id,"@guides")){row=(int)i;break;}}
    ok=ok&&row>=0&&click(app,c,LD_VIEW_ROW_EXPAND+row)&&capture(app,c,root,"functional_groups");
    /* The header eye shares the side-panel authority and reveals the same members. */
    ok=ok&&click(app,c,LD_VIEW_ROW+row);LDViewControls_Close();ok=ok&&capture(app,c,root,"functional_guides_header_on");
    for(size_t i=0;i<store->count;++i)if(!store->items[i].isDeleted&&LDVisibilityGuide_Category(store,&store->items[i]))ok=ok&&Layout_ObjectShown(store,&store->items[i]);
    size_t preset_checks=0;
    for(size_t v=0;v<store->saved_view_count;++v) {
        const LayoutSavedView* view=&store->saved_views[v];if(!Layout_IsGuideView(view))continue;
        Layout_ToggleView(store,view->id);
        for(size_t i=0;i<store->count;++i) if(!store->items[i].isDeleted && Layout_GuideViewMatches(store,&store->items[i],view)) {
            ++preset_checks;ok=ok&&!Layout_ObjectShown(store,&store->items[i]);
        }
        Layout_ToggleView(store,view->id);
    }
    ok=ok&&preset_checks>0&&s->authoringGuides;LDVisibilityTree_Restore(store);s->authoringGuides=helpers;
    s->layout.scene3d.bounds.enabled=bounds;char* after=Layout_SaveToString(&s->layout);ok=ok&&before&&after&&!strcmp(before,after)&&history==Editor_UndoCount(&s->editor);free(before);free(after);
    char path[4096];snprintf(path,sizeof(path),"%s/functional_guides.json",root);FILE* f=fopen(path,"wx");if(!f)return false;fprintf(f,"{\"passed\":%s,\"guides\":%zu,\"preset_exclusion_checks\":%zu,\"assembly_owned_guides\":%zu,\"hidden_after_side_panel\":%zu,\"header_reveals_same_members\":%s,\"document_history_unchanged\":%s,\"category_counts\":[%zu,%zu,%zu,%zu,%zu,%zu,%zu]}\n",ok?"true":"false",guides,preset_checks,parented,hidden,ok?"true":"false",ok?"true":"false",categories[1],categories[2],categories[3],categories[4],categories[5],categories[6],categories[7]);return !fclose(f)&&ok;
}
