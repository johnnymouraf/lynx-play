#include "render/Overlay.h"

#include <algorithm>
#include <array>
#include <cfloat>
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

// Dear ImGui leaves this one for the application to declare, to keep
// <windows.h> out of its header.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                              LPARAM lParam);

namespace lynx::render
{

namespace
{

constexpr UINT kViewCount = 8;
constexpr float kPanelMargin = 20.0F;
constexpr float kStatsWidth = 320.0F;
constexpr float kMenuWidth = 380.0F;
constexpr float kMenuHeight = 680.0F;

constexpr float kBodySize = 14.0F;
constexpr float kCaptionSize = 12.0F;
constexpr float kOverlineSize = 12.0F;
constexpr float kButtonSize = 16.0F;
constexpr float kTitleSize = 18.0F;
constexpr float kMonoSize = 14.0F;

constexpr ImU32 kText = IM_COL32(0xED, 0xF0, 0xF4, 0xFF);
constexpr ImU32 kTextMuted = IM_COL32(0xA1, 0xAA, 0xB8, 0xFF);
constexpr ImU32 kTextSubtle = IM_COL32(0x82, 0x8C, 0x9B, 0xFF);
constexpr ImU32 kAccent = IM_COL32(0xF5, 0xA2, 0x3A, 0xFF);
constexpr ImU32 kAccentSubtle = IM_COL32(0x31, 0x2A, 0x21, 0xFF);
constexpr ImU32 kTextOnAccent = IM_COL32(0x1A, 0x11, 0x05, 0xFF);
constexpr ImU32 kDanger = IM_COL32(0xF2, 0x62, 0x5F, 0xFF);
constexpr ImU32 kDangerSubtle = IM_COL32(0x31, 0x21, 0x26, 0xFF);
constexpr ImU32 kSurfaceHigh = IM_COL32(0x1F, 0x26, 0x31, 0xFF);
constexpr ImU32 kSurfaceHover = IM_COL32(0x28, 0x31, 0x42, 0xFF);
constexpr ImU32 kBorderStrong = IM_COL32(0x3A, 0x45, 0x55, 0xFF);
constexpr ImU32 kBorderControl = IM_COL32(0x62, 0x6D, 0x7F, 0xFF);
constexpr ImU32 kFocus = IM_COL32(0xFF, 0xB6, 0x5C, 0xFF);
constexpr ImU32 kOverlayHover = IM_COL32(0xFF, 0xFF, 0xFF, 0x14);

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
// different threads must not interleave.
std::mutex& imGuiMutex()
{
    static std::mutex mutex;
    return mutex;
}

struct Fonts
{
    ImFont* body = nullptr;
    ImFont* label = nullptr;
    ImFont* caption = nullptr;
    ImFont* overline = nullptr;
    ImFont* button = nullptr;
    ImFont* title = nullptr;
    ImFont* mono = nullptr;
};

struct MenuResult
{
    bool statisticsToggled = false;
    bool reflexChanged = false;
    core::ReflexMode reflex = core::ReflexMode::Off;
    bool microphoneToggled = false;
    bool disconnect = false;
};

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

std::string codecText(const core::StreamMetricsSnapshot& video)
{
    if (video.codec.empty())
    {
        return "—";
    }

    return video.codec + " · SDR";
}

std::string fpsText(const core::StreamMetricsSnapshot& video, double fps)
{
    return video.formatValid ? decimalText(fps) : "—";
}

float textWidth(ImFont* font, float size, const char* text)
{
    return font->CalcTextSizeA(size, FLT_MAX, 0.0F, text).x;
}

void drawText(ImDrawList* drawList, ImFont* font, float size, ImVec2 position, ImU32 color,
              const char* text)
{
    drawList->AddText(font, size, position, color, text);
}

void rightText(ImFont* font, float size, ImU32 color, const char* text)
{
    ImGui::PushFont(font, size);
    const float width = ImGui::CalcTextSize(text).x;
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - width);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void valueRow(const Fonts& fonts, const char* label, const char* value, ImU32 valueColor = kText)
{
    ImGui::PushFont(fonts.label, kBodySize);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    rightText(fonts.mono, kMonoSize, valueColor, value);
}

void sectionLabel(const Fonts& fonts, const char* label)
{
    ImGui::PushFont(fonts.overline, kOverlineSize);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void applyStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(12.0F, 12.0F);
    style.WindowRounding = 10.0F;
    style.WindowBorderSize = 1.0F;
    style.ItemSpacing = ImVec2(8.0F, 4.0F);
    style.FramePadding = ImVec2(12.0F, 8.0F);
    style.FrameRounding = 6.0F;

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0392F, 0.0509F, 0.0706F, 0.851F);
    style.Colors[ImGuiCol_Border] = ImVec4(1.0F, 1.0F, 1.0F, 0.1216F);
    style.Colors[ImGuiCol_Text] = ImVec4(0.9294F, 0.9412F, 0.9569F, 1.0F);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.6314F, 0.6667F, 0.7216F, 1.0F);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0392F, 0.0509F, 0.0706F, 1.0F);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.1216F, 0.1490F, 0.1922F, 1.0F);
    style.Colors[ImGuiCol_Button] = ImVec4(0.1216F, 0.1490F, 0.1922F, 1.0F);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.1569F, 0.1922F, 0.2588F, 1.0F);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.2275F, 0.2706F, 0.3333F, 1.0F);
    style.Colors[ImGuiCol_NavCursor] = ImVec4(0.9608F, 0.7137F, 0.3647F, 1.0F);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.9608F, 0.6353F, 0.2275F, 1.0F);
}

void drawVideoSection(const Fonts& fonts, const OverlayStats& stats)
{
    const core::StreamMetricsSnapshot& video = stats.video;

    sectionLabel(fonts, "VÍDEO");

    const std::string resolution = resolutionText(video, stats.outputWidth, stats.outputHeight);
    const std::string codec = codecText(video);
    const std::string bitrate =
        video.bitrateMbps > 0.0 ? decimalText(video.bitrateMbps) + " Mbps" : "—";
    const std::string loss = video.packetLossValid ? decimalText(video.packetLossPercent) + " %" : "—";

    valueRow(fonts, "Resolução", resolution.c_str());
    valueRow(fonts, "Codec", codec.c_str());
    valueRow(fonts, "FPS recebidos", fpsText(video, video.receivedFps).c_str());
    valueRow(fonts, "FPS decodificados", fpsText(video, video.decodedFps).c_str());
    valueRow(fonts, "FPS exibidos", fpsText(video, video.presentedFps).c_str());
    valueRow(fonts, "Bitrate", bitrate.c_str());
    valueRow(fonts, "Perda de pacotes", loss.c_str());
}

void drawLatencySection(const Fonts& fonts, const OverlayStats& stats)
{
    const core::StreamMetricsSnapshot& video = stats.video;

    sectionLabel(fonts, "LATÊNCIA");

    const std::string decode =
        video.decodeValid ? millisecondsText(video.decodeMilliseconds) : "—";
    const std::string present =
        video.presentIntervalValid ? millisecondsText(video.presentIntervalMilliseconds) : "—";
    const std::string ready =
        video.readyToPresentValid ? millisecondsText(video.readyToPresentMilliseconds) : "—";

    valueRow(fonts, "Decodificação", decode.c_str());
    valueRow(fonts, "Apresentação", present.c_str());
    valueRow(fonts, "Quadro até a tela", ready.c_str());
}

void drawFrameTime(const Fonts& fonts, const OverlayStats& stats)
{
    const core::StreamMetricsSnapshot& video = stats.video;

    if (video.frameTimeCount < 2)
    {
        return;
    }

    float maximum = 33.3F;

    for (std::size_t index = 0; index < video.frameTimeCount; ++index)
    {
        maximum = std::max(maximum, video.frameTimes.at(index) * 1.2F);
    }

    ImGui::PushFont(fonts.caption, kCaptionSize);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
    ImGui::TextUnformatted("Tempo de quadro");
    ImGui::PopStyleColor();
    rightText(fonts.caption, kCaptionSize, kTextMuted,
              millisecondsText(video.presentIntervalMilliseconds).c_str());
    ImGui::PopFont();
    ImGui::NewLine();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0F, 0.0F));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0F, 1.0F, 1.0F, 0.0392F));
    ImGui::PlotLines("##frame-times", video.frameTimes.data(), static_cast<int>(video.frameTimeCount),
                      0, nullptr, 0.0F, maximum, ImVec2(-1.0F, 36.0F));
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void drawGpuSection(const Fonts& fonts, const OverlayStats& stats)
{
    sectionLabel(fonts, "GPU POR ETAPA");

    for (std::size_t stage = 0; stage < kEffectStageCount; ++stage)
    {
        valueRow(fonts, stageLabel(static_cast<EffectStage>(stage)),
                 stats.gpuTimingsValid
                     ? millisecondsText(stats.stageMilliseconds.at(stage), 2).c_str()
                     : "—");
    }

    valueRow(fonts, "Total",
             stats.gpuTimingsValid ? millisecondsText(stats.totalMilliseconds, 2).c_str() : "—");
}

void drawReflexSection(const Fonts& fonts, const ReflexOverlayStats& reflex)
{
    sectionLabel(fonts, "NVIDIA REFLEX");

    switch (reflex.state)
    {
    case ReflexOverlayStats::State::Unavailable:
        valueRow(fonts, "Disponibilidade", reflex.reason.empty() ? "indisponível" : reflex.reason.c_str());
        return;

    case ReflexOverlayStats::State::Waiting:
        valueRow(fonts, "Modo", reflex.mode.c_str());
        valueRow(fonts, "Latência total", "aguardando dados…");
        return;

    case ReflexOverlayStats::State::Active:
    case ReflexOverlayStats::State::Stalled:
    case ReflexOverlayStats::State::Failed:
        valueRow(fonts, "Modo", reflex.mode.c_str());
        valueRow(fonts, "Low latency efetivo", reflex.lowLatencyEffective ? "sim" : "não");
        valueRow(fonts, "Sleep médio", millisecondsText(reflex.sleepMeanMs, 3).c_str());
        valueRow(fonts, "Sleep máximo", millisecondsText(reflex.sleepMaxMs, 3).c_str());
        valueRow(fonts, "Driver vê Sleep", reflex.driverSeesSleep ? "sim" : "não");
        valueRow(fonts, "Latência total",
                 reflex.latencyValid ? millisecondsText(reflex.latencyTotalMs).c_str() : "—");
        valueRow(fonts, "GPU", reflex.latencyValid ? millisecondsText(reflex.latencyGpuMs).c_str() : "—");
        valueRow(fonts, "Render",
                 reflex.latencyValid ? millisecondsText(reflex.latencyRenderMs).c_str() : "—");
        valueRow(fonts, "Apresentação",
                 reflex.latencyValid ? millisecondsText(reflex.latencyPresentMs).c_str() : "—");
        valueRow(fonts, "Driver",
                 reflex.latencyValid ? millisecondsText(reflex.latencyDriverMs).c_str() : "—");
        valueRow(fonts, "Fila do SO",
                 reflex.latencyValid ? millisecondsText(reflex.latencyQueueMs).c_str() : "—");

        if (reflex.state == ReflexOverlayStats::State::Stalled)
        {
            valueRow(fonts, "Aviso", "sleep alto: desligado nesta transmissão", kAccent);
        }

        if (reflex.state == ReflexOverlayStats::State::Failed)
        {
            valueRow(fonts, "Aviso", "o sleep do driver falhou", kDanger);
        }

        return;
    }
}

void drawStatistics(const Fonts& fonts, const OverlayStats& stats)
{
    ImGui::SetNextWindowPos(ImVec2(kPanelMargin, kPanelMargin), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kStatsWidth, 0.0F), ImGuiCond_Always);

    if (!ImGui::Begin("lynx-statistics", nullptr, kStatsFlags))
    {
        ImGui::End();
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0F, 6.0F));
    ImGui::PushFont(fonts.label, kBodySize);
    ImGui::TextUnformatted("Estatísticas");
    ImGui::PopFont();
    rightText(fonts.caption, kCaptionSize, kTextMuted, "F2");

    drawVideoSection(fonts, stats);
    drawLatencySection(fonts, stats);
    drawGpuSection(fonts, stats);
    drawFrameTime(fonts, stats);
    drawReflexSection(fonts, stats.reflex);

    ImGui::PopStyleVar();
    ImGui::End();
}

void drawSwitch(ImDrawList* drawList, ImVec2 topLeft, bool on)
{
    const ImVec2 kTrackSize(48.0F, 28.0F);
    constexpr float kThumbRadius = 11.0F;

    const ImVec2 bottomRight(topLeft.x + kTrackSize.x, topLeft.y + kTrackSize.y);
    drawList->AddRectFilled(topLeft, bottomRight, on ? kAccent : kSurfaceHigh,
                             kTrackSize.y / 2.0F);

    if (!on)
    {
        drawList->AddRect(topLeft, bottomRight, kBorderControl, kTrackSize.y / 2.0F);
    }

    const float thumbX = topLeft.x + (on ? kTrackSize.x - 3.0F - 2.0F * kThumbRadius : 3.0F);
    drawList->AddCircleFilled(ImVec2(thumbX + kThumbRadius, topLeft.y + kTrackSize.y / 2.0F),
                               kThumbRadius, on ? kTextOnAccent : kTextMuted);
}

bool toggleRow(const char* id, const Fonts& fonts, const char* label, const char* detail, bool on,
               float height)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;

    ImGui::PushID(id);
    ImGui::InvisibleButton("row", ImVec2(width, height));
    const bool clicked = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();
    const bool focused = ImGui::IsItemFocused();
    ImGui::PopID();

    ImDrawList* const drawList = ImGui::GetWindowDrawList();

    if (hovered)
    {
        drawList->AddRectFilled(start, ImVec2(start.x + width, start.y + height), kOverlayHover,
                                6.0F);
    }

    if (focused)
    {
        drawList->AddRect(start, ImVec2(start.x + width, start.y + height), kFocus, 6.0F, 0, 2.0F);
    }

    drawText(drawList, fonts.label, kBodySize, ImVec2(start.x + 12.0F, start.y + 8.0F), kText,
             label);

    if (detail != nullptr && detail[0] != '\0')
    {
        drawText(drawList, fonts.caption, kCaptionSize, ImVec2(start.x + 12.0F, start.y + 30.0F),
                 kTextMuted, detail);
    }

    drawSwitch(drawList, ImVec2(start.x + width - 12.0F - 48.0F, start.y + (height - 28.0F) / 2.0F),
               on);

    return clicked;
}

bool actionButton(const char* id, const Fonts& fonts, const char* label, bool danger)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    constexpr float kHeight = 44.0F;

    ImGui::PushID(id);
    ImGui::InvisibleButton("button", ImVec2(width, kHeight));
    const bool clicked = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();
    const bool focused = ImGui::IsItemFocused();
    ImGui::PopID();

    ImDrawList* const drawList = ImGui::GetWindowDrawList();
    const ImVec2 bottomRight(start.x + width, start.y + kHeight);

    drawList->AddRectFilled(start, bottomRight, danger ? kDangerSubtle : kSurfaceHigh, 10.0F);

    if (!danger)
    {
        drawList->AddRect(start, bottomRight, kBorderStrong, 10.0F);
    }

    if (hovered)
    {
        drawList->AddRectFilled(start, bottomRight,
                                 danger ? IM_COL32(0x3B, 0x27, 0x2D, 0xFF) : kSurfaceHover, 10.0F);
    }

    if (focused)
    {
        drawList->AddRect(start, bottomRight, kFocus, 10.0F, 0, 2.0F);
    }

    const float labelWidth = textWidth(fonts.button, kButtonSize, label);
    drawText(drawList, fonts.button, kButtonSize,
             ImVec2(start.x + (width - labelWidth) / 2.0F, start.y + (kHeight - kButtonSize) / 2.0F),
             danger ? kDanger : kText, label);

    return clicked;
}

bool reflexSegments(const Fonts& fonts, const ImVec2& start, const ReflexOverlayStats& reflex,
                    core::ReflexMode current, core::ReflexMode* chosen)
{
    constexpr std::array<core::ReflexMode, 3> kModes{
        core::ReflexMode::Off,
        core::ReflexMode::On,
        core::ReflexMode::Boost,
    };
    constexpr float kSegmentPadding = 16.0F;
    constexpr float kOuterPadding = 4.0F;
    constexpr float kGap = 4.0F;
    constexpr float kHeight = 36.0F;
    constexpr float kContainerHeight = kHeight + 2.0F * kOuterPadding;

    std::array<float, 3> widths{};

    for (std::size_t index = 0; index < kModes.size(); ++index)
    {
        widths.at(index) =
            2.0F * kSegmentPadding + textWidth(fonts.label, kBodySize, reflexModeLabel(kModes.at(index)));
    }

    const float containerWidth = 2.0F * kOuterPadding + widths.at(0) + widths.at(1) + widths.at(2) +
                                 2.0F * kGap;

    ImGui::SetCursorScreenPos(start);
    ImGui::InvisibleButton("reflex-modes", ImVec2(containerWidth, kContainerHeight));
    const bool clicked = ImGui::IsItemClicked() &&
                         reflex.state != ReflexOverlayStats::State::Unavailable;
    const ImVec2 mouse = ImGui::GetIO().MousePos;

    ImDrawList* const drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(start, ImVec2(start.x + containerWidth, start.y + kContainerHeight),
                             IM_COL32(0x0A, 0x0D, 0x12, 0xFF), 10.0F);
    drawList->AddRect(start, ImVec2(start.x + containerWidth, start.y + kContainerHeight),
                       kBorderStrong, 10.0F);

    float x = start.x + kOuterPadding;

    for (std::size_t index = 0; index < kModes.size(); ++index)
    {
        const float width = widths.at(index);
        const ImVec2 segmentTop(x, start.y + kOuterPadding);
        const ImVec2 segmentBottom(x + width, segmentTop.y + kHeight);
        const bool selected = kModes.at(index) == current;
        const bool available = reflex.state != ReflexOverlayStats::State::Unavailable;

        if (selected)
        {
            drawList->AddRectFilled(segmentTop, segmentBottom, kAccentSubtle, 6.0F);
        }

        if (clicked && mouse.x >= segmentTop.x && mouse.x < segmentBottom.x)
        {
            *chosen = kModes.at(index);
        }

        const char* label = reflexModeLabel(kModes.at(index));
        const float labelWidth = textWidth(fonts.label, kBodySize, label);
        const ImU32 color = !available ? kTextSubtle : (selected ? kAccent : kText);
        drawText(drawList, fonts.label, kBodySize,
                 ImVec2(segmentTop.x + (width - labelWidth) / 2.0F,
                        segmentTop.y + (kHeight - kBodySize) / 2.0F),
                 color, label);

        x += width + kGap;
    }

    return clicked;
}

MenuResult drawMenu(const Fonts& fonts, const OverlayStats& stats, const OverlayOptions& options,
                    core::ReflexMode currentReflex, bool microphoneMuted, bool statsVisible)
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float height = std::min(kMenuHeight, std::max(320.0F, display.y - 2.0F * kPanelMargin));

    ImGui::SetNextWindowPos(ImVec2(display.x - kMenuWidth - kPanelMargin, kPanelMargin),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kMenuWidth, height), ImGuiCond_Always);

    MenuResult result;
    result.reflex = currentReflex;

    if (!ImGui::Begin("lynx-menu", nullptr, kMenuFlags))
    {
        ImGui::End();
        return result;
    }

    const Fonts& menuFonts = fonts;

    ImGui::PushFont(menuFonts.title, kTitleSize);
    ImGui::TextUnformatted("Menu rápido");
    ImGui::PopFont();

    // The session line counts from the overlay's first frame.
    const long long seconds = static_cast<long long>(ImGui::GetTime());
    const long long minutes = seconds / 60;
    const std::string sessionLine =
        options.sessionName.empty()
            ? std::string()
            : std::format("{} · {}", options.sessionName,
                          minutes > 0 ? std::format("jogando há {} min", minutes)
                                      : std::string("jogando agora"));

    if (!sessionLine.empty())
    {
        ImGui::PushFont(menuFonts.caption, kCaptionSize);
        ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
        ImGui::TextUnformatted(sessionLine.c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    ImGui::Dummy(ImVec2(0.0F, 4.0F));

    const ImVec2 reflexStart = ImGui::GetCursorScreenPos();
    ImDrawList* const drawList = ImGui::GetWindowDrawList();
    drawText(drawList, menuFonts.label, kBodySize, ImVec2(reflexStart.x + 12.0F, reflexStart.y + 8.0F),
             kText, "NVIDIA Reflex");
    core::ReflexMode chosen = currentReflex;
    const bool reflexClicked = reflexSegments(
        menuFonts, ImVec2(reflexStart.x + 12.0F, reflexStart.y + 36.0F), stats.reflex, currentReflex,
        &chosen);
    ImGui::SetCursorScreenPos(ImVec2(reflexStart.x, reflexStart.y + 88.0F));

    if (reflexClicked && chosen != currentReflex)
    {
        result.reflexChanged = true;
        result.reflex = chosen;
    }

    if (toggleRow("stats-row", menuFonts, "Mostrar estatísticas", "", statsVisible, 44.0F))
    {
        result.statisticsToggled = true;
    }

    if (toggleRow("microphone-row", menuFonts, "Microfone", options.microphoneDetail.c_str(),
                  !microphoneMuted, 54.0F))
    {
        result.microphoneToggled = true;
    }

    const float remaining = ImGui::GetContentRegionAvail().y;
    ImGui::Dummy(ImVec2(0.0F, std::max(8.0F, remaining - 44.0F - 16.0F - 12.0F)));

    if (actionButton("disconnect", menuFonts, "Desconectar", true))
    {
        result.disconnect = true;
    }

    ImGui::Dummy(ImVec2(0.0F, 2.0F));
    ImGui::PushFont(menuFonts.caption, kCaptionSize);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
    const char* const hint = "Fechar: F1 ou segure Create + Options";
    const float hintWidth = ImGui::CalcTextSize(hint).x;
    ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x - hintWidth) / 2.0F);
    ImGui::TextUnformatted(hint);
    ImGui::PopStyleColor();
    ImGui::PopFont();

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
    overlay->options_ = std::move(options);
    overlay->actions_ = std::move(actions);
    overlay->reflex_ = overlay->options_.reflex;
    overlay->microphoneMuted_ = overlay->options_.microphoneMuted;
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

    overlay->loadFonts(fonts);
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
    captionFont_ = add(fontData_.label, kCaptionSize);
    overlineFont_ = add(fontData_.title, kOverlineSize);
    buttonFont_ = add(fontData_.title, kButtonSize);
    titleFont_ = add(fontData_.title, kTitleSize);
    monoFont_ = add(fontData_.mono, kMonoSize);

    if (bodyFont_ == nullptr)
    {
        bodyFont_ = io.Fonts->AddFontDefault();
    }

    if (labelFont_ == nullptr)
    {
        labelFont_ = bodyFont_;
    }

    if (captionFont_ == nullptr)
    {
        captionFont_ = bodyFont_;
    }

    if (overlineFont_ == nullptr)
    {
        overlineFont_ = bodyFont_;
    }

    if (buttonFont_ == nullptr)
    {
        buttonFont_ = bodyFont_;
    }

    if (titleFont_ == nullptr)
    {
        titleFont_ = bodyFont_;
    }

    if (monoFont_ == nullptr)
    {
        monoFont_ = bodyFont_;
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
        .caption = captionFont_,
        .overline = overlineFont_,
        .button = buttonFont_,
        .title = titleFont_,
        .mono = monoFont_,
    };

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    const bool menuOpen = menuOpen_.load();
    const bool statisticsVisible = statisticsVisible_.load();

    if (statisticsVisible)
    {
        drawStatistics(fonts, stats);
    }

    if (menuOpen)
    {
        const MenuResult menu = drawMenu(fonts, stats, options_, reflex_, microphoneMuted_,
                                         statisticsVisible);

        if (menu.statisticsToggled)
        {
            statisticsVisible_.store(!statisticsVisible);
        }

        if (menu.reflexChanged)
        {
            reflex_ = menu.reflex;

            if (actions_.reflexChanged)
            {
                actions_.reflexChanged(reflex_);
            }
        }

        if (menu.microphoneToggled)
        {
            microphoneMuted_ = !microphoneMuted_;

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

} // namespace lynx::render
