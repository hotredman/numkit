// libs/image/include/numkit/image/transform/transform.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::image {

/// dct2(A) — 2-D discrete cosine transform (Type-II, orthonormal). The DCT is
/// separable, so dct2(A) == dct(dct(A).').'  Internally we apply the 1-D
/// signal::dct along columns first, then along rows.
Value dct2(std::pmr::memory_resource *mr, const Value &A);

/// idct2(B) — inverse 2-D DCT.
Value idct2(std::pmr::memory_resource *mr, const Value &B);

/// dctmtx(N) — N×N DCT-II transform matrix D such that D*A applies the
/// 1-D DCT to each column of A. D'*D == eye(N) (orthonormal).
Value dctmtx(std::pmr::memory_resource *mr, double N);

/// integralImage(I) — summed-area table. Output is (M+1)×(N+1) double
/// with a leading zero row and column so that the rectangle sum
/// Σ I[r0..r1, c0..c1] equals
///   J[r1+1, c1+1] - J[r0, c1+1] - J[r1+1, c0] + J[r0, c0].
Value integralImage(std::pmr::memory_resource *mr, const Value &I);

/// integralImage3(V) — 3-D summed-volume table. Output is
/// (M+1)×(N+1)×(P+1) double with leading zero plane / row / column.
Value integralImage3(std::pmr::memory_resource *mr, const Value &V);

/// checkerboard(side, M, N) — 2*M*side × 2*N*side double image with
/// alternating black / white squares; the right half is dimmed to
/// grey (×0.7). Defaults: side=10, M=4, N=4. Matches Octave-image's
/// checkerboard.m.
Value checkerboard(std::pmr::memory_resource *mr,
                   size_t side, size_t M, size_t N);

} // namespace numkit::image
