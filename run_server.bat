@echo off
REM Double-click this file (or run it from any terminal) to launch the server.
REM Run this FIRST, before run.bat - the client (run.bat) connects to it and just
REM waits at "Connecting to ws://..." forever if nothing is listening yet.
REM The server needs no OpenCV PATH setup at all - it never touches OpenCV.

cd /d "%~dp0"

if not exist build\kungfu_server.exe (
    echo build\kungfu_server.exe not found - build the project first:
    echo   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    echo   cmake --build build
    pause
    exit /b 1
)

echo Starting Kung Fu Chess server on port 7777...
build\kungfu_server.exe
echo.
echo Server exited with code %ERRORLEVEL%.
pause
