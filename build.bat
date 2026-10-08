@echo off
setlocal

REM Change working directory to the project root where this batch file lives
cd /d "%~dp0"

REM Ensure WinLibs MinGW-w64 / CMake / Ninja toolchain is on PATH if installed via WinGet
set "WINLIBS_BIN=%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
if exist "%WINLIBS_BIN%\cmake.exe" (
    set "PATH=%WINLIBS_BIN%;%PATH%"
)

echo ============================================
echo Building Thrust...
echo ============================================

REM Configure CMake in build/ if not already configured
if not exist "build\CMakeCache.txt" (
    echo Configuring CMake project...
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    if errorlevel 1 (
        echo.
        echo [ERROR] CMake configuration failed!
        pause
        exit /b 1
    )
)

REM Build the executable
cmake --build build --config Release
if errorlevel 1 (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo [SUCCESS] Build complete: "%~dp0build\thrust.exe"

REM If launched by double-clicking in Explorer, pause so the window stays open
echo %cmdcmdline% | find /i "/c" >nul
if not errorlevel 1 (
    echo.
    pause
)

endlocal
exit /b 0
