#pragma once

#include <maya/MPxContext.h>
#include <maya/MPxContextCommand.h>
#include <maya/MEvent.h>
#include <maya/MDagPath.h>

class ChartreuseManipulator;

class ChartreuseContext : public MPxContext {
public:
  ChartreuseContext();
  void forceExit();
  void select(const MDagPath& dagPath);
  MDagPath selectionDagPath() const;

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
