# AtmosphereSample

![](Screenshot.png)

This sample demonstrates how Diligent Engine can be used to implement various rendering tasks: 
loading textures from files, using complex shaders, rendering to textures, using compute shaders and unordered access views, etc. 

# Build And Run Instructions

## Windows Desktop

Open **EngineAll.sln** solution located in [DiligentGraphics/DiligentEngine/build/Win32](https://github.com/DiligentGraphics/DiligentEngine/tree/master/build/Win32) 
folder, select Win32 or x64 platform, and build the solution. Note that ARM platform is intended to run Android build.

To run the sample from Visual Studio, use the following debugging properties:

* Working Directory: **$(AssetsPath)**
* Command Arguments: **mode={D3D11|D3D12|GL}** (no spaces!)
 
To run the sample from windows explorer or command line, navigate to the sample’s build/Win32 folder and execute the 
appropriate run_*.bat file (you need to build the corresponding configuration first). Do not execute run.bat!

## Universal Windows Platform

Open **EngineAll.sln** solution located in [DiligentGraphics/DiligentEngine/build/UWP](https://github.com/DiligentGraphics/DiligentEngine/tree/master/build/UWP) 
folder, select the desired platform, and build the solution.

## Android

Option I:

1. cd to Atmosphere\build\Win32 folder
2. First time only, initialize the project (do not miss the dot at the end of the command):
   *android update project -p .*
3. *ndk-build*
4. Build the APK
   *ant debug*
5. Install the APK
   *adb install -r ./bin/EngineSampleNativeActivity-debug.apk*
6. Run the app on the device

Option II: instal [Visual GDB Plugin](https://visualgdb.com/), open **EngineAll.sln** solution 
located in [DiligentGraphics/DiligentEngine/build/Win32](https://github.com/DiligentGraphics/DiligentEngine/tree/master/build/Win32) 
folder, select ARM platform and build the solution


Please visit [this page](http://diligentgraphics.com/diligent-engine/getting-started) for detailed build instructions.


**Copyright 2015-2017 Egor Yusov**
