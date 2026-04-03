// tests/main.cpp — GoogleTest entry point
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    //::testing::GTEST_FLAG(filter) = "TW_VM/KnownIssue*";
    return RUN_ALL_TESTS();
}