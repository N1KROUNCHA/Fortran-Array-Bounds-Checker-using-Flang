# Fortran Array Bounds Checker using Flang

A Flang semantic analysis compiler pass and interactive visual pipeline dashboard designed to detect out-of-bounds (OOB) array subscripts **at compile time** in Fortran programs.

---

## 🎯 What it Is
This project is an advanced compile-time array bounds checking tool for Fortran (F90/F95/F2003/F2018) programs. By tapping directly into **Flang's frontend Parse Tree** and **Symbol Table Semantics**, it statically evaluates array subscripts, folds constant subscript expressions, and analyzes array access bounds before your code even executes.

### Key Objectives Implemented:
1. **(a) Symbol Declaration Collection**: Extracts multidimensional array declarations and their custom bounds (e.g. 1-based, 0-based, or negative-bound indexing like `REAL A(-5:5)`).
2. **(b) Subscript Interception**: Hooks into Flang semantic passes to intercept and parse array subscripts (`parser::ArrayElement` nodes).
3. **(c) Constant Propagation & Folding**: Traverses and evaluates compile-time literal expressions (e.g., folding `A(2*3+4)` to `A(10)`).
4. **(d) Diagnostic Reporting**: Emits compilation errors for definite violations and warning notes for runtime-unverifiable variable indices.

---

## 🚀 How to Run

### 📦 Standalone CLI Mode (Quickstart)
Build and run the static analyzer standalone binary without requiring the full LLVM/Flang development package:

1. **Build the compiler pass simulator**:
   ```bash
   ./build.sh
   ```
2. **Run on any Fortran file**:
   ```bash
   ./run.sh testcases/test01_constant_oob.f90
   ```
3. **Run on all testcases with stats**:
   ```bash
   ./run.sh
   ```

---

### 🖥️ Interactive Web Dashboard Mode
Experience the complete analysis visually with a state-of-the-art, glassmorphic dark-themed SPA:

1. **Install dependencies and start the local server**:
   ```bash
   cd frontend
   npm install
   npm start
   ```
2. **Open in Browser**:
   Navigate to [http://localhost:3000](http://localhost:3000)

#### Visual Interactive Features:
* **Interactive Compiler Pipeline Flowchart**: Visualizes the stages of Flang compiler bounds checking as the pass runs.
* **Collapsible SVG Parse Tree**: Renders the complete hierarchical AST for your Fortran source code dynamically. Clicking parent nodes collapses/expands children. Includes step-by-step BFS expansion animations.
* **Flang vs gfortran Comparison**: Features a dedicated comparative module showing the difference between compile-time checking (Flang) and dynamic runtime guard branches (`gfortran -fbounds-check`) using code bubbles and live SVG trees.

---

## 📂 Repository Layout
* **`src/`**: Core C++ source files (`BoundsChecker.cpp`, `BoundsChecker.h`, `main_standalone.cpp`).
* **`testcases/`**: Standard test files representing safe, unsafe, variable-based, and expression-based indices.
* **`frontend/`**: Beautiful glassmorphic UI SPA and Express backend server.
* **`build.sh` & `run.sh`**: Command-line helper scripts.
* **`DESIGN.md`**: Detailed architectural design, design choices, and alternatives.
* **`IMPLEMENTATION.md`**: Inner working details of Flang's semantic pipeline, Visitors, AST nodes, and constant folding.
* **`EVALUATION.md`**: Testing metrics, comparison with existing compilers, and real-world statistics.
