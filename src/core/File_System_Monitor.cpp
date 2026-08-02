#include "core/File_System_Monitor.hpp"

#include "core/Logger.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <thread>
#include <utility>

namespace smf::core {

struct FileSystemMonitor::WatchState {
    WatchId id{};
    std::filesystem::path directory;
    Callback callback;
    WatchOptions options;
    HANDLE handle{INVALID_HANDLE_VALUE};
    std::jthread thread;
};

FileSystemMonitor::FileSystemMonitor(Logger* logger)
    : logger_(logger) {
}

FileSystemMonitor::~FileSystemMonitor() {
    StopAll();
}

FileSystemMonitor::WatchId FileSystemMonitor::Watch(
    const std::filesystem::path& directory,
    Callback callback,
    WatchOptions options) {
    if (!callback) {
        LogWarning("File-system watch rejected because the callback is empty.");
        return 0;
    }

    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(directory, error);
    if (error || !std::filesystem::is_directory(absolute, error)) {
        LogError("Cannot watch directory: " + directory.string());
        return 0;
    }

    HANDLE handle = CreateFileW(
        absolute.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        LogError(std::format(
            "CreateFileW failed for monitored directory '{}' (Win32 {}).",
            absolute.string(),
            GetLastError()));
        return 0;
    }

    auto state = std::make_unique<WatchState>();
    state->id = nextId_.fetch_add(1);
    state->directory = absolute;
    state->callback = std::move(callback);
    state->options = options;
    state->handle = handle;

    WatchState* rawState = state.get();
    state->thread = std::jthread(
        [this, rawState](std::stop_token stopToken) {
            RunWatch(*rawState, stopToken);
        });

    const WatchId id = state->id;
    {
        std::scoped_lock lock(mutex_);
        watches_.push_back(std::move(state));
    }

    LogInfo(std::format(
        "Watching '{}' (id={}, recursive={}).",
        absolute.string(),
        id,
        options.recursive));
    return id;
}

bool FileSystemMonitor::Stop(const WatchId id) {
    std::unique_ptr<WatchState> state;

    {
        std::scoped_lock lock(mutex_);
        const auto found = std::ranges::find_if(
            watches_,
            [id](const std::unique_ptr<WatchState>& candidate) {
                return candidate != nullptr && candidate->id == id;
            });

        if (found == watches_.end()) {
            return false;
        }

        state = std::move(*found);
        watches_.erase(found);
    }

    if (state->thread.joinable()) {
        state->thread.request_stop();
    }

    if (state->handle != INVALID_HANDLE_VALUE) {
        CancelIoEx(state->handle, nullptr);
    }

    if (state->thread.joinable()) {
        state->thread.join();
    }

    if (state->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(state->handle);
        state->handle = INVALID_HANDLE_VALUE;
    }

    LogInfo(std::format("Stopped file-system watch id={}", id));
    return true;
}

void FileSystemMonitor::StopAll() {
    std::vector<std::unique_ptr<WatchState>> watches;

    {
        std::scoped_lock lock(mutex_);
        watches.swap(watches_);
    }

    for (auto& state : watches) {
        if (!state) {
            continue;
        }

        if (state->thread.joinable()) {
            state->thread.request_stop();
        }
        if (state->handle != INVALID_HANDLE_VALUE) {
            CancelIoEx(state->handle, nullptr);
        }
    }

    for (auto& state : watches) {
        if (!state) {
            continue;
        }

        if (state->thread.joinable()) {
            state->thread.join();
        }
        if (state->handle != INVALID_HANDLE_VALUE) {
            CloseHandle(state->handle);
            state->handle = INVALID_HANDLE_VALUE;
        }
    }
}

void FileSystemMonitor::SetDispatcher(Dispatcher dispatcher) {
    std::scoped_lock lock(mutex_);
    dispatcher_ = std::move(dispatcher);
}

bool FileSystemMonitor::IsWatching(const WatchId id) const {
    std::scoped_lock lock(mutex_);
    return std::ranges::any_of(
        watches_,
        [id](const std::unique_ptr<WatchState>& state) {
            return state != nullptr && state->id == id;
        });
}

std::size_t FileSystemMonitor::WatchCount() const {
    std::scoped_lock lock(mutex_);
    return watches_.size();
}

void FileSystemMonitor::RunWatch(
    WatchState& state,
    const std::stop_token stopToken) {
    constexpr DWORD bufferSize = 64U * 1024U;
    alignas(FILE_NOTIFY_INFORMATION) std::array<std::byte, bufferSize> buffer{};
    std::filesystem::path pendingRenameOld;

    while (!stopToken.stop_requested()) {
        DWORD bytesReturned = 0;
        const BOOL success = ReadDirectoryChangesW(
            state.handle,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            state.options.recursive ? TRUE : FALSE,
            state.options.notifyFilter,
            &bytesReturned,
            nullptr,
            nullptr);

        if (stopToken.stop_requested()) {
            break;
        }

        if (success == FALSE) {
            const DWORD error = GetLastError();
            if (error == ERROR_OPERATION_ABORTED || error == ERROR_INVALID_HANDLE) {
                break;
            }
            LogError(std::format(
                "ReadDirectoryChangesW failed for '{}' (Win32 {}).",
                state.directory.string(),
                error));
            break;
        }

        if (bytesReturned == 0) {
            continue;
        }

        DWORD offset = 0;
        while (offset < bytesReturned) {
            const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
                buffer.data() + offset);

            const std::wstring relativeName(
                info->FileName,
                info->FileNameLength / sizeof(wchar_t));
            const std::filesystem::path fullPath =
                state.directory / relativeName;

            Event event{};
            event.path = fullPath;

            std::error_code typeError;
            event.directory = std::filesystem::is_directory(fullPath, typeError);

            bool deliver = true;
            switch (info->Action) {
            case FILE_ACTION_ADDED:
                event.type = EventType::Added;
                break;
            case FILE_ACTION_REMOVED:
                event.type = EventType::Removed;
                break;
            case FILE_ACTION_MODIFIED:
                event.type = EventType::Modified;
                break;
            case FILE_ACTION_RENAMED_OLD_NAME:
                pendingRenameOld = fullPath;
                deliver = false;
                break;
            case FILE_ACTION_RENAMED_NEW_NAME:
                event.type = EventType::Renamed;
                event.oldPath = std::exchange(
                    pendingRenameOld,
                    std::filesystem::path{});
                break;
            default:
                deliver = false;
                break;
            }

            if (deliver) {
                Deliver(state.callback, std::move(event));
            }

            if (info->NextEntryOffset == 0) {
                break;
            }
            offset += info->NextEntryOffset;
        }
    }
}

void FileSystemMonitor::Deliver(
    const Callback& callback,
    Event event) const {
    Dispatcher dispatcher;
    {
        std::scoped_lock lock(mutex_);
        dispatcher = dispatcher_;
    }

    if (dispatcher) {
        dispatcher([
            callback,
            event = std::move(event)]() mutable {
            callback(event);
        });
        return;
    }

    callback(event);
}

void FileSystemMonitor::LogInfo(std::string message) const {
    if (logger_ != nullptr) {
        logger_->Info(message);
    }
}

void FileSystemMonitor::LogWarning(std::string message) const {
    if (logger_ != nullptr) {
        logger_->Warning(message);
    }
}

void FileSystemMonitor::LogError(std::string message) const {
    if (logger_ != nullptr) {
        logger_->Error(message);
    }
}

} // namespace smf::core
