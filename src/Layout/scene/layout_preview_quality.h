#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Preview policy is display-only; source meshes and saved transforms stay intact. */
typedef enum { LAYOUT_PREVIEW_MOVING, LAYOUT_PREVIEW_BALANCED, LAYOUT_PREVIEW_DETAIL } LayoutPreviewQuality;
LayoutPreviewQuality Layout_PreviewQuality(uint64_t now,uint64_t changed);
size_t Layout_PreviewTriangleBudget(LayoutPreviewQuality quality);
int Layout_PreviewRefinementDelayMs(uint64_t now,uint64_t changed);
/* CPU target is bounded, independently of physical drawable size and triangle budget. */
float Layout_PreviewRasterScale(int width,int height,float dpi,bool moving);
size_t Layout_PreviewEdges(uint8_t* rgba,const float* depth,const int32_t* owner,int width,int height,bool wire,bool edges,int selected,int hovered);
bool Layout_PreviewAntialias(uint8_t* rgba,const int32_t* owner,int width,int height);
