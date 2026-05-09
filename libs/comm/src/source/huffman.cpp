// libs/comm/src/source/huffman.cpp
//
// huffmandict: build a Huffman dictionary from symbol probabilities.
//
// Returned as an N-by-2 cell array:
//   dict{k, 1} = symbol value (scalar double)
//   dict{k, 2} = bit code (1×L row vector of 0/1 doubles)
//
// Optional second output is the average code length
// avglen = Σ p_i · length(code_i).
//
// Algorithm: classic min-heap of (combined_prob, node_id) pairs.
// Two smallest pulled per step, fused as parent with bit assignment
// 0=first, 1=second; tree walked top-down to label leaves with
// their bit string. The MATLAB tie-breaking convention is matched
// by sorting with stable order (lexicographic by current symbol
// index), which reproduces MATLAB R2025b on the standard probe set.

#include <numkit/comm/source/huffman.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <queue>
#include <vector>

namespace numkit::comm {

namespace {

struct HeapNode {
    double prob;
    int    id;        // Node id; leaves are 0..K-1, internals are K..2K-2.
    int    order;     // Insertion order to break ties stably.
    bool operator>(const HeapNode &b) const {
        if (prob != b.prob) return prob > b.prob;
        return order > b.order;
    }
};

struct TreeNode {
    int parent  = -1;
    int bit     = 0;  // 0 if this is the "first/lower" child, 1 otherwise.
};

} // namespace

std::pair<Value, double>
huffmandict(std::pmr::memory_resource *mr,
            const Value &symbols, const Value &probs)
{
    const size_t K = symbols.numel();
    if (K == 0)
        throw Error("huffmandict: symbols must be non-empty",
                    0, 0, "huffmandict", "", "m:huffmandict:Empty");
    if (probs.numel() != K)
        throw Error("huffmandict: probs must match length of symbols",
                    0, 0, "huffmandict", "", "m:huffmandict:LenMismatch");

    // Validate probs.
    double sum = 0.0;
    for (size_t k = 0; k < K; ++k) {
        const double p = probs.elemAsDouble(k);
        if (!(p >= 0.0 && p <= 1.0))
            throw Error("huffmandict: probabilities must lie in [0, 1]",
                        0, 0, "huffmandict", "",
                        "m:huffmandict:InvalidProb");
        sum += p;
    }
    if (std::abs(sum - 1.0) > std::sqrt(2.220446049250313e-16))
        throw Error("huffmandict: probabilities must sum to 1",
                    0, 0, "huffmandict", "",
                    "m:huffmandict:InvalidProbSum");

    // Edge case: a single symbol gets a single-bit "0" code.
    if (K == 1) {
        Value dict = Value::cell(1, 2, mr);
        dict.cellAt(0)               = Value::scalar(symbols.elemAsDouble(0), mr);
        Value code = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        code.doubleDataMut()[0] = 0.0;
        dict.cellAt(1)               = std::move(code);
        return {std::move(dict), 1.0};
    }

    // Reserve tree storage: 2K-1 nodes (K leaves + K-1 internals).
    std::vector<TreeNode> tree(2 * K - 1);

    // Build initial leaves heap.
    std::priority_queue<HeapNode, std::vector<HeapNode>, std::greater<>> heap;
    int order_seed = 0;
    for (size_t k = 0; k < K; ++k) {
        heap.push({probs.elemAsDouble(k), static_cast<int>(k), order_seed++});
    }

    int next_id = static_cast<int>(K);
    while (heap.size() > 1) {
        HeapNode a = heap.top(); heap.pop();
        HeapNode b = heap.top(); heap.pop();
        // a is the smaller -> assign bit 0; b -> bit 1.
        tree[a.id].parent = next_id;  tree[a.id].bit = 0;
        tree[b.id].parent = next_id;  tree[b.id].bit = 1;
        heap.push({a.prob + b.prob, next_id, order_seed++});
        ++next_id;
    }
    // Heap root is the tree root; tree[root].parent stays -1.

    // Walk leaf -> root, collect bits, reverse. Build dict cell.
    Value dict = Value::cell(K, 2, mr);
    double avglen = 0.0;
    std::vector<double> bits;
    for (size_t k = 0; k < K; ++k) {
        bits.clear();
        int cur = static_cast<int>(k);
        while (tree[cur].parent != -1) {
            bits.push_back(static_cast<double>(tree[cur].bit));
            cur = tree[cur].parent;
        }
        std::reverse(bits.begin(), bits.end());
        const size_t L = bits.size();
        // MATLAB cell layout is column-major: column 0 = symbols, column 1 = codes.
        dict.cellAt(k)           = Value::scalar(symbols.elemAsDouble(k), mr);
        Value code = Value::matrix(1, L, ValueType::DOUBLE, mr);
        if (L > 0)
            std::copy(bits.begin(), bits.end(), code.doubleDataMut());
        dict.cellAt(K + k)       = std::move(code);
        avglen += probs.elemAsDouble(k) * static_cast<double>(L);
    }
    return {std::move(dict), avglen};
}

namespace detail {

void huffmandict_reg(Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("huffmandict: requires (symbols, probs)",
                    0, 0, "huffmandict", "", "m:huffmandict:nargin");
    auto *mr = ctx.engine->resource();
    auto [dict, avglen] = huffmandict(mr, args[0], args[1]);
    outs[0] = std::move(dict);
    if (nargout > 1)
        outs[1] = Value::scalar(avglen, mr);
}

} // namespace detail

} // namespace numkit::comm
