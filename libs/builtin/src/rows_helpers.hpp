// libs/builtin/src/rows_helpers.hpp
//
// Shared helpers for "treat each row as a tuple" operations
// (unique 'rows', sortrows, future ismember 'rows' etc.).
// All paths assume a column-major DOUBLE matrix.

#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

#include <cmath>
#include <cstddef>

namespace numkit::builtin::detail {

// Lex-compare rows `a` and `b` of a column-major DOUBLE buffer.
// Returns -1 / 0 / +1 in standard cmp convention. NaN is treated as
// strictly greater than any non-NaN (push to end on ascending sort) —
// matches MATLAB sort/sortrows ordering for finite vs NaN entries.
inline int rowLexCmp(const double *p, std::size_t cols, std::size_t rows,
                     std::size_t a, std::size_t b)
{
    for (std::size_t c = 0; c < cols; ++c) {
        const double av = p[c * rows + a];
        const double bv = p[c * rows + b];
        const bool aNan = std::isnan(av);
        const bool bNan = std::isnan(bv);
        if (aNan && bNan) continue;
        if (aNan) return  1;
        if (bNan) return -1;
        if (av < bv) return -1;
        if (av > bv) return  1;
    }
    return 0;
}

// Lex-compare rows by an explicit column priority list. Each entry of
// `cols1based` is the 1-based column index to compare on; a negative
// entry flips the direction for that key (descending). Out-of-range
// entries throw via the caller's pre-validation (this function trusts
// the caller). Pointer + size so the same helper composes with
// std::vector / std::pmr::vector / arrays.
inline int rowLexCmpByCols(const double *p, std::size_t totalCols,
                           std::size_t rows, std::size_t a, std::size_t b,
                           const int *cols1based, std::size_t nCols)
{
    for (std::size_t k = 0; k < nCols; ++k) {
        const int rawCol = cols1based[k];
        const bool desc = rawCol < 0;
        const std::size_t cIdx = static_cast<std::size_t>(desc ? -rawCol : rawCol) - 1;
        (void) totalCols;  // caller validated rawCol in range
        const double av = p[cIdx * rows + a];
        const double bv = p[cIdx * rows + b];
        const bool aNan = std::isnan(av);
        const bool bNan = std::isnan(bv);
        if (aNan && bNan) continue;
        // NaN goes to the end on ascending; flip on descending.
        if (aNan) return desc ? -1 :  1;
        if (bNan) return desc ?  1 : -1;
        if (av < bv) return desc ?  1 : -1;
        if (av > bv) return desc ? -1 :  1;
    }
    return 0;
}

// Gather the rows of `x` indicated by `origRows` into a new DOUBLE matrix
// of shape (nRows, x.dims().cols()), preserving column-major layout.
// `x` must be a 2D DOUBLE matrix. Pointer + size so any contiguous
// row-index container (std::vector / pmr::vector / array) works.
inline Value collectRowsByIndex(Allocator &alloc, const Value &x,
                                 const std::size_t *origRows, std::size_t nRows)
{
    const std::size_t rows = x.dims().rows();
    const std::size_t cols = x.dims().cols();
    auto r = Value::matrix(nRows, cols, ValueType::DOUBLE, &alloc);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    for (std::size_t newRow = 0; newRow < nRows; ++newRow) {
        const std::size_t srcRow = origRows[newRow];
        for (std::size_t c = 0; c < cols; ++c)
            dst[c * nRows + newRow] = src[c * rows + srcRow];
    }
    return r;
}

} // namespace numkit::builtin::detail
