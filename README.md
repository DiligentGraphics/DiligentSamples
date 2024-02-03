# DiligentSamples <img src="https://github.com/DiligentGraphics/DiligentCore/blob/master/media/diligentgraphics-logo.png" height=64 align="right" valign="middle">


This module contains tutorials and sample applications intended to demonstrate the usage of
[Diligent Engine](https://github.com/DiligentGraphics/DiligentEngine). The module depends on
the [Core](https://github.com/DiligentGraphics/DiligentCore), [Tools](https://github.com/DiligentGraphics/DiligentTools),
and [FX](https://github.com/DiligentGraphics/DiligentFX) submodules.

To build and run the applications in the module, please follow the [instructions](https://github.com/DiligentGraphics/DiligentEngine#build-and-run-instructions) in the master repository.

| Platform                                                                                                                                    |   Build Status                    |
| --------------------------------------------------------------------------------------------------------------------------------------------| --------------------------------- |
| <img src="https://github.com/DiligentGraphics/DiligentCore/blob/master/media/windows-logo.png" width=24 valign="middle"> Windows            | [![Build Status](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-windows.yml/badge.svg?branch=master)](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-windows.yml?query=branch%3Amaster) |
| <img src="https://github.com/DiligentGraphics/DiligentCore/blob/master/media/uwindows-logo.png" width=24 valign="middle"> Universal Windows | [![Build Status](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-windows.yml/badge.svg?branch=master)](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-windows.yml?query=branch%3Amaster) |
| <img src="https://github.com/DiligentGraphics/DiligentCore/blob/master/media/linux-logo.png" width=24 valign="middle"> Linux                | [![Build Status](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-linux.yml/badge.svg?branch=master)](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-linux.yml?query=branch%3Amaster) |
| <img src="https://github.com/DiligentGraphics/DiligentCore/blob/master/media/android-logo.png" width=24 valign="middle"> Android            | [![Build Status](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-android.yml/badge.svg?branch=master)](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-android.yml?query=branch%3Amaster) |
| <img src="https://github.com/DiligentGraphics/DiligentCore/blob/master/media/macos-logo.png" width=24 valign="middle"> MacOS                | [![Build Status](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-apple.yml/badge.svg?branch=master)](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-apple.yml?query=branch%3Amaster) |
| <img src="https://github.com/DiligentGraphics/DiligentCore/blob/master/media/apple-logo.png" width=24 valign="middle"> iOS                  | [![Build Status](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-apple.yml/badge.svg?branch=master)](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-apple.yml?query=branch%3Amaster) |
| <img src="https://github.com/DiligentGraphics/DiligentCore/blob/master/media/tvos-logo.png" width=24 valign="middle"> tvOS                  | [![Build Status](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-apple.yml/badge.svg?branch=master)](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-apple.yml?query=branch%3Amaster) |
| <img src="https://github.com/DiligentGraphics/DiligentCore/blob/master/media/emscripten-logo.png" width=24 valign="middle"> Emscripten      | [![Build Status](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-emscripten.yml/badge.svg?branch=master)](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/build-emscripten.yml?query=branch%3Amaster) | 

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](License.txt)
[![Chat on Discord](https://img.shields.io/discord/730091778081947680?logo=discord)](https://discord.gg/t7HGBK7)
[![Appveyor Build Status](https://ci.appveyor.com/api/projects/status/github/DiligentGraphics/DiligentSamples?svg=true)](https://ci.appveyor.com/project/DiligentGraphics/diligentsamples)
[![MSVC Code Analysis](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/msvc_analysis.yml/badge.svg?branch=master)](https://github.com/DiligentGraphics/DiligentSamples/actions/workflows/msvc_analysis.yml?query=branch%3Amaster)
[![Lines of Code](https://tokei.rs/b1/github.com/DiligentGraphics/DiligentSamples)](https://github.com/DiligentGraphics/DiligentSamples)

# Table of Contents

- [Tutorials](#tutorials)
  - [01 - Hello Triangle](#tutorial-01---hello-triangle)
  - [02 - Cube](#tutorial-02---cube)
  - [03 - Texturing](#tutorial-03---texturing)
  - [03 - Texturing-C](#tutorial-03---texturing-c)
  - [03 - Texturing-DotNet](#tutorial-03---texturing-dotnet)
  - [04 - Instancing](#tutorial-04---instancing)
  - [05 - Texture Array](#tutorial-05---texture-array)
  - [06 - Multithreading](#tutorial-06---multithreading)
  - [07 - Geometry Shader](#tutorial-07---geometry-shader)
  - [08 - Tessellation](#tutorial-08---tessellation)
  - [09 - Quads](#tutorial-09---quads)
  - [10 - Data Streaming](#tutorial-10---data-streaming)
  - [11 - Resource Updates](#tutorial-11---resource-updates)
  - [12 - Render Target](#tutorial-12---render-target)
  - [13 - Shadow Map](#tutorial-13---shadow-map)
  - [14 - Compute Shader](#tutorial-14---compute-shader)
  - [15 - Multiple Windows](#tutorial-15---multiple-windows)
  - [16 - Bindless Resources](#tutorial-16---bindless-resources)
  - [17 - MSAA](#tutorial-17---msaa)
  - [18 - Queries](#tutorial-18---queries)
  - [19 - Render Passes](#tutorial-19---render-passes)
  - [20 - Mesh Shader](#tutorial-20---mesh-shader)
  - [21 - Ray Tracing](#tutorial-21---ray-tracing)
  - [22 - Hybrid Rendering](#tutorial-22---hybrid-rendering)
  - [23 - Command Queues](#tutorial-23---command-queues)
  - [24 - Variable Rate Shading](#tutorial-24---variable-rate-shading)
  - [25 - Render State Packager](#tutorial-25---render-state-packager)
  - [26 - Render State Cache](#tutorial-26---render-state-cache)
  - [27 - Post-Processing](#tutorial-27---post-processing)
- [Samples](#samples)
  - [Atmospheric Light Scattering](#atmospheric-light-scattering-sample)
  - [GLFW Demo](#glfw-demo)
  - [GLTF Viewer](#gltf-viewer)
  - [USD Viewer](#usd-viewer)
  - [Shadows](#shadows)
  - [Dear ImGui Demo](#dear-imgui-demo)
  - [Nuklear Demo](#nuklear-demo)
  - [Hello AR](#hello-ar)
  - [Asteroids](#asteroids)
  - [Unity Plugin](#unity-plugin)
- [Build and Run Instructions](#build-and-run-instructions)
- [License](#license)
- [Contributing](#contributing)

# Tutorials

## [Tutorial 01 - Hello Triangle](Tutorials/Tutorial01_HelloTriangle)

![](Tutorials/Tutorial01_HelloTriangle/Screenshot.png)

This tutorial shows how to render simple triangle using Diligent Engine API.


## [Tutorial 02 - Cube](Tutorials/Tutorial02_Cube)

![](Tutorials/Tutorial02_Cube/Animation_Large.gif)

This tutorial demonstrates how to render an actual 3D object, a cube. It shows how to load shaders from files, create and use vertex, 
index and uniform buffers.


## [Tutorial 03 - Texturing](Tutorials/Tutorial03_Texturing)

![](Tutorials/Tutorial03_Texturing/Animation_Large.gif)

This tutorial demonstrates how to apply a texture to a 3D object. It shows how to load a texture from file, create shader resource
binding object and how to sample a texture in the shader.


## [Tutorial 03 - Texturing-C](Tutorials/Tutorial03_Texturing-C)

This tutorial is identical to Tutorial03, but is implemented using the C API.


## [Tutorial 03 - Texturing-DotNet](Tutorials/Tutorial03_Texturing-DotNet)

This tutorial demonstrates how to use the Diligent Engine API in .NET applications.


## [Tutorial 04 - Instancing](Tutorials/Tutorial04_Instancing)

![](Tutorials/Tutorial04_Instancing/Animation_Large.gif)

This tutorial demonstrates how to use instancing to render multiple copies of one object
using unique transformation matrix for every copy.


## [Tutorial 05 - Texture Array](Tutorials/Tutorial05_TextureArray)

![](Tutorials/Tutorial05_TextureArray/Animation_Large.gif)

This tutorial demonstrates how to combine instancing with texture arrays to 
use unique texture for every instance.


## [Tutorial 06 - Multithreading](Tutorials/Tutorial06_Multithreading)

![](Tutorials/Tutorial06_Multithreading/Animation_Large.gif)

This tutorial shows how to generate command lists in parallel from multiple threads.


## [Tutorial 07 - Geometry Shader](Tutorials/Tutorial07_GeometryShader)

![](Tutorials/Tutorial07_GeometryShader/Animation_Large.gif)

This tutorial shows how to use geometry shader to render smooth wireframe.


## [Tutorial 08 - Tessellation](Tutorials/Tutorial08_Tessellation)

![](Tutorials/Tutorial08_Tessellation/Animation_Large.gif)

This tutorial shows how to use hardware tessellation to implement simple adaptive terrain 
rendering algorithm.


## [Tutorial 09 - Quads](Tutorials/Tutorial09_Quads)

![](Tutorials/Tutorial09_Quads/Animation_Large.gif)

This tutorial shows how to render multiple 2D quads, frequently switching textures and blend modes.


## [Tutorial 10 - Data Streaming](Tutorials/Tutorial10_DataStreaming)

![](Tutorials/Tutorial10_DataStreaming/Animation_Large.gif)

This tutorial shows dynamic buffer mapping strategy using `MAP_FLAG_DISCARD` and `MAP_FLAG_DO_NOT_SYNCHRONIZE`
flags to efficiently stream varying amounts of data to GPU.


## [Tutorial 11 - Resource Updates](Tutorials/Tutorial11_ResourceUpdates)

![](Tutorials/Tutorial11_ResourceUpdates/Animation_Large.gif)

This tutorial demonstrates different ways to update buffers and textures in Diligent Engine and explains 
important internal details and performance implications related to each method.


## [Tutorial 12 - Render Target](Tutorials/Tutorial12_RenderTarget)

![](Tutorials/Tutorial12_RenderTarget/Animation_Large.gif)

This tutorial demonstrates how to render a 3d cube into an offscreen render target and do a simple
post-processing effect.


## [Tutorial 13 - Shadow Map](Tutorials/Tutorial13_ShadowMap)

![](Tutorials/Tutorial13_ShadowMap/Animation_Large.gif)

This tutorial demonstrates how to render basic shadows using a shadow map.


## [Tutorial 14 - Compute Shader](Tutorials/Tutorial14_ComputeShader)

This tutorial shows how to implement a simple particle simulation system using compute shaders.

![](Tutorials/Tutorial14_ComputeShader/Animation_Large.gif)


## [Tutorial 15 - Multiple Windows](Tutorials/Tutorial15_MultipleWindows)

This tutorial demonstrates how to use Diligent Engine to render to multiple windows.

![](Tutorials/Tutorial15_MultipleWindows/Screenshot.png)


## [Tutorial 16 - Bindless Resources](Tutorials/Tutorial16_BindlessResources)

This tutorial shows how to implement bindless resources, a technique that leverages
dynamic shader resource indexing feature enabled by the next-gen APIs to significantly improve
rendering performance.

![](Tutorials/Tutorial16_BindlessResources/Animation_Large.gif)


## [Tutorial 17 - MSAA](https://github.com/DiligentGraphics/DiligentSamples/tree/master/Tutorials/Tutorial17_MSAA)

This tutorial demonstrates how to use multisample anti-aliasing (MSAA) to make
geometrical edges look smoother and more temporarily stable.

![](Tutorials/Tutorial17_MSAA/Animation_Large.gif)


## [Tutorial 18 - Queries](https://github.com/DiligentGraphics/DiligentSamples/tree/master/Tutorials/Tutorial18_Queries)

This tutorial demonstrates how to use queries to retrieve various information about
the GPU operation, such as the number of primitives rendered, command processing duration, etc.

![](Tutorials/Tutorial18_Queries/Animation_Large.gif)


## [Tutorial 19 - Render passes](https://github.com/DiligentGraphics/DiligentSamples/tree/master/Tutorials/Tutorial19_RenderPasses)

This tutorial demonstrates how to use the render passes API to implement simple deferred shading.

![](Tutorials/Tutorial19_RenderPasses/Animation_Large.gif)


## [Tutorial 20 - Mesh Shader](https://github.com/DiligentGraphics/DiligentSamples/tree/master/Tutorials/Tutorial20_MeshShader)

This tutorial demonstrates how to use amplification and mesh shaders, the new programmable stages, to implement
view frustum culling and object LOD calculation on the GPU.

![](Tutorials/Tutorial20_MeshShader/Animation_Large.gif)


## [Tutorial 21 - Ray Tracing](https://github.com/DiligentGraphics/DiligentSamples/tree/master/Tutorials/Tutorial21_RayTracing)

This tutorial demonstrates the basics of using ray tracing API in Diligent Engine.

![](Tutorials/Tutorial21_RayTracing/Animation_Large.gif)


## [Tutorial 22 - Hybrid Rendering](https://github.com/DiligentGraphics/DiligentSamples/tree/master/Tutorials/Tutorial22_HybridRendering)

This tutorial demonstrates how to implement a simple hybrid renderer that combines rasterization with ray tracing.

![](Tutorials/Tutorial22_HybridRendering/Animation_Large.gif)


## [Tutorial 23 - Command Queues](https://github.com/DiligentGraphics/DiligentSamples/tree/master/Tutorials/Tutorial23_CommandQueues)

This tutorial demonstrates how to use multiple command queues to perform rendering in parallel with copy and compute operations.

![](Tutorials/Tutorial23_CommandQueues/Animation_Large.gif)


## [Tutorial 24 - Variable Rate Shading](https://github.com/DiligentGraphics/DiligentSamples/tree/master/Tutorials/Tutorial24_VRS)

This tutorial demonstrates how to use variable rate shading to reduce the pixel shading load.

![](Tutorials/Tutorial24_VRS/Animation_Large.gif)


## [Tutorial 25 - Render State Packager](https://github.com/DiligentGraphics/DiligentSamples/tree/tutorial25/Tutorials/Tutorial25_StatePackager)

This tutorial shows how to create and archive pipeline states with the render state packager off-line tool
on the example of a simple path tracer.

![](Tutorials/Tutorial25_StatePackager/Screenshot.jpg)


## [Tutorial 26 - Render State Cache](https://github.com/DiligentGraphics/DiligentSamples/tree/master/Tutorials/Tutorial26_StateCache)

This tutorial expands the path tracing technique implemented in previous tutorial and demonstrates
how to use the render state cache to save pipeline states created at run time and load them when the
application starts.

![](Tutorials/Tutorial26_StateCache/Screenshot.jpg)


## [Tutorial 27 - Post-Processing](https://github.com/DiligentGraphics/DiligentSamples/tree/master/Tutorials/Tutorial27_PostProcessing)

This tutorial demonstrates how to use post-processing effects from the DiligentFX module.

![](Tutorials/Tutorial27_PostProcessing/Screenshot.jpg)


# Samples

## [Atmospheric Light Scattering sample](Samples/Atmosphere)

![](Samples/Atmosphere/Animation_Large.gif)

This sample demonstrates how to integrate
[Epipolar Light Scattering](https://github.com/DiligentGraphics/DiligentFX/tree/master/PostProcess/EpipolarLightScattering)
post-processing effect into an application to render physically-based atmosphere.


## [GLFW Demo](Samples/GLFWDemo)

![](Samples/GLFWDemo/Animation_Large.gif)

This simple maze mini-game demonstrates how to use GLFW to create window and handle keyboard and mouse input.


## [GLTF Viewer](Samples/GLTFViewer)

|||
|-----------------|-----------------|
| ![](https://github.com/DiligentGraphics/DiligentFX/blob/master/PBR/screenshots/damaged_helmet.jpg) | ![](https://github.com/DiligentGraphics/DiligentFX/blob/master/PBR/screenshots/flight_helmet.jpg) |
| ![](https://github.com/DiligentGraphics/DiligentFX/blob/master/PBR/screenshots/mr_spheres.jpg)     | ![](Samples/GLTFViewer/screenshots/cesium_man_large.gif)  |

This sample demonstrates how to use the [Asset Loader](https://github.com/DiligentGraphics/DiligentTools/tree/master/AssetLoader)
and [GLTF PBR Renderer](https://github.com/DiligentGraphics/DiligentFX/tree/master/PBR) to load and render GLTF models.


## [USD Viewer](Samples/USDViewer)

![](Samples/USDViewer/Screenshot.jpg)

This sample demonstrates how to render USD files using [Hydrogent](https://github.com/DiligentGraphics/DiligentFX/tree/master/Hydrogent),
an implementation of the Hydra rendering API in Diligent Engine.


## [Shadows](Samples/Shadows)

![](Samples/Shadows/Screenshot.jpg)

This sample demonstrates how to integrate the [Shadowing component](https://github.com/DiligentGraphics/DiligentFX/tree/master/Components#shadows)
into an application to render high-quality shadows.


## [Dear ImGui Demo](Samples/ImguiDemo)

![](Samples/ImguiDemo/Screenshot.png)

This sample demonstrates the integration of the engine with [dear imgui](https://github.com/ocornut/imgui) UI library.


## [Nuklear Demo](Samples/NuklearDemo)

![](Samples/NuklearDemo/Screenshot.png)

This sample demonstrates the integration of the engine with [nuklear](https://github.com/vurtun/nuklear) UI library.


## [Hello AR](Android/HelloAR)

![](Android/HelloAR/Screenshot.png)

This sample demonstrates how to use Diligent Engine in a basic Android AR application.


## [Asteroids](Samples/Asteroids)

![](Samples/Asteroids/Screenshot.png)

This sample is designed to be a performance benchmark and is based on
[this demo](https://software.intel.com/en-us/articles/asteroids-and-directx-12-performance-and-power-savings)
developed by Intel. It renders 50,000 unique textured asteroids. Every asteroid is a combination of one of 1000
unique meshes and one of 10 unique textures. The sample uses original D3D11 and D3D12 native implementations,
and adds implementations using Diligent Engine API to allow comparing performance of different rendering modes.

## [Unity Plugin](UnityPlugin)

![](UnityPlugin/GhostCubePlugin/Screenshot.png)

This project demonstrates integration of Diligent Engine with Unity.

# Build and Run Instructions

Please refer to Build and Run Instructions section in the
[master repository's readme](https://github.com/DiligentGraphics/DiligentEngine/blob/master/README.md#build-and-run-instructions).

Command line options:

* **--mode**, **-m** {*d3d11*|*d3d12*|*vk*|*gl*} - select rendering back-end (example: *--mode d3d12*, *-m vk*).
* **--width**, **-w** *value* - set desired window width (example: *--width 1024*, *-w 1280*).
* **--height**, **-h** *value* - set desired window height (example: *--height 768*, *-h 1024*).
* **--capture_path** *path* - path to the folder where screen captures will be saved. Specifying this parameter enables screen capture (example: *--capture_path .*).
* **--capture_name** *name* - screen capture file name. Specifying this parameter enables screen capture (example: *--capture_name frame*).
* **--capture_fps** *fps*   - recording fps when capturing frame sequence (example: *--capture_fps 10*). Default value: 15.
* **--capture_frames** *value* - number of frames to capture after the app starts (example: *--capture_frames 50*).
* **--capture_format** {*jpg*|*png*} - image file format (example: *--capture_format jpg*). Default value: jpg.
* **--capture_quality** *value* - jpeg quality (example: *--capture_quality 80*). Default value: 95.
* **--capture_alpha** *value* - when saving png, whether to write alpha channel (example: *--capture_alpha 1*). Default value: false.
* **--validation** *value* - set validation level (example: *--validation 1*). Default value: 1 in debug build; 0 in release builds.
* **--adapter** *value* - select GPU adapter, if there are more than one installed on the system (example: *--adapter 1*). Default value: 0.
* **--adapters_dialog** *value* - whether to show adapters dialog (example: *--adapters_dialog 0*). Default value: 1.
* **--show_ui** *value* - whether to show user interface (example: *--show_ui 0*). Default value: 1.
* **--golden_image_mode** {*none*|*capture*|*compare*|*compare_update*} - golden image capture mode. Default value: none.
* **--golden_image_tolerance** *value* - golden image comparison tolerance. Default value: 0.
* **--non_separable_progs** *value* - force non-separable programs in GL

When image capture is enabled the following hot keys are available:

* **F2** starts frame capture recording.
* **F3** finishes frame capture recroding.
* **F4** saves single screenshot.

To record multiple frames after the app starts, use command line like this:

```
--mode d3d12 --capture_path . --capture_fps 15 --capture_name frame --width 640 --height 480 --capture_format png --capture_frames 50
```

# License

See [Apache 2.0 license](License.txt).

This project has some third-party dependencies, each of which may have independent licensing:

* [nuklear](https://github.com/vurtun/nuklear): A single-header ANSI C gui library ([MIT or Public domain license](https://github.com/DiligentGraphics/nuklear/blob/master/Readme.md)).
* [glfw](https://github.com/glfw/glfw): A multi-platform library for OpenGL, OpenGL ES, Vulkan, window and input ([zlib License](https://github.com/DiligentGraphics/glfw/blob/master/LICENSE.md)).
* [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo): Immediate mode 3D gizmo for scene editing and other controls based on Dear Imgui ([MIT](https://github.com/CedricGuillemet/ImGuizmo/blob/master/LICENSE)).


<a name="contributing"></a>
# Contributing

To contribute your code, submit a [Pull Request](https://github.com/DiligentGraphics/DiligentSamples/pulls) 
to this repository. **Diligent Engine** is licensed under the [Apache 2.0 license](License.txt) that guarantees 
that content in the **DiligentSamples** repository is free of Intellectual Property encumbrances.
In submitting any content to this repository,
[you license that content under the same terms](https://docs.github.com/en/free-pro-team@latest/github/site-policy/github-terms-of-service#6-contributions-under-repository-license),
and you agree that the content is free of any Intellectual Property claims and you have the right to license it under those terms. 

Diligent Engine uses [clang-format](https://clang.llvm.org/docs/ClangFormat.html) to ensure
consistent source code style throughout the code base. The format is validated by CI
for each commit and pull request, and the build will fail if any code formatting issue is found. Please refer
to [this page](https://github.com/DiligentGraphics/DiligentCore/blob/master/doc/code_formatting.md) for instructions
on how to set up clang-format and automatic code formatting.

------------------------------

[diligentgraphics.com](http://diligentgraphics.com)

[![Diligent Engine on Twitter](https://github.com/DiligentGraphics/DiligentCore/blob/master/media/twitter.png)](https://twitter.com/diligentengine)
[![Diligent Engine on Facebook](https://github.com/DiligentGraphics/DiligentCore/blob/master/media/facebook.png)](https://www.facebook.com/DiligentGraphics/)
