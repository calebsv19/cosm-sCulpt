#include "UI/ui_panel_internal.h"

#include <SDL2/SDL.h>
#include <ctype.h>
#include <string.h>

bool UIPanel_HandleTextInput(const char* text) {
    UIPanelState* ui = UIPanel_Get();
    if (!text) return false;

    if (ui->saveDialog.active) {
        for (const char* p = text; *p; ++p) {
            unsigned char c = (unsigned char)*p;
            if (!(isalnum(c) || c == '_' || c == '-' || c == ' ')) continue;
            if (ui->saveDialog.length + 1 >= sizeof(ui->saveDialog.buffer)) break;
            memmove(&ui->saveDialog.buffer[ui->saveDialog.cursor + 1],
                    &ui->saveDialog.buffer[ui->saveDialog.cursor],
                    ui->saveDialog.length - ui->saveDialog.cursor + 1);
            ui->saveDialog.buffer[ui->saveDialog.cursor] = (char)c;
            ui->saveDialog.length++;
            ui->saveDialog.cursor++;
        }
        ui->saveDialog.buffer[ui->saveDialog.length] = '\0';
        return true;
    }

    if (ui->rootDialog.active) {
        for (const char* p = text; *p; ++p) {
            unsigned char c = (unsigned char)*p;
            if (c < 32 || c == 127) continue;
            if (ui->rootDialog.length + 1 >= sizeof(ui->rootDialog.buffer)) break;
            memmove(&ui->rootDialog.buffer[ui->rootDialog.cursor + 1],
                    &ui->rootDialog.buffer[ui->rootDialog.cursor],
                    ui->rootDialog.length - ui->rootDialog.cursor + 1);
            ui->rootDialog.buffer[ui->rootDialog.cursor] = (char)c;
            ui->rootDialog.length++;
            ui->rootDialog.cursor++;
        }
        ui->rootDialog.buffer[ui->rootDialog.length] = '\0';
        return true;
    }

    if (ui->prismDimensionDialog.active) {
        for (const char* p = text; *p; ++p) {
            unsigned char c = (unsigned char)*p;
            if (!(isdigit(c) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E')) continue;
            if (ui->prismDimensionDialog.length + 1 >= sizeof(ui->prismDimensionDialog.buffer)) break;
            memmove(&ui->prismDimensionDialog.buffer[ui->prismDimensionDialog.cursor + 1],
                    &ui->prismDimensionDialog.buffer[ui->prismDimensionDialog.cursor],
                    ui->prismDimensionDialog.length - ui->prismDimensionDialog.cursor + 1);
            ui->prismDimensionDialog.buffer[ui->prismDimensionDialog.cursor] = (char)c;
            ui->prismDimensionDialog.length++;
            ui->prismDimensionDialog.cursor++;
        }
        ui->prismDimensionDialog.buffer[ui->prismDimensionDialog.length] = '\0';
        return true;
    }

    if (ui->sceneBoundsDialog.active) {
        for (const char* p = text; *p; ++p) {
            unsigned char c = (unsigned char)*p;
            if (!(isdigit(c) || c == '.' || c == '-' || c == '+' ||
                  c == 'e' || c == 'E' || c == ',' || isspace(c))) {
                continue;
            }
            if (ui->sceneBoundsDialog.length + 1 >= sizeof(ui->sceneBoundsDialog.buffer)) break;
            memmove(&ui->sceneBoundsDialog.buffer[ui->sceneBoundsDialog.cursor + 1],
                    &ui->sceneBoundsDialog.buffer[ui->sceneBoundsDialog.cursor],
                    ui->sceneBoundsDialog.length - ui->sceneBoundsDialog.cursor + 1);
            ui->sceneBoundsDialog.buffer[ui->sceneBoundsDialog.cursor] = (char)c;
            ui->sceneBoundsDialog.length++;
            ui->sceneBoundsDialog.cursor++;
        }
        ui->sceneBoundsDialog.buffer[ui->sceneBoundsDialog.length] = '\0';
        return true;
    }

    if (ui->constructionPlaneDialog.active) {
        for (const char* p = text; *p; ++p) {
            unsigned char c = (unsigned char)*p;
            if (!(isdigit(c) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E')) continue;
            if (ui->constructionPlaneDialog.length + 1 >= sizeof(ui->constructionPlaneDialog.buffer)) break;
            memmove(&ui->constructionPlaneDialog.buffer[ui->constructionPlaneDialog.cursor + 1],
                    &ui->constructionPlaneDialog.buffer[ui->constructionPlaneDialog.cursor],
                    ui->constructionPlaneDialog.length - ui->constructionPlaneDialog.cursor + 1);
            ui->constructionPlaneDialog.buffer[ui->constructionPlaneDialog.cursor] = (char)c;
            ui->constructionPlaneDialog.length++;
            ui->constructionPlaneDialog.cursor++;
        }
        ui->constructionPlaneDialog.buffer[ui->constructionPlaneDialog.length] = '\0';
        return true;
    }

    if (ui->objectTransformDialog.active) {
        const bool allow_vec3 =
            ui->objectTransformDialog.target == UI_OBJECT_TRANSFORM_DIALOG_TARGET_POSITION;
        for (const char* p = text; *p; ++p) {
            unsigned char c = (unsigned char)*p;
            if (allow_vec3) {
                if (!(isdigit(c) || c == '.' || c == '-' || c == '+' ||
                      c == 'e' || c == 'E' || c == ',' || isspace(c))) {
                    continue;
                }
            } else {
                if (!(isdigit(c) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E')) {
                    continue;
                }
            }
            if (ui->objectTransformDialog.length + 1 >= sizeof(ui->objectTransformDialog.buffer)) break;
            memmove(&ui->objectTransformDialog.buffer[ui->objectTransformDialog.cursor + 1],
                    &ui->objectTransformDialog.buffer[ui->objectTransformDialog.cursor],
                    ui->objectTransformDialog.length - ui->objectTransformDialog.cursor + 1);
            ui->objectTransformDialog.buffer[ui->objectTransformDialog.cursor] = (char)c;
            ui->objectTransformDialog.length++;
            ui->objectTransformDialog.cursor++;
        }
        ui->objectTransformDialog.buffer[ui->objectTransformDialog.length] = '\0';
        return true;
    }

    return false;
}

bool UIPanel_HandleKeyEvent(const SDL_Event* event) {
    UIPanelState* ui = UIPanel_Get();
    if (event->type != SDL_KEYDOWN) return false;

    SDL_Keycode key = event->key.keysym.sym;
    if (ui->saveDialog.active) {
        if (key == SDLK_RETURN) {
            return UIPanel_PerformSave(ui);
        }
        if (key == SDLK_ESCAPE) {
            UIPanel_CloseSaveDialog(ui);
            return true;
        }
        switch (key) {
            case SDLK_BACKSPACE:
                if (ui->saveDialog.cursor > 0 && ui->saveDialog.length > 0) {
                    memmove(&ui->saveDialog.buffer[ui->saveDialog.cursor - 1],
                            &ui->saveDialog.buffer[ui->saveDialog.cursor],
                            ui->saveDialog.length - ui->saveDialog.cursor + 1);
                    ui->saveDialog.cursor--;
                    ui->saveDialog.length--;
                }
                return true;
            case SDLK_DELETE:
                if (ui->saveDialog.cursor < ui->saveDialog.length) {
                    memmove(&ui->saveDialog.buffer[ui->saveDialog.cursor],
                            &ui->saveDialog.buffer[ui->saveDialog.cursor + 1],
                            ui->saveDialog.length - ui->saveDialog.cursor);
                    ui->saveDialog.length--;
                }
                return true;
            case SDLK_LEFT:
                if (ui->saveDialog.cursor > 0) ui->saveDialog.cursor--;
                return true;
            case SDLK_RIGHT:
                if (ui->saveDialog.cursor < ui->saveDialog.length) ui->saveDialog.cursor++;
                return true;
            case SDLK_HOME:
                ui->saveDialog.cursor = 0;
                return true;
            case SDLK_END:
                ui->saveDialog.cursor = ui->saveDialog.length;
                return true;
            default:
                break;
        }
        return false;
    }

    if (ui->rootDialog.active) {
        if (key == SDLK_RETURN) {
            return UIPanel_ApplyRootDialog(ui);
        }
        if (key == SDLK_ESCAPE) {
            UIPanel_CloseRootDialog(ui);
            return true;
        }
        switch (key) {
            case SDLK_BACKSPACE:
                if (ui->rootDialog.cursor > 0 && ui->rootDialog.length > 0) {
                    memmove(&ui->rootDialog.buffer[ui->rootDialog.cursor - 1],
                            &ui->rootDialog.buffer[ui->rootDialog.cursor],
                            ui->rootDialog.length - ui->rootDialog.cursor + 1);
                    ui->rootDialog.cursor--;
                    ui->rootDialog.length--;
                }
                return true;
            case SDLK_DELETE:
                if (ui->rootDialog.cursor < ui->rootDialog.length) {
                    memmove(&ui->rootDialog.buffer[ui->rootDialog.cursor],
                            &ui->rootDialog.buffer[ui->rootDialog.cursor + 1],
                            ui->rootDialog.length - ui->rootDialog.cursor);
                    ui->rootDialog.length--;
                }
                return true;
            case SDLK_LEFT:
                if (ui->rootDialog.cursor > 0) ui->rootDialog.cursor--;
                return true;
            case SDLK_RIGHT:
                if (ui->rootDialog.cursor < ui->rootDialog.length) ui->rootDialog.cursor++;
                return true;
            case SDLK_HOME:
                ui->rootDialog.cursor = 0;
                return true;
            case SDLK_END:
                ui->rootDialog.cursor = ui->rootDialog.length;
                return true;
            default:
                break;
        }
        return false;
    }

    if (ui->prismDimensionDialog.active) {
        if (key == SDLK_RETURN) {
            return UIPanel_ApplyPrismDimensionDialog(ui);
        }
        if (key == SDLK_ESCAPE) {
            UIPanel_ClosePrismDimensionDialog(ui);
            return true;
        }
        switch (key) {
            case SDLK_BACKSPACE:
                if (ui->prismDimensionDialog.cursor > 0 && ui->prismDimensionDialog.length > 0) {
                    memmove(&ui->prismDimensionDialog.buffer[ui->prismDimensionDialog.cursor - 1],
                            &ui->prismDimensionDialog.buffer[ui->prismDimensionDialog.cursor],
                            ui->prismDimensionDialog.length - ui->prismDimensionDialog.cursor + 1);
                    ui->prismDimensionDialog.cursor--;
                    ui->prismDimensionDialog.length--;
                }
                return true;
            case SDLK_DELETE:
                if (ui->prismDimensionDialog.cursor < ui->prismDimensionDialog.length) {
                    memmove(&ui->prismDimensionDialog.buffer[ui->prismDimensionDialog.cursor],
                            &ui->prismDimensionDialog.buffer[ui->prismDimensionDialog.cursor + 1],
                            ui->prismDimensionDialog.length - ui->prismDimensionDialog.cursor);
                    ui->prismDimensionDialog.length--;
                }
                return true;
            case SDLK_LEFT:
                if (ui->prismDimensionDialog.cursor > 0) ui->prismDimensionDialog.cursor--;
                return true;
            case SDLK_RIGHT:
                if (ui->prismDimensionDialog.cursor < ui->prismDimensionDialog.length) ui->prismDimensionDialog.cursor++;
                return true;
            case SDLK_HOME:
                ui->prismDimensionDialog.cursor = 0;
                return true;
            case SDLK_END:
                ui->prismDimensionDialog.cursor = ui->prismDimensionDialog.length;
                return true;
            default:
                break;
        }
        return false;
    }

    if (ui->sceneBoundsDialog.active) {
        if (key == SDLK_RETURN) {
            return UIPanel_ApplySceneBoundsDialog(ui);
        }
        if (key == SDLK_ESCAPE) {
            UIPanel_CloseSceneBoundsDialog(ui);
            return true;
        }
        switch (key) {
            case SDLK_BACKSPACE:
                if (ui->sceneBoundsDialog.cursor > 0 && ui->sceneBoundsDialog.length > 0) {
                    memmove(&ui->sceneBoundsDialog.buffer[ui->sceneBoundsDialog.cursor - 1],
                            &ui->sceneBoundsDialog.buffer[ui->sceneBoundsDialog.cursor],
                            ui->sceneBoundsDialog.length - ui->sceneBoundsDialog.cursor + 1);
                    ui->sceneBoundsDialog.cursor--;
                    ui->sceneBoundsDialog.length--;
                }
                return true;
            case SDLK_DELETE:
                if (ui->sceneBoundsDialog.cursor < ui->sceneBoundsDialog.length) {
                    memmove(&ui->sceneBoundsDialog.buffer[ui->sceneBoundsDialog.cursor],
                            &ui->sceneBoundsDialog.buffer[ui->sceneBoundsDialog.cursor + 1],
                            ui->sceneBoundsDialog.length - ui->sceneBoundsDialog.cursor);
                    ui->sceneBoundsDialog.length--;
                }
                return true;
            case SDLK_LEFT:
                if (ui->sceneBoundsDialog.cursor > 0) ui->sceneBoundsDialog.cursor--;
                return true;
            case SDLK_RIGHT:
                if (ui->sceneBoundsDialog.cursor < ui->sceneBoundsDialog.length) ui->sceneBoundsDialog.cursor++;
                return true;
            case SDLK_HOME:
                ui->sceneBoundsDialog.cursor = 0;
                return true;
            case SDLK_END:
                ui->sceneBoundsDialog.cursor = ui->sceneBoundsDialog.length;
                return true;
            default:
                break;
        }
        return false;
    }

    if (ui->constructionPlaneDialog.active) {
        if (key == SDLK_RETURN) {
            return UIPanel_ApplyConstructionPlaneDialog(ui);
        }
        if (key == SDLK_ESCAPE) {
            UIPanel_CloseConstructionPlaneDialog(ui);
            return true;
        }
        switch (key) {
            case SDLK_BACKSPACE:
                if (ui->constructionPlaneDialog.cursor > 0 && ui->constructionPlaneDialog.length > 0) {
                    memmove(&ui->constructionPlaneDialog.buffer[ui->constructionPlaneDialog.cursor - 1],
                            &ui->constructionPlaneDialog.buffer[ui->constructionPlaneDialog.cursor],
                            ui->constructionPlaneDialog.length - ui->constructionPlaneDialog.cursor + 1);
                    ui->constructionPlaneDialog.cursor--;
                    ui->constructionPlaneDialog.length--;
                }
                return true;
            case SDLK_DELETE:
                if (ui->constructionPlaneDialog.cursor < ui->constructionPlaneDialog.length) {
                    memmove(&ui->constructionPlaneDialog.buffer[ui->constructionPlaneDialog.cursor],
                            &ui->constructionPlaneDialog.buffer[ui->constructionPlaneDialog.cursor + 1],
                            ui->constructionPlaneDialog.length - ui->constructionPlaneDialog.cursor);
                    ui->constructionPlaneDialog.length--;
                }
                return true;
            case SDLK_LEFT:
                if (ui->constructionPlaneDialog.cursor > 0) ui->constructionPlaneDialog.cursor--;
                return true;
            case SDLK_RIGHT:
                if (ui->constructionPlaneDialog.cursor < ui->constructionPlaneDialog.length) ui->constructionPlaneDialog.cursor++;
                return true;
            case SDLK_HOME:
                ui->constructionPlaneDialog.cursor = 0;
                return true;
            case SDLK_END:
                ui->constructionPlaneDialog.cursor = ui->constructionPlaneDialog.length;
                return true;
            default:
                break;
        }
        return false;
    }

    if (ui->objectTransformDialog.active) {
        if (key == SDLK_RETURN) {
            return UIPanel_ApplyObjectTransformDialog(ui);
        }
        if (key == SDLK_ESCAPE) {
            UIPanel_CloseObjectTransformDialog(ui);
            return true;
        }
        switch (key) {
            case SDLK_BACKSPACE:
                if (ui->objectTransformDialog.cursor > 0 && ui->objectTransformDialog.length > 0) {
                    memmove(&ui->objectTransformDialog.buffer[ui->objectTransformDialog.cursor - 1],
                            &ui->objectTransformDialog.buffer[ui->objectTransformDialog.cursor],
                            ui->objectTransformDialog.length - ui->objectTransformDialog.cursor + 1);
                    ui->objectTransformDialog.cursor--;
                    ui->objectTransformDialog.length--;
                }
                return true;
            case SDLK_DELETE:
                if (ui->objectTransformDialog.cursor < ui->objectTransformDialog.length) {
                    memmove(&ui->objectTransformDialog.buffer[ui->objectTransformDialog.cursor],
                            &ui->objectTransformDialog.buffer[ui->objectTransformDialog.cursor + 1],
                            ui->objectTransformDialog.length - ui->objectTransformDialog.cursor);
                    ui->objectTransformDialog.length--;
                }
                return true;
            case SDLK_LEFT:
                if (ui->objectTransformDialog.cursor > 0) ui->objectTransformDialog.cursor--;
                return true;
            case SDLK_RIGHT:
                if (ui->objectTransformDialog.cursor < ui->objectTransformDialog.length) {
                    ui->objectTransformDialog.cursor++;
                }
                return true;
            case SDLK_HOME:
                ui->objectTransformDialog.cursor = 0;
                return true;
            case SDLK_END:
                ui->objectTransformDialog.cursor = ui->objectTransformDialog.length;
                return true;
            default:
                break;
        }
        return false;
    }

    return false;
}
