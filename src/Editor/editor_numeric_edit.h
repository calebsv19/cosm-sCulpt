#pragma once

#include "Editor/editor.h"
#include "core_units.h"

typedef enum EditorNumericEditKind {
    EDITOR_NUMERIC_WIDTH,
    EDITOR_NUMERIC_HEIGHT,
    EDITOR_NUMERIC_DEPTH,
    EDITOR_NUMERIC_POSITION,
    EDITOR_NUMERIC_TRANSLATE
} EditorNumericEditKind;

typedef enum EditorNumericEditStatus {
    EDITOR_NUMERIC_APPLIED,
    EDITOR_NUMERIC_UNCHANGED,
    EDITOR_NUMERIC_INVALID,
    EDITOR_NUMERIC_NOT_FOUND,
    EDITOR_NUMERIC_UNSUPPORTED,
    EDITOR_NUMERIC_CONFLICT,
    EDITOR_NUMERIC_NO_MEMORY
} EditorNumericEditStatus;

typedef struct EditorNumericEdit {
    const char* entity_id;
    EditorNumericEditKind kind;
    CoreUnitKind unit;
    double values[3];
} EditorNumericEdit;

typedef struct EditorNumericEditResult {
    EditorNumericEditStatus status;
    double actual_meters[3];
    char message[128];
} EditorNumericEditResult;

/* Synchronous single-object command. Failure/no-op preserves layout and history.
 * Position/translation uses the existing editor's world-space bounds center.
 * Dimensions require unit instance scale; values are physical, not pixel sizes. */
EditorNumericEditResult Editor_ApplyNumericEdit(EditorState* editor, Layout* layout,
                                                const EditorNumericEdit* edit);
/* One finite scalar with optional m/cm/mm/in/ft (or core_units name) suffix.
 * A missing suffix uses default_unit; output is meters. No expression language. */
bool Editor_ParseLength(const char* text, CoreUnitKind default_unit, double* meters);

/* Candidate-only validation for compound commands. No mutation, history or dirty
 * flags; prepared is valid only for APPLIED/UNCHANGED. Publish synchronously. */
EditorNumericEditResult Editor_PrepareNumericEdit(const Layout* layout,
    const EditorNumericEdit* edit, Object3D* prepared);
