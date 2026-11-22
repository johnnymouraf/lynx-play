#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQmlIntegration>

#include <expected>
#include <memory>
#include <string>

namespace lynx::render
{
class StreamWindow;
class TestPatternSource;
}

namespace lynx::app
{

class SettingsController;

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
    Q_PROPERTY(QUrl licensesUrl READ licensesUrl CONSTANT)
    Q_PROPERTY(QUrl logsUrl READ logsUrl CONSTANT)

public:
    AppController(StartupState state, SettingsController& settings, QObject* parent = nullptr);
    ~AppController() override;

    [[nodiscard]] QString version() const;
    [[nodiscard]] bool remotePlayReady() const;
    [[nodiscard]] QString gpuName() const;
    // The third-party licenses shipped next to the executable.
    [[nodiscard]] QUrl licensesUrl() const;
    [[nodiscard]] QUrl logsUrl() const;

    // Opens the stream window with the animated test pattern, or brings it to
    // the front when it is already open. False when it can't be created.
    Q_INVOKABLE bool openVideoTest();

private:
    [[nodiscard]] std::expected<void, std::string> startVideoTest();
    void closeVideoTest();

    StartupState state_;
    SettingsController& settings_;
    std::unique_ptr<render::TestPatternSource> videoTestSource_;
    std::unique_ptr<render::StreamWindow> videoTestWindow_;
};

} // namespace lynx::app
