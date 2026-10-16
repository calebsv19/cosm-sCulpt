#include "UI/shared_text.h"
#include "UI/ui_panel_internal.h"
#include "UI/font_manager.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct DialogField {
    char* buffer;size_t capacity;size_t* cursor;size_t* length;
    int policy;
    bool (*apply)(UIPanelState*);void (*close)(UIPanelState*);
} DialogField;
enum { TEXT_ANY, TEXT_SAVE, TEXT_BUNDLE, TEXT_NUMBER, TEXT_VEC3, TEXT_PATH };
static LDTextField field;
void UIPanel_SharedTextReset(void) { memset(&field,0,sizeof(field)); }
void UIPanel_SharedTextCancelCapture(void) { LDText_CancelCapture(&field); }
static DialogField current(void) {
    UIPanelState* ui=UIPanel_Get();DialogField f={0};
#define FIELD(member, rule, apply_fn, close_fn) do { f=(DialogField){ui->member.buffer,sizeof(ui->member.buffer),&ui->member.cursor,&ui->member.length,rule,apply_fn,close_fn}; } while(0)
    if(ui->scenePropertyDialog.active) {
        int policy=ui->scenePropertyDialog.target==UI_SCENE_PROPERTY_DIALOG_LABEL?TEXT_ANY:
            ui->scenePropertyDialog.target==UI_SCENE_PROPERTY_DIALOG_LIGHT_PATH?TEXT_PATH:TEXT_VEC3;
        FIELD(scenePropertyDialog,policy,UIPanel_ApplyScenePropertyDialog,UIPanel_CloseScenePropertyDialog);
    } else if(ui->saveDialog.active) FIELD(saveDialog,ui->saveDialog.mode==UI_SAVE_DIALOG_SCENE_BUNDLE?TEXT_BUNDLE:TEXT_SAVE,UIPanel_PerformSave,UIPanel_CloseSaveDialog);
    else if(ui->rootDialog.active) FIELD(rootDialog,TEXT_ANY,UIPanel_ApplyRootDialog,UIPanel_CloseRootDialog);
    else if(ui->prismDimensionDialog.active) FIELD(prismDimensionDialog,TEXT_ANY,UIPanel_ApplyPrismDimensionDialog,UIPanel_ClosePrismDimensionDialog);
    else if(ui->sceneBoundsDialog.active) FIELD(sceneBoundsDialog,TEXT_VEC3,UIPanel_ApplySceneBoundsDialog,UIPanel_CloseSceneBoundsDialog);
    else if(ui->constructionPlaneDialog.active) FIELD(constructionPlaneDialog,TEXT_NUMBER,UIPanel_ApplyConstructionPlaneDialog,UIPanel_CloseConstructionPlaneDialog);
    else if(ui->objectTransformDialog.active) FIELD(objectTransformDialog,ui->objectTransformDialog.target==UI_OBJECT_TRANSFORM_DIALOG_TARGET_POSITION?TEXT_VEC3:TEXT_NUMBER,UIPanel_ApplyObjectTransformDialog,UIPanel_CloseObjectTransformDialog);
#undef FIELD
    return f;
}
static bool allowed(int policy,unsigned char c) {
    if(policy==TEXT_ANY)return c>=32 && c!=127;
    if(policy==TEXT_SAVE || policy==TEXT_BUNDLE)return isalnum(c)||c=='_'||c=='-'||c==' '||(c=='.' && policy==TEXT_SAVE);
    if(policy==TEXT_PATH)return isalnum(c)||c=='_'||c=='-';
    return isdigit(c)||c=='.'||c=='-'||c=='+'||c=='e'||c=='E'||(policy==TEXT_VEC3 && (c==','||isspace(c)));
}
static void sync(DialogField f) {
    *f.cursor=field.edit.cursor;*f.length=strlen(f.buffer);
    /* Rebind observes the updated bytes without losing shared selection. */
    LDText_Bind(&field,f.buffer,f.capacity,*f.cursor);
}
static void insert(DialogField f,const char* text) {
    size_t len=strlen(text);char* filtered=malloc(len+1);if(!filtered)return;
    size_t used=0;for(size_t i=0;i<len;++i)if(allowed(f.policy,(unsigned char)text[i]))filtered[used++]=text[i];
    filtered[used]=0;
    (void)kit_ui_text_insert(&field.edit,filtered);free(filtered);
    /* Keep selection metadata after insertion; acknowledge our own buffer edit. */
    field.expected_cursor=field.edit.cursor;
    *f.cursor=field.edit.cursor;*f.length=strlen(f.buffer);
    LDText_Bind(&field,f.buffer,f.capacity,*f.cursor);
}
bool UIPanel_SharedTextInsert(const char* text) {
    DialogField f=current();if(!f.buffer || !text)return false;
    LDText_Bind(&field,f.buffer,f.capacity,*f.cursor);insert(f,text);
    UIPanel_Get()->scenePropertyDialog.validationMessage[0]=0;return true;
}
bool UIPanel_SharedTextEvent(const SDL_Event* event) {
    DialogField f=current();if(!f.buffer)return false;
    LDText_Bind(&field,f.buffer,f.capacity,*f.cursor);
    if(event->type==SDL_TEXTINPUT)return UIPanel_SharedTextInsert(event->text.text);
    if(event->type==SDL_KEYDOWN && (event->key.keysym.mod&(KMOD_CTRL|KMOD_GUI)) && event->key.keysym.sym==SDLK_v) {
        if(field.edit.composition[0])return true;
        if(!event->key.repeat) {char* text=SDL_GetClipboardText();if(text){insert(f,text);SDL_free(text);}}
        return true;
    }
    KitUiTextEventResult result=LDText_Event(&field,event);sync(f);
    if(result.submit) { (void)f.apply(UIPanel_Get()); if(!current().buffer)UIPanel_SharedTextReset(); }
    if(result.cancel) { f.close(UIPanel_Get());UIPanel_SharedTextReset(); }
    return result.consumed || event->type==SDL_KEYDOWN || event->type==SDL_KEYUP;
}
void UIPanel_SharedTextDraw(SDL_Renderer* renderer,SDL_Rect rect,SDL_Color color) {
    DialogField f=current();if(!f.buffer)return;
    LDText_Bind(&field,f.buffer,f.capacity,*f.cursor);
    LDText_Draw(&field,renderer,FontManager_Get(FONT_DEFAULT),
        (SDL_Rect){rect.x+8,rect.y+6,rect.w-16,rect.h-12},color,true);
}

const KitUiTextPresentation* UIPanel_SharedTextPresentation(void) { return field.ready ? &field.view : NULL; }
