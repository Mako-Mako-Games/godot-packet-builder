@echo off
setlocal

set ADDON_SRC=%~dp0.
set BUILD_DIR=%ADDON_SRC%\build

echo Generating compile_commands.json for clangd...

REM Configure with Ninja and export compile commands
cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -B "%BUILD_DIR%" -S "%ADDON_SRC%"
if %ERRORLEVEL% neq 0 (
    echo CMake configure failed
    exit /b %ERRORLEVEL%
)

REM Build godot-cpp to generate headers
echo Generating godot-cpp headers...
cmake --build "%BUILD_DIR%" --target godot-cpp
if %ERRORLEVEL% neq 0 (
    echo godot-cpp generation failed
    exit /b %ERRORLEVEL%
)

echo.
echo Done. compile_commands.json ready for clangd.
