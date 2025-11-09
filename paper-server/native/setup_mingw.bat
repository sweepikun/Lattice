@echo off
setlocal enabledelayedexpansion

echo Installing dependencies for Windows g++ (MinGW-w64) environment...

:: Check if MSYS2 is installed
if not exist C:\msys64\mingw64\bin\gcc.exe (
    echo MSYS2 not found. Please install MSYS2 first:
    echo 1. Download from https://www.msys2.org/
    echo 2. Run the installer
    echo 3. Open MSYS2 terminal and run:
    echo    pacman -Syu
    echo    pacman -S mingw-w64-x86_64-gcc
    exit /b 1
)

:: Add MSYS2 to PATH if not already there
echo ;%PATH%; | find /C /I ";C:\msys64\mingw64\bin;" > nul
if %ERRORLEVEL% NEQ 0 (
    setx PATH "%PATH%;C:\msys64\mingw64\bin"
    set "PATH=%PATH%;C:\msys64\mingw64\bin"
)

:: Install dependencies using pacman
echo Installing zstd and libdeflate...
C:\msys64\usr\bin\pacman.exe -S --noconfirm mingw-w64-x86_64-zstd mingw-w64-x86_64-libdeflate

:: Create a g++ properties file for VSCode
if not exist .vscode mkdir .vscode

echo Creating VSCode C++ configuration...
echo {
echo     "configurations": [
echo         {
echo             "name": "Win32-MinGW",
echo             "includePath": [
echo                 "${workspaceFolder}/**",
echo                 "C:/msys64/mingw64/include"
echo             ],
echo             "defines": [
echo                 "_DEBUG",
echo                 "UNICODE",
echo                 "_UNICODE"
echo             ],
echo             "compilerPath": "C:/msys64/mingw64/bin/g++.exe",
echo             "cStandard": "c17",
echo             "cppStandard": "c++20",
echo             "intelliSenseMode": "windows-gcc-x64"
echo         }
echo     ],
echo     "version": 4
echo } > .vscode\c_cpp_properties.json

echo Setup complete!
echo.
echo To verify the installation, please run:
echo g++ --version
echo.
echo The required headers should now be available at:
echo C:\msys64\mingw64\include\zstd.h
echo C:\msys64\mingw64\include\libdeflate.h

endlocal
