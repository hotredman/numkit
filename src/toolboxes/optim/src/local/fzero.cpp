// toolboxes/optim/src/local/fzero.cpp
//
// fzero / fminbnd / fminsearch — the Value-level public API. The numerical
// solver kernels (Brent root / outward bracket / Brent-min / Nelder-Mead) now
// live in ops (numkit::ops::{findBracket,brent,brentMin,nelderMead},
// ops/root_solve.hpp) per LAYERING_TARGET_ARCHITECTURE §2/§3/§9 — these wrappers
// just marshal Value<->double and call them. All three accept a numkit::FnHandle
// objective (no Engine dependency); the engine adapters live in bundle.

#include <numkit/optim/local/fzero.hpp>

#include <numkit/ops/root_solve.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>

#include <cmath>
#include <utility>

namespace numkit::optim {

Value fzero(FnHandle fn, double x0, std::pmr::memory_resource *mr)
{
    if (!std::isfinite(x0))
        throw Error("fzero: x0 must be finite", 0, 0, "fzero", "", "numkit:fzero:badX0");
    auto [a, b] = ops::findBracket(fn, x0, mr);
    if (a == b) return Value::scalar(a, mr);
    if (a > b) std::swap(a, b);
    return Value::scalar(ops::brent(fn, a, b, mr), mr);
}

Value fzero(FnHandle fn, double a, double b, std::pmr::memory_resource *mr)
{
    if (!std::isfinite(a) || !std::isfinite(b) || a >= b)
        throw Error("fzero: interval [a, b] must satisfy a < b and be finite", 0, 0, "fzero", "",
                    "numkit:fzero:badInterval");
    return Value::scalar(ops::brent(fn, a, b, mr), mr);
}

Value fminbnd(FnHandle fn, double lo, double hi, double tol, std::pmr::memory_resource *mr)
{
    if (!std::isfinite(lo) || !std::isfinite(hi) || lo >= hi)
        throw Error("fminbnd: lo < hi must be finite", 0, 0, "fminbnd", "",
                    "numkit:fminbnd:badRange");
    if (!(tol > 0)) tol = 1e-6;
    return Value::scalar(ops::brentMin(fn, lo, hi, tol, mr), mr);
}

Value fminsearch(FnHandle fn, Span<const double> x0, double tol, std::pmr::memory_resource *mr)
{
    const std::size_t n = x0.size();
    if (n == 0)
        throw Error("fminsearch: x0 must be non-empty", 0, 0, "fminsearch", "",
                    "numkit:fminsearch:badX0");
    if (!(tol > 0)) tol = 1e-4;
    ScratchArena scratch(mr);
    auto         best = ops::nelderMead(fn, x0.data(), n, tol, &scratch);
    Value        r    = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < n; ++i) r.doubleDataMut()[i] = best[i];
    return r;
}

} // namespace numkit::optim
