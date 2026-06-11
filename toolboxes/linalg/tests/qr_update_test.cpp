// toolboxes/linalg/tests/qr_update_test.cpp
//
// Regression guard for qrupdate / qrinsert / qrdelete.
// All three are pinned via algebraic-identity fingerprints rather
// than literal Q/R entries because the Givens-rotation output is
// unique only up to column sign convention. tools/parity/specs/
// qrupdate.json covers the same identities against MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class QRUpdateTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// qrupdate: A + u*v' reconstructed; Q1 orthogonal; R1 upper-triangular.
TEST_F(QRUpdateTest, RankOneUpdateAlgebraicIdentity)
{
    eval("A = [1 2 3; 4 5 6; 7 8 10; 1 1 1];"
         "[Q, R] = qr(A);"
         "u = [1; 1; 1; 1]; v = [1; 0; 0];"
         "[Q1, R1] = qrupdate(Q, R, u, v);"
         "err = max(max(abs(Q1*R1 - (A + u*v'))));"
         "ortho = max(max(abs(Q1'*Q1 - eye(4))));"
         "upper = max(max(abs(R1 - triu(R1))));");
    EXPECT_LT(evalScalar("err"), 1e-12);
    EXPECT_LT(evalScalar("ortho"), 1e-12);
    EXPECT_LT(evalScalar("upper"), 1e-12);
}

// qrinsert: column inserted at 1-based position k.
TEST_F(QRUpdateTest, ColumnInsertAt2)
{
    eval("A = [1 2 3; 4 5 6; 7 8 10; 1 1 1];"
         "[Q, R] = qr(A);"
         "x = [9; 8; 7; 6];"
         "[Q2, R2] = qrinsert(Q, R, 2, x);"
         "target = [A(:,1) x A(:,2:3)];"
         "err = max(max(abs(Q2*R2 - target)));"
         "ortho = max(max(abs(Q2'*Q2 - eye(4))));"
         "upper = max(max(abs(R2 - triu(R2))));");
    EXPECT_LT(evalScalar("err"), 1e-12);
    EXPECT_LT(evalScalar("ortho"), 1e-12);
    EXPECT_LT(evalScalar("upper"), 1e-12);
}

// qrinsert at the end: insert as new last column.
TEST_F(QRUpdateTest, ColumnInsertAtEnd)
{
    eval("A = magic(4);"
         "[Q, R] = qr(A);"
         "x = (1:4)';"
         "[Q2, R2] = qrinsert(Q, R, 5, x);"
         "err = max(max(abs(Q2*R2 - [A x])));");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

// qrdelete: column k dropped.
TEST_F(QRUpdateTest, ColumnDeleteAt2)
{
    eval("A = [1 2 3; 4 5 6; 7 8 10; 1 1 1];"
         "[Q, R] = qr(A);"
         "[Q3, R3] = qrdelete(Q, R, 2);"
         "target = [A(:,1) A(:,3)];"
         "err = max(max(abs(Q3*R3 - target)));"
         "ortho = max(max(abs(Q3'*Q3 - eye(4))));"
         "upper = max(max(abs(R3 - triu(R3))));");
    EXPECT_LT(evalScalar("err"), 1e-12);
    EXPECT_LT(evalScalar("ortho"), 1e-12);
    EXPECT_LT(evalScalar("upper"), 1e-12);
}

// qrinsert / qrdelete row form: not yet supported in v1 → must throw
// the documented "row form not supported" error.
TEST_F(QRUpdateTest, RowFormNotYetSupportedThrows)
{
    eval("A = [1 2; 3 4]; [Q, R] = qr(A);");
    EXPECT_THROW(eval("qrinsert(Q, R, 1, [5 6], 'row');"), std::exception);
    EXPECT_THROW(eval("qrdelete(Q, R, 1, 'row');"), std::exception);
}
