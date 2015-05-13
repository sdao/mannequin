#pragma once

#include <vector>
#include <string>
#include <map>

#include <maya/MPxContext.h>
#include <maya/MPxContextCommand.h>
#include <maya/MEvent.h>
#include <maya/MDagPath.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>

#include <boost/optional.hpp>

#include "stdext.h"

class MannequinManipulator;
class MannequinMoveManipulator;

namespace JointPresentationStyle {
  constexpr int NONE = 0;
  constexpr int ROTATE = 1 << 1;
  constexpr int TRANSLATE = 1 << 2;
}

class MannequinContext : public MPxContext {
public:
  MannequinContext();
  void forceExit();
  void select(const MDagPath& dagPath);
  void reselect();
  MDagPath selectionDagPath() const;
  void calculateDagLookupTables(MObject skinObj);
  void calculateMaxInfluences(MDagPath meshDagPath, MObject skinObject);
  void calculateLongestJoint(MObject skinObject);
  void calculateJointLengthRatio(MDagPath jointDagPath);
  const std::vector<int>& maxInfluences() const;
  MDagPath meshDagPath() const;
  MObject skinObject() const;
  bool addMannequinManipulator(MDagPath newHighlight = MDagPath());
  bool intersectManip(MPoint linePoint, MVector lineDirection);
  double manipScale() const;
  void setManipScale(double scale);
  bool manipAutoAdjust() const;
  void setManipAutoAdjust(bool autoAdjust);
  double manipAdjustedScale() const;
  int influenceIndexForJointDagPath(const MDagPath& dagPath);
  int presentationStyleForJointDagPath(const MDagPath& dagPath) const;

  virtual void toolOnSetup(MEvent& event) override;
  virtual void toolOffCleanup() override;
  virtual void getClassName(MString& name) const override;
  virtual MStatus doPress(MEvent& event,
    MHWRender::MUIDrawManager& drawMgr,
    const MHWRender::MFrameContext& context);
  virtual MStatus doPress(MEvent& event);
  virtual void abortAction() override;
  void doPress();
  void updateText();

private:
  static constexpr double MANIP_DEFAULT_SCALE = 1.5;
  static constexpr double MANIP_ADJUSTMENT = 0.1;

  MDagPath _meshDagPath;
  MObject _skinObject;
  std::vector<int> _maxInfluences;
  std::map<MDagPath, int> _dagIndexLookup;
  std::map<MDagPath, int> _dagStyleLookup;

  MDagPath _selection;
  MannequinManipulator* _mannequinManip;
  void* _rotateManip;
  MannequinMoveManipulator* _moveManip;

  mutable boost::optional<double> _scale;
  mutable boost::optional<double> _autoAdjust;
  double _longestJoint;
  double _jointLengthRatio;
};

class MannequinContextCommand : public MPxContextCommand
{
public:
  MannequinContextCommand();
  virtual MPxContext* makeObj() override;
  static void* creator();
  virtual MStatus doEditFlags() override;
  virtual MStatus doQueryFlags() override;
  virtual MStatus appendSyntax() override;

private:
  MannequinContext* _mannequinContext;
};
