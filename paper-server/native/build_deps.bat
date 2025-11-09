@echo off
setlocal enabledelayedexpansion

echo Building and installing zstd and libdeflate...

:: Get MinGW installation path from g++
for /f "tokens=*" %%i in ('where g++') do set MINGW_PATH=%%i
set MINGW_PATH=%MINGW_PATH:\bin\g++.exe=%

if not defined MINGW_PATH (
    echo Error: Could not find MinGW installation
    exit /b 1
)

echo Found MinGW at: %MINGW_PATH%

:: Create temp directory for building
set BUILD_DIR=%TEMP%\lattice_build
if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
mkdir %BUILD_DIR%
cd %BUILD_DIR%

:: Build and install zstd
echo Building zstd...
git clone --depth 1 https://github.com/facebook/zstd.git
cd zstd
mingw32-make -j CFLAGS="-O3" || (
    echo Failed to build zstd
    exit /b 1
)

echo Installing zstd...
copy /Y lib\zstd.h "%MINGW_PATH%\include\"
copy /Y lib\libzstd.dll.a "%MINGW_PATH%\lib\"
copy /Y lib\libzstd.dll "%MINGW_PATH%\bin\"

cd %BUILD_DIR%

:: Build and install libdeflate
echo Building libdeflate...
git clone --depth 1 https://github.com/ebiggers/libdeflate.git
cd libdeflate
mkdir build
cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release .. || (
    echo Failed to configure libdeflate
    exit /b 1
)
mingw32-make -j || (
    echo Failed to build libdeflate
    exit /b 1
)

echo Installing libdeflate...
copy /Y ..\libdeflate.h "%MINGW_PATH%\include\"
copy /Y libdeflate.dll.a "%MINGW_PATH%\lib\"
copy /Y libdeflate.dll "%MINGW_PATH%\bin\"

:: Cleanup
cd %~dp0
rmdir /s /q %BUILD_DIR%

echo.
echo Installation complete!
echo.
echo Please verify that the following files exist:
echo %MINGW_PATH%\include\zstd.h
echo %MINGW_PATH%\include\libdeflate.h
echo.
echo If you see any errors, please make sure you have the following tools installed:
echo - git
echo - cmake
echo - mingw32-make

endlocal
