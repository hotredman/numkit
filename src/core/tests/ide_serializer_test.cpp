// core/tests/ide_serializer_test.cpp
//
// Guards for the IDE JSON IPC serializers (apps/numkit/ide_serializer.hpp,
// header-only). The WASM twin (wasm/src/repl_bindings.cpp) must stay
// byte-compatible — the format contract lives in the header comment.

#include "dual_engine_fixture.hpp"  // support/ is on the gtest include path

#include <numkit/core/debug_session.hpp>  // ide_serializer.hpp uses DebugSession (main.cpp includes it first)

#include "../../../../apps/numkit/ide_serializer.hpp"

#include <gtest/gtest.h>
#include <string>

using namespace m_test;

class IdeSerializerTest : public DualEngineTest {};

// --- bugs/opened/ide/object-inspection-json-ipc.md ---
// A classdef object must serialize with kind:"object", its class name, and
// its properties visible (like the struct payload — fields + drill), not as
// a matrix with a "[1x1 object]" text placeholder.
TEST_P(IdeSerializerTest, DISABLED_ObjectInspectionPayload)
{
    eval("classdef BoxO\n  properties\n    a = 1\n    b = [1 2 3]\n  end\nend");
    eval("x = BoxO(); x.a = 42;");
    const std::string json = numkit::ide::getInspectPathJSON(engine, "x", "");
    EXPECT_NE(json.find("\"kind\":\"object\""), std::string::npos)
        << "payload: " << json;
    EXPECT_NE(json.find("BoxO"), std::string::npos) << "class name missing: " << json;
    EXPECT_NE(json.find("\"a\""), std::string::npos) << "property list missing: " << json;
    // Drill-in path into a property must resolve (the path walker needs an
    // object-property step).
    const std::string inner = numkit::ide::getInspectPathJSON(engine, "x", "f:a");
    EXPECT_EQ(inner.find("\"error\""), std::string::npos)
        << "property path not navigable: " << inner;
    EXPECT_NE(inner.find("42"), std::string::npos) << "property value: " << inner;
}

INSTANTIATE_DUAL(IdeSerializerTest);
