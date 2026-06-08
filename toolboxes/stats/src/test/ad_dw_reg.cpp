// toolboxes/signal/src/test/ad_dw_reg.cpp
//
// CallContext register half of test/ad_dw.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/linalg/eig.hpp>   // eig_symmetric — exact DW p-value (Imhof)
#include <numkit/stats/distributions/beta.hpp>
#include <numkit/stats/distributions/normal.hpp>
#include <numkit/stats/test/hypothesis.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "ad_dw_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace detail {

void adtest_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("adtest: requires (x [, alpha])",
                    0, 0, "adtest", "", "numkit:adtest:nargin");
    double alpha = 0.05;
    if (args.size() >= 2 && !args[1].isEmpty())
        alpha = args[1].toScalar();
    auto [h, p, stat, cv] = adtest(args[0], alpha, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(stat);
    if (nargout > 3) outs[3] = std::move(cv);
}

void dwtest_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dwtest: requires (residuals, design)",
                    0, 0, "dwtest", "", "numkit:dwtest:nargin");
    auto *mr = ctx.engine->resource();

    auto toLowerAscii = [](std::string s) {
        for (char &c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        return s;
    };

    // Optional name/value options: 'Tail' (both|right|left, default both) and
    // 'Method' (exact|approximate, default exact — matches MATLAB).
    int  tail  = 0;        // 0=both, 1=right (positive ac), 2=left (negative ac)
    bool exact = true;
    for (size_t i = 2; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("dwtest: expected option name (string)",
                        0, 0, "dwtest", "", "numkit:dwtest:badOption");
        const std::string key = toLowerAscii(args[i].toString());
        const std::string val = toLowerAscii(args[i + 1].toString());
        if (key == "tail") {
            if (val == "both")       tail = 0;
            else if (val == "right") tail = 1;
            else if (val == "left")  tail = 2;
            else throw Error("dwtest: Tail must be 'both', 'right' or 'left'",
                             0, 0, "dwtest", "", "numkit:dwtest:badOption");
        } else if (key == "method") {
            if (val == "exact")            exact = true;
            else if (val == "approximate") exact = false;
            else throw Error("dwtest: Method must be 'exact' or 'approximate'",
                             0, 0, "dwtest", "", "numkit:dwtest:badOption");
        } else {
            throw Error("dwtest: unsupported option '" + key + "'",
                        0, 0, "dwtest", "", "numkit:dwtest:badOption");
        }
    }
    if ((args.size() - 2) % 2 != 0)
        throw Error("dwtest: option name without value",
                    0, 0, "dwtest", "", "numkit:dwtest:badOption");

    double dw = 0.0;
    const double pLeft = dwStatAndPLeft(args[0], args[1], exact, dw, mr);
    double p;
    if (tail == 1)      p = pLeft;                  // 'right': positive autocorrelation
    else if (tail == 2) p = 1.0 - pLeft;            // 'left' : negative autocorrelation
    else                p = std::min(1.0, 2.0 * std::min(pLeft, 1.0 - pLeft)); // 'both'
    outs[0] = Value::scalar(p, mr);
    if (nargout > 1) outs[1] = Value::scalar(dw, mr);
}

} // namespace detail

} // namespace numkit::stats
