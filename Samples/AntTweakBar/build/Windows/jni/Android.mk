
DEPENDENCY_PATH := $(call my-dir)
LOCAL_PATH := $(abspath $(DEPENDENCY_PATH))

include $(CLEAR_VARS)
# Project configuration
LOCAL_MODULE := AntTweakBarSample
LOCAL_CFLAGS := -std=c++11
LOCAL_CPP_FEATURES := exceptions
LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv3 -DENGINE_DLL

#                                   !!!!! VERY IMPORTANT !!!!!
# Not too smart gcc linker links libraries in the order they are declared. From each library it only exports the 
# methods/functions required to resolve CURRENTLY OUTSTANDING dependencies and ignores the rest. 
# If a subsequent library then uses methods/functions that were not originally required by the objects, you will 
# have missing dependencies.
LOCAL_STATIC_LIBRARIES := SampleBase-prebuilt AntTweakBar-prebuilt RenderScript-prebuilt GraphicsEngine-prebuilt GraphicsTools-prebuilt Lua-prebuilt cpufeatures android_native_app_glue ndk_helper
# These libraries depend on each other
LOCAL_WHOLE_STATIC_LIBRARIES := AndroidPlatform-prebuilt BasicPlatform-prebuilt Common-prebuilt
LOCAL_SHARED_LIBRARIES := GraphicsEngineOpenGL-prebuilt

# Include paths
PROJECT_ROOT := $(LOCAL_PATH)/../../..
ENGINE_ROOT := $(PROJECT_ROOT)/../../..
CORE_ROOT := $(ENGINE_ROOT)/diligentcore
TOOLS_ROOT := $(ENGINE_ROOT)/diligenttools
SAMPLES_ROOT := $(ENGINE_ROOT)/diligentsamples
# include directories for static libraries are declared in corresponding LOCAL_EXPORT_C_INCLUDES sections


# Source files
#VisualGDBAndroid: AutoUpdateSourcesInNextLine
LOCAL_SRC_FILES := ../../../Src/AndroidMain.cpp ../../../Src/MengerSponge.cpp

include $(BUILD_SHARED_LIBRARY)


# Declare pre-built static libraries

include $(CLEAR_VARS)
# Declare pre-built Common library
LOCAL_MODULE := SampleBase-prebuilt
LOCAL_SRC_FILES := $(PROJECT_ROOT)/../SampleBase/build/Windows/obj/local/$(TARGET_ARCH_ABI)/libSampleBase.a

# The LOCAL_EXPORT_C_INCLUDES definition ensures that any module that depends on the 
# prebuilt one will have its LOCAL_C_INCLUDES automatically prepended with the path to the 
# prebuilt's include directory, and will thus be able to find headers inside that.
LOCAL_EXPORT_C_INCLUDES := $(PROJECT_ROOT)/../SampleBase/include

include $(PREBUILT_STATIC_LIBRARY)


# Declare pre-built static libraries

include $(CLEAR_VARS)
# Declare pre-built Common library
LOCAL_MODULE := Common-prebuilt
LOCAL_SRC_FILES := $(CORE_ROOT)/Common/build/Windows/obj/local/$(TARGET_ARCH_ABI)/libCommon.a

# The LOCAL_EXPORT_C_INCLUDES definition ensures that any module that depends on the 
# prebuilt one will have its LOCAL_C_INCLUDES automatically prepended with the path to the 
# prebuilt's include directory, and will thus be able to find headers inside that.
LOCAL_EXPORT_C_INCLUDES := $(CORE_ROOT)/Common/include $(CORE_ROOT)/Common/interface $(CORE_ROOT)/Platforms/interface

include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
# Declare pre-built AndroidPlatform library
LOCAL_MODULE := AndroidPlatform-prebuilt
LOCAL_SRC_FILES := $(CORE_ROOT)/Platforms/Android/build/Windows/obj/local/$(TARGET_ARCH_ABI)/libAndroidPlatform.a
LOCAL_EXPORT_C_INCLUDES := $(CORE_ROOT)/Platforms/interface
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
# Declare pre-built BasicPlatform library
LOCAL_MODULE := BasicPlatform-prebuilt
LOCAL_SRC_FILES := $(CORE_ROOT)/Platforms/Basic/build/Windows/obj/local/$(TARGET_ARCH_ABI)/libBasicPlatform.a
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
# Declare pre-built GraphicsEngine library
LOCAL_MODULE := GraphicsEngine-prebuilt
LOCAL_SRC_FILES := $(CORE_ROOT)/Graphics/GraphicsEngine/build/Windows/obj/local/$(TARGET_ARCH_ABI)/libGraphicsEngine.a
LOCAL_EXPORT_C_INCLUDES := $(CORE_ROOT)/Graphics/GraphicsEngine/interface
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
# Declare pre-built GraphicsEngineOpenGL library
LOCAL_MODULE := GraphicsEngineOpenGL-prebuilt
LOCAL_SRC_FILES := $(CORE_ROOT)/Graphics/GraphicsEngineOpenGL/build/Windows/libs/$(TARGET_ARCH_ABI)/libGraphicsEngineOpenGL.so
LOCAL_EXPORT_C_INCLUDES := $(CORE_ROOT)/Graphics/GraphicsEngineOpenGL/interface
include $(PREBUILT_SHARED_LIBRARY)


include $(CLEAR_VARS)
# Declare pre-built RenderScript library
LOCAL_MODULE := RenderScript-prebuilt
LOCAL_SRC_FILES := $(TOOLS_ROOT)/RenderScript/build/Windows/obj/local/$(TARGET_ARCH_ABI)/libRenderScript.a
LOCAL_EXPORT_C_INCLUDES := $(TOOLS_ROOT)/RenderScript/include
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
# Declare pre-built Lua library
LOCAL_MODULE := Lua-prebuilt
LOCAL_SRC_FILES := $(TOOLS_ROOT)/External/lua-5.2.3/build/Windows/obj/local/$(TARGET_ARCH_ABI)/libLua.a
LOCAL_EXPORT_C_INCLUDES := $(TOOLS_ROOT)/External/lua-5.2.3/src
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
# Declare pre-built AntTweakBar library
LOCAL_MODULE := AntTweakBar-prebuilt
LOCAL_SRC_FILES := $(SAMPLES_ROOT)/External/TwBarLib/build/Windows/obj/local/$(TARGET_ARCH_ABI)/libAntTweakBar.a
LOCAL_EXPORT_C_INCLUDES := $(SAMPLES_ROOT)/External/TwBarLib/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
# Declare pre-built Graphics Tools library
LOCAL_MODULE := GraphicsTools-prebuilt
LOCAL_SRC_FILES := $(CORE_ROOT)/Graphics/GraphicsTools/build/Windows/obj/local/$(TARGET_ARCH_ABI)/libGraphicsTools.a
LOCAL_EXPORT_C_INCLUDES := $(CORE_ROOT)/Graphics/GraphicsTools/include
include $(PREBUILT_STATIC_LIBRARY)

$(call import-module,android/ndk_helper)
$(call import-module,android/native_app_glue)
$(call import-module,android/cpufeatures)

