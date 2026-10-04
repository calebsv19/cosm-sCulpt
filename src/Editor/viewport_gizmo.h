#pragma once

/* App-owned screen-space sizing shared by gizmo drawing and picking. Axis length
 * tracks the center viewport, with bounded pixel size independent of world zoom. */
float ViewportGizmo_LengthPixels(void);
float ViewportGizmo_WorldLength(float grid_size, float scale);
int ViewportGizmo_RadiusPixels(void);
int ViewportGizmo_PickRadiusPixels(void);
