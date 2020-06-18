#include "Exception.h"

// Formatted exception is pretty tricky to implement.
// In order to avoid resource leak, stack overflow and oom on critical path,
// we have to reserve some memory in advance.
// This behavior requires a huge amount of memory (to embedded and resource constrained devices)
// to be set aside for the sole purpose of error logging, and the allocation need extra processing
// thus will be slow, but this is the only way we can implement it robustly and standard-compliant

#include <utility>
#include <string>

namespace {
    // 8 Mega binary bytes reserved for seh region in-case of OOM
    // This should be enough for log dumping and preserving important information in case of unrecoverable error
    constexpr int SehReserveSize = 8 * 1024 * 1024;

    // unit: uint32 flag+size; uint32 nextFree;
    std::byte SehReserve[SehReserveSize] = {
            static_cast<std::byte>(0x00),
            static_cast<std::byte>(0x80),
            static_cast<std::byte>(0x00),
            static_cast<std::byte>(0x00)
    };

    int firstFree = 0;

    void* SHAlloc(int size) {
        // TODO: Implement;
        return nullptr;
    }

    void SHFree(void* frag) {
        // TODO: Implement;
    }

    std::string ConstructAggr(const std::vector<std::nested_exception>& e) {
        static std::string_view message { "what(): aggregation of\n"};
        std::vector<std::pair<const char*, int>> whats;
        int size = 0;
        whats.reserve(e.size());
        for (auto& x : e) {
            try { x.rethrow_nested(); }
            catch (std::exception& e) {
                const auto len = strlen(e.what());
                whats.emplace_back(e.what(), len);
                size += len + 2; //NOLINT
            }
            catch(...) {}
        }
        std::string result;
        result.resize(message.size() + size);
        int p = message.size();
        std::memcpy(result.data(), message.data(), p);
        for (auto& [str, len] : whats) {
            result[p++] = '\t';
            std::memcpy(result.data() + p, str, len);
            p+=len;
            result[p++] = '\n';
        }
        result.back()='\0';
        return result;
    }
}

namespace Utils {
    AggregateException::AggregateException(std::vector<std::nested_exception>&& e) noexcept:
        std::runtime_error(ConstructAggr(e)), mExceptions(std::move(e)) {
    }

    void* SehMalloc(int size) {
        auto p = malloc(size);
        if (p) return p;
        return (size < 1024u * 1024u) ? SHAlloc(size) : nullptr;
    }

    void SehFree(void* frag) {
        const auto diff = reinterpret_cast<uintptr_t>(frag) - reinterpret_cast<uintptr_t>(SehReserve);
        const auto uf = static_cast<uintptr_t>(SehReserveSize);
        return (diff < uf) ? SHFree(frag) : free(frag);
    }
}