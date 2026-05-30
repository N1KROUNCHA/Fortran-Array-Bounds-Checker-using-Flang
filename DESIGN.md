# DESIGN: Fortran Array Bounds Checker

This document outlines the architectural approach, design decisions, pipeline topology, and alternatives considered in the implementation of the Fortran Array Bounds Checker.

---

## 🏗️ Core Architectural Approach

Our bounds checker integrates directly into **Flang's frontend compilation pipeline**. Instead of verifying bounds via runtime instrumentation (which wastes CPU cycles and adds branch instructions), our design focuses on **compile-time static verification**.

```
   Source Code (.f90)
           │
           ▼
   ┌───────────────┐
   │  Flang Lexer  │
   └───────┬───────┘
           │ token stream
           ▼
   ┌───────────────┐
   │ Flang Parser  │
   └───────┬───────┘
           │ parser::Program (Parse Tree)
           ▼
┌──────────────────────────────────────────────┐
│ Semantic Analysis (SemanticsContext)         │
│                                              │
│  Phase 1: Harvest Declarations (Symbol)       │
│  Phase 2: Intercept Subscripts (ArrayElement) │
│  Phase 3: Fold Constant Expressions (Expr)    │
│  Phase 4: Emit Diagnostics (Say())            │
│                                              │
│  ===> [ Our Bounds Checker Plugin Runs Here ] │
└──────────────────────┬───────────────────────┘
                       │
                       ▼
           Annotated Symbol Table & AST
```

### 4-Phase Compiler Pass Topology:
1. **Phase 1: Declaration Harvesting**
   * Uses a walker over the symbol table to parse declared array bounds.
   * Recognizes dimensions, lower bounds, upper bounds, and shape specifications.
2. **Phase 2: Subscript Interception**
   * Hooked into Flang's semantic pass. We visit all array element designators (`parser::ArrayElement`) and intercept their subscripts.
3. **Phase 3: Constant Propagation & Folding**
   * Walks subscript expressions recursively, reducing binary operations (`+`, `-`, `*`) and constants into static numbers.
4. **Phase 4: Diagnostics Emission**
   * Computes offset constraints and emits `error:` if index is out of bounds, or `warning:` if the index is variable and cannot be verified at compile time.

---

## ⚡ Design Alternatives Considered

During the design phase, three distinct options were evaluated for implementing array bounds checking:

| Strategy | Pros | Cons | Decision |
| :--- | :--- | :--- | :--- |
| **Option A: Static Frontend Compiler Pass (Chosen)** | • **Zero runtime cost**<br>• Catches errors before execution<br>• Fully integrated into Flang semantics | • Cannot verify user input or variable bounds | **Selected** as the primary focus of this assignment for maximum developer feedback at compile time. |
| **Option B: Lowered LLVM IR Pass (GEP Instrumentation)** | • Intercepts array offsets after compilation<br>• Catches all runtime errors | • Significant performance penalty (~15-30% overhead)<br>• Harder to map errors back to Fortran line numbers | **Considered but rejected** for standalone use due to severe runtime performance penalties. |
| **Option C: Runtime Library Checks (`-fcheck=bounds`)** | • Standard approach for production<br>• Easy to implement via compiler runtime libraries | • Program must be compiled, run, and crashed to reveal errors | Used as the baseline comparison (`gfortran` runtime check) to showcase Flang plugin's compile-time benefits. |

---

## 🛠️ Design Decisions & Trade-offs

### 1. Handling Arbitrary Lower Bounds
Unlike C/C++ which strictly uses `0`-based indexing, Fortran allows custom lower and upper bounds (e.g. `REAL A(-5:5)`).
* **Our Design:** We maintain an explicit representation of the lower bound for each dimension in our symbol table map.
* **Math:** When evaluating subscript index `i` against dimension `D` with bounds `[L:U]`, the checker evaluates `L <= i <= U` directly rather than converting to `0`-based index first. This preserves the original programmer context in the emitted compiler diagnostics.

### 2. Multi-dimensional Subscript Mapping
* **Our Design:** Flang's parse tree holds multidimensional subscripts as a list inside a single flat `parser::ArrayElement` node. We check each dimension independently. This is highly superior to checking flattened 1D indices, as it isolates the exact dimension that violates bounds (e.g., `dimension 2: index 4 is out of bounds [1:3]`).
