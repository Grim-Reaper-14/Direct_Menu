#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
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
        std::size_t totalDeallocations{};
        std::size_t failedRangeChecks{};
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
    [[nodiscard]] bool OwnsRange(const void* address, std::size_t bytes) const noexcept;
    [[nodiscard]] std::size_t SizeOf(const void* address) const noexcept;

    bool ReadBytes(
        const void* address,
        void* destination,
        std::size_t bytes) const noexcept;

    bool WriteBytes(
        void* address,
        const void* source,
        std::size_t bytes) noexcept;

    template <typename T>
    [[nodiscard]] std::optional<T> Read(const void* address) const noexcept {
        static_assert(std::is_trivially_copyable_v<T>);
        T value{};
        if (!ReadBytes(address, &value, sizeof(T))) {
            return std::nullopt;
        }
        return value;
    }

    template <typename T>
    bool Write(void* address, const T& value) noexcept {
        static_assert(std::is_trivially_copyable_v<T>);
        return WriteBytes(address, &value, sizeof(T));
    }

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

    [[nodiscard]] const AllocationRecord* FindContainingLocked(
        const void* address,
        std::size_t bytes) const noexcept;

    void LogInfo(std::string_view message) const;
    void LogWarning(std::string_view message) const;

    mutable std::mutex mutex_;
    std::unordered_map<void*, AllocationRecord> allocations_;
    mutable Statistics stats_{};
    AllocationId nextId_{1};
    Logger* logger_{nullptr};
};

} // namespace smf::core
