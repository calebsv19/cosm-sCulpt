#pragma once
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "kit_ui_text_edit_sdl.h"
#include "kit_ui_text_presentation.h"

typedef struct LDTextField {
    KitUiTextEdit edit;
    KitUiTextPresentation view;
    SDL_Renderer* renderer;
    TTF_Font* font;
    SDL_Rect bounds;
    uint64_t fingerprint;
    size_t expected_cursor;
    bool ready, dragging;
} LDTextField;
void LDText_Bind(LDTextField*, char*, size_t, size_t);
void LDText_CancelCapture(LDTextField*);
KitUiTextEventResult LDText_Event(LDTextField*, const SDL_Event*);
void LDText_Draw(LDTextField*, SDL_Renderer*, TTF_Font*, SDL_Rect, SDL_Color, bool);
int LDText_Label(SDL_Renderer*, TTF_Font*, const char*, SDL_Rect, SDL_Color, bool);
int LDText_LabelRows(SDL_Renderer*, TTF_Font*, const char*, SDL_Rect, SDL_Color, int);
int LDText_CountLines(TTF_Font*, const char*, int);
/* Dialog adapters retain app validation/commit ownership. */
bool UIPanel_SharedTextEvent(const SDL_Event*);
bool UIPanel_SharedTextInsert(const char*);
void UIPanel_SharedTextDraw(SDL_Renderer*, SDL_Rect, SDL_Color);
void UIPanel_SharedTextCancelCapture(void);
void UIPanel_SharedTextReset(void);

const KitUiTextPresentation* UIPanel_SharedTextPresentation(void);
