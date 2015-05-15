from maya import cmds
from maya import OpenMaya as om
from maya import OpenMayaUI as ui

from PySide.QtCore import *
from PySide.QtGui import *


class DragRotationWidget(QWidget):
    def __init__(self, label, dagPath, index):
        super(DragRotationWidget, self).__init__()
        self.label = label
        self.dagPath = dagPath
        self.index = index
        self.setCursor(Qt.SizeHorCursor)
        self.originalRotation = None
        self.newRotation = None
        self.originalMouseX = None

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setClipRegion(event.region())
        painter.setPen(QColor(240, 240, 240))
        painter.drawText(self.contentsRect(),
                         Qt.AlignRight | Qt.AlignVCenter,
                         self.label)

    def mouseMoveEvent(self, event):
        # Since we're not mouse tracking, this happens only when a
        # mouse button is down.
        if self.originalRotation is None or self.originalMouseX is None:
            return

        diff = float(event.globalX() - self.originalMouseX) / 60.0
        if self.index == 0:
            diffRotation = om.MEulerRotation(diff, 0, 0)
        elif self.index == 1:
            diffRotation = om.MEulerRotation(0, diff, 0)
        elif self.index == 2:
            diffRotation = om.MEulerRotation(0, 0, diff)
        else:
            diffRotation = om.MEulerRotation(0, 0, 0)

        self.newRotation = self.originalRotation + diffRotation

        objectXform = om.MFnTransform(self.dagPath)
        objectXform.setRotation(self.newRotation)

    def mousePressEvent(self, event):
        self.originalRotation = om.MEulerRotation()
        self.originalMouseX = event.globalX()
        objectXform = om.MFnTransform(self.dagPath)
        objectXform.getRotation(self.originalRotation)
        self.newRotation = self.originalRotation

    def mouseReleaseEvent(self, event):
        objectXform = om.MFnTransform(self.dagPath)
        objectXform.setRotation(self.originalRotation)

        # Argggh, have to convert between degrees and radians.
        if not self.newRotation.isEquivalent(self.originalRotation):
            nodeName = self.dagPath.partialPathName()
            if self.index == 0:
                x = om.MAngle.internalToUI(self.newRotation.x)
                cmds.setAttr("{0}.rotateX".format(nodeName), x)
            elif self.index == 1:
                y = om.MAngle.internalToUI(self.newRotation.y)
                cmds.setAttr("{0}.rotateY".format(nodeName), y)
            elif self.index == 2:
                z = om.MAngle.internalToUI(self.newRotation.z)
                cmds.setAttr("{0}.rotateZ".format(nodeName), z)

        self.originalRotation = None
        self.newRotation = None
        self.originalMouseX = None

class DragTranslationWidget(QWidget):
    def __init__(self, label, dagPath, index):
        super(DragTranslationWidget, self).__init__()
        self.label = label
        self.dagPath = dagPath
        self.index = index
        self.setCursor(Qt.SizeHorCursor)
        self.originalTranslation = None
        self.newTranslation = None
        self.originalMouseX = None

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setClipRegion(event.region())
        painter.setPen(QColor(240, 240, 240))
        painter.drawText(self.contentsRect(),
                         Qt.AlignRight | Qt.AlignVCenter,
                         self.label)

    def mouseMoveEvent(self, event):
        # Since we're not mouse tracking, this happens only when a
        # mouse button is down.
        if self.originalTranslation is None or self.originalMouseX is None:
            return

        diff = float(event.globalX() - self.originalMouseX) / 20.0
        if self.index == 0:
            diffTranslation = om.MVector(diff, 0, 0)
        elif self.index == 1:
            diffTranslation = om.MVector(0, diff, 0)
        elif self.index == 2:
            diffTranslation = om.MVector(0, 0, diff)
        else:
            diffTranslation = om.MVector(0, 0, 0)

        self.newTranslation = self.originalTranslation + diffTranslation

        objectXform = om.MFnTransform(self.dagPath)
        objectXform.setTranslation(self.newTranslation, om.MSpace.kTransform)

    def mousePressEvent(self, event):
        objectXform = om.MFnTransform(self.dagPath)
        self.originalTranslation = \
            objectXform.getTranslation(om.MSpace.kTransform)
        self.newTranslation = self.originalTranslation
        self.originalMouseX = event.globalX()

    def mouseReleaseEvent(self, event):
        objectXform = om.MFnTransform(self.dagPath)
        objectXform.setTranslation(self.originalTranslation,
            om.MSpace.kTransform)

        # Argggh, have to convert between degrees and radians.
        if not self.newTranslation.isEquivalent(self.originalTranslation):
            nodeName = self.dagPath.partialPathName()
            if self.index == 0:
                x = om.MDistance.internalToUI(self.newTranslation.x)
                cmds.setAttr("{0}.translateX".format(nodeName), x)
            elif self.index == 1:
                y = om.MDistance.internalToUI(self.newTranslation.y)
                cmds.setAttr("{0}.translateY".format(nodeName), y)
            elif self.index == 2:
                z = om.MDistance.internalToUI(self.newTranslation.z)
                cmds.setAttr("{0}.translateZ".format(nodeName), z)

        self.originalTranslation = None
        self.newTranslation = None
        self.originalMouseX = None
