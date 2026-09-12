/// @file play_manifest.hpp
/// @ingroup group_core
///
/// Play manifest extraction (NK Play, portion A).
///
/// A gallery script opens with a block comment carrying the UI manifest:
///
///     %{
///     @play
///     { "title": ..., "groups": [ { "controls": [ { "var": "N", ... } ] } ] }
///     %}
///
/// CANONICAL marker: `@play` (user decision 2026-09-12). The `%{#ui`
/// form from the early gallery prototype is accepted as a LEGACY ALIAS
/// so already-written scripts keep running — drop it once the gallery
/// migrates. The marker sits on the first inner line of the block
/// comment; the JSON follows until the closing `%}`.
///
/// The ENGINE side needs exactly two things from the manifest: the
/// control `var` names (the variables that `setParam` drives) and their
/// `default` values. The FULL JSON is handed to the web layer untouched
/// — it builds the sliders/selects/XY-pads from it. That split keeps
/// this extractor a thin, STL-only pure function with no JSON-tree
/// dependency in core.
///
/// Scope: the FIRST manifest block in the source wins (a script with
/// several blocks is a manifest-authoring error; later ones are
/// ignored, matching "one script = one UI").

#pragma once

#include <string>
#include <vector>

namespace nk {

struct PlayControl
{
    std::string var;        // workspace variable the control drives
    std::string defaultRaw; // raw JSON value of "default" ("" if absent)
};

struct PlayManifest
{
    std::string rawJson;    // the manifest JSON, untouched
    std::vector<PlayControl> controls;

    bool empty() const { return rawJson.empty(); }
};

/// Extract the @play manifest from a script source. Returns an empty
/// manifest when no manifest block is present (plain scripts stay
/// plain). Pure function — no engine, no lexer.
PlayManifest extractPlayManifest(const std::string &source);

}  // namespace nk
