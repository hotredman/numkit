// toolboxes/stats/src/test/ad_dw_detail.hpp — private compute/register shared surface:
// dwStatAndPLeft (Durbin-Watson stat + left p-value; def in ad_dw.cpp, external).
#pragma once
#include <nk/value/value.hpp>
#include <memory_resource>
namespace nk::stats {
double dwStatAndPLeft(const Value &r, const Value &X, bool exact,
                      double &dwOut, std::pmr::memory_resource *mr);
} // namespace nk::stats
