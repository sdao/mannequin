from maya import cmds
from maya import OpenMaya as om
from maya import OpenMayaUI as ui

from PySide.QtCore import *
from PySide.QtGui import *
from PySide.QtUiTools import *
from shiboken import wrapInstance

from mannequin_style import MannequinStylesheets
from mannequin_widgets import DragRotationWidget, DragTranslationWidget

import os
from functools import partial
from collections import OrderedDict
from collections import namedtuple


JointInfo = namedtuple("JointInfo", "dagPath dependNode availableStyles")


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
        if event.type() == QEvent.MouseButtonPress:
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
        self.reset()

    def reset(self,
              parent=None,
              gui=None,
              searchField=None,
              scroll=None,
              prefixTrim=0):
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
        self.scroll = scroll
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
            isTheOne = panel == selectedPanel
            self.panels[panel].groupBox.setFlat(isTheOne)

            if isTheOne:
                y = self.panels[panel].parentWidget().pos().y()
                height = self.panels[panel].parentWidget().height()
                self.scrollEnsureVisible(y, height)

    def layoutJointDisplay(self, jointDisplay):
        file = QFile(os.path.join(os.path.dirname(__file__),
                                  "panel_double.ui"))
        file.open(QFile.ReadOnly)
        container = self.loader.load(file)
        file.close()

        # Add child panels.
        # All joints in display should match.
        # TODO: Deal with more than one possible presentation style.
        style = jointDisplay[0].availableStyles[0]
        for jointInfo in jointDisplay:
            self.insertJointDisplayPanel(jointInfo, style, container)

        # Add container to UI tree.
        self.gui.layout().addWidget(container)

    def insertJointDisplayPanel(self, jointInfo, style, container):
        file = QFile(os.path.join(os.path.dirname(__file__),
                                  "panel_single.ui"))
        file.open(QFile.ReadOnly)
        panelGui = self.loader.load(file)
        file.close()

        # Register panels according to DAG path.
        dagPath = jointInfo.dagPath
        nodeName = dagPath.partialPathName()
        self.dagPaths[nodeName] = dagPath
        self.panels[nodeName] = panelGui

        # Setup panel validators.
        panelGui.xEdit.setValidator(self.validator)
        panelGui.yEdit.setValidator(self.validator)
        panelGui.zEdit.setValidator(self.validator)

        # Setup panel title.
        if len(nodeName) > self.prefixTrim:
            trimmedName = nodeName[self.prefixTrim:]
        else:
            trimmedName = nodeName
        panelGui.groupBox.setTitle(trimmedName)

        # Set up drag-label UI.
        if style == "r":
            rotateXDrag = DragRotationWidget("rotate X", dagPath, 0)
            rotateYDrag = DragRotationWidget("rotate Y", dagPath, 1)
            rotateZDrag = DragRotationWidget("rotate Z", dagPath, 2)
            panelGui.xLabel.layout().addWidget(rotateXDrag, 0, 0)
            panelGui.yLabel.layout().addWidget(rotateYDrag, 0, 0)
            panelGui.zLabel.layout().addWidget(rotateZDrag, 0, 0)
        elif style == "t":
            translateXDrag = DragTranslationWidget("translate X", dagPath, 0)
            translateYDrag = DragTranslationWidget("translate Y", dagPath, 1)
            translateZDrag = DragTranslationWidget("translate Z", dagPath, 2)
            panelGui.xLabel.layout().addWidget(translateXDrag, 0, 0)
            panelGui.yLabel.layout().addWidget(translateYDrag, 0, 0)
            panelGui.zLabel.layout().addWidget(translateZDrag, 0, 0)

        # Set color based on presentation style.
        if style == "r":
            panelGui.groupBox.setStyleSheet(MannequinStylesheets.STYLE_BLUE)
        elif style == "t":
            panelGui.groupBox.setStyleSheet(MannequinStylesheets.STYLE_GREEN)

        # Set current object properties.
        if style == "r":
            self.updatePanelRotation(panelGui, dagPath)
        elif style == "t":
            self.updatePanelTranslation(panelGui, dagPath)

        # Register signal for text box editing.
        if style == "r":
            panelGui.xEdit.editingFinished.connect(
                partial(self.setRotation, dagPath=dagPath, index=0))
            panelGui.yEdit.editingFinished.connect(
                partial(self.setRotation, dagPath=dagPath, index=1))
            panelGui.zEdit.editingFinished.connect(
                partial(self.setRotation, dagPath=dagPath, index=2))
        elif style == "t":
            panelGui.xEdit.editingFinished.connect(
                partial(self.setTranslation, dagPath=dagPath, index=0))
            panelGui.yEdit.editingFinished.connect(
                partial(self.setTranslation, dagPath=dagPath, index=1))
            panelGui.zEdit.editingFinished.connect(
                partial(self.setTranslation, dagPath=dagPath, index=2))

        # Register callback for attribute update.
        callbackId = om.MNodeMessage.addNodeDirtyPlugCallback(
            jointInfo.dependNode,
            self.dirtyPlugCallback,
            None)
        self.callbacks.append(callbackId)

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
            self.updateQueue.append((nodeName, "r"))
            if len(self.updateQueue) == 1:
                cmds.evalDeferred(self.deferredUpdate, low=True)
        elif attrName[:9] == "translate":
            self.updateQueue.append((nodeName, "t"))
            if len(self.updateQueue) == 1:
                cmds.evalDeferred(self.deferredUpdate, low=True)

    def deferredUpdate(self):
        for nodeName, style in self.updateQueue:
            if nodeName not in self.dagPaths:
                continue

            panelGui = self.panels[nodeName]
            dagPath = self.dagPaths[nodeName]

            if style == "r":
                self.updatePanelRotation(panelGui, dagPath)
            elif style == "t":
                self.updatePanelTranslation(panelGui, dagPath)

        self.updateQueue = []

    def updatePanelRotation(self, panelGui, dagPath):
        """Sets the panel rotation in internal units (probably radians)."""

        objectXform = om.MFnTransform(dagPath)
        rotation = om.MEulerRotation()
        objectXform.getRotation(rotation)

        xx = om.MAngle.internalToUI(rotation.x)
        panelGui.xEdit.setText("{:.3f}".format(xx))

        yy = om.MAngle.internalToUI(rotation.y)
        panelGui.yEdit.setText("{:.3f}".format(yy))

        zz = om.MAngle.internalToUI(rotation.z)
        panelGui.zEdit.setText("{:.3f}".format(zz))

    def updatePanelTranslation(self, panelGui, dagPath):
        """Sets the panel translation in internal units (probably cm)."""

        objectXform = om.MFnTransform(dagPath)
        translation = objectXform.getTranslation(om.MSpace.kTransform)

        xx = om.MDistance.internalToUI(translation.x)
        panelGui.xEdit.setText("{:.3f}".format(xx))

        yy = om.MDistance.internalToUI(translation.y)
        panelGui.yEdit.setText("{:.3f}".format(yy))

        zz = om.MDistance.internalToUI(translation.z)
        panelGui.zEdit.setText("{:.3f}".format(zz))

    def setRotation(self, dagPath, index, *args, **kwargs):
        nodeName = dagPath.partialPathName()
        panelGui = self.panels[nodeName]

        # Because I'm lazy, I'm going to use the setAttr command!
        # This means that we won't have to convert units -- they should already
        # be in the UI units, which MEL expects.
        if index == 0 and panelGui.xEdit.isModified():
            x = float(panelGui.xEdit.text())
            cmds.setAttr("{0}.rotateX".format(nodeName), x)
        elif index == 1 and panelGui.yEdit.isModified():
            y = float(panelGui.yEdit.text())
            cmds.setAttr("{0}.rotateY".format(nodeName), y)
        elif index == 2 and panelGui.zEdit.isModified():
            z = float(panelGui.zEdit.text())
            cmds.setAttr("{0}.rotateZ".format(nodeName), z)

        self.updatePanelRotation(panelGui, dagPath)

    def setTranslation(self, dagPath, index, *args, **kwargs):
        nodeName = dagPath.partialPathName()
        panelGui = self.panels[nodeName]

        # Because I'm lazy, I'm going to use the setAttr command!
        # This means that we won't have to convert units -- they should already
        # be in the UI units, which MEL expects.
        if index == 0 and panelGui.xEdit.isModified():
            x = float(panelGui.xEdit.text())
            cmds.setAttr("{0}.translateX".format(nodeName), x)
        elif index == 1 and panelGui.yEdit.isModified():
            y = float(panelGui.yEdit.text())
            cmds.setAttr("{0}.translateY".format(nodeName), y)
        elif index == 2 and panelGui.zEdit.isModified():
            z = float(panelGui.zEdit.text())
            cmds.setAttr("{0}.translateZ".format(nodeName), z)

        self.updatePanelTranslation(panelGui, dagPath)

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

    def scrollEnsureVisible(self, y, height, margin=50):
        scrollVert, _ = cmds.scrollLayout("mannequinScrollLayout",
                                          query=True,
                                          sav=True)
        scrollHeight = cmds.scrollLayout("mannequinScrollLayout",
                                         query=True,
                                         sah=True)

        visibleTop = scrollVert
        visibleBottom = scrollVert + scrollHeight
        belowFold = y >= visibleBottom
        aboveFold = y + height <= visibleTop

        # Adjust margin depending on the scroll area height.
        margin = max(0, min(scrollHeight - height, margin))

        # Scroll if required.
        if aboveFold or height > scrollHeight:
            cmds.scrollLayout("mannequinScrollLayout",
                              edit=True,
                              sbp=("up", 1000000))
            cmds.scrollLayout("mannequinScrollLayout",
                              edit=True,
                              sbp=("down", y - margin))
        elif belowFold:
            cmds.scrollLayout("mannequinScrollLayout",
                              edit=True,
                              sbp=("up", 1000000))
            cmds.scrollLayout("mannequinScrollLayout",
                              edit=True,
                              sbp=("down", y - scrollHeight + height + margin))


# Singleton object, global state.
mannequinToolPanel = MannequinToolPanel()


def setupMannequinUI():
    """Sets up the side panel UI for the Mannequin plugin."""
    currentContext = cmds.currentCtx()
    influenceObjectsStr = cmds.mannequinContext(currentContext,
                                                 q=True,
                                                 io=True)
    ioTokens = influenceObjectsStr.split(" ")
    ioDagPaths = ioTokens[::2]
    ioAvailableStyles = ioTokens[1::2]

    mannequinDockPtr = ui.MQtUtil.findLayout("mannequinPaletteDock")
    mannequinDock = wrapInstance(long(mannequinDockPtr), QWidget)
    mannequinDock.setMinimumWidth(300)

    mannequinScroll = mannequinDock.findChild(QScrollArea)

    mannequinLayoutPtr = ui.MQtUtil.findLayout("mannequinPaletteLayout")
    mannequinLayout = wrapInstance(long(mannequinLayoutPtr), QWidget)

    mannequinSearchPtr = ui.MQtUtil.findControl("mannequinSearchField")
    mannequinSearch = wrapInstance(long(mannequinSearchPtr), QLineEdit)

    file = QFile(os.path.join(os.path.dirname(__file__), "mannequin.ui"))
    file.open(QFile.ReadOnly)
    gui = mannequinToolPanel.loader.load(file, parentWidget=mannequinLayout)
    file.close()

    selList = om.MSelectionList()
    for obj in ioDagPaths:
        selList.add(obj)

    joints = []
    for i in range(selList.length()):
        dagPath = om.MDagPath()
        selList.getDagPath(i, dagPath)

        dependNode = om.MObject()
        selList.getDependNode(i, dependNode)

        styles = ioAvailableStyles[i]

        joints.append(JointInfo(dagPath, dependNode, styles))

    # Alphabetize joints by full DAG path; should sort slightly better.
    joints = sorted(joints, key=lambda j: j.dagPath.fullPathName())

    prefixTrim = commonPrefix(joints)
    mannequinToolPanel.reset(mannequinLayout,
                             gui,
                             mannequinSearch,
                             mannequinScroll,
                             prefixTrim)

    jointDisplays = organizeJoints(joints)
    for jointDisplay in jointDisplays:
        mannequinToolPanel.layoutJointDisplay(jointDisplay)
    mannequinToolPanel.finishLayout()

def organizeJoints(joints):
    """Applies a heuristic for organizing joints for display.

    The heuristic looks for occurences of "left", "right", "l", and "r" in the
    joint names for forming left-right pairs of joints. It uses Maya joint
    labelling or body part names to determine how the joint displays should be
    organized.

    :param joints: the unorganized JointInfos
    :type joints: list[JointInfo]
    :returns: a list of organized JointInfo groups; each group has at most
              two JointInfos
    :rtype: list[list[JointInfo]]
    """
    def stripName(n):
        n = n.lower()
        n = n.replace("left", "~")
        n = n.replace("right", "~")
        n = n.replace("l", "~")
        n = n.replace("r", "~")
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

    # Combine joints with similar names.
    strippedJointNames = [stripName(x.dagPath.partialPathName())
                          for x in joints]
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

        """:type: JointInfo"""
        joint0 = group[0]
        """:type: JointInfo"""
        joint1 = group[1]

        # If group doesn't have matching presentation style, then can't match.
        if joint0.availableStyles != joint1.availableStyles:
            jointPairs += [[x] for x in group]
            continue

        # Determine if the group is a true left-right pair using Maya's
        # joint labels. If possible, insert a left-right double panel.
        # side: 0=center, 1=left, 2=right, 3=none
        dependNode0 = om.MFnDependencyNode(joint0.dependNode)
        dependNode1 = om.MFnDependencyNode(joint1.dependNode)
        attr0 = dependNode0.attribute("side")
        attr1 = dependNode1.attribute("side")
        plug0 = dependNode0.findPlug(attr0, True)
        plug1 = dependNode1.findPlug(attr1, True)
        side0 = plug0.asInt()
        side1 = plug1.asInt()
        if side0 == 1 and side1 == 2:  # left, right
            jointPairs.append([joint0, joint1])
            continue
        elif side0 == 2 and side1 == 1:  # right, left
            jointPairs.append([joint1, joint0])
            continue

        # If joint labels are not conclusive, then try to use a heuristic
        # based on the joint names.
        lfHeuristic0 = leftHeuristic(joint0.dagPath.partialPathName())
        lfHeuristic1 = leftHeuristic(joint1.dagPath.partialPathName())
        rtHeuristic0 = rightHeuristic(joint0.dagPath.partialPathName())
        rtHeuristic1 = rightHeuristic(joint1.dagPath.partialPathName())

        if lfHeuristic0 >= lfHeuristic1 and rtHeuristic1 >= rtHeuristic0:
            jointPairs.append([joint0, joint1])
            continue
        elif lfHeuristic0 <= lfHeuristic1 and rtHeuristic1 <= rtHeuristic0:
            jointPairs.append([joint1, joint0])
            continue

        # And if that doesn't work, then we'll begrudgingly just add the
        # panels individually.
        jointPairs += [[x] for x in group]

    return jointPairs


def commonPrefix(joints):
    """Determines whether the list of joints has a common prefix.

    :param joints: the unorganized JointInfos
    :type joints: list[JointInfo]
    :returns: the length of any prefix shared by the JointInfos,
              or 0 if there is no such prefix
    :rtype: int
    """
    jointNames = [x.dagPath.partialPathName() for x in joints]
    prefix = os.path.commonprefix(jointNames)
    return len(prefix)


def mannequinSelectionChanged(dagString, style):
    """Callback from the C++ MannequinContext when its selection changes.

    This callback may be the result of a Python-initiated selection change.

    :param dagString: the full DAG path of the new selection, or the empty
                      string if there is no selection
    :type dagString: str
    :param style: either the string "r" or "t" indicating the current
                  presentation style for the selection
    :type style: str
    """
    try:
        selList = om.MSelectionList()
        selList.add(dagString)
        dagPath = om.MDagPath()
        selList.getDagPath(0, dagPath)
        mannequinToolPanel.select(dagPath)
        print(style)
    except:
        mannequinToolPanel.select(None)


def tearDownMannequinUI():
    """Remove all of the callbacks and event filters for the UI."""
    mannequinToolPanel.reset()
