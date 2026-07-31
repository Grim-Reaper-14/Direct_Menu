#pragma once

#include "backend/D3D12Backend.hpp"

#include <filesystem>
#include <string>

namespace smf::logging {
class LoggerApi;
}

namespace smf::ui {

class ImageLoader {
public:
    explicit ImageLoader(logging::LoggerApi& logger);

    bool Load(
        const std::filesystem::path& path,
        backend::D3D12Backend& backend,
        std::string& errorMessage);
    void Clear(backend::D3D12Backend& backend);

    [[nodiscard]] const backend::TextureResource& Texture() const noexcept;
    [[nodiscard]] const std::filesystem::path& Path() const noexcept;
    [[nodiscard]] bool HasImage() const noexcept;

private:
    logging::LoggerApi& logger_;
    backend::TextureResource texture_;
    std::filesystem::path path_;
};

} // namespace smf::ui

