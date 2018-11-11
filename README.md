# DiligentSamples <img src="https://github.com/DiligentGraphics/DiligentCore/blob/master/media/diligentgraphics-logo.png" height=64 align="right" valign="middle">


This module contains tutorials and sample applications intended to demonstrate the usage of [Diligent Engine](https://github.com/DiligentGraphics/DiligentEngine). The module depends on the [Core](https://github.com/DiligentGraphics/DiligentCore) and [Tools](https://github.com/DiligentGraphics/DiligentTools) submodules.

To build and run the applications in the module, please follow the [instructions](https://github.com/DiligentGraphics/DiligentEngine/blob/master/README.md) in the master repository.

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](License.txt)
[![Chat on gitter](https://badges.gitter.im/gitterHQ/gitter.png)](https://gitter.im/diligent-engine)


# Tutorials

## [Tutorial 01 - Hello Triangle](Tutorials/Tutorial01_HelloTriangle)

![](Tutorials/Tutorial01_HelloTriangle/Screenshot.png)

This tutorial shows how to render a simple triangle using Diligent Engine API.

## [Tutorial 02 - Cube](Tutorials/Tutorial02_Cube)

![](Tutorials/Tutorial02_Cube/Screenshot.png)

This tutorial demonstrates how to render an actual 3D object, a cube. It shows how to load shaders from files, create and use vertex, 
index and uniform buffers.

## [Tutorial 03 - Texturing](Tutorials/Tutorial03_Texturing)

![](Tutorials/Tutorial03_Texturing/Screenshot.png)

This tutorial demonstrates how to apply a texture to a 3D object. It shows how to load a texture from file, create shader resource
binding object and how to sample a texture in the shader.

## [Tutorial 04 - Instancing](Tutorials/Tutorial04_Instancing)

![](Tutorials/Tutorial04_Instancing/Screenshot.png)

This tutorial demonstrates how to use instancing to render multiple copies of one object
using unique transformation matrix for every copy.

## [Tutorial 05 - Texture Array](Tutorials/Tutorial05_TextureArray)

![](Tutorials/Tutorial05_TextureArray/Screenshot.png)

This tutorial demonstrates how to combine instancing with texture arrays to 
use unique texture for every instance.

## [Tutorial 06 - Multithreading](Tutorials/Tutorial06_Multithreading)

![](Tutorials/Tutorial06_Multithreading/Screenshot.png)

This tutorial shows how to generate command lists in parallel from multiple threads.

## [Tutorial 07 - Geometry Shader](Tutorials/Tutorial07_GeometryShader)

![](Tutorials/Tutorial07_GeometryShader/Screenshot.png)

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


# Samples

## [AntTweakBar Sample](Samples/AntTweakBar)

![](Samples/AntTweakBar/Screenshot.png)

This sample demonstrates how to use [AntTweakBar library](http://anttweakbar.sourceforge.net/doc)
to create simple user interface. 

## [Atmospheric Light Scattering sample](Samples/Atmosphere)

![](Samples/Atmosphere/Screenshot.png)

The sample implements physically-based atmospheric light scattering model and demonstrates how 
Diligent Engine can be used to accomplish various rendering tasks: loading textures from files,
using complex shaders, rendering to textures, using compute shaders and unordered access views, etc.

# Build and Run Instructions

Please refer to Build and Run Instructions section of the
[master repository's readme](https://github.com/DiligentGraphics/DiligentEngine/blob/master/README.md#build-and-run-instructions).

# Version History

## Current progress

* Added Tutorial 10 - Streaming

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


**Copyright 2015-2018 Egor Yusov**
