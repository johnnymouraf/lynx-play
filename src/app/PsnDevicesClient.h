#pragma once

#include "remoteplay/PsnDevices.h"

#include <QObject>

#include <string>
#include <thread>
#include <vector>

namespace mfplay::app
{

// Lists the account's PS5s on a worker thread, since the PSN answers over
// blocking HTTP.
class PsnDevicesClient : public QObject
{
    Q_OBJECT

public:
    explicit PsnDevicesClient(QObject* parent = nullptr);
    ~PsnDevicesClient() override;

    // Lists with this token; asked while a listing runs, it lists again with
    // the latest token once that one ends.
    void refresh(std::string accessToken);

signals:
    // Only a successful listing is announced; a failed one keeps the last.
    void devicesListed(std::vector<mfplay::remoteplay::PsnDevice> devices);

private:
    void start(std::string accessToken);
    void finish(std::expected<std::vector<remoteplay::PsnDevice>, std::string> result);

    std::jthread worker_;
    bool running_ = false;
    std::string pendingToken_;
};

} // namespace mfplay::app
