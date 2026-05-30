#!/bin/bash
# Exit on error
set -e

echo "=== Building Fortran Array Bounds Checker ==="

# Compile the standalone compiler semantic pass simulator
g++ -std=c++17 -O2 src/main_standalone.cpp -o bounds-checker

echo "Build successful! Created compiled binary: ./bounds-checker"
