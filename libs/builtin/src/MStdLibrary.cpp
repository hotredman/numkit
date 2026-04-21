#include <numkit/m/builtin/MStdLibrary.hpp>

#include <numkit/m/core/MTypes.hpp>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace numkit::m::builtin::detail {
// Forward declarations for Phase 6c public-API-backed adapters.
// Each is defined in the corresponding M<Name>.cpp translation unit.

// MStdMath.cpp
void sqrt_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void abs_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void sin_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void cos_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void tan_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void asin_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void acos_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void atan_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void atan2_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void exp_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void log_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void log2_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void log10_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void floor_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void ceil_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void round_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void fix_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void mod_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void rem_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void sign_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void max_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void min_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void sum_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void prod_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void mean_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void linspace_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void logspace_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void rand_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void randn_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void deg2rad_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void rad2deg_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdComplex.cpp
void real_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void imag_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void conj_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void complex_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void angle_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdStrings.cpp
void num2str_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void str2num_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void str2double_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void string_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void char_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void strcmp_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void strcmpi_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void upper_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void lower_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void strtrim_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void strsplit_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void strcat_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void strlength_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void strrep_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void contains_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void startsWith_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void endsWith_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdMatrix.cpp
void zeros_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void ones_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void eye_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void size_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void length_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void numel_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void ndims_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void reshape_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void transpose_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void diag_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void sort_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void find_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void horzcat_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void vertcat_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void meshgrid_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void cumsum_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void cross_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void dot_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
} // namespace numkit::m::builtin::detail

namespace numkit::m {

// ── Warning helper for unsupported features ──────────────────
static void warnNotSupported(CallContext &ctx, const std::string &feature)
{
    ctx.engine->outputText("Warning: '" + feature + "' is not yet supported.\n");
}

void StdLibrary::install(Engine &engine)
{
    registerBinaryOps(engine);
    registerUnaryOps(engine);
    registerIOFunctions(engine);
    registerTypeFunctions(engine);
    registerCellStructFunctions(engine);
    registerStringFunctions(engine);
    registerComplexFunctions(engine);

    registerWorkspaceBuiltins(engine);

    // ── Phase 6c: MStdMath public-API-backed built-ins ─────────────
    engine.registerFunction("sqrt",     &builtin::detail::sqrt_reg);
    engine.registerFunction("abs",      &builtin::detail::abs_reg);
    engine.registerFunction("sin",      &builtin::detail::sin_reg);
    engine.registerFunction("cos",      &builtin::detail::cos_reg);
    engine.registerFunction("tan",      &builtin::detail::tan_reg);
    engine.registerFunction("asin",     &builtin::detail::asin_reg);
    engine.registerFunction("acos",     &builtin::detail::acos_reg);
    engine.registerFunction("atan",     &builtin::detail::atan_reg);
    engine.registerFunction("atan2",    &builtin::detail::atan2_reg);
    engine.registerFunction("exp",      &builtin::detail::exp_reg);
    engine.registerFunction("log",      &builtin::detail::log_reg);
    engine.registerFunction("log2",     &builtin::detail::log2_reg);
    engine.registerFunction("log10",    &builtin::detail::log10_reg);
    engine.registerFunction("floor",    &builtin::detail::floor_reg);
    engine.registerFunction("ceil",     &builtin::detail::ceil_reg);
    engine.registerFunction("round",    &builtin::detail::round_reg);
    engine.registerFunction("fix",      &builtin::detail::fix_reg);
    engine.registerFunction("mod",      &builtin::detail::mod_reg);
    engine.registerFunction("rem",      &builtin::detail::rem_reg);
    engine.registerFunction("sign",     &builtin::detail::sign_reg);
    engine.registerFunction("max",      &builtin::detail::max_reg);
    engine.registerFunction("min",      &builtin::detail::min_reg);
    engine.registerFunction("sum",      &builtin::detail::sum_reg);
    engine.registerFunction("prod",     &builtin::detail::prod_reg);
    engine.registerFunction("mean",     &builtin::detail::mean_reg);
    engine.registerFunction("linspace", &builtin::detail::linspace_reg);
    engine.registerFunction("logspace", &builtin::detail::logspace_reg);
    engine.registerFunction("rand",     &builtin::detail::rand_reg);
    engine.registerFunction("randn",    &builtin::detail::randn_reg);
    engine.registerFunction("deg2rad",  &builtin::detail::deg2rad_reg);
    engine.registerFunction("rad2deg",  &builtin::detail::rad2deg_reg);

    // ── Phase 6c: MStdMatrix public-API-backed built-ins ───────────
    engine.registerFunction("zeros",     &builtin::detail::zeros_reg);
    engine.registerFunction("ones",      &builtin::detail::ones_reg);
    engine.registerFunction("eye",       &builtin::detail::eye_reg);
    engine.registerFunction("size",      &builtin::detail::size_reg);
    engine.registerFunction("length",    &builtin::detail::length_reg);
    engine.registerFunction("numel",     &builtin::detail::numel_reg);
    engine.registerFunction("ndims",     &builtin::detail::ndims_reg);
    engine.registerFunction("reshape",   &builtin::detail::reshape_reg);
    engine.registerFunction("transpose", &builtin::detail::transpose_reg);
    engine.registerFunction("diag",      &builtin::detail::diag_reg);
    engine.registerFunction("sort",      &builtin::detail::sort_reg);
    engine.registerFunction("find",      &builtin::detail::find_reg);
    engine.registerFunction("horzcat",   &builtin::detail::horzcat_reg);
    engine.registerFunction("vertcat",   &builtin::detail::vertcat_reg);
    engine.registerFunction("meshgrid",  &builtin::detail::meshgrid_reg);
    engine.registerFunction("cumsum",    &builtin::detail::cumsum_reg);
    engine.registerFunction("cross",     &builtin::detail::cross_reg);
    engine.registerFunction("dot",       &builtin::detail::dot_reg);

    // ── Phase 6c: MStdComplex public-API-backed built-ins ──────────
    engine.registerFunction("real",    &builtin::detail::real_reg);
    engine.registerFunction("imag",    &builtin::detail::imag_reg);
    engine.registerFunction("conj",    &builtin::detail::conj_reg);
    engine.registerFunction("complex", &builtin::detail::complex_reg);
    engine.registerFunction("angle",   &builtin::detail::angle_reg);

    // ── Phase 6c: MStdStrings public-API-backed built-ins ──────────
    engine.registerFunction("num2str",    &builtin::detail::num2str_reg);
    engine.registerFunction("str2num",    &builtin::detail::str2num_reg);
    engine.registerFunction("str2double", &builtin::detail::str2double_reg);
    engine.registerFunction("string",     &builtin::detail::string_reg);
    engine.registerFunction("char",       &builtin::detail::char_reg);
    engine.registerFunction("strcmp",     &builtin::detail::strcmp_reg);
    engine.registerFunction("strcmpi",    &builtin::detail::strcmpi_reg);
    engine.registerFunction("upper",      &builtin::detail::upper_reg);
    engine.registerFunction("lower",      &builtin::detail::lower_reg);
    engine.registerFunction("strtrim",    &builtin::detail::strtrim_reg);
    engine.registerFunction("strsplit",   &builtin::detail::strsplit_reg);
    engine.registerFunction("strcat",     &builtin::detail::strcat_reg);
    engine.registerFunction("strlength",  &builtin::detail::strlength_reg);
    engine.registerFunction("strrep",     &builtin::detail::strrep_reg);
    engine.registerFunction("contains",   &builtin::detail::contains_reg);
    engine.registerFunction("startsWith", &builtin::detail::startsWith_reg);
    engine.registerFunction("endsWith",   &builtin::detail::endsWith_reg);

    // --- arrayfun (basic scalar version) ---
    engine.registerFunction("arrayfun",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                if (args.size() < 2)
                                    throw std::runtime_error(
                                        "arrayfun requires at least 2 arguments");
                                {
                                    outs[0] = args[1];
                                    return;
                                }
                            });
}

// ============================================================
// Workspace / session builtins
//
// These were previously handled only by TreeWalker::tryBuiltinCall(),
// which meant the VM could never execute them. By registering them
// as externalFuncs, both backends can dispatch them uniformly
// through the standard CALL opcode.
// ============================================================

void StdLibrary::registerWorkspaceBuiltins(Engine &engine)
{
    // ── clear ──────────────────────────────────────────────────
    engine.registerFunction("clear",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                auto *env = ctx.env;
                                bool insideFunc = ctx.engine->isInsideFunctionCall();

                                if (args.empty()) {
                                    // MATLAB: bare 'clear' clears workspace variables only,
                                    // NOT user functions or figures.
                                    env->clearAll();
                                    if (!insideFunc) {
                                        ctx.engine->reinstallConstants();
                                        ctx.engine->markClearAll();
                                    }
                                } else {
                                    std::string first = args[0].isChar() ? args[0].toString() : "";

                                    // Unsupported flags
                                    if (first == "-regexp") {
                                        warnNotSupported(ctx, "clear -regexp");
                                        outs[0] = MValue::empty();
                                        return;
                                    }
                                    if (first == "global") {
                                        auto *gs = ctx.env->globalsEnv();
                                        if (args.size() == 1) {
                                            // clear global — clear all globals
                                            if (gs)
                                                gs->clearAll();
                                            env->clearAll();
                                            ctx.engine->markClearAll();
                                        } else {
                                            // clear global x y — clear specific globals
                                            for (size_t i = 1; i < args.size(); ++i) {
                                                if (args[i].isChar()) {
                                                    std::string gname = args[i].toString();
                                                    if (gs)
                                                        gs->remove(gname);
                                                    env->remove(gname);
                                                }
                                            }
                                        }
                                        outs[0] = MValue::empty();
                                        return;
                                    }
                                    if (first == "import") {
                                        warnNotSupported(ctx, "clear import");
                                        outs[0] = MValue::empty();
                                        return;
                                    }

                                    if (first == "all" || first == "classes") {
                                        if (insideFunc) {
                                            env->clearAll();
                                        } else {
                                            env->clearAll();
                                            ctx.engine->clearUserFunctions();
                                            ctx.engine->figureManager().closeAll();
                                            ctx.engine->reinstallConstants();
                                            ctx.engine->markClearAll();
                                        }
                                    } else if (first == "functions") {
                                        if (!insideFunc)
                                            ctx.engine->clearUserFunctions();
                                    } else {
                                        // `clear x`, `clear pi`, etc.
                                        // Un-shadow a built-in by removing the
                                        // workspace slot — the next read then
                                        // falls back to constantsEnv_. No
                                        // special filtering: MATLAB allows it.
                                        for (auto &a : args) {
                                            if (a.isChar())
                                                env->remove(a.toString());
                                        }
                                    }
                                }
                                outs[0] = MValue::empty();
                            });

    // ── clc ────────────────────────────────────────────────────
    engine.registerFunction("clc",
                            [](Span<const MValue>, size_t, Span<MValue> outs, CallContext &ctx) {
                                ctx.engine->outputText("__CLEAR__\n");
                                outs[0] = MValue::empty();
                            });

    // ── who ────────────────────────────────────────────────────
    engine.registerFunction("who",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                auto *env = ctx.env;

                                // Check for unsupported flags
                                if (!args.empty() && args[0].isChar()) {
                                    std::string first = args[0].toString();
                                    if (first == "-file") {
                                        warnNotSupported(ctx, "who -file");
                                        outs[0] = MValue::empty();
                                        return;
                                    }
                                }

                                std::vector<std::string> names;
                                if (args.empty()) {
                                    // localNames() excludes parent-env constants
                                    // (pi, eps, …) — they show up here only if
                                    // shadowed in the workspace, as in MATLAB.
                                    names = env->localNames();
                                } else {
                                    for (auto &a : args) {
                                        if (a.isChar()) {
                                            std::string varName = a.toString();
                                            if (env->getLocal(varName))
                                                names.push_back(varName);
                                        }
                                    }
                                }
                                std::sort(names.begin(), names.end());

                                std::ostringstream os;
                                if (!names.empty()) {
                                    os << "\nYour variables are:\n\n";
                                    for (auto &n : names)
                                        os << n << "  ";
                                    os << "\n\n";
                                }
                                ctx.engine->outputText(os.str());
                                outs[0] = MValue::empty();
                            });

    // ── whos ───────────────────────────────────────────────────
    engine.registerFunction("whos",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                auto *env = ctx.env;

                                // Check for unsupported flags
                                if (!args.empty() && args[0].isChar()) {
                                    std::string first = args[0].toString();
                                    if (first == "-file") {
                                        warnNotSupported(ctx, "whos -file");
                                        outs[0] = MValue::empty();
                                        return;
                                    }
                                }

                                std::vector<std::string> names;
                                if (args.empty()) {
                                    // localNames() excludes parent-env constants
                                    // (pi, eps, …) — they show up here only if
                                    // shadowed in the workspace, as in MATLAB.
                                    names = env->localNames();
                                } else {
                                    for (auto &a : args) {
                                        if (a.isChar()) {
                                            std::string varName = a.toString();
                                            if (env->getLocal(varName))
                                                names.push_back(varName);
                                        }
                                    }
                                }
                                std::sort(names.begin(), names.end());

                                std::ostringstream os;
                                if (!names.empty()) {
                                    os << "  Name" << std::string(6, ' ') << "Size"
                                       << std::string(13, ' ') << "Bytes  Class"
                                       << std::string(5, ' ') << "Attributes\n\n";
                                    for (auto &n : names) {
                                        auto *val = env->get(n);
                                        if (!val)
                                            continue;
                                        auto &d = val->dims();
                                        std::string sizeStr = std::to_string(d.rows()) + "x"
                                                              + std::to_string(d.cols());
                                        if (d.is3D())
                                            sizeStr += "x" + std::to_string(d.pages());
                                        std::string bytesStr = std::to_string(val->rawBytes());
                                        std::string classStr = mtypeName(val->type());
                                        std::string attrStr;
                                        if (env->isGlobal(n))
                                            attrStr = "global";

                                        os << "  " << n;
                                        for (size_t i = n.size(); i < 10; ++i)
                                            os << " ";
                                        os << sizeStr;
                                        for (size_t i = sizeStr.size(); i < 17; ++i)
                                            os << " ";
                                        for (size_t i = bytesStr.size(); i < 5; ++i)
                                            os << " ";
                                        os << bytesStr << "  " << classStr;
                                        for (size_t i = classStr.size(); i < 10; ++i)
                                            os << " ";
                                        os << attrStr << "\n";
                                    }
                                    os << "\n";
                                }
                                ctx.engine->outputText(os.str());
                                outs[0] = MValue::empty();
                            });

    // ── which ──────────────────────────────────────────────────
    engine.registerFunction("which",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("which requires a name argument");
                                std::string qname = args[0].isChar() ? args[0].toString() : "";
                                auto *env = ctx.env;

                                std::ostringstream os;
                                if (env->getLocal(qname)
                                    || (env->isGlobal(qname) && env->globalsEnv()
                                        && env->globalsEnv()->get(qname)))
                                    os << qname << " is a variable.\n";
                                else if (ctx.engine->hasUserFunction(qname))
                                    os << qname << " is a user-defined function.\n";
                                else if (ctx.engine->hasExternalFunction(qname))
                                    os << "built-in (" << qname << ")\n";
                                else
                                    os << "'" << qname << "' not found.\n";

                                ctx.engine->outputText(os.str());
                                outs[0] = MValue::empty();
                            });

    // ── exist ──────────────────────────────────────────────────
    engine.registerFunction("exist",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("exist requires a name argument");
                                std::string varName = args[0].toString();
                                auto *env = ctx.env;

                                // Optional second argument: type filter
                                std::string typeFilter;
                                if (args.size() >= 2 && args[1].isChar())
                                    typeFilter = args[1].toString();

                                // Unsupported type filters
                                if (typeFilter == "file" || typeFilter == "dir") {
                                    warnNotSupported(ctx, "exist(name, '" + typeFilter + "')");
                                    outs[0] = MValue::scalar(0.0, &ctx.engine->allocator());
                                    return;
                                }
                                if (typeFilter == "class") {
                                    warnNotSupported(ctx, "exist(name, 'class')");
                                    outs[0] = MValue::scalar(0.0, &ctx.engine->allocator());
                                    return;
                                }

                                double code = 0;
                                // Check local scope only for variables (don't leak to parent)
                                bool isVar = (env->getLocal(varName) != nullptr);
                                // Also check global declarations in current env
                                if (!isVar && env->isGlobal(varName)) {
                                    auto *gs = env->globalsEnv();
                                    isVar = (gs && gs->get(varName) != nullptr);
                                }
                                bool isFunc = ctx.engine->hasFunction(varName);

                                if (typeFilter.empty()) {
                                    // No filter: return first match
                                    if (isVar)
                                        code = 1;
                                    else if (isFunc)
                                        code = 5;
                                } else if (typeFilter == "var") {
                                    if (isVar)
                                        code = 1;
                                } else if (typeFilter == "builtin") {
                                    if (ctx.engine->hasExternalFunction(varName))
                                        code = 5;
                                }

                                outs[0] = MValue::scalar(code, &ctx.engine->allocator());
                            });

    // ── class ──────────────────────────────────────────────────
    engine.registerFunction("class",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("class requires an argument");
                                outs[0] = MValue::fromString(mtypeName(args[0].type()),
                                                             &ctx.engine->allocator());
                            });

    // ── tic ────────────────────────────────────────────────────
    engine.registerFunction("tic",
                            [](Span<const MValue>,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto now = Clock::now();
                                ctx.engine->setTicTimer(now);
                                if (nargout > 0) {
                                    double id = static_cast<double>(
                                        std::chrono::duration_cast<std::chrono::microseconds>(
                                            now.time_since_epoch())
                                            .count());
                                    outs[0] = MValue::scalar(id, &ctx.engine->allocator());
                                } else {
                                    outs[0] = MValue::empty();
                                }
                            });

    // ── toc ────────────────────────────────────────────────────
    engine.registerFunction("toc",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto now = Clock::now();
                                TimePoint start;
                                if (!args.empty() && args[0].isScalar()) {
                                    auto us = static_cast<long long>(args[0].toScalar());
                                    start = TimePoint(std::chrono::microseconds(us));
                                } else if (ctx.engine->ticWasCalled()) {
                                    start = ctx.engine->ticTimer();
                                } else {
                                    throw std::runtime_error(
                                        "toc: You must call 'tic' before calling 'toc'.");
                                }
                                double elapsed = std::chrono::duration<double>(now - start).count();
                                if (nargout > 0) {
                                    outs[0] = MValue::scalar(elapsed, &ctx.engine->allocator());
                                } else {
                                    std::ostringstream os;
                                    os << "Elapsed time is " << elapsed << " seconds.\n";
                                    ctx.engine->outputText(os.str());
                                    outs[0] = MValue::empty();
                                }
                            });
}

} // namespace numkit::m