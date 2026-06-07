// libs/image/src/filter/filter_reg.cpp
//
// Register half of the image filter builtins: the CallContext wrappers (two
// detail blocks, mirroring the interleaved compute TU) plus the VM-pausable
// nlfilter callback (NlfilterCallbackBuiltin + registerNlfilterCallbackBuiltin),
// all delegating to the engine-free compute in filter.cpp. library.cpp
// forward-declares + registers these by name. core/callback_builtin.hpp +
// core/vm.hpp (the continuation machinery) live here on the register side.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/filter/filter.hpp>
#include <numkit/image/type_convert/type_convert.hpp>

#include <numkit/core/callback_builtin.hpp>
#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/core/vm.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include "filter_detail.hpp"
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace numkit::image {

// [Phase 2b] relocated from filter.cpp compute: the synchronous Engine-driven
// neighbourhood filters (nlfilter/colfilt). The VM-pausable nlfilter path is
// NlfilterCallbackBuiltin below.

// ── nlfilter (general sliding-neighbourhood) ──────────────────────
//
// MATLAB R2025b nlfilter.m:
//   B = nlfilter(A, [m n], fun)
//   B = nlfilter(A, 'indexed', [m n], fun)
//
// For every pixel (i, j) ∈ A extract the m × n window
//   x = aa(i + 0..m-1, j + 0..n-1)
// from the padded image `aa` and call `fun(x)`. The output element
// is `b(i, j) = fun(x)`. Output class equals the class of the FIRST
// invocation of `fun` (matches MATLAB's `mkconstarray(class(...))`).
// Default `padval = 0`; `'indexed'` form uses `padval = 1` for
// `single` / `double` `A`, otherwise `padval = 0`.
//
// The dispatch goes through Engine::callFunctionHandle, matching the
// pattern adopted in libs/ode/ode45 (function_ref couldn't carry
// func-handle semantics through the round-trip).
Value nlfilter(numkit::Engine &eng, const Value &A,
               std::size_t m, std::size_t n, const Value &fun,
               bool indexed,
               std::pmr::memory_resource *mr)
{
    if (m < 1 || n < 1)
        throw Error("nlfilter: neighbourhood size must be positive",
                    0, 0, "nlfilter", "", "numkit:nlfilter:nhood");
    if (!fun.isFuncHandle())
        throw Error("nlfilter: 3rd argument must be a function handle",
                    0, 0, "nlfilter", "", "numkit:nlfilter:fun");

    const auto &dA = A.dims();
    if (dA.is3D())
        throw Error("nlfilter: A must be a 2-D image",
                    0, 0, "nlfilter", "", "numkit:nlfilter:rank");
    const std::size_t H = dA.rows();
    const std::size_t W = dA.cols();
    const ValueType inT = A.type();

    // Padding: 'indexed' uses 1.0 for single/double, else 0.
    double padval = 0.0;
    if (indexed) {
        padval = (inT == ValueType::DOUBLE || inT == ValueType::SINGLE)
               ? 1.0 : 0.0;
    }

    // Pad above by floor((m-1)/2) rows, below by ceil((m-1)/2);
    // left by floor((n-1)/2), right by ceil((n-1)/2). (MATLAB:
    // mkconstarray(class(a), padval, size(a)+nhood-1) — then drops
    // the original A into the offset block.)
    const std::size_t pad_top  = (m - 1) / 2;
    const std::size_t pad_left = (n - 1) / 2;
    const std::size_t Hpad     = H + m - 1;
    const std::size_t Wpad     = W + n - 1;

    // Build padded array in DOUBLE (we always read out via
    // elemAsDouble; this saves a class-specific dispatch).
    std::pmr::vector<double> aa(Hpad * Wpad, padval, mr);
    for (std::size_t j = 0; j < W; ++j)
        for (std::size_t i = 0; i < H; ++i)
            aa[(j + pad_left) * Hpad + (i + pad_top)] = A.elemAsDouble(j * H + i);

    // Allocate scratch window (DOUBLE — the class for `fun` is what
    // the kernel receives; MATLAB nlfilter forwards the same class
    // as `A`, but we choose DOUBLE here for simplicity. Tests use
    // class-agnostic kernels like `@(x) mean(x(:))`).
    Value window = Value::matrix(m, n, ValueType::DOUBLE, mr);
    double *wd = window.doubleDataMut();

    // First-call invocation determines output class.
    auto fill_window = [&](std::size_t i, std::size_t j) {
        for (std::size_t c = 0; c < n; ++c)
            for (std::size_t r = 0; r < m; ++r)
                wd[c * m + r] = aa[(j + c) * Hpad + (i + r)];
    };

    fill_window(0, 0);
    Value first = eng.callFunctionHandle(
        fun, Span<const Value>(&window, 1));
    if (first.numel() != 1)
        throw Error("nlfilter: fun must return a scalar",
                    0, 0, "nlfilter", "", "numkit:nlfilter:funScalar");
    const ValueType outT = first.type();
    Value B = Value::matrix(H, W, outT, mr);

    auto store = [&](std::size_t r, std::size_t c, const Value &v) {
        const std::size_t idx = c * H + r;
        const double d = v.toScalar();
        switch (outT) {
            case ValueType::DOUBLE:  B.doubleDataMut()[idx]  = d; break;
            case ValueType::SINGLE:  B.singleDataMut()[idx]  = static_cast<float>(d); break;
            case ValueType::UINT8:   B.uint8DataMut()[idx]   = static_cast<uint8_t>(d); break;
            case ValueType::UINT16:  B.uint16DataMut()[idx]  = static_cast<uint16_t>(d); break;
            case ValueType::UINT32:  B.uint32DataMut()[idx]  = static_cast<uint32_t>(d); break;
            case ValueType::UINT64:  B.uint64DataMut()[idx]  = static_cast<uint64_t>(d); break;
            case ValueType::INT8:    B.int8DataMut()[idx]    = static_cast<int8_t>(d); break;
            case ValueType::INT16:   B.int16DataMut()[idx]   = static_cast<int16_t>(d); break;
            case ValueType::INT32:   B.int32DataMut()[idx]   = static_cast<int32_t>(d); break;
            case ValueType::INT64:   B.int64DataMut()[idx]   = static_cast<int64_t>(d); break;
            case ValueType::LOGICAL: B.logicalDataMut()[idx] = d != 0.0 ? 1 : 0; break;
            default:
                throw Error("nlfilter: unsupported fun output class",
                            0, 0, "nlfilter", "", "numkit:nlfilter:outCls");
        }
    };

    store(0, 0, first);

    for (std::size_t i = 0; i < H; ++i) {
        for (std::size_t j = 0; j < W; ++j) {
            if (i == 0 && j == 0) continue;  // already filled
            fill_window(i, j);
            Value r = eng.callFunctionHandle(
                fun, Span<const Value>(&window, 1));
            if (r.numel() != 1)
                throw Error("nlfilter: fun must return a scalar at all (i,j)",
                            0, 0, "nlfilter", "", "numkit:nlfilter:funScalar");
            store(i, j, r);
        }
    }
    return B;
}


// ── colfilt (column-wise neighbourhood) ───────────────────────────
//
// MATLAB R2025b colfilt.m:
//   B = colfilt(A, [m n], block_type, fun)         (whole-matrix)
//   B = colfilt(A, [m n], [mblock nblock], block_type, fun)
//   B = colfilt(A, 'indexed', …)
//
// block_type ∈ {'sliding', 'distinct'} (case-insensitive, abbrev'd
// by leading char). Sliding mode:
//   1. Pad A by (m-1, n-1) with 0 (or 1 for 'indexed' double/single).
//   2. X is the matrix whose columns are the m*n elements of every
//      m × n window centred on (i, j); shape m*n × (H*W).
//   3. Call fun(X) — must return a row vector 1 × (H*W).
//   4. Reshape into H × W.
// Distinct mode:
//   1. Pad A to next multiple of [m, n].
//   2. X has one column per distinct m × n block.
//   3. fun(X) must return a same-size matrix; the columns are then
//      unpacked back into blocks (col2im 'distinct').
//   4. Crop the assembled image back to size(A).
//
// The optional [mblock nblock] arg is purely a memory optimisation
// (MATLAB explicitly notes: "does not change the result"). The
// engine adapter accepts and ignores it.
//
// Output class equals the class of fun()'s return value.
Value colfilt(numkit::Engine &eng, const Value &A,
              std::size_t m, std::size_t n,
              const std::string &block_type, const Value &fun,
              bool indexed,
              std::pmr::memory_resource *mr)
{
    if (m < 1 || n < 1)
        throw Error("colfilt: block size must be positive",
                    0, 0, "colfilt", "", "numkit:colfilt:nhood");
    if (!fun.isFuncHandle())
        throw Error("colfilt: fun must be a function handle",
                    0, 0, "colfilt", "", "numkit:colfilt:fun");

    const auto &dA = A.dims();
    if (dA.is3D())
        throw Error("colfilt: A must be a 2-D image",
                    0, 0, "colfilt", "", "numkit:colfilt:rank");
    const std::size_t H = dA.rows();
    const std::size_t W = dA.cols();
    const ValueType inT = A.type();

    std::string kind = block_type;
    for (auto &c : kind) c = static_cast<char>(
        std::tolower(static_cast<unsigned char>(c)));
    if (kind.empty())
        throw Error("colfilt: block_type must be 'sliding' or 'distinct'",
                    0, 0, "colfilt", "", "numkit:colfilt:blockType");
    const char first = kind[0];
    if (first != 's' && first != 'd')
        throw Error("colfilt: block_type must be 'sliding' or 'distinct'",
                    0, 0, "colfilt", "", "numkit:colfilt:blockType");

    // Common padval rule.
    double padval = 0.0;
    if (indexed && (inT == ValueType::DOUBLE || inT == ValueType::SINGLE))
        padval = 1.0;

    if (first == 's') {
        // ── Sliding ─────────────────────────────────────────────
        const std::size_t pad_top  = (m - 1) / 2;
        const std::size_t pad_left = (n - 1) / 2;
        const std::size_t Hpad = H + m - 1;
        const std::size_t Wpad = W + n - 1;
        const std::size_t Ncol = H * W;

        // Build X = m*n × Ncol in DOUBLE.
        Value X = Value::matrix(m * n, Ncol, ValueType::DOUBLE, mr);
        double *xd = X.doubleDataMut();

        // Pad row-by-col into a temporary scratch.
        std::pmr::vector<double> aa(Hpad * Wpad, padval, mr);
        for (std::size_t j = 0; j < W; ++j)
            for (std::size_t i = 0; i < H; ++i)
                aa[(j + pad_left) * Hpad + (i + pad_top)] =
                    A.elemAsDouble(j * H + i);

        // For each centre (i, j), gather m*n window values into
        // column k = j*H + i.
        for (std::size_t j = 0; j < W; ++j) {
            for (std::size_t i = 0; i < H; ++i) {
                const std::size_t k = j * H + i;
                std::size_t r = 0;
                // im2col 'sliding' iteration order: column-major
                // within each window, i.e. (col-major) flatten.
                for (std::size_t wc = 0; wc < n; ++wc)
                    for (std::size_t wr = 0; wr < m; ++wr)
                        xd[k * (m * n) + (r++)]
                            = aa[(j + wc) * Hpad + (i + wr)];
            }
        }

        Value result = eng.callFunctionHandle(
            fun, Span<const Value>(&X, 1));
        if (result.numel() != Ncol)
            throw Error("colfilt: sliding fun must return a 1 × N row "
                        "vector (one value per column)",
                        0, 0, "colfilt", "", "numkit:colfilt:funShape");

        // Reshape result (regardless of [1, Ncol] or [Ncol, 1]) into H × W.
        Value B = Value::matrix(H, W, result.type(), mr);
        const auto outT = result.type();
        for (std::size_t k = 0; k < Ncol; ++k) {
            const double v = result.elemAsDouble(k);
            const std::size_t idx = k;
            switch (outT) {
                case ValueType::DOUBLE: B.doubleDataMut()[idx]  = v; break;
                case ValueType::SINGLE: B.singleDataMut()[idx]  = static_cast<float>(v); break;
                case ValueType::UINT8:  B.uint8DataMut()[idx]   = static_cast<uint8_t>(v); break;
                case ValueType::UINT16: B.uint16DataMut()[idx]  = static_cast<uint16_t>(v); break;
                case ValueType::INT16:  B.int16DataMut()[idx]   = static_cast<int16_t>(v); break;
                case ValueType::INT32:  B.int32DataMut()[idx]   = static_cast<int32_t>(v); break;
                case ValueType::LOGICAL: B.logicalDataMut()[idx] = v != 0 ? 1 : 0; break;
                default:
                    throw Error("colfilt: unsupported fun output class",
                                0, 0, "colfilt", "", "numkit:colfilt:outCls");
            }
        }
        return B;
    }

    // ── Distinct ────────────────────────────────────────────────────
    const std::size_t mpad = (H % m) ? (m - H % m) : 0;
    const std::size_t npad = (W % n) ? (n - W % n) : 0;
    const std::size_t Hpad = H + mpad;
    const std::size_t Wpad = W + npad;
    const std::size_t mblocks = Hpad / m;
    const std::size_t nblocks = Wpad / n;
    const std::size_t Ncol    = mblocks * nblocks;

    // Pad to multiple of (m, n).
    std::pmr::vector<double> aa(Hpad * Wpad, padval, mr);
    for (std::size_t j = 0; j < W; ++j)
        for (std::size_t i = 0; i < H; ++i)
            aa[j * Hpad + i] = A.elemAsDouble(j * H + i);

    // Build X = m*n × Ncol; column index k = bj * mblocks + bi.
    Value X = Value::matrix(m * n, Ncol, ValueType::DOUBLE, mr);
    double *xd = X.doubleDataMut();
    for (std::size_t bj = 0; bj < nblocks; ++bj) {
        for (std::size_t bi = 0; bi < mblocks; ++bi) {
            const std::size_t k = bj * mblocks + bi;
            std::size_t r = 0;
            for (std::size_t wc = 0; wc < n; ++wc)
                for (std::size_t wr = 0; wr < m; ++wr)
                    xd[k * (m * n) + (r++)]
                        = aa[(bj * n + wc) * Hpad + (bi * m + wr)];
        }
    }

    Value result = eng.callFunctionHandle(
        fun, Span<const Value>(&X, 1));
    if (result.numel() != m * n * Ncol)
        throw Error("colfilt: distinct fun must return a matrix of the "
                    "same shape as its input (m*n × N)",
                    0, 0, "colfilt", "", "numkit:colfilt:funShape");

    // Reassemble padded image, then crop.
    const auto outT = result.type();
    std::pmr::vector<double> bb(Hpad * Wpad, 0.0, mr);
    for (std::size_t bj = 0; bj < nblocks; ++bj) {
        for (std::size_t bi = 0; bi < mblocks; ++bi) {
            const std::size_t k = bj * mblocks + bi;
            std::size_t r = 0;
            for (std::size_t wc = 0; wc < n; ++wc)
                for (std::size_t wr = 0; wr < m; ++wr) {
                    const double v = result.elemAsDouble(k * (m * n) + (r++));
                    bb[(bj * n + wc) * Hpad + (bi * m + wr)] = v;
                }
        }
    }

    Value B = Value::matrix(H, W, outT, mr);
    for (std::size_t j = 0; j < W; ++j) {
        for (std::size_t i = 0; i < H; ++i) {
            const double v = bb[j * Hpad + i];
            const std::size_t idx = j * H + i;
            switch (outT) {
                case ValueType::DOUBLE: B.doubleDataMut()[idx]  = v; break;
                case ValueType::SINGLE: B.singleDataMut()[idx]  = static_cast<float>(v); break;
                case ValueType::UINT8:  B.uint8DataMut()[idx]   = static_cast<uint8_t>(v); break;
                case ValueType::UINT16: B.uint16DataMut()[idx]  = static_cast<uint16_t>(v); break;
                case ValueType::INT16:  B.int16DataMut()[idx]   = static_cast<int16_t>(v); break;
                case ValueType::INT32:  B.int32DataMut()[idx]   = static_cast<int32_t>(v); break;
                case ValueType::LOGICAL: B.logicalDataMut()[idx] = v != 0 ? 1 : 0; break;
                default:
                    throw Error("colfilt: unsupported fun output class",
                                0, 0, "colfilt", "", "numkit:colfilt:outCls");
            }
        }
    }
    return B;
}

namespace detail {

namespace {
PadMode parse_pad_mode(const Value &v, double &pad_value, bool &is_value) {
    is_value = false;
    pad_value = 0.0;
    if (v.isChar() || v.isString()) {
        auto s = v.toString();
        if (s == "replicate") return PadMode::Replicate;
        if (s == "symmetric") return PadMode::Symmetric;
        if (s == "circular")  return PadMode::Circular;
        // Unknown mode → treat as scalar 0.
    }
    if (v.numel() == 1) {
        is_value = true;
        pad_value = v.toScalar();
    }
    return PadMode::Constant;
}
} // anonymous

void padarray_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("padarray: requires (A, padsize[, val|mode][, direction])",
                    0, 0, "padarray", "", "numkit:padarray:nargin");

    std::vector<int> padsize;
    {
        const Value &p = args[1];
        const size_t n = p.numel();
        padsize.resize(n);
        for (size_t i = 0; i < n; ++i) padsize[i] = int(p.elemAsDouble(i));
    }

    PadMode mode = PadMode::Constant;
    double pad_value = 0.0;
    std::string direction = "both";

    // Parse optional trailing args: pad_value-or-mode (one) and/or direction.
    for (size_t i = 2; i < args.size(); ++i) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            auto s = a.toString();
            if (s == "pre" || s == "post" || s == "both") {
                direction = s;
            } else {
                bool dummy;
                mode = parse_pad_mode(a, pad_value, dummy);
            }
        } else {
            // Treat as scalar pad value.
            pad_value = a.toScalar();
            mode = PadMode::Constant;
        }
    }

    outs[0] = padarray(args[0], padsize, mode, pad_value, direction, ctx.engine->resource());
}

void fspecial_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    std::string type = "gaussian";
    if (!args.empty() && (args[0].isChar() || args[0].isString()))
        type = args[0].toString();

    std::vector<double> params;
    if (type == "motion") {
        // 'motion' takes two INDEPENDENT scalars [len, theta]; do NOT
        // size-double the 2nd arg the way the size-based filters do.
        if (args.size() >= 2 && args[1].numel() >= 1)
            params.push_back(args[1].elemAsDouble(0));
        if (args.size() >= 3 && args[2].numel() >= 1)
            params.push_back(args[2].elemAsDouble(0));
    } else {
        // Second positional arg can be a scalar (square size) or a 2-vector
        // [rows cols]; a scalar size is doubled so a following scalar (sigma /
        // alpha) lands in slot 3.
        if (args.size() >= 2) {
            const Value &v = args[1];
            if (v.numel() == 1) {
                params.push_back(v.toScalar());
                params.push_back(v.toScalar());
            } else if (v.numel() == 2) {
                params.push_back(v.elemAsDouble(0));
                params.push_back(v.elemAsDouble(1));
            } else if (v.numel() > 0) {
                for (size_t i = 0; i < v.numel(); ++i)
                    params.push_back(v.elemAsDouble(i));
            }
        }
        // Third positional arg = sigma / alpha / radius (scalar).
        if (args.size() >= 3 && args[2].numel() == 1)
            params.push_back(args[2].toScalar());
    }

    outs[0] = fspecial(type, params, ctx.engine->resource());
}

void imfilter_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imfilter: requires (I, h[, options])",
                    0, 0, "imfilter", "", "numkit:imfilter:nargin");
    PadMode boundary = PadMode::Constant;
    double pad_value = 0.0;
    bool full = false;
    bool flip_kernel = false;  // 'corr' default
    for (size_t i = 2; i < args.size(); ++i) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            auto s = a.toString();
            if      (s == "replicate") boundary = PadMode::Replicate;
            else if (s == "symmetric") boundary = PadMode::Symmetric;
            else if (s == "circular")  boundary = PadMode::Circular;
            else if (s == "full")      full = true;
            else if (s == "same")      full = false;
            else if (s == "conv")      flip_kernel = true;
            else if (s == "corr")      flip_kernel = false;
        } else {
            pad_value = a.toScalar();
            boundary = PadMode::Constant;
        }
    }
    outs[0] = imfilter(args[0], args[1], boundary, pad_value, full, flip_kernel, ctx.engine->resource());
}

void imgaussfilt_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imgaussfilt: requires (I[, sigma][, FilterSize])",
                    0, 0, "imgaussfilt", "", "numkit:imgaussfilt:nargin");
    double sigma = (args.size() >= 2 && !args[1].isEmpty()) ? args[1].toScalar() : 0.5;
    int fs = 0;  // auto
    // Look for 'FilterSize' name-value pair.
    for (size_t i = 2; i + 1 < args.size(); ++i) {
        if ((args[i].isChar() || args[i].isString())
            && args[i].toString() == "FilterSize") {
            fs = (int)args[i + 1].toScalar();
            break;
        }
    }
    outs[0] = imgaussfilt(args[0], sigma, fs, ctx.engine->resource());
}

void imboxfilt_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imboxfilt: requires (I[, FilterSize])",
                    0, 0, "imboxfilt", "", "numkit:imboxfilt:nargin");
    int fs = (args.size() >= 2 && !args[1].isEmpty()) ? (int)args[1].toScalar() : 3;
    outs[0] = imboxfilt(args[0], fs, ctx.engine->resource());
}

void modefilt_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("modefilt: requires (A[, filtSize[, padopt]])",
                    0, 0, "modefilt", "", "numkit:modefilt:nargin");
    // Defaults adapt to input rank — MATLAB picks [3 3] for 2-D, [3 3 3] for 3-D.
    const bool input3D = (args[0].dims().ndim() == 3);
    int fH = 3, fW = 3, fD = input3D ? 3 : 0;
    std::string padopt = "symmetric";
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].isChar() || args[1].isString()) {
            padopt = args[1].toString();
        } else {
            if (args[1].numel() == 1) {
                fH = fW = static_cast<int>(args[1].toScalar());
                if (input3D) fD = fH;
            } else if (args[1].numel() == 2) {
                fH = static_cast<int>(args[1].elemAsDouble(0));
                fW = static_cast<int>(args[1].elemAsDouble(1));
                if (input3D) fD = 1;  // no 3rd dim filter specified
            } else if (args[1].numel() >= 3) {
                fH = static_cast<int>(args[1].elemAsDouble(0));
                fW = static_cast<int>(args[1].elemAsDouble(1));
                fD = static_cast<int>(args[1].elemAsDouble(2));
            }
        }
    }
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (args[2].isChar() || args[2].isString())
            padopt = args[2].toString();
        else
            throw Error("modefilt: padopt must be a string",
                        0, 0, "modefilt", "", "numkit:modefilt:badPad");
    }
    if (input3D)
        outs[0] = modefilt3D(args[0], fH, fW, fD, padopt, ctx.engine->resource());
    else
        outs[0] = modefilt(args[0], fH, fW, padopt, ctx.engine->resource());
}

void integralBoxFilter_reg(Span<const Value> args, size_t /*nargout*/,
                            Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("integralBoxFilter: requires (I [, filterSize [, NV...]])",
                    0, 0, "integralBoxFilter", "", "numkit:integralBoxFilter:nargin");

    // Defaults match MATLAB: 3-by-3 box.
    int fH = 3, fW = 3;
    size_t nvStart = 1;
    if (args.size() >= 2 && !args[1].isEmpty()
        && !args[1].isChar() && !args[1].isString()) {
        const Value &fsArg = args[1];
        if (fsArg.numel() == 1) {
            fH = fW = static_cast<int>(fsArg.toScalar());
        } else if (fsArg.numel() == 2) {
            fH = static_cast<int>(fsArg.elemAsDouble(0));
            fW = static_cast<int>(fsArg.elemAsDouble(1));
        } else {
            throw Error("integralBoxFilter: filterSize must be a scalar or 2-element vector",
                        0, 0, "integralBoxFilter", "", "numkit:integralBoxFilter:badSize");
        }
        nvStart = 2;
    }

    // MATLAB default NormalizationFactor = 1/(fH·fW) (mean); it is a
    // multiplier applied to the box sum, NOT a divisor.
    double normFactor = 1.0 / (static_cast<double>(fH) * static_cast<double>(fW));
    // NV-pair: NormalizationFactor.
    for (size_t i = nvStart; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("integralBoxFilter: name-value name must be a string",
                        0, 0, "integralBoxFilter", "", "numkit:integralBoxFilter:badNVName");
        const std::string key = args[i].toString();
        std::string lower = key;
        for (auto &ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (lower == "normalizationfactor") {
            normFactor = args[i + 1].toScalar();
        } else {
            throw Error("integralBoxFilter: unknown name-value key '" + key + "'",
                        0, 0, "integralBoxFilter", "", "numkit:integralBoxFilter:badNVKey");
        }
    }
    outs[0] = integralBoxFilter(args[0], fH, fW, normFactor, ctx.engine->resource());
}

void integralBoxFilter3_reg(Span<const Value> args, size_t /*nargout*/,
                            Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("integralBoxFilter3: requires (A [, filterSize [, NV...]])",
                    0, 0, "integralBoxFilter3", "", "numkit:integralBoxFilter3:nargin");

    // Default: 3×3×3 box.
    int fH = 3, fW = 3, fP = 3;
    size_t nvStart = 1;
    if (args.size() >= 2 && !args[1].isEmpty()
        && !args[1].isChar() && !args[1].isString()) {
        const Value &fsArg = args[1];
        if (fsArg.numel() == 1) {
            fH = fW = fP = static_cast<int>(fsArg.toScalar());
        } else if (fsArg.numel() == 3) {
            fH = static_cast<int>(fsArg.elemAsDouble(0));
            fW = static_cast<int>(fsArg.elemAsDouble(1));
            fP = static_cast<int>(fsArg.elemAsDouble(2));
        } else {
            throw Error("integralBoxFilter3: filterSize must be a scalar or 3-element vector",
                        0, 0, "integralBoxFilter3", "", "numkit:integralBoxFilter3:badSize");
        }
        nvStart = 2;
    }

    // MATLAB default NormalizationFactor = 1/prod(filterSize) (mean).
    double normFactor = 1.0 / (static_cast<double>(fH) * static_cast<double>(fW)
                               * static_cast<double>(fP));
    for (size_t i = nvStart; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("integralBoxFilter3: name-value name must be a string",
                        0, 0, "integralBoxFilter3", "", "numkit:integralBoxFilter3:badNVName");
        std::string lower = args[i].toString();
        for (auto &ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (lower == "normalizationfactor") {
            normFactor = args[i + 1].toScalar();
        } else {
            throw Error("integralBoxFilter3: unknown name-value key",
                        0, 0, "integralBoxFilter3", "", "numkit:integralBoxFilter3:badNVKey");
        }
    }
    outs[0] = integralBoxFilter3(args[0], fH, fW, fP, normFactor, ctx.engine->resource());
}

void medfilt3_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("medfilt3: requires (V[, [M N P]])",
                    0, 0, "medfilt3", "", "numkit:medfilt3:nargin");
    int M = 3, N = 3, P = 3;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &v = args[1];
        if (v.numel() == 1) {
            M = N = P = static_cast<int>(v.toScalar());
        } else if (v.numel() >= 3) {
            M = static_cast<int>(v.elemAsDouble(0));
            N = static_cast<int>(v.elemAsDouble(1));
            P = static_cast<int>(v.elemAsDouble(2));
        }
    }
    outs[0] = medfilt3(args[0], M, N, P, ctx.engine->resource());
}

void imgaussfilt3_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imgaussfilt3: requires (V[, sigma])",
                    0, 0, "imgaussfilt3", "", "numkit:imgaussfilt3:nargin");
    double sigH = 0.5, sigW = 0.5, sigP = 0.5;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &v = args[1];
        if (v.numel() == 1) {
            sigH = sigW = sigP = v.toScalar();
        } else if (v.numel() >= 3) {
            sigH = v.elemAsDouble(0);
            sigW = v.elemAsDouble(1);
            sigP = v.elemAsDouble(2);
        }
    }
    outs[0] = imgaussfilt3(args[0], sigH, sigW, sigP, ctx.engine->resource());
}

void convmtx2_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("convmtx2: requires (h, m, n) or (h, [m n])",
                    0, 0, "convmtx2", "", "numkit:convmtx2:nargin");
    int m = 0, n = 0;
    if (args.size() >= 3) {
        m = static_cast<int>(args[1].toScalar());
        n = static_cast<int>(args[2].toScalar());
    } else {
        const Value &v = args[1];
        if (v.numel() != 2)
            throw Error("convmtx2: 2nd arg must be a 2-element vector or pair (m, n)",
                        0, 0, "convmtx2", "", "numkit:convmtx2:size");
        m = static_cast<int>(v.elemAsDouble(0));
        n = static_cast<int>(v.elemAsDouble(1));
    }
    outs[0] = convmtx2(args[0], m, n, ctx.engine->resource());
}

void imboxfilt3_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imboxfilt3: requires (V[, FilterSize])",
                    0, 0, "imboxfilt3", "", "numkit:imboxfilt3:nargin");
    int fH = 3, fW = 3, fP = 3;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &v = args[1];
        if (v.numel() == 1) {
            fH = fW = fP = (int)v.toScalar();
        } else if (v.numel() >= 3) {
            fH = (int)v.elemAsDouble(0);
            fW = (int)v.elemAsDouble(1);
            fP = (int)v.elemAsDouble(2);
        }
    }
    outs[0] = imboxfilt3(args[0], fH, fW, fP, ctx.engine->resource());
}

void freqz2_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("freqz2: requires (h [, M, N])",
                    0, 0, "freqz2", "", "numkit:freqz2:nargin");
    size_t M = 64, N = 64;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &v = args[1];
        if (v.numel() == 1) {
            M = N = static_cast<size_t>(v.toScalar());
        } else if (v.numel() >= 2) {
            M = static_cast<size_t>(v.elemAsDouble(0));
            N = static_cast<size_t>(v.elemAsDouble(1));
        }
    }
    if (args.size() >= 3 && !args[2].isEmpty())
        N = static_cast<size_t>(args[2].toScalar());
    auto [H, f1, f2] = freqz2(args[0], M, N, ctx.engine->resource());
    outs[0] = std::move(H);
    if (nargout > 1) outs[1] = std::move(f1);
    if (nargout > 2) outs[2] = std::move(f2);
}

void medfilt2_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("medfilt2: requires (I[, [m n]])",
                    0, 0, "medfilt2", "", "numkit:medfilt2:nargin");
    int rows = 3, cols = 3;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &v = args[1];
        if (v.numel() == 1) {
            rows = cols = (int)v.toScalar();
        } else if (v.numel() >= 2) {
            rows = (int)v.elemAsDouble(0);
            cols = (int)v.elemAsDouble(1);
        }
    }
    outs[0] = medfilt2(args[0], rows, cols, ctx.engine->resource());
}

void im2col_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("im2col: requires (A, [m n] [, block_type])",
                    0, 0, "im2col", "", "numkit:im2col:nargin");
    int m = 0, n = 0;
    const Value &sz = args[1];
    if (sz.numel() == 1) {
        m = n = (int)sz.toScalar();
    } else if (sz.numel() >= 2) {
        m = (int)sz.elemAsDouble(0);
        n = (int)sz.elemAsDouble(1);
    } else {
        throw Error("im2col: block size must be scalar or 2-vector",
                    0, 0, "im2col", "", "numkit:im2col:size");
    }
    std::string mode = "sliding";
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString()))
        mode = args[2].toString();
    outs[0] = im2col(args[0], m, n, mode, ctx.engine->resource());
}

void imbilatfilt_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imbilatfilt: requires (I[, degreeOfSmoothing, spatialSigma])",
                    0, 0, "imbilatfilt", "", "numkit:imbilatfilt:nargin");

    // Default DegreeOfSmoothing depends on input class range — MATLAB
    // uses 0.01 · diff(getrangefromclass(I)). For our purposes:
    //   double / single / logical → 0.01    (range = 1)
    //   uint8                     → 6.50    (= 0.01·255²·1e-3 ish; matches MATLAB)
    //   uint16                    → tiny but absolute; we use 0.01·65535²·1e-3
    // Actually MATLAB stores it as variance of the range Gaussian
    // expressed in the same units as I. For unit-range double the
    // canonical value is 0.01; for integer classes we scale by the
    // range so the *relative* sensitivity matches.
    const Value &I = args[0];
    double dos_default;
    switch (I.type()) {
        case ValueType::UINT8:
            dos_default = 0.01 * 255.0 * 255.0; break;
        case ValueType::UINT16:
            dos_default = 0.01 * 65535.0 * 65535.0; break;
        case ValueType::INT16:
            dos_default = 0.01 * 65535.0 * 65535.0; break;
        default:
            dos_default = 0.01; break;
    }
    double dos    = (args.size() >= 2 && !args[1].isEmpty())
                    ? args[1].toScalar() : dos_default;
    double sigma  = (args.size() >= 3 && !args[2].isEmpty())
                    ? args[2].toScalar() : 1.0;
    outs[0] = imbilatfilt(I, dos, sigma, ctx.engine->resource());
}

void col2im_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("col2im: requires (B, [m n], [mm nn] [, block_type])",
                    0, 0, "col2im", "", "numkit:col2im:nargin");
    int m, n, mm, nn;
    {
        const Value &b = args[1];
        if (b.numel() < 1)
            throw Error("col2im: empty block size",
                        0, 0, "col2im", "", "numkit:col2im:size");
        m = (int)b.elemAsDouble(0);
        n = (b.numel() >= 2) ? (int)b.elemAsDouble(1) : m;
    }
    {
        const Value &s = args[2];
        if (s.numel() < 1)
            throw Error("col2im: empty image size",
                        0, 0, "col2im", "", "numkit:col2im:size");
        mm = (int)s.elemAsDouble(0);
        nn = (s.numel() >= 2) ? (int)s.elemAsDouble(1) : mm;
    }
    std::string mode = "sliding";
    if (args.size() >= 4 && (args[3].isChar() || args[3].isString()))
        mode = args[3].toString();
    outs[0] = col2im(args[0], m, n, mm, nn, mode, ctx.engine->resource());
}

void imnoise_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imnoise: requires (I, mode [, p1, p2])",
                    0, 0, "imnoise", "", "numkit:imnoise:nargin");
    if (!(args[1].isChar() || args[1].isString()))
        throw Error("imnoise: mode must be a string",
                    0, 0, "imnoise", "", "numkit:imnoise:mode");
    const std::string mode = args[1].toString();
    Value p1, p2;
    if (args.size() >= 3) p1 = args[2];
    if (args.size() >= 4) p2 = args[3];
    outs[0] = imnoise(args[0], mode, p1, p2, ctx.engine->resource());
}

void imsharpen_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imsharpen: requires (I[, radius, amount, threshold])",
                    0, 0, "imsharpen", "", "numkit:imsharpen:nargin");
    double radius = 1.0, amount = 0.8, threshold = 0.0;
    // Accept either positional (I, radius, amount, threshold) or
    // name-value pairs ('Radius', r, 'Amount', a, 'Threshold', t).
    size_t i = 1;
    bool sawNV = false;
    while (i < args.size()) {
        if (args[i].isChar() || args[i].isString()) {
            sawNV = true;
            const std::string nm = args[i].toString();
            if (i + 1 >= args.size())
                throw Error("imsharpen: missing value for '" + nm + "'",
                            0, 0, "imsharpen", "", "numkit:imsharpen:nv");
            const double v = args[i + 1].toScalar();
            if (nm == "Radius" || nm == "radius") radius = v;
            else if (nm == "Amount" || nm == "amount") amount = v;
            else if (nm == "Threshold" || nm == "threshold") threshold = v;
            else throw Error("imsharpen: unknown option '" + nm + "'",
                             0, 0, "imsharpen", "", "numkit:imsharpen:opt");
            i += 2;
        } else {
            if (sawNV)
                throw Error("imsharpen: positional after name-value",
                            0, 0, "imsharpen", "", "numkit:imsharpen:syntax");
            const double v = args[i].toScalar();
            if      (i == 1) radius    = v;
            else if (i == 2) amount    = v;
            else if (i == 3) threshold = v;
            ++i;
        }
    }
    outs[0] = imsharpen(args[0], radius, amount, threshold, ctx.engine->resource());
}

void stdfilt_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("stdfilt: requires (I [, domain])",
                    0, 0, "stdfilt", "", "numkit:stdfilt:nargin");
    Value dom;
    if (args.size() >= 2 && !args[1].isEmpty()) dom = args[1];
    outs[0] = stdfilt(args[0], dom, ctx.engine->resource());
}

void rangefilt_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rangefilt: requires (I [, domain])",
                    0, 0, "rangefilt", "", "numkit:rangefilt:nargin");
    Value dom;
    if (args.size() >= 2 && !args[1].isEmpty()) dom = args[1];
    outs[0] = rangefilt(args[0], dom, ctx.engine->resource());
}

void imsmooth_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imsmooth: requires (I [, name [, sigma]])",
                    0, 0, "imsmooth", "", "numkit:imsmooth:nargin");
    auto *mr = ctx.engine->resource();
    std::string name = "Gaussian";
    double sigma = 0.5;
    // imsmooth(I, scalar) — Octave shorthand: scalar is sigma, name=Gaussian.
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].isChar() || args[1].isString()) {
            name = args[1].toString();
            if (args.size() >= 3 && !args[2].isEmpty())
                sigma = args[2].toScalar();
        } else if (args[1].numel() == 1) {
            sigma = args[1].toScalar();
        }
    }
    outs[0] = imsmooth(args[0], name, sigma, mr);
}

void entropyfilt_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("entropyfilt: requires (I [, domain])",
                    0, 0, "entropyfilt", "", "numkit:entropyfilt:nargin");
    Value dom;
    if (args.size() >= 2 && !args[1].isEmpty()) dom = args[1];
    outs[0] = entropyfilt(args[0], dom, ctx.engine->resource());
}

void ordfilt2_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ordfilt2: requires (A, nth, domain [, S] [, padding])",
                    0, 0, "ordfilt2", "", "numkit:ordfilt2:nargin");
    auto *mr = ctx.engine->resource();
    const int nth = static_cast<int>(args[1].toScalar());
    Value domain = args[2];
    if (domain.numel() == 1) {
        // Scalar domain → square true(N).
        const int n = static_cast<int>(domain.toScalar());
        domain = Value::matrix((size_t)n, (size_t)n, ValueType::LOGICAL, mr);
        std::uint8_t *dd = domain.logicalDataMut();
        for (size_t i = 0; i < (size_t)n * (size_t)n; ++i) dd[i] = 1;
    }
    Value S;
    PadMode pad = PadMode::Constant;
    double pad_value = 0.0;
    for (size_t i = 3; i < args.size(); ++i) {
        const Value &a = args[i];
        if (a.isEmpty()) continue;
        if (a.isChar() || a.isString()) {
            const std::string s = a.toString();
            if      (s == "replicate") pad = PadMode::Replicate;
            else if (s == "symmetric") pad = PadMode::Symmetric;
            else if (s == "circular")  pad = PadMode::Circular;
            else throw Error("ordfilt2: unknown padding mode",
                              0, 0, "ordfilt2", "", "numkit:ordfilt2:pad");
        } else if (a.numel() == 1) {
            pad = PadMode::Constant;
            pad_value = a.toScalar();
        } else if (a.dims().rows() == domain.dims().rows() &&
                   a.dims().cols() == domain.dims().cols()) {
            S = a;
        } else {
            throw Error("ordfilt2: unrecognized argument shape",
                        0, 0, "ordfilt2", "", "numkit:ordfilt2:arg");
        }
    }
    outs[0] = ordfilt2(args[0], nth, domain, S, pad, pad_value, mr);
}

void wiener2_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("wiener2: requires (I [, nhood [, noise]])",
                    0, 0, "wiener2", "", "numkit:wiener2:nargin");
    size_t nh = 3, nw = 3;
    double noise = std::nan("");
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].numel() == 1) {
            // Single scalar = noise (Octave convention when no nhood).
            noise = args[1].toScalar();
        } else if (args[1].numel() >= 2) {
            nh = static_cast<size_t>(args[1].elemAsDouble(0));
            nw = static_cast<size_t>(args[1].elemAsDouble(1));
        }
    }
    if (args.size() >= 3 && !args[2].isEmpty())
        noise = args[2].toScalar();
    auto [denoised, n] =
        wiener2(args[0], nh, nw, noise, ctx.engine->resource());
    outs[0] = std::move(denoised);
    if (nargout > 1) outs[1] = std::move(n);
}

} // namespace detail

namespace detail {

void roifilt2_reg(Span<const Value> args, std::size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("roifilt2: requires (h, I, BW) or (I, BW, fun)",
                    0, 0, "roifilt2", "", "numkit:roifilt2:nargin");
    auto *mr = ctx.engine->resource();
    // Function-handle form: J = roifilt2(I, BW, fun).
    if (args[2].isFuncHandle() || args[2].isChar() || args[2].isString()) {
        const Value &I = args[0];
        const Value &BW = args[1];
        if (I.dims().ndim() > 2)
            throw Error("roifilt2: I must be a 2-D image",
                        0, 0, "roifilt2", "", "numkit:roifilt2:imageMustBe2D");
        Value filtRes = ctx.engine->callFunctionHandle(
            args[2], Span<const Value>(&I, 1));
        outs[0] = roifilt2_combine(I, filtRes, BW, mr);
        return;
    }
    // Filter form: J = roifilt2(h, I, BW).
    outs[0] = roifilt2(args[0], args[1], args[2], mr);
}

// State-machine nlfilter (VM_CALLBACKS_PLAN.md): apply a user-code kernel to
// each sliding window as a pausable VM frame. Mirrors nlfilter_reg's parsing and
// nlfilter()'s padding / windowing / column-major output. Builtin handles,
// 'indexed'/rank/arg errors fall back to the synchronous nlfilter_reg.
struct NlfilterCallbackBuiltin : CallbackBuiltin
{
    std::shared_ptr<VmContinuation> tryStart(Span<const Value> args, std::size_t nargout,
                                             Value *dest, Engine &eng) override
    {
        if (args.size() < 3 || nargout > 1)
            return nullptr;
        bool indexed = false;
        std::size_t k = 1;
        if (args[1].isChar() || args[1].isString()) {
            std::string s = args[1].toString();
            for (auto &c : s)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (s != "indexed")
                return nullptr;
            indexed = true;
            k = 2;
        }
        if (k + 1 >= args.size())
            return nullptr;
        const Value &nh = args[k];
        const Value &fn = args[k + 1];
        if (!eng.isUserCodeHandle(fn))
            return nullptr; // builtin handle → synchronous nlfilter
        if (nh.numel() != 2)
            return nullptr;
        const Value &A = args[0];
        const auto &dA = A.dims();
        if (dA.is3D())
            return nullptr;
        const std::size_t m = static_cast<std::size_t>(nh.elemAsDouble(0));
        const std::size_t n = static_cast<std::size_t>(nh.elemAsDouble(1));
        if (m < 1 || n < 1)
            return nullptr;
        const std::size_t H = dA.rows(), W = dA.cols();
        const ValueType inT = A.type();
        double padval = 0.0;
        if (indexed)
            padval = (inT == ValueType::DOUBLE || inT == ValueType::SINGLE) ? 1.0 : 0.0;
        const std::size_t pad_top = (m - 1) / 2, pad_left = (n - 1) / 2;
        const std::size_t Hpad = H + m - 1;
        auto aa = std::make_shared<std::vector<double>>((H + m - 1) * (W + n - 1), padval);
        for (std::size_t j = 0; j < W; ++j)
            for (std::size_t i = 0; i < H; ++i)
                (*aa)[(j + pad_left) * Hpad + (i + pad_top)] = A.elemAsDouble(j * H + i);
        auto *mr = eng.resource();
        auto cont = std::make_shared<LoopContinuation>();
        cont->handle = fn;
        cont->n = H * W;
        cont->dest = dest;
        cont->makeArgs = [aa, H, W, m, n, Hpad, mr](std::size_t kk) -> std::vector<Value> {
            const std::size_t i = kk / W, j = kk % W; // row-major, matching nlfilter()
            Value window = Value::matrix(m, n, ValueType::DOUBLE, mr);
            double *wd = window.doubleDataMut();
            for (std::size_t c = 0; c < n; ++c)
                for (std::size_t r = 0; r < m; ++r)
                    wd[c * m + r] = (*aa)[(j + c) * Hpad + (i + r)];
            return {std::move(window)};
        };
        cont->pack = [H, W, mr](std::vector<Value> &results) -> Value {
            if (results.empty())
                return Value::matrix(H, W, ValueType::DOUBLE, mr);
            const ValueType outT = results[0].type();
            Value B = Value::matrix(H, W, outT, mr);
            for (std::size_t kk = 0; kk < results.size(); ++kk) {
                const Value &v = results[kk];
                if (v.numel() != 1)
                    throw Error("nlfilter: fun must return a scalar", 0, 0, "nlfilter", "",
                                "numkit:nlfilter:funScalar");
                const std::size_t idx = (kk % W) * H + (kk / W); // column-major output
                const double d = v.toScalar();
                switch (outT) {
                    case ValueType::DOUBLE:  B.doubleDataMut()[idx]  = d; break;
                    case ValueType::SINGLE:  B.singleDataMut()[idx]  = static_cast<float>(d); break;
                    case ValueType::UINT8:   B.uint8DataMut()[idx]   = static_cast<uint8_t>(d); break;
                    case ValueType::UINT16:  B.uint16DataMut()[idx]  = static_cast<uint16_t>(d); break;
                    case ValueType::UINT32:  B.uint32DataMut()[idx]  = static_cast<uint32_t>(d); break;
                    case ValueType::UINT64:  B.uint64DataMut()[idx]  = static_cast<uint64_t>(d); break;
                    case ValueType::INT8:    B.int8DataMut()[idx]    = static_cast<int8_t>(d); break;
                    case ValueType::INT16:   B.int16DataMut()[idx]   = static_cast<int16_t>(d); break;
                    case ValueType::INT32:   B.int32DataMut()[idx]   = static_cast<int32_t>(d); break;
                    case ValueType::INT64:   B.int64DataMut()[idx]   = static_cast<int64_t>(d); break;
                    case ValueType::LOGICAL: B.logicalDataMut()[idx] = d != 0.0 ? 1 : 0; break;
                    default:
                        throw Error("nlfilter: unsupported fun output class", 0, 0, "nlfilter", "",
                                    "numkit:nlfilter:outCls");
                }
            }
            return B;
        };
        cont->results.reserve(cont->n);
        return cont;
    }
};

void nlfilter_reg(Span<const Value> args, std::size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nlfilter: requires (A, [m n], fun) or "
                    "(A, 'indexed', [m n], fun)",
                    0, 0, "nlfilter", "", "numkit:nlfilter:nargin");
    auto *mr = ctx.engine->resource();

    bool indexed = false;
    std::size_t k = 1;
    if (args[1].isChar() || args[1].isString()) {
        std::string s = args[1].toString();
        for (auto &c : s) c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
        if (s != "indexed")
            throw Error("nlfilter: unknown literal '" + args[1].toString()
                      + "' (expected 'indexed')",
                        0, 0, "nlfilter", "", "numkit:nlfilter:badLiteral");
        indexed = true;
        k = 2;
    }
    if (k + 1 >= args.size())
        throw Error("nlfilter: requires (A, [m n], fun) "
                    "or (A, 'indexed', [m n], fun)",
                    0, 0, "nlfilter", "", "numkit:nlfilter:nargin");
    const Value &nh = args[k];
    const Value &fn = args[k + 1];
    if (nh.numel() != 2)
        throw Error("nlfilter: neighbourhood must be a 2-element vector",
                    0, 0, "nlfilter", "", "numkit:nlfilter:nhood");
    const std::size_t m = static_cast<std::size_t>(nh.elemAsDouble(0));
    const std::size_t n = static_cast<std::size_t>(nh.elemAsDouble(1));
    outs[0] = nlfilter(*ctx.engine, args[0], m, n, fn, indexed, mr);
}

void colfilt_reg(Span<const Value> args, std::size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("colfilt: requires (A, [m n], block_type, fun) "
                    "or (A, [m n], [mblock nblock], block_type, fun)",
                    0, 0, "colfilt", "", "numkit:colfilt:nargin");
    auto *mr = ctx.engine->resource();

    bool indexed = false;
    std::size_t k = 1;
    if (args[1].isChar() || args[1].isString()) {
        std::string s = args[1].toString();
        for (auto &c : s) c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
        if (s != "indexed")
            throw Error("colfilt: unknown literal '" + args[1].toString()
                      + "' (expected 'indexed')",
                        0, 0, "colfilt", "", "numkit:colfilt:badLiteral");
        indexed = true;
        k = 2;
    }
    if (k + 2 >= args.size())
        throw Error("colfilt: requires (A, [m n], block_type, fun)",
                    0, 0, "colfilt", "", "numkit:colfilt:nargin");
    const Value &nh = args[k];
    if (nh.numel() != 2)
        throw Error("colfilt: neighbourhood must be a 2-element vector",
                    0, 0, "colfilt", "", "numkit:colfilt:nhood");
    const std::size_t m = static_cast<std::size_t>(nh.elemAsDouble(0));
    const std::size_t n = static_cast<std::size_t>(nh.elemAsDouble(1));

    // Detect optional [mblock nblock] — present iff arg[k+1] is a
    // 2-element numeric vector AND arg[k+2] is a string AND arg[k+3]
    // exists (the function handle).
    std::size_t bi = k + 1;
    if (!args[bi].isChar() && !args[bi].isString()
        && args[bi].numel() == 2
        && (bi + 2) < args.size()
        && (args[bi + 1].isChar() || args[bi + 1].isString())) {
        // mblock / nblock is purely a memory optimisation per MATLAB
        // docs — ignore it and proceed.
        bi = bi + 1;
    }
    if (bi + 1 >= args.size())
        throw Error("colfilt: missing block_type and/or fun argument",
                    0, 0, "colfilt", "", "numkit:colfilt:nargin");
    if (!args[bi].isChar() && !args[bi].isString())
        throw Error("colfilt: block_type must be 'sliding' or 'distinct'",
                    0, 0, "colfilt", "", "numkit:colfilt:blockType");
    const std::string kind = args[bi].toString();
    const Value &fn = args[bi + 1];
    outs[0] = colfilt(*ctx.engine, args[0], m, n, kind, fn, indexed, mr);
}

void imnlmfilt_reg(Span<const Value> args, std::size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imnlmfilt: requires (I [, NV...])",
                    0, 0, "imnlmfilt", "", "numkit:imnlmfilt:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    double dos = -1.0;
    int swsize = 21;
    int cwsize = 5;
    std::size_t i = 1;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("imnlmfilt: expected NV-pair name",
                        0, 0, "imnlmfilt", "", "numkit:imnlmfilt:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "degreeofsmoothing") {
            dos = args[i + 1].toScalar();
            if (!(dos > 0))
                throw Error("imnlmfilt: DegreeOfSmoothing must be positive",
                            0, 0, "imnlmfilt", "", "numkit:imnlmfilt:dos");
        } else if (nlo == "searchwindowsize") {
            swsize = static_cast<int>(args[i + 1].toScalar());
        } else if (nlo == "comparisonwindowsize") {
            cwsize = static_cast<int>(args[i + 1].toScalar());
        } else {
            throw Error("imnlmfilt: unknown option '" + name + "'",
                        0, 0, "imnlmfilt", "",
                        "numkit:imnlmfilt:unknownNv");
        }
        i += 2;
    }
    Value J;
    double est;
    imnlmfilt(args[0], dos, swsize, cwsize, J, est, mr);
    outs[0] = std::move(J);
    if (nargout >= 2) outs[1] = Value::scalar(est, mr);
}

void imgaborfilt_reg(Span<const Value> args, std::size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("imgaborfilt: requires (A, wavelength, orientation "
                    "[, NV...]) — gabor() bank form not supported",
                    0, 0, "imgaborfilt", "", "numkit:imgaborfilt:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    const Value &A = args[0];
    const double wavelength  = args[1].toScalar();
    const double orientation = args[2].toScalar();
    double sfb = 1.0;
    double aspect = 0.5;

    std::size_t i = 3;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("imgaborfilt: expected NV-pair name",
                        0, 0, "imgaborfilt", "", "numkit:imgaborfilt:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "spatialfrequencybandwidth")
            sfb = args[i + 1].toScalar();
        else if (nlo == "spatialaspectratio")
            aspect = args[i + 1].toScalar();
        else
            throw Error("imgaborfilt: unknown option '" + name + "'",
                        0, 0, "imgaborfilt", "",
                        "numkit:imgaborfilt:unknownNv");
        i += 2;
    }
    Value mag, phase;
    imgaborfilt(A, wavelength, orientation, sfb, aspect, mag, phase, mr);
    outs[0] = std::move(mag);
    if (nargout >= 2) outs[1] = std::move(phase);
}

void imdiffusefilt_reg(Span<const Value> args, std::size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imdiffusefilt: requires (I [, NV...])",
                    0, 0, "imdiffusefilt", "",
                    "numkit:imdiffusefilt:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    Value thresh;
    std::size_t N = 0;
    std::string connectivity = "maximal";
    std::string conduction = "exponential";

    std::size_t i = 1;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("imdiffusefilt: expected NV-pair name",
                        0, 0, "imdiffusefilt", "",
                        "numkit:imdiffusefilt:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "gradientthreshold") {
            thresh = args[i + 1];
        } else if (nlo == "numberofiterations") {
            const double v = args[i + 1].toScalar();
            if (!(v > 0) || v != std::floor(v))
                throw Error("imdiffusefilt: NumberOfIterations must be a "
                            "positive integer",
                            0, 0, "imdiffusefilt", "",
                            "numkit:imdiffusefilt:n");
            N = static_cast<std::size_t>(v);
        } else if (nlo == "connectivity") {
            connectivity = args[i + 1].toString();
            std::string lo;
            for (char ch : connectivity)
                lo += static_cast<char>(std::tolower(
                    static_cast<unsigned char>(ch)));
            connectivity = lo;
        } else if (nlo == "conductionmethod") {
            conduction = args[i + 1].toString();
            std::string lo;
            for (char ch : conduction)
                lo += static_cast<char>(std::tolower(
                    static_cast<unsigned char>(ch)));
            conduction = lo;
        } else {
            throw Error("imdiffusefilt: unknown option '" + name + "'",
                        0, 0, "imdiffusefilt", "",
                        "numkit:imdiffusefilt:unknownNv");
        }
        i += 2;
    }
    outs[0] = imdiffusefilt(args[0], thresh, N, connectivity, conduction, mr);
}

void imguidedfilter_reg(Span<const Value> args, std::size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imguidedfilter: requires (A [, G] [, NV...])",
                    0, 0, "imguidedfilter", "",
                    "numkit:imguidedfilter:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    Value A = args[0];
    Value G;  // empty → self-guide
    int nhood = 5;
    double eps = -1.0;  // sentinel → class-based default

    std::size_t i = 1;
    if (i < args.size() && !is_string(args[i])) {
        G = args[i];
        ++i;
    }
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("imguidedfilter: expected NV-pair name",
                        0, 0, "imguidedfilter", "",
                        "numkit:imguidedfilter:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "neighborhoodsize") {
            const Value &v = args[i + 1];
            if (v.numel() == 1) {
                nhood = static_cast<int>(v.toScalar());
            } else if (v.numel() == 2) {
                const int n0 = static_cast<int>(v.elemAsDouble(0));
                const int n1 = static_cast<int>(v.elemAsDouble(1));
                if (n0 != n1)
                    throw Error("imguidedfilter: non-square NeighborhoodSize "
                                "not yet supported",
                                0, 0, "imguidedfilter", "",
                                "numkit:imguidedfilter:nhoodNonsq");
                nhood = n0;
            } else {
                throw Error("imguidedfilter: NeighborhoodSize must be a "
                            "scalar or 2-element vector",
                            0, 0, "imguidedfilter", "",
                            "numkit:imguidedfilter:nhoodSize");
            }
        } else if (nlo == "degreeofsmoothing") {
            eps = args[i + 1].toScalar();
            if (!(eps > 0) || !std::isfinite(eps))
                throw Error("imguidedfilter: DegreeOfSmoothing must be a "
                            "positive finite scalar",
                            0, 0, "imguidedfilter", "",
                            "numkit:imguidedfilter:dos");
        } else {
            throw Error("imguidedfilter: unknown option '" + name + "'",
                        0, 0, "imguidedfilter", "",
                        "numkit:imguidedfilter:unknownNv");
        }
        i += 2;
    }
    outs[0] = imguidedfilter(A, G, nhood, eps, mr);
}

} // namespace detail

void registerNlfilterCallbackBuiltin(Engine &engine)
{
    engine.registerCallbackBuiltin("nlfilter",
                                   std::make_shared<detail::NlfilterCallbackBuiltin>());
}

} // namespace numkit::image
