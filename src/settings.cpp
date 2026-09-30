#include "settings.h"

#include <QDir>
#include <QSettings>

Settings::Settings(QObject *parent) : QObject(parent)
{
    QDir dir(QStringLiteral("/etc/backupapp/"));

    const QStringList files = dir.entryList({QStringLiteral("*.conf")}, QDir::Files);
    for (const QString &file : files) {
        QSettings settings(dir.filePath(file), QSettings::IniFormat);

        if (settings.value("name", "").toString().isEmpty()) continue;

        _settingsMap.insert(file, settings.value("name").toString());
    }
}

QVariantList Settings::getProfiles()
{
    QVariantList result;

    for (auto it = _settingsMap.constBegin(); it != _settingsMap.constEnd(); ++it) {
        result.append(QVariantMap{{QStringLiteral("file"), it.key()}, {QStringLiteral("name"), it.value()}});
    }

    return result;
}
