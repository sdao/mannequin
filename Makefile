#-
# ==========================================================================
# Copyright (c) 2011 Autodesk, Inc.
# All rights reserved.
#
# These coded instructions, statements, and computer programs contain
# unpublished proprietary information written by Autodesk, Inc., and are
# protected by Federal copyright law. They may not be disclosed to third
# parties or copied or duplicated in any form, in whole or in part, without
# the prior written consent of Autodesk, Inc.
# ==========================================================================
#+

ifndef INCL_BUILDRULES

#
# Include platform specific build settings
#
TOP := /Applications/Autodesk/maya2016/devkit/plug-ins
include $(TOP)/buildrules

C++FLAGS += -std=c++11

#
# Always build the local plug-in when make is invoked from the
# directory.
#
all : plugins

endif

#
# Variable definitions
#

SRCDIR := src
DSTDIR := .

mannequin_SOURCES  := $(SRCDIR)/mannequin.cpp \
	$(SRCDIR)/mannequin_manipulator.cpp \
	$(SRCDIR)/move_manipulator.cpp
mannequin_OBJECTS  := $(SRCDIR)/mannequin.o \
	$(SRCDIR)/mannequin_manipulator.o \
	$(SRCDIR)/move_manipulator.o
mannequin_PLUGIN   := $(DSTDIR)/mannequin.$(EXT)
mannequin_MODULE   := $(DSTDIR)/mannequin_module
mannequin_MAKEFILE := $(DSTDIR)/Makefile

#
# Include the optional per-plugin Makefile.inc
#
#    The file can contain macro definitions such as:
#       {pluginName}_EXTRA_CFLAGS
#       {pluginName}_EXTRA_C++FLAGS
#       {pluginName}_EXTRA_INCLUDES
#       {pluginName}_EXTRA_LIBS
-include $(SRCDIR)/Makefile.inc


#
# Set target specific flags.
#

$(mannequin_OBJECTS): CFLAGS   := $(CFLAGS)   $(mannequin_EXTRA_CFLAGS)
$(mannequin_OBJECTS): C++FLAGS := $(C++FLAGS) $(mannequin_EXTRA_C++FLAGS)
$(mannequin_OBJECTS): INCLUDES := $(INCLUDES) $(mannequin_EXTRA_INCLUDES)

depend_mannequin:     INCLUDES := $(INCLUDES) $(mannequin_EXTRA_INCLUDES)

$(mannequin_PLUGIN):  LFLAGS   := $(LFLAGS) $(mannequin_EXTRA_LFLAGS)
$(mannequin_PLUGIN):  LIBS     := $(LIBS)   -lOpenMaya -lOpenMayaUI \
	-lOpenMayaAnim -lOpenMayaRender -lFoundation -framework OpenGL \
	$(mannequin_EXTRA_LIBS)

#
# Rules definitions
#

.PHONY: depend_mannequin clean_mannequin Clean_mannequin


$(mannequin_PLUGIN): $(mannequin_OBJECTS)
	-rm -f $@
	-rm -rf $(mannequin_MODULE)

	$(LD) -o $@ $(LFLAGS) $^ $(LIBS)

	mkdir -p $(mannequin_MODULE)
	mkdir -p $(mannequin_MODULE)/plug-ins
	cp -r icons $(mannequin_MODULE)
	cp -r scripts $(mannequin_MODULE)
	cp -r mannequin.bundle $(mannequin_MODULE)/plug-ins

depend_mannequin:
	makedepend $(INCLUDES) $(MDFLAGS) -f$(DSTDIR)/Makefile $(mannequin_SOURCES)

clean_mannequin:
	-rm -f $(mannequin_OBJECTS)
	-rm -rf $(mannequin_MODULE)

Clean_mannequin:
	-rm -f $(mannequin_MAKEFILE).bak $(mannequin_OBJECTS) $(mannequin_PLUGIN)
	-rm -rf $(mannequin_MODULE)


plugins: $(mannequin_PLUGIN)
depend:	 depend_mannequin
clean:	 clean_mannequin
Clean:	 Clean_mannequin
