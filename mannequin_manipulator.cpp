#include "mannequin_manipulator.h"
#include "mannequin.h"

#include <iostream>

#include <maya/MStatus.h>
#include <maya/MFnTransform.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFloatPoint.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MGlobal.h>
#include <maya/MFnManip3D.h>

const MTypeId MannequinManipulator::id = MTypeId(0xcafecab);

MannequinManipulator::MannequinManipulator() : _ctx(NULL) {}

void MannequinManipulator::setup(MannequinContext* ctx,
  MDagPath newHighlight) {
  _ctx = ctx;
  highlight(newHighlight, true);
}

bool MannequinManipulator::highlight(MDagPath dagPath, bool force) {
  if (!force && dagPath == _highlight) {
    // Maintain the status quo if not forced!
    return false;
  }

  do {
    if (!_ctx) {
      break;
    }

    MFnMesh mesh(_ctx->meshDagPath());
    int numPolygons = mesh.numPolygons();
    const std::vector<int>& maxInfluences = _ctx->maxInfluences();
    if (maxInfluences.size() != numPolygons) {
      break;
    }

    MFnSingleIndexedComponent comp;
    MObject compObj = comp.create(MFn::kMeshPolygonComponent);

    int highlightIndex = _ctx->influenceIndexForJointDagPath(dagPath);
    int selectionIndex = _ctx->influenceIndexForJointDagPath(
      _ctx->selectionDagPath());

    for (int i = 0; i < numPolygons; ++i) {
      if (maxInfluences[i] == highlightIndex ||
          maxInfluences[i] == selectionIndex) {
        comp.addElement(i);
      }
    }

    _highlight = dagPath;
    MGlobal::select(_ctx->meshDagPath(), compObj, MGlobal::kReplaceList);
    return true;
  } while (false);

error:
  _highlight = MDagPath();
  MGlobal::clearSelectionList();
  return true;
}

MDagPath MannequinManipulator::highlightedDagPath() const {
  if (!_highlight.hasFn(MFn::kTransform)) {
    return MDagPath();
  }

  return _highlight;
}

void MannequinManipulator::postConstructor() {
  registerForMouseMove();
}

MStatus MannequinManipulator::doMove(M3dView& view, bool& refresh) {
  do {
    if (!_ctx) {
      break;
    }

    // If the mouse is near the border (e.g. within 4px), don't highlight.
    // This works around some bugs where a section can remain highlighted!
    int portWidth = view.portWidth();
    int portHeight = view.portHeight();
    short screenX, screenY;
    mousePosition(screenX, screenY);

    if (screenX < 4 || screenY < 4 ||
        screenX >= portWidth - 4 || screenY >= portHeight - 4) {
      break;
    }

    // Begin the actual hit-testing routine.
    MPoint linePoint;
    MVector lineDirection;
    mouseRayWorld(linePoint, lineDirection);

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
      break;
    }

    bool hitManip = _ctx->intersectManip(this);
    if (hitManip) {
      // We're pointing at the rotation/translation manipulator.
      break;
    }

    // Figure out the joint we've landed on.
    int numPolygons = mesh.numPolygons();
    const std::vector<int>& maxInfluences = _ctx->maxInfluences();
    if (maxInfluences.size() != numPolygons) {
      break;
    }

    MDagPathArray influenceObjects;
    skin.influenceObjects(influenceObjects);
    unsigned int hitFaceInfluence = maxInfluences[hitFace];
    MDagPath influenceDagPath = influenceObjects[hitFaceInfluence];

    refresh = highlight(influenceDagPath);
    return MS::kSuccess;
  } while (false);

error:
  refresh = highlight();
  return MS::kUnknownParameter;
}

void MannequinManipulator::draw(M3dView &view,
  const MDagPath &path,
  M3dView::DisplayStyle style,
  M3dView::DisplayStatus status) {
  if (!_highlight.hasFn(MFn::kTransform)) {
    return;
  }

  MColor green(0.3f, 0.8f, 0.1f);
  view.beginGL();
  view.setDrawColor(green);

  MPoint centerPoint = drawCenter();
  MString text = _highlight.partialPathName();
  view.drawText(text, centerPoint, M3dView::kCenter);

  view.endGL();
}

void MannequinManipulator::preDrawUI(const M3dView &view) {}

void MannequinManipulator::drawUI(MHWRender::MUIDrawManager &drawManager,
  const MHWRender::MFrameContext &frameContext) const {
  if (!_highlight.hasFn(MFn::kTransform)) {
    return;
  }

  MColor green(0.3f, 0.8f, 0.1f);
  drawManager.beginDrawable();
  drawManager.setColor(green);

  MPoint centerPoint = drawCenter();
  MString text = _highlight.partialPathName();
  drawManager.text(centerPoint, text, MHWRender::MUIDrawManager::kCenter);

  drawManager.endDrawable();
}

MPoint MannequinManipulator::drawCenter() const {
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

  return centerPoint;
}

void* MannequinManipulator::creator() {
  return new MannequinManipulator;
}

MStatus MannequinManipulator::initialize() {
  return MS::kSuccess;
}
