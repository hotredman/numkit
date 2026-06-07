// libs/stats/src/test/ad_dw_detail.hpp — private compute/register shared surface:
// dwStatAndPLeft (Durbin-Watson stat + left p-value; def in ad_dw.cpp, external).
#pragma once
#include <numkit/value/value.hpp>
#include <memory_resource>
namespace numkit::stats {
double dwStatAndPLeft(const Value &r, const Value &X, bool exact,
                      double &dwOut, std::pmr::memory_resource *mr);
} // namespace numkit::stats
