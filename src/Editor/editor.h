// src/Editor/editor.h
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "Core/SDLApp/sdl_app_framework.h"
#include "Layout/layout.h"
#include "Math/math_util.h"
#include "Editor/space_gizmo_drag.h"
#include "Layout/scene/layout_scene_path_edit.h"

typedef enum {
    DELETE_MODE_SAFE,      // Only delete selected wall or anchor
    DELETE_MODE_AUTO_PRUNE // Also delete orphan anchors when wall is removed
} DeleteMode;

typedef enum {
    TOOL_IDLE,
    TOOL_PLACING_WALL
} ToolMode;

typedef enum {
    PRIMITIVE_PLACEMENT_PREVIEW_NONE = 0,
    PRIMITIVE_PLACEMENT_PREVIEW_PLANE = 1,
    PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM = 2
} PrimitivePlacementPreviewKind;

typedef enum {
    OBJECT_FACE_EXTRUDE_MODE_NONE = 0,
    OBJECT_FACE_EXTRUDE_MODE_ADD = 1,
    OBJECT_FACE_EXTRUDE_MODE_CUT = 2
} ObjectFaceExtrudeMode;

typedef enum {
    OBJECT_FACE_SKETCH_HANDLE_NONE = 0,
    OBJECT_FACE_SKETCH_HANDLE_BODY = 1,
    OBJECT_FACE_SKETCH_HANDLE_CORNER_MIN_U_MIN_V = 2,
    OBJECT_FACE_SKETCH_HANDLE_CORNER_POS_U_MIN_V = 3,
    OBJECT_FACE_SKETCH_HANDLE_CORNER_POS_U_POS_V = 4,
    OBJECT_FACE_SKETCH_HANDLE_CORNER_MIN_U_POS_V = 5
} ObjectFaceSketchHandleKind;

typedef enum {
    OBJECT_AUTHORING_MODE_NONE = 0,
    OBJECT_AUTHORING_MODE_FACE_SELECT = 1,
    OBJECT_AUTHORING_MODE_SKETCH_DRAW = 2,
    OBJECT_AUTHORING_MODE_SKETCH_SELECT = 3,
    OBJECT_AUTHORING_MODE_OPERATION_PREVIEW = 4
} ObjectAuthoringMode;

typedef enum {
    OBJECT_EDIT_SELECTION_BODY = 0,
    OBJECT_EDIT_SELECTION_FACE = 1,
    OBJECT_EDIT_SELECTION_EDGE = 2,
    OBJECT_EDIT_SELECTION_VERTEX = 3
} ObjectEditSelectionMode;

typedef enum {
    SCENE_AUTHORING_EDIT_MODE_NONE = 0,
    SCENE_AUTHORING_EDIT_MODE_LIGHT = 1,
    SCENE_AUTHORING_EDIT_MODE_PATH = 2
} SceneAuthoringEditMode;

typedef struct {
    char** entries;
    size_t count;
    size_t capacity;
} EditorHistoryStack;

typedef struct {
    int anchorIndex;
    Vec3 startPos;
} AnchorDragSnapshot;

typedef struct {
    ToolMode mode;
    Vec3 anchor;        // Starting point for wall placement
    bool shiftHeld;     // Whether shift-lock is enabled

    bool selectionBoxActive;
    bool selectionBoxAdditive;
    Vec2 selectionBoxStart;
    Vec2 selectionBoxEnd;

    int selectedWallIndex;     // Index of selected wall
    int selectedAnchorIndex;   // Index of selected anchor (-1 if none)
    int hoveredWallIndex;
    int hoveredAnchorIndex;
    int selectedHandleAnchor;
    int selectedHandleComponent;   // 0 = incoming, 1 = outgoing, -1 = none
    uint32_t selectedObject3DId;
    uint32_t selectedObjectAssetBodyId;
    Object3DFaceKind selectedObjectAssetFace;
    int selectedObject3DResizeHandle; // PlaneResizeHandleKind or PLANE_RESIZE_HANDLE_NONE
    int selectedObject3DPrismHandle;  // RectPrismResizeHandleKind or RECT_PRISM_RESIZE_HANDLE_NONE
    int selectedSceneBoundsHandle;    // SceneBoundsHandleKind or SCENE_BOUNDS_HANDLE_NONE
    int hoveredHandleAnchor;
    int hoveredHandleComponent;
    int hoveredGizmoAxis;          // GizmoAxisDirection or -1
    int hoveredObject3DGizmoAxis;  // RectPrismAxisDirection or -1
    int activeObject3DGizmoAxis;   // RectPrismAxisDirection or -1 while dragging
    int hoveredSceneBoundsGizmoAxis; // RectPrismAxisDirection or -1
    int activeSceneBoundsGizmoAxis;  // RectPrismAxisDirection or -1 while dragging
    uint32_t hoveredObject3DId;
    uint32_t hoveredObjectAssetBodyId;
    Object3DFaceKind hoveredObjectAssetFace;
    uint32_t hoveredObjectTopologyBodyId;
    int hoveredObjectTopologyVertexIndex;
    int hoveredObjectTopologyEdgeIndex;
    int hoveredObject3DResizeHandle; // PlaneResizeHandleKind or PLANE_RESIZE_HANDLE_NONE
    int hoveredObject3DPrismHandle;  // RectPrismResizeHandleKind or RECT_PRISM_RESIZE_HANDLE_NONE
    int hoveredSceneBoundsHandle;    // SceneBoundsHandleKind or SCENE_BOUNDS_HANDLE_NONE
    int selectedSceneAuthoringPathIndex;
    int selectedSceneAuthoringControlPointIndex;
    int selectedSceneAuthoringPathElementKind;
    int selectedSceneAuthoringPathSegmentIndex;
    bool selectedSceneAuthoringLightPosition;
    bool selectedSceneAuthoringLightAim;
    bool selectedSceneAuthoringCameraAim;
    int hoveredSceneAuthoringHandleKind;
    int hoveredSceneAuthoringGizmoPart;
    int hoveredSceneAuthoringGizmoAxis;
    int hoveredSceneAuthoringPathElementKind;
    int hoveredSceneAuthoringPathIndex;
    int hoveredSceneAuthoringControlPointIndex;
    int hoveredSceneAuthoringPathSegmentIndex;
    int activeSceneAuthoringGizmoPart;
    int activeSceneAuthoringGizmoAxis;

    DeleteMode deleteMode;

    int* anchorSelection;
    int anchorSelectionCount;
    int anchorSelectionCapacity;

    AnchorDragSnapshot* dragSnapshots;
    int dragSnapshotCount;
    int dragSnapshotCapacity;
    bool isDraggingAnchor;
    bool isResizingObject3D;
    bool isResizingSceneBounds;
    bool isRotatingObject3D;
    bool isScalingObject3D;
    bool object3DRotateMode;
    bool object3DSizeMode;
    bool sceneBoundsHandlesVisible;
    PrimitivePlacementPreviewKind primitivePlacementPreview;
    SceneAuthoringEditMode sceneAuthoringEditMode;
    ObjectAuthoringMode objectAuthoringMode;
    ObjectEditSelectionMode objectEditSelectionMode;
    bool objectFaceSketchToolArmed;
    bool objectFaceSketchDragging;
    bool objectFaceSketchHasRectangle;
    uint32_t objectFaceSketchBodyId;
    Object3DFaceKind objectFaceSketchFace;
    PlaneFrame3 objectFaceSketchFrame;
    Vec2 objectFaceSketchStartUV;
    Vec2 objectFaceSketchCurrentUV;
    int hoveredObjectFaceSketchHandle;
    int selectedObjectFaceSketchHandle;
    int activeObjectFaceSketchHandle;
    bool objectFaceSketchEditDragging;
    Vec2 objectFaceSketchEditStartUV;
    Vec2 objectFaceSketchEditStartMinUV;
    Vec2 objectFaceSketchEditStartMaxUV;
    bool objectFaceExtrudeToolArmed;
    bool objectFaceExtrudeDragging;
    bool objectFaceExtrudeHasPreview;
    ObjectFaceExtrudeMode objectFaceExtrudeMode;
    uint32_t objectFaceExtrudeBodyId;
    Object3DFaceKind objectFaceExtrudeFace;
    PlaneFrame3 objectFaceExtrudeFrame;
    Vec2 objectFaceExtrudeStartScreen;
    float objectFaceExtrudeDepth;
    bool isPreciseDrag;
    GizmoAxisDragSession gizmoDrag;

    EditorHistoryStack undoStack;
    EditorHistoryStack redoStack;
} EditorState;

#define EDITOR_HISTORY_MAX 64

void Editor_Init(EditorState* editor);
void Editor_Free(EditorState* editor);

// Called when a snapped world point is clicked
void Editor_ClickAt(EditorState* editor, Vec3 worldPos);

// Called by input handler on Shift key down/up
void Editor_SetShiftHeld(EditorState* editor, bool held);

void Editor_HistoryCapture(EditorState* editor, const Layout* layout);
bool Editor_Undo(EditorState* editor, Layout* layout);
bool Editor_Redo(EditorState* editor, Layout* layout);
size_t Editor_UndoCount(const EditorState* editor);
size_t Editor_RedoCount(const EditorState* editor);
void Editor_ClearHistory(EditorState* editor);
void Editor_ResetDocumentState(EditorState* editor);
ObjectAuthoringMode Editor_ObjectAuthoringIdleMode(const EditorState* editor);
const char* Editor_ObjectAuthoringModeLabel(ObjectAuthoringMode mode);
const char* Editor_ObjectEditSelectionModeLabel(ObjectEditSelectionMode mode);
const char* Editor_ObjectAuthoringStageLabel(const EditorState* editor);
const char* Editor_ObjectAuthoringPromptLabel(const EditorState* editor);
bool Editor_SetSceneAuthoringEditMode(EditorState* editor, SceneAuthoringEditMode mode);
void Editor_ClearSceneAuthoringEditMode(EditorState* editor);
const char* Editor_SceneAuthoringEditModeLabel(SceneAuthoringEditMode mode);

void Editor_ClearAnchorSelection(EditorState* editor);
void Editor_SelectAnchor(EditorState* editor, int anchorIndex, bool additive);
bool Editor_IsAnchorSelected(const EditorState* editor, int anchorIndex);
int Editor_SelectedAnchorCount(const EditorState* editor);
void Editor_BeginAnchorDrag(EditorState* editor, const Layout* layout);
void Editor_UpdateAnchorDrag(EditorState* editor, Layout* layout, Vec3 primaryNewPos);
void Editor_EndAnchorDrag(EditorState* editor);
void Editor_ResetGizmoDrag(EditorState* editor);
void Editor_SelectAnchorsInBox(EditorState* editor, const Layout* layout, Vec2 min, Vec2 max, bool additive);

// Renders the ghost wall (anchor → current mouse world pos)
void Editor_Render(EditorState* editor, AppContext* ctx);
