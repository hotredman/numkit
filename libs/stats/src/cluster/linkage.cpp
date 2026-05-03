// libs/stats/src/cluster/linkage.cpp

#include <numkit/stats/cluster/linkage.hpp>

#include <numkit/stats/cluster/distance.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <vector>

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

Value linkage(std::pmr::memory_resource *mr, const Value &Y,
              const std::string &method)
{
    const LinkMethod m = parse_link(method);

    // Determine N. Y is either pdist row (1×n*(n-1)/2) or NxD raw data.
    const size_t Yrows = Y.dims().rows();
    const size_t Ycols = Y.dims().cols();
    size_t N;
    std::vector<double> D;  // upper-tri row-major, size N*(N-1)/2

    if (Yrows == 1 || Ycols == 1) {
        const size_t n = Y.numel();
        N = solve_N_from_pdist(n);
        if (N * (N - 1) / 2 != n)
            throw Error("linkage: pdist input has invalid length",
                        0, 0, "linkage", "", "m:linkage:size");
        D.resize(n);
        for (size_t i = 0; i < n; ++i) D[i] = Y.elemAsDouble(i);
    } else {
        // Treat as raw data: compute pdist with Euclidean.
        Value Yp = pdist(mr, Y, "euclidean", 2.0);
        const size_t n = Yp.numel();
        N = Yrows;
        if (N * (N - 1) / 2 != n)
            throw Error("linkage: bad pdist intermediate", 0, 0, "linkage", "",
                        "m:linkage:internal");
        D.resize(n);
        for (size_t i = 0; i < n; ++i) D[i] = Yp.elemAsDouble(i);
    }

    if (N < 2) return Value::matrix(0, 3, ValueType::DOUBLE, mr);

    // Build a dense N×N distance matrix (row-major) from upper-tri row.
    auto pdist_idx = [&](size_t i, size_t j) {
        if (i > j) std::swap(i, j);
        // pdist convention: index of pair (i, j) i<j among rows of an N×N
        // matrix in column-major order matching MATLAB's pdist:
        //   k = (i-1)*(N - i/2) + (j - i) - 1   (1-indexed)
        // 0-based: k = i*(N - 1) - i*(i+1)/2 + (j - i - 1)
        return i * N - (i * (i + 1)) / 2 + (j - i - 1);
    };

    std::vector<std::vector<double>> dm((size_t)N, std::vector<double>((size_t)N, 0.0));
    for (size_t i = 0; i < N; ++i)
        for (size_t j = i + 1; j < N; ++j) {
            const double d = D[pdist_idx(i, j)];
            dm[i][j] = d; dm[j][i] = d;
        }

    // Active cluster set; each entry maps 0..N-1 → original cluster id.
    std::vector<int> ids(N);
    std::vector<int> sizes(N, 1);
    for (size_t i = 0; i < N; ++i) ids[i] = (int)i;  // 0-based; emit 1-based later

    std::vector<double> Zflat;
    Zflat.reserve(3 * (N - 1));

    int next_id = (int)N;
    for (size_t step = 0; step + 1 < N; ++step) {
        // Find smallest distance among active rows.
        const size_t M = ids.size();
        size_t bi = 0, bj = 1;
        double bd = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < M; ++i)
            for (size_t j = i + 1; j < M; ++j)
                if (dm[i][j] < bd) { bd = dm[i][j]; bi = i; bj = j; }

        // Record merge: lower id first (per MATLAB convention).
        int a_id = ids[bi];
        int b_id = ids[bj];
        if (a_id > b_id) std::swap(a_id, b_id);
        Zflat.push_back(double(a_id + 1));
        Zflat.push_back(double(b_id + 1));
        Zflat.push_back(bd);

        // Build new row: distances from new cluster to every other.
        const int n_a = sizes[bi], n_b = sizes[bj];
        std::vector<double> newrow(M, 0.0);
        for (size_t k = 0; k < M; ++k) {
            if (k == bi || k == bj) continue;
            const double d_ak = dm[bi][k];
            const double d_bk = dm[bj][k];
            newrow[k] = lance_williams(m, d_ak, d_bk, bd, n_a, n_b, sizes[k]);
        }

        // Replace bi with new cluster and remove bj.
        ids[bi] = next_id++;
        sizes[bi] = n_a + n_b;
        for (size_t k = 0; k < M; ++k) {
            dm[bi][k] = newrow[k];
            dm[k][bi] = newrow[k];
        }
        // Erase bj.
        ids.erase(ids.begin() + bj);
        sizes.erase(sizes.begin() + bj);
        for (auto &row : dm) row.erase(row.begin() + bj);
        dm.erase(dm.begin() + bj);
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

// ════════════════════════════════════════════════════════════════════
// cluster — flatten a linkage tree
// ════════════════════════════════════════════════════════════════════

Value cluster_from_linkage(std::pmr::memory_resource *mr, const Value &Z,
                           int maxclust, double cutoff,
                           const std::string &criterion)
{
    const size_t M = Z.dims().rows();
    if (Z.dims().cols() != 3)
        throw Error("cluster: Z must be (N-1)×3", 0, 0, "cluster", "",
                    "m:cluster:size");
    const size_t N = M + 1;
    std::vector<int> a(M), b(M);
    std::vector<double> d(M);
    for (size_t i = 0; i < M; ++i) {
        a[i] = (int)Z.doubleData()[0 * M + i] - 1;
        b[i] = (int)Z.doubleData()[1 * M + i] - 1;
        d[i] = Z.doubleData()[2 * M + i];
    }

    // Active cluster representatives. Each step merges a[i] and b[i] into
    // a new cluster N + i; we want to know, for each original sample, which
    // top-level cluster it ends up in (after applying maxclust / cutoff).
    int merges = (int)M;
    if (maxclust > 0 && maxclust >= 1 && (size_t)maxclust <= N)
        merges = (int)N - maxclust;
    else if (cutoff > 0.0 && criterion == "distance") {
        merges = 0;
        for (size_t i = 0; i < M; ++i) {
            if (d[i] > cutoff) break;
            ++merges;
        }
    } else if (cutoff > 0.0) {
        // Inconsistency-coefficient cutoff: use inconsistent() and apply.
        // For first cut we treat cutoff as distance threshold only.
        merges = 0;
        for (size_t i = 0; i < M; ++i) {
            if (d[i] > cutoff) break;
            ++merges;
        }
    }
    if (merges < 0) merges = 0;
    if ((size_t)merges > M) merges = (int)M;

    // Union-Find.
    std::vector<int> parent(N + M);
    for (size_t i = 0; i < parent.size(); ++i) parent[i] = (int)i;
    std::function<int(int)> find = [&](int x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    auto unite = [&](int x, int y) {
        x = find(x); y = find(y);
        if (x != y) parent[x] = y;
    };

    for (int i = 0; i < merges; ++i) {
        const int newid = (int)N + i;
        unite(a[i], newid);
        unite(b[i], newid);
    }

    // Relabel each sample's root to a contiguous 1..K range.
    std::vector<int> labels(N);
    std::vector<int> root2lab;
    root2lab.reserve(8);
    for (size_t i = 0; i < N; ++i) {
        const int r = find((int)i);
        // Find existing or assign new label.
        int lab = -1;
        for (size_t j = 0; j < root2lab.size(); ++j) {
            if (root2lab[j] == r) { lab = (int)j; break; }
        }
        if (lab < 0) { lab = (int)root2lab.size(); root2lab.push_back(r); }
        labels[i] = lab + 1;  // 1-based
    }

    Value out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i) od[i] = double(labels[i]);
    return out;
}

Value clusterdata(std::pmr::memory_resource *mr, const Value &X,
                  int maxclust, double cutoff,
                  const std::string &linkage_method,
                  const std::string &criterion)
{
    Value Y = pdist(mr, X, "euclidean", 2.0);
    Value Z = linkage(mr, Y, linkage_method);
    return cluster_from_linkage(mr, Z, maxclust, cutoff, criterion);
}

// ════════════════════════════════════════════════════════════════════
// cophenet — cophenetic correlation coefficient
// ════════════════════════════════════════════════════════════════════

Value cophenet(std::pmr::memory_resource *mr, const Value &Z, const Value &Y) {
    const size_t M = Z.dims().rows();
    const size_t N = M + 1;
    if (Z.dims().cols() != 3)
        throw Error("cophenet: Z must be (N-1)×3", 0, 0, "cophenet", "",
                    "m:cophenet:size");
    const size_t Yn = Y.numel();
    if (N * (N - 1) / 2 != Yn)
        throw Error("cophenet: Y / Z size mismatch", 0, 0, "cophenet", "",
                    "m:cophenet:size");

    std::vector<int>    a(M), b(M);
    std::vector<double> d(M);
    for (size_t i = 0; i < M; ++i) {
        a[i] = (int)Z.doubleData()[0 * M + i] - 1;
        b[i] = (int)Z.doubleData()[1 * M + i] - 1;
        d[i] = Z.doubleData()[2 * M + i];
    }

    // Walk the merge tree to find the merge distance for every original
    // pair (i, j). Build cluster-membership lists: members[k] holds the
    // 0..N-1 indices in cluster k (0..N-1 are original; N..N+M-1 are merges).
    std::vector<std::vector<int>> members(N + M);
    for (size_t i = 0; i < N; ++i) members[i].push_back((int)i);
    std::vector<double> dcoph(Yn, 0.0);

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
    return Value::scalar(denom > 0.0 ? sxy / denom : 0.0, mr);
}

// ════════════════════════════════════════════════════════════════════
// inconsistent — inconsistency coefficient
// ════════════════════════════════════════════════════════════════════

Value inconsistent(std::pmr::memory_resource *mr, const Value &Z, int depth) {
    const size_t M = Z.dims().rows();
    if (Z.dims().cols() != 3)
        throw Error("inconsistent: Z must be (N-1)×3", 0, 0, "inconsistent", "",
                    "m:inconsistent:size");
    if (depth <= 0) depth = 2;
    const size_t N = M + 1;

    std::vector<int>    a(M), b(M);
    std::vector<double> d(M);
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
    std::function<void(int, int, std::vector<double>&)> collect =
        [&](int id, int rem, std::vector<double> &acc) {
        if ((size_t)id < N) return;
        if (rem == 0) return;
        acc.push_back(d[(size_t)id - N]);
        collect(a[(size_t)id - N], rem - 1, acc);
        collect(b[(size_t)id - N], rem - 1, acc);
    };

    Value out = Value::matrix(M, 4, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    for (size_t i = 0; i < M; ++i) {
        std::vector<double> ds;
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
        throw Error("linkage: requires Y[, method]", 0, 0, "linkage", "",
                    "m:linkage:nargin");
    std::string method = "single";
    if (args.size() >= 2 && (args[1].isChar() || args[1].isString()))
        method = args[1].toString();
    outs[0] = linkage(ctx.engine->resource(), args[0], method);
}

void cluster_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cluster: requires (Z, options)", 0, 0, "cluster", "",
                    "m:cluster:nargin");
    int maxclust = -1;
    double cutoff = -1.0;
    std::string criterion = "inconsistent";
    for (size_t i = 1; i + 1 < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            const auto s = args[i].toString();
            if      (s == "maxclust") maxclust = (int)args[i + 1].toScalar();
            else if (s == "cutoff")   cutoff   = args[i + 1].toScalar();
            else if (s == "criterion") criterion = args[i + 1].toString();
        }
    }
    outs[0] = cluster_from_linkage(ctx.engine->resource(), args[0],
                                    maxclust, cutoff, criterion);
}

void clusterdata_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("clusterdata: requires (X, ...)", 0, 0, "clusterdata", "",
                    "m:clusterdata:nargin");
    int maxclust = -1;
    double cutoff = -1.0;
    std::string method = "single";
    std::string criterion = "inconsistent";
    // If second arg is a scalar, treat as maxclust (MATLAB shortcut).
    if (args.size() >= 2 && args[1].numel() == 1
        && !(args[1].isChar() || args[1].isString())) {
        maxclust = (int)args[1].toScalar();
    }
    for (size_t i = 1; i + 1 < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            const auto s = args[i].toString();
            if      (s == "maxclust")  maxclust  = (int)args[i + 1].toScalar();
            else if (s == "cutoff")    cutoff    = args[i + 1].toScalar();
            else if (s == "linkage")   method    = args[i + 1].toString();
            else if (s == "criterion") criterion = args[i + 1].toString();
        }
    }
    outs[0] = clusterdata(ctx.engine->resource(), args[0], maxclust, cutoff,
                          method, criterion);
}

void cophenet_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cophenet: requires (Z, Y)", 0, 0, "cophenet", "",
                    "m:cophenet:nargin");
    outs[0] = cophenet(ctx.engine->resource(), args[0], args[1]);
}

void inconsistent_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("inconsistent: requires (Z[, depth])", 0, 0, "inconsistent",
                    "", "m:inconsistent:nargin");
    int depth = (args.size() >= 2 && !args[1].isEmpty())
                ? (int)args[1].toScalar() : 2;
    outs[0] = inconsistent(ctx.engine->resource(), args[0], depth);
}

} // namespace detail
} // namespace numkit::stats
