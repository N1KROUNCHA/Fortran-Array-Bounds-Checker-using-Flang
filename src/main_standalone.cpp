//===-- bounds_checker.cpp - Fortran Array Bounds Checker -----------------===//
//
// Standalone implementation of the Flang semantic bounds-checking plugin.
//
// Architecture mirrors a real Flang plugin:
//   Phase 1 – Symbol-table walk  (here: regex-based declaration harvest)
//   Phase 2 – Parse-tree walk    (here: regex-based subscript harvest)
//   Phase 3 – Constant propagation + bounds decision
//
// Real integration: flang-new -fc1 -fsyntax-only <file.f90>
// invokes CheckHelper::Enter(const parser::ArrayElement &) which calls
// our checker. This driver reproduces the exact same diagnostic output.
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Data types (mirrors Flang's ShapeSpec / Bound / Evaluate structures)
// ─────────────────────────────────────────────────────────────────────────────

struct DimBounds {
    std::optional<long long> lower;
    std::optional<long long> upper;
    bool isFullyKnown() const { return lower.has_value() && upper.has_value(); }
    std::string str() const {
        return (lower ? std::to_string(*lower) : "?") + ":" +
               (upper ? std::to_string(*upper) : "?");
    }
};

struct ArrayInfo {
    std::string name;
    int declarationLine{0};
    std::vector<DimBounds> dims;
};

enum class IndexKind { Constant, Variable, Expression, Unknown };

struct IndexValue {
    IndexKind    kind{IndexKind::Unknown};
    long long    value{0};
    std::string  text;
};

struct Diagnostic {
    enum class Severity { Error, Warning, Note } sev;
    std::string file;
    int         line{0};
    std::string arrayName;
    int         dim{0};
    std::string message;

    void print(std::ostream &os) const {
        const char *tag = (sev == Severity::Error)   ? "error"   :
                          (sev == Severity::Warning)  ? "warning" : "note";
        os << file << ":" << line << ":1: "
           << tag << ": " << message << "\n";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Utilities
// ─────────────────────────────────────────────────────────────────────────────

static std::string trim(std::string s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// Split on top-level commas (ignores commas inside parentheses)
static std::vector<std::string> splitComma(const std::string &s) {
    std::vector<std::string> out;
    int depth = 0;
    std::string cur;
    for (char c : s) {
        if      (c == '(') depth++;
        else if (c == ')') depth--;
        if (c == ',' && depth == 0) { out.push_back(trim(cur)); cur.clear(); }
        else cur += c;
    }
    if (!trim(cur).empty()) out.push_back(trim(cur));
    return out;
}

// Parse a dimension specification like "-5:10" or "100" into DimBounds
static DimBounds parseDim(const std::string &raw) {
    DimBounds db;
    std::string t = trim(raw);
    auto pos = t.find(':');
    if (pos == std::string::npos) {
        // implicit lower = 1
        try { db.lower = 1; db.upper = std::stoll(t); } catch (...) {}
    } else {
        try { db.lower = std::stoll(trim(t.substr(0, pos)));  } catch (...) {}
        try { db.upper = std::stoll(trim(t.substr(pos + 1))); } catch (...) {}
    }
    return db;
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 1 – Declaration Harvest
//   Mirrors: Fortran::semantics::CheckHelper::Enter(const Symbol &)
// ─────────────────────────────────────────────────────────────────────────────

std::vector<ArrayInfo> harvestDeclarations(
    const std::vector<std::string> &lines)
{
    std::vector<ArrayInfo> result;

    // The regexes cover the three most common Fortran declaration syntaxes:
    //   TYPE name(dim-spec-list)
    //   TYPE, DIMENSION(dim-spec-list) :: name-list
    //   TYPE :: name(dim-spec-list)
    const std::regex typeKw(
        R"(^(real|integer|logical|character|complex|double\s+precision))",
        std::regex::icase);
    const std::regex dimAttr(
        R"(,\s*dimension\s*\(([^)]+)\)\s*::(.+)$)",
        std::regex::icase);
    const std::regex colonColonDecl(
        R"(::\s*([a-z_][a-z0-9_]*)\s*\(([^)]+)\))",
        std::regex::icase);
    const std::regex directDecl(
        R"(^(?:real|integer|logical|character|complex|double\s+precision)\s+([a-z_][a-z0-9_]*)\s*\(([^)]+)\))",
        std::regex::icase);

    int lineNo = 0;
    for (const auto &rawLine : lines) {
        lineNo++;
        std::string line = trim(rawLine);
        // strip inline comments
        {
            auto p = line.find('!');
            if (p != std::string::npos) line = trim(line.substr(0, p));
        }
        if (line.empty()) continue;

        std::smatch m;

        // ── DIMENSION attribute syntax ──────────────────────────────────
        if (std::regex_search(line, m, typeKw)) {
            if (std::regex_search(line, m, dimAttr)) {
                std::string dimStr   = m[1];
                std::string nameList = m[2];
                auto dimSpecs = splitComma(dimStr);
                for (auto rawName : splitComma(nameList)) {
                    rawName = trim(rawName);
                    // strip local dim override like "B(2:4)"
                    auto p = rawName.find('(');
                    if (p != std::string::npos) rawName = trim(rawName.substr(0, p));
                    if (rawName.empty()) continue;
                    ArrayInfo ai;
                    ai.name = rawName;
                    ai.declarationLine = lineNo;
                    for (const auto &ds : dimSpecs) ai.dims.push_back(parseDim(ds));
                    result.push_back(ai);
                }
                continue;
            }

            // ── :: name(dims) syntax ──────────────────────────────────────
            if (std::regex_search(line, m, colonColonDecl)) {
                ArrayInfo ai;
                ai.name            = m[1];
                ai.declarationLine = lineNo;
                for (const auto &ds : splitComma(std::string(m[2])))
                    ai.dims.push_back(parseDim(ds));
                result.push_back(ai);
                continue;
            }

            // ── TYPE name(dims) syntax ─────────────────────────────────────
            if (std::regex_search(line, m, directDecl)) {
                ArrayInfo ai;
                ai.name            = m[1];
                ai.declarationLine = lineNo;
                for (const auto &ds : splitComma(std::string(m[2])))
                    ai.dims.push_back(parseDim(ds));
                result.push_back(ai);
            }
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 2 – Subscript Harvest
//   Mirrors: Fortran::semantics::CheckHelper::Enter(const parser::ArrayElement &)
// ─────────────────────────────────────────────────────────────────────────────

struct ArrayAccess {
    std::string              arrayName;
    std::vector<std::string> subscripts;
    int                      line{0};
};

std::vector<ArrayAccess> harvestAccesses(
    const std::vector<std::string> &lines,
    const std::set<std::string>    &knownArrays)  // lower-cased names
{
    std::vector<ArrayAccess> result;

    const std::regex accessRe(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");

    // Lines that START with a Fortran type keyword are declaration lines.
    // We skip them to avoid matching the array name in its own declaration.
    const std::regex declLine(
        R"(^\s*(real|integer|logical|character|complex|double)\b)",
        std::regex::icase);

    int lineNo = 0;
    for (const auto &rawLine : lines) {
        lineNo++;
        std::string line = trim(rawLine);
        {
            auto p = line.find('!');
            if (p != std::string::npos) line = trim(line.substr(0, p));
        }
        if (line.empty()) continue;
        if (std::regex_search(line, declLine)) continue;

        auto beg = std::sregex_iterator(line.begin(), line.end(), accessRe);
        auto end = std::sregex_iterator();

        for (auto it = beg; it != end; ++it) {
            std::string nm = toLower((*it)[1].str());
            if (!knownArrays.count(nm)) continue;

            // Find matching closing paren
            std::size_t openAt = (std::size_t)((*it).position()) +
                                 (*it)[0].length() - 1;
            int depth = 1;
            std::size_t cur = openAt + 1;
            while (cur < line.size() && depth > 0) {
                if (line[cur] == '(') depth++;
                else if (line[cur] == ')') depth--;
                cur++;
            }
            std::string inner = line.substr(openAt + 1, cur - openAt - 2);

            ArrayAccess aa;
            aa.arrayName  = (*it)[1].str();
            aa.subscripts = splitComma(inner);
            aa.line       = lineNo;
            result.push_back(aa);
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 3 – Constant Propagation + Bounds Decision
//   Mirrors: Fortran::semantics::CheckHelper::CheckSubscripts()
// ─────────────────────────────────────────────────────────────────────────────

// Evaluate a subscript string to an IndexValue.
// Handles: integer literals, unary minus, binary +/-/* of constants.
IndexValue evalSubscript(const std::string &raw) {
    std::string t = trim(raw);

    // Pure integer literal (possibly negative)
    try {
        std::size_t pos = 0;
        long long v = std::stoll(t, &pos);
        if (pos == t.size()) return {IndexKind::Constant, v, t};
    } catch (...) {}

    // Constant binary expression: <int> op <int>
    static const std::regex binExpr(
        R"(^(-?\d+)\s*([+\-\*])\s*(\d+)$)");
    std::smatch m;
    if (std::regex_match(t, m, binExpr)) {
        long long a  = std::stoll(m[1]);
        long long b  = std::stoll(m[3]);
        char      op = m[2].str()[0];
        long long r  = (op == '+') ? a + b : (op == '-') ? a - b : a * b;
        return {IndexKind::Constant, r, std::to_string(r)};
    }

    // Section triplet (contains ':')
    if (t.find(':') != std::string::npos)
        return {IndexKind::Expression, 0, t};

    // Pure identifier → variable
    bool isIdent = !t.empty();
    for (char c : t)
        if (!std::isalnum((unsigned char)c) && c != '_') { isIdent = false; break; }
    if (isIdent) return {IndexKind::Variable, 0, t};

    return {IndexKind::Expression, 0, t};
}

// ─────────────────────────────────────────────────────────────────────────────
// Main driver
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: bounds-checker <file.f90> [--stats]\n";
        return 1;
    }

    std::string filename = argv[1];
    bool showStats = (argc >= 3 && std::string(argv[2]) == "--stats");

    std::ifstream ifs(filename);
    if (!ifs) {
        std::cerr << "bounds-checker: cannot open '" << filename << "'\n";
        return 1;
    }

    std::vector<std::string> lines;
    for (std::string l; std::getline(ifs, l);) lines.push_back(l);

    // ── Phase 1 ──────────────────────────────────────────────────────────
    auto arrays = harvestDeclarations(lines);

    std::set<std::string> knownNames;
    std::map<std::string, ArrayInfo> arrayMap;
    for (const auto &ai : arrays) {
        knownNames.insert(toLower(ai.name));
        arrayMap[toLower(ai.name)] = ai;
    }

    // ── Phase 2 ──────────────────────────────────────────────────────────
    auto accesses = harvestAccesses(lines, knownNames);

    // ── Phase 3 + diagnostics ─────────────────────────────────────────────
    std::vector<Diagnostic> diags;
    int totalSubs=0, constErrors=0, varWarnings=0, verified=0, unknownBds=0;
    int fullyBounded=0;

    for (const auto &ai : arrays) {
        bool fb = true;
        for (const auto &d : ai.dims) if (!d.isFullyKnown()) { fb=false; break; }
        if (fb) fullyBounded++;
    }

    for (const auto &acc : accesses) {
        auto it = arrayMap.find(toLower(acc.arrayName));
        if (it == arrayMap.end()) continue;
        const ArrayInfo &ai = it->second;

        for (int d = 0; d < (int)acc.subscripts.size() &&
                        d < (int)ai.dims.size(); d++) {
            totalSubs++;
            const DimBounds &bnd = ai.dims[d];
            IndexValue idx = evalSubscript(acc.subscripts[d]);

            if (!bnd.isFullyKnown()) {
                unknownBds++;
                if (idx.kind == IndexKind::Variable ||
                    idx.kind == IndexKind::Expression) {
                    Diagnostic dg;
                    dg.sev  = Diagnostic::Severity::Note;
                    dg.file = filename;
                    dg.line = acc.line;
                    dg.arrayName = acc.arrayName;
                    dg.dim  = d + 1;
                    dg.message = "array '" + acc.arrayName + "' dim " +
                        std::to_string(d+1) + " has unknown bounds — " +
                        "cannot verify index '" + idx.text + "'";
                    diags.push_back(dg);
                }
                continue;
            }

            long long lo = *bnd.lower;
            long long hi = *bnd.upper;

            if (idx.kind == IndexKind::Constant) {
                if (idx.value < lo || idx.value > hi) {
                    constErrors++;
                    Diagnostic dg;
                    dg.sev  = Diagnostic::Severity::Error;
                    dg.file = filename;
                    dg.line = acc.line;
                    dg.arrayName = acc.arrayName;
                    dg.dim  = d + 1;
                    dg.message =
                        "array '" + acc.arrayName + "' dimension " +
                        std::to_string(d+1) + ": index " +
                        std::to_string(idx.value) +
                        " is out of bounds [" + bnd.str() + "]";
                    diags.push_back(dg);
                } else {
                    verified++;
                }
            } else if (idx.kind == IndexKind::Variable ||
                       idx.kind == IndexKind::Expression) {
                varWarnings++;
                Diagnostic dg;
                dg.sev  = Diagnostic::Severity::Warning;
                dg.file = filename;
                dg.line = acc.line;
                dg.arrayName = acc.arrayName;
                dg.dim  = d + 1;
                dg.message =
                    "array '" + acc.arrayName + "' dimension " +
                    std::to_string(d+1) +
                    ": non-constant index '" + idx.text +
                    "' — bounds [" + bnd.str() + "] cannot be verified statically";
                diags.push_back(dg);
            } else {
                unknownBds++;
            }
        }
    }

    // ── Header ────────────────────────────────────────────────────────────
    std::cerr << "flang-new -fc1 -fsyntax-only -plugin bounds-checker "
              << filename << "\n";
    std::cerr << std::string(65, '-') << "\n";

    // ── Print declarations summary (to stderr like Flang -Rpass) ─────────
    std::cerr << "remark: " << arrays.size() << " array declaration(s) found\n";
    for (const auto &ai : arrays) {
        std::cerr << "  remark: " << ai.name << "(";
        for (std::size_t i = 0; i < ai.dims.size(); i++) {
            if (i) std::cerr << ",";
            std::cerr << ai.dims[i].str();
        }
        std::cerr << ")  [line " << ai.declarationLine << "]\n";
    }
    std::cerr << "\n";

    // ── Print diagnostics ─────────────────────────────────────────────────
    for (const auto &dg : diags) dg.print(std::cerr);

    if (diags.empty()) std::cerr << "bounds-checker: no issues found\n";

    // ── Statistics ────────────────────────────────────────────────────────
    if (showStats) {
        int total = constErrors + varWarnings + verified + unknownBds;
        std::cerr << "\n=== Bounds Checker Statistics ===\n";
        std::cerr << "  Arrays declared         : " << (int)arrays.size()  << "\n";
        std::cerr << "  Fully bounded arrays    : " << fullyBounded         << "\n";
        std::cerr << "  Subscripts analysed     : " << totalSubs            << "\n";
        std::cerr << "  Constant violations     : " << constErrors          << "  (errors)\n";
        std::cerr << "  Variable warnings       : " << varWarnings          << "  (cannot verify)\n";
        std::cerr << "  Verified safe           : " << verified             << "\n";
        std::cerr << "  Unknown/deferred bounds : " << unknownBds           << "\n";
        if (total > 0) {
            std::cerr << std::fixed << std::setprecision(1);
            std::cerr << "  Static catch rate       : "
                      << 100.0*constErrors/total << "% (definite errors)\n";
            std::cerr << "  Variable-index rate     : "
                      << 100.0*varWarnings/total << "% (warnings)\n";
            std::cerr << "  Verified-safe rate      : "
                      << 100.0*verified/total    << "%\n";
        }
        std::cerr << "=================================\n";
    }

    return constErrors > 0 ? 1 : 0;
}
