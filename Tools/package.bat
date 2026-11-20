@echo off
rem P01 UpdateBudget - package a standalone Win64 build (BuildCookRun). Flags verified in 01b_Research_Findings.md section 4.
rem Usage: package.bat [Configuration]   Configuration: Development (default; headline benchmarks) | Shipping (no CSV profiler; not used)
rem Note: the launcher (installed) engine cannot build the Test configuration ("Targets cannot be built in the Test configuration with this engine distribution").
rem Output: ..\Build\Windows\UpdateBudget.exe   (Build\ is git-ignored)
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\UpdateBudget\UpdateBudget.uproject"
set "ARCHIVE=%~dp0..\Build"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Development"

call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJECT%" -noP4 -utf8output -unattended -platform=Win64 -clientconfig=%CONFIG% -build -cook -stage -pak -archive -archivedirectory="%ARCHIVE%"
exit /b %ERRORLEVEL%
