#include "chartreuse.h"
#include "chartreuse_manipulator.h"

#include <maya/MStatus.h>
#include <maya/MFnPlugin.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MFnMesh.h>
#include <maya/MFnRotateManip.h>

ChartreuseContext::ChartreuseContext() {}

void ChartreuseContext::forceExit() {
  MGlobal::executeCommand("setToolTo $gSelect");
}

void ChartreuseContext::select(const MDagPath& dagPath) {
  _selection = dagPath;

  MFnRotateManip rotateManip(_rotateManip);
  if (_selection.hasFn(MFn::kTransform)) {
    rotateManip.setManipScale(ROTATE_MANIP_SCALE);
    rotateManip.setVisible(true);
    rotateManip.displayWithNode(_selection.node());
  } else {
    rotateManip.setManipScale(0.001f);
    rotateManip.setVisible(false);
  }
}

MDagPath ChartreuseContext::selectionDagPath() const {
  return _selection;
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
  MObject skinObject;
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
        skinObject = node;
      }
    }
  }

  if (!hasSkinCluster) {
    MGlobal::displayError("Selection has no smooth skin bound");
    forceExit();
    return;
  }

  MObject chartreuseManipObj;
  MStatus err;
  _chartreuseManip = (ChartreuseManipulator*)MPxManipulatorNode::newManipulator(
    "ChartreuseManipulator", chartreuseManipObj, &err);

  if (err.error()) {
    MGlobal::displayError("Could not create manipulator");
    forceExit();
    return;
  }

  _chartreuseManip->setup(this, dagPath, skinObject);
  addManipulator(chartreuseManipObj);

  MFnRotateManip rotateManip;
  _rotateManip = rotateManip.create();
  rotateManip.setManipScale(ROTATE_MANIP_SCALE);
  rotateManip.setVisible(false);
  addManipulator(_rotateManip);

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
