if "%PLATFORM_NAME%"=="Windows" (
    rem  Note: 'exit /b' command in the bat file will terminate this shell too 
	rem  (which is what we want in this case).
	rem  We can run the script with 'call' to keep this shell running.
    ProcessGoldenImages.bat %1 %CONFIGURATION% compare d3d11 d3d12
)

if "%PLATFORM_NAME%"=="Windows8.1" (
    ProcessGoldenImages.bat %1 %CONFIGURATION% compare d3d11
)
