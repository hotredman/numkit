// tests/gtest/integration/try_first_block_bug_test.cpp
//
// Regression test for the compiler register-allocation overflow bug
// that surfaced while running the audio_cepstral parity spec via
// numkit. Symptom seen by users: "Not a double array" thrown
// from inside a try-block on a perfectly legal index expression like
// `mc(2, 2)` on a previously-assigned matrix.
//
// Root cause (fixed in this commit):
//   The bytecode compiler's `nextReg_` was a uint8_t. A long sequence
//   of statements with many cached LOAD_CONSTs + multi-output
//   assignments + chained index reads bumped allocations past slot 255.
//   Post-increment wrapped 256 → 0, so the next argument-staging MOVE
//   wrote into variable slot 0, AND the call's argBase+1 register was
//   never written (still UNSET from frame init). The subsequent
//   CALL_INDIRECT read R[256] as an unset tag and resolveIndices()
//   called doubleData() which threw "Not a double array".
//
// Two-part fix:
//   1. Bytecode reg fields are uint8_t — hard cap is 256 slots per
//      chunk. nextReg_ is now `int`; tempReg/varRegLookup throw a
//      clear "Compiler: register exhaustion" error instead of silently
//      wrapping. Bug surfaces loudly even in a heavier script.
//   2. compileBlock now releases transient temps between statements:
//      nextReg_ resets to maxVarReg_ after each child of a block, so
//      large blocks no longer accumulate dead temp slots forever.
//      The literal cache (constRegCache_) is also dropped at the
//      boundary because its cached regs may sit in the released range.
//
// The original repro is the audio_cepstral parity spec (mfcc + gtcc +
// cepstralCoefficients), but a more compact, dependency-free body
// reproduces the SAME overflow because the trigger is purely the
// number of statements/temps. We exercise both backends.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class CompilerRegisterOverflowTest : public DualEngineTest {};

// ── A: minimal compact reproducer — many short statements in a
//    single block. Each statement does three matrix index reads with
//    cached literals. Without compileBlock's statement-boundary temp
//    release this used to consume ~3 temps + 1 new var per statement,
//    bumping nextReg_ past 256 and (silently) wrapping uint8_t.
TEST_P(CompilerRegisterOverflowTest, ManyStatementsInBlockNoOverflow)
{
    constexpr int kN = 80;
    std::string body = "m = [11 12 13; 21 22 23; 31 32 33];\n";
    for (int i = 0; i < kN; ++i) {
        body += "v" + std::to_string(i)
             + " = m(2, 2) + m(1, 3) + m(3, 1);\n";
    }
    EXPECT_NO_THROW(eval(body));
    // Each v_i should equal 22 + 13 + 31 = 66.
    EXPECT_EQ(evalScalar("v0"), 66.0);
    EXPECT_EQ(evalScalar("v" + std::to_string(kN - 1)), 66.0);
    EXPECT_EQ(evalScalar("m(2,2)"), 22.0);
}

// ── B: the same intensity, wrapped in a try-block — closer to the
//    audio_cepstral failure mode. Try/catch bookkeeping adds a few
//    extra slots so we keep the body shorter than test A.
TEST_P(CompilerRegisterOverflowTest, ManyStatementsInsideTryBlock)
{
    constexpr int kN = 60;
    std::string body =
        "ok = false; msg = '';\n"
        "try\n"
        "    m = [11 12 13; 21 22 23; 31 32 33];\n";
    for (int i = 0; i < kN; ++i) {
        body += "    v" + std::to_string(i)
             + " = m(2, 2) + m(1, 3) + m(3, 1);\n";
    }
    body +=
        "    ok = true;\n"
        "catch ME\n"
        "    msg = ME.message;\n"
        "end\n";
    EXPECT_NO_THROW(eval(body));
    ASSERT_TRUE(evalBool("ok"))
        << "try-body threw: " << eval("msg").toString();
    EXPECT_EQ(evalScalar("v" + std::to_string(kN - 1)), 66.0);
}

// ── C: forced overflow — a *single* expression with so many distinct
//    sub-expressions that even with statement-boundary release we hit
//    the 256-slot wall mid-statement. The bytecode compiler must
//    surface a clear "register exhaustion" error rather than silently
//    corrupt state. The TreeWalker has no per-chunk register file, so
//    it just evaluates the sum.
TEST_P(CompilerRegisterOverflowTest, OverflowInSingleExpression)
{
    // Build `s = 1 + 2 + 3 + ... + 1000;` — 1000 distinct literals.
    // compileBlock has only ONE statement here so no statement-
    // boundary release can save us; the VM compiler must throw.
    std::string body = "s = 1";
    for (int i = 2; i <= 1000; ++i)
        body += " + " + std::to_string(i);
    body += ";";

    if (GetParam() == BackendParam::VM) {
        try {
            eval(body);
            FAIL() << "expected register-exhaustion compile error, got success";
        } catch (const std::exception &e) {
            const std::string msg = e.what();
            EXPECT_NE(msg.find("register"), std::string::npos)
                << "expected 'register' in error, got: " << msg;
        }
    } else {
        // TreeWalker: no register-allocation, just evaluate.
        EXPECT_NO_THROW(eval(body));
        EXPECT_EQ(evalScalar("s"), 500500.0);
    }
}

// ── D: end-to-end contract test — a variable assigned via the
//    canEliminate adopt-temp path in compileAssign must survive any
//    number of subsequent statements unchanged. Two independent
//    mechanisms enforce this:
//      (a) preImportGlobals pre-allocates a low slot for every
//          assigned name, so adopt-temp almost never fires on normal
//          variables (the slot is already in varRegisters_).
//      (b) When adopt-temp DOES fire (e.g. names preImport doesn't
//          see), it pins the adopted slot in maxVarReg_.
//    This test guards the OBSERVABLE contract; it intentionally does
//    not assert which mechanism handled it. Disabling either one
//    alone keeps this test green, but disabling both regresses it
//    immediately on any non-trivial script. See ManyStatementsInBlock
//    above for the load that would force adopt-temp to matter without
//    pre-allocation.
TEST_P(CompilerRegisterOverflowTest, AdoptTempPinsVariableSlot)
{
    // First-assignment via ADD (eligible for canEliminate adopt-temp).
    // big_arr lands in whatever high temp slot the RHS ended on.
    // Then 30 more first-assignments push nextReg_ up.
    std::string body =
        "base = ones(1, 100) * 7;\n"
        "big_arr = base + 1;\n";  // adopt-temp: big_arr claims the ADD's dst slot
    for (int i = 0; i < 30; ++i)
        body += "x" + std::to_string(i) + " = " + std::to_string(i) + " * 2;\n";
    // Read big_arr last — if its slot got reclaimed and overwritten,
    // size and sum will be wrong.
    body +=
        "sz = numel(big_arr);\n"
        "sm = sum(big_arr);\n";
    EXPECT_NO_THROW(eval(body));
    EXPECT_EQ(evalScalar("sz"), 100.0);   // length preserved
    EXPECT_EQ(evalScalar("sm"), 800.0);   // (7+1)*100 = 800
}

// ── E: targeted regression for bug #4 — compileBlock's release must
//    emit runtime CLEAR_VAR for each released slot, otherwise stale
//    values persist in slots that get reused for new unknown
//    variables, and ASSERT_DEF wrongly passes.
//
//    Trigger: statement that loads a literal into a temp slot S, then
//    a try/catch over a statement that references an UNDEFINED name —
//    if the undef gets allocated to slot S and slot S still holds
//    the literal at runtime, ASSERT_DEF doesn't fire and the catch
//    doesn't run.
TEST_P(CompilerRegisterOverflowTest, ReleasedSlotsClearedAtRuntime)
{
    eval(R"(
        result = 0;
        try
            try
                tmp = undefined_a + undefined_b;
            catch
                tmp = 99;
            end
            % After inner catch's `tmp = 99`, the literal 99 sits in
            % some now-released temp slot. The next undefined reference
            % may reuse that slot — ASSERT_DEF must still throw.
            result = tmp + undefined_c;
        catch
            outer_caught = 1;
            result = result + 1000;
        end
    )");
    // Outer catch MUST fire because `undefined_c` is undefined.
    // `result + undefined_c` throws BEFORE assigning result, so result
    // stays at its pre-statement value of 0; the outer catch then
    // brings it to 0 + 1000 = 1000.
    EXPECT_EQ(evalScalar("outer_caught"), 1.0)
        << "outer catch did not fire — ASSERT_DEF saw a stale value "
           "in the reused slot (bug #4 regressed: compileBlock release "
           "is not emitting CLEAR_VAR for released slots)";
    EXPECT_EQ(evalScalar("result"), 1000.0);
}

INSTANTIATE_TEST_SUITE_P(TW_VM, CompilerRegisterOverflowTest,
                         ::testing::Values(BackendParam::TreeWalker,
                                           BackendParam::VM),
                         backendName);
