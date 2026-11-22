#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace lynx::render
{

// One source frame's comparison against the picture before it, from the GPU
// block metric.
struct CadenceSample
{
    // 4x4 blocks whose mean absolute luma difference exceeds the band.
    std::uint32_t changedBlocks = 0;
    std::uint32_t totalBlocks = 0;
    // The whole picture's mean absolute luma difference, in 8-bit levels.
    double meanDifference = 0.0;
};

// The stream's cadence from its frames' distinct/repeated decisions.
//
// Flags fill one window of the stream's frames; when a window completes, its
// period is frames / distinct frames and its cadence is the container rate
// divided by that period. Two consecutive windows must agree within 1.6 fps
// before an estimate is published, which keeps a single misread frame from
// changing it. A window
// with fewer than twelve distinct frames (a static scene) holds the last
// estimate instead of claiming the container rate. reset() starts over after
// a scene cut, a resize, a source change or dropped frames.
class CadenceDetector
{
public:
    struct Options
    {
        // The stream's frame rate; the PS5 always sends 60.
        double containerFramesPerSecond = 60.0;
        // A frame is a repeat when fewer than this share of its blocks change.
        double repeatFraction = 0.01;
        // A scene cut changes most of the picture and a lot of luma.
        double sceneCutFraction = 0.6;
        double sceneCutLevels = 32.0;
        std::size_t windowSize = 60;
        std::size_t minimumDistinct = 12;
        double agreementFramesPerSecond = 1.6;
    };

    enum class Decision
    {
        // The picture changed: a new unique picture.
        Distinct,
        // The picture repeats the one before it.
        Repeat,
        // A hard cut; the window restarts and this frame adds no flag.
        SceneCut,
        // No blocks were measured; nothing changed.
        Ignored,
    };

    struct Estimate
    {
        double framesPerSecond = 0.0;
        std::size_t distinctFrames = 0;
        std::size_t repeatedFrames = 0;
        std::size_t windowFrames = 0;
    };

    CadenceDetector();
    explicit CadenceDetector(Options options);

    // Adds one frame's comparison to the windows.
    Decision observe(const CadenceSample& sample);
    void reset();

    // The published cadence, absent until two windows agree.
    [[nodiscard]] const std::optional<Estimate>& estimate() const;
    // The rolling window, whether or not an estimate is published yet.
    [[nodiscard]] std::size_t windowFrames() const;
    [[nodiscard]] std::size_t repeatedFrames() const;

private:
    void addFlag(bool distinct);
    void finishWindow();

    Options options_;
    // The newest flags; true is a distinct picture.
    std::deque<bool> recent_;
    std::size_t distinctInWindow_ = 0;
    std::size_t distinctInBlock_ = 0;
    std::size_t flagsInBlock_ = 0;
    std::optional<double> previousWindow_;
    std::optional<Estimate> estimate_;
};

} // namespace lynx::render
