#include "MLabStdLibrary.hpp"

#include <algorithm>
#include <limits>
#include <sstream>

namespace mlab {

void StdLibrary::registerStringFunctions(Engine &engine)
{
    engine.registerFunction("num2str",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                std::ostringstream os;
                                os << args[0].toScalar();
                                return {MValue::fromString(os.str(), alloc)};
                            });

    engine.registerFunction("str2num",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                try {
                                    return {MValue::scalar(std::stod(args[0].toString()), alloc)};
                                } catch (...) {
                                    return {MValue::empty()};
                                }
                            });

    engine.registerFunction("str2double",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                try {
                                    return {MValue::scalar(std::stod(args[0].toString()), alloc)};
                                } catch (...) {
                                    return {MValue::scalar(std::numeric_limits<double>::quiet_NaN(), alloc)};
                                }
                            });

    engine.registerFunction("strcmp",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {MValue::logicalScalar(args[0].toString() == args[1].toString(), alloc)};
                            });

    engine.registerFunction("strcmpi",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                std::string a = args[0].toString(), b = args[1].toString();
                                std::transform(a.begin(), a.end(), a.begin(), ::tolower);
                                std::transform(b.begin(), b.end(), b.begin(), ::tolower);
                                return {MValue::logicalScalar(a == b, alloc)};
                            });

    engine.registerFunction("upper",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                std::string s = args[0].toString();
                                std::transform(s.begin(), s.end(), s.begin(), ::toupper);
                                return {MValue::fromString(s, alloc)};
                            });

    engine.registerFunction("lower",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                std::string s = args[0].toString();
                                std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                                return {MValue::fromString(s, alloc)};
                            });

    engine.registerFunction("strtrim",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                std::string s = args[0].toString();
                                size_t start = s.find_first_not_of(" \t\r\n");
                                size_t end = s.find_last_not_of(" \t\r\n");
                                if (start == std::string::npos)
                                    return {MValue::fromString("", alloc)};
                                return {MValue::fromString(s.substr(start, end - start + 1), alloc)};
                            });

    engine.registerFunction("strsplit",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                std::string s = args[0].toString();
                                char delim = args.size() >= 2 ? args[1].toString()[0] : ' ';
                                std::vector<std::string> parts;
                                std::istringstream iss(s);
                                std::string token;
                                while (std::getline(iss, token, delim))
                                    if (!token.empty()) parts.push_back(token);
                                auto c = MValue::cell(1, parts.size());
                                for (size_t i = 0; i < parts.size(); ++i)
                                    c.cellAt(i) = MValue::fromString(parts[i], alloc);
                                return {c};
                            });

    engine.registerFunction("strcat",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                std::string result;
                                for (auto &a : args) result += a.toString();
                                return {MValue::fromString(result, alloc)};
                            });
}

} // namespace mlab
