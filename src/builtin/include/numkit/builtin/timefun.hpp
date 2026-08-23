// src/builtin/include/numkit/builtin/timefun.hpp
//
// Pure C++ Date and time functions, execution timing, calendar, and conversions.
#pragma once

#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @file
/// @ingroup group_timefun
/// @brief Date, time, high-resolution profiling timers, and calendar utilities.
///
/// Provides a clean, engine-free C++ API for serial date numbers, calendar operations,
/// date formatting, date vector conversions, and timing utilities.

// ── Timing & Benchmarking ───────────────────────────────────────────────────

/// @brief Total CPU time consumed by the current process in seconds (`cputime`).
///
/// Measures CPU execution time across all threads. Useful for profiling algorithm efficiency.
///
/// @return Total elapsed CPU seconds since process start as a double.
/// @see tic, toc, pause, clock
double cputime();

/// @brief Halts execution for specified number of seconds (`pause(n)`).
///
/// Suspends current thread execution for the specified duration.
///
/// @param seconds Duration to pause in seconds (supports fractional seconds).
/// @see cputime, tic, toc
void pause(double seconds);

// ── Date and Clock ──────────────────────────────────────────────────────────

/// @brief Current date and time as a serial date number (`now`).
/// @return Serial date number (days since 0000-01-00).
/// @see clock, date, datenum
double now();

/// @brief Current date as formatted text `'dd-mmm-yyyy'` (`date`).
/// @return Date string.
/// @see now, clock, datestr
std::string date();

/// @brief Current date and time as a 6-element vector `[Y, M, D, H, MI, S]` (`clock`).
/// @param mr Memory resource for allocations (nullptr for default).
/// @return `1 x 6` matrix of current date and time components.
/// @see datevec, now, date
Value clock(std::pmr::memory_resource *mr = nullptr);

/// @brief Elapsed time in seconds between two date vectors (`etime(t2, t1)`).
/// @param t2 Ending date vector(s) (`N x 6` or `1 x 6`).
/// @param t1 Starting date vector(s) (`N x 6` or `1 x 6`).
/// @param mr Memory resource.
/// @return Column vector of elapsed seconds.
/// @see clock, datevec
Value etime(const Value &t2, const Value &t1, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts date components or string to serial date number (`datenum(...)`).
/// @param args Span of arguments (e.g. `(str [, fmt])` or `(Y, M, D [, H, MI, S])`).
/// @param mr Memory resource.
/// @return Serial date number array or scalar.
/// @see datestr, datevec, now
Value datenum(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats serial date number or date vector into text string (`datestr(...)`).
/// @param args Span of arguments (e.g. `(D [, fmt])` or `(D, fmt, P)`).
/// @param mr Memory resource.
/// @return Character array or string representation.
/// @see datenum, datevec
Value datestr(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts serial date number or text into date vector matrix `[Y, M, D, H, MI, S]` (`datevec(...)`).
/// @param args Span of arguments (e.g. `(D)` or `(str [, fmt])`).
/// @param mr Memory resource.
/// @return `N x 6` matrix of date components.
/// @see datenum, datestr, clock
Value datevec(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Day of week for date number or text (`weekday(d)`).
/// @param d Serial date number or text representation.
/// @param mr Memory resource.
/// @return Day of week integer (1=Sunday, 2=Monday, ..., 7=Saturday).
/// @see eomday, calendar
Value weekday(const Value &d, std::pmr::memory_resource *mr = nullptr);

/// @brief End of month day for year and month (`eomday(y, m)`).
/// @param y Year.
/// @param m Month (1 to 12).
/// @param mr Memory resource.
/// @return Number of days in the month (28, 29, 30, or 31).
/// @see weekday, calendar
Value eomday(const Value &y, const Value &m, std::pmr::memory_resource *mr = nullptr);

/// @brief Generates 6x7 calendar matrix for year and month (`calendar(...)`).
/// @param args Span containing `()` for current month, `(d)` for date, or `(y, m)`.
/// @param mr Memory resource.
/// @return `6 x 7` calendar matrix.
/// @see weekday, eomday
Value calendar(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Julian date number from serial date or date components (`juliandate(...)`).
/// @param args Span of arguments `(D)` or `(Y, M, D [, H, MI, S])`.
/// @param mr Memory resource.
/// @return Julian date number.
/// @see mjuliandate, datenum
Value juliandate(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Modified Julian date number (`mjuliandate(...)`).
/// @param args Span of arguments `(D)` or `(Y, M, D [, H, MI, S])`.
/// @param mr Memory resource.
/// @return Modified Julian date (`JD - 2400000.5`).
/// @see juliandate, datenum
Value mjuliandate(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Week number of the year for a given date (`weeknum(...)`).
/// @param args Span of arguments `(D [, weekStart [, european]])`.
/// @param mr Memory resource.
/// @return Week of year number.
Value weeknum(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Shifts date by quantity in specified time units (`addtodate(d, q, unit)`).
/// @param d Serial date number.
/// @param quantity Amount to add.
/// @param unit Time unit string (`'day'`, `'month'`, `'year'`, `'hour'`, `'minute'`, `'second'`, `'millisecond'`).
/// @param mr Memory resource.
/// @return Shifted serial date number.
Value addtodate(const Value &d, double quantity, const std::string &unit, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats date number as ISO integer `YYYYMMDD` (`yyyymmdd(...)`).
/// @param args Span of arguments.
/// @param mr Memory resource.
/// @return Array of packed `YYYYMMDD` integer values.
Value yyyymmdd(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
