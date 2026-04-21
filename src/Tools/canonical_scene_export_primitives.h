#ifndef CANONICAL_SCENE_EXPORT_PRIMITIVES_H
#define CANONICAL_SCENE_EXPORT_PRIMITIVES_H

#include <stdbool.h>

#include "cjson/cJSON.h"
#include "Layout/layout.h"
#include "core_object.h"

bool LineDrawingCanonicalScene_AddCanonicalPrimitivePayload(cJSON* object_json, const Object3D* object);
bool LineDrawingCanonicalScene_PopulateScene3DExtension(cJSON* line_drawing_ext, const Layout* layout);
cJSON* LineDrawingCanonicalScene_AppendSceneObjectFromCore(cJSON* objects,
                                                           const CoreObject* object,
                                                           const char* object_id,
                                                           const char* geometry_id,
                                                           const char* material_id,
                                                           const char* tag2,
                                                           const char* tag3);
bool LineDrawingCanonicalScene_AddPrimitiveExtensionPayload(cJSON* object_extensions,
                                                            const Object3D* object);

#endif
