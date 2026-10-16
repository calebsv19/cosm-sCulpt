#include "UI/ui_panel_internal.h"
#include "UI/ui_panel_measurement.h"
#include "UI/shared_text.h"

bool UIPanel_HandleTextInput(const char* text) {
    UIPanelState* ui=UIPanel_Get();
    if(!text)return false;
    if(ui->measurement.active && ui->measurement.placing)return UIPanel_MeasurementText(text);
    return UIPanel_SharedTextInsert(text);
}
bool UIPanel_HandleKeyEvent(const SDL_Event* event) {
    UIPanelState* ui=UIPanel_Get();
    if(event->type==SDL_KEYDOWN && ui->measurement.active && ui->measurement.placing &&
        UIPanel_MeasurementKey(event->key.keysym.sym))return true;
    return UIPanel_SharedTextEvent(event);
}
