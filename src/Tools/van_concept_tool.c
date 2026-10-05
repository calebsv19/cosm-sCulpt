/* Native finalizer for the manually authored van concept. No editor or route API
 * is added: use existing motion, engineering validation and scene compilation. */
#include "Layout/layout_json.h"
#include "Layout/layout_motion.h"
#include "Tools/canonical_scene_export.h"
#include "core_scene_compile.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool path(char out[1024], const char *directory, const char *name) {
    return snprintf(out, 1024, "%s/%s", directory, name) < 1024;
}

static const Object3D *entity(const Layout *layout, const char *id) {
    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D *o = &layout->objectStore.items[i];
        if (!o->isDeleted && strcmp(o->coreMeta.object_id, id) == 0) return o;
    }
    return NULL;
}

static bool write_json(const char *file, cJSON *value) {
    char *text = cJSON_Print(value);
    if (!text) return false;
    FILE *f = fopen(file, "wx");
    bool ok = f && fputs(text, f) >= 0;
    if (f && fclose(f) != 0) ok = false;
    free(text);
    return ok;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: van_concept_tool draft.layout.json new-staging-directory [bed-check-target ...]\n");
        return 1;
    }
    Layout layout = {0}, loaded = {0};
    Layout_Init(&layout, .05f);
    Layout_Init(&loaded, .05f);
    cJSON *report = NULL, *pose_checks = cJSON_CreateArray();
    char *serialized = NULL;
    char layout_path[1024], authoring_path[1024], runtime_path[1024], report_path[1024];
    char diagnostics[512] = {0};
    bool ok = false;
    const char *stage = "paths";
    if (!path(layout_path, argv[2], "van_layout_concept_v1.layout.json") ||
        !path(authoring_path, argv[2], "scene_authoring.json") ||
        !path(runtime_path, argv[2], "scene_runtime.json") ||
        !path(report_path, argv[2], "validation.json")) goto done;
    /* Python owns an exclusive temporary directory; refuse any preexisting
     * expected output so direct invocations cannot overwrite a user document. */
    const char *outputs[] = {layout_path, authoring_path, runtime_path, report_path};
    for (size_t i = 0; i < 4; ++i) {
        FILE *f = fopen(outputs[i], "r");
        if (f) { fclose(f); goto done; }
    }
    stage = "load";
    if (!Layout_LoadFromFile(&layout, argv[1])) goto done;
    stage = "generate envelope";
    if (!Layout_GenerateMotionEnvelope(&layout, "bed_lift", 33, NULL, NULL)) goto done;
    stage = "envelope current";
    const LayoutMotionEnvelope *envelope = Layout_FindRuleEnvelope(&layout.objectStore, "bed_lift");
    if (!envelope || !Layout_MotionEnvelopeCurrent(&layout, envelope)) goto done;
    const Object3D *volume = Layout_ObjectStore_FindConst(&layout.objectStore, envelope->object_id);
    stage = "spatial rule";
    for (int i = 3; i < argc; ++i) {
        LayoutSpatialRule rule = {.kind = LAYOUT_SPATIAL_NO_INTERSECTION};
        snprintf(rule.source, sizeof(rule.source), "%s", volume->coreMeta.object_id);
        snprintf(rule.target, sizeof(rule.target), "%s", argv[i]);
        if (!Layout_EditSpatialRule(&layout, &rule, NULL, NULL, NULL)) goto done;
        /* Transaction publication can relocate object storage. */
        envelope = Layout_FindRuleEnvelope(&layout.objectStore, "bed_lift");
        volume = Layout_ObjectStore_FindConst(&layout.objectStore, envelope->object_id);
    }
    stage = "reload";
    serialized = Layout_SaveToString(&layout);
    if (!serialized || !Layout_LoadFromString(&loaded, serialized) ||
        !Layout_ValidateEngineering(&loaded, diagnostics, sizeof(diagnostics)) ||
        !Layout_ValidateConstraints(&loaded, diagnostics, sizeof(diagnostics))) goto done;
    /* Verify min/max on the reloaded candidate, never on the saved home document. */
    const LayoutConstraint *travel = &loaded.objectStore.constraints[0];
    double maximum = travel->travel_max, minimum = travel->travel_min;
    stage = "travel range";
    const double positions[] = {maximum, minimum};
    if (!pose_checks) goto done;
    for (size_t i = 0; i < 2; ++i) {
        if (!Layout_SetTravelPosition(&loaded, "bed_lift", positions[i], NULL, NULL)) goto done;
        const Object3D *deck = entity(&loaded, "bed_deck");
        const Object3D *mattress = entity(&loaded, "bed_mattress");
        if (!deck || !mattress || fabs(deck->rectPrism.frame.origin.z - positions[i]) > 1e-5 ||
            fabs(mattress->rectPrism.frame.origin.z - positions[i] - .09) > 1e-5) goto done;
        cJSON *pose = cJSON_CreateObject();
        if (!pose) goto done;
        cJSON_AddItemToArray(pose_checks, pose);
        if (!cJSON_AddNumberToObject(pose, "deckCenter_m", deck->rectPrism.frame.origin.z) ||
            !cJSON_AddNumberToObject(pose, "mattressCenter_m", mattress->rectPrism.frame.origin.z)) goto done;
    }
    stage = "report";
    report = Layout_SpatialReportJson(&layout);
    if (!report || !cJSON_AddStringToObject(report, "dimensionsStatus", "concept_not_measured") ||
        !cJSON_AddBoolToObject(report, "nativeReloadAndTravelPassed", true)) goto done;
    cJSON_AddItemToObject(report, "motionPoseChecks", pose_checks);
    pose_checks = NULL;
    stage = "export";
    if (!Layout_SaveToFile(&layout, layout_path) ||
        !LineDrawingCanonicalScene_ExportLayoutToFile(&layout, "van_layout_concept_v1", authoring_path) ||
        core_scene_compile_authoring_file_to_runtime_file(authoring_path, runtime_path,
            diagnostics, sizeof(diagnostics)).code != CORE_OK || !write_json(report_path, report)) goto done;
    ok = true;
done:
    if (!ok) fprintf(stderr, "Concept validation/export failed at %s: %s %s %s\n", stage, diagnostics, layout.geometryMessage, loaded.geometryMessage);
    free(serialized);
    cJSON_Delete(report);
    cJSON_Delete(pose_checks);
    Layout_Free(&layout);
    Layout_Free(&loaded);
    return ok ? 0 : 1;
}
