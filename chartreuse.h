#pragma once

#include <maya/MPxContext.h>
#include <maya/MPxContextCommand.h>
#include <maya/MEvent.h>
#include <maya/MDagPath.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>

class ChartreuseManipulator;

class ChartreuseContext : public MPxContext {
public:
  ChartreuseContext();
  ~ChartreuseContext();
  void forceExit();
  void select(const MDagPath& dagPath);
  MDagPath selectionDagPath() const;
  void calculateMaxInfluences(MDagPath meshDagPath, MObject skinObject);
  const unsigned int* maxInfluences() const;
  MDagPath meshDagPath() const;
  MObject skinObject() const;
  bool addChartreuseManipulator(MDagPath newHighlight = MDagPath());
  bool intersectRotateManip(MPoint linePoint,
    MVector lineDirection,
    float* distanceOut);

  virtual void toolOnSetup(MEvent& event) override;
  virtual void toolOffCleanup() override;
  virtual void getClassName(MString& name) const override;
  virtual MStatus doPress(MEvent& event,
    MHWRender::MUIDrawManager& drawMgr,
    const MHWRender::MFrameContext& context);
  virtual MStatus doPress(MEvent& event);
  virtual void abortAction() override;
  void doPress();

private:
  static constexpr float ROTATE_MANIP_SCALE = 5.0f;

  MDagPath _meshDagPath;
  MObject _skinObject;
  unsigned int* _maxInfluences;

  MDagPath _selection;
  ChartreuseManipulator* _chartreuseManip;
  MObject _rotateManip;
};

class ChartreuseContextCommand : public MPxContextCommand
{
public:
  ChartreuseContextCommand();
  virtual MPxContext* makeObj() override;
  static void* creator();
};
