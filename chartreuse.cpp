#include "chartreuse.h"
#include "chartreuse_manipulator.h"

#include <maya/MStatus.h>
#include <maya/MFnPlugin.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MFnMesh.h>

ChartreuseContext::ChartreuseContext() {}

void ChartreuseContext::toolOnSetup(MEvent& event) {
  MSelectionList list;
  MGlobal::getActiveSelectionList(list);

  if (list.length() == 0) {
    MGlobal::displayError("Nothing selected");
    return;
  }

  MDagPath dagPath;
  list.getDagPath(0, dagPath);
  dagPath.extendToShape();

  if (!dagPath.hasFn(MFn::kMesh)) {
    MGlobal::displayError("Selection is not a mesh");
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
    return;
  }

  MObject manipObj;
  MStatus err;
  ChartreuseManipulator* manip = (ChartreuseManipulator*)
    MPxManipulatorNode::newManipulator("chartreuseManipulator", manipObj, &err);

  if (err.error()) {
    MGlobal::displayError("Could not create manipulator");
    return;
  }

  manip->setup(dagPath, skinObject);
  addManipulator(manipObj);
  MGlobal::clearSelectionList();
}

void ChartreuseContext::toolOffCleanup() {
  deleteManipulators();
}

void ChartreuseContext::getClassName(MString& name) const {
  // Note: when setToolTo is called from MEL, Maya will try to load
  // chartreuseContextProperties and chartreuseContextValues.
  name.set("chartreuseContext");
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

  status = plugin.registerNode("chartreuseManipulator",
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
