// toolboxes/image/src/morph/morph_reg.cpp
//
// Register half of the image morphology builtins: the CallContext wrappers
// (two detail blocks, mirroring the interleaved compute TU) plus the VM-pausable
// makelut callback (MakelutCallbackBuiltin + registerMakelutCallbackBuiltin),
// all delegating to the engine-free compute in morph.cpp. library.cpp
// forward-declares + registers these by name. core/callback_builtin.hpp +
// core/vm.hpp (the continuation machinery) live here on the register side.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/morph/morph.hpp>

#include <numkit/core/callback_builtin.hpp>
#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/core/vm.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace numkit::image {

// [Phase 2b] relocated from morph.cpp compute: the reg-only strel default
// (diamond_cross) and the synchronous Engine-driven makelut. The VM-pausable
// path is MakelutCallbackBuiltin below.
namespace {
Value diamond_cross(std::pmr::memory_resource *mr) {
    Value se = Value::matrix(3, 3, ValueType::LOGICAL, mr);
    std::uint8_t *d = se.logicalDataMut();
    // Column-major fill: cols [c0(r0..2), c1(r0..2), c2(r0..2)].
    // Pattern: [0 1 0; 1 1 1; 0 1 0] — center cross.
    static constexpr std::uint8_t pat[9] = {0,1,0, 1,1,1, 0,1,0};
    for (size_t i = 0; i < 9; ++i) d[i] = pat[i];
    return se;
}
} // anonymous

// makelut — build a bwlookup/applylut table by evaluating `fun` on every
// 2^(n²) binary n×n neighbourhood. Index k's neighbourhood (col-major)
// has position i set to bit (nq-1-i) of k — the inverse of the
// reshape(2^[nq-1:-1:0], n, n) weight kernel used by applylut, so
// bwlookup(BW, makelut(fun, n)) applies fun to each neighbourhood.
Value makelut(numkit::Engine &eng, const Value &fun, int n,
              std::pmr::memory_resource *mr)
{
    if (n != 2 && n != 3)
        throw Error("makelut: N must be 2 or 3.",
                    0, 0, "makelut", "", "numkit:makelut:badN");
    const int nq = n * n;
    const size_t N = size_t{1} << nq;   // 16 or 512
    Value lut = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *ld = lut.doubleDataMut();

    // Reusable logical neighbourhood, mutated per table entry.
    Value nh = Value::matrix(static_cast<size_t>(n), static_cast<size_t>(n),
                             ValueType::LOGICAL, mr);
    uint8_t *nd = nh.logicalDataMut();
    for (size_t k = 0; k < N; ++k) {
        for (int i = 0; i < nq; ++i)
            nd[i] = static_cast<uint8_t>((k >> (nq - 1 - i)) & size_t{1});
        Value r = eng.callFunctionHandle(fun, Span<const Value>(&nh, 1));
        if (r.numel() != 1)
            throw Error("makelut: fun must return a scalar",
                        0, 0, "makelut", "", "numkit:makelut:funScalar");
        ld[k] = r.toScalar();
    }
    return lut;
}

namespace detail {

void strel_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strel: requires shape", 0, 0, "strel", "",
                    "numkit:strel:nargin");
    std::string shape = "square";
    if (args[0].isChar() || args[0].isString()) shape = args[0].toString();
    std::vector<double> params;
    Value arbitrary;
    for (size_t i = 1; i < args.size(); ++i) {
        if (!(args[i].isChar() || args[i].isString())) {
            if (shape == "arbitrary") { arbitrary = args[i]; continue; }
            for (size_t j = 0; j < args[i].numel(); ++j)
                params.push_back(args[i].elemAsDouble(j));
        }
    }
    outs[0] = strel(shape, params, arbitrary, ctx.engine->resource());
}

#define NK_MORPH_REG(name)                                                       \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                 \
                    Span<Value> outs, CallContext &ctx)                         \
    {                                                                             \
        if (args.size() < 2)                                                      \
            throw Error(#name ": requires (I, SE)", 0, 0, #name, "",             \
                        "numkit:" #name ":nargin");                                   \
        outs[0] = name(args[0], args[1], ctx.engine->resource());                \
    }

NK_MORPH_REG(imerode)
NK_MORPH_REG(imdilate)
NK_MORPH_REG(imopen)
NK_MORPH_REG(imclose)

#undef NK_MORPH_REG

void imreconstruct_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imreconstruct: requires (marker, mask [, conn])",
                    0, 0, "imreconstruct", "", "numkit:imreconstruct:nargin");
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? static_cast<int>(args[2].toScalar()) : 8;
    outs[0] = imreconstruct(args[0], args[1], conn, ctx.engine->resource());
}

void imfill_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imfill: requires (BW, 'holes' [, conn])",
                    0, 0, "imfill", "", "numkit:imfill:nargin");
    auto *mr = ctx.engine->resource();
    // Currently we support `imfill(BW, 'holes' [, conn])` only.
    if (args.size() < 2 ||
        !(args[1].isChar() || args[1].isString()))
        throw Error("imfill: only the 'holes' mode is implemented",
                    0, 0, "imfill", "", "numkit:imfill:mode");
    const std::string mode = args[1].toString();
    if (mode != "holes" && mode != "Holes" && mode != "HOLES")
        throw Error("imfill: only 'holes' mode is implemented "
                    "(seed-list mode not yet supported)",
                    0, 0, "imfill", "", "numkit:imfill:mode");
    // Default connectivity is 4 — MATLAB's imfill uses conndef(2,'minimal').
    // With 8-connectivity the background flood leaks through 1-px diagonal
    // gaps in the foreground, so enclosed regions wrongly read as
    // border-reachable and don't fill.
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? static_cast<int>(args[2].toScalar()) : 4;
    outs[0] = imfill_holes(args[0], conn, mr);
}

void imregionalmax_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imregionalmax: requires (I [, conn])",
                    0, 0, "imregionalmax", "", "numkit:imregionalmax:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? static_cast<int>(args[1].toScalar()) : 8;
    outs[0] = imregionalmax(args[0], conn, ctx.engine->resource());
}

void imregionalmin_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imregionalmin: requires (I [, conn])",
                    0, 0, "imregionalmin", "", "numkit:imregionalmin:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? static_cast<int>(args[1].toScalar()) : 8;
    outs[0] = imregionalmin(args[0], conn, ctx.engine->resource());
}

void imhmax_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imhmax: requires (I, h [, conn])",
                    0, 0, "imhmax", "", "numkit:imhmax:nargin");
    const double h = args[1].toScalar();
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? static_cast<int>(args[2].toScalar()) : 8;
    outs[0] = imhmax(args[0], h, conn, ctx.engine->resource());
}

void imhmin_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imhmin: requires (I, h [, conn])",
                    0, 0, "imhmin", "", "numkit:imhmin:nargin");
    const double h = args[1].toScalar();
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? static_cast<int>(args[2].toScalar()) : 8;
    outs[0] = imhmin(args[0], h, conn, ctx.engine->resource());
}

void imextendedmax_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imextendedmax: requires (I, h [, conn])",
                    0, 0, "imextendedmax", "", "numkit:imextendedmax:nargin");
    const double h = args[1].toScalar();
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? static_cast<int>(args[2].toScalar()) : 8;
    outs[0] = imextendedmax(args[0], h, conn, ctx.engine->resource());
}

void imextendedmin_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imextendedmin: requires (I, h [, conn])",
                    0, 0, "imextendedmin", "", "numkit:imextendedmin:nargin");
    const double h = args[1].toScalar();
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? static_cast<int>(args[2].toScalar()) : 8;
    outs[0] = imextendedmin(args[0], h, conn, ctx.engine->resource());
}

void imimposemin_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imimposemin: requires (I, BW [, conn])",
                    0, 0, "imimposemin", "", "numkit:imimposemin:nargin");
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? static_cast<int>(args[2].toScalar()) : 8;
    outs[0] = imimposemin(args[0], args[1], conn, ctx.engine->resource());
}

void imclearborder_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imclearborder: requires (BW [, conn])",
                    0, 0, "imclearborder", "", "numkit:imclearborder:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? static_cast<int>(args[1].toScalar()) : 8;
    outs[0] = imclearborder(args[0], conn, ctx.engine->resource());
}

void imkeepborder_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imkeepborder: requires (BW [, conn])",
                    0, 0, "imkeepborder", "", "numkit:imkeepborder:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? static_cast<int>(args[1].toScalar()) : 8;
    outs[0] = imkeepborder(args[0], conn, ctx.engine->resource());
}

void imtophat_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imtophat: requires (I, SE)", 0, 0, "imtophat", "",
                    "numkit:imtophat:nargin");
    outs[0] = imtophat(args[0], args[1], ctx.engine->resource());
}

void imbothat_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imbothat: requires (I, SE)", 0, 0, "imbothat", "",
                    "numkit:imbothat:nargin");
    outs[0] = imbothat(args[0], args[1], ctx.engine->resource());
}

void mmgradm_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mmgradm: requires (I [, se_dil [, se_ero]])",
                    0, 0, "mmgradm", "", "numkit:mmgradm:nargin");
    auto *mr = ctx.engine->resource();
    // Defaults: arg omitted → elementary cross. Arg explicitly empty
    // means half-gradient (the C++ function reads .numel() == 0).
    Value sed = (args.size() >= 2) ? args[1] : diamond_cross(mr);
    Value see = (args.size() >= 3) ? args[2] : diamond_cross(mr);
    outs[0] = mmgradm(args[0], sed, see, mr);
}

void bwpack_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwpack: requires (BW)", 0, 0, "bwpack", "",
                    "numkit:bwpack:nargin");
    outs[0] = bwpack(args[0], ctx.engine->resource());
}

void bwunpack_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwunpack: requires (BWP [, M])",
                    0, 0, "bwunpack", "", "numkit:bwunpack:nargin");
    size_t M = static_cast<size_t>(-1);
    if (args.size() >= 2 && !args[1].isEmpty())
        M = static_cast<size_t>(args[1].toScalar());
    outs[0] = bwunpack(args[0], M, ctx.engine->resource());
}

void applylut_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("applylut: requires (BW, LUT)",
                    0, 0, "applylut", "", "numkit:applylut:nargin");
    outs[0] = applylut(args[0], args[1], ctx.engine->resource());
}

void bwlookup_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bwlookup: requires (BW, lut)",
                    0, 0, "bwlookup", "", "numkit:bwlookup:nargin");
    outs[0] = bwlookup(args[0], args[1], ctx.engine->resource());
}

// State-machine makelut (VM_CALLBACKS_PLAN.md): evaluate a user-code handle on
// every n×n binary neighbourhood as a pausable VM frame (mirrors makelut()).
// Builtin handles / bad N fall back to the synchronous makelut_reg.
struct MakelutCallbackBuiltin : CallbackBuiltin
{
    std::shared_ptr<VmContinuation> tryStart(Span<const Value> args, std::size_t nargout,
                                             Value *dest, Engine &eng) override
    {
        if (args.size() < 2 || nargout > 1)
            return nullptr;
        if (!eng.isUserCodeHandle(args[0]))
            return nullptr; // builtin handle → synchronous makelut
        const int n = static_cast<int>(args[1].toScalar());
        if (n != 2 && n != 3)
            return nullptr; // sync path reports badN
        const int nq = n * n;
        const std::size_t N = std::size_t{1} << nq;
        auto *mr = eng.resource();
        auto cont = std::make_shared<LoopContinuation>();
        cont->handle = args[0];
        cont->n = N;
        cont->dest = dest;
        cont->makeArgs = [n, nq, mr](std::size_t k) -> std::vector<Value> {
            Value nh = Value::matrix(static_cast<std::size_t>(n), static_cast<std::size_t>(n),
                                     ValueType::LOGICAL, mr);
            uint8_t *nd = nh.logicalDataMut();
            for (int i = 0; i < nq; ++i)
                nd[i] = static_cast<uint8_t>((k >> (nq - 1 - i)) & std::size_t{1});
            return {std::move(nh)};
        };
        cont->pack = [N, mr](std::vector<Value> &results) -> Value {
            Value lut = Value::matrix(N, 1, ValueType::DOUBLE, mr);
            double *ld = lut.doubleDataMut();
            for (std::size_t k = 0; k < results.size(); ++k) {
                if (results[k].numel() != 1)
                    throw Error("makelut: fun must return a scalar", 0, 0, "makelut", "",
                                "numkit:makelut:funScalar");
                ld[k] = results[k].toScalar();
            }
            return lut;
        };
        cont->results.reserve(cont->n);
        return cont;
    }
};

void makelut_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("makelut: requires (fun, n)",
                    0, 0, "makelut", "", "numkit:makelut:nargin");
    const int n = static_cast<int>(args[1].toScalar());
    outs[0] = makelut(*ctx.engine, args[0], n, ctx.engine->resource());
}

void bwmorph3_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || !(args[1].isChar() || args[1].isString()))
        throw Error("bwmorph3: requires (V, operation)",
                    0, 0, "bwmorph3", "", "numkit:bwmorph3:nargin");
    outs[0] = bwmorph3(args[0], args[1].toString(), ctx.engine->resource());
}

void bwhitmiss_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bwhitmiss: requires (BW, interval) or (BW, se1, se2)",
                    0, 0, "bwhitmiss", "", "numkit:bwhitmiss:nargin");
    auto *mr = ctx.engine->resource();
    Value se1, se2;
    if (args.size() == 2) {
        // Single interval matrix with values in {-1, 0, 1}.
        const Value &iv = args[1];
        const size_t H = iv.dims().rows();
        const size_t W = iv.dims().cols();
        const size_t N = iv.numel();
        se1 = Value::matrix(H, W, ValueType::LOGICAL, mr);
        se2 = Value::matrix(H, W, ValueType::LOGICAL, mr);
        std::uint8_t *s1 = se1.logicalDataMut();
        std::uint8_t *s2 = se2.logicalDataMut();
        for (size_t i = 0; i < N; ++i) {
            const double v = iv.elemAsDouble(i);
            s1[i] = (v ==  1.0) ? 1u : 0u;
            s2[i] = (v == -1.0) ? 1u : 0u;
        }
    } else {
        se1 = args[1];
        se2 = args[2];
    }
    outs[0] = bwhitmiss(args[0], se1, se2, mr);
}

} // namespace detail

namespace detail {

void bwmorph_reg(Span<const Value> args, std::size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bwmorph: requires (BW, op[, n])",
                    0, 0, "bwmorph", "", "numkit:bwmorph:nargin");
    if (!args[1].isChar())
        throw Error("bwmorph: op must be a string",
                    0, 0, "bwmorph", "", "numkit:bwmorph:badOp");
    std::string op = args[1].toString();
    // Normalise to lowercase.
    for (auto &ch : op) ch = static_cast<char>(std::tolower(ch));
    // MATLAB accepts "skel" as a prefix-match for "skeleton".
    if (op == "skel") op = "skeleton";

    int n = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) {
        const double v = args[2].toScalar();
        if (std::isinf(v)) n = -1;
        else               n = static_cast<int>(v);
    }
    outs[0] = bwmorph(args[0], op, n, ctx.engine->resource());
}

void bwtraceboundary_reg(Span<const Value> args, std::size_t /*nargout*/,
                         Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("bwtraceboundary: requires (BW, P, FSTEP [, conn] "
                    "[, n, dir])",
                    0, 0, "bwtraceboundary", "",
                    "numkit:bwtraceboundary:nargin");
    auto *mr = ctx.engine->resource();
    if (!args[2].isChar() && !args[2].isString())
        throw Error("bwtraceboundary: FSTEP must be a string",
                    0, 0, "bwtraceboundary", "",
                    "numkit:bwtraceboundary:fstep");
    std::string fstep = args[2].toString();
    // Upper-case fstep.
    for (auto &ch : fstep)
        ch = static_cast<char>(std::toupper(
            static_cast<unsigned char>(ch)));

    int conn = 8;
    std::size_t m_max = std::numeric_limits<std::size_t>::max();
    std::string dir = "clockwise";
    if (args.size() >= 4 && !args[3].isEmpty())
        conn = static_cast<int>(args[3].toScalar());
    if (args.size() >= 5 && !args[4].isEmpty()) {
        const double v = args[4].toScalar();
        if (!std::isinf(v)) {
            if (!(v > 0))
                throw Error("bwtraceboundary: N must be positive or Inf",
                            0, 0, "bwtraceboundary", "",
                            "numkit:bwtraceboundary:n");
            m_max = static_cast<std::size_t>(v);
        }
    }
    if (args.size() >= 6 && !args[5].isEmpty()) {
        if (!args[5].isChar() && !args[5].isString())
            throw Error("bwtraceboundary: DIR must be a string",
                        0, 0, "bwtraceboundary", "",
                        "numkit:bwtraceboundary:dirArg");
        dir = args[5].toString();
        std::string dlo;
        for (char ch : dir)
            dlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        dir = dlo;
    }
    outs[0] = bwtraceboundary(args[0], args[1], fstep, conn,
                              m_max, dir, mr);
}

} // namespace detail

void registerMakelutCallbackBuiltin(Engine &engine)
{
    engine.registerCallbackBuiltin("makelut",
                                   std::make_shared<detail::MakelutCallbackBuiltin>());
}

} // namespace numkit::image
