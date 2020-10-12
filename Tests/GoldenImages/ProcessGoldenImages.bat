@echo off

set num_args=0
for %%x in (%*) do Set /A num_args+=1

if "%num_args%" LSS "5" (
    echo Command line format:
    echo.
    echo   ProcessGoldenImages.bat build_path config mode extra_args backends
    echo.
    echo build_path - path to the root of the build tree
    echo config     - configuration (Debug, Release, etc.^)
    echo mode       - golden image processing mode (capture or compare^)
    echo extra_args - extra arguments for command line (e.g. "-adater sw"^)
    echo backends   - list of rendering backends (e.g. d3d11 d3d12 gl vk^)
    echo. 
    echo Example:
    echo   ProcessGoldenImages.bat c:\Projects\DiligentEngine\build\Win64 Debug compare "-adapter sw" d3d11 d3d12
    EXIT /B -1
)


rem  Enable delayed expansion to be able to use !ERRORLEVEL!
setlocal ENABLEDELAYEDEXPANSION

if "%golden_img_width%" == "" (
    set golden_img_width=512
)
if "%golden_img_height%" == "" (
    set golden_img_height=512
)
if "%golden_images_root_dir%" == "" (
    set golden_images_root_dir=../../../Tests/GoldenImages
)

set build_folder=%1
shift

set config=%1
shift

set golden_img_mode=%1
shift

set extra_agrs=%~1  :: ~ removes surrounding quotes
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
              Tutorial03_Texturing-C^
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
              Tutorial17_MSAA^
			  Tutorial18_Queries^
              Tutorial19_RenderPasses

set Samples=Atmosphere^
            GLTFViewer^
            NuklearDemo^
            Shadows

rem  ImguiDemo has fps counter in the UI, so we have to skip it

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

    rem  Who knows why, but without the echos below, the script fails on Appveyor.
    @echo Testing %app_folder%/%app_name%...
    @echo.

    set show_ui=1
    if "%app_folder%" == "Samples" (
        set show_ui=0
        if "%app_name%" == "ImguiDemo" set show_ui=1
    )
	if "%app_name%" == "Tutorial18_Queries" set show_ui=0
	
    set backends=
    :loop2
        if "%1"=="" goto end_loop2
        set backends=%backends% %1
        shift
        goto loop2
    :end_loop2

    cd "%app_folder%/%app_name%/assets"

    set golden_img_dir=%golden_images_root_dir%/%app_folder%/%app_name%
    if not exist "%golden_img_dir%" (
        md "%golden_img_dir%"
    )

    set EXIT_CODE=0
    for %%X in (%backends%) do (

        set app_path=%build_folder%/DiligentSamples/%app_folder%/%app_name%/%config%/%app_name%.exe
        set capture_name=%app_name%_gi_%%X

        rem   !!!   ERRORLEVEL doesn't get updated inside control blocks like IF statements unless           !!!
        rem   !!!   !ERRORLEVEL! is used instead of %ERRORLEVEL% and delayed expansion is enabled as below:  !!!
        rem   !!!   setlocal ENABLEDELAYEDEXPANSION                                                          !!!
        !app_path! -mode %%X %extra_agrs% -width %golden_img_width% -height %golden_img_height% -golden_image_mode %golden_img_mode% -capture_path %golden_img_dir% -capture_name !capture_name! -capture_format png -adapters_dialog 0 -show_ui %show_ui%

        if !ERRORLEVEL! NEQ 0 (
            if !ERRORLEVEL! GEQ 0 (
                echo Golden image validation failed for %app_name% [%%X]: !ERRORLEVEL! incosistent pixels found
                if "%golden_img_mode%" == "compare_update" echo Golden image updated
            ) else (
                if "%golden_img_mode%" == "compare" echo Golden image validation failed for %app_name% [%%X]
                if "%golden_img_mode%" == "capture" echo Failed to generate golden image for %app_name% [%%X]
                if "%golden_img_mode%" == "compare_update" echo Unable to validate or update golden image for %app_name% [%%X]
            )
            set EXIT_CODE=1
        ) else (
            if "%golden_img_mode%" == "compare" echo Golden image validation passed for %app_name% [%%X]
            if "%golden_img_mode%" == "capture" echo Successfully generated golden image for %app_name% [%%X]
            if "%golden_img_mode%" == "compare_update" echo Golden image validation passed for %app_name%. Image updated. [%%X]
        )

		echo.
        echo.
    )

    cd ../../../

    EXIT /B %EXIT_CODE%
