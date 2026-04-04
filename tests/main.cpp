// tests/main.cpp — GoogleTest entry point
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest();
    //::testing::GTEST_FLAG(filter) = "TW_VM/DebugPhase234Test.*";
    return RUN_ALL_TESTS();
}