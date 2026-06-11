// toolboxes/comm/src/coding/blockcoding.cpp
//
// Block linear coding (Error Correction Codes): gen2par, hammgen,
// cyclpoly, cyclgen, encode, decode. All GF(2) (binary) arithmetic on
// plain double matrices.
//
// MATLAB R2025b semantics (verified via probe + toolbox-doc algorithm):
//   hammgen(m): H(:,i) = coeffs of x^i mod p(x), p = default primitive
//     polynomial of degree m, ascending power. First m columns = I_m, so
//     the code is systematic and g = gen2par(h).
//   gen2par(g): systematic generator<->parity converter (involution).
//   cyclpoly(n,k): first degree-(n-k) poly 1+...+x^(n-k) dividing x^n-1.
//   cyclgen(n,p): cyclic parity/generator matrices (system / nonsystem).
//   encode/decode: reshape into words, generator-multiply / syndrome-decode.

#include <numkit/comm/coding/blockcoding.hpp>

#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace numkit::comm {

namespace {

// ── GF(2) primitives ───────────────────────────────────────────────────

// Default primitive polynomial of degree m (m = 2..16) as an integer bit
// mask: bit j set ⇔ coefficient of x^j is 1. Matches MATLAB gfprimdf.
// Bit 0 (constant) and bit m (leading) are always set.
std::uint64_t defaultPrimMask(long long m, const char *who)
{
    switch (m) {
    case 2:  return 0x7ULL;        // 1+x+x^2
    case 3:  return 0xBULL;        // 1+x+x^3
    case 4:  return 0x13ULL;       // 1+x+x^4
    case 5:  return 0x25ULL;       // 1+x^2+x^5
    case 6:  return 0x43ULL;       // 1+x+x^6
    case 7:  return 0x89ULL;       // 1+x^3+x^7
    case 8:  return 0x11DULL;      // 1+x^2+x^3+x^4+x^8
    case 9:  return 0x211ULL;      // 1+x^4+x^9
    case 10: return 0x409ULL;      // 1+x^3+x^10
    case 11: return 0x805ULL;      // 1+x^2+x^11
    case 12: return 0x1053ULL;     // 1+x+x^4+x^6+x^12
    case 13: return 0x201BULL;     // 1+x+x^3+x^4+x^13
    case 14: return 0x4443ULL;     // 1+x+x^6+x^10+x^14
    case 15: return 0x8003ULL;     // 1+x+x^15
    case 16: return 0x1100BULL;    // 1+x+x^3+x^12+x^16
    default:
        throw Error(std::string(who) + ": M out of supported range [2,16]",
                    0, 0, who, "", std::string("numkit:") + who + ":range");
    }
}

// Read element (i,j) of a numeric Value (column-major) as 0/1.
inline int matBit(const Value &v, std::size_t i, std::size_t j)
{
    const std::size_t rows = v.dims().rows();
    return v.elemAsDouble(j * rows + i) != 0.0 ? 1 : 0;
}

// ── GF(2) polynomial arithmetic (coefficients ascending: index = power) ─

// Highest set power of a, or -1 for the zero polynomial.
inline int polyDeg(const std::vector<int> &a)
{
    for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i)
        if (a[i] & 1) return i;
    return -1;
}

// Remainder of a mod b over GF(2). b must be nonzero.
std::vector<int> polyMod(std::vector<int> a, const std::vector<int> &b)
{
    const int db = polyDeg(b);
    for (auto &x : a) x &= 1;
    for (int da = polyDeg(a); da >= db && da >= 0; da = polyDeg(a))
        for (int j = 0; j <= db; ++j) a[j + (da - db)] ^= b[j];
    a.resize(db > 0 ? static_cast<std::size_t>(db) : 0);  // degree < db
    return a;
}

// Quotient + remainder of a / b over GF(2).
struct PolyQR { std::vector<int> q, r; };
PolyQR polyDivMod(std::vector<int> a, const std::vector<int> &b)
{
    const int db = polyDeg(b);
    for (auto &x : a) x &= 1;
    const int da0 = polyDeg(a);
    std::vector<int> q(da0 >= db ? static_cast<std::size_t>(da0 - db + 1) : 1, 0);
    for (int da = da0; da >= db && da >= 0; da = polyDeg(a)) {
        q[da - db] ^= 1;
        for (int j = 0; j <= db; ++j) a[j + (da - db)] ^= b[j];
    }
    a.resize(db > 0 ? static_cast<std::size_t>(db) : 1);
    return {std::move(q), std::move(a)};
}

// Read a numeric Value (row or column vector) as a 0/1 coefficient list.
std::vector<int> readPolyRow(const Value &v)
{
    const std::size_t nel = v.numel();
    std::vector<int> p(nel);
    for (std::size_t i = 0; i < nel; ++i) p[i] = v.elemAsDouble(i) != 0.0 ? 1 : 0;
    return p;
}

inline std::string toLower(std::string s)
{
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Build a row vector Value from a 0/1 coefficient list.
Value polyToRow(const std::vector<int> &t, std::pmr::memory_resource *mr)
{
    Value v = Value::matrix(1, t.size(), ValueType::DOUBLE, mr);
    double *d = v.doubleDataMut();
    for (std::size_t j = 0; j < t.size(); ++j) d[j] = t[j];
    return v;
}

// Stack `rows` cyclic right-shifts of `row` into a rows×n DOUBLE matrix
// (each successive row is the previous rotated one position right).
Value shiftMatrix(std::vector<int> row, long long rows, long long n,
                  std::pmr::memory_resource *mr)
{
    Value out = Value::matrix(static_cast<std::size_t>(rows),
                              static_cast<std::size_t>(n), ValueType::DOUBLE, mr);
    double *d = out.doubleDataMut();
    for (long long r = 0; r < rows; ++r) {
        for (long long c = 0; c < n; ++c) d[c * rows + r] = row[c];
        const int last = row[n - 1];
        for (long long c = n - 1; c > 0; --c) row[c] = row[c - 1];
        row[0] = last;
    }
    return out;
}

// ── gen2par core (shared by gen2par + hammgen) ─────────────────────────

// Convert an r×c binary matrix (r < c) between [P|I_r]/[I_r|P] systematic
// form and its complement. Returns a fresh (c-r)×c DOUBLE Value.
Value gen2parImpl(const Value &mat, const char *who,
                  std::pmr::memory_resource *mr)
{
    const std::size_t r = mat.dims().rows();
    const std::size_t c = mat.dims().cols();
    if (r == 0 || c == 0 || r >= c)
        throw Error(std::string(who) + ": input must be r×c with r < c",
                    0, 0, who, "", std::string("numkit:") + who + ":shape");
    const std::size_t pcols = c - r;  // parity columns / output rows

    // Branch A: last r columns form I_r  ⇒ mat = [P | I_r]
    bool lastIsI = true;
    for (std::size_t i = 0; i < r && lastIsI; ++i)
        for (std::size_t t = 0; t < r; ++t)
            if (matBit(mat, i, pcols + t) != (i == t ? 1 : 0)) { lastIsI = false; break; }

    // Branch B: first r columns form I_r ⇒ mat = [I_r | P]
    bool firstIsI = true;
    for (std::size_t i = 0; i < r && firstIsI; ++i)
        for (std::size_t t = 0; t < r; ++t)
            if (matBit(mat, i, t) != (i == t ? 1 : 0)) { firstIsI = false; break; }

    Value out = Value::matrix(pcols, c, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    auto put = [&](std::size_t i, std::size_t j, int b) { od[j * pcols + i] = b; };

    if (lastIsI) {
        // out = [I_(c-r) | Pᵀ], P = mat(:, 0..pcols-1)
        for (std::size_t i = 0; i < pcols; ++i) {
            for (std::size_t j = 0; j < pcols; ++j) put(i, j, i == j ? 1 : 0);
            for (std::size_t t = 0; t < r; ++t) put(i, pcols + t, matBit(mat, t, i));
        }
    } else if (firstIsI) {
        // out = [Pᵀ | I_(c-r)], P = mat(:, r..c-1)
        for (std::size_t i = 0; i < pcols; ++i) {
            for (std::size_t t = 0; t < r; ++t) put(i, t, matBit(mat, t, r + i));
            for (std::size_t j = 0; j < pcols; ++j) put(i, r + j, i == j ? 1 : 0);
        }
    } else {
        throw Error(std::string(who) + ": neither the first nor the last r "
                    "columns form an identity matrix (input not systematic)",
                    0, 0, who, "", std::string("numkit:") + who + ":notsystematic");
    }
    return out;
}

} // namespace

// ── gen2par ─────────────────────────────────────────────────────────────

Value gen2par(const Value &mat, std::pmr::memory_resource *mr)
{
    return gen2parImpl(mat, "gen2par", mr);
}

// ── hammgen ─────────────────────────────────────────────────────────────

HammgenResult hammgen(long long m, const Value &primPoly,
                      std::pmr::memory_resource *mr)
{
    if (m < 2)
        throw Error("hammgen: M must be an integer >= 2",
                    0, 0, "hammgen", "", "numkit:hammgen:M");

    // Primitive polynomial as an integer mask (bit j = coeff of x^j).
    std::uint64_t pmask;
    if (primPoly.isEmpty()) {
        pmask = defaultPrimMask(m, "hammgen");
    } else {
        const std::size_t len = primPoly.numel();
        if (static_cast<long long>(len) != m + 1)
            throw Error("hammgen: P must be a binary row of length M+1",
                        0, 0, "hammgen", "", "numkit:hammgen:P");
        pmask = 0;
        for (std::size_t j = 0; j < len; ++j)
            if (primPoly.elemAsDouble(j) != 0.0) pmask |= (1ULL << j);
        if ((pmask & 1ULL) == 0 || (pmask & (1ULL << m)) == 0)
            throw Error("hammgen: P must have nonzero constant and degree-M terms",
                        0, 0, "hammgen", "", "numkit:hammgen:P");
    }

    const long long n = (1LL << m) - 1;
    const long long k = n - m;

    // H(j,i) = bit j of (x^i mod p), i = 0..n-1, j = 0..m-1.
    Value h = Value::matrix(static_cast<std::size_t>(m),
                            static_cast<std::size_t>(n), ValueType::DOUBLE, mr);
    double *hd = h.doubleDataMut();
    const std::uint64_t topBit = 1ULL << m;
    std::uint64_t r = 1;  // x^0
    for (long long i = 0; i < n; ++i) {
        for (long long j = 0; j < m; ++j)
            hd[i * m + j] = (r >> j) & 1ULL;     // column-major, row j
        r <<= 1;                                  // multiply by x
        if (r & topBit) r ^= pmask;               // reduce mod p
    }

    HammgenResult res;
    res.h = std::move(h);
    res.g = gen2parImpl(res.h, "hammgen", mr);    // first m cols are I_m
    res.n = n;
    res.k = k;
    return res;
}

// ── cyclpoly ────────────────────────────────────────────────────────────

Value cyclpoly(long long n, long long k, const std::string &opt,
               std::pmr::memory_resource *mr)
{
    if (n < 1)
        throw Error("cyclpoly: N must be a positive integer",
                    0, 0, "cyclpoly", "", "numkit:cyclpoly:N");
    if (k < 1 || k > n)
        throw Error("cyclpoly: K must satisfy 1 <= K <= N",
                    0, 0, "cyclpoly", "", "numkit:cyclpoly:K");

    const long long m = n - k;
    if (m == 0) return polyToRow({1}, mr);
    if (m == 1) return polyToRow({1, 1}, mr);

    // x^n + 1 over GF(2).
    std::vector<int> pp(static_cast<std::size_t>(n) + 1, 0);
    pp[0] = 1; pp[static_cast<std::size_t>(n)] = 1;
    const long long nn = (1LL << (m - 1)) - 1;

    // Candidate degree-m polynomial 1 + (m-1 inner bits of i) + x^m.
    auto candidate = [&](long long i, bool msbFirst) {
        std::vector<int> t(static_cast<std::size_t>(m) + 1, 0);
        t[0] = 1; t[static_cast<std::size_t>(m)] = 1;
        for (long long p = 0; p < m - 1; ++p)
            t[static_cast<std::size_t>(1 + p)] =
                msbFirst ? static_cast<int>((i >> (m - 2 - p)) & 1)
                         : static_cast<int>((i >> p) & 1);
        return t;
    };
    auto divides = [&](const std::vector<int> &t) {
        return polyDeg(polyMod(pp, t)) < 0;  // remainder is zero
    };
    auto terms = [](const std::vector<int> &t) {
        int s = 0; for (int x : t) s += x; return s;
    };

    const std::string o = toLower(opt);
    if (o.empty()) {                                   // first found
        for (long long i = 0; i <= nn; ++i) {
            auto t = candidate(i, true);
            if (divides(t)) return polyToRow(t, mr);
        }
    } else if (o.rfind("mi", 0) == 0) {                // fewest terms
        for (long long j = 2; j <= m + 1; ++j)
            for (long long i = 0; i <= nn; ++i) {
                auto t = candidate(i, true);
                if (terms(t) == j && divides(t)) return polyToRow(t, mr);
            }
    } else if (o.rfind("ma", 0) == 0) {                // most terms
        for (long long j = m + 1; j >= 2; --j)
            for (long long i = 0; i <= nn; ++i) {
                auto t = candidate(i, false);          // LSB-first, per MATLAB
                if (terms(t) == j && divides(t)) return polyToRow(t, mr);
            }
    } else {                                           // "all" / exhaustive
        std::vector<std::vector<int>> rows;
        for (long long i = 0; i <= nn; ++i) {
            auto t = candidate(i, true);
            if (divides(t)) rows.push_back(std::move(t));
        }
        std::stable_sort(rows.begin(), rows.end(),
            [&](const std::vector<int> &a, const std::vector<int> &b) {
                return terms(a) < terms(b);
            });
        if (rows.empty()) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
        Value out = Value::matrix(rows.size(), static_cast<std::size_t>(m) + 1,
                                  ValueType::DOUBLE, mr);
        double *d = out.doubleDataMut();
        const std::size_t R = rows.size();
        for (std::size_t r = 0; r < R; ++r)
            for (std::size_t c = 0; c < rows[r].size(); ++c)
                d[c * R + r] = rows[r][c];
        return out;
    }
    return Value::matrix(0, 0, ValueType::DOUBLE, mr);  // none found
}

// ── cyclgen ─────────────────────────────────────────────────────────────

CyclgenResult cyclgen(long long n, const Value &genpoly,
                      const std::string &opt, std::pmr::memory_resource *mr)
{
    std::vector<int> p = readPolyRow(genpoly);
    const int m = polyDeg(p);
    if (m < 1)
        throw Error("cyclgen: generator polynomial must have degree >= 1",
                    0, 0, "cyclgen", "", "numkit:cyclgen:P");
    const long long k = n - m;
    if (k < 1)
        throw Error("cyclgen: N must exceed the generator polynomial degree",
                    0, 0, "cyclgen", "", "numkit:cyclgen:N");

    // p must divide x^n + 1.
    std::vector<int> pp(static_cast<std::size_t>(n) + 1, 0);
    pp[0] = 1; pp[static_cast<std::size_t>(n)] = 1;
    if (polyDeg(polyMod(pp, p)) >= 0)
        throw Error("cyclgen: generator polynomial must divide x^N - 1",
                    0, 0, "cyclgen", "", "numkit:cyclgen:P2");

    CyclgenResult res;
    res.k = k;
    const bool nonsys = toLower(opt).find("no") != std::string::npos;

    if (nonsys) {
        // Parity poly h(x) = (x^n + 1) / p(x), degree k.
        std::vector<int> pc = polyDivMod(pp, p).q;
        pc.resize(static_cast<std::size_t>(k) + 1, 0);
        std::vector<int> hrow(static_cast<std::size_t>(n), 0);
        for (long long j = 0; j <= k; ++j) hrow[j] = pc[k - j];   // fliplr(pc)
        res.h = shiftMatrix(hrow, n - k, n, mr);
        std::vector<int> grow(static_cast<std::size_t>(n), 0);
        for (std::size_t j = 0; j < p.size(); ++j) grow[j] = p[j];
        res.g = shiftMatrix(grow, k, n, mr);
    } else {
        // Systematic: b(i,:) = x^(m+i) mod p (k×m); h=[I_m | b'], g=[b | I_k].
        std::vector<std::vector<int>> b(static_cast<std::size_t>(k));
        for (long long i = 0; i < k; ++i) {
            std::vector<int> xpow(static_cast<std::size_t>(m + i) + 1, 0);
            xpow[static_cast<std::size_t>(m + i)] = 1;
            auto rem = polyMod(xpow, p);
            rem.resize(static_cast<std::size_t>(m), 0);
            b[static_cast<std::size_t>(i)] = std::move(rem);
        }
        const std::size_t M = static_cast<std::size_t>(m);
        const std::size_t K = static_cast<std::size_t>(k);
        const std::size_t N = static_cast<std::size_t>(n);
        res.h = Value::matrix(M, N, ValueType::DOUBLE, mr);
        double *hd = res.h.doubleDataMut();
        for (std::size_t r = 0; r < M; ++r) {
            for (std::size_t c = 0; c < M; ++c) hd[c * M + r] = (r == c) ? 1 : 0;
            for (std::size_t t = 0; t < K; ++t) hd[(M + t) * M + r] = b[t][r];
        }
        res.g = Value::matrix(K, N, ValueType::DOUBLE, mr);
        double *gd = res.g.doubleDataMut();
        for (std::size_t r = 0; r < K; ++r) {
            for (std::size_t c = 0; c < M; ++c) gd[c * K + r] = b[r][c];
            for (std::size_t t = 0; t < K; ++t) gd[(M + t) * K + r] = (r == t) ? 1 : 0;
        }
    }
    return res;
}

// ── encode / decode shared helpers ──────────────────────────────────────

namespace {

// Convert a numeric Value matrix to a row-major 0/1 matrix.
std::vector<std::vector<int>> matToRows(const Value &v)
{
    const std::size_t R = v.dims().rows(), C = v.dims().cols();
    std::vector<std::vector<int>> rows(R, std::vector<int>(C));
    for (std::size_t i = 0; i < R; ++i)
        for (std::size_t j = 0; j < C; ++j) rows[i][j] = matBit(v, i, j);
    return rows;
}

// Split "hamming/decimal" into base="hamming" + decimal=true.
void parseMethod(const std::string &method, std::string &base, bool &decimal,
                 const char *who)
{
    const std::string m = toLower(method);
    decimal = m.find("decimal") != std::string::npos;
    if (m.rfind("hamming", 0) == 0)      base = "hamming";
    else if (m.rfind("linear", 0) == 0)  base = "linear";
    else if (m.rfind("cyclic", 0) == 0)  base = "cyclic";
    else throw Error(std::string(who) + ": unknown coding method '" + method + "'",
                     0, 0, who, "", std::string("numkit:") + who + ":method");
}

// Build the k×n generator (gen) and (n-k)×n parity-check (h) matrices for
// the requested method, as row-major 0/1 matrices.
void buildCode(const std::string &base, long long n, long long k,
               const Value &opt, const char *who,
               std::vector<std::vector<int>> &gen,
               std::vector<std::vector<int>> &h,
               std::pmr::memory_resource *mr)
{
    if (base == "hamming") {
        const long long m = n - k;
        if (((1LL << m) - 1) != n)
            throw Error(std::string(who) + ": N and K do not form a Hamming "
                        "code (2^(N-K)-1 must equal N)",
                        0, 0, who, "", std::string("numkit:") + who + ":nklen");
        HammgenResult hr = hammgen(m, opt.isEmpty() ? Value::Empty : opt, mr);
        h = matToRows(hr.h);
        gen = matToRows(hr.g);
    } else if (base == "linear") {
        if (opt.isEmpty())
            throw Error(std::string(who) + ": the linear method requires a "
                        "generator matrix",
                        0, 0, who, "", std::string("numkit:") + who + ":genmat");
        if (static_cast<long long>(opt.dims().rows()) != k ||
            static_cast<long long>(opt.dims().cols()) != n)
            throw Error(std::string(who) + ": generator matrix must be K x N",
                        0, 0, who, "", std::string("numkit:") + who + ":gendims");
        gen = matToRows(opt);
        h = matToRows(gen2par(opt, mr));
    } else {  // cyclic
        Value gp = opt;
        if (opt.isEmpty()) {
            gp = cyclpoly(n, k, "", mr);
            if (gp.numel() == 0)
                throw Error(std::string(who) + ": no valid cyclic generator "
                            "polynomial for (N,K)",
                            0, 0, who, "", std::string("numkit:") + who + ":genpoly");
        }
        CyclgenResult cr = cyclgen(n, gp, "system", mr);
        gen = matToRows(cr.g);
        h = matToRows(cr.h);
    }
}

// MSB-first syndrome integer of error pattern e against parity-check h
// (m×n): bit j (row j of h) carries weight 2^(m-1-j).
inline long long syndromeInt(const std::vector<std::vector<int>> &h,
                             const std::vector<int> &e, long long m, long long n)
{
    long long s = 0;
    for (long long j = 0; j < m; ++j) {
        int b = 0;
        for (long long i = 0; i < n; ++i) b ^= (h[j][i] & e[i]);
        s = (s << 1) | (b & 1);
    }
    return s;
}

// Standard-array syndrome decoding table: row (syndrome integer) -> the
// minimum-weight coset leader (error pattern). Weight 0, then 1, 2, ...
// until every syndrome has a leader. (Tie-breaking for weight >= 2 leaders
// follows lexicographic position order, which suffices for the common
// single-error-correcting codes.)
std::vector<std::vector<int>>
buildSyndTable(const std::vector<std::vector<int>> &h, long long n, long long m)
{
    const long long T = 1LL << m;
    std::vector<std::vector<int>> table(static_cast<std::size_t>(T));
    std::vector<char> filled(static_cast<std::size_t>(T), 0);
    long long remaining = T;

    std::vector<int> e(static_cast<std::size_t>(n), 0);
    const long long s0 = syndromeInt(h, e, m, n);
    table[static_cast<std::size_t>(s0)] = e;
    filled[static_cast<std::size_t>(s0)] = 1;
    --remaining;

    for (long long w = 1; w <= n && remaining > 0; ++w) {
        std::vector<long long> idx(static_cast<std::size_t>(w));
        for (long long t = 0; t < w; ++t) idx[static_cast<std::size_t>(t)] = t;
        while (true) {
            std::vector<int> ep(static_cast<std::size_t>(n), 0);
            for (long long t = 0; t < w; ++t) ep[static_cast<std::size_t>(idx[t])] = 1;
            const long long s = syndromeInt(h, ep, m, n);
            if (!filled[static_cast<std::size_t>(s)]) {
                table[static_cast<std::size_t>(s)] = std::move(ep);
                filled[static_cast<std::size_t>(s)] = 1;
                if (--remaining == 0) break;
            }
            long long t = w - 1;
            while (t >= 0 && idx[static_cast<std::size_t>(t)] == n - w + t) --t;
            if (t < 0) break;
            ++idx[static_cast<std::size_t>(t)];
            for (long long u = t + 1; u < w; ++u)
                idx[static_cast<std::size_t>(u)] = idx[static_cast<std::size_t>(u - 1)] + 1;
        }
    }
    return table;
}

} // namespace

// ── encode ──────────────────────────────────────────────────────────────

EncodeResult encode(const Value &msg, long long n, long long k,
                    const std::string &method, const Value &opt,
                    std::pmr::memory_resource *mr)
{
    if (n < 1) throw Error("encode: N must be a positive integer",
                           0, 0, "encode", "", "numkit:encode:N");
    if (k < 1) throw Error("encode: K must be a positive integer",
                           0, 0, "encode", "", "numkit:encode:K");
    if (n <= k) throw Error("encode: N must exceed K",
                            0, 0, "encode", "", "numkit:encode:nlek");

    std::string base; bool decimal;
    parseMethod(method, base, decimal, "encode");

    const std::size_t R = msg.dims().rows(), C = msg.dims().cols();
    const std::size_t nel = msg.numel();
    const bool isVector = (R == 1 || C == 1);
    const bool isRowVector = isVector && (R == 1);

    std::vector<std::vector<int>> words;
    long long added = 0;
    int typeFlag = 0;

    if (decimal) {
        typeFlag = 1;
        if (!isVector)
            throw Error("encode: message must be a vector for the decimal format",
                        0, 0, "encode", "", "numkit:encode:msgvec");
        const double maxv = std::ldexp(1.0, static_cast<int>(k)) - 1.0;
        for (std::size_t w = 0; w < nel; ++w) {
            const double x = msg.elemAsDouble(w);
            if (x < 0 || x > maxv || std::floor(x) != x)
                throw Error("encode: decimal message entries must be integers "
                            "in [0, 2^K-1]",
                            0, 0, "encode", "", "numkit:encode:msgint");
            std::vector<int> word(static_cast<std::size_t>(k), 0);
            const unsigned long long xi = static_cast<unsigned long long>(x);
            for (long long p = 0; p < k; ++p)
                word[static_cast<std::size_t>(p)] = static_cast<int>((xi >> p) & 1ULL);
            words.push_back(std::move(word));
        }
    } else {
        for (std::size_t i = 0; i < nel; ++i) {
            const double x = msg.elemAsDouble(i);
            if (x != 0.0 && x != 1.0)
                throw Error("encode: binary message must contain only 0s and 1s",
                            0, 0, "encode", "", "numkit:encode:bin");
        }
        if (isVector) {
            typeFlag = 2;
            added = static_cast<long long>(nel % static_cast<std::size_t>(k));
            if (added) added = k - added;
            const std::size_t numWords = (nel + static_cast<std::size_t>(added))
                                         / static_cast<std::size_t>(k);
            for (std::size_t w = 0; w < numWords; ++w) {
                std::vector<int> word(static_cast<std::size_t>(k), 0);
                for (long long p = 0; p < k; ++p) {
                    const std::size_t idx = w * static_cast<std::size_t>(k) + p;
                    if (idx < nel) word[static_cast<std::size_t>(p)] =
                        msg.elemAsDouble(idx) != 0.0 ? 1 : 0;
                }
                words.push_back(std::move(word));
            }
        } else {
            if (static_cast<long long>(C) != k)
                throw Error("encode: message matrix must have K columns",
                            0, 0, "encode", "", "numkit:encode:cols");
            typeFlag = 0;
            for (std::size_t w = 0; w < R; ++w) {
                std::vector<int> word(static_cast<std::size_t>(k));
                for (long long p = 0; p < k; ++p)
                    word[static_cast<std::size_t>(p)] = matBit(msg, w, static_cast<std::size_t>(p));
                words.push_back(std::move(word));
            }
        }
    }

    std::vector<std::vector<int>> gen, h;
    buildCode(base, n, k, opt, "encode", gen, h, mr);

    const std::size_t numWords = words.size();
    const std::size_t N = static_cast<std::size_t>(n);
    std::vector<std::vector<int>> code(numWords, std::vector<int>(N, 0));
    for (std::size_t w = 0; w < numWords; ++w)
        for (long long c = 0; c < n; ++c) {
            int b = 0;
            for (long long p = 0; p < k; ++p)
                b ^= (words[w][static_cast<std::size_t>(p)] & gen[static_cast<std::size_t>(p)][static_cast<std::size_t>(c)]);
            code[w][static_cast<std::size_t>(c)] = b & 1;
        }

    EncodeResult res;
    res.added = added;
    if (typeFlag == 1) {
        res.code = isRowVector ? Value::matrix(1, numWords, ValueType::DOUBLE, mr)
                               : Value::matrix(numWords, 1, ValueType::DOUBLE, mr);
        double *d = res.code.doubleDataMut();
        for (std::size_t w = 0; w < numWords; ++w) {
            double v = 0;
            for (long long p = 0; p < n; ++p)
                if (code[w][static_cast<std::size_t>(p)]) v += std::ldexp(1.0, static_cast<int>(p));
            d[w] = v;
        }
    } else if (typeFlag == 2) {
        const std::size_t L = numWords * N;
        res.code = isRowVector ? Value::matrix(1, L, ValueType::DOUBLE, mr)
                               : Value::matrix(L, 1, ValueType::DOUBLE, mr);
        double *d = res.code.doubleDataMut();
        for (std::size_t w = 0; w < numWords; ++w)
            for (std::size_t p = 0; p < N; ++p) d[w * N + p] = code[w][p];
    } else {
        res.code = Value::matrix(numWords, N, ValueType::DOUBLE, mr);
        double *d = res.code.doubleDataMut();
        for (std::size_t w = 0; w < numWords; ++w)
            for (std::size_t c = 0; c < N; ++c) d[c * numWords + w] = code[w][c];
    }
    return res;
}

// ── decode ──────────────────────────────────────────────────────────────

DecodeResult decode(const Value &code, long long n, long long k,
                    const std::string &method, const Value &opt,
                    std::pmr::memory_resource *mr)
{
    if (n < 1) throw Error("decode: N must be a positive integer",
                           0, 0, "decode", "", "numkit:decode:N");
    if (k < 1) throw Error("decode: K must be a positive integer",
                           0, 0, "decode", "", "numkit:decode:K");
    if (n <= k) throw Error("decode: N must exceed K",
                            0, 0, "decode", "", "numkit:decode:nlek");

    std::string base; bool decimal;
    parseMethod(method, base, decimal, "decode");

    const std::size_t R = code.dims().rows(), C = code.dims().cols();
    const std::size_t nel = code.numel();
    const bool isVector = (R == 1 || C == 1);
    const bool isRowVector = isVector && (R == 1);
    const std::size_t N = static_cast<std::size_t>(n);

    std::vector<std::vector<int>> wordsIn;
    int typeFlag = 0;

    if (decimal) {
        typeFlag = 1;
        if (!isVector)
            throw Error("decode: code must be a vector for the decimal format",
                        0, 0, "decode", "", "numkit:decode:codevec");
        const double maxv = std::ldexp(1.0, static_cast<int>(n)) - 1.0;
        for (std::size_t w = 0; w < nel; ++w) {
            const double x = code.elemAsDouble(w);
            if (x < 0 || x > maxv || std::floor(x) != x)
                throw Error("decode: decimal code entries must be integers in "
                            "[0, 2^N-1]",
                            0, 0, "decode", "", "numkit:decode:codeint");
            std::vector<int> word(N, 0);
            const unsigned long long xi = static_cast<unsigned long long>(x);
            for (std::size_t p = 0; p < N; ++p) word[p] = static_cast<int>((xi >> p) & 1ULL);
            wordsIn.push_back(std::move(word));
        }
    } else {
        for (std::size_t i = 0; i < nel; ++i) {
            const double x = code.elemAsDouble(i);
            if (x != 0.0 && x != 1.0)
                throw Error("decode: binary code must contain only 0s and 1s",
                            0, 0, "decode", "", "numkit:decode:bin");
        }
        if (isVector) {
            typeFlag = 2;
            long long pad = static_cast<long long>(nel % N);
            if (pad) pad = static_cast<long long>(N) - pad;
            const std::size_t numWords = (nel + static_cast<std::size_t>(pad)) / N;
            for (std::size_t w = 0; w < numWords; ++w) {
                std::vector<int> word(N, 0);
                for (std::size_t p = 0; p < N; ++p) {
                    const std::size_t idx = w * N + p;
                    if (idx < nel) word[p] = code.elemAsDouble(idx) != 0.0 ? 1 : 0;
                }
                wordsIn.push_back(std::move(word));
            }
        } else {
            if (static_cast<long long>(C) != n)
                throw Error("decode: code matrix must have N columns",
                            0, 0, "decode", "", "numkit:decode:cols");
            typeFlag = 0;
            for (std::size_t w = 0; w < R; ++w) {
                std::vector<int> word(N);
                for (std::size_t p = 0; p < N; ++p) word[p] = matBit(code, w, p);
                wordsIn.push_back(std::move(word));
            }
        }
    }

    std::vector<std::vector<int>> gen, h;
    buildCode(base, n, k, opt, "decode", gen, h, mr);
    const long long m = n - k;
    const std::vector<std::vector<int>> trt = buildSyndTable(h, n, m);

    // Systematic message columns: gen = [I_k | ...] or [... | I_k].
    bool firstK = true, lastK = true;
    for (long long i = 0; i < k && (firstK || lastK); ++i)
        for (long long j = 0; j < k; ++j) {
            const int want = (i == j) ? 1 : 0;
            if (gen[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] != want) firstK = false;
            if (gen[static_cast<std::size_t>(i)][static_cast<std::size_t>(n - k + j)] != want) lastK = false;
        }
    if (!firstK && !lastK)
        throw Error("decode: generator matrix is not in systematic form",
                    0, 0, "decode", "", "numkit:decode:gensys");
    const long long msgOff = firstK ? 0 : (n - k);

    const std::size_t numWords = wordsIn.size();
    const std::size_t K = static_cast<std::size_t>(k);
    std::vector<std::vector<int>> msgRows(numWords, std::vector<int>(K, 0));
    std::vector<long long> errCnt(numWords, 0);
    for (std::size_t w = 0; w < numWords; ++w) {
        const long long s = syndromeInt(h, wordsIn[w], m, n);
        const std::vector<int> &leader = trt[static_cast<std::size_t>(s)];
        long long cnt = 0;
        for (std::size_t p = 0; p < K; ++p)
            msgRows[w][p] = wordsIn[w][static_cast<std::size_t>(msgOff) + p] ^ leader[static_cast<std::size_t>(msgOff) + p];
        for (std::size_t p = 0; p < N; ++p) cnt += leader[p];
        errCnt[w] = cnt;
    }

    DecodeResult res;
    // msg output
    if (typeFlag == 1) {
        res.msg = isRowVector ? Value::matrix(1, numWords, ValueType::DOUBLE, mr)
                              : Value::matrix(numWords, 1, ValueType::DOUBLE, mr);
        double *d = res.msg.doubleDataMut();
        for (std::size_t w = 0; w < numWords; ++w) {
            double v = 0;
            for (std::size_t p = 0; p < K; ++p)
                if (msgRows[w][p]) v += std::ldexp(1.0, static_cast<int>(p));
            d[w] = v;
        }
    } else if (typeFlag == 2) {
        const std::size_t L = numWords * K;
        res.msg = isRowVector ? Value::matrix(1, L, ValueType::DOUBLE, mr)
                              : Value::matrix(L, 1, ValueType::DOUBLE, mr);
        double *d = res.msg.doubleDataMut();
        for (std::size_t w = 0; w < numWords; ++w)
            for (std::size_t p = 0; p < K; ++p) d[w * K + p] = msgRows[w][p];
    } else {
        res.msg = Value::matrix(numWords, K, ValueType::DOUBLE, mr);
        double *d = res.msg.doubleDataMut();
        for (std::size_t w = 0; w < numWords; ++w)
            for (std::size_t c = 0; c < K; ++c) d[c * numWords + w] = msgRows[w][c];
    }
    // err output: one count per word (column).
    res.err = Value::matrix(numWords, 1, ValueType::DOUBLE, mr);
    double *ed = res.err.doubleDataMut();
    for (std::size_t w = 0; w < numWords; ++w) ed[w] = static_cast<double>(errCnt[w]);
    return res;
}

} // namespace numkit::comm
