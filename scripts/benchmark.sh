#!/bin/bash
# Move to the project root
cd "$(dirname "$0")/.."

make.exe benchmark
./build/benchmark_main.exe
