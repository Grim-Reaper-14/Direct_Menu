#pragma once

#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <vector>

namespace smf::core {

class Logger;

class FileSystemMonitor final {
public:
    enum class EventType {
        Added,
        Removed,
        Modified,
        Renamed
    };

    struct Event {
        EventType type{EventType::Modified};
        std::filesystem::path path;
        std::filesystem::path oldPath;
        bool directory{false};
    };

    struct WatchOptions {
        bool recursive{true};
        DWORD notifyFilter{
            FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_LAST_WRITE |
            FILE_NOTIFY_CHANGE_SIZE |
            FILE_NOTIFY_CHANGE_CREATION
        };
    };

    using WatchId = std::uint64_t;
    using Callback = std::function<void(const Event&)>;
    using Dispatcher = std::function<void(std::function<void()>)>;

    explicit FileSystemMonitor(Logger* logger = nullptr);
    ~FileSystemMonitor();

    FileSystemMonitor(const FileSystemMonitor&) = delete;
    FileSystemMonitor& operator=(const FileSystemMonitor&) = delete;

    WatchId Watch(
        const std::filesystem::path& directory,
        Callback callback,
        WatchOptions options = {});

    bool Stop(WatchId id);
    void StopAll();

    void SetDispatcher(Dispatcher dispatcher);

    [[nodiscard]] bool IsWatching(WatchId id) const;
    [[nodiscard]] std::size_t WatchCount() const;

private:
    struct WatchState;

    void RunWatch(WatchState& state, std::stop_token stopToken);
    void Deliver(const Callback& callback, Event event) const;
    void LogInfo(std::string message) const;
    void LogWarning(std::string message) const;
    void LogError(std::string message) const;

    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<WatchState>> watches_;
    Dispatcher dispatcher_;
    Logger* logger_{nullptr};
    std::atomic_uint64_t nextId_{1};
};

} // namespace smf::core
