// toolboxes/ode/src/ode_detail.hpp
//
// Private (src-only) helpers shared by the explicit Runge-Kutta integrators
// ode45 (Dormand-Prince 5(4)) and ode23 (Bogacki-Shampine 3(2)): the options
// struct, its parser, and the FnHandle RHS bridge. These were verbatim-
// duplicated in ode45.cpp and ode23.cpp; factored here (the filter_detail.hpp
// pattern). ODE-domain + Value-coupled — they stay in the ode toolbox, NOT ops.
//
// The method-specific pieces (DOPRI5 / BS23 tableaux, dense-output / Hermite
// interpolants, the stepping loops) stay in their own TUs.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/fn_handle.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <memory_resource>
#include <string>
#include <vector>

namespace numkit::ode {

// Solver options shared by ode45 / ode23. All defaults are common EXCEPT
// `refine` (ode45 → 4, ode23 → 1 per MATLAB R2025b), which the caller supplies
// to read_opts; the field initialiser here is only a placeholder.
struct OdeOpts {
    double              rel_tol      = 1e-3;
    std::vector<double> abs_tol{1e-6};
    double              max_step     = std::numeric_limits<double>::infinity();
    double              initial_step = 0.0;
    int                 refine       = 4;
    bool                norm_control = false;
    bool                stats        = false;
};

// Parse the options struct. `refine_default` is the solver's default Refine
// (ode45 → 4, ode23 → 1); an explicit opts.Refine field overrides it.
inline OdeOpts read_opts(const Value &opts, int refine_default)
{
    OdeOpts o;
    o.refine = refine_default;
    if (opts.isEmpty() || !opts.isStruct()) return o;
    auto get_double = [&](const char *name, double def) -> double {
        if (opts.hasField(name)) {
            const Value &v = opts.field(name);
            if (!v.isEmpty()) return v.toScalar();
        }
        return def;
    };
    o.rel_tol      = get_double("RelTol", 1e-3);
    o.max_step     = get_double("MaxStep", std::numeric_limits<double>::infinity());
    o.initial_step = get_double("InitialStep", 0.0);
    if (opts.hasField("Refine")) {
        const Value &v = opts.field("Refine");
        if (!v.isEmpty()) {
            const double r = v.toScalar();
            o.refine = (r >= 1.0) ? static_cast<int>(r) : 1;
        }
    }
    if (opts.hasField("NormControl")) {
        const Value &v = opts.field("NormControl");
        if (!v.isEmpty()) {
            const std::string s = v.toString();
            o.norm_control = (!s.empty() && (s[0] == 'o' || s[0] == 'O'));
        }
    }
    if (opts.hasField("AbsTol")) {
        const Value &v = opts.field("AbsTol");
        if (!v.isEmpty()) {
            const std::size_t n = v.numel();
            o.abs_tol.resize(n);
            for (std::size_t i = 0; i < n; ++i) o.abs_tol[i] = v.elemAsDouble(i);
        }
    }
    if (opts.hasField("Stats")) {
        const Value &v = opts.field("Stats");
        if (!v.isEmpty()) {
            const std::string s = v.toString();
            o.stats = (!s.empty() && (s[0] == 'o' || s[0] == 'O'));
        }
    }
    return o;
}

// Evaluate the RHS callback at (t, y) -> dy/dt (length d). Engine-free: the RHS
// is a numkit::FnHandle (args = {t, y}, outs[0] = dy/dt). `solver` ("ode45" /
// "ode23") names the caller in the size-mismatch error so each integrator keeps
// its own message + identifier. The per-TU `eval_rhs` wrapper binds it.
inline void callOdeRhs(FnHandle rhs, double t, const std::vector<double> &y,
                       std::vector<double> &dydt, std::pmr::memory_resource *mr,
                       const char *solver)
{
    const std::size_t d = y.size();
    Value tv = Value::scalar(t, mr);
    Value yv = Value::matrix(d, 1, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < d; ++i) yv.doubleDataMut()[i] = y[i];
    std::array<Value, 2> args_buf{tv, yv};
    Value                out_buf;
    rhs(Span<const Value>(args_buf.data(), 2), Span<Value>(&out_buf, 1), mr);
    if (out_buf.numel() != d)
        throw Error(std::string(solver) + ": RHS returned " + std::to_string(out_buf.numel())
                  + " values but expected " + std::to_string(d),
                    0, 0, solver, "", "numkit:" + std::string(solver) + ":badRhsSize");
    dydt.resize(d);
    for (std::size_t i = 0; i < d; ++i) dydt[i] = out_buf.elemAsDouble(i);
}

} // namespace numkit::ode
