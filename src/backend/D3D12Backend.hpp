#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <vector>

struct ImDrawData;

namespace smf::backend {

struct TextureResource {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor{};
    std::uint32_t width{0};
    std::uint32_t height{0};

    [[nodiscard]] bool Valid() const noexcept {
        return resource != nullptr && gpuDescriptor.ptr != 0;
    }
};

class D3D12Backend {
public:
    D3D12Backend() = default;
    ~D3D12Backend();

    D3D12Backend(const D3D12Backend&) = delete;
    D3D12Backend& operator=(const D3D12Backend&) = delete;

    bool Initialize(HWND window, std::string& errorMessage);
    bool InitializeImGui(HWND window, std::string& errorMessage);
    void ShutdownImGui();
    void Shutdown();

    void NewFrame();
    bool Render(
        ImDrawData* drawData,
        const float clearColor[4],
        std::string& errorMessage);
    void Resize(std::uint32_t width, std::uint32_t height);
    void WaitForIdle();

    [[nodiscard]] bool IsInitialized() const noexcept;
    [[nodiscard]] bool IsOccluded();

    bool CreateTextureFromRgba(
        std::span<const std::uint8_t> rgbaPixels,
        std::uint32_t width,
        std::uint32_t height,
        TextureResource& output,
        std::string& errorMessage);
    void DestroyTexture(TextureResource& texture);

private:
    static constexpr std::uint32_t FrameCount = 3;
    static constexpr std::uint32_t BackBufferCount = 3;
    static constexpr std::uint32_t SrvHeapSize = 256;

    struct FrameContext {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
        std::uint64_t fenceValue{0};
    };

    class DescriptorAllocator {
    public:
        void Initialize(
            ID3D12Device* device,
            ID3D12DescriptorHeap* heap);
        void Shutdown();
        bool Allocate(
            D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle,
            D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle);
        void Free(
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

    private:
        ID3D12DescriptorHeap* heap_{nullptr};
        D3D12_DESCRIPTOR_HEAP_TYPE type_{D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES};
        D3D12_CPU_DESCRIPTOR_HANDLE cpuStart_{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuStart_{};
        std::uint32_t increment_{0};
        std::vector<std::uint32_t> freeIndices_;
        std::mutex mutex_;
    };

    bool CreateRenderTargets();
    void DestroyRenderTargets();
    FrameContext* WaitForNextFrame();
    bool AllocateSrv(
        D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle);
    void FreeSrv(
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

    static D3D12Backend* descriptorCallbackOwner_;

    HWND window_{nullptr};
    std::array<FrameContext, FrameCount> frames_{};
    std::uint64_t frameIndex_{0};
    std::uint64_t lastSignaledFence_{0};

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
    DescriptorAllocator srvAllocator_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;

    HANDLE fenceEvent_{nullptr};
    HANDLE swapChainWaitableObject_{nullptr};

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, BackBufferCount>
        renderTargets_{};
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, BackBufferCount>
        renderTargetDescriptors_{};

    bool tearingSupported_{false};
    bool occluded_{false};
    bool imguiInitialized_{false};
    std::uint32_t swapChainFlags_{DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT};
};

} // namespace smf::backend
