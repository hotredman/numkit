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

} // namespace numkit::image
