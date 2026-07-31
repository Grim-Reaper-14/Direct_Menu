#include "backend/D3D12Backend.hpp"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <limits>

namespace smf::backend {
namespace {

std::string HResultMessage(const std::string_view operation, const HRESULT result) {
    return std::format(
        "{} failed with HRESULT 0x{:08X}.",
        operation,
        static_cast<std::uint32_t>(result));
}

std::string D3D12FailureMessage(
    ID3D12Device* device,
    const std::string_view operation,
    const HRESULT result) {
    std::string message = HResultMessage(operation, result);
    if (device != nullptr) {
        const HRESULT removedReason = device->GetDeviceRemovedReason();
        if (FAILED(removedReason)) {
            message += std::format(
                " Device removal reason: 0x{:08X}.",
                static_cast<std::uint32_t>(removedReason));
        }
    }
    return message;
}

D3D12_HEAP_PROPERTIES HeapProperties(const D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES properties{};
    properties.Type = type;
    properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    properties.CreationNodeMask = 1;
    properties.VisibleNodeMask = 1;
    return properties;
}

D3D12_RESOURCE_DESC BufferDescription(const std::uint64_t byteSize) {
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Alignment = 0;
    description.Width = byteSize;
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = DXGI_FORMAT_UNKNOWN;
    description.SampleDesc.Count = 1;
    description.SampleDesc.Quality = 0;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    description.Flags = D3D12_RESOURCE_FLAG_NONE;
    return description;
}

} // namespace

D3D12Backend* D3D12Backend::descriptorCallbackOwner_ = nullptr;

D3D12Backend::~D3D12Backend() {
    Shutdown();
}

bool D3D12Backend::Initialize(HWND window, std::string& errorMessage) {
    window_ = window;

#if defined(_DEBUG)
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(
            IID_PPV_ARGS(debugController.ReleaseAndGetAddressOf())))) {
        debugController->EnableDebugLayer();
    }
#endif

    HRESULT result = D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(device_.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        errorMessage = HResultMessage("D3D12CreateDevice", result);
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvDescription{};
    rtvDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDescription.NumDescriptors = BackBufferCount;
    rtvDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    result = device_->CreateDescriptorHeap(
        &rtvDescription,
        IID_PPV_ARGS(rtvHeap_.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        errorMessage = HResultMessage("Create RTV descriptor heap", result);
        return false;
    }

    const std::uint32_t rtvIncrement =
        device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    for (auto& descriptor : renderTargetDescriptors_) {
        descriptor = rtvHandle;
        rtvHandle.ptr += rtvIncrement;
    }

    D3D12_DESCRIPTOR_HEAP_DESC srvDescription{};
    srvDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDescription.NumDescriptors = SrvHeapSize;
    srvDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    result = device_->CreateDescriptorHeap(
        &srvDescription,
        IID_PPV_ARGS(srvHeap_.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        errorMessage = HResultMessage("Create SRV descriptor heap", result);
        return false;
    }
    srvAllocator_.Initialize(device_.Get(), srvHeap_.Get());

    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDescription.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    result = device_->CreateCommandQueue(
        &queueDescription,
        IID_PPV_ARGS(commandQueue_.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        errorMessage = HResultMessage("Create command queue", result);
        return false;
    }

    for (auto& frame : frames_) {
        result = device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(frame.commandAllocator.ReleaseAndGetAddressOf()));
        if (FAILED(result)) {
            errorMessage = HResultMessage("Create command allocator", result);
            return false;
        }
    }

    result = device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        frames_.front().commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(commandList_.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        errorMessage = HResultMessage("Create command list", result);
        return false;
    }
    commandList_->Close();

    result = device_->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(fence_.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        errorMessage = HResultMessage("Create fence", result);
        return false;
    }

    fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent_ == nullptr) {
        errorMessage = "CreateEventW failed while creating the D3D12 fence event.";
        return false;
    }

    UINT factoryFlags = 0;
#if defined(_DEBUG)
    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    result = CreateDXGIFactory2(
        factoryFlags,
        IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()));
    if (FAILED(result) && factoryFlags != 0) {
        result = CreateDXGIFactory2(
            0,
            IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()));
    }
    if (FAILED(result)) {
        errorMessage = HResultMessage("CreateDXGIFactory2", result);
        return false;
    }

    BOOL allowTearing = FALSE;
    if (SUCCEEDED(factory->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING,
            &allowTearing,
            sizeof(allowTearing)))) {
        tearingSupported_ = allowTearing == TRUE;
    }
    if (tearingSupported_) {
        swapChainFlags_ |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }

    DXGI_SWAP_CHAIN_DESC1 swapDescription{};
    swapDescription.BufferCount = BackBufferCount;
    swapDescription.Width = 0;
    swapDescription.Height = 0;
    swapDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDescription.Flags = swapChainFlags_;
    swapDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDescription.SampleDesc.Count = 1;
    swapDescription.SampleDesc.Quality = 0;
    swapDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDescription.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapDescription.Scaling = DXGI_SCALING_STRETCH;
    swapDescription.Stereo = FALSE;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> initialSwapChain;
    result = factory->CreateSwapChainForHwnd(
        commandQueue_.Get(),
        window_,
        &swapDescription,
        nullptr,
        nullptr,
        initialSwapChain.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        errorMessage = HResultMessage("CreateSwapChainForHwnd", result);
        return false;
    }

    result = initialSwapChain.As(&swapChain_);
    if (FAILED(result)) {
        errorMessage = HResultMessage("Query IDXGISwapChain3", result);
        return false;
    }

    factory->MakeWindowAssociation(window_, DXGI_MWA_NO_ALT_ENTER);
    swapChain_->SetMaximumFrameLatency(BackBufferCount);
    swapChainWaitableObject_ = swapChain_->GetFrameLatencyWaitableObject();

    if (!CreateRenderTargets()) {
        errorMessage = "Could not create the D3D12 render targets.";
        return false;
    }

    errorMessage.clear();
    return true;
}

bool D3D12Backend::InitializeImGui(HWND window, std::string& errorMessage) {
    if (device_ == nullptr || srvHeap_ == nullptr) {
        errorMessage = "The D3D12 device must be initialized before Dear ImGui.";
        return false;
    }

    if (!ImGui_ImplWin32_Init(window)) {
        errorMessage = "ImGui Win32 backend initialization failed.";
        return false;
    }

    descriptorCallbackOwner_ = this;

    ImGui_ImplDX12_InitInfo initialization{};
    initialization.Device = device_.Get();
    initialization.CommandQueue = commandQueue_.Get();
    initialization.NumFramesInFlight = FrameCount;
    initialization.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initialization.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initialization.SrvDescriptorHeap = srvHeap_.Get();
    initialization.SrvDescriptorAllocFn = [](
                                                 ImGui_ImplDX12_InitInfo*,
                                                 D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
                                                 D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {
        if (descriptorCallbackOwner_ == nullptr) {
            cpu->ptr = 0;
            gpu->ptr = 0;
            return;
        }
        descriptorCallbackOwner_->AllocateSrv(*cpu, *gpu);
    };
    initialization.SrvDescriptorFreeFn = [](
                                                ImGui_ImplDX12_InitInfo*,
                                                const D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                                                const D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
        if (descriptorCallbackOwner_ != nullptr) {
            descriptorCallbackOwner_->FreeSrv(cpu, gpu);
        }
    };

    if (!ImGui_ImplDX12_Init(&initialization)) {
        descriptorCallbackOwner_ = nullptr;
        ImGui_ImplWin32_Shutdown();
        errorMessage = "ImGui Direct3D 12 backend initialization failed.";
        return false;
    }

    imguiInitialized_ = true;
    errorMessage.clear();
    return true;
}

void D3D12Backend::ShutdownImGui() {
    if (!imguiInitialized_) {
        return;
    }

    WaitForIdle();
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    imguiInitialized_ = false;
    descriptorCallbackOwner_ = nullptr;
}

void D3D12Backend::Shutdown() {
    ShutdownImGui();

    if (commandQueue_ != nullptr && fence_ != nullptr) {
        WaitForIdle();
    }

    DestroyRenderTargets();

    if (swapChain_ != nullptr) {
        swapChain_->SetFullscreenState(FALSE, nullptr);
    }
    swapChain_.Reset();

    if (swapChainWaitableObject_ != nullptr) {
        CloseHandle(swapChainWaitableObject_);
        swapChainWaitableObject_ = nullptr;
    }
    if (fenceEvent_ != nullptr) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }

    for (auto& frame : frames_) {
        frame.commandAllocator.Reset();
        frame.fenceValue = 0;
    }

    commandList_.Reset();
    commandQueue_.Reset();
    fence_.Reset();
    srvAllocator_.Shutdown();
    srvHeap_.Reset();
    rtvHeap_.Reset();
    device_.Reset();

    window_ = nullptr;
    frameIndex_ = 0;
    lastSignaledFence_ = 0;
    occluded_ = false;
}

void D3D12Backend::NewFrame() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
}

bool D3D12Backend::Render(
    ImDrawData* drawData,
    const float clearColor[4],
    std::string& errorMessage) {
    if (swapChain_ == nullptr || drawData == nullptr) {
        errorMessage = "Render called without a valid swap chain or draw data.";
        return false;
    }

    FrameContext* frame = WaitForNextFrame();
    if (frame == nullptr) {
        errorMessage = "WaitForNextFrame failed while waiting for the D3D12 fence.";
        return false;
    }

    const UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
    if (backBufferIndex >= renderTargets_.size() ||
        renderTargets_[backBufferIndex] == nullptr) {
        errorMessage = "The current D3D12 back buffer is unavailable.";
        return false;
    }

    HRESULT result = frame->commandAllocator->Reset();
    if (FAILED(result)) {
        errorMessage = D3D12FailureMessage(
            device_.Get(),
            "Reset frame command allocator",
            result);
        return false;
    }
    result = commandList_->Reset(frame->commandAllocator.Get(), nullptr);
    if (FAILED(result)) {
        errorMessage = D3D12FailureMessage(
            device_.Get(),
            "Reset frame command list",
            result);
        return false;
    }

    D3D12_RESOURCE_BARRIER toRenderTarget{};
    toRenderTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRenderTarget.Transition.pResource = renderTargets_[backBufferIndex].Get();
    toRenderTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    toRenderTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    toRenderTarget.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &toRenderTarget);

    commandList_->OMSetRenderTargets(
        1,
        &renderTargetDescriptors_[backBufferIndex],
        FALSE,
        nullptr);
    commandList_->ClearRenderTargetView(
        renderTargetDescriptors_[backBufferIndex],
        clearColor,
        0,
        nullptr);

    ID3D12DescriptorHeap* heaps[] = {srvHeap_.Get()};
    commandList_->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_RenderDrawData(drawData, commandList_.Get());

    D3D12_RESOURCE_BARRIER toPresent = toRenderTarget;
    toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &toPresent);

    result = commandList_->Close();
    if (FAILED(result)) {
        errorMessage = D3D12FailureMessage(
            device_.Get(),
            "Close frame command list",
            result);
        return false;
    }

    ID3D12CommandList* commandLists[] = {commandList_.Get()};
    commandQueue_->ExecuteCommandLists(1, commandLists);

    const std::uint64_t signaledValue = ++lastSignaledFence_;
    result = commandQueue_->Signal(fence_.Get(), signaledValue);
    if (FAILED(result)) {
        errorMessage = D3D12FailureMessage(
            device_.Get(),
            "Signal frame fence",
            result);
        return false;
    }
    frame->fenceValue = signaledValue;

    const HRESULT presentResult = swapChain_->Present(1, 0);
    occluded_ = presentResult == DXGI_STATUS_OCCLUDED;
    ++frameIndex_;
    if (FAILED(presentResult) && !occluded_) {
        errorMessage = D3D12FailureMessage(
            device_.Get(),
            "Present swap chain",
            presentResult);
        return false;
    }

    errorMessage.clear();
    return true;
}

void D3D12Backend::Resize(const std::uint32_t width, const std::uint32_t height) {
    if (swapChain_ == nullptr || width == 0 || height == 0) {
        return;
    }

    WaitForIdle();
    DestroyRenderTargets();

    const HRESULT result = swapChain_->ResizeBuffers(
        BackBufferCount,
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        swapChainFlags_);
    if (SUCCEEDED(result)) {
        CreateRenderTargets();
    }
}

void D3D12Backend::WaitForIdle() {
    if (commandQueue_ == nullptr || fence_ == nullptr || fenceEvent_ == nullptr) {
        return;
    }

    const std::uint64_t value = ++lastSignaledFence_;
    if (FAILED(commandQueue_->Signal(fence_.Get(), value))) {
        return;
    }
    if (fence_->GetCompletedValue() < value) {
        if (SUCCEEDED(fence_->SetEventOnCompletion(value, fenceEvent_))) {
            WaitForSingleObject(fenceEvent_, INFINITE);
        }
    }
}

bool D3D12Backend::IsInitialized() const noexcept {
    return device_ != nullptr && swapChain_ != nullptr;
}

bool D3D12Backend::IsOccluded() {
    if (!occluded_ || swapChain_ == nullptr) {
        return false;
    }

    const HRESULT result = swapChain_->Present(0, DXGI_PRESENT_TEST);
    occluded_ = result == DXGI_STATUS_OCCLUDED;
    return occluded_;
}

bool D3D12Backend::CreateTextureFromRgba(
    const std::span<const std::uint8_t> rgbaPixels,
    const std::uint32_t width,
    const std::uint32_t height,
    TextureResource& output,
    std::string& errorMessage) {
    if (device_ == nullptr || commandQueue_ == nullptr) {
        errorMessage = "The D3D12 backend is not initialized.";
        return false;
    }
    if (width == 0 || height == 0) {
        errorMessage = "The image has invalid dimensions.";
        return false;
    }

    const std::uint64_t requiredBytes =
        static_cast<std::uint64_t>(width) *
        static_cast<std::uint64_t>(height) * 4ULL;
    if (requiredBytes > std::numeric_limits<std::size_t>::max() ||
        rgbaPixels.size() < static_cast<std::size_t>(requiredBytes)) {
        errorMessage = "The decoded image buffer is incomplete.";
        return false;
    }

    DestroyTexture(output);

    D3D12_RESOURCE_DESC textureDescription{};
    textureDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDescription.Width = width;
    textureDescription.Height = height;
    textureDescription.DepthOrArraySize = 1;
    textureDescription.MipLevels = 1;
    textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDescription.SampleDesc.Count = 1;
    textureDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    const D3D12_HEAP_PROPERTIES defaultHeap =
        HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    HRESULT result = device_->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &textureDescription,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(texture.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        errorMessage = HResultMessage("Create image texture", result);
        return false;
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rowCount = 0;
    UINT64 rowSize = 0;
    UINT64 uploadSize = 0;
    device_->GetCopyableFootprints(
        &textureDescription,
        0,
        1,
        0,
        &footprint,
        &rowCount,
        &rowSize,
        &uploadSize);

    const D3D12_HEAP_PROPERTIES uploadHeap =
        HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_RESOURCE_DESC uploadDescription = BufferDescription(uploadSize);
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
    result = device_->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDescription,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        errorMessage = HResultMessage("Create image upload buffer", result);
        return false;
    }

    std::uint8_t* mapped = nullptr;
    const D3D12_RANGE readRange{0, 0};
    result = uploadBuffer->Map(
        0,
        &readRange,
        reinterpret_cast<void**>(&mapped));
    if (FAILED(result) || mapped == nullptr) {
        errorMessage = HResultMessage("Map image upload buffer", result);
        return false;
    }

    const std::size_t sourceRowPitch = static_cast<std::size_t>(width) * 4U;
    for (std::uint32_t row = 0; row < height; ++row) {
        std::memcpy(
            mapped + footprint.Offset +
                static_cast<std::size_t>(row) * footprint.Footprint.RowPitch,
            rgbaPixels.data() + static_cast<std::size_t>(row) * sourceRowPitch,
            sourceRowPitch);
    }
    uploadBuffer->Unmap(0, nullptr);

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> uploadAllocator;
    result = device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(uploadAllocator.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        errorMessage = HResultMessage("Create texture upload allocator", result);
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> uploadList;
    result = device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        uploadAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(uploadList.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        errorMessage = HResultMessage("Create texture upload command list", result);
        return false;
    }

    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = texture.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = uploadBuffer.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = footprint;

    uploadList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    uploadList->ResourceBarrier(1, &barrier);

    result = uploadList->Close();
    if (FAILED(result)) {
        errorMessage = HResultMessage("Close texture upload command list", result);
        return false;
    }

    ID3D12CommandList* uploadLists[] = {uploadList.Get()};
    commandQueue_->ExecuteCommandLists(1, uploadLists);
    WaitForIdle();

    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor{};
    if (!AllocateSrv(cpuDescriptor, gpuDescriptor)) {
        errorMessage = "The shader-resource descriptor heap is full.";
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDescription{};
    srvDescription.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDescription.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(
        texture.Get(),
        &srvDescription,
        cpuDescriptor);

    output.resource = std::move(texture);
    output.cpuDescriptor = cpuDescriptor;
    output.gpuDescriptor = gpuDescriptor;
    output.width = width;
    output.height = height;
    errorMessage.clear();
    return true;
}

void D3D12Backend::DestroyTexture(TextureResource& texture) {
    if (!texture.Valid()) {
        texture.resource.Reset();
        texture = {};
        return;
    }

    WaitForIdle();
    FreeSrv(texture.cpuDescriptor, texture.gpuDescriptor);
    texture.resource.Reset();
    texture = {};
}

bool D3D12Backend::CreateRenderTargets() {
    if (swapChain_ == nullptr || device_ == nullptr) {
        return false;
    }

    for (std::uint32_t index = 0; index < BackBufferCount; ++index) {
        const HRESULT result = swapChain_->GetBuffer(
            index,
            IID_PPV_ARGS(renderTargets_[index].ReleaseAndGetAddressOf()));
        if (FAILED(result)) {
            return false;
        }
        device_->CreateRenderTargetView(
            renderTargets_[index].Get(),
            nullptr,
            renderTargetDescriptors_[index]);
    }
    return true;
}

void D3D12Backend::DestroyRenderTargets() {
    for (auto& target : renderTargets_) {
        target.Reset();
    }
}

D3D12Backend::FrameContext* D3D12Backend::WaitForNextFrame() {
    FrameContext& frame = frames_[frameIndex_ % FrameCount];

    if (swapChainWaitableObject_ != nullptr) {
        if (fence_->GetCompletedValue() < frame.fenceValue) {
            if (FAILED(fence_->SetEventOnCompletion(frame.fenceValue, fenceEvent_))) {
                return nullptr;
            }
            const HANDLE objects[] = {swapChainWaitableObject_, fenceEvent_};
            WaitForMultipleObjects(2, objects, TRUE, INFINITE);
        } else {
            WaitForSingleObject(swapChainWaitableObject_, INFINITE);
        }
    } else if (fence_->GetCompletedValue() < frame.fenceValue) {
        if (FAILED(fence_->SetEventOnCompletion(frame.fenceValue, fenceEvent_))) {
            return nullptr;
        }
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    return &frame;
}

bool D3D12Backend::AllocateSrv(
    D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle) {
    return srvAllocator_.Allocate(cpuHandle, gpuHandle);
}

void D3D12Backend::FreeSrv(
    const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    const D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) {
    srvAllocator_.Free(cpuHandle, gpuHandle);
}

void D3D12Backend::DescriptorAllocator::Initialize(
    ID3D12Device* device,
    ID3D12DescriptorHeap* heap) {
    std::scoped_lock lock{mutex_};
    heap_ = heap;
    const D3D12_DESCRIPTOR_HEAP_DESC description = heap_->GetDesc();
    type_ = description.Type;
    cpuStart_ = heap_->GetCPUDescriptorHandleForHeapStart();
    gpuStart_ = heap_->GetGPUDescriptorHandleForHeapStart();
    increment_ = device->GetDescriptorHandleIncrementSize(type_);

    freeIndices_.clear();
    freeIndices_.reserve(description.NumDescriptors);
    for (std::uint32_t index = description.NumDescriptors; index > 0; --index) {
        freeIndices_.push_back(index - 1);
    }
}

void D3D12Backend::DescriptorAllocator::Shutdown() {
    std::scoped_lock lock{mutex_};
    freeIndices_.clear();
    heap_ = nullptr;
    type_ = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    cpuStart_ = {};
    gpuStart_ = {};
    increment_ = 0;
}

bool D3D12Backend::DescriptorAllocator::Allocate(
    D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle) {
    std::scoped_lock lock{mutex_};
    if (heap_ == nullptr || freeIndices_.empty()) {
        return false;
    }

    const std::uint32_t index = freeIndices_.back();
    freeIndices_.pop_back();
    cpuHandle.ptr = cpuStart_.ptr + static_cast<SIZE_T>(index) * increment_;
    gpuHandle.ptr = gpuStart_.ptr + static_cast<UINT64>(index) * increment_;
    return true;
}

void D3D12Backend::DescriptorAllocator::Free(
    const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    const D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) {
    std::scoped_lock lock{mutex_};
    if (heap_ == nullptr || increment_ == 0 ||
        cpuHandle.ptr < cpuStart_.ptr || gpuHandle.ptr < gpuStart_.ptr) {
        return;
    }

    const auto cpuIndex =
        static_cast<std::uint32_t>((cpuHandle.ptr - cpuStart_.ptr) / increment_);
    const auto gpuIndex =
        static_cast<std::uint32_t>((gpuHandle.ptr - gpuStart_.ptr) / increment_);
    if (cpuIndex != gpuIndex) {
        return;
    }

    const auto heapSize = heap_->GetDesc().NumDescriptors;
    if (cpuIndex < heapSize &&
        std::ranges::find(freeIndices_, cpuIndex) == freeIndices_.end()) {
        freeIndices_.push_back(cpuIndex);
    }
}

} // namespace smf::backend
