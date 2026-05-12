// libs/stats/include/numkit/stats/lda/lda.hpp
//
// Linear / Quadratic Discriminant Analysis — function form.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit::stats {

/// `[c, err, posterior, logp] = classify(sample, training, group[, type])`
///   sample    — Nsamp × d points to classify
///   training  — Ntrn  × d training data
///   group     — Ntrn × 1 vector of class labels (numeric)
///   type      — "linear" (default, LDA pooled cov) or
///               "diaglinear" (LDA with diagonal pooled cov) or
///               "quadratic" (QDA, separate cov per class) or
///               "diagquadratic" / "mahalanobis"
///
/// Returns labels (Nsamp × 1), apparent training error rate, posterior
/// probabilities (Nsamp × K), and log unconditional density (Nsamp × 1).
/// Prior assumed empirical (n_k / N).
std::tuple<Value, Value, Value, Value>
classify(const Value &sample, const Value &training, const Value &group, const std::string &type, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
