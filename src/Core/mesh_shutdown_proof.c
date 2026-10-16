#include "Core/mesh_shutdown_proof.h"
#include "Core/global_state.h"
#include "Layout/layout_json.h"
#include "Layout/scene/layout_mesh_preview_cache.h"
#include "UI/ui_panel.h"
#include "vk_renderer.h"
#include "core_time.h"
#include <stdio.h>
#include <string.h>
bool MeshShutdownProof_Run(AppContext *app, AppCallbacks *callbacks,
                           const char *layout, const char *capture,
                           void (*shutdown)(AppContext *)) {
    GlobalState *s = Global_Get();
    bool ok = Layout_LoadFromFile(&s->layout, layout);
    if (ok) Global_OnLayoutLoaded(layout);
    char *before = Layout_SaveToString(&s->layout);
    size_t undo = Editor_UndoCount(&s->editor), redo = Editor_RedoCount(&s->editor);
    bool dirty = s->layoutDirtySinceSave;
    callbacks->handleUpdate(app);
    ok = ok && App_RenderOnce(app, callbacks->handleRender);
    size_t pending = Layout_MeshPreviewCacheRead().pending_jobs;
    size_t objects = s->layout.objectStore.count;
    UIPanel_BeginInputRootDialog();
    strcpy(UIPanel_Get()->rootDialog.buffer, "preserved-unsaved-root-draft");
    if (capture) ok = ok && vk_renderer_request_capture(app->renderer, capture) == VK_SUCCESS &&
                           App_RenderOnce(app, callbacks->handleRender);
    SDL_Event quit = {.type = SDL_QUIT};
    ok = ok && SDL_PushEvent(&quit) == 1;
    App_Run(app, callbacks);
    char *after = Layout_SaveToString(&s->layout);
    ok = ok && before && after && !strcmp(before, after) &&
         undo == Editor_UndoCount(&s->editor) && redo == Editor_RedoCount(&s->editor) &&
         dirty == s->layoutDirtySinceSave &&
         !strcmp(UIPanel_Get()->rootDialog.buffer, "preserved-unsaved-root-draft");
    Layout_FreeString(before); Layout_FreeString(after);
    CoreTimeNs start = core_time_now_ns();
    shutdown(app);
    uint64_t elapsed = core_time_diff_ns(core_time_now_ns(), start) / 1000000u;
    ok = ok && elapsed < 2000 && objects == 1297 && pending > 0;
    fprintf(stdout, "MESH_SHUTDOWN status=%s objects=%zu pending=%zu shutdown_ms=%llu document=preserved history=preserved draft=preserved\n",
            ok ? "pass" : "fail", objects, pending, (unsigned long long)elapsed);
    return ok;
}
