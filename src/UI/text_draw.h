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

/* Supply backend density to legacy measure-only layout callers. */
void line_drawing_text_bind_renderer(SDL_Renderer* renderer);

/* Density-aware compatibility for layout code that has no renderer parameter. */
int line_drawing_text_size_utf8(TTF_Font* font, const char* text, int* width, int* height);
int line_drawing_text_font_height(TTF_Font* font);

/* Paint whole native texels inside a width limit and the caller's existing clip. */
int line_drawing_text_draw_utf8_clipped(SDL_Renderer*, TTF_Font*, const char*, SDL_Color, SDL_Rect*, int);
