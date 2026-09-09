#pragma once

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

namespace SqlHelpers {

inline QString nonNullText(const QString &text)
{
    return text.isNull() ? QStringLiteral("") : text;
}

inline QString utcNowText()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

inline QString toUtcText(const QDateTime &dateTime)
{
    return dateTime.isValid() ? dateTime.toUTC().toString(Qt::ISODateWithMs) : QString();
}

inline QDateTime fromUtcText(const QString &text)
{
    if (text.isEmpty())
        return {};
    return QDateTime::fromString(text, Qt::ISODateWithMs).toUTC();
}

inline QString queryError(const QString &context, const QSqlQuery &query)
{
    return QStringLiteral("%1：%2").arg(context, query.lastError().text());
}

inline QString escapedLike(QString text)
{
    text.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    text.replace(QStringLiteral("%"), QStringLiteral("\\%"));
    text.replace(QStringLiteral("_"), QStringLiteral("\\_"));
    return text;
}

inline QStringList normalizedTags(const QStringList &tags)
{
    QStringList result;
    for (const QString &tag : tags) {
        const QString cleaned = tag.trimmed();
        if (cleaned.isEmpty())
            continue;
        bool duplicate = false;
        for (const QString &existing : result) {
            if (existing.compare(cleaned, Qt::CaseInsensitive) == 0) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            result.append(cleaned);
    }
    return result;
}

} // namespace SqlHelpers
