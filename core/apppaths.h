#pragma once

#include <QString>

namespace AppPaths {

QString appDataRoot();
QString configFilePath();
QString databaseFilePath();
QString logsRoot();
QString ttsCacheRoot();
QString assetsRoot();
QString modesRoot();
QString voiceRoot();
QString defaultTtsReferenceAudioPath();

} // namespace AppPaths
