// libs/.../anova_detail.hpp — private compute substrate (anon-in-header,
// internal linkage per TU) shared by anova.cpp compute and anova_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::stats {

namespace {

// Bucket observations by group label (preserve first-seen order to match
// MATLAB-style ascending numeric order most of the time; for strict
// matches, callers can sort labels first).
struct Group {
    double label;
    std::vector<double> values;
};

std::vector<Group> bucket(const Value &y, const Value &group)
{
    const size_t N = y.numel();
    if (group.numel() != N)
        throw Error("anova1: y and group must be the same length",
                    0, 0, "anova1", "", "numkit:anova1:size");
    std::vector<Group> g;
    for (size_t i = 0; i < N; ++i) {
        const double yi = y.elemAsDouble(i);
        const double li = group.elemAsDouble(i);
        if (std::isnan(yi) || std::isnan(li)) continue;
        bool found = false;
        for (auto &gg : g) if (gg.label == li) { gg.values.push_back(yi); found = true; break; }
        if (!found) g.push_back({li, {yi}});
    }
    // Sort by label ascending — matches MATLAB's anova1 default ordering.
    std::sort(g.begin(), g.end(),
              [](const Group &a, const Group &b) { return a.label < b.label; });
    return g;
}

} // anonymous

// anova2: two-way ANOVA core (10-tuple of doubles: pCols,pRows,Fc,Fr,dfC,dfR,
// dfE,ssC,ssR,ssE). Def in anova.cpp (external).
std::tuple<double, double, double, double, double,
           double, double, double, double, double>
anova2(const Value &Y, std::pmr::memory_resource *mr);

} // namespace numkit::stats
