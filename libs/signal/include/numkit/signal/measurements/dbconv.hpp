// libs/signal/include/numkit/signal/measurements/dbconv.hpp
//
// Magnitude / dB conversion utilities. Element-wise transforms over
// real or complex arrays.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit::signal {

/// db(x[, signalType]) — convert magnitude to decibels.
///   signalType "voltage" (default): db = 20·log10(|x|).
///   signalType "power":             db = 10·log10(|x|).
/// Complex input → magnitude is taken first.
Value db(const Value &x, const std::string &signalType = "voltage", std::pmr::memory_resource *mr = nullptr);

/// db2mag(d) — inverse of `db` in voltage form: 10^(d/20).
Value db2mag(const Value &d, std::pmr::memory_resource *mr = nullptr);

/// mag2db(x) — alias of db with signalType="voltage" but does NOT take
/// abs(x) first (matches MATLAB: input is required to be non-negative).
Value mag2db(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// db2pow(d) — inverse of pow2db: 10^(d/10).
Value db2pow(const Value &d, std::pmr::memory_resource *mr = nullptr);

/// pow2db(p) — 10·log10(p). Input must be non-negative.
Value pow2db(const Value &p, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
