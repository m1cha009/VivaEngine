@echo off
rem Configures and builds VivaEngine from a plain Windows shell (cmd, PowerShell or Git Bash).
rem
rem   scripts\build.cmd                    builds the windows-debug preset
rem   scripts\build.cmd windows-release    builds another preset
rem
rem Why this script exists: our presets use Ninja with MSVC, and MSVC's compiler (cl.exe) only
rem works inside the "developer environment" that vcvars64.bat sets up: PATH, INCLUDE and LIB
rem pointing at the compiler, the C++ standard library and the Windows SDK. CLion does this
rem by itself; this script does the same for the command line.

setlocal

set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=windows-debug"

rem vswhere.exe comes with every Visual Studio and Build Tools install. We ask it for the newest
rem install that has the x64 C++ compiler; "-products *" is needed to include Build Tools.
set "VS_INSTALLER=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if not exist "%VS_INSTALLER%\vswhere.exe" (
    echo error: vswhere.exe not found. Install Visual Studio Build Tools with the C++ workload.
    exit /b 1
)
set "PATH=%VS_INSTALLER%;%PATH%"

set "VS_PATH="
for /f "usebackq delims=" %%i in (`vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH (
    echo error: no Visual Studio install with the x64 C++ compiler was found.
    exit /b 1
)

call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo error: vcvars64.bat failed.
    exit /b 1
)

cd /d "%~dp0.."
cmake --preset "%PRESET%" || exit /b 1
cmake --build --preset "%PRESET%" || exit /b 1
