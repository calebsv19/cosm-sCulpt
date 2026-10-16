#pragma once
#include "Layout/layout_reference.h"
#include "Layout/layout_constraints.h"

typedef struct {
    PlaneFrame3 frame; /* rigid world frame; origin is in world coordinates */
    double min_meters[3], max_meters[3], world_min_meters[3], world_max_meters[3];
    size_t parts, mesh_parts, excluded, unsupported;
    LayoutMeasurementStatus status;
    char message[128];
} LayoutCollectionInspection;

const Object3D* Layout_InspectionObject(const Layout* layout,const char* entity);
LayoutMeasurementStatus Layout_InspectionFrame(const Layout* layout,const char* entity,PlaneFrame3* out);
bool Layout_InspectionMember(const Layout* layout,const char* collection,const Object3D* object);
LayoutCollectionInspection Layout_InspectCollection(const Layout* layout,const char* collection,const char* frame);
LayoutMeasurementStatus Layout_InspectionDatumPoint(const Layout* layout,const LayoutInspectionDatum* datum,double out_meters[3]);
/* Local primitive size or scaled STL envelope span; not a rotated world AABB. */
bool Layout_InspectionLocalSize(const Layout* layout,const Object3D* object,double out_meters[3]);
LayoutMeasurementResult Layout_EvaluateSavedMeasurement(const Layout* layout,const LayoutSavedMeasurement* measurement);
bool Layout_ValidateMeasurements(const Layout* layout);
bool Layout_SaveMeasurement(Layout* layout,const LayoutSavedMeasurement* measurement,const char* remove_id,
    LayoutGeometryBeforePublish history,void* context);
bool Layout_MeasurementsWriteJson(const Layout* layout,cJSON* engineering);
bool Layout_MeasurementsReadJson(Layout* layout,const cJSON* engineering);
bool Layout_HasInspectionRecords(const Layout* layout);

/* Native primitive corners or conservative transformed STL envelope. */
bool Layout_InspectionCorners(const Object3D* object,Vec3 out[8],size_t* count);
