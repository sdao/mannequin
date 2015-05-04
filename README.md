Mannequin
==========
![Mannequin icon](icons/mannequin_32.png)

Mannequin is a plugin for Maya that allows you to animate a character rig
by interacting directly with the character mesh. Simply hover over the mesh,
and Mannequin will detect and highlight parts of the mesh corresponding to
different bones in the rig.

Mannequin also includes a convenient side panel that gives you quick access
to all the joints in your rig and allows you to search through them.

Packaging/Installation
======================
You will want to create a Maya module for the plugin. The module should have an
`icons` folder and a `scripts` folder like the repo. In addition, the compiled
plugin (`.so`, `.bundle`, or `.mll`) needs to go in a `plug-ins` folder in the
module.

The Makefile included with this project is copied from the Mac OS X version of
the Maya Developer's Kit. To compile Linux or Windows versions, you will need
to copy and modify the Makefile appropriate for your operating system.

Screenshots
===========
![Mannequin manipulators in the viewport](screenshots/manipulators.png)

In this image, the right arm has an active manipulator and can be rotated. The pointer is hovered over the left arm, which highlights it in the viewport.

![Mannequin side panel](screenshots/panel.png)

Mannequin also has a panel containing an editor for each joint in the rig. By typing in the search bar at the top, you can filter the joints.
