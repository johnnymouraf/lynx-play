#pragma once

#include "core/Windows.h"
#include "render/FrameMailbox.h"
#include "render/GpuDevice.h"

#include <cstdint>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace lynx::render
{

// A video source that draws the animated test pattern into 1080p NV12 frames
// at 60 fps, through the same path as decoded video.
class TestPatternSource
{
public:
    [[nodiscard]] static std::expected<std::unique_ptr<TestPatternSource>, std::string> start(
        const GpuDevice& gpu, std::shared_ptr<FrameMailbox> mailbox);

    TestPatternSource(const TestPatternSource&) = delete;
    TestPatternSource& operator=(const TestPatternSource&) = delete;
    TestPatternSource(TestPatternSource&&) = delete;
    TestPatternSource& operator=(TestPatternSource&&) = delete;

    // Stops drawing; frames already handed out stay valid.
    ~TestPatternSource();

private:
    struct Slot;

    TestPatternSource(ComPtr<ID3D12Device> device, std::shared_ptr<FrameMailbox> mailbox);

    [[nodiscard]] std::expected<void, std::string> createResources();
    [[nodiscard]] std::expected<std::shared_ptr<Slot>, std::string> createSlot();
    void run(const std::stop_token& stop);
    [[nodiscard]] std::expected<void, std::string> drawFrame(double seconds);
    [[nodiscard]] std::shared_ptr<Slot> freeSlot();
    void waitForCopy(std::uint64_t value);

    ComPtr<ID3D12Device> device_;
    std::shared_ptr<FrameMailbox> mailbox_;
    ComPtr<ID3D12CommandQueue> copyQueue_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    ComPtr<ID3D12Fence> fence_;
    std::uint64_t lastSignaled_ = 0;
    core::UniqueHandle fenceEvent_;
    std::vector<std::shared_ptr<Slot>> slots_;
    // Last, so the thread starts after every member exists and stops first.
    std::jthread thread_;
};

} // namespace lynx::render
