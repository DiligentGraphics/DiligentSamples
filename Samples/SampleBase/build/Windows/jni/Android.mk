
DEPENDENCY_PATH := $(call my-dir)
LOCAL_PATH := $(abspath $(DEPENDENCY_PATH))

include $(CLEAR_VARS)
# Project configuration
LOCAL_MODULE := SampleBase
LOCAL_CFLAGS := -std=c++11
LOCAL_CPP_FEATURES := exceptions

LOCAL_STATIC_LIBRARIES := cpufeatures android_native_app_glue ndk_helper

# Include paths
PROJECT_ROOT := $(LOCAL_PATH)/../../..
ENGINE_ROOT := $(PROJECT_ROOT)/../../..
CORE_ROOT := $(ENGINE_ROOT)/diligentcore
TOOLS_ROOT := $(ENGINE_ROOT)/diligenttools
SAMPLES_ROOT := $(ENGINE_ROOT)/diligentsamples
# include directories for static libraries are declared in corresponding LOCAL_EXPORT_C_INCLUDES sections
LOCAL_C_INCLUDES := $(PROJECT_ROOT)/include
LOCAL_C_INCLUDES += $(CORE_ROOT)/Common/include
LOCAL_C_INCLUDES += $(CORE_ROOT)/Common/interface
LOCAL_C_INCLUDES += $(CORE_ROOT)/Platforms/interface
LOCAL_C_INCLUDES += $(SAMPLES_ROOT)/External/TwBarLib/include
LOCAL_C_INCLUDES += $(CORE_ROOT)/Graphics/GraphicsEngine/interface
LOCAL_C_INCLUDES += $(CORE_ROOT)/Graphics/GraphicsEngineOpenGL/interface
LOCAL_C_INCLUDES += $(TOOLS_ROOT)/Graphics/RenderScript/include
LOCAL_C_INCLUDES += $(TOOLS_ROOT)/Graphics/External/lua-5.2.3/src
LOCAL_C_INCLUDES += $(TOOLS_ROOT)/Graphics/GraphicsTools/include

# Source files
#VisualGDBAndroid: AutoUpdateSourcesInNextLine
LOCAL_SRC_FILES := ../../../Src/Android/AndroidMainImpl.cpp ../../../Src/Windows/WinMain.cpp

# Build instructions
#VisualGDBAndroid: VSExcludeListLocation
VISUALGDB_VS_EXCLUDED_FILES_Release := ../../../Src/Windows/WinMain.cpp
VISUALGDB_VS_EXCLUDED_FILES_Debug := ../../../Src/Windows/WinMain.cpp
LOCAL_SRC_FILES := $(filter-out $(VISUALGDB_VS_EXCLUDED_FILES_$(VGDB_VSCONFIG)),$(LOCAL_SRC_FILES))

include $(BUILD_STATIC_LIBRARY)

$(call import-module,android/ndk_helper)
$(call import-module,android/native_app_glue)
$(call import-module,android/cpufeatures)
