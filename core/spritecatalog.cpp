#include "spritecatalog.h"

#include "apppaths.h"

#include <QDir>
#include <QFileInfo>

namespace {
QStringList imageFilters()
{
    return {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
            QStringLiteral("*.webp")};
}
} // namespace

SpriteCatalog::SpriteCatalog(QObject *parent)
    : QObject(parent)
    , modesBase_(AppPaths::modesRoot())
{
    QDir root(modesBase_);
    if (!root.exists())
        root.mkpath(QStringLiteral("."));

    modeNames_ = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (modeNames_.isEmpty()) {
        const QString fallback = QStringLiteral("default");
        root.mkdir(fallback);
        modeNames_ = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    }

    if (!modeNames_.isEmpty()) {
        currentMode_ = modeNames_.constFirst();
        rescanCurrentMode();
    }
}

QStringList SpriteCatalog::modeNames() const
{
    return modeNames_;
}

QString SpriteCatalog::currentMode() const
{
    return currentMode_;
}

bool SpriteCatalog::setMode(const QString &name)
{
    if (!modeNames_.contains(name))
        return false;
    if (currentMode_ == name)
        return true;
    currentMode_ = name;
    rescanCurrentMode();
    emit modeChanged(currentMode_);
    return true;
}

int SpriteCatalog::frameCount() const
{
    return frames_.size();
}

int SpriteCatalog::currentFrameIndex() const
{
    return currentIndex_;
}

void SpriteCatalog::setFrameIndex(int index)
{
    if (frames_.isEmpty())
        return;
    const int n = frames_.size();
    const int i = ((index % n) + n) % n;
    if (i == currentIndex_)
        return;
    currentIndex_ = i;
    emit frameChanged(currentIndex_);
}

void SpriteCatalog::nextFrame()
{
    if (frames_.isEmpty())
        return;
    setFrameIndex(currentIndex_ + 1);
}

QImage SpriteCatalog::currentImage() const
{
    if (frames_.isEmpty() || currentIndex_ < 0 || currentIndex_ >= frames_.size())
        return {};
    return frames_.at(currentIndex_);
}

void SpriteCatalog::rescanCurrentMode()
{
    frameFiles_.clear();
    frames_.clear();
    currentIndex_ = 0;
    if (currentMode_.isEmpty())
        return;
    loadFrameFiles();
    emit frameChanged(currentIndex_);
}

bool SpriteCatalog::loadFrameFiles()
{
    const QDir dir(modesBase_ + QLatin1Char('/') + currentMode_);
    if (!dir.exists())
        return false;

    const QStringList names = dir.entryList(imageFilters(), QDir::Files, QDir::Name);
    frameFiles_.reserve(names.size());
    frames_.reserve(names.size());
    for (const QString &fileName : names) {
        const QString path = dir.filePath(fileName);
        QImage img(path);
        if (img.isNull())
            continue;
        frameFiles_.append(path);
        frames_.append(std::move(img));
    }
    return !frames_.isEmpty();
}
