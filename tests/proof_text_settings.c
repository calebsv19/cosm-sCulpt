#define _DARWIN_C_SOURCE
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/workspace_authoring/line_drawing_workspace_authoring_host.h"
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int calls, fail_after = -1, refuse_rename;
/* Inject startup and partial replacement failures into the actual font loader. */
TTF_Font* TTF_OpenFont(const char* path, int size) {
    void* symbol = dlsym(RTLD_NEXT, "TTF_OpenFont");
    TTF_Font* (*open_font)(const char*, int);
    memcpy(&open_font, &symbol, sizeof(open_font));
    assert(open_font);
    if (fail_after >= 0 && calls++ >= fail_after) return NULL;
    if (fail_after < 0) ++calls;
    return open_font(path, size);
}
/* Refuse the actual atomic writer's final replacement, preserving the old file. */
int rename(const char* from, const char* to) {
    void* symbol = dlsym(RTLD_NEXT, "rename");
    int (*replace)(const char*, const char*);
    memcpy(&replace, &symbol, sizeof(replace));
    assert(replace);
    if (refuse_rename) { errno = EIO; return -1; }
    return replace(from, to);
}
/* Assert exact bank identity, including the three independent role slots. */
static void same(TTF_Font** fonts) {
    for (int i = 0; i < FONT_COUNT; ++i) assert(FontManager_Get((UIFontID)i) == fonts[i]);
}
/* Exercise the production authoring host, not a substitute persistence callback. */
int main(int argc, char** argv) {
    assert(argc == 2 && FontManager_Init());
    if (!strcmp(argv[1], "--reopen")) {
        assert(FontManager_LoadPersistedPrefs());
        assert(!strcmp(FontManager_GetSharedFontPresetName(), "daw_default"));
        assert(FontManager_GetZoomStep() == 3);
        char theme[64];assert(line_drawing3d_shared_theme_current_preset(theme,sizeof(theme)));
        assert(!strcmp(theme,"greyscale"));
        FontManager_Quit();puts("separate-process atomic style reopen PASS");return 0;
    }
    fail_after = 0; calls = 0; assert(!FontManager_LoadFonts()); fail_after = -1;
    assert(FontManager_LoadFonts());
    TTF_Font* fonts[FONT_COUNT];
    for (int i = 0; i < FONT_COUNT; ++i) fonts[i] = FontManager_Get((UIFontID)i);
    for (int partial = 0; partial < FONT_COUNT; ++partial) {
        calls = 0; fail_after = partial; assert(!FontManager_SetZoomStep(2));
        same(fonts);assert(FontManager_GetZoomStep() == 0);
        calls = 0;assert(!FontManager_SetSharedFontPresetName("daw_default"));same(fonts);
        assert(FontManager_GetSharedFontPresetId() == CORE_FONT_PRESET_IDE);
    }
    fail_after = -1;
    GlobalState* state = calloc(1, sizeof(*state));assert(state);
    state->screenWidth = 1280; state->screenHeight = 720;
    assert(LineDrawingPaneHost_Init(&state->paneHost,1280,720));
    LineDrawingWorkspaceAuthoringHost_Reset(&state->workspaceAuthoring);
    assert(FontManager_SavePersistedPrefs());
    FILE* file = fopen("data/runtime/ui_style.txt","rb");assert(file);
    char before[256]={0};assert(fread(before,1,sizeof(before)-1,file));fclose(file);
    assert(LineDrawingWorkspaceAuthoringHost_Enter(state).code == CORE_OK);
    assert(FontManager_ApplyStyle(CORE_FONT_PRESET_DAW_DEFAULT,3));
    assert(line_drawing3d_shared_theme_set_preset("greyscale"));
    refuse_rename = 1; assert(LineDrawingWorkspaceAuthoringHost_Apply(state).code != CORE_OK);
    assert(state->workspaceAuthoring.active && state->workspaceAuthoring.draft_baseline_valid);
    file=fopen("data/runtime/ui_style.txt","rb");assert(file);char after[256]={0};assert(fread(after,1,sizeof(after)-1,file));fclose(file);assert(!strcmp(before,after));refuse_rename=0;
    fail_after=0;calls=0;assert(LineDrawingWorkspaceAuthoringHost_Cancel(state).code==CORE_OK);
    assert(calls==0);same(fonts);assert(FontManager_GetZoomStep()==0);fail_after=-1;
    assert(LineDrawingWorkspaceAuthoringHost_Enter(state).code==CORE_OK);
    assert(FontManager_ApplyStyle(CORE_FONT_PRESET_DAW_DEFAULT,3));
    assert(line_drawing3d_shared_theme_set_preset("greyscale"));
    assert(LineDrawingWorkspaceAuthoringHost_Apply(state).code==CORE_OK);
    for (int preset=0;preset<CORE_FONT_PRESET_COUNT;++preset)
        for(int zoom=-2;zoom<=4;++zoom) assert(FontManager_ApplyStyle((CoreFontPresetId)preset,zoom));
    assert(LineDrawingWorkspaceAuthoringHost_Enter(state).code==CORE_OK);
    assert(FontManager_ApplyStyle(CORE_FONT_PRESET_IDE,0));
    FontManager_Quit();free(state);
    puts("startup/partial-slot failures; exact Cancel; failed Apply; all presets/zoom; pending preview shutdown PASS");return 0;
}
