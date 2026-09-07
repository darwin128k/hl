@echo off
setlocal
cd /d "%~dp0"
set "MH=%~dp0..\metahook"
for %%I in ("%MH%") do set "MHABS=%%~fI"

if not exist "%MHABS%\src\metahook.cpp" (
    echo MetaHookSv not found at %MHABS%
    exit /b 1
)

set "CAP_INST=%MHABS%\thirdparty\install\capstone\x86\Release"
set "MM_LIB=%MHABS%\thirdparty\install\MemoryModulePP\x86\Release\lib\MemoryModule.lib"
if not exist "%MM_LIB%" set "MM_LIB=%MHABS%\thirdparty\install\MemoryModulePP\Win32\Release\lib\MemoryModule.lib"

if exist "%CAP_INST%\lib\capstone.lib" if exist "%MM_LIB%" if exist "%MHABS%\Build\svencoop\metahook\gamedata" (
    echo MetaHook deps already present, skipping rebuild
    goto copy_gamedata
)

set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
call %VCVARS% >nul
if errorlevel 1 (
    echo Failed to init MSVC x86 environment
    exit /b 1
)

pushd "%MHABS%"
git submodule update --init -- thirdparty/Detours_fork thirdparty/capstone_fork thirdparty/rapidjson thirdparty/Chocobo1Hash thirdparty/Musa.Veil_fork
if errorlevel 1 (
    echo Failed to init MetaHookSv submodules
    popd
    exit /b 1
)

set "CAP_SRC=%MHABS%\thirdparty\capstone_fork"
set "CAP_BIN=%MHABS%\thirdparty\build\capstone\x86\Release"
if exist "%CAP_BIN%\CMakeCache.txt" findstr /C:"CMAKE_GENERATOR:INTERNAL=Ninja" "%CAP_BIN%\CMakeCache.txt" >nul && rmdir /s /q "%CAP_BIN%"
powershell -NoProfile -Command "(Get-Content -LiteralPath '%CAP_SRC%\CMakeLists.txt' -Raw) -replace 'cmake_policy \(SET CMP0048 OLD\)','cmake_policy (SET CMP0048 NEW)' | Set-Content -LiteralPath '%CAP_SRC%\CMakeLists.txt' -NoNewline"
cmake -S "%CAP_SRC%" -B "%CAP_BIN%" -G "Visual Studio 17 2022" -A Win32 -DCMAKE_INSTALL_PREFIX="%CAP_INST%" -DCAPSTONE_BUILD_SHARED=FALSE -DCAPSTONE_BUILD_STATIC=TRUE -DCAPSTONE_BUILD_STATIC_RUNTIME=TRUE -DCAPSTONE_BUILD_TESTS=FALSE -DCAPSTONE_BUILD_CSTOOL=FALSE -DCAPSTONE_INSTALL=TRUE -DCAPSTONE_X86_SUPPORT=TRUE -DCAPSTONE_ARM_SUPPORT=FALSE -DCAPSTONE_ARM64_SUPPORT=FALSE -DCAPSTONE_M68K_SUPPORT=FALSE -DCAPSTONE_MIPS_SUPPORT=FALSE -DCAPSTONE_PPC_SUPPORT=FALSE -DCAPSTONE_SPARC_SUPPORT=FALSE -DCAPSTONE_SYSZ_SUPPORT=FALSE -DCAPSTONE_XCORE_SUPPORT=FALSE -DCAPSTONE_TMS320C64X_SUPPORT=FALSE -DCAPSTONE_M680X_SUPPORT=FALSE -DCAPSTONE_EVM_SUPPORT=FALSE -DCAPSTONE_MOS65XX_SUPPORT=FALSE -DCMAKE_POLICY_VERSION_MINIMUM=3.5
if errorlevel 1 (
    echo capstone configure failed
    popd
    exit /b 1
)
cmake --build "%CAP_BIN%" --config Release --target install
if errorlevel 1 (
    echo capstone build failed
    popd
    exit /b 1
)
popd

MSBuild.exe "%MHABS%\thirdparty\MemoryModulePP\MemoryModule\MemoryModule.vcxproj" /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir="%MHABS%\\"
if errorlevel 1 (
    echo MemoryModulePP build failed
    exit /b 1
)

if not exist "%MHABS%\Build\svencoop\metahook\gamedata" mkdir "%MHABS%\Build\svencoop\metahook\gamedata"
python "%MHABS%\scripts\sync-gamedata.py" --index-url "https://hlnd2t.github.io/GoldSrc_VibeSignatures/gamesymbols/index.json" --target-dir "%MHABS%\Build\svencoop\metahook\gamedata" --temp-root "%MHABS%\intermediate\GameDataSync"
if errorlevel 1 (
    echo gamedata sync failed
    exit /b 1
)

:copy_gamedata
set ROOT=%~dp0..\..\
if exist "%ROOT%cstrike" (
    if not exist "%ROOT%cstrike\metahook\gamedata" mkdir "%ROOT%cstrike\metahook\gamedata"
    if not exist "%ROOT%cstrike\metahook\plugins" mkdir "%ROOT%cstrike\metahook\plugins"
    if not exist "%ROOT%cstrike\metahook\configs" mkdir "%ROOT%cstrike\metahook\configs"
    xcopy "%MHABS%\Build\svencoop\metahook\gamedata\*" "%ROOT%cstrike\metahook\gamedata\" /E /I /Y >nul
    echo Copied gamedata to cstrike\metahook\gamedata
)

echo MetaHook deps OK
endlocal
