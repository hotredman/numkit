// tests/gtest/integration/compiler_const_leak_test.cpp
//
// Ensures that skipped fallback blocks in FUSE_EWISE and the short-circuit
// && / || operators do not leak their `LOAD_CONST` register allocations into 
// the rest of the compilation unit.

#include "dual_engine_fixture.hpp"

using namespace m_test;
using namespace numkit;

class CompilerConstLeakTest : public DualEngineTest {};

TEST_P(CompilerConstLeakTest, FusionFallbackDoesNotLeakConstants)
{
    // The FUSE_EWISE compilation for (1-X).^2 matches sq_affine.
    // It creates a fallback sequence `fbDst = (1-X) .^ 2` where `2` is loaded
    // as a constant inside the fallback block.
    // Since X is a matrix, the fusion succeeds at runtime and skips the fallback.
    // This test ensures that the `2` loaded in the fallback does not pollute
    // the constant register cache, which would cause the subsequent `-X.^2` to
    // read an uninitialized register.
    
    eval("X = [1, 2; 3, 4];");
    // If the bug is present, the `-X.^2` part will see `b=[0x0]` and throw
    // "Matrix dimensions must agree".
    eval("Z = (1 - X).^2 + (-X.^2);");
    
    // Z = (-X).^2 + (-X.^2) = X.^2 - X.^2 = 0? No, (1-X).^2 - X.^2 = 1 - 2X.
    // Let's just verify it didn't throw and computed the correct answer.
    // X = [1, 2; 3, 4]
    // (1-X) = [0, -1; -2, -3]
    // (1-X).^2 = [0, 1; 4, 9]
    // -X.^2 = [-1, -4; -9, -16]
    // Z = [-1, -3; -5, -7]
    EXPECT_DOUBLE_EQ(evalScalar("Z(1,1);"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("Z(1,2);"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("Z(2,1);"), -5.0);
    EXPECT_DOUBLE_EQ(evalScalar("Z(2,2);"), -7.0);
}

TEST_P(CompilerConstLeakTest, ShortCircuitAndDoesNotLeakConstants)
{
    // && and || also save/restore the constant register cache.
    // We test that a constant defined in the skipped right-hand side of && 
    // does not leak and cause an uninitialized register error.
    
    // X is false, so `(1 == 2)` (the RHS) is skipped.
    // Inside the RHS, the constant `2` is loaded.
    // Then outside the &&, we use `2`.
    eval("X = false;");
    eval("Y = (X && (1 == 2)) + 2;");
    
    // Y should be 0 + 2 = 2. If `2` leaked, it would be uninitialized (0).
    EXPECT_DOUBLE_EQ(evalScalar("Y;"), 2.0);
}

TEST_P(CompilerConstLeakTest, ShortCircuitOrDoesNotLeakConstants)
{
    // X is true, so the RHS `(1 == 3)` is skipped.
    // Constant `3` is inside the RHS.
    eval("X = true;");
    eval("Y = (X || (1 == 3)) + 3;");
    
    // Y should be 1 + 3 = 4.
    EXPECT_DOUBLE_EQ(evalScalar("Y;"), 4.0);
}

INSTANTIATE_DUAL(CompilerConstLeakTest);
