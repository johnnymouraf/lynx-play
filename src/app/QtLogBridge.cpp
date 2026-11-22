#include "app/QtLogBridge.h"

#include <QtGlobal>
#include <QString>

#include <spdlog/spdlog.h>

namespace lynx::app
{

namespace
{

spdlog::level::level_enum toSpdlogLevel(QtMsgType type)
{
    switch (type)
    {
    case QtDebugMsg:
        return spdlog::level::debug;
    case QtInfoMsg:
        return spdlog::level::info;
    case QtWarningMsg:
        return spdlog::level::warn;
    case QtCriticalMsg:
        return spdlog::level::err;
    case QtFatalMsg:
        return spdlog::level::critical;
    }

    return spdlog::level::warn;
}

void forwardToSpdlog(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const auto category = context.category != nullptr ? context.category : "qt";
    spdlog::log(toSpdlogLevel(type), "[{}] {}", category, message.toStdString());
}

} // namespace

void installQtLogBridge()
{
    qInstallMessageHandler(forwardToSpdlog);
}

} // namespace lynx::app
