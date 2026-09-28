#include "Editor/editor_reference_edit.h"
#include "Core/global_state.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static EditorNumericEditResult result(EditorNumericEditStatus status, const char* message) {
    EditorNumericEditResult r = {.status = status};
    snprintf(r.message, sizeof(r.message), "%s", message);
    return r;
}

EditorNumericEditResult Editor_ApplyReferencePlacement(EditorState* editor,
    Layout* layout, const EditorReferencePlacement* placement) {
    if (!editor || !layout || !placement ||
        placement->kind < EDITOR_PLACE_PROJECTED_DISTANCE || placement->kind > EDITOR_PLACE_COINCIDENT_POINTS)
        return result(EDITOR_NUMERIC_INVALID, "Invalid reference placement.");
    EditorResolvedReference a, b;
    EditorMeasurementStatus status = Editor_ResolveReference(layout, &placement->a, &a);
    if (status == EDITOR_MEASUREMENT_OK) status = Editor_ResolveReference(layout, &placement->b, &b);
    if (status != EDITOR_MEASUREMENT_OK)
        return result(EDITOR_NUMERIC_INVALID, Editor_MeasurementStatusLabel(status));
    if (strcmp(placement->a.entity_id, placement->b.entity_id) == 0)
        return result(EDITOR_NUMERIC_CONFLICT, "A and B must belong to different objects.");
    double axis[3] = {placement->world_axis.x, placement->world_axis.y, placement->world_axis.z};
    double delta[3] = {0};
    if (placement->kind == EDITOR_PLACE_PROJECTED_DISTANCE) {
        const double length = hypot(hypot(axis[0], axis[1]), axis[2]);
        if (!isfinite(length) || length <= 1e-12 || !isfinite(placement->target_meters))
            return result(EDITOR_NUMERIC_INVALID, "Enter a finite distance and a nonzero finite world axis.");
        double current = 0;
        for (int i = 0; i < 3; ++i) {
            axis[i] /= length;
            current += (b.point_meters[i] - a.point_meters[i]) * axis[i];
        }
        for (int i = 0; i < 3; ++i) delta[i] = (placement->target_meters - current) * axis[i];
    } else {
        for (int i = 0; i < 3; ++i) delta[i] = a.point_meters[i] - b.point_meters[i];
    }
    EditorNumericEdit edit = {.entity_id = placement->b.entity_id,
        .kind = EDITOR_NUMERIC_TRANSLATE, .unit = CORE_UNIT_METER,
        .values = {delta[0], delta[1], delta[2]}};
    Object3D candidate;
    EditorNumericEditResult r = Editor_PrepareNumericEdit(layout, &edit, &candidate);
    if (r.status != EDITOR_NUMERIC_APPLIED && r.status != EDITOR_NUMERIC_UNCHANGED) return r;
    /* Resolve the resulting feature, not merely the object's transform. This
     * catches face-center precision loss before reserving history or publishing. */
    Layout scratch = *layout;
    scratch.objectStore.items = &candidate;
    scratch.objectStore.count = 1;
    EditorResolvedReference after;
    if (Editor_ResolveReference(&scratch, &placement->b, &after) != EDITOR_MEASUREMENT_OK)
        return result(EDITOR_NUMERIC_CONFLICT, "Resulting reference is not valid.");
    const double tolerance = 1e-6 + (placement->kind == EDITOR_PLACE_PROJECTED_DISTANCE
        ? 1e-7 * fabs(placement->target_meters) : 0);
    double projection = 0;
    for (int i = 0; i < 3; ++i) {
        if (!isfinite(delta[i]) || fabs(after.point_meters[i] - b.point_meters[i] - delta[i]) > tolerance)
            return result(EDITOR_NUMERIC_CONFLICT, "Reference placement exceeds numeric precision or a geometry lock.");
        projection += (after.point_meters[i] - a.point_meters[i]) * axis[i];
    }
    if (placement->kind == EDITOR_PLACE_PROJECTED_DISTANCE &&
        (!isfinite(projection) || fabs(projection - placement->target_meters) > tolerance))
        return result(EDITOR_NUMERIC_CONFLICT, "Requested projected distance cannot be represented exactly enough.");
    if (r.status == EDITOR_NUMERIC_UNCHANGED) {
        snprintf(r.message, sizeof(r.message), "References already satisfy this placement.");
        return r;
    }
    if (!Editor_TryHistoryCapture(editor, layout))
        return result(EDITOR_NUMERIC_NO_MEMORY, "Could not reserve undo history; placement was not applied.");
    *Layout_ObjectStore_Find(&layout->objectStore, candidate.objectId) = candidate;
    if (layout == &Global_Get()->layout) {
        Global_FlagLayoutChanged();
        Global_FlagHitboxesDirty();
    }
    snprintf(r.message, sizeof(r.message), "Placed B once; no persistent constraint was created.");
    return r;
}
