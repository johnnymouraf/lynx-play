#include "app/PsnDevicesClient.h"

#include <QMetaObject>

#include <utility>

#include <spdlog/spdlog.h>

namespace mfplay::app
{

PsnDevicesClient::PsnDevicesClient(QObject* parent) : QObject(parent) {}

PsnDevicesClient::~PsnDevicesClient() = default;

void PsnDevicesClient::refresh(std::string accessToken)
{
    if (accessToken.empty())
    {
        return;
    }

    if (running_)
    {
        pendingToken_ = std::move(accessToken);
        return;
    }

    start(std::move(accessToken));
}

void PsnDevicesClient::start(std::string accessToken)
{
    running_ = true;

    worker_ = std::jthread([this, token = std::move(accessToken)] {
        auto result = remoteplay::listPsnDevices(token);

        QMetaObject::invokeMethod(
            this, [this, result = std::move(result)]() mutable { finish(std::move(result)); },
            Qt::QueuedConnection);
    });
}

void PsnDevicesClient::finish(std::expected<std::vector<remoteplay::PsnDevice>, std::string> result)
{
    running_ = false;

    if (result)
    {
        spdlog::info("The PSN lists {} PS5 with Remote Play on", result->size());
        emit devicesListed(std::move(*result));
    }
    else
    {
        spdlog::warn("The PSN's PS5 list is unavailable: {}", result.error());
    }

    if (!pendingToken_.empty())
    {
        start(std::exchange(pendingToken_, {}));
    }
}

} // namespace mfplay::app
