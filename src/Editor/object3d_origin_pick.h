#pragma once

#include "Core/space_mode_adapter.h"
#include "Layout/hitbox_system.h"

bool Editor_PickNearestObject3DOrigin(const Layout* layout,
                                      const Grid* grid,
                                      const SpaceViewContext* viewCtx,
                                      int mouseX,
                                      int mouseY,
                                      float captureRadiusPx,
                                      uint32_t* outObjectId,
                                      float* outDistSq);

Hitbox Editor_ResolveObject3DBodyPick(const Layout* layout,
                                      const Grid* grid,
                                      const SpaceViewContext* viewCtx,
                                      int mouseX,
                                      int mouseY,
                                      Hitbox baseHit,
                                      float captureRadiusPx);
