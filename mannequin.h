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
#include <maya/MPxManipulatorNode.h>
#include <maya/MCallbackIdArray.h>

#include <boost/optional.hpp>

#include "stdext.h"

class MannequinManipulator;
class MannequinMoveManipulator;

namespace JointPresentationStyle {
  const int NONE = 0;
  const int ROTATE = 1 << 1;
  const int TRANSLATE = 1 << 2;

  inline MString toString(int style) {
    MString result = "";
    result += (style & JointPresentationStyle::ROTATE) ? "r" : "";
    result += (style & JointPresentationStyle::TRANSLATE) ? "t" : "";
    return result;
  }

  inline int fromString(MString string) {
    int result = JointPresentationStyle::NONE;
    if (string.index('r') != -1) {
      result |= JointPresentationStyle::ROTATE;
    }
    if (string.index('t') != -1) {
      result |= JointPresentationStyle::TRANSLATE;
    }
    return result;
  }
}

class MannequinContext : public MPxContext {
public:
  MannequinContext();
  void forceExit();
  void select(const MDagPath& dagPath, int style =
    JointPresentationStyle::NONE);
  void reselect();
  MDagPath selectionDagPath() const;
  int selectionStyle() const;
  void calculateDagLookupTables(MObject skinObj);
  void calculateMaxInfluences(MDagPath meshDagPath, MObject skinObject);
  void calculateLongestJoint(MObject skinObject);
  void calculateJointLengthRatio(MDagPath jointDagPath);
  const std::vector<int>& maxInfluences() const;
  MDagPath meshDagPath() const;
  MObject skinObject() const;
  bool addMannequinManipulator(MDagPath newHighlight = MDagPath());
  bool intersectManip(MPxManipulatorNode* manip);
  double manipScale() const;
  void setManipScale(double scale);
  bool manipAutoAdjust() const;
  void setManipAutoAdjust(bool autoAdjust);
  float manipAdjustedScale() const;
  int influenceIndexForJointDagPath(const MDagPath& dagPath);
  int presentationStyleForJointDagPath(const MDagPath& dagPath) const;
  void updateText();

  virtual void toolOnSetup(MEvent& event) override;
  virtual void toolOffCleanup() override;
  virtual void getClassName(MString& name) const override;
  virtual void abortAction() override;
  virtual void completeAction() override;

  virtual MStatus doPress(MEvent& event,
    MHWRender::MUIDrawManager& drawMgr,
    const MHWRender::MFrameContext& context);
  virtual MStatus doPress(MEvent& event);
  MStatus doPress();

  static void keyframeCallback(bool* retCode, MPlug& plug, void* clientData);

private:
  static const double MANIP_DEFAULT_SCALE;
  static const double MANIP_ADJUSTMENT;

  MDagPath _meshDagPath;
  MObject _skinObject;
  std::vector<int> _maxInfluences;
  std::map<MDagPath, int> _dagIndexLookup;
  std::map<MDagPath, int> _dagStyleLookup;

  MDagPath _selection;
  int _selectionStyle;
  int _availableStyles;

  MannequinManipulator* _mannequinManip;
  void* _rotateManip;
  MannequinMoveManipulator* _moveManip;

  mutable boost::optional<double> _scale;
  mutable boost::optional<bool> _autoAdjust;
  double _longestJoint;
  double _jointLengthRatio;

  MCallbackIdArray _callbacks;
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
