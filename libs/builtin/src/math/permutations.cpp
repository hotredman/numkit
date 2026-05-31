// libs/builtin/src/math/permutations.cpp
//
// Sparse-style permutation utilities for dense matrices.
//   * colperm — sort columns by ascending nonzero count.
//   * symrcm  — symmetric reverse Cuthill-McKee ordering.
//
// MATLAB's colperm and symrcm are documented for sparse matrices but
// work on dense matrices too (any element != 0 counts as nonzero).

#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace numkit::builtin {
namespace detail {

// j = colperm(S) — permutation vector that orders the columns of S
// by ascending count of nonzero entries. Stable: ties broken by
// original column index. Returns a row vector of doubles in MATLAB
// (1-indexed).
void colperm_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("colperm: requires 1 argument",
                     0, 0, "colperm", "", "numkit:colperm:nargin");
    auto *mr = ctx.engine->resource();
    const Value &S = args[0];
    const auto &d = S.dims();
    const std::size_t M = d.rows();
    const std::size_t N = (d.ndim() >= 2) ? d.cols() : 1;
    if (S.numel() != M * N)
        throw Error("colperm: input must be a 2-D matrix",
                     0, 0, "colperm", "", "numkit:colperm:shape");

    // Count nonzeros per column.
    std::vector<std::size_t> nnz(N, 0);
    for (std::size_t c = 0; c < N; ++c) {
        std::size_t k = 0;
        for (std::size_t r = 0; r < M; ++r)
            if (S.elemAsDouble(r + c * M) != 0.0) ++k;
        nnz[c] = k;
    }

    // Stable sort column indices by ascending nnz.
    std::vector<std::size_t> perm(N);
    for (std::size_t i = 0; i < N; ++i) perm[i] = i;
    std::stable_sort(perm.begin(), perm.end(),
        [&](std::size_t a, std::size_t b) { return nnz[a] < nnz[b]; });

    // Output: row vector of 1-indexed permutation indices, DOUBLE.
    auto out = Value::matrix(1, N, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < N; ++i)
        od[i] = double(perm[i] + 1);
    outs[0] = std::move(out);
}

// p = symrcm(S) — symmetric reverse Cuthill-McKee ordering.
//
// Algorithm (Cuthill & McKee 1969, reversed per George 1971):
//   1. Build undirected adjacency pattern from |S| + |S^T|, dropping
//      the diagonal. Two nodes i, j are adjacent iff S(i,j) != 0 or
//      S(j,i) != 0.
//   2. For each connected component, taken in ascending unvisited-
//      node order:
//        a. Start node = lowest-degree unvisited node, tie-broken by
//           smallest original index (matches MATLAB R2025b — full
//           Gibbs-Poole-Stockmeyer pseudoperipheral search not
//           required for the bandwidth-reduction guarantee on the
//           probed examples).
//        b. BFS, sorting newly-discovered neighbours by ascending
//           degree with tie → smallest original index.
//        c. Reverse the component's BFS order.
//   3. Concatenate components.
//
// References:
//   - Cuthill, E., McKee, J. (1969). "Reducing the bandwidth of
//     sparse symmetric matrices." Proc. 24th Nat. Conf. ACM, 157-172.
//   - George, A. (1971). "Computer implementation of the finite
//     element method." Tech. Rep. STAN-CS-71-208, Stanford Univ.
void symrcm_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("symrcm: requires 1 argument",
                     0, 0, "symrcm", "", "numkit:symrcm:nargin");
    auto *mr = ctx.engine->resource();
    const Value &S = args[0];
    const auto &d = S.dims();
    const std::size_t M = d.rows();
    const std::size_t N = (d.ndim() >= 2) ? d.cols() : 1;
    if (M != N)
        throw Error("symrcm: input must be a square matrix",
                     0, 0, "symrcm", "", "numkit:symrcm:shape");
    const std::size_t n = M;
    if (n == 0) {
        outs[0] = Value::matrix(1, 0, ValueType::DOUBLE, mr);
        return;
    }

    // Build undirected adjacency list. Off-diagonal nonzero in either
    // S(i,j) or S(j,i) → edge between i and j.
    std::vector<std::vector<std::size_t>> adj(n);
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t i = 0; i < n; ++i) {
            if (i == j) continue;
            const double a = S.elemAsDouble(i + j * n);
            const double b = S.elemAsDouble(j + i * n);
            if (a != 0.0 || b != 0.0) {
                if (i < j) {
                    // Track edges via the smaller index; avoid dups.
                    adj[i].push_back(j);
                }
            }
        }
    }
    // Mirror to make adj truly undirected (each edge in both lists).
    {
        std::vector<std::vector<std::size_t>> sym(n);
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j : adj[i]) {
                sym[i].push_back(j);
                sym[j].push_back(i);
            }
        // sym may have duplicates per node — fine for degree (don't
        // double-count, since edges are recorded once above).
        // Actually `adj[i].push_back(j)` only once per (i<j), so
        // sym[i] holds each neighbour once. Move into adj.
        adj = std::move(sym);
        for (auto &nbr : adj) {
            std::sort(nbr.begin(), nbr.end());
            nbr.erase(std::unique(nbr.begin(), nbr.end()), nbr.end());
        }
    }

    std::vector<std::size_t> deg(n);
    for (std::size_t i = 0; i < n; ++i) deg[i] = adj[i].size();

    std::vector<std::uint8_t> visited(n, 0);
    std::vector<std::size_t> order; order.reserve(n);

    // Iterate components in order of smallest unvisited node.
    for (std::size_t start_scan = 0; start_scan < n; ++start_scan) {
        if (visited[start_scan]) continue;
        // Find lowest-degree unvisited node in this component-region.
        // (We restrict to "from start_scan onwards" since lower indices
        // already visited — but to match MATLAB's per-component
        // semantics, we want the min-degree node reachable from
        // start_scan. Simpler: collect the component first, then pick
        // its min-degree node.)
        // BFS the component to enumerate nodes.
        std::vector<std::size_t> comp;
        std::vector<std::uint8_t> in_comp(n, 0);
        {
            std::vector<std::size_t> q{start_scan};
            in_comp[start_scan] = 1;
            for (std::size_t qi = 0; qi < q.size(); ++qi) {
                const std::size_t u = q[qi];
                comp.push_back(u);
                for (std::size_t v : adj[u]) {
                    if (!in_comp[v]) { in_comp[v] = 1; q.push_back(v); }
                }
            }
        }
        // Start = min-degree node in component, tiebreak → smallest idx.
        std::size_t start = comp[0];
        for (std::size_t u : comp) {
            if (deg[u] < deg[start] ||
                (deg[u] == deg[start] && u < start))
                start = u;
        }

        // BFS from start with degree-sorted children.
        std::vector<std::size_t> cm_order;
        cm_order.reserve(comp.size());
        std::vector<std::uint8_t> in_q(n, 0);
        std::vector<std::size_t> q{start};
        in_q[start] = 1;
        for (std::size_t qi = 0; qi < q.size(); ++qi) {
            const std::size_t u = q[qi];
            cm_order.push_back(u);
            // Collect unvisited neighbours.
            std::vector<std::size_t> kids;
            for (std::size_t v : adj[u])
                if (!in_q[v]) kids.push_back(v);
            // Sort by ascending degree, tiebreak by ascending index.
            std::stable_sort(kids.begin(), kids.end(),
                [&](std::size_t a, std::size_t b) {
                    if (deg[a] != deg[b]) return deg[a] < deg[b];
                    return a < b;
                });
            for (std::size_t v : kids) {
                in_q[v] = 1;
                q.push_back(v);
            }
        }
        // Reverse and append to global order; mark visited.
        for (auto it = cm_order.rbegin(); it != cm_order.rend(); ++it) {
            order.push_back(*it);
            visited[*it] = 1;
        }
    }

    auto out = Value::matrix(1, n, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i) od[i] = double(order[i] + 1);
    outs[0] = std::move(out);
}

} // namespace detail
} // namespace numkit::builtin
