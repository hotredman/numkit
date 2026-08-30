// toolboxes/control/tests/care_dare_test.cpp
//
// Algebraic Riccati solvers care / dare (matrix sign-function method).
// bugs/control/care-dare.md. Reference values from MATLAB R2025b.
// Output order matches MATLAB: [X, L, G] (solution, closed-loop
// eigenvalues, gain). One TEST_F per documented branch/output.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CareDareTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// --- care (continuous) -------------------------------------------------

// care(A,B,Q): R defaults to identity. X = sqrt(3) on the diagonal.
TEST_F(CareDareTest, CareDefaultR)
{
    eval("X = care([0 1; 0 0], [0; 1], eye(2));");
    EXPECT_NEAR(evalScalar("X(1,1)"),   1.73205080756888, 1e-9);  // sqrt(3)
    EXPECT_NEAR(evalScalar("X(2,2)"),   1.73205080756888, 1e-9);
    EXPECT_NEAR(evalScalar("X(1,2)"),   1.0,              1e-9);
    EXPECT_NEAR(evalScalar("trace(X)"), 3.46410161513776, 1e-9);
    // X must be symmetric.
    EXPECT_NEAR(evalScalar("max(abs(X(:)-reshape(X.',[],1)))"), 0.0, 1e-12);
}

// care residual: A'X + XA - XB R^-1 B'X + Q == 0 to machine precision.
TEST_F(CareDareTest, CareResidual)
{
    eval("A=[0 1;0 0]; B=[0;1]; Q=eye(2); X=care(A,B,Q);");
    eval("Rr = A'*X + X*A - X*B*(B'*X) + Q;");
    EXPECT_NEAR(evalScalar("max(abs(Rr(:)))"), 0.0, 1e-12);
}

// 3-output form [X,L,G]: L = closed-loop eigenvalues, G = gain R^-1 B'X.
TEST_F(CareDareTest, CareGainAndPoles)
{
    eval("[X,L,G] = care([0 1; 0 0], [0; 1], eye(2));");
    EXPECT_NEAR(evalScalar("G(1)"), 1.0,              1e-9);
    EXPECT_NEAR(evalScalar("G(2)"), 1.73205080756888, 1e-9);
    // closed-loop poles -0.8660 +- 0.5000i (stable: Re < 0)
    EXPECT_NEAR(evalScalar("max(real(L))"), -0.86602540378444, 1e-9);
    EXPECT_NEAR(evalScalar("max(abs(imag(L)))"), 0.5, 1e-9);
}

// Non-default R + nonsymmetric A.
TEST_F(CareDareTest, CareWithR)
{
    eval("X = care([-3 2; 1 1], [0; 1], [1 0; 0 2], 3);");
    EXPECT_NEAR(evalScalar("trace(X)"), 9.92682542, 1e-6);
    eval("A=[-3 2;1 1]; B=[0;1]; Q=[1 0;0 2]; R=3;");
    eval("Rr = A'*X + X*A - X*B*(R\\(B'*X)) + Q;");
    EXPECT_NEAR(evalScalar("max(abs(Rr(:)))"), 0.0, 1e-10);
}

// --- dare (discrete) ---------------------------------------------------

// dare(A,B,Q,R): X = stabilizing solution.
TEST_F(CareDareTest, DareSolution)
{
    eval("X = dare([1 1; 0 1], [0; 1], eye(2), 1);");
    EXPECT_NEAR(evalScalar("X(1,1)"),   2.94712296779058, 1e-7);
    EXPECT_NEAR(evalScalar("trace(X)"), 7.56025722770319, 1e-7);
    EXPECT_NEAR(evalScalar("max(abs(X(:)-reshape(X.',[],1)))"), 0.0, 1e-10);
}

// dare residual: A'XA - X - A'XB(R+B'XB)^-1 B'XA + Q == 0.
TEST_F(CareDareTest, DareResidual)
{
    eval("A=[1 1;0 1]; B=[0;1]; Q=eye(2); R=1; X=dare(A,B,Q,R);");
    eval("Rr = A'*X*A - X - (A'*X*B)*((R+B'*X*B)\\(B'*X*A)) + Q;");
    EXPECT_NEAR(evalScalar("max(abs(Rr(:)))"), 0.0, 1e-10);
}

// dare 3-output [X,L,G]: closed-loop poles strictly inside the unit circle.
TEST_F(CareDareTest, DareGainAndPoles)
{
    eval("[X,L,G] = dare([1 1; 0 1], [0; 1], eye(2), 1);");
    EXPECT_NEAR(evalScalar("G(1)"), 0.42208244044, 1e-7);
    EXPECT_NEAR(evalScalar("G(2)"), 1.24392885394, 1e-7);
    EXPECT_LT(evalScalar("max(abs(L))"), 1.0);   // Schur-stable
    EXPECT_NEAR(evalScalar("max(abs(L))"), 0.42208244044, 1e-7);
}

// Singular A: documented gap — clear error, not a crash.
TEST_F(CareDareTest, DareSingularAThrows)
{
    EXPECT_THROW(eval("dare([0 0; 0 0], [0; 1], eye(2), 1);"), std::exception);
}

// --- lqr / dlqr (wrappers on care / dare) ------------------------------

// lqr returns [K, S, P]: gain, Riccati solution, closed-loop poles.
TEST_F(CareDareTest, LqrGainSolutionPoles)
{
    eval("[K,S,P] = lqr([0 1; 0 0], [0; 1], eye(2), 1);");
    EXPECT_NEAR(evalScalar("K(1)"), 1.0,              1e-6);
    EXPECT_NEAR(evalScalar("K(2)"), 1.73205080756888, 1e-6);
    EXPECT_NEAR(evalScalar("S(1,1)"), 1.73205080756888, 1e-7);   // = care X
    EXPECT_NEAR(evalScalar("max(real(P))"), -0.86602540378444, 1e-7);
}

// dlqr returns the discrete LQR gain via the DARE.
TEST_F(CareDareTest, DlqrGain)
{
    eval("K = dlqr([0.9 0.1; 0 0.8], [0; 1], eye(2), 1);");
    EXPECT_NEAR(evalScalar("sum(K)"), 0.71004388, 1e-6);
}

// --- gram (controllability / observability gramian) --------------------

// gram(sys,'c') solves A*Wc + Wc*A' + B*B' = 0 via lyap.
TEST_F(CareDareTest, GramControllability)
{
    eval("Wc = gram(ss([-1 0; 0 -2], [1; 1], [1 1], 0), 'c');");
    EXPECT_NEAR(evalScalar("Wc(1,1)"), 0.5,       1e-9);
    EXPECT_NEAR(evalScalar("Wc(1,2)"), 1.0/3.0,   1e-9);
    EXPECT_NEAR(evalScalar("Wc(2,2)"), 0.25,      1e-9);
    EXPECT_NEAR(evalScalar("sum(Wc(:))"), 1.41666666666667, 1e-9);
}

// gram(sys,'o') solves A'*Wo + Wo*A + C'*C = 0.
TEST_F(CareDareTest, GramObservability)
{
    eval("Wo = gram(ss([-1 0; 0 -2], [1; 1], [1 1], 0), 'o');");
    EXPECT_NEAR(evalScalar("sum(Wo(:))"), 1.41666666666667, 1e-9);
}

// Unknown gramian type throws.
TEST_F(CareDareTest, GramBadTypeThrows)
{
    EXPECT_THROW(eval("gram(ss([-1 0;0 -2],[1;1],[1 1],0), 'x');"), std::exception);
}
