#pragma once

#include <QObject>
#include <QString>

class ThemeManager final : public QObject
{
    Q_OBJECT

public:
    static ThemeManager *instance();

    static QString normalizedId(const QString &id);
    QString requestedId() const;
    QString effectiveId() const;
    bool isLight() const;

    void apply(const QString &id);

signals:
    void themeChanged();

private:
    explicit ThemeManager(QObject *parent = nullptr);
    void applyApplicationPalette();

    QString requestedId_ = QStringLiteral("system");
    QString effectiveId_ = QStringLiteral("night");
    bool systemLight_ = false;
};
