#pragma once
#include "UI/shared_controls.h"
#include "UI/shared_text.h"
typedef struct LDInlineField {
    char *text;
    size_t capacity;
    bool *replace;
    SDL_Rect rect;
    bool *edited;
} LDInlineField;
void LDSpecial_Collect(KitUiSurface *);
void LDSpecial_Record(unsigned, int, const char *, SDL_Rect, const SDL_Rect *, bool);
bool LDSpecial_Dispatch(int, int);
bool LDInline_Event(const SDL_Event *);
void LDInline_Draw(SDL_Renderer *);
void LDInline_Cancel(void);
void LDSpecial_Parts(void);
void LDSpecial_Routes(void);
void LDSpecial_Measure(void);
void LDSpecial_Inspection(void);
void LDSpecial_SceneList(void);
void LDSpecial_Visibility(void);
bool LDInline_Camera(LDInlineField *);
bool LDInline_Path(LDInlineField *);
bool LDInline_Section(LDInlineField *);
bool LDInline_Furniture(LDInlineField *);
bool LDInline_Parts(LDInlineField *);
bool LDInline_Routes(LDInlineField *);
bool LDInline_Inspection(LDInlineField *);
bool LDInline_Visibility(LDInlineField *);

void LDSpecial_ObjectTree(void);
void LDSpecial_Generic(void);
bool LDInline_Active(void);
SDL_Rect LDInline_ValueRect(SDL_Rect, const char *);
void LDInline_VisibilityChanged(void);

void LDSpecial_Camera(void);

void LDSpecial_Path(void);

const KitUiTextPresentation *LDInline_Presentation(void);

void LDSpecial_Section(void);
