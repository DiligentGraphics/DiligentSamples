# USD Viewer

This sample demonstrates how to render USD files using [Hydrogent](https://github.com/DiligentGraphics/DiligentFX/tree/master/Hydrogent),
an implementation of the Hydra rendering API in Diligent Engine.

![Screenshot](Screenshot.jpg)

Please follow [these instructions](https://github.com/DiligentGraphics/DiligentFX/tree/master/Hydrogent#build-instructions) to build Hydrogent and the sample.

If you built the [USD File Format Plugins](https://github.com/adobe/USD-Fileformat-plugins), you may specify the 
`DILIGENT_USD_FILEFORMAT_PLUGINS_PATH` CMake variable to let CMake configure the Visual Studio debugging environment
to find the plugins, for example:

```
-DDILIGENT_USD_FILEFORMAT_PLUGINS_PATH=C:\GitHub\USD-Fileformat-plugin\bin
```
