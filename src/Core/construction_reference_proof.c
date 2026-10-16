#include "Core/construction_reference_proof.h"
#include "Core/construction_reference_export.h"
#include "Core/global_state.h"
#include "Layout/layout_json.h"
#include "Editor/editor.h"
#include "UI/panel/ui_panel_collection_inspection.h"
#include "UI/ui_panel_measurement.h"
#include "vk_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool click(AppContext* app,const AppCallbacks* cb,int action){SDL_Rect box;if(!UIPanel_MeasurementControlRect(action,&box))return false;UIPanelState* ui=UIPanel_Get();ui->rightScroll[UI_PANEL_RIGHT_TAB_MEASURE].scrollOffsetPx+=box.y-ui->rightBodyRect.y-8;UIPanel_CollectionInspectionLayout();if(!UIPanel_MeasurementControlRect(action,&box))return false;SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=box.x+box.w/2;e.button.y=box.y+box.h/2;cb->handleInput(app,&e);return true;}
bool ConstructionReferenceProof_Run(AppContext* app,const AppCallbacks* cb,const char* root){GlobalState* g=Global_Get();Editor_ClearHistory(&g->editor);char* before=Layout_SaveToString(&g->layout);snprintf(g->dataPaths.output_root,sizeof(g->dataPaths.output_root),"%s",root);
    bool ok=before&&UIPanel_BeginCollectionInspection("@scene.driver")&&click(app,cb,INSPECT_FOCUS)&&click(app,cb,INSPECT_EXPORT)&&ConstructionReference_LastDirectory()[0];
    char first[4096];snprintf(first,sizeof(first),"%s",ConstructionReference_LastDirectory());ok=ok&&click(app,cb,INSPECT_EXPORT)&&strcmp(first,ConstructionReference_LastDirectory());
    char* after=Layout_SaveToString(&g->layout);bool unchanged=before&&after&&!strcmp(before,after)&&Editor_UndoCount(&g->editor)==0;ok=ok&&unchanged;free(before);free(after);
    for(int i=0;i<24&&ok;++i){cb->handleUpdate(app);ok=App_RenderOnce(app,cb->handleRender);SDL_Delay(10);}char path[4096];snprintf(path,sizeof(path),"%s/construction_reference.bmp",root);ok=ok&&vk_renderer_request_capture(app->renderer,path)==VK_SUCCESS&&App_RenderOnce(app,cb->handleRender);
    snprintf(path,sizeof(path),"%s/construction_reference_proof.json",root);FILE* f=fopen(path,"wx");if(!f)return false;fprintf(f,"{\"passed\":%s,\"scene_and_history_unchanged\":%s,\"native_button_exports\":2}\n",ok?"true":"false",unchanged?"true":"false");fclose(f);return ok;}
