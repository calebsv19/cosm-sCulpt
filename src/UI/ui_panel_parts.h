#pragma once
#include "UI/ui_panel.h"

typedef enum {
    PARTS_OBJECTS=1, PARTS_ASSEMBLIES, PARTS_FILTERS, PARTS_SELECT,
    PARTS_NAME=10, PARTS_TYPE, PARTS_ROLE, PARTS_PARENT, PARTS_SAVE, PARTS_NEW,
    PARTS_PROPERTIES, PARTS_KEY, PARTS_VALUE, PARTS_PROPERTY_KIND, PARTS_SET_PROPERTY,
    PARTS_REMOVE_PROPERTY, PARTS_MOVE, PARTS_DX, PARTS_DY, PARTS_DZ, PARTS_ANGLE,
    PARTS_AXIS, PARTS_APPLY_MOVE, PARTS_DELETE, PARTS_CONFIRM_DELETE, PARTS_CANCEL,
    PARTS_CLEAR_FILTER, PARTS_APPLY_FILTER, PARTS_ADD_SELECTED
} UIPanelPartsAction;
void UIPanel_LayoutParts(void);
void UIPanel_RenderParts(SDL_Renderer* renderer);
bool UIPanel_PartsClick(int x, int y);
bool UIPanel_PartsEvent(const SDL_Event* event);
void UIPanel_PartsStopInput(void);
bool UIPanel_PartsControlRect(int action, SDL_Rect* rect);

bool UIPanel_PartsSelectEntity(const char* id);
