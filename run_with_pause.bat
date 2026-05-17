@echo off
chcp 65001 >nul
cd /d "C:\Users\cthzy\source\repos\gobang-engine"
"C:\Users\cthzy\source\repos\gobang-engine\gobang-engine.exe" %*
echo.
echo ========================================
echo Engine exited with code %ERRORLEVEL%
echo Press any key to close this window.
echo ========================================
pause >nul
