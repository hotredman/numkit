// src/bundle/src/register/builtin/ops_reg.cpp

#include <nk/builtin/ops.hpp>
#include <nk/bundle/builtin_library.hpp>
#include <nk/core/engine.hpp>
#include <nk/core/types.hpp>
#include <nk/value/value.hpp>
#include <nk/value/span.hpp>

namespace nk::builtin::detail {
void all_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void and_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void any_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ctranspose_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cummax_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cummin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cumprod_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cumsum_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void diag_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void diff_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void eq_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void find_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ge_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gt_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void horzcat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ldivide_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void le_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void lt_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void meshgrid_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void minus_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mldivide_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mpower_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mrdivide_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mtimes_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ndgrid_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ne_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nnz_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nonzeros_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void not_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void or_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pagemtimes_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void plus_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void power_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rdivide_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sort_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sortrows_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void times_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uminus_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uplus_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void vertcat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void xor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
} // namespace nk::builtin::detail

namespace nk {

void BuiltinLibrary::registerBinaryOps(Engine &engine)
{
    engine.registerBinaryOp("+",  [&engine](const Value &a, const Value &b) { return nk::builtin::plus(a, b, engine.resource()); });
    engine.registerBinaryOp("-",  [&engine](const Value &a, const Value &b) { return nk::builtin::minus(a, b, engine.resource()); });
    engine.registerBinaryOp(".*", [&engine](const Value &a, const Value &b) { return nk::builtin::times(a, b, engine.resource()); });
    engine.registerBinaryOp("*",  [&engine](const Value &a, const Value &b) { return nk::builtin::mtimes(a, b, engine.resource()); });
    engine.registerBinaryOp("./", [&engine](const Value &a, const Value &b) { return nk::builtin::rdivide(a, b, engine.resource()); });
    engine.registerBinaryOp("/",  [&engine](const Value &a, const Value &b) { return nk::builtin::mrdivide(a, b, engine.resource()); });
    engine.registerBinaryOp("\\", [&engine](const Value &a, const Value &b) { return nk::builtin::mldivide(a, b, engine.resource()); });
    engine.registerBinaryOp("^",  [&engine](const Value &a, const Value &b) { return nk::builtin::power(a, b, engine.resource()); });
    engine.registerBinaryOp(".^", [&engine](const Value &a, const Value &b) { return nk::builtin::elementPower(a, b, engine.resource()); });

    engine.registerBinaryOp("==", [&engine](const Value &a, const Value &b) { return nk::builtin::eq(a, b, engine.resource()); });
    engine.registerBinaryOp("~=", [&engine](const Value &a, const Value &b) { return nk::builtin::ne(a, b, engine.resource()); });
    engine.registerBinaryOp("<",  [&engine](const Value &a, const Value &b) { return nk::builtin::lt(a, b, engine.resource()); });
    engine.registerBinaryOp(">",  [&engine](const Value &a, const Value &b) { return nk::builtin::gt(a, b, engine.resource()); });
    engine.registerBinaryOp("<=", [&engine](const Value &a, const Value &b) { return nk::builtin::le(a, b, engine.resource()); });
    engine.registerBinaryOp(">=", [&engine](const Value &a, const Value &b) { return nk::builtin::ge(a, b, engine.resource()); });

    engine.registerBinaryOp("&",  [&engine](const Value &a, const Value &b) { return nk::builtin::logical_and(a, b, engine.resource()); });
    engine.registerBinaryOp("|",  [&engine](const Value &a, const Value &b) { return nk::builtin::logical_or(a, b, engine.resource()); });
}

void BuiltinLibrary::registerUnaryOps(Engine &engine)
{
    engine.registerUnaryOp("-",  [&engine](const Value &a) { return nk::builtin::uminus(a, engine.resource()); });
    engine.registerUnaryOp("+",  [&engine](const Value &a) { return nk::builtin::uplus(a, engine.resource()); });
    engine.registerUnaryOp("~",  [&engine](const Value &a) { return nk::builtin::logical_not(a, engine.resource()); });
    engine.registerUnaryOp("'",  [&engine](const Value &a) { return nk::builtin::ctranspose(a, engine.resource()); });
    engine.registerUnaryOp(".'", [&engine](const Value &a) { return nk::builtin::transposeNC(a, engine.resource()); });
}

} // namespace nk

namespace nk::bundle::builtin {

void register_ops(Engine &engine) {
    BuiltinLibrary::registerBinaryOps(engine);
    BuiltinLibrary::registerUnaryOps(engine);

    engine.registerFunction("plus",       &::nk::builtin::detail::plus_reg);
    engine.registerFunction("minus",      &::nk::builtin::detail::minus_reg);
    engine.registerFunction("times",      &::nk::builtin::detail::times_reg);
    engine.registerFunction("mtimes",     &::nk::builtin::detail::mtimes_reg);
    engine.registerFunction("rdivide",    &::nk::builtin::detail::rdivide_reg);
    engine.registerFunction("mrdivide",   &::nk::builtin::detail::mrdivide_reg);
    engine.registerFunction("mldivide",   &::nk::builtin::detail::mldivide_reg);
    engine.registerFunction("ldivide",    &::nk::builtin::detail::ldivide_reg);
    engine.registerFunction("power",      &::nk::builtin::detail::power_reg);
    engine.registerFunction("mpower",     &::nk::builtin::detail::mpower_reg);
    engine.registerFunction("eq",         &::nk::builtin::detail::eq_reg);
    engine.registerFunction("ne",         &::nk::builtin::detail::ne_reg);
    engine.registerFunction("lt",         &::nk::builtin::detail::lt_reg);
    engine.registerFunction("le",         &::nk::builtin::detail::le_reg);
    engine.registerFunction("gt",         &::nk::builtin::detail::gt_reg);
    engine.registerFunction("ge",         &::nk::builtin::detail::ge_reg);
    engine.registerFunction("and",        &::nk::builtin::detail::and_reg);
    engine.registerFunction("or",         &::nk::builtin::detail::or_reg);
    engine.registerFunction("uminus",     &::nk::builtin::detail::uminus_reg);
    engine.registerFunction("uplus",      &::nk::builtin::detail::uplus_reg);
    engine.registerFunction("not",        &::nk::builtin::detail::not_reg);
    engine.registerFunction("ctranspose", &::nk::builtin::detail::ctranspose_reg);
    engine.registerFunction("pagemtimes", &::nk::builtin::detail::pagemtimes_reg);
    engine.registerFunction("diag",       &::nk::builtin::detail::diag_reg);
    engine.registerFunction("sort",       &::nk::builtin::detail::sort_reg);
    engine.registerFunction("sortrows",   &::nk::builtin::detail::sortrows_reg);
    engine.registerFunction("find",       &::nk::builtin::detail::find_reg);
    engine.registerFunction("nnz",        &::nk::builtin::detail::nnz_reg);
    engine.registerFunction("nonzeros",   &::nk::builtin::detail::nonzeros_reg);
    engine.registerFunction("horzcat",    &::nk::builtin::detail::horzcat_reg);
    engine.registerFunction("vertcat",    &::nk::builtin::detail::vertcat_reg);
    engine.registerFunction("meshgrid",   &::nk::builtin::detail::meshgrid_reg);
    engine.registerFunction("ndgrid",     &::nk::builtin::detail::ndgrid_reg);
    engine.registerFunction("cumsum",     &::nk::builtin::detail::cumsum_reg);
    engine.registerFunction("cumprod",    &::nk::builtin::detail::cumprod_reg);
    engine.registerFunction("cummax",     &::nk::builtin::detail::cummax_reg);
    engine.registerFunction("cummin",     &::nk::builtin::detail::cummin_reg);
    engine.registerFunction("diff",       &::nk::builtin::detail::diff_reg);
    engine.registerFunction("any",        &::nk::builtin::detail::any_reg);
    engine.registerFunction("all",        &::nk::builtin::detail::all_reg);
    engine.registerFunction("xor",        &::nk::builtin::detail::xor_reg);
}

} // namespace nk::bundle::builtin
