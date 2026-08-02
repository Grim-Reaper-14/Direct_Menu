#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace smf::core {

class Logger;

class MemoryManagerAPI final {
public:
    using AllocationId = std::uint64_t;

    struct AllocationInfo {
        AllocationId id{};
        void* address{};
        std::size_t size{};
        std::size_t alignment{};
        std::string tag;
    };

    struct Statistics {
        std::size_t liveAllocations{};
        std::size_t liveBytes{};
        std::size_t peakAllocations{};
        std::size_t peakBytes{};
        std::size_t totalAllocations{};
        std::size_t totalBytesAllocated{};
    };

    explicit MemoryManagerAPI(Logger* logger = nullptr);
    ~MemoryManagerAPI();

    MemoryManagerAPI(const MemoryManagerAPI&) = delete;
    MemoryManagerAPI& operator=(const MemoryManagerAPI&) = delete;

    [[nodiscard]] void* Allocate(
        std::size_t size,
        std::size_t alignment = alignof(std::max_align_t),
        std::string tag = {});

    bool Deallocate(void* address) noexcept;
    void ReleaseAll() noexcept;

    [[nodiscard]] std::vector<std::byte> CreateBuffer(std::size_t size) const;

    template <typename T, typename... Args>
    [[nodiscard]] std::unique_ptr<T> CreateObject(Args&&... args) const {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    [[nodiscard]] bool Owns(const void* address) const noexcept;
    [[nodiscard]] std::size_t SizeOf(const void* address) const noexcept;
    [[nodiscard]] std::vector<AllocationInfo> Snapshot() const;
    [[nodiscard]] Statistics Stats() const noexcept;

private:
    struct AllocationRecord {
        AllocationId id{};
        void* address{};
        std::size_t size{};
        std::size_t alignment{};
        std::string tag;
    };

    void LogInfo(std::string_view message) const;
    void LogWarning(std::string_view message) const;

    mutable std::mutex mutex_;
    std::unordered_map<void*, AllocationRecord> allocations_;
    Statistics stats_{};
    AllocationId nextId_{1};
    Logger* logger_{nullptr};
};

} // namespace smf::core
