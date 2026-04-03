#include "MLabStdLibrary.hpp"

#include <algorithm>
#include <limits>
#include <sstream>

namespace mlab {

void StdLibrary::registerStringFunctions(Engine &engine)
{
    engine.registerFunction("num2str",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                std::ostringstream os;
                                os << args[0].toScalar();
                                {
                                    outs[0] = MValue::fromString(os.str(), alloc);
                                    return;
                                }
                            });

    engine.registerFunction("str2num",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                try {
                                    {
                                        outs[0] = MValue::scalar(std::stod(args[0].toString()),
                                                                 alloc);
                                        return;
                                    }
                                } catch (...) {
                                    {
                                        outs[0] = MValue::empty();
                                        return;
                                    }
                                }
                            });

    engine.registerFunction(
        "str2double",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            try {
                {
                    outs[0] = MValue::scalar(std::stod(args[0].toString()), alloc);
                    return;
                }
            } catch (...) {
                {
                    outs[0] = MValue::scalar(std::numeric_limits<double>::quiet_NaN(), alloc);
                    return;
                }
            }
        });

    engine.registerFunction(
        "strcmp", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            {
                outs[0] = MValue::logicalScalar(args[0].toString() == args[1].toString(), alloc);
                return;
            }
        });

    engine.registerFunction("strcmpi",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                std::string a = args[0].toString(), b = args[1].toString();
                                std::transform(a.begin(), a.end(), a.begin(), ::tolower);
                                std::transform(b.begin(), b.end(), b.begin(), ::tolower);
                                {
                                    outs[0] = MValue::logicalScalar(a == b, alloc);
                                    return;
                                }
                            });

    engine.registerFunction("upper",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                std::string s = args[0].toString();
                                std::transform(s.begin(), s.end(), s.begin(), ::toupper);
                                {
                                    outs[0] = MValue::fromString(s, alloc);
                                    return;
                                }
                            });

    engine.registerFunction("lower",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                std::string s = args[0].toString();
                                std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                                {
                                    outs[0] = MValue::fromString(s, alloc);
                                    return;
                                }
                            });

    engine.registerFunction("strtrim",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                std::string s = args[0].toString();
                                size_t start = s.find_first_not_of(" \t\r\n");
                                size_t end = s.find_last_not_of(" \t\r\n");
                                if (start == std::string::npos) {
                                    outs[0] = MValue::fromString("", alloc);
                                    return;
                                }
                                {
                                    outs[0] = MValue::fromString(s.substr(start, end - start + 1),
                                                                 alloc);
                                    return;
                                }
                            });

    engine.registerFunction("strsplit",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                std::string s = args[0].toString();
                                char delim = args.size() >= 2 ? args[1].toString()[0] : ' ';
                                std::vector<std::string> parts;
                                std::istringstream iss(s);
                                std::string token;
                                while (std::getline(iss, token, delim))
                                    if (!token.empty())
                                        parts.push_back(token);
                                auto c = MValue::cell(1, parts.size());
                                for (size_t i = 0; i < parts.size(); ++i)
                                    c.cellAt(i) = MValue::fromString(parts[i], alloc);
                                {
                                    outs[0] = c;
                                    return;
                                }
                            });

    engine.registerFunction("strcat",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                std::string result;
                                for (auto &a : args)
                                    result += a.toString();
                                {
                                    outs[0] = MValue::fromString(result, alloc);
                                    return;
                                }
                            });
}

} // namespace mlab