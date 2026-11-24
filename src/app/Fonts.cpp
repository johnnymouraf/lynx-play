#include "app/Fonts.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QStringList>

#include <spdlog/spdlog.h>

namespace mfplay::app
{

namespace
{

constexpr auto kFontDirectory = ":/qt/qml/MFPlay/assets/fonts";
constexpr auto kOverlayFontDirectory = ":/qt/qml/MFPlay/assets/fonts";
constexpr auto kDefaultFamily = "Inter";
constexpr int kDefaultPixelSize = 14;

std::shared_ptr<const std::vector<std::uint8_t>> fontBytes(const char* name)
{
    const QString path = QString::fromLatin1(kOverlayFontDirectory) + QStringLiteral("/") +
                         QString::fromLatin1(name);
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
    {
        spdlog::warn("The overlay font is missing: {}", name);
        return {};
    }

    const QByteArray bytes = file.readAll();
    const auto* const begin = reinterpret_cast<const std::uint8_t*>(bytes.constData());

    return std::make_shared<const std::vector<std::uint8_t>>(begin, begin + bytes.size());
}

} // namespace

void loadBundledFonts()
{
    const QDir directory(QString::fromLatin1(kFontDirectory));
    const QStringList files = directory.entryList({QStringLiteral("*.ttf")}, QDir::Files);

    for (const QString& file : files)
    {
        const int id = QFontDatabase::addApplicationFont(directory.filePath(file));

        if (id < 0)
        {
            spdlog::error("Cannot load the bundled font {}", file.toStdString());
            continue;
        }

        const QString families = QFontDatabase::applicationFontFamilies(id).join(u',');
        spdlog::debug("Font {} loaded as {}", file.toStdString(), families.toStdString());
    }

    QFont font(QString::fromLatin1(kDefaultFamily));
    font.setPixelSize(kDefaultPixelSize);
    QGuiApplication::setFont(font);
}

render::OverlayFonts overlayFonts()
{
    return {
        .body = fontBytes("Inter-Regular.ttf"),
        .label = fontBytes("Inter-Medium.ttf"),
        .title = fontBytes("Inter-SemiBold.ttf"),
        .mono = fontBytes("JetBrainsMono-Medium.ttf"),
    };
}

} // namespace mfplay::app
