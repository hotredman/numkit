#include "MStdHelpers.hpp"
#include "MStdLibrary.hpp"

#include <algorithm>
#include <limits>
#include <sstream>

namespace numkit::m {

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

    // string() — convert to MATLAB string type
    engine.registerFunction(
        "string",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty()) {
                outs[0] = MValue::stringScalar("", alloc);
                return;
            }
            const auto &a = args[0];
            if (a.isString()) {
                outs[0] = a;
                return;
            }
            if (a.isChar()) {
                outs[0] = MValue::stringScalar(a.toString(), alloc);
                return;
            }
            if (a.isNumeric()) {
                if (a.isScalar()) {
                    std::ostringstream os;
                    os << a.toScalar();
                    outs[0] = MValue::stringScalar(os.str(), alloc);
                } else {
                    // Array → string array
                    auto result = MValue::stringArray(a.dims().rows(), a.dims().cols());
                    for (size_t i = 0; i < a.numel(); ++i) {
                        std::ostringstream os;
                        os << a.doubleData()[i];
                        result.stringElemSet(i, os.str());
                    }
                    outs[0] = result;
                }
                return;
            }
            if (a.isLogical()) {
                outs[0] = MValue::stringScalar(a.toBool() ? "true" : "false", alloc);
                return;
            }
            throw std::runtime_error("Cannot convert input to string");
        });

    // char() — convert string to char array (also handles numeric → char)
    // Note: char() for numeric already exists as a type conversion;
    // here we add string→char support
    engine.registerFunction(
        "char",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty())
                throw std::runtime_error("char requires an argument");
            const auto &a = args[0];
            if (a.isChar()) {
                outs[0] = a;
                return;
            }
            if (a.isString()) {
                outs[0] = MValue::fromString(a.toString(), alloc);
                return;
            }
            if (a.isNumeric()) {
                // char([72 101 108]) → 'Hel'
                std::string s;
                if (a.isScalar()) {
                    s += static_cast<char>(static_cast<int>(a.toScalar()));
                } else {
                    const double *d = a.doubleData();
                    for (size_t i = 0; i < a.numel(); ++i)
                        s += static_cast<char>(static_cast<int>(d[i]));
                }
                outs[0] = MValue::fromString(s, alloc);
                return;
            }
            throw std::runtime_error("Cannot convert to char");
        });

    // strlength — length of each string in a string array
    engine.registerFunction(
        "strlength",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            const auto &a = args[0];
            if (a.isString()) {
                if (a.isScalar()) {
                    outs[0] = MValue::scalar(
                        static_cast<double>(a.toString().size()), alloc);
                } else {
                    auto result = createLike(a, MType::DOUBLE, alloc);
                    for (size_t i = 0; i < a.numel(); ++i)
                        result.doubleDataMut()[i] =
                            static_cast<double>(a.stringElem(i).size());
                    outs[0] = result;
                }
                return;
            }
            if (a.isChar()) {
                outs[0] = MValue::scalar(
                    static_cast<double>(a.numel()), alloc);
                return;
            }
            throw std::runtime_error("Input must be a string or char array");
        });

    // strrep — replace occurrences of pattern in string
    engine.registerFunction(
        "strrep",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.size() < 3)
                throw std::runtime_error("strrep requires 3 arguments");
            std::string s = args[0].toString();
            std::string old_s = args[1].toString();
            std::string new_s = args[2].toString();
            size_t pos = 0;
            while ((pos = s.find(old_s, pos)) != std::string::npos) {
                s.replace(pos, old_s.length(), new_s);
                pos += new_s.length();
            }
            if (args[0].isString())
                outs[0] = MValue::stringScalar(s, alloc);
            else
                outs[0] = MValue::fromString(s, alloc);
        });

    // contains — check if string contains pattern
    engine.registerFunction(
        "contains",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.size() < 2)
                throw std::runtime_error("contains requires 2 arguments");
            std::string s = args[0].toString();
            std::string pat = args[1].toString();
            outs[0] = MValue::logicalScalar(
                s.find(pat) != std::string::npos, alloc);
        });

    // startsWith
    engine.registerFunction(
        "startsWith",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            std::string s = args[0].toString();
            std::string pat = args[1].toString();
            outs[0] = MValue::logicalScalar(
                s.size() >= pat.size() && s.compare(0, pat.size(), pat) == 0,
                alloc);
        });

    // endsWith
    engine.registerFunction(
        "endsWith",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            std::string s = args[0].toString();
            std::string pat = args[1].toString();
            outs[0] = MValue::logicalScalar(
                s.size() >= pat.size()
                    && s.compare(s.size() - pat.size(), pat.size(), pat) == 0,
                alloc);
        });
}

} // namespace numkit::m