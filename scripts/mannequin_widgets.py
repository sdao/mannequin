from maya import cmds
from maya import mel
from maya.api import OpenMaya as om

from PySide.QtCore import *
from PySide.QtGui import *


class DragWidget(QWidget):
    def __init__(self, label, sensitivity):
        super(DragWidget, self).__init__()
        self.label = label
        self.sensitivity = sensitivity
        self.setCursor(Qt.SizeHorCursor)
        self.originalMouseX = None
        self.autoKey = False

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setClipRegion(event.region())
        painter.setPen(QColor(240, 240, 240))
        painter.drawText(self.contentsRect(),
                         Qt.AlignRight | Qt.AlignVCenter,
                         self.label)

    def mousePressEvent(self, event):
        self.originalMouseX = event.globalX()
        self.saveAutoKeyframe()
        self.beginChange()

    def mouseMoveEvent(self, event):
        # Since we're not mouse tracking, this happens only when a mouse button
        # is down.
        if self.originalMouseX is None:
            return

        diff = float(event.globalX() - self.originalMouseX) * self.sensitivity
        self.change(diff)

    def mouseReleaseEvent(self, event):
        if self.originalMouseX is None:
            return

        self.originalMouseX = None
        self.restoreAutoKeyframe()
        self.finalizeChange()

    def beginChange(self):
        pass

    def change(self, diff):
        pass

    def finalizeChange(self):
        pass

    def saveAutoKeyframe(self):
        currentContext = cmds.currentCtx()
        self.autoKey = cmds.mannequinContext(currentContext, e=True, sak=True)

    def restoreAutoKeyframe(self):
        currentContext = cmds.currentCtx()
        cmds.mannequinContext(currentContext, e=True, rak=self.autoKey)


class DragRotationWidget(DragWidget):
    def __init__(self, label, dagPath, index):
        super(DragRotationWidget, self).__init__(label, 1.0 / 60.0)
        self.dagPath = dagPath
        self.index = index
        self.originalRotation = None
        self.newRotation = None

    def beginChange(self):
        objectXform = om.MFnTransform(self.dagPath)
        self.originalRotation = objectXform.rotation()
        self.newRotation = self.originalRotation

    def change(self, diff):
        if self.originalRotation is None:
            return

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
        objectXform.setRotation(self.newRotation, om.MSpace.kTransform)

    def finalizeChange(self):
        objectXform = om.MFnTransform(self.dagPath)
        objectXform.setRotation(self.originalRotation, om.MSpace.kTransform)

        # Argggh, have to convert between degrees and radians.
        if not self.newRotation.isEquivalent(self.originalRotation):
            nodeName = self.dagPath.partialPathName()
            if self.index == 0:
                x = om.MAngle.internalToUI(self.newRotation.x)
                mel.eval("setAttr {0}.rotateX {1}".format(nodeName, x))
            elif self.index == 1:
                y = om.MAngle.internalToUI(self.newRotation.y)
                mel.eval("setAttr {0}.rotateY {1}".format(nodeName, y))
            elif self.index == 2:
                z = om.MAngle.internalToUI(self.newRotation.z)
                mel.eval("setAttr {0}.rotateZ {1}".format(nodeName, z))

        self.originalRotation = None
        self.newRotation = None


class DragTranslationWidget(DragWidget):
    def __init__(self, label, dagPath, index):
        super(DragTranslationWidget, self).__init__(label, 1.0 / 20.0)
        self.dagPath = dagPath
        self.index = index
        self.originalTranslation = None
        self.newTranslation = None

    def beginChange(self):
        objectXform = om.MFnTransform(self.dagPath)
        self.originalTranslation = objectXform.translation(om.MSpace.kTransform)
        self.newTranslation = self.originalTranslation

    def change(self, diff):
        if self.originalTranslation is None:
            return

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

    def finalizeChange(self):
        objectXform = om.MFnTransform(self.dagPath)
        objectXform.setTranslation(self.originalTranslation,
                                   om.MSpace.kTransform)

        # Argggh, have to convert between cms and display distance units.
        if not self.newTranslation.isEquivalent(self.originalTranslation):
            nodeName = self.dagPath.partialPathName()
            if self.index == 0:
                x = om.MDistance.internalToUI(self.newTranslation.x)
                mel.eval("setAttr {0}.translateX {1}".format(nodeName, x))
            elif self.index == 1:
                y = om.MDistance.internalToUI(self.newTranslation.y)
                mel.eval("setAttr {0}.translateY {1}".format(nodeName, y))
            elif self.index == 2:
                z = om.MDistance.internalToUI(self.newTranslation.z)
                mel.eval("setAttr {0}.translateZ {1}".format(nodeName, z))

        self.originalTranslation = None
        self.newTranslation = None
