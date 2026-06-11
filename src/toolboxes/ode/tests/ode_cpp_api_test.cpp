// toolboxes/ode/tests/ode_cpp_api_test.cpp
//
// Engine-free C++ API guard. ode45 / ode23 take a numkit::FnHandle RHS and
// integrate WITHOUT an Engine — this is the layering goal: the solver is a
// pure C++ library API usable by an embedder that has no script engine. These
// tests call numkit::ode::ode45 / ode23 directly with a stack C++ lambda; no
// StandardEngine, no eval(). (The MATLAB-facing ode45/ode23 builtins are the
// pausable .m wrappers — exercised separately in ode45_test/ode23_test.)

#include <numkit/ode/solvers.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <memory_resource>

using namespace numkit;

namespace {
constexpr double kPi = 3.14159265358979323846;

Value rowPair(double a, double b)
{
    Value v = Value::matrix(1, 2, ValueType::DOUBLE);
    v.doubleDataMut()[0] = a;
    v.doubleDataMut()[1] = b;
    return v;
}
} // namespace

// dy/dt = -y, y(0) = 1, tspan = [0 1]  ⇒  y(1) = e^-1.  No Engine in sight —
// the RHS is a plain C++ lambda passed as a numkit::FnHandle.
TEST(OdeCppApi, Ode45ScalarExpDecayEngineFree)
{
    auto rhs = [](Span<const Value> args, Span<Value> outs,
                  std::pmr::memory_resource *mr) {
        const double y = args[1].toScalar();   // args = {t, y}
        outs[0] = Value::scalar(-y, mr);
    };
    Value y0 = Value::scalar(1.0);
    auto [t, y] = ode::ode45(rhs, rowPair(0.0, 1.0), y0, Value::Empty);

    ASSERT_GE(y.numel(), 1u);
    EXPECT_NEAR(t.elemAsDouble(t.numel() - 1), 1.0, 1e-12);
    EXPECT_NEAR(y.elemAsDouble(y.numel() - 1), std::exp(-1.0), 1e-3);
}

// Harmonic oscillator: y' = [y2; -y1], y(0) = [1; 0], tspan = [0 pi]
//   ⇒  y(pi) = [cos(pi); -sin(pi)] = [-1; 0].  Vector RHS, still Engine-free.
TEST(OdeCppApi, Ode45VectorHarmonicEngineFree)
{
    auto rhs = [](Span<const Value> args, Span<Value> outs,
                  std::pmr::memory_resource *mr) {
        const Value &y = args[1];
        const double y1 = y.elemAsDouble(0), y2 = y.elemAsDouble(1);
        Value dy = Value::matrix(2, 1, ValueType::DOUBLE, mr);
        dy.doubleDataMut()[0] = y2;
        dy.doubleDataMut()[1] = -y1;
        outs[0] = std::move(dy);
    };
    Value y0 = Value::matrix(2, 1, ValueType::DOUBLE);
    y0.doubleDataMut()[0] = 1.0;
    y0.doubleDataMut()[1] = 0.0;
    auto [t, y] = ode::ode45(rhs, rowPair(0.0, kPi), y0, Value::Empty);

    // y is m×d (d = 2), column-major: y(end, j) = (m-1) + j*m.
    const size_t m = t.numel();
    ASSERT_GE(m, 1u);
    EXPECT_NEAR(y.elemAsDouble((m - 1) + 0 * m), -1.0, 1e-2);  // y1(pi) = cos(pi)
    EXPECT_NEAR(y.elemAsDouble((m - 1) + 1 * m),  0.0, 1e-2);  // y2(pi) = -sin(pi)
}

// ode23 is Engine-free too: dy/dt = -y, y(0) = 1, tspan = [0 2] ⇒ y(2) = e^-2.
TEST(OdeCppApi, Ode23ScalarExpDecayEngineFree)
{
    auto rhs = [](Span<const Value> args, Span<Value> outs,
                  std::pmr::memory_resource *mr) {
        outs[0] = Value::scalar(-args[1].toScalar(), mr);
    };
    Value y0 = Value::scalar(1.0);
    auto [t, y] = ode::ode23(rhs, rowPair(0.0, 2.0), y0, Value::Empty);

    ASSERT_GE(y.numel(), 1u);
    EXPECT_NEAR(y.elemAsDouble(y.numel() - 1), std::exp(-2.0), 5e-3);
}
