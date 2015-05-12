#include "mannequin.h"
#include "mannequin_manipulator.h"
#include "move_manipulator.h"
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
#include <maya/MFnFreePointTriadManip.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MDagPathArray.h>

constexpr double MannequinContext::MANIP_DEFAULT_SCALE;
constexpr double MannequinContext::MANIP_ADJUSTMENT;

MannequinContext::MannequinContext() {}

void MannequinContext::forceExit() {
  MGlobal::executeCommand("setToolTo $gSelect");
}

void MannequinContext::select(const MDagPath& dagPath) {
  if (_selection == dagPath) {
    return;
  }

  _selection = dagPath;

  MDagPath oldHighlight;
  if (_mannequinManip) {
    // Preserve old highlight while transitioning manipulators.
    oldHighlight = _mannequinManip->highlightedDagPath();
  }
  deleteManipulators();
  addMannequinManipulator(oldHighlight);

  MFnRotateManip rotateManip(_jointManip);
  if (_selection.hasFn(MFn::kTransform)) {
    // Determine which manipulator we should be adding.
    int style = presentationStyleForJointDagPath(_selection);

    MFnTransform selectionXform(_selection);
    calculateJointLengthRatio(_selection);

    if (style & JointPresentationStyle::ROTATE) {
      MFnRotateManip rotateManip;
      _jointManip = rotateManip.create();
      MPlug rotationPlug = selectionXform.findPlug("rotate");

      rotateManip.connectToRotationPlug(rotationPlug);
      rotateManip.displayWithNode(_selection.node());
      rotateManip.setManipScale(manipAdjustedScale());
      rotateManip.setRotateMode(MFnRotateManip::kObjectSpace);
      addManipulator(_jointManip);
    } else if (style & JointPresentationStyle::TRANSLATE) {
			MObject manipObject;
      MoveManipulator* manipulator =
        (MoveManipulator*)MoveManipulator::newManipulator(
          "MannequinMoveManipulator", manipObject);

      manipulator->connectToDependNode(_selection.node());
      addManipulator(manipObject);
    }
  } else {
    // Nothing selected.
    _jointManip = MObject::kNullObj;
  }

  MString pythonSelectionCallback;
  pythonSelectionCallback.format("mannequinSelectionChanged(\"^1s\")",
    _selection.fullPathName());
  MGlobal::executePythonCommand(pythonSelectionCallback);

  updateText();
}

void MannequinContext::reselect() {
  MDagPath oldSelection = _selection;
  if (oldSelection.isValid()) {
    select(MDagPath());
    select(oldSelection);
  }
}

MDagPath MannequinContext::selectionDagPath() const {
  return _selection;
}

void MannequinContext::calculateDagLookupTables(MObject skinObj) {
  MFnSkinCluster skin(skinObj);
  MDagPathArray influenceObjects;
  unsigned int numInfluences = skin.influenceObjects(influenceObjects);

  for (int i = 0; i < numInfluences; ++i) {
    MDagPath dagPath = influenceObjects[i];
    _dagIndexLookup[dagPath] = i;
    _dagStyleLookup[dagPath] = dagPath.childCount() == 0 ?
      JointPresentationStyle::TRANSLATE : JointPresentationStyle::ROTATE;
  }
}

void MannequinContext::calculateMaxInfluences(MDagPath dagPath,
  MObject skinObj) {
  MFnMesh mesh(dagPath);
  MFnSkinCluster skin(skinObj);

  int numPolygons = mesh.numPolygons();
  _maxInfluences.resize(numPolygons);

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
    std::vector<double> weightSums(numInfluences, 0.0f);
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

    _maxInfluences[i] = maxIndex;
  }
}

void MannequinContext::calculateLongestJoint(MObject skinObj) {
  MFnSkinCluster skin(skinObj);

  MDagPathArray influenceObjects;
  int numInfluences = skin.influenceObjects(influenceObjects);

  double maxLength = 0.0;
  for (int i = 0; i < numInfluences; ++i) {
    MDagPath jointDagPath = influenceObjects[i];
    unsigned int children = jointDagPath.childCount();

    // We're looking through the children instead of at the actual joint
    // because we want to avoid the root transform. We end up looking at the
    // same number of objects since a transform can have only one parent.
    for (int i = 0; i < children; ++i) {
      MObject child = jointDagPath.child(i);
      if (child.hasFn(MFn::kJoint)) {
        MFnDagNode dagNode(child);
        MDagPath childDagPath;
        dagNode.getPath(childDagPath);
        MFnTransform childXform(childDagPath);
        maxLength = std::max(maxLength,
          childXform.getTranslation(MSpace::kObject).length());
      }
    }
  }

  _longestJoint = maxLength;
}

void MannequinContext::calculateJointLengthRatio(MDagPath jointDagPath) {
  if (_autoAdjust && _autoAdjust.value()) {
    unsigned int children = jointDagPath.childCount();
    double maxLength = 0.0;

    for (int i = 0; i < children; ++i) {
      MObject child = jointDagPath.child(i);
      if (child.hasFn(MFn::kJoint)) {
        MFnDagNode dagNode(child);
        MDagPath childDagPath;
        dagNode.getPath(childDagPath);
        MFnTransform childXform(childDagPath);
        maxLength = std::max(maxLength,
          childXform.getTranslation(MSpace::kObject).length());
      }
    }

    double rawRatio = maxLength / _longestJoint;
    _jointLengthRatio = rawRatio * 0.75 + 0.25; // Scale to [1/4, 1].
  } else {
    _jointLengthRatio = 1.0;
  }
}

const std::vector<int>& MannequinContext::maxInfluences() const {
  return _maxInfluences;
}

MDagPath MannequinContext::meshDagPath() const {
  return _meshDagPath;
}

MObject MannequinContext::skinObject() const {
  return _skinObject;
}

bool MannequinContext::addMannequinManipulator(MDagPath newHighlight) {
  MObject mannequinManipObj;
  MStatus err;
  _mannequinManip = (MannequinManipulator*)MPxManipulatorNode::newManipulator(
    "MannequinManipulator", mannequinManipObj, &err);

  if (err.error()) {
    return false;
  }

  _mannequinManip->setup(this, newHighlight);
  addManipulator(mannequinManipObj);
  return true;
}

bool MannequinContext::intersectRotateManip(MPoint linePoint,
  MVector lineDirection,
  float* distanceOut) {
  if (_jointManip.isNull()) {
    return false;
  }

  MStatus err;
  MFnTransform selectionXform(_selection);
  MPoint selectionPivot = selectionXform.rotatePivot(MSpace::kWorld, &err);

  // Extend manipulator radius a bit because of the free-rotation "shell".
  float manipRadius = manipAdjustedScale() * MFnManip3D::globalSize() * 1.25f;

  return Util::raySphereIntersection(linePoint,
    lineDirection,
    selectionPivot,
    manipRadius,
    distanceOut);
}

double MannequinContext::manipScale() const {
  if (!_scale) {
    bool optionExists;
    double scale = MGlobal::optionVarDoubleValue("chartreuseManipScale",
      &optionExists);

    if (optionExists) {
      _scale = scale;
    } else {
      _scale = MANIP_DEFAULT_SCALE;
    }
  }

  return _scale.value();
}

void MannequinContext::setManipScale(double scale) {
  MGlobal::setOptionVarValue("chartreuseManipScale", scale);

  _scale = scale;

  if (!_jointManip.isNull()) {
    reselect();
  }
}

bool MannequinContext::manipAutoAdjust() const {
  if (!_autoAdjust) {
    bool optionExists;
    bool autoAdjust = MGlobal::optionVarIntValue("chartreuseManipAutoAdjust",
      &optionExists) > 0;

    if (optionExists) {
      _autoAdjust = autoAdjust;
    } else {
      _autoAdjust = false;
    }
  }

  return _autoAdjust.value();
}

void MannequinContext::setManipAutoAdjust(bool autoAdjust) {
  MGlobal::setOptionVarValue("chartreuseManipAutoAdjust",
    autoAdjust ? 1 : 0);

  _autoAdjust = autoAdjust;

  if (!_jointManip.isNull()) {
    reselect();
  }
}

double MannequinContext::manipAdjustedScale() const {
  return manipScale() * MANIP_ADJUSTMENT * _longestJoint * _jointLengthRatio;
}

int MannequinContext::influenceIndexForJointDagPath(const MDagPath& dagPath) {
  auto value = _dagIndexLookup.find(dagPath);
  if (value != _dagIndexLookup.end()) {
    return value->second;
  }

  return -1;
}

int MannequinContext::presentationStyleForJointDagPath(
  const MDagPath& dagPath) const {
    auto styleIter = _dagStyleLookup.find(dagPath);
    if (styleIter == _dagStyleLookup.end()) {
      return JointPresentationStyle::NONE;
    }

    return styleIter->second;
}

void MannequinContext::toolOnSetup(MEvent& event) {
  MSelectionList list;
  MGlobal::getActiveSelectionList(list);

  MDagPath dagPath;
  MObject skinObj;

  // Verify whether the previous mesh and skin object are still valid.
  if (_meshDagPath.isValid() && !_skinObject.isNull()) {
    bool hasMesh = _meshDagPath.hasFn(MFn::kMesh);
    MStatus err;
    MFnSkinCluster skinCluster(_skinObject, &err);
    bool hasSkin = !err.error();

    if (hasMesh && hasSkin) {
      dagPath = _meshDagPath;
      skinObj = _skinObject;
    }
  }

  // If there's a current selection, try to edit that instead.
  if (list.length() != 0) {
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
  }

  // If we still don't have anything selected, we can't continue.
  if (!dagPath.isValid() || skinObj.isNull()) {
    MGlobal::displayError("Nothing selected");
    forceExit();
    return;
  }

  // Add DAG paths to their lookup tables.
  calculateDagLookupTables(skinObj);

  // Calculate the max influences for each face.
  calculateMaxInfluences(dagPath, skinObj);

  // Determine the longest joint length in the rig.
  calculateLongestJoint(skinObj);

  // Finally add the manipulator.
  bool didAdd = addMannequinManipulator();
  if (!didAdd) {
    MGlobal::displayError("Could not create manipulator");
    forceExit();
    return;
  }

  _meshDagPath = dagPath;
  _skinObject = skinObj;
  MGlobal::clearSelectionList();

  // Set image, title text, etc.
  setImage("mannequin_32.png", MPxContext::kImage1);
  setTitleString("Mannequin");
  updateText();
}

void MannequinContext::toolOffCleanup() {
  select(MDagPath());

  _mannequinManip = NULL;
  _jointManip = MObject::kNullObj;
  _maxInfluences.clear();
  _dagIndexLookup.clear();
  _dagStyleLookup.clear();

  deleteManipulators();
  MGlobal::clearSelectionList();
  MGlobal::executeCommand("mannequinContextFinish");
}

void MannequinContext::getClassName(MString& name) const {
  // Note: when setToolTo is called from MEL, Maya will try to load
  // mannequinContextProperties and mannequinContextValues.
  name.set("mannequinContext");
}

MStatus MannequinContext::doPress(MEvent& event,
  MHWRender::MUIDrawManager& drawMgr,
  const MHWRender::MFrameContext& context) {
  doPress();
  return MS::kSuccess;
}

MStatus MannequinContext::doPress(MEvent& event) {
  doPress();
  return MS::kSuccess;
}

void MannequinContext::doPress() {
  if (!_mannequinManip) {
    return;
  }

  select(_mannequinManip->highlightedDagPath());
}

void MannequinContext::abortAction() {
  select(MDagPath());
}

void MannequinContext::updateText() {
  if (_selection.isValid()) {
    MString help;
    help.format("^1s selected, press ESC to deselect",
      _selection.partialPathName());
    setHelpString(help);
  } else {
    setHelpString("Click on the mesh to select a part");
  }
}

MannequinContextCommand::MannequinContextCommand()
  : _mannequinContext(NULL) {}

MPxContext* MannequinContextCommand::makeObj() {
  _mannequinContext = new MannequinContext();
  return _mannequinContext;
}

void* MannequinContextCommand::creator() {
  return new MannequinContextCommand;
}

MStatus MannequinContextCommand::doEditFlags() {
  MArgParser parse = parser();

  if (!_mannequinContext) {
    return MS::kInvalidParameter;
  }

  if (parse.isFlagSet("-io")) {
    return MS::kInvalidParameter;
  } else if (parse.isFlagSet("-sel")) {
    MStatus err;
    MString arg = parse.flagArgumentString("-sel", 0, &err);
    if (err.error()) {
      return err;
    }

    MFnSkinCluster skin(_mannequinContext->skinObject(), &err);
    if (err.error()) {
      return err;
    }

    MDagPathArray influenceObjects;
    int numInfluences = skin.influenceObjects(influenceObjects);

    for (int i = 0; i < numInfluences; i++) {
      MString fullName = influenceObjects[i].fullPathName();
      MString partialName = influenceObjects[i].partialPathName();

      if (arg == fullName || arg == partialName) {
        _mannequinContext->select(influenceObjects[i]);
        return MS::kSuccess;
      }
    }

    MString errMessage;
    errMessage.format("Couldn't find and select ^1s", arg);
    MGlobal::displayWarning(errMessage);
  } else if (parse.isFlagSet("-ms")) {
    MStatus err;
    double arg = parse.flagArgumentDouble("-ms", 0, &err);
    if (err.error()) {
      return err;
    }

    _mannequinContext->setManipScale(arg);
    return MS::kSuccess;
  } else if (parse.isFlagSet("-ma")) {
    MStatus err;
    bool arg = parse.flagArgumentBool("-ma", 0, &err);
    if (err.error()) {
      return err;
    }

    _mannequinContext->setManipAutoAdjust(arg);
    return MS::kSuccess;
  }

  return MS::kSuccess;
}

MStatus MannequinContextCommand::doQueryFlags() {
  MArgParser parse = parser();

  if (!_mannequinContext) {
    return MS::kInvalidParameter;
  }

  if (parse.isFlagSet("-io")) {
    MStatus err;
    MFnSkinCluster skin(_mannequinContext->skinObject(), &err);
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

      MDagPath dagPath = influenceObjects[i];
      int style = _mannequinContext->presentationStyleForJointDagPath(dagPath);

      result += dagPath.fullPathName();
      result += " !";
      result += (style & JointPresentationStyle::ROTATE) ? "r" : "";
      result += (style & JointPresentationStyle::TRANSLATE) ? "t" : "";
    }

    setResult(result);
  } else if (parse.isFlagSet("-sel")) {
    MDagPath dagPath = _mannequinContext->selectionDagPath();
    MString result;
    if (dagPath.isValid()) {
      result = MString(dagPath.fullPathName());
    } else {
      result = MString("");
    }

    setResult(result);
  } else if (parse.isFlagSet("-ms")) {
    double result = _mannequinContext->manipScale();
    setResult(result);
  } else if (parse.isFlagSet("-ma")) {
    bool result = _mannequinContext->manipAutoAdjust();
    setResult(result);
  }

  return MS::kSuccess;
}

MStatus MannequinContextCommand::appendSyntax() {
	MSyntax syn = syntax();

  syn.addFlag("-io", "-influenceObjects");
  syn.addFlag("-sel", "-selection", MSyntax::kString);
  syn.addFlag("-ms", "-manipSize", MSyntax::kDouble);
  syn.addFlag("-ma", "-manipAdjust", MSyntax::kDouble);

  return MS::kSuccess;
}

MStatus initializePlugin(MObject obj)
{
  MStatus status;
  MFnPlugin plugin(obj, "Steven Dao", "0.1", "Any");

  status = plugin.registerContextCommand("mannequinContext",
    MannequinContextCommand::creator);

  status = plugin.registerNode("MannequinManipulator",
    MannequinManipulator::id,
    &MannequinManipulator::creator,
    &MannequinManipulator::initialize,
    MPxNode::kManipulatorNode);

  status = plugin.registerNode("MannequinMoveManipulator",
    MoveManipulator::id,
    &MoveManipulator::creator,
    &MoveManipulator::initialize,
    MPxNode::kManipContainer);

  status = MGlobal::executePythonCommand("from mannequin import *");
  status = MGlobal::sourceFile("mannequin.mel");
  status = MGlobal::executeCommand("mannequinInstallShelf");

  return status;
}

MStatus uninitializePlugin(MObject obj)
{
  MStatus status;
  MFnPlugin plugin(obj);

  status = plugin.deregisterContextCommand("mannequinContext");
  status = plugin.deregisterNode(MannequinManipulator::id);
  status = plugin.deregisterNode(MoveManipulator::id);

  return status;
}
