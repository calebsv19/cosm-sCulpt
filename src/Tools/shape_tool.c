#include "Layout/layout.h"
#include "Layout/layout_json.h"
#include "ShapeLib/shape_core.h"
#include "ShapeLib/shape_draw_sdl.h"
#include "ShapeLib/shape_json.h"
#include "Tools/shape_from_layout.h"

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Tools/shape_export.h"

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
            size_t lines = 0, curves = 0;
            for (size_t segi = 0; segi < p->segmentCount; ++segi) {
                const ShapeSegment* seg = &p->segments[segi];
                if (seg->type == SHAPE_SEGMENT_LINE) ++lines;
                else if (seg->type == SHAPE_SEGMENT_CUBIC_BEZIER) ++curves;
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
    const char* layoutPath = "config/layout_config.json";
    const char* exportPath = NULL;
    ViewPlaneAxis exportAxis = VIEW_PLANE_XY;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (strcmp(arg, "--view") == 0) {
            viewMode = true;
        } else if (strcmp(arg, "--plane") == 0) {
            if (i + 1 < argc) {
                const char* label = argv[++i];
                if (strcmp(label, "xy") == 0) {
                    exportAxis = VIEW_PLANE_XY;
                } else if (strcmp(label, "yz") == 0) {
                    exportAxis = VIEW_PLANE_YZ;
                } else if (strcmp(label, "xz") == 0) {
                    exportAxis = VIEW_PLANE_XZ;
                } else {
                    fprintf(stderr, "[shape_tool] ERROR: --plane must be one of: xy, yz, xz\n");
                    return 1;
                }
            } else {
                fprintf(stderr, "[shape_tool] ERROR: --plane requires a value (xy|yz|xz)\n");
                return 1;
            }
        } else if (strcmp(arg, "--export-shape") == 0) {
            if (i + 1 < argc) {
                exportPath = argv[++i];
            } else {
                fprintf(stderr, "[shape_tool] ERROR: --export-shape requires a path\n");
                return 1;
            }
        } else {
            layoutPath = arg;
        }
    }

    printf("[shape_tool] Using Layout JSON file: %s\n", layoutPath);
    printf("[shape_tool] Projection plane: %s\n",
           exportAxis == VIEW_PLANE_YZ ? "YZ" :
           exportAxis == VIEW_PLANE_XZ ? "XZ" : "XY");

    Layout layout;
    Layout_Init(&layout, 1.0f);

    if (!Layout_LoadFromFile(&layout, layoutPath)) {
        fprintf(stderr, "[shape_tool] ERROR: failed to load layout from '%s'\n", layoutPath);
        Layout_Free(&layout);
        return 1;
    }

    printf("[shape_tool] Loaded layout: anchors=%zu, walls=%zu\n",
           layout.anchorCount, layout.wallCount);

    ShapeDocument doc;
    if (!ShapeDocument_FromLayoutProjected(layoutPath, &layout, exportAxis, &doc)) {
        fprintf(stderr, "[shape_tool] ERROR: failed to convert Layout to ShapeDocument\n");
        Layout_Free(&layout);
        return 1;
    }

    PrintShapeSummary(&doc);

    if (exportPath) {
        char finalExportPath[SHAPE_EXPORT_PATH_MAX];
        if (!ShapeExport_BuildPath(exportPath, finalExportPath, sizeof(finalExportPath))) {
            fprintf(stderr, "[shape_tool] ERROR: failed to prepare export path for '%s'\n", exportPath);
        } else if (ShapeDocument_SaveToJsonFile(&doc, finalExportPath)) {
            printf("[shape_tool] Exported Shape JSON to '%s'\n", finalExportPath);
        } else {
            fprintf(stderr, "[shape_tool] ERROR: failed to export Shape JSON to '%s'\n", finalExportPath);
        }
    }

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

    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);

    ShapeDocument_Free(&doc);
    Layout_Free(&layout);

    if (sdlInitialized) SDL_Quit();
    return 0;
}
