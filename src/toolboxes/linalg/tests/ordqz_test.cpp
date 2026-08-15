// toolboxes/linalg/tests/ordqz_test.cpp
//
// Unit tests for ordqz (Reorder Generalized Schur Decomposition).

#include <gtest/gtest.h>
#include <numkit/linalg/qz.hpp>
#include <numkit/linalg/ordqz.hpp>
#include <numkit/value/value.hpp>

#include <cmath>

using namespace numkit;
using namespace numkit::linalg;

TEST(OrdqzTest, OrdqzSelectVectorReorder) {
    Value A = Value::matrix(2, 2);
    auto *ad = A.doubleDataMut();
    ad[0] = 4.0; ad[1] = 0.0; ad[2] = 1.0; ad[3] = 2.0;

    Value B = Value::matrix(2, 2);
    auto *bd = B.doubleDataMut();
    bd[0] = 1.0; bd[1] = 0.0; bd[2] = 0.0; bd[3] = 1.0;

    auto [AA, BB, Q, Z] = qz(A, B);

    // Select second eigenvalue to move to top left
    Value select = Value::matrix(1, 2, ValueType::LOGICAL);
    select.logicalDataMut()[0] = 0;
    select.logicalDataMut()[1] = 1;

    auto [AAS, BBS, QS, ZS] = ordqz(AA, BB, Q, Z, select);

    // Verify reconstruction QS * A * ZS == AAS, QS * B * ZS == BBS
    double max_err_a = 0.0;
    double max_err_b = 0.0;
    const double *a = A.doubleData();
    const double *b = B.doubleData();
    const double *aas = AAS.doubleData();
    const double *bbs = BBS.doubleData();
    const double *qs = QS.doubleData();
    const double *zs = ZS.doubleData();

    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            double sa = 0.0;
            double sb = 0.0;
            for (size_t k1 = 0; k1 < 2; ++k1) {
                for (size_t k2 = 0; k2 < 2; ++k2) {
                    sa += qs[i + k1 * 2] * a[k1 + k2 * 2] * zs[k2 + j * 2];
                    sb += qs[i + k1 * 2] * b[k1 + k2 * 2] * zs[k2 + j * 2];
                }
            }
            max_err_a = std::max(max_err_a, std::abs(sa - aas[i + j * 2]));
            max_err_b = std::max(max_err_b, std::abs(sb - bbs[i + j * 2]));
        }
    }
    EXPECT_LT(max_err_a, 1e-10);
    EXPECT_LT(max_err_b, 1e-10);
}
