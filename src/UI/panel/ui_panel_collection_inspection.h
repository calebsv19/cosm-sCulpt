#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>
enum {
    INSPECT_AREA=700,INSPECT_FOCUS,INSPECT_UNITS,INSPECT_FRAME_PICK,INSPECT_FRAME_WORLD,
    INSPECT_PART,INSPECT_SET_A,INSPECT_SET_B,INSPECT_FEATURE_A,INSPECT_FEATURE_B,
    INSPECT_KIND,INSPECT_NAME,INSPECT_SAVE,INSPECT_NEW,INSPECT_DELETE,INSPECT_SAVED,INSPECT_BACK,INSPECT_FRAME_PARTS=718,INSPECT_EXPORT=719,INSPECT_OPEN_REFERENCE=720,
    INSPECT_AREA_BASE=1000,INSPECT_PART_BASE=2000,INSPECT_SAVED_BASE=1000000
};
bool UIPanel_BeginCollectionInspection(const char* collection);
bool UIPanel_CollectionInspectionActive(void);
void UIPanel_CollectionInspectionClose(void);
void UIPanel_CollectionInspectionLayout(void);
void UIPanel_CollectionInspectionRender(SDL_Renderer* renderer);
bool UIPanel_CollectionInspectionClick(int x,int y);
bool UIPanel_CollectionInspectionRect(int action,SDL_Rect* out);
bool UIPanel_CollectionInspectionText(const char* text);
bool UIPanel_CollectionInspectionKey(SDL_Keycode key);
void UIPanel_CollectionInspectionStopInput(void);
bool UIPanel_CollectionInspectionHasInput(void);
bool UIPanel_CollectionInspectionInputEvent(const SDL_Event* event);

void UIPanel_CollectionInspectionViewport(SDL_Renderer* renderer);
