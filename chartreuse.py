from maya import cmds
from maya import mel
from maya import OpenMaya as om
from maya import OpenMayaUI as ui

from PySide.QtCore import *
from PySide.QtGui import *
from PySide.QtUiTools import *

from shiboken import wrapInstance
from functools import partial

import os
import math

class ChartreuseToolPanel(QObject):
    def __init__(self):
        super(ChartreuseToolPanel, self).__init__()
        self.reset(None, None)

    def reset(self, parent, gui):
        try:
            for x in self.callbacks:
                om.MNodeMessage.removeCallback(x)
            self.callbacks = []
        except:
            self.callbacks = []

        self.parent = parent
        self.gui = gui
        self.dagPaths = {}
        self.panels = {}
        self.updateQueue = []

        if self.gui == None:
            self.validator = None
        else:
            self.validator = QDoubleValidator(gui)
            self.validator.setDecimals(3)

    def eventFilter(self, widget, event):
        if (event.type() == QEvent.Resize and self.gui != None):
            geometry = self.gui.geometry()
            geometry.setWidth(self.parent.width())
            self.gui.setGeometry(geometry)
            return True
        return QWidget.eventFilter(self, widget, event)

    def insertPanel(self, dagPath, dependNode):
        loader = QUiLoader()
        file = QFile(os.path.join(os.path.dirname(__file__), "panel_single.ui"))
        file.open(QFile.ReadOnly)
        panelGui = loader.load(file)
        file.close()

        # Setup panel validators.
        panelGui.rotateXEdit.setValidator(self.validator)
        panelGui.rotateYEdit.setValidator(self.validator)
        panelGui.rotateZEdit.setValidator(self.validator)

        # Register panels according to DAG path.
        nodeName = dagPath.partialPathName()
        self.dagPaths[nodeName] = dagPath
        self.panels[nodeName] = panelGui

        # Set object name.
        panelGui.groupBox.setTitle(nodeName)

        # Set current object rotation.
        objectXform = om.MFnTransform(dagPath)
        rotation = om.MEulerRotation()
        objectXform.getRotation(rotation)
        self.updatePanelGui(panelGui, rotation.x, rotation.y, rotation.z)

        # Register callback for attribute update.
        callbackId = om.MNodeMessage.addNodeDirtyPlugCallback(dependNode,
            self.dirtyPlugCallback,
            None)
        self.callbacks.append(callbackId)

        # Register signal for text box editing.
        panelGui.rotateXEdit.editingFinished.connect(partial(self.setRotation,
            dagPath = dagPath, index = 0))
        panelGui.rotateYEdit.editingFinished.connect(partial(self.setRotation,
            dagPath = dagPath, index = 1))
        panelGui.rotateZEdit.editingFinished.connect(partial(self.setRotation,
            dagPath = dagPath, index = 2))

        # Finally add widget to tree.
        self.gui.layout().addWidget(panelGui)

    def dirtyPlugCallback(self, node, plug, *args, **kwargs):
        nodeName, attrName = plug.name().split(".")

        if not nodeName in self.panels:
            return

        x = None
        y = None
        z = None

        if attrName[:6] == "rotate":
            self.updateQueue.append(nodeName)
            if len(self.updateQueue) == 1:
                cmds.evalDeferred(self.deferredUpdate, low = True)

    def deferredUpdate(self):
        for nodeName in self.updateQueue:
            if not nodeName in self.dagPaths:
                continue
            dagPath = self.dagPaths[nodeName]
            objectXform = om.MFnTransform(dagPath)
            rotation = om.MEulerRotation()
            objectXform.getRotation(rotation)
            panelGui = self.panels[nodeName]
            self.updatePanelGui(panelGui, rotation.x, rotation.y, rotation.z)

        self.updateQueue = []

    def updatePanelGui(self, panelGui, x, y, z):
        """Set the panel values in internal units (probably radians)."""
        if x != None:
            xx = om.MAngle.internalToUI(x)
            panelGui.rotateXEdit.setText("{:.3f}".format(xx))

        if y != None:
            yy = om.MAngle.internalToUI(y)
            panelGui.rotateYEdit.setText("{:.3f}".format(yy))

        if z != None:
            zz = om.MAngle.internalToUI(z)
            panelGui.rotateZEdit.setText("{:.3f}".format(zz))

    def setRotation(self, dagPath, index, *args, **kwargs):
        nodeName = dagPath.partialPathName()
        panelGui = self.panels[nodeName]

        # Because I'm lazy, I'm going to use the setAttr command!
        # This means that we won't have to convert units -- they should already
        # be in the UI units, which MEL expects.
        systemUnit = om.MGlobal.executeCommandStringResult("currentUnit -q -a")
        deg = systemUnit[:3] == "deg"

        if index == 0 and panelGui.rotateXEdit.isModified():
            x = float(panelGui.rotateXEdit.text())
            command = "setAttr {0}.rotateX {1}".format(nodeName, x)
            om.MGlobal.executeCommand(command, True, True)
        elif index == 1 and panelGui.rotateYEdit.isModified():
            y = float(panelGui.rotateYEdit.text())
            command = "setAttr {0}.rotateY {1}".format(nodeName, y)
            om.MGlobal.executeCommand(command, True, True)
        elif index == 2 and panelGui.rotateZEdit.isModified():
            z = float(panelGui.rotateZEdit.text())
            command = "setAttr {0}.rotateZ {1}".format(nodeName, z)
            om.MGlobal.executeCommand(command, True, True)

        objectXform = om.MFnTransform(dagPath)
        rotation = om.MEulerRotation()
        objectXform.getRotation(rotation)
        self.updatePanelGui(panelGui, rotation.x, rotation.y, rotation.z)

chartreuseToolPanel = ChartreuseToolPanel()

def setupChartreuseUI(influenceObjectsStr):
    influenceObjects = influenceObjectsStr.split(" ")

    toolLayoutPath = cmds.toolPropertyWindow(query = True, location = True)
    toolLayoutPtr = ui.MQtUtil.findLayout(toolLayoutPath)
    chartreuseLayoutPtr = ui.MQtUtil.findLayout("chartreuseLayout",
        long(toolLayoutPtr))
    chartreuseLayout = wrapInstance(long(chartreuseLayoutPtr), QWidget)

    loader = QUiLoader()
    file = QFile(os.path.join(os.path.dirname(__file__), "chartreuse.ui"))
    file.open(QFile.ReadOnly)
    gui = loader.load(file, parentWidget = chartreuseLayout)
    file.close()

    chartreuseToolPanel.reset(chartreuseLayout, gui)

    selList = om.MSelectionList()
    for obj in influenceObjects:
        selList.add(obj)

    for i in range(selList.length()):
        dagPath = om.MDagPath()
        selList.getDagPath(i, dagPath)
        dependNode = om.MObject()
        selList.getDependNode(i, dependNode)

        chartreuseToolPanel.insertPanel(dagPath, dependNode)

    gui.layout().setAlignment(Qt.AlignTop)
    gui.setMinimumHeight(gui.sizeHint().height())
    chartreuseLayout.setMinimumHeight(gui.sizeHint().height())
    chartreuseLayout.installEventFilter(chartreuseToolPanel)
    gui.show()

def tearDownChartreuseUI():
    chartreuseToolPanel.reset(None, None)
