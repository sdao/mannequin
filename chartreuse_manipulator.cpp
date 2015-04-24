#include "chartreuse_manipulator.h"

#include <limits>
#include <iostream>

#include <maya/MStatus.h>
#include <maya/MFnTransform.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFloatPoint.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSkinCluster.h>

const MTypeId ChartreuseManipulator::id = MTypeId(0xcafecab);

ChartreuseManipulator::ChartreuseManipulator() : _initialized(false) {}

void ChartreuseManipulator::setup(MDagPath meshDagPath, MObject skinObject) {
  _meshDagPath = meshDagPath;
  _skinObject = skinObject;
  _initialized = true;
}

void ChartreuseManipulator::postConstructor() {
  registerForMouseMove();
}

MStatus ChartreuseManipulator::doMove(M3dView& view, bool& refresh) {
  MPoint linePoint;
  MVector lineDirection;
  mouseRayWorld(linePoint, lineDirection);

  if (!_initialized) {
    return doMoveError(refresh);
  }

  MFnMesh mesh(_meshDagPath);

  MFloatPoint worldLinePoint;
  worldLinePoint.setCast(linePoint);

  MFloatPoint hitPoint;
  int hitFace;
  int hitTri;
  float bary0;
  float bary1;
  bool hit = mesh.closestIntersection(worldLinePoint, lineDirection,
    NULL, NULL, false, MSpace::kWorld, 1000.0f, false, NULL, hitPoint, NULL,
    &hitFace, &hitTri, &bary0, &bary1, 1e-3);
  float bary2 = 1.0f - bary0 - bary1;

  if (!hit) {
    return doMoveError(refresh);
  }

  int closestVtx;
  if (bary0 >= bary1 && bary0 >= bary2) {
    closestVtx = 0;
  } else if (bary1 >= bary0 && bary1 >= bary2) {
    closestVtx = 1;
  } else {
    closestVtx = 2;
  }

  int vertexList[3];
  MStatus err =
    mesh.getPolygonTriangleVertices(hitFace, hitTri, vertexList);

  if (err.error()) {
    return doMoveError(refresh);
  }

  int vtx = vertexList[closestVtx];

  MFnSkinCluster skin(_skinObject);
  MFnSingleIndexedComponent c;
  MObject componentObj = c.create(MFn::kMeshVertComponent);
  c.addElement(vtx);

  MDagPathArray influenceObjects;
  unsigned int numObjects = skin.influenceObjects(influenceObjects);

  MDoubleArray weights;
  unsigned int numInfluences;
  err = skin.getWeights(_meshDagPath, componentObj, weights, numInfluences);

  if (err.error()) {
    return doMoveError(refresh);
  }

  float maxVal = std::numeric_limits<float>::min();
  int maxIdx = 0;
  for (int i = 0; i < numInfluences; ++i) {
    if (weights[i] > maxVal) {
      maxVal = weights[i];
      maxIdx = i;
    }
  }
  MDagPath maxObj = influenceObjects[maxIdx];

  if (!maxObj.hasFn(MFn::kTransform)) {
    return doMoveError(refresh);
  }

  _highlight = maxObj;
  refresh = true;
  return MS::kSuccess;
}

MStatus ChartreuseManipulator::doMoveError(bool& refresh) {
  _highlight = MDagPath();
  refresh = true;
  return MS::kSuccess;
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

  MColor red(1.0f, 0.0f, 0.0f);
  drawManager.beginDrawable();
  drawManager.setColor(red);
  drawManager.setLineWidth(4.0f);

  if (numChildJoints == 1) {
    MFnDagNode dagNode(singleChildJoint);
    MDagPath childDagPath;
    dagNode.getPath(childDagPath);
    MFnTransform childXform(childDagPath);
    MPoint childPivot = childXform.rotatePivot(MSpace::kWorld);
    drawManager.line(pivot, childPivot);
  } else {
    drawManager.sphere(pivot, 2.0f);
  }

  drawManager.endDrawable();
}

void* ChartreuseManipulator::creator() {
  return new ChartreuseManipulator;
}

MStatus ChartreuseManipulator::initialize() {
  return MS::kSuccess;
}
