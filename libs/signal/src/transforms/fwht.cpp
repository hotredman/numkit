// libs/signal/src/transforms/fwht.cpp
//
// Fast Walsh-Hadamard transform + inverse.
//
// Algorithm (natural / Hadamard order):
//   1. Pad x to length N = next power of 2 above length(x), or to the
//      explicit n if requested (n must be a power of 2).
//   2. Run the in-place radix-2 Hadamard butterfly:
//        h = 1
//        while h < N:
//          for i in [0, 2h, 4h, ...]:
//            for j in [i, i+h):
//              (x[j], x[j+h]) := (x[j] + x[j+h], x[j] - x[j+h])
//          h *= 2
//   3. For fwht: divide by N (so y(1) = mean(x), matching MATLAB).
//      For ifwht: no division (H · H = N · I → cancels).
//   4. For non-natural orderings, permute:
//        - dyadic:   bit-reversal of indices
//        - sequency: bit-reversal then gray-to-binary (i ↔ g(i)=i XOR i>>1)
//
// References:
//   - Beauchamp, "Applications of Walsh and Related Functions", 1984.
//   - Walsh, "A Closed Set of Normal Orthogonal Functions",
//     Amer. J. Math. 45 (1923) 5-24.

#include <numkit/signal/transforms/fwht.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace numkit::signal {

namespace {

inline bool isPowerOfTwo(std::size_t n) { return n != 0 && (n & (n - 1)) == 0; }

inline std::size_t nextPow2(std::size_t n)
{
    if (n <= 1) return 1;
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

inline int log2_size(std::size_t n)
{
    int k = 0;
    while ((std::size_t(1) << k) < n) ++k;
    return k;
}

// Reverse the low `bits` bits of `i`.
inline std::size_t bitReverse(std::size_t i, int bits)
{
    std::size_t r = 0;
    for (int b = 0; b < bits; ++b) {
        r = (r << 1) | (i & 1);
        i >>= 1;
    }
    return r;
}

// Radix-2 Hadamard butterfly (natural-order) — in-place.
void hadamardButterfly(double *col, std::size_t N)
{
    for (std::size_t h = 1; h < N; h <<= 1) {
        for (std::size_t i = 0; i < N; i += (h << 1)) {
            for (std::size_t j = 0; j < h; ++j) {
                const double a = col[i + j];
                const double b = col[i + j + h];
                col[i + j]     = a + b;
                col[i + j + h] = a - b;
            }
        }
    }
}

// Permute from natural Hadamard order to dyadic (Paley): out[i] = in[bitrev(i)].
void permuteToDyadic(const double *in, double *out, std::size_t N, int log2N)
{
    for (std::size_t i = 0; i < N; ++i)
        out[i] = in[bitReverse(i, log2N)];
}

// Permute from natural Hadamard order to sequency (Walsh).
// Probed against MATLAB: sequency[i] = dyadic[g(i)] where g(i) = i XOR (i>>1).
// Combined transform from natural: sequency[i] = natural[bitrev(g(i))].
void permuteToSequency(const double *in, double *out, std::size_t N, int log2N)
{
    for (std::size_t i = 0; i < N; ++i) {
        const std::size_t g = i ^ (i >> 1);
        out[i] = in[bitReverse(g, log2N)];
    }
}

// Inverse permutations: given the externally-ordered y, undo the
// permutation to get a natural-ordered buffer ready for the inverse
// butterfly. For dyadic the inverse permutation is bit-reverse again
// (bitrev is its own inverse). For sequency the inverse is:
//   natural[bitrev(g(i))] = sequency[i]  →  natural[k] = sequency[i]
//   with k = bitrev(g(i)). The inverse mapping inverts both steps.
void unpermuteFromSequency(const double *in, double *out, std::size_t N, int log2N)
{
    // Forward: out_seq[i] = in_nat[bitrev(g(i))].
    // Inverse: in_nat[k]  = out_seq[i] where bitrev(g(i)) = k.
    // We compute the inverse by iterating i and placing into the natural-
    // order slot bitrev(g(i)).
    for (std::size_t i = 0; i < N; ++i) {
        const std::size_t g = i ^ (i >> 1);
        out[bitReverse(g, log2N)] = in[i];
    }
}

// Resolve user-supplied n: 0 → next power of 2 above length(x). Otherwise
// require power of 2.
std::size_t resolveLen(std::size_t L, std::size_t n)
{
    if (n == 0) return nextPow2(L);
    if (!isPowerOfTwo(n))
        throw Error("fwht: transform length must be a power of 2",
                     0, 0, "fwht", "", "m:fwht:badLength");
    return n;
}

// Apply fwht / ifwht to one column of length L (source `srcCol`), writing
// to `dstCol` (length N where N = resolveLen(L, n)). `forward` toggles
// the 1/N scaling.
void transformOneColumn(const double *srcCol, std::size_t L,
                         double *dstCol, std::size_t N, int log2N,
                         const std::string &ordering, bool forward,
                         std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    ScratchVec<double> buf(N, 0.0, &scratch);

    if (forward) {
        // Forward: copy / truncate / zero-pad input.
        const std::size_t toCopy = std::min(L, N);
        for (std::size_t i = 0; i < toCopy; ++i) buf[i] = srcCol[i];
    } else {
        // Inverse: input is already in `ordering`; undo permutation
        // into natural order before applying the butterfly.
        ScratchVec<double> raw(N, 0.0, &scratch);
        const std::size_t toCopy = std::min(L, N);
        for (std::size_t i = 0; i < toCopy; ++i) raw[i] = srcCol[i];
        if (ordering == "hadamard") {
            std::copy(raw.begin(), raw.end(), buf.begin());
        } else if (ordering == "dyadic") {
            // bit-reverse is self-inverse.
            permuteToDyadic(raw.data(), buf.data(), N, log2N);
        } else { // sequency
            unpermuteFromSequency(raw.data(), buf.data(), N, log2N);
        }
    }

    hadamardButterfly(buf.data(), N);

    if (forward) {
        // Normalise + permute into requested ordering.
        const double inv = 1.0 / static_cast<double>(N);
        for (std::size_t i = 0; i < N; ++i) buf[i] *= inv;
        if (ordering == "hadamard") {
            for (std::size_t i = 0; i < N; ++i) dstCol[i] = buf[i];
        } else if (ordering == "dyadic") {
            permuteToDyadic(buf.data(), dstCol, N, log2N);
        } else { // sequency
            permuteToSequency(buf.data(), dstCol, N, log2N);
        }
    } else {
        // ifwht: H · H = N·I, so the butterfly inverse needs NO 1/N scaling.
        for (std::size_t i = 0; i < N; ++i) dstCol[i] = buf[i];
    }
}

Value runWalshHadamard(const Value &x, std::size_t n,
                       const std::string &ordering, bool forward,
                       std::pmr::memory_resource *mr)
{
    if (ordering != "sequency" && ordering != "hadamard" && ordering != "dyadic")
        throw Error("fwht: ordering must be 'sequency', 'hadamard', or 'dyadic'",
                     0, 0, "fwht", "", "m:fwht:badOrdering");

    const auto &d = x.dims();
    if (d.ndim() > 2)
        throw Error("fwht: input must be a vector or matrix",
                     0, 0, "fwht", "", "m:fwht:notMatrix");
    if (x.numel() == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Treat the input as columns. Row vectors become a single row.
    const std::size_t rows = d.rows();
    const std::size_t cols = d.cols();
    const bool isRowVec = (rows == 1);
    const std::size_t L = isRowVec ? cols : rows;
    const std::size_t nCols = isRowVec ? 1u   : cols;
    const std::size_t N = resolveLen(L, n);
    const int log2N = log2_size(N);

    // Output orientation matches MATLAB: row vector in → row vector out
    // (1 × N), otherwise column-wise transform produces N × cols.
    Value out = isRowVec
        ? Value::matrix(1, N, ValueType::DOUBLE, mr)
        : Value::matrix(N, nCols, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    ScratchArena scratch(mr);
    ScratchVec<double> srcCol(L, 0.0, &scratch);

    for (std::size_t c = 0; c < nCols; ++c) {
        // Gather source column.
        if (isRowVec) {
            for (std::size_t i = 0; i < L; ++i)
                srcCol[i] = x.elemAsDouble(i);
        } else {
            for (std::size_t i = 0; i < L; ++i)
                srcCol[i] = x.elemAsDouble(i + c * rows);
        }

        // Output column. For row-vector input the "column" lives flat.
        double *dstCol = isRowVec ? od : od + c * N;
        transformOneColumn(srcCol.data(), L, dstCol, N, log2N,
                            ordering, forward, mr);
    }

    return out;
}

} // namespace

Value fwht(const Value &x, std::size_t n, const std::string &ordering,
           std::pmr::memory_resource *mr)
{
    return runWalshHadamard(x, n, ordering, /*forward=*/true, mr);
}

Value ifwht(const Value &y, std::size_t n, const std::string &ordering,
            std::pmr::memory_resource *mr)
{
    return runWalshHadamard(y, n, ordering, /*forward=*/false, mr);
}

// ── Engine adapters ─────────────────────────────────────────────────
namespace detail {

static void parseFwhtArgs(Span<const Value> args,
                           std::size_t &n, std::string &ordering)
{
    n = 0;
    ordering = "sequency";
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].isChar() || args[1].isString()) {
            // (x, ordering) — n omitted.
            ordering = args[1].toString();
            return;
        }
        n = static_cast<std::size_t>(args[1].toScalar());
    }
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (!(args[2].isChar() || args[2].isString()))
            throw Error("fwht: ordering must be a string",
                         0, 0, "fwht", "", "m:fwht:badOrdering");
        ordering = args[2].toString();
    }
}

void fwht_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fwht: requires (x [, n [, ordering]])",
                     0, 0, "fwht", "", "m:fwht:nargin");
    std::size_t n; std::string ordering;
    parseFwhtArgs(args, n, ordering);
    outs[0] = fwht(args[0], n, ordering, ctx.engine->resource());
}

void ifwht_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ifwht: requires (y [, n [, ordering]])",
                     0, 0, "ifwht", "", "m:ifwht:nargin");
    std::size_t n; std::string ordering;
    parseFwhtArgs(args, n, ordering);
    outs[0] = ifwht(args[0], n, ordering, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::signal
