#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

void line_drawing_text_register_font_source(TTF_Font* font,
                                            const char* path,
                                            int logical_point_size,
                                            int loaded_point_size,
                                            int kerning_enabled);

void line_drawing_text_unregister_font_source(TTF_Font* font);

void line_drawing_text_reset_renderer(SDL_Renderer* renderer);

int line_drawing_text_measure_utf8(SDL_Renderer* renderer,
                                   TTF_Font* font,
                                   const char* text,
                                   int* out_w,
                                   int* out_h);

int line_drawing_text_draw_utf8(SDL_Renderer* renderer,
                                TTF_Font* font,
                                const char* text,
                                SDL_Color color,
                                SDL_Rect* io_dst);

int line_drawing_text_draw_utf8_at(SDL_Renderer* renderer,
                                   TTF_Font* font,
                                   const char* text,
                                   int x,
                                   int y,
                                   SDL_Color color);
