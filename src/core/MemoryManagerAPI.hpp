#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
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

    static constexpr DWORD QueryProcessAccess =
        PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE;
    static constexpr DWORD ReadProcessAccess =
        QueryProcessAccess | PROCESS_VM_READ;
    static constexpr DWORD ReadWriteProcessAccess =
        ReadProcessAccess | PROCESS_VM_WRITE | PROCESS_VM_OPERATION;

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

    struct ModuleInfo {
        std::uintptr_t baseAddress{};
        std::size_t imageSize{};
        std::wstring name;
        std::wstring path;

        [[nodiscard]] bool IsValid() const noexcept {
            return baseAddress != 0 && imageSize != 0;
        }

        [[nodiscard]] std::uintptr_t EndAddress() const noexcept {
            if (imageSize > std::numeric_limits<std::uintptr_t>::max() - baseAddress) {
                return 0;
            }
            return baseAddress + imageSize;
        }
    };

    struct SectionInfo {
        std::string name;
        std::uintptr_t address{};
        std::size_t size{};
        DWORD characteristics{};

        [[nodiscard]] bool IsReadable() const noexcept {
            return (characteristics & IMAGE_SCN_MEM_READ) != 0;
        }
        [[nodiscard]] bool IsWritable() const noexcept {
            return (characteristics & IMAGE_SCN_MEM_WRITE) != 0;
        }
        [[nodiscard]] bool IsExecutable() const noexcept {
            return (characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        }
    };

    struct PatternByte {
        std::uint8_t value{};
        bool wildcard{};
    };

    struct PatternScanOptions {
        std::string sectionName;
        std::ptrdiff_t resultOffset{};
        std::size_t occurrence{};
        std::size_t chunkSize{1024 * 1024};
        bool validateMemoryPages{true};
        bool skipUnreadablePages{true};
        bool executablePagesOnly{false};
    };

    explicit MemoryManagerAPI(Logger* logger = nullptr);
    ~MemoryManagerAPI();

    MemoryManagerAPI(const MemoryManagerAPI&) = delete;
    MemoryManagerAPI& operator=(const MemoryManagerAPI&) = delete;

    bool AttachToWindow(HWND gameWindow, DWORD desiredAccess = QueryProcessAccess);
    bool AttachToWindowTitle(std::wstring_view windowTitle, DWORD desiredAccess = QueryProcessAccess);
    bool AttachToProcessId(DWORD processId, DWORD desiredAccess = QueryProcessAccess);
    bool AttachToProcessName(std::wstring_view executableName, DWORD desiredAccess = QueryProcessAccess);
    bool RefreshGameWindow() noexcept;
    void DetachProcess() noexcept;

    [[nodiscard]] bool IsProcessAttached() const noexcept;
    [[nodiscard]] bool HasGameWindow() const noexcept;
    [[nodiscard]] DWORD ProcessId() const noexcept;
    [[nodiscard]] std::uintptr_t ModuleBaseAddress(std::wstring_view moduleName = {}) const noexcept;

    [[nodiscard]] HANDLE ProcessHandle() const noexcept;
    [[nodiscard]] HWND GameWindow() const noexcept;

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

    [[nodiscard]] const void* ResolveOffset(
        const void* baseAddress,
        std::ptrdiff_t offset,
        std::size_t bytes = 1) const noexcept;

    [[nodiscard]] void* ResolveOffset(
        void* baseAddress,
        std::ptrdiff_t offset,
        std::size_t bytes = 1) noexcept {
        return const_cast<void*>(
            std::as_const(*this).ResolveOffset(baseAddress, offset, bytes));
    }

    bool ReadBytes(const void* address, void* destination, std::size_t bytes) const noexcept;
    bool WriteBytes(void* address, const void* source, std::size_t bytes) noexcept;

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

    template <typename T>
    [[nodiscard]] std::optional<T> ReadOffset(
        const void* baseAddress,
        const std::ptrdiff_t offset) const noexcept {
        static_assert(std::is_trivially_copyable_v<T>);
        const void* address = ResolveOffset(baseAddress, offset, sizeof(T));
        return address == nullptr ? std::nullopt : Read<T>(address);
    }

    template <typename T>
    bool WriteOffset(
        void* baseAddress,
        const std::ptrdiff_t offset,
        const T& value) noexcept {
        static_assert(std::is_trivially_copyable_v<T>);
        void* address = ResolveOffset(baseAddress, offset, sizeof(T));
        return address != nullptr && Write<T>(address, value);
    }

    [[nodiscard]] std::vector<ModuleInfo> Modules() const;
    [[nodiscard]] std::optional<ModuleInfo> FindModule(std::wstring_view moduleName = {}) const;
    [[nodiscard]] std::vector<SectionInfo> ModuleSections(const ModuleInfo& module) const;
    [[nodiscard]] static std::vector<PatternByte> ParsePattern(std::string_view pattern);

    [[nodiscard]] std::optional<std::uintptr_t> FindPattern(
        const ModuleInfo& module,
        std::string_view pattern,
        const PatternScanOptions& options = {}) const;

    [[nodiscard]] std::optional<std::uintptr_t> FindPattern(
        std::wstring_view moduleName,
        std::string_view pattern,
        const PatternScanOptions& options = {}) const;

    [[nodiscard]] std::vector<std::uintptr_t> FindAllPatterns(
        const ModuleInfo& module,
        std::string_view pattern,
        const PatternScanOptions& options = {}) const;

    [[nodiscard]] std::optional<std::uintptr_t> FindPatternInRange(
        std::uintptr_t startAddress,
        std::size_t rangeSize,
        std::string_view pattern,
        const PatternScanOptions& options = {}) const;

    [[nodiscard]] std::optional<std::uintptr_t> ResolveRelativeAddress(
        std::uintptr_t instructionAddress,
        std::size_t displacementOffset,
        std::size_t instructionSize) const noexcept;

    [[nodiscard]] std::optional<std::uintptr_t> FindRelativeAddress(
        const ModuleInfo& module,
        std::string_view pattern,
        std::size_t displacementOffset,
        std::size_t instructionSize,
        const PatternScanOptions& options = {}) const;

    [[nodiscard]] std::optional<std::uintptr_t> DereferenceRemote(
        std::uintptr_t address,
        std::size_t count = 1) const noexcept;

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

    bool AttachToProcess(DWORD processId, HWND gameWindow, DWORD desiredAccess);
    [[nodiscard]] static HWND FindTopLevelWindow(DWORD processId) noexcept;

    [[nodiscard]] std::vector<std::uintptr_t> ScanPatternRange(
        std::uintptr_t startAddress,
        std::size_t rangeSize,
        const std::vector<PatternByte>& pattern,
        const PatternScanOptions& options,
        bool stopAfterOccurrence) const;

    [[nodiscard]] static std::vector<std::uintptr_t> ScanPatternBuffer(
        const std::uint8_t* buffer,
        std::size_t bufferSize,
        std::uintptr_t bufferAddress,
        const std::vector<PatternByte>& pattern);

    [[nodiscard]] bool ReadProcessBuffer(
        std::uintptr_t address,
        void* destination,
        std::size_t size,
        std::size_t& bytesRead) const noexcept;

    [[nodiscard]] static bool IsReadableProtection(DWORD protection) noexcept;
    [[nodiscard]] static bool IsExecutableProtection(DWORD protection) noexcept;
    [[nodiscard]] static bool AddressAdditionOverflows(
        std::uintptr_t address,
        std::size_t amount) noexcept;

    void LogInfo(std::string_view message) const;
    void LogWarning(std::string_view message) const;

    mutable std::mutex mutex_;
    std::unordered_map<void*, AllocationRecord> allocations_;
    mutable Statistics stats_{};
    AllocationId nextId_{1};
    Logger* logger_{nullptr};
    HANDLE processHandle_{nullptr};
    HWND gameWindow_{nullptr};
    DWORD processId_{0};
    DWORD processAccess_{0};
};

} // namespace smf::core
