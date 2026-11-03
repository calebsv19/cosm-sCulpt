#pragma once
#include "UI/text_draw.h"
/* App-local source identity and backend-specific native raster adapters. */
void line_drawing_sdl_font_register(TTF_Font*,const char*,int,int);
void line_drawing_sdl_font_unregister(TTF_Font*);
int line_drawing_native_font_source(TTF_Font*,const char**,int*);
int line_drawing_sdl_text_measure(SDL_Renderer*,TTF_Font*,const char*,int*,int*);
int line_drawing_sdl_text_draw(SDL_Renderer*,TTF_Font*,const char*,SDL_Color,SDL_Rect*,int);
int line_drawing_fallback_measure(SDL_Renderer*,TTF_Font*,const char*,int*,int*);
int line_drawing_fallback_draw(SDL_Renderer*,TTF_Font*,const char*,SDL_Color,SDL_Rect*);
void line_drawing_fallback_reset(SDL_Renderer*,TTF_Font*);
