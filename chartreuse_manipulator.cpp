#include "chartreuse_manipulator.h"
#include "chartreuse.h"

#include <iostream>

#include <maya/MStatus.h>
#include <maya/MFnTransform.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFloatPoint.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MGlobal.h>

const MTypeId ChartreuseManipulator::id = MTypeId(0xcafecab);

ChartreuseManipulator::ChartreuseManipulator() : _ctx(NULL) {}

void ChartreuseManipulator::setup(ChartreuseContext* ctx,
  MDagPath newHighlight) {
  _ctx = ctx;
}

MDagPath ChartreuseManipulator::highlightedDagPath() const {
  if (!_highlight.hasFn(MFn::kTransform)) {
    return MDagPath();
  }

  return _highlight;
}

void ChartreuseManipulator::postConstructor() {
  registerForMouseMove();
}

MStatus ChartreuseManipulator::doMove(M3dView& view, bool& refresh) {
  MPoint linePoint;
  MVector lineDirection;
  mouseRayWorld(linePoint, lineDirection);

  if (!_ctx) {
    return doMoveError(refresh);
  }

  MFnMesh mesh(_ctx->meshDagPath());
  MFnSkinCluster skin(_ctx->skinObject());

  MFloatPoint worldLinePoint;
  worldLinePoint.setCast(linePoint);

  MFloatPoint hitPoint;
  int hitFace;
  bool hit = mesh.closestIntersection(worldLinePoint, lineDirection,
    NULL, NULL, false, MSpace::kWorld, 1000.0f, false, NULL, hitPoint, NULL,
    &hitFace, NULL, NULL, NULL, 1e-3);

  if (!hit) {
    return doMoveError(refresh);
  }

  // Select all related faces.
  const unsigned int* maxInfluences = _ctx->maxInfluences();
  if (!maxInfluences) {
    return doMoveError(refresh);
  }

  MFnSingleIndexedComponent comp;
  MObject compObj = comp.create(MFn::kMeshPolygonComponent);

  unsigned int hitFaceInfluence = maxInfluences[hitFace];
  int numPolygons = mesh.numPolygons();
  for (int i = 0; i < numPolygons; ++i) {
    if (maxInfluences[i] == hitFaceInfluence) {
      comp.addElement(i);
    }
  }

  MGlobal::select(_ctx->meshDagPath(), compObj, MGlobal::kReplaceList);

  // Finish up!
  MDagPathArray influenceObjects;
  unsigned int numObjects = skin.influenceObjects(influenceObjects);

  _highlight = influenceObjects[hitFaceInfluence];
  refresh = true;
  return MS::kSuccess;
}

MStatus ChartreuseManipulator::doMoveError(bool& refresh) {
  MGlobal::clearSelectionList();

  // We also need to select the current joint selection, if any.
  MDagPath parentSelection = _ctx->selectionDagPath();
  MGlobal::select(parentSelection, MObject::kNullObj, MGlobal::kAddToList);

  _highlight = MDagPath();
  refresh = true;
  return MS::kUnknownParameter;
}

void ChartreuseManipulator::draw(M3dView &view,
  const MDagPath &path,
  M3dView::DisplayStyle style,
  M3dView::DisplayStatus status) {
  // Note: since I'm leaving this empty, the manipulator will not render
  // in the Legacy Viewport.
}

void ChartreuseManipulator::preDrawUI(const M3dView &view) {}

void ChartreuseManipulator::drawUI(MHWRender::MUIDrawManager &drawManager,
  const MHWRender::MFrameContext &frameContext) const {
  if (!_highlight.hasFn(MFn::kTransform)) {
    return;
  }

  MFnTransform selectionXform(_highlight);
  MPoint pivot = selectionXform.rotatePivot(MSpace::kWorld);

  unsigned int children = _highlight.childCount();
  unsigned int numChildJoints = 0;
  MObject singleChildJoint;
  for (int i = 0; i < children; ++i) {
    MObject child = _highlight.child(i);
    if (child.hasFn(MFn::kJoint)) {
      numChildJoints++;
      singleChildJoint = child;
    }
  }

  MColor blue(0.3f, 0.8f, 0.1f);
  drawManager.beginDrawable();
  drawManager.setColor(blue);

  MPoint centerPoint;
  if (numChildJoints == 1) {
    MFnDagNode dagNode(singleChildJoint);
    MDagPath childDagPath;
    dagNode.getPath(childDagPath);
    MFnTransform childXform(childDagPath);
    MPoint childPivot = childXform.rotatePivot(MSpace::kWorld);

    MVector diff = childPivot - pivot;
    centerPoint = pivot + diff * 0.5f;
  } else {
    centerPoint = pivot;
  }

  drawManager.text(centerPoint, _highlight.partialPathName(),
    MHWRender::MUIDrawManager::kCenter);

  drawManager.endDrawable();
}

void* ChartreuseManipulator::creator() {
  return new ChartreuseManipulator;
}

MStatus ChartreuseManipulator::initialize() {
  return MS::kSuccess;
}
