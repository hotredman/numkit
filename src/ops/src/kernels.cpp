// numkit/ops/kernels.cpp
//
// The public kernel facade (numkit/ops/kernels.hpp) — thin forwarders to the
// backend-split detail loops. The facade itself is allowed to reach into
// detail; its callers are not. Keeping the forwarders in one base TU (always
// compiled, no SIMD specialisation of its own) means the public symbol resolves
// to whichever backend (Highway / portable) was linked, with no drift.
#include <numkit/ops/kernels.hpp>

#include <numkit/ops/binary_ops.hpp>  // detail::matmulDoubleLoop

namespace numkit::ops {

void matmulDouble(const double *a, const double *b, double *c,
                  std::size_t M, std::size_t N, std::size_t K)
{
    detail::matmulDoubleLoop(a, b, c, M, N, K);
}

} // namespace numkit::ops
