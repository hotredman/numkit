// core/tests/fusion_registry_test.cpp
//
// Stage 1 of VM element-wise fusion: the generic FusionRule registry + on/off
// gate on Engine. No matcher / opcode yet — this only checks the mechanism:
// rules can be registered and iterated, and fusionEnabled() reflects both the
// flag and whether any rule is present (a bare engine is never "fused").

#include <numkit/core/engine.hpp>
#include <numkit/core/fusion_rule.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <vector>

TEST(FusionRegistryTest, EmptyRegistryDisablesFusion) {
    numkit::Engine e;
    EXPECT_TRUE(e.fusionRules().empty());
    EXPECT_FALSE(e.fusionEnabled());  // no rules → effectively off
}

TEST(FusionRegistryTest, AddRuleAndToggleGate) {
    numkit::Engine e;
    numkit::FusionRule r;
    r.name = "dummy";
    r.match = [](const numkit::ASTNode *)
        -> std::optional<std::vector<const numkit::ASTNode *>> {
        return std::nullopt;
    };
    r.execute = [](const numkit::Value *, std::size_t, numkit::Value &,
                   std::pmr::memory_resource *) { return false; };
    e.addFusionRule(std::move(r));

    EXPECT_EQ(e.fusionRules().size(), 1u);
    EXPECT_STREQ(e.fusionRules()[0].name, "dummy");
    EXPECT_TRUE(e.fusionEnabled());   // default-on flag + a rule present

    e.setFusion(false);
    EXPECT_FALSE(e.fusionEnabled());  // kill switch
    e.setFusion(true);
    EXPECT_TRUE(e.fusionEnabled());
}
