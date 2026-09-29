#pragma once
#include "UI/ui_panel.h"
bool UIPanel_BeginMeasurement(void);
bool UIPanel_MeasurementKey(SDL_Keycode key);
bool UIPanel_MeasurementClick(int x, int y);
void UIPanel_RenderMeasurement(SDL_Renderer* renderer);

bool UIPanel_MeasurementPickAt(int x, int y);
void UIPanel_RenderMeasurementViewport(SDL_Renderer* renderer);

bool UIPanel_MeasurementText(const char* text);

void UIPanel_RenderConstraintViewport(SDL_Renderer* renderer);

void UIPanel_MeasurementStopInput(void);
void UIPanel_MeasurementStartValue(int mode);
void UIPanel_MeasurementApplyButton(int mode);
void UIPanel_MeasurementSelectRule(int index);
void UIPanel_LayoutMeasurementPane(void);
/* Semantic control IDs also support mouse-driven regression tests. */
enum {
    MEASURE_UNITS=1, MEASURE_OBJECT_A, MEASURE_OBJECT_B, MEASURE_FEATURE_A, MEASURE_FEATURE_B,
    MEASURE_PICK_A, MEASURE_PICK_B, MEASURE_OFFSETS, MEASURE_SLOT_A, MEASURE_SLOT_B,
    MEASURE_OFFSET_U, MEASURE_OFFSET_V, MEASURE_OFFSET_N, MEASURE_DISTANCE, MEASURE_JOIN,
    MEASURE_ANGLE, MEASURE_VALUE, MEASURE_ONCE, MEASURE_SAVE, MEASURE_CANCEL,
    MEASURE_RULES, MEASURE_NEW, MEASURE_REMOVE, MEASURE_AXIS_X, MEASURE_AXIS_Y,
    MEASURE_AXIS_Z, MEASURE_PLANE_XY, MEASURE_PLANE_YZ, MEASURE_PLANE_ZX,
    MEASURE_APPLY_OFFSET, MEASURE_RESET_OFFSET, MEASURE_DETAILS, MEASURE_CREATE, MEASURE_FILE, MEASURE_CHOICE_BASE=100
};
bool UIPanel_MeasurementControlRect(int action, SDL_Rect* rect);
