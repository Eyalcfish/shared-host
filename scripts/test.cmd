@echo off
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%.."

make.exe test
build\test_main.exe
