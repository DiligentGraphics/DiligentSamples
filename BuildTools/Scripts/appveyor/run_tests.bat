@echo off

if "%PLATFORM_NAME%"=="Windows" (
	ProcessGoldenImages.bat %1 %CONFIGURATION% compare d3d11 d3d12
)

if "%PLATFORM_NAME%"=="Windows8.1" (
    ProcessGoldenImages.bat %1 %CONFIGURATION% compare d3d11
)
