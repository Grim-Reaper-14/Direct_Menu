#include "core/MemoryManagerAPI.hpp"

#include "core/Logger.hpp"

#include <TlHelp32.h>

#include <algorithm>
#include <cwchar>
#include <format>

namespace smf::core {

MemoryManagerAPI::MemoryManagerAPI(Logger* logger)
    : logger_(logger) {
}

MemoryManagerAPI::~MemoryManagerAPI() {
    DetachProcess();
    ReleaseAll();
}

bool MemoryManagerAPI::AttachToWindow(
    const HWND gameWindow,
    const DWORD desiredAccess) {
    if (gameWindow == nullptr || !IsWindow(gameWindow)) {
        LogWarning("Process attachment rejected because the game window is invalid.");
        return false;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(gameWindow, &processId);
    if (processId == 0) {
        LogWarning("Process attachment failed because the window has no process ID.");
        return false;
    }

    return AttachToProcess(processId, gameWindow, desiredAccess);
}

bool MemoryManagerAPI::AttachToProcessId(
    const DWORD processId,
    const DWORD desiredAccess) {
    if (processId == 0) {
        LogWarning("Process attachment rejected because the PID is zero.");
        return false;
    }

    return AttachToProcess(
        processId,
        FindTopLevelWindow(processId),
        desiredAccess);
}

bool MemoryManagerAPI::AttachToProcessName(
    const std::wstring_view executableName,
    const DWORD desiredAccess) {
    if (executableName.empty()) {
        return false;
    }

    const std::wstring requestedName{executableName};
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD matchedProcessId = 0;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry) != FALSE) {
        do {
            if (_wcsicmp(entry.szExeFile, requestedName.c_str()) == 0) {
                matchedProcessId = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry) != FALSE);
    }
    CloseHandle(snapshot);

    if (matchedProcessId == 0) {
        return false;
    }

    return AttachToProcessId(matchedProcessId, desiredAccess);
}

bool MemoryManagerAPI::AttachToProcess(
    const DWORD processId,
    const HWND gameWindow,
    const DWORD desiredAccess) {
    HANDLE processHandle = OpenProcess(desiredAccess, FALSE, processId);
    if (processHandle == nullptr) {
        LogWarning(std::format(
            "OpenProcess failed for PID {} with Windows error {}.",
            processId,
            GetLastError()));
        return false;
    }

    HANDLE previousHandle = nullptr;
    {
        std::scoped_lock lock(mutex_);
        previousHandle = processHandle_;
        processHandle_ = processHandle;
        gameWindow_ = gameWindow;
        processId_ = processId;
        processAccess_ = desiredAccess;
    }

    if (previousHandle != nullptr) {
        CloseHandle(previousHandle);
    }

    LogInfo(std::format(
        "Attached to window process PID {} with access mask 0x{:08X}.",
        processId,
        desiredAccess));
    return true;
}

bool MemoryManagerAPI::AttachToWindowTitle(
    const std::wstring_view windowTitle,
    const DWORD desiredAccess) {
    if (windowTitle.empty()) {
        LogWarning("Process attachment rejected because the window title is empty.");
        return false;
    }

    const std::wstring title{windowTitle};
    const HWND gameWindow = FindWindowW(nullptr, title.c_str());
    if (gameWindow == nullptr) {
        LogWarning("No top-level window matched the requested game title.");
        return false;
    }

    return AttachToWindow(gameWindow, desiredAccess);
}

bool MemoryManagerAPI::RefreshGameWindow() noexcept {
    DWORD processId = 0;
    {
        std::scoped_lock lock(mutex_);
        processId = processId_;
    }
    if (processId == 0) {
        return false;
    }

    const HWND gameWindow = FindTopLevelWindow(processId);
    {
        std::scoped_lock lock(mutex_);
        if (processId_ != processId) {
            return false;
        }
        gameWindow_ = gameWindow;
    }
    return gameWindow != nullptr;
}

void MemoryManagerAPI::DetachProcess() noexcept {
    HANDLE processHandle = nullptr;
    DWORD processId = 0;
    {
        std::scoped_lock lock(mutex_);
        processHandle = processHandle_;
        processId = processId_;
        processHandle_ = nullptr;
        gameWindow_ = nullptr;
        processId_ = 0;
        processAccess_ = 0;
    }

    if (processHandle != nullptr) {
        CloseHandle(processHandle);
        LogInfo(std::format("Detached from process PID {}.", processId));
    }
}

bool MemoryManagerAPI::IsProcessAttached() const noexcept {
    std::scoped_lock lock(mutex_);
    if (processHandle_ == nullptr || processId_ == 0) {
        return false;
    }

    DWORD exitCode = 0;
    return GetExitCodeProcess(processHandle_, &exitCode) != FALSE &&
           exitCode == STILL_ACTIVE;
}

bool MemoryManagerAPI::HasGameWindow() const noexcept {
    std::scoped_lock lock(mutex_);
    return gameWindow_ != nullptr && IsWindow(gameWindow_);
}

DWORD MemoryManagerAPI::ProcessId() const noexcept {
    std::scoped_lock lock(mutex_);
    return processId_;
}

std::uintptr_t MemoryManagerAPI::ModuleBaseAddress(
    const std::wstring_view moduleName) const noexcept {
    DWORD processId = 0;
    {
        std::scoped_lock lock(mutex_);
        processId = processId_;
    }
    if (processId == 0) {
        return 0;
    }

    const HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        processId);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    const std::wstring requestedName{moduleName};
    std::uintptr_t baseAddress = 0;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry) != FALSE) {
        do {
            if (requestedName.empty() ||
                _wcsicmp(entry.szModule, requestedName.c_str()) == 0) {
                baseAddress = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
                break;
            }
        } while (Module32NextW(snapshot, &entry) != FALSE);
    }

    CloseHandle(snapshot);
    return baseAddress;
}

HANDLE MemoryManagerAPI::ProcessHandle() const noexcept {
    std::scoped_lock lock(mutex_);
    return processHandle_;
}

HWND MemoryManagerAPI::GameWindow() const noexcept {
    std::scoped_lock lock(mutex_);
    return gameWindow_;
}

HWND MemoryManagerAPI::FindTopLevelWindow(const DWORD processId) noexcept {
    struct SearchState {
        DWORD processId{};
        HWND firstWindow{};
        HWND visibleWindow{};
    } state{.processId = processId};

    EnumWindows(
        [](const HWND window, const LPARAM parameter) -> BOOL {
            auto* search = reinterpret_cast<SearchState*>(parameter);
            DWORD windowProcessId = 0;
            GetWindowThreadProcessId(window, &windowProcessId);
            if (windowProcessId != search->processId) {
                return TRUE;
            }

            if (search->firstWindow == nullptr) {
                search->firstWindow = window;
            }
            if (IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr) {
                search->visibleWindow = window;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&state));

    return state.visibleWindow != nullptr ? state.visibleWindow : state.firstWindow;
}

void* MemoryManagerAPI::Allocate(
    const std::size_t size,
    std::size_t alignment,
    std::string tag) {
    if (size == 0) {
        LogWarning("Memory allocation rejected because size is zero.");
        return nullptr;
    }

    if (alignment < alignof(void*)) {
        alignment = alignof(void*);
    }

    if ((alignment & (alignment - 1)) != 0) {
        LogWarning("Memory allocation rejected because alignment is not a power of two.");
        return nullptr;
    }

    void* address = nullptr;
    try {
        address = ::operator new(size, std::align_val_t{alignment});
    } catch (const std::bad_alloc&) {
        LogWarning("Memory allocation failed.");
        return nullptr;
    }

    {
        std::scoped_lock lock(mutex_);
        allocations_.emplace(
            address,
            AllocationRecord{
                .id = nextId_++,
                .address = address,
                .size = size,
                .alignment = alignment,
                .tag = std::move(tag)
            });

        ++stats_.liveAllocations;
        stats_.liveBytes += size;
        ++stats_.totalAllocations;
        stats_.totalBytesAllocated += size;
        stats_.peakAllocations = std::max(stats_.peakAllocations, stats_.liveAllocations);
        stats_.peakBytes = std::max(stats_.peakBytes, stats_.liveBytes);
    }

    LogInfo(std::format("Allocated {} bytes at {}.", size, address));
    return address;
}

bool MemoryManagerAPI::Deallocate(void* address) noexcept {
    if (address == nullptr) {
        return false;
    }

    AllocationRecord record{};
    {
        std::scoped_lock lock(mutex_);
        const auto iterator = allocations_.find(address);
        if (iterator == allocations_.end()) {
            return false;
        }
        record = iterator->second;
        allocations_.erase(iterator);

        --stats_.liveAllocations;
        stats_.liveBytes -= record.size;
        ++stats_.totalDeallocations;
    }

    ::operator delete(record.address, std::align_val_t{record.alignment});
    return true;
}

void MemoryManagerAPI::ReleaseAll() noexcept {
    std::vector<AllocationRecord> pending;
    {
        std::scoped_lock lock(mutex_);
        pending.reserve(allocations_.size());
        for (const auto& [address, record] : allocations_) {
            pending.push_back(record);
        }
        allocations_.clear();
        stats_.totalDeallocations += stats_.liveAllocations;
        stats_.liveAllocations = 0;
        stats_.liveBytes = 0;
    }

    for (const AllocationRecord& record : pending) {
        ::operator delete(record.address, std::align_val_t{record.alignment});
    }
}

std::vector<std::byte> MemoryManagerAPI::CreateBuffer(const std::size_t size) const {
    return std::vector<std::byte>(size);
}

bool MemoryManagerAPI::Owns(const void* address) const noexcept {
    if (address == nullptr) {
        return false;
    }
    std::scoped_lock lock(mutex_);
    return FindContainingLocked(address, 1) != nullptr;
}

bool MemoryManagerAPI::OwnsRange(
    const void* address,
    const std::size_t bytes) const noexcept {
    if (address == nullptr || bytes == 0) {
        return false;
    }
    std::scoped_lock lock(mutex_);
    return FindContainingLocked(address, bytes) != nullptr;
}

std::size_t MemoryManagerAPI::SizeOf(const void* address) const noexcept {
    if (address == nullptr) {
        return 0;
    }

    std::scoped_lock lock(mutex_);
    const AllocationRecord* record = FindContainingLocked(address, 1);
    return record == nullptr ? 0 : record->size;
}

const void* MemoryManagerAPI::ResolveOffset(
    const void* baseAddress,
    const std::ptrdiff_t offset,
    const std::size_t bytes) const noexcept {
    if (baseAddress == nullptr || bytes == 0) {
        return nullptr;
    }

    const auto base = reinterpret_cast<std::uintptr_t>(baseAddress);
    std::uintptr_t resolved = 0;
    if (offset >= 0) {
        const auto positiveOffset = static_cast<std::uintptr_t>(offset);
        if (positiveOffset > UINTPTR_MAX - base) {
            return nullptr;
        }
        resolved = base + positiveOffset;
    } else {
        const auto magnitude =
            static_cast<std::uintptr_t>(-(offset + 1)) + 1;
        if (magnitude > base) {
            return nullptr;
        }
        resolved = base - magnitude;
    }

    const void* address = reinterpret_cast<const void*>(resolved);
    return OwnsRange(address, bytes) ? address : nullptr;
}

bool MemoryManagerAPI::ReadBytes(
    const void* address,
    void* destination,
    const std::size_t bytes) const noexcept {
    if (address == nullptr || destination == nullptr || bytes == 0) {
        return false;
    }

    {
        std::scoped_lock lock(mutex_);
        if (FindContainingLocked(address, bytes) == nullptr) {
            ++stats_.failedRangeChecks;
            return false;
        }
    }

    std::memcpy(destination, address, bytes);
    return true;
}

bool MemoryManagerAPI::WriteBytes(
    void* address,
    const void* source,
    const std::size_t bytes) noexcept {
    if (address == nullptr || source == nullptr || bytes == 0) {
        return false;
    }

    {
        std::scoped_lock lock(mutex_);
        if (FindContainingLocked(address, bytes) == nullptr) {
            ++stats_.failedRangeChecks;
            return false;
        }
    }

    std::memcpy(address, source, bytes);
    return true;
}

std::vector<MemoryManagerAPI::AllocationInfo> MemoryManagerAPI::Snapshot() const {
    std::scoped_lock lock(mutex_);

    std::vector<AllocationInfo> result;
    result.reserve(allocations_.size());

    for (const auto& [address, record] : allocations_) {
        result.push_back(AllocationInfo{
            .id = record.id,
            .address = record.address,
            .size = record.size,
            .alignment = record.alignment,
            .tag = record.tag
        });
    }

    std::ranges::sort(result, {}, &AllocationInfo::id);
    return result;
}

MemoryManagerAPI::Statistics MemoryManagerAPI::Stats() const noexcept {
    std::scoped_lock lock(mutex_);
    return stats_;
}

const MemoryManagerAPI::AllocationRecord* MemoryManagerAPI::FindContainingLocked(
    const void* address,
    const std::size_t bytes) const noexcept {
    const auto requestedStart = reinterpret_cast<std::uintptr_t>(address);
    if (bytes > static_cast<std::size_t>(UINTPTR_MAX - requestedStart)) {
        return nullptr;
    }
    const auto requestedEnd = requestedStart + bytes;

    for (const auto& [baseAddress, record] : allocations_) {
        const auto start = reinterpret_cast<std::uintptr_t>(baseAddress);
        if (record.size > static_cast<std::size_t>(UINTPTR_MAX - start)) {
            continue;
        }
        const auto end = start + record.size;
        if (requestedStart >= start && requestedEnd <= end) {
            return &record;
        }
    }

    return nullptr;
}

void MemoryManagerAPI::LogInfo(const std::string_view message) const {
    if (logger_ != nullptr) {
        logger_->Info(message);
    }
}

void MemoryManagerAPI::LogWarning(const std::string_view message) const {
    if (logger_ != nullptr) {
        logger_->Warning(message);
    }
}

} // namespace smf::core
