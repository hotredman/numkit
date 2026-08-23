/// @file lda.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/lda/lda.hpp
//
// Linear / Quadratic Discriminant Analysis — function form.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>
#include <tuple>

namespace numkit::stats {

/// @addtogroup group_stats
/// @{


/// @brief Classify samples via LDA / QDA
/// (`[c, err, posterior, logp] = classify(sample, training, group, type)`).
///
/// Fits a discriminant model on `(training, group)` and assigns labels
/// to each row of `sample`. Empirical priors (`n_k / N`).
///
/// Supported `type`:
/// - `"linear"`        — LDA with pooled covariance (default)
/// - `"diaglinear"`    — LDA with diagonal pooled covariance (naive Bayes)
/// - `"quadratic"`     — QDA with per-class covariance
/// - `"diagquadratic"` — QDA with per-class diagonal covariance
/// - `"mahalanobis"`   — class-pooled Mahalanobis distance
///
/// @param sample    `N_samp × d` points to classify.
/// @param training  `N_trn × d` training data.
/// @param group     `N_trn × 1` numeric class labels.
/// @param type      Discriminant model — see list above.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Tuple `(c, err, posterior, logp)`:
///                  - `c`         : `N_samp × 1` predicted labels.
///                  - `err`       : apparent training error rate.
///                  - `posterior` : `N_samp × K` posterior probabilities.
///                  - `logp`      : `N_samp × 1` log unconditional density.
/// @see pca
std::tuple<Value, Value, Value, Value>
classify(const Value &sample, const Value &training, const Value &group,
         const std::string &type,
         std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::stats
