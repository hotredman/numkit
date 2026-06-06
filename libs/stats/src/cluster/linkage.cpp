// libs/stats/src/cluster/linkage.cpp

#include <numkit/stats/cluster/linkage.hpp>

#include <numkit/stats/cluster/distance.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <memory_resource>

namespace numkit::stats {

namespace {

enum class LinkMethod {
    Single, Complete, Average, Weighted, Centroid, Median, Ward
};

LinkMethod parse_link(const std::string &s) {
    if (s == "single")   return LinkMethod::Single;
    if (s == "complete") return LinkMethod::Complete;
    if (s == "average")  return LinkMethod::Average;
    if (s == "weighted") return LinkMethod::Weighted;
    if (s == "centroid") return LinkMethod::Centroid;
    if (s == "median")   return LinkMethod::Median;
    if (s == "ward")     return LinkMethod::Ward;
    return LinkMethod::Single;
}

// Read pdist row vector into a square matrix D (size N×N).
// Y has length N(N-1)/2; D is size N (column-major).
size_t solve_N_from_pdist(size_t k) {
    const double Nf = (1.0 + std::sqrt(1.0 + 8.0 * double(k))) / 2.0;
    return (size_t)std::round(Nf);
}

// Lance-Williams update: d(c, k) where c = merge(a, b).
inline double lance_williams(LinkMethod m, double d_ak, double d_bk, double d_ab,
                             int n_a, int n_b, int n_k) {
    switch (m) {
        case LinkMethod::Single:   return std::min(d_ak, d_bk);
        case LinkMethod::Complete: return std::max(d_ak, d_bk);
        case LinkMethod::Average:
            return (n_a * d_ak + n_b * d_bk) / double(n_a + n_b);
        case LinkMethod::Weighted: return 0.5 * (d_ak + d_bk);
        case LinkMethod::Centroid: {
            const double na = n_a, nb = n_b;
            return std::sqrt(
                (na * d_ak * d_ak + nb * d_bk * d_bk) / (na + nb)
                - na * nb * d_ab * d_ab / ((na + nb) * (na + nb)));
        }
        case LinkMethod::Median:
            return std::sqrt(0.5 * d_ak * d_ak + 0.5 * d_bk * d_bk
                             - 0.25 * d_ab * d_ab);
        case LinkMethod::Ward: {
            const double na = n_a, nb = n_b, nk = n_k;
            const double tot = na + nb + nk;
            return std::sqrt(
                ((na + nk) * d_ak * d_ak + (nb + nk) * d_bk * d_bk
                 - nk * d_ab * d_ab) / tot);
        }
    }
    return 0.0;
}

} // anonymous

Value linkage(const Value &Y, const std::string &method, const std::string &metric, double p, std::pmr::memory_resource *mr)
{
    const LinkMethod m = parse_link(method);
    ScratchArena scratch(mr);

    // Determine N. Y is either pdist row (1×n*(n-1)/2) or NxD raw data.
    const size_t Yrows = Y.dims().rows();
    const size_t Ycols = Y.dims().cols();
    size_t N;
    ScratchVec<double> D(&scratch);  // upper-tri row-major

    if (Yrows == 1 || Ycols == 1) {
        const size_t n = Y.numel();
        N = solve_N_from_pdist(n);
        if (N * (N - 1) / 2 != n)
            throw Error("linkage: pdist input has invalid length",
                        0, 0, "linkage", "", "numkit:linkage:size");
        D.resize(n);
        for (size_t i = 0; i < n; ++i) D[i] = Y.elemAsDouble(i);
    } else {
        // Treat as raw data: compute pdist with the requested metric.
        Value Yp = pdist(Y, metric, p, mr);
        const size_t n = Yp.numel();
        N = Yrows;
        if (N * (N - 1) / 2 != n)
            throw Error("linkage: bad pdist intermediate", 0, 0, "linkage", "",
                        "numkit:linkage:internal");
        D.resize(n);
        for (size_t i = 0; i < n; ++i) D[i] = Yp.elemAsDouble(i);
    }

    if (N < 2) return Value::matrix(0, 3, ValueType::DOUBLE, mr);

    // Build a dense N×N distance matrix as flat row-major buffer.
    auto pdist_idx = [&](size_t i, size_t j) {
        if (i > j) std::swap(i, j);
        // pdist convention: index of pair (i, j) i<j among rows of an N×N
        // matrix in column-major order matching MATLAB's pdist:
        //   k = (i-1)*(N - i/2) + (j - i) - 1   (1-indexed)
        // 0-based: k = i*(N - 1) - i*(i+1)/2 + (j - i - 1)
        return i * N - (i * (i + 1)) / 2 + (j - i - 1);
    };

    // dm is N×N flat row-major; we keep it that way and just track an
    // active-row mask + count to avoid per-step erase shuffles.
    ScratchVec<double> dm(N * N, 0.0, &scratch);
    for (size_t i = 0; i < N; ++i)
        for (size_t j = i + 1; j < N; ++j) {
            const double d = D[pdist_idx(i, j)];
            dm[i * N + j] = d;
            dm[j * N + i] = d;
        }

    // Active cluster set.
    ScratchVec<int> ids(N, &scratch);
    ScratchVec<int> sizes(N, 1, &scratch);
    for (size_t i = 0; i < N; ++i) ids[i] = (int)i;  // 0-based; emit 1-based later

    ScratchVec<double> Zflat(&scratch);
    Zflat.reserve(3 * (N - 1));
    ScratchVec<double> newrow(N, &scratch);  // re-used per step

    int next_id = (int)N;
    for (size_t step = 0; step + 1 < N; ++step) {
        // Find smallest distance among active rows. On ties, prefer the
        // pair with the largest (i, j) lex-order — MATLAB R2025b's
        // linkage() picks ties this way (verified by probe), so we use
        // `<=` instead of `<` to keep the last-scanned minimum.
        const size_t M = ids.size();
        size_t bi = 0, bj = 1;
        double bd = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < M; ++i)
            for (size_t j = i + 1; j < M; ++j)
                if (dm[i * N + j] <= bd) { bd = dm[i * N + j]; bi = i; bj = j; }

        // Record merge: lower id first (per MATLAB convention).
        int a_id = ids[bi];
        int b_id = ids[bj];
        if (a_id > b_id) std::swap(a_id, b_id);
        Zflat.push_back(double(a_id + 1));
        Zflat.push_back(double(b_id + 1));
        Zflat.push_back(bd);

        // Build new row: distances from new cluster to every other.
        const int n_a = sizes[bi], n_b = sizes[bj];
        for (size_t k = 0; k < M; ++k) {
            if (k == bi || k == bj) { newrow[k] = 0.0; continue; }
            const double d_ak = dm[bi * N + k];
            const double d_bk = dm[bj * N + k];
            newrow[k] = lance_williams(m, d_ak, d_bk, bd, n_a, n_b, sizes[k]);
        }

        // Replace bi with new cluster and remove bj.
        ids[bi] = next_id++;
        sizes[bi] = n_a + n_b;
        for (size_t k = 0; k < M; ++k) {
            dm[bi * N + k] = newrow[k];
            dm[k * N + bi] = newrow[k];
        }
        // Erase bj from ids/sizes and compact row/col bj of dm.
        ids.erase(ids.begin() + bj);
        sizes.erase(sizes.begin() + bj);
        // Shift columns left for rows < M, then shift rows up.
        for (size_t r = 0; r < M; ++r)
            for (size_t c = bj; c + 1 < M; ++c)
                dm[r * N + c] = dm[r * N + c + 1];
        for (size_t r = bj; r + 1 < M; ++r)
            for (size_t c = 0; c < M - 1; ++c)
                dm[r * N + c] = dm[(r + 1) * N + c];
    }

    // Pack as (N-1)×3 column-major.
    const size_t rows = N - 1;
    Value Z = Value::matrix(rows, 3, ValueType::DOUBLE, mr);
    double *zd = Z.doubleDataMut();
    for (size_t i = 0; i < rows; ++i) {
        zd[0 * rows + i] = Zflat[3 * i + 0];
        zd[1 * rows + i] = Zflat[3 * i + 1];
        zd[2 * rows + i] = Zflat[3 * i + 2];
    }
    return Z;
}

// Backward-compat 2-arg wrapper: defaults to euclidean.
Value linkage(const Value &Y, const std::string &method, std::pmr::memory_resource *mr)
{
    return linkage(Y, method, "euclidean", 2.0, mr);
}

// ════════════════════════════════════════════════════════════════════
// cluster — flatten a linkage tree
// ════════════════════════════════════════════════════════════════════

// Forward decl for inconsistency cutoff.
Value inconsistent(const Value &Z, int depth, std::pmr::memory_resource *mr);

Value cluster_from_linkage(const Value &Z, int maxclust, double cutoff, const std::string &criterion, int depth, std::pmr::memory_resource *mr)
{
    const size_t M = Z.dims().rows();
    if (Z.dims().cols() != 3)
        throw Error("cluster: Z must be (N-1)×3", 0, 0, "cluster", "",
                    "numkit:cluster:size");
    const size_t N = M + 1;
    ScratchArena scratch(mr);
    ScratchVec<int>    a(M, &scratch), b(M, &scratch);
    ScratchVec<double> d(M, &scratch);
    for (size_t i = 0; i < M; ++i) {
        a[i] = (int)Z.doubleData()[0 * M + i] - 1;
        b[i] = (int)Z.doubleData()[1 * M + i] - 1;
        d[i] = Z.doubleData()[2 * M + i];
    }

    // Determine cluster assignment via tree-walk for the inconsistency
    // criterion (MATLAB default for 'cutoff'); use distance threshold for
    // 'distance'; use prefix merges for maxclust.
    ScratchVec<int> labels(N, 0, &scratch);

    if (maxclust > 0 && maxclust >= 1 && (size_t)maxclust <= N) {
        const int merges = (int)N - maxclust;
        // Union-find.
        ScratchVec<int> parent(N + M, &scratch);
        for (size_t i = 0; i < parent.size(); ++i) parent[i] = (int)i;
        std::function<int(int)> find = [&](int x) {
            while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
            return x;
        };
        for (int i = 0; i < merges; ++i) {
            const int newid = (int)N + i;
            int x = find(a[i]), y = find(newid);
            if (x != y) parent[x] = y;
            x = find(b[i]); y = find(newid);
            if (x != y) parent[x] = y;
        }
        ScratchVec<int> root2lab(&scratch);
        for (size_t i = 0; i < N; ++i) {
            const int r = find((int)i);
            int lab = -1;
            for (size_t j = 0; j < root2lab.size(); ++j)
                if (root2lab[j] == r) { lab = (int)j; break; }
            if (lab < 0) { lab = (int)root2lab.size(); root2lab.push_back(r); }
            labels[i] = lab + 1;
        }
    } else if (cutoff > 0.0 && criterion == "distance") {
        // A non-leaf node id (= N + i) is "kept" iff its merge distance d[i]
        // is <= cutoff. Walk top-down: at each node, if kept then collect
        // all leaves under it into one cluster; else recurse into children.
        int next_label = 0;
        std::function<void(int, int)> assign = [&](int id, int lab) {
            if ((size_t)id < N) { labels[(size_t)id] = lab; return; }
            assign(a[(size_t)id - N], lab);
            assign(b[(size_t)id - N], lab);
        };
        std::function<void(int)> walk = [&](int id) {
            if ((size_t)id < N) {
                labels[(size_t)id] = ++next_label;
                return;
            }
            const size_t i = (size_t)id - N;
            if (d[i] <= cutoff) {
                ++next_label;
                assign(id, next_label);
            } else {
                walk(a[i]);
                walk(b[i]);
            }
        };
        walk((int)N + (int)M - 1);
    } else if (cutoff > 0.0) {
        // Inconsistency criterion (MATLAB default).
        Value Yinc = inconsistent(Z, depth, mr);
        const double *yi = Yinc.doubleData();
        // inc per non-leaf node: column 4 (= 3 in 0-based), row layout col-major M×4.
        ScratchVec<double> inc(M, &scratch);
        for (size_t i = 0; i < M; ++i) inc[i] = yi[3 * M + i];
        int next_label = 0;
        std::function<void(int, int)> assign = [&](int id, int lab) {
            if ((size_t)id < N) { labels[(size_t)id] = lab; return; }
            assign(a[(size_t)id - N], lab);
            assign(b[(size_t)id - N], lab);
        };
        std::function<void(int)> walk = [&](int id) {
            if ((size_t)id < N) {
                labels[(size_t)id] = ++next_label;
                return;
            }
            const size_t i = (size_t)id - N;
            if (inc[i] <= cutoff) {
                ++next_label;
                assign(id, next_label);
            } else {
                walk(a[i]);
                walk(b[i]);
            }
        };
        walk((int)N + (int)M - 1);
    } else {
        // No criterion — every leaf is its own cluster.
        for (size_t i = 0; i < N; ++i) labels[i] = (int)i + 1;
    }

    // Compact labels to 1..K in first-encountered order.
    ScratchVec<int> remap(N + 1, 0, &scratch);
    int next = 0;
    ScratchVec<int> compact(N, &scratch);
    for (size_t i = 0; i < N; ++i) {
        if (remap[(size_t)labels[i]] == 0) remap[(size_t)labels[i]] = ++next;
        compact[i] = remap[(size_t)labels[i]];
    }

    Value out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i) od[i] = double(compact[i]);
    return out;
}

// Backward-compat wrapper without depth.
Value cluster_from_linkage(const Value &Z, int maxclust, double cutoff, const std::string &criterion, std::pmr::memory_resource *mr)
{
    return cluster_from_linkage(Z, maxclust, cutoff, criterion, 2, mr);
}

Value clusterdata(const Value &X, int maxclust, double cutoff, const std::string &linkage_method, const std::string &criterion, int depth, const std::string &distance_metric, double p, std::pmr::memory_resource *mr)
{
    Value Y = pdist(X, distance_metric, p, mr);
    Value Z = linkage(Y, linkage_method, mr);
    return cluster_from_linkage(Z, maxclust, cutoff, criterion, depth, mr);
}

Value clusterdata(const Value &X, int maxclust, double cutoff, const std::string &linkage_method, const std::string &criterion, int depth, std::pmr::memory_resource *mr)
{
    return clusterdata(X, maxclust, cutoff, linkage_method, criterion, depth, "euclidean", 2.0, mr);
}

Value clusterdata(const Value &X, int maxclust, double cutoff, const std::string &linkage_method, const std::string &criterion, std::pmr::memory_resource *mr)
{
    return clusterdata(X, maxclust, cutoff, linkage_method, criterion, 2, mr);
}

// ════════════════════════════════════════════════════════════════════
// cophenet — cophenetic correlation coefficient
// ════════════════════════════════════════════════════════════════════

// Returns (correlation, dcoph). The caller uses the scalar c for the
// 1-output case or both for the 2-output case.
//
// Bug fix 2026-05-08: previously returned only the scalar correlation;
// MATLAB's `[c, d] = cophenet(Z, Y)` 2-output form was throwing
// "Undefined function or variable 'd'". Now both outputs are produced.
std::tuple<Value, Value> cophenet_full(const Value &Z, const Value &Y, std::pmr::memory_resource *mr)
{
    const size_t M = Z.dims().rows();
    const size_t N = M + 1;
    if (Z.dims().cols() != 3)
        throw Error("cophenet: Z must be (N-1)×3", 0, 0, "cophenet", "",
                    "numkit:cophenet:size");
    const size_t Yn = Y.numel();
    if (N * (N - 1) / 2 != Yn)
        throw Error("cophenet: Y / Z size mismatch", 0, 0, "cophenet", "",
                    "numkit:cophenet:size");

    ScratchArena scratch(mr);
    ScratchVec<int>    a(M, &scratch), b(M, &scratch);
    ScratchVec<double> d(M, &scratch);
    for (size_t i = 0; i < M; ++i) {
        a[i] = (int)Z.doubleData()[0 * M + i] - 1;
        b[i] = (int)Z.doubleData()[1 * M + i] - 1;
        d[i] = Z.doubleData()[2 * M + i];
    }

    // Walk the merge tree to find the merge distance for every original
    // pair (i, j). Build cluster-membership lists: members[k] holds the
    // 0..N-1 indices in cluster k (0..N-1 are original; N..N+M-1 are merges).
    // The outer container needs uses-allocator construction; plain
    // std::pmr::vector<int> satisfies that (ScratchVec deletes copy ctor
    // and breaks the trait — use plain pmr::vector for nested cases).
    std::pmr::vector<std::pmr::vector<int>> members(N + M,
                                                    std::pmr::vector<int>(&scratch),
                                                    &scratch);
    for (size_t i = 0; i < N; ++i) members[i].push_back((int)i);
    ScratchVec<double> dcoph(Yn, 0.0, &scratch);

    auto pdist_idx = [&](size_t i, size_t j) {
        if (i > j) std::swap(i, j);
        return i * N - (i * (i + 1)) / 2 + (j - i - 1);
    };

    for (size_t i = 0; i < M; ++i) {
        const int newid = (int)N + (int)i;
        const auto &lhs = members[a[i]];
        const auto &rhs = members[b[i]];
        for (int x : lhs)
            for (int y : rhs)
                dcoph[pdist_idx(x, y)] = d[i];
        members[newid].insert(members[newid].end(), lhs.begin(), lhs.end());
        members[newid].insert(members[newid].end(), rhs.begin(), rhs.end());
    }

    // Pearson correlation between Y and dcoph.
    double mx = 0, my = 0;
    for (size_t i = 0; i < Yn; ++i) { mx += Y.elemAsDouble(i); my += dcoph[i]; }
    mx /= Yn; my /= Yn;
    double sxy = 0, sx2 = 0, sy2 = 0;
    for (size_t i = 0; i < Yn; ++i) {
        const double dx = Y.elemAsDouble(i) - mx;
        const double dy = dcoph[i] - my;
        sxy += dx * dy; sx2 += dx * dx; sy2 += dy * dy;
    }
    const double denom = std::sqrt(sx2) * std::sqrt(sy2);
    Value cv = Value::scalar(denom > 0.0 ? sxy / denom : 0.0, mr);

    // Build the 1×Yn cophenetic-distance row vector.
    Value dv = Value::matrix(1, Yn, ValueType::DOUBLE, mr);
    double *dd = dv.doubleDataMut();
    for (size_t i = 0; i < Yn; ++i) dd[i] = dcoph[i];
    return {std::move(cv), std::move(dv)};
}

Value cophenet(const Value &Z, const Value &Y, std::pmr::memory_resource *mr)
{
    auto [c, _d] = cophenet_full(Z, Y, mr);
    return c;
}

// ════════════════════════════════════════════════════════════════════
// inconsistent — inconsistency coefficient
// ════════════════════════════════════════════════════════════════════

Value inconsistent(const Value &Z, int depth, std::pmr::memory_resource *mr) {
    const size_t M = Z.dims().rows();
    if (Z.dims().cols() != 3)
        throw Error("inconsistent: Z must be (N-1)×3", 0, 0, "inconsistent", "",
                    "numkit:inconsistent:size");
    if (depth <= 0) depth = 2;
    const size_t N = M + 1;

    ScratchArena scratch(mr);
    ScratchVec<int>    a(M, &scratch), b(M, &scratch);
    ScratchVec<double> d(M, &scratch);
    for (size_t i = 0; i < M; ++i) {
        a[i] = (int)Z.doubleData()[0 * M + i] - 1;
        b[i] = (int)Z.doubleData()[1 * M + i] - 1;
        d[i] = Z.doubleData()[2 * M + i];
    }

    auto child_dist = [&](int id) -> double {
        if ((size_t)id < N) return -1.0;  // leaf
        return d[(size_t)id - N];
    };

    // For each merge node, walk down `depth` levels, collecting all link
    // distances; compute mean, std, count, inconsistency (= (d - mean)/std).
    std::function<void(int, int, ScratchVec<double>&)> collect =
        [&](int id, int rem, ScratchVec<double> &acc) {
        if ((size_t)id < N) return;
        if (rem == 0) return;
        acc.push_back(d[(size_t)id - N]);
        collect(a[(size_t)id - N], rem - 1, acc);
        collect(b[(size_t)id - N], rem - 1, acc);
    };

    Value out = Value::matrix(M, 4, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    for (size_t i = 0; i < M; ++i) {
        ScratchVec<double> ds(&scratch);
        collect((int)N + (int)i, depth, ds);
        const double n = double(ds.size());
        double mu = 0.0; for (double v : ds) mu += v; mu /= n;
        double var = 0.0; for (double v : ds) var += (v - mu) * (v - mu);
        const double sd = (n > 1.0) ? std::sqrt(var / (n - 1.0)) : 0.0;
        const double inc = (sd > 0.0) ? ((d[i] - mu) / sd) : 0.0;
        od[0 * M + i] = mu;
        od[1 * M + i] = sd;
        od[2 * M + i] = n;
        od[3 * M + i] = inc;
    }
    (void)child_dist;
    return out;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void linkage_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("linkage: requires Y[, method[, metric]]",
                    0, 0, "linkage", "", "numkit:linkage:nargin");
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    std::string method = "single";
    std::string metric = "euclidean";
    double      p      = 2.0;
    if (args.size() >= 2 && (args[1].isChar() || args[1].isString()))
        method = lower(args[1].toString());
    // 3rd arg: metric for the implicit pdist call when Y is raw N×D data.
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString()))
        metric = lower(args[2].toString());
    // 4th arg (Minkowski exponent): MATLAB doesn't pass p positionally
    // here, but accept it as a fallback for the parity harness.
    if (args.size() >= 4 && args[3].numel() == 1
        && !(args[3].isChar() || args[3].isString()))
        p = args[3].toScalar();
    outs[0] = linkage(args[0], method, metric, p, ctx.engine->resource());
}

void cluster_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cluster: requires (Z, options)", 0, 0, "cluster", "",
                    "numkit:cluster:nargin");
    int maxclust = -1;
    double cutoff = -1.0;
    int depth = 2;
    // MATLAB default 'cutoff' criterion is 'inconsistent'.
    std::string criterion = "inconsistent";
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    for (size_t i = 1; i + 1 < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            const auto s = lower(args[i].toString());
            if      (s == "maxclust")  maxclust  = (int)args[i + 1].toScalar();
            else if (s == "cutoff")    cutoff    = args[i + 1].toScalar();
            else if (s == "criterion") criterion = lower(args[i + 1].toString());
            else if (s == "depth")     depth     = (int)args[i + 1].toScalar();
        }
    }
    outs[0] = cluster_from_linkage(args[0], maxclust, cutoff, criterion, depth, ctx.engine->resource());
}

void clusterdata_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("clusterdata: requires (X, ...)", 0, 0, "clusterdata", "",
                    "numkit:clusterdata:nargin");
    int maxclust = -1;
    double cutoff = -1.0;
    int depth = 2;
    double p = 2.0;
    std::string method     = "single";
    std::string criterion  = "inconsistent";
    std::string distance_metric = "euclidean";
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    // MATLAB scalar shortcut: clusterdata(X, c) where c is numeric.
    //   c >= 2  → maxclust (rounded toward zero)
    //   0 < c < 2 → cutoff (inconsistency criterion)
    // Verified via R2025b probe.
    if (args.size() >= 2 && args[1].numel() == 1
        && !(args[1].isChar() || args[1].isString())) {
        const double c = args[1].toScalar();
        if (c >= 2.0) maxclust = (int)c;
        else if (c > 0.0) cutoff = c;
    }
    for (size_t i = 1; i + 1 < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            const auto s = lower(args[i].toString());
            if      (s == "maxclust")  maxclust  = (int)args[i + 1].toScalar();
            else if (s == "cutoff")    cutoff    = args[i + 1].toScalar();
            else if (s == "linkage")   method    = lower(args[i + 1].toString());
            else if (s == "criterion") criterion = lower(args[i + 1].toString());
            else if (s == "depth")     depth     = (int)args[i + 1].toScalar();
            else if (s == "distance")  distance_metric = lower(args[i + 1].toString());
            else if (s == "p")         p         = args[i + 1].toScalar();
            // 'savememory' and other doc'd N-V silently ignored.
        }
    }
    outs[0] = clusterdata(args[0], maxclust, cutoff, method, criterion, depth, distance_metric, p, ctx.engine->resource());
}

void cophenet_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cophenet: requires (Z, Y)", 0, 0, "cophenet", "",
                    "numkit:cophenet:nargin");
    auto [c, d] = cophenet_full(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(c);
    if (nargout > 1) outs[1] = std::move(d);
}

void inconsistent_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("inconsistent: requires (Z[, depth])", 0, 0, "inconsistent",
                    "", "numkit:inconsistent:nargin");
    int depth = (args.size() >= 2 && !args[1].isEmpty())
                ? (int)args[1].toScalar() : 2;
    outs[0] = inconsistent(args[0], depth, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
