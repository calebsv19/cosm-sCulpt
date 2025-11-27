// Tools/shape_tool.c
//
// Small CLI tool to:
//   1) Load a Layout JSON file produced by the line-drawing program.
//   2) Convert it into a ShapeDocument (paths + segments).
//   3) Print a human-readable summary.
//
// Usage:
//   ./shape_tool path/to/layout.json
// If no argument is given, it defaults to: config/layout_config.json

#include "Layout/layout.h"
#include "Layout/layout_json.h"
#include "Tools/shape_asset.h"
#include "Tools/shape_draw_sdl.h"

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void PrintShapeSummary(const ShapeDocument* doc) {
    if (!doc) {
        printf("No document.\n");
        return;
    }

    printf("ShapeDocument: %zu shape(s)\n", doc->shapeCount);
    for (size_t si = 0; si < doc->shapeCount; ++si) {
        const Shape* s = &doc->shapes[si];
        printf("  Shape %zu: name=\"%s\", paths=%zu\n",
               si,
               s->name ? s->name : "(unnamed)",
               s->pathCount);

        for (size_t pi = 0; pi < s->pathCount; ++pi) {
            const ShapePath* p = &s->paths[pi];
            size_t lines = 0;
            size_t curves = 0;
            for (size_t segi = 0; segi < p->segmentCount; ++segi) {
                const ShapeSegment* seg = &p->segments[segi];
                if (seg->type == SHAPE_SEGMENT_LINE) {
                    ++lines;
                } else if (seg->type == SHAPE_SEGMENT_CUBIC_BEZIER) {
                    ++curves;
                }
            }

            printf("    Path %zu: closed=%s, segments=%zu (lines=%zu, curves=%zu)\n",
                   pi,
                   p->closed ? "true" : "false",
                   p->segmentCount,
                   lines,
                   curves);
        }
    }
}

int main(int argc, char** argv) {
    bool viewMode = false;
    const char* jsonPath = "config/layout_config.json";

    if (argc >= 2) {
        if (strcmp(argv[1], "--view") == 0) {
            viewMode = true;
            if (argc >= 3) {
                jsonPath = argv[2];
            }
        } else {
            jsonPath = argv[1];
        }
    }

    printf("[shape_tool] Using JSON file: %s\n", jsonPath);

    Layout layout;
    Layout_Init(&layout, 1.0f);

    if (!Layout_LoadFromFile(&layout, jsonPath)) {
        fprintf(stderr, "[shape_tool] ERROR: failed to load layout from '%s'\n", jsonPath);
        Layout_Free(&layout);
        return 1;
    }

    printf("[shape_tool] Loaded layout: anchors=%zu, walls=%zu\n",
           layout.anchorCount, layout.wallCount);

    ShapeDocument doc;
    if (!ShapeDocument_FromLayout(jsonPath, &layout, &doc)) {
        fprintf(stderr, "[shape_tool] ERROR: failed to convert Layout to ShapeDocument\n");
        Layout_Free(&layout);
        return 1;
    }

    PrintShapeSummary(&doc);

    bool sdlInitialized = false;
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;

    if (viewMode && doc.shapeCount > 0) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "[shape_tool] ERROR: SDL_Init failed: %s\n", SDL_GetError());
        } else {
            sdlInitialized = true;
            const int screenW = 800;
            const int screenH = 600;

            window = SDL_CreateWindow("shape_tool preview",
                                      SDL_WINDOWPOS_CENTERED,
                                      SDL_WINDOWPOS_CENTERED,
                                      screenW, screenH,
                                      SDL_WINDOW_SHOWN);
            if (!window) {
                fprintf(stderr, "[shape_tool] ERROR: SDL_CreateWindow failed: %s\n", SDL_GetError());
            } else {
                renderer = SDL_CreateRenderer(window, -1,
                                              SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
                if (!renderer) {
                    fprintf(stderr, "[shape_tool] ERROR: SDL_CreateRenderer failed: %s\n", SDL_GetError());
                }
            }

            if (renderer) {
                SDL_SetRenderDrawColor(renderer, 15, 15, 20, 255);
                SDL_RenderClear(renderer);

                if (!Shape_DrawToSDL(renderer, &doc.shapes[0], screenW, screenH, 0.5f)) {
                    fprintf(stderr, "[shape_tool] ERROR: failed to draw shape\n");
                }
                SDL_RenderPresent(renderer);

                bool running = true;
                while (running) {
                    SDL_Event ev;
                    while (SDL_PollEvent(&ev)) {
                        if (ev.type == SDL_QUIT) {
                            running = false;
                            break;
                        }
                        if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) {
                            running = false;
                            break;
                        }
                    }
                    SDL_Delay(16);
                }
            }
        }
    }

    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }

    ShapeDocument_Free(&doc);
    Layout_Free(&layout);

    if (sdlInitialized) {
        SDL_Quit();
    }
    return 0;
}
