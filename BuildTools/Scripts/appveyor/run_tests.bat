setlocal ENABLEDELAYEDEXPANSION

set ERROR=0

if "%PLATFORM_NAME%"=="Windows" (
    rem  call is required here because otherwise exit /b command in the bat file will terminate this shell too
    call ProcessGoldenImages.bat %1 %CONFIGURATION% compare d3d11 d3d12 || set ERROR=!errorlevel!
)

if "%PLATFORM_NAME%"=="Windows8.1" (
    call ProcessGoldenImages.bat %1 %CONFIGURATION% compare d3d11 || set ERROR=!errorlevel!
)

exit /B %ERROR%
