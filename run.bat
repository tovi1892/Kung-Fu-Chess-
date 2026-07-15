@echo off
REM Double-click this file (or run it from any terminal) to launch the app.
REM It always runs from the repo root, regardless of where it was started from,
REM because the app loads assets (UI\assets\...) via paths relative to the repo root.

cd /d "%~dp0"
set PATH=C:\opencv\build\x64\vc16\bin;%PATH%

if not exist build\kungfu_app.exe (
    echo build\kungfu_app.exe not found - build the project first:
    echo   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    echo   cmake --build build
    pause
    exit /b 1
)

echo Starting Kung Fu Chess...
build\kungfu_app.exe
echo.
echo App exited with code %ERRORLEVEL%.
pause
