// libs/linalg/tests/schur_convert_test.cpp
//
// Regression guard for cdf2rdf + rsf2csf. MATLAB R2025b convention
// pinned: DR_block = [[a b]; [-b a]], VR(:, k+1) = +Im(v) for positive
// imag eigenvalue. Spec at tools/parity/specs/schur_convert.json.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SchurConvertTest : public ::testing::Test
{
public:
    Engine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// cdf2rdf on rotation matrix [0 -1; 1 0]: DR_block = [0 1; -1 0],
// reconstruction A == VR * DR * inv(VR) exact.
TEST_F(SchurConvertTest, Cdf2RdfRotationMatrix)
{
    eval("A = [0 -1; 1 0];"
         "V_c = [complex(1,0) complex(1,0); complex(0,-1) complex(0,1)] / sqrt(2);"
         "D_c = [complex(0,1) 0; 0 complex(0,-1)];"
         "[VR, DR] = cdf2rdf(V_c, D_c);"
         "err = max(max(abs(A - VR * DR * inv(VR))));");
    EXPECT_DOUBLE_EQ(evalScalar("DR(1,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("DR(1,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("DR(2,1)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("DR(2,2)"), 0.0);
    EXPECT_LT(evalScalar("err"), 1e-13);
}

// cdf2rdf with all-real eigenvalues — identity transform.
TEST_F(SchurConvertTest, Cdf2RdfRealEigenvaluesPassThrough)
{
    eval("V = [complex(1,0) complex(0,0); complex(0,0) complex(1,0)];"
         "D = [complex(2,0) 0; 0 complex(3,0)];"
         "[VR, DR] = cdf2rdf(V, D);");
    EXPECT_DOUBLE_EQ(evalScalar("DR(1,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("DR(2,2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("DR(1,2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("DR(2,1)"), 0.0);
}

// rsf2csf on real 2×2 Schur block: diagonalises to complex conjugate
// eigenvalues on diagonal of Tc.
TEST_F(SchurConvertTest, Rsf2CsfDiagonalises2x2Block)
{
    eval("T_real = [0.5 -1.5; 1.5 0.5];"
         "U_real = eye(2);"
         "[Uc, Tc] = rsf2csf(U_real, T_real);"
         "re11 = real(Tc(1,1)); im11 = imag(Tc(1,1));"
         "re22 = real(Tc(2,2)); im22 = imag(Tc(2,2));"
         "sub  = abs(Tc(2,1));");
    EXPECT_NEAR(evalScalar("re11"),  0.5, 1e-12);
    EXPECT_NEAR(evalScalar("re22"),  0.5, 1e-12);
    EXPECT_NEAR(std::abs(evalScalar("im11")), 1.5, 1e-12);
    EXPECT_NEAR(std::abs(evalScalar("im22")), 1.5, 1e-12);
    // Conjugate pair: im11 + im22 == 0.
    EXPECT_NEAR(evalScalar("im11") + evalScalar("im22"), 0.0, 1e-12);
    EXPECT_LT(evalScalar("sub"), 1e-12);
}

// rsf2csf on an already-upper-triangular real T: no-op (pass through
// as complex).
TEST_F(SchurConvertTest, Rsf2CsfRealUpperTriangularPassThrough)
{
    eval("T = [2 1 0.5; 0 3 -1; 0 0 4];"
         "U = eye(3);"
         "[Uc, Tc] = rsf2csf(U, T);");
    EXPECT_DOUBLE_EQ(evalScalar("real(Tc(1,1))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("real(Tc(2,2))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("real(Tc(3,3))"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(Tc(1,1))"), 0.0);
}
