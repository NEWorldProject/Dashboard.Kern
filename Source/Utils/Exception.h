#pragma once

#include <vector>
#include <exception>

namespace Utils {
    class AggregateException: public std::runtime_error {
    public:
        explicit AggregateException(std::vector<std::nested_exception>&& e) noexcept;

        [[nodiscard]] auto& nested() const noexcept { return mExceptions; }
    private:
        std::vector<std::nested_exception> mExceptions;
    };

    [[maybe_unused]] void* SehMalloc(int size);
    [[maybe_unused]] void SehFree(void* frag);
}