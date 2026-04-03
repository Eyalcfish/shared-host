#!/bin/bash
# Move to the project root
cd "$(dirname "$0")/.."

echo "Building DLL..."
./scripts/make.sh

echo ""
echo "Running Tests..."
./scripts/test.sh

echo ""
echo "Running Benchmarks..."
./scripts/benchmark.sh
