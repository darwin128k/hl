@echo off
setlocal
cd /d %~dp0

set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
call %VCVARS% >nul
if errorlevel 1 (
    echo Failed to init MSVC x86 environment
    exit /b 1
)

if not exist build mkdir build
cd build

cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl ..
if errorlevel 1 (
    echo CMake configure failed
    exit /b 1
)

cmake --build .
if errorlevel 1 (
    echo Build failed
    exit /b 1
)

cd ..

set ROOT=.\
if exist ..\hw.dll (
    set ROOT=..\
) else if exist ..\..\hw.dll (
    set ROOT=..\..\
)
set HL_ORIG=orig\hl.exe
set HL_DST=%ROOT%hl.exe

if not exist orig mkdir orig
if not exist "%HL_ORIG%" (
    if exist "%HL_DST%" (
        echo Backing up current hl.exe to %HL_ORIG%
        copy /Y "%HL_DST%" "%HL_ORIG%" >nul
    )
)

copy /Y build\hl.exe "%HL_DST%" >nul
if errorlevel 1 (
    echo Built OK, but could not copy hl.exe to game root -- is the game running?
    exit /b 1
)

echo Build OK: hl.exe
endlocal
