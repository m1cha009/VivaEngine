@echo off
rem Builds the Release sandbox and copies everything needed to run it into dist\<name>\, the
rem way a Unity build writes the player into an output folder.
rem
rem   scripts\package.cmd M1
rem
rem What gets copied is decided by the install() rules in the CMake files: the executable, plus
rem the compiled shaders and the assets once there are any. dist\ is ignored by git.

setlocal

set "NAME=%~1"
if "%NAME%"=="" (
    echo usage: scripts\package.cmd NAME, for example: scripts\package.cmd M1
    exit /b 1
)

cd /d "%~dp0.."
call scripts\build.cmd windows-release || exit /b 1

if exist "dist\%NAME%" rmdir /s /q "dist\%NAME%"
cmake --install build\windows-release --prefix "dist\%NAME%" || exit /b 1
echo Packaged into dist\%NAME%
