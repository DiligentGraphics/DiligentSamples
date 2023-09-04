# USD Viewer

## Build Instructions

1. Download and build [OpenUSD](https://github.com/PixarAnimationStudios/OpenUSD)
    *  If using Visual Studio, build each configuration in a respective folder, for example:
        ```
        python .\build_scripts\build_usd.py --build-variant debug .\build\Debug
        python .\build_scripts\build_usd.py --build-variant release .\build\Release
        python .\build_scripts\build_usd.py --build-variant relwithdebuginfo .\build\RelWithDebInfo
        ```

2. Provide path to USD install folder with `DILIGENT_USD_PATH` CMake variable, for example:
   `-DDILIGENT_USD_PATH=c:\GitHub\OpenUSD\build`

3. Build the engine by following [standard instructions](https://github.com/DiligentGraphics/DiligentEngine#build-and-run-instructions)

## Run Instructions

To run the application on Windows, the following paths must be added to `PATH` environment variable for the application to find required DLLs:

```
${DILIGENT_USD_PATH}/lib
${DILIGENT_USD_PATH}/bin
```

When running the application from Visual Studio, the paths are automatically added to debugger environment.
