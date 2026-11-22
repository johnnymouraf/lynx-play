#include "app/Fonts.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QStringList>

#include <spdlog/spdlog.h>

namespace lynx::app
{

namespace
{

constexpr auto kFontDirectory = ":/qt/qml/LynxPlay/assets/fonts";
constexpr auto kOverlayFontPath = ":/qt/qml/LynxPlay/assets/fonts/JetBrainsMono-Medium.ttf";
constexpr auto kDefaultFamily = "Inter";
constexpr int kDefaultPixelSize = 14;

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

std::shared_ptr<const std::vector<std::uint8_t>> overlayFont()
{
    QFile file(QString::fromLatin1(kOverlayFontPath));

    if (!file.open(QIODevice::ReadOnly))
    {
        spdlog::warn("The overlay font is missing: {}", kOverlayFontPath);
        return {};
    }

    const QByteArray bytes = file.readAll();
    const auto* const begin = reinterpret_cast<const std::uint8_t*>(bytes.constData());

    return std::make_shared<const std::vector<std::uint8_t>>(begin, begin + bytes.size());
}

} // namespace lynx::app
