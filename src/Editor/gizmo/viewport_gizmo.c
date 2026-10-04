#include "Editor/viewport_gizmo.h"
#include "Core/global_state.h"
#include "Core/line_drawing_pane_host.h"
#include <math.h>

static float viewport_span(void) {
    const GlobalState* state = Global_Get();
    CorePaneRect rect = {0};
    if (state && LineDrawingPaneHost_GetViewportRect(&state->paneHost, &rect) &&
        rect.width > 0 && rect.height > 0)
        return fminf(rect.width, rect.height);
    return 600;
}

float ViewportGizmo_LengthPixels(void) {
    return fminf(100, fmaxf(48, viewport_span() * .065f));
}

float ViewportGizmo_WorldLength(float grid_size, float scale) {
    float pixels_per_unit = grid_size * scale;
    return isfinite(pixels_per_unit) && pixels_per_unit > 0
        ? ViewportGizmo_LengthPixels() / pixels_per_unit : 0;
}

int ViewportGizmo_RadiusPixels(void) {
    return (int)fminf(8, fmaxf(5, viewport_span() * .006f));
}

int ViewportGizmo_PickRadiusPixels(void) {
    return ViewportGizmo_RadiusPixels() + 3;
}
