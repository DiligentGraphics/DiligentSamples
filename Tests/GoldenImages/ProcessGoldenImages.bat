@echo off

rem %1: path to the root of the build tree
rem %2: configuration (Debug, Release, etc.)
rem %3: mode (capture or compare)
rem %4... : rendering backends to run

rem Enable delayed expansion to be able to use !ERRORLEVEL!
setlocal ENABLEDELAYEDEXPANSION

set img_width=512
set img_height=512

set build_folder=%1
shift

set config=%1
shift

set golden_img_mode=%1
shift

set rest_args=
:loop1
    if "%1"=="" goto end_loop1
    set rest_args=%rest_args% %1
    shift
    goto loop1
:end_loop1

cd ../..

set Tutorials=Tutorial01_HelloTriangle^
              Tutorial02_Cube^
              Tutorial03_Texturing^
              Tutorial04_Instancing^
              Tutorial05_TextureArray^
              Tutorial06_Multithreading^
              Tutorial07_GeometryShader^
              Tutorial08_Tessellation^
              Tutorial09_Quads^
              Tutorial10_DataStreaming^
              Tutorial11_ResourceUpdates^
              Tutorial12_RenderTarget^
              Tutorial13_ShadowMap^
              Tutorial14_ComputeShader^
              Tutorial16_BindlessResources^
              Tutorial17_MSAA

set Samples=Atmosphere^
            GLTFViewer^
            ImguiDemo^
            NuklearDemo^
            Shadows

set ERROR=0
set APP_ID=1
for %%X in (%Tutorials%) do (
    call :gen_golden_img Tutorials %%X %rest_args% || set /a ERROR=!ERROR!+!APP_ID!
    set /a APP_ID=!APP_ID!*2
)

for %%X in (%Samples%) do (
    call :gen_golden_img Samples %%X %rest_args% || set /a ERROR=!ERROR!+!APP_ID!
    set /a APP_ID=!APP_ID!*2
)

cd Tests/GoldenImages

EXIT /B %ERROR%



:gen_golden_img

    set app_folder=%1
    shift

    set app_name=%1
    shift

    set show_ui=1
    if "%app_folder%" == "Samples" (
        set show_ui=0
        if "%app_name%" == "ImguiDemo" set show_ui=1
    )

    set backends=
    :loop2
        if "%1"=="" goto end_loop2
        set backends=%backends% %1
        shift
        goto loop2
    :end_loop2

    set golden_img_dir=%app_folder%/%app_name%/golden_images
    if not exist "%golden_img_dir%" (
        md "%golden_img_dir%"
    )
   
    cd "%app_folder%/%app_name%/assets"

    set EXIT_CODE=0
    for %%X in (%backends%) do (

        set app_path=%build_folder%/DiligentSamples/%app_folder%/%app_name%/%config%/%app_name%.exe
        set capture_name=%app_name%_gi_%%X

        REM !!!   ERRORLEVEL doesn't get updated inside control blocks like IF statements unless you use  !!!
        REM !!!   !ERRORLEVEL! instead of %ERRORLEVEL% and use this command at the start of your code:    !!!
        REM !!!   setlocal ENABLEDELAYEDEXPANSION                                                         !!!
        !app_path! -mode %%X -adapter sw -width %img_width% -height %img_height% -golden_image_mode %golden_img_mode% -capture_path ../golden_images -capture_name !capture_name! -capture_format png -adapters_dialog 0 -show_ui %show_ui%

        if "%golden_img_mode%" == "compare" (
            if !ERRORLEVEL! NEQ 0 (
                echo Golden image validation failed for %app_name%-%%X: !ERRORLEVEL! incosistent pixels found
                set EXIT_CODE=1
                ) else (
                echo Golden image validation passed for %app_name%-%%X
            )
        )
        
        if "%golden_img_mode%" == "capture" (
            if !ERRORLEVEL! NEQ 0 (
                echo Failed to generate golden image %app_name%-%%X
                set EXIT_CODE=1
                ) else (
                echo Successfully generated golden image for %app_name%-%%X
            )
        )
		echo.
    )

    cd ../../../

    EXIT /B %EXIT_CODE%
