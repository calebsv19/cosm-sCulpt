#include "UI/ui_panel_internal.h"
#include "UI/ui_panel_file_layout.h"
#include "UI/panel/ui_panel_file_browser_internal.h"
#include "UI/ui_panel_file_summary.h"
#include "UI/ui_panel_object_workspace_summary.h"
#include "UI/ui_panel_scene_list.h"
#include "UI/ui_panel_shell.h"

#include "Core/line_drawing_file_catalog.h"
#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Layout/asset/layout_imported_mesh_asset.h"
#include "Layout/asset/layout_object_asset_mesh_authoring.h"
#include "Layout/layout_json.h"
#include "UI/info_overlay.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static const char* k_runtime_file_browser_mode_path = "data/runtime/file_browser_mode.txt";
static const char* k_runtime_json_root_path = "data/runtime/file_browser_json_root.txt";
static const char* k_runtime_scene_root_path = "data/runtime/file_browser_scene_root.txt";
static const char* k_runtime_object_root_path = "data/runtime/file_browser_object_root.txt";
static const char* k_runtime_mesh_root_path = "data/runtime/file_browser_mesh_root.txt";
static const char* k_runtime_stl_root_path = "data/runtime/file_browser_stl_root.txt";
static const char* k_runtime_last_json_entry_path = "data/runtime/file_browser_last_json_entry.txt";
static const char* k_runtime_last_scene_entry_path = "data/runtime/file_browser_last_scene_entry.txt";
static const char* k_runtime_last_object_entry_path = "data/runtime/file_browser_last_object_entry.txt";
static const char* k_runtime_last_mesh_entry_path = "data/runtime/file_browser_last_mesh_entry.txt";
static const char* k_runtime_last_stl_entry_path = "data/runtime/file_browser_last_stl_entry.txt";
static const Uint32 k_load_progress_finished_ttl_ms = 1400u;

static bool UIPanel_PathIsDirectory(const char* path);
static int UIPanel_FindLoadMenuIndexForPath(const UIPanelState* ui, const char* path);

static const char* UIPanel_LoadProgressBaseName(const char* path) {
    const char* base = NULL;
    if (!path || !path[0]) return "(unset)";
    base = strrchr(path, '/');
    return base ? base + 1 : path;
}

static const char* UIPanel_LoadProgressVerbForMode(UILoadMenuMode mode) {
    switch (mode) {
        case UI_LOAD_MENU_MODE_JSON: return "Load JSON";
        case UI_LOAD_MENU_MODE_SCENE: return "Load scene";
        case UI_LOAD_MENU_MODE_OBJECT: return "Load asset";
        case UI_LOAD_MENU_MODE_RUNTIME_MESH: return "Place mesh";
        case UI_LOAD_MENU_MODE_STL_IMPORT: return "Import STL";
        case UI_LOAD_MENU_MODE_NONE:
        default: return "Load";
    }
}

static void UIPanel_BeginLoadProgress(UIPanelState* ui,
                                      UILoadMenuMode mode,
                                      const char* path,
                                      const char* label) {
    if (!ui || !path || !path[0]) return;
    ui->loadMenu.loadProgressState = UI_LOAD_PROGRESS_LOADING;
    ui->loadMenu.loadProgressMode = mode;
    ui->loadMenu.loadProgressStartedTicks = SDL_GetTicks();
    ui->loadMenu.loadProgressFinishedTicks = 0u;
    ui->loadMenu.loadProgressPermille = 35;
    snprintf(ui->loadMenu.loadProgressPath,
             sizeof(ui->loadMenu.loadProgressPath),
             "%s",
             path);
    snprintf(ui->loadMenu.loadProgressLabel,
             sizeof(ui->loadMenu.loadProgressLabel),
             "%s",
             label && label[0] ? label : UIPanel_LoadProgressBaseName(path));
    snprintf(ui->loadMenu.loadProgressDetail,
             sizeof(ui->loadMenu.loadProgressDetail),
             "%s queued",
             UIPanel_LoadProgressVerbForMode(mode));
}

static void UIPanel_EndLoadProgress(UIPanelState* ui, bool succeeded) {
    if (!ui || ui->loadMenu.loadProgressState == UI_LOAD_PROGRESS_NONE) return;
    ui->loadMenu.loadProgressState = succeeded
        ? UI_LOAD_PROGRESS_COMPLETE
        : UI_LOAD_PROGRESS_FAILED;
    ui->loadMenu.loadProgressFinishedTicks = SDL_GetTicks();
    ui->loadMenu.loadProgressPermille = 1000;
    snprintf(ui->loadMenu.loadProgressDetail,
             sizeof(ui->loadMenu.loadProgressDetail),
             "%s %s",
             UIPanel_LoadProgressVerbForMode(ui->loadMenu.loadProgressMode),
             succeeded ? "finished" : "failed");
}

static void UIPanel_EndLoadProgressWithDetail(UIPanelState* ui,
                                              bool succeeded,
                                              const char* detail) {
    UIPanel_EndLoadProgress(ui, succeeded);
    if (!ui || !detail || !detail[0]) return;
    snprintf(ui->loadMenu.loadProgressDetail,
             sizeof(ui->loadMenu.loadProgressDetail),
             "%s",
             detail);
}

static void UIPanel_RecordStlImportFailure(UIPanelState* ui, const char* detail) {
    const char* message = detail && detail[0] ? detail : "STL import failed: import or placement error";
    UIPanel_EndLoadProgressWithDetail(ui, false, message);
    Global_SetObjectRuntimeMeshStatus(message);
}

static int UIPanel_StlProgressPermille(const CoreMeshCompileProgress* progress) {
    int stage_base = 50;
    int stage_span = 50;
    int within_stage = 0;
    if (!progress) return stage_base;
    switch (progress->stage) {
        case CORE_MESH_COMPILE_PROGRESS_STAGE_PREPARING:
            stage_base = 50;
            stage_span = 50;
            break;
        case CORE_MESH_COMPILE_PROGRESS_STAGE_READING_SOURCE:
            stage_base = 100;
            stage_span = 50;
            break;
        case CORE_MESH_COMPILE_PROGRESS_STAGE_SCANNING_STL:
            stage_base = 150;
            stage_span = 200;
            break;
        case CORE_MESH_COMPILE_PROGRESS_STAGE_PARSING_STL:
            stage_base = 350;
            stage_span = 350;
            break;
        case CORE_MESH_COMPILE_PROGRESS_STAGE_EMITTING_RUNTIME:
            stage_base = 700;
            stage_span = 200;
            break;
        case CORE_MESH_COMPILE_PROGRESS_STAGE_COMPLETE:
            return 950;
        case CORE_MESH_COMPILE_PROGRESS_STAGE_UNKNOWN:
        default:
            return 50;
    }
    if (progress->total > 0u && progress->current < progress->total) {
        within_stage = (int)((progress->current * (size_t)stage_span) / progress->total);
    } else if (progress->total > 0u) {
        within_stage = stage_span;
    }
    return stage_base + within_stage;
}

static const char* UIPanel_StlProgressStageDetail(int stage) {
    switch ((CoreMeshCompileProgressStage)stage) {
        case CORE_MESH_COMPILE_PROGRESS_STAGE_PREPARING:
            return "Preparing STL import";
        case CORE_MESH_COMPILE_PROGRESS_STAGE_READING_SOURCE:
            return "Reading STL source";
        case CORE_MESH_COMPILE_PROGRESS_STAGE_SCANNING_STL:
            return "Scanning ASCII STL triangles";
        case CORE_MESH_COMPILE_PROGRESS_STAGE_PARSING_STL:
            return "Parsing and welding STL triangles";
        case CORE_MESH_COMPILE_PROGRESS_STAGE_EMITTING_RUNTIME:
            return "Writing runtime mesh";
        case CORE_MESH_COMPILE_PROGRESS_STAGE_COMPLETE:
            return "Runtime mesh compiled";
        case CORE_MESH_COMPILE_PROGRESS_STAGE_UNKNOWN:
        default:
            return "Import STL running in background";
    }
}

static void UIPanel_StlImportProgressCallback(const CoreMeshCompileProgress* progress,
                                              void* user_data) {
    UIPanelState* ui = (UIPanelState*)user_data;
    int permille = UIPanel_StlProgressPermille(progress);
    if (!ui || !progress) return;
    if (permille < 0) permille = 0;
    if (permille > 1000) permille = 1000;
    SDL_AtomicSet(&ui->loadMenu.asyncStlProgressPermille, permille);
    SDL_AtomicSet(&ui->loadMenu.asyncStlProgressStage, (int)progress->stage);
}

static int UIPanel_AsyncStlImportThreadMain(void* data) {
    UIPanelState* ui = (UIPanelState*)data;
    char diagnostics[256];
    char authoring_path[MAX_CONFIG_PATH];
    char runtime_path[MAX_CONFIG_PATH];
    bool ok = false;
    if (!ui) return 1;

    diagnostics[0] = '\0';
    authoring_path[0] = '\0';
    runtime_path[0] = '\0';
    ok = LayoutImportedMeshAsset_ImportStlToRuntimeWithProgress(
        ui->loadMenu.asyncStlSourcePath,
        ui->loadMenu.asyncStlAssetRoot,
        authoring_path,
        sizeof(authoring_path),
        runtime_path,
        sizeof(runtime_path),
        diagnostics,
        sizeof(diagnostics),
        UIPanel_StlImportProgressCallback,
        ui);
    snprintf(ui->loadMenu.asyncStlAuthoringPath,
             sizeof(ui->loadMenu.asyncStlAuthoringPath),
             "%s",
             authoring_path);
    snprintf(ui->loadMenu.asyncStlRuntimePath,
             sizeof(ui->loadMenu.asyncStlRuntimePath),
             "%s",
             runtime_path);
    snprintf(ui->loadMenu.asyncStlDiagnostics,
             sizeof(ui->loadMenu.asyncStlDiagnostics),
             "%s",
             diagnostics);
    ui->loadMenu.asyncStlSucceeded = ok;
    SDL_AtomicSet(&ui->loadMenu.asyncStlComplete, 1);
    return ok ? 0 : 1;
}

static bool UIPanel_StartAsyncStlImport(UIPanelState* ui,
                                        const char* stl_path,
                                        const char* label) {
    const char* asset_root = Global_GetObjectAssetRoot();
    if (!ui || !stl_path || !stl_path[0]) return false;

    if (ui->loadMenu.asyncStlActive) {
        UIPanel_RecordStlImportFailure(ui,
                                       "STL import failed: another STL import is already running");
        return false;
    }
    if (Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT) {
        UIPanel_RecordStlImportFailure(ui, "STL import failed: switch to scene mode.");
        return false;
    }
    if (!asset_root || !asset_root[0]) {
        UIPanel_RecordStlImportFailure(ui,
                                       "STL import failed: object asset root is unset.");
        return false;
    }

    snprintf(ui->loadMenu.asyncStlSourcePath,
             sizeof(ui->loadMenu.asyncStlSourcePath),
             "%s",
             stl_path);
    snprintf(ui->loadMenu.asyncStlAssetRoot,
             sizeof(ui->loadMenu.asyncStlAssetRoot),
             "%s",
             asset_root);
    ui->loadMenu.asyncStlAuthoringPath[0] = '\0';
    ui->loadMenu.asyncStlRuntimePath[0] = '\0';
    ui->loadMenu.asyncStlDiagnostics[0] = '\0';
    ui->loadMenu.asyncStlSucceeded = false;
    SDL_AtomicSet(&ui->loadMenu.asyncStlComplete, 0);
    SDL_AtomicSet(&ui->loadMenu.asyncStlProgressPermille, 50);
    SDL_AtomicSet(&ui->loadMenu.asyncStlProgressStage,
                  (int)CORE_MESH_COMPILE_PROGRESS_STAGE_PREPARING);
    ui->loadMenu.asyncStlThread = SDL_CreateThread(UIPanel_AsyncStlImportThreadMain,
                                                   "ld-stl-import",
                                                   ui);
    if (!ui->loadMenu.asyncStlThread) {
        UIPanel_RecordStlImportFailure(ui,
                                       "STL import failed: could not start import worker");
        return false;
    }

    ui->loadMenu.asyncStlActive = true;
    ui->loadMenu.loadProgressPermille = 50;
    snprintf(ui->loadMenu.loadProgressDetail,
             sizeof(ui->loadMenu.loadProgressDetail),
             "Preparing STL import");
    (void)label;
    return true;
}

static bool UIPanel_PlaceCachedStlOrStartImport(UIPanelState* ui,
                                                const char* stl_path,
                                                const char* label) {
    const char* asset_root = Global_GetObjectAssetRoot();
    char authoring_path[MAX_CONFIG_PATH];
    char runtime_path[MAX_CONFIG_PATH];
    char preview_path[MAX_CONFIG_PATH];
    char diagnostics[256];
    LayoutImportedMeshStlCacheState cache_state =
        LAYOUT_IMPORTED_MESH_STL_CACHE_MISSING;
    if (!ui || !stl_path || !stl_path[0]) return false;

    authoring_path[0] = '\0';
    runtime_path[0] = '\0';
    preview_path[0] = '\0';
    diagnostics[0] = '\0';
    if (asset_root && asset_root[0]) {
        cache_state = LayoutImportedMeshAsset_GetStlCacheState(stl_path,
                                                               asset_root,
                                                               authoring_path,
                                                               sizeof(authoring_path),
                                                               runtime_path,
                                                               sizeof(runtime_path),
                                                               preview_path,
                                                               sizeof(preview_path),
                                                               diagnostics,
                                                               sizeof(diagnostics));
    }

    if (cache_state == LAYOUT_IMPORTED_MESH_STL_CACHE_FRESH) {
        const bool placed = UIPanel_PlaceImportedStlRuntimeMesh(stl_path,
                                                               authoring_path,
                                                               runtime_path);
        if (placed) {
            UIPanel_RememberLoadedEntry(UI_LOAD_MENU_MODE_STL_IMPORT, stl_path);
            UIPanel_RefreshConfigList();
            UIPanel_EndLoadProgressWithDetail(ui, true, "Added cached STL");
        } else {
            UIPanel_EndLoadProgressWithDetail(ui, false, "Add cached STL failed");
        }
        (void)preview_path;
        (void)label;
        return placed;
    }

    if (cache_state == LAYOUT_IMPORTED_MESH_STL_CACHE_STALE) {
        snprintf(ui->loadMenu.loadProgressDetail,
                 sizeof(ui->loadMenu.loadProgressDetail),
                 "Reimporting changed STL");
    }
    return UIPanel_StartAsyncStlImport(ui, stl_path, label);
}

void UIPanel_WaitForAsyncStlImport(void) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui || !ui->loadMenu.asyncStlThread) return;
    SDL_WaitThread(ui->loadMenu.asyncStlThread, NULL);
    ui->loadMenu.asyncStlThread = NULL;
    ui->loadMenu.asyncStlActive = false;
    SDL_AtomicSet(&ui->loadMenu.asyncStlComplete, 0);
    SDL_AtomicSet(&ui->loadMenu.asyncStlProgressPermille, 0);
    SDL_AtomicSet(&ui->loadMenu.asyncStlProgressStage,
                  (int)CORE_MESH_COMPILE_PROGRESS_STAGE_UNKNOWN);
}

void UIPanel_TickLoadProgress(void) {
    UIPanelState* ui = UIPanel_Get();
    Uint32 now = SDL_GetTicks();
    if (!ui) return;

    if (ui->loadMenu.asyncStlActive &&
        SDL_AtomicGet(&ui->loadMenu.asyncStlComplete) != 0) {
        bool placed = false;
        bool import_ok = ui->loadMenu.asyncStlSucceeded;
        char diagnostics[256];
        char authoring_path[MAX_CONFIG_PATH];
        char runtime_path[MAX_CONFIG_PATH];
        char stl_path[MAX_CONFIG_PATH];

        snprintf(diagnostics, sizeof(diagnostics), "%s", ui->loadMenu.asyncStlDiagnostics);
        snprintf(authoring_path, sizeof(authoring_path), "%s", ui->loadMenu.asyncStlAuthoringPath);
        snprintf(runtime_path, sizeof(runtime_path), "%s", ui->loadMenu.asyncStlRuntimePath);
        snprintf(stl_path, sizeof(stl_path), "%s", ui->loadMenu.asyncStlSourcePath);
        UIPanel_WaitForAsyncStlImport();

        if (import_ok) {
            placed = UIPanel_PlaceImportedStlRuntimeMesh(stl_path,
                                                         authoring_path,
                                                         runtime_path);
            if (placed) {
                UIPanel_RememberLoadedEntry(UI_LOAD_MENU_MODE_STL_IMPORT, stl_path);
                UIPanel_RefreshConfigList();
            }
        }

        if (placed) {
            UIPanel_EndLoadProgressWithDetail(ui, true, "Import STL finished");
        } else {
            char detail[160];
            snprintf(detail,
                     sizeof(detail),
                     "STL import failed: %s",
                     diagnostics[0] ? diagnostics : "import or placement error");
            UIPanel_RecordStlImportFailure(ui, detail);
        }
        return;
    }

    if (ui->loadMenu.asyncStlActive) {
        int progress_permille = SDL_AtomicGet(&ui->loadMenu.asyncStlProgressPermille);
        int progress_stage = SDL_AtomicGet(&ui->loadMenu.asyncStlProgressStage);
        if (progress_permille < 0) progress_permille = 0;
        if (progress_permille > 950) progress_permille = 950;
        ui->loadMenu.loadProgressPermille = progress_permille;
        snprintf(ui->loadMenu.loadProgressDetail,
                 sizeof(ui->loadMenu.loadProgressDetail),
                 "%s",
                 UIPanel_StlProgressStageDetail(progress_stage));
    }

    if ((ui->loadMenu.loadProgressState == UI_LOAD_PROGRESS_COMPLETE ||
         ui->loadMenu.loadProgressState == UI_LOAD_PROGRESS_FAILED) &&
        ui->loadMenu.loadProgressFinishedTicks != 0u &&
        (Uint32)(now - ui->loadMenu.loadProgressFinishedTicks) >= k_load_progress_finished_ttl_ms) {
        ui->loadMenu.loadProgressState = UI_LOAD_PROGRESS_NONE;
        ui->loadMenu.loadProgressMode = UI_LOAD_MENU_MODE_NONE;
        ui->loadMenu.loadProgressStartedTicks = 0u;
        ui->loadMenu.loadProgressFinishedTicks = 0u;
        ui->loadMenu.loadProgressPermille = 0;
        ui->loadMenu.loadProgressPath[0] = '\0';
        ui->loadMenu.loadProgressLabel[0] = '\0';
        ui->loadMenu.loadProgressDetail[0] = '\0';
        UIPanel_LoadMenuClampScroll(ui);
    }
}

static bool UIPanel_PathIsRegularFile(const char* path) {
    struct stat st = {0};
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static bool UIPanel_CopyParentDirectoryPath(const char* path,
                                            int levels,
                                            char* out_path,
                                            size_t out_path_size) {
    size_t length = 0u;
    if (!path || !path[0] || !out_path || out_path_size == 0u || levels <= 0) return false;
    length = strlen(path);
    if (length >= out_path_size) length = out_path_size - 1u;
    memcpy(out_path, path, length);
    out_path[length] = '\0';

    for (int i = 0; i < levels; ++i) {
        char* slash = strrchr(out_path, '/');
        if (!slash || slash == out_path) return false;
        *slash = '\0';
    }

    return out_path[0] != '\0' && UIPanel_PathIsDirectory(out_path);
}

static bool UIPanel_FileBrowserEnsureRuntimeDir(void) {
    if (mkdir("data", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    if (mkdir("data/runtime", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static const char* UIPanel_FileBrowserModeToken(UILoadMenuMode mode) {
    switch (mode) {
        case UI_LOAD_MENU_MODE_JSON: return "json";
        case UI_LOAD_MENU_MODE_SCENE: return "scene";
        case UI_LOAD_MENU_MODE_OBJECT: return "object";
        case UI_LOAD_MENU_MODE_RUNTIME_MESH: return "runtime_mesh";
        case UI_LOAD_MENU_MODE_STL_IMPORT: return "stl_import";
        case UI_LOAD_MENU_MODE_NONE:
        default: return "none";
    }
}

static UILoadMenuMode UIPanel_FileBrowserModeFromToken(const char* token) {
    if (!token || !token[0]) return UI_LOAD_MENU_MODE_NONE;
    if (strcasecmp(token, "json") == 0) return UI_LOAD_MENU_MODE_JSON;
    if (strcasecmp(token, "scene") == 0) return UI_LOAD_MENU_MODE_SCENE;
    if (strcasecmp(token, "object") == 0) return UI_LOAD_MENU_MODE_OBJECT;
    if (strcasecmp(token, "runtime_mesh") == 0 || strcasecmp(token, "mesh") == 0) {
        return UI_LOAD_MENU_MODE_RUNTIME_MESH;
    }
    if (strcasecmp(token, "stl_import") == 0 || strcasecmp(token, "stl") == 0) {
        return UI_LOAD_MENU_MODE_STL_IMPORT;
    }
    return UI_LOAD_MENU_MODE_NONE;
}

static bool UIPanel_SaveFileBrowserMode(const UIPanelState* ui) {
    FILE* file = NULL;
    const char* token = NULL;
    if (!ui) return false;
    if (!UIPanel_FileBrowserEnsureRuntimeDir()) return false;
    token = UIPanel_FileBrowserModeToken(ui->loadMenu.mode);
    file = fopen(k_runtime_file_browser_mode_path, "wb");
    if (!file) return false;
    if (fputs(token, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static const char* UIPanel_FileBrowserEntryStatePath(UILoadMenuMode mode) {
    switch (mode) {
        case UI_LOAD_MENU_MODE_JSON: return k_runtime_last_json_entry_path;
        case UI_LOAD_MENU_MODE_SCENE: return k_runtime_last_scene_entry_path;
        case UI_LOAD_MENU_MODE_OBJECT: return k_runtime_last_object_entry_path;
        case UI_LOAD_MENU_MODE_RUNTIME_MESH: return k_runtime_last_mesh_entry_path;
        case UI_LOAD_MENU_MODE_STL_IMPORT: return k_runtime_last_stl_entry_path;
        case UI_LOAD_MENU_MODE_NONE:
        default: return NULL;
    }
}

static const char* UIPanel_FileBrowserRootStatePath(UILoadMenuMode mode) {
    switch (mode) {
        case UI_LOAD_MENU_MODE_JSON: return k_runtime_json_root_path;
        case UI_LOAD_MENU_MODE_SCENE: return k_runtime_scene_root_path;
        case UI_LOAD_MENU_MODE_OBJECT: return k_runtime_object_root_path;
        case UI_LOAD_MENU_MODE_RUNTIME_MESH: return k_runtime_mesh_root_path;
        case UI_LOAD_MENU_MODE_STL_IMPORT: return k_runtime_stl_root_path;
        case UI_LOAD_MENU_MODE_NONE:
        default: return NULL;
    }
}

static bool UIPanel_SaveRememberedEntryPath(UILoadMenuMode mode, const char* path) {
    FILE* file = NULL;
    const char* state_path = UIPanel_FileBrowserEntryStatePath(mode);
    if (!state_path || !path || !path[0]) return false;
    if (!UIPanel_FileBrowserEnsureRuntimeDir()) return false;
    file = fopen(state_path, "wb");
    if (!file) return false;
    if (fputs(path, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static bool UIPanel_ClearRememberedEntryPath(UILoadMenuMode mode) {
    const char* state_path = UIPanel_FileBrowserEntryStatePath(mode);
    if (!state_path) return false;
    if (remove(state_path) == 0) return true;
    return errno == ENOENT;
}

bool UIPanel_LoadRememberedEntryPath(UILoadMenuMode mode, char* out_path, size_t out_path_size) {
    FILE* file = NULL;
    const char* state_path = UIPanel_FileBrowserEntryStatePath(mode);
    if (!state_path || !out_path || out_path_size == 0u) return false;
    out_path[0] = '\0';
    file = fopen(state_path, "rb");
    if (!file) return false;
    if (!fgets(out_path, (int)out_path_size, file)) {
        fclose(file);
        out_path[0] = '\0';
        return false;
    }
    fclose(file);
    out_path[strcspn(out_path, "\r\n")] = '\0';
    return out_path[0] != '\0';
}

static bool UIPanel_SaveBrowserRootPath(UILoadMenuMode mode, const char* path) {
    FILE* file = NULL;
    const char* state_path = UIPanel_FileBrowserRootStatePath(mode);
    if (!state_path || !path || !path[0]) return false;
    if (!UIPanel_FileBrowserEnsureRuntimeDir()) return false;
    file = fopen(state_path, "wb");
    if (!file) return false;
    if (fputs(path, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static bool UIPanel_LoadBrowserRootPath(UILoadMenuMode mode, char* out_path, size_t out_path_size) {
    FILE* file = NULL;
    const char* state_path = UIPanel_FileBrowserRootStatePath(mode);
    if (!state_path || !out_path || out_path_size == 0u) return false;
    out_path[0] = '\0';
    file = fopen(state_path, "rb");
    if (!file) return false;
    if (!fgets(out_path, (int)out_path_size, file)) {
        fclose(file);
        out_path[0] = '\0';
        return false;
    }
    fclose(file);
    out_path[strcspn(out_path, "\r\n")] = '\0';
    return out_path[0] != '\0';
}

static bool UIPanel_ResolveBrowserRootForMode(UILoadMenuMode mode,
                                              char* out_path,
                                              size_t out_path_size) {
    const char* current_root = (mode == UI_LOAD_MENU_MODE_OBJECT ||
                                mode == UI_LOAD_MENU_MODE_RUNTIME_MESH ||
                                mode == UI_LOAD_MENU_MODE_STL_IMPORT)
                                   ? Global_GetObjectAssetRoot()
                                   : Global_GetInputRoot();
    if (!out_path || out_path_size == 0u) return false;
    out_path[0] = '\0';
    if (UIPanel_LoadBrowserRootPath(mode, out_path, out_path_size) &&
        UIPanel_PathIsDirectory(out_path)) {
        return true;
    }
    out_path[0] = '\0';
    if (current_root && current_root[0] &&
        snprintf(out_path, out_path_size, "%s", current_root) < (int)out_path_size) {
        return UIPanel_PathIsDirectory(out_path);
    }
    return false;
}

static bool UIPanel_ApplyBrowserRootForMode(UILoadMenuMode mode,
                                            const char* path,
                                            bool persist_global_root) {
    (void)persist_global_root;
    if (!path || !path[0] || !UIPanel_PathIsDirectory(path)) return false;
    return UIPanel_SaveBrowserRootPath(mode, path);
}

void UIPanel_RememberLoadedEntry(UILoadMenuMode mode, const char* path) {
    (void)UIPanel_SaveRememberedEntryPath(mode, path);
}

void UIPanel_SetFileBrowserVisible(UIPanelState* ui, bool visible) {
    if (!ui) return;
    ui->loadMenu.visible = visible;
    ui->loadMenu.open = visible;
}

void UIPanel_CloseFileBrowser(UIPanelState* ui) {
    if (!ui) return;
    UIPanel_SetFileBrowserVisible(ui, false);
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.scrollbarDragging = false;
}

void UIPanel_LoadFileBrowserMode(UIPanelState* ui) {
    FILE* file = NULL;
    char line[32];
    if (!ui) return;
    file = fopen(k_runtime_file_browser_mode_path, "rb");
    if (!file) return;
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return;
    }
    fclose(file);
    line[strcspn(line, "\r\n")] = '\0';
    ui->loadMenu.mode = UIPanel_FileBrowserModeFromToken(line);
    UIPanel_SetFileBrowserVisible(ui, ui->loadMenu.mode != UI_LOAD_MENU_MODE_NONE);
}

static bool UIPanel_AddLoadEntry(UIPanelState* ui, const char* label, const char* full_path) {
    if (!ui || !label || !label[0] || !full_path || !full_path[0]) return false;
    if (ui->loadMenu.count >= MAX_CONFIG_FILES) return false;

    for (int i = 0; i < ui->loadMenu.count; ++i) {
        if (strcasecmp(ui->loadMenu.entryPaths[i], full_path) == 0) {
            return false;
        }
    }

    snprintf(ui->loadMenu.entries[ui->loadMenu.count],
             sizeof(ui->loadMenu.entries[0]),
             "%s",
             label);
    snprintf(ui->loadMenu.entryPaths[ui->loadMenu.count],
             sizeof(ui->loadMenu.entryPaths[0]),
             "%s",
             full_path);
    ui->loadMenu.count++;
    return true;
}

static bool UIPanel_PathIsDirectory(const char* path) {
    struct stat st = {0};
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static void UIPanel_SwapLoadEntries(UIPanelState* ui, int a, int b) {
    char name_tmp[128];
    char path_tmp[MAX_CONFIG_PATH];
    if (!ui || a < 0 || b < 0 || a >= ui->loadMenu.count || b >= ui->loadMenu.count) return;
    snprintf(name_tmp, sizeof(name_tmp), "%s", ui->loadMenu.entries[a]);
    snprintf(path_tmp, sizeof(path_tmp), "%s", ui->loadMenu.entryPaths[a]);
    snprintf(ui->loadMenu.entries[a], sizeof(ui->loadMenu.entries[a]), "%s", ui->loadMenu.entries[b]);
    snprintf(ui->loadMenu.entryPaths[a], sizeof(ui->loadMenu.entryPaths[a]), "%s", ui->loadMenu.entryPaths[b]);
    snprintf(ui->loadMenu.entries[b], sizeof(ui->loadMenu.entries[b]), "%s", name_tmp);
    snprintf(ui->loadMenu.entryPaths[b], sizeof(ui->loadMenu.entryPaths[b]), "%s", path_tmp);
}

static void UIPanel_AppendCatalogEntries(UIPanelState* ui,
                                         const LineDrawingFileCatalogEntry* entries,
                                         int count) {
    int i = 0;
    if (!ui || !entries || count <= 0) return;
    for (i = 0; i < count && ui->loadMenu.count < MAX_CONFIG_FILES; ++i) {
        (void)UIPanel_AddLoadEntry(ui, entries[i].label, entries[i].path);
    }
}

static void UIPanel_ScanJsonDirectory(UIPanelState* ui, const char* root_dir) {
    LineDrawingFileCatalogEntry entries[MAX_CONFIG_FILES];
    int count = 0;
    if (!ui || !root_dir || !root_dir[0]) return;
    memset(entries, 0, sizeof(entries));
    count = LineDrawingFileCatalog_ScanLayoutEntries(entries, MAX_CONFIG_FILES, root_dir);
    UIPanel_AppendCatalogEntries(ui, entries, count);
}

static void UIPanel_ScanSceneRoot(UIPanelState* ui, const char* root_dir) {
    LineDrawingFileCatalogEntry entries[MAX_CONFIG_FILES];
    int count = 0;
    if (!ui || !root_dir || !root_dir[0]) return;
    memset(entries, 0, sizeof(entries));
    count = LineDrawingFileCatalog_ScanSceneEntries(entries, MAX_CONFIG_FILES, root_dir);
    UIPanel_AppendCatalogEntries(ui, entries, count);
}

static void UIPanel_ScanObjectAssetRoot(UIPanelState* ui, const char* root_dir) {
    LineDrawingFileCatalogEntry entries[MAX_CONFIG_FILES];
    int count = 0;
    if (!ui || !root_dir || !root_dir[0]) return;
    memset(entries, 0, sizeof(entries));
    count = LineDrawingFileCatalog_ScanLayoutEntries(entries, MAX_CONFIG_FILES, root_dir);
    UIPanel_AppendCatalogEntries(ui, entries, count);
}

static void UIPanel_ScanRuntimeMeshRoot(UIPanelState* ui, const char* root_dir) {
    LineDrawingFileCatalogEntry entries[MAX_CONFIG_FILES];
    int count = 0;
    if (!ui || !root_dir || !root_dir[0]) return;
    memset(entries, 0, sizeof(entries));
    count = LineDrawingFileCatalog_ScanRuntimeMeshEntries(entries, MAX_CONFIG_FILES, root_dir);
    UIPanel_AppendCatalogEntries(ui, entries, count);
}

static void UIPanel_ScanStlRoot(UIPanelState* ui, const char* root_dir) {
    LineDrawingFileCatalogEntry entries[MAX_CONFIG_FILES];
    int count = 0;
    if (!ui || !root_dir || !root_dir[0]) return;
    memset(entries, 0, sizeof(entries));
    count = LineDrawingFileCatalog_ScanStlEntries(entries, MAX_CONFIG_FILES, root_dir);
    UIPanel_AppendCatalogEntries(ui, entries, count);
}

static int UIPanel_FindActiveLoadMenuIndex(const UIPanelState* ui) {
    const char* active_path = NULL;
    if (!ui) return -1;
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        active_path = Global_GetCurrentConfigPath();
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        active_path = Global_GetCurrentSceneAuthoringPath();
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT) {
        active_path = Global_GetCurrentObjectAssetPath();
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        active_path = Global_GetLastObjectRuntimeMeshPath();
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        active_path = NULL;
    }
    if (!active_path || !active_path[0]) return -1;
    return UIPanel_FindLoadMenuIndexForPath(ui, active_path);
}

static int UIPanel_FindLoadMenuIndexForPath(const UIPanelState* ui, const char* path) {
    if (!ui || !path || !path[0]) return -1;
    for (int i = 0; i < ui->loadMenu.count; ++i) {
        if (strcmp(ui->loadMenu.entryPaths[i], path) == 0) {
            return i;
        }
    }
    return -1;
}

const char* UIPanel_GetActiveSessionPathForMode(const UIPanelState* ui) {
    if (!ui) return NULL;
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        return Global_GetCurrentConfigPath();
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        return Global_GetCurrentSceneAuthoringPath();
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT) {
        return Global_GetCurrentObjectAssetPath();
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        return Global_GetLastObjectRuntimeMeshPath();
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        return NULL;
    }
    return NULL;
}

static const char* UIPanel_GetPreferredSessionPathForMode(const UIPanelState* ui) {
    const LineDrawingRecentContexts* recents = NULL;
    const char* active_path = NULL;
    if (!ui) return NULL;

    active_path = UIPanel_GetActiveSessionPathForMode(ui);
    if (UIPanel_PathIsRegularFile(active_path)) {
        return active_path;
    }

    recents = Global_GetRecentContexts();
    if (!recents) return NULL;
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        return (recents->layouts.count > 0 && recents->layouts.paths[0][0] != '\0')
                   ? recents->layouts.paths[0]
                   : NULL;
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        return (recents->scenes.count > 0 && recents->scenes.paths[0][0] != '\0')
                   ? recents->scenes.paths[0]
                   : NULL;
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT) {
        return (recents->object_assets.count > 0 && recents->object_assets.paths[0][0] != '\0')
                   ? recents->object_assets.paths[0]
                   : NULL;
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        active_path = Global_GetLastObjectRuntimeMeshPath();
        return active_path && active_path[0] ? active_path : NULL;
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        char remembered_path[MAX_CONFIG_PATH];
        if (UIPanel_LoadRememberedEntryPath(UI_LOAD_MENU_MODE_STL_IMPORT,
                                            remembered_path,
                                            sizeof(remembered_path))) {
            return NULL;
        }
        return NULL;
    }
    return NULL;
}

static bool UIPanel_DeriveBrowserRootFromActiveSession(const UIPanelState* ui,
                                                       char* out_path,
                                                       size_t out_path_size) {
    const char* active_path = NULL;
    int levels = 0;
    if (!ui || !out_path || out_path_size == 0u) return false;
    active_path = UIPanel_GetPreferredSessionPathForMode(ui);
    if (!active_path || !active_path[0]) return false;

    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        levels = 1;
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        levels = 2;
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT) {
        levels = 1;
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        levels = 1;
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        levels = 1;
    } else {
        return false;
    }

    return UIPanel_CopyParentDirectoryPath(active_path, levels, out_path, out_path_size);
}

static void UIPanel_LoadMenuClose(UIPanelState* ui) {
    if (!ui) return;
    UIPanel_CloseFileBrowser(ui);
    ui->loadMenu.mode = UI_LOAD_MENU_MODE_NONE;
    (void)UIPanel_SaveFileBrowserMode(ui);
}

static int UIPanel_CountEntriesForRoot(const char* root_path, UILoadMenuMode mode) {
    UIPanelState probe;
    if (!root_path || !root_path[0]) return 0;
    memset(&probe, 0, sizeof(probe));
    if (mode == UI_LOAD_MENU_MODE_JSON) {
        UIPanel_ScanJsonDirectory(&probe, root_path);
    } else if (mode == UI_LOAD_MENU_MODE_SCENE) {
        UIPanel_ScanSceneRoot(&probe, root_path);
    } else if (mode == UI_LOAD_MENU_MODE_OBJECT) {
        UIPanel_ScanObjectAssetRoot(&probe, root_path);
    } else if (mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        UIPanel_ScanRuntimeMeshRoot(&probe, root_path);
    } else if (mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        UIPanel_ScanStlRoot(&probe, root_path);
    }
    return probe.loadMenu.count;
}

static bool UIPanel_ShowFileBrowserForCurrentRoot(UILoadMenuMode mode,
                                                  int anchor_button_id) {
    UIPanelState* ui = UIPanel_Get();
    char resolved_root[MAX_CONFIG_PATH];
    if (!ui) return false;

    ui->loadMenu.mode = mode;
    ui->loadMenu.anchorButtonId = anchor_button_id;
    if (UIPanel_ResolveBrowserRootForMode(mode, resolved_root, sizeof(resolved_root))) {
        snprintf(ui->loadMenu.rootPath, sizeof(ui->loadMenu.rootPath), "%s", resolved_root);
    } else if ((mode == UI_LOAD_MENU_MODE_OBJECT ||
                mode == UI_LOAD_MENU_MODE_RUNTIME_MESH ||
                mode == UI_LOAD_MENU_MODE_STL_IMPORT) &&
               Global_GetObjectAssetRoot() &&
               Global_GetObjectAssetRoot()[0]) {
        snprintf(ui->loadMenu.rootPath,
                 sizeof(ui->loadMenu.rootPath),
                 "%s",
                 Global_GetObjectAssetRoot());
        (void)UIPanel_SaveBrowserRootPath(mode, ui->loadMenu.rootPath);
    } else if (Global_GetInputRoot() && Global_GetInputRoot()[0]) {
        snprintf(ui->loadMenu.rootPath, sizeof(ui->loadMenu.rootPath), "%s", Global_GetInputRoot());
        (void)UIPanel_SaveBrowserRootPath(mode, ui->loadMenu.rootPath);
    } else {
        ui->loadMenu.rootPath[0] = '\0';
    }
    UIPanel_SetActiveLeftTab(ui, UI_PANEL_LEFT_TAB_FILE);
    UIPanel_OnWindowResized(Global_GetScreenWidth(), Global_GetScreenHeight());
    UIPanel_RefreshConfigList();
    UIPanel_SetFileBrowserVisible(ui, true);
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.scrollOffsetPx = 0.0f;
    ui->loadMenu.scrollbarDragging = false;
    UIPanel_LoadMenuClampScroll(ui);
    UIPanel_CloseSaveDialog(ui);
    UIPanel_CloseRootDialog(ui);
    UIPanel_ClosePrismDimensionDialog(ui);
    UIPanel_CloseSceneBoundsDialog(ui);
    UIPanel_CloseConstructionPlaneDialog(ui);
    UIPanel_CloseObjectTransformDialog(ui);
    (void)UIPanel_SaveFileBrowserMode(ui);
    return true;
}

static bool UIPanel_RestorePersistedEntryForMode(UILoadMenuMode mode) {
    const char* path = NULL;
    if (mode == UI_LOAD_MENU_MODE_JSON) {
        path = Global_GetLastLayoutPath();
        if (!path || !path[0]) return false;
        return UIPanel_LoadLayoutFromPath(path);
    }
    if (mode == UI_LOAD_MENU_MODE_SCENE) {
        path = Global_GetLastSceneAuthoringPath();
        if (!path || !path[0]) return false;
        return UIPanel_LoadSceneFromPath(path);
    }
    if (mode == UI_LOAD_MENU_MODE_OBJECT) {
        path = Global_GetLastObjectAssetPath();
        if (!path || !path[0]) return false;
        return UIPanel_LoadObjectAssetFromPath(path);
    }
    if (mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        path = Global_GetLastObjectRuntimeMeshPath();
        if (!path || !path[0]) return false;
        return UIPanel_PlaceRuntimeMeshAsSceneInstance(path);
    }
    if (mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        char remembered_path[MAX_CONFIG_PATH];
        if (!UIPanel_LoadRememberedEntryPath(mode, remembered_path, sizeof(remembered_path))) {
            return false;
        }
        return UIPanel_PlaceCachedStlOrStartImport(UIPanel_Get(), remembered_path, remembered_path);
    }
    return false;
}

bool UIPanel_LoadJsonFromFolderSelection(const char* selected_folder, bool persist_root) {
    if (!selected_folder || !selected_folder[0]) return false;
    if (UIPanel_CountEntriesForRoot(selected_folder, UI_LOAD_MENU_MODE_JSON) <= 0) {
        UIPanelState* ui = UIPanel_Get();
        if (ui) {
            UIPanel_SetFileBrowserVisible(ui, false);
        }
        UIPanel_SetFilePaneActionStatus("Load JSON failed: no JSON files found.");
        return false;
    }
    if (!UIPanel_ApplyBrowserRootForMode(UI_LOAD_MENU_MODE_JSON, selected_folder, persist_root)) {
        UIPanel_SetFilePaneActionStatus("Load JSON failed: root could not be applied.");
        return false;
    }
    if (!Global_SetInputRoot(selected_folder, persist_root)) {
        UIPanel_SetFilePaneActionStatus("Load JSON failed: input root rejected.");
        return false;
    }
    return UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_JSON,
                                                 UI_BTN_LOAD_JSON);
}

bool UIPanel_LoadSceneFromFolderSelection(const char* selected_folder, bool persist_root) {
    if (!selected_folder || !selected_folder[0]) return false;
    if (UIPanel_CountEntriesForRoot(selected_folder, UI_LOAD_MENU_MODE_SCENE) <= 0) {
        UIPanelState* ui = UIPanel_Get();
        if (ui) {
            UIPanel_SetFileBrowserVisible(ui, false);
        }
        UIPanel_SetFilePaneActionStatus("Load scene failed: no scene files found.");
        return false;
    }
    if (!UIPanel_ApplyBrowserRootForMode(UI_LOAD_MENU_MODE_SCENE, selected_folder, persist_root)) {
        UIPanel_SetFilePaneActionStatus("Load scene failed: root could not be applied.");
        return false;
    }
    if (!Global_SetInputRoot(selected_folder, persist_root)) {
        UIPanel_SetFilePaneActionStatus("Load scene failed: input root rejected.");
        return false;
    }
    return UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_SCENE,
                                                 UI_BTN_LOAD_SCENE);
}

bool UIPanel_LoadObjectAssetFromFolderSelection(const char* selected_folder, bool persist_root) {
    if (!selected_folder || !selected_folder[0]) return false;
    if (UIPanel_CountEntriesForRoot(selected_folder, UI_LOAD_MENU_MODE_OBJECT) <= 0) {
        UIPanelState* ui = UIPanel_Get();
        if (ui) {
            UIPanel_SetFileBrowserVisible(ui, false);
        }
        UIPanel_SetFilePaneActionStatus("Load asset failed: no object asset files found.");
        return false;
    }
    if (!UIPanel_ApplyBrowserRootForMode(UI_LOAD_MENU_MODE_OBJECT, selected_folder, persist_root)) {
        UIPanel_SetFilePaneActionStatus("Load asset failed: root could not be applied.");
        return false;
    }
    if (!Global_SetObjectAssetRoot(selected_folder, persist_root)) {
        UIPanel_SetFilePaneActionStatus("Load asset failed: object asset root rejected.");
        return false;
    }
    return UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_OBJECT,
                                                 UI_BTN_LOAD_JSON);
}

bool UIPanel_LoadStlFromFolderSelection(const char* selected_folder, bool persist_root) {
    if (!selected_folder || !selected_folder[0]) return false;
    if (UIPanel_CountEntriesForRoot(selected_folder, UI_LOAD_MENU_MODE_STL_IMPORT) <= 0) {
        UIPanelState* ui = UIPanel_Get();
        if (ui) {
            UIPanel_SetFileBrowserVisible(ui, false);
        }
        UIPanel_SetFilePaneActionStatus("Import STL failed: no STL files found.");
        return false;
    }
    if (!UIPanel_ApplyBrowserRootForMode(UI_LOAD_MENU_MODE_STL_IMPORT,
                                         selected_folder,
                                         persist_root)) {
        UIPanel_SetFilePaneActionStatus("Import STL failed: root could not be applied.");
        return false;
    }
    if (!Global_SetObjectAssetRoot(selected_folder, persist_root)) {
        UIPanel_SetFilePaneActionStatus("Import STL failed: object asset root rejected.");
        return false;
    }
    return UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_STL_IMPORT,
                                                 UI_BTN_LOAD_STL);
}

void UIPanel_RefreshConfigList(void) {
    UIPanelState* ui = UIPanel_Get();
    const char* input_root = NULL;
    if (!ui) return;

    ui->loadMenu.count = 0;
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.activeIndex = -1;
    memset(ui->loadMenu.entries, 0, sizeof(ui->loadMenu.entries));
    memset(ui->loadMenu.entryPaths, 0, sizeof(ui->loadMenu.entryPaths));
    input_root = ui->loadMenu.rootPath[0]
                     ? ui->loadMenu.rootPath
                     : ((ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT ||
                         ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH ||
                         ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT)
                            ? Global_GetObjectAssetRoot()
                            : Global_GetInputRoot());
    if (!ui->loadMenu.rootPath[0] && input_root && input_root[0]) {
        snprintf(ui->loadMenu.rootPath, sizeof(ui->loadMenu.rootPath), "%s", input_root);
    }

    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        UIPanel_ScanJsonDirectory(ui, input_root);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        UIPanel_ScanSceneRoot(ui, input_root);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT) {
        UIPanel_ScanObjectAssetRoot(ui, input_root);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        UIPanel_ScanRuntimeMeshRoot(ui, input_root);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        UIPanel_ScanStlRoot(ui, input_root);
    }

    for (int i = 0; i < ui->loadMenu.count - 1; ++i) {
        for (int j = i + 1; j < ui->loadMenu.count; ++j) {
            if (strcasecmp(ui->loadMenu.entries[j], ui->loadMenu.entries[i]) < 0) {
                UIPanel_SwapLoadEntries(ui, i, j);
            }
        }
    }

    ui->loadMenu.activeIndex = UIPanel_FindActiveLoadMenuIndex(ui);
    if (ui->loadMenu.activeIndex < 0) {
        ui->loadMenu.activeIndex = UIPanel_FindRememberedLoadMenuIndex(ui);
    }
    if (ui->loadMenu.activeIndex >= 0) {
        UIPanel_LoadMenuScrollIndexIntoView(ui, ui->loadMenu.activeIndex);
    }
    if (!ui->loadMenu.visible && ui->loadMenu.mode != UI_LOAD_MENU_MODE_NONE) {
        UIPanel_SetFileBrowserVisible(ui, true);
    }
    UIPanel_LoadMenuClampScroll(ui);
}

void UIPanel_ToggleLoadMenu(void) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui) return;
    if (ui->loadMenu.visible) {
        UIPanel_LoadMenuClose(ui);
        return;
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_NONE) {
        return;
    }
    UIPanel_RefreshConfigList();
    if (ui->loadMenu.count <= 0) {
        UIPanel_SetFileBrowserVisible(ui, true);
        return;
    }
    UIPanel_SetFileBrowserVisible(ui, true);
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.scrollOffsetPx = 0.0f;
    UIPanel_LoadMenuClampScroll(ui);
    (void)UIPanel_SaveFileBrowserMode(ui);
}

bool UIPanel_IsLoadMenuOpen(void) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui) return false;
    return ui->activeLeftTab == UI_PANEL_LEFT_TAB_FILE &&
           ui->loadMenu.visible &&
           ui->loadMenu.mode != UI_LOAD_MENU_MODE_NONE;
}

void UIPanel_ActivateJsonBrowser(void) {
    (void)UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_JSON, UI_BTN_LOAD_JSON);
}

void UIPanel_ActivateSceneBrowser(void) {
    (void)UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_SCENE, UI_BTN_LOAD_SCENE);
}

void UIPanel_ActivateObjectAssetBrowser(void) {
    (void)UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_OBJECT, UI_BTN_LOAD_JSON);
}

void UIPanel_ActivateRuntimeMeshBrowser(void) {
    (void)UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_RUNTIME_MESH,
                                                UI_BTN_LOAD_MESH_ASSET);
}

void UIPanel_ActivateStlImportBrowser(void) {
    (void)UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_STL_IMPORT,
                                                UI_BTN_LOAD_STL);
}

bool UIPanel_RestorePersistedFileSession(void) {
    UIPanelState* ui = UIPanel_Get();
    bool restored = false;
    if (!ui || ui->loadMenu.mode == UI_LOAD_MENU_MODE_NONE) return false;

    restored = UIPanel_RestorePersistedEntryForMode(ui->loadMenu.mode);
    UIPanel_RefreshConfigList();
    UIPanel_SetFileBrowserVisible(ui, true);
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.activeIndex = UIPanel_FindActiveLoadMenuIndex(ui);
    if (ui->loadMenu.activeIndex < 0) {
        ui->loadMenu.activeIndex = UIPanel_FindRememberedLoadMenuIndex(ui);
    }
    ui->loadMenu.scrollOffsetPx = 0.0f;
    UIPanel_LoadMenuClampScroll(ui);
    return restored;
}

bool UIPanel_GetFileBrowserRestoreSummary(
    const UIPanelState* ui,
    UIPanelFileBrowserRestoreSummary* out_summary) {
    const char* active_path = NULL;
    char remembered_path[MAX_CONFIG_PATH];
    if (!ui || !out_summary) return false;

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->mode = ui->loadMenu.mode;
    out_summary->hasMode = ui->loadMenu.mode != UI_LOAD_MENU_MODE_NONE;
    out_summary->visible = ui->loadMenu.visible;
    out_summary->activeIndex = -1;
    out_summary->rememberedIndex = -1;
    snprintf(out_summary->rootPath,
             sizeof(out_summary->rootPath),
             "%s",
             ui->loadMenu.rootPath);

    active_path = UIPanel_GetActiveSessionPathForMode(ui);
    if (active_path && active_path[0]) {
        out_summary->hasActiveSessionPath = true;
        out_summary->activeSessionPathExists = UIPanel_PathIsRegularFile(active_path);
        out_summary->activeIndex = UIPanel_FindActiveLoadMenuIndex(ui);
        snprintf(out_summary->activeSessionPath,
                 sizeof(out_summary->activeSessionPath),
                 "%s",
                 active_path);
    }

    remembered_path[0] = '\0';
    if (UIPanel_LoadRememberedEntryPath(ui->loadMenu.mode,
                                        remembered_path,
                                        sizeof(remembered_path))) {
        out_summary->hasRememberedEntryPath = true;
        out_summary->rememberedEntryExists = UIPanel_PathIsRegularFile(remembered_path);
        out_summary->rememberedIndex = UIPanel_FindRememberedLoadMenuIndex(ui);
        snprintf(out_summary->rememberedEntryPath,
                 sizeof(out_summary->rememberedEntryPath),
                 "%s",
                 remembered_path);
    }

    return true;
}

bool UIPanel_FocusFileBrowserOnActiveSession(void) {
    UIPanelState* ui = UIPanel_Get();
    const char* preferred_path = NULL;
    char root_path[MAX_CONFIG_PATH];
    if (!ui || ui->loadMenu.mode == UI_LOAD_MENU_MODE_NONE) return false;
    preferred_path = UIPanel_GetPreferredSessionPathForMode(ui);
    if (!preferred_path || !preferred_path[0]) return false;
    if (!UIPanel_DeriveBrowserRootFromActiveSession(ui, root_path, sizeof(root_path))) {
        return false;
    }

    if (!UIPanel_ApplyBrowserRootForMode(ui->loadMenu.mode, root_path, false)) {
        return false;
    }
    snprintf(ui->loadMenu.rootPath, sizeof(ui->loadMenu.rootPath), "%s", root_path);
    UIPanel_SetFileBrowserVisible(ui, true);
    UIPanel_RefreshConfigList();
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.activeIndex = UIPanel_FindActiveLoadMenuIndex(ui);
    if (ui->loadMenu.activeIndex < 0) {
        ui->loadMenu.activeIndex = UIPanel_FindLoadMenuIndexForPath(ui, preferred_path);
    }
    if (ui->loadMenu.activeIndex < 0) return false;
    UIPanel_LoadMenuScrollIndexIntoView(ui, ui->loadMenu.activeIndex);
    (void)UIPanel_SaveFileBrowserMode(ui);
    return true;
}

bool UIPanel_ClearRememberedFileBrowserEntry(void) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui || ui->loadMenu.mode == UI_LOAD_MENU_MODE_NONE) return false;
    if (!UIPanel_ClearRememberedEntryPath(ui->loadMenu.mode)) return false;
    UIPanel_RefreshConfigList();
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.activeIndex = UIPanel_FindActiveLoadMenuIndex(ui);
    if (ui->loadMenu.activeIndex >= 0) {
        UIPanel_LoadMenuScrollIndexIntoView(ui, ui->loadMenu.activeIndex);
    }
    return true;
}

bool UIPanel_LoadLayoutFromPath(const char* path) {
    GlobalState* state = Global_Get();
    if (!state || !path || path[0] == '\0') return false;

    Editor_ClearHistory(&state->editor);

    if (Layout_LoadFromFile(&state->layout, path)) {
        SDL_Log("[UI] Loaded layout %s", path);
        Global_OnLayoutLoaded(path);
        UIPanel_RememberLoadedEntry(UI_LOAD_MENU_MODE_JSON, path);
        UIPanel_RefreshConfigList();
        UIPanel_ResetEditorTransientSelection(&state->editor);
        UIPanel_RefreshViewportAfterSceneDocumentLoad(state);
        Editor_HistoryCapture(&state->editor, &state->layout);
        return true;
    }

    SDL_Log("[UI] Failed to load layout %s", path);
    UIPanel_SetFilePaneActionStatus("Load JSON failed: read or parse error.");
    return false;
}

bool UIPanel_LoadObjectAssetFromPath(const char* path) {
    GlobalState* state = Global_Get();
    ObjectAuthoringDocument loaded_authoring;
    bool has_authoring = false;
    char diagnostics[256];
    if (!state || !path || path[0] == '\0') return false;

    Editor_ClearHistory(&state->editor);
    ObjectAuthoringDocument_Init(&loaded_authoring);

    if (LayoutObjectAssetMeshAuthoring_LoadWithAuthoring(&state->layout,
                                                         &loaded_authoring,
                                                         &has_authoring,
                                                         path,
                                                         diagnostics,
                                                         sizeof(diagnostics))) {
        SDL_Log("[UI] Loaded object asset %s", path);
        if (has_authoring) {
            ObjectAuthoringSession_Clear(&state->objectAuthoring);
            state->objectAuthoring.attached = true;
            state->objectAuthoring.sourceSceneObjectId = 0u;
            (void)ObjectAuthoringDocument_Copy(&state->objectAuthoring.document,
                                               &loaded_authoring);
        } else {
            (void)ObjectAuthoringSession_ResetFromLayout(&state->objectAuthoring,
                                                         &state->layout,
                                                         0u);
        }
        Global_OnObjectAssetLoaded(path);
        UIPanel_RememberLoadedEntry(UI_LOAD_MENU_MODE_OBJECT, path);
        UIPanel_RefreshConfigList();
        state->editor.selectedAnchorIndex = -1;
        state->editor.selectedWallIndex = -1;
        state->editor.selectedObject3DId = 0u;
        state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        state->editor.hoveredAnchorIndex = -1;
        state->editor.hoveredWallIndex = -1;
        state->editor.hoveredObject3DId = 0u;
        state->editor.hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        state->editor.hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        state->editor.hoveredHandleAnchor = -1;
        state->editor.hoveredHandleComponent = -1;
        state->editor.hoveredGizmoAxis = -1;
        state->editor.hoveredObject3DGizmoAxis = -1;
        state->editor.activeObject3DGizmoAxis = -1;
        Editor_HistoryCapture(&state->editor, &state->layout);
        ObjectAuthoringDocument_Free(&loaded_authoring);
        return true;
    }

    ObjectAuthoringDocument_Free(&loaded_authoring);
    SDL_Log("[UI] Failed to load object asset %s%s%s%s",
            path,
            diagnostics[0] ? " (" : "",
            diagnostics[0] ? diagnostics : "",
            diagnostics[0] ? ")" : "");
    {
        char status[160];
        snprintf(status,
                 sizeof(status),
                 "Load asset failed: %s",
                 diagnostics[0] ? diagnostics : "read or parse error");
        UIPanel_SetFilePaneActionStatus(status);
    }
    return false;
}

static bool UIPanel_LoadEntryByIndex(int index) {
    UIPanelState* ui = UIPanel_Get();
    bool loaded = false;
    if (!ui || index < 0 || index >= ui->loadMenu.count) return false;
    if (ui->loadMenu.asyncStlActive) {
        snprintf(ui->loadMenu.loadProgressDetail,
                 sizeof(ui->loadMenu.loadProgressDetail),
                 "Import STL running in background");
        if (UIPanel_FindLoadProgressIndex(ui) >= 0) {
            UIPanel_LoadMenuScrollIndexIntoView(ui, UIPanel_FindLoadProgressIndex(ui));
        }
        return true;
    }
    UIPanel_BeginLoadProgress(ui,
                              ui->loadMenu.mode,
                              ui->loadMenu.entryPaths[index],
                              ui->loadMenu.entries[index]);
    UIPanel_LoadMenuScrollIndexIntoView(ui, index);
    if (ui->loadMenu.rootPath[0] != '\0' &&
        (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT ||
         ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH ||
         ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT)) {
        (void)Global_SetObjectAssetRoot(ui->loadMenu.rootPath, true);
    } else if (ui->loadMenu.rootPath[0] != '\0') {
        (void)Global_SetInputRoot(ui->loadMenu.rootPath, true);
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        loaded = UIPanel_LoadLayoutFromPath(ui->loadMenu.entryPaths[index]);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        loaded = UIPanel_LoadSceneFromPath(ui->loadMenu.entryPaths[index]);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT) {
        loaded = UIPanel_LoadObjectAssetFromPath(ui->loadMenu.entryPaths[index]);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        loaded = UIPanel_PlaceRuntimeMeshAsSceneInstance(ui->loadMenu.entryPaths[index]);
        if (loaded) {
            UIPanel_RememberLoadedEntry(UI_LOAD_MENU_MODE_RUNTIME_MESH,
                                        ui->loadMenu.entryPaths[index]);
        }
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        loaded = UIPanel_PlaceCachedStlOrStartImport(ui,
                                                     ui->loadMenu.entryPaths[index],
                                                     ui->loadMenu.entries[index]);
        if (loaded) {
            UIPanel_LoadMenuScrollIndexIntoView(ui, index);
            return true;
        }
        UIPanel_LoadMenuScrollIndexIntoView(ui, index);
        return false;
    }
    UIPanel_EndLoadProgress(ui, loaded);
    UIPanel_LoadMenuScrollIndexIntoView(ui, index);
    return loaded;
}

bool UIPanel_HandleLoadMenuWheel(int mouseX, int mouseY, float wheel_delta) {
    UIPanelState* ui = UIPanel_Get();
    SDL_Rect rect = {0, 0, 0, 0};
    const float row_step = 24.0f;
    if (!ui || !UIPanel_IsLoadMenuOpen()) return false;
    rect = UIPanel_GetLoadMenuRect(ui);
    if (!SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &rect)) return false;
    if (!UIPanel_LoadMenuHasScrollableContent(ui)) return true;
    ui->loadMenu.scrollOffsetPx -= wheel_delta * (row_step * 3.0f);
    UIPanel_LoadMenuClampScroll(ui);
    return true;
}

bool UIPanel_HandleLoadMenuClick(int mouseX, int mouseY) {
    UIPanelState* ui = UIPanel_Get();
    SDL_Rect rect = {0, 0, 0, 0};
    SDL_Rect track = {0, 0, 0, 0};
    SDL_Rect thumb = {0, 0, 0, 0};
    int index = -1;
    if (!ui || !UIPanel_IsLoadMenuOpen()) return false;

    rect = UIPanel_GetLoadMenuRect(ui);
    if (!SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &rect)) {
        return false;
    }

    {
        SDL_Rect set_dir_button = UIPanel_GetLoadMenuSetDirectoryButtonRect(ui);
        if (set_dir_button.w > 0 &&
            SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &set_dir_button)) {
            return UIPanel_OpenDirectoryDialogForActiveBrowser();
        }
    }

    if (ui->loadMenu.count <= 0) {
        return true;
    }

    if (UIPanel_LoadMenuHasScrollableContent(ui)) {
        track = UIPanel_GetLoadMenuScrollTrackRect(ui);
        thumb = UIPanel_GetLoadMenuScrollThumbRect(ui);
        if (SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &track)) {
            float max_offset = UIPanel_LoadMenuMaxScrollOffset(ui);
            float ratio = 0.0f;
            if (!SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &thumb)) {
                int usable_h = track.h - thumb.h;
                int rel_y = mouseY - track.y - (thumb.h / 2);
                if (usable_h > 0) {
                    ratio = (float)rel_y / (float)usable_h;
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;
                    ui->loadMenu.scrollOffsetPx = ratio * max_offset;
                    UIPanel_LoadMenuClampScroll(ui);
                }
            }
            return true;
        }
    }

    index = UIPanel_LoadMenuIndexAtPoint(ui, mouseX, mouseY);
    if (index >= 0) {
        if (UIPanel_LoadEntryByIndex(index)) {
            ui->loadMenu.activeIndex = UIPanel_FindActiveLoadMenuIndex(ui);
        }
        return true;
    }

    return true;
}

void UIPanel_HandleMouseMotion(int mouseX, int mouseY) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    PrimitivePlacementPreviewKind preview = PRIMITIVE_PLACEMENT_PREVIEW_NONE;
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    const bool has_face_target = state &&
        state->editor.selectedObjectAssetBodyId != 0u &&
        state->editor.selectedObjectAssetFace != OBJECT3D_FACE_NONE;
    const bool sketch_active = state &&
        (state->editor.objectFaceSketchToolArmed ||
         state->editor.objectFaceSketchDragging ||
         state->editor.objectFaceSketchHasRectangle);
    const bool use_face_sketch_controls = object_mode && (has_face_target || sketch_active);
    if (ui) {
        for (int i = 0; i < ui->count; ++i) {
            UIButton* btn = &ui->buttons[i];
            const SDL_Rect r = btn->bounds;
            btn->hovered = mouseX >= r.x && mouseX <= r.x + r.w &&
                           mouseY >= r.y && mouseY <= r.y + r.h;
            if (btn->hovered && state && state->spaceMode == SPACE_MODE_3D) {
                if (btn->id == UI_BTN_CREATE_PLANE && !use_face_sketch_controls) {
                    preview = PRIMITIVE_PLACEMENT_PREVIEW_PLANE;
                } else if (btn->id == UI_BTN_CREATE_RECT_PRISM && !use_face_sketch_controls) {
                    preview = PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM;
                }
            }
        }
    }
    if (state && state->editor.primitivePlacementPreview != preview) {
        state->editor.primitivePlacementPreview = preview;
        Global_FlagGridChanged();
    }

    UIPanel_HandleSceneListMouseMotion(mouseX, mouseY);
    UIPanel_ObjectWorkspaceHandleModelTreeMouseMotion(mouseX, mouseY);

    if (!ui || !UIPanel_IsLoadMenuOpen()) {
        if (ui) ui->loadMenu.hoverIndex = -1;
        return;
    }

    ui->loadMenu.hoverIndex = UIPanel_LoadMenuIndexAtPoint(ui, mouseX, mouseY);
}
