#include "Menu/line_drawing_host_menu_internal.h"
#include "UI/shared_controls.h"
#include "kit_ui_window_sdl.h"
#include "Core/global_state.h"

static uint64_t path_key(const char* path) {
    uint64_t key=UINT64_C(14695981039346656037);
    for (const unsigned char* p=(const unsigned char*)path; *p; ++p) { key^=*p; key*=UINT64_C(1099511628211); }
    return key;
}
bool LineDrawingHostMenu_CollectControls(LineDrawingHostMenuState* state) {
    KitUiSurface* s=LDControls_Menu();
    LineDrawingHostMenuLayout l;
    LineDrawingHostMenuModel model;
    LineDrawingHostMenu_BuildModel(&model);
    line_drawing_host_menu_layout(&l,Global_GetScreenWidth(),Global_GetScreenHeight());
    kit_ui_surface_begin(s,1u+(unsigned)state->selected_section);
    for (int i=0; i<LINE_DRAWING_HOST_MENU_SECTION_COUNT; ++i)
        LDControls_Add(s,10,(uint64_t)i,l.nav_rows[i],NULL,true);
    if (state->selected_section==LINE_DRAWING_HOST_MENU_SECTION_BROWSE)
        for (int i=0; i<LINE_DRAWING_HOST_MENU_BROWSE_ACTION_COUNT; ++i)
            LDControls_Add(s,11,(uint64_t)i,l.browse_action_rects[i],NULL,true);
    int count=line_drawing_host_menu_content_count(state,state->selected_section);
    float* scroll=line_drawing_host_menu_scroll_ptr(state,state->selected_section);
    int height=line_drawing_host_menu_content_row_height(state->selected_section);
    for (int i=0; i<count; ++i) {
        SDL_Rect rect={l.list_view_rect.x,l.list_view_rect.y+i*(height+LINE_DRAWING_HOST_MENU_ROW_GAP)-(int)(scroll?*scroll:0),l.list_view_rect.w,height};
        if(rect.y+rect.h<=l.list_view_rect.y || rect.y>=l.list_view_rect.y+l.list_view_rect.h) continue;
        uint64_t key=(uint64_t)i; bool enabled=true;
        switch (state->selected_section) {
            case LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS: enabled=model.item_enabled[i]; break;
            case LINE_DRAWING_HOST_MENU_SECTION_RECENTS:
                key=path_key(state->recent_entries.entries[i].path)^(uint64_t)state->recent_entries.entries[i].kind; break;
            case LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS:
                key=path_key(state->catalog.layouts[state->filtered_layout_indices[i]].path); break;
            case LINE_DRAWING_HOST_MENU_SECTION_SCENES:
                key=path_key(state->catalog.scenes[state->filtered_scene_indices[i]].path); break;
            case LINE_DRAWING_HOST_MENU_SECTION_BROWSE:
                key=path_key(state->browser.entries[i].path); enabled=state->browser.entries[i].enabled; break;
            default: break;
        }
        LDControls_Add(s,12,key,rect,&l.list_view_rect,enabled);
    }
    if(kit_ui_surface_end(s).code!=CORE_OK) { LDControls_Cancel(s); return false; }
    return true;
}

#include <string.h>

/* Shared controls admit a completed activation; the existing menu owns I/O. */
bool LineDrawingHostMenu_HandleEvent(LineDrawingHostMenuState* state, AppContext* ctx,
                                     const SDL_Event* event, LineDrawingHostMenuCommand* command) {
    if (!state || !event || !command) return false;
    memset(command,0,sizeof(*command));
    if(kit_ui_window_event_invalidates_sdl(event)) {
        state->scrollbar_dragging=false;
        LDText_CancelCapture(&state->filter_text);
        LDControls_CancelAll();
    }
    if(!LineDrawingHostMenu_CollectControls(state)) return true;
    KitUiSurface* surface=LDControls_Menu();
    SDL_Point activate;
    bool action_key=(event->type==SDL_KEYDOWN || event->type==SDL_KEYUP) &&
        (event->key.keysym.sym==SDLK_RETURN || event->key.keysym.sym==SDLK_KP_ENTER || event->key.keysym.sym==SDLK_SPACE);
    if(action_key && (surface->interaction.armed_key!=KIT_UI_INTERACTION_KEY_NONE ||
        (state->focus_region==LINE_DRAWING_HOST_MENU_FOCUS_CONTENT && !event->key.keysym.mod))) {
        if(event->type==SDL_KEYDOWN && !event->key.repeat) {
            int* selected=line_drawing_host_menu_selected_content_index_ptr(state,state->selected_section);
            LineDrawingHostMenuLayout l;
            line_drawing_host_menu_layout(&l,Global_GetScreenWidth(),Global_GetScreenHeight());
            float* scroll=line_drawing_host_menu_scroll_ptr(state,state->selected_section);
            int h=line_drawing_host_menu_content_row_height(state->selected_section);
            int y=l.list_view_rect.y+(selected?*selected:0)*(h+LINE_DRAWING_HOST_MENU_ROW_GAP)-(int)(scroll?*scroll:0)+h/2;
            surface->interaction.focused_id=0;
            for(uint32_t i=0;i<surface->count;++i) {
                KitRenderRect b=surface->controls[i].bounds;
                if(surface->keys[i].domain==12 && surface->controls[i].enabled && y>=b.y && y<b.y+b.height)
                    surface->interaction.focused_id=surface->controls[i].id;
            }
        }
        (void)LDControls_Route(surface,event,&activate);
        if(activate.x>=0) {
            SDL_Event click={.type=SDL_MOUSEBUTTONDOWN};
            click.button.button=SDL_BUTTON_LEFT;click.button.x=activate.x;click.button.y=activate.y;
            return line_drawing_host_menu_dispatch(state,ctx,&click,command);
        }
        return true;
    }
    /* Keep the menu's established region/arrow/filter keyboard navigation. */
    bool pointer=event->type==SDL_MOUSEBUTTONDOWN || event->type==SDL_MOUSEBUTTONUP || event->type==SDL_MOUSEMOTION;
    if(pointer && LDControls_Route(surface,event,&activate)) {
        if(activate.x>=0) {
            SDL_Event click=*event; click.type=SDL_MOUSEBUTTONDOWN;
            click.button.button=SDL_BUTTON_LEFT; click.button.x=activate.x; click.button.y=activate.y;
            return line_drawing_host_menu_dispatch(state,ctx,&click,command);
        }
        return true;
    }
    if(event->type==SDL_MOUSEBUTTONDOWN && event->button.button==SDL_BUTTON_LEFT) {
        LineDrawingHostMenuLayout layout;
        line_drawing_host_menu_layout(&layout,Global_GetScreenWidth(),Global_GetScreenHeight());
        if(line_drawing_host_menu_hit_test_content(state,&layout,event->button.x,event->button.y)>=0) return true;
    }
    if(event->type==SDL_KEYDOWN || event->type==SDL_MOUSEWHEEL || event->type==SDL_TEXTINPUT)
        LDControls_Cancel(surface);
    return line_drawing_host_menu_dispatch(state,ctx,event,command);
}
