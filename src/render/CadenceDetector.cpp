#include "render/CadenceDetector.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace mfplay::render
{

CadenceDetector::Options CadenceDetector::optionsFor(double containerFramesPerSecond)
{
    constexpr double kMinimumDistinctShare = 0.2;

    const double windowFrames = std::max(std::round(containerFramesPerSecond), 1.0);
    const double minimumDistinct = std::max(std::round(windowFrames * kMinimumDistinctShare), 1.0);

    Options options;
    options.containerFramesPerSecond = containerFramesPerSecond;
    options.windowSize = static_cast<std::size_t>(windowFrames);
    options.minimumDistinct = static_cast<std::size_t>(minimumDistinct);

    return options;
}

CadenceDetector::CadenceDetector() : CadenceDetector(Options{})
{
}

CadenceDetector::CadenceDetector(Options options) : options_(options)
{
}

CadenceDetector::Decision CadenceDetector::observe(const CadenceSample& sample)
{
    if (sample.totalBlocks == 0)
    {
        return Decision::Ignored;
    }

    const double changed =
        static_cast<double>(sample.changedBlocks) / static_cast<double>(sample.totalBlocks);

    if (changed > options_.sceneCutFraction && sample.meanDifference > options_.sceneCutLevels)
    {
        reset();
        return Decision::SceneCut;
    }

    const bool distinct = changed >= options_.repeatFraction;
    addFlag(distinct);

    return distinct ? Decision::Distinct : Decision::Repeat;
}

void CadenceDetector::reset()
{
    recent_.clear();
    distinctInWindow_ = 0;
    distinctInBlock_ = 0;
    flagsInBlock_ = 0;
    previousWindow_.reset();
    estimate_.reset();
}

const std::optional<CadenceDetector::Estimate>& CadenceDetector::estimate() const
{
    return estimate_;
}

std::size_t CadenceDetector::windowFrames() const
{
    return recent_.size();
}

std::size_t CadenceDetector::repeatedFrames() const
{
    return recent_.size() - distinctInWindow_;
}

void CadenceDetector::addFlag(bool distinct)
{
    recent_.push_back(distinct);

    if (recent_.size() > options_.windowSize)
    {
        if (recent_.front())
        {
            --distinctInWindow_;
        }

        recent_.pop_front();
    }

    if (distinct)
    {
        ++distinctInWindow_;
        ++distinctInBlock_;
    }

    ++flagsInBlock_;

    if (flagsInBlock_ >= options_.windowSize)
    {
        finishWindow();
    }
}

void CadenceDetector::finishWindow()
{
    if (distinctInBlock_ >= options_.minimumDistinct && flagsInBlock_ > 0)
    {
        const double period =
            static_cast<double>(flagsInBlock_) / static_cast<double>(distinctInBlock_);
        const double candidate = options_.containerFramesPerSecond / period;

        if (previousWindow_ &&
            std::abs(candidate - *previousWindow_) <= options_.agreementFramesPerSecond)
        {
            estimate_ = Estimate{
                .framesPerSecond = candidate,
                .distinctFrames = distinctInWindow_,
                .repeatedFrames = recent_.size() - distinctInWindow_,
                .windowFrames = recent_.size(),
            };
        }

        previousWindow_ = candidate;
    }

    distinctInBlock_ = 0;
    flagsInBlock_ = 0;
}

} // namespace mfplay::render
