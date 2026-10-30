/*
    Copyright (C) 2026 Michał Szczepaniak

    This file is part of backupapp.

    backupapp is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    backupapp is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with backupapp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "settings.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>

Settings::Settings(QObject *parent) : QObject(parent)
{
}

QVariantList Settings::getProfiles()
{
    _settingsMap.clear();

    QDir dir(QStringLiteral("/etc/backupapp/"));

    const QStringList files = dir.entryList({QStringLiteral("*.conf")}, QDir::Files);
    for (const QString &file : files) {
        QSettings settings(dir.filePath(file), QSettings::IniFormat);

        if (settings.value("name", "").toString().isEmpty()) continue;

        _settingsMap.insert(file, settings.value("name").toString());
    }

    QVariantList result;

    for (auto it = _settingsMap.constBegin(); it != _settingsMap.constEnd(); ++it) {
        result.append(QVariantMap{{QStringLiteral("file"), it.key()}, {QStringLiteral("name"), it.value()}});
    }

    return result;
}

QString Settings::createProfile(const QString &name)
{
    QString slug = name.toLower();

    slug.replace(QLatin1Char(' '), QLatin1Char('-'));
    slug.remove(QRegularExpression(QStringLiteral("[^a-z0-9_-]")));

    if (slug.isEmpty())
        slug = QStringLiteral("profile");

    QString file = slug + ".conf";

    for (int i = 2; QFile::exists("/etc/backupapp/" + file); i++)
        file = slug + "-" + QString::number(i) + ".conf";

    const QString path = "/etc/backupapp/" + file;

    {
        QSettings settings(path, QSettings::IniFormat);

        settings.setValue("name", name);
        settings.setValue("sourceType", "directory");
        settings.setValue("destinationType", "webdav");
    }

    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);

    return file;
}

void Settings::deleteProfile(const QString &file)
{
    QFile::remove("/etc/backupapp/" + file);
}

QString Settings::getStringValue(const QString &file, const QString &key)
{
    QSettings settings("/etc/backupapp/" + file, QSettings::IniFormat);

    return settings.value(key, "").toString();
}

int Settings::getIntValue(const QString &file, const QString &key)
{
    QSettings settings("/etc/backupapp/" + file, QSettings::IniFormat);

    return settings.value(key, 0).toInt();
}

bool Settings::getBoolValue(const QString &file, const QString &key)
{
    QSettings settings("/etc/backupapp/" + file, QSettings::IniFormat);

    return settings.value(key, false).toBool();
}

QStringList Settings::getStringListValue(const QString &file, const QString &key)
{
    QSettings settings("/etc/backupapp/" + file, QSettings::IniFormat);

    return settings.value(key).toStringList();
}

void Settings::setValue(const QString &file, const QString &key, const QVariant &value)
{
    QSettings settings("/etc/backupapp/" + file, QSettings::IniFormat);

    settings.setValue(key, value);
}

void Settings::setStringListValue(const QString &file, const QString &key, const QStringList &value)
{
    QSettings settings("/etc/backupapp/" + file, QSettings::IniFormat);

    settings.setValue(key, value);
}
