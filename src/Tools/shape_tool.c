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
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(_WIN32)
#include <direct.h>
#endif

#define EXPORT_DIR "export"
#define EXPORT_PATH_MAX 512

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

static bool EnsureDirectoryExists(const char* path) {
    if (!path || !path[0]) return false;
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
#if defined(_WIN32)
    if (_mkdir(path) == 0) {
        return true;
    }
#else
    if (mkdir(path, 0755) == 0) {
        return true;
    }
#endif
    if (errno == EEXIST) {
        return true;
    }
    return false;
}

static const char* ExtractFileName(const char* path) {
    if (!path) {
        return NULL;
    }
    const char* slash = strrchr(path, '/');
    const char* backslash = strrchr(path, '\\');
    const char* start = path;
    if (slash && backslash) {
        start = (slash > backslash) ? slash + 1 : backslash + 1;
    } else if (slash) {
        start = slash + 1;
    } else if (backslash) {
        start = backslash + 1;
    }
    return (*start) ? start : NULL;
}

static bool BuildExportPath(const char* requested,
                            char* outPath,
                            size_t outSize) {
    if (!requested || !outPath || outSize == 0) {
        return false;
    }

    const char* name = ExtractFileName(requested);
    if (!name) {
        return false;
    }

    if (!EnsureDirectoryExists(EXPORT_DIR)) {
        return false;
    }

    int written = snprintf(outPath, outSize, "%s/%s", EXPORT_DIR, name);
    if (written <= 0 || (size_t)written >= outSize) {
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    bool viewMode = false;
    const char* layoutPath = "config/layout_config.json";
    const char* exportPath = NULL;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (strcmp(arg, "--view") == 0) {
            viewMode = true;
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
    if (!ShapeDocument_FromLayout(layoutPath, &layout, &doc)) {
        fprintf(stderr, "[shape_tool] ERROR: failed to convert Layout to ShapeDocument\n");
        Layout_Free(&layout);
        return 1;
    }

    PrintShapeSummary(&doc);

    if (exportPath) {
        char finalExportPath[EXPORT_PATH_MAX];
        if (!BuildExportPath(exportPath, finalExportPath, sizeof(finalExportPath))) {
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
