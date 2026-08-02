#include "core/MemoryManagerAPI.hpp"

#include "core/Logger.hpp"

#include <TlHelp32.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cwchar>
#include <format>
#include <limits>
#include <stdexcept>

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
    const auto module = FindModule(moduleName);
    return module ? module->baseAddress : 0;
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
            static_cast<void>(address);
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
        const auto magnitude = static_cast<std::uintptr_t>(-(offset + 1)) + 1;
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

std::vector<MemoryManagerAPI::ModuleInfo> MemoryManagerAPI::Modules() const {
    std::vector<ModuleInfo> modules;
    DWORD processId = 0;

    {
        std::scoped_lock lock(mutex_);
        processId = processId_;
    }

    if (processId == 0) {
        return modules;
    }

    const HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        processId);

    if (snapshot == INVALID_HANDLE_VALUE) {
        return modules;
    }

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Module32FirstW(snapshot, &entry) != FALSE) {
        do {
            modules.push_back(ModuleInfo{
                .baseAddress = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr),
                .imageSize = static_cast<std::size_t>(entry.modBaseSize),
                .name = entry.szModule,
                .path = entry.szExePath
            });
        } while (Module32NextW(snapshot, &entry) != FALSE);
    }

    CloseHandle(snapshot);
    return modules;
}

std::optional<MemoryManagerAPI::ModuleInfo> MemoryManagerAPI::FindModule(
    const std::wstring_view moduleName) const {
    const auto modules = Modules();
    if (modules.empty()) {
        return std::nullopt;
    }

    if (moduleName.empty()) {
        return modules.front();
    }

    const std::wstring requested{moduleName};
    const auto iterator = std::find_if(
        modules.begin(),
        modules.end(),
        [&requested](const ModuleInfo& module) {
            return _wcsicmp(module.name.c_str(), requested.c_str()) == 0;
        });

    if (iterator == modules.end()) {
        return std::nullopt;
    }

    return *iterator;
}

std::vector<MemoryManagerAPI::SectionInfo> MemoryManagerAPI::ModuleSections(
    const ModuleInfo& module) const {
    std::vector<SectionInfo> sections;
    if (!module.IsValid()) {
        return sections;
    }

    IMAGE_DOS_HEADER dos{};
    std::size_t bytesRead = 0;
    if (!ReadProcessBuffer(module.baseAddress, &dos, sizeof(dos), bytesRead) ||
        bytesRead != sizeof(dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0) {
        return sections;
    }

    const auto ntOffset = static_cast<std::size_t>(dos.e_lfanew);
    if (AddressAdditionOverflows(module.baseAddress, ntOffset)) {
        return sections;
    }

    const std::uintptr_t ntAddress = module.baseAddress + ntOffset;
    DWORD signature = 0;
    if (!ReadProcessBuffer(ntAddress, &signature, sizeof(signature), bytesRead) ||
        bytesRead != sizeof(signature) || signature != IMAGE_NT_SIGNATURE) {
        return sections;
    }

    IMAGE_FILE_HEADER fileHeader{};
    if (!ReadProcessBuffer(
            ntAddress + sizeof(DWORD),
            &fileHeader,
            sizeof(fileHeader),
            bytesRead) ||
        bytesRead != sizeof(fileHeader) ||
        fileHeader.NumberOfSections == 0 ||
        fileHeader.NumberOfSections > 96) {
        return sections;
    }

    const std::uintptr_t firstSection =
        ntAddress + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + fileHeader.SizeOfOptionalHeader;

    const std::uintptr_t moduleEnd = module.EndAddress();
    if (moduleEnd == 0) {
        return sections;
    }

    sections.reserve(fileHeader.NumberOfSections);

    for (WORD index = 0; index < fileHeader.NumberOfSections; ++index) {
        const std::size_t offset = static_cast<std::size_t>(index) * sizeof(IMAGE_SECTION_HEADER);
        if (AddressAdditionOverflows(firstSection, offset)) {
            break;
        }

        IMAGE_SECTION_HEADER header{};
        if (!ReadProcessBuffer(
                firstSection + offset,
                &header,
                sizeof(header),
                bytesRead) ||
            bytesRead != sizeof(header)) {
            break;
        }

        std::array<char, IMAGE_SIZEOF_SHORT_NAME + 1> name{};
        std::memcpy(name.data(), header.Name, IMAGE_SIZEOF_SHORT_NAME);

        SectionInfo section;
        section.name = name.data();
        section.address = module.baseAddress + header.VirtualAddress;
        section.size = std::max<std::size_t>(header.Misc.VirtualSize, header.SizeOfRawData);
        section.characteristics = header.Characteristics;

        if (section.address < module.baseAddress || section.address >= moduleEnd) {
            continue;
        }

        section.size = std::min<std::size_t>(
            section.size,
            static_cast<std::size_t>(moduleEnd - section.address));

        sections.push_back(std::move(section));
    }

    return sections;
}

std::vector<MemoryManagerAPI::PatternByte> MemoryManagerAPI::ParsePattern(
    const std::string_view pattern) {
    std::vector<PatternByte> parsed;
    std::size_t position = 0;

    while (position < pattern.size()) {
        while (position < pattern.size() &&
               std::isspace(static_cast<unsigned char>(pattern[position]))) {
            ++position;
        }

        if (position >= pattern.size()) {
            break;
        }

        const std::size_t start = position;
        while (position < pattern.size() &&
               !std::isspace(static_cast<unsigned char>(pattern[position]))) {
            ++position;
        }

        const std::string_view token = pattern.substr(start, position - start);
        if (token == "?" || token == "??") {
            parsed.push_back(PatternByte{.value = 0, .wildcard = true});
            continue;
        }

        if (token.size() != 2) {
            throw std::invalid_argument("Pattern contains an invalid token.");
        }

        unsigned int value = 0;
        const auto result = std::from_chars(
            token.data(),
            token.data() + token.size(),
            value,
            16);

        if (result.ec != std::errc{} ||
            result.ptr != token.data() + token.size() ||
            value > 0xFF) {
            throw std::invalid_argument("Pattern contains an invalid hexadecimal byte.");
        }

        parsed.push_back(PatternByte{
            .value = static_cast<std::uint8_t>(value),
            .wildcard = false
        });
    }

    if (parsed.empty()) {
        throw std::invalid_argument("Pattern cannot be empty.");
    }

    return parsed;
}

std::optional<std::uintptr_t> MemoryManagerAPI::FindPattern(
    const ModuleInfo& module,
    const std::string_view pattern,
    const PatternScanOptions& options) const {
    if (!module.IsValid()) {
        return std::nullopt;
    }

    std::uintptr_t startAddress = module.baseAddress;
    std::size_t rangeSize = module.imageSize;

    if (!options.sectionName.empty()) {
        const auto sections = ModuleSections(module);
        const auto iterator = std::find_if(
            sections.begin(),
            sections.end(),
            [&options](const SectionInfo& section) {
                return _stricmp(section.name.c_str(), options.sectionName.c_str()) == 0;
            });

        if (iterator == sections.end()) {
            return std::nullopt;
        }

        startAddress = iterator->address;
        rangeSize = iterator->size;
    }

    return FindPatternInRange(startAddress, rangeSize, pattern, options);
}

std::optional<std::uintptr_t> MemoryManagerAPI::FindPattern(
    const std::wstring_view moduleName,
    const std::string_view pattern,
    const PatternScanOptions& options) const {
    const auto module = FindModule(moduleName);
    if (!module) {
        return std::nullopt;
    }

    return FindPattern(*module, pattern, options);
}

std::vector<std::uintptr_t> MemoryManagerAPI::FindAllPatterns(
    const ModuleInfo& module,
    const std::string_view pattern,
    const PatternScanOptions& options) const {
    if (!module.IsValid()) {
        return {};
    }

    std::uintptr_t startAddress = module.baseAddress;
    std::size_t rangeSize = module.imageSize;

    if (!options.sectionName.empty()) {
        const auto sections = ModuleSections(module);
        const auto iterator = std::find_if(
            sections.begin(),
            sections.end(),
            [&options](const SectionInfo& section) {
                return _stricmp(section.name.c_str(), options.sectionName.c_str()) == 0;
            });

        if (iterator == sections.end()) {
            return {};
        }

        startAddress = iterator->address;
        rangeSize = iterator->size;
    }

    auto results = ScanPatternRange(
        startAddress,
        rangeSize,
        ParsePattern(pattern),
        options,
        false);

    if (options.resultOffset == 0) {
        return results;
    }

    std::vector<std::uintptr_t> adjusted;
    adjusted.reserve(results.size());

    for (const auto result : results) {
        if (options.resultOffset >= 0) {
            const auto offset = static_cast<std::size_t>(options.resultOffset);
            if (!AddressAdditionOverflows(result, offset)) {
                adjusted.push_back(result + offset);
            }
        } else {
            const auto offset = static_cast<std::uintptr_t>(
                -static_cast<std::int64_t>(options.resultOffset));
            if (offset <= result) {
                adjusted.push_back(result - offset);
            }
        }
    }

    return adjusted;
}

std::optional<std::uintptr_t> MemoryManagerAPI::FindPatternInRange(
    const std::uintptr_t startAddress,
    const std::size_t rangeSize,
    const std::string_view pattern,
    const PatternScanOptions& options) const {
    if (startAddress == 0 || rangeSize == 0) {
        return std::nullopt;
    }

    const auto results = ScanPatternRange(
        startAddress,
        rangeSize,
        ParsePattern(pattern),
        options,
        true);

    if (options.occurrence >= results.size()) {
        return std::nullopt;
    }

    std::uintptr_t result = results[options.occurrence];
    if (options.resultOffset >= 0) {
        const auto offset = static_cast<std::size_t>(options.resultOffset);
        if (AddressAdditionOverflows(result, offset)) {
            return std::nullopt;
        }
        result += offset;
    } else {
        const auto offset = static_cast<std::uintptr_t>(
            -static_cast<std::int64_t>(options.resultOffset));
        if (offset > result) {
            return std::nullopt;
        }
        result -= offset;
    }

    return result;
}

std::optional<std::uintptr_t> MemoryManagerAPI::ResolveRelativeAddress(
    const std::uintptr_t instructionAddress,
    const std::size_t displacementOffset,
    const std::size_t instructionSize) const noexcept {
    if (instructionAddress == 0 || instructionSize == 0 ||
        AddressAdditionOverflows(instructionAddress, displacementOffset)) {
        return std::nullopt;
    }

    std::int32_t displacement = 0;
    std::size_t bytesRead = 0;
    if (!ReadProcessBuffer(
            instructionAddress + displacementOffset,
            &displacement,
            sizeof(displacement),
            bytesRead) ||
        bytesRead != sizeof(displacement)) {
        return std::nullopt;
    }

    if (AddressAdditionOverflows(instructionAddress, instructionSize)) {
        return std::nullopt;
    }

    const std::uintptr_t nextInstruction = instructionAddress + instructionSize;
    if (displacement >= 0) {
        const auto offset = static_cast<std::uintptr_t>(displacement);
        if (AddressAdditionOverflows(nextInstruction, offset)) {
            return std::nullopt;
        }
        return nextInstruction + offset;
    }

    const auto offset = static_cast<std::uintptr_t>(
        -static_cast<std::int64_t>(displacement));
    if (offset > nextInstruction) {
        return std::nullopt;
    }

    return nextInstruction - offset;
}

std::optional<std::uintptr_t> MemoryManagerAPI::FindRelativeAddress(
    const ModuleInfo& module,
    const std::string_view pattern,
    const std::size_t displacementOffset,
    const std::size_t instructionSize,
    const PatternScanOptions& options) const {
    const auto instruction = FindPattern(module, pattern, options);
    if (!instruction) {
        return std::nullopt;
    }

    return ResolveRelativeAddress(
        *instruction,
        displacementOffset,
        instructionSize);
}

std::optional<std::uintptr_t> MemoryManagerAPI::DereferenceRemote(
    std::uintptr_t address,
    const std::size_t count) const noexcept {
    if (address == 0) {
        return std::nullopt;
    }

    for (std::size_t index = 0; index < count; ++index) {
        std::uintptr_t pointer = 0;
        std::size_t bytesRead = 0;
        if (!ReadProcessBuffer(address, &pointer, sizeof(pointer), bytesRead) ||
            bytesRead != sizeof(pointer) || pointer == 0) {
            return std::nullopt;
        }
        address = pointer;
    }

    return address;
}

std::vector<std::uintptr_t> MemoryManagerAPI::ScanPatternRange(
    const std::uintptr_t startAddress,
    const std::size_t rangeSize,
    const std::vector<PatternByte>& pattern,
    const PatternScanOptions& options,
    const bool stopAfterOccurrence) const {
    std::vector<std::uintptr_t> results;

    if (startAddress == 0 || pattern.empty() || rangeSize < pattern.size() ||
        AddressAdditionOverflows(startAddress, rangeSize)) {
        return results;
    }

    HANDLE processHandle = nullptr;
    {
        std::scoped_lock lock(mutex_);
        processHandle = processHandle_;
    }

    if (processHandle == nullptr) {
        return results;
    }

    const std::uintptr_t endAddress = startAddress + rangeSize;
    const std::size_t chunkSize = std::max(options.chunkSize, pattern.size());
    const std::size_t overlap = pattern.size() > 1 ? pattern.size() - 1 : 0;
    std::uintptr_t currentAddress = startAddress;

    while (currentAddress < endAddress) {
        std::uintptr_t readableEnd = endAddress;

        if (options.validateMemoryPages) {
            MEMORY_BASIC_INFORMATION information{};
            if (VirtualQueryEx(
                    processHandle,
                    reinterpret_cast<LPCVOID>(currentAddress),
                    &information,
                    sizeof(information)) == 0) {
                break;
            }

            const auto regionBase = reinterpret_cast<std::uintptr_t>(information.BaseAddress);
            if (AddressAdditionOverflows(regionBase, information.RegionSize)) {
                break;
            }

            const std::uintptr_t regionEnd = regionBase + information.RegionSize;
            const bool usable =
                information.State == MEM_COMMIT &&
                IsReadableProtection(information.Protect) &&
                (!options.executablePagesOnly || IsExecutableProtection(information.Protect));

            if (!usable) {
                if (!options.skipUnreadablePages) {
                    return {};
                }

                if (regionEnd <= currentAddress) {
                    break;
                }

                currentAddress = std::min(regionEnd, endAddress);
                continue;
            }

            readableEnd = std::min(regionEnd, endAddress);
        }

        const std::size_t remaining = static_cast<std::size_t>(readableEnd - currentAddress);
        const std::size_t requestedSize = std::min(chunkSize, remaining);
        if (requestedSize == 0) {
            break;
        }

        std::vector<std::uint8_t> buffer(requestedSize);
        std::size_t bytesRead = 0;
        if (ReadProcessBuffer(
                currentAddress,
                buffer.data(),
                buffer.size(),
                bytesRead) &&
            bytesRead >= pattern.size()) {
            const auto matches = ScanPatternBuffer(
                buffer.data(),
                bytesRead,
                currentAddress,
                pattern);

            for (const auto match : matches) {
                if (results.empty() || results.back() != match) {
                    results.push_back(match);
                }

                if (stopAfterOccurrence && results.size() > options.occurrence) {
                    return results;
                }
            }
        }

        std::size_t advance = requestedSize;
        if (advance > overlap) {
            advance -= overlap;
        }
        if (advance == 0) {
            advance = requestedSize;
        }
        if (AddressAdditionOverflows(currentAddress, advance)) {
            break;
        }

        currentAddress += advance;
    }

    return results;
}

std::vector<std::uintptr_t> MemoryManagerAPI::ScanPatternBuffer(
    const std::uint8_t* buffer,
    const std::size_t bufferSize,
    const std::uintptr_t bufferAddress,
    const std::vector<PatternByte>& pattern) {
    std::vector<std::uintptr_t> results;

    if (buffer == nullptr || pattern.empty() || bufferSize < pattern.size()) {
        return results;
    }

    const std::size_t lastOffset = bufferSize - pattern.size();
    for (std::size_t offset = 0; offset <= lastOffset; ++offset) {
        bool matched = true;

        for (std::size_t index = 0; index < pattern.size(); ++index) {
            if (!pattern[index].wildcard &&
                buffer[offset + index] != pattern[index].value) {
                matched = false;
                break;
            }
        }

        if (matched && !AddressAdditionOverflows(bufferAddress, offset)) {
            results.push_back(bufferAddress + offset);
        }
    }

    return results;
}

bool MemoryManagerAPI::ReadProcessBuffer(
    const std::uintptr_t address,
    void* destination,
    const std::size_t size,
    std::size_t& bytesRead) const noexcept {
    bytesRead = 0;
    if (address == 0 || destination == nullptr || size == 0) {
        return false;
    }

    HANDLE processHandle = nullptr;
    {
        std::scoped_lock lock(mutex_);
        processHandle = processHandle_;
    }

    if (processHandle == nullptr) {
        return false;
    }

    SIZE_T nativeBytesRead = 0;
    const BOOL result = ReadProcessMemory(
        processHandle,
        reinterpret_cast<LPCVOID>(address),
        destination,
        size,
        &nativeBytesRead);

    bytesRead = static_cast<std::size_t>(nativeBytesRead);
    return result != FALSE || nativeBytesRead > 0;
}

bool MemoryManagerAPI::IsReadableProtection(const DWORD protection) noexcept {
    if ((protection & PAGE_GUARD) != 0 ||
        (protection & PAGE_NOACCESS) != 0) {
        return false;
    }

    switch (protection & 0xFF) {
        case PAGE_READONLY:
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
    }
}

bool MemoryManagerAPI::IsExecutableProtection(const DWORD protection) noexcept {
    if ((protection & PAGE_GUARD) != 0 ||
        (protection & PAGE_NOACCESS) != 0) {
        return false;
    }

    switch (protection & 0xFF) {
        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
    }
}

bool MemoryManagerAPI::AddressAdditionOverflows(
    const std::uintptr_t address,
    const std::size_t amount) noexcept {
    return amount > std::numeric_limits<std::uintptr_t>::max() - address;
}

std::vector<MemoryManagerAPI::AllocationInfo> MemoryManagerAPI::Snapshot() const {
    std::scoped_lock lock(mutex_);

    std::vector<AllocationInfo> result;
    result.reserve(allocations_.size());

    for (const auto& [address, record] : allocations_) {
        static_cast<void>(address);
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
