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
huffmandict(const Value &symbols, const Value &probs,
            std::pmr::memory_resource *mr)
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

// ── huffmanenco / huffmandeco helpers ──────────────────────────────

namespace {

// Read dict: K-by-2 cell, column 0 = symbol scalars, column 1 = code rows.
// Output:
//   syms[k]      = symbol value
//   codes[k]     = vector<uint8_t> of 0/1
struct DictView {
    std::vector<double>                 syms;
    std::vector<std::vector<uint8_t>>   codes;
    size_t K = 0;
};

DictView readDict(const Value &dict)
{
    if (dict.dims().cols() != 2)
        throw Error("huffman codec: dict must be K-by-2 cell",
                    0, 0, "huffman", "", "m:huffman:DictShape");
    const size_t K = dict.dims().rows();
    DictView dv;
    dv.K = K;
    dv.syms.resize(K);
    dv.codes.resize(K);
    for (size_t k = 0; k < K; ++k) {
        // Cell layout: column-major. Index k = (row=k, col=0); k+K = col 1.
        const Value &symV  = dict.cellAt(k);
        const Value &codeV = dict.cellAt(k + K);
        dv.syms[k] = symV.toScalar();
        const size_t L = codeV.numel();
        dv.codes[k].resize(L);
        for (size_t b = 0; b < L; ++b) {
            const double v = codeV.elemAsDouble(b);
            if (v != 0.0 && v != 1.0)
                throw Error("huffman codec: dict codes must contain 0/1",
                            0, 0, "huffman", "", "m:huffman:DictBits");
            dv.codes[k][b] = static_cast<uint8_t>(v);
        }
    }
    return dv;
}

// Lookup symbol -> code index. Linear scan; K is small (typically <=256).
int findSymbolIdx(const DictView &dv, double sym)
{
    for (size_t k = 0; k < dv.K; ++k) {
        if (dv.syms[k] == sym) return static_cast<int>(k);
    }
    return -1;
}

// Decode-side: build a prefix tree.
//   left/right child indices (-1 = none); leaf_sym = >=0 symbol index.
struct DecodeTree {
    struct Node {
        int left  = -1;
        int right = -1;
        int sym   = -1;   // index into dv.syms; -1 if internal node.
    };
    std::vector<Node> nodes{ Node{} };  // start with root.

    void insert(const std::vector<uint8_t> &code, int sym_idx)
    {
        int cur = 0;
        for (uint8_t bit : code) {
            int child = (bit == 0) ? nodes[cur].left
                                   : nodes[cur].right;
            if (child < 0) {
                // push_back may reallocate -> read by value above, then
                // write back via index after the push completes.
                nodes.push_back(Node{});
                child = static_cast<int>(nodes.size() - 1);
                if (bit == 0) nodes[cur].left  = child;
                else          nodes[cur].right = child;
            }
            cur = child;
        }
        nodes[cur].sym = sym_idx;
    }
};

bool isRowOriented(const Value &v)
{
    return v.dims().rows() == 1 && v.dims().cols() >= 1;
}

} // namespace

Value huffmanenco(const Value &sig, const Value &dict,
                  std::pmr::memory_resource *mr)
{
    DictView dv = readDict(dict);

    // Walk sig once to compute output bit count.
    const size_t N = sig.numel();
    size_t total_bits = 0;
    for (size_t i = 0; i < N; ++i) {
        const double s = sig.elemAsDouble(i);
        const int k = findSymbolIdx(dv, s);
        if (k < 0)
            throw Error("huffmanenco: symbol not in dict",
                        0, 0, "huffmanenco", "",
                        "m:huffmanenco:UnknownSym");
        total_bits += dv.codes[k].size();
    }

    const bool row = isRowOriented(sig);
    Value out = Value::matrix(row ? 1 : total_bits,
                              row ? total_bits : 1,
                              ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();

    size_t pos = 0;
    for (size_t i = 0; i < N; ++i) {
        const int k = findSymbolIdx(dv, sig.elemAsDouble(i));
        for (uint8_t bit : dv.codes[k])
            o[pos++] = static_cast<double>(bit);
    }
    return out;
}

Value huffmandeco(const Value &bits, const Value &dict,
                  std::pmr::memory_resource *mr)
{
    DictView dv = readDict(dict);

    // Build decode tree.
    DecodeTree tree;
    for (size_t k = 0; k < dv.K; ++k)
        tree.insert(dv.codes[k], static_cast<int>(k));

    // Walk bits, emit symbols.
    const size_t N = bits.numel();
    std::vector<double> emitted;
    int cur = 0;
    for (size_t i = 0; i < N; ++i) {
        const double v = bits.elemAsDouble(i);
        if (v != 0.0 && v != 1.0)
            throw Error("huffmandeco: input bits must be 0 or 1",
                        0, 0, "huffmandeco", "",
                        "m:huffmandeco:NonBit");
        const int next = (v == 0.0) ? tree.nodes[cur].left
                                    : tree.nodes[cur].right;
        if (next < 0)
            throw Error("huffmandeco: bit pattern does not match dict",
                        0, 0, "huffmandeco", "",
                        "m:huffmandeco:NoMatch");
        cur = next;
        if (tree.nodes[cur].sym >= 0) {
            emitted.push_back(dv.syms[tree.nodes[cur].sym]);
            cur = 0;
        }
    }
    if (cur != 0)
        throw Error("huffmandeco: trailing bits not a complete code",
                    0, 0, "huffmandeco", "",
                    "m:huffmandeco:Incomplete");

    const bool row = isRowOriented(bits);
    const size_t M = emitted.size();
    Value out = Value::matrix(row ? 1 : M, row ? M : 1,
                              ValueType::DOUBLE, mr);
    if (M > 0) std::copy(emitted.begin(), emitted.end(), out.doubleDataMut());
    return out;
}

namespace detail {

void huffmandict_reg(Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("huffmandict: requires (symbols, probs)",
                    0, 0, "huffmandict", "", "m:huffmandict:nargin");
    auto *mr = ctx.engine->resource();
    auto [dict, avglen] = huffmandict(args[0], args[1], mr);
    outs[0] = std::move(dict);
    if (nargout > 1)
        outs[1] = Value::scalar(avglen, mr);
}

void huffmanenco_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("huffmanenco: requires (sig, dict)",
                    0, 0, "huffmanenco", "", "m:huffmanenco:nargin");
    outs[0] = huffmanenco(args[0], args[1], ctx.engine->resource());
}

void huffmandeco_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("huffmandeco: requires (bits, dict)",
                    0, 0, "huffmandeco", "", "m:huffmandeco:nargin");
    outs[0] = huffmandeco(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::comm
