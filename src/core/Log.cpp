#include "core/Log.h"

#include <memory>
#include <vector>

#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

namespace mfplay::core
{

namespace
{

constexpr auto kLogFileName = "mfplay.log";
constexpr std::size_t kMaxLogFileBytes = 8 * 1024 * 1024;
constexpr std::size_t kMaxRotatedFiles = 3;

} // namespace

std::expected<void, std::string> initLogging(const std::filesystem::path& directory)
{
    std::error_code error;
    std::filesystem::create_directories(directory, error);

    if (error)
    {
        return std::unexpected("cannot create the log directory: " + error.message());
    }

    try
    {
        const auto file = (directory / kLogFileName).string();
        std::vector<spdlog::sink_ptr> sinks{
            std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file, kMaxLogFileBytes,
                                                                   kMaxRotatedFiles),
            std::make_shared<spdlog::sinks::msvc_sink_mt>(),
        };

        auto logger = std::make_shared<spdlog::logger>("mfplay", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::debug);
        logger->flush_on(spdlog::level::info);
        logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] [%t] %v");
        spdlog::set_default_logger(std::move(logger));
    }
    catch (const spdlog::spdlog_ex& exception)
    {
        return std::unexpected(std::string("cannot open the log file: ") + exception.what());
    }

    return {};
}

} // namespace mfplay::core
