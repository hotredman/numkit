// tests/dual_engine_fixture.hpp
//
// Parameterized test fixture that runs every test on BOTH backends:
//   - TreeWalker (TW)
//   - Bytecode VM
//
// Usage in test files:
//   #include "dual_engine_fixture.hpp"
//   class MyTests : public DualEngineTest {};
//   TEST_P(MyTests, SomeTest) { EXPECT_DOUBLE_EQ(evalScalar("2+3;"), 5.0); }
//   INSTANTIATE_DUAL(MyTests);

#pragma once

#include "MEngine.hpp"
#include "MStdLibrary.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace mlab_test {

using namespace numkit::m;

// ── Backend parameter ────────────────────────────────────────
enum class BackendParam { TreeWalker, VM };

inline std::string backendName(const ::testing::TestParamInfo<BackendParam> &info)
{
    return info.param == BackendParam::TreeWalker ? "TW" : "VM";
}

// ── Base fixture ─────────────────────────────────────────────
class DualEngineTest : public ::testing::TestWithParam<BackendParam>
{
public:
    Engine engine;
    std::string capturedOutput;

    void SetUp() override
    {
        StdLibrary::install(engine);
        capturedOutput.clear();
        engine.setOutputFunc([this](const std::string &s) { capturedOutput += s; });

        // Select backend
        if (GetParam() == BackendParam::TreeWalker)
            engine.setBackend(Engine::Backend::TreeWalker);
        else
            engine.setBackend(Engine::Backend::VM);
    }

    // ── Convenience helpers ──────────────────────────────────

    MValue eval(const std::string &code) { return engine.eval(code); }

    double evalScalar(const std::string &code) { return eval(code).toScalar(); }

    bool evalBool(const std::string &code) { return eval(code).toBool(); }

    std::string evalString(const std::string &code) { return eval(code).toString(); }

    double getVar(const std::string &name)
    {
        auto *v = engine.getVariable(name);
        if (!v)
            throw std::runtime_error("Variable '" + name + "' not found");
        return v->toScalar();
    }

    MValue *getVarPtr(const std::string &name) { return engine.getVariable(name); }

    void expectElem(const MValue &val, size_t i, double expected)
    {
        if (val.isLogical())
            EXPECT_DOUBLE_EQ(static_cast<double>(val.logicalData()[i]), expected) << "at index " << i;
        else
            EXPECT_DOUBLE_EQ(val(i), expected) << "at index " << i;
    }

    void expectElem2D(const MValue &val, size_t r, size_t c, double expected)
    {
        if (val.isLogical())
            EXPECT_DOUBLE_EQ(
                static_cast<double>(val.logicalData()[val.dims().sub2ind(r, c)]), expected)
                << "at (" << r << "," << c << ")";
        else
            EXPECT_DOUBLE_EQ(val(r, c), expected) << "at (" << r << "," << c << ")";
    }

    size_t rows(const MValue &v) { return v.dims().rows(); }
    size_t cols(const MValue &v) { return v.dims().cols(); }

    FigureManager &fm() { return engine.figureManager(); }
    AxesState &ax() { return fm().currentAxes(); }
};

} // namespace mlab_test

// ── Instantiation macro ──────────────────────────────────────
// Use at the bottom of each test file:
//   INSTANTIATE_DUAL(MySuite);
#define INSTANTIATE_DUAL(SuiteName)                                                                \
    INSTANTIATE_TEST_SUITE_P(TW_VM, SuiteName,                                                     \
                             ::testing::Values(mlab_test::BackendParam::TreeWalker,                 \
                                               mlab_test::BackendParam::VM),                       \
                             mlab_test::backendName)
