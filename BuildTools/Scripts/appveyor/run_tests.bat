@echo off
setlocal ENABLEDELAYEDEXPANSION

set ERROR=0

if "%PLATFORM_NAME%"=="Windows" (
    call ProcessGoldenImages.bat %1 %CONFIGURATION% %2 d3d11 d3d12 || set ERROR=!ERRORLEVEL!
)

if "%PLATFORM_NAME%"=="Windows8.1" (
    call ProcessGoldenImages.bat %1 %CONFIGURATION% %2 d3d11 || set ERROR=!ERRORLEVEL!
)

exit /B %ERROR%
