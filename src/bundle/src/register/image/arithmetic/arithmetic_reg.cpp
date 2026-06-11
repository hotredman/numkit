// toolboxes/image/src/arithmetic/arithmetic_reg.cpp
//
// Register half of the image arithmetic builtins: the CallContext wrappers
// (imadd / imsubtract / immultiply / imdivide / imabsdiff / imcomplement /
// imlincomb / imapplymatrix) that delegate to the engine-free compute in
// arithmetic.cpp. library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/arithmetic/arithmetic.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <cstddef>
#include <vector>

namespace numkit::image {
namespace detail {

namespace {
ValueType classFromString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("imlincomb: output_class must be a string",
                    0, 0, "imlincomb", "", "numkit:imlincomb:badclass");
    auto s = v.toString();
    if (s == "double") return ValueType::DOUBLE;
    if (s == "single") return ValueType::SINGLE;
    if (s == "uint8")  return ValueType::UINT8;
    if (s == "uint16") return ValueType::UINT16;
    if (s == "int16")  return ValueType::INT16;
    if (s == "logical")return ValueType::LOGICAL;
    throw Error("imlincomb: unrecognised output_class", 0, 0, "imlincomb", "",
                "numkit:imlincomb:badclass");
}
} // anonymous

#define NK_BIN_REG(name, fn)                                                      \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                  \
                    Span<Value> outs, CallContext &ctx)                          \
    {                                                                              \
        if (args.size() < 2)                                                       \
            throw Error(#name ": requires (X, Y)",                                \
                         0, 0, #name, "", "numkit:" #name ":nargin");                  \
        outs[0] = fn(args[0], args[1], ctx.engine->resource());                   \
    }

NK_BIN_REG(imadd,      imadd)
NK_BIN_REG(imsubtract, imsubtract)
NK_BIN_REG(immultiply, immultiply)
NK_BIN_REG(imdivide,   imdivide)
NK_BIN_REG(imabsdiff,  imabsdiff)

#undef NK_BIN_REG

void imcomplement_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imcomplement: requires X", 0, 0, "imcomplement", "",
                    "numkit:imcomplement:nargin");
    outs[0] = imcomplement(args[0], ctx.engine->resource());
}

void imlincomb_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imlincomb: requires (k1, A1, k2, A2, ..., [output_class])",
                    0, 0, "imlincomb", "", "numkit:imlincomb:nargin");

    // Trailing string argument (optional) is the output class.
    size_t end = args.size();
    ValueType out_class = ValueType::DOUBLE;
    const Value &last = args[args.size() - 1];
    if ((last.isChar() || last.isString())) {
        out_class = classFromString(last);
        end -= 1;
    } else {
        // Default class = first image's class.
        if (args.size() >= 2) out_class = args[1].type();
    }
    if ((end & 1) == 1 && end < 3) // need at least k1, A1
        throw Error("imlincomb: arguments must come in (coef, image) pairs",
                    0, 0, "imlincomb", "", "numkit:imlincomb:nargin");

    std::vector<double> coefs;
    std::vector<Value>  images;
    for (size_t i = 0; i + 1 < end; i += 2) {
        coefs.push_back(args[i].toScalar());
        images.push_back(args[i + 1]);
    }
    if ((end & 1) == 1) coefs.push_back(args[end - 1].toScalar()); // trailing additive

    outs[0] = imlincomb(coefs, images, out_class, ctx.engine->resource());
}

void imapplymatrix_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imapplymatrix: requires (M, X[, output_class])",
                    0, 0, "imapplymatrix", "", "numkit:imapplymatrix:nargin");
    ValueType out_class = (args.size() >= 3 && (args[2].isChar() || args[2].isString()))
        ? classFromString(args[2])
        : args[1].type();
    outs[0] = imapplymatrix(args[0], args[1], out_class, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::image
