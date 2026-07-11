#include "UI/platform/line_drawing_folder_picker.h"
#include "test_framework.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool write_fake_picker(const char* path, const char* body) {
    FILE* file = fopen(path, "w");
    if (!file) return false;
    if (fputs(body, file) == EOF) {
        (void)fclose(file);
        return false;
    }
    if (fclose(file) != 0) return false;
    return chmod(path, 0700) == 0;
}

static bool read_text(const char* path, char* out_text, size_t out_text_size) {
    FILE* file = fopen(path, "r");
    size_t read_count = 0u;
    if (!file || !out_text || out_text_size == 0u) {
        if (file) (void)fclose(file);
        return false;
    }
    read_count = fread(out_text, 1u, out_text_size - 1u, file);
    out_text[read_count] = '\0';
    (void)fclose(file);
    return true;
}

static void remove_fixture(const char* root, const char* zenity, const char* kdialog, const char* args_log, const char* kdialog_marker) {
    (void)unlink(zenity);
    (void)unlink(kdialog);
    (void)unlink(args_log);
    (void)unlink(kdialog_marker);
    (void)rmdir(root);
}

static bool setup_fixture(char* root,
                          size_t root_size,
                          char* zenity,
                          char* kdialog,
                          char* args_log,
                          char* kdialog_marker) {
    char root_template[] = "/tmp/line_drawing_folder_picker_XXXXXX";
    const char* zenity_script =
        "#!/bin/sh\n"
        "printf '%s\\n' \"$@\" > \"$LINE_DRAWING_FOLDER_PICKER_ARGS_LOG\"\n"
        "case \"$LINE_DRAWING_FOLDER_PICKER_TEST_ZENITY\" in\n"
        "  selected) printf '%s\\n' \"$LINE_DRAWING_FOLDER_PICKER_SELECTED_PATH\"; exit 0 ;;\n"
        "  cancelled) exit 1 ;;\n"
        "  unavailable) exit 127 ;;\n"
        "  *) exit 2 ;;\n"
        "esac\n";
    const char* kdialog_script =
        "#!/bin/sh\n"
        ": > \"$LINE_DRAWING_FOLDER_PICKER_KDIALOG_MARKER\"\n"
        "printf '%s\\n' \"$@\" > \"$LINE_DRAWING_FOLDER_PICKER_ARGS_LOG\"\n"
        "printf '%s\\n' \"$LINE_DRAWING_FOLDER_PICKER_KDIALOG_PATH\"\n";
    char* created_root = mkdtemp(root_template);
    if (!created_root) return false;
    if (snprintf(root, root_size, "%s", created_root) >= (int)root_size ||
        snprintf(zenity, PATH_MAX, "%s/zenity", root) >= PATH_MAX ||
        snprintf(kdialog, PATH_MAX, "%s/kdialog", root) >= PATH_MAX ||
        snprintf(args_log, PATH_MAX, "%s/args.log", root) >= PATH_MAX ||
        snprintf(kdialog_marker, PATH_MAX, "%s/kdialog-ran", root) >= PATH_MAX) {
        return false;
    }
    return write_fake_picker(zenity, zenity_script) && write_fake_picker(kdialog, kdialog_script);
}

static bool test_zenity_returns_selected_path_and_preserves_arguments(void) {
    char root[PATH_MAX];
    char zenity[PATH_MAX];
    char kdialog[PATH_MAX];
    char args_log[PATH_MAX];
    char kdialog_marker[PATH_MAX];
    char output_path[PATH_MAX];
    char args_text[2048];
    const char* selected_path = "/tmp/selected folder";
    const char* initial_path = "/tmp/start folder";
    bool passed = false;

    TEST_ASSERT(setup_fixture(root, sizeof(root), zenity, kdialog, args_log, kdialog_marker));
    TEST_ASSERT(setenv("PATH", root, 1) == 0);
    TEST_ASSERT(setenv("LINE_DRAWING_FOLDER_PICKER_ARGS_LOG", args_log, 1) == 0);
    TEST_ASSERT(setenv("LINE_DRAWING_FOLDER_PICKER_KDIALOG_MARKER", kdialog_marker, 1) == 0);
    TEST_ASSERT(setenv("LINE_DRAWING_FOLDER_PICKER_TEST_ZENITY", "selected", 1) == 0);
    TEST_ASSERT(setenv("LINE_DRAWING_FOLDER_PICKER_SELECTED_PATH", selected_path, 1) == 0);

    TEST_ASSERT(LineDrawing_FolderPicker_Select("Choose a folder", initial_path, output_path, sizeof(output_path)) ==
                LINE_DRAWING_FOLDER_PICKER_SELECTED);
    TEST_ASSERT(strcmp(output_path, selected_path) == 0);
    TEST_ASSERT(access(kdialog_marker, F_OK) != 0);
    TEST_ASSERT(read_text(args_log, args_text, sizeof(args_text)));
    TEST_ASSERT(strstr(args_text, "--file-selection\n--directory\n--title\nChoose a folder\n--filename\n/tmp/start folder\n") != NULL);
    passed = true;
    remove_fixture(root, zenity, kdialog, args_log, kdialog_marker);
    return passed;
}

static bool test_kdialog_fallback_runs_when_zenity_is_unavailable(void) {
    char root[PATH_MAX];
    char zenity[PATH_MAX];
    char kdialog[PATH_MAX];
    char args_log[PATH_MAX];
    char kdialog_marker[PATH_MAX];
    char output_path[PATH_MAX];
    const char* selected_path = "/tmp/kdialog selection";
    bool passed = false;

    TEST_ASSERT(setup_fixture(root, sizeof(root), zenity, kdialog, args_log, kdialog_marker));
    TEST_ASSERT(setenv("PATH", root, 1) == 0);
    TEST_ASSERT(setenv("LINE_DRAWING_FOLDER_PICKER_ARGS_LOG", args_log, 1) == 0);
    TEST_ASSERT(setenv("LINE_DRAWING_FOLDER_PICKER_KDIALOG_MARKER", kdialog_marker, 1) == 0);
    TEST_ASSERT(setenv("LINE_DRAWING_FOLDER_PICKER_TEST_ZENITY", "unavailable", 1) == 0);
    TEST_ASSERT(setenv("LINE_DRAWING_FOLDER_PICKER_KDIALOG_PATH", selected_path, 1) == 0);

    TEST_ASSERT(LineDrawing_FolderPicker_Select("Fallback chooser", "/tmp/default", output_path, sizeof(output_path)) ==
                LINE_DRAWING_FOLDER_PICKER_SELECTED);
    TEST_ASSERT(strcmp(output_path, selected_path) == 0);
    TEST_ASSERT(access(kdialog_marker, F_OK) == 0);
    passed = true;
    remove_fixture(root, zenity, kdialog, args_log, kdialog_marker);
    return passed;
}

static bool test_cancelled_zenity_does_not_open_kdialog(void) {
    char root[PATH_MAX];
    char zenity[PATH_MAX];
    char kdialog[PATH_MAX];
    char args_log[PATH_MAX];
    char kdialog_marker[PATH_MAX];
    char output_path[PATH_MAX];
    bool passed = false;

    TEST_ASSERT(setup_fixture(root, sizeof(root), zenity, kdialog, args_log, kdialog_marker));
    TEST_ASSERT(setenv("PATH", root, 1) == 0);
    TEST_ASSERT(setenv("LINE_DRAWING_FOLDER_PICKER_ARGS_LOG", args_log, 1) == 0);
    TEST_ASSERT(setenv("LINE_DRAWING_FOLDER_PICKER_KDIALOG_MARKER", kdialog_marker, 1) == 0);
    TEST_ASSERT(setenv("LINE_DRAWING_FOLDER_PICKER_TEST_ZENITY", "cancelled", 1) == 0);

    TEST_ASSERT(LineDrawing_FolderPicker_Select("Cancel chooser", NULL, output_path, sizeof(output_path)) ==
                LINE_DRAWING_FOLDER_PICKER_CANCELLED);
    TEST_ASSERT(output_path[0] == '\0');
    TEST_ASSERT(access(kdialog_marker, F_OK) != 0);
    passed = true;
    remove_fixture(root, zenity, kdialog, args_log, kdialog_marker);
    return passed;
}

static bool test_missing_linux_picker_reports_unavailable(void) {
    char root_template[] = "/tmp/line_drawing_folder_picker_empty_XXXXXX";
    char output_path[PATH_MAX];
    char* root = mkdtemp(root_template);
    bool passed = false;

    TEST_ASSERT(root != NULL);
    TEST_ASSERT(setenv("PATH", root, 1) == 0);
    TEST_ASSERT(LineDrawing_FolderPicker_Select("Unavailable chooser", NULL, output_path, sizeof(output_path)) ==
                LINE_DRAWING_FOLDER_PICKER_UNAVAILABLE);
    TEST_ASSERT(output_path[0] == '\0');
    passed = true;
    (void)rmdir(root);
    return passed;
}

int main(void) {
    const TestCase cases[] = {
        {"ZenitySelectedPathAndArguments", test_zenity_returns_selected_path_and_preserves_arguments},
        {"KdialogFallbackWhenZenityUnavailable", test_kdialog_fallback_runs_when_zenity_is_unavailable},
        {"CancelledZenityDoesNotOpenKdialog", test_cancelled_zenity_does_not_open_kdialog},
        {"MissingLinuxPickerReportsUnavailable", test_missing_linux_picker_reports_unavailable},
    };
    return run_test_cases("LineDrawingFolderPicker", cases, sizeof(cases) / sizeof(cases[0])) ? 0 : 1;
}
