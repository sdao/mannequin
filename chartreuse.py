from maya import cmds
from maya import mel
from maya import OpenMaya as om
from maya import OpenMayaUI as ui

from PySide.QtCore import *
from PySide.QtGui import *
from PySide.QtUiTools import *

from shiboken import wrapInstance

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

        # Register panels according to DAG path.
        objectName = dagPath.partialPathName()
        self.dagPaths[objectName] = dagPath
        self.panels[objectName] = panelGui

        # Set object name.
        panelGui.groupBox.setTitle(objectName)

        # Set current object rotation.
        objectXform = om.MFnTransform(dagPath)
        rotation = om.MEulerRotation()
        objectXform.getRotation(rotation)
        self.updatePanelGui(panelGui, rotation.x, rotation.y, rotation.z)
        self.gui.layout().addWidget(panelGui)

        # Register callback for attribute update.
        callbackId = om.MNodeMessage.addNodeDirtyPlugCallback(dependNode,
            self.dirtyPlugCallback,
            None)
        self.callbacks.append(callbackId)

    def dirtyPlugCallback(self, node, plug, *args, **kwargs):
        nodeName, attrName = plug.name().split(".")
        if (attrName == "rotate"):
            if nodeName in self.panels:
                x = plug.child(0).asDouble()
                y = plug.child(1).asDouble()
                z = plug.child(2).asDouble()

                panelGui = self.panels[nodeName]
                self.updatePanelGui(panelGui, x, y, z)

    def updatePanelGui(self, panelGui, x, y, z):
        panelGui.rotateXEdit.setText(str(math.degrees(x)))
        panelGui.rotateYEdit.setText(str(math.degrees(y)))
        panelGui.rotateZEdit.setText(str(math.degrees(z)))

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
