@echo off
rem P01 UpdateBudget - smoke test: run the project in -game mode (uncooked, no rendering), load the default map, quit.
rem Usage: smoke_game.bat [extra args...]   e.g. smoke_game.bat -ExecCmds="stat unit"
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\UpdateBudget\UpdateBudget.uproject"
set "LOG=%~dp0..\UpdateBudget\Saved\Logs\SmokeGame.log"

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" -game -nullrhi -nosound -unattended -nosplash -traceautostart=0 -ExitAfterSeconds=0 -ExecCmds="quit" -log -abslog="%LOG%" %*
exit /b %ERRORLEVEL%
