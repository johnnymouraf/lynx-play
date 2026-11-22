#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration>

namespace lynx::app
{

// The interface's entry point into the application: state the QML screens
// show and the actions they trigger.
class AppController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main()")

    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool remotePlayReady READ remotePlayReady CONSTANT)

public:
    explicit AppController(bool remotePlayReady, QObject* parent = nullptr);

    [[nodiscard]] QString version() const;
    [[nodiscard]] bool remotePlayReady() const;

private:
    bool remotePlayReady_;
};

} // namespace lynx::app
