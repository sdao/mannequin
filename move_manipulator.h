#pragma once

#include <maya/MPxManipulatorNode.h>
#include <maya/MVector.h>

class MannequinMoveManipulator : public MPxManipulatorNode {
public:
  MannequinMoveManipulator();

  void setManipScale(float scale);
  float manipScale() const;

  void recalcMetrics();
  bool active() const;

  virtual void postConstructor() override;
  virtual void draw(M3dView &view,
    const MDagPath &path,
    M3dView::DisplayStyle style,
    M3dView::DisplayStatus status) override;
  virtual void preDrawUI(const M3dView &view) override;
  virtual void drawUI(MHWRender::MUIDrawManager &drawManager,
    const MHWRender::MFrameContext &frameContext) const override;
  virtual MStatus connectToDependNode(const MObject &dependNode) override;
  virtual MStatus doMove(M3dView& view, bool& refresh) override;

  static void* creator();
  static MStatus initialize();
  static const MTypeId id;

private:
  int _translateIndex;

  MTransformationMatrix _parentXform;
  MTransformationMatrix _childXform;

  short _xColor;
  short _yColor;
  short _zColor;
  short _selColor;

  float _manipScale;
  MVector _x;
  MVector _y;
  MVector _z;
  MPoint _origin;

  int _selectedAxis;
};
