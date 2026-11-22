#include "app/AppController.h"

#include "app/Version.h"

namespace lynx::app
{

AppController::AppController(bool remotePlayReady, QObject* parent)
    : QObject(parent), remotePlayReady_(remotePlayReady)
{
}

QString AppController::version() const
{
    return QString::fromUtf8(kVersion.data(), static_cast<qsizetype>(kVersion.size()));
}

bool AppController::remotePlayReady() const
{
    return remotePlayReady_;
}

} // namespace lynx::app
