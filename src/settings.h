#ifndef SETTINGS_H
#define SETTINGS_H

#include <QMap>
#include <QObject>
#include <QVariant>

class Settings : public QObject
{
    Q_OBJECT
public:
    explicit Settings(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList getProfiles();
    Q_INVOKABLE QString createProfile(const QString &name);
    Q_INVOKABLE void deleteProfile(const QString &file);

    Q_INVOKABLE QString getStringValue(const QString &file, const QString &key);
    Q_INVOKABLE int getIntValue(const QString &file, const QString &key);
    Q_INVOKABLE bool getBoolValue(const QString &file, const QString &key);
    Q_INVOKABLE QStringList getStringListValue(const QString &file, const QString &key);

    Q_INVOKABLE void setValue(const QString &file, const QString &key, const QVariant &value);
    Q_INVOKABLE void setStringListValue(const QString &file, const QString &key, const QStringList &value);

signals:

private:
    QMap<QString, QString> _settingsMap;
};

#endif // SETTINGS_H
