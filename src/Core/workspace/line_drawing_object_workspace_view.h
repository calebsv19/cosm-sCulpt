#pragma once

#include "Core/global_state.h"

bool LineDrawingObjectWorkspaceView_SelectBody(GlobalState* state,
                                               ObjectAuthoringBodyId body_id);
bool LineDrawingObjectWorkspaceView_EnterFreeView(GlobalState* state,
                                                  uint32_t object_id);
bool LineDrawingObjectWorkspaceView_FocusFace(GlobalState* state,
                                              uint32_t object_id,
                                              Object3DFaceKind face);
