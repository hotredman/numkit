// funm_test.cpp — general matrix function funm(A, fun).
//
// funm(A, fun) evaluates a scalar function *of a matrix* (not element-wise):
// F = V * diag(fun(diag(D))) / V where [V, D] = eig(A). Implemented as an
// embedded .m on top of the eig builtin (see linalg_library.cpp). Verified
// vs MATLAB R2025b. Fixes bugs/linalg/funm.md.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class FunmTest : public ::testing::Test {
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Diagonal matrix — closed form: funm(diag(d), fun) = diag(fun(d)).
TEST_F(FunmTest, DiagonalExp) {
    eval("F = funm([2 0; 0 3], @exp);");   // MATLAB: diag(e^2, e^3)
    EXPECT_NEAR(evalScalar("F(1,1)"), 7.38905609893065, 1e-9);
    EXPECT_NEAR(evalScalar("F(2,2)"), 20.0855369231877, 1e-9);
    EXPECT_NEAR(evalScalar("F(1,2)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("F(2,1)"), 0.0, 1e-12);
}

// Non-symmetric matrix, distinct real eigenvalues. MATLAB funm([1 2;3 4],@exp).
TEST_F(FunmTest, NonsymmetricExp) {
    eval("F = funm([1 2; 3 4], @exp);");
    EXPECT_NEAR(evalScalar("F(1,1)"), 51.96895619870775, 1e-7);
    EXPECT_NEAR(evalScalar("F(1,2)"), 74.73656457839479, 1e-7);
    EXPECT_NEAR(evalScalar("F(2,1)"), 112.10484686759219, 1e-7);
    EXPECT_NEAR(evalScalar("F(2,2)"), 164.07380306630003, 1e-7);
}

// Trig matrix functions on the same matrix. MATLAB funm([1 2;3 4],@sin/@cos).
TEST_F(FunmTest, NonsymmetricSinCos) {
    eval("Fs = funm([1 2; 3 4], @sin);");
    EXPECT_NEAR(evalScalar("Fs(1,1)"), -0.465581486313731, 1e-9);
    EXPECT_NEAR(evalScalar("Fs(1,2)"), -0.148424459913177, 1e-9);
    eval("Fc = funm([1 2; 3 4], @cos);");
    EXPECT_NEAR(evalScalar("Fc(1,1)"), 0.855423165077998, 1e-9);
}

// Symmetric matrix — funm(A, @sqrt) must equal sqrtm(A). [2 1;1 2] -> eig {1,3}.
TEST_F(FunmTest, SymmetricSqrtMatchesSqrtm) {
    eval("F = funm([2 1; 1 2], @sqrt);");
    EXPECT_NEAR(evalScalar("F(1,1)"), 1.36602540378444, 1e-12);   // (1+sqrt(3))/2
    EXPECT_NEAR(evalScalar("F(1,2)"), 0.36602540378444, 1e-12);   // (sqrt(3)-1)/2
    eval("G = sqrtm([2 1; 1 2]);");
    EXPECT_LT(evalScalar("max(max(abs(F - G)))"), 1e-12);
}

// funm(A, @exp) must equal expm(A) for a diagonalizable matrix.
TEST_F(FunmTest, ExpMatchesExpm) {
    eval("F = funm([1 2; 3 4], @exp); G = expm([1 2; 3 4]);");
    EXPECT_LT(evalScalar("max(max(abs(F - G)))"), 1e-9);
}

// Real input with real eigenvalues yields a real result (no spurious imag).
TEST_F(FunmTest, RealResultForRealInput) {
    eval("F = funm([2 1; 1 2], @exp);");
    EXPECT_EQ(eval("isreal(F)").toBool(), true);
}

// Matrix with complex eigenvalues: [0 -1; 1 0] -> eig {+i, -i}.
// expm([0 -1; 1 0]) = [cos(1) -sin(1); sin(1) cos(1)].
TEST_F(FunmTest, ComplexEigenvaluesExp) {
    eval("F = funm([0 -1; 1 0], @exp);");
    EXPECT_NEAR(evalScalar("F(1,1)"), std::cos(1.0), 1e-12);
    EXPECT_NEAR(evalScalar("F(1,2)"), -std::sin(1.0), 1e-12);
    EXPECT_NEAR(evalScalar("F(2,1)"), std::sin(1.0), 1e-12);
    EXPECT_NEAR(evalScalar("F(2,2)"), std::cos(1.0), 1e-12);
}
