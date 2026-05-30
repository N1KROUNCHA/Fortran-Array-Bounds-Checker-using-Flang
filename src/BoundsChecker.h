//===-- BoundsChecker.h - Fortran Array Bounds Checker --------------------===//
//
// Flang Semantic Analysis Plugin
// Assignment 9: Compile-Time Array Bounds Checking
//
// Detects out-of-bounds array accesses at compile time by:
//   1. Collecting array declarations with dimension bounds from symbol tables
//   2. Intercepting array subscript expressions in the parse tree
//   3. Performing constant propagation on index expressions
//   4. Emitting diagnostics (errors/warnings) based on analysis results
//
//===----------------------------------------------------------------------===//

#pragma once

#include "flang/Parser/parse-tree.h"
#include "flang/Parser/parse-tree-visitor.h"
#include "flang/Semantics/semantics.h"
#include "flang/Semantics/symbol.h"
#include "flang/Semantics/type.h"
#include "flang/Semantics/scope.h"
#include "flang/Semantics/tools.h"
#include "flang/Evaluate/expression.h"
#include "flang/Common/visit.h"

#include <map>
#include <string>
#include <vector>
#include <optional>
#include <iostream>
#include <sstream>

namespace Fortran::bounds {

// ─────────────────────────────────────────────────────────────
// Diagnostic severity
// ─────────────────────────────────────────────────────────────
enum class DiagKind { Note, Warning, Error };

struct Diagnostic {
    DiagKind kind;
    std::string arrayName;
    int dimension;        // 1-based
    std::string message;
    // Source context (line/col populated by the driver)
    int line{0};
    int col{0};
};

// ─────────────────────────────────────────────────────────────
// Represents a single array dimension's bounds
// ─────────────────────────────────────────────────────────────
struct DimBounds {
    std::optional<long long> lower;   // nullopt = unknown/assumed-shape
    std::optional<long long> upper;   // nullopt = unknown/assumed-size

    bool isFullyKnown() const { return lower.has_value() && upper.has_value(); }

    std::string toString() const {
        std::string lo = lower ? std::to_string(*lower) : "?";
        std::string hi = upper ? std::to_string(*upper) : "?";
        return lo + ":" + hi;
    }
};

// ─────────────────────────────────────────────────────────────
// Per-array metadata collected during the declaration pass
// ─────────────────────────────────────────────────────────────
struct ArrayInfo {
    std::string name;
    std::vector<DimBounds> dims;   // one entry per dimension
    bool isAllocatable{false};
    bool isPointer{false};
    bool isAssumedShape{false};
};

// ─────────────────────────────────────────────────────────────
// Result of evaluating a subscript expression
// ─────────────────────────────────────────────────────────────
enum class IndexKind {
    Constant,      // exact integer value known
    Variable,      // symbolic / loop-variable – cannot verify statically
    Expression,    // more complex expr (binary ops, function calls, etc.)
    Unknown
};

struct IndexValue {
    IndexKind kind{IndexKind::Unknown};
    long long value{0};            // valid only when kind == Constant
    std::string exprText;          // human-readable representation
};

// ─────────────────────────────────────────────────────────────
// The main checker class
// ─────────────────────────────────────────────────────────────
class ArrayBoundsChecker {
public:
    explicit ArrayBoundsChecker(
        const Fortran::semantics::SemanticsContext &ctx)
        : context_(ctx) {}

    // ── Phase 1: collect declarations ──────────────────────
    void collectArrayDeclarations(
        const Fortran::semantics::Scope &scope);

    // ── Phase 2: check all subscript references ─────────────
    void checkSubscripts(
        const Fortran::parser::Program &program);

    // ── Reporting ──────────────────────────────────────────
    const std::vector<Diagnostic> &getDiagnostics() const {
        return diagnostics_;
    }

    void printDiagnostics(std::ostream &out = std::cerr) const;

    // ── Statistics ─────────────────────────────────────────
    struct Stats {
        int totalArrays{0};
        int fullyBoundedArrays{0};
        int totalSubscripts{0};
        int constantViolations{0};    // definite errors caught
        int variableWarnings{0};      // possible violations (can't verify)
        int verifiedSafe{0};          // provably within bounds
        int unknownBounds{0};         // array has unknown bounds
    };

    Stats getStats() const { return stats_; }
    void printStats(std::ostream &out = std::cerr) const;

private:
    const Fortran::semantics::SemanticsContext &context_;
    std::map<std::string, ArrayInfo> arrays_;   // name → metadata
    std::vector<Diagnostic> diagnostics_;
    Stats stats_;

    // ── Helpers ─────────────────────────────────────────────
    void collectFromSymbol(const Fortran::semantics::Symbol &sym);

    std::optional<long long> evalConstantBound(
        const Fortran::semantics::Bound &bound) const;

    IndexValue evalIndex(
        const Fortran::parser::Expr &expr) const;

    IndexValue evalIntLiteral(
        const Fortran::parser::IntLiteralConstant &lit) const;

    void checkOneSubscript(
        const std::string &arrayName,
        int dimIdx,
        const IndexValue &idx,
        const DimBounds &bounds);

    void addDiag(DiagKind kind,
                 const std::string &arrayName,
                 int dim,
                 const std::string &msg);

    // Walk the parse tree looking for array subscript uses
    void walkExpr(const Fortran::parser::Expr &expr,
                  const std::string &contextArray = "");
};

} // namespace Fortran::bounds
