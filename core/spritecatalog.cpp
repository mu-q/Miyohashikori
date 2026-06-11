#include "spritecatalog.h"

#include "apppaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

namespace {

QImage loadImageFile(const QString &path)
{
    QImage image;
    if (image.load(path))
        return image;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    if (!image.loadFromData(file.readAll()))
        return {};
    return image;
}

} // namespace

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
    , neutralRestoreTimer_(new QTimer(this))
{
    neutralRestoreTimer_->setSingleShot(true);
    connect(neutralRestoreTimer_, &QTimer::timeout, this, &SpriteCatalog::restoreNeutral);

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

QString SpriteCatalog::currentEmotion() const
{
    return currentEmotion_;
}

QStringList SpriteCatalog::availableEmotions() const
{
    return emotionImages_.keys();
}

bool SpriteCatalog::setEmotion(const QString &emotion)
{
    const QString token = emotion.trimmed().toLower();
    if (token.isEmpty() || !emotionImages_.contains(token))
        return false;
    if (currentEmotion_ == token)
        return true;

    currentEmotion_ = token;
    emit emotionChanged(currentEmotion_);

    if (token == QStringLiteral("neutral"))
        neutralRestoreTimer_->stop();
    else
        scheduleNeutralRestore();

    return true;
}

void SpriteCatalog::restoreNeutral()
{
    if (currentEmotion_ == QStringLiteral("neutral"))
        return;
    setEmotion(QStringLiteral("neutral"));
}

QImage SpriteCatalog::currentImage() const
{
    if (currentEmotion_.isEmpty() || !emotionImages_.contains(currentEmotion_))
        return {};
    return emotionImages_.value(currentEmotion_);
}

void SpriteCatalog::rescanCurrentMode()
{
    emotionImages_.clear();
    currentEmotion_.clear();
    neutralRestoreTimer_->stop();

    if (currentMode_.isEmpty())
        return;

    loadEmotionImages();

    if (emotionImages_.contains(QStringLiteral("neutral")))
        setEmotion(QStringLiteral("neutral"));
    else if (!emotionImages_.isEmpty())
        setEmotion(emotionImages_.constBegin().key());
}

bool SpriteCatalog::loadEmotionImages()
{
    const QStringList roots = {
        QDir::cleanPath(modesBase_ + QLatin1Char('/') + currentMode_),
        QStringLiteral(":/assets/modes/") + currentMode_,
    };

    for (const QString &rootPath : roots) {
        const QDir dir(rootPath);
        if (!dir.exists())
            continue;

        const QStringList names = dir.entryList(imageFilters(), QDir::Files, QDir::Name);
        for (const QString &fileName : names) {
            const QString emotion = QFileInfo(fileName).completeBaseName().toLower();
            if (emotion.isEmpty() || emotionImages_.contains(emotion))
                continue;

            QImage img = loadImageFile(dir.filePath(fileName));
            if (img.isNull())
                continue;
            emotionImages_.insert(emotion, std::move(img));
        }
    }
    return !emotionImages_.isEmpty();
}

void SpriteCatalog::scheduleNeutralRestore()
{
    if (!emotionImages_.contains(QStringLiteral("neutral")))
        return;
    neutralRestoreTimer_->start(kNeutralRestoreMs);
}
