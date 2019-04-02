# DiligentSamples <img src="https://github.com/DiligentGraphics/DiligentCore/blob/master/media/diligentgraphics-logo.png" height=64 align="right" valign="middle">


This module contains tutorials and sample applications intended to demonstrate the usage of [Diligent Engine](https://github.com/DiligentGraphics/DiligentEngine). The module depends on the [Core](https://github.com/DiligentGraphics/DiligentCore) and [Tools](https://github.com/DiligentGraphics/DiligentTools) submodules.

To build and run the applications in the module, please follow the [instructions](https://github.com/DiligentGraphics/DiligentEngine#build-and-run-instructions) in the master repository.

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](License.txt)
[![Chat on gitter](https://badges.gitter.im/gitterHQ/gitter.png)](https://gitter.im/diligent-engine)


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

![](Tutorials/Tutorial08_Tessellation/Screenshot.png)

This tutorial shows how to use hardware tessellation to implement simple adaptive terrain 
rendering algorithm.

## [Tutorial 09 - Quads](Tutorials/Tutorial09_Quads)

![](Tutorials/Tutorial09_Quads/Screenshot.png)

This tutorial shows how to render multiple 2D quads, frequently swithcing textures and blend modes.


## [Tutorial 10 - Data Streaming](Tutorials/Tutorial10_DataStreaming)

![](Tutorials/Tutorial10_DataStreaming/Screenshot.png)

This tutorial shows dynamic buffer mapping strategy using `MAP_FLAG_DISCARD` and `MAP_FLAG_DO_NOT_SYNCHRONIZE`
flags to efficiently stream varying amounts of data to GPU.

## [Tutorial 11 - Resource Updates](Tutorials/Tutorial11_ResourceUpdates)

![](Tutorials/Tutorial11_ResourceUpdates/Screenshot.png)

This tutorial demonstrates different ways to update buffers and textures in Diligent Engine and explains 
important internal details and performance implications related to each method.

## [Tutorial 12 - Render Target](Tutorials/Tutorial12_RenderTarget)

![](Tutorials/Tutorial12_RenderTarget/Screenshot.png)

This tutorial demonstrates how to render a 3d cube into an offscreen render target and do a simple
post-processing effect.

# Samples

## [AntTweakBar Sample](Samples/AntTweakBar)

![](Samples/AntTweakBar/Screenshot.png)

This sample demonstrates how to use [AntTweakBar library](http://anttweakbar.sourceforge.net/doc)
to create simple user interface. 

## [Atmospheric Light Scattering sample](Samples/Atmosphere)

![](Samples/Atmosphere/Screenshot.png)

This sample demonstrates how to integrate
[Epipolar Light Scattering](https://github.com/DiligentGraphics/DiligentFX/tree/master/Postprocess/EpipolarLightScattering)
post-processing effect into an application to render physically-based atmosphere.

# Build and Run Instructions

Please refer to Build and Run Instructions section of the
[master repository's readme](https://github.com/DiligentGraphics/DiligentEngine/blob/master/README.md#build-and-run-instructions).

Command line options:

* **-mode** {*d3d11*|*d3d12*|*vk*|*gl*} - select rendering back-end (exmple: *-mode d3d12*).
* **-width** <value> - set desired window width (example: *-width 1023*).
* **-height** <value> - set desired window height (example: *-height 768*).
* **-capture_path** <path> - path to the folder where screen captures will be saved. Specifying this parameter enables screen capture (example: *-capture_path .*).
* **-capture_name** <name> - screen capture file name. Specifying this parameter enables screen capture (example: *-capture_name frame*).
* **-capture_fps** <fps>   - recording fps when capturing frame sequence (example: *-capture_fps 15*). Default value: 15.
* **-capture_frames** <value> - number of frames to capture after the app starts (example: *-capture_frames 50*).
* **-capture_format** {*jpg*|*png*} - image file format (example: *-capture_format jpg*). Default value: jpg.
* **-capture_quality** <value> - jpeg quality (example: *-capture_quality 80*). Default value: 95.

When image capture is enabled the following hot keys are available:

* F2 starts frame capture recording.
* F3 finishes frame capture recroding.
* F4 saves single screenshot.

To record multiple frames after the app starts, use command line like this:

```
-mode d3d12 -capture_path . -capture_fps 15 -capture_name frame -width 640 -height 480 -capture_format png -capture_frames 50
```

<a name="contributing"></a>
# Contributing

To contribute your code, submit a [Pull Request](https://github.com/DiligentGraphics/DiligentCore/pulls) 
to this repository. **Diligent Engine** is licensed under the [Apache 2.0 license](License.txt) that guarantees 
that code in the **DiligentCore** repository is free of Intellectual Property encumbrances. In submitting code to
this repository, you are agreeing that the code is free of any Intellectual Property claims.  

# Version History

## v2.4.a

* Added rendering backend selection dialog on Win32 and Mac

## v2.3.a

* Added Tutorial 10 - Streaming
* Added Tutorial 11 - Resource Updates

## v2.3

* Implemented Vulkan backend
* Added fullscreen mode selection dialog box
* Implemented fullscreen mode toggle on UWP with shift + enter
* Implemented fullscreen window toggle on Win32 with alt + enter
* Added tutorial 09 - Quads

## v2.2

* Enabled MacOS and iOS platforms
* Fixed multiple issues with OpenGL/GLES

## v2.1.b

* Removed legacy VS projects and solutions
* Added tutorials:
  * 01 - Hello Triangle
  * 02 - Cube
  * 03 - Texturing
  * 04 - Instancing
  * 05 - Texture Array
  * 06 - Multithreading
  * 07 - Geometry Shader
  * 08 - Tessellation

## v2.1.a

* Refactored build system to use CMake and Gradle for Android
* Added support for Linux platform

## v2.1

## v2.0.alpha

## v1.0.0

Initial release


------------------------------

[diligentgraphics.com](http://diligentgraphics.com)

[![Diligent Engine on Twitter](https://github.com/DiligentGraphics/DiligentCore/blob/master/media/twitter.png)](https://twitter.com/diligentengine)
[![Diligent Engine on Facebook](https://github.com/DiligentGraphics/DiligentCore/blob/master/media/facebook.png)](https://www.facebook.com/DiligentGraphics/)
