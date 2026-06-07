// libs/.../knnsearch_detail.hpp — private compute substrate (anon-in-header,
// internal linkage per TU) shared by knnsearch.cpp compute and knnsearch_reg.cpp.
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

// Parse positional/name-value args. Forms accepted:
//   knnsearch(X, Y)                     → K=1
//   knnsearch(X, Y, K)                  → K-NN, default metric
//   knnsearch(X, Y, 'K', K, ...)        → name-value
//   knnsearch(X, Y, 'Distance', metric, ...)
struct KnnOpts { int K = 1; std::string metric = "euclidean"; double p = 2.0; };

KnnOpts parse_knn_args(Span<const Value> args, const char *fn)
{
    KnnOpts o;
    if (args.size() == 3 && args[2].isNumeric() && args[2].isScalar()) {
        // legacy positional K
        o.K = static_cast<int>(args[2].toScalar());
        return o;
    }
    // name-value pairs
    size_t i = 2;
    while (i < args.size()) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error(std::string(fn) + ": expected name-value pair",
                        0, 0, fn, "", "numkit:knn:nvpair");
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (i + 1 >= args.size())
            throw Error(std::string(fn) + ": '" + name + "' missing value",
                        0, 0, fn, "", "numkit:knn:nvval");
        const Value &v = args[i + 1];
        if (name == "k") {
            o.K = static_cast<int>(v.toScalar());
        } else if (name == "distance") {
            if (!v.isChar() && !v.isString())
                throw Error(std::string(fn) + ": Distance must be a string",
                            0, 0, fn, "", "numkit:knn:dist");
            o.metric = v.toString();
        } else if (name == "p" || name == "minkowskiexponent") {
            o.p = v.toScalar();
        } else {
            // ignore unknown names (NSMethod, BucketSize, IncludeTies, …)
        }
        i += 2;
    }
    return o;
}

} // anonymous

} // namespace numkit::stats
