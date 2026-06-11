// bundle/include/numkit/bundle/standard_engine.hpp
//
// StandardEngine — an Engine with the standard library already installed, as a
// stack-constructible convenience:
//
//     numkit::StandardEngine engine;          // ready to eval("sin(1)") etc.
//
// It is a thin subclass over the public installStandardLibrary() composition
// API — no new behaviour, just "construct + install" in one step. The three
// composition flavours (this, installStandardLibrary, makeStandardEngine) all
// live here in bundle/; core itself stays library-agnostic.
//
// Pulls in the full engine.hpp (needed to subclass Engine); embedders who only
// want the free function include the lighter standard_library.hpp instead.

#pragma once

#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

#include <memory_resource>

namespace numkit {

struct StandardEngine : Engine
{
    StandardEngine() { installStandardLibrary(*this); }
    explicit StandardEngine(std::pmr::memory_resource *mr) : Engine(mr)
    {
        installStandardLibrary(*this);
    }
};

} // namespace numkit
