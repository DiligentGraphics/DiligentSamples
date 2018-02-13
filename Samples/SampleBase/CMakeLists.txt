cmake_minimum_required (VERSION 3.6)

project(SampleBase)

if(PLATFORM_WIN32)
    set(SOURCE 
        src/Win32/SampleAppWin32.cpp
        src/SampleApp.cpp
    )
    set(INCLUDE 
        include/SampleApp.h
    )

elseif(PLATFORM_UNIVERSAL_WINDOWS)

    # Windows Runtime types cannot be included into static libraries
    # https://social.msdn.microsoft.com/Forums/en-US/269db513-64ef-4817-a025-43954f614eb3/lnk4264-why-are-static-libraries-not-recommended-when-authoring-windows-runtime-types?forum=winappswithnativecode
    # So as a workaround, we will include all source files into the target app project
    function(append_sample_base_uwp_source TARGET_NAME)
        get_target_property(SAMPLE_BASE_SOURCE_DIR SampleBase SOURCE_DIR)

        set(SAMPLE_BASE_UWP_SOURCE
            ${SAMPLE_BASE_SOURCE_DIR}/src/UWP/TwEventUWP.cpp
            ${SAMPLE_BASE_SOURCE_DIR}/src/UWP/SampleAppUWP.cpp
            ${SAMPLE_BASE_SOURCE_DIR}/src/SampleApp.cpp
        )

        set(SAMPLE_BASE_UWP_INCLUDE
            ${SAMPLE_BASE_SOURCE_DIR}/src/UWP/TwEventUWP.h
            ${SAMPLE_BASE_SOURCE_DIR}/include/SampleApp.h
        )

        set(SAMPLE_BASE_UWP_INCLUDE_DIR
            ${SAMPLE_BASE_SOURCE_DIR}/Src/UWP
        )

        target_sources(${TARGET_NAME} PRIVATE ${SAMPLE_BASE_UWP_SOURCE} ${SAMPLE_BASE_UWP_INCLUDE})
        source_group("src\\SampleBase" FILES ${SAMPLE_BASE_UWP_SOURCE})
        source_group("include\\SampleBase" FILES ${SAMPLE_BASE_UWP_INCLUDE})
        target_include_directories(${TARGET_NAME} PRIVATE ${SAMPLE_BASE_UWP_INCLUDE_DIR})
    endfunction()

elseif(PLATFORM_ANDROID)
    set(SOURCE
        src/Android/SampleAppAndroid.cpp
        src/SampleApp.cpp
    )
    set(INCLUDE 
        include/SampleApp.h
    )
elseif(PLATFORM_LINUX)
    set(SOURCE 
        src/Linux/SampleAppLinux.cpp
        src/SampleApp.cpp
    )
    set(INCLUDE 
        include/SampleApp.h
    )
elseif(PLATFORM_MACOS)

    set(SOURCE
        src/MacOS/SampleAppMacOS.cpp
        src/SampleApp.cpp
    )
    set(INCLUDE
        include/SampleApp.h
    )

elseif(PLATFORM_IOS)
    set(SOURCE
        src/IOS/SampleAppIOS.cpp
        src/SampleApp.cpp
    )
    set(INCLUDE
        include/SampleApp.h
    )

endif()

list(APPEND SOURCE src/SampleBase.cpp)
list(APPEND INCLUDE include/SampleBase.h)


add_library(SampleBase STATIC ${SOURCE} ${INCLUDE})
set_common_target_properties(SampleBase)

target_include_directories(SampleBase
PUBLIC
    include
)


if(MSVC)
    target_compile_options(SampleBase PRIVATE -DUNICODE)
    
    if(PLATFORM_UNIVERSAL_WINDOWS)
        # Disable w4189: local variable is initialized but not referenced
        # Disable w4063: case is not a valid value for switch of enum
        # Consume the windows runtime extensions (/ZW)
        target_compile_options(SampleBase INTERFACE /wd4189 /wd4063 /ZW)
    endif()
endif()

if(PLATFORM_WIN32)
    SET(ENGINE_LIBRARIES
        GraphicsEngineD3D11-shared
        GraphicsEngineD3D12-shared 
        GraphicsEngineOpenGL-shared
    )
elseif(PLATFORM_ANDROID)
    SET(ENGINE_LIBRARIES
        GraphicsEngineOpenGL-shared
    )
elseif(PLATFORM_UNIVERSAL_WINDOWS)
    SET(ENGINE_LIBRARIES
        GraphicsEngineD3D11-static
        GraphicsEngineD3D12-static
    )
elseif(PLATFORM_LINUX)
    SET(ENGINE_LIBRARIES
        GraphicsEngineOpenGL-shared
    )
elseif(PLATFORM_MACOS)
    SET(ENGINE_LIBRARIES
        GraphicsEngineOpenGL-shared
    )
elseif(PLATFORM_IOS)
    # Statically link with the engine on iOS.
    # It is also possible to link dynamically by
    # putting the library into the framework.
    SET(ENGINE_LIBRARIES
        GraphicsEngineOpenGL-static
    )
else()
    message(FATAL_ERROR "Undefined platform")
endif()

target_link_libraries(SampleBase 
PRIVATE 
    BuildSettings 
PUBLIC
    NativeAppBase
    TwBarLib 
    Common 
    GraphicsTools
    TargetPlatform
    ${ENGINE_LIBRARIES}
)

if(PLATFORM_UNIVERSAL_WINDOWS)
    target_link_libraries(SampleBase PRIVATE dxguid.lib)
elseif(PLATFORM_ANDROID)
    target_link_libraries(SampleBase PRIVATE GLESv3 PUBLIC NativeAppGlue)
elseif(PLATFORM_LINUX)
    target_link_libraries(SampleBase PRIVATE GL X11)
elseif(PLATFORM_MACOS OR PLATFORM_IOS)

endif()

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})

set_target_properties(SampleBase PROPERTIES
    FOLDER Samples
)
