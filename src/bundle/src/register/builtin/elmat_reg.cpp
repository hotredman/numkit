// src/bundle/src/register/builtin/elmat_reg.cpp

#include <numkit/builtin/elmat.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/callback_builtin.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

namespace numkit::builtin::detail {

void blkdiag_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void circshift_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void colon_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void compan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cylinder_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void diag_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ellipsoid_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void eye_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void false_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void find_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void flip_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fliplr_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void flipud_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void freqspace_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gallery_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void hadamard_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void hankel_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void hilb_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ind2sub_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void inf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void invhilb_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ipermute_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void iscolumn_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isempty_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isequal_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isequaln_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isequalwithequalnans_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ismatrix_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isrow_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isscalar_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issorted_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issortedrows_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issparse_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isvector_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void kron_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void length_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void linspace_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void logspace_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void magic_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void meshgrid_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ndgrid_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ndims_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nnz_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nonzeros_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void numel_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nzmax_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ones_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pagectranspose_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pagetranspose_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pascal_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void peaks_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void permute_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void repelem_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void repmat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void reshape_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void resize_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rosser_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rot90_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void shiftdim_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void size_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sparse_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sphere_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void squeeze_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sub2ind_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void toeplitz_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void topkrows_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void transpose_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void tril_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void triu_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void true_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void vander_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void wilkinson_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void zeros_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

struct BsxfunCallbackBuiltin : CallbackBuiltin
{
    // Single-shot: bsxfun forwards the WHOLE arrays to the handle in one
    // call (the pinned contract, debug_session_test BreakInsideBsxfunCallback
    // — one pausable frame). Lost when e95e6054 rewrote this per-element.
    std::shared_ptr<VmContinuation> tryStart(Span<const Value> args, std::size_t nargout,
                                             Value *dest, Engine &eng) override
    {
        if (args.size() < 3 || nargout > 1)
            return nullptr;
        if (!eng.isUserCodeHandle(args[0]))
            return nullptr;
        std::vector<Value> callArgs{args[1], args[2]};
        auto cont = std::make_shared<LoopContinuation>();
        cont->handle = args[0];
        cont->n = 1;
        cont->dest = dest;
        cont->makeArgs = [callArgs](std::size_t) -> std::vector<Value> { return callArgs; };
        cont->pack = [](std::vector<Value> &results) -> Value {
            return results.empty() ? Value() : std::move(results[0]);
        };
        cont->results.reserve(1);
        return cont;
    }
};

} // namespace numkit::builtin::detail

namespace numkit::bundle::builtin {

void register_elmat(Engine &engine) {
    engine.registerFunction("blkdiag",    &::numkit::builtin::detail::blkdiag_reg);
    engine.registerFunction("cat",        &::numkit::builtin::detail::cat_reg);
    engine.registerFunction("circshift",  &::numkit::builtin::detail::circshift_reg);
    engine.registerFunction("colon",      &::numkit::builtin::detail::colon_reg);
    engine.registerFunction("compan",     &::numkit::builtin::detail::compan_reg);
    engine.registerFunction("cylinder",   &::numkit::builtin::detail::cylinder_reg);
    engine.registerFunction("ellipsoid",  &::numkit::builtin::detail::ellipsoid_reg);
    engine.registerFunction("eye",        &::numkit::builtin::detail::eye_reg);
    engine.registerFunction("false",      &::numkit::builtin::detail::false_reg);
    engine.registerFunction("flip",       &::numkit::builtin::detail::flip_reg);
    engine.registerFunction("fliplr",     &::numkit::builtin::detail::fliplr_reg);
    engine.registerFunction("flipud",     &::numkit::builtin::detail::flipud_reg);
    engine.registerFunction("hadamard",   &::numkit::builtin::detail::hadamard_reg);
    engine.registerFunction("hankel",     &::numkit::builtin::detail::hankel_reg);
    engine.registerFunction("hilb",       &::numkit::builtin::detail::hilb_reg);
    engine.registerFunction("ind2sub",    &::numkit::builtin::detail::ind2sub_reg);
    engine.registerFunction("inf",        &::numkit::builtin::detail::inf_reg);
    engine.registerFunction("Inf",        &::numkit::builtin::detail::inf_reg);
    engine.registerFunction("invhilb",    &::numkit::builtin::detail::invhilb_reg);
    engine.registerFunction("ipermute",   &::numkit::builtin::detail::ipermute_reg);
    engine.registerFunction("length",     &::numkit::builtin::detail::length_reg);
    engine.registerFunction("magic",      &::numkit::builtin::detail::magic_reg);
    engine.registerFunction("nan",        &::numkit::builtin::detail::nan_reg);
    engine.registerFunction("NaN",        &::numkit::builtin::detail::nan_reg);
    engine.registerFunction("ndims",      &::numkit::builtin::detail::ndims_reg);
    engine.registerFunction("numel",      &::numkit::builtin::detail::numel_reg);
    engine.registerFunction("ones",       &::numkit::builtin::detail::ones_reg);
    engine.registerFunction("pagectranspose", &::numkit::builtin::detail::pagectranspose_reg);
    engine.registerFunction("pagetranspose",  &::numkit::builtin::detail::pagetranspose_reg);
    engine.registerFunction("pascal",     &::numkit::builtin::detail::pascal_reg);
    engine.registerFunction("peaks",      &::numkit::builtin::detail::peaks_reg);
    engine.registerFunction("permute",    &::numkit::builtin::detail::permute_reg);
    engine.registerFunction("repelem",    &::numkit::builtin::detail::repelem_reg);
    engine.registerFunction("repmat",     &::numkit::builtin::detail::repmat_reg);
    engine.registerFunction("reshape",    &::numkit::builtin::detail::reshape_reg);
    engine.registerFunction("resize",     &::numkit::builtin::detail::resize_reg);
    engine.registerFunction("rosser",     &::numkit::builtin::detail::rosser_reg);
    engine.registerFunction("rot90",      &::numkit::builtin::detail::rot90_reg);
    engine.registerFunction("shiftdim",   &::numkit::builtin::detail::shiftdim_reg);
    engine.registerFunction("size",       &::numkit::builtin::detail::size_reg);
    engine.registerFunction("sparse",     &::numkit::builtin::detail::sparse_reg);
    engine.registerFunction("sphere",     &::numkit::builtin::detail::sphere_reg);
    engine.registerFunction("squeeze",    &::numkit::builtin::detail::squeeze_reg);
    engine.registerFunction("sub2ind",    &::numkit::builtin::detail::sub2ind_reg);
    engine.registerFunction("toeplitz",   &::numkit::builtin::detail::toeplitz_reg);
    engine.registerFunction("topkrows",   &::numkit::builtin::detail::topkrows_reg);
    engine.registerFunction("transpose",  &::numkit::builtin::detail::transpose_reg);
    engine.registerFunction("tril",       &::numkit::builtin::detail::tril_reg);
    engine.registerFunction("triu",       &::numkit::builtin::detail::triu_reg);
    engine.registerFunction("true",       &::numkit::builtin::detail::true_reg);
    engine.registerFunction("vander",     &::numkit::builtin::detail::vander_reg);
    engine.registerFunction("wilkinson",  &::numkit::builtin::detail::wilkinson_reg);
    engine.registerFunction("zeros",      &::numkit::builtin::detail::zeros_reg);

    engine.registerFunction("paddata",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw std::runtime_error("paddata requires (X, len[, dim, side])");
            size_t len = static_cast<size_t>(args[1].toScalar());
            int dim = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
            std::string side = (args.size() >= 4) ? args[3].toString() : "right";
            outs[0] = numkit::builtin::paddata(args[0], len, dim, side, ctx.engine->resource());
        });

    engine.registerFunction("trimdata",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw std::runtime_error("trimdata requires (X, len[, dim, side])");
            size_t len = static_cast<size_t>(args[1].toScalar());
            int dim = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
            std::string side = (args.size() >= 4) ? args[3].toString() : "right";
            outs[0] = numkit::builtin::trimdata(args[0], len, dim, side, ctx.engine->resource());
        });

    engine.registerFunction("head",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.empty()) throw std::runtime_error("head requires at least 1 argument");
            size_t k = (args.size() > 1) ? static_cast<size_t>(args[1].toScalar()) : 8;
            outs[0] = numkit::builtin::head(args[0], k, ctx.engine->resource());
        });

    engine.registerFunction("tail",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.empty()) throw std::runtime_error("tail requires at least 1 argument");
            size_t k = (args.size() > 1) ? static_cast<size_t>(args[1].toScalar()) : 8;
            outs[0] = numkit::builtin::tail(args[0], k, ctx.engine->resource());
        });

    engine.registerFunction("bsxfun",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 3)
                throw std::runtime_error("bsxfun requires at least 3 arguments (fun, A, B)");
            const auto &fn = args[0];
            const auto &a = args[1];
            const auto &b = args[2];
            auto *mr = ctx.engine->resource();

            if (fn.isFuncHandle()) {
                const std::string name = fn.funcHandleName();
                if (!name.empty() && name[0] != '@') {
                    try {
                        outs[0] = numkit::builtin::bsxfun(name, a, b, mr);
                        return;
                    } catch (...) {
                    }
                }
            } else if (fn.isChar() || fn.isString()) {
                try {
                    outs[0] = numkit::builtin::bsxfun(fn.toString(), a, b, mr);
                    return;
                } catch (...) {
                }
            }

            if (a.isEmpty() || b.isEmpty()) {
                outs[0] = Value::Empty;
                return;
            }
            size_t ar = a.dims().rows(), ac = a.dims().cols();
            size_t br = b.dims().rows(), bc = b.dims().cols();
            if ((ar != br && ar != 1 && br != 1) || (ac != bc && ac != 1 && bc != 1)) {
                throw std::runtime_error("bsxfun: non-singleton dimensions must match");
            }
            size_t outR = std::max(ar, br);
            size_t outC = std::max(ac, bc);
            auto res = Value::matrix(outR, outC, ValueType::DOUBLE, mr);
            double *outData = res.doubleDataMut();
            for (size_t c = 0; c < outC; ++c) {
                size_t acIdx = (ac == 1) ? 0 : c;
                size_t bcIdx = (bc == 1) ? 0 : c;
                for (size_t r = 0; r < outR; ++r) {
                    size_t arIdx = (ar == 1) ? 0 : r;
                    size_t brIdx = (br == 1) ? 0 : r;
                    Value elemA = Value::scalar(a.elemAsDouble(acIdx * ar + arIdx));
                    Value elemB = Value::scalar(b.elemAsDouble(bcIdx * br + brIdx));
                    Value argsArr[2] = {elemA, elemB};
                    Value callOut = ctx.engine->callFunctionHandle(fn, Span<const Value>(argsArr, 2));
                    outData[c * outR + r] = callOut.toScalar();
                }
            }
            outs[0] = std::move(res);
        });

    engine.registerCallbackBuiltin("bsxfun", std::make_shared<::numkit::builtin::detail::BsxfunCallbackBuiltin>());
}

} // namespace numkit::bundle::builtin
