//-
// ==========================================================================
// Copyright 1995,2006,2008 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk
// license agreement provided at the time of installation or download,
// or which otherwise accompanies this software in either electronic
// or hard copy form.
// ==========================================================================
//+

#pragma once

#include <maya/MFn.h>
#include <maya/MPxNode.h>
#include <maya/MPxManipContainer.h>
#include <maya/MPxSelectionContext.h>
#include <maya/MPxContextCommand.h>
#include <maya/MModelMessage.h>
#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MItSelectionList.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>
#include <maya/MDagPath.h>
#include <maya/MManipData.h>
#include <maya/MMatrix.h>
#include <maya/MFnFreePointTriadManip.h>

class MoveManipulator : public MPxManipContainer
{
public:
  MoveManipulator();
  virtual ~MoveManipulator();

  static void * creator();
  static MStatus initialize();
  virtual MStatus createChildren();

  virtual MStatus connectToDependNode(const MObject &node);

private:
  void updateManipLocations(const MObject &node);

public:
  MDagPath fFreePointManip;
  static MTypeId id;
};


MTypeId MoveManipulator::id( 0xcafebee );

MoveManipulator::MoveManipulator()
{
  // The constructor must not call createChildren for user-defined
  // manipulators.
}


MoveManipulator::~MoveManipulator()
{
}


void *MoveManipulator::creator()
{
  return new MoveManipulator();
}


MStatus MoveManipulator::initialize()
{
  MStatus stat;
  stat = MPxManipContainer::initialize();
  return stat;
}


MStatus MoveManipulator::createChildren()
{
  MStatus stat = MStatus::kSuccess;

  fFreePointManip = addFreePointTriadManip("pointManip",
                       "freePoint");

  return stat;
}

void MoveManipulator::updateManipLocations(const MObject &node)
//
// Description
//        setTranslation and setRotation to the parent's transformation.
//
{
  MFnDagNode dagNodeFn(node);
  MDagPath nodePath;
  dagNodeFn.getPath(nodePath);

        MFnFreePointTriadManip manipFn(fFreePointManip);
  MTransformationMatrix m(nodePath.exclusiveMatrix());

  double rot[3];
  MTransformationMatrix::RotationOrder rOrder;
  m.getRotation(rot, rOrder, MSpace::kWorld);
  manipFn.setRotation(rot, rOrder);

  MVector trans = m.getTranslation(MSpace::kWorld);
  manipFn.setTranslation(trans, MSpace::kWorld);
}

MStatus MoveManipulator::connectToDependNode(const MObject &node)
{
  MStatus stat;

  //
  // This routine connects the distance manip to the scaleY plug on the node
  // and connects the translate plug to the position plug on the freePoint
  // manipulator.
  //

  MFnDependencyNode nodeFn(node);
  MPlug tPlug = nodeFn.findPlug("translate", &stat);

  MFnFreePointTriadManip freePointManipFn(fFreePointManip);
  freePointManipFn.connectToPointPlug(tPlug);

  updateManipLocations(node);

  finishAddingManips();
  MPxManipContainer::connectToDependNode(node);
  return stat;
}
