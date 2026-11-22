#include "app/IconProvider.h"

#include <QColor>
#include <QFile>
#include <QPainter>
#include <QStringList>
#include <QSvgRenderer>

#include <optional>

#include <spdlog/spdlog.h>

namespace lynx::app
{

namespace
{

constexpr int kDefaultIconSize = 24;
constexpr auto kIconDirectory = ":/qt/qml/LynxPlay/assets/icons/";

struct IconRequest
{
    QString name;
    QColor color;
    QByteArray strokeWidth;
};

std::optional<IconRequest> parseRequest(const QString& id)
{
    const QStringList parts = id.split(u'/');

    if (parts.size() != 3)
    {
        return std::nullopt;
    }

    QString colorName = parts[1];
    colorName.prepend(u'#');
    const QColor color = QColor::fromString(colorName);

    bool numericStroke = false;
    parts[2].toDouble(&numericStroke);

    if (!color.isValid() || !numericStroke)
    {
        return std::nullopt;
    }

    return IconRequest{parts[0], color, parts[2].toLatin1()};
}

// Lucide icons stroke with currentColor at a width of 2 set on their root.
QByteArray colorize(QByteArray svg, const IconRequest& request)
{
    svg.replace("currentColor", request.color.name(QColor::HexRgb).toLatin1());
    svg.replace("stroke-width=\"2\"", "stroke-width=\"" + request.strokeWidth + '"');
    return svg;
}

} // namespace

IconProvider::IconProvider() : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage IconProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    const QSize imageSize =
        requestedSize.isValid() ? requestedSize : QSize(kDefaultIconSize, kDefaultIconSize);

    if (size != nullptr)
    {
        *size = imageSize;
    }

    const auto request = parseRequest(id);

    if (!request)
    {
        spdlog::warn("Invalid icon request: {}", id.toStdString());
        return {};
    }

    QFile file(QString::fromLatin1(kIconDirectory) + request->name + QStringLiteral(".svg"));

    if (!file.open(QIODevice::ReadOnly))
    {
        spdlog::warn("Unknown icon: {}", request->name.toStdString());
        return {};
    }

    QSvgRenderer renderer(colorize(file.readAll(), *request));
    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setOpacity(request->color.alphaF());
    renderer.render(&painter);
    painter.end();

    return image;
}

} // namespace lynx::app
