@echo off
setlocal ENABLEDELAYEDEXPANSION
set MSYS_BASH=C:\msys64\usr\bin\bash.exe
if not exist "%MSYS_BASH%" (
  echo MSYS2 bash not found at %MSYS_BASH%
  exit /b 1
)

REM Change to this script's directory (your project root)
cd /d %~dp0

"%MSYS_BASH%" -lc "cd \"$(cygpath -u '%CD%')\" && bash build_ucrt64.sh"
set ERR=%ERRORLEVEL%
if %ERR% neq 0 (
  echo Build failed with error %ERR%.
  exit /b %ERR%
)

echo Build complete.
endlocal
