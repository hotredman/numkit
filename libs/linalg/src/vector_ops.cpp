// libs/linalg/src/vector_ops.cpp
//
// cross / dot / kron — implementations and engine adapters.
// Migrated from libs/builtin/src/language/arrays/matrix.cpp.

#include <numkit/linalg/vector_ops.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/span.hpp>
#include <numkit/core/types.hpp>

namespace numkit::linalg {

Value cross(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    // MATLAB: cross(A, B) operates along the first dimension with
    // size 3. Common shapes: 1x3, 3x1, 3xN, Nx3. The result has the
    // same shape as the inputs. See BUGS.md #18.
    const auto &da = a.dims();
    const auto &db = b.dims();
    if (da.rows() != db.rows() || da.cols() != db.cols())
        throw Error("cross: A and B must have the same shape",
                     0, 0, "cross", "", "numkit:cross:shapeMismatch");

    const size_t nr = da.rows();
    const size_t nc = da.cols();

    // Pick the dimension to cross along: first one of size 3.
    int crossDim;
    if (nr == 3)      crossDim = 0; // cross along rows (each column is a 3-vec)
    else if (nc == 3) crossDim = 1; // cross along cols (each row is a 3-vec)
    else
        throw Error("cross: A and B must have at least one dimension of length 3",
                     0, 0, "cross", "", "numkit:cross:badSize");

    auto out = Value::matrix(nr, nc, ValueType::DOUBLE, mr);
    const double *ad = a.doubleData();
    const double *bd = b.doubleData();
    double *od = out.doubleDataMut();

    if (crossDim == 0) {
        // 3xN: column-major storage, so col c starts at c*3.
        const size_t batches = nc;
        for (size_t c = 0; c < batches; ++c) {
            const size_t base = c * 3;
            const double a0 = ad[base], a1 = ad[base + 1], a2 = ad[base + 2];
            const double b0 = bd[base], b1 = bd[base + 1], b2 = bd[base + 2];
            od[base    ] = a1 * b2 - a2 * b1;
            od[base + 1] = a2 * b0 - a0 * b2;
            od[base + 2] = a0 * b1 - a1 * b0;
        }
    } else {
        // Nx3: column-major, so element (r, k) at r + k*nr.
        const size_t batches = nr;
        for (size_t r = 0; r < batches; ++r) {
            const double a0 = ad[r], a1 = ad[r + nr], a2 = ad[r + 2 * nr];
            const double b0 = bd[r], b1 = bd[r + nr], b2 = bd[r + 2 * nr];
            od[r           ] = a1 * b2 - a2 * b1;
            od[r +     nr  ] = a2 * b0 - a0 * b2;
            od[r + 2 * nr  ] = a0 * b1 - a1 * b0;
        }
    }
    return out;
}

Value dot(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    if (a.numel() != b.numel())
        throw Error("dot: vectors must have same length",
                     0, 0, "dot", "", "numkit:dot:lengthMismatch");
    double s = 0;
    for (size_t i = 0; i < a.numel(); ++i)
        s += a.doubleData()[i] * b.doubleData()[i];
    return Value::scalar(s, mr);
}

Value kron(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    if (a.type() == ValueType::COMPLEX || b.type() == ValueType::COMPLEX)
        throw Error("kron: complex inputs are not supported",
                     0, 0, "kron", "", "numkit:kron:complex");
    if (a.dims().is3D() || a.dims().ndim() > 2
        || b.dims().is3D() || b.dims().ndim() > 2)
        throw Error("kron: inputs must be 2D",
                     0, 0, "kron", "", "numkit:kron:rank");

    const size_t rA = a.dims().rows(), cA = a.dims().cols();
    const size_t rB = b.dims().rows(), cB = b.dims().cols();
    const size_t rOut = rA * rB, cOut = cA * cB;

    auto out = Value::matrix(rOut, cOut, ValueType::DOUBLE, mr);
    if (rOut == 0 || cOut == 0) return out;

    double *dst = out.doubleDataMut();
    for (size_t ja = 0; ja < cA; ++ja)
        for (size_t ia = 0; ia < rA; ++ia) {
            const double av = a.elemAsDouble(ia + ja * rA);
            for (size_t jb = 0; jb < cB; ++jb) {
                const size_t jOut = ja * cB + jb;
                for (size_t ib = 0; ib < rB; ++ib) {
                    const size_t iOut = ia * rB + ib;
                    const double bv = b.elemAsDouble(ib + jb * rB);
                    dst[jOut * rOut + iOut] = av * bv;
                }
            }
        }
    return out;
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters — registered in LinalgLibrary::install
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void cross_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cross: requires 2 arguments",
                     0, 0, "cross", "", "numkit:cross:nargin");
    outs[0] = cross(args[0], args[1], ctx.engine->resource());
}

void dot_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dot: requires 2 arguments",
                     0, 0, "dot", "", "numkit:dot:nargin");
    outs[0] = dot(args[0], args[1], ctx.engine->resource());
}

void kron_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("kron: requires 2 arguments",
                     0, 0, "kron", "", "numkit:kron:nargin");
    outs[0] = kron(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::linalg
