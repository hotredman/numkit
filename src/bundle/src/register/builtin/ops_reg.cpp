// src/bundle/src/register/builtin/ops_reg.cpp

#include <numkit/builtin/ops.hpp>
#include <numkit/bundle/builtin_library.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin::detail {
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
} // namespace numkit::builtin::detail

namespace numkit {

void BuiltinLibrary::registerBinaryOps(Engine &engine)
{
    engine.registerBinaryOp("+",  [&engine](const Value &a, const Value &b) { return numkit::builtin::plus(a, b, engine.resource()); });
    engine.registerBinaryOp("-",  [&engine](const Value &a, const Value &b) { return numkit::builtin::minus(a, b, engine.resource()); });
    engine.registerBinaryOp(".*", [&engine](const Value &a, const Value &b) { return numkit::builtin::times(a, b, engine.resource()); });
    engine.registerBinaryOp("*",  [&engine](const Value &a, const Value &b) { return numkit::builtin::mtimes(a, b, engine.resource()); });
    engine.registerBinaryOp("./", [&engine](const Value &a, const Value &b) { return numkit::builtin::rdivide(a, b, engine.resource()); });
    engine.registerBinaryOp("/",  [&engine](const Value &a, const Value &b) { return numkit::builtin::mrdivide(a, b, engine.resource()); });
    engine.registerBinaryOp("\\", [&engine](const Value &a, const Value &b) { return numkit::builtin::mldivide(a, b, engine.resource()); });
    engine.registerBinaryOp("^",  [&engine](const Value &a, const Value &b) { return numkit::builtin::power(a, b, engine.resource()); });
    engine.registerBinaryOp(".^", [&engine](const Value &a, const Value &b) { return numkit::builtin::elementPower(a, b, engine.resource()); });

    engine.registerBinaryOp("==", [&engine](const Value &a, const Value &b) { return numkit::builtin::eq(a, b, engine.resource()); });
    engine.registerBinaryOp("~=", [&engine](const Value &a, const Value &b) { return numkit::builtin::ne(a, b, engine.resource()); });
    engine.registerBinaryOp("<",  [&engine](const Value &a, const Value &b) { return numkit::builtin::lt(a, b, engine.resource()); });
    engine.registerBinaryOp(">",  [&engine](const Value &a, const Value &b) { return numkit::builtin::gt(a, b, engine.resource()); });
    engine.registerBinaryOp("<=", [&engine](const Value &a, const Value &b) { return numkit::builtin::le(a, b, engine.resource()); });
    engine.registerBinaryOp(">=", [&engine](const Value &a, const Value &b) { return numkit::builtin::ge(a, b, engine.resource()); });

    engine.registerBinaryOp("&",  [&engine](const Value &a, const Value &b) { return numkit::builtin::logical_and(a, b, engine.resource()); });
    engine.registerBinaryOp("|",  [&engine](const Value &a, const Value &b) { return numkit::builtin::logical_or(a, b, engine.resource()); });
}

void BuiltinLibrary::registerUnaryOps(Engine &engine)
{
    engine.registerUnaryOp("-",  [&engine](const Value &a) { return numkit::builtin::uminus(a, engine.resource()); });
    engine.registerUnaryOp("+",  [&engine](const Value &a) { return numkit::builtin::uplus(a, engine.resource()); });
    engine.registerUnaryOp("~",  [&engine](const Value &a) { return numkit::builtin::logical_not(a, engine.resource()); });
    engine.registerUnaryOp("'",  [&engine](const Value &a) { return numkit::builtin::ctranspose(a, engine.resource()); });
    engine.registerUnaryOp(".'", [&engine](const Value &a) { return numkit::builtin::transposeNC(a, engine.resource()); });
}

} // namespace numkit

namespace numkit::bundle::builtin {

void register_ops(Engine &engine) {
    BuiltinLibrary::registerBinaryOps(engine);
    BuiltinLibrary::registerUnaryOps(engine);

    engine.registerFunction("plus",       &::numkit::builtin::detail::plus_reg);
    engine.registerFunction("minus",      &::numkit::builtin::detail::minus_reg);
    engine.registerFunction("times",      &::numkit::builtin::detail::times_reg);
    engine.registerFunction("mtimes",     &::numkit::builtin::detail::mtimes_reg);
    engine.registerFunction("rdivide",    &::numkit::builtin::detail::rdivide_reg);
    engine.registerFunction("mrdivide",   &::numkit::builtin::detail::mrdivide_reg);
    engine.registerFunction("mldivide",   &::numkit::builtin::detail::mldivide_reg);
    engine.registerFunction("ldivide",    &::numkit::builtin::detail::ldivide_reg);
    engine.registerFunction("power",      &::numkit::builtin::detail::power_reg);
    engine.registerFunction("mpower",     &::numkit::builtin::detail::mpower_reg);
    engine.registerFunction("eq",         &::numkit::builtin::detail::eq_reg);
    engine.registerFunction("ne",         &::numkit::builtin::detail::ne_reg);
    engine.registerFunction("lt",         &::numkit::builtin::detail::lt_reg);
    engine.registerFunction("le",         &::numkit::builtin::detail::le_reg);
    engine.registerFunction("gt",         &::numkit::builtin::detail::gt_reg);
    engine.registerFunction("ge",         &::numkit::builtin::detail::ge_reg);
    engine.registerFunction("and",        &::numkit::builtin::detail::and_reg);
    engine.registerFunction("or",         &::numkit::builtin::detail::or_reg);
    engine.registerFunction("uminus",     &::numkit::builtin::detail::uminus_reg);
    engine.registerFunction("uplus",      &::numkit::builtin::detail::uplus_reg);
    engine.registerFunction("not",        &::numkit::builtin::detail::not_reg);
    engine.registerFunction("ctranspose", &::numkit::builtin::detail::ctranspose_reg);
    engine.registerFunction("pagemtimes", &::numkit::builtin::detail::pagemtimes_reg);
    engine.registerFunction("diag",       &::numkit::builtin::detail::diag_reg);
    engine.registerFunction("sort",       &::numkit::builtin::detail::sort_reg);
    engine.registerFunction("sortrows",   &::numkit::builtin::detail::sortrows_reg);
    engine.registerFunction("find",       &::numkit::builtin::detail::find_reg);
    engine.registerFunction("nnz",        &::numkit::builtin::detail::nnz_reg);
    engine.registerFunction("nonzeros",   &::numkit::builtin::detail::nonzeros_reg);
    engine.registerFunction("horzcat",    &::numkit::builtin::detail::horzcat_reg);
    engine.registerFunction("vertcat",    &::numkit::builtin::detail::vertcat_reg);
    engine.registerFunction("meshgrid",   &::numkit::builtin::detail::meshgrid_reg);
    engine.registerFunction("ndgrid",     &::numkit::builtin::detail::ndgrid_reg);
    engine.registerFunction("cumsum",     &::numkit::builtin::detail::cumsum_reg);
    engine.registerFunction("cumprod",    &::numkit::builtin::detail::cumprod_reg);
    engine.registerFunction("cummax",     &::numkit::builtin::detail::cummax_reg);
    engine.registerFunction("cummin",     &::numkit::builtin::detail::cummin_reg);
    engine.registerFunction("diff",       &::numkit::builtin::detail::diff_reg);
    engine.registerFunction("any",        &::numkit::builtin::detail::any_reg);
    engine.registerFunction("all",        &::numkit::builtin::detail::all_reg);
    engine.registerFunction("xor",        &::numkit::builtin::detail::xor_reg);
}

} // namespace numkit::bundle::builtin
