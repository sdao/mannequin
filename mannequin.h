#pragma once

#include <maya/MPxContext.h>
#include <maya/MPxContextCommand.h>
#include <maya/MEvent.h>
#include <maya/MDagPath.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>

class MannequinManipulator;

class MannequinContext : public MPxContext {
public:
  MannequinContext();
  ~MannequinContext();
  void forceExit();
  void select(const MDagPath& dagPath);
  void reselect();
  MDagPath selectionDagPath() const;
  void calculateMaxInfluences(MDagPath meshDagPath, MObject skinObject);
  const unsigned int* maxInfluences() const;
  MDagPath meshDagPath() const;
  MObject skinObject() const;
  bool addMannequinManipulator(MDagPath newHighlight = MDagPath());
  bool intersectRotateManip(MPoint linePoint,
    MVector lineDirection,
    float* distanceOut);
  double manipScale() const;
  void setManipScale(double scale);

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
  static constexpr double MANIP_DEFAULT_SCALE = 5.0;

  MDagPath _meshDagPath;
  MObject _skinObject;
  unsigned int* _maxInfluences;

  MDagPath _selection;
  MannequinManipulator* _mannequinManip;
  MObject _rotateManip;
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
