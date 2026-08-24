#include "test_framework.h"

#include "Core/sculpt_scene_package.h"
#include "test_artifact_helpers.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool test_scene_package_resolves_descriptor_directory_and_authoring(void) {
    char temp_template[] = "/tmp/ld_scene_package_XXXXXX";
    char* root = ld_test_artifact_make_temp_dir(temp_template);
    char scene_dir[PATH_MAX];
    char authoring_path[PATH_MAX];
    char descriptor_path[PATH_MAX];
    char diagnostics[256];
    SculptScenePackageInputKind kind = SCULPT_SCENE_PACKAGE_INPUT_INVALID;
    SculptScenePackagePaths paths;

    TEST_ASSERT(root != NULL);
    TEST_ASSERT(snprintf(scene_dir, sizeof(scene_dir), "%s/iteration_v5", root) < (int)sizeof(scene_dir));
    TEST_ASSERT(mkdir(scene_dir, 0755) == 0);
    TEST_ASSERT(snprintf(authoring_path, sizeof(authoring_path), "%s/scene_authoring.json", scene_dir) <
                (int)sizeof(authoring_path));
    TEST_ASSERT(snprintf(descriptor_path, sizeof(descriptor_path), "%s/%s", scene_dir,
                         SCULPT_SCENE_PACKAGE_FILENAME) < (int)sizeof(descriptor_path));
    TEST_ASSERT(ld_test_artifact_write_text_file(authoring_path, "{}"));
    TEST_ASSERT(SculptScenePackage_WriteManifest(scene_dir,
                                                  "scene_line_drawing_iteration_v5",
                                                  SCULPT_SCENE_PACKAGE_RUNTIME_MISSING,
                                                  SCULPT_SCENE_PACKAGE_RECEIPT_MISSING,
                                                  false,
                                                  diagnostics,
                                                  sizeof(diagnostics)));

    TEST_ASSERT(SculptScenePackage_ResolveInput(scene_dir, &kind, &paths,
                                                 diagnostics, sizeof(diagnostics)));
    TEST_ASSERT(kind == SCULPT_SCENE_PACKAGE_INPUT_AUTHORING);
    TEST_ASSERT(paths.has_package_manifest);
    TEST_ASSERT(!paths.has_runtime);
    TEST_ASSERT(strcmp(paths.authoring_path, authoring_path) == 0);
    TEST_ASSERT(SculptScenePackage_ResolveInput(descriptor_path, &kind, &paths,
                                                 diagnostics, sizeof(diagnostics)));
    TEST_ASSERT(kind == SCULPT_SCENE_PACKAGE_INPUT_AUTHORING);
    TEST_ASSERT(SculptScenePackage_ResolveInput(authoring_path, &kind, &paths,
                                                 diagnostics, sizeof(diagnostics)));
    TEST_ASSERT(kind == SCULPT_SCENE_PACKAGE_INPUT_AUTHORING);

    TEST_ASSERT(unlink(descriptor_path) == 0);
    TEST_ASSERT(unlink(authoring_path) == 0);
    TEST_ASSERT(rmdir(scene_dir) == 0);
    TEST_ASSERT(rmdir(root) == 0);
    return true;
}

static bool test_scene_package_keeps_loose_layout_and_runtime_inputs_distinct(void) {
    char temp_template[] = "/tmp/ld_scene_input_kind_XXXXXX";
    char* root = ld_test_artifact_make_temp_dir(temp_template);
    char loose_layout[PATH_MAX];
    char scene_dir[PATH_MAX];
    char authoring_path[PATH_MAX];
    char runtime_path[PATH_MAX];
    char diagnostics[256];
    SculptScenePackageInputKind kind = SCULPT_SCENE_PACKAGE_INPUT_INVALID;
    SculptScenePackagePaths paths;

    TEST_ASSERT(root != NULL);
    TEST_ASSERT(snprintf(loose_layout, sizeof(loose_layout), "%s/sketch.json", root) < (int)sizeof(loose_layout));
    TEST_ASSERT(snprintf(scene_dir, sizeof(scene_dir), "%s/package", root) < (int)sizeof(scene_dir));
    TEST_ASSERT(mkdir(scene_dir, 0755) == 0);
    TEST_ASSERT(snprintf(authoring_path, sizeof(authoring_path), "%s/scene_authoring.json", scene_dir) <
                (int)sizeof(authoring_path));
    TEST_ASSERT(snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", scene_dir) <
                (int)sizeof(runtime_path));
    TEST_ASSERT(ld_test_artifact_write_text_file(loose_layout, "{}"));
    TEST_ASSERT(ld_test_artifact_write_text_file(authoring_path, "{}"));
    TEST_ASSERT(ld_test_artifact_write_text_file(runtime_path, "{}"));

    TEST_ASSERT(SculptScenePackage_ResolveInput(loose_layout, &kind, &paths,
                                                 diagnostics, sizeof(diagnostics)));
    TEST_ASSERT(kind == SCULPT_SCENE_PACKAGE_INPUT_LAYOUT_JSON);
    TEST_ASSERT(!SculptScenePackage_ResolveInput(runtime_path, &kind, &paths,
                                                  diagnostics, sizeof(diagnostics)));
    TEST_ASSERT(strstr(diagnostics, "compiled output") != NULL);
    TEST_ASSERT(SculptScenePackage_ResolveInput(scene_dir, &kind, &paths,
                                                 diagnostics, sizeof(diagnostics)));
    TEST_ASSERT(kind == SCULPT_SCENE_PACKAGE_INPUT_AUTHORING);
    TEST_ASSERT(!paths.has_package_manifest);
    TEST_ASSERT(paths.has_runtime);

    TEST_ASSERT(unlink(runtime_path) == 0);
    TEST_ASSERT(unlink(authoring_path) == 0);
    TEST_ASSERT(rmdir(scene_dir) == 0);
    TEST_ASSERT(unlink(loose_layout) == 0);
    TEST_ASSERT(rmdir(root) == 0);
    return true;
}

static bool test_scene_package_rejects_unsafe_or_unsupported_descriptor(void) {
    char temp_template[] = "/tmp/ld_scene_package_invalid_XXXXXX";
    char* root = ld_test_artifact_make_temp_dir(temp_template);
    char authoring_path[PATH_MAX];
    char descriptor_path[PATH_MAX];
    char diagnostics[256];
    SculptScenePackageInputKind kind = SCULPT_SCENE_PACKAGE_INPUT_INVALID;
    SculptScenePackagePaths paths;

    TEST_ASSERT(root != NULL);
    TEST_ASSERT(snprintf(authoring_path, sizeof(authoring_path), "%s/scene_authoring.json", root) <
                (int)sizeof(authoring_path));
    TEST_ASSERT(snprintf(descriptor_path, sizeof(descriptor_path), "%s/%s", root,
                         SCULPT_SCENE_PACKAGE_FILENAME) < (int)sizeof(descriptor_path));
    TEST_ASSERT(ld_test_artifact_write_text_file(authoring_path, "{}"));
    TEST_ASSERT(ld_test_artifact_write_text_file(
        descriptor_path,
        "{\"schema_variant\":\"sculpt_scene_package_v1\",\"authoring\":{\"path\":\"../escape.json\"}}"));
    TEST_ASSERT(!SculptScenePackage_ResolveInput(root, &kind, &paths,
                                                  diagnostics, sizeof(diagnostics)));
    TEST_ASSERT(strstr(diagnostics, "unsupported or unsafe") != NULL);
    TEST_ASSERT(!SculptScenePackage_ResolveInput(authoring_path, &kind, &paths,
                                                  diagnostics, sizeof(diagnostics)));
    TEST_ASSERT(strstr(diagnostics, "unsupported or unsafe") != NULL);

    TEST_ASSERT(unlink(descriptor_path) == 0);
    TEST_ASSERT(unlink(authoring_path) == 0);
    TEST_ASSERT(rmdir(root) == 0);
    return true;
}

bool sculpt_scene_package_run_tests(void) {
    const TestCase cases[] = {
        {"ResolvesDescriptorDirectoryAndAuthoring",
         test_scene_package_resolves_descriptor_directory_and_authoring},
        {"KeepsLooseLayoutAndRuntimeDistinct",
         test_scene_package_keeps_loose_layout_and_runtime_inputs_distinct},
        {"RejectsUnsafeOrUnsupportedDescriptor",
         test_scene_package_rejects_unsafe_or_unsupported_descriptor},
    };
    return run_test_cases("SculptScenePackage", cases, sizeof(cases) / sizeof(cases[0]));
}
