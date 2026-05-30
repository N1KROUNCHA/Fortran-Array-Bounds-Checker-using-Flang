# IMPLEMENTATION: Fortran Array Bounds Checker

This document provides deep technical details on the inner workings of our Flang bounds-checking compiler pass, code structures, AST structures, and visitors.

---

## 🌳 Flang AST and Parse Tree Structures

Our plugin hooks into Flang's front-end parsing and semantic checking engine. In Flang, the source code is compiled into a typed Parse Tree (`parser::Program`).

### 1. The Array Element Access Node (`parser::ArrayElement`)
Array accesses in Fortran are represented in Flang's parse tree as:
```cpp
// From flang/include/flang/Parser/parse-tree.h
struct ArrayElement {
    DataRef base;
    std::list<SectionSubscript> subscripts;
};
```
* **`base`**: Refers to the identifier of the array being accessed (e.g. `A`).
* **`subscripts`**: A list of subscript expressions. Each subscript is parsed as a variant which can represent a single element subscript (`parser::Expr`), a triplet (`A(1:5)`), or a vector subscript.

### 2. The Symbol Table Interface
To resolve the bounds of an array, the plugin queries Flang's `Symbol` class:
```cpp
// We look for ObjectEntityDetails containing dimensions
const auto *details = sym.detailsIf<ObjectEntityDetails>();
if (details && !details->shape().empty()) {
    for (const auto &shapeSpec : details->shape()) {
        const auto &lbound = shapeSpec.lbound();
        const auto &ubound = shapeSpec.ubound();
        // Extract boundaries
    }
}
```
* Each dimension is defined by a `ShapeSpec`.
* Boundaries (`lbound()`, `ubound()`) hold `Bound` wrappers. We extract constants by calling Flang's constant folding helper: `evaluate::ToInt64(bound.GetExpr())`.

---

## ⚡ Constant Propagation and Expressions Folding

A crucial part of catching bounds violations at compile time is folding arithmetic expressions. Our standalone driver implements a recursive syntax folding visitor over subscript expressions:

```cpp
// Standalone Folder Logic (BoundsChecker.h)
SubscriptValue evalSubscript(const SubscriptNode& node) {
    if (node.isLiteral()) {
        return SubscriptValue::Constant(node.getIntValue());
    }
    if (node.isUnaryMinus()) {
        auto val = evalSubscript(node.getChild(0));
        return val.isConstant() ? SubscriptValue::Constant(-val.getValue()) : SubscriptValue::Variable();
    }
    if (node.isAdd()) {
        auto left = evalSubscript(node.getLeft());
        auto right = evalSubscript(node.getRight());
        return (left.isConstant() && right.isConstant()) 
            ? SubscriptValue::Constant(left.getValue() + right.getValue()) 
            : SubscriptValue::Variable();
    }
    if (node.isSubtract()) {
        auto left = evalSubscript(node.getLeft());
        auto right = evalSubscript(node.getRight());
        return (left.isConstant() && right.isConstant()) 
            ? SubscriptValue::Constant(left.getValue() - right.getValue()) 
            : SubscriptValue::Variable();
    }
    if (node.isMultiply()) {
        auto left = evalSubscript(node.getLeft());
        auto right = evalSubscript(node.getRight());
        return (left.isConstant() && right.isConstant()) 
            ? SubscriptValue::Constant(left.getValue() * right.getValue()) 
            : SubscriptValue::Variable();
    }
    return SubscriptValue::Variable(); // Fallback for variable indices
}
```

---

## 🔄 LLVM IR Lowering Context

While our plugin performs semantic checks at the Flang AST level, it is useful to look at what Flang generates underneath in **LLVM IR**. 

For a Fortran array declaration:
```fortran
REAL A(10)
A(3) = 99.0
```

1. **Lowering to FIR (Fortran IR)**:
   The compiler uses Column-major indexing. A 1D access translates to:
   ```fir
   %base = fir.address_of(@A) : !fir.ref<!fir.array<10xf32>>
   %c2 = arith.constant 2 : index // 0-based offset computed from (3 - 1)
   %coor = fir.coordinate_of %base, %c2 : (!fir.ref<!fir.array<10xf32>>, index) -> !fir.ref<f32>
   fir.store %val to %coor : !fir.ref<f32>
   ```

2. **Lowering to LLVM IR**:
   The `fir.coordinate_of` instruction is lowered to a **`getelementptr` (GEP)** pointer arithmetic instruction:
   ```llvm
   %base = load float*, float** @A
   %gep = getelementptr float, float* %base, i64 2 ; index - lbound (3 - 1)
   store float 9.900000e+01, float* %gep, align 4
   ```

If index arithmetic results in a compile-time out-of-bounds offset (e.g. accessing `A(0)` which is `-1` relative to `1`-based default), the GEP pointer arithmetic references memory outside the allocated block, leading to **Undefined Behavior (UB)** at runtime. Our Flang plugin intercepts this at compile time before any unsafe GEP instruction can ever be lowered!
