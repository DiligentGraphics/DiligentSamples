# GLFW Demo

This sample demonstrates how to use [GLFW](https://www.glfw.org/) to create window and handle keyboard and mouse input instead of Diligent-NativeAppBase.

GLFW supports only OpenGL (ES) and Vulkan.
To use all available graphics APIs we should create GLFW window with GLFW_NO_API flag and create swapchain using DiligentEngine methonds.
On MacOS we additionaly create CAMetalLayer to allow rendering into view using Metal API.

