This module contains a number of simple graphics applications intended to demonstrate the usage of the Diligent Engine API. For information on how to build and run the samples and for general info about the engine, please visit [diligentgraphics.com/diligent-engine/samples/](http://diligentgraphics.com/diligent-engine/samples/)

## Version History

### v1.0.0

Initial release

### v2.0.alpha

Alpha release of Diligent Engine 2.0. The engine has been updated to take advantages of Direct3D12:

* Pipeline State Object encompasses all coarse-grain state objects like Depth-Stencil State, Blend State, Rasterizer State, shader states etc.
* New shader resource binding model implemented to leverage Direct3D12

Release notes:

* Diligent Engine 2.0 also implements OpenGL and Direct3D11 back-ends
* Alpha release is only available on Windows platform
* Direct3D11 back-end is very thoroughly optimized and has very low overhead compared to native D3D11 implementation
* Direct3D12 implementation, to the contrary, is preliminary and not yet optimized.

For more details on the release, please visit [diligentgraphics.com](http://diligentgraphics.com/2016/03/17/diligent-engine-2-0-powered-by-direct3d12/)

## Running the samples

* To run a sample from Visual Studio, locate the sample’s project in the solution explorer, go to **Project->Properties->Debugging** and set Working Directory to **$(AssetsPath)** (This variable is defined in the EngineRoot.props property page file located in the sample’s build\Windows folder). Then set the project as startup project and run it. By default the sample will run in OpenGL mode. You can also specify the following command line: **"mode=GL"**. To use Direct3D11, add the following command line option: **"mode=D3D11"** (do not use spaces!). To run in Direct3D12, use the following command line: **"mode=D3D12"**.
* To run a sample from windows explorer or command line, navigate to the sample’s **build\Windows** folder and execute the appropriate run_*.bat file (you need to build the corresponding configuration first). 

## License
Copyright 2015-2017 Egor Yusov.
Licensed under the [Apache License, Version 2.0](License.txt)