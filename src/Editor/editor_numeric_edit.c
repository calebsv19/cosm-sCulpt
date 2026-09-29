#include "Editor/editor_numeric_edit.h"
#include "Core/global_state.h"
#include "Layout/layout_constraints.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EditorNumericEditResult result(EditorNumericEditStatus status, const char* message) {
    EditorNumericEditResult r = {0};
    r.status = status;
    snprintf(r.message, sizeof(r.message), "%s", message);
    return r;
}

static bool close_meters(double actual, double requested) {
    return isfinite(actual) && isfinite(requested) &&
           fabs(actual - requested) <= 1e-6 + 1e-7 * fabs(requested);
}

static void values_world(const Object3D* object, EditorNumericEditKind kind, double out[3]) {
    if (kind == EDITOR_NUMERIC_POSITION || kind == EDITOR_NUMERIC_TRANSLATE) {
        Vec3 center = {NAN, NAN, NAN};
        (void)Layout_Object3D_ComputeVisualCenter(object, &center);
        out[0] = center.x; out[1] = center.y; out[2] = center.z;
    } else if (object->kind == OBJECT3D_KIND_PLANE) {
        out[0] = kind == EDITOR_NUMERIC_WIDTH ? object->plane.width : object->plane.height;
    } else {
        out[0] = kind == EDITOR_NUMERIC_WIDTH ? object->rectPrism.width :
                 kind == EDITOR_NUMERIC_HEIGHT ? object->rectPrism.height : object->rectPrism.depth;
    }
}

bool Editor_ParseLength(const char* text, CoreUnitKind default_unit, double* meters) {
    if (meters) *meters = 0;
    if (!text || !meters) return false;
    char* end = NULL;
    errno = 0;
    const double value = strtod(text, &end);
    if (end == text || errno == ERANGE || !isfinite(value)) return false;
    while (isspace((unsigned char)*end)) ++end;
    CoreUnitKind unit = default_unit;
    if (*end) {
        char suffix[32];
        size_t length = strlen(end);
        while (length && isspace((unsigned char)end[length - 1])) --length;
        if (length >= sizeof(suffix)) return false;
        memcpy(suffix, end, length); suffix[length] = '\0';
        if (core_units_parse_kind(suffix, &unit).code != CORE_OK) return false;
    }
    if (core_units_convert(value, unit, CORE_UNIT_METER, meters).code != CORE_OK || !isfinite(*meters)) {
        *meters = 0;
        return false;
    }
    return true;
}

EditorNumericEditResult Editor_PrepareNumericEdit(const Layout* layout,
    const EditorNumericEdit* edit, Object3D* prepared) {
    if (!prepared || !layout || !edit || !edit->entity_id || !edit->entity_id[0] ||
        edit->kind < EDITOR_NUMERIC_WIDTH || edit->kind > EDITOR_NUMERIC_TRANSLATE)
        return result(EDITOR_NUMERIC_INVALID, "Invalid numerical edit.");
    const Object3D* live = NULL;
    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        if (!object->isDeleted && strcmp(object->coreMeta.object_id, edit->entity_id) == 0) {
            if (live) return result(EDITOR_NUMERIC_CONFLICT, "Entity ID is ambiguous.");
            live = object;
        }
    }
    if (!live) return result(EDITOR_NUMERIC_NOT_FOUND, "Entity no longer exists.");
    if (live->coreMeta.flags.locked) return result(EDITOR_NUMERIC_CONFLICT, "Object is locked.");
    const bool position = edit->kind == EDITOR_NUMERIC_POSITION || edit->kind == EDITOR_NUMERIC_TRANSLATE;
    if (!position && (live->kind != OBJECT3D_KIND_PLANE && live->kind != OBJECT3D_KIND_RECT_PRISM))
        return result(EDITOR_NUMERIC_UNSUPPORTED, "Only primitive dimensions are supported.");
    if (!position && live->kind == OBJECT3D_KIND_PLANE && edit->kind == EDITOR_NUMERIC_DEPTH)
        return result(EDITOR_NUMERIC_UNSUPPORTED, "A plane has no depth dimension.");
    if (!position && (live->transform.scale.x != 1.0f || live->transform.scale.y != 1.0f || live->transform.scale.z != 1.0f))
        return result(EDITOR_NUMERIC_UNSUPPORTED, "Dimension edits require instance scale 1, 1, 1.");
    const double scale = Layout_WorldScale(layout);
    if (core_units_validate_world_scale(scale).code != CORE_OK)
        return result(EDITOR_NUMERIC_INVALID, "Invalid document physical scale.");
    const int count = position ? 3 : 1;
    double before[3] = {0}, requested[3] = {0}, world[3] = {0};
    values_world(live, edit->kind, before);
    bool unchanged = true;
    for (int i = 0; i < count; ++i) {
        if (core_units_convert(edit->values[i], edit->unit, CORE_UNIT_METER, &requested[i]).code != CORE_OK)
            return result(EDITOR_NUMERIC_INVALID, "Enter finite lengths in a supported unit.");
        if (edit->kind == EDITOR_NUMERIC_TRANSLATE) requested[i] += before[i] * scale;
        world[i] = requested[i] / scale;
        if (!isfinite(requested[i]) || !isfinite(world[i]) || fabs(world[i]) > FLT_MAX)
            return result(EDITOR_NUMERIC_INVALID, "Length is outside the supported numeric range.");
        if (!close_meters(before[i] * scale, requested[i])) unchanged = false;
    }
    if (!position && requested[0] <= 0)
        return result(EDITOR_NUMERIC_INVALID, "Dimensions must be greater than zero.");
    if (unchanged) {
        *prepared = *live;
        EditorNumericEditResult r = result(EDITOR_NUMERIC_UNCHANGED, "Already at the requested value.");
        for (int i = 0; i < count; ++i) r.actual_meters[i] = before[i] * scale;
        return r;
    }
    /* The existing setters operate on an isolated object candidate. They only
     * publish dirty state when called with the active layout itself. */
    Object3D candidate = *live;
    Layout scratch = *layout;
    scratch.geometryEditActive = true;
    scratch.objectStore.items = &candidate;
    scratch.objectStore.count = 1;
    bool adjusted = false, ok = false;
    if (position) {
        ok = Layout_SetObject3DPosition(&scratch, candidate.objectId,
                (Vec3){(float)world[0], (float)world[1], (float)world[2]}, &adjusted);
    } else {
        float width = candidate.kind == OBJECT3D_KIND_PLANE ? candidate.plane.width : candidate.rectPrism.width;
        float height = candidate.kind == OBJECT3D_KIND_PLANE ? candidate.plane.height : candidate.rectPrism.height;
        float depth = candidate.rectPrism.depth;
        if (edit->kind == EDITOR_NUMERIC_WIDTH) width = (float)world[0];
        if (edit->kind == EDITOR_NUMERIC_HEIGHT) height = (float)world[0];
        if (edit->kind == EDITOR_NUMERIC_DEPTH) depth = (float)world[0];
        ok = candidate.kind == OBJECT3D_KIND_PLANE
            ? Layout_SetPlaneDimensions(&scratch, candidate.objectId, width, height, &adjusted)
            : Layout_SetRectPrismDimensions(&scratch, candidate.objectId, width, height, depth, &adjusted);
    }
    if (!ok || adjusted) return result(EDITOR_NUMERIC_CONFLICT, "Exact edit conflicts with bounds or geometry limits.");
    double actual[3] = {0};
    values_world(&candidate, edit->kind, actual);
    for (int i = 0; i < count; ++i) {
        actual[i] *= scale;
        if (!close_meters(actual[i], requested[i]))
            return result(EDITOR_NUMERIC_CONFLICT, "Exact edit conflicts with a plane lock or numeric precision.");
    }
    *prepared = candidate;
    EditorNumericEditResult r = result(EDITOR_NUMERIC_APPLIED, "Applied exact numerical edit.");
    memcpy(r.actual_meters, actual, sizeof(actual));
    return r;
}

EditorNumericEditResult Editor_ApplyNumericEdit(EditorState* editor, Layout* layout,
                                                const EditorNumericEdit* edit) {
    if (!editor) return result(EDITOR_NUMERIC_INVALID, "Missing editor history.");
    Object3D candidate;
    EditorNumericEditResult r = Editor_PrepareNumericEdit(layout, edit, &candidate);
    if (r.status != EDITOR_NUMERIC_APPLIED) return r;
    if (!Layout_ReplaceGeometryObject(layout, &candidate, Editor_ReserveGeometryHistory, editor))
        return result(EDITOR_NUMERIC_CONFLICT, layout->geometryMessage);
    return r;
}

bool Editor_ReserveGeometryHistory(const Layout* layout, void* editor) {
    return editor && Editor_TryHistoryCapture(editor, (Layout*)layout);
}
