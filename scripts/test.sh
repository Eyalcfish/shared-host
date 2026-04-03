#!/bin/bash
# Move to the project root
cd "$(dirname "$0")/.."

make.exe test
./build/test_main.exe
