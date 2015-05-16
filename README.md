Mannequin
==========
![Mannequin icon](icons/mannequin_maya2016.png)

Mannequin is a plugin for Maya that allows you to animate a character rig
by interacting directly with the character mesh. Simply hover over the mesh,
and Mannequin will detect and highlight parts of the mesh corresponding to
different bones in the rig.

Mannequin also includes a convenient side panel that gives you quick access
to all the joints in your rig and allows you to search through them.

Packaging/Installation
----------------------
Mannequin is currently tested only on Maya 2016. Previous versions of the code
worked on Maya 2015, so with some minor modifications, the current version
should also compile for older Maya versions.

You will want to create a Maya module for the plugin. The module should have an
`icons` folder and a `scripts` folder like the repo. In addition, the compiled
plugin (`.so`, `.bundle`, or `.mll`) needs to go in a `plug-ins` folder in the
module.

The Makefile included with this project is copied from the Mac OS X version of
the Maya Developer's Kit. To compile Linux or Windows versions, you will need
to copy and modify the Makefile appropriate for your operating system.

Usage
-----
Mannequin will install a "Mannequin" shelf in Maya if one doesn't exist. For
best results, hide the joints in your rig before continuing. Select the
smooth-bound mesh and click the ![Mannequin icon](icons/mannequin_maya2016.png)
Mannequin icon to begin. Mannequin doesn't work with NURBS surfaces, only poly
meshes. However, it does work with poly meshes that have nodes such as the
Smooth node applied.

Screenshots
-----------
![Mannequin manipulators in the viewport](screenshots/manipulators.png)

In this image, the lower lip has an active translation manipulator. The pointer is hovered over the outer-left eyebrow, which highlights it in the viewport.

![Mannequin side panel](screenshots/panel.png)

Mannequin also has a panel containing an editor for each joint in the rig. By
typing in the search bar at the top, you can filter the joints.

Rotation channels appear in blue groups, and translation channels appear in
green groups. A yellow border indicates the currently-selected joint. Fields
with red backgrounds indicate a keyframe, just like in the Channel Box.

Technical Details
-----------------
### Technology
The in-viewport manipulators are all done in C++ using the OpenMaya SDK. The
side-panel UI is done using Python and PySide bindings to Qt. There is a bit of
MEL involved, but it is mostly used as glue between the C++ and Python parts,
where necessary.

### Translation Manipulator
Mannequin includes a custom translation manipulator, built from scratch. This
component works independently of the rest of the Mannequin code, so it might
be of separate use to you, depending on your needs.

I had to make a custom translation manipulator because the manipulator that
Maya provides to plug-in developers in the Maya SDK is not the same as the
normal translation manipulator in Maya. In fact, the SDK manipulator is horribly
broken and not very customizable.

This new manipulator only supports single-axis translations, with the axes
defined by the object's pivot, but the translations should feel the same as the
normal Maya manipultors.
