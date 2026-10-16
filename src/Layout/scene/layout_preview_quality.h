#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* CPU target is bounded, independently of physical drawable size and triangle budget. */
float Layout_PreviewRasterScale(int width,int height,float dpi,bool moving);
size_t Layout_PreviewEdges(uint8_t* rgba,const float* depth,const int32_t* owner,int width,int height,bool wire,bool edges,int selected,int hovered);
bool Layout_PreviewAntialias(uint8_t* rgba,const int32_t* owner,int width,int height);
