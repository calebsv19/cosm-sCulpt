#include "UI/panel/ui_panel_stl_options.h"
#include "Core/global_state.h"
#include "Layout/layout_scene_collections.h"
#include <stdio.h>
#include "UI/panel/ui_panel_file_browser_internal.h"
#include "UI/font_manager.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

CoreUnitKind UIPanel_StlSourceUnit(const UIPanelState* ui) {
    CoreUnitKind unit = ui ? ui->loadMenu.stlSourceUnit : CORE_UNIT_UNKNOWN;
    return unit >= CORE_UNIT_METER && unit <= CORE_UNIT_FOOT ? unit : CORE_UNIT_METER;
}

SDL_Rect UIPanel_StlOptionRect(const UIPanelState* ui, int index) {
    SDL_Rect pane = UIPanel_GetLoadMenuRect(ui);
    if (!ui || ui->loadMenu.mode != UI_LOAD_MENU_MODE_STL_IMPORT || index < 0 || index > 3)
        return (SDL_Rect){0};
    int width = (pane.w - 24) / 2;
    return (SDL_Rect){pane.x + 8 + (index%2) * (width + 8), pane.y + (index<2?30:76), width, 22};
}

bool UIPanel_HandleStlOptionsClick(UIPanelState* ui, int x, int y) {
    if (!ui || ui->loadMenu.mode != UI_LOAD_MENU_MODE_STL_IMPORT) return false;
    if(ui->loadMenu.asyncStlActive&&ui->loadMenu.stlCollectionPicker)return true;
    if(ui->loadMenu.stlCollectionPicker) {
        SDL_Rect pane=UIPanel_GetLoadMenuRect(ui);LayoutObjectStore* s=&Global_Get()->layout.objectStore;
        for(unsigned i=0;i<8;++i){SDL_Rect row={pane.x+8,pane.y+100+(int)i*24,pane.w-16,23};
            if(!SDL_PointInRect(&(SDL_Point){x,y},&row))continue;
            unsigned at=ui->loadMenu.stlCollectionPage*8+i;
            if(at==0)ui->loadMenu.stlCollection[0]=0;
            else if(at<=s->scene_collection_count)snprintf(ui->loadMenu.stlCollection,64,"%s",s->scene_collections[at-1].id);
            ui->loadMenu.stlCollectionPicker=false;return true;
        }
        SDL_Rect next={pane.x+8,pane.y+292,pane.w-16,23};
        if(SDL_PointInRect(&(SDL_Point){x,y},&next)){ui->loadMenu.stlCollectionPage=(ui->loadMenu.stlCollectionPage+1)%((s->scene_collection_count+8)/8);return true;}
        ui->loadMenu.stlCollectionPicker=false;return true;
    }
    for (int i = 0; i < 4; ++i) {
        SDL_Rect rect = UIPanel_StlOptionRect(ui, i);
        if (!SDL_PointInRect(&(SDL_Point){x,y}, &rect)) continue;
        /* Worker and placement consume the same settings for the entire operation. */
        if (ui->loadMenu.asyncStlActive) return true;
        if (i == 0) {
            CoreUnitKind current = UIPanel_StlSourceUnit(ui);
            ui->loadMenu.stlSourceUnit = current == CORE_UNIT_FOOT ? CORE_UNIT_METER : current + 1;
        } else if(i==1) {
            ui->loadMenu.stlFitToScene = !ui->loadMenu.stlFitToScene;
        }else if(i==2){ui->loadMenu.stlCollectionPicker=true;ui->loadMenu.stlCollectionPage=0;}
        else ui->loadMenu.stlPartRole=(ui->loadMenu.stlPartRole+1)%4;
        return true;
    }
    return false;
}

void UIPanel_RenderStlOptions(const UIPanelState* ui, SDL_Renderer* renderer) {
    static const char* const labels[] = {"Source: m", "Source: cm", "Source: mm", "Source: in", "Source: ft"};
    if (!ui || !renderer || ui->loadMenu.mode != UI_LOAD_MENU_MODE_STL_IMPORT) return;
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;
    UIPanelVisualPalette palette = {0};
    if (!UIPanelVisual_ResolvePalette(&palette)) return;
    for (int i = 0; i < 2; ++i) {
        SDL_Rect rect = UIPanel_StlOptionRect(ui, i);
        const char* text = i == 0 ? labels[UIPanel_StlSourceUnit(ui) - CORE_UNIT_METER]
            : ui->loadMenu.stlFitToScene ? "Fit to scene" : "Exact size";
        UIPanelVisual_DrawFrame(renderer, rect, palette.workspace_fill, palette.button_border, 90);
        UIPanelSummary_DrawTextClipped(renderer, font, text, rect.x + 4, rect.y + 2,
                                       rect.w - 8, rect.h - 4, palette.text_primary);
    }
    const LayoutSceneCollection* c=Layout_FindSceneCollection(&Global_Get()->layout.objectStore,ui->loadMenu.stlCollection);
    static const char* roles[]={"Part: body", "Part: door", "Part: drawer", "Part: access panel"};
    for(int i=2;i<4;++i){SDL_Rect rect=UIPanel_StlOptionRect(ui,i);
        UIPanelVisual_DrawFrame(renderer,rect,palette.workspace_fill,palette.button_border,90);
        UIPanelSummary_DrawTextClipped(renderer,font,i==2?(c?c->name:"Group: Unassigned"):roles[ui->loadMenu.stlPartRole%4],rect.x+4,rect.y+2,rect.w-24,rect.h-4,palette.text_primary);
        if(i==2){int cx=rect.x+rect.w-12,cy=rect.y+11;SDL_SetRenderDrawColor(renderer,205,219,234,255);for(int n=0;n<5;++n)SDL_RenderDrawLine(renderer,cx-4+n,cy-2+n,cx+4-n,cy-2+n);}
    }
    SDL_Rect pane = UIPanel_GetLoadMenuRect(ui);
    UIPanelSummary_DrawTextClipped(renderer, font,
        ui->loadMenu.asyncStlActive ? "Importing; settings locked" : "STL has no units. Click source to change.",
        pane.x + 8, pane.y + 54, pane.w - 16, 20, palette.text_muted);
}

void UIPanel_RenderStlCollectionPicker(const UIPanelState* ui,SDL_Renderer* renderer){
    if(!ui||!renderer||ui->loadMenu.mode!=UI_LOAD_MENU_MODE_STL_IMPORT||!ui->loadMenu.stlCollectionPicker)return;
    SDL_Rect pane=UIPanel_GetLoadMenuRect(ui);LayoutObjectStore* s=&Global_Get()->layout.objectStore;
    UIPanelVisualPalette palette={0};if(!UIPanelVisual_ResolvePalette(&palette))return;
    TTF_Font* font=FontManager_Get(FONT_DEFAULT);if(!font)return;
    for(unsigned i=0;i<8;++i){unsigned at=ui->loadMenu.stlCollectionPage*8+i;SDL_Rect row={pane.x+8,pane.y+100+(int)i*24,pane.w-16,23};
        UIPanelVisual_DrawFrame(renderer,row,palette.workspace_fill,palette.button_border,255);
        const char* label=at==0?"Unassigned":at<=s->scene_collection_count?s->scene_collections[at-1].name:"";
        UIPanelSummary_DrawTextClipped(renderer,font,label,row.x+4,row.y+2,row.w-8,row.h-4,palette.text_primary);
    }
    SDL_Rect next={pane.x+8,pane.y+292,pane.w-16,23};UIPanelVisual_DrawFrame(renderer,next,palette.workspace_fill,palette.button_border,255);
    UIPanelSummary_DrawTextClipped(renderer,font,"Next page",next.x+4,next.y+2,next.w-8,next.h-4,palette.text_primary);
}
