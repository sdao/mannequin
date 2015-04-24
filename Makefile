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
TOP := /Applications/Autodesk/maya2015/devkit/plug-ins
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

SRCDIR := .
DSTDIR := .

chartreuse_SOURCES  := $(SRCDIR)/chartreuse.cpp \
	$(SRCDIR)/chartreuse_manipulator.cpp
chartreuse_OBJECTS  := $(SRCDIR)/chartreuse.o \
	$(SRCDIR)/chartreuse_manipulator.o
chartreuse_PLUGIN   := $(DSTDIR)/chartreuse.$(EXT)
chartreuse_MAKEFILE := $(DSTDIR)/Makefile

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

$(chartreuse_OBJECTS): CFLAGS   := $(CFLAGS)   $(chartreuse_EXTRA_CFLAGS)
$(chartreuse_OBJECTS): C++FLAGS := $(C++FLAGS) $(chartreuse_EXTRA_C++FLAGS)
$(chartreuse_OBJECTS): INCLUDES := $(INCLUDES) $(chartreuse_EXTRA_INCLUDES)

depend_chartreuse:     INCLUDES := $(INCLUDES) $(chartreuse_EXTRA_INCLUDES)

$(chartreuse_PLUGIN):  LFLAGS   := $(LFLAGS) $(chartreuse_EXTRA_LFLAGS)
$(chartreuse_PLUGIN):  LIBS     := $(LIBS)   -lOpenMaya -lOpenMayaUI \
	-lOpenMayaAnim -lOpenMayaRender -lFoundation $(chartreuse_EXTRA_LIBS)

#
# Rules definitions
#

.PHONY: depend_chartreuse clean_chartreuse Clean_chartreuse


$(chartreuse_PLUGIN): $(chartreuse_OBJECTS)
	-rm -f $@
	$(LD) -o $@ $(LFLAGS) $^ $(LIBS)

depend_chartreuse :
	makedepend $(INCLUDES) $(MDFLAGS) -f$(DSTDIR)/Makefile $(chartreuse_SOURCES)

clean_chartreuse:
	-rm -f $(chartreuse_OBJECTS)

Clean_chartreuse:
	-rm -f $(chartreuse_MAKEFILE).bak $(chartreuse_OBJECTS) $(chartreuse_PLUGIN)


plugins: $(chartreuse_PLUGIN)
depend:	 depend_chartreuse
clean:	 clean_chartreuse
Clean:	 Clean_chartreuse
