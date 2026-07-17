#pragma once

#include "Core/space_mode_adapter.h"
#include "Layout/hitbox_system.h"

bool Editor_PickNearestObject3DOrigin(const Layout* layout,
                                      const Grid* grid,
                                      const SpaceViewContext* viewCtx,
                                      int mouseX,
                                      int mouseY,
                                      uint32_t* outObjectId,
                                      float* outDistSq);

bool Editor_RebuildObject3DOriginPickIndex(const Layout* layout,
                                           const Grid* grid,
                                           const SpaceViewContext* viewCtx);
void Editor_ShutdownObject3DOriginPickIndex(void);

Hitbox Editor_ResolveObject3DBodyPick(const Layout* layout,
                                      const Grid* grid,
                                      const SpaceViewContext* viewCtx,
                                      int mouseX,
                                      int mouseY,
                                      Hitbox baseHit);
