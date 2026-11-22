#include "render/Overlay.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <mutex>
#include <string>
#include <utility>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <spdlog/spdlog.h>

namespace lynx::render
{

namespace
{

constexpr UINT kViewCount = 8;
constexpr float kFontSize = 15.0F;
constexpr float kPanelWidth = 360.0F;
constexpr float kPanelMargin = 20.0F;
constexpr ImGuiWindowFlags kPanelFlags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
    ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
constexpr ImGuiTableFlags kTableFlags =
    ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;

// Dear ImGui has process-wide state; the contexts of two stream windows on
// different threads must not interleave.
std::mutex& imGuiMutex()
{
    static std::mutex mutex;
    return mutex;
}

const char* stageLabel(EffectStage stage)
{
    switch (stage)
    {
    case EffectStage::ColorConversion:
        return "Conversão de cor";
    case EffectStage::Scaling:
        return "Escala";
    case EffectStage::Overlay:
        return "Overlay";
    }

    return "?";
}

// Portuguese writes decimals with a comma, as the design does.
std::string millisecondsText(double milliseconds, int precision = 1)
{
    std::string text = std::format("{:.{}f}", milliseconds, precision);
    std::ranges::replace(text, '.', ',');
    text += " ms";

    return text;
}

void applyStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(12.0F, 12.0F);
    style.WindowRounding = 10.0F;
    style.WindowBorderSize = 1.0F;
    style.ItemSpacing = ImVec2(8.0F, 4.0F);

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0392F, 0.0509F, 0.0706F, 0.851F);
    style.Colors[ImGuiCol_Border] = ImVec4(1.0F, 1.0F, 1.0F, 0.1216F);
    style.Colors[ImGuiCol_Text] = ImVec4(0.9294F, 0.9412F, 0.9569F, 1.0F);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.6314F, 0.6667F, 0.7216F, 1.0F);
}

void row(const char* label, const std::string& value)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(label);

    ImGui::TableNextColumn();
    const float valueWidth = ImGui::CalcTextSize(value.c_str()).x;
    const float room = ImGui::GetContentRegionAvail().x;

    if (room > valueWidth)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + room - valueWidth);
    }

    ImGui::TextUnformatted(value.c_str());
}

void drawGpuSection(const OverlayStats& stats)
{
    ImGui::TextDisabled("GPU POR ETAPA");

    if (!ImGui::BeginTable("gpu-times", 2, kTableFlags))
    {
        return;
    }

    for (std::size_t stage = 0; stage < kEffectStageCount; ++stage)
    {
        row(stageLabel(static_cast<EffectStage>(stage)),
            stats.gpuTimingsValid ? millisecondsText(stats.stageMilliseconds.at(stage)) : "—");
    }

    row("Total", stats.gpuTimingsValid ? millisecondsText(stats.totalMilliseconds) : "—");
    ImGui::EndTable();
}

void drawReflexSection(const ReflexOverlayStats& reflex)
{
    ImGui::TextDisabled("NVIDIA REFLEX");

    if (!ImGui::BeginTable("reflex-values", 2, kTableFlags))
    {
        return;
    }

    switch (reflex.state)
    {
    case ReflexOverlayStats::State::Unavailable:
        row("Disponibilidade", reflex.reason.empty() ? "indisponível" : reflex.reason);
        break;

    case ReflexOverlayStats::State::Waiting:
        row("Modo", reflex.mode);
        row("Latência total", "aguardando dados…");
        break;

    case ReflexOverlayStats::State::Active:
    case ReflexOverlayStats::State::Stalled:
    case ReflexOverlayStats::State::Failed:
        row("Modo", reflex.mode);
        row("Sleep médio", millisecondsText(reflex.sleepMeanMs, 3));
        row("Sleep máximo", millisecondsText(reflex.sleepMaxMs, 3));
        row("Driver vê Sleep", reflex.driverSeesSleep ? "sim" : "não");
        row("Latência total",
            reflex.latencyValid ? millisecondsText(reflex.latencyTotalMs) : "—");
        row("GPU", reflex.latencyValid ? millisecondsText(reflex.latencyGpuMs) : "—");
        row("Render", reflex.latencyValid ? millisecondsText(reflex.latencyRenderMs) : "—");
        row("Apresentação",
            reflex.latencyValid ? millisecondsText(reflex.latencyPresentMs) : "—");
        row("Driver", reflex.latencyValid ? millisecondsText(reflex.latencyDriverMs) : "—");
        row("Fila do SO", reflex.latencyValid ? millisecondsText(reflex.latencyQueueMs) : "—");

        if (reflex.state == ReflexOverlayStats::State::Stalled)
        {
            row("Aviso", "sleep alto: desligado nesta transmissão");
        }

        if (reflex.state == ReflexOverlayStats::State::Failed)
        {
            row("Aviso", "o sleep do driver falhou");
        }

        break;
    }

    ImGui::EndTable();
}

void drawPanel(const OverlayStats& stats)
{
    ImGui::SetNextWindowPos(ImVec2(kPanelMargin, kPanelMargin), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kPanelWidth, 0.0F), ImGuiCond_Always);

    if (!ImGui::Begin("lynx-overlay", nullptr, kPanelFlags))
    {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Estatísticas");
    ImGui::Spacing();
    drawGpuSection(stats);
    ImGui::Spacing();
    drawReflexSection(stats.reflex);
    ImGui::End();
}

} // namespace

std::expected<std::unique_ptr<Overlay>, std::string> Overlay::create(
    ID3D12Device& device, ID3D12CommandQueue& queue, HWND window, UINT bufferCount,
    DXGI_FORMAT format, std::span<const std::uint8_t> font)
{
    std::unique_ptr<Overlay> overlay(new Overlay);
    overlay->device_ = &device;

    const std::lock_guard lock(imGuiMutex());
    overlay->context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(overlay->context_);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoKeyboard;

    if (!ImGui_ImplWin32_Init(window))
    {
        ImGui::DestroyContext(std::exchange(overlay->context_, nullptr));
        return std::unexpected(std::string("ImGui_ImplWin32_Init failed"));
    }

    const D3D12_DESCRIPTOR_HEAP_DESC heapDescription{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = kViewCount,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    };

    if (const HRESULT result =
            device.CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&overlay->viewHeap_));
        FAILED(result))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(std::exchange(overlay->context_, nullptr));
        return std::unexpected("CreateDescriptorHeap (overlay) failed");
    }

    overlay->viewsUsed_.assign(kViewCount, false);
    overlay->viewDescriptorSize_ =
        device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    ImGui_ImplDX12_InitInfo info;
    info.Device = &device;
    info.CommandQueue = &queue;
    info.NumFramesInFlight = static_cast<int>(bufferCount);
    info.RTVFormat = format;
    info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    info.UserData = overlay.get();
    info.SrvDescriptorHeap = overlay->viewHeap_.Get();
    info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* initInfo,
                                   D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
                                   D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
    {
        static_cast<Overlay*>(initInfo->UserData)->allocateView(cpu, gpu);
    };
    info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* initInfo,
                                  D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE)
    {
        static_cast<Overlay*>(initInfo->UserData)->freeView(cpu);
    };

    if (!ImGui_ImplDX12_Init(&info))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(std::exchange(overlay->context_, nullptr));
        return std::unexpected("ImGui_ImplDX12_Init failed");
    }

    overlay->fontData_.assign(font.begin(), font.end());
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    ImFont* loadedFont = nullptr;

    if (!overlay->fontData_.empty())
    {
        loadedFont = io.Fonts->AddFontFromMemoryTTF(overlay->fontData_.data(),
                                                     static_cast<int>(overlay->fontData_.size()),
                                                     kFontSize, &fontConfig);
    }

    if (loadedFont == nullptr)
    {
        spdlog::warn("The overlay uses Dear ImGui's default font; accented text may not show");
        io.Fonts->AddFontDefault();
    }

    applyStyle();
    return overlay;
}

Overlay::~Overlay()
{
    if (context_ == nullptr)
    {
        return;
    }

    const std::lock_guard lock(imGuiMutex());
    ImGui::SetCurrentContext(context_);
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(context_);
    context_ = nullptr;
}

void Overlay::render(ID3D12GraphicsCommandList& commandList, D3D12_CPU_DESCRIPTOR_HANDLE target,
                     const OverlayStats& stats)
{
    if (context_ == nullptr)
    {
        return;
    }

    const std::lock_guard lock(imGuiMutex());
    ImGui::SetCurrentContext(context_);
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    drawPanel(stats);
    ImGui::Render();

    const std::array<ID3D12DescriptorHeap*, 1> heaps{viewHeap_.Get()};
    commandList.SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
    commandList.OMSetRenderTargets(1, &target, FALSE, nullptr);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), &commandList);
}

void Overlay::allocateView(D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
{
    for (std::size_t index = 0; index < viewsUsed_.size(); ++index)
    {
        if (viewsUsed_.at(index))
        {
            continue;
        }

        viewsUsed_.at(index) = true;

        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = viewHeap_->GetCPUDescriptorHandleForHeapStart();
        cpuHandle.ptr += index * viewDescriptorSize_;

        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = viewHeap_->GetGPUDescriptorHandleForHeapStart();
        gpuHandle.ptr += index * viewDescriptorSize_;

        *cpu = cpuHandle;
        *gpu = gpuHandle;
        return;
    }

    spdlog::error("The overlay has no free descriptor for an ImGui texture");
    *cpu = {};
    *gpu = {};
}

void Overlay::freeView(D3D12_CPU_DESCRIPTOR_HANDLE cpu)
{
    const D3D12_CPU_DESCRIPTOR_HANDLE first = viewHeap_->GetCPUDescriptorHandleForHeapStart();

    if (cpu.ptr < first.ptr || viewDescriptorSize_ == 0)
    {
        return;
    }

    const std::size_t index = (cpu.ptr - first.ptr) / viewDescriptorSize_;

    if (index < viewsUsed_.size())
    {
        viewsUsed_.at(index) = false;
    }
}

} // namespace lynx::render
