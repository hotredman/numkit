// tests/main.cpp — GoogleTest entry point
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest();
    //::testing::GTEST_FLAG(filter) = "TW_VM/DynamicFieldTest.DynFieldTwoFields/TW";
    //::testing::GTEST_FLAG(filter) = "EngineAdvancedTest.*";
    //::testing::GTEST_FLAG(filter) = "TW_VM/Array3DTest.*:TW_VM/CellIndexTest.Cell3D*";
    return RUN_ALL_TESTS();
}