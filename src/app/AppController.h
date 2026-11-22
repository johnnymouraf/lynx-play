#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration>

#include <memory>

namespace lynx::render
{
class StreamWindow;
}

namespace lynx::app
{

struct StartupState
{
    bool remotePlayReady = false;
    // Empty when no GPU can render the stream.
    QString gpuName;
};

// The interface's entry point into the application: state the QML screens
// show and the actions they trigger.
class AppController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main()")

    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool remotePlayReady READ remotePlayReady CONSTANT)
    Q_PROPERTY(QString gpuName READ gpuName CONSTANT)

public:
    explicit AppController(StartupState state, QObject* parent = nullptr);
    ~AppController() override;

    [[nodiscard]] QString version() const;
    [[nodiscard]] bool remotePlayReady() const;
    [[nodiscard]] QString gpuName() const;

    // Opens the stream window with the animated test pattern, or brings it to
    // the front when it is already open. False when it can't be created.
    Q_INVOKABLE bool openVideoTest();

private:
    StartupState state_;
    std::unique_ptr<render::StreamWindow> videoTest_;
};

} // namespace lynx::app
