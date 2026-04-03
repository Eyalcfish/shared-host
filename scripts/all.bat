@echo off
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%.."

echo Building DLL...
call scripts\make.bat

echo.
echo Running Tests...
call scripts\test.bat

echo.
echo Running Benchmarks...
call scripts\benchmark.bat
