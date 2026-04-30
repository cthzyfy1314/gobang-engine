@echo off
rem =====================================================================
rem  gobang-engine - MSVC build script
rem  Usage: open "x64 Native Tools Command Prompt for VS", cd to root, run build.bat
rem  Output: gobang-engine.exe in project root
rem =====================================================================

cd /d %~dp0

rem 如果 cl 不在 PATH，定位 vs 工具链
where cl >nul 2>&1
if not errorlevel 1 goto compile

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] cl not found and vswhere missing.
    echo         Please open "x64 Native Tools Command Prompt for VS" and re-run.
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
    echo [ERROR] could not detect Visual Studio installation
    exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

:compile
if not exist build mkdir build

rem ========== 主程序 ==========
echo Building gobang-engine.exe...
cl /W3 /TC /utf-8 /nologo /Fe:gobang-engine.exe /Fo:build\ src\*.c
if errorlevel 1 ( echo [ERROR] main build failed & exit /b 1 )

rem ========== 单元测试 ==========
echo Building test_board.exe...
cl /W3 /TC /utf-8 /nologo /Fe:test_board.exe /Fo:build\ ^
   src\board.c tests\test_board.c
if errorlevel 1 ( echo [ERROR] test_board build failed & exit /b 1 )

echo Building test_search.exe...
cl /W3 /TC /utf-8 /nologo /Fe:test_search.exe /Fo:build\ ^
   src\board.c src\pattern.c src\search.c tests\test_search.c
if errorlevel 1 ( echo [ERROR] test_search build failed & exit /b 1 )

echo Building test_pattern.exe...
cl /W3 /TC /utf-8 /nologo /Fe:test_pattern.exe /Fo:build\ ^
   src\board.c src\pattern.c tests\test_pattern.c
if errorlevel 1 ( echo [ERROR] test_pattern build failed & exit /b 1 )

echo.
echo [OK] build complete.
echo Run:
echo   gobang-engine.exe
echo   test_board.exe
echo   test_search.exe
echo   test_pattern.exe
