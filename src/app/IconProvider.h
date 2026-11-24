#pragma once

#include <QQuickImageProvider>

namespace mfplay::app
{

// Serves the bundled Lucide icons as `image://icon/<name>/<rrggbb>/<stroke width>`,
// drawn in the requested color at the requested size.
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider();

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};

} // namespace mfplay::app
