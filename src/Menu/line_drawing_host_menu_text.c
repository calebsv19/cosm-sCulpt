#include "Menu/line_drawing_host_menu_internal.h"
#include "UI/shared_text.h"
#include <string.h>

static void bind(LineDrawingHostMenuState* state) {
    size_t cursor=state->filter_text.edit.text?state->filter_text.edit.cursor:strlen(state->filter_query);
    LDText_Bind(&state->filter_text,state->filter_query,sizeof(state->filter_query),cursor);
}
KitUiTextEventResult LineDrawingHostMenu_FilterEvent(LineDrawingHostMenuState* state,const SDL_Event* event) {
    if(state->focus_region!=LINE_DRAWING_HOST_MENU_FOCUS_FILTER ||
        !line_drawing_host_menu_section_supports_filter(state->selected_section))
        return (KitUiTextEventResult){.status={CORE_OK,NULL}};
    bind(state);return LDText_Event(&state->filter_text,event);
}
void LineDrawingHostMenu_FilterDraw(SDL_Renderer* renderer,TTF_Font* font,LineDrawingHostMenuState* state,
                                    SDL_Rect bounds,SDL_Color ink,SDL_Color muted) {
    bool active=state->focus_region==LINE_DRAWING_HOST_MENU_FOCUS_FILTER;
    bind(state);
    SDL_Rect text={bounds.x+8,bounds.y+6,bounds.w-16,bounds.h-12};
    LDText_Draw(&state->filter_text,renderer,font,text,ink,active);
    if(!active && !state->filter_query[0])
        (void)LDText_Label(renderer,font,"Type to filter names and paths",text,muted,false);
}
