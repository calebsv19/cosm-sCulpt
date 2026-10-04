#pragma once
#include <stdbool.h>
/* Presentation preferences never change document geometry, grid units or history.
 * The caller chooses an app-private runtime path; malformed files are ignored. */
bool LineDrawingEditorPreferences_Load(const char* path);
bool LineDrawingEditorPreferences_Save(const char* path);
