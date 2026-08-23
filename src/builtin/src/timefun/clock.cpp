// src/builtin/src/timefun/clock.cpp
//
// Clock, current date, CPU time, and execution pausing for numkit::builtin.

#include <numkit/builtin/timefun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <chrono>
#include <ctime>
#include <thread>

namespace numkit::builtin {

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

} // namespace numkit::builtin
