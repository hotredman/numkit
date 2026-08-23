// include/numkit/builtin/timefun.hpp
//
// Date and time functions, execution timing, calendar, and conversions.
#pragma once

#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit {
class Engine;
}

namespace numkit::builtin {

/// @file
/// @brief Date, time, high-resolution profiling timers, and calendar utilities.

// ── Timing & Benchmarking ───────────────────────────────────────────────────

/// @brief Starts a stopwatch timer.
/// @param engine Optional engine instance to record tic state.
/// @return High-resolution timestamp in microseconds.
double tic(Engine *engine = nullptr);

/// @brief Reads elapsed time from stopwatch timer.
/// @param engine Optional engine instance.
/// @param startMicros Start timestamp from tic() (negative to use engine's last tic).
/// @return Elapsed seconds.
double toc(Engine *engine = nullptr, double startMicros = -1.0);

/// @brief Total CPU time consumed by the process in seconds.
double cputime();

/// @brief Halts execution for specified number of seconds.
/// @param seconds Duration in seconds.
void pause(double seconds);

// ── Date and Clock ──────────────────────────────────────────────────────────

/// @brief Current date and time as a serial date number.
double now();

/// @brief Current date as formatted text ('dd-mmm-yyyy').
std::string date();

/// @brief Current date and time as a vector `[year, month, day, hour, minute, second]`.
Value clock(std::pmr::memory_resource *mr = nullptr);

/// @brief Elapsed time in seconds between two date vectors.
Value etime(const Value &t2, const Value &t1, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts date components or string to serial date number.
Value datenum(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats serial date number or date vector into text string.
Value datestr(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts serial date number or text into date vector `[Y, M, D, H, MI, S]`.
Value datevec(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Day of week for date number or text (1=Sunday, 2=Monday, ..., 7=Saturday).
Value weekday(const Value &d, std::pmr::memory_resource *mr = nullptr);

/// @brief End of month day for year and month.
Value eomday(const Value &y, const Value &m, std::pmr::memory_resource *mr = nullptr);

/// @brief Generates 6x7 calendar matrix for year and month.
Value calendar(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Julian date number from serial date or date components.
Value juliandate(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Modified Julian date number.
Value mjuliandate(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Week number of the year for a given date.
Value weeknum(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Shifts date by quantity in specified time units ('day', 'month', 'year', 'hour', 'minute', 'second').
Value addtodate(const Value &d, double quantity, const std::string &unit, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats date number as ISO integer `YYYYMMDD`.
Value yyyymmdd(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

// ── Registration ────────────────────────────────────────────────────────────

/// @brief Registers all date and time builtins into the engine instance.
void register_timefun(Engine &engine);

} // namespace numkit::builtin
