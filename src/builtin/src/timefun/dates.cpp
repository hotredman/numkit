// src/builtin/src/timefun/dates.cpp
//
// Date conversions, calendar operations, and formatting for numkit::builtin.

#include <numkit/builtin/timefun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace numkit::builtin {

static inline double civilToSerial(double yd, double md, double dd, double hd, double mind, double sd) {
    double dInt;
    const double dFrac = std::modf(dd, &dInt);
    int64_t y = static_cast<int64_t>(std::floor(yd));
    int64_t m = static_cast<int64_t>(std::floor(md));
    int64_t d = static_cast<int64_t>(dInt);
    if (m <= 2) y -= 1;
    const int64_t era = (y < 0 ? y - 399 : y) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const int64_t days = era * 146097 + doe - 719468;
    const double frac = (hd * 3600.0 + mind * 60.0 + sd) / 86400.0 + dFrac;
    return static_cast<double>(days) + 719529.0 + frac;
}

static inline void civilFromDays(int64_t z, int64_t &Y, int &M, int &D) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t y = yoe + era * 400;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    D = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    M = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    Y = y + (M <= 2 ? 1 : 0);
}

static inline void extractTime(double frac, int &H, int &MI, double &S) {
    const double total_ms = std::round(frac * 86400.0 * 1.0e3);
    int64_t ms = static_cast<int64_t>(total_ms);
    H  = static_cast<int>(ms / 3600000LL); ms %= 3600000LL;
    MI = static_cast<int>(ms / 60000LL);   ms %= 60000LL;
    S  = static_cast<double>(ms) / 1.0e3;
}

static inline void serialToCivil(double serial, double &yd, double &md, double &dd, double &hd, double &mind, double &sd) {
    if (serial == 0.0) {
        yd = 0; md = 0; dd = 0; hd = 0; mind = 0; sd = 0;
        return;
    }
    const double floored = std::floor(serial);
    const int64_t days = static_cast<int64_t>(floored);
    const double frac = serial - floored;
    const int64_t z = days - 719529;
    int64_t Y; int M, D, H, MI; double S;
    civilFromDays(z, Y, M, D);
    extractTime(frac, H, MI, S);
    if (S >= 60.0) { S -= 60.0; ++MI; }
    if (MI >= 60)  { MI -= 60;  ++H;  }
    if (H  >= 24)  { H  -= 24; civilFromDays(z + 1, Y, M, D); }
    yd = static_cast<double>(Y);
    md = static_cast<double>(M);
    dd = static_cast<double>(D);
    hd = static_cast<double>(H);
    mind = static_cast<double>(MI);
    sd = S;
}

static constexpr double kJDFromSerial  = 1721058.5;
static constexpr double kMJDFromSerial = -678942.0;

Value etime(const Value &t2, const Value &t1, std::pmr::memory_resource *mr) {
    const size_t r2 = t2.dims().rows(), c2 = t2.dims().cols();
    const size_t r1 = t1.dims().rows(), c1 = t1.dims().cols();
    if (c2 != 6 || c1 != 6)
        throw std::runtime_error("etime: date vectors must have 6 columns [Y M D H MI S]");
    if (r1 != r2 && r1 != 1 && r2 != 1)
        throw std::runtime_error("etime: t2 and t1 must have the same number of rows (or one a single row)");
    const size_t N = std::max(r1, r2);

    auto comp = [&](const Value &v, size_t nr, size_t row, size_t col) {
        return v.elemAsDouble(row + col * nr);
    };

    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    for (size_t k = 0; k < N; ++k) {
        const size_t k2 = (r2 == 1 ? 0 : k);
        const size_t k1 = (r1 == 1 ? 0 : k);
        const double dDay =
            civilToSerial(comp(t2, r2, k2, 0), comp(t2, r2, k2, 1), comp(t2, r2, k2, 2), 0, 0, 0)
          - civilToSerial(comp(t1, r1, k1, 0), comp(t1, r1, k1, 1), comp(t1, r1, k1, 2), 0, 0, 0);
        const double dH  = comp(t2, r2, k2, 3) - comp(t1, r1, k1, 3);
        const double dMI = comp(t2, r2, k2, 4) - comp(t1, r1, k1, 4);
        const double dS  = comp(t2, r2, k2, 5) - comp(t1, r1, k1, 5);
        o[k] = 86400.0 * dDay + 3600.0 * dH + 60.0 * dMI + dS;
    }
    return out;
}

Value weeknum(Span<const Value> args, std::pmr::memory_resource *mr) {
    if (args.empty())
        throw std::runtime_error("weeknum: requires a date argument");

    int weekStart = 1;
    if (args.size() >= 2 && !args[1].isEmpty())
        weekStart = static_cast<int>(args[1].toScalar());
    if (weekStart < 1 || weekStart > 7)
        throw std::runtime_error("weeknum: WeekStart must be an integer in 1..7 (1=Sunday)");
    bool european = false;
    if (args.size() >= 3 && !args[2].isEmpty())
        european = args[2].toScalar() != 0.0;

    auto civilToSerialSimple = [](int64_t y, int64_t m, int64_t d) {
        if (m <= 2) y -= 1;
        const int64_t era = (y < 0 ? y - 399 : y) / 400;
        const int64_t yoe = y - era * 400;
        const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468 + 719529;
    };
    auto yearOf = [](double serial) {
        int64_t z = static_cast<int64_t>(std::floor(serial)) - 719529 + 719468;
        const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        const int64_t doe = z - era * 146097;
        const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        int64_t y = yoe + era * 400;
        const int64_t dy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const int64_t mp = (5 * dy + 2) / 153;
        const int64_t m = mp < 10 ? mp + 3 : mp - 9;
        if (m <= 2) y += 1;
        return static_cast<int>(y);
    };
    auto isLeap = [](int y) {
        return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    };
    auto wdJan1 = [](int y) {
        static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        const int yy = y - 1;
        const int dow = ((yy + yy / 4 - yy / 100 + yy / 400 + t[0] + 1) % 7 + 7) % 7;
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
        const int doy = static_cast<int>(std::floor(serial) - civilToSerialSimple(y, 1, 1)) + 1;
        o[i] = static_cast<double>(weekOf(y, doy));
    }
    return out;
}

Value addtodate(const Value &d, double quantity, const std::string &unit, std::pmr::memory_resource *mr) {
    if (d.numel() != 1)
        throw std::runtime_error("addtodate: date number must be a real numeric scalar");
    const double serial = d.toScalar();
    const double q = quantity;
    std::string u = unit;
    for (auto &ch : u)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

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
        const double dayF = std::floor(serial);
        const double frac = serial - dayF;
        const int64_t days = static_cast<int64_t>(dayF) - 719529;
        int64_t z = days + 719468;
        const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        const int64_t doe = z - era * 146097;
        const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        int64_t Y = yoe + era * 400;
        const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const int64_t mp = (5 * doy + 2) / 153;
        const int64_t D = doy - (153 * mp + 2) / 5 + 1;
        int64_t M = mp < 10 ? mp + 3 : mp - 9;
        Y += (M <= 2);

        int64_t nY = Y, nM = M;
        if (u == "month") {
            int64_t tm = (M - 1) + static_cast<int64_t>(std::llround(q));
            int64_t qd = tm / 12, rd = tm % 12;
            if (rd < 0) { qd -= 1; rd += 12; }
            nY = Y + qd;
            nM = rd + 1;
        } else {
            nY = Y + static_cast<int64_t>(std::llround(q));
        }
        auto leap = [](int64_t y) {
            return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
        };
        static const int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int64_t maxD = dim[nM - 1];
        if (nM == 2 && leap(nY)) maxD = 29;
        int64_t nD = D < maxD ? D : maxD;
        int64_t y2 = nY;
        if (nM <= 2) y2 -= 1;
        const int64_t era2 = (y2 < 0 ? y2 - 399 : y2) / 400;
        const int64_t yoe2 = y2 - era2 * 400;
        const int64_t doy2 = (153 * (nM + (nM > 2 ? -3 : 9)) + 2) / 5 + nD - 1;
        const int64_t doe2 = yoe2 * 365 + yoe2 / 4 - yoe2 / 100 + doy2;
        const int64_t newDays = era2 * 146097 + doe2 - 719468 + 719529;
        result = static_cast<double>(newDays) + frac;
    } else {
        throw std::runtime_error("addtodate: units must be one of 'year','month','day','hour','minute','second','millisecond'");
    }
    return Value::scalar(result, mr);
}

Value datenum(Span<const Value> args, std::pmr::memory_resource *mr) {
    if (args.empty())
        throw std::runtime_error("datenum requires at least one argument");

    if (args[0].isChar() || args[0].isString()) {
        const std::string s = args[0].toString();
        static const char *MON3[] = {
            "jan","feb","mar","apr","may","jun",
            "jul","aug","sep","oct","nov","dec"};
        auto tryFmt = [&](const std::string &fmt, double &Y, double &Mo, double &D, double &H, double &MI, double &S) -> bool {
            Y = 0; Mo = 1; D = 1; H = 0; MI = 0; S = 0;
            size_t si = 0, fi = 0;
            auto readNum = [&](int maxD) -> long {
                long v = 0; int n = 0;
                while (si < s.size() && n < maxD && std::isdigit(static_cast<unsigned char>(s[si]))) {
                    v = v * 10 + (s[si] - '0'); ++si; ++n;
                }
                return n > 0 ? v : -1;
            };
            while (fi < fmt.size()) {
                if (fmt.compare(fi, 4, "yyyy") == 0) {
                    long v = readNum(4); if (v < 0) return false; Y = v; fi += 4;
                } else if (fmt.compare(fi, 2, "yy") == 0) {
                    long v = readNum(2); if (v < 0) return false;
                    time_t nowSec = time(nullptr);
                    tm tmNow{};
#if defined(_WIN32)
                    localtime_s(&tmNow, &nowSec);
#else
                    localtime_r(&nowSec, &tmNow);
#endif
                    long curCent = (1900 + tmNow.tm_year) / 100 * 100;
                    Y = curCent + v; fi += 2;
                } else if (fmt.compare(fi, 4, "mmmm") == 0 || fmt.compare(fi, 3, "mmm") == 0) {
                    size_t mlen = fmt.compare(fi, 4, "mmmm") == 0 ? 4 : 3;
                    if (si + 3 > s.size()) return false;
                    std::string m3;
                    for (int i = 0; i < 3; ++i) m3 += static_cast<char>(std::tolower(static_cast<unsigned char>(s[si + i])));
                    int mIdx = -1;
                    for (int i = 0; i < 12; ++i) { if (m3 == MON3[i]) { mIdx = i + 1; break; } }
                    if (mIdx < 0) return false;
                    Mo = mIdx; si += 3;
                    if (mlen == 4) { while (si < s.size() && std::isalpha(static_cast<unsigned char>(s[si]))) ++si; }
                    fi += mlen;
                } else if (fmt.compare(fi, 2, "mm") == 0) {
                    long v = readNum(2); if (v < 0) return false; Mo = v; fi += 2;
                } else if (fmt.compare(fi, 2, "dd") == 0) {
                    long v = readNum(2); if (v < 0) return false; D = v; fi += 2;
                } else if (fmt.compare(fi, 2, "HH") == 0) {
                    long v = readNum(2); if (v < 0) return false; H = v; fi += 2;
                } else if (fmt.compare(fi, 2, "MM") == 0) {
                    long v = readNum(2); if (v < 0) return false; MI = v; fi += 2;
                } else if (fmt.compare(fi, 2, "SS") == 0) {
                    long v = readNum(2); if (v < 0) return false; S = v; fi += 2;
                    if (si < s.size() && (s[si] == '.' || s[si] == ',')) {
                        ++si; double scale = 0.1;
                        while (si < s.size() && std::isdigit(static_cast<unsigned char>(s[si]))) {
                            S += (s[si] - '0') * scale; scale *= 0.1; ++si;
                        }
                    }
                } else {
                    if (si >= s.size() || s[si] != fmt[fi]) return false;
                    ++si; ++fi;
                }
            }
            while (si < s.size() && std::isspace(static_cast<unsigned char>(s[si]))) ++si;
            return si == s.size();
        };

        double Y, Mo, D, H, MI, S;
        if (args.size() >= 2 && (args[1].isChar() || args[1].isString())) {
            std::string fmt = args[1].toString();
            if (tryFmt(fmt, Y, Mo, D, H, MI, S))
                return Value::scalar(civilToSerial(Y, Mo, D, H, MI, S), mr);
            throw std::runtime_error("datenum: date string does not match format '" + fmt + "'");
        }
        static const char *kAutoFmts[] = {
            "yyyy-mm-dd HH:MM:SS", "yyyy-mm-ddTHH:MM:SS", "yyyy-mm-dd",
            "dd-mmm-yyyy HH:MM:SS", "dd-mmm-yyyy", "yyyy/mm/dd HH:MM:SS", "yyyy/mm/dd",
            "mm/dd/yyyy HH:MM:SS", "mm/dd/yyyy", "HH:MM:SS", "dd-mmmm-yyyy", nullptr
        };
        for (int i = 0; kAutoFmts[i]; ++i) {
            if (tryFmt(kAutoFmts[i], Y, Mo, D, H, MI, S))
                return Value::scalar(civilToSerial(Y, Mo, D, H, MI, S), mr);
        }
        throw std::runtime_error("datenum: could not auto-detect format of date string '" + s + "'");
    }

    if (args.size() == 1) {
        const Value &A = args[0];
        const size_t rows = A.dims().rows(), cols = A.dims().cols();
        if (cols == 6 || cols == 3) {
            auto out = Value::matrix(rows, 1, ValueType::DOUBLE, mr);
            double *o = out.doubleDataMut();
            for (size_t r = 0; r < rows; ++r) {
                double Y  = A.elemAsDouble(r);
                double Mo = A.elemAsDouble(r + rows);
                double D  = A.elemAsDouble(r + 2 * rows);
                double H = 0.0, MI = 0.0, S = 0.0;
                if (cols == 6) {
                    H  = A.elemAsDouble(r + 3 * rows);
                    MI = A.elemAsDouble(r + 4 * rows);
                    S  = A.elemAsDouble(r + 5 * rows);
                }
                o[r] = civilToSerial(Y, Mo, D, H, MI, S);
            }
            return out;
        }
        return A;
    }

    if (args.size() != 3 && args.size() != 6)
        throw std::runtime_error("datenum: expected 1, 3, or 6 arguments");

    size_t N = 1;
    for (const auto &a : args) {
        if (a.numel() > 1) {
            if (N > 1 && a.numel() != N)
                throw std::runtime_error("datenum: input vector lengths must match");
            N = a.numel();
        }
    }
    auto pick = [](const Value &a, size_t i) {
        return a.numel() == 1 ? a.toScalar() : a.elemAsDouble(i);
    };
    if (N == 1) {
        const double h = args.size() == 6 ? args[3].toScalar() : 0.0;
        const double mi = args.size() == 6 ? args[4].toScalar() : 0.0;
        const double s = args.size() == 6 ? args[5].toScalar() : 0.0;
        return Value::scalar(civilToSerial(args[0].toScalar(), args[1].toScalar(), args[2].toScalar(), h, mi, s), mr);
    }
    auto out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
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
    return out;
}

Value weekday(const Value &d, std::pmr::memory_resource *mr) {
    auto dayIndex = [](double val) -> int {
        int64_t f = static_cast<int64_t>(std::floor(val)) - 2;
        int64_t r = f % 7;
        if (r < 0) r += 7;
        return static_cast<int>(r) + 1;
    };
    const size_t N = d.numel();
    if (N == 1) {
        return Value::scalar(static_cast<double>(dayIndex(d.toScalar())), mr);
    }
    auto out = Value::matrix(d.dims().rows(), d.dims().cols(), ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i)
        o[i] = static_cast<double>(dayIndex(d.elemAsDouble(i)));
    return out;
}

Value juliandate(Span<const Value> args, std::pmr::memory_resource *mr) {
    if (args.empty())
        throw std::runtime_error("juliandate requires at least one argument");
    if (args.size() == 1) {
        const Value &A = args[0];
        if (A.dims().cols() == 6 || A.dims().cols() == 3) {
            Value s = datenum(args, mr);
            double *p = s.doubleDataMut();
            for (size_t i = 0; i < s.numel(); ++i) p[i] += kJDFromSerial;
            return s;
        }
        const size_t R = A.dims().rows(), C = A.dims().cols();
        auto out = Value::matrix(R, C, ValueType::DOUBLE, mr);
        double *o = out.doubleDataMut();
        for (size_t i = 0; i < R * C; ++i) o[i] = A.elemAsDouble(i) + kJDFromSerial;
        return R == 1 && C == 1 ? Value::scalar(o[0], mr) : out;
    }
    Value s = datenum(args, mr);
    double *p = s.doubleDataMut();
    for (size_t i = 0; i < s.numel(); ++i) p[i] += kJDFromSerial;
    return s;
}

Value mjuliandate(Span<const Value> args, std::pmr::memory_resource *mr) {
    if (args.empty())
        throw std::runtime_error("mjuliandate requires at least one argument");
    if (args.size() == 1) {
        const Value &A = args[0];
        if (A.dims().cols() == 6 || A.dims().cols() == 3) {
            Value s = datenum(args, mr);
            double *p = s.doubleDataMut();
            for (size_t i = 0; i < s.numel(); ++i) p[i] += kMJDFromSerial;
            return s;
        }
        const size_t R = A.dims().rows(), C = A.dims().cols();
        auto out = Value::matrix(R, C, ValueType::DOUBLE, mr);
        double *o = out.doubleDataMut();
        for (size_t i = 0; i < R * C; ++i) o[i] = A.elemAsDouble(i) + kMJDFromSerial;
        return R == 1 && C == 1 ? Value::scalar(o[0], mr) : out;
    }
    Value s = datenum(args, mr);
    double *p = s.doubleDataMut();
    for (size_t i = 0; i < s.numel(); ++i) p[i] += kMJDFromSerial;
    return s;
}

Value eomday(const Value &y, const Value &m, std::pmr::memory_resource *mr) {
    auto isLeap = [](int64_t yr) {
        return (yr % 4 == 0 && yr % 100 != 0) || yr % 400 == 0;
    };
    static const int dim[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    auto eomOne = [&](double yr, double mo) -> double {
        int64_t yi = static_cast<int64_t>(yr);
        int64_t mi = static_cast<int64_t>(mo);
        if (mi < 1 || mi > 12)
            throw std::runtime_error("eomday: month must be in 1..12");
        return (mi == 2 && isLeap(yi)) ? 29.0 : static_cast<double>(dim[mi]);
    };
    const size_t ny = y.numel(), nm = m.numel();
    if (ny == 1 && nm == 1)
        return Value::scalar(eomOne(y.toScalar(), m.toScalar()), mr);
    if (ny != nm && ny != 1 && nm != 1)
        throw std::runtime_error("eomday: input dimensions must agree");
    const size_t N = std::max(ny, nm);
    const size_t rows = ny > 1 ? y.dims().rows() : m.dims().rows();
    const size_t cols = ny > 1 ? y.dims().cols() : m.dims().cols();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        double yi = ny == 1 ? y.toScalar() : y.elemAsDouble(i);
        double mi = nm == 1 ? m.toScalar() : m.elemAsDouble(i);
        o[i] = eomOne(yi, mi);
    }
    return out;
}

Value calendar(Span<const Value> args, std::pmr::memory_resource *mr) {
    double yr = 0, mo = 0;
    if (args.empty()) {
        auto now_tp = std::chrono::system_clock::now();
        time_t t = std::chrono::system_clock::to_time_t(now_tp);
        std::tm tmNow{};
#if defined(_WIN32)
        localtime_s(&tmNow, &t);
#else
        localtime_r(&t, &tmNow);
#endif
        yr = 1900.0 + tmNow.tm_year;
        mo = 1.0 + tmNow.tm_mon;
    } else if (args.size() == 1) {
        double d, h, mi, s;
        serialToCivil(args[0].toScalar(), yr, mo, d, h, mi, s);
    } else {
        yr = args[0].toScalar();
        mo = args[1].toScalar();
    }
    int64_t yi = static_cast<int64_t>(yr);
    int64_t mi = static_cast<int64_t>(mo);
    if (mi < 1 || mi > 12)
        throw std::runtime_error("calendar: month must be in 1..12");
    auto isLeap = [](int64_t y) {
        return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    };
    static const int dim[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int daysInMonth = (mi == 2 && isLeap(yi)) ? 29 : dim[mi];
    double s1 = civilToSerial(static_cast<double>(yi), static_cast<double>(mi), 1.0, 0, 0, 0);
    int64_t f = static_cast<int64_t>(std::floor(s1)) - 2;
    int64_t r = f % 7; if (r < 0) r += 7;
    int startCol = static_cast<int>(r);

    auto out = Value::matrix(6, 7, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    std::fill(o, o + 42, 0.0);
    int day = 1;
    for (int col = startCol; col < 7 && day <= daysInMonth; ++col) {
        o[0 + col * 6] = day++;
    }
    for (int row = 1; row < 6 && day <= daysInMonth; ++row) {
        for (int col = 0; col < 7 && day <= daysInMonth; ++col) {
            o[row + col * 6] = day++;
        }
    }
    return out;
}

Value datestr(Span<const Value> args, std::pmr::memory_resource *mr) {
    if (args.empty()) {
        const double nowSerial = now();
        Value s = Value::scalar(nowSerial, mr);
        return datestr(Span<const Value>(&s, 1), mr);
    }
    const Value &din = args[0];

    struct Comp { int y, mo, d, h, mi, s; };
    auto serialToComp = [&](double dval) -> Comp {
        const double floored = std::floor(dval);
        const int64_t z = static_cast<int64_t>(floored) - 719529;
        const double frac = dval - floored;
        int64_t Y; int M, D;
        civilFromDays(z, Y, M, D);
        int64_t ms = static_cast<int64_t>(std::round(frac * 86400.0 * 1.0e3));
        int H  = static_cast<int>(ms / 3600000LL); ms %= 3600000LL;
        int MI = static_cast<int>(ms / 60000LL);   ms %= 60000LL;
        double S = static_cast<double>(ms) / 1.0e3;
        if (S >= 60.0) { S -= 60.0; ++MI; }
        if (MI >= 60)  { MI -= 60;  ++H;  }
        if (H  >= 24)  { H  -= 24; civilFromDays(z + 1, Y, M, D); }
        return Comp{ static_cast<int>(Y), M, D, H, MI, static_cast<int>(std::round(S)) };
    };

    std::vector<Comp> dates;
    if (din.isChar() || din.isString()) {
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
                while (si < s.size() && n < maxD && std::isdigit(static_cast<unsigned char>(s[si]))) {
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
                "datestr: could not parse date string (supported: ISO yyyy-mm-dd and dd-mmm-yyyy forms)");
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
            const int code = static_cast<int>(std::round(f.elemAsDouble(0)));
            if (code < 0 || code > 31)
                throw std::runtime_error("datestr: unsupported numeric format code (expected 0-31)");
            fmt = DATEFORM[code];
        }
    } else {
        fmt = anyTime ? "dd-mmm-yyyy HH:MM:SS" : "dd-mmm-yyyy";
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
    static const int dt[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    bool hour12 = false;
    for (size_t k = 0; k + 1 < fmt.size(); ++k) {
        char a = (char)std::tolower((unsigned char)fmt[k]);
        char b = (char)std::tolower((unsigned char)fmt[k+1]);
        if ((a == 'a' || a == 'p') && b == 'm') { hour12 = true; break; }
    }

    auto renderOne = [&](const Comp &cc) -> std::string {
        const int yi = cc.y, moi = cc.mo, di = cc.d,
                  hi = cc.h, mii = cc.mi, si = cc.s;
        int yw = yi - (moi < 3 ? 1 : 0);
        int dow = ((yw + yw/4 - yw/100 + yw/400 + dt[(moi - 1 + 12) % 12] + di) % 7 + 7) % 7;
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
            else if (at("m", 1)) { out += MON3[(moi-1+12)%12][0]; i+=1; }
            else if (at("QQ", 2)) { out += 'Q'; out += static_cast<char>('0' + ((moi - 1) / 3 + 1)); i+=2; }
            else if (at("dddd", 4)) { out += DOWF[dow]; i+=4; }
            else if (at("ddd", 3)) { out += DOW3[dow]; i+=3; }
            else if (at("dd", 2)) { std::snprintf(buf,sizeof buf,"%02d",di); out+=buf; i+=2; }
            else if (at("d", 1)) { out += DOW3[dow][0]; i+=1; }
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
    };

    if (dates.size() == 1) {
        return Value::fromString(renderOne(dates[0]), mr);
    }
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
            dst[c * N + r] = (c < rowstr[r].size()) ? rowstr[r][c] : ' ';
    return Mc;
}

Value datevec(Span<const Value> args, std::pmr::memory_resource *mr) {
    if (args.empty())
        throw std::runtime_error("datevec requires at least one argument");
    if (args[0].isChar() || args[0].isString()) {
        const std::string s = args[0].toString();
        static const char *MON3s[] = {
            "jan","feb","mar","apr","may","jun",
            "jul","aug","sep","oct","nov","dec"};
        auto tryFmt = [&](const std::string &fmt, double &Y, double &Mo, double &D, double &H, double &MI, double &S) -> bool {
            Y = 0; Mo = 1; D = 1; H = 0; MI = 0; S = 0;
            size_t si = 0, fi = 0;
            auto readNum = [&](int maxD) -> long {
                long v = 0; int n = 0;
                while (si < s.size() && n < maxD && std::isdigit(static_cast<unsigned char>(s[si]))) {
                    v = v * 10 + (s[si] - '0'); ++si; ++n;
                }
                return n > 0 ? v : -1;
            };
            while (fi < fmt.size()) {
                if (fmt.compare(fi, 4, "yyyy") == 0) {
                    long v = readNum(4); if (v < 0) return false; Y = static_cast<double>(v); fi += 4;
                } else if (fmt.compare(fi, 4, "mmmm") == 0 || fmt.compare(fi, 3, "mmm") == 0) {
                    bool full = fmt.compare(fi, 4, "mmmm") == 0;
                    if (si + 3 > s.size()) return false;
                    std::string mon = s.substr(si, 3);
                    for (auto &c : mon) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    int mi = -1;
                    for (int k = 0; k < 12; ++k) if (mon == MON3s[k]) { mi = k + 1; break; }
                    if (mi < 0) return false;
                    Mo = static_cast<double>(mi); si += 3;
                    if (full) {
                        while (si < s.size() && std::isalpha(static_cast<unsigned char>(s[si]))) ++si;
                        fi += 4;
                    } else { fi += 3; }
                } else if (fmt.compare(fi, 2, "mm") == 0) {
                    long v = readNum(2); if (v < 0) return false; Mo = static_cast<double>(v); fi += 2;
                } else if (fmt.compare(fi, 2, "dd") == 0) {
                    long v = readNum(2); if (v < 0) return false; D = static_cast<double>(v); fi += 2;
                } else if (fmt.compare(fi, 2, "HH") == 0) {
                    long v = readNum(2); if (v < 0) return false; H = static_cast<double>(v); fi += 2;
                } else if (fmt.compare(fi, 2, "MM") == 0) {
                    long v = readNum(2); if (v < 0) return false; MI = static_cast<double>(v); fi += 2;
                } else if (fmt.compare(fi, 2, "SS") == 0) {
                    long v = readNum(2); if (v < 0) return false; S = static_cast<double>(v); fi += 2;
                } else {
                    if (si < s.size() && s[si] == fmt[fi]) { ++si; ++fi; }
                    else return false;
                }
            }
            return si == s.size();
        };
        double Y, Mo, D, H, MI, S;
        bool ok = false;
        if (args.size() >= 2 && (args[1].isChar() || args[1].isString())) {
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
                "datevec: could not parse date string (supported: explicit format string, or ISO yyyy-mm-dd and dd-mmm-yyyy forms)");
        const double vals[6] = {Y, Mo, D, H, MI, S};
        auto out = Value::matrix(1, 6, ValueType::DOUBLE, mr);
        double *o = out.doubleDataMut();
        for (int k = 0; k < 6; ++k) o[k] = vals[k];
        return out;
    }

    const Value &Din = args[0];
    const size_t N = Din.numel();
    auto out = Value::matrix(N, 6, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    double tmp[6];
    for (size_t i = 0; i < N; ++i) {
        serialToCivil(Din.elemAsDouble(i), tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5]);
        for (int c = 0; c < 6; ++c)
            o[i + c * N] = tmp[c];
    }
    return out;
}

Value yyyymmdd(Span<const Value> args, std::pmr::memory_resource *mr) {
    if (args.empty())
        throw std::runtime_error("yyyymmdd requires one argument");
    if (args[0].isChar() || args[0].isString())
        throw std::runtime_error("yyyymmdd: string parsing not supported");

    auto packOne = [&](double dval) {
        if (dval == 0.0) return 0.0;
        const int64_t days = static_cast<int64_t>(std::floor(dval));
        const int64_t z = days - 719529;
        int64_t Y; int M, D;
        civilFromDays(z, Y, M, D);
        return static_cast<double>(Y * 10000 + M * 100 + D);
    };

    const Value &Din = args[0];
    const size_t N = Din.numel();
    if (N == 1) {
        return Value::scalar(packOne(Din.toScalar()), mr);
    }
    auto out = Value::matrix(Din.dims().rows(), Din.dims().cols(), ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i)
        o[i] = packOne(Din.elemAsDouble(i));
    return out;
}

} // namespace numkit::builtin
