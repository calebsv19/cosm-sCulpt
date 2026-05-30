#pragma once

#include "Menu/line_drawing_host_menu_internal.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

const char* line_drawing_host_menu_item_label(LineDrawingHostMenuItemId item_id);
const char* line_drawing_host_menu_item_description(LineDrawingHostMenuItemId item_id);
const char* line_drawing_host_menu_section_label(LineDrawingHostMenuSection section);
int line_drawing_host_menu_font_height(TTF_Font* font);
SDL_Color line_drawing_host_menu_dim(SDL_Color color, Uint8 alpha);
SDL_Color line_drawing_host_menu_mix(SDL_Color a, SDL_Color b, float t);
void line_drawing_host_menu_draw_text(SDL_Renderer* renderer,
                                      TTF_Font* font,
                                      const char* text,
                                      int x,
                                      int y,
                                      SDL_Color color);
void line_drawing_host_menu_draw_text_clipped(SDL_Renderer* renderer,
                                              TTF_Font* font,
                                              const char* text,
                                              int x,
                                              int y,
                                              int max_width,
                                              SDL_Color color);
void line_drawing_host_menu_draw_panel(SDL_Renderer* renderer,
                                       SDL_Rect rect,
                                       SDL_Color fill_color,
                                       SDL_Color border_color,
                                       SDL_Color shadow_color);
void line_drawing_host_menu_draw_accent_bar(SDL_Renderer* renderer,
                                            SDL_Rect rect,
                                            SDL_Color accent_color);
void line_drawing_host_menu_draw_badge(SDL_Renderer* renderer,
                                       TTF_Font* font,
                                       const char* label,
                                       SDL_Rect rect,
                                       SDL_Color fill_color,
                                       SDL_Color border_color,
                                       SDL_Color text_color);
void line_drawing_host_menu_format_preview_summary(const LineDrawingCatalogPreviewData* preview,
                                                   char* out,
                                                   size_t out_size);
void line_drawing_host_menu_format_preview_extents(const LineDrawingCatalogPreviewData* preview,
                                                   char* out,
                                                   size_t out_size);
void line_drawing_host_menu_draw_preview(SDL_Renderer* renderer,
                                         SDL_Rect rect,
                                         const LineDrawingCatalogPreviewData* preview,
                                         SDL_Color background_color,
                                         SDL_Color border_color,
                                         SDL_Color line_color,
                                         SDL_Color muted_color);
void line_drawing_host_menu_draw_section_nav(SDL_Renderer* renderer,
                                             TTF_Font* font,
                                             const LineDrawingHostMenuState* state,
                                             const LineDrawingHostMenuLayout* layout,
                                             SDL_Color panel_fill,
                                             SDL_Color border_color,
                                             SDL_Color title_color,
                                             SDL_Color text_color,
                                             SDL_Color muted_color,
                                             SDL_Color highlight_color,
                                             SDL_Color shadow_color);
void line_drawing_host_menu_draw_quick_action_row(SDL_Renderer* renderer,
                                                  TTF_Font* font,
                                                  const LineDrawingHostMenuModel* model,
                                                  SDL_Rect rect,
                                                  int item_index,
                                                  bool selected,
                                                  SDL_Color fill_color,
                                                  SDL_Color border_color,
                                                  SDL_Color text_color,
                                                  SDL_Color muted_color);
void line_drawing_host_menu_draw_catalog_row(SDL_Renderer* renderer,
                                             TTF_Font* font,
                                             SDL_Rect rect,
                                             const LineDrawingSceneCatalogEntry* entry,
                                             const LineDrawingCatalogPreviewData* preview,
                                             bool selected,
                                             bool active,
                                             SDL_Color fill_color,
                                             SDL_Color border_color,
                                             SDL_Color text_color,
                                             SDL_Color muted_color,
                                             SDL_Color active_color);
void line_drawing_host_menu_draw_recent_row(SDL_Renderer* renderer,
                                            TTF_Font* font,
                                            SDL_Rect rect,
                                            const LineDrawingRecentMenuEntry* entry,
                                            const LineDrawingCatalogPreviewData* preview,
                                            SDL_Color fill_color,
                                            SDL_Color border_color,
                                            SDL_Color text_color,
                                            SDL_Color muted_color,
                                            SDL_Color active_color);
void line_drawing_host_menu_draw_browser_row(SDL_Renderer* renderer,
                                             TTF_Font* font,
                                             SDL_Rect rect,
                                             const LineDrawingRootBrowserEntry* entry,
                                             const LineDrawingCatalogPreviewData* preview,
                                             SDL_Color fill_color,
                                             SDL_Color border_color,
                                             SDL_Color text_color,
                                             SDL_Color muted_color,
                                             SDL_Color active_color);
void line_drawing_host_menu_draw_browse_actions(SDL_Renderer* renderer,
                                                TTF_Font* font,
                                                const LineDrawingHostMenuState* state,
                                                const LineDrawingHostMenuLayout* layout,
                                                SDL_Color panel_fill,
                                                SDL_Color border_color,
                                                SDL_Color title_color,
                                                SDL_Color text_color,
                                                SDL_Color muted_color,
                                                SDL_Color highlight_color);
void line_drawing_host_menu_draw_filter(SDL_Renderer* renderer,
                                        TTF_Font* font,
                                        const LineDrawingHostMenuState* state,
                                        const LineDrawingHostMenuLayout* layout,
                                        SDL_Color panel_fill,
                                        SDL_Color border_color,
                                        SDL_Color title_color,
                                        SDL_Color text_color,
                                        SDL_Color muted_color,
                                        SDL_Color highlight_color);
