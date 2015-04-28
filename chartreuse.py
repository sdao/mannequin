from maya import cmds
from maya import mel
from maya import OpenMaya as om
from maya import OpenMayaUI as ui

from PySide.QtCore import *
from PySide.QtGui import *
from PySide.QtUiTools import *
from shiboken import wrapInstance

from chartreuse_style import ChartreuseStylesheets

import os
import math
from functools import partial
from collections import OrderedDict


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

        if self.gui is None:
            self.validator = None
        else:
            self.validator = QDoubleValidator(gui)
            self.validator.setDecimals(3)

    def eventFilter(self, widget, event):
        if (event.type() == QEvent.Resize and self.gui is not None):
            geometry = self.gui.geometry()
            geometry.setWidth(self.parent.width())
            self.gui.setGeometry(geometry)
            return True
        return QWidget.eventFilter(self, widget, event)

    def layoutJointDisplay(self, jointDisplay):
        loader = QUiLoader()
        file = QFile(os.path.join(os.path.dirname(__file__), "panel_double.ui"))
        file.open(QFile.ReadOnly)
        container = loader.load(file)
        file.close()

        # Add child panels.
        color = jointDisplay[1]
        for joint in jointDisplay[0]:
            dagPath = joint[0]
            dependNode = joint[1]
            self.insertJointDisplayPanel(dagPath, dependNode, color, container)

        # Add container to UI tree.
        self.gui.layout().addWidget(container)

    def insertJointDisplayPanel(self, dagPath, dependNode, color, container):
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

        # Set object color and name.
        if color % 3 == 0:
            panelGui.groupBox.setStyleSheet(ChartreuseStylesheets.STYLE_BLUE)
        elif color % 3 == 1:
            panelGui.groupBox.setStyleSheet(ChartreuseStylesheets.STYLE_GREEN)
        else:
            panelGui.groupBox.setStyleSheet(ChartreuseStylesheets.STYLE_RED)
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
        panelGui.rotateXEdit.editingFinished.connect(
            partial(self.setRotation, dagPath=dagPath, index=0))
        panelGui.rotateYEdit.editingFinished.connect(
            partial(self.setRotation, dagPath=dagPath, index=1))
        panelGui.rotateZEdit.editingFinished.connect(
            partial(self.setRotation, dagPath=dagPath, index=2))

        # Finally add widget to container.
        container.layout().addWidget(panelGui)

    def dirtyPlugCallback(self, node, plug, *args, **kwargs):
        nodeName, attrName = plug.name().split(".")

        if nodeName not in self.panels:
            return

        x = None
        y = None
        z = None

        if attrName[:6] == "rotate":
            self.updateQueue.append(nodeName)
            if len(self.updateQueue) == 1:
                cmds.evalDeferred(self.deferredUpdate, low=True)

    def deferredUpdate(self):
        for nodeName in self.updateQueue:
            if nodeName not in self.dagPaths:
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
        if x is not None:
            xx = om.MAngle.internalToUI(x)
            panelGui.rotateXEdit.setText("{:.3f}".format(xx))

        if y is not None:
            yy = om.MAngle.internalToUI(y)
            panelGui.rotateYEdit.setText("{:.3f}".format(yy))

        if z is not None:
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

# Singleton object, global state.
chartreuseToolPanel = ChartreuseToolPanel()


def setupChartreuseUI(influenceObjectsStr):
    influenceObjects = influenceObjectsStr.split(" ")

    toolLayoutPath = cmds.toolPropertyWindow(query=True, location=True)
    toolLayoutPtr = ui.MQtUtil.findLayout(toolLayoutPath)
    chartreuseLayoutPtr = ui.MQtUtil.findLayout("chartreuseLayout",
                                                long(toolLayoutPtr))
    chartreuseLayout = wrapInstance(long(chartreuseLayoutPtr), QWidget)

    loader = QUiLoader()
    file = QFile(os.path.join(os.path.dirname(__file__), "chartreuse.ui"))
    file.open(QFile.ReadOnly)
    gui = loader.load(file, parentWidget=chartreuseLayout)
    file.close()

    chartreuseToolPanel.reset(chartreuseLayout, gui)

    selList = om.MSelectionList()
    for obj in influenceObjects:
        selList.add(obj)

    joints = []
    for i in range(selList.length()):
        dagPath = om.MDagPath()
        selList.getDagPath(i, dagPath)
        dependNode = om.MObject()
        selList.getDependNode(i, dependNode)
        joints.append((dagPath, dependNode))

    jointDisplays = organizeJoints(joints)
    for jointDisplay in jointDisplays:
        chartreuseToolPanel.layoutJointDisplay(jointDisplay)

    gui.layout().setAlignment(Qt.AlignTop)
    gui.setMinimumHeight(gui.sizeHint().height())
    chartreuseLayout.setMinimumHeight(gui.sizeHint().height())
    chartreuseLayout.installEventFilter(chartreuseToolPanel)
    gui.show()


def organizeJoints(joints):
    """Applies a heuristic for organizing joints for display.

    The heuristic looks for occurences of "left", "right", "l", and "r" in the
    joint names for forming left-right pairs of joints.

    It uses Maya joint labelling or body part names to determine how the joint
    displays should be organized.
    """
    def stripName(n):
        n = n.lower()
        n = n.replace("left", "")
        n = n.replace("right", "")
        n = n.replace("l", "")
        n = n.replace("r", "")
        return n

    def leftHeuristic(n):
        n = n.lower()
        count = 0
        count += n.count("l")
        count += 10 * n.count("left")
        return count

    def rightHeuristic(n):
        n = n.lower()
        count = 0
        count += n.count("r")
        count += 10 * n.count("right")
        return count

    def armHeuristic(n):
        n = n.lower()
        count = 0
        count += n.count("shoulder")
        count += n.count("elbow")
        count += n.count("arm")
        count += n.count("wrist")
        count += n.count("hand")
        count += n.count("finger")
        count += n.count("thumb")
        count += n.count("index")
        count += n.count("middle")
        count += n.count("ring")
        count += n.count("pinky")
        return count

    def legHeuristic(n):
        n = n.lower()
        count = 0
        count += n.count("knee")
        count += n.count("leg")
        count += n.count("ankle")
        count += n.count("foot")
        count += n.count("toe")
        return count

    # Combine joints with similar names.
    strippedJointNames = [stripName(x[0].partialPathName()) for x in joints]
    jointGroups = OrderedDict()
    for i in range(len(strippedJointNames)):
        strippedName = strippedJointNames[i]
        joint = joints[i]

        if strippedName not in jointGroups:
            jointGroups[strippedName] = []

        jointGroups[strippedName].append(joint)

    # Sort everything into pairs if possible.
    jointPairs = []
    for i in jointGroups:
        group = jointGroups[i]

        # If group is not length 2, then just insert as single panels.
        if len(group) != 2:
            jointPairs += [[x] for x in group]
            continue

        # Determine if the group is a true left-right pair using Maya's
        # joint labels. If possible, insert a left-right double panel.
        # side: 0=center, 1=left, 2=right, 3=none
        dependNode0 = om.MFnDependencyNode(group[0][1])
        dependNode1 = om.MFnDependencyNode(group[1][1])
        attr0 = dependNode0.attribute("side")
        attr1 = dependNode1.attribute("side")
        plug0 = dependNode0.findPlug(attr0, True)
        plug1 = dependNode1.findPlug(attr1, True)
        side0 = plug0.asInt()
        side1 = plug1.asInt()
        if side0 == 1 and side1 == 2:  # left, right
            jointPairs.append([group[0], group[1]])
            continue
        elif side0 == 2 and side1 == 1:  # right, left
            jointPairs.append([group[1], group[0]])
            continue

        # If joint labels are not conclusive, then try to use a heuristic
        # based on the joint names.
        lfHeuristic0 = leftHeuristic(group[0][0].partialPathName())
        lfHeuristic1 = leftHeuristic(group[1][0].partialPathName())
        rtHeuristic0 = rightHeuristic(group[0][0].partialPathName())
        rtHeuristic1 = rightHeuristic(group[1][0].partialPathName())

        if lfHeuristic0 >= lfHeuristic1 and rtHeuristic1 >= rtHeuristic0:
            jointPairs.append([group[0], group[1]])
            continue
        elif lfHeuristic0 <= lfHeuristic1 and rtHeuristic1 <= rtHeuristic0:
            jointPairs.append([group[1], group[0]])
            continue

        # And if that doesn't work, then we'll begrudgingly just add the
        # panels individually.
        jointPairs += [[x] for x in group]

    # Now that we've formed pairs, let's determine the color for each display.
    jointColors = []
    for display in jointPairs:
        # type: none=0
        # type: shoulder=10, elbow=11, hand=12, finger=13, thumb=14
        # type: fingers=19-23
        # type: knee=3, foot=4, toe=5, toes=24-29

        # Try to determine coloring by joint label if possible.
        typeIds = []
        for joint in display:
            dependNode = om.MFnDependencyNode(joint[1])
            attr = dependNode.attribute("type")
            plug = dependNode.findPlug(attr, True)
            typeIds.append(plug.asInt())
            
        if typeIds.count(typeIds[0]) == len(typeIds):  # All elements the same.
            isArm = typeIds[0] >= 10 and typeIds[0] <= 14
            isFinger = typeIds[0] >= 19 and typeIds[0] <= 23
            isLeg = typeIds[0] >= 3 and typeIds[0] <= 5
            isToe = typeIds[0] >= 24 and typeIds[0] <= 29
            if isArm or isFinger:
                jointColors.append(1)
                continue
            elif isLeg or isToe:
                jointColors.append(2)
                continue

        # Otherwise try to determine coloring by heuristic.
        heuristics = []
        for joint in display:
            aHeuristic = armHeuristic(joint[0].partialPathName())
            lHeuristic = legHeuristic(joint[0].partialPathName())

            if aHeuristic > lHeuristic:
                heuristics.append(1)
            elif lHeuristic > aHeuristic:
                heuristics.append(2)
            else:
                heuristics.append(0)

        if heuristics.count(heuristics[0]) == len(heuristics):
            jointColors.append(heuristics[0])
            continue

        # If all else fails, it gets the default color.
        jointColors.append(0)

    return zip(jointPairs, jointColors)


def tearDownChartreuseUI():
    chartreuseToolPanel.reset(None, None)
