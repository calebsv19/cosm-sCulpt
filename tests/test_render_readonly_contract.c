#include "test_framework.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* render_contract_read_file(const char* path) {
    FILE* f = NULL;
    long len = 0;
    char* buf = NULL;
    if (!path) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    len = ftell(f);
    if (len < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    buf = (char*)malloc((size_t)len + 1u);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1u, (size_t)len, f) != (size_t)len) {
        fclose(f);
        free(buf);
        return NULL;
    }
    fclose(f);
    buf[len] = '\0';
    return buf;
}

static char* render_contract_read_source(const char* relative_path) {
    char prefixed[512];
    char* text = render_contract_read_file(relative_path);
    if (text) return text;
    snprintf(prefixed, sizeof(prefixed), "line_drawing/%s", relative_path);
    return render_contract_read_file(prefixed);
}

static bool render_contract_source_omits_tokens(const char* relative_path,
                                                const char* const* tokens,
                                                size_t token_count) {
    char* text = render_contract_read_source(relative_path);
    TEST_ASSERT(text != NULL);
    for (size_t i = 0u; i < token_count; ++i) {
        if (strstr(text, tokens[i]) != NULL) {
            fprintf(stderr,
                    "render read-only contract violation: %s contains %s\n",
                    relative_path,
                    tokens[i]);
            free(text);
            return false;
        }
    }
    free(text);
    return true;
}

static bool test_pure_render_surfaces_avoid_mutating_entry_points(void) {
    static const char* const files[] = {
        "src/Editor/render/render_editor.c",
        "src/Layout/render_layout.c",
        "src/Layout/render_layout_surfaces.c",
        "src/UI/panel/render_ui_panel.c",
        "src/UI/panel/ui_panel_create_summary.c",
        "src/UI/panel/ui_panel_edit_summary.c",
        "src/UI/panel/ui_panel_file_summary.c",
        "src/UI/panel/ui_panel_scene_summary.c",
        "src/UI/panel/ui_panel_summary_surface.c",
        "src/UI/panel/ui_panel_view_summary.c"
    };
    static const char* const forbidden_tokens[] = {
        "Global_Flag",
        "Global_Set",
        "Global_On",
        "Global_Record",
        "Global_Toggle",
        "Editor_HistoryCapture",
        "Editor_Undo",
        "Editor_Redo",
        "ObjectAuthoringDocument_Set",
        "ObjectAuthoringSession_Set",
        "Layout_Add",
        "Layout_Create",
        "Layout_Delete",
        "Layout_ObjectStore_Create",
        "Layout_ObjectStore_Delete",
        "UIPanel_Activate",
        "UIPanel_CloseFileBrowser"
    };

    for (size_t i = 0u; i < sizeof(files) / sizeof(files[0]); ++i) {
        TEST_ASSERT(render_contract_source_omits_tokens(
            files[i],
            forbidden_tokens,
            sizeof(forbidden_tokens) / sizeof(forbidden_tokens[0])));
    }
    return true;
}

bool render_readonly_contract_run_tests(void) {
    const TestCase cases[] = {
        {"pure render surfaces avoid mutating entry points",
         test_pure_render_surfaces_avoid_mutating_entry_points}
    };

    return run_test_cases("RenderReadOnlyContract",
                          cases,
                          sizeof(cases) / sizeof(cases[0]));
}
