
# Preamble
DEPENDENCY_PATH := $(call my-dir)
LOCAL_PATH := $(abspath $(DEPENDENCY_PATH))
include $(CLEAR_VARS)

# Project configuration
LOCAL_MODULE := AntTweakBar
# Force .c files to compile as c++
LOCAL_CFLAGS := -std=c++11 -x c++ -DTW_STATIC
LOCAL_CPP_FEATURES := exceptions
LOCAL_STATIC_LIBRARIES += 

# Include paths
PROJECT_ROOT := $(LOCAL_PATH)/../../../
ENGINE_ROOT := $(PROJECT_ROOT)/../../..
CORE_ROOT := $(ENGINE_ROOT)/diligentcore
LOCAL_C_INCLUDES += $(PROJECT_ROOT)/src
LOCAL_C_INCLUDES += $(PROJECT_ROOT)/include
LOCAL_C_INCLUDES += $(CORE_ROOT)/Graphics/GraphicsEngine/interface
LOCAL_C_INCLUDES += $(CORE_ROOT)/Common/include
LOCAL_C_INCLUDES += $(CORE_ROOT)/Common/interface
LOCAL_C_INCLUDES += $(CORE_ROOT)/Platforms/interface

# Source files
#VisualGDBAndroid: AutoUpdateSourcesInNextLine
LOCAL_SRC_FILES := ../../../src/TwBar.cpp ../../../src/TwColors.cpp ../../../src/TwGraphImpl.cpp ../../../src/TwEventGLFW.c ../../../src/TwEventGLUT.c ../../../src/TwEventSDL.c ../../../src/TwEventSDL12.c ../../../src/TwEventSDL13.c ../../../src/TwEventSFML.cpp ../../../src/TwEventWin.c ../../../src/TwFonts.cpp ../../../src/TwMgr.cpp ../../../src/TwPrecomp.cpp

#VisualGDBAndroid: VSExcludeListLocation
VISUALGDB_VS_EXCLUDED_FILES_Release := ../../../src/TwEventGLUT.c ../../../src/TwEventWin.c
VISUALGDB_VS_EXCLUDED_FILES_Debug := ../../../src/TwEventGLUT.c ../../../src/TwEventWin.c
LOCAL_SRC_FILES := $(filter-out $(VISUALGDB_VS_EXCLUDED_FILES_$(VGDB_VSCONFIG)),$(LOCAL_SRC_FILES))

include $(BUILD_STATIC_LIBRARY)
