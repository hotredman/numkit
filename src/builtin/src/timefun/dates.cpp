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

static inline void serialToCivil(double serial, double &yd, double &md, double &dd, double &hd, double &mind, double &sd) {
    double intPart;
    double frac = std::modf(serial, &intPart);
    if (frac < 0.0) {
        frac += 1.0;
        intPart -= 1.0;
    }
    int64_t z = static_cast<int64_t>(intPart) - 719529 + 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = yoe + era * 400;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    const int64_t d = doy - (153 * mp + 2) / 5 + 1;
    const int64_t m = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2) y += 1;

    yd = static_cast<double>(y);
    md = static_cast<double>(m);
    dd = static_cast<double>(d);

    double totalSec = frac * 86400.0;
    double h = std::floor(totalSec / 3600.0);
    totalSec -= h * 3600.0;
    double mi = std::floor(totalSec / 60.0);
    double s = totalSec - mi * 60.0;

    hd = h;
    mind = mi;
    sd = s;
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
        double y, m, day, h, mi, s;
        serialToCivil(serial, y, m, day, h, mi, s);
        int64_t iy = static_cast<int64_t>(y);
        int64_t im = static_cast<int64_t>(m);
        if (u == "year")
            iy += static_cast<int64_t>(q);
        else {
            int64_t totalM = (iy * 12 + (im - 1)) + static_cast<int64_t>(q);
            iy = totalM >= 0 ? totalM / 12 : (totalM - 11) / 12;
            im = (totalM % 12 + 12) % 12 + 1;
        }
        auto isLeap = [](int64_t yr) {
            return (yr % 4 == 0 && yr % 100 != 0) || yr % 400 == 0;
        };
        static const int dim[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int maxDay = (im == 2 && isLeap(iy)) ? 29 : dim[im];
        double newDay = std::min(day, static_cast<double>(maxDay));
        result = civilToSerial(static_cast<double>(iy), static_cast<double>(im), newDay, h, mi, s);
    } else {
        throw std::runtime_error("addtodate: unrecognized time unit '" + u + "'");
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
        double Y, Mo, D, H, MI, S;
        serialToCivil(nowSerial, Y, Mo, D, H, MI, S);
        static const char *MON3[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%02d-%s-%04d %02d:%02d:%02d",
                      static_cast<int>(D), MON3[static_cast<int>(Mo) - 1], static_cast<int>(Y),
                      static_cast<int>(H), static_cast<int>(MI), static_cast<int>(S));
        return Value::fromString(std::string(buf), mr);
    }
    const Value &D_in = args[0];
    std::string fmt = "dd-mmm-yyyy HH:MM:SS";
    if (args.size() >= 2) {
        if (args[1].isChar() || args[1].isString())
            fmt = args[1].toString();
        else if (args[1].isScalar()) {
            int code = static_cast<int>(args[1].toScalar());
            switch (code) {
                case 0: fmt = "dd-mmm-yyyy HH:MM:SS"; break;
                case 1: fmt = "dd-mmm-yyyy"; break;
                case 2: fmt = "mm/dd/yy"; break;
                case 6: fmt = "mm/dd"; break;
                case 10: fmt = "yyyy"; break;
                case 13: fmt = "HH:MM:SS"; break;
                case 20: fmt = "dd/mm/yyyy"; break;
                case 23: fmt = "mm/dd/yyyy"; break;
                case 26: fmt = "yyyy/mm/dd"; break;
                case 29: fmt = "yyyy-mm-dd"; break;
                case 30: fmt = "yyyymmddTHHMMSS"; break;
                case 31: fmt = "yyyy-mm-dd HH:MM:SS"; break;
                default: fmt = "dd-mmm-yyyy"; break;
            }
        }
    }
    static const char *MON3[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    auto formatOne = [&](double serial) -> std::string {
        double Y, Mo, D, H, MI, S;
        serialToCivil(serial, Y, Mo, D, H, MI, S);
        std::string res;
        for (size_t fi = 0; fi < fmt.size(); ) {
            if (fmt.compare(fi, 4, "yyyy") == 0) {
                char b[8]; std::snprintf(b, sizeof(b), "%04d", static_cast<int>(Y));
                res += b; fi += 4;
            } else if (fmt.compare(fi, 2, "yy") == 0) {
                char b[8]; std::snprintf(b, sizeof(b), "%02d", static_cast<int>(Y) % 100);
                res += b; fi += 2;
            } else if (fmt.compare(fi, 3, "mmm") == 0) {
                res += MON3[std::clamp(static_cast<int>(Mo) - 1, 0, 11)];
                fi += 3;
            } else if (fmt.compare(fi, 2, "mm") == 0) {
                char b[8]; std::snprintf(b, sizeof(b), "%02d", static_cast<int>(Mo));
                res += b; fi += 2;
            } else if (fmt.compare(fi, 2, "dd") == 0) {
                char b[8]; std::snprintf(b, sizeof(b), "%02d", static_cast<int>(D));
                res += b; fi += 2;
            } else if (fmt.compare(fi, 2, "HH") == 0) {
                char b[8]; std::snprintf(b, sizeof(b), "%02d", static_cast<int>(H));
                res += b; fi += 2;
            } else if (fmt.compare(fi, 2, "MM") == 0) {
                char b[8]; std::snprintf(b, sizeof(b), "%02d", static_cast<int>(MI));
                res += b; fi += 2;
            } else if (fmt.compare(fi, 2, "SS") == 0) {
                char b[8]; std::snprintf(b, sizeof(b), "%02d", static_cast<int>(S));
                res += b; fi += 2;
            } else {
                res += fmt[fi++];
            }
        }
        return res;
    };

    if (D_in.numel() == 1) {
        return Value::fromString(formatOne(D_in.toScalar()), mr);
    }
    const size_t N = D_in.numel();
    auto c = Value::cell(N, 1, mr);
    for (size_t i = 0; i < N; ++i) {
        c.cellAt(i) = Value::fromString(formatOne(D_in.elemAsDouble(i)), mr);
    }
    return c;
}

Value datevec(Span<const Value> args, std::pmr::memory_resource *mr) {
    if (args.empty())
        throw std::runtime_error("datevec requires at least one argument");
    if (args[0].isChar() || args[0].isString()) {
        Value dnum = datenum(args, mr);
        double Y, Mo, D, H, MI, S;
        serialToCivil(dnum.toScalar(), Y, Mo, D, H, MI, S);
        auto out = Value::matrix(1, 6, ValueType::DOUBLE, mr);
        double *o = out.doubleDataMut();
        o[0] = Y; o[1] = Mo; o[2] = D; o[3] = H; o[4] = MI; o[5] = S;
        return out;
    }
    const Value &D_in = args[0];
    const size_t N = D_in.numel();
    auto out = Value::matrix(N, 6, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        double Y, Mo, D, H, MI, S;
        serialToCivil(D_in.elemAsDouble(i), Y, Mo, D, H, MI, S);
        o[i + 0 * N] = Y;
        o[i + 1 * N] = Mo;
        o[i + 2 * N] = D;
        o[i + 3 * N] = H;
        o[i + 4 * N] = MI;
        o[i + 5 * N] = S;
    }
    return out;
}

Value yyyymmdd(Span<const Value> args, std::pmr::memory_resource *mr) {
    if (args.empty())
        throw std::runtime_error("yyyymmdd requires at least one argument");
    const Value &D_in = args[0];
    const size_t N = D_in.numel();
    auto out = Value::matrix(D_in.dims().rows(), D_in.dims().cols(), ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        double Y, Mo, D, H, MI, S;
        serialToCivil(D_in.elemAsDouble(i), Y, Mo, D, H, MI, S);
        o[i] = Y * 10000.0 + Mo * 100.0 + D;
    }
    return D_in.numel() == 1 ? Value::scalar(o[0], mr) : out;
}

} // namespace numkit::builtin
