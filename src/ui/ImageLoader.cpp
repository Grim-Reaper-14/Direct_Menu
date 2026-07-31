#include "ui/ImageLoader.hpp"

#include "logging/Logger.hpp"

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstdint>
#include <format>
#include <limits>
#include <vector>

namespace smf::ui {
namespace {

std::string WicError(const std::string_view operation, const HRESULT result) {
    return std::format(
        "{} failed with HRESULT 0x{:08X}.",
        operation,
        static_cast<std::uint32_t>(result));
}

} // namespace

ImageLoader::ImageLoader(logging::LoggerApi& logger)
    : logger_(logger) {
}

bool ImageLoader::Load(
    const std::filesystem::path& path,
    backend::D3D12Backend& backend,
    std::string& errorMessage) {
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        errorMessage = WicError("Create WIC imaging factory", result);
        logger_.Error(errorMessage);
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    result = factory->CreateDecoderFromFilename(
        path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        decoder.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        errorMessage = WicError("Open image", result);
        logger_.Error(errorMessage);
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    result = decoder->GetFrame(0, frame.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        errorMessage = WicError("Decode image frame", result);
        logger_.Error(errorMessage);
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    result = frame->GetSize(&width, &height);
    if (FAILED(result) || width == 0 || height == 0) {
        errorMessage = "The selected image has invalid dimensions.";
        logger_.Error(errorMessage);
        return false;
    }

    constexpr UINT maximumDimension = 16384;
    if (width > maximumDimension || height > maximumDimension) {
        errorMessage = "The selected image is larger than 16384 pixels on one axis.";
        logger_.Warning(errorMessage);
        return false;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    result = factory->CreateFormatConverter(converter.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        errorMessage = WicError("Create image format converter", result);
        logger_.Error(errorMessage);
        return false;
    }

    result = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(result)) {
        errorMessage = WicError("Convert image to RGBA", result);
        logger_.Error(errorMessage);
        return false;
    }

    const std::uint64_t stride64 = static_cast<std::uint64_t>(width) * 4ULL;
    const std::uint64_t byteCount64 = stride64 * static_cast<std::uint64_t>(height);
    if (stride64 > std::numeric_limits<UINT>::max() ||
        byteCount64 > std::numeric_limits<UINT>::max() ||
        byteCount64 > std::numeric_limits<std::size_t>::max()) {
        errorMessage = "The decoded image is too large for the WIC upload path.";
        logger_.Warning(errorMessage);
        return false;
    }

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(byteCount64));
    result = converter->CopyPixels(
        nullptr,
        static_cast<UINT>(stride64),
        static_cast<UINT>(byteCount64),
        pixels.data());
    if (FAILED(result)) {
        errorMessage = WicError("Copy decoded image pixels", result);
        logger_.Error(errorMessage);
        return false;
    }

    if (!backend.CreateTextureFromRgba(
            pixels,
            width,
            height,
            texture_,
            errorMessage)) {
        logger_.Error(errorMessage);
        return false;
    }

    path_ = path;
    logger_.Info("Loaded image: " + path.string());
    errorMessage.clear();
    return true;
}

void ImageLoader::Clear(backend::D3D12Backend& backend) {
    backend.DestroyTexture(texture_);
    path_.clear();
}

const backend::TextureResource& ImageLoader::Texture() const noexcept {
    return texture_;
}

const std::filesystem::path& ImageLoader::Path() const noexcept {
    return path_;
}

bool ImageLoader::HasImage() const noexcept {
    return texture_.Valid();
}

} // namespace smf::ui

