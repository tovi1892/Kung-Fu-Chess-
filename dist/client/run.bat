@echo off
REM Double-click this file (or run it from any terminal) to launch the client.
REM This connects to a server over the network - run run_server.bat FIRST (on this
REM machine or another one), or this will sit at "Connecting to ws://..." forever.
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
REM Optional first argument: the server's address (default 127.0.0.1, i.e. same machine).
REM Example, connecting to a server on another computer: run.bat 192.168.1.42
build\kungfu_app.exe %1
echo.
echo App exited with code %ERRORLEVEL%.
pause
