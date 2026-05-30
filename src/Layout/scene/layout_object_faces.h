#pragma once

#include "Core/space_mode_adapter.h"
#include "Layout/Grid/grid.h"
#include "Layout/layout.h"

const char* Layout_Object3DFaceKind_Label(Object3DFaceKind face);
bool Layout_Object3DFaceKind_IsRectPrismFace(Object3DFaceKind face);
bool Layout_Object3DFaceKind_IsValidForObject(const Object3D* object,
                                              Object3DFaceKind face);
bool Layout_Object3DFace_GetFrame(const Object3D* object,
                                  Object3DFaceKind face,
                                  PlaneFrame3* out_frame);
bool Layout_Object3D_PickVisibleFaceAtScreenPoint(const Object3D* object,
                                                  const SpaceViewContext* view_ctx,
                                                  const Grid* grid,
                                                  int mouse_x,
                                                  int mouse_y,
                                                  Object3DFaceKind* out_face);
bool Layout_Object3D_DefaultAuthoringFaceForView(const Object3D* object,
                                                 const SpaceViewContext* view_ctx,
                                                 Object3DFaceKind* out_face);
