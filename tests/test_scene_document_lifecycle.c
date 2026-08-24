#include "test_layout_internal.h"

#include "Core/scene_document_lifecycle.h"
#include "Tools/scene_export.h"
#include "UI/ui_panel_internal.h"

#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

static bool lifecycle_write_fixture(const char* root, LineDrawingSceneExportPaths* paths) {
    Layout layout;
    char diagnostics[256];
    char attachment_dir[512];
    char attachment_path[512];
    bool ok = false;
    Layout_Init(&layout, 1.0f);
    ok = Layout_AddAnchor3(&layout, (Vec3){0.0f, 0.0f, 0.0f}) >= 0 &&
         Layout_AddAnchor3(&layout, (Vec3){2.0f, 1.0f, 0.0f}) >= 0 &&
         LineDrawingSceneExport_ExportLayoutToOutputRoot(
             &layout, "source_scene.json", root, paths, diagnostics, sizeof(diagnostics));
    Layout_Free(&layout);
    if (!ok) return false;
    if (snprintf(attachment_dir, sizeof(attachment_dir), "%s/attachments", paths->scene_dir) >= (int)sizeof(attachment_dir) ||
        snprintf(attachment_path, sizeof(attachment_path), "%s/notes.txt", attachment_dir) >= (int)sizeof(attachment_path) ||
        mkdir(attachment_dir, 0755) != 0) {
        return false;
    }
    return ld_test_write_text_file_basic(attachment_path, "source-owned attachment\n") &&
           ld_test_write_text_file_basic(paths->runtime_path, "runtime sentinel\n");
}

static bool test_scene_document_open_save_save_as_export_reopen(void) {
    char source_root_template[] = "/tmp/ld_document_source_XXXXXX";
    char output_root_template[] = "/tmp/ld_document_output_XXXXXX";
    char* source_root = mkdtemp(source_root_template);
    char* output_root = mkdtemp(output_root_template);
    LineDrawingSceneExportPaths source_paths;
    char save_as_dir[512];
    char save_as_authoring[512];
    char save_as_runtime[512];
    char save_as_package[512];
    char save_as_dependencies[512];
    char save_as_receipt[512];
    char save_as_attachment[512];
    char export_authoring[512];
    char export_runtime[512];
    char diagnostics[256];
    char* source_after_save = NULL;
    char* source_after_save_as = NULL;
    char* runtime_text = NULL;
    char* package_text = NULL;
    size_t reopened_anchor_count = 0u;
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;

    TEST_ASSERT(source_root != NULL);
    TEST_ASSERT(output_root != NULL);
    TEST_ASSERT(lifecycle_write_fixture(source_root, &source_paths));

    ld_test_init_runtime();
    state = Global_Get();
    ui = UIPanel_Get();
    TEST_ASSERT(state != NULL && ui != NULL);
    TEST_ASSERT(Global_SetOutputRoot(output_root, false));
    TEST_ASSERT(UIPanel_LoadSceneFromPath(source_paths.authoring_path));
    TEST_ASSERT(strcmp(Global_GetCurrentSceneAuthoringPath(), source_paths.authoring_path) == 0);
    TEST_ASSERT(!Global_IsLayoutDirty());

    TEST_ASSERT(Layout_AddAnchor3(&state->layout, (Vec3){7.0f, 3.0f, 1.0f}) >= 0);
    Global_FlagLayoutChanged();
    TEST_ASSERT(UIPanel_SaveDocument());
    TEST_ASSERT(strcmp(Global_GetCurrentSceneAuthoringPath(), source_paths.authoring_path) == 0);
    TEST_ASSERT(!Global_IsLayoutDirty());
    runtime_text = ld_test_read_text_file(source_paths.runtime_path);
    TEST_ASSERT(runtime_text != NULL && strcmp(runtime_text, "runtime sentinel\n") == 0);
    free(runtime_text);
    source_after_save = ld_test_read_text_file(source_paths.authoring_path);
    TEST_ASSERT(source_after_save != NULL);
    package_text = ld_test_read_text_file(source_paths.package_manifest_path);
    TEST_ASSERT(package_text != NULL);
    TEST_ASSERT(strstr(package_text, "\"stale\"") != NULL);
    free(package_text);
    package_text = NULL;

    TEST_ASSERT(Layout_AddAnchor3(&state->layout, (Vec3){9.0f, 4.0f, 2.0f}) >= 0);
    Global_FlagLayoutChanged();
    UIPanel_BeginSaveAsDialog();
    TEST_ASSERT(ui->saveDialog.active);
    TEST_ASSERT(ui->saveDialog.mode == UI_SAVE_DIALOG_SCENE_BUNDLE);
    snprintf(ui->saveDialog.buffer, sizeof(ui->saveDialog.buffer), "source_scene_copy");
    ui->saveDialog.length = strlen(ui->saveDialog.buffer);
    ui->saveDialog.cursor = ui->saveDialog.length;
    TEST_ASSERT(UIPanel_PerformSave(ui));
    TEST_ASSERT(LineDrawingSceneDocument_BuildSiblingSaveAsPaths(
        source_paths.authoring_path,
        "source_scene_copy",
        save_as_dir,
        sizeof(save_as_dir),
        save_as_authoring,
        sizeof(save_as_authoring),
        diagnostics,
        sizeof(diagnostics)));
    TEST_ASSERT(strcmp(Global_GetCurrentSceneAuthoringPath(), save_as_authoring) == 0);
    TEST_ASSERT(!Global_IsLayoutDirty());
    TEST_ASSERT(snprintf(save_as_runtime, sizeof(save_as_runtime), "%s/scene_runtime.json", save_as_dir) < (int)sizeof(save_as_runtime));
    TEST_ASSERT(snprintf(save_as_package, sizeof(save_as_package), "%s/scene_package.json", save_as_dir) < (int)sizeof(save_as_package));
    TEST_ASSERT(snprintf(save_as_dependencies, sizeof(save_as_dependencies), "%s/scene_dependencies.json", save_as_dir) < (int)sizeof(save_as_dependencies));
    TEST_ASSERT(snprintf(save_as_receipt, sizeof(save_as_receipt), "%s/scene_export_receipt.json", save_as_dir) < (int)sizeof(save_as_receipt));
    TEST_ASSERT(snprintf(save_as_attachment, sizeof(save_as_attachment), "%s/attachments/notes.txt", save_as_dir) < (int)sizeof(save_as_attachment));
    TEST_ASSERT(access(save_as_authoring, F_OK) == 0);
    TEST_ASSERT(access(save_as_package, F_OK) == 0);
    TEST_ASSERT(access(save_as_runtime, F_OK) != 0);
    TEST_ASSERT(access(save_as_dependencies, F_OK) != 0);
    TEST_ASSERT(access(save_as_receipt, F_OK) != 0);
    TEST_ASSERT(access(save_as_attachment, F_OK) == 0);
    package_text = ld_test_read_text_file(save_as_package);
    TEST_ASSERT(package_text != NULL);
    TEST_ASSERT(strstr(package_text, "scene_line_drawing_source_scene_copy") != NULL);
    TEST_ASSERT(strstr(package_text, "\"missing\"") != NULL);
    free(package_text);
    package_text = NULL;
    TEST_ASSERT(LineDrawingSceneDocument_RuntimeStatus(save_as_authoring) ==
                LINE_DRAWING_SCENE_RUNTIME_MISSING);
    source_after_save_as = ld_test_read_text_file(source_paths.authoring_path);
    TEST_ASSERT(source_after_save_as != NULL);
    TEST_ASSERT(strcmp(source_after_save, source_after_save_as) == 0);
    free(source_after_save_as);
    free(source_after_save);

    Global_FlagLayoutChanged();
    UIPanel_ExportScene();
    TEST_ASSERT(strcmp(Global_GetCurrentSceneAuthoringPath(), save_as_authoring) == 0);
    TEST_ASSERT(Global_IsLayoutDirty());
    TEST_ASSERT(snprintf(export_authoring, sizeof(export_authoring), "%s/source_scene_copy/scene_authoring.json", output_root) < (int)sizeof(export_authoring));
    TEST_ASSERT(snprintf(export_runtime, sizeof(export_runtime), "%s/source_scene_copy/scene_runtime.json", output_root) < (int)sizeof(export_runtime));
    TEST_ASSERT(access(export_authoring, F_OK) == 0);
    TEST_ASSERT(access(export_runtime, F_OK) == 0);
    TEST_ASSERT(LineDrawingSceneDocument_RuntimeStatus(export_authoring) ==
                LINE_DRAWING_SCENE_RUNTIME_CURRENT);
    reopened_anchor_count = state->layout.anchorCount;

    ld_test_shutdown_runtime();
    ld_test_init_runtime();
    TEST_ASSERT(UIPanel_LoadSceneFromPath(save_as_authoring));
    TEST_ASSERT(strcmp(Global_GetCurrentSceneAuthoringPath(), save_as_authoring) == 0);
    TEST_ASSERT(Global_Get()->layout.anchorCount == reopened_anchor_count);
    TEST_ASSERT(!Global_IsLayoutDirty());
    ld_test_shutdown_runtime();

    LineDrawingSceneDocument_RemoveClonedBundle(save_as_dir);
    LineDrawingSceneDocument_RemoveClonedBundle(source_paths.scene_dir);
    LineDrawingSceneDocument_RemoveClonedBundle(output_root);
    LineDrawingSceneDocument_RemoveClonedBundle(source_root);
    return true;
}

static bool test_scene_document_save_as_rejects_unsafe_or_existing_destination(void) {
    char root_template[] = "/tmp/ld_document_paths_XXXXXX";
    char* root = mkdtemp(root_template);
    LineDrawingSceneExportPaths source_paths;
    char scene_dir[512];
    char authoring[512];
    char diagnostics[256];
    TEST_ASSERT(root != NULL);
    TEST_ASSERT(lifecycle_write_fixture(root, &source_paths));
    TEST_ASSERT(!LineDrawingSceneDocument_BuildSiblingSaveAsPaths(
        source_paths.authoring_path, "../escape", scene_dir, sizeof(scene_dir),
        authoring, sizeof(authoring), diagnostics, sizeof(diagnostics)));
    TEST_ASSERT(LineDrawingSceneDocument_BuildSiblingSaveAsPaths(
        source_paths.authoring_path, "source_scene", scene_dir, sizeof(scene_dir),
        authoring, sizeof(authoring), diagnostics, sizeof(diagnostics)));
    TEST_ASSERT(!LineDrawingSceneDocument_CloneBundleForSaveAs(
        source_paths.authoring_path, scene_dir, diagnostics, sizeof(diagnostics)));
    LineDrawingSceneDocument_RemoveClonedBundle(source_paths.scene_dir);
    LineDrawingSceneDocument_RemoveClonedBundle(root);
    return true;
}

static bool test_scene_document_runtime_status_tracks_missing_current_and_stale(void) {
    char root_template[] = "/tmp/ld_document_runtime_XXXXXX";
    char* root = mkdtemp(root_template);
    LineDrawingSceneExportPaths paths;
    struct timeval runtime_times[2] = {{100, 0}, {100, 0}};
    struct timeval authoring_times[2] = {{200, 0}, {200, 0}};
    TEST_ASSERT(root != NULL);
    TEST_ASSERT(lifecycle_write_fixture(root, &paths));
    TEST_ASSERT(utimes(paths.runtime_path, runtime_times) == 0);
    TEST_ASSERT(utimes(paths.authoring_path, authoring_times) == 0);
    TEST_ASSERT(LineDrawingSceneDocument_RuntimeStatus(paths.authoring_path) ==
                LINE_DRAWING_SCENE_RUNTIME_STALE);
    TEST_ASSERT(unlink(paths.runtime_path) == 0);
    TEST_ASSERT(LineDrawingSceneDocument_RuntimeStatus(paths.authoring_path) ==
                LINE_DRAWING_SCENE_RUNTIME_MISSING);
    LineDrawingSceneDocument_RemoveClonedBundle(paths.scene_dir);
    LineDrawingSceneDocument_RemoveClonedBundle(root);
    return true;
}

bool scene_document_lifecycle_run_tests(void) {
    const TestCase cases[] = {
        { "scene_document_open_save_save_as_export_reopen",
          test_scene_document_open_save_save_as_export_reopen },
        { "scene_document_save_as_rejects_unsafe_or_existing_destination",
          test_scene_document_save_as_rejects_unsafe_or_existing_destination },
        { "scene_document_runtime_status_tracks_missing_current_and_stale",
          test_scene_document_runtime_status_tracks_missing_current_and_stale },
    };
    return run_test_cases("SceneDocumentLifecycle", cases, sizeof(cases) / sizeof(cases[0]));
}
