// src/bundle/src/register/builtin/datatypes_reg.cpp

#include <numkit/builtin/datatypes.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/callback_builtin.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/object.hpp>
#include <numkit/core/vm.hpp>
#include <numkit/core/build_info.hpp>
#include <numkit/runtime/runtime.hpp>
#include <numkit/runtime/language/cells/cell.hpp>
#include <numkit/runtime/language/structures/struct.hpp>
#include <numkit/runtime/help/help_catalog.hpp>
#include <numkit/lang/operators/binary_ops.hpp>
#include <numkit/lang/operators/unary_ops.hpp>
#include <numkit/lang/types/types.hpp>
#include <numkit/math/arithmetic/rounding.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace numkit {
void registerSplitapplyCallbackBuiltin(Engine &engine);
void registerIntegralM(Engine &engine);
void registerCellfunCallbackBuiltin(Engine &engine);
void registerStructfunCallbackBuiltin(Engine &engine);
}

namespace numkit::builtin::detail {
void allfinite_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void anymissing_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void anynan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cast_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cell_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cell2mat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cell2struct_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cellfun_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cellstr_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void double_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fieldnames_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void flintmax_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void getfield_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void int16_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void int32_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void int64_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void int8_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void intmax_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void intmin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void iscell_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void iscellstr_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ischar_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void iscolumn_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isempty_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isequal_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isequaln_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isfield_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isfinite_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isfloat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isinf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isinteger_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void islogical_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ismatrix_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ismissing_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isnan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isnumeric_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isreal_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isrow_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isscalar_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issingle_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issorted_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issortedrows_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issparse_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isstring_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isstruct_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isuniform_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isvector_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void logical_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mat2cell_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void num2cell_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void orderfields_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void realmax_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void realmin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rmfield_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void setfield_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void single_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void standardizeMissing_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void struct_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void struct2cell_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void structfun_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void swapbytes_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void typecast_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uint16_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uint32_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uint64_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uint8_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
static void warnNotSupported(CallContext &ctx, const std::string &feature)
{
    ctx.engine->outputText("Warning: '" + feature + "' is not yet supported.\n");
}
struct ArrayfunCallbackBuiltin : CallbackBuiltin
{
    std::shared_ptr<VmContinuation> tryStart(Span<const Value> args, std::size_t nargout,
                                             Value *dest, Engine &eng) override
    {
        if (args.size() < 2 || nargout > 1)
            return nullptr;
        if (!eng.isUserCodeHandle(args[0]))
            return nullptr; // builtin handle → fast synchronous path
        // Collect input arrays + parse trailing 'UniformOutput'/'ErrorHandler'
        // exactly as the synchronous arrayfun (ErrorHandler is skipped, not
        // modelled). Any malformed form → nullptr so the sync path reports it.
        bool uniform = true;
        auto inputs = std::make_shared<std::vector<Value>>();
        for (std::size_t k = 1; k < args.size(); ++k) {
            if (args[k].isChar() && k + 1 < args.size()) {
                std::string key = args[k].toString();
                for (auto &ch : key)
                    ch = static_cast<char>(std::tolower((unsigned char)ch));
                if (key == "uniformoutput") {
                    uniform = args[k + 1].toScalar() != 0.0;
                    ++k;
                    continue;
                }
                if (key == "errorhandler") {
                    ++k;
                    continue;
                }
            }
            inputs->push_back(args[k]);
        }
        if (inputs->empty())
            return nullptr;
        const std::size_t n = (*inputs)[0].numel();
        for (const auto &p : *inputs)
            if (p.numel() != n)
                return nullptr; // size mismatch → sync path throws the error
        auto *mr = eng.resource();
        const std::size_t rows = (*inputs)[0].dims().rows();
        const std::size_t cols = (*inputs)[0].dims().cols();
        auto cont = std::make_shared<LoopContinuation>();
        cont->handle = args[0];
        cont->n = n;
        cont->dest = dest;
        cont->makeArgs = [inputs, mr](std::size_t i) -> std::vector<Value> {
            std::vector<Value> callArgs(inputs->size());
            for (std::size_t k = 0; k < inputs->size(); ++k)
                callArgs[k] = Value::scalar((*inputs)[k].elemAsDouble(i), mr);
            return callArgs;
        };
        cont->pack = [uniform, rows, cols, mr](std::vector<Value> &results) -> Value {
            const std::size_t n = results.size();
            if (uniform) {
                Value out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
                for (std::size_t k = 0; k < n; ++k)
                    out.doubleDataMut()[k] = results[k].toScalar();
                return out;
            }
            Value out = Value::cell(rows, cols, mr);
            for (std::size_t k = 0; k < n; ++k)
                out.cellAt(k) = results[k];
            return out;
        };
        cont->results.reserve(n);
        return cont;
    }
};
Value metaConcat(const std::vector<Value> &v, std::pmr::memory_resource *mr)
{
    if (v.empty())
        return Value::Empty;
    return Value::objectConcatN(v.data(), v.size(), Dims{v.size(), 1}, mr);
}
Value buildMetaProperty(const PropMeta &p, std::pmr::memory_resource *mr)
{
    auto st = std::make_shared<ObjectState>(mr);
    st->props.emplace("Name", Value::fromString(p.name, mr));
    st->props.emplace("GetAccess", Value::fromString(p.getAccess, mr));
    st->props.emplace("SetAccess", Value::fromString(p.setAccess, mr));
    st->props.emplace("Constant", Value::logicalScalar(p.isConstant, mr));
    st->props.emplace("Dependent", Value::logicalScalar(p.isDependent, mr));
    return Value::object("meta.property", std::move(st), /*isHandle=*/false, mr);
}
Value buildMetaMethod(const MethodMeta &m, std::pmr::memory_resource *mr)
{
    auto st = std::make_shared<ObjectState>(mr);
    st->props.emplace("Name", Value::fromString(m.name, mr));
    st->props.emplace("Static", Value::logicalScalar(m.isStatic, mr));
    st->props.emplace("Access", Value::fromString(m.access, mr));
    st->props.emplace("Abstract", Value::logicalScalar(m.isAbstract, mr));
    return Value::object("meta.method", std::move(st), /*isHandle=*/false, mr);
}
Value buildMetaClass(const std::string &cn, Engine &eng, std::pmr::memory_resource *mr)
{
    const BuiltinClass *cls = eng.findClass(cn);
    auto st = std::make_shared<ObjectState>(mr);
    st->props.emplace("Name", Value::fromString(cn, mr));
    std::vector<std::string> sup = cls ? cls->superclasses : std::vector<std::string>{};
    if (cls && cls->isHandle && std::find(sup.begin(), sup.end(), "handle") == sup.end())
        sup.push_back("handle");
    std::vector<Value> supObjs;
    for (const auto &s : sup)
        supObjs.push_back(buildMetaClass(s, eng, mr)); // recurse over ancestors (acyclic)
    st->props.emplace("SuperclassList", metaConcat(supObjs, mr));
    std::vector<Value> propObjs;
    if (cls)
        for (const auto &p : cls->propMeta)
            propObjs.push_back(buildMetaProperty(p, mr));
    st->props.emplace("PropertyList", metaConcat(propObjs, mr));
    std::vector<Value> methObjs;
    if (cls)
        for (const auto &m : cls->methodMeta)
            methObjs.push_back(buildMetaMethod(m, mr));
    st->props.emplace("MethodList", metaConcat(methObjs, mr));
    st->props.emplace("Sealed", Value::logicalScalar(cls && cls->isSealed, mr));
    st->props.emplace("Abstract", Value::logicalScalar(cls && cls->isAbstract, mr));
    return Value::object("meta.class", std::move(st), /*isHandle=*/false, mr);
}

} // namespace numkit::builtin::detail

namespace numkit::bundle::builtin {

void register_datatypes(Engine &engine) {
    using namespace ::numkit::builtin::detail;
// ── Pack 34: function-handle introspection ────────────────────────
    //
    // functions(@h) returns a small struct describing the handle.
    // numkit handles only carry a name, so the {function, type, file}
    // fields are the natural set; advanced MATLAB metadata
    // (workspace, parentage) is not available.
    engine.registerFunction("functions",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.empty() || !args[0].isFuncHandle())
                throw std::runtime_error("functions: argument must be a function handle");
            auto *mr = ctx.engine->resource();
            auto s = Value::structure(mr);
            const std::string name = args[0].funcHandleName();
            s.field("function") = Value::fromString(name, mr);
            s.field("type")     = Value::fromString("simple", mr);
            s.field("file")     = Value::fromString("", mr);
            outs[0] = std::move(s);
        });

    // localfunctions() — MATLAB returns a cell of handles to local
    // functions defined in the current m-file. Without per-file
    // function-table introspection we return the empty cell, which
    // matches MATLAB when called outside a function file.
    engine.registerFunction("localfunctions",
        [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = Value::cell(0, 1, ctx.engine->resource());
        });

    
// ── Phase 6c: types.cpp public-API-backed built-ins ────────────
    engine.registerFunction("double",    &::numkit::builtin::detail::double_reg);
    engine.registerFunction("single",    &::numkit::builtin::detail::single_reg);
    engine.registerFunction("int8",      &::numkit::builtin::detail::int8_reg);
    engine.registerFunction("int16",     &::numkit::builtin::detail::int16_reg);
    engine.registerFunction("int32",     &::numkit::builtin::detail::int32_reg);
    engine.registerFunction("int64",     &::numkit::builtin::detail::int64_reg);
    engine.registerFunction("uint8",     &::numkit::builtin::detail::uint8_reg);
    engine.registerFunction("uint16",    &::numkit::builtin::detail::uint16_reg);
    engine.registerFunction("uint32",    &::numkit::builtin::detail::uint32_reg);
    engine.registerFunction("uint64",    &::numkit::builtin::detail::uint64_reg);
    engine.registerFunction("logical",   &::numkit::builtin::detail::logical_reg);
    engine.registerFunction("cast",      &::numkit::builtin::detail::cast_reg);
    engine.registerFunction("swapbytes", &::numkit::builtin::detail::swapbytes_reg);
    engine.registerFunction("typecast",  &::numkit::builtin::detail::typecast_reg);
    engine.registerFunction("isnumeric", &::numkit::builtin::detail::isnumeric_reg);
    engine.registerFunction("islogical", &::numkit::builtin::detail::islogical_reg);
    engine.registerFunction("ischar",    &::numkit::builtin::detail::ischar_reg);
    engine.registerFunction("isstring",  &::numkit::builtin::detail::isstring_reg);
    engine.registerFunction("iscell",    &::numkit::builtin::detail::iscell_reg);
    engine.registerFunction("isstruct",  &::numkit::builtin::detail::isstruct_reg);
    engine.registerFunction("isempty",   &::numkit::builtin::detail::isempty_reg);
    engine.registerFunction("isscalar",  &::numkit::builtin::detail::isscalar_reg);
    engine.registerFunction("isreal",    &::numkit::builtin::detail::isreal_reg);
    engine.registerFunction("isinteger", &::numkit::builtin::detail::isinteger_reg);
    engine.registerFunction("isfloat",   &::numkit::builtin::detail::isfloat_reg);
    engine.registerFunction("issingle",  &::numkit::builtin::detail::issingle_reg);
    engine.registerFunction("issparse",  &::numkit::builtin::detail::issparse_reg);
    engine.registerFunction("isnan",     &::numkit::builtin::detail::isnan_reg);
    engine.registerFunction("isinf",     &::numkit::builtin::detail::isinf_reg);
    engine.registerFunction("isfinite",  &::numkit::builtin::detail::isfinite_reg);
    engine.registerFunction("ismissing", &::numkit::builtin::detail::ismissing_reg);
    engine.registerFunction("anymissing",&::numkit::builtin::detail::anymissing_reg);
    engine.registerFunction("standardizeMissing", &::numkit::builtin::detail::standardizeMissing_reg);
    engine.registerFunction("isvector",   &::numkit::builtin::detail::isvector_reg);
    engine.registerFunction("isrow",      &::numkit::builtin::detail::isrow_reg);
    engine.registerFunction("iscolumn",   &::numkit::builtin::detail::iscolumn_reg);
    engine.registerFunction("ismatrix",   &::numkit::builtin::detail::ismatrix_reg);
    engine.registerFunction("issorted",   &::numkit::builtin::detail::issorted_reg);
    engine.registerFunction("issortedrows",&::numkit::builtin::detail::issortedrows_reg);
    engine.registerFunction("isuniform",  &::numkit::builtin::detail::isuniform_reg);
    // issymmetric / ishermitian / isbanded / isdiag / istril / istriu /
    // bandwidth registered by LinalgLibrary::install (toolboxes/linalg).
    // vecnorm registered by LinalgLibrary::install (toolboxes/linalg).
    // compat aliases — same fns reachable via `import compat.*`.
    // compat.{issymmetric,ishermitian,isbanded,isdiag,istril,istriu,bandwidth}
    // registered by LinalgLibrary::install (toolboxes/linalg).
    // compat.vecnorm registered by LinalgLibrary::install (toolboxes/linalg).
    // rref / planerot + compat aliases registered by
    // LinalgLibrary::install (toolboxes/linalg).
    // ldl / compat.ldl registered by LinalgLibrary::install (toolboxes/linalg).
    // lsqminnorm / lsqnonneg + compat aliases registered by
    // LinalgLibrary::install (toolboxes/linalg).
    // balance registered by LinalgLibrary::install (toolboxes/linalg).
    // compat.balance registered by LinalgLibrary::install (toolboxes/linalg).
    engine.registerFunction("flintmax",   &::numkit::builtin::detail::flintmax_reg);
    engine.registerFunction("intmax",     &::numkit::builtin::detail::intmax_reg);
    engine.registerFunction("intmin",     &::numkit::builtin::detail::intmin_reg);
    engine.registerFunction("realmax",    &::numkit::builtin::detail::realmax_reg);
    engine.registerFunction("realmin",    &::numkit::builtin::detail::realmin_reg);
    engine.registerFunction("allfinite",  &::numkit::builtin::detail::allfinite_reg);
    engine.registerFunction("anynan",     &::numkit::builtin::detail::anynan_reg);
    engine.registerFunction("isequal",   &::numkit::builtin::detail::isequal_reg);
    engine.registerFunction("isequaln",  &::numkit::builtin::detail::isequaln_reg);
    // `class` registered in registerWorkspaceBuiltins() as a lambda
    // (formats type via mtypeName, more elaborate than the bare reg).

    
// ── Phase 6c: datatypes/{cell,struct}/ public-API-backed built-ins ───────
    engine.registerFunction("struct",     &::numkit::builtin::detail::struct_reg);
    engine.registerFunction("fieldnames", &::numkit::builtin::detail::fieldnames_reg);
    engine.registerFunction("isfield",    &::numkit::builtin::detail::isfield_reg);
    engine.registerFunction("rmfield",    &::numkit::builtin::detail::rmfield_reg);
    engine.registerFunction("cell",       &::numkit::builtin::detail::cell_reg);
    engine.registerFunction("cellfun",    &::numkit::builtin::detail::cellfun_reg);
    ::numkit::builtin::registerCellfunCallbackBuiltin(engine); // VM-pausable callbacks
    engine.registerFunction("num2cell",   &::numkit::builtin::detail::num2cell_reg);
    engine.registerFunction("cell2mat",   &::numkit::builtin::detail::cell2mat_reg);
    engine.registerFunction("iscellstr",  &::numkit::builtin::detail::iscellstr_reg);
    engine.registerFunction("cellstr",    &::numkit::builtin::detail::cellstr_reg);
    engine.registerFunction("mat2cell",   &::numkit::builtin::detail::mat2cell_reg);

    
// ── Pack 24: deal + celldisp (lambdas) ────────────────────────────
    // deal — distribute inputs to outputs. Single input → broadcast to
    // all outputs; multiple inputs → 1-to-1 with outs.
    engine.registerFunction("deal",
        [](Span<const Value> args, size_t nargout,
           Span<Value> outs, CallContext &) {
            if (args.empty())
                throw std::runtime_error("deal requires at least 1 argument");
            if (args.size() == 1) {
                for (size_t i = 0; i < nargout && i < outs.size(); ++i)
                    outs[i] = args[0];
                return;
            }
            const size_t n = std::min(nargout, args.size());
            for (size_t i = 0; i < n && i < outs.size(); ++i)
                outs[i] = args[i];
        });

    // celldisp(c[, name]) — print each cell's contents.
    engine.registerFunction("celldisp",
        [](Span<const Value> args, size_t, Span<Value>, CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error("celldisp requires 1 argument");
            const Value &c = args[0];
            if (!c.isCell())
                throw std::runtime_error("celldisp: input must be a cell");
            const std::string name = (args.size() >= 2)
                                          ? args[1].toString()
                                          : std::string("ans");
            for (size_t i = 0; i < c.numel(); ++i) {
                ctx.engine->outputText(name + "{" + std::to_string(i + 1) + "} =\n");
                ctx.engine->outputText(c.cellAt(i).formatDisplay("") + "\n");
            }
        });
    engine.registerFunction("structfun",  &::numkit::builtin::detail::structfun_reg);
    ::numkit::builtin::registerStructfunCallbackBuiltin(engine); // VM-pausable callbacks
    engine.registerFunction("getfield",   &::numkit::builtin::detail::getfield_reg);
    engine.registerFunction("setfield",   &::numkit::builtin::detail::setfield_reg);
    engine.registerFunction("orderfields",&::numkit::builtin::detail::orderfields_reg);
    engine.registerFunction("struct2cell",&::numkit::builtin::detail::struct2cell_reg);
    engine.registerFunction("cell2struct",&::numkit::builtin::detail::cell2struct_reg);

    // arrayfun(@fn, A [, B, ...] [, 'UniformOutput', true|false])
    //
    // For each element of A (and any additional input arrays B, C,
    // ...), invokes fn with the per-position scalar values and
    // collects the results. UniformOutput=true (the default) packs
    // scalar results into a numeric array of the same shape as A;
    // false collects them in a cell array.
    //
    // Earlier this function was a stub that returned A verbatim,
    // ignoring fn — see BUGS.md #11. The real lambda body is now
    // applied via Engine::callFunctionHandle, the same path
    // cellfun/structfun already use.
    engine.registerFunction("arrayfun",
                            [](Span<const Value> args,
                               size_t nargout,
                               Span<Value> outs,
                               CallContext &ctx) {
                                (void)nargout;
                                if (args.size() < 2)
                                    throw std::runtime_error(
                                        "arrayfun requires at least 2 arguments");
                                if (!args[0].isFuncHandle())
                                    throw std::runtime_error(
                                        "arrayfun: first argument must be a function handle");
                                const Value &handle = args[0];

                                // Collect input arrays + parse trailing
                                // 'UniformOutput' / 'ErrorHandler' N-V pairs.
                                bool uniformOutput = true;
                                std::vector<const Value *> inputs;
                                inputs.reserve(args.size() - 1);
                                for (size_t i = 1; i < args.size(); ++i) {
                                    if (args[i].isChar() && i + 1 < args.size()) {
                                        std::string key = args[i].toString();
                                        for (auto &c : key)
                                            c = (char)std::tolower((unsigned char)c);
                                        if (key == "uniformoutput") {
                                            uniformOutput = args[i + 1].toScalar() != 0.0;
                                            ++i;   // skip the value
                                            continue;
                                        }
                                        if (key == "errorhandler") {
                                            // not modelled; just skip
                                            ++i;
                                            continue;
                                        }
                                    }
                                    inputs.push_back(&args[i]);
                                }
                                if (inputs.empty())
                                    throw std::runtime_error(
                                        "arrayfun: at least one input array required");

                                const size_t n = inputs[0]->numel();
                                for (const auto *p : inputs) {
                                    if (p->numel() != n)
                                        throw std::runtime_error(
                                            "arrayfun: all input arrays must be the same size");
                                }
                                auto *mr = ctx.engine->resource();

                                // Walk every element. Per call, build a
                                // scalar-Value arg list and invoke the handle.
                                std::vector<Value> callArgs(inputs.size());
                                if (uniformOutput) {
                                    auto out = Value::matrix(inputs[0]->dims().rows(),
                                                             inputs[0]->dims().cols(),
                                                             ValueType::DOUBLE, mr);
                                    for (size_t i = 0; i < n; ++i) {
                                        for (size_t k = 0; k < inputs.size(); ++k)
                                            callArgs[k] = Value::scalar(
                                                inputs[k]->elemAsDouble(i), mr);
                                        Value r = ctx.engine->callFunctionHandle(
                                            handle,
                                            Span<const Value>(callArgs.data(), callArgs.size()),
                                            ctx.env);
                                        out.doubleDataMut()[i] = r.toScalar();
                                    }
                                    outs[0] = std::move(out);
                                } else {
                                    // UniformOutput=false → result is a CELL
                                    // of size matching A.
                                    auto cell = Value::cell(inputs[0]->dims().rows(),
                                                            inputs[0]->dims().cols(), mr);
                                    for (size_t i = 0; i < n; ++i) {
                                        for (size_t k = 0; k < inputs.size(); ++k)
                                            callArgs[k] = Value::scalar(
                                                inputs[k]->elemAsDouble(i), mr);
                                        Value r = ctx.engine->callFunctionHandle(
                                            handle,
                                            Span<const Value>(callArgs.data(), callArgs.size()),
                                            ctx.env);
                                        cell.cellAt(i) = std::move(r);
                                    }
                                    outs[0] = std::move(cell);
                                }
                            });
    // arrayfun callbacks as pausable VM frames (state-machine callbacks); a
    // user-code handle runs each callback on the VM, builtin handles / other
    // forms fall back to the synchronous arrayfun above.
    engine.registerCallbackBuiltin(
        "arrayfun", std::make_shared<::numkit::builtin::detail::ArrayfunCallbackBuiltin>());

    
// ── class ──────────────────────────────────────────────────
    engine.registerFunction("class",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("class requires an argument");
                                // OBJECT instances report their registered class name.
                                outs[0] = Value::fromString(
                                    args[0].isObject() ? args[0].objectClassName()
                                                       : mtypeName(args[0].type()),
                                    ctx.engine->resource());
                            });

    // ── isa(x, classOrCategory) ────────────────────────────────
    engine.registerFunction(
        "isa", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw std::runtime_error("isa requires two arguments");
            const std::string target = args[1].toString();
            const Value &x = args[0];
            bool result = false;
            if (x.isObject()) {
                const std::string cn = x.objectClassName();
                result = (cn == target);
                const BuiltinClass *cls = ctx.engine->findClass(cn);
                if (!result && cls)
                    for (const auto &s : cls->superclasses)
                        if (s == target) {
                            result = true;
                            break;
                        }
                if (!result && target == "handle" && cls && cls->isHandle)
                    result = true;
            } else {
                const ValueType t = x.type();
                const std::string tn = mtypeName(t);
                const bool isFloat = (t == ValueType::DOUBLE || t == ValueType::SINGLE
                                      || t == ValueType::COMPLEX);
                const bool isInt = (t == ValueType::INT8 || t == ValueType::INT16
                                    || t == ValueType::INT32 || t == ValueType::INT64
                                    || t == ValueType::UINT8 || t == ValueType::UINT16
                                    || t == ValueType::UINT32 || t == ValueType::UINT64);
                if (tn == target)
                    result = true;
                else if (target == "numeric")
                    result = isFloat || isInt;
                else if (target == "float")
                    result = isFloat;
                else if (target == "integer")
                    result = isInt;
            }
            outs[0] = Value::logicalScalar(result, ctx.engine->resource());
        });

    // ── isobject / properties / methods (class introspection) ──
    engine.registerFunction(
        "isobject", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = Value::logicalScalar(!args.empty() && args[0].isObject(),
                                           ctx.engine->resource());
        });
    engine.registerFunction(
        "properties", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error("properties requires an argument");
            std::string cn = args[0].isObject() ? args[0].objectClassName()
                                                : args[0].toString();
            const BuiltinClass *cls = ctx.engine->findClass(cn);
            const size_t n = cls ? cls->propNames.size() : 0;
            Value c = Value::cell(n, 1, ctx.engine->resource());
            for (size_t i = 0; i < n; ++i)
                c.cellAt(i) = Value::fromString(cls->propNames[i], ctx.engine->resource());
            outs[0] = c;
        });
    engine.registerFunction(
        "methods", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error("methods requires an argument");
            std::string cn = args[0].isObject() ? args[0].objectClassName()
                                                : args[0].toString();
            const BuiltinClass *cls = ctx.engine->findClass(cn);
            std::vector<std::string> names;
            if (cls)
                for (const auto &[mn, fn] : cls->methods)
                    if (std::find(cls->hidden.begin(), cls->hidden.end(), mn)
                        == cls->hidden.end()) // omit Hidden methods
                        names.push_back(mn);
            std::sort(names.begin(), names.end());
            Value c = Value::cell(names.size(), 1, ctx.engine->resource());
            for (size_t i = 0; i < names.size(); ++i)
                c.cellAt(i) = Value::fromString(names[i], ctx.engine->resource());
            outs[0] = c;
        });

    // ── isprop / ismethod / superclasses (class introspection) ──
    // isprop(obj, name): true if obj is an object whose class declares
    // property `name`. Non-objects (and unknown classes) → false (MATLAB does
    // not error). `name` may be a cellstr → element-wise logical array.
    engine.registerFunction(
        "isprop", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw Error("isprop: requires (obj, name)", 0, 0, "isprop", "",
                            "numkit:isprop:nargin");
            const BuiltinClass *cls =
                args[0].isObject() ? ctx.engine->findClass(args[0].objectClassName()) : nullptr;
            auto has = [&](const std::string &n) {
                return cls && std::find(cls->propNames.begin(), cls->propNames.end(), n)
                                  != cls->propNames.end();
            };
            if (args[1].isCell()) {
                const size_t n = args[1].numel();
                Value r = Value::matrix(args[1].dims().rows(), args[1].dims().cols(),
                                        ValueType::LOGICAL, ctx.engine->resource());
                for (size_t i = 0; i < n; ++i)
                    r.logicalDataMut()[i] = has(args[1].cellAt(i).toString()) ? 1 : 0;
                outs[0] = r;
            } else {
                outs[0] = Value::logicalScalar(has(args[1].toString()), ctx.engine->resource());
            }
        });
    // ismethod(obj, name): true if obj's class defines method `name`.
    engine.registerFunction(
        "ismethod", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw Error("ismethod: requires (obj, name)", 0, 0, "ismethod", "",
                            "numkit:ismethod:nargin");
            std::string cn = args[0].isObject() ? args[0].objectClassName()
                                                : args[0].toString();
            const BuiltinClass *cls = ctx.engine->findClass(cn);
            auto has = [&](const std::string &n) {
                return cls && cls->methods.count(n) > 0;
            };
            if (args[1].isCell()) {
                const size_t n = args[1].numel();
                Value r = Value::matrix(args[1].dims().rows(), args[1].dims().cols(),
                                        ValueType::LOGICAL, ctx.engine->resource());
                for (size_t i = 0; i < n; ++i)
                    r.logicalDataMut()[i] = has(args[1].cellAt(i).toString()) ? 1 : 0;
                outs[0] = r;
            } else {
                outs[0] = Value::logicalScalar(has(args[1].toString()), ctx.engine->resource());
            }
        });
    // superclasses(obj | 'ClassName'): cell column of ancestor class names.
    engine.registerFunction(
        "superclasses", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.empty())
                throw Error("superclasses: requires (obj or 'ClassName')", 0, 0,
                            "superclasses", "", "numkit:superclasses:nargin");
            std::string cn = args[0].isObject() ? args[0].objectClassName()
                                                : args[0].toString();
            const BuiltinClass *cls = ctx.engine->findClass(cn);
            std::vector<std::string> names;
            if (cls) {
                names = cls->superclasses; // transitive ancestor list
                // A handle subclass lists `handle` as an ancestor in MATLAB,
                // but it's the semantics marker, not a registered classdef.
                if (cls->isHandle
                    && std::find(names.begin(), names.end(), "handle") == names.end())
                    names.push_back("handle");
            }
            Value c = Value::cell(names.size(), 1, ctx.engine->resource());
            for (size_t i = 0; i < names.size(); ++i)
                c.cellAt(i) = Value::fromString(names[i], ctx.engine->resource());
            outs[0] = c;
        });

    // ── meta.class / meta.property / meta.method + metaclass(x) ──
    // Real reflection objects: metaclass(x).PropertyList(i).Name etc. work as in
    // MATLAB. All three share a propGet that serves the fields stashed in the
    // instance's ObjectState by the buildMeta* helpers above.
    {
        auto metaPropGet = [](const Value &self, const std::string &name, Value &out,
                              CallContext &) -> bool {
            const ObjectState *st = self.objectStateConst();
            auto it = st->props.find(name);
            if (it == st->props.end())
                return false;
            out = it->second;
            return true;
        };
        BuiltinClass mp;
        mp.name = "meta.property";
        mp.propNames = {"Name", "GetAccess", "SetAccess", "Constant", "Dependent"};
        mp.propGet = metaPropGet;
        engine.registerClass(std::move(mp));

        BuiltinClass mm;
        mm.name = "meta.method";
        mm.propNames = {"Name", "Static", "Access", "Abstract"};
        mm.propGet = metaPropGet;
        engine.registerClass(std::move(mm));

        BuiltinClass mc;
        mc.name = "meta.class";
        mc.propNames = {"Name", "SuperclassList", "PropertyList", "MethodList",
                        "Sealed", "Abstract"};
        mc.propGet = metaPropGet;
        engine.registerClass(std::move(mc));
    }
    engine.registerFunction(
        "metaclass", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.empty())
                throw Error("metaclass: requires 1 argument", 0, 0, "metaclass", "",
                            "numkit:metaclass:nargin");
            auto *mr = ctx.engine->resource();
            std::string cn;
            if (args[0].isObject()) {
                cn = args[0].objectClassName();
            } else if (args[0].type() == ValueType::CHAR
                       && ctx.engine->findClass(args[0].toString())) {
                // A char row naming a registered class → meta of that class
                // (the programmatic equivalent of MATLAB's `?ClassName`).
                cn = args[0].toString();
            } else {
                cn = mtypeName(args[0].type());
            }
            outs[0] = buildMetaClass(cn, *ctx.engine, mr);
        });

    
// ── Pack 36: type-predicate stubs for absent types ─────────────
    // These predicates always return logical false because numkit has
    // no categorical / table / timetable / datetime / duration types
    // yet. Returning false (instead of erroring) is correct MATLAB
    // behaviour — `iscategorical(double_array)` etc. all return false.
    // Lets MATLAB-source code that defensively checks the type port
    // without errors.
    //
    // Note: `isordinal` / `isprotected` do throw in MATLAB when the
    // input is non-categorical, so we deliberately do NOT stub those
    // here — keeping their absence preserves the type-error signal.
    auto alwaysFalsePredicate =
        [](Span<const Value>, size_t /*nargout*/, Span<Value> outs,
           CallContext &ctx) {
            outs[0] = Value::logicalScalar(false, ctx.engine->resource());
        };
    engine.registerFunction("iscategorical",     alwaysFalsePredicate);
    engine.registerFunction("istable",           alwaysFalsePredicate);
    engine.registerFunction("istimetable",       alwaysFalsePredicate);
    engine.registerFunction("istabular",         alwaysFalsePredicate);
    engine.registerFunction("isdatetime",        alwaysFalsePredicate);
    engine.registerFunction("isduration",        alwaysFalsePredicate);
    engine.registerFunction("iscalendarduration",alwaysFalsePredicate);
}
} // namespace numkit::bundle::builtin
