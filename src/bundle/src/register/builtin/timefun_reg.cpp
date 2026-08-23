// src/bundle/src/register/builtin/timefun_reg.cpp

#include <numkit/builtin/timefun.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

namespace numkit::bundle::builtin {

void register_timefun(Engine &engine) {
    engine.registerFunction("clock",
        [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = numkit::builtin::clock(ctx.engine->resource());
        });

    engine.registerFunction("date",
        [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = Value::fromString(numkit::builtin::date(), ctx.engine->resource());
        });

    engine.registerFunction("pause",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &) {
            double s = args.empty() ? 0.0 : args[0].toScalar();
            numkit::builtin::pause(s);
            outs[0] = Value();
        });

    engine.registerFunction("tic",
        [](Span<const Value>, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto now = Clock::now();
            ctx.engine->setTicTimer(now);
            if (nargout > 0) {
                double id = static_cast<double>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        now.time_since_epoch()).count());
                outs[0] = Value::scalar(id, ctx.engine->resource());
            } else {
                outs[0] = Value();
            }
        });

    engine.registerFunction("toc",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            auto now = Clock::now();
            TimePoint start;
            if (!args.empty() && args[0].isScalar()) {
                auto us = static_cast<long long>(args[0].toScalar());
                start = TimePoint(std::chrono::microseconds(us));
            } else if (ctx.engine->ticWasCalled()) {
                start = ctx.engine->ticTimer();
            } else {
                throw std::runtime_error("toc: You must call 'tic' before calling 'toc'.");
            }
            double elapsed = std::chrono::duration<double>(now - start).count();
            if (nargout > 0) {
                outs[0] = Value::scalar(elapsed, ctx.engine->resource());
            } else {
                std::ostringstream os;
                os << "Elapsed time is " << elapsed << " seconds.\n";
                ctx.engine->outputText(os.str());
                outs[0] = Value();
            }
        });

    engine.registerFunction("cputime",
        [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = Value::scalar(numkit::builtin::cputime(), ctx.engine->resource());
        });

    engine.registerFunction("now",
        [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = Value::scalar(numkit::builtin::now(), ctx.engine->resource());
        });

    engine.registerFunction("etime",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw std::runtime_error("etime: requires two date vectors (t2, t1)");
            outs[0] = numkit::builtin::etime(args[0], args[1], ctx.engine->resource());
        });

    engine.registerFunction("weeknum",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = numkit::builtin::weeknum(args, ctx.engine->resource());
        });

    engine.registerFunction("addtodate",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 3)
                throw std::runtime_error("addtodate: requires (D, quantity, units)");
            outs[0] = numkit::builtin::addtodate(args[0], args[1].toScalar(), args[2].toString(), ctx.engine->resource());
        });

    engine.registerFunction("datenum",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = numkit::builtin::datenum(args, ctx.engine->resource());
        });

    engine.registerFunction("weekday",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error("weekday requires at least one input");
            auto *mr = ctx.engine->resource();
            outs[0] = numkit::builtin::weekday(args[0], mr);
            if (nargout > 1) {
                bool wantLong = false;
                if (args.size() >= 2 && (args[1].isChar() || args[1].isString())) {
                    std::string fmt = args[1].toString();
                    for (auto &c : fmt)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (fmt == "long")
                        wantLong = true;
                    else if (fmt != "short")
                        throw std::runtime_error("weekday: format must be 'short' or 'long'");
                }
                static const char *kShort[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
                static const char *kLong[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
                double dval = args[0].numel() == 1 ? args[0].toScalar() : args[0].elemAsDouble(0);
                int64_t f = static_cast<int64_t>(std::floor(dval)) - 2;
                int64_t r = f % 7;
                if (r < 0) r += 7;
                int dayIdx = static_cast<int>(r) + 1;
                outs[1] = Value::fromString(wantLong ? kLong[dayIdx - 1] : kShort[dayIdx - 1], mr);
            }
        });

    engine.registerFunction("juliandate",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = numkit::builtin::juliandate(args, ctx.engine->resource());
        });

    engine.registerFunction("eomday",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw std::runtime_error("eomday: requires year and month");
            outs[0] = numkit::builtin::eomday(args[0], args[1], ctx.engine->resource());
        });

    engine.registerFunction("calendar",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = numkit::builtin::calendar(args, ctx.engine->resource());
        });

    engine.registerFunction("datestr",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = numkit::builtin::datestr(args, ctx.engine->resource());
        });

    engine.registerFunction("datevec",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error("datevec requires at least one argument");
            auto *mr = ctx.engine->resource();
            Value dv = numkit::builtin::datevec(args, mr);
            if (nargout <= 1) {
                outs[0] = std::move(dv);
            } else {
                const size_t N = dv.dims().rows();
                const double *o = dv.doubleData();
                for (size_t c = 0; c < std::min(nargout, size_t(6)); ++c) {
                    if (N == 1) {
                        outs[c] = Value::scalar(o[c * N], mr);
                    } else {
                        auto col = Value::matrix(N, 1, ValueType::DOUBLE, mr);
                        double *p = col.doubleDataMut();
                        for (size_t i = 0; i < N; ++i)
                            p[i] = o[i + c * N];
                        outs[c] = std::move(col);
                    }
                }
            }
        });

    engine.registerFunction("yyyymmdd",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = numkit::builtin::yyyymmdd(args, ctx.engine->resource());
        });

    engine.registerFunction("mjuliandate",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = numkit::builtin::mjuliandate(args, ctx.engine->resource());
        });
}

} // namespace numkit::bundle::builtin
