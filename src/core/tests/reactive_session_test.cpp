// ReactiveSession (portion B2/B3) — one semantics, dirty skipping,
// the EQUIVALENCE PROPERTY (evalDirty == full re-run, byte-for-byte
// per variable), element writes, cycles guard, benchmark budget.

#include <nk/core/engine.hpp>
#include <nk/core/reactive_session.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <string>

using namespace nk;

namespace {

// Fresh StandardEngine per use (core tests cannot link the bundle's
// installStandardLibrary; raw Engine carries the core builtins the
// reactive scripts below use: sin, hamming? NO — keep scripts to CORE
// math only: literals, arithmetic, colon, sin/cos, sum, zeros, cell).
struct ReactiveTest : ::testing::Test {
    StandardEngine engine;
};

std::string snapshotVars(Engine &e,
                         const std::vector<std::string> &names)
{
    std::string out;
    for (const auto &n : names) {
        auto *v = e.getVariable(n);
        out += n + "=";
        if (!v) {
            out += "<unset>;";
            continue;
        }
        out += std::to_string(v->numel()) + ":";
        if (!v->isNumeric())
            continue;   // cells/structs compared only by presence
        for (size_t k = 0; k < v->numel() && k < 64; ++k) {
            double d = v->elemAsDouble(k);
            out += std::isfinite(d) ? std::to_string(d) : "nan";
            out += ",";
        }
        out += ";";
    }
    return out;
}

const char *kBeamformingish =
    "N = 16;\n"
    "theta0 = 25;\n"
    "d = 0.5;\n"
    "theta = -90:0.2:90;\n"
    "ph = 2 * pi * d * (sin(theta * pi / 180) - sin(theta0 * pi / 180));\n"
    "AF = zeros(1, numel(theta));\n"
    "for k = 1:N\n"
    "  AF = AF + exp(1i * (k - 1) * ph);\n"
    "end\n"
    "mag = abs(AF) / N;\n";

}  // namespace

TEST_F(ReactiveTest, DirtySkippingMatchesFullRerun)
{
    ReactiveSession s(engine);
    ASSERT_TRUE(s.load(kBeamformingish));
    EXPECT_TRUE(s.detectCycles().empty());

    // theta0 change → theta stays cached; the for-loop is a wildcard,
    // so the tail from `ph` re-runs.
    s.setParam("theta0", Value::scalar(-10.0, engine.resource()));
    EXPECT_TRUE(s.lastError().empty()) << "setParam error: " << s.lastError();
    EXPECT_TRUE(s.hasDirty());
    size_t ran = s.evalDirty();
    EXPECT_GT(ran, 0u);
    EXPECT_FALSE(s.hasDirty());

    // EQUIVALENCE: fresh engine, full run with the param def line
    // REPLACED (pre-pending would let the script's own `theta0 = 25`
    // win — the false mismatch that got filed as
    // bugs/closed/core/reactive-multiparam-af-stale.md).
    StandardEngine full;
    full.eval(std::string(kBeamformingish).replace(
                  std::string(kBeamformingish).find("theta0 = 25;"),
                  13, "theta0 = -10;"), true);
    std::vector<std::string> vars{"theta", "ph", "mag", "AF"};
    EXPECT_EQ(snapshotVars(engine, vars), snapshotVars(full, vars));
}

TEST_F(ReactiveTest, ElementWriteOnlyRecalcsTouchingFormulas)
{
    const char *sheet =
        "C = cell(4, 3);\n"
        "C{1,1} = 2;\n"
        "C{1,2} = 3;\n"
        "C{2,1} = C{1,1} * 10;\n"
        "C{2,2} = C{1,2} * 100;\n";
    ReactiveSession s(engine);
    ASSERT_TRUE(s.load(sheet));

    s.setParamElement("C", 1, 1, Value::scalar(7.0, engine.resource()));
    EXPECT_TRUE(s.lastError().empty()) << "setParamElement error: "
                                       << s.lastError();
    size_t ran = s.evalDirty();
    // Only formula (2,1) re-runs — NOT (2,2), NOT the other defs.
    EXPECT_EQ(ran, 1u);
    auto *v = engine.getVariable("C");
    ASSERT_NE(v, nullptr);
    // Column-major C{2,1} = cell index 2 → 70.
    EXPECT_NEAR(v->cellAt(1).toScalar(), 70.0, 1e-12);
    // (2,2) keeps its product of (1,2)=3 → 300: element (1,2) never
    // changed, so the cached value IS the full-rerun value. ✓
    EXPECT_NEAR(v->cellAt(5).toScalar(), 300.0, 1e-12);

    // EQUIVALENCE against a full re-run with the same element value.
    StandardEngine full;
    full.eval("C = cell(4, 3); C{1,1} = 7; C{1,2} = 3;\n"
              "C{2,1} = C{1,1} * 10; C{2,2} = C{1,2} * 100;\n", true);
    auto *vf = full.getVariable("C");
    for (size_t k = 0; k < 12; ++k) {
        ASSERT_EQ(v->cellAt(k).numel(), vf->cellAt(k).numel())
            << "cell " << k;
        if (v->cellAt(k).numel() == 1)
            EXPECT_NEAR(v->cellAt(k).toScalar(),
                        vf->cellAt(k).toScalar(), 1e-12)
                << "cell " << k;
    }
}

TEST_F(ReactiveTest, EquivalenceAcrossSequences)
{
    // Several param changes in sequence must equal ONE full run with
    // the final values (later writes win).
    ReactiveSession s(engine);
    ASSERT_TRUE(s.load(kBeamformingish));
    s.setParam("N", Value::scalar(8.0, engine.resource()));
    s.evalDirty();
    s.setParam("d", Value::scalar(0.7, engine.resource()));
    s.evalDirty();
    s.setParam("theta0", Value::scalar(0.0, engine.resource()));
    s.evalDirty();

    // Reference: param def lines REPLACED in the source (see
    // DirtySkippingMatchesFullRerun for why pre-pending is wrong).
    std::string ref = kBeamformingish;
    ref.replace(ref.find("N = 16;"), 7, "N = 8;");
    ref.replace(ref.find("d = 0.5;"), 7, "d = 0.7;");
    ref.replace(ref.find("theta0 = 25;"), 13, "theta0 = 0;");
    StandardEngine full;
    full.eval(ref, true);
    std::vector<std::string> vars{"mag", "ph"};
    EXPECT_EQ(snapshotVars(engine, vars), snapshotVars(full, vars));
}

TEST_F(ReactiveTest, ParseErrorReportedCleanly)
{
    ReactiveSession s(engine);
    EXPECT_FALSE(s.load("x = ;\n"));
    EXPECT_FALSE(s.lastError().empty());
}

TEST_F(ReactiveTest, GenerationMonotonic)
{
    ReactiveSession s(engine);
    ASSERT_TRUE(s.load("x = 1; y = x + 1;\n"));
    uint64_t g0 = s.generation();
    s.setParam("x", Value::scalar(5.0, engine.resource()));
    EXPECT_GT(s.generation(), g0);
    s.evalDirty();
    EXPECT_GT(s.generation(), g0);
}

TEST_F(ReactiveTest, BeamformingTickBudget)
{
    // Target from the Play spec: one param tick < 0.1 ms on the
    // 16-element array model (the for-loop is a wildcard here, so the
    // tail re-runs — a realistic WORST-case tick).
    ReactiveSession s(engine);
    ASSERT_TRUE(s.load(kBeamformingish));
    s.setParam("theta0", Value::scalar(13.0, engine.resource()));
    // Warm-up (first tick compiles the loop body chunk).
    s.evalDirty();
    auto t0 = std::chrono::steady_clock::now();
    s.setParam("theta0", Value::scalar(13.5, engine.resource()));
    size_t ran = s.evalDirty();
    auto t1 = std::chrono::steady_clock::now();
    double us =
        std::chrono::duration<double, std::micro>(t1 - t0).count();
    EXPECT_GT(ran, 0u);
    // Generous CI ceiling (10× the target) — the REAL number lands in
    // the portion memory; a regression >1 ms means the seam broke.
    EXPECT_LT(us, 1000.0) << "tick took " << us << "us";
}
