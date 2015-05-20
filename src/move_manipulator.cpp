#include "move_manipulator.h"
#include "util.h"

#include <limits>

#include <maya/MFnDependencyNode.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnManip3D.h>
#include <maya/MGLFunctionTable.h>
#include <maya/MHardwareRenderer.h>
#include <maya/MAngle.h>
#include <maya/MQuaternion.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

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
  _origin.get(origin);

  float x[4], y[4], z[4];
  (_origin + (_x * size)).get(x);
  (_origin + (_y * size)).get(y);
  (_origin + (_z * size)).get(z);

  view.beginGL();

  GLUquadricObj* quadric = gluNewQuadric();
  gluQuadricNormals(quadric, GLU_SMOOTH);
  gluQuadricTexture(quadric, true);
  gluQuadricDrawStyle(quadric, GLU_FILL);

  colorAndName(view, _glPickableItem + 0, true, xColor());
  gGLFT->glBegin(MGL_LINES);
    gGLFT->glVertex3fv(origin);
    gGLFT->glVertex3fv(x);
  gGLFT->glEnd();
  glDrawCone(quadric, _origin + (_x * handleOfs), _x, handleHeight,
    handleRadius);

  colorAndName(view, _glPickableItem + 1, true, yColor());
  gGLFT->glBegin(MGL_LINES);
    gGLFT->glVertex3fv(origin);
    gGLFT->glVertex3fv(y);
  gGLFT->glEnd();
  glDrawCone(quadric, _origin + (_y * handleOfs), _y, handleHeight,
    handleRadius);

  colorAndName(view, _glPickableItem + 2, true, zColor());
  gGLFT->glBegin(MGL_LINES);
    gGLFT->glVertex3fv(origin);
    gGLFT->glVertex3fv(z);
  gGLFT->glEnd();
  glDrawCone(quadric, _origin + (_z * handleOfs), _z, handleHeight,
    handleRadius);

  gluDeleteQuadric(quadric);

  view.endGL();
}

void MannequinMoveManipulator::glDrawCone(GLUquadricObj* quadric,
  MPoint pos,
  MVector dir,
  float height,
  float radius) const {
  MQuaternion zToDir = MVector::zAxis.rotateTo(dir);

  MVector axis;
  double rotateRad;
  zToDir.getAxisAngle(axis, rotateRad);

  double rotateDeg = MAngle(rotateRad).as(MAngle::kDegrees);

  glPushMatrix();
    glTranslated(pos.x, pos.y, pos.z);
    glRotated(rotateDeg, axis.x, axis.y, axis.z);
    gluCylinder(quadric, radius, 0.0, height, 8, 1);
  glPopMatrix();
}

void MannequinMoveManipulator::preDrawUI(const M3dView &view) {
  recalcMetrics();

  _xColor = xColor();
  _yColor = yColor();
  _zColor = zColor();
  _selColor = selectedColor();

  _selected[0] = shouldDrawHandleAsSelected(0);
  _selected[1] = shouldDrawHandleAsSelected(1);
  _selected[2] = shouldDrawHandleAsSelected(2);
}

void MannequinMoveManipulator::drawUI(MHWRender::MUIDrawManager &drawManager,
  const MHWRender::MFrameContext &frameContext) const {
  float size = _manipScale * MFnManip3D::globalSize();
  float handleSize = MFnManip3D::handleSize() / 100.0f; // Probably on [0, 100].
  float handleHeight = size * handleSize * 0.5f;
  float handleOfs = size - handleHeight;
  float handleRadius = handleHeight * 0.25f;

  beginDrawable(drawManager, _glPickableItem + 0, true);
  drawManager.setLineWidth(MFnManip3D::lineSize());
  drawManager.setColorIndex(_selected[0] ? _selColor : _xColor);
  drawManager.line(_origin, _origin + (_x * size));
  drawManager.cone(_origin + (_x * handleOfs), _x, handleRadius, handleHeight,
    true);
  drawManager.endDrawable();

  beginDrawable(drawManager, _glPickableItem + 1, true);
  drawManager.setLineWidth(MFnManip3D::lineSize());
  drawManager.setColorIndex(_selected[1] ? _selColor : _yColor);
  drawManager.line(_origin, _origin + (_y * size));
  drawManager.cone(_origin + (_y * handleOfs), _y, handleRadius, handleHeight,
    true);
  drawManager.endDrawable();

  beginDrawable(drawManager, _glPickableItem + 2, true);
  drawManager.setLineWidth(MFnManip3D::lineSize());
  drawManager.setColorIndex(_selected[2] ? _selColor : _zColor);
  drawManager.line(_origin, _origin + (_z * size));
  drawManager.cone(_origin + (_z * handleOfs), _z, handleRadius, handleHeight,
    true);
  drawManager.endDrawable();
};

void MannequinMoveManipulator::beginDrawable(
  MHWRender::MUIDrawManager &drawManager,
  unsigned int name,
  bool pickable) const {
#if MAYA_API_VERSION >= 201600
  drawManager.beginDrawable(name, pickable);
#else
  drawManager.beginDrawable();
#endif
}

bool MannequinMoveManipulator::shouldDrawHandleAsSelected(int axis) {
#if MAYA_API_VERSION >= 201600
  bool result;
  MPxManipulatorNode::shouldDrawHandleAsSelected(
    _glPickableItem + axis,
    result);

  return result;
#else
  GLuint activeAxis;
  glActiveName(activeAxis);

  return activeAxis == _glPickableItem + axis;
#endif
}

MStatus MannequinMoveManipulator::doPress(M3dView& view) {
  getPointValue(_translateIndex, false, _opValueBegin);

  GLuint activeAxis;
  glActiveName(activeAxis);

  if (activeAxis == _glPickableItem + 0) {
    _opAxis = _x;
    _opAxisIndex = 0;
  } else if (activeAxis == _glPickableItem + 1) {
    _opAxis = _y;
    _opAxisIndex = 1;
  } else if (activeAxis == _glPickableItem + 2) {
    _opAxis = _z;
    _opAxisIndex = 2;
  } else {
    _opAxis = MVector::zero;
    _opValid = false;
    return MS::kUnknownParameter;
  }

  _opOrigin = _origin;

  // Determine the translation "plane"; it is orthogonal to the axis and faces
  // the view as best as possible.
  short originX, originY;
  view.worldToView(_opOrigin, originX, originY);

  MPoint rayNear;
  MVector dirToOrigin;
  view.viewToWorld(originX, originY, rayNear, dirToOrigin);

  MVector dirInPlane = dirToOrigin ^ _opAxis;
  _opPlaneNormal = dirInPlane ^ _opAxis;

  // Determine where the current mouse ray hits the plane.
  MPoint rayOrigin;
  MVector rayDirection;
  mouseRayWorld(rayOrigin, rayDirection);

  MPoint isect;
  bool didIsect = Util::rayPlaneIntersection(rayOrigin,
                                             rayDirection,
                                             _opOrigin,
                                             _opPlaneNormal,
                                             &isect);

  if (!didIsect) {
    _opValid = false;
    return MS::kUnknownParameter;
  }

  _opHitBegin = isect;
  _opValid = true;

  // We need to calculate the handle directions in parent space. This is
  // because the handle positions align with the child pivot rotation, so they
  // DO NOT correspond to the child's X, Y, and Z-position, which are
  // indicated in terms of the parent's coordinate space.
  MMatrix parentInverse = _parentXform.asMatrixInverse();
  _xInParentSpace = _x * parentInverse;
  _yInParentSpace = _y * parentInverse;
  _zInParentSpace = _z * parentInverse;

  return MS::kSuccess;
}

MStatus MannequinMoveManipulator::doDrag(M3dView& view) {
  if (!_opValid) {
    return MS::kUnknownParameter;
  }

  MPoint rayOrigin;
  MVector rayDirection;
  mouseRayWorld(rayOrigin, rayDirection);

  MPoint isect;
  bool didIsect = Util::rayPlaneIntersection(rayOrigin,
                                             rayDirection,
                                             _opOrigin,
                                             _opPlaneNormal,
                                             &isect);

  if (!didIsect) {
    // Leave the point where it is. The user's probably gone past the horizon.
    return MS::kSuccess;
  }

  _opHitCurrent = isect;
  MVector diff = _opHitCurrent - _opHitBegin;

  // Now let's project diff onto the axis!
  MVector axisNormal = _opAxis.normal();
  double ofs = (diff * axisNormal) / _opAxis.length();
  _opDiffProj = (diff * axisNormal) * axisNormal;

  MPoint newTranslate;
  if (_opAxisIndex == 0) {
    newTranslate = _opValueBegin + ofs * _xInParentSpace;
  } else if (_opAxisIndex == 1) {
    newTranslate = _opValueBegin + ofs * _yInParentSpace;
  } else if (_opAxisIndex == 2) {
    newTranslate = _opValueBegin + ofs * _zInParentSpace;
  } else {
    newTranslate = _opValueBegin;
  }

  setPointValue(_translateIndex, newTranslate);
  return MS::kSuccess;
}

MStatus MannequinMoveManipulator::doRelease(M3dView& view) {
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

  MMatrix childMatrix = _childXform.asMatrix();
  _x = (MVector::xAxis * childMatrix).normal();
  _y = (MVector::yAxis * childMatrix).normal();
  _z = (MVector::zAxis * childMatrix).normal();
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
  float handleRadius = std::max(handleHeight * 0.3f, 4.0f);
  // Note: slightly exaggerated; normally handleHeight * 0.25f.

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
