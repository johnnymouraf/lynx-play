#pragma once

#include "core/Windows.h"
#include "render/AdapterSelection.h"
#include "render/GpuDevice.h"
#include "render/Layout.h"
#include "render/VideoFrame.h"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace lynx::render
{

// Why RTX Video Super Resolution can't run here, for the RTX screen.
enum class VsrUnavailable
{
    None,
    NoNvidiaGpu,
    NoD3D11,
    FormatUnsupported,
    ProcessorFailed,
    ExtensionRejected,
};

struct VsrAvailability
{
    bool available = false;
    VsrUnavailable reason = VsrUnavailable::NoNvidiaGpu;
    // For the log; English, like every other log line.
    std::string detail;
};

// The picture the video processor is built for: the source size and format
// and the size it scales to.
struct VsrPicture
{
    SurfaceSize input;
    SurfaceSize output;
    PixelFormat format = PixelFormat::Nv12;
    ColorMatrix matrix = ColorMatrix::Bt709;
    ColorRange range = ColorRange::Limited;

    bool operator==(const VsrPicture&) const = default;
};

// Whether the video processor path can run: VSR is on, the stream renders on
// an NVIDIA adapter, and the fit rectangle doesn't shrink the picture (the
// driver's model only upscales and cleans up; it doesn't run when downward).
[[nodiscard]] bool videoSuperResolutionEligible(bool enabled, std::uint32_t vendorId,
                                                SurfaceSize input, SurfaceSize output);

// NVIDIA's RTX Video Super Resolution through the driver's D3D11 video
// processor extension, fed by textures shared with the Direct3D 12 renderer:
// the renderer copies each decoded frame into an NV12/P010 input texture and
// the processor writes R8G8B8A8 at the output size, ordered by one fence
// shared between both devices. One instance per stream window.
class VideoSuperResolution
{
public:
    // Checks the extension on the stream's adapter without touching a stream;
    // never fails the app on a machine without an NVIDIA GPU.
    [[nodiscard]] static VsrAvailability probe();

    // Uses the renderer's D3D11 device, a native device on the stream's
    // adapter (D3D11On12 returns E_NOTIMPL for the extension).
    [[nodiscard]] static std::expected<VideoSuperResolution, std::string> create(
        ID3D12Device& device12, ID3D11Device& device11, ID3D11DeviceContext& context11,
        UINT slotCount);

    VideoSuperResolution(const VideoSuperResolution&) = delete;
    VideoSuperResolution& operator=(const VideoSuperResolution&) = delete;
    VideoSuperResolution(VideoSuperResolution&&) = default;
    VideoSuperResolution& operator=(VideoSuperResolution&&) = default;
    ~VideoSuperResolution() = default;

    [[nodiscard]] bool configuredFor(const VsrPicture& picture) const;

    // Builds the processor, the views and the shared textures for these
    // sizes. A no-op when they are already the configured ones; the GPU must
    // be idle, since the old textures are released.
    [[nodiscard]] std::expected<void, std::string> configure(const VsrPicture& picture);

    [[nodiscard]] ID3D12Resource* input(UINT slot) const;
    [[nodiscard]] ID3D12Resource* output(UINT slot) const;

    // The fence shared with the renderer's D3D12 queue.
    [[nodiscard]] ID3D12Fence* fence() const;

    // A value the D3D12 queue signals once it has copied a frame into a slot.
    [[nodiscard]] UINT64 inputReadyValue();

    // Waits for `inputReady` on the D3D11 context, runs the video processor
    // into `slot` and signals the returned value: the D3D12 queue waits for
    // it before drawing the output. Flushed so the signal reaches the
    // scheduler without a CPU wait.
    [[nodiscard]] std::expected<UINT64, std::string> blit(UINT slot, UINT64 inputReady);

    // What the configured processor does, for the log.
    [[nodiscard]] const std::string& description() const;

private:
    VideoSuperResolution() = default;

    struct Slot
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> input;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> inputTexture;
        // Set when the processor rejects the shared input texture: the shared
        // one is copied into this local one before the blit.
        Microsoft::WRL::ComPtr<ID3D11Texture2D> inputLocal;
        Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> inputView;
        Microsoft::WRL::ComPtr<ID3D12Resource> output;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> outputTexture;
        Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> outputView;
    };

    [[nodiscard]] std::expected<Slot, std::string> createSlot(const VsrPicture& picture);

    Microsoft::WRL::ComPtr<ID3D12Device> device12_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11Device5> device5_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context4_;
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> videoDevice_;
    Microsoft::WRL::ComPtr<ID3D11VideoContext1> videoContext_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> enumerator_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> processor_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence12_;
    Microsoft::WRL::ComPtr<ID3D11Fence> fence11_;
    std::vector<Slot> slots_;
    std::optional<VsrPicture> configured_;
    std::string description_;
    UINT slotCount_ = 0;
    UINT64 nextFenceValue_ = 0;
};

} // namespace lynx::render
