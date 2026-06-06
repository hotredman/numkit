// libs/signal/include/numkit/signal/measurements/dbconv.hpp
//
// Magnitude / dB conversion utilities. Element-wise transforms over
// real or complex arrays.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>

namespace numkit::signal {

/// Convert magnitude or power to decibels.
///
/// For `signalType == "voltage"` (default):
/// \f$ y = 20 \log_{10} |x| \f$.
///
/// For `signalType == "power"`:
/// \f$ y = 10 \log_{10} |x| \f$.
///
/// Complex input is reduced to magnitude before the log.
///
/// @param x           Input (real or complex, any shape). Non-positive
///                    magnitudes yield `-inf`.
/// @param signalType  `"voltage"` (default) or `"power"`.
/// @param mr          Memory resource (nullptr → process default).
/// @return            DOUBLE array, same shape as `x`.
/// @throws            numkit::Error  on unknown `signalType`.
///
/// @see db2mag, db2pow, mag2db, pow2db
Value db(const Value &                x,
         const std::string &          signalType = "voltage",
         std::pmr::memory_resource *  mr         = nullptr);

/// Inverse of `db` in voltage form: \f$ y = 10^{d/20} \f$.
///
/// @param d   Magnitudes in dB.
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE array, same shape as `d`.
///
/// @see db, mag2db
Value db2mag(const Value &                d,
             std::pmr::memory_resource *  mr = nullptr);

/// Magnitude to decibels (voltage form): \f$ y = 20 \log_{10} x \f$.
///
/// Unlike `db("voltage")`, does NOT take `abs(x)` first — `mag2db`
/// requires the input to be non-negative.
///
/// @param x   Non-negative magnitudes. Negative values → `nan`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE array of dB values.
///
/// @see db2mag, db
Value mag2db(const Value &                x,
             std::pmr::memory_resource *  mr = nullptr);

/// Inverse of `pow2db`: \f$ y = 10^{d/10} \f$.
///
/// @param d   Power values in dB.
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE array of linear-power values.
///
/// @see pow2db
Value db2pow(const Value &                d,
             std::pmr::memory_resource *  mr = nullptr);

/// Power to decibels: \f$ y = 10 \log_{10} p \f$.
///
/// @param p   Non-negative power values. Negative → `nan`; zero → `-inf`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE array of dB values.
///
/// @see db2pow, mag2db
Value pow2db(const Value &                p,
             std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
