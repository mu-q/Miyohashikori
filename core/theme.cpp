#include "theme.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QStyle>

namespace {

bool systemUsesLightColors()
{
    const QColor window = QGuiApplication::palette().color(QPalette::Window);
    return window.lightness() >= 128;
}

} // namespace

ThemeManager *ThemeManager::instance()
{
    // QApplication owns the singleton. Keeping the QObject itself in static storage while
    // also parenting it to qApp would make application shutdown delete it twice.
    static ThemeManager *manager = new ThemeManager(qApp);
    return manager;
}

ThemeManager::ThemeManager(QObject *parent) : QObject(parent), systemLight_(systemUsesLightColors())
{
}

QString ThemeManager::normalizedId(const QString &id)
{
    const QString value = id.trimmed().toLower();
    if (value == QStringLiteral("night") || value == QStringLiteral("mist"))
        return value;
    return QStringLiteral("system");
}

QString ThemeManager::requestedId() const
{
    return requestedId_;
}

QString ThemeManager::effectiveId() const
{
    return effectiveId_;
}

bool ThemeManager::isLight() const
{
    return effectiveId_ == QStringLiteral("mist");
}

void ThemeManager::apply(const QString &id)
{
    requestedId_ = normalizedId(id);
    effectiveId_ = requestedId_ == QStringLiteral("system")
        ? (systemLight_ ? QStringLiteral("mist") : QStringLiteral("night"))
        : requestedId_;
    applyApplicationPalette();
    emit themeChanged();
}

void ThemeManager::applyApplicationPalette()
{
    QPalette palette;
    if (isLight()) {
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#F4F8FF")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#253552")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#FFFFFF")));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#EAF1FC")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#253552")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#E2ECFB")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#253552")));
        palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#8B78C5")));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#7A88A2")));
        palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#FFFFFF")));
        palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#253552")));
    } else {
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#172238")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#DCE9FF")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#121C30")));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#21304D")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#EDF4FF")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#2E4164")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#EDF4FF")));
        palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#7CAAF8")));
        palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#14203A")));
        palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#91A2BE")));
        palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#243553")));
        palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#EDF4FF")));
    }
    qApp->setPalette(palette);

    const QString global = isLight() ? QStringLiteral(R"(
        QToolTip { color:#253552; background:#ffffff; border:1px solid #b8c9e4; padding:5px; }
        QMenu { color:#253552; background:#f8fbff; border:1px solid #c1d0e6; padding:5px; }
        QMenu::item { padding:7px 24px 7px 10px; border-radius:6px; }
        QMenu::item:selected { background:#e2ecfb; }
        QMessageBox, QFileDialog { background:#f4f8ff; color:#253552; }
    )") : QStringLiteral(R"(
        QToolTip { color:#edf4ff; background:#243553; border:1px solid #5875a2; padding:5px; }
        QMenu { color:#edf4ff; background:#1c2942; border:1px solid #40587e; padding:5px; }
        QMenu::item { padding:7px 24px 7px 10px; border-radius:6px; }
        QMenu::item:selected { background:#30496f; }
        QMessageBox, QFileDialog { background:#172238; color:#edf4ff; }
    )");
    qApp->setStyleSheet(global);
}
