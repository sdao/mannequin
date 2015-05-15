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
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MDagPathArray.h>
#include <maya/M3dView.h>
#include <maya/MItMeshPolygon.h>

constexpr double MannequinContext::MANIP_DEFAULT_SCALE;
constexpr double MannequinContext::MANIP_ADJUSTMENT;

MannequinContext::MannequinContext()
  : _mannequinManip(nullptr),
    _moveManip(nullptr),
    _selectionStyle(JointPresentationStyle::NONE) {}

void MannequinContext::forceExit() {
  MGlobal::executeCommand("setToolTo $gSelect");
}

void MannequinContext::select(const MDagPath& dagPath, int style) {
  // Determine which style to use/request.
  if (style == JointPresentationStyle::NONE) {
    // Try to preserve the presentation style used previously.
    style = _selectionStyle;
  }

  int availableStyles = presentationStyleForJointDagPath(dagPath);
  if (style & availableStyles) {
    // Requested presentation style available.
    style = style & availableStyles;
  } else {
    // Requested presentation style unavailable, so ignore it.
    style = availableStyles;
  }

  // If the DAG path + style combo is still the same, then DO NOTHING.
  if ((_selection == dagPath) && (style & _selectionStyle)) {
    return;
  }

  _selection = dagPath;
  calculateJointLengthRatio(_selection);

  MDagPath oldHighlight;
  if (_mannequinManip) {
    // Preserve old highlight while transitioning manipulators.
    oldHighlight = _mannequinManip->highlightedDagPath();
  }

  deleteManipulators();
  addMannequinManipulator(oldHighlight);
  _rotateManip = nullptr;
  _moveManip = nullptr;

  if (_selection.hasFn(MFn::kTransform)) {
    MFnTransform selectionXform(_selection);

    // Just use the first presentation style that we can.
    if (style & JointPresentationStyle::ROTATE) {
      MFnRotateManip rotateManip;
      MObject rotateManipObj = rotateManip.create();
      _rotateManip = (void*)true;

      MPlug rotationPlug = selectionXform.findPlug("rotate");

      rotateManip.connectToRotationPlug(rotationPlug);
      rotateManip.displayWithNode(_selection.node());
      rotateManip.setManipScale(manipAdjustedScale());
      rotateManip.setRotateMode(MFnRotateManip::kObjectSpace);

      _availableStyles = availableStyles;
      _selectionStyle = JointPresentationStyle::ROTATE;
      addManipulator(rotateManipObj);
    } else if (style & JointPresentationStyle::TRANSLATE) {
      MObject moveManipObj;
      _moveManip = (MannequinMoveManipulator*)
        MPxManipulatorNode::newManipulator(
          "MannequinMoveManipulator",
          moveManipObj);

      _moveManip->connectToDependNode(_selection.node());
      _moveManip->setManipScale(manipAdjustedScale() * 1.25f);

      _availableStyles = availableStyles;
      _selectionStyle = JointPresentationStyle::TRANSLATE;
      addManipulator(moveManipObj);
    }
  }

  MString pythonSelectionCallback;
  pythonSelectionCallback.format("mannequinSelectionChanged(\"^1s\", \"^2s\")",
    _selection.fullPathName(),
    JointPresentationStyle::toString(_selectionStyle));
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

int MannequinContext::selectionStyle() const {
  return _selectionStyle;
}

void MannequinContext::calculateDagLookupTables(MObject skinObj) {
  MFnSkinCluster skin(skinObj);
  MDagPathArray influenceObjects;
  unsigned int numInfluences = skin.influenceObjects(influenceObjects);

  for (int i = 0; i < numInfluences; ++i) {
    MDagPath dagPath = influenceObjects[i];
    _dagIndexLookup[dagPath] = i;
    _dagStyleLookup[dagPath] = dagPath.childCount() == 0 ?
#ifdef TERMINAL_JOINTS_ROTATE
      JointPresentationStyle::TRANSLATE | JointPresentationStyle::ROTATE :
      JointPresentationStyle::ROTATE;
#else
      JointPresentationStyle::TRANSLATE : JointPresentationStyle::ROTATE;
#endif
  }
}

void MannequinContext::calculateMaxInfluences(MDagPath dagPath,
  MObject skinObj) {
  MFnMesh mesh(dagPath);
  MFnSkinCluster skin(skinObj);

  MFnSingleIndexedComponent comp;
  MObject compObj = comp.create(MFn::kMeshVertComponent);
  comp.setCompleteData(mesh.numVertices());

  MDoubleArray weights;
  unsigned int numInfluences;
  skin.getWeights(dagPath, compObj, weights, numInfluences);

  _maxInfluences.resize(mesh.numPolygons());

  int i = 0;
  for (MItMeshPolygon it(dagPath); !it.isDone(); it.next()) {
    MIntArray polyVertices;
    it.getVertices(polyVertices);

    std::vector<double> weightSums(numInfluences, 0.0f);
    for (int vtx = 0; vtx < polyVertices.length(); ++vtx) {
      int vtxId = polyVertices[vtx];
      for (int influence = 0; influence < numInfluences; ++influence) {
        weightSums[influence] += weights[vtxId * numInfluences + influence];
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

    _maxInfluences[i++] = maxIndex;
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
  if (_autoAdjust && _autoAdjust.value() && jointDagPath.isValid()) {
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

bool MannequinContext::intersectManip(MPxManipulatorNode* manip) {
  if (_rotateManip) {
    MPoint linePoint;
    MVector lineDirection;
    manip->mouseRayWorld(linePoint, lineDirection);

    MStatus err;
    MFnTransform selectionXform(_selection);
    MPoint selectionPivot = selectionXform.rotatePivot(MSpace::kWorld, &err);

    // Extend manipulator radius a bit because of the free-rotation "shell".
    float manipRadius = manipAdjustedScale() * MFnManip3D::globalSize() * 1.25f;

    bool hit;
    float distanceOut;
    hit = Util::raySphereIntersection(linePoint,
      lineDirection,
      selectionPivot,
      manipRadius,
      &distanceOut);

    if (hit) {
      return true;
    }
  }

  if (_moveManip) {
    bool hit = _moveManip->intersectManip(manip);
    if (hit) {
      return true;
    }
  }

  return false;
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

  if (_rotateManip || _moveManip) {
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

  if (_rotateManip || _moveManip) {
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
  setImage("mannequin_maya2016.png", MPxContext::kImage1);
  setTitleString("Mannequin");
  updateText();
}

void MannequinContext::toolOffCleanup() {
  select(MDagPath());

  _mannequinManip = nullptr;
  _rotateManip = nullptr;
  _moveManip = nullptr;

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
  return doPress();
}

MStatus MannequinContext::doPress(MEvent& event) {
  return doPress();
}

MStatus MannequinContext::doPress() {
  if (!_mannequinManip) {
    return MS::kUnknownParameter;
  }

  select(_mannequinManip->highlightedDagPath());
  return MS::kSuccess;
}

void MannequinContext::abortAction() {
  select(MDagPath());
}

void MannequinContext::completeAction() {
  if (_selection.isValid() && _selectionStyle != _availableStyles) {
    if (_selectionStyle == JointPresentationStyle::ROTATE) {
      select(_selection, JointPresentationStyle::TRANSLATE);
    } else if (_selectionStyle == JointPresentationStyle::TRANSLATE) {
      select(_selection, JointPresentationStyle::ROTATE);
    }
  }
}

void MannequinContext::updateText() {
  if (_selection.isValid() && _selectionStyle != _availableStyles) {
    MString next;
    if (_selectionStyle == JointPresentationStyle::ROTATE) {
      next = "translation";
    } else if (_selectionStyle == JointPresentationStyle::TRANSLATE) {
      next = "rotation";
    } else {
      next = "???";
    }

    MString help;
    help.format(
      "^1s selected. Press ESC to deselect. Press ENTER to switch to ^2s.",
      _selection.partialPathName(),
      next);
    setHelpString(help);
  } else if (_selection.isValid()) {
    MString help;
    help.format(
      "^1s selected. Press ESC to deselect.",
      _selection.partialPathName());
    setHelpString(help);
  } else {
    setHelpString("Click on the mesh to select a part.");
  }
}

MannequinContextCommand::MannequinContextCommand()
  : _mannequinContext(nullptr) {}

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
    MString nameArg = parse.flagArgumentString("-sel", 0, &err);
    if (err.error()) {
      return err;
    }

    MString styleArg = parse.flagArgumentString("-sel", 1, &err);
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

      if (nameArg == fullName || nameArg == partialName) {
        _mannequinContext->select(influenceObjects[i],
          JointPresentationStyle::fromString(styleArg));
        return MS::kSuccess;
      }
    }

    MString errMessage;
    errMessage.format("Couldn't find and select ^1s", nameArg);
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
      result += " ";
      result += JointPresentationStyle::toString(style);
    }

    setResult(result);
  } else if (parse.isFlagSet("-sel")) {
    MDagPath dagPath = _mannequinContext->selectionDagPath();
    MString result;
    if (dagPath.isValid()) {
      result = MString(dagPath.fullPathName());
      result += " ";
      result += _mannequinContext->selectionStyle();
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
  syn.addFlag("-sel", "-selection", MSyntax::kString, MSyntax::kString);
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
    MannequinMoveManipulator::id,
    &MannequinMoveManipulator::creator,
    &MannequinMoveManipulator::initialize,
    MPxNode::kManipulatorNode);

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
  status = plugin.deregisterNode(MannequinMoveManipulator::id);

  return status;
}
