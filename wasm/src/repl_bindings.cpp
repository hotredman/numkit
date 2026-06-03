#include <emscripten/bind.h>
#include <string>
#include <sstream>
#include <memory>
#include <iostream>
#include <map>
#include <vector>
#include <set>
#include <cmath>
#include <algorithm>

#include <numkit/core/engine.hpp>
#include <numkit/core/value_stats.hpp>
#include <numkit/core/value_json.hpp>
#include <numkit/builtin/library.hpp>
#include <numkit/core/debug_session.hpp>
#include <numkit/core/vfs.hpp>
#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>
#include <numkit/graph/ast_serialize.hpp>
#include <numkit/graph/lowering.hpp>
#include <numkit/graph/serialize.hpp>

// ════════════════════════════════════════════════════════════════
// Helper: format Value for variable preview
// ════════════════════════════════════════════════════════════════
static std::string valuePreview(const numkit::Value &val) {
    using numkit::ValueType;
    try {
        if (val.isScalar()) {
            if (val.type() == ValueType::DOUBLE) {
                double v = val.toScalar();
                if (std::isnan(v)) return "NaN";
                if (std::isinf(v)) return v > 0 ? "Inf" : "-Inf";
                if (v == static_cast<int64_t>(v) && std::abs(v) < 1e15)
                    return std::to_string(static_cast<int64_t>(v));
                std::ostringstream os; os << v; return os.str();
            }
            if (val.type() == ValueType::LOGICAL)
                return val.toBool() ? "true" : "false";
            if (val.type() == ValueType::COMPLEX) {
                auto c = val.toComplex();
                std::ostringstream os;
                os << c.real();
                if (c.imag() >= 0) os << "+";
                os << c.imag() << "i";
                return os.str();
            }
            if (numkit::isIntegerType(val.type()))
                return numkit::numericCellJSON(val, 0);
            if (val.type() == ValueType::SINGLE) {
                double v = val.elemAsDouble(0);
                if (std::isnan(v)) return "NaN";
                if (std::isinf(v)) return v > 0 ? "Inf" : "-Inf";
                if (v == static_cast<int64_t>(v) && std::abs(v) < 1e15)
                    return std::to_string(static_cast<int64_t>(v));
                std::ostringstream os; os << v; return os.str();
            }
        }
        if (val.type() == ValueType::CHAR)
            return "'" + val.toString() + "'";
        auto &d = val.dims();
        std::ostringstream os;
        os << "[" << d.rows() << "x" << d.cols();
        if (d.is3D()) os << "x" << d.pages();
        os << " " << numkit::mtypeName(val.type()) << "]";
        const ValueType pt = val.type();
        if ((numkit::isFloatType(pt) || numkit::isIntegerType(pt)) && val.numel() <= 10) {
            os << " [";
            for (size_t i = 0; i < val.numel(); ++i) {
                if (i) os << " ";
                if (numkit::isIntegerType(pt)) { os << numkit::numericCellJSON(val, i); continue; }
                double v = val.elemAsDouble(i);
                if (v == static_cast<int64_t>(v) && std::abs(v) < 1e15)
                    os << static_cast<int64_t>(v);
                else
                    os << v;
            }
            os << "]";
        }
        return os.str();
    } catch (...) {
        return "<error>";
    }
}

static std::string escapeJSON(const std::string &s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '"':  result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                result += buf;
            } else {
                result += c;
            }
        }
    }
    return result;
}

// ════════════════════════════════════════════════════════════════
// Matrix cell-data serialization — shared by getVarFullJSON and the
// path-inspector's matrix payload (no duplicated number formatting).
// ════════════════════════════════════════════════════════════════
//
// Emits the JSON value for a matrix-like Value's cells: a 2-D array
// [[...],...] (column-major source → row-major JSON). CHAR becomes one
// row of 1-char strings. Non-numeric / non-char fall back to a 1×1
// array holding the preview string.
static void emitMatrixDataArray(std::ostringstream &os, const numkit::Value &val,
                                size_t page = 0) {
    using numkit::ValueType;
    const auto &d = val.dims();
    const size_t rows = d.rows();
    const size_t cols = d.cols();
    // Page p is the contiguous block [p*rows*cols, …) — column-major slices
    // stack one after another regardless of rank, so a single linear offset
    // addresses any 2-D slice of a 3-D / N-D array (0 for 2-D / page 0).
    const size_t pageOff = page * rows * cols;
    os << "[";
    if (val.type() == ValueType::CHAR) {
        std::string str = val.toString();
        os << "[";
        for (size_t i = 0; i < str.size(); ++i) {
            if (i) os << ",";
            os << "\"" << escapeJSON(std::string(1, str[i])) << "\"";
        }
        os << "]";
    } else if (numkit::isRealNumericCell(val.type())) {
        // DOUBLE / SINGLE / LOGICAL / INT8..UINT64 — one array per matrix
        // row; cells via the shared numericCellJSON token (integers exact,
        // floats NaN/Inf-aware, logical true/false).
        for (size_t r = 0; r < rows; ++r) {
            if (r) os << ",";
            os << "[";
            for (size_t c = 0; c < cols; ++c) { if (c) os << ","; os << numkit::numericCellJSON(val, pageOff + c * rows + r); }
            os << "]";
        }
    } else if (val.type() == ValueType::COMPLEX) {
        const numkit::Complex *p = val.complexData();
        for (size_t r = 0; r < rows; ++r) {
            if (r) os << ",";
            os << "[";
            for (size_t c = 0; c < cols; ++c) {
                if (c) os << ",";
                const auto &z = p[pageOff + c * rows + r];
                std::ostringstream s; s.precision(12);
                s << z.real(); if (z.imag() >= 0) s << "+"; s << z.imag() << "i";
                os << "\"" << s.str() << "\"";
            }
            os << "]";
        }
    } else {
        os << "[\"" << escapeJSON(valuePreview(val)) << "\"]";
    }
    os << "]";
}

// ════════════════════════════════════════════════════════════════
// Path-addressed inspection (MATLAB-style drill-in)
// ════════════════════════════════════════════════════════════════
//
// A path is a compact ';'-delimited string of typed steps the JS builds:
//   "f:data;e:2;c:3"  →  root.data, then struct-array element 2, then cell 3
// Step kinds: f = struct field (value = name), e = element index (struct
// array / matrix), c = cell index. Indices are 0-based. MATLAB field
// names are identifiers (no ';'/':'), so the delimited form is
// unambiguous and needs no JSON parser. Empty string = the root itself.
//
// Above MATRIX_INSPECT_CAP elements a drilled matrix returns shape only
// (truncated:true) — full inline data would freeze the UI. Path-addressed
// tiling of sub-values is a deliberate follow-up.
static constexpr size_t MATRIX_INSPECT_CAP = 250000;

struct PathStep { char kind; std::string name; size_t idx; };

static std::vector<PathStep> parseInspectPath(const std::string &s) {
    std::vector<PathStep> steps;
    size_t i = 0;
    while (i < s.size()) {
        size_t semi = s.find(';', i);
        std::string tok = s.substr(i, semi == std::string::npos ? std::string::npos : semi - i);
        i = (semi == std::string::npos) ? s.size() : semi + 1;
        if (tok.empty()) continue;
        size_t colon = tok.find(':');
        if (colon == std::string::npos) continue;
        PathStep st{};
        st.kind = tok[0];
        const std::string val = tok.substr(colon + 1);
        if (st.kind == 'f') st.name = val;
        else st.idx = static_cast<size_t>(std::strtoull(val.c_str(), nullptr, 10));
        steps.push_back(st);
    }
    return steps;
}

// Walk the steps from `root`. field()/cellAt() return references into
// the existing tree (no copy); elemAt() materialises a temporary kept
// alive in `owned` (reserved up front so no realloc invalidates the
// returned pointer). Returns nullptr on any out-of-range / type-mismatch.
static const numkit::Value *resolveInspectPath(const numkit::Value &root,
                                               const std::vector<PathStep> &steps,
                                               std::vector<numkit::Value> &owned) {
    owned.reserve(steps.size());
    const numkit::Value *cur = &root;
    for (const auto &st : steps) {
        if (st.kind == 'f') {
            if (!cur->isStruct() || !cur->hasField(st.name)) return nullptr;
            cur = &cur->field(st.name);
        } else if (st.kind == 'e') {
            if (st.idx >= cur->numel()) return nullptr;
            owned.push_back(cur->elemAt(st.idx));
            cur = &owned.back();
        } else if (st.kind == 'c') {
            if (!cur->isCell() || st.idx >= cur->numel()) return nullptr;
            cur = &cur->cellAt(st.idx);
        } else {
            return nullptr;
        }
    }
    return cur;
}

// One cell descriptor for a struct-table / cell-grid: type + size +
// preview summary + whether double-click should drill into it. Drillable
// = struct / cell / a multi-element non-char array (worth a table).
// Full round-trip JSON number (matches the precision the stats query uses).
static std::string jnum(double x) {
    std::ostringstream o; o.precision(17); o << x; return o.str();
}

// Serialize the display-stat columns as a JSON object, or "" when the
// value isn't numeric. Shared by emitInspectCell + the workspace list.
static std::string statsJSON(const numkit::Value &val) {
    numkit::ValueStats st;
    if (!numkit::computeValueStats(val, st)) return "";
    std::ostringstream os;
    os << "\"stats\":{\"min\":" << jnum(st.min) << ",\"max\":" << jnum(st.max)
       << ",\"mean\":" << jnum(st.mean) << ",\"median\":" << jnum(st.median)
       << ",\"mode\":" << jnum(st.mode) << ",\"var\":" << jnum(st.var)
       << ",\"std\":" << jnum(st.std) << "}";
    return os.str();
}

static void emitInspectCell(std::ostringstream &os, const std::string &label,
                            const numkit::Value &val) {
    const auto &d = val.dims();
    // Every field is openable, matching MATLAB's Variable Editor: a
    // scalar opens as a 1x1 table, a string as a 1xN char table, a
    // matrix as its grid, struct/cell drill further. Keeping the `drill`
    // field (rather than dropping it) leaves room to mark a future type
    // as non-openable; today it's unconditionally true.
    const bool drill = true;
    os << "{";
    if (!label.empty()) os << "\"label\":\"" << escapeJSON(label) << "\",";
    os << "\"type\":\"" << numkit::mtypeName(val.type()) << "\""
       << ",\"size\":\"" << d.rows() << "x" << d.cols();
    if (d.is3D()) os << "x" << d.pages();
    os << "\",\"summary\":\"" << escapeJSON(valuePreview(val)) << "\""
       << ",\"drill\":" << (drill ? "true" : "false");
    std::string sj = statsJSON(val);
    if (!sj.empty()) os << "," << sj;
    os << "}";
}

// Build the inspect payload for a resolved value:
//   STRUCT → { kind:"struct", rows, cols, numel, fields:[], elems:[[cell]] }
//   CELL   → { kind:"cell",   rows, cols, elems:[cell] }   (column-major)
//   else   → { kind:"matrix", type, rows, cols, data | truncated }
static std::string emitInspectPayload(const numkit::Value &val) {
    using numkit::ValueType;
    std::ostringstream os;
    const auto &d = val.dims();

    if (val.isStruct()) {
        const auto order = val.fieldNamesInOrder();
        const size_t n = val.numel();
        os << "{\"kind\":\"struct\",\"rows\":" << d.rows() << ",\"cols\":" << d.cols()
           << ",\"numel\":" << n << ",\"fields\":[";
        for (size_t f = 0; f < order.size(); ++f) {
            if (f) os << ",";
            os << "\"" << escapeJSON(order[f]) << "\"";
        }
        os << "],\"elems\":[";
        // One inner array per element; cells in field order. Single
        // struct uses structFields(); arrays use structArrayElem(i).
        const bool isArr = val.isStructArray();
        for (size_t e = 0; e < n; ++e) {
            if (e) os << ",";
            const auto &fields = isArr ? val.structArrayElem(e) : val.structFields();
            os << "[";
            for (size_t f = 0; f < order.size(); ++f) {
                if (f) os << ",";
                auto it = fields.find(order[f]);
                if (it == fields.end()) os << "{\"type\":\"\",\"size\":\"\",\"summary\":\"\",\"drill\":false}";
                else emitInspectCell(os, "", it->second);
            }
            os << "]";
        }
        os << "]}";
        return os.str();
    }

    if (val.isCell()) {
        const size_t rows = d.rows(), cols = d.cols(), n = val.numel();
        os << "{\"kind\":\"cell\",\"rows\":" << rows << ",\"cols\":" << cols << ",\"elems\":[";
        // Column-major linear order; label "{r,c}" 1-based.
        for (size_t i = 0; i < n; ++i) {
            if (i) os << ",";
            const size_t r = (rows > 0) ? (i % rows) : 0;
            const size_t c = (rows > 0) ? (i / rows) : 0;
            const std::string label = "{" + std::to_string(r + 1) + "," + std::to_string(c + 1) + "}";
            emitInspectCell(os, label, val.cellAt(i));
        }
        os << "]}";
        return os.str();
    }

    // Matrix / scalar / char / etc.
    os << "{\"kind\":\"matrix\",\"type\":\"" << numkit::mtypeName(val.type()) << "\""
       << ",\"rows\":" << d.rows() << ",\"cols\":" << d.cols();
    if (val.numel() > MATRIX_INSPECT_CAP) {
        os << ",\"truncated\":true}";
    } else {
        os << ",\"data\":";
        emitMatrixDataArray(os, val);
        os << "}";
    }
    return os.str();
}

// ════════════════════════════════════════════════════════════════
// ReplSession
// ════════════════════════════════════════════════════════════════
class ReplSession {
public:
    ReplSession() {
        engine_ = std::make_unique<numkit::Engine>();
        restoreOutputFunc();
    }

    // ── Virtual filesystem bridge ──
    //
    // The IDE registers one JS object per named filesystem (typically
    // "temporary" and "local"), exposing sync methods readFile(path),
    // writeFile(path, content), exists(path). We wrap each in a
    // CallbackFS and hand it to the engine. Handlers are cached so
    // reset() (which rebuilds the engine) can re-install them.
    //
    // Note: callbacks are SYNC — the IDE-side adapter must either keep
    // a sync-accessible mirror of tempFS/localFS or rely on Asyncify.
    void registerFs(const std::string &name, emscripten::val handler) {
        fsHandlers_[name] = handler;
        installFs(name, handler);
    }

    void pushScriptOrigin(const std::string &fsName) {
        engine_->pushScriptOrigin(fsName);
    }
    void pushScriptOriginWithDir(const std::string &fsName, const std::string &scriptDir) {
        engine_->pushScriptOrigin(fsName, scriptDir);
    }
    void popScriptOrigin() { engine_->popScriptOrigin(); }

    // Build timestamp — just calls the in-engine `version` builtin
    // and returns its string. Single source of truth in library.cpp.
    std::string version() {
        try {
            auto v = engine_->eval("version;");
            return v.toString();
        } catch (...) {
            return "unknown";
        }
    }

    std::string execute(const std::string& code) {
        // During active debug session, evaluate in the current frame context
        if (debugSession_ && debugSession_->isActive()) {
            return debugEval(code);
        }

        outputBuf_.clear();

        auto r = engine_->evalSafe(code);

        std::string output = outputBuf_;

        if (r.ok) {
            while (!output.empty() &&
                   (output.back() == '\n' || output.back() == ' '))
                output.pop_back();
            return output;
        }

        if (!output.empty() && output.back() != '\n')
            output += '\n';
        if (r.errorLine > 0) {
            output += "__ERROR_LINE__:" + std::to_string(r.errorLine) + "\n";
            output += "Error (line " + std::to_string(r.errorLine) + "): " + r.errorMessage;
            if (!r.errorContext.empty())
                output += " (" + r.errorContext + ")";
        } else {
            output += "Error: " + r.errorMessage;
            if (!r.errorContext.empty())
                output += " (" + r.errorContext + ")";
        }
        return output;
    }

    // Evaluate expression in debug context (saves/restores VM paused state)
    std::string debugEval(const std::string &code) {
        return debugSession_->eval(code);
    }

    // ── Debug API (clean, no replay) ──

    std::string debugStart(const std::string &code) {
        debugSession_ = std::make_unique<numkit::DebugSession>(*engine_);

        // Set breakpoints from saved list
        debugSession_->setBreakpoints(breakpointLines_);

        auto status = debugSession_->start(code);
        return buildDebugResult(status);
    }

    std::string debugResume(int action) {
        if (!debugSession_ || !debugSession_->isActive())
            return "{\"status\":\"completed\"}";

        auto da = static_cast<numkit::DebugAction>(action);
        auto status = debugSession_->resume(da);
        return buildDebugResult(status);
    }

    void debugStop() {
        if (debugSession_)
            debugSession_->stop();
        debugSession_.reset();
        restoreOutputFunc();
    }

    void setBreakpoints(const std::string &linesJson) {
        // Reset state FIRST — if the JSON is malformed, we still want the
        // engine's breakpoint manager to end up empty, not to retain a
        // stale set from the previous call. A previous bug here caused
        // "removed" breakpoints to keep firing because the early return
        // on a bad parse skipped the bpm.clearAll().
        breakpointLines_.clear();
        auto &bpm = engine_->breakpointManager();
        bpm.clearAll();

        // Parse simple JSON array: [1, 5, 10]
        std::string s = linesJson;
        size_t start = s.find('[');
        size_t end = s.rfind(']');
        if (start == std::string::npos || end == std::string::npos) return;
        s = s.substr(start + 1, end - start - 1);

        std::istringstream iss(s);
        std::string token;
        while (std::getline(iss, token, ',')) {
            int line = 0;
            try { line = std::stoi(token); } catch (...) { continue; }
            if (line > 0) breakpointLines_.push_back(static_cast<uint16_t>(line));
        }

        for (auto line : breakpointLines_)
            bpm.addBreakpoint(line);
    }

    void reset() {
        debugSession_.reset();
        engine_ = std::make_unique<numkit::Engine>();
        restoreOutputFunc();
        // Re-install VFS handlers on the fresh engine so csvread/csvwrite
        // keep routing through tempFS/localFS after a reset.
        for (auto &[name, handler] : fsHandlers_)
            installFs(name, handler);
    }

    std::string getWorkspace() {
        outputBuf_.clear();
        try {
            engine_->eval("whos");
            std::string out = outputBuf_;
            if (!out.empty()) return out;
        } catch (...) {}
        return "No variables in workspace.";
    }

    std::string getWorkspaceJSON() {
        // During active debug session, return frame variables instead of workspace
        if (debugSession_ && debugSession_->isActive()) {
            return getDebugFrameVarsJSON();
        }
        try {
            return engine_->workspaceJSON();
        } catch (...) {
            return "{}";
        }
    }

    /**
     * Serialise a single workspace variable as a JSON object containing its
     * full numeric data, suitable for the Variable Editor table:
     *
     *   { "name":"x", "type":"double", "rows":M, "cols":N,
     *     "data":[[r0c0, r0c1, ...], [r1c0, ...], ...] }
     *
     * For non-numeric types we fall back to a single-cell preview string.
     * Storage in numkit is column-major (MATLAB convention) — we transpose
     * to row-major here so the table reads naturally.
     */
    /* ---- Cheap dimension-only query (no data) ---- */
    std::string getVarShapeJSON(const std::string &name) {
        try {
            const numkit::Value *valPtr = nullptr;
            if (debugSession_ && debugSession_->isActive()) {
                auto snap = debugSession_->snapshot();
                for (auto &v : snap.variables) {
                    if (v.name == name && v.value) { valPtr = v.value; break; }
                }
            }
            if (!valPtr) valPtr = engine_->getVariable(name);
            if (!valPtr) {
                return "{\"error\":\"variable '" + escapeJSON(name) + "' not found\"}";
            }
            const auto &val = *valPtr;
            const auto &d = val.dims();
            // pages = number of 2-D row×col slices (1 for 2-D). dims carries
            // the full shape so the viewer can build per-dimension (MATLAB
            // A(:,:,k3,…)) page navigation; a slice p is the contiguous
            // block [p*rows*cols, (p+1)*rows*cols).
            const size_t rc = d.rows() * d.cols();
            const size_t pages = (rc > 0) ? (val.numel() / rc) : 1;
            std::ostringstream os;
            os << "{\"name\":\"" << escapeJSON(name) << "\""
               << ",\"type\":\"" << numkit::mtypeName(val.type()) << "\""
               << ",\"rows\":" << d.rows()
               << ",\"cols\":" << d.cols()
               << ",\"ndim\":" << d.ndim()
               << ",\"pages\":" << pages
               << ",\"dims\":[";
            for (int i = 0; i < d.ndim(); ++i) { if (i) os << ","; os << d.dim(i); }
            os << "]"
               << ",\"numel\":" << val.numel() << "}";
            return os.str();
        } catch (const std::exception &e) {
            return std::string("{\"error\":\"") + escapeJSON(e.what()) + "\"}";
        } catch (...) { return "{\"error\":\"unknown\"}"; }
    }

    /* ---- Aggregate stats over the full matrix (no copy, native speed) ---- */
    //
    // Used by VariableEditor to drive heatmap colouring on huge matrices
    // where loading every cell into JS would be too expensive. Walks the
    // backing array once at C++ speed and returns:
    //   { rows, cols, min, max, mean, n, hasNaN }
    // For LOGICAL true=1 / false=0; for COMPLEX |z|; non-numeric returns
    // {error}.
    std::string getVarStatsJSON(const std::string &name, int page = -1) {
        try {
            using numkit::ValueType;
            const numkit::Value *valPtr = nullptr;
            if (debugSession_ && debugSession_->isActive()) {
                auto snap = debugSession_->snapshot();
                for (auto &v : snap.variables) {
                    if (v.name == name && v.value) { valPtr = v.value; break; }
                }
            }
            if (!valPtr) valPtr = engine_->getVariable(name);
            if (!valPtr) return "{\"error\":\"variable not found\"}";
            const auto &val = *valPtr;
            const auto &d = val.dims();
            const size_t totalRows = d.rows();
            const size_t totalCols = d.cols();
            const size_t numel = val.numel();
            // Optional page restriction: when page>=0, stats cover only that
            // 2-D slice's block [page*rc, page*rc+rc) — drives per-slice
            // heatmap colouring for 3-D / N-D arrays in tile mode.
            const size_t rc = totalRows * totalCols;
            const size_t pages = (rc > 0) ? (numel / rc) : 1;
            size_t i0 = 0, i1 = numel;
            if (page >= 0 && (size_t)page < pages) { i0 = (size_t)page * rc; i1 = i0 + rc; }
            double mn = std::numeric_limits<double>::infinity();
            double mx = -std::numeric_limits<double>::infinity();
            double sum = 0.0;
            size_t n = 0;
            bool hasNaN = false;
            if (val.type() == ValueType::DOUBLE) {
                const double *p = val.doubleData();
                for (size_t i = i0; i < i1; ++i) {
                    double v = p[i];
                    if (std::isnan(v)) { hasNaN = true; continue; }
                    if (!std::isfinite(v)) continue;
                    if (v < mn) mn = v;
                    if (v > mx) mx = v;
                    sum += v;
                    ++n;
                }
            } else if (val.type() == ValueType::LOGICAL) {
                const uint8_t *p = val.logicalData();
                for (size_t i = i0; i < i1; ++i) {
                    double v = p[i] ? 1.0 : 0.0;
                    if (v < mn) mn = v;
                    if (v > mx) mx = v;
                    sum += v;
                    ++n;
                }
            } else if (val.type() == ValueType::COMPLEX) {
                const numkit::Complex *p = val.complexData();
                for (size_t i = i0; i < i1; ++i) {
                    double mag = std::hypot(p[i].real(), p[i].imag());
                    if (std::isnan(mag)) { hasNaN = true; continue; }
                    if (!std::isfinite(mag)) continue;
                    if (mag < mn) mn = mag;
                    if (mag > mx) mx = mag;
                    sum += mag;
                    ++n;
                }
            } else if (numkit::isFloatType(val.type()) || numkit::isIntegerType(val.type())) {
                // SINGLE + INT8..UINT64 — read each element as double.
                for (size_t i = i0; i < i1; ++i) {
                    double v = val.elemAsDouble(i);
                    if (std::isnan(v)) { hasNaN = true; continue; }
                    if (!std::isfinite(v)) continue;
                    if (v < mn) mn = v;
                    if (v > mx) mx = v;
                    sum += v;
                    ++n;
                }
            } else {
                return "{\"error\":\"non-numeric type\"}";
            }
            std::ostringstream os;
            os.precision(17);
            os << "{\"rows\":" << totalRows
               << ",\"cols\":" << totalCols
               << ",\"n\":" << n
               << ",\"hasNaN\":" << (hasNaN ? "true" : "false");
            if (n > 0) {
                os << ",\"min\":" << mn
                   << ",\"max\":" << mx
                   << ",\"mean\":" << (sum / n);
                // Full stat set (median/mode/var/std) for the matrix
                // StatsBar's column chooser — same helper the struct/
                // workspace serializers use.
                numkit::ValueStats fs;
                if (numkit::computeValueStatsRange(val, i0, i1 - i0, fs)) {
                    os << ",\"median\":" << fs.median
                       << ",\"mode\":" << fs.mode
                       << ",\"var\":" << fs.var
                       << ",\"std\":" << fs.std;
                }
            } else {
                os << ",\"min\":null,\"max\":null,\"mean\":null";
            }
            os << "}";
            return os.str();
        } catch (const std::exception &e) {
            return std::string("{\"error\":\"") + escapeJSON(e.what()) + "\"}";
        } catch (...) { return "{\"error\":\"unknown\"}"; }
    }

    /* ---- Tile fetch — only the requested rectangle of cells ---- */
    std::string getVarTileJSON(const std::string &name, int r0, int c0, int rowsIn, int colsIn, int page = 0) {
        try {
            using numkit::ValueType;
            const numkit::Value *valPtr = nullptr;
            if (debugSession_ && debugSession_->isActive()) {
                auto snap = debugSession_->snapshot();
                for (auto &v : snap.variables) {
                    if (v.name == name && v.value) { valPtr = v.value; break; }
                }
            }
            if (!valPtr) valPtr = engine_->getVariable(name);
            if (!valPtr) return "{\"error\":\"variable not found\"}";
            const auto &val = *valPtr;
            const auto &d = val.dims();
            const size_t totalRows = d.rows();
            const size_t totalCols = d.cols();
            // Page offset for 3-D / N-D: read from the selected 2-D slice.
            const size_t rc = totalRows * totalCols;
            const size_t pages = (rc > 0) ? (val.numel() / rc) : 1;
            if (page < 0) page = 0;
            if (pages > 0 && (size_t)page >= pages) page = (int)pages - 1;
            const size_t pageOff = (size_t)page * rc;
            if (r0 < 0) r0 = 0;
            if (c0 < 0) c0 = 0;
            const size_t rEnd = std::min(totalRows, (size_t)(r0 + rowsIn));
            const size_t cEnd = std::min(totalCols, (size_t)(c0 + colsIn));
            if ((size_t)r0 >= totalRows || (size_t)c0 >= totalCols
                || rEnd <= (size_t)r0 || cEnd <= (size_t)c0) {
                return "{\"error\":\"out of range\",\"r0\":" + std::to_string(r0)
                     + ",\"c0\":" + std::to_string(c0) + "}";
            }

            std::ostringstream os;
            os << "{\"r0\":" << r0
               << ",\"c0\":" << c0
               << ",\"rows\":" << (rEnd - r0)
               << ",\"cols\":" << (cEnd - c0)
               << ",\"page\":" << page
               << ",\"type\":\"" << numkit::mtypeName(val.type()) << "\""
               << ",\"data\":[";

            if (numkit::isRealNumericCell(val.type())) {
                // DOUBLE / SINGLE / LOGICAL / INT8..UINT64 — shared token.
                for (size_t r = (size_t)r0; r < rEnd; ++r) {
                    if (r > (size_t)r0) os << ",";
                    os << "[";
                    for (size_t c = (size_t)c0; c < cEnd; ++c) {
                        if (c > (size_t)c0) os << ",";
                        os << numkit::numericCellJSON(val, pageOff + c * totalRows + r);
                    }
                    os << "]";
                }
            } else if (val.type() == ValueType::COMPLEX) {
                const numkit::Complex *p = val.complexData();
                for (size_t r = (size_t)r0; r < rEnd; ++r) {
                    if (r > (size_t)r0) os << ",";
                    os << "[";
                    for (size_t c = (size_t)c0; c < cEnd; ++c) {
                        if (c > (size_t)c0) os << ",";
                        const auto &z = p[pageOff + c * totalRows + r];
                        std::ostringstream s;
                        s.precision(12);
                        s << z.real();
                        if (z.imag() >= 0) s << "+";
                        s << z.imag() << "i";
                        os << "\"" << s.str() << "\"";
                    }
                    os << "]";
                }
            } else if (val.type() == ValueType::CHAR) {
                const char *p = val.charData();
                for (size_t r = (size_t)r0; r < rEnd; ++r) {
                    if (r > (size_t)r0) os << ",";
                    os << "[";
                    for (size_t c = (size_t)c0; c < cEnd; ++c) {
                        if (c > (size_t)c0) os << ",";
                        char ch = p[pageOff + c * totalRows + r];
                        os << "\"" << escapeJSON(std::string(1, ch)) << "\"";
                    }
                    os << "]";
                }
            } else {
                for (size_t r = (size_t)r0; r < rEnd; ++r) {
                    if (r > (size_t)r0) os << ",";
                    os << "[";
                    for (size_t c = (size_t)c0; c < cEnd; ++c) {
                        if (c > (size_t)c0) os << ",";
                        os << "\"—\"";
                    }
                    os << "]";
                }
            }
            os << "]}";
            return os.str();
        } catch (const std::exception &e) {
            return std::string("{\"error\":\"") + escapeJSON(e.what()) + "\"}";
        } catch (...) { return "{\"error\":\"unknown\"}"; }
    }

    std::string getVarFullJSON(const std::string &name, int page = 0) {
        try {
            using numkit::ValueType;
            // During debug, prefer the paused frame's variable.
            const numkit::Value *valPtr = nullptr;
            if (debugSession_ && debugSession_->isActive()) {
                auto snap = debugSession_->snapshot();
                for (auto &v : snap.variables) {
                    if (v.name == name && v.value) { valPtr = v.value; break; }
                }
            }
            if (!valPtr) valPtr = engine_->getVariable(name);
            if (!valPtr) {
                return "{\"error\":\"variable '" + escapeJSON(name) + "' not found\"}";
            }
            const auto &val = *valPtr;
            const auto &d = val.dims();
            const size_t rows = d.rows();
            const size_t cols = d.cols();
            const size_t rc = rows * cols;
            const size_t pages = (rc > 0) ? (val.numel() / rc) : 1;
            if (page < 0) page = 0;
            if (pages > 0 && (size_t)page >= pages) page = (int)pages - 1;

            std::ostringstream os;
            os << "{\"name\":\"" << escapeJSON(name) << "\""
               << ",\"type\":\"" << numkit::mtypeName(val.type()) << "\""
               << ",\"rows\":" << rows
               << ",\"cols\":" << cols
               << ",\"page\":" << page
               << ",\"pages\":" << pages
               << ",\"data\":";
            // Cell-data array — shared with the path-inspector's matrix
            // payload via emitMatrixDataArray (CHAR / DOUBLE / LOGICAL /
            // COMPLEX, with the CELL/STRUCT/FUNC preview fallback). page
            // selects the 2-D slice for 3-D / N-D arrays.
            emitMatrixDataArray(os, val, (size_t)page);
            os << "}";
            return os.str();
        } catch (const std::exception &e) {
            return std::string("{\"error\":\"") + escapeJSON(e.what()) + "\"}";
        } catch (...) {
            return "{\"error\":\"unknown error\"}";
        }
    }

    // Path-addressed inspection for the MATLAB-style drill-in Variable
    // Editor. Resolves `pathStr` (";"-delimited typed steps; "" = root)
    // against the named variable, then returns the struct-table /
    // cell-grid / matrix payload for the resolved sub-value. Honours the
    // paused debug frame's variable when a debug session is active.
    std::string getInspectPathJSON(const std::string &name, const std::string &pathStr) {
        try {
            const numkit::Value *rootPtr = nullptr;
            if (debugSession_ && debugSession_->isActive()) {
                auto snap = debugSession_->snapshot();
                for (auto &v : snap.variables) {
                    if (v.name == name && v.value) { rootPtr = v.value; break; }
                }
            }
            if (!rootPtr) rootPtr = engine_->getVariable(name);
            if (!rootPtr) {
                return "{\"error\":\"variable '" + escapeJSON(name) + "' not found\"}";
            }
            auto steps = parseInspectPath(pathStr);
            std::vector<numkit::Value> owned;
            const numkit::Value *cur = resolveInspectPath(*rootPtr, steps, owned);
            if (!cur) return "{\"error\":\"invalid path\"}";
            return emitInspectPayload(*cur);
        } catch (const std::exception &e) {
            return std::string("{\"error\":\"") + escapeJSON(e.what()) + "\"}";
        } catch (...) {
            return "{\"error\":\"unknown error\"}";
        }
    }

    std::string getDebugFrameVarsJSON() {
        try {
            auto snap = debugSession_->snapshot();
            std::string result = "{";
            bool first = true;
            for (auto &v : snap.variables) {
                if (!v.value) continue;
                if (v.name == "nargin" || v.name == "nargout") continue;
                if (!first) result += ",";
                auto &val = *v.value;
                result += "\"" + escapeJSON(v.name) + "\":{";
                result += "\"type\":\"" + std::string(numkit::mtypeName(val.type())) + "\"";
                auto &d = val.dims();
                result += ",\"size\":\"" + std::to_string(d.rows()) + "x" + std::to_string(d.cols());
                if (d.is3D()) result += "x" + std::to_string(d.pages());
                result += "\"";
                result += ",\"preview\":";
                if (val.type() == numkit::ValueType::DOUBLE && val.isScalar()) {
                    double dv = val.toScalar();
                    if (std::isnan(dv)) result += "\"NaN\"";
                    else if (std::isinf(dv)) result += (dv > 0 ? "\"Inf\"" : "\"-Inf\"");
                    else result += std::to_string(dv);
                } else if (val.type() == numkit::ValueType::LOGICAL && val.isScalar()) {
                    result += (val.toBool() ? "true" : "false");
                } else if (val.type() == numkit::ValueType::CHAR) {
                    result += "\"" + escapeJSON(val.toString()) + "\"";
                } else {
                    result += "\"" + escapeJSON(valuePreview(val)) + "\"";
                }
                result += "}";
                first = false;
            }
            result += "}";
            return result;
        } catch (...) {
            return "{}";
        }
    }

    std::string complete(const std::string& partial) {
        if (partial.empty()) return "";
        static const char* keywords[] = {
            "break","case","catch","continue","else","elseif","end",
            "for","function","global","if","otherwise","return",
            "switch","try","while",
            "zeros","ones","eye","rand","randn","linspace","logspace",
            "reshape","meshgrid","size","length","numel",
            "sin","cos","tan","asin","acos","atan","atan2",
            "exp","log","log2","log10","sqrt","abs","sign",
            "floor","ceil","round","mod","rem","pow",
            "min","max","sum","prod","mean","cumsum","sort",
            "real","imag","conj","deg2rad","rad2deg",
            "upper","lower","strcmp","strcmpi","strcat","strsplit",
            "disp","fprintf","sprintf","num2str",
            "clear","clc","who","whos",
            "true","false","pi","inf","nan","eps",
            "isempty","isnumeric","ischar",
            "plot","bar","scatter","hist","figure","subplot",
            "title","xlabel","ylabel","zlabel","legend",
            "grid","hold","axis","view","close","help",
            nullptr
        };
        std::string result;
        for (int i = 0; keywords[i]; ++i) {
            const char* kw = keywords[i];
            if (std::string(kw).substr(0, partial.size()) == partial) {
                if (!result.empty()) result += ',';
                result += kw;
            }
        }
        return result;
    }

    void restoreOutputFunc() {
        engine_->setOutputFunc([this](const std::string &s) { outputBuf_ += s; });
    }

    /* ---- Display-grid tile fetcher with binary transit ----
     *
     * Returns a Uint8Array VIEW into a thread-local engine-side buffer
     * containing display-pixel-grid uint8 indices. Zero-copy: the JS side
     * reads typed-memory directly from the WASM heap. Caller must consume
     * the data before the next call (the buffer is reused).
     *
     * srcR0/srcC0/srcH/srcW are the visible source-rect (fractional OK
     * for log inverse). xLog/yLog enable log10 axis transforms.
     *
     * On error returns null (no buffer allocated).
     */
    emscripten::val getFigureDisplayTile(int figId, int axIdx, int dsIdx,
                                         double srcR0, double srcC0,
                                         double srcH, double srcW,
                                         int displayH, int displayW,
                                         bool xLog, bool yLog) {
        try {
            const auto &fm = engine_->figureManager();
            displayTileBuf_.resize(static_cast<size_t>(displayH) * displayW);
            const bool ok = fm.getFigureDisplayTile(
                figId, axIdx, dsIdx,
                srcR0, srcC0, srcH, srcW,
                displayH, displayW, xLog, yLog,
                displayTileBuf_.data());
            if (!ok) return emscripten::val::null();
            return emscripten::val(emscripten::typed_memory_view(
                displayTileBuf_.size(), displayTileBuf_.data()));
        } catch (...) {
            return emscripten::val::null();
        }
    }

    /* ---- Source-grid tile-fetcher (kept for legacy callers) ----
     *
     * Reads a sub-rectangle (r0..r0+h, c0..c0+w) from the figure's zQuantized
     * (uint8 indices), mean-pooled by lod×lod, returns row-major uint8 JSON.
     */
    std::string getFigureTileJSON(int figId, int axIdx, int dsIdx,
                                  int r0, int c0, int h, int w, int lod) {
        try {
            const auto &fm = engine_->figureManager();
            std::vector<uint8_t> tile;
            size_t outRows = 0, outCols = 0;
            bool ok = fm.getFigureTile(figId, axIdx, dsIdx, r0, c0, h, w, lod,
                                       tile, outRows, outCols);
            if (!ok) {
                return "{\"error\":\"out of range or no zQuantized\"}";
            }

            std::ostringstream os;
            os << "{\"rows\":" << outRows
               << ",\"cols\":" << outCols
               << ",\"data\":[";
            for (size_t i = 0; i < tile.size(); ++i) {
                if (i) os << ",";
                os << static_cast<int>(tile[i]);
            }
            os << "]}";
            return os.str();
        } catch (const std::exception &e) {
            return std::string("{\"error\":\"") + e.what() + "\"}";
        }
    }

    // Decimated viewport tile for a downsampled line series (Phase 2c). The
    // full x/y live in the engine's figure manager; this returns only the
    // ~4*width points visible in [x0, x1] so zoom reveals detail without
    // ever shipping the full array. algo: 0=M4, 1=LTTB, 2=none.
    //   { x:[...], y:[...] } | { error }
    std::string getSeriesTileJSON(int figId, int axIdx, int dsIdx,
                                  double x0, double x1, int width, int algo) {
        try {
            const auto &fm = engine_->figureManager();
            numkit::DecimatedSeries s =
                fm.getSeriesDisplayTile(figId, axIdx, dsIdx, x0, x1, width, algo);
            std::ostringstream os;
            os << "{\"x\":" << numkit::figdetail::serializeFlatDoubles(s.x)
               << ",\"y\":" << numkit::figdetail::serializeFlatDoubles(s.y) << "}";
            return os.str();
        } catch (const std::exception &e) {
            return std::string("{\"error\":\"") + escapeJSON(e.what()) + "\"}";
        } catch (...) { return "{\"error\":\"unknown\"}"; }
    }

private:
    std::unique_ptr<numkit::Engine> engine_;
    std::string outputBuf_;
    std::unique_ptr<numkit::DebugSession> debugSession_;
    std::vector<uint16_t> breakpointLines_;
    // Reused buffer for getFigureDisplayTile — JS gets a typed_memory_view
    // straight into here, so it must persist until the next call.
    std::vector<uint8_t> displayTileBuf_;
    std::map<std::string, emscripten::val> fsHandlers_;

    void installFs(const std::string &name, emscripten::val handler) {
        auto readFn = [handler](const std::string &p) -> std::string {
            return handler.call<std::string>("readFile", p);
        };
        auto writeFn = [handler](const std::string &p, const std::string &c) {
            handler.call<void>("writeFile", p, c);
        };
        auto existsFn = [handler](const std::string &p) -> bool {
            return handler.call<bool>("exists", p);
        };
        // Binary-safe read: JS returns a Uint8Array; copy its bytes
        // straight into the std::string's buffer — no std::string/UTF-8
        // round-trip (which would mangle bytes ≥ 0x80). Falls back to the
        // text readFile if the handler predates the binary hook.
        auto readBytesFn = [handler](const std::string &p) -> std::string {
            if (handler["readFileBytes"].isUndefined() || handler["readFileBytes"].isNull())
                return handler.call<std::string>("readFile", p);
            emscripten::val arr = handler.call<emscripten::val>("readFileBytes", p);
            const std::size_t len = arr["length"].as<std::size_t>();
            std::string out(len, '\0');
            if (len) {
                // Wrap the std::string buffer as a JS Uint8Array view over the
                // WASM heap, then copy the source array into it. Using
                // typed_memory_view avoids depending on Module.HEAPU8 being
                // exported (it isn't, in this build).
                emscripten::val view = emscripten::val(emscripten::typed_memory_view(
                    len, reinterpret_cast<unsigned char *>(out.data())));
                view.call<void>("set", arr);
            }
            return out;
        };
        // Binary-safe write: hand the bytes to JS as a Uint8Array. We copy
        // the std::string buffer into a FRESH JS array (not a heap view) so
        // the handler can retain it past this call without dangling on WASM
        // memory. Falls back to text writeFile if the handler predates it.
        auto writeBytesFn = [handler](const std::string &p, const std::string &bytes) {
            if (handler["writeFileBytes"].isUndefined() || handler["writeFileBytes"].isNull()) {
                handler.call<void>("writeFile", p, bytes);
                return;
            }
            const std::size_t len = bytes.size();
            emscripten::val u8 = emscripten::val::global("Uint8Array").new_(len);
            if (len) {
                emscripten::val view = emscripten::val(emscripten::typed_memory_view(
                    len, reinterpret_cast<const unsigned char *>(bytes.data())));
                u8.call<void>("set", view);
            }
            handler.call<void>("writeFileBytes", p, u8);
        };
        auto cfs = std::make_unique<numkit::CallbackFS>(name, readFn, writeFn, existsFn);
        cfs->setReadBytes(readBytesFn);
        cfs->setWriteBytes(writeBytesFn);
        engine_->registerVirtualFS(std::move(cfs));
    }

    std::string buildDebugResult(numkit::ExecStatus status) {
        std::string output = debugSession_ ? debugSession_->takeOutput() : "";
        std::string result;

        if (status == numkit::ExecStatus::Paused) {
            auto snap = debugSession_->snapshot();

            // Determine pause reason: breakpoint or step
            bool onBreakpoint = engine_->breakpointManager().shouldBreak(snap.line);
            const char *reason = onBreakpoint ? "breakpoint" : "step";

            result = "{\"status\":\"paused\",\"pauseState\":{";
            result += "\"line\":" + std::to_string(snap.line);
            result += ",\"col\":" + std::to_string(snap.col);
            result += ",\"function\":\"" + escapeJSON(snap.functionName) + "\"";
            result += ",\"reason\":\"" + std::string(reason) + "\"";

            // Variables — structured format matching workspaceJSON
            result += ",\"variables\":{";
            bool first = true;
            for (auto &v : snap.variables) {
                if (!v.value) continue;
                if (v.name == "nargin" || v.name == "nargout") continue;
                if (!first) result += ",";
                auto &val = *v.value;
                result += "\"" + escapeJSON(v.name) + "\":{";
                result += "\"type\":\"" + std::string(numkit::mtypeName(val.type())) + "\"";
                auto &d = val.dims();
                result += ",\"size\":\"" + std::to_string(d.rows()) + "x" + std::to_string(d.cols());
                if (d.is3D()) result += "x" + std::to_string(d.pages());
                result += "\"";
                result += ",\"preview\":";
                if (val.type() == numkit::ValueType::DOUBLE && val.isScalar()) {
                    double dv = val.toScalar();
                    if (std::isnan(dv)) result += "\"NaN\"";
                    else if (std::isinf(dv)) result += (dv > 0 ? "\"Inf\"" : "\"-Inf\"");
                    else result += std::to_string(dv);
                } else if (val.type() == numkit::ValueType::LOGICAL && val.isScalar()) {
                    result += (val.toBool() ? "true" : "false");
                } else if (val.type() == numkit::ValueType::CHAR) {
                    result += "\"" + escapeJSON(val.toString()) + "\"";
                } else {
                    result += "\"" + escapeJSON(valuePreview(val)) + "\"";
                }
                result += "}";
                first = false;
            }
            result += "}";

            // Call stack
            result += ",\"callStack\":[";
            for (size_t i = 0; i < snap.callStack.size(); ++i) {
                if (i > 0) result += ",";
                auto &sf = snap.callStack[i];
                result += "{\"function\":\"" + escapeJSON(sf.functionName) + "\"";
                result += ",\"line\":" + std::to_string(sf.line) + "}";
            }
            result += "]";

            result += "}";
            if (!output.empty())
                result += ",\"output\":\"" + escapeJSON(output) + "\"";
            result += "}";
        } else {
            // Completed or error
            if (debugSession_ && !debugSession_->errorMessage().empty()) {
                result = "{\"status\":\"error\",\"message\":\"" +
                         escapeJSON(debugSession_->errorMessage()) + "\"";
                if (debugSession_->errorLine() > 0)
                    result += ",\"line\":" + std::to_string(debugSession_->errorLine());
                if (!output.empty())
                    result += ",\"output\":\"" + escapeJSON(output) + "\"";
                result += "}";
            } else {
                result = "{\"status\":\"completed\"";
                if (!output.empty())
                    result += ",\"output\":\"" + escapeJSON(output) + "\"";
                result += "}";
            }
            debugSession_.reset();
            restoreOutputFunc();
        }

        return result;
    }
};

static std::unique_ptr<ReplSession> g_session;

std::string repl_init() {
    // Idempotent: construct the session on first call, otherwise just
    // return the greeting. The IDE calls this once at startup before
    // registering VFS adapters; any lazy `if (!g_session) repl_init()`
    // fallbacks below also rely on this no-op-on-reinit semantics to
    // avoid dropping adapters that were registered earlier in the session.
    //
    // Exception routing: the ReplSession constructor calls every
    // library's install() (graphics, signal, builtin, …), and any of
    // them might throw on duplicate function registration or other
    // setup errors. Emscripten's binding bridge does NOT preserve
    // std::exception::what() across the C++→JS boundary — the JS
    // caller sees `err.message` as undefined and the renderer drops
    // into fallback mode silently. We catch here and log to stderr
    // (Emscripten routes this to printErr → console.warn) BEFORE
    // rethrowing, so the actual reason lands in DevTools console.
    if (!g_session) {
        try {
            g_session = std::make_unique<ReplSession>();
        } catch (const std::exception &e) {
            std::cerr << "[repl_init] FATAL: " << e.what() << std::endl;
            throw;
        } catch (...) {
            std::cerr << "[repl_init] FATAL: unknown C++ exception" << std::endl;
            throw;
        }
    }
    return "numkit MInterpreter v2.2\nType commands below.";
}

std::string repl_execute(const std::string& input) {
    if (!g_session) repl_init();
    size_t start = input.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = input.find_last_not_of(" \t\n\r");
    std::string trimmed = input.substr(start, end - start + 1);
    if (trimmed.empty()) return "";
    if (trimmed == "clc") return "__CLEAR__";
    if (trimmed == "help") {
        return "Commands: clc, clear, who, whos, help\n"
               "Keys: Enter=exec, Shift+Enter=newline, Tab=autocomplete";
    }
    return g_session->execute(trimmed);
}

std::string repl_complete(const std::string& partial) {
    if (!g_session) return "";
    return g_session->complete(partial);
}

std::string repl_reset() {
    if (g_session) g_session->reset();
    return "Workspace cleared.";
}

std::string repl_workspace() {
    if (!g_session) return "No active session.";
    return g_session->getWorkspace();
}

std::string repl_get_vars() {
    if (!g_session) return "{}";
    return "__VARS__:" + g_session->getWorkspaceJSON();
}

std::string repl_get_var_data(const std::string &name) {
    if (!g_session) return "{\"error\":\"no session\"}";
    return g_session->getVarFullJSON(name);
}

std::string repl_inspect_path(const std::string &name, const std::string &pathStr) {
    if (!g_session) return "{\"error\":\"no session\"}";
    return g_session->getInspectPathJSON(name, pathStr);
}

std::string repl_get_var_shape(const std::string &name) {
    if (!g_session) return "{\"error\":\"no session\"}";
    return g_session->getVarShapeJSON(name);
}

std::string repl_get_var_tile(const std::string &name, int r0, int c0, int rows, int cols, int page) {
    if (!g_session) return "{\"error\":\"no session\"}";
    return g_session->getVarTileJSON(name, r0, c0, rows, cols, page);
}

std::string repl_get_var_stats(const std::string &name, int page) {
    if (!g_session) return "{\"error\":\"no session\"}";
    return g_session->getVarStatsJSON(name, page);
}

// Full data for a single 2-D slice (page) of a 3-D / N-D array. page 0 is
// the first slice, identical to repl_get_var_data for a 2-D value.
std::string repl_get_var_page(const std::string &name, int page) {
    if (!g_session) return "{\"error\":\"no session\"}";
    return g_session->getVarFullJSON(name, page);
}

std::string repl_get_figure_tile(int figId, int axIdx, int dsIdx,
                                 int r0, int c0, int h, int w, int lod) {
    if (!g_session) return "{\"error\":\"no session\"}";
    return g_session->getFigureTileJSON(figId, axIdx, dsIdx, r0, c0, h, w, lod);
}

std::string repl_get_series_tile(int figId, int axIdx, int dsIdx,
                                 double x0, double x1, int width, int algo) {
    if (!g_session) return "{\"error\":\"no session\"}";
    return g_session->getSeriesTileJSON(figId, axIdx, dsIdx, x0, x1, width, algo);
}

emscripten::val repl_get_figure_display_tile(int figId, int axIdx, int dsIdx,
                                             double srcR0, double srcC0,
                                             double srcH, double srcW,
                                             int displayH, int displayW,
                                             bool xLog, bool yLog) {
    if (!g_session) return emscripten::val::null();
    return g_session->getFigureDisplayTile(figId, axIdx, dsIdx,
                                           srcR0, srcC0, srcH, srcW,
                                           displayH, displayW, xLog, yLog);
}

std::string repl_version() {
    if (!g_session) repl_init();
    return g_session->version();
}

// ── Debug API (clean — no replay) ──

void repl_debug_set_breakpoints(const std::string &linesJson) {
    if (!g_session) repl_init();
    g_session->setBreakpoints(linesJson);
}

std::string repl_debug_start(const std::string &code) {
    if (!g_session) repl_init();
    return g_session->debugStart(code);
}

std::string repl_debug_resume(int action) {
    if (!g_session) return "{\"status\":\"completed\"}";
    return g_session->debugResume(action);
}

void repl_debug_stop() {
    if (g_session) g_session->debugStop();
}

// Legacy API (kept for backward compat, delegates to new API)
std::string repl_debug_execute(const std::string &code, int skipBp) {
    if (!g_session) repl_init();
    if (skipBp == 0) {
        // Fresh start
        return g_session->debugStart(code);
    } else {
        // Continue (legacy: skipBp > 0 means "continue from last pause")
        return g_session->debugResume(0); // 0 = Continue
    }
}

// ── Virtual filesystem bridge ──
//
// JS-side usage:
//   Module.repl_register_fs('temporary', {
//       readFile: (path) => /* sync-accessible mirror of tempFS */,
//       writeFile: (path, content) => { /* write-through */ },
//       exists: (path) => /* bool */,
//   });
//   Module.repl_push_script_origin('temporary');  // before running a script
//   // ... run script via repl_execute ...
//   Module.repl_pop_script_origin();
//
// Callbacks are called synchronously from C++; the JS adapter must
// serve them without awaiting promises (mirror tempFS/localFS into a
// sync-accessible Map, or build with Asyncify).

void repl_register_fs(const std::string &name, emscripten::val handler) {
    if (!g_session) repl_init();
    g_session->registerFs(name, handler);
}

void repl_push_script_origin(const std::string &fsName) {
    if (!g_session) repl_init();
    g_session->pushScriptOrigin(fsName);
}

void repl_push_script_origin_with_dir(const std::string &fsName,
                                       const std::string &scriptDir) {
    if (!g_session) repl_init();
    g_session->pushScriptOriginWithDir(fsName, scriptDir);
}

void repl_pop_script_origin() {
    if (!g_session) return;
    g_session->popScriptOrigin();
}

// ════════════════════════════════════════════════════════════════
// Script-graph visualizer — offline AST → NodeGraph IR → JSON.
// No engine session needed; pure analysis pass over the parsed AST.
// Errors (parse failures) are returned as {"error":"..."} so the
// IDE can surface them without throwing across the WASM boundary.
// ════════════════════════════════════════════════════════════════
static std::string jsonError(const std::string &message) {
    // escapeJSON returns escaped-but-unquoted content (see line 71);
    // wrap in quotes ourselves so the result is a valid JSON value.
    return std::string("{\"error\":\"") + escapeJSON(message) + "\"}";
}

std::string buildScriptGraph(const std::string &source) {
    try {
        numkit::Lexer lex(source);
        auto tokens = lex.tokenize();
        numkit::Parser parser(tokens);
        auto root = parser.parse();
        if (!root) return jsonError("parser returned null AST");
        // Pass `tokens` so lowering can use the lexer's COMMENT
        // positions to trim trailing `% ...` from per-node sourceText.
        // No second lex; the parser already filtered COMMENT tokens
        // internally via its centralized advance() helper.
        auto g = numkit::graph::lowerScript(*root, source, tokens);
        return numkit::graph::toJSON(g);
    } catch (const std::exception &e) {
        return jsonError(e.what());
    } catch (...) {
        return jsonError("unknown exception during graph build");
    }
}

/** Literal parse-tree dump for the IDE's AST inspector view. Same
 *  lex+parse pipeline as buildScriptGraph but emits the raw AST
 *  instead of the lowered NodeGraph IR. */
std::string buildAST(const std::string &source) {
    try {
        numkit::Lexer lex(source);
        auto tokens = lex.tokenize();
        numkit::Parser parser(tokens);
        auto root = parser.parse();
        if (!root) return jsonError("parser returned null AST");
        return numkit::graph::toASTJSON(*root);
    } catch (const std::exception &e) {
        return jsonError(e.what());
    } catch (...) {
        return jsonError("unknown exception during AST build");
    }
}

EMSCRIPTEN_BINDINGS(numkit_ide) {
    emscripten::function("repl_init",      &repl_init);
    emscripten::function("repl_execute",   &repl_execute);
    emscripten::function("repl_complete",  &repl_complete);
    emscripten::function("repl_reset",     &repl_reset);
    emscripten::function("repl_workspace", &repl_workspace);
    emscripten::function("repl_get_vars",     &repl_get_vars);
    emscripten::function("repl_get_var_data",  &repl_get_var_data);
    emscripten::function("repl_get_var_page",  &repl_get_var_page);
    emscripten::function("repl_inspect_path", &repl_inspect_path);
    emscripten::function("repl_get_var_shape", &repl_get_var_shape);
    emscripten::function("repl_get_var_tile",  &repl_get_var_tile);
    emscripten::function("repl_get_var_stats", &repl_get_var_stats);
    emscripten::function("repl_get_figure_tile", &repl_get_figure_tile);
    emscripten::function("repl_get_series_tile", &repl_get_series_tile);
    emscripten::function("repl_get_figure_display_tile", &repl_get_figure_display_tile);
    emscripten::function("repl_version",   &repl_version);
    emscripten::function("repl_debug_set_breakpoints", &repl_debug_set_breakpoints);
    emscripten::function("repl_debug_start",           &repl_debug_start);
    emscripten::function("repl_debug_resume",          &repl_debug_resume);
    emscripten::function("repl_debug_stop",            &repl_debug_stop);
    // Virtual filesystem bridge
    emscripten::function("repl_register_fs",           &repl_register_fs);
    emscripten::function("repl_push_script_origin",    &repl_push_script_origin);
    emscripten::function("repl_push_script_origin_with_dir",
                                                       &repl_push_script_origin_with_dir);
    emscripten::function("repl_pop_script_origin",     &repl_pop_script_origin);
    // Script-graph visualizer — pure analysis pass, no engine state.
    emscripten::function("buildScriptGraph",           &buildScriptGraph);
    emscripten::function("buildAST",                   &buildAST);
    // Legacy (kept for backward compat)
    emscripten::function("repl_debug_execute",         &repl_debug_execute);
}
