#include "move_manipulator.h"
#include "util.h"

#include <limits>

#include <maya/MFnDependencyNode.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnManip3D.h>
#include <maya/MGLFunctionTable.h>
#include <maya/MHardwareRenderer.h>

const MTypeId MannequinMoveManipulator::id = MTypeId(0xcafebee);

MannequinMoveManipulator::MannequinMoveManipulator() {}

void MannequinMoveManipulator::postConstructor() {
  addPointValue("translate", MPoint(0, 0, 0), _translateIndex);
  glFirstHandle(_glPickableItem);
}

MStatus
MannequinMoveManipulator::connectToDependNode(const MObject &dependNode) {
  MStatus status;
  MFnDependencyNode nodeFn(dependNode, &status);

  if (!status)
     return MS::kFailure;

  MPlug translatePlug = nodeFn.findPlug("translate", &status);

  if (!status)
     return MS::kFailure;

  int plugIndex = 0;
  status = connectPlugToValue(translatePlug, _translateIndex, plugIndex);

  if (!status)
     return MS::kFailure;

  MFnDagNode dagNodeFn(dependNode);
  MDagPath nodePath;
  dagNodeFn.getPath(nodePath);

  MTransformationMatrix m(nodePath.exclusiveMatrix());
  _parentXform = m;

  MTransformationMatrix n(nodePath.inclusiveMatrix());
  _childXform = n;

  finishAddingManips();
  return MPxManipulatorNode::connectToDependNode(dependNode);
}

void MannequinMoveManipulator::draw(M3dView &view,
  const MDagPath &path,
  M3dView::DisplayStyle style,
  M3dView::DisplayStatus status) {
  static MGLFunctionTable *gGLFT = 0;
	if (0 == gGLFT) {
		gGLFT = MHardwareRenderer::theRenderer()->glFunctionTable();
  }

  recalcMetrics();
  float size = _manipScale * MFnManip3D::globalSize();
  float handleSize = MFnManip3D::handleSize() / 100.0f; // Probably on [0, 100].
  float handleHeight = size * handleSize * 0.5f;
  float handleOfs = size - handleHeight;
  float handleRadius = handleHeight * 0.25f;

  float origin[4];
  float x[4], y[4], z[4];
  _origin.get(origin);
  (_origin + (_x * size)).get(x);
  (_origin + (_y * size)).get(y);
  (_origin + (_z * size)).get(z);

  bool selected[3];
  shouldDrawHandleAsSelected(_glPickableItem + 0, selected[0]);
  shouldDrawHandleAsSelected(_glPickableItem + 1, selected[1]);
  shouldDrawHandleAsSelected(_glPickableItem + 2, selected[2]);

  view.beginGL();

  colorAndName(view, _glPickableItem + 0, true,
    selected[0] ? selectedColor() : xColor());
	gGLFT->glBegin(MGL_LINES);
		gGLFT->glVertex3fv(origin);
		gGLFT->glVertex3fv(x);
	gGLFT->glEnd();

  colorAndName(view, _glPickableItem + 1, true,
    selected[0] ? selectedColor() : yColor());
	gGLFT->glBegin(MGL_LINES);
		gGLFT->glVertex3fv(origin);
		gGLFT->glVertex3fv(y);
	gGLFT->glEnd();

  colorAndName(view, _glPickableItem + 2, true,
    selected[0] ? selectedColor() : zColor());
	gGLFT->glBegin(MGL_LINES);
		gGLFT->glVertex3fv(origin);
		gGLFT->glVertex3fv(z);
	gGLFT->glEnd();

  view.endGL();
}

void MannequinMoveManipulator::preDrawUI(const M3dView &view) {
  recalcMetrics();

  _xColor = xColor();
  _yColor = yColor();
  _zColor = zColor();
  _selColor = selectedColor();
}

void MannequinMoveManipulator::drawUI(MHWRender::MUIDrawManager &drawManager,
  const MHWRender::MFrameContext &frameContext) const {
  float size = _manipScale * MFnManip3D::globalSize();
  float handleSize = MFnManip3D::handleSize() / 100.0f; // Probably on [0, 100].
  float handleHeight = size * handleSize * 0.5f;
  float handleOfs = size - handleHeight;
  float handleRadius = handleHeight * 0.25f;

  bool selected[3];
  shouldDrawHandleAsSelected(_glPickableItem + 0, selected[0]);
  shouldDrawHandleAsSelected(_glPickableItem + 1, selected[1]);
  shouldDrawHandleAsSelected(_glPickableItem + 2, selected[2]);

  drawManager.beginDrawable(_glPickableItem + 0, true);
  drawManager.setLineWidth(MFnManip3D::lineSize());
  drawManager.setColorIndex(selected[0] ? _selColor : _xColor);
  drawManager.line(_origin, _origin + (_x * size));
  drawManager.cone(_origin + (_x * handleOfs), _x, handleRadius, handleHeight,
    true);
  drawManager.endDrawable();

  drawManager.beginDrawable(_glPickableItem + 1, true);
  drawManager.setLineWidth(MFnManip3D::lineSize());
  drawManager.setColorIndex(selected[1] ? _selColor : _yColor);
  drawManager.line(_origin, _origin + (_y * size));
  drawManager.cone(_origin + (_y * handleOfs), _y, handleRadius, handleHeight,
    true);
  drawManager.endDrawable();

  drawManager.beginDrawable(_glPickableItem + 2, true);
  drawManager.setLineWidth(MFnManip3D::lineSize());
  drawManager.setColorIndex(selected[2] ? _selColor : _zColor);
  drawManager.line(_origin, _origin + (_z * size));
  drawManager.cone(_origin + (_z * handleOfs), _z, handleRadius, handleHeight,
    true);
  drawManager.endDrawable();
};

MStatus MannequinMoveManipulator::doPress(M3dView& view) {
  getPointValue(_translateIndex, false, _originalTranslate);
  mousePosition(_originalMouseX, _originalMouseY);
  return MS::kSuccess;
}

MStatus MannequinMoveManipulator::doDrag(M3dView& view) {
  MPoint p(0, 0, 0);
  setPointValue(_translateIndex, p);
  std::cout << "Drag!\n";
  return MS::kSuccess;
}

MStatus MannequinMoveManipulator::doRelease(M3dView& view) {
  setPointValue(_translateIndex, _originalTranslate);
  std::cout << "Release!\n";
  return MS::kSuccess;
}

void MannequinMoveManipulator::setManipScale(float scale) {
  _manipScale = scale;
}

float MannequinMoveManipulator::manipScale() const {
  return _manipScale;
}

void MannequinMoveManipulator::recalcMetrics() {
  MPoint translate;
  getPointValue(_translateIndex, false, translate);

  _x = MVector::xAxis * _childXform.asMatrix();
  _y = MVector::yAxis * _childXform.asMatrix();
  _z = MVector::zAxis * _childXform.asMatrix();
  _origin = translate * _parentXform.asMatrix();
}

bool MannequinMoveManipulator::intersectManip(MPxManipulatorNode* manip) const {
  M3dView view = M3dView::active3dView();

  float size = _manipScale * MFnManip3D::globalSize();
  MPoint xEnd = _origin + (_x * size);
  MPoint yEnd = _origin + (_y * size);
  MPoint zEnd = _origin + (_z * size);

  short mx, my;
  manip->mousePosition(mx, my);

  short ox, oy, xx, xy, yx, yy, zx, zy;
  view.worldToView(_origin, ox, oy);
  view.worldToView(xEnd, xx, xy);
  view.worldToView(yEnd, yx, yy);
  view.worldToView(zEnd, zx, zy);

  // Calculate approximate handle size in view space.
  float viewLength = 0.0f;
  viewLength = std::max(viewLength,
    sqrtf(pow(float(xx) - float(ox), 2) + pow(float(xy) - float(oy), 2)));
  viewLength = std::max(viewLength,
    sqrtf(pow(float(yx) - float(ox), 2) + pow(float(yy) - float(oy), 2)));
  viewLength = std::max(viewLength,
    sqrtf(pow(float(zx) - float(ox), 2) + pow(float(zy) - float(oy), 2)));

  float handleSize = MFnManip3D::handleSize() / 100.0f; // Probably on [0, 100].
  float handleHeight = viewLength * handleSize * 0.5f;
  float handleRadius = std::max(handleHeight * 0.5f, 2.0f);

  // Determine if we're in range to any of the lines.
  float curDist, t;

  curDist = Util::distanceToLine(ox, oy, xx, xy, mx, my, &t);
  if (curDist < handleRadius && t >= 0.0f && t <= 1.0f) {
    return true;
  }

  curDist = Util::distanceToLine(ox, oy, yx, yy, mx, my, &t);
  if (curDist < handleRadius && t >= 0.0f && t <= 1.0f) {
    return true;
  }

  curDist = Util::distanceToLine(ox, oy, zx, zy, mx, my, &t);
  if (curDist < handleRadius && t >= 0.0f && t <= 1.0f) {
    return true;
  }

  return false;
}

void* MannequinMoveManipulator::creator() {
  return new MannequinMoveManipulator;
}

MStatus MannequinMoveManipulator::initialize() {
  return MS::kSuccess;
}
