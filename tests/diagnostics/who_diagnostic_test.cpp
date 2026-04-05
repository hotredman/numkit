// tests/test_who_diagnostic.cpp
#include "dual_engine_fixture.hpp"

using namespace mlab_test;
using namespace mlab;

class WhoDiagnostic : public DualEngineTest
{};

TEST_P(WhoDiagnostic, WhoSeesVarsFromPreviousEval)
{
    eval("diag_x = 42;");
    eval("diag_y = 7;");
    capturedOutput.clear();
    eval("who");

    std::cerr << "DIAGNOSTIC WHO-1 [" << (GetParam() == BackendParam::VM ? "VM" : "TW")
              << "]: output = '" << capturedOutput << "'\n";

    EXPECT_TRUE(capturedOutput.find("diag_x") != std::string::npos)
        << "who should show diag_x from previous eval";
    EXPECT_TRUE(capturedOutput.find("diag_y") != std::string::npos)
        << "who should show diag_y from previous eval";
}

TEST_P(WhoDiagnostic, WhoSeesVarsAfterError)
{
    try {
        eval("err_a = 100;\nerr_b = 200;\nv = [1 2 3];\nv(999);\n");
    } catch (...) {}

    capturedOutput.clear();
    eval("who");

    std::cerr << "DIAGNOSTIC WHO-2 [" << (GetParam() == BackendParam::VM ? "VM" : "TW")
              << "]: output = '" << capturedOutput << "'\n";

    EXPECT_TRUE(capturedOutput.find("err_a") != std::string::npos)
        << "who should show err_a after error";
    EXPECT_TRUE(capturedOutput.find("err_b") != std::string::npos)
        << "who should show err_b after error";
}

INSTANTIATE_DUAL(WhoDiagnostic);