#include "render/Overlay.h"

#include "render/Hdr.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <spdlog/spdlog.h>

// Dear ImGui leaves this one for the application to declare, to keep
// <windows.h> out of its header.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                              LPARAM lParam);

namespace mfplay::render
{

namespace
{

constexpr UINT kViewCount = 8;

constexpr float kPanelMargin = 20.0F;
constexpr float kStatsWidth = 320.0F;
constexpr float kMenuWidth = 380.0F;
constexpr float kMenuHeight = 680.0F;
constexpr float kPanelPadding = 16.0F;
constexpr float kGap = 6.0F;

constexpr float kRowHeight = 20.0F;
constexpr float kCaptionHeight = 16.0F;
constexpr float kOverlineHeight = 12.0F;
constexpr float kOverlineTracking = 2.0F;

constexpr float kRadiusSm = 4.0F;
constexpr float kRadiusMd = 6.0F;
constexpr float kTileRadius = 5.0F;

constexpr float kBodySize = 14.0F;
constexpr float kCaptionSize = 12.0F;
constexpr float kOverlineSize = 10.0F;
constexpr float kTitleSize = 18.0F;
constexpr float kButtonSize = 16.0F;
constexpr float kMonoSize = 14.0F;

constexpr float kBodyTextOffset = 3.0F;
constexpr float kCaptionTextOffset = 2.0F;

constexpr ImU32 kText = IM_COL32(0xF5, 0xF5, 0xF5, 0xFF);
constexpr ImU32 kTextMuted = IM_COL32(0xA6, 0xA6, 0xA6, 0xFF);
constexpr ImU32 kTextSubtle = IM_COL32(0x73, 0x73, 0x73, 0xFF);
constexpr ImU32 kAccent = IM_COL32(0x00, 0x70, 0xD1, 0xFF);
constexpr ImU32 kOnAccent = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
constexpr ImU32 kDanger = IM_COL32(0xF2, 0x62, 0x5F, 0xFF);
constexpr ImU32 kDangerSubtle = IM_COL32(0x33, 0x1E, 0x1E, 0xFF);
constexpr ImU32 kStandby = IM_COL32(0xFF, 0xB5, 0x47, 0xFF);
constexpr ImU32 kSurfaceHigh = IM_COL32(0x2C, 0x2C, 0x2C, 0xFF);
constexpr ImU32 kBorderControl = IM_COL32(0x5C, 0x5C, 0x5C, 0xFF);
constexpr ImU32 kMenu = IM_COL32(0x12, 0x12, 0x12, 0xFF);
constexpr ImU32 kOverlayControl = IM_COL32(0xFF, 0xFF, 0xFF, 0x0F);
constexpr ImU32 kOverlayHover = IM_COL32(0xFF, 0xFF, 0xFF, 0x14);
constexpr ImU32 kPlotBackground = IM_COL32(0xFF, 0xFF, 0xFF, 0x0A);
constexpr ImU32 kFocus = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
constexpr ImU32 kIconTile = IM_COL32(0x00, 0x00, 0x00, 0x33);

constexpr ImGuiWindowFlags kStatsFlags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
    ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
constexpr ImGuiWindowFlags kMenuFlags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;

// Dear ImGui has process-wide state; the contexts of two stream windows on
// different threads must not interleave. Windows also dispatches messages
// synchronously from inside a handler: Dear ImGui's ReleaseCapture on a mouse
// button up sends WM_CAPTURECHANGED to the same window, which re-enters
// Overlay::handleMessage on this thread. A recursive mutex keeps that legal.
std::recursive_mutex& imGuiMutex()
{
    static std::recursive_mutex mutex;
    return mutex;
}

struct Fonts
{
    ImFont* body = nullptr;
    ImFont* label = nullptr;
    ImFont* title = nullptr;
    ImFont* mono = nullptr;
    ImFont* monoStrong = nullptr;
};

struct MenuResult
{
    bool statisticsToggled = false;
    bool reflexChanged = false;
    core::ReflexMode reflex = core::ReflexMode::Off;
    bool upscalingChanged = false;
    core::Upscaling upscaling = core::Upscaling::Shader;
    bool frameGenerationChanged = false;
    core::FrameGenerationMode frameGeneration = core::FrameGenerationMode::Off;
    bool microphoneToggled = false;
    bool windowLayoutChanged = false;
    core::WindowLayout windowLayout;
    bool disconnect = false;
};

const char* stageLabel(EffectStage stage)
{
    switch (stage)
    {
    case EffectStage::ColorConversion:
        return "Conversão de cor";
    case EffectStage::RtxVideoHdr:
        return "RTX HDR";
    case EffectStage::VideoSuperResolution:
        return "RTX VSR";
    case EffectStage::DlssSuperResolution:
        return "DLSS SR";
    case EffectStage::Cadence:
        return "Cadência";
    case EffectStage::Scaling:
        return "Composição";
    case EffectStage::Overlay:
        return "Overlay";
    case EffectStage::OpticalFlow:
        return "Fluxo óptico";
    }

    return "?";
}

const char* reflexModeLabel(core::ReflexMode mode)
{
    switch (mode)
    {
    case core::ReflexMode::Off:
        return "Desligado";
    case core::ReflexMode::On:
        return "Ligado";
    case core::ReflexMode::Boost:
        return "Ligado + Boost";
    }

    return "Desligado";
}

// The segmented control's labels, as short as the design's three columns.
const char* reflexSegmentLabel(core::ReflexMode mode)
{
    switch (mode)
    {
    case core::ReflexMode::Off:
        return "Desligado";
    case core::ReflexMode::On:
        return "Ligado";
    case core::ReflexMode::Boost:
        return "Boost";
    }

    return "Desligado";
}

std::string windowLayoutText(const core::WindowLayout& layout)
{
    if (layout.mode == core::WindowMode::Fullscreen)
    {
        return "Tela cheia";
    }

    if (core::windowSizePresetIndex(layout.size))
    {
        return std::format("{} × {}", layout.size.width, layout.size.height);
    }

    return std::format("Personalizado ({} × {})", layout.size.width, layout.size.height);
}

// Portuguese writes decimals with a comma, as the design does.
std::string millisecondsText(double milliseconds, int precision = 1)
{
    std::string text = std::format("{:.{}f}", milliseconds, precision);
    std::ranges::replace(text, '.', ',');
    text += " ms";

    return text;
}

std::string decimalText(double value, int precision = 1)
{
    std::string text = std::format("{:.{}f}", value, precision);
    std::ranges::replace(text, '.', ',');
    return text;
}

std::string resolutionText(const core::StreamMetricsSnapshot& video, std::uint32_t outputWidth,
                           std::uint32_t outputHeight)
{
    if (!video.formatValid)
    {
        return "—";
    }

    std::string text = std::format("{}×{}", video.width, video.height);

    if (outputWidth != 0 && outputHeight != 0 &&
        (outputWidth != video.width || outputHeight != video.height))
    {
        text += std::format(" → {}×{}", outputWidth, outputHeight);
    }

    return text;
}

std::string codecText(const core::StreamMetricsSnapshot& video, bool hdrStream)
{
    if (video.codec.empty())
    {
        return "—";
    }

    return video.codec + (hdrStream ? " · HDR" : " · SDR");
}

std::string hdrText(const OverlayStats& stats)
{
    if (stats.rtxVideoHdr)
    {
        return "RTX Video HDR";
    }

    if (!stats.hdrStream)
    {
        return "—";
    }

    return stats.hdrSwapChain ? "PQ → HDR10" : "PQ → SDR";
}

// Dear ImGui's 8-bit sRGB vertex color as a PQ code, keeping its alpha.
std::uint32_t pqColor(std::uint32_t color, float sdrWhiteNits)
{
    const auto code = [sdrWhiteNits](std::uint32_t channel)
    {
        const double srgb = static_cast<double>(channel) / 255.0;
        return static_cast<std::uint32_t>(
            std::lround(srgbToPq(srgb, static_cast<double>(sdrWhiteNits)) * 255.0));
    };

    const std::uint32_t alpha = color & 0xFF000000U;
    return alpha | (code((color >> 16) & 0xFFU) << 16) | (code((color >> 8) & 0xFFU) << 8) |
           code(color & 0xFFU);
}

std::string fpsText(const core::StreamMetricsSnapshot& video, double fps)
{
    return video.formatValid ? decimalText(fps) + " fps" : "—";
}

std::string cadenceText(const OverlayStats& stats)
{
    if (!stats.cadenceValid)
    {
        return "—";
    }

    return std::format("{} fps · {}/{} repetidos",
                       decimalText(stats.cadenceFramesPerSecond), stats.cadenceRepeatedFrames,
                       stats.cadenceWindowFrames);
}

float textWidth(ImFont* font, float size, std::string_view text)
{
    return font->CalcTextSizeA(size, FLT_MAX, 0.0F, text.data(), text.data() + text.size()).x;
}

void drawText(ImDrawList* drawList, ImFont* font, float size, ImVec2 position, ImU32 color,
              std::string_view text)
{
    drawList->AddText(font, size, position, color, text.data(), text.data() + text.size());
}

// The uppercase overlines carry the design's 2 px tracking.
void drawTrackedText(ImDrawList* drawList, ImFont* font, float size, ImVec2 position, ImU32 color,
                     std::string_view text, float tracking)
{
    float x = position.x;

    for (std::size_t index = 0; index < text.size();)
    {
        std::size_t next = index + 1;

        while (next < text.size() && (static_cast<unsigned char>(text.at(next)) & 0xC0U) == 0x80U)
        {
            ++next;
        }

        const std::string_view glyph = text.substr(index, next - index);
        drawList->AddText(font, size, ImVec2(x, position.y), color, glyph.data(),
                          glyph.data() + glyph.size());
        x += textWidth(font, size, glyph) + tracking;
        index = next;
    }
}

// Breaks a caption on its spaces so it fits the row; a word wider than the
// row stays on a line of its own.
std::vector<std::string> wrapText(ImFont* font, float size, std::string_view text, float width)
{
    std::vector<std::string> lines;
    std::string line;
    std::size_t start = 0;

    while (start < text.size())
    {
        std::size_t end = text.find(' ', start);

        if (end == std::string_view::npos)
        {
            end = text.size();
        }

        const std::string_view word = text.substr(start, end - start);
        const std::string candidate =
            line.empty() ? std::string(word) : line + " " + std::string(word);

        if (line.empty() || textWidth(font, size, candidate) <= width)
        {
            line = candidate;
        }
        else
        {
            lines.push_back(line);
            line = std::string(word);
        }

        start = end + 1;
    }

    if (!line.empty())
    {
        lines.push_back(line);
    }

    if (lines.empty())
    {
        lines.emplace_back();
    }

    return lines;
}

void applyStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(kPanelPadding, kPanelPadding);
    style.WindowRounding = kRadiusMd;
    style.WindowBorderSize = 0.0F;
    style.ItemSpacing = ImVec2(0.0F, kGap);
    style.FramePadding = ImVec2(12.0F, 8.0F);
    style.FrameRounding = kRadiusMd;

    // #121212 at 90%: the stream stays visible under the panels.
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0706F, 0.0706F, 0.0706F, 0.9020F);
    style.Colors[ImGuiCol_Border] = ImVec4(1.0F, 1.0F, 1.0F, 0.1216F);
    style.Colors[ImGuiCol_Text] = ImVec4(0.9608F, 0.9608F, 0.9608F, 1.0F);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.3020F, 0.3020F, 0.3020F, 1.0F);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(1.0F, 1.0F, 1.0F, 0.0588F);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0F, 1.0F, 1.0F, 0.0784F);
    style.Colors[ImGuiCol_Button] = ImVec4(0.1725F, 0.1725F, 0.1725F, 1.0F);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.2000F, 0.2000F, 0.2000F, 1.0F);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.2314F, 0.2314F, 0.2314F, 1.0F);
    style.Colors[ImGuiCol_NavCursor] = ImVec4(1.0F, 1.0F, 1.0F, 1.0F);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.0F, 0.4392F, 0.8196F, 1.0F);
}

void sectionHeading(const Fonts& fonts, std::string_view label)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;

    drawTrackedText(ImGui::GetWindowDrawList(), fonts.monoStrong, kOverlineSize,
                    ImVec2(start.x, start.y + 1.0F), kTextSubtle, label, kOverlineTracking);
    ImGui::Dummy(ImVec2(width, kOverlineHeight));
}

void statRow(const Fonts& fonts, std::string_view label, const std::string& value,
             ImU32 valueColor = kText)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImDrawList* const drawList = ImGui::GetWindowDrawList();

    drawText(drawList, fonts.body, kBodySize, ImVec2(start.x, start.y + kBodyTextOffset),
             kTextMuted, label);

    const float labelWidth = textWidth(fonts.body, kBodySize, label);
    const float valueWidth = textWidth(fonts.mono, kMonoSize, value);
    const bool sameLine = labelWidth + 12.0F + valueWidth <= width;

    if (sameLine)
    {
        drawText(drawList, fonts.mono, kMonoSize,
                 ImVec2(start.x + width - valueWidth, start.y + kBodyTextOffset), valueColor,
                 value);
        ImGui::Dummy(ImVec2(width, kRowHeight));
        return;
    }

    // A value the row can't hold goes under its label, right-aligned.
    const std::vector<std::string> lines = wrapText(fonts.mono, kMonoSize, value, width);

    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        const std::string& line = lines.at(index);
        const float lineWidth = textWidth(fonts.mono, kMonoSize, line);

        drawText(drawList, fonts.mono, kMonoSize,
                 ImVec2(start.x + width - lineWidth,
                        start.y + kRowHeight * static_cast<float>(index + 1) + kBodyTextOffset),
                 valueColor, line);
    }

    ImGui::Dummy(ImVec2(width, kRowHeight * static_cast<float>(lines.size() + 1)));
}

void statisticsHeader(const Fonts& fonts)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImDrawList* const drawList = ImGui::GetWindowDrawList();

    drawText(drawList, fonts.label, kBodySize, ImVec2(start.x, start.y + kBodyTextOffset), kText,
             "Estatísticas");

    const float shortcutWidth = textWidth(fonts.label, kCaptionSize, "F2");
    drawText(drawList, fonts.label, kCaptionSize,
             ImVec2(start.x + width - shortcutWidth, start.y + kCaptionTextOffset), kTextMuted,
             "F2");

    ImGui::Dummy(ImVec2(width, 24.0F));
}

void drawVideoSection(const Fonts& fonts, const OverlayStats& stats)
{
    const core::StreamMetricsSnapshot& video = stats.video;

    sectionHeading(fonts, "VÍDEO");

    const std::string resolution = resolutionText(video, stats.outputWidth, stats.outputHeight);
    const std::string codec = codecText(video, stats.hdrStream);
    const std::string bitrate =
        video.bitrateMbps > 0.0 ? decimalText(video.bitrateMbps) + " Mbps" : "—";
    const std::string loss =
        video.packetLossValid ? decimalText(video.packetLossPercent) + " %" : "—";

    statRow(fonts, "Resolução", resolution);
    statRow(fonts, "Codec", codec);
    statRow(fonts, "HDR", hdrText(stats));
    statRow(fonts, "Fonte", fpsText(video, video.sourceFps));
    statRow(fonts, "Jogo", fpsText(video, video.gameFps));
    statRow(fonts, "Gerados", fpsText(video, video.generatedFps),
            video.generatedFps > 0.0 ? kAccent : kText);
    statRow(fonts, "Exibidos", fpsText(video, video.displayedFps));
    statRow(fonts, "Cadência", cadenceText(stats));
    statRow(fonts, "Bitrate", bitrate);
    statRow(fonts, "Perda de pacotes", loss);
}

void drawLatencySection(const Fonts& fonts, const OverlayStats& stats)
{
    const core::StreamMetricsSnapshot& video = stats.video;

    sectionHeading(fonts, "LATÊNCIA");

    const std::string present =
        video.presentIntervalValid ? millisecondsText(video.presentIntervalMilliseconds) : "—";

    statRow(fonts, "Decodificação",
            video.decodeValid ? millisecondsText(video.decodeMilliseconds) : "—");
    statRow(fonts, "Apresentação", present);
    statRow(fonts, "Quadro até a tela",
            video.readyToPresentValid ? millisecondsText(video.readyToPresentMilliseconds) : "—");
}

void drawGpuSection(const Fonts& fonts, const OverlayStats& stats)
{
    sectionHeading(fonts, "GPU POR ETAPA");

    for (std::size_t index = 0; index < kEffectStageCount; ++index)
    {
        const auto stage = static_cast<EffectStage>(index);
        std::string value = "—";
        ImU32 color = kText;

        if (stage == EffectStage::OpticalFlow && stats.opticalFlow != StageOverlayState::Running)
        {
            value = stats.opticalFlow == StageOverlayState::Off ? "desligado" : "indisponível";
            color = kTextSubtle;
        }
        else if (stage == EffectStage::DlssSuperResolution &&
                 stats.dlss != StageOverlayState::Running)
        {
            value = stats.dlss == StageOverlayState::Off ? "desligado" : "indisponível";
            color = kTextSubtle;
        }
        else if (stats.gpuTimingsValid)
        {
            value = millisecondsText(stats.stageMilliseconds.at(index), 2);
        }

        statRow(fonts, stageLabel(stage), value, color);
    }

    statRow(fonts, "Frame Generation",
            stats.frameGeneration == core::FrameGenerationMode::X2 ? "Ligado 2x" : "Desligado",
            stats.frameGeneration == core::FrameGenerationMode::X2 ? kAccent : kTextSubtle);
    statRow(fonts, "Total",
            stats.gpuTimingsValid ? millisecondsText(stats.totalMilliseconds, 2) : "—");
}

void drawFrameTime(const Fonts& fonts, const OverlayStats& stats)
{
    const core::StreamMetricsSnapshot& video = stats.video;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImDrawList* const drawList = ImGui::GetWindowDrawList();

    drawText(drawList, fonts.label, kCaptionSize,
             ImVec2(start.x, start.y + kCaptionTextOffset), kTextMuted, "Tempo de quadro");

    const std::string value = video.presentIntervalValid
                                   ? millisecondsText(video.presentIntervalMilliseconds)
                                   : "—";
    const float valueWidth = textWidth(fonts.label, kCaptionSize, value);
    drawText(drawList, fonts.label, kCaptionSize,
             ImVec2(start.x + width - valueWidth, start.y + kCaptionTextOffset), kTextMuted, value);

    constexpr float kPlotHeight = 36.0F;
    const ImVec2 plotTop(start.x, start.y + 20.0F);
    const ImVec2 plotBottom(plotTop.x + width, plotTop.y + kPlotHeight);
    drawList->AddRectFilled(plotTop, plotBottom, kPlotBackground, kRadiusSm);

    if (video.frameTimeCount >= 2)
    {
        float maximum = 33.3F;

        for (std::size_t index = 0; index < video.frameTimeCount; ++index)
        {
            maximum = std::max(maximum, video.frameTimes.at(index) * 1.2F);
        }

        const float innerWidth = width - 8.0F;
        const float innerHeight = kPlotHeight - 8.0F;
        const float step = innerWidth / static_cast<float>(video.frameTimeCount - 1);

        std::vector<ImVec2> points;
        points.reserve(video.frameTimeCount);

        for (std::size_t index = 0; index < video.frameTimeCount; ++index)
        {
            const float fraction =
                std::clamp(video.frameTimes.at(index) / maximum, 0.0F, 1.0F);
            points.emplace_back(plotTop.x + 4.0F + step * static_cast<float>(index),
                                plotBottom.y - 4.0F - fraction * innerHeight);
        }

        drawList->AddPolyline(points.data(), static_cast<int>(points.size()), kAccent, 0, 1.5F);
    }

    ImGui::Dummy(ImVec2(width, 56.0F));
}

void drawReflexSection(const Fonts& fonts, const ReflexOverlayStats& reflex)
{
    sectionHeading(fonts, "NVIDIA REFLEX");

    switch (reflex.state)
    {
    case ReflexOverlayStats::State::Unavailable:
        statRow(fonts, "Disponibilidade",
                reflex.reason.empty() ? std::string("indisponível") : reflex.reason);
        return;

    case ReflexOverlayStats::State::Waiting:
        statRow(fonts, "Modo", reflex.mode);
        statRow(fonts, "Latência total", "aguardando dados…");
        return;

    case ReflexOverlayStats::State::Active:
    case ReflexOverlayStats::State::Stalled:
    case ReflexOverlayStats::State::Failed:
        break;
    }

    statRow(fonts, "Modo", reflex.mode);
    statRow(fonts, "Low latency efetivo", reflex.lowLatencyEffective ? "sim" : "não");
    statRow(fonts, "Sleep médio", millisecondsText(reflex.sleepMeanMs, 3));
    statRow(fonts, "Sleep máximo", millisecondsText(reflex.sleepMaxMs, 3));
    statRow(fonts, "Driver vê Sleep", reflex.driverSeesSleep ? "sim" : "não");
    statRow(fonts, "Latência total",
            reflex.latencyValid ? millisecondsText(reflex.latencyTotalMs) : "—");
    statRow(fonts, "GPU", reflex.latencyValid ? millisecondsText(reflex.latencyGpuMs) : "—");
    statRow(fonts, "Render",
            reflex.latencyValid ? millisecondsText(reflex.latencyRenderMs) : "—");
    statRow(fonts, "Apresentação",
            reflex.latencyValid ? millisecondsText(reflex.latencyPresentMs) : "—");
    statRow(fonts, "Driver",
            reflex.latencyValid ? millisecondsText(reflex.latencyDriverMs) : "—");
    statRow(fonts, "Fila do SO",
            reflex.latencyValid ? millisecondsText(reflex.latencyQueueMs) : "—");

    if (reflex.state == ReflexOverlayStats::State::Stalled)
    {
        statRow(fonts, "Aviso", "sleep alto: desligado nesta transmissão", kStandby);
    }

    if (reflex.state == ReflexOverlayStats::State::Failed)
    {
        statRow(fonts, "Aviso", "o sleep do driver falhou", kDanger);
    }
}

std::string touchText(const OverlayGamepad::Touch& touch)
{
    if (!touch.down)
    {
        return "—";
    }

    return std::format("({}, {})", touch.x, touch.y);
}

// The line the statistics write to the log once a second, so a controller
// problem can be reported with the exact values the console receives.
std::string controllerSummary(const OverlayGamepad& gamepad)
{
    if (!gamepad.connected)
    {
        return "controller: none";
    }

    return std::format(
        "controller: buttons={} L2={} R2={} touch1={} touch2={} "
        "gyro=({:.2f}, {:.2f}, {:.2f}) accel=({:.2f}, {:.2f}, {:.2f})",
        gamepad.buttons.empty() ? "none" : gamepad.buttons, gamepad.leftTrigger,
        gamepad.rightTrigger, touchText(gamepad.touches[0]), touchText(gamepad.touches[1]),
        gamepad.gyro[0], gamepad.gyro[1], gamepad.gyro[2], gamepad.accel[0], gamepad.accel[1],
        gamepad.accel[2]);
}

void drawControllerSection(const Fonts& fonts, const OverlayGamepad& gamepad)
{
    sectionHeading(fonts, "CONTROLE");

    if (!gamepad.connected)
    {
        statRow(fonts, "Estado", "sem controle");
        return;
    }

    statRow(fonts, "Botões", gamepad.buttons.empty() ? "—" : gamepad.buttons);
    statRow(fonts, "Gatilhos",
            std::format("L2 {} · R2 {}", gamepad.leftTrigger, gamepad.rightTrigger));
    statRow(fonts, "Toque 1", touchText(gamepad.touches[0]));
    statRow(fonts, "Toque 2", touchText(gamepad.touches[1]));
    statRow(fonts, "Giroscópio", std::format("{:.2f} {:.2f} {:.2f} rad/s", gamepad.gyro[0],
                                              gamepad.gyro[1], gamepad.gyro[2]));
    statRow(fonts, "Acelerômetro", std::format("{:.2f} {:.2f} {:.2f} g", gamepad.accel[0],
                                                gamepad.accel[1], gamepad.accel[2]));
}

void drawStatistics(const Fonts& fonts, const OverlayStats& stats, const OverlayGamepad& gamepad)
{
    ImGui::SetNextWindowPos(ImVec2(kPanelMargin, kPanelMargin), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kStatsWidth, 0.0F), ImGuiCond_Always);

    if (!ImGui::Begin("mfplay-statistics", nullptr, kStatsFlags))
    {
        ImGui::End();
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0F, kGap));

    statisticsHeader(fonts);
    drawVideoSection(fonts, stats);
    drawLatencySection(fonts, stats);
    drawGpuSection(fonts, stats);
    drawFrameTime(fonts, stats);
    drawReflexSection(fonts, stats.reflex);
    drawControllerSection(fonts, gamepad);

    ImGui::PopStyleVar();
    ImGui::End();
}

void drawSwitch(ImDrawList* drawList, ImVec2 topLeft, bool on)
{
    constexpr ImVec2 kTrackSize(48.0F, 28.0F);
    constexpr float kThumbRadius = 11.0F;
    constexpr float kInset = 3.0F;

    const ImVec2 bottomRight(topLeft.x + kTrackSize.x, topLeft.y + kTrackSize.y);
    drawList->AddRectFilled(topLeft, bottomRight, on ? kAccent : kSurfaceHigh, kTrackSize.y / 2.0F);

    if (!on)
    {
        drawList->AddRect(topLeft, bottomRight, kBorderControl, kTrackSize.y / 2.0F, 0, 1.0F);
    }

    const float thumbX =
        topLeft.x + (on ? kTrackSize.x - kInset - 2.0F * kThumbRadius : kInset) + kThumbRadius;
    drawList->AddCircleFilled(ImVec2(thumbX, topLeft.y + kTrackSize.y / 2.0F), kThumbRadius,
                               on ? kOnAccent : kTextMuted);
}

void drawControlBlock(ImDrawList* drawList, ImVec2 start, ImVec2 size, bool hovered, bool focused)
{
    const ImVec2 bottomRight(start.x + size.x, start.y + size.y);
    drawList->AddRectFilled(start, bottomRight, kOverlayControl, kRadiusMd);

    if (hovered)
    {
        drawList->AddRectFilled(start, bottomRight, kOverlayHover, kRadiusMd);
    }

    if (focused)
    {
        drawList->AddRect(start, bottomRight, kFocus, kRadiusMd, 0, 2.0F);
    }
}

bool toggleRow(const char* id, const Fonts& fonts, std::string_view label, std::string_view detail,
               bool on)
{
    constexpr float kBlockPaddingY = 8.0F;
    constexpr float kBlockPaddingX = 12.0F;
    constexpr float kToggleWidth = 48.0F;
    constexpr float kToggleHeight = 28.0F;
    constexpr float kToggleGap = 12.0F;
    constexpr float kDetailGap = 2.0F;

    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float detailWidth = width - 2.0F * kBlockPaddingX - kToggleWidth - kToggleGap;
    const std::vector<std::string> detailLines =
        detail.empty() ? std::vector<std::string>{}
                       : wrapText(fonts.label, kCaptionSize, detail, detailWidth);
    const float textHeight =
        kRowHeight + (detailLines.empty()
                          ? 0.0F
                          : kDetailGap + kCaptionHeight * static_cast<float>(detailLines.size()));
    const float height = 2.0F * kBlockPaddingY + std::max(textHeight, kToggleHeight);

    ImGui::PushID(id);
    ImGui::SetCursorScreenPos(start);
    const bool activated = ImGui::InvisibleButton("row", ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    const bool focused = ImGui::IsItemFocused();
    ImGui::PopID();

    ImDrawList* const drawList = ImGui::GetWindowDrawList();
    drawControlBlock(drawList, start, ImVec2(width, height), hovered, focused);

    const float contentHeight = height - 2.0F * kBlockPaddingY;
    const float textTop = start.y + kBlockPaddingY + (contentHeight - textHeight) / 2.0F;
    drawText(drawList, fonts.label, kBodySize, ImVec2(start.x + kBlockPaddingX, textTop + 3.0F),
             kText, label);

    float detailY = textTop + kRowHeight + kDetailGap;

    for (const std::string& line : detailLines)
    {
        drawText(drawList, fonts.label, kCaptionSize,
                 ImVec2(start.x + kBlockPaddingX, detailY + kCaptionTextOffset), kTextMuted, line);
        detailY += kCaptionHeight;
    }

    drawSwitch(drawList,
               ImVec2(start.x + width - kBlockPaddingX - kToggleWidth,
                      start.y + (height - kToggleHeight) / 2.0F),
               on);

    return activated;
}

// A row whose value cycles through choices when activated.
bool cycleRow(const char* id, const Fonts& fonts, std::string_view label, std::string_view detail,
              std::string_view value)
{
    constexpr float kBlockPaddingY = 8.0F;
    constexpr float kBlockPaddingX = 12.0F;
    constexpr float kDetailGap = 2.0F;

    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const std::vector<std::string> detailLines =
        wrapText(fonts.label, kCaptionSize, detail, width - 2.0F * kBlockPaddingX);
    const float textHeight =
        kRowHeight + kDetailGap + kCaptionHeight * static_cast<float>(detailLines.size());
    const float height = 2.0F * kBlockPaddingY + std::max(textHeight, 28.0F);

    ImGui::PushID(id);
    ImGui::SetCursorScreenPos(start);
    const bool activated = ImGui::InvisibleButton("row", ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    const bool focused = ImGui::IsItemFocused();
    ImGui::PopID();

    ImDrawList* const drawList = ImGui::GetWindowDrawList();
    drawControlBlock(drawList, start, ImVec2(width, height), hovered, focused);

    const float labelY = start.y + kBlockPaddingY + 3.0F;
    drawText(drawList, fonts.label, kBodySize, ImVec2(start.x + kBlockPaddingX, labelY), kText,
             label);

    const float valueWidth = textWidth(fonts.label, kBodySize, value);
    drawText(drawList, fonts.label, kBodySize,
             ImVec2(start.x + width - kBlockPaddingX - valueWidth, labelY), kAccent, value);

    float detailY = start.y + kBlockPaddingY + kRowHeight + kDetailGap;

    for (const std::string& line : detailLines)
    {
        drawText(drawList, fonts.label, kCaptionSize,
                 ImVec2(start.x + kBlockPaddingX, detailY + kCaptionTextOffset), kTextMuted, line);
        detailY += kCaptionHeight;
    }

    return activated;
}

bool reflexBlock(const Fonts& fonts, const ReflexOverlayStats& reflex, core::ReflexMode current,
                 core::ReflexMode* chosen)
{
    constexpr std::array<core::ReflexMode, 3> kModes{
        core::ReflexMode::Off,
        core::ReflexMode::On,
        core::ReflexMode::Boost,
    };
    constexpr float kBlockPadding = 12.0F;
    constexpr float kLabelGap = 10.0F;
    constexpr float kOuterPadding = 4.0F;
    constexpr float kSegmentGap = 4.0F;
    constexpr float kSegmentHeight = 36.0F;

    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = 2.0F * kBlockPadding + kRowHeight + kLabelGap +
                         2.0F * kOuterPadding + kSegmentHeight;

    ImDrawList* const drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(start, ImVec2(start.x + width, start.y + height), kOverlayControl,
                             kRadiusMd);

    drawText(drawList, fonts.label, kBodySize,
             ImVec2(start.x + kBlockPadding, start.y + kBlockPadding + 3.0F), kText,
             "NVIDIA Reflex");

    const ImVec2 containerTop(start.x + kBlockPadding,
                               start.y + kBlockPadding + kRowHeight + kLabelGap);
    const ImVec2 containerBottom(containerTop.x + width - 2.0F * kBlockPadding,
                                  containerTop.y + 2.0F * kOuterPadding + kSegmentHeight);
    drawList->AddRectFilled(containerTop, containerBottom, kMenu, kRadiusMd);

    const float segmentWidth = (containerBottom.x - containerTop.x - 2.0F * kOuterPadding -
                                2.0F * kSegmentGap) /
                                static_cast<float>(kModes.size());
    const bool available = reflex.state != ReflexOverlayStats::State::Unavailable;
    bool clicked = false;
    float x = containerTop.x + kOuterPadding;

    for (std::size_t index = 0; index < kModes.size(); ++index)
    {
        const core::ReflexMode mode = kModes.at(index);
        const bool selected = mode == current;
        const ImVec2 segmentTop(x, containerTop.y + kOuterPadding);
        const ImVec2 segmentBottom(segmentTop.x + segmentWidth, segmentTop.y + kSegmentHeight);

        drawList->AddRectFilled(segmentTop, segmentBottom,
                                 selected ? kAccent : IM_COL32(0, 0, 0, 0), kRadiusSm);

        ImGui::SetCursorScreenPos(segmentTop);
        ImGui::PushID(static_cast<int>(index));
        const bool activated =
            ImGui::InvisibleButton("segment", ImVec2(segmentWidth, kSegmentHeight));
        const bool hovered = ImGui::IsItemHovered();
        const bool focused = ImGui::IsItemFocused();
        ImGui::PopID();

        if (hovered && !selected)
        {
            drawList->AddRectFilled(segmentTop, segmentBottom, kOverlayHover, kRadiusSm);
        }

        if (focused)
        {
            drawList->AddRect(segmentTop, segmentBottom, kFocus, kRadiusSm, 0, 2.0F);
        }

        const char* const label = reflexSegmentLabel(mode);
        const float labelWidth = textWidth(fonts.label, kBodySize, label);
        const ImU32 color = !available ? kTextSubtle : (selected ? kOnAccent : kTextMuted);

        drawText(drawList, fonts.label, kBodySize,
                 ImVec2(segmentTop.x + (segmentWidth - labelWidth) / 2.0F,
                        segmentTop.y + (kSegmentHeight - kBodySize) / 2.0F + 2.0F),
                 color, label);

        if (activated)
        {
            *chosen = mode;
            clicked = true;
        }

        x += segmentWidth + kSegmentGap;
    }

    ImGui::SetCursorScreenPos(start);
    ImGui::Dummy(ImVec2(width, height));
    return clicked;
}

void drawPowerIcon(ImDrawList* drawList, ImVec2 center, float radius, ImU32 color)
{
    constexpr float kPi = 3.14159265F;
    constexpr float kArcGap = 0.55F;
    constexpr float kStroke = 1.8F;

    drawList->PathArcTo(center, radius, -kPi / 2.0F + kArcGap,
                         -kPi / 2.0F + 2.0F * kPi - kArcGap, 24);
    drawList->PathStroke(color, 0, kStroke);
    drawList->AddLine(ImVec2(center.x, center.y - radius - 1.5F),
                       ImVec2(center.x, center.y - 1.0F), color, kStroke);
}

bool disconnectButton(const Fonts& fonts)
{
    constexpr float kHeight = 52.0F;
    constexpr float kTileSize = 32.0F;
    constexpr float kTileGap = 12.0F;

    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;

    ImGui::PushID("disconnect");
    const bool activated = ImGui::InvisibleButton("button", ImVec2(width, kHeight));
    const bool hovered = ImGui::IsItemHovered();
    const bool focused = ImGui::IsItemFocused();
    ImGui::PopID();

    ImDrawList* const drawList = ImGui::GetWindowDrawList();
    const ImVec2 bottomRight(start.x + width, start.y + kHeight);
    drawList->AddRectFilled(start, bottomRight, kDangerSubtle, kRadiusMd);

    if (hovered)
    {
        drawList->AddRectFilled(start, bottomRight, kOverlayHover, kRadiusMd);
    }

    if (focused)
    {
        drawList->AddRect(start, bottomRight, kFocus, kRadiusMd, 0, 2.0F);
    }

    constexpr std::string_view kLabel = "Desconectar";
    const float labelWidth = textWidth(fonts.title, kButtonSize, kLabel);
    const float contentWidth = kTileSize + kTileGap + labelWidth;
    const ImVec2 tileTop(start.x + (width - contentWidth) / 2.0F,
                          start.y + (kHeight - kTileSize) / 2.0F);

    drawList->AddRectFilled(tileTop, ImVec2(tileTop.x + kTileSize, tileTop.y + kTileSize),
                             kIconTile, kTileRadius);
    drawPowerIcon(drawList, ImVec2(tileTop.x + kTileSize / 2.0F, tileTop.y + kTileSize / 2.0F),
                  6.5F, kDanger);
    drawText(drawList, fonts.title, kButtonSize,
             ImVec2(tileTop.x + kTileSize + kTileGap,
                    start.y + (kHeight - kButtonSize) / 2.0F - 1.0F),
             kDanger, kLabel);

    return activated;
}

void menuHeader(const Fonts& fonts, const OverlayOptions& options)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImDrawList* const drawList = ImGui::GetWindowDrawList();

    drawText(drawList, fonts.title, kTitleSize, ImVec2(start.x + 4.0F, start.y + 5.0F), kText,
             "Menu rápido");

    if (!options.sessionName.empty())
    {
        const long long seconds = static_cast<long long>(ImGui::GetTime());
        const long long minutes = seconds / 60;
        const std::string session =
            minutes > 0
                ? std::format("{} · jogando há {} min", options.sessionName, minutes)
                : std::format("{} · jogando agora", options.sessionName);

        drawText(drawList, fonts.label, kCaptionSize, ImVec2(start.x + 4.0F, start.y + 31.0F),
                 kTextMuted, session);
    }

    ImGui::Dummy(ImVec2(width, 52.0F));
}

void menuFooter(const Fonts& fonts)
{
    constexpr std::string_view kHint = "Fechar: F1 ou segure Create + Options";

    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float hintWidth = textWidth(fonts.label, kCaptionSize, kHint);

    drawText(ImGui::GetWindowDrawList(), fonts.label, kCaptionSize,
             ImVec2(start.x + (width - hintWidth) / 2.0F, start.y + kCaptionTextOffset), kTextMuted,
             kHint);
    ImGui::Dummy(ImVec2(width, kCaptionHeight));
}

MenuResult drawMenu(const Fonts& fonts, const OverlayStats& stats, const OverlayOptions& options,
                    core::ReflexMode currentReflex, bool microphoneMuted, bool statsVisible,
                    core::WindowLayout windowLayout)
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float height = std::min(kMenuHeight, std::max(320.0F, display.y - 2.0F * kPanelMargin));

    ImGui::SetNextWindowPos(ImVec2(display.x - kMenuWidth - kPanelMargin, kPanelMargin),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kMenuWidth, height), ImGuiCond_Always);

    MenuResult result;
    result.reflex = currentReflex;

    if (!ImGui::Begin("mfplay-menu", nullptr, kMenuFlags))
    {
        ImGui::End();
        return result;
    }

    menuHeader(fonts, options);

    core::ReflexMode chosen = currentReflex;

    if (reflexBlock(fonts, stats.reflex, currentReflex, &chosen))
    {
        result.reflexChanged = true;
        result.reflex = chosen;
        spdlog::info("overlay: reflex segment chose {} (state {})", static_cast<int>(chosen),
                     static_cast<int>(stats.reflex.state));
    }

    const bool vsrOn = stats.upscaling == core::Upscaling::Vsr;

    if (toggleRow("vsr-row", fonts, "RTX Video Super Resolution",
                  "Limpa artefatos e amplia para a tela.", vsrOn))
    {
        result.upscalingChanged = true;
        result.upscaling = vsrOn ? core::Upscaling::Shader : core::Upscaling::Vsr;
        spdlog::info("overlay: upscaling {}", core::upscalingName(result.upscaling));
    }

    const bool frameGenerationOn = stats.frameGeneration == core::FrameGenerationMode::X2;

    if (toggleRow("frame-generation-row", fonts, "DLSS Frame Generation 2x",
                  "Dobra o fps; adiciona cerca de 1 quadro de latência.", frameGenerationOn))
    {
        result.frameGenerationChanged = true;
        result.frameGeneration =
            frameGenerationOn ? core::FrameGenerationMode::Off : core::FrameGenerationMode::X2;
        spdlog::info("overlay: frame generation {}",
                     core::frameGenerationName(result.frameGeneration));
    }

    if (toggleRow("statistics-row", fonts, "Mostrar estatísticas", std::string_view{},
                  statsVisible))
    {
        result.statisticsToggled = true;
    }

    if (toggleRow("microphone-row", fonts, "Microfone", options.microphoneDetail,
                  !microphoneMuted))
    {
        result.microphoneToggled = true;
    }

    if (cycleRow("window-row", fonts, "Janela", "Clique para alternar tela cheia e tamanhos",
                 windowLayoutText(windowLayout)))
    {
        result.windowLayoutChanged = true;
        result.windowLayout = core::nextWindowLayout(windowLayout);
    }

    constexpr float kActionsPadding = 8.0F;
    constexpr float kActionHeight = 52.0F;
    const float remaining = ImGui::GetContentRegionAvail().y;
    ImGui::Dummy(ImVec2(0.0F, std::max(8.0F, remaining - 3.0F * kGap - kActionsPadding -
                                                   kActionHeight - kCaptionHeight)));
    ImGui::Dummy(ImVec2(0.0F, kActionsPadding));

    if (disconnectButton(fonts))
    {
        result.disconnect = true;
    }

    menuFooter(fonts);

    ImGui::End();
    return result;
}

} // namespace

std::expected<std::unique_ptr<Overlay>, std::string> Overlay::create(
    ID3D12Device& device, ID3D12CommandQueue& queue, HWND window, UINT bufferCount,
    DXGI_FORMAT format, OverlayFonts fonts, OverlayOptions options, OverlayActions actions)
{
    std::unique_ptr<Overlay> overlay(new Overlay);
    overlay->device_ = &device;
    overlay->queue_ = &queue;
    overlay->bufferCount_ = bufferCount;
    overlay->format_ = format;
    overlay->hdrOutput_ = options.hdrOutput;
    overlay->sdrWhiteNits_ = options.sdrWhiteNits;
    overlay->options_ = std::move(options);
    overlay->actions_ = std::move(actions);
    overlay->reflex_ = overlay->options_.reflex;
    overlay->microphoneMuted_ = overlay->options_.microphoneMuted;
    overlay->windowMode_.store(overlay->options_.windowMode);
    overlay->windowSize_.store(overlay->options_.windowSize);
    overlay->statisticsVisible_.store(overlay->options_.statisticsVisible);
    overlay->reportedStatisticsVisible_ = overlay->options_.statisticsVisible;

    const std::lock_guard lock(imGuiMutex());
    overlay->context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(overlay->context_);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

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

    if (!overlay->createDx12Backend())
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(std::exchange(overlay->context_, nullptr));
        return std::unexpected("ImGui_ImplDX12_Init failed");
    }

    overlay->loadFonts(fonts);
    applyStyle();
    return overlay;
}

bool Overlay::createDx12Backend()
{
    ImGui_ImplDX12_InitInfo info;
    info.Device = device_;
    info.CommandQueue = queue_;
    info.NumFramesInFlight = static_cast<int>(bufferCount_);
    info.RTVFormat = format_;
    info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    info.UserData = this;
    info.SrvDescriptorHeap = viewHeap_.Get();
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

    return ImGui_ImplDX12_Init(&info);
}

void Overlay::setOutputFormat(DXGI_FORMAT format, bool hdrOutput, float sdrWhiteNits)
{
    if (context_ == nullptr)
    {
        return;
    }

    if (format == format_ && hdrOutput == hdrOutput_ && sdrWhiteNits == sdrWhiteNits_)
    {
        return;
    }

    const std::lock_guard lock(imGuiMutex());
    ImGui::SetCurrentContext(context_);
    format_ = format;
    hdrOutput_ = hdrOutput;
    sdrWhiteNits_ = sdrWhiteNits;
    ImGui_ImplDX12_Shutdown();

    if (!createDx12Backend())
    {
        spdlog::error("The overlay could not switch to output format {}", static_cast<int>(format));
    }
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

void Overlay::loadFonts(const OverlayFonts& fonts)
{
    fontData_ = fonts;
    ImGuiIO& io = ImGui::GetIO();

    const auto add = [&io](const std::shared_ptr<const std::vector<std::uint8_t>>& data, float size)
        -> ImFont*
    {
        if (!data || data->empty())
        {
            return nullptr;
        }

        ImFontConfig config;
        config.FontDataOwnedByAtlas = false;

        return io.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(data->data()),
                                               static_cast<int>(data->size()), size, &config);
    };

    bodyFont_ = add(fontData_.body, kBodySize);
    labelFont_ = add(fontData_.label, kBodySize);
    titleFont_ = add(fontData_.title, kTitleSize);
    monoFont_ = add(fontData_.mono, kMonoSize);
    monoStrongFont_ = add(fontData_.monoStrong, kOverlineSize);

    if (bodyFont_ == nullptr)
    {
        bodyFont_ = io.Fonts->AddFontDefault();
    }

    if (labelFont_ == nullptr)
    {
        labelFont_ = bodyFont_;
    }

    if (titleFont_ == nullptr)
    {
        titleFont_ = bodyFont_;
    }

    if (monoFont_ == nullptr)
    {
        monoFont_ = bodyFont_;
    }

    if (monoStrongFont_ == nullptr)
    {
        monoStrongFont_ = monoFont_;
    }

    io.FontDefault = bodyFont_;
}

void Overlay::render(ID3D12GraphicsCommandList& commandList, D3D12_CPU_DESCRIPTOR_HANDLE target,
                     const OverlayStats& stats, const OverlayGamepad& gamepad)
{
    if (context_ == nullptr)
    {
        return;
    }

    const std::lock_guard lock(imGuiMutex());
    ImGui::SetCurrentContext(context_);
    sendGamepad(gamepad);
    reportState();

    const Fonts fonts{
        .body = bodyFont_,
        .label = labelFont_,
        .title = titleFont_,
        .mono = monoFont_,
        .monoStrong = monoStrongFont_,
    };

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    const bool menuOpen = menuOpen_.load();
    const bool statisticsVisible = statisticsVisible_.load();

    if (statisticsVisible)
    {
        drawStatistics(fonts, stats, gamepad);

        const auto now = std::chrono::steady_clock::now();

        if (now >= nextControllerLog_)
        {
            spdlog::info("{}", controllerSummary(gamepad));
            nextControllerLog_ = now + std::chrono::seconds(1);
        }
    }

    if (menuOpen)
    {
        const core::WindowLayout windowLayout{
            .mode = windowMode_.load(),
            .size = windowSize_.load(),
        };
        const MenuResult menu = drawMenu(fonts, stats, options_, reflex_, microphoneMuted_,
                                         statisticsVisible, windowLayout);

        if (menu.statisticsToggled)
        {
            spdlog::info("overlay: statistics toggle {}", !statisticsVisible);
            statisticsVisible_.store(!statisticsVisible);
        }

        if (menu.reflexChanged)
        {
            reflex_ = menu.reflex;
            spdlog::info("overlay: applying reflex {}", static_cast<int>(reflex_));

            if (actions_.reflexChanged)
            {
                actions_.reflexChanged(reflex_);
            }
            else
            {
                spdlog::warn("overlay: no reflex action");
            }
        }

        if (menu.upscalingChanged && actions_.upscalingChanged)
        {
            actions_.upscalingChanged(menu.upscaling);
        }

        if (menu.frameGenerationChanged && actions_.frameGenerationChanged)
        {
            actions_.frameGenerationChanged(menu.frameGeneration);
        }

        if (menu.windowLayoutChanged)
        {
            windowMode_.store(menu.windowLayout.mode);
            windowSize_.store(menu.windowLayout.size);
            spdlog::info("overlay: window layout mode {} at {}x{}",
                         core::windowModeName(menu.windowLayout.mode),
                         menu.windowLayout.size.width, menu.windowLayout.size.height);

            if (actions_.windowLayoutChanged)
            {
                actions_.windowLayoutChanged(menu.windowLayout.mode, menu.windowLayout.size);
            }
            else
            {
                spdlog::warn("overlay: no window layout action");
            }
        }

        if (menu.microphoneToggled)
        {
            microphoneMuted_ = !microphoneMuted_;
            spdlog::info("overlay: microphone {}", microphoneMuted_ ? "muted" : "unmuted");

            if (actions_.microphoneMutedChanged)
            {
                actions_.microphoneMutedChanged(microphoneMuted_);
            }
        }

        if (menu.disconnect && actions_.disconnect)
        {
            actions_.disconnect();
        }
    }

    reportState();
    ImGui::Render();

    if (hdrOutput_)
    {
        ImDrawData* const drawData = ImGui::GetDrawData();

        for (int index = 0; drawData != nullptr && index < drawData->CmdListsCount; ++index)
        {
            ImDrawList* const drawList = drawData->CmdLists[index];

            for (int vertex = 0; vertex < drawList->VtxBuffer.Size; ++vertex)
            {
                drawList->VtxBuffer[vertex].col =
                    pqColor(drawList->VtxBuffer[vertex].col, sdrWhiteNits_);
            }
        }
    }

    const std::array<ID3D12DescriptorHeap*, 1> heaps{viewHeap_.Get()};
    commandList.SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
    commandList.OMSetRenderTargets(1, &target, FALSE, nullptr);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), &commandList);
}

void Overlay::sendGamepad(const OverlayGamepad& gamepad)
{
    ImGuiIO& io = ImGui::GetIO();
    const bool active = menuOpen_.load() && gamepad.connected;
    constexpr float kStickThreshold = 0.5F;

    const auto key = [&io, active](ImGuiKey key, bool down) { io.AddKeyEvent(key, active && down); };

    key(ImGuiKey_GamepadFaceDown, gamepad.cross);
    key(ImGuiKey_GamepadFaceRight, gamepad.circle);
    key(ImGuiKey_GamepadFaceLeft, gamepad.square);
    key(ImGuiKey_GamepadFaceUp, gamepad.triangle);
    key(ImGuiKey_GamepadDpadUp, gamepad.dpadUp || gamepad.leftY < -kStickThreshold);
    key(ImGuiKey_GamepadDpadDown, gamepad.dpadDown || gamepad.leftY > kStickThreshold);
    key(ImGuiKey_GamepadDpadLeft, gamepad.dpadLeft || gamepad.leftX < -kStickThreshold);
    key(ImGuiKey_GamepadDpadRight, gamepad.dpadRight || gamepad.leftX > kStickThreshold);
    key(ImGuiKey_GamepadL1, gamepad.l1);
    key(ImGuiKey_GamepadR1, gamepad.r1);
    key(ImGuiKey_GamepadL3, gamepad.l3);
    key(ImGuiKey_GamepadR3, gamepad.r3);
    key(ImGuiKey_GamepadStart, gamepad.options);
    key(ImGuiKey_GamepadBack, gamepad.create);
}

void Overlay::reportState()
{
    const bool menuOpen = menuOpen_.load();

    if (menuOpen != reportedMenuOpen_)
    {
        reportedMenuOpen_ = menuOpen;

        if (actions_.menuOpenChanged)
        {
            actions_.menuOpenChanged(menuOpen);
        }
    }

    const bool statisticsVisible = statisticsVisible_.load();

    if (statisticsVisible != reportedStatisticsVisible_)
    {
        reportedStatisticsVisible_ = statisticsVisible;

        if (actions_.statisticsVisibleChanged)
        {
            actions_.statisticsVisibleChanged(statisticsVisible);
        }
    }
}

void Overlay::toggleMenu()
{
    menuOpen_.store(!menuOpen_.load());
}

void Overlay::toggleStatistics()
{
    statisticsVisible_.store(!statisticsVisible_.load());
}

void Overlay::setWindowLayout(core::WindowMode mode, core::WindowSize size)
{
    windowMode_.store(mode);
    windowSize_.store(size);
}

bool Overlay::menuOpen() const
{
    return menuOpen_.load();
}

bool Overlay::statisticsVisible() const
{
    return statisticsVisible_.load();
}

bool Overlay::visible() const
{
    return menuOpen_.load() || statisticsVisible_.load();
}

bool Overlay::handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (context_ == nullptr)
    {
        return false;
    }

    const std::lock_guard lock(imGuiMutex());
    ImGui::SetCurrentContext(context_);
    return ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam) != 0;
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

} // namespace mfplay::render
