// toolboxes/signal/src/math/permutations_reg.cpp
//
// CallContext register half of math/permutations.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/library.hpp>
#include <numkit/math/permutations.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::builtin {
using namespace numkit::math;  // C4c localized (umbrella removed)

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
    outs[0] = colperm(args[0], ctx.engine->resource());
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
    outs[0] = symrcm(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
