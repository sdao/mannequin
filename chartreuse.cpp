#include "chartreuse.h"
#include "chartreuse_manipulator.h"

#include <limits>

#include <maya/MStatus.h>
#include <maya/MFnPlugin.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MFnMesh.h>
#include <maya/MFnRotateManip.h>
#include <maya/MFnSingleIndexedComponent.h>

ChartreuseContext::ChartreuseContext() : _maxInfluences(NULL) {}

ChartreuseContext::~ChartreuseContext() {
  delete[] _maxInfluences;
}

void ChartreuseContext::forceExit() {
  MGlobal::executeCommand("setToolTo $gSelect");
}

void ChartreuseContext::select(const MDagPath& dagPath) {
  _selection = dagPath;

  MDagPath oldHighlight;
  if (_chartreuseManip) {
    // Preserve old highlight while transitioning manipulators.
    oldHighlight = _chartreuseManip->highlightedDagPath();
  }
  deleteManipulators();
  addChartreuseManipulator(oldHighlight);

  MFnRotateManip rotateManip(_rotateManip);
  if (_selection.hasFn(MFn::kTransform)) {
    MFnRotateManip rotateManip;
    _rotateManip = rotateManip.create();

    MFnTransform selectionXform(_selection);
    MPlug rotationPlug = selectionXform.findPlug("rotate");
    MPlug rotationCenterPlug = selectionXform.findPlug("rotatePivot");

    rotateManip.connectToRotationPlug(rotationPlug);
    rotateManip.connectToRotationCenterPlug(rotationCenterPlug);
    rotateManip.displayWithNode(_selection.node());
    rotateManip.setManipScale(ROTATE_MANIP_SCALE);
    rotateManip.setRotateMode(MFnRotateManip::kObjectSpace);
    addManipulator(_rotateManip);
  } else {
    _rotateManip = MObject::kNullObj;
  }
}

MDagPath ChartreuseContext::selectionDagPath() const {
  return _selection;
}

void ChartreuseContext::calculateMaxInfluences(MDagPath dagPath,
  MObject skinObj) {
  MFnMesh mesh(dagPath);
  MFnSkinCluster skin(skinObj);

  int numPolygons = mesh.numPolygons();
  delete[] _maxInfluences;
  _maxInfluences = new unsigned int[numPolygons];

  for (int i = 0; i < numPolygons; ++i) {
    MIntArray polyVertices;
    mesh.getPolygonVertices(i, polyVertices);

    MFnSingleIndexedComponent comp;
    MObject compObj = comp.create(MFn::kMeshVertComponent);
    comp.addElements(polyVertices);

    MDoubleArray weights;
    unsigned int numInfluences;
    skin.getWeights(dagPath, compObj, weights, numInfluences);

    int count = 0;
    double* weightSums = new double[numInfluences];
    for (int influence = 0; influence < numInfluences; ++influence) {
      weightSums[influence] = 0.0f;
    }
    for (int vtx = 0; vtx < polyVertices.length(); ++vtx) {
      for (int influence = 0; influence < numInfluences; ++influence) {
        weightSums[influence] += weights[count];
        count++;
      }
    }

    double maxWeight = std::numeric_limits<double>::min();
    int maxIndex = 0;
    for (int influence = 0; influence < numInfluences; ++influence) {
      if (weightSums[influence] > maxWeight) {
        maxWeight = weightSums[influence];
        maxIndex = influence;
      }
    }

    delete[] weightSums;
    _maxInfluences[i] = maxIndex;
  }
}

const unsigned int* ChartreuseContext::maxInfluences() const {
  return _maxInfluences;
}

MDagPath ChartreuseContext::meshDagPath() const {
  return _meshDagPath;
}

MObject ChartreuseContext::skinObject() const {
  return _skinObject;
}

bool ChartreuseContext::addChartreuseManipulator(MDagPath newHighlight) {
  MObject chartreuseManipObj;
  MStatus err;
  _chartreuseManip = (ChartreuseManipulator*)MPxManipulatorNode::newManipulator(
    "ChartreuseManipulator", chartreuseManipObj, &err);

  if (err.error()) {
    return false;
  }

  _chartreuseManip->setup(this, newHighlight);
  addManipulator(chartreuseManipObj);
  return true;
}

void ChartreuseContext::toolOnSetup(MEvent& event) {
  MSelectionList list;
  MGlobal::getActiveSelectionList(list);

  if (list.length() == 0) {
    MGlobal::displayError("Nothing selected");
    forceExit();
    return;
  }

  MDagPath dagPath;
  list.getDagPath(0, dagPath);
  dagPath.extendToShape();

  if (!dagPath.hasFn(MFn::kMesh)) {
    MGlobal::displayError("Selection is not a mesh");
    forceExit();
    return;
  }

  MFnMesh mesh(dagPath);
  MItDependencyNodes depNodeIter(MFn::kSkinClusterFilter);
  bool hasSkinCluster = false;
  MObject skinObj;
  for(; !depNodeIter.isDone() && !hasSkinCluster; depNodeIter.next()) {
    MObject node = depNodeIter.item();
    MStatus err;
    MFnSkinCluster skinCluster(node, &err);
    if (err.error()) {
      continue;
    }

    unsigned int numGeoms = skinCluster.numOutputConnections();
    for (unsigned int i = 0; i < numGeoms && !hasSkinCluster; ++i) {
      unsigned int index = skinCluster.indexForOutputConnection(i);
      MObject output = skinCluster.outputShapeAtIndex(index);
      if(output == mesh.object())
      {
        hasSkinCluster = true;
        skinObj = node;
      }
    }
  }

  if (!hasSkinCluster) {
    MGlobal::displayError("Selection has no smooth skin bound");
    forceExit();
    return;
  }

  // Calculate the max influences for each face.
  calculateMaxInfluences(dagPath, skinObj);

  // Finally add the manipulator.
  bool didAdd = addChartreuseManipulator();
  if (!didAdd) {
    MGlobal::displayError("Could not create manipulator");
    forceExit();
    return;
  }

  _meshDagPath = dagPath;
  _skinObject = skinObj;
  MGlobal::clearSelectionList();
}

void ChartreuseContext::toolOffCleanup() {
  _chartreuseManip = NULL;
  _rotateManip = MObject::kNullObj;
  deleteManipulators();
  MGlobal::clearSelectionList();
}

void ChartreuseContext::getClassName(MString& name) const {
  // Note: when setToolTo is called from MEL, Maya will try to load
  // chartreuseContextProperties and chartreuseContextValues.
  name.set("chartreuseContext");
}

MStatus ChartreuseContext::doPress(MEvent& event,
  MHWRender::MUIDrawManager& drawMgr,
  const MHWRender::MFrameContext& context) {
  doPress();
  return MS::kSuccess;
}

MStatus ChartreuseContext::doPress(MEvent& event) {
  doPress();
  return MS::kSuccess;
}

void ChartreuseContext::doPress() {
  if (!_chartreuseManip) {
    return;
  }

  select(_chartreuseManip->highlightedDagPath());
}

void ChartreuseContext::abortAction() {
  select(MDagPath());
}

ChartreuseContextCommand::ChartreuseContextCommand() {}

MPxContext* ChartreuseContextCommand::makeObj() {
  return new ChartreuseContext();
}

void* ChartreuseContextCommand::creator() {
  return new ChartreuseContextCommand;
}

MStatus initializePlugin(MObject obj)
{
  MStatus status;
  MFnPlugin plugin(obj, "Steven Dao", "0.1", "Any");

  status = plugin.registerContextCommand("chartreuseContext",
    ChartreuseContextCommand::creator);

  status = plugin.registerNode("ChartreuseManipulator",
    ChartreuseManipulator::id,
    &ChartreuseManipulator::creator,
    &ChartreuseManipulator::initialize,
    MPxNode::kManipulatorNode);

  return status;
}

MStatus uninitializePlugin(MObject obj)
{
  MStatus status;
  MFnPlugin plugin(obj);

  status = plugin.deregisterContextCommand("chartreuseContext");
  status = plugin.deregisterNode(ChartreuseManipulator::id);

  return status;
}
