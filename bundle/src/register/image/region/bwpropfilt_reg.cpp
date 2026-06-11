// toolboxes/image/src/region/bwpropfilt_reg.cpp
//
// Register half of the image bwpropfilt builtins: the CallContext wrappers
// delegating to the engine-free compute in bwpropfilt.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/region/region.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::image {

namespace detail {

void bwpropfilt_reg(Span<const Value> args, std::size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    // MATLAB signatures:
    //   bwpropfilt(BW, attrib, range)
    //   bwpropfilt(BW, attrib, n)
    //   bwpropfilt(BW, attrib, n, keep)
    //   bwpropfilt(BW, I, attrib, ...)
    //   bwpropfilt(CC, attrib, ...)
    //   bwpropfilt(CC, I, attrib, ...)
    //   bwpropfilt(..., conn)
    if (args.size() < 3)
        throw Error("bwpropfilt: requires (IN, attrib, range_or_n)",
                    0, 0, "bwpropfilt", "", "numkit:bwpropfilt:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    const Value &in1 = args[0];
    const bool first_is_struct = in1.isStruct();

    std::size_t i = 1;
    Value marker;
    // Optional intensity image: arg 2 is numeric (not a string).
    if (!is_string(args[i])) {
        marker = args[i];
        ++i;
    }
    // attrib.
    if (i >= args.size() || !is_string(args[i]))
        throw Error("bwpropfilt: expected attribute string",
                    0, 0, "bwpropfilt", "", "numkit:bwpropfilt:noAttr");
    const std::string attrib = args[i].toString();
    ++i;
    // range or n.
    if (i >= args.size())
        throw Error("bwpropfilt: requires range or n",
                    0, 0, "bwpropfilt", "", "numkit:bwpropfilt:noRange");
    const Value &p = args[i];
    ++i;
    double p_min = 0, p_max = 0;
    std::size_t keep_n = 0;
    bool keep_largest = true;
    if (p.numel() == 1) {
        keep_n = static_cast<std::size_t>(p.toScalar());
        if (keep_n == 0)
            throw Error("bwpropfilt: n must be a positive integer",
                        0, 0, "bwpropfilt", "", "numkit:bwpropfilt:nZero");
    } else if (p.numel() == 2) {
        p_min = p.elemAsDouble(0);
        p_max = p.elemAsDouble(1);
        if (p_min > p_max)
            throw Error("bwpropfilt: range must be nondecreasing",
                        0, 0, "bwpropfilt", "",
                        "numkit:bwpropfilt:rangeOrder");
    } else {
        throw Error("bwpropfilt: p must be scalar n or 2-element range",
                    0, 0, "bwpropfilt", "", "numkit:bwpropfilt:pShape");
    }
    int conn = 8;
    // Optional 'keep' direction (only with scalar n).
    if (i < args.size() && is_string(args[i])) {
        std::string dir = args[i].toString();
        std::string dlo;
        for (char ch : dir)
            dlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (dlo == "largest")        keep_largest = true;
        else if (dlo == "smallest")  keep_largest = false;
        else throw Error("bwpropfilt: keep direction must be "
                         "'largest' or 'smallest'",
                         0, 0, "bwpropfilt", "",
                         "numkit:bwpropfilt:dirBad");
        ++i;
    }
    // Optional connectivity arg (numeric scalar).
    if (i < args.size() && !is_string(args[i])) {
        conn = static_cast<int>(args[i].toScalar());
        if (conn != 4 && conn != 8)
            throw Error("bwpropfilt: connectivity must be 4 or 8",
                        0, 0, "bwpropfilt", "",
                        "numkit:bwpropfilt:connBad");
        if (first_is_struct)
            throw Error("bwpropfilt: connectivity not allowed with CC input",
                        0, 0, "bwpropfilt", "",
                        "numkit:bwpropfilt:connWithCC");
        ++i;
    }

    if (first_is_struct) {
        outs[0] = bwpropfilt(Value::Empty, in1, marker, attrib,
                             p_min, p_max, keep_n, keep_largest, conn, mr);
    } else {
        outs[0] = bwpropfilt(in1, Value::Empty, marker, attrib,
                             p_min, p_max, keep_n, keep_largest, conn, mr);
    }
}

}  // namespace detail
}  // namespace numkit::image
