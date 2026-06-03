// libs/comm/src/coding/convcoding.cpp
//
// Convolutional coding (Error Correction Codes): poly2trellis.
//
// MATLAB R2025b semantics (verified via probe):
//   trellis = poly2trellis(K, [g1 g2 ... gn])  — rate 1/n feed-forward.
//   poly2trellis(3, [6 7]) ->
//     numInputSymbols=2, numOutputSymbols=4, numStates=4,
//     nextStates=[0 2;0 2;1 3;1 3], outputs=[0 3;1 2;3 0;2 1].

#include <numkit/comm/coding/convcoding.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace numkit::comm {

namespace {

// Interpret a non-negative integer whose decimal digits are an octal
// numeral (MATLAB CodeGenerator convention): 171 -> octal 171 -> 121.
std::uint64_t octalToDecimal(std::uint64_t oct, const char *who)
{
    std::uint64_t result = 0, mult = 1;
    while (oct > 0) {
        const std::uint64_t digit = oct % 10;
        if (digit > 7)
            throw Error(std::string(who) + ": CodeGenerator entries must be "
                        "octal numbers (digits 0-7)",
                        0, 0, who, "", std::string("numkit:") + who + ":octal");
        result += digit * mult;
        mult *= 8;
        oct /= 10;
    }
    return result;
}

inline int bitParity(std::uint64_t x)
{
    int p = 0;
    while (x) { p ^= 1; x &= (x - 1); }
    return p;
}

} // namespace

Value poly2trellis(const Value &constraintLength, const Value &codeGenerator,
                   std::pmr::memory_resource *mr)
{
    if (constraintLength.numel() != 1)
        throw Error("poly2trellis: only a scalar ConstraintLength (rate 1/n) "
                    "is supported in this revision (rate k/n deferred)",
                    0, 0, "poly2trellis", "", "numkit:poly2trellis:rate");
    const long long K = static_cast<long long>(constraintLength.toScalar());
    if (K < 1)
        throw Error("poly2trellis: ConstraintLength must be >= 1",
                    0, 0, "poly2trellis", "", "numkit:poly2trellis:K");

    const std::size_t n = codeGenerator.numel();
    if (n == 0)
        throw Error("poly2trellis: CodeGenerator must be non-empty",
                    0, 0, "poly2trellis", "", "numkit:poly2trellis:gen");

    // Parse octal generators into K-bit binary masks.
    std::vector<std::uint64_t> g(n);
    for (std::size_t i = 0; i < n; ++i)
        g[i] = octalToDecimal(
            static_cast<std::uint64_t>(codeGenerator.elemAsDouble(i)),
            "poly2trellis");

    const std::size_t numStates = static_cast<std::size_t>(1) << (K - 1);
    const std::size_t numInput  = 2;                                   // k = 1
    const std::size_t numOutput = static_cast<std::size_t>(1) << n;

    Value ns = Value::matrix(numStates, numInput, ValueType::DOUBLE, mr);
    Value ou = Value::matrix(numStates, numInput, ValueType::DOUBLE, mr);
    double *nsd = ns.doubleDataMut();
    double *oud = ou.doubleDataMut();

    const std::uint64_t stateMask = numStates - 1;  // 0 when numStates==1
    for (std::size_t s = 0; s < numStates; ++s) {
        for (std::size_t u = 0; u < numInput; ++u) {
            // K-bit register: newest input at the MSB, then the (K-1) state
            // bits below it.
            const std::uint64_t reg =
                (static_cast<std::uint64_t>(u) << (K - 1)) | s;
            const std::uint64_t nextState = (reg >> 1) & stateMask;
            std::uint64_t out = 0;
            for (std::size_t i = 0; i < n; ++i)            // g1 = MSB
                out = (out << 1) | static_cast<std::uint64_t>(bitParity(reg & g[i]));
            // Column-major (numStates × 2): element (s, u) at u*numStates + s.
            nsd[u * numStates + s] = static_cast<double>(nextState);
            oud[u * numStates + s] = static_cast<double>(out);
        }
    }

    Value t = Value::structArray(1, 1, mr);
    auto &el = t.structArrayElem(0);
    el.emplace("numInputSymbols",  Value::scalar(static_cast<double>(numInput),  mr));
    el.emplace("numOutputSymbols", Value::scalar(static_cast<double>(numOutput), mr));
    el.emplace("numStates",        Value::scalar(static_cast<double>(numStates), mr));
    el.emplace("nextStates", std::move(ns));
    el.emplace("outputs",    std::move(ou));
    return t;
}

namespace detail {

void poly2trellis_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("poly2trellis: requires (ConstraintLength, CodeGenerator)",
                    0, 0, "poly2trellis", "", "numkit:poly2trellis:nargin");
    if (args.size() >= 3 && !args[2].isEmpty())
        throw Error("poly2trellis: FeedbackConnection (feedback codes) is not "
                    "supported in this revision",
                    0, 0, "poly2trellis", "", "numkit:poly2trellis:feedback");
    outs[0] = poly2trellis(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::comm
