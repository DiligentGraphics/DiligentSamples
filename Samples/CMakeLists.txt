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

if(PLATFORM_WIN32 AND D3D11_SUPPORTED AND D3D12_SUPPORTED)
    if(TARGET Diligent-TextureLoader)
	    add_subdirectory(Asteroids)
    else()
        message("Unable to find Diligent-TextureLoader target: Asteroids demo will be disabled")
    endif()
endif()
