@echo off
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%.."

make.exe benchmark
build\benchmark_main.exe
