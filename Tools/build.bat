@echo off
rem P01 UpdateBudget - build one target from the command line (reproducible; no IDE needed).
rem Usage: build.bat [Target] [Configuration]
rem   Target:        UpdateBudgetEditor (default) | UpdateBudget
rem   Configuration: Development (default) | Shipping | DebugGame (the launcher engine cannot build Test)
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\UpdateBudget\UpdateBudget.uproject"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=UpdateBudgetEditor"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Development"

call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" %TARGET% Win64 %CONFIG% -Project="%PROJECT%" -WaitMutex -NoHotReloadFromIDE
exit /b %ERRORLEVEL%
