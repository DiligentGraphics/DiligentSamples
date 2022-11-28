@echo off

rem !!! DO NOT USE :: for comments as it misbehaves and is parsed as drive letter !!!

rem Enable delayed expansion to be able to use !ERRORLEVEL!
setlocal ENABLEDELAYEDEXPANSION
setlocal

for /F %%d in ('echo prompt $E ^| cmd') do (set "ESC=%%d")

set FONT_RED=%ESC%[91m
set FONT_GREEN=%ESC%[92m
set FONT_YELLOW=%ESC%[93m
set FONT_DEFAULT=%ESC%[0m

set num_args=0
for %%x in (%*) do Set /A num_args+=1

if "%num_args%" LSS "3" (
    echo %FONT_RED%At least three arguments are required%FONT_DEFAULT%
    goto help
)

if "%DILIGENT_BUILD_TYPE%" == "" (
    echo %FONT_RED%Required DILIGENT_BUILD_TYPE variable is not set%FONT_DEFAULT%
    goto help
)

if "%DILIGENT_BUILD_DIR%" == "" (
    echo %FONT_RED%Required DILIGENT_BUILD_DIR variable is not set%FONT_DEFAULT%
    goto help
)

if "%GOLDEN_IMAGE_WIDTH%" == "" (
    set GOLDEN_IMAGE_WIDTH=512
)
if "%GOLDEN_IMAGE_HEIGHT%" == "" (
    set GOLDEN_IMAGE_HEIGHT=512
)

rem ~ removes surrounding quotes
set golden_images_dir=%~1
shift

set golden_img_mode=%~1
shift

set golden_img_mode_ok=
if "%golden_img_mode%" == "capture" (set golden_img_mode_ok=OK)
if "%golden_img_mode%" == "compare" (set golden_img_mode_ok=OK)
if "%golden_img_mode%" == "compare_update" (set golden_img_mode_ok=OK)
if NOT "%golden_img_mode_ok%" == "OK" (
    echo %FONT_RED%%golden_img_mode% is not a valid golden image mode%FONT_DEFAULT%
    goto help
)

set test_modes=
:loop1
    rem ~ removes surrounding quotes
    if "%~1"=="" goto end_loop1
    rem Do not remove quotes yet as this will ungroup arguments
    set test_modes=%test_modes% %1
    shift
    goto loop1
:end_loop1


echo Build type:  %DILIGENT_BUILD_TYPE%
echo Build dir:   %DILIGENT_BUILD_DIR%
echo Img mode:    %golden_img_mode%
echo Img dir:     %golden_images_dir%
echo Img size:    %GOLDEN_IMAGE_WIDTH% x %GOLDEN_IMAGE_HEIGHT%
echo Test modes:  %test_modes%
echo.

cd ..

set TestApps="Tutorials/Tutorial01_HelloTriangle"^
             "Tutorials/Tutorial02_Cube"^
             "Tutorials/Tutorial03_Texturing"^
             "Tutorials/Tutorial03_Texturing-C"^
             "Tutorials/Tutorial04_Instancing"^
             "Tutorials/Tutorial05_TextureArray"^
             "Tutorials/Tutorial06_Multithreading"^
             "Tutorials/Tutorial07_GeometryShader"^
             "Tutorials/Tutorial08_Tessellation"^
             "Tutorials/Tutorial09_Quads"^
             "Tutorials/Tutorial10_DataStreaming"^
             "Tutorials/Tutorial11_ResourceUpdates"^
             "Tutorials/Tutorial12_RenderTarget"^
             "Tutorials/Tutorial13_ShadowMap"^
             "Tutorials/Tutorial14_ComputeShader"^
             "Tutorials/Tutorial16_BindlessResources"^
             "Tutorials/Tutorial17_MSAA"^
			 "Tutorials/Tutorial18_Queries --show_ui 0"^
             "Tutorials/Tutorial19_RenderPasses"^
             "Tutorials/Tutorial23_CommandQueues --show_ui 0"^
             "Tutorials/Tutorial25_StatePackager --show_ui 0"^
             "Tutorials/Tutorial26_StateCache --show_ui 0"^
             "Tutorials/Tutorial26_StateCache --show_ui 0"^
             "Samples/Atmosphere --show_ui 0"^
             "Samples/GLTFViewer --show_ui 0"^
             "Samples/NuklearDemo --show_ui 0"^
             "Samples/Shadows --show_ui 0"

rem  ImguiDemo has fps counter in the UI, so we have to skip it

set TESTS_FAILED=0
set TESTS_PASSED=0
set TESTS_SKIPPED=0
set TEST_ID=0

for %%X in (%TestApps%) do (
    call :gen_golden_img %%X
)

for %%X in (%ADDITIONAL_TEST_APPS%) do (
    call :gen_golden_img %%X
)

cd Tests

for /l %%i in (1,1,!TEST_ID!) do ( 
   echo !TEST_STATUS[%%i]!
)

echo.
if "%TESTS_PASSED%" NEQ "0" (
    echo %FONT_GREEN%!TESTS_PASSED! tests PASSED%FONT_DEFAULT%
)

if "%TESTS_FAILED%" NEQ "0" (
    echo %FONT_RED%!TESTS_FAILED! tests FAILED%FONT_DEFAULT%
)

if "%TESTS_SKIPPED%" NEQ "0" (
    echo %FONT_YELLOW%!TESTS_SKIPPED! tests SKIPPED%FONT_DEFAULT%
)

EXIT /B !TESTS_FAILED!

rem For some reason, colored font does not work after the line that starts the sample app
:print_colored
    echo %~1
    EXIT /B 0



:gen_golden_img
    rem https://ss64.com/nt/for_f.html
    rem Tutorials/Tutorial18_Queries --show_ui 0
    rem |-------| |----------------| |---------| 
    rem     ^            ^                ^
    rem     1            2                *
    for /F "tokens=1,2,* delims=/ " %%a in (%1) do (
        set app_folder=%%a
        set app_name=%%b
        set extra_args=%%c
    )
    shift

    @echo Testing %app_folder%/%app_name% %extra_args%
    @echo.

    cd "%app_folder%/%app_name%/assets"

    set golden_img_dir=%golden_images_dir%/%app_folder%/%app_name%
    if not exist "%golden_img_dir%" (
        md "%golden_img_dir%"
    )

    set EXIT_CODE=0
    for %%X in (%test_modes%) do (
        rem ~ removes quotes from %%X
        set test_mode=%%~X

        set app_path=%DILIGENT_BUILD_DIR%/DiligentSamples/%app_folder%/%app_name%
        if exist "!app_path!/%DILIGENT_BUILD_TYPE%" (
            set app_path=!app_path!/%DILIGENT_BUILD_TYPE%
        )
        set app_path=!app_path!/%app_name%.exe

        rem Get the backend name
        set mode_param=0
        for %%Y in (!test_mode!) do (
            if "%%Y" == "--mode" (
                set mode_param=1
            ) else (
                if "%%Y" == "-m" (
                    set mode_param=1
                ) else (
                    if "!mode_param!" == "1" (
                        set backend_name=%%Y
                        set mode_param=0
                    )
                )
            )
        )
        set capture_name=%app_name%_!backend_name!

        set SKIP_TEST=0
        if "!backend_name!" == "gl" (
            rem !str:abc=! replaces substring abc in str with empty string
            if not "!test_mode:--non_separable_progs=!" == "!test_mode!" (
                if "%app_name%" == "Tutorial07_GeometryShader" (set SKIP_TEST=1)
                if "%app_name%" == "Tutorial08_Tessellation" (set SKIP_TEST=1)
            )
        )

        if "!SKIP_TEST!" == "0" (
            rem   !!!   ERRORLEVEL doesn't get updated inside control blocks like IF statements unless           !!!
            rem   !!!   !ERRORLEVEL! is used instead of %ERRORLEVEL% and delayed expansion is enabled as below:  !!!
            rem   !!!   setlocal ENABLEDELAYEDEXPANSION                                                          !!!
            set cmd_args=!test_mode! --width %GOLDEN_IMAGE_WIDTH% --height %GOLDEN_IMAGE_HEIGHT% --golden_image_mode %golden_img_mode% --capture_path %golden_img_dir% --capture_name !capture_name! --capture_format png --adapters_dialog 0 %extra_args%
            echo !app_path! !cmd_args!
            !app_path! !cmd_args!
            rem It is important to save the value of !ERRORLEVEL! so that it is not overridden by further commands
            set ERROR=!ERRORLEVEL!

            if "!ERROR!" == "0" (
                set /a TESTS_PASSED=!TESTS_PASSED!+1

                if "%golden_img_mode%" == "compare"        (set STATUS=%FONT_GREEN%Golden image validation PASSED for %app_name% [!test_mode!].%FONT_DEFAULT%)
                if "%golden_img_mode%" == "capture"        (set STATUS=%FONT_GREEN%Successfully generated golden image for %app_name% [!test_mode!].%FONT_DEFAULT%)
                if "%golden_img_mode%" == "compare_update" (set STATUS=%FONT_GREEN%Golden image validation PASSED for %app_name%. Image updated. [!test_mode!].%FONT_DEFAULT%)
            ) else (
                set EXIT_CODE=1
                set /a TESTS_FAILED=!TESTS_FAILED!+1

                if "%golden_img_mode%" == "compare"        (set STATUS=%FONT_RED%Golden image validation FAILED for %app_name% [!test_mode!]. Error code: !ERROR!.%FONT_DEFAULT%)
                if "%golden_img_mode%" == "capture"        (set STATUS=%FONT_RED%FAILED to generate golden image for %app_name% [!test_mode!]. Error code: !ERROR!.%FONT_DEFAULT%)
                if "%golden_img_mode%" == "compare_update" (set STATUS=%FONT_RED%Golden image validation FAILED for %app_name% [!test_mode!]. Error code: !ERROR!.%FONT_DEFAULT%)
            )
        ) else (
            set STATUS=%FONT_YELLOW%Golden image processing SKIPPED for %app_name% [!test_mode!].%FONT_DEFAULT%
            set /a TESTS_SKIPPED=!TESTS_SKIPPED!+1
        )

        rem For some reason, colored font does not work after the line that starts the sample app
        call :print_colored "!STATUS!"

        rem Increment test id first to make it work properly with for loop
        set /a TEST_ID=!TEST_ID!+1
        rem Note that a single variable can only contain up to 8191 characters, so we must use array.
        set TEST_STATUS[!TEST_ID!]=!STATUS!

		echo.
        echo.
    )

    cd ../../../

    EXIT /B !EXIT_CODE!



:help

    echo.
    echo === ProcessGoldenImages.bat ===
    echo.
    echo Required variables:
    echo.
    echo   DILIGENT_BUILD_TYPE - Build type (e.g. Debug, Release, etc.^)
    echo   DILIGENT_BUILD_DIR  - Path to the build directory
    echo.
    echo Optional variables:
    echo.
    echo   GOLDEN_IMAGE_WIDTH   - Golden image width (Default: 512^)
    echo   GOLDEN_IMAGE_HEIGHT  - Golden image height (Default: 512^)
    echo   ADDITIONAL_TEST_APPS - A list of additional applications to test.
    echo                          Each application should be defined as follows:
    echo                            Folder/Application ^<optional arguments^>
    echo.
    echo Command line format:
    echo.
    echo   ProcessGoldenImages.bat golden_images_dir golden_img_mode test_modes
    echo.
    echo     golden_images_dir - Path to the golden images directory
    echo     golden_img_mode   - golden image processing mode (capture, compare, or compare_update^)
    echo     test_modes        - list of test modes (e.g. "--mode d3d11" "--mode d3d11 --adapter sw" "--mode d3d12" "--mode gl" "--mode vk"^)
    echo. 
    echo Example:
    echo   set DILIGENT_BUILD_DIR=c:\git\DiligentEngine\build\Win64
    echo   set DILIGENT_BUILD_TYPE=Debug
    echo   ProcessGoldenImages.bat c:\git\DiligentTestData\GoldenImages compare "--mode d3d11" "--mode d3d12 --adapter sw"

    EXIT /B 1
