#include "chartreuse.h"
#include "chartreuse_manipulator.h"
#include "util.h"

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
#include <maya/MDagPathArray.h>

ChartreuseContext::ChartreuseContext() : _maxInfluences(NULL) {}

ChartreuseContext::~ChartreuseContext() {
  delete[] _maxInfluences;
}

void ChartreuseContext::forceExit() {
  MGlobal::executeCommand("setToolTo $gSelect");
}

void ChartreuseContext::select(const MDagPath& dagPath) {
  if (_selection == dagPath) {
    return;
  }

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

    rotateManip.connectToRotationPlug(rotationPlug);
    rotateManip.displayWithNode(_selection.node());
    rotateManip.setManipScale(ROTATE_MANIP_SCALE);
    rotateManip.setRotateMode(MFnRotateManip::kObjectSpace);
    addManipulator(_rotateManip);
  } else {
    _rotateManip = MObject::kNullObj;
  }

  MString pythonSelectionCallback;
  pythonSelectionCallback.format("chartreuseSelectionChanged(\"^1s\")",
    _selection.fullPathName());
  MGlobal::executePythonCommand(pythonSelectionCallback);

  updateText();
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

bool ChartreuseContext::intersectRotateManip(MPoint linePoint,
  MVector lineDirection,
  float* distanceOut) {
  if (_rotateManip.isNull()) {
    return false;
  }

  MStatus err;
  MFnTransform selectionXform(_selection);
  MPoint selectionPivot = selectionXform.rotatePivot(MSpace::kWorld, &err);

  // Extend manipulator radius a bit because of the free-rotation "shell".
  float manipRadius = ROTATE_MANIP_SCALE * MFnManip3D::globalSize() * 1.25f;

  return Util::raySphereIntersection(linePoint,
    lineDirection,
    selectionPivot,
    manipRadius,
    distanceOut);
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

  // Set image, title text, etc.
  setImage("chartreuse_32.png", MPxContext::kImage1);
  setTitleString("Chartreuse");
  updateText();
}

void ChartreuseContext::toolOffCleanup() {
  select(MDagPath());

  _chartreuseManip = NULL;
  _rotateManip = MObject::kNullObj;

  deleteManipulators();
  MGlobal::clearSelectionList();
  MGlobal::executeCommand("chartreuseContextFinish");
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

void ChartreuseContext::updateText() {
  if (_selection.isValid()) {
    MString help;
    help.format("^1s selected, press ESC to deselect",
      _selection.partialPathName());
    setHelpString(help);
  } else {
    setHelpString("Click on the mesh to select a part");
  }
}

ChartreuseContextCommand::ChartreuseContextCommand()
  : _chartreuseContext(NULL) {}

MPxContext* ChartreuseContextCommand::makeObj() {
  _chartreuseContext = new ChartreuseContext();
  return _chartreuseContext;
}

void* ChartreuseContextCommand::creator() {
  return new ChartreuseContextCommand;
}

MStatus ChartreuseContextCommand::doEditFlags() {
  MArgParser parse = parser();

  if (!_chartreuseContext) {
    return MS::kInvalidParameter;
  }

  if (parse.isFlagSet("-io")) {
    return MS::kInvalidParameter;
  } else if (parse.isFlagSet("-sel")) {
    MString arg = parse.flagArgumentString("-sel", 0);

    MStatus err;
    MFnSkinCluster skin(_chartreuseContext->skinObject(), &err);
    if (err.error()) {
      return err;
    }

    MDagPathArray influenceObjects;
    int numInfluences = skin.influenceObjects(influenceObjects);

    for (int i = 0; i < numInfluences; i++) {
      MString fullName = influenceObjects[i].fullPathName();
      MString partialName = influenceObjects[i].partialPathName();

      if (arg == fullName || arg == partialName) {
        _chartreuseContext->select(influenceObjects[i]);
        return MS::kSuccess;
      }
    }

    MString errMessage;
    errMessage.format("Couldn't find and select ^1s", arg);
    MGlobal::displayWarning(errMessage);
  }

  return MS::kSuccess;
}

MStatus ChartreuseContextCommand::doQueryFlags() {
  MArgParser parse = parser();

  if (!_chartreuseContext) {
    return MS::kInvalidParameter;
  }

  if (parse.isFlagSet("-io")) {
    MStatus err;
    MFnSkinCluster skin(_chartreuseContext->skinObject(), &err);
    if (err.error()) {
      return err;
    }

    MDagPathArray influenceObjects;
    int numInfluences = skin.influenceObjects(influenceObjects);

    MString result;
    for (int i = 0; i < numInfluences; i++) {
      if (i != 0) {
        result += " ";
      }

      result += influenceObjects[i].fullPathName();
    }

    setResult(result);
  } else if (parse.isFlagSet("-sel")) {
    MDagPath dagPath = _chartreuseContext->selectionDagPath();
    MString result;
    if (dagPath.isValid()) {
      result = MString(dagPath.fullPathName());
    } else {
      result = MString("");
    }

    setResult(result);
  }

  return MS::kSuccess;
}

MStatus ChartreuseContextCommand::appendSyntax() {
	MSyntax syn = syntax();

  syn.addFlag("-io", "-influenceObjects");
  syn.addFlag("-sel", "-selection", MSyntax::kString);

  return MS::kSuccess;
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

  status = MGlobal::executePythonCommand("from chartreuse import *");
  status = MGlobal::sourceFile("chartreuse.mel");
  status = MGlobal::executeCommand("chartreuseInstallShelf");

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
