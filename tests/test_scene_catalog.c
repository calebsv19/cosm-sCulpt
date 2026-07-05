#include "test_framework.h"

#include "Menu/line_drawing_scene_catalog.h"
#include "test_artifact_helpers.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool test_catalog_refresh_collects_sorted_layouts_and_scenes(void) {
    char temp_template[] = "/tmp/ld_host_catalog_XXXXXX";
    char* root = NULL;
    char alpha_json[PATH_MAX];
    char beta_json[PATH_MAX];
    char grouped_dir[PATH_MAX];
    char grouped_scene[PATH_MAX];
    char grouped_authoring[PATH_MAX];
    char grouped_runtime[PATH_MAX];
    char root_scene[PATH_MAX];
    char root_authoring[PATH_MAX];
    char root_runtime[PATH_MAX];
    LineDrawingSceneCatalog catalog;

    root = ld_test_artifact_make_temp_dir(temp_template);
    TEST_ASSERT(root != NULL);

    snprintf(alpha_json, sizeof(alpha_json), "%s/alpha.json", root);
    snprintf(beta_json, sizeof(beta_json), "%s/beta.json", root);
    snprintf(grouped_dir, sizeof(grouped_dir), "%s/group_a", root);
    snprintf(grouped_scene, sizeof(grouped_scene), "%s/scene_b", grouped_dir);
    TEST_ASSERT(ld_test_artifact_scene_authoring_path(grouped_authoring,
                                                      sizeof(grouped_authoring),
                                                      grouped_scene));
    TEST_ASSERT(ld_test_artifact_scene_runtime_path(grouped_runtime,
                                                    sizeof(grouped_runtime),
                                                    grouped_scene));
    snprintf(root_scene, sizeof(root_scene), "%s/root_scene", root);
    TEST_ASSERT(ld_test_artifact_scene_authoring_path(root_authoring,
                                                      sizeof(root_authoring),
                                                      root_scene));
    TEST_ASSERT(ld_test_artifact_scene_runtime_path(root_runtime,
                                                    sizeof(root_runtime),
                                                    root_scene));

    TEST_ASSERT(ld_test_artifact_write_text_file(alpha_json, "{}"));
    TEST_ASSERT(ld_test_artifact_write_text_file(beta_json, "{}"));
    TEST_ASSERT(ld_test_artifact_make_dir(grouped_dir));
    TEST_ASSERT(ld_test_artifact_make_dir(grouped_scene));
    TEST_ASSERT(ld_test_artifact_write_text_file(grouped_authoring, "{}"));
    TEST_ASSERT(ld_test_artifact_write_text_file(grouped_runtime, "{}"));
    TEST_ASSERT(ld_test_artifact_make_dir(root_scene));
    TEST_ASSERT(ld_test_artifact_write_text_file(root_authoring, "{}"));
    TEST_ASSERT(ld_test_artifact_write_text_file(root_runtime, "{}"));

    LineDrawingSceneCatalog_Init(&catalog);
    LineDrawingSceneCatalog_Refresh(&catalog, root, beta_json, grouped_authoring);

    TEST_ASSERT(catalog.layout_count == 2);
    TEST_ASSERT(strcmp(catalog.layouts[0].label, "alpha.json") == 0);
    TEST_ASSERT(strcmp(catalog.layouts[1].label, "beta.json") == 0);
    TEST_ASSERT(catalog.active_layout_index == 1);

    TEST_ASSERT(catalog.scene_count == 2);
    TEST_ASSERT(strcmp(catalog.scenes[0].label, "group_a/scene_b") == 0);
    TEST_ASSERT(strcmp(catalog.scenes[1].label, "root_scene") == 0);
    TEST_ASSERT(catalog.active_scene_index == 0);
    return true;
}

static bool test_catalog_query_matches_label_and_path_case_insensitively(void) {
    LineDrawingSceneCatalogEntry entry = {0};
    snprintf(entry.label, sizeof(entry.label), "%s", "Group_A/Scene_B");
    snprintf(entry.path, sizeof(entry.path), "%s", "/tmp/group_a/scene_b/scene_authoring.json");

    TEST_ASSERT(LineDrawingSceneCatalog_EntryMatchesQuery(&entry, ""));
    TEST_ASSERT(LineDrawingSceneCatalog_EntryMatchesQuery(&entry, "scene_b"));
    TEST_ASSERT(LineDrawingSceneCatalog_EntryMatchesQuery(&entry, "GROUP_A"));
    TEST_ASSERT(LineDrawingSceneCatalog_EntryMatchesQuery(&entry, "authoring"));
    TEST_ASSERT(!LineDrawingSceneCatalog_EntryMatchesQuery(&entry, "missing"));
    return true;
}

bool scene_catalog_run_tests(void) {
    const TestCase cases[] = {
        {"RefreshCollectsSortedLayoutsAndScenes", test_catalog_refresh_collects_sorted_layouts_and_scenes},
        {"QueryMatchesLabelAndPath", test_catalog_query_matches_label_and_path_case_insensitively},
    };
    return run_test_cases("SceneCatalog", cases, sizeof(cases) / sizeof(cases[0]));
}
