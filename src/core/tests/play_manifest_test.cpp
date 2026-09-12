// Play manifest extraction (portion A) — pure unit tests against the
// REAL gallery manifest format (user sample 2026-09-12; canonical
// marker @play, legacy alias %{#ui from the early prototype).

#include <numkit/core/play_manifest.hpp>

#include <gtest/gtest.h>
#include <sstream>

using namespace numkit;

namespace {

const char *kSample = R"MAT(%{
@play
{
  "title": "Phased array (Beamforming)",
  "groups": [
    {
      "name": "Geometry",
      "controls": [
        { "var": "N", "label": "Elements", "min": 8, "max": 64, "step": 2, "default": 16 },
        { "var": "d", "label": "Spacing", "min": 0.1, "max": 1.5, "step": 0.05, "default": 0.5 }
      ]
    },
    {
      "name": "Beam",
      "controls": [
        { "var": "theta0", "label": "Steer", "min": -60, "max": 60, "default": 25 },
        { "var": "win", "label": "Window", "type": "select", "options": ["rect", "hamming"], "default": "hamming" }
      ]
    }
  ]
}
%}
N = 16;
theta0 = 25;
w = hamming(N);
)MAT";

}  // namespace

TEST(PlayManifest, ExtractsCanonicalMarker)
{
    PlayManifest m = extractPlayManifest(kSample);
    ASSERT_FALSE(m.empty());
    EXPECT_NE(m.rawJson.find("\"title\""), std::string::npos);
    ASSERT_EQ(m.controls.size(), 4u);
    EXPECT_EQ(m.controls[0].var, "N");
    EXPECT_EQ(m.controls[0].defaultRaw, "16");
    EXPECT_EQ(m.controls[1].var, "d");
    EXPECT_EQ(m.controls[1].defaultRaw, "0.5");
    EXPECT_EQ(m.controls[2].var, "theta0");
    EXPECT_EQ(m.controls[2].defaultRaw, "25");
    EXPECT_EQ(m.controls[3].var, "win");
    EXPECT_EQ(m.controls[3].defaultRaw, "\"hamming\"");
}

TEST(PlayManifest, LegacyUiAliasStillWorks)
{
    const char *src = "%{#ui\n{ \"controls\": [ { \"var\": \"x\", \"default\": 1 } ] }\n%}\nx = 1;";
    PlayManifest m = extractPlayManifest(src);
    ASSERT_EQ(m.controls.size(), 1u);
    EXPECT_EQ(m.controls[0].var, "x");
}

TEST(PlayManifest, PlainScriptIsEmpty)
{
    PlayManifest m = extractPlayManifest("x = 1;\nplot(x);\n");
    EXPECT_TRUE(m.empty());
}

TEST(PlayManifest, NonManifestBlockCommentIgnored)
{
    const char *src = "%{\nJust a regular doc block with @playish words.\n%}\nx = 1;";
    PlayManifest m = extractPlayManifest(src);
    EXPECT_TRUE(m.empty());
}

TEST(PlayManifest, MarkerMustBeStandaloneWord)
{
    // `@playx` is NOT a marker.
    const char *src = "%{\n@playx { \"controls\": [] }\n%}\nx = 1;";
    EXPECT_TRUE(extractPlayManifest(src).empty());
}

TEST(PlayManifest, UnterminatedBlockIsEmpty)
{
    const char *src = "%{\n@play\n{ \"controls\": [ { \"var\": \"x\" } ] }\nx = 1;";
    EXPECT_TRUE(extractPlayManifest(src).empty());
}

TEST(PlayManifest, PercentBraceInTextIgnored)
{
    // `%{` inside a string literal (not at line start) is not a block.
    const char *src = "s = 'a%{b';\n%{\n@play\n{\"controls\": [{\"var\":\"z\"}]}\n%}\nz=1;";
    PlayManifest m = extractPlayManifest(src);
    ASSERT_EQ(m.controls.size(), 1u);
    EXPECT_EQ(m.controls[0].var, "z");
}
