@echo off
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%.."

echo Building DLL...
call scripts\make.cmd

echo.
echo Running Tests...
call scripts\test.cmd

echo.
echo Running Benchmarks...
call scripts\benchmark.cmd
