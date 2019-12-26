@echo off

if "%PLATFORM_NAME%"=="Windows" (
    call ProcessGoldenImages.bat %1 %CONFIGURATION% %2 d3d11 d3d12
)

if "%PLATFORM_NAME%"=="Windows8.1" (
    call ProcessGoldenImages.bat %1 %CONFIGURATION% %2 d3d11
)
