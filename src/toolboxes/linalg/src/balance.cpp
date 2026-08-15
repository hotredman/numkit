// toolboxes/linalg/src/balance.cpp
//
// MATLAB balance: diagonal-similarity scaling and permutation for eigenvalue computations.
// Parlett-Reinsch (1969) algorithm; same as EISPACK `balanc`.

#include <numkit/linalg/balance.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace numkit::linalg {

namespace {

// Parlett-Reinsch permutation phase: isolate rows/columns with zero off-diagonal entries
void balancePermute(double *B, size_t n, size_t &low, size_t &high, double *p)
{
    for (size_t i = 0; i < n; ++i) p[i] = static_cast<double>(i + 1);
    if (n <= 1) { low = 0; high = (n == 0) ? 0 : n - 1; return; }

    low = 0;
    high = n - 1;

    bool found = true;
    while (found) {
        found = false;

        // Search for isolated rows in [low, high]
        for (std::intptr_t i = static_cast<std::intptr_t>(high); i >= static_cast<std::intptr_t>(low); --i) {
            size_t ui = static_cast<size_t>(i);
            bool isolated = true;
            for (size_t j = low; j <= high; ++j) {
                if (j == ui) continue;
                if (B[ui + j * n] != 0.0) { isolated = false; break; }
            }
            if (isolated) {
                p[high] = static_cast<double>(ui + 1);
                if (ui != high) {
                    for (size_t k = 0; k < n; ++k) std::swap(B[ui + k * n], B[high + k * n]);
                    for (size_t k = 0; k < n; ++k) std::swap(B[k + ui * n], B[k + high * n]);
                }
                if (high == 0) break;
                high--;
                found = true;
                break;
            }
        }
        if (found) continue;

        // Search for isolated columns in [low, high]
        for (size_t j = low; j <= high; ++j) {
            bool isolated = true;
            for (size_t i = low; i <= high; ++i) {
                if (i == j) continue;
                if (B[i + j * n] != 0.0) { isolated = false; break; }
            }
            if (isolated) {
                p[low] = static_cast<double>(j + 1);
                if (j != low) {
                    for (size_t k = 0; k < n; ++k) std::swap(B[j + k * n], B[low + k * n]);
                    for (size_t k = 0; k < n; ++k) std::swap(B[k + j * n], B[k + low * n]);
                }
                low++;
                found = true;
                break;
            }
        }
    }
}

// Parlett-Reinsch scaling phase on submatrix B[low..high, low..high]
void balanceScale(double *B, size_t n, double *d, size_t low, size_t high)
{
    constexpr double radix = 2.0;
    constexpr double sqrdx = radix * radix;
    constexpr double threshold = 0.95;

    for (size_t i = 0; i < n; ++i) d[i] = 1.0;
    if (n <= 1 || low >= high) return;

    bool done = false;
    while (!done) {
        done = true;
        for (size_t i = low; i <= high; ++i) {
            double r = 0.0;
            double c = 0.0;
            for (size_t j = low; j <= high; ++j) {
                if (j == i) continue;
                r += std::abs(B[i + j * n]);
                c += std::abs(B[j + i * n]);
            }
            if (r == 0.0 || c == 0.0) continue;

            double g = r / radix;
            double f = 1.0;
            const double s = c + r;
            while (c < g) {
                f *= radix;
                c *= sqrdx;
            }
            g = r * radix;
            while (c >= g) {
                f /= radix;
                c /= sqrdx;
            }
            if ((c + r) / f < threshold * s) {
                done = false;
                d[i] *= f;
                for (size_t j = low; j <= high; ++j) B[i + j * n] /= f;
                for (size_t j = low; j <= high; ++j) B[j + i * n] *= f;
            }
        }
    }
}

} // namespace

BalanceResult
balance_impl(const Value &A, bool noperm, std::pmr::memory_resource *mr)
{
    if (A.dims().is3D())
        throw Error("balance: input must be 2D",
                    0, 0, "balance", "", "numkit:balance:Not2D");
    const size_t n = A.dims().rows();
    if (A.dims().cols() != n)
        throw Error("balance: matrix must be square",
                    0, 0, "balance", "", "numkit:balance:NotSquare");
    if (A.isComplex())
        throw Error("balance: complex input not supported in v1",
                    0, 0, "balance", "", "numkit:balance:NoComplex");

    BalanceResult R;
    R.B = Value::matrix(n, n, ValueType::DOUBLE, mr);
    R.d_col = Value::matrix(n == 0 ? 0 : n, n == 0 ? 0 : 1, ValueType::DOUBLE, mr);
    R.perm_col = Value::matrix(n == 0 ? 0 : n, n == 0 ? 0 : 1, ValueType::DOUBLE, mr);

    if (n == 0) return R;

    std::copy(A.doubleData(), A.doubleData() + n * n, R.B.doubleDataMut());

    ScratchArena scratch(mr);
    ScratchVec<double> d(n, &scratch);

    double *pd = R.perm_col.doubleDataMut();
    size_t low = 0, high = (n > 0) ? n - 1 : 0;

    if (!noperm) {
        balancePermute(R.B.doubleDataMut(), n, low, high, pd);
    } else {
        for (size_t i = 0; i < n; ++i) pd[i] = static_cast<double>(i + 1);
    }

    balanceScale(R.B.doubleDataMut(), n, d.data(), low, high);

    double *dd = R.d_col.doubleDataMut();
    for (size_t i = 0; i < n; ++i) dd[i] = d[i];

    return R;
}

} // namespace numkit::linalg
