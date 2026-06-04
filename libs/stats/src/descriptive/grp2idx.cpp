// libs/stats/src/descriptive/grp2idx.cpp
//
// MATLAB grp2idx(S): turn a grouping variable into a 1-based index vector.
//
//   [G, GN, GL] = grp2idx(S)
//
//   G  = column vector of group indices, one per element of S.
//   GN = column cell of the group names (as char rows).
//   GL = group levels — for the non-categorical inputs handled here, GL == GN.
//
// Ordering rules (matching MATLAB R2025b):
//   - Numeric / logical S: groups are the sorted-ascending unique values;
//     each group name is num2str of the value. NaN maps to a NaN index and is
//     excluded from the group set.
//   - cellstr / string S: groups are the unique strings in FIRST-APPEARANCE
//     order; the name is the string itself.
//   - A single char row vector is treated as one group label.
//
// (categorical input and column-aligned char matrices are not yet handled.)

#include <numkit/stats/descriptive/descriptive.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace numkit::stats {

namespace {

// Format a numeric group value the way MATLAB's grp2idx labels it: integers
// print with no decimals, other values with ~5 significant digits.
std::string fmtGroupNum(double v)
{
    char buf[64];
    if (std::isfinite(v) && std::floor(v) == v && std::fabs(v) < 1e15)
        std::snprintf(buf, sizeof(buf), "%.0f", v);
    else
        std::snprintf(buf, sizeof(buf), "%.5g", v);
    return std::string(buf);
}

} // namespace

// ── Public C++ API (see descriptive.hpp) ──────────────────────────────

Grp2idxResult grp2idx(const Value &s, std::pmr::memory_resource *mr)
{
    const size_t n = s.numel();

    std::vector<double>      g;        // index per element (NaN allowed)
    std::vector<std::string> names;    // group names in group order

    if (s.isCell() || (s.isString() && n != 1)) {
        // cellstr / multi-element string array: first-appearance order.
        const auto &vec = s.cellDataVec();
        g.reserve(vec.size());
        for (const auto &e : vec) {
            const std::string str = e.toString();
            int idx = -1;
            for (size_t k = 0; k < names.size(); ++k)
                if (names[k] == str) { idx = static_cast<int>(k); break; }
            if (idx < 0) { names.push_back(str); idx = static_cast<int>(names.size()) - 1; }
            g.push_back(static_cast<double>(idx + 1));
        }
    } else if (s.isChar() || s.isString()) {
        // A single char row / string scalar is one group label.
        names.push_back(s.toString());
        g.push_back(1.0);
    } else {
        // Numeric / logical: sorted-ascending unique values; NaN -> NaN index.
        std::vector<double> uniq;
        uniq.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            const double xi = s.elemAsDouble(i);
            if (!std::isnan(xi)) uniq.push_back(xi);
        }
        std::sort(uniq.begin(), uniq.end());
        uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
        names.reserve(uniq.size());
        for (double u : uniq) names.push_back(fmtGroupNum(u));
        g.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            const double xi = s.elemAsDouble(i);
            if (std::isnan(xi)) {
                g.push_back(std::numeric_limits<double>::quiet_NaN());
                continue;
            }
            const auto it = std::lower_bound(uniq.begin(), uniq.end(), xi);
            g.push_back(static_cast<double>((it - uniq.begin()) + 1));
        }
    }

    Grp2idxResult r;
    // G: column vector.
    r.G = Value::matrix(g.size(), 1, ValueType::DOUBLE, mr);
    std::copy(g.begin(), g.end(), r.G.doubleDataMut());
    // GN / GL: column cell of char rows (GL == GN for these input types).
    r.GN = Value::cell(names.size(), 1, mr);
    r.GL = Value::cell(names.size(), 1, mr);
    for (size_t k = 0; k < names.size(); ++k) {
        r.GN.cellAt(k) = Value::fromString(names[k], mr);
        r.GL.cellAt(k) = Value::fromString(names[k], mr);
    }
    return r;
}

namespace detail {

void grp2idx_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.empty())
        throw Error("grp2idx: requires (s)",
                    0, 0, "grp2idx", "", "numkit:grp2idx:nargin");
    Grp2idxResult r = grp2idx(args[0], ctx.engine->resource());
    outs[0] = std::move(r.G);
    if (nargout >= 2) outs[1] = std::move(r.GN);
    if (nargout >= 3) outs[2] = std::move(r.GL);
}

} // namespace detail
} // namespace numkit::stats
