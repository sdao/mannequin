from maya import cmds
from maya import OpenMaya as om
from maya import OpenMayaUI as ui

from PySide.QtCore import *
from PySide.QtGui import *
from PySide.QtUiTools import *
from shiboken import wrapInstance

from mannequin_style import MannequinStylesheets
from mannequin_widgets import DragWidget

import os
from functools import partial
from collections import OrderedDict


class ResizeEventFilter(QObject):
    def __init__(self):
        super(ResizeEventFilter, self).__init__()
        self.source = None
        self.target = None

    def install(self, source, target):
        self.source = source
        self.target = target
        self.source.installEventFilter(self)

    def remove(self):
        if self.source is not None:
            self.source.removeEventFilter(self)
        self.source = None
        self.target = None

    def eventFilter(self, widget, event):
        if (event.type() == QEvent.Resize and
                self.source is not None and
                self.target is not None):
            geometry = self.target.geometry()
            sourceWidth = self.source.width()
            minWidth = self.target.minimumWidth()
            geometry.setWidth(max(sourceWidth, minWidth))
            self.target.setGeometry(geometry)
            return True

        return QWidget.eventFilter(self, widget, event)


class FocusEventFilter(QObject):
    def __init__(self):
        super(FocusEventFilter, self).__init__()
        self.panels = {}

    def install(self, panels):
        self.panels = panels.copy()
        for x in self.panels:
            self.panels[x].groupBox.installEventFilter(self)

    def remove(self):
        for x in self.panels:
            self.panels[x].groupBox.removeEventFilter(self)
        self.panels = {}

    def eventFilter(self, widget, event):
        if (event.type() == QEvent.MouseButtonPress):
            for x in self.panels:
                if self.panels[x].groupBox == widget:
                    # Select node with name x.
                    self.selectNode(x)
                    return True
            return True

        return QWidget.eventFilter(self, widget, event)

    def selectNode(self, nodeName):
        currentContext = cmds.currentCtx()
        cmds.mannequinContext(currentContext, e=True, sel=nodeName)

class MannequinToolPanel():
    def __init__(self):
        self.loader = QUiLoader()
        self.resizeEventFilter = ResizeEventFilter()
        self.focusEventFilter = FocusEventFilter()
        self.reset(None, None, None)

    def reset(self, parent, gui, searchField, prefixTrim=0):
        # Cleanup callbacks.
        try:
            for x in self.callbacks:
                om.MNodeMessage.removeCallback(x)
        except AttributeError:
            pass

        self.callbacks = []

        # Reset everything.
        self.parent = parent
        self.gui = gui
        self.searchField = searchField
        self.prefixTrim = prefixTrim
        self.dagPaths = {}
        self.panels = {}
        self.updateQueue = []

        self.resizeEventFilter.remove()
        self.focusEventFilter.remove()

        # Set up GUI number validator.
        if gui is None:
            self.validator = None
        else:
            self.validator = QDoubleValidator(gui)
            self.validator.setDecimals(3)

    def select(self, dagPath):
        if dagPath is None:
            selectedPanel = ""
        else:
            selectedPanel = dagPath.partialPathName()

        for panel in self.panels:
            self.panels[panel].groupBox.setFlat(panel == selectedPanel)

    def layoutJointDisplay(self, jointDisplay):
        file = QFile(os.path.join(os.path.dirname(__file__),
                                  "panel_double.ui"))
        file.open(QFile.ReadOnly)
        container = self.loader.load(file)
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
        file = QFile(os.path.join(os.path.dirname(__file__),
                                  "panel_single.ui"))
        file.open(QFile.ReadOnly)
        panelGui = self.loader.load(file)
        file.close()

        # Set up drag-label UI.
        rotateXDrag = DragWidget("rotate X", dagPath, 0)
        rotateYDrag = DragWidget("rotate Y", dagPath, 1)
        rotateZDrag = DragWidget("rotate Z", dagPath, 2)
        panelGui.rotateXLabel.layout().addWidget(rotateXDrag, 0, 0)
        panelGui.rotateYLabel.layout().addWidget(rotateYDrag, 0, 0)
        panelGui.rotateZLabel.layout().addWidget(rotateZDrag, 0, 0)

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
            panelGui.groupBox.setStyleSheet(MannequinStylesheets.STYLE_BLUE)
        elif color % 3 == 1:
            panelGui.groupBox.setStyleSheet(MannequinStylesheets.STYLE_GREEN)
        else:
            panelGui.groupBox.setStyleSheet(MannequinStylesheets.STYLE_RED)

        if len(nodeName) > self.prefixTrim:
            trimmedName = nodeName[self.prefixTrim:]
        else:
            trimmedName = nodeName
        panelGui.groupBox.setTitle(trimmedName)

        # Set current object rotation.
        objectXform = om.MFnTransform(dagPath)
        rotation = om.MEulerRotation()
        objectXform.getRotation(rotation)
        self.updatePanelGui(panelGui, rotation.x, rotation.y, rotation.z)

        # Register callback for attribute update.
        callbackId = om.MNodeMessage.addNodeDirtyPlugCallback(
            dependNode,
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

    def finishLayout(self):
        # Setup resize event filter.
        self.resizeEventFilter.install(self.parent, self.gui)

        # Setup focus event filter.
        self.focusEventFilter.install(self.panels)

        # Setup the rest of the UI and show it.
        self.searchField.textChanged.connect(self.search)
        self.gui.layout().setAlignment(Qt.AlignTop)
        self.relayout()
        self.gui.show()

    def dirtyPlugCallback(self, node, plug, *args, **kwargs):
        nodeName, attrName = plug.name().split(".")

        if nodeName not in self.panels:
            return

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
        if index == 0 and panelGui.rotateXEdit.isModified():
            x = float(panelGui.rotateXEdit.text())
            cmds.setAttr("{0}.rotateX".format(nodeName), x)
        elif index == 1 and panelGui.rotateYEdit.isModified():
            y = float(panelGui.rotateYEdit.text())
            cmds.setAttr("{0}.rotateY".format(nodeName), y)
        elif index == 2 and panelGui.rotateZEdit.isModified():
            z = float(panelGui.rotateZEdit.text())
            cmds.setAttr("{0}.rotateZ".format(nodeName), z)

        objectXform = om.MFnTransform(dagPath)
        rotation = om.MEulerRotation()
        objectXform.getRotation(rotation)
        self.updatePanelGui(panelGui, rotation.x, rotation.y, rotation.z)

    def search(self, text):
        for name in self.panels:
            if text.lower() in name.lower():
                self.panels[name].show()
            else:
                self.panels[name].hide()

        self.gui.layout().activate()
        self.relayout()
        QTimer.singleShot(0, self.relayout)

    def relayout(self):
        self.gui.setMinimumHeight(self.gui.sizeHint().height())
        self.gui.setMaximumHeight(self.gui.sizeHint().height())
        self.parent.setMinimumHeight(self.gui.sizeHint().height())
        self.parent.setMaximumHeight(self.gui.sizeHint().height())


# Singleton object, global state.
mannequinToolPanel = MannequinToolPanel()


def setupMannequinUI():
    currentContext = cmds.currentCtx()
    influenceObjectsStr = cmds.mannequinContext(currentContext,
                                                 q=True,
                                                 io=True)
    ioTokens = influenceObjectsStr.split(" ")
    ioDagPaths = ioTokens[::2]
    ioPresentations = ioTokens[1::2]
    influenceObjects = ioDagPaths

    mannequinDockPtr = ui.MQtUtil.findLayout("mannequinPaletteDock")
    mannequinDock = wrapInstance(long(mannequinDockPtr), QWidget)
    mannequinDock.setMinimumWidth(300)

    mannequinLayoutPtr = ui.MQtUtil.findLayout("mannequinPaletteLayout")
    mannequinLayout = wrapInstance(long(mannequinLayoutPtr), QWidget)

    mannequinSearchPtr = ui.MQtUtil.findControl("mannequinSearchField")
    mannequinSearch = wrapInstance(long(mannequinSearchPtr), QLineEdit)

    file = QFile(os.path.join(os.path.dirname(__file__), "mannequin.ui"))
    file.open(QFile.ReadOnly)
    gui = mannequinToolPanel.loader.load(file, parentWidget=mannequinLayout)
    file.close()

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

    prefixTrim = commonPrefix(joints)
    mannequinToolPanel.reset(mannequinLayout,
                             gui,
                             mannequinSearch,
                             prefixTrim)

    jointDisplays = organizeJoints(joints)
    for jointDisplay in jointDisplays:
        mannequinToolPanel.layoutJointDisplay(jointDisplay)
    mannequinToolPanel.finishLayout()


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


def commonPrefix(joints):
    """Determines whether the list of joints has a common prefix.
    Returns the length of the prefix, or 0 if there is none.
    """
    jointNames = [x[0].partialPathName() for x in joints]
    prefix = os.path.commonprefix(jointNames)
    return len(prefix)


def mannequinSelectionChanged(dagString):
    try:
        selList = om.MSelectionList()
        selList.add(dagString)
        dagPath = om.MDagPath()
        selList.getDagPath(0, dagPath)
        mannequinToolPanel.select(dagPath)
    except:
        mannequinToolPanel.select(None)


def tearDownMannequinUI():
    mannequinToolPanel.reset(None, None, None)
