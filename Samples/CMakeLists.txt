cmake_minimum_required (VERSION 3.6)

if(TARGET DiligentFX)
    add_subdirectory(Atmosphere)
endif()

if(TARGET Diligent-AssetLoader AND TARGET DiligentFX)
    add_subdirectory(GLTFViewer)
    add_subdirectory(Shadows)
endif()

add_subdirectory(ImguiDemo)
add_subdirectory(NuklearDemo)

if(TARGET glfw)
    add_subdirectory(GLFWDemo)
endif()

if(TARGET ozz_animation)
    add_subdirectory(Animation)
endif()