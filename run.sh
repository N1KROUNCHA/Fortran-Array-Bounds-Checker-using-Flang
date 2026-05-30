#!/bin/bash

# Ensure build binary exists
if [ ! -f "./bounds-checker" ]; then
    echo "Compiled binary ./bounds-checker not found. Running build.sh first..."
    ./build.sh
fi

# Usage helper
if [ $# -eq 0 ]; then
    echo "Usage: ./run.sh [test_file.f90] [--stats]"
    echo "Running default test cases to demonstrate capability..."
    echo ""
    
    echo "=== Running Test 1: Constant OOB ==="
    ./bounds-checker testcases/test01_constant_oob.f90 --stats
    echo ""
    
    echo "=== Running Test 2: Variable Indices (Warnings) ==="
    ./bounds-checker testcases/test02_variable_index.f90 --stats
    echo ""
    
    echo "=== Running Test 3: Correct Usage (Clean) ==="
    ./bounds-checker testcases/test03_correct_usage.f90 --stats
    echo ""
    
    echo "=== Running Test 5: Constant Expressions ==="
    ./bounds-checker testcases/test05_const_expr.f90 --stats
else
    ./bounds-checker "$@"
fi
