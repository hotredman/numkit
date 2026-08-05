// toolboxes/linalg/tests/funm_parlett_test.cpp
//
// Unit tests for funm (Schur-Parlett general matrix functions).

#include <gtest/gtest.h>
#include <numkit/linalg/matrix_functions.hpp>
#include <numkit/value/value.hpp>

#include <cmath>

using namespace numkit;
using namespace numkit::linalg;

TEST(FunmParlettTest, FunmJordanBlockDefective) {
    // J = [1 1; 0 1] (defective 2x2 matrix with confluent eigenvalue 1)
    Value J = Value::matrix(2, 2);
    auto *jd = J.doubleDataMut();
    jd[0] = 1.0; jd[1] = 0.0; jd[2] = 1.0; jd[3] = 1.0;

    // f(x) = sin(x) => f(J) = [sin(1) cos(1); 0 sin(1)]
    Value res = funm(J, "sin");
    EXPECT_EQ(res.dims().rows(), 2);
    EXPECT_EQ(res.dims().cols(), 2);

    const double *rd = res.doubleData();
    EXPECT_NEAR(rd[0], std::sin(1.0), 1e-4); // (0, 0)
    EXPECT_NEAR(rd[1], 0.0, 1e-4);          // (1, 0)
    EXPECT_NEAR(rd[2], std::cos(1.0), 1e-4); // (0, 1)
    EXPECT_NEAR(rd[3], std::sin(1.0), 1e-4); // (1, 1)
}

TEST(FunmParlettTest, FunmExpMatchesExpm) {
    // A = [1 2; 3 4]
    Value A = Value::matrix(2, 2);
    auto *ad = A.doubleDataMut();
    ad[0] = 1.0; ad[1] = 3.0; ad[2] = 2.0; ad[3] = 4.0;

    Value resFunm = funm(A, "exp");
    Value resExpm = expm(A);

    const double *fmd = resFunm.doubleData();
    const double *exd = resExpm.doubleData();

    for (size_t i = 0; i < 4; ++i) {
        EXPECT_NEAR(fmd[i], exd[i], 1e-4);
    }
}
