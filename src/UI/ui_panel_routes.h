#pragma once
#include "UI/ui_panel.h"
typedef enum {
    ROUTES_SELECT=1, ROUTES_NEW, ROUTES_NAME, ROUTES_CABLE, ROUTES_PIPE,
    ROUTES_ENDPOINTS, ROUTES_SOURCE, ROUTES_DESTINATION, ROUTES_USE_SOURCE, ROUTES_USE_DESTINATION,
    ROUTES_PREVIOUS, ROUTES_NEXT, ROUTES_X, ROUTES_Y, ROUTES_Z, ROUTES_INSERT,
    ROUTES_DELETE_POINT, ROUTES_EARLIER, ROUTES_LATER, ROUTES_PICK,
    ROUTES_REFRESH, ROUTES_SAVE, ROUTES_CANCEL, ROUTES_REMOVE, ROUTES_CONFIRM_REMOVE,
    ROUTES_CHOICE_BASE=1000
} UIPanelRoutesAction;
void UIPanel_LayoutRoutes(void);
void UIPanel_RenderRoutes(SDL_Renderer* renderer);
void UIPanel_RenderRouteViewport(SDL_Renderer* renderer);
void UIPanel_RoutesStopInput(void);
bool UIPanel_RoutesEvent(const SDL_Event* event);
bool UIPanel_RoutesControlRect(int action, SDL_Rect* rect);
