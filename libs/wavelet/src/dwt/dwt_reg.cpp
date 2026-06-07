// libs/wavelet/src/dwt/dwt_reg.cpp
//
// Register half of the single-level DWT pair: the CallContext builtins
// dwt / idwt (argument parsing, custom-filter vs wname dispatch, optional
// `len` and 'mode' N-V handling) that delegate to the engine-free compute
// in dwt.cpp. This is the only DWT TU that needs the engine; keeping it
// separate lets the compute build against value+fs+ops alone.
// library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/wavelet/dwt/dwt.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <cctype>
#include <string>
#include <tuple>
#include <vector>

namespace numkit::wavelet {
namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("dwt/idwt: expected a string for wavelet name",
                    0, 0, "", "", "numkit:wavelet:type");
    return v.toString();
}

// Read a Value (numeric vector) into a flat double buffer.
static std::vector<double> readVec(const Value &v) {
    const size_t n = v.numel();
    std::vector<double> out(n);
    for (size_t i = 0; i < n; ++i) out[i] = v.elemAsDouble(i);
    return out;
}

// Walk trailing N-V pairs starting at args[start]; returns true if a
// 'mode' arg specifies anything other than 'sym'. Throws on unsupported
// boundary modes (only 'sym' implemented for now).
static bool parse_mode_nv(Span<const Value> args, size_t start,
                          const char *fn) {
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    for (size_t i = start; i + 1 < args.size(); i += 2) {
        if (!(args[i].isChar() || args[i].isString())) break;
        const std::string key = lower(args[i].toString());
        if (key == "mode") {
            const std::string m = lower(args[i + 1].toString());
            if (m != "sym" && m != "symh") {
                throw Error(std::string(fn) + ": only 'mode'='sym' is "
                            "implemented (got '" + m + "')",
                            0, 0, fn, "", "numkit:wavelet:mode_nyi");
            }
        }
    }
    return false;
}

void dwt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dwt: requires (x, wname) or (x, Lo_D, Hi_D)",
                    0, 0, "dwt", "", "numkit:dwt:nargin");
    auto *mr = ctx.engine->resource();
    Value cA, cD;
    if (args[1].isChar() || args[1].isString()) {
        // dwt(x, wname[, 'mode', extmode])
        parse_mode_nv(args, 2, "dwt");
        std::tie(cA, cD) = dwt(args[0], args[1].toString(), mr);
    } else {
        // dwt(x, Lo_D, Hi_D[, 'mode', extmode])
        if (args.size() < 3 || (args[2].isChar() || args[2].isString()))
            throw Error("dwt: custom-filter form requires (x, Lo_D, Hi_D)",
                        0, 0, "dwt", "", "numkit:dwt:nargin");
        parse_mode_nv(args, 3, "dwt");
        std::tie(cA, cD) = dwt_with_filters_pub(args[0],
                                                readVec(args[1]), readVec(args[2]),
                                                mr);
    }
    if (outs.size() >= 1) outs[0] = cA;
    if (outs.size() >= 2) outs[1] = cD;
}

void idwt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("idwt: requires (cA, cD, wname[, len]) or "
                    "(cA, cD, Lo_R, Hi_R[, len])",
                    0, 0, "idwt", "", "numkit:idwt:nargin");
    auto *mr = ctx.engine->resource();
    long long len = -1;

    if (args[2].isChar() || args[2].isString()) {
        // idwt(cA, cD, wname[, len][, 'mode', extmode])
        const std::string wname = args[2].toString();
        size_t i = 3;
        // Optional positional `len` (numeric scalar) BEFORE any N-V.
        if (i < args.size() && args[i].numel() == 1
            && !(args[i].isChar() || args[i].isString())) {
            len = (long long)args[i].toScalar();
            ++i;
        }
        parse_mode_nv(args, i, "idwt");
        outs[0] = idwt(args[0], args[1], wname, len, mr);
    } else {
        // idwt(cA, cD, Lo_R, Hi_R[, len][, 'mode', extmode])
        if (args.size() < 4 || (args[3].isChar() || args[3].isString()))
            throw Error("idwt: custom-filter form requires "
                        "(cA, cD, Lo_R, Hi_R)",
                        0, 0, "idwt", "", "numkit:idwt:nargin");
        size_t i = 4;
        if (i < args.size() && args[i].numel() == 1
            && !(args[i].isChar() || args[i].isString())) {
            len = (long long)args[i].toScalar();
            ++i;
        }
        parse_mode_nv(args, i, "idwt");
        outs[0] = idwt_with_filters_pub(args[0], args[1],
                                        readVec(args[2]), readVec(args[3]),
                                        len, mr);
    }
}

} // namespace detail
} // namespace numkit::wavelet
