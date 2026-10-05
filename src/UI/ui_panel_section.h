#pragma once
#include "UI/ui_panel.h"
enum {
    UI_SECTION_OFF = 1,
    UI_SECTION_EXACT,
    UI_SECTION_CUTAWAY,
    UI_SECTION_ALONG,
    UI_SECTION_ACROSS,
    UI_SECTION_HEIGHT,
    UI_SECTION_TRACK,
    UI_SECTION_POSITION,
    UI_SECTION_STEP,
    UI_SECTION_PREVIOUS,
    UI_SECTION_NEXT,
    UI_SECTION_FLIP,
    UI_SECTION_APPLY,
    UI_SECTION_CANCEL,
    UI_SECTION_SELECTED,
    UI_SECTION_ALL
};
void UIPanel_RenderSection(SDL_Renderer* renderer);
bool UIPanel_SectionClick(int x, int y);
bool UIPanel_SectionEvent(const SDL_Event* event);
bool UIPanel_SectionControlRect(int action, SDL_Rect* rect);
int UIPanel_SectionHeight(void);
