@echo off
cd ..\WindowsStore\AntTweakBarSample.Shared
SET SAMPLE_PATH="..\..\..\..\..\build\Windows\bin\%~1\AntTweakBarSample\AntTweakBarSample.exe"

if not exist %SAMPLE_PATH% (
	echo Executable not found in the diligentsamples\build\Windows directory. Checking parent directory.
    SET SAMPLE_PATH="..\"%SAMPLE_PATH%
)

if not exist %SAMPLE_PATH% (
    echo Executable not found. Please build the solution for the selected configuration.
	exit /B 1
)

%SAMPLE_PATH% -bUseOpenGL=%~2
