// toolboxes/linalg/tests/eigs_test.cpp
//
// Unit tests for eigs and svds.

#include <gtest/gtest.h>
#include <numkit/linalg/eigs.hpp>
#include <numkit/value/value.hpp>

using namespace numkit;
using namespace numkit::linalg;

TEST(EigsTest, EigsSubsetValues) {
    // A = [1 0 0; 0 5 0; 0 0 3]
    Value A = Value::matrix(3, 3);
    auto *ad = A.doubleDataMut();
    std::fill(ad, ad + 9, 0.0);
    ad[0 + 0*3] = 1.0; ad[1 + 1*3] = 5.0; ad[2 + 2*3] = 3.0;

    Value ev = eigs_values(A, 2);
    EXPECT_EQ(ev.numel(), 2);

    const double *ed = ev.doubleData();
    // Top 2 eigenvalues by magnitude are 5 and 3
    EXPECT_NEAR(ed[0], 5.0, 1e-12);
    EXPECT_NEAR(ed[1], 3.0, 1e-12);
}

TEST(EigsTest, EigsFullDecomposition) {
    Value A = Value::matrix(3, 3);
    auto *ad = A.doubleDataMut();
    std::fill(ad, ad + 9, 0.0);
    ad[0 + 0*3] = 1.0; ad[1 + 1*3] = 5.0; ad[2 + 2*3] = 3.0;

    auto [Vk, Dk] = eigs(A, 2);
    EXPECT_EQ(Vk.dims().rows(), 3); EXPECT_EQ(Vk.dims().cols(), 2);
    EXPECT_EQ(Dk.dims().rows(), 2); EXPECT_EQ(Dk.dims().cols(), 2);

    const double *dd = Dk.doubleData();
    EXPECT_NEAR(dd[0 + 0*2], 5.0, 1e-12);
    EXPECT_NEAR(dd[1 + 1*2], 3.0, 1e-12);
}

TEST(EigsTest, SvdsSubsetValues) {
    Value A = Value::matrix(3, 3);
    auto *ad = A.doubleDataMut();
    std::fill(ad, ad + 9, 0.0);
    ad[0 + 0*3] = 1.0; ad[1 + 1*3] = 5.0; ad[2 + 2*3] = 3.0;

    Value sv = svds_values(A, 2);
    EXPECT_EQ(sv.numel(), 2);

    const double *sd = sv.doubleData();
    EXPECT_NEAR(sd[0], 5.0, 1e-12);
    EXPECT_NEAR(sd[1], 3.0, 1e-12);
}
