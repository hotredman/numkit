#include <numkit/builtin/timefun.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/callback_builtin.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/object.hpp>
#include <numkit/core/vm.hpp>
#include <numkit/core/build_info.hpp>
#include <numkit/runtime/runtime.hpp>
#include <numkit/runtime/language/cells/cell.hpp>
#include <numkit/runtime/language/structures/struct.hpp>
#include <numkit/runtime/help/help_catalog.hpp>
#include <numkit/lang/operators/binary_ops.hpp>
#include <numkit/lang/operators/unary_ops.hpp>
#include <numkit/lang/types/types.hpp>
#include <numkit/math/arithmetic/rounding.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace numkit::builtin {

double tic(Engine *engine) {
    auto now = Clock::now();
    if (engine) engine->setTicTimer(now);
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count());
}

double toc(Engine *engine, double startMicros) {
    auto now = Clock::now();
    TimePoint start;
    if (startMicros >= 0.0) {
        start = TimePoint(std::chrono::microseconds(static_cast<long long>(startMicros)));
    } else if (engine && engine->ticWasCalled()) {
        start = engine->ticTimer();
    } else {
        throw std::runtime_error("toc: You must call 'tic' before calling 'toc'.");
    }
    return std::chrono::duration<double>(now - start).count();
}

double cputime() {
    return static_cast<double>(std::clock()) / static_cast<double>(CLOCKS_PER_SEC);
}

void pause(double seconds) {
    if (seconds > 0.0) {
        std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    }
}

double now() {
    const auto unix_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return 719529.0 + static_cast<double>(unix_us) / 86400000000.0;
}

std::string date() {
    auto t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    static const char *kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::snprintf(buf, sizeof(buf), "%02d-%s-%04d", tm.tm_mday, kMonths[tm.tm_mon % 12], 1900 + tm.tm_year);
    return std::string(buf);
}

Value clock(std::pmr::memory_resource *mr) {
    auto now_tp = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now_tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now_tp.time_since_epoch()) % 1000;
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    auto out = Value::matrix(1, 6, ValueType::DOUBLE, mr);
    double *d = out.doubleDataMut();
    d[0] = 1900.0 + tm.tm_year;
    d[1] = 1.0 + tm.tm_mon;
    d[2] = static_cast<double>(tm.tm_mday);
    d[3] = static_cast<double>(tm.tm_hour);
    d[4] = static_cast<double>(tm.tm_min);
    d[5] = static_cast<double>(tm.tm_sec) + static_cast<double>(ms.count()) / 1000.0;
    return out;
}

namespace detail {


} // namespace detail

void register_timefun(Engine &engine) {
    engine.registerFunction("clock",
                            [](Span<const Value>,
                               size_t,
                               Span<Value> outs,
                               CallContext &ctx) {
                                outs[0] = numkit::builtin::clock(ctx.engine->resource());
                            });

    engine.registerFunction("date",
                            [](Span<const Value>,
                               size_t,
                               Span<Value> outs,
                               CallContext &ctx) {
                                outs[0] = Value::fromString(numkit::builtin::date(), ctx.engine->resource());
                            });

    engine.registerFunction("pause",
                            [](Span<const Value> args,
                               size_t,
                               Span<Value> outs,
                               CallContext &) {
                                double s = args.empty() ? 0.0 : args[0].toScalar();
                                numkit::builtin::pause(s);
                                outs[0] = Value();
                            });

// ── tic ────────────────────────────────────────────────────
    engine.registerFunction("tic",
                            [](Span<const Value>,
                               size_t nargout,
                               Span<Value> outs,
                               CallContext &ctx) {
                                auto now = Clock::now();
                                ctx.engine->setTicTimer(now);
                                if (nargout > 0) {
                                    double id = static_cast<double>(
                                        std::chrono::duration_cast<std::chrono::microseconds>(
                                            now.time_since_epoch())
                                            .count());
                                    outs[0] = Value::scalar(id, ctx.engine->resource());
                                } else {
                                    outs[0] = Value();
                                }
                            });

    // ── toc ────────────────────────────────────────────────────
    engine.registerFunction("toc",
                            [](Span<const Value> args,
                               size_t nargout,
                               Span<Value> outs,
                               CallContext &ctx) {
                                auto now = Clock::now();
                                TimePoint start;
                                if (!args.empty() && args[0].isScalar()) {
                                    auto us = static_cast<long long>(args[0].toScalar());
                                    start = TimePoint(std::chrono::microseconds(us));
                                } else if (ctx.engine->ticWasCalled()) {
                                    start = ctx.engine->ticTimer();
                                } else {
                                    throw std::runtime_error(
                                        "toc: You must call 'tic' before calling 'toc'.");
                                }
                                double elapsed = std::chrono::duration<double>(now - start).count();
                                if (nargout > 0) {
                                    outs[0] = Value::scalar(elapsed, ctx.engine->resource());
                                } else {
                                    std::ostringstream os;
                                    os << "Elapsed time is " << elapsed << " seconds.\n";
                                    ctx.engine->outputText(os.str());
                                    outs[0] = Value();
                                }
                            });

    // ── cputime ───────────────────────────────────────────────
    // MATLAB cputime: total CPU seconds used by the current process
    // since startup. std::clock() is the standard portable handle.
    engine.registerFunction("cputime",
                            [](Span<const Value>,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                const double t = static_cast<double>(std::clock())
                                               / static_cast<double>(CLOCKS_PER_SEC);
                                outs[0] = Value::scalar(t, ctx.engine->resource());
                            });

    // ── now ───────────────────────────────────────────────────
    // MATLAB now: serial date number for current local time.
    // Days since 0000-01-00 (MATLAB epoch). 1970-01-01 maps to 719529.
    //   now = 719529 + (Unix microseconds) / 86_400_000_000
    // (MATLAB has deprecated `now` in favour of datetime() but many
    // scripts still call it.)
    engine.registerFunction("now",
                            [](Span<const Value>,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                const auto unix_us = std::chrono::duration_cast<
                                    std::chrono::microseconds>(
                                    std::chrono::system_clock::now().time_since_epoch()).count();
                                const double serial =
                                    719529.0 + static_cast<double>(unix_us) / 86400000000.0;
                                outs[0] = Value::scalar(serial, ctx.engine->resource());
                            });

    // ── etime ─────────────────────────────────────────────────
    // MATLAB etime(t2, t1): elapsed seconds between two date vectors.
    // Each input is a 6-element date vector [Y M D H MI S] (one row) or
    // an N-by-6 matrix of such rows; the result is an N-by-1 column of
    // elapsed seconds. A single row in one argument broadcasts against
    // N rows in the other. The computation is calendar-aware:
    //   etime = (datenum(t2) - datenum(t1)) * 86400
    // so month/year/leap-day boundaries are handled correctly. MATLAB
    // requires exactly 6 columns (it indexes column 6); fewer columns
    // raise an error here too.
    engine.registerFunction("etime",
                            [](Span<const Value> args,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.size() < 2)
                                    throw std::runtime_error(
                                        "etime: requires two date vectors (t2, t1)");

                                auto civilToSerial = [](double yd, double md,
                                                        double dd, double hd,
                                                        double mind, double sd) {
                                    double dInt;
                                    const double dFrac = std::modf(dd, &dInt);
                                    int64_t y = static_cast<int64_t>(std::floor(yd));
                                    int64_t m = static_cast<int64_t>(std::floor(md));
                                    int64_t d = static_cast<int64_t>(dInt);
                                    if (m <= 2) y -= 1;
                                    const int64_t era = (y < 0 ? y - 399 : y) / 400;
                                    const int64_t yoe = y - era * 400;
                                    const int64_t doy =
                                        (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
                                    const int64_t doe =
                                        yoe * 365 + yoe / 4 - yoe / 100 + doy;
                                    const int64_t days = era * 146097 + doe - 719468;
                                    const double frac =
                                        (hd * 3600.0 + mind * 60.0 + sd) / 86400.0 + dFrac;
                                    return static_cast<double>(days) + 719529.0 + frac;
                                };

                                auto *mr = ctx.engine->resource();
                                const Value &t2 = args[0];
                                const Value &t1 = args[1];
                                const size_t r2 = t2.dims().rows(), c2 = t2.dims().cols();
                                const size_t r1 = t1.dims().rows(), c1 = t1.dims().cols();
                                if (c2 != 6 || c1 != 6)
                                    throw std::runtime_error(
                                        "etime: date vectors must have 6 columns "
                                        "[Y M D H MI S]");
                                if (r1 != r2 && r1 != 1 && r2 != 1)
                                    throw std::runtime_error(
                                        "etime: t2 and t1 must have the same number of "
                                        "rows (or one a single row)");
                                const size_t N = std::max(r1, r2);

                                // Column-major: element (row, col) at row + col*nrows.
                                // Separate the integer date-day part from the
                                // H/MI/S part the way MATLAB does: the day
                                // difference is an exact integer, and the small
                                // time terms subtract directly, so a fractional
                                // second does not lose precision to cancellation
                                // inside a ~7.4e5 serial date number.
                                auto comp = [&](const Value &v, size_t nr,
                                                size_t row, size_t col) {
                                    return v.elemAsDouble(row + col * nr);
                                };

                                auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                for (size_t k = 0; k < N; ++k) {
                                    const size_t k2 = (r2 == 1 ? 0 : k);
                                    const size_t k1 = (r1 == 1 ? 0 : k);
                                    const double dDay =
                                        civilToSerial(comp(t2, r2, k2, 0),
                                                      comp(t2, r2, k2, 1),
                                                      comp(t2, r2, k2, 2), 0, 0, 0)
                                      - civilToSerial(comp(t1, r1, k1, 0),
                                                      comp(t1, r1, k1, 1),
                                                      comp(t1, r1, k1, 2), 0, 0, 0);
                                    const double dH  = comp(t2, r2, k2, 3) - comp(t1, r1, k1, 3);
                                    const double dMI = comp(t2, r2, k2, 4) - comp(t1, r1, k1, 4);
                                    const double dS  = comp(t2, r2, k2, 5) - comp(t1, r1, k1, 5);
                                    o[k] = 86400.0 * dDay + 3600.0 * dH + 60.0 * dMI + dS;
                                }
                                outs[0] = std::move(out);
                            });

    // ── weeknum ───────────────────────────────────────────────
    // MATLAB weeknum(D [, WeekStart [, European]]): week-of-year number
    // for serial date number D (element-wise, shape preserved).
    //   WeekStart : day the week begins, 1=Sunday .. 7=Saturday (default 1).
    //   European  : 0 (US, default) or 1. US convention counts the partial
    //               first week as week 1. The "European" convention applies
    //               the ISO-style rule that the first week with at least 4
    //               days in the year is week 1; a shorter leading partial
    //               week is donated to the last week of the previous year.
    // Both honour WeekStart. Algorithm is pure integer arithmetic on the
    // day-of-year and the weekday of Jan 1 (Sakamoto).
    engine.registerFunction("weeknum",
                            [](Span<const Value> args,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "weeknum: requires a date argument");
                                auto *mr = ctx.engine->resource();

                                int weekStart = 1;
                                if (args.size() >= 2 && !args[1].isEmpty())
                                    weekStart = static_cast<int>(args[1].toScalar());
                                if (weekStart < 1 || weekStart > 7)
                                    throw std::runtime_error(
                                        "weeknum: WeekStart must be an integer in 1..7 "
                                        "(1=Sunday)");
                                bool european = false;
                                if (args.size() >= 3 && !args[2].isEmpty())
                                    european = args[2].toScalar() != 0.0;

                                auto civilToSerial = [](int64_t y, int64_t m,
                                                        int64_t d) {
                                    if (m <= 2) y -= 1;
                                    const int64_t era = (y < 0 ? y - 399 : y) / 400;
                                    const int64_t yoe = y - era * 400;
                                    const int64_t doy =
                                        (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
                                    const int64_t doe =
                                        yoe * 365 + yoe / 4 - yoe / 100 + doy;
                                    return era * 146097 + doe - 719468 + 719529;
                                };
                                // Calendar year of a MATLAB serial date number
                                // (Howard Hinnant civil_from_days).
                                auto yearOf = [](double serial) {
                                    int64_t z = static_cast<int64_t>(std::floor(serial))
                                              - 719529 + 719468;
                                    const int64_t era =
                                        (z >= 0 ? z : z - 146096) / 146097;
                                    const int64_t doe = z - era * 146097;
                                    const int64_t yoe =
                                        (doe - doe / 1460 + doe / 36524
                                         - doe / 146096) / 365;
                                    int64_t y = yoe + era * 400;
                                    const int64_t dy =
                                        doe - (365 * yoe + yoe / 4 - yoe / 100);
                                    const int64_t mp = (5 * dy + 2) / 153;
                                    const int64_t m = mp < 10 ? mp + 3 : mp - 9;
                                    if (m <= 2) y += 1;
                                    return static_cast<int>(y);
                                };
                                auto isLeap = [](int y) {
                                    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
                                };
                                // Weekday of Jan 1 (Sakamoto), 1=Sunday .. 7=Saturday.
                                auto wdJan1 = [](int y) {
                                    static const int t[] =
                                        {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
                                    const int yy = y - 1;  // month 1 < 3
                                    const int dow =
                                        ((yy + yy / 4 - yy / 100 + yy / 400 + t[0] + 1)
                                         % 7 + 7) % 7;  // 0=Sunday
                                    return dow + 1;
                                };
                                auto weekOf = [&](int y, int doy) {
                                    const int offset = (wdJan1(y) - weekStart + 7) % 7;
                                    if (!european)
                                        return (doy - 1 + offset) / 7 + 1;
                                    const int partial = (offset == 0) ? 0 : (7 - offset);
                                    const int firstWS = (offset == 0) ? 1 : (8 - offset);
                                    if (partial >= 4)
                                        return (doy - 1 + offset) / 7 + 1;
                                    if (doy >= firstWS)
                                        return (doy - firstWS) / 7 + 1;
                                    // Leading partial week -> last week of prior year.
                                    const int yp = y - 1;
                                    const int doyp = isLeap(yp) ? 366 : 365;
                                    const int offp = (wdJan1(yp) - weekStart + 7) % 7;
                                    const int partp = (offp == 0) ? 0 : (7 - offp);
                                    const int fwp = (offp == 0) ? 1 : (8 - offp);
                                    if (partp >= 4)
                                        return (doyp - 1 + offp) / 7 + 1;
                                    return (doyp - fwp) / 7 + 1;
                                };

                                const Value &D = args[0];
                                const size_t nr = D.dims().rows();
                                const size_t nc = D.dims().cols();
                                const size_t n = D.numel();
                                auto out = Value::matrix(nr, nc, ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                for (size_t i = 0; i < n; ++i) {
                                    const double serial = D.elemAsDouble(i);
                                    const int y = yearOf(serial);
                                    const int doy = static_cast<int>(
                                        std::floor(serial) - civilToSerial(y, 1, 1)) + 1;
                                    o[i] = static_cast<double>(weekOf(y, doy));
                                }
                                outs[0] = std::move(out);
                            });

    // ── addtodate ─────────────────────────────────────────────
    // MATLAB addtodate(D, Q, units): add Q units to serial date number D
    // (scalar). Time units ('day','hour','minute','second','millisecond')
    // are plain serial arithmetic. Calendar units ('month','year') add to
    // the month/year component and clamp the day to the last valid day of
    // the resulting month (Jan 31 + 1 month -> Feb 28/29; Feb 29 + 1 year
    // -> Feb 28); the time-of-day fraction is preserved exactly by keeping
    // the integer-day and fractional parts separate.
    engine.registerFunction("addtodate",
                            [](Span<const Value> args,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.size() < 3)
                                    throw std::runtime_error(
                                        "addtodate: requires (D, quantity, units)");
                                if (args[0].numel() != 1)
                                    throw std::runtime_error(
                                        "addtodate: date number must be a real "
                                        "numeric scalar");
                                auto *mr = ctx.engine->resource();
                                const double serial = args[0].toScalar();
                                const double q = args[1].toScalar();
                                std::string u = args[2].toString();
                                for (auto &ch : u)
                                    ch = static_cast<char>(std::tolower(
                                        static_cast<unsigned char>(ch)));

                                double result;
                                if (u == "day")
                                    result = serial + q;
                                else if (u == "hour")
                                    result = serial + q / 24.0;
                                else if (u == "minute")
                                    result = serial + q / 1440.0;
                                else if (u == "second")
                                    result = serial + q / 86400.0;
                                else if (u == "millisecond")
                                    result = serial + q / 86400000.0;
                                else if (u == "month" || u == "year") {
                                    // Split integer day (calendar) from the
                                    // time-of-day fraction so it survives intact.
                                    const double dayF = std::floor(serial);
                                    const double frac = serial - dayF;
                                    const int64_t days =
                                        static_cast<int64_t>(dayF) - 719529;
                                    // civil_from_days (Howard Hinnant).
                                    int64_t z = days + 719468;
                                    const int64_t era =
                                        (z >= 0 ? z : z - 146096) / 146097;
                                    const int64_t doe = z - era * 146097;
                                    const int64_t yoe =
                                        (doe - doe / 1460 + doe / 36524
                                         - doe / 146096) / 365;
                                    int64_t Y = yoe + era * 400;
                                    const int64_t doy =
                                        doe - (365 * yoe + yoe / 4 - yoe / 100);
                                    const int64_t mp = (5 * doy + 2) / 153;
                                    const int64_t D = doy - (153 * mp + 2) / 5 + 1;
                                    int64_t M = mp < 10 ? mp + 3 : mp - 9;
                                    Y += (M <= 2);

                                    int64_t nY = Y, nM = M;
                                    if (u == "month") {
                                        int64_t tm = (M - 1)
                                                   + static_cast<int64_t>(
                                                         std::llround(q));
                                        int64_t qd = tm / 12, rd = tm % 12;
                                        if (rd < 0) { qd -= 1; rd += 12; }
                                        nY = Y + qd;
                                        nM = rd + 1;
                                    } else {  // year
                                        nY = Y + static_cast<int64_t>(
                                                     std::llround(q));
                                    }
                                    // Clamp day to the new month's length.
                                    auto leap = [](int64_t y) {
                                        return (y % 4 == 0 && y % 100 != 0)
                                            || y % 400 == 0;
                                    };
                                    static const int dim[] =
                                        {31, 28, 31, 30, 31, 30,
                                         31, 31, 30, 31, 30, 31};
                                    int64_t maxD = dim[nM - 1];
                                    if (nM == 2 && leap(nY)) maxD = 29;
                                    int64_t nD = D < maxD ? D : maxD;
                                    // civilToSerial(nY, nM, nD) -> integer day.
                                    int64_t y2 = nY;
                                    if (nM <= 2) y2 -= 1;
                                    const int64_t era2 =
                                        (y2 < 0 ? y2 - 399 : y2) / 400;
                                    const int64_t yoe2 = y2 - era2 * 400;
                                    const int64_t doy2 =
                                        (153 * (nM + (nM > 2 ? -3 : 9)) + 2) / 5
                                        + nD - 1;
                                    const int64_t doe2 =
                                        yoe2 * 365 + yoe2 / 4 - yoe2 / 100 + doy2;
                                    const int64_t newDays =
                                        era2 * 146097 + doe2 - 719468 + 719529;
                                    result = static_cast<double>(newDays) + frac;
                                } else {
                                    throw std::runtime_error(
                                        "addtodate: units must be one of "
                                        "'year','month','day','hour','minute',"
                                        "'second','millisecond'");
                                }
                                outs[0] = Value::scalar(result, mr);
                            });

    // ── datenum ───────────────────────────────────────────────
    // MATLAB datenum: serial date number from date components.
    //
    // Supported forms (string-parse forms deferred):
    //   datenum(Y, M, D)
    //   datenum(Y, M, D, H, MI, S)
    //   datenum(V)               with V row 1x3, 1x6, or matrix Nx3 / Nx6
    //
    // Algorithm: Howard Hinnant's `days_from_civil` (proleptic Gregorian)
    // returns days since 1970-01-01; add 719529 for the MATLAB epoch
    // (1 = 0000-01-01, MATLAB's "year zero" reference). Month/day overflow
    // wraps naturally, matching MATLAB behaviour.
    engine.registerFunction("datenum",
                            [](Span<const Value> args,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "datenum requires at least one argument");

                                auto civilToSerial = [](double yd, double md,
                                                        double dd, double hd,
                                                        double mind, double sd) {
                                    // Floor day to integer; fractional part
                                    // contributes to time-of-day fraction.
                                    double dInt;
                                    const double dFrac = std::modf(dd, &dInt);
                                    int64_t y = static_cast<int64_t>(std::floor(yd));
                                    int64_t m = static_cast<int64_t>(std::floor(md));
                                    int64_t d = static_cast<int64_t>(dInt);
                                    if (m <= 2) y -= 1;
                                    const int64_t era = (y < 0 ? y - 399 : y) / 400;
                                    const int64_t yoe = y - era * 400;
                                    const int64_t doy =
                                        (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5
                                        + d - 1;
                                    const int64_t doe =
                                        yoe * 365 + yoe / 4 - yoe / 100 + doy;
                                    const int64_t days =
                                        era * 146097 + doe - 719468;
                                    const double frac =
                                        (hd * 3600.0 + mind * 60.0 + sd) / 86400.0
                                        + dFrac;
                                    return static_cast<double>(days) + 719529.0
                                         + frac;
                                };

                                auto *mr = ctx.engine->resource();

                                // ── String date input: datenum(str [, fmt]) ──
                                // Parses a single date string with an explicit
                                // format string, or auto-detects the common ISO
                                // (yyyy-mm-dd[ HH:MM:SS]) and dd-mmm-yyyy forms.
                                if (args[0].isChar() || args[0].isString()) {
                                    const std::string s = args[0].toString();
                                    static const char *MON3[] = {
                                        "jan","feb","mar","apr","may","jun",
                                        "jul","aug","sep","oct","nov","dec"};
                                    auto tryFmt = [&](const std::string &fmt,
                                                      double &Y, double &Mo,
                                                      double &D, double &H,
                                                      double &MI, double &S) -> bool {
                                        Y = 0; Mo = 1; D = 1; H = 0; MI = 0; S = 0;
                                        size_t si = 0, fi = 0;
                                        auto readNum = [&](int maxD) -> long {
                                            long v = 0; int n = 0;
                                            while (si < s.size() && n < maxD
                                                   && std::isdigit(
                                                       static_cast<unsigned char>(s[si]))) {
                                                v = v * 10 + (s[si] - '0'); ++si; ++n;
                                            }
                                            return n > 0 ? v : -1;
                                        };
                                        while (fi < fmt.size()) {
                                            if (fmt.compare(fi, 4, "yyyy") == 0) {
                                                long v = readNum(4); if (v < 0) return false;
                                                Y = static_cast<double>(v); fi += 4;
                                            } else if (fmt.compare(fi, 4, "mmmm") == 0
                                                       || fmt.compare(fi, 3, "mmm") == 0) {
                                                bool full = fmt.compare(fi, 4, "mmmm") == 0;
                                                if (si + 3 > s.size()) return false;
                                                std::string mon = s.substr(si, 3);
                                                for (auto &c : mon)
                                                    c = static_cast<char>(std::tolower(
                                                        static_cast<unsigned char>(c)));
                                                int mi = -1;
                                                for (int k = 0; k < 12; ++k)
                                                    if (mon == MON3[k]) { mi = k + 1; break; }
                                                if (mi < 0) return false;
                                                Mo = static_cast<double>(mi);
                                                si += 3;
                                                if (full) {
                                                    while (si < s.size() && std::isalpha(
                                                               static_cast<unsigned char>(s[si]))) ++si;
                                                    fi += 4;
                                                } else { fi += 3; }
                                            } else if (fmt.compare(fi, 2, "mm") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                Mo = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "dd") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                D = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "HH") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                H = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "MM") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                MI = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "SS") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                S = static_cast<double>(v); fi += 2;
                                            } else {
                                                if (si < s.size() && s[si] == fmt[fi]) { ++si; ++fi; }
                                                else return false;
                                            }
                                        }
                                        return si == s.size();   // full consume
                                    };

                                    double Y, Mo, D, H, MI, S;
                                    bool ok = false;
                                    if (args.size() >= 2
                                        && (args[1].isChar() || args[1].isString())) {
                                        ok = tryFmt(args[1].toString(), Y, Mo, D, H, MI, S);
                                    } else {
                                        static const char *cands[] = {
                                            "yyyy-mm-dd HH:MM:SS", "yyyy-mm-dd",
                                            "dd-mmm-yyyy HH:MM:SS", "dd-mmm-yyyy"};
                                        for (const char *c : cands)
                                            if (tryFmt(c, Y, Mo, D, H, MI, S)) { ok = true; break; }
                                    }
                                    if (!ok)
                                        throw std::runtime_error(
                                            "datenum: could not parse date string "
                                            "(supported: explicit format string, or "
                                            "ISO yyyy-mm-dd and dd-mmm-yyyy forms)");
                                    outs[0] = Value::scalar(
                                        civilToSerial(Y, Mo, D, H, MI, S), mr);
                                    return;
                                }

                                // ── Single-arg form: V is 1x3, 1x6, Nx3, Nx6 ─
                                if (args.size() == 1) {
                                    const Value &V = args[0];
                                    const size_t R = V.dims().rows();
                                    const size_t C = V.dims().cols();
                                    if (C != 3 && C != 6)
                                        throw std::runtime_error(
                                            "datenum: single-arg matrix must "
                                            "have 3 or 6 columns");
                                    auto out = Value::matrix(
                                        R, 1, ValueType::DOUBLE, mr);
                                    double *o = out.doubleDataMut();
                                    for (size_t i = 0; i < R; ++i) {
                                        const double y = V.elemAsDouble(i);
                                        const double m = V.elemAsDouble(i + R);
                                        const double d = V.elemAsDouble(i + 2 * R);
                                        double h = 0.0, mi = 0.0, s = 0.0;
                                        if (C == 6) {
                                            h  = V.elemAsDouble(i + 3 * R);
                                            mi = V.elemAsDouble(i + 4 * R);
                                            s  = V.elemAsDouble(i + 5 * R);
                                        }
                                        o[i] = civilToSerial(y, m, d, h, mi, s);
                                    }
                                    if (R == 1)
                                        outs[0] = Value::scalar(o[0], mr);
                                    else
                                        outs[0] = std::move(out);
                                    return;
                                }

                                // ── Multi-arg form: 3 or 6 args ─────────────
                                if (args.size() != 3 && args.size() != 6)
                                    throw std::runtime_error(
                                        "datenum: expected 3 or 6 arguments "
                                        "(Y,M,D[,H,MI,S])");
                                // Determine output size = max numel across args
                                // (broadcast scalars). All non-scalar inputs
                                // must share the same numel.
                                size_t N = 1;
                                for (const auto &a : args) {
                                    if (a.numel() > 1) {
                                        if (N > 1 && a.numel() != N)
                                            throw std::runtime_error(
                                                "datenum: input vector lengths "
                                                "must match");
                                        N = a.numel();
                                    }
                                }
                                auto pick = [](const Value &a, size_t i) {
                                    return a.numel() == 1
                                               ? a.toScalar()
                                               : a.elemAsDouble(i);
                                };
                                if (N == 1) {
                                    const double h = args.size() == 6
                                                         ? args[3].toScalar()
                                                         : 0.0;
                                    const double mi = args.size() == 6
                                                          ? args[4].toScalar()
                                                          : 0.0;
                                    const double s = args.size() == 6
                                                         ? args[5].toScalar()
                                                         : 0.0;
                                    outs[0] = Value::scalar(
                                        civilToSerial(args[0].toScalar(),
                                                      args[1].toScalar(),
                                                      args[2].toScalar(),
                                                      h, mi, s),
                                        mr);
                                    return;
                                }
                                auto out = Value::matrix(
                                    N, 1, ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                for (size_t i = 0; i < N; ++i) {
                                    const double y = pick(args[0], i);
                                    const double m = pick(args[1], i);
                                    const double d = pick(args[2], i);
                                    double h = 0.0, mi = 0.0, s = 0.0;
                                    if (args.size() == 6) {
                                        h  = pick(args[3], i);
                                        mi = pick(args[4], i);
                                        s  = pick(args[5], i);
                                    }
                                    o[i] = civilToSerial(y, m, d, h, mi, s);
                                }
                                outs[0] = std::move(out);
                            });

    // ── weekday ───────────────────────────────────────────────
    // MATLAB weekday(D[, fmt]): day-of-week index 1..7 with Sunday=1,
    // Saturday=7 (US convention). Optional second output is the day
    // name as 'short' (Sun..Sat) or 'long' (Sunday..Saturday).
    //
    // Algorithm: serial 1 (= 0000-01-01) is a Saturday in MATLAB's
    // calendar. Solving: dow(d) = ((floor(d) - 2) mod 7) + 1 with
    // positive-modulo. Verified against MATLAB R2025b for current
    // and historical dates.
    engine.registerFunction("weekday",
                            [](Span<const Value> args,
                               size_t nargout,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "weekday requires at least one input");
                                bool wantLong = false;
                                if (args.size() >= 2
                                    && (args[1].isChar() || args[1].isString())) {
                                    std::string fmt = args[1].toString();
                                    for (auto &c : fmt)
                                        c = static_cast<char>(
                                            std::tolower(
                                                static_cast<unsigned char>(c)));
                                    if (fmt == "long")
                                        wantLong = true;
                                    else if (fmt != "short")
                                        throw std::runtime_error(
                                            "weekday: format must be 'short' "
                                            "or 'long'");
                                }
                                static const char *kShort[7] = {
                                    "Sun", "Mon", "Tue", "Wed",
                                    "Thu", "Fri", "Sat"
                                };
                                static const char *kLong[7] = {
                                    "Sunday", "Monday",   "Tuesday",
                                    "Wednesday", "Thursday", "Friday",
                                    "Saturday"
                                };
                                auto dayIndex = [](double d) -> int {
                                    // Positive-result modulo of (floor(d)-2) by 7.
                                    int64_t f = static_cast<int64_t>(
                                        std::floor(d)) - 2;
                                    int64_t r = f % 7;
                                    if (r < 0) r += 7;
                                    return static_cast<int>(r) + 1;
                                };

                                auto *mr = ctx.engine->resource();
                                const Value &D = args[0];
                                const size_t N = D.numel();

                                if (N == 1) {
                                    int idx = dayIndex(D.toScalar());
                                    outs[0] = Value::scalar(
                                        static_cast<double>(idx), mr);
                                    if (nargout > 1) {
                                        outs[1] = Value::fromString(
                                            wantLong ? kLong[idx - 1]
                                                     : kShort[idx - 1],
                                            mr);
                                    }
                                    return;
                                }

                                // Vector / matrix output: same shape as input.
                                auto out = Value::matrix(
                                    D.dims().rows(), D.dims().cols(),
                                    ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                for (size_t i = 0; i < N; ++i)
                                    o[i] = static_cast<double>(
                                        dayIndex(D.elemAsDouble(i)));
                                outs[0] = std::move(out);
                                // Name output: only meaningful for scalar D
                                // in MATLAB's current API; for vector input,
                                // MATLAB returns the name of the FIRST element
                                // (legacy behaviour). Match it.
                                if (nargout > 1) {
                                    int idx = dayIndex(D.elemAsDouble(0));
                                    outs[1] = Value::fromString(
                                        wantLong ? kLong[idx - 1]
                                                 : kShort[idx - 1],
                                        mr);
                                }
                            });

    // ── juliandate ────────────────────────────────────────────
    // MATLAB juliandate: Julian day number from date components.
    //
    // Reference relationship: serial-MATLAB-date(1970,1,1) = 719529
    // and Julian-Date(1970-01-01 00:00 UTC) = 2440587.5, so:
    //
    //   JD = datenum-serial + 1721058.5
    //
    // Verified against well-known anchors:
    //   1970-01-01 00:00 = 2440587.5 (Unix epoch)
    //   2000-01-01 12:00 = 2451545.0 (J2000.0)
    //
    // Signatures (string + datetime forms deferred):
    //   juliandate(Y, M, D)              -- separate scalar/vector args
    //   juliandate(Y, M, D, H, MI, S)
    //   juliandate(V)                    -- V is Nx3 or Nx6
    engine.registerFunction("juliandate",
                            [](Span<const Value> args,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "juliandate requires at least one "
                                        "argument");
                                if (args[0].isChar() || args[0].isString())
                                    throw std::runtime_error(
                                        "juliandate: string parsing not "
                                        "yet supported");

                                auto civilToSerial = [](double yd, double md,
                                                        double dd, double hd,
                                                        double mind, double sd) {
                                    double dInt;
                                    const double dFrac = std::modf(dd, &dInt);
                                    int64_t y = static_cast<int64_t>(
                                        std::floor(yd));
                                    int64_t m = static_cast<int64_t>(
                                        std::floor(md));
                                    int64_t d = static_cast<int64_t>(dInt);
                                    if (m <= 2) y -= 1;
                                    const int64_t era =
                                        (y < 0 ? y - 399 : y) / 400;
                                    const int64_t yoe = y - era * 400;
                                    const int64_t doy =
                                        (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5
                                        + d - 1;
                                    const int64_t doe =
                                        yoe * 365 + yoe / 4 - yoe / 100 + doy;
                                    const int64_t days =
                                        era * 146097 + doe - 719468;
                                    const double frac =
                                        (hd * 3600.0 + mind * 60.0 + sd) / 86400.0
                                        + dFrac;
                                    return static_cast<double>(days) + 719529.0
                                         + frac;
                                };
                                const double kJDOffset = 1721058.5;

                                auto *mr = ctx.engine->resource();

                                // Single-arg matrix form
                                if (args.size() == 1) {
                                    const Value &V = args[0];
                                    const size_t R = V.dims().rows();
                                    const size_t C = V.dims().cols();
                                    if (C != 3 && C != 6)
                                        throw std::runtime_error(
                                            "juliandate: single-arg matrix "
                                            "must have 3 or 6 columns");
                                    auto out = Value::matrix(
                                        R, 1, ValueType::DOUBLE, mr);
                                    double *o = out.doubleDataMut();
                                    for (size_t i = 0; i < R; ++i) {
                                        const double y = V.elemAsDouble(i);
                                        const double m = V.elemAsDouble(i + R);
                                        const double d = V.elemAsDouble(i + 2 * R);
                                        double h = 0.0, mi = 0.0, s = 0.0;
                                        if (C == 6) {
                                            h  = V.elemAsDouble(i + 3 * R);
                                            mi = V.elemAsDouble(i + 4 * R);
                                            s  = V.elemAsDouble(i + 5 * R);
                                        }
                                        o[i] = civilToSerial(y, m, d, h, mi, s)
                                             + kJDOffset;
                                    }
                                    if (R == 1)
                                        outs[0] = Value::scalar(o[0], mr);
                                    else
                                        outs[0] = std::move(out);
                                    return;
                                }

                                if (args.size() != 3 && args.size() != 6)
                                    throw std::runtime_error(
                                        "juliandate: expected 3 or 6 "
                                        "arguments (Y,M,D[,H,MI,S])");
                                size_t N = 1;
                                for (const auto &a : args) {
                                    if (a.numel() > 1) {
                                        if (N > 1 && a.numel() != N)
                                            throw std::runtime_error(
                                                "juliandate: input vector "
                                                "lengths must match");
                                        N = a.numel();
                                    }
                                }
                                auto pick = [](const Value &a, size_t i) {
                                    return a.numel() == 1
                                               ? a.toScalar()
                                               : a.elemAsDouble(i);
                                };
                                if (N == 1) {
                                    const double h = args.size() == 6
                                                         ? args[3].toScalar()
                                                         : 0.0;
                                    const double mi = args.size() == 6
                                                          ? args[4].toScalar()
                                                          : 0.0;
                                    const double s = args.size() == 6
                                                         ? args[5].toScalar()
                                                         : 0.0;
                                    outs[0] = Value::scalar(
                                        civilToSerial(args[0].toScalar(),
                                                      args[1].toScalar(),
                                                      args[2].toScalar(),
                                                      h, mi, s)
                                            + kJDOffset,
                                        mr);
                                    return;
                                }
                                auto out = Value::matrix(
                                    N, 1, ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                for (size_t i = 0; i < N; ++i) {
                                    const double y = pick(args[0], i);
                                    const double m = pick(args[1], i);
                                    const double d = pick(args[2], i);
                                    double h = 0.0, mi = 0.0, s = 0.0;
                                    if (args.size() == 6) {
                                        h  = pick(args[3], i);
                                        mi = pick(args[4], i);
                                        s  = pick(args[5], i);
                                    }
                                    o[i] = civilToSerial(y, m, d, h, mi, s)
                                         + kJDOffset;
                                }
                                outs[0] = std::move(out);
                            });

    // ── eomday ────────────────────────────────────────────────
    // MATLAB eomday(y, m): last day of the given month (28..31).
    //
    // Leap year rule (proleptic Gregorian):
    //   isLeap(y) = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0
    //
    // Shape: output preserves the broadcast shape of (y, m). Both
    // scalar -> scalar; matched non-scalars must have identical
    // shape; one scalar broadcasts.
    engine.registerFunction("eomday",
                            [](Span<const Value> args,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.size() < 2)
                                    throw std::runtime_error(
                                        "eomday requires (year, month)");
                                static const int kMonthDays[12] = {
                                    31, 28, 31, 30, 31, 30,
                                    31, 31, 30, 31, 30, 31
                                };
                                auto isLeap = [](int64_t y) {
                                    return (y % 4 == 0 && y % 100 != 0)
                                        || (y % 400 == 0);
                                };
                                auto monthEnd = [&](double yd, double md) {
                                    int64_t y = static_cast<int64_t>(
                                        std::floor(yd));
                                    int64_t m = static_cast<int64_t>(
                                        std::floor(md));
                                    if (m < 1 || m > 12)
                                        throw std::runtime_error(
                                            "eomday: month must be in 1..12");
                                    int days = kMonthDays[m - 1];
                                    if (m == 2 && isLeap(y)) days = 29;
                                    return static_cast<double>(days);
                                };

                                auto *mr = ctx.engine->resource();
                                const Value &Y = args[0];
                                const Value &M = args[1];

                                // Both scalar -> scalar output.
                                if (Y.numel() == 1 && M.numel() == 1) {
                                    outs[0] = Value::scalar(
                                        monthEnd(Y.toScalar(), M.toScalar()),
                                        mr);
                                    return;
                                }
                                // Determine output shape (broadcast).
                                size_t R, C;
                                if (Y.numel() == 1) {
                                    R = M.dims().rows();
                                    C = M.dims().cols();
                                } else if (M.numel() == 1) {
                                    R = Y.dims().rows();
                                    C = Y.dims().cols();
                                } else {
                                    if (Y.dims().rows() != M.dims().rows()
                                        || Y.dims().cols() != M.dims().cols())
                                        throw std::runtime_error(
                                            "eomday: y and m must have the "
                                            "same shape (or one scalar)");
                                    R = Y.dims().rows();
                                    C = Y.dims().cols();
                                }
                                auto out = Value::matrix(
                                    R, C, ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                const size_t N = R * C;
                                for (size_t i = 0; i < N; ++i) {
                                    const double yi = Y.numel() == 1
                                                          ? Y.toScalar()
                                                          : Y.elemAsDouble(i);
                                    const double mi = M.numel() == 1
                                                          ? M.toScalar()
                                                          : M.elemAsDouble(i);
                                    o[i] = monthEnd(yi, mi);
                                }
                                outs[0] = std::move(out);
                            });

    // ── datevec ───────────────────────────────────────────────
    // MATLAB datevec(d): inverse of datenum.
    //
    // Single output: N-by-6 matrix, one row per scalar input element
    // (column-major linearisation for matrix input). Six outputs:
    // separate length-N column vectors (Y, M, D, H, MI, S).
    //
    // Algorithm: Howard Hinnant's `civil_from_days` to recover (Y, M, D)
    // from the integer day index, then extract H, MI, S from the
    // fractional part. Microsecond rounding tames double-precision
    // noise so datenum->datevec round-trips give exact integers.
    //
    // Edge: datevec(0) = [0 0 0 0 0 0] (matches MATLAB literal).
    // calendar(year, month) — 6x7 matrix for the given month. Columns are
    // Sunday..Saturday; each day sits in its day-of-week column, weeks run down
    // the rows, empty cells are 0, and the grid is always padded to 6 rows.
    // (The no-arg "current month" and datenum forms are not yet supported.)
    engine.registerFunction("calendar",
                            [](Span<const Value> args, size_t /*nargout*/,
                               Span<Value> outs, CallContext &ctx) {
                                auto *mr = ctx.engine->resource();
                                if (args.size() < 2)
                                    throw std::runtime_error(
                                        "calendar: requires (year, month); the "
                                        "no-arg and datenum forms are not yet "
                                        "supported");
                                const int y = static_cast<int>(args[0].toScalar());
                                const int m = static_cast<int>(args[1].toScalar());
                                if (m < 1 || m > 12)
                                    throw std::runtime_error(
                                        "calendar: month must be in 1..12");
                                static const int dim[] = {
                                    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                                int nd = dim[m - 1];
                                if (m == 2 && ((y % 4 == 0 && y % 100 != 0)
                                               || y % 400 == 0))
                                    nd = 29;
                                // Day-of-week of the 1st (Sakamoto, 0 = Sunday).
                                static const int t[] = {
                                    0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
                                int yy = y - (m < 3 ? 1 : 0);
                                int dow = ((yy + yy / 4 - yy / 100 + yy / 400
                                            + t[m - 1] + 1) % 7 + 7) % 7;
                                auto out = Value::matrix(6, 7, ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();   // column-major
                                for (int k = 0; k < 42; ++k) o[k] = 0.0;
                                for (int d = 1; d <= nd; ++d) {
                                    const int pos = dow + (d - 1);
                                    const int row = pos / 7;
                                    const int col = pos % 7;
                                    o[row + col * 6] = static_cast<double>(d);
                                }
                                outs[0] = std::move(out);
                            });

    // datestr(D [, fmt]) — format a serial date number (or a 1x6 date vector)
    // as text. Supports a format STRING with the common field tokens
    // (yyyy yy mmmm mmm mm dddd ddd dd HH MM SS) and an auto-selected default
    // format. (numeric format codes, AM/PM 12-hour, and multi-date matrix
    // inputs are not yet handled.)
    engine.registerFunction("datestr",
                            [](Span<const Value> args, size_t /*nargout*/,
                               Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "datestr requires at least one argument");
                                auto *mr = ctx.engine->resource();
                                const Value &din = args[0];

                                auto civilFromDays = [](int64_t z, int64_t &Y,
                                                        int &M, int &D) {
                                    z += 719468;
                                    const int64_t era =
                                        (z >= 0 ? z : z - 146096) / 146097;
                                    const int64_t doe = z - era * 146097;
                                    const int64_t yoe =
                                        (doe - doe / 1460 + doe / 36524
                                         - doe / 146096) / 365;
                                    const int64_t y = yoe + era * 400;
                                    const int64_t doy =
                                        doe - (365 * yoe + yoe / 4 - yoe / 100);
                                    const int64_t mp = (5 * doy + 2) / 153;
                                    D = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
                                    M = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
                                    Y = y + (M <= 2 ? 1 : 0);
                                };

                                struct Comp { int y, mo, d, h, mi, s; };
                                auto serialToComp = [&](double dval) -> Comp {
                                    const double floored = std::floor(dval);
                                    const int64_t z =
                                        static_cast<int64_t>(floored) - 719529;
                                    const double frac = dval - floored;
                                    int64_t Y; int M, D;
                                    civilFromDays(z, Y, M, D);
                                    int64_t ms = static_cast<int64_t>(
                                        std::round(frac * 86400.0 * 1.0e3));
                                    int H  = static_cast<int>(ms / 3600000LL); ms %= 3600000LL;
                                    int MI = static_cast<int>(ms / 60000LL);   ms %= 60000LL;
                                    double S = static_cast<double>(ms) / 1.0e3;
                                    if (S >= 60.0) { S -= 60.0; ++MI; }
                                    if (MI >= 60)  { MI -= 60;  ++H;  }
                                    if (H  >= 24)  { H  -= 24; civilFromDays(z + 1, Y, M, D); }
                                    return Comp{ static_cast<int>(Y), M, D, H, MI,
                                                 static_cast<int>(std::round(S)) };
                                };

                                // Build the list of dates. An N-by-6 matrix
                                // (cols==6) is N DATE VECTORS (one per row);
                                // anything else (scalar / vector / non-6-col
                                // matrix) is serial date NUMBERS in column-major
                                // order, each a separate date. Matches MATLAB
                                // R2025b's datestr disambiguation: a 6x1 column
                                // is 6 dates, a 1x6 row is one date vector.
                                std::vector<Comp> dates;
                                if (din.isChar() || din.isString()) {
                                    // String date input: auto-detect the common
                                    // ISO / dd-mmm-yyyy forms (same forms as
                                    // datevec/datenum), one date. Any 2nd arg is
                                    // the OUTPUT format, not the input parse spec.
                                    static const char *MON3p[] = {
                                        "jan","feb","mar","apr","may","jun",
                                        "jul","aug","sep","oct","nov","dec"};
                                    const std::string s = din.toString();
                                    auto tryFmt = [&](const char *fmt, Comp &c) -> bool {
                                        int Y = 0, Mo = 1, D = 1, H = 0, MI = 0, S = 0;
                                        const std::string F = fmt;
                                        size_t si = 0, fi = 0;
                                        auto readNum = [&](int maxD) -> long {
                                            long v = 0; int n = 0;
                                            while (si < s.size() && n < maxD
                                                   && std::isdigit((unsigned char)s[si])) {
                                                v = v * 10 + (s[si] - '0'); ++si; ++n;
                                            }
                                            return n > 0 ? v : -1;
                                        };
                                        while (fi < F.size()) {
                                            if (F.compare(fi,4,"yyyy")==0) { long v=readNum(4); if(v<0)return false; Y=(int)v; fi+=4; }
                                            else if (F.compare(fi,3,"mmm")==0) {
                                                if (si+3>s.size()) return false;
                                                std::string m=s.substr(si,3);
                                                for (auto &ch:m) ch=(char)std::tolower((unsigned char)ch);
                                                int mi=-1; for(int k=0;k<12;++k) if(m==MON3p[k]){mi=k+1;break;}
                                                if (mi<0) return false; Mo=mi; si+=3; fi+=3;
                                            }
                                            else if (F.compare(fi,2,"mm")==0) { long v=readNum(2); if(v<0)return false; Mo=(int)v; fi+=2; }
                                            else if (F.compare(fi,2,"dd")==0) { long v=readNum(2); if(v<0)return false; D=(int)v; fi+=2; }
                                            else if (F.compare(fi,2,"HH")==0) { long v=readNum(2); if(v<0)return false; H=(int)v; fi+=2; }
                                            else if (F.compare(fi,2,"MM")==0) { long v=readNum(2); if(v<0)return false; MI=(int)v; fi+=2; }
                                            else if (F.compare(fi,2,"SS")==0) { long v=readNum(2); if(v<0)return false; S=(int)v; fi+=2; }
                                            else { if (si<s.size() && s[si]==F[fi]) { ++si; ++fi; } else return false; }
                                        }
                                        if (si != s.size()) return false;
                                        c = Comp{Y, Mo, D, H, MI, S};
                                        return true;
                                    };
                                    static const char *cands[] = {
                                        "yyyy-mm-dd HH:MM:SS", "yyyy-mm-dd",
                                        "dd-mmm-yyyy HH:MM:SS", "dd-mmm-yyyy"};
                                    Comp c{}; bool ok = false;
                                    for (const char *f : cands) if (tryFmt(f, c)) { ok = true; break; }
                                    if (!ok)
                                        throw std::runtime_error(
                                            "datestr: could not parse date string "
                                            "(supported: ISO yyyy-mm-dd and dd-mmm-yyyy forms)");
                                    dates.push_back(c);
                                } else {
                                    const size_t Rr = din.dims().rows();
                                    const size_t Cc = din.dims().cols();
                                    if (Cc == 6 && !din.dims().is3D()) {
                                        dates.reserve(Rr);
                                        for (size_t r = 0; r < Rr; ++r)
                                            dates.push_back(Comp{
                                                static_cast<int>(din.elemAsDouble(0 * Rr + r)),
                                                static_cast<int>(din.elemAsDouble(1 * Rr + r)),
                                                static_cast<int>(din.elemAsDouble(2 * Rr + r)),
                                                static_cast<int>(din.elemAsDouble(3 * Rr + r)),
                                                static_cast<int>(din.elemAsDouble(4 * Rr + r)),
                                                static_cast<int>(std::round(din.elemAsDouble(5 * Rr + r)))});
                                    } else {
                                        const size_t n = din.numel();
                                        dates.reserve(n);
                                        for (size_t k = 0; k < n; ++k)
                                            dates.push_back(serialToComp(din.elemAsDouble(k)));
                                    }
                                }
                                if (dates.empty())
                                    throw std::runtime_error("datestr: empty date input");
                                bool anyTime = false;
                                for (const auto &c : dates)
                                    if (c.h != 0 || c.mi != 0 || c.s != 0) { anyTime = true; break; }

                                std::string fmt;
                                if (args.size() >= 2) {
                                    const Value &f = args[1];
                                    if (f.isChar() || f.isString())
                                        fmt = f.toString();
                                    else {
                                        // Numeric format code -> MATLAB dateform
                                        // string. Quarter formats use lowercase
                                        // yy/yyyy here to match the token loop;
                                        // the rendered output equals MATLAB's.
                                        static const char *DATEFORM[] = {
                                            "dd-mmm-yyyy HH:MM:SS", // 0
                                            "dd-mmm-yyyy",          // 1
                                            "mm/dd/yy",             // 2
                                            "mmm",                  // 3
                                            "m",                    // 4
                                            "mm",                   // 5
                                            "mm/dd",                // 6
                                            "dd",                   // 7
                                            "ddd",                  // 8
                                            "d",                    // 9
                                            "yyyy",                 // 10
                                            "yy",                   // 11
                                            "mmmyy",                // 12
                                            "HH:MM:SS",             // 13
                                            "HH:MM:SS PM",          // 14
                                            "HH:MM",                // 15
                                            "HH:MM PM",             // 16
                                            "QQ-yy",                // 17
                                            "QQ",                   // 18
                                            "dd/mm",                // 19
                                            "dd/mm/yy",             // 20
                                            "mmm.dd,yyyy HH:MM:SS", // 21
                                            "mmm.dd,yyyy",          // 22
                                            "mm/dd/yyyy",           // 23
                                            "dd/mm/yyyy",           // 24
                                            "yy/mm/dd",             // 25
                                            "yyyy/mm/dd",           // 26
                                            "QQ-yyyy",              // 27
                                            "mmmyyyy",              // 28
                                            "yyyy-mm-dd",           // 29
                                            "yyyymmddTHHMMSS",      // 30
                                            "yyyy-mm-dd HH:MM:SS",  // 31
                                        };
                                        const int code = static_cast<int>(
                                            std::round(f.elemAsDouble(0)));
                                        if (code < 0 || code > 31)
                                            throw std::runtime_error(
                                                "datestr: unsupported numeric "
                                                "format code (expected 0-31)");
                                        fmt = DATEFORM[code];
                                    }
                                } else {
                                    fmt = anyTime ? "dd-mmm-yyyy HH:MM:SS"
                                                  : "dd-mmm-yyyy";
                                }

                                static const char *MON3[] = {
                                    "Jan","Feb","Mar","Apr","May","Jun",
                                    "Jul","Aug","Sep","Oct","Nov","Dec"};
                                static const char *MONF[] = {
                                    "January","February","March","April","May",
                                    "June","July","August","September","October",
                                    "November","December"};
                                static const char *DOW3[] = {
                                    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
                                static const char *DOWF[] = {
                                    "Sunday","Monday","Tuesday","Wednesday",
                                    "Thursday","Friday","Saturday"};
                                // Day of week via Sakamoto's algorithm.
                                static const int dt[] = {0,3,2,5,0,3,5,1,4,6,2,4};
                                // A meridiem token ('AM'/'PM', case-insensitive)
                                // anywhere in the format switches HH to a
                                // 12-hour, space-padded clock and prints AM/PM
                                // by time of day (12 AM = midnight, 12 PM = noon).
                                bool hour12 = false;
                                for (size_t k = 0; k + 1 < fmt.size(); ++k) {
                                    char a = (char)std::tolower((unsigned char)fmt[k]);
                                    char b = (char)std::tolower((unsigned char)fmt[k+1]);
                                    if ((a == 'a' || a == 'p') && b == 'm') { hour12 = true; break; }
                                }

                                // Render one date with the chosen format.
                                auto renderOne = [&](const Comp &cc) -> std::string {
                                const int yi = cc.y, moi = cc.mo, di = cc.d,
                                          hi = cc.h, mii = cc.mi, si = cc.s;
                                int yw = yi - (moi < 3 ? 1 : 0);
                                int dow = ((yw + yw/4 - yw/100 + yw/400
                                            + dt[(moi - 1 + 12) % 12] + di) % 7 + 7) % 7;
                                const int h12 = (hi % 12 == 0) ? 12 : (hi % 12);
                                std::string out;
                                char buf[16];
                                size_t i = 0;
                                auto at = [&](const char *t, size_t L) {
                                    return fmt.compare(i, L, t) == 0;
                                };
                                while (i < fmt.size()) {
                                    if (at("yyyy", 4)) { std::snprintf(buf,sizeof buf,"%04d",yi); out+=buf; i+=4; }
                                    else if (at("yy", 2)) { std::snprintf(buf,sizeof buf,"%02d",((yi%100)+100)%100); out+=buf; i+=2; }
                                    else if (at("mmmm", 4)) { out += MONF[(moi-1+12)%12]; i+=4; }
                                    else if (at("mmm", 3)) { out += MON3[(moi-1+12)%12]; i+=3; }
                                    else if (at("mm", 2)) { std::snprintf(buf,sizeof buf,"%02d",moi); out+=buf; i+=2; }
                                    else if (at("m", 1)) { out += MON3[(moi-1+12)%12][0]; i+=1; }   // first letter of month
                                    else if (at("QQ", 2)) { out += 'Q'; out += static_cast<char>('0' + ((moi - 1) / 3 + 1)); i+=2; }   // quarter
                                    else if (at("dddd", 4)) { out += DOWF[dow]; i+=4; }
                                    else if (at("ddd", 3)) { out += DOW3[dow]; i+=3; }
                                    else if (at("dd", 2)) { std::snprintf(buf,sizeof buf,"%02d",di); out+=buf; i+=2; }
                                    else if (at("d", 1)) { out += DOW3[dow][0]; i+=1; }   // first letter of weekday
                                    else if (at("HH", 2)) {
                                        if (hour12) std::snprintf(buf,sizeof buf,"%2d",h12);
                                        else        std::snprintf(buf,sizeof buf,"%02d",hi);
                                        out+=buf; i+=2;
                                    }
                                    else if (at("MM", 2)) { std::snprintf(buf,sizeof buf,"%02d",mii); out+=buf; i+=2; }
                                    else if (at("SS", 2)) { std::snprintf(buf,sizeof buf,"%02d",si); out+=buf; i+=2; }
                                    else if (hour12 && i + 1 < fmt.size()
                                             && ((std::tolower((unsigned char)fmt[i])=='a'
                                                  || std::tolower((unsigned char)fmt[i])=='p')
                                                 && std::tolower((unsigned char)fmt[i+1])=='m')) {
                                        out += (hi < 12) ? "AM" : "PM"; i += 2;
                                    }
                                    else { out += fmt[i]; ++i; }
                                }
                                return out;
                                };  // renderOne

                                if (dates.size() == 1) {
                                    outs[0] = Value::fromString(renderOne(dates[0]), mr);
                                } else {
                                    // Multi-date: one row per date, stacked into
                                    // an N x maxWidth char matrix (right-padded
                                    // with spaces), matching MATLAB datestr.
                                    std::vector<std::string> rowstr;
                                    rowstr.reserve(dates.size());
                                    size_t maxW = 0;
                                    for (const auto &c : dates) {
                                        rowstr.push_back(renderOne(c));
                                        if (rowstr.back().size() > maxW)
                                            maxW = rowstr.back().size();
                                    }
                                    const size_t N = rowstr.size();
                                    Value Mc = Value::matrix(N, maxW, ValueType::CHAR, mr);
                                    char *dst = static_cast<char *>(Mc.rawDataMut());
                                    for (size_t r = 0; r < N; ++r)
                                        for (size_t c = 0; c < maxW; ++c)
                                            dst[c * N + r] =   // column-major
                                                (c < rowstr[r].size()) ? rowstr[r][c] : ' ';
                                    outs[0] = Mc;
                                }
                            });

    engine.registerFunction("datevec",
                            [](Span<const Value> args,
                               size_t nargout,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "datevec requires at least one "
                                        "argument");
                                // String date input: datevec(str [, fmt]) —
                                // parse with an explicit format or auto-detect
                                // the common ISO / dd-mmm-yyyy forms (same
                                // parser as datenum), returning [Y M D H MI S].
                                if (args[0].isChar() || args[0].isString()) {
                                    auto *mrs = ctx.engine->resource();
                                    const std::string s = args[0].toString();
                                    static const char *MON3s[] = {
                                        "jan","feb","mar","apr","may","jun",
                                        "jul","aug","sep","oct","nov","dec"};
                                    auto tryFmt = [&](const std::string &fmt,
                                                      double &Y, double &Mo,
                                                      double &D, double &H,
                                                      double &MI, double &S) -> bool {
                                        Y = 0; Mo = 1; D = 1; H = 0; MI = 0; S = 0;
                                        size_t si = 0, fi = 0;
                                        auto readNum = [&](int maxD) -> long {
                                            long v = 0; int n = 0;
                                            while (si < s.size() && n < maxD
                                                   && std::isdigit(
                                                       static_cast<unsigned char>(s[si]))) {
                                                v = v * 10 + (s[si] - '0'); ++si; ++n;
                                            }
                                            return n > 0 ? v : -1;
                                        };
                                        while (fi < fmt.size()) {
                                            if (fmt.compare(fi, 4, "yyyy") == 0) {
                                                long v = readNum(4); if (v < 0) return false;
                                                Y = static_cast<double>(v); fi += 4;
                                            } else if (fmt.compare(fi, 4, "mmmm") == 0
                                                       || fmt.compare(fi, 3, "mmm") == 0) {
                                                bool full = fmt.compare(fi, 4, "mmmm") == 0;
                                                if (si + 3 > s.size()) return false;
                                                std::string mon = s.substr(si, 3);
                                                for (auto &c : mon)
                                                    c = static_cast<char>(std::tolower(
                                                        static_cast<unsigned char>(c)));
                                                int mi = -1;
                                                for (int k = 0; k < 12; ++k)
                                                    if (mon == MON3s[k]) { mi = k + 1; break; }
                                                if (mi < 0) return false;
                                                Mo = static_cast<double>(mi); si += 3;
                                                if (full) {
                                                    while (si < s.size() && std::isalpha(
                                                               static_cast<unsigned char>(s[si]))) ++si;
                                                    fi += 4;
                                                } else { fi += 3; }
                                            } else if (fmt.compare(fi, 2, "mm") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                Mo = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "dd") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                D = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "HH") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                H = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "MM") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                MI = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "SS") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                S = static_cast<double>(v); fi += 2;
                                            } else {
                                                if (si < s.size() && s[si] == fmt[fi]) { ++si; ++fi; }
                                                else return false;
                                            }
                                        }
                                        return si == s.size();
                                    };
                                    double Y, Mo, D, H, MI, S;
                                    bool ok = false;
                                    if (args.size() >= 2
                                        && (args[1].isChar() || args[1].isString())) {
                                        ok = tryFmt(args[1].toString(), Y, Mo, D, H, MI, S);
                                    } else {
                                        static const char *cands[] = {
                                            "yyyy-mm-dd HH:MM:SS", "yyyy-mm-dd",
                                            "dd-mmm-yyyy HH:MM:SS", "dd-mmm-yyyy"};
                                        for (const char *c : cands)
                                            if (tryFmt(c, Y, Mo, D, H, MI, S)) { ok = true; break; }
                                    }
                                    if (!ok)
                                        throw std::runtime_error(
                                            "datevec: could not parse date string "
                                            "(supported: explicit format string, or "
                                            "ISO yyyy-mm-dd and dd-mmm-yyyy forms)");
                                    const double vals[6] = {Y, Mo, D, H, MI, S};
                                    if (nargout <= 1) {
                                        auto out = Value::matrix(1, 6, ValueType::DOUBLE, mrs);
                                        double *o = out.doubleDataMut();
                                        for (int k = 0; k < 6; ++k) o[k] = vals[k];
                                        outs[0] = std::move(out);
                                    } else {
                                        for (int k = 0; k < 6
                                                        && k < static_cast<int>(nargout); ++k)
                                            outs[k] = Value::scalar(vals[k], mrs);
                                    }
                                    return;
                                }

                                auto civilFromDays = [](int64_t z,
                                                        int64_t &Y, int &M,
                                                        int &D) {
                                    z += 719468;
                                    const int64_t era =
                                        (z >= 0 ? z : z - 146096) / 146097;
                                    const int64_t doe = z - era * 146097;
                                    const int64_t yoe =
                                        (doe - doe / 1460 + doe / 36524
                                         - doe / 146096)
                                        / 365;
                                    const int64_t y = yoe + era * 400;
                                    const int64_t doy =
                                        doe - (365 * yoe + yoe / 4 - yoe / 100);
                                    const int64_t mp = (5 * doy + 2) / 153;
                                    D = static_cast<int>(
                                        doy - (153 * mp + 2) / 5 + 1);
                                    M = static_cast<int>(
                                        mp < 10 ? mp + 3 : mp - 9);
                                    Y = y + (M <= 2 ? 1 : 0);
                                };
                                auto extractTime = [](double frac, int &H,
                                                      int &MI, double &S) {
                                    // Round to milliseconds. Microsecond
                                    // rounding is at the FP-precision edge
                                    // for typical serial-date magnitudes
                                    // (~7e5 days -> ~7us absolute precision)
                                    // and shows up as +/-1us noise on round-
                                    // trips. Millisecond gives a comfortable
                                    // margin while still preserving MATLAB-
                                    // displayed fractional-second resolution.
                                    const double total_ms =
                                        std::round(frac * 86400.0 * 1.0e3);
                                    int64_t ms = static_cast<int64_t>(total_ms);
                                    H  = static_cast<int>(ms / 3600000LL);
                                    ms %= 3600000LL;
                                    MI = static_cast<int>(ms / 60000LL);
                                    ms %= 60000LL;
                                    S  = static_cast<double>(ms) / 1.0e3;
                                };
                                auto vecOf = [&](double dval, double *out6) {
                                    if (dval == 0.0) {
                                        for (int k = 0; k < 6; ++k)
                                            out6[k] = 0.0;
                                        return;
                                    }
                                    const double floored = std::floor(dval);
                                    const int64_t days =
                                        static_cast<int64_t>(floored);
                                    const double frac = dval - floored;
                                    const int64_t z = days - 719529;
                                    int64_t Y;
                                    int M, D, H, MI;
                                    double S;
                                    civilFromDays(z, Y, M, D);
                                    extractTime(frac, H, MI, S);
                                    // Carry from S/MI/H into D/M/Y if rounding
                                    // pushed seconds to 60.
                                    if (S >= 60.0) { S -= 60.0; ++MI; }
                                    if (MI >= 60)  { MI -= 60;  ++H;  }
                                    if (H  >= 24)  { H  -= 24;
                                        // Day rolled over -- recompute civil.
                                        civilFromDays(z + 1, Y, M, D);
                                    }
                                    out6[0] = static_cast<double>(Y);
                                    out6[1] = static_cast<double>(M);
                                    out6[2] = static_cast<double>(D);
                                    out6[3] = static_cast<double>(H);
                                    out6[4] = static_cast<double>(MI);
                                    out6[5] = S;
                                };

                                auto *mr = ctx.engine->resource();
                                const Value &Din = args[0];
                                const size_t N = Din.numel();

                                // Compute N x 6 output column-major.
                                auto out = Value::matrix(
                                    N, 6, ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                double tmp[6];
                                for (size_t i = 0; i < N; ++i) {
                                    vecOf(Din.elemAsDouble(i), tmp);
                                    for (int c = 0; c < 6; ++c)
                                        o[i + c * N] = tmp[c];
                                }

                                if (nargout <= 1) {
                                    outs[0] = std::move(out);
                                    return;
                                }
                                // 6-output form: separate column vectors
                                // (or scalars if N == 1).
                                for (int c = 0; c < 6
                                                && c < static_cast<int>(nargout);
                                     ++c) {
                                    if (N == 1) {
                                        outs[c] = Value::scalar(
                                            o[c * N], mr);
                                    } else {
                                        auto col = Value::matrix(
                                            N, 1, ValueType::DOUBLE, mr);
                                        double *p = col.doubleDataMut();
                                        for (size_t i = 0; i < N; ++i)
                                            p[i] = o[i + c * N];
                                        outs[c] = std::move(col);
                                    }
                                }
                            });

    // ── yyyymmdd ──────────────────────────────────────────────
    // Packed integer date: Y*10000 + M*100 + D from a MATLAB serial
    // date number. Output preserves input shape.
    //
    // EXTENSION vs MATLAB: MATLAB R2025b's yyyymmdd accepts only
    // datetime input (numkit has no datetime class yet). Accepting
    // a serial date number here matches the spirit of the function
    // and is the call most users want; the equivalent MATLAB call
    // is `yyyymmdd(datetime(d, 'ConvertFrom', 'datenum'))`. Parity
    // spec wraps the input with that conversion.
    engine.registerFunction("yyyymmdd",
                            [](Span<const Value> args,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "yyyymmdd requires one argument");
                                if (args[0].isChar() || args[0].isString())
                                    throw std::runtime_error(
                                        "yyyymmdd: string parsing not "
                                        "supported");
                                auto civilFromDays = [](int64_t z,
                                                        int64_t &Y, int &M,
                                                        int &D) {
                                    z += 719468;
                                    const int64_t era =
                                        (z >= 0 ? z : z - 146096) / 146097;
                                    const int64_t doe = z - era * 146097;
                                    const int64_t yoe =
                                        (doe - doe / 1460 + doe / 36524
                                         - doe / 146096)
                                        / 365;
                                    const int64_t y = yoe + era * 400;
                                    const int64_t doy =
                                        doe - (365 * yoe + yoe / 4 - yoe / 100);
                                    const int64_t mp = (5 * doy + 2) / 153;
                                    D = static_cast<int>(
                                        doy - (153 * mp + 2) / 5 + 1);
                                    M = static_cast<int>(
                                        mp < 10 ? mp + 3 : mp - 9);
                                    Y = y + (M <= 2 ? 1 : 0);
                                };
                                auto packOne = [&](double dval) {
                                    if (dval == 0.0) return 0.0;
                                    const int64_t days =
                                        static_cast<int64_t>(std::floor(dval));
                                    const int64_t z = days - 719529;
                                    int64_t Y;
                                    int M, D;
                                    civilFromDays(z, Y, M, D);
                                    return static_cast<double>(
                                        Y * 10000 + M * 100 + D);
                                };

                                auto *mr = ctx.engine->resource();
                                const Value &Din = args[0];
                                const size_t N = Din.numel();
                                if (N == 1) {
                                    outs[0] = Value::scalar(
                                        packOne(Din.toScalar()), mr);
                                    return;
                                }
                                auto out = Value::matrix(
                                    Din.dims().rows(), Din.dims().cols(),
                                    ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                for (size_t i = 0; i < N; ++i)
                                    o[i] = packOne(Din.elemAsDouble(i));
                                outs[0] = std::move(out);
                            });

    // ── mjuliandate ───────────────────────────────────────────
    // Modified Julian Date = JD - 2400000.5. MJD epoch is
    // 1858-11-17 00:00 (so mjuliandate(1858,11,17,0,0,0) = 0).
    //
    // Relationship to MATLAB serial date:
    //   MJD = serial + 1721058.5 - 2400000.5 = serial - 678942
    // both fractional offsets cancel exactly so noon at Y-M-D 00:00
    // gives a half-integer MJD only via the H,MI,S contribution.
    //
    // Signatures (string + datetime forms deferred):
    //   mjuliandate(Y, M, D)
    //   mjuliandate(Y, M, D, H, MI, S)
    //   mjuliandate(V)               with V row 1x3, 1x6, or matrix Nx3 / Nx6
    engine.registerFunction("mjuliandate",
                            [](Span<const Value> args,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "mjuliandate requires at least one "
                                        "argument");
                                if (args[0].isChar() || args[0].isString())
                                    throw std::runtime_error(
                                        "mjuliandate: string parsing not "
                                        "yet supported");

                                auto civilToSerial = [](double yd, double md,
                                                        double dd, double hd,
                                                        double mind, double sd) {
                                    double dInt;
                                    const double dFrac = std::modf(dd, &dInt);
                                    int64_t y = static_cast<int64_t>(
                                        std::floor(yd));
                                    int64_t m = static_cast<int64_t>(
                                        std::floor(md));
                                    int64_t d = static_cast<int64_t>(dInt);
                                    if (m <= 2) y -= 1;
                                    const int64_t era =
                                        (y < 0 ? y - 399 : y) / 400;
                                    const int64_t yoe = y - era * 400;
                                    const int64_t doy =
                                        (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5
                                        + d - 1;
                                    const int64_t doe =
                                        yoe * 365 + yoe / 4 - yoe / 100 + doy;
                                    const int64_t days =
                                        era * 146097 + doe - 719468;
                                    const double frac =
                                        (hd * 3600.0 + mind * 60.0 + sd) / 86400.0
                                        + dFrac;
                                    return static_cast<double>(days) + 719529.0
                                         + frac;
                                };
                                const double kMJDFromSerial = -678942.0;

                                auto *mr = ctx.engine->resource();

                                // Single-arg matrix form
                                if (args.size() == 1) {
                                    const Value &V = args[0];
                                    const size_t R = V.dims().rows();
                                    const size_t C = V.dims().cols();
                                    if (C != 3 && C != 6)
                                        throw std::runtime_error(
                                            "mjuliandate: single-arg matrix "
                                            "must have 3 or 6 columns");
                                    auto out = Value::matrix(
                                        R, 1, ValueType::DOUBLE, mr);
                                    double *o = out.doubleDataMut();
                                    for (size_t i = 0; i < R; ++i) {
                                        const double y = V.elemAsDouble(i);
                                        const double m = V.elemAsDouble(i + R);
                                        const double d = V.elemAsDouble(i + 2 * R);
                                        double h = 0.0, mi = 0.0, s = 0.0;
                                        if (C == 6) {
                                            h  = V.elemAsDouble(i + 3 * R);
                                            mi = V.elemAsDouble(i + 4 * R);
                                            s  = V.elemAsDouble(i + 5 * R);
                                        }
                                        o[i] = civilToSerial(y, m, d, h, mi, s)
                                             + kMJDFromSerial;
                                    }
                                    if (R == 1)
                                        outs[0] = Value::scalar(o[0], mr);
                                    else
                                        outs[0] = std::move(out);
                                    return;
                                }

                                if (args.size() != 3 && args.size() != 6)
                                    throw std::runtime_error(
                                        "mjuliandate: expected 3 or 6 "
                                        "arguments (Y,M,D[,H,MI,S])");
                                size_t N = 1;
                                for (const auto &a : args) {
                                    if (a.numel() > 1) {
                                        if (N > 1 && a.numel() != N)
                                            throw std::runtime_error(
                                                "mjuliandate: input vector "
                                                "lengths must match");
                                        N = a.numel();
                                    }
                                }
                                auto pick = [](const Value &a, size_t i) {
                                    return a.numel() == 1
                                               ? a.toScalar()
                                               : a.elemAsDouble(i);
                                };
                                if (N == 1) {
                                    const double h = args.size() == 6
                                                         ? args[3].toScalar()
                                                         : 0.0;
                                    const double mi = args.size() == 6
                                                          ? args[4].toScalar()
                                                          : 0.0;
                                    const double s = args.size() == 6
                                                         ? args[5].toScalar()
                                                         : 0.0;
                                    outs[0] = Value::scalar(
                                        civilToSerial(args[0].toScalar(),
                                                      args[1].toScalar(),
                                                      args[2].toScalar(),
                                                      h, mi, s)
                                            + kMJDFromSerial,
                                        mr);
                                    return;
                                }
                                auto out = Value::matrix(
                                    N, 1, ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                for (size_t i = 0; i < N; ++i) {
                                    const double y = pick(args[0], i);
                                    const double m = pick(args[1], i);
                                    const double d = pick(args[2], i);
                                    double h = 0.0, mi = 0.0, s = 0.0;
                                    if (args.size() == 6) {
                                        h  = pick(args[3], i);
                                        mi = pick(args[4], i);
                                        s  = pick(args[5], i);
                                    }
                                    o[i] = civilToSerial(y, m, d, h, mi, s)
                                         + kMJDFromSerial;
                                }
                                outs[0] = std::move(out);
                            });

    
}

} // namespace numkit::builtin
