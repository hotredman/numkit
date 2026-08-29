#pragma once

#include <cstddef>
#include <string>

namespace numkit {

class StackGuard
{
public:
    static constexpr size_t SAFETY_MARGIN_BYTES = 64 * 1024; // 64 KB

    static void check(const char *stage = nullptr);

    explicit StackGuard(const char *stage = nullptr)
    {
        check(stage);
    }
};

} // namespace numkit
